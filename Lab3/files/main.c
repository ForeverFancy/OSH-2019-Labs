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
#include <signal.h>
#include <errno.h>

#define BIND_IP_ADDR "127.0.0.1"
#define BIND_PORT 8000
#define MAX_RECV_LEN 1048576
#define MAX_SEND_LEN 1048576
#define MAX_PATH_LEN 1024
#define MAX_HOST_LEN 1024
#define MAX_CONN 100
#define MAX_FDS 1000

#define HTTP_STATUS_200 "HTTP/1.0 200 OK\r\n"
#define HTTP_STATUS_404 "HTTP/1.0 404 Not Found"
#define HTTP_STATUS_500 "HTTP/1.0 500 Internal Server Error"

int flag = 1;

struct buf
{
    int fd;
    char header_buffer[MAX_PATH_LEN];
    char read_buffer[MAX_SEND_LEN];
    int have_read;
    int have_read_header;
    int have_written_header;
    int have_written_file;
    int header_length;
    int file_size;
    int read_offset;
    int header_offset;
    off_t write_offset;
} buff[1000];

void init()
{
    memset(buff, 0, 1000 * sizeof(struct buf));
}

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

void parse_request(char* request, char *path, ssize_t* path_len)
{
    char *req = request;
    char *method;
    method = strtok(req, " ");
    if (strcmp(method, "GET") != 0)
        //TODO:Handle the unsupported method.
        return ;
    strcpy(path, strtok(NULL, " "));
    *path_len = strlen(path);
}

int handle_read(int epoll_fd, int client_socket)
{
    // char *path = (char *)malloc(MAX_PATH_LEN * sizeof(char));
    // char *temp_path;
    // int fd;
    printf("Reading from client.\n");
    if (buff[client_socket].have_read==0)
    {
        //for (;;)
        {
            ssize_t valread = read(client_socket, buff[client_socket].read_buffer + buff[client_socket].read_offset, 1024);
            printf("Reading valread:%ld\n",valread);
            if (valread == -1)
            {
                if(errno != EAGAIN)
                {
                    handle_error("Reading from client");
                }
                else
                {
                    return 0;
                }
                printf("Waiting to read.\n");
            }
            else if (valread == 0)
            {
                buff[client_socket].have_read = 1;
                //break;
            }
            else
            {
                buff[client_socket].read_offset += valread;
            }
        }
    }

    return 1;

    // printf("buffer:%s\n", buffer);
    // parse_request(buffer, path, &path_len);
    // temp_path = path;
    // if(path[0] == '/')
    //     temp_path++;
    // printf("Path:%s\nPath_len:%ld\n", temp_path, path_len);
    // free(buffer);
    // buffer = (char *)malloc(MAX_RECV_LEN * sizeof(char));

    // for (int i = 0; i < 1000; i++)
    // {
    //     if (strcmp(buff[i].path, temp_path) == 0)
    //     {
    //         buff[client_socket].buffer = buff[i].buffer;
    //         buff[client_socket].path = temp_path;
    //         return ;
    //     }
    // }

    // strcpy(buff[client_socket].buffer, buffer);
    // strcpy(buff[client_socket].path, temp_path);
    // free(buffer);
    // free(path);

}

int handle_write(int epoll_fd, int client_socket)
{
    printf("Sending to client.\n");
    int fd;
    char *temp_path;
    char *path = (char *)malloc(MAX_PATH_LEN * sizeof(char));
    ssize_t path_len;
    char *buffer = buff[client_socket].read_buffer;
    char *filebuffer = (char *)malloc(MAX_SEND_LEN * sizeof(char));
    parse_request(buffer, path, &path_len);
    temp_path = path;
    if (path[0] == '/')
        temp_path++;
    printf("path:%s\n", temp_path);
    if ((fd = open(temp_path, O_RDONLY | O_NONBLOCK)) == -1) //FILE NOTFOUND
    {
        if(errno == ENOENT)
        {
            send(client_socket, HTTP_STATUS_404, strlen(HTTP_STATUS_404), 0);
            close(fd);
            shutdown(client_socket, SHUT_RDWR);
            if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_socket, NULL) == -1)
                handle_error("epoll delete");
            close(client_socket);
        }
        else if(errno == 21)
        {
            send(client_socket, HTTP_STATUS_500, strlen(HTTP_STATUS_500), 0);
            close(fd);
            shutdown(client_socket, SHUT_RDWR);
            if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_socket, NULL) == -1)
                handle_error("epoll delete");
            close(client_socket);
        }
        else
            handle_error("Opening file");

    }

    if (buff[client_socket].have_read_header == 0)
    {
        struct stat statbuf;
        fstat(fd, &statbuf);
        buff[client_socket].file_size = statbuf.st_size;
        char *length_response = (char *)malloc(1024 * sizeof(char));
        strcpy(buff[client_socket].header_buffer, HTTP_STATUS_200);
        sprintf(length_response,"Content-Length: %d\r\n\r\n",buff[client_socket].file_size);
        strcpy(buff[client_socket].header_buffer, length_response);
        buff[client_socket].header_length = strlen(buff[client_socket].header_buffer);
        buff[client_socket].have_read_header = 1;
        
    }

    if (buff[client_socket].have_written_header == 0)
    {
        while (buff[client_socket].header_offset < buff[client_socket].header_length)
        {
            int ret = send(client_socket, buff[client_socket].header_buffer+buff[client_socket].header_offset, buff[client_socket].header_length-buff[client_socket].header_offset, 0);
            if (ret == -1)
                if(errno!=EAGAIN)
                    handle_error("Sending header");
                else
                    return 0;
                
            buff[client_socket].header_offset += ret;
        }
        buff[client_socket].have_written_header = 1;
    }

    if (buff[client_socket].have_written_file == 0)
    {
        while (buff[client_socket].write_offset < buff[client_socket].file_size)
        {
            int ret = sendfile(client_socket, fd, &buff[client_socket].write_offset, buff[client_socket].file_size);
            printf("ret=%d\n", ret);
            if (ret == -1)
                if(errno != EAGAIN)
                    handle_error("Sending file");
                else
                    return 0;
            //buff[client_socket].write_offset += ret;
        }
        buff[client_socket].have_written_file = 1;
    }

    if (buff[client_socket].have_written_file)
    {
        close(fd);
        return 1;
    }

}

int handle_client(int epoll_fd, int client_socket)
{
    char *temp_path;
    ssize_t path_len;
    int fd;
    char *path = (char *)malloc(MAX_PATH_LEN * sizeof(char));    
    char *response = (char *)malloc(MAX_SEND_LEN * sizeof(char));
    char *buffer = (char *)malloc(MAX_RECV_LEN * sizeof(char));
    
    int valread = read( client_socket , buffer, 1024);
    if (valread < 0)
    {
        printf("client_socket:%d  Nothing to read.\n",client_socket);
    }
    else
    {
        // fprintf(f, "buffer:%s\n", buffer);
        parse_request(buffer, path, &path_len);
        temp_path = path;
        if(path_len==0)
        {
            sprintf(response,"HTTP/1.0 %s\r\nContent-Length: 0\r\n\r\n",
                HTTP_STATUS_500);
            size_t response_len = strlen(response);
            write(client_socket, response, response_len);

            shutdown(client_socket, SHUT_RDWR);
            close(client_socket);
            
            free(response);
            free(buffer);
            free(path);

            return 0;
        }
        if(temp_path[0]=='/')
            temp_path++;
        printf("Path:%s\nPath_len:%ld\n", temp_path, path_len);
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
        printf("Read success.\n");
        // size_t response_len = strlen(response);
    }
    else
    {
        printf("404.\n");
        sprintf(response, "HTTP/1.0 %s\r\nContent-Length: 0\r\n\r\n",
                HTTP_STATUS_404);
        size_t response_len = strlen(response);
        write(client_socket, response, response_len);
    }

    if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_socket, NULL) == -1)
       handle_error("epoll delete");

    shutdown(client_socket, SHUT_RDWR);
    close(client_socket);
    
    free(response);
    free(buffer);
    free(path);

    return 0;
}

int run_server()
{
    int server_socket;
    int epfd, num_of_events;
    struct epoll_event ev, events[MAX_FDS];

    init();

    if ((server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
    {
        perror("Could not open socket");
        return 1;
    }
    
    set_nonblocking(server_socket);

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

    epfd = epoll_create(MAX_FDS);      // Create epoll.
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
        num_of_events = epoll_wait(epfd, events, MAX_FDS, -1);
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
                //handle_client(epfd, client_socket);
            }
            else if (events[i].events & EPOLLIN)
            {
                client_socket = events[i].data.fd;
                if(handle_read(epfd, client_socket))
                {
                    ev.events = EPOLLOUT | EPOLLET;
                    ev.data.fd = client_socket;
                    if (epoll_ctl(epfd, EPOLL_CTL_MOD, client_socket, &ev) == -1)
                        handle_error("epoll modified");
                }
                //close(client_socket);
            }
            else if (events[i].events & EPOLLOUT)
            {
                client_socket = events[i].data.fd;
                int ret = handle_write(epfd, client_socket);
                memset(&buff[client_socket], 0, sizeof(struct buf));
                if(ret == 1)
                {
                    shutdown(client_socket, SHUT_RDWR);
                    if (epoll_ctl(epfd, EPOLL_CTL_DEL, client_socket, NULL) == -1)
                        handle_error("epoll delete");
                    close(client_socket);
                }

            }
        }
        
    }
    
    return 0;
}

int main()
{
    //signal(SIGINT, sighandler);
    run_server();
    return 0;
}