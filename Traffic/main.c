#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>

//~~~~~~~~~~~~~~~~~~ defines init~~~~~~~~~~~~~~~~~~~
#define N 5
#define FIN_PROB 0.1
#define MIN_INTER_ARRIVAL_IN_NS 8000000
#define MAX_INTER_ARRIVAL_IN_NS 9000000
#define INTER_MOVES_IN_NS 100000
#define SIM_TIME 2
#define MAX_CARS  (int)(((SIM_TIME*1000000000)/(MIN_INTER_ARRIVAL_IN_NS))+10) //+10 for saftey
#define lEN (4 * N - 4)
//~~~~~~~~~~~~~~~~~~ prototype init~~~~~~~~~~~~~~~~~~~
void* Step(void *arg);
void* Print(void *arg);
void* Generate(void *arg);
void Print_in_road();

//~~~~~~~~~~~~~~~~~~ mutex init~~~~~~~~~~~~~~~~~~~
pthread_mutexattr_t attr;
pthread_mutex_t cell_mutex[(4 * N) - 4] = { 0 };

//~~~~~~~~~~~~~~~~~~ pthreads init~~~~~~~~~~~~~~~~~~~
pthread_t print_func;
pthread_t generator_func[4];
pthread_t cars[4][(int)MAX_CARS] = { 0 };

//~~~~~~~~~~~~~~~~~~ global variebles init~~~~~~~~~~~~~~~~~~~
char Print_road[(4 * N) - 4] = { " " };
int argg_index[4] = { 0 };
int generator_flag[4] = {0,0,0,0};
struct timespec start_time;

int main() 
{
    
    srand(time(NULL));   //init for the rand function
    int i;

 //~~~~~~~~~~~~~~~~~~ mutex init~~~~~~~~~~~~~~~~~~~
    
    if (pthread_mutexattr_init(&attr) != 0)                             //init for the attribute 
    {                          
        printf("Error in initializing mutex");
        exit(-1);
    }
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);         //set the mutex as error check type
    for(i=0;i<4*N-4;i++)                                                //init all the cars mutex
    {                                              
        if (pthread_mutex_init(&cell_mutex[i], &attr) != 0) 
        {
            printf("Error in initializing mutex\n");
            exit(-1);
        }
    }

    clock_gettime(CLOCK_REALTIME,&start_time);                          //init the clock
    for (i = 0; i < 4; i++)                                             //create 4 generators threads
    {                                                
        argg_index[i] = i;
        if (pthread_create(&generator_func[i], NULL, Generate, (void*)&argg_index[i]) != 0) 
        {
            printf("Generator func initializing failed\n");
            exit(-1);
        }
    }
    if (pthread_create(&print_func,NULL,Print,NULL) !=0)                //create the print func thread 
    {              
        printf("Print func initializing failed\n");
        exit(-1);
    }

    pthread_join(print_func,NULL);                                      //finish the print func
    for(i=0;i<4;i++)                                                    //finish the generator funcs
        pthread_join(generator_func[i],NULL);
    for(i=0;i<4*N-4;i++)                                                //delete the mutexes
        pthread_mutex_destroy(&cell_mutex[i]);
    return 0;
}