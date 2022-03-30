/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#ifndef _GZ_LOG_H_
#define _GZ_LOG_H_

/*
 * Ring buffer that supports one secure producer thread and one
 * linux side consumer thread.
 */
struct log_rb {
	uint32_t alloc;
	uint32_t put;
	uint32_t sz;
	char data[0];
} __packed;

#define TRUSTY_LOG_API_VERSION		(1)

/* get_gz_log_buffer is deprecated after GKI */
void get_gz_log_buffer(unsigned long *addr, unsigned long *paddr,
		       unsigned long *size, unsigned long *start);
#endif
