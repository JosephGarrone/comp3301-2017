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
void create_comms(void);
void create_game(void);
void create_threads(void);
void create_semaphores(void);
void lock_print_to_shell(const char* format, ...);
void lock_print_to_display(const char* format, ...);
void *shell_out_handler(void* voidPtr);
void handle_input(char* input);
void handle_new_life_form(char* input);
void *display_out_handler(void* voidPtr);
void run_game_logic(void);
void copy_new_state_to_old(void);
void send_to_display(void);
void add_new_life_forms(void);
void check_for_recently_dead(void);
void *row_logic(void* voidPtr);
void draw_coords(int** state, int id, ...);
bool safe_coords(int y, int x);
int count_neighbours(int row, int i);
int highest_neighbour(int row, int i);
void clear_states(void);

/* Globals */
// Contains application data
Application* app;
// Semaphore for controlling the game loop
sem_t runGame;
// Semaphore for output back to the shell
sem_t sendToShell;
// Semaphore for output to the display
sem_t sendToDisplay;
// Semaphore to control when new life is added
sem_t addLifeForm;
// Semaphores to control the running of threads
sem_t runThreads;
sem_t finishedThreads;
// Thread for the shell output
pthread_t shellOutput;
// Thread for the display output
pthread_t displayOutput;

int main(int argc, char** argv) {
    app = (Application*) malloc(sizeof(Application));
    app->silence = false;

    s4354198_read_args(argc, argv);

    create_comms();

    create_semaphores();

    create_game();

    create_threads();

    run_game_logic();
    
    return 0;
}

/**
 * Create FIFOs and handle FIFO setup
 */
void create_comms(void) {
    app->comms = (Comms*) malloc(sizeof(Comms));

    while (access(FIFO_SHELL_CAG, F_OK) != 0) {
        ; // Wait for the FIFO to be created
    }
    app->comms->fromShell = fopen(FIFO_SHELL_CAG, "r");

    mkfifo(FIFO_CAG_SHELL, FIFO_CAG_SHELL_PERMS);
    app->comms->toShell = fopen(FIFO_CAG_SHELL, "w");

    mkfifo(FIFO_CAG_DISPLAY, FIFO_CAG_DISPLAY_PERMS);
    app->comms->toDisplay = fopen(FIFO_CAG_DISPLAY, "w");

    while (access(FIFO_DISPLAY_CAG, F_OK) != 0) {
        ; // Wait for the FIFO to be created
    }
    app->comms->fromDisplay = fopen(FIFO_DISPLAY_CAG, "r");
    
    mkfifo(FIFO_CAG_CR, FIFO_CAG_CR_PERMS);
    app->comms->toRecord = fopen(FIFO_CAG_CR, "w");
}

/**
 * Creates the game related structs
 */
void create_game(void) {
    app->game = (Game*) malloc(sizeof(Game));
    app->game->paused = true;
    app->game->newLifeForms = NULL;

    app->game->oldState = (int**) malloc(sizeof(int*) * app->shellArgs->height);
    for (int i = 0; i < app->shellArgs->height; i++) {
        app->game->oldState[i] = (int*) malloc(sizeof(int) * app->shellArgs->width);
        for (int j = 0; j < app->shellArgs->width; j++) {
            app->game->oldState[i][j] = 0;
        }
    }

    app->game->newState = (int**) malloc(sizeof(int*) * app->shellArgs->height);
    for (int i = 0; i < app->shellArgs->height; i++) {
        app->game->newState[i] = (int*) malloc(sizeof(int) * app->shellArgs->width);
        for (int j = 0; j < app->shellArgs->width; j++) {
            app->game->newState[i][j] = 0;
        }
    }

    app->game->threads = (pthread_t*) malloc(sizeof(pthread_t) * app->shellArgs->height);
    for (int i = 0; i < app->shellArgs->height; i++) {
        int* arg = (int*) malloc(sizeof(int));
        *arg = i;
        pthread_create(&(app->game->threads[i]), NULL, &row_logic, (void*) arg);
    }
}

/**
 * Create all the required threadsd
 */
void create_threads(void) {
    pthread_create(&shellOutput, NULL, &shell_out_handler, NULL);
    pthread_create(&displayOutput, NULL, &display_out_handler, NULL);
}

/**
 * Create all the semaphores
 */
void create_semaphores(void) {
    sem_init(&runGame, 0, 1);
    sem_init(&sendToShell, 0, 1);
    sem_init(&sendToDisplay, 0, 1);
    sem_init(&addLifeForm, 0, 1);
    sem_init(&runThreads, 0, 0);
    sem_init(&finishedThreads, 0, 0);
}

/**
 * Semaphore locked access to the user shell fifo
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
    va_list args2;
    va_start(args, format);

    va_copy(args2, args);

    sem_wait(&sendToDisplay);
    vfprintf(app->comms->toDisplay, format, args);
    fflush(app->comms->toDisplay);
    vfprintf(app->comms->toRecord, format, args2);
    fflush(app->comms->toRecord);
    sem_post(&sendToDisplay);
    
    va_end(args);
    va_end(args2);
}

/**
 * Handles output from the shell
 */
void* shell_out_handler(void* voidPtr) {
    bool stop = false;
    char* input = NULL;
    char* cleanedInput = NULL;
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

            handle_input(cleanedInput);

            free(cleanedInput);
        }
    }

    return NULL;
}

/**
 * Handles input from the shell
 */
void handle_input(char* input) {
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

    if (s4354198_str_match(token, COMMS_NEW)) {
        handle_new_life_form(input);
    } else if (s4354198_str_match(token, COMMS_STOP)) {
        sem_wait(&runGame);
        app->game->paused = true;
        sem_post(&runGame);
    } else if (s4354198_str_match(token, COMMS_START)) {
        sem_wait(&runGame);
        app->game->paused = false;
        sem_post(&runGame);
    } else if (s4354198_str_match(token, COMMS_CLEAR)) {
        sem_wait(&runGame);
        clear_states();
        sem_post(&runGame);
    } else if (s4354198_str_match(token, CMD_STOP_OUTPUT)) {
        sem_wait(&runGame);
        app->game->paused = true;
        app->silence = true;
        sem_post(&runGame);
    } else if (s4354198_str_match(token, CMD_START_OUTPUT)) {
        sem_wait(&runGame);
        app->silence = false;
        sem_post(&runGame);
    } else {
        error = true;
    }

    if (error) {
        lock_print_to_shell("Unrecognised command (%s)\n", original);
    }
}

/**
 * Handles the creation of a new lifeform
 */
void handle_new_life_form(char* input) {
    int id = -1;
    LifeType lifeType = CELL;
    FormType formType = ALIVE;
    int x = 0;
    int y = 0;
    char* token;
    char* endToken;

    for (int i = 0; i < 5; i++) {
        token = strsep(&input, " ");

        switch (i) {
            case 0:
                id = strtol(token, &endToken, 10);
                break;
            case 1:
                lifeType = (LifeType) strtol(token, &endToken, 10);
                break;
            case 2:
                formType = (FormType) strtol(token, &endToken, 10);
                break;
            case 3:
                x = strtol(token, &endToken, 10);
                break;
            case 4:
                y = strtol(token, &endToken, 10);
                break;
        }
    }

    sem_wait(&addLifeForm);

    LifeForm *new = (LifeForm*) malloc(sizeof(LifeForm));
    new->id = id;
    new->x = x;
    new->y = y;
    new->lifeType = lifeType;
    new->formType = formType;
    new->next = NULL;

    // Add lifeform to the directional linked list of new lifeforms
    if (app->game->newLifeForms == NULL) {
        app->game->newLifeForms = new;
    } else {
        LifeForm *last = app->game->newLifeForms;
        while (last->next != NULL) {
            last = last->next;
        }
        last->next = new;
    }

    sem_post(&addLifeForm);
}

/**
 * Checks if a pair of coordinates are safe
 */
bool safe_coords(int y, int x) {
    return (y >= 0 && y < app->shellArgs->height && x >= 0 && x < app->shellArgs->width);
}

/**
 * Sets all the pairs of the coorindates in the specified state
 */
void draw_coords(int** state, int id, ...) {
    va_list args;
    va_start(args, id);
    int arg1;
    int arg2;

    while((arg1 = va_arg(args, int)) != -1 && (arg2 = va_arg(args, int)) != -1) {
        if (safe_coords(arg1, arg2)) {
            state[arg1][arg2] = id;
        }
    }

    va_end(args);
}

/**
 * Adds new lifeforms to the game state
 */
void add_new_life_forms(void) {
    sem_wait(&addLifeForm);

    LifeForm *lifeForm;
    int** state = app->game->newState;

    // Iterate through all life forms and add them
    while ((lifeForm = app->game->newLifeForms) != NULL) {
        int id = lifeForm->id;
        int x = lifeForm->x;
        int y = lifeForm->y;

        // Change to 0 indexing
        x--;
        y--;

        switch (lifeForm->formType) {
            case ALIVE:
                draw_coords(state, id, y, x, -1);
                break;
            case DEAD:
                draw_coords(state, id, y, x, -1);
                break;
            case BLOCK:
                draw_coords(state, id, y, x, y, x+1, y+1, x, y+1, x+1, -1);
                break;
            case BEEHIVE:
                draw_coords(state, id, y, x+1, y, x+2, y+1, x, y+1, x+3, y+2, x+1, y+2, x+2, -1);
                break;
            case LOAF:
                draw_coords(state, id, y, x+1, y, x+2, y+1, x, y+1, x+3, y+2, x+1, y+2, x+3, y+3, x+2, -1);
                break;
            case BOAT:
                draw_coords(state, id, y, x, y, x+1, y+1, x, y+1, x+2, y+2, x+1, -1);
                break;
            case BLINKER:
                draw_coords(state, id, y, x, y+1, x, y+2, x, -1);
                break;
            case TOAD:
                draw_coords(state, id, y, x+1, y, x+2, y, x+3, y+1, x, y+1, x+1, y+1, x+2, -1);
                break;
            case BEACON:
                draw_coords(state, id, y, x, y, x+1, y+1, x, y+2, x+3, y+3, x+2, y+3, x+3, -1);
                break;
            case GLIDER:
                draw_coords(state, id, y, x, y, x+2, y+1, x+1, y+1, x+2, y+2, x+1, -1);
                break;
        }

        app->game->newLifeForms = lifeForm->next;
        free(lifeForm);
    }

    check_for_recently_dead();

    sem_post(&addLifeForm);
}

/**
 * Checks for ids that are no longer present
 */
void check_for_recently_dead(void) {
    int cells = app->shellArgs->height * app->shellArgs->width;
    int index = 0;
    int oldIds[cells];
    int currentIds[cells];
    int removed[cells];

    // Get list of prev state ids
    for (int i = 0; i < app->shellArgs->height; i++) {
        for (int j = 0; j < app->shellArgs->width; j++) {
            int id = app->game->oldState[i][j];
            oldIds[index++] = id;
        }
    }

    // Get list of current state ids
    index = 0;
    for (int i = 0; i < app->shellArgs->height; i++) {
        for (int j = 0; j < app->shellArgs->width; j++) {
            int id = app->game->newState[i][j];
            currentIds[index++] = id;
        }
    }

    // Find the missing ones
    index = 0;
    for (int i = 0; i < cells; i++) {
        int oldId = oldIds[i];

        bool found = false;
        for (int j = 0; j < cells; j++) {
            int currentId = currentIds[j];

            if (currentId == oldId) {
                found = true;
                break;
            }
        }

        if (!found) {
            removed[index++] = oldId;
        }
    }

    // Notify the shell about the dead ids
    for (int i = 0; i < index; i++) {
        if (removed[i] != 0) {
            lock_print_to_shell("%s %d\n", COMMS_DEAD, removed[i]);
        }
    }
}

/**
 * Handles output from the display
 */
void* display_out_handler(void* voidPtr) {
    bool stop = false;
    char* input = NULL;
    char* cleanedInput = NULL;
    size_t size;

    while (!stop) {
        int read = getline(&input, &size, app->comms->fromDisplay);
        
        if (read == -1) {
            stop = true;
        } else {
            // Remove the newline from the end
            cleanedInput = (char*) malloc(sizeof(char) * strlen(input));
            strncpy(cleanedInput, input, strlen(input) - 1);
            cleanedInput[strlen(input) - 1] = '\0';

            lock_print_to_shell("(Display) %s\n", cleanedInput);

            free(cleanedInput);
        }
    }

    return NULL;
}

/**
 * Sends the current game state to the display
 */
void send_to_display(void) {
    char* messages[app->shellArgs->height];
    int totalSize = 0;

    for (int i = 0; i < app->shellArgs->height; i++) {
        int size = BUFFER_SIZE;
        int index = 0;
        char* message = (char*) malloc(sizeof(char) * size);

        for (int j = 0; j < app->shellArgs->width; j++) {
            index += sprintf(&message[index], "%d", app->game->newState[i][j]);

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
 * Clears all states
 */
void clear_states(void) {
    // Get list of current state ids
    for (int i = 0; i < app->shellArgs->height; i++) {
        for (int j = 0; j < app->shellArgs->width; j++) {
            int id = app->game->newState[i][j];
            lock_print_to_shell("%s %d\n", COMMS_DEAD, id);
        }
    }

    // Zero everything
    for (int i = 0; i < app->shellArgs->height; i++) {
        for (int j = 0; j < app->shellArgs->width; j++) {
            app->game->oldState[i][j] = 0;
            app->game->newState[i][j] = 0;
        }
    }
}

/**
 * Runs the game logic
 */
void run_game_logic(void) {
    bool stop = false;

    while (!stop) {
        sem_wait(&runGame);

        // If game is not paused, then unlock the sem 10 times
        if (!app->game->paused) {
            for (int i = 0; i < app->shellArgs->height; i++) {
                sem_post(&runThreads);
            }
            // Synchronise at end of all the threads
            for (int i = 0; i < app->shellArgs->height; i++) {
                sem_wait(&finishedThreads);
            }
        }

        add_new_life_forms();

        if (!app->silence) {
            send_to_display();
        }

        copy_new_state_to_old();

        sem_post(&runGame);

        usleep(app->shellArgs->refreshRate * 1000);
    }
}

/**
 * Copies the new state to the old state
 */
void copy_new_state_to_old(void) {
    for (int i = 0; i < app->shellArgs->height; i++) {
        for (int j = 0; j < app->shellArgs->width; j++) {
            app->game->oldState[i][j] = app->game->newState[i][j];
        }
    }
}

/**
 * Handles the individual row logic
 */
void* row_logic(void* voidPtr) {
    int row = *((int*) voidPtr);
    free((int*) voidPtr);
    bool stop = false;
    int width = app->shellArgs->width;

    while (!stop) {
        sem_wait(&runThreads);
        
        int** newState = app->game->newState;
        int* myState = newState[row];

        for (int i = 0; i < width; i++) {
            int id = myState[i];

            int neighbours = count_neighbours(row, i);
            int highest = highest_neighbour(row, i);

            if (id > 0) {
                if (neighbours < 2) { // Under population
                    myState[i] = 0;
                } else if (neighbours == 2 || neighbours == 3) { // Survive and take
                    myState[i] = highest;
                } else if (neighbours > 3) { // Over population
                    myState[i] = 0;
                }
            } else {
                if (neighbours == 3) {
                    myState[i] = highest;
                }
            }
        }

        sem_post(&finishedThreads);

        usleep(50000);
    }

    return NULL;
}

/**
 * Counts the neighbours of a cell
 */
int count_neighbours(int row, int i) {
    int count = 0;

    int** state = app->game->oldState;

    // All the coords to check
    int pairs[8][2] = {
        {row - 1, i - 1},
        {row - 1, i},
        {row - 1, i + 1},
        {row, i - 1},
        {row, i + 1},
        {row + 1, i - 1},
        {row + 1, i},
        {row + 1, i + 1}
    };

    // Check each coord
    for (int j = 0; j < 8; j++) {
        int y = pairs[j][0];
        int x = pairs[j][1];

        if (safe_coords(y, x)) {
            if (state[y][x] != 0) {
                count++;
            }
        }
    }

    //lock_print_to_shell("Count %d for %d,%d\n", count, i, row);

    return count;
}

/**
 * Get the value of the highest neighbour
 */
int highest_neighbour(int row, int i) {
    int max = 0;
    
    int** state = app->game->oldState;

    // All the coords to check
    int pairs[8][2] = {
        {row - 1, i - 1},
        {row - 1, i},
        {row - 1, i + 1},
        {row, i - 1},
        {row, i + 1},
        {row + 1, i - 1},
        {row + 1, i},
        {row + 1, i + 1}
    };

    // Check each coord
    for (int j = 0; j < 8; j++) {
        int y = pairs[j][0];
        int x = pairs[j][1];

        if (safe_coords(y, x)) {
            int id = state[y][x];
            
            if (id > max) {
                max = id;
            }
        }
    }

    return max;
}