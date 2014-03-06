/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __H_VPU_HFI_H__
#define __H_VPU_HFI_H__

#include "vpu_resources.h"

#define VPU_PIL_DEFAULT_TIMEOUT_MS	500

/*
 * vpu_hfi_packet_t packet definition
 * HFI layer only need to know how big the packet is
 * size	: size of the packet in bytes
 * data	: data of the packet
 */
struct vpu_hfi_packet {
	u32 size;
	u32 data[1];
};

/* local event */
enum vpu_hfi_event {
	VPU_LOCAL_EVENT_WD, /* watchdog bite */
	VPU_LOCAL_EVENT_BOOT, /* boot failure */
};

/* queue IDs */
enum vpu_hfi_queue_ids {
	/* system channel */
	VPU_SYSTEM_CMD_QUEUE_ID = 0,
	VPU_SYSTEM_MSG_QUEUE_ID,

	/* session channels */
	VPU_SESSION_CMD_QUEUE_0_ID,
	VPU_SESSION_MSG_QUEUE_0_ID,
	VPU_SESSION_CMD_QUEUE_1_ID,
	VPU_SESSION_MSG_QUEUE_1_ID,

	/* unused */
	VPU_QUEUE_INDEX_RESERVED_1,

	/* logging channel */
	VPU_SYSTEM_LOG_QUEUE_ID,

	VPU_QUEUE_INDEX_MAX
};

/* channel IDs */
#define VPU_NUM_SESSION_CHANNELS		2
enum vpu_hfi_channel_ids {
	VPU_SYSTEM_CHANNEL_ID = 0,
	VPU_SESSION_CHANNEL_ID_BASE,
	VPU_LOGGING_CHANNEL_ID =
			VPU_SESSION_CHANNEL_ID_BASE + VPU_NUM_SESSION_CHANNELS,

	VPU_CHANNEL_ID_MAX
};

typedef void (*hfi_handle_msg)(u32 cid, struct vpu_hfi_packet *pkt, void *priv);
typedef void (*hfi_handle_event)(u32 cid, enum vpu_hfi_event evt, void *priv);

/* init/deinit */
int vpu_hfi_init(struct vpu_platform_resources *res);
void vpu_hfi_deinit(void);

/* start/stop */
int vpu_hfi_start(hfi_handle_msg msg_handler, hfi_handle_event event_handler);
void vpu_hfi_stop(void);

/* enable/disable a communication pipe */
void vpu_hfi_enable(u32 cid, void *priv);
void vpu_hfi_disable(u32 cid);

/* send data via IPC, but not generate interrupt */
int vpu_hfi_write_packet(u32 cid, struct vpu_hfi_packet *packet);
int vpu_hfi_write_packet_extra(u32 cid, struct vpu_hfi_packet *packet,
				u8 *extra_data, u32 extra_size);

/* send data via IPC, and generate interrupt */
int vpu_hfi_write_packet_commit(u32 cid, struct vpu_hfi_packet *packet);
int vpu_hfi_write_packet_extra_commit(u32 cid, struct vpu_hfi_packet *packet,
						u8 *extra_data, u32 extra_size);

/**
 * vpu_hfi_dump_queue_headers() - dump the IPC queue table header contents
 * @idx:	index of the Tx / Rx queues to be dumped
 * @buf:	debug buffer to write into
 * @buf_size:	maximum size to read, in bytes
 *
 * Return:	the number of bytes read
 */
int vpu_hfi_dump_queue_headers(int idx, char *buf, size_t buf_size);

#ifdef CONFIG_DEBUG_FS

/**
 * vpu_hfi_set_pil_timeout() - set pil timeout
 * @pil_timeout:	the time to wait for pil
 */
void vpu_hfi_set_pil_timeout(u32 pil_timeout);

/**
 * vpu_hfi_print_queues() - print the content of the IPC queues
 * @buf:	debug buffer to write into
 * @buf_size:	maximum size to read, in bytes
 *
 * Return:	the number of bytes read
 */
size_t vpu_hfi_print_queues(char *buf, size_t buf_size);

/**
 * vpu_hfi_write_csr_reg() - write a value into a CSR register
 * @off:	offset (from base) to write
 * @val:	value to write
 *
 * Return: 0 on success, -ve on failure
 */
int vpu_hfi_write_csr_reg(u32 off, u32 val);

/**
 * vpu_hfi_dump_csr_regs() - dump the contents of the VPU CSR registers
 * @buf:	debug buffer to write into
 * @buf_size:	maximum size to read, in bytes
 *
 * Return: the number of bytes read
 */
int vpu_hfi_dump_csr_regs(char *buf, size_t buf_size);

/**
 * vpu_hfi_dump_smem_line() - dump the content of shared memory
 * @buf:	buffer to write into
 * @buf_size:	maximum size to read, in bytes
 * @offset:	smem read location (<base_addr> + offset)
 *
 * Return: the number of valid bytes in buf
 */
int vpu_hfi_dump_smem_line(char *buf, size_t size, u32 offset);

/* read the contents of a queue into buf; returns the number of bytes read */
int vpu_hfi_read_log_data(u32 cid, char *buf, int buf_size);

/**
 * vpu_hfi_set_watchdog() - enable/disable watchdog
 * @enable:	0 to disable, otherwise enable
  */
void vpu_hfi_set_watchdog(u32 enable);

#endif /* CONFIG_DEBUG_FS */

#endif /* __H_VPU_HFI_H__ */
