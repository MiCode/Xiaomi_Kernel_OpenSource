/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __SSPM_IPI_H__
#define __SSPM_IPI_H__

#include <linux/ioport.h>
#include <linux/io.h>
#include <linux/semaphore.h>
#include <linux/completion.h>

#include "sspm_ipi_pin.h"

struct ipi_action {
	void *data;
	unsigned int id;
	spinlock_t *lock;
};


#define IPI_REG_OK 0
#define IPI_REG_ALREADY -1
#define IPI_REG_ACTION_ERROR -2
#define IPI_REG_SEMAPHORE_FAIL -3

#define IPI_SERVICE_NOT_AVAILABLE -100
#define IPI_SERVICE_NOT_INITED -101

/* ipi_send() return code */
#define IPI_DONE             0
#define IPI_RETURN           1
#define IPI_BUSY            -1
#define IPI_TIMEOUT_AVL     -2
#define IPI_TIMEOUT_ACK     -3
#define IPI_MODULE_ID_ERROR -4
#define IPI_HW_ERROR        -5
#define IPI_NO_MEMORY       -6
#define IPI_USED_IN_WAIT    -7
#define IPI_PIN_MISUES      -8

extern int sspm_ipi_init(void);

/* definition for opts arguments (new) */
#define IPI_OPT_WAIT          0
#define IPI_OPT_POLLING       1

#define sspm_ipi_send_sync_new  sspm_ipi_send_sync
extern int sspm_ipi_recv_registration(int mid, struct ipi_action *act);
extern int sspm_ipi_recv_registration_ex(int mid, spinlock_t *lock,
	struct ipi_action *act);
extern int sspm_ipi_recv_unregistration(int mid);
extern int sspm_ipi_recv_wait(int mid);
extern void sspm_ipi_recv_complete(int mid);
extern int sspm_ipi_send_sync(int mid, int opts, void *buffer, int slot,
	void *retbuf, int retslot);
extern int sspm_ipi_send_ack(int mid, unsigned int *data);
extern int sspm_ipi_send_ack_ex(int mid, void *data, int retslot);

/* old definition (obslete) */
#define IPI_OPT_DEFAUT          0
#define IPI_OPT_NOLOCK          (IPI_OPT_REDEF_MASK)
#define IPI_OPT_LOCK_BUSY       (IPI_OPT_REDEF_MASK|IPI_OPT_LOCK_MASK)
#define IPI_OPT_LOCK_POLLING    (IPI_OPT_REDEF_MASK|IPI_OPT_LOCK_MASK| \
				 IPI_OPT_POLLING_MASK)
extern int sspm_ipi_is_inited(void);
extern int sspm_ipi_send_async(int mid, int opts, void *buffer, int slot);
extern int sspm_ipi_send_async_wait(int mid, int opts, void *retbuf);
extern int sspm_ipi_send_async_wait_ex(int mid, int opts, void *retbuf,
	int retslot);

#endif /* __SSPM_IPI_H__ */
