// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2017-2018 Samsung Electronics Co., Ltd.
 * Modified work Copyright (C) 2018, Google, Inc.
 * Modified work Copyright (C) 2019 SK hynix
 */

#ifndef _SKHPB_H_
#define _SKHPB_H_

#include <linux/spinlock.h>
#include <linux/circ_buf.h>
#include <linux/workqueue.h>

/* Version info*/
#define SKHPB_DD_VER				0x010508

/* This quirk makes HPB driver always works as Devie Control Mode.
 * To cover old Configuration descriptor format which interpret
 * the bHPBControl field as RESERVED. */
#define SKHPB_QUIRK_ALWAYS_DEVICE_CONTROL_MODE (1 << 1)

/* Discard SubRegion activation hint information that has been processed,
 * when the host enters RPM/SPM sleep.
 * Must not be set the bit in ufs_quirks.h.*/
#define SKHPB_QUIRK_PURGE_HINT_INFO_WHEN_SLEEP (1 << 20)

/* Constant value*/
#define SKHPB_SECTOR					512
#define SKHPB_BLOCK					4096
#define SKHPB_SECTORS_PER_BLOCK			(SKHPB_BLOCK / SKHPB_SECTOR)
#define SKHPB_BITS_PER_DWORD				32
#define SKHPB_MAX_ACTIVE_NUM				2
#define SKHPB_MAX_INACTIVE_NUM			2

#define SKHPB_ENTRY_SIZE				0x08
#define SKHPB_ENTREIS_PER_OS_PAGE			(PAGE_SIZE / SKHPB_ENTRY_SIZE)

/* Description */
#define SKHPB_UFS_FEATURE_SUPPORT_HPB_BIT			0x80

#define SKHPB_QUERY_DESC_DEVICE_MAX_SIZE		0x43
#define SKHPB_QUERY_DESC_CONFIGURAION_MAX_SIZE		0xE6
#define SKHPB_QUERY_DESC_UNIT_MAX_SIZE			0x29
#define SKHPB_QUERY_DESC_GEOMETRY_MAX_SIZE		0x4D

/* Configuration for HPB */
#define SKHPB_CONF_LU_ENABLE			0x00
#define SKHPB_CONF_ACTIVE_REGIONS		0x10
#define SKHPB_CONF_PINNED_START		0x12
#define SKHPB_CONF_PINNED_NUM			0x14

/* Parameter Macros */
#define SKHPB_DEV(h)	((h)->hba->dev)
#define SKHPB_MAX_BVEC_SIZE	128

/* Use for HPB activate */
#define SKHPB_CONFIG_LEN	0xd0

#define SKHPB_READ_LARGE_CHUNK_SUPPORT
#define SKHPB_READ_LARGE_CHUNK_MAX_BLOCK_COUNT (128) //TRANSFER LENGTH: 8bit

typedef u64 skhpb_t;

enum skhpb_lu_set {
	LU_DISABLE	= 0x00,
	LU_ENABLE	= 0x01,
	LU_HPB_ENABLE	= 0x02,
	LU_SET_MAX,
};

struct skhpb_config_desc {
	unsigned char conf_dev_desc[16];
	unsigned char unit[UFS_UPIU_MAX_GENERAL_LUN][24];
};

/* Response UPIU types */
#define SKHPB_RSP_NONE					0x00
#define SKHPB_RSP_REQ_REGION_UPDATE			0x01
#define SKHPB_RSP_HPB_RESET				0x02
#define SKHPB_PER_ACTIVE_INFO_BYTES				4
#define SKHPB_PER_INACTIVE_INFO_BYTES				2

/* Vender defined OPCODE */
#define SKHPB_READ				  		0xF8
#define SKHPB_READ_BUFFER				0xF9
#define SKHPB_WRITE_BUFFER				0xFA

#define SKHPB_DEV_DATA_SEG_LEN				0x14
#define SKHPB_DEV_SENSE_SEG_LEN				0x12
#define SKHPB_DEV_DES_TYPE					0x80
#define SKHPB_DEV_ADDITIONAL_LEN				0x10

/* BYTE SHIFT */
#define SKHPB_ZERO_BYTE_SHIFT					0
#define SKHPB_ONE_BYTE_SHIFT					8
#define SKHPB_TWO_BYTE_SHIFT					16
#define SKHPB_THREE_BYTE_SHIFT				24

#define SKHPB_SHIFT_BYTE_0(num)		((num) << SKHPB_ZERO_BYTE_SHIFT)
#define SKHPB_SHIFT_BYTE_1(num)		((num) << SKHPB_ONE_BYTE_SHIFT)

#define SKHPB_GET_BYTE_0(num)			(((num) >> SKHPB_ZERO_BYTE_SHIFT) & 0xff)
#define SKHPB_GET_BYTE_1(num)			(((num) >> SKHPB_ONE_BYTE_SHIFT) & 0xff)
#define SKHPB_GET_BYTE_2(num)			(((num) >> SKHPB_TWO_BYTE_SHIFT) & 0xff)
#define SKHPB_GET_BYTE_3(num)			(((num) >> SKHPB_THREE_BYTE_SHIFT) & 0xff)

#define REGION_UNIT_SIZE(bit_offset)		(0x01 << (bit_offset))

enum SKHPB_STATE {
	SKHPB_PRESENT = 1,
	SKHPB_NOT_SUPPORTED = -1,
	SKHPB_FAILED = -2,
	SKHPB_NEED_INIT = 0,
	SKHPB_RESET = -3,
};

enum SKHPB_BUFFER_MODE {
	R_BUFFER = 0,
	W_BUFFER = 1,
};

enum SKHPB_CMD {
	SKHPB_CMD_READ 	= 0,
	SKHPB_CMD_WRITE = 1,
	SKHPB_CMD_DISCARD = 2,
	SKHPB_CMD_OTHERS = 3,
};

enum SKHPB_REGION_STATE {
	SKHPB_REGION_INACTIVE,
	SKHPB_REGION_ACTIVE,
};

enum SKHPB_SUBREGION_STATE {
	SKHPB_SUBREGION_UNUSED,
	SKHPB_SUBREGION_DIRTY,
	SKHPB_SUBREGION_CLEAN,
	SKHPB_SUBREGION_ISSUED,
};

enum SKHPB_CONTROL_MODE {
	HOST_CTRL_MODE = 0,
	DEV_CTRL_MODE = 1,
};

enum SKHPB_RST_TIME {
	SKHPB_MAP_RSP_DISABLE = 0,
	SKHPB_MAP_RSP_ENABLE = 1,
};

struct skhpb_func_desc {
	/*** Device Descriptor ***/
	/* 06h bNumberLU */
	int lu_cnt;
	/* 10h wSpecVersion */
	u16 spec_ver;
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

struct skhpb_lu_desc {
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

struct skhpb_rsp_active_list {
	u16 region[SKHPB_MAX_ACTIVE_NUM];
	u16 subregion[SKHPB_MAX_ACTIVE_NUM];
};

struct skhpb_rsp_inactive_list {
	u16 region[SKHPB_MAX_INACTIVE_NUM];
};

struct skhpb_rsp_update_entry {
	unsigned int lpn;
	skhpb_t ppn;
};

struct skhpb_rsp_info {
	int type;
	int active_cnt;
	int inactive_cnt;
	struct skhpb_rsp_active_list active_list;
	struct skhpb_rsp_inactive_list inactive_list;

	__u64 RSP_start;
	__u64 RSP_tasklet_enter;

	struct list_head list_rsp_info;
};

struct skhpb_rsp_field {
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

struct skhpb_map_ctx {
	struct page **m_page;
	unsigned int *ppn_dirty;

	struct list_head list_table;
};

struct skhpb_subregion {
	struct skhpb_map_ctx *mctx;
	enum SKHPB_SUBREGION_STATE subregion_state;
	int region;
	int subregion;
	bool last;
	struct list_head list_subregion;
};

struct skhpb_region {
	struct skhpb_subregion *subregion_tbl;
	enum SKHPB_REGION_STATE region_state;
	bool is_pinned;
	int region;
	int subregion_count;
	/*below information is used by lru*/
	struct list_head list_region;
	int hit_count;
};

struct skhpb_map_req {
	struct skhpb_lu *hpb;
	struct skhpb_map_ctx *mctx;
	struct bio bio;
	struct bio *pbio;
	struct bio_vec bvec[SKHPB_MAX_BVEC_SIZE];
	void (*end_io)(struct request *rq, int err);
	void *end_io_data;
	int region;
	int subregion;
	int subregion_mem_size;
	int lun;
	int retry_cnt;

	/* for debug : RSP Profiling */
	__u64 RSP_start; // get the request from device
	__u64 RSP_issue; // issue scsi cmd
	__u64 RSP_end;	 // complete the request

	char sense[SCSI_SENSE_BUFFERSIZE];

	struct list_head list_map_req;
	int rwbuffer_flag;
};

enum SKHPB_SELECTION_TYPE {
	TYPE_LRU = 1,
	TYPE_LFU = 2,
};

struct skhpb_victim_select_info {
	int selection_type;
	struct list_head lru;
	int max_lru_active_count;	// supported hpb #region - pinned #region
	atomic64_t active_count;
};

struct skhpb_lu {
	struct skhpb_region *region_tbl;
	struct skhpb_rsp_info *rsp_info;
	struct skhpb_map_req *map_req;

	struct list_head lh_map_ctx;
	struct list_head lh_subregion_req;
	struct list_head lh_rsp_info;

	struct list_head lh_rsp_info_free;
	struct list_head lh_map_req_free;
	struct list_head lh_map_req_retry;
	int debug_free_table;

	bool lu_hpb_enable;

	struct delayed_work skhpb_pinned_work;
	struct delayed_work skhpb_map_req_retry_work;
	struct work_struct skhpb_rsp_work;
	struct bio_vec bvec[SKHPB_MAX_BVEC_SIZE];

	int subregions_per_lu;
	int regions_per_lu;
	int subregion_mem_size;
	int last_subregion_mem_size;

	/* for selecting victim */
	struct skhpb_victim_select_info lru_info;

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
	bool identical_size;

#define BITS_PER_PPN_DIRTY (BITS_PER_BYTE * sizeof(unsigned int))
	int ppn_dirties_per_subregion;

	int mpage_bytes;
	int mpages_per_subregion;

	/* for debug constant variables */
	unsigned long long lu_num_blocks;

	u8 lun;

	struct ufs_hba *hba;

	spinlock_t hpb_lock;
	spinlock_t rsp_list_lock;
	spinlock_t map_list_lock;

	struct kobject kobj;
	struct mutex sysfs_lock;
	struct skhpb_sysfs_entry *sysfs_entries;

	bool hpb_control_mode;

	/* for debug */
	bool force_hpb_read_disable;
	bool force_map_req_disable;
	bool read_buf_debug;
	atomic64_t hit;
	atomic64_t size_miss;
	atomic64_t region_miss;
	atomic64_t subregion_miss;
	atomic64_t entry_dirty_miss;
	atomic64_t rb_noti_cnt;
	atomic64_t rb_fail;
	atomic64_t reset_noti_cnt;
	atomic64_t w_map_req_cnt;
#if defined(SKHPB_READ_LARGE_CHUNK_SUPPORT)
	atomic64_t lc_entry_dirty_miss;
	atomic64_t lc_reg_subreg_miss;
	atomic64_t lc_hit;
#endif
	atomic64_t map_req_cnt;
	atomic64_t region_add;
	atomic64_t region_evict;
	atomic64_t canceled_resp;
	atomic64_t canceled_map_req;
	atomic64_t alloc_map_req_cnt;
};

struct skhpb_sysfs_entry {
	struct attribute    attr;
	ssize_t (*show)(struct skhpb_lu *hpb, char *buf);
	ssize_t (*store)(struct skhpb_lu *hpb, const char *, size_t);
};

struct ufshcd_lrb;

void ufshcd_init_hpb(struct ufs_hba *hba);
void skhpb_init_handler(struct work_struct *work);
void skhpb_prep_fn(struct ufs_hba *hba, struct ufshcd_lrb *lrbp);
void skhpb_rsp_upiu(struct ufs_hba *hba, struct ufshcd_lrb *lrbp);
void skhpb_suspend(struct ufs_hba *hba);
void skhpb_resume(struct ufs_hba *hba);
void skhpb_release(struct ufs_hba *hba, int state);
int skhpb_issue_req_dev_ctx(struct skhpb_lu *hpb, unsigned char *buf,
				int buf_length);
int skhpb_control_validation(struct ufs_hba *hba,
				struct skhpb_config_desc *config);

extern u32 skhpb_debug_mask;
extern int debug_map_req;
enum SKHPB_LOG_LEVEL {
	SKHPB_LOG_LEVEL_OFF       = 0,
	SKHPB_LOG_LEVEL_ERR       = 1,
	SKHPB_LOG_LEVEL_INFO      = 2,
	SKHPB_LOG_LEVEL_DEBUG     = 3,
	SKHPB_LOG_LEVEL_HEX       = 4,
};
enum SKHPB_LOG_MASK {
	SKHPB_LOG_OFF             = SKHPB_LOG_LEVEL_OFF,		/* 0 */
	SKHPB_LOG_ERR             = (1U << SKHPB_LOG_LEVEL_ERR),	/* 2 */
	SKHPB_LOG_INFO            = (1U << SKHPB_LOG_LEVEL_INFO),	/* 4 */
	SKHPB_LOG_DEBUG           = (1U << SKHPB_LOG_LEVEL_DEBUG),	/* 8 */
	SKHPB_LOG_HEX           	= (1U << SKHPB_LOG_LEVEL_HEX),	/* 16 */
};
#define SKHPB_DRIVER_E(fmt, args...)								\
	do {											\
		if (likely(skhpb_debug_mask & SKHPB_LOG_ERR))                             	\
			pr_err("[HPB E][%s:%d] "	fmt, __func__, __LINE__, ##args);	\
	} while (0)

#define SKHPB_DRIVER_I(fmt, args...)								\
	do {											\
		if (unlikely(skhpb_debug_mask & SKHPB_LOG_INFO))                             	\
			pr_err("[HPB][%s:%d] "	fmt, __func__, __LINE__, ##args);		\
	} while (0)

#define SKHPB_DRIVER_D(fmt, args...)								\
	do {											\
		if (unlikely(skhpb_debug_mask & SKHPB_LOG_DEBUG))                             	\
			printk(KERN_DEBUG "[HPB][%s:%d] "	fmt, __func__, __LINE__, ##args);		\
	} while (0)

#define SKHPB_DRIVER_HEXDUMP(fmt, args...)							\
	do {											\
		if (unlikely(skhpb_debug_mask & SKHPB_LOG_HEX)) {					\
			print_hex_dump(KERN_DEBUG, fmt, DUMP_PREFIX_ADDRESS, ##args);		\
		}                             							\
	} while (0)

#define SKHPB_MAP_REQ_TIME(map_req, val, print)							\
	do {											\
		if (unlikely(debug_map_req)) {							\
			val = ktime_to_us(ktime_get());						\
			if (print) {								\
				SKHPB_DRIVER_I("SKHPB COMPL BUFFER %d - %d\n",			\
						map_req->region, map_req->subregion);		\
				SKHPB_DRIVER_I("start~issue = %lluus, issue~end = %lluus\n",	\
						map_req->RSP_issue - map_req->RSP_start,	\
						map_req->RSP_end - map_req->RSP_issue);		\
			}									\
		}										\
	} while (0)

#define SKHPB_RSP_TIME(val)									\
	do {											\
		if (unlikely(debug_map_req)) {							\
			val = ktime_to_us(ktime_get());						\
		}										\
	} while (0)

#endif /* End of Header */
