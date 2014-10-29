#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <pthread.h>

#include "ReusableBarrier.h"

void ReusableBarrier_init(ReusableBarrier *bar, unsigned num_threads)
{
    pthread_mutex_init(&bar->cond_lock, NULL);
    pthread_cond_init(&bar->cond, NULL);
    bar->count_threads = num_threads;
    bar->num_threads = num_threads;
}

void ReusableBarrier_destroy(ReusableBarrier *bar)
{
    pthread_mutex_destroy(&bar->cond_lock); 
    pthread_cond_destroy(&bar->cond);
}

void ReusableBarrier_wait(ReusableBarrier *bar)
{
    // acquire -> intra in regiunea critica
    pthread_mutex_lock(&(bar->cond_lock));
    (bar->count_threads)--;
    
    if(bar->count_threads == 0)
    {
        // trezeste toate thread-urile, acestea vor putea reintra 
        // in regiunea critica dupa release
        pthread_cond_broadcast(&(bar->cond));
        bar->count_threads = bar->num_threads;
    }
    else
    {
        // iese din regiunea critica, se blocheaza,
        // cand se deblocheaza face acquire pe lock
        pthread_cond_wait(&(bar->cond), &(bar->cond_lock)); 
    }
    // iesim din regiunea critica
    pthread_mutex_unlock(&(bar->cond_lock));
}

