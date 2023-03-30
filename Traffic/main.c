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

void* Step(void *arg) 
//function for move a car to the next place
{                                                 
    int first_time = 1;                                                 //first time enter the road
    struct timespec last_move;
    struct timespec curr_move;
    double time_in_ns = 0;
    int move_permission = 0;
    int generator_index=(int)(*(int *) arg);
    int enter_location = generator_index * (N - 1);
    int err=0;

    while (first_time && !(generator_flag[generator_index]))            //create a new car
    { 
        pthread_mutex_lock(&cell_mutex[(enter_location - 1+ lEN) % lEN]);  //lock the position before the car generaor
        err=pthread_mutex_trylock(&cell_mutex[enter_location]);         //try to lock the car position

        if (err == 0)                                                   //if succeeded, change the road array
        {
            clock_gettime(CLOCK_REALTIME, &curr_move);
            Print_road[enter_location] = '*';
            pthread_mutex_unlock(&cell_mutex[(enter_location - 1 + lEN) % lEN]);
            first_time = 0;
        }
        else
            pthread_mutex_unlock(&cell_mutex[(enter_location - 1 + lEN) % lEN]); //unlock and continue
    }

    while (!(generator_flag[generator_index]))                          
    {                        
        while (!move_permission)                                        //move the car to the next place
        {
            clock_gettime(CLOCK_REALTIME, &last_move);
            time_in_ns = (double)(last_move.tv_sec - curr_move.tv_sec) * 1000000000 + (double)(last_move.tv_nsec - curr_move.tv_nsec);
            if (time_in_ns >= INTER_MOVES_IN_NS)                        //the interval is correct
                move_permission = 1;
        }
        clock_gettime(CLOCK_REALTIME, &curr_move);                      
        enter_location = enter_location + 1;
        move_permission = 0;
        
        pthread_mutex_lock(&cell_mutex[(enter_location) % lEN] );           //change position if can
        Print_road[(enter_location -1)%(4*N-4)] = ' ';
        Print_road[enter_location %(4*N-4)] = '*';
        pthread_mutex_unlock(&cell_mutex[(enter_location -1)%lEN] );
        if(enter_location %(N-1)==0)                                        // if getting into the sink, check probability
        {                                       
            if((rand() % 100) <= 100*FIN_PROB)                              //calculate the probability of sinking
            {
                Print_road[enter_location %(4*N-4)] = ' ';                  //we sink
                pthread_mutex_unlock(&cell_mutex[(enter_location) % lEN]);
                return NULL;
            }
        }

    }
    if (!first_time)                                                       //open the mutex in the end
        pthread_mutex_unlock(&cell_mutex[(enter_location) % lEN]);
    return NULL;
}

void* Generate(void* arg) {
    //function for the generation proccess
    int i;
    int generate_flag = 0;                                                  //if we can generate after the interval                                     
    int end_of_simulation = 0;
    struct timespec generate_time;
    struct timespec car_interval;
    int index = (int)(*(int*)arg);                                        //the index of the generator
    int index_of_cars = 0;                                                  //index for the numbers of cars
    double time_in_ns;
    double car_generate_interval;


    while (!end_of_simulation)                                              //while the simulation isnt over
    {
        clock_gettime(CLOCK_REALTIME, &generate_time);
        time_in_ns = (double)(generate_time.tv_sec - start_time.tv_sec) * 1000000000 + (double)(generate_time.tv_nsec - start_time.tv_nsec);
        if (time_in_ns < SIM_TIME * 1000000000)                             //check the time of the simulation
        {
            clock_gettime(CLOCK_REALTIME, &car_interval);
            car_generate_interval = (rand() % (MAX_INTER_ARRIVAL_IN_NS + 1 - MIN_INTER_ARRIVAL_IN_NS)) + MIN_INTER_ARRIVAL_IN_NS; //randomize a number
            while (!generate_flag)
            {
                clock_gettime(CLOCK_REALTIME, &generate_time);
                time_in_ns = (double)(generate_time.tv_sec - car_interval.tv_sec) * 1000000000 + (double)(generate_time.tv_nsec - car_interval.tv_nsec);
                if (time_in_ns >= car_generate_interval)                    //check if the interval of generation is correct
                    generate_flag = 1;
            }

            if (pthread_create(&cars[index][index_of_cars], NULL, Step, ((void*)&index)) != 0) //create a new car thread
            {
                printf("CAR INIT FAILD\n");
                exit(-1);
            }
            index_of_cars++;                                                //increase the number of cars
            generate_flag = 0;                                              //change the flag and wait again for the interval
        }
        else                                                                //the simulation is over
            end_of_simulation = 1;
    }

    generator_flag[index] = 1;                                              //mark the generator function to end the simulaton
    for (i = 0; i < index_of_cars; i++)
        pthread_join(cars[index][i], NULL);                                 //close all the cars threads of all the generators
    return NULL;
}


void* Print(void* arg) 
//function for the array print
{
    int i;
    struct timespec print_time;
    struct timespec last_print_time;
    double time_in_ns = 0;
    int end_of_simulation = 0;
    int number_of_prints = 0;
    clock_gettime(CLOCK_REALTIME, &last_print_time);
    while (!end_of_simulation)                                               //while the simulation isnt over
    {                                           
        clock_gettime(CLOCK_REALTIME, &print_time);
        time_in_ns = (double)(print_time.tv_sec - last_print_time.tv_sec) * 1000000000 + (double)(print_time.tv_nsec - last_print_time.tv_nsec);
        if (time_in_ns >= (SIM_TIME * 1000000000 / 10)) 
        {
            clock_gettime(CLOCK_REALTIME, &last_print_time);
            Print_in_road();                                                //print the array
            number_of_prints++;                                             //count the number of prints
            if (number_of_prints == 10)                                     //print only 10 times                             
                end_of_simulation = 1;
        }
    }
    return NULL;
}

void Print_in_road()
//function for the print function
{
    int i;
    char middle[N - 1];                                                     //line with @ in the middle
    for (i = 0; i < N - 2; i++)                                             //generate the @ in the line
        middle[i] = '@';
    middle[N - 2] = '\0';                                                   //end with \0 in the end
    printf("\n");
    for (i = N - 1; i >= 0; i--)                                            //prinit the first line
        printf("%c", Print_road[i]);
    printf("\n");
    for (i = 0; i < N - 2; i++)                                             //print the middle line with the cars positions
    {
        printf("%c", Print_road[i + N]);
        printf("%s", middle);
        printf("%c\n", Print_road[4 * N - 5 - i]);
    }
    for (i = 2 * N - 2; i < 3 * N - 2; i++)                                 //print the last line
        printf("%c", Print_road[i]);
    printf("\n");
}