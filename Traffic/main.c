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
