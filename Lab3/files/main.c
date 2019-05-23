#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>

#define BIND_IP_ADDR "127.0.0.1"
#define BIND_PORT 8000
#define MAX_RECV_LEN 1048576
#define MAX_SEND_LEN 1048576
#define MAX_PATH_LEN 1024
#define MAX_HOST_LEN 1024
#define MAX_CONN 300

#define HTTP_STATUS_200 "HTTP/1.0 200 OK\r\n"
#define HTTP_STATUS_404 "HTTP/1.0 404 Not Found"
#define HTTP_STATUS_500 "HTTP/1.0 500 Internal Server Error"

void handle_error(char *msg)
{
    perror(msg);
    exit(1);
}

void set_nonblocking(int fd)
{
    int flag;
    if ((flag = fcntl(fd, F_GETFL, 0)) == -1)
        handle_error("fcntl");
    else
    {
        int temp;
        flag = flag | O_NONBLOCK;
        if ((temp = fcntl(fd, F_SETFL, flag)) == -1)
            handle_error("fcntl");
    }
}

char *parse_request(char* request, ssize_t req_len, ssize_t* path_len)
{
    char *req = request;
    char *path = (char *)malloc(MAX_PATH_LEN * sizeof(char));
    char *method;
    method = strtok(req, " ");
    if (strcmp(method, "GET") != 0)
        //TODO:Handle the wrong method.
        return NULL;
    strcpy(path,strtok(NULL, " "));
    strcat(path, "\0");
    *path_len = strlen(path);
    return path;
}

int handle_client(int epoll_fd, int client_socket)
{
    char *path;
    char *temp_path;
    ssize_t path_len;
    int fd;
    char *return_buf = (char *)malloc(MAX_SEND_LEN * sizeof(char));
    char *response = (char *)malloc(MAX_SEND_LEN * sizeof(char));
    
    char *buffer = (char *)malloc(MAX_RECV_LEN * sizeof(char));
    int valread = read( client_socket , buffer, 1024);
    if (valread < 0)
    {
        printf("Nothing to read.\n");
    }
    else
    {
        printf("buffer:%s\n", buffer);
        path = parse_request(buffer, strlen(buffer), &path_len);
        temp_path = path;
        printf("Path:%s\nPath_len:%ld\n", path, path_len);
        if(path_len==0)
        {
            sprintf(response,"HTTP/1.0 %s\r\nContent-Length: 0\r\n\r\n",
                HTTP_STATUS_500);
            size_t response_len = strlen(response);
            write(client_socket, response, response_len);

            shutdown(client_socket, SHUT_RDWR);
            close(client_socket);
            free(response);

            return 0;
        }
        if(path[0]=='/')
            temp_path++;
    }
    
    if ((fd = open(temp_path, O_RDONLY | O_NONBLOCK)) != -1) //FILE FOUND
    {
        struct stat stat_buf;
	    fstat(fd,&stat_buf);
        send(client_socket, HTTP_STATUS_200, strlen(HTTP_STATUS_200), 0);
        sprintf(response,"Content-Length: %zd\r\n\r\n",stat_buf.st_size);
        send(client_socket, response, strlen(response), 0);
        sendfile(client_socket, fd, NULL, stat_buf.st_size);
        close(fd);
        // int bytes_read = read(fd, return_buf, 1024);
        // printf("Read success.\n");
        // size_t response_len = strlen(response);
    }
    else
    {
        sprintf(response,"HTTP/1.0 %s\r\nContent-Length: 0\r\n\r\n",
                HTTP_STATUS_404);
    }
    
    
    // size_t response_len = strlen(response);
    // write(client_socket, response, response_len);

    if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_socket, NULL) == -1)
       handle_error("epoll delete");

    shutdown(client_socket, SHUT_RDWR);
    close(client_socket);
    
    free(response);
    free(return_buf);
    free(buffer);
    free(path);

    return 0;
}

int run_server()
{
    int server_socket;
    int epfd, num_of_events;
    struct epoll_event ev, events[MAX_CONN];

    if ((server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
    {
        perror("Could not open socket");
        return 1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(BIND_IP_ADDR);
    server_addr.sin_port = htons(BIND_PORT);

    if(bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr))<0)
    {
        handle_error("Bind failed");
    }

    if(listen(server_socket, MAX_CONN))
    {
        handle_error("Listen failed");
    }

    printf("\nServer Waiting for client on port 8000.\n");

    epfd = epoll_create(MAX_CONN);      // Create epoll.
    if (epfd == -1)
        handle_error("Create epoll");

    ev.events = EPOLLIN;
    ev.data.fd = server_socket;

    if ((epoll_ctl(epfd, EPOLL_CTL_ADD, server_socket, &ev)) == -1)    //Register event.
        handle_error("Register epoll");

    struct sockaddr_in client_addr;
    socklen_t client_addr_size = sizeof(client_addr);

    while (1)
    {
        int client_socket;
        num_of_events = epoll_wait(epfd, events, MAX_CONN, -1);
        if (num_of_events == -1)
            handle_error("epoll wait");
        
        for (int i = 0; i < num_of_events; i++)
        {
            if (events[i].data.fd == server_socket)
            {
                client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_size);
                if(client_socket == -1)
                    handle_error("accept");
                set_nonblocking(client_socket);
                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = client_socket;
                if((epoll_ctl(epfd, EPOLL_CTL_ADD, client_socket, &ev)) == -1)
                    handle_error("epoll connect");
                else
                    handle_client(epfd, client_socket);
            }
        }
        
        // if ((clients[available_index] = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_size)) < 0)
        // {
        //     perror("Accept failed");
        //     exit(EXIT_FAILURE);
        // }
        // else
        // {
        //     pid_t pid = fork();
        //     if(pid==0)
        //     {
        //         int flag = handle_client(clients[available_index]);
        //         if(flag==0)
        //             clients[available_index] = -1;
        //         _exit(0);
        //     }
        //     while(clients[available_index]!=-1)
        //         available_index=(available_index+1)%MAX_CONN;
        // }
    }

    return 0;
}

int main()
{
    run_server();
    return 0;
}