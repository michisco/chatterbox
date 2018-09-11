/** 
 * @file configSetup.h
 * @brief File contenente la struttura del file di configurazione
 *   
   \author Michele Morisco 505252 
       Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
       originale dell'autore  
	*/

#ifndef CONFIGSETUP_H_
#define CONFIGSETUP_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define MAX_BUF  150

/**
 *  @struct configureSetup
 *  @brief struttura del file di configurazione
 *
 *  @var unixPath path per la creazione del socket AF_UNIX
 *  @var maxConnections numero massimo di connessioni pendenti 
 *  @var threadsPool numero di thread nel pool
 *  @var maxMsgSize dimensione massima di un messaggio testuale
 *  @var maxFileSize dimensione massima di un file accettato dal server
 *  @var maxHistMsg numero massimo che il server ricorda per ogni client 
 *  @var dirName directory dove salvare i file da inviare agli utenti
 *  @var statFileName file il quale verranno scritte le statistiche del server 
 */
typedef struct configure{

	char unixPath[MAX_BUF];
	int maxConnections;
	int threadsPool;
	int maxMsgSize;
	int maxFileSize;
	int maxHistMsg;
	char dirName[MAX_BUF];
	char statFileName[MAX_BUF];

}ConfigureSetup;

#endif /* CONFIGSETUP_H_ */
