// 服务端 可收发信息
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h> //提供专属结构体的头文件和 htons
#include <string.h>
#include <netinet/in.h> //提供宏定义 INADDR_ANY
#include <pthread.h>
#include <sys/un.h>
#include <unistd.h>

void *ClientSolve(void *arg)
{
    char send_buf[128] = {0};

    int cur_client = *((int *)arg); // 获取外面传入的当前客户端套接字描述符值

    // 开始和客户端进行沟通
    while (1)
    {
        bzero(send_buf, sizeof(send_buf));
        scanf("%s", send_buf);
        write(cur_client, send_buf, strlen(send_buf));
    }
}

int main(int argc, char const *argv[])
{
    // ----------------申请套接字-------------------------------------
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    /*
    int socket(
    int domain,             // AF_LOCAL--本地回环网络通信
                            // AF_INET--IPV4协议
                            // AF_INET6--IPV6协议
    int type,               // SOCK_STREAM 数据流形式---面向连接TCP协议
                            // SOCK_DGRAM 数据报文形式---UDP协议
    int protocol);          // IPPROTO_TCP IPPROTO_UDP
                            // 如果你选填0 则自动选择和type对应的默认协议
     */
    if (socket_fd == -1)
    {
        perror("socket error");
        return -1;
    }
    // ----------------配置TCP-------------------------------
    /*
    struct sockaddr_in{
        sa_family_t   sin_family;   //地址族
        uint16_t      sin_port;     //端口号
        struct in_addr    sin_addr;  //32位IP地址
        char     sin_zero;      //预留未使用
    };
    struct in_addr{
        In_addr_t  s_addr;    //32位IPv4地址
    };
    */
    struct sockaddr_in server_addr;
    socklen_t len = sizeof(server_addr);
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    server_addr.sin_addr.s_addr = inet_addr("192.168.39.10");

    int bind_ret = bind(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    /*int bind(
    int sockfd,                     // socket返回值
    const struct sockaddr *addr,    // 存放配置好的IP结构体
    socklen_t addrlen);             // 结构体的大小
    */
    if (bind_ret == -1)
    {
        perror("bind error");
        return -1;
    }

    // 从内核socket资源中获取bind后的信息-----查看是否正确
    struct sockaddr_in getinfo;
    memset(&getinfo, 0, sizeof(getinfo));
    if (-1 == getsockname(socket_fd, (struct sockaddr *)&getinfo, &len))
    {
        perror("getsockname err");
    }
    printf("服务器自己的IP:%s\n", inet_ntoa(getinfo.sin_addr)); // ntohl(getinfo.sin_addr.s_addr));
    printf("服务器自己的端口号:%d\n", ntohs(getinfo.sin_port));

    int listen_ret = listen(socket_fd, 4);
    /*int listen(
    int sockfd,                     // socket返回值
    int backlog);*/
    // 连接上限值
    if (listen_ret == -1)
    {
        perror("listen_ret error");
        return -1;
    }

    // ----------------接收信息-------------------------------
    struct sockaddr_in client_addr;
    int client_connect = accept(socket_fd, (struct sockaddr *)&client_addr, &len);
    /*int accept(
    int sockfd,                     // socket返回值
    struct sockaddr *addr,          // 客户端的struct sockaddr_in结构体指针
    socklen_t *addrlen);*/
    // 结构体大小
    if (client_connect == -1)
    {
        perror("accept error");
        return -1;
    }
    else
        printf("连接成功\n");

    char buf[100] = {0};
    while (1)
    {
        bzero(buf, 100);
        read(client_connect, buf, sizeof(buf));
        printf("%s\n", buf);
        if (strncmp(buf, "quit", 4) == 0)
        {
            break;
        }
        pthread_t clentthr;
        pthread_create(&clentthr, NULL, ClientSolve, (void *)&client_connect); // 创建一个线程，传入当前连接成功的客户端套接字描述符
    }

    return 0;
}