#include "../include/client.h"
#include <arpa/inet.h>
#include <jpeglib.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <stdatomic.h>
#include <sys/socket.h>
#include <unistd.h>

atomic_bool socketLock; // 0:未被使用  1:正在被使用

int sendMessage(int socket_fd, char *buf, int bufSize)
{
    atomic_bool disire = 0;
    while (!atomic_compare_exchange_strong(&socketLock, &disire, TRUE))
        ;                                     // 等待socketLock为0
    int ret = write(socket_fd, buf, bufSize); // 发送数据
    atomic_store(&disire, TRUE);
    while (!atomic_compare_exchange_strong(&socketLock, &disire, FALSE))
        ;
    return ret;
}

void terminalSovle(void *arg)
{
    // TODO:终端输入
    int socket_fd = *(int *)arg;
    while (1)
    {
        printf("请输入指令：\n");
        char buf[1024] = {0};
        fgets(buf, sizeof(buf), stdin);
        buf[strlen(buf) - 1] = '\0';
        printf("buf = %s\n", buf);
        if (strcmp(buf, "exit") == 0)
        {
            printf("退出程序\n");
        }
        else
        {
            // 向主机发送字符串
            sendMessage(socket_fd, buf, strlen(buf));
        }
    }
}

void *voiceSovle(void *arg)
{
    // TODO:语音输入
    return NULL;
}

// 向主机发送信息

int client(int argc, char *argv[])
{
    atomic_init(&socketLock, 0);
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0); // socket
    if (socket_fd == -1)
    {
        perror("socket error");
        return -1;
    }

    struct sockaddr_in server_addr; // server addr
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    server_addr.sin_addr.s_addr = inet_addr(argv[2]);

    int con_ret = connect(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    /*
    int connect(
    int sockfd,                     // socket函数返回值
    const struct sockaddr *addr,    // 结构体指针
    socklen_t addrlen);             // 结构体大小
    */
    if (con_ret == -1)
    {
        perror("connect error");
        return -1;
    }
    terminalSovle((void *)&socket_fd);

    return 0;
}