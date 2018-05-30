/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __VIRTCLK_FRONT_H
#define __VIRTCLK_FRONT_H

enum virtclk_cmd {
	CLK_MSG_GETID = 1,
	CLK_MSG_ENABLE,
	CLK_MSG_DISABLE,
	CLK_MSG_RESET,
	CLK_MSG_SETFREQ,
	CLK_MSG_GETFREQ,
	CLK_MSG_MAX
};

#define CLOCK_FLAG_NODE_TYPE_REMOTE	0xff00

struct clk_msg_header {
	u32 cmd;
	u32 len;
	u32 clk_id;
} __packed;

struct clk_msg_rsp {
	struct clk_msg_header header;
	u32 rsp;
} __packed;

struct clk_msg_setfreq {
	struct clk_msg_header header;
	u32 freq;
} __packed;

struct clk_msg_reset {
	struct clk_msg_header header;
	u32 reset;
} __packed;

struct clk_msg_getid {
	struct clk_msg_header header;
	char name[32];
} __packed;

struct clk_msg_getfreq {
	struct clk_msg_rsp rsp;
	u32 freq;
} __packed;

struct virtclk_front_data {
	int handle;
	struct rt_mutex lock;
};

extern struct virtclk_front_data virtclk_front_ctx;
int virtclk_front_init_iface(void);

#endif
