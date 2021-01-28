// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
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
#include "../misc/mediatek/sspm/sspm_ipi.h"

#define OFFS_WFI_S	0x037c
#define DVFS_D_LEN	(4)

struct mtk_cpu_dvfs_info {
	struct cpumask cpus;
	struct clk *cpu_clk;
	struct device *cpu_dev;
	struct list_head list_head;
	struct mutex lock;
	struct regulator *proc_reg;
	struct regulator *sram_reg;
	void __iomem *csram_base;
};

static LIST_HEAD(dvfs_info_list);

enum cpu_dvfs_ipi_type {
	IPI_DVFS_INIT,
	IPI_SET_CLUSTER_ON_OFF,
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

static int dvfs_to_spm2_command(u32 cmd, struct cdvfs_data *cdvfs_d)
{
	unsigned int len = DVFS_D_LEN;
	int ack_data;
	unsigned int ret = 0;

	switch (cmd) {
	case IPI_DVFS_INIT:
		cdvfs_d->cmd = cmd;
		sspm_ipi_send_sync_new(IPI_ID_CPU_DVFS, IPI_OPT_POLLING,
					cdvfs_d, len, &ack_data, 1);
		if (ret) {
			pr_debug("#@# %s(%d) sspm_ipi_send_sync ret %d\n",
					__func__, __LINE__, ret);
		} else if (ack_data < 0) {
			ret = ack_data;
			pr_debug("#@# %s(%d) cmd(%d) return %d\n",
					__func__, __LINE__, cmd, ret);
		}
		break;

	case IPI_SET_CLUSTER_ON_OFF:
		cdvfs_d->cmd = cmd;
		sspm_ipi_send_sync_new(IPI_ID_CPU_DVFS, IPI_OPT_POLLING,
					cdvfs_d, len, &ack_data, 1);
		if (ret) {
			pr_debug("ret = %d, set cluster%d ON/OFF state to %d\n",
					ret, cdvfs_d->u.set_fv.arg[0],
					cdvfs_d->u.set_fv.arg[1]);
		} else if (ack_data < 0) {
			pr_debug("ret = %d, set cluster%d ON/OFF state to %d\n",
					ret, cdvfs_d->u.set_fv.arg[0],
					cdvfs_d->u.set_fv.arg[1]);
		}
		break;

	default:
		break;
	}

	return ret;
}

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
	unsigned int cluster_id = policy->cpu / 6;

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
	struct regulator *proc_reg = ERR_PTR(-ENODEV);
	struct regulator *sram_reg = ERR_PTR(-ENODEV);
	struct clk *cpu_clk = ERR_PTR(-ENODEV);
	int ret;

	cpu_dev = get_cpu_device(cpu);
	if (!cpu_dev)
		return -ENODEV;

	cpu_clk = devm_clk_get(cpu_dev, "cpu");
	if (IS_ERR(cpu_clk)) {
		if (PTR_ERR(cpu_clk) == -EPROBE_DEFER)
			pr_debug("cpu clk for cpu%d not ready, retry.\n", cpu);
		else
			pr_debug("failed to get cpu clk for cpu%d\n", cpu);
		ret = PTR_ERR(cpu_clk);
		return ret;
	}

	ret = clk_prepare_enable(cpu_clk);
	if (ret)
		pr_debug("cannot enable parent clock: %d\n", ret);

	proc_reg = regulator_get_optional(cpu_dev, "proc");
	if (IS_ERR(proc_reg)) {
		if (PTR_ERR(proc_reg) == -EPROBE_DEFER)
			pr_debug("proc regulator for cpu%d not ready, retry.\n",
					cpu);
		else
			pr_debug("failed to get proc regulator for cpu%d\n",
					cpu);

		ret = PTR_ERR(proc_reg);
		goto out_free_resources;
	}

	/* Both presence and absence of sram regulator are valid cases. */
	sram_reg = regulator_get_optional(cpu_dev, "sram");

	/* Get OPP-sharing information from "operating-points-v2" bindings */
	ret = dev_pm_opp_of_get_sharing_cpus(cpu_dev, &info->cpus);
	if (ret)
		goto out_free_resources;

	ret = dev_pm_opp_of_cpumask_add_table(&info->cpus);
	if (ret)
		goto out_free_resources;

	info->cpu_dev = cpu_dev;
	info->proc_reg = proc_reg;
	info->sram_reg = IS_ERR(sram_reg) ? NULL : sram_reg;
	info->cpu_clk = cpu_clk;
	mutex_init(&info->lock);

	return 0;

out_free_resources:
	if (!IS_ERR(proc_reg))
		regulator_put(proc_reg);
	if (!IS_ERR(sram_reg))
		regulator_put(sram_reg);
	if (!IS_ERR(cpu_clk))
		clk_put(cpu_clk);

	return ret;
}

static void mtk_cpu_dvfs_info_release(struct mtk_cpu_dvfs_info *info)
{
	if (!IS_ERR(info->proc_reg))
		regulator_put(info->proc_reg);
	if (!IS_ERR(info->sram_reg))
		regulator_put(info->sram_reg);
	if (!IS_ERR(info->cpu_clk))
		clk_put(info->cpu_clk);

	dev_pm_opp_of_cpumask_remove_table(&info->cpus);
}

static int mtk_cpufreq_init(struct cpufreq_policy *policy)
{
	struct mtk_cpu_dvfs_info *info;
	struct cdvfs_data cdvfs_d;
	struct cpufreq_frequency_table *freq_table;
	struct em_data_callback em_cb = EM_DATA_CB(of_dev_pm_opp_get_cpu_power);
	int ret;

	info = mtk_cpu_dvfs_info_lookup(policy->cpu);
	if (!info)
		return -EINVAL;

	ret = dev_pm_opp_init_cpufreq_table(info->cpu_dev, &freq_table);
	if (ret)
		return ret;

	ret = cpufreq_frequency_table_verify(policy, freq_table);
	if (ret)
		goto out_free_cpufreq_table;

	ret = dev_pm_opp_get_opp_count(info->cpu_dev);
	if (ret <= 0) {
		ret = -EINVAL;
		goto out_free_opp;
	}

	cpumask_copy(policy->cpus, &info->cpus);
	em_register_perf_domain(policy->cpus, ret, &em_cb);
	policy->driver_data = info;
	policy->clk = info->cpu_clk;
	policy->freq_table = freq_table;
	policy->transition_delay_us = 1000; /* us */
	/* Cluster, ON:1/OFF:0 */
	cdvfs_d.u.set_fv.arg[0] = policy->cpu / 6;
	cdvfs_d.u.set_fv.arg[1] = 1;
	dvfs_to_spm2_command(IPI_SET_CLUSTER_ON_OFF, &cdvfs_d);

	return 0;

out_free_opp:
	dev_pm_opp_of_cpumask_remove_table(policy->cpus);
out_free_cpufreq_table:
	dev_pm_opp_free_cpufreq_table(info->cpu_dev, &freq_table);
	return ret;
}

static int mtk_cpufreq_exit(struct cpufreq_policy *policy)
{
	struct cdvfs_data cdvfs_d;
	struct mtk_cpu_dvfs_info *info = policy->driver_data;

	/* Cluster, ON:1/OFF:0 */
	cdvfs_d.u.set_fv.arg[0] = policy->cpu / 6;
	cdvfs_d.u.set_fv.arg[1] = 0;
	dvfs_to_spm2_command(IPI_SET_CLUSTER_ON_OFF, &cdvfs_d);
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

	cdvfs_d.u.set_fv.arg[0] = 0;
	dvfs_to_spm2_command(IPI_DVFS_INIT, &cdvfs_d);

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
	{ .compatible = "mediatek,sspm-dvfsp", },
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
MODULE_DESCRIPTION("Medaitek SSPM CPUFreq Platform driver");
MODULE_LICENSE("GPL v2");
