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
 * Prints the specified message to stdout and flushes.
 */
void s4354198_p(const char *format, ...) {
    va_list args;
    va_start(args, format);

    vfprintf(stdout, format, args);
    fflush(stdout);
    
    va_end(args);
}

/**
 * Outputs the specified message as a non-proc message 
 */
void s4354198_o(const char *format, ...) {
    va_list args;
    va_start(args, format);

    fprintf(stdout, "$");
    vfprintf(stdout, format, args);
    fflush(stdout);
    
    va_end(args);
}

/**
 * Checks if two strings are equal
 */
bool s4354198_str_match(char *base, char *other) {
    if (base == NULL || other == NULL) {
        return false;
    }
    
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
 * Checks a string to see if it is completely whitespace
 */
bool s4354198_is_white_space(char* input) {
    bool result = true;

    for (int i = 0; i < strlen(input); i++) {
        if (input[i] != ' ') {
            result = false;
            break;
        }
    }

    return result;
}