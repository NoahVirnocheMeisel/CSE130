#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

//general purpose error_message printer
void file_error(const char *error_message) {
    printf("%s\n", error_message);
    exit(1);
}

//function to guarantee that we will write n bytes from a buffer to a file.
int write_n_bytes(int fd, char *buffer, int number_of_bytes_to_write) {
    int p = 0;
    while (number_of_bytes_to_write > 0) {
        int bytes_written = write(fd, buffer + p, number_of_bytes_to_write);
        //error in writing
        if (bytes_written == -1) {
            free(buffer);
            close(fd);
            file_error("Unable to Write File");
        }

        if (number_of_bytes_to_write <= 0) {
            break;
        }
        //decrement remaining bytes, and incrememnt the buffer by the same amount.
        number_of_bytes_to_write -= bytes_written;
        p += bytes_written;
    }
    return p;
}

//function to guarantee that we will read n byte from a file to a buffer
int read_n_bytes(int fd, char *buffer, int number_of_bytes_to_read) {
    int p = 0;
    while (number_of_bytes_to_read > 0) {
        int bytes_read = read(fd, buffer + p, number_of_bytes_to_read);
        //check if valid_file_read
        if (bytes_read == -1) {
            free(buffer);
            close(fd);
            file_error("Unable to read file");
        }

        if (bytes_read == 0)
            break;

        number_of_bytes_to_read -= bytes_read;
        p += bytes_read;
    }
    return p;
}

//reads in 4 bytes, and then returns an int to be used in our array of function in main. The int is 0 if command is set\n, 1 if get\n
int get_command(void) {
    char *buffer = calloc(5, sizeof(char));
    read_n_bytes(0, buffer, 4);
    if (strcmp(buffer, "set\n") == 0) {
        free(buffer);
        return 0;
    } else if (strcmp(buffer, "get\n") == 0) {
        free(buffer);
        return 1;
    }
    write_n_bytes(2, "Invalid Command\n", 16);
    free(buffer);
    exit(1);
}

//reads in bytes from stdin(using read_n_bytes) until we hit a newline or stdin is close
int read_bytes(char *buffer) {
    int off_set = 0;
    while (read_n_bytes(0, buffer + off_set, 1) == 1) {
        if (buffer[off_set] == '\n') {
            buffer[off_set] = 0;
            return strlen(buffer);
        }
        off_set += 1;
    }
    return 0;
}

//reads to newline from stdin, stores in a buffer. opens what it reads as a file and returns the file descriptor
int get_file(int mode) {
    char *buffer = calloc(PATH_MAX + 1, sizeof(char));
    int file_bytes = read_bytes(buffer);

    if (strlen(buffer) < 1 || strlen(buffer) > PATH_MAX || file_bytes == 0) {
        write_n_bytes(2, "Invalid Command\n", 16);
        free(buffer);
        exit(1);
    }

    //test for directory, found the condition(which uses the stat function to a create a stat struct from our file path, and then checks if its a directory)
    struct stat sb;
    //This conditional idea  was found in this stackoverflow thread that I discoverd while looking into directory vs file
    //https://stackoverflow.com/questions/4553012/checking-if-a-file-is-a-directory-or-just-a-file
    if (stat(buffer, &sb) == 0 && S_ISDIR(sb.st_mode)) {
        write_n_bytes(2, "Invalid Command\n", 16);
        free(buffer);
        exit(1);
    }

    if (mode == 0) {
        int fd = open(buffer, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        free(buffer);
        return fd;

    } else if (mode == 1) {
        //get
        int fd = open(buffer, O_RDONLY);
        if (fd < 0) {
            write_n_bytes(2, "Invalid Command\n", 16);
            free(buffer);
            exit(1);
        }
        free(buffer);
        return fd;
    }
    free(buffer);
    file_error("This is awkward, we reached the end of get_file");
    return -4;
}

//set component of our memory.c function. Takes a file descriptor and sets the contents of that file to be n bytes from stdin
//n bytes is read first, and then at newline starts writing to file until we exceed n bytes or hit EOF
int set(int file_descriptor) {
    //get number of bytes to write
    char *buffer = (char *) calloc(4096, sizeof(char));
    read_bytes(buffer);
    int bytes_desired = atoi(buffer);
    free(buffer);

    while (bytes_desired > 0) {
        int bytes_too_write = 0;
        char *buffer = (char *) calloc(4096, sizeof(char));
        int bytes_read = read_n_bytes(0, buffer, 4096);
        if (bytes_desired > bytes_read) {
            bytes_too_write = bytes_read;
        } else {
            bytes_too_write = bytes_desired;
            bytes_desired = 0;
        }
        write_n_bytes(file_descriptor, buffer, bytes_too_write);
        free(buffer);
        if (bytes_read == 0) {
            break;
        }
    }
    close(file_descriptor);
    write_n_bytes(1, "OK\n", 3);
    return 0;
}

//prints the contents of a file descriptor
int get(int file_descriptor) {
    //Check if stdin still has input
    char *buffer = calloc(10, sizeof(char));
    int checking_for_extra = read_n_bytes(0, buffer, 10);
    if (checking_for_extra != 0) {
        write_n_bytes(2, "Invalid Command\n", 16);
        free(buffer);
        close(file_descriptor);
        exit(1);
    }
    free(buffer);
    while (1) {
        char *buffer = calloc(4096, sizeof(char));
        int bytes_read = read_n_bytes(file_descriptor, buffer, 4096);
        //printf("%lu\n",strlen(buffer));
        //printf("%d\n",bytes_read);
        write_n_bytes(1, buffer, bytes_read);
        free(buffer);
        if (bytes_read == 0) {
            break;
        }
    }
    close(file_descriptor);
    return 0;
}

//main function. Basically just calling all of the functions implemented above.
int main(void) {

    // 0 is set, 1 is get

    //get our command
    int mode = get_command();
    //return a file descriptor
    //get our file descriptor
    int fd = get_file(mode);
    //execute command on our file
    if (fd > 2) {
        if (mode) {
            get(fd);
        } else if (mode == 0) {
            set(fd);
        }
    }
    return 0;
}
