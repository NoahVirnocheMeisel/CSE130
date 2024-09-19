#include "rwlock.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>

//Utilizing code provided in the OSTEP Book for general structure/implentation for the reader/writer lock. Pivoted away from semaphores for
//the NWAY lock.

struct rwlock {
    sem_t multiple_process_lock;
    sem_t single_process_lock;
    //pthread_cond_t writers_turn;
    int more_than_one_agent_count;
    int priority;
    pthread_cond_t signal_reader;
    pthread_cond_t signal_writer;
    pthread_mutex_t lock;
    int writer;
    int readers;
    int num_gone;
    int nway_max;
    int num_writers;
    int readers_waiting;
};

//TODO: Notes on issue. Throughput test is stalling forever. Threads are getting the lock twice???All reads are exiting but writes stalled?

rwlock_t *rwlock_new(PRIORITY p, uint32_t n) {
    rwlock_t *rw = calloc(1, sizeof(rwlock_t));
    rw->more_than_one_agent_count = 0;
    rw->priority = p;
    rw->nway_max = n;
    rw->num_gone = 0;
    rw->num_writers = 0;
    rw->readers_waiting = 0;
    sem_init(&rw->multiple_process_lock, 0, 1);
    sem_init(&rw->single_process_lock, 0, 1);
    pthread_cond_init(&rw->signal_reader, NULL);
    pthread_cond_init(&rw->signal_writer, NULL);
    pthread_mutex_init(&rw->lock, NULL);
    return rw;
}
void rwlock_delete(rwlock_t **l) {
    if (l != NULL && *l != NULL) {
        sem_destroy(&((*l)->multiple_process_lock));
        sem_destroy(&((*l)->single_process_lock));
        pthread_mutex_destroy(&(*l)->lock);
        pthread_cond_destroy(&(*l)->signal_reader);
        pthread_cond_destroy(&(*l)->signal_writer);
        free(*l);
        *l = NULL;
        l = NULL;
    }
}
void reader_lock(rwlock_t *rw) {
    if (rw->priority == READERS) {
        sem_wait(&rw->multiple_process_lock);
        rw->more_than_one_agent_count++;
        if (rw->more_than_one_agent_count
            == 1) { // first reader gets writelock, and blocks any writing
            sem_wait(&rw->single_process_lock);
        }
        sem_post(&rw->multiple_process_lock);
    } else if (rw->priority == WRITERS) {
        //Semaphore Implementation that feels scuffed
        sem_wait(&rw->multiple_process_lock);
    } else if (rw->priority == N_WAY) {
        //Obtain the lock, check if the number of readers in the critical region + the number of completed is greater than n
        //if so, and there are writers, suspend the thread until signaled.
        sem_wait(&rw->single_process_lock);
        rw->readers_waiting++;
        sem_post(&rw->single_process_lock);
        pthread_mutex_lock(&rw->lock);
        //rw->readers_waiting++;
        while (((rw->num_gone + rw->more_than_one_agent_count) >= rw->nway_max)
               && (rw->num_writers > 0)) {
            pthread_cond_wait(&rw->signal_reader, &rw->lock);
        }
        rw->readers_waiting--;
        rw->more_than_one_agent_count++;
        pthread_mutex_unlock(&rw->lock);

    } else {
        fprintf(stderr, "Invalid Priority\n");
    }
}

void reader_unlock(rwlock_t *rw) {
    if (rw->priority == READERS) {
        sem_wait(&rw->multiple_process_lock);
        rw->more_than_one_agent_count--;
        if (rw->more_than_one_agent_count == 0) {
            sem_post(&rw->single_process_lock);
        }
        sem_post(&rw->multiple_process_lock);
    } else if (rw->priority == WRITERS) {
        sem_post(&rw->multiple_process_lock);
    } else if (rw->priority == N_WAY) {
        //obtain the lock, and then mark read as complete, while also incremeting the number of completed reads.
        //If this is the last thread in one batch of reads, signal all of the writers to wake up once.
        pthread_mutex_lock(&rw->lock);
        (rw->more_than_one_agent_count)--;
        rw->num_gone++;
        if (((rw->more_than_one_agent_count == 0 && rw->num_gone >= rw->nway_max)
                || (rw->more_than_one_agent_count == 0 && rw->readers_waiting == 0))) {
            pthread_cond_signal(&rw->signal_writer);
        } else {
            pthread_cond_broadcast(&rw->signal_reader);
        }
        pthread_mutex_unlock(&rw->lock);

    } else {
        fprintf(stderr, "Invalid Priority\n");
    }
}
void writer_lock(rwlock_t *rw) {
    if (rw->priority == READERS) {
        sem_wait(&rw->single_process_lock);
    } else if (rw->priority == WRITERS) {
        pthread_mutex_lock(&rw->lock);
        rw->num_writers++;
        pthread_mutex_unlock(&rw->lock);
        sem_wait(&rw->single_process_lock);
        if (rw->more_than_one_agent_count == 1) {
            sem_wait(&rw->multiple_process_lock);
        }
        rw->num_writers--;
        rw->more_than_one_agent_count++;
    } else if (rw->priority == N_WAY) {
        //Obtain the lock to increment the number of waiting writers. Then wait if there are still readers, and still more reads needed
        pthread_mutex_lock(&rw->lock);
        (rw->num_writers)++;
        if (rw->readers_waiting > 0 || rw->more_than_one_agent_count > 0) {
            pthread_cond_wait(&rw->signal_writer, &rw->lock);
        }
        (rw->num_writers)--;
        //pthread_mutex_unlock(&rw->lock);
    } else {
        fprintf(stderr, "Invalid Priority\n");
    }
}
void writer_unlock(rwlock_t *rw) {
    if (rw->priority == READERS) {
        sem_post(&rw->single_process_lock);
    } else if (rw->priority == WRITERS) {
        rw->more_than_one_agent_count--;
        if (rw->num_writers == 0) {
            sem_post(&rw->multiple_process_lock);
        }
        sem_post(&rw->single_process_lock);
    } else if (rw->priority == N_WAY) {
        //reset number of reads to 0, and decrement the number of waiting writers
        //wake up the sleeping reader threads, and then unlock the mutex
        //pthread_mutex_lock(&rw->lock);
        rw->num_gone = 0;
        if (rw->readers_waiting == 0 && rw->more_than_one_agent_count == 0) {
            pthread_cond_signal(&rw->signal_writer);
        } else {
            pthread_cond_broadcast(&rw->signal_reader);
        }
        pthread_mutex_unlock(&rw->lock);
    } else {
        fprintf(stderr, "Invalid Priority\n");
    }
}
