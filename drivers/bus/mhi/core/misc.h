/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 *
 */

#ifndef _MHI_CORE_MISC_H_
#define _MHI_CORE_MISC_H_

#include <linux/mhi_misc.h>

#define MHI_FORCE_WAKE_DELAY_US (100)
#define MHI_IPC_LOG_PAGES (100)
#define MAX_RDDM_TABLE_SIZE (6)
#define MHI_REG_SIZE (SZ_4K)

/* MHI misc capability registers */
#define MISC_OFFSET (0x24)
#define MISC_CAP_MASK (0xFFFFFFFF)
#define MISC_CAP_SHIFT (0)

#define CAP_CAPID_MASK (0xFF000000)
#define CAP_CAPID_SHIFT (24)
#define CAP_NEXT_CAP_MASK (0x00FFF000)
#define CAP_NEXT_CAP_SHIFT (12)

/* MHI Bandwidth scaling offsets */
#define BW_SCALE_CFG_OFFSET (0x04)
#define BW_SCALE_CFG_CHAN_DB_ID_MASK (0xFE000000)
#define BW_SCALE_CFG_CHAN_DB_ID_SHIFT (25)
#define BW_SCALE_CFG_ENABLED_MASK (0x01000000)
#define BW_SCALE_CFG_ENABLED_SHIFT (24)
#define BW_SCALE_CFG_ER_ID_MASK (0x00F80000)
#define BW_SCALE_CFG_ER_ID_SHIFT (19)

#define BW_SCALE_CAP_ID (3)
#define MHI_TRE_GET_EV_BW_REQ_SEQ(tre) (((tre)->dword[0] >> 8) & 0xFF)
#define MHI_BW_SCALE_CHAN_DB		126

#define MHI_BW_SCALE_SETUP(er_index) (((MHI_BW_SCALE_CHAN_DB << \
	BW_SCALE_CFG_CHAN_DB_ID_SHIFT) & BW_SCALE_CFG_CHAN_DB_ID_MASK) | \
	((1 << BW_SCALE_CFG_ENABLED_SHIFT) & BW_SCALE_CFG_ENABLED_MASK) | \
	(((er_index) << BW_SCALE_CFG_ER_ID_SHIFT) & BW_SCALE_CFG_ER_ID_MASK))

#define MHI_BW_SCALE_RESULT(status, seq) (((status) & 0xF) << 8 | \
					((seq) & 0xFF))
#define MHI_BW_SCALE_NACK 0xF

#define MHI_VERB(fmt, ...) do { \
	struct mhi_private *mhi_priv = \
		dev_get_drvdata(&mhi_cntrl->mhi_dev->dev); \
	dev_dbg(dev, "[D][%s] " fmt, __func__, ##__VA_ARGS__); \
	if (mhi_priv->log_lvl <= MHI_MSG_LVL_VERBOSE) \
		ipc_log_string(mhi_priv->log_buf, "[D][%s] " fmt, __func__, \
			       ##__VA_ARGS__); \
} while (0)

#define MHI_LOG(fmt, ...) do {	\
	struct mhi_private *mhi_priv = \
		dev_get_drvdata(&mhi_cntrl->mhi_dev->dev); \
	dev_dbg(dev, "[I][%s] " fmt, __func__, ##__VA_ARGS__); \
	if (mhi_priv->log_lvl <= MHI_MSG_LVL_INFO) \
		ipc_log_string(mhi_priv->log_buf, "[I][%s] " fmt, __func__, \
			       ##__VA_ARGS__); \
} while (0)

#define MHI_ERR(fmt, ...) do {	\
	struct mhi_private *mhi_priv = \
		dev_get_drvdata(&mhi_cntrl->mhi_dev->dev); \
	dev_err(dev, "[E][%s] " fmt, __func__, ##__VA_ARGS__); \
	if (mhi_priv->log_lvl <= MHI_MSG_LVL_ERROR) \
		ipc_log_string(mhi_priv->log_buf, "[E][%s] " fmt, __func__, \
			       ##__VA_ARGS__); \
} while (0)

#define MHI_CRITICAL(fmt, ...) do { \
	struct mhi_private *mhi_priv = \
		dev_get_drvdata(&mhi_cntrl->mhi_dev->dev); \
	dev_crit(dev, "[C][%s] " fmt, __func__, ##__VA_ARGS__); \
	if (mhi_priv->log_lvl <= MHI_MSG_LVL_CRITICAL) \
		ipc_log_string(mhi_priv->log_buf, "[C][%s] " fmt, __func__, \
			       ##__VA_ARGS__); \
} while (0)

/**
 * struct rddm_table_info - rddm table info
 * @base_address - Start offset of the file
 * @actual_phys_address - phys addr offset of file
 * @size - size of file
 * @description - file description
 * @file_name - name of file
 */
struct rddm_table_info {
	u64 base_address;
	u64 actual_phys_address;
	u64 size;
	char description[20];
	char file_name[20];
};

/**
 * struct rddm_header - rddm header
 * @version - header ver
 * @header_size - size of header
 * @rddm_table_info - array of rddm table info
 */
struct rddm_header {
	u32 version;
	u32 header_size;
	struct rddm_table_info table_info[MAX_RDDM_TABLE_SIZE];
};

/**
 * struct file_info - keeping track of file info while traversing the rddm
 * table header
 * @file_offset - current file offset
 * @seg_idx - mhi buf seg array index
 * @rem_seg_len - remaining length of the segment containing current file
 */
struct file_info {
	u8 *file_offset;
	u32 file_size;
	u32 seg_idx;
	u32 rem_seg_len;
};

/**
 * struct mhi_private - For private variables of an MHI controller
 */
struct mhi_private {
	struct list_head node;
	struct mhi_controller *mhi_cntrl;
	enum MHI_DEBUG_LEVEL log_lvl;
	void *log_buf;
	u32 saved_pm_state;
	enum mhi_state saved_dev_state;
	u32 m2_timeout_ms;
	void *priv_data;
	void __iomem *bw_scale_db;
	int (*bw_scale)(struct mhi_controller *mhi_cntrl,
			struct mhi_link_info *link_info);
	phys_addr_t base_addr;
};

/**
 * struct mhi_bus - For MHI controller debug
 */
struct mhi_bus {
	struct list_head controller_list;
	struct mutex lock;
};

#ifdef CONFIG_MHI_BUS_MISC
void mhi_misc_init(void);
void mhi_misc_exit(void);
int mhi_misc_init_mmio(struct mhi_controller *mhi_cntrl);
int mhi_misc_register_controller(struct mhi_controller *mhi_cntrl);
void mhi_misc_unregister_controller(struct mhi_controller *mhi_cntrl);
int mhi_process_misc_bw_ev_ring(struct mhi_controller *mhi_cntrl,
				struct mhi_event *mhi_event, u32 event_quota);
#else
static inline void mhi_misc_init(void)
{
}

static inline void mhi_misc_exit(void)
{
}

static inline int mhi_misc_init_mmio(struct mhi_controller *mhi_cntrl)
{
	return 0;
}

static inline int mhi_misc_register_controller(struct mhi_controller *mhi_cntrl)
{
	return 0;
}

static inline void mhi_misc_unregister_controller(struct mhi_controller
						  *mhi_cntrl)
{
}

static inline int mhi_process_misc_bw_ev_ring(struct mhi_controller *mhi_cntrl,
				struct mhi_event *mhi_event, u32 event_quota)
{
	return 0;
}
#endif

#endif /* _MHI_CORE_MISC_H_ */
