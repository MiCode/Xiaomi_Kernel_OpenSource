/*
 * Universal Flash Storage Host Performance Booster
 *
 * Original work Copyright (C) 2017-2018 Samsung Electronics Co., Ltd.
 * Modified work Copyright (C) 2018, Google, Inc.
 * Modified work Copyright (C) 2019 SK hynix
 *
 * Origianl work Authors:
 *	Yongmyung Lee <ymhungry.lee@samsung.com>
 *	Jinyoung Choi <j-young.choi@samsung.com>
 * Modifier
 * 	Kihyun Cho <kihyun.cho@sk.com>
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

#ifndef _UFSHPB10_H_
#define _UFSHPB10_H_

#include <linux/spinlock.h>
#include <linux/circ_buf.h>
#include <linux/workqueue.h>

/* Version info*/
#define UFSHPB10_DD_VER				0x010405

/* QUIRKs */
/* Use READ16 instead of HPB_READ command,
 * This is workaround solution to countmeasure QCT ICE issue
 */
#define UFSHPB_QUIRK_USE_READ_16_FOR_ENCRYPTION (1 << 0)
/* This quirk makes HPB driver always works as Devie Control Mode.
 * To cover old Configuration descriptor format which interpret
 * the bHPBControl field as RESERVED.
 */
#define UFSHPB_QUIRK_ALWAYS_DEVICE_CONTROL_MODE (1 << 1)


/* Constant value*/
#define SECTOR					512
#define BLOCK					4096
#define SECTORS_PER_BLOCK			(BLOCK / SECTOR)
#define BITS_PER_DWORD				32
#define MAX_MAP_REQ				16
#define MAX_ACTIVE_NUM				2
#define MAX_INACTIVE_NUM			2

#define HPB_ENTRY_SIZE				0x08
#define HPB10_ENTREIS_PER_OS_PAGE		(PAGE_SIZE / HPB_ENTRY_SIZE)

/* Description */
#define UFS_FEATURE_SUPPORT_HPB_BIT			0x80

#define UFSHPB_QUERY_DESC_DEVICE_MAX_SIZE		0x43
#define UFSHPB_QUERY_DESC_CONFIGURAION_MAX_SIZE		0xE6
#define UFSHPB_QUERY_DESC_UNIT_MAX_SIZE			0x29
#define UFSHPB_QUERY_DESC_GEOMETRY_MAX_SIZE		0x4D

/* Configuration for HPB */
#define UFSHPB_CONF_LU_ENABLE			0x00
#define UFSHPB_CONF_ACTIVE_REGIONS		0x10
#define UFSHPB_CONF_PINNED_START		0x12
#define UFSHPB_CONF_PINNED_NUM			0x14

/* Parameter Macros */
#define HPB_DEV(h)	((h)->hba->dev)
#define MAX_BVEC_SIZE	128

/* Use for HPB activate */
#define UFSHPB_CONFIG_LEN	0xd0

typedef u64 hpb_t;

enum ufshpb_lu_set {
	LU_DISABLE	= 0x00,
	LU_ENABLE	= 0x01,
	LU_HPB_ENABLE	= 0x02,
	LU_SET_MAX,
};

struct ufshpb_config_desc {
	unsigned char conf_dev_desc[16];
	unsigned char unit[UFS_UPIU_MAX_GENERAL_LUN][24];
};

/* Response UPIU types */
#define HPB_RSP_NONE					0x00
#define HPB_RSP_REQ_REGION_UPDATE			0x01
#define HPB_RSP_HPB_RESET				0x02
#define PER_ACTIVE_INFO_BYTES				4
#define PER_INACTIVE_INFO_BYTES				2

/* Vender defined OPCODE */
#define UFSHPB_READ				  		0xF8
#define UFSHPB_READ_BUFFER				0xF9

#define DEV_DATA_SEG_LEN				0x14
#define DEV_SENSE_SEG_LEN				0x12
#define DEV_DES_TYPE					0x80
#define DEV_ADDITIONAL_LEN				0x10

/* BYTE SHIFT */
#define ZERO_BYTE_SHIFT					0
#define ONE_BYTE_SHIFT					8
#define TWO_BYTE_SHIFT					16
#define THREE_BYTE_SHIFT				24

#define SHIFT_BYTE_0(num)		((num) << ZERO_BYTE_SHIFT)
#define SHIFT_BYTE_1(num)		((num) << ONE_BYTE_SHIFT)

#define REGION_UNIT_SIZE(bit_offset)		(0x01 << (bit_offset))

enum HPB_BUFFER_MODE {
	R_BUFFER = 0,
	W_BUFFER = 1,
};

enum HPB_CONTROL_MODE {
	HOST_CTRL_MODE = 0,
	DEV_CTRL_MODE = 1,
};

struct ufshpb_func_desc {
	/*** Device Descriptor ***/
	/* 06h bNumberLU */
	int lu_cnt;
	/* 40h HPB Version */
	u16 hpb_ver;
	/* 42h HPB control mode */
	u8  hpb_control_mode;

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
	hpb_t ppn;
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

struct ufshpb10_rsp_field {
	u8 sense_data_len[2];
	u8 desc_type;
	u8 additional_len;
	u8 hpb_type;
	u8 lun;
	u8 active_region_cnt;
	u8 inactive_region_cnt;
	u8 hpb_active_field[8];
	u8 hpb_inactive_field[4];
};
struct ufshpb10_subregion {
	struct ufshpb_map_ctx *mctx;
	enum HPBSUBREGION_STATE subregion_state;
	int region;
	int subregion;
	bool last;
	struct list_head list_subregion;
};

struct ufshpb10_region {
	struct ufshpb10_subregion *subregion_tbl;
	enum HPBREGION_STATE region_state;
	bool is_pinned;
	int region;
	int subregion_count;

	/*below information is used by lru*/
	struct list_head list_region;
	int hit_count;
};

struct ufshpb_map_req {
	struct ufshpb10_lu *hpb;
	struct ufshpb_map_ctx *mctx;
	struct bio bio;
	struct bio *pbio;
	struct bio_vec bvec[MAX_BVEC_SIZE];
	void (*end_io)(struct request *rq, int err);
	void *end_io_data;
	int region;
	int subregion;
	int subregion_mem_size;
	int lun;
	int retry_cnt;

	/* for debug : RSP Profiling */
	__u64 RSP_start; // get the request from device
	__u64 RSP_tasklet_enter; // tesklet sched time
	__u64 RSP_issue; // issue scsi cmd
	__u64 RSP_endio;
	__u64 RSP_tasklet_enter2;
	__u64 RSP_end;	 // complete the request

	char sense[SCSI_SENSE_BUFFERSIZE];

	struct list_head list_map_req;
};

struct victim10_select_info {
	int selection_type;
	struct list_head lru;
	int max_lru_active_count;	// supported hpb #region - pinned #region
	atomic64_t active_count;
};

struct ufshpb10_lu {
	struct ufshpb10_region *region_tbl;
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

	struct delayed_work ufshpb_pinned_work;
	struct delayed_work ufshpb_map_req_retry_work;
	struct tasklet_struct ufshpb_tasklet;
	struct bio_vec bvec[MAX_BVEC_SIZE];

	int subregions_per_lu;
	int regions_per_lu;
	int subregion_mem_size;
	int last_subregion_mem_size;

	/* for selecting victim */
	struct victim10_select_info lru_info;

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

#define BITS_PER_PPN_DIRTY (BITS_PER_BYTE * sizeof(unsigned int))
	int ppn_dirties_per_subregion;

	int mpage_bytes;
	int mpages_per_subregion;

	/* for debug constant variables */
	unsigned long long lu_num_blocks;

	int lun;

	struct ufs_hba *hba;

	spinlock_t hpb_lock;
	spinlock_t rsp_list_lock;

	struct kobject kobj;
	struct mutex sysfs_lock;
	struct ufshpb10_sysfs_entry *sysfs_entries;

	/* for debug */
	bool force_hpb_read_disable;
	bool force_map_req_disable;
	bool read_buf_debug;
	atomic64_t hit;
	atomic64_t miss;
	atomic64_t region_miss;
	atomic64_t subregion_miss;
	atomic64_t entry_dirty_miss;
	atomic64_t rb_noti_cnt;
	atomic64_t rb_fail;
	atomic64_t reset_noti_cnt;
//#if defined(HPB_READ_LARGE_CHUNK_SUPPORT)
	atomic64_t lc_entry_dirty_miss;
	atomic64_t lc_reg_subreg_miss;
	atomic64_t lc_err_miss;
//#endif
	atomic64_t map_req_cnt;
	atomic64_t region_add;
	atomic64_t region_evict;
	atomic64_t canceled_resp;
	atomic64_t canceled_map_req;
};

struct ufshpb10_sysfs_entry {
	struct attribute    attr;
	ssize_t (*show)(struct ufshpb10_lu *hpb, char *buf);
	ssize_t (*store)(struct ufshpb10_lu *hpb, const char *, size_t);
};

struct ufshcd_lrb;

void ufshcd10_init_hpb(struct ufs_hba *hba);
void ufshpb10_init_handler(struct work_struct *work);
void ufshpb10_prep_fn(struct ufs_hba *hba, struct ufshcd_lrb *lrbp);
void ufshpb10_rsp_upiu(struct ufs_hba *hba, struct ufshcd_lrb *lrbp);
void ufshpb10_suspend(struct ufs_hba *hba);
void ufshpb10_release(struct ufs_hba *hba, int state);
int ufshpb10_issue_req_dev_ctx(struct ufshpb10_lu *hpb, unsigned char *buf,
				int buf_length);
int ufshpb10_control_validation(struct ufs_hba *hba,
				struct ufshpb_config_desc *config);

extern u32 ufshpb_debug_mask;
enum hpb_log_level {
	HPB_LOG_LEVEL_OFF       = 0,
	HPB_LOG_LEVEL_ERR       = 1,
	HPB_LOG_LEVEL_INFO      = 2,
	HPB_LOG_LEVEL_DEBUG     = 3,
	HPB_LOG_LEVEL_HEX       = 4,
};
enum hpb_log_mask {
	HPB_LOG_OFF             = HPB_LOG_LEVEL_OFF,		/* 0 */
	HPB_LOG_ERR             = (1U << HPB_LOG_LEVEL_ERR),	/* 2 */
	HPB_LOG_INFO            = (1U << HPB_LOG_LEVEL_INFO),	/* 4 */
	HPB_LOG_DEBUG           = (1U << HPB_LOG_LEVEL_DEBUG),	/* 8 */
	HPB_LOG_HEX           	= (1U << HPB_LOG_LEVEL_HEX),	/* 16 */
};
#define HPB_DRIVER_E(fmt, args...)								\
	do {											\
		if(likely(ufshpb_debug_mask & HPB_LOG_ERR))                             	\
			pr_err("[HPB E][%s:%d] "	fmt, __func__, __LINE__, ##args);	\
	} while (0)

#define HPB_DRIVER_I(fmt, args...)								\
	do {											\
		if(unlikely(ufshpb_debug_mask & HPB_LOG_INFO))                             	\
			pr_err("[HPB][%s:%d] "	fmt, __func__, __LINE__, ##args);		\
	} while (0)

#define HPB_DRIVER_D(fmt, args...)								\
	do {											\
		if(unlikely(ufshpb_debug_mask & HPB_LOG_DEBUG))                             	\
			pr_err("[HPB][%s:%d] "	fmt, __func__, __LINE__, ##args);		\
	} while (0)

#define HPB_DRIVER_HEXDUMP(fmt, args...)							\
	do {											\
		if(unlikely(ufshpb_debug_mask & HPB_LOG_HEX)) {					\
			print_hex_dump(KERN_DEBUG, fmt, DUMP_PREFIX_ADDRESS, ##args);		\
		}                             							\
	} while (0)

#endif /* End of Header */
