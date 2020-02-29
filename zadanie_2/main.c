#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include <errno.h>
#define test_errno(msg) do{if (errno) {perror(msg); exit(1);}} while(0)

#include <ctype.h>
#define RED    "\033[31m"
#define GREEN  "\033[32m"
#define YELLOW "\033[33m"
#define BLUE   "\033[34m"
#define RESET   "\033[0m"

#define KO 3 //liczba producentów tlenu
#define KH 3 //liczba producentów wodoru
#define N 10 //wielkość bufora

// Zmienne globalne
int bufO[N], bufH[N]; // tablice producentow
int check; // sygnał
int madeH2O,h,o; // ilość wody, ilość wodoru, ilość tlenu
unsigned int seed;

// Deklaracja i inicjalizacja mutexa
pthread_mutex_t mx = PTHREAD_MUTEX_INITIALIZER;

// Deklaracja zmiennych warunkowych
pthread_cond_t ph=PTHREAD_COND_INITIALIZER,// wracaj do pracy H
po=PTHREAD_COND_INITIALIZER, // wracaj do pracy O
c=PTHREAD_COND_INITIALIZER, // sprawdz
w=PTHREAD_COND_INITIALIZER;// czekaj az konsument sprawdzi

// PRODUCENT TLENU
void* produce_oxygen(void* arg){
  int errno;
  int thid = (size_t)arg;

  while(1){

    printf("Thread producer O nr %d is " GREEN "entering critical section" RESET "...\n", thid);

    sleep(rand_r(&seed)%4+5);// oczekiwanie na produkcje
      
    errno = pthread_mutex_lock(&mx);
    test_errno("pthread_mutex_lock_oxygen");
    printf("Thread producer O nr %d is " GREEN "enter"RESET" critical section" RESET "...\n", thid);

    if(check) pthread_cond_wait(&w,&mx);// poczekaj az konsument sprawdzi czy da sie zrobic wode
  
    printf("Thread producer O nr %d is " RED "in critical section" RESET "...\n", thid);
    for(int i=0;i<N;i++) if(bufO[i]==0){ bufO[i]=1; break;}
    o+=1;
    check=1;

    errno = pthread_cond_signal(&c);// wyslanie sygnalu do konsumenta
    test_errno("pthread_cond_signal");
    printf("Thread producer O nr %d is " YELLOW "waiting to produce"RESET"...\n", thid);

    errno = pthread_cond_wait(&po,&mx);// czekanie na sygnalu do pracy
    test_errno("pthread_cond_wait_oxy");

    printf("Thread producer O nr %d is "BLUE"leaving critical section"RESET"...\n", thid);
    errno = pthread_mutex_unlock(&mx);
    test_errno("pthread_mutex_unlock_oxygen");
  }
  return NULL;
}

// PRODUCENT WODORU
void* producent_hydrogen(void* arg){
  int errno;
  int thid = (size_t)arg;

  while(1){

    printf("Thread producer H nr %d is "GREEN"entering critical section"RESET"...\n" ,thid);
    sleep(rand_r(&seed)%4+6);// oczekiwanie na produkcje
      
    errno = pthread_mutex_lock(&mx);
    test_errno("pthread_mutex_lock_hydrogen");
    printf("Thread producer H nr %d is " GREEN "enter"RESET" critical section" RESET "...\n", thid);
    if(check) pthread_cond_wait(&w,&mx);// oczekiwanie na sygnal do pracy
      
    printf("Thread producer H nr %d is "RED"in critical section"RESET"...\n", thid);
    for(int i=0;i<N;i++) if(bufH[i]==0) {bufH[i]=1; break;}
    h++;
    check=1;

    errno = pthread_cond_signal(&c);// wyslanie sygnalu do konsumenta
    test_errno("pthread_cond_signal_hydro");
    
    printf("Thread producer H nr %d is "YELLOW"waiting to produce"RESET"...\n",thid);

    errno = pthread_cond_wait(&ph,&mx);// czekanie na sygnalu do pracy
    test_errno("pthread_cond_wait_hydro");

    printf("Thread producer H nr %d is "BLUE"leaving critical section"RESET"...\n", thid);
    errno = pthread_mutex_unlock(&mx);
    test_errno("pthread_mutex_unlock_hydrogen");

  }
  return NULL;
}

void* makeH2O(void* arg){
  int thid = *(int*)arg;
  check=0;
  int made=0;

  while(1){
    errno = pthread_mutex_lock(&mx);
    test_errno("pthread_mutex_lock_H20");

    printf("\nThread make H20 is "GREEN"entering critical section"RESET"...\n");

    if(!check){// czekanie na sygnalu do pracy
      printf("Thread make H20 is "YELLOW"waiting to check"RESET"...\n");
      errno = pthread_cond_wait(&c,&mx);
      test_errno("pthread_cond_wait-chek");
    }

    printf("Thread make H20 is "RED"in critical section"RESET"...\n");
    if(h>=2&&o>=1){        
      int hi=2,oi=1; 
      for  (int i=0;i<N/2;i++){
        if (bufO[i]==1 && oi>0){ bufO[i]=0; oi=oi-1; }
        if (bufH[i]==1 && hi>0){ bufH[i]=0; hi=hi-1; }
        if (hi==0 && oi==0) {
          madeH2O++;
          made=1;
          h-=2;
          o-=1;
          printf(GREEN"Thread %d made H20........... made H20: %d\n"RESET, thid,madeH2O);
          break;
        }
      }
    }
    // usleep(1200000);

    if (made){
      made=0;
      check=0;

      errno = pthread_cond_signal(&po);// wysanie sygnalu do pracy
      test_errno("pthread_cond_signal-o");
      errno = pthread_cond_signal(&ph);// wysanie sygnalu do pracy
      test_errno("pthread_cond_signal-h");
      errno = pthread_cond_signal(&ph);// wysanie sygnalu do pracy
      test_errno("pthread_cond_signal-h");          
    }
    else{// wypisywanie tablicy producentow
      printf("O ");
      for  (int i=0;i<N;i++)  printf("%d ",bufO[i]);
        printf("\n");
      
      printf("H ");
      for  (int i=0;i<N;i++)  printf("%d ",bufH[i]);
        printf("\n");
      
      check=0;
      errno = pthread_cond_signal(&w);// wysanie sygnalu do produkcji
      test_errno("pthread_cond_signal_w");
    }
    printf("Thread make H20 is "BLUE"leaving critical section"RESET"...\n");    
    pthread_mutex_unlock(&mx);
    test_errno("pthread_mutex_unlock");
  }   
  return NULL;
}


int main(void) {
  srand(time(0));

  pthread_t producersH[KH],producersO[KO],consumer;
  int con_id=KO+KH;

  /* Inicjaalizacja wątków */
  pthread_create(&consumer,NULL,&makeH2O,&con_id);

  for (int i =0;i<KH;i++){
    pthread_create(&producersH[i],NULL,&producent_hydrogen,(void *)(size_t)i);
  }

  for (int i =0;i<KO;i++){
    pthread_create(&producersO[i],NULL,&produce_oxygen,(void*)(size_t)i);
  }


  /* Czekaj na zakończenie wątków */
  for (int i =0;i<KH;i++){
    pthread_join(producersH[i],NULL);
  }
  for (int i =0;i<KO;i++){
    pthread_join(producersO[i],NULL);
  }
  pthread_join(consumer,NULL);

  return 0;
}