// 客户端 可发送和接受数据
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h> //提供专属结构体的头文件和 htons
#include <string.h>
#include <netinet/in.h> //提供宏定义 INADDR_ANY
#include <pthread.h>

void *ClientSolve(void *arg)
{
    char recv_buf[128] = {0};

    int cur_server = *((int *)arg); // 获取外面传入的当前客户端套接字描述符值

    // 接受服务端信息
    while (1)
    {
        bzero(recv_buf, sizeof(recv_buf));
        // 客户端不断接受服务器的请求
        read(cur_server, recv_buf, sizeof(recv_buf));
        printf("收到的服务端信息:%s\n", recv_buf);

        if (0 == strncmp(recv_buf, "quit", 4))
        {
            break;
        }
    }
}

int main(int argc, char const *argv[])
{
    pthread_t clentthr;
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
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    server_addr.sin_addr.s_addr = inet_addr("192.168.39.10");

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

    // ----------------发送信息-------------------------------
    char send_buf[100] = {0};
    while (1)
    {
        bzero(send_buf, 100);
        scanf("%s", send_buf);
        write(socket_fd, send_buf, strlen(send_buf));
        if (strncmp(send_buf, "quit", 4) == 0)
        {
            break;
        }
        /*
        ssize_t send(
        int sockfd,             // sockfd返回值
        const void *buf,        // 发送信息的存储区域
        size_t len,             // 发送信息的长度
        int flags);             // 0，自动匹配
         */
        pthread_t clentthr;
        pthread_create(&clentthr, NULL, ClientSolve, (void *)&socket_fd); // 创建一个线程，传入当前连接成功的客户端套接字描述符
    }

    return 0;
}