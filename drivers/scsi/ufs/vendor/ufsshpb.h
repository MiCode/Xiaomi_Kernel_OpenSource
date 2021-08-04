/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Universal Flash Storage Host Performance Booster
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

#ifndef _UFSSHPB_H_
#define _UFSSHPB_H_

#include <linux/interrupt.h>
#include <linux/sysfs.h>
#include <linux/blktrace_api.h>
#include <linux/blkdev.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_driver.h>
#include <linux/of.h>

#if defined(CONFIG_HPB_ERR_INJECTION)
#include <linux/random.h>
#endif

#include "../../../block/blk.h"
#include "../../../block/blk-mq.h"
#include "../../../block/blk-mq-sched.h"
#include "../scsi_priv.h"

/* Version info*/
#define UFSSHPB_VER				0x0221
#define UFSSHPB_DD_VER				0x020705
#define UFSSHPB_DD_VER_POST			""

/* Constant value*/
#define MAX_ACTIVE_NUM				2
#define MAX_INACTIVE_NUM			2

#define HPB_ENTRY_SIZE				0x08
#define HPB_ENTREIS_PER_OS_PAGE			(OS_PAGE_SIZE / HPB_ENTRY_SIZE)

#define RETRY_DELAY_MS				5000

/* HPB Support Chunk Size */
#define HPB_MULTI_CHUNK_LOW			9
#define HPB_MULTI_CHUNK_HIGH			128
#define MAX_HPB_CONTEXT_ID			0x7f

/* Description */
#define UFS_FEATURE_SUPPORT_HPB_BIT		0x80

/* Response UPIU types */
#define HPB_RSP_NONE				0x00
#define HPB_RSP_REQ_REGION_UPDATE		0x01

/* Vender defined OPCODE */
#define UFSSHPB_READ_BUFFER			0xF9
#define UFSSHPB_WRITE_BUFFER			0xFA

#define UFSSHPB_GROUP_NUMBER			0x11
#define UFSSHPB_RB_ID_READ			0x01
#define UFSSHPB_RB_ID_SET_HCM			0x02
#define UFSSHPB_WB_ID_UNSET_HCM			0x01
#define UFSSHPB_WB_ID_UNSET_HCM_ALL		0x03
#define UFSSHPB_WB_ID_PREFETCH			0x02
#define TRANSFER_LEN				0x01

#define DEV_DATA_SEG_LEN			0x14
#define DEV_SENSE_SEG_LEN			0x12
#define DEV_DES_TYPE				0x80
#define DEV_ADDITIONAL_LEN			0x10

#define PINNED_NOT_SET				(-1)
#define DEFAULT_WB_REQUEUE_TIME_MS		10

#if defined(CONFIG_HPB_DEBUG_SYSFS)
#define BLOCK_KB				4
#endif

/*
 * UFSSHPB DEBUG
 */

#define HPB_DEBUG(hpb, msg, args...)			\
	do { if (hpb->debug)				\
		printk(KERN_ERR "%s:%d " msg "\n",	\
		       __func__, __LINE__, ##args);	\
	} while (0)

#define TMSG_CMD(hpb, msg, rq, rgn, srgn)				\
	do { if (hpb->ufsf->sdev_ufs_lu[hpb->lun] &&			\
		 hpb->ufsf->sdev_ufs_lu[hpb->lun]->request_queue)	\
			blk_add_trace_msg(				\
			hpb->ufsf->sdev_ufs_lu[hpb->lun]->request_queue,\
			"%llu + %u " msg " %d - %d",			\
			(unsigned long long) blk_rq_pos(rq),		\
			(unsigned int) blk_rq_sectors(rq), rgn, srgn);	\
	} while (0)

enum UFSSHPB_STATE {
	HPB_PRESENT = 1,
	HPB_SUSPEND = 2,
	HPB_FAILED = -2,
	HPB_NEED_INIT = 0,
	HPB_WAIT_INIT = -1,
	HPB_RESET = -3,
};

enum HPB_RGN_STATE {
	HPB_RGN_INACTIVE,
	HPB_RGN_ACTIVE,
	HPB_RGN_PINNED,
	HPB_RGN_HCM,
};

enum HPB_SRGN_STATE {
	HPB_SRGN_UNUSED,
	HPB_SRGN_DIRTY,
	HPB_SRGN_CLEAN,
	HPB_SRGN_ISSUED,
};

enum HPB_UPDATE_INFO {
	HPB_UPDATE_NONE,
	HPB_UPDATE_FROM_DEV,
	HPB_UPDATE_FROM_FS,
};

#if defined(CONFIG_HPB_ERR_INJECTION)
enum HPB_ERR_INJECTION_SELECT {
	HPB_ERR_INJECTION_DISABLE,
	HPB_ERR_INJECTION_BITFLIP,
	HPB_ERR_INJECTION_OFFSET,
	HPB_ERR_INJECTION_RANDOM,
};
#endif

struct ufsshpb_dev_info {
	int num_lu;
	int version;
	int rgn_size;
	int srgn_size;
};

struct ufsshpb_active_field {
	__be16 active_rgn;
	__be16 active_srgn;
} __packed;

struct ufsshpb_rsp_field {
	__be16 sense_data_len;
	u8 desc_type;
	u8 additional_len;
	u8 hpb_type;
	u8 reserved;
	u8 active_rgn_cnt;
	u8 inactive_rgn_cnt;
	struct ufsshpb_active_field hpb_active_field[2];
	__be16 hpb_inactive_field[2];
} __packed;

struct ufsshpb_map_ctx {
	struct page **m_page;
	unsigned int *ppn_dirty;

	struct list_head list_table;
};

struct ufsshpb_subregion {
	struct ufsshpb_map_ctx *mctx;
	enum HPB_SRGN_STATE srgn_state;
	int rgn_idx;
	int srgn_idx;

	/* below information is used by rsp_list */
	struct list_head list_act_srgn;
};

struct ufsshpb_region {
	struct ufsshpb_subregion *srgn_tbl;
	enum HPB_RGN_STATE rgn_state;
	int rgn_idx;
	int srgn_cnt;

	/* below information is used by rsp_list */
	struct list_head list_inact_rgn;
	atomic_t reason;

	/* below information is used by lru */
	struct list_head list_lru_rgn;

#if defined(CONFIG_HPB_DEBUG_SYSFS)
	/* for debug */
	atomic64_t rgn_pinned_low_hit;
	atomic64_t rgn_pinned_high_hit;
	atomic64_t rgn_pinned_low_miss;
	atomic64_t rgn_pinned_high_miss;
	atomic64_t rgn_active_low_hit;
	atomic64_t rgn_active_high_hit;
	atomic64_t rgn_active_low_miss;
	atomic64_t rgn_active_high_miss;
	atomic64_t rgn_inactive_low_read;
	atomic64_t rgn_inactive_high_read;
	atomic64_t rgn_pinned_rb_cnt;
	atomic64_t rgn_active_rb_cnt;
#endif
};

struct ufsshpb_req {
	struct request *req;
	struct bio *bio;
	struct ufsshpb_lu *hpb;
	struct list_head list_req;
	void (*end_io)(struct request *rq, int err);
	void *end_io_data;
	char sense[SCSI_SENSE_BUFFERSIZE];

	union {
		struct {
			struct ufsshpb_map_ctx *mctx;
			unsigned int rgn_idx;
			unsigned int srgn_idx;
			unsigned int lun;
		} rb;
		struct {
			struct page *m_page;
			unsigned int len;
			unsigned long lpn;
		} wb;
	};
};

enum selection_type {
	LRU = 1,
};

struct victim_select_info {
	int selection_type;

	/* for active region */
	struct list_head lh_lru_rgn;
	int max_lru_active_cnt; /* supported hpb #region - pinned #region */
	atomic64_t active_cnt;

	/* for HCM_active region*/
	struct list_head lh_lru_hcm_region;
};

struct ufsshpb_lu {
	struct ufsf_feature *ufsf;
	int lun;
	int qd;
	struct ufsshpb_region *rgn_tbl;

	spinlock_t hpb_lock;

	spinlock_t retry_list_lock;
	struct ufsshpb_req *map_req;
	int num_inflight_map_req;
	int throttle_map_req;
	struct list_head lh_map_req_free;
	struct list_head lh_map_req_retry;
	struct list_head lh_map_ctx_free;

	spinlock_t rsp_list_lock;
	struct list_head lh_pinned_srgn;
	struct list_head lh_act_srgn;
	struct list_head lh_inact_rgn;

	struct kobject kobj;
	struct mutex sysfs_lock;
	struct ufsshpb_sysfs_entry *sysfs_entries;

	struct ufsshpb_req *pre_req;
	int num_inflight_pre_req;
	int throttle_pre_req;
	struct list_head lh_pre_req_free;
	int ctx_id_ticket;
	int pre_req_min_tr_len;
	int pre_req_max_tr_len;
	unsigned int requeue_timeout_ms;

	struct work_struct pinned_work;
	struct delayed_work retry_work;
	struct work_struct task_work;

	/* for selecting victim */
	struct victim_select_info lru_info;

	int lu_max_active_rgns;
	int lu_pinned_rgn_startidx;
	int lu_pinned_end_offset;
	int lu_num_pinned_rgns;
	int srgns_per_lu;
	int rgns_per_lu;
	int srgns_per_rgn;
	int srgn_mem_size;
	int entries_per_rgn_shift;
	int entries_per_rgn_mask;
	int entries_per_srgn;
	int entries_per_srgn_shift;
	int entries_per_srgn_mask;
	int dwords_per_srgn;
	unsigned long long srgn_unit_size;
	int mpage_bytes;
	int mpages_per_srgn;
	int lu_num_blocks;

	/* for debug */
	int alloc_mctx;
	int debug_free_table;
	int throttle_hcm_req;
	int num_inflight_hcm_req;
#if defined(CONFIG_HPB_DEBUG_SYSFS)
	int lba_rgns_info;
#endif
	bool hcm_disable;
	bool force_disable;
	bool force_map_req_disable;
	bool debug;
	atomic64_t hit;
	atomic64_t miss;
	atomic64_t rb_noti_cnt;
	atomic64_t rb_active_cnt;
	atomic64_t rb_inactive_cnt;
	atomic64_t dev_inact_hcm_cnt;
	atomic64_t map_req_cnt;
	atomic64_t map_compl_cnt;
	atomic64_t pre_req_cnt;
	atomic64_t set_hcm_req_cnt;
	atomic64_t unset_hcm_req_cnt;
#if defined(CONFIG_HPB_DEBUG_SYSFS)
	atomic64_t pinned_low_hit;
	atomic64_t pinned_high_hit;
	atomic64_t pinned_low_miss;
	atomic64_t pinned_high_miss;
	atomic64_t active_low_hit;
	atomic64_t active_high_hit;
	atomic64_t active_low_miss;
	atomic64_t active_high_miss;
	atomic64_t inactive_low_read;
	atomic64_t inactive_high_read;
	atomic64_t pinned_rb_cnt;
	atomic64_t active_rb_cnt;

	atomic64_t act_rsp_list_cnt;
	atomic64_t inact_rsp_list_cnt;
	atomic64_t fs_set_hcm_cnt;
	atomic64_t fs_unset_hcm_cnt;
	atomic64_t unset_hcm_victim;
#endif
#if defined(CONFIG_HPB_ERR_INJECTION)
	enum HPB_ERR_INJECTION_SELECT err_injection_select;
	u64 err_injection_bitflip;
	u64 err_injection_offset;
	int err_injection_random;
#endif
};

struct ufsshpb_sysfs_entry {
	struct attribute    attr;
	ssize_t (*show)(struct ufsshpb_lu *hpb, char *buf);
	ssize_t (*store)(struct ufsshpb_lu *hpb, const char *, size_t);
};

struct ufs_hba;
struct ufshcd_lrb;

int ufsshpb_get_state(struct ufsf_feature *ufsf);
void ufsshpb_set_state(struct ufsf_feature *ufsf, int state);
void ufsshpb_get_dev_info(struct ufsf_feature *ufsf, u8 *desc_buf);
void ufsshpb_get_geo_info(struct ufsf_feature *ufsf, u8 *geo_buf);
void ufsshpb_get_lu_info(struct ufsf_feature *ufsf, int lun, u8 *unit_buf);
void ufsshpb_init_handler(struct work_struct *work);
void ufsshpb_rsp_upiu(struct ufsf_feature *ufsf, struct ufshcd_lrb *lrbp);
void ufsshpb_resume(struct ufsf_feature *ufsf);
void ufsshpb_suspend(struct ufsf_feature *ufsf);
void ufsshpb_reset_host(struct ufsf_feature *ufsf);
void ufsshpb_reset(struct ufsf_feature *ufsf);
int ufsshpb_prep_fn(struct ufsf_feature *ufsf, struct ufshcd_lrb *lrbp);
void ufsshpb_remove(struct ufsf_feature *ufsf, int state);
#endif /* End of Header */
