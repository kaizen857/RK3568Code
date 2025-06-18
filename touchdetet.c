#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/input.h>
#include <errno.h>

#define TOUCH_DEVICE "/dev/input/event6"

int main()
{
    int fd;
    struct input_event ev;
    int x = 0, y = 0;
    int touch_pressed = 0;

    // 打开触摸屏设备
    fd = open(TOUCH_DEVICE, O_RDONLY);
    if (fd == -1)
    {
        perror("无法打开触摸屏设备");
        return EXIT_FAILURE;
    }

    printf("触摸屏检测程序已启动，监听设备 %s\n", TOUCH_DEVICE);
    printf("按Ctrl+C退出程序\n\n");

    while (1)
    {
        // 读取输入事件
        if (read(fd, &ev, sizeof(struct input_event)) < 0)
        {
            perror("读取输入事件失败");
            close(fd);
            return EXIT_FAILURE;
        }

        // 处理触摸事件
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
            // 同步事件，通常表示一个完整的事件报告
            if (touch_pressed)
            {
                printf("触摸位置: X=%d, Y=%d\n", x, y);
            }
            else
            {
                printf("触摸释放\n");
            }
        }
    }

    close(fd);
    return EXIT_SUCCESS;
}