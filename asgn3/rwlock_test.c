/**
 * @File rwlock_test.c
 *
 * Customizable test for the reader/writer lock.
 * Inputs: rwlock priority, N_WAY thread count, num reader threads, num writer
 *         threads, test duration
 * Output: rwlock thread statistics
 * Compile: clang -Wall -Werror -Wextra -Wpedantic -Wstrict-prototypes
 *          [-DDEBUG] -o rwlock_test rwlock_test.c rwlock.o -lpthread
 *
 * @author Mitchell Elliott.
 *
 */

#include "rwlock.h"

#include <assert.h>
#include <limits.h>
#include <err.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

pthread_mutex_t printmutex = PTHREAD_MUTEX_INITIALIZER;

#define ferror(...)                                                                                \
    do {                                                                                           \
        pthread_mutex_lock(&printmutex);                                                           \
        fprintf(stderr, "[%s:%s():%d]\t", __FILE__, __func__, __LINE__);                           \
        fprintf(stderr, __VA_ARGS__);                                                              \
        fprintf(stderr, "\n");                                                                     \
        pthread_mutex_unlock(&printmutex);                                                         \
    } while (0);

#ifdef DEBUG
#define fdebug(...)                                                                                \
    do {                                                                                           \
        pthread_mutex_lock(&printmutex);                                                           \
        fprintf(stderr, "[%s:%s():%d]\t", __FILE__, __func__, __LINE__);                           \
        fprintf(stderr, __VA_ARGS__);                                                              \
        fprintf(stderr, "\n");                                                                     \
        pthread_mutex_unlock(&printmutex);                                                         \
    } while (0);
#else
#define fdebug(x, ...) ((void) 0)
#endif

#define ANY_RD (1 << 0)
#define ONE_WR (1 << 1)
#define STRESS (1 << 2)
#define TPUT   (1 << 3)

int start = 0;
int done = 0;
pthread_mutex_t setmutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t donemutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t rwlockmutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t rcv;
pthread_cond_t wcv;
int areaders = 0;
int awriters = 0;
int nreaders = 0;
int nwriters = 0;
int allreaders = 0;
int nwayreaders = 0;
int rc = 0;

typedef struct ThreadObj {
    pthread_t thread;
    int id;
    int *counts;
} ThreadObj;

typedef ThreadObj *Thread;

// The reader/writer lock
rwlock_t *rwlock;

// Sleep for a random amount of time
void random_sleep(unsigned int value) {
    unsigned r = rand() % value;
    int rc = usleep(r);
    assert(!rc);
}

// Safely see if we're able to start
int get_start(void) {
    pthread_mutex_lock(&setmutex);
    int s = start;
    pthread_mutex_unlock(&setmutex);
    return s;
}

// Safely say that we've started
void set_start(void) {
    pthread_mutex_lock(&setmutex);
    start = 1;
    pthread_cond_broadcast(&rcv);
    pthread_cond_broadcast(&wcv);
    pthread_mutex_unlock(&setmutex);
}

// Safely see if we're done
int get_done(void) {
    pthread_mutex_lock(&donemutex);
    int d = done;
    pthread_mutex_unlock(&donemutex);
    return d;
}

// Safely say that we're done
void set_done(void) {
    pthread_mutex_lock(&donemutex);
    done = 1;
    pthread_mutex_unlock(&donemutex);
}

void *reader_thread(void *args) {
    Thread thread = (Thread) args;
    int id = thread->id;
    int *counts = (int *) thread->counts;

    pthread_mutex_lock(&rwlockmutex);

    while (!get_start()) {
        fdebug("reader %d waiting", id);
        pthread_cond_wait(&rcv, &rwlockmutex);
    }

    pthread_mutex_unlock(&rwlockmutex);

    fdebug("reader %d starting", id);

    while (!get_done()) {
        reader_lock(rwlock);
        fdebug("reader %d got lock", id);

        pthread_mutex_lock(&rwlockmutex);

        if (get_done()) {
            pthread_mutex_unlock(&rwlockmutex);
            reader_unlock(rwlock);
            return NULL;
        }

        counts[id]++;
        areaders++;

        if (areaders > nreaders || areaders < 0) {
            ferror("Error: invalid number of readers in the critical section - areaders: %d "
                   "awriters: %d",
                areaders, awriters);
            rc = rc | ANY_RD;
        } else if (areaders == nreaders) {
            allreaders = 1;
        }

        if (awriters > 0) {
            ferror("Error: writers in the critical section - areaders: %d awriters: %d", areaders,
                awriters);
            rc = rc | STRESS;
        }

        pthread_mutex_unlock(&rwlockmutex);

        random_sleep(10000);

        pthread_mutex_lock(&rwlockmutex);
        areaders--;
        pthread_mutex_unlock(&rwlockmutex);

        fdebug("reader %d done", id);
        reader_unlock(rwlock);
    }

    fdebug("reader %d exiting", id);
    return NULL;
}

void *writer_thread(void *args) {
    Thread thread = (Thread) args;
    int id = thread->id;
    int *counts = (int *) thread->counts;

    pthread_mutex_lock(&rwlockmutex);

    while (!get_start()) {
        fdebug("writer %d waiting", id);
        pthread_cond_wait(&wcv, &rwlockmutex);
    }

    pthread_mutex_unlock(&rwlockmutex);

    fdebug("writer %d starting", id);

    while (!get_done()) {
        writer_lock(rwlock);
        fdebug("writer %d got lock", id);

        pthread_mutex_lock(&rwlockmutex);

        if (get_done()) {
            pthread_mutex_unlock(&rwlockmutex);
            writer_unlock(rwlock);
            return NULL;
        }

        counts[id]++;
        awriters++;

        if (awriters > 1 || awriters < 0) {
            ferror("Error: multiple writers in the critical section - areaders: %d awriters: %d",
                areaders, awriters);
            rc = rc | ONE_WR;
        }

        if (areaders > 0) {
            ferror("Error: readers in the critical section - areaders: %d awriters: %d", areaders,
                awriters);
            rc = rc | STRESS;
        }

        pthread_mutex_unlock(&rwlockmutex);

        random_sleep(10000);

        pthread_mutex_lock(&rwlockmutex);
        awriters--;
        pthread_mutex_unlock(&rwlockmutex);

        fdebug("writer %d done", id);
        writer_unlock(rwlock);
    }

    fdebug("writer %d exiting", id);
    return NULL;
}

int main(int argc, char **argv) {
    if (argc < 6) {
        warnx("wrong arguments: %s", argv[0]);
        fprintf(stderr, "usage: %s <priority> <n> <readers> <writers> <time>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *priorityStr = argv[1];
    PRIORITY priority;

    if (strcmp(priorityStr, "READERS") == 0) {
        priority = READERS;
    } else if (strcmp(priorityStr, "WRITERS") == 0) {
        priority = WRITERS;
    } else if (strcmp(priorityStr, "N_WAY") == 0) {
        priority = N_WAY;
    } else {
        warnx("invalid priority: %s", priorityStr);
        return EXIT_FAILURE;
    }

    char *endptr = NULL;
    int n = (int) strtol(argv[2], &endptr, 10);

    if (endptr && *endptr != '\0') {
        warnx("invalid n: %s", argv[2]);
        return EXIT_FAILURE;
    }

    if (n < 0) {
        warnx("invalid n: %s", argv[2]);
        return EXIT_FAILURE;
    }

    nreaders = (int) strtol(argv[3], &endptr, 10);

    if (endptr && *endptr != '\0') {
        warnx("invalid readers: %s", argv[3]);
        return EXIT_FAILURE;
    }

    if (nreaders < 0) {
        warnx("invalid readers: %s", argv[3]);
        return EXIT_FAILURE;
    }

    nwriters = (int) strtol(argv[4], &endptr, 10);

    if (endptr && *endptr != '\0') {
        warnx("invalid writers: %s", argv[4]);
        return EXIT_FAILURE;
    }

    if (nwriters < 0) {
        warnx("invalid writers: %s", argv[4]);
        return EXIT_FAILURE;
    }

    int testTime = (int) strtol(argv[5], &endptr, 10);

    if (endptr && *endptr != '\0') {
        warnx("invalid test time: %s", argv[5]);
        return EXIT_FAILURE;
    }

    if (testTime < 0) {
        warnx("invalid test time: %s", argv[5]);
        return EXIT_FAILURE;
    }

    fprintf(stderr,
        "--------------------------------------------------------------------------------\n");
    fprintf(stderr, "Test started with priority: %s, n: %d, readers: %d, writers: %d, time: %d\n",
        priorityStr, n, nreaders, nwriters, testTime);

    Thread readerThreads[nreaders];
    Thread writerThreads[nwriters];
    uintptr_t sum = 0;
    int threadCount = nreaders + nwriters;
    int counts[threadCount];
    memset(counts, 0, sizeof(int) * threadCount);
    rwlock = rwlock_new(priority, n);

    if (rwlock == NULL) {
        warnx("rwlock_new() failed");
        return EXIT_FAILURE;
    }

    for (int i = 0; i < nreaders; ++i) {
        readerThreads[i] = malloc(sizeof(ThreadObj));
        readerThreads[i]->id = i;
        readerThreads[i]->counts = counts;
        pthread_create(&readerThreads[i]->thread, NULL, reader_thread, (void *) readerThreads[i]);
    }

    for (int i = 0; i < nwriters; ++i) {
        writerThreads[i] = malloc(sizeof(ThreadObj));
        writerThreads[i]->id = i + nreaders;
        writerThreads[i]->counts = counts;
        pthread_create(&writerThreads[i]->thread, NULL, writer_thread, (void *) writerThreads[i]);
    }

    sleep(1);
    set_start();

    // Sleep for some time then cancel all of the threads
    sleep(testTime);
    fprintf(stderr, "Finished sleeping\n");
    set_done();
    fprintf(stderr, "Stopping threads\n");

    for (int i = 0; i < nreaders; ++i) {
        pthread_join(readerThreads[i]->thread, NULL);
        fdebug("Reader %d joined", i);
        free(readerThreads[i]);
        sum += counts[i];
    }

    for (int i = 0; i < nwriters; ++i) {
        pthread_join(writerThreads[i]->thread, NULL);
        fdebug("Writer %d joined", i);
        free(writerThreads[i]);
        sum += counts[i + nreaders];
    }

    fprintf(stderr, "Test finished\n");

    rwlock_delete(&rwlock);

    fprintf(stderr,
        "--------------------------------------------------------------------------------\n");
    fprintf(stderr, "Thread Statistics:\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Reader Threads: %d\n", nreaders);

    int totalReaderCount = 0;

    for (int i = 0; i < nreaders; ++i) {
        fprintf(stderr, "%i: %i (%lf)\n", i, counts[i], (double) counts[i] / (double) sum);
        totalReaderCount += counts[i];
    }

    fprintf(stderr, "\n");
    fprintf(stderr, "Total Readers: %d\n", totalReaderCount);
    fprintf(stderr, "\n");
    fprintf(stderr, "Writer Threads: %d\n", nwriters);

    int totalWriterCount = 0;

    int j = 0;
    for (int i = 0; i < nwriters; ++i) {
        j = i + nreaders;
        fprintf(stderr, "%i: %i (%lf)\n", j, counts[j], (double) counts[j] / (double) sum);
        totalWriterCount += counts[j];
    }

    fprintf(stderr, "\n");
    fprintf(stderr, "Total Writers: %d\n", totalWriterCount);
    fprintf(stderr, "\n");

    double readerWriterRatio = 0;
    readerWriterRatio
        = (totalWriterCount > 0) ? (double) totalReaderCount / (double) totalWriterCount : INT_MAX;

    fprintf(stderr, "Reader/Writer ratio: (%f)\n", readerWriterRatio);

    if (priority == READERS) {
        if (!allreaders) {
            fprintf(stderr, "Not all readers were able to read at the same time\n");
            rc = rc | ANY_RD;
        }

        if (readerWriterRatio < nreaders) {
            fprintf(stderr, "Reader/Writer ratio is too small: %f\n", readerWriterRatio);
            rc = rc | TPUT;
        }
    }

    if (priority == WRITERS) {
        if (readerWriterRatio > nwriters) {
            fprintf(stderr, "Reader/Writer ratio is too large: %f\n", readerWriterRatio);
            rc = rc | TPUT;
        }
    }

    if (priority == N_WAY) {
        // Check if the reader/writer ratio is within 10% of n
        double threshold = 0.1;
        double readerWriterRatioDiff = 0;
        readerWriterRatioDiff
            = readerWriterRatio > n ? readerWriterRatio - n : n - readerWriterRatio;

        // Calculate the percentage difference between the reader/writer ratio and n
        double percentageDiff = 0;
        percentageDiff = readerWriterRatioDiff / n;

        if (percentageDiff > threshold) {
            if (totalReaderCount > totalWriterCount) {
                fprintf(stderr, "Reader/Writer ratio is too small: %f\n", readerWriterRatio);
                rc = rc | TPUT;
            } else {
                fprintf(stderr, "Reader/Writer ratio is too large: %f\n", readerWriterRatio);
                rc = rc | TPUT;
            }
        }
    }

    fprintf(stderr, "\n");

    // Check for any errors
    if (rc == 0) {
        fprintf(stderr, "No errors found\n");
    } else {
        if (rc & ANY_RD) {
            fprintf(stderr, "ANY_RD test failed\n");
        }

        if (rc & ONE_WR) {
            fprintf(stderr, "ONE_WR test failed\n");
        }

        if (rc & STRESS) {
            fprintf(stderr, "STRESS test failed\n");
        }

        if (rc & TPUT) {
            fprintf(stderr, "TPUT test failed\n");
        }
    }

    fprintf(stderr, "\n");
    fprintf(stderr, "Return Code: %d\n", rc);
    fprintf(stderr,
        "--------------------------------------------------------------------------------\n");
    return rc;
}
