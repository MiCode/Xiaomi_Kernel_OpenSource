/*
 * arch/arm/mach-tegra/tegra14_emc.c
 *
 * Copyright (c) 2013, NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
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
#include <linux/platform_device.h>
#include <linux/platform_data/tegra_emc.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/hrtimer.h>
#include <linux/pasr.h>

#include <asm/cputime.h>

#include "clock.h"
#include "dvfs.h"
#include "board.h"
#include "iomap.h"
#include "tegra14_emc.h"

#ifdef CONFIG_TEGRA_EMC_SCALING_ENABLE
static bool emc_enable = true;
#else
static bool emc_enable;
#endif
module_param(emc_enable, bool, 0644);

u8 tegra_emc_bw_efficiency = 100;
static struct emc_iso_usage tegra14_emc_iso_usage[] = {
	{ BIT(EMC_USER_DC1),				80 },
	{ BIT(EMC_USER_DC2),				80 },
	{ BIT(EMC_USER_DC1) | BIT(EMC_USER_DC2),	50 },
	{ BIT(EMC_USER_DC1) | BIT(EMC_USER_VI),		50 },
	{ BIT(EMC_USER_DC2) | BIT(EMC_USER_VI),		50 },
	{ BIT(EMC_USER_BB),	50 },
};

#define PLL_C_DIRECT_FLOOR		333500000
#define EMC_STATUS_UPDATE_TIMEOUT	100
#define TEGRA_EMC_TABLE_MAX_SIZE	16

#define TEGRA_EMC_MODE_REG_17	0x00110000
#define TEGRA_EMC_MRW_DEV_SHIFT	30
#define TEGRA_EMC_MRW_DEV1	2
#define TEGRA_EMC_MRW_DEV2	1

#define TEGRA_MC_EMEM_ADR_CFG_DEV	0x58
#define TEGRA_EMEM_DEV_DEVSIZE_SHIFT	16
#define TEGRA_EMEM_DEV_DEVSIZE_MASK	0xF

enum {
	DLL_CHANGE_NONE = 0,
	DLL_CHANGE_ON,
	DLL_CHANGE_OFF,
};

#define CLK_RST_CONTROLLER_CLK_SOURCE_EMC	0x19c
#define EMC_CLK_DIV_SHIFT		0
#define EMC_CLK_DIV_MASK		(0xFF << EMC_CLK_DIV_SHIFT)
#define EMC_CLK_SOURCE_SHIFT		29
#define EMC_CLK_SOURCE_MASK		(0x7 << EMC_CLK_SOURCE_SHIFT)
#define EMC_CLK_LOW_JITTER_ENABLE	(0x1 << 31)
#define EMC_CLK_FORCE_CC_TRIGGER	(0x1 << 27)
#define	EMC_CLK_MC_SAME_FREQ		(0x1 << 16)

#define PMC_IO_DPD2_REQ			0x1C0
#define PMC_IO_DPD2_REQ_CODE_SHIFT	30
#define PMC_IO_DPD2_REQ_CODE_DPD_OFF	0x1
#define PMC_IO_DPD2_REQ_CODE_DPD_ON	0x2
#define PMC_IO_DPD2_REQ_DISC_BIAS	(0x1 << 27)

/* FIXME: actual Tegar14 list */
#define BURST_REG_LIST \
	DEFINE_REG(TEGRA_EMC_BASE, EMC_RC),			\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_RFC),			\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_RFC_SLR),		\
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
	DEFINE_REG(TEGRA_EMC_BASE, EMC_WDV_MASK),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_QUSE_WIDTH),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_IBDLY),			\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_PUTERM),			\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_PUTERM_WIDTH),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_CDB_CNTL_2),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_QRST),			\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_RDV_MASK),		\
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
	DEFINE_REG(TEGRA_EMC_BASE, EMC_TCKESR),			\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_TPD),			\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_TFAW),			\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_TRPAB),			\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_TCLKSTABLE),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_TCLKSTOP),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_TREFBW),			\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_ODT_WRITE),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_ODT_READ),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_FBIO_CFG5),		\
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
	DEFINE_REG(TEGRA_EMC_BASE, EMC_DLL_XFORM_ADDR0),	\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_DLL_XFORM_ADDR1),	\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_DLL_XFORM_ADDR2),	\
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
	DEFINE_REG(TEGRA_EMC_BASE, EMC_XM2CMDPADCTRL4),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_XM2DQSPADCTRL2),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_XM2DQPADCTRL2),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_XM2CLKPADCTRL),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_XM2CLKPADCTRL2),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_XM2COMPPADCTRL),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_XM2VTTGENPADCTRL),	\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_XM2DQSPADCTRL3),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_XM2DQSPADCTRL4),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_DSR_VTTGEN_DRV),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_TXDSRVTTGEN),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_FBIO_SPARE),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_CTT_TERM_CTRL),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_ZCAL_INTERVAL),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_ZCAL_WAIT_CNT),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_MRS_WAIT_CNT),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_MRS_WAIT_CNT2),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_AUTO_CAL_CONFIG2),	\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_AUTO_CAL_CONFIG3),	\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_AUTO_CAL_CONFIG),	\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_CTT),			\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_CTT_DURATION),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_DYN_SELF_REF_CONTROL),	\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_QUSE),			\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_EINPUT),			\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_EINPUT_DURATION),	\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_QSAFE),			\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_RDV),			\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_FBIO_CFG6),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_PIPE_MACRO_CTL),		\
	DEFINE_REG(TEGRA_EMC_BASE, EMC_QPOP),			\
	DEFINE_REG(TEGRA_MC_BASE, MC_EMEM_ARB_CFG),		\
	DEFINE_REG(TEGRA_MC_BASE, MC_EMEM_ARB_OUTSTANDING_REQ),	\
	DEFINE_REG(TEGRA_MC_BASE, MC_EMEM_ARB_OUTSTANDING_REQ_RING3),	\
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
	DEFINE_REG(TEGRA_MC_BASE, MC_EMEM_ARB_RING3_THROTTLE),	\

#define BURST_UP_DOWN_REG_LIST \
	DEFINE_REG(TEGRA_MC_BASE, MC_PTSA_GRANT_DECREMENT),	\
	DEFINE_REG(TEGRA_MC_BASE, MC_LATENCY_ALLOWANCE_G2_0),	\
	DEFINE_REG(TEGRA_MC_BASE, MC_LATENCY_ALLOWANCE_G2_1),	\
	DEFINE_REG(TEGRA_MC_BASE, MC_LATENCY_ALLOWANCE_NV_0),	\
	DEFINE_REG(TEGRA_MC_BASE, MC_LATENCY_ALLOWANCE_NV2_0),	\
	DEFINE_REG(TEGRA_MC_BASE, MC_LATENCY_ALLOWANCE_NV_1),	\
	DEFINE_REG(TEGRA_MC_BASE, MC_LATENCY_ALLOWANCE_NV2_1),	\
	DEFINE_REG(TEGRA_MC_BASE, MC_LATENCY_ALLOWANCE_EPP_0),	\
	DEFINE_REG(TEGRA_MC_BASE, MC_LATENCY_ALLOWANCE_EPP_1),

#define DEFINE_REG(base, reg) ((base) ? (IO_ADDRESS((base)) + (reg)) : 0)
static void __iomem *burst_reg_addr[TEGRA14_EMC_MAX_NUM_REGS] = {
	BURST_REG_LIST
};
#ifndef EMULATE_CLOCK_SWITCH
static void __iomem *burst_up_down_reg_addr[TEGRA14_EMC_MAX_NUM_REGS] = {
	BURST_UP_DOWN_REG_LIST
};
#endif
#undef DEFINE_REG

#define DEFINE_REG(base, reg)	reg##_INDEX
enum {
	BURST_REG_LIST
};
#undef DEFINE_REG

#define CLK_RST_CONTROLLER_CLK_OUT_ENB_X_SET	0x284
#define CLK_RST_CONTROLLER_CLK_OUT_ENB_X_CLR	0x288
#define CLK_ENB_EMC_DLL				(1 << 14)
#define CLK_RST_CONTROLLER_CLK_SOURCE_EMC_DLL	0x664
#define EMC_DLL_DYN_MUX_CTRL			(1 << 16)

#define FBIO_SPARE_CFG_SW_DLL_RST_CTRL_N	(1 << 27)

struct emc_sel {
	struct clk	*input;
	u32		value;
	unsigned long	input_rate;
};
static struct emc_sel tegra_emc_clk_sel[TEGRA_EMC_TABLE_MAX_SIZE];
static struct tegra14_emc_table start_timing;
static const struct tegra14_emc_table *emc_timing;
static unsigned long dram_over_temp_state = DRAM_OVER_TEMP_NONE;

static ktime_t clkchange_time;
static int clkchange_delay = 100;

static const struct tegra14_emc_table *tegra_emc_table;
static const struct tegra14_emc_table *tegra_emc_table_derated;
static int tegra_emc_table_size;

static u32 dram_dev_num;
static u32 dram_type = -1;

static u32 dsr_override;

static int pasr_enable;

static struct clk *emc;

static struct {
	cputime64_t time_at_clock[TEGRA_EMC_TABLE_MAX_SIZE];
	int last_sel;
	u64 last_update;
	u64 clkchange_count;
	spinlock_t spinlock;
} emc_stats;

static DEFINE_SPINLOCK(emc_access_lock);

static void __iomem *emc_base = IO_ADDRESS(TEGRA_EMC_BASE);
/* !!!FIXME!!! Need clean up for T148 */
#ifdef CONFIG_ARCH_TEGRA_11x_SOC
static void __iomem *emc0_base = IO_ADDRESS(TEGRA_EMC0_BASE);
static void __iomem *emc1_base = IO_ADDRESS(TEGRA_EMC1_BASE);
#endif
static void __iomem *mc_base = IO_ADDRESS(TEGRA_MC_BASE);
static void __iomem *clk_base = IO_ADDRESS(TEGRA_CLK_RESET_BASE);
static void __iomem *pmc_base = IO_ADDRESS(TEGRA_PMC_BASE);

static inline void emc_writel(u32 val, unsigned long addr)
{
	writel(val, emc_base + addr);
}
/* !!!FIXME!!! Need clean up for T148 */
#ifdef CONFIG_ARCH_TEGRA_11x_SOC
static inline void emc0_writel(u32 val, unsigned long addr)
{
	writel(val, emc0_base + addr);
}
static inline void emc1_writel(u32 val, unsigned long addr)
{
	writel(val, emc1_base + addr);
}
#endif
static inline u32 emc_readl(unsigned long addr)
{
	return readl(emc_base + addr);
}
static inline void mc_writel(u32 val, unsigned long addr)
{
	writel(val, mc_base + addr);
}
static inline u32 mc_readl(unsigned long addr)
{
	return readl(mc_base + addr);
}

static inline void ccfifo_writel(u32 val, unsigned long addr)
{
	writel(val, emc_base + EMC_CCFIFO_DATA);
	writel(addr, emc_base + EMC_CCFIFO_ADDR);
}

static int last_round_idx;
static inline int get_start_idx(unsigned long rate)
{
	if (tegra_emc_table[last_round_idx].rate == rate)
		return last_round_idx;
	return 0;
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
	const struct tegra14_emc_table *next_timing, unsigned long state)
{
#define REFRESH_X2	1
#define REFRESH_X4	2
#define REFRESH_SPEEDUP(val, speedup)				\
	do {							\
		val = ((val) & 0xFFFF0000) |			\
			(((val) & 0xFFFF) >> (speedup));	\
	} while (0)

	u32 ref = next_timing->burst_regs[EMC_REFRESH_INDEX];
	u32 pre_ref = next_timing->burst_regs[EMC_PRE_REFRESH_REQ_CNT_INDEX];
	u32 dsr_cntrl = next_timing->burst_regs[EMC_DYN_SELF_REF_CONTROL_INDEX];

	switch (state) {
	case DRAM_OVER_TEMP_NONE:
		break;
	case DRAM_OVER_TEMP_REFRESH_X2:
		REFRESH_SPEEDUP(ref, REFRESH_X2);
		REFRESH_SPEEDUP(pre_ref, REFRESH_X2);
		REFRESH_SPEEDUP(dsr_cntrl, REFRESH_X2);
		break;
	case DRAM_OVER_TEMP_REFRESH_X4:
	case DRAM_OVER_TEMP_THROTTLE:
		REFRESH_SPEEDUP(ref, REFRESH_X4);
		REFRESH_SPEEDUP(pre_ref, REFRESH_X4);
		REFRESH_SPEEDUP(dsr_cntrl, REFRESH_X4);
		break;
	default:
		WARN(1, "%s: Failed to set dram over temp state %lu\n",
		     __func__, state);
		return;
	}

	__raw_writel(ref, burst_reg_addr[EMC_REFRESH_INDEX]);
	__raw_writel(pre_ref, burst_reg_addr[EMC_PRE_REFRESH_REQ_CNT_INDEX]);
	__raw_writel(dsr_cntrl, burst_reg_addr[EMC_DYN_SELF_REF_CONTROL_INDEX]);
}

static inline bool dqs_preset(const struct tegra14_emc_table *next_timing,
			      const struct tegra14_emc_table *last_timing)
{
	bool ret = false;

#define DQS_SET(reg, bit)						\
	do {								\
		if ((next_timing->burst_regs[EMC_##reg##_INDEX] &	\
		     EMC_##reg##_##bit##_ENABLE) &&			\
		    (!(last_timing->burst_regs[EMC_##reg##_INDEX] &	\
		       EMC_##reg##_##bit##_ENABLE)))   {		\
			emc_writel(last_timing->burst_regs[EMC_##reg##_INDEX] \
				   | EMC_##reg##_##bit##_ENABLE, EMC_##reg); \
			ret = true;					\
		}							\
	} while (0)

	DQS_SET(XM2DQSPADCTRL2, RX_FT_REC);
	DQS_SET(XM2DQSPADCTRL2, VREF_DQ);

	return ret;
}

static inline int get_dll_change(const struct tegra14_emc_table *next_timing,
				 const struct tegra14_emc_table *last_timing)
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

static inline void set_dram_mode(const struct tegra14_emc_table *next_timing,
				 const struct tegra14_emc_table *last_timing,
				 int dll_change)
{
	/* first mode_2, then mode_1; mode_reset is not applicable */
	if (next_timing->emc_mode_2 != last_timing->emc_mode_2)
		ccfifo_writel(next_timing->emc_mode_2, EMC_MRW2);
	if (next_timing->emc_mode_1 != last_timing->emc_mode_1)
		ccfifo_writel(next_timing->emc_mode_1, EMC_MRW);
	if (next_timing->emc_mode_4 != last_timing->emc_mode_4)
		ccfifo_writel(next_timing->emc_mode_4, EMC_MRW4);
}

static inline void do_clock_change(u32 clk_setting)
{
	int err;

	mc_readl(MC_EMEM_ADR_CFG);	/* completes prev writes */
	writel(clk_setting, clk_base + emc->reg);
	readl(clk_base + emc->reg);/* completes prev write */

	err = wait_for_update(EMC_INTSTATUS,
			      EMC_INTSTATUS_CLKCHANGE_COMPLETE, true);
	if (err) {
		pr_err("%s: clock change completion error: %d", __func__, err);
		BUG();
	}
}

static u32 emc_prelock_dll(const struct tegra14_emc_table *last_timing,
			   const struct tegra14_emc_table *next_timing)
{
	u32 dll_locked, dll_out;
	u32 emc_cfg_dig_dll;
	u32 emc_dll_clk_src;
	u32 fbio_spare_old, fbio_spare_new;
	u32 div_value = (next_timing->src_sel_reg & EMC_CLK_DIV_MASK);
	u32 src_value = (next_timing->src_sel_reg & EMC_CLK_SOURCE_MASK);

	/* WAR for missing PLLC_UD DLL clock source selector. */
	if (src_value == (0x7 << EMC_CLK_SOURCE_SHIFT))
		src_value = (0x1 << EMC_CLK_SOURCE_SHIFT);

	/*
	 * Step 1:
	 *   If the DLL is disabled don't bother disabling it again.
	 */
	emc_cfg_dig_dll = emc_readl(EMC_CFG_DIG_DLL);

	if (last_timing->rate <= 408000)
		goto skip_dll_disable;

	emc_cfg_dig_dll &= ~EMC_CFG_DIG_DLL_EN;
	emc_writel(emc_cfg_dig_dll, EMC_CFG_DIG_DLL);
	emc_timing_update();

skip_dll_disable:
	/*
	 * Step 1.25:
	 *   Force the DLL into override mode.
	 */
	dll_out = emc_readl(EMC_DIG_DLL_STATUS) & EMC_DIG_DLL_STATUS_OUT;
	emc_cfg_dig_dll = emc_readl(EMC_CFG_DIG_DLL);
	emc_cfg_dig_dll &= ~(EMC_CFG_DIG_DLL_OVERRIDE_VAL_MASK <<
			     EMC_CFG_DIG_DLL_OVERRIDE_VAL_SHIFT);
	emc_cfg_dig_dll |= ((dll_out & EMC_CFG_DIG_DLL_OVERRIDE_VAL_MASK) <<
			    EMC_CFG_DIG_DLL_OVERRIDE_VAL_SHIFT);
	emc_cfg_dig_dll |= EMC_CFG_DIG_DLL_OVERRIDE_EN;
	emc_writel(emc_cfg_dig_dll, EMC_CFG_DIG_DLL);
	emc_timing_update();

	/*
	 * Step 1.5:
	 *   Force the DLL into one shot mode.
	 */
	emc_cfg_dig_dll = emc_readl(EMC_CFG_DIG_DLL);
	emc_cfg_dig_dll &= ~(EMC_CFG_DIG_DLL_MODE_MASK <<
			     EMC_CFG_DIG_DLL_MODE_SHIFT);
	emc_cfg_dig_dll |= (EMC_CFG_DIG_DLL_MODE_RUN_TIL_LOCK <<
			    EMC_CFG_DIG_DLL_MODE_SHIFT);
	emc_cfg_dig_dll &= ~(EMC_CFG_DIG_DLL_UDSET_MASK <<
			     EMC_CFG_DIG_DLL_UDSET_SHIFT);
	emc_cfg_dig_dll |= (0x2 << EMC_CFG_DIG_DLL_UDSET_SHIFT);
	emc_cfg_dig_dll &= ~(EMC_CFG_DIG_DLL_LOCK_LIMIT_MASK <<
			     EMC_CFG_DIG_DLL_LOCK_LIMIT_SHIFT);
	emc_cfg_dig_dll |= (next_timing->emc_cfg_dig_dll &
			    (3 << EMC_CFG_DIG_DLL_LOCK_LIMIT_SHIFT));
	emc_writel(emc_cfg_dig_dll, EMC_CFG_DIG_DLL);
	emc_timing_update();

	/*
	 * Step 2:
	 *   Set the DLL's prelocking input. We do this so that we can get an
	 * override value for when we do the actual swap to this PLL later on.
	 * Once we swap, we will want this override value.
	 */
	emc_dll_clk_src = (div_value | src_value);
	writel(emc_dll_clk_src,
	       clk_base + CLK_RST_CONTROLLER_CLK_SOURCE_EMC_DLL);
	writel(emc_dll_clk_src | EMC_DLL_DYN_MUX_CTRL,
	       clk_base + CLK_RST_CONTROLLER_CLK_SOURCE_EMC_DLL);

	/*
	 * Step 2.5:
	 *   Enable CLK to DLL from CAR.
	 */
	writel(CLK_ENB_EMC_DLL,
	       clk_base + CLK_RST_CONTROLLER_CLK_OUT_ENB_X_SET);
	udelay(1);

	/*
	 * Step 3:
	 *   Reset the DLL and wait for it to lock against the target freq.
	 */
	fbio_spare_old = emc_readl(EMC_FBIO_SPARE);
	fbio_spare_new = fbio_spare_old | FBIO_SPARE_CFG_SW_DLL_RST_CTRL_N;

	emc_writel(fbio_spare_new, EMC_FBIO_SPARE);
	emc_timing_update();
	emc_writel(fbio_spare_old, EMC_FBIO_SPARE);
	emc_timing_update();

	emc_cfg_dig_dll = emc_readl(EMC_CFG_DIG_DLL);
	emc_cfg_dig_dll |= (EMC_CFG_DIG_DLL_EN);
	emc_writel(emc_cfg_dig_dll, EMC_CFG_DIG_DLL);
	emc_timing_update();
	udelay(1);

	do {
		dll_locked = emc_readl(EMC_DIG_DLL_STATUS) &
			EMC_DIG_DLL_STATUS_LOCKED;
	} while (!dll_locked);

	if (WARN_ON((emc_readl(EMC_INTSTATUS) &
		     EMC_INTSTATUS_DLL_LOCK_TIMEOUT_INT)))
		emc_writel(EMC_INTSTATUS_DLL_LOCK_TIMEOUT_INT,
			   EMC_INTSTATUS);

	/*
	 * Step 4:
	 *   Disable CLK to DLL from CAR.
	 */
	writel(CLK_ENB_EMC_DLL,
	       clk_base + CLK_RST_CONTROLLER_CLK_OUT_ENB_X_CLR);
	udelay(1);

	/*
	 * Step 5:
	 *   Now, reinvoke the state machine logic without the clock.
	 */
	emc_cfg_dig_dll = emc_readl(EMC_CFG_DIG_DLL);
	emc_cfg_dig_dll &= ~EMC_CFG_DIG_DLL_EN;
	emc_writel(emc_cfg_dig_dll, EMC_CFG_DIG_DLL);
	emc_timing_update();

	emc_cfg_dig_dll |= EMC_CFG_DIG_DLL_EN;
	emc_writel(emc_cfg_dig_dll, EMC_CFG_DIG_DLL);
	emc_timing_update();
	udelay(1);

	dll_out = emc_readl(EMC_DIG_DLL_STATUS) & EMC_DIG_DLL_STATUS_OUT;

	emc_cfg_dig_dll = emc_readl(EMC_CFG_DIG_DLL);
	emc_cfg_dig_dll &= ~EMC_CFG_DIG_DLL_EN;
	emc_writel(emc_cfg_dig_dll, EMC_CFG_DIG_DLL);
	emc_timing_update();

	return dll_out;
}

static noinline void emc_set_clock(const struct tegra14_emc_table *next_timing,
				   const struct tegra14_emc_table *last_timing,
				   u32 clk_setting)
{
	int i, dll_change, pre_wait;
	bool dyn_sref_enabled, zcal_long;

	u32 dll_override, emc_cfg_dig_dll, pmc_dpd;
	u32 t_start, t_diff;

	u32 use_prelock = 0;
	u32 emc_cfg_reg = emc_readl(EMC_CFG);

	if (!(clk_setting & EMC_CLK_FORCE_CC_TRIGGER))
		use_prelock = next_timing->emc_cfg_dig_dll & EMC_CFG_DIG_DLL_EN;

	dyn_sref_enabled = emc_cfg_reg & EMC_CFG_DYN_SREF_ENABLE;
	dll_change = get_dll_change(next_timing, last_timing);
	zcal_long = (next_timing->burst_regs[EMC_ZCAL_INTERVAL_INDEX] != 0) &&
		(last_timing->burst_regs[EMC_ZCAL_INTERVAL_INDEX] == 0);

	/* 1. clear clkchange_complete interrupts */
	emc_writel(EMC_INTSTATUS_CLKCHANGE_COMPLETE |
		   EMC_INTSTATUS_DLL_LOCK_TIMEOUT_INT, EMC_INTSTATUS);

	/* 1.5 On t148, prelock the DLL - assuming the DLL is enabled. */
	if (use_prelock)
		dll_override = emc_prelock_dll(last_timing, next_timing);
	else
		dll_override = 0;

	/* 2. disable dynamic self-refresh and preset dqs vref, then wait for
	   possible self-refresh entry/exit and/or dqs vref settled - waiting
	   before the clock change decreases worst case change stall time */
	pre_wait = 0;
	if (dyn_sref_enabled) {
		emc_cfg_reg &= ~EMC_CFG_DYN_SREF_ENABLE;
		emc_writel(emc_cfg_reg, EMC_CFG);
		pre_wait = 5;		/* 5us+ for self-refresh entry/exit */
	}

	/* 2.5 check dq/dqs vref delay */
	if (dqs_preset(next_timing, last_timing)) {
		if (pre_wait < 30)
			pre_wait = 30; /* (T148) 30us+ for dqs vref settled */
		/* Take the DISC pads out of DPD. */
		pmc_dpd = (PMC_IO_DPD2_REQ_CODE_DPD_OFF <<
			   PMC_IO_DPD2_REQ_CODE_SHIFT);
		writel(pmc_dpd | PMC_IO_DPD2_REQ_DISC_BIAS,
		       pmc_base + PMC_IO_DPD2_REQ);
	}
	emc_timing_update();
	t_start = tegra_read_usec_raw();

	/* 3. For t148, leave auto cal alone. */

	/* 4. program burst shadow registers */
	for (i = 0; i < next_timing->burst_regs_num; i++) {
		if (!burst_reg_addr[i])
			continue;
		__raw_writel(next_timing->burst_regs[i], burst_reg_addr[i]);
	}
	if (!use_prelock && !(clk_setting & EMC_CLK_FORCE_CC_TRIGGER))
		writel(next_timing->emc_cfg_dig_dll | EMC_CFG_DIG_DLL_RESET |
		       EMC_CFG_DIG_DLL_OVERRIDE_EN, emc_base + EMC_CFG_DIG_DLL);

	if ((dram_type == DRAM_TYPE_LPDDR2) &&
	    (dram_over_temp_state != DRAM_OVER_TEMP_NONE))
		set_over_temp_timing(next_timing, dram_over_temp_state);

	emc_cfg_reg &= ~EMC_CFG_UPDATE_MASK;
	emc_cfg_reg |= next_timing->emc_cfg & EMC_CFG_UPDATE_MASK;
	emc_writel(emc_cfg_reg, EMC_CFG);
	wmb();
	barrier();

	/* 4.1 On ddr3 when DLL is re-started predict MRS long wait count and
	   overwrite DFS table setting - No DDR3 on t148. */

	/* 5.2 Moved to ccfifo - see 6.1. */

	/* 6. turn Off dll and enter self-refresh on DDR3 - No DDR3. */

	/* 6.1 Disable auto-refresh right before clock change. */
	ccfifo_writel(EMC_REFCTRL_DISABLE_ALL(dram_dev_num), EMC_REFCTRL);

	/* 6.2 Set EMC_SEL_DPD_CTRL based on next freq: disable DATA_DPD for
	   frequencies above 408 MHz. */
	if (next_timing->rate > 408000)
		ccfifo_writel(emc_readl(EMC_SEL_DPD_CTRL) &
			      ~EMC_SEL_DPD_CTRL_DATA_DPD_ENABLE,
			      EMC_SEL_DPD_CTRL);
	else
		ccfifo_writel(emc_readl(EMC_SEL_DPD_CTRL) |
			      EMC_SEL_DPD_CTRL_DATA_DPD_ENABLE,
			      EMC_SEL_DPD_CTRL);

	/* 7. flow control marker 2 */
	ccfifo_writel(1, EMC_STALL_THEN_EXE_AFTER_CLKCHANGE);

	/* 7.05 Re-enable autorefresh with the CCFIFO. */
	ccfifo_writel(EMC_REFCTRL_ENABLE_ALL(dram_dev_num), EMC_REFCTRL);

	/* 7.1 Use the new override value. */
	if (use_prelock) {
		emc_cfg_dig_dll = next_timing->emc_cfg_dig_dll &
			~(EMC_CFG_DIG_DLL_OVERRIDE_VAL_MASK <<
			  EMC_CFG_DIG_DLL_OVERRIDE_VAL_SHIFT);
		emc_cfg_dig_dll |= (dll_override <<
				    EMC_CFG_DIG_DLL_OVERRIDE_VAL_SHIFT);
		emc_cfg_dig_dll |= EMC_CFG_DIG_DLL_OVERRIDE_EN;
		emc_cfg_dig_dll &= ~EMC_CFG_DIG_DLL_EN;
		emc_cfg_dig_dll &= ~EMC_CFG_DIG_DLL_STALL_RW_UNTIL_LOCK;
		ccfifo_writel(emc_cfg_dig_dll, EMC_CFG_DIG_DLL);
	}

	/* 7.2 Force an emc timing update here. */
	ccfifo_writel(1, EMC_TIMING_CONTROL);

	/* 8. exit self-refresh on DDR3 - No DDR3 on t148. */

	/* 9. set dram mode registers */
	set_dram_mode(next_timing, last_timing, dll_change);

	/* 9.5 Update two fields in EMC_CFG that are not shadowed. */
	emc_cfg_reg &= ~(EMC_CFG_MAN_PRE_WR | EMC_CFG_MAN_PRE_RD);
	emc_cfg_reg |= next_timing->emc_cfg &
		       (EMC_CFG_MAN_PRE_WR | EMC_CFG_MAN_PRE_RD);
	ccfifo_writel(emc_cfg_reg, EMC_CFG);

	/* 10. issue zcal command if turning zcal On */
	if (zcal_long) {
		ccfifo_writel(EMC_ZQ_CAL_LONG_CMD_DEV0, EMC_ZQ_CAL);
		if (dram_dev_num > 1)
			ccfifo_writel(EMC_ZQ_CAL_LONG_CMD_DEV1, EMC_ZQ_CAL);
	}

	/* 10.1 dummy write to RO register to remove stall after change */
	ccfifo_writel(0, EMC_CCFIFO_STATUS);

	/* 11.5 program burst_up_down registers if emc rate is going down */
	if (next_timing->rate < last_timing->rate) {
		for (i = 0; i < next_timing->burst_up_down_regs_num; i++)
			__raw_writel(next_timing->burst_up_down_regs[i],
				burst_up_down_reg_addr[i]);
		wmb();
	}

	/* 11.75 Wait what ever time is necessary to make sure pre_wait us
	   have elapsed since programming vref mode. */
	if (pre_wait) {
		t_diff = tegra_read_usec_raw() - t_start;
		if (t_diff <= pre_wait)
			udelay(1 + pre_wait - t_diff);
	}

	/* 12-14. read any MC register to ensure the programming is done
	   change EMC clock source register wait for clk change completion */
	do_clock_change(clk_setting);

	/* 14.1 re-enable auto-refresh */

	/* 14.2 program burst_up_down registers if emc rate is going up */
	if (next_timing->rate > last_timing->rate) {
		for (i = 0; i < next_timing->burst_up_down_regs_num; i++)
			__raw_writel(next_timing->burst_up_down_regs[i],
				burst_up_down_reg_addr[i]);
		wmb();
	}

	/* 14.3 Check if we are entering schmit mode. If we are, then DPD can
	   be requested for the BG BIAS cells of the DISC pads. */
	if (~next_timing->burst_regs[EMC_XM2DQSPADCTRL2_INDEX] &
	    EMC_XM2DQSPADCTRL2_RX_FT_REC_ENABLE) {
		pmc_dpd = (PMC_IO_DPD2_REQ_CODE_DPD_ON <<
			   PMC_IO_DPD2_REQ_CODE_SHIFT);
		writel(pmc_dpd | PMC_IO_DPD2_REQ_DISC_BIAS,
		       pmc_base + PMC_IO_DPD2_REQ);
	}

	/* 15. restore auto-cal. On t148, this is just a reprogramming - its
	   already enabled during the clock change itself. */
	emc_writel(next_timing->emc_acal_interval, EMC_AUTO_CAL_INTERVAL);

	/* 16. restore dynamic self-refresh - for t148 if requested, we will
	   leave DSR disabled. Otherwise just follow the table entry. */
	if (!dsr_override && next_timing->emc_cfg & EMC_CFG_DYN_SREF_ENABLE) {
		emc_cfg_reg |= EMC_CFG_DYN_SREF_ENABLE;
		emc_writel(emc_cfg_reg, EMC_CFG);
	}

	/* 17. set zcal wait count */
	if (zcal_long)
		emc_writel(next_timing->emc_zcal_cnt_long, EMC_ZCAL_WAIT_CNT);

	/* 18. update restored timing */
	emc_timing_update();
}

static inline void emc_get_timing(struct tegra14_emc_table *timing)
{
	int i;

	/* Burst updates depends on previous state; burst_up_down are
	 * stateless. */
	for (i = 0; i < timing->burst_regs_num; i++) {
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
	timing->emc_mode_4 = 0;
	timing->emc_cfg = emc_readl(EMC_CFG);
	timing->rate = clk_get_rate_locked(emc) / 1000;
}

/* The EMC registers have shadow registers. When the EMC clock is updated
 * in the clock controller, the shadow registers are copied to the active
 * registers, allowing glitchless memory bus frequency changes.
 * This function updates the shadow registers for a new clock frequency,
 * and relies on the clock lock on the emc clock to avoid races between
 * multiple frequency changes. In addition access lock prevents concurrent
 * access to EMC registers from reading MRR registers */
int tegra_emc_set_rate(unsigned long rate)
{
	int i;
	u32 clk_setting;
	const struct tegra14_emc_table *last_timing;
	unsigned long flags;
	s64 last_change_delay;

	if (!tegra_emc_table)
		return -EINVAL;

	/* Table entries specify rate in kHz */
	rate = rate / 1000;

	i = get_start_idx(rate);
	for (; i < tegra_emc_table_size; i++) {
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

	clk_setting = tegra_emc_clk_sel[i].value;

	if (!timekeeping_suspended) {
		last_change_delay = ktime_us_delta(ktime_get(), clkchange_time);
		if ((last_change_delay >= 0) &&
		    (last_change_delay < clkchange_delay))
			udelay(clkchange_delay - (int)last_change_delay);
	}

	spin_lock_irqsave(&emc_access_lock, flags);
	/* Pick from the EMC tables based on the status of the over temp state
	   flag. */
	emc_set_clock(dram_over_temp_state != DRAM_OVER_TEMP_THROTTLE ?
		      &tegra_emc_table[i] : &tegra_emc_table_derated[i],
		      last_timing, clk_setting);
	clkchange_time = timekeeping_suspended ? clkchange_time : ktime_get();
	emc_timing = dram_over_temp_state != DRAM_OVER_TEMP_THROTTLE ?
		&tegra_emc_table[i] : &tegra_emc_table_derated[i];
	if (dram_over_temp_state == DRAM_OVER_TEMP_THROTTLE)
		pr_debug("[emc] Picked derated freq.\n");
	spin_unlock_irqrestore(&emc_access_lock, flags);

	emc_last_stats_update(i);

	pr_debug("%s: rate %lu setting 0x%x\n", __func__, rate, clk_setting);

	return 0;
}

long tegra_emc_round_rate_updown(unsigned long rate, bool up)
{
	int i;
	unsigned long table_rate;

	if (!tegra_emc_table)
		return clk_get_rate_locked(emc); /* no table - no rate change */

	if (!emc_enable)
		return -EINVAL;

	pr_debug("%s: %lu\n", __func__, rate);

	/* Table entries specify rate in kHz */
	rate = rate / 1000;

	i = get_start_idx(rate);
	for (; i < tegra_emc_table_size; i++) {
		if (tegra_emc_clk_sel[i].input == NULL)
			continue;	/* invalid entry */

		table_rate = tegra_emc_table[i].rate;
		if (table_rate >= rate) {
			if (!up && i && (table_rate > rate)) {
				i--;
				table_rate = tegra_emc_table[i].rate;
			}
			pr_debug("%s: using %lu\n", __func__, table_rate);
			last_round_idx = i;
			return table_rate * 1000;
		}
	}

	return -EINVAL;
}

struct clk *tegra_emc_predict_parent(unsigned long rate, u32 *div_value)
{
	int i;

	if (!tegra_emc_table) {
		if (rate == clk_get_rate_locked(emc)) {
			*div_value = emc->div - 2;
			return emc->parent;
		}
		return NULL;
	}

	pr_debug("%s: %lu\n", __func__, rate);

	/* Table entries specify rate in kHz */
	rate = rate / 1000;

	i = get_start_idx(rate);
	for (; i < tegra_emc_table_size; i++) {
		if (tegra_emc_table[i].rate == rate) {
			struct clk *p = tegra_emc_clk_sel[i].input;

			if (p && (tegra_emc_clk_sel[i].input_rate ==
				  clk_get_rate(p))) {
				*div_value = (tegra_emc_clk_sel[i].value &
					EMC_CLK_DIV_MASK) >> EMC_CLK_DIV_SHIFT;
				return p;
			}
		}
	}
	return NULL;
}

bool tegra_emc_is_parent_ready(unsigned long rate, struct clk **parent,
		unsigned long *parent_rate, unsigned long *backup_rate)
{

	int i;
	struct clk *p = NULL;
	unsigned long p_rate = 0;

	if (!tegra_emc_table)
		return true;

	pr_debug("%s: %lu\n", __func__, rate);

	/* Table entries specify rate in kHz */
	rate = rate / 1000;

	i = get_start_idx(rate);
	for (; i < tegra_emc_table_size; i++) {
		if (tegra_emc_table[i].rate == rate) {
			p = tegra_emc_clk_sel[i].input;
			if (!p)
				continue;	/* invalid entry */

			p_rate = tegra_emc_clk_sel[i].input_rate;
			if (p_rate == clk_get_rate(p))
				return true;
			break;
		}
	}

	/* Table match not found - "non existing parent" is ready */
	if (!p)
		return true;

#ifdef CONFIG_TEGRA_PLLM_SCALED
	/*
	 * Table match found, but parent is not ready - check if backup entry
	 * was found during initialization, and return the respective backup
	 * rate
	 */
	if (emc->shared_bus_backup.input &&
	    (emc->shared_bus_backup.input != p)) {
		*parent = p;
		*parent_rate = p_rate;
		*backup_rate = emc->shared_bus_backup.bus_rate;
		return false;
	}
#else
	/*
	 * Table match found, but parent is not ready - continue search
	 * for backup rate: min rate above requested that has different
	 * parent source (since only pll_c is scaled and may not be ready,
	 * any other parent can provide backup)
	 */
	*parent = p;
	*parent_rate = p_rate;

	for (i++; i < tegra_emc_table_size; i++) {
		p = tegra_emc_clk_sel[i].input;
		if (!p)
			continue;	/* invalid entry */

		if (p != (*parent)) {
			*backup_rate = tegra_emc_table[i].rate * 1000;
			return false;
		}
	}
#endif
	/* Parent is not ready, and no backup found */
	*backup_rate = -EINVAL;
	return false;
}

static inline const struct clk_mux_sel *get_emc_input(u32 val)
{
	const struct clk_mux_sel *sel;

	for (sel = emc->inputs; sel->input != NULL; sel++) {
		if (sel->value == val)
			break;
	}
	return sel;
}

static int find_matching_input(const struct tegra14_emc_table *table,
	struct clk *pll_c, struct clk *pll_m, struct emc_sel *emc_clk_sel)
{
	u32 div_value = (table->src_sel_reg & EMC_CLK_DIV_MASK) >>
		EMC_CLK_DIV_SHIFT;
	u32 src_value = (table->src_sel_reg & EMC_CLK_SOURCE_MASK) >>
		EMC_CLK_SOURCE_SHIFT;
	unsigned long input_rate = 0;
	unsigned long table_rate = table->rate * 1000; /* table rate in kHz */
	const struct clk_mux_sel *sel = get_emc_input(src_value);

#ifdef CONFIG_TEGRA_PLLM_SCALED
	struct clk *scalable_pll = pll_m;
#else
	struct clk *scalable_pll = pll_c;
#endif
	pr_info_once("tegra: %s is selected as scalable EMC clock source\n",
		     scalable_pll->name);

	if (div_value & 0x1) {
		pr_warn("tegra: invalid odd divider for EMC rate %lu\n",
			table_rate);
		return -EINVAL;
	}
	if (!sel->input) {
		pr_warn("tegra: no matching input found for EMC rate %lu\n",
			table_rate);
		return -EINVAL;
	}
	if (div_value && (table->src_sel_reg & EMC_CLK_LOW_JITTER_ENABLE)) {
		pr_warn("tegra: invalid LJ path for EMC rate %lu\n",
			table_rate);
		return -EINVAL;
	}
	if (!(table->src_sel_reg & EMC_CLK_MC_SAME_FREQ) !=
	    !(MC_EMEM_ARB_MISC0_EMC_SAME_FREQ &
	      table->burst_regs[MC_EMEM_ARB_MISC0_INDEX])) {
		pr_warn("tegra: ambiguous EMC to MC ratio for EMC rate %lu\n",
			table_rate);
		return -EINVAL;
	}

#ifndef CONFIG_TEGRA_DUAL_CBUS
	if (sel->input == pll_c) {
		pr_warn("tegra: %s is cbus source: no EMC rate %lu support\n",
			sel->input->name, table_rate);
		return -EINVAL;
	}
#endif

	if (sel->input == scalable_pll) {
		input_rate = table_rate * (1 + div_value / 2);
	} else {
		/* all other sources are fixed, must exactly match the rate */
		input_rate = clk_get_rate(sel->input);
		if (input_rate != (table_rate * (1 + div_value / 2))) {
			pr_warn("tegra: EMC rate %lu does not match %s rate %lu\n",
				table_rate, sel->input->name, input_rate);
			return -EINVAL;
		}
	}

#ifdef CONFIG_TEGRA_PLLM_SCALED
		if (sel->input == pll_c) {
			/* maybe overwritten in a loop - end up at max rate
			   from pll_c */
			emc->shared_bus_backup.input = pll_c;
			emc->shared_bus_backup.bus_rate = table_rate;
		}
#endif
	/* Get ready emc clock selection settings for this table rate */
	emc_clk_sel->input = sel->input;
	emc_clk_sel->input_rate = input_rate;
	emc_clk_sel->value = table->src_sel_reg;

	return 0;
}

static void adjust_emc_dvfs_table(const struct tegra14_emc_table *table,
				  int table_size)
{
	int i, j;
	unsigned long rate;

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

#ifdef CONFIG_TEGRA_PLLM_SCALED
/* When pll_m is scaled, pll_c must provide backup rate;
   if not - remove rates that require pll_m scaling */
static int purge_emc_table(unsigned long max_rate)
{
	int i;
	int ret = 0;

	if (emc->shared_bus_backup.input)
		return ret;

	pr_warn("tegra: selected pll_m scaling option but no backup source:\n");
	pr_warn("       removed not supported entries from the table:\n");

	/* made all entries with non matching rate invalid */
	for (i = 0; i < tegra_emc_table_size; i++) {
		struct emc_sel *sel = &tegra_emc_clk_sel[i];
		if (sel->input) {
			if (clk_get_rate(sel->input) != sel->input_rate) {
				pr_warn("       EMC rate %lu\n",
					tegra_emc_table[i].rate * 1000);
				sel->input = NULL;
				sel->input_rate = 0;
				sel->value = 0;
				if (max_rate == tegra_emc_table[i].rate)
					ret = -EINVAL;
			}
		}
	}
	return ret;
}
#else
/* When pll_m is fixed @ max EMC rate, it always provides backup for pll_c */
#define purge_emc_table(max_rate) (0)
#endif

static int init_emc_table(const struct tegra14_emc_table *table,
			  const struct tegra14_emc_table *table_der,
			  int table_size)
{
	int i, mv;
	u32 reg;
	bool max_entry = false;
	bool emc_max_dvfs_sel = get_emc_max_dvfs();
	unsigned long boot_rate, max_rate;
	struct clk *pll_c = tegra_get_clock_by_name("pll_c");
	struct clk *pll_m = tegra_get_clock_by_name("pll_m");

	emc_stats.clkchange_count = 0;
	spin_lock_init(&emc_stats.spinlock);
	emc_stats.last_update = get_jiffies_64();
	emc_stats.last_sel = TEGRA_EMC_TABLE_MAX_SIZE;

	if (dram_type != DRAM_TYPE_LPDDR2) {
		pr_err("tegra: not supported DRAM type %u\n", dram_type);
		return -ENODATA;
	}

	if (!table || !table_size) {
		pr_err("tegra: EMC DFS table is empty\n");
		return -ENODATA;
	}

	boot_rate = clk_get_rate(emc) / 1000;
	max_rate = boot_rate;

	tegra_emc_table_size = min(table_size, TEGRA_EMC_TABLE_MAX_SIZE);
	switch (table[0].rev) {
	case 0x50:
	case 0x51:
		pr_err("tegra: invalid EMC DFS table (0x%02x): too old.\n",
		       table[0].rev);
		break;
	case 0x52:
		start_timing.burst_regs_num = table[0].burst_regs_num;
		break;
	default:
		pr_err("tegra: invalid EMC DFS table: unknown rev 0x%x\n",
			table[0].rev);
		return -ENODATA;
	}

	/* Check that the derated table and non-derated table match. */
	if (WARN(!table_der, "tegra: emc: Missing derated tables!\n"))
		return -EINVAL;
	for (i = 0; i < tegra_emc_table_size; i++) {
		if (table[i].rate        != table_der[i].rate ||
		    table[i].rev         != table_der[i].rev ||
		    table[i].emc_min_mv  != table_der[i].emc_min_mv ||
		    table[i].src_sel_reg != table_der[i].src_sel_reg) {
			pr_err("tegra: emc: Derated table mismatch.\n");
			return -EINVAL;
		}
	}
	pr_info("tegra: emc: Derated table is valid.\n");

	/* Match EMC source/divider settings with table entries */
	for (i = 0; i < tegra_emc_table_size; i++) {
		unsigned long table_rate = table[i].rate;

		/* Skip "no-rate" entry, or entry violating ascending order */
		if (!table_rate ||
		    (i && (table_rate <= table[i-1].rate)))
			continue;

		BUG_ON(table[i].rev != table[0].rev);

		if (find_matching_input(&table[i], pll_c, pll_m,
					&tegra_emc_clk_sel[i]))
			continue;

		if (table_rate == boot_rate)
			emc_stats.last_sel = i;

		if (emc_max_dvfs_sel) {
			/* EMC max rate = max table entry above boot rate */
			if (table_rate >= max_rate) {
				max_rate = table_rate;
				max_entry = true;
			}
		} else if (table_rate == max_rate) {
			/* EMC max rate = boot rate */
			max_entry = true;
			break;
		}
	}

	/* Validate EMC rate and voltage limits */
	if (!max_entry) {
		pr_err("tegra: invalid EMC DFS table: entry for max rate"
		       " %lu kHz is not found\n", max_rate);
		return -ENODATA;
	}

	tegra_emc_table = table;
	tegra_emc_table_derated = table_der;

	/*
	 * Purge rates that cannot be reached because table does not specify
	 * proper backup source. If maximum rate was purged, fall back on boot
	 * rate as maximum limit. In any case propagate new maximum limit
	 * down stream to shared users, and check it against nominal voltage.
	 */
	if (purge_emc_table(max_rate))
		max_rate = boot_rate;
	tegra_init_max_rate(emc, max_rate * 1000);

	if (emc->dvfs) {
		adjust_emc_dvfs_table(tegra_emc_table, tegra_emc_table_size);
		mv = tegra_dvfs_predict_peak_millivolts(emc, max_rate * 1000);
		if ((mv <= 0) || (mv > emc->dvfs->max_millivolts)) {
			tegra_emc_table = NULL;
			pr_err("tegra: invalid EMC DFS table: maximum rate %lu"
			       " kHz does not match nominal voltage %d\n",
			       max_rate, emc->dvfs->max_millivolts);
			return -ENODATA;
		}
	}

	pr_info("tegra: validated EMC DFS table\n");

	/* Configure clock change mode according to dram type */
	reg = emc_readl(EMC_CFG_2) & (~EMC_CFG_2_MODE_MASK);
	reg |= ((dram_type == DRAM_TYPE_LPDDR2) ? EMC_CFG_2_PD_MODE :
		EMC_CFG_2_SREF_MODE) << EMC_CFG_2_MODE_SHIFT;
	emc_writel(reg, EMC_CFG_2);

#if defined(CONFIG_TEGRA_ERRATA_1252872)
	emc_writel(tegra_emc_table->emc_acal_interval, EMC_AUTO_CAL_INTERVAL);
	emc_timing_update();
#endif
	return 0;
}

#ifdef CONFIG_PASR
/* Check if the attached memory device uses LPDDR3 protocol.
 * Bit 8 (enable LPDDR3 write preamble toggle) of EMC_FBIO_SPARE is enabled
 * for LPDDR3.
 */
static bool tegra14_is_lpddr3(void)
{
	return emc_readl(EMC_FBIO_SPARE) & BIT(8);
}

static void tegra14_pasr_apply_mask(u16 *mem_reg, void *cookie)
{
	u32 val = 0;
	int device = (int)cookie;

	val = TEGRA_EMC_MODE_REG_17 | *mem_reg;
	val |= device << TEGRA_EMC_MRW_DEV_SHIFT;

	emc_writel(val, EMC_MRW);

	pr_debug("%s: cookie = %d mem_reg = 0x%04x val = 0x%08x\n", __func__,
			(int)cookie, *mem_reg, val);
}

static int tegra14_pasr_enable(const char *arg, const struct kernel_param *kp)
{
	unsigned int old_pasr_enable;
	void *cookie;
	u16 mem_reg;
	unsigned long device_size;

	if (!tegra14_is_lpddr3())
		return -ENOSYS;

	old_pasr_enable = pasr_enable;
	param_set_int(arg, kp);

	if (old_pasr_enable == pasr_enable)
		return 0;

	device_size = (mc_readl(TEGRA_MC_EMEM_ADR_CFG_DEV) >>
				TEGRA_EMEM_DEV_DEVSIZE_SHIFT) &
				TEGRA_EMEM_DEV_DEVSIZE_MASK;
	device_size = (4 << device_size) << 20;

	/* Cookie represents the device number to write to MRW register.
	 * 0x2 to for only dev0, 0x1 for dev1.
	 */
	if (pasr_enable == 0) {
		mem_reg = 0;

		cookie = (void *)(int)TEGRA_EMC_MRW_DEV1;
		if (!pasr_register_mask_function(TEGRA_DRAM_BASE,
							NULL, cookie))
			tegra14_pasr_apply_mask(&mem_reg, cookie);
		cookie = (void *)(int)TEGRA_EMC_MRW_DEV2;
		if (!pasr_register_mask_function(TEGRA_DRAM_BASE + device_size,
							NULL, cookie))
			tegra14_pasr_apply_mask(&mem_reg, cookie);
	} else {
		cookie = (void *)(int)2;
		pasr_register_mask_function(TEGRA_DRAM_BASE,
					&tegra14_pasr_apply_mask, cookie);

		cookie = (void *)(int)1;
		pasr_register_mask_function(TEGRA_DRAM_BASE + device_size,
					&tegra14_pasr_apply_mask, cookie);
	}

	return 0;
}

static struct kernel_param_ops tegra14_pasr_enable_ops = {
	.set = tegra14_pasr_enable,
	.get = param_get_int,
};
module_param_cb(pasr_enable, &tegra14_pasr_enable_ops, &pasr_enable, 0644);
#endif

static int tegra14_emc_probe(struct platform_device *pdev)
{
	struct tegra14_emc_pdata *pdata;
	struct resource *res;
	u32 padctrl;

	pasr_enable = 0;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "missing register base\n");
		return -ENOMEM;
	}

	pdata = pdev->dev.platform_data;
	if (!pdata) {
		dev_err(&pdev->dev, "missing platform data\n");
		return -ENODATA;
	}

#if defined(CONFIG_TEGRA_ERRATA_1252872)
	padctrl = emc_readl(EMC_XM2CMDPADCTRL);
	padctrl &= ~(0x7 << 12);
	padctrl &= ~(0x7 << 20);
	emc_writel(padctrl, EMC_XM2CMDPADCTRL);
#endif

	return init_emc_table(pdata->tables, pdata->tables_derated,
			      pdata->num_tables);
}

static struct platform_driver tegra14_emc_driver = {
	.driver         = {
		.name   = "tegra-emc",
		.owner  = THIS_MODULE,
	},
	.probe          = tegra14_emc_probe,
};

int __init tegra14_emc_init(void)
{
	int ret = platform_driver_register(&tegra14_emc_driver);
	if (!ret) {
		tegra_emc_iso_usage_table_init(tegra14_emc_iso_usage,
			ARRAY_SIZE(tegra14_emc_iso_usage));
		if (emc_enable) {
			unsigned long rate = tegra_emc_round_rate_updown(
				emc->boot_rate, false);
			if (!IS_ERR_VALUE(rate))
				tegra_clk_preset_emc_monitor(rate);
		}
	}
	return ret;
}

void tegra_emc_timing_invalidate(void)
{
	emc_timing = NULL;
}

void tegra_emc_dram_type_init(struct clk *c)
{
	emc = c;

	dram_type = (emc_readl(EMC_FBIO_CFG5) &
		     EMC_CFG5_TYPE_MASK) >> EMC_CFG5_TYPE_SHIFT;

	dram_dev_num = (mc_readl(MC_EMEM_ADR_CFG) & 0x1) + 1; /* 2 dev max */
}

int tegra_emc_get_dram_type(void)
{
	return dram_type;
}

static int emc_read_mrr(int dev, int addr)
{
	int ret;
	u32 val, emc_cfg;

	if (dram_type != DRAM_TYPE_LPDDR2)
		return -ENODEV;

	ret = wait_for_update(EMC_STATUS, EMC_STATUS_MRR_DIVLD, false);
	if (ret)
		return ret;

	emc_cfg = emc_readl(EMC_CFG);
	if (emc_cfg & EMC_CFG_DRAM_ACPD) {
		emc_writel(emc_cfg & ~EMC_CFG_DRAM_ACPD, EMC_CFG);
		emc_timing_update();
	}

	val = dev ? DRAM_DEV_SEL_1 : DRAM_DEV_SEL_0;
	val |= (addr << EMC_MRR_MA_SHIFT) & EMC_MRR_MA_MASK;
	emc_writel(val, EMC_MRR);

	ret = wait_for_update(EMC_STATUS, EMC_STATUS_MRR_DIVLD, true);
	if (emc_cfg & EMC_CFG_DRAM_ACPD) {
		emc_writel(emc_cfg, EMC_CFG);
		emc_timing_update();
	}
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

	mr4 = (mr4 & LPDDR2_MR4_TEMP_MASK) >> LPDDR2_MR4_TEMP_SHIFT;
	return mr4;
}

int tegra_emc_dsr_override(int override)
{
	int i, emc_cfg;
	unsigned long flags, rate;
	const struct tegra14_emc_table *tbl_timing;

	if (override != TEGRA_EMC_DSR_NORMAL &&
	    override != TEGRA_EMC_DSR_OVERRIDE)
		return -EINVAL;

	/* No-op. */
	if (override == dsr_override)
		return 0;

	spin_lock_irqsave(&emc_access_lock, flags);

	dsr_override = override;
	emc_cfg = emc_readl(EMC_CFG);

	/*
	 * If override is specified, just turn off DSR. Otherwise follow the
	 * state specified in the current rate's table entry. However, it is
	 * possible that we will be booting (warm or cold) with a rate that
	 * is not in the table. If this is the case, we cannot determine what
	 * needs to happen so we will simply leave dsr_override set to 0 and
	 * let the next clock change handle returning DSR state to the table's
	 * state.
	 */
	emc_cfg &= ~EMC_CFG_DYN_SREF_ENABLE;
	if (override == TEGRA_EMC_DSR_NORMAL) {
		/* Get table entry for current rate if we don't have a valid
		   rate already. */
		tbl_timing = emc_timing;
		if (!tbl_timing) {
			/* Without the table, we can't do anything here. */
			if (!tegra_emc_table) {
				spin_unlock_irqrestore(&emc_access_lock, flags);
				return -EINVAL;
			}
			rate = clk_get_rate_all_locked(emc) / 1000;
			for (i = 0; i < tegra_emc_table_size; i++) {
				if (tegra_emc_table[i].rate == rate) {
					tbl_timing = &tegra_emc_table[i];
					break;
				}
			}
		}

		if (tbl_timing && tbl_timing->emc_cfg & EMC_CFG_DYN_SREF_ENABLE)
			emc_cfg |= EMC_CFG_DYN_SREF_ENABLE;
	}

	emc_writel(emc_cfg, EMC_CFG);
	emc_timing_update();
	spin_unlock_irqrestore(&emc_access_lock, flags);
	return 0;
}

int tegra_emc_dsr_override_status(void)
{
	return dsr_override;
}

int tegra_emc_dsr_status(void)
{
	int emc_cfg = emc_readl(EMC_CFG);

	if (emc_cfg & EMC_CFG_DYN_SREF_ENABLE)
		return 1;
	else
		return 0;
}

int tegra_emc_set_over_temp_state(unsigned long state)
{
	int offset;
	unsigned long flags;

	if (dram_type != DRAM_TYPE_LPDDR2 || !emc_timing)
		return -ENODEV;

	if (state > DRAM_OVER_TEMP_THROTTLE)
		return -EINVAL;

	/* Silently do nothing if there is no state change. */
	if (state == dram_over_temp_state)
		return 0;

	/*
	 * If derating needs to be turned on/off force a clock change. That
	 * will take care of the refresh as well. In derating is not going to
	 * be changed then all that is needed is an update to the refresh
	 * settings.
	 */
	spin_lock_irqsave(&emc_access_lock, flags);
	if (state == DRAM_OVER_TEMP_THROTTLE) {
		dram_over_temp_state = state;
		offset = emc_timing - tegra_emc_table;
		emc_set_clock(emc_timing, &tegra_emc_table_derated[offset],
			      tegra_emc_clk_sel[offset].value |
			      EMC_CLK_FORCE_CC_TRIGGER);
		emc_timing = &tegra_emc_table_derated[offset];
	} else if (dram_over_temp_state == DRAM_OVER_TEMP_THROTTLE) {
		dram_over_temp_state = state;
		offset = emc_timing - tegra_emc_table_derated;
		emc_set_clock(emc_timing, &tegra_emc_table[offset],
			      tegra_emc_clk_sel[offset].value |
			      EMC_CLK_FORCE_CC_TRIGGER);
		emc_timing = &tegra_emc_table[offset];
	} else {
		set_over_temp_timing(emc_timing, state);
		emc_timing_update();
		if (state != DRAM_OVER_TEMP_NONE)
			emc_writel(EMC_REF_FORCE_CMD, EMC_REF);
		dram_over_temp_state = state;
	}
	spin_unlock_irqrestore(&emc_access_lock, flags);

	return 0;
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

static int emc_table_info_show(struct seq_file *s, void *data)
{
	int i;

	seq_printf(s, "Table info:\n");
	seq_printf(s, "  Rev: 0x%02x\n", tegra_emc_table->rev);
	seq_printf(s, "  Table ID: %s\n", tegra_emc_table->table_id);
	seq_printf(s, "  Possible rates (kHz):\n");

	for (i = 0; i < tegra_emc_table_size; i++) {
		if (tegra_emc_clk_sel[i].input == NULL)
			continue;
		seq_printf(s, "    %lu\n", tegra_emc_table[i].rate);
	}

	return 0;
}

static int emc_table_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, emc_table_info_show, inode->i_private);
}

static const struct file_operations emc_table_info_fops = {
	.open		= emc_table_info_open,
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
	return tegra_emc_set_over_temp_state(val);
}
DEFINE_SIMPLE_ATTRIBUTE(over_temp_state_fops, over_temp_state_get,
			over_temp_state_set, "%llu\n");

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

static int dsr_get(void *data, u64 *val)
{
	*val = tegra_emc_dsr_status();
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(dsr_fops, dsr_get, NULL, "%llu\n");

static int dsr_override_set(void *data, u64 val)
{
	if (val)
		tegra_emc_dsr_override(TEGRA_EMC_DSR_OVERRIDE);
	else
		tegra_emc_dsr_override(TEGRA_EMC_DSR_NORMAL);

	return 0;
}

static int dsr_override_get(void *data, u64 *val)
{
	*val = tegra_emc_dsr_override_status();
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(dsr_override_fops, dsr_override_get,
			dsr_override_set, "%llu\n");

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

	if (!debugfs_create_u32("clkchange_delay", S_IRUGO | S_IWUSR,
		emc_debugfs_root, (u32 *)&clkchange_delay))
		goto err_out;

	if (!debugfs_create_file("dram_temperature", S_IRUGO, emc_debugfs_root,
				 NULL, &dram_temperature_fops))
		goto err_out;

	if (!debugfs_create_file("over_temp_state", S_IRUGO | S_IWUSR,
				 emc_debugfs_root, NULL, &over_temp_state_fops))
		goto err_out;

	if (!debugfs_create_file("efficiency", S_IRUGO | S_IWUSR,
				 emc_debugfs_root, NULL, &efficiency_fops))
		goto err_out;

	if (!debugfs_create_file("dsr", S_IRUGO | S_IWUSR,
				 emc_debugfs_root, NULL, &dsr_fops))
		goto err_out;

	if (!debugfs_create_file("dsr_override", S_IRUGO | S_IWUSR,
				 emc_debugfs_root, NULL, &dsr_override_fops))
		goto err_out;

	if (!debugfs_create_file("table_info", S_IRUGO | S_IWUSR,
				 emc_debugfs_root, NULL, &emc_table_info_fops))
		goto err_out;

	if (tegra_emc_iso_usage_debugfs_init(emc_debugfs_root))
		goto err_out;

	return 0;

err_out:
	debugfs_remove_recursive(emc_debugfs_root);
	return -ENOMEM;
}

late_initcall(tegra_emc_debug_init);
#endif
