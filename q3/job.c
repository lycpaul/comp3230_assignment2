#include "job.h"
#include <unistd.h>
#include <semaphore.h>

void reportJobDone(sem_t *sem_worker, int num_worker) {
	int *num_free_worker = malloc(sizeof(num_free_worker));
	sem_getvalue(sem_worker, num_free_worker);
	if(*num_free_worker < num_worker) {
		free(num_free_worker);
		sem_post(sem_worker);
	}else {
		printf("Error, number of free workers exceeds num_worker\n");
		free(num_free_worker);
		exit(1);
	}
}

int requestSpace(sem_t *space) {
#if DEBUG
	int *num_free_space = malloc(sizeof(num_free_space));
	sem_getvalue(space, num_free_space);
	printf("Requesting free space, current space=%d...\n", *num_free_space);
#endif
	sem_wait(space);
#if DEBUG
	sem_getvalue(space, num_free_space);
	printf("Space requested, current space=%d...\n", *num_free_space);
#endif

	return 0;
}

void releaseSpace(sem_t *space, int space_limit) {
	int *num_free_space = malloc(sizeof(num_free_space));
	sem_getvalue(space, num_free_space);
	if(*num_free_space < space_limit) {
#if DEBUG
		printf("releasing free space, current space=%d...\n", *num_free_space);
#endif
		sem_post(space);
#if DEBUG
		sem_getvalue(space, num_free_space);
		printf("Space released, current space=%d...\n", *num_free_space);
#endif
		free(num_free_space);
	} else {
		printf("Error, releasing space that doesn't exist\n");
		free(num_free_space);
		exit(1);
	}
}

void makeItem(sem_t *space, int makeTime, sem_t* item) {
	usleep(100000*makeTime);
	// sleep(makeTime);
	requestSpace(space);
	sem_post(item);
}

int getItem(sem_t *space, int space_limit, sem_t *item) {
	int rv = sem_trywait(item);
	if (rv==0) {
		releaseSpace(space, space_limit);
		printf("I got the item\n");
		return 1;
	} else {
		return 0;
	}
}

void makeSkeleton(sem_t *sem_space, sem_t *sem_skeleton) {
	makeItem(sem_space, TIME_SKELETON, sem_skeleton);
}

void makeEngine(sem_t *sem_space, sem_t *sem_engine) {
	makeItem(sem_space, TIME_ENGINE, sem_engine);
}

void makeChassis(sem_t *sem_space, sem_t *sem_chassis) {
	makeItem(sem_space, TIME_CHASSIS, sem_chassis);
}

void makeWindow(sem_t *sem_space, sem_t *sem_window) {
	makeItem(sem_space, TIME_WINDOW, sem_window);
}

void makeTire(sem_t *sem_space, sem_t *sem_tire) {
	makeItem(sem_space, TIME_TIRE, sem_tire);
}

void makeBattery(sem_t *sem_space, sem_t *sem_battery ) {
	// call makeItem and pass in the time for makeing battery
	makeItem(sem_space, TIME_BATTERY, sem_battery);
}

void makeBody(sem_t *sem_space, int space_limit, sem_t *sem_body,
		sem_t *sem_skeleton, sem_t *sem_engine, sem_t *sem_chassis) {
	//	making the assembly work more flexible
	//	locking for all required components instead of blocking by particlar component
	int getS=0, getE=0, getC=0;
	while ((getS+getE+getC)<3) {
		if (getS<1)
			getS += getItem(sem_space, space_limit, sem_skeleton);
		if (getE<1)
			getE += getItem(sem_space, space_limit, sem_engine);
		if (getC<1)
			getC += getItem(sem_space, space_limit, sem_chassis);
	}
	makeItem(sem_space, TIME_BODY, sem_body);
}

void makeCar(sem_t *sem_space, int space_limit, sem_t *sem_car,
		sem_t *sem_window, sem_t *sem_tire, sem_t *sem_battery, sem_t *sem_body) {
		
	//	making the assembly work more flexible
	//	locking for all required components instead of blocking by particlar component
	int getW=0, getT=0, getBt=0, getBd=0;
	while ((getW+getT+getBt+getBd)<13) {
		if (getW<7)
			getW += getItem(sem_space, space_limit, sem_window);
		if (getT<4)
			getT += getItem(sem_space, space_limit, sem_tire);
		if (getBt<1)
			getBt += getItem(sem_space, space_limit, sem_battery);
		if (getBd<1)	
			getBd += getItem(sem_space, space_limit, sem_body);
	}

	usleep(100000*TIME_CAR);
	//	sleep(TIME_CAR);
	sem_post(sem_car);
}

