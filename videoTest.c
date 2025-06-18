#include <dirent.h>
#include <fcntl.h>
#include <jpeglib.h>
#include <linux/fb.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>

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
int load_jpeg(const char *filename, unsigned char **image, int *width,
              int *height)
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
    buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo, JPOOL_IMAGE,
                                        row_stride, 1);

    while (cinfo.output_scanline < cinfo.output_height)
    {
        jpeg_read_scanlines(&cinfo, buffer, 1);
        memcpy(*image + (cinfo.output_scanline - 1) * row_stride, buffer[0],
               row_stride);
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    fclose(infile);
    return 1;
}

// 显示图像到帧缓冲区
void display_image(unsigned char *image, int width, int height, char *fbp,
                   int fb_width, int fb_height, int fb_line_length)
{
    int start_x = (fb_width - width) / 2;
    int start_y = (fb_height - height) / 2;
    int x, y;

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

int compare_filenames(const void *a, const void *b)
{
    const char *fa = *(const char **)a;
    const char *fb = *(const char **)b;

    // 提取文件名中的数字部分
    int num_a = atoi(strrchr(fa, '/') + 1); // 忽略路径前缀
    int num_b = atoi(strrchr(fb, '/') + 1);

    return (num_a - num_b);
}

void *play_audio_thread(void *arg)
{
    char *dir_path = (char *)arg;
    char audio_file[256];
    char audio_name[256];

    // 从路径中提取目录名（如"./video3" -> "video3"）
    const char *last_slash = strrchr(dir_path, '/');
    if (last_slash != NULL)
    {
        strncpy(audio_name, last_slash + 1, sizeof(audio_name) - 1);
    }
    else
    {
        strncpy(audio_name, dir_path, sizeof(audio_name) - 1);
    }
    audio_name[sizeof(audio_name) - 1] = '\0'; // 确保字符串终止

    snprintf(audio_file, sizeof(audio_file), "%s/%s.mp3", dir_path, audio_name);

    char command[512];
    snprintf(command, sizeof(command),
             "sleep 0.4 && amixer -c 0 set Master 30%% && mpg123 -o alsa -v \"%s\"",
             audio_file);

    system(command);
    return NULL;
}

int main(int argc, char **argv)
{
    system("mpg123 -o alsa -v --delay 0.1 /dev/null 2>/dev/null");
    DIR *dir;
    struct dirent *ent;
    char **filenames = NULL;
    int file_count = 0;
    int fb_fd;
    char *fbp = NULL;
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    struct timeval tv;
    int current_image = 0;

    char *dir_path = "./video2"; // 默认路径
    if (argc > 1)
    {
        dir_path = argv[1];
    }

    if ((dir = opendir(dir_path)) != NULL)
    {
        while ((ent = readdir(dir)) != NULL)
        {
            if (ent->d_type == DT_REG && strstr(ent->d_name, ".jpg"))
            {
                filenames = realloc(filenames, (file_count + 1) * sizeof(char *));
                filenames[file_count] = malloc(strlen(ent->d_name) + strlen(dir_path) + 2); // +2 for '/' and null terminator
                sprintf(filenames[file_count], "%s/%s", dir_path, ent->d_name);
                file_count++;
            }
        }
        closedir(dir);
    }
    qsort(filenames, file_count, sizeof(char *), compare_filenames);

    if (file_count == 0)
    {
        fprintf(stderr, "没有找到图像文件\n");
        return 1;
    }

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

    fbp = mmap(0, finfo.line_length * vinfo.yres, PROT_READ | PROT_WRITE,
               MAP_SHARED, fb_fd, 0);
    if ((void *)fbp == MAP_FAILED)
    {
        perror("无法映射帧缓冲区");
        close(fb_fd);
        return 1;
    }

    long long frame_start_time, frame_end_time, elapsed_time, delay_time;
    pthread_t audio_thread;
    if (pthread_create(&audio_thread, NULL, play_audio_thread, dir_path) != 0)
    {
        perror("无法创建音频线程");
        return 1;
    }
    pthread_detach(audio_thread); // 分离线程，避免资源泄漏
    while (1)
    {
        // 记录一帧开始的时间戳
        gettimeofday(&tv, NULL);
        frame_start_time = tv.tv_sec * 1000000LL + tv.tv_usec;

        // 加载和显示图像
        unsigned char *image = NULL;
        int width, height;

        if (load_jpeg(filenames[current_image], &image, &width, &height))
        {
            // 使用从系统中获取的实际屏幕分辨率
            display_image(image, width, height, fbp, vinfo.xres, vinfo.yres,
                          finfo.line_length);
            free(image);
        }

        // 切换到下一张图片
        current_image = (current_image + 1);
        if (current_image == file_count)
        {
            break;
        }

        // 记录一帧结束的时间戳
        gettimeofday(&tv, NULL);
        frame_end_time = tv.tv_sec * 1000000LL + tv.tv_usec;

        // 计算处理该帧所花费的时间
        elapsed_time = frame_end_time - frame_start_time;

        // 帧间延迟
        delay_time = FRAME_DELAY - elapsed_time;
        if (delay_time > 0)
        {
            struct timespec ts;
            ts.tv_sec = delay_time / 1000000;
            ts.tv_nsec = (delay_time % 1000000) * 1000;
            nanosleep(&ts, NULL);
        }
        // 如果处理时间已经超过或等于目标帧时长，则不休眠，立即开始下一帧
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