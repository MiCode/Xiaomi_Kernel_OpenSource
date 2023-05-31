/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Universal Flash Storage Ring Buffer
 *
 * Copyright (C) 2019-2019 Samsung Electronics Co., Ltd.
 *
 * Authors:
 *	Yongmyung Lee <ymhungry.lee@samsung.com>
 *	Jinyoung Choi <j-young.choi@samsung.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * See the COPYING file in the top-level directory or visit
 * <http://www.gnu.org/licenses/gpl-2.0.html>
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * This program is provided "AS IS" and "WITH ALL FAULTS" and
 * without warranty of any kind. You are solely responsible for
 * determining the appropriateness of using and distributing
 * the program and assume all risks associated with your exercise
 * of rights with respect to the program, including but not limited
 * to infringement of third party rights, the risks and costs of
 * program errors, damage to or loss of data, programs or equipment,
 * and unavailability or interruption of operations. Under no
 * circumstances will the contributor of this Program be liable for
 * any damages of any kind arising from your use or distribution of
 * this program.
 *
 * The Linux Foundation chooses to take subject only to the GPLv2
 * license terms, and distributes only under these terms.
 */

#ifndef _UFSRINGBUF_H_
#define _UFSRINGBUF_H_

#include <linux/proc_fs.h>
#include <linux/sysfs.h>
#include <linux/seq_file.h>
#include <asm/unaligned.h>
#include <scsi/scsi_common.h>
#include <linux/kernel.h>
#include <linux/random.h>
#include <linux/time.h>

#define UFSRINGBUF_VER		0x0101
#define UFSRINGBUF_DD_VER	0x010200
#define UFSRINGBUF_DD_VER_POST	""

#define UFS_FEATURE_SUPPORT_RINGBUF_BIT		0x800

#define RESET_DELAY		(5 * HZ)

/* for buffer */
#define HIST_BUFFER_UNIT		512
#define HIST_BLOCK_DESC_BYTES		50
#define HIST_BUFFER_HEADER_BYTES	16
#define HIST_BLK_DESC_START		16

/* for read buffer cmd */
#define RING_BUFFER_MODE	0x1D
#define GET_VOLATILE_BUF	0x00
#define GET_NON_VOLATILE_BUF	0x01

/* for vendor cmd */
#define GET_DEFAULT_LU		0
#define VENDOR_CMD_TIMEOUT	(10 * HZ)
#define VENDOR_CMD_OP		0xC0
#define VENDOR_CDB_LENGTH	16
#define ENTER_VENDOR		0x00
#define EXIT_VENDOR		0x01
#define SET_PASSWD		0x03
#define VENDOR_INPUT_LEN	8
#define MAX_CDB_SIZE		16	/* for compatibility */

/* for debugfs */
#define BUFF_LINE_SIZE		16
#define RW	0600
#define R	0400
#define W	0200

#define RINGBUF_VENDOR_SIG	0xABCDDCBA

/* for POC*/
#if defined(CONFIG_UFSRINGBUF_POC)
#define INPUT_SIG 0x9E4829C1
#define INPUT_PARAM 0X00000000
#endif

#define RINGBUF_MSG(file, msg, args...)					\
	do {	if (file)						\
			seq_printf(file, msg "\n", ##args);		\
		else							\
			pr_err(msg "\n", ##args); } while (0)		\

enum UFSRINGBUF_STATE {
	RINGBUF_NEED_INIT = 0,
	RINGBUF_PRESENT = 1,
	RINGBUF_FAILED = -2,
	RINGBUF_RESET = -3,
};

struct history_block_desc {
	__be32 host_time_a;	/* 0..3 */
	__be32 host_time_b;	/* 4..7 */
	__be32 device_time;	/* 8..11 */
	__u8 command_state;	/* 12 */
	__u8 response;		/* 13 */
	__u8 status;		/* 14 */
	__u8 sense_key;		/* 15 */
	__u8 asc;		/* 16 */
	__u8 ascq;		/* 17 */
	__be32 upiu[8];		/* 18..49 */
} __packed;

struct ufsringbuf_dev {
	struct ufsf_feature *ufsf;

	int transfer_bytes;

	bool volatile_hist;		/* default false */
	bool parsing;			/* default false */
	bool record_en_drv;		/* default false */

	u32 input_parameter; /* vendor specific */
	u32 input_signature;	/* vendor specific */

	void *msg_buffer;

	struct delayed_work ringbuf_reset_work;

	/* for sysfs & procfs */
	struct kobject kobj;
	struct mutex sysfs_lock;
	struct ufsringbuf_sysfs_entry *sysfs_entries;
	struct proc_dir_entry *ringbuf_proc_root;
};

struct ufsringbuf_sysfs_entry {
	struct attribute attr;
	ssize_t (*show)(struct ufsringbuf_dev *ringbuf, char *buf);
	ssize_t (*store)(struct ufsringbuf_dev *ringbuf, const char *buf,
			 size_t count);
};

struct ufshcd_lrb;

int ufsringbuf_get_state(struct ufsf_feature *ufsf);
void ufsringbuf_set_state(struct ufsf_feature *ufsf, int state);
void ufsringbuf_init(struct ufsf_feature *ufsf);
void ufsringbuf_get_dev_info(struct ufsf_feature *ufsf, u8 *desc_buf);
void ufsringbuf_get_geo_info(struct ufsf_feature *ufsf, u8 *geo_buf);
void ufsringbuf_prep_fn(struct ufsf_feature *ufsf, struct ufshcd_lrb *lrbp);
void ufsringbuf_reset_host(struct ufsf_feature *ufsf);
void ufsringbuf_reset(struct ufsf_feature *ufsf);
void ufsringbuf_remove(struct ufsf_feature *ufsf);
void ufsringbuf_resume(struct ufsf_feature *ufsf);
#endif /* ENd of Header */
