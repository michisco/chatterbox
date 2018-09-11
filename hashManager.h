/**
 * @file hashManager.h
 * @brief File contenente definizioni di funzioni per la gestione di hash table
 *   
	\author Michele Morisco 505252 
       Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
       originale dell'autore  
     */  


#ifndef HASHMANAGER_H_
#define HASHMANAGER_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>

#include <message.h>
#include <config.h>
#include <info_user.h>
#include <fileManager.h>
#include <stats.h>
#include <configSetup.h>

#define INITIAL_SIZE                  50

/**
 * @function new_ht_user
 * @brief Crea nuova hash table per gli utenti registrati
 *
 * @return nuova hash table
 */
ht_user *new_ht_user();

/**
 * @function new_ht_group
 * @brief Crea nuova hash table per i gruppi
 *
 * @return nuova hash table
 */
ht_group *new_ht_group();

/**
 * @function insertUser
 * @brief Inserisce un nuovo utente
 *
 * @param ht hash table degli utenti
 * @param key chiave di item (username utente registrato)
 * @param user info utente    
 *
 * @return 0 esito negativo - utente già presente
 *         1 esito positivo - utente inserito 
 *         -1 errore
 */
int insertUser(ht_user *ht, const char *key, USER_INFO **user);

/**
 * @function insertGroup
 * @brief Inserisce un nuovo gruppo
 *
 * @param ht hash table dei gruppo
 * @param key chiave di item (nome gruppo)
 * @param group info gruppo    
 *
 * @return 0 esito negativo - utente già presente
 *         1 esito positivo - utente inserito 
 *         -1 errore
 */
int insertGroup(ht_group *ht, const char *key, GROUP_INFO **group);

/**
 * @function searchUser
 * @brief Cerca un utente all'interno di un hash table
 *
 * @param ht hash table degli utenti
 * @param key chiave di item (username utente registrato) 
 *
 * @return l'utente, se esiste
 */
USER_INFO *searchUser(ht_user *ht, const char *key);

/**
 * @function searchUserByFd
 * @brief Cerca un utente all'interno di un hash table tramite fd 
 *
 * @param fd file descriptor associato all'utente
 * @param ht hash table degli utenti  
 *
 * @return l'utente, se esiste
 */
USER_INFO *searchUserByFd(int fd, ht_user *ht);

/**
 * @function searchUserToGroup
 * @brief Cerca un utente all'interno di un hash table di un gruppo
 *
 * @param ht hash table degli utenti
 * @param key chiave di item (username utente registrato) 
 *
 * @return 1, se esiste altrimenti 0
 */
int searchUserToGroup(ht_user *ht, const char *key);

/**
 * @function searchGroup
 * @brief Cerca un gruppo all'interno di un hash table
 *
 * @param ht hash table dei gruppi
 * @param key chiave di item (nome gruppo registrato) 
 *
 * @return l'utente, se esiste
 */
GROUP_INFO *searchGroup(ht_group *ht, const char *key);

/**
 * @function removeUser
 * @brief Elimina un utente
 *
 * @param ht hash table degli utenti
 * @param key chiave di item (username utente registrato)   
 *
 * @return 0 esito negativo - non trovato 
 *         1 esito positivo - trovato e eliminato 
 *         -1 errore
 */
int removeUser(ht_user *htUser, ht_group *htGroup, const char *key);

/**
 * @function removeGroup
 * @brief Elimina un gruppo
 *
 * @param ht hash table dei gruppi
 * @param key chiave di item (nome gruppo registrato)   
 *
 * @return 0 esito negativo - non trovato 
 *         1 esito positivo - trovato e eliminato 
 *         -1 errore
 */
int removeGroup(ht_group *ht, const char *key);

/**
 * @function userConnection
 * @brief Aggiorna lo stato di connessione dell'utente
 *
 * @param ht hash table degli utenti
 * @param key chiave di item (username utente registrato)
 * @param fd file descriptor corrispondente dell'utente    
 *
 * @return 0 esito negativo - non trovato 
 *         1 esito positivo - trovato e aggiornato 
 *         -1 errore
 */
int userConnection(ht_user *ht, const char *key, int fd);

/**
 * @function userMsg
 * @brief Aggiorna la history dei messaggi dell'utente
 *
 * @param user info di un utente
 * @param newMsg nuovo messaggio da inserire nella history associata all'utente   
 *
 * @return 0 esito negativo - non trovato 
 *         1 esito positivo - trovato e aggiornato 
 *         -1 errore
 */
int userMsg(USER_INFO *user, MSG_INFO newMsg);

/**
 * @function addUserToGroup
 * @brief Iscrive un utente all'interno di un gruppo
 *
 * @param group info di un gruppo
 * @param user nome utente da aggiungere
 *
 * @return 0 esito negativo - non aggiornato - utente già presente
 *         1 esito positivo - aggiornato 
 *         -1 errore
 */
int addUserToGroup(GROUP_INFO *group, char *key);

/**
 * @function removeUserToGroup
 * @brief Disiscrive un utente all'interno di un gruppo
 *
 * @param group info di un gruppo
 * @param user nome utente da togliere
 *
 * @return 0 esito negativo - non trovato 
 *         1 esito positivo - trovato e aggiornato 
 *         -1 errore
 */
int removeUserToGroup(GROUP_INFO *group, const char *user);

/**
 * @function delete_ht_user
 * @brief Cancella hash table degli utenti
 *
 * @param ht hash table degli utenti  
 *
 * @return 0 esito positivo - hash table cancellata
 *         -1 errore
 */
int delete_ht_user(ht_user *ht);

/**
 * @function delete_ht_group
 * @brief Cancella hash table dei gruppi
 *
 * @param ht hash table dei gruppi  
 *
 * @return 0 esito positivo - hash table cancellata
 *         -1 errore
 */
int delete_ht_group(ht_group *ht);

/**
 * @function printListUser
 * @brief Crea la lista dei utenti da inviare  
 *
 * @param head lista degli utenti connessi
 * @param dim dimensione degli utenti connessi
 *
 * @return una stringa con tutti gli utenti connessi
 *
 */
char *printListUser(ht_user *curr, int *dim);

#endif /* HASHMANAGER_H_ */
