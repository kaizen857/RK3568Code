#include "../include/server.h"
#include <stdio.h>
#include <string.h>
int main(int argc, char *argv[])
{
    if (argc > 1)
    {
        if (strcmp(argv[1], "server") == 0)
        {
            server(argc, argv);
        }
    }
    else
    {
        printf("Please enter the correct command\n");
    }
}