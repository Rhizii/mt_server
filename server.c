#include "server.h"
#include "requests.h"
#include "thread_pool.h"

// reads the config file and creates a server socket based of that info
// creates a server_info struct which is passed to must functions - 'helper'
void init_server(char* config_file, struct server_info* info)
{
    FILE* f = fopen(config_file, "rb");
    if (NULL == f)
    {
        puts("could not open config file!");
    }

    /* USYD CODE CITATION ACKNOWLEDGEMENT
 * I declare that the majority of the following function has been taken from the
 * seminar of Week 11 with only minor changes and it is not my own work. 
 * 
 * Week 11 Seminar Demoss
 * https://edstem.org/courses/3996/resources
 */ 
    
    uint16_t port;
    char target_dir[MAX_FILEPATH];
    int server_fd = -1;
    struct sockaddr_in server_addr;
    // struct in_addr addr;
    in_addr_t ip_addr;
   	int option = 1; 

    
    fread(&ip_addr, 1, sizeof(in_addr_t), f);
    fread(&port, 1, sizeof(u_int16_t), f);

    int i = 0;
    while (1)
    {
        if (feof(f))
        {
            target_dir[i-1] = '\0';
            break;
        }
        target_dir[i] = fgetc(f);
        i++;
    }
    fclose(f);
    
    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0)
    {
        puts("failed to create server socket");
        return;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = ip_addr;
    server_addr.sin_port = port;

    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
                 &option, sizeof(int));

    int flags = fcntl(server_fd,F_GETFL,0);
    if (flags < -1)
    {
        perror("cannot set to non-blocking\n");
    }
    fcntl(server_fd, F_SETFL, flags | O_NONBLOCK);
    
    if (bind(server_fd, (struct sockaddr*) &server_addr, 
            sizeof(struct sockaddr_in))) 
    {
		perror("server fd could not be binded");
		return;
	}

    listen(server_fd, MAX_LISTENING);

    info->target_dir = strdup(target_dir);
    info->c_info = create_compression_info();
    info->addr = server_addr;
    info->server_socket = server_fd;
    info->cap_file_requests = 20;
    info->n_file_requests = 0;
    info->file_requests = malloc(sizeof(*info->file_requests)*20);
    sem_init(&info->shutdown_sem, 0, 0);
}


void shutdown_server(struct server_info* s_info)
{
    free(s_info->target_dir);
    free(s_info->file_requests);
    free_compression_info(s_info->c_info);
    free(s_info);

    exit(0);
}



int main (int argc, char** argv)
{
    if (argc != 2)
    {
        puts("Provide config file!");
        return 1;
    }
    struct server_info* server_info = malloc(sizeof(*server_info));
    init_server(argv[1], server_info);
    
        
    // adding server socket to epoll 
    int epfd = epoll_create1(0);
    if (epfd < 0)
        perror("epoll_create failed");

    server_info->epfd = epfd;

    int ret;

    struct epoll_event event;
    
    event.data.fd = server_info->server_socket;
    event.events = EPOLLIN;

    ret = epoll_ctl(epfd, EPOLL_CTL_ADD, server_info->server_socket, &event);
    if (ret < 0)
        perror("epoll ctl of server socket failed");


    create_thread_pool(server_info);
    
    // waiting for signal from worker threads to shutdown
    sem_wait(&server_info->shutdown_sem);

    cleanup_thread_pool(server_info);        
    shutdown_server(server_info);
    

}