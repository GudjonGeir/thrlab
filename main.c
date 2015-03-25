#include <assert.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "help.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in below.
 *
 * === User information ===
 * User 1: 
 * SSN:
 * User 2:
 * SSN:
 * === End User Information ===
 ********************************************************/

struct chairs
{
    struct customer **customer; 	/* Array of customers */
    int max;						/* Maximum number of customers */
	int front;						/* customer[(front+1)%max] us first customer */
	int rear;						/* customer[rear%max] us last customer */
	sem_t mutex;					/* Protects accesses to customer */
	sem_t free_chairs;				/* Counts available chairs */
	sem_t occupied_chairs;			/* Counts available customers for cutting */
    /* TODO: Add more variables related to threads */
    /* Hint: Think of the consumer producer thread problem */
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

static void *barber_work(void *arg)
{
    struct barber *barber = arg;
    struct chairs *chairs = &barber->simulator->chairs;
    struct customer *customer = 0; /* TODO: Fetch a customer from a chair */

    /* Main barber loop */
    while (true) {
		sem_wait(&chairs->occupied_chairs);
		sem_wait(&chairs->mutex);
		customer = chairs->customer[(++chairs->front) % (chairs->max)];
		sem_post(&chairs->mutex);
		sem_post(&chairs->free_chairs);
		thrlab_prepare_customer(customer, barber->room);
        thrlab_sleep(5 * (customer->hair_length - customer->hair_goal));
        thrlab_dismiss_customer(customer, barber->room);
    }
    return NULL;
}

/**
 * Initialize data structures and create waiting barber threads.
 */
static void setup(struct simulator *simulator)
{
    struct chairs *chairs = &simulator->chairs;
    /* Setup semaphores*/
    chairs->max = thrlab_get_num_chairs();
	chairs->front = chairs->rear = 0;
    sem_init(&chairs->mutex, 0, 1);							/* Binary semaphore for locking */
	sem_init(&chairs->free_chairs, 0, chairs->max);			/* max free chairs */
	sem_init(&chairs->occupied_chairs, 0,  0);				/* zero occupied chairs */

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
		pthread_create(&simulator->barberThread[i], 0, barber_work, barber);
		pthread_detach(simulator->barberThread[i]);
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

    sem_init(&customer->mutex, 0, 0);

	// sem_getvalue(sem_t *sem, int *sval);
	// if(chairs->free_chairs > 0)
	{
		sem_wait(&chairs->free_chairs);
		sem_wait(&chairs->mutex);
		// TODO: create customer thread and wait
		thrlab_accept_customer(customer);
		chairs->customer[(++chairs->rear) % chairs->max] = customer;
		sem_post(&chairs->mutex);
		sem_post(&chairs->occupied_chairs);
	}
	// else
	{
		thrlab_reject_customer(customer);
	}
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
