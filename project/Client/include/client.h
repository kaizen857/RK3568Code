#ifndef _CLIENT_H_
#define _CLIENT_H_

#define PCM_DEVICE "default"
#define FORMAT SND_PCM_FORMAT_S16_LE
#define CHANNELS 2
#define SAMPLE_RATE 44100
#define BITS_PER_SAMPLE 16
#define WAV_HEADER_SIZE 44
#define BUFFER_SIZE 4096

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

int client(int argc, char *argv[]);
int album(int argc, char *argv[]);
int video(int argc, char *argv[]);

#endif