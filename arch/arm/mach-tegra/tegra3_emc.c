/*
 * arch/arm/mach-tegra/tegra3_emc.c
 *
 * Copyright (C) 2011-2012, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#include <linux/kernel.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/suspend.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/platform_data/tegra_emc.h>

#include <asm/cputime.h>
#include <asm/cacheflush.h>

#include <mach/iomap.h>
#include <mach/latency_allowance.h>

#include "clock.h"
#include "dvfs.h"
#include "tegra3_emc.h"
#include "fuse.h"

#ifdef CONFIG_TEGRA_EMC_SCALING_ENABLE
static bool emc_enable = true;
#else
static bool emc_enable;
#endif
module_param(emc_enable, bool, 0644);

u8 tegra_emc_bw_efficiency = 35;
u8 tegra_emc_bw_efficiency_boost = 45;

#define EMC_MIN_RATE_DDR3		25500000
#define EMC_STATUS_UPDATE_TIMEOUT	100
#define TEGRA_EMC_TABLE_MAX_SIZE	16

enum {
	DLL_CHANGE_NONE = 0,
	DLL_CHANGE_ON,
	DLL_CHANGE_OFF,
};

#define EMC_CLK_DIV_SHIFT		0
#define EMC_CLK_DIV_MASK		(0xFF << EMC_CLK_DIV_SHIFT)
#define EMC_CLK_SOURCE_SHIFT		30
#define EMC_CLK_SOURCE_MASK		(0x3 << EMC_CLK_SOURCE_SHIFT)
#define EMC_CLK_LOW_JITTER_ENABLE	(0x1 << 29)
#define	EMC_CLK_MC_SAME_FREQ		(0x1 << 16)

#define BURST_REG_LIST \
	DEFINE_REG(TEGRA_EMC_BASE, EMC_RC),			\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_RFC),			\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_RAS),			\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_RP),			\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_R2W),			\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_W2R),			\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_R2P),			\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_W2P),			\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_RD_RCD),			\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_WR_RCD),			\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_RRD),			\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_REXT),			\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_WEXT),			\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_WDV),			\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_QUSE),			\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_QRST),			\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_QSAFE),			\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_RDV),			\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_REFRESH),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_BURST_REFRESH_NUM),	\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_PRE_REFRESH_REQ_CNT),	\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_PDEX2WR),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_PDEX2RD),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_PCHG2PDEN),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_ACT2PDEN),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_AR2PDEN),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_RW2PDEN),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_TXSR),			\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_TXSRDLL),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_TCKE),			\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_TFAW),			\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_TRPAB),			\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_TCLKSTABLE),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_TCLKSTOP),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_TREFBW),			\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_QUSE_EXTRA),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_FBIO_CFG6),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_ODT_WRITE),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_ODT_READ),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_FBIO_CFG5),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_CFG_DIG_DLL),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_CFG_DIG_DLL_PERIOD),	\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_DLL_XFORM_DQS0),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_DLL_XFORM_DQS1),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_DLL_XFORM_DQS2),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_DLL_XFORM_DQS3),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_DLL_XFORM_DQS4),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_DLL_XFORM_DQS5),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_DLL_XFORM_DQS6),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_DLL_XFORM_DQS7),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_DLL_XFORM_QUSE0),	\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_DLL_XFORM_QUSE1),	\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_DLL_XFORM_QUSE2),	\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_DLL_XFORM_QUSE3),	\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_DLL_XFORM_QUSE4),	\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_DLL_XFORM_QUSE5),	\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_DLL_XFORM_QUSE6),	\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_DLL_XFORM_QUSE7),	\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_DLI_TRIM_TXDQS0),	\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_DLI_TRIM_TXDQS1),	\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_DLI_TRIM_TXDQS2),	\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_DLI_TRIM_TXDQS3),	\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_DLI_TRIM_TXDQS4),	\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_DLI_TRIM_TXDQS5),	\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_DLI_TRIM_TXDQS6),	\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_DLI_TRIM_TXDQS7),	\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_DLL_XFORM_DQ0),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_DLL_XFORM_DQ1),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_DLL_XFORM_DQ2),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_DLL_XFORM_DQ3),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_XM2CMDPADCTRL),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_XM2DQSPADCTRL2),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_XM2DQPADCTRL2),		\
	DEFINE_REG(0		 , EMC_XM2CLKPADCTRL),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_XM2COMPPADCTRL),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_XM2VTTGENPADCTRL),	\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_XM2VTTGENPADCTRL2),	\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_XM2QUSEPADCTRL),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_XM2DQSPADCTRL3),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_CTT_TERM_CTRL),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_ZCAL_INTERVAL),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_ZCAL_WAIT_CNT),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_MRS_WAIT_CNT),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_AUTO_CAL_CONFIG),	\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_CTT),			\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_CTT_DURATION),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_DYN_SELF_REF_CONTROL),	\
								\
	DEFINE_REG(TEGRA_MC_BASE, MC_EMEM_ARB_CFG),		\
	DEFINE_REG(TEGRA_MC_BASE, MC_EMEM_ARB_OUTSTANDING_REQ),	\
	DEFINE_REG(TEGRA_MC_BASE, MC_EMEM_ARB_TIMING_RCD),	\
	DEFINE_REG(TEGRA_MC_BASE, MC_EMEM_ARB_TIMING_RP),	\
	DEFINE_REG(TEGRA_MC_BASE, MC_EMEM_ARB_TIMING_RC),	\
	DEFINE_REG(TEGRA_MC_BASE, MC_EMEM_ARB_TIMING_RAS),	\
	DEFINE_REG(TEGRA_MC_BASE, MC_EMEM_ARB_TIMING_FAW),	\
	DEFINE_REG(TEGRA_MC_BASE, MC_EMEM_ARB_TIMING_RRD),	\
	DEFINE_REG(TEGRA_MC_BASE, MC_EMEM_ARB_TIMING_RAP2PRE),	\
	DEFINE_REG(TEGRA_MC_BASE, MC_EMEM_ARB_TIMING_WAP2PRE),	\
	DEFINE_REG(TEGRA_MC_BASE, MC_EMEM_ARB_TIMING_R2R),	\
	DEFINE_REG(TEGRA_MC_BASE, MC_EMEM_ARB_TIMING_W2W),	\
	DEFINE_REG(TEGRA_MC_BASE, MC_EMEM_ARB_TIMING_R2W),	\
	DEFINE_REG(TEGRA_MC_BASE, MC_EMEM_ARB_TIMING_W2R),	\
	DEFINE_REG(TEGRA_MC_BASE, MC_EMEM_ARB_DA_TURNS),	\
	DEFINE_REG(TEGRA_MC_BASE, MC_EMEM_ARB_DA_COVERS),	\
	DEFINE_REG(TEGRA_MC_BASE, MC_EMEM_ARB_MISC0),		\
	DEFINE_REG(TEGRA_MC_BASE, MC_EMEM_ARB_RING1_THROTTLE),	\
								\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_FBIO_SPARE),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_CFG_RSV),

#define DEFINE_REG(base, reg) ((base) ? (IO_ADDRESS((base)) + (reg)) : 0)
static const void __iomem *burst_reg_addr[TEGRA30_EMC_NUM_REGS] = {
	BURST_REG_LIST
};
#undef DEFINE_REG

#define DEFINE_REG(base, reg)	reg##_INDEX
enum {
	BURST_REG_LIST
};
#undef DEFINE_REG

static int emc_num_burst_regs;

struct emc_sel {
	struct clk	*input;
	u32		value;
	unsigned long	input_rate;
};

static struct emc_sel tegra_emc_clk_sel[TEGRA_EMC_TABLE_MAX_SIZE];
static struct tegra30_emc_table start_timing;
static const struct tegra30_emc_table *emc_timing;
static unsigned long dram_over_temp_state = DRAM_OVER_TEMP_NONE;

static const u32 *dram_to_soc_bit_map;
static const struct tegra30_emc_table *tegra_emc_table;
static int tegra_emc_table_size;

static u32 dram_dev_num;
static u32 emc_cfg_saved;
static u32 dram_type = -1;

static struct clk *emc;
static struct clk *bridge;

static struct {
	cputime64_t time_at_clock[TEGRA_EMC_TABLE_MAX_SIZE];
	int last_sel;
	u64 last_update;
	u64 clkchange_count;
	spinlock_t spinlock;
} emc_stats;

static DEFINE_SPINLOCK(emc_access_lock);

static void __iomem *emc_base = IO_ADDRESS(TEGRA_EMC_BASE);
static void __iomem *mc_base = IO_ADDRESS(TEGRA_MC_BASE);
static void __iomem *clk_base = IO_ADDRESS(TEGRA_CLK_RESET_BASE);

static inline void emc_writel(u32 val, unsigned long addr)
{
	writel(val, emc_base + addr);
	barrier();
}
static inline u32 emc_readl(unsigned long addr)
{
	return readl(emc_base + addr);
}
static inline void mc_writel(u32 val, unsigned long addr)
{
	writel(val, mc_base + addr);
	barrier();
}
static inline u32 mc_readl(unsigned long addr)
{
	return readl(mc_base + addr);
}

static void emc_last_stats_update(int last_sel)
{
	unsigned long flags;
	u64 cur_jiffies = get_jiffies_64();

	spin_lock_irqsave(&emc_stats.spinlock, flags);

	if (emc_stats.last_sel < TEGRA_EMC_TABLE_MAX_SIZE)
		emc_stats.time_at_clock[emc_stats.last_sel] =
			emc_stats.time_at_clock[emc_stats.last_sel] +
			(cur_jiffies - emc_stats.last_update);

	emc_stats.last_update = cur_jiffies;

	if (last_sel < TEGRA_EMC_TABLE_MAX_SIZE) {
		emc_stats.clkchange_count++;
		emc_stats.last_sel = last_sel;
	}
	spin_unlock_irqrestore(&emc_stats.spinlock, flags);
}

static int wait_for_update(u32 status_reg, u32 bit_mask, bool updated_state)
{
	int i;
	for (i = 0; i < EMC_STATUS_UPDATE_TIMEOUT; i++) {
		if (!!(emc_readl(status_reg) & bit_mask) == updated_state)
			return 0;
		udelay(1);
	}
	return -ETIMEDOUT;
}

static inline void emc_timing_update(void)
{
	int err;

	emc_writel(0x1, EMC_TIMING_CONTROL);
	err = wait_for_update(EMC_STATUS,
			      EMC_STATUS_TIMING_UPDATE_STALLED, false);
	if (err) {
		pr_err("%s: timing update error: %d", __func__, err);
		BUG();
	}
}

static inline void auto_cal_disable(void)
{
	int err;

	emc_writel(0, EMC_AUTO_CAL_INTERVAL);
	err = wait_for_update(EMC_AUTO_CAL_STATUS,
			      EMC_AUTO_CAL_STATUS_ACTIVE, false);
	if (err) {
		pr_err("%s: disable auto-cal error: %d", __func__, err);
		BUG();
	}
}

static inline void set_over_temp_timing(
	const struct tegra30_emc_table *next_timing, unsigned long state)
{
#define REFRESH_SPEEDUP(val)						      \
	do {								      \
		val = ((val) & 0xFFFF0000) | (((val) & 0xFFFF) >> 2);	      \
	} while (0)

	u32 ref = next_timing->burst_regs[EMC_REFRESH_INDEX];
	u32 pre_ref = next_timing->burst_regs[EMC_PRE_REFRESH_REQ_CNT_INDEX];
	u32 dsr_cntrl = next_timing->burst_regs[EMC_DYN_SELF_REF_CONTROL_INDEX];

	switch (state) {
	case DRAM_OVER_TEMP_NONE:
		break;
	case DRAM_OVER_TEMP_REFRESH:
		REFRESH_SPEEDUP(ref);
		REFRESH_SPEEDUP(pre_ref);
		REFRESH_SPEEDUP(dsr_cntrl);
		break;
	default:
		pr_err("%s: Failed to set dram over temp state %lu\n",
		       __func__, state);
		BUG();
	}

	__raw_writel(ref, burst_reg_addr[EMC_REFRESH_INDEX]);
	__raw_writel(pre_ref, burst_reg_addr[EMC_PRE_REFRESH_REQ_CNT_INDEX]);
	__raw_writel(dsr_cntrl, burst_reg_addr[EMC_DYN_SELF_REF_CONTROL_INDEX]);
}

static inline void set_mc_arbiter_limits(void)
{
	u32 reg = mc_readl(MC_EMEM_ARB_OUTSTANDING_REQ);
	u32 max_val = 0x50 << EMC_MRS_WAIT_CNT_SHORT_WAIT_SHIFT;

	if (!(reg & MC_EMEM_ARB_OUTSTANDING_REQ_HOLDOFF_OVERRIDE) ||
	    ((reg & MC_EMEM_ARB_OUTSTANDING_REQ_MAX_MASK) > max_val)) {
		reg = MC_EMEM_ARB_OUTSTANDING_REQ_LIMIT_ENABLE |
			MC_EMEM_ARB_OUTSTANDING_REQ_HOLDOFF_OVERRIDE | max_val;
		mc_writel(reg, MC_EMEM_ARB_OUTSTANDING_REQ);
		mc_writel(0x1, MC_TIMING_CONTROL);
	}
}

static inline void disable_early_ack(u32 mc_override)
{
	static u32 override_val;

	override_val = mc_override & (~MC_EMEM_ARB_OVERRIDE_EACK_MASK);
	mc_writel(override_val, MC_EMEM_ARB_OVERRIDE);
	__cpuc_flush_dcache_area(&override_val, sizeof(override_val));
	outer_clean_range(__pa(&override_val), __pa(&override_val + 1));
	override_val |= mc_override & MC_EMEM_ARB_OVERRIDE_EACK_MASK;
}

static inline void enable_early_ack(u32 mc_override)
{
	mc_writel((mc_override | MC_EMEM_ARB_OVERRIDE_EACK_MASK),
			MC_EMEM_ARB_OVERRIDE);
}

static inline bool dqs_preset(const struct tegra30_emc_table *next_timing,
			      const struct tegra30_emc_table *last_timing)
{
	bool ret = false;

#define DQS_SET(reg, bit)						      \
	do {								      \
		if ((next_timing->burst_regs[EMC_##reg##_INDEX] &	      \
		     EMC_##reg##_##bit##_ENABLE) &&			      \
		    (!(last_timing->burst_regs[EMC_##reg##_INDEX] &	      \
		       EMC_##reg##_##bit##_ENABLE)))   {		      \
			emc_writel(last_timing->burst_regs[EMC_##reg##_INDEX] \
				   | EMC_##reg##_##bit##_ENABLE, EMC_##reg);  \
			ret = true;					      \
		}							      \
	} while (0)

	DQS_SET(XM2DQSPADCTRL2, VREF);
	DQS_SET(XM2DQSPADCTRL3, VREF);
	DQS_SET(XM2QUSEPADCTRL, IVREF);

	return ret;
}

static inline void overwrite_mrs_wait_cnt(
	const struct tegra30_emc_table *next_timing,
	bool zcal_long)
{
	u32 reg;
	u32 cnt = 512;

	/* For ddr3 when DLL is re-started: overwrite EMC DFS table settings
	   for MRS_WAIT_LONG with maximum of MRS_WAIT_SHORT settings and
	   expected operation length. Reduce the latter by the overlapping
	   zq-calibration, if any */
	if (zcal_long)
		cnt -= dram_dev_num * 256;

	reg = (next_timing->burst_regs[EMC_MRS_WAIT_CNT_INDEX] &
		EMC_MRS_WAIT_CNT_SHORT_WAIT_MASK) >>
		EMC_MRS_WAIT_CNT_SHORT_WAIT_SHIFT;
	if (cnt < reg)
		cnt = reg;

	reg = (next_timing->burst_regs[EMC_MRS_WAIT_CNT_INDEX] &
		(~EMC_MRS_WAIT_CNT_LONG_WAIT_MASK));
	reg |= (cnt << EMC_MRS_WAIT_CNT_LONG_WAIT_SHIFT) &
		EMC_MRS_WAIT_CNT_LONG_WAIT_MASK;

	emc_writel(reg, EMC_MRS_WAIT_CNT);
}

static inline bool need_qrst(const struct tegra30_emc_table *next_timing,
			     const struct tegra30_emc_table *last_timing,
			     u32 emc_dpd_reg)
{
	u32 last_mode = (last_timing->burst_regs[EMC_FBIO_CFG5_INDEX] &
		EMC_CFG5_QUSE_MODE_MASK) >> EMC_CFG5_QUSE_MODE_SHIFT;
	u32 next_mode = (next_timing->burst_regs[EMC_FBIO_CFG5_INDEX] &
		EMC_CFG5_QUSE_MODE_MASK) >> EMC_CFG5_QUSE_MODE_SHIFT;

	/* QUSE DPD is disabled */
	bool ret = !(emc_dpd_reg & EMC_SEL_DPD_CTRL_QUSE_DPD_ENABLE) &&

	/* QUSE uses external mode before or after clock change */
		(((last_mode != EMC_CFG5_QUSE_MODE_PULSE_INTERN) &&
		  (last_mode != EMC_CFG5_QUSE_MODE_INTERNAL_LPBK)) ||
		 ((next_mode != EMC_CFG5_QUSE_MODE_PULSE_INTERN) &&
		  (next_mode != EMC_CFG5_QUSE_MODE_INTERNAL_LPBK)))  &&

	/* QUSE pad switches from schmitt to vref mode */
		(((last_timing->burst_regs[EMC_XM2QUSEPADCTRL_INDEX] &
		   EMC_XM2QUSEPADCTRL_IVREF_ENABLE) == 0) &&
		 ((next_timing->burst_regs[EMC_XM2QUSEPADCTRL_INDEX] &
		   EMC_XM2QUSEPADCTRL_IVREF_ENABLE) != 0));

	return ret;
}

static inline void periodic_qrst_enable(u32 emc_cfg_reg, u32 emc_dbg_reg)
{
	/* enable write mux => enable periodic QRST => restore mux */
	emc_writel(emc_dbg_reg | EMC_DBG_WRITE_MUX_ACTIVE, EMC_DBG);
	emc_writel(emc_cfg_reg | EMC_CFG_PERIODIC_QRST, EMC_CFG);
	emc_writel(emc_dbg_reg, EMC_DBG);
}

static inline int get_dll_change(const struct tegra30_emc_table *next_timing,
				 const struct tegra30_emc_table *last_timing)
{
	bool next_dll_enabled = !(next_timing->emc_mode_1 & 0x1);
	bool last_dll_enabled = !(last_timing->emc_mode_1 & 0x1);

	if (next_dll_enabled == last_dll_enabled)
		return DLL_CHANGE_NONE;
	else if (next_dll_enabled)
		return DLL_CHANGE_ON;
	else
		return DLL_CHANGE_OFF;
}

static inline void set_dram_mode(const struct tegra30_emc_table *next_timing,
				 const struct tegra30_emc_table *last_timing,
				 int dll_change)
{
	if (dram_type == DRAM_TYPE_DDR3) {
		/* first mode_1, then mode_2, then mode_reset*/
		if (next_timing->emc_mode_1 != last_timing->emc_mode_1)
			emc_writel(next_timing->emc_mode_1, EMC_EMRS);
		if (next_timing->emc_mode_2 != last_timing->emc_mode_2)
			emc_writel(next_timing->emc_mode_2, EMC_EMRS);

		if ((next_timing->emc_mode_reset !=
		     last_timing->emc_mode_reset) ||
		    (dll_change == DLL_CHANGE_ON))
		{
			u32 reg = next_timing->emc_mode_reset &
				(~EMC_MODE_SET_DLL_RESET);
			if (dll_change == DLL_CHANGE_ON) {
				reg |= EMC_MODE_SET_DLL_RESET;
				reg |= EMC_MODE_SET_LONG_CNT;
			}
			emc_writel(reg, EMC_MRS);
		}
	} else {
		/* first mode_2, then mode_1; mode_reset is not applicable */
		if (next_timing->emc_mode_2 != last_timing->emc_mode_2)
			emc_writel(next_timing->emc_mode_2, EMC_MRW);
		if (next_timing->emc_mode_1 != last_timing->emc_mode_1)
			emc_writel(next_timing->emc_mode_1, EMC_MRW);
	}
}

static inline void do_clock_change(u32 clk_setting)
{
	int err;

	mc_readl(MC_EMEM_ADR_CFG);	/* completes prev writes */
	writel(clk_setting, (u32)clk_base + emc->reg);
	readl((u32)clk_base + emc->reg);/* completes prev write */

	err = wait_for_update(EMC_INTSTATUS,
			      EMC_INTSTATUS_CLKCHANGE_COMPLETE, true);
	if (err) {
		pr_err("%s: clock change completion error: %d", __func__, err);
		BUG();
	}
}

static noinline void emc_set_clock(const struct tegra30_emc_table *next_timing,
				   const struct tegra30_emc_table *last_timing,
				   u32 clk_setting)
{
	int i, dll_change, pre_wait;
	bool dyn_sref_enabled, vref_cal_toggle, qrst_used, zcal_long;

	u32 mc_override = mc_readl(MC_EMEM_ARB_OVERRIDE);
	u32 emc_cfg_reg = emc_readl(EMC_CFG);
	u32 emc_dbg_reg = emc_readl(EMC_DBG);

	dyn_sref_enabled = emc_cfg_reg & EMC_CFG_DYN_SREF_ENABLE;
	dll_change = get_dll_change(next_timing, last_timing);
	zcal_long = (next_timing->burst_regs[EMC_ZCAL_INTERVAL_INDEX] != 0) &&
		(last_timing->burst_regs[EMC_ZCAL_INTERVAL_INDEX] == 0);

	/* FIXME: remove steps enumeration below? */

	/* 1. clear clkchange_complete interrupts */
	emc_writel(EMC_INTSTATUS_CLKCHANGE_COMPLETE, EMC_INTSTATUS);

	/* 2. disable dynamic self-refresh and preset dqs vref, then wait for
	   possible self-refresh entry/exit and/or dqs vref settled - waiting
	   before the clock change decreases worst case change stall time */
	pre_wait = 0;
	if (dyn_sref_enabled) {
		emc_cfg_reg &= ~EMC_CFG_DYN_SREF_ENABLE;
		emc_writel(emc_cfg_reg, EMC_CFG);
		pre_wait = 5;		/* 5us+ for self-refresh entry/exit */
	}

	/* 2.25 update MC arbiter settings */
	set_mc_arbiter_limits();
	if (mc_override & MC_EMEM_ARB_OVERRIDE_EACK_MASK)
		disable_early_ack(mc_override);

	/* 2.5 check dq/dqs vref delay */
	if (dqs_preset(next_timing, last_timing)) {
		if (pre_wait < 3)
			pre_wait = 3;	/* 3us+ for dqs vref settled */
	}
	if (pre_wait) {
		emc_timing_update();
		udelay(pre_wait);
	}

	/* 3. disable auto-cal if vref mode is switching */
	vref_cal_toggle = (next_timing->emc_acal_interval != 0) &&
		((next_timing->burst_regs[EMC_XM2COMPPADCTRL_INDEX] ^
		  last_timing->burst_regs[EMC_XM2COMPPADCTRL_INDEX]) &
		 EMC_XM2COMPPADCTRL_VREF_CAL_ENABLE);
	if (vref_cal_toggle)
		auto_cal_disable();

	/* 4. program burst shadow registers */
	for (i = 0; i < emc_num_burst_regs; i++) {
		if (!burst_reg_addr[i])
			continue;
		__raw_writel(next_timing->burst_regs[i], burst_reg_addr[i]);
	}
	if ((dram_type == DRAM_TYPE_LPDDR2) &&
	    (dram_over_temp_state != DRAM_OVER_TEMP_NONE))
		set_over_temp_timing(next_timing, dram_over_temp_state);
	wmb();
	barrier();

	/* On ddr3 when DLL is re-started predict MRS long wait count and
	   overwrite DFS table setting */
	if ((dram_type == DRAM_TYPE_DDR3) && (dll_change == DLL_CHANGE_ON))
		overwrite_mrs_wait_cnt(next_timing, zcal_long);

	/* the last read below makes sure prev writes are completed */
	qrst_used = need_qrst(next_timing, last_timing,
			      emc_readl(EMC_SEL_DPD_CTRL));

	/* 5. flow control marker 1 (no EMC read access after this) */
	emc_writel(1, EMC_STALL_BEFORE_CLKCHANGE);

	/* 6. enable periodic QRST */
	if (qrst_used)
		periodic_qrst_enable(emc_cfg_reg, emc_dbg_reg);

	/* 6.1 disable auto-refresh to save time after clock change */
	emc_writel(EMC_REFCTRL_DISABLE_ALL(dram_dev_num), EMC_REFCTRL);

	/* 7. turn Off dll and enter self-refresh on DDR3 */
	if (dram_type == DRAM_TYPE_DDR3) {
		if (dll_change == DLL_CHANGE_OFF)
			emc_writel(next_timing->emc_mode_1, EMC_EMRS);
		emc_writel(DRAM_BROADCAST(dram_dev_num) |
			   EMC_SELF_REF_CMD_ENABLED, EMC_SELF_REF);
	}

	/* 8. flow control marker 2 */
	emc_writel(1, EMC_STALL_AFTER_CLKCHANGE);

	/* 8.1 enable write mux, update unshadowed pad control */
	emc_writel(emc_dbg_reg | EMC_DBG_WRITE_MUX_ACTIVE, EMC_DBG);
	emc_writel(next_timing->burst_regs[EMC_XM2CLKPADCTRL_INDEX],
		   EMC_XM2CLKPADCTRL);

	/* 9. restore periodic QRST, and disable write mux */
	if ((qrst_used) || (next_timing->emc_periodic_qrst !=
			    last_timing->emc_periodic_qrst)) {
		emc_cfg_reg = next_timing->emc_periodic_qrst ?
			emc_cfg_reg | EMC_CFG_PERIODIC_QRST :
			emc_cfg_reg & (~EMC_CFG_PERIODIC_QRST);
		emc_writel(emc_cfg_reg, EMC_CFG);
	}
	emc_writel(emc_dbg_reg, EMC_DBG);

	/* 10. exit self-refresh on DDR3 */
	if (dram_type == DRAM_TYPE_DDR3)
		emc_writel(DRAM_BROADCAST(dram_dev_num), EMC_SELF_REF);

	/* 11. set dram mode registers */
	set_dram_mode(next_timing, last_timing, dll_change);

	/* 12. issue zcal command if turning zcal On */
	if (zcal_long) {
		emc_writel(EMC_ZQ_CAL_LONG_CMD_DEV0, EMC_ZQ_CAL);
		if (dram_dev_num > 1)
			emc_writel(EMC_ZQ_CAL_LONG_CMD_DEV1, EMC_ZQ_CAL);
	}

	/* 13. flow control marker 3 */
	emc_writel(1, EMC_UNSTALL_RW_AFTER_CLKCHANGE);

	/* 14. read any MC register to ensure the programming is done
	       change EMC clock source register (EMC read access restored)
	       wait for clk change completion */
	do_clock_change(clk_setting);

	/* 14.1 re-enable auto-refresh */
	emc_writel(EMC_REFCTRL_ENABLE_ALL(dram_dev_num), EMC_REFCTRL);

	/* 15. restore auto-cal */
	if (vref_cal_toggle)
		emc_writel(next_timing->emc_acal_interval,
			   EMC_AUTO_CAL_INTERVAL);

	/* 16. restore dynamic self-refresh */
	if (next_timing->rev >= 0x32)
		dyn_sref_enabled = next_timing->emc_dsr;
	if (dyn_sref_enabled) {
		emc_cfg_reg |= EMC_CFG_DYN_SREF_ENABLE;
		emc_writel(emc_cfg_reg, EMC_CFG);
	}

	/* 17. set zcal wait count */
	if (zcal_long)
		emc_writel(next_timing->emc_zcal_cnt_long, EMC_ZCAL_WAIT_CNT);

	/* 18. update restored timing */
	udelay(2);
	emc_timing_update();

	/* 18.a restore early ACK */
	mc_writel(mc_override, MC_EMEM_ARB_OVERRIDE);
}

static inline void emc_get_timing(struct tegra30_emc_table *timing)
{
	int i;

	for (i = 0; i < emc_num_burst_regs; i++) {
		if (burst_reg_addr[i])
			timing->burst_regs[i] = __raw_readl(burst_reg_addr[i]);
		else
			timing->burst_regs[i] = 0;
	}
	timing->emc_acal_interval = 0;
	timing->emc_zcal_cnt_long = 0;
	timing->emc_mode_reset = 0;
	timing->emc_mode_1 = 0;
	timing->emc_mode_2 = 0;
	timing->emc_periodic_qrst = (emc_readl(EMC_CFG) &
				     EMC_CFG_PERIODIC_QRST) ? 1 : 0;
}

/* After deep sleep EMC power features are not restored.
 * Do it at run-time after the 1st clock change.
 */
static inline void emc_cfg_power_restore(void)
{
	u32 reg = emc_readl(EMC_CFG);
	u32 pwr_mask = EMC_CFG_PWR_MASK;

	if (tegra_emc_table[0].rev >= 0x32)
		pwr_mask &= ~EMC_CFG_DYN_SREF_ENABLE;

	if ((reg ^ emc_cfg_saved) & pwr_mask) {
		reg = (reg & (~pwr_mask)) | (emc_cfg_saved & pwr_mask);
		emc_writel(reg, EMC_CFG);
		emc_timing_update();
	}
}

/* The EMC registers have shadow registers. When the EMC clock is updated
 * in the clock controller, the shadow registers are copied to the active
 * registers, allowing glitchless memory bus frequency changes.
 * This function updates the shadow registers for a new clock frequency,
 * and relies on the clock lock on the emc clock to avoid races between
 * multiple frequency changes */
static int emc_set_rate(unsigned long rate, bool use_backup)
{
	int i;
	u32 clk_setting;
	const struct tegra30_emc_table *last_timing;
	unsigned long flags;

	if (!tegra_emc_table)
		return -EINVAL;

	/* Table entries specify rate in kHz */
	rate = rate / 1000;

	for (i = 0; i < tegra_emc_table_size; i++) {
		if (tegra_emc_clk_sel[i].input == NULL)
			continue;	/* invalid entry */

		if (tegra_emc_table[i].rate == rate)
			break;
	}

	if (i >= tegra_emc_table_size)
		return -EINVAL;

	if (!emc_timing) {
		/* can not assume that boot timing matches dfs table even
		   if boot frequency matches one of the table nodes */
		emc_get_timing(&start_timing);
		last_timing = &start_timing;
	}
	else
		last_timing = emc_timing;

	clk_setting = use_backup ? emc->shared_bus_backup.value :
		tegra_emc_clk_sel[i].value;

	spin_lock_irqsave(&emc_access_lock, flags);
	emc_set_clock(&tegra_emc_table[i], last_timing, clk_setting);
	if (!emc_timing)
		emc_cfg_power_restore();
	emc_timing = &tegra_emc_table[i];
	spin_unlock_irqrestore(&emc_access_lock, flags);

	emc_last_stats_update(i);

	pr_debug("%s: rate %lu setting 0x%x\n", __func__, rate, clk_setting);

	return 0;
}

int tegra_emc_set_rate(unsigned long rate)
{
	return emc_set_rate(rate, false);
}

int tegra_emc_backup(unsigned long rate)
{
	BUG_ON(rate != emc->shared_bus_backup.bus_rate);
	return emc_set_rate(rate, true);
}

/* Select the closest EMC rate that is higher than the requested rate */
long tegra_emc_round_rate(unsigned long rate)
{
	int i;
	int best = -1;
	unsigned long distance = ULONG_MAX;

	if (!tegra_emc_table)
		return clk_get_rate_locked(emc); /* no table - no rate change */

	if (!emc_enable)
		return -EINVAL;

	pr_debug("%s: %lu\n", __func__, rate);

	/* Table entries specify rate in kHz */
	rate = rate / 1000;

	for (i = 0; i < tegra_emc_table_size; i++) {
		if (tegra_emc_clk_sel[i].input == NULL)
			continue;	/* invalid entry */

		if (tegra_emc_table[i].rate >= rate &&
		    (tegra_emc_table[i].rate - rate) < distance) {
			distance = tegra_emc_table[i].rate - rate;
			best = i;
		}
	}

	if (best < 0)
		return -EINVAL;

	pr_debug("%s: using %lu\n", __func__, tegra_emc_table[best].rate);

	return tegra_emc_table[best].rate * 1000;
}

struct clk *tegra_emc_predict_parent(unsigned long rate, u32 *div_value)
{
	int i;

	if (!tegra_emc_table)
		return ERR_PTR(-ENOENT);

	pr_debug("%s: %lu\n", __func__, rate);

	/* Table entries specify rate in kHz */
	rate = rate / 1000;

	for (i = 0; i < tegra_emc_table_size; i++) {
		if (tegra_emc_table[i].rate == rate) {
			struct clk *p = tegra_emc_clk_sel[i].input;

			*div_value = (tegra_emc_clk_sel[i].value &
				EMC_CLK_DIV_MASK) >> EMC_CLK_DIV_SHIFT;
			if (tegra_emc_clk_sel[i].input_rate != clk_get_rate(p))
				return NULL;

			return p;
		}
	}
	return ERR_PTR(-ENOENT);
}

int find_matching_input(unsigned long table_rate, bool mc_same_freq,
			struct emc_sel *emc_clk_sel, struct clk *cbus)
{
	u32 div_value = 0;
	unsigned long input_rate = 0;
	const struct clk_mux_sel *sel;
	const struct clk_mux_sel *parent_sel = NULL;
	const struct clk_mux_sel *backup_sel = NULL;

	/* Table entries specify rate in kHz */
	table_rate *= 1000;

	for (sel = emc->inputs; sel->input != NULL; sel++) {
		if (sel->input == emc->shared_bus_backup.input) {
			backup_sel = sel;
			continue;	/* skip backup souce */
		}

		if (sel->input == emc->parent)
			parent_sel = sel;

		input_rate = clk_get_rate(sel->input);

		if ((input_rate >= table_rate) &&
		     (input_rate % table_rate == 0)) {
			div_value = 2 * input_rate / table_rate - 2;
			break;
		}
	}

#ifdef CONFIG_TEGRA_PLLM_RESTRICTED
	/*
	 * When match not found, check if this rate can be backed-up by cbus
	 * Then, we will be able to re-lock boot parent PLLM, and use it as
	 * an undivided source. Backup is supported only on LPDDR2 platforms
	 * with restricted PLLM usage. Just one backup entry is recognized,
	 * and it must be between EMC maximum and half maximum rates.
	 */
	if ((dram_type == DRAM_TYPE_LPDDR2) && (sel->input == NULL) &&
	    (emc->shared_bus_backup.bus_rate == 0) && cbus) {
		BUG_ON(!parent_sel || !backup_sel);

		if ((table_rate == clk_round_rate(cbus, table_rate)) &&
		    (table_rate < clk_get_max_rate(emc)) &&
		    (table_rate >= clk_get_max_rate(emc) / 2)) {
			emc->shared_bus_backup.bus_rate = table_rate;

			/* Get ready emc clock backup selection settings */
			emc->shared_bus_backup.value =
				(backup_sel->value << EMC_CLK_SOURCE_SHIFT) |
				(cbus->div << EMC_CLK_DIV_SHIFT) |
				(mc_same_freq ? EMC_CLK_MC_SAME_FREQ : 0);

			/* Select undivided PLLM as regular source */
			sel = parent_sel;
			input_rate = table_rate;
			div_value = 0;
		}
	}
#endif

	if (sel->input) {
		emc_clk_sel->input = sel->input;
		emc_clk_sel->input_rate = input_rate;

		/* Get ready emc clock selection settings for this table rate */
		emc_clk_sel->value = sel->value << EMC_CLK_SOURCE_SHIFT;
		emc_clk_sel->value |= (div_value << EMC_CLK_DIV_SHIFT);
		if ((div_value == 0) && (emc_clk_sel->input == emc->parent))
			emc_clk_sel->value |= EMC_CLK_LOW_JITTER_ENABLE;
		if (mc_same_freq)
			emc_clk_sel->value |= EMC_CLK_MC_SAME_FREQ;
		return 0;
	}
	return -EINVAL;
}

static void adjust_emc_dvfs_table(const struct tegra30_emc_table *table,
				  int table_size)
{
	int i, j;
	unsigned long rate;

	if (table[0].rev < 0x33)
		return;

	for (i = 0; i < MAX_DVFS_FREQS; i++) {
		int mv = emc->dvfs->millivolts[i];
		if (!mv)
			break;

		/* For each dvfs voltage find maximum supported rate;
		   use 1MHz placeholder if not found */
		for (rate = 1000, j = 0; j < table_size; j++) {
			if (tegra_emc_clk_sel[j].input == NULL)
				continue;	/* invalid entry */

			if ((mv >= table[j].emc_min_mv) &&
			    (rate < table[j].rate))
				rate = table[j].rate;
		}
		/* Table entries specify rate in kHz */
		emc->dvfs->freqs[i] = rate * 1000;
	}
}

static bool is_emc_bridge(void)
{
	int mv;
	unsigned long rate;

	bridge = tegra_get_clock_by_name("bridge.emc");
	BUG_ON(!bridge);

	/* LPDDR2 does not need a bridge entry in DFS table: just lock bridge
	   rate at minimum so it won't interfere with emc bus operations */
	if (dram_type == DRAM_TYPE_LPDDR2) {
		clk_set_rate(bridge, 0);
		return true;
	}

	/* DDR3 requires EMC DFS table to include a bridge entry with frequency
	   above minimum bridge threshold, and voltage below bridge threshold */
	rate = clk_round_rate(bridge, TEGRA_EMC_BRIDGE_RATE_MIN);
	if (IS_ERR_VALUE(rate))
		return false;

	mv = tegra_dvfs_predict_millivolts(emc, rate);
	if (IS_ERR_VALUE(mv) || (mv > TEGRA_EMC_BRIDGE_MVOLTS_MIN))
		return false;

	if (clk_set_rate(bridge, rate))
		return false;

	return true;
}

static int tegra_emc_suspend_notify(struct notifier_block *nb,
				unsigned long event, void *data)
{
	if (event != PM_SUSPEND_PREPARE)
		return NOTIFY_OK;

	if (dram_type == DRAM_TYPE_DDR3) {
		if (clk_prepare_enable(bridge)) {
			pr_info("Tegra emc suspend:"
				" failed to enable bridge.emc\n");
			return NOTIFY_STOP;
		}
		pr_info("Tegra emc suspend: enabled bridge.emc\n");
	}
	return NOTIFY_OK;
};
static struct notifier_block tegra_emc_suspend_nb = {
	.notifier_call = tegra_emc_suspend_notify,
	.priority = 2,
};

static int tegra_emc_resume_notify(struct notifier_block *nb,
				unsigned long event, void *data)
{
	if (event != PM_POST_SUSPEND)
		return NOTIFY_OK;

	if (dram_type == DRAM_TYPE_DDR3) {
		clk_disable_unprepare(bridge);
		pr_info("Tegra emc resume: disabled bridge.emc\n");
	}
	return NOTIFY_OK;
};
static struct notifier_block tegra_emc_resume_nb = {
	.notifier_call = tegra_emc_resume_notify,
	.priority = -1,
};

static int tegra_emc_get_table_ns_per_tick(unsigned int emc_rate,
					unsigned int table_tick_len)
{
	unsigned int ns_per_tick = 0;
	unsigned int mc_period_10ns = 0;
	unsigned int reg;

	reg = mc_readl(MC_EMEM_ARB_MISC0) & MC_EMEM_ARB_MISC0_EMC_SAME_FREQ;

	mc_period_10ns = ((reg ? (NSEC_PER_MSEC * 10) : (20 * NSEC_PER_MSEC)) /
			(emc_rate));
	ns_per_tick = ((table_tick_len & MC_EMEM_ARB_CFG_CYCLE_MASK)
		* mc_period_10ns) / (10 *
		(1 + ((table_tick_len & MC_EMEM_ARB_CFG_EXTRA_TICK_MASK)
		>> MC_EMEM_ARB_CFG_EXTRA_TICK_SHIFT)));

	/* round new_ns_per_tick to 30/60 */
	if (ns_per_tick < 45)
		ns_per_tick = 30;
	else
		ns_per_tick = 60;

	return ns_per_tick;
}

#ifdef CONFIG_OF
static struct device_node *tegra_emc_ramcode_devnode(struct device_node *np)
{
	struct device_node *iter;
	u32 reg;

	for_each_child_of_node(np, iter) {
		if (of_property_read_u32(np, "nvidia,ram-code", &reg))
			continue;
		if (reg == tegra_bct_strapping)
			return of_node_get(iter);
	}

	return NULL;
}

static struct tegra30_emc_pdata *tegra_emc_dt_parse_pdata(
		struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct device_node *tnp, *iter;
	struct tegra30_emc_pdata *pdata;
	int ret, i, num_tables;

	if (!np)
		return NULL;

	if (of_find_property(np, "nvidia,use-ram-code", NULL)) {
		tnp = tegra_emc_ramcode_devnode(np);
		if (!tnp)
			dev_warn(&pdev->dev,
				"can't find emc table for ram-code 0x%02x\n",
					tegra_bct_strapping);
	} else
		tnp = of_node_get(np);

	if (!tnp)
		return NULL;

	num_tables = 0;
	for_each_child_of_node(tnp, iter)
		if (of_device_is_compatible(iter, "nvidia,tegra30-emc-table"))
			num_tables++;

	if (!num_tables) {
		pdata = NULL;
		goto out;
	}

	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	pdata->tables = devm_kzalloc(&pdev->dev,
				sizeof(*pdata->tables) * num_tables,
					GFP_KERNEL);

	i = 0;
	for_each_child_of_node(tnp, iter) {
		u32 u;
		int num_burst_regs;
		struct property *prop;

		ret = of_property_read_u32(iter, "nvidia,revision", &u);
		if (ret) {
			dev_err(&pdev->dev, "no revision in %s\n",
				iter->full_name);
			continue;
		}
		pdata->tables[i].rev = u;

		ret = of_property_read_u32(iter, "clock-frequency", &u);
		if (ret) {
			dev_err(&pdev->dev, "no clock-frequency in %s\n",
				iter->full_name);
			continue;
		}
		pdata->tables[i].rate = u;

		prop = of_find_property(iter, "nvidia,emc-registers", NULL);
		if (!prop)
			continue;

		num_burst_regs = prop->length / sizeof(u);

		ret = of_property_read_u32_array(iter, "nvidia,emc-registers",
						pdata->tables[i].burst_regs,
							num_burst_regs);
		if (ret) {
			dev_err(&pdev->dev,
				"malformed emc-registers property in %s\n",
				iter->full_name);
			continue;
		}

		of_property_read_u32(iter, "nvidia,emc-zcal-cnt-long",
					&pdata->tables[i].emc_zcal_cnt_long);
		of_property_read_u32(iter, "nvidia,emc-acal-interval",
					&pdata->tables[i].emc_acal_interval);
		of_property_read_u32(iter, "nvidia,emc-periodic-qrst",
					&pdata->tables[i].emc_periodic_qrst);
		of_property_read_u32(iter, "nvidia,emc-mode-reset",
					&pdata->tables[i].emc_mode_reset);
		of_property_read_u32(iter, "nvidia,emc-mode-1",
					&pdata->tables[i].emc_mode_1);
		of_property_read_u32(iter, "nvidia,emc-mode-2",
					&pdata->tables[i].emc_mode_2);
		of_property_read_u32(iter, "nvidia,emc-dsr",
					&pdata->tables[i].emc_dsr);

		ret = of_property_read_u32(iter, "nvidia,emc-min-mv", &u);
		if (!ret)
			pdata->tables[i].emc_min_mv = u;

		i++;
	}
	pdata->num_tables = i;

out:
	of_node_put(tnp);
	return pdata;
}
#else
static struct tegra30_emc_pdata *tegra_emc_dt_parse_pdata(
					struct platform_device *pdev)
{
	return NULL;
}
#endif

static int __devinit tegra30_emc_probe(struct platform_device *pdev)
{
	int i, mv;
	u32 reg;
	bool max_entry = false;
	unsigned long boot_rate, max_rate;
	struct clk *cbus = tegra_get_clock_by_name("cbus");
	unsigned int ns_per_tick = 0;
	unsigned int cur_ns_per_tick = 0;
	struct tegra30_emc_pdata *pdata;
	struct resource *res;

	if (tegra_emc_table)
		return -EINVAL;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "missing register base\n");
		return -ENOMEM;
	}

	pdata = pdev->dev.platform_data;

	if (!pdata)
		pdata = tegra_emc_dt_parse_pdata(pdev);

	pdev->dev.platform_data = pdata;

	emc_stats.clkchange_count = 0;
	spin_lock_init(&emc_stats.spinlock);
	emc_stats.last_update = get_jiffies_64();
	emc_stats.last_sel = TEGRA_EMC_TABLE_MAX_SIZE;

	boot_rate = clk_get_rate(emc) / 1000;
	max_rate = clk_get_max_rate(emc) / 1000;

	if ((dram_type != DRAM_TYPE_DDR3) && (dram_type != DRAM_TYPE_LPDDR2)) {
		pr_err("tegra: not supported DRAM type %u\n", dram_type);
		return -ENODATA;
	}

	if (emc->parent != tegra_get_clock_by_name("pll_m")) {
		pr_err("tegra: boot parent %s is not supported by EMC DFS\n",
			emc->parent->name);
		return -ENODATA;
	}

	if (!pdata || !pdata->tables || !pdata->num_tables) {
		pr_err("tegra: EMC DFS table is empty\n");
		return -ENODATA;
	}

	tegra_emc_table_size = min(pdata->num_tables, TEGRA_EMC_TABLE_MAX_SIZE);
	switch (pdata->tables[0].rev) {
	case 0x30:
		emc_num_burst_regs = 105;
		break;
	case 0x31:
	case 0x32:
	case 0x33:
		emc_num_burst_regs = 107;
		break;
	default:
		pr_err("tegra: invalid EMC DFS table: unknown rev 0x%x\n",
			pdata->tables[0].rev);
		return -ENODATA;
	}

	/* Match EMC source/divider settings with table entries */
	for (i = 0; i < tegra_emc_table_size; i++) {
		bool mc_same_freq = MC_EMEM_ARB_MISC0_EMC_SAME_FREQ &
			pdata->tables[i].burst_regs[MC_EMEM_ARB_MISC0_INDEX];
		unsigned long table_rate = pdata->tables[i].rate;
		if (!table_rate)
			continue;

		BUG_ON(pdata->tables[i].rev != pdata->tables[0].rev);

		if (find_matching_input(table_rate, mc_same_freq,
					&tegra_emc_clk_sel[i], cbus))
			continue;

		if (table_rate == boot_rate)
			emc_stats.last_sel = i;

		if (table_rate == max_rate)
			max_entry = true;

		cur_ns_per_tick = tegra_emc_get_table_ns_per_tick(table_rate,
			pdata->tables[i].burst_regs[MC_EMEM_ARB_CFG_INDEX]);

		if (ns_per_tick == 0) {
			ns_per_tick = cur_ns_per_tick;
		} else if (ns_per_tick != cur_ns_per_tick) {
			pr_err("tegra: invalid EMC DFS table: "
				"mismatched DFS tick lengths "
				"within table!\n");
			ns_per_tick = 0;
			return -EINVAL;
		}
	}

	/* Validate EMC rate and voltage limits */
	if (!max_entry) {
		pr_err("tegra: invalid EMC DFS table: entry for max rate"
		       " %lu kHz is not found\n", max_rate);
		return -EINVAL;
	}

	tegra_latency_allowance_update_tick_length(ns_per_tick);

	tegra_emc_table = pdata->tables;

	adjust_emc_dvfs_table(tegra_emc_table, tegra_emc_table_size);
	mv = tegra_dvfs_predict_millivolts(emc, max_rate * 1000);
	if ((mv <= 0) || (mv > emc->dvfs->max_millivolts)) {
		tegra_emc_table = NULL;
		pr_err("tegra: invalid EMC DFS table: maximum rate %lu kHz does"
		       " not match nominal voltage %d\n",
				max_rate, emc->dvfs->max_millivolts);
		return -ENODATA;
	}

	if (!is_emc_bridge()) {
		tegra_emc_table = NULL;
		pr_err("tegra: invalid EMC DFS table: emc bridge not found");
		return -ENODATA;
	}
	pr_info("tegra: validated EMC DFS table\n");

	/* Configure clock change mode according to dram type */
	reg = emc_readl(EMC_CFG_2) & (~EMC_CFG_2_MODE_MASK);
	reg |= ((dram_type == DRAM_TYPE_LPDDR2) ? EMC_CFG_2_PD_MODE :
		EMC_CFG_2_SREF_MODE) << EMC_CFG_2_MODE_SHIFT;
	emc_writel(reg, EMC_CFG_2);

	register_pm_notifier(&tegra_emc_suspend_nb);
	register_pm_notifier(&tegra_emc_resume_nb);

	return 0;
}

static struct of_device_id tegra30_emc_of_match[] __devinitdata = {
	{ .compatible = "nvidia,tegra30-emc", },
	{ },
};

static struct platform_driver tegra30_emc_driver = {
	.driver = {
		.name = "tegra-emc",
		.owner = THIS_MODULE,
		.of_match_table = tegra30_emc_of_match,
	},
	.probe = tegra30_emc_probe,
};

int __init tegra30_init_emc(void)
{
	return platform_driver_register(&tegra30_emc_driver);
}

void tegra_emc_timing_invalidate(void)
{
	emc_timing = NULL;
}

void tegra_init_dram_bit_map(const u32 *bit_map, int map_size)
{
	BUG_ON(map_size != 32);
	dram_to_soc_bit_map = bit_map;
}

void tegra_emc_dram_type_init(struct clk *c)
{
	emc = c;

	dram_type = (emc_readl(EMC_FBIO_CFG5) &
		     EMC_CFG5_TYPE_MASK) >> EMC_CFG5_TYPE_SHIFT;
	if (dram_type == DRAM_TYPE_DDR3)
		emc->min_rate = EMC_MIN_RATE_DDR3;

	dram_dev_num = (mc_readl(MC_EMEM_ADR_CFG) & 0x1) + 1; /* 2 dev max */
	emc_cfg_saved = emc_readl(EMC_CFG);
}

int tegra_emc_get_dram_type(void)
{
	return dram_type;
}

static u32 soc_to_dram_bit_swap(u32 soc_val, u32 dram_mask, u32 dram_shift)
{
	int bit;
	u32 dram_val = 0;

	/* tegra clocks definitions use shifted mask always */
	if (!dram_to_soc_bit_map)
		return soc_val & dram_mask;

	for (bit = dram_shift; bit < 32; bit++) {
		u32 dram_bit_mask = 0x1 << bit;
		u32 soc_bit_mask = dram_to_soc_bit_map[bit];

		if (!(dram_bit_mask & dram_mask))
			break;

		if (soc_bit_mask & soc_val)
			dram_val |= dram_bit_mask;
	}

	return dram_val;
}

static int emc_read_mrr(int dev, int addr)
{
	int ret;
	u32 val;

	if (dram_type != DRAM_TYPE_LPDDR2)
		return -ENODEV;

	ret = wait_for_update(EMC_STATUS, EMC_STATUS_MRR_DIVLD, false);
	if (ret)
		return ret;

	val = dev ? DRAM_DEV_SEL_1 : DRAM_DEV_SEL_0;
	val |= (addr << EMC_MRR_MA_SHIFT) & EMC_MRR_MA_MASK;
	emc_writel(val, EMC_MRR);

	ret = wait_for_update(EMC_STATUS, EMC_STATUS_MRR_DIVLD, true);
	if (ret)
		return ret;

	val = emc_readl(EMC_MRR) & EMC_MRR_DATA_MASK;
	return val;
}

int tegra_emc_get_dram_temperature(void)
{
	int mr4;
	unsigned long flags;

	spin_lock_irqsave(&emc_access_lock, flags);

	mr4 = emc_read_mrr(0, 4);
	if (IS_ERR_VALUE(mr4)) {
		spin_unlock_irqrestore(&emc_access_lock, flags);
		return mr4;
	}
	spin_unlock_irqrestore(&emc_access_lock, flags);

	mr4 = soc_to_dram_bit_swap(
		mr4, LPDDR2_MR4_TEMP_MASK, LPDDR2_MR4_TEMP_SHIFT);
	return mr4;
}

int tegra_emc_set_over_temp_state(unsigned long state)
{
	unsigned long flags;

	if (dram_type != DRAM_TYPE_LPDDR2)
		return -ENODEV;

	spin_lock_irqsave(&emc_access_lock, flags);

	/* Update refresh timing if state changed */
	if (emc_timing && (dram_over_temp_state != state)) {
		set_over_temp_timing(emc_timing, state);
		emc_timing_update();
		if (state != DRAM_OVER_TEMP_NONE)
			emc_writel(EMC_REF_FORCE_CMD, EMC_REF);
		dram_over_temp_state = state;
	}
	spin_unlock_irqrestore(&emc_access_lock, flags);
	return 0;
}

/* non-zero state value will reduce eack_disable_refcnt */
static int tegra_emc_set_eack_state(unsigned long state)
{
	unsigned long flags;
	u32 mc_override;
	static int eack_disable_refcnt = 0;

	spin_lock_irqsave(&emc_access_lock, flags);

	/*
	 * refcnt > 0 implies there is at least one client requiring eack
	 * disabled. refcnt of 0 implies eack is enabled
	 */
	if (eack_disable_refcnt == 1 && state) {
		mc_override = mc_readl(MC_EMEM_ARB_OVERRIDE);
		enable_early_ack(mc_override);
	} else if (eack_disable_refcnt == 0 && !state) {
		mc_override = mc_readl(MC_EMEM_ARB_OVERRIDE);
		disable_early_ack(mc_override);
	}

	if (state) {
		if (likely(eack_disable_refcnt > 0)) {
			--eack_disable_refcnt;
		} else {
			pr_err("%s: Ignored a request to underflow eack "
				"disable reference counter\n",__func__);
			dump_stack();
		}
	} else {
		++eack_disable_refcnt;
	}

	spin_unlock_irqrestore(&emc_access_lock, flags);
	return 0;
}

int tegra_emc_enable_eack(void) {
	return tegra_emc_set_eack_state(1);
}

int tegra_emc_disable_eack(void) {
	return tegra_emc_set_eack_state(0);
}

#ifdef CONFIG_DEBUG_FS

static struct dentry *emc_debugfs_root;

static int emc_stats_show(struct seq_file *s, void *data)
{
	int i;

	emc_last_stats_update(TEGRA_EMC_TABLE_MAX_SIZE);

	seq_printf(s, "%-10s %-10s \n", "rate kHz", "time");
	for (i = 0; i < tegra_emc_table_size; i++) {
		if (tegra_emc_clk_sel[i].input == NULL)
			continue;	/* invalid entry */

		seq_printf(s, "%-10lu %-10llu \n", tegra_emc_table[i].rate,
			   cputime64_to_clock_t(emc_stats.time_at_clock[i]));
	}
	seq_printf(s, "%-15s %llu\n", "transitions:",
		   emc_stats.clkchange_count);
	seq_printf(s, "%-15s %llu\n", "time-stamp:",
		   cputime64_to_clock_t(emc_stats.last_update));

	return 0;
}

static int emc_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, emc_stats_show, inode->i_private);
}

static const struct file_operations emc_stats_fops = {
	.open		= emc_stats_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int dram_temperature_get(void *data, u64 *val)
{
	*val = tegra_emc_get_dram_temperature();
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(dram_temperature_fops, dram_temperature_get,
			NULL, "%lld\n");

static int over_temp_state_get(void *data, u64 *val)
{
	*val = dram_over_temp_state;
	return 0;
}
static int over_temp_state_set(void *data, u64 val)
{
	tegra_emc_set_over_temp_state(val);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(over_temp_state_fops, over_temp_state_get,
			over_temp_state_set, "%llu\n");

static int eack_state_get(void *data, u64 *val)
{
	unsigned long flags;
	u32 mc_override;

	spin_lock_irqsave(&emc_access_lock, flags);
	mc_override = mc_readl(MC_EMEM_ARB_OVERRIDE);
	spin_unlock_irqrestore(&emc_access_lock, flags);

	*val = (mc_override & MC_EMEM_ARB_OVERRIDE_EACK_MASK);
	return 0;
}

static int eack_state_set(void *data, u64 val)
{
	tegra_emc_set_eack_state(val);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(eack_state_fops, eack_state_get,
			eack_state_set, "%llu\n");

static int efficiency_get(void *data, u64 *val)
{
	*val = tegra_emc_bw_efficiency;
	return 0;
}
static int efficiency_set(void *data, u64 val)
{
	tegra_emc_bw_efficiency = (val > 100) ? 100 : val;
	if (emc)
		tegra_clk_shared_bus_update(emc);

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(efficiency_fops, efficiency_get,
			efficiency_set, "%llu\n");

static int efficiency_boost_get(void *data, u64 *val)
{
	*val = tegra_emc_bw_efficiency_boost;
	return 0;
}
static int efficiency_boost_set(void *data, u64 val)
{
	tegra_emc_bw_efficiency_boost = (val > 100) ? 100 : val;
	if (emc)
		tegra_clk_shared_bus_update(emc);

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(efficiency_boost_fops, efficiency_boost_get,
			efficiency_boost_set, "%llu\n");

static int __init tegra_emc_debug_init(void)
{
	if (!tegra_emc_table)
		return 0;

	emc_debugfs_root = debugfs_create_dir("tegra_emc", NULL);
	if (!emc_debugfs_root)
		return -ENOMEM;

	if (!debugfs_create_file(
		"stats", S_IRUGO, emc_debugfs_root, NULL, &emc_stats_fops))
		goto err_out;

	if (!debugfs_create_file("dram_temperature", S_IRUGO, emc_debugfs_root,
				 NULL, &dram_temperature_fops))
		goto err_out;

	if (!debugfs_create_file("over_temp_state", S_IRUGO | S_IWUSR,
				 emc_debugfs_root, NULL, &over_temp_state_fops))
		goto err_out;

	if (!debugfs_create_file(
		"eack_state", S_IRUGO | S_IWUSR, emc_debugfs_root, NULL, &eack_state_fops))
		goto err_out;

	if (!debugfs_create_file("efficiency", S_IRUGO | S_IWUSR,
				 emc_debugfs_root, NULL, &efficiency_fops))
		goto err_out;

	if (!debugfs_create_file("efficiency_boost", S_IRUGO | S_IWUSR,
				 emc_debugfs_root, NULL, &efficiency_boost_fops))
		goto err_out;

	return 0;

err_out:
	debugfs_remove_recursive(emc_debugfs_root);
	return -ENOMEM;
}

late_initcall(tegra_emc_debug_init);
#endif
