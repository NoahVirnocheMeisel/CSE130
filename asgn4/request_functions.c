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
#include "request_functions.h"
#include <pthread.h>

void cleanup(int socket) {
    char *leftovers = calloc(1000, sizeof(char));
    while (read_n_bytes(socket, leftovers, 1000) != 0) {
        free(leftovers);
        leftovers = calloc(1000, sizeof(char));
    }
    close(socket);
}

//function that returns true if we are using HTTP/1.1, false otherwise
bool is_valid_version(Request *req, int socket) {
    if (strcmp(req->Version, "HTTP/1.1") != 0) {
        char *unsupported_version_response
            = "HTTP/1.1 505 Version Not Supported\r\nContent-Length: 22\r\n\r\nVersion Not "
              "Supported\n";
        write_n_bytes(socket, unsupported_version_response, strlen(unsupported_version_response));
        char *audit_invalid_version = calloc(100, sizeof(char));
        snprintf(
            audit_invalid_version, 2048, "%s,%s,505,%s\n", req->Method, req->URI, req->request_Id);
        fprintf(stderr, "%s", audit_invalid_version);
        return false;
    }
    return true;
}

void internal_server_error(int socket) {
    printf("This is an internal Server Error-Uh Oh :( %d", socket);
}

//Logic for performing a put_request. If statements in main check for the type of put request.
int perform_put(long socket, Request *req, Arguments *args) {

    //flush extra stuff to the file.
    // char *hold_this = calloc(req->start_of_message, sizeof(char));
    // read_n_bytes(socket, hold_this, req->start_of_message);
    //fprintf(stderr, "CONTENT:%s\n", req->content_length);
    char *string_content_num
        = strndup(((req->content_length) + 16), strlen(req->content_length) - 16);
    int file_size = atoi(string_content_num);
    //free(hold_this);
    free(string_content_num);
    int type = -1;
    pthread_mutex_lock(&(args->rwlock_access));
    rwlock_t *lock = get_uri_rwlock(args->store, req->URI);
    //fprintf(stderr,"%s\n",req->URI);
    //200 OK Put request
    writer_lock(lock);
    int fd = 0;
    if (access(req->URI, F_OK) == 0) {
        type = 0;
        fd = open(req->URI, O_WRONLY | O_TRUNC, 0666);
    } else {
        type = 1;
        fd = open(req->URI, O_WRONLY | O_CREAT, 0666);
    }
    pthread_mutex_unlock(&(args->rwlock_access));
    //Determine Type of Put Requst HERE
    write_n_bytes(fd, req->extra, strlen(req->extra));
    //fprintf(stderr,"START:%s:END\n",req->extra);
    pass_n_bytes(socket, fd, file_size);
    if (type == 0) {
        char *message = "HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nOK\n";
        write_n_bytes(socket, message, strlen(message));
        char *audit_ok_message = calloc(200, sizeof(char));
        snprintf(audit_ok_message, 2048, "%s,%s,200,%s\n", req->Method, req->URI, req->request_Id);
        fprintf(stderr, "%s", audit_ok_message);
        free(audit_ok_message);
        close(fd);
        writer_unlock(lock);
        return 0;
    } else {
        char *message = "HTTP/1.1 201 Created\r\nContent-Length: 8\r\n\r\nCreated\n";
        write_n_bytes(socket, message, strlen(message));
        char *audit_created_message = calloc(200, sizeof(char));
        snprintf(
            audit_created_message, 2048, "%s,%s,201,%s\n", req->Method, req->URI, req->request_Id);
        fprintf(stderr, "%s", audit_created_message);
        free(audit_created_message);
        close(fd);
        writer_unlock(lock);
        return 0;
    }
}

//Self contained logic for performing a get_request
int perform_get(long socket, Request *req, Arguments *args) {

    //char *hold_this_junk = calloc(req->start_of_message, sizeof(char));
    //read_n_bytes(socket, hold_this_junk, req->start_of_message);
    //free(hold_this_junk);
    pthread_mutex_lock(&(args->rwlock_access));
    //fprintf(stderr,"%s\n",req->URI);
    rwlock_t *lock = get_uri_rwlock(args->store, req->URI);
    reader_lock(lock);
    int fd = open(req->URI, O_RDONLY);
    pthread_mutex_unlock(&(args->rwlock_access));
    if (fd < 0) {
        //Check whether the file exists or if it is bad permissions
        if (access(req->URI, F_OK) == 0) {
            //our file_exists, so probably bad perms
            char *message = "HTTP/1.1 403 Forbidden\r\nContent-Length: 10\r\n\r\nForbidden\n";
            write_n_bytes(socket, message, strlen(message));
            close(fd);
            char *audit_Forbidden_message = calloc(200, sizeof(char));
            snprintf(audit_Forbidden_message, 2048, "%s,%s,403,%s\n", req->Method, req->URI,
                req->request_Id);
            fprintf(stderr, "%s", audit_Forbidden_message);
            free(audit_Forbidden_message);
            reader_unlock(lock);
            return -1;
        } else {
            //this file doesn't exist, hit that client with a  classic 404 file not found kekw what loser they are requesting a file that doesn't exist
            char *message = "HTTP/1.1 404 Not Found\r\nContent-Length: 10\r\n\r\nNot Found\n";
            write_n_bytes(socket, message, strlen(message));
            close(fd);
            char *audit_Not_Found_message = calloc(200, sizeof(char));
            snprintf(audit_Not_Found_message, 2048, "%s,%s,404,%s\n", req->Method, req->URI,
                req->request_Id);
            fprintf(stderr, "%s", audit_Not_Found_message);
            free(audit_Not_Found_message);
            reader_unlock(lock);
            return -1;
        }

    } else {
        if (!is_valid_version(req, socket)) {
            reader_unlock(lock);
            return -1;
        }
        //using stat to find the size of the file, before I start reading it(for content-length)
        struct stat file_info;
        int file_size = 0;
        if (stat(req->URI, &file_info) == -1) {
            file_size = 0;
        } else {
            file_size = file_info.st_size;
        }
        //check if our file is a directory before proceeding
        if (S_ISDIR(file_info.st_mode)) {
            //our file_exists, so probably bad perms or directory in this case
            char *message_dir = "HTTP/1.1 403 Forbidden\r\nContent-Length: 10\r\n\r\nForbidden\n";
            write_n_bytes(socket, message_dir, strlen(message_dir));
            close(fd);
            char *audit_Forbidden_message = calloc(200, sizeof(char));
            snprintf(audit_Forbidden_message, 2048, "%s,%s,403,%s\n", req->Method, req->URI,
                req->request_Id);
            fprintf(stderr, "%s", audit_Forbidden_message);
            free(audit_Forbidden_message);
            reader_unlock(lock);
            return -1;
        }
        char *initial_message = calloc(60, sizeof(char));
        snprintf(initial_message, 2048, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\n\r\n", file_size);
        // if (result_of_making_message < 0) {
        //     internal_server_error(socket);
        //     return 0;
        // }
        //char *buffer = calloc(10240,sizeof(char));
        write_n_bytes(socket, initial_message, strlen(initial_message));
        //printf("%s",initial_message);
        free(initial_message);
        //int bytes_remaining = file_size;
        //fprintf(stderr,"%d\n",file_size);
        pass_n_bytes(fd, socket, file_size);
        char *audit_Ok_message = calloc(200, sizeof(char));
        snprintf(audit_Ok_message, 2048, "%s,%s,200,%s\n", req->Method, req->URI, req->request_Id);
        fprintf(stderr, "%s", audit_Ok_message);
        free(audit_Ok_message);
        close(fd);
        reader_unlock(lock);
        //write contents to file
    }
    return -2433;
}

//checks if the Method is Get-0 or Put-1, -1 otherwise
int request_type(Request *req) {
    if (strcmp(req->Method, "GET") == 0) {
        return 0;
    } else if (strcmp(req->Method, "PUT") == 0) {
        return 1;
    }
    return -1;
}

//checks Request struct field to determine if request is valid or not.
bool valid_request(Request *req) {
    if (req->are_the_header_fields_good == -1 || strcmp(req->Method, "ERROR") == 0) {
        return false;
    }
    return true;
}

//for printing to stderr quickly, idk
void file_error(const char *error_message) {
    fprintf(stderr, "%s\n", error_message);
}
