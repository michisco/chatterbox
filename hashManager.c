/**
 * @file hashManager.c
 * @brief File che contiene le funzioni delle interfacce in hashManager.h
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
#include <math.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <config.h>
#include <info_user.h>
#include <hashManager.h>
#include <fileManager.h>
#include <connections.h>
#include <stats.h>
#include <configSetup.h>

/*creo oggetti per definire gli elementi cancellati dalla hash table*/
static item_user DELETED_ITEM_USER = {NULL, NULL};
static item_group DELETED_ITEM_GROUP = {NULL, NULL};

/*funzione hash per trovare gli elementi*/
int f_hash(const char* s, const int a, const int n) {
    long hash = 0;
    const int len_s = strlen(s);
    for (int i = 0; i < len_s; i++) {
        hash += (long)pow(a, len_s - (i+1)) * s[i];
        hash = hash % n;
    }
    return (int)hash;
}

/*double hash*/
int get_hash(const char* s, const int num_buckets, const int attempt) {
    const int hash_a = f_hash(s, 157, num_buckets);
    const int hash_b = f_hash(s, 167, num_buckets);
    return (hash_a + (attempt * (hash_b + 1))) % num_buckets;
}

/*creo hash table per gli utenti con una dimensione fissata*/
ht_user* ht_user_sized(const int size) {
	
	/*controllo parametro*/
	if(size < 0)
		return NULL;
	ht_user *ht = malloc(sizeof(ht_user));
	if(ht == NULL)
		return NULL;
	ht->size = size;

    	ht->count = 0;
    	ht->items = calloc((size_t)ht->size, sizeof(item_user*));

	if(ht->items == NULL)
		return NULL;
   	return ht;
}

/*creo hash table per i gruppi con una dimensione fissata*/
ht_group* ht_group_sized(const int size) {
	
	/*controllo parametro*/
	if(size < 0)
		return NULL;
	ht_group *ht = malloc(sizeof(ht_group));
	if(ht == NULL)
		return NULL;
	ht->size = size;

    	ht->count = 0;
    	ht->items = calloc((size_t)ht->size, sizeof(item_group*));

	if(ht->items == NULL)
		return NULL;
   	return ht;
}

/*crea nuovo elemento utente da inserire nella hash table*/
item_user* new_item_user(const char *k, USER_INFO *user) {
	
	/*controllo parametri*/
	if(k == NULL)
		return NULL;
    	item_user *new = (item_user *) malloc(sizeof(item_user));
	if(new == NULL)
		return NULL;

    	new->username = strdup(k);
	new->user = user;
    	return new;
}

/*crea nuovo elemento gruppo da inserire nella hash table*/
item_group* new_item_group(const char* k, GROUP_INFO *group) {
	
	/*controllo parametri*/
	if(k == NULL || group == NULL)
		return NULL;
    	item_group *new = (item_group *) malloc(sizeof(item_group));
	if(new == NULL)
		return NULL;

    	new->name = strdup(k);
	new->group = group;
    	return new;
}

/*elimino l'elemento utente*/
void del_item_user(item_user* userItem) {
	free(userItem->user->msg_list);
	free(userItem->username);
	free(userItem->user);	
    	free(userItem);
}

/*elimino l'elemento utente*/
void del_item_group(item_group* groupItem) {
	delete_ht_user(groupItem->group->users);
	free(groupItem->name);
	free(groupItem->group);	
    	free(groupItem);
}

int delete_ht_user(ht_user *ht){
	/*controllo parametro*/
	if(ht == NULL)
		return -1;
	/*scorro hash table*/
    	for (int i = 0; i < ht->size; i++) {
    	   	item_user* item = ht->items[i];
		/*controllo che l'item abbia un oggetto dentro per poterlo eliminare*/
    	   	if (item != NULL){
			if(item != &DELETED_ITEM_USER)
    	      	 		del_item_user(item);
		} 
    	}
    	free(ht->items);
    	free(ht);
	
	return 0;
}

int delete_ht_group(ht_group *ht){
	/*controllo parametro*/
	if(ht == NULL)
		return -1;
    	for (int i = 0; i < ht->size; i++) {
    	   	item_group* item = ht->items[i];
		/*controllo che l'item abbia un oggetto dentro per poterlo eliminare*/
    	   	if (item != NULL){
			if(item != &DELETED_ITEM_GROUP)
    	      	 		del_item_group(item);
		} 
    	}
    	free(ht->items);
    	free(ht);
	
	return 0;
}

/*ridimensiono la hash table degli utenti*/
void ht_user_resize(ht_user* ht, const int size) {
 	
	/*controllo parametri*/
	if (size < INITIAL_SIZE || ht == NULL) {
        	return;
    	}
	
    	ht_user* new = ht_user_sized(size);
	/*scorro hash table*/
    	for (int i = 0; i < ht->size; i++) {
        	item_user* item = ht->items[i];
		/*controllo che l'item abbia un oggetto al suo interno*/
        	if (item != NULL && item != &DELETED_ITEM_USER){
			USER_INFO *old = malloc(sizeof(USER_INFO));
			strcpy(old->username, item->user->username);
			old->fd = item->user->fd;
			old->msg_list = (MSG_INFO *) malloc((item->user->maxHistMsg)*sizeof(MSG_INFO));

			/*inserisco ogni messaggio della history dell'utente nel nuovo item*/
			for(int j = 0; j < item->user->countMsg; j++)
				userMsg(old, item->user->msg_list[j]);
			old->currentIndex = item->user->currentIndex;
			old->countMsg = item->user->countMsg; 
			old->maxHistMsg = item->user->maxHistMsg; 
			/*inserisco nuovamente l'utente nella nuova hash table ridimensionata*/			
	   	    	insertUser(new, item->username, &old);
		}
    	}

	/*ora passo la nuova dimensione alla hash table passata per argomento*/
    	const int tmp_size = ht->size;
    	ht->size = new->size;
    	new->size = tmp_size;

	/*passo gli item alla hash table passata per argomento*/
    	item_user** tmp_items = ht->items;
    	ht->items = new->items;
    	new->items = tmp_items;
	
	/*cancello la vecchia hash table*/
    	delete_ht_user(new);
}

/*ridimensioniamo la hash table degli utenti*/
void ht_group_resize(ht_group* ht, const int size) {
 	
	/*controllo parametri*/
	if (size < INITIAL_SIZE || ht == NULL) {
        	return;
    	}
    	ht_group* new = ht_group_sized(size);
	/*scorro hash table*/
    	for (int i = 0; i < ht->size; i++) {
        	item_group* item = ht->items[i];
		/*controllo che l'item abbia un oggetto al suo interno*/
        	if (item != NULL && item != &DELETED_ITEM_GROUP){
			GROUP_INFO *old = malloc(sizeof(GROUP_INFO));
			strcpy(old->name, item->group->name);
			strcpy(old->host, item->group->host);
			old->users = new_ht_user();

			/*inserisco ogni utente del gruppo nel nuovo item*/
			for(int j = 0; j < item->group->users->size; j++){
				if(item->group->users->items[j] != NULL && item->group->users->items[j] != &DELETED_ITEM_USER)
					addUserToGroup(old, item->group->users->items[j]->username);
			}
			old->countUser = item->group->countUser;
			/*inserisco nuovamente il gruppo nella nuova hash table ridimensionata*/			
	   	    	insertGroup(new, item->name, &old);
		}
    	}

	/*ora passo la nuova dimensione alla hash table passata per argomento*/
    	const int tmp_size = ht->size;
    	ht->size = new->size;
    	new->size = tmp_size;

	/*passo gli item alla hash table passata per argomento*/
    	item_group** tmp_items = ht->items;
    	ht->items = new->items;
    	new->items = tmp_items;

	/*cancello la vecchia hash table*/
    	delete_ht_group(new);
}

/*raddoppio la dimensione della hash table degli utenti*/
void user_resize_up(ht_user* ht) {
    	const int new_size = ht->size * 2;
	ht_user_resize(ht, new_size);
}

/*dimezzo la dimensione della hash table degli utenti*/
void user_resize_down(ht_user* ht) {
    	const int new_size = ht->size / 2;
    	ht_user_resize(ht, new_size);
}

/*raddoppio la dimensione della hash table dei gruppi*/
void group_resize_up(ht_group* ht) {
    	const int new_size = ht->size * 2;
	ht_group_resize(ht, new_size);
}

/*dimezzo la dimensione della hash table dei gruppi*/
void group_resize_down(ht_group* ht) {
    	const int new_size = ht->size / 2;
    	ht_group_resize(ht, new_size);
}

ht_user *new_ht_user(){
	return ht_user_sized(INITIAL_SIZE);
}

ht_group *new_ht_group(){
	return ht_group_sized(INITIAL_SIZE);
}

int insertUser(ht_user *ht, const char *key, USER_INFO **user){

	/*controllo parametri*/
	if(ht == NULL || key == NULL)
		return -1;
	
	int load = ht->count * 100 / ht->size;
    	if (load > 70) 
        	user_resize_up(ht);

	/*se esiste già l'utente restituisce tale errore identificato con 0*/
	USER_INFO *rep = searchUser(ht, key);
	if(rep != NULL)
		return 0;

	/*preparo l'item dell'utente*/
	item_user* item = new_item_user(key, *user);
	if(item == NULL)
		return -1;

	/*ottengo l'indice per inserirlo nella hash table*/
    	int index = get_hash(item->username, ht->size, 0);
    	item_user* curr = ht->items[index];
    	int i = 1;
	/*controllo se non è già occupato da un altro item altrimenti trovo un altro indice*/
    	while (curr != NULL && curr != &DELETED_ITEM_USER) {
        	index = get_hash(item->username, ht->size, i);
        	curr = ht->items[index];
        	i++;
    	} 
    	ht->items[index] = item;
    	ht->count++;
	return 1;
}

int insertGroup(ht_group *ht, const char *key, GROUP_INFO **group){

	/*controllo parametri*/
	if(ht == NULL || key == NULL)
		return -1;
	
	int load = ht->count * 100 / ht->size;
    	if (load > 70) 
        	group_resize_up(ht);

	/*se esiste già il gruppo restituisce tale errore identificato con 0*/
	GROUP_INFO *rep = searchGroup(ht, key);
	if(rep != NULL)
		return 0;
	
	/*preparo l'item del gruppo*/
	item_group* item = new_item_group(key, *group);
	if(item == NULL)
		return -1;
	
	/*ottengo l'indice per inserirlo nella hash table*/
    	int index = get_hash(item->name, ht->size, 0);
    	item_group* curr = ht->items[index];
    	int i = 1;
	/*controllo se non è già occupato da un altro item altrimenti trovo un altro indice*/
    	while (curr != NULL && curr != &DELETED_ITEM_GROUP) {
        	index = get_hash(item->name, ht->size, i);
        	curr = ht->items[index];
        	i++;
    	} 
    	ht->items[index] = item;
    	ht->count++;
	return 1;
}

/*elimina i messaggi pendenti dell'utente key ancora non consegnati agli altri utenti*/
int deleteMsgs(ht_user *ht, const char *key){
	/*scorro hash table*/
	for(int i = 0; i < ht->size; i++){
		item_user* item = ht->items[i];
		if(item != NULL){
			if(item != &DELETED_ITEM_USER){
				/*controllo se l'utente ha dei messaggi nella history*/
				if(item->user->countMsg > 0){
					int nMsg = 0;
					MSG_INFO *temp = malloc(item->user->maxHistMsg*sizeof(MSG_INFO));
					int z = 0;
					/*scorro i messaggi della history*/
					for(int j = 0; j < item->user->maxHistMsg; j++){
						int fdFile = 0;
						/*i messaggi devono essere non letti*/
						if(item->user->msg_list[j].isRead == 0){
							fdFile = open(item->user->msg_list[j].path, O_RDONLY, 0666);
							if(fdFile > 0){
								message_t msg;
								/*leggo il messaggio*/
								if(readMsg(fdFile, &msg) > 0){
									/*controllo che il messaggio è stato inviato da l'utente eliminato e non lo inserisco nel nuovo array*/
									if(strcmp(msg.hdr.sender, key) != 0){
										temp[z] = item->user->msg_list[j];
										nMsg++;
										z++;
									}
									else	
										remove(item->user->msg_list[j].path);						
								}
								else 
									return -1;
							}
							close(fdFile);
						}
						else{
							temp[z] = item->user->msg_list[j];
							nMsg++;
							z++;
						}
			
					}
					free(item->user->msg_list);
					item->user->msg_list = temp;
					item->user->countMsg = nMsg;
				}
			}	
		}
	}
	return 1;
}

/*cancello l'utente eliminato dai gruppi che faceva parte*/
int deleteUserGroup(ht_group *ht, const char *key){
	/*scorro hash table*/
	for(int i = 0; i < ht->size; i++){
		item_group* item = ht->items[i];
		if(item != NULL){
			if(item != &DELETED_ITEM_GROUP){
				/*se l'utente è eliminato è l'host allora cancello il gruppo*/
				if(strcmp(item->group->host, key) == 0)
					removeGroup(ht, item->name);
				/*altrimenti cerco l'utente nel gruppo per eliminarlo*/
				else{
					USER_INFO *res = searchUser(item->group->users, key);
					if(res != NULL)
						removeUserToGroup(item->group, key);
				}	
			}	
		}
	}
	return 1;
}

int removeUser(ht_user *htUser, ht_group *htGroup, const char *key){
	
	/*controllo parametri*/
	if(htUser == NULL || key == NULL || htGroup == NULL)
		return -1;
	int load = htUser->count * 100 / htUser->size;
	if (load < 10) 
        	user_resize_down(htUser);
	
	int index = get_hash(key, htUser->size, 0);
    	item_user* item = htUser->items[index];
    	int i = 1;
    	while (item != NULL) {
        	if (item != &DELETED_ITEM_USER) {
			char name[MAX_NAME_LENGTH+1];
			strcpy(name, item->username);			
        		if (strcmp(name, key) == 0) {
				/*elimino l'item e assegno all'index vuoto l'oggetto dell'utente cancellato */
        		   	del_item_user(item);
        		        htUser->items[index] = &DELETED_ITEM_USER;
				htUser->count--;
				/*cancello messaggi pendenti inviati dall'utente eliminato*/
				if(deleteMsgs(htUser, name) < 0)
					return -1;
				/*cancello l'utente dai gruppi che faceva parte*/
				if(deleteUserGroup(htGroup, name) < 0)
					return -1;
				return 1;
        		}
        	}
        	index = get_hash(key, htUser->size, i);
        	item = htUser->items[index];
        	i++;
    	}
	 
    	return 0;
}

int removeGroup(ht_group *ht, const char *key){
	/*controllo parametri*/
	if(ht == NULL || key == NULL)
		return -1;
	int load = ht->count * 100 / ht->size;
	if (load < 10) 
        	group_resize_down(ht);
	
	int index = get_hash(key, ht->size, 0);
    	item_group* item = ht->items[index];
    	int i = 1;
    	while (item != NULL) {
        	if (item != &DELETED_ITEM_GROUP) {
			char name[MAX_NAME_LENGTH+1];
			strcpy(name, item->name);			
        		if (strcmp(name, key) == 0){
				/*elimino l'item e assegno all'index vuoto l'oggetto del gruppo cancellato */
        		   	del_item_group(item);
        		        ht->items[index] = &DELETED_ITEM_GROUP;
				ht->count--;
				return 1;
        		}
        	}
        	index = get_hash(key, ht->size, i);
        	item = ht->items[index];
        	i++;
    	} 
    	return 0;
}

USER_INFO *searchUser(ht_user *ht, const char *key){
	/*controllo parametri*/
	if(ht == NULL || key == NULL)
		return NULL;

	int index = get_hash(key, ht->size, 0);
    	item_user *item = ht->items[index];
    	int i = 1;
    	while (item != NULL) {
		if (item != &DELETED_ITEM_USER) {
        		if (strcmp(item->username, key) == 0)
        	  	  	return item->user;
		}
        	index = get_hash(key, ht->size, i);
        	item = ht->items[index];
        	i++;
    	} 
    	return NULL;
}

USER_INFO *searchUserByFd(int fd, ht_user *ht){	

	/*controllo parametri*/
	if(fd < 0 || ht == NULL) 
		return NULL;
	
	for(int i = 0; i < ht->size; i++){
		item_user *item = ht->items[i];
		if(item != NULL){
			if(item != &DELETED_ITEM_USER) {
				USER_INFO *user_temp = item->user;
				if(user_temp->fd == fd)
					return user_temp;	
			}
		}
	}
	
	return NULL;
}

int searchUserToGroup(ht_user *ht, const char *key){
	/*controllo parametri*/
	if(ht == NULL || key == NULL)
		return 0;

	int index = get_hash(key, ht->size, 0);
    	item_user *item = ht->items[index];
    	int i = 1;
    	while (item != NULL) {
		if (item != &DELETED_ITEM_USER) {
        		if (strcmp(item->username, key) == 0)
        	  	  	return 1;
		}
        	index = get_hash(key, ht->size, i);
        	item = ht->items[index];
        	i++;
    	} 
    	return 0;
}

GROUP_INFO *searchGroup(ht_group *ht, const char *key){
	/*controllo parametri*/
	if(ht == NULL || key == NULL)
		return NULL;
	
	int index = get_hash(key, ht->size, 0);
    	item_group *item = ht->items[index];
    	int i = 1;
    	while (item != NULL) {
		if (item != &DELETED_ITEM_GROUP) {
        		if (strcmp(item->name, key) == 0)
        	  	  	return item->group;
		}
        	index = get_hash(key, ht->size, i);
        	item = ht->items[index];
        	i++;
    	} 
    	return NULL;
}

/*restituisce l'indice dell'utente da cercare*/
int indexUser(ht_user *ht, const char *key){

	/*controllo parametri*/
	if(ht == NULL || key == NULL)
		return -1;

	int index = get_hash(key, ht->size, 0);
    	item_user *item = ht->items[index];
    	int i = 1;
    	while (item != NULL) {
		if (item != &DELETED_ITEM_USER) {
        		if (strcmp(item->username, key) == 0)
        	  	  	return index;
		}
        	index = get_hash(key, ht->size, i);
        	item = ht->items[index];
        	i++;
    	} 
    	return -1;
}

int userConnection(ht_user *ht, const char *key, int fd){
	
	/*controllo parametri*/
	if(ht == NULL || key == NULL)
		return -1;

	int index = indexUser(ht, key);
	if(index < 0)
		return 0;
	ht->items[index]->user->fd = fd;
    	return 1;
}

int userMsg(USER_INFO *user, MSG_INFO newMsg){

	/*controllo parametri*/
	if(user == NULL)
		return -1;

	int n = user->countMsg;
	int ind = user->currentIndex;
	
	/*controllo se posso inserire un messaggio*/
	if(n < user->maxHistMsg)
		n++;

	strcpy(user->msg_list[ind].path, newMsg.path);
	user->msg_list[ind].isFile =  newMsg.isFile;
	user->msg_list[ind].isRead = newMsg.isRead;
	/*controllo se si è arrivati alla fine dell'array*/
	if(ind == user->maxHistMsg - 1)
		ind = 0;
	else
		ind++;

	user->countMsg = n;
	user->currentIndex = ind;
	
	return 1;
}

int addUserToGroup(GROUP_INFO *group, char *key){
	
	/*controllo parametri*/
	if(key == NULL || group == NULL)
		return -1;	

	if(strcmp(key, group->host) == 0)
		return 0;
	
	USER_INFO *infoUser = NULL;
	int esito = insertUser(group->users, key, &infoUser);
	if(esito < 0)
		return esito;

	return 1;		
}

int removeUserToGroup(GROUP_INFO *group, const char *user){
	
	/*controllo parametri*/
	if(user == NULL || group == NULL)
		return -1;	

	ht_user *htUser = group->users;
	int load = htUser->count * 100 / htUser->size;
	if (load < 10) 
        	user_resize_down(htUser);
	
	int index = get_hash(user, htUser->size, 0);
    	item_user* item = htUser->items[index];
    	int i = 1;
    	while (item != NULL) {
        	if (item != &DELETED_ITEM_USER) {
			char name[MAX_NAME_LENGTH+1];
			strcpy(name, item->username);
			/*rimuovo l'utente dal gruppo*/			
        		if (strcmp(name, user) == 0) {
        		   	free(item->username);
				free(item);
        		        htUser->items[index] = &DELETED_ITEM_USER;
				htUser->count--;
				return 1;
        		}
        	}
        	index = get_hash(user, htUser->size, i);
        	item = htUser->items[index];
        	i++;
    	} 
    	return 0;
}

/*inserisco spazi in una stringa*/
void setBlank(char *str, int size){
	int i;
	int cancel = 0;
	for(i = 0; i < size; i++){
		if(cancel) str[i] = 0;
		else {
			if(str[i] == 0) cancel = 1;
		}
	}
}

char *printListUser(ht_user *curr, int *dim){
	
	char *result = malloc(sizeof(char)*(MAX_NAME_LENGTH + 2)); 
	if(result == NULL) 
		return NULL;

	char temp[MAX_NAME_LENGTH + 1];
	/* incremento per ogni utente aggiunto */
	int inc = 0; 
	/* inizializzo il buffer con una stringa vuota */
	result[0] = 0;
	*dim = 0;
	
	for (int i = 0; i < curr->size; i++) {
    	    	item_user *item = curr->items[i];
    	    	if (item != NULL){
			if(item != &DELETED_ITEM_USER) {
				USER_INFO *user_temp = item->user;
				if(user_temp->fd >= 0){
					inc++;
					strcpy(temp, item->username);
					strcat(&result[*dim], temp);
					/*riempo la stringa rimanente con degli spazi*/
					setBlank(&result[*dim], (MAX_NAME_LENGTH + 1));	
					result = realloc(result, (inc + 1)*(MAX_NAME_LENGTH + 1)+1);
					*dim = inc*(MAX_NAME_LENGTH + 1);
					result[*dim] = 0;	
				}

			}
		}	  	        
    	} 
	if (inc == 0)
   	  	strcpy(result, "Nessun utente connesso"); 
	return result;
}
