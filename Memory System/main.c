
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


void sim_init()
//init the simulation
{
	key_t msgkey;							//key for the queue
	time_t t;
	int i, j;

	srand((unsigned)time(&t));				//rand init

	//create the queue with open queue func
	msgkey = ftok(".", 'm');
	if ((queue_id = open_queue(msgkey)) == -1) {
		msgctl(queue_id, IPC_RMID, NULL);
		printf("Error in creating queue\n");
		exit(1);
	}

	// Create proccess
	for (i = 0; i < 3; i++)
	{
		if ((pid[i] = fork()) < 0)
		{
			printf("Error in fork!\n");
			for (j = i; j >= 0; j--) {
				kill(pid[j], SIGKILL);
			}
			exit(1);
		}

		//run the function to evert proccess created
		if (pid[i] == 0)
		{
			switch (i) {
			case 0:     //Process 1
				procFunc(i);
				break;
			case 1:     //Process 2
				procFunc(i);
				break;
			case 2:     //HD
				HDFunc();
				break;
			}
		}
	}

	//the father will continue from here and become MMU

	//init mutexes, conditions and threads. if error occurs, destory all previous inits
	if (pthread_mutex_init(&PageTable_mutex, NULL))			//page table mutex
	{
		printf("Error in init page mutex!\n");
		msgctl(queue_id, IPC_RMID, NULL);
		for (j = 0; j < 3; j++)
			kill(pid[j], SIGKILL);
		pthread_mutex_destroy(&PageTable_mutex);
		exit(1);
	}

	if (pthread_mutex_init(&PageCount_mutex, NULL))			//page counter mutex
	{
		printf("Error in init counter mutex!\n");
		msgctl(queue_id, IPC_RMID, NULL);
		for (j = 0; j < 3; j++)
			kill(pid[j], SIGKILL);
		pthread_mutex_destroy(&PageTable_mutex);
		pthread_mutex_destroy(&PageCount_mutex);
		exit(1);
	}

	if (pthread_mutex_init(&Condition_mutex, NULL))			//condition variable mutex
	{
		printf("Error in init condition mutex!\n");
		msgctl(queue_id, IPC_RMID, NULL);
		for (j = 0; j < 3; j++)
			kill(pid[j], SIGKILL);
		pthread_mutex_destroy(&PageTable_mutex);
		pthread_mutex_destroy(&PageCount_mutex);
		pthread_mutex_destroy(&Condition_mutex);
		exit(1);
	}

	if (pthread_cond_init(&Evicter_condition, NULL))		//condition variable init
	{
		printf("Error in init evicter condition!\n");
		msgctl(queue_id, IPC_RMID, NULL);
		for (j = 0; j < 3; j++)
			kill(pid[j], SIGKILL);
		pthread_mutex_destroy(&PageTable_mutex);
		pthread_mutex_destroy(&PageCount_mutex);
		pthread_mutex_destroy(&Condition_mutex);
		pthread_cond_destroy(&Evicter_condition);
		exit(1);
	}

	if ((pthread_create(&tid[0], NULL, MMU_main, NULL)))	//MMU main thread init
	{
		printf("Error in creating MMU main thread\n");
		msgctl(queue_id, IPC_RMID, NULL);
		for (j = 0; j < 3; j++)
			kill(pid[j], SIGKILL);
		pthread_mutex_destroy(&PageTable_mutex);
		pthread_mutex_destroy(&PageCount_mutex);
		pthread_mutex_destroy(&Condition_mutex);
		pthread_cond_destroy(&Evicter_condition);
		pthread_cancel(tid[0]);
		exit(1);
	}
	if ((pthread_create(&tid[1], NULL, MMU_evicter, NULL)))	//MMU evicter thread init
	{
		printf("Error in creating MMU evicter thread\n");
		msgctl(queue_id, IPC_RMID, NULL);
		for (j = 0; j < 3; j++)
			kill(pid[j], SIGKILL);
		pthread_mutex_destroy(&PageTable_mutex);
		pthread_mutex_destroy(&PageCount_mutex);
		pthread_mutex_destroy(&Condition_mutex);
		pthread_cond_destroy(&Evicter_condition);
		pthread_cancel(tid[0]);
		pthread_cancel(tid[1]);
		exit(1);
	}

	if ((pthread_create(&tid[2], NULL, MMU_printer, NULL)))	//MMU printer thread init
	{
		printf("Error in creating MMU printer thread\n");
		msgctl(queue_id, IPC_RMID, NULL);
		for (j = 0; j < 3; j++)
			kill(pid[j], SIGKILL);
		pthread_mutex_destroy(&PageTable_mutex);
		pthread_mutex_destroy(&PageCount_mutex);
		pthread_mutex_destroy(&Condition_mutex);
		pthread_cond_destroy(&Evicter_condition);
		pthread_cancel(tid[0]);
		pthread_cancel(tid[1]);
		pthread_cancel(tid[2]);
		exit(1);
	}
}


