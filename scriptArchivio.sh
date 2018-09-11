#!/bin/bash

#controllo se ci sono gli argomenti oppure se l'utente ha digitato '--help'
if [ $# -eq 0 ] || [ $# -eq 1 ] || [ $1 = "--help" ]; then
	echo "use:"
	echo "$0 path_name time"
	echo "- path_name specifica il percorso dove è contenuto il file di configurazione"
	echo "- time specifica il tempo in minuti"
	exit 1
fi

#controllo se ci sono più di due argomenti
if [ "$#" -gt 2 ]; then
	echo "Troppi argomenti"
	exit 1
fi

#controllo che il file esista e sia regolare
if [ ! -f $1 ]; then
	echo "il file $1 non esiste o non e' un file regolare"
	exit 1
fi

#controllo che il file non sia vuoto
if [ ! -s $1 ]; then 
	echo "il file $1 e' vuoto"
	exit 1
fi

#salvo il secondo argomento (i minuti) in una variabile
t=$2

#controllo che l'intero sia maggiore di -1
if [ "$t" -le -1 ]; then 
	echo "$2 non e' un numero positivo"
	exit 1
fi

#salvo in due variabili i prefissi 'DirName' e '='
prefix="DirName"
prefix2="="

#cerco la riga nel file di configurazione che inizia con la parola 'DirName' e la salvo in una variabile
var1=$(grep ^DirName $1)

#cancello i due prefissi 'DirName' e '=' dal variabile in modo da ottenere il percorso della directory DirName
step="${var1//$prefix/}"
res="${step//$prefix2/}"

cd $res

#controllo che il tempo inserito sia uguale a 0 così stampo tutti i file presenti nella directory 
if [ "$t" -eq "0" ]; then
	find -type f -printf '%P\n'
#altrimenti archivio tutti i file (e directory) che sono più vecchi di t minuti e successivamente li cancello 
else
	#creo un archivio vuoto dopodiché appendo ogni file (o directory) che rispetta la condizione all'archivio
	tar -cf oldFile.tar.gz -T /dev/null 
	find * -mmin +$t -type f -exec tar -rf oldFile.tar.gz {} \;
	#cancello ogni file (ed eventuali directory) che sono state aggiunte nell'archivio
	find -mmin +$t -type f -exec rm -f {} \;
	#cancello le directory vuote
	find -empty -type d -delete
	
	#se l'archivio rimane vuoto allora lo cancello
	if ! tar -tf oldFile.tar.gz &> /dev/null; then
		rm oldFile.tar.gz	
	fi
fi

