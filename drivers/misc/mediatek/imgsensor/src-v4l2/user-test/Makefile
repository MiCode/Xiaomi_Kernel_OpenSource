# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2020 MediaTek Inc.

CROSS_COMPILE = aarch64-linux-gnu-
CFLAGS = -Werror -Wall -I../inc --static
LDFLAGS = --static

OBJS = user-test.o
TARGET = imgsensor-user-test

all: $(OPTS) $(TARGET)

imgsensor-user-test: $(OBJS)
	$(CROSS_COMPILE)gcc $(LDFLAGS) $(LIBS) -o $@ $^

%.o: %.c
	$(CROSS_COMPILE)gcc $(CFLAGS) $(INCS) -c $<

clean:
	rm -f *.o $(TARGET)
