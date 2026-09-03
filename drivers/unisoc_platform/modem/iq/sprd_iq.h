/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2018 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _SPRD_IQ_H
#define _SPRD_IQ_H

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sprd-iq: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/sizes.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>
#include <linux/mod_devicetable.h>
#include <linux/memblock.h>
#include <linux/poll.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/debugfs.h>
#include <linux/sipc.h>
#ifdef CONFIG_X86
#include <asm/cacheflush.h>
#endif
#include <uapi/linux/sched/types.h>

#define MAX_CHAR_NUM               128

#define IQ_BUF_INIT                0x5A5A5A5A
#define IQ_BUF_WRITE_FINISHED      0x5A5A8181
#define IQ_BUF_READ_FINISHED       0x81815A5A
#define IQ_BUF_READING             0x81005a00

#define IQ_BUF_OPEN                0x424F504E
#define IQ_BUF_LOCK                0x424C434B
#define DATA_AP_MOVE               0x4441504D
#define DATA_AP_MOVING             0x504D4441
#define DATA_CP_INJECT             0x44435049
#define DATA_AP_MOVE_FINISH        DATA_CP_INJECT
#define DATA_RESET                 0x44525354
#define MAX_PB_HEADER_SIZE		   0x100

#define IQ_TRANSFER_SIZE (500*1024)

#define SPRD_IQ_CLASS_NAME		"sprd_iq"

#define CMDLINE_SIZE 0x1000

#if IS_ENABLED(CONFIG_USB_F_VSERIAL)
extern void kernel_vser_register_callback(void *function, void *p);
extern ssize_t vser_pass_user_write(char *buf, size_t count);
extern void kernel_vser_set_pass_mode(bool pass);
#endif

#if IS_ENABLED(CONFIG_USB_F_VSERIAL_BYPASS_USER)
#define _kernel_vser_register_callback(para1, para2) kernel_vser_register_callback(para1, para2)
#define _vser_pass_user_write(para1, para2) vser_pass_user_write(para1, para2)
#define _kernel_vser_set_pass_mode(para) kernel_vser_set_pass_mode(para)
#else
#define _vser_pass_user_write(para1, para2) para2
#define _kernel_vser_register_callback(para1, para2)
#define _kernel_vser_set_pass_mode(para)
#endif

enum {
	CMD_GET_IQ_BUF_INFO = 0x0,
	CMD_GET_IQ_PB_INFO,
	CMD_SET_IQ_CH_TYPE = 0x80,
	CMD_SET_IQ_WR_FINISHED,
	CMD_SET_IQ_RD_FINISHED,
	CMD_SET_IQ_MOVE_FINISHED,
};

enum {
	IQ_USB_MODE = 0,
	IQ_SLOG_MODE,
	PLAY_BACK_MODE,
};

struct iq_buf_info {
	u32 base_offs;
	u32 data_len;
};

struct iq_header {
	u32  WR_RD_FLAG;
	u32  data_addr;
	u32  data_len;
	u32  reserved;
};

struct iq_pb_data_header {
	u32 data_status;
	u32 iqdata_offset;
	u32 iqdata_length;
	char iqdata_filename[MAX_CHAR_NUM];
};

struct iq_header_info {
	struct iq_header *head_1;
	struct iq_header *head_2;
	struct iq_pb_data_header *ipd_head;
};

struct sprd_iq_mgr {
	uint mode;
	uint ch;
	phys_addr_t base;
	phys_addr_t size;
	u32 mapping_offs;
	void *vbase;
	struct reserved_mem *rmem;
	struct iq_header_info *header_info;
	struct task_struct *iq_thread;
	wait_queue_head_t wait;
};
#endif //_SPRD_IQ_H
