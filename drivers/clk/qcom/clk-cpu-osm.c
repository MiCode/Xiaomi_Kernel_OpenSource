/*
 * Copyright (c) 2017, The Linux Foundation. All rights reserved.
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
#include <linux/clk-provider.h>
#include <linux/cpu.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/pm_opp.h>
#include <linux/pm_qos.h>
#include <linux/interrupt.h>
#include <linux/regulator/driver.h>
#include <linux/regmap.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/cpufreq.h>
#include <linux/slab.h>
#include <soc/qcom/scm.h>
#include <dt-bindings/clock/qcom,cpucc-sdm845.h>

#include "common.h"
#include "clk-regmap.h"
#include "clk-rcg.h"
#include "clk-voter.h"
#include "clk-debug.h"

#define OSM_INIT_RATE			300000000UL
#define OSM_TABLE_SIZE			40
#define SINGLE_CORE			1
#define MAX_CLUSTER_CNT			3
#define MAX_MEM_ACC_VAL_PER_LEVEL	3
#define MAX_CORE_COUNT			4
#define CORE_COUNT_VAL(val)		((val & GENMASK(18, 16)) >> 16)

#define OSM_CYCLE_COUNTER_CTRL_REG		0x760
#define OSM_CYCLE_COUNTER_USE_XO_EDGE_EN	BIT(8)

#define OSM_REG_SIZE			32

#define L3_EFUSE_SHIFT			29
#define L3_EFUSE_MASK			0x7
#define PWRCL_EFUSE_SHIFT		29
#define PWRCL_EFUSE_MASK		0x7
#define PERFCL_EFUSE_SHIFT		29
#define PERFCL_EFUSE_MASK		0x7

#define ENABLE_REG			0x0
#define FREQ_REG			0x110
#define VOLT_REG			0x114
#define OVERRIDE_REG			0x118
#define SPM_CC_INC_HYSTERESIS		0x1c
#define SPM_CC_DEC_HYSTERESIS		0x20
#define SPM_CORE_INACTIVE_MAPPING	0x28
#define CC_ZERO_BEHAV_CTRL		0xc
#define ENABLE_OVERRIDE			BIT(0)
#define SPM_CC_DCVS_DISABLE		0x24
#define LLM_FREQ_VOTE_INC_HYSTERESIS	0x30
#define LLM_FREQ_VOTE_DEC_HYSTERESIS	0x34
#define LLM_INTF_DCVS_DISABLE		0x40
#define LLM_VOLTAGE_VOTE_INC_HYSTERESIS	0x38
#define LLM_VOLTAGE_VOTE_DEC_HYSTERESIS	0x3c
#define VMIN_REDUCTION_ENABLE_REG	0x48
#define VMIN_REDUCTION_TIMER_REG	0x4c
#define PDN_FSM_CTRL_REG		0x54
#define DELTA_DEX_VAL			BVAL(31, 23, 0xa)
#define IGNORE_PLL_LOCK			BIT(15)
#define CC_BOOST_FSM_EN			BIT(0)
#define CC_BOOST_FSM_TIMERS_REG0	0x58
#define CC_BOOST_FSM_TIMERS_REG1	0x5c
#define CC_BOOST_FSM_TIMERS_REG2	0x60
#define DCVS_BOOST_FSM_EN_MASK		BIT(2)
#define DCVS_BOOST_FSM_TIMERS_REG0	0x64
#define DCVS_BOOST_FSM_TIMERS_REG1	0x68
#define DCVS_BOOST_FSM_TIMERS_REG2	0x6c
#define PS_BOOST_FSM_EN_MASK		BIT(1)
#define PS_BOOST_FSM_TIMERS_REG0	0x74
#define PS_BOOST_FSM_TIMERS_REG1	0x78
#define PS_BOOST_FSM_TIMERS_REG2	0x7c
#define BOOST_PROG_SYNC_DELAY_REG	0x80
#define DCVS_DROOP_FSM_EN_MASK		BIT(5)
#define DROOP_PROG_SYNC_DELAY_REG	0x9c
#define DROOP_RELEASE_TIMER_CTRL	0x88
#define DROOP_CTRL_REG			0x84
#define DCVS_DROOP_TIMER_CTRL		0x98
#define PLL_SW_OVERRIDE_ENABLE		0xa0
#define PLL_SW_OVERRIDE_DROOP_EN	BIT(0)
#define SPM_CORE_COUNT_CTRL		0x2c
#define CORE_DCVS_CTRL			0xbc
#define OVERRIDE_CLUSTER_IDLE_ACK	0x800
#define REQ_GEN_FSM_STATUS		0x70c

#define PLL_MIN_LVAL			0x21
#define PLL_MIN_FREQ_REG		0x94
#define PLL_POST_DIV1			0x1F
#define PLL_POST_DIV2			0x11F
#define PLL_MODE			0x0
#define PLL_L_VAL			0x4
#define PLL_USER_CTRL			0xc
#define PLL_CONFIG_CTL_LO		0x10
#define PLL_CONFIG_CTL_HI		0x14
#define MIN_VCO_VAL			0x2b

#define MAX_VC				63
#define MAX_MEM_ACC_LEVELS		3
#define MAX_MEM_ACC_VAL_PER_LEVEL	3
#define MAX_MEM_ACC_VALUES		(MAX_MEM_ACC_LEVELS * \
					MAX_MEM_ACC_VAL_PER_LEVEL)
#define MEM_ACC_ADDRS			3

#define ISENSE_ON_DATA			0xf
#define ISENSE_OFF_DATA			0x0
#define CONSTANT_32			0x20

#define APM_MX_MODE			0x0
#define APM_APC_MODE			0x2
#define APM_READ_DATA_MASK		0xc
#define APM_MX_MODE_VAL			0x4
#define APM_APC_READ_VAL		0x8
#define APM_MX_READ_VAL			0x4
#define APM_CROSSOVER_VC		0xb0

#define MEM_ACC_SEQ_CONST(n)		(n)
#define MEM_ACC_APM_READ_MASK		0xff
#define MEMACC_CROSSOVER_VC		0xb8

#define PLL_WAIT_LOCK_TIME_US		10
#define PLL_WAIT_LOCK_TIME_NS		(PLL_WAIT_LOCK_TIME_US * 1000)
#define SAFE_FREQ_WAIT_NS		5000
#define DEXT_DECREMENT_WAIT_NS		1000

#define DATA_MEM(n)			(0x400 + (n) * 4)

#define DCVS_PERF_STATE_DESIRED_REG_0	0x780
#define DCVS_PERF_STATE_DESIRED_REG(n) (DCVS_PERF_STATE_DESIRED_REG_0 + \
					(4 * n))
#define OSM_CYCLE_COUNTER_STATUS_REG_0	0x7d0
#define OSM_CYCLE_COUNTER_STATUS_REG(n)	(OSM_CYCLE_COUNTER_STATUS_REG_0 + \
					(4 * n))

/* ACD registers */
#define ACD_HW_VERSION		0x0
#define ACDCR			0x4
#define ACDTD			0x8
#define ACDSSCR			0x28
#define ACD_EXTINT_CFG		0x30
#define ACD_DCVS_SW		0x34
#define ACD_GFMUX_CFG		0x3c
#define ACD_READOUT_CFG		0x48
#define ACD_AVG_CFG_0		0x4c
#define ACD_AVG_CFG_1		0x50
#define ACD_AVG_CFG_2		0x54
#define ACD_AUTOXFER_CFG	0x80
#define ACD_AUTOXFER		0x84
#define ACD_AUTOXFER_CTL	0x88
#define ACD_AUTOXFER_STATUS	0x8c
#define ACD_WRITE_CTL		0x90
#define ACD_WRITE_STATUS	0x94
#define ACD_READOUT		0x98

#define ACD_MASTER_ONLY_REG_ADDR	0x80
#define ACD_1P1_MAX_REG_OFFSET		0x100
#define ACD_WRITE_CTL_UPDATE_EN		BIT(0)
#define ACD_WRITE_CTL_SELECT_SHIFT	1
#define ACD_GFMUX_CFG_SELECT		BIT(0)
#define ACD_AUTOXFER_START_CLEAR	0
#define ACD_AUTOXFER_START_SET		1
#define AUTO_XFER_DONE_MASK		BIT(0)
#define ACD_DCVS_SW_DCVS_IN_PRGR_SET	BIT(0)
#define ACD_DCVS_SW_DCVS_IN_PRGR_CLEAR	0
#define ACD_LOCAL_TRANSFER_TIMEOUT_NS   500

#define ACD_REG_RELATIVE_ADDR(addr) (addr / 4)
#define ACD_REG_RELATIVE_ADDR_BITMASK(addr) \
			(1 << (ACD_REG_RELATIVE_ADDR(addr)))

static const struct regmap_config osm_qcom_regmap_config = {
	.reg_bits       = 32,
	.reg_stride     = 4,
	.val_bits       = 32,
	.fast_io	= true,
};

enum clk_osm_bases {
	OSM_BASE,
	PLL_BASE,
	EFUSE_BASE,
	SEQ_BASE,
	ACD_BASE,
	NUM_BASES,
};

enum clk_osm_lut_data {
	FREQ,
	FREQ_DATA,
	PLL_OVERRIDES,
	MEM_ACC_LEVEL,
	VIRTUAL_CORNER,
	NUM_FIELDS,
};

struct osm_entry {
	u16 virtual_corner;
	u16 open_loop_volt;
	u32 freq_data;
	u32 override_data;
	u32 mem_acc_level;
	long frequency;
};

static struct dentry *osm_debugfs_base;

struct clk_osm {
	struct clk_hw hw;
	struct osm_entry osm_table[OSM_TABLE_SIZE];
	struct dentry *debugfs;
	struct regulator *vdd_reg;
	struct platform_device *vdd_dev;
	void *vbases[NUM_BASES];
	unsigned long pbases[NUM_BASES];
	spinlock_t lock;

	u32 num_entries;
	u32 cluster_num;
	u32 core_num;
	u32 apm_crossover_vc;
	u32 apm_threshold_vc;
	u32 mem_acc_crossover_vc;
	u32 mem_acc_threshold_vc;
	u32 min_cpr_vc;
	u32 cycle_counter_reads;
	u32 cycle_counter_delay;
	u32 cycle_counter_factor;
	u64 total_cycle_counter;
	u32 prev_cycle_counter;
	u32 l_val_base;
	u32 apcs_pll_user_ctl;
	u32 apcs_pll_min_freq;
	u32 cfg_gfmux_addr;
	u32 apcs_cbc_addr;
	u32 speedbin;
	u32 mem_acc_crossover_vc_addr;
	u32 mem_acc_addr[MEM_ACC_ADDRS];
	u32 ramp_ctl_addr;
	u32 apm_mode_ctl;
	u32 apm_status_ctl;
	u32 osm_clk_rate;
	u32 xo_clk_rate;
	bool secure_init;
	bool per_core_dcvs;
	bool red_fsm_en;
	bool boost_fsm_en;
	bool safe_fsm_en;
	bool ps_fsm_en;
	bool droop_fsm_en;

	struct notifier_block panic_notifier;
	u32 trace_periodic_timer;
	bool trace_en;
	bool wdog_trace_en;

	bool acd_init;
	u32 acd_td;
	u32 acd_cr;
	u32 acd_sscr;
	u32 acd_extint0_cfg;
	u32 acd_extint1_cfg;
	u32 acd_autoxfer_ctl;
	u32 acd_debugfs_addr;
	bool acd_avg_init;
	u32 acd_avg_cfg0;
	u32 acd_avg_cfg1;
	u32 acd_avg_cfg2;
};

static struct regulator *vdd_l3;
static struct regulator *vdd_pwrcl;
static struct regulator *vdd_perfcl;

static inline int clk_osm_acd_mb(struct clk_osm *c)
{
	return readl_relaxed_no_log((char *)c->vbases[ACD_BASE] +
					ACD_HW_VERSION);
}

static int clk_osm_acd_local_read_reg(struct clk_osm *c, u32 offset)
{
	u32 reg = 0;
	int timeout;

	if (offset >= ACD_MASTER_ONLY_REG_ADDR) {
		pr_err("ACD register at offset=0x%x not locally readable\n",
			offset);
		return -EINVAL;
	}

	/* Set select field in read control register */
	writel_relaxed(ACD_REG_RELATIVE_ADDR(offset),
			(char *)c->vbases[ACD_BASE] + ACD_READOUT_CFG);

	/* Clear write control register */
	writel_relaxed(reg, (char *)c->vbases[ACD_BASE] + ACD_WRITE_CTL);

	/* Set select and update_en fields in write control register */
	reg = (ACD_REG_RELATIVE_ADDR(ACD_READOUT_CFG)
	       << ACD_WRITE_CTL_SELECT_SHIFT)
		| ACD_WRITE_CTL_UPDATE_EN;
	writel_relaxed(reg, (char *)c->vbases[ACD_BASE] + ACD_WRITE_CTL);

	/* Ensure writes complete before polling */
	clk_osm_acd_mb(c);

	/* Poll write status register */
	for (timeout = ACD_LOCAL_TRANSFER_TIMEOUT_NS; timeout > 0;
	     timeout -= 100) {
		reg = readl_relaxed((char *)c->vbases[ACD_BASE]
					+ ACD_WRITE_STATUS);
		if ((reg & (ACD_REG_RELATIVE_ADDR_BITMASK(ACD_READOUT_CFG))))
			break;
		ndelay(100);
	}

	if (!timeout) {
		pr_err("local read timed out, offset=0x%x status=0x%x\n",
			offset, reg);
		return -ETIMEDOUT;
	}

	reg = readl_relaxed((char *)c->vbases[ACD_BASE] + ACD_READOUT);
	return reg;
}

static int clk_osm_acd_local_write_reg(struct clk_osm *c, u32 val, u32 offset)
{
	u32 reg = 0;
	int timeout;

	if (offset >= ACD_MASTER_ONLY_REG_ADDR) {
		pr_err("ACD register at offset=0x%x not transferrable\n",
			offset);
		return -EINVAL;
	}

	/* Clear write control register */
	writel_relaxed(reg, (char *)c->vbases[ACD_BASE] + ACD_WRITE_CTL);

	/* Set select and update_en fields in write control register */
	reg = (ACD_REG_RELATIVE_ADDR(offset) << ACD_WRITE_CTL_SELECT_SHIFT)
		| ACD_WRITE_CTL_UPDATE_EN;
	writel_relaxed(reg, (char *)c->vbases[ACD_BASE] + ACD_WRITE_CTL);

	/* Ensure writes complete before polling */
	clk_osm_acd_mb(c);

	/* Poll write status register */
	for (timeout = ACD_LOCAL_TRANSFER_TIMEOUT_NS; timeout > 0;
	     timeout -= 100) {
		reg = readl_relaxed((char *)c->vbases[ACD_BASE]
					+ ACD_WRITE_STATUS);
		if ((reg & (ACD_REG_RELATIVE_ADDR_BITMASK(offset))))
			break;
		ndelay(100);
	}

	if (!timeout) {
		pr_err("local write timed out, offset=0x%x val=0x%x status=0x%x\n",
			offset, val, reg);
		return -ETIMEDOUT;
	}

	return 0;
}

static int clk_osm_acd_master_write_through_reg(struct clk_osm *c,
						u32 val, u32 offset)
{
	writel_relaxed(val, (char *)c->vbases[ACD_BASE] + offset);

	/* Ensure writes complete before transfer to local copy */
	clk_osm_acd_mb(c);

	return clk_osm_acd_local_write_reg(c, val, offset);
}

static int clk_osm_acd_auto_local_write_reg(struct clk_osm *c, u32 mask)
{
	u32 numregs, bitmask = mask;
	u32 reg = 0;
	int timeout;

	/* count number of bits set in register mask */
	for (numregs = 0; bitmask; numregs++)
		bitmask &= bitmask - 1;

	/* Program auto-transfer mask */
	writel_relaxed(mask, (char *)c->vbases[ACD_BASE] + ACD_AUTOXFER_CFG);

	/* Clear start field in auto-transfer register */
	writel_relaxed(ACD_AUTOXFER_START_CLEAR,
			(char *)c->vbases[ACD_BASE] + ACD_AUTOXFER);

	/* Set start field in auto-transfer register */
	writel_relaxed(ACD_AUTOXFER_START_SET,
			(char *)c->vbases[ACD_BASE] + ACD_AUTOXFER);

	/* Ensure writes complete before polling */
	clk_osm_acd_mb(c);

	/* Poll auto-transfer status register */
	for (timeout = ACD_LOCAL_TRANSFER_TIMEOUT_NS * numregs;
	     timeout > 0; timeout -= 100) {
		reg = readl_relaxed((char *)c->vbases[ACD_BASE]
					+ ACD_AUTOXFER_STATUS);
		if (reg & AUTO_XFER_DONE_MASK)
			break;
		ndelay(100);
	}

	if (!timeout) {
		pr_err("local register auto-transfer timed out, mask=0x%x registers=%d status=0x%x\n",
			mask, numregs, reg);
		return -ETIMEDOUT;
	}

	return 0;
}

static inline struct clk_osm *to_clk_osm(struct clk_hw *_hw)
{
	return container_of(_hw, struct clk_osm, hw);
}

static inline void clk_osm_masked_write_reg(struct clk_osm *c, u32 val,
					    u32 offset, u32 mask)
{
	u32 val2, orig_val;

	val2 = orig_val = readl_relaxed((char *)c->vbases[OSM_BASE] + offset);
	val2 &= ~mask;
	val2 |= val & mask;

	if (val2 != orig_val)
		writel_relaxed(val2, (char *)c->vbases[OSM_BASE] + offset);
}

static inline void clk_osm_write_seq_reg(struct clk_osm *c, u32 val, u32 offset)
{
	writel_relaxed(val, (char *)c->vbases[SEQ_BASE] + offset);
}

static inline void clk_osm_write_reg(struct clk_osm *c, u32 val, u32 offset,
					int base)
{
	writel_relaxed(val, (char *)c->vbases[base] + offset);
}

static inline int clk_osm_read_reg(struct clk_osm *c, u32 offset)
{
	return readl_relaxed((char *)c->vbases[OSM_BASE] + offset);
}

static inline int clk_osm_read_reg_no_log(struct clk_osm *c, u32 offset)
{
	return readl_relaxed_no_log((char *)c->vbases[OSM_BASE] + offset);
}

static inline int clk_osm_mb(struct clk_osm *c, int base)
{
	return readl_relaxed_no_log((char *)c->vbases[base] + ENABLE_REG);
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

static long clk_osm_round_rate(struct clk_hw *hw, unsigned long rate,
				unsigned long *parent_rate)
{
	int i;
	unsigned long rrate = 0;

	/*
	 * If the rate passed in is 0, return the first frequency in the
	 * FMAX table.
	 */
	if (!rate)
		return hw->init->rate_max[0];

	for (i = 0; i < hw->init->num_rate_max; i++) {
		if (is_better_rate(rate, rrate, hw->init->rate_max[i])) {
			rrate = hw->init->rate_max[i];
			if (rate == rrate)
				break;
		}
	}

	pr_debug("%s: rate %lu, rrate %ld, Rate max %ld\n", __func__, rate,
						rrate, hw->init->rate_max[i]);

	return rrate;
}

static int clk_osm_search_table(struct osm_entry *table, int entries, long rate)
{
	int quad_core_index, single_core_index = 0;
	int core_count;

	for (quad_core_index = 0; quad_core_index < entries;
						quad_core_index++) {
		core_count = CORE_COUNT_VAL(table[quad_core_index].freq_data);
		if (rate == table[quad_core_index].frequency &&
					core_count == SINGLE_CORE) {
			single_core_index = quad_core_index;
			continue;
		}
		if (rate == table[quad_core_index].frequency &&
					core_count == MAX_CORE_COUNT)
			return quad_core_index;
	}
	if (single_core_index)
		return single_core_index;

	return -EINVAL;
}

static int clk_osm_enable(struct clk_hw *hw)
{
	struct clk_osm *cpuclk = to_clk_osm(hw);

	clk_osm_write_reg(cpuclk, 1, ENABLE_REG, OSM_BASE);

	/* Make sure the write goes through before proceeding */
	clk_osm_mb(cpuclk, OSM_BASE);

	/* Wait for 5us for OSM hardware to enable */
	udelay(5);

	pr_debug("OSM clk enabled for cluster=%d\n", cpuclk->cluster_num);

	return 0;
}

const struct clk_ops clk_ops_cpu_osm = {
	.enable = clk_osm_enable,
	.round_rate = clk_osm_round_rate,
	.list_rate = clk_osm_list_rate,
	.debug_init = clk_debug_measure_add,
};

static struct clk_ops clk_ops_core;

static int cpu_clk_set_rate(struct clk_hw *hw, unsigned long rate,
				    unsigned long parent_rate)
{
	struct clk_osm *cpuclk = to_clk_osm(hw);
	struct clk_hw *p_hw = clk_hw_get_parent(hw);
	struct clk_osm *parent = to_clk_osm(p_hw);
	int index = 0;
	unsigned long r_rate;

	if (!cpuclk || !parent)
		return -EINVAL;

	r_rate = clk_osm_round_rate(p_hw, rate, NULL);

	if (rate != r_rate) {
		pr_err("invalid requested rate=%ld\n", rate);
		return -EINVAL;
	}

	/* Convert rate to table index */
	index = clk_osm_search_table(parent->osm_table,
				     parent->num_entries, r_rate);
	if (index < 0) {
		pr_err("cannot set %s to %lu\n", clk_hw_get_name(hw), rate);
		return -EINVAL;
	}
	pr_debug("rate: %lu --> index %d\n", rate, index);
	/*
	 * Choose index and send request to OSM hardware.
	 * TODO: Program INACTIVE_OS_REQUEST if needed.
	 */
	clk_osm_write_reg(parent, index,
			DCVS_PERF_STATE_DESIRED_REG(cpuclk->core_num),
			OSM_BASE);

	/* Make sure the write goes through before proceeding */
	clk_osm_mb(parent, OSM_BASE);

	return 0;
}

static int l3_clk_set_rate(struct clk_hw *hw, unsigned long rate,
				    unsigned long parent_rate)
{
	struct clk_osm *cpuclk = to_clk_osm(hw);
	int index = 0;
	unsigned long r_rate;

	if (!cpuclk)
		return -EINVAL;

	r_rate = clk_osm_round_rate(hw, rate, NULL);

	if (rate != r_rate) {
		pr_err("invalid requested rate=%ld\n", rate);
		return -EINVAL;
	}

	/* Convert rate to table index */
	index = clk_osm_search_table(cpuclk->osm_table,
				     cpuclk->num_entries, r_rate);
	if (index < 0) {
		pr_err("cannot set %s to %lu\n", clk_hw_get_name(hw), rate);
		return -EINVAL;
	}
	pr_debug("rate: %lu --> index %d\n", rate, index);

	clk_osm_write_reg(cpuclk, index, DCVS_PERF_STATE_DESIRED_REG_0,
				OSM_BASE);

	/* Make sure the write goes through before proceeding */
	clk_osm_mb(cpuclk, OSM_BASE);

	return 0;
}

static long cpu_clk_round_rate(struct clk_hw *hw, unsigned long rate,
					unsigned long *parent_rate)
{
	struct clk_hw *parent_hw = clk_hw_get_parent(hw);

	if (!parent_hw)
		return -EINVAL;

	return clk_hw_round_rate(parent_hw, rate);
}

static unsigned long cpu_clk_recalc_rate(struct clk_hw *hw,
					unsigned long parent_rate)
{
	struct clk_osm *cpuclk = to_clk_osm(hw);
	struct clk_hw *p_hw = clk_hw_get_parent(hw);
	struct clk_osm *parent = to_clk_osm(p_hw);
	int index = 0;

	if (!cpuclk || !parent)
		return -EINVAL;

	index = clk_osm_read_reg(parent,
			DCVS_PERF_STATE_DESIRED_REG(cpuclk->core_num));

	pr_debug("%s: Index %d, freq %ld\n", __func__, index,
				parent->osm_table[index].frequency);

	/* Convert index to frequency */
	return parent->osm_table[index].frequency;
}

static unsigned long l3_clk_recalc_rate(struct clk_hw *hw,
					unsigned long parent_rate)
{
	struct clk_osm *cpuclk = to_clk_osm(hw);
	int index = 0;

	if (!cpuclk)
		return -EINVAL;

	index = clk_osm_read_reg(cpuclk, DCVS_PERF_STATE_DESIRED_REG_0);

	pr_debug("%s: Index %d, freq %ld\n", __func__, index,
				cpuclk->osm_table[index].frequency);

	/* Convert index to frequency */
	return cpuclk->osm_table[index].frequency;
}


const struct clk_ops clk_ops_l3_osm = {
	.enable = clk_osm_enable,
	.round_rate = clk_osm_round_rate,
	.list_rate = clk_osm_list_rate,
	.recalc_rate = l3_clk_recalc_rate,
	.set_rate = l3_clk_set_rate,
	.debug_init = clk_debug_measure_add,
};

static struct clk_init_data osm_clks_init[] = {
	[0] = {
		.name = "l3_clk",
		.parent_names = (const char *[]){ "bi_tcxo_ao" },
		.num_parents = 1,
		.ops = &clk_ops_l3_osm,
	},
	[1] = {
		.name = "pwrcl_clk",
		.parent_names = (const char *[]){ "bi_tcxo_ao" },
		.num_parents = 1,
		.ops = &clk_ops_cpu_osm,
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
	.hw.init = &osm_clks_init[0],
};

static DEFINE_CLK_VOTER(l3_cluster0_vote_clk, l3_clk, 0);
static DEFINE_CLK_VOTER(l3_cluster1_vote_clk, l3_clk, 0);

static struct clk_osm pwrcl_clk = {
	.cluster_num = 1,
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
		.ops = &clk_ops_core,
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
		.ops = &clk_ops_core,
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
		.ops = &clk_ops_core,
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
		.ops = &clk_ops_core,
	},
};

static struct clk_osm perfcl_clk = {
	.cluster_num = 2,
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
		.ops = &clk_ops_core,
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
		.ops = &clk_ops_core,
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
		.ops = &clk_ops_core,
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
		.ops = &clk_ops_core,
	},
};

/*
 * Use the cpu* clocks only for writing to the PERF_STATE_DESIRED registers.
 * Note that we are currently NOT programming the APSS_LMH_GFMUX_CFG &
 * APSS_OSM_GFMUX_CFG registers.
 */

static struct clk_hw *osm_qcom_clk_hws[] = {
	[L3_CLK] = &l3_clk.hw,
	[L3_CLUSTER0_VOTE_CLK] = &l3_cluster0_vote_clk.hw,
	[L3_CLUSTER1_VOTE_CLK] = &l3_cluster1_vote_clk.hw,
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
};

static struct clk_osm *logical_cpu_to_clk(int cpu)
{
	struct device_node *cpu_node;
	const u32 *cell;
	u64 hwid;
	static struct clk_osm *cpu_clk_map[NR_CPUS];
	struct clk_osm *clk_cpu_map[] = {
		&cpu0_pwrcl_clk,
		&cpu1_pwrcl_clk,
		&cpu2_pwrcl_clk,
		&cpu3_pwrcl_clk,
		&cpu4_perfcl_clk,
		&cpu5_perfcl_clk,
		&cpu6_perfcl_clk,
		&cpu7_perfcl_clk,
	};

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
osm_set_index(struct clk_osm *c, unsigned int index, unsigned int num)
{
	clk_osm_write_reg(c, index, DCVS_PERF_STATE_DESIRED_REG(num), OSM_BASE);

	/* Make sure the write goes through before proceeding */
	clk_osm_mb(c, OSM_BASE);
}

static int
osm_cpufreq_target_index(struct cpufreq_policy *policy, unsigned int index)
{
	struct clk_osm *c = policy->driver_data;

	osm_set_index(c, index, c->core_num);
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
	index = clk_osm_read_reg(c, DCVS_PERF_STATE_DESIRED_REG(c->core_num));

	return policy->freq_table[index].frequency;
}

static int osm_cpufreq_cpu_init(struct cpufreq_policy *policy)
{
	struct cpufreq_frequency_table *table;
	struct clk_osm *c, *parent;
	struct clk_hw *p_hw;
	int ret;
	unsigned int i;
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
	c->vbases[OSM_BASE] = parent->vbases[OSM_BASE];

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

		if (core_count != MAX_CORE_COUNT)
			table[i].frequency = CPUFREQ_ENTRY_INVALID;

		/* Two of the same frequencies means end of table */
		if (i > 0 && table[i - 1].driver_data == table[i].driver_data) {
			struct cpufreq_frequency_table *prev = &table[i - 1];

			if (prev->frequency == CPUFREQ_ENTRY_INVALID) {
				prev->flags = CPUFREQ_BOOST_FREQ;
				prev->frequency = prev->driver_data;
			}

			break;
		}
	}
	table[i].frequency = CPUFREQ_TABLE_END;

	ret = cpufreq_table_validate_and_show(policy, table);
	if (ret) {
		pr_err("%s: invalid frequency table: %d\n", __func__, ret);
		goto err;
	}

	policy->driver_data = c;

	clk_osm_enable(&parent->hw);
	udelay(300);

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

static inline int clk_osm_count_ns(struct clk_osm *c, u64 nsec)
{
	u64 temp;

	temp = (u64)c->osm_clk_rate * nsec;
	do_div(temp, 1000000000);

	return temp;
}

static void clk_osm_program_mem_acc_regs(struct clk_osm *c)
{
	int curr_level, i, j = 0;
	int mem_acc_level_map[MAX_MEM_ACC_LEVELS] = {MAX_VC, MAX_VC, MAX_VC};

	curr_level = c->osm_table[0].mem_acc_level;
	for (i = 0; i < c->num_entries; i++) {
		if (curr_level == MAX_MEM_ACC_LEVELS)
			break;

		if (c->osm_table[i].mem_acc_level != curr_level) {
			mem_acc_level_map[j++] =
				c->osm_table[i].virtual_corner;
			curr_level = c->osm_table[i].mem_acc_level;
		}
	}

	if (c->secure_init) {
		clk_osm_write_seq_reg(c,
				c->pbases[OSM_BASE] + MEMACC_CROSSOVER_VC,
				DATA_MEM(57));
		clk_osm_write_seq_reg(c, c->mem_acc_addr[0], DATA_MEM(48));
		clk_osm_write_seq_reg(c, c->mem_acc_addr[1], DATA_MEM(49));
		clk_osm_write_seq_reg(c, c->mem_acc_addr[2], DATA_MEM(50));
		clk_osm_write_seq_reg(c, c->mem_acc_crossover_vc,
							DATA_MEM(78));
		clk_osm_write_seq_reg(c, mem_acc_level_map[0], DATA_MEM(79));
		if (c == &perfcl_clk)
			clk_osm_write_seq_reg(c, c->mem_acc_threshold_vc,
								DATA_MEM(80));
		else
			clk_osm_write_seq_reg(c, mem_acc_level_map[1],
								DATA_MEM(80));
		/*
		 * Note that DATA_MEM[81] -> DATA_MEM[89] values will be
		 * confirmed post-si. Use a value of 1 for DATA_MEM[89] and
		 * leave the rest of them as 0.
		 */
		clk_osm_write_seq_reg(c, 1, DATA_MEM(89));
	} else {
		scm_io_write(c->pbases[SEQ_BASE] + DATA_MEM(78),
						c->mem_acc_crossover_vc);
		scm_io_write(c->pbases[SEQ_BASE] + DATA_MEM(79),
						mem_acc_level_map[0]);
		if (c == &perfcl_clk)
			scm_io_write(c->pbases[SEQ_BASE] + DATA_MEM(80),
						c->mem_acc_threshold_vc);
		else
			scm_io_write(c->pbases[SEQ_BASE] + DATA_MEM(80),
						mem_acc_level_map[1]);
	}
}

static void clk_osm_program_apm_regs(struct clk_osm *c)
{
	if (c == &l3_clk || c == &pwrcl_clk)
		return;

	/*
	 * Program address of the control register used to configure
	 * the Array Power Mux controller
	 */
	clk_osm_write_seq_reg(c, c->apm_mode_ctl, DATA_MEM(41));

	/* Program address of controller status register */
	clk_osm_write_seq_reg(c, c->apm_status_ctl, DATA_MEM(43));

	/* Program address of crossover register */
	clk_osm_write_seq_reg(c, c->pbases[OSM_BASE] + APM_CROSSOVER_VC,
						DATA_MEM(44));

	/* Program mode value to switch APM to VDD_APC */
	clk_osm_write_seq_reg(c, APM_APC_MODE, DATA_MEM(72));

	/* Program mode value to switch APM to VDD_MX */
	clk_osm_write_seq_reg(c, APM_MX_MODE, DATA_MEM(73));

	/* Program mask used to move into read_mask port */
	clk_osm_write_seq_reg(c, APM_READ_DATA_MASK, DATA_MEM(74));

	/* Value used to move into read_exp port */
	clk_osm_write_seq_reg(c, APM_APC_READ_VAL, DATA_MEM(75));
	clk_osm_write_seq_reg(c, APM_MX_READ_VAL, DATA_MEM(76));
}

static void clk_osm_do_additional_setup(struct clk_osm *c,
					struct platform_device *pdev)
{
	if (!c->secure_init)
		return;

	dev_info(&pdev->dev, "Performing additional OSM setup due to lack of TZ for cluster=%d\n",
						 c->cluster_num);

	/* PLL L_VAL & post-div programming */
	clk_osm_write_seq_reg(c, c->apcs_pll_min_freq, DATA_MEM(32));
	clk_osm_write_seq_reg(c, c->l_val_base, DATA_MEM(33));
	clk_osm_write_seq_reg(c, c->apcs_pll_user_ctl, DATA_MEM(34));
	clk_osm_write_seq_reg(c, PLL_POST_DIV1, DATA_MEM(35));
	clk_osm_write_seq_reg(c, PLL_POST_DIV2, DATA_MEM(36));

	/* APM Programming */
	clk_osm_program_apm_regs(c);

	/* GFMUX Programming */
	clk_osm_write_seq_reg(c, c->cfg_gfmux_addr, DATA_MEM(37));
	clk_osm_write_seq_reg(c, 0x1, DATA_MEM(65));
	clk_osm_write_seq_reg(c, 0x2, DATA_MEM(66));
	clk_osm_write_seq_reg(c, 0x3, DATA_MEM(67));
	clk_osm_write_seq_reg(c, 0x40000000, DATA_MEM(68));
	clk_osm_write_seq_reg(c, 0x20000000, DATA_MEM(69));
	clk_osm_write_seq_reg(c, 0x10000000, DATA_MEM(70));
	clk_osm_write_seq_reg(c, 0x70000000, DATA_MEM(71));

	/* Override programming */
	clk_osm_write_seq_reg(c, c->pbases[OSM_BASE] +
			OVERRIDE_CLUSTER_IDLE_ACK, DATA_MEM(54));
	clk_osm_write_seq_reg(c, 0x3, DATA_MEM(55));
	clk_osm_write_seq_reg(c, c->pbases[OSM_BASE] + PDN_FSM_CTRL_REG,
					DATA_MEM(40));
	clk_osm_write_seq_reg(c, c->pbases[OSM_BASE] + REQ_GEN_FSM_STATUS,
					DATA_MEM(60));
	clk_osm_write_seq_reg(c, 0x10, DATA_MEM(61));
	clk_osm_write_seq_reg(c, 0x70, DATA_MEM(62));
	clk_osm_write_seq_reg(c, c->apcs_cbc_addr, DATA_MEM(112));
	clk_osm_write_seq_reg(c, 0x2, DATA_MEM(113));

	if (c == &perfcl_clk) {
		int rc;
		u32 isense_addr;

		/* Performance cluster isense programming */
		rc = of_property_read_u32(pdev->dev.of_node,
				"qcom,perfcl-isense-addr", &isense_addr);
		if (rc) {
			dev_err(&pdev->dev, "unable to find qcom,perfcl-isense-addr property, rc=%d\n",
				rc);
			return;
		}
		clk_osm_write_seq_reg(c, isense_addr, DATA_MEM(45));
		clk_osm_write_seq_reg(c, ISENSE_ON_DATA, DATA_MEM(46));
		clk_osm_write_seq_reg(c, ISENSE_OFF_DATA, DATA_MEM(47));
	}

	clk_osm_write_seq_reg(c, c->ramp_ctl_addr, DATA_MEM(105));
	clk_osm_write_seq_reg(c, CONSTANT_32, DATA_MEM(92));

	/* Enable/disable CPR ramp settings */
	clk_osm_write_seq_reg(c, 0x101C031, DATA_MEM(106));
	clk_osm_write_seq_reg(c, 0x1010031, DATA_MEM(107));
}

static void clk_osm_setup_fsms(struct clk_osm *c)
{
	u32 val;

	/* Voltage Reduction FSM */
	if (c->red_fsm_en) {
		val = clk_osm_read_reg(c, VMIN_REDUCTION_ENABLE_REG) | BIT(0);
		val |= BVAL(6, 1, c->min_cpr_vc);
		clk_osm_write_reg(c, val, VMIN_REDUCTION_ENABLE_REG,
					OSM_BASE);

		clk_osm_write_reg(c, clk_osm_count_ns(c, 10000),
				  VMIN_REDUCTION_TIMER_REG, OSM_BASE);
	}

	/* Boost FSM */
	if (c->boost_fsm_en) {
		val = clk_osm_read_reg(c, PDN_FSM_CTRL_REG);
		val |= DELTA_DEX_VAL | CC_BOOST_FSM_EN | IGNORE_PLL_LOCK;
		clk_osm_write_reg(c, val, PDN_FSM_CTRL_REG, OSM_BASE);

		val = clk_osm_read_reg(c, CC_BOOST_FSM_TIMERS_REG0);
		val |= BVAL(15, 0, clk_osm_count_ns(c, PLL_WAIT_LOCK_TIME_NS));
		val |= BVAL(31, 16, clk_osm_count_ns(c, SAFE_FREQ_WAIT_NS));
		clk_osm_write_reg(c, val, CC_BOOST_FSM_TIMERS_REG0, OSM_BASE);

		val = clk_osm_read_reg(c, CC_BOOST_FSM_TIMERS_REG1);
		val |= BVAL(15, 0, clk_osm_count_ns(c, PLL_WAIT_LOCK_TIME_NS));
		val |= BVAL(31, 16, clk_osm_count_ns(c, PLL_WAIT_LOCK_TIME_NS));
		clk_osm_write_reg(c, val, CC_BOOST_FSM_TIMERS_REG1, OSM_BASE);

		val = clk_osm_read_reg(c, CC_BOOST_FSM_TIMERS_REG2);
		val |= BVAL(15, 0, clk_osm_count_ns(c, DEXT_DECREMENT_WAIT_NS));
		clk_osm_write_reg(c, val, CC_BOOST_FSM_TIMERS_REG2, OSM_BASE);
	}

	/* Safe Freq FSM */
	if (c->safe_fsm_en) {
		val = clk_osm_read_reg(c, PDN_FSM_CTRL_REG);
		clk_osm_write_reg(c, val | DCVS_BOOST_FSM_EN_MASK,
				  PDN_FSM_CTRL_REG, OSM_BASE);

		val = clk_osm_read_reg(c, DCVS_BOOST_FSM_TIMERS_REG0);
		val |= BVAL(31, 16, clk_osm_count_ns(c, 1000));
		clk_osm_write_reg(c, val, DCVS_BOOST_FSM_TIMERS_REG0, OSM_BASE);

		val = clk_osm_read_reg(c, DCVS_BOOST_FSM_TIMERS_REG1);
		val |= BVAL(15, 0, clk_osm_count_ns(c, SAFE_FREQ_WAIT_NS));
		clk_osm_write_reg(c, val, DCVS_BOOST_FSM_TIMERS_REG1, OSM_BASE);

		val = clk_osm_read_reg(c, DCVS_BOOST_FSM_TIMERS_REG2);
		val |= BVAL(15, 0, clk_osm_count_ns(c, DEXT_DECREMENT_WAIT_NS));
		clk_osm_write_reg(c, val, DCVS_BOOST_FSM_TIMERS_REG2, OSM_BASE);

	}

	/* Pulse Swallowing FSM */
	if (c->ps_fsm_en) {
		val = clk_osm_read_reg(c, PDN_FSM_CTRL_REG);
		clk_osm_write_reg(c, val | PS_BOOST_FSM_EN_MASK,
					PDN_FSM_CTRL_REG, OSM_BASE);

		val = clk_osm_read_reg(c, PS_BOOST_FSM_TIMERS_REG0);
		val |= BVAL(15, 0, clk_osm_count_ns(c, SAFE_FREQ_WAIT_NS));
		val |= BVAL(31, 16, clk_osm_count_ns(c, 1000));
		clk_osm_write_reg(c, val, PS_BOOST_FSM_TIMERS_REG0, OSM_BASE);

		val = clk_osm_read_reg(c, PS_BOOST_FSM_TIMERS_REG1);
		val |= BVAL(15, 0, clk_osm_count_ns(c, SAFE_FREQ_WAIT_NS));
		val |= BVAL(31, 16, clk_osm_count_ns(c, 1000));
		clk_osm_write_reg(c, val, PS_BOOST_FSM_TIMERS_REG1, OSM_BASE);

		val = clk_osm_read_reg(c, PS_BOOST_FSM_TIMERS_REG2);
		val |= BVAL(15, 0, clk_osm_count_ns(c, DEXT_DECREMENT_WAIT_NS));
		clk_osm_write_reg(c, val, PS_BOOST_FSM_TIMERS_REG2, OSM_BASE);
	}

	/* PLL signal timing control */
	if (c->boost_fsm_en || c->safe_fsm_en || c->ps_fsm_en)
		clk_osm_write_reg(c, 0x2, BOOST_PROG_SYNC_DELAY_REG, OSM_BASE);

	/* DCVS droop FSM - only if RCGwRC is not used for di/dt control */
	if (c->droop_fsm_en) {
		val = clk_osm_read_reg(c, PDN_FSM_CTRL_REG);
		clk_osm_write_reg(c, val | DCVS_DROOP_FSM_EN_MASK,
					PDN_FSM_CTRL_REG, OSM_BASE);
	}

	if (c->ps_fsm_en || c->droop_fsm_en) {
		clk_osm_write_reg(c, 0x1, DROOP_PROG_SYNC_DELAY_REG, OSM_BASE);
		clk_osm_write_reg(c, clk_osm_count_ns(c, 100),
					DROOP_RELEASE_TIMER_CTRL, OSM_BASE);
		clk_osm_write_reg(c, clk_osm_count_ns(c, 150),
					DCVS_DROOP_TIMER_CTRL, OSM_BASE);
		/*
		 * TODO: Check if DCVS_DROOP_CODE used is correct. Also check
		 * if RESYNC_CTRL should be set for L3.
		 */
		val = BIT(31) | BVAL(22, 16, 0x2) | BVAL(6, 0, 0x8);
		clk_osm_write_reg(c, val, DROOP_CTRL_REG, OSM_BASE);
	}
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
		val = clk_osm_count_ns(&l3_clk, array[l3_clk.cluster_num]);
		clk_osm_write_reg(&l3_clk, val,
					LLM_VOLTAGE_VOTE_INC_HYSTERESIS,
					OSM_BASE);

		val = clk_osm_count_ns(&pwrcl_clk,
						array[pwrcl_clk.cluster_num]);
		clk_osm_write_reg(&pwrcl_clk, val,
					LLM_VOLTAGE_VOTE_INC_HYSTERESIS,
					OSM_BASE);

		val = clk_osm_count_ns(&perfcl_clk,
						array[perfcl_clk.cluster_num]);
		clk_osm_write_reg(&perfcl_clk, val,
					LLM_VOLTAGE_VOTE_INC_HYSTERESIS,
					OSM_BASE);
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
		val = clk_osm_count_ns(&l3_clk, array[l3_clk.cluster_num]);
		clk_osm_write_reg(&l3_clk, val,
					LLM_VOLTAGE_VOTE_DEC_HYSTERESIS,
					OSM_BASE);

		val = clk_osm_count_ns(&pwrcl_clk,
					       array[pwrcl_clk.cluster_num]);
		clk_osm_write_reg(&pwrcl_clk, val,
					LLM_VOLTAGE_VOTE_DEC_HYSTERESIS,
					OSM_BASE);

		val = clk_osm_count_ns(&perfcl_clk,
					array[perfcl_clk.cluster_num]);
		clk_osm_write_reg(&perfcl_clk, val,
					LLM_VOLTAGE_VOTE_DEC_HYSTERESIS,
					OSM_BASE);
	}

	/* Enable or disable honoring of LLM Voltage requests */
	rc = of_property_read_bool(pdev->dev.of_node,
					"qcom,enable-llm-volt-vote");
	if (rc) {
		dev_dbg(&pdev->dev, "Honoring LLM Voltage requests\n");
		val = 0;
	} else
		val = 1;

	/* Enable or disable LLM VOLT DVCS */
	regval = val | clk_osm_read_reg(&l3_clk, LLM_INTF_DCVS_DISABLE);
	clk_osm_write_reg(&l3_clk, regval, LLM_INTF_DCVS_DISABLE, OSM_BASE);
	regval = val | clk_osm_read_reg(&pwrcl_clk, LLM_INTF_DCVS_DISABLE);
	clk_osm_write_reg(&pwrcl_clk, regval, LLM_INTF_DCVS_DISABLE, OSM_BASE);
	regval = val | clk_osm_read_reg(&perfcl_clk, LLM_INTF_DCVS_DISABLE);
	clk_osm_write_reg(&perfcl_clk, regval, LLM_INTF_DCVS_DISABLE, OSM_BASE);

	/* Wait for the writes to complete */
	clk_osm_mb(&perfcl_clk, OSM_BASE);

	devm_kfree(&pdev->dev, array);
	return 0;
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
		val = clk_osm_count_ns(&l3_clk, array[l3_clk.cluster_num]);
		clk_osm_write_reg(&l3_clk, val, LLM_FREQ_VOTE_INC_HYSTERESIS,
			OSM_BASE);

		val = clk_osm_count_ns(&pwrcl_clk,
						array[pwrcl_clk.cluster_num]);
		clk_osm_write_reg(&pwrcl_clk, val,
					LLM_FREQ_VOTE_INC_HYSTERESIS,
					OSM_BASE);

		val = clk_osm_count_ns(&perfcl_clk,
						array[perfcl_clk.cluster_num]);
		clk_osm_write_reg(&perfcl_clk, val,
					LLM_FREQ_VOTE_INC_HYSTERESIS,
					OSM_BASE);
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
		val = clk_osm_count_ns(&l3_clk, array[l3_clk.cluster_num]);
		clk_osm_write_reg(&l3_clk, val, LLM_FREQ_VOTE_DEC_HYSTERESIS,
					OSM_BASE);

		val = clk_osm_count_ns(&pwrcl_clk,
					       array[pwrcl_clk.cluster_num]);
		clk_osm_write_reg(&pwrcl_clk, val,
					LLM_FREQ_VOTE_DEC_HYSTERESIS, OSM_BASE);

		val = clk_osm_count_ns(&perfcl_clk,
					       array[perfcl_clk.cluster_num]);
		clk_osm_write_reg(&perfcl_clk, val,
					LLM_FREQ_VOTE_DEC_HYSTERESIS, OSM_BASE);
	}

	/* Enable or disable honoring of LLM frequency requests */
	rc = of_property_read_bool(pdev->dev.of_node,
					"qcom,enable-llm-freq-vote");
	if (rc) {
		dev_dbg(&pdev->dev, "Honoring LLM Frequency requests\n");
		val = 0;
	} else
		val = BIT(1);

	/* Enable or disable LLM FREQ DVCS */
	regval = val | clk_osm_read_reg(&l3_clk, LLM_INTF_DCVS_DISABLE);
	clk_osm_write_reg(&l3_clk, regval, LLM_INTF_DCVS_DISABLE, OSM_BASE);
	regval = val | clk_osm_read_reg(&pwrcl_clk, LLM_INTF_DCVS_DISABLE);
	clk_osm_write_reg(&pwrcl_clk, regval, LLM_INTF_DCVS_DISABLE, OSM_BASE);
	regval = val | clk_osm_read_reg(&perfcl_clk, LLM_INTF_DCVS_DISABLE);
	clk_osm_write_reg(&perfcl_clk, regval, LLM_INTF_DCVS_DISABLE, OSM_BASE);

	/* Wait for the write to complete */
	clk_osm_mb(&perfcl_clk, OSM_BASE);

	devm_kfree(&pdev->dev, array);
	return 0;
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
		val = clk_osm_count_ns(&l3_clk,
					array[l3_clk.cluster_num]);
		clk_osm_write_reg(&l3_clk, val, SPM_CC_INC_HYSTERESIS,
			OSM_BASE);

		val = clk_osm_count_ns(&pwrcl_clk,
					array[pwrcl_clk.cluster_num]);
		clk_osm_write_reg(&pwrcl_clk, val, SPM_CC_INC_HYSTERESIS,
					OSM_BASE);

		val = clk_osm_count_ns(&perfcl_clk,
					array[perfcl_clk.cluster_num]);
		clk_osm_write_reg(&perfcl_clk, val, SPM_CC_INC_HYSTERESIS,
					OSM_BASE);
	}

	rc = of_property_read_u32_array(of, "qcom,down-timer",
					array, MAX_CLUSTER_CNT);
	if (rc) {
		dev_dbg(&pdev->dev, "No down timer value, rc=%d\n", rc);
	} else {
		val = clk_osm_count_ns(&l3_clk,
				       array[l3_clk.cluster_num]);
		clk_osm_write_reg(&l3_clk, val, SPM_CC_DEC_HYSTERESIS,
					OSM_BASE);

		val = clk_osm_count_ns(&pwrcl_clk,
				       array[pwrcl_clk.cluster_num]);
		clk_osm_write_reg(&pwrcl_clk, val, SPM_CC_DEC_HYSTERESIS,
					OSM_BASE);

		clk_osm_count_ns(&perfcl_clk,
				       array[perfcl_clk.cluster_num]);
		clk_osm_write_reg(&perfcl_clk, val, SPM_CC_DEC_HYSTERESIS,
					OSM_BASE);
	}

	/* OSM index override for cluster PC */
	rc = of_property_read_u32_array(of, "qcom,pc-override-index",
					array, MAX_CLUSTER_CNT);
	if (rc) {
		dev_dbg(&pdev->dev, "No PC override index value, rc=%d\n",
			rc);
		clk_osm_write_reg(&pwrcl_clk, 0, CC_ZERO_BEHAV_CTRL, OSM_BASE);
		clk_osm_write_reg(&perfcl_clk, 0, CC_ZERO_BEHAV_CTRL,
					OSM_BASE);
	} else {
		val = BVAL(6, 1, array[pwrcl_clk.cluster_num])
			| ENABLE_OVERRIDE;
		clk_osm_write_reg(&pwrcl_clk, val, CC_ZERO_BEHAV_CTRL,
					OSM_BASE);
		val = BVAL(6, 1, array[perfcl_clk.cluster_num])
			| ENABLE_OVERRIDE;
		clk_osm_write_reg(&perfcl_clk, val, CC_ZERO_BEHAV_CTRL,
					OSM_BASE);
	}

	/* Wait for the writes to complete */
	clk_osm_mb(&perfcl_clk, OSM_BASE);

	rc = of_property_read_bool(pdev->dev.of_node, "qcom,set-c3-active");
	if (rc) {
		dev_dbg(&pdev->dev, "Treat cores in C3 as active\n");

		val = clk_osm_read_reg(&l3_clk, SPM_CORE_INACTIVE_MAPPING);
		val &= ~BIT(2);
		clk_osm_write_reg(&l3_clk, val, SPM_CORE_INACTIVE_MAPPING,
					OSM_BASE);

		val = clk_osm_read_reg(&pwrcl_clk, SPM_CORE_INACTIVE_MAPPING);
		val &= ~BIT(2);
		clk_osm_write_reg(&pwrcl_clk, val, SPM_CORE_INACTIVE_MAPPING,
					OSM_BASE);

		val = clk_osm_read_reg(&perfcl_clk, SPM_CORE_INACTIVE_MAPPING);
		val &= ~BIT(2);
		clk_osm_write_reg(&perfcl_clk, val, SPM_CORE_INACTIVE_MAPPING,
					OSM_BASE);
	}

	rc = of_property_read_bool(pdev->dev.of_node, "qcom,set-c2-active");
	if (rc) {
		dev_dbg(&pdev->dev, "Treat cores in C2 as active\n");

		val = clk_osm_read_reg(&l3_clk, SPM_CORE_INACTIVE_MAPPING);
		val &= ~BIT(1);
		clk_osm_write_reg(&l3_clk, val, SPM_CORE_INACTIVE_MAPPING,
					OSM_BASE);

		val = clk_osm_read_reg(&pwrcl_clk, SPM_CORE_INACTIVE_MAPPING);
		val &= ~BIT(1);
		clk_osm_write_reg(&pwrcl_clk, val, SPM_CORE_INACTIVE_MAPPING,
					OSM_BASE);

		val = clk_osm_read_reg(&perfcl_clk, SPM_CORE_INACTIVE_MAPPING);
		val &= ~BIT(1);
		clk_osm_write_reg(&perfcl_clk, val, SPM_CORE_INACTIVE_MAPPING,
					OSM_BASE);
	}

	rc = of_property_read_bool(pdev->dev.of_node, "qcom,disable-cc-dvcs");
	if (rc) {
		dev_dbg(&pdev->dev, "Disabling CC based DCVS\n");
		val = 1;
	} else
		val = 0;

	clk_osm_write_reg(&l3_clk, val, SPM_CC_DCVS_DISABLE, OSM_BASE);
	clk_osm_write_reg(&pwrcl_clk, val, SPM_CC_DCVS_DISABLE, OSM_BASE);
	clk_osm_write_reg(&perfcl_clk, val, SPM_CC_DCVS_DISABLE, OSM_BASE);

	/* Wait for the writes to complete */
	clk_osm_mb(&perfcl_clk, OSM_BASE);

	devm_kfree(&pdev->dev, array);
	return 0;
}

static void clk_osm_setup_cluster_pll(struct clk_osm *c)
{
	writel_relaxed(0x0, c->vbases[PLL_BASE] + PLL_MODE);
	writel_relaxed(0x26, c->vbases[PLL_BASE] + PLL_L_VAL);
	writel_relaxed(0x8, c->vbases[PLL_BASE] +
			PLL_USER_CTRL);
	writel_relaxed(0x20000AA8, c->vbases[PLL_BASE] +
			PLL_CONFIG_CTL_LO);
	writel_relaxed(0x000003D2, c->vbases[PLL_BASE] +
			PLL_CONFIG_CTL_HI);
	writel_relaxed(0x2, c->vbases[PLL_BASE] +
			PLL_MODE);

	/* Ensure writes complete before delaying */
	clk_osm_mb(c, PLL_BASE);

	udelay(PLL_WAIT_LOCK_TIME_US);

	writel_relaxed(0x6, c->vbases[PLL_BASE] + PLL_MODE);

	/* Ensure write completes before delaying */
	clk_osm_mb(c, PLL_BASE);

	usleep_range(50, 75);

	writel_relaxed(0x7, c->vbases[PLL_BASE] + PLL_MODE);
}

static void clk_osm_misc_programming(struct clk_osm *c)
{
	u32 lval = 0xFF, val;
	int i;

	clk_osm_write_reg(c, BVAL(23, 16, 0xF), SPM_CORE_COUNT_CTRL,
				OSM_BASE);
	clk_osm_write_reg(c, PLL_MIN_LVAL, PLL_MIN_FREQ_REG, OSM_BASE);

	/* Pattern to set/clear PLL lock in PDN_FSM_CTRL_REG */
	val = clk_osm_read_reg(c, PDN_FSM_CTRL_REG);
	if (c->secure_init) {
		val |= IGNORE_PLL_LOCK;
		clk_osm_write_seq_reg(c, val, DATA_MEM(108));
		val &= ~IGNORE_PLL_LOCK;
		clk_osm_write_seq_reg(c, val, DATA_MEM(109));
		clk_osm_write_seq_reg(c, MIN_VCO_VAL, DATA_MEM(110));
	} else {
		val |= IGNORE_PLL_LOCK;
		scm_io_write(c->pbases[SEQ_BASE] + DATA_MEM(108), val);
		val &= ~IGNORE_PLL_LOCK;
		scm_io_write(c->pbases[SEQ_BASE] + DATA_MEM(109), val);
	}

	/* Program LVAL corresponding to first turbo VC */
	for (i = 0; i < c->num_entries; i++) {
		if (c->osm_table[i].mem_acc_level == MAX_MEM_ACC_LEVELS) {
			lval = c->osm_table[i].freq_data & GENMASK(7, 0);
			break;
		}
	}

	if (c->secure_init)
		clk_osm_write_seq_reg(c, lval, DATA_MEM(114));
	else
		scm_io_write(c->pbases[SEQ_BASE] + DATA_MEM(114), lval);

}

static int clk_osm_setup_hw_table(struct clk_osm *c)
{
	struct osm_entry *entry = c->osm_table;
	int i;
	u32 freq_val = 0, volt_val = 0, override_val = 0;
	u32 table_entry_offset, last_mem_acc_level, last_virtual_corner = 0;

	for (i = 0; i < OSM_TABLE_SIZE; i++) {
		if (i < c->num_entries) {
			freq_val = entry[i].freq_data;
			volt_val = BVAL(27, 24, entry[i].mem_acc_level)
				| BVAL(21, 16, entry[i].virtual_corner)
				| BVAL(11, 0, entry[i].open_loop_volt);
			override_val = entry[i].override_data;

			if (last_virtual_corner && last_virtual_corner ==
			    entry[i].virtual_corner && last_mem_acc_level !=
			    entry[i].mem_acc_level) {
				pr_err("invalid LUT entry at row=%d virtual_corner=%d, mem_acc_level=%d\n",
				       i, entry[i].virtual_corner,
				       entry[i].mem_acc_level);
				return -EINVAL;
			}
			last_virtual_corner = entry[i].virtual_corner;
			last_mem_acc_level = entry[i].mem_acc_level;
		}

		table_entry_offset = i * OSM_REG_SIZE;
		clk_osm_write_reg(c, freq_val, FREQ_REG + table_entry_offset,
					OSM_BASE);
		clk_osm_write_reg(c, volt_val, VOLT_REG + table_entry_offset,
					OSM_BASE);
		clk_osm_write_reg(c, override_val, OVERRIDE_REG +
				  table_entry_offset, OSM_BASE);
	}

	/* Make sure all writes go through */
	clk_osm_mb(c, OSM_BASE);

	return 0;
}

static void clk_osm_print_osm_table(struct clk_osm *c)
{
	int i;
	struct osm_entry *table = c->osm_table;
	u32 pll_src, pll_div, lval, core_count;

	pr_debug("Index, Frequency, VC, OLV (mv), Core Count, PLL Src, PLL Div, L-Val, ACC Level\n");
	for (i = 0; i < c->num_entries; i++) {
		pll_src = (table[i].freq_data & GENMASK(31, 30)) >> 30;
		pll_div = (table[i].freq_data & GENMASK(29, 28)) >> 28;
		lval = table[i].freq_data & GENMASK(7, 0);
		core_count = (table[i].freq_data & GENMASK(18, 16)) >> 16;

		pr_debug("%3d, %11lu, %2u, %5u, %2u, %6u, %8u, %7u, %5u\n",
			i,
			table[i].frequency,
			table[i].virtual_corner,
			table[i].open_loop_volt,
			core_count,
			pll_src,
			pll_div,
			lval,
			table[i].mem_acc_level);
	}
	pr_debug("APM threshold corner=%d, crossover corner=%d\n",
			c->apm_threshold_vc, c->apm_crossover_vc);
	pr_debug("MEM-ACC threshold corner=%d, crossover corner=%d\n",
			c->mem_acc_threshold_vc, c->mem_acc_crossover_vc);
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

static int add_opp(struct clk_osm *c, struct device *dev)
{
	unsigned long rate = 0;
	u32 uv;
	long rc;
	int j = 0;
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

		if (rate == max_rate && max_rate != min_rate) {
			pr_info("Set OPP pair (%lu Hz, %d uv) on %s\n",
				rate, uv, dev_name(dev));
			break;
		}

		if (min_rate == max_rate)
			break;
	}
	return 0;
}

static void populate_opp_table(struct platform_device *pdev)
{
	int cpu;
	struct device *cpu_dev;
	struct clk_osm *c, *parent;
	struct clk_hw *hw_parent;
	struct device_node *l3_node_0, *l3_node_4;
	struct platform_device *l3_dev_0, *l3_dev_4;

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
			if (add_opp(parent, cpu_dev))
				pr_err("Failed to add OPP levels for %s\n",
					dev_name(cpu_dev));
	}

	l3_node_0 = of_parse_phandle(pdev->dev.of_node, "l3-dev0", 0);
	if (!l3_node_0) {
		pr_err("can't find the L3 cluster 0 dt node\n");
		return;
	}

	l3_dev_0 = of_find_device_by_node(l3_node_0);
	if (!l3_dev_0) {
		pr_err("can't find the L3 cluster 0 dt device\n");
		return;
	}

	if (add_opp(&l3_clk, &l3_dev_0->dev))
		pr_err("Failed to add OPP levels for L3 cluster 0\n");

	l3_node_4 = of_parse_phandle(pdev->dev.of_node, "l3-dev4", 0);
	if (!l3_node_4) {
		pr_err("can't find the L3 cluster 1 dt node\n");
		return;
	}

	l3_dev_4 = of_find_device_by_node(l3_node_4);
	if (!l3_dev_4) {
		pr_err("can't find the L3 cluster 1 dt device\n");
		return;
	}

	if (add_opp(&l3_clk, &l3_dev_4->dev))
		pr_err("Failed to add OPP levels for L3 cluster 1\n");
}

static u64 clk_osm_get_cpu_cycle_counter(int cpu)
{
	u32 val;
	unsigned long flags;
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
	if (parent->per_core_dcvs)
		val = clk_osm_read_reg_no_log(parent,
			OSM_CYCLE_COUNTER_STATUS_REG(c->core_num));
	else
		val = clk_osm_read_reg_no_log(parent,
			OSM_CYCLE_COUNTER_STATUS_REG(0));

	if (val < c->prev_cycle_counter) {
		/* Handle counter overflow */
		c->total_cycle_counter += UINT_MAX -
			c->prev_cycle_counter + val;
		c->prev_cycle_counter = val;
	} else {
		c->total_cycle_counter += val - c->prev_cycle_counter;
		c->prev_cycle_counter = val;
	}
	spin_unlock_irqrestore(&parent->lock, flags);

	return c->total_cycle_counter;
}

static void clk_osm_setup_cycle_counters(struct clk_osm *c)
{
	u32 ratio = c->osm_clk_rate;
	u32 val = 0;

	/* Enable cycle counter */
	val = BIT(0);
	/* Setup OSM clock to XO ratio */
	do_div(ratio, c->xo_clk_rate);
	val |= BVAL(5, 1, ratio - 1) | OSM_CYCLE_COUNTER_USE_XO_EDGE_EN;

	clk_osm_write_reg(c, val, OSM_CYCLE_COUNTER_CTRL_REG, OSM_BASE);
	pr_debug("OSM to XO clock ratio: %d\n", ratio);
}

static int clk_osm_resolve_crossover_corners(struct clk_osm *c,
					struct platform_device *pdev)
{
	struct regulator *regulator = c->vdd_reg;
	int count, vc, i, memacc_threshold, apm_threshold;
	int rc = 0;
	u32 corner_volt;

	if (c == &l3_clk || c == &pwrcl_clk)
		return rc;

	rc = of_property_read_u32(pdev->dev.of_node,
				  "qcom,perfcl-apcs-apm-threshold-voltage",
				  &apm_threshold);
	if (rc) {
		pr_err("qcom,perfcl-apcs-apm-threshold-voltage property not specified\n");
		return rc;
	}

	rc = of_property_read_u32(pdev->dev.of_node,
				  "qcom,perfcl-apcs-mem-acc-threshold-voltage",
				  &memacc_threshold);
	if (rc) {
		pr_err("qcom,perfcl-apcs-mem-acc-threshold-voltage property not specified\n");
		return rc;
	}

	/*
	 * Initialize VC settings in case none of them go above the voltage
	 * limits
	 */
	c->apm_threshold_vc = c->apm_crossover_vc = c->mem_acc_crossover_vc =
				c->mem_acc_threshold_vc = MAX_VC;

	count = regulator_count_voltages(regulator);
	if (count < 0) {
		pr_err("Failed to get the number of virtual corners supported\n");
		return count;
	}

	c->apm_crossover_vc = count - 2;
	c->mem_acc_crossover_vc = count - 1;

	for (i = 0; i < OSM_TABLE_SIZE; i++) {
		vc = c->osm_table[i].virtual_corner + 1;
		corner_volt = regulator_list_corner_voltage(regulator, vc);

		if (c->apm_threshold_vc == MAX_VC &&
				corner_volt >= apm_threshold)
			c->apm_threshold_vc = c->osm_table[i].virtual_corner;

		if (c->mem_acc_threshold_vc == MAX_VC &&
				corner_volt >= memacc_threshold)
			c->mem_acc_threshold_vc =
				c->osm_table[i].virtual_corner;
	}

	return rc;
}

static int clk_osm_resolve_open_loop_voltages(struct clk_osm *c)
{
	struct regulator *regulator = c->vdd_reg;
	u32 vc, mv;
	int i;

	for (i = 0; i < OSM_TABLE_SIZE; i++) {
		vc = c->osm_table[i].virtual_corner + 1;
		/* Voltage is in uv. Convert to mv */
		mv = regulator_list_corner_voltage(regulator, vc) / 1000;
		c->osm_table[i].open_loop_volt = mv;
	}

	return 0;
}

static int clk_osm_get_lut(struct platform_device *pdev,
			   struct clk_osm *c, char *prop_name)
{
	struct device_node *of = pdev->dev.of_node;
	int prop_len, total_elems, num_rows, i, j, k;
	int rc = 0;
	u32 *array;
	u32 *fmax_temp;
	u32 data;
	unsigned long abs_fmax = 0;
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

	fmax_temp = devm_kzalloc(&pdev->dev, num_rows * sizeof(unsigned long),
					GFP_KERNEL);
	if (!fmax_temp)
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
		c->osm_table[j].mem_acc_level = array[i + MEM_ACC_LEVEL];
		/* Voltage corners are 0 based in the OSM LUT */
		c->osm_table[j].virtual_corner = array[i + VIRTUAL_CORNER] - 1;
		pr_debug("index=%d freq=%ld virtual_corner=%d freq_data=0x%x override_data=0x%x mem_acc_level=0x%x\n",
			 j, c->osm_table[j].frequency,
			 c->osm_table[j].virtual_corner,
			 c->osm_table[j].freq_data,
			 c->osm_table[j].override_data,
			 c->osm_table[j].mem_acc_level);

		data = (array[i + FREQ_DATA] & GENMASK(18, 16)) >> 16;
		if (!last_entry && data == MAX_CORE_COUNT) {
			fmax_temp[k] = array[i];
			k++;
		}

		if (i < total_elems - NUM_FIELDS)
			i += NUM_FIELDS;
		else {
			abs_fmax = array[i];
			last_entry = true;
		}
	}
	fmax_temp[k] = abs_fmax;

	osm_clks_init[c->cluster_num].rate_max = devm_kzalloc(&pdev->dev,
						 k * sizeof(unsigned long),
						       GFP_KERNEL);
	if (!osm_clks_init[c->cluster_num].rate_max) {
		rc = -ENOMEM;
		goto exit;
	}

	for (i = 0; i < k; i++)
		osm_clks_init[c->cluster_num].rate_max[i] = fmax_temp[i];

	osm_clks_init[c->cluster_num].num_rate_max = k;
exit:
	devm_kfree(&pdev->dev, fmax_temp);
	devm_kfree(&pdev->dev, array);
	return rc;
}

static int clk_osm_parse_acd_dt_configs(struct platform_device *pdev)
{
	struct device_node *of = pdev->dev.of_node;
	u32 *array;
	int rc = 0;

	array = devm_kzalloc(&pdev->dev, MAX_CLUSTER_CNT * sizeof(u32),
				GFP_KERNEL);
	if (!array)
		return -ENOMEM;

	l3_clk.acd_init = platform_get_resource_byname(pdev, IORESOURCE_MEM,
				"l3_acd") != NULL ? true : false;
	pwrcl_clk.acd_init = platform_get_resource_byname(pdev, IORESOURCE_MEM,
				"pwrcl_acd") != NULL ? true : false;
	perfcl_clk.acd_init = platform_get_resource_byname(pdev, IORESOURCE_MEM,
				"perfcl_acd") != NULL ? true : false;

	if (pwrcl_clk.acd_init || perfcl_clk.acd_init || l3_clk.acd_init) {
		rc = of_property_read_u32_array(of, "qcom,acdtd-val",
						array, MAX_CLUSTER_CNT);
		if (rc) {
			dev_err(&pdev->dev, "unable to find qcom,acdtd-val property, rc=%d\n",
				rc);
			return -EINVAL;
		}

		pwrcl_clk.acd_td = array[pwrcl_clk.cluster_num];
		perfcl_clk.acd_td = array[perfcl_clk.cluster_num];
		l3_clk.acd_td = array[l3_clk.cluster_num];

		rc = of_property_read_u32_array(of, "qcom,acdcr-val",
						array, MAX_CLUSTER_CNT);
		if (rc) {
			dev_err(&pdev->dev, "unable to find qcom,acdcr-val property, rc=%d\n",
				rc);
			return -EINVAL;
		}

		pwrcl_clk.acd_cr = array[pwrcl_clk.cluster_num];
		perfcl_clk.acd_cr = array[perfcl_clk.cluster_num];
		l3_clk.acd_cr = array[l3_clk.cluster_num];

		rc = of_property_read_u32_array(of, "qcom,acdsscr-val",
						array, MAX_CLUSTER_CNT);
		if (rc) {
			dev_err(&pdev->dev, "unable to find qcom,acdsscr-val property, rc=%d\n",
				rc);
			return -EINVAL;
		}

		pwrcl_clk.acd_sscr = array[pwrcl_clk.cluster_num];
		perfcl_clk.acd_sscr = array[perfcl_clk.cluster_num];
		l3_clk.acd_sscr =  array[l3_clk.cluster_num];

		rc = of_property_read_u32_array(of, "qcom,acdextint0-val",
						array, MAX_CLUSTER_CNT);
		if (rc) {
			dev_err(&pdev->dev, "unable to find qcom,acdextint0-val property, rc=%d\n",
				rc);
			return -EINVAL;
		}

		pwrcl_clk.acd_extint0_cfg = array[pwrcl_clk.cluster_num];
		perfcl_clk.acd_extint0_cfg = array[perfcl_clk.cluster_num];
		l3_clk.acd_extint0_cfg =  array[l3_clk.cluster_num];

		rc = of_property_read_u32_array(of, "qcom,acdextint1-val",
						array, MAX_CLUSTER_CNT);
		if (rc) {
			dev_err(&pdev->dev, "unable to find qcom,acdextint1-val property, rc=%d\n",
				rc);
			return -EINVAL;
		}

		pwrcl_clk.acd_extint1_cfg = array[pwrcl_clk.cluster_num];
		perfcl_clk.acd_extint1_cfg = array[perfcl_clk.cluster_num];
		l3_clk.acd_extint1_cfg =  array[l3_clk.cluster_num];

		rc = of_property_read_u32_array(of, "qcom,acdautoxfer-val",
						array, MAX_CLUSTER_CNT);
		if (rc) {
			dev_err(&pdev->dev, "unable to find qcom,acdautoxfer-val property, rc=%d\n",
				rc);
			return -EINVAL;
		}

		pwrcl_clk.acd_autoxfer_ctl = array[pwrcl_clk.cluster_num];
		perfcl_clk.acd_autoxfer_ctl = array[perfcl_clk.cluster_num];
		l3_clk.acd_autoxfer_ctl =  array[l3_clk.cluster_num];

		rc = of_property_read_u32_array(of, "qcom,acdavg-init",
						array, MAX_CLUSTER_CNT);
		if (rc) {
			dev_err(&pdev->dev, "unable to find qcom,acdavg-init property, rc=%d\n",
				rc);
			return -EINVAL;
		}
		pwrcl_clk.acd_avg_init = array[pwrcl_clk.cluster_num];
		perfcl_clk.acd_avg_init = array[perfcl_clk.cluster_num];
		l3_clk.acd_avg_init =  array[l3_clk.cluster_num];
	}

	if (pwrcl_clk.acd_avg_init || perfcl_clk.acd_avg_init ||
	    l3_clk.acd_avg_init) {
		rc = of_property_read_u32_array(of, "qcom,acdavgcfg0-val",
						array, MAX_CLUSTER_CNT);
		if (rc) {
			dev_err(&pdev->dev, "unable to find qcom,acdavgcfg0-val property, rc=%d\n",
				rc);
			return -EINVAL;
		}
		pwrcl_clk.acd_avg_cfg0 = array[pwrcl_clk.cluster_num];
		perfcl_clk.acd_avg_cfg0 = array[perfcl_clk.cluster_num];
		l3_clk.acd_avg_cfg0 =  array[l3_clk.cluster_num];

		rc = of_property_read_u32_array(of, "qcom,acdavgcfg1-val",
						array, MAX_CLUSTER_CNT);
		if (rc) {
			dev_err(&pdev->dev, "unable to find qcom,acdavgcfg1-val property, rc=%d\n",
				rc);
			return -EINVAL;
		}
		pwrcl_clk.acd_avg_cfg1 = array[pwrcl_clk.cluster_num];
		perfcl_clk.acd_avg_cfg1 = array[perfcl_clk.cluster_num];
		l3_clk.acd_avg_cfg1 =  array[l3_clk.cluster_num];

		rc = of_property_read_u32_array(of, "qcom,acdavgcfg2-val",
						array, MAX_CLUSTER_CNT);
		if (rc) {
			dev_err(&pdev->dev, "unable to find qcom,acdavgcfg2-val property, rc=%d\n",
				rc);
			return -EINVAL;
		}
		pwrcl_clk.acd_avg_cfg2 = array[pwrcl_clk.cluster_num];
		perfcl_clk.acd_avg_cfg2 = array[perfcl_clk.cluster_num];
		l3_clk.acd_avg_cfg2 =  array[l3_clk.cluster_num];
	}

	devm_kfree(&pdev->dev, array);
	return rc;
}

static int clk_osm_parse_dt_configs(struct platform_device *pdev)
{
	struct device_node *of = pdev->dev.of_node;
	u32 *array;
	int rc = 0;
	struct resource *res;
	char l3_min_cpr_vc_str[] = "qcom,l3-min-cpr-vc-bin0";
	char pwrcl_min_cpr_vc_str[] = "qcom,pwrcl-min-cpr-vc-bin0";
	char perfcl_min_cpr_vc_str[] = "qcom,perfcl-min-cpr-vc-bin0";

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

	l3_clk.l_val_base = array[l3_clk.cluster_num];
	pwrcl_clk.l_val_base = array[pwrcl_clk.cluster_num];
	perfcl_clk.l_val_base = array[perfcl_clk.cluster_num];

	rc = of_property_read_u32_array(of, "qcom,apcs-pll-user-ctl",
					array, MAX_CLUSTER_CNT);
	if (rc) {
		dev_err(&pdev->dev, "unable to find qcom,apcs-pll-user-ctl property, rc=%d\n",
			rc);
		return -EINVAL;
	}

	l3_clk.apcs_pll_user_ctl = array[l3_clk.cluster_num];
	pwrcl_clk.apcs_pll_user_ctl = array[pwrcl_clk.cluster_num];
	perfcl_clk.apcs_pll_user_ctl = array[perfcl_clk.cluster_num];

	rc = of_property_read_u32_array(of, "qcom,apcs-pll-min-freq",
					array, MAX_CLUSTER_CNT);
	if (rc) {
		dev_err(&pdev->dev, "unable to find qcom,apcs-pll-min-freq property, rc=%d\n",
			rc);
		return -EINVAL;
	}

	l3_clk.apcs_pll_min_freq = array[l3_clk.cluster_num];
	pwrcl_clk.apcs_pll_min_freq = array[pwrcl_clk.cluster_num];
	perfcl_clk.apcs_pll_min_freq = array[perfcl_clk.cluster_num];

	rc = of_property_read_u32_array(of, "qcom,apm-mode-ctl",
				  array, MAX_CLUSTER_CNT);
	if (rc) {
		dev_err(&pdev->dev, "unable to find qcom,apm-mode-ctl property, rc=%d\n",
			rc);
		return -EINVAL;
	}

	l3_clk.apm_mode_ctl = array[l3_clk.cluster_num];
	pwrcl_clk.apm_mode_ctl = array[pwrcl_clk.cluster_num];
	perfcl_clk.apm_mode_ctl = array[perfcl_clk.cluster_num];

	rc = of_property_read_u32_array(of, "qcom,apm-status-ctrl",
				  array, MAX_CLUSTER_CNT);
	if (rc) {
		dev_err(&pdev->dev, "unable to find qcom,apm-status-ctrl property, rc=%d\n",
			rc);
		return -EINVAL;
	}

	l3_clk.apm_status_ctl = array[l3_clk.cluster_num];
	pwrcl_clk.apm_status_ctl = array[pwrcl_clk.cluster_num];
	perfcl_clk.apm_status_ctl = array[perfcl_clk.cluster_num];

	rc = of_property_read_u32_array(of, "qcom,cfg-gfmux-addr",
					array, MAX_CLUSTER_CNT);
	if (rc) {
		dev_err(&pdev->dev, "unable to find qcom,cfg-gfmux-addr property, rc=%d\n",
			rc);
		return -EINVAL;
	}

	l3_clk.cfg_gfmux_addr = array[l3_clk.cluster_num];
	pwrcl_clk.cfg_gfmux_addr = array[pwrcl_clk.cluster_num];
	perfcl_clk.cfg_gfmux_addr = array[perfcl_clk.cluster_num];

	rc = of_property_read_u32_array(of, "qcom,apcs-cbc-addr",
					array, MAX_CLUSTER_CNT);
	if (rc) {
		dev_err(&pdev->dev, "unable to find qcom,apcs-cbc-addr property, rc=%d\n",
			rc);
		return -EINVAL;
	}

	l3_clk.apcs_cbc_addr = array[l3_clk.cluster_num];
	pwrcl_clk.apcs_cbc_addr = array[pwrcl_clk.cluster_num];
	perfcl_clk.apcs_cbc_addr = array[perfcl_clk.cluster_num];

	rc = of_property_read_u32_array(of, "qcom,apcs-ramp-ctl-addr",
					array, MAX_CLUSTER_CNT);
	if (rc) {
		dev_err(&pdev->dev, "unable to find qcom,apcs-ramp-ctl-addr property, rc=%d\n",
			rc);
		return -EINVAL;
	}

	l3_clk.ramp_ctl_addr = array[l3_clk.cluster_num];
	pwrcl_clk.ramp_ctl_addr = array[pwrcl_clk.cluster_num];
	perfcl_clk.ramp_ctl_addr = array[perfcl_clk.cluster_num];

	rc = of_property_read_u32(of, "qcom,xo-clk-rate",
				  &pwrcl_clk.xo_clk_rate);
	if (rc) {
		dev_err(&pdev->dev, "unable to find qcom,xo-clk-rate property, rc=%d\n",
			rc);
		return -EINVAL;
	}

	l3_clk.xo_clk_rate = perfcl_clk.xo_clk_rate = pwrcl_clk.xo_clk_rate;

	rc = of_property_read_u32(of, "qcom,osm-clk-rate",
				  &pwrcl_clk.osm_clk_rate);
	if (rc) {
		dev_err(&pdev->dev, "unable to find qcom,osm-clk-rate property, rc=%d\n",
			rc);
		return -EINVAL;
	}
	l3_clk.osm_clk_rate = perfcl_clk.osm_clk_rate = pwrcl_clk.osm_clk_rate;

	rc = of_property_read_u32(of, "qcom,cc-reads",
				  &pwrcl_clk.cycle_counter_reads);
	if (rc) {
		dev_err(&pdev->dev, "unable to find qcom,cc-reads property, rc=%d\n",
			rc);
		return -EINVAL;
	}
	l3_clk.cycle_counter_reads = perfcl_clk.cycle_counter_reads =
			pwrcl_clk.cycle_counter_reads;

	rc = of_property_read_u32(of, "qcom,cc-delay",
				  &pwrcl_clk.cycle_counter_delay);
	if (rc)
		dev_dbg(&pdev->dev, "no delays between cycle counter reads\n");
	else
		l3_clk.cycle_counter_delay = perfcl_clk.cycle_counter_delay =
			pwrcl_clk.cycle_counter_delay;

	rc = of_property_read_u32(of, "qcom,cc-factor",
				  &pwrcl_clk.cycle_counter_factor);
	if (rc)
		dev_dbg(&pdev->dev, "no factor specified for cycle counter estimation\n");
	else
		l3_clk.cycle_counter_factor = perfcl_clk.cycle_counter_factor =
			pwrcl_clk.cycle_counter_factor;

	l3_clk.red_fsm_en = perfcl_clk.red_fsm_en = pwrcl_clk.red_fsm_en =
		of_property_read_bool(of, "qcom,red-fsm-en");

	l3_clk.boost_fsm_en = perfcl_clk.boost_fsm_en =
		pwrcl_clk.boost_fsm_en =
		of_property_read_bool(of, "qcom,boost-fsm-en");

	l3_clk.safe_fsm_en = perfcl_clk.safe_fsm_en = pwrcl_clk.safe_fsm_en =
		of_property_read_bool(of, "qcom,safe-fsm-en");

	l3_clk.ps_fsm_en = perfcl_clk.ps_fsm_en = pwrcl_clk.ps_fsm_en =
		of_property_read_bool(of, "qcom,ps-fsm-en");

	l3_clk.droop_fsm_en = perfcl_clk.droop_fsm_en =
		pwrcl_clk.droop_fsm_en =
		of_property_read_bool(of, "qcom,droop-fsm-en");

	devm_kfree(&pdev->dev, array);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"l3_sequencer");
	if (!res) {
		dev_err(&pdev->dev,
			"Unable to get platform resource for l3_sequencer\n");
		return -ENOMEM;
	}

	l3_clk.pbases[SEQ_BASE] = (unsigned long)res->start;
	l3_clk.vbases[SEQ_BASE] = devm_ioremap(&pdev->dev, res->start,
						  resource_size(res));

	if (!l3_clk.vbases[SEQ_BASE]) {
		dev_err(&pdev->dev, "Unable to map l3_sequencer base\n");
		return -ENOMEM;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"pwrcl_sequencer");
	if (!res) {
		dev_err(&pdev->dev,
			"Unable to get platform resource for pwrcl_sequencer\n");
		return -ENOMEM;
	}

	pwrcl_clk.pbases[SEQ_BASE] = (unsigned long)res->start;
	pwrcl_clk.vbases[SEQ_BASE] = devm_ioremap(&pdev->dev, res->start,
						  resource_size(res));

	if (!pwrcl_clk.vbases[SEQ_BASE]) {
		dev_err(&pdev->dev, "Unable to map pwrcl_sequencer base\n");
		return -ENOMEM;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"perfcl_sequencer");
	if (!res) {
		dev_err(&pdev->dev,
			"Unable to get platform resource for perfcl_sequencer\n");
		return -ENOMEM;
	}

	perfcl_clk.pbases[SEQ_BASE] = (unsigned long)res->start;
	perfcl_clk.vbases[SEQ_BASE] = devm_ioremap(&pdev->dev, res->start,
						  resource_size(res));

	if (!perfcl_clk.vbases[SEQ_BASE]) {
		dev_err(&pdev->dev, "Unable to map perfcl_sequencer base\n");
		return -ENOMEM;
	}

	snprintf(l3_min_cpr_vc_str, ARRAY_SIZE(l3_min_cpr_vc_str),
			"qcom,l3-min-cpr-vc-bin%d", l3_clk.speedbin);
	rc = of_property_read_u32(of, l3_min_cpr_vc_str, &l3_clk.min_cpr_vc);
	if (rc) {
		dev_err(&pdev->dev, "unable to find %s property, rc=%d\n",
			l3_min_cpr_vc_str, rc);
		return -EINVAL;
	}

	snprintf(pwrcl_min_cpr_vc_str, ARRAY_SIZE(pwrcl_min_cpr_vc_str),
			"qcom,pwrcl-min-cpr-vc-bin%d", pwrcl_clk.speedbin);
	rc = of_property_read_u32(of, pwrcl_min_cpr_vc_str,
						&pwrcl_clk.min_cpr_vc);
	if (rc) {
		dev_err(&pdev->dev, "unable to find %s property, rc=%d\n",
			pwrcl_min_cpr_vc_str, rc);
		return -EINVAL;
	}

	snprintf(perfcl_min_cpr_vc_str, ARRAY_SIZE(perfcl_min_cpr_vc_str),
			"qcom,perfcl-min-cpr-vc-bin%d", perfcl_clk.speedbin);
	rc = of_property_read_u32(of, perfcl_min_cpr_vc_str,
						&perfcl_clk.min_cpr_vc);
	if (rc) {
		dev_err(&pdev->dev, "unable to find %s property, rc=%d\n",
			perfcl_min_cpr_vc_str, rc);
		return -EINVAL;
	}

	l3_clk.secure_init = perfcl_clk.secure_init = pwrcl_clk.secure_init =
		of_property_read_bool(pdev->dev.of_node, "qcom,osm-no-tz");

	if (!pwrcl_clk.secure_init)
		return rc;

	rc = of_property_read_u32_array(of, "qcom,l3-mem-acc-addr",
					l3_clk.mem_acc_addr, MEM_ACC_ADDRS);
	if (rc) {
		dev_err(&pdev->dev, "unable to find qcom,l3-mem-acc-addr property, rc=%d\n",
			rc);
		return -EINVAL;
	}

	rc = of_property_read_u32_array(of, "qcom,pwrcl-mem-acc-addr",
					pwrcl_clk.mem_acc_addr, MEM_ACC_ADDRS);
	if (rc) {
		dev_err(&pdev->dev, "unable to find qcom,pwrcl-mem-acc-addr property, rc=%d\n",
			rc);
		return -EINVAL;
	}

	rc = of_property_read_u32_array(of, "qcom,perfcl-mem-acc-addr",
					perfcl_clk.mem_acc_addr, MEM_ACC_ADDRS);
	if (rc) {
		dev_err(&pdev->dev, "unable to find qcom,perfcl-mem-acc-addr property, rc=%d\n",
			rc);
		return -EINVAL;
	}

	return rc;
}

static int clk_osm_acd_resources_init(struct platform_device *pdev)
{
	struct resource *res;
	unsigned long pbase;
	void *vbase;
	int rc = 0;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"pwrcl_acd");
	if (res) {
		pbase = (unsigned long)res->start;
		vbase = devm_ioremap(&pdev->dev, res->start,
					resource_size(res));
		if (!vbase) {
			dev_err(&pdev->dev, "Unable to map pwrcl_acd base\n");
			return -ENOMEM;
		}
		pwrcl_clk.pbases[ACD_BASE] = pbase;
		pwrcl_clk.vbases[ACD_BASE] = vbase;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"perfcl_acd");
	if (res) {
		pbase = (unsigned long)res->start;
		vbase = devm_ioremap(&pdev->dev, res->start,
					resource_size(res));
		if (!vbase) {
			dev_err(&pdev->dev, "Unable to map perfcl_acd base\n");
			return -ENOMEM;
		}
		perfcl_clk.pbases[ACD_BASE] = pbase;
		perfcl_clk.vbases[ACD_BASE] = vbase;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"l3_acd");
	if (res) {
		pbase = (unsigned long)res->start;
		vbase = devm_ioremap(&pdev->dev, res->start,
					resource_size(res));
		if (!vbase) {
			dev_err(&pdev->dev, "Unable to map l3_acd base\n");
			return -ENOMEM;
		}
		l3_clk.pbases[ACD_BASE] = pbase;
		l3_clk.vbases[ACD_BASE] = vbase;
	}
	return rc;
}

static int clk_osm_resources_init(struct platform_device *pdev)
{
	struct device_node *node;
	struct resource *res;
	unsigned long pbase;
	int rc = 0;
	void *vbase;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"osm_l3_base");
	if (!res) {
		dev_err(&pdev->dev,
			"Unable to get platform resource for osm_l3_base");
		return -ENOMEM;
	}

	l3_clk.pbases[OSM_BASE] = (unsigned long)res->start;
	l3_clk.vbases[OSM_BASE] = devm_ioremap(&pdev->dev, res->start,
						  resource_size(res));

	if (!l3_clk.vbases[OSM_BASE]) {
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

	pwrcl_clk.pbases[OSM_BASE] = (unsigned long)res->start;
	pwrcl_clk.vbases[OSM_BASE] = devm_ioremap(&pdev->dev, res->start,
						  resource_size(res));
	if (!pwrcl_clk.vbases[OSM_BASE]) {
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

	perfcl_clk.pbases[OSM_BASE] = (unsigned long)res->start;
	perfcl_clk.vbases[OSM_BASE] = devm_ioremap(&pdev->dev, res->start,
						  resource_size(res));

	if (!perfcl_clk.vbases[OSM_BASE]) {
		dev_err(&pdev->dev, "Unable to map osm_perfcl_base base\n");
		return -ENOMEM;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "l3_pll");
	if (!res) {
		dev_err(&pdev->dev,
			"Unable to get platform resource for l3_pll\n");
		return -ENOMEM;
	}
	pbase = (unsigned long)res->start;
	vbase = devm_ioremap(&pdev->dev, res->start, resource_size(res));

	if (!vbase) {
		dev_err(&pdev->dev, "Unable to map l3_pll base\n");
		return -ENOMEM;
	}

	l3_clk.pbases[PLL_BASE] = pbase;
	l3_clk.vbases[PLL_BASE] = vbase;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "pwrcl_pll");
	if (!res) {
		dev_err(&pdev->dev,
			"Unable to get platform resource for pwrcl_pll\n");
		return -ENOMEM;
	}
	pbase = (unsigned long)res->start;
	vbase = devm_ioremap(&pdev->dev, res->start, resource_size(res));

	if (!vbase) {
		dev_err(&pdev->dev, "Unable to map pwrcl_pll base\n");
		return -ENOMEM;
	}

	pwrcl_clk.pbases[PLL_BASE] = pbase;
	pwrcl_clk.vbases[PLL_BASE] = vbase;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "perfcl_pll");
	if (!res) {
		dev_err(&pdev->dev,
			"Unable to get platform resource for perfcl_pll\n");
		return -ENOMEM;
	}
	pbase = (unsigned long)res->start;
	vbase = devm_ioremap(&pdev->dev, res->start, resource_size(res));

	if (!vbase) {
		dev_err(&pdev->dev, "Unable to map perfcl_pll base\n");
		return -ENOMEM;
	}

	perfcl_clk.pbases[PLL_BASE] = pbase;
	perfcl_clk.vbases[PLL_BASE] = vbase;

	/* efuse speed bin fuses are optional */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
					   "l3_efuse");
	if (res) {
		pbase = (unsigned long)res->start;
		vbase = devm_ioremap(&pdev->dev, res->start,
				     resource_size(res));
		if (!vbase) {
			dev_err(&pdev->dev, "Unable to map in l3_efuse base\n");
			return -ENOMEM;
		}
		l3_clk.pbases[EFUSE_BASE] = pbase;
		l3_clk.vbases[EFUSE_BASE] = vbase;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
					   "pwrcl_efuse");
	if (res) {
		pbase = (unsigned long)res->start;
		vbase = devm_ioremap(&pdev->dev, res->start,
				     resource_size(res));
		if (!vbase) {
			dev_err(&pdev->dev, "Unable to map pwrcl_efuse base\n");
			return -ENOMEM;
		}
		pwrcl_clk.pbases[EFUSE_BASE] = pbase;
		pwrcl_clk.vbases[EFUSE_BASE] = vbase;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
					   "perfcl_efuse");
	if (res) {
		pbase = (unsigned long)res->start;
		vbase = devm_ioremap(&pdev->dev, res->start,
				     resource_size(res));
		if (!vbase) {
			dev_err(&pdev->dev, "Unable to map perfcl_efuse base\n");
			return -ENOMEM;
		}
		perfcl_clk.pbases[EFUSE_BASE] = pbase;
		perfcl_clk.vbases[EFUSE_BASE] = vbase;
	}

	vdd_l3 = devm_regulator_get(&pdev->dev, "vdd-l3");
	if (IS_ERR(vdd_l3)) {
		rc = PTR_ERR(vdd_l3);
		if (rc != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get the l3 vreg, rc=%d\n",
				rc);
		return rc;
	}
	l3_clk.vdd_reg = vdd_l3;

	vdd_pwrcl = devm_regulator_get(&pdev->dev, "vdd-pwrcl");
	if (IS_ERR(vdd_pwrcl)) {
		rc = PTR_ERR(vdd_pwrcl);
		if (rc != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get the pwrcl vreg, rc=%d\n",
				rc);
		return rc;
	}
	pwrcl_clk.vdd_reg = vdd_pwrcl;

	vdd_perfcl = devm_regulator_get(&pdev->dev, "vdd-perfcl");
	if (IS_ERR(vdd_perfcl)) {
		rc = PTR_ERR(vdd_perfcl);
		if (rc != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get the perfcl vreg, rc=%d\n",
				rc);
		return rc;
	}
	perfcl_clk.vdd_reg = vdd_perfcl;

	node = of_parse_phandle(pdev->dev.of_node, "vdd-l3-supply", 0);
	if (!node) {
		pr_err("Unable to find vdd-l3-supply\n");
		return -EINVAL;
	}

	l3_clk.vdd_dev = of_find_device_by_node(node->parent->parent);
	if (!l3_clk.vdd_dev) {
		pr_err("Unable to find device for vdd-l3-supply node\n");
		return -EINVAL;
	}

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

	node = of_parse_phandle(pdev->dev.of_node, "vdd-perfcl-supply", 0);
	if (!node) {
		pr_err("Unable to find vdd-perfcl-supply\n");
		return -EINVAL;
	}

	perfcl_clk.vdd_dev = of_find_device_by_node(node->parent->parent);
	if (!perfcl_clk.vdd_dev) {
		pr_err("Unable to find device for vdd-perfcl-supply\n");
		return -EINVAL;
	}

	return 0;
}

static int debugfs_get_debug_reg(void *data, u64 *val)
{
	struct clk_osm *c = data;

	if (c->acd_debugfs_addr >= ACD_MASTER_ONLY_REG_ADDR)
		*val = readl_relaxed((char *)c->vbases[ACD_BASE] +
					c->acd_debugfs_addr);
	else
		*val = clk_osm_acd_local_read_reg(c, c->acd_debugfs_addr);
	return 0;
}

static int debugfs_set_debug_reg(void *data, u64 val)
{
	struct clk_osm *c = data;

	if (c->acd_debugfs_addr >= ACD_MASTER_ONLY_REG_ADDR)
		clk_osm_write_reg(c, val, c->acd_debugfs_addr, ACD_BASE);
	else
		clk_osm_acd_master_write_through_reg(c, val,
							c->acd_debugfs_addr);

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(debugfs_acd_debug_reg_fops,
			debugfs_get_debug_reg,
			debugfs_set_debug_reg,
			"0x%llx\n");

static int debugfs_get_debug_reg_addr(void *data, u64 *val)
{
	struct clk_osm *c = data;

	*val = c->acd_debugfs_addr;
	return 0;
}

static int debugfs_set_debug_reg_addr(void *data, u64 val)
{
	struct clk_osm *c = data;

	if (val > ACD_1P1_MAX_REG_OFFSET) {
		pr_err("invalid ACD register address offset, must be between 0-0x%x\n",
			ACD_1P1_MAX_REG_OFFSET);
		return 0;
	}

	c->acd_debugfs_addr = val;
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(debugfs_acd_debug_reg_addr_fops,
			debugfs_get_debug_reg_addr,
			debugfs_set_debug_reg_addr,
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

	c->debugfs = debugfs_create_dir(clk_hw_get_name(&c->hw),
					osm_debugfs_base);
	if (IS_ERR_OR_NULL(c->debugfs)) {
		pr_err("osm debugfs directory creation failed\n");
		return;
	}

	temp = debugfs_create_file("acd_debug_reg",
			0644,
			c->debugfs, c,
			&debugfs_acd_debug_reg_fops);
	if (IS_ERR_OR_NULL(temp)) {
		pr_err("debugfs_acd_debug_reg_fops debugfs file creation failed\n");
		goto exit;
	}

	temp = debugfs_create_file("acd_debug_reg_addr",
			0644,
			c->debugfs, c,
			&debugfs_acd_debug_reg_addr_fops);
	if (IS_ERR_OR_NULL(temp)) {
		pr_err("debugfs_acd_debug_reg_addr_fops debugfs file creation failed\n");
		goto exit;
	}

exit:
	if (IS_ERR_OR_NULL(temp))
		debugfs_remove_recursive(c->debugfs);
}

static int clk_osm_acd_init(struct clk_osm *c)
{

	int rc = 0;
	u32 auto_xfer_mask = 0;

	if (c->secure_init) {
		clk_osm_write_reg(c, c->pbases[ACD_BASE] + ACDCR,
					DATA_MEM(115), OSM_BASE);
		clk_osm_write_reg(c, c->pbases[ACD_BASE] + ACD_WRITE_CTL,
					DATA_MEM(116), OSM_BASE);
	}

	if (!c->acd_init)
		return 0;

	c->acd_debugfs_addr = ACD_HW_VERSION;

	/* Program ACD tunable-length delay register */
	clk_osm_write_reg(c, c->acd_td, ACDTD, ACD_BASE);
	auto_xfer_mask |= ACD_REG_RELATIVE_ADDR_BITMASK(ACDTD);

	/* Program ACD control register */
	clk_osm_write_reg(c, c->acd_cr, ACDCR, ACD_BASE);
	auto_xfer_mask |= ACD_REG_RELATIVE_ADDR_BITMASK(ACDCR);

	/* Program ACD soft start control register */
	clk_osm_write_reg(c, c->acd_sscr, ACDSSCR, ACD_BASE);
	auto_xfer_mask |= ACD_REG_RELATIVE_ADDR_BITMASK(ACDSSCR);

	/* Program initial ACD external interface configuration register */
	clk_osm_write_reg(c, c->acd_extint0_cfg, ACD_EXTINT_CFG, ACD_BASE);
	auto_xfer_mask |= ACD_REG_RELATIVE_ADDR_BITMASK(ACD_EXTINT_CFG);

	/* Program ACD auto-register transfer control register */
	clk_osm_write_reg(c, c->acd_autoxfer_ctl, ACD_AUTOXFER_CTL, ACD_BASE);

	/* Ensure writes complete before transfers to local copy */
	clk_osm_acd_mb(c);

	/* Transfer master copies */
	rc = clk_osm_acd_auto_local_write_reg(c, auto_xfer_mask);
	if (rc)
		return rc;

	/* Switch CPUSS clock source to ACD clock */
	auto_xfer_mask |= ACD_REG_RELATIVE_ADDR_BITMASK(ACD_GFMUX_CFG);
	rc = clk_osm_acd_master_write_through_reg(c, ACD_GFMUX_CFG_SELECT,
							ACD_GFMUX_CFG);
	if (rc)
		return rc;

	/* Program ACD_DCVS_SW */
	rc = clk_osm_acd_master_write_through_reg(c,
					ACD_DCVS_SW_DCVS_IN_PRGR_SET,
					ACD_DCVS_SW);
	if (rc)
		return rc;

	rc = clk_osm_acd_master_write_through_reg(c,
					ACD_DCVS_SW_DCVS_IN_PRGR_CLEAR,
					ACD_DCVS_SW);
	if (rc)
		return rc;

	udelay(1);

	/* Program final ACD external interface configuration register */
	rc = clk_osm_acd_master_write_through_reg(c, c->acd_extint1_cfg,
							ACD_EXTINT_CFG);
	if (rc)
		return rc;

	if (c->acd_avg_init) {
		auto_xfer_mask |= ACD_REG_RELATIVE_ADDR_BITMASK(ACD_AVG_CFG_2);
		rc = clk_osm_acd_master_write_through_reg(c, c->acd_avg_cfg2,
								ACD_AVG_CFG_2);
		if (rc)
			return rc;

		auto_xfer_mask |= ACD_REG_RELATIVE_ADDR_BITMASK(ACD_AVG_CFG_1);
		rc = clk_osm_acd_master_write_through_reg(c, c->acd_avg_cfg1,
								ACD_AVG_CFG_1);
		if (rc)
			return rc;

		auto_xfer_mask |= ACD_REG_RELATIVE_ADDR_BITMASK(ACD_AVG_CFG_0);
		rc = clk_osm_acd_master_write_through_reg(c, c->acd_avg_cfg0,
								ACD_AVG_CFG_0);
		if (rc)
			return rc;
	}

	/*
	 * ACDCR, ACDTD, ACDSSCR, ACD_EXTINT_CFG, ACD_GFMUX_CFG
	 * must be copied from master to local copy on PC exit.
	 * Also, ACD_AVG_CFG0, ACF_AVG_CFG1, and ACD_AVG_CFG2 when
	 * AVG is enabled.
	 */
	clk_osm_write_reg(c, auto_xfer_mask, ACD_AUTOXFER_CFG, ACD_BASE);
	return 0;
}

static int clk_cpu_osm_driver_probe(struct platform_device *pdev)
{
	int rc = 0, i;
	int pvs_ver = 0;
	u32 pte_efuse, val;
	int num_clks = ARRAY_SIZE(osm_qcom_clk_hws);
	struct clk *ext_xo_clk, *clk;
	struct device *dev = &pdev->dev;
	struct clk_onecell_data *clk_data;
	char l3speedbinstr[] = "qcom,l3-speedbin0-v0";
	char perfclspeedbinstr[] = "qcom,perfcl-speedbin0-v0";
	char pwrclspeedbinstr[] = "qcom,pwrcl-speedbin0-v0";
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

	clk_data = devm_kzalloc(&pdev->dev, sizeof(struct clk_onecell_data),
								GFP_KERNEL);
	if (!clk_data)
		goto exit;

	clk_data->clks = devm_kzalloc(&pdev->dev, (num_clks *
					sizeof(struct clk *)), GFP_KERNEL);
	if (!clk_data->clks)
		goto clk_err;

	clk_data->clk_num = num_clks;

	rc = clk_osm_parse_dt_configs(pdev);
	if (rc) {
		dev_err(&pdev->dev, "Unable to parse OSM device tree configurations\n");
		return rc;
	}

	rc = clk_osm_parse_acd_dt_configs(pdev);
	if (rc) {
		dev_err(&pdev->dev, "Unable to parse ACD device tree configurations\n");
		return rc;
	}

	rc = clk_osm_resources_init(pdev);
	if (rc) {
		if (rc != -EPROBE_DEFER)
			dev_err(&pdev->dev, "OSM resources init failed, rc=%d\n",
				rc);
		return rc;
	}

	rc = clk_osm_acd_resources_init(pdev);
	if (rc) {
		dev_err(&pdev->dev, "ACD resources init failed, rc=%d\n",
			rc);
		return rc;
	}

	if (l3_clk.vbases[EFUSE_BASE]) {
		/* Multiple speed-bins are supported */
		pte_efuse = readl_relaxed(l3_clk.vbases[EFUSE_BASE]);
		l3_clk.speedbin = ((pte_efuse >> L3_EFUSE_SHIFT) &
						    L3_EFUSE_MASK);
		snprintf(l3speedbinstr, ARRAY_SIZE(l3speedbinstr),
			 "qcom,l3-speedbin%d-v%d", l3_clk.speedbin, pvs_ver);
	}

	dev_info(&pdev->dev, "using L3 speed bin %u and pvs_ver %d\n",
		 l3_clk.speedbin, pvs_ver);

	rc = clk_osm_get_lut(pdev, &l3_clk, l3speedbinstr);
	if (rc) {
		dev_err(&pdev->dev, "Unable to get OSM LUT for L3, rc=%d\n",
			rc);
		return rc;
	}

	if (pwrcl_clk.vbases[EFUSE_BASE]) {
		/* Multiple speed-bins are supported */
		pte_efuse = readl_relaxed(pwrcl_clk.vbases[EFUSE_BASE]);
		pwrcl_clk.speedbin = ((pte_efuse >> PWRCL_EFUSE_SHIFT) &
						    PWRCL_EFUSE_MASK);
		snprintf(pwrclspeedbinstr, ARRAY_SIZE(pwrclspeedbinstr),
			 "qcom,pwrcl-speedbin%d-v%d", pwrcl_clk.speedbin,
							pvs_ver);
	}

	dev_info(&pdev->dev, "using pwrcl speed bin %u and pvs_ver %d\n",
		 pwrcl_clk.speedbin, pvs_ver);

	rc = clk_osm_get_lut(pdev, &pwrcl_clk, pwrclspeedbinstr);
	if (rc) {
		dev_err(&pdev->dev, "Unable to get OSM LUT for power cluster, rc=%d\n",
			rc);
		return rc;
	}

	if (perfcl_clk.vbases[EFUSE_BASE]) {
		/* Multiple speed-bins are supported */
		pte_efuse = readl_relaxed(perfcl_clk.vbases[EFUSE_BASE]);
		perfcl_clk.speedbin = ((pte_efuse >> PERFCL_EFUSE_SHIFT) &
							PERFCL_EFUSE_MASK);
		snprintf(perfclspeedbinstr, ARRAY_SIZE(perfclspeedbinstr),
			 "qcom,perfcl-speedbin%d-v%d", perfcl_clk.speedbin,
							pvs_ver);
	}

	dev_info(&pdev->dev, "using perfcl speed bin %u and pvs_ver %d\n",
		 perfcl_clk.speedbin, pvs_ver);

	rc = clk_osm_get_lut(pdev, &perfcl_clk, perfclspeedbinstr);
	if (rc) {
		dev_err(&pdev->dev, "Unable to get OSM LUT for perf cluster, rc=%d\n",
			rc);
		return rc;
	}

	rc = clk_osm_resolve_open_loop_voltages(&l3_clk);
	if (rc) {
		if (rc == -EPROBE_DEFER)
			return rc;
		dev_err(&pdev->dev, "Unable to determine open-loop voltages for L3, rc=%d\n",
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

	rc = clk_osm_resolve_crossover_corners(&l3_clk, pdev);
	if (rc)
		dev_info(&pdev->dev,
			"No APM crossover corner programmed for L3\n");

	rc = clk_osm_resolve_crossover_corners(&pwrcl_clk, pdev);
	if (rc)
		dev_info(&pdev->dev,
			"No APM crossover corner programmed for pwrcl_clk\n");

	rc = clk_osm_resolve_crossover_corners(&perfcl_clk, pdev);
	if (rc)
		dev_info(&pdev->dev, "No MEM-ACC crossover corner programmed\n");

	clk_osm_setup_cycle_counters(&l3_clk);
	clk_osm_setup_cycle_counters(&pwrcl_clk);
	clk_osm_setup_cycle_counters(&perfcl_clk);

	clk_osm_print_osm_table(&l3_clk);
	clk_osm_print_osm_table(&pwrcl_clk);
	clk_osm_print_osm_table(&perfcl_clk);

	rc = clk_osm_setup_hw_table(&l3_clk);
	if (rc) {
		dev_err(&pdev->dev, "failed to setup l3 hardware table\n");
		goto exit;
	}
	rc = clk_osm_setup_hw_table(&pwrcl_clk);
	if (rc) {
		dev_err(&pdev->dev, "failed to setup power cluster hardware table\n");
		goto exit;
	}
	rc = clk_osm_setup_hw_table(&perfcl_clk);
	if (rc) {
		dev_err(&pdev->dev, "failed to setup perf cluster hardware table\n");
		goto exit;
	}

	/* Policy tuning */
	rc = clk_osm_set_cc_policy(pdev);
	if (rc < 0) {
		dev_err(&pdev->dev, "cc policy setup failed");
		goto exit;
	}

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

	clk_osm_setup_fsms(&l3_clk);
	clk_osm_setup_fsms(&pwrcl_clk);
	clk_osm_setup_fsms(&perfcl_clk);

	/* Program VC at which the array power supply needs to be switched */
	clk_osm_write_reg(&perfcl_clk, perfcl_clk.apm_threshold_vc,
				APM_CROSSOVER_VC, OSM_BASE);
	if (perfcl_clk.secure_init) {
		clk_osm_write_seq_reg(&perfcl_clk, perfcl_clk.apm_crossover_vc,
				DATA_MEM(77));
		clk_osm_write_seq_reg(&perfcl_clk,
				(0x39 | (perfcl_clk.apm_threshold_vc << 6)),
				DATA_MEM(111));
	} else {
		scm_io_write(perfcl_clk.pbases[SEQ_BASE] + DATA_MEM(77),
				perfcl_clk.apm_crossover_vc);
		scm_io_write(perfcl_clk.pbases[SEQ_BASE] + DATA_MEM(111),
				(0x39 | (perfcl_clk.apm_threshold_vc << 6)));
	}

	/*
	 * Perform typical secure-world HW initialization
	 * as necessary.
	 */
	clk_osm_do_additional_setup(&l3_clk, pdev);
	clk_osm_do_additional_setup(&pwrcl_clk, pdev);
	clk_osm_do_additional_setup(&perfcl_clk, pdev);

	/* MEM-ACC Programming */
	clk_osm_program_mem_acc_regs(&l3_clk);
	clk_osm_program_mem_acc_regs(&pwrcl_clk);
	clk_osm_program_mem_acc_regs(&perfcl_clk);

	if (of_property_read_bool(pdev->dev.of_node, "qcom,osm-pll-setup")) {
		clk_osm_setup_cluster_pll(&l3_clk);
		clk_osm_setup_cluster_pll(&pwrcl_clk);
		clk_osm_setup_cluster_pll(&perfcl_clk);
	}

	/* Misc programming */
	clk_osm_misc_programming(&l3_clk);
	clk_osm_misc_programming(&pwrcl_clk);
	clk_osm_misc_programming(&perfcl_clk);

	pwrcl_clk.per_core_dcvs = perfcl_clk.per_core_dcvs =
			of_property_read_bool(pdev->dev.of_node,
				"qcom,enable-per-core-dcvs");
	if (pwrcl_clk.per_core_dcvs) {
		val = clk_osm_read_reg(&pwrcl_clk, CORE_DCVS_CTRL);
		val |= BIT(0);
		clk_osm_write_reg(&pwrcl_clk, val, CORE_DCVS_CTRL, OSM_BASE);

		val = clk_osm_read_reg(&perfcl_clk, CORE_DCVS_CTRL);
		val |= BIT(0);
		clk_osm_write_reg(&perfcl_clk, val, CORE_DCVS_CTRL, OSM_BASE);
	}

	clk_ops_core = clk_dummy_ops;
	clk_ops_core.set_rate = cpu_clk_set_rate;
	clk_ops_core.round_rate = cpu_clk_round_rate;
	clk_ops_core.recalc_rate = cpu_clk_recalc_rate;

	rc = clk_osm_acd_init(&l3_clk);
	if (rc) {
		pr_err("failed to initialize ACD for L3, rc=%d\n", rc);
		goto exit;
	}
	rc = clk_osm_acd_init(&pwrcl_clk);
	if (rc) {
		pr_err("failed to initialize ACD for pwrcl, rc=%d\n", rc);
		goto exit;
	}
	rc = clk_osm_acd_init(&perfcl_clk);
	if (rc) {
		pr_err("failed to initialize ACD for perfcl, rc=%d\n", rc);
		goto exit;
	}

	spin_lock_init(&l3_clk.lock);
	spin_lock_init(&pwrcl_clk.lock);
	spin_lock_init(&perfcl_clk.lock);

	/* Register OSM l3, pwr and perf clocks with Clock Framework */
	for (i = 0; i < num_clks; i++) {
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

	/* Set the L3 clock to run off GPLL0 and enable OSM for the domain */
	rc = clk_set_rate(l3_clk.hw.clk, OSM_INIT_RATE);
	if (rc) {
		dev_err(&pdev->dev, "Unable to set init rate on L3 cluster, rc=%d\n",
			rc);
		goto provider_err;
	}
	WARN(clk_prepare_enable(l3_clk.hw.clk),
		     "Failed to enable clock for L3\n");
	udelay(300);

	/* Configure default rate to lowest frequency */
	for (i = 0; i < MAX_CORE_COUNT; i++) {
		osm_set_index(&pwrcl_clk, 0, i);
		osm_set_index(&perfcl_clk, 0, i);
	}

	populate_opp_table(pdev);
	populate_debugfs_dir(&l3_clk);
	populate_debugfs_dir(&pwrcl_clk);
	populate_debugfs_dir(&perfcl_clk);

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
arch_initcall(clk_cpu_osm_init);

static void __exit clk_cpu_osm_exit(void)
{
	platform_driver_unregister(&clk_cpu_osm_driver);
}
module_exit(clk_cpu_osm_exit);

MODULE_DESCRIPTION("QTI CPU clock driver for OSM");
MODULE_LICENSE("GPL v2");
