#include "../include/client.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

int main(int argc, char *argv[])
{
    if (argc > 1)
    {
        if (strcmp(argv[1], "album") == 0) // album ./Picture
        {
            album(argc, argv);
        }
        else if (strcmp(argv[1], "client") == 0) // client 192.168.39.10
        {
            client(argc, argv);
        }
        else if (strcmp(argv[1], "video") == 0) // video ./video
        {
            video(argc, argv);
        }
    }
    else
    {
        printf("please input valid command");
    }
}