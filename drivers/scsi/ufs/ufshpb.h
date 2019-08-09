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

#ifndef _UFSHPB_H_
#define _UFSHPB_H_

#include <linux/spinlock.h>
#include <linux/circ_buf.h>
#include <linux/workqueue.h>

#define UFSHPB_ERROR_INJECT

/* Version info*/
#define UFSHPB_VER				0x0103
#define UFSHPB_DD_VER				0x0135

/* Constant value*/
#define SECTOR					512
#define BLOCK					4096
#define SECTORS_PER_BLOCK			(BLOCK / SECTOR)
#define BITS_PER_DWORD				32
#define MAX_MAP_REQ				16
#define MAX_ACTIVE_NUM				2
#define MAX_INACTIVE_NUM			2

#define HPB_ENTRY_SIZE				0x08
#define OS_PAGE_SIZE				4096
#define HPB_ENTREIS_PER_OS_PAGE			(OS_PAGE_SIZE / HPB_ENTRY_SIZE)
#define IOCTL_DEV_CTX_MAX_SIZE			OS_PAGE_SIZE
#define OS_PAGE_SHIFT				12

/* Description */
#define UFS_FEATURE_SUPPORT_HPB_BIT			0x80
#define UFSHPB_QUERY_DESC_DEVICE_MAX_SIZE		0x48
#define UFSHPB_QUERY_DESC_CONFIGURAION_MAX_SIZE		0xD0
#define UFSHPB_QUERY_DESC_UNIT_MAX_SIZE			0x2C
#define UFSHPB_QUERY_DESC_GEOMETRY_MAX_SIZE		0x50

/* Response UPIU types */
#define HPB_RSP_NONE					0x00
#define HPB_RSP_REQ_REGION_UPDATE			0x01
#define PER_ACTIVE_INFO_BYTES				4
#define PER_INACTIVE_INFO_BYTES				2

/* Vender defined OPCODE */
#define UFSHPB_READ_BUFFER				0xF9

#define DEV_DATA_SEG_LEN				0x14
#define DEV_SENSE_SEG_LEN				0x12
#define DEV_DES_TYPE					0x80
#define DEV_ADDITIONAL_LEN				0x11

/* BYTE SHIFT */
#define ZERO_BYTE_SHIFT					0
#define ONE_BYTE_SHIFT					8
#define TWO_BYTE_SHIFT					16
#define THREE_BYTE_SHIFT				24
#define FOUR_BYTE_SHIFT					32
#define FIVE_BYTE_SHIFT					40
#define SIX_BYTE_SHIFT					48
#define SEVEN_BYTE_SHIFT				56

#define SHIFT_BYTE_0(num)		((num) << ZERO_BYTE_SHIFT)
#define SHIFT_BYTE_1(num)		((num) << ONE_BYTE_SHIFT)
#define SHIFT_BYTE_2(num)		((num) << TWO_BYTE_SHIFT)
#define SHIFT_BYTE_3(num)		((num) << THREE_BYTE_SHIFT)
#define SHIFT_BYTE_4(num)		((num) << FOUR_BYTE_SHIFT)
#define SHIFT_BYTE_5(num)		((num) << FIVE_BYTE_SHIFT)
#define SHIFT_BYTE_6(num)		((num) << SIX_BYTE_SHIFT)
#define SHIFT_BYTE_7(num)		((num) << SEVEN_BYTE_SHIFT)

#define GET_BYTE_0(num)			(((num) >> ZERO_BYTE_SHIFT) & 0xff)
#define GET_BYTE_1(num)			(((num) >> ONE_BYTE_SHIFT) & 0xff)
#define GET_BYTE_2(num)			(((num) >> TWO_BYTE_SHIFT) & 0xff)
#define GET_BYTE_3(num)			(((num) >> THREE_BYTE_SHIFT) & 0xff)
#define GET_BYTE_4(num)			(((num) >> FOUR_BYTE_SHIFT) & 0xff)
#define GET_BYTE_5(num)			(((num) >> FIVE_BYTE_SHIFT) & 0xff)
#define GET_BYTE_6(num)			(((num) >> SIX_BYTE_SHIFT) & 0xff)
#define GET_BYTE_7(num)			(((num) >> SEVEN_BYTE_SHIFT) & 0xff)

#define REGION_UNIT_SIZE(bit_offset)		(0x01 << (bit_offset))

enum UFSHPB_STATE {
	HPB_PRESENT = 1,
	HPB_NOT_SUPPORTED = -1,
	HPB_FAILED = -2,
	HPB_NEED_INIT = 0,
	HPB_RESET = -3,
};

enum HPBREGION_STATE {
	HPBREGION_INACTIVE, HPBREGION_ACTIVE, HPBREGION_PINNED,
};

enum HPBSUBREGION_STATE {
	HPBSUBREGION_UNUSED,
	HPBSUBREGION_DIRTY,
	HPBSUBREGION_CLEAN,
	HPBSUBREGION_ISSUED,
};

struct ufshpb_func_desc {
	/*** Device Descriptor ***/
	/* 06h bNumberLU */
	int lu_cnt;
	/* 40h HPB Version */
	u16 hpb_ver;

	/*** Geometry Descriptor ***/
	/* 48h bHPBRegionSize (UNIT: 512KB) */
	u8 hpb_region_size;
	/* 49h bHPBNumberLU */
	u8 hpb_number_lu;
	/* 4Ah bHPBSubRegionSize */
	u8 hpb_subregion_size;
	/* 4B:4Ch wDeviceMaxActiveHPBRegions */
	u16 hpb_device_max_active_regions;
};

struct ufshpb_lu_desc {
	/*** Unit Descriptor ****/
	/* 03h bLUEnable */
	int lu_enable;
	/* 06h lu queue depth info*/
	int lu_queue_depth;
	/* 0Ah bLogicalBlockSize. default 0x0C = 4KB */
	int lu_logblk_size;
	/* 0Bh qLogicalBlockCount. same as the read_capacity ret val. */
	u64 lu_logblk_cnt;

	/* 23h:24h wLUMaxActiveHPBRegions */
	u16 lu_max_active_hpb_regions;
	/* 25h:26h wHPBPinnedRegionStartIdx */
	u16 hpb_pinned_region_startidx;
	/* 27h:28h wNumHPBPinnedRegions */
	u16 lu_num_hpb_pinned_regions;


	/* if 03h value is 02h, hpb_enable is set. */
	bool lu_hpb_enable;

	int lu_hpb_pinned_end_offset;
};

struct ufshpb_rsp_active_list {
	u16 region[MAX_ACTIVE_NUM];
	u16 subregion[MAX_ACTIVE_NUM];
};

struct ufshpb_rsp_inactive_list {
	u16 region[MAX_INACTIVE_NUM];
};

struct ufshpb_rsp_update_entry {
	unsigned int lpn;
	unsigned long long ppn;
};

struct ufshpb_rsp_info {
	int type;
	int active_cnt;
	int inactive_cnt;
	struct ufshpb_rsp_active_list active_list;
	struct ufshpb_rsp_inactive_list inactive_list;

	__u64 RSP_start;
	__u64 RSP_tasklet_enter;

	struct list_head list_rsp_info;
};

struct ufshpb_rsp_field {
	u8 sense_data_len[2];
	u8 desc_type;
	u8 additional_len;
	u8 hpb_type;
	u8 reserved;
	u8 active_region_cnt;
	u8 inactive_region_cnt;
	u8 hpb_active_field[8];
	u8 hpb_inactive_field[4];
};

struct ufshpb_map_ctx {
	struct page **m_page;
	unsigned int *ppn_dirty;

	struct list_head list_table;
};

struct ufshpb_subregion {
	struct ufshpb_map_ctx *mctx;
	enum HPBSUBREGION_STATE subregion_state;
	int region;
	int subregion;

	struct list_head list_subregion;
};

struct ufshpb_region {
	struct ufshpb_subregion *subregion_tbl;
	enum HPBREGION_STATE region_state;
	int region;
	int subregion_count;

	/*below information is used by lru*/
	struct list_head list_region;
	int hit_count;
};

struct ufshpb_map_req {
	struct ufshpb_lu *hpb;
	struct ufshpb_map_ctx *mctx;
	struct request req;
	struct bio bio;
#define MAX_BVEC_SIZE 128
	struct bio_vec bvec[MAX_BVEC_SIZE];
	void (*end_io)(struct request *rq, int err);
	void *end_io_data;
	int region;
	int subregion;
	int lun;
	int retry_cnt;

	/* for debug : RSP Profiling */
	__u64 RSP_start; // get the request from device
	__u64 RSP_tasklet_enter1; // tesklet sched time
	__u64 RSP_issue; // issue scsi cmd
	__u64 RSP_endio;
	__u64 RSP_tasklet_enter2;
	__u64 RSP_end;	 // complete the request

	char sense[SCSI_SENSE_BUFFERSIZE];

	struct list_head list_map_req;
};

enum selection_type {
	LRU = 1,
	LFU = 2,
};

struct victim_select_info {
	int selection_type;
	struct list_head lru;
	int max_lru_active_cnt;	// supported hpb #region - pinned #region
	atomic64_t active_cnt;
};

struct ufshpb_lu {
	struct ufshpb_region *region_tbl;
	struct ufshpb_rsp_info *rsp_info;
	struct ufshpb_map_req *map_req;

	struct list_head lh_map_ctx;
	struct list_head lh_subregion_req;
	struct list_head lh_rsp_info;

	struct list_head lh_rsp_info_free;
	struct list_head lh_map_req_free;
	struct list_head lh_map_req_retry;
	int debug_free_table;

	bool lu_hpb_enable;

	struct work_struct ufshpb_work;
	struct delayed_work ufshpb_retry_work;
	struct tasklet_struct ufshpb_tasklet;

	int subregions_per_lu;
	int regions_per_lu;
	int subregion_mem_size;

	/* for selecting victim */
	struct victim_select_info lru_info;

	int hpb_ver;
	int lu_max_active_regions;

	int entries_per_subregion;
	int entries_per_subregion_shift;
	int entries_per_subregion_mask;

	int entries_per_region_shift;
	int entries_per_region_mask;
	int subregions_per_region;

	int dwords_per_subregion;
	unsigned long long subregion_unit_size;

	int mpage_bytes;
	int mpages_per_subregion;

	/* for debug constant variables */
	int lu_num_blocks;

	int lun;

	struct ufs_hba *hba;

	spinlock_t hpb_lock;
	spinlock_t rsp_list_lock;

	struct kobject kobj;
	struct mutex sysfs_lock;
	struct ufshpb_sysfs_entry *sysfs_entries;

	/* for debug */
	bool force_disable;
	bool force_map_req_disable;
	bool debug;
	bool read_buf_debug;
	atomic64_t hit;
	atomic64_t miss;
	atomic64_t region_miss;
	atomic64_t subregion_miss;
	atomic64_t entry_dirty_miss;
	atomic64_t rb_noti_cnt;
	atomic64_t map_req_cnt;
	atomic64_t region_add;
	atomic64_t region_evict;
	atomic64_t rb_fail;
#ifdef UFSHPB_ERROR_INJECT
	bool err_inject;
#endif
};

struct ufshpb_sysfs_entry {
	struct attribute    attr;
	ssize_t (*show)(struct ufshpb_lu *hpb, char *buf);
	ssize_t (*store)(struct ufshpb_lu *hpb, const char *, size_t);
};

struct ufshcd_lrb;

void ufshpb_init_handler(struct work_struct *work);
void ufshpb_prep_fn(struct ufs_hba *hba, struct ufshcd_lrb *lrbp);
void ufshpb_rsp_upiu(struct ufs_hba *hba, struct ufshcd_lrb *lrbp);
void ufshpb_release(struct ufs_hba *hba, int state);
int ufshpb_issue_req_dev_ctx(struct ufshpb_lu *hpb, unsigned char *buf,
	int buf_length);
#endif /* End of Header */
