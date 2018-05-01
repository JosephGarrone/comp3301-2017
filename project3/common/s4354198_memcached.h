#ifndef MEMCACHED_H
#define MEMCACHED_H

#include <stdarg.h>
#include <stdbool.h>
#include <libmemcached/memcached.h>

/* Defines */
#define MEMCACHED_HOST "localhost"
#define MEMCACHED_PORT "11211"
#define MEMCACHED_PORT_INT 11211

/* Function prototypes */
memcached_st* s4354198_create_memcached_server();
memcached_st* s4354198_connect_memcached_server();
bool s4354198_mem_set(char* key, char* value);
char* s4354198_mem_get(char* key);
void s4354198_wait_mem_get(char* key, char* check, char** result);
void s4354198_wait_not_mem_get(char* key, char* check, char** result);

#endif