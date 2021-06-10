/*
 * Universal Flash Storage tw Write
 *
 * Copyright (C) 2017-2018 Samsung Electronics Co., Ltd.
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

#ifndef _UFSTW_H_
#define _UFSTW_H_

#include <linux/interrupt.h>
#include <linux/sysfs.h>
#include <linux/blktrace_api.h>
#include <linux/blkdev.h>
#include <scsi/scsi_cmnd.h>

#include "../../../block/blk.h"

#define UFSTW_VER					0x0101
#define UFSTW_DD_VER					0x0103

#define UFSTW_FLUSH_CHECK_PERIOD_MS			1000
#define UFSTW_FLUSH_WORKER_TH_MIN			3
#define UFSTW_FLUSH_WORKER_TH_MAX			8
#define UFSTW_LIFETIME_SECT				2097152 /* 1GB */
#define UFSTW_MAX_LIFETIME_VALUE			0x0B
/* TW 1.0.1[31], TW 1.1.0[7] */
#define MASK_UFSTW_LIFETIME_NOT_GUARANTEE		0x80000080

/*
 * UFSTW DEBUG
 */
#define TW_DEBUG(ufsf, msg, args...)			\
	do { if (ufsf->tw_debug)			\
		printk(KERN_ERR "%s:%d " msg "\n",	\
		       __func__, __LINE__, ##args);	\
	} while (0)

enum {
	FLUSH_IDLE = 0,
	FLUSH_RUN,
	FLUSH_COMPLETE,
	FLUSH_FAIL,
	FLUSH_NUM_OF_STATE,
};

enum UFSTW_STATE {
	TW_NOT_SUPPORTED = -1,
	TW_NEED_INIT = 0,
	TW_PRESENT = 1,
	TW_FAILED = -2,
	TW_RESET = -3,
};

enum {
	TW_MODE_DISABLED,
	TW_MODE_MANUAL,
	TW_MODE_FS,
	TW_MODE_NUM
};

enum {
	TW_EE_MODE_DISABLE,
	TW_EE_MODE_AUTO,
	TW_EE_MODE_NUM
};

enum {
	TW_FLAG_ENABLE_NONE = 0,
	TW_FLAG_ENABLE_CLEAR = 1,
	TW_FLAG_ENABLE_SET = 2,
};

struct ufstw_dev_info {
	bool tw_device;

	/* from Device Descriptor */
	u16 tw_ver;
	u8 tw_buf_no_reduct;
	u8 tw_buf_type;

	/* from Geometry Descriptor */
	u8 tw_number_lu;
};

struct ufstw_lu {
	struct ufsf_feature *ufsf;

	int lun;

	/* Flags */
	bool tw_flush_enable;
	bool tw_flush_during_hibern_enter;
	struct mutex flush_lock;

	/* lifetiem estimated */
	unsigned int tw_lifetime_est;
	spinlock_t lifetime_lock;
	u32 stat_write_sec;
	struct work_struct tw_lifetime_work;

	/* Attributes */
	unsigned int tw_flush_status;
	unsigned int tw_available_buffer_size;
	unsigned int tw_current_tw_buffer_size;

	/* mode manual/fs */
	atomic_t tw_mode;
	bool tw_enable;
	atomic_t active_cnt;
	struct mutex mode_lock;

	/* Worker */
	struct delayed_work tw_flush_work;
	struct delayed_work tw_flush_h8_work;
	unsigned long next_q;
	unsigned int flush_th_max;
	unsigned int flush_th_min;

	/* for sysfs */
	struct kobject kobj;
	struct mutex sysfs_lock;
	struct ufstw_sysfs_entry *sysfs_entries;
};

struct ufstw_sysfs_entry {
	struct attribute attr;
	ssize_t (*show)(struct ufstw_lu *tw, char *buf);
	ssize_t (*store)(struct ufstw_lu *tw, const char *buf, size_t count);
};

struct ufshcd_lrb;

void ufstw_get_dev_info(struct ufstw_dev_info *tw_dev_info, u8 *desc_buf);
void ufstw_get_geo_info(struct ufstw_dev_info *tw_dev_info, u8 *geo_buf);
int ufstw_get_lu_info(struct ufsf_feature *ufsf, unsigned int lun, u8 *lu_buf);
void ufstw_init(struct ufsf_feature *ufsf);
void ufstw_prep_fn(struct ufsf_feature *ufsf, struct ufshcd_lrb *lrbp);
void ufstw_init_work_fn(struct work_struct *work);
void ufstw_ee_handler(struct ufsf_feature *ufsf);
void ufstw_error_handler(struct ufsf_feature *ufsf);
void ufstw_reset_work_fn(struct work_struct *work);
void ufstw_suspend(struct ufsf_feature *ufsf);
void ufstw_resume(struct ufsf_feature *ufsf);
void ufstw_release(struct kref *kref);
bool ufstw_need_flush(struct ufsf_feature *ufsf);


#endif /* End of Header */
