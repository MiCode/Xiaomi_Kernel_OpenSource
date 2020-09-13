/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

/*
 * @file	mkt_mssv.c
 * @brief   Driver for MSSV
 *
 */

#define __MTK_MSSV_C__

/*
 *=============================================================
 * Include files
 *=============================================================
 */

/* system includes */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/uaccess.h>

#include <mt-plat/mtk_secure_api.h>
#include <mtk_udi_internal.h>

#ifdef CONFIG_OF
	#include <linux/cpu.h>
	#include <linux/cpu_pm.h>
	#include <linux/of.h>
	#include <linux/of_irq.h>
	#include <linux/of_address.h>
	#include <linux/of_fdt.h>
	#include <mt-plat/aee.h>
#endif

/*
 *=============================================================
 * LOG
 *=============================================================
 */
#define MSSV_TAG	 "[CPU][MSSV] "

#define mssv_emerg(fmt, args...)	pr_debug(MSSV_TAG fmt, ##args)
#define mssv_alert(fmt, args...)	pr_debug(MSSV_TAG fmt, ##args)
#define mssv_crit(fmt, args...)		pr_debug(MSSV_TAG fmt, ##args)
#define mssv_error(fmt, args...)	pr_debug(MSSV_TAG fmt, ##args)
#define mssv_warning(fmt, args...)	pr_debug(MSSV_TAG fmt, ##args)
#define mssv_notice(fmt, args...)	pr_debug(MSSV_TAG fmt, ##args)
#define mssv_info(fmt, args...)		pr_debug(MSSV_TAG fmt, ##args)


#define MCUCFG_BASE				(0x0C530000)
#define LCPU_PLL_DIVIDER_CFG     (MCUCFG_BASE + 0xa2a0)
#define BCPU_PLL_DIVIDER_CFG     (MCUCFG_BASE + 0xa2a4)
#define DSU_PLL_DIVIDER_CFG     (MCUCFG_BASE + 0xa2e0)

#define MP_PLLDIV_MUX1SEL_SHIFT_BITS    (9)

#define ARMPLL_LL_CON1     (0x1000C204)
#define ARMPLL_L_CON1      (0x1000C214)
#define CCIPLL_CON1		   (0x1000C294)

enum {
	CLKSQ, /* freq: 26M */
	ARMPLL,
	MAINPLL,
	UNIVPLL
};

/*
 *=============================================================
 * static Variable
 *=============================================================
 */

#ifdef CONFIG_OF
static unsigned int mssv_state;
#endif
static int mssv_test;
static int mssv_loop_count = 30000;
static int mssv_log_count = 100;
static int mssv_current_loop_count;
static int mssv_low_freq_us = 90;
static int mssv_high_freq_us = 10;
static int cur_clock_source = 1;
static int mssv_high_opp = -1;
static int mssv_low_opp = -1;
static int mssv_cur_opp;
static int mssv_opp_switch_test;

/*
 *=============================================================
 * global function
 *=============================================================
 */
int mssv_calc_new_opp_idx(int opp)
{
	if (mssv_opp_switch_test == 0)
		return opp;

	mssv_cur_opp = (mssv_cur_opp == mssv_high_opp) ?
		mssv_low_opp : mssv_high_opp;

	return mssv_cur_opp;
}

unsigned int cpumssv_get_state(void)
{
	return mssv_state;
}

/*
 *=============================================================
 * static function
 *=============================================================
 */
static void switch_pll(unsigned int pll, unsigned int reg)
{
	unsigned int val;

	if (pll == CLKSQ) {
		/* Switch to 26MHz. */
		val = udi_reg_read(reg);
		val &= ~(0x600);
		val |= (0x0 << 9);
		udi_reg_write(reg, val);
		return;
	}

	/* Switch back to ARMPLL, MAINPLL or UNVPLL. */
	val = udi_reg_read(reg);
	val &= ~(0x600);
	val |= (pll << MP_PLLDIV_MUX1SEL_SHIFT_BITS);
	udi_reg_write(reg, val);
}

/* 0: 26mhz, 1: armpll */
static void switch_clock_source_v2(unsigned char clock_switch_mask, int source)
{
	// mssv_info("Current clock source = %s\n",
	//	((cur_clock_source == 0) ? "26mhz" : "armpll"));

	if (cur_clock_source == source)
		return;

	if (clock_switch_mask & (1 << 0)) { /* L (CA55) */
		switch_pll(source, LCPU_PLL_DIVIDER_CFG);
	}

	if (clock_switch_mask & (1 << 1)) { /* BIG (CA76) */
		switch_pll(source, BCPU_PLL_DIVIDER_CFG);
	}

	cur_clock_source = source;
}

static void test_26Mhz_to_max(int type)
{
	int sev = (type >> 4);
	unsigned char clock_switch_mask = (type & 0xF);

	/* switch to 26Mhz */
	//switch_clock_source_v2(clock_switch_mask, CLKSQ);

	if (sev == 1)
		asm volatile("sev" : : : "memory");

	mssv_test = 2;
	mssv_current_loop_count = 0;
	udelay(100);

	while (mssv_current_loop_count <= mssv_loop_count) {
		if ((mssv_current_loop_count % mssv_log_count) == 1) {
			mssv_error("mssv current loop count = %d\n",
				  mssv_current_loop_count);
		}

		/* switch to 26Mhz */
		switch_clock_source_v2(clock_switch_mask, CLKSQ);

		udelay(mssv_low_freq_us);

		/* switch to armpll */
		switch_clock_source_v2(clock_switch_mask, ARMPLL);

		udelay(mssv_high_freq_us);
		mssv_current_loop_count++;
	}
	mssv_error("mssv current loop count = %d\n", mssv_current_loop_count);

	/* switch to 26Mhz */
	//switch_clock_source_v2(clock_switch_mask, CLKSQ);

	mssv_test = 1;	/* test pass */
}

static int mssv_test_proc_show(struct seq_file *m, void *v)
{
	if (mssv_test == 1)
		seq_puts(m, "pass\n");
	else if (mssv_test == 0)
		seq_puts(m, "no test yet\n");
	else if (mssv_test == 2)
		seq_puts(m, "test in progress\n");

	// seq_printf(m, "big cpu wfe status=0x%x\n",
	//	read_big_cpu_wfe_status());
	seq_printf(m, "mssv_cur_opp = %d\n", mssv_cur_opp);
	seq_printf(m, "mssv_opp_switch_test = %d\n", mssv_opp_switch_test);

	return 0;
}

static ssize_t mssv_test_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int test = 0;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (kstrtoint(buf, 16, &test)) {
		mssv_info("bad argument!! Should be 0x\"01\" or 0x\"11\"\n");
		goto out;
	}

	switch (test >> 4) {
	case 0:
	case 1:
		test_26Mhz_to_max(test);
		break;
	default:
		mssv_info("wrong test %d\n", test);
	}

out:
	free_page((unsigned long)buf);

	return count;
}

static int mssv_log_count_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "log_count = %d\n", mssv_log_count);

	return 0;
}

static ssize_t mssv_log_count_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int test = 0;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (kstrtoint(buf, 10, &test)) {
		mssv_info("bad argument!! Should be an integer\n");
		goto out;
	}

	mssv_log_count = test;

out:
	free_page((unsigned long)buf);

	return count;
}

static int mssv_loop_count_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "mssv_loop_count = %d\n", mssv_loop_count);

	return 0;
}

static ssize_t mssv_loop_count_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int test = 0;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (kstrtoint(buf, 10, &test)) {
		mssv_info("bad argument!! Should be an integer\n");
		goto out;
	}

	mssv_loop_count = test;

out:
	free_page((unsigned long)buf);

	return count;
}

static int mssv_current_loop_count_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "current_loop_count = %d\n", mssv_current_loop_count);

	return 0;
}

static int mssv_low_freq_us_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "low freq us = %d\n", mssv_low_freq_us);

	return 0;
}

static ssize_t mssv_low_freq_us_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int test = 0;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (kstrtoint(buf, 10, &test)) {
		mssv_info("bad argument!! Should be an integer\n");
		goto out;
	}

	mssv_low_freq_us = test;

out:
	free_page((unsigned long)buf);

	return count;
}

static int mssv_high_freq_us_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "high freq us = %d\n", mssv_high_freq_us);

	return 0;
}

static ssize_t mssv_high_freq_us_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int test = 0;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (kstrtoint(buf, 10, &test)) {
		mssv_info("bad argument!! Should be an integer\n");
		goto out;
	}

	mssv_high_freq_us = test;

out:
	free_page((unsigned long)buf);

	return count;
}

//extern unsigned int mt_get_abist_freq(unsigned int ID);

static int mssv_clock_source_proc_show(struct seq_file *m, void *v)
{
	unsigned int value;

	seq_printf(m, "clock source = %s\n",
		((cur_clock_source == 0) ? "26mhz" : "armpll"));

	value = udi_reg_read(LCPU_PLL_DIVIDER_CFG);
	seq_printf(m, "4L reg [%x] = 0x%x\n", LCPU_PLL_DIVIDER_CFG, value);

	value = udi_reg_read(BCPU_PLL_DIVIDER_CFG);
	seq_printf(m, "4B reg [%x] = 0x%x\n", BCPU_PLL_DIVIDER_CFG, value);

//	seq_printf(m, "4L freq(armpll) = %d khz.\n", mt_get_abist_freq(22));
//	seq_printf(m, "4B freq(armpll) = %d khz.\n", mt_get_abist_freq(20));
//	seq_printf(m, "DSU freq(armpll) = %d khz.\n", mt_get_abist_freq(49));

	return 0;
}

static ssize_t mssv_clock_source_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int source = 0;
	unsigned char clock_switch_mask;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (sscanf(buf, "%c %d", &clock_switch_mask, &source) != 2) {
		mssv_info("bad argument!! Should be <clock_switch_mask> <0/1>\n");
		goto out;
	}

	switch_clock_source_v2(clock_switch_mask, source);

out:
	free_page((unsigned long)buf);

	return count;
}

static int mssv_high_opp_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "high opp = %d\n", mssv_high_opp);

	return 0;
}

static ssize_t mssv_high_opp_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int opp = 0;

	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (kstrtoint(buf, 10, &opp)) {
		mssv_info("bad argument!! Should be \"0\" ~ \"15\"\n");
		goto out;
	}

	mssv_high_opp = opp;

out:
	free_page((unsigned long)buf);

	return count;
}

static int mssv_low_opp_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "low opp = %d\n", mssv_low_opp);

	return 0;
}

static ssize_t mssv_low_opp_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int opp = 0;

	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (kstrtoint(buf, 10, &opp)) {
		mssv_info("bad argument!! Should be \"0\" ~ \"15\"\n");
		goto out;
	}

	mssv_low_opp = opp;

out:
	free_page((unsigned long)buf);

	return count;
}

#define PROC_FOPS_RW(name)					\
	static int name ## _proc_open(struct inode *inode,	\
		struct file *file)				\
	{							\
		return single_open(file, name ## _proc_show,	\
			PDE_DATA(inode));			\
	}							\
	static const struct file_operations name ## _proc_fops = {\
		.owner		  = THIS_MODULE,		\
		.open		   = name ## _proc_open,	\
		.read		   = seq_read,			\
		.llseek		 = seq_lseek,			\
		.release		= single_release,	\
		.write		  = name ## _proc_write,	\
	}

#define PROC_FOPS_RO(name)					\
	static int name ## _proc_open(struct inode *inode,	\
		struct file *file)				\
	{							\
		return single_open(file, name ## _proc_show,	\
			PDE_DATA(inode));			\
	}							\
	static const struct file_operations name ## _proc_fops = {\
		.owner		  = THIS_MODULE,		\
		.open		   = name ## _proc_open,	\
		.read		   = seq_read,			\
		.llseek		 = seq_lseek,			\
		.release		= single_release,	\
	}

#define PROC_ENTRY(name)	{__stringify(name), &name ## _proc_fops}

PROC_FOPS_RW(mssv_test);
PROC_FOPS_RW(mssv_log_count);
PROC_FOPS_RW(mssv_loop_count);
PROC_FOPS_RO(mssv_current_loop_count);
PROC_FOPS_RW(mssv_low_freq_us);
PROC_FOPS_RW(mssv_high_freq_us);
PROC_FOPS_RW(mssv_clock_source);
PROC_FOPS_RW(mssv_high_opp);
PROC_FOPS_RW(mssv_low_opp);

static int create_procfs(void)
{
	struct proc_dir_entry *mssv_dir = NULL;
	int i;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	struct pentry mssv_entries[] = {
		PROC_ENTRY(mssv_test),
		PROC_ENTRY(mssv_log_count),
		PROC_ENTRY(mssv_loop_count),
		PROC_ENTRY(mssv_current_loop_count),
		PROC_ENTRY(mssv_low_freq_us),
		PROC_ENTRY(mssv_high_freq_us),
		PROC_ENTRY(mssv_clock_source),
		PROC_ENTRY(mssv_high_opp),
		PROC_ENTRY(mssv_low_opp),
	};

	mssv_dir = proc_mkdir("mssv", NULL);
	if (!mssv_dir) {
		mssv_error("[%s]: mkdir /proc/mssv failed\n", __func__);
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(mssv_entries); i++) {
		if (!proc_create(mssv_entries[i].name, 0664,
				mssv_dir, mssv_entries[i].fops)) {
			mssv_error("[%s]: create /proc/mssv/%s failed\n",
				__func__, mssv_entries[i].name);
			return -3;
		}
	}
	return 0;
}

static int mssv_probe(struct platform_device *pdev)
{
#ifdef CONFIG_OF
	struct device_node *node = NULL;
	int rc = 0;

	node = pdev->dev.of_node;
	if (!node) {
		mssv_error("get MSSV device node err\n");
		return -ENODEV;
	}

	rc = of_property_read_u32(node, "state", &mssv_state);
	if (!rc) {
		mssv_error("state from DTree; rc(%d) state(0x%x)\n",
			rc, mssv_state);
	}
#endif
	mssv_info("MSSV probe OK! state(0x%x)\n", mssv_state);

	if (mssv_state == 0)
		return 0;

	create_procfs();

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mt_mssv_of_match[] = {
	{ .compatible = "mediatek,cpumssv", },
	{},
};
#endif

static struct platform_driver mssv_driver = {
	.remove		= NULL,
	.shutdown	= NULL,
	.probe		= mssv_probe,
	.suspend	= NULL,
	.resume		= NULL,
	.driver		= {
		.name   = "mt-mssv",
#ifdef CONFIG_OF
		.of_match_table = mt_mssv_of_match,
#endif
	},
};

static int __init mssv_init(void)
{
	int err = 0;

	err = platform_driver_register(&mssv_driver);
	if (err) {
		mssv_info("MSSV driver callback register failed..\n");
		return err;
	}

	return 0;
}

static void __exit mssv_exit(void)
{
	mssv_info("mssv de-initialization\n");
}

late_initcall(mssv_init);

MODULE_DESCRIPTION("MediaTek CPU MSSV Driver v1");
MODULE_LICENSE("GPL");

#undef __MTK_MSSV_C__
