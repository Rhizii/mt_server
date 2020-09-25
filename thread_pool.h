#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/sysinfo.h>

#include "server.h"



void* accepter_thread(void* args);

void* worker_thread(void* args);

void create_thread_pool(struct server_info* s_info);

void cleanup_thread_pool(struct server_info* s_info);

#endif