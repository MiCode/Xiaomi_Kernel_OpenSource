/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __FREQHOPPING_DRV_H
#define __FREQHOPPING_DRV_H

#include <linux/proc_fs.h>
#include "mtk_freqhopping.h"

struct mt_fh_hal_driver {
#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) || \
	defined(CONFIG_MTK_TINYSYS_MCUPM_SUPPORT)
	void (*fh_pll_set)(int pll_id, int field, int value);
	int (*fh_pll_get)(int pll_id, int field);
#endif
	struct fh_pll_t *fh_pll;
	unsigned int pll_cnt;
	void (*mt_fh_lock)(unsigned long *flags);
	void (*mt_fh_unlock)(unsigned long *flags);
	int (*mt_fh_hal_init)(void);
	int (*mt_fh_get_init)(void);
	int (*mt_fh_hal_ctrl)(struct freqhopping_ioctl *fh_ctl, bool enable);
	void (*mt_fh_hal_default_conf)(void);
	int (*mt_dfs_armpll)(unsigned int coreid, unsigned int dds);
	int (*mt_fh_hal_dumpregs_read)(struct seq_file *m, void *v);
	int (*mt_fh_hal_slt_start)(void);
	int (*mt_dfs_general_pll)(enum FH_PLL_ID pll_id,
				unsigned int target_dds);
	void (*mt_fh_popod_restore)(void);
	void (*mt_fh_popod_save)(void);

	void (*ioctl)(unsigned int ctlid, void *arg);
};


#define FH_IO_PROC_READ 0x001

enum FH_DEVCTL_CMD_ID {
	FH_DCTL_CMD_ID = 0x1000,
	FH_DCTL_CMD_DVFS = 0x1001,
	FH_DCTL_CMD_DVFS_SSC_ENABLE = 0x1002,
	FH_DCTL_CMD_DVFS_SSC_DISABLE = 0x1003,
	FH_DCTL_CMD_SSC_ENABLE = 0x1004,
	FH_DCTL_CMD_SSC_DISABLE = 0x1005,
	FH_DCTL_CMD_GENERAL_DFS = 0x1006,
#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) || \
	defined(CONFIG_MTK_TINYSYS_MCUPM_SUPPORT)
	FH_DCTL_CMD_ARM_DFS = 0x1007,
	FH_DCTL_CMD_MM_DFS = 0x1008,
	FH_DCTL_CMD_FH_CONFIG = 0x1009,
	FH_DCTL_CMD_SSC_TBL_CONFIG = 0x100A,
	FH_DCTL_CMD_GET_PLL_STRUCT = 0x100B,
	FH_DCTL_CMD_SET_PLL_STRUCT = 0x100C,
	FH_DCTL_CMD_GET_INIT_STATUS = 0x100D,
	FH_DCTL_CMD_PLL_PAUSE = 0x100E,
#endif
	FH_DCTL_CMD_MAX,
	FH_DBG_CMD_TR_BEGIN_LOW = 0x2001,
	FH_DBG_CMD_TR_BEGIN_HIGH = 0x2002,
	FH_DBG_CMD_TR_END_LOW = 0x2003,
	FH_DBG_CMD_TR_END_HIGH = 0x2004,
	FH_DBG_CMD_TR_ID = 0x2005,
	FH_DBG_CMD_TR_VAL = 0x2006,
};


/* define structure for correspoinding ctlid */
struct FH_IO_PROC_READ_T {
	struct seq_file *m;
	void *v;
	struct fh_pll_t *pll;
};


int mt_freqhopping_devctl(unsigned int cmd, void *args);
int mt_dfs_armpll(unsigned int pll, unsigned int dds);
int mt_dfs_general_pll(unsigned int pll_id, unsigned int target_dds);
int freqhopping_config(unsigned int pll_id, unsigned long def_set_idx,
			unsigned int enable);
#endif
