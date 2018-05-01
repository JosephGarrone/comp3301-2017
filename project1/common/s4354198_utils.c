#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <ctype.h>
#include <signal.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include <termios.h>
#include <semaphore.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "s4354198_utils.h"
#include "s4354198_defines.h"
#include "s4354198_structs.h"

/* Externs */
extern Application* app;

/**
 * Prints the specified message to stderr and then exits with 
 * the specified code.
 */
void s4354198_exit(int code, const char *format, ...) {
    va_list args;
    va_start(args, format);

    vfprintf(stderr, format, args);
    
    va_end(args);

    exit(code);
}

/**
 * Checks if two strings are equal
 */
bool s4354198_str_match(char *base, char *other) {
    if (strlen(base) != strlen(other)) {
        return false;
    }

    for (int i = 0; i < strlen(other); i++) {
        if (base[i] != other[i]) {
            return false;
        }
    }
    
    return true;
}

/**
 * Reads a line from the given stream and echos as necessary
 */ 
char* s4354198_getline(FILE *stream, int* totalSize, sem_t* lock) {
    int size = BUFFER_SIZE;
    int read = 0;
    char chr;

    globalInput = (char*) malloc(sizeof(char) * size);
    globalInput[0] = '\0';
    char* buffer = globalInput;

    while (true) {
        chr = fgetc(stream);

        if (lock != NULL) {
            sem_wait(lock);
        }

        if (chr == EOF || chr == '\n') {
            printf("%c", chr);
            fflush(stdout);
            sem_post(lock);
            break;
        }

        printf("%c", chr);
        fflush(stdout);

        if (chr == DEL && read > 0) {
            buffer[read--] = '\0';
            printf("%s", BACKSPACE);
            fflush(stdout);
            if (lock != NULL) {
                sem_post(lock);
            }
            continue;
        }

        if (read == size - 2) {
            size *= 2;
            buffer = (char*) realloc(buffer, size);
        }
        buffer[read++] = chr;
        buffer[read] = '\0';
        if (lock != NULL) {
            sem_post(lock);
        }
    }

    buffer[read] = '\0';

    *totalSize = read;
    
    if (chr == EOF) {
        *totalSize = -1;
    }
    
    return buffer;
}

/**
 * Attempts to read command line arguments and errors as required
 * */
void s4354198_read_args(int argc, char** argv) {
    int chr;
    char* endToken;

    opterr = 0;

    app->shellArgs = (ShellArgs*) malloc(sizeof(ShellArgs));

    while ((chr = getopt(argc, argv, "w:h:r:")) != -1) {
        switch (chr) {
            case 'w':
                app->shellArgs->width = strtol(optarg, &endToken, 10);
                break;
            case 'h':
                app->shellArgs->height = strtol(optarg, &endToken, 10);
                break;
            case 'r':
                app->shellArgs->refreshRate = strtol(optarg, &endToken, 10);
                break;
            case '?':
                if (isprint(optopt)) {
                    fprintf(stderr, "Unknown option '-%c'.\n", optopt);
                } else {
                    fprintf(stderr, "Unknown option character '\\x%x'.\n", optopt);
                }
                break;
            default:
                abort();
                break;
        }
    }
    
    if (app->shellArgs->width > MAX_WIDTH || app->shellArgs->height < MIN_WIDTH) {
        s4354198_exit(1, "Invalid width (%d) specified. Must be >= %d and <= %d.\n", 
            app->shellArgs->width, MIN_WIDTH, MAX_WIDTH);
    }

    if (app->shellArgs->height > MAX_HEIGHT || app->shellArgs->height < MIN_HEIGHT) {
        s4354198_exit(1, "Invalid height (%d) specified. Must be >= %d and <= %d.\n", 
            app->shellArgs->height, MIN_HEIGHT, MAX_HEIGHT);
    }

    if (app->shellArgs->refreshRate > MAX_REFRESH || app->shellArgs->refreshRate < MIN_REFRESH) {
        s4354198_exit(1, "Invalid refresh rate (%d ms) specified. Must be >= %d ms and"
            " <= %d ms.\n", app->shellArgs->refreshRate, MIN_REFRESH, MAX_REFRESH);
    }
}