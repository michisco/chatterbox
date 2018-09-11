/**
 * @file fileManager.c
 * @brief File che contiene le funzioni delle interfacce in fileManager.h
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

#include <config.h>
#include <info_user.h>
#include <fileManager.h>
#include <stats.h>
#include <configSetup.h>
#include <hashManager.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

ConfigureSetup readConfigFile(char *path){
	/* controllo parametro */
	if(path == NULL) 
		exit(EXIT_FAILURE);

	FILE *ifp;

	if((ifp = fopen(path, "r"))==NULL){
		perror("file in apertura");
		exit(EXIT_FAILURE);
	}
	
	char *buf = (char *) malloc((MAX_BUF + 1)*sizeof(char));
	ConfigureSetup cs;
	char *variable, *token, *tmpstr;
	char s[MAX_BUF + 1] = "";
	char temp[MAX_BUF + 1] = "";
 
	while(fgets(buf, MAX_BUF, ifp)!=NULL){ 
		/*se non inizia con '#' o non è una riga vuota significa che è un dato che ci interessa*/
		if(buf[0] != '#' && buf[0] != '\n' && buf[0] != ' '){
			//copio la riga letta in una stringa temporanea
			strcpy(temp, buf);
			//salvo il nome della variabile scritta nel file
			variable = strtok_r(buf, " ", &tmpstr);
			if(variable[strlen(variable)-1] == '\t' || variable[strlen(variable)-1] == ' ')
				variable[strlen(variable)-1]=0;

			token = strtok_r(temp, "=", &tmpstr);
			/*se ho raggiunto già la fine della riga significa che non ha nessun valore assegnato alla variabile*/
			if(token == NULL) 
				strcpy(s, " ");
			else{
				token = strtok_r(NULL, " ", &tmpstr);
				/*se ho raggiunto già la fine della riga significa che il valore assegnato è vuoto*/
				if(token[0] == '\n')
					strcpy(s, " ");
				else{
					//suddivido in token la riga rimanente
					while(token){
			                	//controllo se la prima lettera del token sia diversa da un blank
						if(token[0] != ' ' && token[0] != '\n' ){
								if(token[strlen(token)-1] == '\n')
									token[strlen(token)-1]=0;
								strcpy(s, token);
						}
						token = strtok_r(NULL, " ", &tmpstr);
					}
				}
			} 
			/*controllo se siano presenti tutte le variabili per la configurazione altrimenti chiudo ed esco dal programma*/
			if(strcmp(variable, "UnixPath") == 0){
				/*se il valore acquisito fosse vuoto allora imposto un valore di default*/
				if(s[0] == ' ' || s == NULL)
					strcpy(cs.unixPath, "/tmp/chatty_socket");
				else 
					strcpy(cs.unixPath, s);
			}
			else if(strcmp(variable, "MaxConnections") == 0){
				/*se il valore acquisito fosse vuoto allora imposto un valore di default*/
				if(s[0] == ' ' || s == NULL)
					cs.maxConnections = 32;
				else{
					int n = atoi(s);
					cs.maxConnections = n;
				}		
			}
			else if(strcmp(variable, "ThreadsInPool") == 0){
				/*se il valore acquisito fosse vuoto allora imposto un valore di default*/
				if(s[0] == ' ' || s == NULL)
					cs.threadsPool = 8;
				else{
					int n = atoi(s);
					/*se il valore è 0 allora imposto un valore di default*/
					if(n > 0)
						cs.threadsPool = n;
					else
						cs.threadsPool = 8;
				}
			}
			else if(strcmp(variable, "MaxMsgSize") == 0){
				/*se il valore acquisito fosse vuoto allora imposto un valore di default*/
				if(s[0] == ' ' || s == NULL)
					cs.maxMsgSize = 512;
				else{
					int n = atoi(s);
					cs.maxMsgSize = n;
				}
			}
			else if(strcmp(variable, "MaxFileSize") == 0){
				/*se il valore acquisito fosse vuoto allora imposto un valore di default*/
				if(s[0] == ' ' || s == NULL)
					cs.maxFileSize = 1024;
				else{
					int n = atoi(s);
					cs.maxFileSize = n;
				}
			}
			else if(strcmp(variable, "MaxHistMsgs") == 0){
				/*se il valore acquisito fosse vuoto allora imposto un valore di default*/
				if(s[0] == ' ' || s == NULL)
					cs.maxHistMsg = 16;
				else{
					int n = atoi(s);
					cs.maxHistMsg = n;
				}
			}
			else if(strcmp(variable, "DirName") == 0){
				/*se il valore acquisito fosse vuoto allora imposto un valore di default*/
				if(s[0] == ' ' || s == NULL)
					strcpy(cs.dirName, "/tmp/chatty");
				else 
					strcpy(cs.dirName, s);
			}
			else if(strcmp(variable, "StatFileName") == 0){
				/*se il valore acquisito fosse vuoto allora imposto una stringa vuota*/
				if(s[0] == ' ' || s == NULL)
					strcpy(cs.statFileName, "");
				else 
					strcpy(cs.statFileName, s);
			}
			else{
				perror("Nel file di configurazione ci sono una o più informazioni mancanti");
				free(buf);
				fclose(ifp);
				exit(EXIT_FAILURE);
			}
					
		}
	}		

	free(buf);
	fclose(ifp);
	return cs;	
}

int removeDirectory(const char *path){
	/*apro la directory corrispondente al percorso*/
	DIR *directory = opendir(path);
	size_t pathLen = strlen(path);
	int report = -1;

	if(directory){
		struct dirent *p;
		report = 0;

		/*utilizzo readddir per controllare se ci sono altre eventuali directory al suo interno*/
		while(!report && (p = readdir(directory))){
			int report2 = -1;
			char *buf;
			size_t len;

			/*salto le directory con nome '.' e '..'*/
			if(!strcmp(p->d_name, ".") || !strcmp(p->d_name, ".."))
				continue;
			len = pathLen + strlen(p->d_name) + 2;
			buf = malloc(len);
	
			if(buf){
				/*utilizzo stat dato che molti sistemi d_type non è supportato*/
				struct stat statBuf;
				snprintf(buf, len, "%s/%s", path, p->d_name);
		
				if(!stat(buf, &statBuf)){
					/*controllo se il percorso dato è una directory a quel punto ricorsivamente cancello la directory*/
					if(S_ISDIR(statBuf.st_mode))
						report2 = removeDirectory(buf);
					/*altrimenti elimino il file*/
					else
						report2 = unlink(buf);

				}
				free(buf);
			}
			report = report2;
		}
		/*chiudo la directory*/
		closedir(directory);
	}
	/*a quel punto quando la directory è vuota la elimino completamente*/
	if(!report)
		report = rmdir(path);
	return report;
}
