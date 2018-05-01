#ifndef UTILS_H
#define UTILS_H

#include <stdarg.h>
#include <semaphore.h>

/* Globals */
// Holds the global input buffer
char* globalInput;

/* Function prototypes */
void s4354198_exit(int code, const char *format, ...);
bool s4354198_str_match(char *base, char *other);
char* s4354198_getline(FILE *stream, int* totalSize, sem_t* lock);
void s4354198_read_args(int argc, char** argv);

#endif