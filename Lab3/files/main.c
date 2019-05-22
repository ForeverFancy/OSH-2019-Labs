#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

#define BIND_IP_ADDR "127.0.0.1"
#define BIND_PORT 8000
#define MAX_RECV_LEN 1048576
#define MAX_SEND_LEN 1048576
#define MAX_PATH_LEN 1024
#define MAX_HOST_LEN 1024
#define MAX_CONN 20

#define HTTP_STATUS_200 "200 OK"
#define HTTP_STATUS_404 "404 Not Found"
#define HTTP_STATUS_500 "500 Internal Server Error"

int clients[MAX_CONN];

void parse_request(char* request, ssize_t req_len, char* path, ssize_t* path_len)
{
    char *req = request;
    char *method;
    method = strtok(req, " ");
    if (strcmp(method, "GET") != 0)
        //TODO:Handle the wrong method.
        return;
    strcpy(path,strtok(NULL, " "));
    *path_len = strlen(path);
}

int handle_client(int client_socket)
{
    char *path = (char *)malloc(MAX_PATH_LEN * sizeof(char));
    ssize_t path_len;
    int fd;
    char *return_buf = (char *)malloc(MAX_SEND_LEN * sizeof(char));
    char *response = (char *)malloc(MAX_SEND_LEN * sizeof(char));
    
    char buffer[1024] = {0};
    int valread = read( client_socket , buffer, 1024);
    if (valread < 0)
    {
        printf("Nothing to read.\n");
    }
    else
    {
        printf("buffer:%s\n", buffer);
        parse_request(buffer, strlen(buffer), path, &path_len);
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
            path++;
    }
    
    if ((fd = open(path, O_RDONLY)) != -1) //FILE FOUND
    {
        int bytes_read = read(fd, return_buf, 1024);
        printf("Read success.\n");
        sprintf(response,"HTTP/1.0 %s\r\nContent-Length: %zd\r\n\r\n%s",
                HTTP_STATUS_200, strlen(return_buf), return_buf);
        size_t response_len = strlen(response);
    }
    else
    {
        sprintf(response,"HTTP/1.0 %s\r\nContent-Length: 0\r\n\r\n",
                HTTP_STATUS_404);
    }
    
    
    size_t response_len = strlen(response);
    write(client_socket, response, response_len);

    shutdown(client_socket, SHUT_RDWR);
    close(client_socket);
    free(response);

    return 0;
}

int main()
{
    int available_index = 0;
    int server_socket;
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
        perror("Bind failed");
        return 1;
    }

    if(listen(server_socket, MAX_CONN))
    {
        perror("Listen failed");
        return 1;
    }

    struct sockaddr_in client_addr;
    socklen_t client_addr_size = sizeof(client_addr);

    for (int i = 0; i < MAX_CONN;i++)
    {
        clients[i] = -1;
    }

    while (1)
    {
        int client_socket;
        if ((clients[available_index] = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_size)) < 0)
        {
            perror("Accept failed");
            exit(EXIT_FAILURE);
        }
        else
        {
            pid_t pid = fork();
            if(pid==0)
            {
                int flag = handle_client(clients[available_index]);
                if(flag==0)
                    clients[available_index] = -1;
                _exit(0);
            }
            while(clients[available_index]!=-1)
                available_index=(available_index+1)%MAX_CONN;
        }
    }

    return 0;
}
