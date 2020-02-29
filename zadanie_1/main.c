#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h> 

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>

#include <time.h>
#include <ctype.h>
#include <ctype.h>
#define RED    "\033[1;31m"
#define RED1    "\033[31m"
#define GREEN  "\033[32m"
#define YELLOW "\033[33m"
#define BLUE   "\033[1;34m"
#define RESET   "\033[0m"

#define reading_room 0
#define sreader 1
#define slibrary 2
#define schange 3

#define key 4528
#define keysem 0x123^(ftok("/tmp/",2)<<getpid())

#define N 5 // liczba procesorów
#define K 2 // pojemność półki na książki 

typedef struct {
  long type;
  char mtext;
} Library;

typedef struct{
int readers;
int books;
int ready;// czekanie na wszystkich potomków
int work[N]; // 0 - writer 1 - reader
int toread[K][N]; // tablica dwuwymiarowa, mówiąca który komunikat musi przeczytać dany proces
int message[K];// zabezpieczenie przed odebraniem złego komunikatu
} Memory;

int process;
Memory *memory;
Library library;
static struct sembuf sem;

// PODNOSZENIE SEMAFORA
void V(int semid, int semnum){
  sem.sem_num = semnum;
  sem.sem_op = 1;
  sem.sem_flg = 0;
  if (semop(semid, &sem, 1) == -1){
    perror("Error in V: Podnoszenie semafora");
    exit(1);}
}

// OPUSZCZANIE SEMAFORA
void P(int semid, int semnum){
  sem.sem_num = semnum;
  sem.sem_op = -1;
  sem.sem_flg = 0;
  if (semop(semid, &sem, 1) == -1){
    perror("Error in P: Opuszczenie semafora");
    exit(1);
  }            
}

// PISARZ
void writer(int semid,int msgid){
  P(semid,reading_room);

  for (int k =0; k<K; k++){
    if(memory->message[k]){
      if(memory->toread[k][process]){
        msgrcv(msgid, &library, sizeof(Library), k+1, 0);
        printf(RED"Writer %d"BLUE" read book nr %ld name \"%c\"\t"RESET,process, library.type, library.mtext);
        memory->toread[k][process]=0;

        int send=0;
        for (int p=0;p<N;p++)
          if(memory->toread[k][p]){ send=1; break;}
        
        if (send)
        {
          msgsnd(msgid, &library, sizeof(Library), 0);
          printf("\n");
        }
        else 
        {
          memory->message[k]=0;
          memory->books=k;V(semid,slibrary);
          printf(YELLOW"Remove book nr %d\n"RESET,k+1);
        }
        k=K;
        break;
      }            
    }
  }

  for (int j=0;j<N;j++)
    if(memory->message[j]==0)
      {memory->books=j;
      break;}
  if(memory->books<=K-1){
  P(semid,schange);
  int readerstoread=0;	
  for (int j=0;j<N;j++) 
    if(memory->work[j]) 
    { memory->toread[memory->books][j]=1;
      readerstoread++;
    }
  
  library.type=memory->books+1;
  library.mtext='a'+memory->books;
  if (msgsnd(msgid, &library, sizeof(Library), 0)==-1){ perror("Blad wyslania komunikatu"); exit(1); }

  if (readerstoread){

    P(semid,slibrary);
    printf(RED"Writer %d send book nr %ld name \"%c\"\t"RESET,process, library.type, library.mtext);
    memory->message[memory->books]=1;
  	memory->books=(memory->books+1)%K;
  }
  else
    printf(RED"Writer %d write book nr %ld name \"%c\" but don't send\t"RESET,process, library.type, library.mtext);

  printf(BLUE"Readers: ");
  for (int i =0;i<N;i++) 
  { if(memory->toread[memory->books-1][i]) printf("%d ",i);}
  printf("\n"RESET);

  V(semid,schange);}
  else
	    printf(RED"Writer %d don't write book - full shelf. books \"%d\" \n"RESET,process, memory->books );

  // if(memory->books > 0) V(semid,sreader);
  V(semid,reading_room);
}

// CZYTELNIK
void reader(int semid,int msgid){

  P(semid,sreader);   
  if(memory->readers==0) 
  	P(semid,reading_room);
  
  (memory->readers)++;
  V(semid,sreader);
  int nothing=0;
  for (int k =0; k<K; k++){
      nothing++;
    if(memory->message[k]){
      if(memory->toread[k][process])
      { 
      	nothing=K+1;
        msgrcv(msgid, &library, sizeof(Library), k+1, 0);
        printf(BLUE"Reader %d read book nr %ld name \"%c\"\t"RESET,process, library.type, library.mtext);
        memory->toread[k][process]=0;

        int send=0;
        for (int p=0;p<N;p++)
          if(memory->toread[k][p]) {send=1;  break; }
        
        if (send)
        {
          msgsnd(msgid,&library,sizeof(Library), 0);
          printf("\n");
        }
        else 
        {
          memory->message[k]=0;
          memory->books=k;
          V(semid,slibrary);
          printf(YELLOW"Remove book nr %d\n"RESET,k+1);
        }
        k=K;
        break;
      }
    }
  }

 if(nothing==K)
    printf(BLUE"Reader %d nothing to read\n"RESET,process);

  P(semid,sreader);
  (memory->readers)--; 
  if (memory->readers == 0) V(semid,reading_room);
  V(semid,sreader);
}

// ZWRACA FUNCKJĘ PROCESORA - CZYTELNIK/PISARZ
char* name(){
  if(memory->work[process]) return BLUE"reader"RESET;
  else return RED"writer"RESET;
}

// FUNKCJA FAZY RELAXU
void relax(int semid){
  srand(time(NULL)^(getpid()<<16));
  sleep(rand()%2+5);// faza relaksu

  P(semid,schange);
  int change=rand()%10;
  if (change<=4){ // prawdopodobieństwo na zmiane
    if(memory->work[process]) memory->work[process]=0;
    else memory->work[process]=1;}
  V(semid,schange);
}

// FUNKCJA FAZY KORZYSTANIA Z CZYTELNI
void working(int semid,int msgid){
  if(memory->work[process]) reader(semid,msgid);
  else  writer(semid,msgid);
}


int main(int argc, char* argv[]) {
  // Tworzenie pamięci współdzielonej
  int shmid;
  shmid = shmget(key, sizeof(memory), IPC_CREAT|0666);
  if (shmid == -1){
	perror("Utworzenie segmentu pamieci wspoldzielonej\n");
	exit(1);
  }	

  // Przyłączenie segmentu pamieci współdzielonej
  memory = (Memory*)shmat(shmid, NULL, 0);
  if (memory == NULL){perror("Przylaczenie segmentu pamieci wspoldzielonej");exit(1);}

  // Inicjalizacja zmiennych współdzielonych
  memory->books=0;
  memory->readers=0;
  memory->ready=0;
  for (int k=0;k<K;k++){
    for (int n=0;n<N;n++) memory->toread[k][n]=0;
    memory->message[k]=0;
  }

 
  // Tworzenie kolejki komunikatów
  int msgid;
  msgid=msgget(0x124,IPC_CREAT|0600);
  if (msgid == -1){
    perror("Utworzenie kolejki komunikatow");
    exit(1);
  }

  // Tworzenie semaforów 
  int semid;
  semid = semget(keysem, 4, IPC_CREAT | IPC_EXCL | 0666);
  if (semid == -1){
    perror("Utworzenie tablicy semaforow");
    exit(1);
  }
  else{
    if (semctl(semid, reading_room, SETVAL, (int)1) == -1){
      perror("Nadanie wartosci semaforowi czytelnia");
      exit(1);
    }
    if (semctl(semid, sreader, SETVAL, (int)1) == -1){
      perror("Nadanie wartosci semaforowi czytelnicy");
      exit(1);
    }
    if (semctl(semid, slibrary, SETVAL, (int)K) == -1){
      perror("Nadanie wartosci semaforowi biblioteka (K-liczba książek)");
      exit(1);
    }
    if (semctl(semid, schange, SETVAL, (int)1) == -1){
      perror("Nadanie wartosci semaforowi zmiany funkcji");
      exit(1);
    }
  }
 
  // Tworzenie procesów
  pid_t pid;
  for (process=0;process<N;process++)
  {
    pid=fork();
    if(pid == -1) {	perror("Problem przy tworzeniu procesu"); exit(1); }
    else if (pid==0){// potomek

      while(1){// czekaj dopóki wszystkie potomki nie wejdą
        if (memory->ready==N) break;
        else sleep(3);
      }

      while(1){      
        srand(time(NULL)^(getpid()<<16));// random

        int state = rand()%10;// losowanie funkcji
        if (state<=4) memory->work[process]=1;
        else memory->work[process]=0;
        
        printf(RESET"Proces %d - %s\n",process,name());
        relax(semid); // faza relaksu
        printf(RESET"Proces %d - %s after relax\n",process,name());
        working(semid,msgid);//faza korzystania z czytelni 
      }
    }
    else// proces macierzysty
    {
      printf("The parent process %d has ended \n",process);
      memory->ready++;
    }
  }

  int status;
  for (int i=0;i<N;i++)
  {
    pid=wait(&status);
    printf("Child (pid = %d) exited with status %d.\n", pid, status);
  }

  //Usuwanie pamieci i kolejki komunikatow
  shmctl(shmid,IPC_RMID,NULL);	
  for (int i=0;i<4;i++) semctl(semid,i,IPC_RMID);	
  msgctl(msgid, IPC_RMID, NULL); 
  return 0;
}