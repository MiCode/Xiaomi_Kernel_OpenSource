// SPDX-License-Identifier: GPL-2.0
/*
 * cpufreq-dbg.c - CPUFreq debug Driver
 *
 * Copyright (c) 2020 MediaTek Inc.
 * Chienwei Chang <chienwei.chang@mediatek.com>
 */

/* system includes */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/cpu.h>
#include <linux/io.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/init.h>
#include <linux/kobject.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <linux/cpufreq.h>
#include <linux/uaccess.h>
#include "mtk_cpu_dbg.h"
#include <linux/delay.h>

#define OFFS_LOG_S		0x03d0
#define OFFS_LOG_E		0x0fa0
#define CSRAM_SIZE		0x1400		/* 5K bytes */
#define USRAM_SIZE		0xC00
#define DBG_REPO_NUM		(CSRAM_SIZE / sizeof(u32))
#define USRAM_REPO_NUM		(USRAM_SIZE / sizeof(u32))
#define REPO_I_LOG_S		(OFFS_LOG_S / sizeof(u32))
#define REPO_I_LOG_E		(OFFS_LOG_E / sizeof(u32))

#define ENTRY_EACH_LOG		5

#define CSRAM_BASE	0x0011bc00
#define CSRAM_SIZE	0x1400
#define USRAM_BASE	0x00115400
#define APMIXED_BASE	0x1000c20c
#define APMIXED_SIZE	0x10
#define MCUCFG_BASE	0x0c53a2a0
#define MCUCFG_SIZE	0x10

#define	LL_OFF	4
#define	L_OFF	76
#define	CCI_OFF	148
#define get_volt(offs, repo) ((repo[offs] >> 12) & 0x1FFFF)
#define get_freq(offs, repo) ((repo[offs] & 0xFFF) * 1000)


static DEFINE_MUTEX(cpufreq_mutex);
static int cpufreq_force_opp;
static void __iomem *csram_base;
static void __iomem *usram_base;

u32 *g_dbg_repo;
u32 *g_usram_repo;
u32 *g_cpufreq_debug;
u32 *g_phyclk;
u32 *g_CCI_opp_idx;
u32 *g_LL_opp_idx;
u32 *g_L_opp_idx;
unsigned int seq;

struct pll_addr pll_addr[CLUSTER_NRS];

struct pll_addr_offs pll_addr_offs[CLUSTER_NRS] = {
	[0] = {
		.armpll_con = 0,
		.clkdiv_cfg = 0,
	},
	[1] = {
		.armpll_con = 0x10,
		.clkdiv_cfg = 0x4,
	},
};

static unsigned int pll_to_clk(unsigned int pll_f, unsigned int ckdiv1)
{
	unsigned int freq = pll_f;

	switch (ckdiv1) {
	case 8:
		break;
	case 9:
		freq = freq * 3 / 4;
		break;
	case 10:
		freq = freq * 2 / 4;
		break;
	case 11:
		freq = freq * 1 / 4;
		break;
	case 16:
		break;
	case 17:
		freq = freq * 4 / 5;
		break;
	case 18:
		freq = freq * 3 / 5;
		break;
	case 19:
		freq = freq * 2 / 5;
		break;
	case 20:
		freq = freq * 1 / 5;
		break;
	case 24:
		break;
	case 25:
		freq = freq * 5 / 6;
		break;
	case 26:
		freq = freq * 4 / 6;
		break;
	case 27:
		freq = freq * 3 / 6;
		break;
	case 28:
		freq = freq * 2 / 6;
		break;
	case 29:
		freq = freq * 1 / 6;
		break;
	default:
		break;
	}

	return freq;
}

static unsigned int _cpu_freq_calc(unsigned int con1, unsigned int ckdiv1)
{
	unsigned int freq;
	unsigned int posdiv;

	posdiv = _GET_BITS_VAL_(26:24, con1);

	con1 &= _BITMASK_(21:0);
	freq = ((con1 * 26) >> 14) * 1000;

	switch (posdiv) {
	case 0:
		break;
	case 1:
		freq = freq / 2;
		break;
	case 2:
		freq = freq / 4;
		break;
	case 3:
		freq = freq / 8;
		break;
	default:
		freq = freq / 16;
		break;
	};

	return pll_to_clk(freq, ckdiv1);
}

unsigned int get_cur_phy_freq(int cluster)
{
	unsigned int con1;
	unsigned int ckdiv1;
	unsigned int cur_khz;

	con1 = readl((void __iomem *)ioremap(pll_addr[cluster].reg_addr[0], 4));
	ckdiv1 = readl((void __iomem *)ioremap(pll_addr[cluster].reg_addr[1], 4));
	ckdiv1 = _GET_BITS_VAL_(21:17, ckdiv1);
	cur_khz = _cpu_freq_calc(con1, ckdiv1);

	return cur_khz;
}

static int dbg_repo_proc_show(struct seq_file *m, void *v)
{
	int i;
	u32 *repo = m->private;
	char ch;

	for (i = 0; i < DBG_REPO_NUM; i++) {
		if (i >= REPO_I_LOG_S && (i - REPO_I_LOG_S) %
						ENTRY_EACH_LOG == 0)
			ch = ':';	/* timestamp */
		else
			ch = '.';
			seq_printf(m, "%4d%c%08x%c",
				i, ch, repo[i], i % 4 == 3 ? '\n' : ' ');
	}

	return 0;
}

PROC_FOPS_RO(dbg_repo);

static int cpufreq_debug_proc_show(struct seq_file *m, void *v)
{
	return 0;
}

static ssize_t cpufreq_debug_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int cpu = 0, min = 0, max = 0;
	unsigned long MHz;
	unsigned long mHz;
	char *buf = (char *) __get_free_page(GFP_USER);
	struct device *cpu_dev = get_cpu_device(cpu);
	struct cpufreq_policy *policy;

	if (copy_from_user(buf, buffer, count)) {
		free_page((unsigned long)buf);
		return -EINVAL;
	}

	if (sscanf(buf, "%d %d %d", &cpu, &min, &max) != 3) {
		free_page((unsigned long)buf);
		return -EINVAL;
	}

	free_page((unsigned long)buf);
	if (min <= 0 || max <= 0)
		return -EINVAL;
	MHz = max;
	mHz = min;
	MHz = MHz * 1000;
	mHz = mHz * 1000;
	dev_pm_opp_find_freq_floor(cpu_dev, &MHz);
	dev_pm_opp_find_freq_floor(cpu_dev, &mHz);

	pr_info("[DVFS] core%d: (%d %d) -- (%ld %ld)\n", cpu, min, max, mHz, MHz);

	max = (unsigned int)(MHz / 1000);
	min = (unsigned int)(mHz / 1000);

	policy = cpufreq_cpu_get(cpu);
	down_write(&policy->rwsem);
	policy->cpuinfo.max_freq = max;
	policy->cpuinfo.min_freq = min;
	up_write(&policy->rwsem);
	cpufreq_cpu_put(policy);
	cpufreq_update_limits(cpu);

	return count;
}


PROC_FOPS_RW(cpufreq_debug);


static int usram_repo_proc_show(struct seq_file *m, void *v)
{
	int i;
	u32 *repo = m->private;
	char ch;

	for (i = 0; i < USRAM_REPO_NUM; i++) {
		ch = '.';
		seq_printf(m, "%4d%c%08x%c",
				i, ch, repo[i], i % 4 == 3 ? '\n' : ' ');
	}

	return 0;
}

PROC_FOPS_RO(usram_repo);

static int opp_idx_show(struct seq_file *m, void *v, u32 pos)
{
	u32 *repo = m->private;
	u32 opp = 0;
	u32 temp = 0xFF;

	while (repo[pos] != 0xDEADBA5E && repo[pos] != temp) {
		temp = repo[pos];
		seq_printf(m, "\t%-2d (%u, %u)\n", opp,
			get_freq(pos, repo), get_volt(pos, repo));
		pos++;
		opp++;
	}

	return 0;

}

static int CCI_opp_idx_proc_show(struct seq_file *m, void *v)
{
	return opp_idx_show(m, v, CCI_OFF);
}

PROC_FOPS_RO(CCI_opp_idx);

static int LL_opp_idx_proc_show(struct seq_file *m, void *v)
{
	return opp_idx_show(m, v, LL_OFF);
}

PROC_FOPS_RO(LL_opp_idx);

static int L_opp_idx_proc_show(struct seq_file *m, void *v)
{
	return opp_idx_show(m, v, L_OFF);
}

PROC_FOPS_RO(L_opp_idx);

static int phyclk_proc_show(struct seq_file *m, void *v)
{
	int i;
	char *name_arr[2] = {"LL", "L"};

	for (i = 0; i < CLUSTER_NRS; i++)
		seq_printf(m, "cluster: %s, frequency = %d\n", name_arr[i], get_cur_phy_freq(i));

	return 0;
}

PROC_FOPS_RO(phyclk);

static int create_cpufreq_debug_fs(void)
{
	struct proc_dir_entry *dir = NULL;
	int i;

	struct pentry {
		const char *name;
		const struct proc_ops *fops;
		void *data;
	};

	const struct pentry entries[] = {
		PROC_ENTRY_DATA(dbg_repo),
		PROC_ENTRY_DATA(cpufreq_debug),
		PROC_ENTRY_DATA(phyclk),
		PROC_ENTRY_DATA(CCI_opp_idx),
		PROC_ENTRY_DATA(L_opp_idx),
		PROC_ENTRY_DATA(LL_opp_idx),
		PROC_ENTRY_DATA(usram_repo),
	};


	/* create /proc/cpuhvfs */
	dir = proc_mkdir("cpuhvfs", NULL);
	if (!dir) {
		pr_info("fail to create /proc/cpuhvfs @ %s()\n",
								__func__);
		return -ENOMEM;
	}

	for (i = 0; i < ARRAY_SIZE(entries)-1; i++) {
		if (!proc_create_data
			(entries[i].name, 0664,
			dir, entries[i].fops, csram_base))
			pr_info("%s(), create /proc/cpuhvfs/%s failed\n",
						__func__, entries[0].name);
	}
	i = ARRAY_SIZE(entries)-1;
	proc_create_data(entries[i].name, 0664, dir, entries[i].fops, usram_base);

	return 0;
}

static int mtk_cpuhvfs_init(void)
{
	int ret = 0;

	seq = 0;

	cpufreq_force_opp = 0;

	usram_base = ioremap(USRAM_BASE, USRAM_SIZE);

	if (!csram_base)
		csram_base = ioremap(CSRAM_BASE, CSRAM_SIZE);

	for (ret = 0; ret < CLUSTER_NRS; ret++) {
		pll_addr[ret].reg_addr[0] = APMIXED_BASE + pll_addr_offs[ret].armpll_con;
		pll_addr[ret].reg_addr[1] = MCUCFG_BASE + pll_addr_offs[ret].clkdiv_cfg;
	}

	create_cpufreq_debug_fs();
#ifdef EEM_DBG
	ret = mtk_eem_init();
	if (ret)
		pr_info("eem dbg init fail\n");
#endif
	return ret;
}
module_init(mtk_cpuhvfs_init)

static void mtk_cpuhvfs_exit(void)
{
}
module_exit(mtk_cpuhvfs_exit);

MODULE_DESCRIPTION("MTK CPU DVFS Platform Driver v0.1.1");
MODULE_AUTHOR("Chienwei Chang <chiewei.chang@mediatek.com>");
MODULE_LICENSE("GPL v2");

