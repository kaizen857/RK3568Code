#include "../include/client.h"
#include <alsa/asoundlib.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <jpeglib.h>
#include <netinet/in.h>
#include <linux/input.h>
#include <stdio.h>
#include <string.h>
#include <stdatomic.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>

#define TOUCH_DEVICE "/dev/input/event6"

atomic_int socketLock = 0; // 0:未被使用  1:正在被使用
atomic_int isexit = 0;
atomic_int isRecording = 0;
atomic_int isRecordStop = 0;

int true = 1;
int false = 0;

void shutown(int socket_fd);
void *recording(); // 录音

/**
 * @brief 发送数据
 * @param socket_fd socket文件描述符
 * @param buf        发送的数据
 * @param bufSize    发送的数据长度
 * @return           返回发送的字节数
 */
int sendMessage(int socket_fd, char *buf, int bufSize)
{
    while (!atomic_compare_exchange_strong(&socketLock, &true, TRUE))
        ;                                     // 等待socketLock为0
    int ret = write(socket_fd, buf, bufSize); // 发送数据

    while (!atomic_compare_exchange_strong(&socketLock, &false, FALSE))
        ;
    return ret;
}
char buf[8192] = {0};
void *terminalSovle(void *arg)
{
    // TODO:终端输入
    int socket_fd = *(int *)arg;
    while (1)
    {
        printf("请输入指令：\n");

        fgets(buf, sizeof(buf), stdin);
        buf[strlen(buf) - 1] = '\0';
        // printf("buf = %s\n", buf);
        memmove(buf + 1, buf, strlen(buf));
        buf[0] = '1';
        if (strcmp(buf, "1exit") == 0)
        {
            sendMessage(socket_fd, buf, strlen(buf));
            printf("退出程序\n");
            atomic_store(&isexit, 1);
            return NULL;
        }
        else
        {
            // 向主机发送字符串
            sendMessage(socket_fd, buf, strlen(buf));
            // 接收主机返回的字符串
            memset(buf, 0, sizeof(buf));
            int ret = read(socket_fd, buf, sizeof(buf));
            if (ret == -1)
            {
                perror("read error");
                return NULL;
            }
            printf("主机返回：%s\n", buf);
        }
    }
}

void *voiceSovle(void *arg)
{
    // TODO:语音输入
    int socket_fd = *(int *)arg;
    int fd;
    struct input_event ev;
    int x = 0, y = 0;
    int touch_pressed = 0;

    // 打开触摸屏设备
    fd = open(TOUCH_DEVICE, O_RDONLY);
    if (fd == -1)
    {
        perror("无法打开触摸屏设备");
        atomic_store(&isexit, 1);
        return NULL;
    }
    while (1)
    {
        // TODO:判断屏幕是否按下
        if (read(fd, &ev, sizeof(struct input_event)) < 0)
        {
            perror("读取输入事件失败");
            close(fd);
            atomic_store(&isexit, 1);
            return NULL;
        }
        if (ev.type == EV_ABS)
        {
            switch (ev.code)
            {
            case ABS_PRESSURE: // 有些触摸屏用这个表示触摸压力
                touch_pressed = (ev.value > 0);
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
                // TODO:判断是否开始录制
                if (atomic_load(&isRecording))
                {
                    continue;
                }
                else
                {
                    // TODO:开始录制(新开个线程)
                    pthread_t tid;
                    pthread_create(&tid, NULL, recording, NULL);
                    pthread_detach(tid);
                }
            }
            else
            {
                printf("结束录制");
                // TODO:结束录制
                if (atomic_load(&isRecording))
                {
                    atomic_store(&isRecording, 0);
                    printf("结束录音\n");
                    while (1)
                    {
                        if (atomic_load(&isRecordStop))
                        {
                            break;
                        }
                        else
                        {
                            usleep(50000);
                        }
                    }
                    atomic_store(&isRecordStop, 0);
                    // TODO:发送录音数据，录音数据存储在output.wav中
                    // 1. 读取WAV文件
                    FILE *file = fopen("output.wav", "rb");
                    if (!file)
                    {
                        perror("Failed to open WAV file");
                        atomic_store(&isexit, 1);
                        return NULL;
                    }

                    // 获取文件大小
                    fseek(file, 0, SEEK_END);
                    long file_size = ftell(file);
                    fseek(file, 0, SEEK_SET);

                    // 分配内存存储文件内容
                    uint8_t *wav_data = (uint8_t *)malloc(file_size);
                    if (!wav_data)
                    {
                        perror("Memory allocation failed");
                        fclose(file);
                        atomic_store(&isexit, 1);
                        return NULL;
                    }

                    // 读取文件内容
                    size_t bytes_read = fread(wav_data, 1, file_size, file);
                    if (bytes_read != file_size)
                    {
                        perror("Failed to read WAV file");
                        free(wav_data);
                        fclose(file);
                        atomic_store(&isexit, 1);
                        return NULL;
                    }
                    fclose(file);

                    // 3. 发送文件大小信息（先发送文件大小，便于服务端准备接收）
                    uint32_t file_size_net = htonl(file_size);
                    uint8_t sendMessage[36];
                    printf("file_size_net: %u\n", file_size_net);
                    memset(sendMessage, 0, sizeof(sendMessage));
                    sendMessage[0] = '2';
                    memcpy(sendMessage + 1, &file_size_net, sizeof(file_size_net));
                    sendMessage[34] = '\0';
                    if (write(socket_fd, sendMessage, 36) != sizeof(sendMessage))
                    {
                        perror("Failed to send file size");
                        free(wav_data);
                        atomic_store(&isexit, 1);
                        return NULL;
                    }
                    // usleep(50000);
                    //  4. 发送文件内容
                    size_t total_sent = 0;
                    int retry_count = 0;
                    const int max_retries = 3;     // 最大重试次数
                    const int timeout_us = 500000; // 超时时间500ms

                    while (total_sent < file_size)
                    {
                        ssize_t sent = write(socket_fd, wav_data + total_sent,
                                             (file_size - total_sent) > BUFFER_SIZE ? BUFFER_SIZE : (file_size - total_sent));

                        if (sent < 0)
                        {
                            perror("Failed to send data");
                            if (errno == EAGAIN || errno == EWOULDBLOCK)
                            {
                                if (retry_count++ < max_retries)
                                {
                                    usleep(timeout_us);
                                    continue;
                                }
                            }
                            free(wav_data);
                            atomic_store(&isexit, 1);
                            return NULL;
                        }

                        // 如果是最后一个包，需要确认接收
                        if (total_sent + sent >= file_size)
                        {
                            // 等待服务端确认
                            char ack;
                            fd_set read_fds;
                            struct timeval tv;
                            int ret;

                            for (retry_count = 0; retry_count < max_retries; retry_count++)
                            {
                                FD_ZERO(&read_fds);
                                FD_SET(socket_fd, &read_fds);
                                tv.tv_sec = 0;
                                tv.tv_usec = timeout_us;

                                ret = select(socket_fd + 1, &read_fds, NULL, NULL, &tv);
                                if (ret > 0 && FD_ISSET(socket_fd, &read_fds))
                                {
                                    if (read(socket_fd, &ack, 1) == 1 && ack == 'A')
                                    {
                                        break; // 收到确认
                                    }
                                }

                                // 超时或错误，重发最后一个包
                                // tmd要是再丢包就直接开摆了
                                sent = write(socket_fd, wav_data + total_sent,
                                             (file_size - total_sent) > BUFFER_SIZE ? BUFFER_SIZE : (file_size - total_sent));
                                if (sent < 0)
                                {
                                    perror("Failed to resend last packet");
                                    break;
                                }
                            }

                            if (retry_count >= max_retries)
                            {
                                fprintf(stderr, "Failed to get ACK for last packet after %d retries\n", max_retries);
                                free(wav_data);
                                atomic_store(&isexit, 1);
                                return NULL;
                            }
                        }

                        total_sent += sent;
                        retry_count = 0; // 重置重试计数
                    }
                    printf("Successfully sent %ld bytes of WAV data to\n", file_size);
                }
            }
        }
    }
    return NULL;
}

void write_wav_header(FILE *file, int channels, int sample_rate, int bits_per_sample, int data_size)
{
    wav_header_t header;

    // 填写 WAV 文件头
    memcpy(header.riff, "RIFF", 4);
    header.overall_size = data_size + WAV_HEADER_SIZE - 8;
    memcpy(header.wave, "WAVE", 4);
    memcpy(header.fmt_chunk_marker, "fmt ", 4);
    header.length_of_fmt = 16;
    header.format_type = 1; // PCM
    header.channels = channels;
    header.sample_rate = sample_rate;
    header.byterate = sample_rate * channels * bits_per_sample / 8;
    header.block_align = channels * bits_per_sample / 8;
    header.bits_per_sample = bits_per_sample;
    memcpy(header.data_chunk_header, "data", 4);
    header.data_size = data_size;

    fwrite(&header, 1, sizeof(wav_header_t), file);
}

void *recording()
{
    // TODO:音频录制
    if (!atomic_load(&isRecording))
    {
        atomic_store(&isRecording, 1);
        atomic_store(&isRecordStop, 0);
        printf("开始录音\n");
        unsigned int sample_rate = SAMPLE_RATE;
        int channels = CHANNELS;
        snd_pcm_uframes_t frames = 32; // 每次读取32帧

        // 打开 ALSA PCM 设备
        snd_pcm_t *pcm_handle;
        snd_pcm_hw_params_t *params;
        snd_pcm_uframes_t frames_per_period;
        int pcm;

        pcm = snd_pcm_open(&pcm_handle, PCM_DEVICE, SND_PCM_STREAM_CAPTURE, 0);
        if (pcm < 0)
        {
            fprintf(stderr, "ERROR: Can't open \"%s\" PCM device. %s\n", PCM_DEVICE, snd_strerror(pcm));
            return NULL;
        }

        // 设置硬件参数
        snd_pcm_hw_params_malloc(&params);
        snd_pcm_hw_params_any(pcm_handle, params);
        snd_pcm_hw_params_set_access(pcm_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
        snd_pcm_hw_params_set_format(pcm_handle, params, FORMAT);
        snd_pcm_hw_params_set_channels(pcm_handle, params, channels);
        snd_pcm_hw_params_set_rate_near(pcm_handle, params, &sample_rate, 0);
        snd_pcm_hw_params_set_period_size_near(pcm_handle, params, &frames, 0);
        pcm = snd_pcm_hw_params(pcm_handle, params);
        if (pcm < 0)
        {
            fprintf(stderr, "ERROR: Can't set hardware parameters. %s\n", snd_strerror(pcm));
            return NULL;
        }

        snd_pcm_hw_params_get_period_size(params, &frames_per_period, 0);

        FILE *file = fopen("output.wav", "wb");
        if (!file)
        {
            fprintf(stderr, "ERROR: Can't open output file.\n");
            return NULL;
        }
        write_wav_header(file, channels, sample_rate, BITS_PER_SAMPLE, 0); // 先写入空的WAV头

        // 分配缓冲区
        int buffer_size = frames_per_period * channels * BITS_PER_SAMPLE / 8;
        char *buffer = (char *)malloc(buffer_size);
        long long total_bytes = 0;
        while (atomic_load(&isRecording) && (total_bytes < SAMPLE_RATE * 60 * channels * BITS_PER_SAMPLE / 8))
        {
            pcm = snd_pcm_readi(pcm_handle, buffer, frames_per_period);
            if (pcm == -EPIPE)
            {
                fprintf(stderr, "XRUN.\n");
                snd_pcm_prepare(pcm_handle);
            }
            else if (pcm < 0)
            {
                fprintf(stderr, "ERROR: Can't read from PCM device. %s\n", snd_strerror(pcm));
            }
            else
            {
                fwrite(buffer, 1, buffer_size, file);
                total_bytes += buffer_size;
            }
        }
        // 更新 WAV 头部文件大小信息
        fseek(file, 0, SEEK_SET);
        write_wav_header(file, channels, sample_rate, BITS_PER_SAMPLE, total_bytes);
        // 清理
        free(buffer);
        fclose(file);
        snd_pcm_drain(pcm_handle);
        snd_pcm_close(pcm_handle);
        atomic_store(&isRecordStop, 1);
    }
    else
    {
        return NULL;
    }
    return NULL;
}

// 向主机发送信息

int client(int argc, char *argv[])
{
    atomic_init(&socketLock, 0);
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0); // socket
    if (socket_fd == -1)
    {
        perror("socket error");
        return -1;
    }

    struct sockaddr_in server_addr; // server addr
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    server_addr.sin_addr.s_addr = inet_addr(argv[2]);

    int con_ret = connect(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    /*
    int connect(
    int sockfd,                     // socket函数返回值
    const struct sockaddr *addr,    // 结构体指针
    socklen_t addrlen);             // 结构体大小
    */
    if (con_ret == -1)
    {
        perror("connect error");
        return -1;
    }
    pthread_t terminal, sound;
    pthread_create(&terminal, NULL, terminalSovle, (void *)&socket_fd);
    pthread_create(&sound, NULL, voiceSovle, (void *)&socket_fd);
    pthread_detach(terminal);
    pthread_detach(sound);
    while (1)
    {
        if (atomic_load(&isexit))
        {
            shutown(socket_fd);
            return 0;
        }
        else
        {
            usleep(500000);
        }
    }
    return 0;
}

void shutown(int socket_fd)
{
    if (socket_fd != -1)
    {
        if (close(socket_fd) == -1)
        {
            perror("close error");
            return;
        }
    }
    else
    {
        perror("socket error");
    }
}