/*
 * Copyright (c) 2015, 2017, The Linux Foundation. All rights reserved.
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
#ifndef __SEEMP_LOGK_STUB__
#define __SEEMP_LOGK_STUB__

#ifdef CONFIG_SEEMP_CORE
#include <linux/kernel.h>
#include <linux/seemp_api.h>
#include <linux/socket.h>

#define MAX_BUF_SIZE 188

#define SEEMP_LOGK_API_SIZE sizeof(int)

/* Write: api_id + skip encoding byte + params */
#define SEEMP_LOGK_RECORD(api_id, format, ...) do {            \
	*((int *)(buf - SEEMP_LOGK_API_SIZE)) = api_id;             \
	snprintf(buf + 1, MAX_BUF_SIZE - 1, format, ##__VA_ARGS__); \
} while (0)

extern void *(*seemp_logk_kernel_begin)(char **buf);
extern void (*seemp_logk_kernel_end)(void *blck);

static inline void *seemp_setup_buf(char **buf)
{
	void *blck;

	if (seemp_logk_kernel_begin && seemp_logk_kernel_end) {
		blck = seemp_logk_kernel_begin(buf);
		if (!*buf) {
			seemp_logk_kernel_end(blck);
			return NULL;
		}
	} else {
		return NULL;
	}
	return blck;
}
/*
 * NOTE: only sendto is going to be instrumented
 * since send sys call internally calls sendto
 * with 2 extra parameters
 */
static inline void seemp_logk_sendto(int fd, void __user *buff, size_t len,
		unsigned int flags, struct sockaddr __user *addr, int addr_len)
{
	char *buf = NULL;
	void *blck = NULL;

	/*sets up buf and blck correctly*/
	blck = seemp_setup_buf(&buf);
	if (!blck)
		return;

	/*fill the buf*/
	SEEMP_LOGK_RECORD(SEEMP_API_kernel__sendto, "len=%u,fd=%d",
			(unsigned int)len, fd);

	seemp_logk_kernel_end(blck);
}

static inline void seemp_logk_rtic(__u8 type, pid_t pid, __u8 asset_id[0x20],
		__u8 asset_category, __u8 response)
{
	char *buf = NULL;
	void *blck = NULL;

	blck = seemp_setup_buf(&buf);
	if (!blck)
		return;

	SEEMP_LOGK_RECORD(SEEMP_API_kernel__rtic,
		"app_pid=%d,rtic_type=%u,asset_id=%s,asset_category=%u,response=%u",
		pid, type, asset_id, asset_category, response);

	seemp_logk_kernel_end(blck);
}
#else
static inline void seemp_logk_sendto(int fd, void __user *buff,
		size_t len, unsigned int flags, struct sockaddr __user *addr,
		int addr_len)
{
}

static inline void seemp_logk_rtic(__u8 type, __u64 actor, __u8 asset_id[0x20],
		__u8 asset_category, __u8 response)
{
}
#endif
#endif
