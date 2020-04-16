/*
 * Copyright (c) 2015-2018 TrustKernel Incorporated
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

#ifndef __TEE_PROCFS_H__
#define __TEE_PROCFS_H__

#define TEE_LOG_IRQ	280

#define TEE_LOG_CTL_BUF_SIZE	256

#define TEE_LOG_SIGNAL_THRESHOLD_SIZE 1024

#define TEE_CRASH_MAGIC_NO	0xdeadbeef

struct tee;

int tee_init_procfs(struct tee *tee);
void tee_exit_procfs(void);

#endif
