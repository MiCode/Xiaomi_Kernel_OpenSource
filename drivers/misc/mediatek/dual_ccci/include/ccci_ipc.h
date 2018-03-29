/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/*****************************************************************************
 *
 *Filename:
 *---------
 *ccci_ipc.h
 *
 *
 *Author:
 *-------
 *Yalin wang (mtk80678)
 *
 ****************************************************************************/

#ifndef __CCCI_IPC_H__
#define __CCCI_IPC_H__
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/timer.h>
#include <linux/list.h>
#include <linux/sched.h>
#include <linux/mutex.h>
#include <linux/bitops.h>
#include <linux/atomic.h>
#include "ccci_tty.h"
#define IPC_MSGSVC_RVC_DONE 0x12344321
#define MAX_NUM_IPC_TASKS_MD    10
#define MAX_NUM_IPC_TASKS        10
#define CCCI_IPC_BUFFER_SIZE  (4<<10)
#define CCCI_IPC_DEV_MAJOR  183
#define CCCI_TASK_PENDING  0x01
#define CCCI_MD_IMAGE_MAPPING_SIZE (20<<20)
#define CCCI_IPC_MAGIC 'P'
#define CCCI_IPC_RESET_RECV    _IO(CCCI_IPC_MAGIC, 0)
#define CCCI_IPC_RESET_SEND    _IO(CCCI_IPC_MAGIC, 1)
#define CCCI_IPC_WAIT_MD_READY    _IO(CCCI_IPC_MAGIC, 2)

typedef unsigned int uint32;
typedef unsigned char uint8;
typedef unsigned short uint16;

typedef struct {
	uint8 ref_count;
	uint16 msg_len;
	uint8 data[0];
} local_para_struct;

typedef struct {
	uint16 pdu_len;
	uint8 ref_count;
	uint8 pb_resvered;
	uint16 free_header_space;
	uint16 free_tail_space;
	uint8 data[0];
} peer_buff_struct;

typedef struct ipc_ilm_struct {
	uint32 src_mod_id;
	uint32 dest_mod_id;
	uint32 sap_id;
	uint32 msg_id;
	local_para_struct *local_para_ptr;
	peer_buff_struct *peer_buff_ptr;
} ipc_ilm_t;

typedef struct {
	uint32 rx_offset;
	uint32 tx_offset;
	uint32 size;
	uint8 buffer[CCCI_IPC_BUFFER_SIZE];
} BUFF;

typedef struct {
	BUFF buff_wr;
	BUFF buff_rd;
} CCCI_IPC_BUFFER;

typedef struct {
	ipc_ilm_t ilm_md[MAX_NUM_IPC_TASKS_MD];	/*md side ilms */
	ipc_ilm_t ilm[MAX_NUM_IPC_TASKS];
	CCCI_IPC_BUFFER buffer;
} CCCI_IPC_MEM;

typedef struct {
	struct list_head list;
	void *data;
	uint32 len;
} CCCI_RECV_ITEM;

typedef struct {
	spinlock_t lock;
	unsigned long flag;
	atomic_t user;
	unsigned long w_jiffies;
	uint32 to_id;
	struct fasync_struct *fasync;
	ipc_ilm_t *ilm_p;
	uint32 time_out;
	uint32 ilm_phy_addr;
	wait_queue_head_t read_wait_queue;
	wait_queue_head_t write_wait_queue;
	struct list_head recv_list;
	void *owner;
} IPC_TASK;

typedef struct IPC_MSGSVC_TASKMAP_STRUCT {
	uint32 extq_id;		/*IPC universal mapping external queue */
	uint32 task_id;		/*IPC processor internal task id */

} IPC_MSGSVC_TASKMAP_T;

extern int __init ccci_ipc_init(int);
extern void __exit ccci_ipc_exit(int);

#define offset_of(type, mem)  ((uint32)(&(((type *)0)->mem)))
#define CCCI_IPC_SMEM_SIZE  (sizeof(CCCI_IPC_MEM))

#endif				/*__CCCI_IPC_H__ */
