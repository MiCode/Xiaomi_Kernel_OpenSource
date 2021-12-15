/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/smp.h>
/* #include <linux/earlysuspend.h> */
#include <linux/clk-provider.h>
#include <linux/io.h>

/* **** */
/* #include <mach/mt_typedefs.h> */
#include <mt-plat/sync_write.h>
/*#include "mt_clkmgr.h"*/
#include "mt6739_clkmgr.h"
/* #include <mach/mt_dcm.h> */
/* #include <mach/mt_idvfs.h> */ /* Fix when idvfs ready */
/*#include "mt_spm.h"*/
/*#include <mach/mt_spm_mtcmos.h>*/
/* #include <mach/mt_spm_sleep.h> */
/* #include <mach/mt_gpufreq.h> */
/* #include <mach/irqs.h> */

/* #include <mach/upmu_common.h> */
/* #include <mach/upmu_sw.h> */
/* #include <mach/upmu_hw.h> */
/*#include "mt_freqhopping_drv.h"*/

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#endif

#ifdef CONFIG_OF
void __iomem *clk_apmixed_base;
void __iomem *clk_mcucfg_base;
#endif


/************************************************
 **********         log debug          **********
 ************************************************/
#define TAG     "[Power/clkmgr] "

#define clk_err(fmt, args...)       \
	pr_info(TAG fmt, ##args)
#define clk_warn(fmt, args...)      \
	pr_info(TAG fmt, ##args)
#define clk_info(fmt, args...)      \
	pr_info(TAG fmt, ##args)
#define clk_dbg(fmt, args...)       \
	pr_info(TAG fmt, ##args)

/************************************************
 **********      register access       **********
 ************************************************/

#define clk_readl(addr) \
	readl(addr)
    /* DRV_Reg32(addr) */

#define clk_writel(addr, val)   \
	mt_reg_sync_writel(val, addr)

#define clk_setl(addr, val) \
	mt_reg_sync_writel(clk_readl(addr) | (val), addr)

#define clk_clrl(addr, val) \
	mt_reg_sync_writel(clk_readl(addr) & ~(val), addr)

/************************************************
 **********      struct definition     **********
 ************************************************/
/************************************************
 **********      global variablies     **********
 ************************************************/
/************************************************
 **********      spin lock protect     **********
 ************************************************/
/************************************************
 **********          pll part          **********
 ************************************************/
/************************************************
 **********         subsys part        **********
 ************************************************/
/*ARMPLL1*/
#define ARMPLL_LL_CON0           (clk_apmixed_base + 0x200)
#define ARMPLL_LL_CON1           (clk_apmixed_base + 0x204)
#define ARMPLL_LL_PWR_CON0       (clk_apmixed_base + 0x20C)
/*MMPLL*/
#define MMPLL_CON0           (clk_apmixed_base + 0x270)
#define MMPLL_CON1           (clk_apmixed_base + 0x274)
#define MMPLL_PWR_CON0       (clk_apmixed_base + 0x27C)
/*GPUPLL*/
#define GPUPLL_CON0           (clk_apmixed_base + 0x240)
#define GPUPLL_CON1           (clk_apmixed_base + 0x244)
#define GPUPLL_PWR_CON0       (clk_apmixed_base + 0x24C)

/* MCUCFG Register */
#if 0
#define MP0_PLL_DIV_CFG           (clk_mcucfg_base + 0x7A0) /*ARMPLL_M*/
#define MP1_PLL_DIV_CFG           (clk_mcucfg_base + 0x7A4) /*ARMPLL_L*/
#define MP2_PLL_DIV_CFG           (clk_mcucfg_base + 0x7A8) /*ARMPLL_B*/
#define BUS_PLL_DIV_CFG           (clk_mcucfg_base + 0x7C0) /*CCIPLL*/
#endif
/************************************************
 **********         cg_clk part        **********
 ************************************************/

/************************************************
 **********       initialization       **********
 ************************************************/

/************************************************
 **********       function debug       **********
 ************************************************/
#if 0
static void clk_dump(void)
{
	pr_info("[ARMPLL1_CON0]=0x%08x\n", clk_readl(ARMPLL1_CON0));
	pr_info("[ARMPLL1_CON1]=0x%08x\n", clk_readl(ARMPLL1_CON1));
	pr_info("[MP0_PLL_DIV_CFG]=0x%08x\n", clk_readl(MP0_PLL_DIV_CFG));
	pr_info("[CCF] ARMPLL1(M): %d\n", mt_get_abist_freq(35));
	pr_info("[ARMPLL2_CON0]=0x%08x\n", clk_readl(ARMPLL2_CON0));
	pr_info("[ARMPLL2_CON1]=0x%08x\n", clk_readl(ARMPLL2_CON1));
	pr_info("[MP1_PLL_DIV_CFG]=0x%08x\n", clk_readl(MP1_PLL_DIV_CFG));
	pr_info("[CCF] ARMPLL2(L): %d\n", mt_get_abist_freq(34));
	pr_info("[ARMPLL3_CON0]=0x%08x\n", clk_readl(ARMPLL3_CON0));
	pr_info("[ARMPLL3_CON1]=0x%08x\n", clk_readl(ARMPLL3_CON1));
	pr_info("[MP2_PLL_DIV_CFG]=0x%08x\n", clk_readl(MP2_PLL_DIV_CFG));
	pr_info("[CCF] ARMPLL3(B): %d\n", mt_get_abist_freq(36));
	pr_info("[CCIPLL_CON0]=0x%08x\n", clk_readl(CCIPLL_CON0));
	pr_info("[CCIPLL_CON1]=0x%08x\n", clk_readl(CCIPLL_CON1));
	pr_info("[BUS_PLL_DIV_CFG]=0x%08x\n", clk_readl(BUS_PLL_DIV_CFG));
	pr_info("[CCF] CCIPLL: %d\n", mt_get_abist_freq(11));

}

static int armpll1_fsel_read(struct seq_file *m, void *v)
{
	return 0;
}

static int armpll2_fsel_read(struct seq_file *m, void *v)
{
	return 0;
}

static int armpll3_fsel_read(struct seq_file *m, void *v)
{
	return 0;
}

static int ccipll_fsel_read(struct seq_file *m, void *v)
{
	return 0;
}
#endif
static int mmpll_fsel_read(struct seq_file *m, void *v)
{
	return 0;
}

static int gpupll_fsel_read(struct seq_file *m, void *v)
{
	return 0;
}
#if 0
static int pll_div_value_map(int index)
{
	int div = 0x08;

	switch (index) {
	case 1:
		div = 0x08;
	break;

	case 2:
		div = 0x0A;
	break;
	case 4:
		div = 0x0B;
	break;
	case 6:
		div = 0x1D;
	break;
	default:
		div = 0x08;
	break;
	}
	return div;
}
#define SDM_PLL_N_INFO_MASK 0x003FFFFF /*N_INFO[21:0]*/
#define ARMPLL_POSDIV_MASK  0x70000000 /*POSDIV[30:28]*/
#define SDM_PLL_N_INFO_CHG  0x80000000
#define ARMPLL_DIV_MASK	    0xFFE1FFFF
#define ARMPLL_DIV_SHIFT    17
static ssize_t armpll1_fsel_write(struct file *file, const char __user *buffer,
				   size_t count, loff_t *data)
{
	char desc[32];
	int len = 0;
	unsigned int ctrl_value = 0;
	int div;
	unsigned int value = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (sscanf(desc, "%x %x", &div, &value) == 2) {
		clk_dump();
		ctrl_value = clk_readl(ARMPLL1_CON1);
		ctrl_value &= ~(SDM_PLL_N_INFO_MASK | ARMPLL_POSDIV_MASK);
		ctrl_value |= value & (SDM_PLL_N_INFO_MASK | ARMPLL_POSDIV_MASK);
		ctrl_value |= SDM_PLL_N_INFO_CHG;
		clk_writel(ARMPLL1_CON1, ctrl_value);
		udelay(20);
		ctrl_value = clk_readl(MP0_PLL_DIV_CFG);
		ctrl_value &= ARMPLL_DIV_MASK;
		ctrl_value |= (pll_div_value_map(div) << ARMPLL_DIV_SHIFT);
		clk_writel(MP0_PLL_DIV_CFG, ctrl_value);
	}
	clk_dump();
	return count;
}

static ssize_t armpll2_fsel_write(struct file *file, const char __user *buffer,
				  size_t count, loff_t *data)
{
	char desc[32];
	int len = 0;
	unsigned int ctrl_value = 0;
	int div;
	unsigned int value;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (sscanf(desc, "%x %x", &div, &value) == 2) {
		clk_dump();
		ctrl_value = clk_readl(ARMPLL2_CON1);
		ctrl_value &= ~(SDM_PLL_N_INFO_MASK | ARMPLL_POSDIV_MASK);
		ctrl_value |= value & (SDM_PLL_N_INFO_MASK | ARMPLL_POSDIV_MASK);
		ctrl_value |= SDM_PLL_N_INFO_CHG;
		clk_writel(ARMPLL2_CON1, ctrl_value);
		udelay(20);
		ctrl_value = clk_readl(MP1_PLL_DIV_CFG);
		ctrl_value &= ARMPLL_DIV_MASK;
		ctrl_value |= (pll_div_value_map(div) << ARMPLL_DIV_SHIFT);
		clk_writel(MP1_PLL_DIV_CFG, ctrl_value);
	}
	clk_dump();
	return count;
}

static ssize_t armpll3_fsel_write(struct file *file, const char __user *buffer,
				    size_t count, loff_t *data)
{
	char desc[32];
	int len = 0;
	unsigned int ctrl_value = 0;
	int div;
	unsigned int value;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (sscanf(desc, "%x %x", &div, &value) == 2) {
		clk_dump();
		ctrl_value = clk_readl(ARMPLL3_CON1);
		ctrl_value &= ~(SDM_PLL_N_INFO_MASK | ARMPLL_POSDIV_MASK);
		ctrl_value |= value & (SDM_PLL_N_INFO_MASK | ARMPLL_POSDIV_MASK);
		ctrl_value |= SDM_PLL_N_INFO_CHG;
		clk_writel(ARMPLL3_CON1, ctrl_value);
		udelay(20);
		ctrl_value = clk_readl(MP2_PLL_DIV_CFG);
		ctrl_value &= ARMPLL_DIV_MASK;
		ctrl_value |= (pll_div_value_map(div) << ARMPLL_DIV_SHIFT);
		clk_writel(MP2_PLL_DIV_CFG, ctrl_value);
	}
	clk_dump();
	return count;

}

static ssize_t ccipll_fsel_write(struct file *file, const char __user *buffer,
				    size_t count, loff_t *data)
{
	char desc[32];
	int len = 0;
	unsigned int ctrl_value = 0;
	int div;
	unsigned int value;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (sscanf(desc, "%x %x", &div, &value) == 2) {
		clk_dump();
		ctrl_value = clk_readl(CCIPLL_CON1);
		ctrl_value &= ~(SDM_PLL_N_INFO_MASK | ARMPLL_POSDIV_MASK);
		ctrl_value |= value & (SDM_PLL_N_INFO_MASK | ARMPLL_POSDIV_MASK);
		ctrl_value |= SDM_PLL_N_INFO_CHG;
		clk_writel(CCIPLL_CON1, ctrl_value);
		udelay(20);
		ctrl_value = clk_readl(BUS_PLL_DIV_CFG);
		ctrl_value &= ARMPLL_DIV_MASK;
		ctrl_value |= (pll_div_value_map(div) << ARMPLL_DIV_SHIFT);
		clk_writel(BUS_PLL_DIV_CFG, ctrl_value);
	}
	clk_dump();
	return count;

}
#endif
static ssize_t mmpll_fsel_write(struct file *file, const char __user *buffer,
				    size_t count, loff_t *data)
{
		char desc[32];
		int len = 0;
		unsigned int con0_value, con1_value;

		len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
			if (copy_from_user(desc, buffer, len))
				return 0;
		desc[len] = '\0';
		if (sscanf(desc, "%x %x", &con1_value, &con0_value) == 2) {
			clk_writel(MMPLL_CON1, con1_value);
			clk_writel(MMPLL_CON0, con0_value);
			udelay(20);
		}
		return count;
}

static ssize_t gpupll_fsel_write(struct file *file, const char __user *buffer,
				    size_t count, loff_t *data)
{
		char desc[32];
		int len = 0;
		unsigned int con0_value, con1_value;

		len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
			if (copy_from_user(desc, buffer, len))
				return 0;
		desc[len] = '\0';
		if (sscanf(desc, "%x %x", &con1_value, &con0_value) == 2) {
			clk_writel(GPUPLL_CON1, con1_value);
			clk_writel(GPUPLL_CON0, con0_value);
			udelay(20);
		}
		return count;
}

static int mm_clk_speed_dump_read(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", mt_get_abist_freq(34));
	return 0;
}

static int gpupll_speed_dump_read(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", mt_get_abist_freq(27));
	return 0;
}

static int univpll_speed_dump_read(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", mt_get_abist_freq(24));
	return 0;
}
#if 0
/************ L ********************/
static int proc_armpll1_fsel_open(struct inode *inode, struct file *file)
{
	clk_err("%s", __func__);

	return single_open(file, armpll1_fsel_read, NULL);
}

static const struct file_operations armpll1_fsel_proc_fops = {
	.owner = THIS_MODULE,
	.open = proc_armpll1_fsel_open,
	.read = seq_read,
	.write = armpll1_fsel_write,
	.release = single_release,
};
/************ LL ********************/
static int proc_armpll2_fsel_open(struct inode *inode, struct file *file)
{
	clk_err("%s", __func__);

	return single_open(file, armpll2_fsel_read, NULL);
}

static const struct file_operations armpll2_fsel_proc_fops = {
	.owner = THIS_MODULE,
	.open = proc_armpll2_fsel_open,
	.read = seq_read,
	.write = armpll2_fsel_write,
	.release = single_release,
};
/************ BIG ********************/
static int proc_armpll3_fsel_open(struct inode *inode, struct file *file)
{
	clk_err("%s", __func__);

	return single_open(file, armpll3_fsel_read, NULL);
}

static const struct file_operations armpll3_fsel_proc_fops = {
	.owner = THIS_MODULE,
	.open = proc_armpll3_fsel_open,
	.read = seq_read,
	.write = armpll3_fsel_write,
	.release = single_release,
};
/************ CCI ********************/
static int proc_ccipll_fsel_open(struct inode *inode, struct file *file)
{
	clk_err("%s", __func__);

	return single_open(file, ccipll_fsel_read, NULL);
}

static const struct file_operations ccipll_fsel_proc_fops = {
	.owner = THIS_MODULE,
	.open = proc_ccipll_fsel_open,
	.read = seq_read,
	.write = ccipll_fsel_write,
	.release = single_release,
};
#endif
/************ MM ********************/
static int proc_mmpll_fsel_open(struct inode *inode, struct file *file)
{
	clk_err("%s", __func__);

	return single_open(file, mmpll_fsel_read, NULL);
}

static const struct file_operations mmpll_fsel_proc_fops = {
	.owner = THIS_MODULE,
	.open = proc_mmpll_fsel_open,
	.read = seq_read,
	.write = mmpll_fsel_write,
	.release = single_release,
};
/************ GPU ********************/
static int proc_gpupll_fsel_open(struct inode *inode, struct file *file)
{
	clk_err("%s", __func__);

	return single_open(file, gpupll_fsel_read, NULL);
}

static const struct file_operations gpupll_fsel_proc_fops = {
	.owner = THIS_MODULE,
	.open = proc_gpupll_fsel_open,
	.read = seq_read,
	.write = gpupll_fsel_write,
	.release = single_release,
};
/************ mm_clk ********************/
static int proc_mm_clk_open(struct inode *inode, struct file *file)
{
	return single_open(file, mm_clk_speed_dump_read, NULL);
}

static const struct file_operations mm_fops = {
	.owner = THIS_MODULE,
	.open = proc_mm_clk_open,
	.read = seq_read,
};
/************ gpupll ********************/
static int proc_gpupll_open(struct inode *inode, struct file *file)
{
	return single_open(file, gpupll_speed_dump_read, NULL);
}

static const struct file_operations gpu_fops = {
	.owner = THIS_MODULE,
	.open = proc_gpupll_open,
	.read = seq_read,
};
/************ univpll ********************/
static int proc_univpll_open(struct inode *inode, struct file *file)
{
	return single_open(file, univpll_speed_dump_read, NULL);
}

static const struct file_operations univ_fops = {
	.owner = THIS_MODULE,
	.open = proc_univpll_open,
	.read = seq_read,
};

void mt_clkmgr_debug_init(void)
{
/*use proc_create*/
	struct proc_dir_entry *entry;
	struct proc_dir_entry *clkmgr_dir;

	clkmgr_dir = proc_mkdir("clkmgr", NULL);
	if (!clkmgr_dir) {
		clk_err("[%s]: fail to mkdir /proc/clkmgr\n", __func__);
		return;
	}
#if 0
	entry =
	    proc_create("armpll1_fsel", S_IRUGO | S_IWUSR | S_IWGRP, clkmgr_dir,
			&armpll1_fsel_proc_fops);
	entry =
	    proc_create("armpll2_fsel", S_IRUGO | S_IWUSR | S_IWGRP, clkmgr_dir,
			&armpll2_fsel_proc_fops);
	entry =
	    proc_create("armpll3_fsel", S_IRUGO | S_IWUSR | S_IWGRP, clkmgr_dir,
			&armpll3_fsel_proc_fops);
	entry =
	    proc_create("ccipll_fsel", S_IRUGO | S_IWUSR | S_IWGRP, clkmgr_dir,
			&ccipll_fsel_proc_fops);
#endif
	entry =
	    proc_create("mmpll_fsel", S_IRUGO | S_IWUSR | S_IWGRP, clkmgr_dir,
			&mmpll_fsel_proc_fops);
	entry =
	    proc_create("gpupll_fsel", S_IRUGO | S_IWUSR | S_IWGRP, clkmgr_dir,
			&gpupll_fsel_proc_fops);
	entry =
	    proc_create("mm_speed_dump", S_IRUGO, clkmgr_dir,
			&mm_fops);
	entry =
	    proc_create("gpu_speed_dump", S_IRUGO, clkmgr_dir,
			&gpu_fops);
	entry =
	    proc_create("univpll_speed_dump", S_IRUGO, clkmgr_dir,
			&univ_fops);
}

/*move to other place*/
int univpll_is_used(void)
{
	/*
	* 0: univpll is not used, sspm can disable
	* 1: univpll is used, sspm cannot disable
	*/
	struct clk *c = __clk_lookup("univpll");

	return __clk_get_enable_count(c);
}

#ifdef CONFIG_OF
void iomap(void)
{
	struct device_node *node;

/*apmixed*/
	node = of_find_compatible_node(NULL, NULL, "mediatek,apmixed");
	if (!node)
		pr_info("[CLK_APMIXED] find node failed\n");
	clk_apmixed_base = of_iomap(node, 0);
	if (!clk_apmixed_base)
		pr_info("[CLK_APMIXED] base failed\n");
#if 0
/*mcucfg*/
	node = of_find_compatible_node(NULL, NULL, "mediatek,mcucfg");
	if (!node)
		pr_info("[CLK_MCUCFG] find node failed\n");
	clk_mcucfg_base = of_iomap(node, 0);
	if (!clk_mcucfg_base)
		pr_info("[CLK_MCUCFG] base failed\n");
#endif
}
#endif


static int mt_clkmgr_debug_module_init(void)
{
	iomap();
	mt_clkmgr_debug_init();
	return 0;
}

module_init(mt_clkmgr_debug_module_init);
