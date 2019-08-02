/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (c) 2015-2019 TrustKernel Incorporated
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
