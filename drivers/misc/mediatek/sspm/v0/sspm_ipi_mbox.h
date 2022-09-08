/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __SSPM_IPI_MBOX_H__
#define __SSPM_IPI_MBOX_H__

#include <linux/ioport.h>
#include <linux/io.h>
#include <linux/semaphore.h>
#include <linux/completion.h>
#include <linux/mutex.h>

#define IPI_MBOX_TOTAL  4

#define IPI_MBOX0_64D   0
#define IPI_MBOX1_64D   0
#define IPI_MBOX2_64D   0
#define IPI_MBOX3_64D   0
#define IPI_MBOX4_64D   0
#define IPI_MBOX_MODE   ((IPI_MBOX4_64D<<4)|(IPI_MBOX3_64D<<3)| \
			(IPI_MBOX2_64D<<2)|(IPI_MBOX1_64D<<1)| \
			IPI_MBOX0_64D)

#define IPI_MBOX0_SLOTS ((IPI_MBOX0_64D+1)*32)
#define IPI_MBOX1_SLOTS ((IPI_MBOX1_64D+1)*32)
#define IPI_MBOX2_SLOTS ((IPI_MBOX2_64D+1)*32)
#define IPI_MBOX3_SLOTS ((IPI_MBOX3_64D+1)*32)
#define IPI_MBOX4_SLOTS ((IPI_MBOX4_64D+1)*32)

//#define TOTAL_SEND_PIN    12
//#define TOTAL_RECV_PIN     4

//extern struct _mbox_info *mbox_table;
//extern struct _pin_send  *send_pintable;
//extern struct _pin_recv  *recv_pintable;
//extern char *(*pin_name);

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

#define IPI_LOCK_ORIGINAL	0x1
#define IPI_LOCK_NEW		0x2
#define IPI_LOCK_CHANGE		0x4

__weak void sspm_ipi_timeout_cb(int ipi_id) {}

//extern struct _mbox_info *mbox_table;
//extern struct _pin_send *send_pintable;
//extern struct _pin_recv *recv_pintable;
//extern char *(*pin_name);

#endif /* __SSPM_IPI_MBOX_H__ */
