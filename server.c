#include "server.h"

char Lettere[7];//Array Lettere random
struct Utente *Utenti;//Struttura per la memorizzazione informazioni sull' utente
int fdGlobal = 0;//File Descriptor del Socket
int fdLog = 0;//File Descriptor del file di log
int NumeroUtenti=0;//Nuemero di utenti 
int ConnessioniAttive=0;//Numero di utenti connessi

//Dichiarazioni dei MUTEX
pthread_mutex_t Connessioni = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t AccessoFile = PTHREAD_MUTEX_INITIALIZER;

/* Funzione che gestisce l'handler di due segnali */
void foo(int s)
{
   time_t timetemp;
   int fd,fd2,i,k;
   char car,Buffer[256],Lunghezza[5],*BufferLog,*BufferClassifica;
   i=fd=fd2=k=0;

   if(s==SIGINT)
   {
       //Il server e' stato interrotto bruscamente, gestiamo questo evento.
       for(i=0;i<NumeroUtenti;i++)
       {
          fd2=Utenti[i].fdut;
          close(fd2);
       }   
       close(fdGlobal);
       //memorizzazione nel file di log delle informazioni di fine partita    
       time(&timetemp);
       BufferLog=ctime(&timetemp);
       strcpy(Buffer,"La Partita e' terminata bruscamente a causa di un'interruzione del server il: ");
       strcat(Buffer,BufferLog);
       strcat(Buffer,"--------------------------------------------------------------------------------");
       if(write(fdLog,Buffer,strlen(Buffer))<0)
          perror("Write Buffer"),exit(1);
       Buffer[0]='\n';    
       if(write(fdLog,Buffer,1)<0)
          perror("Write Buffer"),exit(1);
       close(fdLog);
       kill(getpid(),SIGKILL);
   }
   else if(s==SIGALRM)
   {   //Il server e' stato interrotto a causa del fine tempo partita
       //Generiamo la classifica finale per comunicarla ai client
       Classifica("Utente_ClassificaFinale.txt");
       k=0;   
       if ((fd=open("Utente_ClassificaFinale.txt",O_RDONLY,S_IRWXU ))<0)
          perror("Errore Apertura File Utente_ClassificaFinale.txt "), exit(1);
       BufferClassifica=(char *)malloc(50*NumeroUtenti*sizeof(char));
       while(read(fd,&car,1) > 0)
       {
          BufferClassifica[k]=car;
          k++;
       }
       BufferClassifica[k+1]='\0';
       itoa(k,Lunghezza);
       close(fd);
       //Comunichiamo la classifica finale a tutti i client connessi
       for(i=0;i<NumeroUtenti;i++)
       {
          if (Utenti[i].Online)
          {  
             if(write(Utenti[i].fdut,Lunghezza,5)<0)
                perror("Write Lunghezza 1");
             if(write(Utenti[i].fdut,BufferClassifica,k)<0)
                perror("Write Informazioni");
          }
       }
       free(BufferClassifica);
       /* Settiamo la variabile ConessioniAttive ad un valore default molto alto 
          poichè durante il regolare svolgimento della partita la variabile 
          indicava la presenza di client connessi per gestire un'eventuale
          chiusura anticipata del server. Tale eventualità non deve essere più
          verificata in quanto la partita è terminata */ 
       if(pthread_mutex_lock(&Connessioni)<0)
       {
          perror("Errore mutex lock connessioni");
          pthread_exit(NULL);
       }
       ConnessioniAttive=1000;
       if(pthread_mutex_unlock(&Connessioni)<0)
       {
          perror("Errore mutex unlock connessioni");
          pthread_exit(NULL);
       }   
       //Attendiamo la terminazione dei thread     
       for (i=0;i<NumeroUtenti;i++)
       {
          if(Utenti[i].Online)
          {   
             pthread_join(Utenti[i].tid,NULL);
             close(Utenti[i].fdut);
          }      
       }      
       free(Utenti);//Liberiamo lo spazio dell'array Utenti
       close(fdGlobal);
       //memorizzazione nel file di log delle informazioni di fine partita
       time(&timetemp);
       BufferLog=ctime(&timetemp);
       strcpy(Buffer,"La Partita e' terminata correttamente il: ");
       strcat(Buffer,BufferLog);
       strcat(Buffer,"--------------------------------------------------------------------------------");
       if(write(fdLog,Buffer,strlen(Buffer))<0)
           perror("Write Buffer"),exit(1);
       Buffer[0]='\n';    
       if(write(fdLog,Buffer,1)<0)
           perror("Write Buffer"),exit(1);
       close(fdLog);
       kill(getpid(),SIGKILL);
   }      
}

int main(int argc, char* argv[])
{
   struct sockaddr_in mio_indirizzo; //Struttura per l'indirizzo del server
   struct sockaddr_in indirizzo_client,indirizzo_temp;//Struttura per l'indirizzo del client
   struct hostent *ClientIpDomain;
   char *Ip,*Buffer,BufferLog[128];
   struct Argomenti *Arg;
   //Variabili per descrittore dei file,Porta,Tempo partita,parole restanti
   int fd2,fd3,Porta,Tempo,err,i;
   signal(SIGINT,foo);//Assegna l'handler per la ricezione futura del segnale SIGINT
   signal(SIGPIPE,SIG_IGN);//Assegna l'handler per la ricezione futura del segnale SIGPIPE
   signal(SIGALRM,foo);
   pid_t pid;//Variabile di tipo pid_t per il processo
   pthread_t tid;
   time_t time1,time0,timetemp;//Variabili per memorizzare informazioni temporali 
   fd2=fd3=pid=err=i=0;//Inizializzazione
   ClientIpDomain=NULL;
   //Controllo sul numero di argomenti ricevuti in input
   if(argc != 3)
   {
      printf("USAGE: [Porta(es.5200)] [Tempo in sec.(es.120)]\n");
      exit(1);
   }
   //Controllo sul primo argomento : Porta
   Porta= atoi(argv[1]);
   if((Porta < 5000) || (Porta > 32768))
   {  
      printf("La porta deve essere compresa tra 5000 e 32768\n");
      exit(1);
   }
   //Controllo sul secondo argomento : Tempo partita 
   Tempo=atoi(argv[2]);
   if((Tempo < 60) || (Tempo>600))
   {
      printf("Il tempo deve essere compreso tra 60(1 Minuto) e 600(10 Minuti)");
      exit(1);
   }
   //Funzione che cancella i file creati da una precedente partita
   Cleanex(); 
   //Creazione del socket
   if ((fdGlobal =socket(PF_INET,SOCK_STREAM,0))< 0) 
   { 
        perror ("SOCKET");
        exit(1);
   }
   socklen_t client_size= sizeof (indirizzo_client);
   //Riempimento della struttura sockaddr_in
   mio_indirizzo.sin_family = AF_INET;
   mio_indirizzo.sin_port=htons(Porta);
   mio_indirizzo.sin_addr.s_addr= htonl(INADDR_ANY);
   //Assegnazione dell'indirizzo al socket
   if(bind(fdGlobal,(struct sockaddr *) &mio_indirizzo,sizeof(mio_indirizzo))< 0)
      perror("Errore BIND"),exit(1);
   //Mettersi in ascolto
   if(listen(fdGlobal,10) < 0)
      perror("Errore LISTEN"),exit(1);
   //Creazione del file logparole per tenere traccia delle parole inserite
   if((fd3=open("logparole.txt",O_RDWR | O_CREAT | O_APPEND | O_SYNC , S_IRWXU )) < 0)
      perror("Errore Open LogParole"),exit(1);  
   //Funzione che genera le sei lettere random
   GeneraLettere(Lettere);
   //Funzione che crea il file con tutte le parole esatte
   ParoleTotali(Lettere);
   time(&time1);//Acquisizione tempo inizio della partita
   alarm(Tempo);//Verrà inviato il segnale trascorso il tempo della partita
   // Apertura e creazione del file di log del server con tutte le attività
   if((fdLog=open("log.txt",O_RDWR | O_CREAT | O_APPEND | O_SYNC , S_IRWXU )) < 0)
      perror("Errore Open Log"),exit(1);
   // Informazioni di inizio partita da inserire nel file di log
   time(&timetemp);
   Buffer=ctime(&timetemp);
   if(write(fdLog,"La partita e' cominciata il: ",29)<0)
      perror("Write Lunghezza 1"),exit(1);
   if(write(fdLog,Buffer,strlen(Buffer))<0)
      perror("Write Lunghezza 1"),exit(1);
   //Inizia il ciclo che gestisce la connessione dei client e la partita
   do
   {  //Ricezione connessione da parte di un client
      if((fd2=accept(fdGlobal,(struct sockaddr *) &indirizzo_client,&client_size))<0)
         perror("Errore Accept"),exit(1);
      if((difftime(time0,time1)) < Tempo)
      {  //Un client si è connesso registriamo le informazioni nel file di log
         indirizzo_temp=indirizzo_client;
         ClientIpDomain=gethostbyaddr(&(indirizzo_client.sin_addr),sizeof(indirizzo_client.sin_addr),AF_INET);
         if( ClientIpDomain != NULL )
         { 
            Ip=(char*)malloc((strlen(ClientIpDomain->h_name))*sizeof(char)); // Dominio del client
            strcpy(Ip,ClientIpDomain->h_name);
         }
         else
         {
            Ip=(char*)malloc(16*sizeof(char));
            Ip=inet_ntoa(indirizzo_temp.sin_addr);
         }
         time(&timetemp);// Ora e data di connessione
         Buffer=ctime(&timetemp);
         strcpy(BufferLog,Ip);
         strcat(BufferLog," si e' connesso alla partita il: ");
         strcat(BufferLog,Buffer);
         if(write(fdLog,BufferLog,strlen(BufferLog))<0)
            perror("Write Lunghezza 1"),exit(1);
         //Fine scrittura informazioni nel file di log del server
         if(pthread_mutex_lock(&Connessioni)<0)
            perror("Errore mutex lock connessioni"),exit(1);
         ConnessioniAttive++;//Variabile che conta i client attivi e connessi
         if(pthread_mutex_unlock(&Connessioni)<0)
            perror("Errore mutex unlock connessioni"),exit(1);
         //Allocazione della struttura Arg da passare ai thread
         Arg = (struct Argomenti *)malloc(sizeof(struct Argomenti));
		   Arg->IdUtente = NumeroUtenti;
         NumeroUtenti++;//Indice per l'array di utenti
         //Ampliamo l'array di utenti allocando lo spazio per il nuovo client 
		   Utenti=(struct Utente *)realloc(Utenti,NumeroUtenti* sizeof(struct Utente));
		   //La struttura Utente relativa al client connesso viene inizializzata
		   strcpy(Utenti[NumeroUtenti-1].Nick,"NICK_DEFAULT");//Nickname
		   Utenti[NumeroUtenti-1].fdut = fd2;//fd per le comunicazioni
		   Utenti[NumeroUtenti-1].Online = TRUE;//indica lo stato del client
		   time(&(Utenti[NumeroUtenti-1].Start));//tempo di connessione
		   Utenti[NumeroUtenti-1].StartMatch = time1;//tempo inizio partita
		   Utenti[NumeroUtenti-1].Tempo = Tempo;//durata partita  
		   Utenti[NumeroUtenti-1].Score=0;//Punteggio iniziale
		   strcpy(Utenti[NumeroUtenti-1].Ip,Ip);//Proprio indirizzo IP
		   //free(Ip);
         // Connesso un client creiamo il relativo thread
         if ( (err=pthread_create(&tid, NULL, GestionePartita,(void *)Arg)) != 0) 
         {
            perror("PTHREAD CREATE");
            exit(1);
         }
         time(&time0);//tempo attuale per la verifica del ciclo
      }      
   }
   while((difftime(time0,time1)) < Tempo);  

   return 0;
}

/* Funzione che gestisce il thread associato a ciascun client connesso,è qui che 
   si sviluppa l'intera partita, dall'immissione del nickname al calcolo dei
   punteggi delle parole e creazione delle classifiche. In caso di disconnessione
   del client il server continua la sua attività registrando questo evento.Al
   termine del tempo stabilito il server invia un segnale alarm gestito dall'handler
   foo che attende l'ultima lettura della classifica da parte di questa funzione 
   per poi far terminare questo thread. */ 
void *GestionePartita(void *Arg_t)
{
   int i,fd;
   time_t time0,timetemp;
   char LettereClient[27],FileInizio[32],car,Buffer[128],Lunghezza[5],BufferLog[128],*BufferTime,*BufferClassifica;//Array lettere ricevute dal client
   struct Argomenti *Arg;
   i=fd=0;
   Arg = (struct Argomenti *)Arg_t;
   Utenti[Arg->IdUtente].tid=pthread_self();
   // Lettura del Nickname
   if((i=read(Utenti[Arg->IdUtente].fdut,Buffer,20))<0)
   {
      perror("Errore Read Nick");
      pthread_exit(NULL);
   }
   else if (i == 0)//In caso di Pipe rotta e vuota
   {  //Il client si è disconnesso quindi il suo stato cambia
      Utenti[Arg->IdUtente].Online=FALSE;
      // Decrementiamo la variabile che conta i client attivi
      if(pthread_mutex_lock(&Connessioni)<0)
      {
         perror("Errore mutex lock connessioni");
         pthread_exit(NULL);
      }
      ConnessioniAttive--;
      if(pthread_mutex_unlock(&Connessioni)<0)
      {
         perror("Errore mutex unlock connessioni");
         pthread_exit(NULL);
      } 
      //Memorizzazione nel file di log la disconnessione del client
      time(&timetemp);
      BufferTime=ctime(&timetemp);
      strcpy(BufferLog,Utenti[Arg->IdUtente].Ip);
      strcat(BufferLog," si e' disconnesso il: ");
      strcat(BufferLog,BufferTime);
      if(write(fdLog,BufferLog,strlen(BufferLog))<0)
      {
         perror("Write Lunghezza 1");
         pthread_exit(NULL);
      }
      free(Arg); // Liberiamo Arg
      // Se le connessioni attive sono 0 allora significa che 
      // l'ultimo client connesso si è disconnesso e quindi il
      // server deve terminare.
      if(ConnessioniAttive==0)
         kill(getpid(),SIGINT);  
      pthread_exit(NULL);     
      return NULL;  
   }
   //Il nickname è stato ricevuto con successo, gli mettiamo quindi
   //il carattere di fine stringa e lo memorizziamo.
   Buffer[i]='\0';
   strcpy(Utenti[Arg->IdUtente].Nick,Buffer);
   //calcolo del tempo rimanente per il client 
   time(&time0);
   itoa((Utenti[Arg->IdUtente].Tempo)-(difftime(time0,(Utenti[Arg->IdUtente].StartMatch))),Buffer);
   
   if(write(Utenti[Arg->IdUtente].fdut,Buffer,5)<0)//Tempo rimanente della partita
   {
      perror("Errore Write Tempo");
      pthread_exit(NULL);
   }
   //La funzione scrive il file di "benvenuto" che il client visualizza
   //appena connesso.
   GestioneInformazioni(Arg->IdUtente,(Utenti[Arg->IdUtente].Tempo)-(difftime(time0,(Utenti[Arg->IdUtente].StartMatch))),FileInizio);
   //Apertura di file inizio con le info di benvenuto
   if((fd=open(FileInizio,O_RDONLY,S_IRWXU))<0)
   {
      perror("Apertura File NomeUtente");
      pthread_exit(NULL);
   }
   //Memorizzazione del file in un Buffer
   i=0;
   i=CountLine(FileInizio);
   BufferClassifica=(char *)malloc((i*100)*sizeof(char));
   i=0;
   //printf("Lseek=%d\n",i);
   while(read(fd,&car,1) > 0)
   {
      BufferClassifica[i]=car;
      i++;
   }
   BufferClassifica[i]='\0';
   itoa(i,Lunghezza);
   //Scriviamo prima la lunghezza di questo Buffer
   if(write(Utenti[Arg->IdUtente].fdut,Lunghezza,5)<0)
   {
      perror("Write Lunghezza 1");
      pthread_exit(NULL);
   }
   // e poi il Buffer vero e prorpio
   if(write(Utenti[Arg->IdUtente].fdut,BufferClassifica,i)<0)
   {
      perror("Write Informazioni");
      pthread_exit(NULL);
   }
   close(fd);
   free(BufferClassifica);
   //Una volta finite le operazioni iniziali siamo pronti a gestire
   //la partita e quindi a decodificare le giocate del client. Il
   //client infatti può inviare sia parole di gioco che comandi di
   //menu testuali.
   while(1)
   {  
      if((read(Utenti[Arg->IdUtente].fdut,&LettereClient,27))>0)
      {  //Se il client ha scritto info vuole la classifica
         if (strcmp(LettereClient,"info")==0)
         {
            //Generiamo quindi la classifica attuale  
            Classifica(FileInizio);
            //apriamo il file della classifica
            if((fd=open(FileInizio,O_RDONLY,S_IRWXU))< 0)
            {
               perror("Errore Apertura FileInizio");
               pthread_exit(NULL);
            }
            //memorizziamo il contenuto del file classifica in un buffer
            i=0; 
            BufferClassifica=(char *)malloc((50*NumeroUtenti) * sizeof(char));  
            while(read(fd,&car,1)>0)
            {
               BufferClassifica[i]=car;
               i++;
            }
            BufferClassifica[i+1]='\0';
            itoa(i,Lunghezza);
            //Scriviamo prima la lunghezza di questo Buffer
            if(write(Utenti[Arg->IdUtente].fdut,Lunghezza,5)<0)
            {
               perror("Write Lunghezza Informazioni");
               pthread_exit(NULL);
            }
            // e poi il Buffer vero e prorpio
            if(write(Utenti[Arg->IdUtente].fdut,BufferClassifica,i)<0)
            {
               perror("Errore Scrittura della Classifica");
               pthread_exit(NULL);
            }
            close(fd);  
            free(BufferClassifica);  
         }   
         else
         {  //Il client ha inviato una parola e bisogna calcolarne 
            //il punteggio, aggiornare il punteggio del client e 
            //comunicare il risultato al client.
				i=Score(Lettere,LettereClient);
            Utenti[Arg->IdUtente].Score=(Utenti[Arg->IdUtente].Score)+i;
            strcpy(Buffer,"Il punteggio della parola ");
            strcat(Buffer,LettereClient);
            strcat(Buffer," e' ");
            itoa(i,BufferLog);
            strcat(Buffer,BufferLog);
            if(write(Utenti[Arg->IdUtente].fdut,Buffer,60)<0)
               perror("Write Lunghezza Informazioni"),exit(1);
         }   
      }
      else break; //La read è fallita quindi il client è disconnesso
   }   
   //Se siamo usciti dal while è perchè il client si è disconnesso
   //e quindi procediamo con le operazioni di disconnessione client.
   Utenti[Arg->IdUtente].Online=FALSE;//Lo stato del client cambia
   //Decrementiamo le connessioni attive
   if(pthread_mutex_lock(&Connessioni)<0)
   {
      perror("Errore mutex lock connessioni");
      pthread_exit(NULL);
   }
   ConnessioniAttive--;
   if(pthread_mutex_unlock(&Connessioni)<0)
   {
      perror("Errore mutex unlock connessioni");
      pthread_exit(NULL);
   } 
   //Memorizzazione nel file di log della disconnessione
   time(&timetemp);
   BufferTime=ctime(&timetemp);
   strcpy(BufferLog,Utenti[Arg->IdUtente].Ip);
   strcat(BufferLog," si e' disconnesso il: ");
   strcat(BufferLog,BufferTime);
   if(write(fdLog,BufferLog,strlen(BufferLog))<0)
   {
      perror("Write Lunghezza 1");
      pthread_exit(NULL);
   }
   free(Arg);//Liberiamo la struttura Arg
   // Se le connessioni attive sono 0 allora significa che 
   // l'ultimo client connesso si è disconnesso; quindi il server deve terminare.
   if(ConnessioniAttive==0)
      kill(getpid(),SIGINT);
   pthread_exit(NULL);
   return NULL;
}

/* Questa funzione crea il file Utente_NomeUtenteID.txt il cui
   contenuto sarà: lettere random, secondi rimanenti, parole
   già indovinate, parole corrette ancora da indovinare. */
void GestioneInformazioni(int IdUtente,int Tempo,char *NomeFile)
{
   int fd,fd2,ParoleRimaste;
   char Buffer[4],car;
   fd=fd2=ParoleRimaste=0;
   //Creazione NomeFile Utente_NomeUtenteIDUtente.txt
   strcpy(NomeFile,"Utente_");
   strcat(NomeFile,Utenti[IdUtente].Nick);
   itoa(IdUtente,Buffer);
   strcat(NomeFile,Buffer);
   strcat(NomeFile,".txt");
   if((fd=open(NomeFile,O_RDWR | O_CREAT | O_SYNC,S_IRWXU))<0)
      perror("Apertura NomeFile"),exit(1);
   //Scrittura lettere random
   if(write(fd,Lettere,6)<0)
      perror("Errore Scrittura Lettere Random"),exit(1);
   if(write(fd," - queste sono le lettere random\n",33)<0)
      perror("Errore Scrittura Lettere Random2"),exit(1);
   //Scrittura tempo mancante
   itoa(Tempo,Buffer);
   if(write(fd,Buffer,strlen(Buffer))<0)
      perror("Errore Scrittura Tempo Mancante"),exit(1);
   if(write(fd," secondi mancanti al termine della partita.\nParole scoperte:\n",61)<0)
      perror("Errore Scrittura TempoMancante2"),exit(1);
   //Scrittura dal file logparole
   if((fd2=open("logparole.txt",O_RDONLY,S_IRWXU))<0)
      perror("Apertura logParole"),exit(1);
   while(read(fd2,&car,1) > 0)
   {
      write(fd,&car,1);
   }
   //Scrittura del numero di parole mancanti
   if(write(fd,"\n",1)<0)
      perror("Errore Scrittura numero parole mancanti"),exit(1);
   ParoleRimaste= (CountLine("Paroleok.txt") - CountLine("logparole.txt"));
   itoa(ParoleRimaste,Buffer);
   if(write(fd,Buffer,strlen(Buffer))<0)
      perror("Errore Scrittura Buffer"),exit(1);
   if(write(fd," - Parole corrette non scoperte.\n",33)<0)
      perror("Errore Scrittura numero parole mancanti"),exit(1);
   //chiusura dei file
   close(fd);
   close(fd2);   
}

/* Funzione GeneraLettere : Genera le sei lettere random composte da due vocali
   e quattro consonanti tutte diverse tra loro caricandole nell' array ricevuto
   in input */
void GeneraLettere(char *Lettere)
{
   //Array contenenti Consonanti e Vocali 
   char Vocali[5]={'a','e','i','o','u'};
   char Consonanti[14]={'b','c','d','f','g','l','m','n','p','r','s','t','v','z'};
   int i,j,k,flag;
   
   i=j=k=flag=0;
   //Generazione di un numero random per le vocali
   srand(time(NULL));
   i=rand()% 5;
   //Generazioni delle due vocali random
   Lettere[0]=Vocali[i];
   Lettere[1] = Lettere[0];
   do // Evitare di generare due vocali uguali
   {
      i=rand()% 5;
      if (Vocali[i] != Lettere[0])
      {
         Lettere[1]=Vocali[i];
      }
   }
   while(Lettere[0] == Lettere[1]);
   //Generazione di un numero random per le consonanti
   i=rand()% 14;
   //Inizializazione delle consonanti
   for (j=2;j<6;j++)    
      Lettere[j]=Consonanti[i]; 
   j=2; 
   //Inserimento delle consonanti 
   while(j<6)
   {  
      i=rand()% 14;
      k=2;
      flag=TRUE;
      //Controllo per evitare duplicazioni
      while(flag && k>=2 && k<6)
      {
         if (Consonanti[i] == Lettere[k])
         flag=FALSE;
         k++;
      }
      if (flag)
      {
         j++;
         if (j<6)
            Lettere[j]=Consonanti[i];
      }   
   }
   Lettere[6]='\0';//Inserimento carattere di fine stringa
}

/* Funzione Score : Ritorna un intero riguardante il punteggio della parola
   ricevuta in input (LettereClient).
   Ritornerà 0 se: 
   -la parola non è composta dalle sei lettere random
   -la parola è già stata inserita da un altro utente
   -la parola non è presente nel dizionario
   Altrimenti assegna il punteggio:
   - 2 punti consonante ,1 punto vocale */
int Score(char *Lettere,char *LettereClient)
{
   char Vocali[5]={'a','e','i','o','u'};
   int InDizionario,InLogparole,flag;
   int i,j,k,Punteggio,lung,fd,fd1; 
   char Buffer[64];
   pid_t pid;
   flag=TRUE;
   i=j=k=InDizionario=InLogparole=lung=fd=fd1=Punteggio=0; 
   lung=strlen(LettereClient);
   // Controllo se la parola è formata dalle 6 lettere random
   while(flag && i<lung)
   {
      k=0; 
      for(j=0;j<6;j++)
         if (LettereClient[i]== Lettere[j])
            k++;
      if (k==0) 
         flag=FALSE; 
	   i++; 
   }
   //Buffer contiene l'argomento di AWK
   strcpy(Buffer,"$1 == \"");
   strcat(Buffer,LettereClient);
   strcat(Buffer,"\" {a=a+1} END {print a}");
   //Se la parola è composta dalle sei lettere random
   if (flag)
   {
      // CONTROLLO SE LA PAROLA E' NEL FILE logparole.txt
      // quindi se un altro utente ha già digitato questa parola
      if ((pid=fork())<0)
         perror("Fork"),exit(1);
      if (pid == 0)
      {  //Il file controlloparole servirà per il controllo di logparole
    	   if(pthread_mutex_lock(&AccessoFile)<0)
                 perror("Errore mutex lock Classifica"),exit(1);
    	   if((fd=open("controlloparole.txt", O_RDWR | O_CREAT | O_SYNC | O_TRUNC, S_IRWXU))<0)
    	      perror("Errore Apertura file controlloparole"),exit(1);
   	   dup2(fd,STDOUT_FILENO);
         if ( execlp("awk","awk",Buffer,"logparole.txt",NULL) < 0)
      	   perror("ERRORE"),exit(1);
      	if(pthread_mutex_unlock(&AccessoFile)<0)
            perror("Errore mutex lock Classifica"),exit(1);
      }    
      wait(0);
      //Apertura del file controllo parole in sola lettura
      if(pthread_mutex_lock(&AccessoFile)<0)
                 perror("Errore mutex lock Classifica"),exit(1);
      if((fd1=open("controlloparole.txt", O_RDONLY , S_IRWXU))<0)
         perror("Apertura ControlloParole"),exit(1);
      //Lettura del primo carattere del file controlloparole
      if(read(fd1,Buffer,1)<0)
         perror("Errore Lettura ControlloParole"),exit(1);
      if(pthread_mutex_unlock(&AccessoFile)<0)
         perror("Errore mutex lock Classifica"),exit(1);
      //Se la parola è già presente nel file logparole
      if (Buffer[0]== '1')
         InLogparole= TRUE;
      // CONTROLLO SE LA PAROLA E' PRESENTE NEL DIZIONARIO
      //Buffer contiene l'argomento di AWK
      strcpy(Buffer,"$1 == \"");
      strcat(Buffer,LettereClient);
      strcat(Buffer,"\" {a=a+1} END {print a}");
   
      if ((pid=fork())<0)
         perror("Fork"),exit(1);
      if (pid == 0)
      {
         if(pthread_mutex_lock(&AccessoFile)<0)
            perror("Errore mutex lock Classifica"),exit(1);
         if((fd=open("controllo.txt",O_RDWR | O_CREAT | O_SYNC | O_TRUNC, S_IRWXU))<0)
            perror("Errore Apertura file Controllo"),exit(1);
         dup2(fd,STDOUT_FILENO);
         if ( execlp("awk","awk",Buffer,"Paroleok.txt",NULL) < 0)
      	   printf("ERRORE");
      	if(pthread_mutex_unlock(&AccessoFile)<0)
         perror("Errore mutex lock Classifica"),exit(1);
      }    
      wait(0);
      //Apertura del file controllo in sola lettura 
      if(pthread_mutex_lock(&AccessoFile)<0)
         perror("Errore mutex lock Classifica"),exit(1);
      if((fd1=open("controllo.txt",O_RDONLY , S_IRWXU))<0)
         perror("Errore Apertura file controllo"),exit(1); 
      //Lettura del primo carattere del file controllo
      if(read(fd1,Buffer,1)<0)
         perror("Errore Lettura Controllo"),exit(1);
      if(pthread_mutex_unlock(&AccessoFile)<0)
         perror("Errore mutex lock Classifica"),exit(1);
      //Se la parola è presente nel dizionario      
      if (Buffer[0]== '1')
         InDizionario= TRUE;
      // SE LA PAROLA E' NEL DIZIONARIO E NESSUNO L'AVEVA GIA INSERITA
      // Inseriamo la parola nel file logparole.txt e assegnamo il punteggio
      if(InDizionario && !InLogparole)
      {
         //Apertura in append di logparole per inserire la parola
         if(pthread_mutex_lock(&AccessoFile)<0)
         perror("Errore mutex lock Classifica"),exit(1);
         if((fd=open("logparole.txt",O_RDWR |O_APPEND | O_SYNC , S_IRWXU))<0)
            perror("Errore Apertura File LogParole"),exit(1);
         if(write(fd,LettereClient,lung)<0)
            perror("Errore Scrittura LogParole"),exit(1);
         if(write(fd,"\n",1)<0)
            perror("Errore Scrittura LogParole"),exit(1);
         close(fd);
         if(pthread_mutex_unlock(&AccessoFile)<0)
         perror("Errore mutex lock Classifica"),exit(1);
         Punteggio=lung;
         //Calcolo del punteggio
         for(i=0;i<lung;i++)
         {
            k=0;
            for(j=0;j<5;j++)
               if (LettereClient[i] == Vocali[j])
                  k++;
            if (k==0)
               Punteggio++;
         }
      }
   }  
   return Punteggio;  
}


/* Funzione ParoleTotali : Riceve in input le sei lettere random (Lettere), 
   filtra dal file Dizionario tutte le parole possibili con quelle sei lettere,
   contandole e ritornando tale valore. Tali parole saranno salvate nel file
   Paroleok.txt */
int ParoleTotali(char *Lettere)
{
   int fd,i,k,flag,IsOk,ParoleOk,fd1;
   char Car, Buffer[28];
   fd=i=ParoleOk=fd1=flag=IsOk=k=0;
   // Apertura in lettura del file dizionario.txt
   if((fd=open("dizionario.txt",O_RDONLY))<0)
      perror("Errore Apertura Dizionario"),exit(1);
   //Ciclo che itera finchè la read ha successo 
   while(read(fd,&Car,1) > 0)
   { 
      i=0;
      IsOk=TRUE;
      //Riposizionameto dell' offset all'interno del file
      if(lseek(fd,-1,SEEK_CUR)<0)
         perror("Errore Lseek"),exit(1);
      do
      {
         if(read(fd,&Car,1)<0)
            perror("Errore Read"),exit(1);
         Buffer[i]=Car;
         k=0;
         flag=FALSE;
         //Controllo che il carattere acquisito sia presente nelle lettere random
         while(!flag && k<6)
         {
            if(Buffer[i]== Lettere[k] || Buffer[i]=='\n')
                flag=TRUE;
            k++;
         }
         if(!flag)
            IsOk=FALSE;
         i++;
      }
      while(Car != '\n');//acquisizione fino al carattere NewLine
      Buffer[i]='\0'; 
      //Se la parola è composta dalle lettere random  
      if(IsOk)
      {  
         //Apertura del file Paroleok.txt
         if((fd1=open("Paroleok.txt" , O_RDWR | O_CREAT | O_APPEND | O_SYNC , S_IRWXU))<0)
            perror("Apertura File ParoleOk"),exit(1);
         //Inserimento della parola nel file
         if(write(fd1,Buffer,strlen(Buffer))<0)
            perror("Errore Write"),exit(1);
         ParoleOk++;//Aggiornamento del numero di parole 
      }
   }   
   return ParoleOk;
}

/*-----------------------------------------------------------
   Funzione CountLine : Ricevuto il nome di un file in input
   restituisce il numero di linee del file.
-----------------------------------------------------------*/ 
int CountLine(char *NomeFile)
{
   int fd,NumeroLog;
   char Car;
   fd=NumeroLog=0;
   //Apertura del file logparole.txt
   if((fd=open(NomeFile,O_RDONLY))<0)
      perror("Errore Apertura File"),exit(1);
   //Ciclo che itera finchè la read ha successo    
   while(read(fd,&Car,1) > 0)
   {
      if(Car=='\n')//Se il carattere e' NewLine
         NumeroLog++;//Aggiornamento del numero di parole 
   }     
   return NumeroLog;
}


/*Fonte Sito Web: www.pierotofy.it --------------------------
  La funzione ricevuto un intero n in Input lo converte in 
  caratteri. La funzione è la duale della nota atoi       
-----------------------------------------------------------*/
char *itoa(int n, char *s)
{
   int i, sign;
   if ((sign = n) < 0)   /* tiene traccia del segno */
      n = -n;   /* rende n positivo */
   i = 0;
   do {   /* genera le cifre nell'ordine inverso */
      s[i++] = n % 10 + '0';   /* estrae la cifra seguente */
   } while ((n /= 10) > 0);   /* elimina la cifra da n */
   if (sign < 0)
      s[i++] = '-';
   s[i] = '\0';
   reverse(s);
   return s;
}

/* Funzione ausiliaria della funzione itoa --------------*/
void reverse(char *s)
{
   int c, i, j;
   for (i = 0, j = strlen(s) - 1; i < j; i++, j--)
   {
      c = s[i];
      s[i] = s[j];
      s[j] = c;
   }
}

/*-----------------------------------------------------------
  Funzione che elimina i file creati durante l'esecuzione del
  programma. Lancia lo script Cleanex.sh
-----------------------------------------------------------*/
void Cleanex()
{
   pid_t pid; 
   if ((pid=fork())<0)
      perror("Fork"),exit(1);
   if (pid == 0)
      if ( execl("./Cleanex.sh","./Cleanex.sh",NULL) < 0)
      	perror("ERRORE"),exit(1);
   wait(0);
}

/* ---------------------------------------------------------
   Funzione che genera la classifica in tempo reale della 
   partita salvandola in un file il cui nome viene ricevuto
   in Input.
------------------------------------------------------------*/
void Classifica(char *NomeFile)
{
   int fd,i;
   char *Buffer,b[4];
   i=fd=0;
   Buffer=(char *)malloc((50*NumeroUtenti) * sizeof(char));
   if ( (fd = open(NomeFile, O_RDWR | O_CREAT | O_SYNC | O_TRUNC, S_IRWXU))< 0)
      perror("Errore Open"),exit(1);
   strcpy(Buffer,"Punteggio :\n");
   for(i=0;i<NumeroUtenti;i++)
   {
      strcat(Buffer,Utenti[i].Nick);
      strcat(Buffer,"\t\t");
      itoa(Utenti[i].Score,b);
      strcat(Buffer,b);
      strcat(Buffer,"\n");
   }  
   if ((write(fd,Buffer,strlen(Buffer))<0))
      perror("Errore write"),exit(1);
   free(Buffer);
   close(fd);     
}
