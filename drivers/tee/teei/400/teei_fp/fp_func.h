/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
 * All Rights Reserved.
 *
 */

#ifndef TEEI_FUNC_H
#define TEEI_FUNC_H

#include <teei_ioc.h>
#define MICROTRUST_FP_SIZE	0x80000
#define FP_MAJOR		254
#define DEV_NAME		"teei_fp"

extern struct semaphore fp_api_lock;
extern wait_queue_head_t __fp_open_wq;
extern wait_queue_head_t __wait_spi_wq;
extern unsigned long teei_config_flag;

int send_fp_command(void *buffer, unsigned long size);
#endif /* end of TEEI_FUNC_H */
