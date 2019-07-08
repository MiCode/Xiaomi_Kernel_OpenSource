/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _TRUSTY_LOG_H_
#define _TRUSTY_LOG_H_

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

#define TRUSTY_LOG_API_VERSION	1

#endif

