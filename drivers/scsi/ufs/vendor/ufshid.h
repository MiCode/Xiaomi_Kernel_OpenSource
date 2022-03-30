/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Universal Flash Storage Host Initiated Defrag (UFS HID)
 *
 * Copyright (C) 2019-2019 Samsung Electronics Co., Ltd.
 *
 * Authors:
 *	Yongmyung Lee <ymhungry.lee@samsung.com>
 *	Jieon Seol <jieon.seol@samsung.com>
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

#ifndef _UFSHID_H_
#define _UFSHID_H_

#include <linux/interrupt.h>
#include <linux/sysfs.h>
#include <linux/blktrace_api.h>
#include <linux/blkdev.h>
#include <linux/bitfield.h>
#include <scsi/scsi_cmnd.h>

#include "../../../block/blk.h"

#define UFSHID_VER					0x0101
#define UFSHID_DD_VER					0x010202
#define UFSHID_DD_VER_POST				""

#define UFS_FEATURE_SUPPORT_HID_BIT			0x400

#define HID_TRIGGER_WORKER_DELAY_MS_DEFAULT	2000
#define HID_TRIGGER_WORKER_DELAY_MS_MIN		100
#define HID_TRIGGER_WORKER_DELAY_MS_MAX		10000

#define HID_FRAG_LEVEL_MASK		0xF
#define HID_FRAG_UPDATE_STAT_SHIFT	30
#define HID_EXECUTE_REQ_STAT_SHIFT	31
#define HID_FRAG_UPDATE_STAT(val)	((val >> HID_FRAG_UPDATE_STAT_SHIFT) & 0x1)
#define HID_EXECUTE_REQ_STAT(val)	((val >> HID_EXECUTE_REQ_STAT_SHIFT) & 0x1)

#define HID_DEBUG(hid, msg, args...)					\
	do { if (hid->hid_debug)					\
		pr_err("%40s:%3d [%01d%02d%02d] " msg "\n",		\
		       __func__, __LINE__,				\
		       hid->hid_trigger,				\
		       atomic_read(&hid->ufsf->hba->dev->power.usage_count),\
		       hid->ufsf->hba->clk_gating.active_reqs, ##args);	\
	} while (0)

enum UFSHID_STATE {
	HID_NEED_INIT = 0,
	HID_PRESENT = 1,
	HID_SUSPEND = 2,
	HID_FAILED = -2,
	HID_RESET = -3,
};

enum {
	HID_OP_DISABLE	= 0,
	HID_OP_ANALYZE	= 1,
	HID_OP_EXECUTE	= 2,
	HID_OP_MAX
};

enum {
	HID_NOT_REQUIRED	= 0,
	HID_REQUIRED		= 1
};

enum {
	HID_LEV_GRAY	= 0,
	HID_LEV_GREEN	= 1,
	HID_LEV_YELLOW	= 2,
	HID_LEV_RED	= 3,
};

struct ufshid_dev {
	struct ufsf_feature *ufsf;

	unsigned int hid_trigger;   /* default value is false */
	struct delayed_work hid_trigger_work;
	unsigned int hid_trigger_delay;

	u32 ahit;			/* to restore ahit value */
	bool is_auto_enabled;

	/* for sysfs */
	struct kobject kobj;
	struct mutex sysfs_lock;
	struct ufshid_sysfs_entry *sysfs_entries;

	/* for debug */
	bool hid_debug;
#if defined(CONFIG_UFSHID_POC)
	bool block_suspend;
#endif
#if defined(CONFIG_UFSHID_DEBUG)
	u64 read_cnt;
	u64 write_cnt;
	u64 read_sec;
	u64 write_sec;
	u64 write_query_cnt;
#endif
};

struct ufshid_sysfs_entry {
	struct attribute attr;
	ssize_t (*show)(struct ufshid_dev *hid, char *buf);
	ssize_t (*store)(struct ufshid_dev *hid, const char *buf, size_t count);
};

int ufshid_get_state(struct ufsf_feature *ufsf);
void ufshid_set_state(struct ufsf_feature *ufsf, int state);
void ufshid_get_dev_info(struct ufsf_feature *ufsf, u8 *desc_buf);
void ufshid_set_init_state(struct ufsf_feature *ufsf);
void ufshid_init(struct ufsf_feature *ufsf);
void ufshid_reset(struct ufsf_feature *ufsf);
void ufshid_reset_host(struct ufsf_feature *ufsf);
void ufshid_remove(struct ufsf_feature *ufsf);
void ufshid_suspend(struct ufsf_feature *ufsf);
void ufshid_resume(struct ufsf_feature *ufsf);
void ufshid_on_idle(struct ufsf_feature *ufsf);
void ufshid_acc_io_stat_during_trigger(struct ufsf_feature *ufsf,
				       struct ufshcd_lrb *lrbp);
#endif /* End of Header */
