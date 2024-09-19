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
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include "handle_connection.h"
#define OPTIONS "t:"

//comment so that I have changes to update commit

int main(int argc, char **argv) {
    if (argc < 2) {
        file_error("Invalid Port");
    }

    //Process Command Line Stuff
    int opt = 0;
    int thread_count = 4;
    int port_number = 1443;
    while ((opt = getopt(argc, argv, OPTIONS)) != -1) {
        switch (opt) {
        case 't':
            thread_count = atoi(argv[optind - 1]);
            port_number = atoi(argv[optind]);

            break;
        }
    }
    //fprintf(stderr, "%d %d\n", thread_count, port_number);
    if (port_number < 1 || port_number > 65535) {
        file_error("Invalid Port");
    }
    //Open Connection
    Listener_Socket *sock = calloc(1, sizeof(Listener_Socket));
    if (listener_init(sock, port_number) == -1) {
        file_error("Invalid Port");
    }

    //Initalize shared resources and create threads
    queue_t *Request_Buffer = queue_new(thread_count);
    //rw_store *store = initalize_store(1000);
    pthread_mutex_t rw_access;
    pthread_mutex_init(&rw_access, NULL);
    Arguments *args = calloc(1, sizeof(Arguments));
    args->Requests = Request_Buffer;
    args->rwlock_access = rw_access;
    args->store = initalize_store(1000);
    pthread_t threads[thread_count];
    //args->audit_log = open("audit_log", O_WRONLY | O_CREAT, 0666);
    for (int i = 0; i < thread_count; i++) {
        pthread_create(&threads[i], NULL, handle_connection, args);
    }
    //reads until we are bing chillin
    while (1) {
        long socket = listener_accept(sock);
        if (socket > 0) { //established a connection
            queue_push(Request_Buffer, (void *) socket);
        }
    }
    return 0;
}
