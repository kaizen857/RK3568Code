#include <stdio.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int main(int argc, char const *argv[])
{
    // 1. 打开文件
    int fd = open("/dev/fb0", O_RDWR);
    if (fd == -1)
    {
        printf("open file error\n");
        return -1;
    }

    // 2. 写入数据（颜色）
    int red_color = 0xFF0000;
    for (int i = 0; i < 1024 * 600; ++i)
    {
        write(fd, &red_color, 4);
    }

    // 3. 保存退出（自动保存的功能）
    int ret = close(fd);
    if (ret == -1)
    {
        printf("close file error\n");
        return -2;
    }

    return 0; // 程序的出口，只要程序执行到这里，便会无条件结束
}