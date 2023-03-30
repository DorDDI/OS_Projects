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
