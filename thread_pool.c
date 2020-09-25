#include "thread_pool.h"
#include "requests.h"
#include "server.h"


void* accepter_thread(void* args)
{
    struct server_info* s_info = args;
    struct epoll_event events[SOMAXCONN];
    int server_socket = s_info->server_socket;
    int epfd = s_info->epfd;
    int write_fd = s_info->write_fd;

    struct epoll_event event;
    int n_events = 0;

    struct sockaddr_in client_addr;
    uint32_t addr_len = sizeof(struct sockaddr_in);

    int client_socket = 1;

    while (true)
    {       
        n_events = epoll_wait(epfd, events, SOMAXCONN, TIMEOUT);

        for (size_t i = 0; i < n_events; i++)
        {
            if (events[i].data.fd == server_socket)
            {
                // accepting all incoming clients
                while (true)
                {
                    client_socket = accept(server_socket, 
                            (struct sockaddr*) &client_addr, &addr_len);

                    if (!(client_socket > 0))
                    {
                        break;
                    }
                        
                    
                    usleep(500);

                    event.data.fd = client_socket;
                    event.events = EPOLLIN | EPOLLONESHOT;            
                    epoll_ctl(epfd, EPOLL_CTL_ADD, client_socket, &event);

                }
                
                break;
            }
            
            else
            {
                // handling an existig client
                if (events[i].events & (EPOLLIN | EPOLLONESHOT)) 
                {
                    // adding client to queue, so that one of the worker threads
                    // can handle their request
                    write(write_fd, &events[i].data.fd, sizeof(int));

                }
                
            }
        }
    }

    return (void*) NULL;
}


// these threads handle all client requests and read data from a pipe
void* worker_thread(void* args)
{
    struct server_info* s_info = args;
    int n_bytes;
    int ret;
    int client_socket;
    struct epoll_event event;
    


    while (true)
    {
        ////fprintf(stderr, "thread: waiting for data\n");
        n_bytes = read(s_info->read_fd, &client_socket,sizeof(int));
        ////fprintf(stderr, "thread read bytes: %d and socket: %d\n",n_bytes, client_socket);

        // read fd closed, indicating shutdown message has been sent 
        if (n_bytes < sizeof(int))
        {
            break;
        }

        ret = handle_request(client_socket, s_info);

        if (ret == 1)
        {
            ret = epoll_ctl(s_info->epfd, EPOLL_CTL_DEL, client_socket, &event);
         
            shutdown(client_socket, SHUT_RDWR);
            close(client_socket);
        }
        else if (ret == 2)
        {
            // shut down signal received  
            close(s_info->write_fd);
            close(s_info->read_fd);
            sem_post(&s_info->shutdown_sem);
            
            break;
        }
            
    }

    return (void*) NULL;

}


void create_thread_pool(struct server_info* s_info)
{
    // creating thread queue in which client socket fds are stored
    int queue[2];
    pipe(queue);

    s_info->read_fd = queue[0];
    s_info->write_fd = queue[1];

    int n_threads = get_nprocs()-1;
    pthread_t* ptids = malloc(sizeof(*ptids)*n_threads);

    s_info->ptids = ptids;
    s_info->n_threads = n_threads;

    
    for (int i = 1; i < n_threads; i++)
    {
       pthread_create(&ptids[i], NULL, worker_thread, (void*) s_info);
    }

    // creating the accepter thread last 
    pthread_create(&ptids[0], NULL, accepter_thread, (void*) s_info);

}


void cleanup_thread_pool(struct server_info* s_info)
{
    pthread_cancel(s_info->ptids[0]);

    for (int i = 1; i < s_info->n_threads; i++)
    {
        pthread_cancel(s_info->ptids[i]);        
    }

    free(s_info->ptids);
}