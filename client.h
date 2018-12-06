#include <arpa/inet.h>//Per operatori Internet
#include <ctype.h>//Dichiara funzioni utilizzate per la classificazione dei caratteri
#include <errno.h>//Per testare i codici di errore restituiti dalle funzioni di libreria
#include <fcntl.h>//Open  
#include <malloc.h>//Per allocazione dinamica della memoria
#include <netdb.h>//Utile per gethostbyaddr
#include <netinet/ip.h>//Contiene le strutture sockaddr_in e in_addr, utili per i socket TCP
#include <pthread.h>//Libreria per la gestione dei thread Posix
#include <signal.h>//Per la gestione dei segnali
#include <stdio.h>//Fornisce le funzionalità basilari di input/output del C
#include <stdlib.h>
#include <string.h>//Libreria per la manipolazione delle stringhe
#include <sys/socket.h>//Contiene la dichiarazione del dominio internet
#include <sys/stat.h>//Open
#include <sys/types.h>//Definita dallo standard Posix per la gestione dei tipi derivati,Open, Lseek
#include <sys/un.h>//Definizione per i socket Unix
#include <sys/wait.h>//Funzioni per la wait
#include <time.h>//Per convertire tra vari formati di data e ora
#include <unistd.h>//Write, Lseek, Close, Exit

#define FALSE 0
#define TRUE 1

void Logging(char *Nick);// Riga 185 del file client.c
void InLettere(char *LettereClient); //Riga 198 del file client.c


