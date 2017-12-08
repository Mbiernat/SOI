#include <stdio.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#if defined(__GNU_LIBRARY__) && !defined(_SEM_SEMUN_UNDEFINED)
/* jest zdefiniowane w sys/sem.h */
#else
union semun
{
    int val;            	//value for SETVAL
    struct semid_ds *buf;   	//buffer for IPC_STAT, IPC_SET
    unsigned short int *array;  //array for GETALL, SETALL
    struct seminfo *__buf;  	//buffer for IPC_INFO
};
#endif

typedef struct product
{
	int  val;
	char letterProduced;
	bool isReadConsumerA;
	bool isReadConsumerB;	

} pies;

// DEFINED CONSTS
#define MAX_PRODUCTS   10
#define MAX_SEMS 	4
#define KILL_PRODUCERS		MAX_PRODUCTS + 2
#define KILL_CONSUMERS		MAX_PRODUCTS + 3
#define DISABLE_CONSUMERS	MAX_PRODUCTS + 4
#define DISABLE_PRODUCERS	MAX_PRODUCTS + 5

#define SETUP		0
#define CONSUMER_A	1
#define PRODUCER_A	2
#define CONSUMER_B	3
#define PRODUCER_B	4

// Semaphore numbers
#define CONSUM_CHECK	0
#define ADD_OR_DELETE	1
//#define ADD		2
#define BLOCK_ADDING	2
#define BLOCK_DELETING	3

#define MEMORY_KEY	6578


// FUNCTIONS
void Init_semaphores(int sem_id, unsigned short int *semvals);
void Init_buffer(struct product *prod_buff);

void LockSemaphore(int sem_id, int sem_number);
void UnlockSemaphore(int sem_id, int sem_number);
void adjustSem_Deleting(int sem_id);
void adjustSem_Adding(int sem_id, int Producer_ID);
int getBufferLength(int sem_id);

void SetupProcess(int sem_id, int memory_id);
void ConsumerProcess(int Consumer_ID, int sem_id, int memory_id);
void ProducerProcess(int Producer_ID, int sem_id, int memory_id);

void ReadProduct();
void CheckProduct();
void ConsumeProduct();
void ProduceProduct(int Producer_ID);

struct Product *tail;


int main(int argc, char * argv[])
{
	key_t sem_key;     //sem_key to make sem_id
	int sem_id;    

// Creating semaphore
    	if ((sem_key = ftok(".", 'A')) == -1)
	{
	    fprintf(stderr, "Error creating sem_key.\n");
	    exit(1);
	}  
 
	if ((sem_id = semget(sem_key, MAX_SEMS, IPC_CREAT | 0600)) == -1)
	{
	    fprintf(stderr, "Error creating sem_id.\n");
	    exit(1);
	}

// Creating memory 
	int memory_id;

	if ((memory_id = shmget(MEMORY_KEY, sizeof(struct product)*MAX_PRODUCTS, 0666 | IPC_CREAT)) == -1)
	{
	    fprintf(stderr, "Error creating shared memory.\n");
	    exit(1);
	}


	
	

	switch(atoi(argv[1]))
	{
		case SETUP:
			SetupProcess(sem_id, memory_id);
			break;
		case CONSUMER_A:
			ConsumerProcess(CONSUMER_A, sem_id, memory_id);
			break;
		case PRODUCER_A:
			ProducerProcess(PRODUCER_A, sem_id, memory_id);
			break;
		case CONSUMER_B:
		//	ConsumerProcess(CONSUMER_B, sem_id);
			break;
		case PRODUCER_B:
		//	ProducerProcess(PRODUCER_B, sem_id);
			break;
		default:
			printf("No process launched!\n");
			break;
	}
	return 0;
}

/*============================================================================*
 *									      *
 *============================================================================*/
void SetupProcess(int sem_id, int memory_id)
{
	printf("================================================\n");
	printf("Setup Process: 	Semaphore group ID == %d\n\n", sem_id);

	//=====================================================
	// Initializing shared memory
	struct product *prod_buff;
	prod_buff = (struct product*)shmat(memory_id, NULL, 0);
	if (prod_buff == NULL)
	{
		perror("Przylaczenie segmentu pamieci wspoldzielonej");
		exit(1);
	}
	printf("MEMORY     INITIALIZED.\n");

	//=====================================================
	// Initializing semaphores
	unsigned short int semvals[MAX_SEMS];

	semvals[CONSUM_CHECK] 	= 1;
	semvals[ADD_OR_DELETE]  = 1;
	semvals[BLOCK_ADDING]   = MAX_PRODUCTS - 3 - 2;
	semvals[BLOCK_DELETING] = 2;  // 0;

	Init_semaphores(sem_id, semvals);
	printf("SEMAPHORES INITIALIZED.\n");
	
	//=====================================================
	// Initializing buffer
	Init_buffer(prod_buff);
	printf("BUFFER     INITIALIZED.\n\n");



	//=====================================================
	// MAIN LOOP
	printf("=========== WHILE ===========\n");
	printf("Press [ ] and [enter]:\n");
	printf("-- [q] to exit.\n");
	printf("-- [a] to add element.\n");
	printf("-- [d] to delete.\n");

	char c;
	int buff_len;

	while(1)
	{
		c = getchar();
		if(c == 'q')
			break;
		else if(c == 'a')
		{
			adjustSem_Adding(sem_id, PRODUCER_A);
			buff_len = getBufferLength(sem_id);
			printf("Element added.\n");
			printf("NUMBER OF ELEMENTS: %d\n\n", buff_len);
		}
		else if(c == 'd')
		{
			adjustSem_Deleting(sem_id);
			buff_len = getBufferLength(sem_id);
			printf("Element deleted.\n");
			printf("NUMBER OF ELEMENTS: %d\n\n", buff_len);
		}
		else if(c == 'k')
		{
			prod_buff[KILL_CONSUMERS].val = 1;
			prod_buff[KILL_PRODUCERS].val = 1;
			printf("\nKILLED PRODUCER / CONSUMER PROCCESSES.\n");
		}
	}
	printf("\n====== EXITING PROCESS ======\n");

	//=====================================================
	// Closing shared memory
	shmctl(memory_id, IPC_RMID, 0);
	printf("MEMORY     CLOSED.\n");

	//=====================================================
	// Closing semaphores
	semctl(sem_id, 0, IPC_RMID);
	semctl(sem_id, 1, IPC_RMID);
	semctl(sem_id, 2, IPC_RMID);
	semctl(sem_id, 3, IPC_RMID);
	printf("SEMAPHORES CLOSED.\n");
	printf("================================================\n");
}

/*============================================================================*/
// Consumer related functions:

void ConsumerProcess(int Consumer_ID, int sem_id, int memory_id)
{
	printf("================================================\n");
	printf("Consumer Process:  Semaphore group ID == %d\n", sem_id);


	int buf2[2];
	struct product *prod_buff;
	prod_buff = (struct product*)shmat(memory_id, NULL, 0);
	if (prod_buff == NULL)
	{
		perror("Przylaczenie segmentu pamieci wspoldzielonej");
		exit(1);
	}


	//=====================================================
	// MAIN LOOP

	int buff_len;

	while(1)
	{
		if(prod_buff[KILL_CONSUMERS].val == 1)
		{
			perror("PROCCES KILLED ");
			exit(1);
		}
		if(prod_buff[DISABLE_CONSUMERS].val == 1)
		{
			printf("\nPROCCES DISABLED.\n");
			while(prod_buff[DISABLE_CONSUMERS].val == 1);
			printf("\nPROCCES ANABLED.\n");
		}



	//	printf("Press [q] and [enter] to exit.\n");
	//	if(getchar() == 'q')
	//		break;


		buff_len = getBufferLength(sem_id);
		printf("\nNUMBER OF ELEMENTS: %d\n", buff_len);
		printf("Consuming element...\n");
		adjustSem_Deleting(sem_id);

		LockSemaphore(sem_id, ADD_OR_DELETE);
		//====================================

		ConsumeProduct();

		//====================================
		UnlockSemaphore(sem_id, ADD_OR_DELETE);
		
		printf("Element consumed\n");
		buff_len = getBufferLength(sem_id);
		printf("NUMBER OF ELEMENTS: %d\n\n", buff_len);

		sleep(2);


	}
}

void ReadProduct()
{
	
}

void CheckProduct()
{

}

void ConsumeProduct()
{

}


/*============================================================================*/
// Produces related functions:

void ProducerProcess(int Producer_ID, int sem_id, int memory_id)
{
	printf("================================================\n");
	printf("Producer Process:  Semaphore group ID == %d\n", sem_id);


	struct product *prod_buff;
	prod_buff = (struct product*)shmat(memory_id, NULL, 0);
	if (prod_buff == NULL)
	{
		perror("Przylaczenie segmentu pamieci wspoldzielonej");
		exit(1);
	}

	//=====================================================
	// MAIN LOOP

	int buff_len;

	while(1)
	{
		if(prod_buff[KILL_PRODUCERS].val == 1)
		{
			perror("PROCCES KILLED ");
			exit(1);
		}
		if(prod_buff[DISABLE_PRODUCERS].val == 1)
		{
			printf("\nPROCCES DISABLED.\n");
			while(prod_buff[DISABLE_CONSUMERS].val == 1);
			printf("\nPROCCES ANABLED.\n");
		}
		

		buff_len = getBufferLength(sem_id);
		printf("\nNUMBER OF ELEMENTS: %d\n", buff_len);
		printf("PRODUCING ELEMENT...\n");
		adjustSem_Adding(sem_id, PRODUCER_A);

		LockSemaphore(sem_id, ADD_OR_DELETE);
		//====================================

		ProduceProduct(PRODUCER_A);

		//====================================
		UnlockSemaphore(sem_id, ADD_OR_DELETE);
		
		printf("ELEMENT PRODUCED\n");
		buff_len = getBufferLength(sem_id);
		printf("NUMBER OF ELEMENTS: %d\n\n", buff_len);
		
	//	if(getchar() == 'q')
	//		break;

		sleep(3);
	}
}

void ProduceProduct(int Producer_ID)
{

}

/*============================================================================*/

void LockSemaphore(int sem_id, int sem_number)
{
	struct sembuf sem_operation = { sem_number, -1, SEM_UNDO };
	 
	if (semop(sem_id, &sem_operation, 1) == -1)
	{
		fprintf(stderr, "Error locking semaphore\n");
		exit(1);
	}
}

void UnlockSemaphore(int sem_id, int sem_number)
{
	struct sembuf sem_operation = { sem_number, 1, SEM_UNDO };
	 
	if (semop(sem_id, &sem_operation, 1) == -1)
	{
		fprintf(stderr, "Error unlocking semaphore\n");
		exit(1);
	}
}

void adjustSem_Adding(int sem_id, int Producer_ID)
{
	int value;
	// If PRODUCER_A => function adds +1 element  to buffer
	// If PRODUCER_B => function adds +2 elements to buffer
	value = (Producer_ID == PRODUCER_A) ? 1 : 2;

	int errors[2];

	struct sembuf sem_operation_1 = { BLOCK_ADDING, -value, SEM_UNDO };
	errors[0] = semop(sem_id, &sem_operation_1, 1);

	struct sembuf sem_operation_2 = { BLOCK_DELETING, value, SEM_UNDO };
	errors[1] = semop(sem_id, &sem_operation_2, 1);
	

	int i;
	for(i = 0; i <= 1; i++)
		if(errors[i] == -1)
		{
			fprintf(stderr, "Error adjusting semaphore adding!\n");
    			exit(1);
		}
}

void adjustSem_Deleting(int sem_id)
{
	int errors[2];

	struct sembuf sem_operation_1 = { BLOCK_ADDING, 1, SEM_UNDO };
	errors[0] = semop(sem_id, &sem_operation_1, 1);

	struct sembuf sem_operation_2 = { BLOCK_DELETING, -1, SEM_UNDO };
	errors[1] = semop(sem_id, &sem_operation_2, 1);
	
	int i;
	for(i = 0; i <= 1; i++)
		if(errors[i] == -1)
		{
			fprintf(stderr, "Error adjusting semaphore adding!\n");
    			exit(1);
		}
}

void Init_semaphores(int sem_id, unsigned short int *semvals)
{ 
	union semun semval;

	semval.array = semvals;	
    	if (semctl(sem_id, 0, SETALL, semval) == -1)
    	{
    		fprintf(stderr, "Error initializing semaphore\n");
    		exit(1);
    	}
}

void Init_buffer(struct product *prod_buff)
{
	int i;

	for(i = 0; i < 3; i++)
	{
		prod_buff[i].val = 1;
		prod_buff[i].letterProduced = 'd';
		prod_buff[i].isReadConsumerA = false;
		prod_buff[i].isReadConsumerB = false;
	}

	// Elements acting like flags:
	prod_buff[KILL_CONSUMERS].val = 0;
	prod_buff[KILL_PRODUCERS].val = 0;
	prod_buff[DISABLE_CONSUMERS].val = 0;
	prod_buff[DISABLE_PRODUCERS].val = 0;
}	

int getBufferLength(int sem_id)
{
	union semun semval;
	int val;

	val = semctl(sem_id, BLOCK_DELETING, GETVAL, semval);
	if(val == -1)
	{
		fprintf(stderr, "Error getting buffer length!\n");
		exit(1);
	}
	return(val + 3);
}
