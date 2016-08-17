/*
 * arch/arm/mach-tegra/tegra3_clocks.c
 *
 * Copyright (c) 2010-2013 NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
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
#include <linux/module.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/cpufreq.h>
#include <linux/syscore_ops.h>

#include <asm/clkdev.h>

#include <mach/iomap.h>
#include <mach/edp.h>
#include <mach/hardware.h>

#include "clock.h"
#include "fuse.h"
#include "dvfs.h"
#include "pm.h"
#include "sleep.h"
#include "tegra3_emc.h"

#define RST_DEVICES_L			0x004
#define RST_DEVICES_H			0x008
#define RST_DEVICES_U			0x00C
#define RST_DEVICES_V			0x358
#define RST_DEVICES_W			0x35C
#define RST_DEVICES_SET_L		0x300
#define RST_DEVICES_CLR_L		0x304
#define RST_DEVICES_SET_V		0x430
#define RST_DEVICES_CLR_V		0x434
#define RST_DEVICES_NUM			5

#define CLK_OUT_ENB_L			0x010
#define CLK_OUT_ENB_H			0x014
#define CLK_OUT_ENB_U			0x018
#define CLK_OUT_ENB_V			0x360
#define CLK_OUT_ENB_W			0x364
#define CLK_OUT_ENB_SET_L		0x320
#define CLK_OUT_ENB_CLR_L		0x324
#define CLK_OUT_ENB_SET_V		0x440
#define CLK_OUT_ENB_CLR_V		0x444
#define CLK_OUT_ENB_NUM			5

#define RST_DEVICES_V_SWR_CPULP_RST_DIS	(0x1 << 1)
#define CLK_OUT_ENB_V_CLK_ENB_CPULP_EN	(0x1 << 1)

#define PERIPH_CLK_TO_BIT(c)		(1 << (c->u.periph.clk_num % 32))
#define PERIPH_CLK_TO_RST_REG(c)	\
	periph_clk_to_reg((c), RST_DEVICES_L, RST_DEVICES_V, 4)
#define PERIPH_CLK_TO_RST_SET_REG(c)	\
	periph_clk_to_reg((c), RST_DEVICES_SET_L, RST_DEVICES_SET_V, 8)
#define PERIPH_CLK_TO_RST_CLR_REG(c)	\
	periph_clk_to_reg((c), RST_DEVICES_CLR_L, RST_DEVICES_CLR_V, 8)

#define PERIPH_CLK_TO_ENB_REG(c)	\
	periph_clk_to_reg((c), CLK_OUT_ENB_L, CLK_OUT_ENB_V, 4)
#define PERIPH_CLK_TO_ENB_SET_REG(c)	\
	periph_clk_to_reg((c), CLK_OUT_ENB_SET_L, CLK_OUT_ENB_SET_V, 8)
#define PERIPH_CLK_TO_ENB_CLR_REG(c)	\
	periph_clk_to_reg((c), CLK_OUT_ENB_CLR_L, CLK_OUT_ENB_CLR_V, 8)

#define CLK_MASK_ARM			0x44
#define MISC_CLK_ENB			0x48

#define OSC_CTRL			0x50
#define OSC_CTRL_OSC_FREQ_MASK		(0xF<<28)
#define OSC_CTRL_OSC_FREQ_13MHZ		(0x0<<28)
#define OSC_CTRL_OSC_FREQ_19_2MHZ	(0x4<<28)
#define OSC_CTRL_OSC_FREQ_12MHZ		(0x8<<28)
#define OSC_CTRL_OSC_FREQ_26MHZ		(0xC<<28)
#define OSC_CTRL_OSC_FREQ_16_8MHZ	(0x1<<28)
#define OSC_CTRL_OSC_FREQ_38_4MHZ	(0x5<<28)
#define OSC_CTRL_OSC_FREQ_48MHZ		(0x9<<28)
#define OSC_CTRL_MASK			(0x3f2 | OSC_CTRL_OSC_FREQ_MASK)

#define OSC_CTRL_PLL_REF_DIV_MASK	(3<<26)
#define OSC_CTRL_PLL_REF_DIV_1		(0<<26)
#define OSC_CTRL_PLL_REF_DIV_2		(1<<26)
#define OSC_CTRL_PLL_REF_DIV_4		(2<<26)

#define PERIPH_CLK_SOURCE_I2S1		0x100
#define PERIPH_CLK_SOURCE_EMC		0x19c
#define PERIPH_CLK_SOURCE_OSC		0x1fc
#define PERIPH_CLK_SOURCE_NUM1 \
	((PERIPH_CLK_SOURCE_OSC - PERIPH_CLK_SOURCE_I2S1) / 4)

#define PERIPH_CLK_SOURCE_G3D2		0x3b0
#define PERIPH_CLK_SOURCE_SE		0x42c
#define PERIPH_CLK_SOURCE_NUM2 \
	((PERIPH_CLK_SOURCE_SE - PERIPH_CLK_SOURCE_G3D2) / 4 + 1)

#define AUDIO_DLY_CLK			0x49c
#define AUDIO_SYNC_CLK_SPDIF		0x4b4
#define PERIPH_CLK_SOURCE_NUM3 \
	((AUDIO_SYNC_CLK_SPDIF - AUDIO_DLY_CLK) / 4 + 1)

#define PERIPH_CLK_SOURCE_NUM		(PERIPH_CLK_SOURCE_NUM1 + \
					 PERIPH_CLK_SOURCE_NUM2 + \
					 PERIPH_CLK_SOURCE_NUM3)

#define CPU_SOFTRST_CTRL		0x380

#define PERIPH_CLK_SOURCE_DIVU71_MASK	0xFF
#define PERIPH_CLK_SOURCE_DIVU16_MASK	0xFFFF
#define PERIPH_CLK_SOURCE_DIV_SHIFT	0
#define PERIPH_CLK_SOURCE_DIVIDLE_SHIFT	8
#define PERIPH_CLK_SOURCE_DIVIDLE_VAL	50
#define PERIPH_CLK_UART_DIV_ENB		(1<<24)
#define PERIPH_CLK_VI_SEL_EX_SHIFT	24
#define PERIPH_CLK_VI_SEL_EX_MASK	(0x3<<PERIPH_CLK_VI_SEL_EX_SHIFT)
#define PERIPH_CLK_NAND_DIV_EX_ENB	(1<<8)
#define PERIPH_CLK_DTV_POLARITY_INV	(1<<25)

#define AUDIO_SYNC_SOURCE_MASK		0x0F
#define AUDIO_SYNC_DISABLE_BIT		0x10
#define AUDIO_SYNC_TAP_NIBBLE_SHIFT(c)	((c->reg_shift - 24) * 4)

#define PLL_BASE			0x0
#define PLL_BASE_BYPASS			(1<<31)
#define PLL_BASE_ENABLE			(1<<30)
#define PLL_BASE_REF_ENABLE		(1<<29)
#define PLL_BASE_OVERRIDE		(1<<28)
#define PLL_BASE_LOCK			(1<<27)
#define PLL_BASE_DIVP_MASK		(0x7<<20)
#define PLL_BASE_DIVP_SHIFT		20
#define PLL_BASE_DIVN_MASK		(0x3FF<<8)
#define PLL_BASE_DIVN_SHIFT		8
#define PLL_BASE_DIVM_MASK		(0x1F)
#define PLL_BASE_DIVM_SHIFT		0

#define PLL_OUT_RATIO_MASK		(0xFF<<8)
#define PLL_OUT_RATIO_SHIFT		8
#define PLL_OUT_OVERRIDE		(1<<2)
#define PLL_OUT_CLKEN			(1<<1)
#define PLL_OUT_RESET_DISABLE		(1<<0)

#define PLL_MISC(c)			\
	(((c)->flags & PLL_ALT_MISC_REG) ? 0x4 : 0xc)
#define PLL_MISC_LOCK_ENABLE(c)	\
	(((c)->flags & (PLLU | PLLD)) ? (1<<22) : (1<<18))

#define PLL_MISC_DCCON_SHIFT		20
#define PLL_MISC_CPCON_SHIFT		8
#define PLL_MISC_CPCON_MASK		(0xF<<PLL_MISC_CPCON_SHIFT)
#define PLL_MISC_LFCON_SHIFT		4
#define PLL_MISC_LFCON_MASK		(0xF<<PLL_MISC_LFCON_SHIFT)
#define PLL_MISC_VCOCON_SHIFT		0
#define PLL_MISC_VCOCON_MASK		(0xF<<PLL_MISC_VCOCON_SHIFT)
#define PLLD_MISC_CLKENABLE		(1<<30)

#define PLLU_BASE_POST_DIV		(1<<20)

#define PLLD_BASE_DSIB_MUX_SHIFT	25
#define PLLD_BASE_DSIB_MUX_MASK		(1<<PLLD_BASE_DSIB_MUX_SHIFT)
#define PLLD_BASE_CSI_CLKENABLE		(1<<26)
#define PLLD_MISC_DSI_CLKENABLE		(1<<30)
#define PLLD_MISC_DIV_RST		(1<<23)
#define PLLD_MISC_DCCON_SHIFT		12

#define PLLDU_LFCON_SET_DIVN		600

/* FIXME: OUT_OF_TABLE_CPCON per pll */
#define OUT_OF_TABLE_CPCON		0x8

#define SUPER_CLK_MUX			0x00
#define SUPER_STATE_SHIFT		28
#define SUPER_STATE_MASK		(0xF << SUPER_STATE_SHIFT)
#define SUPER_STATE_STANDBY		(0x0 << SUPER_STATE_SHIFT)
#define SUPER_STATE_IDLE		(0x1 << SUPER_STATE_SHIFT)
#define SUPER_STATE_RUN			(0x2 << SUPER_STATE_SHIFT)
#define SUPER_STATE_IRQ			(0x3 << SUPER_STATE_SHIFT)
#define SUPER_STATE_FIQ			(0x4 << SUPER_STATE_SHIFT)
#define SUPER_LP_DIV2_BYPASS		(0x1 << 16)
#define SUPER_SOURCE_MASK		0xF
#define	SUPER_FIQ_SOURCE_SHIFT		12
#define	SUPER_IRQ_SOURCE_SHIFT		8
#define	SUPER_RUN_SOURCE_SHIFT		4
#define	SUPER_IDLE_SOURCE_SHIFT		0

#define SUPER_CLK_DIVIDER		0x04
#define SUPER_CLOCK_SKIP_ENABLE		(0x1 << 31)
#define SUPER_CLOCK_DIV_U71_SHIFT	16
#define SUPER_CLOCK_DIV_U71_MASK	(0xff << SUPER_CLOCK_DIV_U71_SHIFT)
#define SUPER_CLOCK_SKIP_MUL_SHIFT	8
#define SUPER_CLOCK_SKIP_MUL_MASK	(0xff << SUPER_CLOCK_SKIP_MUL_SHIFT)
#define SUPER_CLOCK_SKIP_DIV_SHIFT	0
#define SUPER_CLOCK_SKIP_DIV_MASK	(0xff << SUPER_CLOCK_SKIP_DIV_SHIFT)
#define SUPER_CLOCK_SKIP_MASK		\
	(SUPER_CLOCK_SKIP_MUL_MASK | SUPER_CLOCK_SKIP_DIV_MASK)
#define SUPER_CLOCK_SKIP_TERM_MAX	256

#define BUS_CLK_DISABLE			(1<<3)
#define BUS_CLK_DIV_MASK		0x3

#define PMC_CTRL			0x0
 #define PMC_CTRL_BLINK_ENB		(1 << 7)

#define PMC_DPD_PADS_ORIDE		0x1c
 #define PMC_DPD_PADS_ORIDE_BLINK_ENB	(1 << 20)

#define PMC_BLINK_TIMER_DATA_ON_SHIFT	0
#define PMC_BLINK_TIMER_DATA_ON_MASK	0x7fff
#define PMC_BLINK_TIMER_ENB		(1 << 15)
#define PMC_BLINK_TIMER_DATA_OFF_SHIFT	16
#define PMC_BLINK_TIMER_DATA_OFF_MASK	0xffff

#define PMC_PLLP_WB0_OVERRIDE				0xf8
#define PMC_PLLP_WB0_OVERRIDE_PLLM_ENABLE		(1 << 12)
#define PMC_PLLP_WB0_OVERRIDE_PLLM_OVERRIDE		(1 << 11)
#define PMC_PLLM_WB0_OVERRIDE				0x1dc
#define PMC_PLLM_WB0_OVERRIDE_DIVP_MASK			(0x7<<15)
#define PMC_PLLM_WB0_OVERRIDE_DIVP_SHIFT		15
#define PMC_PLLM_WB0_OVERRIDE_DIVN_MASK			(0x3FF<<5)
#define PMC_PLLM_WB0_OVERRIDE_DIVN_SHIFT		5
#define PMC_PLLM_WB0_OVERRIDE_DIVM_MASK			(0x1F)
#define PMC_PLLM_WB0_OVERRIDE_DIVM_SHIFT		0

#define UTMIP_PLL_CFG2					0x488
#define UTMIP_PLL_CFG2_STABLE_COUNT(x)			(((x) & 0xfff) << 6)
#define UTMIP_PLL_CFG2_ACTIVE_DLY_COUNT(x)		(((x) & 0x3f) << 18)
#define UTMIP_PLL_CFG2_FORCE_PD_SAMP_A_POWERDOWN	(1 << 0)
#define UTMIP_PLL_CFG2_FORCE_PD_SAMP_B_POWERDOWN	(1 << 2)
#define UTMIP_PLL_CFG2_FORCE_PD_SAMP_C_POWERDOWN	(1 << 4)

#define UTMIP_PLL_CFG1					0x484
#define UTMIP_PLL_CFG1_ENABLE_DLY_COUNT(x)		(((x) & 0x1f) << 27)
#define UTMIP_PLL_CFG1_XTAL_FREQ_COUNT(x)		(((x) & 0xfff) << 0)
#define UTMIP_PLL_CFG1_FORCE_PLL_ENABLE_POWERDOWN	(1 << 14)
#define UTMIP_PLL_CFG1_FORCE_PLL_ACTIVE_POWERDOWN	(1 << 12)
#define UTMIP_PLL_CFG1_FORCE_PLLU_POWERDOWN		(1 << 16)

#define PLLE_BASE_CML_ENABLE		(1<<31)
#define PLLE_BASE_ENABLE		(1<<30)
#define PLLE_BASE_DIVCML_SHIFT		24
#define PLLE_BASE_DIVCML_MASK		(0xf<<PLLE_BASE_DIVCML_SHIFT)
#define PLLE_BASE_DIVP_SHIFT		16
#define PLLE_BASE_DIVP_MASK		(0x3f<<PLLE_BASE_DIVP_SHIFT)
#define PLLE_BASE_DIVN_SHIFT		8
#define PLLE_BASE_DIVN_MASK		(0xFF<<PLLE_BASE_DIVN_SHIFT)
#define PLLE_BASE_DIVM_SHIFT		0
#define PLLE_BASE_DIVM_MASK		(0xFF<<PLLE_BASE_DIVM_SHIFT)
#define PLLE_BASE_DIV_MASK		\
	(PLLE_BASE_DIVCML_MASK | PLLE_BASE_DIVP_MASK | \
	 PLLE_BASE_DIVN_MASK | PLLE_BASE_DIVM_MASK)
#define PLLE_BASE_DIV(m, n, p, cml)		\
	 (((cml)<<PLLE_BASE_DIVCML_SHIFT) | ((p)<<PLLE_BASE_DIVP_SHIFT) | \
	  ((n)<<PLLE_BASE_DIVN_SHIFT) | ((m)<<PLLE_BASE_DIVM_SHIFT))

#define PLLE_MISC_SETUP_BASE_SHIFT	16
#define PLLE_MISC_SETUP_BASE_MASK	(0xFFFF<<PLLE_MISC_SETUP_BASE_SHIFT)
#define PLLE_MISC_READY			(1<<15)
#define PLLE_MISC_LOCK			(1<<11)
#define PLLE_MISC_LOCK_ENABLE		(1<<9)
#define PLLE_MISC_SETUP_EX_SHIFT	2
#define PLLE_MISC_SETUP_EX_MASK		(0x3<<PLLE_MISC_SETUP_EX_SHIFT)
#define PLLE_MISC_SETUP_MASK		\
	  (PLLE_MISC_SETUP_BASE_MASK | PLLE_MISC_SETUP_EX_MASK)
#define PLLE_MISC_SETUP_VALUE		\
	  ((0x7<<PLLE_MISC_SETUP_BASE_SHIFT) | (0x0<<PLLE_MISC_SETUP_EX_SHIFT))

#define PLLE_SS_CTRL			0x68
#define	PLLE_SS_INCINTRV_SHIFT		24
#define	PLLE_SS_INCINTRV_MASK		(0x3f<<PLLE_SS_INCINTRV_SHIFT)
#define	PLLE_SS_INC_SHIFT		16
#define	PLLE_SS_INC_MASK		(0xff<<PLLE_SS_INC_SHIFT)
#define	PLLE_SS_MAX_SHIFT		0
#define	PLLE_SS_MAX_MASK		(0x1ff<<PLLE_SS_MAX_SHIFT)
#define PLLE_SS_COEFFICIENTS_MASK	\
	(PLLE_SS_INCINTRV_MASK | PLLE_SS_INC_MASK | PLLE_SS_MAX_MASK)
#define PLLE_SS_COEFFICIENTS_12MHZ	\
	((0x18<<PLLE_SS_INCINTRV_SHIFT) | (0x1<<PLLE_SS_INC_SHIFT) | \
	 (0x24<<PLLE_SS_MAX_SHIFT))
#define PLLE_SS_DISABLE			((1<<12) | (1<<11) | (1<<10))

#define PLLE_AUX			0x48c
#define PLLE_AUX_PLLP_SEL		(1<<2)
#define PLLE_AUX_CML_SATA_ENABLE	(1<<1)
#define PLLE_AUX_CML_PCIE_ENABLE	(1<<0)

#define	PMC_SATA_PWRGT			0x1ac
#define PMC_SATA_PWRGT_PLLE_IDDQ_VALUE	(1<<5)
#define PMC_SATA_PWRGT_PLLE_IDDQ_SWCTL	(1<<4)

#define ROUND_DIVIDER_UP	0
#define ROUND_DIVIDER_DOWN	1

/* PLLP default fixed rate in h/w controlled mode */
#define PLLP_DEFAULT_FIXED_RATE		216000000

/* Threshold to engage CPU clock skipper during CPU rate change */
#define SKIPPER_ENGAGE_RATE		 800000000

static void tegra3_pllp_init_dependencies(unsigned long pllp_rate);
static int tegra3_clk_shared_bus_update(struct clk *bus);
static int tegra3_emc_relock_set_rate(struct clk *emc, unsigned long old_rate,
	unsigned long new_rate, unsigned long new_pll_rate);

static unsigned long cpu_stay_on_backup_max;
static struct clk *emc_bridge;

static bool detach_shared_bus;
module_param(detach_shared_bus, bool, 0644);

static int skipper_delay = 10;
module_param(skipper_delay, int, 0644);

void tegra3_set_cpu_skipper_delay(int delay)
{
	skipper_delay = delay;
}

/**
* Structure defining the fields for USB UTMI clocks Parameters.
*/
struct utmi_clk_param
{
	/* Oscillator Frequency in KHz */
	u32 osc_frequency;
	/* UTMIP PLL Enable Delay Count  */
	u8 enable_delay_count;
	/* UTMIP PLL Stable count */
	u8 stable_count;
	/*  UTMIP PLL Active delay count */
	u8 active_delay_count;
	/* UTMIP PLL Xtal frequency count */
	u8 xtal_freq_count;
};

static const struct utmi_clk_param utmi_parameters[] =
{
/*	OSC_FREQUENCY, 	ENABLE_DLY, 	STABLE_CNT, 	ACTIVE_DLY, 	XTAL_FREQ_CNT */
	{13000000,	0x02,		0x33,		0x05,		0x7F},
	{19200000,	0x03,		0x4B,		0x06,		0xBB},
	{12000000,	0x02,		0x2F,		0x04,		0x76},
	{26000000,	0x04,		0x66,		0x09,		0xFE},
	{16800000,	0x03,		0x41,		0x0A,		0xA4},
};

static void __iomem *reg_clk_base = IO_ADDRESS(TEGRA_CLK_RESET_BASE);
static void __iomem *reg_pmc_base = IO_ADDRESS(TEGRA_PMC_BASE);
static void __iomem *misc_gp_hidrev_base = IO_ADDRESS(TEGRA_APB_MISC_BASE);

#define MISC_GP_HIDREV                  0x804

/*
 * Some peripheral clocks share an enable bit, so refcount the enable bits
 * in registers CLK_ENABLE_L, ... CLK_ENABLE_W, and protect refcount updates
 * with lock
 */
static DEFINE_SPINLOCK(periph_refcount_lock);
static int tegra_periph_clk_enable_refcount[CLK_OUT_ENB_NUM * 32];

#define clk_writel(value, reg) \
	__raw_writel(value, reg_clk_base + (reg))
#define clk_readl(reg) \
	__raw_readl(reg_clk_base + (reg))
#define pmc_writel(value, reg) \
	__raw_writel(value, reg_pmc_base + (reg))
#define pmc_readl(reg) \
	__raw_readl(reg_pmc_base + (reg))
#define chipid_readl() \
	__raw_readl(misc_gp_hidrev_base + MISC_GP_HIDREV)

#define clk_writel_delay(value, reg) 					\
	do {								\
		__raw_writel((value), reg_clk_base + (reg));	\
		udelay(2);						\
	} while (0)


static inline int clk_set_div(struct clk *c, u32 n)
{
	return clk_set_rate(c, (clk_get_rate(c->parent) + n-1) / n);
}

static inline u32 periph_clk_to_reg(
	struct clk *c, u32 reg_L, u32 reg_V, int offs)
{
	u32 reg = c->u.periph.clk_num / 32;
	BUG_ON(reg >= RST_DEVICES_NUM);
	if (reg < 3) {
		reg = reg_L + (reg * offs);
	} else {
		reg = reg_V + ((reg - 3) * offs);
	}
	return reg;
}

static int clk_div_x1_get_divider(unsigned long parent_rate, unsigned long rate,
			u32 max_x, u32 flags, u32 round_mode)
{
	s64 divider_ux1 = parent_rate;
	if (!rate)
		return -EINVAL;

	if (!(flags & DIV_U71_INT))
		divider_ux1 *= 2;

	if (round_mode == ROUND_DIVIDER_UP)
		divider_ux1 += rate - 1;
	do_div(divider_ux1, rate);

	if (flags & DIV_U71_INT)
		divider_ux1 *= 2;

	if (divider_ux1 - 2 < 0)
		return 0;

	if (divider_ux1 - 2 > max_x)
		return -EINVAL;

	return divider_ux1 - 2;
}

static int clk_div71_get_divider(unsigned long parent_rate, unsigned long rate,
				 u32 flags, u32 round_mode)
{
	return clk_div_x1_get_divider(parent_rate, rate, 0xFF,
			flags, round_mode);
}

static int clk_div151_get_divider(unsigned long parent_rate, unsigned long rate,
				 u32 flags, u32 round_mode)
{
	return clk_div_x1_get_divider(parent_rate, rate, 0xFFFF,
			flags, round_mode);
}

static int clk_div16_get_divider(unsigned long parent_rate, unsigned long rate)
{
	s64 divider_u16;

	divider_u16 = parent_rate;
	if (!rate)
		return -EINVAL;
	divider_u16 += rate - 1;
	do_div(divider_u16, rate);

	if (divider_u16 - 1 < 0)
		return 0;

	if (divider_u16 - 1 > 0xFFFF)
		return -EINVAL;

	return divider_u16 - 1;
}

/* clk_m functions */
static unsigned long tegra3_clk_m_autodetect_rate(struct clk *c)
{
	u32 osc_ctrl = clk_readl(OSC_CTRL);
	u32 auto_clock_control = osc_ctrl & ~OSC_CTRL_OSC_FREQ_MASK;
	u32 pll_ref_div = osc_ctrl & OSC_CTRL_PLL_REF_DIV_MASK;

	c->rate = tegra_clk_measure_input_freq();
	switch (c->rate) {
	case 12000000:
		auto_clock_control |= OSC_CTRL_OSC_FREQ_12MHZ;
		BUG_ON(pll_ref_div != OSC_CTRL_PLL_REF_DIV_1);
		break;
	case 13000000:
		auto_clock_control |= OSC_CTRL_OSC_FREQ_13MHZ;
		BUG_ON(pll_ref_div != OSC_CTRL_PLL_REF_DIV_1);
		break;
	case 19200000:
		auto_clock_control |= OSC_CTRL_OSC_FREQ_19_2MHZ;
		BUG_ON(pll_ref_div != OSC_CTRL_PLL_REF_DIV_1);
		break;
	case 26000000:
		auto_clock_control |= OSC_CTRL_OSC_FREQ_26MHZ;
		BUG_ON(pll_ref_div != OSC_CTRL_PLL_REF_DIV_1);
		break;
	case 16800000:
		auto_clock_control |= OSC_CTRL_OSC_FREQ_16_8MHZ;
		BUG_ON(pll_ref_div != OSC_CTRL_PLL_REF_DIV_1);
		break;
	case 38400000:
		auto_clock_control |= OSC_CTRL_OSC_FREQ_38_4MHZ;
		BUG_ON(pll_ref_div != OSC_CTRL_PLL_REF_DIV_2);
		break;
	case 48000000:
		auto_clock_control |= OSC_CTRL_OSC_FREQ_48MHZ;
		BUG_ON(pll_ref_div != OSC_CTRL_PLL_REF_DIV_4);
		break;
	default:
		pr_err("%s: Unexpected clock rate %ld", __func__, c->rate);
		BUG();
	}
	clk_writel(auto_clock_control, OSC_CTRL);
	return c->rate;
}

static void tegra3_clk_m_init(struct clk *c)
{
	pr_debug("%s on clock %s\n", __func__, c->name);
	tegra3_clk_m_autodetect_rate(c);
}

static int tegra3_clk_m_enable(struct clk *c)
{
	pr_debug("%s on clock %s\n", __func__, c->name);
	return 0;
}

static void tegra3_clk_m_disable(struct clk *c)
{
	pr_debug("%s on clock %s\n", __func__, c->name);
	WARN(1, "Attempting to disable main SoC clock\n");
}

static struct clk_ops tegra_clk_m_ops = {
	.init		= tegra3_clk_m_init,
	.enable		= tegra3_clk_m_enable,
	.disable	= tegra3_clk_m_disable,
};

static struct clk_ops tegra_clk_m_div_ops = {
	.enable		= tegra3_clk_m_enable,
};

/* PLL reference divider functions */
static void tegra3_pll_ref_init(struct clk *c)
{
	u32 pll_ref_div = clk_readl(OSC_CTRL) & OSC_CTRL_PLL_REF_DIV_MASK;
	pr_debug("%s on clock %s\n", __func__, c->name);

	switch (pll_ref_div) {
	case OSC_CTRL_PLL_REF_DIV_1:
		c->div = 1;
		break;
	case OSC_CTRL_PLL_REF_DIV_2:
		c->div = 2;
		break;
	case OSC_CTRL_PLL_REF_DIV_4:
		c->div = 4;
		break;
	default:
		pr_err("%s: Invalid pll ref divider %d", __func__, pll_ref_div);
		BUG();
	}
	c->mul = 1;
	c->state = ON;
}

static struct clk_ops tegra_pll_ref_ops = {
	.init		= tegra3_pll_ref_init,
	.enable		= tegra3_clk_m_enable,
	.disable	= tegra3_clk_m_disable,
};

/* super clock functions */
/* "super clocks" on tegra3 have two-stage muxes, fractional 7.1 divider and
 * clock skipping super divider.  We will ignore the clock skipping divider,
 * since we can't lower the voltage when using the clock skip, but we can if
 * we lower the PLL frequency. We will use 7.1 divider for CPU super-clock
 * only when its parent is a fixed rate PLL, since we can't change PLL rate
 * in this case.
 */
static void tegra3_super_clk_init(struct clk *c)
{
	u32 val;
	int source;
	int shift;
	const struct clk_mux_sel *sel;

	val = clk_readl(c->reg + SUPER_CLK_MUX);
	c->state = ON;
	BUG_ON(((val & SUPER_STATE_MASK) != SUPER_STATE_RUN) &&
		((val & SUPER_STATE_MASK) != SUPER_STATE_IDLE));
	shift = ((val & SUPER_STATE_MASK) == SUPER_STATE_IDLE) ?
		SUPER_IDLE_SOURCE_SHIFT : SUPER_RUN_SOURCE_SHIFT;
	source = (val >> shift) & SUPER_SOURCE_MASK;
	if (c->flags & DIV_2)
		source |= val & SUPER_LP_DIV2_BYPASS;
	for (sel = c->inputs; sel->input != NULL; sel++) {
		if (sel->value == source)
			break;
	}
	BUG_ON(sel->input == NULL);
	c->parent = sel->input;

	if (c->flags & DIV_U71) {
		/* Init safe 7.1 divider value (does not affect PLLX path).
		   Super skipper is enabled to be ready for emergency throttle,
		   but set 1:1 */
		c->mul = 2;
		c->div = 2;
		if (!(c->parent->flags & PLLX)) {
			val = clk_readl(c->reg + SUPER_CLK_DIVIDER);
			val &= SUPER_CLOCK_DIV_U71_MASK;
			val >>= SUPER_CLOCK_DIV_U71_SHIFT;
			val = max(val, c->u.cclk.div71);
			c->u.cclk.div71 = val;
			c->div += val;
		}
		val = SUPER_CLOCK_SKIP_ENABLE +
			(c->u.cclk.div71 << SUPER_CLOCK_DIV_U71_SHIFT);
		clk_writel(val, c->reg + SUPER_CLK_DIVIDER);
	}
	else
		clk_writel(0, c->reg + SUPER_CLK_DIVIDER);
}

static int tegra3_super_clk_enable(struct clk *c)
{
	return 0;
}

static void tegra3_super_clk_disable(struct clk *c)
{
	/* since tegra 3 has 2 CPU super clocks - low power lp-mode clock and
	   geared up g-mode super clock - mode switch may request to disable
	   either of them; accept request with no affect on h/w */
}

static int tegra3_super_clk_set_parent(struct clk *c, struct clk *p)
{
	u32 val;
	const struct clk_mux_sel *sel;
	int shift;

	val = clk_readl(c->reg + SUPER_CLK_MUX);;
	BUG_ON(((val & SUPER_STATE_MASK) != SUPER_STATE_RUN) &&
		((val & SUPER_STATE_MASK) != SUPER_STATE_IDLE));
	shift = ((val & SUPER_STATE_MASK) == SUPER_STATE_IDLE) ?
		SUPER_IDLE_SOURCE_SHIFT : SUPER_RUN_SOURCE_SHIFT;
	for (sel = c->inputs; sel->input != NULL; sel++) {
		if (sel->input == p) {
			/* For LP mode super-clock switch between PLLX direct
			   and divided-by-2 outputs is allowed only when other
			   than PLLX clock source is current parent */
			if ((c->flags & DIV_2) && (p->flags & PLLX) &&
			    ((sel->value ^ val) & SUPER_LP_DIV2_BYPASS)) {
				if (c->parent->flags & PLLX)
					return -EINVAL;
				val ^= SUPER_LP_DIV2_BYPASS;
				clk_writel_delay(val, c->reg);
			}
			val &= ~(SUPER_SOURCE_MASK << shift);
			val |= (sel->value & SUPER_SOURCE_MASK) << shift;

			/* 7.1 divider for CPU super-clock does not affect
			   PLLX path */
			if (c->flags & DIV_U71) {
				u32 div = 0;
				if (!(p->flags & PLLX)) {
					div = clk_readl(c->reg +
							SUPER_CLK_DIVIDER);
					div &= SUPER_CLOCK_DIV_U71_MASK;
					div >>= SUPER_CLOCK_DIV_U71_SHIFT;
				}
				c->div = div + 2;
				c->mul = 2;
			}

			if (c->refcnt)
				tegra_clk_prepare_enable(p);

			clk_writel_delay(val, c->reg);

			if (c->refcnt && c->parent)
				tegra_clk_disable_unprepare(c->parent);

			clk_reparent(c, p);
			return 0;
		}
	}
	return -EINVAL;
}

static DEFINE_SPINLOCK(super_divider_lock);

static void tegra3_super_clk_divider_update(struct clk *c, u8 div)
{
	u32 val;
	unsigned long flags;

	spin_lock_irqsave(&super_divider_lock, flags);
	val = clk_readl(c->reg + SUPER_CLK_DIVIDER);
	val &= ~SUPER_CLOCK_DIV_U71_MASK;
	val |= div << SUPER_CLOCK_DIV_U71_SHIFT;
	clk_writel(val, c->reg + SUPER_CLK_DIVIDER);
	spin_unlock_irqrestore(&super_divider_lock, flags);
	udelay(2);
}

static void tegra3_super_clk_skipper_update(struct clk *c, u8 mul, u8 div)
{
	u32 val;
	unsigned long flags;

	spin_lock_irqsave(&super_divider_lock, flags);
	val = clk_readl(c->reg + SUPER_CLK_DIVIDER);

	/* multiplier or divider value = the respective field + 1 */
	if (mul && div) {
		u32 old_mul = ((val & SUPER_CLOCK_SKIP_MUL_MASK) >>
			       SUPER_CLOCK_SKIP_MUL_SHIFT) + 1;
		u32 old_div = ((val & SUPER_CLOCK_SKIP_DIV_MASK) >>
			       SUPER_CLOCK_SKIP_DIV_SHIFT) + 1;

		if (mul >= div) {
			/* improper fraction is only used to reciprocate the
			   previous proper one - the division below is exact */
			old_mul /= div;
			old_div /= mul;
		} else {
			old_mul *= mul;
			old_div *= div;
		}
		mul = (old_mul <= SUPER_CLOCK_SKIP_TERM_MAX) ?
			old_mul : SUPER_CLOCK_SKIP_TERM_MAX;
		div = (old_div <= SUPER_CLOCK_SKIP_TERM_MAX) ?
			old_div : SUPER_CLOCK_SKIP_TERM_MAX;
	}

	if (!mul || (mul >= div)) {
		mul = 1;
		div = 1;
	}
	val &= ~SUPER_CLOCK_SKIP_MASK;
	val |= SUPER_CLOCK_SKIP_ENABLE |
		((mul - 1) << SUPER_CLOCK_SKIP_MUL_SHIFT) |
		((div - 1) << SUPER_CLOCK_SKIP_DIV_SHIFT);

	clk_writel(val, c->reg + SUPER_CLK_DIVIDER);
	spin_unlock_irqrestore(&super_divider_lock, flags);
}

/*
 * Do not use super clocks "skippers", since dividing using a clock skipper
 * does not allow the voltage to be scaled down. Instead adjust the rate of
 * the parent clock. This requires that the parent of a super clock have no
 * other children, otherwise the rate will change underneath the other
 * children. Special case: if fixed rate PLL is CPU super clock parent the
 * rate of this PLL can't be changed, and it has many other children. In
 * this case use 7.1 fractional divider to adjust the super clock rate.
 */
static int tegra3_super_clk_set_rate(struct clk *c, unsigned long rate)
{
	if ((c->flags & DIV_U71) && (c->parent->flags & PLL_FIXED)) {
		int div = clk_div71_get_divider(c->parent->u.pll.fixed_rate,
					rate, c->flags, ROUND_DIVIDER_DOWN);
		if (div < 0)
			return div;

		tegra3_super_clk_divider_update(c, div);
		c->u.cclk.div71 = div;
		c->div = div + 2;
		c->mul = 2;
		return 0;
	}
	return clk_set_rate(c->parent, rate);
}

static struct clk_ops tegra_super_ops = {
	.init			= tegra3_super_clk_init,
	.enable			= tegra3_super_clk_enable,
	.disable		= tegra3_super_clk_disable,
	.set_parent		= tegra3_super_clk_set_parent,
	.set_rate		= tegra3_super_clk_set_rate,
};

static int tegra3_twd_clk_set_rate(struct clk *c, unsigned long rate)
{
	/* The input value 'rate' is the clock rate of the CPU complex. */
	c->rate = (rate * c->mul) / c->div;
	return 0;
}

static struct clk_ops tegra3_twd_ops = {
	.set_rate	= tegra3_twd_clk_set_rate,
};

static struct clk tegra3_clk_twd = {
	/* NOTE: The twd clock must have *NO* parent. It's rate is directly
		 updated by tegra3_cpu_cmplx_clk_set_rate() because the
		 frequency change notifer for the twd is called in an
		 atomic context which cannot take a mutex. */
	.name     = "twd",
	.ops      = &tegra3_twd_ops,
	.max_rate = 1400000000,	/* Same as tegra_clk_cpu_cmplx.max_rate */
	.mul      = 1,
	.div      = 2,
};

/* virtual cpu clock functions */
/* some clocks can not be stopped (cpu, memory bus) while the SoC is running.
   To change the frequency of these clocks, the parent pll may need to be
   reprogrammed, so the clock must be moved off the pll, the pll reprogrammed,
   and then the clock moved back to the pll. Clock skipper maybe temporarily
   engaged during the switch to limit frequency jumps. To hide this sequence,
   a virtual clock handles it.
 */
static void tegra3_cpu_clk_init(struct clk *c)
{
	c->state = (!is_lp_cluster() == (c->u.cpu.mode == MODE_G))? ON : OFF;
}

static int tegra3_cpu_clk_enable(struct clk *c)
{
	return 0;
}

static void tegra3_cpu_clk_disable(struct clk *c)
{
	/* since tegra 3 has 2 virtual CPU clocks - low power lp-mode clock
	   and geared up g-mode clock - mode switch may request to disable
	   either of them; accept request with no affect on h/w */
}

static int tegra3_cpu_clk_set_rate(struct clk *c, unsigned long rate)
{
	int ret = 0;
	bool skipped = false;
	bool skip = (c->u.cpu.mode == MODE_G) && skipper_delay;
	bool skip_from_backup = skip && (rate >= SKIPPER_ENGAGE_RATE);
	bool skip_to_backup =
		skip && (clk_get_rate_all_locked(c) >= SKIPPER_ENGAGE_RATE);

	if (c->dvfs) {
		if (!c->dvfs->dvfs_rail)
			return -ENOSYS;
		else if ((!c->dvfs->dvfs_rail->disabled) &&
			  (!c->dvfs->dvfs_rail->reg) &&
			  (clk_get_rate_locked(c) < rate)) {
			WARN(1, "Increasing CPU rate while regulator is not"
				" ready may overclock CPU\n");
			return -ENOSYS;
		}
	}

	/*
	 * Take an extra reference to the main pll so it doesn't turn
	 * off when we move the cpu off of it
	 */
	tegra_clk_prepare_enable(c->u.cpu.main);

	if (c->parent->parent != c->u.cpu.backup) {
		if (skip_to_backup) {
			/* on G CPU use 1/2 skipper step for main <=> backup */
			skipped = true;
			tegra3_super_clk_skipper_update(c->parent, 1, 2);
			udelay(skipper_delay);
		}

		ret = clk_set_parent(c->parent, c->u.cpu.backup);
		if (ret) {
			pr_err("Failed to switch cpu to clock %s\n",
			       c->u.cpu.backup->name);
			goto out;
		}

		if (skipped && !skip_from_backup) {
			skipped = false;
			tegra3_super_clk_skipper_update(c->parent, 2, 1);
		}
	}

	if (rate <= cpu_stay_on_backup_max) {
		ret = clk_set_rate(c->parent, rate);
		if (ret)
			pr_err("Failed to set cpu rate %lu on backup source\n",
			       rate);
		goto out;
	} else {
		ret = clk_set_rate(c->parent, c->u.cpu.backup_rate);
		if (ret) {
			pr_err("Failed to set cpu rate %lu on backup source\n",
			       c->u.cpu.backup_rate);
			goto out;
		}
	}

	if (rate != clk_get_rate(c->u.cpu.main)) {
		ret = clk_set_rate(c->u.cpu.main, rate);
		if (ret) {
			pr_err("Failed to change cpu pll to %lu\n", rate);
			goto out;
		}
	}

	if (!skipped && skip_from_backup) {
		skipped = true;
		tegra3_super_clk_skipper_update(c->parent, 1, 2);
	}

	ret = clk_set_parent(c->parent, c->u.cpu.main);
	if (ret) {
		pr_err("Failed to switch cpu to clock %s\n", c->u.cpu.main->name);
		goto out;
	}

out:
	if (skipped) {
		udelay(skipper_delay);
		tegra3_super_clk_skipper_update(c->parent, 2, 1);
	}
	tegra_clk_disable_unprepare(c->u.cpu.main);
	return ret;
}

static struct clk_ops tegra_cpu_ops = {
	.init     = tegra3_cpu_clk_init,
	.enable   = tegra3_cpu_clk_enable,
	.disable  = tegra3_cpu_clk_disable,
	.set_rate = tegra3_cpu_clk_set_rate,
};


static void tegra3_cpu_cmplx_clk_init(struct clk *c)
{
	int i = !!is_lp_cluster();

	BUG_ON(c->inputs[0].input->u.cpu.mode != MODE_G);
	BUG_ON(c->inputs[1].input->u.cpu.mode != MODE_LP);
	c->parent = c->inputs[i].input;
}

/* cpu complex clock provides second level vitualization (on top of
   cpu virtual cpu rate control) in order to hide the CPU mode switch
   sequence */
#if PARAMETERIZE_CLUSTER_SWITCH
static unsigned int switch_delay;
static unsigned int switch_flags;
static DEFINE_SPINLOCK(parameters_lock);

void tegra_cluster_switch_set_parameters(unsigned int us, unsigned int flags)
{
	spin_lock(&parameters_lock);
	switch_delay = us;
	switch_flags = flags;
	spin_unlock(&parameters_lock);
}
#endif

static int tegra3_cpu_cmplx_clk_enable(struct clk *c)
{
	return 0;
}

static void tegra3_cpu_cmplx_clk_disable(struct clk *c)
{
	pr_debug("%s on clock %s\n", __func__, c->name);

	/* oops - don't disable the CPU complex clock! */
	BUG();
}

static int tegra3_cpu_cmplx_clk_set_rate(struct clk *c, unsigned long rate)
{
	unsigned long flags;
	int ret;
	struct clk *parent = c->parent;

	if (!parent->ops || !parent->ops->set_rate)
		return -ENOSYS;

	clk_lock_save(parent, &flags);

	ret = clk_set_rate_locked(parent, rate);

	/* We can't parent the twd to directly to the CPU complex because
	   the TWD frequency update notifier is called in an atomic context
	   and the CPU frequency update requires a mutex. Update the twd
	   clock rate with the new CPU complex rate. */
	clk_set_rate(&tegra3_clk_twd, clk_get_rate_locked(parent));

	clk_unlock_restore(parent, &flags);

	return ret;
}

static int tegra3_cpu_cmplx_clk_set_parent(struct clk *c, struct clk *p)
{
	int ret;
	unsigned int flags, delay;
	const struct clk_mux_sel *sel;
	unsigned long rate;
	if (!c->parent)
		return -EINVAL;

	rate = clk_get_rate(c->parent);

	pr_debug("%s: %s %s\n", __func__, c->name, p->name);
	BUG_ON(c->parent->u.cpu.mode != (is_lp_cluster() ? MODE_LP : MODE_G));

	for (sel = c->inputs; sel->input != NULL; sel++) {
		if (sel->input == p)
			break;
	}
	if (!sel->input)
		return -EINVAL;

#if PARAMETERIZE_CLUSTER_SWITCH
	spin_lock(&parameters_lock);
	flags = switch_flags;
	delay = switch_delay;
	switch_flags = 0;
	spin_unlock(&parameters_lock);

	if (flags) {
		/* over/under-clocking after switch - allow, but update rate */
		if ((rate > p->max_rate) || (rate < p->min_rate)) {
			unsigned long fl;

			rate = rate > p->max_rate ? p->max_rate : p->min_rate;
			ret = clk_set_rate(c->parent, rate);
			if (ret) {
				pr_err("%s: Failed to set rate %lu for %s\n",
				        __func__, rate, p->name);
				return ret;
			}
			clk_lock_save(c->parent, &fl);
			clk_set_rate(&tegra3_clk_twd, clk_get_rate_locked(c->parent));
			clk_unlock_restore(c->parent, &fl);
		}
	} else
#endif
	{
		if (p == c->parent)		/* already switched - exit*/
			return 0;

		if (rate > p->max_rate) {	/* over-clocking - no switch */
			pr_warn("%s: No %s mode switch to %s at rate %lu\n",
				 __func__, c->name, p->name, rate);
			return -ECANCELED;
		}
		flags = TEGRA_POWER_CLUSTER_IMMEDIATE;
		delay = 0;
	}
	flags |= (p->u.cpu.mode == MODE_LP) ? TEGRA_POWER_CLUSTER_LP :
		TEGRA_POWER_CLUSTER_G;

	/* Since in both LP and G mode CPU main and backup sources are the
	   same, set rate on the new parent just synchronizes super-clock
	   muxes before mode switch with no PLL re-locking */
	ret = clk_set_rate(p, rate);
	if (ret) {
		pr_err("%s: Failed to set rate %lu for %s\n",
		       __func__, rate, p->name);
		return ret;
	}

	/* Enabling new parent scales new mode voltage rail in advanvce
	   before the switch happens*/
	if (c->refcnt)
		tegra_clk_prepare_enable(p);

	/* switch CPU mode */
	ret = tegra_cluster_control(delay, flags);
	if (ret) {
		if (c->refcnt)
			tegra_clk_disable_unprepare(p);
		pr_err("%s: Failed to switch %s mode to %s\n",
		       __func__, c->name, p->name);
		return ret;
	}

	/* Disabling old parent scales old mode voltage rail */
	if (c->refcnt && c->parent)
		tegra_clk_disable_unprepare(c->parent);

	clk_reparent(c, p);
	return 0;
}

static long tegra3_cpu_cmplx_round_rate(struct clk *c,
	unsigned long rate)
{
	if (rate > c->parent->max_rate)
		rate = c->parent->max_rate;
	else if (rate < c->parent->min_rate)
		rate = c->parent->min_rate;
	return rate;
}

static struct clk_ops tegra_cpu_cmplx_ops = {
	.init     = tegra3_cpu_cmplx_clk_init,
	.enable   = tegra3_cpu_cmplx_clk_enable,
	.disable  = tegra3_cpu_cmplx_clk_disable,
	.set_rate = tegra3_cpu_cmplx_clk_set_rate,
	.set_parent = tegra3_cpu_cmplx_clk_set_parent,
	.round_rate = tegra3_cpu_cmplx_round_rate,
};

/* virtual cop clock functions. Used to acquire the fake 'cop' clock to
 * reset the COP block (i.e. AVP) */
static void tegra3_cop_clk_reset(struct clk *c, bool assert)
{
	unsigned long reg = assert ? RST_DEVICES_SET_L : RST_DEVICES_CLR_L;

	pr_debug("%s %s\n", __func__, assert ? "assert" : "deassert");
	clk_writel(1 << 1, reg);
}

static struct clk_ops tegra_cop_ops = {
	.reset    = tegra3_cop_clk_reset,
};

/* bus clock functions */
static void tegra3_bus_clk_init(struct clk *c)
{
	u32 val = clk_readl(c->reg);
	c->state = ((val >> c->reg_shift) & BUS_CLK_DISABLE) ? OFF : ON;
	c->div = ((val >> c->reg_shift) & BUS_CLK_DIV_MASK) + 1;
	c->mul = 1;
}

static int tegra3_bus_clk_enable(struct clk *c)
{
	u32 val = clk_readl(c->reg);
	val &= ~(BUS_CLK_DISABLE << c->reg_shift);
	clk_writel(val, c->reg);
	return 0;
}

static void tegra3_bus_clk_disable(struct clk *c)
{
	u32 val = clk_readl(c->reg);
	val |= BUS_CLK_DISABLE << c->reg_shift;
	clk_writel(val, c->reg);
}

static int tegra3_bus_clk_set_rate(struct clk *c, unsigned long rate)
{
	u32 val = clk_readl(c->reg);
	unsigned long parent_rate = clk_get_rate(c->parent);
	int i;
	for (i = 1; i <= 4; i++) {
		if (rate >= parent_rate / i) {
			val &= ~(BUS_CLK_DIV_MASK << c->reg_shift);
			val |= (i - 1) << c->reg_shift;
			clk_writel(val, c->reg);
			c->div = i;
			c->mul = 1;
			return 0;
		}
	}
	return -EINVAL;
}

static struct clk_ops tegra_bus_ops = {
	.init			= tegra3_bus_clk_init,
	.enable			= tegra3_bus_clk_enable,
	.disable		= tegra3_bus_clk_disable,
	.set_rate		= tegra3_bus_clk_set_rate,
};

/* Virtual system bus complex clock is used to hide the sequence of
   changing sclk/hclk/pclk parents and dividers to configure requested
   sclk target rate. */
static void tegra3_sbus_cmplx_init(struct clk *c)
{
	unsigned long rate;

	c->max_rate = c->parent->max_rate;
	c->min_rate = c->parent->min_rate;

	/* Threshold must be an exact proper factor of low range parent,
	   and both low/high range parents have 7.1 fractional dividers */
	rate = clk_get_rate(c->u.system.sclk_low->parent);
	if (c->u.system.threshold) {
		BUG_ON(c->u.system.threshold > rate) ;
		BUG_ON((rate % c->u.system.threshold) != 0);
	}
	BUG_ON(!(c->u.system.sclk_low->flags & DIV_U71));
	BUG_ON(!(c->u.system.sclk_high->flags & DIV_U71));
}

/* This special sbus round function is implemented because:
 *
 * (a) fractional dividers can not be used to derive system bus clock with one
 * exception: 1 : 2.5 divider is allowed at 1.2V and above (and we do need this
 * divider to reach top sbus frequencies from high frequency source).
 *
 * (b) since sbus is a shared bus, and its frequency is set to the highest
 * enabled shared_bus_user clock, the target rate should be rounded up divider
 * ladder (if max limit allows it) - for pll_div and peripheral_div common is
 * rounding down - special case again.
 *
 * Note that final rate is trimmed (not rounded up) to avoid spiraling up in
 * recursive calls. Lost 1Hz is added in tegra3_sbus_cmplx_set_rate before
 * actually setting divider rate.
 */
static unsigned long sclk_high_2_5_rate;
static bool sclk_high_2_5_valid;

static long tegra3_sbus_cmplx_round_rate(struct clk *c, unsigned long rate)
{
	int i, divider;
	unsigned long source_rate, round_rate;
	struct clk *new_parent;

	rate = max(rate, c->min_rate);

	if (!sclk_high_2_5_rate) {
		source_rate = clk_get_rate(c->u.system.sclk_high->parent);
		sclk_high_2_5_rate = 2 * source_rate / 5;
		i = tegra_dvfs_predict_millivolts(c, sclk_high_2_5_rate);
		if (!IS_ERR_VALUE(i) && (i >= 1200) &&
		    (sclk_high_2_5_rate <= c->max_rate))
			sclk_high_2_5_valid = true;
	}

	new_parent = (rate <= c->u.system.threshold) ?
		c->u.system.sclk_low : c->u.system.sclk_high;
	source_rate = clk_get_rate(new_parent->parent);

	divider = clk_div71_get_divider(source_rate, rate,
		new_parent->flags | DIV_U71_INT, ROUND_DIVIDER_DOWN);
	if (divider < 0)
		return divider;

	round_rate = source_rate * 2 / (divider + 2);
	if (round_rate > c->max_rate) {
		divider += 2;
		round_rate = source_rate * 2 / (divider + 2);
	}

	if (new_parent == c->u.system.sclk_high) {
		/* Check if 1 : 2.5 ratio provides better approximation */
		if (sclk_high_2_5_valid) {
			if (((sclk_high_2_5_rate < round_rate) &&
			    (sclk_high_2_5_rate >= rate)) ||
			    ((round_rate < sclk_high_2_5_rate) &&
			     (round_rate < rate)))
				round_rate = sclk_high_2_5_rate;
		}

		if (round_rate <= c->u.system.threshold)
			round_rate = c->u.system.threshold;
	}
	return round_rate;
}

static int tegra3_sbus_cmplx_set_rate(struct clk *c, unsigned long rate)
{
	int ret;
	struct clk *new_parent;

	/* - select the appropriate sclk parent
	   - keep hclk at the same rate as sclk
	   - set pclk at 1:2 rate of hclk unless pclk minimum is violated,
	     in the latter case switch to 1:1 ratio */

	if (rate >= c->u.system.pclk->min_rate * 2) {
		ret = clk_set_div(c->u.system.pclk, 2);
		if (ret) {
			pr_err("Failed to set 1 : 2 pclk divider\n");
			return ret;
		}
	}

	new_parent = (rate <= c->u.system.threshold) ?
		c->u.system.sclk_low : c->u.system.sclk_high;

	ret = clk_set_rate(new_parent, rate + 1);
	if (ret) {
		pr_err("Failed to set sclk source %s to %lu\n",
		       new_parent->name, rate);
		return ret;
	}

	if (new_parent != clk_get_parent(c->parent)) {
		ret = clk_set_parent(c->parent, new_parent);
		if (ret) {
			pr_err("Failed to switch sclk source to %s\n",
			       new_parent->name);
			return ret;
		}
	}

	if (rate < c->u.system.pclk->min_rate * 2) {
		ret = clk_set_div(c->u.system.pclk, 1);
		if (ret) {
			pr_err("Failed to set 1 : 1 pclk divider\n");
			return ret;
		}
	}

	return 0;
}

static struct clk_ops tegra_sbus_cmplx_ops = {
	.init = tegra3_sbus_cmplx_init,
	.set_rate = tegra3_sbus_cmplx_set_rate,
	.round_rate = tegra3_sbus_cmplx_round_rate,
	.shared_bus_update = tegra3_clk_shared_bus_update,
};

/* Blink output functions */

static void tegra3_blink_clk_init(struct clk *c)
{
	u32 val;

	val = pmc_readl(PMC_CTRL);
	c->state = (val & PMC_CTRL_BLINK_ENB) ? ON : OFF;
	c->mul = 1;
	val = pmc_readl(c->reg);

	if (val & PMC_BLINK_TIMER_ENB) {
		unsigned int on_off;

		on_off = (val >> PMC_BLINK_TIMER_DATA_ON_SHIFT) &
			PMC_BLINK_TIMER_DATA_ON_MASK;
		val >>= PMC_BLINK_TIMER_DATA_OFF_SHIFT;
		val &= PMC_BLINK_TIMER_DATA_OFF_MASK;
		on_off += val;
		/* each tick in the blink timer is 4 32KHz clocks */
		c->div = on_off * 4;
	} else {
		c->div = 1;
	}
}

static int tegra3_blink_clk_enable(struct clk *c)
{
	u32 val;

	val = pmc_readl(PMC_DPD_PADS_ORIDE);
	pmc_writel(val | PMC_DPD_PADS_ORIDE_BLINK_ENB, PMC_DPD_PADS_ORIDE);

	val = pmc_readl(PMC_CTRL);
	pmc_writel(val | PMC_CTRL_BLINK_ENB, PMC_CTRL);

	return 0;
}

static void tegra3_blink_clk_disable(struct clk *c)
{
	u32 val;

	val = pmc_readl(PMC_CTRL);
	pmc_writel(val & ~PMC_CTRL_BLINK_ENB, PMC_CTRL);

	val = pmc_readl(PMC_DPD_PADS_ORIDE);
	pmc_writel(val & ~PMC_DPD_PADS_ORIDE_BLINK_ENB, PMC_DPD_PADS_ORIDE);
}

static int tegra3_blink_clk_set_rate(struct clk *c, unsigned long rate)
{
	unsigned long parent_rate = clk_get_rate(c->parent);
	if (rate >= parent_rate) {
		c->div = 1;
		pmc_writel(0, c->reg);
	} else {
		unsigned int on_off;
		u32 val;

		on_off = DIV_ROUND_UP(parent_rate / 8, rate);
		c->div = on_off * 8;

		val = (on_off & PMC_BLINK_TIMER_DATA_ON_MASK) <<
			PMC_BLINK_TIMER_DATA_ON_SHIFT;
		on_off &= PMC_BLINK_TIMER_DATA_OFF_MASK;
		on_off <<= PMC_BLINK_TIMER_DATA_OFF_SHIFT;
		val |= on_off;
		val |= PMC_BLINK_TIMER_ENB;
		pmc_writel(val, c->reg);
	}

	return 0;
}

static struct clk_ops tegra_blink_clk_ops = {
	.init			= &tegra3_blink_clk_init,
	.enable			= &tegra3_blink_clk_enable,
	.disable		= &tegra3_blink_clk_disable,
	.set_rate		= &tegra3_blink_clk_set_rate,
};

/* PLL Functions */
static int tegra3_pll_clk_wait_for_lock(struct clk *c, u32 lock_reg, u32 lock_bit)
{
#if USE_PLL_LOCK_BITS
	int i;
	for (i = 0; i < c->u.pll.lock_delay; i++) {
		udelay(2);		/* timeout = 2 * lock time */
		if (clk_readl(lock_reg) & lock_bit) {
			udelay(PLL_POST_LOCK_DELAY);
			return 0;
		}
	}
	pr_err("Timed out waiting for lock bit on pll %s", c->name);
	return -1;
#endif
	udelay(c->u.pll.lock_delay);

	return 0;
}


static void tegra3_utmi_param_configure(struct clk *c)
{
	u32 reg;
	int i;
	unsigned long main_rate =
		clk_get_rate(c->parent->parent);

	for (i = 0; i < ARRAY_SIZE(utmi_parameters); i++) {
		if (main_rate == utmi_parameters[i].osc_frequency) {
			break;
		}
	}

	if (i >= ARRAY_SIZE(utmi_parameters)) {
		pr_err("%s: Unexpected main rate %lu\n", __func__, main_rate);
		return;
	}

	reg = clk_readl(UTMIP_PLL_CFG2);

	/* Program UTMIP PLL stable and active counts */
	/* [FIXME] arclk_rst.h says WRONG! This should be 1ms -> 0x50 Check! */
	reg &= ~UTMIP_PLL_CFG2_STABLE_COUNT(~0);
	reg |= UTMIP_PLL_CFG2_STABLE_COUNT(
			utmi_parameters[i].stable_count);

	reg &= ~UTMIP_PLL_CFG2_ACTIVE_DLY_COUNT(~0);

	reg |= UTMIP_PLL_CFG2_ACTIVE_DLY_COUNT(
			utmi_parameters[i].active_delay_count);

	/* Remove power downs from UTMIP PLL control bits */
	reg &= ~UTMIP_PLL_CFG2_FORCE_PD_SAMP_A_POWERDOWN;
	reg &= ~UTMIP_PLL_CFG2_FORCE_PD_SAMP_B_POWERDOWN;
	reg &= ~UTMIP_PLL_CFG2_FORCE_PD_SAMP_C_POWERDOWN;

	clk_writel(reg, UTMIP_PLL_CFG2);

	/* Program UTMIP PLL delay and oscillator frequency counts */
	reg = clk_readl(UTMIP_PLL_CFG1);
	reg &= ~UTMIP_PLL_CFG1_ENABLE_DLY_COUNT(~0);

	reg |= UTMIP_PLL_CFG1_ENABLE_DLY_COUNT(
		utmi_parameters[i].enable_delay_count);

	reg &= ~UTMIP_PLL_CFG1_XTAL_FREQ_COUNT(~0);
	reg |= UTMIP_PLL_CFG1_XTAL_FREQ_COUNT(
		utmi_parameters[i].xtal_freq_count);

	/* Remove power downs from UTMIP PLL control bits */
	reg &= ~UTMIP_PLL_CFG1_FORCE_PLL_ENABLE_POWERDOWN;
	reg &= ~UTMIP_PLL_CFG1_FORCE_PLL_ACTIVE_POWERDOWN;
	reg &= ~UTMIP_PLL_CFG1_FORCE_PLLU_POWERDOWN;

	clk_writel(reg, UTMIP_PLL_CFG1);
}

static void tegra3_pll_m_override_update(struct clk *c, bool init)
{
	u32 val = pmc_readl(PMC_PLLP_WB0_OVERRIDE);

	if (!(val & PMC_PLLP_WB0_OVERRIDE_PLLM_OVERRIDE))
		return;

	/* override PLLM state with PMC settings */
	c->state = (val & PMC_PLLP_WB0_OVERRIDE_PLLM_ENABLE) ? ON : OFF;

	val = pmc_readl(PMC_PLLM_WB0_OVERRIDE);
	c->mul = (val & PMC_PLLM_WB0_OVERRIDE_DIVN_MASK) >>
		PMC_PLLM_WB0_OVERRIDE_DIVN_SHIFT;
	c->div = (val & PMC_PLLM_WB0_OVERRIDE_DIVM_MASK) >>
		PMC_PLLM_WB0_OVERRIDE_DIVM_SHIFT;
	c->div *= (0x1 << ((val & PMC_PLLM_WB0_OVERRIDE_DIVP_MASK) >>
			   PMC_PLLM_WB0_OVERRIDE_DIVP_SHIFT));

	/* Save initial override settings in Scratch2 register; will be used by
	   LP0 entry code to restore PLLM boot configuration */
	if (init)
		pmc_writel(val, PMC_SCRATCH2);
}

static void tegra3_pll_clk_init(struct clk *c)
{
	u32 val = clk_readl(c->reg + PLL_BASE);

	c->state = (val & PLL_BASE_ENABLE) ? ON : OFF;

	if (c->flags & PLL_FIXED && !(val & PLL_BASE_OVERRIDE)) {
		const struct clk_pll_freq_table *sel;
		unsigned long input_rate = clk_get_rate(c->parent);
		c->u.pll.fixed_rate = PLLP_DEFAULT_FIXED_RATE;

		for (sel = c->u.pll.freq_table; sel->input_rate != 0; sel++) {
			if (sel->input_rate == input_rate &&
				sel->output_rate == c->u.pll.fixed_rate) {
				c->mul = sel->n;
				c->div = sel->m * sel->p;
				return;
			}
		}
		pr_err("Clock %s has unknown fixed frequency\n", c->name);
		BUG();
	} else if (val & PLL_BASE_BYPASS) {
		c->mul = 1;
		c->div = 1;
	} else {
		c->mul = (val & PLL_BASE_DIVN_MASK) >> PLL_BASE_DIVN_SHIFT;
		c->div = (val & PLL_BASE_DIVM_MASK) >> PLL_BASE_DIVM_SHIFT;
		if (c->flags & PLLU)
			c->div *= (val & PLLU_BASE_POST_DIV) ? 1 : 2;
		else
			c->div *= (0x1 << ((val & PLL_BASE_DIVP_MASK) >>
					PLL_BASE_DIVP_SHIFT));
	}

	if (c->flags & PLL_FIXED) {
		c->u.pll.fixed_rate = clk_get_rate_locked(c);
	}

	if (c->flags & PLLU) {
		tegra3_utmi_param_configure(c);
	}

	if (c->flags & PLLM)
		tegra3_pll_m_override_update(c, true);
}

static int tegra3_pll_clk_enable(struct clk *c)
{
	u32 val;
	pr_debug("%s on clock %s\n", __func__, c->name);

#if USE_PLL_LOCK_BITS
	/* toggle lock enable bit to reset lock detection circuit (couple
	   register reads provide enough duration for reset pulse) */
	val = clk_readl(c->reg + PLL_MISC(c));
	val &= ~PLL_MISC_LOCK_ENABLE(c);
	clk_writel(val, c->reg + PLL_MISC(c));
	val = clk_readl(c->reg + PLL_MISC(c));
	val = clk_readl(c->reg + PLL_MISC(c));
	val |= PLL_MISC_LOCK_ENABLE(c);
	clk_writel(val, c->reg + PLL_MISC(c));
#endif
	val = clk_readl(c->reg + PLL_BASE);
	val &= ~PLL_BASE_BYPASS;
	val |= PLL_BASE_ENABLE;
	clk_writel(val, c->reg + PLL_BASE);

	if (c->flags & PLLM) {
		val = pmc_readl(PMC_PLLP_WB0_OVERRIDE);
		val |= PMC_PLLP_WB0_OVERRIDE_PLLM_ENABLE;
		pmc_writel(val, PMC_PLLP_WB0_OVERRIDE);
		pmc_readl(PMC_PLLP_WB0_OVERRIDE);
	}

	tegra3_pll_clk_wait_for_lock(c, c->reg + PLL_BASE, PLL_BASE_LOCK);

	return 0;
}

static void tegra3_pll_clk_disable(struct clk *c)
{
	u32 val;
	pr_debug("%s on clock %s\n", __func__, c->name);

	val = clk_readl(c->reg);
	val &= ~(PLL_BASE_BYPASS | PLL_BASE_ENABLE);
	clk_writel(val, c->reg);

	if (c->flags & PLLM) {
		val = pmc_readl(PMC_PLLP_WB0_OVERRIDE);
		val &= ~PMC_PLLP_WB0_OVERRIDE_PLLM_ENABLE;
		pmc_writel(val, PMC_PLLP_WB0_OVERRIDE);
	}
}

static int tegra3_pllm_override_rate(
	struct clk *c, const struct clk_pll_freq_table *sel, u32 p_div)
{
	u32 val, old_base;

	old_base = val = pmc_readl(PMC_PLLM_WB0_OVERRIDE);

	/* Keep default CPCON and DCCON in override configuration */
	val &= ~(PMC_PLLM_WB0_OVERRIDE_DIVM_MASK |
		 PMC_PLLM_WB0_OVERRIDE_DIVN_MASK |
		 PMC_PLLM_WB0_OVERRIDE_DIVP_MASK);
	val |= (sel->m << PMC_PLLM_WB0_OVERRIDE_DIVM_SHIFT) |
		(sel->n << PMC_PLLM_WB0_OVERRIDE_DIVN_SHIFT) |
		(p_div << PMC_PLLM_WB0_OVERRIDE_DIVP_SHIFT);

	if (val != old_base)
		pmc_writel(val, PMC_PLLM_WB0_OVERRIDE);

	return 0;
}

static int tegra3_pll_clk_set_rate(struct clk *c, unsigned long rate)
{
	u32 val, p_div, old_base;
	unsigned long input_rate;
	const struct clk_pll_freq_table *sel;
	struct clk_pll_freq_table cfg;

	pr_debug("%s: %s %lu\n", __func__, c->name, rate);

	if (c->flags & PLL_FIXED) {
		int ret = 0;
		if (rate != c->u.pll.fixed_rate) {
			pr_err("%s: Can not change %s fixed rate %lu to %lu\n",
			       __func__, c->name, c->u.pll.fixed_rate, rate);
			ret = -EINVAL;
		}
		return ret;
	}

	if ((c->flags & PLLM) && (c->state == ON)) {
		if (rate != clk_get_rate_locked(c)) {
			pr_err("%s: Can not change memory %s rate in flight\n",
			       __func__, c->name);
			return -EINVAL;
		}
		return 0;
	}

	p_div = 0;
	input_rate = clk_get_rate(c->parent);

	/* Check if the target rate is tabulated */
	for (sel = c->u.pll.freq_table; sel->input_rate != 0; sel++) {
		if (sel->input_rate == input_rate && sel->output_rate == rate) {
			if (c->flags & PLLU) {
				BUG_ON(sel->p < 1 || sel->p > 2);
				if (sel->p == 1)
					p_div = PLLU_BASE_POST_DIV;
			} else {
				BUG_ON(sel->p < 1);
				for (val = sel->p; val > 1; val >>= 1, p_div++);
				p_div <<= PLL_BASE_DIVP_SHIFT;
			}
			break;
		}
	}

	/* Configure out-of-table rate */
	if (sel->input_rate == 0) {
		unsigned long cfreq;
		BUG_ON(c->flags & PLLU);
		sel = &cfg;

		switch (input_rate) {
		case 12000000:
		case 26000000:
			cfreq = (rate <= 1000000 * 1000) ? 1000000 : 2000000;
			break;
		case 13000000:
			cfreq = (rate <= 1000000 * 1000) ? 1000000 : 2600000;
			break;
		case 16800000:
		case 19200000:
			cfreq = (rate <= 1200000 * 1000) ? 1200000 : 2400000;
			break;
		default:
			if (c->parent->flags & DIV_U71_FIXED) {
				/* PLLP_OUT1 rate is not in PLLA table */
				pr_warn("%s: failed %s ref/out rates %lu/%lu\n",
					__func__, c->name, input_rate, rate);
				cfreq = input_rate/(input_rate/1000000);
				break;
			}
			pr_err("%s: Unexpected reference rate %lu\n",
			       __func__, input_rate);
			BUG();
		}

		/* Raise VCO to guarantee 0.5% accuracy */
		for (cfg.output_rate = rate; cfg.output_rate < 200 * cfreq;
		      cfg.output_rate <<= 1, p_div++);

		cfg.p = 0x1 << p_div;
		cfg.m = input_rate / cfreq;
		cfg.n = cfg.output_rate / cfreq;
		cfg.cpcon = OUT_OF_TABLE_CPCON;

		if ((cfg.m > (PLL_BASE_DIVM_MASK >> PLL_BASE_DIVM_SHIFT)) ||
		    (cfg.n > (PLL_BASE_DIVN_MASK >> PLL_BASE_DIVN_SHIFT)) ||
		    (p_div > (PLL_BASE_DIVP_MASK >> PLL_BASE_DIVP_SHIFT)) ||
		    (cfg.output_rate > c->u.pll.vco_max)) {
			pr_err("%s: Failed to set %s out-of-table rate %lu\n",
			       __func__, c->name, rate);
			return -EINVAL;
		}
		p_div <<= PLL_BASE_DIVP_SHIFT;
	}

	c->mul = sel->n;
	c->div = sel->m * sel->p;

	if (c->flags & PLLM) {
		val = pmc_readl(PMC_PLLP_WB0_OVERRIDE);
		if (val & PMC_PLLP_WB0_OVERRIDE_PLLM_OVERRIDE)
			return tegra3_pllm_override_rate(
				c, sel, p_div >> PLL_BASE_DIVP_SHIFT);
	}

	old_base = val = clk_readl(c->reg + PLL_BASE);
	val &= ~(PLL_BASE_DIVM_MASK | PLL_BASE_DIVN_MASK |
		 ((c->flags & PLLU) ? PLLU_BASE_POST_DIV : PLL_BASE_DIVP_MASK));
	val |= (sel->m << PLL_BASE_DIVM_SHIFT) |
		(sel->n << PLL_BASE_DIVN_SHIFT) | p_div;
	if (val == old_base)
		return 0;

	if (c->state == ON) {
		tegra3_pll_clk_disable(c);
		val &= ~(PLL_BASE_BYPASS | PLL_BASE_ENABLE);
	}
	clk_writel(val, c->reg + PLL_BASE);

	if (c->flags & PLL_HAS_CPCON) {
		val = clk_readl(c->reg + PLL_MISC(c));
		val &= ~PLL_MISC_CPCON_MASK;
		val |= sel->cpcon << PLL_MISC_CPCON_SHIFT;
		if (c->flags & (PLLU | PLLD)) {
			val &= ~PLL_MISC_LFCON_MASK;
			if (sel->n >= PLLDU_LFCON_SET_DIVN)
				val |= 0x1 << PLL_MISC_LFCON_SHIFT;
		} else if (c->flags & (PLLX | PLLM)) {
			val &= ~(0x1 << PLL_MISC_DCCON_SHIFT);
			if (rate >= (c->u.pll.vco_max >> 1))
				val |= 0x1 << PLL_MISC_DCCON_SHIFT;
		}
		clk_writel(val, c->reg + PLL_MISC(c));
	}

	if (c->state == ON)
		tegra3_pll_clk_enable(c);

	return 0;
}

static struct clk_ops tegra_pll_ops = {
	.init			= tegra3_pll_clk_init,
	.enable			= tegra3_pll_clk_enable,
	.disable		= tegra3_pll_clk_disable,
	.set_rate		= tegra3_pll_clk_set_rate,
};

static void tegra3_pllp_clk_init(struct clk *c)
{
	tegra3_pll_clk_init(c);
	tegra3_pllp_init_dependencies(c->u.pll.fixed_rate);
}

#if defined(CONFIG_PM_SLEEP)
static void tegra3_pllp_clk_resume(struct clk *c)
{
	unsigned long rate = c->u.pll.fixed_rate;
	tegra3_pll_clk_init(c);
	BUG_ON(rate != c->u.pll.fixed_rate);
}
#endif

static struct clk_ops tegra_pllp_ops = {
	.init			= tegra3_pllp_clk_init,
	.enable			= tegra3_pll_clk_enable,
	.disable		= tegra3_pll_clk_disable,
	.set_rate		= tegra3_pll_clk_set_rate,
};

static int
tegra3_plld_clk_cfg_ex(struct clk *c, enum tegra_clk_ex_param p, u32 setting)
{
	u32 val, mask, reg;

	switch (p) {
	case TEGRA_CLK_PLLD_CSI_OUT_ENB:
		mask = PLLD_BASE_CSI_CLKENABLE;
		reg = c->reg + PLL_BASE;
		break;
	case TEGRA_CLK_PLLD_DSI_OUT_ENB:
		mask = PLLD_MISC_DSI_CLKENABLE;
		reg = c->reg + PLL_MISC(c);
		break;
	case TEGRA_CLK_PLLD_MIPI_MUX_SEL:
		if (!(c->flags & PLL_ALT_MISC_REG)) {
			mask = PLLD_BASE_DSIB_MUX_MASK;
			reg = c->reg + PLL_BASE;
			break;
		}
	/* fall through - error since PLLD2 does not have MUX_SEL control */
	default:
		return -EINVAL;
	}

	val = clk_readl(reg);
	if (setting)
		val |= mask;
	else
		val &= ~mask;
	clk_writel(val, reg);
	return 0;
}

static struct clk_ops tegra_plld_ops = {
	.init			= tegra3_pll_clk_init,
	.enable			= tegra3_pll_clk_enable,
	.disable		= tegra3_pll_clk_disable,
	.set_rate		= tegra3_pll_clk_set_rate,
	.clk_cfg_ex		= tegra3_plld_clk_cfg_ex,
};

static void tegra3_plle_clk_init(struct clk *c)
{
	u32 val;

	val = clk_readl(PLLE_AUX);
	c->parent = (val & PLLE_AUX_PLLP_SEL) ?
		tegra_get_clock_by_name("pll_p") :
		tegra_get_clock_by_name("pll_ref");

	val = clk_readl(c->reg + PLL_BASE);
	c->state = (val & PLLE_BASE_ENABLE) ? ON : OFF;
	c->mul = (val & PLLE_BASE_DIVN_MASK) >> PLLE_BASE_DIVN_SHIFT;
	c->div = (val & PLLE_BASE_DIVM_MASK) >> PLLE_BASE_DIVM_SHIFT;
	c->div *= (val & PLLE_BASE_DIVP_MASK) >> PLLE_BASE_DIVP_SHIFT;
}

static void tegra3_plle_clk_disable(struct clk *c)
{
	u32 val;
	pr_debug("%s on clock %s\n", __func__, c->name);

	val = clk_readl(c->reg + PLL_BASE);
	val &= ~(PLLE_BASE_CML_ENABLE | PLLE_BASE_ENABLE);
	clk_writel(val, c->reg + PLL_BASE);
}

static void tegra3_plle_training(struct clk *c)
{
	u32 val;

	/* PLLE is already disabled, and setup cleared;
	 * create falling edge on PLLE IDDQ input */
	val = pmc_readl(PMC_SATA_PWRGT);
	val |= PMC_SATA_PWRGT_PLLE_IDDQ_VALUE;
	pmc_writel(val, PMC_SATA_PWRGT);

	val = pmc_readl(PMC_SATA_PWRGT);
	val |= PMC_SATA_PWRGT_PLLE_IDDQ_SWCTL;
	pmc_writel(val, PMC_SATA_PWRGT);

	val = pmc_readl(PMC_SATA_PWRGT);
	val &= ~PMC_SATA_PWRGT_PLLE_IDDQ_VALUE;
	pmc_writel(val, PMC_SATA_PWRGT);

	do {
		val = clk_readl(c->reg + PLL_MISC(c));
	} while (!(val & PLLE_MISC_READY));
}

static int tegra3_plle_configure(struct clk *c, bool force_training)
{
	u32 val;
	const struct clk_pll_freq_table *sel;
	unsigned long rate = c->u.pll.fixed_rate;
	unsigned long input_rate = clk_get_rate(c->parent);

	for (sel = c->u.pll.freq_table; sel->input_rate != 0; sel++) {
		if (sel->input_rate == input_rate && sel->output_rate == rate)
			break;
	}

	if (sel->input_rate == 0)
		return -ENOSYS;

	/* disable PLLE, clear setup fiels */
	tegra3_plle_clk_disable(c);

	val = clk_readl(c->reg + PLL_MISC(c));
	val &= ~(PLLE_MISC_LOCK_ENABLE | PLLE_MISC_SETUP_MASK);
	clk_writel(val, c->reg + PLL_MISC(c));

	/* training */
	val = clk_readl(c->reg + PLL_MISC(c));
	if (force_training || (!(val & PLLE_MISC_READY)))
		tegra3_plle_training(c);

	/* configure dividers, setup, disable SS */
	val = clk_readl(c->reg + PLL_BASE);
	val &= ~PLLE_BASE_DIV_MASK;
	val |= PLLE_BASE_DIV(sel->m, sel->n, sel->p, sel->cpcon);
	clk_writel(val, c->reg + PLL_BASE);
	c->mul = sel->n;
	c->div = sel->m * sel->p;

	val = clk_readl(c->reg + PLL_MISC(c));
	val |= PLLE_MISC_SETUP_VALUE;
	val |= PLLE_MISC_LOCK_ENABLE;
	clk_writel(val, c->reg + PLL_MISC(c));

	val = clk_readl(PLLE_SS_CTRL);
	val |= PLLE_SS_DISABLE;
	clk_writel(val, PLLE_SS_CTRL);

	/* enable and lock PLLE*/
	val = clk_readl(c->reg + PLL_BASE);
	val |= (PLLE_BASE_CML_ENABLE | PLLE_BASE_ENABLE);
	clk_writel(val, c->reg + PLL_BASE);

	tegra3_pll_clk_wait_for_lock(c, c->reg + PLL_MISC(c), PLLE_MISC_LOCK);

#if USE_PLLE_SS
	/* configure spread spectrum coefficients */
	/* FIXME: coefficients for 216MHZ input? */
#ifdef CONFIG_TEGRA_SILICON_PLATFORM
	if (input_rate == 12000000)
#endif
	{
		val = clk_readl(PLLE_SS_CTRL);
		val &= ~(PLLE_SS_COEFFICIENTS_MASK | PLLE_SS_DISABLE);
		val |= PLLE_SS_COEFFICIENTS_12MHZ;
		clk_writel(val, PLLE_SS_CTRL);
	}
#endif
	return 0;
}

static int tegra3_plle_clk_enable(struct clk *c)
{
	pr_debug("%s on clock %s\n", __func__, c->name);
	return tegra3_plle_configure(c, !c->set);
}

static struct clk_ops tegra_plle_ops = {
	.init			= tegra3_plle_clk_init,
	.enable			= tegra3_plle_clk_enable,
	.disable		= tegra3_plle_clk_disable,
};

/* Clock divider ops (non-atomic shared register access) */
static DEFINE_SPINLOCK(pll_div_lock);

static int tegra3_pll_div_clk_set_rate(struct clk *c, unsigned long rate);
static void tegra3_pll_div_clk_init(struct clk *c)
{
	if (c->flags & DIV_U71) {
		u32 divu71;
		u32 val = clk_readl(c->reg);
		val >>= c->reg_shift;
		c->state = (val & PLL_OUT_CLKEN) ? ON : OFF;
		if (!(val & PLL_OUT_RESET_DISABLE))
			c->state = OFF;

		if (c->u.pll_div.default_rate) {
			int ret = tegra3_pll_div_clk_set_rate(
					c, c->u.pll_div.default_rate);
			if (!ret)
				return;
		}
		divu71 = (val & PLL_OUT_RATIO_MASK) >> PLL_OUT_RATIO_SHIFT;
		c->div = (divu71 + 2);
		c->mul = 2;
	} else if (c->flags & DIV_2) {
		c->state = ON;
		if (c->flags & (PLLD | PLLX)) {
			c->div = 2;
			c->mul = 1;
		}
		else
			BUG();
	} else {
		c->state = ON;
		c->div = 1;
		c->mul = 1;
	}
}

static int tegra3_pll_div_clk_enable(struct clk *c)
{
	u32 val;
	u32 new_val;
	unsigned long flags;

	pr_debug("%s: %s\n", __func__, c->name);
	if (c->flags & DIV_U71) {
		spin_lock_irqsave(&pll_div_lock, flags);
		val = clk_readl(c->reg);
		new_val = val >> c->reg_shift;
		new_val &= 0xFFFF;

		new_val |= PLL_OUT_CLKEN | PLL_OUT_RESET_DISABLE;

		val &= ~(0xFFFF << c->reg_shift);
		val |= new_val << c->reg_shift;
		clk_writel_delay(val, c->reg);
		spin_unlock_irqrestore(&pll_div_lock, flags);
		return 0;
	} else if (c->flags & DIV_2) {
		return 0;
	}
	return -EINVAL;
}

static void tegra3_pll_div_clk_disable(struct clk *c)
{
	u32 val;
	u32 new_val;
	unsigned long flags;

	pr_debug("%s: %s\n", __func__, c->name);
	if (c->flags & DIV_U71) {
		spin_lock_irqsave(&pll_div_lock, flags);
		val = clk_readl(c->reg);
		new_val = val >> c->reg_shift;
		new_val &= 0xFFFF;

		new_val &= ~(PLL_OUT_CLKEN | PLL_OUT_RESET_DISABLE);

		val &= ~(0xFFFF << c->reg_shift);
		val |= new_val << c->reg_shift;
		clk_writel_delay(val, c->reg);
		spin_unlock_irqrestore(&pll_div_lock, flags);
	}
}

static int tegra3_pll_div_clk_set_rate(struct clk *c, unsigned long rate)
{
	u32 val;
	u32 new_val;
	int divider_u71;
	unsigned long parent_rate = clk_get_rate(c->parent);
	unsigned long flags;

	pr_debug("%s: %s %lu\n", __func__, c->name, rate);
	if (c->flags & DIV_U71) {
		divider_u71 = clk_div71_get_divider(
			parent_rate, rate, c->flags, ROUND_DIVIDER_UP);
		if (divider_u71 >= 0) {
			spin_lock_irqsave(&pll_div_lock, flags);
			val = clk_readl(c->reg);
			new_val = val >> c->reg_shift;
			new_val &= 0xFFFF;
			if (c->flags & DIV_U71_FIXED)
				new_val |= PLL_OUT_OVERRIDE;
			new_val &= ~PLL_OUT_RATIO_MASK;
			new_val |= divider_u71 << PLL_OUT_RATIO_SHIFT;

			val &= ~(0xFFFF << c->reg_shift);
			val |= new_val << c->reg_shift;
			clk_writel_delay(val, c->reg);
			c->div = divider_u71 + 2;
			c->mul = 2;
			spin_unlock_irqrestore(&pll_div_lock, flags);
			return 0;
		}
	} else if (c->flags & DIV_2)
		return clk_set_rate(c->parent, rate * 2);

	return -EINVAL;
}

static long tegra3_pll_div_clk_round_rate(struct clk *c, unsigned long rate)
{
	int divider;
	unsigned long parent_rate = clk_get_rate(c->parent);
	pr_debug("%s: %s %lu\n", __func__, c->name, rate);

	if (c->flags & DIV_U71) {
		divider = clk_div71_get_divider(
			parent_rate, rate, c->flags, ROUND_DIVIDER_UP);
		if (divider < 0)
			return divider;
		return DIV_ROUND_UP(parent_rate * 2, divider + 2);
	} else if (c->flags & DIV_2)
		/* no rounding - fixed DIV_2 dividers pass rate to parent PLL */
		return rate;

	return -EINVAL;
}

static struct clk_ops tegra_pll_div_ops = {
	.init			= tegra3_pll_div_clk_init,
	.enable			= tegra3_pll_div_clk_enable,
	.disable		= tegra3_pll_div_clk_disable,
	.set_rate		= tegra3_pll_div_clk_set_rate,
	.round_rate		= tegra3_pll_div_clk_round_rate,
};

/* Periph clk ops */
static inline u32 periph_clk_source_mask(struct clk *c)
{
	if (c->flags & MUX8)
		 return 7 << 29;
	else if (c->flags & MUX_PWM)
		return 3 << 28;
	else if (c->flags & MUX_CLK_OUT)
		return 3 << (c->u.periph.clk_num + 4);
	else if (c->flags & PLLD)
		return PLLD_BASE_DSIB_MUX_MASK;
	else
		return 3 << 30;
}

static inline u32 periph_clk_source_shift(struct clk *c)
{
	if (c->flags & MUX8)
		 return 29;
	else if (c->flags & MUX_PWM)
		return 28;
	else if (c->flags & MUX_CLK_OUT)
		return c->u.periph.clk_num + 4;
	else if (c->flags & PLLD)
		return PLLD_BASE_DSIB_MUX_SHIFT;
	else
		return 30;
}

static void tegra3_periph_clk_init(struct clk *c)
{
	u32 val = clk_readl(c->reg);
	const struct clk_mux_sel *mux = 0;
	const struct clk_mux_sel *sel;
	if (c->flags & MUX) {
		for (sel = c->inputs; sel->input != NULL; sel++) {
			if (((val & periph_clk_source_mask(c)) >>
			    periph_clk_source_shift(c)) == sel->value)
				mux = sel;
		}
		BUG_ON(!mux);

		c->parent = mux->input;
	} else {
		c->parent = c->inputs[0].input;
	}

	if (c->flags & DIV_U71) {
		u32 divu71 = val & PERIPH_CLK_SOURCE_DIVU71_MASK;
		if (c->flags & DIV_U71_IDLE) {
			val &= ~(PERIPH_CLK_SOURCE_DIVU71_MASK <<
				PERIPH_CLK_SOURCE_DIVIDLE_SHIFT);
			val |= (PERIPH_CLK_SOURCE_DIVIDLE_VAL <<
				PERIPH_CLK_SOURCE_DIVIDLE_SHIFT);
			clk_writel(val, c->reg);
		}
		c->div = divu71 + 2;
		c->mul = 2;
	} else if (c->flags & DIV_U151) {
		u32 divu151 = val & PERIPH_CLK_SOURCE_DIVU16_MASK;
		if ((c->flags & DIV_U151_UART) &&
		    (!(val & PERIPH_CLK_UART_DIV_ENB))) {
			divu151 = 0;
		}
		c->div = divu151 + 2;
		c->mul = 2;
	} else if (c->flags & DIV_U16) {
		u32 divu16 = val & PERIPH_CLK_SOURCE_DIVU16_MASK;
		c->div = divu16 + 1;
		c->mul = 1;
	} else {
		c->div = 1;
		c->mul = 1;
	}

	c->state = ON;

	if (c->flags & PERIPH_NO_ENB)
		return;

	if (!(clk_readl(PERIPH_CLK_TO_ENB_REG(c)) & PERIPH_CLK_TO_BIT(c)))
		c->state = OFF;
	if (!(c->flags & PERIPH_NO_RESET))
		if (clk_readl(PERIPH_CLK_TO_RST_REG(c)) & PERIPH_CLK_TO_BIT(c))
			c->state = OFF;
}

static int tegra3_periph_clk_enable(struct clk *c)
{
	unsigned long flags;
	pr_debug("%s on clock %s\n", __func__, c->name);

	if (c->flags & PERIPH_NO_ENB)
		return 0;

	spin_lock_irqsave(&periph_refcount_lock, flags);

	tegra_periph_clk_enable_refcount[c->u.periph.clk_num]++;
	if (tegra_periph_clk_enable_refcount[c->u.periph.clk_num] > 1) {
		spin_unlock_irqrestore(&periph_refcount_lock, flags);
		return 0;
	}

	clk_writel_delay(PERIPH_CLK_TO_BIT(c), PERIPH_CLK_TO_ENB_SET_REG(c));
	if (!(c->flags & PERIPH_NO_RESET) && !(c->flags & PERIPH_MANUAL_RESET)) {
		if (clk_readl(PERIPH_CLK_TO_RST_REG(c)) & PERIPH_CLK_TO_BIT(c)) {
			udelay(RESET_PROPAGATION_DELAY);
			clk_writel(PERIPH_CLK_TO_BIT(c), PERIPH_CLK_TO_RST_CLR_REG(c));
		}
	}
	spin_unlock_irqrestore(&periph_refcount_lock, flags);
	return 0;
}

static void tegra3_periph_clk_disable(struct clk *c)
{
	unsigned long val, flags;
	pr_debug("%s on clock %s\n", __func__, c->name);

	if (c->flags & PERIPH_NO_ENB)
		return;

	spin_lock_irqsave(&periph_refcount_lock, flags);

	if (c->refcnt)
		tegra_periph_clk_enable_refcount[c->u.periph.clk_num]--;

	if (tegra_periph_clk_enable_refcount[c->u.periph.clk_num] == 0) {
		/* If peripheral is in the APB bus then read the APB bus to
		 * flush the write operation in apb bus. This will avoid the
		 * peripheral access after disabling clock*/
		if (c->flags & PERIPH_ON_APB)
			val = chipid_readl();

		clk_writel_delay(
			PERIPH_CLK_TO_BIT(c), PERIPH_CLK_TO_ENB_CLR_REG(c));
	}
	spin_unlock_irqrestore(&periph_refcount_lock, flags);
}

static void tegra3_periph_clk_reset(struct clk *c, bool assert)
{
	unsigned long val;
	pr_debug("%s %s on clock %s\n", __func__,
		 assert ? "assert" : "deassert", c->name);

	if (c->flags & PERIPH_NO_ENB)
		return;

	if (!(c->flags & PERIPH_NO_RESET)) {
		if (assert) {
			/* If peripheral is in the APB bus then read the APB
			 * bus to flush the write operation in apb bus. This
			 * will avoid the peripheral access after disabling
			 * clock */
			if (c->flags & PERIPH_ON_APB)
				val = chipid_readl();

			clk_writel(PERIPH_CLK_TO_BIT(c),
				   PERIPH_CLK_TO_RST_SET_REG(c));
		} else
			clk_writel(PERIPH_CLK_TO_BIT(c),
				   PERIPH_CLK_TO_RST_CLR_REG(c));
	}
}

static int tegra3_periph_clk_set_parent(struct clk *c, struct clk *p)
{
	u32 val;
	const struct clk_mux_sel *sel;
	pr_debug("%s: %s %s\n", __func__, c->name, p->name);

	if (!(c->flags & MUX))
		return (p == c->parent) ? 0 : (-EINVAL);

	for (sel = c->inputs; sel->input != NULL; sel++) {
		if (sel->input == p) {
			val = clk_readl(c->reg);
			val &= ~periph_clk_source_mask(c);
			val |= (sel->value << periph_clk_source_shift(c));

			if (c->refcnt)
				tegra_clk_prepare_enable(p);

			clk_writel_delay(val, c->reg);

			if (c->refcnt && c->parent)
				tegra_clk_disable_unprepare(c->parent);

			clk_reparent(c, p);
			return 0;
		}
	}

	return -EINVAL;
}

static int tegra3_periph_clk_set_rate(struct clk *c, unsigned long rate)
{
	u32 val;
	int divider;
	unsigned long parent_rate = clk_get_rate(c->parent);

	if (c->flags & DIV_U71) {
		divider = clk_div71_get_divider(
			parent_rate, rate, c->flags, ROUND_DIVIDER_UP);
		if (divider >= 0) {
			val = clk_readl(c->reg);
			val &= ~PERIPH_CLK_SOURCE_DIVU71_MASK;
			val |= divider;
			clk_writel_delay(val, c->reg);
			c->div = divider + 2;
			c->mul = 2;
			return 0;
		}
	} else if (c->flags & DIV_U151) {
		divider = clk_div151_get_divider(
			parent_rate, rate, c->flags, ROUND_DIVIDER_UP);
		if (divider >= 0) {
			val = clk_readl(c->reg);
			val &= ~PERIPH_CLK_SOURCE_DIVU16_MASK;
			val |= divider;
			if (c->flags & DIV_U151_UART) {
				if (divider)
					val |= PERIPH_CLK_UART_DIV_ENB;
				else
					val &= ~PERIPH_CLK_UART_DIV_ENB;
			}
			clk_writel_delay(val, c->reg);
			c->div = divider + 2;
			c->mul = 2;
			return 0;
		}
	} else if (c->flags & DIV_U16) {
		divider = clk_div16_get_divider(parent_rate, rate);
		if (divider >= 0) {
			val = clk_readl(c->reg);
			val &= ~PERIPH_CLK_SOURCE_DIVU16_MASK;
			val |= divider;
			clk_writel_delay(val, c->reg);
			c->div = divider + 1;
			c->mul = 1;
			return 0;
		}
	} else if (parent_rate <= rate) {
		c->div = 1;
		c->mul = 1;
		return 0;
	}
	return -EINVAL;
}

static long tegra3_periph_clk_round_rate(struct clk *c,
	unsigned long rate)
{
	int divider;
	unsigned long parent_rate = clk_get_rate(c->parent);
	pr_debug("%s: %s %lu\n", __func__, c->name, rate);

	if (c->flags & DIV_U71) {
		divider = clk_div71_get_divider(
			parent_rate, rate, c->flags, ROUND_DIVIDER_UP);
		if (divider < 0)
			return divider;

		return DIV_ROUND_UP(parent_rate * 2, divider + 2);
	} else if (c->flags & DIV_U151) {
		divider = clk_div151_get_divider(
			parent_rate, rate, c->flags, ROUND_DIVIDER_UP);
		if (divider < 0)
			return divider;

		return DIV_ROUND_UP(parent_rate * 2, divider + 2);
	} else if (c->flags & DIV_U16) {
		divider = clk_div16_get_divider(parent_rate, rate);
		if (divider < 0)
			return divider;
		return DIV_ROUND_UP(parent_rate, divider + 1);
	}
	return -EINVAL;
}

static struct clk_ops tegra_periph_clk_ops = {
	.init			= &tegra3_periph_clk_init,
	.enable			= &tegra3_periph_clk_enable,
	.disable		= &tegra3_periph_clk_disable,
	.set_parent		= &tegra3_periph_clk_set_parent,
	.set_rate		= &tegra3_periph_clk_set_rate,
	.round_rate		= &tegra3_periph_clk_round_rate,
	.reset			= &tegra3_periph_clk_reset,
};


/* Periph extended clock configuration ops */
static int
tegra3_vi_clk_cfg_ex(struct clk *c, enum tegra_clk_ex_param p, u32 setting)
{
	if (p == TEGRA_CLK_VI_INP_SEL) {
		u32 val = clk_readl(c->reg);
		val &= ~PERIPH_CLK_VI_SEL_EX_MASK;
		val |= (setting << PERIPH_CLK_VI_SEL_EX_SHIFT) &
			PERIPH_CLK_VI_SEL_EX_MASK;
		clk_writel(val, c->reg);
		return 0;
	}
	return -EINVAL;
}

static struct clk_ops tegra_vi_clk_ops = {
	.init			= &tegra3_periph_clk_init,
	.enable			= &tegra3_periph_clk_enable,
	.disable		= &tegra3_periph_clk_disable,
	.set_parent		= &tegra3_periph_clk_set_parent,
	.set_rate		= &tegra3_periph_clk_set_rate,
	.round_rate		= &tegra3_periph_clk_round_rate,
	.clk_cfg_ex		= &tegra3_vi_clk_cfg_ex,
	.reset			= &tegra3_periph_clk_reset,
};

static int
tegra3_nand_clk_cfg_ex(struct clk *c, enum tegra_clk_ex_param p, u32 setting)
{
	if (p == TEGRA_CLK_NAND_PAD_DIV2_ENB) {
		u32 val = clk_readl(c->reg);
		if (setting)
			val |= PERIPH_CLK_NAND_DIV_EX_ENB;
		else
			val &= ~PERIPH_CLK_NAND_DIV_EX_ENB;
		clk_writel(val, c->reg);
		return 0;
	}
	return -EINVAL;
}

static struct clk_ops tegra_nand_clk_ops = {
	.init			= &tegra3_periph_clk_init,
	.enable			= &tegra3_periph_clk_enable,
	.disable		= &tegra3_periph_clk_disable,
	.set_parent		= &tegra3_periph_clk_set_parent,
	.set_rate		= &tegra3_periph_clk_set_rate,
	.round_rate		= &tegra3_periph_clk_round_rate,
	.clk_cfg_ex		= &tegra3_nand_clk_cfg_ex,
	.reset			= &tegra3_periph_clk_reset,
};


static int
tegra3_dtv_clk_cfg_ex(struct clk *c, enum tegra_clk_ex_param p, u32 setting)
{
	if (p == TEGRA_CLK_DTV_INVERT) {
		u32 val = clk_readl(c->reg);
		if (setting)
			val |= PERIPH_CLK_DTV_POLARITY_INV;
		else
			val &= ~PERIPH_CLK_DTV_POLARITY_INV;
		clk_writel(val, c->reg);
		return 0;
	}
	return -EINVAL;
}

static struct clk_ops tegra_dtv_clk_ops = {
	.init			= &tegra3_periph_clk_init,
	.enable			= &tegra3_periph_clk_enable,
	.disable		= &tegra3_periph_clk_disable,
	.set_parent		= &tegra3_periph_clk_set_parent,
	.set_rate		= &tegra3_periph_clk_set_rate,
	.round_rate		= &tegra3_periph_clk_round_rate,
	.clk_cfg_ex		= &tegra3_dtv_clk_cfg_ex,
	.reset			= &tegra3_periph_clk_reset,
};

static int tegra3_dsib_clk_set_parent(struct clk *c, struct clk *p)
{
	const struct clk_mux_sel *sel;
	struct clk *d = tegra_get_clock_by_name("pll_d");

	pr_debug("%s: %s %s\n", __func__, c->name, p->name);

	for (sel = c->inputs; sel->input != NULL; sel++) {
		if (sel->input == p) {
			if (c->refcnt)
				tegra_clk_prepare_enable(p);

			/* The DSIB parent selection bit is in PLLD base
			   register - can not do direct r-m-w, must be
			   protected by PLLD lock */
			tegra_clk_cfg_ex(
				d, TEGRA_CLK_PLLD_MIPI_MUX_SEL, sel->value);

			if (c->refcnt && c->parent)
				tegra_clk_disable_unprepare(c->parent);

			clk_reparent(c, p);
			return 0;
		}
	}

	return -EINVAL;
}

static struct clk_ops tegra_dsib_clk_ops = {
	.init			= &tegra3_periph_clk_init,
	.enable			= &tegra3_periph_clk_enable,
	.disable		= &tegra3_periph_clk_disable,
	.set_parent		= &tegra3_dsib_clk_set_parent,
	.set_rate		= &tegra3_periph_clk_set_rate,
	.round_rate		= &tegra3_periph_clk_round_rate,
	.reset			= &tegra3_periph_clk_reset,
};

/* pciex clock support only reset function */
static struct clk_ops tegra_pciex_clk_ops = {
	.reset    = tegra3_periph_clk_reset,
};

/* Output clock ops (non-atomic shared register access) */

static DEFINE_SPINLOCK(clk_out_lock);

static void tegra3_clk_out_init(struct clk *c)
{
	const struct clk_mux_sel *mux = 0;
	const struct clk_mux_sel *sel;
	u32 val = pmc_readl(c->reg);

	c->state = (val & (0x1 << c->u.periph.clk_num)) ? ON : OFF;
	c->mul = 1;
	c->div = 1;

	for (sel = c->inputs; sel->input != NULL; sel++) {
		if (((val & periph_clk_source_mask(c)) >>
		     periph_clk_source_shift(c)) == sel->value)
			mux = sel;
	}
	BUG_ON(!mux);
	c->parent = mux->input;
}

static int tegra3_clk_out_enable(struct clk *c)
{
	u32 val;
	unsigned long flags;

	pr_debug("%s on clock %s\n", __func__, c->name);

	spin_lock_irqsave(&clk_out_lock, flags);
	val = pmc_readl(c->reg);
	val |= (0x1 << c->u.periph.clk_num);
	pmc_writel(val, c->reg);
	spin_unlock_irqrestore(&clk_out_lock, flags);

	return 0;
}

static void tegra3_clk_out_disable(struct clk *c)
{
	u32 val;
	unsigned long flags;

	pr_debug("%s on clock %s\n", __func__, c->name);

	spin_lock_irqsave(&clk_out_lock, flags);
	val = pmc_readl(c->reg);
	val &= ~(0x1 << c->u.periph.clk_num);
	pmc_writel(val, c->reg);
	spin_unlock_irqrestore(&clk_out_lock, flags);
}

static int tegra3_clk_out_set_parent(struct clk *c, struct clk *p)
{
	u32 val;
	unsigned long flags;
	const struct clk_mux_sel *sel;

	pr_debug("%s: %s %s\n", __func__, c->name, p->name);

	for (sel = c->inputs; sel->input != NULL; sel++) {
		if (sel->input == p) {
			if (c->refcnt)
				tegra_clk_prepare_enable(p);

			spin_lock_irqsave(&clk_out_lock, flags);
			val = pmc_readl(c->reg);
			val &= ~periph_clk_source_mask(c);
			val |= (sel->value << periph_clk_source_shift(c));
			pmc_writel(val, c->reg);
			spin_unlock_irqrestore(&clk_out_lock, flags);

			if (c->refcnt && c->parent)
				tegra_clk_disable_unprepare(c->parent);

			clk_reparent(c, p);
			return 0;
		}
	}
	return -EINVAL;
}

static struct clk_ops tegra_clk_out_ops = {
	.init			= &tegra3_clk_out_init,
	.enable			= &tegra3_clk_out_enable,
	.disable		= &tegra3_clk_out_disable,
	.set_parent		= &tegra3_clk_out_set_parent,
};


/* External memory controller clock ops */
static void tegra3_emc_clk_init(struct clk *c)
{
	tegra3_periph_clk_init(c);
	tegra_emc_dram_type_init(c);

	/* On A01 limit EMC maximum rate to boot frequency;
	   starting with A02 full PLLM range should be supported */
	if (tegra_revision == TEGRA_REVISION_A01)
		c->max_rate = clk_get_rate_locked(c);
	else
		c->max_rate = clk_get_rate(c->parent);
}

static long tegra3_emc_clk_round_rate(struct clk *c, unsigned long rate)
{
	long new_rate = max(rate, c->min_rate);

	new_rate = tegra_emc_round_rate(new_rate);
	if (new_rate < 0)
		new_rate = c->max_rate;

	return new_rate;
}

static int tegra3_emc_clk_set_rate(struct clk *c, unsigned long rate)
{
	int ret;
	u32 div_value;
	struct clk *p;

	/* The tegra3 memory controller has an interlock with the clock
	 * block that allows memory shadowed registers to be updated,
	 * and then transfer them to the main registers at the same
	 * time as the clock update without glitches. During clock change
	 * operation both clock parent and divider may change simultaneously
	 * to achieve requested rate. */
	p = tegra_emc_predict_parent(rate, &div_value);
	div_value += 2;		/* emc has fractional DIV_U71 divider */

	/* No matching rate in emc dfs table */
	if (IS_ERR(p))
		return PTR_ERR(p);

	/* Table rate found, but need to relock source pll */
	if (!p)
		return tegra3_emc_relock_set_rate(c, clk_get_rate_locked(c),
						  rate, rate * (div_value / 2));

	if (p == c->parent) {
		if (div_value == c->div)
			return 0;
	} else if (c->refcnt)
		tegra_clk_prepare_enable(p);

	ret = tegra_emc_set_rate(rate);
	if (ret < 0)
		return ret;

	if (p != c->parent) {
		if(c->refcnt && c->parent)
			tegra_clk_disable_unprepare(c->parent);
		clk_reparent(c, p);
	}
	c->div = div_value;
	c->mul = 2;
	return 0;
}

static struct clk_ops tegra_emc_clk_ops = {
	.init			= &tegra3_emc_clk_init,
	.enable			= &tegra3_periph_clk_enable,
	.disable		= &tegra3_periph_clk_disable,
	.set_rate		= &tegra3_emc_clk_set_rate,
	.round_rate		= &tegra3_emc_clk_round_rate,
	.reset			= &tegra3_periph_clk_reset,
	.shared_bus_update	= &tegra3_clk_shared_bus_update,
};

/* Clock doubler ops (non-atomic shared register access) */
static DEFINE_SPINLOCK(doubler_lock);

static void tegra3_clk_double_init(struct clk *c)
{
	u32 val = clk_readl(c->reg);
	c->mul = val & (0x1 << c->reg_shift) ? 1 : 2;
	c->div = 1;
	c->state = ON;
	if (!(clk_readl(PERIPH_CLK_TO_ENB_REG(c)) & PERIPH_CLK_TO_BIT(c)))
		c->state = OFF;
};

static int tegra3_clk_double_set_rate(struct clk *c, unsigned long rate)
{
	u32 val;
	unsigned long parent_rate = clk_get_rate(c->parent);
	unsigned long flags;

	if (rate == parent_rate) {
		spin_lock_irqsave(&doubler_lock, flags);
		val = clk_readl(c->reg) | (0x1 << c->reg_shift);
		clk_writel(val, c->reg);
		c->mul = 1;
		c->div = 1;
		spin_unlock_irqrestore(&doubler_lock, flags);
		return 0;
	} else if (rate == 2 * parent_rate) {
		spin_lock_irqsave(&doubler_lock, flags);
		val = clk_readl(c->reg) & (~(0x1 << c->reg_shift));
		clk_writel(val, c->reg);
		c->mul = 2;
		c->div = 1;
		spin_unlock_irqrestore(&doubler_lock, flags);
		return 0;
	}
	return -EINVAL;
}

static struct clk_ops tegra_clk_double_ops = {
	.init			= &tegra3_clk_double_init,
	.enable			= &tegra3_periph_clk_enable,
	.disable		= &tegra3_periph_clk_disable,
	.set_rate		= &tegra3_clk_double_set_rate,
};

/* Audio sync clock ops */
static int tegra3_sync_source_set_rate(struct clk *c, unsigned long rate)
{
	c->rate = rate;
	return 0;
}

static struct clk_ops tegra_sync_source_ops = {
	.set_rate		= &tegra3_sync_source_set_rate,
};

static void tegra3_audio_sync_clk_init(struct clk *c)
{
	int source;
	const struct clk_mux_sel *sel;
	u32 val = clk_readl(c->reg);
	c->state = (val & AUDIO_SYNC_DISABLE_BIT) ? OFF : ON;
	source = val & AUDIO_SYNC_SOURCE_MASK;
	for (sel = c->inputs; sel->input != NULL; sel++)
		if (sel->value == source)
			break;
	BUG_ON(sel->input == NULL);
	c->parent = sel->input;
}

static int tegra3_audio_sync_clk_enable(struct clk *c)
{
	u32 val = clk_readl(c->reg);
	clk_writel((val & (~AUDIO_SYNC_DISABLE_BIT)), c->reg);
	return 0;
}

static void tegra3_audio_sync_clk_disable(struct clk *c)
{
	u32 val = clk_readl(c->reg);
	clk_writel((val | AUDIO_SYNC_DISABLE_BIT), c->reg);
}

static int tegra3_audio_sync_clk_set_parent(struct clk *c, struct clk *p)
{
	u32 val;
	const struct clk_mux_sel *sel;
	for (sel = c->inputs; sel->input != NULL; sel++) {
		if (sel->input == p) {
			val = clk_readl(c->reg);
			val &= ~AUDIO_SYNC_SOURCE_MASK;
			val |= sel->value;

			if (c->refcnt)
				tegra_clk_prepare_enable(p);

			clk_writel(val, c->reg);

			if (c->refcnt && c->parent)
				tegra_clk_disable_unprepare(c->parent);

			clk_reparent(c, p);
			return 0;
		}
	}

	return -EINVAL;
}

static struct clk_ops tegra_audio_sync_clk_ops = {
	.init       = tegra3_audio_sync_clk_init,
	.enable     = tegra3_audio_sync_clk_enable,
	.disable    = tegra3_audio_sync_clk_disable,
	.set_parent = tegra3_audio_sync_clk_set_parent,
};

/* cml0 (pcie), and cml1 (sata) clock ops (non-atomic shared register access) */
static DEFINE_SPINLOCK(cml_lock);

static void tegra3_cml_clk_init(struct clk *c)
{
	u32 val = clk_readl(c->reg);
	c->state = val & (0x1 << c->u.periph.clk_num) ? ON : OFF;
}

static int tegra3_cml_clk_enable(struct clk *c)
{
	u32 val;
	unsigned long flags;

	spin_lock_irqsave(&cml_lock, flags);
	val = clk_readl(c->reg);
	val |= (0x1 << c->u.periph.clk_num);
	clk_writel(val, c->reg);
	spin_unlock_irqrestore(&cml_lock, flags);
	return 0;
}

static void tegra3_cml_clk_disable(struct clk *c)
{
	u32 val;
	unsigned long flags;

	spin_lock_irqsave(&cml_lock, flags);
	val = clk_readl(c->reg);
	val &= ~(0x1 << c->u.periph.clk_num);
	clk_writel(val, c->reg);
	spin_unlock_irqrestore(&cml_lock, flags);
}

static struct clk_ops tegra_cml_clk_ops = {
	.init			= &tegra3_cml_clk_init,
	.enable			= &tegra3_cml_clk_enable,
	.disable		= &tegra3_cml_clk_disable,
};


/* cbus ops */
/*
 * Some clocks require dynamic re-locking of source PLL in order to
 * achieve frequency scaling granularity that matches characterized
 * core voltage steps. The cbus clock creates a shared bus that
 * provides a virtual root for such clocks to hide and synchronize
 * parent PLL re-locking as well as backup operations.
*/

static void tegra3_clk_cbus_init(struct clk *c)
{
	c->state = OFF;
	c->set = true;
	c->shared_bus_backup.bus_rate =
		clk_get_rate(c->shared_bus_backup.input) /
		c->shared_bus_backup.value;
}

static int tegra3_clk_cbus_enable(struct clk *c)
{
	return 0;
}

static long tegra3_clk_cbus_round_rate(struct clk *c, unsigned long rate)
{
	int i;

	if (!c->dvfs)
		return rate;

	/* update min now, since no dvfs table was available during init
	   (skip placeholder entries set to 1 kHz) */
	if (!c->min_rate) {
		for (i = 0; i < (c->dvfs->num_freqs - 1); i++) {
			if (c->dvfs->freqs[i] > 1 * c->dvfs->freqs_mult) {
				c->min_rate = c->dvfs->freqs[i];
				break;
			}
		}
		BUG_ON(!c->min_rate);
	}
	rate = max(rate, c->min_rate);

	for (i = 0; i < (c->dvfs->num_freqs - 1); i++) {
		unsigned long f = c->dvfs->freqs[i];
		if (f >= rate)
			break;
	}
	return c->dvfs->freqs[i];
}

static int cbus_switch_one(struct clk *c, struct clk *p, u32 div, bool abort)
{
	int ret = 0;

	/* set new divider if it is bigger than the current one */
	if (c->div < c->mul * div) {
		ret = clk_set_div(c, div);
		if (ret) {
			pr_err("%s: failed to set %s clock divider %u: %d\n",
			       __func__, c->name, div, ret);
			if (abort)
				return ret;
		}
	}

	ret = clk_set_parent(c, p);
	if (ret) {
		pr_err("%s: failed to set %s clock parent %s: %d\n",
		       __func__, c->name, p->name, ret);
		if (abort)
			return ret;
	}

	/* set new divider if it is smaller than the current one */
	if (c->div > c->mul * div) {
		ret = clk_set_div(c, div);
		if (ret)
			pr_err("%s: failed to set %s clock divider %u: %d\n",
			       __func__, c->name, div, ret);
	}

	return ret;
}

static int cbus_backup(struct clk *c)
{
	int ret;
	struct clk *user;

	list_for_each_entry(user, &c->shared_bus_list,
			u.shared_bus_user.node) {
		bool enabled = user->u.shared_bus_user.client &&
			(user->u.shared_bus_user.enabled ||
			user->u.shared_bus_user.client->refcnt);
		if (enabled) {
			ret = cbus_switch_one(user->u.shared_bus_user.client,
					      c->shared_bus_backup.input,
					      c->shared_bus_backup.value *
					      user->div, true);
			if (ret)
				return ret;
		}
	}
	return 0;
}

static void cbus_restore(struct clk *c)
{
	struct clk *user;

	list_for_each_entry(user, &c->shared_bus_list,
			u.shared_bus_user.node) {
		bool back = user->u.shared_bus_user.client && (c->parent !=
			user->u.shared_bus_user.client->parent);
		if (back)
			cbus_switch_one(user->u.shared_bus_user.client,
					c->parent, c->div * user->div, false);
	}
}

static int tegra3_clk_cbus_set_rate(struct clk *c, unsigned long rate)
{
	int ret;

	if (rate == 0)
		return 0;

	ret = tegra_clk_prepare_enable(c->parent);
	if (ret) {
		pr_err("%s: failed to enable %s clock: %d\n",
		       __func__, c->name, ret);
		return ret;
	}

	ret = cbus_backup(c);
	if (ret)
		goto out;

	ret = clk_set_rate(c->parent, rate * c->div);
	if (ret) {
		pr_err("%s: failed to set %s clock rate %lu: %d\n",
		       __func__, c->name, rate, ret);
		goto out;
	}

	cbus_restore(c);

out:
	tegra_clk_disable_unprepare(c->parent);
	return ret;
}

static struct clk_ops tegra_clk_cbus_ops = {
	.init = tegra3_clk_cbus_init,
	.enable = tegra3_clk_cbus_enable,
	.set_rate = tegra3_clk_cbus_set_rate,
	.round_rate = tegra3_clk_cbus_round_rate,
	.shared_bus_update = tegra3_clk_shared_bus_update,
};

/* shared bus ops */
/*
 * Some clocks may have multiple downstream users that need to request a
 * higher clock rate.  Shared bus clocks provide a unique shared_bus_user
 * clock to each user.  The frequency of the bus is set to the highest
 * enabled shared_bus_user clock, with a minimum value set by the
 * shared bus.
 */

static noinline int shared_bus_set_rate(struct clk *bus, unsigned long rate,
					unsigned long old_rate)
{
	int ret, mv, old_mv;
	unsigned long bridge_rate = emc_bridge->u.shared_bus_user.rate;

	/* If bridge is not needed (LPDDR2) just set bus rate */
	if (tegra_emc_get_dram_type() == DRAM_TYPE_LPDDR2)
		return clk_set_rate_locked(bus, rate);

	mv = tegra_dvfs_predict_millivolts(bus, rate);
	old_mv = tegra_dvfs_predict_millivolts(bus, old_rate);
	if (IS_ERR_VALUE(mv) || IS_ERR_VALUE(old_mv)) {
		pr_err("%s: Failed to predict %s voltage for %lu => %lu\n",
		       __func__, bus->name, old_rate, rate);
		return -EINVAL;
	}

	/* emc bus: set bridge rate as intermediate step when crossing
	 * bridge threshold in any direction
	 */
	if (bus->flags & PERIPH_EMC_ENB) {
		if (((mv > TEGRA_EMC_BRIDGE_MVOLTS_MIN) &&
		     (old_rate < bridge_rate)) ||
		    ((old_mv > TEGRA_EMC_BRIDGE_MVOLTS_MIN) &&
		     (rate < bridge_rate))) {
			ret = clk_set_rate_locked(bus, bridge_rate);
			if (ret) {
				pr_err("%s: Failed to set emc bridge rate %lu\n",
					__func__, bridge_rate);
				return ret;
			}
		}
		return clk_set_rate_locked(bus, rate);
	}

	/* sbus and cbus: enable/disable emc bridge user when crossing voltage
	 * threshold up/down respectively; hence, emc rate is kept above the
	 * bridge rate as long as any sbus or cbus user requires high voltage
	 */
	if ((mv > TEGRA_EMC_BRIDGE_MVOLTS_MIN) &&
	    (old_mv <= TEGRA_EMC_BRIDGE_MVOLTS_MIN)) {
		ret = tegra_clk_prepare_enable(emc_bridge);
		if (ret) {
			pr_err("%s: Failed to enable emc bridge\n", __func__);
			return ret;
		}
	}

	ret = clk_set_rate_locked(bus, rate);
	if (ret)
		return ret;

	if ((mv <= TEGRA_EMC_BRIDGE_MVOLTS_MIN) &&
	    (old_mv > TEGRA_EMC_BRIDGE_MVOLTS_MIN))
		tegra_clk_disable_unprepare(emc_bridge);

	return 0;
}

static int tegra3_clk_shared_bus_update(struct clk *bus)
{
	struct clk *c;
	unsigned long old_rate;
	unsigned long rate = bus->min_rate;
	unsigned long bw = 0;
	unsigned long ceiling = bus->max_rate;
	u8 emc_bw_efficiency = tegra_emc_bw_efficiency_boost;

	if (detach_shared_bus)
		return 0;

	list_for_each_entry(c, &bus->shared_bus_list,
			u.shared_bus_user.node) {
		/* Ignore requests from disabled floor and bw users, and from
		 * auto-users riding the bus. Always honor ceiling users, even
		 * if they are disabled - we do not want to keep enabled parent
		 * bus just because ceiling is set.
		 */
		if (c->u.shared_bus_user.enabled ||
		    (c->u.shared_bus_user.mode == SHARED_CEILING)) {
			if (!strcmp(c->name, "3d.emc"))
				emc_bw_efficiency = tegra_emc_bw_efficiency;

			switch (c->u.shared_bus_user.mode) {
			case SHARED_BW:
				if (bw < bus->max_rate)
					bw += c->u.shared_bus_user.rate;
				break;
			case SHARED_CEILING:
				ceiling = min(c->u.shared_bus_user.rate,
					       ceiling);
				break;
			case SHARED_AUTO:
			case SHARED_FLOOR:
			default:
				rate = max(c->u.shared_bus_user.rate, rate);
			}
		}
	}
	if (bw) {
		if (bus->flags & PERIPH_EMC_ENB) {
			bw = emc_bw_efficiency ?
				(bw / emc_bw_efficiency) : bus->max_rate;
			bw = (bw < bus->max_rate / 100) ?
				(bw * 100) : bus->max_rate;
		}
		bw = clk_round_rate_locked(bus, bw);
	}
	rate = min(max(rate, bw), ceiling);

	old_rate = clk_get_rate_locked(bus);
	if (rate == old_rate)
		return 0;

	return shared_bus_set_rate(bus, rate, old_rate);
};

static void tegra_clk_shared_bus_user_init(struct clk *c)
{
	c->max_rate = c->parent->max_rate;
	c->u.shared_bus_user.rate = c->parent->max_rate;
	c->state = OFF;
	c->set = true;

	if (c->u.shared_bus_user.client_id) {
		c->u.shared_bus_user.client =
			tegra_get_clock_by_name(c->u.shared_bus_user.client_id);
		if (!c->u.shared_bus_user.client) {
			pr_err("%s: could not find clk %s\n", __func__,
			       c->u.shared_bus_user.client_id);
			return;
		}
		c->u.shared_bus_user.client->flags |=
			c->parent->flags & PERIPH_ON_CBUS;
		c->flags |= c->parent->flags & PERIPH_ON_CBUS;
		c->div = c->u.shared_bus_user.client_div ? : 1;
		c->mul = 1;
	}

	list_add_tail(&c->u.shared_bus_user.node,
		&c->parent->shared_bus_list);
}

static int tegra_clk_shared_bus_user_set_rate(struct clk *c, unsigned long rate)
{
	c->u.shared_bus_user.rate = rate;
	return tegra_clk_shared_bus_update(c->parent);
}

static long tegra_clk_shared_bus_user_round_rate(
	struct clk *c, unsigned long rate)
{
	/* auto user follow others, by itself it run at minimum bus rate */
	if (c->u.shared_bus_user.mode == SHARED_AUTO)
		rate = 0;

	/* BW users should not be rounded until aggregated */
	if (c->u.shared_bus_user.mode == SHARED_BW)
		return rate;

	return clk_round_rate(c->parent, rate);
}

static int tegra_clk_shared_bus_user_enable(struct clk *c)
{
	int ret;

	c->u.shared_bus_user.enabled = true;
	ret = tegra_clk_shared_bus_update(c->parent);
	if (!ret && c->u.shared_bus_user.client)
		return tegra_clk_prepare_enable(c->u.shared_bus_user.client);

	return ret;
}

static void tegra_clk_shared_bus_user_disable(struct clk *c)
{
	if (c->u.shared_bus_user.client)
		tegra_clk_disable_unprepare(c->u.shared_bus_user.client);
	c->u.shared_bus_user.enabled = false;
	tegra_clk_shared_bus_update(c->parent);
}

static void tegra_clk_shared_bus_user_reset(struct clk *c, bool assert)
{
	if (c->u.shared_bus_user.client) {
		if (c->u.shared_bus_user.client->ops &&
		    c->u.shared_bus_user.client->ops->reset)
			c->u.shared_bus_user.client->ops->reset(
				c->u.shared_bus_user.client, assert);
	}
}

static struct clk_ops tegra_clk_shared_bus_user_ops = {
	.init = tegra_clk_shared_bus_user_init,
	.enable = tegra_clk_shared_bus_user_enable,
	.disable = tegra_clk_shared_bus_user_disable,
	.set_rate = tegra_clk_shared_bus_user_set_rate,
	.round_rate = tegra_clk_shared_bus_user_round_rate,
	.reset = tegra_clk_shared_bus_user_reset,
};

/* emc bridge ops */
/* On Tegra3 platforms emc configurations for DDR3 low rates can not work
 * at high core voltage; the intermediate step (bridge) is mandatory whenever
 * core voltage is crossing the threshold: TEGRA_EMC_BRIDGE_MVOLTS_MIN (fixed
 * for the entire Tegra3 arch); also emc must run above the bridge rate if any
 * other than emc clock requires high voltage. LP CPU, memory, sbus and cbus
 * together include all clocks that may require core voltage above threshold
 * (other peripherals can reach their maximum rates below threshold). LP CPU
 * dependency is taken care of via tegra_emc_to_cpu_ratio() api. Memory clock
 * transitions are forced to step through bridge rate; sbus and cbus control
 * emc bridge to set emc clock floor as necessary.
 *
 * EMC bridge is implemented as a special emc shared bus user: initialized at
 * minimum rate until updated once by emc dvfs setup; then it is only enabled
 * or disabled when sbus and/or cbus voltage is crossing the threshold.
 */
static void tegra3_clk_emc_bridge_init(struct clk *c)
{
	tegra_clk_shared_bus_user_init(c);
	c->u.shared_bus_user.rate = 0;
}

static int tegra3_clk_emc_bridge_set_rate(struct clk *c, unsigned long rate)
{
	if (c->u.shared_bus_user.rate == 0)
		c->u.shared_bus_user.rate = rate;
	return 0;
}

static struct clk_ops tegra_clk_emc_bridge_ops = {
	.init = tegra3_clk_emc_bridge_init,
	.enable = tegra_clk_shared_bus_user_enable,
	.disable = tegra_clk_shared_bus_user_disable,
	.set_rate = tegra3_clk_emc_bridge_set_rate,
	.round_rate = tegra_clk_shared_bus_user_round_rate,
};

/* Clock definitions */
static struct clk tegra_clk_32k = {
	.name = "clk_32k",
	.rate = 32768,
	.ops  = NULL,
	.max_rate = 32768,
};

static struct clk tegra_clk_m = {
	.name      = "clk_m",
	.flags     = ENABLE_ON_INIT,
	.ops       = &tegra_clk_m_ops,
	.reg       = 0x1fc,
	.reg_shift = 28,
	.max_rate  = 48000000,
};

static struct clk tegra_clk_m_div2 = {
	.name      = "clk_m_div2",
	.ops       = &tegra_clk_m_div_ops,
	.parent    = &tegra_clk_m,
	.mul       = 1,
	.div       = 2,
	.state     = ON,
	.max_rate  = 24000000,
};

static struct clk tegra_clk_m_div4 = {
	.name      = "clk_m_div4",
	.ops       = &tegra_clk_m_div_ops,
	.parent    = &tegra_clk_m,
	.mul       = 1,
	.div       = 4,
	.state     = ON,
	.max_rate  = 12000000,
};

static struct clk tegra_pll_ref = {
	.name      = "pll_ref",
	.flags     = ENABLE_ON_INIT,
	.ops       = &tegra_pll_ref_ops,
	.parent    = &tegra_clk_m,
	.max_rate  = 26000000,
};

static struct clk_pll_freq_table tegra_pll_c_freq_table[] = {
	{ 12000000, 1200000000, 600,  6, 1, 8},
	{ 13000000, 1200000000, 923, 10, 1, 8},		/* actual: 1199.9 MHz */
	{ 16800000, 1200000000, 500,  7, 1, 8},
	{ 19200000, 1200000000, 500,  8, 1, 8},
	{ 26000000, 1200000000, 600, 13, 1, 8},

	{ 12000000, 1040000000, 520,  6, 1, 8},
	{ 13000000, 1040000000, 480,  6, 1, 8},
	{ 16800000, 1040000000, 495,  8, 1, 8},		/* actual: 1039.5 MHz */
	{ 19200000, 1040000000, 325,  6, 1, 6},
	{ 26000000, 1040000000, 520, 13, 1, 8},

	{ 12000000, 832000000, 416,  6, 1, 8},
	{ 13000000, 832000000, 832, 13, 1, 8},
	{ 16800000, 832000000, 396,  8, 1, 8},		/* actual: 831.6 MHz */
	{ 19200000, 832000000, 260,  6, 1, 8},
	{ 26000000, 832000000, 416, 13, 1, 8},

	{ 12000000, 624000000, 624, 12, 1, 8},
	{ 13000000, 624000000, 624, 13, 1, 8},
	{ 16800000, 624000000, 520, 14, 1, 8},
	{ 19200000, 624000000, 520, 16, 1, 8},
	{ 26000000, 624000000, 624, 26, 1, 8},

	{ 12000000, 600000000, 600, 12, 1, 8},
	{ 13000000, 600000000, 600, 13, 1, 8},
	{ 16800000, 600000000, 500, 14, 1, 8},
	{ 19200000, 600000000, 375, 12, 1, 6},
	{ 26000000, 600000000, 600, 26, 1, 8},

	{ 12000000, 520000000, 520, 12, 1, 8},
	{ 13000000, 520000000, 520, 13, 1, 8},
	{ 16800000, 520000000, 495, 16, 1, 8},		/* actual: 519.75 MHz */
	{ 19200000, 520000000, 325, 12, 1, 6},
	{ 26000000, 520000000, 520, 26, 1, 8},

	{ 12000000, 416000000, 416, 12, 1, 8},
	{ 13000000, 416000000, 416, 13, 1, 8},
	{ 16800000, 416000000, 396, 16, 1, 8},		/* actual: 415.8 MHz */
	{ 19200000, 416000000, 260, 12, 1, 6},
	{ 26000000, 416000000, 416, 26, 1, 8},
	{ 0, 0, 0, 0, 0, 0 },
};

static struct clk tegra_pll_c = {
	.name      = "pll_c",
	.flags	   = PLL_HAS_CPCON,
	.ops       = &tegra_pll_ops,
	.reg       = 0x80,
	.parent    = &tegra_pll_ref,
	.max_rate  = 1400000000,
	.u.pll = {
		.input_min = 2000000,
		.input_max = 31000000,
		.cf_min    = 1000000,
		.cf_max    = 6000000,
		.vco_min   = 20000000,
		.vco_max   = 1400000000,
		.freq_table = tegra_pll_c_freq_table,
		.lock_delay = 300,
	},
};

static struct clk tegra_pll_c_out1 = {
	.name      = "pll_c_out1",
	.ops       = &tegra_pll_div_ops,
	.flags     = DIV_U71 | PERIPH_ON_CBUS,
	.parent    = &tegra_pll_c,
	.reg       = 0x84,
	.reg_shift = 0,
	.max_rate  = 700000000,
};

static struct clk_pll_freq_table tegra_pll_m_freq_table[] = {
	{ 12000000, 666000000, 666, 12, 1, 8},
	{ 13000000, 666000000, 666, 13, 1, 8},
	{ 16800000, 666000000, 555, 14, 1, 8},
	{ 19200000, 666000000, 555, 16, 1, 8},
	{ 26000000, 666000000, 666, 26, 1, 8},
	{ 12000000, 600000000, 600, 12, 1, 8},
	{ 13000000, 600000000, 600, 13, 1, 8},
	{ 16800000, 600000000, 500, 14, 1, 8},
	{ 19200000, 600000000, 375, 12, 1, 6},
	{ 26000000, 600000000, 600, 26, 1, 8},
	{ 0, 0, 0, 0, 0, 0 },
};

static struct clk tegra_pll_m = {
	.name      = "pll_m",
	.flags     = PLL_HAS_CPCON | PLLM,
	.ops       = &tegra_pll_ops,
	.reg       = 0x90,
	.parent    = &tegra_pll_ref,
	.max_rate  = 900000000,
	.u.pll = {
		.input_min = 2000000,
		.input_max = 31000000,
		.cf_min    = 1000000,
		.cf_max    = 6000000,
		.vco_min   = 20000000,
		.vco_max   = 1200000000,
		.freq_table = tegra_pll_m_freq_table,
		.lock_delay = 300,
	},
};

static struct clk tegra_pll_m_out1 = {
	.name      = "pll_m_out1",
	.ops       = &tegra_pll_div_ops,
	.flags     = DIV_U71,
	.parent    = &tegra_pll_m,
	.reg       = 0x94,
	.reg_shift = 0,
	.max_rate  = 900000000,
};

static struct clk_pll_freq_table tegra_pll_p_freq_table[] = {
	{ 12000000, 216000000, 432, 12, 2, 8},
	{ 13000000, 216000000, 432, 13, 2, 8},
	{ 16800000, 216000000, 360, 14, 2, 8},
	{ 19200000, 216000000, 360, 16, 2, 8},
	{ 26000000, 216000000, 432, 26, 2, 8},
	{ 0, 0, 0, 0, 0, 0 },
};

static struct clk tegra_pll_p = {
	.name      = "pll_p",
	.flags     = ENABLE_ON_INIT | PLL_FIXED | PLL_HAS_CPCON,
	.ops       = &tegra_pllp_ops,
	.reg       = 0xa0,
	.parent    = &tegra_pll_ref,
	.max_rate  = 432000000,
	.u.pll = {
		.input_min = 2000000,
		.input_max = 31000000,
		.cf_min    = 1000000,
		.cf_max    = 6000000,
		.vco_min   = 20000000,
		.vco_max   = 1400000000,
		.freq_table = tegra_pll_p_freq_table,
		.lock_delay = 300,
	},
};

static struct clk tegra_pll_p_out1 = {
	.name      = "pll_p_out1",
	.ops       = &tegra_pll_div_ops,
	.flags     = DIV_U71 | DIV_U71_FIXED,
	.parent    = &tegra_pll_p,
	.reg       = 0xa4,
	.reg_shift = 0,
	.max_rate  = 432000000,
};

static struct clk tegra_pll_p_out2 = {
	.name      = "pll_p_out2",
	.ops       = &tegra_pll_div_ops,
	.flags     = DIV_U71 | DIV_U71_FIXED,
	.parent    = &tegra_pll_p,
	.reg       = 0xa4,
	.reg_shift = 16,
	.max_rate  = 432000000,
};

static struct clk tegra_pll_p_out3 = {
	.name      = "pll_p_out3",
	.ops       = &tegra_pll_div_ops,
	.flags     = DIV_U71 | DIV_U71_FIXED,
	.parent    = &tegra_pll_p,
	.reg       = 0xa8,
	.reg_shift = 0,
	.max_rate  = 432000000,
};

static struct clk tegra_pll_p_out4 = {
	.name      = "pll_p_out4",
	.ops       = &tegra_pll_div_ops,
	.flags     = ENABLE_ON_INIT | DIV_U71 | DIV_U71_FIXED,
	.parent    = &tegra_pll_p,
	.reg       = 0xa8,
	.reg_shift = 16,
	.max_rate  = 432000000,
};

static struct clk_pll_freq_table tegra_pll_a_freq_table[] = {
	{ 9600000, 564480000, 294, 5, 1, 4},
	{ 9600000, 552960000, 288, 5, 1, 4},
	{ 9600000, 24000000,  5,   2, 1, 1},

	{ 28800000, 56448000, 49, 25, 1, 1},
	{ 28800000, 73728000, 64, 25, 1, 1},
	{ 28800000, 24000000,  5,  6, 1, 1},
	{ 0, 0, 0, 0, 0, 0 },
};

static struct clk tegra_pll_a = {
	.name      = "pll_a",
	.flags     = PLL_HAS_CPCON,
	.ops       = &tegra_pll_ops,
	.reg       = 0xb0,
	.parent    = &tegra_pll_p_out1,
	.max_rate  = 700000000,
	.u.pll = {
		.input_min = 2000000,
		.input_max = 31000000,
		.cf_min    = 1000000,
		.cf_max    = 6000000,
		.vco_min   = 20000000,
		.vco_max   = 1400000000,
		.freq_table = tegra_pll_a_freq_table,
		.lock_delay = 300,
	},
};

static struct clk tegra_pll_a_out0 = {
	.name      = "pll_a_out0",
	.ops       = &tegra_pll_div_ops,
	.flags     = DIV_U71,
	.parent    = &tegra_pll_a,
	.reg       = 0xb4,
	.reg_shift = 0,
	.max_rate  = 100000000,
};

static struct clk_pll_freq_table tegra_pll_d_freq_table[] = {
	{ 12000000, 216000000, 216, 12, 1, 4},
	{ 13000000, 216000000, 216, 13, 1, 4},
	{ 16800000, 216000000, 180, 14, 1, 4},
	{ 19200000, 216000000, 180, 16, 1, 4},
	{ 26000000, 216000000, 216, 26, 1, 4},

	{ 12000000, 594000000, 594, 12, 1, 8},
	{ 13000000, 594000000, 594, 13, 1, 8},
	{ 16800000, 594000000, 495, 14, 1, 8},
	{ 19200000, 594000000, 495, 16, 1, 8},
	{ 26000000, 594000000, 594, 26, 1, 8},

	{ 12000000, 1000000000, 1000, 12, 1, 12},
	{ 13000000, 1000000000, 1000, 13, 1, 12},
	{ 19200000, 1000000000, 625,  12, 1, 8},
	{ 26000000, 1000000000, 1000, 26, 1, 12},

	{ 0, 0, 0, 0, 0, 0 },
};

static struct clk tegra_pll_d = {
	.name      = "pll_d",
	.flags     = PLL_HAS_CPCON | PLLD,
	.ops       = &tegra_plld_ops,
	.reg       = 0xd0,
	.parent    = &tegra_pll_ref,
	.max_rate  = 1000000000,
	.u.pll = {
		.input_min = 2000000,
		.input_max = 40000000,
		.cf_min    = 1000000,
		.cf_max    = 6000000,
		.vco_min   = 40000000,
		.vco_max   = 1000000000,
		.freq_table = tegra_pll_d_freq_table,
		.lock_delay = 1000,
	},
};

static struct clk tegra_pll_d_out0 = {
	.name      = "pll_d_out0",
	.ops       = &tegra_pll_div_ops,
	.flags     = DIV_2 | PLLD,
	.parent    = &tegra_pll_d,
	.max_rate  = 500000000,
};

static struct clk tegra_pll_d2 = {
	.name      = "pll_d2",
	.flags     = PLL_HAS_CPCON | PLL_ALT_MISC_REG | PLLD,
	.ops       = &tegra_plld_ops,
	.reg       = 0x4b8,
	.parent    = &tegra_pll_ref,
	.max_rate  = 1000000000,
	.u.pll = {
		.input_min = 2000000,
		.input_max = 40000000,
		.cf_min    = 1000000,
		.cf_max    = 6000000,
		.vco_min   = 40000000,
		.vco_max   = 1000000000,
		.freq_table = tegra_pll_d_freq_table,
		.lock_delay = 1000,
	},
};

static struct clk tegra_pll_d2_out0 = {
	.name      = "pll_d2_out0",
	.ops       = &tegra_pll_div_ops,
	.flags     = DIV_2 | PLLD,
	.parent    = &tegra_pll_d2,
	.max_rate  = 500000000,
};

static struct clk_pll_freq_table tegra_pll_u_freq_table[] = {
	{ 12000000, 480000000, 960, 12, 2, 12},
	{ 13000000, 480000000, 960, 13, 2, 12},
	{ 16800000, 480000000, 400, 7,  2, 5},
	{ 19200000, 480000000, 200, 4,  2, 3},
	{ 26000000, 480000000, 960, 26, 2, 12},
	{ 0, 0, 0, 0, 0, 0 },
};

static struct clk tegra_pll_u = {
	.name      = "pll_u",
	.flags     = PLL_HAS_CPCON | PLLU,
	.ops       = &tegra_pll_ops,
	.reg       = 0xc0,
	.parent    = &tegra_pll_ref,
	.max_rate  = 480000000,
	.u.pll = {
		.input_min = 2000000,
		.input_max = 40000000,
		.cf_min    = 1000000,
		.cf_max    = 6000000,
		.vco_min   = 480000000,
		.vco_max   = 960000000,
		.freq_table = tegra_pll_u_freq_table,
		.lock_delay = 1000,
	},
};

static struct clk_pll_freq_table tegra_pll_x_freq_table[] = {
	/* 1.7 GHz */
	{ 12000000, 1700000000, 850,  6,  1, 8},
	{ 13000000, 1700000000, 915,  7,  1, 8},	/* actual: 1699.2 MHz */
	{ 16800000, 1700000000, 708,  7,  1, 8},	/* actual: 1699.2 MHz */
	{ 19200000, 1700000000, 885,  10, 1, 8},	/* actual: 1699.2 MHz */
	{ 26000000, 1700000000, 850,  13, 1, 8},

	/* 1.6 GHz */
	{ 12000000, 1600000000, 800,  6,  1, 8},
	{ 13000000, 1600000000, 738,  6,  1, 8},	/* actual: 1599.0 MHz */
	{ 16800000, 1600000000, 857,  9,  1, 8},	/* actual: 1599.7 MHz */
	{ 19200000, 1600000000, 500,  6,  1, 8},
	{ 26000000, 1600000000, 800,  13, 1, 8},

	/* 1.5 GHz */
	{ 12000000, 1500000000, 750,  6,  1, 8},
	{ 13000000, 1500000000, 923,  8,  1, 8},	/* actual: 1499.8 MHz */
	{ 16800000, 1500000000, 625,  7,  1, 8},
	{ 19200000, 1500000000, 625,  8,  1, 8},
	{ 26000000, 1500000000, 750,  13, 1, 8},

	/* 1.4 GHz */
	{ 12000000, 1400000000, 700,  6,  1, 8},
	{ 13000000, 1400000000, 969,  9,  1, 8},	/* actual: 1399.7 MHz */
	{ 16800000, 1400000000, 1000, 12, 1, 8},
	{ 19200000, 1400000000, 875,  12, 1, 8},
	{ 26000000, 1400000000, 700,  13, 1, 8},

	/* 1.3 GHz */
	{ 12000000, 1300000000, 975,  9,  1, 8},
	{ 13000000, 1300000000, 1000, 10, 1, 8},
	{ 16800000, 1300000000, 928,  12, 1, 8},	/* actual: 1299.2 MHz */
	{ 19200000, 1300000000, 812,  12, 1, 8},	/* actual: 1299.2 MHz */
	{ 26000000, 1300000000, 650,  13, 1, 8},

	/* 1.2 GHz */
	{ 12000000, 1200000000, 1000, 10, 1, 8},
	{ 13000000, 1200000000, 923,  10, 1, 8},	/* actual: 1199.9 MHz */
	{ 16800000, 1200000000, 1000, 14, 1, 8},
	{ 19200000, 1200000000, 1000, 16, 1, 8},
	{ 26000000, 1200000000, 600,  13, 1, 8},

	/* 1.1 GHz */
	{ 12000000, 1100000000, 825,  9,  1, 8},
	{ 13000000, 1100000000, 846,  10, 1, 8},	/* actual: 1099.8 MHz */
	{ 16800000, 1100000000, 982,  15, 1, 8},	/* actual: 1099.8 MHz */
	{ 19200000, 1100000000, 859,  15, 1, 8},	/* actual: 1099.5 MHz */
	{ 26000000, 1100000000, 550,  13, 1, 8},

	/* 1 GHz */
	{ 12000000, 1000000000, 1000, 12, 1, 8},
	{ 13000000, 1000000000, 1000, 13, 1, 8},
	{ 16800000, 1000000000, 833,  14, 1, 8},	/* actual: 999.6 MHz */
	{ 19200000, 1000000000, 625,  12, 1, 8},
	{ 26000000, 1000000000, 1000, 26, 1, 8},

	{ 0, 0, 0, 0, 0, 0 },
};

static struct clk tegra_pll_x = {
	.name      = "pll_x",
	.flags     = PLL_HAS_CPCON | PLL_ALT_MISC_REG | PLLX,
	.ops       = &tegra_pll_ops,
	.reg       = 0xe0,
	.parent    = &tegra_pll_ref,
	.max_rate  = 1700000000,
	.u.pll = {
		.input_min = 2000000,
		.input_max = 31000000,
		.cf_min    = 1000000,
		.cf_max    = 6000000,
		.vco_min   = 20000000,
		.vco_max   = 1700000000,
		.freq_table = tegra_pll_x_freq_table,
		.lock_delay = 300,
	},
};

static struct clk tegra_pll_x_out0 = {
	.name      = "pll_x_out0",
	.ops       = &tegra_pll_div_ops,
	.flags     = DIV_2 | PLLX,
	.parent    = &tegra_pll_x,
	.max_rate  = 850000000,
};


static struct clk_pll_freq_table tegra_pll_e_freq_table[] = {
	/* PLLE special case: use cpcon field to store cml divider value */
	{ 12000000,  100000000, 150, 1,  18, 11},
	{ 216000000, 100000000, 200, 18, 24, 13},
#ifndef CONFIG_TEGRA_SILICON_PLATFORM
	{ 13000000,  100000000, 200, 1,  26, 13},
#endif
	{ 0, 0, 0, 0, 0, 0 },
};

static struct clk tegra_pll_e = {
	.name      = "pll_e",
	.flags     = PLL_ALT_MISC_REG,
	.ops       = &tegra_plle_ops,
	.reg       = 0xe8,
	.max_rate  = 100000000,
	.u.pll = {
		.input_min = 12000000,
		.input_max = 216000000,
		.cf_min    = 12000000,
		.cf_max    = 12000000,
		.vco_min   = 1200000000,
		.vco_max   = 2400000000U,
		.freq_table = tegra_pll_e_freq_table,
		.lock_delay = 300,
		.fixed_rate = 100000000,
	},
};

static struct clk tegra_cml0_clk = {
	.name      = "cml0",
	.parent    = &tegra_pll_e,
	.ops       = &tegra_cml_clk_ops,
	.reg       = PLLE_AUX,
	.max_rate  = 100000000,
	.u.periph  = {
		.clk_num = 0,
	},
};

static struct clk tegra_cml1_clk = {
	.name      = "cml1",
	.parent    = &tegra_pll_e,
	.ops       = &tegra_cml_clk_ops,
	.reg       = PLLE_AUX,
	.max_rate  = 100000000,
	.u.periph  = {
		.clk_num   = 1,
	},
};

static struct clk tegra_pciex_clk = {
	.name      = "pciex",
	.parent    = &tegra_pll_e,
	.ops       = &tegra_pciex_clk_ops,
	.max_rate  = 100000000,
	.u.periph  = {
		.clk_num   = 74,
	},
};

/* Audio sync clocks */
#define SYNC_SOURCE(_id, _dev)				\
	{						\
		.name      = #_id "_sync",		\
		.lookup    = {				\
			.dev_id    = #_dev ,		\
			.con_id    = "ext_audio_sync",	\
		},					\
		.rate      = 24000000,			\
		.max_rate  = 24000000,			\
		.ops       = &tegra_sync_source_ops	\
	}
static struct clk tegra_sync_source_list[] = {
	SYNC_SOURCE(spdif_in, tegra30-spdif),
	SYNC_SOURCE(i2s0, tegra30-i2s.0),
	SYNC_SOURCE(i2s1, tegra30-i2s.1),
	SYNC_SOURCE(i2s2, tegra30-i2s.2),
	SYNC_SOURCE(i2s3, tegra30-i2s.3),
	SYNC_SOURCE(i2s4, tegra30-i2s.4),
	SYNC_SOURCE(vimclk, vimclk),
};

static struct clk_mux_sel mux_audio_sync_clk[] =
{
	{ .input = &tegra_sync_source_list[0],	.value = 0},
	{ .input = &tegra_sync_source_list[1],	.value = 1},
	{ .input = &tegra_sync_source_list[2],	.value = 2},
	{ .input = &tegra_sync_source_list[3],	.value = 3},
	{ .input = &tegra_sync_source_list[4],	.value = 4},
	{ .input = &tegra_sync_source_list[5],	.value = 5},
	{ .input = &tegra_pll_a_out0,		.value = 6},
	{ .input = &tegra_sync_source_list[6],	.value = 7},
	{ 0, 0 }
};

#define AUDIO_SYNC_CLK(_id, _dev, _index)		\
	{						\
		.name      = #_id,			\
		.lookup    = {				\
			.dev_id    = #_dev,		\
			.con_id    = "audio_sync",	\
		},					\
		.inputs    = mux_audio_sync_clk,	\
		.reg       = 0x4A0 + (_index) * 4,	\
		.max_rate  = 24000000,			\
		.ops       = &tegra_audio_sync_clk_ops	\
	}
static struct clk tegra_clk_audio_list[] = {
	AUDIO_SYNC_CLK(audio0, tegra30-i2s.0, 0),
	AUDIO_SYNC_CLK(audio1, tegra30-i2s.1, 1),
	AUDIO_SYNC_CLK(audio2, tegra30-i2s.2, 2),
	AUDIO_SYNC_CLK(audio3, tegra30-i2s.3, 3),
	AUDIO_SYNC_CLK(audio4, tegra30-i2s.4, 4),
	AUDIO_SYNC_CLK(audio, tegra30-spdif, 5),
};

#define AUDIO_SYNC_2X_CLK(_id, _dev, _index)			\
	{							\
		.name      = #_id "_2x",			\
		.lookup    = {					\
			.dev_id    = #_dev,			\
			.con_id    = "audio_sync_2x"		\
		},						\
		.flags     = PERIPH_NO_RESET,			\
		.max_rate  = 48000000,				\
		.ops       = &tegra_clk_double_ops,		\
		.reg       = 0x49C,				\
		.reg_shift = 24 + (_index),			\
		.parent    = &tegra_clk_audio_list[(_index)],	\
		.u.periph = {					\
			.clk_num = 113 + (_index),		\
		},						\
	}
static struct clk tegra_clk_audio_2x_list[] = {
	AUDIO_SYNC_2X_CLK(audio0, tegra30-i2s.0, 0),
	AUDIO_SYNC_2X_CLK(audio1, tegra30-i2s.1, 1),
	AUDIO_SYNC_2X_CLK(audio2, tegra30-i2s.2, 2),
	AUDIO_SYNC_2X_CLK(audio3, tegra30-i2s.3, 3),
	AUDIO_SYNC_2X_CLK(audio4, tegra30-i2s.4, 4),
	AUDIO_SYNC_2X_CLK(audio, tegra30-spdif, 5),
};

#define MUX_I2S_SPDIF(_id, _index)					\
static struct clk_mux_sel mux_pllaout0_##_id##_2x_pllp_clkm[] = {	\
	{.input = &tegra_pll_a_out0, .value = 0},			\
	{.input = &tegra_clk_audio_2x_list[(_index)], .value = 1},	\
	{.input = &tegra_pll_p, .value = 2},				\
	{.input = &tegra_clk_m, .value = 3},				\
	{ 0, 0},							\
}
MUX_I2S_SPDIF(audio0, 0);
MUX_I2S_SPDIF(audio1, 1);
MUX_I2S_SPDIF(audio2, 2);
MUX_I2S_SPDIF(audio3, 3);
MUX_I2S_SPDIF(audio4, 4);
MUX_I2S_SPDIF(audio, 5);		/* SPDIF */

/* External clock outputs (through PMC) */
#define MUX_EXTERN_OUT(_id)						\
static struct clk_mux_sel mux_clkm_clkm2_clkm4_extern##_id[] = {	\
	{.input = &tegra_clk_m,		.value = 0},			\
	{.input = &tegra_clk_m_div2,	.value = 1},			\
	{.input = &tegra_clk_m_div4,	.value = 2},			\
	{.input = NULL,			.value = 3}, /* placeholder */	\
	{ 0, 0},							\
}
MUX_EXTERN_OUT(1);
MUX_EXTERN_OUT(2);
MUX_EXTERN_OUT(3);

static struct clk_mux_sel *mux_extern_out_list[] = {
	mux_clkm_clkm2_clkm4_extern1,
	mux_clkm_clkm2_clkm4_extern2,
	mux_clkm_clkm2_clkm4_extern3,
};

#define CLK_OUT_CLK(_id)					\
	{							\
		.name      = "clk_out_" #_id,			\
		.lookup    = {					\
			.dev_id    = "clk_out_" #_id,		\
			.con_id	   = "extern" #_id,		\
		},						\
		.ops       = &tegra_clk_out_ops,		\
		.reg       = 0x1a8,				\
		.inputs    = mux_clkm_clkm2_clkm4_extern##_id,	\
		.flags     = MUX_CLK_OUT,			\
		.max_rate  = 216000000,				\
		.u.periph = {					\
			.clk_num   = (_id - 1) * 8 + 2,		\
		},						\
	}
static struct clk tegra_clk_out_list[] = {
	CLK_OUT_CLK(1),
	CLK_OUT_CLK(2),
	CLK_OUT_CLK(3),
};

/* called after peripheral external clocks are initialized */
static void init_clk_out_mux(void)
{
	int i;
	struct clk *c;

	/* output clock con_id is the name of peripheral
	   external clock connected to input 3 of the output mux */
	for (i = 0; i < ARRAY_SIZE(tegra_clk_out_list); i++) {
		c = tegra_get_clock_by_name(
			tegra_clk_out_list[i].lookup.con_id);
		if (!c)
			pr_err("%s: could not find clk %s\n", __func__,
			       tegra_clk_out_list[i].lookup.con_id);
		mux_extern_out_list[i][3].input = c;
	}
}

/* Peripheral muxes */
static struct clk_mux_sel mux_cclk_g[] = {
	{ .input = &tegra_clk_m,	.value = 0},
	{ .input = &tegra_pll_c,	.value = 1},
	{ .input = &tegra_clk_32k,	.value = 2},
	{ .input = &tegra_pll_m,	.value = 3},
	{ .input = &tegra_pll_p,	.value = 4},
	{ .input = &tegra_pll_p_out4,	.value = 5},
	{ .input = &tegra_pll_p_out3,	.value = 6},
	/* { .input = &tegra_clk_d,	.value = 7}, - no use on tegra3 */
	{ .input = &tegra_pll_x,	.value = 8},
	{ 0, 0},
};

static struct clk_mux_sel mux_cclk_lp[] = {
	{ .input = &tegra_clk_m,	.value = 0},
	{ .input = &tegra_pll_c,	.value = 1},
	{ .input = &tegra_clk_32k,	.value = 2},
	{ .input = &tegra_pll_m,	.value = 3},
	{ .input = &tegra_pll_p,	.value = 4},
	{ .input = &tegra_pll_p_out4,	.value = 5},
	{ .input = &tegra_pll_p_out3,	.value = 6},
	/* { .input = &tegra_clk_d,	.value = 7}, - no use on tegra3 */
	{ .input = &tegra_pll_x_out0,	.value = 8},
	{ .input = &tegra_pll_x,	.value = 8 | SUPER_LP_DIV2_BYPASS},
	{ 0, 0},
};

static struct clk_mux_sel mux_sclk[] = {
	{ .input = &tegra_clk_m,	.value = 0},
	{ .input = &tegra_pll_c_out1,	.value = 1},
	{ .input = &tegra_pll_p_out4,	.value = 2},
	{ .input = &tegra_pll_p_out3,	.value = 3},
	{ .input = &tegra_pll_p_out2,	.value = 4},
	/* { .input = &tegra_clk_d,	.value = 5}, - no use on tegra3 */
	{ .input = &tegra_clk_32k,	.value = 6},
	{ .input = &tegra_pll_m_out1,	.value = 7},
	{ 0, 0},
};

static struct clk tegra_clk_cclk_g = {
	.name	= "cclk_g",
	.flags  = DIV_U71 | DIV_U71_INT,
	.inputs	= mux_cclk_g,
	.reg	= 0x368,
	.ops	= &tegra_super_ops,
	.max_rate = 1700000000,
};

static struct clk tegra_clk_cclk_lp = {
	.name	= "cclk_lp",
	.flags  = DIV_2 | DIV_U71 | DIV_U71_INT,
	.inputs	= mux_cclk_lp,
	.reg	= 0x370,
	.ops	= &tegra_super_ops,
	.max_rate = 620000000,
};

static struct clk tegra_clk_sclk = {
	.name	= "sclk",
	.inputs	= mux_sclk,
	.reg	= 0x28,
	.ops	= &tegra_super_ops,
	.max_rate = 378000000,
	.min_rate = 12000000,
};

static struct clk tegra_clk_virtual_cpu_g = {
	.name      = "cpu_g",
	.parent    = &tegra_clk_cclk_g,
	.ops       = &tegra_cpu_ops,
	.max_rate  = 1700000000,
	.u.cpu = {
		.main      = &tegra_pll_x,
		.backup    = &tegra_pll_p,
		.mode      = MODE_G,
	},
};

static struct clk tegra_clk_virtual_cpu_lp = {
	.name      = "cpu_lp",
	.parent    = &tegra_clk_cclk_lp,
	.ops       = &tegra_cpu_ops,
	.max_rate  = 620000000,
	.u.cpu = {
		.main      = &tegra_pll_x,
		.backup    = &tegra_pll_p,
		.mode      = MODE_LP,
	},
};

static struct clk_mux_sel mux_cpu_cmplx[] = {
	{ .input = &tegra_clk_virtual_cpu_g,	.value = 0},
	{ .input = &tegra_clk_virtual_cpu_lp,	.value = 1},
	{ 0, 0},
};

static struct clk tegra_clk_cpu_cmplx = {
	.name      = "cpu",
	.inputs    = mux_cpu_cmplx,
	.ops       = &tegra_cpu_cmplx_ops,
	.max_rate  = 1700000000,
};

static struct clk tegra_clk_cop = {
	.name      = "cop",
	.parent    = &tegra_clk_sclk,
	.ops       = &tegra_cop_ops,
	.max_rate  = 378000000,
};

static struct clk tegra_clk_hclk = {
	.name		= "hclk",
	.flags		= DIV_BUS,
	.parent		= &tegra_clk_sclk,
	.reg		= 0x30,
	.reg_shift	= 4,
	.ops		= &tegra_bus_ops,
	.max_rate       = 378000000,
	.min_rate       = 12000000,
};

static struct clk tegra_clk_pclk = {
	.name		= "pclk",
	.flags		= DIV_BUS,
	.parent		= &tegra_clk_hclk,
	.reg		= 0x30,
	.reg_shift	= 0,
	.ops		= &tegra_bus_ops,
	.max_rate       = 167000000,
	.min_rate       = 12000000,
};

static struct raw_notifier_head sbus_rate_change_nh;

static struct clk tegra_clk_sbus_cmplx = {
	.name	   = "sbus",
	.parent    = &tegra_clk_sclk,
	.ops       = &tegra_sbus_cmplx_ops,
	.u.system  = {
		.pclk = &tegra_clk_pclk,
		.hclk = &tegra_clk_hclk,
		.sclk_low = &tegra_pll_p_out4,
		.sclk_high = &tegra_pll_m_out1,
	},
	.rate_change_nh = &sbus_rate_change_nh,
};

static struct clk tegra_clk_blink = {
	.name		= "blink",
	.parent		= &tegra_clk_32k,
	.reg		= 0x40,
	.ops		= &tegra_blink_clk_ops,
	.max_rate	= 32768,
};

static struct clk_mux_sel mux_pllm_pllc_pllp_plla[] = {
	{ .input = &tegra_pll_m, .value = 0},
	{ .input = &tegra_pll_c, .value = 1},
	{ .input = &tegra_pll_p, .value = 2},
	{ .input = &tegra_pll_a_out0, .value = 3},
	{ 0, 0},
};

static struct clk_mux_sel mux_pllm_pllc_pllp_clkm[] = {
	{ .input = &tegra_pll_m, .value = 0},
	{ .input = &tegra_pll_c, .value = 1},
	{ .input = &tegra_pll_p, .value = 2},
	{ .input = &tegra_clk_m, .value = 3},
	{ 0, 0},
};

static struct clk_mux_sel mux_pllp_pllc_pllm_clkm[] = {
	{ .input = &tegra_pll_p, .value = 0},
	{ .input = &tegra_pll_c, .value = 1},
#ifndef CONFIG_TEGRA_PLLM_RESTRICTED
	{ .input = &tegra_pll_m, .value = 2},
#endif
	{ .input = &tegra_clk_m, .value = 3},
	{ 0, 0},
};

static struct clk_mux_sel mux_pllp_clkm[] = {
	{ .input = &tegra_pll_p, .value = 0},
	{ .input = &tegra_clk_m, .value = 3},
	{ 0, 0},
};

static struct clk_mux_sel mux_pllp_plld_pllc_clkm[] = {
	{.input = &tegra_pll_p, .value = 0},
	{.input = &tegra_pll_d_out0, .value = 1},
	{.input = &tegra_pll_c, .value = 2},
	{.input = &tegra_clk_m, .value = 3},
	{ 0, 0},
};

static struct clk_mux_sel mux_pllp_pllm_plld_plla_pllc_plld2_clkm[] = {
	{.input = &tegra_pll_p, .value = 0},
#ifndef CONFIG_TEGRA_PLLM_RESTRICTED
	{.input = &tegra_pll_m, .value = 1},
#endif
	{.input = &tegra_pll_d_out0, .value = 2},
	{.input = &tegra_pll_a_out0, .value = 3},
	{.input = &tegra_pll_c, .value = 4},
	{.input = &tegra_pll_d2_out0, .value = 5},
	{.input = &tegra_clk_m, .value = 6},
	{ 0, 0},
};

static struct clk_mux_sel mux_plla_pllc_pllp_clkm[] = {
	{ .input = &tegra_pll_a_out0, .value = 0},
	/* { .input = &tegra_pll_c, .value = 1}, no use on tegra3 */
	{ .input = &tegra_pll_p, .value = 2},
	{ .input = &tegra_clk_m, .value = 3},
	{ 0, 0},
};

static struct clk_mux_sel mux_pllp_pllc_clk32_clkm[] = {
	{.input = &tegra_pll_p,     .value = 0},
	{.input = &tegra_pll_c,     .value = 1},
	{.input = &tegra_clk_32k,   .value = 2},
	{.input = &tegra_clk_m,     .value = 3},
	{ 0, 0},
};

static struct clk_mux_sel mux_pllp_pllc_clkm_clk32[] = {
	{.input = &tegra_pll_p,     .value = 0},
	{.input = &tegra_pll_c,     .value = 1},
	{.input = &tegra_clk_m,     .value = 2},
	{.input = &tegra_clk_32k,   .value = 3},
	{ 0, 0},
};

static struct clk_mux_sel mux_pllp_pllc_pllm[] = {
	{.input = &tegra_pll_p,     .value = 0},
	{.input = &tegra_pll_c,     .value = 1},
#ifndef CONFIG_TEGRA_PLLM_RESTRICTED
	{.input = &tegra_pll_m,     .value = 2},
#endif
	{ 0, 0},
};

static struct clk_mux_sel mux_clk_m[] = {
	{ .input = &tegra_clk_m, .value = 0},
	{ 0, 0},
};

static struct clk_mux_sel mux_pllp_out3[] = {
	{ .input = &tegra_pll_p_out3, .value = 0},
	{ 0, 0},
};

static struct clk_mux_sel mux_plld_out0[] = {
	{ .input = &tegra_pll_d_out0, .value = 0},
	{ 0, 0},
};

static struct clk_mux_sel mux_plld_out0_plld2_out0[] = {
	{ .input = &tegra_pll_d_out0,  .value = 0},
	{ .input = &tegra_pll_d2_out0, .value = 1},
	{ 0, 0},
};

static struct clk_mux_sel mux_clk_32k[] = {
	{ .input = &tegra_clk_32k, .value = 0},
	{ 0, 0},
};

static struct clk_mux_sel mux_plla_clk32_pllp_clkm_plle[] = {
	{ .input = &tegra_pll_a_out0, .value = 0},
	{ .input = &tegra_clk_32k,    .value = 1},
	{ .input = &tegra_pll_p,      .value = 2},
	{ .input = &tegra_clk_m,      .value = 3},
	{ .input = &tegra_pll_e,      .value = 4},
	{ 0, 0},
};

static struct raw_notifier_head emc_rate_change_nh;

static struct clk tegra_clk_emc = {
	.name = "emc",
	.ops = &tegra_emc_clk_ops,
	.reg = 0x19c,
	.max_rate = 900000000,
	.min_rate = 12000000,
	.inputs = mux_pllm_pllc_pllp_clkm,
	.flags = MUX | DIV_U71 | PERIPH_EMC_ENB,
	.u.periph = {
		.clk_num = 57,
	},
	.shared_bus_backup = {
		.input = &tegra_pll_c,
	},
	.rate_change_nh = &emc_rate_change_nh,
};

static struct clk tegra_clk_emc_bridge = {
	.name      = "bridge.emc",
	.ops       = &tegra_clk_emc_bridge_ops,
	.parent    = &tegra_clk_emc,
};

static RAW_NOTIFIER_HEAD(cbus_rate_change_nh);

static struct clk tegra_clk_cbus = {
	.name	   = "cbus",
	.parent    = &tegra_pll_c,
	.ops       = &tegra_clk_cbus_ops,
	.max_rate  = 700000000,
	.mul	   = 1,
	.div	   = CONFIG_TEGRA_CBUS_CLOCK_DIVIDER,
	.flags     = PERIPH_ON_CBUS,
	.shared_bus_backup = {
		.input = &tegra_pll_p,
		.value = 2,
	},
	.rate_change_nh = &cbus_rate_change_nh,
};

#define PERIPH_CLK(_name, _dev, _con, _clk_num, _reg, _max, _inputs, _flags) \
	{						\
		.name      = _name,			\
		.lookup    = {				\
			.dev_id    = _dev,		\
			.con_id	   = _con,		\
		},					\
		.ops       = &tegra_periph_clk_ops,	\
		.reg       = _reg,			\
		.inputs    = _inputs,			\
		.flags     = _flags,			\
		.max_rate  = _max,			\
		.u.periph = {				\
			.clk_num   = _clk_num,		\
		},					\
	}

#define PERIPH_CLK_EX(_name, _dev, _con, _clk_num, _reg, _max, _inputs,	\
			_flags, _ops) 					\
	{						\
		.name      = _name,			\
		.lookup    = {				\
			.dev_id    = _dev,		\
			.con_id	   = _con,		\
		},					\
		.ops       = _ops,			\
		.reg       = _reg,			\
		.inputs    = _inputs,			\
		.flags     = _flags,			\
		.max_rate  = _max,			\
		.u.periph = {				\
			.clk_num   = _clk_num,		\
		},					\
	}

#define SHARED_CLK(_name, _dev, _con, _parent, _id, _div, _mode)\
	{						\
		.name      = _name,			\
		.lookup    = {				\
			.dev_id    = _dev,		\
			.con_id    = _con,		\
		},					\
		.ops = &tegra_clk_shared_bus_user_ops,	\
		.parent = _parent,			\
		.u.shared_bus_user = {			\
			.client_id = _id,		\
			.client_div = _div,		\
			.mode = _mode,			\
		},					\
	}
struct clk tegra_list_clks[] = {
	PERIPH_CLK("apbdma",	"tegra-dma",		NULL,	34,	0,	26000000,  mux_clk_m,			0),
	PERIPH_CLK("rtc",	"rtc-tegra",		NULL,	4,	0,	32768,     mux_clk_32k,			PERIPH_NO_RESET | PERIPH_ON_APB),
	PERIPH_CLK("kbc",	"tegra-kbc",		NULL,	36,	0,	32768,	   mux_clk_32k, 		PERIPH_NO_RESET | PERIPH_ON_APB),
	PERIPH_CLK("timer",	"timer",		NULL,	5,	0,	26000000,  mux_clk_m,			0),
	PERIPH_CLK("kfuse",	"kfuse-tegra",		NULL,	40,	0,	26000000,  mux_clk_m,			PERIPH_ON_APB),
	PERIPH_CLK("fuse",	"fuse-tegra",		"fuse",	39,	0,	26000000,  mux_clk_m,			PERIPH_ON_APB),
	PERIPH_CLK("fuse_burn",	"fuse-tegra",		"fuse_burn",	39,	0,	26000000,  mux_clk_m,		PERIPH_ON_APB),
	PERIPH_CLK("apbif",	"tegra30-ahub",		"apbif", 107,	0,	26000000,  mux_clk_m,			PERIPH_ON_APB),
	PERIPH_CLK("i2s0",	"tegra30-i2s.0",	"i2s",	30,	0x1d8,	26000000,  mux_pllaout0_audio0_2x_pllp_clkm,	MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("i2s1",	"tegra30-i2s.1",	"i2s",	11,	0x100,	26000000,  mux_pllaout0_audio1_2x_pllp_clkm,	MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("i2s2",	"tegra30-i2s.2",	"i2s",	18,	0x104,	26000000,  mux_pllaout0_audio2_2x_pllp_clkm,	MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("i2s3",	"tegra30-i2s.3",	"i2s",	101,	0x3bc,	26000000,  mux_pllaout0_audio3_2x_pllp_clkm,	MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("i2s4",	"tegra30-i2s.4",	"i2s",	102,	0x3c0,	26000000,  mux_pllaout0_audio4_2x_pllp_clkm,	MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("spdif_out",	"tegra30-spdif",	"spdif_out",	10,	0x108,	26000000, mux_pllaout0_audio_2x_pllp_clkm,	MUX | DIV_U71 | PERIPH_ON_APB),
#ifdef CONFIG_TEGRA_PCI
	PERIPH_CLK("spdif_in",  "tegra30-spdif",        "spdif_in",     10,     0x10c,  408000000, mux_pllp_pllc_pllm,          MUX | DIV_U71 | PERIPH_ON_APB),
#else
	PERIPH_CLK("spdif_in",	"tegra30-spdif",	"spdif_in",	10,	0x10c,	100000000, mux_pllp_pllc_pllm,		MUX | DIV_U71 | PERIPH_ON_APB),
#endif
	PERIPH_CLK("pwm",	"pwm",			NULL,	17,	0x110,	408000000, mux_pllp_pllc_clk32_clkm,	MUX | MUX_PWM | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("d_audio",	"tegra30-ahub",		"d_audio", 106,	0x3d0,	48000000,  mux_plla_pllc_pllp_clkm,	MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("dam0",	"tegra30-dam.0",	NULL,   108,	0x3d8,	48000000,  mux_plla_pllc_pllp_clkm,	MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("dam1",	"tegra30-dam.1",	NULL,   109,	0x3dc,	48000000,  mux_plla_pllc_pllp_clkm,	MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("dam2",	"tegra30-dam.2",	NULL,   110,	0x3e0,	48000000,  mux_plla_pllc_pllp_clkm,	MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("hda",	"tegra30-hda",		"hda",   125,	0x428,	108000000, mux_pllp_pllc_pllm_clkm,	MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("hda2codec_2x",	"tegra30-hda",	"hda2codec",   111,	0x3e4,	48000000,  mux_pllp_pllc_pllm_clkm,	MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("hda2hdmi",	"tegra30-hda",		"hda2hdmi",	128,	0,	48000000,  mux_clk_m,			PERIPH_ON_APB),
	PERIPH_CLK("sbc1",	"spi_tegra.0",		"spi",	41,	0x134,	160000000, mux_pllp_pllc_pllm_clkm,	MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("sbc2",	"spi_tegra.1",		"spi",	44,	0x118,	160000000, mux_pllp_pllc_pllm_clkm,	MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("sbc3",	"spi_tegra.2",		"spi",	46,	0x11c,	160000000, mux_pllp_pllc_pllm_clkm,	MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("sbc4",	"spi_tegra.3",		"spi",	68,	0x1b4,	160000000, mux_pllp_pllc_pllm_clkm,	MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("sbc5",	"spi_tegra.4",		"spi",	104,	0x3c8,	160000000, mux_pllp_pllc_pllm_clkm,	MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("sbc6",	"spi_tegra.5",		"spi",	105,	0x3cc,	160000000, mux_pllp_pllc_pllm_clkm,	MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("sata_oob",	"tegra_sata_oob",	NULL,	123,	0x420,	216000000, mux_pllp_pllc_pllm_clkm,	MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("sata",	"tegra_sata",		NULL,	124,	0x424,	216000000, mux_pllp_pllc_pllm_clkm,	MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("sata_cold",	"tegra_sata_cold",	NULL,	129,	0,	48000000,  mux_clk_m,			PERIPH_ON_APB),
	PERIPH_CLK_EX("ndflash","tegra_nand",		NULL,	13,	0x160,	240000000, mux_pllp_pllc_pllm_clkm,	MUX | DIV_U71 | PERIPH_ON_APB,	&tegra_nand_clk_ops),
	PERIPH_CLK("ndspeed",	"tegra_nand_speed",	NULL,	80,	0x3f8,	240000000, mux_pllp_pllc_pllm_clkm,	MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("vfir",	"vfir",			NULL,	7,	0x168,	72000000,  mux_pllp_pllc_pllm_clkm,	MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("sdmmc1",	"sdhci-tegra.0",	NULL,	14,	0x150,	208000000, mux_pllp_pllc_pllm_clkm,	MUX | DIV_U71), /* scales with voltage */
	PERIPH_CLK("sdmmc2",	"sdhci-tegra.1",	NULL,	9,	0x154,	50000000, mux_pllp_pllc_pllm_clkm,	MUX | DIV_U71), /* scales with voltage */
	PERIPH_CLK("sdmmc3",	"sdhci-tegra.2",	NULL,	69,	0x1bc,	208000000, mux_pllp_pllc_pllm_clkm,	MUX | DIV_U71), /* scales with voltage */
	PERIPH_CLK("sdmmc4",	"sdhci-tegra.3",	NULL,	15,	0x164,	102000000, mux_pllp_pllc_pllm_clkm,	MUX | DIV_U71), /* scales with voltage */
	PERIPH_CLK("vcp",	"tegra-avp",		"vcp",	29,	0,	250000000, mux_clk_m, 			0),
	PERIPH_CLK("bsea",	"tegra-avp",		"bsea",	62,	0,	250000000, mux_clk_m, 			0),
	PERIPH_CLK("bsev",	"tegra-aes",		"bsev",	63,	0,	250000000, mux_clk_m, 			0),
	PERIPH_CLK("cec",	"tegra_cec",		NULL,	136,	0,	26000000,  mux_clk_m,			PERIPH_ON_APB),
	PERIPH_CLK("vde",	"vde",			NULL,	61,	0x1c8,	600000000, mux_pllp_pllc_pllm_clkm,	MUX | DIV_U71 | DIV_U71_INT),
#ifdef CONFIG_TEGRA_PCI
	PERIPH_CLK("csite",     "csite",                NULL,   73,     0x1d4,  408000000, mux_pllp_pllc_pllm_clkm,     MUX | DIV_U71 | PERIPH_ON_APB), /* max rate ??? */
#else
	PERIPH_CLK("csite",	"csite",		NULL,	73,	0x1d4,	144000000, mux_pllp_pllc_pllm_clkm,	MUX | DIV_U71 | PERIPH_ON_APB), /* max rate ??? */
#endif
	PERIPH_CLK("la",	"la",			NULL,	76,	0x1f8,	26000000,  mux_pllp_pllc_pllm_clkm,	MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("owr",	"tegra_w1",		NULL,	71,	0x1cc,	26000000,  mux_pllp_pllc_pllm_clkm,	MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("nor",	"tegra-nor",		NULL,	42,	0x1d0,	127000000, mux_pllp_pllc_pllm_clkm,	MUX | DIV_U71), /* requires min voltage */
	PERIPH_CLK("mipi",	"mipi",			NULL,	50,	0x174,	60000000,  mux_pllp_pllc_pllm_clkm,	MUX | DIV_U71 | PERIPH_ON_APB), /* scales with voltage */
	PERIPH_CLK("i2c1",	"tegra-i2c.0",		"div-clk",	12,	0x124,	26000000,  mux_pllp_clkm,	MUX | DIV_U16 | PERIPH_ON_APB),
	PERIPH_CLK("i2c2",	"tegra-i2c.1",		"div-clk",	54,	0x198,	26000000,  mux_pllp_clkm,	MUX | DIV_U16 | PERIPH_ON_APB),
	PERIPH_CLK("i2c3",	"tegra-i2c.2",		"div-clk",	67,	0x1b8,	26000000,  mux_pllp_clkm,	MUX | DIV_U16 | PERIPH_ON_APB),
	PERIPH_CLK("i2c4",	"tegra-i2c.3",		"div-clk",	103,	0x3c4,	26000000,  mux_pllp_clkm,	MUX | DIV_U16 | PERIPH_ON_APB),
	PERIPH_CLK("i2c5",	"tegra-i2c.4",		"div-clk",	47,	0x128,	26000000,  mux_pllp_clkm,	MUX | DIV_U16 | PERIPH_ON_APB),
	PERIPH_CLK("i2c1-fast",	"tegra-i2c.0",		"fast-clk",	0,	0,	108000000, mux_pllp_out3,	PERIPH_NO_ENB),
	PERIPH_CLK("i2c2-fast",	"tegra-i2c.1",		"fast-clk",	0,	0,	108000000, mux_pllp_out3,	PERIPH_NO_ENB),
	PERIPH_CLK("i2c3-fast",	"tegra-i2c.2",		"fast-clk",	0,	0,	108000000, mux_pllp_out3,	PERIPH_NO_ENB),
	PERIPH_CLK("i2c4-fast",	"tegra-i2c.3",		"fast-clk",	0,	0,	108000000, mux_pllp_out3,	PERIPH_NO_ENB),
	PERIPH_CLK("i2c5-fast",	"tegra-i2c.4",		"fast-clk",	0,	0,	108000000, mux_pllp_out3,	PERIPH_NO_ENB),
	PERIPH_CLK("uarta",	"tegra_uart.0",		NULL,	6,	0x178,	900000000, mux_pllp_pllc_pllm_clkm,	MUX | DIV_U151 | DIV_U151_UART | PERIPH_ON_APB),
	PERIPH_CLK("uartb",	"tegra_uart.1",		NULL,	7,	0x17c,	900000000, mux_pllp_pllc_pllm_clkm,	MUX | DIV_U151 | DIV_U151_UART | PERIPH_ON_APB),
	PERIPH_CLK("uartc",	"tegra_uart.2",		NULL,	55,	0x1a0,	900000000, mux_pllp_pllc_pllm_clkm,	MUX | DIV_U151 | DIV_U151_UART | PERIPH_ON_APB),
	PERIPH_CLK("uartd",	"tegra_uart.3",		NULL,	65,	0x1c0,	900000000, mux_pllp_pllc_pllm_clkm,	MUX | DIV_U151 | DIV_U151_UART | PERIPH_ON_APB),
	PERIPH_CLK("uarte",	"tegra_uart.4",		NULL,	66,	0x1c4,	900000000, mux_pllp_pllc_pllm_clkm,	MUX | DIV_U151 | DIV_U151_UART | PERIPH_ON_APB),
	PERIPH_CLK("uarta_dbg",	"serial8250.0",		"uarta", 6,	0x178,	900000000, mux_pllp_clkm,		MUX | DIV_U151 | DIV_U151_UART | PERIPH_ON_APB),
	PERIPH_CLK("uartb_dbg",	"serial8250.0",		"uartb", 7,	0x17c,	900000000, mux_pllp_clkm,		MUX | DIV_U151 | DIV_U151_UART | PERIPH_ON_APB),
	PERIPH_CLK("uartc_dbg",	"serial8250.0",		"uartc", 55,	0x1a0,	900000000, mux_pllp_clkm,		MUX | DIV_U151 | DIV_U151_UART | PERIPH_ON_APB),
	PERIPH_CLK("uartd_dbg",	"serial8250.0",		"uartd", 65,	0x1c0,	900000000, mux_pllp_clkm,		MUX | DIV_U151 | DIV_U151_UART | PERIPH_ON_APB),
	PERIPH_CLK("uarte_dbg",	"serial8250.0",		"uarte", 66,	0x1c4,	900000000, mux_pllp_clkm,		MUX | DIV_U151 | DIV_U151_UART | PERIPH_ON_APB),
	PERIPH_CLK_EX("vi",	"vi",			"vi",	20,	0x148,	470000000, mux_pllm_pllc_pllp_plla,	MUX | DIV_U71 | DIV_U71_INT,	&tegra_vi_clk_ops),
#ifdef CONFIG_TEGRA_PCI
	PERIPH_CLK("vi_sensor", "vi",                   "vi_sensor",    20,     0x1a8,  750000000, mux_pllm_pllc_pllp_plla,     MUX | DIV_U71 | PERIPH_NO_RESET),
#else
	PERIPH_CLK("vi_sensor",	"vi",			"vi_sensor",	20,	0x1a8,	150000000, mux_pllm_pllc_pllp_plla,	MUX | DIV_U71 | PERIPH_NO_RESET),
#endif
	PERIPH_CLK("3d",	"3d",			NULL,	24,	0x158,	600000000, mux_pllm_pllc_pllp_plla,	MUX | DIV_U71 | DIV_U71_INT | DIV_U71_IDLE | PERIPH_MANUAL_RESET),
	PERIPH_CLK("3d2",       "3d2",			NULL,	98,	0x3b0,	600000000, mux_pllm_pllc_pllp_plla,	MUX | DIV_U71 | DIV_U71_INT | DIV_U71_IDLE | PERIPH_MANUAL_RESET),
	PERIPH_CLK("2d",	"2d",			NULL,	21,	0x15c,	600000000, mux_pllm_pllc_pllp_plla,	MUX | DIV_U71 | DIV_U71_INT | DIV_U71_IDLE),
	PERIPH_CLK("epp",	"epp",			NULL,	19,	0x16c,	600000000, mux_pllm_pllc_pllp_plla,	MUX | DIV_U71 | DIV_U71_INT),
	PERIPH_CLK("mpe",	"mpe",			NULL,	60,	0x170,	600000000, mux_pllm_pllc_pllp_plla,	MUX | DIV_U71 | DIV_U71_INT),
	PERIPH_CLK("host1x",	"host1x",		NULL,	28,	0x180,	300000000, mux_pllm_pllc_pllp_plla,	MUX | DIV_U71 | DIV_U71_INT),
	PERIPH_CLK("cve",	"cve",			NULL,	49,	0x140,	250000000, mux_pllp_plld_pllc_clkm,	MUX | DIV_U71), /* requires min voltage */
	PERIPH_CLK("tvo",	"tvo",			NULL,	49,	0x188,	250000000, mux_pllp_plld_pllc_clkm,	MUX | DIV_U71), /* requires min voltage */
	PERIPH_CLK_EX("dtv",	"dtv",			NULL,	79,	0x1dc,	250000000, mux_clk_m,			PERIPH_ON_APB,	&tegra_dtv_clk_ops),
	PERIPH_CLK("hdmi",	"hdmi",			NULL,	51,	0x18c,	148500000, mux_pllp_pllm_plld_plla_pllc_plld2_clkm,	MUX | MUX8 | DIV_U71),
	PERIPH_CLK("tvdac",	"tvdac",		NULL,	53,	0x194,	220000000, mux_pllp_plld_pllc_clkm,	MUX | DIV_U71), /* requires min voltage */
	PERIPH_CLK("disp1",	"tegradc.0",		NULL,	27,	0x138,	600000000, mux_pllp_pllm_plld_plla_pllc_plld2_clkm,	MUX | MUX8),
	PERIPH_CLK("disp2",	"tegradc.1",		NULL,	26,	0x13c,	600000000, mux_pllp_pllm_plld_plla_pllc_plld2_clkm,	MUX | MUX8),
	PERIPH_CLK("usbd",	"tegra-udc.0",		NULL,	22,	0,	480000000, mux_clk_m,			0), /* requires min voltage */
	PERIPH_CLK("usb2",	"tegra-ehci.1",		NULL,	58,	0,	480000000, mux_clk_m,			0), /* requires min voltage */
	PERIPH_CLK("usb3",	"tegra-ehci.2",		NULL,	59,	0,	480000000, mux_clk_m,			0), /* requires min voltage */
	PERIPH_CLK("dsia",	"tegradc.0",		"dsia",	48,	0,	500000000, mux_plld_out0,		0),
	PERIPH_CLK_EX("dsib",	"tegradc.1",		"dsib",	82,	0xd0,	500000000, mux_plld_out0_plld2_out0,	MUX | PLLD,	&tegra_dsib_clk_ops),
	PERIPH_CLK("dsi1-fixed", "tegradc.0",		"dsi-fixed",	0,	0,	108000000, mux_pllp_out3,	PERIPH_NO_ENB),
	PERIPH_CLK("dsi2-fixed", "tegradc.1",		"dsi-fixed",	0,	0,	108000000, mux_pllp_out3,	PERIPH_NO_ENB),
	PERIPH_CLK("csi",	"vi",			"csi",	52,	0,	102000000, mux_pllp_out3,		0),
	PERIPH_CLK("isp",	"vi",			"isp",	23,	0,	150000000, mux_clk_m,			0), /* same frequency as VI */
	PERIPH_CLK("csus",	"vi",			"csus",	92,	0,	150000000, mux_clk_m,			PERIPH_NO_RESET),

	PERIPH_CLK("tsensor",	"tegra-tsensor",	NULL,	100,	0x3b8,	216000000, mux_pllp_pllc_clkm_clk32,	MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("actmon",	"actmon",		NULL,	119,	0x3e8,	216000000, mux_pllp_pllc_clk32_clkm,	MUX | DIV_U71),
	PERIPH_CLK("extern1",	"extern1",		NULL,	120,	0x3ec,	216000000, mux_plla_clk32_pllp_clkm_plle,	MUX | MUX8 | DIV_U71),
	PERIPH_CLK("extern2",	"extern2",		NULL,	121,	0x3f0,	216000000, mux_plla_clk32_pllp_clkm_plle,	MUX | MUX8 | DIV_U71),
	PERIPH_CLK("extern3",	"extern3",		NULL,	122,	0x3f4,	216000000, mux_plla_clk32_pllp_clkm_plle,	MUX | MUX8 | DIV_U71),
	PERIPH_CLK("i2cslow",	"i2cslow",		NULL,	81,	0x3fc,	26000000,  mux_pllp_pllc_clk32_clkm,	MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("pcie",	"tegra-pcie",		"pcie",	70,	0,	250000000, mux_clk_m, 			0),
	PERIPH_CLK("afi",	"tegra-pcie",		"afi",	72,	0,	250000000, mux_clk_m, 			0),
	PERIPH_CLK("se",	"se",			NULL,	127,	0x42c,	625000000, mux_pllp_pllc_pllm_clkm,	MUX | DIV_U71 | DIV_U71_INT | PERIPH_ON_APB),
#ifdef CONFIG_TEGRA_PCI
	PERIPH_CLK("mselect",	"mselect",		NULL,	99,	0x3b4,	204000000, mux_pllp_clkm,		MUX | DIV_U71),
#else
	PERIPH_CLK("mselect",	"mselect",		NULL,	99,	0x3b4,	108000000, mux_pllp_clkm,		MUX | DIV_U71),
#endif
	SHARED_CLK("avp.sclk",	"tegra-avp",		"sclk",	&tegra_clk_sbus_cmplx, NULL, 0, 0),
	SHARED_CLK("bsea.sclk",	"tegra-aes",		"sclk",	&tegra_clk_sbus_cmplx, NULL, 0, 0),
	SHARED_CLK("usbd.sclk",	"tegra-udc.0",		"sclk",	&tegra_clk_sbus_cmplx, NULL, 0, 0),
	SHARED_CLK("usb1.sclk",	"tegra-ehci.0",		"sclk",	&tegra_clk_sbus_cmplx, NULL, 0, 0),
	SHARED_CLK("usb2.sclk",	"tegra-ehci.1",		"sclk",	&tegra_clk_sbus_cmplx, NULL, 0, 0),
	SHARED_CLK("usb3.sclk",	"tegra-ehci.2",		"sclk",	&tegra_clk_sbus_cmplx, NULL, 0, 0),
	SHARED_CLK("wake.sclk",	"wake_sclk",		"sclk",	&tegra_clk_sbus_cmplx, NULL, 0, 0),
	SHARED_CLK("mon.avp",	"tegra_actmon",		"avp",	&tegra_clk_sbus_cmplx, NULL, 0, 0),
	SHARED_CLK("cap.sclk",	"cap_sclk",		NULL,	&tegra_clk_sbus_cmplx, NULL, 0, SHARED_CEILING),
	SHARED_CLK("cap.throttle.sclk", "cap_throttle",	NULL,	&tegra_clk_sbus_cmplx, NULL, 0, SHARED_CEILING),
	SHARED_CLK("floor.sclk", "floor_sclk",		NULL,	&tegra_clk_sbus_cmplx, NULL, 0, 0),
	SHARED_CLK("sbc1.sclk", "spi_tegra.0",		"sclk", &tegra_clk_sbus_cmplx, NULL, 0, 0),
	SHARED_CLK("sbc2.sclk", "spi_tegra.1",		"sclk", &tegra_clk_sbus_cmplx, NULL, 0, 0),
	SHARED_CLK("sbc3.sclk", "spi_tegra.2",		"sclk", &tegra_clk_sbus_cmplx, NULL, 0, 0),
	SHARED_CLK("sbc4.sclk", "spi_tegra.3",		"sclk", &tegra_clk_sbus_cmplx, NULL, 0, 0),
	SHARED_CLK("sbc5.sclk", "spi_tegra.4",		"sclk", &tegra_clk_sbus_cmplx, NULL, 0, 0),
	SHARED_CLK("sbc6.sclk", "spi_tegra.5",		"sclk", &tegra_clk_sbus_cmplx, NULL, 0, 0),

	SHARED_CLK("avp.emc",	"tegra-avp",		"emc",	&tegra_clk_emc, NULL, 0, 0),
	SHARED_CLK("mon_cpu.emc",	"tegra_mon",		"cpu_emc",
					&tegra_clk_emc, NULL, 0, 0),
	SHARED_CLK("disp1.emc",	"tegradc.0",		"emc",	&tegra_clk_emc, NULL, 0, SHARED_BW),
	SHARED_CLK("disp2.emc",	"tegradc.1",		"emc",	&tegra_clk_emc, NULL, 0, SHARED_BW),
	SHARED_CLK("hdmi.emc",	"hdmi",			"emc",	&tegra_clk_emc, NULL, 0, 0),
	SHARED_CLK("usbd.emc",	"tegra-udc.0",		"emc",	&tegra_clk_emc, NULL, 0, 0),
	SHARED_CLK("usb1.emc",	"tegra-ehci.0",		"emc",	&tegra_clk_emc, NULL, 0, 0),
	SHARED_CLK("usb2.emc",	"tegra-ehci.1",		"emc",	&tegra_clk_emc, NULL, 0, 0),
	SHARED_CLK("usb3.emc",	"tegra-ehci.2",		"emc",	&tegra_clk_emc, NULL, 0, 0),
	SHARED_CLK("mon.emc",	"tegra_actmon",		"emc",	&tegra_clk_emc, NULL, 0, 0),
	SHARED_CLK("cap.emc",	"cap.emc",		NULL,	&tegra_clk_emc, NULL, 0, SHARED_CEILING),
	SHARED_CLK("cap.throttle.emc", "cap_throttle",	NULL,	&tegra_clk_emc, NULL, 0, SHARED_CEILING),
	SHARED_CLK("3d.emc",	"tegra_gr3d",		"emc",	&tegra_clk_emc, NULL, 0, 0),
	SHARED_CLK("2d.emc",	"tegra_gr2d",		"emc",	&tegra_clk_emc, NULL, 0, 0),
	SHARED_CLK("mpe.emc",	"tegra_mpe",		"emc",	&tegra_clk_emc, NULL, 0, SHARED_BW),
	SHARED_CLK("camera.emc", "vi",			"emc",	&tegra_clk_emc, NULL, 0, SHARED_BW),
	SHARED_CLK("sdmmc4.emc", "sdhci-tegra.3",	"emc",	&tegra_clk_emc, NULL, 0, 0),
	SHARED_CLK("floor.emc",	"floor.emc",		NULL,	&tegra_clk_emc, NULL, 0, 0),

	SHARED_CLK("host1x.cbus", "tegra_host1x",	"host1x", &tegra_clk_cbus, "host1x", 2, SHARED_AUTO),
	SHARED_CLK("3d.cbus",	"tegra_gr3d",		"gr3d",	&tegra_clk_cbus, "3d",  0, 0),
	SHARED_CLK("3d2.cbus",	"tegra_gr3d",		"gr3d2", &tegra_clk_cbus, "3d2", 0, 0),
	SHARED_CLK("2d.cbus",	"tegra_gr2d",		"gr2d",	&tegra_clk_cbus, "2d",  0, 0),
	SHARED_CLK("epp.cbus",	"tegra_gr2d",		"epp",	&tegra_clk_cbus, "epp", 0, 0),
	SHARED_CLK("mpe.cbus",	"tegra_mpe",		"mpe",	&tegra_clk_cbus, "mpe", 0, 0),
	SHARED_CLK("vde.cbus",	"tegra-avp",		"vde",	&tegra_clk_cbus, "vde", 0, 0),
#ifdef CONFIG_TEGRA_SE_ON_CBUS
	SHARED_CLK("se.cbus",	"tegra-se",		NULL,	&tegra_clk_cbus, "se",  0, 0),
#endif
	SHARED_CLK("cap.cbus",	"cap.cbus",		NULL,	&tegra_clk_cbus, NULL,  0, SHARED_CEILING),
	SHARED_CLK("cap.throttle.cbus",	"cap_throttle",	NULL,	&tegra_clk_cbus, NULL,  0, SHARED_CEILING),
	SHARED_CLK("cap.profile.cbus", "profile.cbus",	NULL,	&tegra_clk_cbus, NULL,  0, SHARED_CEILING),
	SHARED_CLK("floor.cbus", "floor.cbus",		NULL,	&tegra_clk_cbus, NULL,  0, 0),
};

#define CLK_DUPLICATE(_name, _dev, _con)		\
	{						\
		.name	= _name,			\
		.lookup	= {				\
			.dev_id	= _dev,			\
			.con_id		= _con,		\
		},					\
	}

/* Some clocks may be used by different drivers depending on the board
 * configuration.  List those here to register them twice in the clock lookup
 * table under two names.
 */
struct clk_duplicate tegra_clk_duplicates[] = {
	CLK_DUPLICATE("usbd", "utmip-pad", NULL),
	CLK_DUPLICATE("usbd", "tegra-ehci.0", NULL),
	CLK_DUPLICATE("usbd", "tegra-otg", NULL),
	CLK_DUPLICATE("hdmi", "tegradc.0", "hdmi"),
	CLK_DUPLICATE("hdmi", "tegradc.1", "hdmi"),
	CLK_DUPLICATE("dsib", "tegradc.0", "dsib"),
	CLK_DUPLICATE("dsia", "tegradc.1", "dsia"),
	CLK_DUPLICATE("pwm", "tegra_pwm.0", NULL),
	CLK_DUPLICATE("pwm", "tegra_pwm.1", NULL),
	CLK_DUPLICATE("pwm", "tegra_pwm.2", NULL),
	CLK_DUPLICATE("pwm", "tegra_pwm.3", NULL),
	CLK_DUPLICATE("cop", "tegra-avp", "cop"),
	CLK_DUPLICATE("bsev", "tegra-avp", "bsev"),
	CLK_DUPLICATE("cop", "nvavp", "cop"),
	CLK_DUPLICATE("bsev", "nvavp", "bsev"),
	CLK_DUPLICATE("vde", "tegra-aes", "vde"),
	CLK_DUPLICATE("bsea", "tegra-aes", "bsea"),
	CLK_DUPLICATE("bsea", "nvavp", "bsea"),
	CLK_DUPLICATE("cml1", "tegra_sata_cml", NULL),
	CLK_DUPLICATE("cml0", "tegra_pcie", "cml"),
	CLK_DUPLICATE("pciex", "tegra_pcie", "pciex"),
	CLK_DUPLICATE("i2c1", "tegra-i2c-slave.0", NULL),
	CLK_DUPLICATE("i2c2", "tegra-i2c-slave.1", NULL),
	CLK_DUPLICATE("i2c3", "tegra-i2c-slave.2", NULL),
	CLK_DUPLICATE("i2c4", "tegra-i2c-slave.3", NULL),
	CLK_DUPLICATE("i2c5", "tegra-i2c-slave.4", NULL),
	CLK_DUPLICATE("sbc1", "spi_slave_tegra.0", NULL),
	CLK_DUPLICATE("sbc2", "spi_slave_tegra.1", NULL),
	CLK_DUPLICATE("sbc3", "spi_slave_tegra.2", NULL),
	CLK_DUPLICATE("sbc4", "spi_slave_tegra.3", NULL),
	CLK_DUPLICATE("sbc5", "spi_slave_tegra.4", NULL),
	CLK_DUPLICATE("sbc6", "spi_slave_tegra.5", NULL),
	CLK_DUPLICATE("twd", "smp_twd", NULL),
	CLK_DUPLICATE("vcp", "nvavp", "vcp"),
	CLK_DUPLICATE("avp.sclk", "nvavp", "sclk"),
	CLK_DUPLICATE("avp.emc", "nvavp", "emc"),
	CLK_DUPLICATE("vde.cbus", "nvavp", "vde"),
	CLK_DUPLICATE("epp.cbus", "tegra_isp", "epp"),
};

struct clk *tegra_ptr_clks[] = {
	&tegra_clk_32k,
	&tegra_clk_m,
	&tegra_clk_m_div2,
	&tegra_clk_m_div4,
	&tegra_pll_ref,
	&tegra_pll_m,
	&tegra_pll_m_out1,
	&tegra_pll_c,
	&tegra_pll_c_out1,
	&tegra_pll_p,
	&tegra_pll_p_out1,
	&tegra_pll_p_out2,
	&tegra_pll_p_out3,
	&tegra_pll_p_out4,
	&tegra_pll_a,
	&tegra_pll_a_out0,
	&tegra_pll_d,
	&tegra_pll_d_out0,
	&tegra_pll_d2,
	&tegra_pll_d2_out0,
	&tegra_pll_u,
	&tegra_pll_x,
	&tegra_pll_x_out0,
	&tegra_pll_e,
	&tegra_cml0_clk,
	&tegra_cml1_clk,
	&tegra_pciex_clk,
	&tegra_clk_cclk_g,
	&tegra_clk_cclk_lp,
	&tegra_clk_sclk,
	&tegra_clk_hclk,
	&tegra_clk_pclk,
	&tegra_clk_virtual_cpu_g,
	&tegra_clk_virtual_cpu_lp,
	&tegra_clk_cpu_cmplx,
	&tegra_clk_blink,
	&tegra_clk_cop,
	&tegra_clk_sbus_cmplx,
	&tegra_clk_emc,
	&tegra3_clk_twd,
	&tegra_clk_emc_bridge,
	&tegra_clk_cbus,
};

static int tegra3_emc_relock_set_rate(struct clk *emc, unsigned long old_rate,
	unsigned long new_rate, unsigned long new_pll_rate)
{
	int ret;

	struct clk *sbus = &tegra_clk_sbus_cmplx;
	struct clk *cbus = &tegra_clk_cbus;
	struct clk *pll_m = &tegra_pll_m;
	unsigned long backup_rate = emc->shared_bus_backup.bus_rate;
	unsigned long flags;

	bool on_pllm = emc->parent == pll_m;

	/*
	 * Relock procedure pre-conditions:
	 * - LPDDR2 only
	 * - EMC clock is enabled, and EMC backup rate is found in DFS table
	 * - All 3 shared buses: emc, sbus, cbus can sleep
	 */
	if ((tegra_emc_get_dram_type() != DRAM_TYPE_LPDDR2) || !emc->refcnt ||
	    !backup_rate || (cbus->parent != emc->shared_bus_backup.input) ||
	    !clk_cansleep(emc) || !clk_cansleep(cbus) || !clk_cansleep(sbus))
		return -ENOSYS;

	/* Move sbus from PLLM by setting it at low rate threshold level */
	clk_lock_save(sbus, &flags);
	if (clk_get_rate_locked(sbus) > sbus->u.system.threshold) {
		ret = clk_set_rate_locked(sbus, sbus->u.system.threshold);
		if (ret)
			goto _sbus_out;
	}

	/* If PLLM is current EMC parent set cbus to backup rate, and move EMC
	   to backup PLLC */
	if (on_pllm) {
		clk_lock_save(cbus, &flags);
		tegra_clk_prepare_enable(cbus->parent);
		ret = clk_set_rate_locked(cbus, backup_rate);
		if (ret) {
			tegra_clk_disable_unprepare(cbus->parent);
			goto _cbus_out;
		}

		ret = tegra_emc_backup(backup_rate);
		if (ret) {
			tegra_clk_disable_unprepare(cbus->parent);
			goto _cbus_out;
		}
		tegra_clk_disable_unprepare(emc->parent);
		clk_reparent(emc, cbus->parent);
	}

	/*
	 * Re-lock PLLM and switch EMC to it; relocking error indicates that
	 * PLLM has some other than EMC or sbus client. In this case PLLM has
	 * not been changed, and we still can safely switch back. Recursive
	 * tegra3_emc_clk_set_rate() call below will be resolved, since PLLM
	 * is now matching target rate.
	 */
	ret = clk_set_rate(pll_m, new_pll_rate);
	if (ret) {
		if (on_pllm)
			tegra3_emc_clk_set_rate(emc, old_rate);
	} else
		ret = tegra3_emc_clk_set_rate(emc, new_rate);

_cbus_out:
	if (on_pllm) {
		tegra3_clk_shared_bus_update(cbus);
		clk_unlock_restore(cbus, &flags);
	}

_sbus_out:
	tegra3_clk_shared_bus_update(sbus);
	clk_unlock_restore(sbus, &flags);

	return ret;
}

/*
 * Backup rate targets for each CPU mode is selected below Fmax(Vmin), and
 * high enough to avoid voltage droop when CPU clock is switched between
 * backup and main clock sources. Actual backup rates will be rounded based
 * on backup source fixed frequency. Maximum stay-on-backup rate will be set
 * as a minimum of G and LP backup rates to be supported in both modes.
 *
 * Sbus threshold must be exact factor of pll_p rate.
 */
#define CPU_G_BACKUP_RATE_TARGET	440000000
#define CPU_LP_BACKUP_RATE_TARGET	220000000

static void tegra3_pllp_init_dependencies(unsigned long pllp_rate)
{
	u32 div;
	unsigned long backup_rate;

	switch (pllp_rate) {
	case 216000000:
		tegra_pll_p_out1.u.pll_div.default_rate = 28800000;
		tegra_pll_p_out3.u.pll_div.default_rate = 72000000;
		tegra_clk_sbus_cmplx.u.system.threshold = 108000000;
		break;
	case 408000000:
		tegra_pll_p_out1.u.pll_div.default_rate = 9600000;
		tegra_pll_p_out3.u.pll_div.default_rate = 102000000;
		tegra_clk_sbus_cmplx.u.system.threshold = 204000000;
		break;
	case 204000000:
		tegra_pll_p_out1.u.pll_div.default_rate = 4800000;
		tegra_pll_p_out3.u.pll_div.default_rate = 102000000;
		tegra_clk_sbus_cmplx.u.system.threshold = 204000000;
		break;
	default:
		pr_err("tegra: PLLP rate: %lu is not supported\n", pllp_rate);
		BUG();
	}
	pr_info("tegra: PLLP fixed rate: %lu\n", pllp_rate);

	div = DIV_ROUND_UP(pllp_rate, CPU_G_BACKUP_RATE_TARGET);
	backup_rate = pllp_rate / div;
	tegra_clk_cclk_g.u.cclk.div71 = 2 * div - 2;
	tegra_clk_virtual_cpu_g.u.cpu.backup_rate = backup_rate;
	cpu_stay_on_backup_max = backup_rate;

	div = DIV_ROUND_UP(pllp_rate, CPU_LP_BACKUP_RATE_TARGET);
	backup_rate = pllp_rate / div;
	tegra_clk_cclk_lp.u.cclk.div71 = 2 * div - 2;
	tegra_clk_virtual_cpu_lp.u.cpu.backup_rate = backup_rate;
	cpu_stay_on_backup_max = min(cpu_stay_on_backup_max, backup_rate);
}

bool tegra_clk_is_parent_allowed(struct clk *c, struct clk *p)
{
	if (c->flags & PERIPH_ON_CBUS)
		return p != &tegra_pll_m;
	else
		return p != &tegra_pll_c;

	return true;
}

static void tegra3_init_one_clock(struct clk *c)
{
	clk_init(c);
	INIT_LIST_HEAD(&c->shared_bus_list);
	if (!c->lookup.dev_id && !c->lookup.con_id)
		c->lookup.con_id = c->name;
	c->lookup.clk = c;
	clkdev_add(&c->lookup);
}

/*
 * Emergency throttle of G-CPU by setting G-super clock skipper underneath
 * clock framework, dvfs, and cpufreq driver s/w layers. Can be called in
 * ISR context for EDP events. When releasing throttle, LP-divider is cleared
 * just in case it was set as a result of save/restore operations across
 * cluster switch (should not happen)
 */
void tegra_edp_throttle_cpu_now(u8 factor)
{
	if (factor > 1) {
		if (!is_lp_cluster())
			tegra3_super_clk_skipper_update(
				&tegra_clk_cclk_g, 1, factor);
	} else if (factor == 0) {
		tegra3_super_clk_skipper_update(&tegra_clk_cclk_g, 0, 0);
		tegra3_super_clk_skipper_update(&tegra_clk_cclk_lp, 0, 0);
	}
}

#ifdef CONFIG_CPU_FREQ

/*
 * Frequency table index must be sequential starting at 0 and frequencies
 * must be ascending. Re-configurable PLLX is used as a source for rates
 * above 204MHz. Rates 204MHz and below are divided down from fixed frequency
 * PLLP that may run either at 408MHz or at 204MHz on Tegra3 silicon platforms
 * (on FPGA platform PLLP output is reported as 216MHz, but no respective
 * tables are provided, since there is no clock scaling on FPGA at all).
 */

static struct cpufreq_frequency_table freq_table_300MHz[] = {
	{ 0, 204000 },
	{ 1, 300000 },
	{ 2, CPUFREQ_TABLE_END },
};

static struct cpufreq_frequency_table freq_table_900MHz[] = {
	{ 0, 450000 },
	{ 1, 900000 },
	{ 2, CPUFREQ_TABLE_END },
};

static struct cpufreq_frequency_table freq_table_1p0GHz[] = {
	{ 0,  51000 },
	{ 1, 102000 },
	{ 2, 204000 },
	{ 3, 312000 },
	{ 4, 456000 },
	{ 5, 608000 },
	{ 6, 760000 },
	{ 7, 816000 },
	{ 8, 912000 },
	{ 9, 1000000 },
	{10, CPUFREQ_TABLE_END },
};

static struct cpufreq_frequency_table freq_table_1p3GHz[] = {
	{ 0,   51000 },
	{ 1,  102000 },
	{ 2,  204000 },
	{ 3,  340000 },
	{ 4,  475000 },
	{ 5,  640000 },
	{ 6,  760000 },
	{ 7,  860000 },
	{ 8, 1000000 },
	{ 9, 1100000 },
	{10, 1200000 },
	{11, 1300000 },
	{12, CPUFREQ_TABLE_END },
};

static struct cpufreq_frequency_table freq_table_1p4GHz[] = {
	{ 0,   51000 },
	{ 1,  102000 },
	{ 2,  204000 },
	{ 3,  370000 },
	{ 4,  475000 },
	{ 5,  620000 },
	{ 6,  760000 },
	{ 7,  860000 },
	{ 8, 1000000 },
	{ 9, 1100000 },
	{10, 1200000 },
	{11, 1300000 },
	{12, 1400000 },
	{13, CPUFREQ_TABLE_END },
};

static struct cpufreq_frequency_table freq_table_1p5GHz[] = {
	{ 0,   51000 },
	{ 1,  102000 },
	{ 2,  204000 },
	{ 3,  340000 },
	{ 4,  475000 },
	{ 5,  640000 },
	{ 6,  760000 },
	{ 7,  860000 },
	{ 8, 1000000 },
	{ 9, 1100000 },
	{10, 1200000 },
	{11, 1300000 },
	{12, 1400000 },
	{13, 1500000 },
	{14, CPUFREQ_TABLE_END },
};

static struct cpufreq_frequency_table freq_table_1p7GHz[] = {
	{ 0,   51000 },
	{ 1,  102000 },
	{ 2,  204000 },
	{ 3,  370000 },
	{ 4,  475000 },
	{ 5,  620000 },
	{ 6,  760000 },
	{ 7,  910000 },
	{ 8, 1000000 },
	{ 9, 1150000 },
	{10, 1300000 },
	{11, 1400000 },
	{12, 1500000 },
	{13, 1600000 },
	{14, 1700000 },
	{15, CPUFREQ_TABLE_END },
};

static struct tegra_cpufreq_table_data cpufreq_tables[] = {
	{ freq_table_300MHz, 0,  1 },
	{ freq_table_900MHz, 1,  1 },
	{ freq_table_1p0GHz, 2,  8 },
	{ freq_table_1p3GHz, 2, 10 },
	{ freq_table_1p4GHz, 2, 11 },
	{ freq_table_1p5GHz, 2, 12 },
	{ freq_table_1p7GHz, 2, 12 },
};

static int clip_cpu_rate_limits(
	struct tegra_cpufreq_table_data *data,
	struct cpufreq_policy *policy,
	struct clk *cpu_clk_g,
	struct clk *cpu_clk_lp)
{
	int idx, ret;
	struct cpufreq_frequency_table *freq_table = data->freq_table;

	/* clip CPU G mode maximum frequency to table entry */
	ret = cpufreq_frequency_table_target(policy, freq_table,
		cpu_clk_g->max_rate / 1000, CPUFREQ_RELATION_H, &idx);
	if (ret) {
		pr_err("%s: G CPU max rate %lu outside of cpufreq table",
		       __func__, cpu_clk_g->max_rate);
		return ret;
	}
	cpu_clk_g->max_rate = freq_table[idx].frequency * 1000;
	if (cpu_clk_g->max_rate < cpu_clk_lp->max_rate) {
		pr_err("%s: G CPU max rate %lu is below LP CPU max rate %lu",
		       __func__, cpu_clk_g->max_rate, cpu_clk_lp->max_rate);
		return -EINVAL;
	}

	/* clip CPU LP mode maximum frequency to table entry, and
	   set CPU G mode minimum frequency one table step below */
	ret = cpufreq_frequency_table_target(policy, freq_table,
		cpu_clk_lp->max_rate / 1000, CPUFREQ_RELATION_H, &idx);
	if (ret || !idx) {
		pr_err("%s: LP CPU max rate %lu %s of cpufreq table", __func__,
		       cpu_clk_lp->max_rate, ret ? "outside" : "at the bottom");
		return ret;
	}
	cpu_clk_lp->max_rate = freq_table[idx].frequency * 1000;
	cpu_clk_g->min_rate = freq_table[idx-1].frequency * 1000;
	data->suspend_index = idx;
	return 0;
}

struct tegra_cpufreq_table_data *tegra_cpufreq_table_get(void)
{
	int i, ret;
	unsigned long selection_rate;
	struct clk *cpu_clk_g = tegra_get_clock_by_name("cpu_g");
	struct clk *cpu_clk_lp = tegra_get_clock_by_name("cpu_lp");

	/* For table selection use top cpu_g rate in dvfs ladder; selection
	   rate may exceed cpu max_rate (e.g., because of edp limitations on
	   cpu voltage) - in any case max_rate will be clipped to the table */
	if (cpu_clk_g->dvfs && cpu_clk_g->dvfs->num_freqs)
		selection_rate =
			cpu_clk_g->dvfs->freqs[cpu_clk_g->dvfs->num_freqs - 1];
	else
		selection_rate = cpu_clk_g->max_rate;

	for (i = 0; i < ARRAY_SIZE(cpufreq_tables); i++) {
		struct cpufreq_policy policy;
		policy.cpu = 0;	/* any on-line cpu */
		ret = cpufreq_frequency_table_cpuinfo(
			&policy, cpufreq_tables[i].freq_table);
		if (!ret) {
			if ((policy.max * 1000) == selection_rate) {
				ret = clip_cpu_rate_limits(
					&cpufreq_tables[i],
					&policy, cpu_clk_g, cpu_clk_lp);
				if (!ret)
					return &cpufreq_tables[i];
			}
		}
	}
	WARN(1, "%s: No cpufreq table matching G & LP cpu ranges", __func__);
	return NULL;
}

/* On DDR3 platforms there is an implicit dependency in this mapping: when cpu
 * exceeds max dvfs level for LP CPU clock at TEGRA_EMC_BRIDGE_MVOLTS_MIN, the
 * respective emc rate should be above TEGRA_EMC_BRIDGE_RATE_MIN
 */
/* FIXME: explicitly check this dependency */
unsigned long tegra_emc_to_cpu_ratio(unsigned long cpu_rate)
{
	static unsigned long emc_max_rate = 0;

	if (emc_max_rate == 0)
		emc_max_rate = clk_round_rate(
			tegra_get_clock_by_name("emc"), ULONG_MAX);

	/* Vote on memory bus frequency based on cpu frequency;
	   cpu rate is in kHz, emc rate is in Hz */
	if (cpu_rate >= 925000)
		return emc_max_rate;	/* cpu >= 925 MHz, emc max */
	else if (cpu_rate >= 450000)
		return emc_max_rate/2;	/* cpu >= 450 MHz, emc max/2 */
	else if (cpu_rate >= 250000)
		return 100000000;	/* cpu >= 250 MHz, emc 100 MHz */
	else
		return 0;		/* emc min */
}

int tegra_update_mselect_rate(unsigned long cpu_rate)
{
	static struct clk *mselect = NULL;

	unsigned long mselect_rate;

	if (!mselect) {
		mselect = tegra_get_clock_by_name("mselect");
		if (!mselect)
			return -ENODEV;
	}

#ifdef CONFIG_TEGRA_PCI
	/* Vote on mselect frequency based on cpu frequency:
	   keep mselect at cpu rate up to 204 MHz;
	   cpu rate is in kHz, mselect rate is in Hz */
	mselect_rate = cpu_rate * 1000;
	mselect_rate = min(mselect_rate, 204000000UL);
#else
	/* Vote on mselect frequency based on cpu frequency:
	   keep mselect at half of cpu rate up to 102 MHz;
	   cpu rate is in kHz, mselect rate is in Hz */
	mselect_rate = DIV_ROUND_UP(cpu_rate, 2) * 1000;
	mselect_rate = min(mselect_rate, 102000000UL);
#endif

	if (mselect_rate != clk_get_rate(mselect))
		return clk_set_rate(mselect, mselect_rate);

	return 0;
}
#endif

#ifdef CONFIG_PM_SLEEP
static u32 clk_rst_suspend[RST_DEVICES_NUM + CLK_OUT_ENB_NUM +
			   PERIPH_CLK_SOURCE_NUM + 24];

static int tegra_clk_suspend(void)
{
	unsigned long off;
	u32 *ctx = clk_rst_suspend;

	*ctx++ = clk_readl(OSC_CTRL) & OSC_CTRL_MASK;
	*ctx++ = clk_readl(CPU_SOFTRST_CTRL);

	*ctx++ = clk_readl(tegra_pll_p_out1.reg);
	*ctx++ = clk_readl(tegra_pll_p_out3.reg);

	*ctx++ = clk_readl(tegra_pll_c.reg + PLL_BASE);
	*ctx++ = clk_readl(tegra_pll_c.reg + PLL_MISC(&tegra_pll_c));
	*ctx++ = clk_readl(tegra_pll_a.reg + PLL_BASE);
	*ctx++ = clk_readl(tegra_pll_a.reg + PLL_MISC(&tegra_pll_a));
	*ctx++ = clk_readl(tegra_pll_d.reg + PLL_BASE);
	*ctx++ = clk_readl(tegra_pll_d.reg + PLL_MISC(&tegra_pll_d));
	*ctx++ = clk_readl(tegra_pll_d2.reg + PLL_BASE);
	*ctx++ = clk_readl(tegra_pll_d2.reg + PLL_MISC(&tegra_pll_d2));

	*ctx++ = clk_readl(tegra_pll_m_out1.reg);
	*ctx++ = clk_readl(tegra_pll_a_out0.reg);
	*ctx++ = clk_readl(tegra_pll_c_out1.reg);

	*ctx++ = clk_readl(tegra_clk_cclk_g.reg);
	*ctx++ = clk_readl(tegra_clk_cclk_g.reg + SUPER_CLK_DIVIDER);
	*ctx++ = clk_readl(tegra_clk_cclk_lp.reg);
	*ctx++ = clk_readl(tegra_clk_cclk_lp.reg + SUPER_CLK_DIVIDER);

	*ctx++ = clk_readl(tegra_clk_sclk.reg);
	*ctx++ = clk_readl(tegra_clk_sclk.reg + SUPER_CLK_DIVIDER);
	*ctx++ = clk_readl(tegra_clk_pclk.reg);

	for (off = PERIPH_CLK_SOURCE_I2S1; off <= PERIPH_CLK_SOURCE_OSC;
			off += 4) {
		if (off == PERIPH_CLK_SOURCE_EMC)
			continue;
		*ctx++ = clk_readl(off);
	}
	for (off = PERIPH_CLK_SOURCE_G3D2; off <= PERIPH_CLK_SOURCE_SE;
			off+=4) {
		*ctx++ = clk_readl(off);
	}
	for (off = AUDIO_DLY_CLK; off <= AUDIO_SYNC_CLK_SPDIF; off+=4) {
		*ctx++ = clk_readl(off);
	}

	*ctx++ = clk_readl(RST_DEVICES_L);
	*ctx++ = clk_readl(RST_DEVICES_H);
	*ctx++ = clk_readl(RST_DEVICES_U);
	*ctx++ = clk_readl(RST_DEVICES_V);
	*ctx++ = clk_readl(RST_DEVICES_W);

	*ctx++ = clk_readl(CLK_OUT_ENB_L);
	*ctx++ = clk_readl(CLK_OUT_ENB_H);
	*ctx++ = clk_readl(CLK_OUT_ENB_U);
	*ctx++ = clk_readl(CLK_OUT_ENB_V);
	*ctx++ = clk_readl(CLK_OUT_ENB_W);

	*ctx++ = clk_readl(MISC_CLK_ENB);
	*ctx++ = clk_readl(CLK_MASK_ARM);

	return 0;
}

static void tegra_clk_resume(void)
{
	unsigned long off;
	const u32 *ctx = clk_rst_suspend;
	u32 val;
	u32 pllc_base;
	u32 plla_base;
	u32 plld_base;
	u32 plld2_base;
	u32 pll_p_out12, pll_p_out34;
	u32 pll_a_out0, pll_m_out1, pll_c_out1;
	struct clk *p;

	val = clk_readl(OSC_CTRL) & ~OSC_CTRL_MASK;
	val |= *ctx++;
	clk_writel(val, OSC_CTRL);
	clk_writel(*ctx++, CPU_SOFTRST_CTRL);

	/* Since we are going to reset devices and switch clock sources in this
	 * function, plls and secondary dividers is required to be enabled. The
	 * actual value will be restored back later. Note that boot plls: pllm,
	 * pllp, and pllu are already configured and enabled.
	 */
	val = PLL_OUT_CLKEN | PLL_OUT_RESET_DISABLE;
	val |= val << 16;
	pll_p_out12 = *ctx++;
	clk_writel(pll_p_out12 | val, tegra_pll_p_out1.reg);
	pll_p_out34 = *ctx++;
	clk_writel(pll_p_out34 | val, tegra_pll_p_out3.reg);

	pllc_base = *ctx++;
	clk_writel(pllc_base | PLL_BASE_ENABLE, tegra_pll_c.reg + PLL_BASE);
	clk_writel(*ctx++, tegra_pll_c.reg + PLL_MISC(&tegra_pll_c));

	plla_base = *ctx++;
	clk_writel(plla_base | PLL_BASE_ENABLE, tegra_pll_a.reg + PLL_BASE);
	clk_writel(*ctx++, tegra_pll_a.reg + PLL_MISC(&tegra_pll_a));

	plld_base = *ctx++;
	clk_writel(plld_base | PLL_BASE_ENABLE, tegra_pll_d.reg + PLL_BASE);
	clk_writel(*ctx++, tegra_pll_d.reg + PLL_MISC(&tegra_pll_d));

	plld2_base = *ctx++;
	clk_writel(plld2_base | PLL_BASE_ENABLE, tegra_pll_d2.reg + PLL_BASE);
	clk_writel(*ctx++, tegra_pll_d2.reg + PLL_MISC(&tegra_pll_d2));

	udelay(1000);

	val = PLL_OUT_CLKEN | PLL_OUT_RESET_DISABLE;
	pll_m_out1 = *ctx++;
	clk_writel(pll_m_out1 | val, tegra_pll_m_out1.reg);
	pll_a_out0 = *ctx++;
	clk_writel(pll_a_out0 | val, tegra_pll_a_out0.reg);
	pll_c_out1 = *ctx++;
	clk_writel(pll_c_out1 | val, tegra_pll_c_out1.reg);

	clk_writel(*ctx++, tegra_clk_cclk_g.reg);
	clk_writel(*ctx++, tegra_clk_cclk_g.reg + SUPER_CLK_DIVIDER);
	clk_writel(*ctx++, tegra_clk_cclk_lp.reg);
	clk_writel(*ctx++, tegra_clk_cclk_lp.reg + SUPER_CLK_DIVIDER);

	clk_writel(*ctx++, tegra_clk_sclk.reg);
	clk_writel(*ctx++, tegra_clk_sclk.reg + SUPER_CLK_DIVIDER);
	clk_writel(*ctx++, tegra_clk_pclk.reg);

	/* enable all clocks before configuring clock sources */
	clk_writel(0xfdfffff1ul, CLK_OUT_ENB_L);
	clk_writel(0xfefff7f7ul, CLK_OUT_ENB_H);
	clk_writel(0x75f79bfful, CLK_OUT_ENB_U);
	clk_writel(0xfffffffful, CLK_OUT_ENB_V);
	clk_writel(0x00003ffful, CLK_OUT_ENB_W);
	wmb();

	for (off = PERIPH_CLK_SOURCE_I2S1; off <= PERIPH_CLK_SOURCE_OSC;
			off += 4) {
		if (off == PERIPH_CLK_SOURCE_EMC)
			continue;
		clk_writel(*ctx++, off);
	}
	for (off = PERIPH_CLK_SOURCE_G3D2; off <= PERIPH_CLK_SOURCE_SE;
			off += 4) {
		clk_writel(*ctx++, off);
	}
	for (off = AUDIO_DLY_CLK; off <= AUDIO_SYNC_CLK_SPDIF; off+=4) {
		clk_writel(*ctx++, off);
	}
	wmb();

	udelay(RESET_PROPAGATION_DELAY);

	clk_writel(*ctx++, RST_DEVICES_L);
	clk_writel(*ctx++, RST_DEVICES_H);
	clk_writel(*ctx++, RST_DEVICES_U);

	/* For LP0 resume, don't reset lpcpu, since we are running from it */
	val = *ctx++;
	val &= ~RST_DEVICES_V_SWR_CPULP_RST_DIS;
	clk_writel(val, RST_DEVICES_V);

	clk_writel(*ctx++, RST_DEVICES_W);
	wmb();

	clk_writel(*ctx++, CLK_OUT_ENB_L);
	clk_writel(*ctx++, CLK_OUT_ENB_H);
	clk_writel(*ctx++, CLK_OUT_ENB_U);

	/* For LP0 resume, clk to lpcpu is required to be on */
	val = *ctx++;
	val |= CLK_OUT_ENB_V_CLK_ENB_CPULP_EN;
	clk_writel(val, CLK_OUT_ENB_V);

	clk_writel(*ctx++, CLK_OUT_ENB_W);
	wmb();

	clk_writel(*ctx++, MISC_CLK_ENB);
	clk_writel(*ctx++, CLK_MASK_ARM);

	/* Restore back the actual pll and secondary divider values */
	/* FIXME: need to root cause why pllc is required to be on
	 * clk_writel(pllc_base, tegra_pll_c.reg + PLL_BASE);
	 */
	clk_writel(pll_p_out12, tegra_pll_p_out1.reg);
	clk_writel(pll_p_out34, tegra_pll_p_out3.reg);

	clk_writel(plla_base, tegra_pll_a.reg + PLL_BASE);
	clk_writel(plld_base, tegra_pll_d.reg + PLL_BASE);
	clk_writel(plld2_base, tegra_pll_d2.reg + PLL_BASE);

	clk_writel(pll_m_out1, tegra_pll_m_out1.reg);
	clk_writel(pll_a_out0, tegra_pll_a_out0.reg);
	clk_writel(pll_c_out1, tegra_pll_c_out1.reg);

	/* Since EMC clock is not restored, and may not preserve parent across
	   suspend, update current state, and mark EMC DFS as out of sync */
	tegra3_pll_m_override_update(&tegra_pll_m, false);
	p = tegra_clk_emc.parent;
	tegra3_periph_clk_init(&tegra_clk_emc);

	if (p != tegra_clk_emc.parent) {
		/* FIXME: old parent is left enabled here even if EMC was its
		   only child before suspend (never happens on Tegra3) */
		pr_debug("EMC parent(refcount) across suspend: %s(%d) : %s(%d)",
			p->name, p->refcnt, tegra_clk_emc.parent->name,
			tegra_clk_emc.parent->refcnt);

		BUG_ON(!p->refcnt);
		p->refcnt--;

		/* the new parent is enabled by low level code, but ref count
		   need to be updated up to the root */
		p = tegra_clk_emc.parent;
		while (p && ((p->refcnt++) == 0))
			p = p->parent;
	}
	tegra_emc_timing_invalidate();

	tegra3_pll_clk_init(&tegra_pll_u); /* Re-init utmi parameters */
	tegra3_pllp_clk_resume(&tegra_pll_p); /* Fire a bug if not restored */
}
#else
#define tegra_clk_suspend NULL
#define tegra_clk_resume NULL
#endif

static struct syscore_ops tegra_clk_syscore_ops = {
	.suspend = tegra_clk_suspend,
	.resume = tegra_clk_resume,
};

#ifdef CONFIG_TEGRA_PREINIT_CLOCKS

#define CLK_RSTENB_DEV_V_0_DAM2_BIT	(1 << 14)
#define CLK_RSTENB_DEV_V_0_DAM1_BIT	(1 << 13)
#define CLK_RSTENB_DEV_V_0_DAM0_BIT	(1 << 12)
#define CLK_RSTENB_DEV_V_0_AUDIO_BIT	(1 << 10)
#define CLK_RSTENB_DEV_V_0_3D2_BIT	(1 << 2)

#define CLK_RSTENB_DEV_L_0_HOST1X_BIT	(1 << 28)
#define CLK_RSTENB_DEV_L_0_DISP1_BIT	(1 << 27)
#define CLK_RSTENB_DEV_L_0_3D_BIT	(1 << 24)
#define CLK_RSTENB_DEV_L_0_2D_BIT	(1 << 21)
#define CLK_RSTENB_DEV_L_0_VI_BIT	(1 << 20)
#define CLK_RSTENB_DEV_L_0_EPP_BIT	(1 << 19)

#define CLK_RSTENB_DEV_H_0_VDE_BIT	(1 << 29)
#define CLK_RSTENB_DEV_H_0_MPE_BIT	(1 << 28)

#define DISP1_CLK_REG_OFFSET		0x138
#define DISP1_CLK_SRC_SHIFT		29
#define DISP1_CLK_SRC_MASK		(0x7 << DISP1_CLK_SRC_SHIFT)
#define DISP1_CLK_SRC_PLLP_OUT0 	0
#define DISP1_CLK_SRC_PLLM_OUT0 	1
#define DISP1_CLK_SRC_PLLD_OUT0 	2
#define DISP1_CLK_SRC_PLLA_OUT0 	3
#define DISP1_CLK_SRC_PLLC_OUT0 	4
#define DISP1_CLK_SRC_PLLD2_OUT0	5
#define DISP1_CLK_SRC_CLKM		6
#define DISP1_CLK_SRC_DEFAULT (DISP1_CLK_SRC_PLLP_OUT0 << DISP1_CLK_SRC_SHIFT)

#define HOST1X_CLK_REG_OFFSET		0x180
#define HOST1X_CLK_SRC_SHIFT		30
#define HOST1X_CLK_SRC_MASK		(0x3 << HOST1X_CLK_SRC_SHIFT)
#define HOST1X_CLK_SRC_PLLM_OUT0	0
#define HOST1X_CLK_SRC_PLLC_OUT0	1
#define HOST1X_CLK_SRC_PLLP_OUT0	2
#define HOST1X_CLK_SRC_PLLA_OUT0	3
#define HOST1X_CLK_SRC_DEFAULT (\
		HOST1X_CLK_SRC_PLLP_OUT0 << HOST1X_CLK_SRC_SHIFT)
#define HOST1X_CLK_IDLE_DIV_SHIFT	8
#define HOST1X_CLK_IDLE_DIV_MASK	(0xff << HOST1X_CLK_IDLE_DIV_SHIFT)
#define HOST1X_CLK_IDLE_DIV_DEFAULT	(0 << HOST1X_CLK_IDLE_DIV_SHIFT)
#define HOST1X_CLK_DIV_SHIFT		0
#define HOST1X_CLK_DIV_MASK		(0xff << HOST1X_CLK_DIV_SHIFT)
#define HOST1X_CLK_DIV_DEFAULT		(3 << HOST1X_CLK_DIV_SHIFT)

#define AUDIO_CLK_REG_OFFSET		0x3d0
#define DAM0_CLK_REG_OFFSET		0x3d8
#define DAM1_CLK_REG_OFFSET		0x3dc
#define DAM2_CLK_REG_OFFSET		0x3e0
#define AUDIO_CLK_SRC_SHIFT		28
#define AUDIO_CLK_SRC_MASK		(0x0f << AUDIO_CLK_SRC_SHIFT)
#define AUDIO_CLK_SRC_PLLA_OUT0 	0x01
#define AUDIO_CLK_SRC_PLLC_OUT0 	0x05
#define AUDIO_CLK_SRC_PLLP_OUT0 	0x09
#define AUDIO_CLK_SRC_CLKM		0x0d
#define AUDIO_CLK_SRC_DEFAULT (\
		AUDIO_CLK_SRC_CLKM << AUDIO_CLK_SRC_SHIFT)
#define AUDIO_CLK_DIV_SHIFT		0
#define AUDIO_CLK_DIV_MASK		(0xff << AUDIO_CLK_DIV_SHIFT)
#define AUDIO_CLK_DIV_DEFAULT (\
		(0 << AUDIO_CLK_DIV_SHIFT))

#define VCLK_SRC_SHIFT  		30
#define VCLK_SRC_MASK			(0x3 << VCLK_SRC_SHIFT)
#define VCLK_SRC_PLLM_OUT0		0
#define VCLK_SRC_PLLC_OUT0		1
#define VCLK_SRC_PLLP_OUT0		2
#define VCLK_SRC_PLLA_OUT0		3
#define VCLK_SRC_DEFAULT		(VCLK_SRC_PLLM_OUT0 << VCLK_SRC_SHIFT)
#define VCLK_IDLE_DIV_SHIFT		8
#define VCLK_IDLE_DIV_MASK		(0xff << VCLK_IDLE_DIV_SHIFT)
#define VCLK_IDLE_DIV_DEFAULT		(0 << VCLK_IDLE_DIV_SHIFT)
#define VCLK_DIV_SHIFT  		0
#define VCLK_DIV_MASK			(0xff << VCLK_DIV_SHIFT)
#define VCLK_DIV_DEFAULT		(0xa << VCLK_DIV_SHIFT)

#define VI_CLK_REG_OFFSET		0x148
#define  VI_CLK_SEL_VI_SENSOR_CLK	(1 << 25)
#define  VI_CLK_SEL_EXTERNAL_CLK	(1 << 24)
#define  VI_SENSOR_CLK_REG_OFFSET	0x1a8
#define G3D_CLK_REG_OFFSET		0x158
#define G2D_CLK_REG_OFFSET		0x15c
#define EPP_CLK_REG_OFFSET		0x16c
#define MPE_CLK_REG_OFFSET		0x170
#define VDE_CLK_REG_OFFSET		0x170
#define G3D2_CLK_REG_OFFSET		0x3b0

static void __init clk_setbit(u32 reg, u32 bit)
{
	u32 val = clk_readl(reg);

	if ((val & bit) == bit)
		return;
	val |= bit;
	clk_writel(val, reg);
	udelay(2);
}

static void __init clk_clrbit(u32 reg, u32 bit)
{
	u32 val = clk_readl(reg);

	if ((val & bit) == 0)
		return;
	val &= ~bit;
	clk_writel(val, reg);
	udelay(2);
}

static void __init clk_setbits(u32 reg, u32 bits, u32 mask)
{
	u32 val = clk_readl(reg);

	if ((val & mask) == bits)
		return;
	val &= ~mask;
	val |= bits;
	clk_writel(val, reg);
	udelay(2);
}

static void __init vclk_init(int tag, u32 src, u32 rebit)
{
	u32 rst, enb;

	switch (tag) {
	case 'L':
		rst = RST_DEVICES_L;
		enb = CLK_OUT_ENB_L;
		break;
	case 'H':
		rst = RST_DEVICES_H;
		enb = CLK_OUT_ENB_H;
		break;
	case 'U':
		rst = RST_DEVICES_U;
		enb = CLK_OUT_ENB_U;
		break;
	case 'V':
		rst = RST_DEVICES_V;
		enb = CLK_OUT_ENB_V;
		break;
	case 'W':
		rst = RST_DEVICES_W;
		enb = CLK_OUT_ENB_W;
		break;
	default:
		/* Quietly ignore. */
		return;
	}

	clk_setbit(rst, rebit);
	clk_clrbit(enb, rebit);
	clk_setbits(src, VCLK_SRC_DEFAULT, VCLK_SRC_MASK);
	clk_setbits(src, VCLK_DIV_DEFAULT, VCLK_DIV_MASK);
	clk_clrbit(rst, rebit);
}

static int __init tegra_soc_preinit_clocks(void)
{
	/*
	 * Make sure host1x clock configuration has:
	 *	HOST1X_CLK_SRC    : PLLP_OUT0.
	 *	HOST1X_CLK_DIVISOR: >2 to start from safe enough frequency.
	 */
	clk_setbit(RST_DEVICES_L, CLK_RSTENB_DEV_L_0_HOST1X_BIT);
	clk_setbit(CLK_OUT_ENB_L, CLK_RSTENB_DEV_L_0_HOST1X_BIT);
	clk_setbits(HOST1X_CLK_REG_OFFSET,
		    HOST1X_CLK_DIV_DEFAULT, HOST1X_CLK_DIV_MASK);
	clk_setbits(HOST1X_CLK_REG_OFFSET,
		    HOST1X_CLK_IDLE_DIV_DEFAULT, HOST1X_CLK_IDLE_DIV_MASK);
	clk_setbits(HOST1X_CLK_REG_OFFSET,
		    HOST1X_CLK_SRC_DEFAULT, HOST1X_CLK_SRC_MASK);
	clk_clrbit(RST_DEVICES_L, CLK_RSTENB_DEV_L_0_HOST1X_BIT);

	/*
	 *  Make sure disp1 clock configuration ha:
	 *	DISP1_CLK_SRC:	DISP1_CLK_SRC_PLLP_OUT0
	 */
	clk_setbit(RST_DEVICES_L, CLK_RSTENB_DEV_L_0_DISP1_BIT);
	clk_setbit(CLK_OUT_ENB_L, CLK_RSTENB_DEV_L_0_DISP1_BIT);
	clk_setbits(DISP1_CLK_REG_OFFSET,
		    DISP1_CLK_SRC_DEFAULT, DISP1_CLK_SRC_MASK);
	clk_clrbit(RST_DEVICES_L, CLK_RSTENB_DEV_L_0_DISP1_BIT);

	/*
	 *  Make sure dam2 clock configuration has:
	 *	DAM2_CLK_SRC:	AUDIO_CLK_SRC_CLKM
	 */
	clk_setbit(RST_DEVICES_V, CLK_RSTENB_DEV_V_0_DAM2_BIT);
	clk_setbit(CLK_OUT_ENB_V, CLK_RSTENB_DEV_V_0_DAM2_BIT);
	clk_setbits(DAM2_CLK_REG_OFFSET,
		    AUDIO_CLK_DIV_DEFAULT, AUDIO_CLK_DIV_MASK);
	clk_setbits(DAM2_CLK_REG_OFFSET,
		    AUDIO_CLK_SRC_DEFAULT, AUDIO_CLK_SRC_MASK);
	clk_clrbit(RST_DEVICES_V, CLK_RSTENB_DEV_V_0_DAM2_BIT);

	/*
	 *  Make sure dam1 clock configuration has:
	 *	DAM1_CLK_SRC:	AUDIO_CLK_SRC_CLKM
	 */
	clk_setbit(RST_DEVICES_V, CLK_RSTENB_DEV_V_0_DAM1_BIT);
	clk_setbit(CLK_OUT_ENB_V, CLK_RSTENB_DEV_V_0_DAM1_BIT);
	clk_setbits(DAM1_CLK_REG_OFFSET,
		    AUDIO_CLK_DIV_DEFAULT, AUDIO_CLK_DIV_MASK);
	clk_setbits(DAM1_CLK_REG_OFFSET,
		    AUDIO_CLK_SRC_DEFAULT, AUDIO_CLK_SRC_MASK);
	clk_clrbit(RST_DEVICES_V, CLK_RSTENB_DEV_V_0_DAM1_BIT);

	/*
	 *  Make sure dam0 clock configuration has:
	 *	DAM0_CLK_SRC:	AUDIO_CLK_SRC_CLKM
	 */
	clk_setbit(RST_DEVICES_V, CLK_RSTENB_DEV_V_0_DAM0_BIT);
	clk_setbit(CLK_OUT_ENB_V, CLK_RSTENB_DEV_V_0_DAM0_BIT);
	clk_setbits(DAM0_CLK_REG_OFFSET,
		    AUDIO_CLK_DIV_DEFAULT, AUDIO_CLK_DIV_MASK);
	clk_setbits(DAM0_CLK_REG_OFFSET,
		    AUDIO_CLK_SRC_DEFAULT, AUDIO_CLK_SRC_MASK);
	clk_clrbit(RST_DEVICES_V, CLK_RSTENB_DEV_V_0_DAM0_BIT);

	/*
	 *  Make sure d_audio clock configuration has:
	 *	AUDIO_CLK_SRC:	AUDIO_CLK_SRC_CLKM
	 */
	clk_setbit(RST_DEVICES_V, CLK_RSTENB_DEV_V_0_AUDIO_BIT);
	clk_setbit(CLK_OUT_ENB_V, CLK_RSTENB_DEV_V_0_AUDIO_BIT);
	clk_setbits(AUDIO_CLK_REG_OFFSET,
		    AUDIO_CLK_DIV_DEFAULT, AUDIO_CLK_DIV_MASK);
	clk_setbits(AUDIO_CLK_REG_OFFSET,
		    AUDIO_CLK_SRC_DEFAULT, AUDIO_CLK_SRC_MASK);
	clk_clrbit(RST_DEVICES_V, CLK_RSTENB_DEV_V_0_AUDIO_BIT);

	/* Pre-initialize Video clocks. */
	vclk_init('L', G3D_CLK_REG_OFFSET, CLK_RSTENB_DEV_L_0_3D_BIT);
	vclk_init('L', G2D_CLK_REG_OFFSET, CLK_RSTENB_DEV_L_0_2D_BIT);
	vclk_init('L', VI_CLK_REG_OFFSET, CLK_RSTENB_DEV_L_0_VI_BIT);
	vclk_init('L', EPP_CLK_REG_OFFSET, CLK_RSTENB_DEV_L_0_EPP_BIT);
	vclk_init('H', VDE_CLK_REG_OFFSET, CLK_RSTENB_DEV_H_0_VDE_BIT);
	vclk_init('H', MPE_CLK_REG_OFFSET, CLK_RSTENB_DEV_H_0_MPE_BIT);
	vclk_init('V', G3D2_CLK_REG_OFFSET, CLK_RSTENB_DEV_V_0_3D2_BIT);

	return 0;
}
#endif /* CONFIG_TEGRA_PREINIT_CLOCKS */

void __init tegra30_init_clocks(void)
{
	int i;
	struct clk *c;

#ifdef CONFIG_TEGRA_PREINIT_CLOCKS
	tegra_soc_preinit_clocks();
#endif /* CONFIG_TEGRA_PREINIT_CLOCKS */

	for (i = 0; i < ARRAY_SIZE(tegra_ptr_clks); i++)
		tegra3_init_one_clock(tegra_ptr_clks[i]);

	for (i = 0; i < ARRAY_SIZE(tegra_list_clks); i++)
		tegra3_init_one_clock(&tegra_list_clks[i]);

	for (i = 0; i < ARRAY_SIZE(tegra_clk_duplicates); i++) {
		c = tegra_get_clock_by_name(tegra_clk_duplicates[i].name);
		if (!c) {
			pr_err("%s: Unknown duplicate clock %s\n", __func__,
				tegra_clk_duplicates[i].name);
			continue;
		}

		tegra_clk_duplicates[i].lookup.clk = c;
		clkdev_add(&tegra_clk_duplicates[i].lookup);
	}

	for (i = 0; i < ARRAY_SIZE(tegra_sync_source_list); i++)
		tegra3_init_one_clock(&tegra_sync_source_list[i]);
	for (i = 0; i < ARRAY_SIZE(tegra_clk_audio_list); i++)
		tegra3_init_one_clock(&tegra_clk_audio_list[i]);
	for (i = 0; i < ARRAY_SIZE(tegra_clk_audio_2x_list); i++)
		tegra3_init_one_clock(&tegra_clk_audio_2x_list[i]);

	init_clk_out_mux();
	for (i = 0; i < ARRAY_SIZE(tegra_clk_out_list); i++)
		tegra3_init_one_clock(&tegra_clk_out_list[i]);

	emc_bridge = &tegra_clk_emc_bridge;

	/* Initialize to default */
	tegra_init_cpu_edp_limits(0);

	register_syscore_ops(&tegra_clk_syscore_ops);
}
