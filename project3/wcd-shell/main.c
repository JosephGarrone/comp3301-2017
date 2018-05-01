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
#include <libmemcached/memcached.h>

#include "../common/s4354198_structs.h"
#include "../common/s4354198_defines.h"
#include "../common/s4354198_utils.h"
#include "../common/s4354198_externs.h"
#include "../common/s4354198_memcached.h"

/* Function prototypes */
void await_input(void);
void create_semaphores(void);
void display_help(void);
void graceful_exit(void);
void handle_user_input(char* input);
void lock_print_pipe(FILE* dest, const char *format, ...);
void lock_print(const char *format, ...);
void make_stdin_unbuffered(void);
void* process_out_handler(void* voidPtr);
void* process_key_handler(void* voidPtr);
void prompt(void);
void register_signal_handlers(void);
void restore_prompt(void);
void restore_stdin_buffering(void);
void sigint_handler(int signal);
void start_control();
void start_memcached();
void start_other_processes(void);
void start_remote();
void start_threads(void);
void stop_all_processes(void);
void stop_all_threads(void);
void handle_sys();

/* Globals */
// Contains application data
Application* app;
// Threads for monitoring output from subprocesses
pthread_t controlOutput;
pthread_t remoteOutput;
pthread_t memOutput;
pthread_t keyOutput;
// Terminal settings old
struct termios oldTio;
// Terminal settings new
struct termios newTio;
// Output based semaphore
sem_t outputAllowed;
// Other process comms semaphore
sem_t otherProcessAllowed;

/* Externs */
extern char* globalInput;

int main(int argc, char** argv) {
    app = (Application*) malloc(sizeof(Application));

    register_signal_handlers();

    make_stdin_unbuffered();

    create_semaphores();

    start_other_processes();

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

    printf("\n");
    
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
 * Semaphore locked access to other processes stdin's
 */
void lock_print_pipe(FILE* dest, const char *format, ...) {
    va_list args;
    va_start(args, format);

    sem_wait(&otherProcessAllowed);
    vfprintf(dest, format, args);
    fflush(dest);
    sem_post(&otherProcessAllowed);
    
    va_end(args);
}

/**
 * Create all the semaphores
 */ 
void create_semaphores(void) {
    sem_init(&outputAllowed, 0, 1);
    sem_init(&otherProcessAllowed, 0, 1);
}

/**
 * Starts all the other threads needed
 */
void start_threads(void) {
    pthread_create(&controlOutput, NULL, &process_out_handler, (void*) app->comms->fromControl);
    pthread_create(&remoteOutput, NULL, &process_out_handler, (void*) app->comms->fromRemote);
    pthread_create(&memOutput, NULL, &process_out_handler, (void*) app->comms->fromMem);
    pthread_create(&keyOutput, NULL, &process_key_handler, NULL);
}

/**
 * Stops all the threads
 */
void stop_all_threads(void) {
    // void* returnValue;
    // pthread_cancel(drawingProcessOutput);
    // pthread_join(drawingProcessOutput, &returnValue);
    // pthread_cancel(engineOutput);
    // pthread_join(engineOutput, &returnValue);
    // pthread_cancel(recorderOutput);
    // pthread_join(recorderOutput, &returnValue);
    // pthread_cancel(playerOutput);
    // pthread_join(playerOutput, &returnValue);
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
 * Starts all other required processes
 */
void start_other_processes(void) {
    app->runtimeInfo = (RuntimeInfo*) malloc(sizeof(RuntimeInfo));
    app->comms = (Comms*) malloc(sizeof(Comms));

    start_memcached();
    start_control();
    start_remote();
}

/**
 * Start memcached process
 */
void start_memcached() {
    int processIn[2];
    int processOut[2];

    if (pipe(processIn) || pipe(processOut)) {
        s4354198_exit(1, "Failed to create memcached process pipes.\n");
    }

    pid_t pid = fork();
    
    if (pid == -1) {
        // Failed to fork
        s4354198_exit(1, "Error creating memcached process.\n");
    } else if (pid > 0) {
        app->runtimeInfo->memcached = pid;
        app->comms->toMem = fdopen(processIn[SOCKET_WRITE], "w");
        app->comms->fromMem = fdopen(processOut[SOCKET_READ], "r");
    } else {
        dup2(processOut[SOCKET_WRITE], STDOUT);
        dup2(processIn[SOCKET_READ], STDIN);

        execl(MEMCACHED_EXECUTABLE, MEMCACHED_EXECUTABLE, NULL);
        s4354198_exit(1, "execl failed to create memcached process.\n");
    }
}

/**
 * Start the control process
 */
void start_control() {
    int processIn[2];
    int processOut[2];

    if (pipe(processIn) || pipe(processOut)) {
        s4354198_exit(1, "Failed to create control process pipes.\n");
    }

    pid_t pid = fork();
    
    if (pid == -1) {
        // Failed to fork
        s4354198_exit(1, "Error creating control process.\n");
    } else if (pid > 0) {
        app->runtimeInfo->control = pid;
        app->comms->toControl = fdopen(processIn[SOCKET_WRITE], "w");
        app->comms->fromControl = fdopen(processOut[SOCKET_READ], "r");
    } else {
        dup2(processOut[SOCKET_WRITE], STDOUT);
        dup2(processIn[SOCKET_READ], STDIN);

        execl(CONTROL_EXECUTABLE, CONTROL_EXECUTABLE, NULL);
        s4354198_exit(1, "execl failed to create control process.\n");
    }
}

/**
 * Start the remote process
 */
void start_remote() {
    int processIn[2];
    int processOut[2];

    if (pipe(processIn) || pipe(processOut)) {
        s4354198_exit(1, "Failed to create remote process pipes.\n");
    }

    pid_t pid = fork();
    
    if (pid == -1) {
        // Failed to fork
        s4354198_exit(1, "Error creating remote process.\n");
    } else if (pid > 0) {
        app->runtimeInfo->remote = pid;
        app->comms->toRemote = fdopen(processIn[SOCKET_WRITE], "w");
        app->comms->fromRemote = fdopen(processOut[SOCKET_READ], "r");
    } else {
        dup2(processOut[SOCKET_WRITE], STDOUT);
        dup2(processIn[SOCKET_READ], STDIN);

        execl(REMOTE_EXECUTABLE, REMOTE_EXECUTABLE, NULL);
        s4354198_exit(1, "execl failed to create remote process.\n");
    }
}

/**
 * Stops all the associated processes
 */
void stop_all_processes(void) {
    int status;

    sem_wait(&outputAllowed);

    if (app->runtimeInfo != NULL) {
        if (app->runtimeInfo->control != 0) {
            kill(app->runtimeInfo->control, SIGINT);
            waitpid(app->runtimeInfo->control, &status, 0);
            printf("control has been killed\n");
        }

        if (app->runtimeInfo->remote != 0) {
            kill(app->runtimeInfo->remote, SIGINT);
            waitpid(app->runtimeInfo->remote, &status, 0);
            printf("remote has been killed\n");
        }

        if (app->runtimeInfo->memcached != 0) {
            kill(app->runtimeInfo->memcached, SIGKILL);
            waitpid(app->runtimeInfo->memcached, &status, 0);
            printf("memcached has been killed\n");
        }
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
        clone = (char*) malloc(sizeof(char) * (size + 2));
        strncpy(clone, input, size);
        clone[strlen(input)] = '\0';
        free(input);
        globalInput = NULL;

        if (size == -1) {
            stop = true;
        } else {
            handle_user_input(clone);

            sem_wait(&outputAllowed);
            free(clone);
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
    char* original = strdup(input);

    token = strsep(&input, " ");

    // NULL token indicates it was an empty string
    if (token == NULL) {
        return;
    }

    if (s4354198_str_match(token, CMD_HELP)) {
        display_help();
    } else if (s4354198_str_match(token, CMD_WCD)) {
        lock_print_pipe(app->comms->toControl, CMD_WCD" %s\n", input);
    } else if (s4354198_str_match(token, CMD_SYS)) {
        handle_sys();
    } else if (s4354198_str_match(token, CMD_CLEAR)) {
        lock_print_pipe(app->comms->toControl, CMD_CLEAR" %s\n", input);
    } else if (s4354198_str_match(token, CMD_LIST)) {
        lock_print_pipe(app->comms->toControl, CMD_LIST"\n", input);
    } else if (s4354198_str_match(token, CMD_KILL)) {
        lock_print_pipe(app->comms->toControl, CMD_KILL" %s\n", input);
    } else if (s4354198_str_match(token, CMD_SEL)) {
        lock_print_pipe(app->comms->toControl, CMD_SEL" %s\n", input);
    } else if (s4354198_str_match(token, CMD_IMG)) {
        lock_print_pipe(app->comms->toControl, CMD_IMG" %s\n", input);
    } else if (s4354198_str_match(token, CMD_EXIT)) {
        graceful_exit();
    } else {
        error = true;
    }
    
    if (error) {
        lock_print(PROMPT_ERROR"Unrecognised command (%s)\n"PROMPT_RESET, original);
    }

    free(original);
}

/**
 * Handles the sys command
 */
void handle_sys() {
    char line[500];
    int read;

    int rpiMem;
    int rpiLoaded;
    char* rpiState = malloc(sizeof(char) * 50);
    int rpiOffset;

    // lirc_rpi
    FILE* modules = fopen("/proc/modules", "r");
    while (read != 4) {
        fgets(line, sizeof(line), modules);
        read = sscanf(line, "lirc_rpi %d %d - %s 0x%x (C)\n", &rpiMem, &rpiLoaded, rpiState, &rpiOffset);
    }
    fclose(modules);
    read = 0;
    
    int devMem;
    int devLoaded;
    char* devState = malloc(sizeof(char) * 50);
    int devOffset;

    // lirc_dev
    modules = fopen("/proc/modules", "r");
    while (read != 4) {
        fgets(line, sizeof(line), modules);
        read = sscanf(line, "lirc_dev %d %d lirc_rpi, %s 0x%x\n", &devMem, &devLoaded, devState, &devOffset);
    }
    fclose(modules);

    lock_print(PROMPT_ERROR"Module: lirc_rpi  Memory Size: %d bytes  Instances: %d  State: %s  Offset: %d bytes\n"PROMPT_RESET, rpiMem, rpiLoaded, rpiState, rpiOffset);
    lock_print(PROMPT_ERROR"Module: lirc_dev  Memory Size: %d bytes  Instances: %d  State: %s  Offset: %d bytes\n"PROMPT_RESET, devMem, devLoaded, devState, devOffset);

    free(rpiState);
    free(devState);
}

/**
 * Displays help information on stdout
 */
void display_help(void) {
    sem_wait(&outputAllowed);
    printf(PROMPT_RESET);
    printf(
        PROMPT_HELP"wcd <x> <y> <width> <height>\n"PROMPT_RESET
        "    Create a webcam display of the specified size\n"
        "    at the specified coordinates.\n\n"
        PROMPT_HELP"sys\n"PROMPT_RESET
        "    Display the LIRC module usage.\n\n"
        PROMPT_HELP"list\n"PROMPT_RESET
        "    Lists all the webcam displays and their IDs.\n\n"
        PROMPT_HELP"clear <id>\n"PROMPT_RESET
        "    Clears the specified webcam display.\n\n"
        PROMPT_HELP"sel <id>\n"PROMPT_RESET
        "    Selects the active webcam display.\n\n"
        PROMPT_HELP"kill <id>\n"PROMPT_RESET
        "    Kills the specified webcam display.\n\n"
        PROMPT_HELP"help\n"PROMPT_RESET
        "    Displays this list of each command and its usage.\n\n"
        PROMPT_HELP"exit\n"PROMPT_RESET
        "    Exits this program.\n\n"
    );
    sem_post(&outputAllowed);
}

/**
 * Handles the output from the drawing processes
 */
void* process_out_handler(void* voidPtr) {
    bool stop = false;
    char* input = NULL;
    char* cleanedInput = NULL;
    FILE* file = (FILE*) voidPtr;
    size_t size;

    while (!stop) {
        int read = getline(&input, &size, file);

        if (read == -1) {
            stop = true;
        } else {
            // Remove the newline from the end
            cleanedInput = (char*) malloc(sizeof(char) * strlen(input));
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
            if (cleanedInput[0] == '$') {
                printf(PROMPT_ERROR"%s\n"PROMPT_RESET, &(cleanedInput[1]));
            } else {
                printf(PROMPT_TIME"PROC: %s\n"PROMPT_RESET, cleanedInput);
            }

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
 * Process memcache keys
 */
void* process_key_handler(void* voidPtr) {
    char command[100];
    int index = 0;

    app->memc = s4354198_connect_memcached_server();
    
    while (1) {
        // lock_print("Run\n");
        char* result = NULL;
        s4354198_wait_not_mem_get(MEM_KEY, "-", &result);
        free(result);
        // lock_print("Got key\n");
    
        char* key = s4354198_mem_get(MEM_KEY);

        s4354198_mem_set(MEM_KEY, "-");

        //lock_print("Reset key\n");

        if (key[0] == '@') {

        } else if (key[0] == '$') {

        } else if (key[0] == '!') {
            
        } else if (key[0] == '#') {
            
        } else if (key[0] == '*') {
            
        } else if (key[0] == '(') {
            handle_user_input(strdup("clear"));
        } else if (key[0] == ')') {
            handle_user_input(strdup("clear"));
        } else if (key[0] == '.') {
            if (strlen(command) == 1 && command[0] == 'S') {
                handle_user_input(strdup("img raw"));
            } else if (s4354198_str_match(command, "RAW")) {
                handle_user_input(strdup("img raw"));
            } else if (s4354198_str_match(command, "FLP")) {
                handle_user_input(strdup("img flp"));
            } else if (s4354198_str_match(command, "BLR")) {
                handle_user_input(strdup("img blr"));
            } else if (strlen(command) > 3) {
                char temp[4];
                temp[0] = command[0];
                temp[1] = command[1];
                temp[2] = command[3];
                temp[3] = '\0';
                if (s4354198_str_match(temp, "DLY")) {
                    char otherCommand[100];
                    sprintf(otherCommand, "img dly %s", &(command[3]));
                    handle_user_input(otherCommand);
                }
            } else {
                handle_user_input(strdup(command));
            }

            index = 0;
            command[0] = '\0';
        } else {
            command[index++] = key[0];
            command[index] = '\0';
            lock_print(PROMPT_ERROR"Remote command: %s\n"PROMPT_RESET, command);
        }

        free(key);

        usleep(100000);
    }

    return NULL;
}