#ifndef REQUESTS_H
#define REQUESTS_H


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
#include <stdint.h>
#include <endian.h>
#include <dirent.h>

#include "server.h"



#define SET_BIT(BF, N) BF |= ((uint8_t)(00000001) << (7-N))
#define CLR_BIT(BF, N) BF &= ~((uint8_t)00000001) << (7-N))
#define IS_BIT_SET(BF, N) ((BF >> (7-N)) & 0x1)



#define MSG_HEADER_SZ (1)
#define PAYLOAD_LEN_SZ (8)

#define PAYLOAD_COMPRESSED_BIT (4)
#define COMPRESS_RESPONSE_BIT (5)

// defining all types digits for message headers
#define ERROR_RESPONSE (0xf)
#define ECHO_REQUEST (0x0)
#define ECHO_RESPONSE (0x1)
#define DIR_LIST_REQUEST (0x2)
#define DIR_LIST_RESPONSE (0x3)
#define FILE_SIZE_REQUEST (0x4)
#define FILE_SIZE_RESPONSE (0x5)
#define FILE_RETRIEVE_REQUEST (0x6)
#define FILE_RETRIEVE_RESPONSE (0x7)
#define SHUTDOWN_REQUEST (0x8)
#define NULL_BYTE (0x00)

#define MAX_FILE_NAME (160)



struct request {
    int client_socket;
    uint8_t msg_type;
    bool payload_compressed;
    bool compress_response;
    uint64_t payload_len;
    uint8_t* payload;
};

struct file_request{
    uint32_t session_id;
    uint64_t start_offset;
    uint64_t n_bytes;
    char file_name[MAX_FILE_NAME];
    bool completed;
};




struct request* construct_request(struct server_info* s_info,
                         int client_socket);

int handle_request(int client_socket, struct server_info* info);

void handle_error(int client_socket);

void handle_echo(struct request* request, struct server_info* s_info);

uint8_t* get_list_of_files(char* target_dir, uint64_t* files_len);

void handle_dir_listing(struct request* request, struct server_info* info);

void handle_file_size_query(struct request* request, struct server_info* s_info);

int update_file_requests(struct server_info* s_info, uint32_t* session_id, 
        uint64_t* start_offset, uint64_t* n_bytes, char* file_name);

void send_file(struct server_info* s_info, struct request* request, 
    FILE* f, uint64_t* file_data_size, uint32_t* session_id, 
    uint64_t* start_offset, uint64_t* n_bytes);

void handle_file_retrieval(struct request* request, struct server_info* s_info);






#endif