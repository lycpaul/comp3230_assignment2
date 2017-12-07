OBJS = thread_func.o test.o
CC = gcc
DEBUG = -g
CFLAGS = -Wall -fopenmp -pthread -std=c99 

all: test.out

test.out: $(OBJS)  
	$(CC) $(CFLAGS) $(OBJS) -o test.out

test.o: thread_func.h test.c
	$(CC) $(CFLAGS) -c test.c

thread_func.o : thread_func.c thread_func.h
	$(CC) $(CFLAGS) -c thread_func.c 

clean:
	rm *.o *.out
