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
        return false;
    }
    return true;
}

//Internal_server_error. Not even sure how to implement this/what cases would cause it
//TODO: How to handle Inconsistent content-length with put
//Common approach - whatever i want.No test
//TODO: How to handle error code precedence
//400 error first

//If i can parse it-then next most important is 501
//After that 505
//Not gonna check for multiple errors.
//TODO: How to handle socket on closure of program-need to free? Is that possible?
//Need signal handler, not neccessary. Need to only handle indiviudal connection closing, not closing server.
//TODO: What cases might we have a 500 error? Any hints about what to consider?
//TODO: Connection error thing
//as long as success should be fine
void internal_server_error(int socket) {
    printf("This is an internal Server Error-Uh Oh :( %d", socket);
}

//Logic for performing a put_request. If statements in main check for the type of put request.
int perform_put(int socket, int fd, Request *req, int type) {

    if (!is_valid_version(req, socket)) {
        return -1;
    }
    //flush extra stuff to the file.
    char *hold_this = calloc(req->start_of_message, sizeof(char));
    read_n_bytes(socket, hold_this, req->start_of_message);
    fprintf(stderr, "CONTENT:%s\n", req->content_length);
    char *string_content_num
        = strndup(((req->content_length) + 16), strlen(req->content_length) - 16);
    int file_size = atoi(string_content_num);
    free(hold_this);
    free(string_content_num);
    pass_n_bytes(socket, fd, file_size);
    if (type == 0) {
        char *message = "HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nOK\n";
        write_n_bytes(socket, message, strlen(message));
        return 0;
    } else {
        char *message = "HTTP/1.1 201 Created\r\nContent-Length: 8\r\n\r\nCreated\n";
        write_n_bytes(socket, message, strlen(message));
        return 0;
    }
}

//Self contained logic for performing a get_request
int perform_get(int socket, Request *req) {
    char *hold_this_junk = calloc(req->start_of_message, sizeof(char));
    read_n_bytes(socket, hold_this_junk, req->start_of_message);
    free(hold_this_junk);
    int fd = open(req->URI, O_RDONLY);
    if (fd < 0) {
        //Check whether the file exists or if it is bad permissions
        if (access(req->URI, F_OK) == 0) {
            //our file_exists, so probably bad perms
            char *message = "HTTP/1.1 403 Forbidden\r\nContent-Length: 10\r\n\r\nForbidden\n";
            write_n_bytes(socket, message, strlen(message));
            return -1;
        } else {
            //this file doesn't exist, hit that client with a  classic 404 file not found kekw what loser they are requesting a file that doesn't exist
            char *message = "HTTP/1.1 404 Not Found\r\nContent-Length: 10\r\n\r\nNot Found\n";
            write_n_bytes(socket, message, strlen(message));
            return -1;
        }

    } else {
        if (!is_valid_version(req, socket)) {
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
            //our file_exists, so probably bad perms
            char *message_dir = "HTTP/1.1 403 Forbidden\r\nContent-Length: 10\r\n\r\nForbidden\n";
            write_n_bytes(socket, message_dir, strlen(message_dir));
            return -1;
        }

        //printf("File Size: %d\n",file_size);
        char *initial_message = calloc(60, sizeof(char));
        int result_of_making_message = snprintf(
            initial_message, 2048, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\n\r\n", file_size);
        if (result_of_making_message < 0) {
            internal_server_error(socket);
            return 0;
        }
        //char *buffer = calloc(10240,sizeof(char));
        write_n_bytes(socket, initial_message, strlen(initial_message));
        //printf("%s",initial_message);
        free(initial_message);
        int bytes_remaining = file_size;
        pass_n_bytes(fd, socket, bytes_remaining);
        close(fd);
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
    if (req->are_the_header_fields_good == -1
        || (strcmp(req->Method, "PUT") == 0
            && strcmp(req->content_length, "No-Content-Length") == 0)) {
        return false;
    }
    return true;
}

//for printing to stderr quickly, idk
void file_error(const char *error_message) {
    fprintf(stderr, "%s\n", error_message);
}
int main(int argc, char **argv) {
    if (argc < 2) {
        file_error("Invalid Port");
    }
    //Get our port_number from argv[1], and check that it's valid
    int port_number = atoi(argv[1]);
    if (port_number < 1 || port_number > 65535) {
        file_error("Invalid Port");
    }
    //printf("wtf is going on\n");
    //Initalize a Listener_Socket and bind it to our port_number
    //Listener_Socket *sock = calloc(1, sizeof(Listener_Socket));
    Listener_Socket *sock = calloc(1, sizeof(Listener_Socket));
    if (listener_init(sock, port_number) == -1) {
        file_error("Invalid Port");
    }
    //Main loop of the function. Socket will continously listen on provided port for new connections. If it detects one, peaks at message
    //Parses message, and then reads it again but ignores the message.
    while (1) {
        int socket = listener_accept(sock);
        if (socket > 0) { //established a connection
            char *buffer = calloc(2048, sizeof(char));
            recv(socket, buffer, 2048, MSG_PEEK);
            Request *req = split_request(buffer);
            fprintf(stderr, "%s\n", buffer);
            free(buffer);
            //Check for 400 error
            if (!valid_request(req)) {
                char *invalid_request_response
                    = "HTTP/1.1 400 Bad Request\r\nContent-Length: 12\r\n\r\nBad Request\n";
                write_n_bytes(socket, invalid_request_response, strlen(invalid_request_response));
                //send invalid_request respone
                //check for 505 error
            } else if (request_type(req) < 0) {
                char *Not_implemented = "HTTP/1.1 501 Not Implemented\r\nContent-Length: "
                                        "16\r\n\r\nNot Implemented\n";
                write_n_bytes(socket, Not_implemented, strlen(Not_implemented));

            } else if (!is_valid_version(req, socket)) {
                //handled inside function, we are just chillin here
            } else {
                //process get request
                if (request_type(req) == 0) {
                    perform_get(socket, req);
                    //process put request
                } else if (request_type(req) == 1) {

                    int fd = open(req->URI, O_WRONLY | O_TRUNC, 0666);

                    //200 OK Put request
                    if (fd > 0) {
                        perform_put(socket, fd, req, 0);
                        //201 Created Put request
                    } else {
                        close(fd);
                        //file doesn't exist, but that's okay we are gonna make it
                        fd = open(req->URI, O_WRONLY | O_CREAT, 0666);
                        if (fd > 0) {
                            perform_put(socket, fd, req, 1);
                            //Nope, bad news bears. Either a 403 or a 500. Probably a 403.
                        } else {
                            char *message
                                = "HTTP/1.1 403 Forbidden\r\nContent-Length: 10\r\n\r\nForbidden\n";
                            write_n_bytes(socket, message, strlen(message));
                        }
                    }

                    //Not Implemented-501 error
                } else {
                    char *Not_implemented = "HTTP/1.1 501 Not Implemented\r\nContent-Length: "
                                            "16\r\n\r\nNot Implemented\n";
                    write_n_bytes(socket, Not_implemented, strlen(Not_implemented));
                    //Request Not Implemeneted
                }
            }
            //close up our requests and such
            close_request(&req);
            cleanup(socket);
        }
    }
    //free(sock);
    return 0;
}
