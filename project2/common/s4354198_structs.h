#ifndef STRUCTS_H
#define STRUCTS_H

#include <pthread.h>
#include <X11/Xlib.h>

#include "hdf5.h"

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

typedef enum {
    TYPE_FILE,
    TYPE_DIR,
    TYPE_VOLUME
} NodeType;

typedef enum {
    STATE_NOTHING,
    STATE_INIT,
    STATE_STARTED,
    STATE_PAUSED,
    STATE_FINISHED
} StateType;

typedef struct {
    int width;
    int height;
    int refreshRate;
} ShellArgs;

typedef struct {
    pid_t display;
    pid_t cag;
    pid_t userShell;
    pid_t recorder;
    pid_t player;
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
    FILE* toRecord;
    FILE* fromRecord;
    FILE* toPlayer;
    FILE* fromPlayer;
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
    char* filename;
    bool loaded;
    hid_t file;
} CFSInfo;

typedef struct {
    char* volume;
    char* directory;
    char* target;
    bool isDir;
    bool exists;
} PathInfo;

typedef struct {
    ShellArgs* shellArgs;
    RuntimeInfo* runtimeInfo;
    Comms* comms;
    DrawingProcess* drawProcesses;
    Game* game;
    Display* display;
    Window window;
    CFSInfo* cfs;
    StateType pState;
    StateType rState;
    GC gc;
    bool readyForDrawing;
    bool drawingsSTFU;
    unsigned long *colours;
    unsigned long upMilliseconds;
    int lastId;
    int duration;
    int frame;
    int maxFrame;
    char* cwd;
    char* prFile;
    bool silence;
} Application;

typedef struct INode INode;
struct INode {
    char* name;
    char* type;
    char* sectors;
    char* timestamp;
    char* mode;
    char* owner;
    INode* prev;
    INode* next;
    PathInfo* info;
};

typedef struct {
    char* result;
    int inode;
    char* niceName;
    char* actualName;
    PathInfo* pathInfo;
} FindDirInfo;

#endif