#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "request_parser.h"

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
} Requst;

int find_number_of_expressions(regmatch_t pmatch[]) {
    int number_of_expression = 0;
    int index = 1;
    while (true) {
        if (pmatch[index].rm_eo != -1) {
            number_of_expression++;
            index++;
        } else {
            break;
        }
    }
    return number_of_expression;
}

Request *split_request(char *buffer) {
    regex_t regex;
    char *re = "^([A-Z]{1,8}) *(/[a-zA-Z0-9.-]{1,63}) *(HTTP/[0-9]\\.[0-9])\r\n(.*\r\n)*\r\n(.*)*$";
    //char *extra_bytes = "^([a-zA-Z0-9.-]{1,128}: [\\x01-\\x20]{1,128}\r\n)*\r\n(.*)*$"
    //TODO: REMOVE STUFF AFTER THAT LAST \r\n.
    //regex_t     regex_to_get_extra_bytes;
    //regcomp(&regex_to_get_extra_bytes,extra_bytes,REG_EXTENDED | REG_NEWLINE);
    regcomp(&regex, re, REG_EXTENDED | REG_NEWLINE);
    size_t nmatch_o = 7;
    regmatch_t pmatch[7];
    int i = regexec(&regex, buffer, nmatch_o, pmatch, 0);
    Request *req = calloc(1, sizeof(Request));
    if (i) {
        req->Method = "ERROR";
        req->URI = "ERROR";
        req->Version = "ERROR";
        req->content_length = "ERROR";
        req->are_the_header_fields_good = -1;
        regfree(&regex);
        return req;
    }

    int num = find_number_of_expressions(pmatch);
    //printf("%d\n",num);
    req->Method = strndup(&buffer[pmatch[1].rm_so], pmatch[1].rm_eo - pmatch[1].rm_so);
    req->URI = strndup(&buffer[pmatch[2].rm_so] + 1, pmatch[2].rm_eo - pmatch[2].rm_so - 1);
    req->Version = strndup(&buffer[pmatch[3].rm_so], pmatch[3].rm_eo - pmatch[3].rm_so);
    req->content_length = "No-Content-Length";
    req->extra = "";
    if (num > 4) {
        req->start_of_message = pmatch[5].rm_so;
        //req->extra = strndup(&buffer[pmatch[5].rm_so], 2048);
        //int make_it_good = regexec(&regex_to_get_extra_bytes)
    }
    if (num > 3) {
        //string is contents starting from two after the Version(to account for \r\n) all the way to the start of our final expression -2
        char *temp_buffer
            = strndup(&buffer[pmatch[3].rm_eo] + 2, (pmatch[4].rm_eo) - (pmatch[3].rm_eo));
        //printf("Stuff:%sEND",temp_buffer);
        regfree(&regex);
        int nmatch = 3;
        regex_t parse_regex;
        regex_t content_regex;
        regmatch_t new_pmatch[3];
        regmatch_t content_pmatch[2];
        char *header_fields_parse = "^([a-zA-Z0-9.-]{1,128}: [\\x20-\\x7E]{1,128}\r\n)(.*)*\r\n$";
        char *content_parse = "^(Content-Length: [0-9]{1,128})\r\n$";
        regcomp(&parse_regex, header_fields_parse, REG_EXTENDED | REG_NEWLINE);
        regcomp(&content_regex, content_parse, REG_EXTENDED | REG_NEWLINE);
        while (true) {
            int header_in_request = regexec(&parse_regex, temp_buffer, nmatch, new_pmatch, 0);
            // printf("%s",temp_buffer);
            // printf("%d\n",header_in_request);
            if (!header_in_request) {
                //printf("%d %d\n",new_pmatch[1].rm_so,new_pmatch[1].rm_eo);
                char *header_field = strndup(
                    &temp_buffer[new_pmatch[1].rm_so], new_pmatch[1].rm_eo - new_pmatch[1].rm_so);
                //printf("FIELD:%sFIELDEND",header_field);
                char *catch = calloc(600, sizeof(char));
                char *stuff_before = strndup(temp_buffer, new_pmatch[1].rm_so);
                char *stuff_after = strndup(
                    &temp_buffer[new_pmatch[1].rm_eo], (new_pmatch[0].rm_eo) - new_pmatch[1].rm_eo);
                strcat(catch, stuff_before);
                strcat(catch, stuff_after);
                free(stuff_before);
                free(stuff_after);
                //char *catch = strndup(&temp_buffer[new_pmatch[1].rm_so],new_pmatch[1].rm_eo-new_pmatch[1].rm_so);
                free(temp_buffer);
                temp_buffer = catch;
                int content_check = regexec(&content_regex, header_field, 2, content_pmatch, 0);
                if (!content_check) {
                    req->content_length = strndup(&header_field[content_pmatch[1].rm_so],
                        content_pmatch[1].rm_eo - content_pmatch[1].rm_so);
                }
                free(header_field);

            } else {
                int are_the_header_fields_good = 0;
                if (strlen(temp_buffer) > 2) {
                    req->are_the_header_fields_good = are_the_header_fields_good;
                    break;
                }
                are_the_header_fields_good = 1;
                req->are_the_header_fields_good = are_the_header_fields_good;
                break;
            }
        }
        regfree(&content_regex);
        regfree(&parse_regex);
        free(temp_buffer);
    } else {
        regfree(&regex);
        req->are_the_header_fields_good = 1;
    }

    return req;
}

void close_request(Request **req) {
    if (req != NULL && *req != NULL) {
        if (strcmp((*req)->Version, "ERROR") != 0) {
            free((*req)->Method);
            free((*req)->URI);
            free((*req)->Version);

            if (strcmp((*req)->content_length, "No-Content-Length") != 0) {
                free((*req)->content_length);
            }
        }
        free(*req);
        req = NULL;
    }
}
