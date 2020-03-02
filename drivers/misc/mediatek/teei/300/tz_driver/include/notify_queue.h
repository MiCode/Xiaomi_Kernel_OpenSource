/*
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef NOTIFY_QUEUE_H
#define NOTIFY_QUEUE_H

#include <linux/types.h>

struct NQ_head {
	u32 start_index;
	u32 end_index;
	u32 Max_count;
	unsigned char reserve[20];
};

struct NQ_entry {
	u32 valid_flag;
	u32 length;
	u64 buffer_addr;
	u32 cmd;
	unsigned char reserve[12];
};

#pragma pack(1)
struct create_NQ_struct {
	u64 n_t_nq_phy_addr;
	u32 n_t_size;
	u64 t_n_nq_phy_addr;
	u32 t_n_size;
};
#pragma pack()

extern unsigned long t_nt_buffer;

int add_nq_entry(u32 cmd, unsigned long command_buff,
				int command_length, int valid_flag);

unsigned char *get_nq_entry(unsigned char *buffer_addr);
long create_nq_buffer(void);

#endif /* end of NOTIFY_QUEUE_H */
