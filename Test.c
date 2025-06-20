
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

int main(int argc, char *argv[])
{
    if (argc > 1 && strcmp(argv[1], "album") == 0) // album ./Picture
    {
    }
    else if (argc > 1) // 192.168.39.10
    {
        printf("%s", argv[1]);
    }
    else
    {
        printf("Please enter a valid IP address or 'album'");
    }
}