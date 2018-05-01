#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdarg.h>
#include <libmemcached/memcached.h>

#include "../common/s4354198_structs.h"
#include "../common/s4354198_defines.h"
#include "../common/s4354198_utils.h"
#include "../common/s4354198_externs.h"
#include "../common/s4354198_memcached.h"

/* Function prototypes */
void sigint_handler(int signal);

/* Globals */

// Contains application data
Application* app;

int main(int argc, char** argv) {
    app = (Application*) malloc(sizeof(Application));
    
    signal(SIGINT, sigint_handler);

    app->memc = s4354198_create_memcached_server();
    //app->memc = s4354198_connect_memcached_server();

    if (app->memc == NULL) {
        fprintf(stderr, "Failed to start memcached server.\n");
        return 1;
    }

    s4354198_mem_set(MEM_DISPLAYS, " ");
    s4354198_mem_set(MEM_NEW_DISP, "-");
    s4354198_mem_set(MEM_SEL_DISP, "-");
    s4354198_mem_set(MEM_KEY, "-");

    while (1) {
        sleep(10); // loop
    }

    return 0;
}

/**
 * Handles exit 
 */
void sigint_handler(int signal) {
    fprintf(stderr, "Exiting memcached\n");

    memcached_free(app->memc);

    exit(0);
}