/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "clk: %s: " fmt, __func__

#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/cpu.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/pm_opp.h>
#include <linux/interrupt.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/cpufreq.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <dt-bindings/clock/qcom,cpucc-sdm845.h>
#include <dt-bindings/regulator/qcom,rpmh-regulator.h>

#include "common.h"
#include "clk-regmap.h"
#include "clk-voter.h"
#include "clk-debug.h"

#define OSM_INIT_RATE			300000000UL
#define XO_RATE				19200000UL
#define OSM_TABLE_SIZE			40
#define SINGLE_CORE			1
#define MAX_CLUSTER_CNT			3
#define MAX_MEM_ACC_VAL_PER_LEVEL	3
#define CORE_COUNT_VAL(val)		((val & GENMASK(18, 16)) >> 16)
#define CURRENT_LVAL(val)		((val & GENMASK(23, 16)) >> 16)

#define OSM_REG_SIZE			32

#define ENABLE_REG			0x0
#define FREQ_REG			0x110
#define VOLT_REG			0x114
#define CORE_DCVS_CTRL			0xbc
#define PSTATE_STATUS			0x700

#define DCVS_PERF_STATE_DESIRED_REG_0_V1	0x780
#define DCVS_PERF_STATE_DESIRED_REG_0_V2	0x920
#define DCVS_PERF_STATE_DESIRED_REG(n, v1) \
	(((v1) ? DCVS_PERF_STATE_DESIRED_REG_0_V1 \
		: DCVS_PERF_STATE_DESIRED_REG_0_V2) + 4 * (n))

#define OSM_CYCLE_COUNTER_STATUS_REG_0_V1	0x7d0
#define OSM_CYCLE_COUNTER_STATUS_REG_0_V2	0x9c0
#define OSM_CYCLE_COUNTER_STATUS_REG(n, v1) \
	(((v1) ? OSM_CYCLE_COUNTER_STATUS_REG_0_V1 \
		: OSM_CYCLE_COUNTER_STATUS_REG_0_V2) + 4 * (n))

static DEFINE_VDD_REGS_INIT(vdd_l3_mx_ao, 1);
static DEFINE_VDD_REGS_INIT(vdd_pwrcl_mx_ao, 1);

struct osm_entry {
	u16 virtual_corner;
	u16 open_loop_volt;
	unsigned long frequency;
	u32 lval;
	u16 ccount;
};

struct clk_osm {
	struct clk_hw hw;
	struct osm_entry osm_table[OSM_TABLE_SIZE];
	struct dentry *debugfs;
	void __iomem *vbase;
	phys_addr_t pbase;
	spinlock_t lock;
	bool per_core_dcvs;
	u32 num_entries;
	u32 cluster_num;
	u32 core_num;
	unsigned long rate;
	u64 total_cycle_counter;
	u32 prev_cycle_counter;
	u32 max_core_count;
	u32 mx_turbo_freq;
};

static bool is_sdm845v1;

static inline struct clk_osm *to_clk_osm(struct clk_hw *_hw)
{
	return container_of(_hw, struct clk_osm, hw);
}

static inline void clk_osm_write_reg(struct clk_osm *c, u32 val, u32 offset)
{
	writel_relaxed(val, c->vbase + offset);
}

static inline int clk_osm_read_reg(struct clk_osm *c, u32 offset)
{
	return readl_relaxed(c->vbase + offset);
}

static inline int clk_osm_read_reg_no_log(struct clk_osm *c, u32 offset)
{
	return readl_relaxed_no_log(c->vbase + offset);
}

static inline int clk_osm_mb(struct clk_osm *c)
{
	return readl_relaxed_no_log(c->vbase + ENABLE_REG);
}

static long clk_osm_list_rate(struct clk_hw *hw, unsigned int n,
					unsigned long rate_max)
{
	if (n >= hw->init->num_rate_max)
		return -ENXIO;
	return hw->init->rate_max[n];
}

static inline bool is_better_rate(unsigned long req, unsigned long best,
			unsigned long new)
{
	if (IS_ERR_VALUE(new))
		return false;

	return (req <= new && new < best) || (best < req && best < new);
}

static int clk_osm_search_table(struct osm_entry *table, int entries,
				unsigned long rate)
{
	int index;

	for (index = 0; index < entries; index++) {
		if (rate == table[index].frequency)
			return index;
	}

	return -EINVAL;
}

static int clk_osm_set_rate(struct clk_hw *hw, unsigned long rate,
				unsigned long parent_rate)
{
	struct clk_osm *c = to_clk_osm(hw);

	c->rate = rate;

	return 0;
}

static unsigned long clk_osm_recalc_rate(struct clk_hw *hw,
				unsigned long parent_rate)
{
	struct clk_osm *c = to_clk_osm(hw);

	return c->rate;
}

static int clk_osm_determine_rate(struct clk_hw *hw,
				struct clk_rate_request *req)
{
	int i;
	unsigned long rrate = 0;
	unsigned long rate = req->rate;

	/*
	 * If the rate passed in is 0, return the first frequency in the
	 * FMAX table.
	 */
	if (!rate) {
		req->rate = hw->init->rate_max[0];
		return 0;
	}

	for (i = 0; i < hw->init->num_rate_max; i++) {
		if (is_better_rate(rate, rrate, hw->init->rate_max[i])) {
			rrate = hw->init->rate_max[i];
			if (rate == rrate)
				break;
		}
	}

	req->rate = rrate;

	pr_debug("clk:%s rate %lu, rrate %lu, Rate max %lu index %u\n",
			hw->init->name, rate, rrate, hw->init->rate_max[i], i);

	return 0;
}

static int clk_cpu_set_rate(struct clk_hw *hw, unsigned long rate,
						unsigned long parent_rate)
{
	struct clk_osm *c = to_clk_osm(hw);
	struct clk_hw *p_hw = clk_hw_get_parent(hw);
	struct clk_osm *parent = to_clk_osm(p_hw);
	int core_num, index = 0;

	if (!c || !parent)
		return -EINVAL;

	index = clk_osm_search_table(parent->osm_table,
					parent->num_entries, rate);
	if (index < 0) {
		pr_err("cannot set %s to %lu\n", clk_hw_get_name(hw), rate);
		return -EINVAL;
	}

	core_num = parent->per_core_dcvs ? c->core_num : 0;
	clk_osm_write_reg(parent, index,
				DCVS_PERF_STATE_DESIRED_REG(core_num,
							is_sdm845v1));

	/* Make sure the write goes through before proceeding */
	clk_osm_mb(parent);

	return 0;
}

static int clk_pwrcl_set_rate(struct clk_hw *hw, unsigned long rate,
						unsigned long parent_rate)
{
	struct clk_hw *p_hw = clk_hw_get_parent(hw);
	struct clk_osm *parent = to_clk_osm(p_hw);
	int ret, index = 0, count = 40;
	u32 curr_lval;

	ret = clk_cpu_set_rate(hw, rate, parent_rate);
	if (ret)
		return ret;

	index = clk_osm_search_table(parent->osm_table,
					parent->num_entries, rate);
	if (index < 0)
		return -EINVAL;

	/*
	 * Poll the CURRENT_FREQUENCY value of the PSTATE_STATUS register to
	 * check if the L_VAL has been updated.
	 */
	while (count-- > 0) {
		curr_lval = CURRENT_LVAL(clk_osm_read_reg(parent,
								PSTATE_STATUS));
		if (curr_lval <= parent->osm_table[index].lval)
			return 0;
		udelay(50);
	}
	pr_err("cannot set %s to %lu\n", clk_hw_get_name(hw), rate);
	return -ETIMEDOUT;
}

static unsigned long clk_cpu_recalc_rate(struct clk_hw *hw,
					unsigned long parent_rate)
{
	struct clk_osm *c = to_clk_osm(hw);
	struct clk_hw *p_hw = clk_hw_get_parent(hw);
	struct clk_osm *parent = to_clk_osm(p_hw);
	int core_num, index = 0;

	if (!c || !parent)
		return -EINVAL;

	core_num = parent->per_core_dcvs ? c->core_num : 0;
	index = clk_osm_read_reg(parent,
				DCVS_PERF_STATE_DESIRED_REG(core_num,
							is_sdm845v1));
	return parent->osm_table[index].frequency;
}

static int clk_cpu_determine_rate(struct clk_hw *hw,
				struct clk_rate_request *req)
{
	struct clk_hw *parent_hw = clk_hw_get_parent(hw);
	unsigned long rate = req->rate;

	req->rate = clk_hw_round_rate(parent_hw, rate);

	return 0;
}

static int l3_clk_set_rate(struct clk_hw *hw, unsigned long rate,
				    unsigned long parent_rate)
{
	struct clk_osm *cpuclk = to_clk_osm(hw);
	struct clk_rate_request req;
	int index = 0;
	int count = 40;
	u32 curr_lval;

	if (!cpuclk)
		return -EINVAL;

	req.rate = rate;
	clk_osm_determine_rate(hw, &req);

	if (rate != req.rate) {
		pr_err("invalid requested rate=%ld\n", rate);
		return -EINVAL;
	}

	/* Convert rate to table index */
	index = clk_osm_search_table(cpuclk->osm_table,
				     cpuclk->num_entries, req.rate);
	if (index < 0) {
		pr_err("cannot set %s to %lu\n", clk_hw_get_name(hw), rate);
		return -EINVAL;
	}
	pr_debug("rate: %lu --> index %d\n", rate, index);

	clk_osm_write_reg(cpuclk, index,
				DCVS_PERF_STATE_DESIRED_REG(0, is_sdm845v1));

	/* Make sure the write goes through before proceeding */
	clk_osm_mb(cpuclk);

	/*
	 * Poll the CURRENT_FREQUENCY value of the PSTATE_STATUS register to
	 * check if the L_VAL has been updated.
	 */
	if (cpuclk->rate >= cpuclk->mx_turbo_freq &&
					rate < cpuclk->mx_turbo_freq) {
		while (count-- > 0) {
			curr_lval = CURRENT_LVAL(clk_osm_read_reg(cpuclk,
								PSTATE_STATUS));
			if (curr_lval <= cpuclk->osm_table[index].lval) {
				cpuclk->rate = rate;
				return 0;
			}
			udelay(50);
		}
		pr_err("cannot set %s to %lu\n", clk_hw_get_name(hw), rate);
		return -ETIMEDOUT;
	}
	cpuclk->rate = rate;
	return 0;
}

static unsigned long l3_clk_recalc_rate(struct clk_hw *hw,
					unsigned long parent_rate)
{
	struct clk_osm *cpuclk = to_clk_osm(hw);
	int index = 0;

	if (!cpuclk)
		return -EINVAL;

	index = clk_osm_read_reg(cpuclk,
				DCVS_PERF_STATE_DESIRED_REG(0, is_sdm845v1));

	pr_debug("%s: Index %d, freq %ld\n", __func__, index,
				cpuclk->osm_table[index].frequency);

	/* Convert index to frequency */
	return cpuclk->osm_table[index].frequency;
}

static const struct clk_ops clk_ops_l3_osm = {
	.determine_rate = clk_osm_determine_rate,
	.list_rate = clk_osm_list_rate,
	.recalc_rate = l3_clk_recalc_rate,
	.set_rate = l3_clk_set_rate,
	.debug_init = clk_debug_measure_add,
};

static const struct clk_ops clk_ops_pwrcl_core = {
	.set_rate = clk_pwrcl_set_rate,
	.determine_rate = clk_cpu_determine_rate,
	.recalc_rate = clk_cpu_recalc_rate,
	.debug_init = clk_debug_measure_add,
};

static const struct clk_ops clk_ops_perfcl_core = {
	.set_rate = clk_cpu_set_rate,
	.determine_rate = clk_cpu_determine_rate,
	.recalc_rate = clk_cpu_recalc_rate,
	.debug_init = clk_debug_measure_add,
};

static const struct clk_ops clk_ops_cpu_osm = {
	.set_rate = clk_osm_set_rate,
	.recalc_rate = clk_osm_recalc_rate,
	.list_rate = clk_osm_list_rate,
	.determine_rate = clk_osm_determine_rate,
	.debug_init = clk_debug_measure_add,
};

static struct clk_init_data osm_clks_init[] = {
	[0] = {
		.name = "l3_clk",
		.parent_names = (const char *[]){ "bi_tcxo_ao" },
		.num_parents = 1,
		.flags = CLK_CHILD_NO_RATE_PROP,
		.ops = &clk_ops_l3_osm,
		.vdd_class = &vdd_l3_mx_ao,
	},
	[1] = {
		.name = "pwrcl_clk",
		.parent_names = (const char *[]){ "bi_tcxo_ao" },
		.num_parents = 1,
		.ops = &clk_ops_cpu_osm,
		.vdd_class = &vdd_pwrcl_mx_ao,
	},
	[2] = {
		.name = "perfcl_clk",
		.parent_names = (const char *[]){ "bi_tcxo_ao" },
		.num_parents = 1,
		.ops = &clk_ops_cpu_osm,
	},
};

static struct clk_osm l3_clk = {
	.cluster_num = 0,
	.max_core_count = 4,
	.hw.init = &osm_clks_init[0],
};

static DEFINE_CLK_VOTER(l3_cluster0_vote_clk, l3_clk, 0);
static DEFINE_CLK_VOTER(l3_cluster1_vote_clk, l3_clk, 0);
static DEFINE_CLK_VOTER(l3_misc_vote_clk, l3_clk, 0);
static DEFINE_CLK_VOTER(l3_gpu_vote_clk, l3_clk, 0);

static struct clk_osm pwrcl_clk = {
	.cluster_num = 1,
	.max_core_count = 4,
	.hw.init = &osm_clks_init[1],
};

static struct clk_osm cpu0_pwrcl_clk = {
	.core_num = 0,
	.total_cycle_counter = 0,
	.prev_cycle_counter = 0,
	.hw.init = &(struct clk_init_data){
		.name = "cpu0_pwrcl_clk",
		.parent_names = (const char *[]){ "pwrcl_clk" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_ops_pwrcl_core,
	},
};

static struct clk_osm cpu1_pwrcl_clk = {
	.core_num = 1,
	.total_cycle_counter = 0,
	.prev_cycle_counter = 0,
	.hw.init = &(struct clk_init_data){
		.name = "cpu1_pwrcl_clk",
		.parent_names = (const char *[]){ "pwrcl_clk" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_ops_pwrcl_core,
	},
};

static struct clk_osm cpu2_pwrcl_clk = {
	.core_num = 2,
	.total_cycle_counter = 0,
	.prev_cycle_counter = 0,
	.hw.init = &(struct clk_init_data){
		.name = "cpu2_pwrcl_clk",
		.parent_names = (const char *[]){ "pwrcl_clk" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_ops_pwrcl_core,
	},
};

static struct clk_osm cpu3_pwrcl_clk = {
	.core_num = 3,
	.total_cycle_counter = 0,
	.prev_cycle_counter = 0,
	.hw.init = &(struct clk_init_data){
		.name = "cpu3_pwrcl_clk",
		.parent_names = (const char *[]){ "pwrcl_clk" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_ops_pwrcl_core,
	},
};

static struct clk_osm cpu4_pwrcl_clk = {
	.core_num = 4,
	.total_cycle_counter = 0,
	.prev_cycle_counter = 0,
	.hw.init = &(struct clk_init_data){
		.name = "cpu4_pwrcl_clk",
		.parent_names = (const char *[]){ "pwrcl_clk" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_ops_pwrcl_core,
	},
};

static struct clk_osm cpu5_pwrcl_clk = {
	.core_num = 5,
	.total_cycle_counter = 0,
	.prev_cycle_counter = 0,
	.hw.init = &(struct clk_init_data){
		.name = "cpu5_pwrcl_clk",
		.parent_names = (const char *[]){ "pwrcl_clk" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_ops_pwrcl_core,
	},
};

static struct clk_osm perfcl_clk = {
	.cluster_num = 2,
	.max_core_count = 4,
	.hw.init = &osm_clks_init[2],
};


static struct clk_osm cpu4_perfcl_clk = {
	.core_num = 0,
	.total_cycle_counter = 0,
	.prev_cycle_counter = 0,
	.hw.init = &(struct clk_init_data){
		.name = "cpu4_perfcl_clk",
		.parent_names = (const char *[]){ "perfcl_clk" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_ops_perfcl_core,
	},
};

static struct clk_osm cpu5_perfcl_clk = {
	.core_num = 1,
	.total_cycle_counter = 0,
	.prev_cycle_counter = 0,
	.hw.init = &(struct clk_init_data){
		.name = "cpu5_perfcl_clk",
		.parent_names = (const char *[]){ "perfcl_clk" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_ops_perfcl_core,
	},
};

static struct clk_osm cpu6_perfcl_clk = {
	.core_num = 2,
	.total_cycle_counter = 0,
	.prev_cycle_counter = 0,
	.hw.init = &(struct clk_init_data){
		.name = "cpu6_perfcl_clk",
		.parent_names = (const char *[]){ "perfcl_clk" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_ops_perfcl_core,
	},
};

static struct clk_osm cpu7_perfcl_clk = {
	.core_num = 3,
	.total_cycle_counter = 0,
	.prev_cycle_counter = 0,
	.hw.init = &(struct clk_init_data){
		.name = "cpu7_perfcl_clk",
		.parent_names = (const char *[]){ "perfcl_clk" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_ops_perfcl_core,
	},
};

static struct clk_hw *osm_qcom_clk_hws[] = {
	[L3_CLK] = &l3_clk.hw,
	[L3_CLUSTER0_VOTE_CLK] = &l3_cluster0_vote_clk.hw,
	[L3_CLUSTER1_VOTE_CLK] = &l3_cluster1_vote_clk.hw,
	[L3_MISC_VOTE_CLK] = &l3_misc_vote_clk.hw,
	[L3_GPU_VOTE_CLK] = &l3_gpu_vote_clk.hw,
	[PWRCL_CLK] = &pwrcl_clk.hw,
	[CPU0_PWRCL_CLK] = &cpu0_pwrcl_clk.hw,
	[CPU1_PWRCL_CLK] = &cpu1_pwrcl_clk.hw,
	[CPU2_PWRCL_CLK] = &cpu2_pwrcl_clk.hw,
	[CPU3_PWRCL_CLK] = &cpu3_pwrcl_clk.hw,
	[PERFCL_CLK] = &perfcl_clk.hw,
	[CPU4_PERFCL_CLK] = &cpu4_perfcl_clk.hw,
	[CPU5_PERFCL_CLK] = &cpu5_perfcl_clk.hw,
	[CPU6_PERFCL_CLK] = &cpu6_perfcl_clk.hw,
	[CPU7_PERFCL_CLK] = &cpu7_perfcl_clk.hw,
	[CPU4_PWRCL_CLK] = NULL,
	[CPU5_PWRCL_CLK] = NULL,
};

static struct clk_osm *clk_cpu_map[] = {
	&cpu0_pwrcl_clk,
	&cpu1_pwrcl_clk,
	&cpu2_pwrcl_clk,
	&cpu3_pwrcl_clk,
	&cpu4_perfcl_clk,
	&cpu5_perfcl_clk,
	&cpu6_perfcl_clk,
	&cpu7_perfcl_clk,
};

static struct clk_osm *logical_cpu_to_clk(int cpu)
{
	struct device_node *cpu_node;
	const u32 *cell;
	u64 hwid;
	static struct clk_osm *cpu_clk_map[NR_CPUS];

	if (!cpu_clk_map[cpu]) {
		cpu_node = of_get_cpu_node(cpu, NULL);
		if (!cpu_node)
			return NULL;

		cell = of_get_property(cpu_node, "reg", NULL);
		if (!cell) {
			pr_err("%s: missing reg property\n",
			       cpu_node->full_name);
			of_node_put(cpu_node);
			return NULL;
		}

		hwid = of_read_number(cell, of_n_addr_cells(cpu_node));
		hwid = (hwid >> 8) & 0xff;
		of_node_put(cpu_node);
		if (hwid >= ARRAY_SIZE(clk_cpu_map)) {
			pr_err("unsupported CPU number - %d (hw_id - %llu)\n",
			       cpu, hwid);
			return NULL;
		}

		cpu_clk_map[cpu] = clk_cpu_map[hwid];
	}

	return cpu_clk_map[cpu];
}

static struct clk_osm *osm_configure_policy(struct cpufreq_policy *policy)
{
	int cpu;
	struct clk_hw *parent, *c_parent;
	struct clk_osm *first;
	struct clk_osm *c, *n;

	c = logical_cpu_to_clk(policy->cpu);
	if (!c)
		return NULL;

	c_parent = clk_hw_get_parent(&c->hw);
	if (!c_parent)
		return NULL;

	/*
	 * Don't put any other CPUs into the policy if we're doing
	 * per_core_dcvs
	 */
	if (to_clk_osm(c_parent)->per_core_dcvs)
		return c;

	first = c;
	/* Find CPUs that share the same clock domain */
	for_each_possible_cpu(cpu) {
		n = logical_cpu_to_clk(cpu);
		if (!n)
			continue;

		parent = clk_hw_get_parent(&n->hw);
		if (!parent)
			return NULL;
		if (parent != c_parent)
			continue;

		cpumask_set_cpu(cpu, policy->cpus);
		if (n->core_num == 0)
			first = n;
	}

	return first;
}

static void
osm_set_index(struct clk_osm *c, unsigned int index)
{
	struct clk_hw *p_hw = clk_hw_get_parent(&c->hw);
	struct clk_osm *parent = to_clk_osm(p_hw);
	unsigned long rate = 0;

	if (index >= OSM_TABLE_SIZE) {
		pr_err("Passing an index (%u) that's greater than max (%d)\n",
					index, OSM_TABLE_SIZE - 1);
		return;
	}

	rate = parent->osm_table[index].frequency;
	if (!rate)
		return;

	clk_set_rate(c->hw.clk, clk_round_rate(c->hw.clk, rate));
}

static int
osm_cpufreq_target_index(struct cpufreq_policy *policy, unsigned int index)
{
	struct clk_osm *c = policy->driver_data;

	osm_set_index(c, index);
	return 0;
}

static unsigned int osm_cpufreq_get(unsigned int cpu)
{
	struct cpufreq_policy *policy = cpufreq_cpu_get_raw(cpu);
	struct clk_osm *c;
	u32 index;

	if (!policy)
		return 0;

	c = policy->driver_data;
	index = clk_osm_read_reg(c,
			DCVS_PERF_STATE_DESIRED_REG(c->core_num, is_sdm845v1));
	return policy->freq_table[index].frequency;
}

static int osm_cpufreq_cpu_init(struct cpufreq_policy *policy)
{
	struct cpufreq_frequency_table *table;
	struct clk_osm *c, *parent;
	struct clk_hw *p_hw;
	int ret;
	unsigned int i, prev_cc = 0;
	unsigned int xo_kHz;

	c = osm_configure_policy(policy);
	if (!c) {
		pr_err("no clock for CPU%d\n", policy->cpu);
		return -ENODEV;
	}

	p_hw = clk_hw_get_parent(&c->hw);
	if (!p_hw) {
		pr_err("no parent clock for CPU%d\n", policy->cpu);
		return -ENODEV;
	}

	parent = to_clk_osm(p_hw);
	c->vbase = parent->vbase;

	p_hw = clk_hw_get_parent(p_hw);
	if (!p_hw) {
		pr_err("no xo clock for CPU%d\n", policy->cpu);
		return -ENODEV;
	}
	xo_kHz = clk_hw_get_rate(p_hw) / 1000;

	table = kcalloc(OSM_TABLE_SIZE + 1, sizeof(*table), GFP_KERNEL);
	if (!table)
		return -ENOMEM;

	for (i = 0; i < OSM_TABLE_SIZE; i++) {
		u32 data, src, div, lval, core_count;

		data = clk_osm_read_reg(c, FREQ_REG + i * OSM_REG_SIZE);
		src = (data & GENMASK(31, 30)) >> 30;
		div = (data & GENMASK(29, 28)) >> 28;
		lval = data & GENMASK(7, 0);
		core_count = CORE_COUNT_VAL(data);

		if (!src)
			table[i].frequency = OSM_INIT_RATE / 1000;
		else
			table[i].frequency = xo_kHz * lval;
		table[i].driver_data = table[i].frequency;

		if (core_count != parent->max_core_count)
			table[i].frequency = CPUFREQ_ENTRY_INVALID;

		/*
		 * Two of the same frequencies with the same core counts means
		 * end of table.
		 */
		if (i > 0 && table[i - 1].driver_data == table[i].driver_data
					&& prev_cc == core_count) {
			struct cpufreq_frequency_table *prev = &table[i - 1];

			if (prev->frequency == CPUFREQ_ENTRY_INVALID) {
				prev->flags = CPUFREQ_BOOST_FREQ;
				prev->frequency = prev->driver_data;
			}

			break;
		}
		prev_cc = core_count;
	}
	table[i].frequency = CPUFREQ_TABLE_END;

	ret = cpufreq_table_validate_and_show(policy, table);
	if (ret) {
		pr_err("%s: invalid frequency table: %d\n", __func__, ret);
		goto err;
	}

	policy->driver_data = c;
	return 0;

err:
	kfree(table);
	return ret;
}

static int osm_cpufreq_cpu_exit(struct cpufreq_policy *policy)
{
	kfree(policy->freq_table);
	policy->freq_table = NULL;
	return 0;
}

static struct freq_attr *osm_cpufreq_attr[] = {
	&cpufreq_freq_attr_scaling_available_freqs,
	&cpufreq_freq_attr_scaling_boost_freqs,
	NULL
};

static struct cpufreq_driver qcom_osm_cpufreq_driver = {
	.flags		= CPUFREQ_STICKY | CPUFREQ_NEED_INITIAL_FREQ_CHECK |
			  CPUFREQ_HAVE_GOVERNOR_PER_POLICY,
	.verify		= cpufreq_generic_frequency_table_verify,
	.target_index	= osm_cpufreq_target_index,
	.get		= osm_cpufreq_get,
	.init		= osm_cpufreq_cpu_init,
	.exit		= osm_cpufreq_cpu_exit,
	.name		= "osm-cpufreq",
	.attr		= osm_cpufreq_attr,
	.boost_enabled	= true,
};

static u32 find_voltage(struct clk_osm *c, unsigned long rate)
{
	struct osm_entry *table = c->osm_table;
	int entries = c->num_entries, i;

	for (i = 0; i < entries; i++) {
		if (rate == table[i].frequency) {
			/* OPP table voltages have units of mV */
			return table[i].open_loop_volt * 1000;
		}
	}

	return -EINVAL;
}

static int add_opp(struct clk_osm *c, struct device **device_list, int count)
{
	unsigned long rate = 0;
	u32 uv;
	long rc;
	int i, j = 0;
	unsigned long min_rate = c->hw.init->rate_max[0];
	unsigned long max_rate =
			c->hw.init->rate_max[c->hw.init->num_rate_max - 1];

	while (1) {
		rate = c->hw.init->rate_max[j++];
		uv = find_voltage(c, rate);
		if (uv <= 0) {
			pr_warn("No voltage for %lu.\n", rate);
			return -EINVAL;
		}

		for (i = 0; i < count; i++) {
			rc = dev_pm_opp_add(device_list[i], rate, uv);
			if (rc) {
				pr_warn("failed to add OPP for %lu\n", rate);
				return rc;
			}
		}

		/*
		 * Print the OPP pair for the lowest and highest frequency for
		 * each device that we're populating. This is important since
		 * this information will be used by thermal mitigation and the
		 * scheduler.
		 */
		if (rate == min_rate) {
			for (i = 0; i < count; i++) {
				pr_info("Set OPP pair (%lu Hz, %d uv) on %s\n",
					rate, uv, dev_name(device_list[i]));
			}
		}

		if (rate == max_rate && max_rate != min_rate) {
			for (i = 0; i < count; i++) {
				pr_info("Set OPP pair (%lu Hz, %d uv) on %s\n",
					rate, uv, dev_name(device_list[i]));
			}
			break;
		}

		if (min_rate == max_rate)
			break;
	}
	return 0;
}

static int derive_device_list(struct device **device_list,
				struct device_node *np,
				char *phandle_name, int count)
{
	int i;
	struct platform_device *pdev;
	struct device_node *dev_node;

	for (i = 0; i < count; i++) {
		dev_node = of_parse_phandle(np, phandle_name, i);
		if (!dev_node) {
			pr_err("Unable to get device_node pointer for opp-handle (%s)\n",
					phandle_name);
			return -ENODEV;
		}

		pdev = of_find_device_by_node(dev_node);
		if (!pdev) {
			pr_err("Unable to find platform_device node for opp-handle (%s)\n",
						phandle_name);
			return -ENODEV;
		}
		device_list[i] = &pdev->dev;
	}
	return 0;
}

static void populate_l3_opp_table(struct device_node *np, char *phandle_name)
{
	struct device **device_list;
	int len, count, ret = 0;

	if (of_find_property(np, phandle_name, &len)) {
		count = len / sizeof(u32);

		device_list = kcalloc(count, sizeof(struct device *),
							GFP_KERNEL);
		if (!device_list)
			return;

		ret = derive_device_list(device_list, np, phandle_name, count);
		if (ret < 0) {
			pr_err("Failed to fill device_list for %s\n",
							phandle_name);
			return;
		}
	} else {
		pr_debug("Unable to find %s\n", phandle_name);
		return;
	}

	if (add_opp(&l3_clk, device_list, count))
		pr_err("Failed to add OPP levels for %s\n", phandle_name);

	kfree(device_list);
}

static void populate_opp_table(struct platform_device *pdev)
{
	int cpu;
	struct device *cpu_dev;
	struct clk_osm *c, *parent;
	struct clk_hw *hw_parent;
	struct device_node *np = pdev->dev.of_node;

	for_each_possible_cpu(cpu) {
		c = logical_cpu_to_clk(cpu);
		if (!c) {
			pr_err("no clock device for CPU=%d\n", cpu);
			return;
		}

		hw_parent = clk_hw_get_parent(&c->hw);
		parent = to_clk_osm(hw_parent);
		cpu_dev = get_cpu_device(cpu);
		if (cpu_dev)
			if (add_opp(parent, &cpu_dev, 1))
				pr_err("Failed to add OPP levels for %s\n",
					dev_name(cpu_dev));
	}

	populate_l3_opp_table(np, "l3-devs");
}

static u64 clk_osm_get_cpu_cycle_counter(int cpu)
{
	u32 val;
	int core_num;
	unsigned long flags;
	u64 cycle_counter_ret;
	struct clk_osm *parent, *c = logical_cpu_to_clk(cpu);

	if (IS_ERR_OR_NULL(c)) {
		pr_err("no clock device for CPU=%d\n", cpu);
		return 0;
	}

	parent = to_clk_osm(clk_hw_get_parent(&c->hw));

	spin_lock_irqsave(&parent->lock, flags);
	/*
	 * Use core 0's copy as proxy for the whole cluster when per
	 * core DCVS is disabled.
	 */
	core_num = parent->per_core_dcvs ? c->core_num : 0;
	val = clk_osm_read_reg_no_log(parent,
			OSM_CYCLE_COUNTER_STATUS_REG(core_num, is_sdm845v1));

	if (val < c->prev_cycle_counter) {
		/* Handle counter overflow */
		c->total_cycle_counter += UINT_MAX -
			c->prev_cycle_counter + val;
		c->prev_cycle_counter = val;
	} else {
		c->total_cycle_counter += val - c->prev_cycle_counter;
		c->prev_cycle_counter = val;
	}
	cycle_counter_ret = c->total_cycle_counter;
	spin_unlock_irqrestore(&parent->lock, flags);

	return cycle_counter_ret;
}

static int clk_osm_read_lut(struct platform_device *pdev, struct clk_osm *c)
{
	u32 data, src, lval, i, j = OSM_TABLE_SIZE;
	struct clk_vdd_class *vdd = osm_clks_init[c->cluster_num].vdd_class;

	for (i = 0; i < OSM_TABLE_SIZE; i++) {
		data = clk_osm_read_reg(c, FREQ_REG + i * OSM_REG_SIZE);
		src = ((data & GENMASK(31, 30)) >> 30);
		lval = (data & GENMASK(7, 0));
		c->osm_table[i].ccount = CORE_COUNT_VAL(data);
		c->osm_table[i].lval = lval;

		if (!src)
			c->osm_table[i].frequency = OSM_INIT_RATE;
		else
			c->osm_table[i].frequency = XO_RATE * lval;

		data = clk_osm_read_reg(c, VOLT_REG + i * OSM_REG_SIZE);
		c->osm_table[i].virtual_corner =
					((data & GENMASK(21, 16)) >> 16);
		c->osm_table[i].open_loop_volt = (data & GENMASK(11, 0));

		pr_debug("index=%d freq=%ld virtual_corner=%d open_loop_voltage=%u\n",
			 i, c->osm_table[i].frequency,
			 c->osm_table[i].virtual_corner,
			 c->osm_table[i].open_loop_volt);

		if (i > 0 && j == OSM_TABLE_SIZE &&
				c->osm_table[i].frequency ==
					c->osm_table[i - 1].frequency &&
			c->osm_table[i].ccount == c->osm_table[i - 1].ccount)
			j = i;
	}

	osm_clks_init[c->cluster_num].rate_max = devm_kcalloc(&pdev->dev,
						 j, sizeof(unsigned long),
						       GFP_KERNEL);
	if (!osm_clks_init[c->cluster_num].rate_max)
		return -ENOMEM;

	if (vdd) {
		vdd->level_votes = devm_kcalloc(&pdev->dev, j,
					sizeof(*vdd->level_votes), GFP_KERNEL);
		if (!vdd->level_votes)
			return -ENOMEM;

		vdd->vdd_uv = devm_kcalloc(&pdev->dev, j, sizeof(*vdd->vdd_uv),
								GFP_KERNEL);
		if (!vdd->vdd_uv)
			return -ENOMEM;

		for (i = 0; i < j; i++) {
			if (c->osm_table[i].frequency < c->mx_turbo_freq)
				vdd->vdd_uv[i] = RPMH_REGULATOR_LEVEL_NOM;
			else
				vdd->vdd_uv[i] = RPMH_REGULATOR_LEVEL_TURBO;
		}
		vdd->num_levels = j;
		vdd->cur_level = j;
		vdd->use_max_uV = true;
	}

	for (i = 0; i < j; i++)
		osm_clks_init[c->cluster_num].rate_max[i] =
					c->osm_table[i].frequency;

	c->num_entries = osm_clks_init[c->cluster_num].num_rate_max = j;
	return 0;
}

static int clk_osm_resources_init(struct platform_device *pdev)
{
	struct resource *res;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"osm_l3_base");
	if (!res) {
		dev_err(&pdev->dev,
			"Unable to get platform resource for osm_l3_base");
		return -ENOMEM;
	}

	l3_clk.pbase = (unsigned long)res->start;
	l3_clk.vbase = devm_ioremap(&pdev->dev, res->start, resource_size(res));

	if (!l3_clk.vbase) {
		dev_err(&pdev->dev, "Unable to map osm_l3_base base\n");
		return -ENOMEM;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"osm_pwrcl_base");
	if (!res) {
		dev_err(&pdev->dev,
			"Unable to get platform resource for osm_pwrcl_base");
		return -ENOMEM;
	}

	pwrcl_clk.pbase = (unsigned long)res->start;
	pwrcl_clk.vbase = devm_ioremap(&pdev->dev, res->start,
						  resource_size(res));
	if (!pwrcl_clk.vbase) {
		dev_err(&pdev->dev, "Unable to map osm_pwrcl_base base\n");
		return -ENOMEM;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"osm_perfcl_base");
	if (!res) {
		dev_err(&pdev->dev,
			"Unable to get platform resource for osm_perfcl_base");
		return -ENOMEM;
	}

	perfcl_clk.pbase = (unsigned long)res->start;
	perfcl_clk.vbase = devm_ioremap(&pdev->dev, res->start,
						  resource_size(res));

	if (!perfcl_clk.vbase) {
		dev_err(&pdev->dev, "Unable to map osm_perfcl_base base\n");
		return -ENOMEM;
	}

	return 0;
}

static void clk_cpu_osm_driver_sdm670_fixup(void)
{
	osm_qcom_clk_hws[CPU4_PERFCL_CLK] = NULL;
	osm_qcom_clk_hws[CPU5_PERFCL_CLK] = NULL;
	osm_qcom_clk_hws[CPU4_PWRCL_CLK] = &cpu4_pwrcl_clk.hw;
	osm_qcom_clk_hws[CPU5_PWRCL_CLK] = &cpu5_pwrcl_clk.hw;

	clk_cpu_map[4] = &cpu4_pwrcl_clk;
	clk_cpu_map[5] = &cpu5_pwrcl_clk;

	cpu6_perfcl_clk.core_num = 0;
	cpu7_perfcl_clk.core_num = 1;

	pwrcl_clk.max_core_count = 6;
	perfcl_clk.max_core_count = 2;
}

/* Request MX supply if configured in device tree */
static int clk_cpu_osm_request_mx_supply(struct device *dev)
{
	u32 *array;
	int rc;

	if (!of_find_property(dev->of_node, "qcom,mx-turbo-freq", NULL)) {
		osm_clks_init[l3_clk.cluster_num].vdd_class = NULL;
		osm_clks_init[pwrcl_clk.cluster_num].vdd_class = NULL;
		return 0;
	}

	vdd_l3_mx_ao.regulator[0] = devm_regulator_get(dev, "vdd_l3_mx_ao");
	if (IS_ERR(vdd_l3_mx_ao.regulator[0])) {
		rc = PTR_ERR(vdd_l3_mx_ao.regulator[0]);
		if (rc != -EPROBE_DEFER)
			dev_err(dev, "Unable to get vdd_l3_mx_ao regulator, rc=%d\n",
				rc);
		return rc;
	}

	vdd_pwrcl_mx_ao.regulator[0] = devm_regulator_get(dev,
							"vdd_pwrcl_mx_ao");
	if (IS_ERR(vdd_pwrcl_mx_ao.regulator[0])) {
		rc = PTR_ERR(vdd_pwrcl_mx_ao.regulator[0]);
		if (rc != -EPROBE_DEFER)
			dev_err(dev, "Unable to get vdd_pwrcl_mx_ao regulator, rc=%d\n",
				rc);
		return rc;
	}

	array = kcalloc(MAX_CLUSTER_CNT, sizeof(*array), GFP_KERNEL);
	if (!array)
		return -ENOMEM;

	rc = of_property_read_u32_array(dev->of_node, "qcom,mx-turbo-freq",
					array, MAX_CLUSTER_CNT);
	if (rc) {
		dev_err(dev, "unable to read qcom,mx-turbo-freq property, rc=%d\n",
			rc);
		kfree(array);
		return rc;
	}

	l3_clk.mx_turbo_freq = array[l3_clk.cluster_num];
	pwrcl_clk.mx_turbo_freq = array[pwrcl_clk.cluster_num];
	perfcl_clk.mx_turbo_freq = array[perfcl_clk.cluster_num];

	kfree(array);

	return 0;
}

static int clk_cpu_osm_driver_probe(struct platform_device *pdev)
{
	int rc = 0, i, cpu;
	u32 val;
	int num_clks = ARRAY_SIZE(osm_qcom_clk_hws);
	struct clk *ext_xo_clk, *clk;
	struct clk_osm *osm_clk;
	struct device *dev = &pdev->dev;
	struct clk_onecell_data *clk_data;
	struct cpu_cycle_counter_cb cb = {
		.get_cpu_cycle_counter = clk_osm_get_cpu_cycle_counter,
	};

	/*
	 * Require the RPM-XO clock to be registered before OSM.
	 * The cpuss_gpll0_clk_src is listed to be configured by BL.
	 */
	ext_xo_clk = devm_clk_get(dev, "xo_ao");
	if (IS_ERR(ext_xo_clk)) {
		if (PTR_ERR(ext_xo_clk) != -EPROBE_DEFER)
			dev_err(dev, "Unable to get xo clock\n");
		return PTR_ERR(ext_xo_clk);
	}

	is_sdm845v1 = of_device_is_compatible(pdev->dev.of_node,
					"qcom,clk-cpu-osm");

	if (of_device_is_compatible(pdev->dev.of_node,
					 "qcom,clk-cpu-osm-sdm670"))
		clk_cpu_osm_driver_sdm670_fixup();

	rc = clk_cpu_osm_request_mx_supply(dev);
	if (rc)
		return rc;

	clk_data = devm_kzalloc(&pdev->dev, sizeof(struct clk_onecell_data),
								GFP_KERNEL);
	if (!clk_data)
		goto exit;

	clk_data->clks = devm_kzalloc(&pdev->dev, (num_clks *
					sizeof(struct clk *)), GFP_KERNEL);
	if (!clk_data->clks)
		goto clk_err;

	clk_data->clk_num = num_clks;

	rc = clk_osm_resources_init(pdev);
	if (rc) {
		if (rc != -EPROBE_DEFER)
			dev_err(&pdev->dev, "OSM resources init failed, rc=%d\n",
				rc);
		return rc;
	}

	/* Check if per-core DCVS is enabled/not */
	val = clk_osm_read_reg(&pwrcl_clk, CORE_DCVS_CTRL);
	if (val & BIT(0))
		pwrcl_clk.per_core_dcvs = true;

	val = clk_osm_read_reg(&perfcl_clk, CORE_DCVS_CTRL);
	if (val & BIT(0))
		perfcl_clk.per_core_dcvs = true;

	rc = clk_osm_read_lut(pdev, &l3_clk);
	if (rc) {
		dev_err(&pdev->dev, "Unable to read OSM LUT for L3, rc=%d\n",
			rc);
		return rc;
	}

	rc = clk_osm_read_lut(pdev, &pwrcl_clk);
	if (rc) {
		dev_err(&pdev->dev, "Unable to read OSM LUT for power cluster, rc=%d\n",
			rc);
		return rc;
	}

	rc = clk_osm_read_lut(pdev, &perfcl_clk);
	if (rc) {
		dev_err(&pdev->dev, "Unable to read OSM LUT for perf cluster, rc=%d\n",
			rc);
		return rc;
	}

	spin_lock_init(&l3_clk.lock);
	spin_lock_init(&pwrcl_clk.lock);
	spin_lock_init(&perfcl_clk.lock);

	/* Register OSM l3, pwr and perf clocks with Clock Framework */
	for (i = 0; i < num_clks; i++) {
		if (!osm_qcom_clk_hws[i])
			continue;

		clk = devm_clk_register(&pdev->dev, osm_qcom_clk_hws[i]);
		if (IS_ERR(clk)) {
			dev_err(&pdev->dev, "Unable to register CPU clock at index %d\n",
				i);
			return PTR_ERR(clk);
		}
		clk_data->clks[i] = clk;
	}

	rc = of_clk_add_provider(pdev->dev.of_node, of_clk_src_onecell_get,
								clk_data);
	if (rc) {
		dev_err(&pdev->dev, "Unable to register CPU clocks\n");
			goto provider_err;
	}

	get_online_cpus();

	WARN(clk_prepare_enable(l3_cluster0_vote_clk.hw.clk),
			"clk: Failed to enable cluster0 clock for L3\n");
	WARN(clk_prepare_enable(l3_cluster1_vote_clk.hw.clk),
			"clk: Failed to enable cluster1 clock for L3\n");
	WARN(clk_prepare_enable(l3_misc_vote_clk.hw.clk),
			"clk: Failed to enable misc clock for L3\n");
	WARN(clk_prepare_enable(l3_gpu_vote_clk.hw.clk),
			"clk: Failed to enable iocoherent bwmon clock for L3\n");

	/*
	 * Call clk_prepare_enable for the silver clock explicitly in order to
	 * place an implicit vote on MX
	 */
	for_each_online_cpu(cpu) {
		osm_clk = logical_cpu_to_clk(cpu);
		if (!osm_clk)
			return -EINVAL;
		clk_prepare_enable(osm_clk->hw.clk);
	}
	populate_opp_table(pdev);

	of_platform_populate(pdev->dev.of_node, NULL, NULL, &pdev->dev);
	register_cpu_cycle_counter_cb(&cb);
	put_online_cpus();

	rc = cpufreq_register_driver(&qcom_osm_cpufreq_driver);
	if (rc)
		goto provider_err;

	pr_info("OSM CPUFreq driver inited\n");
	return 0;

provider_err:
	if (clk_data)
		devm_kfree(&pdev->dev, clk_data->clks);
clk_err:
	devm_kfree(&pdev->dev, clk_data);
exit:
	dev_err(&pdev->dev, "OSM CPUFreq driver failed to initialize, rc=%d\n",
		rc);
	panic("Unable to Setup OSM CPUFreq");
}

static const struct of_device_id match_table[] = {
	{ .compatible = "qcom,clk-cpu-osm" },
	{ .compatible = "qcom,clk-cpu-osm-v2" },
	{ .compatible = "qcom,clk-cpu-osm-sdm670" },
	{}
};

static struct platform_driver clk_cpu_osm_driver = {
	.probe = clk_cpu_osm_driver_probe,
	.driver = {
		.name = "clk-cpu-osm",
		.of_match_table = match_table,
		.owner = THIS_MODULE,
	},
};

static int __init clk_cpu_osm_init(void)
{
	return platform_driver_register(&clk_cpu_osm_driver);
}
subsys_initcall(clk_cpu_osm_init);

static void __exit clk_cpu_osm_exit(void)
{
	platform_driver_unregister(&clk_cpu_osm_driver);
}
module_exit(clk_cpu_osm_exit);

MODULE_DESCRIPTION("QTI CPU clock driver for OSM");
MODULE_LICENSE("GPL v2");
