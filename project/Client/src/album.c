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
#include <linux/input.h>
#include <dirent.h> // 添加目录操作头文件
#include <ctype.h>  // 添加字符处理头文件

#define LCD_WIDTH 1024
#define LCD_HEIGHT 600
#define LCD_BPP 32
#define BYTES_PER_LINE 4096
#define TOUCH_DEVICE "/dev/input/event6"

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

// 比较函数用于文件名排序
int compare_filenames(const void *a, const void *b)
{
    return strcmp(*(const char **)a, *(const char **)b);
}

// 计算缩放后的尺寸，保持宽高比
void calculate_scaled_size(int src_width, int src_height,
                           int *dst_width, int *dst_height)
{
    float src_ratio = (float)src_width / src_height;
    float dst_ratio = (float)LCD_WIDTH / LCD_HEIGHT;

    if (src_ratio > dst_ratio)
    {
        // 以宽度为基准缩放
        *dst_width = LCD_WIDTH;
        *dst_height = (int)(LCD_WIDTH / src_ratio);
    }
    else
    {
        // 以高度为基准缩放
        *dst_height = LCD_HEIGHT;
        *dst_width = (int)(LCD_HEIGHT * src_ratio);
    }
}

// 高质量图像缩放 (双线性插值)
void scale_image_high_quality(unsigned char *src, int src_width, int src_height,
                              unsigned char *dst, int dst_width, int dst_height)
{
    float x_ratio = (float)(src_width - 1) / dst_width;
    float y_ratio = (float)(src_height - 1) / dst_height;
    int x, y, i;

    for (y = 0; y < dst_height; y++)
    {
        for (x = 0; x < dst_width; x++)
        {
            float src_x = x * x_ratio;
            float src_y = y * y_ratio;

            int x1 = (int)src_x;
            int y1 = (int)src_y;
            int x2 = (x1 < src_width - 1) ? x1 + 1 : x1;
            int y2 = (y1 < src_height - 1) ? y1 + 1 : y1;

            float x_weight = src_x - x1;
            float y_weight = src_y - y1;

            // 获取四个相邻像素
            unsigned char *p11 = src + (y1 * src_width + x1) * 3;
            unsigned char *p12 = src + (y1 * src_width + x2) * 3;
            unsigned char *p21 = src + (y2 * src_width + x1) * 3;
            unsigned char *p22 = src + (y2 * src_width + x2) * 3;

            // 双线性插值
            for (i = 0; i < 3; i++)
            {
                float value =
                    p11[i] * (1 - x_weight) * (1 - y_weight) + p12[i] * x_weight * (1 - y_weight) + p21[i] * (1 - x_weight) * y_weight + p22[i] * x_weight * y_weight;

                dst[(y * dst_width + x) * 3 + i] = (unsigned char)value;
            }
        }
    }
}

// 将图像居中显示在LCD上
void center_image_on_lcd(unsigned char *scaled_image, int scaled_width, int scaled_height,
                         unsigned char *fb_buffer, int fb_width, int fb_height, int fb_line_length)
{
    int start_x = (fb_width - scaled_width) / 2;
    int start_y = (fb_height - scaled_height) / 2;
    int x, y;

    // 清屏为黑色
    memset(fb_buffer, 0, fb_line_length * fb_height);

    // 居中绘制图像
    for (y = 0; y < scaled_height; y++)
    {
        for (x = 0; x < scaled_width; x++)
        {
            int fb_y = start_y + y;
            if (fb_y < 0 || fb_y >= fb_height)
                continue;

            int fb_x = start_x + x;
            if (fb_x < 0 || fb_x >= fb_width)
                continue;

            unsigned char *src = scaled_image + (y * scaled_width + x) * 3;
            unsigned char *dst = fb_buffer + fb_y * fb_line_length + fb_x * 4;

            // 转换为目标像素格式: ARGB8888 (实际为BGRA在内存中的排列)
            dst[0] = src[2]; // B
            dst[1] = src[1]; // G
            dst[2] = src[0]; // R
            dst[3] = 0xFF;   // A (不透明)
        }
    }
}

int album(int argc, char *argv[])
{
    struct jpeg_decompress_struct cinfo;
    struct my_error_mgr jerr;
    FILE *infile;
    JSAMPARRAY buffer;
    int row_stride;
    unsigned char *raw_image = NULL;
    unsigned char *scaled_image = NULL;
    int fb_fd = 0;
    char *fbp = NULL;
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    long int screensize = 0;
    int scaled_width, scaled_height;

    // 添加目录读取相关变量
    DIR *dir;
    struct dirent *ent;
    char **filenames = NULL;
    int file_count = 0;
    int current_index = 0;
    // char *dir_path = "./Picture"; // 默认图片目录
    char *dir_path = NULL; // 默认图片目录
    // 如果命令行参数指定了目录，则使用指定的目录
    if (argc > 1)
    {
        dir_path = argv[2];
    }

    // 读取目录中的JPEG文件
    if ((dir = opendir(dir_path)) != NULL)
    {
        while ((ent = readdir(dir)) != NULL)
        {
            // 检查是否为普通文件且扩展名为.jpg或.jpeg(不区分大小写)
            if (ent->d_type == DT_REG && (strstr(ent->d_name, ".jpg") || strstr(ent->d_name, ".JPG") || strstr(ent->d_name, ".jpeg") || strstr(ent->d_name, ".JPEG")))
            {
                // 分配内存并保存完整路径
                filenames = realloc(filenames, (file_count + 1) * sizeof(char *));
                filenames[file_count] = malloc(strlen(dir_path) + strlen(ent->d_name) + 2);
                sprintf(filenames[file_count], "%s/%s", dir_path, ent->d_name);
                file_count++;
            }
        }
        closedir(dir);

        // 按文件名排序
        if (file_count > 0)
        {
            qsort(filenames, file_count, sizeof(char *), compare_filenames);
        }
        else
        {
            fprintf(stderr, "在目录 %s 中没有找到JPEG图片\n", dir_path);
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        perror("无法打开图片目录");
        exit(EXIT_FAILURE);
    }

    // 打开触摸屏设备
    int fd = open(TOUCH_DEVICE, O_RDONLY);
    if (fd == -1)
    {
        perror("无法打开触摸屏设备");
        exit(EXIT_FAILURE);
    }

    struct input_event ev;
    int x = 0, y = 0;
    int touch_pressed = 0;

    while (1)
    {
        // 1. 打开当前JPEG文件
        if ((infile = fopen(filenames[current_index], "rb")) == NULL)
        {
            fprintf(stderr, "无法打开图像文件 %s\n", filenames[current_index]);
            continue; // 跳过无法打开的文件，继续下一个
        }

        // 2. 初始化JPEG解压结构
        cinfo.err = jpeg_std_error(&jerr.pub);
        jerr.pub.error_exit = my_error_exit;

        if (setjmp(jerr.setjmp_buffer))
        {
            jpeg_destroy_decompress(&cinfo);
            fclose(infile);
            if (raw_image)
                free(raw_image);
            if (scaled_image)
                free(scaled_image);
            continue; // 跳过有问题的文件，继续下一个
        }

        jpeg_create_decompress(&cinfo);
        jpeg_stdio_src(&cinfo, infile);
        jpeg_read_header(&cinfo, TRUE);

        // 3. 开始解压JPEG
        cinfo.out_color_space = JCS_RGB; // 设置为RGB格式

        jpeg_start_decompress(&cinfo);

        // 计算缩放后尺寸 (保持宽高比)
        calculate_scaled_size(cinfo.output_width, cinfo.output_height,
                              &scaled_width, &scaled_height);

        printf("当前图片: %s\n原始图像尺寸: %dx%d, 缩放后尺寸: %dx%d\n",
               filenames[current_index],
               cinfo.output_width, cinfo.output_height,
               scaled_width, scaled_height);

        // 分配内存存储原始图像数据
        raw_image = (unsigned char *)malloc(cinfo.output_width * cinfo.output_height * 3);
        if (!raw_image)
        {
            fprintf(stderr, "内存分配失败\n");
            jpeg_destroy_decompress(&cinfo);
            fclose(infile);
            continue;
        }

        // 逐行读取图像数据
        row_stride = cinfo.output_width * cinfo.output_components;
        buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo, JPOOL_IMAGE, row_stride, 1);

        while (cinfo.output_scanline < cinfo.output_height)
        {
            jpeg_read_scanlines(&cinfo, buffer, 1);
            memcpy(raw_image + (cinfo.output_scanline - 1) * row_stride, buffer[0], row_stride);
        }

        // 4. 完成解压
        jpeg_finish_decompress(&cinfo);
        jpeg_destroy_decompress(&cinfo);
        fclose(infile);

        // 5. 缩放图像
        scaled_image = (unsigned char *)malloc(scaled_width * scaled_height * 3);
        if (!scaled_image)
        {
            fprintf(stderr, "内存分配失败\n");
            free(raw_image);
            continue;
        }

        scale_image_high_quality(raw_image, cinfo.output_width, cinfo.output_height,
                                 scaled_image, scaled_width, scaled_height);
        free(raw_image); // 原始图像数据不再需要

        // 6. 打开帧缓冲区设备
        fb_fd = open("/dev/fb0", O_RDWR);
        if (fb_fd == -1)
        {
            perror("无法打开帧缓冲区设备");
            free(scaled_image);
            continue;
        }

        // 获取屏幕信息
        if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo))
        {
            perror("无法获取可变屏幕信息");
            close(fb_fd);
            free(scaled_image);
            continue;
        }

        // 7. 映射帧缓冲区到内存
        if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo))
        {
            perror("无法获取固定屏幕信息");
            close(fb_fd);
            free(scaled_image);
            continue;
        }

        screensize = finfo.line_length * vinfo.yres;
        fbp = (char *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
        if ((void *)fbp == MAP_FAILED)
        {
            perror("无法映射帧缓冲区");
            close(fb_fd);
            free(scaled_image);
            continue;
        }

        // 8. 将图像居中显示到LCD
        center_image_on_lcd(scaled_image, scaled_width, scaled_height,
                            (unsigned char *)fbp, LCD_WIDTH, LCD_HEIGHT, finfo.line_length);

        // 9. 处理触摸事件
        while (1)
        {
            if (read(fd, &ev, sizeof(struct input_event)) < 0)
            {
                perror("读取输入事件失败");
                break;
            }

            if (ev.type == EV_ABS)
            {
                switch (ev.code)
                {
                case ABS_X:
                    x = ev.value;
                    break;
                case ABS_Y:
                    y = ev.value;
                    break;
                case ABS_PRESSURE: // 有些触摸屏用这个表示触摸压力
                    touch_pressed = (ev.value > 0);
                    break;
                case ABS_MT_POSITION_X: // 多点触控X坐标
                    x = ev.value;
                    break;
                case ABS_MT_POSITION_Y: // 多点触控Y坐标
                    y = ev.value;
                    break;
                case ABS_MT_TRACKING_ID: // 多点触控ID
                    touch_pressed = (ev.value != -1);
                    break;
                }
            }
            else if (ev.type == EV_KEY && ev.code == BTN_TOUCH)
            {
                // 单点触控的按下/释放事件
                touch_pressed = ev.value;
            }
            else if (ev.type == EV_SYN && ev.code == SYN_REPORT)
            {
                if (!touch_pressed)
                {
                    printf("触摸坐标: x:%d, y:%d\n", x, y);
                    if (x > 0 && x < 512 && y > 0 && y < 600)
                    { // 左半屏 - 上一张
                        current_index = (current_index - 1 + file_count) % file_count;
                        break;
                    }
                    else if (x > 900 && x < 1024 && y > 0 && y < 100)
                    { // 右上角 - 退出
                        memset((unsigned char *)fbp, 0, finfo.line_length * LCD_HEIGHT);
                        munmap(fbp, screensize);
                        close(fb_fd);
                        free(scaled_image);

                        // 释放文件名数组内存
                        for (int i = 0; i < file_count; i++)
                        {
                            free(filenames[i]);
                        }
                        free(filenames);

                        close(fd);
                        exit(EXIT_SUCCESS);
                    }
                    else if (x > 512 && x < 1024 && y > 0 && y < 600)
                    { // 右半屏 - 下一张
                        current_index = (current_index + 1) % file_count;
                        break;
                    }
                }
            }
        }

        // 10. 清理当前图片资源
        munmap(fbp, screensize);
        close(fb_fd);
        free(scaled_image);
    }

    // 释放文件名数组内存 (正常情况下不会执行到这里)
    for (int i = 0; i < file_count; i++)
    {
        free(filenames[i]);
    }
    free(filenames);

    close(fd);
    printf("图片浏览结束\n");
    exit(EXIT_SUCCESS);
}