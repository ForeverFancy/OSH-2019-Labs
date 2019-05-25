#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>
#include <errno.h>
#include <sys/resource.h>

#define BIND_IP_ADDR "0.0.0.0"
#define BIND_PORT 8000
#define MAX_RECV_LEN 1048576
#define MAX_SEND_LEN 1048576
#define MAX_PATH_LEN 1024
#define MAX_HOST_LEN 1024
#define MAX_HEADER_LEN 4096
#define MAX_CONN 200
#define MAX_FDS 3000

#define HTTP_STATUS_200 "HTTP/1.0 200 OK\r\n"
#define HTTP_STATUS_404 "HTTP/1.0 404 Not Found\r\n"
#define HTTP_STATUS_500 "HTTP/1.0 500 Internal Server Error\r\n"

int flag = 1;

struct buf
{
    int fd;
    char header_buffer[MAX_HEADER_LEN];
    char read_buffer[MAX_HEADER_LEN];
    char path[MAX_PATH_LEN];
    int open_file;
    int read_path;
    int have_read;
    int have_read_header;
    int have_written_header;
    int have_written_file;
    int header_length;
    int file_size;
    int read_offset;
    int header_offset;
    off_t write_offset;
    int write_remaining;
} buff[MAX_FDS];

void init()
{
    memset(buff, 0, MAX_FDS * sizeof(struct buf));
}

void set_root()
{
    char wd[MAX_PATH_LEN];
    getcwd(wd, sizeof(wd));
    chroot(wd);
    chdir(".");
#ifdef DEBUG

#endif
}

void handle_error(char *msg)
{
    perror(msg);
    exit(1);
}

int check_end(char *request, int length)
{
    for (int j = 0; j <= length - 4; j++)
    {
        if (request[j] == '\r' && request[j + 1] == '\n' && request[j + 2] == '\r' && request[j + 3] == '\n')
            return 1;
    }
    return 0;
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

void close_request(int client_socket)
{
#ifdef DEBUG
    printf("Reject the request.\n");
#endif
    memset(&buff[client_socket], 0, sizeof(struct buf));
    close(client_socket);
    return;
}

int parse_request(int client_socket, char *request, int length, char *path, ssize_t *path_len)
{
    char *req = request;

    char *method = (char *)malloc(MAX_PATH_LEN * sizeof(char));
    int t = 0;
    while (req[t] != '\0' && req[t] != ' ' && t < length)
    {
        t++;
    }
    int s = t + 1;
    while (req[s] != '\0' && req[s] != ' ' && s < length)
    {
        s++;
    }

    if (req[t] == '\0')
    {
        free(method);
        close_request(client_socket);
        return -1;
    }
    else if (req[t] == ' ')
    {
        req[t] = '\0';
        strncpy(method, req, t);
#ifdef DEBUG
        printf("t=%d\n", t);
        printf("method:%s\n", method);
#endif
        if (strcmp(method, "GET") != 0)
        {
            close_request(client_socket);
            free(method);
            return -1;
        }
    }

    if (req[s] == '\0')
    {
        free(method);
        close_request(client_socket);
        return -1;
    }
    req[s] = '\0';
    strncpy(path, req + t + 1, s - t);
    *path_len = s - t - 1;
    free(method);

#ifdef DEBUG
    printf("%s\n", path);
#endif
    return 0;
}

int handle_read(int epoll_fd, int client_socket)
{
#ifdef DEBUG
    printf("Reading from client.\n");
#endif
    if (buff[client_socket].have_read == 0)
    {
        while (buff[client_socket].have_read == 0)
        {
            ssize_t valread = read(client_socket, buff[client_socket].read_buffer + buff[client_socket].read_offset, 4096);
#ifdef DEBUG
            printf("Reading valread:%ld\n", valread);
            printf("Reading offset:%d\n", buff[client_socket].read_offset);
#endif
            if (valread == -1)
            {
#ifdef DEBUG
                printf("errno:%d\n", errno);
#endif
                if (check_end(buff[client_socket].read_buffer, buff[client_socket].read_offset))
                {
#ifdef DEBUG
                    printf("End of request.\n");
#endif
                    buff[client_socket].have_read = 1;
                }
                else if (errno == EAGAIN)
                {
                    return 0;
                }
                else if (errno == EPIPE)
                {
                    return -1;
                }
                else if (errno != EAGAIN)
                {
                    handle_error("Reading from client");
                }
            }
            else if (check_end(buff[client_socket].read_buffer, buff[client_socket].read_offset + valread))
            {
#ifdef DEBUG
                printf("End of request.\n");
#endif
                buff[client_socket].read_offset += valread;
                buff[client_socket].have_read = 1;
                //break;
            }
            else if (valread == 0)
            {
                buff[client_socket].have_read = 1;
            }
            else
            {
                buff[client_socket].read_offset += valread;
            }
        }
    }

    return 1;
}

int handle_write(int epoll_fd, int client_socket)
{
#ifdef DEBUG
    printf("Sending to client.\n");
    printf("Client num:%d\n", client_socket);
#endif

    int fd;
    if (buff[client_socket].read_path == 0)
    {
        char *temp_path;
        char *path = (char *)malloc(MAX_PATH_LEN * sizeof(char));
        ssize_t path_len;
        char *buffer = buff[client_socket].read_buffer;
        int ret = parse_request(client_socket, buffer, buff[client_socket].read_offset, path, &path_len);
        if (ret == -1)
        {
            free(path);
            return 0;
        }
        temp_path = path;
        if (path_len == 0)
        {
            send(client_socket, HTTP_STATUS_500, strlen(HTTP_STATUS_500), 0);
            free(path);
            return -1;
        }
        if (path[0] == '/' && path_len == 1)
        {
            send(client_socket, HTTP_STATUS_500, strlen(HTTP_STATUS_500), 0);
            free(path);
            return -1;
        }
        else if (path[0] != '/')
        {
            send(client_socket, HTTP_STATUS_500, strlen(HTTP_STATUS_500), 0);
            free(path);
            return -1;
        }
        else
            temp_path++;
        strcpy(buff[client_socket].path, temp_path);
        buff[client_socket].read_path == 1;
#ifdef DEBUG
        printf("path:%s\n", temp_path);
#endif
        free(path);
    }
#ifdef DEBUG
    printf("Done reading path.\n");
    printf("Client num:%d\n", client_socket);
#endif
    if (buff[client_socket].open_file == 0)
    {
        if ((fd = open(buff[client_socket].path, O_RDONLY | O_NONBLOCK)) == -1) //FILE NOTFOUND
        {
#ifdef DEBUG
            printf("Open file failed.\n");
#endif
            if (errno == ENOENT)
            {
                send(client_socket, HTTP_STATUS_404, strlen(HTTP_STATUS_404), 0);
                return -1;
            }
            else if (errno == 21)
            {
                send(client_socket, HTTP_STATUS_500, strlen(HTTP_STATUS_500), 0);
                return -1;
            }
            else if (errno == EMFILE)
            {
                struct rlimit r;
                if(getrlimit(RLIMIT_NOFILE,&r) < 0)
                    handle_error("getrlimit");
                r.rlim_cur = r.rlim_max = 4096;
                if (setrlimit(RLIMIT_NOFILE, &r) < 0)
                    handle_error("setrlimit");
            }
            else
                handle_error("Opening file");
        }
        else
        {
#ifdef DEBUG
            printf("Open file successfully.\n");
#endif
            buff[client_socket].fd = fd;
            buff[client_socket].open_file = 1;
        }
    }

    if (buff[client_socket].have_read_header == 0)
    {
        struct stat statbuf;
        fstat(fd, &statbuf);
        buff[client_socket].file_size = statbuf.st_size;
        char *length_response = (char *)malloc(1024 * sizeof(char));
        strcpy(buff[client_socket].header_buffer, HTTP_STATUS_200);
        sprintf(length_response, "Content-Length: %d\r\n\r\n", buff[client_socket].file_size);
        strcat(buff[client_socket].header_buffer, length_response);
        buff[client_socket].header_length = strlen(buff[client_socket].header_buffer);
        buff[client_socket].have_read_header = 1;
        free(length_response);
    }

    if (buff[client_socket].have_written_header == 0)
    {
#ifdef DEBUG
        printf("Sending header.\n");
#endif
        while (buff[client_socket].header_offset < buff[client_socket].header_length)
        {
            int ret = send(client_socket, buff[client_socket].header_buffer + buff[client_socket].header_offset, buff[client_socket].header_length - buff[client_socket].header_offset, 0);
            if (ret == -1)
                if (errno == EPIPE)
                {
                    return -2;
                }
                else if (errno == EAGAIN)
                    return 0;
                else if (errno == ECONNRESET)
                {
                    return -2;
                }
                else
                    handle_error("Sending header");

            buff[client_socket].header_offset += ret;
        }
        buff[client_socket].have_written_header = 1;
    }

    if (buff[client_socket].have_written_file == 0)
    {
#ifdef DEBUG
        printf("filesize:%d\n", buff[client_socket].file_size);
#endif
        while (buff[client_socket].file_size > buff[client_socket].write_offset)
        {
#ifdef DEBUG
            printf("client num:%d\n", client_socket);
#endif
            int ret = sendfile(client_socket, buff[client_socket].fd, &buff[client_socket].write_offset, buff[client_socket].file_size);
#ifdef DEBUG
            printf("ret=%d\n", ret);
            printf("write_offset:%ld\n", buff[client_socket].write_offset);
#endif
            if (ret == -1)
            {
#ifdef DEBUG
                printf("errno:%d\n", errno);
#endif
                if (errno == EPIPE)
                {
                    return -2;
                }
                else if (errno == EAGAIN)
                    return 0;
                else if (errno == ECONNRESET)
                {
                    return -2;
                }
                
                else
                    handle_error("Sending file");
            }
            // else if (ret == 0)
            // {
            //     buff[client_socket].have_written_file = 1;
            //     return 1;
            // }
        }

        buff[client_socket].have_written_file = 1;
    }

    if (buff[client_socket].have_written_file)
    {
#ifdef DEBUG
        printf("Sending completed.\n");
#endif
        return 1;
    }
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

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        handle_error("Bind failed");
    }

    if (listen(server_socket, MAX_CONN))
    {
        handle_error("Listen failed");
    }

    printf("\nServer Waiting for client on port 8000.\n");

    epfd = epoll_create(MAX_FDS); // Create epoll.
    if (epfd == -1)
        handle_error("Create epoll");

    ev.events = EPOLLIN;
    ev.data.fd = server_socket;

    if ((epoll_ctl(epfd, EPOLL_CTL_ADD, server_socket, &ev)) == -1) //Register event.
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
                if (client_socket == -1)
                    handle_error("accept");
                set_nonblocking(client_socket);
                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = client_socket;
                if ((epoll_ctl(epfd, EPOLL_CTL_ADD, client_socket, &ev)) == -1)
                    handle_error("epoll connect");
            }
            else if (events[i].events & EPOLLIN)
            {
                client_socket = events[i].data.fd;
                int ret = handle_read(epfd, client_socket);
                if (ret == 1)
                {
                    ev.events = EPOLLOUT;
                    ev.data.fd = client_socket;
                    if (epoll_ctl(epfd, EPOLL_CTL_MOD, client_socket, &ev) == -1)
                        handle_error("epoll modified");
                }
            }
            else if (events[i].events & EPOLLOUT)
            {
                client_socket = events[i].data.fd;
                int ret = handle_write(epfd, client_socket);

                if (ret == 1)
                {
                    close(buff[client_socket].fd);
                    memset(&buff[client_socket], 0, sizeof(struct buf));
                    if (epoll_ctl(epfd, EPOLL_CTL_DEL, client_socket, NULL) == -1)
                        handle_error("epoll delete");
                    close(client_socket);
                }
                else if (ret == -1)
                {
                    memset(&buff[client_socket], 0, sizeof(struct buf));
                    if (epoll_ctl(epfd, EPOLL_CTL_DEL, client_socket, NULL) == -1)
                        handle_error("epoll delete");
                    close(client_socket);
                }
                else if (ret == -2)
                {
                    close(buff[client_socket].fd);
                    memset(&buff[client_socket], 0, sizeof(struct buf));
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
    signal(SIGPIPE, SIG_IGN);
    run_server();
    return 0;
}