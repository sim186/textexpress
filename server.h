#include <arpa/inet.h>//Per operatori Internet
#include <ctype.h>//Dichiara funzioni utilizzate per la classificazione dei caratteri
#include <errno.h>//Per testare i codici di errore restituiti dalle funzioni di libreria
#include <fcntl.h>//Open  
#include <malloc.h>//Per allocazione dinamica della memoria
#include <netdb.h>//Utile per gethostbyaddr
#include <netinet/in.h>
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



#define TRUE 1
#define FALSE 0

struct Argomenti{
   int IdUtente;
};

struct Utente{
   int fdut;
   int Score;
   int Online;
   int Tempo;
   time_t Start;
   time_t StartMatch;
   char Nick[21];
   char Ip[128];   
   pthread_t tid;
};
void *GestionePartita(void *Arg_t);// Riga 258 del file server.c
void GestioneInformazioni(int IdUtente,int Tempo,char *NomeFile);// Riga 452 del file server.c
void GeneraLettere(char *Lettere);// Riga 498 del file server.c
int Score(char *Lettere,char *LettereClient);// Riga 588 del file server.c
int ParoleTotali(char *Lettere);//Riga 678 del file server.c
int CountLine(char *NomeFile);// Riga 742 del file server.c
char *itoa(int n, char *s);// Riga 764 del file server.c
void reverse(char *s);// Riga 781 del file server.c
void Cleanex();// Riga 796 del file server.c
void Classifica(char *NomeFile);// Riga 812 del server.c






