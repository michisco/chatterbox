/**
 * @file threadPoolHandler.c
 * @brief File che contiene le funzioni delle interfacce in threadPoolHandler.h
 *  
   \author Michele Morisco 505252 
       Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
       originale dell'autore  
     */ 

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>

#include <configSetup.h>
#include <threadPoolHandler.h>

/*variabili per la concorrenza tra i thread*/
static pthread_mutex_t mutexThread = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t conditionThread = PTHREAD_COND_INITIALIZER;

pthread_t *threads;
/*numero di thread chiusi*/
int closingThread;
pthread_attr_t attr;

void *poolCreator(void *arg){
	threadPoolCreator tpc = *(threadPoolCreator *) arg;
	closingThread = 0;
	ConfigureSetup cfs = tpc.cf;

	threads = (pthread_t *) malloc(cfs.threadsPool*sizeof(pthread_t));
	if(threads == NULL)
		return NULL;
	
	/*assegno ai thread lo stato detach */
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	for(int i = 0; i < cfs.threadsPool; i++){
		/*creo i thread worker*/
		if (pthread_create((&threads[i]), &attr, ThreadF, arg) != 0)
			fprintf(stderr, "pthread_create FALLITA");
	}
	return NULL;
}

void *ThreadF(void *arg){
	threadPoolCreator tpc = *(threadPoolCreator *) arg;

	pthread_cleanup_push(clean_upPool, &tpc);

	/*chiamo funzione passata come parametro*/
	if(tpc.function != NULL) 
		tpc.function(tpc.argt);

	pthread_cleanup_pop(0);

	return NULL;
}

void *poolDestroy(void *arg){
	threadPoolCreator tpc = *(threadPoolCreator *) arg;
	ConfigureSetup cfs = tpc.cf;

	for(int i = 0; i < cfs.threadsPool; i++){
		/*distruggo i thread*/
		if (pthread_cancel(threads[i]) != 0)
			fprintf(stderr, "pthread_cancel FALLITA");
	}

	/*concorrenza per la cancellazione di un thread*/
	pthread_mutex_lock(&mutexThread);
	while(tpc.cf.threadsPool > closingThread)
		pthread_cond_wait(&conditionThread, &mutexThread);
	pthread_mutex_unlock(&mutexThread);

	/*attendo un secondo per liberare memoria*/
	sleep(1);
	pthread_attr_destroy(&attr);
	free(threads);

	return NULL;
}

void clean_upPool(void *arg){
	threadPoolCreator tpc = *(threadPoolCreator *) arg;

	/*chiamo la cleanup passata come parametro*/
	if(tpc.cleanup != NULL) 
		tpc.cleanup(tpc.argc);
	
	pthread_mutex_lock(&mutexThread);
	closingThread++;
	pthread_cond_signal(&conditionThread);
	pthread_mutex_unlock(&mutexThread);
}

