#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <jpeglib.h>
#include <setjmp.h>
#include <string.h>
#include <sys/time.h>
#include <dirent.h>

#define LCD_WIDTH 1024
#define LCD_HEIGHT 600
#define TARGET_FPS 30
#define FRAME_DELAY (1000000 / TARGET_FPS)

// 错误处理结构体
struct my_error_mgr
{
    struct jpeg_error_mgr pub;
    jmp_buf setjmp_buffer;
};

typedef struct my_error_mgr *my_error_ptr;

// JPEG错误处理回调
METHODDEF(void)
my_error_exit(j_common_ptr cinfo)
{
    my_error_ptr myerr = (my_error_ptr)cinfo->err;
    (*cinfo->err->output_message)(cinfo);
    longjmp(myerr->setjmp_buffer, 1);
}

// 加载JPEG图像
int load_jpeg(const char *filename, unsigned char **image, int *width, int *height)
{
    struct jpeg_decompress_struct cinfo;
    struct my_error_mgr jerr;
    FILE *infile;
    JSAMPARRAY buffer;
    int row_stride;

    if ((infile = fopen(filename, "rb")) == NULL)
    {
        fprintf(stderr, "无法打开图像文件 %s\n", filename);
        return 0;
    }

    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = my_error_exit;

    if (setjmp(jerr.setjmp_buffer))
    {
        jpeg_destroy_decompress(&cinfo);
        fclose(infile);
        return 0;
    }

    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, infile);
    jpeg_read_header(&cinfo, TRUE);
    cinfo.out_color_space = JCS_RGB;
    jpeg_start_decompress(&cinfo);

    *width = cinfo.output_width;
    *height = cinfo.output_height;
    *image = (unsigned char *)malloc(*width * *height * 3);
    if (!*image)
    {
        fprintf(stderr, "内存分配失败\n");
        jpeg_destroy_decompress(&cinfo);
        fclose(infile);
        return 0;
    }

    row_stride = cinfo.output_width * cinfo.output_components;
    buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo, JPOOL_IMAGE, row_stride, 1);

    while (cinfo.output_scanline < cinfo.output_height)
    {
        jpeg_read_scanlines(&cinfo, buffer, 1);
        memcpy(*image + (cinfo.output_scanline - 1) * row_stride, buffer[0], row_stride);
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    fclose(infile);
    return 1;
}

// 显示图像到帧缓冲区
void display_image(unsigned char *image, int width, int height,
                   char *fbp, int fb_width, int fb_height, int fb_line_length)
{
    int start_x = (fb_width - width) / 2;
    int start_y = (fb_height - height) / 2;
    int x, y;

    // 清屏为黑色
    memset(fbp, 0, fb_line_length * fb_height);

    // 居中绘制图像
    for (y = 0; y < height; y++)
    {
        for (x = 0; x < width; x++)
        {
            int fb_y = start_y + y;
            if (fb_y < 0 || fb_y >= fb_height)
                continue;

            int fb_x = start_x + x;
            if (fb_x < 0 || fb_x >= fb_width)
                continue;

            unsigned char *src = image + (y * width + x) * 3;
            unsigned char *dst = fbp + fb_y * fb_line_length + fb_x * 4;

            dst[0] = src[2]; // B
            dst[1] = src[1]; // G
            dst[2] = src[0]; // R
            dst[3] = 0xFF;   // A
        }
    }
}

int main()
{
    DIR *dir;
    struct dirent *ent;
    char **filenames = NULL;
    int file_count = 0;
    int fb_fd;
    char *fbp = NULL;
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    struct timeval tv;
    long long last_frame_time = 0;
    long long current_time;
    int current_image = 0;

    // 1. 获取图像文件列表
    if ((dir = opendir("./adjusted_images")) != NULL)
    {
        while ((ent = readdir(dir)) != NULL)
        {
            if (ent->d_type == DT_REG && strstr(ent->d_name, ".jpg"))
            {
                filenames = realloc(filenames, (file_count + 1) * sizeof(char *));
                filenames[file_count] = malloc(strlen(ent->d_name) + 20);
                sprintf(filenames[file_count], "./adjusted_images/%s", ent->d_name);
                file_count++;
            }
        }
        closedir(dir);
    }

    if (file_count == 0)
    {
        fprintf(stderr, "没有找到图像文件\n");
        return 1;
    }

    // 2. 初始化帧缓冲区
    fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd == -1)
    {
        perror("无法打开帧缓冲区设备");
        return 1;
    }

    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo))
    {
        perror("无法获取可变屏幕信息");
        close(fb_fd);
        return 1;
    }

    if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo))
    {
        perror("无法获取固定屏幕信息");
        close(fb_fd);
        return 1;
    }

    fbp = mmap(0, finfo.line_length * vinfo.yres, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if ((void *)fbp == MAP_FAILED)
    {
        perror("无法映射帧缓冲区");
        close(fb_fd);
        return 1;
    }

    // 3. 主显示循环
    while (1)
    {
        gettimeofday(&tv, NULL);
        current_time = tv.tv_sec * 1000000 + tv.tv_usec;

        if (current_time - last_frame_time >= FRAME_DELAY)
        {
            unsigned char *image = NULL;
            int width, height;

            if (load_jpeg(filenames[current_image], &image, &width, &height))
            {
                display_image(image, width, height, fbp,
                              LCD_WIDTH, LCD_HEIGHT, finfo.line_length);
                free(image);
            }

            current_image = (current_image + 1) % file_count;
            last_frame_time = current_time;
        }

        usleep(10000); // 短暂休眠以减少CPU占用
    }

    // 清理资源
    munmap(fbp, finfo.line_length * vinfo.yres);
    close(fb_fd);
    for (int i = 0; i < file_count; i++)
    {
        free(filenames[i]);
    }
    free(filenames);

    return 0;
}