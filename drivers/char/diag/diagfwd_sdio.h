/* Copyright (c) 2019, The Linux Foundation. All rights reserved.
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

#ifndef DIAGFWD_SDIO_H
#define DIAGFWD_SDIO_H

#ifdef CONFIG_QTI_SDIO_CLIENT

#ifdef CONFIG_DIAG_OVER_USB
#include <linux/usb/usbdiag.h>
#endif
#include <linux/usb/diag_bridge.h>

#define SDIO_1			0
#define NUM_SDIO_DEV		1

#define DIAG_SDIO_NAME_SZ	24

struct diag_sdio_info {
	int id;
	int dev_id;
	int ch_num;
	int mempool;
	uint8_t opened;
	uint8_t enabled;
	uint8_t suspended;
	char name[DIAG_SDIO_NAME_SZ];
	struct work_struct read_work;
	struct work_struct open_work;
	struct work_struct close_work;
	struct workqueue_struct *sdio_wq;
	spinlock_t lock;
};

extern struct diag_sdio_info diag_sdio[NUM_SDIO_DEV];
extern int qti_client_open(int id, struct diag_bridge_ops *ops);
extern int qti_client_close(int id);
extern int qti_client_read(int id, char *buf, size_t count);
extern int qti_client_write(int id, char *buf, size_t count);


int diag_sdio_init(void);
void diag_sdio_exit(void);

#endif
#endif /*CONFIG_QTI_SDIO_CLIENT*/
