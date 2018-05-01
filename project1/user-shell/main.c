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

/* Function prototypes */
void graceful_exit(void);
void register_signal_handlers(void);
void exit_prog(char* message);
void create_semaphores(void);
void restore_stdin_buffering(void);
void make_stdin_unbuffered(void);
void prepare_for_draw_processes(void);
void start_threads(void);
void start_other_processes(void);
void start_cag(void);
void start_display(void);
void stop_all_threads(void);
void stop_all_processes(void);
void lock_print(const char* format, ...);
void prompt(void);
void restore_prompt(void);
void await_input(void);
void handle_user_input(char* input);
void create_drawing_process(LifeType lifeType, FormType formType, int x, int y);
void notify_cag_new_lifeform(DrawingProcess* process);
void remove_drawing_process(DrawingProcess* process);
bool get_form_type(FormType* formType, char** input);
bool get_form_coord(int* coord, char** input);
bool handle_draw(char* input, LifeType lifeType);
void display_help(void);
void kill_drawing(int id);
void handle_life_form(void);
void* report_alive(void* voidPtr);
void* count_milliseconds(void* voidPtr);
void* drawing_process_out_handler(void* voidPtr);
void* engine_out_handler(void* ptr);

/* Globals */
// Contains application data
Application* app;
// Thread for monitoring drawing processes
pthread_t drawingProcessOutput;
// Thread for engine output
pthread_t engineOutput;
// Terminal settings old
struct termios oldTio;
// Terminal settings new
struct termios newTio;
// Output based semaphore
sem_t outputAllowed;
// Semaphore for comms to the cag
sem_t commsToCag;

/* Externs */
extern char* formToString[];
extern char* lifeToString[];
extern char* globalInput;

int main(int argc, char** argv) {
    app = (Application*) malloc(sizeof(Application));
    app->lastId = 0;
    app->drawProcesses = NULL;

    register_signal_handlers();

    s4354198_read_args(argc, argv);

    make_stdin_unbuffered();

    create_semaphores();

    start_other_processes();

    prepare_for_draw_processes();
    
    start_threads();    

    await_input();

    stop_all_processes();

    restore_stdin_buffering();

    return 0;
}

/**
 * Gracefully stop the program
 */
void graceful_exit(void) {
    stop_all_threads();

    restore_prompt();
    
    stop_all_processes();

    restore_stdin_buffering();

    exit(0);
}

/**
 * Handler for the SIGINT signal (Ctrl + C)
 */
void sigint_handler(int signal) {
    graceful_exit();
}

/**
 * Register all the required signal handlers
 */
void register_signal_handlers(void) {
    signal(SIGINT, sigint_handler);
}

/**
 * Semaphore locked access to stdout
 */
void lock_print(const char *format, ...) {
    va_list args;
    va_start(args, format);

    sem_wait(&outputAllowed);
    vfprintf(stdout, format, args);
    fflush(stdout);
    sem_post(&outputAllowed);
    
    va_end(args);
}

/**
 * Create all the semaphores
 */ 
void create_semaphores(void) {
    sem_init(&outputAllowed, 0, 1);
    sem_init(&commsToCag, 0, 1);
}

/**
 * Starts all the other threads needed
 */
void start_threads(void) {
    pthread_create(&drawingProcessOutput, NULL, &drawing_process_out_handler, NULL);
    pthread_create(&engineOutput, NULL, &engine_out_handler, NULL);
}

/**
 * Stops all the threads
 */
void stop_all_threads(void) {
    void* returnValue;
    pthread_cancel(drawingProcessOutput);
    pthread_join(drawingProcessOutput, &returnValue);
}

/**
 * Makes stdin unbuffered
 */
void make_stdin_unbuffered(void) {
    tcgetattr(STDIN_FILENO, &oldTio);
    
    newTio = oldTio;

    newTio.c_lflag &=(~ICANON & ~ECHO);

    tcsetattr(STDIN_FILENO, TCSANOW, &newTio);
}

/**
 * Makes stdin buffered again
 */ 
void restore_stdin_buffering(void) {
    tcsetattr(STDIN_FILENO, TCSANOW, &oldTio);
}

/**
 * Creates a pipe for draw process communications
 */
void prepare_for_draw_processes(void) {
    int processIn[2];
    int processOut[2];

    if (pipe(processIn) || pipe(processOut)) {
        s4354198_exit(1, "Failed to create draw process pipes.\n");
    }

    app->comms->toDrawing = fdopen(processIn[SOCKET_WRITE], "w");
    app->comms->fromDrawing = fdopen(processOut[SOCKET_READ], "r");
    app->comms->toShellDescriptor = processOut[SOCKET_WRITE];
    app->comms->fromShellDescriptor = processIn[SOCKET_READ];
}

/**
 * Starts all other required processes
 */
void start_other_processes(void) {
    app->runtimeInfo = (RuntimeInfo*) malloc(sizeof(RuntimeInfo));
    app->comms = (Comms*) malloc(sizeof(Comms));

    app->runtimeInfo->userShell = getpid();

    start_cag();
    start_display();
}

/**
 * Forks and starts the cag process
 */
void start_cag(void) {
    pid_t pid = fork();

    if (pid == -1) {
        // Failed to fork
        s4354198_exit(1, "Error creating cag process.\n");
    } else if (pid > 0) {
        app->runtimeInfo->cag = pid;
        
        mkfifo(FIFO_SHELL_CAG, FIFO_SHELL_CAG_PERMS);
        app->comms->toCag = fopen(FIFO_SHELL_CAG, "w");

        while (access(FIFO_CAG_SHELL, F_OK) != 0) {
            ; // Wait for the FIFO to be created
        }
        app->comms->fromCag = fopen(FIFO_CAG_SHELL, "r");
    } else {
        char width[3];
        char height[3];
        char refreshRate[5];

        sprintf(width, "%d", app->shellArgs->width);
        sprintf(height, "%d", app->shellArgs->height);
        sprintf(refreshRate, "%d", app->shellArgs->refreshRate);

        execl(CAG_EXECUTABLE, CAG_EXECUTABLE, "-w", width, "-h", 
            height, "-r", refreshRate, NULL);
        s4354198_exit(1, "execl failed to create cag process.\n");
    }
}

/**
 * Forks and starts the X11-based display process
 */
void start_display(void) {
    pid_t pid = fork();

    if (pid == -1) {
        // Failed to fork
        s4354198_exit(1, "Error creating display process.\n");
    } else if (pid > 0) {
        app->runtimeInfo->display = pid;
    } else {
        char width[3];
        char height[3];
        char refreshRate[5];

        sprintf(width, "%d", app->shellArgs->width);
        sprintf(height, "%d", app->shellArgs->height);
        sprintf(refreshRate, "%d", app->shellArgs->refreshRate);

        execl(DISPLAY_EXECUTABLE, DISPLAY_EXECUTABLE, "-w", width, "-h", 
            height, "-r", refreshRate, NULL);
        s4354198_exit(1, "execl failed to create display process.\n");
    }
}

/**
 * Stops all the associated processes
 */
void stop_all_processes(void) {
    int status;

    sem_wait(&outputAllowed);

    DrawingProcess *process = app->drawProcesses;
    while (process != NULL) {
        kill(process->pid, SIGKILL);
        waitpid(process->pid, &status, 0);
        printf("drawing process %d has been killed\n", process->pid);

        process = process->next;
    }

    if (app->runtimeInfo != NULL) {
        if (app->runtimeInfo->cag != 0) {
            kill(app->runtimeInfo->cag, SIGKILL);
            waitpid(app->runtimeInfo->cag, &status, 0);
            printf("cag has been killed\n");
        }

        if (app->runtimeInfo->display != 0) {
            kill(app->runtimeInfo->display, SIGKILL);
            waitpid(app->runtimeInfo->display, &status, 0);
            printf("display has been killed\n");
        }

        unlink(FIFO_SHELL_CAG);
        unlink(FIFO_CAG_DISPLAY);
    }

    sem_post(&outputAllowed);
}

/**
 * Prompt the user for input
 */
void prompt(void) {
    printf(PROMPT_COLOUR PROMPT PROMPT_INPUT_COLOUR);
    fflush(stdout);
}

/**
 * Resets the terminal
 */
void restore_prompt(void) {
    printf(PROMPT_RESET);
    fflush(stdout);
}

/**
 * Await input and process it
 */
void await_input(void) {
    bool stop = false;
    char* input;
    char* clone;
    int size;

    while (!stop) {
        sem_wait(&outputAllowed);
        prompt();
        sem_post(&outputAllowed);

        input = s4354198_getline(stdin, &size, &outputAllowed);
        clone = (char*) malloc(sizeof(char) * (size + 1));
        strncpy(clone, input, size);
        clone[strlen(input)] = '\0';

        if (size == -1) {
            stop = true;
        } else {
            handle_user_input(clone);

            sem_wait(&outputAllowed);
            free(input);
            free(clone);
            globalInput = NULL;
            sem_post(&outputAllowed);
        }
    }

    sem_wait(&outputAllowed);
    restore_prompt();
    sem_post(&outputAllowed);
}

/**
 * Perform actions depending on user input
 */
void handle_user_input(char* input) {
    char* token;
    bool error = false;
    int inputLength = strlen(input) + 1;
    char original[inputLength];
    strncpy(original, input, inputLength);

    token = strsep(&input, " ");

    // NULL token indicates it was an empty string
    if (token == NULL) {
        return;
    }

    if (s4354198_str_match(token, CMD_CELL)) {
        handle_draw(input, CELL);
    } else if (s4354198_str_match(token, CMD_STILL)) {
        handle_draw(input, STILL);
    } else if (s4354198_str_match(token, CMD_OSC)) {
        handle_draw(input, OSC);
    } else if (s4354198_str_match(token, CMD_SHIP)) {
        handle_draw(input, SHIP);
    } else if (s4354198_str_match(token, CMD_START)) {
        fprintf(app->comms->toCag, "%s\n", COMMS_START);
        fflush(app->comms->toCag);
    } else if (s4354198_str_match(token, CMD_STOP)) {
        fprintf(app->comms->toCag, "%s\n", COMMS_STOP);
        fflush(app->comms->toCag);
    } else if (s4354198_str_match(token, CMD_CLEAR)) {
        fprintf(app->comms->toCag, "%s\n", COMMS_CLEAR);
        fflush(app->comms->toCag);
    } else if (s4354198_str_match(token, CMD_HELP)) {
        display_help();
    } else if (s4354198_str_match(token, CMD_END)) {
        graceful_exit();
    } else {
        fprintf(app->comms->toCag, "%s\n", original);
        fflush(app->comms->toCag);
        error = true;
    }
    
    if (error) {
        lock_print(PROMPT_ERROR"Unrecognised command (%s)\n"PROMPT_RESET, original);
    }
}

/**
 * Parses the input for the form type
 */
bool get_form_type(FormType* formType, char** input) {
    char* token;

    if (*input == NULL) {
        lock_print(PROMPT_ERROR"Expected type argument.\n"PROMPT_RESET);
        return false;
    }

    int inputLength = strlen(*input) + 1;
    char original[inputLength];
    strncpy(original, *input, inputLength);
    token = strsep(input, " ");

    if (token == NULL) {
        lock_print(PROMPT_ERROR"Expected type argument (%s).\n"PROMPT_RESET,
            original);
        return false;
    }

    if (s4354198_str_match(token, FORM_ALIVE)) {
        *formType = ALIVE;
    } else if (s4354198_str_match(token, FORM_DEAD)) {
        *formType = DEAD;
    } else if (s4354198_str_match(token, FORM_BLOCK)) {
        *formType = BLOCK;
    } else if (s4354198_str_match(token, FORM_BEEHIVE)) {
        *formType = BEEHIVE;
    } else if (s4354198_str_match(token, FORM_LOAF)) {
        *formType = LOAF;
    } else if (s4354198_str_match(token, FORM_BOAT)) {
        *formType = BOAT;
    } else if (s4354198_str_match(token, FORM_BLINKER)) {
        *formType = BLINKER;
    } else if (s4354198_str_match(token, FORM_TOAD)) {
        *formType = TOAD;
    } else if (s4354198_str_match(token, FORM_BEACON)) {
        *formType = BEACON;
    } else if (s4354198_str_match(token, FORM_GLIDER)) {
        *formType = GLIDER;
    } else {
        lock_print(PROMPT_ERROR"Type argument didn't match a known type (%s).\n"PROMPT_RESET,
            original);
        return false;
    }

    return true;
}

/**
 * Parses the input for a coordinate
 */
bool get_form_coord(int* coord, char** input) {
    char* token;
    char* endToken;

    if (*input == NULL) {
        lock_print(PROMPT_ERROR"Expected a coordinate argument.\n"PROMPT_RESET);
        return false;
    }

    int inputLength = strlen(*input) + 1;
    char original[inputLength];
    strncpy(original, *input, inputLength);
    token = strsep(input, " ");
    
    if (token == NULL) {
        lock_print(PROMPT_ERROR"Expected a coordinate argument (%s).\n"PROMPT_RESET,
            original);
        return false;
    }

    // Check arg is solely a number
    for (int i = 0; i < strlen(token); i++) {
        if (!isdigit(token[i])) {
            lock_print(PROMPT_ERROR"Coordinate must be a number (%s).\n"PROMPT_RESET,
                original);
            return false;
        }
    }

    *coord = strtol(token, &endToken, 10);

    return true;
}

/**
 * Handle draw command and parse arguments
 */
bool handle_draw(char* input, LifeType lifeType) {
    FormType formType;
    int x;
    int y;

    // Parse all the data
    if (!get_form_type(&formType, &input) ||
            !get_form_coord(&x, &input) || 
            !get_form_coord(&y, &input)) {
        return false;
    }

    if (x > app->shellArgs->width) {
        lock_print(PROMPT_ERROR"X Coordinate must be >= 1 and <= %d (%d).\n"PROMPT_RESET,
            app->shellArgs->width,
            x
        );
        return false;
    }

    if (y > app->shellArgs->height) {
        lock_print(PROMPT_ERROR"Y Coordinate must be >= 1 and <= %d (%d).\n"PROMPT_RESET,
            app->shellArgs->height,
            y
        );
        return false;
    }

    // Check invalid life type
    bool invalidForm = false;
    switch (lifeType) {
        case CELL:
            invalidForm = (formType != ALIVE && formType != DEAD);
            break;
        case STILL:
            invalidForm = (formType != BLOCK && formType != BEEHIVE && formType != LOAF && formType != BOAT);
            break;
        case OSC:
            invalidForm = (formType != BLINKER && formType != TOAD && formType != BEACON);
            break;
        case SHIP:
            invalidForm = (formType != GLIDER);
            break;
    }

    if (invalidForm) {
        lock_print(PROMPT_ERROR"Invalid type or state (%s) for form (%s).\n"PROMPT_RESET,        
            formToString[(int)formType],
            lifeToString[(int)lifeType]
        );
        return false;
    }

    create_drawing_process(lifeType, formType, x, y);

    return true;
}

/**
 * Creates a new drawing process with supplied types and coords
 */
void create_drawing_process(LifeType lifeType, FormType formType, int x, int y) {
    DrawingProcess* start = app->drawProcesses;
    DrawingProcess* last = app->drawProcesses;
    DrawingProcess* new = (DrawingProcess*) malloc(sizeof(DrawingProcess));

    // Find the last drawing process
    while (last != NULL && last->next != NULL) {
        last = last->next;
    }

    new->lifeType = lifeType;
    new->formType = formType;
    new->x = x;
    new->y = y;
    new->id = 0;

    // Special case of DEAD
    if (formType == DEAD) {
        notify_cag_new_lifeform(new);
        free(new);
        return;
    }
    
    new->id = ++(app->lastId);

    new->prev = last;
    new->next = NULL;
    if (last != NULL) {
        last->next = new;
    }

    // No elements existed, so start the linked list
    if (start == NULL) {
        app->drawProcesses = new;
    }

    pid_t pid = fork();

    if (pid == -1) {
        // Failed to fork
        s4354198_exit(1, "Error creating drawing process.\n");
    } else if (pid > 0) {
        new->pid = pid;
        
        notify_cag_new_lifeform(new);
    } else {
        // Store the pid
        new->pid = getpid();

        // Set the first element in the life form process's
        // draw process list to itself
        app->drawProcesses = new;

        // Setup STDIN/STDOUT
        dup2(app->comms->toShellDescriptor, STDOUT);
        dup2(app->comms->fromShellDescriptor, STDIN);

        handle_life_form();

        s4354198_exit(1, "Life form has ended\n");
    }
}

/**
 * Notify the CAG that a new lifeform has been created
 */
void notify_cag_new_lifeform(DrawingProcess* process) {
    fprintf(app->comms->toCag, "%s %d %d %d %d %d\n", 
        COMMS_NEW,
        process->id,
        (int) process->lifeType,
        (int) process->formType,
        process->x,
        process->y
    );
    fflush(app->comms->toCag);
}

/**
 * Deletes a drawing process and removes it from the linked list
 */
void remove_drawing_process(DrawingProcess* process) {
    if (process->prev != NULL) {
        process->prev->next = process->next;
    }

    if (process->next != NULL) {
        process->next->prev = process->prev;
    }

    if (process == app->drawProcesses) {
        if (process->next != NULL) {
            app->drawProcesses = process->next;
        } else {
            app->drawProcesses = NULL;
        }
    }
    
    free(process);
}

/**
 * Displays help information on stdout
 */
void display_help(void) {
    sem_wait(&outputAllowed);
    printf(PROMPT_RESET);
    printf(
        PROMPT_HELP"cell <state> <x> <y> "PROMPT_RESET
                             "Draw a cell at x and y coordinates.\n"
        "                     State: 'alive' or 'dead'\n"
        PROMPT_HELP"still <type> <x> <y> "PROMPT_RESET
                             "Draw a still life at x and y coordinates.\n"
        "                     Note x and y are at the top left edge of the\n"
        "                     figure. Types: 'block', 'beehive', 'loaf' and\n"
        "                     'boat'\n"
        PROMPT_HELP"osc <type> <x> <y>   "PROMPT_RESET
                             "Draw an oscillator at x and y coordinates.\n"
        "                     Note x and y are at the top left edge of the\n"
        "                     figure. Types: 'blinker', 'toad' and 'beacon'\n"
        PROMPT_HELP"ship <type> <x> <y>  "PROMPT_RESET
                             "Draw a spaceship at x and y coordinates. Note\n"
        "                     x and y are at the top left edge of the figure\n"
        "                     Types: 'glider'\n"
        PROMPT_HELP"start                "PROMPT_RESET
                             "Start or restart all cellular automation\n"
        PROMPT_HELP"stop                 "PROMPT_RESET
                             "Stop all cellular automation\n"
        PROMPT_HELP"clear                "PROMPT_RESET
                             "Clear all visual cellular automation\n"
        PROMPT_HELP"help                 "PROMPT_RESET
                             "Displays this list of each command and its usage\n"
        PROMPT_HELP"end                  "PROMPT_RESET
                             "End all processes and threads associated with\n"
        "                     the CAG System (includes User Shell, Cellular\n"
        "                     Automation and Cellular Display), using kill()\n"
    );
    sem_post(&outputAllowed);
}

/**
 * Logic for handling life form logic in the drawing process
 */
void handle_life_form(void) {
    pthread_t millisecondsThread;
    pthread_t reportThread;

    pthread_create(&millisecondsThread, NULL, &count_milliseconds, NULL);
    pthread_create(&reportThread, NULL, &report_alive, NULL);

    while (1) {
        sleep(1)
        ; // Do nothing
    }
}

/**
 * Report the time alive to the shell
 */
void* report_alive(void* voidPtr) {
    while (1) {
        sleep(1);
        
        printf("(%d) %s (%s) originating at (%d, %d) has been alive %lums.\n",
            app->drawProcesses->id,
            lifeToString[(int)app->drawProcesses->lifeType],
            formToString[(int)app->drawProcesses->formType],
            app->drawProcesses->x,
            app->drawProcesses->y,
            app->upMilliseconds
        );
        fflush(stdout);
    }

    return NULL;
}

/**
 * Counts the number of milliseconds since the process started
 */
void* count_milliseconds(void* voidPtr) {
    while (1) {
        usleep(10000);
        
        app->upMilliseconds += 10;
    }
    
    return NULL;
}

/**
 * Kill a specific drawing process
 */
void kill_drawing(int id) {
    int status;

    DrawingProcess *process = app->drawProcesses;
    while (process != NULL && process->id != id) {
        process = process->next;
    }

    if (process != NULL) {
        kill(process->pid, SIGKILL);
        waitpid(process->pid, &status, 0);
        remove_drawing_process(process);
    }
}

/**
 * Handles the output from the drawing processes
 */
void* drawing_process_out_handler(void* voidPtr) {
    bool stop = false;
    char* input = NULL;
    char* cleanedInput = NULL;
    size_t size;

    while (!stop) {
        int read = getline(&input, &size, app->comms->fromDrawing);

        if (read == -1) {
            stop = true;
        } else {
            // Remove the newline from the end
            cleanedInput = (char*) malloc(sizeof(char) * (strlen(input) - 1));
            strncpy(cleanedInput, input, strlen(input) - 1);
            cleanedInput[strlen(input) - 1] = '\0';
            
            int charsToDelete = strlen(PROMPT);
            
            if (globalInput != NULL) {
                charsToDelete = strlen(globalInput);
            }

            for (int i = 0; i < charsToDelete; i++) {
                printf("%s", BACKSPACE);
            }

            sem_wait(&outputAllowed);

            // Return to start of the line
            printf("\r");
            printf(PROMPT_TIME"DRAW: %s\n"PROMPT_RESET, cleanedInput);

            prompt();

            printf("%s", globalInput);
            fflush(stdout);

            sem_post(&outputAllowed);

            free(cleanedInput);
        }
    }

    return NULL;
}

/**
 * Handles the output from the drawing processes
 */
void* engine_out_handler(void* voidPtr) {
    bool stop = false;
    char* input = NULL;
    char* endToken;
    char* cleanedInput = NULL;
    size_t size;

    while (!stop) {
        int read = getline(&input, &size, app->comms->fromCag);

        if (read == -1) {
            stop = true;
        } else {
            // Remove the newline from the end
            cleanedInput = (char*) malloc(sizeof(char) * (strlen(input) - 1));
            char* originalPtr = cleanedInput;
            strncpy(cleanedInput, input, strlen(input) - 1);
            cleanedInput[strlen(input) - 1] = '\0';

            char* token;
            int inputLength = strlen(cleanedInput) + 1;
            char original[inputLength];
            strncpy(original, cleanedInput, inputLength);

            token = strsep(&cleanedInput, " ");

            if (s4354198_str_match(token, COMMS_DEAD)) {
                token = strsep(&cleanedInput, " ");
                int id = strtol(token, &endToken, 10);
                kill_drawing(id);
            } else {
                int charsToDelete = strlen(PROMPT);
    
                if (globalInput != NULL) {
                    charsToDelete = strlen(globalInput);
                }
    
                for (int i = 0; i < charsToDelete; i++) {
                    printf("%s", BACKSPACE);
                }
    
                sem_wait(&outputAllowed);
    
                // Return to start of the line
                printf("\r");
                printf(PROMPT_TIME"CAG: %s\n"PROMPT_RESET, original);
    
                prompt();
    
                printf("%s", globalInput);
                fflush(stdout);
    
                sem_post(&outputAllowed);
            }

            free(originalPtr);
        }
    }

    return NULL;
}