//Functions to perform miscellaneous tasks in the HTTPSERVER
#include "rwlock.h"
#include "queue.h"
#include "rwlock_storage.h"

typedef struct {
    queue_t *Requests;
    pthread_mutex_t rwlock_access;
    rw_store *store;
    //int audit_log;
} Arguments;

void cleanup(int socket);

bool is_valid_version(Request *req, int socket);

void internal_server_error(int socket);

int perform_put(long socket, Request *req, Arguments *args);

int perform_get(long socket, Request *req, Arguments *args);

int request_type(Request *req);

bool valid_request(Request *req);

void file_error(const char *error_message);
