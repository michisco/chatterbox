 /** \file connections.c  
       \author Michele Morisco 505252 
       Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
       originale dell'autore  
     */  
#define _POSIX_C_SOURCE 200809L

#include <sys/types.h> 
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/un.h>

#define SYSCALL(r, c, e) \
	if((r=c)==-1) {perror(e);}

#include <message.h>
#include <connections.h>
#include <ops.h>

static inline int readn(long fd, void *buf, size_t size) {
    size_t left = size;
    int r;
    char *bufptr = (char*)buf;
    while(left>0) {
	if ((r=read((int)fd ,bufptr,left)) == -1) {
	    if (errno == EINTR) 
		continue;
	    if (errno == ECONNRESET || errno == EPIPE) /*errore nella connessione - chiusura*/
		return 0; 
	    return -1;
	}
	if (r == 0) return 0;   /*gestione chiusura socket o EOF*/
        left    -= r;
	bufptr  += r;
    }
    return size;
}

static inline int writen(long fd, void *buf, size_t size) {
    size_t left = size;
    int r;
    char *bufptr = (char*)buf;
    while(left>0) {
	if ((r=write((int)fd ,bufptr,left)) == -1) {
	    if (errno == EINTR) 
		continue;
	    if (errno == ECONNRESET || errno == EPIPE) /*errore nella connessione - chiusura*/
		return 0; 
	    return -1;
	}
	if (r == 0) return 0;  /*gestione chiusura socket o EOF*/
        left    -= r;
	bufptr  += r;
    }
    return 1;
}

int openConnection(char* path, unsigned int ntimes, unsigned int secs){
	
	int sockfd;

	/* controllo parametri */
	if(path == NULL || ntimes > MAX_RETRIES || secs > MAX_SLEEPING){
		errno = EINVAL;
		return -1;
	}
					
	/* creo il socket */
	SYSCALL(sockfd, socket(AF_UNIX, SOCK_STREAM, 0), "socket");

	/* setto l'indirizzo */
	struct sockaddr_un serv_addr;
	memset(&serv_addr, '0', sizeof(serv_addr));
	serv_addr.sun_family = AF_UNIX;
	strncpy(serv_addr.sun_path, path, strlen(path)+1);

	int i=0;
	/* assegno l'indirizzo al socket */
	while(i<ntimes){
		if(connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr))<0){ 
			i++; 
			sleep(secs);
		}
		else 
			return sockfd;	
	}
	return -1;
}

int readHeader(long connfd, message_hdr_t *hdr){

	/* controllo parametri */
	if(connfd < 0 || hdr == NULL){
		errno = EINVAL;
		return -1;	
	}
		
	int rep;
	char *sender = malloc(sizeof(char)*(MAX_NAME_LENGTH + 1));

	/*controllo che la malloc sia andata a buon fine*/
	if(sender == NULL){
		errno = ENOMEM;
		return -1;
	}
	
	SYSCALL(rep, readn(connfd, sender, (MAX_NAME_LENGTH+1)*sizeof(char)), "sender error4");
	/*controllo che la read sia andata a buon fine altrimenti restituisco l'esito della funzione (0 = connessione chiusa, -1 = errore)*/
	if(rep > 0){
		strcpy(hdr->sender, sender);
		SYSCALL(rep, readn(connfd, &(hdr->op), sizeof(op_t)), "sender error5");
	}
	else { 
		free(sender);
		return rep;
	}

	free(sender);
	return rep;
}

int readData(long fd, message_data_t *data){

	/* controllo parametri */
	if(fd < 0 || data == NULL){
		errno = EINVAL;
		return -1;
	}
	
	int rep;
	char *receiver = malloc(sizeof(char)*(MAX_NAME_LENGTH + 1));

	/*controllo che la malloc sia andata a buon fine*/
	if(receiver == NULL){
		errno = ENOMEM;
		return -1;
	}
	
	data->hdr.len = 0;
	SYSCALL(rep, readn(fd, receiver, (MAX_NAME_LENGTH+1)*sizeof(char)), "receiver1 error");
	
	if(rep > 0){
		strcpy(data->hdr.receiver, receiver);
		SYSCALL(rep, readn(fd, &(data->hdr.len), sizeof(int)), "lenght1 error");
	}
	else{
		free(receiver);
		return rep;
	}
	
	if(rep > 0){
		if(data->hdr.len == 0) 
			data->buf = NULL;
		else{
			data->buf = malloc(sizeof(char)*(data->hdr.len + 1));
			SYSCALL(rep, readn(fd, data->buf, data->hdr.len*sizeof(char)), "data1 error");
		}
	}
	else{ 
		free(receiver);
		return rep;
	}

	free(receiver);
	return rep;
}

int readMsg(long fd, message_t *msg){

	/* controllo parametri */
	if(fd < 0 || msg == NULL){
		errno = EINVAL;
		return -1;
	}	

	int esito;

	/* leggo lo header del messaggio */
	esito = readHeader(fd, &msg->hdr);
	/* se l'esito è positivo leggo il resto del messaggio*/
	if(esito > 0){ 
		esito = readData(fd, &msg->data);
	}

	return esito;	
}


int sendRequest(long fd, message_t *msg){

	/* controllo parametri */
	if(fd < 0 || msg == NULL){
		errno = EINVAL;
		return -1;
	}

	int rep;
	
	rep = sendHeader(fd, &msg->hdr);
	if(rep > 0)
		rep = sendData(fd, &msg->data);
	return rep;
}

int sendHeader(long fd, message_hdr_t *msg){

	/* controllo parametri */
	if(fd < 0 || msg == NULL){
		errno = EINVAL;
		return -1;
	}

	int rep = 1;
	char name[MAX_NAME_LENGTH+1]= "";
	
	message_hdr_t header = *msg;
	
	if(header.sender != NULL)
		strcpy(name, header.sender);
	else return -1; 

	SYSCALL(rep, writen(fd, name, (MAX_NAME_LENGTH+1)*sizeof(char)), "sender error1");
	
	if(rep > 0)
		SYSCALL(rep, writen(fd, &(header.op), sizeof(op_t)), "operation error");

	return rep;
}


int sendData(long fd, message_data_t *msg){

	/* controllo parametri */
	if(fd < 0 || msg == NULL){
		errno = EINVAL;
		return -1;	
	}	

	int rep = 1;
	char name[MAX_NAME_LENGTH+1] = "";
	message_data_t data = *msg;
	message_data_hdr_t dataHeader = data.hdr;
	unsigned int n = dataHeader.len;

	if(dataHeader.receiver != NULL)
		strcpy(name, dataHeader.receiver);
	else return -1;

	SYSCALL(rep, writen(fd, name, (MAX_NAME_LENGTH+1)*sizeof(char)), "receiver error");

	if(rep > 0)
		SYSCALL(rep, writen(fd, &n, sizeof(int)), "lenght error");
	
	if(n > 0 && rep > 0)
		SYSCALL(rep, writen(fd, data.buf, n*sizeof(char)), "buffer error");

	return rep;
}
