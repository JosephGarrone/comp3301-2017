#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include <semaphore.h>
#include <stdarg.h>
#include <errno.h>
#include <libmemcached/memcached.h>

#include "../common/s4354198_structs.h"
#include "../common/s4354198_defines.h"
#include "../common/s4354198_utils.h"
#include "../common/s4354198_externs.h"
#include "../common/s4354198_memcached.h"

/* Function prototypes */
void sigint_handler(int signal);
char get_key(char* keyVal);
bool can_rotate(char key);
char rotate_key(char rotKey, char key);
void send_key(char key);
unsigned long long get_milli();

/* Globals */

// Contains application data
Application* app;

int main(int argc, char** argv) {
    app = (Application*) malloc(sizeof(Application));
    app->memc = s4354198_connect_memcached_server();
    
    signal(SIGINT, sigint_handler);

    // Open stream
    FILE* irwOutput;
    int fd;
    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, "/var/run/lirc/lircd");
    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    connect(fd, (struct sockaddr*)&addr, sizeof(addr));
    irwOutput = fdopen(fd, "r");
    // fprintf(stderr, "Errno %d\n", errno);
    
    // Read key presses
    char lastKey = ' ';
    char rotKey = ' ';
    unsigned long long lastMilli = 0;
    while (1) {
        char hex[50], rand[50], keyVal[50], conf[50];
        fscanf(irwOutput, "%s %s %s %s\n", &hex, &rand, &keyVal, &conf);
        char key = get_key(keyVal);

        if (can_rotate(key)) {
            if (lastKey == key) {
                if (get_milli() - lastMilli <= 1000) {
                    rotKey = rotate_key(rotKey, key);
                } else { // time expired, so send both
                    if (rotKey != ' ') {
                        send_key(rotKey);
                    }
                    send_key(key);
                    rotKey = ' ';
                }
            } else if (rotKey == ' ') {
                rotKey = key;
            } else {
                if (rotKey != ' ') {
                    send_key(rotKey);
                } else {
                    send_key(lastKey);
                }
                rotKey = key;
            }
        } else {
            if (rotKey != ' ') {
                send_key(rotKey);
            }
            rotKey = ' ';
            send_key(key);
        }

        lastKey = key;
        lastMilli = get_milli();

        s4354198_p("LIRC: %s %s %s %s\n", hex, rand, keyVal, conf);
    }

    return 0;
}

/**
 * Sends a key
 */
void send_key(char key) {
    char* result = NULL;
    s4354198_wait_mem_get(MEM_KEY, "-", &result);
    free(result);
    
    char keyStr[2];
    sprintf(keyStr, "%c", key);
    s4354198_mem_set(MEM_KEY, keyStr);
}

/**
 * Rotates a character
 */
char rotate_key(char rotKey, char key) {
    if (key == '2') {
        if (rotKey == '2') {
            return 'A';
        } else if (rotKey == 'A') {
            return 'B';
        } else if (rotKey == 'B') {
            return 'C';
        } else if (rotKey == 'C') {
            return '2';
        } else {
            return '2';
        }
    } else if (key == '3') {
        if (rotKey == '3') {
            return 'D';
        } else if (rotKey == 'D') {
            return 'E';
        } else if (rotKey == 'E') {
            return 'F';
        } else if (rotKey == 'F') {
            return '3';
        } else {
            return '3';
        }
    } else if (key == '4') {
        if (rotKey == '4') {
            return 'G';
        } else if (rotKey == 'G') {
            return 'H';
        } else if (rotKey == 'H') {
            return 'I';
        } else if (rotKey == 'I') {
            return '4';
        } else {
            return '4';
        }
    } else if (key == '5') {
        if (rotKey == '5') {
            return 'J';
        } else if (rotKey == 'J') {
            return 'K';
        } else if (rotKey == 'K') {
            return 'L';
        } else if (rotKey == 'L') {
            return '5';
        } else {
            return '5';
        }
    } else if (key == '6') {
        if (rotKey == '6') {
            return 'M';
        } else if (rotKey == 'M') {
            return 'N';
        } else if (rotKey == 'N') {
            return 'O';
        } else if (rotKey == 'O') {
            return '6';
        } else {
            return '6';
        }
    } else if (key == '7') {
        if (rotKey == '7') {
            return 'P';
        } else if (rotKey == 'P') {
            return 'Q';
        } else if (rotKey == 'Q') {
            return 'R';
        } else if (rotKey == 'R') {
            return 'S';
        } else if (rotKey == 'S') {
            return '7';
        } else {
            return '7';
        }
    } else if (key == '8') {
        if (rotKey == '8') {
            return 'T';
        } else if (rotKey == 'T') {
            return 'U';
        } else if (rotKey == 'U') {
            return 'V';
        } else if (rotKey == 'V') {
            return '8';
        } else {
            return '8';
        }
    } else if (key == '9') {
        if (rotKey == '9') {
            return 'W';
        } else if (rotKey == 'W') {
            return 'X';
        } else if (rotKey == 'X') {
            return 'Y';
        } else if (rotKey == 'Y') {
            return 'Z';
        } else if (rotKey == 'Z') {
            return '9';
        } else {
            return '9';
        }
    }

    return ' ';
}

/**
 * Get the time in milliseconds
 */
unsigned long long get_milli() {
    struct timeval time;
    gettimeofday(&time, NULL);
    return (unsigned long long)(time.tv_sec) * 1000 +
        (unsigned long long)(time.tv_usec) / 1000;
}

/**
 * Determines whether a key can be rotated
 */
bool can_rotate(char key) {
    return (key >= '2' && key <= '9');
}

/**
 * Convert a key code into a letter
 */
char get_key(char* keyVal) {
    if (s4354198_str_match(keyVal, "KEY_0")) {
        return '0';
    } else if (s4354198_str_match(keyVal, "KEY_1")) {
        return '1';
    } else if (s4354198_str_match(keyVal, "KEY_2")) {
        return '2';        
    } else if (s4354198_str_match(keyVal, "KEY_3")) {
        return '3';        
    } else if (s4354198_str_match(keyVal, "KEY_4")) {
        return '4';        
    } else if (s4354198_str_match(keyVal, "KEY_5")) {
        return '5';        
    } else if (s4354198_str_match(keyVal, "KEY_6")) {
        return '6';        
    } else if (s4354198_str_match(keyVal, "KEY_7")) {
        return '7';        
    } else if (s4354198_str_match(keyVal, "KEY_8")) {
        return '8';        
    } else if (s4354198_str_match(keyVal, "KEY_9")) {
        return '9';        
    } else if (s4354198_str_match(keyVal, "KEY_VOLUMEUP")) {
        return '@';        
    } else if (s4354198_str_match(keyVal, "KEY_VOLUMEDOWN")) {
        return '$';        
    } else if (s4354198_str_match(keyVal, "KEY_CHANNELUP")) {
        return '!';        
    } else if (s4354198_str_match(keyVal, "KEY_CHANNELDOWN")) {
        return '#';        
    } else if (s4354198_str_match(keyVal, "KEY_RECORD")) {
        return '*';        
    } else if (s4354198_str_match(keyVal, "KEY_TIME")) {
        return '(';        
    } else if (s4354198_str_match(keyVal, "KEY_MUTE")) {
        return ')';        
    } else if (s4354198_str_match(keyVal, "KEY_POWER")) {
        return '.';
    }

    return ' ';
}

/**
 * Handles exit 
 */
void sigint_handler(int signal) {
    fprintf(stderr, "Exiting remote\n");

    exit(0);
}