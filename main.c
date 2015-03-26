#include <assert.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "help.h"
#include <string.h>
#include <errno.h>

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in below.
 *
 * === User information ===
 * User 1: gudjonj13
 * SSN: 090391-2089
 * User 2: solveigsg12
 * SSN: 010291-2169
 * === End User Information ===
 ********************************************************/

static void *barber_work(void *arg);

/**
 * Wrapper functions
 */
void Sem_init(sem_t *sem, int pshared, unsigned int value)
{
	int ret;

	if((ret = sem_init(sem, pshared, value)) < 0)
	{
		fprintf(stderr, "sem_init error: %s", strerror(errno));
	}
}

void Sem_wait(sem_t *sem)
{
	int ret;

	if((ret = sem_wait(sem)) < 0)
	{
		fprintf(stderr, "sem_wait error: %s", strerror(errno));
	}
}

void Sem_post(sem_t *sem)
{
	int ret;

	if((ret = sem_post(sem)) < 0)
	{
		fprintf(stderr, "sem_post error: %s", strerror(errno));
	}
}

void Sem_getvalue(sem_t *sem, int *sval)
{
	int ret;

	if((ret = sem_getvalue(sem, sval)) < 0)
	{
		fprintf(stderr, "sem_getvalue error: %s", strerror(errno));
	}
}

void Pthread_create(pthread_t *tid, pthread_attr_t *attr, void * (*routine)(void *), void *arg)
{
	int ret;

	if((ret = pthread_create(tid, attr, routine, arg)) < 0)
	{
		fprintf(stderr, "pthread_create error: %s", strerror(errno));
	}
}

void Pthread_detach(pthread_t tid)
{
	int ret;

	if((ret = pthread_detach(tid)) < 0)
	{
		fprintf(stderr, "pthread_detach error: %s", strerror(errno));
	}
}
/**
 * Wrapper functions end
 */

struct chairs
{
    struct customer **customer; 	/* Array of customers */
    int max;						/* Maximum number of customers */
	int front;						/* customer[(front+1)%max] us first customer */
	int rear;						/* customer[rear%max] us last customer */
	sem_t mutex;					/* Protects accesses to customer */
	sem_t free_chairs;				/* Counts available chairs */
	sem_t occupied_chairs;			/* Counts available customers for cutting */
};

struct barber
{
    int room;
    struct simulator *simulator;
};

struct simulator
{
    struct chairs chairs;
    
    pthread_t *barberThread;
    struct barber **barber;
};

/**
 * Initialize data structures and create waiting barber threads.
 */
static void setup(struct simulator *simulator)
{
    struct chairs *chairs = &simulator->chairs;
    /* Setup semaphores*/
    chairs->max = thrlab_get_num_chairs();
	chairs->front = chairs->rear = 0;
    Sem_init(&chairs->mutex, 0, 1);							/* Binary semaphore for locking */
	Sem_init(&chairs->free_chairs, 0, chairs->max);			/* max free chairs */
	Sem_init(&chairs->occupied_chairs, 0,  0);				/* zero occupied chairs */

    /* Create chairs*/
    chairs->customer = malloc(sizeof(struct customer *) * thrlab_get_num_chairs());
    
    /* Create barber thread data */
    simulator->barberThread = malloc(sizeof(pthread_t) * thrlab_get_num_barbers());
    simulator->barber = malloc(sizeof(struct barber*) * thrlab_get_num_barbers());

    /* Start barber threads */
    struct barber *barber;
    for (unsigned int i = 0; i < thrlab_get_num_barbers(); i++) {
		barber = calloc(sizeof(struct barber), 1);
		barber->room = i;
		barber->simulator = simulator;
		simulator->barber[i] = barber;
		Pthread_create(&simulator->barberThread[i], 0, barber_work, barber);
		Pthread_detach(simulator->barberThread[i]);
    }
}

/**
 * Free all used resources and end the barber threads.
 */
static void cleanup(struct simulator *simulator)
{
    /* Free chairs */
    free(simulator->chairs.customer);

    /* Free barber thread data */
    free(simulator->barber);
    free(simulator->barberThread);
}

/**
 * Called in a new thread each time a customer has arrived.
 */
static void customer_arrived(struct customer *customer, void *arg)
{
    struct simulator *simulator = arg;
    struct chairs *chairs = &simulator->chairs;

    Sem_init(&customer->mutex, 0, 0);

	/**
	 * Get the int value of free_chairs semaphore, if there are free chairs
	 * accept the customer, else reject him/her
	 */
	Sem_wait(&chairs->mutex);  /* Only one customer can check for free chairs at a time */
	int free_chairs;
	Sem_getvalue(&chairs->free_chairs, &free_chairs);
	if(free_chairs > 0)
	{
		Sem_wait(&chairs->free_chairs);
		thrlab_accept_customer(customer);
		chairs->customer[(++chairs->rear) % chairs->max] = customer;
		Sem_post(&chairs->mutex);
		Sem_post(&chairs->occupied_chairs);

		/* Customer waits until hair is cut */
		Sem_wait(&customer->mutex);
	}
	else
	{
		thrlab_reject_customer(customer);
		Sem_post(&chairs->mutex);
	}
}

static void *barber_work(void *arg)
{
    struct barber *barber = arg;
    struct chairs *chairs = &barber->simulator->chairs;
    struct customer *customer;

    /* Main barber loop */
    while (true) {
		Sem_wait(&chairs->occupied_chairs);			/* Barber thread waits for customers */
		Sem_wait(&chairs->mutex);
		customer = chairs->customer[(++chairs->front) % (chairs->max)]; /* Get next customer in line */
		Sem_post(&chairs->mutex);
		Sem_post(&chairs->free_chairs);				/* Open up a chair */
		thrlab_prepare_customer(customer, barber->room);
        thrlab_sleep(5 * (customer->hair_length - customer->hair_goal));
        thrlab_dismiss_customer(customer, barber->room);

		/* Customer can leave barber shop */
		Sem_post(&customer->mutex);
    }
    return NULL;
}

int main (int argc, char **argv)
{
    struct simulator simulator;

    thrlab_setup(&argc, &argv);
    setup(&simulator);

    thrlab_wait_for_customers(customer_arrived, &simulator);

    thrlab_cleanup();
    cleanup(&simulator);

    return EXIT_SUCCESS;
}
