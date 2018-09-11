/**
 * @file fileManager.h
 * @brief File contenente definizioni di funzioni per la gestione dei file di utenti/gruppi
 *   
   \author Michele Morisco 505252 
       Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
       originale dell'autore  
     */  


#ifndef FILEMANAGER_H_
#define FILEMANAGER_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <message.h>
#include <config.h>
#include <info_user.h>
#include <stats.h>
#include <configSetup.h>
#include <hashManager.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

/**
 * @function readConfigFile
 * @brief Legge il file che contiene tutti le info 
 *	  sulla configurazione del server
 *
 * @param path percorso in cui risiede il file dove leggere le info 
 *
 * @return la struttura con tutte le informazioni acquisite dal file
 */
ConfigureSetup readConfigFile(char *path);

/**
 * @function removeDirectory
 * @brief Rimuove ogni singolo file all'interno di una directory.  
 *	  Eliminando la directory stessa.
 *
 * @param path percorso in cui risiede la directory
 *
 * @return 0 - esito positivo
 *	   -1 - esito negativo
 */
int removeDirectory(const char *path);
#endif /* FILEMANAGER_H_ */
