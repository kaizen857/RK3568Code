#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <unistd.h>

int main(int args, char **argv)
{
    struct fb_fix_screeninfo fb_fix; // 固定参数信息
    struct fb_var_screeninfo fb_var; // 可变参数信息
    int fd = open("/dev/fb0", O_RDWR);
    if (fd < 0)
    {
        perror("open fb failed");
        return -1;
    }
    ioctl(fd, FBIOGET_FSCREENINFO, &fb_fix); // 获取固定参数信息
    ioctl(fd, FBIOGET_VSCREENINFO, &fb_var); // 获取可变参数信息
    printf(
        "分辨率：%d * %d\n"
        "像素深度：%d bit\n"
        "像素格式: R<%d %d> G<%d %d> B<%d %d>\n"
        "每行所占大小：%d 字节\n",
        fb_var.xres, fb_var.yres,
        fb_var.bits_per_pixel,
        fb_var.red.offset, fb_var.red.length,
        fb_var.green.offset, fb_var.green.length,
        fb_var.blue.offset, fb_var.blue.length,
        fb_fix.line_length);
    close(fd);
    return 0;
}