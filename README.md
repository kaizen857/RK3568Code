# RK3568 开发板课程实训实验集

大二下学期课程实训期间在 RK3568 开发板（Linux）上进行的嵌入式实验代码集合。

## 实验内容

| 文件 | 说明 |
|---|---|
| `Test.c` / `test1.c` / `test2.c` / `touchdetet.c` / `videoTest.c` / `image_videoTest.c` | 基础外设测试（GPIO / 触摸 / 图像视频采集） |
| `audioRecordTest.c` | 音频录制测试 |
| `libjpegtest.c` | libjpeg 解码测试 |
| `info.c` | 系统信息读取 |
| `deepseek.c` / `deepseek.py` | DeepSeek API 调用实验 |
| `client.c` / `server.c` | 简易客户端/服务端通信实验 |
| `project/Client` | 客户端项目：相册与视频功能（`album.c` / `video.c` / `client.c`，`build.sh` 构建） |
| `project/Server` | 服务端项目：语音交互服务（`server.c` / `deepseek.py`，`Makefile` / `32bit_make.sh` / `64bit_make.sh` 构建） |

## 构建与运行

各实验文件为独立 C 程序，直接编译即可，例如：

```bash
gcc -o test Test.c -lm
```

`project/Client` 与 `project/Server` 各有独立构建脚本（`build.sh` / `Makefile`）。

## 说明

- 本仓库为课程实训期间的实验记录，代码为实验性质、结构较松散。
- 实验均在 RK3568 开发板（Linux）上实际运行验证。
- `project/Server` 原依赖讯飞语音 SDK 的闭源库（`libmsc.so` 及配套头文件），出于许可与体积考虑已从仓库移除；如需编译相关代码请自行获取 SDK 并恢复 `project/Server/libs/` 与 `project/Server/include/`。
