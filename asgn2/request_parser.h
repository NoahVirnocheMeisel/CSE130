#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <regex.h>
#include <string.h>
#include <stdbool.h>
#include <regex.h>
#include <string.h>
#include <stdbool.h>
typedef struct {
    /** @brief Struct to dynamically increase the size of our buffer as we read a request
    
    */
    char *Method;
    char *URI;
    char *Version;
    char *content_length;
    int start_of_message;
    int are_the_header_fields_good;
    char *extra;
} Request;

int find_number_of_expressions(regmatch_t pmatch[]);

Request *split_request(char *buffer);

void close_request(Request **req);
