
//~~~~~~~~~~~~~~~~~~ include init~~~~~~~~~~~~~~~~~~~
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <wait.h>
#include <string.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>


//~~~~~~~~~~~~~~~~~~ defines init~~~~~~~~~~~~~~~~~~~
// Waiting time
#define INTER_MEM_ACCS_T 		10000	//in ns
#define MEM_WR_T 				1000			//in ns
#define TIME_BETWEEN_SNAPSHOTS 	100000000 	//in ns
#define HD_ACCS_T 				100000	    //in ns
#define SIM_T 					1			//in seconds

// Probabilities
#define WR_RATE 				0.5			//Probabilities 50%
#define HIT_RATE 				0.5			//Probabilities 50%

// Memory Sizes
#define USED_SLOTS_TH			3			//number of used slots
#define N 						5			//max number of pages

// MMU defines
#define INVALID					0
#define VALID					1
#define DIRTY					2
#define MISS					0
#define HIT						1

// Messege
#define MSG_BUFFER_SIZE			128			//size of the message Buffer
#define PROCCESS_REQ 			1			//request from proccess
#define HD_REQ 					2			//request from HD
#define HD_ACK 					3			//acknowledge for HD
#define MMU_ACK_PROC1			4			//acknowledge for Process 1
#define MMU_ACK_PROC2			5			//acknowledge for Process 2
#define RD_REQ					0			//request for read
#define WR_REQ					1			//request fore write


//~~~~~~~~~~~~~~~~~~ Globals Variables~~~~~~~~~~~~~~~~~~~
int queue_id;						//Queue ID
pid_t pid[3];						//Process IDs
pthread_t tid[3];					//Thread IDs
int page_table[N] = { 0 };			//Page table array
int page_count = 0;					//Page counter
int Queue_index = 0;				//the correct index in the clock algorithm
int simflag = 1;


//~~~~~~~~~~~~~~~~~~ Mutex~~~~~~~~~~~~~~~~~~~
pthread_mutex_t PageTable_mutex;    //mutex to change the page table
pthread_mutex_t PageCount_mutex;	//mutex to change the page counteer
pthread_mutex_t Condition_mutex;    //mutex for the condition variable
pthread_cond_t Evicter_condition;   //condition variable for evicter


//~~~~~~~~~~~~~~~~~~ Struct message~~~~~~~~~~~~~~~~~~~
typedef struct message {			//struct of the send message
	long mtype;						//type of the message - req, ack and to who
	int reqType;					//type of the proccess to MMU - WR/RD
	int ProcNum;					//who is send the message
	char mtext[MSG_BUFFER_SIZE];	//the text of the message
} message;


//~~~~~~~~~~~~~~~~~~ prototype init~~~~~~~~~~~~~~~~~~~
void sim_init();					//init the simulation
int open_queue(key_t);				//create the queue
void procFunc(int i);				//func for the two proccess
void HDFunc();						//func for the HD proccess
void* MMU_main();					//func for the MMU main thread
void* MMU_printer();				//func for the MMU main printer
void* MMU_evicter();				//func for the MMU main evicter
void pages_lock();					//lock the page table change
void pages_unlock();				//unlock the page table change
void count_lock();					//lock the counter change
void count_unlock();				//unlock the counter change
void call_evicter();				//func to handle the evicter condition
void quit_simulation(int);			//finish simulation and destroy all proccesses, threads and mutex
void send_message(int, struct message*);		//send a message to the queue
void read_message(int, struct message*, long);	//recieve a message from the queue


//~~~~~~~~~~~~~~~~~~ main pogram~~~~~~~~~~~~~~~~~~~

int main()
//main program
{
	sim_init();						//start the init func of the simuation
	sleep(SIM_T);					//sleep until the sim is over
	simflag = 0;
	quit_simulation(0);				//quit the simulation with no errors encounter
	return 0;
}


