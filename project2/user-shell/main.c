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
void start_recorder(void);
void start_player(void);
void stop_all_threads(void);
void stop_all_processes(void);
void lock_print(const char* format, ...);
void lock_print_to_cag(const char* format, ...);
void lock_print_to_player(const char* format, ...);
void lock_print_to_recorder(const char* format, ...);
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
void* recorder_out_handler(void* ptr);
void* player_out_handler(void* ptr);
void handle_mount(char* input);
void handle_ls(char* input);
void handle_cd(char* input);
void handle_mkdir(char* input);
void handle_touch(char* input);
void handle_frame(char* input);
void handle_rec(char* input);
void handle_size(char* input);
void handle_free(char* input);
void handle_p(char* input);
void handle_s(char* input);
void handle_play(char* input);
void handle_halp();
void display_frame(int* data);
void display_nodes(INode* nodes);

/* Globals */
// Contains application data
Application* app;
// Thread for monitoring drawing processes
pthread_t drawingProcessOutput;
// Thread for engine output
pthread_t engineOutput;
// Thread for recorder output
pthread_t recorderOutput;
// Thread for player output
pthread_t playerOutput;
// Terminal settings old
struct termios oldTio;
// Terminal settings new
struct termios newTio;
// Output based semaphore
sem_t outputAllowed;
// Semaphore for comms to the cag
sem_t sendToCag;
// Semaphore for comms to the recorder
sem_t sendToRecorder;
// Semaphore for comms to the player
sem_t sendToPlayer;

/* Externs */
extern char* formToString[];
extern char* lifeToString[];
extern char* globalInput;

int main(int argc, char** argv) {
    app = (Application*) malloc(sizeof(Application));
    app->lastId = 0;
    app->drawProcesses = NULL;
    app->cwd = strdup(DEFAULT_DIR);
    app->cfs = (CFSInfo*) malloc(sizeof(CFSInfo));
    app->cfs->loaded = false;
    app->drawingsSTFU = false;
    app->upMilliseconds = 0;
    app->pState = STATE_NOTHING;
    app->rState = STATE_NOTHING;

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
 * Semaphore locked access to the cag fifo
 */
void lock_print_to_cag(const char *format, ...) {
    va_list args;
    va_start(args, format);

    sem_wait(&sendToCag);
    vfprintf(app->comms->toCag, format, args);
    fflush(app->comms->toCag);
    sem_post(&sendToCag);
    
    va_end(args);
}

/**
 * Semaphore locked access to the player fifo
 */
void lock_print_to_player(const char *format, ...) {
    va_list args;
    va_start(args, format);

    sem_wait(&sendToPlayer);
    vfprintf(app->comms->toPlayer, format, args);
    fflush(app->comms->toPlayer);
    sem_post(&sendToPlayer);
    
    va_end(args);
}

/**
 * Semaphore locked access to the recorder fifo
 */
void lock_print_to_recorder(const char *format, ...) {
    va_list args;
    va_start(args, format);

    sem_wait(&sendToRecorder);
    vfprintf(app->comms->toRecord, format, args);
    fflush(app->comms->toRecord);
    sem_post(&sendToRecorder);
    
    va_end(args);
}

/**
 * Create all the semaphores
 */ 
void create_semaphores(void) {
    sem_init(&outputAllowed, 0, 1);
    sem_init(&sendToCag, 0, 1);
    sem_init(&sendToPlayer, 0, 1);
    sem_init(&sendToRecorder, 0, 1);
}

/**
 * Starts all the other threads needed
 */
void start_threads(void) {
    pthread_create(&drawingProcessOutput, NULL, &drawing_process_out_handler, NULL);
    pthread_create(&engineOutput, NULL, &engine_out_handler, NULL);
    pthread_create(&recorderOutput, NULL, &recorder_out_handler, NULL);
    pthread_create(&playerOutput, NULL, &player_out_handler, NULL);
}

/**
 * Stops all the threads
 */
void stop_all_threads(void) {
    void* returnValue;
    pthread_cancel(drawingProcessOutput);
    pthread_join(drawingProcessOutput, &returnValue);
    pthread_cancel(engineOutput);
    pthread_join(engineOutput, &returnValue);
    pthread_cancel(recorderOutput);
    pthread_join(recorderOutput, &returnValue);
    pthread_cancel(playerOutput);
    pthread_join(playerOutput, &returnValue);
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
    start_recorder();
    start_player();
}

/**
 * Forks and starts the recorder process
 */
void start_recorder(void) {
    pid_t pid = fork();
    
    if (pid == -1) {
        // Failed to fork
        s4354198_exit(1, "Error creating recorder process.\n");
    } else if (pid > 0) {
        app->runtimeInfo->recorder = pid;
        
        mkfifo(FIFO_SHELL_CR, FIFO_SHELL_CR_PERMS);
        app->comms->toRecord = fopen(FIFO_SHELL_CR, "w");

        while (access(FIFO_CR_SHELL, F_OK) != 0) {
            ; // Wait for the FIFO to be created
        }
        app->comms->fromRecord = fopen(FIFO_CR_SHELL, "r");
    } else {
        char width[3];
        char height[3];
        char refreshRate[5];

        sprintf(width, "%d", app->shellArgs->width);
        sprintf(height, "%d", app->shellArgs->height);
        sprintf(refreshRate, "%d", app->shellArgs->refreshRate);

        execl(CR_EXECUTABLE, CR_EXECUTABLE, "-w", width, "-h", 
            height, "-r", refreshRate, NULL);
        s4354198_exit(1, "execl failed to create recorder process.\n");
    }
}

/**
 * Forks and starts the player process
 */
void start_player(void) {
    pid_t pid = fork();
    
    if (pid == -1) {
        // Failed to fork
        s4354198_exit(1, "Error creating player process.\n");
    } else if (pid > 0) {
        app->runtimeInfo->player = pid;

        mkfifo(FIFO_SHELL_CP, FIFO_SHELL_CP_PERMS);
        app->comms->toPlayer = fopen(FIFO_SHELL_CP, "w");

        while (access(FIFO_CP_SHELL, F_OK) != 0) {
            ; // Wait for the FIFO to be created
        }
        app->comms->fromPlayer = fopen(FIFO_CP_SHELL, "r");
    } else {
        char width[3];
        char height[3];
        char refreshRate[5];

        sprintf(width, "%d", app->shellArgs->width);
        sprintf(height, "%d", app->shellArgs->height);
        sprintf(refreshRate, "%d", app->shellArgs->refreshRate);

        execl(CP_EXECUTABLE, CP_EXECUTABLE, "-w", width, "-h", 
            height, "-r", refreshRate, NULL);
        s4354198_exit(1, "execl failed to create player process.\n");
    }
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
        if (app->runtimeInfo->player != 0) {
            kill(app->runtimeInfo->player, SIGKILL);
            waitpid(app->runtimeInfo->player, &status, 0);
            printf("player has been killed\n");
        }

        if (app->runtimeInfo->recorder != 0) {
            kill(app->runtimeInfo->recorder, SIGKILL);
            waitpid(app->runtimeInfo->recorder, &status, 0);
            printf("recorder has been killed\n");
        }

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
        unlink(FIFO_CAG_SHELL);
        unlink(FIFO_CAG_DISPLAY);
        unlink(FIFO_DISPLAY_CAG);
        unlink(FIFO_SHELL_CR);
        unlink(FIFO_CR_SHELL);
        unlink(FIFO_CAG_CR);
    }

    sem_post(&outputAllowed);
}

/**
 * Prompt the user for input
 */
void prompt(void) {
    if (app->cfs->loaded) {
        printf(PROMPT_COLOUR PROMPT PROMPT_INPUT_COLOUR, app->cwd);
    } else {
        printf(PROMPT_COLOUR PROMPT PROMPT_INPUT_COLOUR, "no fs");
    }
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
        if (app->pState == STATE_PAUSED || app->pState == STATE_STARTED) {
            lock_print(PROMPT_ERROR"Cannot begin simulation while player is running\n"PROMPT_RESET);
        } else {
            fprintf(app->comms->toCag, "%s\n", COMMS_START);
            fflush(app->comms->toCag);
        }
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
    } else if (s4354198_str_match(token, CMD_MOUNT)) {
        handle_mount(input);
    } else if (s4354198_str_match(token, CMD_LS)) {
        handle_ls(input);
    } else if (s4354198_str_match(token, CMD_CD)) {
        handle_cd(input);
    } else if (s4354198_str_match(token, CMD_MKDIR)) {
        handle_mkdir(input);
    } else if (s4354198_str_match(token, CMD_FRAME)) {
        handle_frame(input);
    } else if (s4354198_str_match(token, CMD_REC)) {
        handle_rec(input);
    } else if (s4354198_str_match(token, CMD_SIZE)) {
        handle_size(input);
    } else if (s4354198_str_match(token, CMD_FREE)) {
        handle_free(input);
    } else if (s4354198_str_match(token, CMD_S)) {
        handle_s(input);
    } else if (s4354198_str_match(token, CMD_P)) {
        handle_p(input);
    } else if (s4354198_str_match(token, CMD_PLAY)) {
        handle_play(input);
    } else if (s4354198_str_match(token, CMD_TOUCH)) {
        handle_touch(input);
    } else if (s4354198_str_match(token, CMD_HALP)) {
        handle_halp();
    } else if (s4354198_str_match(token, CMD_TOGGLE_DRAWINGS_OUT)) {
        app->drawingsSTFU = !app->drawingsSTFU;
        if (app->drawingsSTFU) {
            lock_print("Drawings have been shut up\n");
        } else {
            lock_print("Drawings are allowed to speak now...\n");
        }
    } else {
        fprintf(app->comms->toCag, "%s\n", original);
        fflush(app->comms->toCag);
        fprintf(app->comms->toRecord, "%s\n", original);
        fflush(app->comms->toRecord);
        fprintf(app->comms->toPlayer, "%s\n", original);
        fflush(app->comms->toPlayer);
        error = true;
    }
    
    if (error) {
        lock_print(PROMPT_ERROR"Unrecognised command (%s)\n"PROMPT_RESET, original);
    }
}

/**
 * Handle mounting of HDF file system
 */
void handle_mount(char* input) {
    if (app->cfs->loaded) {
        return lock_print(PROMPT_ERROR"Filesystem is already mounted\n"PROMPT_RESET);
    }

    if (input != NULL) {
        if (s4354198_is_white_space(input)) {
            return lock_print(PROMPT_ERROR"Invalid filename\n"PROMPT_RESET);
        }

        app->cfs->filename = strdup(input);
        app->cfs->loaded = true;

        if (access(input, F_OK) != -1) {
            lock_print("Mounted '%s' at /\n", app->cfs->filename);
        } else {
            s4354198_createCFS(app->cfs->filename);
            lock_print("Created '%s' and mounted at /\n", app->cfs->filename);
        }

        lock_print_to_player("%s %s\n", PR_CMD_INIT, app->cfs->filename);
        lock_print_to_recorder("%s %s\n", PR_CMD_INIT, app->cfs->filename);
        app->pState = STATE_INIT;
        app->rState = STATE_INIT;
    } else {
        lock_print(PROMPT_ERROR"No filename specified\n"PROMPT_RESET);
    } 
}

/**
 * Prints a table of nodes
 */
void display_nodes(INode* nodes) {
    if (nodes == NULL || nodes->name == NULL) {
        return lock_print("directory is empty\n");
    }

    INode* start = nodes;
    INode* current = nodes;
    int widths[6] = {4, 4, 7, 9, 4, 5};
    int items = 0;

    while (current != NULL && current->name != NULL) {
        if (strlen(current->name) > widths[0]) {
            widths[0] = strlen(current->name);
        }

        if (strlen(current->type) > widths[1]) {
            widths[1] = strlen(current->type);
        }
        
        if (strlen(current->sectors) > widths[2]) {
            widths[2] = strlen(current->sectors);
        }

        if (strlen(current->timestamp) > widths[3]) {
            widths[3] = strlen(current->timestamp);
        }
        
        if (strlen(current->mode) > widths[4]) {
            widths[4] = strlen(current->mode);
        }

        if (strlen(current->owner) > widths[5]) {
            widths[5] = strlen(current->owner);
        }

        items++;
        current = current->next;
    }

    int maxLineLength = 0;
    for (int i = 0; i < 6; i++) {
        maxLineLength += widths[i] + 2;
    }

    char format[100];
    sprintf(format, "%%-%ds %%-%ds %%-%ds %%-%ds %%-%ds %%-%ds", 
        widths[0], widths[1], widths[2], widths[3], widths[4], widths[5]);

    char* output = (char*) malloc(sizeof(char) * maxLineLength * (items + 1));
    int index = 0;

    current = start;
    index += sprintf(&(output[index]), format, "Name", "Type", "Sectors", "Timestamp", "Mode", "Owner");
    index += sprintf(&(output[index]), "\n");
    while (current != NULL && current->name != NULL) {
        index += sprintf(&(output[index]), format, 
            current->name,
            current->type,
            current->sectors,
            current->timestamp,
            current->mode,
            current->owner
        );
        index += sprintf(&(output[index]), "\n");
        
        current = current->next;
    }

    // Free everything
    current = start;
    INode* temp;
    while (current != NULL && current->name != NULL) {
        temp = current->next;

        free(current->name);
        free(current->type);
        free(current->sectors);
        free(current->timestamp);
        free(current->mode);
        free(current->owner);
        free(current);
        
        current = temp;
    }

    // Print
    lock_print(PROMPT_RESET"%s", output);
}

/**
 * Displays a frame
 */
void display_frame(int* data) {
    int widths[MAX_WIDTH];
    for (int i = 0; i < MAX_WIDTH; i++) {
        widths[i] = 0;
    }
    int items = MAX_HEIGHT;

    char temp[50];

    for (int i = 0; i < MAX_HEIGHT; i++) {
        for (int j = 0; j < MAX_WIDTH; j++) {
            int val = data[i * MAX_WIDTH + j];
            sprintf(temp, "%d", val);

            for (int k = 0; k < MAX_WIDTH; k++) {
                if (strlen(temp) > widths[k]) {
                    widths[k] = strlen(temp);
                }
            }
        }
    }

    int maxLineLength = 20;
    for (int i = 0; i < MAX_WIDTH; i++) {
        maxLineLength += widths[i] + 2;
    }

    char** formats = (char**) malloc(sizeof(char*) * MAX_WIDTH);
    for (int i = 0; i < MAX_WIDTH; i++) {
        formats[i] = (char*) malloc(sizeof(char) * 300);
        sprintf(formats[i], "%%-%ds", widths[i]);
    }
    char* output = (char*) malloc(sizeof(char) * maxLineLength * (items + 2));
    int index = 0;
    
    for (int i = 0; i < MAX_HEIGHT; i++) {
        for (int j = 0; j < MAX_WIDTH; j++) {
            int val = data[i * MAX_WIDTH + j];
            sprintf(temp, "%d", val);
            index += sprintf(&(output[index]), formats[j], temp);
            
            if (j < MAX_WIDTH - 1) {
                index += sprintf(&(output[index]), " ");
            }
        }
        index += sprintf(&(output[index]), "\n");
    }

    // Free
    for (int i = 0; i < MAX_WIDTH; i++) {
        free(formats[i]);
    }
    free(formats);
    free(data);

    // Print
    lock_print(PROMPT_RESET"%s", output);
}

/**
 * Handle listing of a directory
 */
void handle_ls(char* input) {
    if (!app->cfs->loaded) {
        return lock_print(ERR_NO_MOUNTED);
    }

    char* path = strdup(app->cwd);

    if (input != NULL) {
        free(path);
        path = s4354198_path_get_abs(app->cwd, input);
    }

    char* origPath = strdup(path);
    char* origPtr = path;
    
    char* msg;
    PathInfo* pathInfo = s4354198_path_info();
    bool byDir;

    if (s4354198_path(path, &pathInfo, &msg)) {
        if (pathInfo->target != NULL && s4354198_str_match(pathInfo->target, "/")) {
            // Do nothing
        } else if (s4354198_path_taken(pathInfo, &byDir) && !byDir) {
            lock_print(PROMPT_ERROR"Cannot ls '%s': is not a directory\n"PROMPT_RESET, origPath);
            s4354198_path_free_info(pathInfo);
            free(origPtr);
            free(origPath);
            return;
        } else if (!s4354198_path_taken(pathInfo, &byDir)) {
            lock_print(PROMPT_ERROR"Cannot ls '%s': directory does not exist\n"PROMPT_RESET, origPath);
            s4354198_path_free_info(pathInfo);
            free(origPtr);
            free(origPath);
            return;
        }

        INode* nodes = s4354198_ls(pathInfo);
        display_nodes(nodes);
    } else {
        lock_print(PROMPT_ERROR"%s\n"PROMPT_RESET, msg);
        free(msg);
    }
    
    free(origPtr);
    free(origPath);
}

/**
 * Handles change of directory
 */
void handle_cd(char* input) {
    if (!app->cfs->loaded) {
        return lock_print(ERR_NO_MOUNTED);
    }
    
    if (input != NULL) {
        if (s4354198_is_white_space(input)) {
            return lock_print(PROMPT_ERROR"Invalid directory name\n"PROMPT_RESET);
        }

        char* path = s4354198_path_get_abs(app->cwd, input);
        char* origPath = strdup(path);
        char* origPtr = path;

        char* msg;
        PathInfo* pathInfo = s4354198_path_info();
        bool byDir;

        // Check the path is valid and create directory
        if (s4354198_path(path, &pathInfo, &msg)) {
            if (pathInfo->target == NULL && pathInfo->volume == NULL) {
                lock_print(PROMPT_ERROR"Cannot cd '%s': invalid path\n"PROMPT_RESET, origPath);
            } else if (s4354198_path_taken(pathInfo, &byDir) && !byDir) {
                lock_print(PROMPT_ERROR"Cannot cd '%s': not a directory\n"PROMPT_RESET, origPath);
            } else if (pathInfo->target != NULL && pathInfo->directory != NULL) {
                lock_print(PROMPT_ERROR"Cannot cd '%s': not a directory\n"PROMPT_RESET, origPath);
            } else {
                free(app->cwd);
                app->cwd = strdup(origPath);

                if (app->cwd[strlen(app->cwd)] != '/') {
                    char* newStr = (char*) malloc(sizeof(char) * strlen(app->cwd) + 2);
                    sprintf(newStr, "%s/", app->cwd);
                    free(app->cwd);
                    app->cwd = newStr;
                }
            }
            
            s4354198_path_free_info(pathInfo);
        } else {
            lock_print(PROMPT_ERROR"%s\n"PROMPT_RESET, msg);
            free(msg);
        }
        
        free(origPtr);
        free(origPath);
    } else {
        lock_print(PROMPT_ERROR"No directory name specified\n"PROMPT_RESET);
    }
}

/**
 * Handle creation of directories
 */
void handle_mkdir(char* input) {
    if (!app->cfs->loaded) {
        return lock_print(ERR_NO_MOUNTED);
    }

    if (input != NULL) {
        if (s4354198_is_white_space(input)) {
            return lock_print(PROMPT_ERROR"Invalid directory name\n"PROMPT_RESET);
        }

        char* path = s4354198_path_get_abs(app->cwd, input);
        char* origPath = strdup(path);
        char* origPtr = path;
        
        char* msg;
        PathInfo* pathInfo = s4354198_path_info();
        bool byDir;

        // Check the path is valid and create directory
        if (s4354198_path(path, &pathInfo, &msg)) {
            if (pathInfo->target == NULL) {
                lock_print(PROMPT_ERROR"Cannot create directory '%s': invalid path\n"PROMPT_RESET, origPath);
            } else if (s4354198_path_taken(pathInfo, &byDir)) {
                lock_print(PROMPT_ERROR"Cannot create directory '%s': file exists\n"PROMPT_RESET, origPath);
            } else if (pathInfo->target != NULL && pathInfo->directory != NULL) {
                lock_print(PROMPT_ERROR"Cannot create directory '%s': subdirectories are not supported\n"PROMPT_RESET, origPath);
            } else {
                s4354198_mkdir(pathInfo);
            }
            
            s4354198_path_free_info(pathInfo);
        } else {
            lock_print(PROMPT_ERROR"%s\n"PROMPT_RESET, msg);
            free(msg);
        }
        
        free(origPtr);
        free(origPath);
    } else {
        lock_print(PROMPT_ERROR"No directory name specified\n"PROMPT_RESET);
    }
}

/**
 * Handle creation of files
 */
void handle_touch(char* input) {
    if (!app->cfs->loaded) {
        return lock_print(ERR_NO_MOUNTED);
    }

    if (input != NULL) {
        if (s4354198_is_white_space(input)) {
            return lock_print(PROMPT_ERROR"Invalid filename\n"PROMPT_RESET);
        }

        char* path = s4354198_path_get_abs(app->cwd, input);
        char* origPath = strdup(path);
        char* origPtr = path;
        
        char* msg;
        PathInfo* pathInfo = s4354198_path_info();
        bool byDir;

        // Check the path is valid and create dfile
        if (s4354198_path(path, &pathInfo, &msg)) {
            if (pathInfo->target == NULL) {
                lock_print(PROMPT_ERROR"Cannot create file '%s': invalid path\n"PROMPT_RESET, origPath);
            } else if (s4354198_path_taken(pathInfo, &byDir)) {
                lock_print(PROMPT_ERROR"Cannot create file '%s': file exists\n"PROMPT_RESET, origPath);
            } else if (pathInfo->isDir) {
                lock_print(PROMPT_ERROR"Cannot create file '%s': path specified is a directory\n"PROMPT_RESET, origPath);
            } else {
                s4354198_mkfile(pathInfo);
            }

            s4354198_path_free_info(pathInfo);
        } else {
            lock_print(PROMPT_ERROR"%s\n"PROMPT_RESET, msg);
            free(msg);
        }
        
        free(origPtr);
        free(origPath);
    } else {
        lock_print(PROMPT_ERROR"No filename specified\n"PROMPT_RESET);
    }
}

/**
 * Handles the frame command
 */
void handle_frame(char* input) {
    if (!app->cfs->loaded) {
        return lock_print(ERR_NO_MOUNTED);
    }
    
    if (input != NULL) {
        char* file = strdup(strsep(&input, " "));
        int frame = -1;

        if (s4354198_is_white_space(file)) {
            return lock_print(PROMPT_ERROR"Invalid filename specified\n"PROMPT_RESET);
        }

        char* path = s4354198_path_get_abs(app->cwd, file);
        char* origPath = strdup(path);
        char* origPtr = path;

        char* msg;
        PathInfo* pathInfo = s4354198_path_info();
        bool byDir;

        // Check the path is valid
        bool failed = true;
        if (s4354198_path(path, &pathInfo, &msg)) {
            if (pathInfo->target == NULL) {
                failed = true;
                lock_print(PROMPT_ERROR"Invalid filename '%s': invalid path\n"PROMPT_RESET, origPath);
            } else if (s4354198_path_taken(pathInfo, &byDir)) {
                if (byDir == true) {
                    failed = true;
                    lock_print(PROMPT_ERROR"Invalid filename '%s': file exists\n"PROMPT_RESET, origPath);
                } else {
                    failed = false;
                }
            } else if (pathInfo->isDir) {
                failed = true;
                lock_print(PROMPT_ERROR"Invalid filename '%s': path specified is a directory\n"PROMPT_RESET, origPath);
            } 
        } else {
            lock_print(PROMPT_ERROR"%s\n"PROMPT_RESET, msg);
            failed = true;
        }

        if (failed) {
            s4354198_path_free_info(pathInfo);
            free(origPtr);
            free(origPath);
            return;
        }
        
        if (input != NULL) {
            for (int i = 0; i < strlen(input); i++) {
                if (!isdigit(input[i])) {
                    return lock_print(PROMPT_ERROR"Invalid frame specified '%s'\n"PROMPT_RESET, input);
                }
            }

            frame = atoi(input);
        }

        // Get the file sectors
        int sectors = s4354198_get_file_sector_count(pathInfo);
        
        // Check frame validity
        if (frame > sectors || frame <= 0) {
            return lock_print(PROMPT_ERROR"Invalid frame '%d': '%s' only has %d frames, please specify a frame between 1 and %d\n"PROMPT_RESET,
                frame, origPath, sectors, sectors);
        }

        int* data = s4354198_get_file_frame(pathInfo, frame);

        display_frame(data);

        s4354198_path_free_info(pathInfo); 
        free(origPtr);
        free(file);
        free(origPath);
    } else {
        lock_print(PROMPT_ERROR"No filename specified\n"PROMPT_RESET);
    }
}

/**
 * Handles the recorder command
 */
void handle_rec(char* input) {
    if (!app->cfs->loaded) {
        return lock_print(ERR_NO_MOUNTED);
    }

    if (app->rState == STATE_NOTHING) {
        return lock_print(PROMPT_ERROR"Can't record as recorder has not been initialised\n"PROMPT_RESET);
    }

    if (app->rState != STATE_INIT) {
        return lock_print(PROMPT_ERROR"Recorder is currently either recording or paused, please stop the recording before trying to record again\n"PROMPT_RESET);
    }

    if (app->pState != STATE_INIT && app->pState != STATE_FINISHED) {
        return lock_print(PROMPT_ERROR"Player is currently running, please stop that first\n"PROMPT_RESET);
    }

    if (input != NULL) {
        char* file = strdup(strsep(&input, " "));
        int time = -1;

        if (s4354198_is_white_space(file)) {
            return lock_print(PROMPT_ERROR"Invalid filename specified\n"PROMPT_RESET);
        }

        char* path = s4354198_path_get_abs(app->cwd, file);
        char* origPath = strdup(path);
        char* origPtr = path;

        char* msg;
        PathInfo* pathInfo = s4354198_path_info();
        bool byDir;

        // Check the path is valid
        bool failed = false;
        if (s4354198_path(path, &pathInfo, &msg)) {
            if (pathInfo->target == NULL) {
                failed = true;
                lock_print(PROMPT_ERROR"Invalid filename '%s': invalid path\n"PROMPT_RESET, origPath);
            } else if (s4354198_path_taken(pathInfo, &byDir)) {
                failed = true;
                lock_print(PROMPT_ERROR"Invalid filename '%s': file exists\n"PROMPT_RESET, origPath);
            } else if (pathInfo->isDir) {
                failed = true;
                lock_print(PROMPT_ERROR"Invalid filename '%s': path specified is a directory\n"PROMPT_RESET, origPath);
            } 
        } else {
            lock_print(PROMPT_ERROR"%s\n"PROMPT_RESET, msg);
            failed = true;
        }

        if (failed) {
            s4354198_path_free_info(pathInfo);
            free(origPtr);
            free(origPath);
            return;
        }

        if (input != NULL) {
            for (int i = 0; i < strlen(input); i++) {
                if (!isdigit(input[i])) {
                    return lock_print(PROMPT_ERROR"Invalid time specified '%s'\n"PROMPT_RESET, input);
                }
            }

            time = atoi(input);
        }

        // Make the file
        s4354198_mkfile(pathInfo);

        lock_print_to_recorder("%s %s %d\n", PR_CMD_START, origPath, time);
        lock_print_to_recorder("%s\n", PR_CMD_RESUME);
        app->rState = STATE_STARTED;
        
        s4354198_path_free_info(pathInfo);
        free(origPtr);
        free(file);
        free(origPath);
    } else {
        lock_print(PROMPT_ERROR"No filename specified\n"PROMPT_RESET);
    }
}

/**
 * Handles the size command
 */
void handle_size(char* input) {
    if (!app->cfs->loaded) {
        return lock_print(ERR_NO_MOUNTED);
    }

    if (input != NULL) {
        char* file = strdup(strsep(&input, " "));

        if (s4354198_is_white_space(file)) {
            return lock_print(PROMPT_ERROR"Invalid filename specified\n"PROMPT_RESET);
        }

        char* path = s4354198_path_get_abs(app->cwd, file);
        char* origPath = strdup(path);
        char* origPtr = path;

        char* msg;
        PathInfo* pathInfo = s4354198_path_info();
        bool byDir;

        // Check the path is valid
        bool failed = false;
        if (s4354198_path(path, &pathInfo, &msg)) {
            if (pathInfo->target == NULL) {
                failed = true;
                lock_print(PROMPT_ERROR"Invalid filename '%s': invalid path\n"PROMPT_RESET, origPath);
            } else if (s4354198_path_taken(pathInfo, &byDir) && byDir == true) {
                failed = true;
                lock_print(PROMPT_ERROR"Invalid filename '%s': file exists\n"PROMPT_RESET, origPath);
            } else if (pathInfo->isDir) {
                failed = true;
                lock_print(PROMPT_ERROR"Invalid filename '%s': path specified is a directory\n"PROMPT_RESET, origPath);
            } 
        } else {
            lock_print(PROMPT_ERROR"%s\n"PROMPT_RESET, msg);
            failed = true;
        }

        if (failed) {
            s4354198_path_free_info(pathInfo);
            free(origPtr);
            free(origPath);
            return;
        }

        // Get the file sectors
        int sectors = s4354198_get_file_sector_count(pathInfo);
        lock_print("'%s' is roughly %.3fkB (%d sectors)\n", 
            origPath, (sectors * MAX_WIDTH * MAX_HEIGHT * sizeof(int)) / 1024.0f, sectors);
        
        s4354198_path_free_info(pathInfo); 
        free(origPtr);
        free(file);
        free(origPath);
    } else {
        lock_print(PROMPT_ERROR"No filename specified\n"PROMPT_RESET);
    }
}

/**
 * Handles the free command
 */
void handle_free(char* input) {
    if (!app->cfs->loaded) {
        return lock_print(ERR_NO_MOUNTED);
    }

    int used = s4354198_get_used_sectors();
    int total = CFS_VOLUME_COUNT * CFS_VOLUME_SECTORS;
    int secSize = MAX_WIDTH * MAX_HEIGHT * sizeof(int);

    lock_print("%.3fkB/%.3fkB free (%d/%d sectors used)\n", 
        ((total - used) * secSize) / 1024.0f,
        (total * secSize) / 1024.0f,
        used,
        total
    );
}

/**
 * Handle p keypress
 */
void handle_p(char* input) {
    if (!app->cfs->loaded) {
        return lock_print(ERR_NO_MOUNTED);
    }
    
    if (app->rState == STATE_NOTHING) {
        return lock_print(PROMPT_ERROR"Can't pause as recorder has not been initialised\n"PROMPT_RESET);
    }
    
    if (app->pState == STATE_NOTHING) {
        return lock_print(PROMPT_ERROR"Can't pause as player has not been initialised\n"PROMPT_RESET);
    }

    if (app->rState != STATE_INIT) {
        if (app->rState == STATE_STARTED) {
            lock_print("Recorder paused\n");
            lock_print_to_recorder("%s\n", PR_CMD_PAUSE);
            app->rState = STATE_PAUSED;
        } else if (app->rState == STATE_PAUSED) {
            lock_print("Recorder resumed\n");
            lock_print_to_recorder("%s\n", PR_CMD_RESUME);
            app->rState = STATE_STARTED;
        } else {
            lock_print(PROMPT_ERROR"Invalid recorder state for play/pause, %d\n"PROMPT_ERROR, app->rState);
        }
    } else if (app->pState != STATE_INIT) {
        if (app->pState == STATE_STARTED) {
            lock_print("Player paused\n");
            lock_print_to_player("%s\n", PR_CMD_PAUSE);
            app->pState = STATE_PAUSED;
        } else if (app->pState == STATE_PAUSED) {
            lock_print("Player resumed\n");
            lock_print_to_player("%s\n", PR_CMD_RESUME);
            app->pState = STATE_STARTED;
        } else {
            lock_print(PROMPT_ERROR"Invalid player state for play/pause, %d\n"PROMPT_ERROR, app->pState);
        }
    } else {
        return lock_print(PROMPT_ERROR"Both player and recorder are not in a state to either pause or resume\n"PROMPT_RESET);
    }
}

/**
 * Handles s key press
 */
void handle_s(char* input) {
    if (!app->cfs->loaded) {
        return lock_print(ERR_NO_MOUNTED);
    }
    
    if (app->rState == STATE_NOTHING) {
        return lock_print(PROMPT_ERROR"Can't stop as recorder has not been initialised\n"PROMPT_RESET);
    }
    
    if (app->pState == STATE_NOTHING) {
        return lock_print(PROMPT_ERROR"Can't stop as player has not been initialised\n"PROMPT_RESET);
    }

    if (app->rState != STATE_INIT) {
        if (app->rState == STATE_STARTED) {
            lock_print("Recorder stopped\n");
            lock_print_to_recorder("%s\n", PR_CMD_STOP);
            app->rState = STATE_INIT;
        } else if (app->rState == STATE_PAUSED) {
            lock_print("Recorder stopped\n");
            lock_print_to_recorder("%s\n", PR_CMD_STOP);
            app->rState = STATE_INIT;
        } else {
            lock_print(PROMPT_ERROR"Invalid recorder state for stop, %d\n"PROMPT_ERROR, app->rState);
        }
    } else if (app->pState != STATE_INIT) {
        if (app->pState == STATE_STARTED) {
            lock_print("Player stopped\n");
            lock_print_to_player("%s\n", PR_CMD_STOP);
            app->pState = STATE_INIT;
            fprintf(app->comms->toCag, "%s\n", CMD_START_OUTPUT);
            fflush(app->comms->toCag);
        } else if (app->pState == STATE_PAUSED) {
            lock_print("Player stopped\n");
            lock_print_to_player("%s\n", PR_CMD_STOP);
            app->pState = STATE_INIT;
            fprintf(app->comms->toCag, "%s\n", CMD_START_OUTPUT);
            fflush(app->comms->toCag);
        } else {
            lock_print(PROMPT_ERROR"Invalid player state for stop, %d\n"PROMPT_ERROR, app->pState);
        }
    } else {
        return lock_print(PROMPT_ERROR"Both player and recorder are not in a state to stop\n"PROMPT_RESET);
    }
}

/**
 * Handles play command
 */
void handle_play(char* input) {
    if (!app->cfs->loaded) {
        return lock_print(ERR_NO_MOUNTED);
    }
    
    if (app->pState == STATE_NOTHING) {
        return lock_print(PROMPT_ERROR"Can't play as player has not been initialised\n"PROMPT_RESET);
    }

    if (app->pState != STATE_INIT) {
        return lock_print(PROMPT_ERROR"Player is currently either playing or paused, please stop the player before trying to play again\n"PROMPT_RESET);
    }

    if (app->rState != STATE_INIT && app->rState != STATE_FINISHED) {
        return lock_print(PROMPT_ERROR"Recorder is currently running, please stop that first\n"PROMPT_RESET);
    }

    if (input != NULL) {
        char* file = strdup(strsep(&input, " "));

        if (s4354198_is_white_space(file)) {
            return lock_print(PROMPT_ERROR"Invalid filename specified\n"PROMPT_RESET);
        }

        char* path = s4354198_path_get_abs(app->cwd, file);
        char* origPath = strdup(path);
        char* origPtr = path;

        char* msg;
        PathInfo* pathInfo = s4354198_path_info();
        bool byDir;

        // Check the path is valid
        bool failed = true;
        if (s4354198_path(path, &pathInfo, &msg)) {
            if (pathInfo->target == NULL) {
                failed = true;
                lock_print(PROMPT_ERROR"Invalid filename '%s': invalid path\n"PROMPT_RESET, origPath);
            } else if (s4354198_path_taken(pathInfo, &byDir)) {
                if (byDir == true) {
                    failed = true;
                    lock_print(PROMPT_ERROR"Invalid filename '%s': file exists\n"PROMPT_RESET, origPath);
                } else {
                    failed = false;
                }
            } else if (pathInfo->isDir) {
                failed = true;
                lock_print(PROMPT_ERROR"Invalid filename '%s': path specified is a directory\n"PROMPT_RESET, origPath);
            } 
        } else {
            lock_print(PROMPT_ERROR"%s\n"PROMPT_RESET, msg);
            failed = true;
        }

        if (failed) {
            s4354198_path_free_info(pathInfo);
            free(origPtr);
            free(origPath);
            return;
        }

        if (failed) {
            s4354198_path_free_info(pathInfo);
            free(origPtr);
            free(origPath);
            return;
        }

        // Stop simulation if its running
        fprintf(app->comms->toCag, "%s\n", COMMS_STOP);
        fflush(app->comms->toCag);
        fprintf(app->comms->toCag, "%s\n", CMD_STOP_OUTPUT);
        fflush(app->comms->toCag);

        lock_print_to_player("%s %s\n", PR_CMD_START, origPath);
        lock_print_to_player("%s\n", PR_CMD_RESUME);
        app->pState = STATE_STARTED;
        
        s4354198_path_free_info(pathInfo);
        free(origPtr);
        free(file);
        free(origPath);
    } else {
        lock_print(PROMPT_ERROR"No filename specified\n"PROMPT_RESET);
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
        "                     State: 'alive' or 'dead'\n\n"
        PROMPT_HELP"still <type> <x> <y> "PROMPT_RESET
                             "Draw a still life at x and y coordinates.\n"
        "                     Note x and y are at the top left edge of the\n"
        "                     figure. Types: 'block', 'beehive', 'loaf' and\n"
        "                     'boat'\n\n"
        PROMPT_HELP"osc <type> <x> <y>   "PROMPT_RESET
                             "Draw an oscillator at x and y coordinates.\n"
        "                     Note x and y are at the top left edge of the\n"
        "                     figure. Types: 'blinker', 'toad' and 'beacon'\n\n"
        PROMPT_HELP"ship <type> <x> <y>  "PROMPT_RESET
                             "Draw a spaceship at x and y coordinates. Note\n"
        "                     x and y are at the top left edge of the figure\n"
        "                     Types: 'glider'\n\n"
        PROMPT_HELP"start                "PROMPT_RESET
                             "Start or restart all cellular automation\n\n"
        PROMPT_HELP"stop                 "PROMPT_RESET
                             "Stop all cellular automation\n\n"
        PROMPT_HELP"clear                "PROMPT_RESET
                             "Clear all visual cellular automation\n\n"
        PROMPT_HELP"mount <hdf5 file>    "PROMPT_RESET
                             "Create new or open existing specified HDF5 file\n"
        "                     for the CFS to be used.\n\n"
        PROMPT_HELP"ls [path]            "PROMPT_RESET
                             "List the files and directories in the current\n"
        "                     directory, or path [if supplied].\n\n"
        PROMPT_HELP"cd <directory>       "PROMPT_RESET
                             "Change directory or volume (fv0 or fv1)\n\n"
        PROMPT_HELP"mkdir <directory>    "PROMPT_RESET
                             "Make a directory. Note, this will not make sub-\n"
        "                     directories.\n\n"
        
        PROMPT_HELP"frame <file> <frame> "PROMPT_RESET
                             "Display particular frame of specified file\n\n"
        PROMPT_HELP"rec <file> [time]    "PROMPT_RESET
                             "Starts recording the currently displayed game\n"
        "                     frame until time elapses or [s] or [p] are used.\n\n"
        PROMPT_HELP"size <file>          "PROMPT_RESET
                             "List the estimated size of the file in kilobytes.\n\n"
        PROMPT_HELP"free                 "PROMPT_RESET
                             "Show the estimated free space in kilobytes.\n\n"
        PROMPT_HELP"s                    "PROMPT_RESET
                             "Stops the recording or play through.\n\n"
        PROMPT_HELP"p                    "PROMPT_RESET
                             "Pauses or resumes the recording or play through.\n\n"
        PROMPT_HELP"play <file>          "PROMPT_RESET
                             "Initiate game playback from the specified file.\n\n"
        PROMPT_HELP"help                 "PROMPT_RESET
                             "Displays this list of each command and its usage.\n\n"
        PROMPT_HELP"touch                "PROMPT_RESET
                             "Creates a file without writing anything to it.\n\n"
        PROMPT_HELP"stfu                 "PROMPT_RESET
                             "Toggles output for the drawing subprocesses.\n\n"
        PROMPT_HELP"end                  "PROMPT_RESET
                             "End all processes and threads associated with\n"
        "                     the CAG System (includes User Shell, Cellular\n"
        "                     Automation and Cellular Display), using kill().\n\n"
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
        sleep(1); // Do nothing
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
            cleanedInput = (char*) malloc(sizeof(char) * strlen(input));
            strncpy(cleanedInput, input, strlen(input) - 1);
            cleanedInput[strlen(input) - 1] = '\0';
            
            if (!app->drawingsSTFU) {
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
            }

            free(cleanedInput);
        }
    }

    return NULL;
}

/**
 * Handles the output from the engine process
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
            cleanedInput = (char*) malloc(sizeof(char) * strlen(input));
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
    
                if (globalInput != NULL) {
                    printf("%s", globalInput);
                }
                fflush(stdout);
    
                sem_post(&outputAllowed);
            }

            free(originalPtr);
        }
    }

    return NULL;
}

/**
 * Handles the output from the recorder process
 */
void* recorder_out_handler(void* voidPtr) {
    bool stop = false;
    char* input = NULL;
    //char* endToken;
    char* cleanedInput = NULL;
    size_t size;

    while (!stop) {
        int read = getline(&input, &size, app->comms->fromRecord);

        if (read == -1) {
            stop = true;
        } else {
            // Remove the newline from the end
            cleanedInput = (char*) malloc(sizeof(char) * strlen(input));
            char* originalPtr = cleanedInput;
            strncpy(cleanedInput, input, strlen(input) - 1);
            cleanedInput[strlen(input) - 1] = '\0';

            char* token;
            int inputLength = strlen(cleanedInput) + 1;
            char original[inputLength];
            strncpy(original, cleanedInput, inputLength);

            token = strsep(&cleanedInput, " ");

            if (s4354198_str_match(token, PR_CMD_DONE)) {
                app->rState = STATE_INIT;
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
                printf(PROMPT_TIME"RECORDER: %s\n"PROMPT_RESET, original);
    
                prompt();
    
                if (globalInput != NULL) {
                    printf("%s", globalInput);
                }
                fflush(stdout);
    
                sem_post(&outputAllowed);
            }

            free(originalPtr);
        }
    }

    return NULL;
}

/**
 * Handles the output from the player process
 */
void* player_out_handler(void* voidPtr) {
    bool stop = false;
    char* input = NULL;
    char* cleanedInput = NULL;
    size_t size;

    while (!stop) {
        int read = getline(&input, &size, app->comms->fromPlayer);

        if (read == -1) {
            stop = true;
        } else {
            // Remove the newline from the end
            cleanedInput = (char*) malloc(sizeof(char) * strlen(input));
            char* originalPtr = cleanedInput;
            strncpy(cleanedInput, input, strlen(input) - 1);
            cleanedInput[strlen(input) - 1] = '\0';

            char* token;
            int inputLength = strlen(cleanedInput) + 1;
            char original[inputLength];
            strncpy(original, cleanedInput, inputLength);

            token = strsep(&cleanedInput, " ");

            if (s4354198_str_match(token, PR_CMD_DONE)) {
                app->pState = STATE_INIT;
                fprintf(app->comms->toCag, "%s\n", CMD_START_OUTPUT);
                fflush(app->comms->toCag);
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
                printf(PROMPT_TIME"PLAYER: %s\n"PROMPT_RESET, original);
    
                prompt();
    
                if (globalInput != NULL) {
                    printf("%s", globalInput);
                }
                fflush(stdout);
    
                sem_post(&outputAllowed);
            }

            free(originalPtr);
        }
    }

    return NULL;
}

void handle_halp() {
    char* mds = "\x0D\x0A\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20"
    "\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20"
    "\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20"
    "\x20\x20\x20\x20\x20\x0D\x0A\x20\x20\x20\x20\x20\x20\x20\x20\x20"
    "\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x2E\x3A\x6F\x79\x79\x68"
    "\x64\x64\x6D\x4E\x6D\x68\x79\x79\x6F\x2F\x2D\x60\x20\x20\x20\x20"
    "\x20\x20\x20\x20\x20\x20\x20\x20\x20\x0D\x0A\x20\x20\x20\x20\x20"
    "\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x2D\x6F\x64\x4E\x4E"
    "\x4E\x4E\x4E\x4E\x4E\x4E\x4E\x4E\x4E\x4E\x4E\x4E\x4E\x4E\x4E\x4E"
    "\x68\x2B\x60\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x0D\x0A\x20"
    "\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x60\x2B\x68\x4E"
    "\x4E\x4E\x4E\x4E\x4E\x4E\x4E\x4E\x4E\x4E\x4E\x4E\x4E\x4E\x4E\x4E"
    "\x4E\x4E\x4E\x4E\x4E\x4E\x6D\x2F\x20\x20\x20\x20\x20\x20\x20\x20"
    "\x20\x0D\x0A\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x2E"
    "\x79\x4E\x4E\x4E\x4E\x4E\x4E\x4E\x4E\x4E\x4E\x4E\x4E\x4D\x4D\x4D"
    "\x4E\x4E\x4E\x4D\x4D\x4D\x4D\x4E\x4D\x4E\x4E\x4E\x79\x2D\x20\x20"
    "\x20\x20\x20\x20\x20\x0D\x0A\x20\x20\x20\x20\x20\x20\x20\x20\x20"
    "\x20\x20\x60\x79\x4E\x4E\x4E\x4E\x4E\x4E\x4E\x4E\x4E\x4E\x4E\x4E"
    "\x4E\x4E\x4E\x4E\x4E\x4E\x4E\x4E\x4E\x4E\x4D\x4D\x4D\x4D\x4E\x4D"
    "\x4E\x6D\x60\x20\x20\x20\x20\x20\x20\x0D\x0A\x20\x20\x20\x20\x20"
    "\x20\x20\x20\x20\x20\x60\x79\x4E\x4E\x4E\x4E\x4E\x4D\x4E\x4E\x4E"
    "\x6D\x6D\x6D\x6D\x6D\x6D\x6D\x6D\x6D\x64\x6D\x6D\x6D\x6D\x4E\x4E"
    "\x4E\x4E\x4D\x4D\x4D\x4E\x68\x60\x20\x20\x20\x20\x20\x0D\x0A\x20"
    "\x20\x20\x20\x20\x20\x20\x20\x20\x2E\x73\x4E\x4E\x4E\x4E\x4E\x4E"
    "\x4E\x4E\x6D\x6D\x6D\x6D\x64\x64\x64\x64\x64\x64\x64\x64\x64\x64"
    "\x64\x6D\x6D\x6D\x4E\x4E\x4E\x4E\x4D\x4E\x4E\x2D\x20\x20\x20\x20"
    "\x20\x0D\x0A\x20\x20\x20\x20\x20\x20\x20\x20\x20\x3A\x4E\x4E\x4E"
    "\x4E\x4E\x4E\x4E\x6D\x6D\x6D\x6D\x6D\x64\x64\x64\x68\x68\x68\x68"
    "\x68\x68\x64\x64\x64\x64\x6D\x6D\x6D\x6D\x4E\x4E\x4D\x4D\x4E\x2E"
    "\x20\x20\x20\x20\x20\x0D\x0A\x20\x20\x20\x20\x20\x20\x20\x20\x20"
    "\x2D\x4E\x4E\x4E\x4D\x4E\x4E\x6D\x6D\x6D\x6D\x6D\x64\x64\x64\x68"
    "\x68\x68\x68\x68\x68\x68\x68\x64\x64\x6D\x6D\x6D\x6D\x6D\x4E\x4E"
    "\x4E\x4E\x6D\x20\x20\x20\x20\x20\x20\x0D\x0A\x20\x20\x20\x20\x20"
    "\x20\x20\x20\x20\x60\x4E\x4E\x4D\x4E\x4E\x4E\x6D\x6D\x6D\x6D\x6D"
    "\x64\x64\x64\x64\x68\x68\x68\x68\x68\x68\x68\x64\x64\x6D\x6D\x6D"
    "\x6D\x6D\x6D\x4E\x4E\x4E\x6D\x20\x60\x20\x60\x20\x60\x0D\x0A\x20"
    "\x20\x20\x20\x20\x20\x20\x20\x20\x2E\x6D\x4D\x4D\x4E\x4E\x6D\x6D"
    "\x6D\x6D\x6D\x6D\x64\x64\x64\x68\x68\x79\x79\x68\x68\x68\x68\x68"
    "\x64\x64\x6D\x6D\x6D\x6D\x6D\x6D\x6D\x4E\x6D\x20\x20\x20\x20\x20"
    "\x20\x0D\x0A\x20\x20\x20\x20\x20\x20\x20\x20\x60\x3A\x6D\x4E\x4E"
    "\x4E\x6D\x6D\x6D\x6D\x6D\x6D\x6D\x6D\x6D\x6D\x64\x64\x68\x79\x79"
    "\x79\x68\x68\x64\x6D\x6D\x6D\x6D\x6D\x6D\x6D\x6D\x6D\x4E\x73\x20"
    "\x20\x20\x20\x20\x20\x0D\x0A\x20\x20\x20\x20\x20\x20\x20\x20\x79"
    "\x68\x64\x64\x4E\x4E\x6D\x6D\x6D\x6D\x6D\x6D\x6D\x6D\x6D\x6D\x6D"
    "\x6D\x6D\x64\x68\x68\x64\x6D\x6D\x6D\x6D\x6D\x6D\x6D\x6D\x6D\x6D"
    "\x6D\x4E\x68\x2D\x20\x20\x20\x20\x20\x0D\x0A\x20\x20\x20\x20\x20"
    "\x20\x20\x2F\x6D\x64\x68\x79\x64\x6D\x6D\x6D\x6D\x6D\x6D\x6D\x6D"
    "\x6D\x6D\x64\x64\x6D\x6D\x6D\x68\x64\x6D\x6D\x64\x64\x6D\x6D\x6D"
    "\x4E\x6D\x6D\x6D\x64\x6D\x68\x2B\x60\x60\x60\x20\x60\x0D\x0A\x20"
    "\x20\x20\x20\x20\x20\x20\x60\x4E\x64\x79\x79\x64\x6D\x6D\x6D\x6D"
    "\x64\x64\x64\x64\x64\x64\x64\x64\x6D\x6D\x64\x68\x68\x6D\x6D\x64"
    "\x64\x64\x64\x6D\x6D\x64\x64\x6D\x64\x68\x68\x2F\x20\x20\x20\x20"
    "\x20\x0D\x0A\x20\x20\x20\x20\x20\x20\x20\x20\x79\x6D\x68\x68\x64"
    "\x6D\x6D\x6D\x6D\x64\x64\x64\x68\x68\x68\x64\x64\x6D\x6D\x64\x68"
    "\x64\x6D\x6D\x64\x64\x68\x68\x68\x68\x64\x64\x64\x64\x68\x79\x2F"
    "\x20\x20\x20\x20\x20\x0D\x0A\x20\x20\x20\x20\x20\x20\x20\x20\x2F"
    "\x6D\x64\x64\x6D\x6D\x6D\x6D\x6D\x64\x64\x68\x68\x68\x68\x68\x64"
    "\x6D\x6D\x68\x68\x64\x6D\x6D\x64\x64\x64\x64\x68\x68\x64\x64\x64"
    "\x64\x64\x68\x60\x20\x20\x20\x20\x20\x0D\x0A\x20\x20\x20\x20\x20"
    "\x20\x20\x20\x60\x73\x64\x68\x64\x6D\x6D\x6D\x6D\x64\x64\x68\x68"
    "\x68\x68\x64\x64\x64\x64\x79\x79\x68\x64\x6D\x64\x68\x68\x68\x68"
    "\x64\x64\x64\x64\x64\x68\x73\x20\x20\x20\x20\x20\x20\x0D\x0A\x20"
    "\x20\x20\x20\x20\x20\x20\x20\x20\x60\x2D\x2F\x6D\x6D\x6D\x6D\x6D"
    "\x64\x64\x68\x68\x64\x64\x64\x64\x64\x64\x68\x68\x64\x64\x64\x64"
    "\x68\x68\x68\x68\x64\x64\x64\x64\x68\x79\x2D\x20\x20\x20\x20\x20"
    "\x20\x0D\x0A\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x68"
    "\x6D\x6D\x6D\x6D\x64\x64\x64\x64\x64\x64\x64\x64\x6D\x64\x64\x64"
    "\x64\x6D\x64\x64\x64\x68\x68\x68\x64\x64\x64\x64\x3A\x60\x20\x20"
    "\x60\x20\x20\x20\x20\x0D\x0A\x20\x20\x20\x20\x20\x20\x20\x20\x20"
    "\x20\x20\x20\x2F\x6D\x6D\x6D\x6D\x64\x64\x64\x64\x6D\x6D\x64\x64"
    "\x68\x68\x68\x68\x68\x64\x64\x64\x64\x64\x64\x64\x64\x64\x64\x73"
    "\x20\x20\x20\x20\x60\x20\x20\x20\x20\x0D\x0A\x20\x20\x20\x20\x20"
    "\x20\x20\x20\x20\x20\x20\x20\x2E\x4E\x6D\x6D\x6D\x6D\x64\x64\x64"
    "\x64\x64\x64\x64\x68\x68\x79\x79\x68\x68\x64\x64\x64\x64\x64\x64"
    "\x64\x64\x68\x60\x20\x20\x20\x20\x20\x20\x20\x20\x20\x0D\x0A\x20"
    "\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x3A\x6D\x6D\x6D\x6D"
    "\x6D\x6D\x64\x64\x64\x64\x64\x64\x68\x68\x68\x68\x68\x64\x64\x64"
    "\x64\x64\x64\x64\x64\x73\x60\x20\x20\x20\x20\x20\x20\x20\x20\x20"
    "\x20\x0D\x0A\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x2E\x73"
    "\x79\x6D\x6D\x6D\x6D\x6D\x6D\x6D\x64\x64\x64\x64\x64\x64\x64\x64"
    "\x64\x64\x64\x64\x64\x6D\x6D\x6D\x64\x20\x20\x20\x20\x20\x20\x20"
    "\x20\x20\x20\x20\x20\x0D\x0A\x20\x20\x20\x20\x20\x20\x20\x20\x20"
    "\x60\x2F\x6F\x6F\x73\x64\x64\x64\x64\x6D\x6D\x6D\x6D\x64\x64\x64"
    "\x64\x64\x64\x64\x64\x64\x64\x64\x6D\x6D\x6D\x64\x73\x20\x20\x20"
    "\x20\x20\x20\x20\x20\x20\x20\x20\x20\x0D\x0A\x20\x20\x20\x20\x20"
    "\x20\x20\x20\x2E\x2B\x6F\x6F\x6F\x6F\x73\x64\x64\x64\x64\x64\x6D"
    "\x6D\x6D\x6D\x64\x64\x64\x64\x64\x64\x6D\x6D\x6D\x6D\x64\x64\x64"
    "\x73\x60\x20\x20\x20\x60\x60\x20\x60\x60\x60\x20\x20\x0D\x0A\x20"
    "\x20\x60\x20\x60\x2E\x2D\x3A\x2F\x2B\x6F\x6F\x6F\x6F\x2B\x73\x64"
    "\x64\x64\x68\x68\x64\x64\x6D\x6D\x6D\x6D\x6D\x6D\x6D\x6D\x6D\x64"
    "\x64\x64\x64\x64\x79\x2D\x20\x20\x20\x20\x20\x60\x20\x20\x20\x20"
    "\x20\x0D\x0A\x2D\x3A\x3A\x3A\x2F\x2F\x2F\x2B\x2B\x2B\x2B\x6F\x6F"
    "\x6F\x2B\x2B\x79\x64\x64\x68\x68\x68\x68\x64\x68\x68\x68\x64\x64"
    "\x64\x64\x64\x64\x64\x64\x64\x79\x2B\x2F\x20\x20\x20\x20\x20\x20"
    "\x20\x20\x20\x20\x20\x0D\x0A\x2B\x2B\x2F\x2F\x2F\x2F\x2B\x2B\x2B"
    "\x2B\x2B\x2B\x6F\x2B\x2B\x2B\x2B\x79\x68\x68\x68\x79\x68\x68\x68"
    "\x68\x68\x64\x64\x68\x68\x68\x68\x64\x64\x73\x2B\x2B\x2B\x2D\x20"
    "\x20\x20\x20\x20\x20\x20\x20\x20\x20\x0D\x0A\x2B\x2F\x2F\x2F\x2F"
    "\x2F\x2B\x2B\x2B\x2B\x2B\x2B\x2B\x6F\x2B\x2F\x2B\x6F\x6F\x73\x79"
    "\x79\x79\x79\x68\x68\x68\x68\x68\x68\x68\x68\x64\x68\x73\x6F\x2B"
    "\x2B\x6F\x6F\x2D\x60\x20\x2E\x2D\x2D\x2D\x2D\x2D\x2D\x0D\x0A\x2F"
    "\x2F\x2F\x2F\x2F\x2B\x2B\x2B\x2B\x2B\x2B\x2B\x2B\x2B\x2F\x2F\x2F"
    "\x2B\x2B\x2B\x2B\x6F\x79\x79\x68\x68\x68\x79\x68\x68\x68\x64\x68"
    "\x6F\x2B\x2B\x2B\x2B\x2B\x6F\x2B\x2B\x3A\x3A\x2F\x2B\x2F\x2F\x3A"
    "\x3A\x0D\x0A\x2F\x2F\x2F\x2F\x2B\x2B\x2B\x2B\x2B\x2B\x2F\x2F\x2F"
    "\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x6F\x79\x79\x79\x79\x79\x68"
    "\x68\x79\x6F\x2B\x2B\x2F\x2B\x2B\x2B\x2B\x6F\x2B\x2B\x2B\x2F\x2F"
    "\x2F\x2F\x2F\x2F\x2F\x0D\x0A\x2F\x2F\x2F\x2F\x2B\x2F\x2F\x2F\x2F"
    "\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2B\x79"
    "\x79\x79\x79\x79\x6F\x2F\x2F\x2F\x2F\x2F\x2F\x2B\x2F\x2F\x2B\x2B"
    "\x2B\x2B\x2B\x2B\x2F\x2F\x2F\x2B\x2B\x0D\x0A\x2F\x2F\x2F\x2F\x2F"
    "\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F"
    "\x2F\x3A\x2F\x2B\x79\x79\x6F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F"
    "\x2F\x2F\x2F\x2B\x2B\x2B\x2B\x2B\x2F\x2F\x2F\x2F\x2B\x0D\x0A\x2F"
    "\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F"
    "\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2B\x2F\x2F\x2F\x2F\x2F\x2F\x2F"
    "\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2B\x2B\x2B\x2F\x2F\x2F\x2F\x2F"
    "\x2F\x0D\x0A\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F"
    "\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2B\x2B\x2F\x2F\x2F"
    "\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2B"
    "\x2F\x2F\x2F\x2F\x2F\x0D\x0A\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F"
    "\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F"
    "\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F"
    "\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2F\x2B";

    lock_print(PROMPT_RESET"%s\n\n\n\t\tPlease help me...\n\n", mds);
}