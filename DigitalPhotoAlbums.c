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

int main()
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
}