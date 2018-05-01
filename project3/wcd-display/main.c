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
#include <X11/Xutil.h>
#include <libmemcached/memcached.h>

#include "../common/s4354198_structs.h"
#include "../common/s4354198_defines.h"
#include "../common/s4354198_utils.h"
#include "../common/s4354198_externs.h"
#include "../common/s4354198_memcached.h"

/* Function prototypes */
void sigint_handler(int signal);
void create_display();
void display_frame(char* frame);
void get_frame(char* frame, int* count);

/* Globals */
// X11 variables
GC gc;
Display* display;
Window window;
XImage* image;
// Contains application data
Application* app;

int main(int argc, char** argv) {
    app = (Application*) malloc(sizeof(Application));

    app->x = atoi(argv[1]);
    app->y = atoi(argv[2]);
    app->width = atoi(argv[3]);
    app->height = atoi(argv[4]);

    signal(SIGINT, sigint_handler);

    app->memc = s4354198_connect_memcached_server();

    char* result = NULL;
    s4354198_wait_not_mem_get(MEM_NEW_DISP, "-", &result);
    app->instance = atoi(result);
    free(result);

    // Update the displays list
    char* displays = s4354198_mem_get(MEM_DISPLAYS);
    char* newDisplays = (char*) malloc(sizeof(char) * strlen(displays) + strlen(result) + 50);
    if (displays[0] == ' ') {
        sprintf(newDisplays, "%s:%d:%d:%d:%d", result, app->x, app->y, app->width, app->height);
    } else {
        sprintf(newDisplays, "%s,%s:%d:%d:%d:%d", displays, result, app->x, app->y, app->width, app->height);
    }
    s4354198_mem_set(MEM_DISPLAYS, newDisplays);

    free(newDisplays);
    free(displays);

    s4354198_p("Display %d started with %d, %d, %d, %d\n", 
        app->instance, app->x, app->y, app->width, app->height);

    create_display();

    char* frame = (char*) malloc(sizeof(char) * (FRAME_WIDTH * FRAME_HEIGHT * 3));
    int count = 0;
    
    // Notify finished startup
    s4354198_mem_set(MEM_NEW_DISP, "-");

    while (1) {
        get_frame(frame, &count);
        display_frame(frame);
    }

    return 0;
}

/**
 * Gets a frame
 */
void get_frame(char* frame, int* count) {
    char filename[25]; 
    sprintf(filename, "%d_tmp_save%d.ppm", app->instance, (*count)++);

    int width, height, depth;
    fscanf(stdin, "P6\n%d %d %d\n", &width, &height, &depth);

    if (width != app->width || height != app->height || depth != FRAME_DEPTH) {
        return;
    }

    fread(frame, sizeof(char), app->width * app->height * 3, stdin);
}

/**
 * Display a frame
 */
void display_frame(char* frame) {
    for (int i = 0; i < app->width * app->height * 3; i += 3) {
        char red = frame[i];
        char green = frame[i + 1];
        char blue = frame[i + 2];

        unsigned long pixel = red << 16 | green << 8 | blue;

        int x = (int)((i / 3) % app->width);
        int y = (int)((i / 3) / app->width);

        if (x >= app->width) {
            x = app->width - 1;
        }

        if (y >= app->height) {
            y = app->height - 1;
            break;
        }

        XPutPixel(image, x, y, pixel);
    }

    XPutImage(display, window, gc, image, 0, 0, 0, 0, app->width, app->height);
}

/**
 * Creates a display
 */
void create_display() {
    display = XOpenDisplay(0);

    int blackColour = BlackPixel(display, DefaultScreen(display));
    int whiteColour = WhitePixel(display, DefaultScreen(display));

    window = XCreateSimpleWindow(display, DefaultRootWindow(display),
        app->x, app->y, app->width, app->height,
        0, blackColour, blackColour);

    XSelectInput(display, window, StructureNotifyMask);

    XMapWindow(display, window);
    XMapRaised(display, window);

    gc = XCreateGC(display, window, 0, 0);

    XSetForeground(display, gc, whiteColour);

    XFlush(display);

    XMoveWindow(display, window, app->x, app->y);

    sleep(1);

    image = XGetImage(display, window, 0, 0, app->width, app->height, AllPlanes, ZPixmap);
}

/**
 * Handles exit 
 */
void sigint_handler(int signal) {
    s4354198_p("Exiting display\n");

    memcached_free(app->memc);

    exit(0);
}