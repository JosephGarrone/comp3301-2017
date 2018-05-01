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
void lock_print_to_display(const char* format, ...);
void *shell_output_handler(void* voidPtr);
void *timer_handler(void* voidPtr);
void *playback_handler(void* voidPtr);
void send_to_display(int* data);

/* Globals */
// Contains application data
Application* app;
// Shell output thread
pthread_t shellOutput;
// Timer thread
pthread_t timer;
// Playback thread
pthread_t playback;
// Semaphore for output to the shell
sem_t sendToShell;
// Semaphore for output to the display
sem_t sendToDisplay;

int main(int argc, char** argv) {
    app = (Application*) malloc(sizeof(Application));
    app->cfs = (CFSInfo*) malloc(sizeof(CFSInfo));
    app->cfs->loaded = true;
    app->pState = STATE_NOTHING;
    app->upMilliseconds = 0;

    sem_init(&sendToShell, 0, 1);
    sem_init(&sendToDisplay, 0, 1);

    s4354198_read_args(argc, argv);

    await_comms();

    create_threads();

    // Stop doing work - wait for the FIFO thread to end
    pthread_join(shellOutput, NULL);
    
    return 0;
}

/**
 * Wait for comms to be created
 */
void await_comms(void) {
    app->comms = (Comms*) malloc(sizeof(Comms));
    
    while (access(FIFO_SHELL_CP, F_OK) != 0) {
        ; // Wait for the FIFO to be created
    }
    app->comms->fromShell = fopen(FIFO_SHELL_CP, "r");

    mkfifo(FIFO_CP_SHELL, FIFO_CP_SHELL_PERMS);
    app->comms->toShell = fopen(FIFO_CP_SHELL, "w");
    
    mkfifo(FIFO_CP_DISPLAY, FIFO_CP_DISPLAY_PERMS);
    app->comms->toDisplay = fopen(FIFO_CP_DISPLAY, "w");
}

/**
 * Create the threads
 */
void create_threads(void) {
    pthread_create(&shellOutput, NULL, &shell_output_handler, NULL);
    pthread_create(&timer, NULL, &timer_handler, NULL);
    pthread_create(&playback, NULL, &playback_handler, NULL);
}

/**
 * Handler for timer
 */
void* timer_handler(void* voidPtr) {
    while (1) {
        usleep(10000);

        if (app->pState == STATE_STARTED) {
            app->upMilliseconds += 10;
        } else if (app->pState == STATE_PAUSED) {
            // Do nothing, just hold the time
        }
    }
    return NULL;
}

/**
 * Sends a frame to the screen
 */
void send_to_display(int* data) {
    char* messages[app->shellArgs->height];
    int totalSize = 0;

    for (int i = 0; i < app->shellArgs->height; i++) {
        int size = BUFFER_SIZE;
        int index = 0;
        char* message = (char*) malloc(sizeof(char) * size);

        for (int j = 0; j < app->shellArgs->width; j++) {
            index += sprintf(&message[index], "%d", data[i * MAX_WIDTH + j]);

            if (j < app->shellArgs->width - 1) {
                index += sprintf(&message[index], ",");
            }

            // Account for \n and \0
            if (index == size - 2) {
                size *= 2;
                message = (char*) realloc(message, size);
            }
        }

        messages[i] = message;        
        totalSize += strlen(message);
    }

    // Account for NUL terminator
    totalSize++;
    char finalMessage[totalSize + (app->shellArgs->height)];
    int index = 0;
    for (int i = 0; i < app->shellArgs->height; i++) {
        index += sprintf(&finalMessage[index], "%s", messages[i]);

        if (i < app->shellArgs->height - 1) {
            index += sprintf(&finalMessage[index], ",");
        }

        free(messages[i]);
    }

    lock_print_to_display("%s\n", finalMessage);
}

/**
 * Handler for timer
 */
void* playback_handler(void* voidPtr) {
    while (1) {
        if (app->pState == STATE_STARTED) {
            // Play the next frame
            if (app->frame < app->maxFrame) {
                lock_print_to_shell("Playing frame %d\n", app->frame);

                char* temp = strdup(app->prFile);
                int* data = s4354198_get_file_frame_from_filename(temp, app->frame + 1);

                send_to_display(data);

                free(data);
                free(temp);

                app->frame++;
            } else {
                lock_print_to_shell("%s\n", PR_CMD_DONE);
                lock_print_to_shell("Finished playing %s and ran for %lums\n", 
                    app->prFile, app->upMilliseconds);
                app->pState = STATE_INIT;
                app->upMilliseconds = 0;
                app->frame = 0;
            }
        } else if (app->pState == STATE_PAUSED) {
            // Do nothing, just hold the time
        }

        usleep(app->shellArgs->refreshRate * 1000);
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
                app->pState = STATE_INIT;
                app->upMilliseconds = 0;
            } else if (s4354198_str_match(token, PR_CMD_START)) {
                app->prFile = strdup(strsep(&clone, " "));
                app->frame = 0;
                char* temp = strdup(app->prFile);
                app->maxFrame = s4354198_get_file_sector_count_from_filename(temp);
                free(temp);
                app->pState = STATE_INIT;
            } else if (s4354198_str_match(token, PR_CMD_PAUSE)) {
                app->pState = STATE_PAUSED;
            } else if (s4354198_str_match(token, PR_CMD_RESUME)) {
                app->pState = STATE_STARTED;
            } else if (s4354198_str_match(token, PR_CMD_STOP)) {
                lock_print_to_shell("Finished playing %s and ran for %lums\n", 
                    app->prFile, app->upMilliseconds);
                app->pState = STATE_INIT;
                app->upMilliseconds = 0;
                app->frame = 0;
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

/**
 * Semaphore locked access to the display fifo
 */
void lock_print_to_display(const char *format, ...) {
    va_list args;
    va_start(args, format);

    sem_wait(&sendToDisplay);
    vfprintf(app->comms->toDisplay, format, args);
    fflush(app->comms->toDisplay);
    sem_post(&sendToDisplay);
    
    va_end(args);
}