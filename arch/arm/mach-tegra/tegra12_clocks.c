/*
 * arch/arm/mach-tegra/tegra12_clocks.c
 *
 * Copyright (C) 2011-2014 NVIDIA Corporation. All rights reserved.
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
#include <linux/platform_device.h>
#include <linux/tegra-soc.h>

#include <asm/clkdev.h>

#include <mach/edp.h>
#include <mach/mc.h>

#include "clock.h"
#include "dvfs.h"
#include "iomap.h"
#include "pm.h"
#include "sleep.h"
#include "devices.h"
#include "tegra12_emc.h"
#include "tegra_cl_dvfs.h"
#include "cpu-tegra.h"
#include "tegra11_soctherm.h"

#define RST_DEVICES_L			0x004
#define RST_DEVICES_H			0x008
#define RST_DEVICES_U			0x00C
#define RST_DEVICES_V			0x358
#define RST_DEVICES_W			0x35C
#define RST_DEVICES_X			0x28C
#define RST_DEVICES_SET_L		0x300
#define RST_DEVICES_CLR_L		0x304
#define RST_DEVICES_SET_V		0x430
#define RST_DEVICES_CLR_V		0x434
#define RST_DEVICES_SET_X		0x290
#define RST_DEVICES_CLR_X		0x294
#define RST_DEVICES_NUM			6

#define CLK_OUT_ENB_L			0x010
#define CLK_OUT_ENB_H			0x014
#define CLK_OUT_ENB_U			0x018
#define CLK_OUT_ENB_V			0x360
#define CLK_OUT_ENB_W			0x364
#define CLK_OUT_ENB_X			0x280
#define CLK_OUT_ENB_SET_L		0x320
#define CLK_OUT_ENB_CLR_L		0x324
#define CLK_OUT_ENB_SET_V		0x440
#define CLK_OUT_ENB_CLR_V		0x444
#define CLK_OUT_ENB_SET_X		0x284
#define CLK_OUT_ENB_CLR_X		0x288
#define CLK_OUT_ENB_NUM			6

#define CLK_OUT_ENB_L_RESET_MASK	0xfcd7dff1
#define CLK_OUT_ENB_H_RESET_MASK	0xefddfff7
#define CLK_OUT_ENB_U_RESET_MASK	0xfbfefbfa
#define CLK_OUT_ENB_V_RESET_MASK	0xffc1fffb
#define CLK_OUT_ENB_W_RESET_MASK	0x3f7fbfff
#define CLK_OUT_ENB_X_RESET_MASK	0x00170979

#define RST_DEVICES_V_SWR_CPULP_RST_DIS	(0x1 << 1) /* Reserved on Tegra11 */
#define CLK_OUT_ENB_V_CLK_ENB_CPULP_EN	(0x1 << 1)

#define PERIPH_CLK_TO_BIT(c)		(1 << (c->u.periph.clk_num % 32))
#define PERIPH_CLK_TO_RST_REG(c)	\
	periph_clk_to_reg((c), RST_DEVICES_L, RST_DEVICES_V, RST_DEVICES_X, 4)
#define PERIPH_CLK_TO_RST_SET_REG(c)	\
	periph_clk_to_reg((c), RST_DEVICES_SET_L, RST_DEVICES_SET_V, \
		RST_DEVICES_SET_X, 8)
#define PERIPH_CLK_TO_RST_CLR_REG(c)	\
	periph_clk_to_reg((c), RST_DEVICES_CLR_L, RST_DEVICES_CLR_V, \
		RST_DEVICES_CLR_X, 8)

#define PERIPH_CLK_TO_ENB_REG(c)	\
	periph_clk_to_reg((c), CLK_OUT_ENB_L, CLK_OUT_ENB_V, CLK_OUT_ENB_X, 4)
#define PERIPH_CLK_TO_ENB_SET_REG(c)	\
	periph_clk_to_reg((c), CLK_OUT_ENB_SET_L, CLK_OUT_ENB_SET_V, \
		CLK_OUT_ENB_SET_X, 8)
#define PERIPH_CLK_TO_ENB_CLR_REG(c)	\
	periph_clk_to_reg((c), CLK_OUT_ENB_CLR_L, CLK_OUT_ENB_CLR_V, \
		CLK_OUT_ENB_CLR_X, 8)

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
#define PERIPH_CLK_SOURCE_EMC_MC_SAME	(1<<16)

#define PERIPH_CLK_SOURCE_LA		0x1f8
#define PERIPH_CLK_SOURCE_NUM1 \
	((PERIPH_CLK_SOURCE_LA - PERIPH_CLK_SOURCE_I2S1) / 4)

#define PERIPH_CLK_SOURCE_MSELECT	0x3b4
#define PERIPH_CLK_SOURCE_SE		0x42c
#define PERIPH_CLK_SOURCE_NUM2 \
	((PERIPH_CLK_SOURCE_SE - PERIPH_CLK_SOURCE_MSELECT) / 4 + 1)

#define AUDIO_DLY_CLK			0x49c
#define AUDIO_SYNC_CLK_SPDIF		0x4b4
#define PERIPH_CLK_SOURCE_NUM3 \
	((AUDIO_SYNC_CLK_SPDIF - AUDIO_DLY_CLK) / 4 + 1)

#define SPARE_REG			0x55c
#define SPARE_REG_CLK_M_DIVISOR_SHIFT	2
#define SPARE_REG_CLK_M_DIVISOR_MASK	(3 << SPARE_REG_CLK_M_DIVISOR_SHIFT)

#define PERIPH_CLK_SOURCE_XUSB_HOST	0x600
#define PERIPH_CLK_SOURCE_VIC		0x678
#define PERIPH_CLK_SOURCE_NUM4 \
	((PERIPH_CLK_SOURCE_VIC - PERIPH_CLK_SOURCE_XUSB_HOST) / 4 + 1)

#define PERIPH_CLK_SOURCE_NUM		(PERIPH_CLK_SOURCE_NUM1 + \
					 PERIPH_CLK_SOURCE_NUM2 + \
					 PERIPH_CLK_SOURCE_NUM3 + \
					 PERIPH_CLK_SOURCE_NUM4)

#define CPU_SOFTRST_CTRL		0x380
#define CPU_SOFTRST_CTRL1		0x384
#define CPU_SOFTRST_CTRL2		0x388

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

#define PERIPH_CLK_SOR_CLK_SEL_SHIFT	14
#define PERIPH_CLK_SOR_CLK_SEL_MASK	(0x3<<PERIPH_CLK_SOR_CLK_SEL_SHIFT)

/* PLL common */
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

#define PLL_BASE_PARSE(pll, cfg, b)					       \
	do {								       \
		(cfg).m = ((b) & pll##_BASE_DIVM_MASK) >> PLL_BASE_DIVM_SHIFT; \
		(cfg).n = ((b) & pll##_BASE_DIVN_MASK) >> PLL_BASE_DIVN_SHIFT; \
		(cfg).p = ((b) & pll##_BASE_DIVP_MASK) >> PLL_BASE_DIVP_SHIFT; \
	} while (0)

#define PLL_OUT_RATIO_MASK		(0xFF<<8)
#define PLL_OUT_RATIO_SHIFT		8
#define PLL_OUT_OVERRIDE		(1<<2)
#define PLL_OUT_CLKEN			(1<<1)
#define PLL_OUT_RESET_DISABLE		(1<<0)

#define PLL_MISC(c)			\
	(((c)->flags & PLL_ALT_MISC_REG) ? 0x4 : 0xc)
#define PLL_MISCN(c, n)		\
	((c)->u.pll.misc1 + ((n) - 1) * PLL_MISC(c))
#define PLL_MISC_LOCK_ENABLE(c)	\
	(((c)->flags & (PLLU | PLLD)) ? (1<<22) : (1<<18))

#define PLL_MISC_DCCON_SHIFT		20
#define PLL_MISC_CPCON_SHIFT		8
#define PLL_MISC_CPCON_MASK		(0xF<<PLL_MISC_CPCON_SHIFT)
#define PLL_MISC_LFCON_SHIFT		4
#define PLL_MISC_LFCON_MASK		(0xF<<PLL_MISC_LFCON_SHIFT)
#define PLL_MISC_VCOCON_SHIFT		0
#define PLL_MISC_VCOCON_MASK		(0xF<<PLL_MISC_VCOCON_SHIFT)

#define PLL_FIXED_MDIV(c, ref)		((ref) > (c)->u.pll.cf_max ? 2 : 1)

/* PLLU */
#define PLLU_BASE_OVERRIDE		(1<<24)
#define PLLU_BASE_POST_DIV		(1<<20)

/* PLLD */
#define PLLD_BASE_DIVN_MASK		(0x7FF<<8)
#define PLLD_BASE_CSI_CLKENABLE		(1<<26)
#define PLLD_BASE_DSI_MUX_SHIFT		25
#define PLLD_BASE_DSI_MUX_MASK		(1<<PLLD_BASE_DSI_MUX_SHIFT)
#define PLLD_BASE_CSI_CLKSOURCE		(1<<23)

#define PLLD_MISC_DSI_CLKENABLE		(1<<30)
#define PLLD_MISC_DIV_RST		(1<<23)
#define PLLD_MISC_DCCON_SHIFT		12

#define PLLDU_LFCON			2

/* PLLC2 and PLLC3 (PLLCX) */
#define PLLCX_USE_DYN_RAMP		0
#define PLLCX_BASE_PHASE_LOCK		(1<<26)
#define PLLCX_BASE_DIVP_MASK		(0x7<<PLL_BASE_DIVP_SHIFT)
#define PLLCX_BASE_DIVN_MASK		(0xFF<<PLL_BASE_DIVN_SHIFT)
#define PLLCX_BASE_DIVM_MASK		(0x3<<PLL_BASE_DIVM_SHIFT)
#define PLLCX_PDIV_MAX	((PLLCX_BASE_DIVP_MASK >> PLL_BASE_DIVP_SHIFT))
#define PLLCX_IS_DYN(new_p, old_p)	(((new_p) <= 8) && ((old_p) <= 8))

#define PLLCX_MISC_STROBE		(1<<31)
#define PLLCX_MISC_RESET		(1<<30)
#define PLLCX_MISC_SDM_DIV_SHIFT	28
#define PLLCX_MISC_SDM_DIV_MASK		(0x3 << PLLCX_MISC_SDM_DIV_SHIFT)
#define PLLCX_MISC_FILT_DIV_SHIFT	26
#define PLLCX_MISC_FILT_DIV_MASK	(0x3 << PLLCX_MISC_FILT_DIV_SHIFT)
#define PLLCX_MISC_ALPHA_SHIFT		18
#define PLLCX_MISC_ALPHA_MASK		(0xFF << PLLCX_MISC_ALPHA_SHIFT)
#define PLLCX_MISC_KB_SHIFT		9
#define PLLCX_MISC_KB_MASK		(0x1FF << PLLCX_MISC_KB_SHIFT)
#define PLLCX_MISC_KA_SHIFT		2
#define PLLCX_MISC_KA_MASK		(0x7F << PLLCX_MISC_KA_SHIFT)
#define PLLCX_MISC_VCO_GAIN_SHIFT	0
#define PLLCX_MISC_VCO_GAIN_MASK	(0x3 << PLLCX_MISC_VCO_GAIN_SHIFT)

#define PLLCX_MISC_KOEF_LOW_RANGE	\
	((0x14 << PLLCX_MISC_KA_SHIFT) | (0x23 << PLLCX_MISC_KB_SHIFT))

#define PLLCX_MISC_DIV_LOW_RANGE	\
	((0x1 << PLLCX_MISC_SDM_DIV_SHIFT) | (0x1 << PLLCX_MISC_FILT_DIV_SHIFT))
#define PLLCX_MISC_DIV_HIGH_RANGE	\
	((0x2 << PLLCX_MISC_SDM_DIV_SHIFT) | (0x2 << PLLCX_MISC_FILT_DIV_SHIFT))
#define PLLCX_FD_ULCK_FRM_SHIFT		12
#define PLLCX_FD_ULCK_FRM_MASK		(0x3 << PLLCX_FD_ULCK_FRM_SHIFT)
#define PLLCX_FD_LCK_FRM_SHIFT		8
#define PLLCX_FD_LCK_FRM_MASK		(0x3 << PLLCX_FD_LCK_FRM_SHIFT)
#define PLLCX_PD_ULCK_FRM_SHIFT		28
#define PLLCX_PD_ULCK_FRM_MASK		(0x3 << PLLCX_PD_ULCK_FRM_SHIFT)
#define PLLCX_PD_LCK_FRM_SHIFT		24
#define PLLCX_PD_LCK_FRM_MASK		(0x3 << PLLCX_PD_LCK_FRM_SHIFT)
#define PLLCX_PD_OUT_HYST_SHIFT		20
#define PLLCX_PD_OUT_HYST_MASK		(0x3 << PLLCX_PD_OUT_HYST_SHIFT)
#define PLLCX_PD_IN_HYST_SHIFT		16
#define PLLCX_PD_IN_HYST_MASK		(0x3 << PLLCX_PD_IN_HYST_SHIFT)

#define PLLCX_MISC_DEFAULT_VALUE	((0x0 << PLLCX_MISC_VCO_GAIN_SHIFT) | \
					PLLCX_MISC_KOEF_LOW_RANGE | \
					(0x19 << PLLCX_MISC_ALPHA_SHIFT) | \
					PLLCX_MISC_DIV_LOW_RANGE | \
					PLLCX_MISC_RESET)
#define PLLCX_MISC1_DEFAULT_VALUE	0x000d2308
#define PLLCX_MISC2_DEFAULT_VALUE	((0x2 << PLLCX_PD_ULCK_FRM_SHIFT) | \
					(0x1 << PLLCX_PD_LCK_FRM_SHIFT)	  | \
					(0x3 << PLLCX_PD_OUT_HYST_SHIFT)  | \
					(0x1 << PLLCX_PD_IN_HYST_SHIFT)   | \
					(0x2 << PLLCX_FD_ULCK_FRM_SHIFT)  | \
					(0x2 << PLLCX_FD_LCK_FRM_SHIFT))
#define PLLCX_MISC3_DEFAULT_VALUE	0x200

#define PLLCX_MISC1_IDDQ		(0x1 << 27)

/* PLLX and PLLC (PLLXC)*/
#define PLLXC_USE_DYN_RAMP		0
#define PLLXC_BASE_DIVP_MASK		(0xF<<PLL_BASE_DIVP_SHIFT)
#define PLLXC_BASE_DIVN_MASK		(0xFF<<PLL_BASE_DIVN_SHIFT)
#define PLLXC_BASE_DIVM_MASK		(0xFF<<PLL_BASE_DIVM_SHIFT)

/* PLLXC has 4-bit PDIV, but entry 15 is not allowed in h/w,
   and s/w usage is limited to 5 */
#define PLLXC_PDIV_MAX			14
#define PLLXC_SW_PDIV_MAX		5

/* PLLX */
#define PLLX_MISC2_DYNRAMP_STEPB_SHIFT	24
#define PLLX_MISC2_DYNRAMP_STEPB_MASK	(0xFF << PLLX_MISC2_DYNRAMP_STEPB_SHIFT)
#define PLLX_MISC2_DYNRAMP_STEPA_SHIFT	16
#define PLLX_MISC2_DYNRAMP_STEPA_MASK	(0xFF << PLLX_MISC2_DYNRAMP_STEPA_SHIFT)
#define PLLX_MISC2_NDIV_NEW_SHIFT	8
#define PLLX_MISC2_NDIV_NEW_MASK	(0xFF << PLLX_MISC2_NDIV_NEW_SHIFT)
#define PLLX_MISC2_LOCK_OVERRIDE	(0x1 << 4)
#define PLLX_MISC2_DYNRAMP_DONE		(0x1 << 2)
#define PLLX_MISC2_CLAMP_NDIV		(0x1 << 1)
#define PLLX_MISC2_EN_DYNRAMP		(0x1 << 0)

#define PLLX_MISC3_IDDQ			(0x1 << 3)

#define PLLX_HW_CTRL_CFG		0x548
#define PLLX_HW_CTRL_CFG_SWCTRL		(0x1 << 0)

/* PLLC */
#define PLLC_BASE_LOCK_OVERRIDE		(1<<28)

#define PLLC_MISC_IDDQ			(0x1 << 26)
#define PLLC_MISC_LOCK_ENABLE		(0x1 << 24)

#define PLLC_MISC1_CLAMP_NDIV		(0x1 << 26)
#define PLLC_MISC1_EN_DYNRAMP		(0x1 << 25)
#define PLLC_MISC1_DYNRAMP_STEPA_SHIFT	17
#define PLLC_MISC1_DYNRAMP_STEPA_MASK	(0xFF << PLLC_MISC1_DYNRAMP_STEPA_SHIFT)
#define PLLC_MISC1_DYNRAMP_STEPB_SHIFT	9
#define PLLC_MISC1_DYNRAMP_STEPB_MASK	(0xFF << PLLC_MISC1_DYNRAMP_STEPB_SHIFT)
#define PLLC_MISC1_NDIV_NEW_SHIFT	1
#define PLLC_MISC1_NDIV_NEW_MASK	(0xFF << PLLC_MISC1_NDIV_NEW_SHIFT)
#define PLLC_MISC1_DYNRAMP_DONE		(0x1 << 0)

/* PLLM */
#define PLLM_BASE_DIVP_MASK		(0xF << PLL_BASE_DIVP_SHIFT)
#define PLLM_BASE_DIVN_MASK		(0xFF << PLL_BASE_DIVN_SHIFT)
#define PLLM_BASE_DIVM_MASK		(0xFF << PLL_BASE_DIVM_SHIFT)

/* PLLM has 4-bit PDIV, but entry 15 is not allowed in h/w,
   and s/w usage is limited to 5 */
#define PLLM_PDIV_MAX			14
#define PLLM_SW_PDIV_MAX		5

#define PLLM_MISC_FSM_SW_OVERRIDE	(0x1 << 10)
#define PLLM_MISC_IDDQ			(0x1 << 5)
#define PLLM_MISC_LOCK_DISABLE		(0x1 << 4)
#define PLLM_MISC_LOCK_OVERRIDE		(0x1 << 3)

#define PMC_PLLP_WB0_OVERRIDE			0xf8
#define PMC_PLLP_WB0_OVERRIDE_PLLM_ENABLE	(1 << 12)
#define PMC_PLLP_WB0_OVERRIDE_PLLM_OVERRIDE	(1 << 11)

/* M, N layout for PLLM override and base registers are the same */
#define PMC_PLLM_WB0_OVERRIDE			0x1dc

#define PMC_PLLM_WB0_OVERRIDE_2			0x2b0
#define PMC_PLLM_WB0_OVERRIDE_2_DIVP_SHIFT	27
#define PMC_PLLM_WB0_OVERRIDE_2_DIVP_MASK	(0xF << 27)

/* PLLSS */
#define PLLSS_CFG(c)	((c)->u.pll.misc1 + 0)
#define PLLSS_CTRL1(c)	((c)->u.pll.misc1 + 4)
#define PLLSS_CTRL2(c)	((c)->u.pll.misc1 + 8)

#define PLLSS_BASE_DIVP_MASK		(0xF << PLL_BASE_DIVP_SHIFT)
#define PLLSS_BASE_DIVN_MASK		(0xFF << PLL_BASE_DIVN_SHIFT)
#define PLLSS_BASE_DIVM_MASK		(0xFF << PLL_BASE_DIVM_SHIFT)
#define PLLSS_BASE_SOURCE_SHIFT		25
#define PLLSS_BASE_SOURCE_MASK		(3 << PLLSS_BASE_SOURCE_SHIFT)

/* PLLSS has 4-bit PDIV, but entry 15 is not allowed in h/w,
   and s/w usage is limited to 5 */
#define PLLSS_PDIV_MAX			14
#define PLLSS_SW_PDIV_MAX		5

#define PLLSS_MISC_LOCK_ENABLE		(0x1 << 30)

#define PLLSS_BASE_LOCK_OVERRIDE	(0x1 << 24)
#define PLLSS_BASE_LOCK			(0x1 << 27)
#define PLLSS_BASE_IDDQ			(0x1 << 19)

#define PLLSS_MISC_DEFAULT_VALUE ( \
	(PLLSS_MISC_KCP	<< 25) | \
	(PLLSS_MISC_KVCO << 24) | \
	(PLLSS_MISC_SETUP << 0))
#define PLLSS_CFG_DEFAULT_VALUE ( \
	(PLLSS_EN_SDM << 31) | \
	(PLLSS_EN_SSC << 30) | \
	(PLLSS_EN_DITHER2 << 29) | \
	(PLLSS_EN_DITHER << 28) | \
	(PLLSS_SDM_RESET << 27) | \
	(PLLSS_CLAMP << 22))
#define PLLSS_CTRL1_DEFAULT_VALUE \
	((PLLSS_SDM_SSC_MAX << 16) | (PLLSS_SDM_SSC_MIN << 0))
#define PLLSS_CTRL2_DEFAULT_VALUE \
	((PLLSS_SDM_SSC_STEP << 16) | (PLLSS_SDM_DIN << 0))

/* PLLSS configuration */
#define PLLSS_MISC_KCP			0
#define PLLSS_MISC_KVCO			0
#define PLLSS_MISC_SETUP		0
#define PLLSS_EN_SDM			0
#define PLLSS_EN_SSC			0
#define PLLSS_EN_DITHER2		0
#define PLLSS_EN_DITHER			1
#define PLLSS_SDM_RESET			0
#define PLLSS_CLAMP			0
#define PLLSS_SDM_SSC_MAX		0
#define PLLSS_SDM_SSC_MIN		0
#define PLLSS_SDM_SSC_STEP		0
#define PLLSS_SDM_DIN			0

/* PLLRE */
#define PLLRE_BASE_DIVP_SHIFT		16
#define PLLRE_BASE_DIVP_MASK		(0xF << PLLRE_BASE_DIVP_SHIFT)
#define PLLRE_BASE_DIVN_MASK		(0xFF << PLL_BASE_DIVN_SHIFT)
#define PLLRE_BASE_DIVM_MASK		(0xFF << PLL_BASE_DIVM_SHIFT)

/* PLLRE has 4-bit PDIV, but entry 15 is not allowed in h/w,
   and s/w usage is limited to 5 */
#define PLLRE_PDIV_MAX			14
#define PLLRE_SW_PDIV_MAX		5

#define PLLRE_MISC_LOCK_ENABLE		(0x1 << 30)
#define PLLRE_MISC_LOCK_OVERRIDE	(0x1 << 29)
#define PLLRE_MISC_LOCK			(0x1 << 24)
#define PLLRE_MISC_IDDQ			(0x1 << 16)

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
#define SUPER_CLOCK_DIV_U71_SHIFT	16
#define SUPER_CLOCK_DIV_U71_MASK	(0xff << SUPER_CLOCK_DIV_U71_SHIFT)

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

#define UTMIP_PLL_CFG2					0x488
#define UTMIP_PLL_CFG2_STABLE_COUNT(x)			(((x) & 0xfff) << 6)
#define UTMIP_PLL_CFG2_ACTIVE_DLY_COUNT(x)		(((x) & 0x3f) << 18)
#define UTMIP_PLL_CFG2_FORCE_PD_SAMP_A_POWERDOWN	(1 << 0)
#define UTMIP_PLL_CFG2_FORCE_PD_SAMP_A_POWERUP		(1 << 1)
#define UTMIP_PLL_CFG2_FORCE_PD_SAMP_B_POWERDOWN	(1 << 2)
#define UTMIP_PLL_CFG2_FORCE_PD_SAMP_B_POWERUP		(1 << 3)
#define UTMIP_PLL_CFG2_FORCE_PD_SAMP_C_POWERDOWN	(1 << 4)
#define UTMIP_PLL_CFG2_FORCE_PD_SAMP_C_POWERUP		(1 << 5)
#define UTMIP_PLL_CFG2_FORCE_PD_SAMP_D_POWERDOWN	(1 << 24)
#define UTMIP_PLL_CFG2_FORCE_PD_SAMP_D_POWERUP		(1 << 25)

#define UTMIP_PLL_CFG1					0x484
#define UTMIP_PLL_CFG1_ENABLE_DLY_COUNT(x)		(((x) & 0x1f) << 27)
#define UTMIP_PLL_CFG1_XTAL_FREQ_COUNT(x)		(((x) & 0xfff) << 0)
#define UTMIP_PLL_CFG1_FORCE_PLL_ENABLE_POWERUP	(1 << 15)
#define UTMIP_PLL_CFG1_FORCE_PLL_ENABLE_POWERDOWN	(1 << 14)
#define UTMIP_PLL_CFG1_FORCE_PLL_ACTIVE_POWERDOWN	(1 << 12)
#define UTMIP_PLL_CFG1_FORCE_PLLU_POWERUP		(1 << 17)
#define UTMIP_PLL_CFG1_FORCE_PLLU_POWERDOWN		(1 << 16)

/* PLLE */
#define PLLE_BASE_LOCK_OVERRIDE		(0x1 << 29)
#define PLLE_BASE_DIVCML_SHIFT		24
#define PLLE_BASE_DIVCML_MASK		(0xf<<PLLE_BASE_DIVCML_SHIFT)

#define PLLE_BASE_DIVN_MASK		(0xFF<<PLL_BASE_DIVN_SHIFT)
#define PLLE_BASE_DIVM_MASK		(0xFF<<PLL_BASE_DIVM_SHIFT)

/* PLLE has 4-bit CMLDIV, but entry 15 is not allowed in h/w */
#define PLLE_CMLDIV_MAX			14
#define PLLE_MISC_READY			(1<<15)
#define PLLE_MISC_IDDQ_SW_CTRL		(1<<14)
#define PLLE_MISC_IDDQ_SW_VALUE		(1<<13)
#define PLLE_MISC_LOCK			(1<<11)
#define PLLE_MISC_LOCK_ENABLE		(1<<9)
#define PLLE_MISC_PLLE_PTS		(1<<8)
#define PLLE_MISC_VREG_BG_CTRL_SHIFT	4
#define PLLE_MISC_VREG_BG_CTRL_MASK	(0x3<<PLLE_MISC_VREG_BG_CTRL_SHIFT)
#define PLLE_MISC_VREG_CTRL_SHIFT	2
#define PLLE_MISC_VREG_CTRL_MASK	(0x3<<PLLE_MISC_VREG_CTRL_SHIFT)

#define PLLE_SS_CTRL			0x68
#define	PLLE_SS_INCINTRV_SHIFT		24
#define	PLLE_SS_INCINTRV_MASK		(0x3f<<PLLE_SS_INCINTRV_SHIFT)
#define	PLLE_SS_INC_SHIFT		16
#define	PLLE_SS_INC_MASK		(0xff<<PLLE_SS_INC_SHIFT)
#define	PLLE_SS_CNTL_INVERT		(0x1 << 15)
#define	PLLE_SS_CNTL_CENTER		(0x1 << 14)
#define	PLLE_SS_CNTL_SSC_BYP		(0x1 << 12)
#define	PLLE_SS_CNTL_INTERP_RESET	(0x1 << 11)
#define	PLLE_SS_CNTL_BYPASS_SS		(0x1 << 10)
#define	PLLE_SS_MAX_SHIFT		0
#define	PLLE_SS_MAX_MASK		(0x1ff<<PLLE_SS_MAX_SHIFT)
#define PLLE_SS_COEFFICIENTS_MASK	\
	(PLLE_SS_INCINTRV_MASK | PLLE_SS_INC_MASK | PLLE_SS_MAX_MASK)
#define PLLE_SS_COEFFICIENTS_VAL	\
	((0x20<<PLLE_SS_INCINTRV_SHIFT) | (0x1<<PLLE_SS_INC_SHIFT) | \
	 (0x25<<PLLE_SS_MAX_SHIFT))
#define PLLE_SS_DISABLE			(PLLE_SS_CNTL_SSC_BYP |\
	PLLE_SS_CNTL_INTERP_RESET | PLLE_SS_CNTL_BYPASS_SS)

#define PLLE_AUX			0x48c
#define PLLE_AUX_PLLRE_SEL		(1<<28)
#define PLLE_AUX_SEQ_STATE_SHIFT	26
#define PLLE_AUX_SEQ_STATE_MASK		(0x3<<PLLE_AUX_SEQ_STATE_SHIFT)
#define PLLE_AUX_SEQ_START_STATE	(1<<25)
#define PLLE_AUX_SEQ_ENABLE		(1<<24)
#define PLLE_AUX_SS_SWCTL		(1<<6)
#define PLLE_AUX_ENABLE_SWCTL		(1<<4)
#define PLLE_AUX_USE_LOCKDET		(1<<3)
#define PLLE_AUX_PLLP_SEL		(1<<2)

#define PLLE_AUX_CML_SATA_ENABLE	(1<<1)
#define PLLE_AUX_CML_PCIE_ENABLE	(1<<0)

/* USB PLLs PD HW controls */
#define XUSBIO_PLL_CFG0				0x51c
#define XUSBIO_PLL_CFG0_SEQ_START_STATE		(1<<25)
#define XUSBIO_PLL_CFG0_SEQ_ENABLE		(1<<24)
#define XUSBIO_PLL_CFG0_PADPLL_USE_LOCKDET	(1<<6)
#define XUSBIO_PLL_CFG0_CLK_ENABLE_SWCTL	(1<<2)
#define XUSBIO_PLL_CFG0_PADPLL_RESET_SWCTL	(1<<0)

#define UTMIPLL_HW_PWRDN_CFG0			0x52c
#define UTMIPLL_HW_PWRDN_CFG0_SEQ_START_STATE	(1<<25)
#define UTMIPLL_HW_PWRDN_CFG0_SEQ_ENABLE	(1<<24)
#define UTMIPLL_HW_PWRDN_CFG0_USE_LOCKDET	(1<<6)
#define UTMIPLL_HW_PWRDN_CFG0_SEQ_RESET_INPUT_VALUE	(1<<5)
#define UTMIPLL_HW_PWRDN_CFG0_SEQ_IN_SWCTL	(1<<4)
#define UTMIPLL_HW_PWRDN_CFG0_CLK_ENABLE_SWCTL	(1<<2)
#define UTMIPLL_HW_PWRDN_CFG0_IDDQ_OVERRIDE	(1<<1)
#define UTMIPLL_HW_PWRDN_CFG0_IDDQ_SWCTL	(1<<0)

#define PLLU_HW_PWRDN_CFG0			0x530
#define PLLU_HW_PWRDN_CFG0_SEQ_START_STATE	(1<<25)
#define PLLU_HW_PWRDN_CFG0_SEQ_ENABLE		(1<<24)
#define PLLU_HW_PWRDN_CFG0_USE_LOCKDET		(1<<6)
#define PLLU_HW_PWRDN_CFG0_CLK_ENABLE_SWCTL	(1<<2)
#define PLLU_HW_PWRDN_CFG0_CLK_SWITCH_SWCTL	(1<<0)

#define USB_PLLS_SEQ_START_STATE		(1<<25)
#define USB_PLLS_SEQ_ENABLE			(1<<24)
#define USB_PLLS_USE_LOCKDET			(1<<6)
#define USB_PLLS_ENABLE_SWCTL			((1<<2) | (1<<0))

/* XUSB PLL PAD controls */
#define XUSB_PADCTL_IOPHY_PLL_P0_CTL1_0         0x40
#define XUSB_PADCTL_IOPHY_PLL_P0_CTL1_0_PLL_PWR_OVRD    (1<<3)
#define XUSB_PADCTL_IOPHY_PLL_P0_CTL1_0_PLL_IDDQ        (1<<0)


/* DFLL */
#define DFLL_BASE				0x2f4
#define DFLL_BASE_RESET				(1<<0)

#define	LVL2_CLK_GATE_OVRE			0x554

#define ROUND_DIVIDER_UP	0
#define ROUND_DIVIDER_DOWN	1
#define DIVIDER_1_5_ALLOWED	0

/* Tegra CPU clock and reset control regs */
#define TEGRA_CLK_RST_CONTROLLER_CLK_CPU_CMPLX		0x4c
#define TEGRA_CLK_RST_CONTROLLER_RST_CPU_CMPLX_SET	0x340
#define TEGRA_CLK_RST_CONTROLLER_RST_CPU_CMPLX_CLR	0x344
#define TEGRA30_CLK_RST_CONTROLLER_CLK_CPU_CMPLX_CLR	0x34c
#define TEGRA30_CLK_RST_CONTROLLER_CPU_CMPLX_STATUS	0x470

#define CPU_CLOCK(cpu)	(0x1 << (8 + cpu))
#define CPU_RESET(cpu)	(0x111001ul << (cpu))

/* PLLP default fixed rate in h/w controlled mode */
#define PLLP_DEFAULT_FIXED_RATE		408000000

/* Use PLL_RE as PLLE input (default - OSC via pll reference divider) */
#define USE_PLLE_INPUT_PLLRE    0

static bool tegra12_is_dyn_ramp(struct clk *c,
				unsigned long rate, bool from_vco_min);
static void tegra12_pllp_init_dependencies(unsigned long pllp_rate);
static unsigned long tegra12_clk_shared_bus_update(struct clk *bus,
	struct clk **bus_top, struct clk **bus_slow, unsigned long *rate_cap);
static unsigned long tegra12_clk_cap_shared_bus(struct clk *bus,
	unsigned long rate, unsigned long ceiling);

static bool detach_shared_bus;
module_param(detach_shared_bus, bool, 0644);

/* Defines default range for dynamic frequency lock loop (DFLL)
   to be used as CPU clock source:
   "0" - DFLL is not used,
   "1" - DFLL is used as a source for all CPU rates
   "2" - DFLL is used only for high rates above crossover with PLL dvfs curve
*/
static int use_dfll;

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
/*	OSC_FREQUENCY,	ENABLE_DLY,	STABLE_CNT,	ACTIVE_DLY,	XTAL_FREQ_CNT */
	{13000000,	0x02,		0x33,		0x05,		0x7F},
	{19200000,	0x03,		0x4B,		0x06,		0xBB},
	{12000000,	0x02,		0x2F,		0x04,		0x76},
	{26000000,	0x04,		0x66,		0x09,		0xFE},
	{16800000,	0x03,		0x41,		0x0A,		0xA4},
};

static void __iomem *reg_clk_base = IO_ADDRESS(TEGRA_CLK_RESET_BASE);
static void __iomem *reg_pmc_base = IO_ADDRESS(TEGRA_PMC_BASE);
static void __iomem *misc_gp_base = IO_ADDRESS(TEGRA_APB_MISC_BASE);
static void __iomem *reg_xusb_padctl_base = IO_ADDRESS(TEGRA_XUSB_PADCTL_BASE);

#define MISC_GP_TRANSACTOR_SCRATCH_0		0x864
#define MISC_GP_TRANSACTOR_SCRATCH_LA_ENABLE	(0x1 << 1)
#define MISC_GP_TRANSACTOR_SCRATCH_DDS_ENABLE	(0x1 << 2)
#define MISC_GP_TRANSACTOR_SCRATCH_DP2_ENABLE	(0x1 << 3)

/*
 * Some peripheral clocks share an enable bit, so refcount the enable bits
 * in registers CLK_ENABLE_L, ... CLK_ENABLE_W, and protect refcount updates
 * with lock
 */
static DEFINE_SPINLOCK(periph_refcount_lock);
static int tegra_periph_clk_enable_refcount[CLK_OUT_ENB_NUM * 32];

#define clk_writel(value, reg) \
	__raw_writel(value, (void *)((u32)reg_clk_base + (reg)))
#define clk_readl(reg) \
	__raw_readl((void *)((u32)reg_clk_base + (reg)))
#define pmc_writel(value, reg) \
	__raw_writel(value,(void *)((u32)reg_pmc_base + (reg)))
#define pmc_readl(reg) \
	__raw_readl((void *)((u32)reg_pmc_base + (reg)))
#define xusb_padctl_writel(value, reg) \
	 __raw_writel(value, reg_xusb_padctl_base + (reg))
#define xusb_padctl_readl(reg) \
	__raw_readl(reg_xusb_padctl_base + (reg))

#define clk_writel_delay(value, reg) 					\
	do {								\
		__raw_writel((value), reg_clk_base + (reg));		\
		__raw_readl(reg_clk_base + (reg));			\
		udelay(2);						\
	} while (0)

#define pll_writel_delay(value, reg)					\
	do {								\
		__raw_writel((value), reg_clk_base + (reg));		\
		__raw_readl(reg_clk_base + (reg));			\
		udelay(1);						\
	} while (0)


static inline int clk_set_div(struct clk *c, u32 n)
{
	return clk_set_rate(c, (clk_get_rate(c->parent) + n-1) / n);
}

static inline u32 periph_clk_to_reg(
	struct clk *c, u32 reg_L, u32 reg_V, u32 reg_X, int offs)
{
	u32 reg = c->u.periph.clk_num / 32;
	BUG_ON(reg >= RST_DEVICES_NUM);
	if (reg < 3)
		reg = reg_L + (reg * offs);
	else if (reg < 5)
		reg = reg_V + ((reg - 3) * offs);
	else
		reg = reg_X;
	return reg;
}

static int clk_div_x1_get_divider(unsigned long parent_rate, unsigned long rate,
			u32 max_x,
				 u32 flags, u32 round_mode)
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

#if !DIVIDER_1_5_ALLOWED
	if (divider_ux1 == 3)
		divider_ux1 = (round_mode == ROUND_DIVIDER_UP) ? 4 : 2;
#endif
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

static inline bool bus_user_is_slower(struct clk *a, struct clk *b)
{
	return a->u.shared_bus_user.client->max_rate * a->div <
		b->u.shared_bus_user.client->max_rate * b->div;
}

static inline bool bus_user_request_is_lower(struct clk *a, struct clk *b)
{
	return a->u.shared_bus_user.rate * a->div <
		b->u.shared_bus_user.rate * b->div;
}

/* clk_m functions */
static unsigned long tegra12_clk_m_autodetect_rate(struct clk *c)
{
	u32 osc_ctrl = clk_readl(OSC_CTRL);
	u32 auto_clock_control = osc_ctrl & ~OSC_CTRL_OSC_FREQ_MASK;
	u32 pll_ref_div = osc_ctrl & OSC_CTRL_PLL_REF_DIV_MASK;

	u32 spare = clk_readl(SPARE_REG);
	u32 divisor = (spare & SPARE_REG_CLK_M_DIVISOR_MASK)
		>> SPARE_REG_CLK_M_DIVISOR_SHIFT;
	u32 spare_update = spare & ~SPARE_REG_CLK_M_DIVISOR_MASK;

	c->rate = tegra_clk_measure_input_freq();
	switch (c->rate) {
	case 12000000:
		auto_clock_control |= OSC_CTRL_OSC_FREQ_12MHZ;
		BUG_ON(pll_ref_div != OSC_CTRL_PLL_REF_DIV_1);
		BUG_ON(divisor != 0);
		break;
	case 13000000:
		auto_clock_control |= OSC_CTRL_OSC_FREQ_13MHZ;
		BUG_ON(pll_ref_div != OSC_CTRL_PLL_REF_DIV_1);
		BUG_ON(divisor != 0);
		break;
	case 19200000:
		auto_clock_control |= OSC_CTRL_OSC_FREQ_19_2MHZ;
		BUG_ON(pll_ref_div != OSC_CTRL_PLL_REF_DIV_1);
		BUG_ON(divisor != 0);
		break;
	case 26000000:
		auto_clock_control |= OSC_CTRL_OSC_FREQ_26MHZ;
		BUG_ON(pll_ref_div != OSC_CTRL_PLL_REF_DIV_1);
		BUG_ON(divisor != 0);
		break;
	case 16800000:
		auto_clock_control |= OSC_CTRL_OSC_FREQ_16_8MHZ;
		BUG_ON(pll_ref_div != OSC_CTRL_PLL_REF_DIV_1);
		BUG_ON(divisor != 0);
		break;
	case 38400000:
		auto_clock_control |= OSC_CTRL_OSC_FREQ_38_4MHZ;
		BUG_ON(pll_ref_div != OSC_CTRL_PLL_REF_DIV_2);
		BUG_ON(divisor != 1);
		spare_update |= (1 << SPARE_REG_CLK_M_DIVISOR_SHIFT);
		break;
	case 48000000:
		auto_clock_control |= OSC_CTRL_OSC_FREQ_48MHZ;
		BUG_ON(pll_ref_div != OSC_CTRL_PLL_REF_DIV_4);
		BUG_ON(divisor != 3);
		spare_update |= (3 << SPARE_REG_CLK_M_DIVISOR_SHIFT);
		break;
	case 115200:	/* fake 13M for QT */
	case 230400:	/* fake 13M for QT */
		auto_clock_control |= OSC_CTRL_OSC_FREQ_13MHZ;
		c->rate = 13000000;
		BUG_ON(pll_ref_div != OSC_CTRL_PLL_REF_DIV_1);
		BUG_ON(divisor != 0);
		break;
	default:
		pr_err("%s: Unexpected clock rate %ld", __func__, c->rate);
		BUG();
	}

	clk_writel(auto_clock_control, OSC_CTRL);
	clk_writel(spare_update, SPARE_REG);

	return c->rate;
}

static void tegra12_clk_m_init(struct clk *c)
{
	pr_debug("%s on clock %s\n", __func__, c->name);
	tegra12_clk_m_autodetect_rate(c);
}

static int tegra12_clk_m_enable(struct clk *c)
{
	pr_debug("%s on clock %s\n", __func__, c->name);
	return 0;
}

static void tegra12_clk_m_disable(struct clk *c)
{
	pr_debug("%s on clock %s\n", __func__, c->name);
	WARN(1, "Attempting to disable main SoC clock\n");
}

static struct clk_ops tegra_clk_m_ops = {
	.init		= tegra12_clk_m_init,
	.enable		= tegra12_clk_m_enable,
	.disable	= tegra12_clk_m_disable,
};

static struct clk_ops tegra_clk_m_div_ops = {
	.enable		= tegra12_clk_m_enable,
};

/* PLL reference divider functions */
static void tegra12_pll_ref_init(struct clk *c)
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
	.init		= tegra12_pll_ref_init,
	.enable		= tegra12_clk_m_enable,
	.disable	= tegra12_clk_m_disable,
};

/* super clock functions */
/* "super clocks" on tegra12x have two-stage muxes, fractional 7.1 divider and
 * clock skipping super divider.  We will ignore the clock skipping divider,
 * since we can't lower the voltage when using the clock skip, but we can if
 * we lower the PLL frequency. Note that skipping divider can and will be used
 * by thermal control h/w for automatic throttling. There is also a 7.1 divider
 * that most CPU super-clock inputs can be routed through. We will not use it
 * as well (keep default 1:1 state), to avoid high jitter on PLLX and DFLL path
 * and possible concurrency access issues with thermal h/w (7.1 divider setting
 * share register with clock skipping divider)
 */
static void tegra12_super_clk_init(struct clk *c)
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

	/*
	 * Enforce PLLX DIV2 bypass setting as early as possible. It is always
	 * safe to do for both cclk_lp and cclk_g when booting on G CPU. (In
	 * case of booting on LP CPU, cclk_lp will be updated during the cpu
	 * rate change after boot, and cclk_g after the cluster switch.)
	 */
	if ((c->flags & DIV_U71) && (!is_lp_cluster())) {
		val |= SUPER_LP_DIV2_BYPASS;
		clk_writel_delay(val, c->reg);
	}

	for (sel = c->inputs; sel->input != NULL; sel++) {
		if (sel->value == source)
			break;
	}
	BUG_ON(sel->input == NULL);
	c->parent = sel->input;

	/* Update parent in case when LP CPU PLLX DIV2 bypassed */
	if ((c->flags & DIV_2) && (c->parent->flags & PLLX) &&
	    (val & SUPER_LP_DIV2_BYPASS))
		c->parent = c->parent->parent;

	/* Update parent in case when LP CPU PLLX DIV2 bypassed */
	if ((c->flags & DIV_2) && (c->parent->flags & PLLX) &&
	    (val & SUPER_LP_DIV2_BYPASS))
		c->parent = c->parent->parent;

	if (c->flags & DIV_U71) {
		c->mul = 2;
		c->div = 2;

		/*
		 * Make sure 7.1 divider is 1:1; clear h/w skipper control -
		 * it will be enabled by soctherm later
		 */
		val = clk_readl(c->reg + SUPER_CLK_DIVIDER);
		BUG_ON(val & SUPER_CLOCK_DIV_U71_MASK);
		val = 0;
		clk_writel(val, c->reg + SUPER_CLK_DIVIDER);
	}
	else
		clk_writel(0, c->reg + SUPER_CLK_DIVIDER);
}

static int tegra12_super_clk_enable(struct clk *c)
{
	return 0;
}

static void tegra12_super_clk_disable(struct clk *c)
{
	/* since tegra 3 has 2 CPU super clocks - low power lp-mode clock and
	   geared up g-mode super clock - mode switch may request to disable
	   either of them; accept request with no affect on h/w */
}

static int tegra12_super_clk_set_parent(struct clk *c, struct clk *p)
{
	u32 val;
	const struct clk_mux_sel *sel;
	int shift;

	val = clk_readl(c->reg + SUPER_CLK_MUX);
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

			if (c->flags & DIV_U71) {
				/* Make sure 7.1 divider is 1:1 */
				u32 div = clk_readl(c->reg + SUPER_CLK_DIVIDER);
				BUG_ON(div & SUPER_CLOCK_DIV_U71_MASK);
			}

			if (c->refcnt)
				clk_enable(p);

			clk_writel_delay(val, c->reg);

			if (c->refcnt && c->parent)
				clk_disable(c->parent);

			clk_reparent(c, p);
			return 0;
		}
	}
	return -EINVAL;
}

/*
 * Do not use super clocks "skippers", since dividing using a clock skipper
 * does not allow the voltage to be scaled down. Instead adjust the rate of
 * the parent clock. This requires that the parent of a super clock have no
 * other children, otherwise the rate will change underneath the other
 * children.
 */
static int tegra12_super_clk_set_rate(struct clk *c, unsigned long rate)
{
	/* In tegra12_cpu_clk_set_plls() and  tegra12_sbus_cmplx_set_rate()
	 * this call is skipped by directly setting rate of source plls. If we
	 * ever use 7.1 divider at other than 1:1 setting, or exercise s/w
	 * skipper control, not only this function, but cpu and sbus set_rate
	 * APIs should be changed accordingly.
	 */
	return clk_set_rate(c->parent, rate);
}

#ifdef CONFIG_PM_SLEEP
static void tegra12_super_clk_resume(struct clk *c, struct clk *backup,
				     u32 setting)
{
	u32 val;
	const struct clk_mux_sel *sel;
	int shift;

	/* For sclk and cclk_g super clock just restore saved value */
	if (!(c->flags & DIV_2)) {
		clk_writel_delay(setting, c->reg);
		return;
	}

	/*
	 * For cclk_lp supper clock: switch to backup (= not PLLX) source,
	 * safely restore PLLX DIV2 bypass, and only then restore full
	 * setting
	 */
	val = clk_readl(c->reg);
	BUG_ON(((val & SUPER_STATE_MASK) != SUPER_STATE_RUN) &&
		((val & SUPER_STATE_MASK) != SUPER_STATE_IDLE));
	shift = ((val & SUPER_STATE_MASK) == SUPER_STATE_IDLE) ?
		SUPER_IDLE_SOURCE_SHIFT : SUPER_RUN_SOURCE_SHIFT;
	for (sel = c->inputs; sel->input != NULL; sel++) {
		if (sel->input == backup) {
			val &= ~(SUPER_SOURCE_MASK << shift);
			val |= (sel->value & SUPER_SOURCE_MASK) << shift;

			BUG_ON(backup->flags & PLLX);
			clk_writel_delay(val, c->reg);

			val &= ~SUPER_LP_DIV2_BYPASS;
			val |= (setting & SUPER_LP_DIV2_BYPASS);
			clk_writel_delay(val, c->reg);
			clk_writel_delay(setting, c->reg);
			return;
		}
	}
	BUG();
}
#endif

static struct clk_ops tegra_super_ops = {
	.init			= tegra12_super_clk_init,
	.enable			= tegra12_super_clk_enable,
	.disable		= tegra12_super_clk_disable,
	.set_parent		= tegra12_super_clk_set_parent,
	.set_rate		= tegra12_super_clk_set_rate,
};

/* virtual cpu clock functions */
/* some clocks can not be stopped (cpu, memory bus) while the SoC is running.
   To change the frequency of these clocks, the parent pll may need to be
   reprogrammed, so the clock must be moved off the pll, the pll reprogrammed,
   and then the clock moved back to the pll.  To hide this sequence, a virtual
   clock handles it.
 */
static void tegra12_cpu_clk_init(struct clk *c)
{
	c->state = (!is_lp_cluster() == (c->u.cpu.mode == MODE_G))? ON : OFF;
}

static int tegra12_cpu_clk_enable(struct clk *c)
{
	return 0;
}

static void tegra12_cpu_clk_disable(struct clk *c)
{
	/* since tegra 3 has 2 virtual CPU clocks - low power lp-mode clock
	   and geared up g-mode clock - mode switch may request to disable
	   either of them; accept request with no affect on h/w */
}

static int tegra12_cpu_clk_set_plls(struct clk *c, unsigned long rate,
				    unsigned long old_rate)
{
	int ret = 0;
	bool on_main = false;
	unsigned long backup_rate, main_rate;
	unsigned long vco_min = c->u.cpu.main->u.pll.vco_min;

	/*
	 * Take an extra reference to the main pll so it doesn't turn off when
	 * we move the cpu off of it. If possible, use main pll dynamic ramp
	 * to reach target rate in one shot. Otherwise, use dynamic ramp to
	 * lower current rate to pll VCO minimum level before switching to
	 * backup source.
	 */
	if (c->parent->parent == c->u.cpu.main) {
		bool dramp = (rate > c->u.cpu.backup_rate) &&
			tegra12_is_dyn_ramp(c->u.cpu.main, rate, false);
		clk_enable(c->u.cpu.main);
		on_main = true;

		if (dramp ||
		    ((old_rate > vco_min) &&
		     tegra12_is_dyn_ramp(c->u.cpu.main, vco_min, false))) {
			main_rate = dramp ? rate : vco_min;
			ret = clk_set_rate(c->u.cpu.main, main_rate);
			if (ret) {
				pr_err("Failed to set cpu rate %lu on source"
				       " %s\n", main_rate, c->u.cpu.main->name);
				goto out;
			}
			if (dramp)
				goto out;
		} else if (old_rate > vco_min) {
#if PLLXC_USE_DYN_RAMP
			pr_warn("No dynamic ramp down: %s: %lu to %lu\n",
				c->u.cpu.main->name, old_rate, vco_min);
#endif
		}
	}

	/* Switch to back-up source, and stay on it if target rate is below
	   backup rate */
	if (c->parent->parent != c->u.cpu.backup) {
		ret = clk_set_parent(c->parent, c->u.cpu.backup);
		if (ret) {
			pr_err("Failed to switch cpu to %s\n",
			       c->u.cpu.backup->name);
			goto out;
		}
	}

	backup_rate = min(rate, c->u.cpu.backup_rate);
	if (backup_rate != clk_get_rate_locked(c)) {
		ret = clk_set_rate(c->u.cpu.backup, backup_rate);
		if (ret) {
			pr_err("Failed to set cpu rate %lu on backup source\n",
			       backup_rate);
			goto out;
		}
	}
	if (rate == backup_rate)
		goto out;

	/* Switch from backup source to main at rate not exceeding pll VCO
	   minimum. Use dynamic ramp to reach target rate if it is above VCO
	   minimum. */
	main_rate = rate;
	if (rate > vco_min) {
		if (tegra12_is_dyn_ramp(c->u.cpu.main, rate, true))
			main_rate = vco_min;
#if PLLXC_USE_DYN_RAMP
		else
			pr_warn("No dynamic ramp up: %s: %lu to %lu\n",
				c->u.cpu.main->name, vco_min, rate);
#endif
	}

	ret = clk_set_rate(c->u.cpu.main, main_rate);
	if (ret) {
		pr_err("Failed to set cpu rate %lu on source"
		       " %s\n", main_rate, c->u.cpu.main->name);
		goto out;
	}
	ret = clk_set_parent(c->parent, c->u.cpu.main);
	if (ret) {
		pr_err("Failed to switch cpu to %s\n", c->u.cpu.main->name);
		goto out;
	}
	if (rate != main_rate) {
		ret = clk_set_rate(c->u.cpu.main, rate);
		if (ret) {
			pr_err("Failed to set cpu rate %lu on source"
			       " %s\n", rate, c->u.cpu.main->name);
			goto out;
		}
	}

out:
	if (on_main)
		clk_disable(c->u.cpu.main);

	return ret;
}

static int tegra12_cpu_clk_dfll_on(struct clk *c, unsigned long rate,
				   unsigned long old_rate)
{
	int ret;
	struct clk *dfll = c->u.cpu.dynamic;
	unsigned long dfll_rate_min = c->dvfs->dfll_data.use_dfll_rate_min;

	/* dfll rate request */
	ret = clk_set_rate(dfll, rate);
	if (ret) {
		pr_err("Failed to set cpu rate %lu on source"
		       " %s\n", rate, dfll->name);
		return ret;
	}

	/* 1st time - switch to dfll */
	if (c->parent->parent != dfll) {
		if (max(old_rate, rate) < dfll_rate_min) {
			/* set interim cpu dvfs rate at dfll_rate_min to
			   prevent voltage drop below dfll Vmin */
			ret = tegra_dvfs_set_rate(c, dfll_rate_min);
			if (ret) {
				pr_err("Failed to set cpu dvfs rate %lu\n",
				       dfll_rate_min);
				return ret;
			}
		}

		tegra_dvfs_rail_mode_updating(tegra_cpu_rail, true);
		ret = clk_set_parent(c->parent, dfll);
		if (ret) {
			tegra_dvfs_rail_mode_updating(tegra_cpu_rail, false);
			pr_err("Failed to switch cpu to %s\n", dfll->name);
			return ret;
		}
		ret = tegra_clk_cfg_ex(dfll, TEGRA_CLK_DFLL_LOCK, 1);
		WARN(ret, "Failed to lock %s at rate %lu\n", dfll->name, rate);

		/* prevent legacy dvfs voltage scaling */
		tegra_dvfs_dfll_mode_set(c->dvfs, rate);
		tegra_dvfs_rail_mode_updating(tegra_cpu_rail, false);
	}
	return 0;
}

static int tegra12_cpu_clk_dfll_off(struct clk *c, unsigned long rate,
				    unsigned long old_rate)
{
	int ret;
	struct clk *pll;
	struct clk *dfll = c->u.cpu.dynamic;
	unsigned long dfll_rate_min = c->dvfs->dfll_data.use_dfll_rate_min;

	rate = min(rate, c->max_rate - c->dvfs->dfll_data.max_rate_boost);
	pll = (rate <= c->u.cpu.backup_rate) ? c->u.cpu.backup : c->u.cpu.main;
	dfll_rate_min = max(rate, dfll_rate_min);

	/* set target rate last time in dfll mode */
	if (old_rate != dfll_rate_min) {
		ret = tegra_dvfs_set_rate(c, dfll_rate_min);
		if (!ret)
			ret = clk_set_rate(dfll, dfll_rate_min);

		if (ret) {
			pr_err("Failed to set cpu rate %lu on source %s\n",
			       dfll_rate_min, dfll->name);
			return ret;
		}
	}

	/* unlock dfll - release volatge rail control */
	tegra_dvfs_rail_mode_updating(tegra_cpu_rail, true);
	ret = tegra_clk_cfg_ex(dfll, TEGRA_CLK_DFLL_LOCK, 0);
	if (ret) {
		pr_err("Failed to unlock %s\n", dfll->name);
		goto back_to_dfll;
	}

	/* restore legacy dvfs operations and set appropriate voltage */
	ret = tegra_dvfs_dfll_mode_clear(c->dvfs, dfll_rate_min);
	if (ret) {
		pr_err("Failed to set cpu rail for rate %lu\n", rate);
		goto back_to_dfll;
	}

	/* set pll to target rate and return to pll source */
	ret = clk_set_rate(pll, rate);
	if (ret) {
		pr_err("Failed to set cpu rate %lu on source"
		       " %s\n", rate, pll->name);
		goto back_to_dfll;
	}
	ret = clk_set_parent(c->parent, pll);
	if (ret) {
		pr_err("Failed to switch cpu to %s\n", pll->name);
		goto back_to_dfll;
	}

	/* If going up, adjust voltage here (down path is taken care of by the
	   framework after set rate exit) */
	if (old_rate <= rate)
		tegra_dvfs_set_rate(c, rate);

	tegra_dvfs_rail_mode_updating(tegra_cpu_rail, false);
	return 0;

back_to_dfll:
	tegra_clk_cfg_ex(dfll, TEGRA_CLK_DFLL_LOCK, 1);
	tegra_dvfs_dfll_mode_set(c->dvfs, old_rate);
	tegra_dvfs_rail_mode_updating(tegra_cpu_rail, false);
	return ret;
}

static int tegra12_cpu_clk_set_rate(struct clk *c, unsigned long rate)
{
	unsigned long old_rate = clk_get_rate_locked(c);
	bool has_dfll = c->u.cpu.dynamic &&
		(c->u.cpu.dynamic->state != UNINITIALIZED);
	bool is_dfll = c->parent->parent == c->u.cpu.dynamic;

	/* On SILICON allow CPU rate change only if cpu regulator is connected.
	   Ignore regulator connection on FPGA and SIMULATION platforms. */
	if (c->dvfs && tegra_platform_is_silicon()) {
		if (!c->dvfs->dvfs_rail)
			return -ENOSYS;
		else if ((!c->dvfs->dvfs_rail->reg) && (old_rate < rate) &&
			 (c->boot_rate < rate)) {
			WARN(1, "Increasing CPU rate while regulator is not"
				" ready is not allowed\n");
			return -ENOSYS;
		}
	}
	if (has_dfll && c->dvfs && c->dvfs->dvfs_rail) {
		if (tegra_dvfs_is_dfll_range(c->dvfs, rate))
			return tegra12_cpu_clk_dfll_on(c, rate, old_rate);
		else if (is_dfll)
			return tegra12_cpu_clk_dfll_off(c, rate, old_rate);
	}
	return tegra12_cpu_clk_set_plls(c, rate, old_rate);
}

static long tegra12_cpu_clk_round_rate(struct clk *c, unsigned long rate)
{
	unsigned long max_rate = c->max_rate;

	/* Remove dfll boost to maximum rate when running on PLL */
	if (c->dvfs && !tegra_dvfs_is_dfll_scale(c->dvfs, rate))
		max_rate -= c->dvfs->dfll_data.max_rate_boost;

	if (rate > max_rate)
		rate = max_rate;
	else if (rate < c->min_rate)
		rate = c->min_rate;
	return rate;
}

static struct clk_ops tegra_cpu_ops = {
	.init     = tegra12_cpu_clk_init,
	.enable   = tegra12_cpu_clk_enable,
	.disable  = tegra12_cpu_clk_disable,
	.set_rate = tegra12_cpu_clk_set_rate,
	.round_rate = tegra12_cpu_clk_round_rate,
};


static void tegra12_cpu_cmplx_clk_init(struct clk *c)
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

static int tegra12_cpu_cmplx_clk_enable(struct clk *c)
{
	return 0;
}

static void tegra12_cpu_cmplx_clk_disable(struct clk *c)
{
	pr_debug("%s on clock %s\n", __func__, c->name);

	/* oops - don't disable the CPU complex clock! */
	BUG();
}

static int tegra12_cpu_cmplx_clk_set_rate(struct clk *c, unsigned long rate)
{
	unsigned long flags;
	int ret;
	struct clk *parent = c->parent;

	if (!parent->ops || !parent->ops->set_rate)
		return -ENOSYS;

	clk_lock_save(parent, &flags);

	ret = clk_set_rate_locked(parent, rate);

	clk_unlock_restore(parent, &flags);

	return ret;
}

static int tegra12_cpu_cmplx_clk_set_parent(struct clk *c, struct clk *p)
{
	int ret;
	unsigned int flags, delay;
	const struct clk_mux_sel *sel;
	unsigned long rate = clk_get_rate(c->parent);
	struct clk *dfll = c->parent->u.cpu.dynamic ? : p->u.cpu.dynamic;
	struct clk *p_source_old = NULL;
	struct clk *p_source;

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
			rate = rate > p->max_rate ? p->max_rate : p->min_rate;
			ret = clk_set_rate(c->parent, rate);
			if (ret) {
				pr_err("%s: Failed to set rate %lu for %s\n",
				        __func__, rate, p->name);
				return ret;
			}
		}
	} else
#endif
	{
		if (rate > p->max_rate) {	/* over-clocking - no switch */
			pr_warn("%s: No %s mode switch to %s at rate %lu\n",
				 __func__, c->name, p->name, rate);
			return -ECANCELED;
		}
		flags = TEGRA_POWER_CLUSTER_IMMEDIATE;
		flags |= TEGRA_POWER_CLUSTER_PART_DEFAULT;
		delay = 0;
	}
	flags |= (p->u.cpu.mode == MODE_LP) ? TEGRA_POWER_CLUSTER_LP :
		TEGRA_POWER_CLUSTER_G;

	if (p == c->parent) {
		if (flags & TEGRA_POWER_CLUSTER_FORCE) {
			/* Allow parameterized switch to the same mode */
			ret = tegra_cluster_control(delay, flags);
			if (ret)
				pr_err("%s: Failed to force %s mode to %s\n",
				       __func__, c->name, p->name);
			return ret;
		}
		return 0;	/* already switched - exit */
	}

	tegra_dvfs_rail_mode_updating(tegra_cpu_rail, true);
	if (c->parent->parent->parent == dfll) {
		/* G (DFLL selected as clock source) => LP switch:
		 * turn DFLL into open loop mode ("release" VDD_CPU rail)
		 * select target p_source for LP, and get its rate ready
		 */
		ret = tegra_clk_cfg_ex(dfll, TEGRA_CLK_DFLL_LOCK, 0);
		if (ret)
			goto abort;

		p_source = rate <= p->u.cpu.backup_rate ?
			p->u.cpu.backup : p->u.cpu.main;
		ret = clk_set_rate(p_source, rate);
		if (ret)
			goto abort;
	} else if ((p->parent->parent == dfll) ||
		   (p->dvfs && tegra_dvfs_is_dfll_range(p->dvfs, rate))) {
		/* LP => G (DFLL selected as clock source) switch:
		 * set DFLL rate ready (DFLL is still disabled)
		 * (set target p_source as dfll, G source is already selected)
		 */
		p_source = dfll;
		ret = clk_set_rate(dfll,
			tegra_dvfs_rail_is_dfll_mode(tegra_cpu_rail) ? rate :
			max(rate, p->dvfs->dfll_data.use_dfll_rate_min));
		if (ret)
			goto abort;

		ret = tegra_dvfs_rail_dfll_mode_set_cold(tegra_cpu_rail);
		if (ret)
			goto abort;
	} else
		/* DFLL is not selected on either side of the switch:
		 * set target p_source equal to current clock source
		 */
		p_source = c->parent->parent->parent;

	/* Switch new parent to target clock source if necessary */
	if (p->parent->parent != p_source) {
		clk_enable(p->parent->parent);
		clk_enable(p->parent);
		p_source_old = p->parent->parent;
		ret = clk_set_parent(p->parent, p_source);
		if (ret) {
			pr_err("%s: Failed to set parent %s for %s\n",
			       __func__, p_source->name, p->name);
			goto abort;
		}
	}

	/* Enabling new parent scales new mode voltage rail in advanvce
	   before the switch happens (if p_source is DFLL: open loop mode) */
	if (c->refcnt)
		clk_enable(p);

	/* switch CPU mode */
	ret = tegra_cluster_control(delay, flags);
	if (ret) {
		if (c->refcnt)
			clk_disable(p);
		pr_err("%s: Failed to switch %s mode to %s\n",
		       __func__, c->name, p->name);
		goto abort;
	}

	/*
	 * Lock DFLL now (resume closed loop VDD_CPU control).
	 * G CPU operations are resumed on DFLL if it was the last G CPU
	 * clock source, or if resume rate is in DFLL usage range in case
	 * when auto-switch between PLL and DFLL is enabled.
	 */
	if (p_source == dfll) {
		if (tegra_dvfs_rail_is_dfll_mode(tegra_cpu_rail)) {
			tegra_clk_cfg_ex(dfll, TEGRA_CLK_DFLL_LOCK, 1);
		} else {
			clk_set_rate(dfll, rate);
			tegra_clk_cfg_ex(dfll, TEGRA_CLK_DFLL_LOCK, 1);
			tegra_dvfs_dfll_mode_set(p->dvfs, rate);
		}
	}

	/* Disabling old parent scales old mode voltage rail */
	if (c->refcnt)
		clk_disable(c->parent);
	if (p_source_old) {
		clk_disable(p->parent);
		clk_disable(p_source_old);
	}

	clk_reparent(c, p);

	tegra_dvfs_rail_mode_updating(tegra_cpu_rail, false);
	return 0;

abort:
	/* Re-lock DFLL if necessary after aborted switch */
	if (c->parent->parent->parent == dfll) {
		clk_set_rate(dfll, rate);
		tegra_clk_cfg_ex(dfll, TEGRA_CLK_DFLL_LOCK, 1);
	}
	if (p_source_old) {
		clk_disable(p->parent);
		clk_disable(p_source_old);
	}
	tegra_dvfs_rail_mode_updating(tegra_cpu_rail, false);

	pr_err("%s: aborted switch from %s to %s\n",
	       __func__, c->parent->name, p->name);
	return ret;
}

static long tegra12_cpu_cmplx_round_rate(struct clk *c,
	unsigned long rate)
{
	return clk_round_rate(c->parent, rate);
}

static struct clk_ops tegra_cpu_cmplx_ops = {
	.init     = tegra12_cpu_cmplx_clk_init,
	.enable   = tegra12_cpu_cmplx_clk_enable,
	.disable  = tegra12_cpu_cmplx_clk_disable,
	.set_rate = tegra12_cpu_cmplx_clk_set_rate,
	.set_parent = tegra12_cpu_cmplx_clk_set_parent,
	.round_rate = tegra12_cpu_cmplx_round_rate,
};

/* virtual cop clock functions. Used to acquire the fake 'cop' clock to
 * reset the COP block (i.e. AVP) */
static void tegra12_cop_clk_reset(struct clk *c, bool assert)
{
	unsigned long reg = assert ? RST_DEVICES_SET_L : RST_DEVICES_CLR_L;

	pr_debug("%s %s\n", __func__, assert ? "assert" : "deassert");
	clk_writel(1 << 1, reg);
}

static struct clk_ops tegra_cop_ops = {
	.reset    = tegra12_cop_clk_reset,
};

/* bus clock functions */
static DEFINE_SPINLOCK(bus_clk_lock);

static int bus_set_div(struct clk *c, int div)
{
	u32 val;
	unsigned long flags;

	if (!div || (div > (BUS_CLK_DIV_MASK + 1)))
		return -EINVAL;

	spin_lock_irqsave(&bus_clk_lock, flags);
	val = clk_readl(c->reg);
	val &= ~(BUS_CLK_DIV_MASK << c->reg_shift);
	val |= (div - 1) << c->reg_shift;
	clk_writel(val, c->reg);
	c->div = div;
	spin_unlock_irqrestore(&bus_clk_lock, flags);

	return 0;
}

static void tegra12_bus_clk_init(struct clk *c)
{
	u32 val = clk_readl(c->reg);
	c->state = ((val >> c->reg_shift) & BUS_CLK_DISABLE) ? OFF : ON;
	c->div = ((val >> c->reg_shift) & BUS_CLK_DIV_MASK) + 1;
	c->mul = 1;
}

static int tegra12_bus_clk_enable(struct clk *c)
{
	u32 val = clk_readl(c->reg);
	val &= ~(BUS_CLK_DISABLE << c->reg_shift);
	clk_writel(val, c->reg);
	return 0;
}

static void tegra12_bus_clk_disable(struct clk *c)
{
	u32 val = clk_readl(c->reg);
	val |= BUS_CLK_DISABLE << c->reg_shift;
	clk_writel(val, c->reg);
}

static int tegra12_bus_clk_set_rate(struct clk *c, unsigned long rate)
{
	unsigned long parent_rate = clk_get_rate(c->parent);
	int i;

	if (tegra_platform_is_qt())
		return 0;
	for (i = 1; i <= 4; i++) {
		if (rate >= parent_rate / i)
			return bus_set_div(c, i);
	}
	return -EINVAL;
}

static struct clk_ops tegra_bus_ops = {
	.init			= tegra12_bus_clk_init,
	.enable			= tegra12_bus_clk_enable,
	.disable		= tegra12_bus_clk_disable,
	.set_rate		= tegra12_bus_clk_set_rate,
};

/* Virtual system bus complex clock is used to hide the sequence of
   changing sclk/hclk/pclk parents and dividers to configure requested
   sclk target rate. */
static void tegra12_sbus_cmplx_init(struct clk *c)
{
	unsigned long rate;

	c->max_rate = c->parent->max_rate;
	c->min_rate = c->parent->min_rate;

	/* Threshold must be an exact proper factor of low range parent,
	   and both low/high range parents have 7.1 fractional dividers */
	rate = clk_get_rate(c->u.system.sclk_low->parent);
	if (tegra_platform_is_qt())
		return;
	if (c->u.system.threshold) {
		BUG_ON(c->u.system.threshold > rate) ;
		BUG_ON((rate % c->u.system.threshold) != 0);
	}
	BUG_ON(!(c->u.system.sclk_low->flags & DIV_U71));
	BUG_ON(!(c->u.system.sclk_high->flags & DIV_U71));
}

/* This special sbus round function is implemented because:
 *
 * (a) sbus complex clock source is selected automatically based on rate
 *
 * (b) since sbus is a shared bus, and its frequency is set to the highest
 * enabled shared_bus_user clock, the target rate should be rounded up divider
 * ladder (if max limit allows it) - for pll_div and peripheral_div common is
 * rounding down - special case again.
 *
 * Note that final rate is trimmed (not rounded up) to avoid spiraling up in
 * recursive calls. Lost 1Hz is added in tegra12_sbus_cmplx_set_rate before
 * actually setting divider rate.
 */
static long tegra12_sbus_cmplx_round_updown(struct clk *c, unsigned long rate,
					    bool up)
{
	int divider;
	unsigned long source_rate, round_rate;
	struct clk *new_parent;

	rate = max(rate, c->min_rate);

	new_parent = (rate <= c->u.system.threshold) ?
		c->u.system.sclk_low : c->u.system.sclk_high;
	source_rate = clk_get_rate(new_parent->parent);

	divider = clk_div71_get_divider(source_rate, rate,
		new_parent->flags, up ? ROUND_DIVIDER_DOWN : ROUND_DIVIDER_UP);
	if (divider < 0)
		return c->min_rate;

	if (divider == 1)
		divider = 0;

	round_rate = source_rate * 2 / (divider + 2);
	if (round_rate > c->max_rate) {
		divider += new_parent->flags & DIV_U71_INT ? 2 : 1;
#if !DIVIDER_1_5_ALLOWED
		divider = max(2, divider);
#endif
		round_rate = source_rate * 2 / (divider + 2);
	}

	if (new_parent == c->u.system.sclk_high) {
		/* Prevent oscillation across threshold */
		if (round_rate <= c->u.system.threshold)
			round_rate = c->u.system.threshold;
	}
	return round_rate;
}

static long tegra12_sbus_cmplx_round_rate(struct clk *c, unsigned long rate)
{
	return tegra12_sbus_cmplx_round_updown(c, rate, true);
}

/*
 * FIXME: This limitation may have been relaxed on Tegra12.
 * This issue has to be visited again once the new limitation is clarified.
 *
 * Limitations on SCLK/HCLK/PCLK dividers:
 * (A) H/w limitation:
 *	if SCLK >= 60MHz, SCLK:PCLK >= 2
 * (B) S/w policy limitation, in addition to (A):
 *	if any APB bus shared user request is enabled, HCLK:PCLK >= 2
 *  Reason for (B): assuming APB bus shared user has requested X < 60MHz,
 *  HCLK = PCLK = X, and new AHB user is coming on-line requesting Y >= 60MHz,
 *  we can consider 2 paths depending on order of changing HCLK rate and
 *  HCLK:PCLK ratio
 *  (i)  HCLK:PCLK = X:X => Y:Y* => Y:Y/2,   (*) violates rule (A)
 *  (ii) HCLK:PCLK = X:X => X:X/2* => Y:Y/2, (*) under-clocks APB user
 *  In this case we can not guarantee safe transition from HCLK:PCLK = 1:1
 *  below 60MHz to HCLK rate above 60MHz without under-clocking APB user.
 *  Hence, policy (B).
 *
 *  Note: when there are no request from APB users, path (ii) can be used to
 *  increase HCLK above 60MHz, and HCLK:PCLK = 1:1 is allowed.
 */

#define SCLK_PCLK_UNITY_RATIO_RATE_MAX	60000000
#define BUS_AHB_DIV_MAX			(BUS_CLK_DIV_MASK + 1UL)
#define BUS_APB_DIV_MAX			(BUS_CLK_DIV_MASK + 1UL)

static int tegra12_sbus_cmplx_set_rate(struct clk *c, unsigned long rate)
{
	int ret;
	struct clk *new_parent;

	/*
	 * Configure SCLK/HCLK/PCLK guranteed safe combination:
	 * - select the appropriate sclk parent
	 * - keep hclk at the same rate as sclk
	 * - set pclk at 1:2 rate of hclk
	 */
	bus_set_div(c->u.system.pclk, 2);
	bus_set_div(c->u.system.hclk, 1);
	c->child_bus->child_bus->div = 2;
	c->child_bus->div = 1;

	if (rate == clk_get_rate_locked(c))
		return 0;

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

	return 0;
}

static int tegra12_clk_sbus_update(struct clk *bus)
{
	int ret, div;
	bool p_requested;
	unsigned long s_rate, h_rate, p_rate, ceiling;
	struct clk *ahb, *apb;

	if (detach_shared_bus)
		return 0;

	s_rate = tegra12_clk_shared_bus_update(bus, &ahb, &apb, &ceiling);
	if (bus->override_rate)
		return clk_set_rate_locked(bus, s_rate);

	ahb = bus->child_bus;
	apb = ahb->child_bus;
	h_rate = ahb->u.shared_bus_user.rate;
	p_rate = apb->u.shared_bus_user.rate;
	p_requested = apb->refcnt > 1;

	/* Propagate ratio requirements up from PCLK to SCLK */
	if (p_requested)
		h_rate = max(h_rate, p_rate * 2);
	s_rate = max(s_rate, h_rate);
	if (s_rate >= SCLK_PCLK_UNITY_RATIO_RATE_MAX)
		s_rate = max(s_rate, p_rate * 2);

	/* Propagate cap requirements down from SCLK to PCLK */
	s_rate = tegra12_clk_cap_shared_bus(bus, s_rate, ceiling);
	if (s_rate >= SCLK_PCLK_UNITY_RATIO_RATE_MAX)
		p_rate = min(p_rate, s_rate / 2);
	h_rate = min(h_rate, s_rate);
	if (p_requested)
		p_rate = min(p_rate, h_rate / 2);


	/* Set new sclk rate in safe 1:1:2, rounded "up" configuration */
	ret = clk_set_rate_locked(bus, s_rate);
	if (ret)
		return ret;

	/* Finally settle new bus divider values */
	s_rate = clk_get_rate_locked(bus);
	div = min(s_rate / h_rate, BUS_AHB_DIV_MAX);
	if (div != 1) {
		bus_set_div(bus->u.system.hclk, div);
		ahb->div = div;
	}

	h_rate = clk_get_rate(bus->u.system.hclk);
	div = min(h_rate / p_rate, BUS_APB_DIV_MAX);
	if (div != 2) {
		bus_set_div(bus->u.system.pclk, div);
		apb->div = div;
	}

	return 0;
}

static struct clk_ops tegra_sbus_cmplx_ops = {
	.init = tegra12_sbus_cmplx_init,
	.set_rate = tegra12_sbus_cmplx_set_rate,
	.round_rate = tegra12_sbus_cmplx_round_rate,
	.round_rate_updown = tegra12_sbus_cmplx_round_updown,
	.shared_bus_update = tegra12_clk_sbus_update,
};

/* Blink output functions */

static void tegra12_blink_clk_init(struct clk *c)
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

static int tegra12_blink_clk_enable(struct clk *c)
{
	u32 val;

	val = pmc_readl(PMC_DPD_PADS_ORIDE);
	pmc_writel(val | PMC_DPD_PADS_ORIDE_BLINK_ENB, PMC_DPD_PADS_ORIDE);

	val = pmc_readl(PMC_CTRL);
	pmc_writel(val | PMC_CTRL_BLINK_ENB, PMC_CTRL);

	return 0;
}

static void tegra12_blink_clk_disable(struct clk *c)
{
	u32 val;

	val = pmc_readl(PMC_CTRL);
	pmc_writel(val & ~PMC_CTRL_BLINK_ENB, PMC_CTRL);

	val = pmc_readl(PMC_DPD_PADS_ORIDE);
	pmc_writel(val & ~PMC_DPD_PADS_ORIDE_BLINK_ENB, PMC_DPD_PADS_ORIDE);
}

static int tegra12_blink_clk_set_rate(struct clk *c, unsigned long rate)
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
	.init			= &tegra12_blink_clk_init,
	.enable			= &tegra12_blink_clk_enable,
	.disable		= &tegra12_blink_clk_disable,
	.set_rate		= &tegra12_blink_clk_set_rate,
};

/* PLL Functions */
static int tegra12_pll_clk_wait_for_lock(
	struct clk *c, u32 lock_reg, u32 lock_bits)
{
#if USE_PLL_LOCK_BITS
	int i;
	u32 val = 0;

	for (i = 0; i < (c->u.pll.lock_delay / PLL_PRE_LOCK_DELAY + 1); i++) {
		udelay(PLL_PRE_LOCK_DELAY);
		val = clk_readl(lock_reg);
		if ((val & lock_bits) == lock_bits) {
			udelay(PLL_POST_LOCK_DELAY);
			return 0;
		}
	}

	/* PLLCX lock bits may fluctuate after the lock - do detailed reporting
	   at debug level (phase lock bit happens to uniquely identify PLLCX) */
	if (lock_bits & PLLCX_BASE_PHASE_LOCK) {
		pr_debug("Timed out waiting %s locks: %s %s not set\n", c->name,
		       val & PLL_BASE_LOCK ? "" : "frequency_lock",
		       val & PLLCX_BASE_PHASE_LOCK ? "" : "phase_lock");
		pr_debug("base =  0x%x\n", val);
		pr_debug("misc =  0x%x\n", clk_readl(c->reg + PLL_MISC(c)));
		pr_debug("misc1 = 0x%x\n", clk_readl(c->reg + PLL_MISCN(c, 1)));
		pr_debug("misc2 = 0x%x\n", clk_readl(c->reg + PLL_MISCN(c, 2)));
		pr_debug("misc3 = 0x%x\n", clk_readl(c->reg + PLL_MISCN(c, 3)));
		return -ETIMEDOUT;
	} else {
		pr_err("Timed out waiting for %s lock bit ([0x%x] = 0x%x)\n",
		       c->name, lock_reg, val);
		return -ETIMEDOUT;
	}
#endif
	udelay(c->u.pll.lock_delay);
	return 0;
}

static void usb_plls_hw_control_enable(u32 reg)
{
	u32 val = clk_readl(reg);
	val |= USB_PLLS_USE_LOCKDET | USB_PLLS_SEQ_START_STATE;
	val &= ~USB_PLLS_ENABLE_SWCTL;
	val |= USB_PLLS_SEQ_START_STATE;
	pll_writel_delay(val, reg);

	val |= USB_PLLS_SEQ_ENABLE;
	pll_writel_delay(val, reg);
}

static void tegra12_utmi_param_configure(struct clk *c)
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
	reg |= UTMIP_PLL_CFG2_FORCE_PD_SAMP_A_POWERUP;
	reg |= UTMIP_PLL_CFG2_FORCE_PD_SAMP_B_POWERUP;
	reg |= UTMIP_PLL_CFG2_FORCE_PD_SAMP_C_POWERUP;
	reg |= UTMIP_PLL_CFG2_FORCE_PD_SAMP_D_POWERUP;
	reg &= ~UTMIP_PLL_CFG2_FORCE_PD_SAMP_A_POWERDOWN;
	reg &= ~UTMIP_PLL_CFG2_FORCE_PD_SAMP_B_POWERDOWN;
	reg &= ~UTMIP_PLL_CFG2_FORCE_PD_SAMP_C_POWERDOWN;
	reg &= ~UTMIP_PLL_CFG2_FORCE_PD_SAMP_D_POWERDOWN;

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
	reg &= ~UTMIP_PLL_CFG1_FORCE_PLLU_POWERUP;
	reg &= ~UTMIP_PLL_CFG1_FORCE_PLLU_POWERDOWN;
	clk_writel(reg, UTMIP_PLL_CFG1);

	/* Setup HW control of UTMIPLL */
	reg = clk_readl(UTMIPLL_HW_PWRDN_CFG0);
	reg |= UTMIPLL_HW_PWRDN_CFG0_USE_LOCKDET;
	reg &= ~UTMIPLL_HW_PWRDN_CFG0_CLK_ENABLE_SWCTL;
	reg |= UTMIPLL_HW_PWRDN_CFG0_SEQ_START_STATE;
	clk_writel(reg, UTMIPLL_HW_PWRDN_CFG0);

	reg = clk_readl(UTMIP_PLL_CFG1);
	reg &= ~UTMIP_PLL_CFG1_FORCE_PLL_ENABLE_POWERUP;
	reg &= ~UTMIP_PLL_CFG1_FORCE_PLL_ENABLE_POWERDOWN;
	clk_writel(reg, UTMIP_PLL_CFG1);

	udelay(1);

	/* Setup SW override of UTMIPLL assuming USB2.0
	   ports are assigned to USB2 */
	reg = clk_readl(UTMIPLL_HW_PWRDN_CFG0);
	reg |= UTMIPLL_HW_PWRDN_CFG0_IDDQ_SWCTL;
	reg |= UTMIPLL_HW_PWRDN_CFG0_IDDQ_OVERRIDE;
	clk_writel(reg, UTMIPLL_HW_PWRDN_CFG0);

	udelay(1);

	/* Enable HW control UTMIPLL */
	reg = clk_readl(UTMIPLL_HW_PWRDN_CFG0);
	reg |= UTMIPLL_HW_PWRDN_CFG0_SEQ_ENABLE;
	clk_writel(reg, UTMIPLL_HW_PWRDN_CFG0);
}

static void tegra12_pll_clk_init(struct clk *c)
{
	u32 val = clk_readl(c->reg + PLL_BASE);
	u32 divn_mask = c->flags & PLLD ?
		PLLD_BASE_DIVN_MASK : PLL_BASE_DIVN_MASK;

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
		c->mul = (val & divn_mask) >> PLL_BASE_DIVN_SHIFT;
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
		/* Configure UTMI PLL power management */
		tegra12_utmi_param_configure(c);

		/* Put PLLU under h/w control */
		usb_plls_hw_control_enable(PLLU_HW_PWRDN_CFG0);

		val = clk_readl(c->reg + PLL_BASE);
		val &= ~PLLU_BASE_OVERRIDE;
		clk_writel(val, c->reg + PLL_BASE);

		/* Set XUSB PLL pad pwr override and iddq */
		val = xusb_padctl_readl(XUSB_PADCTL_IOPHY_PLL_P0_CTL1_0);
		val |= XUSB_PADCTL_IOPHY_PLL_P0_CTL1_0_PLL_PWR_OVRD;
		val |= XUSB_PADCTL_IOPHY_PLL_P0_CTL1_0_PLL_IDDQ;
		xusb_padctl_writel(val, XUSB_PADCTL_IOPHY_PLL_P0_CTL1_0);
	}
}

static int tegra12_pll_clk_enable(struct clk *c)
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

	tegra12_pll_clk_wait_for_lock(c, c->reg + PLL_BASE, PLL_BASE_LOCK);

	return 0;
}

static void tegra12_pll_clk_disable(struct clk *c)
{
	u32 val;
	pr_debug("%s on clock %s\n", __func__, c->name);

	val = clk_readl(c->reg);
	val &= ~(PLL_BASE_BYPASS | PLL_BASE_ENABLE);
	if (tegra_platform_is_qt())
		return;
	clk_writel(val, c->reg);
}

/* Special comparison frequency selection for PLLD at 12MHz refrence rate */
unsigned long get_pll_cfreq_special(struct clk *c, unsigned long input_rate,
				   unsigned long rate, unsigned long *vco)
{
	if (!(c->flags & PLLD) || (input_rate != 12000000))
		return 0;

	*vco = c->u.pll.vco_min;

	if (rate <= 250000000)
		return 4000000;
	else if (rate <= 500000000)
		return 2000000;
	else
		return 1000000;
}

/* Common comparison frequency selection */
unsigned long get_pll_cfreq_common(struct clk *c, unsigned long input_rate,
				   unsigned long rate, unsigned long *vco)
{
	unsigned long cfreq = 0;

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

	/* Raise VCO to guarantee 0.5% accuracy, and vco min boundary */
	*vco = max(200 * cfreq, c->u.pll.vco_min);
	return cfreq;
}

static u8 get_pll_cpcon(struct clk *c, u16 n)
{
	if (c->flags & PLLD) {
		if (n >= 1000)
			return 15;
		else if (n >= 600)
			return 12;
		else if (n >= 300)
			return 8;
		else if (n >= 50)
			return 3;
		else
			return 2;
	}
	return c->u.pll.cpcon_default ? : OUT_OF_TABLE_CPCON;
}

static int tegra12_pll_clk_set_rate(struct clk *c, unsigned long rate)
{
	u32 val, p_div, old_base;
	unsigned long input_rate;
	const struct clk_pll_freq_table *sel;
	struct clk_pll_freq_table cfg;
	u32 divn_mask = c->flags & PLLD ?
		PLLD_BASE_DIVN_MASK : PLL_BASE_DIVN_MASK;

	pr_debug("%s: %s %lu\n", __func__, c->name, rate);

	if (tegra_platform_is_qt())
		return 0;
	if (c->flags & PLL_FIXED) {
		int ret = 0;
		if (rate != c->u.pll.fixed_rate) {
			pr_err("%s: Can not change %s fixed rate %lu to %lu\n",
			       __func__, c->name, c->u.pll.fixed_rate, rate);
			ret = -EINVAL;
		}
		return ret;
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
		unsigned long cfreq, vco;
		BUG_ON(c->flags & PLLU);
		sel = &cfg;

		/* If available, use pll specific algorithm to select comparison
		   frequency, and vco target */
		cfreq = get_pll_cfreq_special(c, input_rate, rate, &vco);
		if (!cfreq)
			cfreq = get_pll_cfreq_common(c, input_rate, rate, &vco);

		for (cfg.output_rate = rate; cfg.output_rate < vco; p_div++)
			cfg.output_rate <<= 1;

		cfg.p = 0x1 << p_div;
		cfg.m = input_rate / cfreq;
		cfg.n = cfg.output_rate / cfreq;
		cfg.cpcon = get_pll_cpcon(c, cfg.n);

		if ((cfg.m > (PLL_BASE_DIVM_MASK >> PLL_BASE_DIVM_SHIFT)) ||
		    (cfg.n > (divn_mask >> PLL_BASE_DIVN_SHIFT)) ||
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

	old_base = val = clk_readl(c->reg + PLL_BASE);
	val &= ~(PLL_BASE_DIVM_MASK | divn_mask |
		 ((c->flags & PLLU) ? PLLU_BASE_POST_DIV : PLL_BASE_DIVP_MASK));
	val |= (sel->m << PLL_BASE_DIVM_SHIFT) |
		(sel->n << PLL_BASE_DIVN_SHIFT) | p_div;
	if (val == old_base)
		return 0;

	if (c->state == ON) {
		tegra12_pll_clk_disable(c);
		val &= ~(PLL_BASE_BYPASS | PLL_BASE_ENABLE);
	}
	clk_writel(val, c->reg + PLL_BASE);

	if (c->flags & PLL_HAS_CPCON) {
		val = clk_readl(c->reg + PLL_MISC(c));
		val &= ~PLL_MISC_CPCON_MASK;
		val |= sel->cpcon << PLL_MISC_CPCON_SHIFT;
		if (c->flags & (PLLU | PLLD)) {
			val &= ~PLL_MISC_LFCON_MASK;
			val |= PLLDU_LFCON << PLL_MISC_LFCON_SHIFT;
		}
		clk_writel(val, c->reg + PLL_MISC(c));
	}

	if (c->state == ON)
		tegra12_pll_clk_enable(c);

	return 0;
}

static struct clk_ops tegra_pll_ops = {
	.init			= tegra12_pll_clk_init,
	.enable			= tegra12_pll_clk_enable,
	.disable		= tegra12_pll_clk_disable,
	.set_rate		= tegra12_pll_clk_set_rate,
};

static void tegra12_pllp_clk_init(struct clk *c)
{
	tegra12_pll_clk_init(c);
	tegra12_pllp_init_dependencies(c->u.pll.fixed_rate);
}

#ifdef CONFIG_PM_SLEEP
static void tegra12_pllp_clk_resume(struct clk *c)
{
	unsigned long rate = c->u.pll.fixed_rate;
	tegra12_pll_clk_init(c);
	BUG_ON(rate != c->u.pll.fixed_rate);
}
#endif

static struct clk_ops tegra_pllp_ops = {
	.init			= tegra12_pllp_clk_init,
	.enable			= tegra12_pll_clk_enable,
	.disable		= tegra12_pll_clk_disable,
	.set_rate		= tegra12_pll_clk_set_rate,
};

static int
tegra12_plld_clk_cfg_ex(struct clk *c, enum tegra_clk_ex_param p, u32 setting)
{
	u32 val, mask, reg;
	u32 clear = 0;

	switch (p) {
	case TEGRA_CLK_PLLD_CSI_OUT_ENB:
		mask = PLLD_BASE_CSI_CLKENABLE | PLLD_BASE_CSI_CLKSOURCE;
		reg = c->reg + PLL_BASE;
		break;
	case TEGRA_CLK_MIPI_CSI_OUT_ENB:
		mask = PLLD_BASE_CSI_CLKENABLE;
		clear = PLLD_BASE_CSI_CLKSOURCE;
		reg = c->reg + PLL_BASE;
		break;
	case TEGRA_CLK_PLLD_DSI_OUT_ENB:
		mask = PLLD_MISC_DSI_CLKENABLE;
		reg = c->reg + PLL_MISC(c);
		break;
	case TEGRA_CLK_PLLD_MIPI_MUX_SEL:
		mask = PLLD_BASE_DSI_MUX_MASK;
		reg = c->reg + PLL_BASE;
		break;
	default:
		return -EINVAL;
	}

	val = clk_readl(reg);
	if (setting) {
		val |= mask;
		val &= ~clear;
	} else
		val &= ~mask;
	clk_writel(val, reg);
	return 0;
}

static struct clk_ops tegra_plld_ops = {
	.init			= tegra12_pll_clk_init,
	.enable			= tegra12_pll_clk_enable,
	.disable		= tegra12_pll_clk_disable,
	.set_rate		= tegra12_pll_clk_set_rate,
	.clk_cfg_ex		= tegra12_plld_clk_cfg_ex,
};

/*
 * Dynamic ramp PLLs:
 *  PLLC2 and PLLC3 (PLLCX)
 *  PLLX and PLLC (PLLXC)
 *
 * When scaling PLLC and PLLX, dynamic ramp is allowed for any transition that
 * changes NDIV only. As a matter of policy we will make sure that switching
 * between output rates above VCO minimum is always dynamic. The pre-requisite
 * for the above guarantee is the following configuration convention:
 * - pll configured with fixed MDIV
 * - when output rate is above VCO minimum PDIV = 0 (p-value = 1)
 * Switching between output rates below VCO minimum may or may not be dynamic,
 * and switching across VCO minimum is never dynamic.
 *
 * PLLC2 and PLLC3 in addition to dynamic ramp mechanism have also glitchless
 * output dividers. However dynamic ramp without overshoot is guaranteed only
 * when output divisor is less or equal 8.
 *
 * Of course, dynamic ramp is applied provided PLL is already enabled.
 */

/*
 * Common configuration policy for dynamic ramp PLLs:
 * - always set fixed M-value based on the reference rate
 * - always set P-value value 1:1 for output rates above VCO minimum, and
 *   choose minimum necessary P-value for output rates below VCO minimum
 * - calculate N-value based on selected M and P
 */
static int pll_dyn_ramp_cfg(struct clk *c, struct clk_pll_freq_table *cfg,
	unsigned long rate, unsigned long input_rate, u32 *pdiv)
{
	u32 p;

	if (!rate)
		return -EINVAL;

	p = DIV_ROUND_UP(c->u.pll.vco_min, rate);
	p = c->u.pll.round_p_to_pdiv(p, pdiv);
	if (IS_ERR_VALUE(p))
		return -EINVAL;

	cfg->m = PLL_FIXED_MDIV(c, input_rate);
	cfg->p = p;
	cfg->output_rate = rate * cfg->p;
	if (cfg->output_rate > c->u.pll.vco_max)
		cfg->output_rate = c->u.pll.vco_max;
	cfg->n = cfg->output_rate * cfg->m / input_rate;

	/* can use PLLCX N-divider field layout for all dynamic ramp PLLs */
	if (cfg->n > (PLLCX_BASE_DIVN_MASK >> PLL_BASE_DIVN_SHIFT))
		return -EINVAL;

	return 0;
}

static int pll_dyn_ramp_find_cfg(struct clk *c, struct clk_pll_freq_table *cfg,
	unsigned long rate, unsigned long input_rate, u32 *pdiv)
{
	const struct clk_pll_freq_table *sel;

	/* Check if the target rate is tabulated */
	for (sel = c->u.pll.freq_table; sel->input_rate != 0; sel++) {
		if (sel->input_rate == input_rate && sel->output_rate == rate) {
			u32 p = c->u.pll.round_p_to_pdiv(sel->p, pdiv);
			BUG_ON(IS_ERR_VALUE(p));
			BUG_ON(sel->m != PLL_FIXED_MDIV(c, input_rate));
			*cfg = *sel;
			return 0;
		}
	}

	/* Configure out-of-table rate */
	if (pll_dyn_ramp_cfg(c, cfg, rate, input_rate, pdiv)) {
		pr_err("%s: Failed to set %s out-of-table rate %lu\n",
		       __func__, c->name, rate);
		return -EINVAL;
	}
	return 0;
}

static inline void pll_do_iddq(struct clk *c, u32 offs, u32 iddq_bit, bool set)
{
	u32 val = clk_readl(c->reg + offs);
	if (set)
		val |= iddq_bit;
	else
		val &= ~iddq_bit;
	clk_writel_delay(val, c->reg + offs);
}


static u8 pllcx_p[PLLCX_PDIV_MAX + 1] = {
/* PDIV: 0, 1, 2, 3, 4, 5,  6,  7 */
/* p: */ 1, 2, 3, 4, 6, 8, 12, 16 };

static u32 pllcx_round_p_to_pdiv(u32 p, u32 *pdiv)
{
	int i;

	if (p) {
		for (i = 0; i <= PLLCX_PDIV_MAX; i++) {
			/* Do not use DIV3 p values - mapped to even PDIV */
			if (i && ((i & 0x1) == 0))
				continue;

			if (p <= pllcx_p[i]) {
				if (pdiv)
					*pdiv = i;
				return pllcx_p[i];
			}
		}
	}
	return -EINVAL;
}

static void pllcx_update_dynamic_koef(struct clk *c, unsigned long input_rate,
					u32 n)
{
	u32 val, n_threshold;

	switch (input_rate) {
	case 12000000:
		n_threshold = 70;
		break;
	case 13000000:
	case 26000000:
		n_threshold = 71;
		break;
	case 16800000:
		n_threshold = 55;
		break;
	case 19200000:
		n_threshold = 48;
		break;
	default:
		pr_err("%s: Unexpected reference rate %lu\n",
			__func__, input_rate);
		BUG();
		return;
	}

	val = clk_readl(c->reg + PLL_MISC(c));
	val &= ~(PLLCX_MISC_SDM_DIV_MASK | PLLCX_MISC_FILT_DIV_MASK);
	val |= n <= n_threshold ?
		PLLCX_MISC_DIV_LOW_RANGE : PLLCX_MISC_DIV_HIGH_RANGE;
	clk_writel(val, c->reg + PLL_MISC(c));
}

static void pllcx_strobe(struct clk *c)
{
	u32 reg = c->reg + PLL_MISC(c);
	u32 val = clk_readl(reg);

	val |= PLLCX_MISC_STROBE;
	pll_writel_delay(val, reg);

	val &= ~PLLCX_MISC_STROBE;
	clk_writel(val, reg);
}

static void pllcx_set_defaults(struct clk *c, unsigned long input_rate, u32 n)
{
	u32 misc1val = PLLCX_MISC1_DEFAULT_VALUE;
	if (c->state == ON)
		BUG_ON(!tegra_platform_is_linsim());
	else
		misc1val |= PLLCX_MISC1_IDDQ;

	clk_writel(PLLCX_MISC_DEFAULT_VALUE, c->reg + PLL_MISC(c));
	clk_writel(misc1val, c->reg + PLL_MISCN(c, 1));
	clk_writel(PLLCX_MISC2_DEFAULT_VALUE, c->reg + PLL_MISCN(c, 2));
	clk_writel(PLLCX_MISC3_DEFAULT_VALUE, c->reg + PLL_MISCN(c, 3));

	pllcx_update_dynamic_koef(c, input_rate, n);
}

static void tegra12_pllcx_clk_init(struct clk *c)
{
	unsigned long input_rate = clk_get_rate(c->parent);
	u32 m, n, p, val;

	/* clip vco_min to exact multiple of input rate to avoid crossover
	   by rounding */
	c->u.pll.vco_min =
		DIV_ROUND_UP(c->u.pll.vco_min, input_rate) * input_rate;
	c->min_rate = DIV_ROUND_UP(c->u.pll.vco_min, pllcx_p[PLLCX_PDIV_MAX]);

	val = clk_readl(c->reg + PLL_BASE);
	c->state = (val & PLL_BASE_ENABLE) ? ON : OFF;

	/*
	 * PLLCX is not a boot PLL, it should be left disabled by boot-loader,
	 * and no enabled module clocks should use it as a source during clock
	 * init.
	 */
	BUG_ON(c->state == ON && !tegra_platform_is_linsim());
	/*
	 * Most of PLLCX register fields are shadowed, and can not be read
	 * directly from PLL h/w. Hence, actual PLLCX boot state is unknown.
	 * Initialize PLL to default state: disabled, reset; shadow registers
	 * loaded with default parameters; dividers are preset for half of
	 * minimum VCO rate (the latter assured that shadowed divider settings
	 * are within supported range).
	 */
	m = PLL_FIXED_MDIV(c, input_rate);
	n = m * c->u.pll.vco_min / input_rate;
	p = pllcx_p[1];
	val = (m << PLL_BASE_DIVM_SHIFT) | (n << PLL_BASE_DIVN_SHIFT) |
		(1 << PLL_BASE_DIVP_SHIFT);
	clk_writel(val, c->reg + PLL_BASE);	/* PLL disabled */

	pllcx_set_defaults(c, input_rate, n);

	c->mul = n;
	c->div = m * p;
}

static int tegra12_pllcx_clk_enable(struct clk *c)
{
	u32 val;
	pr_debug("%s on clock %s\n", __func__, c->name);

	pll_do_iddq(c, PLL_MISCN(c, 1), PLLCX_MISC1_IDDQ, false);

	val = clk_readl(c->reg + PLL_BASE);
	val &= ~PLL_BASE_BYPASS;
	val |= PLL_BASE_ENABLE;
	pll_writel_delay(val, c->reg + PLL_BASE);

	val = clk_readl(c->reg + PLL_MISC(c));
	val &= ~PLLCX_MISC_RESET;
	pll_writel_delay(val, c->reg + PLL_MISC(c));

	pllcx_strobe(c);
	tegra12_pll_clk_wait_for_lock(c, c->reg + PLL_BASE,
			PLL_BASE_LOCK | PLLCX_BASE_PHASE_LOCK);
	return 0;
}

static void tegra12_pllcx_clk_disable(struct clk *c)
{
	u32 val;
	pr_debug("%s on clock %s\n", __func__, c->name);

	val = clk_readl(c->reg);
	val &= ~(PLL_BASE_BYPASS | PLL_BASE_ENABLE);
	clk_writel(val, c->reg);

	val = clk_readl(c->reg + PLL_MISC(c));
	val |= PLLCX_MISC_RESET;
	pll_writel_delay(val, c->reg + PLL_MISC(c));

	pll_do_iddq(c, PLL_MISCN(c, 1), PLLCX_MISC1_IDDQ, true);
}

static int tegra12_pllcx_clk_set_rate(struct clk *c, unsigned long rate)
{
	u32 val, pdiv;
	unsigned long input_rate;
	struct clk_pll_freq_table cfg, old_cfg;
	const struct clk_pll_freq_table *sel = &cfg;

	pr_debug("%s: %s %lu\n", __func__, c->name, rate);
	if (tegra_platform_is_qt())
		return 0;

	input_rate = clk_get_rate(c->parent);

	if (pll_dyn_ramp_find_cfg(c, &cfg, rate, input_rate, &pdiv))
		return -EINVAL;

	c->mul = sel->n;
	c->div = sel->m * sel->p;

	val = clk_readl(c->reg + PLL_BASE);
	PLL_BASE_PARSE(PLLCX, old_cfg, val);
	old_cfg.p = pllcx_p[old_cfg.p];

	BUG_ON(old_cfg.m != sel->m);
	if ((sel->n == old_cfg.n) && (sel->p == old_cfg.p))
		return 0;

#if PLLCX_USE_DYN_RAMP
	if (c->state == ON && ((sel->n == old_cfg.n) ||
			       PLLCX_IS_DYN(sel->p, old_cfg.p))) {
		/*
		 * Dynamic ramp if PLL is enabled, and M divider is unchanged:
		 * - Change P divider 1st if intermediate rate is below either
		 *   old or new rate.
		 * - Change N divider with DFS strobe - target rate is either
		 *   final new rate or below old rate
		 * - If divider has been changed, exit without waiting for lock.
		 *   Otherwise, wait for lock and change divider.
		 */
		if (sel->p > old_cfg.p) {
			val &= ~PLLCX_BASE_DIVP_MASK;
			val |= pdiv << PLL_BASE_DIVP_SHIFT;
			clk_writel(val, c->reg + PLL_BASE);
		}

		if (sel->n != old_cfg.n) {
			pllcx_update_dynamic_koef(c, input_rate, sel->n);
			val &= ~PLLCX_BASE_DIVN_MASK;
			val |= sel->n << PLL_BASE_DIVN_SHIFT;
			pll_writel_delay(val, c->reg + PLL_BASE);

			pllcx_strobe(c);
			tegra12_pll_clk_wait_for_lock(c, c->reg + PLL_BASE,
					PLL_BASE_LOCK | PLLCX_BASE_PHASE_LOCK);
		}

		if (sel->p < old_cfg.p) {
			val &= ~PLLCX_BASE_DIVP_MASK;
			val |= pdiv << PLL_BASE_DIVP_SHIFT;
			clk_writel(val, c->reg + PLL_BASE);
		}
		return 0;
	}
#endif

	val &= ~(PLLCX_BASE_DIVN_MASK | PLLCX_BASE_DIVP_MASK);
	val |= (sel->n << PLL_BASE_DIVN_SHIFT) |
		(pdiv << PLL_BASE_DIVP_SHIFT);

	if (c->state == ON) {
		tegra12_pllcx_clk_disable(c);
		val &= ~(PLL_BASE_BYPASS | PLL_BASE_ENABLE);
	}
	pllcx_update_dynamic_koef(c, input_rate, sel->n);
	clk_writel(val, c->reg + PLL_BASE);
	if (c->state == ON)
		tegra12_pllcx_clk_enable(c);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static void tegra12_pllcx_clk_resume_enable(struct clk *c)
{
	unsigned long rate = clk_get_rate_all_locked(c->parent);
	u32 val = clk_readl(c->reg + PLL_BASE);
	enum clk_state state = c->state;

	if (val & PLL_BASE_ENABLE)
		return;		/* already resumed */

	/* Restore input divider */
	val &= ~PLLCX_BASE_DIVM_MASK;
	val |= PLL_FIXED_MDIV(c, rate) << PLL_BASE_DIVM_SHIFT;
	clk_writel(val, c->reg + PLL_BASE);

	/* temporarily sync h/w and s/w states, final sync happens
	   in tegra_clk_resume later */
	c->state = OFF;
	pllcx_set_defaults(c, rate, c->mul);

	rate = clk_get_rate_all_locked(c);
	tegra12_pllcx_clk_set_rate(c, rate);
	tegra12_pllcx_clk_enable(c);
	c->state = state;
}
#endif

static struct clk_ops tegra_pllcx_ops = {
	.init			= tegra12_pllcx_clk_init,
	.enable			= tegra12_pllcx_clk_enable,
	.disable		= tegra12_pllcx_clk_disable,
	.set_rate		= tegra12_pllcx_clk_set_rate,
};


/* non-monotonic mapping below is not a typo */
static u8 pllxc_p[PLLXC_PDIV_MAX + 1] = {
/* PDIV: 0, 1, 2, 3, 4, 5, 6,  7,  8,  9, 10, 11, 12, 13, 14 */
/* p: */ 1, 2, 3, 4, 5, 6, 8, 10, 12, 16, 12, 16, 20, 24, 32 };

static u32 pllxc_round_p_to_pdiv(u32 p, u32 *pdiv)
{
	if (!p || (p > PLLXC_SW_PDIV_MAX + 1))
		return -EINVAL;

	if (pdiv)
		*pdiv = p - 1;
	return p;
}

static void pllxc_get_dyn_steps(struct clk *c, unsigned long input_rate,
				u32 *step_a, u32 *step_b)
{
	switch (input_rate) {
	case 12000000:
	case 13000000:
	case 26000000:
		*step_a = 0x2B;
		*step_b = 0x0B;
		return;
	case 16800000:
		*step_a = 0x1A;
		*step_b = 0x09;
		return;
	case 19200000:
		*step_a = 0x12;
		*step_b = 0x08;
		return;
	default:
		pr_err("%s: Unexpected reference rate %lu\n",
			__func__, input_rate);
		BUG();
	}
}

static void pllx_set_defaults(struct clk *c, unsigned long input_rate)
{
	u32 val;
	u32 step_a, step_b;

	/* Only s/w dyn ramp control is supported */
	val = clk_readl(PLLX_HW_CTRL_CFG);
	BUG_ON(!(val & PLLX_HW_CTRL_CFG_SWCTRL) && !tegra_platform_is_linsim());

	pllxc_get_dyn_steps(c, input_rate, &step_a, &step_b);
	val = step_a << PLLX_MISC2_DYNRAMP_STEPA_SHIFT;
	val |= step_b << PLLX_MISC2_DYNRAMP_STEPB_SHIFT;

	/* Get ready dyn ramp state machine, disable lock override */
	clk_writel(val, c->reg + PLL_MISCN(c, 2));

	/* Enable outputs to CPUs and configure lock */
	val = 0;
#if USE_PLL_LOCK_BITS
	val |= PLL_MISC_LOCK_ENABLE(c);
#endif
	clk_writel(val, c->reg + PLL_MISC(c));

	/* Check/set IDDQ */
	val = clk_readl(c->reg + PLL_MISCN(c, 3));
	if (c->state == ON) {
		BUG_ON(val & PLLX_MISC3_IDDQ && !tegra_platform_is_linsim());
	} else {
		val |= PLLX_MISC3_IDDQ;
		clk_writel(val, c->reg + PLL_MISCN(c, 3));
	}
}

static void pllc_set_defaults(struct clk *c, unsigned long input_rate)
{
	u32 val;
	u32 step_a, step_b;

	/* Get ready dyn ramp state machine */
	pllxc_get_dyn_steps(c, input_rate, &step_a, &step_b);
	val = step_a << PLLC_MISC1_DYNRAMP_STEPA_SHIFT;
	val |= step_b << PLLC_MISC1_DYNRAMP_STEPB_SHIFT;
	clk_writel(val, c->reg + PLL_MISCN(c, 1));

	/* Configure lock and check/set IDDQ */
	val = clk_readl(c->reg + PLL_BASE);
	val &= ~PLLC_BASE_LOCK_OVERRIDE;
	clk_writel(val, c->reg + PLL_BASE);

	val = clk_readl(c->reg + PLL_MISC(c));
#if USE_PLL_LOCK_BITS
	val |= PLLC_MISC_LOCK_ENABLE;
#else
	val &= ~PLLC_MISC_LOCK_ENABLE;
#endif
	clk_writel(val, c->reg + PLL_MISC(c));

	if (c->state == ON) {
		BUG_ON(val & PLLC_MISC_IDDQ && !tegra_platform_is_linsim());
	} else {
		val |= PLLC_MISC_IDDQ;
		clk_writel(val, c->reg + PLL_MISC(c));
	}
}

static void tegra12_pllxc_clk_init(struct clk *c)
{
	unsigned long input_rate = clk_get_rate(c->parent);
	u32 m, p, val;

	/* clip vco_min to exact multiple of input rate to avoid crossover
	   by rounding */
	c->u.pll.vco_min =
		DIV_ROUND_UP(c->u.pll.vco_min, input_rate) * input_rate;
	c->min_rate =
		DIV_ROUND_UP(c->u.pll.vco_min, pllxc_p[PLLXC_SW_PDIV_MAX]);

	val = clk_readl(c->reg + PLL_BASE);
	c->state = (val & PLL_BASE_ENABLE) ? ON : OFF;

	m = (val & PLLXC_BASE_DIVM_MASK) >> PLL_BASE_DIVM_SHIFT;
	p = (val & PLLXC_BASE_DIVP_MASK) >> PLL_BASE_DIVP_SHIFT;
	BUG_ON(p > PLLXC_PDIV_MAX);
	p = pllxc_p[p];

	c->div = m * p;
	c->mul = (val & PLLXC_BASE_DIVN_MASK) >> PLL_BASE_DIVN_SHIFT;

	if (c->flags & PLLX)
		pllx_set_defaults(c, input_rate);
	else
		pllc_set_defaults(c, input_rate);
}

static int tegra12_pllxc_clk_enable(struct clk *c)
{
	u32 val;
	pr_debug("%s on clock %s\n", __func__, c->name);

	if (c->flags & PLLX)
		pll_do_iddq(c, PLL_MISCN(c, 3), PLLX_MISC3_IDDQ, false);
	else
		pll_do_iddq(c, PLL_MISC(c), PLLC_MISC_IDDQ, false);

	val = clk_readl(c->reg + PLL_BASE);
	val |= PLL_BASE_ENABLE;
	clk_writel(val, c->reg + PLL_BASE);

	tegra12_pll_clk_wait_for_lock(c, c->reg + PLL_BASE, PLL_BASE_LOCK);

	return 0;
}

static void tegra12_pllxc_clk_disable(struct clk *c)
{
	u32 val;
	pr_debug("%s on clock %s\n", __func__, c->name);

	val = clk_readl(c->reg + PLL_BASE);
	val &= ~PLL_BASE_ENABLE;
	clk_writel(val, c->reg + PLL_BASE);

	if (c->flags & PLLX)
		pll_do_iddq(c, PLL_MISCN(c, 3), PLLX_MISC3_IDDQ, true);
	else
		pll_do_iddq(c, PLL_MISC(c), PLLC_MISC_IDDQ, true);

}

#define PLLXC_DYN_RAMP(pll_misc, reg)					\
	do {								\
		u32 misc = clk_readl((reg));				\
									\
		misc &= ~pll_misc##_NDIV_NEW_MASK;			\
		misc |= sel->n << pll_misc##_NDIV_NEW_SHIFT;		\
		pll_writel_delay(misc, (reg));				\
									\
		misc |= pll_misc##_EN_DYNRAMP;				\
		clk_writel(misc, (reg));				\
		tegra12_pll_clk_wait_for_lock(c, (reg),			\
					pll_misc##_DYNRAMP_DONE);	\
									\
		val &= ~PLLXC_BASE_DIVN_MASK;				\
		val |= sel->n << PLL_BASE_DIVN_SHIFT;			\
		pll_writel_delay(val, c->reg + PLL_BASE);		\
									\
		misc &= ~pll_misc##_EN_DYNRAMP;				\
		pll_writel_delay(misc, (reg));				\
	} while (0)

static int tegra12_pllxc_clk_set_rate(struct clk *c, unsigned long rate)
{
	u32 val, pdiv;
	unsigned long input_rate;
	struct clk_pll_freq_table cfg, old_cfg;
	const struct clk_pll_freq_table *sel = &cfg;

	pr_debug("%s: %s %lu\n", __func__, c->name, rate);
	if (tegra_platform_is_qt())
		return 0;

	input_rate = clk_get_rate(c->parent);

	if (pll_dyn_ramp_find_cfg(c, &cfg, rate, input_rate, &pdiv))
		return -EINVAL;

	c->mul = sel->n;
	c->div = sel->m * sel->p;

	val = clk_readl(c->reg + PLL_BASE);
	PLL_BASE_PARSE(PLLXC, old_cfg, val);
	old_cfg.p = pllxc_p[old_cfg.p];

	if ((sel->m == old_cfg.m) && (sel->n == old_cfg.n) &&
	    (sel->p == old_cfg.p))
		return 0;

#if PLLXC_USE_DYN_RAMP
	/*
	 * Dynamic ramp can be used if M, P dividers are unchanged
	 * (coveres superset of conventional dynamic ramps)
	 */
	if ((c->state == ON) && (sel->m == old_cfg.m) &&
	    (sel->p == old_cfg.p)) {

		if (c->flags & PLLX) {
			u32 reg = c->reg + PLL_MISCN(c, 2);
			PLLXC_DYN_RAMP(PLLX_MISC2, reg);
		} else {
			u32 reg = c->reg + PLL_MISCN(c, 1);
			PLLXC_DYN_RAMP(PLLC_MISC1, reg);
		}

		return 0;
	}
#endif
	if (c->state == ON) {
		/* Use "ENABLE" pulse without placing PLL into IDDQ */
		val &= ~PLL_BASE_ENABLE;
		pll_writel_delay(val, c->reg + PLL_BASE);
	}

	val &= ~(PLLXC_BASE_DIVM_MASK |
		 PLLXC_BASE_DIVN_MASK | PLLXC_BASE_DIVP_MASK);
	val |= (sel->m << PLL_BASE_DIVM_SHIFT) |
		(sel->n << PLL_BASE_DIVN_SHIFT) | (pdiv << PLL_BASE_DIVP_SHIFT);
	clk_writel(val, c->reg + PLL_BASE);

	if (c->state == ON) {
		val |= PLL_BASE_ENABLE;
		clk_writel(val, c->reg + PLL_BASE);
		tegra12_pll_clk_wait_for_lock(c, c->reg + PLL_BASE,
					      PLL_BASE_LOCK);
	}
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static void tegra12_pllxc_clk_resume_enable(struct clk *c)
{
	unsigned long rate = clk_get_rate_all_locked(c->parent);
	enum clk_state state = c->state;

	if (clk_readl(c->reg + PLL_BASE) & PLL_BASE_ENABLE)
		return;		/* already resumed */

	/* temporarily sync h/w and s/w states, final sync happens
	   in tegra_clk_resume later */
	c->state = OFF;
	if (c->flags & PLLX)
		pllx_set_defaults(c, rate);
	else
		pllc_set_defaults(c, rate);

	rate = clk_get_rate_all_locked(c);
	tegra12_pllxc_clk_set_rate(c, rate);
	tegra12_pllxc_clk_enable(c);
	c->state = state;
}
#endif

static struct clk_ops tegra_pllxc_ops = {
	.init			= tegra12_pllxc_clk_init,
	.enable			= tegra12_pllxc_clk_enable,
	.disable		= tegra12_pllxc_clk_disable,
	.set_rate		= tegra12_pllxc_clk_set_rate,
};


/* FIXME: pllm suspend/resume */

/* non-monotonic mapping below is not a typo */
static u8 pllm_p[PLLM_PDIV_MAX + 1] = {
/* PDIV: 0, 1, 2, 3, 4, 5, 6,  7,  8,  9, 10, 11, 12, 13, 14 */
/* p: */ 1, 2, 3, 4, 5, 6, 8, 10, 12, 16, 12, 16, 20, 24, 32 };

static u32 pllm_round_p_to_pdiv(u32 p, u32 *pdiv)
{
	if (!p || (p > PLLM_SW_PDIV_MAX + 1))
		return -EINVAL;

	if (pdiv)
		*pdiv = p - 1;
	return p;
}

static void pllm_set_defaults(struct clk *c, unsigned long input_rate)
{
	u32 val = clk_readl(c->reg + PLL_MISC(c));

	val &= ~PLLM_MISC_LOCK_OVERRIDE;
#if USE_PLL_LOCK_BITS
	val &= ~PLLM_MISC_LOCK_DISABLE;
#else
	val |= PLLM_MISC_LOCK_DISABLE;
#endif

	if (c->state != ON)
		val |= PLLM_MISC_IDDQ;
	else
		BUG_ON(val & PLLM_MISC_IDDQ && !tegra_platform_is_linsim());

	clk_writel(val, c->reg + PLL_MISC(c));
}

static void tegra12_pllm_clk_init(struct clk *c)
{
	unsigned long input_rate = clk_get_rate(c->parent);
	u32 m, p, val;

	/* clip vco_min to exact multiple of input rate to avoid crossover
	   by rounding */
	c->u.pll.vco_min =
		DIV_ROUND_UP(c->u.pll.vco_min, input_rate) * input_rate;
	c->min_rate =
		DIV_ROUND_UP(c->u.pll.vco_min, pllm_p[PLLM_SW_PDIV_MAX]);

	val = pmc_readl(PMC_PLLP_WB0_OVERRIDE);
	if (val & PMC_PLLP_WB0_OVERRIDE_PLLM_OVERRIDE) {
		c->state = (val & PMC_PLLP_WB0_OVERRIDE_PLLM_ENABLE) ? ON : OFF;

		/* Tegra12 has bad default value of PMC_PLLM_WB0_OVERRIDE.
		 * If bootloader does not initialize PLLM, kernel has to
		 * initialize the register with sane value. */
		if (c->state == OFF) {
			val = pmc_readl(PMC_PLLM_WB0_OVERRIDE);
			m = (val & PLLM_BASE_DIVM_MASK) >> PLL_BASE_DIVM_SHIFT;
			if (m != PLL_FIXED_MDIV(c, input_rate)) {
				/* Copy DIVM and DIVN from PLLM_BASE */
				pr_info("%s: Fixing DIVM and DIVN\n", __func__);
				val = clk_readl(c->reg + PLL_BASE);
				val &= (PLLM_BASE_DIVM_MASK
					| PLLM_BASE_DIVN_MASK);
				pmc_writel(val, PMC_PLLM_WB0_OVERRIDE);
			}
		}

		val = pmc_readl(PMC_PLLM_WB0_OVERRIDE_2);
		p = (val & PMC_PLLM_WB0_OVERRIDE_2_DIVP_MASK) >>
			PMC_PLLM_WB0_OVERRIDE_2_DIVP_SHIFT;

		val = pmc_readl(PMC_PLLM_WB0_OVERRIDE);
	} else {
		val = clk_readl(c->reg + PLL_BASE);
		c->state = (val & PLL_BASE_ENABLE) ? ON : OFF;
		p = (val & PLLM_BASE_DIVP_MASK) >> PLL_BASE_DIVP_SHIFT;
	}

	m = (val & PLLM_BASE_DIVM_MASK) >> PLL_BASE_DIVM_SHIFT;
	BUG_ON(m != PLL_FIXED_MDIV(c, input_rate)
			 && tegra_platform_is_silicon());
	c->div = m * pllm_p[p];
	c->mul = (val & PLLM_BASE_DIVN_MASK) >> PLL_BASE_DIVN_SHIFT;

	pllm_set_defaults(c, input_rate);
}

static int tegra12_pllm_clk_enable(struct clk *c)
{
	u32 val;
	pr_debug("%s on clock %s\n", __func__, c->name);

	pll_do_iddq(c, PLL_MISC(c), PLLM_MISC_IDDQ, false);

	/* Just enable both base and override - one would work */
	val = clk_readl(c->reg + PLL_BASE);
	val |= PLL_BASE_ENABLE;
	clk_writel(val, c->reg + PLL_BASE);

	val = pmc_readl(PMC_PLLP_WB0_OVERRIDE);
	val |= PMC_PLLP_WB0_OVERRIDE_PLLM_ENABLE;
	pmc_writel(val, PMC_PLLP_WB0_OVERRIDE);
	val = pmc_readl(PMC_PLLP_WB0_OVERRIDE);

	tegra12_pll_clk_wait_for_lock(c, c->reg + PLL_BASE, PLL_BASE_LOCK);
	return 0;
}

static void tegra12_pllm_clk_disable(struct clk *c)
{
	u32 val;
	pr_debug("%s on clock %s\n", __func__, c->name);

	/* Just disable both base and override - one would work */
	val = pmc_readl(PMC_PLLP_WB0_OVERRIDE);
	val &= ~PMC_PLLP_WB0_OVERRIDE_PLLM_ENABLE;
	pmc_writel(val, PMC_PLLP_WB0_OVERRIDE);
	val = pmc_readl(PMC_PLLP_WB0_OVERRIDE);

	val = clk_readl(c->reg + PLL_BASE);
	val &= ~PLL_BASE_ENABLE;
	clk_writel(val, c->reg + PLL_BASE);

	pll_do_iddq(c, PLL_MISC(c), PLLM_MISC_IDDQ, true);
}

static int tegra12_pllm_clk_set_rate(struct clk *c, unsigned long rate)
{
	u32 val, pdiv;
	unsigned long input_rate;
	struct clk_pll_freq_table cfg;
	const struct clk_pll_freq_table *sel = &cfg;

	pr_debug("%s: %s %lu\n", __func__, c->name, rate);

	if (c->state == ON) {
		if (rate != clk_get_rate_locked(c)) {
			pr_err("%s: Can not change memory %s rate in flight\n",
			       __func__, c->name);
			return -EINVAL;
		}
		return 0;
	}

	input_rate = clk_get_rate(c->parent);

	if (pll_dyn_ramp_find_cfg(c, &cfg, rate, input_rate, &pdiv))
		return -EINVAL;

	c->mul = sel->n;
	c->div = sel->m * sel->p;

	val = pmc_readl(PMC_PLLP_WB0_OVERRIDE);
	if (val & PMC_PLLP_WB0_OVERRIDE_PLLM_OVERRIDE) {
		val = pmc_readl(PMC_PLLM_WB0_OVERRIDE_2);
		val &= ~PMC_PLLM_WB0_OVERRIDE_2_DIVP_MASK;
		val |= pdiv << PMC_PLLM_WB0_OVERRIDE_2_DIVP_SHIFT;
		pmc_writel(val, PMC_PLLM_WB0_OVERRIDE_2);

		val = pmc_readl(PMC_PLLM_WB0_OVERRIDE);
		val &= ~(PLLM_BASE_DIVM_MASK | PLLM_BASE_DIVN_MASK);
		val |= (sel->m << PLL_BASE_DIVM_SHIFT) |
			(sel->n << PLL_BASE_DIVN_SHIFT);
		pmc_writel(val, PMC_PLLM_WB0_OVERRIDE);
	} else {
		val = clk_readl(c->reg + PLL_BASE);
		val &= ~(PLLM_BASE_DIVM_MASK | PLLM_BASE_DIVN_MASK |
			 PLLM_BASE_DIVP_MASK);
		val |= (sel->m << PLL_BASE_DIVM_SHIFT) |
			(sel->n << PLL_BASE_DIVN_SHIFT) |
			(pdiv << PLL_BASE_DIVP_SHIFT);
		clk_writel(val, c->reg + PLL_BASE);
	}

	return 0;
}

static struct clk_ops tegra_pllm_ops = {
	.init			= tegra12_pllm_clk_init,
	.enable			= tegra12_pllm_clk_enable,
	.disable		= tegra12_pllm_clk_disable,
	.set_rate		= tegra12_pllm_clk_set_rate,
};


static u8 pllss_p[PLLSS_PDIV_MAX + 1] = {
/* PDIV: 0, 1, 2, 3, 4, 5, 6,  7,  8,  9, 10, 11, 12, 13, 14 */
/* p: */ 1, 2, 3, 4, 5, 6, 8, 10, 12, 16, 12, 16, 20, 24, 32 };

static u32 pllss_round_p_to_pdiv(u32 p, u32 *pdiv)
{
	if (!p || (p > PLLSS_SW_PDIV_MAX + 1))
		return -EINVAL;

	if (pdiv)
		*pdiv = p - 1;
	return p;
}

static void pllss_set_defaults(struct clk *c, unsigned long input_rate)
{
	u32 val;

	clk_writel(PLLSS_MISC_DEFAULT_VALUE, c->reg + PLL_MISC(c));
	clk_writel(PLLSS_CFG_DEFAULT_VALUE, c->reg + PLLSS_CFG(c));
	clk_writel(PLLSS_CTRL1_DEFAULT_VALUE, c->reg + PLLSS_CTRL1(c));
	clk_writel(PLLSS_CTRL2_DEFAULT_VALUE, c->reg + PLLSS_CTRL2(c));

	val = clk_readl(c->reg + PLL_MISC(c));
#if USE_PLL_LOCK_BITS
	val |= PLLSS_MISC_LOCK_ENABLE;
#else
	val &= ~PLLSS_MISC_LOCK_ENABLE;
#endif
	clk_writel(val, c->reg + PLL_MISC(c));

	val = clk_readl(c->reg + PLL_BASE);
	if (c->state != ON)
		val |= PLLSS_BASE_IDDQ;
	else
		BUG_ON(val & PLLSS_BASE_IDDQ && !tegra_platform_is_linsim());
	val &= ~PLLSS_BASE_LOCK_OVERRIDE;
	clk_writel(val, c->reg + PLL_BASE);
}

static void tegra12_pllss_clk_init(struct clk *c)
{
	unsigned long input_rate;
	u32 m, n, p_div, val;

	val = clk_readl(c->reg + PLL_BASE);
	BUG_ON(((val & PLLSS_BASE_SOURCE_MASK)
			>> PLLSS_BASE_SOURCE_SHIFT) != 0);

	input_rate = clk_get_rate(c->parent);

	/* clip vco_min to exact multiple of input rate to avoid crossover
	   by rounding */
	c->u.pll.vco_min =
		DIV_ROUND_UP(c->u.pll.vco_min, input_rate) * input_rate;
	c->min_rate =
		DIV_ROUND_UP(c->u.pll.vco_min, pllss_p[PLLSS_SW_PDIV_MAX]);

	/* Assuming bootloader does not initialize these PLLs */
	n = (val & PLLSS_BASE_DIVN_MASK) >> PLL_BASE_DIVN_SHIFT;
	BUG_ON(n > 1);

	/* Reset default value of those PLLs are not safe.
	   For example, they cause problem in LP0 resume.
	   Replace them here with the safe value. */
	m = PLL_FIXED_MDIV(c, input_rate);
	n = c->u.pll.vco_min / input_rate * m;
	p_div = PLLSS_SW_PDIV_MAX;
	val &= ~PLLSS_BASE_DIVM_MASK;
	val &= ~PLLSS_BASE_DIVN_MASK;
	val &= ~PLLSS_BASE_DIVP_MASK;
	val |= m << PLL_BASE_DIVM_SHIFT;
	val |= n << PLL_BASE_DIVN_SHIFT;
	val |= p_div << PLL_BASE_DIVP_SHIFT;
	clk_writel(val, c->reg + PLL_BASE);

	c->state = (val & PLL_BASE_ENABLE) ? ON : OFF;

	m = (val & PLLSS_BASE_DIVM_MASK) >> PLL_BASE_DIVM_SHIFT;
	n = (val & PLLSS_BASE_DIVN_MASK) >> PLL_BASE_DIVN_SHIFT;
	p_div = (val & PLLSS_BASE_DIVP_MASK) >> PLL_BASE_DIVP_SHIFT;

	c->div = m * pllss_p[p_div];
	c->mul = n;

	/* FIXME: hack for bringup */
	pr_info("%s: val=%08x m=%d n=%d p_div=%d input_rate=%lu\n",
		c->name, val, m, n, p_div, input_rate);

	pllss_set_defaults(c, input_rate);
}

static int tegra12_pllss_clk_enable(struct clk *c)
{
	u32 val;
	pr_debug("%s on clock %s\n", __func__, c->name);

	pll_do_iddq(c, PLL_BASE, PLLSS_BASE_IDDQ, false);

	val = clk_readl(c->reg + PLL_BASE);
	val |= PLL_BASE_ENABLE;
	clk_writel(val, c->reg + PLL_BASE);

	tegra12_pll_clk_wait_for_lock(c, c->reg + PLL_BASE, PLLSS_BASE_LOCK);
	return 0;
}

static void tegra12_pllss_clk_disable(struct clk *c)
{
	u32 val;
	pr_debug("%s on clock %s\n", __func__, c->name);

	val = clk_readl(c->reg + PLL_BASE);
	val &= ~PLL_BASE_ENABLE;
	clk_writel(val, c->reg + PLL_BASE);

	pll_do_iddq(c, PLL_BASE, PLLSS_BASE_IDDQ, true);
}

static int tegra12_pllss_clk_set_rate(struct clk *c, unsigned long rate)
{
	u32 val, pdiv, old_base;
	unsigned long input_rate;
	struct clk_pll_freq_table cfg, old_cfg;
	const struct clk_pll_freq_table *sel = &cfg;

	pr_debug("%s: %s %lu\n", __func__, c->name, rate);
	if (tegra_platform_is_qt())
		return 0;

	input_rate = clk_get_rate(c->parent);

	if (pll_dyn_ramp_find_cfg(c, &cfg, rate, input_rate, &pdiv))
		return -EINVAL;

	c->mul = sel->n;
	c->div = sel->m * sel->p;

	val = clk_readl(c->reg + PLL_BASE);
	PLL_BASE_PARSE(PLLSS, old_cfg, val);
	old_cfg.p = pllss_p[old_cfg.p];

	if ((sel->m == old_cfg.m) && (sel->n == old_cfg.n) &&
	    (sel->p == old_cfg.p))
		return 0;

	val = old_base = clk_readl(c->reg + PLL_BASE);
	val &= ~(PLLSS_BASE_DIVM_MASK
		| PLLSS_BASE_DIVN_MASK | PLLSS_BASE_DIVP_MASK);
	val |= (sel->m << PLL_BASE_DIVM_SHIFT)
		| (sel->n << PLL_BASE_DIVN_SHIFT)
		| (pdiv << PLL_BASE_DIVP_SHIFT);

	if (val == old_base)
		return 0;

	if (c->state == ON) {
		/* Use "ENABLE" pulse without placing PLL into IDDQ */
		val &= ~PLL_BASE_ENABLE;
		old_base &= ~PLL_BASE_ENABLE;
		pll_writel_delay(old_base, c->reg + PLL_BASE);
	}

	clk_writel(val, c->reg + PLL_BASE);

	if (c->state == ON) {
		val |= PLL_BASE_ENABLE;
		clk_writel(val, c->reg + PLL_BASE);
		tegra12_pll_clk_wait_for_lock(
			c, c->reg + PLL_BASE, PLLSS_BASE_LOCK);
	}
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static void tegra12_pllss_clk_resume_enable(struct clk *c)
{
	unsigned long rate = clk_get_rate_all_locked(c->parent);
	u32 val = clk_readl(c->reg + PLL_BASE);
	enum clk_state state = c->state;

	if (val & PLL_BASE_ENABLE)
		return;		/* already resumed */

	/* Restore input divider */
	val &= ~PLLSS_BASE_DIVM_MASK;
	val |= PLL_FIXED_MDIV(c, rate) << PLL_BASE_DIVM_SHIFT;
	clk_writel(val, c->reg + PLL_BASE);

	/* temporarily sync h/w and s/w states, final sync happens
	   in tegra_clk_resume later */
	c->state = OFF;
	pllss_set_defaults(c, rate);

	rate = clk_get_rate_all_locked(c);
	tegra12_pllss_clk_set_rate(c, rate);
	tegra12_pllss_clk_enable(c);
	c->state = state;
}
#endif

static struct clk_ops tegra_pllss_ops = {
	.init			= tegra12_pllss_clk_init,
	.enable			= tegra12_pllss_clk_enable,
	.disable		= tegra12_pllss_clk_disable,
	.set_rate		= tegra12_pllss_clk_set_rate,
	/* s/w policy, no set_parent, always use tegra_pll_ref */
};

/* non-monotonic mapping below is not a typo */
static u8 pllre_p[PLLRE_PDIV_MAX + 1] = {
/* PDIV: 0, 1, 2, 3, 4, 5, 6,  7,  8,  9, 10, 11, 12, 13, 14 */
/* p: */ 1, 2, 3, 4, 5, 6, 8, 10, 12, 16, 12, 16, 20, 24, 32 };

static u32 pllre_round_p_to_pdiv(u32 p, u32 *pdiv)
{
	if (!p || (p > PLLRE_SW_PDIV_MAX + 1))
		return -EINVAL;

	if (pdiv)
		*pdiv = p - 1;
	return p;
}

static void pllre_set_defaults(struct clk *c, unsigned long input_rate)
{
	u32 val = clk_readl(c->reg + PLL_MISC(c));

	val &= ~PLLRE_MISC_LOCK_OVERRIDE;
#if USE_PLL_LOCK_BITS
	val |= PLLRE_MISC_LOCK_ENABLE;
#else
	val &= ~PLLRE_MISC_LOCK_ENABLE;
#endif

	if (c->state != ON)
		val |= PLLRE_MISC_IDDQ;
	else
		BUG_ON(val & PLLRE_MISC_IDDQ && !tegra_platform_is_linsim());

	clk_writel(val, c->reg + PLL_MISC(c));
}

static void tegra12_pllre_clk_init(struct clk *c)
{
	unsigned long input_rate = clk_get_rate(c->parent);
	u32 m, val;

	/* clip vco_min to exact multiple of input rate to avoid crossover
	   by rounding */
	c->u.pll.vco_min =
		DIV_ROUND_UP(c->u.pll.vco_min, input_rate) * input_rate;
	c->min_rate = c->u.pll.vco_min;

	val = clk_readl(c->reg + PLL_BASE);
	c->state = (val & PLL_BASE_ENABLE) ? ON : OFF;

	if (!val) {
		/* overwrite h/w por state with min setting */
		m = PLL_FIXED_MDIV(c, input_rate);
		val = (m << PLL_BASE_DIVM_SHIFT) |
			(c->min_rate / input_rate << PLL_BASE_DIVN_SHIFT);
		clk_writel(val, c->reg + PLL_BASE);
	}

	m = (val & PLLRE_BASE_DIVM_MASK) >> PLL_BASE_DIVM_SHIFT;
	BUG_ON(m != PLL_FIXED_MDIV(c, input_rate));

	c->div = m;
	c->mul = (val & PLLRE_BASE_DIVN_MASK) >> PLL_BASE_DIVN_SHIFT;

	pllre_set_defaults(c, input_rate);
}

static int tegra12_pllre_clk_enable(struct clk *c)
{
	u32 val;
	pr_debug("%s on clock %s\n", __func__, c->name);

	pll_do_iddq(c, PLL_MISC(c), PLLRE_MISC_IDDQ, false);

	val = clk_readl(c->reg + PLL_BASE);
	val |= PLL_BASE_ENABLE;
	clk_writel(val, c->reg + PLL_BASE);

	tegra12_pll_clk_wait_for_lock(c, c->reg + PLL_MISC(c), PLLRE_MISC_LOCK);
	return 0;
}

static void tegra12_pllre_clk_disable(struct clk *c)
{
	u32 val;
	pr_debug("%s on clock %s\n", __func__, c->name);

	val = clk_readl(c->reg + PLL_BASE);
	val &= ~PLL_BASE_ENABLE;
	clk_writel(val, c->reg + PLL_BASE);

	pll_do_iddq(c, PLL_MISC(c), PLLRE_MISC_IDDQ, true);
}

static int tegra12_pllre_clk_set_rate(struct clk *c, unsigned long rate)
{
	u32 val, old_base;
	unsigned long input_rate;
	struct clk_pll_freq_table cfg;

	pr_debug("%s: %s %lu\n", __func__, c->name, rate);

	if (rate < c->min_rate) {
		pr_err("%s: Failed to set %s rate %lu\n",
		       __func__, c->name, rate);
		return -EINVAL;
	}

	input_rate = clk_get_rate(c->parent);
	cfg.m = PLL_FIXED_MDIV(c, input_rate);
	cfg.n = rate * cfg.m / input_rate;

	c->mul = cfg.n;
	c->div = cfg.m;

	val = old_base = clk_readl(c->reg + PLL_BASE);
	val &= ~(PLLRE_BASE_DIVM_MASK | PLLRE_BASE_DIVN_MASK);
	val |= (cfg.m << PLL_BASE_DIVM_SHIFT) | (cfg.n << PLL_BASE_DIVN_SHIFT);
	if (val == old_base)
		return 0;

	if (c->state == ON) {
		/* Use "ENABLE" pulse without placing PLL into IDDQ */
		val &= ~PLL_BASE_ENABLE;
		old_base &= ~PLL_BASE_ENABLE;
		pll_writel_delay(old_base, c->reg + PLL_BASE);
	}

	clk_writel(val, c->reg + PLL_BASE);

	if (c->state == ON) {
		val |= PLL_BASE_ENABLE;
		clk_writel(val, c->reg + PLL_BASE);
		tegra12_pll_clk_wait_for_lock(
			c, c->reg + PLL_MISC(c), PLLRE_MISC_LOCK);
	}
	return 0;
}

static struct clk_ops tegra_pllre_ops = {
	.init			= tegra12_pllre_clk_init,
	.enable			= tegra12_pllre_clk_enable,
	.disable		= tegra12_pllre_clk_disable,
	.set_rate		= tegra12_pllre_clk_set_rate,
};

static void tegra12_pllre_out_clk_init(struct clk *c)
{
	u32 p, val;

	val = clk_readl(c->reg);
	p = (val & PLLRE_BASE_DIVP_MASK) >> PLLRE_BASE_DIVP_SHIFT;
	BUG_ON(p > PLLRE_PDIV_MAX);
	p = pllre_p[p];

	c->div = p;
	c->mul = 1;
	c->state = c->parent->state;
}

static int tegra12_pllre_out_clk_enable(struct clk *c)
{
	return 0;
}

static void tegra12_pllre_out_clk_disable(struct clk *c)
{
}

static int tegra12_pllre_out_clk_set_rate(struct clk *c, unsigned long rate)
{
	u32 val, p, pdiv;
	unsigned long input_rate, flags;

	pr_debug("%s: %s %lu\n", __func__, c->name, rate);

	clk_lock_save(c->parent, &flags);
	input_rate = clk_get_rate_locked(c->parent);

	p = DIV_ROUND_UP(input_rate, rate);
	p = c->parent->u.pll.round_p_to_pdiv(p, &pdiv);
	if (IS_ERR_VALUE(p)) {
		pr_err("%s: Failed to set %s rate %lu\n",
		       __func__, c->name, rate);
		clk_unlock_restore(c->parent, &flags);
		return -EINVAL;
	}
	c->div = p;

	val = clk_readl(c->reg);
	val &= ~PLLRE_BASE_DIVP_MASK;
	val |= pdiv << PLLRE_BASE_DIVP_SHIFT;
	clk_writel(val, c->reg);

	clk_unlock_restore(c->parent, &flags);
	return 0;
}

static struct clk_ops tegra_pllre_out_ops = {
	.init			= tegra12_pllre_out_clk_init,
	.enable			= tegra12_pllre_out_clk_enable,
	.disable		= tegra12_pllre_out_clk_disable,
	.set_rate		= tegra12_pllre_out_clk_set_rate,
};

#ifdef CONFIG_PM_SLEEP
/* Resume both pllre_vco and pllre_out */
static void tegra12_pllre_clk_resume_enable(struct clk *c)
{
	u32 pdiv;
	u32 val = clk_readl(c->reg + PLL_BASE);
	unsigned long rate = clk_get_rate_all_locked(c->parent->parent);
	enum clk_state state = c->parent->state;

	if (val & PLL_BASE_ENABLE)
		return;		/* already resumed */

	/* temporarily sync h/w and s/w states, final sync happens
	   in tegra_clk_resume later */
	c->parent->state = OFF;
	pllre_set_defaults(c->parent, rate);

	/* restore PLLRE VCO feedback loop (m, n) */
	rate = clk_get_rate_all_locked(c->parent);
	tegra12_pllre_clk_set_rate(c->parent, rate);

	/* restore PLLRE post-divider */
	c->parent->u.pll.round_p_to_pdiv(c->div, &pdiv);
	val = clk_readl(c->reg);
	val &= ~PLLRE_BASE_DIVP_MASK;
	val |= pdiv << PLLRE_BASE_DIVP_SHIFT;
	clk_writel(val, c->reg);

	tegra12_pllre_clk_enable(c->parent);
	c->parent->state = state;
}
#endif

/* non-monotonic mapping below is not a typo */
static u8 plle_p[PLLE_CMLDIV_MAX + 1] = {
/* CMLDIV: 0, 1, 2, 3, 4, 5, 6,  7,  8,  9, 10, 11, 12, 13, 14 */
/* p: */   1, 2, 3, 4, 5, 6, 8, 10, 12, 16, 12, 16, 20, 24, 32 };

static inline void select_pll_e_input(u32 aux_reg)
{
#if USE_PLLE_INPUT_PLLRE
	aux_reg |= PLLE_AUX_PLLRE_SEL;
#else
	aux_reg &= ~(PLLE_AUX_PLLRE_SEL | PLLE_AUX_PLLP_SEL);
#endif
	clk_writel(aux_reg, PLLE_AUX);
}

static void tegra12_plle_clk_init(struct clk *c)
{
	u32 val, p;
	struct clk *pll_ref = tegra_get_clock_by_name("pll_ref");
	struct clk *re_vco = tegra_get_clock_by_name("pll_re_vco");
	struct clk *pllp = tegra_get_clock_by_name("pllp");
#if USE_PLLE_INPUT_PLLRE
	struct clk *ref = re_vco;
#else
	struct clk *ref = pll_ref;
#endif

	val = clk_readl(c->reg + PLL_BASE);
	c->state = (val & PLL_BASE_ENABLE) ? ON : OFF;
	c->mul = (val & PLLE_BASE_DIVN_MASK) >> PLL_BASE_DIVN_SHIFT;
	c->div = (val & PLLE_BASE_DIVM_MASK) >> PLL_BASE_DIVM_SHIFT;
	p = (val & PLLE_BASE_DIVCML_MASK) >> PLLE_BASE_DIVCML_SHIFT;
	c->div *= plle_p[p];

	val = clk_readl(PLLE_AUX);
	c->parent = (val & PLLE_AUX_PLLRE_SEL) ? re_vco :
		(val & PLLE_AUX_PLLP_SEL) ? pllp : pll_ref;
	if (c->parent != ref) {
		if (c->state == ON) {
			WARN(1, "%s: pll_e is left enabled with %s input\n",
			     __func__, c->parent->name);
		} else {
			c->parent = ref;
			select_pll_e_input(val);
		}
	}
}

static void tegra12_plle_clk_disable(struct clk *c)
{
	u32 val;
	pr_debug("%s on clock %s\n", __func__, c->name);

	/* FIXME: do we need to restore other s/w controls ? */
	val = clk_readl(c->reg + PLL_BASE);
	val &= ~PLL_BASE_ENABLE;
	clk_writel(val, c->reg + PLL_BASE);

	val = clk_readl(c->reg + PLL_MISC(c));
	val |= PLLE_MISC_IDDQ_SW_CTRL | PLLE_MISC_IDDQ_SW_VALUE;
	pll_writel_delay(val, c->reg + PLL_MISC(c));

	/* Set XUSB PLL pad pwr override and iddq */
	val = xusb_padctl_readl(XUSB_PADCTL_IOPHY_PLL_P0_CTL1_0);
	val |= XUSB_PADCTL_IOPHY_PLL_P0_CTL1_0_PLL_PWR_OVRD;
	val |= XUSB_PADCTL_IOPHY_PLL_P0_CTL1_0_PLL_IDDQ;
	xusb_padctl_writel(val, XUSB_PADCTL_IOPHY_PLL_P0_CTL1_0);

}

static int tegra12_plle_clk_enable(struct clk *c)
{
	u32 val;
	const struct clk_pll_freq_table *sel;
	unsigned long rate = c->u.pll.fixed_rate;
	unsigned long input_rate = clk_get_rate(c->parent);

	if (c->state == ON) {
		/* BL left plle enabled - don't change configuartion */
		pr_warn("%s: pll_e is already enabled\n", __func__);
		return 0;
	}

	for (sel = c->u.pll.freq_table; sel->input_rate != 0; sel++) {
		if (sel->input_rate == input_rate && sel->output_rate == rate)
			break;
	}

	if (sel->input_rate == 0) {
		pr_err("%s: %s input rate %lu is out-of-table\n",
		       __func__, c->name, input_rate);
		return -EINVAL;
	}

	/* setup locking configuration, s/w control of IDDQ and enable modes,
	   take pll out of IDDQ via s/w control, setup VREG */
	val = clk_readl(c->reg + PLL_BASE);
	val &= ~PLLE_BASE_LOCK_OVERRIDE;
	clk_writel(val, c->reg + PLL_BASE);

	val = clk_readl(c->reg + PLL_MISC(c));
	val |= PLLE_MISC_LOCK_ENABLE;
	val |= PLLE_MISC_IDDQ_SW_CTRL;
	val &= ~PLLE_MISC_IDDQ_SW_VALUE;
	val |= PLLE_MISC_PLLE_PTS;
	clk_writel(val, c->reg + PLL_MISC(c));
	udelay(5);

	/* configure dividers, disable SS */
	val = clk_readl(PLLE_SS_CTRL);
	val |= PLLE_SS_DISABLE;
	clk_writel(val, PLLE_SS_CTRL);

	val = clk_readl(c->reg + PLL_BASE);
	val &= ~(PLLE_BASE_DIVM_MASK | PLLE_BASE_DIVN_MASK |
		 PLLE_BASE_DIVCML_MASK);
	val |= (sel->m << PLL_BASE_DIVM_SHIFT) |
		(sel->n << PLL_BASE_DIVN_SHIFT) |
		(sel->cpcon << PLLE_BASE_DIVCML_SHIFT);
	pll_writel_delay(val, c->reg + PLL_BASE);
	c->mul = sel->n;
	c->div = sel->m * sel->p;

	/* enable and lock pll */
	val |= PLL_BASE_ENABLE;
	clk_writel(val, c->reg + PLL_BASE);
	tegra12_pll_clk_wait_for_lock(
		c, c->reg + PLL_MISC(c), PLLE_MISC_LOCK);
#if USE_PLLE_SS
	val = clk_readl(PLLE_SS_CTRL);
	val &= ~(PLLE_SS_CNTL_CENTER | PLLE_SS_CNTL_INVERT);
	val &= ~PLLE_SS_COEFFICIENTS_MASK;
	val |= PLLE_SS_COEFFICIENTS_VAL;
	clk_writel(val, PLLE_SS_CTRL);
	val &= ~(PLLE_SS_CNTL_SSC_BYP | PLLE_SS_CNTL_BYPASS_SS);
	pll_writel_delay(val, PLLE_SS_CTRL);
	val &= ~PLLE_SS_CNTL_INTERP_RESET;
	pll_writel_delay(val, PLLE_SS_CTRL);
#endif
#if !USE_PLLE_SWCTL
	/* switch pll under h/w control */
	val = clk_readl(c->reg + PLL_MISC(c));
	val &= ~PLLE_MISC_IDDQ_SW_CTRL;
	clk_writel(val, c->reg + PLL_MISC(c));

	val = clk_readl(PLLE_AUX);
	val |= PLLE_AUX_USE_LOCKDET | PLLE_AUX_SEQ_START_STATE;
	val &= ~(PLLE_AUX_ENABLE_SWCTL | PLLE_AUX_SS_SWCTL);
	pll_writel_delay(val, PLLE_AUX);
	val |= PLLE_AUX_SEQ_ENABLE;
	pll_writel_delay(val, PLLE_AUX);
#endif

	/* clear XUSB PLL pad pwr override and iddq */
	val = xusb_padctl_readl(XUSB_PADCTL_IOPHY_PLL_P0_CTL1_0);
	val &= ~XUSB_PADCTL_IOPHY_PLL_P0_CTL1_0_PLL_PWR_OVRD;
	val &= ~XUSB_PADCTL_IOPHY_PLL_P0_CTL1_0_PLL_IDDQ;
	xusb_padctl_writel(val, XUSB_PADCTL_IOPHY_PLL_P0_CTL1_0);

	/* enable hw control of xusb brick pll */
	usb_plls_hw_control_enable(XUSBIO_PLL_CFG0);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static void tegra12_plle_clk_resume(struct clk *c)
{
	u32 val = clk_readl(c->reg + PLL_BASE);
	if (val & PLL_BASE_ENABLE)
		return;		/* already resumed */

	/* Restore parent */
	val = clk_readl(PLLE_AUX);
	select_pll_e_input(val);
}
#endif

static struct clk_ops tegra_plle_ops = {
	.init			= tegra12_plle_clk_init,
	.enable			= tegra12_plle_clk_enable,
	.disable		= tegra12_plle_clk_disable,
};

/*
 * Tegra12 includes dynamic frequency lock loop (DFLL) with automatic voltage
 * control as possible CPU clock source. It is included in the Tegra12 clock
 * tree as "complex PLL" with standard Tegra clock framework APIs. However,
 * DFLL locking logic h/w access APIs are separated in the tegra_cl_dvfs.c
 * module. Hence, DFLL operations, with the exception of initialization, are
 * basically cl-dvfs wrappers.
 */

/* DFLL operations */
#ifdef	CONFIG_ARCH_TEGRA_HAS_CL_DVFS
static void tune_cpu_trimmers(bool trim_high)
{
	tegra_soctherm_adjust_cpu_zone(trim_high);
}
#endif

static void __init tegra12_dfll_cpu_late_init(struct clk *c)
{
#ifdef CONFIG_ARCH_TEGRA_HAS_CL_DVFS
	int ret;
	struct clk *cpu = tegra_get_clock_by_name("cpu_g");

	if (!cpu || !cpu->dvfs) {
		pr_err("%s: CPU dvfs is not present\n", __func__);
		return;
	}
	tegra_dvfs_set_dfll_tune_trimmers(cpu->dvfs, tune_cpu_trimmers);

	/* release dfll clock source reset, init cl_dvfs control logic, and
	   move dfll to initialized state, so it can be used as CPU source */
	tegra_periph_reset_deassert(c);
	ret = tegra_init_cl_dvfs();
	if (!ret) {
		c->state = OFF;
		c->u.dfll.cl_dvfs = platform_get_drvdata(&tegra_cl_dvfs_device);
		if (tegra_platform_is_silicon())
			use_dfll = CONFIG_TEGRA_USE_DFLL_RANGE;
		tegra_dvfs_set_dfll_range(cpu->dvfs, use_dfll);
		tegra_cl_dvfs_debug_init(c);
		pr_info("Tegra CPU DFLL is initialized with use_dfll = %d\n", use_dfll);
	}
#endif
}

static void tegra12_dfll_clk_init(struct clk *c)
{
	c->ops->init = tegra12_dfll_cpu_late_init;
}

static int tegra12_dfll_clk_enable(struct clk *c)
{
	return tegra_cl_dvfs_enable(c->u.dfll.cl_dvfs);
}

static void tegra12_dfll_clk_disable(struct clk *c)
{
	tegra_cl_dvfs_disable(c->u.dfll.cl_dvfs);
}

static int tegra12_dfll_clk_set_rate(struct clk *c, unsigned long rate)
{
	int ret = tegra_cl_dvfs_request_rate(c->u.dfll.cl_dvfs, rate);

	if (!ret)
		c->rate = tegra_cl_dvfs_request_get(c->u.dfll.cl_dvfs);

	return ret;
}

static void tegra12_dfll_clk_reset(struct clk *c, bool assert)
{
	u32 val = assert ? DFLL_BASE_RESET : 0;
	clk_writel_delay(val, c->reg);
}

static int
tegra12_dfll_clk_cfg_ex(struct clk *c, enum tegra_clk_ex_param p, u32 setting)
{
	if (p == TEGRA_CLK_DFLL_LOCK)
		return setting ? tegra_cl_dvfs_lock(c->u.dfll.cl_dvfs) :
				 tegra_cl_dvfs_unlock(c->u.dfll.cl_dvfs);
	return -EINVAL;
}

#ifdef CONFIG_PM_SLEEP
static void tegra12_dfll_clk_resume(struct clk *c)
{
	if (!(clk_readl(c->reg) & DFLL_BASE_RESET))
		return;		/* already resumed */

	if (c->state != UNINITIALIZED) {
		tegra_periph_reset_deassert(c);
		tegra_cl_dvfs_resume(c->u.dfll.cl_dvfs);
	}
}
#endif

static struct clk_ops tegra_dfll_ops = {
	.init			= tegra12_dfll_clk_init,
	.enable			= tegra12_dfll_clk_enable,
	.disable		= tegra12_dfll_clk_disable,
	.set_rate		= tegra12_dfll_clk_set_rate,
	.reset			= tegra12_dfll_clk_reset,
	.clk_cfg_ex		= tegra12_dfll_clk_cfg_ex,
};

/* DFLL sysfs interface */
static int tegra12_use_dfll_cb(const char *arg, const struct kernel_param *kp)
{
	int ret = 0;
	unsigned long c_flags, p_flags;
	unsigned int old_use_dfll;
	struct clk *c = tegra_get_clock_by_name("cpu");
	struct clk *dfll = tegra_get_clock_by_name("dfll_cpu");

	if (!c->parent || !c->parent->dvfs || !dfll)
		return -ENOSYS;

	ret = tegra_cpu_reg_mode_force_normal(true);
	if (ret) {
		pr_err("%s: Failed to force regulator normal mode\n", __func__);
		return ret;
	}

	clk_lock_save(c, &c_flags);
	if (dfll->state == UNINITIALIZED) {
		pr_err("%s: DFLL is not initialized\n", __func__);
		clk_unlock_restore(c, &c_flags);
		tegra_cpu_reg_mode_force_normal(false);
		return -ENOSYS;
	}
	if (c->parent->u.cpu.mode == MODE_LP) {
		pr_err("%s: DFLL is not used on LP CPU\n", __func__);
		clk_unlock_restore(c, &c_flags);
		tegra_cpu_reg_mode_force_normal(false);
		return -ENOSYS;
	}

	clk_lock_save(c->parent, &p_flags);
	old_use_dfll = use_dfll;
	param_set_int(arg, kp);

	if (use_dfll != old_use_dfll) {
		ret = tegra_dvfs_set_dfll_range(c->parent->dvfs, use_dfll);
		if (ret) {
			use_dfll = old_use_dfll;
		} else {
			ret = clk_set_rate_locked(c->parent,
				clk_get_rate_locked(c->parent));
			if (ret) {
				use_dfll = old_use_dfll;
				tegra_dvfs_set_dfll_range(
					c->parent->dvfs, use_dfll);
			}
		}
	}
	clk_unlock_restore(c->parent, &p_flags);
	clk_unlock_restore(c, &c_flags);
	tegra_update_cpu_edp_limits();
	return ret;
}

static struct kernel_param_ops tegra12_use_dfll_ops = {
	.set = tegra12_use_dfll_cb,
	.get = param_get_int,
};
module_param_cb(use_dfll, &tegra12_use_dfll_ops, &use_dfll, 0644);


/* Clock divider ops (non-atomic shared register access) */
static DEFINE_SPINLOCK(pll_div_lock);

static int tegra12_pll_div_clk_set_rate(struct clk *c, unsigned long rate);
static void tegra12_pll_div_clk_init(struct clk *c)
{
	if (c->flags & DIV_U71) {
		u32 val, divu71;
		if (c->parent->state == OFF)
			c->ops->disable(c);

		val = clk_readl(c->reg);
		val >>= c->reg_shift;
		c->state = (val & PLL_OUT_CLKEN) ? ON : OFF;
		if (!(val & PLL_OUT_RESET_DISABLE))
			c->state = OFF;

		if (c->u.pll_div.default_rate) {
			int ret = tegra12_pll_div_clk_set_rate(
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
	} else if (c->flags & PLLU) {
		u32 val = clk_readl(c->reg);
		c->state = val & (0x1 << c->reg_shift) ? ON : OFF;
	} else {
		c->state = ON;
		c->div = 1;
		c->mul = 1;
	}
}

static int tegra12_pll_div_clk_enable(struct clk *c)
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
	} else if (c->flags & PLLU) {
		clk_lock_save(c->parent, &flags);
		val = clk_readl(c->reg) | (0x1 << c->reg_shift);
		clk_writel_delay(val, c->reg);
		clk_unlock_restore(c->parent, &flags);
		return 0;
	}
	return -EINVAL;
}

static void tegra12_pll_div_clk_disable(struct clk *c)
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
	} else if (c->flags & PLLU) {
		clk_lock_save(c->parent, &flags);
		val = clk_readl(c->reg) & (~(0x1 << c->reg_shift));
		clk_writel_delay(val, c->reg);
		clk_unlock_restore(c->parent, &flags);
	}
}

static int tegra12_pll_div_clk_set_rate(struct clk *c, unsigned long rate)
{
	u32 val;
	u32 new_val;
	int divider_u71;
	unsigned long parent_rate = clk_get_rate(c->parent);
	unsigned long flags;

	pr_debug("%s: %s %lu\n", __func__, c->name, rate);
	if (tegra_platform_is_qt())
		return 0;
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

static long tegra12_pll_div_clk_round_rate(struct clk *c, unsigned long rate)
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
	.init			= tegra12_pll_div_clk_init,
	.enable			= tegra12_pll_div_clk_enable,
	.disable		= tegra12_pll_div_clk_disable,
	.set_rate		= tegra12_pll_div_clk_set_rate,
	.round_rate		= tegra12_pll_div_clk_round_rate,
};

/* Periph clk ops */
static inline u32 periph_clk_source_mask(struct clk *c)
{
	if (c->u.periph.src_mask)
		return c->u.periph.src_mask;
	else if (c->flags & MUX_PWM)
		return 3 << 28;
	else if (c->flags & MUX_CLK_OUT)
		return 3 << (c->u.periph.clk_num + 4);
	else if (c->flags & PLLD)
		return PLLD_BASE_DSI_MUX_MASK;
	else
		return 7 << 29;
}

static inline u32 periph_clk_source_shift(struct clk *c)
{
	if (c->u.periph.src_shift)
		return c->u.periph.src_shift;
	else if (c->flags & MUX_PWM)
		return 28;
	else if (c->flags & MUX_CLK_OUT)
		return c->u.periph.clk_num + 4;
	else if (c->flags & PLLD)
		return PLLD_BASE_DSI_MUX_SHIFT;
	else
		return 29;
}

static void tegra12_periph_clk_init(struct clk *c)
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
		if (c->flags & PLLU) {
			/* for xusb_hs clock enforce SS div2 source */
			val &= ~periph_clk_source_mask(c);
			clk_writel_delay(val, c->reg);
		}
		c->parent = c->inputs[0].input;
	}

	/* if peripheral is left under reset - enforce safe rate */
	if (!(c->flags & PERIPH_NO_RESET) &&
	    (clk_readl(PERIPH_CLK_TO_RST_REG(c)) & PERIPH_CLK_TO_BIT(c))) {
		tegra_periph_clk_safe_rate_init(c);
		 val = clk_readl(c->reg);
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

	if (c->flags & PERIPH_NO_ENB) {
		c->state = c->parent->state;
		return;
	}

	c->state = ON;

	if (!(clk_readl(PERIPH_CLK_TO_ENB_REG(c)) & PERIPH_CLK_TO_BIT(c)))
		c->state = OFF;
	if (!(c->flags & PERIPH_NO_RESET))
		if (clk_readl(PERIPH_CLK_TO_RST_REG(c)) & PERIPH_CLK_TO_BIT(c))
			c->state = OFF;
}

static int tegra12_periph_clk_enable(struct clk *c)
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

static void tegra12_periph_clk_disable(struct clk *c)
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
			val = tegra_read_chipid();

		clk_writel_delay(
			PERIPH_CLK_TO_BIT(c), PERIPH_CLK_TO_ENB_CLR_REG(c));
	}
	spin_unlock_irqrestore(&periph_refcount_lock, flags);
}

static void tegra12_periph_clk_reset(struct clk *c, bool assert)
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
				val = tegra_read_chipid();

			clk_writel(PERIPH_CLK_TO_BIT(c),
				   PERIPH_CLK_TO_RST_SET_REG(c));
		} else
			clk_writel(PERIPH_CLK_TO_BIT(c),
				   PERIPH_CLK_TO_RST_CLR_REG(c));
	}
}

static int tegra12_periph_clk_set_parent(struct clk *c, struct clk *p)
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
				clk_enable(p);

			clk_writel_delay(val, c->reg);

			if (c->refcnt && c->parent)
				clk_disable(c->parent);

			clk_reparent(c, p);
			return 0;
		}
	}

	return -EINVAL;
}

static int tegra12_periph_clk_set_rate(struct clk *c, unsigned long rate)
{
	u32 val;
	int divider;
	unsigned long parent_rate = clk_get_rate(c->parent);

	if (tegra_platform_is_qt())
		return 0;
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

static long tegra12_periph_clk_round_rate(struct clk *c,
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
	.init			= &tegra12_periph_clk_init,
	.enable			= &tegra12_periph_clk_enable,
	.disable		= &tegra12_periph_clk_disable,
	.set_parent		= &tegra12_periph_clk_set_parent,
	.set_rate		= &tegra12_periph_clk_set_rate,
	.round_rate		= &tegra12_periph_clk_round_rate,
	.reset			= &tegra12_periph_clk_reset,
};

/* 1x shared bus ops */
static long _1x_round_updown(struct clk *c, struct clk *src,
			     unsigned long rate, bool up)
{
	int divider;
	unsigned long source_rate, round_rate;

	source_rate = clk_get_rate(src);

	divider = clk_div71_get_divider(source_rate, rate + (up ? -1 : 1),
		c->flags, up ? ROUND_DIVIDER_DOWN : ROUND_DIVIDER_UP);

	if (divider < 0)
		return c->min_rate;

	round_rate = source_rate * 2 / (divider + 2);

	if (round_rate > c->max_rate) {
		divider += c->flags & DIV_U71_INT ? 2 : 1;
#if !DIVIDER_1_5_ALLOWED
		divider = max(2, divider);
#endif
		round_rate = source_rate * 2 / (divider + 2);
	}
	return round_rate;
}

static long tegra12_1xbus_round_updown(struct clk *c, unsigned long rate,
					    bool up)
{
	unsigned long pll_low_rate, pll_high_rate;

	rate = max(rate, c->min_rate);

	pll_low_rate = _1x_round_updown(c, c->u.periph.pll_low, rate, up);
	if (rate <= c->u.periph.threshold) {
		c->u.periph.pll_selected = c->u.periph.pll_low;
		return pll_low_rate;
	}

	pll_high_rate = _1x_round_updown(c, c->u.periph.pll_high, rate, up);
	if (pll_high_rate <= c->u.periph.threshold) {
		c->u.periph.pll_selected = c->u.periph.pll_low;
		return pll_low_rate;  /* prevent oscillation across threshold */
	}

	if (up) {
		/* rounding up: both plls may hit max, and round down */
		if (pll_high_rate < rate) {
			if (pll_low_rate < pll_high_rate) {
				c->u.periph.pll_selected = c->u.periph.pll_high;
				return pll_high_rate;
			}
		} else {
			if ((pll_low_rate < rate) ||
			    (pll_low_rate > pll_high_rate)) {
				c->u.periph.pll_selected = c->u.periph.pll_high;
				return pll_high_rate;
			}
		}
	} else if (pll_low_rate < pll_high_rate) {
		/* rounding down: to get here both plls able to round down */
		c->u.periph.pll_selected = c->u.periph.pll_high;
		return pll_high_rate;
	}
	c->u.periph.pll_selected = c->u.periph.pll_low;
	return pll_low_rate;
}

static long tegra12_1xbus_round_rate(struct clk *c, unsigned long rate)
{
	return tegra12_1xbus_round_updown(c, rate, true);
}

static int tegra12_1xbus_set_rate(struct clk *c, unsigned long rate)
{
	/* Compensate rate truncating during rounding */
	return tegra12_periph_clk_set_rate(c, rate + 1);
}

static int tegra12_clk_1xbus_update(struct clk *c)
{
	int ret;
	struct clk *new_parent;
	unsigned long rate, old_rate;

	if (detach_shared_bus)
		return 0;

	rate = tegra12_clk_shared_bus_update(c, NULL, NULL, NULL);

	old_rate = clk_get_rate_locked(c);
	pr_debug("\n1xbus %s: rate %lu on parent %s: new request %lu\n",
		 c->name, old_rate, c->parent->name, rate);
	if (rate == old_rate)
		return 0;

	if (!c->u.periph.min_div_low || !c->u.periph.min_div_high) {
		unsigned long r, m = c->max_rate;
		r = clk_get_rate(c->u.periph.pll_low);
		c->u.periph.min_div_low = DIV_ROUND_UP(r, m) * c->mul;
		r = clk_get_rate(c->u.periph.pll_high);
		c->u.periph.min_div_high = DIV_ROUND_UP(r, m) * c->mul;
	}

	new_parent = c->u.periph.pll_selected;

	/*
	 * The transition procedure below is guaranteed to switch to the target
	 * parent/rate without violation of max clock limits. It would attempt
	 * to switch without dip in bus rate if it is possible, but this cannot
	 * be guaranteed (example: switch from 408 MHz : 1 to 624 MHz : 2 with
	 * maximum bus limit 408 MHz will be executed as 408 => 204 => 312 MHz,
	 * and there is no way to avoid rate dip in this case).
	 */
	if (new_parent != c->parent) {
		int interim_div = 0;
		/* Switching to pll_high may over-clock bus if current divider
		   is too small - increase divider to safe value */
		if ((new_parent == c->u.periph.pll_high) &&
		    (c->div < c->u.periph.min_div_high))
			interim_div = c->u.periph.min_div_high;

		/* Switching to pll_low may dip down rate if current divider
		   is too big - decrease divider as much as we can */
		if ((new_parent == c->u.periph.pll_low) &&
		    (c->div > c->u.periph.min_div_low) &&
		    (c->div > c->u.periph.min_div_high))
			interim_div = c->u.periph.min_div_low;

		if (interim_div) {
			u64 interim_rate = old_rate * c->div;
			do_div(interim_rate, interim_div);
			ret = clk_set_rate_locked(c, interim_rate);
			if (ret) {
				pr_err("Failed to set %s rate to %lu\n",
				       c->name, (unsigned long)interim_rate);
				return ret;
			}
			pr_debug("1xbus %s: rate %lu on parent %s\n", c->name,
				 clk_get_rate_locked(c), c->parent->name);
		}

		ret = clk_set_parent_locked(c, new_parent);
		if (ret) {
			pr_err("Failed to set %s parent %s\n",
			       c->name, new_parent->name);
			return ret;
		}

		old_rate = clk_get_rate_locked(c);
		pr_debug("1xbus %s: rate %lu on parent %s\n", c->name,
			 old_rate, c->parent->name);
		if (rate == old_rate)
			return 0;
	}

	ret = clk_set_rate_locked(c, rate);
	if (ret) {
		pr_err("Failed to set %s rate to %lu\n", c->name, rate);
		return ret;
	}
	pr_debug("1xbus %s: rate %lu on parent %s\n", c->name,
		 clk_get_rate_locked(c), c->parent->name);
	return 0;

}

static struct clk_ops tegra_1xbus_clk_ops = {
	.init			= &tegra12_periph_clk_init,
	.enable			= &tegra12_periph_clk_enable,
	.disable		= &tegra12_periph_clk_disable,
	.set_parent		= &tegra12_periph_clk_set_parent,
	.set_rate		= &tegra12_1xbus_set_rate,
	.round_rate		= &tegra12_1xbus_round_rate,
	.round_rate_updown	= &tegra12_1xbus_round_updown,
	.reset			= &tegra12_periph_clk_reset,
	.shared_bus_update	= &tegra12_clk_1xbus_update,
};

/* msenc clock propagation WAR for bug 1005168 */
static int tegra12_msenc_clk_enable(struct clk *c)
{
	int ret = tegra12_periph_clk_enable(c);
	if (ret)
		return ret;

	clk_writel(0, LVL2_CLK_GATE_OVRE);
	clk_writel(0x00400000, LVL2_CLK_GATE_OVRE);
	udelay(1);
	clk_writel(0, LVL2_CLK_GATE_OVRE);
	return 0;
}

static struct clk_ops tegra_msenc_clk_ops = {
	.init			= &tegra12_periph_clk_init,
	.enable			= &tegra12_msenc_clk_enable,
	.disable		= &tegra12_periph_clk_disable,
	.set_parent		= &tegra12_periph_clk_set_parent,
	.set_rate		= &tegra12_periph_clk_set_rate,
	.round_rate		= &tegra12_periph_clk_round_rate,
	.reset			= &tegra12_periph_clk_reset,
};
/* Periph extended clock configuration ops */
static int
tegra12_vi_clk_cfg_ex(struct clk *c, enum tegra_clk_ex_param p, u32 setting)
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
	.init			= &tegra12_periph_clk_init,
	.enable			= &tegra12_periph_clk_enable,
	.disable		= &tegra12_periph_clk_disable,
	.set_parent		= &tegra12_periph_clk_set_parent,
	.set_rate		= &tegra12_periph_clk_set_rate,
	.round_rate		= &tegra12_periph_clk_round_rate,
	.clk_cfg_ex		= &tegra12_vi_clk_cfg_ex,
	.reset			= &tegra12_periph_clk_reset,
};

static int
tegra12_sor_clk_cfg_ex(struct clk *c, enum tegra_clk_ex_param p, u32 setting)
{
	if (p == TEGRA_CLK_SOR_CLK_SEL) {
		u32 val = clk_readl(c->reg);
		val &= ~PERIPH_CLK_SOR_CLK_SEL_MASK;
		val |= (setting << PERIPH_CLK_SOR_CLK_SEL_SHIFT) &
			PERIPH_CLK_SOR_CLK_SEL_MASK;
		clk_writel(val, c->reg);
		return 0;
	}
	return -EINVAL;
}

static struct clk_ops tegra_sor_clk_ops = {
	.init			= &tegra12_periph_clk_init,
	.enable			= &tegra12_periph_clk_enable,
	.disable		= &tegra12_periph_clk_disable,
	.set_parent		= &tegra12_periph_clk_set_parent,
	.set_rate		= &tegra12_periph_clk_set_rate,
	.round_rate		= &tegra12_periph_clk_round_rate,
	.clk_cfg_ex		= &tegra12_sor_clk_cfg_ex,
	.reset			= &tegra12_periph_clk_reset,
};

static int
tegra12_dtv_clk_cfg_ex(struct clk *c, enum tegra_clk_ex_param p, u32 setting)
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
	.init			= &tegra12_periph_clk_init,
	.enable			= &tegra12_periph_clk_enable,
	.disable		= &tegra12_periph_clk_disable,
	.set_parent		= &tegra12_periph_clk_set_parent,
	.set_rate		= &tegra12_periph_clk_set_rate,
	.round_rate		= &tegra12_periph_clk_round_rate,
	.clk_cfg_ex		= &tegra12_dtv_clk_cfg_ex,
	.reset			= &tegra12_periph_clk_reset,
};

static struct clk_ops tegra_dsi_clk_ops = {
	.init			= &tegra12_periph_clk_init,
	.enable			= &tegra12_periph_clk_enable,
	.disable		= &tegra12_periph_clk_disable,
	.set_rate		= &tegra12_periph_clk_set_rate,
	.round_rate		= &tegra12_periph_clk_round_rate,
	.reset			= &tegra12_periph_clk_reset,
};

/* pciex clock support only reset function */
static void tegra12_pciex_clk_init(struct clk *c)
{
	c->state = c->parent->state;
}

static int tegra12_pciex_clk_enable(struct clk *c)
{
	return 0;
}

static void tegra12_pciex_clk_disable(struct clk *c)
{
}

static int tegra12_pciex_clk_set_rate(struct clk *c, unsigned long rate)
{
	unsigned long parent_rate = clk_get_rate(c->parent);

	/*
	 * the only supported pcie configurations:
	 * Gen1: plle = 100MHz, link at 250MHz
	 * Gen2: plle = 100MHz, link at 500MHz
	 */
	if (parent_rate == 100000000) {
		if (rate == 500000000) {
			c->mul = 5;
			c->div = 1;
			return 0;
		} else if (rate == 250000000) {
			c->mul = 5;
			c->div = 2;
			return 0;
		}
	}
	return -EINVAL;
}

static struct clk_ops tegra_pciex_clk_ops = {
	.init     = tegra12_pciex_clk_init,
	.enable	  = tegra12_pciex_clk_enable,
	.disable  = tegra12_pciex_clk_disable,
	.set_rate = tegra12_pciex_clk_set_rate,
	.reset    = tegra12_periph_clk_reset,
};

/* Output clock ops */

static DEFINE_SPINLOCK(clk_out_lock);

static void tegra12_clk_out_init(struct clk *c)
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

static int tegra12_clk_out_enable(struct clk *c)
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

static void tegra12_clk_out_disable(struct clk *c)
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

static int tegra12_clk_out_set_parent(struct clk *c, struct clk *p)
{
	u32 val;
	unsigned long flags;
	const struct clk_mux_sel *sel;

	pr_debug("%s: %s %s\n", __func__, c->name, p->name);

	for (sel = c->inputs; sel->input != NULL; sel++) {
		if (sel->input == p) {
			if (c->refcnt)
				clk_enable(p);

			spin_lock_irqsave(&clk_out_lock, flags);
			val = pmc_readl(c->reg);
			val &= ~periph_clk_source_mask(c);
			val |= (sel->value << periph_clk_source_shift(c));
			pmc_writel(val, c->reg);
			spin_unlock_irqrestore(&clk_out_lock, flags);

			if (c->refcnt && c->parent)
				clk_disable(c->parent);

			clk_reparent(c, p);
			return 0;
		}
	}
	return -EINVAL;
}

static struct clk_ops tegra_clk_out_ops = {
	.init			= &tegra12_clk_out_init,
	.enable			= &tegra12_clk_out_enable,
	.disable		= &tegra12_clk_out_disable,
	.set_parent		= &tegra12_clk_out_set_parent,
};


/* External memory controller clock ops */
static void tegra12_emc_clk_init(struct clk *c)
{
	tegra12_periph_clk_init(c);
	tegra_emc_dram_type_init(c);
}

static long tegra12_emc_clk_round_updown(struct clk *c, unsigned long rate,
					 bool up)
{
	unsigned long new_rate = max(rate, c->min_rate);

	new_rate = tegra_emc_round_rate_updown(new_rate, up);
	if (IS_ERR_VALUE(new_rate))
		new_rate = c->max_rate;

	return new_rate;
}

static long tegra12_emc_clk_round_rate(struct clk *c, unsigned long rate)
{
	return tegra12_emc_clk_round_updown(c, rate, true);
}

static int tegra12_emc_clk_set_rate(struct clk *c, unsigned long rate)
{
	int ret;
	u32 div_value;
	struct clk *p;

	if (tegra_platform_is_qt())
		return 0;
	/* The tegra12x memory controller has an interlock with the clock
	 * block that allows memory shadowed registers to be updated,
	 * and then transfer them to the main registers at the same
	 * time as the clock update without glitches. During clock change
	 * operation both clock parent and divider may change simultaneously
	 * to achieve requested rate. */
	p = tegra_emc_predict_parent(rate, &div_value);
	div_value += 2;		/* emc has fractional DIV_U71 divider */
	if (IS_ERR_OR_NULL(p)) {
		pr_err("%s: Failed to predict emc parent for rate %lu\n",
		       __func__, rate);
		return -EINVAL;
	}

	if (p == c->parent) {
		if (div_value == c->div)
			return 0;
	} else if (c->refcnt)
		clk_enable(p);

	ret = tegra_emc_set_rate(rate);
	if (ret < 0)
		return ret;

	if (p != c->parent) {
		if(c->refcnt && c->parent)
			clk_disable(c->parent);
		clk_reparent(c, p);
	}
	c->div = div_value;
	c->mul = 2;
	return 0;
}

static int tegra12_clk_emc_bus_update(struct clk *bus)
{
	struct clk *p = NULL;
	unsigned long rate, old_rate, parent_rate, backup_rate;

	if (detach_shared_bus)
		return 0;

	rate = tegra12_clk_shared_bus_update(bus, NULL, NULL, NULL);

	old_rate = clk_get_rate_locked(bus);
	if (rate == old_rate)
		return 0;

	if (!tegra_emc_is_parent_ready(rate, &p, &parent_rate, &backup_rate)) {
		if (bus->parent == p) {
			/* need backup to re-lock current parent */
			int ret;
			if (IS_ERR_VALUE(backup_rate)) {
				pr_err("%s: No backup for %s rate %lu\n",
				       __func__, bus->name, rate);
				return -EINVAL;
			}

			/* set volatge for backup rate if going up */
			if (backup_rate > old_rate) {
				ret = tegra_dvfs_set_rate(bus, backup_rate);
				if (ret) {
					pr_err("%s: dvfs failed on %s rate %lu\n",
					      __func__, bus->name, backup_rate);
					return -EINVAL;
				}
			}

			trace_clock_set_rate(bus->name, backup_rate, 0);
			ret = bus->ops->set_rate(bus, backup_rate);
			if (ret) {
				pr_err("%s: Failed to backup %s for rate %lu\n",
				       __func__, bus->name, rate);
				return -EINVAL;
			}
			clk_rate_change_notify(bus, backup_rate);
		}
		if (p->refcnt) {
			pr_err("%s: %s has other than emc child\n",
			       __func__, p->name);
			return -EINVAL;
		}

		if (clk_set_rate(p, parent_rate)) {
			pr_err("%s: Failed to set %s rate %lu\n",
			       __func__, p->name, parent_rate);
			return -EINVAL;
		}
	}

	return clk_set_rate_locked(bus, rate);
}

static struct clk_ops tegra_emc_clk_ops = {
	.init			= &tegra12_emc_clk_init,
	.enable			= &tegra12_periph_clk_enable,
	.disable		= &tegra12_periph_clk_disable,
	.set_rate		= &tegra12_emc_clk_set_rate,
	.round_rate		= &tegra12_emc_clk_round_rate,
	.round_rate_updown	= &tegra12_emc_clk_round_updown,
	.reset			= &tegra12_periph_clk_reset,
	.shared_bus_update	= &tegra12_clk_emc_bus_update,
};

void tegra_mc_divider_update(struct clk *emc)
{
	emc->child_bus->div = (clk_readl(emc->reg) &
			       PERIPH_CLK_SOURCE_EMC_MC_SAME) ? 1 : 2;
}

static void tegra12_mc_clk_init(struct clk *c)
{
	c->state = ON;
	if (!(clk_readl(PERIPH_CLK_TO_ENB_REG(c)) & PERIPH_CLK_TO_BIT(c)))
		c->state = OFF;

	c->parent->child_bus = c;
	tegra_mc_divider_update(c->parent);
	c->mul = 1;
}

static struct clk_ops tegra_mc_clk_ops = {
	.init			= &tegra12_mc_clk_init,
	.enable			= &tegra12_periph_clk_enable,
	.disable		= &tegra12_periph_clk_disable,
};

/* Clock doubler ops (non-atomic shared register access) */
static DEFINE_SPINLOCK(doubler_lock);

static void tegra12_clk_double_init(struct clk *c)
{
	u32 val = clk_readl(c->reg);
	c->mul = val & (0x1 << c->reg_shift) ? 1 : 2;
	c->div = 1;
	c->state = ON;
	if (!(clk_readl(PERIPH_CLK_TO_ENB_REG(c)) & PERIPH_CLK_TO_BIT(c)))
		c->state = OFF;
};

static int tegra12_clk_double_set_rate(struct clk *c, unsigned long rate)
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
	.init			= &tegra12_clk_double_init,
	.enable			= &tegra12_periph_clk_enable,
	.disable		= &tegra12_periph_clk_disable,
	.set_rate		= &tegra12_clk_double_set_rate,
};

/* Audio sync clock ops */
static int tegra12_sync_source_set_rate(struct clk *c, unsigned long rate)
{
	c->rate = rate;
	return 0;
}

static struct clk_ops tegra_sync_source_ops = {
	.set_rate		= &tegra12_sync_source_set_rate,
};

static void tegra12_audio_sync_clk_init(struct clk *c)
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

static int tegra12_audio_sync_clk_enable(struct clk *c)
{
	u32 val = clk_readl(c->reg);
	clk_writel((val & (~AUDIO_SYNC_DISABLE_BIT)), c->reg);
	return 0;
}

static void tegra12_audio_sync_clk_disable(struct clk *c)
{
	u32 val = clk_readl(c->reg);
	clk_writel((val | AUDIO_SYNC_DISABLE_BIT), c->reg);
}

static int tegra12_audio_sync_clk_set_parent(struct clk *c, struct clk *p)
{
	u32 val;
	const struct clk_mux_sel *sel;
	for (sel = c->inputs; sel->input != NULL; sel++) {
		if (sel->input == p) {
			val = clk_readl(c->reg);
			val &= ~AUDIO_SYNC_SOURCE_MASK;
			val |= sel->value;

			if (c->refcnt)
				clk_enable(p);

			clk_writel(val, c->reg);

			if (c->refcnt && c->parent)
				clk_disable(c->parent);

			clk_reparent(c, p);
			return 0;
		}
	}

	return -EINVAL;
}

static struct clk_ops tegra_audio_sync_clk_ops = {
	.init       = tegra12_audio_sync_clk_init,
	.enable     = tegra12_audio_sync_clk_enable,
	.disable    = tegra12_audio_sync_clk_disable,
	.set_parent = tegra12_audio_sync_clk_set_parent,
};

/* cml0 (pcie), and cml1 (sata) clock ops */
static void tegra12_cml_clk_init(struct clk *c)
{
	u32 val = clk_readl(c->reg);
	c->state = val & (0x1 << c->u.periph.clk_num) ? ON : OFF;
}

static int tegra12_cml_clk_enable(struct clk *c)
{
	u32 val = clk_readl(c->reg);
	val |= (0x1 << c->u.periph.clk_num);
	clk_writel(val, c->reg);
	return 0;
}

static void tegra12_cml_clk_disable(struct clk *c)
{
	u32 val = clk_readl(c->reg);
	val &= ~(0x1 << c->u.periph.clk_num);
	clk_writel(val, c->reg);
}

static struct clk_ops tegra_cml_clk_ops = {
	.init			= &tegra12_cml_clk_init,
	.enable			= &tegra12_cml_clk_enable,
	.disable		= &tegra12_cml_clk_disable,
};


/* cbus ops */
/*
 * Some clocks require dynamic re-locking of source PLL in order to
 * achieve frequency scaling granularity that matches characterized
 * core voltage steps. The cbus clock creates a shared bus that
 * provides a virtual root for such clocks to hide and synchronize
 * parent PLL re-locking as well as backup operations.
*/

static void tegra12_clk_cbus_init(struct clk *c)
{
	c->state = OFF;
	c->set = true;
}

static int tegra12_clk_cbus_enable(struct clk *c)
{
	return 0;
}

static long tegra12_clk_cbus_round_updown(struct clk *c, unsigned long rate,
					  bool up)
{
	int i;
	const int *millivolts;

	if (!c->dvfs) {
		if (!c->min_rate)
			c->min_rate = c->parent->min_rate;
		rate = max(rate, c->min_rate);
		return rate;
	}

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

	millivolts = tegra_dvfs_get_millivolts_pll(c->dvfs);
	for (i = 0; ; i++) {
		unsigned long f = c->dvfs->freqs[i];
		int mv = millivolts[i];
		if ((f >= rate) || (mv >= c->dvfs->max_millivolts) ||
		    ((i + 1) >=  c->dvfs->num_freqs)) {
			if (!up && i && (f > rate))
				i--;
			break;
		}
	}
	return c->dvfs->freqs[i];
}

static long tegra12_clk_cbus_round_rate(struct clk *c, unsigned long rate)
{
	return tegra12_clk_cbus_round_updown(c, rate, true);
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

	if (c->parent != p) {
		ret = clk_set_parent(c, p);
		if (ret) {
			pr_err("%s: failed to set %s clock parent %s: %d\n",
			       __func__, c->name, p->name, ret);
			if (abort)
				return ret;
		}
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
		struct clk *client = user->u.shared_bus_user.client;
		if (client && (client->state == ON) &&
		    (client->parent == c->parent)) {
			ret = cbus_switch_one(client,
					      c->shared_bus_backup.input,
					      c->shared_bus_backup.value *
					      user->div, true);
			if (ret)
				return ret;
		}
	}
	return 0;
}

static int cbus_dvfs_set_rate(struct clk *c, unsigned long rate)
{
	int ret;
	struct clk *user;

	list_for_each_entry(user, &c->shared_bus_list,
			u.shared_bus_user.node) {
		struct clk *client =  user->u.shared_bus_user.client;
		if (client && client->refcnt && (client->parent == c->parent)) {
			ret = tegra_dvfs_set_rate(c, rate);
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
		if (user->u.shared_bus_user.client)
			cbus_switch_one(user->u.shared_bus_user.client,
					c->parent, c->div * user->div, false);
	}
}

static int get_next_backup_div(struct clk *c, unsigned long rate)
{
	u32 div = c->div;
	unsigned long backup_rate = clk_get_rate(c->shared_bus_backup.input);

	rate = max(rate, clk_get_rate_locked(c));
	rate = rate - (rate >> 2);	/* 25% margin for backup rate */
	if ((u64)rate * div < backup_rate)
		div = DIV_ROUND_UP(backup_rate, rate);

	BUG_ON(!div);
	return div;
}

static int tegra12_clk_cbus_set_rate(struct clk *c, unsigned long rate)
{
	int ret;
	bool dramp;

	if (rate == 0)
		return 0;
	if (tegra_platform_is_qt())
		return 0;
	ret = clk_enable(c->parent);
	if (ret) {
		pr_err("%s: failed to enable %s clock: %d\n",
		       __func__, c->name, ret);
		return ret;
	}

	dramp = tegra12_is_dyn_ramp(c->parent, rate * c->div, false);
	if (!dramp) {
		c->shared_bus_backup.value = get_next_backup_div(c, rate);
		ret = cbus_backup(c);
		if (ret)
			goto out;
	}

	ret = clk_set_rate(c->parent, rate * c->div);
	if (ret) {
		pr_err("%s: failed to set %s clock rate %lu: %d\n",
		       __func__, c->name, rate, ret);
		goto out;
	}

	/* Safe voltage setting is taken care of by cbus clock dvfs; the call
	 * below only records requirements for each enabled client.
	 */
	if (dramp)
		ret = cbus_dvfs_set_rate(c, rate);

	cbus_restore(c);

out:
	clk_disable(c->parent);
	return ret;
}

static inline void cbus_move_enabled_user(
	struct clk *user, struct clk *dst, struct clk *src)
{
	clk_enable(dst);
	list_move_tail(&user->u.shared_bus_user.node, &dst->shared_bus_list);
	clk_disable(src);
	clk_reparent(user, dst);
}

#ifdef CONFIG_TEGRA_DYNAMIC_CBUS
static int tegra12_clk_cbus_update(struct clk *bus)
{
	int ret, mv;
	struct clk *slow = NULL;
	struct clk *top = NULL;
	unsigned long rate;
	unsigned long old_rate;
	unsigned long ceiling;

	if (detach_shared_bus)
		return 0;

	rate = tegra12_clk_shared_bus_update(bus, &top, &slow, &ceiling);

	/* use dvfs table of the slowest enabled client as cbus dvfs table */
	if (bus->dvfs && slow && (slow != bus->u.cbus.slow_user)) {
		int i;
		unsigned long *dest = &bus->dvfs->freqs[0];
		unsigned long *src =
			&slow->u.shared_bus_user.client->dvfs->freqs[0];
		if (slow->div > 1)
			for (i = 0; i < bus->dvfs->num_freqs; i++)
				dest[i] = src[i] * slow->div;
		else
			memcpy(dest, src, sizeof(*dest) * bus->dvfs->num_freqs);
	}

	/* update bus state variables and rate */
	bus->u.cbus.slow_user = slow;
	bus->u.cbus.top_user = top;

	rate = tegra12_clk_cap_shared_bus(bus, rate, ceiling);
	mv = tegra_dvfs_predict_millivolts(bus, rate);
	if (IS_ERR_VALUE(mv))
		return -EINVAL;

	if (bus->dvfs) {
		mv -= bus->dvfs->cur_millivolts;
		if (bus->refcnt && (mv > 0)) {
			ret = tegra_dvfs_set_rate(bus, rate);
			if (ret)
				return ret;
		}
	}

	old_rate = clk_get_rate_locked(bus);
	if (IS_ENABLED(CONFIG_TEGRA_MIGRATE_CBUS_USERS) || (old_rate != rate)) {
		ret = bus->ops->set_rate(bus, rate);
		if (ret)
			return ret;
	}

	if (bus->dvfs) {
		if (bus->refcnt && (mv <= 0)) {
			ret = tegra_dvfs_set_rate(bus, rate);
			if (ret)
				return ret;
		}
	}

	clk_rate_change_notify(bus, rate);
	return 0;
};
#else
static int tegra12_clk_cbus_update(struct clk *bus)
{
	unsigned long rate, old_rate;

	if (detach_shared_bus)
		return 0;

	rate = tegra12_clk_shared_bus_update(bus, NULL, NULL, NULL);

	old_rate = clk_get_rate_locked(bus);
	if (rate == old_rate)
		return 0;

	return clk_set_rate_locked(bus, rate);
}
#endif

static int tegra12_clk_cbus_migrate_users(struct clk *user)
{
#ifdef CONFIG_TEGRA_MIGRATE_CBUS_USERS
	struct clk *src_bus, *dst_bus, *top_user, *c;
	struct list_head *pos, *n;

	if (!user->u.shared_bus_user.client || !user->inputs)
		return 0;

	/* Dual cbus on Tegra12 */
	src_bus = user->inputs[0].input;
	dst_bus = user->inputs[1].input;

	if (!src_bus->u.cbus.top_user && !dst_bus->u.cbus.top_user)
		return 0;

	/* Make sure top user on the source bus is requesting highest rate */
	if (!src_bus->u.cbus.top_user || (dst_bus->u.cbus.top_user &&
		bus_user_request_is_lower(src_bus->u.cbus.top_user,
					   dst_bus->u.cbus.top_user)))
		swap(src_bus, dst_bus);

	/* If top user is the slow one on its own (source) bus, do nothing */
	top_user = src_bus->u.cbus.top_user;
	BUG_ON(!top_user->u.shared_bus_user.client);
	if (!bus_user_is_slower(src_bus->u.cbus.slow_user, top_user))
		return 0;

	/* If source bus top user is slower than all users on destination bus,
	   move top user; otherwise move all users slower than the top one */
	if (!dst_bus->u.cbus.slow_user ||
	    !bus_user_is_slower(dst_bus->u.cbus.slow_user, top_user)) {
		cbus_move_enabled_user(top_user, dst_bus, src_bus);
	} else {
		list_for_each_safe(pos, n, &src_bus->shared_bus_list) {
			c = list_entry(pos, struct clk, u.shared_bus_user.node);
			if (c->u.shared_bus_user.enabled &&
			    c->u.shared_bus_user.client &&
			    bus_user_is_slower(c, top_user))
				cbus_move_enabled_user(c, dst_bus, src_bus);
		}
	}

	/* Update destination bus 1st (move clients), then source */
	tegra_clk_shared_bus_update(dst_bus);
	tegra_clk_shared_bus_update(src_bus);
#endif
	return 0;
}

static struct clk_ops tegra_clk_cbus_ops = {
	.init = tegra12_clk_cbus_init,
	.enable = tegra12_clk_cbus_enable,
	.set_rate = tegra12_clk_cbus_set_rate,
	.round_rate = tegra12_clk_cbus_round_rate,
	.round_rate_updown = tegra12_clk_cbus_round_updown,
	.shared_bus_update = tegra12_clk_cbus_update,
};

/* shared bus ops */
/*
 * Some clocks may have multiple downstream users that need to request a
 * higher clock rate.  Shared bus clocks provide a unique shared_bus_user
 * clock to each user.  The frequency of the bus is set to the highest
 * enabled shared_bus_user clock, with a minimum value set by the
 * shared bus.
 *
 * Optionally shared bus may support users migration. Since shared bus and
 * its * children (users) have reversed rate relations: user rates determine
 * bus rate, * switching user from one parent/bus to another may change rates
 * of both parents. Therefore we need a cross-bus lock on top of individual
 * user and bus locks. For now, limit bus switch support to cbus only if
 * CONFIG_TEGRA_MIGRATE_CBUS_USERS is set.
 */

static unsigned long tegra12_clk_shared_bus_update(struct clk *bus,
	struct clk **bus_top, struct clk **bus_slow, unsigned long *rate_cap)
{
	struct clk *c;
	struct clk *slow = NULL;
	struct clk *top = NULL;

	unsigned long override_rate = 0;
	unsigned long top_rate = 0;
	unsigned long rate = bus->min_rate;
	unsigned long bw = 0;
	unsigned long iso_bw = 0;
	unsigned long ceiling = bus->max_rate;
	unsigned long ceiling_but_iso = bus->max_rate;
	u32 usage_flags = 0;
	bool rate_set = false;

	list_for_each_entry(c, &bus->shared_bus_list,
			u.shared_bus_user.node) {
		bool cap_user = (c->u.shared_bus_user.mode == SHARED_CEILING) ||
			(c->u.shared_bus_user.mode == SHARED_CEILING_BUT_ISO);
		/*
		 * Ignore requests from disabled floor and bw users, and from
		 * auto-users riding the bus. Always honor ceiling users, even
		 * if they are disabled - we do not want to keep enabled parent
		 * bus just because ceiling is set. Ignore SCLK/AHB/APB dividers
		 * to propagate flat max request.
		 */
		if (c->u.shared_bus_user.enabled || cap_user) {
			unsigned long request_rate = c->u.shared_bus_user.rate;
			if (!(c->flags & DIV_BUS))
				request_rate *=	c->div ? : 1;
			usage_flags |= c->u.shared_bus_user.usage_flag;

			if (!(c->flags & BUS_RATE_LIMIT))
				rate_set = true;

			switch (c->u.shared_bus_user.mode) {
			case SHARED_ISO_BW:
				iso_bw += request_rate;
				if (iso_bw > bus->max_rate)
					iso_bw = bus->max_rate;
				/* fall thru */
			case SHARED_BW:
				bw += request_rate;
				if (bw > bus->max_rate)
					bw = bus->max_rate;
				break;
			case SHARED_CEILING_BUT_ISO:
				ceiling_but_iso =
					min(request_rate, ceiling_but_iso);
				break;
			case SHARED_CEILING:
				ceiling = min(request_rate, ceiling);
				break;
			case SHARED_OVERRIDE:
				if (override_rate == 0)
					override_rate = request_rate;
				break;
			case SHARED_AUTO:
				break;
			case SHARED_FLOOR:
			default:
				rate = max(request_rate, rate);
				if (c->u.shared_bus_user.client
							&& request_rate) {
					if (top_rate < request_rate) {
						top_rate = request_rate;
						top = c;
					} else if ((top_rate == request_rate) &&
						bus_user_is_slower(c, top)) {
						top = c;
					}
				}
			}
			if (c->u.shared_bus_user.client &&
				(!slow || bus_user_is_slower(c, slow)))
				slow = c;
		}
	}

	if (bus->flags & PERIPH_EMC_ENB) {
		unsigned long iso_bw_min;
		bw = tegra_emc_apply_efficiency(
			bw, iso_bw, bus->max_rate, usage_flags, &iso_bw_min);
		if (bus->ops && bus->ops->round_rate)
			iso_bw_min = bus->ops->round_rate(bus, iso_bw_min);
		ceiling_but_iso = max(ceiling_but_iso, iso_bw_min);
	}

	rate = override_rate ? : max(rate, bw);
	ceiling = min(ceiling, ceiling_but_iso);
	ceiling = override_rate ? bus->max_rate : ceiling;
	bus->override_rate = override_rate;

	if (bus_top && bus_slow && rate_cap) {
		/* If dynamic bus dvfs table, let the caller to complete
		   rounding and aggregation */
		*bus_top = top;
		*bus_slow = slow;
		*rate_cap = ceiling;
	} else {
		/*
		 * If satic bus dvfs table, complete rounding and aggregation.
		 * In case when no user requested bus rate, and bus retention
		 * is enabled, don't scale down - keep current rate.
		 */
		if (!rate_set && (bus->shared_bus_flags & SHARED_BUS_RETENTION))
			rate = clk_get_rate_locked(bus);
		rate = tegra12_clk_cap_shared_bus(bus, rate, ceiling);
	}

	return rate;
};

static unsigned long tegra12_clk_cap_shared_bus(struct clk *bus,
	unsigned long rate, unsigned long ceiling)
{
	if (bus->ops && bus->ops->round_rate_updown)
		ceiling = bus->ops->round_rate_updown(bus, ceiling, false);

	rate = min(rate, ceiling);

	if (bus->ops && bus->ops->round_rate)
		rate = bus->ops->round_rate(bus, rate);

	return rate;
}

static int tegra_clk_shared_bus_migrate_users(struct clk *user)
{
	if (detach_shared_bus)
		return 0;

	/* Only cbus migration is supported */
	if (user->flags & PERIPH_ON_CBUS)
		return tegra12_clk_cbus_migrate_users(user);
	return -ENOSYS;
}

static void tegra_clk_shared_bus_user_init(struct clk *c)
{
	c->max_rate = c->parent->max_rate;
	c->u.shared_bus_user.rate = c->parent->max_rate;
	c->state = OFF;
	c->set = true;

	if ((c->u.shared_bus_user.mode == SHARED_CEILING) ||
	    (c->u.shared_bus_user.mode == SHARED_CEILING_BUT_ISO)) {
		c->state = ON;
		c->refcnt++;
	}

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

static int tegra_clk_shared_bus_user_set_parent(struct clk *c, struct clk *p)
{
	int ret;
	const struct clk_mux_sel *sel;

	if (detach_shared_bus)
		return 0;

	if (c->parent == p)
		return 0;

	if (!(c->inputs && c->cross_clk_mutex && clk_cansleep(c)))
		return -ENOSYS;

	for (sel = c->inputs; sel->input != NULL; sel++) {
		if (sel->input == p)
			break;
	}
	if (!sel->input)
		return -EINVAL;

	if (c->refcnt)
		clk_enable(p);

	list_move_tail(&c->u.shared_bus_user.node, &p->shared_bus_list);
	ret = tegra_clk_shared_bus_update(p);
	if (ret) {
		list_move_tail(&c->u.shared_bus_user.node,
			       &c->parent->shared_bus_list);
		tegra_clk_shared_bus_update(c->parent);
		clk_disable(p);
		return ret;
	}

	tegra_clk_shared_bus_update(c->parent);

	if (c->refcnt)
		clk_disable(c->parent);

	clk_reparent(c, p);

	return 0;
}

static int tegra_clk_shared_bus_user_set_rate(struct clk *c, unsigned long rate)
{
	int ret;

	c->u.shared_bus_user.rate = rate;
	ret = tegra_clk_shared_bus_update(c->parent);

	if (!ret && c->cross_clk_mutex && clk_cansleep(c))
		tegra_clk_shared_bus_migrate_users(c);

	return ret;
}

static long tegra_clk_shared_bus_user_round_rate(
	struct clk *c, unsigned long rate)
{
	/*
	 * Defer rounding requests until aggregated. BW users must not be
	 * rounded at all, others just clipped to bus range (some clients
	 * may use round api to find limits). Ignore SCLK/AHB and AHB/APB
	 * dividers to keep flat bus requests propagation.
	 */
	if ((c->u.shared_bus_user.mode != SHARED_BW) &&
	    (c->u.shared_bus_user.mode != SHARED_ISO_BW)) {
		if (!(c->flags & DIV_BUS) && (c->div > 1))
			rate *= c->div;

		if (rate > c->parent->max_rate)
			rate = c->parent->max_rate;
		else if (rate < c->parent->min_rate)
			rate = c->parent->min_rate;

		if (!(c->flags & DIV_BUS) && (c->div > 1))
			rate /= c->div;
	}
	return rate;
}

static int tegra_clk_shared_bus_user_enable(struct clk *c)
{
	int ret;

	c->u.shared_bus_user.enabled = true;
	ret = tegra_clk_shared_bus_update(c->parent);
	if (!ret && c->u.shared_bus_user.client)
		ret = clk_enable(c->u.shared_bus_user.client);

	if (!ret && c->cross_clk_mutex && clk_cansleep(c))
		tegra_clk_shared_bus_migrate_users(c);

	return ret;
}

static void tegra_clk_shared_bus_user_disable(struct clk *c)
{
	if (c->u.shared_bus_user.client)
		clk_disable(c->u.shared_bus_user.client);
	c->u.shared_bus_user.enabled = false;
	tegra_clk_shared_bus_update(c->parent);

	if (c->cross_clk_mutex && clk_cansleep(c))
		tegra_clk_shared_bus_migrate_users(c);
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
	.set_parent = tegra_clk_shared_bus_user_set_parent,
	.set_rate = tegra_clk_shared_bus_user_set_rate,
	.round_rate = tegra_clk_shared_bus_user_round_rate,
	.reset = tegra_clk_shared_bus_user_reset,
};

/* shared bus connector ops (user/bus connector to cascade shared buses) */
static int tegra12_clk_shared_connector_update(struct clk *bus)
{
	unsigned long rate, old_rate;

	if (detach_shared_bus)
		return 0;

	rate = tegra12_clk_shared_bus_update(bus, NULL, NULL, NULL);

	old_rate = clk_get_rate_locked(bus);
	if (rate == old_rate)
		return 0;

	return clk_set_rate_locked(bus, rate);
}

static struct clk_ops tegra_clk_shared_connector_ops = {
	.init = tegra_clk_shared_bus_user_init,
	.enable = tegra_clk_shared_bus_user_enable,
	.disable = tegra_clk_shared_bus_user_disable,
	.set_parent = tegra_clk_shared_bus_user_set_parent,
	.set_rate = tegra_clk_shared_bus_user_set_rate,
	.round_rate = tegra_clk_shared_bus_user_round_rate,
	.reset = tegra_clk_shared_bus_user_reset,
	.shared_bus_update = tegra12_clk_shared_connector_update,
};

/* coupled gate ops */
/*
 * Some clocks may have common enable/disable control, but run at different
 * rates, and have different dvfs tables. Coupled gate clock synchronize
 * enable/disable operations for such clocks.
 */

static int tegra12_clk_coupled_gate_enable(struct clk *c)
{
	int ret;
	const struct clk_mux_sel *sel;

	BUG_ON(!c->inputs);
	pr_debug("%s on clock %s\n", __func__, c->name);

	for (sel = c->inputs; sel->input != NULL; sel++) {
		if (sel->input == c->parent)
			continue;

		ret = clk_enable(sel->input);
		if (ret) {
			while (sel != c->inputs) {
				sel--;
				if (sel->input == c->parent)
					continue;
				clk_disable(sel->input);
			}
			return ret;
		}
	}

	return tegra12_periph_clk_enable(c);
}

static void tegra12_clk_coupled_gate_disable(struct clk *c)
{
	const struct clk_mux_sel *sel;

	BUG_ON(!c->inputs);
	pr_debug("%s on clock %s\n", __func__, c->name);

	tegra12_periph_clk_disable(c);

	if (!c->refcnt)	/* happens only on boot clean-up: don't propagate */
		return;

	for (sel = c->inputs; sel->input != NULL; sel++) {
		if (sel->input == c->parent)
			continue;

		if (sel->input->set)	/* enforce coupling after boot only */
			clk_disable(sel->input);
	}
}

static struct clk_ops tegra_clk_coupled_gate_ops = {
	.init			= tegra12_periph_clk_init,
	.enable			= tegra12_clk_coupled_gate_enable,
	.disable		= tegra12_clk_coupled_gate_disable,
	.reset			= &tegra12_periph_clk_reset,
};


/*
 * AHB and APB shared bus operations
 * APB shared bus is a user of AHB shared bus
 * AHB shared bus is a user of SCLK complex shared bus
 * SCLK/AHB and AHB/APB dividers can be dynamically changed. When AHB and APB
 * users requests are propagated to SBUS target rate, current values of the
 * dividers are ignored, and flat maximum request is selected as SCLK bus final
 * target. Then the dividers will be re-evaluated, based on AHB and APB targets.
 * Both AHB and APB buses are always enabled.
 */
static void tegra12_clk_ahb_apb_init(struct clk *c, struct clk *bus_clk)
{
	tegra_clk_shared_bus_user_init(c);
	c->max_rate = bus_clk->max_rate;
	c->min_rate = bus_clk->min_rate;
	c->mul = bus_clk->mul;
	c->div = bus_clk->div;

	c->u.shared_bus_user.rate = clk_get_rate(bus_clk);
	c->u.shared_bus_user.enabled = true;
	c->parent->child_bus = c;
}

static void tegra12_clk_ahb_init(struct clk *c)
{
	struct clk *bus_clk = c->parent->u.system.hclk;
	tegra12_clk_ahb_apb_init(c, bus_clk);
}

static void tegra12_clk_apb_init(struct clk *c)
{
	struct clk *bus_clk = c->parent->parent->u.system.pclk;
	tegra12_clk_ahb_apb_init(c, bus_clk);
}

static int tegra12_clk_ahb_apb_update(struct clk *bus)
{
	unsigned long rate;

	if (detach_shared_bus)
		return 0;

	rate = tegra12_clk_shared_bus_update(bus, NULL, NULL, NULL);
	return clk_set_rate_locked(bus, rate);
}

static struct clk_ops tegra_clk_ahb_ops = {
	.init = tegra12_clk_ahb_init,
	.set_rate = tegra_clk_shared_bus_user_set_rate,
	.round_rate = tegra_clk_shared_bus_user_round_rate,
	.shared_bus_update = tegra12_clk_ahb_apb_update,
};

static struct clk_ops tegra_clk_apb_ops = {
	.init = tegra12_clk_apb_init,
	.set_rate = tegra_clk_shared_bus_user_set_rate,
	.round_rate = tegra_clk_shared_bus_user_round_rate,
	.shared_bus_update = tegra12_clk_ahb_apb_update,
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
	{ 12000000, 600000000, 100, 1, 2},
	{ 13000000, 600000000,  92, 1, 2},	/* actual: 598.0 MHz */
	{ 16800000, 600000000,  71, 1, 2},	/* actual: 596.4 MHz */
	{ 19200000, 600000000,  62, 1, 2},	/* actual: 595.2 MHz */
	{ 26000000, 600000000,  92, 2, 2},	/* actual: 598.0 MHz */
	{ 0, 0, 0, 0, 0, 0 },
};

static struct clk tegra_pll_c = {
	.name      = "pll_c",
	.ops       = &tegra_pllxc_ops,
	.reg       = 0x80,
	.parent    = &tegra_pll_ref,
	.max_rate  = 1400000000,
	.u.pll = {
		.input_min = 12000000,
		.input_max = 800000000,
		.cf_min    = 12000000,
		.cf_max    = 19200000,	/* s/w policy, h/w capability 50 MHz */
		.vco_min   = 600000000,
		.vco_max   = 1400000000,
		.freq_table = tegra_pll_c_freq_table,
		.lock_delay = 300,
		.misc1 = 0x88 - 0x80,
		.round_p_to_pdiv = pllxc_round_p_to_pdiv,
	},
};

static struct clk tegra_pll_c_out1 = {
	.name      = "pll_c_out1",
	.ops       = &tegra_pll_div_ops,
#ifdef CONFIG_TEGRA_DUAL_CBUS
	.flags     = DIV_U71,
#else
	.flags     = DIV_U71 | PERIPH_ON_CBUS,
#endif
	.parent    = &tegra_pll_c,
	.reg       = 0x84,
	.reg_shift = 0,
	.max_rate  = 700000000,
};

static struct clk_pll_freq_table tegra_pll_cx_freq_table[] = {
	{ 12000000, 600000000, 100, 1, 2},
	{ 13000000, 600000000,  92, 1, 2},	/* actual: 598.0 MHz */
	{ 16800000, 600000000,  71, 1, 2},	/* actual: 596.4 MHz */
	{ 19200000, 600000000,  62, 1, 2},	/* actual: 595.2 MHz */
	{ 26000000, 600000000,  92, 2, 2},	/* actual: 598.0 MHz */
	{ 0, 0, 0, 0, 0, 0 },
};

static struct clk tegra_pll_c2 = {
	.name      = "pll_c2",
	.ops       = &tegra_pllcx_ops,
	.flags     = PLL_ALT_MISC_REG,
	.reg       = 0x4e8,
	.parent    = &tegra_pll_ref,
	.max_rate  = 1200000000,
	.u.pll = {
		.input_min = 12000000,
		.input_max = 48000000,
		.cf_min    = 12000000,
		.cf_max    = 19200000,
		.vco_min   = 650000000,
		.vco_max   = 1300000000,
		.freq_table = tegra_pll_cx_freq_table,
		.lock_delay = 360,
		.misc1 = 0x4f0 - 0x4e8,
		.round_p_to_pdiv = pllcx_round_p_to_pdiv,
	},
};

static struct clk tegra_pll_c3 = {
	.name      = "pll_c3",
	.ops       = &tegra_pllcx_ops,
	.flags     = PLL_ALT_MISC_REG,
	.reg       = 0x4fc,
	.parent    = &tegra_pll_ref,
	.max_rate  = 1200000000,
	.u.pll = {
		.input_min = 12000000,
		.input_max = 48000000,
		.cf_min    = 12000000,
		.cf_max    = 19200000,
		.vco_min   = 650000000,
		.vco_max   = 1300000000,
		.freq_table = tegra_pll_cx_freq_table,
		.lock_delay = 360,
		.misc1 = 0x504 - 0x4fc,
		.round_p_to_pdiv = pllcx_round_p_to_pdiv,
	},
};

static struct clk_pll_freq_table tegra_pll_m_freq_table[] = {
	{ 12000000, 800000000, 66, 1, 1},	/* actual: 792.0 MHz */
	{ 13000000, 800000000, 61, 1, 1},	/* actual: 793.0 MHz */
	{ 16800000, 800000000, 47, 1, 1},	/* actual: 789.6 MHz */
	{ 19200000, 800000000, 41, 1, 1},	/* actual: 787.2 MHz */
	{ 26000000, 800000000, 61, 2, 1},	/* actual: 793.0 MHz */
	{ 0, 0, 0, 0, 0, 0 },
};

static struct clk tegra_pll_m = {
	.name      = "pll_m",
	.flags     = PLLM,
	.ops       = &tegra_pllm_ops,
	.reg       = 0x90,
	.parent    = &tegra_pll_ref,
	.max_rate  = 1200000000,
	.u.pll = {
		.input_min = 12000000,
		.input_max = 500000000,
		.cf_min    = 12000000,
		.cf_max    = 19200000,	/* s/w policy, h/w capability 50 MHz */
		.vco_min   = 500000000,
		.vco_max   = 1200000000,
		.freq_table = tegra_pll_m_freq_table,
		.lock_delay = 300,
		.misc1 = 0x98 - 0x90,
		.round_p_to_pdiv = pllm_round_p_to_pdiv,
	},
};

static struct clk tegra_pll_m_out1 = {
	.name      = "pll_m_out1",
	.ops       = &tegra_pll_div_ops,
	.flags     = DIV_U71 | DIV_U71_INT,
	.parent    = &tegra_pll_m,
	.reg       = 0x94,
	.reg_shift = 0,
	.max_rate  = 1200000000,
};

static struct clk_pll_freq_table tegra_pll_p_freq_table[] = {
	{ 12000000, 408000000, 816, 12, 2, 8},
	{ 13000000, 408000000, 816, 13, 2, 8},
	{ 16800000, 408000000, 680, 14, 2, 8},
	{ 19200000, 408000000, 680, 16, 2, 8},
	{ 26000000, 408000000, 816, 26, 2, 8},
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
		.vco_min   = 200000000,
		.vco_max   = 700000000,
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
	.flags     = DIV_U71 | DIV_U71_FIXED | DIV_U71_INT,
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
	.flags     = DIV_U71 | DIV_U71_FIXED,
	.parent    = &tegra_pll_p,
	.reg       = 0xa8,
	.reg_shift = 16,
	.max_rate  = 432000000,
};

static struct clk tegra_pll_p_out5 = {
	.name      = "pll_p_out5",
	.ops       = &tegra_pll_div_ops,
	.flags     = DIV_U71 | DIV_U71_FIXED,
	.parent    = &tegra_pll_p,
	.reg       = 0x67c,
	.reg_shift = 16,
	.max_rate  = 432000000,
};

static struct clk_pll_freq_table tegra_pll_a_freq_table[] = {
	{  9600000, 282240000, 147,  5, 1, 4},
	{  9600000, 368640000, 192,  5, 1, 4},
	{  9600000, 240000000, 200,  8, 1, 8},

	{ 28800000, 282240000, 245, 25, 1, 8},
	{ 28800000, 368640000, 320, 25, 1, 8},
	{ 28800000, 240000000, 200, 24, 1, 8},
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
		.vco_min   = 200000000,
		.vco_max   = 700000000,
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
	{ 12000000, 216000000, 864, 12, 4, 12},
	{ 13000000, 216000000, 864, 13, 4, 12},
	{ 16800000, 216000000, 720, 14, 4, 12},
	{ 19200000, 216000000, 720, 16, 4, 12},
	{ 26000000, 216000000, 864, 26, 4, 12},

	{ 12000000, 594000000,  99,  2, 1,  8},
	{ 13000000, 594000000, 594, 13, 1, 12},
	{ 16800000, 594000000, 495, 14, 1, 12},
	{ 19200000, 594000000, 495, 16, 1, 12},
	{ 26000000, 594000000, 594, 26, 1, 12},

	{ 12000000, 1000000000, 1000, 12, 1, 12},
	{ 13000000, 1000000000, 1000, 13, 1, 12},
	{ 19200000, 1000000000, 625,  12, 1, 12},
	{ 26000000, 1000000000, 1000, 26, 1, 12},

	{ 0, 0, 0, 0, 0, 0 },
};

static struct clk tegra_pll_d = {
	.name      = "pll_d",
	.flags     = PLL_HAS_CPCON | PLLD,
	.ops       = &tegra_plld_ops,
	.reg       = 0xd0,
	.parent    = &tegra_pll_ref,
	.max_rate  = 1500000000,
	.u.pll = {
		.input_min = 2000000,
		.input_max = 40000000,
		.cf_min    = 1000000,
		.cf_max    = 6000000,
		.vco_min   = 500000000,
		.vco_max   = 1500000000,
		.freq_table = tegra_pll_d_freq_table,
		.lock_delay = 1000,
	},
};

static struct clk tegra_pll_d_out0 = {
	.name      = "pll_d_out0",
	.ops       = &tegra_pll_div_ops,
	.flags     = DIV_2 | PLLD,
	.parent    = &tegra_pll_d,
	.max_rate  = 750000000,
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

static struct clk tegra_pll_u_480M = {
	.name      = "pll_u_480M",
	.flags     = PLLU,
	.ops       = &tegra_pll_div_ops,
	.reg       = 0xc0,
	.reg_shift = 22,
	.parent    = &tegra_pll_u,
	.mul       = 1,
	.div       = 1,
	.max_rate  = 480000000,
};

static struct clk tegra_pll_u_60M = {
	.name      = "pll_u_60M",
	.flags     = PLLU,
	.ops       = &tegra_pll_div_ops,
	.reg       = 0xc0,
	.reg_shift = 23,
	.parent    = &tegra_pll_u,
	.mul       = 1,
	.div       = 8,
	.max_rate  = 60000000,
};

static struct clk tegra_pll_u_48M = {
	.name      = "pll_u_48M",
	.flags     = PLLU,
	.ops       = &tegra_pll_div_ops,
	.reg       = 0xc0,
	.reg_shift = 25,
	.parent    = &tegra_pll_u,
	.mul       = 1,
	.div       = 10,
	.max_rate  = 48000000,
};

static struct clk tegra_pll_u_12M = {
	.name      = "pll_u_12M",
	.flags     = PLLU,
	.ops       = &tegra_pll_div_ops,
	.reg       = 0xc0,
	.reg_shift = 21,
	.parent    = &tegra_pll_u,
	.mul       = 1,
	.div       = 40,
	.max_rate  = 12000000,
};

static struct clk_pll_freq_table tegra_pll_x_freq_table[] = {
	/* 1 GHz */
	{ 12000000, 1000000000, 83, 1, 1},	/* actual: 996.0 MHz */
	{ 13000000, 1000000000, 76, 1, 1},	/* actual: 988.0 MHz */
	{ 16800000, 1000000000, 59, 1, 1},	/* actual: 991.2 MHz */
	{ 19200000, 1000000000, 52, 1, 1},	/* actual: 998.4 MHz */
	{ 26000000, 1000000000, 76, 2, 1},	/* actual: 988.0 MHz */

	{ 0, 0, 0, 0, 0, 0 },
};

static struct clk tegra_pll_x = {
	.name      = "pll_x",
	.flags     = PLL_ALT_MISC_REG | PLLX,
	.ops       = &tegra_pllxc_ops,
	.reg       = 0xe0,
	.parent    = &tegra_pll_ref,
	.max_rate  = 3000000000UL,
	.u.pll = {
		.input_min = 12000000,
		.input_max = 800000000,
		.cf_min    = 12000000,
		.cf_max    = 19200000,	/* s/w policy, h/w capability 50 MHz */
		.vco_min   = 700000000,
		.vco_max   = 3000000000UL,
		.freq_table = tegra_pll_x_freq_table,
		.lock_delay = 300,
		.misc1 = 0x510 - 0xe0,
		.round_p_to_pdiv = pllxc_round_p_to_pdiv,
	},
};

static struct clk tegra_pll_x_out0 = {
	.name      = "pll_x_out0",
	.ops       = &tegra_pll_div_ops,
	.flags     = DIV_2 | PLLX,
	.parent    = &tegra_pll_x,
	.max_rate  = 1500000000UL,
};

static struct clk tegra_dfll_cpu = {
	.name      = "dfll_cpu",
	.flags     = DFLL,
	.ops       = &tegra_dfll_ops,
	.reg	   = 0x2f4,
	.max_rate  = 3000000000UL,
};

static struct clk_pll_freq_table tegra_pllc4_freq_table[] = {
	{ 12000000, 600000000, 100, 1, 2},
	{ 13000000, 600000000,  92, 1, 2},	/* actual: 598.0 MHz */
	{ 16800000, 600000000,  71, 1, 2},	/* actual: 596.4 MHz */
	{ 19200000, 600000000,  62, 1, 2},	/* actual: 595.2 MHz */
	{ 26000000, 600000000,  92, 2, 2},	/* actual: 598.0 MHz */
	{ 0, 0, 0, 0, 0, 0 },
};

static struct clk tegra_pll_c4 = {
	.name      = "pll_c4",
	.flags     = PLL_ALT_MISC_REG,
	.ops       = &tegra_pllss_ops,
	.reg       = 0x5a4,
	.parent    = &tegra_pll_ref,	/* s/w policy, always tegra_pll_ref */
	.max_rate  = 600000000,
	.u.pll = {
		.input_min = 12000000,
		.input_max = 1000000000,
		.cf_min    = 12000000,
		.cf_max    = 19200000,	/* s/w policy, h/w capability 38 MHz */
		.vco_min   = 600000000,
		.vco_max   = 1200000000,
		.freq_table = tegra_pllc4_freq_table,
		.lock_delay = 300,
		.misc1 = 0x8,
		.round_p_to_pdiv = pllss_round_p_to_pdiv,
	},
};

static struct clk_pll_freq_table tegra_plldp_freq_table[] = {
	{ 12000000, 270000000,  90, 1, 4},
	{ 13000000, 270000000,  83, 1, 4},	/* actual: 269.8 MHz */
	{ 16800000, 270000000,  96, 1, 6},	/* actual: 268.8 MHz */
	{ 19200000, 270000000,  84, 1, 6},	/* actual: 268.8 MHz */
	{ 26000000, 270000000,  83, 2, 4},	/* actual: 269.8 MHz */
	{ 0, 0, 0, 0, 0, 0 },
};

static struct clk tegra_pll_dp = {
	.name      = "pll_dp",
	.flags     = PLL_ALT_MISC_REG,
	.ops       = &tegra_pllss_ops,
	.reg       = 0x590,
	.parent    = &tegra_pll_ref,	/* s/w policy, always tegra_pll_ref */
	.max_rate  = 600000000,
	.u.pll = {
		.input_min = 12000000,
		.input_max = 1000000000,
		.cf_min    = 12000000,
		.cf_max    = 19200000,	/* s/w policy, h/w capability 38 MHz */
		.vco_min   = 600000000,
		.vco_max   = 1200000000,
		.freq_table = tegra_plldp_freq_table,
		.lock_delay = 300,
		.misc1 = 0x8,
		.round_p_to_pdiv = pllss_round_p_to_pdiv,
	},
};

static struct clk_pll_freq_table tegra_plld2_freq_table[] = {
	{ 12000000, 594000000,  99, 1, 2,  3}, /*4kx2k @ 30Hz*/
	{ 12000000, 297000000,  99, 1, 4,  1}, /*1080p @ 60Hz*/
	{ 12000000, 148500000,  99, 1, 8,  1}, /* 720p @ 60Hz*/
	{ 12000000,  54000000,  54, 1, 6,  1}, /* 480p @ 60Hz*/
	{ 13000000, 594000000,  91, 1, 2},	/* actual: 591.5 MHz */
	{ 16800000, 594000000,  71, 1, 2},	/* actual: 596.4 MHz */
	{ 19200000, 594000000,  62, 1, 2},	/* actual: 595.2 MHz */
	{ 26000000, 594000000,  91, 2, 2},	/* actual: 591.5 MHz */
	{ 0, 0, 0, 0, 0, 0 },
};

static struct clk tegra_pll_d2 = {
	.name      = "pll_d2",
	.flags     = PLL_ALT_MISC_REG,
	.ops       = &tegra_pllss_ops,
	.reg       = 0x4b8,
	.parent    = &tegra_pll_ref,	/* s/w policy, always tegra_pll_ref */
	.max_rate  = 600000000,
	.u.pll = {
		.input_min = 12000000,
		.input_max = 1000000000,
		.cf_min    = 12000000,
		.cf_max    = 19200000,	/* s/w policy, h/w capability 38 MHz */
		.vco_min   = 600000000,
		.vco_max   = 1200000000,
		.freq_table = tegra_plld2_freq_table,
		.lock_delay = 300,
		.misc1 = 0x570 - 0x4b8,
		.round_p_to_pdiv = pllss_round_p_to_pdiv,
		.cpcon_default = 0,
	},
};

static struct clk tegra_pll_re_vco = {
	.name      = "pll_re_vco",
	.flags     = PLL_ALT_MISC_REG,
	.ops       = &tegra_pllre_ops,
	.reg       = 0x4c4,
	.parent    = &tegra_pll_ref,
	.max_rate  = 672000000,
	.u.pll = {
		.input_min = 12000000,
		.input_max = 1000000000,
		.cf_min    = 12000000,
		.cf_max    = 19200000,	/* s/w policy, h/w capability 38 MHz */
		.vco_min   = 300000000,
		.vco_max   = 672000000,
		.lock_delay = 300,
		.round_p_to_pdiv = pllre_round_p_to_pdiv,
	},
};

static struct clk tegra_pll_re_out = {
	.name      = "pll_re_out",
	.ops       = &tegra_pllre_out_ops,
	.parent    = &tegra_pll_re_vco,
	.reg       = 0x4c4,
	.max_rate  = 672000000,
};

static struct clk_pll_freq_table tegra_pll_e_freq_table[] = {
	/* PLLE special case: use cpcon field to store cml divider value */
	{ 336000000, 100000000, 100, 21,  16, 11},
	{ 312000000, 100000000, 200, 26,  24, 13},
	{ 13000000,  100000000, 200, 1,  26, 13},
	{ 12000000,  100000000, 200, 1,  24, 13},
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
		.input_max = 1000000000,
		.cf_min    = 12000000,
		.cf_max    = 75000000,
		.vco_min   = 1600000000,
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
	.max_rate  = 500000000,
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

static struct clk_mux_sel mux_d_audio_clk[] = {
	{ .input = &tegra_pll_a_out0,		.value = 0},
	{ .input = &tegra_pll_p,		.value = 0x8000},
	{ .input = &tegra_clk_m,		.value = 0xc000},
	{ .input = &tegra_sync_source_list[0],	.value = 0xE000},
	{ .input = &tegra_sync_source_list[1],	.value = 0xE001},
	{ .input = &tegra_sync_source_list[2],	.value = 0xE002},
	{ .input = &tegra_sync_source_list[3],	.value = 0xE003},
	{ .input = &tegra_sync_source_list[4],	.value = 0xE004},
	{ .input = &tegra_sync_source_list[5],	.value = 0xE005},
	{ .input = &tegra_pll_a_out0,		.value = 0xE006},
	{ .input = &tegra_sync_source_list[6],	.value = 0xE007},
	{ 0, 0 }
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

#define AUDIO_SYNC_CLK(_id, _dev, _index)			\
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
	AUDIO_SYNC_CLK(audio, tegra30-spdif, 5),	/* SPDIF */
};

#define AUDIO_SYNC_2X_CLK(_id, _dev, _index)				\
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
	AUDIO_SYNC_2X_CLK(audio, tegra30-spdif, 5),	/* SPDIF */
};

#define MUX_I2S_SPDIF(_id, _index)					\
static struct clk_mux_sel mux_pllaout0_##_id##_2x_pllp_clkm[] = {	\
	{.input = &tegra_pll_a_out0, .value = 0},			\
	{.input = &tegra_clk_audio_2x_list[(_index)], .value = 2},	\
	{.input = &tegra_pll_p, .value = 4},				\
	{.input = &tegra_clk_m, .value = 6},				\
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

#define CLK_OUT_CLK(_id, _max_rate)					\
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
		.max_rate  = _max_rate,				\
		.u.periph = {					\
			.clk_num   = (_id - 1) * 8 + 2,		\
		},						\
	}
static struct clk tegra_clk_out_list[] = {
	CLK_OUT_CLK(1, 26000000),
	CLK_OUT_CLK(2, 40800000),
	CLK_OUT_CLK(3, 26000000),
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
	/* { .input = &tegra_pll_c2,	.value = 6}, - no use on tegra12x */
	/* { .input = &tegra_clk_c3,	.value = 7}, - no use on tegra12x */
	{ .input = &tegra_pll_x,	.value = 8},
	{ .input = &tegra_dfll_cpu,	.value = 15},
	{ 0, 0},
};

static struct clk_mux_sel mux_cclk_lp[] = {
	{ .input = &tegra_clk_m,	.value = 0},
	{ .input = &tegra_pll_c,	.value = 1},
	{ .input = &tegra_clk_32k,	.value = 2},
	{ .input = &tegra_pll_m,	.value = 3},
	{ .input = &tegra_pll_p,	.value = 4},
	{ .input = &tegra_pll_p_out4,	.value = 5},
	/* { .input = &tegra_pll_c2,	.value = 6}, - no use on tegra12x */
	/* { .input = &tegra_clk_c3,	.value = 7}, - no use on tegra12x */
	{ .input = &tegra_pll_x_out0,	.value = 8},
	{ .input = &tegra_pll_x,	.value = 8 | SUPER_LP_DIV2_BYPASS},
	{ 0, 0},
};

static struct clk_mux_sel mux_sclk[] = {
	{ .input = &tegra_clk_m,	.value = 0},
	{ .input = &tegra_pll_c_out1,	.value = 1},
	{ .input = &tegra_pll_p_out4,	.value = 2},
	{ .input = &tegra_pll_p,	.value = 3},
	{ .input = &tegra_pll_p_out2,	.value = 4},
	{ .input = &tegra_pll_c,	.value = 5},
	{ .input = &tegra_clk_32k,	.value = 6},
	{ .input = &tegra_pll_m_out1,	.value = 7},
	{ 0, 0},
};

static struct clk tegra_clk_cclk_g = {
	.name	= "cclk_g",
	.flags  = DIV_U71 | DIV_U71_INT | MUX,
	.inputs	= mux_cclk_g,
	.reg	= 0x368,
	.ops	= &tegra_super_ops,
	.max_rate = 3000000000UL,
};

static struct clk tegra_clk_cclk_lp = {
	.name	= "cclk_lp",
	.flags  = DIV_2 | DIV_U71 | DIV_U71_INT | MUX,
	.inputs	= mux_cclk_lp,
	.reg	= 0x370,
	.ops	= &tegra_super_ops,
	.max_rate = 1350000000,
};

static struct clk tegra_clk_sclk = {
	.name	= "sclk",
	.inputs	= mux_sclk,
	.reg	= 0x28,
	.ops	= &tegra_super_ops,
	.max_rate = 408000000,
	.min_rate = 12000000,
};

static struct raw_notifier_head cpu_g_rate_change_nh;

static struct clk tegra_clk_virtual_cpu_g = {
	.name      = "cpu_g",
	.parent    = &tegra_clk_cclk_g,
	.ops       = &tegra_cpu_ops,
	.max_rate  = 3000000000UL,
	.min_rate  = 3187500,
	.u.cpu = {
		.main      = &tegra_pll_x,
		.backup    = &tegra_pll_p_out4,
		.dynamic   = &tegra_dfll_cpu,
		.mode      = MODE_G,
	},
	.rate_change_nh = &cpu_g_rate_change_nh,
};

static struct clk tegra_clk_virtual_cpu_lp = {
	.name      = "cpu_lp",
	.parent    = &tegra_clk_cclk_lp,
	.ops       = &tegra_cpu_ops,
	.max_rate  = 1350000000,
	.min_rate  = 3187500,
	.u.cpu = {
		.main      = &tegra_pll_x,
		.backup    = &tegra_pll_p_out4,
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
	.max_rate  = 3000000000UL,
};

static struct clk tegra_clk_cop = {
	.name      = "cop",
	.parent    = &tegra_clk_sclk,
	.ops       = &tegra_cop_ops,
	.max_rate  = 408000000,
};

static struct clk tegra_clk_hclk = {
	.name		= "hclk",
	.flags		= DIV_BUS,
	.parent		= &tegra_clk_sclk,
	.reg		= 0x30,
	.reg_shift	= 4,
	.ops		= &tegra_bus_ops,
	.max_rate       = 408000000,
	.min_rate       = 12000000,
};

static struct clk tegra_clk_pclk = {
	.name		= "pclk",
	.flags		= DIV_BUS,
	.parent		= &tegra_clk_hclk,
	.reg		= 0x30,
	.reg_shift	= 0,
	.ops		= &tegra_bus_ops,
	.max_rate       = 204000000,
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
		.sclk_low = &tegra_pll_p_out2,
#ifdef CONFIG_TEGRA_PLLM_SCALED
		.sclk_high = &tegra_pll_c_out1,
#else
		.sclk_high = &tegra_pll_m_out1,
#endif
	},
	.rate_change_nh = &sbus_rate_change_nh,
};

static struct clk tegra_clk_ahb = {
	.name	   = "ahb.sclk",
	.flags	   = DIV_BUS,
	.parent    = &tegra_clk_sbus_cmplx,
	.ops       = &tegra_clk_ahb_ops,
};

static struct clk tegra_clk_apb = {
	.name	   = "apb.sclk",
	.flags	   = DIV_BUS,
	.parent    = &tegra_clk_ahb,
	.ops       = &tegra_clk_apb_ops,
};

static struct clk tegra_clk_blink = {
	.name		= "blink",
	.parent		= &tegra_clk_32k,
	.reg		= 0x40,
	.ops		= &tegra_blink_clk_ops,
	.max_rate	= 32768,
};


/* Multimedia modules muxes */
static struct clk_mux_sel mux_pllm_pllc2_c_c3_pllp_plla[] = {
	{ .input = &tegra_pll_m,  .value = 0},
	{ .input = &tegra_pll_c2, .value = 1},
	{ .input = &tegra_pll_c,  .value = 2},
	{ .input = &tegra_pll_c3, .value = 3},
	{ .input = &tegra_pll_p,  .value = 4},
	{ .input = &tegra_pll_a_out0, .value = 6},
	{ 0, 0},
};

static struct clk_mux_sel mux_pllm_pllc_pllp_plla[] = {
	{ .input = &tegra_pll_m, .value = 0},
	{ .input = &tegra_pll_c, .value = 2},
	{ .input = &tegra_pll_p, .value = 4},
	{ .input = &tegra_pll_a_out0, .value = 6},
	{ 0, 0},
};

static struct clk_mux_sel mux_pllm_pllc_pllp_plla_v2[] = {
	{ .input = &tegra_pll_m, .value = 0},
	{ .input = &tegra_pll_c, .value = 1},
	{ .input = &tegra_pll_p, .value = 2},
	{ .input = &tegra_pll_a_out0, .value = 3},
	/* Skip C2(4) */
	/* Skip C2(5) */
	{ 0, 0},
};

static struct clk_mux_sel mux_pllm_pllc_pllp_plla_pllc2_c3_clkm[] = {
	{ .input = &tegra_pll_m, .value = 0},
	{ .input = &tegra_pll_c, .value = 1},
	{ .input = &tegra_pll_p, .value = 2},
	{ .input = &tegra_pll_a_out0, .value = 3},
	{ .input = &tegra_pll_c2, .value = 4},
	{ .input = &tegra_pll_c3, .value = 5},
	{ .input = &tegra_clk_m, .value = 6},
	{ 0, 0},
};

static struct clk_mux_sel mux_pllm_pllc_pllp_plla_pllc4[] = {
	{ .input = &tegra_pll_m, .value = 0},
	/* Skip C2(1) */
	{ .input = &tegra_pll_c, .value = 2},
	/* Skip C2(3) */
	{ .input = &tegra_pll_p, .value = 4},
	{ .input = &tegra_pll_a_out0, .value = 6},
	{ .input = &tegra_pll_c4, .value = 7},
	{ 0, 0},
};

static struct clk_mux_sel mux_pllm_pllc_pllp_plla_clkm_pllc4[] = {
	{ .input = &tegra_pll_m, .value = 0},
	{ .input = &tegra_pll_c, .value = 1},
	{ .input = &tegra_pll_p, .value = 2},
	{ .input = &tegra_pll_a_out0, .value = 3},
	/* Skip C2(4) & C3(5) */
	{ .input = &tegra_clk_m, .value = 6},
	{ .input = &tegra_pll_c4, .value = 7},
	{ 0, 0},
};

static struct clk_mux_sel mux_plla_pllc_pllp_clkm[] = {
	{ .input = &tegra_pll_a_out0, .value = 0},
	{ .input = &tegra_pll_c, .value = 2},
	{ .input = &tegra_pll_p, .value = 4},
	{ .input = &tegra_clk_m, .value = 6},
	{ 0, 0},
};

/* EMC muxes */
/* FIXME: add EMC latency mux */
static struct clk_mux_sel mux_pllm_pllc_pllp_clkm[] = {
	{ .input = &tegra_pll_m, .value = 0},
	{ .input = &tegra_pll_c, .value = 1},
	{ .input = &tegra_pll_p, .value = 2},
	{ .input = &tegra_clk_m, .value = 3},
	{ .input = &tegra_pll_m, .value = 4}, /* low jitter PLLM output */
	/* { .input = &tegra_pll_c2, .value = 5}, - no use on tegra12x */
	/* { .input = &tegra_pll_c3, .value = 6}, - no use on tegra12x */
	{ .input = &tegra_pll_c, .value = 7}, /* low jitter PLLC output */
	{ 0, 0},
};


/* Display subsystem muxes */
static struct clk_mux_sel mux_pllp_pllm_plld_plla_pllc_plld2_clkm[] = {
	{.input = &tegra_pll_p, .value = 0},
	{.input = &tegra_pll_m, .value = 1},
	{.input = &tegra_pll_d_out0, .value = 2},
	{.input = &tegra_pll_a_out0, .value = 3},
	{.input = &tegra_pll_c, .value = 4},
	{.input = &tegra_pll_d2, .value = 5},
	{.input = &tegra_clk_m, .value = 6},
	{ 0, 0},
};

static struct clk_mux_sel mux_plld_out0[] = {
	{ .input = &tegra_pll_d_out0,  .value = 0},
	{ 0, 0},
};

/* Peripheral muxes */
static struct clk_mux_sel mux_pllp_pllc_clkm[] = {
	{.input = &tegra_pll_p,     .value = 0},
	{.input = &tegra_pll_c,     .value = 2},
	{.input = &tegra_clk_m,     .value = 6},
	{ 0, 0},
};

static struct clk_mux_sel mux_pllp_pllc_clkm1[] = {
	{.input = &tegra_pll_p,     .value = 0},
	{.input = &tegra_pll_c,     .value = 1},
	{.input = &tegra_clk_m,     .value = 3},
	{ 0, 0},
};

static struct clk_mux_sel mux_pllp_pllc2_c_c3_pllm_clkm[] = {
	{ .input = &tegra_pll_p,  .value = 0},
	{ .input = &tegra_pll_c2, .value = 1},
	{ .input = &tegra_pll_c,  .value = 2},
	{ .input = &tegra_pll_c3, .value = 3},
	{ .input = &tegra_pll_m,  .value = 4},
	{ .input = &tegra_clk_m,  .value = 6},
	{ 0, 0},
};

static struct clk_mux_sel mux_pllp_pllc_pllm_clkm[] = {
	{ .input = &tegra_pll_p, .value = 0},
	{ .input = &tegra_pll_c, .value = 2},
	{ .input = &tegra_pll_m, .value = 4},
	{ .input = &tegra_clk_m, .value = 6},
	{ 0, 0},
};

static struct clk_mux_sel mux_pllp_pllc_pllm[] = {
	{.input = &tegra_pll_p,     .value = 0},
	{.input = &tegra_pll_c,     .value = 2},
	{.input = &tegra_pll_m,     .value = 4},
	{ 0, 0},
};

static struct clk_mux_sel mux_pllp_clkm_clk32_plle[] = {
	{ .input = &tegra_pll_p,    .value = 0},
	{ .input = &tegra_clk_m,    .value = 1},
	{ .input = &tegra_clk_32k,  .value = 2},
	{ .input = &tegra_pll_e,    .value = 3},
	{ 0, 0},
};

static struct clk_mux_sel mux_pllp_clkm[] = {
	{ .input = &tegra_pll_p, .value = 0},
	{ .input = &tegra_clk_m, .value = 6},
	{ 0, 0},
};

static struct clk_mux_sel mux_pllp_pllc_clk32_clkm[] = {
	{.input = &tegra_pll_p,     .value = 0},
	{.input = &tegra_pll_c,     .value = 2},
	{.input = &tegra_clk_32k,   .value = 4},
	{.input = &tegra_clk_m,     .value = 6},
	{ 0, 0},
};

static struct clk_mux_sel mux_pllp_pllc_clkm_clk32[] = {
	{.input = &tegra_pll_p,     .value = 0},
	{.input = &tegra_pll_c,     .value = 2},
	{.input = &tegra_clk_m,     .value = 4},
	{.input = &tegra_clk_32k,   .value = 6},
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

static struct clk_mux_sel mux_clkm_pllp_pllc_pllre[] = {
	{ .input = &tegra_clk_m,  .value = 0},
	{ .input = &tegra_pll_p,  .value = 1},
	{ .input = &tegra_pll_c,  .value = 3},
	{ .input = &tegra_pll_re_out,  .value = 5},
	{ 0, 0},
};

static struct clk_mux_sel mux_clkm_48M_pllp_480M[] = {
	{ .input = &tegra_clk_m,      .value = 0},
	{ .input = &tegra_pll_u_48M,  .value = 2},
	{ .input = &tegra_pll_p,      .value = 4},
	{ .input = &tegra_pll_u_480M, .value = 6},
	{ 0, 0},
};

static struct clk_mux_sel mux_clkm_pllre_clk32_480M_pllc_ref[] = {
	{ .input = &tegra_clk_m,      .value = 0},
	{ .input = &tegra_pll_re_out, .value = 1},
	{ .input = &tegra_clk_32k,    .value = 2},
	{ .input = &tegra_pll_u_480M, .value = 3},
	{ .input = &tegra_pll_c,      .value = 4},
	{ .input = &tegra_pll_ref,    .value = 7},
	{ 0, 0},
};

static struct clk_mux_sel mux_pllp3_pllc_clkm[] = {
	{ .input = &tegra_pll_p_out3, .value = 0},
	{ .input = &tegra_pll_c,  .value = 1},
	{ .input = &tegra_clk_m,  .value = 3},
	{ 0, 0},
};

/* Single clock source ("fake") muxes */
static struct clk_mux_sel mux_clk_m[] = {
	{ .input = &tegra_clk_m, .value = 0},
	{ 0, 0},
};

static struct clk_mux_sel mux_pllp_out3[] = {
	{ .input = &tegra_pll_p_out3, .value = 0},
	{ 0, 0},
};

static struct clk_mux_sel mux_clk_32k[] = {
	{ .input = &tegra_clk_32k, .value = 0},
	{ 0, 0},
};

static struct clk_mux_sel mux_plld[] = {
	{ .input = &tegra_pll_d_out0, .value = 1},
	{ 0, 0},
};


static struct raw_notifier_head emc_rate_change_nh;

static struct clk tegra_clk_emc = {
	.name = "emc",
	.ops = &tegra_emc_clk_ops,
	.reg = 0x19c,
	.max_rate = 1200000000,
	.min_rate = 12750000,
	.inputs = mux_pllm_pllc_pllp_clkm,
	.flags = MUX | DIV_U71 | PERIPH_EMC_ENB,
	.u.periph = {
		.clk_num = 57,
	},
	.rate_change_nh = &emc_rate_change_nh,
};

static struct clk tegra_clk_mc = {
	.name = "mc",
	.ops = &tegra_mc_clk_ops,
	.max_rate =  533000000,
	.parent = &tegra_clk_emc,
	.flags = PERIPH_NO_RESET,
	.u.periph = {
		.clk_num = 32,
	},
};

static struct raw_notifier_head host1x_rate_change_nh;

static struct clk tegra_clk_host1x = {
	.name      = "host1x",
	.lookup    = {
		.dev_id = "host1x",
	},
	.ops       = &tegra_1xbus_clk_ops,
	.reg       = 0x180,
	.inputs    = mux_pllm_pllc_pllp_plla,
	.flags     = MUX | DIV_U71 | DIV_U71_INT,
	.max_rate  = 500000000,
	.min_rate  = 12000000,
	.u.periph = {
		.clk_num   = 28,
		.pll_low = &tegra_pll_p,
#ifdef CONFIG_TEGRA_PLLM_SCALED
		.pll_high = &tegra_pll_c,
#else
		.pll_high = &tegra_pll_m,
#endif
	},
	.rate_change_nh = &host1x_rate_change_nh,
};

static struct raw_notifier_head mselect_rate_change_nh;

static struct clk tegra_clk_mselect = {
	.name      = "mselect",
	.lookup    = {
		.dev_id = "mselect",
	},
	.ops       = &tegra_1xbus_clk_ops,
	.reg       = 0x3b4,
	.inputs    = mux_pllp_clkm,
	.flags     = MUX | DIV_U71 | DIV_U71_INT,
	.max_rate  = 408000000,
	.min_rate  = 12000000,
	.u.periph = {
		.clk_num   = 99,
		.pll_low = &tegra_pll_p,
		.pll_high = &tegra_pll_p,
		.threshold = 408000000,
	},
	.rate_change_nh = &mselect_rate_change_nh,
};

#ifdef CONFIG_TEGRA_DUAL_CBUS

static struct raw_notifier_head c2bus_rate_change_nh;
static struct raw_notifier_head c3bus_rate_change_nh;

static struct clk tegra_clk_c2bus = {
	.name      = "c2bus",
	.parent    = &tegra_pll_c2,
	.ops       = &tegra_clk_cbus_ops,
	.max_rate  = 700000000,
	.mul       = 1,
	.div       = 1,
	.flags     = PERIPH_ON_CBUS,
	.shared_bus_backup = {
		.input = &tegra_pll_p,
	},
	.rate_change_nh = &c2bus_rate_change_nh,
};
static struct clk tegra_clk_c3bus = {
	.name      = "c3bus",
	.parent    = &tegra_pll_c3,
	.ops       = &tegra_clk_cbus_ops,
	.max_rate  = 900000000,
	.mul       = 1,
	.div       = 1,
	.flags     = PERIPH_ON_CBUS,
	.shared_bus_backup = {
		.input = &tegra_pll_p,
	},
	.rate_change_nh = &c3bus_rate_change_nh,
};

#ifdef CONFIG_TEGRA_MIGRATE_CBUS_USERS
static DEFINE_MUTEX(cbus_mutex);
#define CROSS_CBUS_MUTEX (&cbus_mutex)
#else
#define CROSS_CBUS_MUTEX NULL
#endif


static struct clk_mux_sel mux_clk_cbus[] = {
	{ .input = &tegra_clk_c2bus, .value = 0},
	{ .input = &tegra_clk_c3bus, .value = 1},
	{ 0, 0},
};

#define DUAL_CBUS_CLK(_name, _dev, _con, _parent, _id, _div, _mode)\
	{						\
		.name      = _name,			\
		.lookup    = {				\
			.dev_id    = _dev,		\
			.con_id    = _con,		\
		},					\
		.ops = &tegra_clk_shared_bus_user_ops,	\
		.parent = _parent,			\
		.inputs = mux_clk_cbus,			\
		.flags = MUX,				\
		.u.shared_bus_user = {			\
			.client_id = _id,		\
			.client_div = _div,		\
			.mode = _mode,			\
		},					\
		.cross_clk_mutex = CROSS_CBUS_MUTEX,	\
	}

#else

static struct raw_notifier_head cbus_rate_change_nh;

static struct clk tegra_clk_cbus = {
	.name	   = "cbus",
	.parent    = &tegra_pll_c,
	.ops       = &tegra_clk_cbus_ops,
	.max_rate  = 700000000,
	.mul	   = 1,
	.div	   = 2,
	.flags     = PERIPH_ON_CBUS,
	.shared_bus_backup = {
		.input = &tegra_pll_p,
	},
	.rate_change_nh = &cbus_rate_change_nh,
};
#endif

static struct clk_ops tegra_clk_gpu_ops = {
	.enable		= &tegra12_periph_clk_enable,
	.disable	= &tegra12_periph_clk_disable,
	.reset		= &tegra12_periph_clk_reset,
};

/* This is a dummy clock for gpu. The enable/disable/reset routine controls
   input clock of the actual gpu clock. The input clock itself has a fixed
   frequency. The actual gpu clock's frequency is controlled by gpu driver,
   not here in clock framework. However, we assoicate this dummy clock with
   dvfs to control voltage of gpu rail along with frequency change of actual
   gpu clock. So frequency here and in dvfs are based on the acutal gpu clock. */
static struct clk tegra_clk_gpu = {
	.name      = "gpu_ref",
	.ops       = &tegra_clk_gpu_ops,
	.parent    = &tegra_pll_ref,
	.u.periph  = {
		.clk_num = 184,
	},
	.max_rate  = 48000000,
	.min_rate  = 12000000,
};

#define RATE_GRANULARITY	100000 /* 0.1 MHz */
#if defined(CONFIG_TEGRA_CLOCK_DEBUG_FUNC)
static int gbus_round_pass_thru;
void tegra_gbus_round_pass_thru_enable(bool enable)
{
	if (enable)
		gbus_round_pass_thru = 1;
	else
		gbus_round_pass_thru = 0;
}
EXPORT_SYMBOL(tegra_gbus_round_pass_thru_enable);
#else
#define gbus_round_pass_thru	0
#endif

static void tegra12_clk_gbus_init(struct clk *c)
{
	unsigned long rate;
	bool enabled;

	pr_debug("%s on clock %s (export ops %s)\n", __func__,
		 c->name, c->u.export_clk.ops ? "ready" : "not ready");

	if (!c->u.export_clk.ops || !c->u.export_clk.ops->init)
		return;

	c->u.export_clk.ops->init(c->u.export_clk.ops->data, &rate, &enabled);
	c->div = clk_get_rate(c->parent) / RATE_GRANULARITY;
	c->mul = rate / RATE_GRANULARITY;
	c->state = enabled ? ON : OFF;
}

static int tegra12_clk_gbus_enable(struct clk *c)
{
	pr_debug("%s on clock %s (export ops %s)\n", __func__,
		 c->name, c->u.export_clk.ops ? "ready" : "not ready");

	if (!c->u.export_clk.ops || !c->u.export_clk.ops->enable)
		return -ENOENT;

	return c->u.export_clk.ops->enable(c->u.export_clk.ops->data);
}

static void tegra12_clk_gbus_disable(struct clk *c)
{
	pr_debug("%s on clock %s (export ops %s)\n", __func__,
		 c->name, c->u.export_clk.ops ? "ready" : "not ready");

	if (!c->u.export_clk.ops || !c->u.export_clk.ops->disable)
		return;

	c->u.export_clk.ops->disable(c->u.export_clk.ops->data);
}

static int tegra12_clk_gbus_set_rate(struct clk *c, unsigned long rate)
{
	int ret;

	pr_debug("%s %lu on clock %s (export ops %s)\n", __func__,
		 rate, c->name, c->u.export_clk.ops ? "ready" : "not ready");

	if (!c->u.export_clk.ops || !c->u.export_clk.ops->set_rate)
		return -ENOENT;

	ret = c->u.export_clk.ops->set_rate(c->u.export_clk.ops->data, &rate);
	if (!ret)
		c->mul = rate / RATE_GRANULARITY;
	return ret;
}

static long tegra12_clk_gbus_round_updown(struct clk *c, unsigned long rate,
					  bool up)
{
	return gbus_round_pass_thru ? rate :
		tegra12_clk_cbus_round_updown(c, rate, up);
}

static long tegra12_clk_gbus_round_rate(struct clk *c, unsigned long rate)
{
	return tegra12_clk_gbus_round_updown(c, rate, true);
}

static struct clk_ops tegra_clk_gbus_ops = {
	.init		= tegra12_clk_gbus_init,
	.enable		= tegra12_clk_gbus_enable,
	.disable	= tegra12_clk_gbus_disable,
	.set_rate	= tegra12_clk_gbus_set_rate,
	.round_rate	= tegra12_clk_gbus_round_rate,
	.round_rate_updown = tegra12_clk_gbus_round_updown,
	.shared_bus_update = tegra12_clk_shared_connector_update, /* re-use */
};

static struct raw_notifier_head gbus_rate_change_nh;

static struct clk tegra_clk_gbus = {
	.name      = "gbus",
	.ops       = &tegra_clk_gbus_ops,
	.parent    = &tegra_clk_gpu,
	.max_rate  = 1032000000,
	.shared_bus_flags = SHARED_BUS_RETENTION,
	.rate_change_nh = &gbus_rate_change_nh,
};

static void tegra12_camera_mclk_init(struct clk *c)
{
	c->state = OFF;
	c->set = true;

	if (!strcmp(c->name, "mclk")) {
		c->parent = tegra_get_clock_by_name("vi_sensor");
		c->max_rate = c->parent->max_rate;
	} else if (!strcmp(c->name, "mclk2")) {
		c->parent = tegra_get_clock_by_name("vi_sensor2");
		c->max_rate = c->parent->max_rate;
	}
}

static int tegra12_camera_mclk_set_rate(struct clk *c, unsigned long rate)
{
	return clk_set_rate(c->parent, rate);
}

static struct clk_ops tegra_camera_mclk_ops = {
	.init     = tegra12_camera_mclk_init,
	.enable   = tegra12_periph_clk_enable,
	.disable  = tegra12_periph_clk_disable,
	.set_rate = tegra12_camera_mclk_set_rate,
};

static struct clk tegra_camera_mclk = {
	.name = "mclk",
	.ops = &tegra_camera_mclk_ops,
	.u.periph = {
		.clk_num = 92, /* csus */
	},
	.flags = PERIPH_NO_RESET,
};

static struct clk tegra_camera_mclk2 = {
	.name = "mclk2",
	.ops = &tegra_camera_mclk_ops,
	.u.periph = {
		.clk_num = 171, /* vim2_clk */
	},
	.flags = PERIPH_NO_RESET,
};

static struct clk tegra_clk_isp = {
	.name = "isp",
	.ops = &tegra_periph_clk_ops,
	.reg = 0x144,
	.max_rate = 700000000,
	.inputs = mux_pllm_pllc_pllp_plla_clkm_pllc4,
	.flags = MUX | DIV_U71 | PERIPH_NO_ENB | PERIPH_NO_RESET,
};

static struct clk_mux_sel mux_isp[] = {
	{ .input = &tegra_clk_isp, .value = 0},
	{ 0, 0},
};

static struct raw_notifier_head c4bus_rate_change_nh;

static struct clk tegra_clk_c4bus = {
	.name	   = "c4bus",
	.parent    = &tegra_pll_c4,
	.ops       = &tegra_clk_cbus_ops,
	.max_rate  = 700000000,
	.mul	   = 1,
	.div	   = 1,
	.shared_bus_backup = {
		.input = &tegra_pll_p,
	},
	.rate_change_nh = &c4bus_rate_change_nh,
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

#define D_AUDIO_CLK(_name, _dev, _con, _clk_num, _reg, _max, _inputs, _flags) \
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
			.src_mask  = 0xE01F << 16,	\
			.src_shift = 16,		\
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
#define SHARED_LIMIT(_name, _dev, _con, _parent, _id, _div, _mode)\
	{						\
		.name      = _name,			\
		.lookup    = {				\
			.dev_id    = _dev,		\
			.con_id    = _con,		\
		},					\
		.ops = &tegra_clk_shared_bus_user_ops,	\
		.parent = _parent,			\
		.flags     = BUS_RATE_LIMIT,		\
		.u.shared_bus_user = {			\
			.client_id = _id,		\
			.client_div = _div,		\
			.mode = _mode,			\
		},					\
	}
#define SHARED_CONNECT(_name, _dev, _con, _parent, _id, _div, _mode)\
	{						\
		.name      = _name,			\
		.lookup    = {				\
			.dev_id    = _dev,		\
			.con_id    = _con,		\
		},					\
		.ops = &tegra_clk_shared_connector_ops,	\
		.parent = _parent,			\
		.u.shared_bus_user = {			\
			.client_id = _id,		\
			.client_div = _div,		\
			.mode = _mode,			\
		},					\
	}

#define SHARED_EMC_CLK(_name, _dev, _con, _parent, _id, _div, _mode, _flag)\
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
			.usage_flag = _flag,		\
		},					\
	}

static DEFINE_MUTEX(sbus_cross_mutex);
#define SHARED_SCLK(_name, _dev, _con, _parent, _id, _div, _mode)\
	{						\
		.name = _name,				\
		.lookup = {				\
			.dev_id = _dev,			\
			.con_id = _con,			\
		},					\
		.ops = &tegra_clk_shared_bus_user_ops,	\
		.parent = _parent,			\
		.u.shared_bus_user = {			\
			.client_id = _id,		\
			.client_div = _div,		\
			.mode = _mode,			\
		},					\
		.cross_clk_mutex = &sbus_cross_mutex,	\
}

struct clk tegra_list_clks[] = {
	PERIPH_CLK("apbdma",	"tegra-apbdma",		NULL,	34,	0,	26000000,  mux_clk_m,			0),
	PERIPH_CLK("rtc",	"rtc-tegra",		NULL,	4,	0,	32768,     mux_clk_32k,			PERIPH_NO_RESET | PERIPH_ON_APB),
	PERIPH_CLK("kbc",	"tegra-kbc",		NULL,	36,	0,	32768,	   mux_clk_32k, 		PERIPH_NO_RESET | PERIPH_ON_APB),
	PERIPH_CLK("timer",	"timer",		NULL,	5,	0,	26000000,  mux_clk_m,			0),
	PERIPH_CLK("kfuse",	"kfuse-tegra",		NULL,	40,	0,	26000000,  mux_clk_m,			PERIPH_ON_APB),
	PERIPH_CLK("fuse",	"fuse-tegra",		"fuse",	39,	0,	26000000,  mux_clk_m,			PERIPH_ON_APB),
	PERIPH_CLK("fuse_burn",	"fuse-tegra",		"fuse_burn",	39,	0,	26000000,  mux_clk_m,		PERIPH_ON_APB),
	PERIPH_CLK("apbif",	"tegra30-ahub",		"apbif", 107,	0,	26000000,  mux_clk_m,			PERIPH_ON_APB),
	PERIPH_CLK("i2s0",	"tegra30-i2s.0",	NULL,	30,	0x1d8,	24576000,  mux_pllaout0_audio0_2x_pllp_clkm,	MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("i2s1",	"tegra30-i2s.1",	NULL,	11,	0x100,	24576000,  mux_pllaout0_audio1_2x_pllp_clkm,	MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("i2s2",	"tegra30-i2s.2",	NULL,	18,	0x104,	24576000,  mux_pllaout0_audio2_2x_pllp_clkm,	MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("i2s3",	"tegra30-i2s.3",	NULL,	101,	0x3bc,	24576000,  mux_pllaout0_audio3_2x_pllp_clkm,	MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("i2s4",	"tegra30-i2s.4",	NULL,	102,	0x3c0,	24576000,  mux_pllaout0_audio4_2x_pllp_clkm,	MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("spdif_out",	"tegra30-spdif",	"spdif_out",	10,	0x108,	 24576000, mux_pllaout0_audio_2x_pllp_clkm,	MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("spdif_in",	"tegra30-spdif",	"spdif_in",	10,	0x10c,	100000000, mux_pllp_pllc_pllm,		MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("pwm",	"tegra-pwm",		NULL,	17,	0x110,	48000000, mux_pllp_pllc_clk32_clkm,	MUX | DIV_U71 | PERIPH_ON_APB),
	D_AUDIO_CLK("d_audio",	"tegra30-ahub",		"d_audio",	106,	0x3d0,	48000000,  mux_d_audio_clk,	MUX | DIV_U71 | PERIPH_ON_APB),
	D_AUDIO_CLK("dam0",	"tegra30-dam.0",	NULL,	108,	0x3d8,	40000000,  mux_d_audio_clk,	MUX | DIV_U71 | PERIPH_ON_APB),
	D_AUDIO_CLK("dam1",	"tegra30-dam.1",	NULL,	109,	0x3dc,	40000000,  mux_d_audio_clk,	MUX | DIV_U71 | PERIPH_ON_APB),
	D_AUDIO_CLK("dam2",	"tegra30-dam.2",	NULL,	110,	0x3e0,	40000000,  mux_d_audio_clk,	MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("adx",	"adx",			NULL,   154,	0x638,	24580000, mux_plla_pllc_pllp_clkm,	MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("adx1",	"adx1",			NULL,   180,	0x670,	24580000, mux_plla_pllc_pllp_clkm,	MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("amx",	"amx",			NULL,   153,	0x63c,	24600000, mux_plla_pllc_pllp_clkm,	MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("amx1",	"amx1",			NULL,   185,	0x674,	24600000, mux_plla_pllc_pllp_clkm,	MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("afc0",	"tegra124-afc.0",	NULL,   186,	0,	26000000, mux_clk_m,			PERIPH_ON_APB),
	PERIPH_CLK("afc1",	"tegra124-afc.1",	NULL,   187,	0,	26000000, mux_clk_m,			PERIPH_ON_APB),
	PERIPH_CLK("afc2",	"tegra124-afc.2",	NULL,   188,	0,	26000000, mux_clk_m,			PERIPH_ON_APB),
	PERIPH_CLK("afc3",	"tegra124-afc.3",	NULL,   189,	0,	26000000, mux_clk_m,			PERIPH_ON_APB),
	PERIPH_CLK("afc4",	"tegra124-afc.4",	NULL,   190,	0,	26000000, mux_clk_m,			PERIPH_ON_APB),
	PERIPH_CLK("afc5",	"tegra124-afc.5",	NULL,   191,	0,	26000000, mux_clk_m,			PERIPH_ON_APB),
	PERIPH_CLK("hda",	"tegra30-hda",		"hda",   125,	0x428,	108000000, mux_pllp_pllc_pllm_clkm,	MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("hda2codec_2x",	"tegra30-hda",	"hda2codec",   111,	0x3e4,	48000000,  mux_pllp_pllc_pllm_clkm,	MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("hda2hdmi",	"tegra30-hda",		"hda2hdmi",	128,	0,	48000000,  mux_clk_m,			PERIPH_ON_APB),
	PERIPH_CLK("sbc1",	"spi-tegra114.0",	NULL,	41,	0x134, 33000000, mux_pllp_pllc_pllm_clkm,	MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("sbc2",	"spi-tegra114.1",	NULL,	44,	0x118, 33000000, mux_pllp_pllc_pllm_clkm,	MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("sbc3",	"spi-tegra114.2",	NULL,	46,	0x11c, 33000000, mux_pllp_pllc_pllm_clkm,	MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("sbc4",	"spi-tegra114.3",	NULL,	68,	0x1b4, 33000000, mux_pllp_pllc_pllm_clkm,	MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("sbc5",	"spi-tegra114.4",	NULL,	104,	0x3c8, 33000000, mux_pllp_pllc_pllm_clkm,	MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("sbc6",	"spi-tegra114.5",	NULL,	105,	0x3cc, 33000000, mux_pllp_pllc_pllm_clkm,	MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("sata_oob",	"tegra_sata_oob",	NULL,	123,	0x420,	216000000, mux_pllp_pllc_pllm_clkm,	MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("sata",	"tegra_sata",		NULL,	124,	0x424,	216000000, mux_pllp_pllc_pllm_clkm,	MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("sata_cold",	"tegra_sata_cold",	NULL,	129,	0,	48000000,  mux_clk_m,			PERIPH_ON_APB),
	PERIPH_CLK("vfir",	"vfir",			NULL,	7,	0x168,	72000000,  mux_pllp_pllc_pllm_clkm,	MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("sdmmc1",	"sdhci-tegra.0",	NULL,	14,	0x150,	208000000, mux_pllp_pllc_pllm_clkm,	MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("sdmmc2",	"sdhci-tegra.1",	NULL,	9,	0x154,	200000000, mux_pllp_pllc_pllm_clkm,	MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("sdmmc3",	"sdhci-tegra.2",	NULL,	69,	0x1bc,	208000000, mux_pllp_pllc_pllm_clkm,	MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("sdmmc4",	"sdhci-tegra.3",	NULL,	15,	0x164,	200000000, mux_pllp_pllc_pllm_clkm,	MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("sdmmc1_ddr", "sdhci-tegra.0",	"ddr",	14,	0x150,	100000000, mux_pllp_pllc_pllm_clkm,	MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("sdmmc3_ddr", "sdhci-tegra.2",	"ddr",	69,	0x1bc,	100000000, mux_pllp_pllc_pllm_clkm,	MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("sdmmc4_ddr", "sdhci-tegra.3",	"ddr",	15,	0x164,	102000000, mux_pllp_pllc_pllm_clkm,	MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("vcp",	"tegra-avp",		"vcp",	29,	0,	250000000, mux_clk_m, 			0),
	PERIPH_CLK("bsea",	"tegra-avp",		"bsea",	62,	0,	250000000, mux_clk_m, 			0),
	PERIPH_CLK("bsev",	"tegra-aes",		"bsev",	63,	0,	250000000, mux_clk_m, 			0),
	PERIPH_CLK("cec",	"tegra_cec",		NULL,	136,	0,	250000000, mux_clk_m,			PERIPH_ON_APB),
	PERIPH_CLK("vde",	"vde",			NULL,	61,	0x1c8,	600000000, mux_pllp_pllc2_c_c3_pllm_clkm,	MUX | DIV_U71 | DIV_U71_INT),
	PERIPH_CLK("csite",	"csite",		NULL,	73,	0x1d4,	144000000, mux_pllp_pllc_pllm_clkm,	MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("la",	"la",			NULL,	76,	0x1f8,	26000000,  mux_pllp_pllc_pllm_clkm,	MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("trace",	"trace",		NULL,	77,	0x634,	26000000,  mux_pllp_pllc_pllm_clkm,	MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("owr",	"tegra_w1",		NULL,	71,	0x1cc,	26000000,  mux_pllp_pllc_pllm_clkm,	MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("nor",	"tegra-nor",		NULL,	42,	0x1d0,	127000000, mux_pllp_pllc_pllm_clkm,	MUX | DIV_U71),
	PERIPH_CLK("mipi",	"mipi",			NULL,	50,	0x174,	60000000,  mux_pllp_pllc_pllm_clkm,	MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("i2c1",	"tegra12-i2c.0",	"div-clk",	12,	0x124,	136000000, mux_pllp_clkm,	MUX | DIV_U16 | PERIPH_ON_APB),
	PERIPH_CLK("i2c2",	"tegra12-i2c.1",	"div-clk",	54,	0x198,	136000000, mux_pllp_clkm,	MUX | DIV_U16 | PERIPH_ON_APB),
	PERIPH_CLK("i2c3",	"tegra12-i2c.2",	"div-clk",	67,	0x1b8,	136000000, mux_pllp_clkm,	MUX | DIV_U16 | PERIPH_ON_APB),
	PERIPH_CLK("i2c4",	"tegra12-i2c.3",	"div-clk",	103,	0x3c4,	136000000, mux_pllp_clkm,	MUX | DIV_U16 | PERIPH_ON_APB),
	PERIPH_CLK("i2c5",	"tegra12-i2c.4",	"div-clk",	47,	0x128,	136000000,  mux_pllp_clkm,	MUX | DIV_U16 | PERIPH_ON_APB),
	PERIPH_CLK("i2c6",	"tegra12-i2c.5",	"div-clk",	166,	0x65c,	58300000,  mux_pllp_clkm,	MUX | DIV_U16 | PERIPH_ON_APB),
	PERIPH_CLK("mipi-cal",	"mipi-cal",		NULL,	56,	0,	60000000,  mux_clk_m,			PERIPH_ON_APB),
	PERIPH_CLK("mipi-cal-fixed", "mipi-cal-fixed",	NULL,	0,	0,	108000000, mux_pllp_out3,	PERIPH_NO_ENB),
	PERIPH_CLK("uarta",	"serial-tegra.0",		NULL,	6,	0x178,	800000000, mux_pllp_pllc_pllm_clkm,	MUX | DIV_U151 | DIV_U151_UART | PERIPH_ON_APB),
	PERIPH_CLK("uartb",	"serial-tegra.1",		NULL,	7,	0x17c,	800000000, mux_pllp_pllc_pllm_clkm,	MUX | DIV_U151 | DIV_U151_UART | PERIPH_ON_APB),
	PERIPH_CLK("uartc",	"serial-tegra.2",		NULL,	55,	0x1a0,	800000000, mux_pllp_pllc_pllm_clkm,	MUX | DIV_U151 | DIV_U151_UART | PERIPH_ON_APB),
	PERIPH_CLK("uartd",	"serial-tegra.3",		NULL,	65,	0x1c0,	800000000, mux_pllp_pllc_pllm_clkm,	MUX | DIV_U151 | DIV_U151_UART | PERIPH_ON_APB),
	PERIPH_CLK("vic03",	"vic03",		NULL,	178,	0x678,	900000000, mux_pllm_pllc_pllp_plla_pllc2_c3_clkm,	MUX | DIV_U71),
	PERIPH_CLK_EX("vi",	"vi",			"vi",	20,	0x148,	700000000, mux_pllm_pllc_pllp_plla_pllc4,	MUX | DIV_U71 | DIV_U71_INT, &tegra_vi_clk_ops),
	PERIPH_CLK("vi_sensor",	 NULL,			"vi_sensor",	164,	0x1a8,	150000000, mux_pllm_pllc_pllp_plla,	MUX | DIV_U71 | PERIPH_NO_RESET),
	PERIPH_CLK("vi_sensor2", NULL,			"vi_sensor2",	165,	0x658,	150000000, mux_pllm_pllc_pllp_plla,	MUX | DIV_U71 | PERIPH_NO_RESET),
	PERIPH_CLK_EX("msenc",	"msenc",		NULL,	91,	0x1f0,	600000000, mux_pllm_pllc2_c_c3_pllp_plla,	MUX | DIV_U71 | DIV_U71_INT, &tegra_msenc_clk_ops),
	PERIPH_CLK("tsec",	"tsec",			NULL,	83,	0x1f4,	900000000, mux_pllp_pllc2_c_c3_pllm_clkm,	MUX | DIV_U71 | DIV_U71_INT),
	PERIPH_CLK_EX("dtv",	"dtv",			NULL,	79,	0x1dc,	250000000, mux_clk_m,			PERIPH_ON_APB,	&tegra_dtv_clk_ops),
	PERIPH_CLK("hdmi",	"hdmi",			NULL,	51,	0x18c,	594000000, mux_pllp_pllm_plld_plla_pllc_plld2_clkm,	MUX | DIV_U71),
	PERIPH_CLK("disp1",	"tegradc.0",		NULL,	27,	0x138,	600000000, mux_pllp_pllm_plld_plla_pllc_plld2_clkm,	MUX),
	PERIPH_CLK("disp2",	"tegradc.1",		NULL,	26,	0x13c,	600000000, mux_pllp_pllm_plld_plla_pllc_plld2_clkm,	MUX),
	PERIPH_CLK_EX("sor0",	"sor0",			NULL,	182,	0x414,	198000000, mux_pllp_pllm_plld_plla_pllc_plld2_clkm,	MUX | DIV_U71, &tegra_sor_clk_ops),
	PERIPH_CLK("dpaux",	"dpaux",		NULL,	181,	0,	24000000, mux_clk_m,			0),
	PERIPH_CLK("usbd",	"tegra-udc.0",		NULL,	22,	0,	480000000, mux_clk_m,			0),
	PERIPH_CLK("usb2",	"tegra-ehci.1",		NULL,	58,	0,	480000000, mux_clk_m,			0),
	PERIPH_CLK("usb3",	"tegra-ehci.2",		NULL,	59,	0,	480000000, mux_clk_m,			0),
	PERIPH_CLK_EX("dsia",	"tegradc.0",		"dsia",	48,	0xd0,	750000000, mux_plld_out0,		PLLD,	&tegra_dsi_clk_ops),
	PERIPH_CLK_EX("dsib",	"tegradc.1",		"dsib",	82,	0x4b8,	750000000, mux_plld_out0,		PLLD,	&tegra_dsi_clk_ops),
	PERIPH_CLK("dsi1-fixed", "tegradc.0",		"dsi-fixed",	0,	0,	108000000, mux_pllp_out3,	PERIPH_NO_ENB),
	PERIPH_CLK("dsi2-fixed", "tegradc.1",		"dsi-fixed",	0,	0,	108000000, mux_pllp_out3,	PERIPH_NO_ENB),
	PERIPH_CLK("csi",	"vi",			"csi",	52,	0,	750000000, mux_plld,			PLLD),
	PERIPH_CLK("ispa",	"isp",			"ispa",	23,	0,	700000000, mux_isp,			PERIPH_ON_APB),
	PERIPH_CLK("ispb",	"isp",			"ispb",	3,	0,	700000000, mux_isp,			PERIPH_ON_APB),
	PERIPH_CLK("csus",	"vi",			"csus",	92,	0,	150000000, mux_clk_m,			PERIPH_NO_RESET),
	PERIPH_CLK("vim2_clk",	"vi",			"vim2_clk",	171,	0,	150000000, mux_clk_m,		PERIPH_NO_RESET),
	PERIPH_CLK("cilab",	"vi",			"cilab", 144,	0x614,	102000000, mux_pllp_pllc_clkm,		MUX | DIV_U71),
	PERIPH_CLK("cilcd",	"vi",			"cilcd", 145,	0x618,	102000000, mux_pllp_pllc_clkm,		MUX | DIV_U71),
	PERIPH_CLK("cile",	"vi",			"cile",  146,	0x61c,	102000000, mux_pllp_pllc_clkm,		MUX | DIV_U71),
	PERIPH_CLK("dsialp",	"tegradc.0",		"dsialp", 147,	0x620,	156000000, mux_pllp_pllc_clkm,		MUX | DIV_U71),
	PERIPH_CLK("dsiblp",	"tegradc.1",		"dsiblp", 148,	0x624,	156000000, mux_pllp_pllc_clkm,		MUX | DIV_U71),
	PERIPH_CLK("entropy",	"entropy",		NULL, 149,	0x628,	102000000, mux_pllp_clkm_clk32_plle,	MUX | DIV_U71),
	PERIPH_CLK("hdmi_audio", "hdmi_audio",		NULL, 176,	0x668,	48000000,  mux_pllp_pllc_clkm1,		MUX | DIV_U71 | PERIPH_NO_RESET),
	PERIPH_CLK("clk72mhz",	"clk72mhz",		NULL, 177,	0x66c,	102000000, mux_pllp3_pllc_clkm,		MUX | DIV_U71 | PERIPH_NO_RESET),

	PERIPH_CLK("tsensor",	"tegra-tsensor",	NULL,	100,	0x3b8,	 12000000, mux_pllp_pllc_clkm_clk32,	MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("actmon",	"actmon",		NULL,	119,	0x3e8,	216000000, mux_pllp_pllc_clk32_clkm,	MUX | DIV_U71),
	PERIPH_CLK("extern1",	"extern1",		NULL,	120,	0x3ec,	216000000, mux_plla_clk32_pllp_clkm_plle,	MUX | DIV_U71),
	PERIPH_CLK("extern2",	"extern2",		NULL,	121,	0x3f0,	216000000, mux_plla_clk32_pllp_clkm_plle,	MUX | DIV_U71),
	PERIPH_CLK("extern3",	"extern3",		NULL,	122,	0x3f4,	216000000, mux_plla_clk32_pllp_clkm_plle,	MUX | DIV_U71),
	PERIPH_CLK("i2cslow",	"i2cslow",		NULL,	81,	0x3fc,	26000000,  mux_pllp_pllc_clk32_clkm,	MUX | DIV_U71 | PERIPH_ON_APB),
	PERIPH_CLK("pcie",	"tegra-pcie",		"pcie",	70,	0,	250000000, mux_clk_m, 			0),
	PERIPH_CLK("afi",	"tegra-pcie",		"afi",	72,	0,	250000000, mux_clk_m, 			0),
	PERIPH_CLK("se",	"se",			NULL,	127,	0x42c,	600000000, mux_pllp_pllc2_c_c3_pllm_clkm,	MUX | DIV_U71 | DIV_U71_INT | PERIPH_ON_APB),
	PERIPH_CLK("cl_dvfs_ref", "tegra_cl_dvfs",	"ref",	155,	0x62c,	54000000,  mux_pllp_clkm,		MUX | DIV_U71 | DIV_U71_INT | PERIPH_ON_APB),
	PERIPH_CLK("cl_dvfs_soc", "tegra_cl_dvfs",	"soc",	155,	0x630,	54000000,  mux_pllp_clkm,		MUX | DIV_U71 | DIV_U71_INT | PERIPH_ON_APB),
	PERIPH_CLK("soc_therm",	"soc_therm",		NULL,   78,	0x644,	136000000, mux_pllm_pllc_pllp_plla_v2,	MUX | DIV_U71 | PERIPH_ON_APB),

	PERIPH_CLK("dds",	"dds",			NULL,	150,	0,	26000000, mux_clk_m,			PERIPH_ON_APB),
	PERIPH_CLK("dp2",	"dp2",			NULL,	152,	0,	26000000, mux_clk_m,			PERIPH_ON_APB),

	SHARED_SCLK("automotive.hclk", "automotive",	"hclk",	&tegra_clk_ahb,        NULL, 0, 0),
	SHARED_SCLK("automotive.pclk", "automotive",	"pclk",	&tegra_clk_apb,        NULL, 0, 0),

	SHARED_SCLK("avp.sclk",	 "tegra-avp",		"sclk",	&tegra_clk_sbus_cmplx, NULL, 0, 0),
	SHARED_SCLK("bsea.sclk", "tegra-aes",		"sclk",	&tegra_clk_sbus_cmplx, NULL, 0, 0),
	SHARED_SCLK("usbd.sclk", "tegra-udc.0",		"sclk",	&tegra_clk_ahb,        NULL, 0, 0),
	SHARED_SCLK("usb1.sclk", "tegra-ehci.0",	"sclk",	&tegra_clk_ahb,        NULL, 0, 0),
	SHARED_SCLK("usb2.sclk", "tegra-ehci.1",	"sclk",	&tegra_clk_ahb,        NULL, 0, 0),
	SHARED_SCLK("usb3.sclk", "tegra-ehci.2",	"sclk",	&tegra_clk_ahb,        NULL, 0, 0),
	SHARED_SCLK("wake.sclk", "wake_sclk",		"sclk",	&tegra_clk_sbus_cmplx, NULL, 0, 0),
	SHARED_SCLK("automotive.sclk", "automotive",	"sclk", &tegra_clk_sbus_cmplx, NULL, 0, 0),
	SHARED_SCLK("via.sclk",	"tegra_vi.0",		"sclk",	&tegra_clk_sbus_cmplx, NULL, 0, 0),
	SHARED_SCLK("vib.sclk",	"tegra_vi.1",		"sclk",	&tegra_clk_sbus_cmplx, NULL, 0, 0),
	SHARED_SCLK("ispa.sclk",	"tegra_isp.0",		"sclk",	&tegra_clk_sbus_cmplx, NULL, 0, 0),
	SHARED_SCLK("ispb.sclk",	"tegra_isp.1",		"sclk",	&tegra_clk_sbus_cmplx, NULL, 0, 0),
	SHARED_SCLK("mon.avp",	"tegra_actmon",		"avp",	&tegra_clk_sbus_cmplx, NULL, 0, 0),
	SHARED_SCLK("cap.sclk",	"cap_sclk",		NULL,	&tegra_clk_sbus_cmplx, NULL, 0, SHARED_CEILING),
	SHARED_SCLK("cap.vcore.sclk",	"cap.vcore.sclk", NULL,	&tegra_clk_sbus_cmplx, NULL, 0, SHARED_CEILING),
	SHARED_SCLK("cap.throttle.sclk", "cap_throttle", NULL,	&tegra_clk_sbus_cmplx, NULL, 0, SHARED_CEILING),
	SHARED_SCLK("floor.sclk", "floor_sclk",		NULL,	&tegra_clk_sbus_cmplx, NULL, 0, 0),
	SHARED_SCLK("override.sclk", "override_sclk",	NULL,	&tegra_clk_sbus_cmplx, NULL, 0, SHARED_OVERRIDE),
	SHARED_SCLK("sbc1.sclk", "tegra12-spi.0",	"sclk", &tegra_clk_apb,        NULL, 0, 0),
	SHARED_SCLK("sbc2.sclk", "tegra12-spi.1",	"sclk", &tegra_clk_apb,        NULL, 0, 0),
	SHARED_SCLK("sbc3.sclk", "tegra12-spi.2",	"sclk", &tegra_clk_apb,        NULL, 0, 0),
	SHARED_SCLK("sbc4.sclk", "tegra12-spi.3",	"sclk", &tegra_clk_apb,        NULL, 0, 0),
	SHARED_SCLK("sbc5.sclk", "tegra12-spi.4",	"sclk", &tegra_clk_apb,        NULL, 0, 0),
	SHARED_SCLK("sbc6.sclk", "tegra12-spi.5",	"sclk", &tegra_clk_apb,        NULL, 0, 0),

	SHARED_EMC_CLK("avp.emc",	"tegra-avp",	"emc",	&tegra_clk_emc, NULL, 0, 0, 0),
	SHARED_EMC_CLK("mon_cpu.emc", "tegra_mon", "cpu_emc",
						&tegra_clk_emc, NULL, 0, 0, 0),
	SHARED_EMC_CLK("disp1.emc",	"tegradc.0",	"emc",	&tegra_clk_emc, NULL, 0, SHARED_ISO_BW, BIT(EMC_USER_DC1)),
	SHARED_EMC_CLK("disp2.emc",	"tegradc.1",	"emc",	&tegra_clk_emc, NULL, 0, SHARED_ISO_BW, BIT(EMC_USER_DC2)),
	SHARED_EMC_CLK("hdmi.emc",	"hdmi",		"emc",	&tegra_clk_emc, NULL, 0, 0, 0),
	SHARED_EMC_CLK("usbd.emc",	"tegra-udc.0",	"emc",	&tegra_clk_emc, NULL, 0, 0, 0),
	SHARED_EMC_CLK("usb1.emc",	"tegra-ehci.0",	"emc",	&tegra_clk_emc, NULL, 0, 0, 0),
	SHARED_EMC_CLK("usb2.emc",	"tegra-ehci.1",	"emc",	&tegra_clk_emc, NULL, 0, 0, 0),
	SHARED_EMC_CLK("usb3.emc",	"tegra-ehci.2",	"emc",	&tegra_clk_emc, NULL, 0, 0, 0),
	SHARED_EMC_CLK("sdmmc3.emc",    "sdhci-tegra.2","emc",  &tegra_clk_emc, NULL, 0, 0, 0),
	SHARED_EMC_CLK("sdmmc4.emc",    "sdhci-tegra.3","emc",  &tegra_clk_emc, NULL, 0, 0, 0),
	SHARED_EMC_CLK("mon.emc",	"tegra_actmon",	"emc",	&tegra_clk_emc, NULL, 0, 0, 0),
	SHARED_EMC_CLK("cap.emc",	"cap.emc",	NULL,	&tegra_clk_emc, NULL, 0, SHARED_CEILING, 0),
	SHARED_EMC_CLK("cap.vcore.emc",	"cap.vcore.emc", NULL,	&tegra_clk_emc, NULL, 0, SHARED_CEILING, 0),
	SHARED_EMC_CLK("cap.throttle.emc", "cap_throttle", NULL, &tegra_clk_emc, NULL, 0, SHARED_CEILING_BUT_ISO, 0),
	SHARED_EMC_CLK("3d.emc",	"tegra_gk20a.0", "emc",	&tegra_clk_emc, NULL, 0, 0, BIT(EMC_USER_3D)),
	SHARED_EMC_CLK("msenc.emc",	"tegra_msenc",	"emc",	&tegra_clk_emc, NULL, 0, SHARED_BW,	BIT(EMC_USER_MSENC)),
	SHARED_EMC_CLK("tsec.emc",	"tegra_tsec",	"emc",	&tegra_clk_emc, NULL, 0, 0, 0),
	SHARED_EMC_CLK("via.emc",	"tegra_vi.0",	"emc",	&tegra_clk_emc, NULL, 0, SHARED_ISO_BW,	BIT(EMC_USER_VI)),
	SHARED_EMC_CLK("vib.emc",	"tegra_vi.1",	"emc",	&tegra_clk_emc, NULL, 0, SHARED_ISO_BW,	BIT(EMC_USER_VI2)),
	SHARED_EMC_CLK("ispa.emc",	"tegra_isp.0",	"emc",	&tegra_clk_emc, NULL, 0, SHARED_ISO_BW,	BIT(EMC_USER_ISP1)),
	SHARED_EMC_CLK("ispb.emc",	"tegra_isp.1",	"emc",	&tegra_clk_emc, NULL, 0, SHARED_ISO_BW,	BIT(EMC_USER_ISP2)),
	SHARED_EMC_CLK("iso.emc",	"iso",		"emc",	&tegra_clk_emc, NULL, 0, 0, 0),
	SHARED_EMC_CLK("override.emc", "override.emc",	NULL,	&tegra_clk_emc, NULL, 0, SHARED_OVERRIDE, 0),
	SHARED_EMC_CLK("vic.emc",	"tegra_vic03.0",	"emc",  &tegra_clk_emc, NULL, 0, 0, 0),
	SHARED_EMC_CLK("battery.emc",	"battery_edp",	"emc",	&tegra_clk_emc, NULL, 0, SHARED_CEILING, 0),
	SHARED_LIMIT("floor.emc",	"floor.emc",	NULL,	&tegra_clk_emc, NULL,  0, 0),
	SHARED_LIMIT("floor.profile.emc", "profile.emc", "floor", &tegra_clk_emc, NULL,  0, 0),

#ifdef CONFIG_TEGRA_DUAL_CBUS
	DUAL_CBUS_CLK("msenc.cbus",	"tegra_msenc",		"msenc", &tegra_clk_c2bus, "msenc", 0, 0),
	DUAL_CBUS_CLK("vde.cbus",	"tegra-avp",		"vde",	 &tegra_clk_c2bus, "vde",   0, 0),
	DUAL_CBUS_CLK("se.cbus",	"tegra12-se",		NULL,	 &tegra_clk_c2bus, "se",    0, 0),
	SHARED_LIMIT("cap.c2bus",	"cap.c2bus",		NULL,	 &tegra_clk_c2bus, NULL,    0, SHARED_CEILING),
	SHARED_LIMIT("cap.vcore.c2bus",	"cap.vcore.c2bus",	NULL,	 &tegra_clk_c2bus, NULL,    0, SHARED_CEILING),
	SHARED_LIMIT("cap.throttle.c2bus", "cap_throttle",	NULL,	 &tegra_clk_c2bus, NULL,    0, SHARED_CEILING),
	SHARED_LIMIT("floor.c2bus",	"floor.c2bus",		NULL,	 &tegra_clk_c2bus, NULL,    0, 0),
	SHARED_CLK("override.c2bus",	"override.c2bus",	NULL,	 &tegra_clk_c2bus, NULL,  0, SHARED_OVERRIDE),

	DUAL_CBUS_CLK("vic03.cbus",	"tegra_vic03.0",		"vic03", &tegra_clk_c3bus, "vic03", 0, 0),
	DUAL_CBUS_CLK("tsec.cbus",	"tegra_tsec",		"tsec",  &tegra_clk_c3bus,  "tsec", 0, 0),
	SHARED_LIMIT("cap.c3bus",	"cap.c3bus",		NULL,	 &tegra_clk_c3bus, NULL,    0, SHARED_CEILING),
	SHARED_LIMIT("cap.vcore.c3bus",	"cap.vcore.c3bus",	NULL,	 &tegra_clk_c3bus, NULL,    0, SHARED_CEILING),
	SHARED_LIMIT("cap.throttle.c3bus", "cap_throttle",	NULL,	 &tegra_clk_c3bus, NULL,    0, SHARED_CEILING),
	SHARED_LIMIT("floor.c3bus",	"floor.c3bus",		NULL,	 &tegra_clk_c3bus, NULL,    0, 0),
	SHARED_CLK("override.c3bus",	"override.c3bus",	NULL,	 &tegra_clk_c3bus, NULL,  0, SHARED_OVERRIDE),
#else
	SHARED_CLK("vic03.cbus",  "tegra_vic03.0",	"vic03", &tegra_clk_cbus, "vic03", 0, 0),
	SHARED_CLK("msenc.cbus","tegra_msenc",		"msenc",&tegra_clk_cbus, "msenc", 0, 0),
	SHARED_CLK("tsec.cbus",	"tegra_tsec",		"tsec", &tegra_clk_cbus, "tsec", 0, 0),
	SHARED_CLK("vde.cbus",	"tegra-avp",		"vde",	&tegra_clk_cbus, "vde", 0, 0),
	SHARED_CLK("se.cbus",	"tegra12-se",		NULL,	&tegra_clk_cbus, "se",  0, 0),
	SHARED_LIMIT("cap.cbus", "cap.cbus",		NULL,	&tegra_clk_cbus, NULL,  0, SHARED_CEILING),
	SHARED_LIMIT("cap.vcore.cbus", "cap.vcore.cbus", NULL,	&tegra_clk_cbus, NULL,  0, SHARED_CEILING),
	SHARED_LIMIT("cap.throttle.cbus", "cap_throttle", NULL,	&tegra_clk_cbus, NULL,  0, SHARED_CEILING),
	SHARED_LIMIT("floor.cbus", "floor.cbus",	NULL,	&tegra_clk_cbus, NULL,  0, 0),
	SHARED_CLK("override.cbus", "override.cbus",	NULL,	&tegra_clk_cbus, NULL,  0, SHARED_OVERRIDE),
#endif
	SHARED_CLK("gk20a.gbus",	"tegra_gk20a",	"gpu",	&tegra_clk_gbus, NULL,  0, 0),
	SHARED_LIMIT("cap.gbus",	"cap.gbus",	NULL,	&tegra_clk_gbus, NULL,  0, SHARED_CEILING),
	SHARED_LIMIT("edp.gbus",	"edp.gbus",	NULL,	&tegra_clk_gbus, NULL,  0, SHARED_CEILING),
	SHARED_LIMIT("battery.gbus",	"battery_edp",	"gpu",	&tegra_clk_gbus, NULL,  0, SHARED_CEILING),
	SHARED_LIMIT("cap.throttle.gbus", "cap_throttle", NULL,	&tegra_clk_gbus, NULL,  0, SHARED_CEILING),
	SHARED_LIMIT("cap.profile.gbus", "profile.gbus", "cap",	&tegra_clk_gbus, NULL,  0, SHARED_CEILING),
	SHARED_CLK("override.gbus",	"override.gbus", NULL,	&tegra_clk_gbus, NULL,  0, SHARED_OVERRIDE),
	SHARED_LIMIT("floor.gbus",	"floor.gbus",	NULL,	&tegra_clk_gbus, NULL,  0, 0),
	SHARED_LIMIT("floor.profile.gbus", "profile.gbus", "floor", &tegra_clk_gbus, NULL,  0, 0),

	SHARED_CLK("nv.host1x",	"tegra_host1x",		"host1x", &tegra_clk_host1x, NULL,  0, 0),
	SHARED_CLK("vi.host1x",	"tegra_vi",		"host1x", &tegra_clk_host1x, NULL,  0, 0),
	SHARED_LIMIT("cap.host1x", "cap.host1x",	NULL,	  &tegra_clk_host1x, NULL,  0, SHARED_CEILING),
	SHARED_LIMIT("cap.vcore.host1x", "cap.vcore.host1x", NULL, &tegra_clk_host1x, NULL,  0, SHARED_CEILING),
	SHARED_LIMIT("floor.host1x", "floor.host1x",	NULL,	  &tegra_clk_host1x, NULL,  0, 0),
	SHARED_CLK("override.host1x", "override.host1x", NULL,    &tegra_clk_host1x, NULL,  0, SHARED_OVERRIDE),

	SHARED_CLK("cpu.mselect",	  "cpu",        "mselect",   &tegra_clk_mselect, NULL,  0, 0),
	SHARED_CLK("pcie.mselect",	  "tegra_pcie", "mselect",   &tegra_clk_mselect, NULL,  0, 0),
	SHARED_LIMIT("cap.vcore.mselect", "cap.vcore.mselect", NULL, &tegra_clk_mselect, NULL,  0, SHARED_CEILING),
	SHARED_CLK("override.mselect",    "override.mselect",  NULL, &tegra_clk_mselect, NULL,  0, SHARED_OVERRIDE),
};

/* VI, ISP buses */
static struct clk tegra_visp_clks[] = {
	SHARED_CONNECT("vi.c4bus",	"vi.c4bus",	NULL,	&tegra_clk_c4bus,   "vi",    0, 0),
	SHARED_CONNECT("isp.c4bus",	"isp.c4bus",	NULL,	&tegra_clk_c4bus,   "isp",   0, 0),
	SHARED_CLK("override.c4bus",	"override.c4bus", NULL,	&tegra_clk_c4bus,    NULL,   0, SHARED_OVERRIDE),

	SHARED_CLK("via.vi.c4bus",	"via.vi",	NULL,	&tegra_visp_clks[0], NULL,   0, 0),
	SHARED_CLK("vib.vi.c4bus",	"vib.vi",	NULL,	&tegra_visp_clks[0], NULL,   0, 0),

	SHARED_CLK("ispa.isp.c4bus",	"ispa.isp",	NULL,	&tegra_visp_clks[1], "ispa", 0, 0),
	SHARED_CLK("ispb.isp.c4bus",	"ispb.isp",	NULL,	&tegra_visp_clks[1], "ispb", 0, 0),
};

/* XUSB clocks */
#define XUSB_ID "tegra-xhci"
/* xusb common clock gate - enabled on init and never disabled */
static void tegra12_xusb_gate_clk_init(struct clk *c)
{
	tegra12_periph_clk_enable(c);
}

static struct clk_ops tegra_xusb_gate_clk_ops = {
	.init    = tegra12_xusb_gate_clk_init,
};

static struct clk tegra_clk_xusb_gate = {
	.name      = "xusb_gate",
	.flags     = ENABLE_ON_INIT | PERIPH_NO_RESET,
	.ops       = &tegra_xusb_gate_clk_ops,
	.rate      = 12000000,
	.max_rate  = 48000000,
	.u.periph = {
		.clk_num   = 143,
	},
};

static struct clk tegra_xusb_source_clks[] = {
	PERIPH_CLK("xusb_host_src",	XUSB_ID, "host_src",	143,	0x600,	112000000, mux_clkm_pllp_pllc_pllre,	MUX | DIV_U71 | PERIPH_NO_RESET | PERIPH_ON_APB),
	PERIPH_CLK("xusb_falcon_src",	XUSB_ID, "falcon_src",	143,	0x604,	336000000, mux_clkm_pllp_pllc_pllre,	MUX | DIV_U71 | PERIPH_NO_RESET),
	PERIPH_CLK("xusb_fs_src",	XUSB_ID, "fs_src",	143,	0x608,	 48000000, mux_clkm_48M_pllp_480M,	MUX | DIV_U71 | PERIPH_NO_RESET),
	PERIPH_CLK("xusb_ss_src",	XUSB_ID, "ss_src",	143,	0x610,	120000000, mux_clkm_pllre_clk32_480M_pllc_ref,	MUX | DIV_U71 | PERIPH_NO_RESET),
	PERIPH_CLK("xusb_dev_src",	XUSB_ID, "dev_src",	95,	0x60c,	112000000, mux_clkm_pllp_pllc_pllre,	MUX | DIV_U71 | PERIPH_NO_RESET | PERIPH_ON_APB),
	SHARED_EMC_CLK("xusb.emc",	XUSB_ID, "emc",	&tegra_clk_emc,	NULL,	0,	SHARED_BW, 0),
};

static struct clk tegra_xusb_ss_div2 = {
	.name      = "xusb_ss_div2",
	.ops       = &tegra_clk_m_div_ops,
	.parent    = &tegra_xusb_source_clks[3],
	.mul       = 1,
	.div       = 2,
	.state     = OFF,
	.max_rate  = 61200000,
};

static struct clk_mux_sel mux_ss_div2_pllu_60M[] = {
	{ .input = &tegra_xusb_ss_div2, .value = 0},
	{ .input = &tegra_pll_u_60M,    .value = 1},
	{ 0, 0},
};

static struct clk tegra_xusb_hs_src = {
	.name      = "xusb_hs_src",
	.lookup    = {
		.dev_id    = XUSB_ID,
		.con_id	   = "hs_src",
	},
	.ops       = &tegra_periph_clk_ops,
	.reg       = 0x610,
	.inputs    = mux_ss_div2_pllu_60M,
	.flags     = MUX | PLLU | PERIPH_NO_ENB,
	.max_rate  = 60000000,
	.u.periph = {
		.src_mask  = 0x1 << 25,
		.src_shift = 25,
	},
};

static struct clk_mux_sel mux_xusb_host[] = {
	{ .input = &tegra_xusb_source_clks[0], .value = 0},
	{ .input = &tegra_xusb_source_clks[1], .value = 1},
	{ .input = &tegra_xusb_source_clks[2], .value = 2},
	{ .input = &tegra_xusb_hs_src,         .value = 5},
	{ 0, 0},
};

static struct clk_mux_sel mux_xusb_ss[] = {
	{ .input = &tegra_xusb_source_clks[3], .value = 3},
	{ .input = &tegra_xusb_source_clks[0], .value = 0},
	{ .input = &tegra_xusb_source_clks[1], .value = 1},
	{ 0, 0},
};

static struct clk_mux_sel mux_xusb_dev[] = {
	{ .input = &tegra_xusb_source_clks[4], .value = 4},
	{ .input = &tegra_xusb_source_clks[2], .value = 2},
	{ .input = &tegra_xusb_source_clks[3], .value = 3},
	{ 0, 0},
};

static struct clk tegra_xusb_coupled_clks[] = {
	PERIPH_CLK_EX("xusb_host", XUSB_ID, "host", 89,	0, 350000000, mux_xusb_host, 0,	&tegra_clk_coupled_gate_ops),
	PERIPH_CLK_EX("xusb_ss",   XUSB_ID, "ss",  156,	0, 350000000, mux_xusb_ss,   0,	&tegra_clk_coupled_gate_ops),
	PERIPH_CLK_EX("xusb_dev",  XUSB_ID, "dev",  95, 0, 120000000, mux_xusb_dev,  0,	&tegra_clk_coupled_gate_ops),
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
	CLK_DUPLICATE("uarta",	"serial8250.0", NULL),
	CLK_DUPLICATE("uartb",	"serial8250.1", NULL),
	CLK_DUPLICATE("uartc",	"serial8250.2", NULL),
	CLK_DUPLICATE("uartd",	"serial8250.3", NULL),
	CLK_DUPLICATE("usbd", XUSB_ID, "utmip-pad"),
	CLK_DUPLICATE("usbd", "utmip-pad", NULL),
	CLK_DUPLICATE("usbd", "tegra-ehci.0", NULL),
	CLK_DUPLICATE("usbd", "tegra-otg", NULL),
	CLK_DUPLICATE("disp1", "tegra_dc_dsi_vs1.0", NULL),
	CLK_DUPLICATE("disp1.emc", "tegra_dc_dsi_vs1.0", "emc"),
	CLK_DUPLICATE("disp1", "tegra_dc_dsi_vs1.1", NULL),
	CLK_DUPLICATE("disp1.emc", "tegra_dc_dsi_vs1.1", "emc"),
	CLK_DUPLICATE("hdmi", "tegradc.0", "hdmi"),
	CLK_DUPLICATE("hdmi", "tegradc.1", "hdmi"),
	CLK_DUPLICATE("dsib", "tegradc.0", "dsib"),
	CLK_DUPLICATE("dsia", "tegradc.1", "dsia"),
	CLK_DUPLICATE("dsiblp", "tegradc.0", "dsiblp"),
	CLK_DUPLICATE("dsialp", "tegradc.1", "dsialp"),
	CLK_DUPLICATE("dsia", "tegra_dc_dsi_vs1.0", "dsia"),
	CLK_DUPLICATE("dsia", "tegra_dc_dsi_vs1.1", "dsia"),
	CLK_DUPLICATE("dsialp", "tegra_dc_dsi_vs1.0", "dsialp"),
	CLK_DUPLICATE("dsialp", "tegra_dc_dsi_vs1.1", "dsialp"),
	CLK_DUPLICATE("dsi1-fixed", "tegra_dc_dsi_vs1.0", "dsi-fixed"),
	CLK_DUPLICATE("dsi1-fixed", "tegra_dc_dsi_vs1.1", "dsi-fixed"),
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
	CLK_DUPLICATE("clk_m", NULL, "apb_pclk"),
	CLK_DUPLICATE("i2c1", "tegra-i2c-slave.0", NULL),
	CLK_DUPLICATE("i2c2", "tegra-i2c-slave.1", NULL),
	CLK_DUPLICATE("i2c3", "tegra-i2c-slave.2", NULL),
	CLK_DUPLICATE("i2c4", "tegra-i2c-slave.3", NULL),
	CLK_DUPLICATE("i2c5", "tegra-i2c-slave.4", NULL),
	CLK_DUPLICATE("cl_dvfs_ref", "tegra12-i2c.4", NULL),
	CLK_DUPLICATE("cl_dvfs_soc", "tegra12-i2c.4", NULL),
	CLK_DUPLICATE("sbc1", "tegra11-spi-slave.0", NULL),
	CLK_DUPLICATE("sbc2", "tegra11-spi-slave.1", NULL),
	CLK_DUPLICATE("sbc3", "tegra11-spi-slave.2", NULL),
	CLK_DUPLICATE("sbc4", "tegra11-spi-slave.3", NULL),
	CLK_DUPLICATE("sbc5", "tegra11-spi-slave.4", NULL),
	CLK_DUPLICATE("sbc6", "tegra11-spi-slave.5", NULL),
	CLK_DUPLICATE("vcp", "nvavp", "vcp"),
	CLK_DUPLICATE("avp.sclk", "nvavp", "sclk"),
	CLK_DUPLICATE("avp.emc", "nvavp", "emc"),
	CLK_DUPLICATE("vde.cbus", "nvavp", "vde"),
	CLK_DUPLICATE("i2c5", "tegra_cl_dvfs", "i2c"),
	CLK_DUPLICATE("cpu_g", "tegra_cl_dvfs", "safe_dvfs"),
	CLK_DUPLICATE("actmon", "tegra_host1x", "actmon"),
	CLK_DUPLICATE("gpu_ref", "tegra_gk20a.0", "PLLG_ref"),
	CLK_DUPLICATE("gbus", "tegra_gk20a.0", "PLLG_out"),
	CLK_DUPLICATE("pll_p_out5", "tegra_gk20a.0", "pwr"),
	CLK_DUPLICATE("ispa.isp.c4bus", "tegra_isp.0", "isp"),
	CLK_DUPLICATE("ispb.isp.c4bus", "tegra_isp.1", "isp"),
	CLK_DUPLICATE("via.vi.c4bus", "tegra_vi.0", "vi"),
	CLK_DUPLICATE("vib.vi.c4bus", "tegra_vi.1", "vi"),
	CLK_DUPLICATE("csi", "tegra_vi.0", "csi"),
	CLK_DUPLICATE("csi", "tegra_vi.1", "csi"),
	CLK_DUPLICATE("csus", "tegra_vi.0", "csus"),
	CLK_DUPLICATE("vim2_clk", "tegra_vi.1", "vim2_clk"),
	CLK_DUPLICATE("cilab", "tegra_vi.0", "cilab"),
	CLK_DUPLICATE("cilcd", "tegra_vi.1", "cilcd"),
	CLK_DUPLICATE("cile", "tegra_vi.1", "cile"),
	CLK_DUPLICATE("i2s0", NULL, "i2s0"),
	CLK_DUPLICATE("i2s1", NULL, "i2s1"),
	CLK_DUPLICATE("i2s2", NULL, "i2s2"),
	CLK_DUPLICATE("i2s3", NULL, "i2s3"),
	CLK_DUPLICATE("i2s4", NULL, "i2s4"),
	CLK_DUPLICATE("dam0", NULL, "dam0"),
	CLK_DUPLICATE("dam1", NULL, "dam1"),
	CLK_DUPLICATE("dam2", NULL, "dam2"),
	CLK_DUPLICATE("spdif_in", NULL, "spdif_in"),
	CLK_DUPLICATE("mclk", NULL, "default_mclk"),
	CLK_DUPLICATE("amx", "tegra124-amx.0", NULL),
	CLK_DUPLICATE("amx1", "tegra124-amx.1", NULL),
	CLK_DUPLICATE("adx", "tegra124-adx.0", NULL),
	CLK_DUPLICATE("adx1", "tegra124-adx.1", NULL),
	CLK_DUPLICATE("amx", "tegra30-ahub-apbif", "amx"),
	CLK_DUPLICATE("amx1", "tegra30-ahub-apbif", "amx1"),
	CLK_DUPLICATE("adx", "tegra30-ahub-apbif", "adx"),
	CLK_DUPLICATE("adx1", "tegra30-ahub-apbif", "adx1"),
	CLK_DUPLICATE("d_audio", "tegra30-ahub-xbar", "d_audio"),
	CLK_DUPLICATE("apbif", "tegra30-ahub-apbif", "apbif"),
	CLK_DUPLICATE("afc0", "tegra30-ahub-apbif", "afc0"),
	CLK_DUPLICATE("afc1", "tegra30-ahub-apbif", "afc1"),
	CLK_DUPLICATE("afc2", "tegra30-ahub-apbif", "afc2"),
	CLK_DUPLICATE("afc3", "tegra30-ahub-apbif", "afc3"),
	CLK_DUPLICATE("afc4", "tegra30-ahub-apbif", "afc4"),
	CLK_DUPLICATE("afc5", "tegra30-ahub-apbif", "afc5"),
	CLK_DUPLICATE("cpu_g", "tegra_simon", "cpu"),
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
	&tegra_pll_c2,
	&tegra_pll_c3,
	&tegra_pll_p,
	&tegra_pll_p_out1,
	&tegra_pll_p_out2,
	&tegra_pll_p_out3,
	&tegra_pll_p_out4,
	&tegra_pll_p_out5,
	&tegra_pll_a,
	&tegra_pll_a_out0,
	&tegra_pll_d,
	&tegra_pll_d_out0,
	&tegra_clk_xusb_gate,
	&tegra_pll_u,
	&tegra_pll_u_480M,
	&tegra_pll_u_60M,
	&tegra_pll_u_48M,
	&tegra_pll_u_12M,
	&tegra_pll_x,
	&tegra_pll_x_out0,
	&tegra_dfll_cpu,
	&tegra_pll_d2,
	&tegra_pll_c4,
	&tegra_pll_dp,
	&tegra_pll_re_vco,
	&tegra_pll_re_out,
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
	&tegra_clk_ahb,
	&tegra_clk_apb,
	&tegra_clk_emc,
	&tegra_clk_mc,
	&tegra_clk_host1x,
	&tegra_clk_mselect,
#ifdef CONFIG_TEGRA_DUAL_CBUS
	&tegra_clk_c2bus,
	&tegra_clk_c3bus,
#else
	&tegra_clk_cbus,
#endif
	&tegra_clk_gpu,
	&tegra_clk_gbus,
	&tegra_clk_isp,
	&tegra_clk_c4bus,
};

struct clk *tegra_ptr_camera_mclks[] = {
	&tegra_camera_mclk,
	&tegra_camera_mclk2,
};

/* Return true from this function if the target rate can be locked without
   switching pll clients to back-up source */
static bool tegra12_is_dyn_ramp(
	struct clk *c, unsigned long rate, bool from_vco_min)
{
#if PLLCX_USE_DYN_RAMP
	/* PLLC2, PLLC3 support dynamic ramp only when output divider <= 8 */
	if ((c == &tegra_pll_c2) || (c == &tegra_pll_c3)) {
		struct clk_pll_freq_table cfg, old_cfg;
		unsigned long input_rate = clk_get_rate(c->parent);

		u32 val = clk_readl(c->reg + PLL_BASE);
		PLL_BASE_PARSE(PLLCX, old_cfg, val);
		old_cfg.p = pllcx_p[old_cfg.p];

		if (!pll_dyn_ramp_find_cfg(c, &cfg, rate, input_rate, NULL)) {
			if ((cfg.n == old_cfg.n) ||
			    PLLCX_IS_DYN(cfg.p, old_cfg.p))
				return true;
		}
	}
#endif

#if PLLXC_USE_DYN_RAMP
	/* PPLX, PLLC support dynamic ramp when changing NDIV only */
	if ((c == &tegra_pll_x) || (c == &tegra_pll_c)) {
		struct clk_pll_freq_table cfg, old_cfg;
		unsigned long input_rate = clk_get_rate(c->parent);

		if (from_vco_min) {
			old_cfg.m = PLL_FIXED_MDIV(c, input_rate);
			old_cfg.p = 1;
		} else {
			u32 val = clk_readl(c->reg + PLL_BASE);
			PLL_BASE_PARSE(PLLXC, old_cfg, val);
			old_cfg.p = pllxc_p[old_cfg.p];
		}

		if (!pll_dyn_ramp_find_cfg(c, &cfg, rate, input_rate, NULL)) {
			if ((cfg.m == old_cfg.m) && (cfg.p == old_cfg.p))
				return true;
		}
	}
#endif
	return false;
}

/*
 * Backup pll is used as transitional CPU clock source while main pll is
 * relocking; in addition all CPU rates below backup level are sourced from
 * backup pll only. Target backup levels for each CPU mode are selected high
 * enough to avoid voltage droop when CPU clock is switched between backup and
 * main plls. Actual backup rates will be rounded to match backup source fixed
 * frequency. Backup rates are also used as stay-on-backup thresholds, and must
 * be kept the same in G and LP mode (will need to add a separate stay-on-backup
 * parameter to allow different backup rates if necessary).
 *
 * Sbus threshold must be exact factor of pll_p rate.
 */
#define CPU_G_BACKUP_RATE_TARGET	200000000
#define CPU_LP_BACKUP_RATE_TARGET	200000000

static void tegra12_pllp_init_dependencies(unsigned long pllp_rate)
{
	u32 div;
	unsigned long backup_rate;

	switch (pllp_rate) {
	case 216000000:
		tegra_pll_p_out1.u.pll_div.default_rate = 28800000;
		tegra_pll_p_out3.u.pll_div.default_rate = 72000000;
		tegra_clk_sbus_cmplx.u.system.threshold = 108000000;
		tegra_clk_host1x.u.periph.threshold = 108000000;
		break;
	case 408000000:
		tegra_pll_p_out1.u.pll_div.default_rate = 9600000;
		tegra_pll_p_out3.u.pll_div.default_rate = 102000000;
		tegra_clk_sbus_cmplx.u.system.threshold = 204000000;
		tegra_clk_host1x.u.periph.threshold = 204000000;
		break;
	case 204000000:
		tegra_pll_p_out1.u.pll_div.default_rate = 4800000;
		tegra_pll_p_out3.u.pll_div.default_rate = 102000000;
		tegra_clk_sbus_cmplx.u.system.threshold = 204000000;
		tegra_clk_host1x.u.periph.threshold = 204000000;
		break;
	default:
		pr_err("tegra: PLLP rate: %lu is not supported\n", pllp_rate);
		BUG();
	}
	pr_info("tegra: PLLP fixed rate: %lu\n", pllp_rate);

	div = pllp_rate / CPU_G_BACKUP_RATE_TARGET;
	backup_rate = pllp_rate / div;
	tegra_clk_virtual_cpu_g.u.cpu.backup_rate = backup_rate;

	div = pllp_rate / CPU_LP_BACKUP_RATE_TARGET;
	backup_rate = pllp_rate / div;
	tegra_clk_virtual_cpu_lp.u.cpu.backup_rate = backup_rate;
}

static void tegra12_init_one_clock(struct clk *c)
{
	clk_init(c);
	INIT_LIST_HEAD(&c->shared_bus_list);
	if (!c->lookup.dev_id && !c->lookup.con_id)
		c->lookup.con_id = c->name;
	c->lookup.clk = c;
	clkdev_add(&c->lookup);
}

/* Direct access to CPU clock sources fot CPU idle driver */
int tegra12_cpu_g_idle_rate_exchange(unsigned long *rate)
{
	int ret = 0;
	struct clk *dfll = tegra_clk_cpu_cmplx.parent->u.cpu.dynamic;
	unsigned long old_rate, new_rate, flags;

	if (!dfll || !tegra_dvfs_rail_is_dfll_mode(tegra_cpu_rail))
		return -EPERM;

	/* Clipping min to oscillator rate is pretty much arbitrary */
	new_rate = max(*rate, tegra_clk_m.rate);

	clk_lock_save(dfll, &flags);

	old_rate = clk_get_rate_locked(dfll);
	*rate = old_rate;
	if (new_rate != old_rate)
		ret = clk_set_rate_locked(dfll, new_rate);

	clk_unlock_restore(dfll, &flags);
	return ret;
}

int tegra12_cpu_lp_idle_rate_exchange(unsigned long *rate)
{
	int ret = 0;
	struct clk *backup = tegra_clk_cpu_cmplx.parent->u.cpu.backup;
	unsigned long old_rate, flags;
	unsigned long new_rate = min(
		*rate, tegra_clk_cpu_cmplx.parent->u.cpu.backup_rate);

	clk_lock_save(backup, &flags);

	old_rate = clk_get_rate_locked(backup);
	*rate = old_rate;
	if (new_rate != old_rate)
		ret = clk_set_rate_locked(backup, new_rate);

	clk_unlock_restore(backup, &flags);
	return ret;
}

void tegra_edp_throttle_cpu_now(u8 factor)
{
	/* empty definition for tegra12 */
	return;
}

bool tegra_clk_is_parent_allowed(struct clk *c, struct clk *p)
{
	/*
	 * Most of the Tegra12 multimedia and peripheral muxes include pll_c2
	 * and pll_c3 as possible inputs. However, per clock policy these plls
	 * are allowed to be used only by handful devices aggregated on cbus.
	 * For all others, instead of enforcing policy at run-time in this
	 * function, we simply stripped out pll_c2 and pll_c3 options from the
	 * respective muxes statically.
	 */

	/*
	 * In configuration with dual cbus pll_c can be used as a scaled clock
	 * source for EMC only when pll_m is fixed, or as a general fixed rate
	 * clock source for EMC and other peripherals if pll_m is scaled. In
	 * configuration with single cbus pll_c can be used as a scaled cbus
	 * clock source only. No direct use for pll_c by super clocks.
	 */
	if ((p == &tegra_pll_c) && (c != &tegra_pll_c_out1)) {
		if (c->ops == &tegra_super_ops)
			return false;
#ifdef CONFIG_TEGRA_DUAL_CBUS
#ifndef CONFIG_TEGRA_PLLM_SCALED
		return c->flags & PERIPH_EMC_ENB;
#endif
#else
		return c->flags & PERIPH_ON_CBUS;
#endif
	}

	/*
	 * In any configuration pll_m must not be used as a clock source for
	 * cbus modules. If pll_m is scaled it can be used as EMC source only.
	 * Otherwise fixed rate pll_m can be used as clock source for EMC and
	 * other peripherals. No direct use for pll_m by super clocks.
	 */
	if ((p == &tegra_pll_m) && (c != &tegra_pll_m_out1)) {
		if (c->ops == &tegra_super_ops)
			return false;

		if (c->flags & PERIPH_ON_CBUS)
			return false;
#ifdef CONFIG_TEGRA_PLLM_SCALED
		return c->flags & PERIPH_EMC_ENB;
#endif
	}
	return true;
}

/* Internal LA may request some clocks to be enabled on init via TRANSACTION
   SCRATCH register settings */
void __init tegra12x_clk_init_la(void)
{
	struct clk *c;
	u32 reg = readl((void *)
		((uintptr_t)misc_gp_base + MISC_GP_TRANSACTOR_SCRATCH_0));

	if (!(reg & MISC_GP_TRANSACTOR_SCRATCH_LA_ENABLE))
		return;

	c = tegra_get_clock_by_name("la");
	if (WARN(!c, "%s: could not find la clk\n", __func__))
		return;
	clk_enable(c);

	if (reg & MISC_GP_TRANSACTOR_SCRATCH_DDS_ENABLE) {
		c = tegra_get_clock_by_name("dds");
		if (WARN(!c, "%s: could not find la clk\n", __func__))
			return;
		clk_enable(c);
	}
	if (reg & MISC_GP_TRANSACTOR_SCRATCH_DP2_ENABLE) {
		c = tegra_get_clock_by_name("dp2");
		if (WARN(!c, "%s: could not find la clk\n", __func__))
			return;
		clk_enable(c);

		c = tegra_get_clock_by_name("hdmi");
		if (WARN(!c, "%s: could not find la clk\n", __func__))
			return;
		clk_enable(c);
	}
}

#ifdef CONFIG_CPU_FREQ

/*
 * Frequency table index must be sequential starting at 0 and frequencies
 * must be ascending.
 */
#define CPU_FREQ_STEP 102000 /* 102MHz cpu_g table step */
#define CPU_FREQ_TABLE_MAX_SIZE (2 * MAX_DVFS_FREQS + 1)

static struct cpufreq_frequency_table freq_table[CPU_FREQ_TABLE_MAX_SIZE];
static struct tegra_cpufreq_table_data freq_table_data;

struct tegra_cpufreq_table_data *tegra_cpufreq_table_get(void)
{
	int i, j;
	bool g_vmin_done = false;
	unsigned int freq, lp_backup_freq, g_vmin_freq, g_start_freq, max_freq;
	struct clk *cpu_clk_g = tegra_get_clock_by_name("cpu_g");
	struct clk *cpu_clk_lp = tegra_get_clock_by_name("cpu_lp");

	/* Initialize once */
	if (freq_table_data.freq_table)
		return &freq_table_data;

	/* Clean table */
	for (i = 0; i < CPU_FREQ_TABLE_MAX_SIZE; i++) {
		freq_table[i].index = i;
		freq_table[i].frequency = CPUFREQ_TABLE_END;
	}

	lp_backup_freq = cpu_clk_lp->u.cpu.backup_rate / 1000;
	if (!lp_backup_freq) {
		WARN(1, "%s: cannot make cpufreq table: no LP CPU backup rate\n",
		     __func__);
		return NULL;
	}
	if (!cpu_clk_lp->dvfs) {
		WARN(1, "%s: cannot make cpufreq table: no LP CPU dvfs\n",
		     __func__);
		return NULL;
	}
	if (!cpu_clk_g->dvfs) {
		WARN(1, "%s: cannot make cpufreq table: no G CPU dvfs\n",
		     __func__);
		return NULL;
	}
	g_vmin_freq = cpu_clk_g->dvfs->freqs[0] / 1000;
	if (g_vmin_freq < lp_backup_freq) {
		WARN(1, "%s: cannot make cpufreq table: LP CPU backup rate"
			" exceeds G CPU rate at Vmin\n", __func__);
		return NULL;
	}
	/* Avoid duplicate frequency if g_vim_freq is already part of table */
	if (g_vmin_freq == lp_backup_freq)
		g_vmin_done = true;

	/* Start with backup frequencies */
	i = 0;
	freq = lp_backup_freq;
	freq_table[i++].frequency = freq/4;
	freq_table[i++].frequency = freq/2;
	freq_table[i++].frequency = freq;

	/* Throttle low index at backup level*/
	freq_table_data.throttle_lowest_index = i - 1;

	/*
	 * Next, set table steps along LP CPU dvfs ladder, but make sure G CPU
	 * dvfs rate at minimum voltage is not missed (if it happens to be below
	 * LP maximum rate)
	 */
	max_freq = cpu_clk_lp->max_rate / 1000;
	for (j = 0; j < cpu_clk_lp->dvfs->num_freqs; j++) {
		freq = cpu_clk_lp->dvfs->freqs[j] / 1000;
		if (freq <= lp_backup_freq)
			continue;

		if (!g_vmin_done && (freq >= g_vmin_freq)) {
			g_vmin_done = true;
			if (freq > g_vmin_freq)
				freq_table[i++].frequency = g_vmin_freq;
		}
		freq_table[i++].frequency = freq;

		if (freq == max_freq)
			break;
	}

	/* Set G CPU min rate at least one table step below LP maximum */
	cpu_clk_g->min_rate = min(freq_table[i-2].frequency, g_vmin_freq)*1000;

	/* Suspend index at max LP CPU */
	freq_table_data.suspend_index = i - 1;

	/* Fill in "hole" (if any) between LP CPU maximum rate and G CPU dvfs
	   ladder rate at minimum voltage */
	if (freq < g_vmin_freq) {
		int n = (g_vmin_freq - freq) / CPU_FREQ_STEP;
		for (j = 0; j <= n; j++) {
			freq = g_vmin_freq - CPU_FREQ_STEP * (n - j);
			freq_table[i++].frequency = freq;
		}
	}

	/* Now, step along the rest of G CPU dvfs ladder */
	g_start_freq = freq;
	max_freq = cpu_clk_g->max_rate / 1000;
	for (j = 0; j < cpu_clk_g->dvfs->num_freqs; j++) {
		freq = cpu_clk_g->dvfs->freqs[j] / 1000;
		if (freq > g_start_freq)
			freq_table[i++].frequency = freq;
		if (freq == max_freq)
			break;
	}

	/* Throttle high index one step below maximum */
	BUG_ON(i >= CPU_FREQ_TABLE_MAX_SIZE);
	freq_table_data.throttle_highest_index = i - 2;
	freq_table_data.freq_table = freq_table;
	return &freq_table_data;
}

unsigned long tegra_emc_to_cpu_ratio(unsigned long cpu_rate)
{
	static unsigned long emc_max_rate;

	if (emc_max_rate == 0)
		emc_max_rate = clk_round_rate(
			tegra_get_clock_by_name("emc"), ULONG_MAX);

	/* Vote on memory bus frequency based on cpu frequency;
	   cpu rate is in kHz, emc rate is in Hz */
	if (cpu_rate >= 1300000)
		return emc_max_rate;	/* cpu >= 1.3GHz, emc max */
	else if (cpu_rate >= 975000)
		return 400000000;	/* cpu >= 975 MHz, emc 400 MHz */
	else if (cpu_rate >= 725000)
		return  200000000;	/* cpu >= 725 MHz, emc 200 MHz */
	else if (cpu_rate >= 500000)
		return  100000000;	/* cpu >= 500 MHz, emc 100 MHz */
	else if (cpu_rate >= 275000)
		return  50000000;	/* cpu >= 275 MHz, emc 50 MHz */
	else
		return 0;		/* emc min */
}

int tegra_update_mselect_rate(unsigned long cpu_rate)
{
	static struct clk *mselect; /* statics init to 0 */

	unsigned long mselect_rate;

	if (!mselect) {
		mselect = tegra_get_clock_by_name("cpu.mselect");
		if (!mselect)
			return -ENODEV;
	}

	/* Vote on mselect frequency based on cpu frequency:
	   keep mselect at half of cpu rate up to 102 MHz;
	   cpu rate is in kHz, mselect rate is in Hz */
	mselect_rate = DIV_ROUND_UP(cpu_rate, 2) * 1000;
	mselect_rate = min(mselect_rate, 102000000UL);
	return clk_set_rate(mselect, mselect_rate);
}
#endif

#ifdef CONFIG_PM_SLEEP
static u32 clk_rst_suspend[RST_DEVICES_NUM + CLK_OUT_ENB_NUM +
			   PERIPH_CLK_SOURCE_NUM + 27];

static int tegra12_clk_suspend(void)
{
	unsigned long off;
	u32 *ctx = clk_rst_suspend;

	*ctx++ = clk_readl(OSC_CTRL) & OSC_CTRL_MASK;
	*ctx++ = clk_readl(CPU_SOFTRST_CTRL);
	*ctx++ = clk_readl(CPU_SOFTRST_CTRL1);
	*ctx++ = clk_readl(CPU_SOFTRST_CTRL2);

	*ctx++ = clk_readl(tegra_pll_p_out1.reg);
	*ctx++ = clk_readl(tegra_pll_p_out3.reg);
	*ctx++ = clk_readl(tegra_pll_p_out5.reg);

	*ctx++ = clk_readl(tegra_pll_a.reg + PLL_BASE);
	*ctx++ = clk_readl(tegra_pll_a.reg + PLL_MISC(&tegra_pll_a));
	*ctx++ = clk_readl(tegra_pll_d.reg + PLL_BASE);
	*ctx++ = clk_readl(tegra_pll_d.reg + PLL_MISC(&tegra_pll_d));
	*ctx++ = clk_readl(tegra_pll_d2.reg + PLL_BASE);
	*ctx++ = clk_readl(tegra_pll_d2.reg + PLL_MISC(&tegra_pll_d2));

	*ctx++ = clk_readl(tegra_pll_m_out1.reg);
	*ctx++ = clk_readl(tegra_pll_a_out0.reg);
	*ctx++ = clk_readl(tegra_pll_c_out1.reg);

	*ctx++ = clk_readl(tegra_clk_cclk_lp.reg);
	*ctx++ = clk_readl(tegra_clk_cclk_lp.reg + SUPER_CLK_DIVIDER);

	*ctx++ = clk_readl(tegra_clk_sclk.reg);
	*ctx++ = clk_readl(tegra_clk_sclk.reg + SUPER_CLK_DIVIDER);
	*ctx++ = clk_readl(tegra_clk_pclk.reg);

	for (off = PERIPH_CLK_SOURCE_I2S1; off <= PERIPH_CLK_SOURCE_LA;
			off += 4) {
		if (off == PERIPH_CLK_SOURCE_EMC)
			continue;
		*ctx++ = clk_readl(off);
	}
	for (off = PERIPH_CLK_SOURCE_MSELECT; off <= PERIPH_CLK_SOURCE_SE;
			off+=4) {
		*ctx++ = clk_readl(off);
	}
	for (off = AUDIO_DLY_CLK; off <= AUDIO_SYNC_CLK_SPDIF; off+=4) {
		*ctx++ = clk_readl(off);
	}
	for (off = PERIPH_CLK_SOURCE_XUSB_HOST;
		off <= PERIPH_CLK_SOURCE_VIC; off += 4)
		*ctx++ = clk_readl(off);

	*ctx++ = clk_readl(RST_DEVICES_L);
	*ctx++ = clk_readl(RST_DEVICES_H);
	*ctx++ = clk_readl(RST_DEVICES_U);
	*ctx++ = clk_readl(RST_DEVICES_V);
	*ctx++ = clk_readl(RST_DEVICES_W);
	*ctx++ = clk_readl(RST_DEVICES_X);

	*ctx++ = clk_readl(CLK_OUT_ENB_L);
	*ctx++ = clk_readl(CLK_OUT_ENB_H);
	*ctx++ = clk_readl(CLK_OUT_ENB_U);
	*ctx++ = clk_readl(CLK_OUT_ENB_V);
	*ctx++ = clk_readl(CLK_OUT_ENB_W);
	*ctx++ = clk_readl(CLK_OUT_ENB_X);

	*ctx++ = clk_readl(tegra_clk_cclk_g.reg);
	*ctx++ = clk_readl(tegra_clk_cclk_g.reg + SUPER_CLK_DIVIDER);

	*ctx++ = clk_readl(SPARE_REG);
	*ctx++ = clk_readl(MISC_CLK_ENB);
	*ctx++ = clk_readl(CLK_MASK_ARM);

	*ctx++ = clk_get_rate_all_locked(&tegra_clk_emc);
	return 0;
}

static void tegra12_clk_resume(void)
{
	unsigned long off, rate;
	const u32 *ctx = clk_rst_suspend;
	u32 val;
	u32 plla_base;
	u32 plld_base;
	u32 plld2_base;
	u32 pll_p_out12, pll_p_out34;
	u32 pll_a_out0, pll_m_out1, pll_c_out1;
	struct clk *p;

	/* FIXME: OSC_CTRL already restored by warm boot code? */
	val = clk_readl(OSC_CTRL) & ~OSC_CTRL_MASK;
	val |= *ctx++;
	clk_writel(val, OSC_CTRL);
	clk_writel(*ctx++, CPU_SOFTRST_CTRL);
	clk_writel(*ctx++, CPU_SOFTRST_CTRL1);
	clk_writel(*ctx++, CPU_SOFTRST_CTRL2);

	/* Since we are going to reset devices and switch clock sources in this
	 * function, plls and secondary dividers is required to be enabled. The
	 * actual value will be restored back later. Note that boot plls: pllm,
	 * pllp, and pllu are already configured and enabled
	 */
	val = PLL_OUT_CLKEN | PLL_OUT_RESET_DISABLE;
	val |= val << 16;
	pll_p_out12 = *ctx++;
	clk_writel(pll_p_out12 | val, tegra_pll_p_out1.reg);
	pll_p_out34 = *ctx++;
	clk_writel(pll_p_out34 | val, tegra_pll_p_out3.reg);

	/* Restore as is, GPU is rail-gated, anyway */
	clk_writel(*ctx++, tegra_pll_p_out5.reg);

	tegra12_pllss_clk_resume_enable(&tegra_pll_c4);
	tegra12_pllss_clk_resume_enable(&tegra_pll_d2);
	tegra12_pllss_clk_resume_enable(&tegra_pll_dp);
	tegra12_pllcx_clk_resume_enable(&tegra_pll_c2);
	tegra12_pllcx_clk_resume_enable(&tegra_pll_c3);
	tegra12_pllxc_clk_resume_enable(&tegra_pll_c);
	tegra12_pllxc_clk_resume_enable(&tegra_pll_x);
	tegra12_pllre_clk_resume_enable(&tegra_pll_re_out);

	plla_base = *ctx++;
	clk_writel(*ctx++, tegra_pll_a.reg + PLL_MISC(&tegra_pll_a));
	clk_writel(plla_base | PLL_BASE_ENABLE, tegra_pll_a.reg + PLL_BASE);

	plld_base = *ctx++;
	clk_writel(*ctx++, tegra_pll_d.reg + PLL_MISC(&tegra_pll_d));
	clk_writel(plld_base | PLL_BASE_ENABLE, tegra_pll_d.reg + PLL_BASE);

	plld2_base = *ctx++;
	clk_writel(*ctx++, tegra_pll_d2.reg + PLL_MISC(&tegra_pll_d2));
	clk_writel(plld2_base | PLL_BASE_ENABLE, tegra_pll_d2.reg + PLL_BASE);

	udelay(1000);

	val = PLL_OUT_CLKEN | PLL_OUT_RESET_DISABLE;
	pll_m_out1 = *ctx++;
	clk_writel(pll_m_out1 | val, tegra_pll_m_out1.reg);
	pll_a_out0 = *ctx++;
	clk_writel(pll_a_out0 | val, tegra_pll_a_out0.reg);
	pll_c_out1 = *ctx++;
	clk_writel(pll_c_out1 | val, tegra_pll_c_out1.reg);

	val = *ctx++;
	tegra12_super_clk_resume(&tegra_clk_cclk_lp,
		tegra_clk_virtual_cpu_lp.u.cpu.backup, val);
	clk_writel(*ctx++, tegra_clk_cclk_lp.reg + SUPER_CLK_DIVIDER);

	clk_writel(*ctx++, tegra_clk_sclk.reg);
	clk_writel(*ctx++, tegra_clk_sclk.reg + SUPER_CLK_DIVIDER);
	clk_writel(*ctx++, tegra_clk_pclk.reg);

	/* enable all clocks before configuring clock sources */
	clk_writel(CLK_OUT_ENB_L_RESET_MASK, CLK_OUT_ENB_L);
	clk_writel(CLK_OUT_ENB_H_RESET_MASK, CLK_OUT_ENB_H);
	clk_writel(CLK_OUT_ENB_U_RESET_MASK, CLK_OUT_ENB_U);
	clk_writel(CLK_OUT_ENB_V_RESET_MASK, CLK_OUT_ENB_V);
	clk_writel(CLK_OUT_ENB_W_RESET_MASK, CLK_OUT_ENB_W);
	clk_writel(CLK_OUT_ENB_X_RESET_MASK, CLK_OUT_ENB_X);
	wmb();

	for (off = PERIPH_CLK_SOURCE_I2S1; off <= PERIPH_CLK_SOURCE_LA;
			off += 4) {
		if (off == PERIPH_CLK_SOURCE_EMC)
			continue;
		clk_writel(*ctx++, off);
	}
	for (off = PERIPH_CLK_SOURCE_MSELECT; off <= PERIPH_CLK_SOURCE_SE;
			off += 4) {
		clk_writel(*ctx++, off);
	}
	for (off = AUDIO_DLY_CLK; off <= AUDIO_SYNC_CLK_SPDIF; off+=4) {
		clk_writel(*ctx++, off);
	}
	for (off = PERIPH_CLK_SOURCE_XUSB_HOST;
		off <= PERIPH_CLK_SOURCE_VIC; off += 4)
		clk_writel(*ctx++, off);

	udelay(RESET_PROPAGATION_DELAY);

	clk_writel(*ctx++, RST_DEVICES_L);
	clk_writel(*ctx++, RST_DEVICES_H);
	clk_writel(*ctx++, RST_DEVICES_U);
	clk_writel(*ctx++, RST_DEVICES_V);
	clk_writel(*ctx++, RST_DEVICES_W);
	clk_writel(*ctx++, RST_DEVICES_X);
	wmb();

	clk_writel(*ctx++, CLK_OUT_ENB_L);
	clk_writel(*ctx++, CLK_OUT_ENB_H);
	clk_writel(*ctx++, CLK_OUT_ENB_U);

	/* For LP0 resume, clk to lpcpu is required to be on */
	val = *ctx++;
	val |= CLK_OUT_ENB_V_CLK_ENB_CPULP_EN;
	clk_writel(val, CLK_OUT_ENB_V);

	clk_writel(*ctx++, CLK_OUT_ENB_W);
	clk_writel(*ctx++, CLK_OUT_ENB_X);
	wmb();

	/* DFLL resume after cl_dvfs and i2c5 clocks are resumed */
	tegra12_dfll_clk_resume(&tegra_dfll_cpu);

	/* CPU G clock restored after DFLL and PLLs */
	clk_writel(*ctx++, tegra_clk_cclk_g.reg);
	clk_writel(*ctx++, tegra_clk_cclk_g.reg + SUPER_CLK_DIVIDER);

	clk_writel(*ctx++, SPARE_REG);
	clk_writel(*ctx++, MISC_CLK_ENB);
	clk_writel(*ctx++, CLK_MASK_ARM);

	/* Restore back the actual pll and secondary divider values */
	clk_writel(pll_p_out12, tegra_pll_p_out1.reg);
	clk_writel(pll_p_out34, tegra_pll_p_out3.reg);

	p = &tegra_pll_c4;
	if (p->state == OFF)
		tegra12_pllss_clk_disable(p);
	p = &tegra_pll_d2;
	if (p->state == OFF)
		tegra12_pllss_clk_disable(p);
	p = &tegra_pll_dp;
	if (p->state == OFF)
		tegra12_pllss_clk_disable(p);
	p = &tegra_pll_c2;
	if (p->state == OFF)
		tegra12_pllcx_clk_disable(p);
	p = &tegra_pll_c3;
	if (p->state == OFF)
		tegra12_pllcx_clk_disable(p);
	p = &tegra_pll_c;
	if (p->state == OFF)
		tegra12_pllxc_clk_disable(p);
	p = &tegra_pll_x;
	if (p->state == OFF)
		tegra12_pllxc_clk_disable(p);
	p = &tegra_pll_re_vco;
	if (p->state == OFF)
		tegra12_pllre_clk_disable(p);

	clk_writel(plla_base, tegra_pll_a.reg + PLL_BASE);
	clk_writel(plld_base, tegra_pll_d.reg + PLL_BASE);
	clk_writel(plld2_base, tegra_pll_d2.reg + PLL_BASE);

	clk_writel(pll_m_out1, tegra_pll_m_out1.reg);
	clk_writel(pll_a_out0, tegra_pll_a_out0.reg);
	clk_writel(pll_c_out1, tegra_pll_c_out1.reg);

	/* Since EMC clock is not restored, and may not preserve parent across
	   suspend, update current state, and mark EMC DFS as out of sync */
	p = tegra_clk_emc.parent;
	tegra12_periph_clk_init(&tegra_clk_emc);

	/* Turn Off pll_m if it was OFF before suspend, and emc was not switched
	   to pll_m across suspend; re-init pll_m to sync s/w and h/w states */
	if ((tegra_pll_m.state == OFF) &&
	    (&tegra_pll_m != tegra_clk_emc.parent))
		tegra12_pllm_clk_disable(&tegra_pll_m);
	tegra12_pllm_clk_init(&tegra_pll_m);

	if (p != tegra_clk_emc.parent) {
		pr_debug("EMC parent(refcount) across suspend: %s(%d) : %s(%d)",
			p->name, p->refcnt, tegra_clk_emc.parent->name,
			tegra_clk_emc.parent->refcnt);

		/* emc switched to the new parent by low level code, but ref
		   count and s/w state need to be updated */
		clk_disable(p);
		clk_enable(tegra_clk_emc.parent);
	}

	rate = clk_get_rate_all_locked(&tegra_clk_emc);
	if (*ctx != rate) {
		tegra_dvfs_set_rate(&tegra_clk_emc, rate);
		if (p == tegra_clk_emc.parent) {
			rate = clk_get_rate_all_locked(p);
			tegra_dvfs_set_rate(p, rate);
		}
	}
	tegra_emc_timing_invalidate();

	tegra12_pll_clk_init(&tegra_pll_u); /* Re-init utmi parameters */
	tegra12_plle_clk_resume(&tegra_pll_e); /* Restore plle parent as pll_re_vco */
	tegra12_pllp_clk_resume(&tegra_pll_p); /* Fire a bug if not restored */
	tegra12_mc_holdoff_enable();
}

static struct syscore_ops tegra_clk_syscore_ops = {
	.suspend = tegra12_clk_suspend,
	.resume = tegra12_clk_resume,
	.save = tegra12_clk_suspend,
	.restore = tegra12_clk_resume,
};
#endif

/* Tegra12 CPU clock and reset control functions */
static void tegra12_wait_cpu_in_reset(u32 cpu)
{
	unsigned int reg;

	do {
		reg = readl(reg_clk_base +
			    TEGRA30_CLK_RST_CONTROLLER_CPU_CMPLX_STATUS);
		cpu_relax();
	} while (!(reg & (1 << cpu)));	/* check CPU been reset or not */

	return;
}

static void tegra12_put_cpu_in_reset(u32 cpu)
{
	writel(CPU_RESET(cpu),
	       reg_clk_base + TEGRA_CLK_RST_CONTROLLER_RST_CPU_CMPLX_SET);
	dmb();
}

static void tegra12_cpu_out_of_reset(u32 cpu)
{
	writel(CPU_RESET(cpu),
	       reg_clk_base + TEGRA_CLK_RST_CONTROLLER_RST_CPU_CMPLX_CLR);
	wmb();
}

static void tegra12_enable_cpu_clock(u32 cpu)
{
	unsigned int reg;

	writel(CPU_CLOCK(cpu),
	       reg_clk_base + TEGRA30_CLK_RST_CONTROLLER_CLK_CPU_CMPLX_CLR);
	reg = readl(reg_clk_base +
		    TEGRA30_CLK_RST_CONTROLLER_CLK_CPU_CMPLX_CLR);
}
static void tegra12_disable_cpu_clock(u32 cpu)
{
}

static struct tegra_cpu_car_ops tegra12_cpu_car_ops = {
	.wait_for_reset	= tegra12_wait_cpu_in_reset,
	.put_in_reset	= tegra12_put_cpu_in_reset,
	.out_of_reset	= tegra12_cpu_out_of_reset,
	.enable_clock	= tegra12_enable_cpu_clock,
	.disable_clock	= tegra12_disable_cpu_clock,
};

void __init tegra12_cpu_car_ops_init(void)
{
	tegra_cpu_car_ops = &tegra12_cpu_car_ops;
}

static void tegra12_init_xusb_clocks(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(tegra_xusb_source_clks); i++)
		tegra12_init_one_clock(&tegra_xusb_source_clks[i]);

	tegra12_init_one_clock(&tegra_xusb_ss_div2);
	tegra12_init_one_clock(&tegra_xusb_hs_src);

	for (i = 0; i < ARRAY_SIZE(tegra_xusb_coupled_clks); i++)
		tegra12_init_one_clock(&tegra_xusb_coupled_clks[i]);
}

void __init tegra12x_init_clocks(void)
{
	int i;
	struct clk *c;

	for (i = 0; i < ARRAY_SIZE(tegra_ptr_clks); i++)
		tegra12_init_one_clock(tegra_ptr_clks[i]);

	/* Fix bug in simulator clock routing */
	if (tegra_platform_is_linsim()) {
		for (i = 0; i < ARRAY_SIZE(tegra_list_clks); i++) {
			if (!strcmp("msenc", tegra_list_clks[i].name)) {
				tegra_list_clks[i].u.periph.clk_num = 60;
				tegra_list_clks[i].reg = 0x170;
				tegra_list_clks[i].flags &= ~MUX8;
			}
		}
	}

	for (i = 0; i < ARRAY_SIZE(tegra_list_clks); i++)
		tegra12_init_one_clock(&tegra_list_clks[i]);

	for (i = 0; i < ARRAY_SIZE(tegra_visp_clks); i++)
		tegra12_init_one_clock(&tegra_visp_clks[i]);

	for (i = 0; i < ARRAY_SIZE(tegra_ptr_camera_mclks); i++)
		tegra12_init_one_clock(tegra_ptr_camera_mclks[i]);

	for (i = 0; i < ARRAY_SIZE(tegra_sync_source_list); i++)
		tegra12_init_one_clock(&tegra_sync_source_list[i]);
	for (i = 0; i < ARRAY_SIZE(tegra_clk_audio_list); i++)
		tegra12_init_one_clock(&tegra_clk_audio_list[i]);
	for (i = 0; i < ARRAY_SIZE(tegra_clk_audio_2x_list); i++)
		tegra12_init_one_clock(&tegra_clk_audio_2x_list[i]);

	init_clk_out_mux();
	for (i = 0; i < ARRAY_SIZE(tegra_clk_out_list); i++)
		tegra12_init_one_clock(&tegra_clk_out_list[i]);

	tegra12_init_xusb_clocks();

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

	/* Initialize to default */
	tegra_init_cpu_edp_limits(0);

	tegra12_cpu_car_ops_init();

#ifdef CONFIG_PM_SLEEP
	register_syscore_ops(&tegra_clk_syscore_ops);
#endif

}

static int __init tegra12x_clk_late_init(void)
{
	clk_disable(&tegra_pll_d);
	clk_disable(&tegra_pll_re_vco);
	return 0;
}
late_initcall(tegra12x_clk_late_init);
