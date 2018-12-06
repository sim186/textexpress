#include "client.h"
int fd;

void foo(int s) //assegna l'handler a due segnali diversi
{
   int Lunghezza,ControlRW;
   char *Buffer,LungRead[8];
   Lunghezza=ControlRW=0;
   Buffer=NULL;

   if (s==SIGPIPE)
   {
      printf("\n......................................................................\n");
      printf("Server Disconnesso a causa di SIGPIPE \n");
      printf("\n......................................................................\n");
      kill(getpid(),SIGKILL);
   }
   else if(s==SIGALRM)
   {
      printf("\n.........................................................................\n");
      printf(" La partita è terminata, il server sta per inviare la classifica finale!\n");
      printf(".........................................................................\n");
      if((ControlRW=read(fd,LungRead,5))<0)//Lunghezza della classifica da leggere
         perror("Errore Read"),exit(1);
      else if (ControlRW == 0)
         perror("Server Disconnesso"),exit(1);  
      LungRead[5]='\0';       
      Lunghezza=atoi(LungRead);
      //Allocazione dinamica di buffer per leggere la classifica finale
      Buffer=(char *)malloc((Lunghezza+1)*sizeof(char));
      //Memorizzazione della classifica all'interno di buffer
      if((ControlRW=read(fd,Buffer,Lunghezza))<0)
         perror("Errore Read"),exit(1);
      else if (ControlRW == 0)
         perror("Server Disconnesso"),exit(1);      
      Buffer[Lunghezza]='\0';
      printf("Classifica Finale:\n%s\n",Buffer);
      close(fd);
      free(Buffer);
      kill(getpid(),SIGKILL);
   }
}


int main(int argc , char *argv[])
{  
   //Struttura per la memorizzazione dell'indirizzo del client
   struct sockaddr_in mio_indirizzo_client;
   struct hostent *Host;
   int Porta,Lunghezza,i,Tempo,ControlRW;
   char Nick[21], LettereClient[27],*Buffer,LungRead[64],LettereRand[7];
   Buffer=NULL;
   time_t time0,time1;
   Lunghezza=i=Tempo=ControlRW=0;
   //Assegniamo gli handler per alcuni segnali
   signal(SIGPIPE,foo);
   signal(SIGALRM,foo);
   //Controllo sul numero di argomenti ricevuti in input 
   if(argc != 3)
   {
      printf("USAGE: [nome/indirizzoIP Server] [Porta(es.5200)]\n");
      exit(1);
   }   
   //Riempimento della struttura sockaddr_in
   mio_indirizzo_client.sin_family = AF_INET;
   Host=gethostbyname(argv[1]);
   if(!Host) herror("gethostbyname"),exit(1);
   mio_indirizzo_client.sin_addr.s_addr = *(uint32_t *)(Host->h_addr_list[0]);
   //Argomento 2: Porta
   Porta = atoi(argv[2]);
   if((Porta < 5000) || (Porta > 32768))
   {  
      printf("La porta deve essere compresa tra 5000 e 32768\n");
      exit(1);
   }
   mio_indirizzo_client.sin_port = htons(Porta);
   //Creazione del socket   
   if ((fd =socket(PF_INET,SOCK_STREAM,0))< 0) 
   { 
      perror ("SOCKET");
      exit(1);
   }
   //Connessione a un dato indirizzo
   if((connect(fd,(struct sockaddr *) &mio_indirizzo_client,sizeof(mio_indirizzo_client)))<0)
   {    
       perror("Connect");
       exit(1);
   }
   //Funzione che restituisce il nick del client
   printf("\n\n\n\n\n\n\n\n\n\n\n");
   printf("----------------------------------------------------------\n");
   printf("          TEXT EXPRESS 1.0 - BENVENUTO A BORDO\n");
   printf("----------------------------------------------------------\n");
   Logging(Nick);
   //Comunicazione del nickname al server
   if((ControlRW=write(fd,Nick,20))<0)
      perror("Errore Write"),exit(1);
   //Riceviamo dal server il tempo rimanente al termine della partita     
   if((ControlRW=read(fd,LungRead,5))<0)
      perror("Errore Read Tempo"),exit(1);
   else if (ControlRW == 0)
      perror("Server Disconnesso"),exit(1);      
   Tempo=atoi(LungRead);
   alarm(Tempo);//Verrà inviato il segnale trascorso il tempo della partita
   if((ControlRW=read(fd,LungRead,5))<0)//Dimensione del file Informazioni Iniziali
      perror("Errore Read dim info"),exit(1);
   else if (ControlRW == 0)
      perror("Server Disconnesso"),exit(1);     
   Lunghezza=atoi(LungRead);
   //Allocazione dinamica di Buffer che conterrà le informazioni iniziali    
   Buffer=(char *)malloc((Lunghezza+1) * sizeof(char));
   if((ControlRW=read(fd,Buffer,Lunghezza))<0)
      perror("Errore Read"),exit(1);
   else if (ControlRW == 0)
      perror("Server Disconnesso"),exit(1);      
   Buffer[Lunghezza]='\0';
   printf("----------------------------------------------------------\n");
   printf("Benvenuto! Ecco i dettagli della partita in corso:\n%s\n",Buffer);
   printf("----------------------------------------------------------\n");
   //Salviamo le lettere random per stamparle continuamente al client
   strncpy(LettereRand,Buffer,6);
   LettereRand[6]='\0';
   time(&time1);
   free(Buffer);
   while(1)
   {
      time(&time0);
      printf("----------------------------------------------------------\n");
      printf("Tempo rimanente : %d secondi | ",(int )(Tempo - difftime(time0,time1)));
      printf("Lettere in gioco: %s\n",LettereRand);
      printf("----------------------------------------------------------\n");
      // Il client scrive la parola che viene salvata in LettereClient
      // dalla funzione InLettere e successivamente decodificata.
      InLettere(LettereClient);
      if(strcmp(LettereClient,"quit")==0)
      {   //Il client ha deciso di terminare la partita
          kill(getpid(),SIGINT);   
      }
      else if (strcmp(LettereClient,"info")==0)
      {  //Il client vuole la classifica aggiornata
         //quindi inviamo al server tale richiesta
         if(write(fd,LettereClient,27)< 0)
            perror("Errore write"),exit(1);   
         //Leggiamo la lunghezza della classifica
         if((ControlRW=read(fd,LungRead,5))<0)
            perror("Errore Read"),exit(1);
         else if (ControlRW == 0)
            perror("Server Disconnesso"),exit(1);      
         Lunghezza=atoi(LungRead);
         //Allocazione dinamica di buffer per memorizzare la classifica
         Buffer=(char *)malloc((Lunghezza+1)*sizeof(char));
         //Il server invia la classifica e la salviamo in buffer
         if((ControlRW=read(fd,Buffer,Lunghezza))<0)
            perror("Errore Read"),exit(1);
         else if (ControlRW == 0)
            perror("Server Disconnesso"),exit(1);      
         Buffer[Lunghezza]='\0';
         printf("Classifica Aggiornata:\n%s\n",Buffer);
         free(Buffer);
      }
      else if(strcmp(LettereClient,"help")==0)
      {  //L'utente vuole leggere il manuale del gioco
         printf("----------------------------------------------------------\n");
         printf("          TEXT EXPRESS - MANUALE D'USO\n");
         printf("----------------------------------------------------------\n");
         printf("1)Il nick name deve essere lungo almeno 4 caratteri\n");
         printf("2)Durante il gioco hai la possibilità di digitare parole speciali\n");
         printf("  Le parole speciali:\n");
         printf("   HELP per questa guida\n   INFO per la classifica aggiornata\n   QUIT per uscire\n");
         printf("3)Le parole inserite devono essere lunghe almeno 4 e massimo 26 caratteri\n");
         printf("4)Le parole non dovranno contenere caratteri accentati\n");
         printf("   Esempio per scrivere citta' ---> citta\n");
         printf("5)Al termine della partita verrà visualizzata la classifica finale\n");
      }   
      else 
      {  //Il client non ha digitato una parola speciale e quindi
         //ha inserito una parola per giocare.
         //Inviamo la parola al server che la controllerà
         if(write(fd,LettereClient,27)<0)
            perror("Errore write"),exit(1);
         time(&time0);
         if((difftime(time0,time1)) < Tempo)
         {  //Il server comunica il punteggio della parola
            if((ControlRW=read(fd,LungRead,60))<0)
               perror("Errore Read"),exit(1);
            else if (ControlRW == 0)
               perror("Server Disconnesso"),exit(1);
            printf("%s\n",LungRead);
         }
      } 
   }
   return 0;
}

/* Funzione Logging : funzione che preleva da stdin il nick
   e lo inserisce nell'array Nick */
void Logging(char *Nick)
{
   do//Acquisizione del nome fino a che la stringa inserita non sia minore di 4
   {
      printf("Inserire NickName (Max 20 - Min 4 lettere)$ ");
      scanf("%20s",Nick);//Preleva 20 caratteri 
      scanf("%*[^\n]");//Pulisce il buffer di stdin
   } 
   while((strlen(Nick))<4);
}

/* Funzione InLettere : funzione per la gestione del controllo della dimensione
   della parola inserita dal client */
void InLettere(char *LettereClient)
{
   int i=0;
   //Acquisizione della parola fino a che la stringa inserita non sia minore di 4
   do
   {
      printf("[QUIT]-Esci | [INFO]-Classifica | [HELP]-Manuale");
      printf("Inserire Parola (Min 4 - Max 26 lettere)$ ");
      scanf("%26s",LettereClient);//Preleva massimo 26 caratteri
      scanf("%*[^\n]");//Pulisce il buffer di stdin
   }
   while((strlen(LettereClient))<4);
   //Ciclo per la conversione in minuscolo della parola
   for (i=0;i<(strlen(LettereClient));i++)
   {  
     LettereClient[i]=tolower(LettereClient[i]);
   }  
}

