#include "rwlock_storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <regex.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>
#include "asgn2_helper_funcs.h"
#include "request_parser.h"
#include <sys/stat.h>
#include "rwlock.h"
//create rwlock_store object

rw_store *initalize_store(int max_size) {
    rw_store *store = calloc(1, sizeof(rw_store));
    store->max_size = max_size;
    store->size = 0;
    store->elements = calloc(max_size, sizeof(KVPair *));
    return store;
}

/*
typedef struct {
    rwlock_t* lock;
    char* URI;
} KVPair;

*/

rwlock_t *get_uri_rwlock(rw_store *s, char *URI) {
    for (int i = 0; i < s->size; i++) {
        //fprintf(stderr,"%s\n",)
        if (strcmp(((s->elements)[i])->URI, URI) == 0) {
            return (((s->elements)[i])->lock);
        }
    }
    //URI Not in array, attempt to add
    if (s->size >= s->max_size) {
        return NULL;
    }
    KVPair *new_kv = calloc(1, sizeof(KVPair));
    new_kv->URI = strndup(URI, strlen(URI));
    new_kv->lock = rwlock_new(N_WAY, 1);
    s->elements[s->size] = new_kv;
    (s->size)++;
    return (new_kv->lock);
}
