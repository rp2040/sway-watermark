CC = gcc
CFLAGS = -Wall -O2 -I../algorithm -I../config
LIBS = -lm

# 启用OpenCV（需安装：sudo apt install libopencv-dev pkg-config）
USE_OPENCV = 1

ifeq ($(USE_OPENCV), 1)
    CFLAGS += -DUSE_OPENCV `pkg-config --cflags opencv4`
    LIBS += `pkg-config --libs opencv4`
endif

# 编译检测工具
detect: detect.c bch.c ../algorithm/rgb2yuv.c ../algorithm/embed.c
	$(CC) $(CFLAGS) -o detect detect.c bch.c ../algorithm/rgb2yuv.c ../algorithm/embed.c $(LIBS)

# 清理编译产物
clean:
	rm -f detect