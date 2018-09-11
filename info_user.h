/** 
 * @file info_user.h
 * @brief File contenente alcune define con informazioni riguardo agli utenti, ai gruppi che fanno parte e al file di configurazione
 *
    \author Michele Morisco 505252 
       Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
       originale dell'autore  
     */  
#ifndef INFO_USER_H_
#define INFO_USER_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <config.h>
#include <message.h>

/**
 *  @struct messagge_info
 *  @brief rappresenta le informazioni dei messaggi 
 *
 *  @var path percorso in cui risiede il messaggio
 *  @var isRead flag per determinare se il messaggio è stato letto
 *  @var isFile flag per determinare se si tratta di un messaggio testuale o di un file
 */
typedef struct message_info{
	char path[150];
	int isRead;
	int isFile;
}MSG_INFO;

/**
 *  @struct userI
 *  @brief rappresenta le informazioni sull'utente 
 *
 *  @var username nickname dell'utente
 *  @var fd file descriptor associato
 *  @var msg_list array di messaggi associati all'utente
 *  @var countMsg numero di messaggi nell'array 
 *  @var currentIndex indice che punta l'ultimo messaggio inserito
 *  @var maxHistMsg numero massimo di messaggi nella history
 */
typedef struct userI{
	char username[MAX_NAME_LENGTH+1];
	int fd;
	MSG_INFO *msg_list;
	int countMsg;
	int currentIndex;
	int maxHistMsg;
}USER_INFO;

/**
 *  @struct item_user
 *  @brief Item che contiene le informazioni degli utenti 
 *
 *  @var username nickname utente (chiave)
 *  @var user tipo utente 
 */
typedef struct {
	char *username;
	USER_INFO *user;
}item_user;

/**
 *  @struct ht_user
 *  @brief Hash table per gli utenti 
 *
 *  @var size dimensione della hash table
 *  @var count contatore degli elementi
 *  @var items gli utenti registrati
 */
typedef struct {
	int size;
	int count;
	item_user** items;
}ht_user;

/**
 *  @struct groupI
 *  @brief rappresenta le informazioni sul gruppo 
 *
 *  @var name nome del gruppo
 *  @var host nickname di chi ha creato il gruppo
 *  @var users hashtable degli utenti che fanno parte del gruppo
 *  @var countUser numero di utenti nel gruppo
 */
typedef struct groupI{
	char name[MAX_NAME_LENGTH+1]; 
	char host[MAX_NAME_LENGTH+1];
	ht_user *users;
	int countUser;
}GROUP_INFO;

/**
 *  @struct item_group
 *  @brief Item che contiene le informazioni dei gruppi
 *
 *  @var name nome gruppo (chiave)
 *  @var group tipo gruppo 
 */
typedef struct {
	char *name;
	GROUP_INFO *group;
}item_group;

/**
 *  @struct ht_group
 *  @brief Hash table per i gruppi
 *
 *  @var size dimensione della hash table
 *  @var count contatore degli elementi
 *  @var items gli utenti registrati
 */
typedef struct {
	int size;
	int count;
	item_group** items;
}ht_group;

#endif /* INFO_USER_H_ */
