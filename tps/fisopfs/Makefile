CC = gcc
CFLAGS := -ggdb3 -O2 -Wall -std=c11
CFLAGS += -Wno-unused-function -Wvla
FUSEFLAG := -D_FILE_OFFSET_BITS=64 -I/usr/include/fuse -lfuse -pthread

# Flags for FUSE
LDLIBS := $(shell pkg-config fuse --cflags --libs)

# Name for the filesystem!
FS_NAME := fisopfs
SRCS :=   $(wildcard *.c) #Todos los .c de la carpeta


all: build
	
build: #$(FS_NAME)
	$(CC) $(CFLAGS) $(SRCS) $(FUSEFLAG) -o $(FS_NAME) 

format: .clang-files .clang-format
	xargs -r clang-format -i <$<

clean:
	rm -rf $(EXEC) *.o core vgcore.* $(FS_NAME)

.PHONY: all build clean format
