// SPDX-License-Identifier: GPL-2.0
/*
 * mediatek-mcupm-cpufreq.c - MCUPM based CPUFreq Driver
 *
 * Copyright (c) 2020 MediaTek Inc.
 * Wei-Chia Su <Wei-Chia.Su@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/cpumask.h>
#include <linux/energy_model.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <linux/regulator/consumer.h>
#include <linux/soc/mediatek/mtk_tinysys_ipi.h>
#include "../misc/mediatek/mcupm/include/mcupm_driver.h"
#include "../misc/mediatek/mcupm/include/mcupm_ipi_id.h"

#define OFFS_WFI_S	0x037c
#define DVFS_D_LEN	(4)

enum cpu_dvfs_ipi_type {
	IPI_DVFS_INIT,
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

struct mtk_cpu_dvfs_info {
	struct cpumask cpus;
	struct device *cpu_dev;
	struct list_head list_head;
	struct mutex lock;
	void __iomem *csram_base;
};

static LIST_HEAD(dvfs_info_list);

static struct mtk_cpu_dvfs_info *mtk_cpu_dvfs_info_lookup(int cpu)
{
	struct mtk_cpu_dvfs_info *info;
	struct list_head *list;

	list_for_each(list, &dvfs_info_list) {
		info = list_entry(list, struct mtk_cpu_dvfs_info, list_head);
		if (cpumask_test_cpu(cpu, &info->cpus))
			return info;
	}
	return NULL;
}

static int mtk_cpufreq_set_target(struct cpufreq_policy *policy,
				  unsigned int index)
{
	struct mtk_cpu_dvfs_info *info = policy->driver_data;
	unsigned int cluster_id = policy->cpu / 4;

	writel_relaxed(index, info->csram_base + (OFFS_WFI_S + (cluster_id * 4))
		       );
	arch_set_freq_scale(policy->related_cpus,
			    policy->freq_table[index].frequency,
			    policy->cpuinfo.max_freq);

	return 0;
}

static int mtk_cpu_dvfs_info_init(struct mtk_cpu_dvfs_info *info, int cpu)
{
	struct device *cpu_dev;
	int ret;

	cpu_dev = get_cpu_device(cpu);
	if (!cpu_dev)
		return -ENODEV;

	/* Get OPP-sharing information from "operating-points-v2" bindings */
	ret = dev_pm_opp_of_get_sharing_cpus(cpu_dev, &info->cpus);
	if (ret)
		goto out_free_resources;

	ret = dev_pm_opp_of_cpumask_add_table(&info->cpus);
	if (ret)
		goto out_free_resources;

	info->cpu_dev = cpu_dev;
	mutex_init(&info->lock);

	return 0;

out_free_resources:

	return ret;
}

static void mtk_cpu_dvfs_info_release(struct mtk_cpu_dvfs_info *info)
{
	dev_pm_opp_of_cpumask_remove_table(&info->cpus);
}

static int mtk_cpufreq_init(struct cpufreq_policy *policy)
{
	struct mtk_cpu_dvfs_info *info;
	struct cpufreq_frequency_table *freq_table;
	int ret;

	info = mtk_cpu_dvfs_info_lookup(policy->cpu);
	if (!info)
		return -EINVAL;

	ret = dev_pm_opp_init_cpufreq_table(info->cpu_dev, &freq_table);
	if (ret)
		return ret;

	cpumask_copy(policy->cpus, &info->cpus);
	policy->driver_data = info;
	policy->freq_table = freq_table;
	policy->transition_delay_us = 1000; /* us */
	dev_pm_opp_of_register_em(policy->cpus);

	return 0;

out_free_opp:
	dev_pm_opp_of_cpumask_remove_table(policy->cpus);
out_free_cpufreq_table:
	dev_pm_opp_free_cpufreq_table(info->cpu_dev, &freq_table);
	return ret;
}

static int mtk_cpufreq_exit(struct cpufreq_policy *policy)
{
	struct mtk_cpu_dvfs_info *info = policy->driver_data;

	dev_pm_opp_free_cpufreq_table(info->cpu_dev, &policy->freq_table);

	return 0;
}

static struct cpufreq_driver mtk_cpufreq_driver = {
	.flags = CPUFREQ_STICKY | CPUFREQ_NEED_INITIAL_FREQ_CHECK |
		 CPUFREQ_HAVE_GOVERNOR_PER_POLICY,
	.verify = cpufreq_generic_frequency_table_verify,
	.target_index = mtk_cpufreq_set_target,
	.init = mtk_cpufreq_init,
	.exit = mtk_cpufreq_exit,
	.name = "mtk-cpufreq",
	.attr = cpufreq_generic_attr,
};

static int mtk_cpufreq_probe(struct platform_device *pdev)
{
	struct mtk_cpu_dvfs_info *info;
	struct list_head *list, *tmp;
	int cpu, ret;
	struct cdvfs_data cdvfs_d;
	uint32_t cpufreq_buf[4];
	struct mtk_ipi_device *mcupm_ipidev;

	mcupm_ipidev = (struct mtk_ipi_device *) get_mcupm_ipidev();
	if (!mcupm_ipidev)
		return -ENODEV;
	ret = mtk_ipi_register(mcupm_ipidev, CH_S_CPU_DVFS, NULL, NULL,
			 (void *) &cpufreq_buf);
	if (ret)
		return -EINVAL;

	cdvfs_d.cmd = IPI_DVFS_INIT;
	cdvfs_d.u.set_fv.arg[0] = 1;
	ret = mtk_ipi_send_compl(mcupm_ipidev, CH_S_CPU_DVFS,
				 IPI_SEND_POLLING, &cdvfs_d,
				 sizeof(struct cdvfs_data)/MBOX_SLOT_SIZE,
				 2000);
	if (ret)
		return 0;

	for_each_possible_cpu(cpu) {
		info = mtk_cpu_dvfs_info_lookup(cpu);
		if (info)
			continue;

		info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
		if (!info) {
			ret = -ENOMEM;
			goto release_dvfs_info_list;
		}

		info->csram_base = of_iomap(pdev->dev.of_node, 0);
		if (!info->csram_base) {
			ret = -ENOMEM;
			goto release_dvfs_info_list;
		}

		ret = mtk_cpu_dvfs_info_init(info, cpu);
		if (ret)
			goto release_dvfs_info_list;

		list_add(&info->list_head, &dvfs_info_list);
	}

	ret = cpufreq_register_driver(&mtk_cpufreq_driver);
	if (ret)
		goto release_dvfs_info_list;

	return 0;

release_dvfs_info_list:
	list_for_each_safe(list, tmp, &dvfs_info_list) {
		info = list_entry(list, struct mtk_cpu_dvfs_info, list_head);
		mtk_cpu_dvfs_info_release(info);
		list_del(list);
	}

	return ret;
}

/* List of machines supported by this driver */
static const struct of_device_id mtk_cpufreq_machines[] = {
	{ .compatible = "mediatek,mcupm-dvfsp", },
	{ }
};

MODULE_DEVICE_TABLE(of, mtk_cpufreq_machines);

static struct platform_driver mtk_cpufreq_platdrv = {
	.probe		= mtk_cpufreq_probe,
	.driver = {
		.name	= "dvfsp",
		.of_match_table = of_match_ptr(mtk_cpufreq_machines),
	},
};
module_platform_driver(mtk_cpufreq_platdrv);

MODULE_AUTHOR("Wei-Chia Su <Wei-Chia.Su@mediatek.com>");
MODULE_DESCRIPTION("Medaitek MCUPM CPUFreq Platform driver");
MODULE_LICENSE("GPL v2");
