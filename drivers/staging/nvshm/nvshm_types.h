/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _NVSHM_TYPES_H
#define _NVSHM_TYPES_H

#include <linux/workqueue.h>
#include <linux/platform_data/nvshm.h> /* NVSHM_SERIAL_BYTE_SIZE */

/* NVSHM common types */

/* Shared memory fixed offsets */

#define NVSHM_IPC_BASE (0x0)              /* IPC mailbox base offset */
#define NVSHM_IPC_MAILBOX (0x0)           /* IPC mailbox offset */
#define NVSHM_IPC_RETCODE (0x4)           /* IPC mailbox return code offset */
#define NVSHM_IPC_SIZE (4096)             /* IPC mailbox region size */
#define NVSHM_CONFIG_OFFSET (8)           /* shared memory config offset */
#define NVSHM_MAX_CHANNELS (12)           /* Maximum number of channels */
#define NVSHM_CHAN_NAME_SIZE (27)         /* max channel name size in chars */

/* Versions: */
#define NVSHM_MAJOR(v) (v >> 16)
#define NVSHM_MINOR(v) (v & 0xFFFF)
/** Version 1.3 is supported. Keep until modem image has reached v2.x in main */
#define NVSHM_CONFIG_VERSION_1_3 (0x00010003)
/** Version with statistics export support, otherwise compatible with v1.3 */
#define NVSHM_CONFIG_VERSION (0x00020001)


#define NVSHM_AP_POOL_ID (128) /* IOPOOL ID - use 128-255 for AP */

#define NVSHM_RATE_LIMIT_TTY (128)
#define NVSHM_RATE_LIMIT_LOG (256)
#define NVSHM_RATE_LIMIT_NET (512)
#define NVSHM_RATE_LIMIT_RPC (128)
#define NVSHM_RATE_LIMIT_TRESHOLD (16)

/* NVSHM_IPC mailbox messages ids */
enum nvshm_ipc_mailbox {
	/* Boot status */
	NVSHM_IPC_BOOT_COLD_BOOT_IND = 0x01,
	NVSHM_IPC_BOOT_FW_REQ,
	NVSHM_IPC_BOOT_RESTART_FW_REQ,
	NVSHM_IPC_BOOT_FW_CONF,
	NVSHM_IPC_READY,

	/* Boot errors */
	NVSHM_IPC_BOOT_ERROR_BT2_HDR = 0x1000,
	NVSHM_IPC_BOOT_ERROR_BT2_SIGN,
	NVSHM_IPC_BOOT_ERROR_HWID,
	NVSHM_IPC_BOOT_ERROR_APP_HDR,
	NVSHM_IPC_BOOT_ERROR_APP_SIGN,
	NVSHM_IPC_BOOT_ERROR_UNLOCK_HEADER,
	NVSHM_IPC_BOOT_ERROR_UNLOCK_SIGN,
	NVSHM_IPC_BOOT_ERROR_UNLOCK_PCID,

	NVSHM_IPC_MAX_MSG = 0xFFFF
};

/* NVSHM Config */

/* Channel type */
enum nvshm_chan_type {
	NVSHM_CHAN_UNMAP = 0,
	NVSHM_CHAN_TTY,
	NVSHM_CHAN_LOG,
	NVSHM_CHAN_NET,
	NVSHM_CHAN_RPC,
};

/* Channel mapping structure */
struct nvshm_chan_map {
	/* tty/net/log */
	int type;
	/* Name of device - reflected in sysfs */
	char name[NVSHM_CHAN_NAME_SIZE+1];
};

/*
 * This structure is set by BB after boot to give AP its current shmem mapping
 * BB initialize all descriptor content and give initial empty element
 * for each queue
 * BB enqueue free AP descriptor element into AP queue
 * AP initialize its queues pointer with empty descriptors offset
 * and retreive its decriptors
 * from ap queue.
 */
struct nvshm_config {
	int version;
	int shmem_size;
	int region_ap_desc_offset;
	int region_ap_desc_size;
	int region_bb_desc_offset;
	int region_bb_desc_size;
	int region_ap_data_offset;
	int region_ap_data_size;
	int region_bb_data_offset;
	int region_bb_data_size;
	int queue_ap_offset;
	int queue_bb_offset;
	struct nvshm_chan_map chan_map[NVSHM_MAX_CHANNELS];
	char serial[NVSHM_SERIAL_BYTE_SIZE];
	int region_dxp1_stats_offset;
	int region_dxp1_stats_size;
	int guard;
};

/*
 * This structure holds data fragments reference
 * WARNING: ALL POINTERS ARE IN BASEBAND MAPPING
 * NO POINTER SHOULD BE USED WITHOUT PROPER MACRO
 * see nvshm_iobuf.h for reference
 */
struct nvshm_iobuf {
	/* Standard iobuf part - This part is fixed and cannot be changed */
	unsigned char   *npdu_data;
	unsigned short   length;
	unsigned short   data_offset;
	unsigned short   total_length;
	unsigned char ref;
	unsigned char pool_id;
	struct nvshm_iobuf *next;
	struct nvshm_iobuf *sg_next;
	unsigned short flags;
	unsigned short _size;
	void *_handle;
	unsigned int _reserved;

	/* Extended iobuf - This part is not yet fixed (under spec/review) */
	struct nvshm_iobuf *qnext;
	int chan;
	int qflags;
	int _reserved1;
	int _reserved2;
	int _reserved3;
	int _reserved4;
	int _reserved5;
};

/* channel structure */
struct nvshm_channel {
	int index;
	struct nvshm_chan_map map;
	struct nvshm_if_operations *ops;
	void *data;
	int rate_counter;
	int xoff;
	struct work_struct start_tx_work;
};


#endif /* _NVSHM_TYPES_H */
