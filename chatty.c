/*
 * membox Progetto del corso di LSO 2017/2018
 *
 * Dipartimento di Informatica Università di Pisa
 * Docenti: Prencipe, Torquati
 * 
 */
/**
 * @file chatty.c
 * @brief File principale del server chatterbox
 *
   \author Michele Morisco 505252 
       Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
       originale dell'autore  
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <ctype.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/un.h>
#include <fcntl.h>
#include <sys/stat.h>

/* inserire gli altri include che servono */
#define SYSCALL(r, c, e) \
	if((r=c)==-1) {printf("ERRORE SYSCALL\n"); perror(e);}

#include <info_user.h>
#include <configSetup.h>
#include <connections.h>
#include <message.h>
#include <ops.h>
#include <config.h>
#include <fileManager.h>
#include <stats.h>
#include <hashManager.h>
#include <threadPoolHandler.h>

/* struttura che memorizza le statistiche del server, struct statistics 
 * e' definita in stats.h.
 *
 */
struct statistics  chattyStats = { 0,0,0,0,0,0,0 };

typedef struct threadAccept{
	ConfigureSetup *cfs;
	int conn_fd;
	struct sockaddr_un *sa;
}threadACC;

static item_user DELETED_ITEM_USER = {NULL, NULL};
/*struttura che contiene gli utenti registrati al server*/
ht_user *userDB = NULL;
/*struttura che contiene i gruppi registrati al server*/
ht_group *groupDB = NULL;
/*numero degli utenti connessi al server*/
int nconn;
/*variabile che contiene tutte le configurazioni del server*/
static ConfigureSetup configureSet;
/*dimensione massima dei messaggi*/
static int MaxMsgDim;
int connfd_sel;
int *connfdList;
int acceptMux;
fd_set set;
/*identificatore standard per salvataggio messaggi*/
char fpath[10] = "history/";

/*variabili di condizionamenti per mutua esclusione sulla accept */
static pthread_mutex_t mutexAccept;
static pthread_cond_t conditionAccept = PTHREAD_COND_INITIALIZER;

/*mutua esclusione per il file delle statistiche */
static pthread_mutex_t mutexStats;

/*mutua esclusione per la gestione dei file */
static pthread_mutex_t mutexFile;

/*variabili di condizionamenti per mutua esclusione sugli utenti e le sue operazioni */
static pthread_mutex_t mutexUser;
static pthread_cond_t conditionUser = PTHREAD_COND_INITIALIZER;

static pthread_mutexattr_t attr;

static void usage(const char *progname) {
    fprintf(stderr, "Il server va lanciato con il seguente comando:\n");
    fprintf(stderr, "  %s -f conffile\n", progname);
}

void cleanup(){
	unlink(configureSet.unixPath);
}

/* funzione per l'invio di messaggi al client */
int sendMessagge(message_t msg, op_t op){

	pthread_mutex_lock(&mutexUser);
	/*controllo se il ricevente è presente nella lista utenti*/
	USER_INFO *receiver = searchUser(userDB, msg.data.hdr.receiver);
	pthread_mutex_unlock(&mutexUser);

	int esito, previous = 0;
	int fdReceiver;
	MSG_INFO temp;
	char pathFile[MAX_BUF];
	strcpy(pathFile, "");

	if(op == TXT_MESSAGE)
		temp.isFile = 0;
	else
		temp.isFile = 1;
		
	msg.hdr.op = op;
	strcpy(temp.path, msg.data.buf);

	if(receiver == NULL)
		return -1;
	fdReceiver = receiver->fd;

	/* receiver connesso */
	if(fdReceiver > 0){
		/* aspetto che finiscano altre operazioni sul fd*/
		pthread_mutex_lock(&mutexUser);
		previous = connfdList[fdReceiver];		

		while(previous != 0 && previous != -1 && previous != (int) pthread_self()){
			pthread_cond_wait(&conditionUser, &mutexUser);
			previous = connfdList[fdReceiver];
		}

		connfdList[fdReceiver] = (int) pthread_self();
		pthread_mutex_unlock(&mutexUser);
		
		/*setto flag di messaggio letto*/
		temp.isRead = 1;
		esito = sendRequest(fdReceiver, &msg);
		
		/* disimpegno fd dal thread attuale */	
		pthread_mutex_lock(&mutexUser);
		connfdList[fdReceiver] = previous;
		pthread_cond_broadcast(&conditionUser);
		pthread_mutex_unlock(&mutexUser);
		
		if(esito > 0){
			/* aggiorno le statistiche */
			if(op == TXT_MESSAGE){
				pthread_mutex_lock(&mutexStats);
				chattyStats.ndelivered++;
				pthread_mutex_unlock(&mutexStats);
			}
			else{
				pthread_mutex_lock(&mutexStats);
				chattyStats.nfiledelivered++;
				pthread_mutex_unlock(&mutexStats);
			}
		}
	}
	/* receiver non connesso attualmente */
	else
		temp.isRead = 0;	
	
	/*preparo percorso per il salvataggio del messaggio nella directory 'history'*/	
	strcat(pathFile, fpath);
	strcat(pathFile, msg.data.hdr.receiver);
	strcat(pathFile, "/");
	strcat(pathFile, "msg");
	char numb[12];
	sprintf(numb, "%d", receiver->currentIndex);
	strcat(pathFile, numb);

	/* salvo il messaggio nella apposita directory*/
	pthread_mutex_lock(&mutexFile);
	remove(pathFile);
	fdReceiver = -1;
	fdReceiver = open(pathFile, O_CREAT | O_RDWR, 0777);
	if(fdReceiver > 0){
		esito = sendRequest(fdReceiver, &msg);
		close(fdReceiver);
	}
	else 
		esito = -1;

	/*ora salvo il percorso dove risiede il messaggio in una lista*/
	if(esito > 0){
		strcpy(temp.path, pathFile);
		esito = userMsg(receiver, temp);
	}
	pthread_mutex_unlock(&mutexFile);
	if(esito > 0 && !temp.isRead){
		/* aggiorno le statistiche */
		if(op == TXT_MESSAGE){
			pthread_mutex_lock(&mutexStats);
			chattyStats.nnotdelivered++;
			pthread_mutex_unlock(&mutexStats);
		}
		else{
			pthread_mutex_lock(&mutexStats);
			chattyStats.nfilenotdelivered++;
			pthread_mutex_unlock(&mutexStats);
		}
	}
	return esito;
}

int cmd_server(message_t msg, int fd){
	op_t op = msg.hdr.op;
	message_t rep;
	char *buffer;
	message_t *bufferMsg;
	char temp[MAX_BUF];
	int report, dimBuff, i, isError, isCancel;

	isCancel = 0;
	setHeader(&rep.hdr, OP_FAIL, "");
	setData(&rep.data, "", NULL, 0);

	switch(op){
		case REGISTER_OP:{		
			/*controllo se esiste già un utente registrato con quel nickname*/
			pthread_mutex_lock(&mutexUser);
			/*preparo l'informazioni dell'utente*/
			USER_INFO *new = malloc(sizeof(USER_INFO));
			strcpy(new->username, msg.hdr.sender);
			new->fd = -1;
			new->msg_list = (MSG_INFO *) malloc(MaxMsgDim*sizeof(MSG_INFO));
			new->countMsg = 0; 
			new->currentIndex = 0;
			new->maxHistMsg = MaxMsgDim; 

			/*inserisco il nuovo utente nella hash table*/
			report = insertUser(userDB, msg.hdr.sender, &new);
			pthread_mutex_unlock(&mutexUser);
			
			/*caso in cui l'utente è gia registrato*/
			if(report == 0){
				setHeader(&rep.hdr, OP_NICK_ALREADY, msg.hdr.sender);
				sendHeader(fd, &rep.hdr);
				return -1;
			}
			/*caso in cui l'inserimento avviene un errore*/
			else if(report < 0){
				setHeader(&rep.hdr, OP_FAIL, msg.hdr.sender);
				sendHeader(fd, &rep.hdr);
				return -1;
			}
			/*aggiunta del nickname*/
			else{
				strcpy(temp, "");
				/*creo una cartella per il nuovo utente nella directory 'history'*/
				strcat(temp, fpath);
				strcat(temp, msg.hdr.sender);
				mkdir(temp, 0777);

				/* aggiorno le statistiche */
				pthread_mutex_lock(&mutexStats);
				chattyStats.nusers++;
				pthread_mutex_unlock(&mutexStats);
			}
		} 
		case CONNECT_OP:{
			if(chattyStats.nusers == 0){
				setHeader(&rep.hdr, OP_NICK_UNKNOWN, msg.hdr.sender);
				sendHeader(fd, &rep.hdr);
				return -1;
			}
			/*se l'utente non è presente nella lista dei registrati allora mando un errore*/
			pthread_mutex_lock(&mutexUser);
			USER_INFO *res = searchUser(userDB, msg.hdr.sender);
			pthread_mutex_unlock(&mutexUser);

			if(res == NULL){
				setHeader(&rep.hdr, OP_NICK_UNKNOWN, msg.hdr.sender);
				sendHeader(fd, &rep.hdr);
				return -1;
			}

			/*utente già connesso - questo tipo di risposta dal client non verrà riconosciuta*/
			if(res->fd != -1){
				//setHeader(&rep.hdr, OP_CONNECTED_ALREADY, msg.hdr.sender);
				setHeader(&rep.hdr, OP_FAIL, msg.hdr.sender);
				sendHeader(fd, &rep.hdr);
			}
			else{
				pthread_mutex_lock(&mutexUser);
				/*assegno fd sull'utente connesso*/
				userConnection(userDB, msg.hdr.sender, fd);
				pthread_mutex_unlock(&mutexUser);

				/* aggiorno le statistiche */
				pthread_mutex_lock(&mutexStats);
				chattyStats.nonline++;
				pthread_mutex_unlock(&mutexStats);
			}		
		}
		case USRLIST_OP:{
			/*ottengo la lista degli utenti online*/
			char *usrlist = printListUser(userDB, &dimBuff);
			setHeader(&rep.hdr, OP_OK, msg.hdr.sender);
			setData(&rep.data, msg.hdr.sender, usrlist, dimBuff);
			sendRequest(fd, &rep);	
			if(usrlist)
				free(usrlist);	
		}break;
		case UNREGISTER_OP:{
			/*se l'utente prova a deregistrare un altro utente allora mando un errore*/
			if(strcmp(msg.hdr.sender, msg.data.hdr.receiver) != 0){
				setHeader(&rep.hdr, OP_FAIL, msg.hdr.sender);
				sendHeader(fd, &rep.hdr);
				return -1;
			}
			/*se l'utente non è presente nella lista dei registrati allora mando un errore*/
			pthread_mutex_lock(&mutexUser);
			report = removeUser(userDB, groupDB, msg.hdr.sender);
			pthread_mutex_unlock(&mutexUser);

			strcpy(temp, "");
			/*elimino la cartella corrispondente all'utente nella directory 'history'*/
			strcat(temp, fpath);
			strcat(temp, msg.hdr.sender);
			removeDirectory(temp);

			if(report < 1){
				setHeader(&rep.hdr, OP_NICK_UNKNOWN, msg.hdr.sender);
				sendHeader(fd, &rep.hdr);
				return -1;
			}
			else{ 
				/* aggiorno le statistiche */
				pthread_mutex_lock(&mutexStats);
				chattyStats.nusers--;
				pthread_mutex_unlock(&mutexStats);
				
				isCancel = 1;
				setHeader(&rep.hdr, OP_OK, msg.hdr.sender);
				sendHeader(fd, &rep.hdr);
			}
		}
		case DISCONNECT_OP:{
			if(fd == connfd_sel) 
				connfd_sel--;
			nconn--;
			close(fd);
				
			pthread_mutex_lock(&mutexAccept);
			acceptMux = 1;
			pthread_mutex_unlock(&mutexAccept);
			pthread_cond_signal(&conditionAccept);
			
			if(!isCancel){
				pthread_mutex_lock(&mutexUser);
				USER_INFO *res = searchUserByFd(fd, userDB);
				if(res == NULL){
					pthread_mutex_unlock(&mutexUser);
					return -1;	
				}
				/*setto il fd relativo all'utente a -1 - non connesso*/
				userConnection(userDB, res->username, -1);
				pthread_mutex_unlock(&mutexUser);
			}

			/* aggiorno le statistiche */
			pthread_mutex_lock(&mutexStats);
			chattyStats.nonline--;
			pthread_mutex_unlock(&mutexStats);
		}break;
		case  POSTTXT_OP:{
			int isGroup = 0;
			GROUP_INFO *resGroup = NULL;
			ht_user *usersGroup = NULL;
			/*controllo se il file non superi la dimensione dichiarata nel file configurazione */
			if(msg.data.hdr.len > configureSet.maxMsgSize){
				setHeader(&rep.hdr, OP_MSG_TOOLONG, msg.hdr.sender);
				sendHeader(fd, &rep.hdr);
				return -1;
			}

			/*controllo se l'utente non si invia a se stesso un messaggio*/
			if(strcmp(msg.hdr.sender,  msg.data.hdr.receiver) == 0){
				setHeader(&rep.hdr, OP_FAIL, msg.hdr.sender);
				sendHeader(fd, &rep.hdr);
				return -1;
			}
			printf("Invio messaggio [%s] da %s\n", msg.data.buf, msg.hdr.sender);

			/*controllo se è presente un nickname nella lista registrati*/
			pthread_mutex_lock(&mutexUser);
			USER_INFO *resUser = searchUser(userDB, msg.data.hdr.receiver);
			pthread_mutex_unlock(&mutexUser);

			/*se non è presente il nome nella lista controllo se si tratta di un gruppo*/
			if(resUser == NULL){
				/*controllo se è presente un nome nella lista dei gruppi*/
				pthread_mutex_lock(&mutexUser);
				resGroup = searchGroup(groupDB, msg.data.hdr.receiver);
				pthread_mutex_unlock(&mutexUser);
			
				if(resGroup == NULL){
					setHeader(&rep.hdr, OP_NICK_UNKNOWN, msg.hdr.sender);
					sendHeader(fd, &rep.hdr);
					return -1;
				}
				usersGroup = resGroup->users;

				/*controllo se è presente il mittente nella lista degli utenti nel gruppo*/
				if(strcmp(resGroup->host, msg.hdr.sender) != 0){
					pthread_mutex_lock(&mutexUser);
					int esito = searchUserToGroup(usersGroup, msg.hdr.sender);
					pthread_mutex_unlock(&mutexUser);
					if(esito == NULL){
						setHeader(&rep.hdr, OP_NICK_UNKNOWN, msg.hdr.sender);
						sendHeader(fd, &rep.hdr);
						return -1;
					}
				}
				isGroup = 1;
			}
			
			pthread_mutex_lock(&mutexUser);
			connfdList[fd] = -1;
			pthread_mutex_unlock(&mutexUser);

			/*se si tratta di un gruppo mando il messaggio a tutti gli utenti che ne fanno parte*/
			if(isGroup > 0){
				strcpy(msg.data.hdr.receiver, resGroup->host);
				report = sendMessagge(msg, 21);

				if(report <= 0){
					setHeader(&rep.hdr, OP_FAIL, msg.hdr.sender);
					sendHeader(fd, &rep.hdr);
					return -1;
				}
				
				/*scorro la lista con tutti i partecipanti del gruppo*/
				for(int i = 0; i < usersGroup->size; i++){
					item_user *item = usersGroup->items[i];
					if(item != NULL){
						if(item != &DELETED_ITEM_USER){
							strcpy(msg.data.hdr.receiver, item->username);
							report = sendMessagge(msg, 21);
			
							if(report <= 0){
								setHeader(&rep.hdr, OP_FAIL, msg.hdr.sender);
								sendHeader(fd, &rep.hdr);
								return -1;
							}
						}
					}
				}
			}
			else
				report = sendMessagge(msg, 21);

			pthread_mutex_lock(&mutexUser);
			while(connfdList[fd] != -1)
				pthread_cond_wait(&conditionUser, &mutexUser);

			connfdList[fd] = (int) pthread_self();
			pthread_mutex_unlock(&mutexUser);
			if(report <= 0){
				setHeader(&rep.hdr, OP_FAIL, msg.hdr.sender);
				sendHeader(fd, &rep.hdr);
				return -1;
			}
			setHeader(&rep.hdr, OP_OK, msg.hdr.sender);
			sendHeader(fd, &rep.hdr);	
		}break;
		case POSTTXTALL_OP:{
			/*controllo se il file non superi la dimensione dichiarata nel file configurazione */
			if(msg.data.hdr.len > configureSet.maxMsgSize){
				setHeader(&rep.hdr, OP_MSG_TOOLONG, msg.hdr.sender);
				sendHeader(fd, &rep.hdr);
				return -1;
			}
			
			pthread_mutex_lock(&mutexUser);
			connfdList[fd] = -1;
			pthread_mutex_unlock(&mutexUser);
			
			/* scorro tutta la lista degli utenti registrati */
			for(int ind = 0; ind < userDB->size; ind++){
				if(userDB->items[ind] != NULL ){
					if(userDB->items[ind] != &DELETED_ITEM_USER){
					/*invia i messaggi a tutti gli utenti tranne a se stesso*/
						if(userDB->items[ind]->username != NULL && strcmp(msg.hdr.sender,  userDB->items[ind]->username) != 0){
							strcpy(msg.data.hdr.receiver, userDB->items[ind]->username);
							sendMessagge(msg, 21);
						}
					}
				}
			}

			pthread_mutex_lock(&mutexUser);
			while(connfdList[fd] != -1)
				pthread_cond_wait(&conditionUser, &mutexUser);

			connfdList[fd] = (int) pthread_self();
			pthread_mutex_unlock(&mutexUser);

			setHeader(&rep.hdr, OP_OK, msg.hdr.sender);
			sendHeader(fd, &rep.hdr);
		}break;
		case POSTFILE_OP:{
			int isGroup = 0;
			GROUP_INFO *resGroup = NULL;
			ht_user *usersGroup = NULL;						
			/*setto il percorso per il salvataggio del file*/
			strcpy(temp, configureSet.dirName);
			strcat(temp, "/");
			strcat(temp, msg.data.buf);

			readData(fd, &rep.data);
			if(rep.data.hdr.len > (configureSet.maxFileSize*1024)){
				setHeader(&rep.hdr, OP_MSG_TOOLONG, msg.hdr.sender);
				sendHeader(fd, &rep.hdr);
				if(rep.data.buf != NULL)
					free(rep.data.buf);
				return -1;	
			}
	
			setHeader(&rep.hdr, OP_OK, msg.hdr.sender);
			sendHeader(fd, &rep.hdr);

			/*controllo se è presente un nickname nella lista registrati*/
			pthread_mutex_lock(&mutexUser);
			USER_INFO *resUser = searchUser(userDB, msg.data.hdr.receiver);
			pthread_mutex_unlock(&mutexUser);

			/*se non è presente il nome nella lista controllo se si tratta di un gruppo*/
			if(resUser == NULL){
				/*controllo se è presente un nome nella lista dei gruppi*/
				pthread_mutex_lock(&mutexUser);
				resGroup = searchGroup(groupDB, msg.data.hdr.receiver);
				pthread_mutex_unlock(&mutexUser);
				if(resGroup == NULL){
					setHeader(&rep.hdr, OP_NICK_UNKNOWN, msg.hdr.sender);
					sendHeader(fd, &rep.hdr);
					if(rep.data.buf != NULL)
						free(rep.data.buf);
					return -1;
				}
				
				usersGroup = resGroup->users;

				/*controllo se è presente il mittente nella lista degli utenti nel gruppo*/
				if(strcmp(resGroup->host, msg.hdr.sender) != 0){
					pthread_mutex_lock(&mutexUser);
					int esito = searchUserToGroup(usersGroup, msg.hdr.sender);
					pthread_mutex_unlock(&mutexUser);

					if(esito < 1){
						setHeader(&rep.hdr, OP_NICK_UNKNOWN, msg.hdr.sender);
						sendHeader(fd, &rep.hdr);

						if(rep.data.buf != NULL)
							free(rep.data.buf);
						return -1;
					}
				}
				isGroup = 1;
			}
			
			pthread_mutex_lock(&mutexFile);
			unlink(temp);
			i = open(temp, O_CREAT | O_WRONLY | O_TRUNC, 0777);
			write(i, rep.data.buf, rep.data.hdr.len);
			close(i);
			pthread_mutex_unlock(&mutexFile);
			
			if(i < 0){
				if(rep.data.buf != NULL)
					free(rep.data.buf);
				return -1;
			}

			pthread_mutex_lock(&mutexUser);
			connfdList[fd] = -1;
			pthread_mutex_unlock(&mutexUser);

			/*se si tratta di un gruppo mando il messaggio a tutti gli utenti che ne fanno parte*/
			if(isGroup > 0){
				strcpy(msg.data.hdr.receiver, resGroup->host);
				report = sendMessagge(msg, 22);

				if(report <= 0){
					setHeader(&rep.hdr, OP_FAIL, msg.hdr.sender);
					sendHeader(fd, &rep.hdr);
				
					if(rep.data.buf != NULL)
						free(rep.data.buf);
					return -1;
				}
								
				/*scorro la lista con tutti i partecipanti del gruppo*/
				for(int i = 0; i < usersGroup->size; i++){
					item_user *item = usersGroup->items[i];
					if(item != NULL){
						if(item != &DELETED_ITEM_USER){
							strcpy(msg.data.hdr.receiver, item->username);
							report = sendMessagge(msg, 22);
			
							if(report <= 0){
								setHeader(&rep.hdr, OP_FAIL, msg.hdr.sender);
								sendHeader(fd, &rep.hdr);
								
								if(rep.data.buf != NULL)
									free(rep.data.buf);			
								return -1;
							}
						}
					}
				}
			}
			else
				report = sendMessagge(msg, 22);

			pthread_mutex_lock(&mutexUser);
			while(connfdList[fd] != -1)
				pthread_cond_wait(&conditionUser, &mutexUser);

			connfdList[fd] = (int) pthread_self();
			pthread_mutex_unlock(&mutexUser);
			if(report <= 0){
				if(rep.data.buf != NULL)
					free(rep.data.buf);
				setHeader(&rep.hdr, OP_FAIL, msg.hdr.sender);
				sendHeader(fd, &rep.hdr);
				return -1;
			}

			if(rep.data.buf != NULL)
				free(rep.data.buf);	
			
		}break;
		case GETFILE_OP:{
			buffer = calloc(configureSet.maxFileSize + 1, 1);
			strcpy(temp, configureSet.dirName);
			strcat(temp, "/");
			strcat(temp, msg.data.buf);

			pthread_mutex_lock(&mutexFile);
			i = open(temp, O_RDONLY, 0777);
			if(i > 0){
				dimBuff = read(i, buffer, configureSet.maxFileSize + 1);
				close(i);
				pthread_mutex_unlock(&mutexFile);
			}
			else{
				close(i);
				pthread_mutex_unlock(&mutexFile);
				setHeader(&rep.hdr, OP_NO_SUCH_FILE, msg.hdr.sender);
				sendHeader(fd, &rep.hdr);

				if(buffer != NULL)
					free(buffer);
				return -1;
			}

			setHeader(&rep.hdr, OP_OK, msg.hdr.sender);
			setData(&rep.data, msg.hdr.sender, buffer, dimBuff);
			sendRequest(fd, &rep);

			if(buffer != NULL)
				free(buffer);
		}break;
		case GETPREVMSGS_OP:{
			/*controllo se è presente un nickname nella lista registrati*/
			pthread_mutex_lock(&mutexUser);
			USER_INFO *res = searchUser(userDB, msg.hdr.sender);
			pthread_mutex_unlock(&mutexUser);
			
			/*se non è presente il nickname mando un errore */
			if(res == NULL){
				setHeader(&rep.hdr, OP_NICK_UNKNOWN, msg.hdr.sender);
				sendHeader(fd, &rep.hdr);
				return -1;
			}
			int nMsg = res->countMsg;
			int fdFile = -1;
			pthread_mutex_lock(&mutexUser);
		
			isError = 0;
			int i = 0;
			int j = 0;
			int n = 0;
			int nnotDelivered = 0;
			int nfilenotDelivered = 0;	
			if(nMsg < MaxMsgDim)
				j = 0;
			else 
				j = res->currentIndex;

			bufferMsg = malloc(sizeof(message_t) + 1);			
			/*invio messaggi pendenti*/
			while(i < nMsg && !isError){
				strcpy(temp, "");
				strcpy(temp, res->msg_list[j].path);
				fdFile = open(temp, O_RDONLY, 0666);
				if(fdFile > 0){
					if(res->msg_list[j].isRead == 0){
						if(res->msg_list[j].isFile == 0)
							nnotDelivered++;
						else 
							nfilenotDelivered++;
						res->msg_list[j].isRead = 1;
					}

					if(j >= nMsg - 1)
						j = 0;
					else
						j++;
					
					if(readMsg(fdFile, bufferMsg + i) > 0){
						i++;
						n++;
						bufferMsg = realloc(bufferMsg, (i+1)*sizeof(message_t) + 1);
					}
					else 
						isError = 1;
					close(fdFile);
				}
				else
					isError = 1;
			}

			if(!isError){
				/*invio numero di messaggi pendenti*/
				setHeader(&rep.hdr, OP_OK, msg.hdr.sender);				
				setData(&rep.data, msg.hdr.sender, (char *) &n, sizeof(int));
				sendRequest(fd, &rep);

				for(int i = 0; i < n; i++){
					sendRequest(fd, bufferMsg + i);
					free(bufferMsg[i].data.buf);
				}
				free(bufferMsg);
				pthread_mutex_unlock(&mutexUser);

				/* aggiorno le statistiche */	
				pthread_mutex_lock(&mutexStats);
				chattyStats.nnotdelivered -= nnotDelivered;
				chattyStats.ndelivered += nnotDelivered;
				chattyStats.nfilenotdelivered -= nfilenotDelivered;
				chattyStats.nfiledelivered += nfilenotDelivered;
				pthread_mutex_unlock(&mutexStats);

			}
			else{
				for(int i = 0; i < n; i++)
					free(bufferMsg[i].data.buf);
	
				free(bufferMsg);
				pthread_mutex_unlock(&mutexUser);
				i = 0;
				setHeader(&rep.hdr, OP_OK, msg.hdr.sender);
				setData(&rep.data, msg.hdr.sender, (char *) &i, sizeof(int));
				sendRequest(fd, &rep);	
			} 
				
		}break;
		case CREATEGROUP_OP:{
			/*controllo se esiste già un gruppo registrato con quel nome*/
			pthread_mutex_lock(&mutexUser);
			GROUP_INFO *new = malloc(sizeof(GROUP_INFO));
			strcpy(new->name, msg.data.hdr.receiver);
			strcpy(new->host, msg.hdr.sender);
			new->users = new_ht_user();

			if(new->users == NULL){
				setHeader(&rep.hdr, OP_FAIL, msg.hdr.sender);
				sendHeader(fd, &rep.hdr);
				return -1;
			}	
			new->countUser = 0; 

			/*inserisco il nuovo gruppo nella hash table*/
			report = insertGroup(groupDB, msg.data.hdr.receiver, &new);
			pthread_mutex_unlock(&mutexUser);
			
			if(report == 0){
				setHeader(&rep.hdr, OP_NICK_ALREADY, msg.hdr.sender);
				sendHeader(fd, &rep.hdr);
				return -1;
			}
			else if(report < 0){
				setHeader(&rep.hdr, OP_FAIL, msg.hdr.sender);
				sendHeader(fd, &rep.hdr);
				return -1;
			}
			else{
				setHeader(&rep.hdr, OP_OK, msg.hdr.sender);
				sendHeader(fd, &rep.hdr);
			}
		}break;
		case ADDGROUP_OP:{
			/*se il gruppo  non è presente nella lista dei gruppi allora mando un errore*/
			pthread_mutex_lock(&mutexUser);
			GROUP_INFO *resGroup = searchGroup(groupDB, msg.data.hdr.receiver);
			pthread_mutex_unlock(&mutexUser);

			/*se non è presente il nome mando un errore */
			if(resGroup == NULL){
				setHeader(&rep.hdr, OP_NICK_UNKNOWN, msg.hdr.sender);
				sendHeader(fd, &rep.hdr);
				return -1;
			}

			int esito = addUserToGroup(resGroup, msg.hdr.sender);
			
			if(esito == 0){
				setHeader(&rep.hdr, OP_NICK_ALREADY, msg.hdr.sender);
				sendHeader(fd, &rep.hdr);
				return -1;
			}
			else if(esito < 0){
				setHeader(&rep.hdr, OP_FAIL, msg.hdr.sender);
				sendHeader(fd, &rep.hdr);
				return -1;
			}
			else{
				setHeader(&rep.hdr, OP_OK, msg.hdr.sender);
				sendHeader(fd, &rep.hdr);
			}
			return 1;
		}break;
		case DELGROUP_OP:{
			/*se il gruppo  non è presente nella lista dei gruppi allora mando un errore*/
			pthread_mutex_lock(&mutexUser);
			GROUP_INFO *resGroup = searchGroup(groupDB, msg.data.hdr.receiver);
			pthread_mutex_unlock(&mutexUser);

			/*se non è presente il nome mando un errore */
			if(resGroup == NULL){
				setHeader(&rep.hdr, OP_NICK_UNKNOWN, msg.hdr.sender);
				sendHeader(fd, &rep.hdr);
				return -1;
			}
			
			int esito = 0;

			/*se è il capogruppo che vuole cancellarsi allora elimino il gruppo che ha creato*/
			if(strcmp(resGroup->host, msg.hdr.sender) == 0){
				esito = removeGroup(groupDB, msg.data.hdr.receiver);
				if(esito < 0){
					setHeader(&rep.hdr, OP_FAIL, msg.hdr.sender);
					sendHeader(fd, &rep.hdr);
					return -1;
				}
			}
			else{
				esito = removeUserToGroup(resGroup, msg.hdr.sender);
				if(esito == 0){
					setHeader(&rep.hdr, OP_NICK_UNKNOWN, msg.hdr.sender);
					sendHeader(fd, &rep.hdr);
					return -1;
				}
				else if(esito < 0){
					setHeader(&rep.hdr, OP_FAIL, msg.hdr.sender);
					sendHeader(fd, &rep.hdr);
					return -1;
				}
				else{
					setHeader(&rep.hdr, OP_OK, msg.hdr.sender);
					sendHeader(fd, &rep.hdr);
				}
			}
		}break;
		default: printf("Comando sconosciuto\n"); return -1;
	}
	return 0;
}

void *spawn_thread(void *arg) {
    	assert(arg);
	/* socket del server */
	int socket = *(int *) arg; 
    	int state,
	ch, /* fd del client */
	i, request;
	fd_set readSet; /*set dei fd per gli eventi*/
	sigset_t setsigs;

	/* voglio evitare che i thread ascoltino i segnali*/
	sigfillset(&setsigs);
	pthread_sigmask(SIG_SETMASK, &setsigs, NULL);

	/*voglio inoltre garantire che venga gestita una richiesta per thread*/
	pthread_mutex_lock(&mutexAccept);
	while(acceptMux == 0)
		pthread_cond_wait(&conditionAccept, &mutexAccept);
	
	acceptMux = 0;
	readSet = set;
	pthread_mutex_unlock(&mutexAccept);
	pthread_cond_signal(&conditionAccept);

	/* entro nel ciclo TRUE dove verranno gestite le connessioni socket con i client */
    	do{
		ch = -1;
		int cfd;
		readSet = set;
		/* la select monitora i file descriptor finché non sono pronti */
		SYSCALL(cfd, select((connfd_sel + 1), &readSet, NULL, NULL, NULL), "select");
		/* scorro fd pronti */
		for(i = 0; i <= connfd_sel; i++){
			/* controllo se il fd pronto è parte del set*/
			if(FD_ISSET(i, &readSet)){
				/* se è uguale al socket del server - aggiunta nuovo client*/
				if(i == socket){
					SYSCALL(ch, accept(socket, (struct sockaddr*)NULL, 0), "accept");
					pthread_mutex_lock(&mutexUser);
		
					/*controllo se non supera il limite di connessioni pendenti*/
					if(nconn <= configureSet.maxConnections){
						if(ch > connfd_sel)
							connfd_sel = ch;
						
						/* nuovo utente connesso */
						if(ch >= 0){
							nconn++;
							/* aggiorno il set */
							FD_SET(ch, &set);
						}
					}
					else{ 
						message_t msg;
						/* mando un messaggio di errore al client */
						setHeader(&msg.hdr, OP_END, "chatty.o");
						//setData(&msg.data, "", NULL, 0);
						sendHeader(ch, &msg.hdr);
						close(ch);
					}

					pthread_mutex_unlock(&mutexUser);
					break;
				}
				/* altrimenti messaggio da gestire di un client */
				else{
					pthread_mutex_lock(&mutexUser);
					if(connfdList[i] == 0){
						/* nell'array di mutex inserisco l'ID del thread chiamante al fd corrispondente impegnandolo */
						connfdList[i] = (int) pthread_self();
						pthread_mutex_unlock(&mutexUser);
						break;
					}
					pthread_mutex_unlock(&mutexUser);
				}
			}	
		}
		/* operazione da gestire */
		pthread_mutex_lock(&mutexUser);
		request = connfdList[i];
		pthread_mutex_unlock(&mutexUser);
		/* se l'operazione da gestire è uguale a pthread_self allora entro nel ramo if*/
		if(request == (int) pthread_self()){
			message_t msg;
			/* leggo la richiesta */
			int esito = readMsg(i, &msg);

			/* se l'esito è negativo significa che non ho più richieste da gestire e dunque disconnetto */
			if(esito <= 0){
				/* disattivo la cancellazione dei thread fino alla conclusione della richiesta */
				pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &state);
				FD_CLR(i, &set);
				/* imposto l'operazione di disconnessione e la gestisco */
				msg.hdr.op = DISCONNECT_OP;
				int result = cmd_server(msg, i);
				if(result < 0){
					pthread_mutex_lock(&mutexStats);
					chattyStats.nerrors++;
					pthread_mutex_unlock(&mutexStats);
				}

				pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &state);
			}
			/*se è una richiesta di deregistrazione tolgo il fd dal set e lo disconnetto subito dopo la richiesta*/
			else if(msg.hdr.op == UNREGISTER_OP){
				/* disattivo la cancellazione dei thread fino alla conclusione della richiesta */
				pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &state);
				FD_CLR(i, &set);
	
				int result = cmd_server(msg, i);
				if(result < 0){
					pthread_mutex_lock(&mutexStats);
					chattyStats.nerrors++;
					pthread_mutex_unlock(&mutexStats);
				}

				pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &state);
			}
			else{
				pthread_mutex_lock(&mutexAccept);
				acceptMux = 1;
				pthread_mutex_unlock(&mutexAccept);
				pthread_cond_signal(&conditionAccept);

				/* disattivo la cancellazione dei thread fino alla conclusione della richiesta */
				pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &state);
				/* elaboro una richiesta di un client */
				int result = cmd_server(msg, i);
				if(result < 0){
					pthread_mutex_lock(&mutexStats);
					chattyStats.nerrors++;
					pthread_mutex_unlock(&mutexStats);
				}
				
				free(msg.data.buf);
				pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &state);
			}

			/* disimpegno fd dal thread attuale */	
			pthread_mutex_lock(&mutexUser);
			connfdList[i] = 0;
			pthread_cond_broadcast(&conditionUser);
			pthread_mutex_unlock(&mutexUser);

			/* aspetto che sia solo prima accettare la prossima richiesta */
			pthread_mutex_lock(&mutexAccept);
			while(acceptMux == 0)
				pthread_cond_wait(&conditionAccept, &mutexAccept);

			acceptMux = 0;
			readSet = set;
			pthread_mutex_unlock(&mutexAccept);
			pthread_cond_signal(&conditionAccept);
		}
    	}while(1);	
}
	
void *clean_spawn(void *arg){
	pthread_mutex_unlock(&mutexAccept);
	pthread_cond_signal(&conditionAccept);
	pthread_mutex_unlock(&mutexUser);
	pthread_cond_signal(&conditionUser);
	pthread_mutex_unlock(&mutexFile);

	return NULL;
}

int connectClient(){
    	int listenfd, sig;
	threadPoolCreator tpCreator;
	struct sigaction s;
	sigset_t setsignals;
	
	/*inizializzo l'attributo di mutex con lo stato PTHREAD_MUTEX_ERRORCHECK*/
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
	/*inizializzo tutti i mutex con l'attributo*/
	pthread_mutex_init(&mutexAccept, &attr);
	pthread_mutex_init(&mutexUser, &attr);
	pthread_mutex_init(&mutexStats, &attr);
	pthread_mutex_init(&mutexFile, &attr);

	/* inizializzazione gestione segnali */
	sigemptyset(&setsignals);
	sigaddset(&setsignals, SIGINT);
	sigaddset(&setsignals, SIGUSR1);
	sigaddset(&setsignals, SIGQUIT);
	sigaddset(&setsignals, SIGTERM);
	pthread_sigmask(SIG_SETMASK, &setsignals, NULL);

	/* ignoro SIGPIPE per evitare che il server vada in crash quando il client si disconnette */
	memset(&s, 0, sizeof(s));
	s.sa_handler = SIG_IGN;
	if ((sigaction(SIGPIPE,&s,NULL)) == -1 ) {   
		perror("sigaction");
		return -1;
    	} 

	cleanup();
    	atexit(cleanup);
	
    	//creo il socket	
	SYSCALL(listenfd, socket(AF_UNIX, SOCK_STREAM, 0), "socket");
	connfd_sel = listenfd;

  	//setto l'indirizzo
    	struct sockaddr_un serv_addr;
    	memset(&serv_addr, '0', sizeof(serv_addr));
    	serv_addr.sun_family = AF_UNIX;
    	strncpy(serv_addr.sun_path, configureSet.unixPath, strlen(configureSet.unixPath)+1);
	FD_ZERO(&set);
	FD_SET(listenfd, &set);

    	int notused;
    	//assegno l'indirizzo al socket
    	SYSCALL(notused, bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)), "bind");
  	
	//setto il socket in modalità passiva e definisco un n. massimo di connessioni pendenti
	if(notused >= 0)
    		SYSCALL(notused, listen(listenfd, configureSet.maxConnections), "listen");
	
	/* se bind o listen hanno fallito esco dal programma */
	if(notused < 0) 
		return -1;

	connfdList = calloc(configureSet.maxConnections + 10, sizeof(pthread_mutex_t));

	tpCreator.cf = configureSet;
	tpCreator.function = spawn_thread;
	tpCreator.cleanup = clean_spawn;
	tpCreator.argt = (void *) &listenfd;
	tpCreator.argc = NULL;

	poolCreator((void *) &tpCreator);

	/* attendo segnale per la chiusura del server */
	while(1){
		sigwait(&setsignals, &sig);
		/* controllo se il segnale è SIGUSR1 dunque scrivo il file stats*/
		if(sig == SIGUSR1){
			/* se non c'è il file per il salvataggio delle statistiche ignoro tale segnale*/
			if(strcmp(configureSet.statFileName, "") != 0){
				FILE *statsFile = fopen(configureSet.statFileName, "a+");
			
				/* aggiorno statistiche */
				pthread_mutex_lock(&mutexStats);
				printStats(statsFile);
				pthread_mutex_unlock(&mutexStats);

				fclose(statsFile);	
			}
		}
		/* altrimenti chiudo direttamente il server*/
		else
			break;
	}

    	close(listenfd);

	/* se sono arrivato fin qui significa che sto chiudendo il server, distruggo il pool */
	poolDestroy((void *) &tpCreator);
	free(connfdList);
	return 0;
}

int main(int argc, char *argv[]) {

	if((char) getopt(argc, argv, "f") != 'f'){
		usage("./prova");
		return 0;
	}

	chattyStats.nusers = 0;
	/* leggo e acquisisco i dati dal file di configurazione */
	configureSet = readConfigFile(argv[optind]);
	MaxMsgDim = configureSet.maxHistMsg;

	/*inizializzo le due hash table*/
	userDB = new_ht_user();
	groupDB = new_ht_group();
	
	if(userDB == NULL || groupDB == NULL)
		return 0;	

	/*creo cartella per i messaggi da inviare agli utenti */
	mkdir(fpath, 0777);
	mkdir(configureSet.dirName, 0777);

	acceptMux = 1;
	nconn = 0;
	connfd_sel = 0;
	
	/* classe che contiene tutta la gestione della connessione del server */
	connectClient();
		
	/*elimino tutte le hash table*/
	if(userDB)
		delete_ht_user(userDB);
	if(groupDB)
		delete_ht_group(groupDB);

	/*rimuovo le directory e i file dentro su 'history' e il path dirName*/
	removeDirectory(fpath);
	removeDirectory(configureSet.dirName);

	return 0;
}
