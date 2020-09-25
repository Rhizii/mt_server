#include "requests.h"
#include "server.h"
#include "thread_pool.h"
#include "compression.h"

// This file contains all the logic behind handling each request.
// Each request type has its own function: handle_[request].


// creates a request struct which stores all the information about relevant
// information about the request
struct request* construct_request(struct server_info* s_info,
                                 int client_socket)
{
    struct request* r = malloc(sizeof(*r));
    r->client_socket =  client_socket;
    uint8_t msg_header;
    

    int bytes_recv = recv(client_socket, &msg_header, 1, 0);
    
    // case when the connection is closed from the client's side
    if (bytes_recv < 1)
    {
        free(r);
        return NULL;
    }  
        
    
    bytes_recv = recv(client_socket, &(r->payload_len), 8, 0);
    if (bytes_recv != 8)
        perror("Could not read payload length");

    
    r->payload_len = be64toh(r->payload_len);
    

    if (r->payload_len > 0)
    {
        r->payload = malloc(sizeof(*r->payload)*r->payload_len);

        bytes_recv = recv(client_socket, r->payload, r->payload_len, 0);
        if (bytes_recv != r->payload_len)
            perror("Could not read all payload bytes");
    }
    
    // rearming socket so that it is tracked by epoll
    struct epoll_event event;
    event.data.fd = client_socket;
    event.events = EPOLLIN | EPOLLONESHOT | EPOLLET;

    epoll_ctl(s_info->epfd, EPOLL_CTL_MOD, client_socket, &event);


    // setting information from the messsage header
    r->msg_type = msg_header >> 4; 

    r->payload_compressed = IS_BIT_SET(msg_header, PAYLOAD_COMPRESSED_BIT);
    r->compress_response = IS_BIT_SET(msg_header, COMPRESS_RESPONSE_BIT);
    return r;
}


// checks type of the request and calls the appropiate function to handle it
int handle_request(int client_socket, struct server_info* s_info)
{
    struct request* r = construct_request(s_info, client_socket);
    
    if (NULL == r)
    {
        handle_error(client_socket);
        free(r);
        return 1;
    }
        
    if (r->msg_type == SHUTDOWN_REQUEST)
    {
        if (r->payload_len > 0)
            free(r->payload);

        free(r);
        return 2;
    }
    else if (r->msg_type == ECHO_REQUEST)
    {
        handle_echo(r, s_info);
    }    
        
    else if (r->msg_type == DIR_LIST_REQUEST && 
            htobe64(r->payload_len) == 0)
    {
        handle_dir_listing(r, s_info);
    }
    else if (r->msg_type == FILE_SIZE_REQUEST)
    {
        handle_file_size_query(r, s_info);
    }
    else if (r->msg_type == FILE_RETRIEVE_REQUEST)
    {
        handle_file_retrieval(r, s_info);
    }
    else
    {
        handle_error(client_socket);

        if (r->payload_len > 0)
            free(r->payload);

        free(r);
        return 1;
    }

    return 0;
    
}


void handle_error(int client_socket)
{
    size_t response_size = MSG_HEADER_SZ + PAYLOAD_LEN_SZ;
    char* response = malloc(sizeof(char)*response_size);

    // setting message header byte
    response[0] = ERROR_RESPONSE << 4;

    // setting payload length to zero
    uint64_t host_len = htobe64(0);
    memcpy(response+1, &host_len, PAYLOAD_LEN_SZ);

    int bytes_sent = send(client_socket, response, response_size, 0);

    if (bytes_sent != response_size)
        perror("failed to send all bytes\n");

    close(client_socket);
    
    free(response);
}


void handle_echo(struct request* request, struct server_info* s_info)
{
    
    if (request->compress_response && !request->payload_compressed)
    {
        compress_payload(s_info->c_info, &request->payload,
                 &request->payload_len);   
    }
        
    size_t response_size = MSG_HEADER_SZ + PAYLOAD_LEN_SZ + 
    request->payload_len;
    uint8_t* response = malloc(sizeof(*response)*response_size);

    // copying message header byte
    response[0] = ECHO_RESPONSE << 4;

    if (request->payload_compressed || request->compress_response)
    {   
        SET_BIT(response[0], PAYLOAD_COMPRESSED_BIT);
        
    }

    // copying payload len 
    uint64_t be_len = htobe64(request->payload_len);
    memcpy(response+1, &be_len, PAYLOAD_LEN_SZ);

    // copying payload
    memcpy(response + 9, request->payload, request->payload_len);
    
    int bytes_sent = send(request->client_socket, response, response_size, 0);

    if (bytes_sent != response_size)
        perror("failed to send all bytes");

    
    free(response);
    if (request->payload_len > 0)
        free(request->payload);
    free(request);
    
}

// returns a list of regular file names, separated by null bytes
uint8_t* get_list_of_files(char* target_dir, uint64_t* files_len)
{
    *files_len = 0;
    size_t max_len = 50;
    uint8_t* files = malloc(sizeof(*files)*max_len);
    size_t name_len = 0;


    struct dirent* dir;
    DIR* d = opendir(target_dir);

    if (d == NULL)
        perror("couldn't open target directory ");
    
    while ((dir = readdir(d)) != NULL)
    {
        // only including regular files
        if (strcmp(dir->d_name, ".") == 0 || 
            strcmp(dir->d_name, "..") == 0 || 
            dir->d_type != DT_REG)
        {
            continue;
        }

        if (*files_len >= max_len)
        {
            max_len *= 2;
            files = realloc(files, sizeof(char)*max_len);
        }
        
        name_len = strlen(dir->d_name);

        memcpy(files + *files_len, dir->d_name, name_len);
        *files_len += name_len;

        files[*files_len] = NULL_BYTE;
        *files_len += 1;
        
    }

    closedir(d);

    // if no files in directory - setting null byte
    if (*files_len == 0)
    {
        files[0] = NULL_BYTE;
        *files_len += 1;
    }


    return files;
}

// handles directory listing request - sends file names 
void handle_dir_listing(struct request* request, struct server_info* s_info)
{
    uint64_t payload_len = 0;
    
    uint8_t* payload = get_list_of_files(s_info->target_dir, &payload_len);

   if (request->compress_response)
    {
        compress_payload(s_info->c_info, &payload, &payload_len);
    }

    uint64_t response_size = MSG_HEADER_SZ + PAYLOAD_LEN_SZ + payload_len;
    uint8_t* response = malloc(sizeof(*response)*response_size);

    // construct response
    response[0] = DIR_LIST_RESPONSE << 4;

    if (request->compress_response)
    {
        SET_BIT(response[0], PAYLOAD_COMPRESSED_BIT);
    }

    uint64_t be_len = htobe64(payload_len);

    memcpy(response + 1, &be_len, PAYLOAD_LEN_SZ);
    memcpy(response + 9, payload, payload_len);

    int bytes_sent = send(request->client_socket, response, response_size, 0);

    if (bytes_sent != response_size)
        perror("failed to send all bytes");

    free(payload);
    free(response);
    free(request);
}


void handle_file_size_query(struct request* request, struct server_info* s_info)
{      
    size_t file_name_len = strlen(s_info->target_dir) + 
                                request->payload_len + 5;
    char* file_name = malloc(sizeof(char)*file_name_len);

    sprintf(file_name, "%s/%s", s_info->target_dir, request->payload);

    FILE* f = fopen(file_name, "r");
    
    // file doesnt exist
    if (NULL == f)
    {
        handle_error(request->client_socket);   
    }
    else
    {
        fseek(f, 0, SEEK_END); 
        uint64_t file_size = ftell(f);
        uint64_t be_file_size = htobe64(file_size);

        uint64_t payload_len = 8;
        uint8_t* payload = malloc(sizeof(*payload)*payload_len);
        
        memcpy(payload, &be_file_size, 8);

        if (request->compress_response)
        {
            compress_payload(s_info->c_info, &payload, &payload_len);   
        }

        
        uint64_t response_size = MSG_HEADER_SZ + PAYLOAD_LEN_SZ + payload_len;
        char* response = malloc(sizeof(*response)*response_size);

        uint64_t be_len = htobe64(payload_len);
    
        // constructing response
        response[0] = FILE_SIZE_RESPONSE << 4;

        if (request->compress_response)
        {
            SET_BIT(response[0], PAYLOAD_COMPRESSED_BIT);
        }
    
        memcpy(response + 1, &be_len, PAYLOAD_LEN_SZ);
        memcpy(response + 9, payload, payload_len);

        int bytes_sent = send(request->client_socket, response,
                             response_size, 0);

        if (bytes_sent != response_size)
            perror("failed to send all bytes");

        free(response);
        free(payload);
    }
        
    
    free(file_name);
    free(request->payload);
    free(request);
    
}

               
// updates the shared list of current file requests appropiately 
int update_file_requests(struct server_info* s_info, uint32_t* session_id, 
                uint64_t* start_offset, uint64_t* n_bytes, char* file_name)
{
    pthread_mutex_lock(&s_info->f_requests_lock);
    struct file_request curr;

    for (size_t i = 0; i < s_info->n_file_requests; i++)
    {
        curr = s_info->file_requests[i];
        if (*session_id == curr.session_id && 
            *start_offset == curr.start_offset &&
            *n_bytes == curr.n_bytes &&
            (strcmp(file_name, curr.file_name) == 0) && 
            curr.completed)
        {
            // indicating that the file request has already been handled 
            // by another thread
            *n_bytes = 0;
            return 1;
        }
    }

    // creating a new file request since the it doesnt already exist in the
    // shared list
    size_t j = s_info->n_file_requests;
    curr = s_info->file_requests[j];

    s_info->file_requests[j].session_id = *session_id;
    s_info->file_requests[j].start_offset = *start_offset;
    s_info->file_requests[j].n_bytes = *n_bytes;
    s_info->file_requests[j].completed = true;

    memcpy(&s_info->file_requests[j].file_name, file_name, strlen(file_name)+1);

    s_info->n_file_requests++;

    pthread_mutex_unlock(&s_info->f_requests_lock);

    return 0;
}

void send_file(struct server_info* s_info, struct request* request, FILE* f, 
    uint64_t* file_data_size, uint32_t* session_id, uint64_t* start_offset, 
    uint64_t* n_bytes)
{
    fseek(f, *start_offset, SEEK_SET);

    // copying file data
    uint8_t* file_data = malloc(sizeof(char)*(*n_bytes));
    
    for (size_t i = 0; i < *n_bytes; i++)
    {
        file_data[i] = fgetc(f);
    }

    uint64_t payload_len = 4 + 8 + 8 + *n_bytes;
    uint8_t* payload = malloc(sizeof(*payload)*payload_len);

    uint64_t be_start_offset = htobe64(*start_offset);
    uint64_t be_n_bytes = htobe64(*file_data_size);

    memcpy(payload, session_id, 4);
    memcpy(payload + 4, &be_start_offset, 8);
    memcpy(payload + 12, &be_n_bytes, 8);
    memcpy(payload + 20, file_data, *n_bytes);

    if (request->compress_response)
    {
        compress_payload(s_info->c_info, &payload, &payload_len);
    }
        

    uint64_t response_size = MSG_HEADER_SZ + PAYLOAD_LEN_SZ + 
                        payload_len;
    uint8_t* response = malloc(sizeof(*response)*response_size);
    
    uint64_t be_payload_len = htobe64(payload_len);    

    // constructnig response
    response[0] = FILE_RETRIEVE_RESPONSE << 4;

    if (request->compress_response)
    {
        SET_BIT(response[0], PAYLOAD_COMPRESSED_BIT);
    }
    
    memcpy(response + 1, &be_payload_len, 8);
    memcpy(response + 9, payload, payload_len);
 
    int bytes_sent = send(request->client_socket, response,
                        response_size, 0);

    if (bytes_sent != response_size)
        perror("failed to send all bytes");

    free(file_data);
    free(payload);
    free(response);   
}

void handle_file_retrieval(struct request* request, struct server_info* s_info)
{
    uint32_t session_id;
    uint64_t start_offset;
    uint64_t n_bytes_file;
    char* target_file;
    

    uint64_t file_name_len = request->payload_len - (4+8+8) +
                                 strlen(s_info->target_dir) + 5;
    char* file_name = malloc(sizeof(char)*file_name_len);

    // decompressing
    if (request->payload_compressed)
    {
        decompress_payload(s_info->c_info, &request->payload, 
                            &request->payload_len);
    }
    
    memcpy(&session_id, request->payload, 4);
    memcpy(&start_offset, request->payload + 4, 8);
    memcpy(&n_bytes_file, request->payload + 12, 8);
    target_file = (char*) (request->payload + 20);
    // memcpy(target_file, request->payload + 20, t;arget_file_len);

    start_offset = be64toh(start_offset);
    n_bytes_file = be64toh(n_bytes_file);

    update_file_requests(s_info, &session_id, &start_offset,
                         &n_bytes_file, target_file);

    // file request has already been handled by another thead
    if (n_bytes_file == 0)
    {
        handle_error(request->client_socket);

        free(file_name);
        free(request->payload);
        free(request);
        return;
    }
    

    sprintf(file_name, "%s/%s", s_info->target_dir, request->payload + 20);   

    FILE* f = fopen(file_name, "r");
    
    uint64_t file_data_size = n_bytes_file;
    

    if (NULL == f)
    {
        
        handle_error(request->client_socket);
    }
    else 
    {
        
        fseek(f, 0, SEEK_END); 
        uint64_t file_size = ftell(f);
        fseek(f, 0, SEEK_SET);

        // checking for out of range offset and lengths
        if (start_offset < 0 || (start_offset + n_bytes_file) > file_size)
        {
            handle_error(request->client_socket);
        }
        else
        {
            send_file(s_info, request, f, &file_data_size, &session_id,
                         &start_offset, &n_bytes_file);
        }

    }    

    free(file_name);
    free(request->payload);
    free(request);
    

}