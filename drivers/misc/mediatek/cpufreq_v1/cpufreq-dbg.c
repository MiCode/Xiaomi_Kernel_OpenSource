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
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <linux/cpufreq.h>
#include <linux/uaccess.h>
#include "mtk_cpu_dbg.h"
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include "../mcupm/include/mcupm_driver.h"
#include "../mcupm/include/mcupm_ipi_id.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "[cpuhvfs]: " fmt

#define PLL_CONFIG_PROP_NAME "pll-con"
#define TBL_OFF_PROP_NAME "tbl-off"
#define CLK_DIV_PROP_NAME "clk-div"
#define APMIXED_BASE_PROP_NAME "apmixedsys"
#define MCUCFG_BASE_PROP_NAME "clk-div-base"
#define MCUCFG_VERSION "mcucfg-ver"
#define CSRAM_DVFS_LOG_RANGE "cslog-range"

#define get_freq(offs, repo) ((repo[offs] & 0xFFF) * 1000)

#define ENTRY_EACH_LOG	5


#ifdef DSU_DVFS_ENABLE
unsigned int force_disable;
#endif
unsigned int dbg_repo_num;
unsigned int usram_repo_num;
unsigned int repo_i_log_s;
unsigned int repo_i_log_e;
int dvfs_ackdata;

static DEFINE_MUTEX(cpufreq_mutex);
static void __iomem *csram_base;
static void __iomem *usram_base;
static void __iomem *apmixed_base;
static void __iomem *mcucfg_base;

u32 *g_dbg_repo;
u32 *g_usram_repo;
u32 *g_cpufreq_debug;
u32 *g_cpufreq_cci_mode;
u32 *g_cpufreq_cci_idx;
u32 *g_phyclk;
u32 *g_phyvolt;
u32 *g_C0_opp_idx;
u32 *g_C1_opp_idx;
u32 *g_C2_opp_idx;
u32 *g_C3_opp_idx;

int g_num_cluster;//g_num_cluster<=MAX_CLUSTER_NRS
struct pll_addr pll_addr[MAX_CLUSTER_NRS];//domain + cci
unsigned int cluster_off[MAX_CLUSTER_NRS];//domain + cci

static struct regulator *vprocs[MAX_CLUSTER_NRS];
static unsigned int vprocs_step[MAX_CLUSTER_NRS];

static enum mcucfg_ver g_mcucfg_ver = MCUCFG_V0;
static unsigned int (*pll_to_clk_wrapper)(unsigned int, unsigned int);

static unsigned int pll_to_clk_v0(unsigned int pll_f, unsigned int ckdiv1)
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

static unsigned int pll_to_clk_v1(unsigned int pll_f, unsigned int ckdiv1)
{
	unsigned int freq = pll_f;

	switch (ckdiv1) {
	case 1:
		freq = freq / 2;
		break;
	case 3:
		freq = freq / 4;
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

	return pll_to_clk_wrapper(freq, ckdiv1);
}

static int cpuhvfs_set_volt(int cluster_id, unsigned int volt)
{
	struct cdvfs_data cdvfs_d;

	cdvfs_d.cmd = IPI_SET_VOLT;
	cdvfs_d.u.set_fv.arg[0] = cluster_id;
	cdvfs_d.u.set_fv.arg[1] = volt;
	return mtk_ipi_send_compl(get_mcupm_ipidev(), CH_S_CPU_DVFS,
			IPI_SEND_POLLING, &cdvfs_d,
			sizeof(struct cdvfs_data)/MBOX_SLOT_SIZE, 2000);
}

static int cpuhvfs_set_freq(int cluster_id, unsigned int freq)
{
	struct cdvfs_data cdvfs_d;

	cdvfs_d.cmd = IPI_SET_FREQ;
	cdvfs_d.u.set_fv.arg[0] = cluster_id;
	cdvfs_d.u.set_fv.arg[1] = freq;

	return mtk_ipi_send_compl(get_mcupm_ipidev(), CH_S_CPU_DVFS,
			IPI_SEND_POLLING, &cdvfs_d,
			sizeof(struct cdvfs_data)/MBOX_SLOT_SIZE, 2000);
}

unsigned int get_cur_phy_freq(int cluster)
{
	unsigned int con1;
	unsigned int ckdiv1;
	unsigned int cur_khz;

	con1 = readl(apmixed_base+pll_addr[cluster].reg_addr[0]);
	ckdiv1 = readl(mcucfg_base+pll_addr[cluster].reg_addr[1]);

	switch (g_mcucfg_ver) {
	case MCUCFG_V0:
		ckdiv1 = _GET_BITS_VAL_(21:17, ckdiv1);
		pll_to_clk_wrapper = &pll_to_clk_v0;
		break;
	case MCUCFG_V1:
		ckdiv1 = _GET_BITS_VAL_(6:4, ckdiv1);
		pll_to_clk_wrapper = &pll_to_clk_v1;
		break;
	default:
		pr_notice("invalid mcucfg version: %d\n",
			g_mcucfg_ver);
		ckdiv1 = _GET_BITS_VAL_(21:17, ckdiv1);
		pll_to_clk_wrapper = &pll_to_clk_v0;
		break;
	}

	cur_khz = _cpu_freq_calc(con1, ckdiv1);

	return cur_khz;
}

void update_cci_mode(unsigned int mode, unsigned int use_id)
{
	/* mode = 0(Normal as 50%) mode = 1(Perf as 70%) */
#ifdef DSU_DVFS_ENABLE
	if (use_id == FPS_PERF && force_disable)
		return;
	if (!use_id) {
		if (mode == PERF) {
			swpm_pmu_enable(0);
			force_disable = 1;
		} else {
			swpm_pmu_enable(1);
			force_disable = 0;
		}
	}
#endif
	csram_write(OFFS_CCI_TBL_USER, use_id);
	csram_write(OFFS_CCI_TBL_MODE, mode);
	csram_write(OFFS_CCI_TOGGLE_BIT, 1);
}

static int dbg_repo_proc_show(struct seq_file *m, void *v)
{
	int i;
	u32 *repo = m->private;
	char ch;

	for (i = 0; i < dbg_repo_num; i++) {
		if (i >= repo_i_log_s && (i - repo_i_log_s) %
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

	if (!buf)
		return -ENOMEM;

	if (count > 255) {
		free_page((unsigned long)buf);
		return -EINVAL;
	}

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
	if (cpu > 7 || cpu < 0)
		return -EINVAL;
	MHz = max;
	mHz = min;
	MHz = MHz * 1000;
	mHz = mHz * 1000;
	dev_pm_opp_find_freq_floor(cpu_dev, &MHz);
	dev_pm_opp_find_freq_floor(cpu_dev, &mHz);

	max = (unsigned int)(MHz / 1000);
	min = (unsigned int)(mHz / 1000);

	policy = cpufreq_cpu_get(cpu);
	if (policy == NULL) {
		return count;
	}
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

	for (i = 0; i < usram_repo_num; i++) {
		ch = '.';
		seq_printf(m, "%4d%c%08x%c",
				i, ch, repo[i], i % 4 == 3 ? '\n' : ' ');
	}

	return 0;
}

PROC_FOPS_RO(usram_repo);

static int cpufreq_cci_mode_proc_show(struct seq_file *m, void *v)
{
	unsigned int mode;

	mode = csram_read(OFFS_CCI_TBL_MODE);

	if (mode == 0)
		seq_puts(m, "cci_mode as Normal mode 0\n");
	else if (mode == 1)
		seq_puts(m, "cci_mode as Perf mode 1\n");
	else
		seq_puts(m, "cci_mode as Unknown mode 2\n");

	return 0;
}

static ssize_t cpufreq_cci_mode_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int mode = 0;
	int rc;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count > 255) {
		free_page((unsigned long)buf);
		return -EINVAL;
	}

	if (copy_from_user(buf, buffer, count)) {
		free_page((unsigned long)buf);
		return -EINVAL;
	}

	rc = kstrtoint(buf, 10, &mode);

	if (rc < 0)
		tag_pr_info(
		"Usage: echo <mode>(0:Nom 1:Perf)\n");
	else
		update_cci_mode(mode, 0);

	free_page((unsigned long)buf);
	return count;

}
PROC_FOPS_RW(cpufreq_cci_mode);

static int cpufreq_cci_idx_proc_show(struct seq_file *m, void *v)
{
	unsigned int cci_idx;

	cci_idx = csram_read(OFFS_CCI_IDX);

	seq_printf(m, "DSU OPP IDX %4d\n", cci_idx);

	return 0;
}
PROC_FOPS_RO(cpufreq_cci_idx);
static int opp_idx_show(struct seq_file *m, void *v, u32 pos)
{
	u32 *repo = m->private;
	u32 opp = 0;
	u32 prev_freq = 0x0; //some invalid freq value

	while (pos < dbg_repo_num && get_freq(pos, repo) != prev_freq) {
		prev_freq = get_freq(pos, repo);
		seq_printf(m, "\t%-2d (%u, kHz)\n", opp, get_freq(pos, repo));
		pos++;
		opp++;
	}

	return 0;

}

static int C0_opp_idx_proc_show(struct seq_file *m, void *v)
{
	return opp_idx_show(m, v, cluster_off[0]);
}

PROC_FOPS_RO(C0_opp_idx);

static int C1_opp_idx_proc_show(struct seq_file *m, void *v)
{
	return opp_idx_show(m, v, cluster_off[1]);
}

PROC_FOPS_RO(C1_opp_idx);

static int C2_opp_idx_proc_show(struct seq_file *m, void *v)
{
	return opp_idx_show(m, v, cluster_off[2]);
}

PROC_FOPS_RO(C2_opp_idx);

static int C3_opp_idx_proc_show(struct seq_file *m, void *v)
{
	return opp_idx_show(m, v, cluster_off[3]);
}

PROC_FOPS_RO(C3_opp_idx);

static int phyclk_proc_show(struct seq_file *m, void *v)
{
	int i;
	static const char * const name_arr[] = {"C0", "C1", "C2", "C3"};

	for (i = 0; i < g_num_cluster; i++)
		seq_printf(m, "old cluster: %s, freq = %d\n", name_arr[i], get_cur_phy_freq(i));
	return 0;
}

static ssize_t phyclk_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int cluster = 0, freq = 0;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count > 255) {
		free_page((unsigned long)buf);
		return -EINVAL;
	}

	if (copy_from_user(buf, buffer, count)) {
		free_page((unsigned long)buf);
		return -EINVAL;
	}

	if (sscanf(buf, "%d %d", &cluster, &freq) != 2) {
		free_page((unsigned long)buf);
		return -EINVAL;
	}

	free_page((unsigned long)buf);
	if (freq <= 0)
		return -EINVAL;

	cpuhvfs_set_freq(cluster, freq);

	return count;
}

PROC_FOPS_RW(phyclk);

static int phyvolt_proc_show(struct seq_file *m, void *v)
{
	int i, cur_uv;
	static const char * const name_arr[] = {"C0", "C1", "C2", "C3"};

	for (i = 0; i < g_num_cluster; i++) {
		if (!IS_ERR(vprocs[i]) && vprocs[i]) {
			cur_uv = regulator_get_voltage(vprocs[i]);
			cur_uv += vprocs_step[i];
		} else {
			cur_uv = -ENODEV;
		}
		seq_printf(m, "old cluster: %s, volt = %d\n", name_arr[i], cur_uv);
	}
	return 0;
}

static ssize_t phyvolt_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int cluster = 0, volt = 0;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count > 255) {
		free_page((unsigned long)buf);
		return -EINVAL;
	}

	if (copy_from_user(buf, buffer, count)) {
		free_page((unsigned long)buf);
		return -EINVAL;
	}

	if (sscanf(buf, "%d %d", &cluster, &volt) != 2) {
		free_page((unsigned long)buf);
		return -EINVAL;
	}

	free_page((unsigned long)buf);
	if (volt <= 0)
		return -EINVAL;


	cpuhvfs_set_volt(cluster, volt);

	return count;
}
PROC_FOPS_RW(phyvolt);

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
		PROC_ENTRY_DATA(phyvolt),
		PROC_ENTRY_DATA(cpufreq_cci_mode),
		PROC_ENTRY_DATA(cpufreq_cci_idx),
		PROC_ENTRY_DATA(usram_repo),
	};
	const struct pentry clusters[MAX_CLUSTER_NRS] = {
		PROC_ENTRY_DATA(C0_opp_idx),//L  or LL
		PROC_ENTRY_DATA(C1_opp_idx),//BL or L
		PROC_ENTRY_DATA(C2_opp_idx),//B  or CCI
		PROC_ENTRY_DATA(C3_opp_idx),//CCI
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

	if (g_num_cluster > MAX_CLUSTER_NRS) {
		pr_info("fail to create CX_opp_idx @ %s()\n", __func__);
		return -EINVAL;
	}
	// CPU Clusters + CCI cluster
	for (i = 0; i < g_num_cluster; i++) {
		if (!proc_create_data(clusters[i].name, 0664, dir, clusters[i].fops, csram_base))
			pr_info("%s(), create /proc/cpuhvfs/%s failed\n",
				__func__, entries[0].name);
	}

	return 0;
}

static int mtk_cpuhvfs_init(void)
{
	int ret = 0, i;
	unsigned int mcucfg_ver_tmp;
	struct device_node *hvfs_node;
	struct device_node *apmixed_node;
	struct device_node *mcucfg_node;
	struct platform_device *pdev;
	struct resource *csram_res, *usram_res;
	static const char * const vproc_names[] = {"proc1", "proc2", "proc3", "proc4"};

	hvfs_node = of_find_node_by_name(NULL, "cpuhvfs");
	if (hvfs_node == NULL) {
		pr_notice("failed to find node @ %s\n", __func__);
		return -ENODEV;
	}

	pdev = of_find_device_by_node(hvfs_node);
	if (pdev == NULL) {
		pr_notice("failed to find pdev @ %s\n", __func__);
		return -EINVAL;
	}

	usram_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	usram_base = ioremap(usram_res->start, resource_size(usram_res));
	if (usram_base == NULL) {
		pr_notice("failed to map usram_base @ %s\n", __func__);
		return -EINVAL;
	}
	usram_repo_num = (resource_size(usram_res) / sizeof(u32));

	csram_res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	csram_base = ioremap(csram_res->start, resource_size(csram_res));
	if (csram_base == NULL) {
		pr_notice("failed to map csram_base @ %s\n", __func__);
		return -EINVAL;
	}

	dbg_repo_num = (resource_size(csram_res) / sizeof(u32));
	//Start address from csram
	of_property_read_u32_index(hvfs_node, CSRAM_DVFS_LOG_RANGE, 0, &repo_i_log_s);
	//End address from csram
	of_property_read_u32_index(hvfs_node, CSRAM_DVFS_LOG_RANGE, 1, &repo_i_log_e);
	//only used for having a pretty dump format
	repo_i_log_s /= sizeof(u32);
	repo_i_log_e /= sizeof(u32);

	/* get PLL config & CLKDIV for every cluster */
	ret = of_property_count_u32_elems(hvfs_node, PLL_CONFIG_PROP_NAME);
	if (ret < 0) {
		pr_notice("failed to get num_cluster @ %s\n", __func__);
		return -EINVAL;
	}
	g_num_cluster = ret;
	ret = of_property_count_u32_elems(hvfs_node, CLK_DIV_PROP_NAME);
	if (ret != g_num_cluster) {
		pr_notice("clk-div size is not aligned@ %s\n", __func__);
		return -EINVAL;
	}

	//get APMIXED_BASE & MCUCFG_BASE
	apmixed_node = of_parse_phandle(hvfs_node, APMIXED_BASE_PROP_NAME, 0);
	if (apmixed_node == NULL) {
		pr_notice("failed to get apmixed base @ %s\n", __func__);
		return -EINVAL;
	}
	apmixed_base = of_iomap(apmixed_node, 0);

	mcucfg_node = of_parse_phandle(hvfs_node, MCUCFG_BASE_PROP_NAME, 0);
	if (mcucfg_node == NULL) {
		pr_notice("failed to get mcucfg base @ %s\n", __func__);
		return -EINVAL;
	}
	mcucfg_base = of_iomap(mcucfg_node, 0);

	for (i = 0; i < g_num_cluster; i++) {
		of_property_read_u32_index(hvfs_node,
			PLL_CONFIG_PROP_NAME, i, &pll_addr[i].reg_addr[0]);
		of_property_read_u32_index(hvfs_node,
			CLK_DIV_PROP_NAME, i, &pll_addr[i].reg_addr[1]);
	}

	/* MCUCFG clkdiv format version */
	ret = of_property_read_u32(hvfs_node, MCUCFG_VERSION, &mcucfg_ver_tmp);
	if (ret >= 0 && ret < MAX_MCUCFG_VERSION)
		g_mcucfg_ver = (enum mcucfg_ver)mcucfg_ver_tmp;
	pr_notice("mcucfg version: %d", g_mcucfg_ver);

	//Offsets used to fetch OPP table
	ret = of_property_count_u32_elems(hvfs_node, TBL_OFF_PROP_NAME);
	if (ret != g_num_cluster) {
		pr_notice("only get %d opp offset@ %s\n", ret, __func__);
		return -EINVAL;
	}
	for (i = 0; i < g_num_cluster; i++)
		of_property_read_u32_index(hvfs_node, TBL_OFF_PROP_NAME, i, &cluster_off[i]);

	// Get regulators for dvfs debugging
	for (i = 0; i < g_num_cluster; i++) {
		vprocs[i] = devm_regulator_get_optional(&pdev->dev, vproc_names[i]);
		if (!IS_ERR(vprocs[i]) && vprocs[i])
			pr_info("regulator used for %s was found\n", vproc_names[i]);
		else
			pr_info("regulator used for %s was not found\n", vproc_names[i]);

		/* if we need to add some step */
		ret = of_property_read_u32(hvfs_node, vproc_names[i], vprocs_step + i);
		if (ret != 0)
			vprocs_step[i] = 0; /* default value */
	}

	create_cpufreq_debug_fs();
#ifdef EEM_DBG
	ret = mtk_eem_init(pdev);
	if (ret)
		pr_info("eem dbg init fail: %d\n", ret);
#endif
	ret = mtk_ipi_register(get_mcupm_ipidev(), CH_S_CPU_DVFS, NULL, NULL,
		(void *)&dvfs_ackdata);
	return 0;
}
module_init(mtk_cpuhvfs_init)

static void mtk_cpuhvfs_exit(void)
{
}
module_exit(mtk_cpuhvfs_exit);

MODULE_DESCRIPTION("MTK CPU DVFS Platform Driver v0.1.1");
MODULE_AUTHOR("Chienwei Chang <chiewei.chang@mediatek.com>");
MODULE_LICENSE("GPL v2");
