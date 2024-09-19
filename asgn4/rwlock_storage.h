#include "asgn2_helper_funcs.h"
#include "rwlock.h"

typedef struct {
    rwlock_t *lock;
    char *URI;
} KVPair;

typedef struct {
    KVPair **elements;
    int size;
    int max_size;
} rw_store;

rw_store *initalize_store(int max_size);

rwlock_t *get_uri_rwlock(rw_store *s, char *URI);
