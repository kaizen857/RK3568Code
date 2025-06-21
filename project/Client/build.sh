# 1. 单独编译每个 .c 文件
aarch64-none-linux-gnu-gcc ./src/main.c -c -o main.o -I./include
aarch64-none-linux-gnu-gcc ./src/album.c -c -o album.o -I./include
aarch64-none-linux-gnu-gcc ./src/video.c -c -o video.o -I./include
aarch64-none-linux-gnu-gcc ./src/client.c -c -o client.o -I./include -lasound -lpthread

# 2. 链接所有 .o 文件
aarch64-none-linux-gnu-gcc \
    main.o album.o video.o client.o \
    -o main \
    -Wl,-Bstatic -ljpeg \
    -Wl,-Bdynamic -lasound -lpthread