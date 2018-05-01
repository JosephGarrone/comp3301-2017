#ifndef STRUCTS_H
#define STRUCTS_H

#include <pthread.h>
#include <X11/Xlib.h>

typedef enum {
    CELL,
    STILL,
    OSC,
    SHIP
} LifeType;

typedef enum {
    ALIVE,
    DEAD,
    BLOCK,
    BEEHIVE,
    LOAF,
    BOAT,
    BLINKER,
    TOAD,
    BEACON,
    GLIDER
} FormType;

typedef struct {
    int width;
    int height;
    int refreshRate;
} ShellArgs;

typedef struct {
    pid_t display;
    pid_t cag;
    pid_t userShell;
} RuntimeInfo;

typedef struct {
    FILE* toShell;
    FILE* fromShell;
    FILE* toCag;
    FILE* fromCag;
    FILE* toDisplay;
    FILE* fromDisplay;
    FILE* toDrawing;
    FILE* fromDrawing;
    int toShellDescriptor;
    int fromShellDescriptor;
} Comms;

typedef struct DrawingProcess DrawingProcess;
struct DrawingProcess {
    pid_t pid;
    LifeType lifeType;
    FormType formType;
    int x;
    int y;
    int id;
    DrawingProcess* next;
    DrawingProcess* prev;
};

typedef struct LifeForm LifeForm;
struct LifeForm {
    int id;
    int x;
    int y;
    LifeType lifeType;
    FormType formType;
    LifeForm* next;
};

typedef struct {
    int** oldState;
    int** newState;
    pthread_t* threads;
    LifeForm* newLifeForms;    
    bool paused;
} Game;

typedef struct {
    ShellArgs* shellArgs;
    RuntimeInfo* runtimeInfo;
    Comms* comms;
    DrawingProcess* drawProcesses;
    Game* game;
    Display* display;
    Window window;
    GC gc;
    bool readyForDrawing;
    unsigned long *colours;
    unsigned long upMilliseconds;
    int lastId;
} Application;

#endif