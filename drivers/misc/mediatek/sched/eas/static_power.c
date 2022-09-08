// SPDX-License-Identifier: GPL-2.0
/*
 * static_power.c - static power api
 *
 * Copyright (c) 2020 MediaTek Inc.
 * Chienwei Chang <chienwei.chang@mediatek.com>
 */

/* system includes */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/init.h>
#include <linux/kobject.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/cpufreq.h>
#if IS_ENABLED(CONFIG_MTK_LEAKAGE_AWARE_TEMP)
#define __LKG_PROCFS__ 1
#define __LKG_DEBUG__ 0

struct leakage_para {
	int a_b_para[36];
	int c_para[36];
};

struct leakage_data {
	void __iomem *base;
	int policy[8];
	int instance[8];
	struct leakage_para tbl[8];
	int clusters;
	int init;
};

struct leakage_data info;

unsigned int mtk_get_leakage(unsigned int cpu, unsigned int opp, unsigned int temperature)
{
	int i, j;
	int power, a, b, c;

	if (info.init != 0x5A5A) {
		pr_info("[leakage] not yet init!\n");
		return 0;
	}

	j = 1 << cpu;
	for (i = 0; i < info.clusters; i++) {
		if (j & info.policy[i])
			break;
	}
	if (i >= info.clusters)
		return 0;

	if (opp >= 36)
		return 0;

	if (info.tbl[i].a_b_para[opp] == 0 && info.tbl[i].c_para[opp] == 0) {
		info.tbl[i].a_b_para[opp] =
			readl_relaxed((info.base + 0x240 + i * 0x120 + opp * 8));
		info.tbl[i].c_para[opp] =
			readl_relaxed((info.base + 0x240 + i * 0x120 + opp * 8 + 4));
	}

	a = info.tbl[i].a_b_para[opp];
	b = ((a >> 12) & 0xFFFFF);
	a = a & 0xFFF;
	c = info.tbl[i].c_para[opp];
	power = (temperature*temperature*a - b*temperature+c)/10;

	return power;
}
EXPORT_SYMBOL_GPL(mtk_get_leakage);

#if __LKG_PROCFS__
#define PROC_FOPS_RW(name)                                              \
	static int name ## _proc_open(struct inode *inode, struct file *file)\
	{                                                                       \
		return single_open(file, name ## _proc_show, PDE_DATA(inode));  \
	}                                                                       \
static const struct proc_ops name ## _proc_fops = {             \
		.proc_open           = name ## _proc_open,                              \
		.proc_read           = seq_read,                                        \
		.proc_lseek         = seq_lseek,                                        \
		.proc_release        = single_release,                          \
		.proc_write          = name ## _proc_write,                             \
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

#define PROC_ENTRY(name)        {__stringify(name), &name ## _proc_fops}
#define PROC_ENTRY_DATA(name)   \
{__stringify(name), &name ## _proc_fops, g_ ## name}


#define LAST_LL_CORE	3

int cpu, opp, temp;
u32 *g_leakage_trial;


static int leakage_trial_proc_show(struct seq_file *m, void *v)
{
	int power, a, b, c;
	u32 *repo = m->private;
	if (cpu >= info.clusters || cpu < 0)
		return 0;

	if (temp > 150 || temp < 0)
		return 0;

	if (opp > 32 || opp < 0)
		return 0;

	a = repo[144+cpu*72+opp*2];
	b = ((a >> 12) & 0xFFFFF);
	a = a & 0xFFF;
	c = repo[144+cpu*72+opp*2+1];
	power = (temp*temp*a - b*temp+c)/10;
	seq_printf(m, "power: %d, a, b, c = (%d %d %d) %d\n", power, a, b, c, info.instance[cpu]);

	return 0;
}

static ssize_t leakage_trial_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
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

	if (sscanf(buf, "%d %d %d", &cpu, &opp, &temp) != 3) {
		free_page((unsigned long)buf);
		return -EINVAL;
	}
	return count;
}

PROC_FOPS_RW(leakage_trial);
#endif
static int create_spower_debug_fs(void)
{
#if __LKG_PROCFS__
	struct proc_dir_entry *dir = NULL;

	struct pentry {
		const char *name;
		const struct proc_ops *fops;
		void *data;
	};

	const struct pentry entries[] = {
		PROC_ENTRY_DATA(leakage_trial),
	};


	/* create /proc/cpuhvfs */
	dir = proc_mkdir("static_power", NULL);
	if (!dir) {
		pr_info("fail to create /proc/static_power @ %s()\n",
								__func__);
		return -ENOMEM;
	}

	proc_create_data(entries[0].name, 0664, dir, entries[0].fops, info.base);
#endif
	return 0;
}

static int mtk_static_power_probe(struct platform_device *pdev)
{
	int cpu;
	struct cpufreq_policy *tP, *pre_tP;
	int cpu_no;
#if __LKG_DEBUG__
	unsigned int i, power;
#endif

	info.base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(info.base))
		return PTR_ERR(info.base);

	info.clusters = 0;
	tP = cpufreq_cpu_get(0);
	pre_tP = tP;
	if (tP) {
		info.policy[0] = 1;
		cpufreq_cpu_put(tP);
		cpu_no = 0;
		for_each_online_cpu(cpu) {
			tP = cpufreq_cpu_get(cpu);
			if (tP != pre_tP) {
				info.instance[info.clusters] = cpu_no;
				memset(&info.tbl[info.clusters], 0, sizeof(struct leakage_para));
				info.policy[++info.clusters] = 0;
				cpu_no = 0;
				pre_tP = tP;
			}
			info.policy[info.clusters] |= (1 << cpu);
			cpu_no++;
			if (tP)
				cpufreq_cpu_put(tP);
		}
	} else {
		/* no policy? should check dvfs status first */
		pr_info("cannot get policy, no available static power");
		return 0;
	}
	info.instance[info.clusters] = cpu_no;
	info.clusters++;

	create_spower_debug_fs();
#if __LKG_DEBUG__
	for_each_possible_cpu(cpu) {
		for (i = 0; i < 16; i++) {
			power = mtk_get_leakage(cpu, i, 40);
			pr_info("[leakage] power = %d\n", power);
		}
	}
#endif
	info.init = 0x5A5A;

	return 0;
}

static const struct of_device_id mtk_static_power_match[] = {
	{ .compatible = "mediatek,mtk-lkg" },
	{}
};


static struct platform_driver mtk_static_power_driver = {
	.probe = mtk_static_power_probe,
	.driver = {
		.name = "mtk-lkg",
		.of_match_table = mtk_static_power_match,
	},
};

int mtk_static_power_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&mtk_static_power_driver);
	return ret;

}
#else
unsigned int mtk_get_leakage(unsigned int cpu, unsigned int opp, unsigned int temperature)
{
	return 0;
}
EXPORT_SYMBOL_GPL(mtk_get_leakage);

int mtk_static_power_init(void)
{
	return 0;
}
#endif

MODULE_DESCRIPTION("MTK static power Platform Driver v0.1.1");
MODULE_AUTHOR("Chienwei Chang <chiewei.chang@mediatek.com>");
MODULE_LICENSE("GPL v2");

