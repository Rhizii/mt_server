#ifndef SERVER_H
#define SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <sys/select.h>
#include <sys/poll.h>
#include <sys/epoll.h>
#include <time.h>
#include <sys/sysinfo.h>
#include <semaphore.h>
#include <fcntl.h>


#include "compression.h"


#define MAX_FILEPATH (30)
#define MAX_LISTENING (10)
#define STARTING_CLIENTS (5)
#define TIMEOUT (100)





struct server_info {
    int server_socket;
    struct sockaddr_in addr;
    char* target_dir;
    int epfd;
    
    sem_t shutdown_sem;
    int read_fd;
    int write_fd;

    pthread_t* ptids;
    int n_threads;
    struct epoll_event* events;

    struct file_request* file_requests;
    size_t n_file_requests;
    size_t cap_file_requests;
    pthread_mutex_t f_requests_lock;
    

    struct compression_info* c_info;



};




void init_server(char* config_file, struct server_info* info);




#endif