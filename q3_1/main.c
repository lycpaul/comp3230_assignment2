#include "definitions.h"
#include "main.h"
#include <omp.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>

sem_t sem_worker;
sem_t sem_space;

sem_t sem_skeleton;
sem_t sem_engine;
sem_t sem_chassis;
sem_t sem_body;

sem_t sem_window;
sem_t sem_tire;
sem_t sem_battery;
sem_t sem_car;

//	semaphores for the consumer and producer model
sem_t full, available;
pthread_mutex_t mutex;
int productionGoal[7];
int allocated[7];
int available_space;

int num_cars;
int num_spaces;
int num_workers;

bool finishedProduction() {
	for(int i=0; i<8; i++)
		if (productionGoal[i]>allocated[i]) return false;
	printf("Finished Production\n");
	return true;
}

void *producer(void *arg) {
	// resource package is passed to here
	resource_pack *rpack = (resource_pack *) arg;

	//	job schedule and their repective job times
	int jobList[8] = {SKELETON, ENGINE, CHASSIS, WINDOW, WINDOW, TIRE, TIRE, BATTERY};
	int timesList[8] = {1,1,1,4,3,2,2,1};
	
	//	Each works got their own pthread_t and work_pack
	pthread_t threads[num_workers];
	work_pack wpackList[num_workers];
	
	int jid=0, tid=0;
	bool assignedAllThreads = false; /*for checking if all threads have been assigned job before*/
	
	while(true) {
		//	block the producer to assign new worker, untill space is released by consumer
		sem_wait(&available);
		if(finishedProduction()) break;
		pthread_mutex_lock(&mutex);	/*enter the critical session*/
		
		#if DEBUG
		printf("=-=-=-=-=-=Producer enter critical session=-=-=-=-=-=\n");
		#endif
		
		//	prepare forn new work pack
		wpackList[tid].resource = rpack;
		wpackList[tid].tid = tid;
		wpackList[tid].jid = jobList[jid];
		wpackList[tid].times = timesList[jid];
		
		//	check if have enough space, if not, call the consumer to free up some space
		if (available_space <= wpackList[tid].times) {
			#if DEBUG
			printf("Producer running out of space, wake consumer up\n");
			#endif
			pthread_mutex_unlock(&mutex);	/*leave the critical session*/
			sem_post(&full);	/* wake up consumer */
			continue;
		} else if (
			(allocated[BODY]		 >= (1+allocated[CAR])
				&& allocated[WINDOW] >= (7+7*allocated[CAR])
				&& allocated[TIRE]   >= (4+4*allocated[CAR])
				&& allocated[BATTERY]>= (1+allocated[CAR]))
		   || (allocated[SKELETON]   >= (1+allocated[BODY])
		   		&& allocated[ENGINE] >= (1+allocated[BODY])
		   		&& allocated[CHASSIS]>= (1+allocated[BODY]))
		   		)
   		{
			// check if we can assembly BODY or CAR, call the consumer to do this
			#if DEBUG
			printf("seems like we can make a BODY or CAR, wake the consumer up\n");
			#endif
			pthread_mutex_unlock(&mutex);	/*leave the critical session*/
			sem_post(&full);	/* wake up consumer */
			continue;
		}
		else sem_post(&available); /*continue producer task*/
		
		//	update the produced matrix
		allocated[wpackList[tid].jid] += wpackList[tid].times;
		available_space -= wpackList[tid].times;
		#if DEBUG
		printf("allocated matrix: %d %d %d %d\n", allocated[0],allocated[1],allocated[2],allocated[3]);
		printf("allocated matrix: %d %d %d %d\n", allocated[4],allocated[5],allocated[6],allocated[7]);
		printf("Available_space: %d\n", available_space);
		#endif
		
		//	before assigning this tid, make sure it is free
		if (assignedAllThreads) { 
			#if DEBUG
			printf("Producer waiting old thread joint\n");
			#endif
			pthread_join(threads[tid], NULL);
		}
		
		//	requesting for a worker
		sem_wait(&sem_worker);
	
		//	finally, give him/her a work to do
		#if DEBUG
		printf("Producer: tid: %d doing %d\n", wpackList[tid].tid, wpackList[tid].jid);
		#endif
		pthread_create(&threads[tid], NULL, work, (void *)&wpackList[tid]);
		
		//	update variables
		if (++tid == num_workers) {
			assignedAllThreads = true;
			tid %= num_workers;
		}
		jid = (jid+1)%8;
		
		
		pthread_mutex_unlock(&mutex);	/*leave the critical session*/
		
		//	check production stage
		if(finishedProduction()) break;
	}
	#if DEBUG
	printf("leaving producer thread\n");
	#endif
	//	join all threads before levaing
	if (assignedAllThreads) {
		for (int i=0; i<num_workers; i++)
			pthread_join(threads[i], NULL);
	} else {
		for (int i=0; i<tid; i++)
			pthread_join(threads[i], NULL);
	}
	pthread_exit(0);
}

void *consumer(void *arg) {
	// resource package is passed to here
	resource_pack *rpack = (resource_pack *) arg;
	
	//	Each works got their own pthread_t and work_pack
	pthread_t threads[num_workers];
	work_pack wpackList[num_workers];
	
	int tid=0;
	bool assignedAllThreads = false; /*for checking if all threads have been assigned job before*/
	
	while(true) {
		//	wait untill the producer wake me up
		sem_wait(&full);
		
		#if DEBUG
		printf("=-=-=-=-=-=Consumer enter critical session=-=-=-=-=-=\n");
		#endif
		
		pthread_mutex_lock(&mutex);	/*enter the critical session*/
		
		//	Ensure finished previous job
		if (assignedAllThreads) { 
			#if DEBUG
			printf("Consumer waiting old thread join\n");
			#endif
			pthread_join(threads[tid], NULL);
		}
		
		//	prepare for new work pack
		wpackList[tid].resource = rpack;
		wpackList[tid].tid = tid;
		wpackList[tid].times = 1;
		
		//	decide which one to produce, BODY or CAR
		if (allocated[BODY]		 >= (1+allocated[CAR])
			&& allocated[WINDOW] >= (7+7*allocated[CAR])
			&& allocated[TIRE]   >= (4+4*allocated[CAR])
			&& allocated[BATTERY]>= (1+allocated[CAR]))
			wpackList[tid].jid = CAR;
		else if (allocated[SKELETON]>= (1+allocated[BODY])
				 || allocated[ENGINE]>= (1+allocated[BODY])
				 || allocated[CHASSIS]>= (1+allocated[BODY]))
			wpackList[tid].jid = BODY;
		else wpackList[tid].jid = CAR;

		//	update the produced matrix
		if (wpackList[tid].jid == BODY) {
			available_space += allocated[SKELETON]+allocated[ENGINE]+allocated[CHASSIS]-3*(allocated[BODY])-1;	//	remarks, body itself consume space
		} else {
			available_space += allocated[BODY]+allocated[WINDOW]+allocated[TIRE]+allocated[BATTERY]-13*allocated[CAR];
		}
		allocated[wpackList[tid].jid] += wpackList[tid].times;
		#if DEBUG
		printf("allocated matrix: %d %d %d %d\n", allocated[0],allocated[1],allocated[2],allocated[3]);
		printf("allocated matrix: %d %d %d %d\n", allocated[4],allocated[5],allocated[6],allocated[7]);
		printf("Available_space: %d\n", available_space);
		#endif
		
		//	requesting for a worker
		sem_wait(&sem_worker);
	
		//	finally, give him/her a work to do
		#if DEBUG
		printf("Consumer: tid: %d doing %d\n", wpackList[tid].tid, wpackList[tid].jid);
		#endif
		pthread_create(&threads[tid], NULL, work, (void*)&wpackList[tid]);
		
		//	update variables
		if (++tid == num_workers) {
			assignedAllThreads = true;
			tid %= num_workers;
		}
		
		//	space is freed automatically after the thread return,
		//	now wake up the producer
		pthread_mutex_unlock(&mutex);	/*leave the critical session*/
		sem_post(&available);	/* wake up consumer */
		//	check production stage
		if(finishedProduction()) break;
	}
	#if DEBUG
	printf("leaving consumer thread\n");
	#endif
	//	join all threads before levaing
	if (assignedAllThreads) {
		for (int i=0; i<num_workers; i++)
			pthread_join(threads[i], NULL);
	} else {
		for (int i=0; i<tid; i++)
			pthread_join(threads[i], NULL);
	}
	
	pthread_exit(0);
}

int main(int argc, char** argv)
{
	if (argc < 4) {
	printf("Usage: %s <number of cars> <number of spaces> <number of workers>\n", 
	argv[0]);
	return EXIT_SUCCESS;
	}
	num_cars     = atoi(argv[1]);
	num_spaces   = atoi(argv[2]);
	num_workers  = atoi(argv[3]);

	printf("Name: Lee Yu Chung UID: 3035180665\n");
	printf("Job defined, %d workers will build %d cars with %d storage spaces\n",
			num_workers, num_cars, num_spaces);

	resource_pack *rpack = (struct resource_pack*) malloc(sizeof(struct resource_pack));
	initResourcePack(rpack, num_spaces, num_workers); /*put semaphores into resource_pack*/

	double production_time = omp_get_wtime();
	
	/*working space for the consumer and producer model*/
	
	//	init the mutex and condition variables
	pthread_mutex_init(&mutex, NULL);
	sem_init(&full, 0, 0);
	sem_init(&available, 0, 1);	/*available is a binary state that indicates running out of space*/
	
	//	init the production goal
	for (int i=0; i<8; i++) {
		if (i==WINDOW) productionGoal[i] = num_cars*7;
		else if (i==TIRE) productionGoal[i] = num_cars*4;
		else productionGoal[i] = num_cars;
		allocated[i] = 0; /*monitoring job assigned and matching with production goal*/
	}
	available_space = num_spaces;
	
	//	create the threads
	pthread_t tid_con, tid_pro;
	pthread_create(&tid_con, NULL, consumer, (void*)rpack);
	pthread_create(&tid_pro, NULL, producer, (void*)rpack);
	#if DEBUG	
	printf("produced consumer and producer thread \n");
	#endif
	
	//	wait for the threads to finish
	pthread_join(tid_con, NULL);
	pthread_join(tid_pro, NULL);
	
	pthread_mutex_destroy(&mutex);
	sem_destroy(&full);
	sem_destroy(&available);
	
	/*working space for the consumer and producer model*/
	
	production_time = omp_get_wtime() - production_time;
	reportResults(production_time);

	destroySem();
	free(rpack);
	return EXIT_SUCCESS;
}

void reportResults(double production_time) {
	int *sem_value = malloc(sizeof(int));
	printf("=====Final report=====\n");

	sem_getvalue(&sem_skeleton, sem_value);
	printf("Unused Skeleton: %d\n",   *sem_value);
	sem_getvalue(&sem_engine,   sem_value);
	printf("Unused Engine: %d\n",     *sem_value);
	sem_getvalue(&sem_chassis,  sem_value);
	printf("Unused Chassis: %d\n",    *sem_value);
	sem_getvalue(&sem_body,     sem_value);
	printf("Unused Body: %d\n",       *sem_value);
	sem_getvalue(&sem_window,   sem_value);
	printf("Unused Window: %d\n",     *sem_value);
	sem_getvalue(&sem_tire,     sem_value);
	printf("Unused Tire: %d\n",       *sem_value);
	sem_getvalue(&sem_battery,  sem_value);
	printf("Unused Battery: %d\n",    *sem_value);

	sem_getvalue(&sem_space, sem_value);
	if (*sem_value < num_spaces) {
		printf("There are waste car parts!\n");
	}
	sem_getvalue(&sem_car, sem_value);
	printf("Production of %d %s done, production time: %f sec, space usage: %d\n", 
			*sem_value,
			*sem_value > 1 ? "cars" : "car",	       
			production_time, num_spaces);
	printf("==========\n");
	free(sem_value);
}

void initResourcePack(struct resource_pack *pack,
		int space_limit, int num_workers) {
	initSem();
	pack->space_limit  = space_limit;
	pack->num_workers  = num_workers;
	pack->sem_space    = &sem_space   ;
	pack->sem_worker   = &sem_worker  ;

	pack->sem_skeleton = &sem_skeleton;
	pack->sem_engine   = &sem_engine  ;
	pack->sem_chassis  = &sem_chassis ;
	pack->sem_body     = &sem_body    ;

	pack->sem_window   = &sem_window  ;
	pack->sem_tire     = &sem_tire    ;
	pack->sem_battery  = &sem_battery ;
	pack->sem_car      = &sem_car     ;
}

int destroySem(){
#if DEBUG
	printf("Destroying semaphores...\n");
#endif
	sem_destroy(&sem_worker);
	sem_destroy(&sem_space);

	sem_destroy(&sem_skeleton);
	sem_destroy(&sem_engine);
	sem_destroy(&sem_chassis);
	sem_destroy(&sem_body);

	sem_destroy(&sem_window);
	sem_destroy(&sem_tire);
	sem_destroy(&sem_battery);
	sem_destroy(&sem_car);
#if DEBUG
	printf("Semaphores destroyed\n");
#endif
	return 0;
}

int initSem(){
#if DEBUG
	printf("Initiating semaphores...\n");
#endif
	sem_init(&sem_worker,   0, num_workers);
	sem_init(&sem_space,    0, num_spaces);

	sem_init(&sem_skeleton, 0, 0);
	sem_init(&sem_engine,   0, 0);
	sem_init(&sem_chassis,  0, 0);
	sem_init(&sem_body,     0, 0);

	sem_init(&sem_window,   0, 0);
	sem_init(&sem_tire,     0, 0);
	sem_init(&sem_battery,  0, 0);
	sem_init(&sem_car,      0, 0);
#if DEBUG
	printf("Init semaphores done!\n");
#endif
	return 0;
}

