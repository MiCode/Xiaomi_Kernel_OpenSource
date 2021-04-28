# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2020 MediaTek Inc.

# CROSS_COMPILE = aarch64-linux-gnu-
CFLAGS = -DFS_UT -Werror -Wall -Wframe-larger-than=512 --static
LDFLAGS = --static

INCS = -I ../ \

LIBS = -lpthread

$(info INCS = $(INCS))

SRCS = ut_fs_test.c \
	   ../frame_sync.c \
	   ../frame_sync_algo.c \
	   ../frame_monitor.c \

#OBJS = ut_fs_test.o \
#	   frame_sync.o \
#	   frame_sync_algo.o \
#	   frame_monitor.o \

TARGET = ut_fs_test

all: $(OPTS) $(TARGET)

ut_fs_test: $(SRCS)
	gcc $(LDFLAGS) $(CFLAGS) $(INCS) $^ -o $@ $(LIBS)

# ut_fs_test: $(OBJS)
#	gcc $(LDFLAGS) $(INCS) -o $(LIBS) $@ $^

%.o: %.c
	gcc $(CFLAGS) $(INCS) -c $<

%.o: ../%.c
	gcc $(CFLAGS) $(INCS) $(LIBS) -c $<

clean:
	rm -f *.o $(TARGET)
