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
#include "handle_connection.h"

void *handle_connection(void *argus) {
    void *socketid;
    Arguments *args = (Arguments *) argus;
    while (true) {
        //printf("Hello there\n");
        queue_pop(args->Requests, &socketid);
        long socket = (long) socketid;
        char *buffer = calloc(2048, sizeof(char));
        read_n_bytes(socket, buffer, 2048);
        //fprintf(stderr,"%s",buffer);
        Request *req = calloc(1, sizeof(Request));
        split_request(buffer, req);
        if (!valid_request(req)) {
            //fprintf(stderr, "AYE");
            char *invalid_request_response
                = "HTTP/1.1 400 Bad Request\r\nContent-Length: 12\r\n\r\nBad Request\n";
            write_n_bytes(socket, invalid_request_response, strlen(invalid_request_response));
            char *audit_invalid_request_message = calloc(100, sizeof(char));
            snprintf(audit_invalid_request_message, 2048, "%s,%s,400,%s\n", req->Method, req->URI,
                req->request_Id);
            fprintf(stderr, "%s", audit_invalid_request_message);
            //send invalid_request respone
            //check for 505 error
        } else if (!is_valid_version(req, socket)) {
            //handled inside function, we are just chillin here
        } else {
            if (request_type(req) == 0) {
                perform_get(socket, req, args);
            } else if (request_type(req) == 1) {
                perform_put(socket, req, args);
            } else {
                char *Not_implemented = "HTTP/1.1 501 Not Implemented\r\nContent-Length: "
                                        "16\r\n\r\nNot Implemented\n";
                write_n_bytes(socket, Not_implemented, strlen(Not_implemented));
                char *audit_Not_Implemented_message = calloc(100, sizeof(char));
                snprintf(audit_Not_Implemented_message, 2048, "%s,%s,,%s\n", req->Method, req->URI,
                    req->request_Id);
                fprintf(stderr, "%s", audit_Not_Implemented_message);
                //Request Not Implemeneted
            }
        }
        free(buffer);
        close_request(&req);
        cleanup(socket);
    }
    return NULL;
}
