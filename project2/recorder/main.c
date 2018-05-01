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
#include <X11/Xlib.h>

#include "../common/s4354198_structs.h"
#include "../common/s4354198_defines.h"
#include "../common/s4354198_utils.h"
#include "../common/s4354198_externs.h"
#include "../common/s4354198_cfs.h"

/* Function prototypes */
void await_comms(void);
void create_threads(void);
void lock_print_to_shell(const char* format, ...);
void *cag_output_handler(void* voidPtr);
void *shell_output_handler(void* voidPtr);
void *timer_handler(void* voidPtr);

/* Globals */
// Contains application data
Application* app;
// Cag output thread
pthread_t cagOutput;
// Shell output thread
pthread_t shellOutput;
// Timer thread
pthread_t timer;
// Semaphore for output to the shell
sem_t sendToShell;

int main(int argc, char** argv) {
    app = (Application*) malloc(sizeof(Application));
    app->cfs = (CFSInfo*) malloc(sizeof(CFSInfo));
    app->cfs->loaded = true;
    app->rState = STATE_NOTHING;
    app->upMilliseconds = 0;
    app->duration = -1;

    sem_init(&sendToShell, 0, 1);

    s4354198_read_args(argc, argv);

    await_comms();

    create_threads();

    // Stop doing work - wait for the FIFO thread to end
    pthread_join(cagOutput, NULL);
    pthread_join(shellOutput, NULL);
    
    return 0;
}

/**
 * Wait for comms to be created
 */
void await_comms(void) {
    app->comms = (Comms*) malloc(sizeof(Comms));
    
    while (access(FIFO_CAG_CR, F_OK) != 0) {
        ; // Wait for the FIFO to be created
    }
    app->comms->fromCag = fopen(FIFO_CAG_CR, "r");

    while (access(FIFO_SHELL_CR, F_OK) != 0) {
        ; // Wait for the FIFO to be created
    }
    app->comms->fromShell = fopen(FIFO_SHELL_CR, "r");

    mkfifo(FIFO_CR_SHELL, FIFO_CR_SHELL_PERMS);
    app->comms->toShell = fopen(FIFO_CR_SHELL, "w");
}

/**
 * Create the threads
 */
void create_threads(void) {
    pthread_create(&cagOutput, NULL, &cag_output_handler, NULL);
    pthread_create(&shellOutput, NULL, &shell_output_handler, NULL);
    pthread_create(&timer, NULL, &timer_handler, NULL);
}

/**
 * Handler for timer
 */
void* timer_handler(void* voidPtr) {
    while (1) {
        usleep(10000);

        if (app->rState == STATE_STARTED) {
            app->upMilliseconds += 10;
        } else if (app->rState == STATE_PAUSED) {
            // Do nothing, just hold the time
        }
    }
    return NULL;
}

/**
 * Handler for CAG output
 */
void* cag_output_handler(void* voidPtr) {
    bool stop = false;
    char* input = NULL;
    char* cleanedInput = NULL;
    char* endToken;
    int* data = (int*) malloc(sizeof(int*) * MAX_HEIGHT * MAX_WIDTH);
    size_t size;

    for (int i = 0; i < MAX_HEIGHT * MAX_WIDTH; i++) {
        data[i] = 0;
    }
    
    while (!stop) {
        int read = getline(&input, &size, app->comms->fromCag);

        if (read == -1) {
            stop = true;
        } else {
            // Remove the newline from the end
            cleanedInput = (char*) malloc(sizeof(char) * strlen(input));
            strncpy(cleanedInput, input, strlen(input) - 1);
            cleanedInput[strlen(input) - 1] = '\0';

            if (app->duration > 0 && app->upMilliseconds / 1000 >= app->duration) {
                lock_print_to_shell("%s\n", PR_CMD_DONE);
                lock_print_to_shell("Recording saved to %s and ran for %lums\n", 
                    app->prFile, app->upMilliseconds);
                app->rState = STATE_INIT;
                app->upMilliseconds = 0;
            } else if (app->rState == STATE_STARTED) {
                lock_print_to_shell("%lums: saving frame\n", app->upMilliseconds);

                for (int i = 0; i < MAX_HEIGHT; i++) {
                    for (int j = 0; j < MAX_WIDTH; j++) {
                        if (i < app->shellArgs->width && j < app->shellArgs->height) {
                            data[i * MAX_WIDTH + j] = strtol(strsep(&cleanedInput, ","), &endToken, 10);
                        } else {
                            data[i * MAX_WIDTH + j] = 0;
                        }
                    }
                }

                if (!s4354198_write_sector_to_file(app->prFile, data)) {
                    lock_print_to_shell("Ran out of space\n");
                    lock_print_to_shell("%s\n", PR_CMD_DONE);
                    lock_print_to_shell("Recording saved to %s and ran for %lums\n", 
                        app->prFile, app->upMilliseconds);
                    app->rState = STATE_INIT;
                    app->upMilliseconds = 0;
                }
            } else if (app->rState == STATE_PAUSED) {
                // Do nothing
            }

            free(cleanedInput);
        }
    }

    return NULL;
}

/**
 * Handler for shell output
 */
void* shell_output_handler(void* voidPtr) {
    bool stop = false;
    char* input = NULL;
    char* cleanedInput = NULL;
    char* clone = NULL;
    char* token = NULL;
    size_t size;

    while (!stop) {
        int read = getline(&input, &size, app->comms->fromShell);

        if (read == -1) {
            stop = true;
        } else {
            // Remove the newline from the end
            cleanedInput = (char*) malloc(sizeof(char) * strlen(input));
            strncpy(cleanedInput, input, strlen(input) - 1);
            cleanedInput[strlen(input) - 1] = '\0';

            clone = cleanedInput;

            token = strsep(&clone, " ");

            if (s4354198_str_match(token, PR_CMD_INIT)) {
                app->cfs->filename = strdup(clone);
                app->rState = STATE_INIT;
                app->upMilliseconds = 0;
            } else if (s4354198_str_match(token, PR_CMD_START)) {
                app->prFile = strdup(strsep(&clone, " "));
                app->duration = atoi(clone);
                app->rState = STATE_INIT;
            } else if (s4354198_str_match(token, PR_CMD_PAUSE)) {
                app->rState = STATE_PAUSED;
            } else if (s4354198_str_match(token, PR_CMD_RESUME)) {
                app->rState = STATE_STARTED;
            } else if (s4354198_str_match(token, PR_CMD_STOP)) {
                lock_print_to_shell("Recording saved to %s and ran for %lums\n", 
                    app->prFile, app->upMilliseconds);
                app->rState = STATE_INIT;
                app->upMilliseconds = 0;
            } else {
                lock_print_to_shell("Unknown command '%s'\n", token);
            }

            free(cleanedInput);
        }
    }

    return NULL;
}

/**
 * Semaphore locked access to the shell fifo
 */
void lock_print_to_shell(const char *format, ...) {
    va_list args;
    va_start(args, format);

    sem_wait(&sendToShell);
    vfprintf(app->comms->toShell, format, args);
    fflush(app->comms->toShell);
    sem_post(&sendToShell);
    
    va_end(args);
}