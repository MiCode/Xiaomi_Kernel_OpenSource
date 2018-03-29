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

#if !defined(__AEE_IPANIC_VERSION_H__)
#define __AEE_IPANIC_VERSION_H__

#include <linux/version.h>
#include <linux/kmsg_dump.h>

struct ipanic_log_index {
	u32 idx;
	u64 seq;
};

extern u32 log_first_idx;
extern u64 log_first_seq;
extern u32 log_next_idx;
extern u64 log_next_seq;
int ipanic_kmsg_dump3(struct kmsg_dumper *dumper, char *buf, size_t len);

#endif
