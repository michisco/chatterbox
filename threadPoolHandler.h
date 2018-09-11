/** 
 * @file threadPoolHandler.h
 * @brief Contiene funzioni per creare o distruggere thread e per definire la struttura del pool
 *
   \author Michele Morisco 505252 
       Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
       originale dell'autore  
     */ 

#ifndef THREADPOOLHANDLER_H_
#define THREADPOOLHANDLER_H_

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>

#include <configSetup.h>

/**
 *  @struct threadPoolCreator
 *  @brief struttura di un pool di thread
 *
 *  @var receiver nickname del ricevente
 *  @var len lunghezza del buffer dati
 */
typedef struct threadPool{
	void *(*function) (void *);
	void *(*cleanup) (void *); 
	void *argc;
	void *argt;
	ConfigureSetup cf;
}threadPoolCreator;

/**
 * @function ThreadF
 * @brief rappresenta un singolo thread
 *
 * @param arg prende una struttura threadPool
 *
 */
void *ThreadF(void *arg);

/**
 * @function poolCreator
 * @brief crea un pool di thread
 *
 * @param arg prende una struttura threadPool
 *
 */
void *poolCreator(void *arg);

/**
 * @function poolDestroy
 * @brief distrugge un pool di thread
 *
 * @param arg prende una struttura threadPool
 *
 */
void *poolDestroy(void *arg);

/**
 * @function clean_upPool
 * @brief clean_up di un pool di thread
 *
 * @param arg prende una struttura threadPool
 *
 */
void clean_upPool(void *arg);

#endif /* THREADPOOLHANDLER_H_ */
