// SPDX-License-Identifier: GPL-2.0
/*
 * static_power.c - static power api
 *
 * Copyright (c) 2022 MediaTek Inc.
 * Chung-Kai Yang <Chung-kai.Yang@mediatek.com>
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
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/cpufreq.h>
#include <linux/topology.h>
#include <linux/types.h>
#include "energy_model.h"


#define __LKG_PROCFS__ 0
#define __LKG_DEBUG__ 0

static struct eemsn_log *eemsn_log;
static void __iomem *usram_base, *csram_base;
struct mtk_em_perf_domain *pds;

struct mtk_em_perf_domain *mtk_em_pd_ptr_public;
struct mtk_em_perf_domain *mtk_em_pd_ptr_private;

struct leakage_data info;
static unsigned int total_cpu, total_cluster;
static unsigned int cpu_mapping[16];

static unsigned int interpolate(unsigned int a, unsigned int b,
								unsigned int x, unsigned int y,
								unsigned int r)
{
	unsigned int i = x - a;
	unsigned int j = y - b;
	unsigned int l = r - a;

	return ((i == 0) ? b : (j * l / i + b));
}

static unsigned int mtk_convert_dyn_pwr(unsigned int base_freq,
									unsigned int cur_freq,
									unsigned int base_volt,
									unsigned int cur_volt,
									unsigned int base_dyn_pwr)
{
	u64 new_dyn_pwr;

	new_dyn_pwr = (u64)base_dyn_pwr * (u64)cur_volt;
	new_dyn_pwr = div64_u64(new_dyn_pwr, base_freq);
	new_dyn_pwr *= cur_volt;
	new_dyn_pwr = div64_u64(new_dyn_pwr, base_volt);
	new_dyn_pwr *= cur_freq;
	new_dyn_pwr = div64_u64(new_dyn_pwr, base_volt);

	return (unsigned int)new_dyn_pwr;
}

/**
 * This function is to free the memory which is reserved for private table.
 */
static void free_private_table(int pd_count)
{
	int i;

	if (!mtk_em_pd_ptr_private)
		return;

	for (i = 0; i < pd_count; i++)
		kfree(mtk_em_pd_ptr_private[i].table);

	kfree(mtk_em_pd_ptr_private);
	mtk_em_pd_ptr_private = NULL;
}

static int init_private_table(unsigned int cluster)
{
	unsigned int index = 0, base_freq_idx = 0;
	unsigned int max_freq, min_freq, cur_freq;
	struct mtk_em_perf_domain *pd_public, *pd_private;
	struct mtk_em_perf_state *ps_base, *ps_next, *ps_new, ps_temp;

	pd_public = &mtk_em_pd_ptr_public[cluster];
	pd_private = &mtk_em_pd_ptr_private[cluster];
	max_freq = pd_private->max_freq;
	min_freq = pd_private->min_freq;
	ps_base = &pd_public->table[base_freq_idx];
	ps_next = &pd_public->table[base_freq_idx + 1];

	for (index = 0, cur_freq = max_freq;
		 index < pd_private->nr_perf_states;
		 index++, cur_freq -= FREQ_STEP) {
		if (cur_freq < ps_next->freq && ps_next->freq != min_freq) {
			base_freq_idx++;

			ps_base = &pd_public->table[base_freq_idx];
			ps_next = &pd_public->table[base_freq_idx + 1];
		}

		if (cur_freq == ps_base->freq) {
			ps_new = ps_base;
		} else if (cur_freq == ps_next->freq || cur_freq < min_freq) {
			ps_new = ps_next;
		} else { // cur_freq < base_freq && cur_freq > next_freq
			ps_temp.volt =
				interpolate(ps_next->freq,
							ps_next->volt,
							ps_base->freq,
							ps_base->volt,
							cur_freq);
			ps_temp.capacity =
				interpolate(ps_next->freq,
							ps_next->capacity,
							ps_base->freq,
							ps_base->capacity,
							cur_freq);
			ps_temp.leakage_para.a_b_para.a =
				interpolate(ps_next->volt,
							ps_next->leakage_para.a_b_para.a,
							ps_base->volt,
							ps_base->leakage_para.a_b_para.a,
							ps_temp.volt);
			ps_temp.leakage_para.a_b_para.b =
				interpolate(ps_next->volt,
							ps_next->leakage_para.a_b_para.b,
							ps_base->volt,
							ps_base->leakage_para.a_b_para.b,
							ps_temp.volt);
			ps_temp.leakage_para.c =
				interpolate(ps_next->volt,
							ps_next->leakage_para.c,
							ps_base->volt,
							ps_base->leakage_para.c,
							ps_temp.volt);
			ps_temp.dyn_pwr =
				mtk_convert_dyn_pwr(ps_base->freq,
									cur_freq,
									ps_base->volt,
									ps_temp.volt,
									ps_base->dyn_pwr);
			ps_new = &ps_temp;
		}

		pd_private->table[index].freq = cur_freq;
		pd_private->table[index].volt = ps_new->volt;
		pd_private->table[index].capacity = ps_new->capacity;
		pd_private->table[index].leakage_para = ps_new->leakage_para;
		pd_private->table[index].dyn_pwr = ps_new->dyn_pwr;
		/* Init for power efficiency(e.g., dyn_pwr / capacity) */
		pd_private->table[index].pwr_eff = ps_new->dyn_pwr / ps_new->capacity;
	}

	return 0;
}

/**
 * This function is to free the memory which is reserved for public table.
 */
static void free_public_table(int pd_count)
{
	int i;

	if (!mtk_em_pd_ptr_public)
		return;

	for (i = 0; i < pd_count; i++)
		kfree(mtk_em_pd_ptr_public[i].table);

	kfree(mtk_em_pd_ptr_public);
	mtk_em_pd_ptr_public = NULL;
}

/**
 * This function initializes frequency, capacity, dynamic power, and leakage
 * parameters of public table.
 */
static int init_public_table(void)
{
	unsigned int cpu, opp, cluster = 0;
	void __iomem *base = csram_base;
	unsigned long offset = CAPACITY_TBL_OFFSET;
	unsigned long cap, next_cap, end_cap;
	struct mtk_em_perf_domain *pd_info_public;
	struct cpufreq_policy *policy;

	mtk_em_pd_ptr_public = kcalloc(MAX_PD_COUNT, sizeof(struct mtk_em_perf_domain),
			GFP_KERNEL);
	if (!mtk_em_pd_ptr_public)
		return -ENOMEM;

	for_each_possible_cpu(cpu) {
		struct em_perf_domain *pd;

		pd = em_cpu_get(cpu);

		if (!pd) {
			pr_info("%s: %d: em_cpu_get return NULL for cpu#%d", __func__,
					__LINE__, cpu);
			continue;
		}
		if (cpu != cpumask_first(to_cpumask(pd->cpus)))
			continue;

		policy = cpufreq_cpu_get(cpu);
		if (!policy) {
			/* no policy? should check topology or dvfs status first */
			pr_info("%s: %d: cannot get policy for CPU: %d, no available static power\n",
					__func__, __LINE__, cpu);
			free_public_table(cluster);

			return -ENOMEM;
		}

		pd_info_public = &mtk_em_pd_ptr_public[cluster];
		pd_info_public->cpumask = topology_core_cpumask(cpu);
		pd_info_public->cluster_num = cluster;
		pd_info_public->nr_perf_states = pd->nr_perf_states;
		pd_info_public->max_freq = policy->cpuinfo.max_freq;
		pd_info_public->min_freq = policy->cpuinfo.min_freq;
		cpufreq_cpu_put(policy);

		pd_info_public->table =
			kcalloc(pd->nr_perf_states, sizeof(struct mtk_em_perf_state),
					GFP_KERNEL);

		if (!pd_info_public->table)
			goto nomem;

		for (opp = 0; opp < pd->nr_perf_states; opp++) {
			int temp;

			cap = ioread16(base + offset);
			next_cap = ioread16(base + offset + CAPACITY_ENTRY_SIZE);
			if (cap == 0 || next_cap == 0)
				goto err;

			pd_info_public->table[opp].capacity = cap;
			if (opp == pd->nr_perf_states - 1)
				next_cap = -1;

			pd_info_public->table[opp].freq =
				pd->table[pd->nr_perf_states - opp - 1].frequency;
			pd_info_public->table[opp].volt =
				(unsigned int) eemsn_log->det_log[cluster].volt_tbl_init2[opp]
								* VOLT_STEP;
			pd_info_public->table[opp].dyn_pwr =
				pd->table[pd->nr_perf_states - opp - 1].power * 1000;

			temp =	readl_relaxed((usram_base + 0x240 + cluster * 0x120 + opp * 8));

			pd_info_public->table[opp].leakage_para.c =
					readl_relaxed((usram_base + 0x240 +
								   cluster * 0x120 +
								   opp * 8 + 4));
			pd_info_public->table[opp].leakage_para.a_b_para.b =
							((temp >> 12) & 0xFFFFF);
			pd_info_public->table[opp].leakage_para.a_b_para.a = temp & 0xFFF;
			pd_info_public->table[opp].pwr_eff = pd_info_public->table[opp].dyn_pwr
					/ pd_info_public->table[opp].capacity;

			offset += CAPACITY_ENTRY_SIZE;
		}

		/* repeated last cap 0 between each cluster */
		end_cap = ioread16(base + offset);
		if (end_cap != cap)
			goto err;
		offset += CAPACITY_ENTRY_SIZE;
		cluster++;
	}

	return 0;

nomem:
	pr_info("%s: allocate public table for cluster %d failed\n",
			__func__, cluster);
err:
	pr_info("%s: capacity count or value does not match on cluster %d\n",
			__func__, cluster);
	free_public_table(cluster);

	return -ENOENT;
}

static unsigned int mtk_get_nr_perf_states(unsigned int cluster)
{
	unsigned int min_freq = mtk_em_pd_ptr_private[cluster].min_freq;
	unsigned int max_freq = mtk_em_pd_ptr_private[cluster].max_freq;
	unsigned int result;

	if (min_freq > max_freq) {
		pr_info("%s: %d: min freq is larger than max freq\n", __func__,
			__LINE__);
		return 0;
	}

	result = (max_freq - min_freq) / FREQ_STEP;
	result += ((max_freq - min_freq) % FREQ_STEP) ? 2 : 1;

	return result;
}

inline unsigned int mtk_get_leakage(unsigned int cpu, unsigned int idx, unsigned int degree)
{
	int a, b, c, power, cluster;
	struct mtk_em_perf_domain *pd;

	if (info.init != 0x5A5A) {
		pr_info("[leakage] not yet init!\n");
		return 0;
	}

	if (cpu > total_cpu)
		return 0;
	cluster = cpu_mapping[cpu];

	pd = &mtk_em_pd_ptr_private[cluster];
	if (idx >= pd->nr_perf_states) {
		pr_debug("%s: %d: input index is out of nr_perf_states\n", __func__,
				__LINE__);
		return 0;
	}

	a = pd->table[idx].leakage_para.a_b_para.a;
	b = pd->table[idx].leakage_para.a_b_para.b;
	c = pd->table[idx].leakage_para.c;

	power = degree * (degree * a - b) + c;

	return (power /= (cluster > 0) ? 10 : 100);
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

static int input_cluster, input_opp, input_temp;
u32 *g_leakage_trial;

static int leakage_trial_proc_show(struct seq_file *m, void *v)
{
	struct mtk_em_perf_domain *pd;
	struct mtk_em_perf_state *ps;
	int a, b, c, power;

	if (input_cluster >= total_cluster || input_cluster < 0)
		return 0;

	if (input_temp > 150 || input_temp < 0)
		return 0;

	if (input_opp > 32 || input_opp < 0)
		return 0;

	pd = &mtk_em_pd_ptr_public[input_cluster];
	ps = &pd->table[input_opp];
	a = ps->leakage_para.a_b_para.a;
	b = ps->leakage_para.a_b_para.b;
	c = ps->leakage_para.c;
	power = input_temp * (input_temp * a - b) + c;
	power /= (input_cluster > 0) ? 10 : 100;

	seq_printf(m, "power: %d, a, b, c = (%d %d %d) %*pbl\n",
				power, a, b, c,
				cpumask_pr_args(pd->cpumask));

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

	if (sscanf(buf, "%d %d %d", &input_cluster, &input_opp, &input_temp) != 3) {
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
#if __LKG_DEBUG__
	unsigned int i, power;
#endif
	int ret = 0, err = 0;
	unsigned int cpu, cluster = 0, size;
	struct device_node *dvfs_node;
	struct platform_device *pdev_temp;
	struct resource *usram_res, *csram_res, *eem_res;
	struct cpumask *cpumask;
	struct mtk_em_perf_domain *pd_public, *pd_private;

	pr_info("[Static Power v2.1.1] Start to parse DTS\n");

	dvfs_node = of_find_node_by_name(NULL, "cpuhvfs");
	if (dvfs_node == NULL) {
		pr_info("failed to find node @ %s\n", __func__);
		return -ENODEV;
	}

	pdev_temp = of_find_device_by_node(dvfs_node);
	if (pdev_temp == NULL) {
		pr_info("failed to find pdev @ %s\n", __func__);
		return -EINVAL;
	}

	usram_res = platform_get_resource(pdev_temp, IORESOURCE_MEM, 0);
	if (usram_res)
		usram_base = ioremap(usram_res->start, resource_size(usram_res));
	else {
		pr_info("%s can't get resource, ret: %d\n", __func__, err);
		return -ENODEV;
	}

	csram_res = platform_get_resource(pdev_temp, IORESOURCE_MEM, 1);
	if (csram_res)
		csram_base = ioremap(csram_res->start, resource_size(csram_res));
	else {
		pr_info("%s can't get resource, ret: %d\n", __func__, err);
		return -ENODEV;
	}

	eem_res = platform_get_resource(pdev_temp, IORESOURCE_MEM, 2);
	if (eem_res)
		eemsn_log = ioremap(eem_res->start, resource_size(eem_res));
	else {
		pr_info("%s can't get resource, ret: %d\n", __func__, err);
		return -ENODEV;
	}

	pr_info("[Static Power v2.1.1] MTK EM start\n");

	cpumask = topology_core_cpumask(0);
	ret = init_public_table();
	if (ret < 0) {
		pr_info("%s: initialize public table failed, ret: %d\n",
				__func__, ret);
		return ret;
	}

	mtk_em_pd_ptr_private = kcalloc(MAX_PD_COUNT, sizeof(struct mtk_em_perf_domain),
			GFP_KERNEL);
	if (!mtk_em_pd_ptr_private) {
		pr_info("%s can't get private table ptr, ret: %d\n", __func__, err);
		return -ENOMEM;
	}

	for_each_possible_cpu(cpu) {
		if (cpumask_test_cpu(cpu, cpumask) && cpu != 0) {
			pr_debug("%s: cpu %d in %d\n", __func__, cpu, cluster);
			cpu_mapping[cpu] = cluster - 1;
			continue;
		} else {
			pd_public = &mtk_em_pd_ptr_public[cluster];
			if (!pd_public) {
				pr_info("failed to get public cpu%d device\n", cpu);
				return -ENODEV;
			}
			pd_private = &mtk_em_pd_ptr_private[cluster];
			if (!pd_private) {
				pr_info("failed to get private cpu%d device\n", cpu);
				return -ENODEV;
			}

			cpumask = topology_core_cpumask(cpu);
			pd_private->cpumask = topology_core_cpumask(cpu);
			pd_private->cluster_num = cluster;
			pd_private->max_freq = pd_public->max_freq;
			pd_private->min_freq = pd_public->min_freq;

			size = mtk_get_nr_perf_states(cluster);
			pd_private->nr_perf_states = size;
			pd_private->table =
				kcalloc(size, sizeof(struct mtk_em_perf_state), GFP_KERNEL);
			if (!pd_private->table)
				goto nomem;

			init_private_table(cluster);

			cpu_mapping[cpu] = cluster;
			cluster++;
		}
		pr_info("%s: MTK_EM: CPU %d: created perf domain\n", __func__, cpu);
	}

	total_cluster = cluster;
	total_cpu = cpu + 1;

	pr_info("%s: [cpu_mapping]: ", __func__);
	for (cpu = 0; cpu < 8; cpu++)
		pr_info("cpu: %d, cluster: %d, ", cpu, cpu_mapping[cpu]);
	pr_info("\n");

	/* Create debug fs */
	info.base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(info.base))
		return PTR_ERR(info.base);

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

	pr_info("[Static Power v2.1.1] MTK EM done\n");

	return ret;

nomem:
	pr_info("%s: allocate private table for cluster %d failed\n",
			__func__, cluster);
	free_private_table(cluster);

	return -ENOENT;
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

int __init mtk_static_power_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&mtk_static_power_driver);
	return ret;
}

MODULE_DESCRIPTION("MTK static power Platform Driver v2.1.1");
MODULE_AUTHOR("Chung-Kai Yang <Chung-kai.Yang@mediatek.com>");
MODULE_LICENSE("GPL v2");
