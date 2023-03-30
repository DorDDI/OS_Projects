
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


void procFunc(int proc_index)
//func for the two proccess
{
	time_t t;
	srand((unsigned)time(&t));				//init the rand func
	message send;							//struct for the send message
	message receive;						//struct for the receive message

	send.mtype = PROCCESS_REQ;				//type is proccess request
	send.ProcNum = proc_index + 1;			//the proccess is proc_index (0/1) +1 for proc 1/2

	while (simflag)
	{
		usleep(INTER_MEM_ACCS_T / (double)1000);
		if (rand() % 100 <= WR_RATE * 100)	//calculate if the request is RD/WR
			send.reqType = WR_REQ;
		else
			send.reqType = RD_REQ;

		if (simflag == 0)
			break;

		sprintf(send.mtext, "Process number %d request RD/WR 0/1 - %d\n", proc_index + 1, send.reqType); //save the message
		send_message(queue_id, &send);		//send the message
		read_message(queue_id, &receive, MMU_ACK_PROC1 + proc_index); //read the message that equal to the selected type

	}
}


void HDFunc()
{
	message send;							//struct for the send message
	message receive;						//struct for the receive message
	send.mtype = HD_ACK;				    //type is HD acknowledge
	send.ProcNum = HD_ACK;
	strcpy(send.mtext, "HD acknowledge");	//save the message 

	while (simflag) {
		read_message(queue_id, &receive, HD_REQ);	//read the message that equal to the selected type
		usleep(HD_ACCS_T / (double)1000);			//sleep
		send_message(queue_id, &send);				//send the message
	}
}


int open_queue(key_t keyval)
//create the queue
{
	int qid;
	if ((qid = msgget(keyval, IPC_CREAT | 0660)) == -1)
	{
		return(-1);
	}
	return(qid);
}


void send_message(int qid_f, struct message* qbuf)
//send a message to the queue
{
	int result, length;
	length = sizeof(struct message) - sizeof(long);
	if ((result = msgsnd(qid_f, qbuf, length, 0)) == -1)
	{
		printf("Error in sending message\n");
		quit_simulation(1);
	}
}


void read_message(int qid_f, struct message* qbuf, long type)
//recieve a message from the queue
{
	int     result, length;
	length = sizeof(struct message) - sizeof(long);
	if ((result = msgrcv(qid_f, qbuf, length, type, 0)) == -1)
	{
		printf("Error in receiving message\n");
		quit_simulation(1);
	}
}


void* MMU_main()
{
	int Miss_Hit;					//variable that will save if it was hit r miss
	message send;					//struct for the send message
	message recieve_proccess;		//struct for the recieive message from proccess
	message recieve_HD;				//struct for the recieive message from HD

	while (simflag)
	{
		//~~~~calculate probability~~~~
		read_message(queue_id, &recieve_proccess, PROCCESS_REQ); //read the message that equal to the selected type
		if (page_count == 0)				//if we dont have pages  
			Miss_Hit = MISS;				//sure will be miss
		else
		{
			int prob = rand() % 100;		//random a probability for miss/hit
			if (prob < HIT_RATE * 100)		//check and save if the prob is miss/hit
				Miss_Hit = HIT;
			else
				Miss_Hit = MISS;
		}

		if (Miss_Hit == MISS)				//prob = miss
		{
			if (page_count == N)			//if the queue is full, run the evicter
				call_evicter();
			//continue after finish the evicter func
	
			if (simflag == 0)
				break;
		//~~~~send to HD req~~~~

			send.mtype = HD_REQ;			//type is HD request
			send.ProcNum = HD_REQ;
			strcpy(send.mtext, "Message is a HD_REQ");		//save the message
			send_message(queue_id, &send);	//send the message
			read_message(queue_id, &recieve_HD, HD_ACK); //read the message that equal to the selected type

			if (simflag == 0)
				break;

			pages_lock();
			page_table[(Queue_index + page_count) % N] = VALID; //change the page table safely
			pages_unlock();

			if (simflag == 0)
				break;

			count_lock();
			page_count += 1;				//increase the counter safely
			count_unlock();

			if ((page_count > N) || (page_count < 0))  //error in the counter range - quit
				quit_simulation(1);
		}

		else
		{
			if (Miss_Hit == HIT)
			{
			}								//do nothing - immediately action
			else
			{
				printf("Error, invalid action for MMU_main\n");   //not hit ro miss - error
				quit_simulation(1);
			}
		}

		if (simflag == 0)
			break;
		//~~~~send acknowledge to the proccesses~~~~

		if (recieve_proccess.ProcNum == 1)			//send to proccess 1
			send.mtype = MMU_ACK_PROC1;
		else		                              	//send to proccess 2
			send.mtype = MMU_ACK_PROC2;
		send.ProcNum = send.mtype;
		strcpy(send.mtext, "MMU acknowledge");		//save the message

		//~~~~handle the RD/WR request~~~~

		if (recieve_proccess.reqType == WR_REQ)		//Write
		{
			if (simflag == 0)
				break;
			usleep(MEM_WR_T / (double)1000);
			pages_lock();
			page_table[(Queue_index + (rand() % page_count)) % N] = DIRTY; //change the page table saftly
			pages_unlock();
			send_message(queue_id, &send);			//send the message
		}
		else
		{
			if (simflag == 0)
				break;
			if (recieve_proccess.reqType == RD_REQ)	//Read
				send_message(queue_id, &send);		//send the message
			else                                    //not RD/WR - error
			{
				printf("Error invalid RD/WR request\n");
				quit_simulation(1);
			}
		}
	}
}


void* MMU_printer()
//func for the MMU main printer
{
	message send;							//struct for the send message
	int copy[N] = { 0 };					//array of the copied page table
	int temp[N] = { 0 };					//array of the prev copied page table
	int i;
	int flag;								//flag if there is a problem

	while (simflag) {
		flag = 0;
		pages_lock();
		memcpy(copy, page_table, N * sizeof(int));		//copy the page table saftly
		pages_unlock();

		for (i = 0; i < N; i++)							//check if the sim is stuck
		{
			if (copy[i] != temp[i])
				flag = 1;
		}
		//print the copied page table:
		//INVALID -> "-", VALID -> "0", DIRTY -> "1"
		if (simflag == 0)
			break;
		if (flag == 0)									//if the sim is stuck, refresh it
		{
			if (pthread_mutex_lock(&Condition_mutex))		//safely lock condition mutex
			{
				printf("Error in locking condition variable mutex\n");
				quit_simulation(1);
			}

			if (pthread_cond_signal(&Evicter_condition))	//signal with the condition variable
			{
				printf("Error in signaling mmu\n");
				quit_simulation(1);
			}

			if (pthread_mutex_unlock(&Condition_mutex))		//safely unlock condition mutex
			{
				printf("Error in unlocking condition variable mutex\n");
				quit_simulation(1);
			}

		}
		for (i = 0; i < N; i++) {
			printf("%d|", i);
			if (copy[i] == INVALID) {
				printf("-\n");
			}
			else if (copy[i] == VALID) {
				printf("0\n");
			}
			else if (copy[i] == DIRTY) {
				printf("1\n");
			}
			else {
				printf("Incorrect page value\n");
			}
		}
		printf("\n\n");
		memcpy(temp, copy, N * sizeof(int));			//save the copied array	
		usleep(TIME_BETWEEN_SNAPSHOTS / (double)1000);
	}
}


void* MMU_evicter()
//func for the MMU main evicter
{
	
	message send;					//struct for the send message
	message recieve;				//struct for the recieve message

	send.mtype = HD_REQ;			//type is HD request
	send.ProcNum = HD_REQ;			//type is HD request
	strcpy(send.mtext, "Evicter access");  	//save the message

	while (simflag)
	{
		if (pthread_mutex_lock(&Condition_mutex))					//safely lock condition mutex
		{
			printf("Error in locking condition variable mutex\n");
			quit_simulation(1);
		}

		if (pthread_cond_wait(&Evicter_condition, &Condition_mutex)) //wait for a signal from the condition
		{
			printf("Error in waiting for evicter\n");
			quit_simulation(1);
		}

		if (pthread_mutex_unlock(&Condition_mutex))					//safely unlock condition mutex
		{
			printf("Error in unlocking condition variable mutex\n");
			quit_simulation(1);
		}

		if (simflag == 0)
			break;
		while (page_count >= USED_SLOTS_TH)
		{
			if (simflag == 0)
				break;
			pages_lock();
			if (page_table[Queue_index] == DIRTY)			//if dirty, write to HD
			{
				send_message(queue_id, &send);				//send message
				read_message(queue_id, &recieve, HD_ACK);   //read the message that equal to the selected type
			}

			page_table[Queue_index] = INVALID;				//clear the page
			Queue_index = (Queue_index + 1) % N;			//increase the queue pointer
			pages_unlock();

			if (simflag == 0)
				break;

			count_lock();
			page_count--;									//decrease the counter safetly
			count_unlock();

			if ((page_count > N) || (page_count < 0))		//error in the counter range - quit
				quit_simulation(1);
			
		}

		//signal to the MMU that the evicting is finished (return to wake evicter)
		if (pthread_mutex_lock(&Condition_mutex))		//safely lock condition mutex
		{
			printf("Error in locking condition variable mutex\n");
			quit_simulation(1);
		}

		if (pthread_cond_signal(&Evicter_condition))	//signal with the condition variable
		{
			printf("Error in signaling mmu\n");
			quit_simulation(1);
		}

		if (pthread_mutex_unlock(&Condition_mutex))		//safely unlock condition mutex
		{
			printf("Error in unlocking condition variable mutex\n");
			quit_simulation(1);
		}

	}
}


void pages_lock()
//lock the page table change 
{
	if (pthread_mutex_lock(&PageTable_mutex))		//safely lock page table mutex
	{
		printf("Error in locking mutex\n");
		quit_simulation(1);
	}
}


void pages_unlock()
//unlock the page table change 
{
	if (pthread_mutex_unlock(&PageTable_mutex))		//safely unlock page table mutex
	{
		printf("Error in unlocking page tanle mutex\n");
		quit_simulation(1);
	}
}


void count_lock()
//lock the counter change
{
	if (pthread_mutex_lock(&PageCount_mutex))		//safely lock page counter mutex
	{
		printf("Error in locking page table mutex\n");
		quit_simulation(1);
	}
}


void count_unlock()
//unlock the counter change
{
	if (pthread_mutex_unlock(&PageCount_mutex))		//safely unlock page counter mutex
	{
		printf("Error in unlocking page count mutex\n");
		quit_simulation(1);
	}
}


void call_evicter()
//func to handle the evicter condition
{
	usleep(1000 / (double)1000);

	if (pthread_mutex_lock(&Condition_mutex))					//safely lock condition mutex
	{
		printf("Error in locking page count mutex\n");
		quit_simulation(1);
	}

	if (pthread_cond_signal(&Evicter_condition))				//signal with the condition variable
	{
		printf("Error in signaling evicter\n");
		quit_simulation(1);
	}

	if (pthread_mutex_unlock(&Condition_mutex))					//safely unlock condition mutex
	{
		printf("Error in unlocking mutex\n");
		quit_simulation(1);
	}

	if (pthread_mutex_lock(&Condition_mutex))					//safely lock condition mutex
	{
		printf("Error in locking mutex\n");
		quit_simulation(1);
	}

	//wait until the evicter func is finished and the MMU main can continue

	if (pthread_cond_wait(&Evicter_condition, &Condition_mutex))  //wait for a signal from the condition
	{
		printf("Error in waiting for evicter\n");
		quit_simulation(1);
	}

	if (pthread_mutex_unlock(&Condition_mutex))					//safely unlock condition mutex
	{
		printf("Error in unlocking condition variable mutex\n");
		quit_simulation(1);
	}
}


void quit_simulation(int num)
//finish simulation and destroy all proccesses, threads and mutex
{
	int i;
	simflag = 0;
	usleep(1000000 / (double)1000);

	if (pthread_cond_signal(&Evicter_condition))	//signal with the condition variable
	{
		printf("Error in signaling mmu\n");
		quit_simulation(1);
	}

	if (pthread_cond_signal(&Evicter_condition))	//signal with the condition variable
	{
		printf("Error in signaling mmu\n");
		quit_simulation(1);
	}


	for (i = 0; i < 3; i++)								//destroy all the threads
	{
		if (pthread_join(tid[i],NULL))
			printf("Error in destroying threads!\n");
	}

	for (i = 0; i < 3; i++)						    	//destroy all the proccesses
		kill(pid[i], SIGKILL);

	msgctl(queue_id, IPC_RMID, NULL);					//destroy the queue


	if (pthread_mutex_destroy(&PageTable_mutex))		//destroy the page table mutex
	{
		printf("Error in destroying page table mutex\n");
		num = 1;
	}

	if (pthread_mutex_destroy(&PageCount_mutex))		//destroy the page counter mutex
	{
		printf("Error in destroying page count mutex\n");
		num = 1;
	}


	if (pthread_mutex_destroy(&Condition_mutex))		//destroy the condition variable mutex
	{
		printf("Error in destroying cv mutex\n");
		num = 1;
	}


	if (pthread_cond_destroy(&Evicter_condition))		//destroy the condition variable
	{
		printf("Error in destroying cv evicter\n");
		num = 1;
	}

	if (num == 1)
		exit(1);										//exit with 1 if there was an error in the sim
	else
		printf("Successfully Finish Simulation\n");
}
