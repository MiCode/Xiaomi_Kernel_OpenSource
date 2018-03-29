/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _MT_GPT_H_
#define _MT_GPT_H_

#include <linux/types.h>

#define GPT1            0x0
#define GPT2            0x1
#define GPT3            0x2
#define GPT4            0x3
#define GPT5            0x4
#define GPT6            0x5
#define NR_GPTS         0x6


#define GPT_ONE_SHOT    0x0
#define GPT_REPEAT      0x1
#define GPT_KEEP_GO     0x2
#define GPT_FREE_RUN    0x3


#define GPT_CLK_DIV_1   0x0000
#define GPT_CLK_DIV_2   0x0001
#define GPT_CLK_DIV_3   0x0002
#define GPT_CLK_DIV_4   0x0003
#define GPT_CLK_DIV_5   0x0004
#define GPT_CLK_DIV_6   0x0005
#define GPT_CLK_DIV_7   0x0006
#define GPT_CLK_DIV_8   0x0007
#define GPT_CLK_DIV_9   0x0008
#define GPT_CLK_DIV_10  0x0009
#define GPT_CLK_DIV_11  0x000a
#define GPT_CLK_DIV_12  0x000b
#define GPT_CLK_DIV_13  0x000c
#define GPT_CLK_DIV_16  0x000d
#define GPT_CLK_DIV_32  0x000e
#define GPT_CLK_DIV_64  0x000f


#define GPT_CLK_SRC_SYS 0x0
#define GPT_CLK_SRC_RTC 0x1


#define GPT_NOAUTOEN    0x0001
#define GPT_NOIRQEN     0x0002


extern int request_gpt(unsigned int id, unsigned int mode, unsigned int clksrc,
unsigned int clkdiv, unsigned int cmp,
void (*func)(unsigned long), unsigned int flags);

extern int free_gpt(unsigned int id);
extern int start_gpt(unsigned int id);
extern int stop_gpt(unsigned int id);
extern int restart_gpt(unsigned int id);
extern int gpt_is_counting(unsigned int id);
extern int gpt_set_cmp(unsigned int id, unsigned int val);
extern int gpt_get_cmp(unsigned int id, unsigned int *ptr);
extern int gpt_get_cnt(unsigned int id, unsigned int *ptr);
extern int gpt_check_irq(unsigned int id);
extern int gpt_check_and_ack_irq(unsigned int id);

#endif
