/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/clk.h>
#include <linux/clk/msm-clk-provider.h>
#include <linux/clk/msm-clk.h>
#include <linux/clk/msm-clock-generic.h>
#include <linux/cpu.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/pm_opp.h>
#include <linux/pm_qos.h>
#include <linux/interrupt.h>
#include <linux/regulator/driver.h>

#include <soc/qcom/clock-pll.h>
#include <soc/qcom/clock-local2.h>
#include <soc/qcom/clock-alpha-pll.h>

#include <dt-bindings/clock/msm-clocks-cobalt.h>

#include "clock.h"

enum clk_osm_bases {
	OSM_BASE,
	PLL_BASE,
	NUM_BASES,
};

enum clk_osm_lut_data {
	FREQ,
	FREQ_DATA,
	PLL_OVERRIDES,
	NUM_FIELDS,
};

#define SEQ_REG(n) (0x300 + n*4)

#define OSM_TABLE_SIZE 40
#define MAX_CLUSTER_CNT 2
#define MAX_CONFIG 4

#define ENABLE_REG 0x1004
#define INDEX_REG 0x1150
#define FREQ_REG 0x1154
#define VOLT_REG 0x1158
#define OVERRIDE_REG 0x115C

#define OSM_CYCLE_COUNTER_CTRL_REG 0x1F00
#define OSM_CYCLE_COUNTER_STATUS_REG 0x1F04
#define DCVS_PERF_STATE_DESIRED_REG 0x1F10
#define DCVS_PERF_STATE_DEVIATION_INTR_STAT 0x1F14
#define DCVS_PERF_STATE_DEVIATION_INTR_EN 0x1F18
#define DCVS_PERF_STATE_DEVIATION_INTR_CLEAR 0x1F1C
#define DCVS_PERF_STATE_DEVIATION_CORRECTED_INTR_STAT 0x1F20
#define DCVS_PERF_STATE_DEVIATION_CORRECTED_INTR_EN 0x1F24
#define DCVS_PERF_STATE_DEVIATION_CORRECTED_INTR_CLEAR 0x1F28
#define DCVS_PERF_STATE_MET_INTR_STAT 0x1F2C
#define DCVS_PERF_STATE_MET_INTR_EN 0x1F30
#define DCVS_PERF_STATE_MET_INTR_CLR 0x1F34
#define OSM_CORE_TABLE_SIZE 8192
#define OSM_REG_SIZE 32

#define PLL_MODE		0x0
#define PLL_L_VAL		0x4
#define PLL_USER_CTRL		0xC
#define PLL_CONFIG_CTL_LO	0x10
#define PLL_STATUS		0x2C
#define PLL_LOCK_DET_MASK	BIT(16)
#define PLL_WAIT_LOCK_TIME_US 5
#define PLL_MIN_LVAL 32

#define CC_ZERO_BEHAV_CTRL 0x100C
#define SPM_CC_DCVS_DISABLE 0x1020
#define SPM_CC_CTRL 0x1028
#define SPM_CC_HYSTERESIS 0x101C
#define SPM_CORE_RET_MAPPING 0x1024

#define LLM_FREQ_VOTE_HYSTERESIS 0x102C
#define LLM_VOLT_VOTE_HYSTERESIS 0x1030
#define LLM_INTF_DCVS_DISABLE 0x1034

#define ENABLE_OVERRIDE BIT(0)

#define ITM_CL0_DISABLE_CL1_ENABLED 0x2
#define ITM_CL0_ENABLED_CL1_DISABLE 0x1

#define APM_MX_MODE 0
#define APM_APC_MODE BIT(1)
#define APM_MODE_SWITCH_MASK (BVAL(4, 2, 7) | BVAL(1, 0, 3))
#define APM_MX_MODE_VAL 0
#define APM_APC_MODE_VAL 0x3

#define GPLL_SEL 0x400
#define PLL_EARLY_SEL 0x500
#define PLL_MAIN_SEL 0x300
#define RCG_UPDATE 0x3
#define RCG_UPDATE_SUCCESS 0x2
#define PLL_POST_DIV1 0x1F
#define PLL_POST_DIV2 0x11F

#define VMIN_REDUC_ENABLE_REG 0x103C
#define VMIN_REDUC_TIMER_REG 0x1040
#define PDN_FSM_CTRL_REG 0x1070
#define CC_BOOST_TIMER_REG0 0x1074
#define CC_BOOST_TIMER_REG1 0x1078
#define CC_BOOST_TIMER_REG2 0x107C
#define CC_BOOST_EN_MASK BIT(0)
#define PS_BOOST_EN_MASK BIT(1)
#define DCVS_BOOST_EN_MASK BIT(2)
#define PC_RET_EXIT_DROOP_EN_MASK BIT(3)
#define WFX_DROOP_EN_MASK BIT(4)
#define DCVS_DROOP_EN_MASK BIT(5)
#define LMH_PS_EN_MASK BIT(6)
#define IGNORE_PLL_LOCK_MASK BIT(15)
#define SAFE_FREQ_WAIT_US 1
#define DCVS_BOOST_TIMER_REG0 0x1084
#define DCVS_BOOST_TIMER_REG1 0x1088
#define DCVS_BOOST_TIMER_REG2 0x108C
#define PS_BOOST_TIMER_REG0 0x1094
#define PS_BOOST_TIMER_REG1 0x1098
#define PS_BOOST_TIMER_REG2 0x109C
#define BOOST_PROG_SYNC_DELAY_REG 0x10A0
#define DROOP_CTRL_REG 0x10A4
#define DROOP_PROG_SYNC_DELAY_REG 0x10B8
#define DROOP_UNSTALL_TIMER_CTRL_REG 0x10AC
#define DROOP_WAIT_TO_RELEASE_TIMER_CTRL0_REG 0x10B0
#define DROOP_WAIT_TO_RELEASE_TIMER_CTRL1_REG 0x10B4

#define DCVS_DROOP_TIMER_CTRL 0x10B8
#define SEQ_MEM_ADDR 0x500
#define SEQ_CFG_BR_ADDR 0x170
#define MAX_INSTRUCTIONS 256
#define MAX_BR_INSTRUCTIONS 49

static u32 seq_instr[] = {
	0xc2005000, 0x2c9e3b21, 0xc0ab2cdc, 0xc2882525, 0x359dc491,
	0x700a500b, 0x70005001, 0x390938c8, 0xcb44c833, 0xce56cd54,
	0x341336e0, 0xadba0000, 0x10004000, 0x70005001, 0x1000500c,
	0xc792c5a1, 0x501625e1, 0x3da335a2, 0x50170006, 0x50150006,
	0x1000c633, 0x1000acb3, 0xc422acb4, 0xaefc1000, 0x700a500b,
	0x70005001, 0x5010aefd, 0x5012700b, 0xad41700c, 0x0000adb9,
	0x500c181b, 0x5011500f, 0x181b3413, 0x853984b9, 0x0003bd80,
	0xa0012ba4, 0x71050006, 0x500e1000, 0x500c1000, 0x38801c0a,
	0x1c063b18, 0x1c073b43, 0x1c061000, 0x1c073983, 0x3840500c,
	0x00001c0a, 0x50021000, 0x00007001, 0x81031000, 0x70025003,
	0x70035004, 0x3b441000, 0x81043985, 0x70025003, 0x50054003,
	0xa1467009, 0x0003b1c0, 0x4005238b, 0x83081000, 0x850c848b,
	0x830d1000, 0x850c848e, 0x38811000, 0xa7183842, 0xa79aa759,
	0x0000a7db, 0x8c101000, 0x8d128c91, 0x00008d93, 0x8c141000,
	0x8d168c95, 0x00008d97, 0x50061000, 0x39cd3a4c, 0x3ad03a8f,
	0x10004006, 0x70065007, 0xa00f2c12, 0x00064007, 0x700d7105,
	0xa9641000, 0x40071c1a, 0x1000700d, 0x70065007, 0x50101c16,
	0x24115012, 0x700d4007, 0x10004007, 0xa821a00f, 0x71050006,
	0x700d4007, 0x91ad500c, 0x500f1c15, 0x00005011, 0x2bd41000,
	0xa00f500c, 0x71050006, 0xa00f1000, 0x0006a821, 0x500c7005,
	0x1c1591ad, 0x5011500f, 0x2bce1000, 0x50101c16, 0xa0225012,
	0x0006a82a, 0x91a67105, 0x500f1c15, 0x500c5011, 0xa00f5014,
	0x71050006, 0x10000000, 0x501391a4, 0xa9632217, 0x10001c1a,
	0xa9632217, 0x10001c1a, 0x70075008, 0xa9634008, 0x50091c1a,
	0x40097008, 0x848e1000, 0xb1c0850c, 0x2b990003, 0x1000400d,
	0x1000500d, 0x84b0abaf, 0xbb808531, 0x10000003, 0x0006a037,
	0x10007105,
};

static u32 seq_br_instr[] = {
	0xfa, 0x10a, 0x116, 0xce, 0xea,
	0xf2, 0xba, 0xc2, 0x9a, 0xaa,
	0x122, 0xe0, 0x17a, 0x19a, 0x1a2,
	0x130, 0x14c, 0x160, 0x142, 0x96,
	0x186, 0x1cc, 0x1c0, 0x1d4, 0x1e6,
	0x1f4, 0x1f8, 0x30, 0x5e, 0x84,
	0x7a, 0x1fe, 0x34, 0x3c, 0x54,
	0x58, 0x204, 0x2e,
};

DEFINE_EXT_CLK(xo_ao, NULL);
DEFINE_EXT_CLK(sys_apcsaux_clk_gcc, NULL);

struct osm_entry {
	u16 virtual_corner;
	u16 open_loop_volt;
	u32 freq_data;
	u32 override_data;
	long frequency;
};

static struct dentry *osm_debugfs_base;

struct clk_osm {
	struct clk c;
	struct osm_entry osm_table[OSM_TABLE_SIZE];
	struct dentry *debugfs;
	struct regulator *vdd_reg;
	struct platform_device *vdd_dev;
	void *vbases[NUM_BASES];
	unsigned long pbases[NUM_BASES];
	u32 cpu_reg_mask;

	u32 num_entries;
	u32 cluster_num;
	u32 irq;
	u32 apm_crossover_vc;

	u32 cycle_counter_reads;
	u32 cycle_counter_delay;
	u32 cycle_counter_factor;
	u32 l_val_base;
	u32 apcs_itm_present;
	u32 apcs_cfg_rcgr;
	u32 apcs_cmd_rcgr;
	u32 apcs_pll_user_ctl;
	u32 apm_mode_ctl;
	u32 apm_ctrl_status;
	u32 osm_clk_rate;
	u32 xo_clk_rate;
	bool red_fsm_en;
	bool boost_fsm_en;
	bool safe_fsm_en;
	bool ps_fsm_en;
	bool droop_fsm_en;
	bool wfx_fsm_en;
	bool pc_fsm_en;
};

static void clk_osm_write_reg(struct clk_osm *c, int val, u32 offset)
{
	writel_relaxed(val , (char *)c->vbases[OSM_BASE] + offset
		       + c->cluster_num * OSM_CORE_TABLE_SIZE);
}

static int clk_osm_read_reg(struct clk_osm *c, u32 offset)
{
	return readl_relaxed((char *)c->vbases[OSM_BASE] + offset +
			     c->cluster_num * OSM_CORE_TABLE_SIZE);
}

static inline int clk_osm_count_us(struct clk_osm *c, u32 usec)
{
	u64 temp = c->osm_clk_rate;

	do_div(temp, 1000000);
	return temp * usec;
}

static inline struct clk_osm *to_clk_osm(struct clk *c)
{
	return container_of(c, struct clk_osm, c);
}

static enum handoff clk_osm_handoff(struct clk *c)
{
	return HANDOFF_DISABLED_CLK;
}

static long clk_osm_list_rate(struct clk *c, unsigned n)
{
	if (n >= c->num_fmax)
		return -ENXIO;
	return c->fmax[n];
}

static long clk_osm_round_rate(struct clk *c, unsigned long rate)
{
	int i;

	for (i = 0; i < c->num_fmax; i++)
		if (rate <= c->fmax[i])
			return c->fmax[i];

	return c->fmax[i-1];
}

static int clk_osm_search_table(struct osm_entry *table, int entries, long rate)
{
	int i;

	for (i = 0; i < entries; i++)
		if (rate == table[i].frequency)
			return i;
	return -EINVAL;
}

static int clk_osm_set_rate(struct clk *c, unsigned long rate)
{
	struct clk_osm *cpuclk = to_clk_osm(c);
	int index = 0;
	unsigned long r_rate;

	r_rate = clk_osm_round_rate(c, rate);

	if (rate != r_rate) {
		pr_err("invalid rate requested rate=%ld\n", rate);
		return -EINVAL;
	}

	/* Convert rate to table index */
	index = clk_osm_search_table(cpuclk->osm_table,
				     cpuclk->num_entries, r_rate);
	if (index < 0) {
		pr_err("cannot set cluster %u to %lu\n",
		       cpuclk->cluster_num, rate);
		return -EINVAL;
	}
	pr_debug("rate: %lu --> index %d\n", rate, index);

	/* Choose index and send request to OSM hardware */
	clk_osm_write_reg(cpuclk, index, DCVS_PERF_STATE_DESIRED_REG);

	/* Make sure the write goes through before proceeding */
	mb();

	return 0;
}

static int clk_osm_enable(struct clk *c)
{
	struct clk_osm *cpuclk = to_clk_osm(c);

	clk_osm_write_reg(cpuclk, 1, ENABLE_REG);

	/* Make sure the write goes through before proceeding */
	mb();

	/* Wait for 5us for OSM hardware to enable */
	udelay(5);

	pr_debug("OSM clk enabled for cluster=%d\n", cpuclk->cluster_num);

	return 0;
}

static struct clk_ops clk_ops_cpu_osm = {
	.enable = clk_osm_enable,
	.set_rate = clk_osm_set_rate,
	.round_rate = clk_osm_round_rate,
	.list_rate = clk_osm_list_rate,
	.handoff = clk_osm_handoff,
};

static struct regulator *vdd_pwrcl;
static struct regulator *vdd_perfcl;

static struct clk_osm pwrcl_clk = {
	.cluster_num = 0,
	.cpu_reg_mask = 0x3,
	.c = {
		.dbg_name = "pwrcl_clk",
		.ops = &clk_ops_cpu_osm,
		.parent = &xo_ao.c,
		CLK_INIT(pwrcl_clk.c),
	},
};

static struct clk_osm perfcl_clk = {
	.cluster_num = 1,
	.cpu_reg_mask = 0x103,
	.c = {
		.dbg_name = "perfcl_clk",
		.ops = &clk_ops_cpu_osm,
		.parent = &xo_ao.c,
		CLK_INIT(perfcl_clk.c),
	},
};

static struct clk_lookup cpu_clocks_osm[] = {
	CLK_LIST(pwrcl_clk),
	CLK_LIST(perfcl_clk),
	CLK_LIST(sys_apcsaux_clk_gcc),
	CLK_LIST(xo_ao),
};

static void clk_osm_print_osm_table(struct clk_osm *c)
{
	int i;
	struct osm_entry *table = c->osm_table;
	u32 pll_src, pll_div, lval;

	pr_debug("Index, Frequency, VC, OLV (mv), PLLSrc, PLLDivVal, LVal\n");
	for (i = 0; i < c->num_entries; i++) {
		pll_src = (table[i].freq_data & GENMASK(27, 26)) >> 26;
		pll_div = (table[i].freq_data & GENMASK(25, 24)) >> 24;
		lval = table[i].freq_data & GENMASK(7, 0);

		pr_debug("%3d, %11lu, %2u, %5u, %6u, %8u, %7u\n",
			i,
			table[i].frequency,
			table[i].virtual_corner,
			table[i].open_loop_volt,
			pll_src,
			pll_div,
			lval);
	}
	pr_debug("APM crossover corner: %d\n",
		 c->apm_crossover_vc);
}

static int clk_osm_get_lut(struct platform_device *pdev,
			   struct clk_osm *c, char *prop_name)
{
	struct clk *clk = &c->c;
	struct device_node *of = pdev->dev.of_node;
	int prop_len, total_elems, num_rows, i, j, k;
	int rc = 0;
	u32 *array;
	u32 data;
	bool last_entry = false;

	if (!of_find_property(of, prop_name, &prop_len)) {
		dev_err(&pdev->dev, "missing %s\n", prop_name);
		return -EINVAL;
	}

	total_elems = prop_len / sizeof(u32);
	if (total_elems % NUM_FIELDS) {
		dev_err(&pdev->dev, "bad length %d\n", prop_len);
		return -EINVAL;
	}

	num_rows = total_elems / NUM_FIELDS;

	clk->fmax = devm_kzalloc(&pdev->dev, num_rows * sizeof(unsigned long),
			       GFP_KERNEL);
	if (!clk->fmax)
		return -ENOMEM;

	array = devm_kzalloc(&pdev->dev, prop_len, GFP_KERNEL);
	if (!array)
		return -ENOMEM;

	rc = of_property_read_u32_array(of, prop_name, array, total_elems);
	if (rc) {
		dev_err(&pdev->dev, "Unable to parse OSM table, rc=%d\n", rc);
		goto exit;
	}

	pr_debug("%s: Entries in Table: %d\n", __func__, num_rows);
	c->num_entries = num_rows;
	if (c->num_entries > OSM_TABLE_SIZE) {
		pr_err("LUT entries %d exceed maximum size %d\n",
		       c->num_entries, OSM_TABLE_SIZE);
		return -EINVAL;
	}

	for (i = 0, j = 0, k = 0; j < OSM_TABLE_SIZE; j++) {
		c->osm_table[j].frequency = array[i + FREQ];
		c->osm_table[j].freq_data = array[i + FREQ_DATA];
		c->osm_table[j].override_data = array[i + PLL_OVERRIDES];
		pr_debug("index=%d freq=%ld freq_data=0x%x override_data=0x%x\n",
			 j, c->osm_table[j].frequency,
			 c->osm_table[j].freq_data,
			 c->osm_table[j].override_data);

		data = (array[i + FREQ_DATA] & GENMASK(18, 16)) >> 16;
		if (!last_entry && data == MAX_CONFIG) {
			clk->fmax[k] = array[i];
			k++;
		}

		if (i < total_elems - NUM_FIELDS)
			i += NUM_FIELDS;
		else
			last_entry = true;
	}
	clk->num_fmax = k;
exit:
	devm_kfree(&pdev->dev, array);
	return rc;
}

static int clk_osm_parse_dt_configs(struct platform_device *pdev)
{
	struct device_node *of = pdev->dev.of_node;
	u32 *array;
	int rc = 0;

	array = devm_kzalloc(&pdev->dev, MAX_CLUSTER_CNT * sizeof(u32),
			     GFP_KERNEL);
	if (!array)
		return -ENOMEM;

	rc = of_property_read_u32_array(of, "qcom,l-val-base",
					array, MAX_CLUSTER_CNT);
	if (rc) {
		dev_err(&pdev->dev, "unable to find qcom,l-val-base property, rc=%d\n",
			rc);
		return -EINVAL;
	}

	pwrcl_clk.l_val_base = array[pwrcl_clk.cluster_num];
	perfcl_clk.l_val_base = array[perfcl_clk.cluster_num];

	rc = of_property_read_u32_array(of, "qcom,apcs-itm-present",
				  array, MAX_CLUSTER_CNT);
	if (rc) {
		dev_err(&pdev->dev, "unable to find qcom,apcs-itm-present property, rc=%d\n",
			rc);
		return -EINVAL;
	}

	pwrcl_clk.apcs_itm_present = array[pwrcl_clk.cluster_num];
	perfcl_clk.apcs_itm_present = array[perfcl_clk.cluster_num];

	rc = of_property_read_u32_array(of, "qcom,apcs-cfg-rcgr",
					array, MAX_CLUSTER_CNT);
	if (rc) {
		dev_err(&pdev->dev, "unable to find qcom,apcs-cfg-rcgr property, rc=%d\n",
			rc);
		return -EINVAL;
	}

	pwrcl_clk.apcs_cfg_rcgr = array[pwrcl_clk.cluster_num];
	perfcl_clk.apcs_cfg_rcgr = array[perfcl_clk.cluster_num];

	rc = of_property_read_u32_array(of, "qcom,apcs-cmd-rcgr",
					array, MAX_CLUSTER_CNT);
	if (rc) {
		dev_err(&pdev->dev, "unable to find qcom,apcs-cmd-rcgr property, rc=%d\n",
			rc);
		return -EINVAL;
	}

	pwrcl_clk.apcs_cmd_rcgr = array[pwrcl_clk.cluster_num];
	perfcl_clk.apcs_cmd_rcgr = array[perfcl_clk.cluster_num];

	rc = of_property_read_u32_array(of, "qcom,apcs-pll-user-ctl",
					array, MAX_CLUSTER_CNT);
	if (rc) {
		dev_err(&pdev->dev, "unable to find qcom,apcs-pll-user-ctl property, rc=%d\n",
			rc);
		return -EINVAL;
	}

	pwrcl_clk.apcs_pll_user_ctl = array[pwrcl_clk.cluster_num];
	perfcl_clk.apcs_pll_user_ctl = array[perfcl_clk.cluster_num];

	rc = of_property_read_u32_array(of, "qcom,apm-mode-ctl",
				  array, MAX_CLUSTER_CNT);
	if (rc) {
		dev_err(&pdev->dev, "unable to find qcom,apm-mode-ctl property, rc=%d\n",
			rc);
		return -EINVAL;
	}

	pwrcl_clk.apm_mode_ctl = array[pwrcl_clk.cluster_num];
	perfcl_clk.apm_mode_ctl = array[perfcl_clk.cluster_num];

	rc = of_property_read_u32_array(of, "qcom,apm-ctrl-status",
				  array, MAX_CLUSTER_CNT);
	if (rc) {
		dev_err(&pdev->dev, "unable to find qcom,apm-ctrl-status property, rc=%d\n",
			rc);
		return -EINVAL;
	}

	pwrcl_clk.apm_ctrl_status = array[pwrcl_clk.cluster_num];
	perfcl_clk.apm_ctrl_status = array[perfcl_clk.cluster_num];

	rc = of_property_read_u32(of, "qcom,xo-clk-rate",
				  &pwrcl_clk.xo_clk_rate);
	if (rc) {
		dev_err(&pdev->dev, "unable to find qcom,xo-clk-rate property, rc=%d\n",
			rc);
		return -EINVAL;
	}

	perfcl_clk.xo_clk_rate = pwrcl_clk.xo_clk_rate;

	rc = of_property_read_u32(of, "qcom,osm-clk-rate",
				  &pwrcl_clk.osm_clk_rate);
	if (rc) {
		dev_err(&pdev->dev, "unable to find qcom,osm-clk-rate property, rc=%d\n",
			rc);
		return -EINVAL;
	}
	perfcl_clk.osm_clk_rate = pwrcl_clk.osm_clk_rate;

	rc = of_property_read_u32(of, "qcom,cc-reads",
				  &pwrcl_clk.cycle_counter_reads);
	if (rc) {
		dev_err(&pdev->dev, "unable to find qcom,cc-reads property, rc=%d\n",
			rc);
		return -EINVAL;
	}
	perfcl_clk.cycle_counter_reads = pwrcl_clk.cycle_counter_reads;

	rc = of_property_read_u32(of, "qcom,cc-delay",
				  &pwrcl_clk.cycle_counter_delay);
	if (rc)
		dev_dbg(&pdev->dev, "no delays between cycle counter reads\n");
	else
		perfcl_clk.cycle_counter_delay = pwrcl_clk.cycle_counter_delay;

	rc = of_property_read_u32(of, "qcom,cc-factor",
				  &pwrcl_clk.cycle_counter_factor);
	if (rc)
		dev_dbg(&pdev->dev, "no factor specified for cycle counter estimation\n");
	else
		perfcl_clk.cycle_counter_factor =
			pwrcl_clk.cycle_counter_factor;

	perfcl_clk.red_fsm_en = pwrcl_clk.red_fsm_en =
		of_property_read_bool(of, "qcom,red-fsm-en");

	perfcl_clk.boost_fsm_en = pwrcl_clk.boost_fsm_en =
		of_property_read_bool(of, "qcom,boost-fsm-en");

	perfcl_clk.safe_fsm_en = pwrcl_clk.safe_fsm_en =
		of_property_read_bool(of, "qcom,safe-fsm-en");

	perfcl_clk.ps_fsm_en = pwrcl_clk.ps_fsm_en =
		of_property_read_bool(of, "qcom,ps-fsm-en");

	perfcl_clk.droop_fsm_en = pwrcl_clk.droop_fsm_en =
		of_property_read_bool(of, "qcom,droop-fsm-en");

	perfcl_clk.wfx_fsm_en = pwrcl_clk.wfx_fsm_en =
		of_property_read_bool(of, "qcom,wfx-fsm-en");

	perfcl_clk.pc_fsm_en = pwrcl_clk.pc_fsm_en =
		of_property_read_bool(of, "qcom,pc-fsm-en");

	devm_kfree(&pdev->dev, array);
	return rc;
}

static int clk_osm_resources_init(struct platform_device *pdev)
{
	struct device_node *node;
	struct resource *res;
	struct clk *c;
	unsigned long pbase;
	int i, rc = 0;
	void *vbase;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "osm");
	if (!res) {
		dev_err(&pdev->dev,
			"Unable to get platform resource for osm");
		return -ENOMEM;
	}

	pwrcl_clk.pbases[OSM_BASE] = (unsigned long)res->start;
	pwrcl_clk.vbases[OSM_BASE] = devm_ioremap(&pdev->dev, res->start,
						  resource_size(res));
	if (!pwrcl_clk.vbases[OSM_BASE]) {
		dev_err(&pdev->dev, "Unable to map in osm base\n");
		return -ENOMEM;
	}

	perfcl_clk.pbases[OSM_BASE] = pwrcl_clk.pbases[OSM_BASE];
	perfcl_clk.vbases[OSM_BASE] = pwrcl_clk.vbases[OSM_BASE];

	for (i = 0; i < MAX_CLUSTER_CNT; i++) {
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						   i == pwrcl_clk.cluster_num ?
						   "pwrcl_pll" : "perfcl_pll");
		if (!res) {
			dev_err(&pdev->dev,
				"Unable to get platform resource\n");
			return -ENOMEM;
		}
		pbase = (unsigned long)res->start;
		vbase = devm_ioremap(&pdev->dev, res->start,
				     resource_size(res));

		if (!vbase) {
			dev_err(&pdev->dev, "Unable to map in base\n");
			return -ENOMEM;
		}

		if (i == pwrcl_clk.cluster_num) {
			pwrcl_clk.pbases[PLL_BASE] = pbase;
			pwrcl_clk.vbases[PLL_BASE] = vbase;
		} else {
			perfcl_clk.pbases[PLL_BASE] = pbase;
			perfcl_clk.vbases[PLL_BASE] = vbase;
		}
	}

	vdd_pwrcl = devm_regulator_get(&pdev->dev, "vdd-pwrcl");
	if (IS_ERR(vdd_pwrcl)) {
		rc = PTR_ERR(vdd_pwrcl);
		if (rc != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get the pwrcl vreg, rc=%d\n",
				rc);
		return rc;
	}

	vdd_perfcl = devm_regulator_get(&pdev->dev, "vdd-perfcl");
	if (IS_ERR(vdd_perfcl)) {
		rc = PTR_ERR(vdd_perfcl);
		if (rc != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get the perfcl vreg, rc=%d\n",
				rc);
		return rc;
	}

	pwrcl_clk.vdd_reg = vdd_pwrcl;
	perfcl_clk.vdd_reg = vdd_perfcl;

	node = of_parse_phandle(pdev->dev.of_node, "vdd-pwrcl-supply", 0);
	if (!node) {
		pr_err("Unable to find vdd-pwrcl-supply\n");
		return -EINVAL;
	}

	pwrcl_clk.vdd_dev = of_find_device_by_node(node->parent->parent);
	if (!pwrcl_clk.vdd_dev) {
		pr_err("Unable to find device for vdd-pwrcl-supply node\n");
		return -EINVAL;
	}

	node = of_parse_phandle(pdev->dev.of_node,
				"vdd-perfcl-supply", 0);
	if (!node) {
		pr_err("Unable to find vdd-perfcl-supply\n");
		return -EINVAL;
	}

	perfcl_clk.vdd_dev = of_find_device_by_node(node->parent->parent);
	if (!perfcl_clk.vdd_dev) {
		pr_err("Unable to find device for vdd-perfcl-supply\n");
		return -EINVAL;
	}

	c = devm_clk_get(&pdev->dev, "aux_clk");
	if (IS_ERR(c)) {
		rc = PTR_ERR(c);
		if (rc != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get aux_clk, rc=%d\n",
				rc);
		return rc;
	}
	sys_apcsaux_clk_gcc.c.parent = c;

	c = devm_clk_get(&pdev->dev, "xo_ao");
	if (IS_ERR(c)) {
		rc = PTR_ERR(c);
		if (rc != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get xo_ao clk, rc=%d\n",
				rc);
		return rc;
	}
	xo_ao.c.parent = c;

	return 0;
}

static void clk_osm_setup_cluster_pll(struct clk_osm *c)
{
	writel_relaxed(0x0, c->vbases[PLL_BASE] + PLL_MODE);
	writel_relaxed(0x20, c->vbases[PLL_BASE] + PLL_L_VAL);
	writel_relaxed(0x01000008, c->vbases[PLL_BASE] +
		       PLL_USER_CTRL);
	writel_relaxed(0x20004AA8, c->vbases[PLL_BASE] +
		       PLL_CONFIG_CTL_LO);
	writel_relaxed(0x2, c->vbases[PLL_BASE] +
		       PLL_MODE);

	/* Ensure writes complete before delaying */
	mb();

	udelay(PLL_WAIT_LOCK_TIME_US);

	writel_relaxed(0x6, c->vbases[PLL_BASE] + PLL_MODE);

	/* Ensure write completes before delaying */
	mb();

	usleep_range(50, 75);

	writel_relaxed(0x7, c->vbases[PLL_BASE] + PLL_MODE);
}

static void clk_osm_setup_hw_table(struct clk_osm *c)
{
	struct osm_entry *entry = c->osm_table;
	int i;
	u32 freq_val, volt_val, override_val;
	u32 table_entry_offset;

	for (i = 0; i < OSM_TABLE_SIZE; i++) {
		if (i < c->num_entries) {
			freq_val = entry[i].freq_data;
			volt_val = BVAL(21, 16, entry[i].virtual_corner)
				| BVAL(11, 0, entry[i].open_loop_volt);
			override_val = entry[i].override_data;
		}
		table_entry_offset = i * OSM_REG_SIZE;
		clk_osm_write_reg(c, i, INDEX_REG + table_entry_offset);
		clk_osm_write_reg(c, freq_val, FREQ_REG + table_entry_offset);
		clk_osm_write_reg(c, volt_val, VOLT_REG + table_entry_offset);
		clk_osm_write_reg(c, override_val, OVERRIDE_REG +
				  table_entry_offset);
	}

	/* Make sure all writes go through */
	mb();
}

static int clk_osm_resolve_open_loop_voltages(struct clk_osm *c)
{
	struct regulator *regulator = c->vdd_reg;
	struct dev_pm_opp *opp;
	unsigned long freq;
	u32 vc, mv, data;
	int i, rc = 0;

	/*
	 * Determine frequency -> virtual corner -> open-loop voltage
	 * mapping from the OPP table.
	 */
	for (i = 0; i < OSM_TABLE_SIZE; i++) {
		freq = c->osm_table[i].frequency;
		/*
		 * Only frequencies that are supported across all configurations
		 * are present in the OPP table associated with the regulator
		 * device.
		 */
		data = (c->osm_table[i].freq_data & GENMASK(18, 16)) >> 16;
		if (data != MAX_CONFIG) {
			if (i < 1) {
				pr_err("Invalid LUT entry at index 0\n");
				return -EINVAL;
			}
			c->osm_table[i].open_loop_volt =
				c->osm_table[i-1].open_loop_volt;
			c->osm_table[i].virtual_corner =
				c->osm_table[i-1].virtual_corner;
			continue;
		}

		rcu_read_lock();
		opp = dev_pm_opp_find_freq_exact(&c->vdd_dev->dev, freq, true);
		if (IS_ERR(opp)) {
			rc = PTR_ERR(opp);
			if (rc == -ERANGE)
				pr_err("Frequency %lu not found\n", freq);
			goto exit;
		}

		vc = dev_pm_opp_get_voltage(opp);
		if (!vc) {
			pr_err("No virtual corner found for frequency %lu\n",
			       freq);
			rc = -ERANGE;
			goto exit;
		}

		rcu_read_unlock();

		/* Voltage is in uv. Convert to mv */
		mv = regulator_list_corner_voltage(regulator, vc) / 1000;

		/* CPR virtual corners are zero-based numbered */
		vc--;
		c->osm_table[i].open_loop_volt = mv;
		c->osm_table[i].virtual_corner = vc;
	}

	return 0;
exit:
	rcu_read_unlock();
	return rc;
}

static int clk_osm_resolve_crossover_corners(struct clk_osm *c,
				     struct platform_device *pdev)
{
	struct dev_pm_opp *opp;
	unsigned long freq = 0;
	int vc, rc = 0;

	rcu_read_lock();
	opp = dev_pm_opp_find_freq_exact(&c->vdd_dev->dev, freq, true);
	if (IS_ERR(opp)) {
		rc = PTR_ERR(opp);
		if (rc == -ERANGE)
			pr_debug("APM placeholder frequency entry not found\n");
		goto exit;
	}
	vc = dev_pm_opp_get_voltage(opp);
	if (!vc) {
		pr_debug("APM crossover corner not found\n");
		rc = -ERANGE;
		goto exit;
	}
	rcu_read_unlock();
	vc--;
	c->apm_crossover_vc = vc;

	return 0;
exit:
	rcu_read_unlock();
	return rc;
}

static int clk_osm_set_cc_policy(struct platform_device *pdev)
{
	int rc = 0, val;
	u32 *array;
	struct device_node *of = pdev->dev.of_node;

	array = devm_kzalloc(&pdev->dev, MAX_CLUSTER_CNT * sizeof(u32),
			     GFP_KERNEL);
	if (!array)
		return -ENOMEM;

	rc = of_property_read_u32_array(of, "qcom,up-timer", array,
					MAX_CLUSTER_CNT);
	if (rc) {
		dev_dbg(&pdev->dev, "No up timer value, rc=%d\n",
			 rc);
	} else {
		val = clk_osm_read_reg(&pwrcl_clk, SPM_CC_HYSTERESIS)
			| BVAL(31, 16, clk_osm_count_us(&pwrcl_clk,
					array[pwrcl_clk.cluster_num]));
		clk_osm_write_reg(&pwrcl_clk, val, SPM_CC_HYSTERESIS);
		val = clk_osm_read_reg(&perfcl_clk, SPM_CC_HYSTERESIS)
			| BVAL(31, 16, clk_osm_count_us(&perfcl_clk,
					array[perfcl_clk.cluster_num]));
		clk_osm_write_reg(&perfcl_clk, val, SPM_CC_HYSTERESIS);
	}

	rc = of_property_read_u32_array(of, "qcom,down-timer",
					array, MAX_CLUSTER_CNT);
	if (rc) {
		dev_dbg(&pdev->dev, "No down timer value, rc=%d\n", rc);
	} else {
		val = clk_osm_read_reg(&pwrcl_clk, SPM_CC_HYSTERESIS)
			| BVAL(15, 0, clk_osm_count_us(&pwrcl_clk,
				       array[pwrcl_clk.cluster_num]));
		clk_osm_write_reg(&pwrcl_clk, val, SPM_CC_HYSTERESIS);
		val = clk_osm_read_reg(&perfcl_clk, SPM_CC_HYSTERESIS)
			| BVAL(15, 0, clk_osm_count_us(&perfcl_clk,
				       array[perfcl_clk.cluster_num]));
		clk_osm_write_reg(&perfcl_clk, val, SPM_CC_HYSTERESIS);
	}

	/* OSM index override for cluster PC */
	rc = of_property_read_u32_array(of, "qcom,pc-override-index",
					array, MAX_CLUSTER_CNT);
	if (rc) {
		dev_dbg(&pdev->dev, "No PC override index value, rc=%d\n",
			rc);
		clk_osm_write_reg(&pwrcl_clk, 0, CC_ZERO_BEHAV_CTRL);
		clk_osm_write_reg(&perfcl_clk, 0, CC_ZERO_BEHAV_CTRL);
	} else {
		val = BVAL(6, 1, array[pwrcl_clk.cluster_num])
			| ENABLE_OVERRIDE;
		clk_osm_write_reg(&pwrcl_clk, val, CC_ZERO_BEHAV_CTRL);
		val = BVAL(6, 1, array[perfcl_clk.cluster_num])
			| ENABLE_OVERRIDE;
		clk_osm_write_reg(&perfcl_clk, val, CC_ZERO_BEHAV_CTRL);
	}

	/* Wait for the writes to complete */
	mb();

	rc = of_property_read_bool(pdev->dev.of_node, "qcom,set-ret-inactive");
	if (rc) {
		dev_dbg(&pdev->dev, "Treat cores in retention as active\n");
		val = 1;
	} else {
		dev_dbg(&pdev->dev, "Treat cores in retention as inactive\n");
		val = 0;
	}

	clk_osm_write_reg(&pwrcl_clk, val, SPM_CORE_RET_MAPPING);
	clk_osm_write_reg(&perfcl_clk, val, SPM_CORE_RET_MAPPING);

	rc = of_property_read_bool(pdev->dev.of_node, "qcom,disable-cc-dvcs");
	if (rc) {
		dev_dbg(&pdev->dev, "Disabling CC based DCVS\n");
		val = 1;
	} else
		val = 0;

	clk_osm_write_reg(&pwrcl_clk, val, SPM_CC_DCVS_DISABLE);
	clk_osm_write_reg(&perfcl_clk, val, SPM_CC_DCVS_DISABLE);

	/* Wait for the writes to complete */
	mb();

	devm_kfree(&pdev->dev, array);
	return 0;
}

static void clk_osm_setup_itm_to_osm_handoff(void)
{
	/* Program address of ITM_PRESENT of CPUSS */
	clk_osm_write_reg(&pwrcl_clk, pwrcl_clk.apcs_itm_present,
			  SEQ_REG(37));
	clk_osm_write_reg(&pwrcl_clk, 0, SEQ_REG(38));
	clk_osm_write_reg(&perfcl_clk, perfcl_clk.apcs_itm_present,
			  SEQ_REG(37));
	clk_osm_write_reg(&perfcl_clk, 0, SEQ_REG(38));

	/*
	 * Program data to write to ITM_PRESENT assuming ITM for other domain
	 * is enabled and the ITM for this domain is to be disabled.
	 */
	clk_osm_write_reg(&pwrcl_clk, ITM_CL0_DISABLE_CL1_ENABLED,
			  SEQ_REG(39));
	clk_osm_write_reg(&perfcl_clk, ITM_CL0_ENABLED_CL1_DISABLE,
			  SEQ_REG(39));
}

static int clk_osm_set_llm_freq_policy(struct platform_device *pdev)
{
	struct device_node *of = pdev->dev.of_node;
	u32 *array;
	int rc = 0, val, regval;

	array = devm_kzalloc(&pdev->dev, MAX_CLUSTER_CNT * sizeof(u32),
			     GFP_KERNEL);
	if (!array)
		return -ENOMEM;

	/*
	 * Setup Timer to control how long OSM should wait before performing
	 * DCVS when a LLM up frequency request is received.
	 * Time is specified in us.
	 */
	rc = of_property_read_u32_array(of, "qcom,llm-freq-up-timer", array,
					MAX_CLUSTER_CNT);
	if (rc) {
		dev_dbg(&pdev->dev, "Unable to get CC up timer value: %d\n",
			rc);
	} else {
		val = clk_osm_read_reg(&pwrcl_clk, LLM_FREQ_VOTE_HYSTERESIS)
			| BVAL(31, 16, clk_osm_count_us(&pwrcl_clk,
						array[pwrcl_clk.cluster_num]));
		clk_osm_write_reg(&pwrcl_clk, val, LLM_FREQ_VOTE_HYSTERESIS);
		val = clk_osm_read_reg(&perfcl_clk, LLM_FREQ_VOTE_HYSTERESIS)
			| BVAL(31, 16, clk_osm_count_us(&perfcl_clk,
						array[perfcl_clk.cluster_num]));
		clk_osm_write_reg(&perfcl_clk, val, LLM_FREQ_VOTE_HYSTERESIS);
	}

	/*
	 * Setup Timer to control how long OSM should wait before performing
	 * DCVS when a LLM down frequency request is received.
	 * Time is specified in us.
	 */
	rc = of_property_read_u32_array(of, "qcom,llm-freq-down-timer",
					array, MAX_CLUSTER_CNT);
	if (rc) {
		dev_dbg(&pdev->dev, "No LLM Frequency down timer value: %d\n",
			rc);
	} else {
		val = clk_osm_read_reg(&pwrcl_clk, LLM_FREQ_VOTE_HYSTERESIS)
			| BVAL(15, 0, clk_osm_count_us(&pwrcl_clk,
					       array[pwrcl_clk.cluster_num]));
		clk_osm_write_reg(&pwrcl_clk, val, LLM_FREQ_VOTE_HYSTERESIS);
		val = clk_osm_read_reg(&perfcl_clk, LLM_FREQ_VOTE_HYSTERESIS)
			| BVAL(15, 0, clk_osm_count_us(&perfcl_clk,
					       array[perfcl_clk.cluster_num]));
		clk_osm_write_reg(&perfcl_clk, val, LLM_FREQ_VOTE_HYSTERESIS);
	}

	/* Enable or disable honoring of LLM frequency requests */
	rc = of_property_read_bool(pdev->dev.of_node,
					"qcom,enable-llm-freq-vote");
	if (rc) {
		dev_dbg(&pdev->dev, "Honoring LLM Frequency requests\n");
		val = 0;
	} else
		val = 1;

	/* Enable or disable LLM FREQ DVCS */
	regval = val | clk_osm_read_reg(&pwrcl_clk, LLM_INTF_DCVS_DISABLE);
	clk_osm_write_reg(&pwrcl_clk, regval, LLM_INTF_DCVS_DISABLE);
	regval = val | clk_osm_read_reg(&perfcl_clk, LLM_INTF_DCVS_DISABLE);
	clk_osm_write_reg(&perfcl_clk, regval, LLM_INTF_DCVS_DISABLE);

	/* Wait for the write to complete */
	mb();

	devm_kfree(&pdev->dev, array);
	return 0;
}

static int clk_osm_set_llm_volt_policy(struct platform_device *pdev)
{
	struct device_node *of = pdev->dev.of_node;
	u32 *array;
	int rc = 0, val, regval;

	array = devm_kzalloc(&pdev->dev, MAX_CLUSTER_CNT * sizeof(u32),
			     GFP_KERNEL);
	if (!array)
		return -ENOMEM;

	/*
	 * Setup Timer to control how long OSM should wait before performing
	 * DCVS when a LLM up voltage request is received.
	 * Time is specified in us.
	 */
	rc = of_property_read_u32_array(of, "qcom,llm-volt-up-timer",
					array, MAX_CLUSTER_CNT);
	if (rc) {
		dev_dbg(&pdev->dev, "No LLM voltage up timer value, rc=%d\n",
			rc);
	} else {
		val = clk_osm_read_reg(&pwrcl_clk, LLM_VOLT_VOTE_HYSTERESIS)
			| BVAL(31, 16, clk_osm_count_us(&pwrcl_clk,
						array[pwrcl_clk.cluster_num]));
		clk_osm_write_reg(&pwrcl_clk, val, LLM_VOLT_VOTE_HYSTERESIS);
		val = clk_osm_read_reg(&perfcl_clk, LLM_VOLT_VOTE_HYSTERESIS)
			| BVAL(31, 16, clk_osm_count_us(&perfcl_clk,
						array[perfcl_clk.cluster_num]));
		clk_osm_write_reg(&perfcl_clk, val, LLM_VOLT_VOTE_HYSTERESIS);
	}

	/*
	 * Setup Timer to control how long OSM should wait before performing
	 * DCVS when a LLM down voltage request is received.
	 * Time is specified in us.
	 */
	rc = of_property_read_u32_array(of, "qcom,llm-volt-down-timer",
					array, MAX_CLUSTER_CNT);
	if (rc) {
		dev_dbg(&pdev->dev, "No LLM Voltage down timer value: %d\n",
									rc);
	} else {
		val = clk_osm_read_reg(&pwrcl_clk, LLM_VOLT_VOTE_HYSTERESIS)
			| BVAL(15, 0, clk_osm_count_us(&pwrcl_clk,
					       array[pwrcl_clk.cluster_num]));
		clk_osm_write_reg(&pwrcl_clk, val, LLM_VOLT_VOTE_HYSTERESIS);
		val = clk_osm_read_reg(&perfcl_clk, LLM_VOLT_VOTE_HYSTERESIS)
			| BVAL(15, 0, clk_osm_count_us(&perfcl_clk,
					       array[perfcl_clk.cluster_num]));
		clk_osm_write_reg(&perfcl_clk, val, LLM_VOLT_VOTE_HYSTERESIS);
	}

	/* Enable or disable honoring of LLM Voltage requests */
	rc = of_property_read_bool(pdev->dev.of_node,
					"qcom,enable-llm-volt-vote");
	if (rc) {
		dev_dbg(&pdev->dev, "Honoring LLM Voltage requests\n");
		val = 0;
	} else
		val = BIT(1);

	/* Enable or disable LLM VOLT DVCS */
	regval = val | clk_osm_read_reg(&pwrcl_clk, LLM_INTF_DCVS_DISABLE);
	clk_osm_write_reg(&pwrcl_clk, val, LLM_INTF_DCVS_DISABLE);
	regval = val | clk_osm_read_reg(&perfcl_clk, LLM_INTF_DCVS_DISABLE);
	clk_osm_write_reg(&perfcl_clk, val, LLM_INTF_DCVS_DISABLE);

	/* Wait for the writes to complete */
	mb();

	devm_kfree(&pdev->dev, array);
	return 0;
}

static void clk_osm_program_apm_regs(struct clk_osm *c)
{
	/*
	 * Program address of the control register used to configure
	 * the Array Power Mux controller
	 */
	clk_osm_write_reg(c, c->apm_mode_ctl, SEQ_REG(2));

	/* Program mode value to switch APM from VDD_APCC to VDD_MX */
	clk_osm_write_reg(c, APM_MX_MODE, SEQ_REG(22));

	/* Program mode value to switch APM from VDD_MX to VDD_APCC */
	clk_osm_write_reg(c, APM_APC_MODE, SEQ_REG(25));

	/* Program address of controller status register */
	clk_osm_write_reg(c, c->apm_ctrl_status, SEQ_REG(3));

	/* Program mask used to determine status of APM power supply switch */
	clk_osm_write_reg(c, APM_MODE_SWITCH_MASK, SEQ_REG(24));

	/* Program value used to determine current APM power supply is VDD_MX */
	clk_osm_write_reg(c, APM_MX_MODE_VAL, SEQ_REG(23));

	/*
	 * Program value used to determine current APM power supply
	 * is VDD_APCC
	 */
	clk_osm_write_reg(c, APM_APC_MODE_VAL, SEQ_REG(26));
}

void clk_osm_setup_sequencer(struct clk_osm *c)
{
	u32 i;

	pr_debug("Setting up sequencer for cluster=%d\n", c->cluster_num);
	for (i = 0; i < ARRAY_SIZE(seq_instr); i++) {
		clk_osm_write_reg(c, seq_instr[i],
				  (long)(SEQ_MEM_ADDR + i * 4));
	}

	pr_debug("Setting up sequencer branch instructions for cluster=%d\n",
		c->cluster_num);
	for (i = 0; i < ARRAY_SIZE(seq_br_instr); i++) {
		clk_osm_write_reg(c, seq_br_instr[i],
				  (long)(SEQ_CFG_BR_ADDR + i * 4));
	}
}

static void clk_osm_setup_cycle_counters(struct clk_osm *c)
{
	u32 ratio = c->osm_clk_rate;
	u32 val = 0;

	/* Enable cycle counter */
	val |= BIT(0);
	/* Setup OSM clock to XO ratio */
	do_div(ratio, c->xo_clk_rate);
	val |= BVAL(5, 1, ratio - 1);
	clk_osm_write_reg(c, val, OSM_CYCLE_COUNTER_CTRL_REG);
	pr_debug("OSM to XO clock ratio: %d\n", ratio);
}

static void clk_osm_setup_osm_was(struct clk_osm *c)
{
	u32 val;

	clk_osm_write_reg(c, c->pbases[OSM_BASE] + SEQ_REG(42) +
			  c->cluster_num *
			  OSM_CORE_TABLE_SIZE, SEQ_REG(40));
	clk_osm_write_reg(c, c->pbases[OSM_BASE] + SEQ_REG(43) +
			  c->cluster_num *
			  OSM_CORE_TABLE_SIZE, SEQ_REG(41));
	clk_osm_write_reg(c, 0x1, SEQ_REG(44));
	clk_osm_write_reg(c, 0x0, SEQ_REG(45));
	clk_osm_write_reg(c, c->pbases[OSM_BASE] + PDN_FSM_CTRL_REG +
			  c->cluster_num *
			  OSM_CORE_TABLE_SIZE, SEQ_REG(46));

	val = clk_osm_read_reg(c, PDN_FSM_CTRL_REG);
	val |= IGNORE_PLL_LOCK_MASK;
	clk_osm_write_reg(c, val, SEQ_REG(47));
	val &= ~IGNORE_PLL_LOCK_MASK;
	clk_osm_write_reg(c, val, SEQ_REG(48));
}

static void clk_osm_do_additional_setup(struct clk_osm *c,
					struct platform_device *pdev)
{
	u32 val;

	if (!of_property_read_bool(pdev->dev.of_node, "qcom,osm-no-tz"))
		return;

	dev_info(&pdev->dev, "Performing additional OSM setup due to lack of TZ for cluster=%d\n",
		 c->cluster_num);

	clk_osm_write_reg(c, BVAL(23, 16, 0xF), SPM_CC_CTRL);

	/* PLL LVAL programming */
	clk_osm_write_reg(c, c->l_val_base, SEQ_REG(0));
	clk_osm_write_reg(c, PLL_MIN_LVAL, SEQ_REG(21));

	/* PLL post-div programming */
	clk_osm_write_reg(c, c->apcs_pll_user_ctl, SEQ_REG(18));
	clk_osm_write_reg(c, PLL_POST_DIV2, SEQ_REG(19));
	clk_osm_write_reg(c, PLL_POST_DIV1, SEQ_REG(29));

	/* APM Programming */
	clk_osm_program_apm_regs(c);

	/* GFMUX Programming */
	clk_osm_write_reg(c, c->apcs_cfg_rcgr, SEQ_REG(16));
	clk_osm_write_reg(c, GPLL_SEL, SEQ_REG(17));
	clk_osm_write_reg(c, PLL_EARLY_SEL, SEQ_REG(20));
	clk_osm_write_reg(c, PLL_MAIN_SEL, SEQ_REG(32));
	clk_osm_write_reg(c, c->apcs_cmd_rcgr, SEQ_REG(33));
	clk_osm_write_reg(c, RCG_UPDATE, SEQ_REG(34));
	clk_osm_write_reg(c, RCG_UPDATE_SUCCESS, SEQ_REG(35));
	clk_osm_write_reg(c, RCG_UPDATE, SEQ_REG(36));

	/* Reduction FSM */
	if (c->red_fsm_en) {
		val = clk_osm_read_reg(c, VMIN_REDUC_ENABLE_REG) | BIT(0);
		clk_osm_write_reg(c, val, VMIN_REDUC_ENABLE_REG);
		clk_osm_write_reg(c, BVAL(15, 0, clk_osm_count_us(c, 10)),
				  VMIN_REDUC_TIMER_REG);
	}

	/* Boost FSM */
	if (c->boost_fsm_en) {
		val = clk_osm_read_reg(c, PDN_FSM_CTRL_REG);
		clk_osm_write_reg(c, val | CC_BOOST_EN_MASK, PDN_FSM_CTRL_REG);
		val = clk_osm_read_reg(c, CC_BOOST_TIMER_REG0);
		val |= BVAL(15, 0, clk_osm_count_us(c, PLL_WAIT_LOCK_TIME_US));
		val |= BVAL(31, 16, clk_osm_count_us(c,
						     SAFE_FREQ_WAIT_US));
		clk_osm_write_reg(c, val, CC_BOOST_TIMER_REG0);

		val = clk_osm_read_reg(c, CC_BOOST_TIMER_REG1);
		val |= BVAL(15, 0, clk_osm_count_us(c, PLL_WAIT_LOCK_TIME_US));
		val |= BVAL(31, 16, clk_osm_count_us(c, SAFE_FREQ_WAIT_US));
		clk_osm_write_reg(c, val, CC_BOOST_TIMER_REG1);

		val = clk_osm_read_reg(c, CC_BOOST_TIMER_REG2);
		val |= BVAL(15, 0, clk_osm_count_us(c, PLL_WAIT_LOCK_TIME_US));
		clk_osm_write_reg(c, val, CC_BOOST_TIMER_REG2);
	}

	/* Safe Freq FSM */
	if (c->safe_fsm_en) {
		val = clk_osm_read_reg(c, PDN_FSM_CTRL_REG);
		clk_osm_write_reg(c, val | DCVS_BOOST_EN_MASK,
				  PDN_FSM_CTRL_REG);

		val = clk_osm_read_reg(c, DCVS_BOOST_TIMER_REG0);
		val |= BVAL(31, 16, clk_osm_count_us(c, SAFE_FREQ_WAIT_US));
		clk_osm_write_reg(c, val, DCVS_BOOST_TIMER_REG0);

		val = clk_osm_read_reg(c, DCVS_BOOST_TIMER_REG1);
		val |= BVAL(15, 0, clk_osm_count_us(c, PLL_WAIT_LOCK_TIME_US));
		clk_osm_write_reg(c, val, DCVS_BOOST_TIMER_REG1);
	}

	/* PS FSM */
	if (c->ps_fsm_en) {
		val = clk_osm_read_reg(c, PDN_FSM_CTRL_REG);
		clk_osm_write_reg(c, val | PS_BOOST_EN_MASK, PDN_FSM_CTRL_REG);

		val = clk_osm_read_reg(c, PS_BOOST_TIMER_REG0) |
			BVAL(31, 16, clk_osm_count_us(c, 1));
		clk_osm_write_reg(c, val, PS_BOOST_TIMER_REG0);

		val = clk_osm_read_reg(c, PS_BOOST_TIMER_REG1) |
			clk_osm_count_us(c, 1);
		clk_osm_write_reg(c, val, PS_BOOST_TIMER_REG1);
	}

	/* PLL signal timing control */
	if (c->boost_fsm_en || c->safe_fsm_en || c->ps_fsm_en)
		clk_osm_write_reg(c, 0x5, BOOST_PROG_SYNC_DELAY_REG);

	/* Droop FSM */
	if (c->wfx_fsm_en) {
		/* WFx FSM */
		val = clk_osm_read_reg(c, PDN_FSM_CTRL_REG);
		clk_osm_write_reg(c, val | WFX_DROOP_EN_MASK, PDN_FSM_CTRL_REG);

		val = clk_osm_read_reg(c, DROOP_UNSTALL_TIMER_CTRL_REG) |
			BVAL(31, 16, clk_osm_count_us(c, 1));
		clk_osm_write_reg(c, val, DROOP_UNSTALL_TIMER_CTRL_REG);

		val = clk_osm_read_reg(c,
			       DROOP_WAIT_TO_RELEASE_TIMER_CTRL0_REG) |
			BVAL(31, 16, clk_osm_count_us(c, 1));
		clk_osm_write_reg(c, val,
				  DROOP_WAIT_TO_RELEASE_TIMER_CTRL0_REG);
	}

	/* PC/RET FSM */
	if (c->pc_fsm_en) {
		val = clk_osm_read_reg(c, PDN_FSM_CTRL_REG);
		clk_osm_write_reg(c, val | PC_RET_EXIT_DROOP_EN_MASK,
				  PDN_FSM_CTRL_REG);

		val = clk_osm_read_reg(c, DROOP_UNSTALL_TIMER_CTRL_REG) |
			BVAL(15, 0, clk_osm_count_us(c, 1));
		clk_osm_write_reg(c, val, DROOP_UNSTALL_TIMER_CTRL_REG);
	}

	/* DCVS droop FSM - only if RCGwRC is not used for di/dt control */
	if (c->droop_fsm_en) {
		val = clk_osm_read_reg(c, PDN_FSM_CTRL_REG);
		clk_osm_write_reg(c, val | DCVS_DROOP_EN_MASK,
				  PDN_FSM_CTRL_REG);
	}

	if (c->wfx_fsm_en || c->ps_fsm_en || c->droop_fsm_en) {
		val = clk_osm_read_reg(c,
			       DROOP_WAIT_TO_RELEASE_TIMER_CTRL0_REG) |
			BVAL(15, 0, clk_osm_count_us(c, 1));
		clk_osm_write_reg(c, val,
			  DROOP_WAIT_TO_RELEASE_TIMER_CTRL0_REG);
		clk_osm_write_reg(c, 0x1, DROOP_PROG_SYNC_DELAY_REG);
		val = clk_osm_read_reg(c, DROOP_CTRL_REG) |
			BVAL(22, 16, 0x2);
		clk_osm_write_reg(c, val, DROOP_CTRL_REG);
	}

	pr_debug("seq_size: %lu, seqbr_size: %lu\n", ARRAY_SIZE(seq_instr),
						ARRAY_SIZE(seq_br_instr));
	clk_osm_setup_sequencer(&pwrcl_clk);
	clk_osm_setup_sequencer(&perfcl_clk);
}

static void clk_osm_apm_vc_setup(struct clk_osm *c)
{
	/*
	 * APM crossover virtual corner at which the switch
	 * from APC to MX and vice-versa should take place.
	 */
	clk_osm_write_reg(c, c->apm_crossover_vc, SEQ_REG(1));

	/* Ensure writes complete before delaying */
	mb();
}

static irqreturn_t clk_osm_debug_irq_cb(int irq, void *data)
{
	struct clk_osm *c = data;
	unsigned long first, second, total_delta = 0;
	u32 val, factor;
	int i;

	val = clk_osm_read_reg(c, DCVS_PERF_STATE_DEVIATION_INTR_STAT);
	if (val & BIT(0)) {
		pr_info("OS DCVS performance state deviated\n");
		clk_osm_write_reg(c, BIT(0),
				  DCVS_PERF_STATE_DEVIATION_INTR_CLEAR);
	}

	val = clk_osm_read_reg(c,
			       DCVS_PERF_STATE_DEVIATION_CORRECTED_INTR_STAT);
	if (val & BIT(0)) {
		pr_info("OS DCVS performance state corrected\n");
		clk_osm_write_reg(c, BIT(0),
			  DCVS_PERF_STATE_DEVIATION_CORRECTED_INTR_CLEAR);
	}

	val = clk_osm_read_reg(c, DCVS_PERF_STATE_MET_INTR_STAT);
	if (val & BIT(0)) {
		pr_info("OS DCVS performance state desired reached\n");
		clk_osm_write_reg(c, BIT(0), DCVS_PERF_STATE_MET_INTR_CLR);
	}

	factor = c->cycle_counter_factor ? c->cycle_counter_factor : 1;

	for (i = 0; i < c->cycle_counter_reads; i++) {
		first = clk_osm_read_reg(c, OSM_CYCLE_COUNTER_STATUS_REG);

		if (c->cycle_counter_delay)
			udelay(c->cycle_counter_delay);

		second = clk_osm_read_reg(c, OSM_CYCLE_COUNTER_STATUS_REG);
		total_delta = total_delta + ((second - first) / factor);
	}

	pr_info("cluster=%d, L_VAL (estimated)=%lu\n",
		c->cluster_num, total_delta / c->cycle_counter_factor);

	return IRQ_HANDLED;
}

static int clk_osm_setup_irq(struct platform_device *pdev, struct clk_osm *c,
			 char *irq_name)
{
	int rc = 0;

	rc = c->irq = platform_get_irq_byname(pdev, irq_name);
	if (rc < 0) {
		dev_err(&pdev->dev, "%s irq not specified\n", irq_name);
		return rc;
	}

	rc = devm_request_irq(&pdev->dev, c->irq,
			      clk_osm_debug_irq_cb,
			      IRQF_TRIGGER_RISING | IRQF_SHARED,
			      "OSM IRQ", c);
	if (rc)
		dev_err(&pdev->dev, "Request IRQ failed for OSM IRQ\n");

	return rc;
}

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

static int add_opp(struct clk_osm *c, struct device *dev,
	   unsigned long max_rate, unsigned long min_rate)
{
	unsigned long rate = 0;
	u32 uv;
	long rc;
	int j = 1;

	while (1) {
		rate = c->c.fmax[j++];
		uv = find_voltage(c, rate);
		if (uv <= 0) {
			pr_warn("No voltage for %lu.\n", rate);
			return -EINVAL;
		}

		rc = dev_pm_opp_add(dev, rate, uv);
		if (rc) {
			pr_warn("failed to add OPP for %lu\n", rate);
			return rc;
		}

		/*
		 * Print the OPP pair for the lowest and highest frequency for
		 * each device that we're populating. This is important since
		 * this information will be used by thermal mitigation and the
		 * scheduler.
		 */
		if (rate == min_rate)
			pr_info("Set OPP pair (%lu Hz, %d uv) on %s\n",
				rate, uv, dev_name(dev));
		if (rate == max_rate) {
			pr_info("Set OPP pair (%lu Hz, %d uv) on %s\n",
				rate, uv, dev_name(dev));
			break;
		}
	}

	return 0;
}

static struct clk *logical_cpu_to_clk(int cpu)
{
	struct device_node *cpu_node;
	const u32 *cell;
	u64 hwid;
	static struct clk *cpu_clk_map[NR_CPUS];

	if (cpu_clk_map[cpu])
		return cpu_clk_map[cpu];

	cpu_node = of_get_cpu_node(cpu, NULL);
	if (!cpu_node)
		goto fail;

	cell = of_get_property(cpu_node, "reg", NULL);
	if (!cell) {
		pr_err("%s: missing reg property\n", cpu_node->full_name);
		goto fail;
	}

	hwid = of_read_number(cell, of_n_addr_cells(cpu_node));
	if ((hwid | pwrcl_clk.cpu_reg_mask) == pwrcl_clk.cpu_reg_mask) {
		cpu_clk_map[cpu] = &pwrcl_clk.c;
		return &pwrcl_clk.c;
	}
	if ((hwid | perfcl_clk.cpu_reg_mask) == perfcl_clk.cpu_reg_mask) {
		cpu_clk_map[cpu] = &perfcl_clk.c;
		return &perfcl_clk.c;
	}

fail:
	return NULL;
}

static void populate_opp_table(struct platform_device *pdev)
{
	int cpu;

	for_each_possible_cpu(cpu) {
		if (logical_cpu_to_clk(cpu) == &pwrcl_clk.c) {
			WARN(add_opp(&pwrcl_clk, get_cpu_device(cpu),
			     pwrcl_clk.c.fmax[pwrcl_clk.c.num_fmax - 1],
			     pwrcl_clk.c.fmax[1]),
			     "Failed to add OPP levels for power cluster\n");
		}
		if (logical_cpu_to_clk(cpu) == &perfcl_clk.c) {
			WARN(add_opp(&perfcl_clk, get_cpu_device(cpu),
			     perfcl_clk.c.fmax[perfcl_clk.c.num_fmax - 1],
			     perfcl_clk.c.fmax[1]),
			     "Failed to add OPP levels for perf cluster\n");
		}
	}
}

static int debugfs_get_perf_state_met_irq(void *data, u64 *val)
{
	struct clk_osm *c = data;

	*val = clk_osm_read_reg(c, DCVS_PERF_STATE_MET_INTR_EN);
	return 0;
}

static int debugfs_set_perf_state_met_irq(void *data, u64 val)
{
	struct clk_osm *c = data;

	clk_osm_write_reg(c, val ? 1 : 0,
			  DCVS_PERF_STATE_MET_INTR_EN);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(debugfs_perf_state_met_irq_fops,
			debugfs_get_perf_state_met_irq,
			debugfs_set_perf_state_met_irq,
			"%llu\n");

static int debugfs_get_perf_state_deviation_irq(void *data, u64 *val)
{
	struct clk_osm *c = data;

	*val = clk_osm_read_reg(c,
				DCVS_PERF_STATE_DEVIATION_INTR_EN);
	return 0;
}

static int debugfs_set_perf_state_deviation_irq(void *data, u64 val)
{
	struct clk_osm *c = data;

	clk_osm_write_reg(c, val ? 1 : 0,
			  DCVS_PERF_STATE_DEVIATION_INTR_EN);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(debugfs_perf_state_deviation_irq_fops,
			debugfs_get_perf_state_deviation_irq,
			debugfs_set_perf_state_deviation_irq,
			"%llu\n");

static int debugfs_get_perf_state_deviation_corrected_irq(void *data, u64 *val)
{
	struct clk_osm *c = data;

	*val = clk_osm_read_reg(c,
			DCVS_PERF_STATE_DEVIATION_CORRECTED_INTR_EN);
	return 0;
}

static int debugfs_set_perf_state_deviation_corrected_irq(void *data, u64 val)
{
	struct clk_osm *c = data;

	clk_osm_write_reg(c, val ? 1 : 0,
		      DCVS_PERF_STATE_DEVIATION_CORRECTED_INTR_EN);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(debugfs_perf_state_deviation_corrected_irq_fops,
			debugfs_get_perf_state_deviation_corrected_irq,
			debugfs_set_perf_state_deviation_corrected_irq,
			"%llu\n");

static void populate_debugfs_dir(struct clk_osm *c)
{
	struct dentry *temp;

	if (osm_debugfs_base == NULL) {
		osm_debugfs_base = debugfs_create_dir("osm", NULL);
		if (IS_ERR_OR_NULL(osm_debugfs_base)) {
			pr_err("osm debugfs base directory creation failed\n");
			osm_debugfs_base = NULL;
			return;
		}
	}

	c->debugfs = debugfs_create_dir(c->c.dbg_name, osm_debugfs_base);
	if (IS_ERR_OR_NULL(c->debugfs)) {
		pr_err("osm debugfs directory creation failed\n");
		return;
	}

	temp = debugfs_create_file("perf_state_met_irq_enable",
				   S_IRUGO | S_IWUSR,
				   c->debugfs, c,
				   &debugfs_perf_state_met_irq_fops);
	if (IS_ERR_OR_NULL(temp)) {
		pr_err("perf_state_met_irq_enable debugfs file creation failed\n");
		goto exit;
	}

	temp = debugfs_create_file("perf_state_deviation_irq_enable",
				   S_IRUGO | S_IWUSR,
				   c->debugfs, c,
				   &debugfs_perf_state_deviation_irq_fops);
	if (IS_ERR_OR_NULL(temp)) {
		pr_err("perf_state_deviation_irq_enable debugfs file creation failed\n");
		goto exit;
	}

	temp = debugfs_create_file("perf_state_deviation_corrected_irq_enable",
			   S_IRUGO | S_IWUSR,
			   c->debugfs, c,
			   &debugfs_perf_state_deviation_corrected_irq_fops);
	if (IS_ERR_OR_NULL(temp)) {
		pr_err("debugfs_perf_state_deviation_corrected_irq_fops debugfs file creation failed\n");
		goto exit;
	}

exit:
	if (IS_ERR_OR_NULL(temp))
		debugfs_remove_recursive(c->debugfs);
}

static unsigned long init_rate = 300000000;

static int cpu_clock_osm_driver_probe(struct platform_device *pdev)
{
	char perfclspeedbinstr[] = "qcom,perfcl-speedbin0-v0";
	char pwrclspeedbinstr[] = "qcom,pwrcl-speedbin0-v0";
	int rc;

	rc = clk_osm_resources_init(pdev);
	if (rc) {
		if (rc != -EPROBE_DEFER)
			dev_err(&pdev->dev, "resources init failed, rc=%d\n",
				rc);
		return rc;
	}

	rc = clk_osm_parse_dt_configs(pdev);
	if (rc) {
		dev_err(&pdev->dev, "Unable to parse device tree configurations\n");
		return rc;
	}

	rc = clk_osm_get_lut(pdev, &pwrcl_clk,
			     pwrclspeedbinstr);
	if (rc) {
		dev_err(&pdev->dev, "Unable to get OSM LUT for power cluster, rc=%d\n",
			rc);
		return rc;
	}

	rc = clk_osm_get_lut(pdev, &perfcl_clk, perfclspeedbinstr);
	if (rc) {
		dev_err(&pdev->dev, "Unable to get OSM LUT for perf cluster, rc=%d\n",
			rc);
		return rc;
	}

	rc = clk_osm_resolve_open_loop_voltages(&pwrcl_clk);
	if (rc) {
		if (rc == -EPROBE_DEFER)
			return rc;
		dev_err(&pdev->dev, "Unable to determine open-loop voltages for power cluster, rc=%d\n",
			rc);
		return rc;
	}

	rc = clk_osm_resolve_open_loop_voltages(&perfcl_clk);
	if (rc) {
		if (rc == -EPROBE_DEFER)
			return rc;
		dev_err(&pdev->dev, "Unable to determine open-loop voltages for perf cluster, rc=%d\n",
			rc);
		return rc;
	}

	rc = clk_osm_resolve_crossover_corners(&pwrcl_clk, pdev);
	if (rc)
		dev_info(&pdev->dev, "No APM crossover corner programmed\n");

	rc = clk_osm_resolve_crossover_corners(&perfcl_clk, pdev);
	if (rc)
		dev_info(&pdev->dev, "No APM crossover corner programmed\n");

	clk_osm_setup_cycle_counters(&pwrcl_clk);
	clk_osm_setup_cycle_counters(&perfcl_clk);

	clk_osm_print_osm_table(&pwrcl_clk);
	clk_osm_print_osm_table(&perfcl_clk);

	/* Program the minimum PLL frequency */
	clk_osm_write_reg(&pwrcl_clk, PLL_MIN_LVAL, SEQ_REG(27));
	clk_osm_write_reg(&perfcl_clk, PLL_MIN_LVAL, SEQ_REG(27));

	clk_osm_setup_hw_table(&pwrcl_clk);
	clk_osm_setup_hw_table(&perfcl_clk);

	/* Policy tuning */
	rc = clk_osm_set_cc_policy(pdev);
	if (rc < 0) {
		dev_err(&pdev->dev, "cc policy setup failed");
		goto exit;
	}

	clk_osm_setup_itm_to_osm_handoff();

	/* LLM Freq Policy Tuning */
	rc = clk_osm_set_llm_freq_policy(pdev);
	if (rc < 0) {
		dev_err(&pdev->dev, "LLM Frequency Policy setup failed");
		goto exit;
	}

	/* LLM Voltage Policy Tuning */
	rc = clk_osm_set_llm_volt_policy(pdev);
	if (rc < 0) {
		dev_err(&pdev->dev, "Failed to set LLM voltage Policy");
		goto exit;
	}

	/*
	 * Perform typical secure-world HW initialization
	 * as necessary.
	 */
	clk_osm_do_additional_setup(&pwrcl_clk, pdev);
	clk_osm_do_additional_setup(&perfcl_clk, pdev);

	/* Program APM crossover corners */
	clk_osm_apm_vc_setup(&pwrcl_clk);
	clk_osm_apm_vc_setup(&perfcl_clk);

	rc = clk_osm_setup_irq(pdev, &pwrcl_clk, "pwrcl-irq");
	if (rc)
		pr_err("Debug IRQ not set for pwrcl\n");

	rc = clk_osm_setup_irq(pdev, &perfcl_clk, "perfcl-irq");
	if (rc)
		pr_err("Debug IRQ not set for perfcl\n");

	clk_osm_setup_osm_was(&pwrcl_clk);
	clk_osm_setup_osm_was(&perfcl_clk);

	if (of_property_read_bool(pdev->dev.of_node,
				  "qcom,osm-pll-setup")) {
		clk_osm_setup_cluster_pll(&pwrcl_clk);
		clk_osm_setup_cluster_pll(&perfcl_clk);
	}

	rc = of_msm_clock_register(pdev->dev.of_node, cpu_clocks_osm,
				   ARRAY_SIZE(cpu_clocks_osm));
	if (rc) {
		dev_err(&pdev->dev, "Unable to register CPU clocks, rc=%d\n",
			rc);
		return rc;
	}

	/*
	 * The hmss_gpll0 clock runs at 300 MHz. Ensure it is at the correct
	 * frequency before enabling OSM. LUT index 0 is always sourced from
	 * this clock.
	 */
	rc = clk_set_rate(&sys_apcsaux_clk_gcc.c, init_rate);
	if (rc) {
		dev_err(&pdev->dev, "Unable to set init rate on hmss_gpll0, rc=%d\n",
			rc);
		return rc;
	}
	clk_prepare_enable(&sys_apcsaux_clk_gcc.c);

	/* Set 300MHz index */
	rc = clk_set_rate(&pwrcl_clk.c, init_rate);
	if (rc) {
		dev_err(&pdev->dev, "Unable to set init rate on pwr cluster, rc=%d\n",
			rc);
		clk_disable_unprepare(&sys_apcsaux_clk_gcc.c);
		return rc;
	}

	rc = clk_set_rate(&perfcl_clk.c, init_rate);
	if (rc) {
		dev_err(&pdev->dev, "Unable to set init rate on perf cluster, rc=%d\n",
			rc);
		clk_disable_unprepare(&sys_apcsaux_clk_gcc.c);
		return rc;
	}

	/* Enable OSM */
	WARN(clk_prepare_enable(&pwrcl_clk.c),
	     "Failed to enable power cluster clock\n");
	WARN(clk_prepare_enable(&perfcl_clk.c),
	     "Failed to enable perf cluster clock\n");

	populate_opp_table(pdev);
	populate_debugfs_dir(&pwrcl_clk);
	populate_debugfs_dir(&perfcl_clk);

	pr_info("OSM driver inited\n");
	return 0;

exit:
	dev_err(&pdev->dev, "OSM driver failed to initialize, rc=%d",
		rc);
	panic("Unable to Setup OSM");
}

static struct of_device_id match_table[] = {
	{ .compatible = "qcom,cpu-clock-osm" },
	{}
};

static struct platform_driver cpu_clock_osm_driver = {
	.probe = cpu_clock_osm_driver_probe,
	.driver = {
		.name = "cpu-clock-osm",
		.of_match_table = match_table,
		.owner = THIS_MODULE,
	},
};

static int __init cpu_clock_osm_init(void)
{
	return platform_driver_register(&cpu_clock_osm_driver);
}
arch_initcall(cpu_clock_osm_init);

static void __exit cpu_clock_osm_exit(void)
{
	platform_driver_unregister(&cpu_clock_osm_driver);
}
module_exit(cpu_clock_osm_exit);

MODULE_DESCRIPTION("CPU clock driver for OSM");
MODULE_LICENSE("GPL v2");
