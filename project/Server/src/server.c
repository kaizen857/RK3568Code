#include "../include/server.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

char question[4096];
char buffer[4096];
char buffer1[8192];
char cmd[8192];
char result[8192];

void shift_string_left(char *str, size_t k)
{
    size_t len = strlen(str);
    if (k >= len)
    {
        // 若 k 超过字符串长度，直接清空字符串
        str[0] = '\0';
        return;
    }
    // 计算需要移动的字节数（从第 k 个字符到末尾）
    size_t bytes_to_move = len - k + 1; // +1 包含结尾的 '\0'
    // 使用 memmove 前移字符串
    memmove(str, str + k, bytes_to_move);
}

int handleMessage(int socket, char *message, size_t lens)
{
    // TODO:消息处理
    memset(question, 0, sizeof(question));
    memset(buffer, 0, sizeof(buffer));
    memset(buffer1, 0, sizeof(buffer1));
    memcpy(question, message, lens);
    if (question[0] == '1') // 常规消息（文字信息）
    {
        strcpy(question, question + 1);
        if (strcmp(question, "exit") == 0)
        {
            return 1;
        }
        printf("问题: %s\n", question);
        sprintf(cmd, "python3 deepseek.py %s", question);
        FILE *fp = popen(cmd, "r");
        if (fp == NULL)
        {
            perror("无法加载模型\n");
        }

        // 读取chatglm 模型的输出内容
        while (1)
        {
            if (fgets(buffer, sizeof(buffer), fp) == NULL)
            {
                printf("加载完毕\n");
                break;
            }
            strcat(buffer1, buffer);
        }

        // printf("search_results： %s\n", search_results);
        char *p = strstr(buffer1, "</think>");
        if (p != NULL)
        {
            shift_string_left(p, 9);
            strcpy(result, p);
            write(socket, result, strlen(result));
        }
        else
        {
            write(socket, "加载失败", 13);
        }
        return 0;
    }
    else if (question[0] == '2') // 语音文件
    {
    }
    return 0;
}

int server(int argc, char *argv[])
{
    // TODO:Server
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
    server_addr.sin_addr.s_addr = inet_addr(argv[2]); //"192.168.39.10"

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
    {
        printf("连接成功\n");
    }
    char buf[4096];
    while (1)
    {
        memset(buf, 0, sizeof(buf));
        read(client_connect, buf, sizeof(buf));
        if (handleMessage(client_connect, buf, strlen(buf)))
        {
            break;
        }
    }

    return 0;
}