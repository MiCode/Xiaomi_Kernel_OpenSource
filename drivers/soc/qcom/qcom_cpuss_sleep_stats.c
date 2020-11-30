// SPDX-License-Identifier: GPL-2.0-only

/*
 * Copyright (c) 2020 The Linux Foundation. All rights reserved.
 */

#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#define MAX_POSSIBLE_CPUS	8
#define MIN_POSSIBLE_CPUS	1
#define SEQ_LPM_STR_SZ		22
#define CPUNAME_SZ		6
#define STATS_NAME_SZ		25
#define OFFSET_8BYTES		0x08
#define OFFSET_4BYTES		0x04

#define MAX_CPU_LPM_BITS	4
#define MAX_CL_LPM_BITS		4
#define MAX_CPU_RESIDENCY_BITS	5
#define MAX_CL_RESIDENCY_BITS	3

#define APSS_LPM_COUNTER_CPUx_C1_LO_VAL		0x8000
#define APSS_LPM_COUNTER_CPUx_C2D_LO_VAL	0x8048
#define APSS_LPM_COUNTER_CPUx_C3_LO_VAL		0x8090
#define APSS_LPM_COUNTER_CPUx_C4_LO_VAL		0x80D8

#define APSS_CPU_LPM_RESIDENCY_CNTR_CFG_n	0xC004
#define APSS_CL_LPM_RESIDENCY_CNTR_CFG		0xC030

#define APSS_LPM_RESIDENCY_C2_D2_CNTR_n		0xC040
#define APSS_LPM_RESIDENCY_C3_CNTR_n		0xC090
#define APSS_LPM_RESIDENCY_C4_D4_CNTR_n		0xC0D0

/* bits are per CPU LPM_CFG register */
#define LPM_COUNTER_EN_C1	BIT(0)
#define LPM_COUNTER_EN_C2D	BIT(1)
#define LPM_COUNTER_EN_C3	BIT(2)
#define LPM_COUNTER_EN_C4	BIT(3)

/* bits are per CL LPM_CFG register */
#define LPM_COUNTER_EN_D1	BIT(0)
#define LPM_COUNTER_EN_D2D	BIT(1)
#define LPM_COUNTER_EN_D3	BIT(2)
#define LPM_COUNTER_EN_D4	BIT(3)

/* Core residency cfg as per register */
#define RESIDENCY_CNTR_C2_EN	BIT(0)
#define RESIDENCY_CNTR_C3_EN	BIT(2)
#define RESIDENCY_CNTR_C4_EN	BIT(4)

/* Cluster residency bits as per cfg register*/
#define RESIDENCY_CNTR_D2_EN	BIT(0)
#define RESIDENCY_CNTR_D4_EN	BIT(2)

struct qcom_cpuss_stats {
	char mode_name[20];
	void __iomem *reg;	/* iomapped reg */
	struct list_head node;
};

struct qcom_target_info {
	struct platform_device *pdev;
	int ncpu;
	phys_addr_t per_cpu_lpm_cfg[MAX_POSSIBLE_CPUS];
	u32 per_cpu_lpm_cfg_size[MAX_POSSIBLE_CPUS];
	phys_addr_t apss_seq_mem_base;
	u32 apss_seq_mem_size;
	phys_addr_t l3_seq_lpm_cfg;
	u32 l3_seq_lpm_size;
	struct qcom_cpuss_stats complete_stats;

	struct dentry *stats_rootdir;
	struct dentry *cpu_dir[MAX_POSSIBLE_CPUS];
	struct dentry *cl_rootdir;
};

static char *get_str_cpu_lpm_state(u8 state)
{
	switch (state) {
	case LPM_COUNTER_EN_C1:
		return "C1_count";
	case LPM_COUNTER_EN_C2D:
		return "C2D_count";
	case LPM_COUNTER_EN_C3:
		return "C3_count";
	case LPM_COUNTER_EN_C4:
		return "C4_count";
	default:
		return NULL;
	}
}

static char *get_str_cl_lpm_state(u8 state)
{
	switch (state) {
	case LPM_COUNTER_EN_D1:
		return "D1_count";
	case LPM_COUNTER_EN_D2D:
		return "D2D_count";
	case LPM_COUNTER_EN_D3:
		return "D3_count";
	case LPM_COUNTER_EN_D4:
		return "D4_count";
	default:
		return NULL;
	}
}

static char *get_str_cpu_res(u32 cfg)
{
	switch (cfg) {
	case RESIDENCY_CNTR_C2_EN:
		return "C2_residency";
	case RESIDENCY_CNTR_C3_EN:
		return "C3_residency";
	case RESIDENCY_CNTR_C4_EN:
		return "C4_residency";
	default:
		return NULL;
	}
}

static char *get_str_cl_res(u8 cfg)
{
	switch (cfg) {
	case RESIDENCY_CNTR_D2_EN:
		return "D2_residency";
	case RESIDENCY_CNTR_D4_EN:
		return "D4_residency";
	default:
		return NULL;
	}

}

static int get_cpu_lpm_read_offset(u8 cpu, u8 cpu_mode)
{
	int offset;

	switch (cpu_mode) {
	case LPM_COUNTER_EN_C1:
		offset = APSS_LPM_COUNTER_CPUx_C1_LO_VAL +  (cpu * OFFSET_8BYTES);
		break;
	case LPM_COUNTER_EN_C2D:
		offset = APSS_LPM_COUNTER_CPUx_C2D_LO_VAL +  (cpu * OFFSET_8BYTES);
		break;
	case LPM_COUNTER_EN_C3:
		offset = APSS_LPM_COUNTER_CPUx_C3_LO_VAL +  (cpu * OFFSET_8BYTES);
		break;
	case LPM_COUNTER_EN_C4:
		offset = APSS_LPM_COUNTER_CPUx_C4_LO_VAL +  (cpu * OFFSET_8BYTES);
		break;
	default:
		pr_err("Unknown mode\n");
		offset = -EINVAL;
	}

	return offset;
}

static int get_cpu_residency_read_offset(u8 cpu, u8 cpu_res_cfg)
{
	int offset;

	switch (cpu_res_cfg) {
	case RESIDENCY_CNTR_C2_EN:
		offset = APSS_LPM_RESIDENCY_C2_D2_CNTR_n + (cpu * OFFSET_8BYTES);
		break;
	case RESIDENCY_CNTR_C3_EN:
		offset = APSS_LPM_RESIDENCY_C3_CNTR_n + (cpu * OFFSET_8BYTES);
		break;
	case RESIDENCY_CNTR_C4_EN:
		offset = APSS_LPM_RESIDENCY_C4_D4_CNTR_n + (cpu * OFFSET_8BYTES);
		break;
	default:
		pr_err("Unknown mode\n");
		offset = -EINVAL;
	}

	return offset;
}

static int get_cl_residency_read_offset(int ncpu, u8 cl_res_cfg)
{
	int offset;

	switch (cl_res_cfg) {
	case RESIDENCY_CNTR_D2_EN:
		offset = APSS_LPM_RESIDENCY_C2_D2_CNTR_n + OFFSET_8BYTES * ncpu;
		break;
	case RESIDENCY_CNTR_D4_EN:
		offset = APSS_LPM_RESIDENCY_C4_D4_CNTR_n + OFFSET_8BYTES * ncpu;
		break;
	default:
		pr_err("Unknown mode\n");
		offset = -EINVAL;
	}

	return offset;
}

static int get_cl_lpm_read_offset(int ncpu, u8 cl_mode)
{
	int offset;

	switch (cl_mode) {
	case LPM_COUNTER_EN_D1:
		offset = APSS_LPM_COUNTER_CPUx_C1_LO_VAL + ncpu * OFFSET_8BYTES;
		break;
	case LPM_COUNTER_EN_D2D:
		offset = APSS_LPM_COUNTER_CPUx_C2D_LO_VAL + ncpu * OFFSET_8BYTES;
		break;
	case LPM_COUNTER_EN_D3:
		offset = APSS_LPM_COUNTER_CPUx_C3_LO_VAL + ncpu * OFFSET_8BYTES;
		break;
	case LPM_COUNTER_EN_D4:
		offset = APSS_LPM_COUNTER_CPUx_C4_LO_VAL + ncpu * OFFSET_8BYTES;
		break;
	default:
		pr_err("Unknown mode\n");
		offset = -EINVAL;
	}

	return offset;
}

static int qcom_cpuss_sleep_stats_show(struct seq_file *s, void *d)
{
	void __iomem *reg = (void __iomem *)s->private;
	u64 val;

	val = readq_relaxed(reg);
	seq_printf(s, "%ld\n", val);

	return 0;
}

DEFINE_SHOW_ATTRIBUTE(qcom_cpuss_sleep_stats);

static int qcom_cpuss_all_stats_show(struct seq_file *s, void *d)
{
	struct list_head *node1 = (struct list_head *)s->private;
	struct qcom_cpuss_stats *data;
	u64 count;

	list_for_each_entry(data, node1, node) {
		count = readq_relaxed(data->reg);
		seq_printf(s, "%s: %ld\n", data->mode_name, count);
	}

	return 0;
}

DEFINE_SHOW_ATTRIBUTE(qcom_cpuss_all_stats);

static void store_stats_data(struct qcom_target_info *t_info, char *str,
			     void __iomem *reg)
{
	struct qcom_cpuss_stats *store_stats_data;

	store_stats_data = devm_kzalloc(&t_info->pdev->dev, sizeof(*store_stats_data),
					GFP_KERNEL);
	store_stats_data->reg = reg;
	strlcpy(store_stats_data->mode_name, str,
		sizeof(store_stats_data->mode_name));

	list_add_tail(&store_stats_data->node, &t_info->complete_stats.node);
}

static int qcom_cpuss_sleep_stats_create_cpu_debugfs(struct qcom_target_info *t_info,
					     int cpu, u32 lpm_cfg)
{
	void __iomem *reg, *base;
	struct platform_device *pdev = t_info->pdev;
	u32 offset;
	int bit;
	char cpu_name[CPUNAME_SZ] = {0}, stats_name[STATS_NAME_SZ] = {0}, *state;

	snprintf(cpu_name, sizeof(cpu_name), "pcpu%u", cpu);
	t_info->cpu_dir[cpu] = debugfs_create_dir(cpu_name,
						  t_info->stats_rootdir);

	base = devm_ioremap(&pdev->dev, t_info->apss_seq_mem_base,
			    t_info->apss_seq_mem_size);
	if (!base)
		return -ENOMEM;

	for (bit = 0; bit < MAX_CPU_LPM_BITS; bit++) {
		if (lpm_cfg & BIT(bit)) {
			offset = get_cpu_lpm_read_offset(cpu, BIT(bit));
			if (offset == -EINVAL)
				return offset;

			reg = base + offset;
			state = get_str_cpu_lpm_state(1 << bit);
			if (state) {
				debugfs_create_file(state, 0444,
						    t_info->cpu_dir[cpu],
						    (void *) reg,
						    &qcom_cpuss_sleep_stats_fops);
				snprintf(stats_name, sizeof(stats_name),
					 "pcpu%u: %s", cpu, state);
				store_stats_data(t_info, stats_name, reg);
			}
		}
	}

	return 0;
}

static int qcom_cpuss_sleep_stats_create_cluster_debugfs(struct qcom_target_info *t_info,
						u32 cl_cfg)
{
	void __iomem *reg, *base;
	struct platform_device *pdev = t_info->pdev;
	u32 offset;
	int bit;
	char *state;

	t_info->cl_rootdir = debugfs_create_dir("L3", t_info->stats_rootdir);
	if (!t_info->cl_rootdir)
		return -ENOMEM;

	base = devm_ioremap(&pdev->dev, t_info->apss_seq_mem_base,
			    t_info->apss_seq_mem_size);
	if (!base)
		return -ENOMEM;

	for (bit = 0; bit < MAX_CL_LPM_BITS; bit++) {
		if (cl_cfg & BIT(bit)) {
			offset = get_cl_lpm_read_offset(t_info->ncpu, BIT(bit));
			if (offset == -EINVAL)
				return offset;

			reg = base + offset;
			state = get_str_cl_lpm_state(1 << bit);
			if (state) {
				debugfs_create_file(state, 0444,
						    t_info->cl_rootdir,
						    (void *) reg,
						    &qcom_cpuss_sleep_stats_fops);
				store_stats_data(t_info, state, reg);
			}
		}
	}

	return 0;
}

static int qcom_cpuss_sleep_stats_create_cpu_residency_debugfs(struct qcom_target_info *t_info,
						int cpu, u32 residency_cfg)
{
	void __iomem *reg, *base;
	struct platform_device *pdev = t_info->pdev;
	u32 offset;
	int bit;
	char stats_name[STATS_NAME_SZ] = {0}, *state;

	base = devm_ioremap(&pdev->dev, t_info->apss_seq_mem_base,
			    t_info->apss_seq_mem_size);
	if (!base)
		return -ENOMEM;

	for (bit = 0; bit < MAX_CPU_RESIDENCY_BITS; bit += 2) {
		if (residency_cfg & BIT(bit)) {
			offset = get_cpu_residency_read_offset(cpu, BIT(bit));
			if (offset == -EINVAL)
				return offset;

			reg = base + offset;
			state = get_str_cpu_res(1 << bit);
			if (state) {
				debugfs_create_file(state, 0444,
						    t_info->cpu_dir[cpu],
						    (void *) reg,
						    &qcom_cpuss_sleep_stats_fops);
				snprintf(stats_name, sizeof(stats_name),
					 "pcpu%u: %s", cpu, state);
				store_stats_data(t_info, stats_name, reg);
			}
		}
	}

	return 0;
}

static int qcom_cpuss_sleep_stats_create_cl_residency_debugfs(struct qcom_target_info *t_info,
						u32 cl_residency_cfg)
{
	void __iomem *reg;
	struct platform_device *pdev = t_info->pdev;
	u32 offset;
	int bit;
	char *state;

	for (bit = 0; bit < MAX_CL_RESIDENCY_BITS; bit += 2) {
		if (cl_residency_cfg & BIT(bit)) {
			offset = get_cl_residency_read_offset(t_info->ncpu,
							      BIT(bit));
			if (offset == -EINVAL)
				return offset;

		reg = devm_ioremap(&pdev->dev, t_info->apss_seq_mem_base +
				   offset, 0x4);
		if (!reg)
			return -ENOMEM;
		state = get_str_cl_res(1 << bit);
		if (state) {
			debugfs_create_file(state, 0444, t_info->cl_rootdir,
					    (void *) reg,
					    &qcom_cpuss_sleep_stats_fops);
			store_stats_data(t_info, state, reg);
		}
		}
	}

	return 0;
}

static int qcom_cpuss_read_lpm_and_residency_cfg_informaion(struct qcom_target_info *t_info)
{
	void __iomem *reg, *base;
	struct platform_device *pdev = t_info->pdev;
	u32 val;
	int i, ret;

	/* per cpu lpm and residency */
	base = devm_ioremap(&pdev->dev, t_info->apss_seq_mem_base,
			   t_info->apss_seq_mem_size);
	if (!base)
		return -ENOMEM;

	for (i = 0; i < t_info->ncpu; i++) {
		reg = devm_ioremap(&pdev->dev, t_info->per_cpu_lpm_cfg[i],
				   t_info->per_cpu_lpm_cfg_size[i]);
		val = readl_relaxed(reg);
		ret = qcom_cpuss_sleep_stats_create_cpu_debugfs(t_info, i, val);
		if (ret)
			return ret;

		reg = base + (APSS_CPU_LPM_RESIDENCY_CNTR_CFG_n + OFFSET_4BYTES * i);
		val = readl_relaxed(reg);
		ret = qcom_cpuss_sleep_stats_create_cpu_residency_debugfs(t_info, i, val);
		if (ret)
			return ret;
	}

	/* cluster lpm */
	reg = devm_ioremap(&pdev->dev, t_info->l3_seq_lpm_cfg,
			   t_info->l3_seq_lpm_size);
	if (!reg)
		return -ENOMEM;

	val = readl_relaxed(reg);
	ret = qcom_cpuss_sleep_stats_create_cluster_debugfs(t_info, val);
	if (ret)
		return ret;

	/* cluster residency */
	reg = devm_ioremap(&pdev->dev, t_info->apss_seq_mem_base,
			   t_info->apss_seq_mem_size);
	if (!reg)
		return -ENOMEM;

	reg += APSS_CL_LPM_RESIDENCY_CNTR_CFG;
	val = readl_relaxed(reg);
	ret = qcom_cpuss_sleep_stats_create_cl_residency_debugfs(t_info, val);

	return ret;
}

static int qcom_cpuss_sleep_stats_probe(struct platform_device *pdev)
{
	int ret, i;
	struct dentry *root_dir;
	struct qcom_target_info *t_info;
	struct resource *res;

	t_info = devm_kzalloc(&pdev->dev, sizeof(struct qcom_target_info),
			      GFP_KERNEL);
	if (!t_info)
		return -ENOMEM;

	INIT_LIST_HEAD(&t_info->complete_stats.node);

	root_dir = debugfs_create_dir("qcom_cpuss_sleep_stats", NULL);
	t_info->stats_rootdir = root_dir;
	t_info->pdev = pdev;

	/* Get cfg address for cpu/cluster */
	for (i = 0; i < MAX_POSSIBLE_CPUS; i++) {
		char reg_name[SEQ_LPM_STR_SZ] = {0};

		snprintf(reg_name, sizeof(reg_name), "seq_lpm_cntr_cfg_cpu%u", i);
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						   reg_name);
		if (!res)
			continue;

		t_info->per_cpu_lpm_cfg[i] = res->start;
		t_info->per_cpu_lpm_cfg_size[i] = resource_size(res);
	}

	res =  platform_get_resource_byname(pdev, IORESOURCE_MEM,
					    "apss_seq_mem_base");
	if (!res)
		return -ENODEV;

	t_info->apss_seq_mem_base = res->start;
	t_info->apss_seq_mem_size = resource_size(res);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
					   "l3_seq_lpm_cntr_cfg");
	if (!res)
		return -ENODEV;

	t_info->l3_seq_lpm_cfg = res->start;
	t_info->l3_seq_lpm_size = resource_size(res);

	of_property_read_u32(pdev->dev.of_node, "num-cpus", &t_info->ncpu);
	if (t_info->ncpu < MIN_POSSIBLE_CPUS ||  t_info->ncpu > MAX_POSSIBLE_CPUS)
		return -EINVAL;

	/*
	 * Function to read cfgs register to know lpm stats per cpu/cluster and
	 * create debugfs
	 */
	ret = qcom_cpuss_read_lpm_and_residency_cfg_informaion(t_info);
	if (ret)
		return ret;

	debugfs_create_file("stats", 0444, root_dir,
				(void *) &t_info->complete_stats.node,
				&qcom_cpuss_all_stats_fops);

	platform_set_drvdata(pdev, root_dir);

	return ret;
}

static int qcom_cpuss_sleep_stats_remove(struct platform_device *pdev)
{
	struct dentry *root = platform_get_drvdata(pdev);

	debugfs_remove_recursive(root);

	return 0;
}

static const struct of_device_id qcom_cpuss_stats_table[] = {
		{.compatible = "qcom,cpuss-sleep-stats", },
		{ },
};

static struct platform_driver qcom_cpuss_sleep_stats = {
	.probe = qcom_cpuss_sleep_stats_probe,
	.remove = qcom_cpuss_sleep_stats_remove,
	.driver	= {
		.name = "qcom_cpuss_sleep_stats",
		.of_match_table	= qcom_cpuss_stats_table,
	},
};

module_platform_driver(qcom_cpuss_sleep_stats);

MODULE_DESCRIPTION("Qualcomm Technologies, Inc. (QTI) CPUSS sleep stats driver");
MODULE_LICENSE("GPL v2");
