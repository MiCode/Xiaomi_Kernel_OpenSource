/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __FREQHOPPING_DRV_H
#define __FREQHOPPING_DRV_H

#include <linux/proc_fs.h>
#include "mach/mtk_freqhopping.h"


/* move to /mediatek/platform/prj, can config. by prj. */
/* #define MEMPLL_SSC 0 */
/* #define MAINPLL_SSC 1 */

/* Export API */
int mt_freqhopping_devctl(unsigned int cmd, void *args);

struct mt_fh_hal_proc_func {
	int (*dumpregs_read)(struct seq_file *m, void *v);
	int (*dvfs_read)(struct seq_file *m, void *v);
	int (*dvfs_write)(struct file *file, const char *buffer,
		unsigned long count, void *data);

};

struct mt_fh_hal_driver {
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	void (*fh_pll_set)(int pll_id, int field, int value);
	int (*fh_pll_get)(int pll_id, int field);
#endif
	struct fh_pll_t *fh_pll;
	struct freqhopping_ssc *fh_usrdef;
	unsigned int mempll;
	unsigned int lvdspll;
	unsigned int mainpll;
	unsigned int msdcpll;
	unsigned int mmpll;
	unsigned int vencpll;
	unsigned int pll_cnt;

	struct mt_fh_hal_proc_func proc;

	void (*mt_fh_hal_init)(void);
	int (*mt_fh_hal_ctrl)(struct freqhopping_ioctl *fh_ctl, bool enable);
	void (*mt_fh_lock)(unsigned long *flags);
	void (*mt_fh_unlock)(unsigned long *flags);
	int (*mt_fh_get_init)(void);
	void (*mt_fh_popod_restore)(void);
	void (*mt_fh_popod_save)(void);

	int (*mt_l2h_mempll)(void);
	int (*mt_h2l_mempll)(void);
	int (*mt_dfs_armpll)(unsigned int coreid, unsigned int dds);
	int (*mt_dfs_mmpll)(unsigned int target_dds);
	int (*mt_dfs_vencpll)(unsigned int target_dds);
	int (*mt_dfs_mpll)(unsigned int target_dds);
	int (*mt_dfs_mempll)(unsigned int target_dds);
	int (*mt_is_support_DFS_mode)(void);
	int (*mt_l2h_dvfs_mempll)(void);
	int (*mt_h2l_dvfs_mempll)(void);
	int (*mt_dram_overclock)(int clk);
	int (*mt_get_dramc)(void);
	void (*mt_fh_default_conf)(void);
	int (*mt_dfs_general_pll)(unsigned int pll_id, unsigned int target_dds);
	void (*ioctl)(unsigned int ctlid, void *arg);
};

/* define ctlid for ioctl() */
#define FH_IO_PROC_READ 0x001

enum FH_DEVCTL_CMD_ID {
	FH_DCTL_CMD_ID = 0x1000,
	FH_DCTL_CMD_DVFS = 0x1001,
	FH_DCTL_CMD_DVFS_SSC_ENABLE = 0x1002,
	FH_DCTL_CMD_DVFS_SSC_DISABLE = 0x1003,
	FH_DCTL_CMD_SSC_ENABLE = 0x1004,
	FH_DCTL_CMD_SSC_DISABLE = 0x1005,
	FH_DCTL_CMD_GENERAL_DFS = 0x1006,
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	FH_DCTL_CMD_ARM_DFS = 0x1007,
	FH_DCTL_CMD_MM_DFS = 0x1008,
	FH_DCTL_CMD_FH_CONFIG = 0x1009,
	FH_DCTL_CMD_SSC_TBL_CONFIG = 0x100A,
	FH_DCTL_CMD_GET_PLL_STRUCT = 0x100B,
	FH_DCTL_CMD_SET_PLL_STRUCT = 0x100C,
	FH_DCTL_CMD_GET_INIT_STATUS = 0x100D,
	FH_DCTL_CMD_PLL_PAUSE = 0x100E,
#endif
	FH_DCTL_CMD_MAX
};


/* define structure for correspoinding ctlid */
struct FH_IO_PROC_READ_T {
	struct seq_file *m;
	void *v;
	struct fh_pll_t *pll;
};

struct mt_fh_hal_driver *mt_get_fh_hal_drv(void);

#define FH_BUG_ON(x) \
do {    \
	if ((x)) \
		pr_notice("BUGON %s:%d %s:%d\n", \
		__func__, __LINE__, current->comm, current->pid); \
} while (0)

#endif
