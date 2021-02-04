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

#ifndef __SSPM_IPI_MBOX_H__
#define __SSPM_IPI_MBOX_H__

#include <linux/ioport.h>
#include <linux/io.h>
#include <linux/semaphore.h>
#include <linux/completion.h>
#include <linux/mutex.h>

extern struct completion sema_ipi_task[];

/* pre-define slot and size here */
/* the array index is mapped to bit position of INT_IRQ or OUT_IRQ */
/* the slot is where the data begin */
/* the size is how many registers are needed */
struct _pin_send {

	struct mutex mutex_send;
	struct completion comp_ack;

	unsigned int mbox:3,	/* mailbox number used by the pin */
		 slot:6,	/* bit position of INT_IRQ or OUT_IRQ */
		 size:6,	/* register number used to pass argument */
		 async:1,	/* pin can use async functions */
		 retdata:1,	/* return data or not */
		 lock:3,	/* Linux lock method: 0: mutex; 1: busy wait */
		 polling:1,	/* Linux ack polling method */
		 unused:11;
	uint32_t *prdata;
};

struct _pin_recv {
	struct ipi_action *act;
	unsigned int mbox:3,	/* mailbox number used by the pin */
		 slot:6,	/* slot offset of the pin  */
		 size:6,	/* register number used to pass argument */
		 shared:2,	/* shared slot */
		 retdata:2,	/* return data or not */
		 lock:2,	/* Linux lock method: 0: mutex; 1: busy wait */
		 share_grp:5,	/* shared group */
		 unused:6;
};

struct _mbox_info {
	unsigned int start:8,	/* start index of pin table */
		 end:8,		/* end index of pin table */
		 used_slot:8,	/* used slots in the mailbox */
		 mode:2,	/* 0:disable, 1:for received, 2: for send */
		 unused:6;
};

#define IPI_SLOT_SHARED 1
#define IPI_SLOT_NONE	0

#define IPI_DATA_RETURN 1
#define IPI_DATA_NONE	0

#define IPI_LOCK_ORIGNAL	0x1
#define IPI_LOCK_NEW		0x2
#define IPI_LOCK_CHANGE		0x4

__weak void sspm_ipi_timeout_cb(int ipi_id) {}

#endif /* __SSPM_IPI_MBOX_H__ */
