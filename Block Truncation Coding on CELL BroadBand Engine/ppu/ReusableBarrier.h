#ifndef ReusableBarrier_H
#define ReusableBarrier_H

#include <pthread.h>


typedef struct ReusableBarrier
{
	unsigned num_threads;
	unsigned count_threads;
	pthread_cond_t cond;
	pthread_mutex_t cond_lock;
} ReusableBarrier;


void ReusableBarrier_init(ReusableBarrier *bar, unsigned num_threads);
void ReusableBarrier_destroy(ReusableBarrier *bar);
void ReusableBarrier_wait(ReusableBarrier *bar);

#endif

