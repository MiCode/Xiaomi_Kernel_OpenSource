/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#ifndef _UFS_MEDIATEK_DBG_H
#define _UFS_MEDIATEK_DBG_H

#if IS_ENABLED(CONFIG_SCSI_UFS_MEDIATEK_DBG)

#include <linux/device.h>
#include <linux/seq_file.h>
#include <linux/types.h>

/*
 * snprintf may return a value of size or "more" to indicate
 * that the output was truncated, thus be careful of "more"
 * case.
 */
#define SPREAD_PRINTF(buff, size, evt, fmt, args...) \
do { \
	if (buff && size && *(size)) { \
		unsigned long var = snprintf(*(buff), *(size), fmt, ##args); \
		if (var > 0) { \
			if (var > *(size)) \
				var = *(size); \
			*(size) -= var; \
			*(buff) += var; \
		} \
	} \
	if (evt) \
		seq_printf(evt, fmt, ##args); \
	if (!buff && !evt) { \
		pr_info(fmt, ##args); \
	} \
} while (0)

enum ufsdbg_cmd_type {
	UFSDBG_CMD_LIST_DUMP    = 0,
	UFSDBG_PWR_MODE_DUMP    = 1,
	UFSDBG_HEALTH_DUMP      = 2,
	UFSDBG_CMD_LIST_ENABLE  = 3,
	UFSDBG_CMD_LIST_DISABLE = 4,
	UFSDBG_CMD_QOS_ON       = 5,
	UFSDBG_CMD_QOS_OFF      = 6,
	UFSDBG_UNKNOWN
};

enum ufsdbg_pm_state {
	UFSDBG_RUNTIME_SUSPEND,
	UFSDBG_RUNTIME_RESUME,
	UFSDBG_SYSTEM_SUSPEND,
	UFSDBG_SYSTEM_RESUME
};

/* clock scaling state  */
enum clk_scaling_state {
	CLKS_SCALE_DOWN,
	CLKS_SCALE_UP,
	CLKS_SCALE_UNKNOWN
};

enum cmd_hist_event {
	CMD_SEND		= 0,
	CMD_COMPLETED		= 1,
	CMD_DEV_SEND		= 2,
	CMD_DEV_COMPLETED	= 3,
	CMD_TM_SEND		= 4,
	CMD_TM_COMPLETED	= 5,
	CMD_TM_COMPLETED_ERR	= 6,
	CMD_UIC_SEND		= 7,
	CMD_UIC_CMPL_GENERAL	= 8,
	CMD_UIC_CMPL_PWR_CTRL	= 9,
	CMD_REG_TOGGLE		= 10,
	CMD_ABORTING		= 11,
	CMD_DI_FAIL		= 12,
	CMD_DEVICE_RESET	= 13,
	CMD_PERF_MODE		= 14,
	CMD_DEBUG_PROC		= 15,
	CMD_GENERIC		= 16,
	CMD_CLK_GATING		= 17,
	CMD_PM			= 18,
	CMD_CLK_SCALING		= 19,
	CMD_UNKNOWN
};

struct tm_cmd_struct {
	u8 lun;
	u8 tag;
	u8 task_tag;
	u16 tm_func;
};

struct utp_cmd_struct {
	u8 opcode;
	u8 crypt_en;
	u8 crypt_keyslot;
	u16 tag;
	u32 doorbell;
	u32 intr;
	int transfer_len;
	u64 lba;
};

struct dev_cmd_struct {
	u8 type;
	u8 opcode;
	u8 idn;
	u8 index;
	u8 selector;
	u16 tag;
};

struct uic_cmd_struct {
	u8 cmd;
	u32 arg1;
	u32 arg2;
	u32 arg3;
};

struct clk_gating_event_struct {
	u8 state;
};

struct clk_scaling_event_struct {
	u8 state;
	int err;
};

struct ufs_pm_struct {
	u8 state;
	int err;
	s64 time_us;
	int pwr_mode;
	int link_state;
};

struct cmd_hist_struct {
	u8 cpu;
	enum cmd_hist_event event;
	pid_t pid;
	u64 time;
	u64 duration;
	union {
		struct tm_cmd_struct tm;
		struct dev_cmd_struct dev;
		struct utp_cmd_struct utp;
		struct uic_cmd_struct uic;
		struct clk_gating_event_struct clk_gating;
		struct clk_scaling_event_struct clk_scaling;
		struct ufs_pm_struct pm;
	} cmd;
};

struct ufs_mtk_clk_scaling_attr {
	struct device_attribute downdifferential;
	struct device_attribute upthreshold;
};

int ufs_mtk_dbg_register(struct ufs_hba *hba);
void ufs_mtk_dbg_dump(u32 latest_cnt);
int ufs_mtk_dbg_cmd_hist_enable(void);
int ufs_mtk_dbg_cmd_hist_disable(void);
void ufs_mtk_init_clk_scaling_sysfs(struct ufs_hba *hba);
void ufs_mtk_remove_clk_scaling_sysfs(struct ufs_hba *hba);

#else

#define ufs_mtk_dbg_register(...)

#endif

#endif

