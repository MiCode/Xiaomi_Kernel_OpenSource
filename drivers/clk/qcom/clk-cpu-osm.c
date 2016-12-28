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
#include <soc/qcom/scm.h>
#include <dt-bindings/clock/qcom,cpu-osm.h>

#include "common.h"
#include "clk-regmap.h"
#include "clk-rcg.h"

enum {
	LMH_LITE_CLK_SRC,
	P_XO,
};

enum clk_osm_bases {
	OSM_BASE,
	PLL_BASE,
	EFUSE_BASE,
	ACD_BASE,
	NUM_BASES,
};

enum clk_osm_lut_data {
	FREQ,
	FREQ_DATA,
	PLL_OVERRIDES,
	SPARE_DATA,
	VIRTUAL_CORNER,
	NUM_FIELDS,
};

enum clk_osm_trace_method {
	XOR_PACKET,
	PERIODIC_PACKET,
};

enum clk_osm_trace_packet_id {
	TRACE_PACKET0,
	TRACE_PACKET1,
	TRACE_PACKET2,
	TRACE_PACKET3,
};

#define SEQ_REG(n)					(0x300 + (n) * 4)
#define MEM_ACC_SEQ_REG_CFG_START(n)			(SEQ_REG(12 + (n)))
#define MEM_ACC_SEQ_CONST(n)				(n)
#define MEM_ACC_INSTR_COMP(n)				(0x67 + ((n) * 0x40))
#define MEM_ACC_SEQ_REG_VAL_START(n)			(SEQ_REG(60 + (n)))
#define SEQ_REG1_OFFSET					0x1048
#define VERSION_REG					0x0

#define OSM_TABLE_SIZE					40
#define MAX_CLUSTER_CNT					2
#define LLM_SW_OVERRIDE_CNT				3
#define CORE_COUNT_VAL(val)			((val & GENMASK(18, 16)) >> 16)
#define SINGLE_CORE					1
#define MAX_CORE_COUNT					4

#define ENABLE_REG					0x1004
#define INDEX_REG					0x1150
#define FREQ_REG					0x1154
#define VOLT_REG					0x1158
#define OVERRIDE_REG					0x115C
#define SPARE_REG					0x1164

#define OSM_CYCLE_COUNTER_CTRL_REG			0x1F00
#define OSM_CYCLE_COUNTER_STATUS_REG			0x1F04
#define DCVS_PERF_STATE_DESIRED_REG			0x1F10
#define DCVS_PERF_STATE_DEVIATION_INTR_STAT		0x1F14
#define DCVS_PERF_STATE_DEVIATION_INTR_EN		0x1F18
#define DCVS_PERF_STATE_DEVIATION_INTR_CLEAR		0x1F1C
#define DCVS_PERF_STATE_DEVIATION_CORRECTED_INTR_STAT	0x1F20
#define DCVS_PERF_STATE_DEVIATION_CORRECTED_INTR_EN	0x1F24
#define DCVS_PERF_STATE_DEVIATION_CORRECTED_INTR_CLEAR	0x1F28
#define DCVS_PERF_STATE_MET_INTR_STAT			0x1F2C
#define DCVS_PERF_STATE_MET_INTR_EN			0x1F30
#define DCVS_PERF_STATE_MET_INTR_CLR			0x1F34
#define OSM_CORE_TABLE_SIZE				8192
#define OSM_REG_SIZE					32

#define WDOG_DOMAIN_PSTATE_STATUS			0x1c00
#define WDOG_PROGRAM_COUNTER				0x1c74

#define OSM_CYCLE_COUNTER_USE_XO_EDGE_EN		BIT(8)

#define PLL_MODE					0x0
#define PLL_L_VAL					0x4
#define PLL_USER_CTRL					0xC
#define PLL_CONFIG_CTL_LO				0x10
#define PLL_TEST_CTL_HI					0x1C
#define PLL_STATUS					0x2C
#define PLL_LOCK_DET_MASK				BIT(16)
#define PLL_WAIT_LOCK_TIME_US				10
#define PLL_WAIT_LOCK_TIME_NS			(PLL_WAIT_LOCK_TIME_US * 1000)
#define PLL_MIN_LVAL					43
#define L_VAL(freq_data)			((freq_data) & GENMASK(7, 0))

#define CC_ZERO_BEHAV_CTRL				0x100C
#define SPM_CC_DCVS_DISABLE				0x1020
#define SPM_CC_CTRL					0x1028
#define SPM_CC_HYSTERESIS				0x101C
#define SPM_CORE_RET_MAPPING				0x1024
#define CFG_DELAY_VAL_3					0x12C

#define LLM_FREQ_VOTE_HYSTERESIS			0x102C
#define LLM_VOLT_VOTE_HYSTERESIS			0x1030
#define LLM_INTF_DCVS_DISABLE				0x1034

#define ENABLE_OVERRIDE					BIT(0)

#define ITM_CL0_DISABLE_CL1_ENABLED			0x2
#define ITM_CL0_ENABLED_CL1_DISABLE			0x1

#define APM_MX_MODE					0
#define APM_APC_MODE					BIT(1)
#define APM_MODE_SWITCH_MASK			(BVAL(4, 2, 7) | BVAL(1, 0, 3))
#define APM_MX_MODE_VAL					0
#define APM_APC_MODE_VAL				0x3

#define GPLL_SEL					0x400
#define PLL_EARLY_SEL					0x500
#define PLL_MAIN_SEL					0x300
#define RCG_UPDATE					0x3
#define RCG_UPDATE_SUCCESS				0x2
#define PLL_POST_DIV1					0x1F
#define PLL_POST_DIV2					0x11F

#define LLM_SW_OVERRIDE_REG				0x1038
#define VMIN_REDUC_ENABLE_REG				0x103C
#define VMIN_REDUC_TIMER_REG				0x1040
#define PDN_FSM_CTRL_REG				0x1070
#define CC_BOOST_TIMER_REG0				0x1074
#define CC_BOOST_TIMER_REG1				0x1078
#define CC_BOOST_TIMER_REG2				0x107C
#define CC_BOOST_EN_MASK				BIT(0)
#define PS_BOOST_EN_MASK				BIT(1)
#define DCVS_BOOST_EN_MASK				BIT(2)
#define PC_RET_EXIT_DROOP_EN_MASK			BIT(3)
#define WFX_DROOP_EN_MASK				BIT(4)
#define DCVS_DROOP_EN_MASK				BIT(5)
#define LMH_PS_EN_MASK					BIT(6)
#define IGNORE_PLL_LOCK_MASK				BIT(15)
#define SAFE_FREQ_WAIT_NS				5000
#define DEXT_DECREMENT_WAIT_NS				1000
#define DCVS_BOOST_TIMER_REG0				0x1084
#define DCVS_BOOST_TIMER_REG1				0x1088
#define DCVS_BOOST_TIMER_REG2				0x108C
#define PS_BOOST_TIMER_REG0				0x1094
#define PS_BOOST_TIMER_REG1				0x1098
#define PS_BOOST_TIMER_REG2				0x109C
#define BOOST_PROG_SYNC_DELAY_REG			0x10A0
#define DROOP_CTRL_REG					0x10A4
#define DROOP_RELEASE_TIMER_CTRL			0x10A8
#define DROOP_PROG_SYNC_DELAY_REG			0x10BC
#define DROOP_UNSTALL_TIMER_CTRL_REG			0x10AC
#define DROOP_WAIT_TO_RELEASE_TIMER_CTRL0_REG		0x10B0
#define DROOP_WAIT_TO_RELEASE_TIMER_CTRL1_REG		0x10B4
#define OSM_PLL_SW_OVERRIDE_EN				0x10C0

#define PLL_SW_OVERRIDE_DROOP_EN			BIT(0)
#define DCVS_DROOP_TIMER_CTRL				0x10B8
#define SEQ_MEM_ADDR					0x500
#define SEQ_CFG_BR_ADDR					0x170
#define MAX_INSTRUCTIONS				256
#define MAX_BR_INSTRUCTIONS				49

#define MAX_MEM_ACC_LEVELS				3
#define MAX_MEM_ACC_VAL_PER_LEVEL			3
#define MAX_MEM_ACC_VALUES			(MAX_MEM_ACC_LEVELS * \
						    MAX_MEM_ACC_VAL_PER_LEVEL)
#define MEM_ACC_APM_READ_MASK				0xff

#define TRACE_CTRL					0x1F38
#define TRACE_CTRL_EN_MASK				BIT(0)
#define TRACE_CTRL_ENABLE				1
#define TRACE_CTRL_DISABLE				0
#define TRACE_CTRL_ENABLE_WDOG_STATUS			BIT(30)
#define TRACE_CTRL_PACKET_TYPE_MASK			BVAL(2, 1, 3)
#define TRACE_CTRL_PACKET_TYPE_SHIFT			1
#define TRACE_CTRL_PERIODIC_TRACE_EN_MASK		BIT(3)
#define TRACE_CTRL_PERIODIC_TRACE_ENABLE		BIT(3)
#define PERIODIC_TRACE_TIMER_CTRL			0x1F3C
#define PERIODIC_TRACE_MIN_NS				1000
#define PERIODIC_TRACE_MAX_NS				21474836475ULL
#define PERIODIC_TRACE_DEFAULT_NS			1000000

#define PLL_DD_USER_CTL_LO_ENABLE			0x0f04c408
#define PLL_DD_USER_CTL_LO_DISABLE			0x1f04c41f
#define PLL_DD_D0_USER_CTL_LO				0x17916208
#define PLL_DD_D1_USER_CTL_LO				0x17816208

#define PWRCL_EFUSE_SHIFT				0
#define PWRCL_EFUSE_MASK				0
#define PERFCL_EFUSE_SHIFT				29
#define PERFCL_EFUSE_MASK				0x7

/* ACD registers */
#define ACD_HW_VERSION					0x0
#define ACDCR						0x4
#define ACDTD						0x8
#define ACDSSCR						0x28
#define ACD_EXTINT_CFG					0x30
#define ACD_DCVS_SW					0x34
#define ACD_GFMUX_CFG					0x3c
#define ACD_READOUT_CFG					0x48
#define ACD_AUTOXFER_CFG				0x80
#define ACD_AUTOXFER					0x84
#define ACD_AUTOXFER_CTL				0x88
#define ACD_AUTOXFER_STATUS				0x8c
#define ACD_WRITE_CTL					0x90
#define ACD_WRITE_STATUS				0x94
#define ACD_READOUT					0x98

#define ACD_MASTER_ONLY_REG_ADDR			0x80
#define ACD_WRITE_CTL_UPDATE_EN				BIT(0)
#define ACD_WRITE_CTL_SELECT_SHIFT			1
#define ACD_GFMUX_CFG_SELECT				BIT(0)
#define ACD_AUTOXFER_START_CLEAR			0
#define ACD_AUTOXFER_START_SET				BIT(0)
#define AUTO_XFER_DONE_MASK				BIT(0)
#define ACD_DCVS_SW_DCVS_IN_PRGR_SET			BIT(0)
#define ACD_DCVS_SW_DCVS_IN_PRGR_CLEAR			0
#define ACD_LOCAL_TRANSFER_TIMEOUT_NS			500

#define ACD_REG_RELATIVE_ADDR(addr)			(addr / 4)
#define ACD_REG_RELATIVE_ADDR_BITMASK(addr) \
					(1 << (ACD_REG_RELATIVE_ADDR(addr)))

#define F(f, s, h, m, n) { (f), (s), (2 * (h) - 1), (m), (n) }

static u32 seq_instr[] = {
	0xc2005000, 0x2c9e3b21, 0xc0ab2cdc, 0xc2882525, 0x359dc491,
	0x700a500b, 0x5001aefc, 0xaefd7000, 0x390938c8, 0xcb44c833,
	0xce56cd54, 0x341336e0, 0xa4baadba, 0xb480a493, 0x10004000,
	0x70005001, 0x1000500c, 0xc792c5a1, 0x501625e1, 0x3da335a2,
	0x50170006, 0x50150006, 0x1000c633, 0x1000acb3, 0xc422acb4,
	0xaefc1000, 0x700a500b, 0x70005001, 0x5010aefd, 0x5012700b,
	0xad41700c, 0x84e5adb9, 0xb3808566, 0x239b0003, 0x856484e3,
	0xb9800007, 0x2bad0003, 0xac3aa20b, 0x0003181b, 0x0003bb40,
	0xa30d239b, 0x500c181b, 0x5011500f, 0x181b3413, 0x853984b9,
	0x0003bd80, 0xa0012ba4, 0x72050803, 0x500e1000, 0x500c1000,
	0x1c011c0a, 0x3b181c06, 0x1c073b43, 0x1c061000, 0x1c073983,
	0x1c02500c, 0x10001c0a, 0x70015002, 0x81031000, 0x70025003,
	0x70035004, 0x3b441000, 0x81553985, 0x70025003, 0x50054003,
	0xa1467009, 0x0003b1c0, 0x4005238b, 0x835a1000, 0x855c84db,
	0x1000a51f, 0x84de835d, 0xa52c855c, 0x50061000, 0x39cd3a4c,
	0x3ad03a8f, 0x10004006, 0x70065007, 0xa00f2c12, 0x08034007,
	0xaefc7205, 0xaefd700d, 0xa9641000, 0x40071c1a, 0x700daefc,
	0x1000aefd, 0x70065007, 0x50101c16, 0x40075012, 0x700daefc,
	0x2411aefd, 0xa8211000, 0x0803a00f, 0x500c7005, 0x1c1591e0,
	0x500f5014, 0x10005011, 0x500c2bd4, 0x0803a00f, 0x10007205,
	0xa00fa9d1, 0x0803a821, 0xa9d07005, 0x91e0500c, 0x500f1c15,
	0x10005011, 0x1c162bce, 0x50125010, 0xa022a82a, 0x70050803,
	0x1c1591df, 0x5011500f, 0x5014500c, 0x0803a00f, 0x10007205,
	0x501391a4, 0x22172217, 0x70075008, 0xa9634008, 0x1c1a0006,
	0x70085009, 0x10004009, 0x00008ed9, 0x3e05c8dd, 0x1c033604,
	0xabaf1000, 0x856284e1, 0x0003bb80, 0x1000239f, 0x0803a037,
	0x10007205, 0x8dc61000, 0x38a71c2a, 0x1c2a8dc4, 0x100038a6,
	0x1c2a8dc5, 0x8dc73867, 0x38681c2a, 0x8c491000, 0x8d4b8cca,
	0x10001c00, 0x8ccd8c4c, 0x1c008d4e, 0x8c4f1000, 0x8d518cd0,
	0x10001c00, 0xa759a79a, 0x1000a718, 0xbf80af9b, 0x00001000,
};

static u32 seq_br_instr[] = {
	0x248, 0x20e, 0x21c, 0xf6, 0x112,
	0x11c, 0xe4, 0xea, 0xc6, 0xd6,
	0x126, 0x108, 0x184, 0x1a8, 0x1b0,
	0x134, 0x158, 0x16e, 0x14a, 0xc2,
	0x190, 0x1d2, 0x1cc, 0x1d4, 0x1e8,
	0x0, 0x1f6, 0x32, 0x66, 0xb0,
	0xa6, 0x1fc, 0x3c, 0x44, 0x5c,
	0x60, 0x204, 0x30, 0x22a, 0x234,
	0x23e, 0x0, 0x250, 0x0, 0x0, 0x9a,
	0x20c,
};

struct osm_entry {
	u16 virtual_corner;
	u16 open_loop_volt;
	u32 freq_data;
	u32 override_data;
	u32 spare_data;
	long frequency;
};

static void __iomem *virt_base;
static struct dentry *osm_debugfs_base;
static struct regulator *vdd_pwrcl;
static struct regulator *vdd_perfcl;

static const struct regmap_config osm_qcom_regmap_config = {
	.reg_bits       = 32,
	.reg_stride     = 4,
	.val_bits       = 32,
	.fast_io        = true,
};

struct clk_osm {
	struct clk_hw hw;
	struct osm_entry osm_table[OSM_TABLE_SIZE];
	struct dentry *debugfs;
	struct regulator *vdd_reg;
	struct platform_device *vdd_dev;
	void *vbases[NUM_BASES];
	unsigned long pbases[NUM_BASES];
	spinlock_t lock;

	u32 cpu_reg_mask;
	u32 num_entries;
	u32 cluster_num;
	u32 irq;
	u32 apm_crossover_vc;
	u32 apm_threshold_vc;
	u32 mem_acc_crossover_vc;
	u32 mem_acc_threshold_vc;
	u32 cycle_counter_reads;
	u32 cycle_counter_delay;
	u32 cycle_counter_factor;
	u64 total_cycle_counter;
	u32 prev_cycle_counter;
	u32 l_val_base;
	u32 apcs_itm_present;
	u32 apcs_cfg_rcgr;
	u32 apcs_cmd_rcgr;
	u32 apcs_pll_user_ctl;
	u32 apcs_mem_acc_cfg[MAX_MEM_ACC_VAL_PER_LEVEL];
	u32 apcs_mem_acc_val[MAX_MEM_ACC_VALUES];
	u32 llm_sw_overr[LLM_SW_OVERRIDE_CNT];
	u32 apm_mode_ctl;
	u32 apm_ctrl_status;
	u32 osm_clk_rate;
	u32 xo_clk_rate;
	u32 acd_td;
	u32 acd_cr;
	u32 acd_sscr;
	u32 acd_extint0_cfg;
	u32 acd_extint1_cfg;
	u32 acd_autoxfer_ctl;
	u32 acd_debugfs_addr;
	bool acd_init;
	bool secure_init;
	bool red_fsm_en;
	bool boost_fsm_en;
	bool safe_fsm_en;
	bool ps_fsm_en;
	bool droop_fsm_en;
	bool wfx_fsm_en;
	bool pc_fsm_en;

	enum clk_osm_trace_method trace_method;
	enum clk_osm_trace_packet_id trace_id;
	struct notifier_block panic_notifier;
	u32 trace_periodic_timer;
	bool trace_en;
	bool wdog_trace_en;
};

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

static inline void clk_osm_write_reg(struct clk_osm *c, u32 val, u32 offset)
{
	writel_relaxed(val, (char *)c->vbases[OSM_BASE] + offset);
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
	return readl_relaxed_no_log((char *)c->vbases[base] + VERSION_REG);
}

static inline int clk_osm_acd_mb(struct clk_osm *c)
{
	return readl_relaxed_no_log((char *)c->vbases[ACD_BASE] +
				    ACD_HW_VERSION);
}

static inline void clk_osm_acd_master_write_reg(struct clk_osm *c,
						u32 val, u32 offset)
{
	writel_relaxed(val, (char *)c->vbases[ACD_BASE] + offset);
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

	reg = readl_relaxed((char *)c->vbases[ACD_BASE]
			    + ACD_READOUT);
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

	/* Program auto-transfter mask */
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

static inline int clk_osm_count_ns(struct clk_osm *c, u64 nsec)
{
	u64 temp;

	temp = (u64)c->osm_clk_rate * nsec;
	do_div(temp, 1000000000);

	return temp;
}

static inline struct clk_osm *to_clk_osm(struct clk_hw *_hw)
{
	return container_of(_hw, struct clk_osm, hw);
}

static long clk_osm_list_rate(struct clk_hw *hw, unsigned n,
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

static int clk_osm_set_rate(struct clk_hw *hw, unsigned long rate,
				    unsigned long parent_rate)
{
	struct clk_osm *cpuclk = to_clk_osm(hw);
	int index = 0;
	unsigned long r_rate;

	r_rate = clk_osm_round_rate(hw, rate, NULL);

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

	if (cpuclk->llm_sw_overr[0]) {
		clk_osm_write_reg(cpuclk, cpuclk->llm_sw_overr[0],
							LLM_SW_OVERRIDE_REG);
		clk_osm_write_reg(cpuclk, cpuclk->llm_sw_overr[1],
							  LLM_SW_OVERRIDE_REG);
		udelay(1);
	}

	/* Choose index and send request to OSM hardware */
	clk_osm_write_reg(cpuclk, index, DCVS_PERF_STATE_DESIRED_REG);

	if (cpuclk->llm_sw_overr[0]) {
		udelay(1);
		clk_osm_write_reg(cpuclk, cpuclk->llm_sw_overr[2],
							  LLM_SW_OVERRIDE_REG);
	}

	/* Make sure the write goes through before proceeding */
	clk_osm_mb(cpuclk, OSM_BASE);

	return 0;
}

static int clk_osm_enable(struct clk_hw *hw)
{
	struct clk_osm *cpuclk = to_clk_osm(hw);

	clk_osm_write_reg(cpuclk, 1, ENABLE_REG);

	/* Make sure the write goes through before proceeding */
	clk_osm_mb(cpuclk, OSM_BASE);

	/* Wait for 5us for OSM hardware to enable */
	udelay(5);

	pr_debug("OSM clk enabled for cluster=%d\n", cpuclk->cluster_num);

	return 0;
}

static unsigned long clk_osm_recalc_rate(struct clk_hw *hw,
					unsigned long parent_rate)
{
	struct clk_osm *cpuclk = to_clk_osm(hw);
	int index = 0;

	index = clk_osm_read_reg(cpuclk, DCVS_PERF_STATE_DESIRED_REG);

	pr_debug("%s: Index %d, freq %ld\n", __func__, index,
				cpuclk->osm_table[index].frequency);

	/* Convert index to frequency.
	 * The frequency corresponding to the index requested might not
	 * be what the clock is actually running at.
	 * There are other inputs into OSM(acd, LMH, sequencer)
	 * which might decide the final rate.
	 */
	return cpuclk->osm_table[index].frequency;
}

static struct clk_ops clk_ops_cpu_osm = {
	.enable = clk_osm_enable,
	.set_rate = clk_osm_set_rate,
	.round_rate = clk_osm_round_rate,
	.list_rate = clk_osm_list_rate,
	.recalc_rate = clk_osm_recalc_rate,
};

static const struct parent_map gcc_parent_map_1[] = {
	{ P_XO, 0 },
	{ LMH_LITE_CLK_SRC, 1 },
};

static const char * const gcc_parent_names_1[] = {
	"xo",
	"hmss_gpll0_clk_src",
};

static struct freq_tbl ftbl_osm_clk_src[] = {
	F(200000000, LMH_LITE_CLK_SRC, 3, 0, 0),
	{ }
};

/* APCS_COMMON_LMH_CMD_RCGR */
static struct clk_rcg2 osm_clk_src = {
	.cmd_rcgr = 0x0012c,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = gcc_parent_map_1,
	.freq_tbl = ftbl_osm_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "osm_clk_src",
		.parent_names = gcc_parent_names_1,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_fixed_factor sys_apcsaux_clk_gcc = {
	.div = 1,
	.mult = 1,
	.hw.init = &(struct clk_init_data){
		.name = "sys_apcsaux_clk_gcc",
		.parent_names = (const char *[]){ "hmss_gpll0_clk_src" },
		.num_parents = 1,
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_init_data osm_clks_init[] = {
	[0] = {
		.name = "pwrcl_clk",
		.parent_names = (const char *[]){ "cxo_a" },
		.num_parents = 1,
		.ops = &clk_ops_cpu_osm,
	},
	[1] = {
		.name = "perfcl_clk",
		.parent_names = (const char *[]){ "cxo_a" },
		.num_parents = 1,
		.ops = &clk_ops_cpu_osm,
	},
};

static struct clk_osm pwrcl_clk = {
	.cluster_num = 0,
	.cpu_reg_mask = 0x3,
	.hw.init = &osm_clks_init[0],
};

static struct clk_osm perfcl_clk = {
	.cluster_num = 1,
	.cpu_reg_mask = 0x103,
	.hw.init = &osm_clks_init[1],
};

static struct clk_hw *osm_qcom_clk_hws[] = {
	[SYS_APCSAUX_CLK_GCC] = &sys_apcsaux_clk_gcc.hw,
	[PWRCL_CLK] = &pwrcl_clk.hw,
	[PERFCL_CLK] = &perfcl_clk.hw,
	[OSM_CLK_SRC] = &osm_clk_src.clkr.hw,
};

static void clk_osm_print_osm_table(struct clk_osm *c)
{
	int i;
	struct osm_entry *table = c->osm_table;
	u32 pll_src, pll_div, lval, core_count;

	pr_debug("Index, Frequency, VC, OLV (mv), Core Count, PLL Src, PLL Div, L-Val, ACC Level\n");
	for (i = 0; i < c->num_entries; i++) {
		pll_src = (table[i].freq_data & GENMASK(27, 26)) >> 26;
		pll_div = (table[i].freq_data & GENMASK(25, 24)) >> 24;
		lval = L_VAL(table[i].freq_data);
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
			table[i].spare_data);
	}
	pr_debug("APM threshold corner=%d, crossover corner=%d\n",
			c->apm_threshold_vc, c->apm_crossover_vc);
	pr_debug("MEM-ACC threshold corner=%d, crossover corner=%d\n",
			c->mem_acc_threshold_vc, c->mem_acc_crossover_vc);
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
		c->osm_table[j].spare_data = array[i + SPARE_DATA];
		/* Voltage corners are 0 based in the OSM LUT */
		c->osm_table[j].virtual_corner = array[i + VIRTUAL_CORNER] - 1;
		pr_debug("index=%d freq=%ld virtual_corner=%d freq_data=0x%x override_data=0x%x spare_data=0x%x\n",
			 j, c->osm_table[j].frequency,
			 c->osm_table[j].virtual_corner,
			 c->osm_table[j].freq_data,
			 c->osm_table[j].override_data,
			 c->osm_table[j].spare_data);

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

static int clk_osm_parse_dt_configs(struct platform_device *pdev)
{
	struct device_node *of = pdev->dev.of_node;
	u32 *array;
	int i, rc = 0;

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

	for (i = 0; i < LLM_SW_OVERRIDE_CNT; i++)
		of_property_read_u32_index(of, "qcom,llm-sw-overr",
					   pwrcl_clk.cluster_num *
					   LLM_SW_OVERRIDE_CNT + i,
					   &pwrcl_clk.llm_sw_overr[i]);

	for (i = 0; i < LLM_SW_OVERRIDE_CNT; i++)
		of_property_read_u32_index(of, "qcom,llm-sw-overr",
					   perfcl_clk.cluster_num *
					   LLM_SW_OVERRIDE_CNT + i,
					   &perfcl_clk.llm_sw_overr[i]);

	if (pwrcl_clk.acd_init || perfcl_clk.acd_init) {
		rc = of_property_read_u32_array(of, "qcom,acdtd-val",
						array, MAX_CLUSTER_CNT);
		if (rc) {
			dev_err(&pdev->dev, "unable to find qcom,acdtd-val property, rc=%d\n",
				rc);
			return -EINVAL;
		}

		pwrcl_clk.acd_td = array[pwrcl_clk.cluster_num];
		perfcl_clk.acd_td = array[perfcl_clk.cluster_num];

		rc = of_property_read_u32_array(of, "qcom,acdcr-val",
						array, MAX_CLUSTER_CNT);
		if (rc) {
			dev_err(&pdev->dev, "unable to find qcom,acdcr-val property, rc=%d\n",
				rc);
			return -EINVAL;
		}

		pwrcl_clk.acd_cr = array[pwrcl_clk.cluster_num];
		perfcl_clk.acd_cr = array[perfcl_clk.cluster_num];

		rc = of_property_read_u32_array(of, "qcom,acdsscr-val",
						array, MAX_CLUSTER_CNT);
		if (rc) {
			dev_err(&pdev->dev, "unable to find qcom,acdsscr-val property, rc=%d\n",
				rc);
			return -EINVAL;
		}

		pwrcl_clk.acd_sscr = array[pwrcl_clk.cluster_num];
		perfcl_clk.acd_sscr = array[perfcl_clk.cluster_num];

		rc = of_property_read_u32_array(of, "qcom,acdextint0-val",
						array, MAX_CLUSTER_CNT);
		if (rc) {
			dev_err(&pdev->dev, "unable to find qcom,acdextint0-val property, rc=%d\n",
				rc);
			return -EINVAL;
		}

		pwrcl_clk.acd_extint0_cfg = array[pwrcl_clk.cluster_num];
		perfcl_clk.acd_extint0_cfg = array[perfcl_clk.cluster_num];

		rc = of_property_read_u32_array(of, "qcom,acdextint1-val",
						array, MAX_CLUSTER_CNT);
		if (rc) {
			dev_err(&pdev->dev, "unable to find qcom,acdextint1-val property, rc=%d\n",
				rc);
			return -EINVAL;
		}

		pwrcl_clk.acd_extint1_cfg = array[pwrcl_clk.cluster_num];
		perfcl_clk.acd_extint1_cfg = array[perfcl_clk.cluster_num];

		rc = of_property_read_u32_array(of, "qcom,acdautoxfer-val",
						array, MAX_CLUSTER_CNT);
		if (rc) {
			dev_err(&pdev->dev, "unable to find qcom,acdautoxfer-val property, rc=%d\n",
				rc);
			return -EINVAL;
		}

		pwrcl_clk.acd_autoxfer_ctl = array[pwrcl_clk.cluster_num];
		perfcl_clk.acd_autoxfer_ctl = array[perfcl_clk.cluster_num];
	}

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

	perfcl_clk.secure_init = pwrcl_clk.secure_init =
		of_property_read_bool(pdev->dev.of_node, "qcom,osm-no-tz");

	if (!pwrcl_clk.secure_init)
		return rc;

	rc = of_property_read_u32_array(of, "qcom,pwrcl-apcs-mem-acc-cfg",
					pwrcl_clk.apcs_mem_acc_cfg,
					MAX_MEM_ACC_VAL_PER_LEVEL);
	if (rc) {
		dev_err(&pdev->dev, "unable to find qcom,pwrcl-apcs-mem-acc-cfg property, rc=%d\n",
			rc);
		return -EINVAL;
	}

	of_property_read_u32_array(of, "qcom,perfcl-apcs-mem-acc-cfg",
				   perfcl_clk.apcs_mem_acc_cfg,
				   MAX_MEM_ACC_VAL_PER_LEVEL);
	if (rc) {
		dev_err(&pdev->dev, "unable to find qcom,perfcl-apcs-mem-acc-cfg property, rc=%d\n",
			rc);
		return -EINVAL;
	}

	rc = of_property_read_u32_array(of, "qcom,pwrcl-apcs-mem-acc-val",
					pwrcl_clk.apcs_mem_acc_val,
					MAX_MEM_ACC_VALUES);
	if (rc) {
		dev_err(&pdev->dev, "unable to find qcom,pwrcl-apcs-mem-acc-val property, rc=%d\n",
			rc);
		return -EINVAL;
	}

	rc = of_property_read_u32_array(of, "qcom,perfcl-apcs-mem-acc-val",
					perfcl_clk.apcs_mem_acc_val,
					MAX_MEM_ACC_VALUES);
	if (rc) {
		dev_err(&pdev->dev, "unable to find qcom,perfcl-apcs-mem-acc-val property, rc=%d\n",
			rc);
		return -EINVAL;
	}

	return rc;
}

static int clk_osm_resources_init(struct platform_device *pdev)
{
	struct device_node *node;
	struct resource *res;
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

	perfcl_clk.pbases[OSM_BASE] = pwrcl_clk.pbases[OSM_BASE] +
				perfcl_clk.cluster_num * OSM_CORE_TABLE_SIZE;
	perfcl_clk.vbases[OSM_BASE] = pwrcl_clk.vbases[OSM_BASE]  +
				perfcl_clk.cluster_num * OSM_CORE_TABLE_SIZE;

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

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "apcs_common");
	if (!res) {
		dev_err(&pdev->dev, "Failed to get apcs common base\n");
		return -EINVAL;
	}

	virt_base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (!virt_base) {
		dev_err(&pdev->dev, "Failed to map apcs common registers\n");
		return -ENOMEM;
	}

	osm_clk_src.clkr.regmap = devm_regmap_init_mmio(&pdev->dev, virt_base,
						&osm_qcom_regmap_config);
	if (IS_ERR(osm_clk_src.clkr.regmap)) {
		dev_err(&pdev->dev, "Couldn't get regmap OSM clock\n");
		return PTR_ERR(osm_clk_src.clkr.regmap);
	}

	/* efuse speed bin fuses are optional */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
					   "pwrcl_efuse");
	if (res) {
		pbase = (unsigned long)res->start;
		vbase = devm_ioremap(&pdev->dev, res->start,
				     resource_size(res));
		if (!vbase) {
			dev_err(&pdev->dev, "Unable to map in pwrcl_efuse base\n");
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
			dev_err(&pdev->dev, "Unable to map in perfcl_efuse base\n");
			return -ENOMEM;
		}
		perfcl_clk.pbases[EFUSE_BASE] = pbase;
		perfcl_clk.vbases[EFUSE_BASE] = vbase;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
					   "pwrcl_acd");
	if (res) {
		pbase = (unsigned long)res->start;
		vbase = devm_ioremap(&pdev->dev, res->start,
				     resource_size(res));
		if (!vbase) {
			dev_err(&pdev->dev, "Unable to map in pwrcl_acd base\n");
			return -ENOMEM;
		}
		pwrcl_clk.pbases[ACD_BASE] = pbase;
		pwrcl_clk.vbases[ACD_BASE] = vbase;
		pwrcl_clk.acd_init = true;
	} else {
		pwrcl_clk.acd_init = false;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
					   "perfcl_acd");
	if (res) {
		pbase = (unsigned long)res->start;
		vbase = devm_ioremap(&pdev->dev, res->start,
				     resource_size(res));
		if (!vbase) {
			dev_err(&pdev->dev, "Unable to map in perfcl_acd base\n");
			return -ENOMEM;
		}
		perfcl_clk.pbases[ACD_BASE] = pbase;
		perfcl_clk.vbases[ACD_BASE] = vbase;
		perfcl_clk.acd_init = true;
	} else {
		perfcl_clk.acd_init = false;
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
	clk_osm_mb(c, PLL_BASE);

	udelay(PLL_WAIT_LOCK_TIME_US);

	writel_relaxed(0x6, c->vbases[PLL_BASE] + PLL_MODE);

	/* Ensure write completes before delaying */
	clk_osm_mb(c, PLL_BASE);

	usleep_range(50, 75);

	writel_relaxed(0x7, c->vbases[PLL_BASE] + PLL_MODE);
}

static int clk_osm_setup_hw_table(struct clk_osm *c)
{
	struct osm_entry *entry = c->osm_table;
	int i;
	u32 freq_val, volt_val, override_val, spare_val;
	u32 table_entry_offset, last_spare, last_virtual_corner = 0;

	for (i = 0; i < OSM_TABLE_SIZE; i++) {
		if (i < c->num_entries) {
			freq_val = entry[i].freq_data;
			volt_val = BVAL(21, 16, entry[i].virtual_corner)
				| BVAL(11, 0, entry[i].open_loop_volt);
			override_val = entry[i].override_data;
			spare_val = entry[i].spare_data;

			if (last_virtual_corner && last_virtual_corner ==
			    entry[i].virtual_corner && last_spare !=
			    entry[i].spare_data) {
				pr_err("invalid LUT entry at row=%d virtual_corner=%d, spare_data=%d\n",
				       i, entry[i].virtual_corner,
				       entry[i].spare_data);
				return -EINVAL;
			}
			last_virtual_corner = entry[i].virtual_corner;
			last_spare = entry[i].spare_data;
		}

		table_entry_offset = i * OSM_REG_SIZE;
		clk_osm_write_reg(c, i, INDEX_REG + table_entry_offset);
		clk_osm_write_reg(c, freq_val, FREQ_REG + table_entry_offset);
		clk_osm_write_reg(c, volt_val, VOLT_REG + table_entry_offset);
		clk_osm_write_reg(c, override_val, OVERRIDE_REG +
				  table_entry_offset);
		clk_osm_write_reg(c, spare_val, SPARE_REG +
				  table_entry_offset);
	}

	/* Make sure all writes go through */
	clk_osm_mb(c, OSM_BASE);

	return 0;
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

static int clk_osm_resolve_crossover_corners(struct clk_osm *c,
				     struct platform_device *pdev,
				     const char *mem_acc_prop)
{
	struct regulator *regulator = c->vdd_reg;
	int count, vc, i, apm_threshold;
	int mem_acc_threshold = 0;
	int rc = 0;
	u32 corner_volt;

	rc = of_property_read_u32(pdev->dev.of_node,
				  "qcom,apm-threshold-voltage",
				  &apm_threshold);
	if (rc) {
		pr_info("qcom,apm-threshold-voltage property not specified\n");
		return rc;
	}

	if (mem_acc_prop)
		of_property_read_u32(pdev->dev.of_node, mem_acc_prop,
						 &mem_acc_threshold);

	/* Determine crossover virtual corner */
	count = regulator_count_voltages(regulator);
	if (count < 0) {
		pr_err("Failed to get the number of virtual corners supported\n");
		return count;
	}

	/*
	 * CPRh corners (in hardware) are ordered:
	 * 0 - n-1		- for n functional corners
	 * APM crossover	- required for OSM
	 * [MEM ACC Crossover]	- optional
	 *
	 * 'count' corresponds to the total number of corners including n
	 * functional corners, the APM crossover corner, and potentially the
	 * MEM ACC cross over corner.
	 */
	if (mem_acc_threshold) {
		c->apm_crossover_vc = count - 2;
		c->mem_acc_crossover_vc = count - 1;
	} else {
		c->apm_crossover_vc = count - 1;
	}

	/* Determine APM threshold virtual corner */
	for (i = 0; i < OSM_TABLE_SIZE; i++) {
		vc = c->osm_table[i].virtual_corner + 1;
		corner_volt = regulator_list_corner_voltage(regulator, vc);

		if (corner_volt >= apm_threshold) {
			c->apm_threshold_vc = c->osm_table[i].virtual_corner;
			break;
		}
	}

	/* Determine MEM ACC threshold virtual corner */
	if (mem_acc_threshold) {
		for (i = 0; i < OSM_TABLE_SIZE; i++) {
			vc = c->osm_table[i].virtual_corner + 1;
			corner_volt =
				regulator_list_corner_voltage(regulator, vc);

			if (corner_volt >= mem_acc_threshold) {
				c->mem_acc_threshold_vc
					= c->osm_table[i].virtual_corner;
				break;
			}
		}
	}

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
		val = clk_osm_read_reg(&pwrcl_clk, SPM_CC_HYSTERESIS)
			| BVAL(31, 16, clk_osm_count_ns(&pwrcl_clk,
					array[pwrcl_clk.cluster_num]));
		clk_osm_write_reg(&pwrcl_clk, val, SPM_CC_HYSTERESIS);
		val = clk_osm_read_reg(&perfcl_clk, SPM_CC_HYSTERESIS)
			| BVAL(31, 16, clk_osm_count_ns(&perfcl_clk,
					array[perfcl_clk.cluster_num]));
		clk_osm_write_reg(&perfcl_clk, val, SPM_CC_HYSTERESIS);
	}

	rc = of_property_read_u32_array(of, "qcom,down-timer",
					array, MAX_CLUSTER_CNT);
	if (rc) {
		dev_dbg(&pdev->dev, "No down timer value, rc=%d\n", rc);
	} else {
		val = clk_osm_read_reg(&pwrcl_clk, SPM_CC_HYSTERESIS)
			| BVAL(15, 0, clk_osm_count_ns(&pwrcl_clk,
				       array[pwrcl_clk.cluster_num]));
		clk_osm_write_reg(&pwrcl_clk, val, SPM_CC_HYSTERESIS);
		val = clk_osm_read_reg(&perfcl_clk, SPM_CC_HYSTERESIS)
			| BVAL(15, 0, clk_osm_count_ns(&perfcl_clk,
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
	clk_osm_mb(&perfcl_clk, OSM_BASE);

	rc = of_property_read_bool(pdev->dev.of_node, "qcom,set-ret-inactive");
	if (rc) {
		dev_dbg(&pdev->dev, "Treat cores in retention as active\n");
		val = 0;
	} else {
		dev_dbg(&pdev->dev, "Treat cores in retention as inactive\n");
		val = 1;
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
	clk_osm_mb(&perfcl_clk, OSM_BASE);

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
			| BVAL(31, 16, clk_osm_count_ns(&pwrcl_clk,
						array[pwrcl_clk.cluster_num]));
		clk_osm_write_reg(&pwrcl_clk, val, LLM_FREQ_VOTE_HYSTERESIS);
		val = clk_osm_read_reg(&perfcl_clk, LLM_FREQ_VOTE_HYSTERESIS)
			| BVAL(31, 16, clk_osm_count_ns(&perfcl_clk,
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
			| BVAL(15, 0, clk_osm_count_ns(&pwrcl_clk,
					       array[pwrcl_clk.cluster_num]));
		clk_osm_write_reg(&pwrcl_clk, val, LLM_FREQ_VOTE_HYSTERESIS);
		val = clk_osm_read_reg(&perfcl_clk, LLM_FREQ_VOTE_HYSTERESIS)
			| BVAL(15, 0, clk_osm_count_ns(&perfcl_clk,
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
	clk_osm_mb(&perfcl_clk, OSM_BASE);

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
			| BVAL(31, 16, clk_osm_count_ns(&pwrcl_clk,
						array[pwrcl_clk.cluster_num]));
		clk_osm_write_reg(&pwrcl_clk, val, LLM_VOLT_VOTE_HYSTERESIS);
		val = clk_osm_read_reg(&perfcl_clk, LLM_VOLT_VOTE_HYSTERESIS)
			| BVAL(31, 16, clk_osm_count_ns(&perfcl_clk,
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
			| BVAL(15, 0, clk_osm_count_ns(&pwrcl_clk,
					       array[pwrcl_clk.cluster_num]));
		clk_osm_write_reg(&pwrcl_clk, val, LLM_VOLT_VOTE_HYSTERESIS);
		val = clk_osm_read_reg(&perfcl_clk, LLM_VOLT_VOTE_HYSTERESIS)
			| BVAL(15, 0, clk_osm_count_ns(&perfcl_clk,
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
	clk_osm_write_reg(&pwrcl_clk, regval, LLM_INTF_DCVS_DISABLE);
	regval = val | clk_osm_read_reg(&perfcl_clk, LLM_INTF_DCVS_DISABLE);
	clk_osm_write_reg(&perfcl_clk, regval, LLM_INTF_DCVS_DISABLE);

	/* Wait for the writes to complete */
	clk_osm_mb(&perfcl_clk, OSM_BASE);

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

	/* Program address of controller status register */
	clk_osm_write_reg(c, c->apm_ctrl_status, SEQ_REG(3));

	/* Program mode value to switch APM from VDD_APCC to VDD_MX */
	clk_osm_write_reg(c, APM_MX_MODE, SEQ_REG(77));

	/* Program value used to determine current APM power supply is VDD_MX */
	clk_osm_write_reg(c, APM_MX_MODE_VAL, SEQ_REG(78));

	/* Program mask used to determine status of APM power supply switch */
	clk_osm_write_reg(c, APM_MODE_SWITCH_MASK, SEQ_REG(79));

	/* Program mode value to switch APM from VDD_MX to VDD_APCC */
	clk_osm_write_reg(c, APM_APC_MODE, SEQ_REG(80));

	/*
	 * Program value used to determine current APM power supply
	 * is VDD_APCC
	 */
	clk_osm_write_reg(c, APM_APC_MODE_VAL, SEQ_REG(81));
}

static void clk_osm_program_mem_acc_regs(struct clk_osm *c)
{
	struct osm_entry *table = c->osm_table;
	int i, curr_level, j = 0;
	int mem_acc_level_map[MAX_MEM_ACC_LEVELS] = {0, 0, 0};
	int threshold_vc[4];

	curr_level = c->osm_table[0].spare_data;
	for (i = 0; i < c->num_entries; i++) {
		if (curr_level == MAX_MEM_ACC_LEVELS)
			break;

		if (c->osm_table[i].spare_data != curr_level) {
			mem_acc_level_map[j++] =
				c->osm_table[i].virtual_corner - 1;
			curr_level = c->osm_table[i].spare_data;
		}
	}

	if (c->secure_init) {
		clk_osm_write_reg(c, MEM_ACC_SEQ_CONST(1), SEQ_REG(51));
		clk_osm_write_reg(c, MEM_ACC_SEQ_CONST(2), SEQ_REG(52));
		clk_osm_write_reg(c, MEM_ACC_SEQ_CONST(3), SEQ_REG(53));
		clk_osm_write_reg(c, MEM_ACC_SEQ_CONST(4), SEQ_REG(54));
		clk_osm_write_reg(c, MEM_ACC_APM_READ_MASK, SEQ_REG(59));
		clk_osm_write_reg(c, mem_acc_level_map[0], SEQ_REG(55));
		clk_osm_write_reg(c, mem_acc_level_map[0] + 1, SEQ_REG(56));
		clk_osm_write_reg(c, mem_acc_level_map[1], SEQ_REG(57));
		clk_osm_write_reg(c, mem_acc_level_map[1] + 1, SEQ_REG(58));
		clk_osm_write_reg(c, c->pbases[OSM_BASE] + SEQ_REG(28),
				  SEQ_REG(49));

		for (i = 0; i < MAX_MEM_ACC_VALUES; i++)
			clk_osm_write_reg(c, c->apcs_mem_acc_val[i],
					  MEM_ACC_SEQ_REG_VAL_START(i));

		for (i = 0; i < MAX_MEM_ACC_VAL_PER_LEVEL; i++)
			clk_osm_write_reg(c, c->apcs_mem_acc_cfg[i],
					  MEM_ACC_SEQ_REG_CFG_START(i));
	} else {
		if (c->mem_acc_crossover_vc)
			scm_io_write(c->pbases[OSM_BASE] + SEQ_REG(88),
					c->mem_acc_crossover_vc);

		threshold_vc[0] = mem_acc_level_map[0];
		threshold_vc[1] = mem_acc_level_map[0] + 1;
		threshold_vc[2] = mem_acc_level_map[1];
		threshold_vc[3] = mem_acc_level_map[1] + 1;

		/*
		 * Use dynamic MEM ACC threshold voltage based value for the
		 * highest MEM ACC threshold if it is specified instead of the
		 * fixed mapping in the LUT.
		 */
		if (c->mem_acc_threshold_vc) {
			threshold_vc[2] = c->mem_acc_threshold_vc - 1;
			threshold_vc[3] = c->mem_acc_threshold_vc;
			if (threshold_vc[1] >= threshold_vc[2])
				threshold_vc[1] = threshold_vc[2] - 1;
			if (threshold_vc[0] >= threshold_vc[1])
				threshold_vc[0] = threshold_vc[1] - 1;
		}

		scm_io_write(c->pbases[OSM_BASE] + SEQ_REG(55),
						threshold_vc[0]);
		scm_io_write(c->pbases[OSM_BASE] + SEQ_REG(56),
						threshold_vc[1]);
		scm_io_write(c->pbases[OSM_BASE] + SEQ_REG(57),
						threshold_vc[2]);
		scm_io_write(c->pbases[OSM_BASE] + SEQ_REG(58),
						threshold_vc[3]);
		/* SEQ_REG(49) = SEQ_REG(28) init by TZ */
	}

	/*
	 * Program L_VAL corresponding to the first virtual
	 * corner with MEM ACC level 3.
	 */
	if (c->mem_acc_threshold_vc)
		for (i = 0; i < c->num_entries; i++)
			if (c->mem_acc_threshold_vc == table[i].virtual_corner)
				scm_io_write(c->pbases[OSM_BASE] + SEQ_REG(32),
						L_VAL(table[i].freq_data));
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
	val |= BVAL(5, 1, ratio - 1) | OSM_CYCLE_COUNTER_USE_XO_EDGE_EN;

	clk_osm_write_reg(c, val, OSM_CYCLE_COUNTER_CTRL_REG);

	c->total_cycle_counter = 0;
	c->prev_cycle_counter = 0;

	pr_debug("OSM to XO clock ratio: %d\n", ratio);
}

static void clk_osm_setup_fsms(struct clk_osm *c)
{
	u32 val;

	/* Reduction FSM */
	if (c->red_fsm_en) {
		val = clk_osm_read_reg(c, VMIN_REDUC_ENABLE_REG) | BIT(0);
		clk_osm_write_reg(c, val, VMIN_REDUC_ENABLE_REG);
		clk_osm_write_reg(c, BVAL(15, 0, clk_osm_count_ns(c, 10000)),
				  VMIN_REDUC_TIMER_REG);
	}

	/* Boost FSM */
	if (c->boost_fsm_en) {
		val = clk_osm_read_reg(c, PDN_FSM_CTRL_REG);
		clk_osm_write_reg(c, val | CC_BOOST_EN_MASK, PDN_FSM_CTRL_REG);

		val = clk_osm_read_reg(c, CC_BOOST_TIMER_REG0);
		val |= BVAL(15, 0, clk_osm_count_ns(c, PLL_WAIT_LOCK_TIME_NS));
		val |= BVAL(31, 16, clk_osm_count_ns(c, SAFE_FREQ_WAIT_NS));
		clk_osm_write_reg(c, val, CC_BOOST_TIMER_REG0);

		val = clk_osm_read_reg(c, CC_BOOST_TIMER_REG1);
		val |= BVAL(15, 0, clk_osm_count_ns(c, PLL_WAIT_LOCK_TIME_NS));
		val |= BVAL(31, 16, clk_osm_count_ns(c, PLL_WAIT_LOCK_TIME_NS));
		clk_osm_write_reg(c, val, CC_BOOST_TIMER_REG1);

		val = clk_osm_read_reg(c, CC_BOOST_TIMER_REG2);
		val |= BVAL(15, 0, clk_osm_count_ns(c, DEXT_DECREMENT_WAIT_NS));
		clk_osm_write_reg(c, val, CC_BOOST_TIMER_REG2);
	}

	/* Safe Freq FSM */
	if (c->safe_fsm_en) {
		val = clk_osm_read_reg(c, PDN_FSM_CTRL_REG);
		clk_osm_write_reg(c, val | DCVS_BOOST_EN_MASK,
				  PDN_FSM_CTRL_REG);

		val = clk_osm_read_reg(c, DCVS_BOOST_TIMER_REG0);
		val |= BVAL(15, 0, clk_osm_count_ns(c, PLL_WAIT_LOCK_TIME_NS));
		val |= BVAL(31, 16, clk_osm_count_ns(c, SAFE_FREQ_WAIT_NS));
		clk_osm_write_reg(c, val, DCVS_BOOST_TIMER_REG0);

		val = clk_osm_read_reg(c, DCVS_BOOST_TIMER_REG1);
		val |= BVAL(15, 0, clk_osm_count_ns(c, PLL_WAIT_LOCK_TIME_NS));
		val |= BVAL(31, 16, clk_osm_count_ns(c, PLL_WAIT_LOCK_TIME_NS));
		clk_osm_write_reg(c, val, DCVS_BOOST_TIMER_REG1);

		val = clk_osm_read_reg(c, DCVS_BOOST_TIMER_REG2);
		val |= BVAL(15, 0, clk_osm_count_ns(c, DEXT_DECREMENT_WAIT_NS));
		clk_osm_write_reg(c, val, DCVS_BOOST_TIMER_REG2);

	}

	/* PS FSM */
	if (c->ps_fsm_en) {
		val = clk_osm_read_reg(c, PDN_FSM_CTRL_REG);
		clk_osm_write_reg(c, val | PS_BOOST_EN_MASK, PDN_FSM_CTRL_REG);

		val = clk_osm_read_reg(c, PS_BOOST_TIMER_REG0);
		val |= BVAL(15, 0, clk_osm_count_ns(c, PLL_WAIT_LOCK_TIME_NS));
		val |= BVAL(31, 16, clk_osm_count_ns(c, SAFE_FREQ_WAIT_NS));
		clk_osm_write_reg(c, val, PS_BOOST_TIMER_REG0);

		val = clk_osm_read_reg(c, PS_BOOST_TIMER_REG1);
		val |= BVAL(15, 0, clk_osm_count_ns(c, PLL_WAIT_LOCK_TIME_NS));
		val |= BVAL(31, 16, clk_osm_count_ns(c, PLL_WAIT_LOCK_TIME_NS));
		clk_osm_write_reg(c, val, PS_BOOST_TIMER_REG1);

		val = clk_osm_read_reg(c, PS_BOOST_TIMER_REG2);
		val |= BVAL(15, 0, clk_osm_count_ns(c, DEXT_DECREMENT_WAIT_NS));
		clk_osm_write_reg(c, val, PS_BOOST_TIMER_REG2);
	}

	/* PLL signal timing control */
	if (c->boost_fsm_en || c->safe_fsm_en || c->ps_fsm_en)
		clk_osm_write_reg(c, 0x5, BOOST_PROG_SYNC_DELAY_REG);

	/* Droop FSM */
	if (c->wfx_fsm_en) {
		/* WFx FSM */
		val = clk_osm_read_reg(c, PDN_FSM_CTRL_REG);
		clk_osm_write_reg(c, val | WFX_DROOP_EN_MASK, PDN_FSM_CTRL_REG);

		val = clk_osm_read_reg(c, DROOP_UNSTALL_TIMER_CTRL_REG);
		val |= BVAL(31, 16, clk_osm_count_ns(c, 500));
		clk_osm_write_reg(c, val, DROOP_UNSTALL_TIMER_CTRL_REG);

		val = clk_osm_read_reg(c,
			       DROOP_WAIT_TO_RELEASE_TIMER_CTRL0_REG);
		val |= BVAL(31, 16, clk_osm_count_ns(c, 250));
		clk_osm_write_reg(c, val,
				DROOP_WAIT_TO_RELEASE_TIMER_CTRL0_REG);
	}

	/* PC/RET FSM */
	if (c->pc_fsm_en) {
		val = clk_osm_read_reg(c, PDN_FSM_CTRL_REG);
		clk_osm_write_reg(c, val | PC_RET_EXIT_DROOP_EN_MASK,
				  PDN_FSM_CTRL_REG);

		val = clk_osm_read_reg(c, DROOP_UNSTALL_TIMER_CTRL_REG);
		val |= BVAL(15, 0, clk_osm_count_ns(c, 500));
		clk_osm_write_reg(c, val, DROOP_UNSTALL_TIMER_CTRL_REG);

		val = clk_osm_read_reg(c,
				DROOP_WAIT_TO_RELEASE_TIMER_CTRL0_REG);
		val |= BVAL(15, 0, clk_osm_count_ns(c, 250));
		clk_osm_write_reg(c, val,
				DROOP_WAIT_TO_RELEASE_TIMER_CTRL0_REG);
	}

	/* DCVS droop FSM - only if RCGwRC is not used for di/dt control */
	if (c->droop_fsm_en) {
		val = clk_osm_read_reg(c, PDN_FSM_CTRL_REG);
		clk_osm_write_reg(c, val | DCVS_DROOP_EN_MASK,
				  PDN_FSM_CTRL_REG);
	}

	if (c->wfx_fsm_en || c->ps_fsm_en || c->droop_fsm_en) {
		clk_osm_write_reg(c, 0x1, DROOP_PROG_SYNC_DELAY_REG);
		clk_osm_write_reg(c, clk_osm_count_ns(c, 5),
				  DROOP_RELEASE_TIMER_CTRL);
		clk_osm_write_reg(c, clk_osm_count_ns(c, 500),
				  DCVS_DROOP_TIMER_CTRL);
		val = clk_osm_read_reg(c, DROOP_CTRL_REG);
		val |= BIT(31) | BVAL(22, 16, 0x2) |
			BVAL(6, 0, 0x8);
		clk_osm_write_reg(c, val, DROOP_CTRL_REG);
	}

	/* Enable the PLL Droop Override */
	val = clk_osm_read_reg(c, OSM_PLL_SW_OVERRIDE_EN);
	val |= PLL_SW_OVERRIDE_DROOP_EN;
	clk_osm_write_reg(c, val, OSM_PLL_SW_OVERRIDE_EN);
}

static void clk_osm_do_additional_setup(struct clk_osm *c,
					struct platform_device *pdev)
{
	if (!c->secure_init)
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
	clk_osm_write_reg(c, c->apcs_cmd_rcgr, SEQ_REG(33));
	clk_osm_write_reg(c, RCG_UPDATE, SEQ_REG(34));
	clk_osm_write_reg(c, GPLL_SEL, SEQ_REG(17));
	clk_osm_write_reg(c, PLL_EARLY_SEL, SEQ_REG(82));
	clk_osm_write_reg(c, PLL_MAIN_SEL, SEQ_REG(83));
	clk_osm_write_reg(c, RCG_UPDATE_SUCCESS, SEQ_REG(84));
	clk_osm_write_reg(c, RCG_UPDATE, SEQ_REG(85));

	/* ITM to OSM handoff */
	clk_osm_setup_itm_to_osm_handoff();

	pr_debug("seq_size: %zu, seqbr_size: %zu\n", ARRAY_SIZE(seq_instr),
						ARRAY_SIZE(seq_br_instr));
	clk_osm_setup_sequencer(&pwrcl_clk);
	clk_osm_setup_sequencer(&perfcl_clk);
}

static void clk_osm_apm_vc_setup(struct clk_osm *c)
{
	/*
	 * APM crossover virtual corner corresponds to switching
	 * voltage during APM transition. APM threshold virtual
	 * corner is the first corner which requires switch
	 * sequence of APM from MX to APC.
	 */
	if (c->secure_init) {
		clk_osm_write_reg(c, c->apm_threshold_vc, SEQ_REG(1));
		clk_osm_write_reg(c, c->apm_crossover_vc, SEQ_REG(72));
		clk_osm_write_reg(c, c->pbases[OSM_BASE] + SEQ_REG(1),
							  SEQ_REG(8));
		clk_osm_write_reg(c, c->apm_threshold_vc, SEQ_REG(15));
		clk_osm_write_reg(c, c->apm_threshold_vc != 0 ?
					  c->apm_threshold_vc - 1 : 0xff,
					  SEQ_REG(31));
		clk_osm_write_reg(c, 0x3b | c->apm_threshold_vc << 6,
							  SEQ_REG(73));
		clk_osm_write_reg(c, 0x39 | c->apm_threshold_vc << 6,
							  SEQ_REG(76));

		/* Ensure writes complete before returning */
		clk_osm_mb(c, OSM_BASE);
	} else {
		if (c->apm_threshold_vc)
			clk_osm_write_reg(c, c->apm_threshold_vc,
						  SEQ_REG1_OFFSET);
		scm_io_write(c->pbases[OSM_BASE] + SEQ_REG(72),
					     c->apm_crossover_vc);
		scm_io_write(c->pbases[OSM_BASE] + SEQ_REG(15),
					     c->apm_threshold_vc);
		scm_io_write(c->pbases[OSM_BASE] + SEQ_REG(31),
				     c->apm_threshold_vc != 0 ?
				     c->apm_threshold_vc - 1 : 0xff);
		scm_io_write(c->pbases[OSM_BASE] + SEQ_REG(76),
				     0x39 | c->apm_threshold_vc << 6);
	}
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
		cpu_clk_map[cpu] = pwrcl_clk.hw.clk;
		return pwrcl_clk.hw.clk;
	}
	if ((hwid | perfcl_clk.cpu_reg_mask) == perfcl_clk.cpu_reg_mask) {
		cpu_clk_map[cpu] = perfcl_clk.hw.clk;
		return perfcl_clk.hw.clk;
	}

fail:
	return NULL;
}

static u64 clk_osm_get_cpu_cycle_counter(int cpu)
{
	struct clk_osm *c;
	u32 val;
	unsigned long flags;

	if (logical_cpu_to_clk(cpu) == pwrcl_clk.hw.clk)
		c = &pwrcl_clk;
	else if (logical_cpu_to_clk(cpu) == perfcl_clk.hw.clk)
		c = &perfcl_clk;
	else {
		pr_err("no clock device for CPU=%d\n", cpu);
		return 0;
	}

	spin_lock_irqsave(&c->lock, flags);
	val = clk_osm_read_reg_no_log(c, OSM_CYCLE_COUNTER_STATUS_REG);

	if (val < c->prev_cycle_counter) {
		/* Handle counter overflow */
		c->total_cycle_counter += UINT_MAX -
			c->prev_cycle_counter + val;
		c->prev_cycle_counter = val;
	} else {
		c->total_cycle_counter += val - c->prev_cycle_counter;
		c->prev_cycle_counter = val;
	}
	spin_unlock_irqrestore(&c->lock, flags);

	return c->total_cycle_counter;
}

static void populate_opp_table(struct platform_device *pdev)
{
	int cpu;

	for_each_possible_cpu(cpu) {
		if (logical_cpu_to_clk(cpu) == pwrcl_clk.hw.clk) {
			WARN(add_opp(&pwrcl_clk, get_cpu_device(cpu)),
			     "Failed to add OPP levels for power cluster\n");
		}
		if (logical_cpu_to_clk(cpu) == perfcl_clk.hw.clk) {
			WARN(add_opp(&perfcl_clk, get_cpu_device(cpu)),
			     "Failed to add OPP levels for perf cluster\n");
		}
	}
}

static int debugfs_get_trace_enable(void *data, u64 *val)
{
	struct clk_osm *c = data;

	*val = c->trace_en;
	return 0;
}

static int debugfs_set_trace_enable(void *data, u64 val)
{
	struct clk_osm *c = data;

	clk_osm_masked_write_reg(c, val ? TRACE_CTRL_ENABLE :
				 TRACE_CTRL_DISABLE,
				 TRACE_CTRL, TRACE_CTRL_EN_MASK);
	c->trace_en = val ? true : false;
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(debugfs_trace_enable_fops,
			debugfs_get_trace_enable,
			debugfs_set_trace_enable,
			"%llu\n");

static int debugfs_get_wdog_trace(void *data, u64 *val)
{
	struct clk_osm *c = data;

	*val = c->wdog_trace_en;
	return 0;
}

static int debugfs_set_wdog_trace(void *data, u64 val)
{
	struct clk_osm *c = data;
	int regval;

	regval = clk_osm_read_reg(c, TRACE_CTRL);
	regval = val ? regval | TRACE_CTRL_ENABLE_WDOG_STATUS :
			regval & ~TRACE_CTRL_ENABLE_WDOG_STATUS;
	clk_osm_write_reg(c, regval, TRACE_CTRL);
	c->wdog_trace_en = val ? true : false;

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(debugfs_trace_wdog_enable_fops,
			debugfs_get_wdog_trace,
			debugfs_set_wdog_trace,
			"%llu\n");

#define MAX_DEBUG_BUF_LEN 15

static DEFINE_MUTEX(debug_buf_mutex);
static char debug_buf[MAX_DEBUG_BUF_LEN];

static ssize_t debugfs_trace_method_set(struct file *file,
					const char __user *buf,
					size_t count, loff_t *ppos)
{
	struct clk_osm *c = file->private_data;
	u32 val;

	if (IS_ERR(file) || file == NULL) {
		pr_err("input error %ld\n", PTR_ERR(file));
		return -EINVAL;
	}

	if (!c) {
		pr_err("invalid clk_osm handle\n");
		return -EINVAL;
	}

	if (count < MAX_DEBUG_BUF_LEN) {
		mutex_lock(&debug_buf_mutex);

		if (copy_from_user(debug_buf, (void __user *) buf, count)) {
			mutex_unlock(&debug_buf_mutex);
			return -EFAULT;
		}
		debug_buf[count] = '\0';
		mutex_unlock(&debug_buf_mutex);

		/* check that user entered a supported packet type */
		if (strcmp(debug_buf, "periodic\n") == 0) {
			clk_osm_write_reg(c, clk_osm_count_ns(c,
					      PERIODIC_TRACE_DEFAULT_NS),
					  PERIODIC_TRACE_TIMER_CTRL);
			clk_osm_masked_write_reg(c,
				 TRACE_CTRL_PERIODIC_TRACE_ENABLE,
				 TRACE_CTRL, TRACE_CTRL_PERIODIC_TRACE_EN_MASK);
			c->trace_method = PERIODIC_PACKET;
			c->trace_periodic_timer = PERIODIC_TRACE_DEFAULT_NS;
			return count;
		} else if (strcmp(debug_buf, "xor\n") == 0) {
			val = clk_osm_read_reg(c, TRACE_CTRL);
			val &= ~TRACE_CTRL_PERIODIC_TRACE_ENABLE;
			clk_osm_write_reg(c, val, TRACE_CTRL);
			c->trace_method = XOR_PACKET;
			return count;
		}
	}

	pr_err("error, supported trace mode types: 'periodic' or 'xor'\n");
	return -EINVAL;
}

static ssize_t debugfs_trace_method_get(struct file *file, char __user *buf,
					size_t count, loff_t *ppos)
{
	struct clk_osm *c = file->private_data;
	int len, rc;

	if (IS_ERR(file) || file == NULL) {
		pr_err("input error %ld\n", PTR_ERR(file));
		return -EINVAL;
	}

	if (!c) {
		pr_err("invalid clk_osm handle\n");
		return -EINVAL;
	}

	mutex_lock(&debug_buf_mutex);

	if (c->trace_method == PERIODIC_PACKET)
		len = snprintf(debug_buf, sizeof(debug_buf), "periodic\n");
	else if (c->trace_method == XOR_PACKET)
		len = snprintf(debug_buf, sizeof(debug_buf), "xor\n");

	rc = simple_read_from_buffer((void __user *) buf, len, ppos,
				     (void *) debug_buf, len);

	mutex_unlock(&debug_buf_mutex);

	return rc;
}

static int debugfs_trace_method_open(struct inode *inode, struct file *file)
{
	if (IS_ERR(file) || file == NULL) {
		pr_err("input error %ld\n", PTR_ERR(file));
		return -EINVAL;
	}

	file->private_data = inode->i_private;
	return 0;
}

static const struct file_operations debugfs_trace_method_fops = {
	.write	= debugfs_trace_method_set,
	.open   = debugfs_trace_method_open,
	.read	= debugfs_trace_method_get,
};

static int debugfs_get_trace_packet_id(void *data, u64 *val)
{
	struct clk_osm *c = data;

	*val = c->trace_id;
	return 0;
}

static int debugfs_set_trace_packet_id(void *data, u64 val)
{
	struct clk_osm *c = data;

	if (val < TRACE_PACKET0 || val > TRACE_PACKET3) {
		pr_err("supported trace IDs=%d-%d\n",
		       TRACE_PACKET0, TRACE_PACKET3);
		return 0;
	}

	clk_osm_masked_write_reg(c, val << TRACE_CTRL_PACKET_TYPE_SHIFT,
				 TRACE_CTRL, TRACE_CTRL_PACKET_TYPE_MASK);
	c->trace_id = val;
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(debugfs_trace_packet_id_fops,
			debugfs_get_trace_packet_id,
			debugfs_set_trace_packet_id,
			"%llu\n");

static int debugfs_get_trace_periodic_timer(void *data, u64 *val)
{
	struct clk_osm *c = data;

	*val = c->trace_periodic_timer;
	return 0;
}

static int debugfs_set_trace_periodic_timer(void *data, u64 val)
{
	struct clk_osm *c = data;

	if (val < PERIODIC_TRACE_MIN_NS || val > PERIODIC_TRACE_MAX_NS) {
		pr_err("supported periodic trace periods=%d-%lld ns\n",
		       PERIODIC_TRACE_MIN_NS, PERIODIC_TRACE_MAX_NS);
		return 0;
	}

	clk_osm_write_reg(c, clk_osm_count_ns(c, val),
			  PERIODIC_TRACE_TIMER_CTRL);
	c->trace_periodic_timer = val;
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(debugfs_trace_periodic_timer_fops,
			debugfs_get_trace_periodic_timer,
			debugfs_set_trace_periodic_timer,
			"%llu\n");

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
		clk_osm_acd_master_write_reg(c, val, c->acd_debugfs_addr);
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

	c->debugfs = debugfs_create_dir(c->hw.init->name, osm_debugfs_base);
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

	temp = debugfs_create_file("wdog_trace_enable",
			   S_IRUGO | S_IWUSR,
			   c->debugfs, c,
			   &debugfs_trace_wdog_enable_fops);
	if (IS_ERR_OR_NULL(temp)) {
		pr_err("debugfs_trace_wdog_enable_fops debugfs file creation failed\n");
		goto exit;
	}

	temp = debugfs_create_file("trace_enable",
			   S_IRUGO | S_IWUSR,
			   c->debugfs, c,
			   &debugfs_trace_enable_fops);
	if (IS_ERR_OR_NULL(temp)) {
		pr_err("debugfs_trace_enable_fops debugfs file creation failed\n");
		goto exit;
	}

	temp = debugfs_create_file("trace_method",
			   S_IRUGO | S_IWUSR,
			   c->debugfs, c,
			   &debugfs_trace_method_fops);
	if (IS_ERR_OR_NULL(temp)) {
		pr_err("debugfs_trace_method_fops debugfs file creation failed\n");
		goto exit;
	}

	temp = debugfs_create_file("trace_packet_id",
			   S_IRUGO | S_IWUSR,
			   c->debugfs, c,
			   &debugfs_trace_packet_id_fops);
	if (IS_ERR_OR_NULL(temp)) {
		pr_err("debugfs_trace_packet_id_fops debugfs file creation failed\n");
		goto exit;
	}

	temp = debugfs_create_file("trace_periodic_timer",
			   S_IRUGO | S_IWUSR,
			   c->debugfs, c,
			   &debugfs_trace_periodic_timer_fops);
	if (IS_ERR_OR_NULL(temp)) {
		pr_err("debugfs_trace_periodic_timer_fops debugfs file creation failed\n");
		goto exit;
	}

	temp = debugfs_create_file("acd_debug_reg",
			   S_IRUGO | S_IWUSR,
			   c->debugfs, c,
			   &debugfs_acd_debug_reg_fops);
	if (IS_ERR_OR_NULL(temp)) {
		pr_err("debugfs_acd_debug_reg_fops debugfs file creation failed\n");
		goto exit;
	}

	temp = debugfs_create_file("acd_debug_reg_addr",
			   S_IRUGO | S_IWUSR,
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

static int clk_osm_panic_callback(struct notifier_block *nfb,
				  unsigned long event,
				  void *data)
{
	void __iomem *virt_addr;
	u32 value, reg;
	struct clk_osm *c = container_of(nfb,
					 struct clk_osm,
					 panic_notifier);

	reg = c->pbases[OSM_BASE] + WDOG_DOMAIN_PSTATE_STATUS;
	virt_addr = ioremap(reg, 0x4);
	if (virt_addr != NULL) {
		value = readl_relaxed(virt_addr);
		pr_err("DOM%d_PSTATE_STATUS[0x%08x]=0x%08x\n", c->cluster_num,
		       reg, value);
		iounmap(virt_addr);
	}

	reg = c->pbases[OSM_BASE] + WDOG_PROGRAM_COUNTER;
	virt_addr = ioremap(reg, 0x4);
	if (virt_addr != NULL) {
		value = readl_relaxed(virt_addr);
		pr_err("DOM%d_PROGRAM_COUNTER[0x%08x]=0x%08x\n", c->cluster_num,
		       reg, value);
		iounmap(virt_addr);
	}

	virt_addr = ioremap(c->apm_ctrl_status, 0x4);
	if (virt_addr != NULL) {
		value = readl_relaxed(virt_addr);
		pr_err("APM_CTLER_STATUS_%d[0x%08x]=0x%08x\n", c->cluster_num,
		       c->apm_ctrl_status, value);
		iounmap(virt_addr);
	}

	return NOTIFY_OK;
}

static int clk_osm_acd_init(struct clk_osm *c)
{

	int rc = 0;
	u32 auto_xfer_mask = 0;

	if (!c->acd_init)
		return 0;

	c->acd_debugfs_addr = ACD_HW_VERSION;

	/* Program ACD tunable-length delay register */
	clk_osm_acd_master_write_reg(c, c->acd_td, ACDTD);
	auto_xfer_mask |= ACD_REG_RELATIVE_ADDR_BITMASK(ACDTD);

	/* Program ACD control register */
	clk_osm_acd_master_write_reg(c, c->acd_cr, ACDCR);
	auto_xfer_mask |= ACD_REG_RELATIVE_ADDR_BITMASK(ACDCR);

	/* Program ACD soft start control register */
	clk_osm_acd_master_write_reg(c, c->acd_sscr, ACDSSCR);
	auto_xfer_mask |= ACD_REG_RELATIVE_ADDR_BITMASK(ACDSSCR);

	/* Program initial ACD external interface configuration register */
	clk_osm_acd_master_write_reg(c, c->acd_extint0_cfg, ACD_EXTINT_CFG);
	auto_xfer_mask |= ACD_REG_RELATIVE_ADDR_BITMASK(ACD_EXTINT_CFG);

	/* Program ACD auto-register transfer control register */
	clk_osm_acd_master_write_reg(c, c->acd_autoxfer_ctl, ACD_AUTOXFER_CTL);

	/* Ensure writes complete before transfers to local copy */
	clk_osm_acd_mb(c);

	/* Transfer master copies */
	rc = clk_osm_acd_auto_local_write_reg(c, auto_xfer_mask);
	if (rc)
		return rc;

	/* Switch CPUSS clock source to ACD clock */
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

	/*
	 * ACDCR, ACDTD, ACDSSCR, ACD_EXTINT_CFG, ACD_GFMUX_CFG
	 * must be copied from master to local copy on PC exit.
	 */
	auto_xfer_mask |= ACD_REG_RELATIVE_ADDR_BITMASK(ACD_GFMUX_CFG);
	clk_osm_acd_master_write_reg(c, auto_xfer_mask, ACD_AUTOXFER_CFG);

	return 0;
}

static unsigned long init_rate = 300000000;
static unsigned long osm_clk_init_rate = 200000000;
static unsigned long pwrcl_boot_rate = 1401600000;
static unsigned long perfcl_boot_rate = 1747200000;

static int clk_cpu_osm_driver_probe(struct platform_device *pdev)
{
	int rc, cpu, i;
	int speedbin = 0, pvs_ver = 0;
	u32 pte_efuse;
	int num_clks = ARRAY_SIZE(osm_qcom_clk_hws);
	struct clk *clk;
	struct clk *ext_xo_clk, *ext_hmss_gpll0_clk_src;
	struct device *dev = &pdev->dev;
	struct clk_onecell_data *clk_data;
	char perfclspeedbinstr[] = "qcom,perfcl-speedbin0-v0";
	char pwrclspeedbinstr[] = "qcom,pwrcl-speedbin0-v0";
	struct cpu_cycle_counter_cb cb = {
		.get_cpu_cycle_counter = clk_osm_get_cpu_cycle_counter,
	};

	/*
	 * Require the RPM-XO clock and GCC-HMSS-GPLL0 clocks to be registererd
	 * before OSM.
	 */
	ext_xo_clk = devm_clk_get(dev, "xo_a");
	if (IS_ERR(ext_xo_clk)) {
		if (PTR_ERR(ext_xo_clk) != -EPROBE_DEFER)
			dev_err(dev, "Unable to get xo clock\n");
		return PTR_ERR(ext_xo_clk);
	}

	ext_hmss_gpll0_clk_src = devm_clk_get(dev, "aux_clk");
	if (IS_ERR(ext_hmss_gpll0_clk_src)) {
		if (PTR_ERR(ext_hmss_gpll0_clk_src) != -EPROBE_DEFER)
			dev_err(dev, "Unable to get aux_clk clock\n");
		return PTR_ERR(ext_hmss_gpll0_clk_src);
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

	if (pwrcl_clk.vbases[EFUSE_BASE]) {
		/* Multiple speed-bins are supported */
		pte_efuse = readl_relaxed(pwrcl_clk.vbases[EFUSE_BASE]);
		speedbin = ((pte_efuse >> PWRCL_EFUSE_SHIFT) &
						    PWRCL_EFUSE_MASK);
		snprintf(pwrclspeedbinstr, ARRAY_SIZE(pwrclspeedbinstr),
			 "qcom,pwrcl-speedbin%d-v%d", speedbin, pvs_ver);
	}

	dev_info(&pdev->dev, "using pwrcl speed bin %u and pvs_ver %d\n",
		 speedbin, pvs_ver);

	rc = clk_osm_get_lut(pdev, &pwrcl_clk, pwrclspeedbinstr);
	if (rc) {
		dev_err(&pdev->dev, "Unable to get OSM LUT for power cluster, rc=%d\n",
			rc);
		return rc;
	}

	if (perfcl_clk.vbases[EFUSE_BASE]) {
		/* Multiple speed-bins are supported */
		pte_efuse = readl_relaxed(perfcl_clk.vbases[EFUSE_BASE]);
		speedbin = ((pte_efuse >> PERFCL_EFUSE_SHIFT) &
							PERFCL_EFUSE_MASK);
		snprintf(perfclspeedbinstr, ARRAY_SIZE(perfclspeedbinstr),
			 "qcom,perfcl-speedbin%d-v%d", speedbin, pvs_ver);
	}

	dev_info(&pdev->dev, "using perfcl speed bin %u and pvs_ver %d\n",
		 speedbin, pvs_ver);

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

	rc = clk_osm_resolve_crossover_corners(&pwrcl_clk, pdev, NULL);
	if (rc)
		dev_info(&pdev->dev, "No APM crossover corner programmed\n");

	rc = clk_osm_resolve_crossover_corners(&perfcl_clk, pdev,
				"qcom,perfcl-apcs-mem-acc-threshold-voltage");
	if (rc)
		dev_info(&pdev->dev, "No MEM-ACC crossover corner programmed\n");

	clk_osm_setup_cycle_counters(&pwrcl_clk);
	clk_osm_setup_cycle_counters(&perfcl_clk);

	clk_osm_print_osm_table(&pwrcl_clk);
	clk_osm_print_osm_table(&perfcl_clk);

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

	clk_osm_setup_fsms(&pwrcl_clk);
	clk_osm_setup_fsms(&perfcl_clk);

	/*
	 * Perform typical secure-world HW initialization
	 * as necessary.
	 */
	clk_osm_do_additional_setup(&pwrcl_clk, pdev);
	clk_osm_do_additional_setup(&perfcl_clk, pdev);

	/* MEM-ACC Programming */
	clk_osm_program_mem_acc_regs(&pwrcl_clk);
	clk_osm_program_mem_acc_regs(&perfcl_clk);

	/* Program APM crossover corners */
	clk_osm_apm_vc_setup(&pwrcl_clk);
	clk_osm_apm_vc_setup(&perfcl_clk);

	rc = clk_osm_setup_irq(pdev, &pwrcl_clk, "pwrcl-irq");
	if (rc)
		pr_err("Debug IRQ not set for pwrcl\n");

	rc = clk_osm_setup_irq(pdev, &perfcl_clk, "perfcl-irq");
	if (rc)
		pr_err("Debug IRQ not set for perfcl\n");

	if (of_property_read_bool(pdev->dev.of_node, "qcom,osm-pll-setup")) {
		clk_osm_setup_cluster_pll(&pwrcl_clk);
		clk_osm_setup_cluster_pll(&perfcl_clk);
	}

	rc = clk_osm_acd_init(&pwrcl_clk);
	if (rc) {
		pr_err("failed to initialize ACD for pwrcl, rc=%d\n", rc);
		return rc;
	}
	rc = clk_osm_acd_init(&perfcl_clk);
	if (rc) {
		pr_err("failed to initialize ACD for perfcl, rc=%d\n", rc);
		return rc;
	}

	spin_lock_init(&pwrcl_clk.lock);
	spin_lock_init(&perfcl_clk.lock);

	pwrcl_clk.panic_notifier.notifier_call = clk_osm_panic_callback;
	atomic_notifier_chain_register(&panic_notifier_list,
				       &pwrcl_clk.panic_notifier);
	perfcl_clk.panic_notifier.notifier_call = clk_osm_panic_callback;
	atomic_notifier_chain_register(&panic_notifier_list,
				       &perfcl_clk.panic_notifier);

	/* Register OSM pwr and perf clocks with Clock Framework */
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

	/*
	 * The hmss_gpll0 clock runs at 300 MHz. Ensure it is at the correct
	 * frequency before enabling OSM. LUT index 0 is always sourced from
	 * this clock.
	 */
	rc = clk_set_rate(sys_apcsaux_clk_gcc.hw.clk, init_rate);
	if (rc) {
		dev_err(&pdev->dev, "Unable to set init rate on hmss_gpll0, rc=%d\n",
			rc);
		return rc;
	}
	clk_prepare_enable(sys_apcsaux_clk_gcc.hw.clk);

	rc = clk_set_rate(osm_clk_src.clkr.hw.clk, osm_clk_init_rate);
	if (rc) {
		dev_err(&pdev->dev, "Unable to set init rate on osm_clk, rc=%d\n",
			rc);
		goto exit2;
	}

	/* Make sure index zero is selected */
	rc = clk_set_rate(pwrcl_clk.hw.clk, init_rate);
	if (rc) {
		dev_err(&pdev->dev, "Unable to set init rate on pwr cluster, rc=%d\n",
			rc);
		goto exit2;
	}

	rc = clk_set_rate(perfcl_clk.hw.clk, init_rate);
	if (rc) {
		dev_err(&pdev->dev, "Unable to set init rate on perf cluster, rc=%d\n",
			rc);
		goto exit2;
	}

	get_online_cpus();

	/* Enable OSM */
	for_each_online_cpu(cpu) {
		WARN(clk_prepare_enable(logical_cpu_to_clk(cpu)),
		     "Failed to enable clock for cpu %d\n", cpu);
	}

	/* Set final boot rate */
	rc = clk_set_rate(pwrcl_clk.hw.clk, pwrcl_boot_rate);
	if (rc) {
		dev_err(&pdev->dev, "Unable to set boot rate on pwr cluster, rc=%d\n",
			rc);
		goto exit2;
	}

	rc = clk_set_rate(perfcl_clk.hw.clk, perfcl_boot_rate);
	if (rc) {
		dev_err(&pdev->dev, "Unable to set boot rate on perf cluster, rc=%d\n",
			rc);
		goto exit2;
	}

	populate_opp_table(pdev);
	populate_debugfs_dir(&pwrcl_clk);
	populate_debugfs_dir(&perfcl_clk);

	of_platform_populate(pdev->dev.of_node, NULL, NULL, &pdev->dev);

	register_cpu_cycle_counter_cb(&cb);

	pr_info("OSM driver inited\n");
	put_online_cpus();

	return 0;

exit2:
	clk_disable_unprepare(sys_apcsaux_clk_gcc.hw.clk);
provider_err:
	if (clk_data)
		devm_kfree(&pdev->dev, clk_data->clks);
clk_err:
	devm_kfree(&pdev->dev, clk_data);
exit:
	dev_err(&pdev->dev, "OSM driver failed to initialize, rc=%d\n", rc);
	panic("Unable to Setup OSM");
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

MODULE_DESCRIPTION("CPU clock driver for OSM");
MODULE_LICENSE("GPL v2");
