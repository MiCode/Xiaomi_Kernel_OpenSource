/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mtk_cpu_dbg.h - cpu dvfs debug driver common define
 *
 * Copyright (c) 2020 MediaTek Inc.
 * chienwei Chang <chienwei.chang@mediatek.com>
 */

#undef  BIT
#define BIT(bit)					(1U << (bit))
/* static void __iomem *cspm_base; */
#define csram_read(offs)	__raw_readl(csram_base + (offs))
#define csram_write(offs, val)	__raw_writel(val, csram_base + (offs))

#define TAG	"[Power/cpufreq] "
#define tag_pr_info(fmt, args...)	pr_info(TAG fmt, ##args)
#define tag_pr_notice(fmt, args...)	pr_notice(TAG fmt, ##args)

#define PROC_FOPS_RW(name)						\
	static int name ## _proc_open(struct inode *inode, struct file *file)\
{									\
	return single_open(file, name ## _proc_show, PDE_DATA(inode));	\
}									\
static const struct proc_ops name ## _proc_fops = {		\
	.proc_open           = name ## _proc_open,				\
	.proc_read           = seq_read,					\
	.proc_lseek         = seq_lseek,					\
	.proc_release        = single_release,				\
	.proc_write          = name ## _proc_write,				\
}

#define PROC_FOPS_RO(name)                                                     \
	static int name##_proc_open(struct inode *inode, struct file *file)    \
	{                                                                      \
		return single_open(file, name##_proc_show, PDE_DATA(inode));   \
	}                                                                      \
	static const struct proc_ops name##_proc_fops = {               \
		.proc_open = name##_proc_open,                                      \
		.proc_read = seq_read,                                              \
		.proc_lseek = seq_lseek,                                           \
		.proc_release = single_release,                                     \
	}

#define PROC_ENTRY(name)	{__stringify(name), &name ## _proc_fops}
#define PROC_ENTRY_DATA(name)	\
{__stringify(name), &name ## _proc_fops, g_ ## name}
#define LAST_LL_CORE	3
#define MAX_CLUSTER_NRS	4
#define _BITMASK_(_bits_)               \
(((unsigned int) -1 >> (31 - ((1) ? _bits_))) & ~((1U << ((0) ? _bits_)) - 1))
#define _GET_BITS_VAL_(_bits_, _val_)   \
(((_val_) & (_BITMASK_(_bits_))) >> ((0) ? _bits_))

#define OFFS_CCI_TBL_USER    0x0F94
#define OFFS_CCI_TOGGLE_BIT  0x0F98
#define OFFS_CCI_TBL_MODE 0x0F9C
#define OFFS_CCI_IDX      0x0550
enum dsu_user {
	DSU_CMD,
	SWPM,
	FPS_PERF,
};

struct pll_addr_offs {
	unsigned int armpll_con;
	unsigned int clkdiv_cfg;
};

struct pll_addr {
	unsigned int reg_addr[2];
};

enum {
	IPI_DVFS_INIT,
	IPI_SET_VOLT,
	IPI_SET_FREQ,
	IPI_GET_VOLT,
	IPI_GET_FREQ,

	NR_DVFS_IPI,
};

struct cdvfs_data {
	unsigned int cmd;
	union {
		struct {
			unsigned int arg[3];
		} set_fv;
	} u;
};

enum mcucfg_ver {
	MCUCFG_V0, /* armpll in apmixed */
	MCUCFG_V1, /* armpll in mcusys */
	MAX_MCUCFG_VERSION,
};

extern int mtk_eem_init(struct platform_device *pdev);

