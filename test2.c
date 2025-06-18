#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <inttypes.h>

int main(int argc, char const *argv[])
{
    // 1. 打开两个文件（lcd屏幕、bmp图片）
    int lcd_fd = open("/dev/fb0", O_RDWR);
    int bmp_fd = open("image.bmp", O_RDWR);
    if (lcd_fd == -1 || bmp_fd == -1)
    {
        printf("fail to open file\n");
        return -1;
    }

    // 2. 先获取bmp图片的像素数据
    lseek(bmp_fd, 54, SEEK_SET); // 跳过BMP文件头和DIB头
    int bmp_width = 1024;
    int bmp_height = 573;
    char buf[bmp_width * bmp_height * 3];
    bzero(buf, bmp_width * bmp_height * 3);
    // bzero是用来清空数组的
    read(bmp_fd, buf, bmp_width * bmp_height * 3);

    // 3. 写入获取到的像素数据到lcd屏幕文件中
    int *mmap_start = (int *)mmap(NULL, 1024 * 600 * 4,
                                  PROT_READ | PROT_WRITE,
                                  MAP_SHARED, lcd_fd, 0);
    if (mmap_start == (void *)-1)
    {
        printf("mmap error\n");
        return -2;
    }

    int x, y, n;
    for (y = 0, n = 0; y < bmp_height; y++)
    {
        for (x = 0; x < bmp_width; x++, n += 3)
        {
            // 将BMP图像数据转换为32位颜色格式并写入Framebuffer
            uint32_t color = (buf[n + 2] << 16) | (buf[n + 1] << 8) | buf[n];

            // 计算在Framebuffer中的位置
            int fb_x = x;
            int fb_y = 600 - y - 1; // BMP图像是从下到上存储的
            int fb_pos = fb_y * 1024 + fb_x;

            // 写入颜色值
            mmap_start[fb_pos] = color;
        }
    }

    munmap(mmap_start, 1024 * 600 * 4);

    // 4. 关闭两个文件（lcd屏幕、bmp图片）
    close(lcd_fd);
    close(bmp_fd);

    return 0;
}
