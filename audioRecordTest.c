#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <alsa/asoundlib.h>

#define PCM_DEVICE "default"
#define FORMAT SND_PCM_FORMAT_S16_LE
#define CHANNELS 2
#define SAMPLE_RATE 44100
#define BITS_PER_SAMPLE 16
#define WAV_HEADER_SIZE 44

// WAV 文件头
typedef struct
{
    char riff[4];                   // "RIFF"
    unsigned int overall_size;      // 文件大小 - 8
    char wave[4];                   // "WAVE"
    char fmt_chunk_marker[4];       // "fmt "
    unsigned int length_of_fmt;     // 格式数据块大小
    unsigned short format_type;     // 格式类别 (PCM = 1)
    unsigned short channels;        // 通道数
    unsigned int sample_rate;       // 采样率
    unsigned int byterate;          // 每秒字节数
    unsigned short block_align;     // 一个样本的字节数
    unsigned short bits_per_sample; // 每个样本的位数
    char data_chunk_header[4];      // "data"
    unsigned int data_size;         // 音频数据大小
} wav_header_t;

// 生成WAV文件头
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

// 主函数
int main()
{
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
        return -1;
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
        return -1;
    }

    snd_pcm_hw_params_get_period_size(params, &frames_per_period, 0);

    // 打开WAV文件并写入头部
    FILE *file = fopen("output.wav", "wb");
    if (!file)
    {
        fprintf(stderr, "ERROR: Can't open output file.\n");
        return -1;
    }
    write_wav_header(file, channels, sample_rate, BITS_PER_SAMPLE, 0); // 先写入空的WAV头

    // 分配缓冲区
    int buffer_size = frames_per_period * channels * BITS_PER_SAMPLE / 8;
    char *buffer = (char *)malloc(buffer_size);

    // 录音循环
    int total_bytes = 0;
    while (total_bytes < SAMPLE_RATE * 10 * channels * BITS_PER_SAMPLE / 8)
    { // 录音10秒
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

    return 0;
}
