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

/* Colours global */
#define TOTAL_COLOURS 497
unsigned long black = 0x000000;
unsigned long white = 0xFFFFFF;
unsigned long colours[TOTAL_COLOURS] = {
    0x295f99,
    0xff7f00,
    0xffff00,
    0x7f007f,
    0x00a833,
    0xff0000,
    0xe0e8f0,
    0x502600,
    0x4875a7,
    0xe17000,
    0xffffda,
    0x3d1412,
    0x23698a,
    0xff6d00,
    0xffff24,
    0x74036d,
    0x059d41,
    0xff1200,
    0xdaf207,
    0x91006d,
    0x24b42b,
    0xec0012,
    0x355195,
    0xff9100,
    0x07932b,
    0xff2424,
    0xffdada,
    0x2b2c07,
    0xffec00,
    0x730d83,
    0xc2d1e1,
    0x6d3500,
    0x668cb6,
    0xc46100,
    0xffffb6,
    0x481124,
    0x1d747b,
    0xff5b00,
    0xffff48,
    0x69065b,
    0x0b9350,
    0xff2400,
    0xb6e60e,
    0xa3005b,
    0x48c124,
    0xda0024,
    0xffb6b6,
    0x24410e,
    0x424391,
    0xffa300,
    0xffda00,
    0x661b86,
    0x0e7f24,
    0xff4848,
    0x91d915,
    0xb60048,
    0x6dcd1d,
    0xc80036,
    0xffc800,
    0x5a288a,
    0x156a1d,
    0xff6d6d,
    0xffb600,
    0x4e368e,
    0x1d5515,
    0xff9191,
    0xdbf128,
    0x88055d,
    0x29ab3d,
    0xeb100f,
    0xffea1f,
    0x691271,
    0x0d8a3a,
    0xff341f,
    0xdbe006,
    0x871075,
    0x2aa025,
    0xeb2131,
    0xff8205,
    0x2f5a86,
    0x352818,
    0xffddbb,
    0xe08301,
    0x52659f,
    0x493b06,
    0xe2c6d0,
    0xa75300,
    0x85a3c4,
    0x8a4400,
    0xa3bad3,
    0xe36002,
    0x427e95,
    0x59210f,
    0xdfe9cf,
    0x11885e,
    0xff3600,
    0xffff6d,
    0x5e0a48,
    0xffff91,
    0x530d36,
    0x177e6d,
    0xff4800,
    0xb8d30c,
    0x9b1468,
    0xb7e32c,
    0x9c074e,
    0x4db939,
    0xd70d1f,
    0x4ead1f,
    0xd71d3f,
    0xffd51a,
    0x5d2075,
    0xdccd05,
    0x7c217d,
    0x308c1f,
    0xe94251,
    0xdf9502,
    0x5d5496,
    0xdcef49,
    0x7e0b4e,
    0x2ea24f,
    0xe9200d,
    0xffe73e,
    0x5e165f,
    0x148049,
    0xff431a,
    0x157634,
    0xff553e,
    0x434f0c,
    0xe4a5b0,
    0xff960a,
    0x3a4c82,
    0x2d3b1f,
    0xffbb9c,
    0xc27402,
    0x6f78a9,
    0x684905,
    0xc5b3c6,
    0xc0d4c3,
    0x752d0d,
    0x6294a1,
    0xc75305,
    0xdeebad,
    0x621b1f,
    0x3d8784,
    0xe45005,
    0xffdf9c,
    0x40232a,
    0xff720a,
    0x286477,
    0x94c612,
    0xaf175a,
    0xdcdc23,
    0x7e1665,
    0x94d530,
    0xaf093e,
    0x70c735,
    0xc30b2e,
    0x71b918,
    0xc31a4d,
    0x309736,
    0xe92f2a,
    0xddba04,
    0x723286,
    0xdea803,
    0x67438e,
    0x377718,
    0xe76371,
    0x3d6312,
    0xe58491,
    0xa46603,
    0x8c8cb3,
    0x865704,
    0xa89fbc,
    0xffc014,
    0x512f79,
    0x1d632d,
    0xff775d,
    0xffab0f,
    0x463d7d,
    0x254f26,
    0xff997c,
    0xe27508,
    0x4c6d8d,
    0xac4707,
    0x81a9ac,
    0x903a0a,
    0xa0beb8,
    0x523516,
    0xe1cab3,
    0xddee6b,
    0x75103e,
    0xffe45d,
    0x541a4e,
    0x1b7758,
    0xff5314,
    0x339960,
    0xe8300a,
    0x4a1f3c,
    0xfee27c,
    0xddec8c,
    0x6c162e,
    0x389072,
    0xe64007,
    0x216d68,
    0xff620f,
    0xbac00a,
    0x922875,
    0xb9e04a,
    0x940f41,
    0x51b24e,
    0xd41b1a,
    0x53991a,
    0xd33b5a,
    0xffd034,
    0x532664,
    0x1d6e43,
    0xff6234,
    0xbed7a5,
    0x7c261a,
    0x5e9b8c,
    0xca450a,
    0x625d0a,
    0xc995ab,
    0x335472,
    0xff8914,
    0x373630,
    0xffc082,
    0x78649c,
    0xc08704,
    0x2c5d62,
    0xff7c1f,
    0x47767c,
    0xe3670f,
    0x368f48,
    0xe83d23,
    0x246553,
    0xff6f29,
    0x265b3d,
    0xff824e,
    0x2e4837,
    0xffa168,
    0x3e456d,
    0xffa11f,
    0x378430,
    0xe74e46,
    0x493569,
    0xffb829,
    0x4a2b53,
    0xffca4e,
    0x59a377,
    0xce370f,
    0x55aa62,
    0xd12914,
    0x52a532,
    0xd42a36,
    0x403042,
    0xffc568,
    0x565c85,
    0xe1890d,
    0x588514,
    0xd05975,
    0x74c14d,
    0xbe1727,
    0x4b481c,
    0xe2ab98,
    0x6a8095,
    0xc5680a,
    0x75a614,
    0xbd3563,
    0x5d710f,
    0xcc7790,
    0x7eaf94,
    0xb03b0f,
    0x96d04b,
    0xa91334,
    0x97b30f,
    0xa82e6c,
    0x80508f,
    0xbe9a06,
    0x74286d,
    0xddc71d,
    0x5b2e26,
    0xe0ce96,
    0x9ec39d,
    0x963014,
    0x751c55,
    0xddd840,
    0x9374a1,
    0xa17906,
    0x893c82,
    0xbcad08,
    0x826b08,
    0xae85a6,
    0xbcda87,
    0x841e27,
    0xbbdd69,
    0x8c1634,
    0xbace27,
    0x931b59,
    0xc3b7ac,
    0x6f4113,
    0x3b8659,
    0xe64b1c,
    0x417e6b,
    0xe55916,
    0x2e544d,
    0xfe8c3e,
    0x364c5d,
    0xff962e,
    0x7ab57c,
    0xb52f17,
    0x77bb64,
    0xba231f,
    0x75b32e,
    0xbe2542,
    0x3e7029,
    0xe66d61,
    0x9bc782,
    0x9d271f,
    0x99cc67,
    0xa31d29,
    0x403c58,
    0xfeb03e,
    0x374247,
    0xfea953,
    0x88929d,
    0xa85b0d,
    0x445c23,
    0xe48c7d,
    0x799210,
    0xb84f79,
    0x97c12b,
    0xa9204e,
    0x604a7d,
    0xdf9e12,
    0x9ba00c,
    0xa1467d,
    0x7d7f0c,
    0xb36a90,
    0x6a3975,
    0xdeb318,
    0x6c2245,
    0xded55d,
    0x642835,
    0xdfd179,
    0x9a5d8f,
    0x9e8c09,
    0x8c4e10,
    0xa5a5a4,
    0x53422c,
    0xe1b17f,
    0x3d7c41,
    0xe65a3a,
    0x6c2e5d,
    0xdec236,
    0x506474,
    0xe27d18,
    0x763a21,
    0xc1bc91,
    0x695519,
    0xc69b94,
    0x8c234b,
    0xbcca41,
    0x58912c,
    0xd0464e,
    0x8b2f65,
    0xbcba21,
    0x579e46,
    0xd1362d,
    0x658781,
    0xc85b13,
    0x726c89,
    0xc37c10,
    0x4c5533,
    0xe39468,
    0x5c3b3d,
    0xe0b667,
    0x64354d,
    0xdfbc4e,
    0x45683a,
    0xe57751,
    0x7d322f,
    0xbfc176,
    0x852a3d,
    0xbdc55c,
    0x63691f,
    0xca7f7d,
    0x634065,
    0xe0ab2c,
    0x59526c,
    0xe19422,
    0x4a6c63,
    0xe37224,
    0x437452,
    0xe5662f,
    0x834371,
    0xbea51b,
    0x5e7d26,
    0xcd6265,
    0x7b587d,
    0xc09116,
    0x91451c,
    0xa3aa8c,
    0x876216,
    0xaa8b90,
    0xa32941,
    0x9abb43,
    0xa2375d,
    0x9bac25,
    0xad5018,
    0x849987,
    0xa56f13,
    0x8e7b8d,
    0x5c965a,
    0xce4325,
    0x618f6e,
    0xcb4f1c,
    0x799f28,
    0xb93f56,
    0x79ac44,
    0xba3037,
    0x704e28,
    0xc4a17d,
    0x544e44,
    0xe29b54,
    0x4b614b,
    0xe38142,
    0x5b4754,
    0xe1a340,
    0x82761c,
    0xaf727d,
    0x973c29,
    0xa0b073,
    0x5d8a3f,
    0xce5141,
    0x7e8b22,
    0xb45869,
    0xa18419,
    0x95657d,
    0x52595c,
    0xe28a32,
    0x843756,
    0xbeb438,
    0x9e981f,
    0x9c4e6d,
    0x9d3335,
    0x9db55b,
    0xb14522,
    0x809f71,
    0xc5721c,
    0x6d7376,
    0xb53b2d,
    0x7ca65a,
    0x6a6230,
    0xc78769,
    0x774638,
    0xc2a766,
    0x8c5a24,
    0xa7927b,
    0xa96620,
    0x8a8279,
    0x647637,
    0xcb6c55,
    0x7e3f47,
    0xc0ae4f,
    0xc86729,
    0x687b64,
    0xc38826,
    0x755f6c,
    0x9d404f,
    0x9ea63a,
    0x7e983c,
    0xb54848,
    0xcb5c35,
    0x638251,
    0x7d4b61,
    0xc19e2f,
    0x925133,
    0xa49865,
    0x876e2c,
    0xac796a,
    0xad5c2e,
    0x868a65,
    0xa57b29,
    0x906c6b,
    0x974841,
    0xa19f50,
    0x705a40,
    0xc58e56,
    0xb1523b,
    0x829151,
    0x765351,
    0xc39642,
    0x838334,
    0xb16159,
    0x696e49
};

/* Function prototypes */
void await_comms(void);
void create_threads(void);
void create_display(void);
void update_screen(char* input);
void lock_print_to_cag(const char* format, ...);
void *cag_output_handler(void* voidPtr);
void *player_output_handler(void* voidPtr);

/* Globals */
// Contains application data
Application* app;
// Cag output thread
pthread_t cagOutput;
// Player output thread
pthread_t playerOutput;
// Semaphore for output to the cag
sem_t sendToCag;

int main(int argc, char** argv) {
    app = (Application*) malloc(sizeof(Application));
    app->readyForDrawing = false;

    sem_init(&sendToCag, 0, 1);

    s4354198_read_args(argc, argv);

    await_comms();

    create_threads();

    create_display();

    // Stop doing work - wait for the FIFO thread to end
    pthread_join(cagOutput, NULL);
    pthread_join(playerOutput, NULL);
    
    return 0;
}

/**
 * Wait for comms to be created
 */
void await_comms(void) {
    app->comms = (Comms*) malloc(sizeof(Comms));
    
    while (access(FIFO_CAG_DISPLAY, F_OK) != 0) {
        ; // Wait for the FIFO to be created
    }
    app->comms->fromCag = fopen(FIFO_CAG_DISPLAY, "r");

    mkfifo(FIFO_DISPLAY_CAG, FIFO_DISPLAY_CAG_PERMS);
    app->comms->toCag = fopen(FIFO_DISPLAY_CAG, "w");
    
    while (access(FIFO_CP_DISPLAY, F_OK) != 0) {
        ; // Wait for the FIFO to be created
    }
    app->comms->fromPlayer = fopen(FIFO_CP_DISPLAY, "r");
}

/**
 * Create the threads
 */
void create_threads(void) {
    pthread_create(&cagOutput, NULL, &cag_output_handler, NULL);
    pthread_create(&playerOutput, NULL, &player_output_handler, NULL);
}

/**
 * Creates the X11 window
 */
void create_display(void) {
    app->display = XOpenDisplay(0);

    int blackColour = BlackPixel(app->display, DefaultScreen(app->display));
    int whiteColour = WhitePixel(app->display, DefaultScreen(app->display));

    app->window = XCreateSimpleWindow(app->display, DefaultRootWindow(app->display),
        0, 0, app->shellArgs->width * CELL_SIDE, app->shellArgs->height * CELL_SIDE,
        0, blackColour, blackColour);

    XSelectInput(app->display, app->window, StructureNotifyMask);

    XMapWindow(app->display, app->window);

    app->gc = XCreateGC(app->display, app->window, 0, 0);

    XSetForeground(app->display, app->gc, whiteColour);

    XFlush(app->display);

    for (int i = 0; i < app->shellArgs->height; i++) {
        for (int j = 0; j < app->shellArgs->width; j++) {
            XSetForeground(app->display, app->gc, blackColour);
            XFillRectangle(app->display, app->window, app->gc, j * 20, i * 20, CELL_SIDE, CELL_SIDE);
            
            XFlush(app->display);
        }
    }

    app->readyForDrawing = true;
}

/**
 * Handler for CAG output
 */
void* cag_output_handler(void* voidPtr) {
    bool stop = false;
    char* input = NULL;
    char* cleanedInput = NULL;
    size_t size;

    while (!stop) {
        int read = getline(&input, &size, app->comms->fromCag);

        if (read == -1) {
            stop = true;
        } else {
            // Remove the newline from the end
            cleanedInput = (char*) malloc(sizeof(char) * strlen(input));
            strncpy(cleanedInput, input, strlen(input) - 1);
            cleanedInput[strlen(input) - 1] = '\0';

            if (app->readyForDrawing) {
                update_screen(cleanedInput);
            }

            free(cleanedInput);
        }
    }

    return NULL;
}

/**
 * Handler for player output
 */
void* player_output_handler(void* voidPtr) {
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
            strncpy(cleanedInput, input, strlen(input) - 1);
            cleanedInput[strlen(input) - 1] = '\0';

            if (app->readyForDrawing) {
                update_screen(cleanedInput);
            }

            free(cleanedInput);
        }
    }

    return NULL;
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
 * Updates the screen
 */
void update_screen(char *input) {
    char* token;
    char* endToken;
    int inputLength = strlen(input) + 1;
    char original[inputLength];
    strncpy(original, input, inputLength);

    int y = 0;
    int x = 0;
    int width = app->shellArgs->width;
    while ((token = strsep(&input, ",")) != NULL) {
        int id = strtol(token, &endToken, 10);

        if (id == 0) {
            XSetForeground(app->display, app->gc, black);
        } else {
            XSetForeground(app->display, app->gc, colours[id % TOTAL_COLOURS]);
        }
        XFillRectangle(app->display, app->window, app->gc, x * 20, y * 20, CELL_SIDE, CELL_SIDE);
        
        XFlush(app->display);

        x++;
        if (x == width) {
            x = 0;
            y++;
        }
    }
}