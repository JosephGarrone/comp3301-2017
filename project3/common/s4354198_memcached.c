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
#include <libmemcached/memcached.h>

#include "s4354198_utils.h"
#include "s4354198_defines.h"
#include "s4354198_structs.h"
#include "s4354198_memcached.h"

extern Application* app;

/**
 * Creates the memcached server
 */
memcached_st* s4354198_create_memcached_server() {
    memcached_server_st *servers = NULL;
    memcached_st *memc;
    memcached_return rc;
    
    memc = memcached_create(NULL);
    servers = memcached_server_list_append(servers, MEMCACHED_HOST, MEMCACHED_PORT_INT, &rc);
    rc = memcached_server_push(memc, servers);

    if (rc != MEMCACHED_SUCCESS) {
        fprintf(stderr, "Error starting memcached server.\n");
    }

    return memc;
}

/**
 * Connects to the memcached server
 */
memcached_st* s4354198_connect_memcached_server() {
    return s4354198_create_memcached_server();
}

/**
 * Sets a value in shared memory
 */
bool s4354198_mem_set(char* key, char* value) {
    memcached_return_t rc = memcached_set(app->memc, key, strlen(key), value, strlen(value), (time_t) 0, (uint32_t) 0);

    //s4354198_p("Setting '%s' to '%s' had result %d\n", key, value, rc == MEMCACHED_SUCCESS);

    return rc == MEMCACHED_SUCCESS;
}

/**
 * Gets a value from shared memory
 */
char* s4354198_mem_get(char* key) {
    size_t valLen;
    uint32_t flags;
    memcached_return_t rc;
    
    char* result = memcached_get(app->memc, key, strlen(key), &valLen, &flags, &rc);

    //printf(stderr, "Read %s = %s\n", key, result);

    return result;
}

/**
 * Waits until a mem value equals a certain value
 */
void s4354198_wait_mem_get(char* key, char* check, char** result) {
    *result = s4354198_mem_get(key);

    while (!s4354198_str_match(*result, check)) {
        free(*result);
        *result = s4354198_mem_get(key);

        // Sleep 10ms
        usleep(10000);
    }
}

/**
 * Waits until a mem value does not equal a certain value
 */
void s4354198_wait_not_mem_get(char* key, char* check, char** result) {
    *result = s4354198_mem_get(key);

    while (s4354198_str_match(*result, check)) {
        free(*result);
        *result = s4354198_mem_get(key);

        // Sleep 10ms
        usleep(10000);
    }
}