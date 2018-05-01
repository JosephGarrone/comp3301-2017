#ifndef STRUCTS_H
#define STRUCTS_H

#include <libmemcached/memcached.h>

#define MAX_DISPLAYS 5

typedef struct {
    pid_t control;
    pid_t remote;
    pid_t memcached;
    pid_t display[MAX_DISPLAYS];
    pid_t avconv[MAX_DISPLAYS];
} RuntimeInfo;

typedef struct {
    FILE* toControl;
    FILE* fromControl;
    FILE* toRemote;
    FILE* fromRemote;
    FILE* toMem;
    FILE* fromMem;
    FILE* toAv[MAX_DISPLAYS];
    FILE* fromDisplay[MAX_DISPLAYS];
    int toAvFd[MAX_DISPLAYS];
    int toDisplayFd[MAX_DISPLAYS];
} Comms;

typedef struct {
    RuntimeInfo* runtimeInfo;
    Comms* comms;
    memcached_st* memc;
    int instance;
    int* displays;
    int width;
    int height;
    int x;
    int y;
} Application;

#endif