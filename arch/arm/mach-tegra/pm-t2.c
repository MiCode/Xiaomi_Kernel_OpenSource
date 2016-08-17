/*
 * arch/arm/mach-tegra/pm-t2.c
 *
 * Tegra 2 LP0 scratch register preservation
 *
 * Copyright (c) 2009-2012, NVIDIA CORPORATION. All rights reserved.
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
 */

#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/module.h>

#include <mach/iomap.h>
#include <mach/irqs.h>

#include "pm.h"

#define PMC_SCRATCH3	0x5c
#define PMC_SCRATCH5	0x64
#define PMC_SCRATCH6	0x68
#define PMC_SCRATCH7	0x6c
#define PMC_SCRATCH8	0x70
#define PMC_SCRATCH9	0x74
#define PMC_SCRATCH10	0x78
#define PMC_SCRATCH11	0x7c
#define PMC_SCRATCH12	0x80
#define PMC_SCRATCH13	0x84
#define PMC_SCRATCH14	0x88
#define PMC_SCRATCH15	0x8c
#define PMC_SCRATCH16	0x90
#define PMC_SCRATCH17	0x94
#define PMC_SCRATCH18	0x98
#define PMC_SCRATCH19	0x9c
#define PMC_SCRATCH20	0xa0
#define PMC_SCRATCH21	0xa4
#define PMC_SCRATCH22	0xa8
#define PMC_SCRATCH23	0xac
#define PMC_SCRATCH25	0x100
#define PMC_SCRATCH35	0x128
#define PMC_SCRATCH36	0x12c
#define PMC_SCRATCH40	0x13c

struct pmc_scratch_field {
	unsigned long addr;
	unsigned int mask;
	int shift_src;
	int shift_dst;
};

#define field(reg, start, end, dst)					\
	{								\
		.addr = (reg),						\
		.mask = 0xfffffffful >> (31 - ((end) - (start))),	\
		.shift_src = (start),					\
		.shift_dst = (dst),					\
	}

static const struct pmc_scratch_field pllx[] __initdata = {
	field(TEGRA_CLK_RESET_BASE + 0xe0, 20, 22, 15), /* PLLX_DIVP */
	field(TEGRA_CLK_RESET_BASE + 0xe0, 8, 17, 5), /* PLLX_DIVN */
	field(TEGRA_CLK_RESET_BASE + 0xe0, 0, 4, 0), /* PLLX_DIVM */
	field(TEGRA_CLK_RESET_BASE + 0xe4, 8, 11, 22), /* PLLX_CPCON */
	field(TEGRA_CLK_RESET_BASE + 0xe4, 4, 7, 18), /* PLLX_LFCON */
	field(TEGRA_APB_MISC_BASE + 0x8e4, 24, 27, 27), /* XM2CFGC_VREF_DQ */
	field(TEGRA_APB_MISC_BASE + 0x8c8, 3, 3, 26), /* XM2CFGC_SCHMT_EN */
	field(TEGRA_APB_MISC_BASE + 0x8d0, 2, 2, 31), /* XM2CLKCFG_PREEMP_EN */
};

static const struct pmc_scratch_field emc_0[] __initdata = {
	field(TEGRA_EMC_BASE + 0x3c, 0, 4, 27), /* R2W */
	field(TEGRA_EMC_BASE + 0x34, 0, 5, 15), /* RAS */
	field(TEGRA_EMC_BASE + 0x2c, 0, 5, 0), /* RC */
	field(TEGRA_EMC_BASE + 0x30, 0, 8, 6), /* RFC */
	field(TEGRA_EMC_BASE + 0x38, 0, 5, 21), /* RP */
};

static const struct pmc_scratch_field emc_1[] __initdata = {
	field(TEGRA_EMC_BASE + 0x44, 0, 4, 5), /* R2P */
	field(TEGRA_EMC_BASE + 0x4c, 0, 5, 15), /* RD_RCD */
	field(TEGRA_EMC_BASE + 0x54, 0, 3, 27), /* RRD */
	field(TEGRA_EMC_BASE + 0x48, 0, 4, 10), /* W2P */
	field(TEGRA_EMC_BASE + 0x40, 0, 4, 0), /* W2R */
	field(TEGRA_EMC_BASE + 0x50, 0, 5, 21), /* WR_RCD */
};

static const struct pmc_scratch_field emc_2[] __initdata = {
	field(TEGRA_EMC_BASE + 0x2b8, 2, 2, 31), /* CLKCHANGE_SR_ENABLE */
	field(TEGRA_EMC_BASE + 0x2b8, 10, 10, 30), /* USE_ADDR_CLK */
	field(TEGRA_EMC_BASE + 0x80, 0, 4, 25), /* PCHG2PDEN */
	field(TEGRA_EMC_BASE + 0x64, 0, 3, 12), /* QRST */
	field(TEGRA_EMC_BASE + 0x68, 0, 3, 16), /* QSAFE */
	field(TEGRA_EMC_BASE + 0x60, 0, 3, 8), /* QUSE */
	field(TEGRA_EMC_BASE + 0x6c, 0, 4, 20), /* RDV */
	field(TEGRA_EMC_BASE + 0x58, 0, 3, 0), /* REXT */
	field(TEGRA_EMC_BASE + 0x5c, 0, 3, 4), /* WDV */
};

static const struct pmc_scratch_field emc_3[] __initdata = {
	field(TEGRA_EMC_BASE + 0x74, 0, 3, 16), /* BURST_REFRESH_NUM */
	field(TEGRA_EMC_BASE + 0x7c, 0, 3, 24), /* PDEX2RD */
	field(TEGRA_EMC_BASE + 0x78, 0, 3, 20), /* PDEX2WR */
	field(TEGRA_EMC_BASE + 0x70, 0, 4, 0), /* REFRESH_LO */
	field(TEGRA_EMC_BASE + 0x70, 5, 15, 5), /* REFRESH */
	field(TEGRA_EMC_BASE + 0xa0, 0, 3, 28), /* TCLKSTABLE */
};

static const struct pmc_scratch_field emc_4[] __initdata = {
	field(TEGRA_EMC_BASE + 0x84, 0, 4, 0), /* ACT2PDEN */
	field(TEGRA_EMC_BASE + 0x88, 0, 4, 5), /* AR2PDEN */
	field(TEGRA_EMC_BASE + 0x8c, 0, 5, 10), /* RW2PDEN */
	field(TEGRA_EMC_BASE + 0x94, 0, 3, 28), /* TCKE */
	field(TEGRA_EMC_BASE + 0x90, 0, 11, 16), /* TXSR */
};

static const struct pmc_scratch_field emc_5[] __initdata = {
	field(TEGRA_EMC_BASE + 0x8, 10, 10, 30), /* AP_REQ_BUSY_CTRL */
	field(TEGRA_EMC_BASE + 0x8, 24, 24, 31), /* CFG_PRIORITY */
	field(TEGRA_EMC_BASE + 0x8, 2, 2, 26), /* FORCE_UPDATE */
	field(TEGRA_EMC_BASE + 0x8, 4, 4, 27), /* MRS_WAIT */
	field(TEGRA_EMC_BASE + 0x8, 5, 5, 28), /* PERIODIC_QRST */
	field(TEGRA_EMC_BASE + 0x8, 9, 9, 29), /* READ_DQM_CTRL */
	field(TEGRA_EMC_BASE + 0x8, 0, 0, 24), /* READ_MUX */
	field(TEGRA_EMC_BASE + 0x8, 1, 1, 25), /* WRITE_MUX */
	field(TEGRA_EMC_BASE + 0xa4, 0, 3, 6), /* TCLKSTOP */
	field(TEGRA_EMC_BASE + 0xa8, 0, 13, 10), /* TREFBW */
	field(TEGRA_EMC_BASE + 0x9c, 0, 5, 0), /* TRPAB */
};

static const struct pmc_scratch_field emc_6[] __initdata = {
	field(TEGRA_EMC_BASE + 0xfc, 0, 1, 0), /* DQSIB_DLY_MSB_BYTE_0 */
	field(TEGRA_EMC_BASE + 0xfc, 8, 9, 2), /* DQSIB_DLY_MSB_BYTE_1 */
	field(TEGRA_EMC_BASE + 0xfc, 16, 17, 4), /* DQSIB_DLY_MSB_BYTE_2 */
	field(TEGRA_EMC_BASE + 0xfc, 24, 25, 6), /* DQSIB_DLY_MSB_BYTE_3 */
	field(TEGRA_EMC_BASE + 0x110, 0, 1, 8), /* QUSE_DLY_MSB_BYTE_0 */
	field(TEGRA_EMC_BASE + 0x110, 8, 9, 10), /* QUSE_DLY_MSB_BYTE_1 */
	field(TEGRA_EMC_BASE + 0x110, 16, 17, 12), /* QUSE_DLY_MSB_BYTE_2 */
	field(TEGRA_EMC_BASE + 0x110, 24, 25, 14), /* QUSE_DLY_MSB_BYTE_3 */
	field(TEGRA_EMC_BASE + 0xac, 0, 3, 22), /* QUSE_EXTRA */
	field(TEGRA_EMC_BASE + 0x98, 0, 5, 16), /* TFAW */
	field(TEGRA_APB_MISC_BASE + 0x8e4, 5, 5, 30), /* XM2CFGC_VREF_DQ_EN */
	field(TEGRA_APB_MISC_BASE + 0x8e4, 16, 19, 26), /* XM2CFGC_VREF_DQS */
};

static const struct pmc_scratch_field emc_dqsib_dly[] __initdata = {
	field(TEGRA_EMC_BASE + 0xf8, 0, 31, 0), /* DQSIB_DLY_BYTE_0 - DQSIB_DLY_BYTE_3*/
};

static const struct pmc_scratch_field emc_quse_dly[] __initdata = {
	field(TEGRA_EMC_BASE + 0x10c, 0, 31, 0), /* QUSE_DLY_BYTE_0 - QUSE_DLY_BYTE_3*/
};

static const struct pmc_scratch_field emc_clktrim[] __initdata = {
	field(TEGRA_EMC_BASE + 0x2d0, 0, 29, 0), /* DATA0_CLKTRIM - DATA3_CLKTRIM +
					* MCLK_ADDR_CLKTRIM */
};

static const struct pmc_scratch_field emc_autocal_fbio[] __initdata = {
	field(TEGRA_EMC_BASE + 0x2a4, 29, 29, 29), /* AUTO_CAL_ENABLE */
	field(TEGRA_EMC_BASE + 0x2a4, 30, 30, 30), /* AUTO_CAL_OVERRIDE */
	field(TEGRA_EMC_BASE + 0x2a4, 8, 12, 14), /* AUTO_CAL_PD_OFFSET */
	field(TEGRA_EMC_BASE + 0x2a4, 0, 4, 9), /* AUTO_CAL_PU_OFFSET */
	field(TEGRA_EMC_BASE + 0x2a4, 16, 25, 19), /* AUTO_CAL_STEP */
	field(TEGRA_EMC_BASE + 0xf4, 16, 16, 0), /* CFG_DEN_EARLY */
	field(TEGRA_EMC_BASE + 0x104, 8, 8, 8), /* CTT_TERMINATION */
	field(TEGRA_EMC_BASE + 0x104, 7, 7, 7), /* DIFFERENTIAL_DQS */
	field(TEGRA_EMC_BASE + 0x104, 9, 9, 31), /* DQS_PULLD */
	field(TEGRA_EMC_BASE + 0x104, 0, 1, 4), /* DRAM_TYPE */
	field(TEGRA_EMC_BASE + 0x104, 4, 4, 6), /* DRAM_WIDTH */
	field(TEGRA_EMC_BASE + 0x114, 0, 2, 1), /* CFG_QUSE_LATE */
};

static const struct pmc_scratch_field emc_autocal_interval[] __initdata = {
	field(TEGRA_EMC_BASE + 0x2a8, 0, 27, 0), /* AUTOCAL_INTERVAL */
	field(TEGRA_EMC_BASE + 0x2b8, 1, 1, 29), /* CLKCHANGE_PD_ENABLE */
	field(TEGRA_EMC_BASE + 0x2b8, 0, 0, 28), /* CLKCHANGE_REQ_ENABLE */
	field(TEGRA_EMC_BASE + 0x2b8, 8, 9, 30), /* PIN_CONFIG */
};

static const struct pmc_scratch_field emc_cfgs[] __initdata = {
	field(TEGRA_EMC_BASE + 0x10, 8, 9, 3), /* EMEM_BANKWIDTH */
	field(TEGRA_EMC_BASE + 0x10, 0, 2, 0), /* EMEM_COLWIDTH */
	field(TEGRA_EMC_BASE + 0x10, 16, 19, 5), /* EMEM_DEVSIZE */
	field(TEGRA_EMC_BASE + 0x10, 24, 25, 9), /* EMEM_NUMDEV */
	field(TEGRA_EMC_BASE + 0xc, 24, 24, 21), /* AUTO_PRE_RD */
	field(TEGRA_EMC_BASE + 0xc, 25, 25, 22), /* AUTO_PRE_WR */
	field(TEGRA_EMC_BASE + 0xc, 16, 16, 20), /* CLEAR_AP_PREV_SPREQ */
	field(TEGRA_EMC_BASE + 0xc, 29, 29, 23), /* DRAM_ACPD */
	field(TEGRA_EMC_BASE + 0xc, 30, 30, 24), /* DRAM_CLKSTOP_PDSR_ONLY */
	field(TEGRA_EMC_BASE + 0xc, 31, 31, 25), /* DRAM_CLKSTOP */
	field(TEGRA_EMC_BASE + 0xc, 8, 15, 12), /* PRE_IDLE_CYCLES */
	field(TEGRA_EMC_BASE + 0xc, 0, 0, 11), /* PRE_IDLE_EN */
	field(TEGRA_EMC_BASE + 0x2bc, 28, 29, 28), /* CFG_DLL_LOCK_LIMIT */
	field(TEGRA_EMC_BASE + 0x2bc, 6, 7, 30), /* CFG_DLL_MODE */
	field(TEGRA_MC_BASE + 0x10c, 0, 0, 26), /* LL_CTRL */
	field(TEGRA_MC_BASE + 0x10c, 1, 1, 27), /* LL_SEND_BOTH */
};

static const struct pmc_scratch_field emc_adr_cfg1[] __initdata = {
	field(TEGRA_EMC_BASE + 0x14, 8, 9, 8), /* EMEM1_BANKWIDTH */
	field(TEGRA_EMC_BASE + 0x14, 0, 2, 5), /* EMEM1_COLWIDTH */
	field(TEGRA_EMC_BASE + 0x14, 16, 19, 10), /* EMEM1_DEVSIZE */
	field(TEGRA_EMC_BASE + 0x2dc, 24, 28, 0), /* TERM_DRVUP */
	field(TEGRA_APB_MISC_BASE + 0x8d4, 0, 3, 14), /* XM2COMP_VREF_SEL */
	field(TEGRA_APB_MISC_BASE + 0x8d8, 16, 18, 21), /* XM2VTTGEN_CAL_DRVDN */
	field(TEGRA_APB_MISC_BASE + 0x8d8, 24, 26, 18), /* XM2VTTGEN_CAL_DRVUP */
	field(TEGRA_APB_MISC_BASE + 0x8d8, 1, 1, 30), /* XM2VTTGEN_SHORT_PWRGND */
	field(TEGRA_APB_MISC_BASE + 0x8d8, 0, 0, 31), /* XM2VTTGEN_SHORT */
	field(TEGRA_APB_MISC_BASE + 0x8d8, 12, 14, 24), /* XM2VTTGEN_VAUXP_LEVEL */
	field(TEGRA_APB_MISC_BASE + 0x8d8, 8, 10, 27), /* XM2VTTGEN_VCLAMP_LEVEL */
};

static const struct pmc_scratch_field emc_digital_dll[] __initdata = {
	field(TEGRA_EMC_BASE + 0x2bc, 1, 1, 23), /* DLI_TRIMMER_EN */
	field(TEGRA_EMC_BASE + 0x2bc, 0, 0, 22), /* DLL_EN */
	field(TEGRA_EMC_BASE + 0x2bc, 5, 5, 27), /* DLL_LOWSPEED */
	field(TEGRA_EMC_BASE + 0x2bc, 2, 2, 24), /* DLL_OVERRIDE_EN */
	field(TEGRA_EMC_BASE + 0x2bc, 8, 11, 28), /* DLL_UDSET */
	field(TEGRA_EMC_BASE + 0x2bc, 4, 4, 26), /* PERBYTE_TRIMMER_OVERRIDE */
	field(TEGRA_EMC_BASE + 0x2bc, 3, 3, 25), /* USE_SINGLE_DLL */
	field(TEGRA_MC_BASE + 0xc, 0, 21, 0), /* EMEM_SIZE_KB */
};

static const struct pmc_scratch_field emc_dqs_clktrim[] __initdata = {
	field(TEGRA_EMC_BASE + 0x2d4, 0, 29, 0), /* DQS0_CLKTRIM - DQS3 + MCLK*/
	field(TEGRA_APB_MISC_BASE + 0x8e4, 3, 3, 31), /* XM2CFGC_CTT_HIZ_EN */
	field(TEGRA_APB_MISC_BASE + 0x8e4, 4, 4, 30), /* XM2CFGC_VREF_DQS_EN */
};

static const struct pmc_scratch_field emc_dq_clktrim[] __initdata = {
	field(TEGRA_EMC_BASE + 0x2d8, 0, 29, 0),
	field(TEGRA_APB_MISC_BASE + 0x8e4, 2, 2, 30), /* XM2CFGC_PREEMP_EN */
	field(TEGRA_APB_MISC_BASE + 0x8e4, 0, 0, 31), /* XM2CFGC_RX_FT_REC_EN */
};

static const struct pmc_scratch_field emc_dll_xform_dqs[] __initdata = {
	field(TEGRA_EMC_BASE + 0x2bc, 16, 25, 20), /* CFG_DLL_OVERRIDE_VAL */
	field(TEGRA_EMC_BASE + 0x2c0, 0, 4, 0), /* DQS_MULT */
	field(TEGRA_EMC_BASE + 0x2c0, 8, 22, 5), /* DQS_OFFS */
	field(TEGRA_MC_BASE + 0x10c, 31, 31, 30), /* LL_DRAM_INTERLEAVE */
};

static const struct pmc_scratch_field emc_odt_rw[] __initdata = {
	field(TEGRA_EMC_BASE + 0x2c4, 0, 4, 0), /* QUSE_MULT */
	field(TEGRA_EMC_BASE + 0x2c4, 8, 22, 5), /* QUSE_OFF */
	field(TEGRA_EMC_BASE + 0xb4, 31, 31, 29), /* DISABLE_ODT_DURING_READ */
	field(TEGRA_EMC_BASE + 0xb4, 30, 30, 28), /* B4_READ */
	field(TEGRA_EMC_BASE + 0xb4, 0, 2, 25), /* RD_DELAY */
	field(TEGRA_EMC_BASE + 0xb0, 31, 31, 24), /* ENABLE_ODT_DURING_WRITE */
	field(TEGRA_EMC_BASE + 0xb0, 30, 30, 23), /* B4_WRITE */
	field(TEGRA_EMC_BASE + 0xb0, 0, 2, 20), /* WR_DELAY */
};

static const struct pmc_scratch_field arbitration_xbar[] __initdata = {
	field(TEGRA_AHB_GIZMO_BASE + 0xdc, 0, 31, 0),
};

static const struct pmc_scratch_field emc_zcal[] __initdata = {
	field(TEGRA_EMC_BASE + 0x2e0, 0, 23, 0), /* ZCAL_REF_INTERVAL */
	field(TEGRA_EMC_BASE + 0x2e4, 0, 7, 24), /* ZCAL_WAIT_CNT */
};

static const struct pmc_scratch_field emc_ctt_term[] __initdata = {
	field(TEGRA_EMC_BASE + 0x2dc, 15, 19, 26), /* TERM_DRVDN */
	field(TEGRA_EMC_BASE + 0x2dc, 8, 12, 21), /* TERM_OFFSET */
	field(TEGRA_EMC_BASE + 0x2dc, 31, 31, 31), /* TERM_OVERRIDE */
	field(TEGRA_EMC_BASE + 0x2dc, 0, 2, 18), /* TERM_SLOPE */
	field(TEGRA_EMC_BASE + 0x2e8, 16, 23, 8), /* ZQ_MRW_MA */
	field(TEGRA_EMC_BASE + 0x2e8, 0, 7, 0), /* ZQ_MRW_OP */
};

static const struct pmc_scratch_field xm2_cfgd[] __initdata = {
	field(TEGRA_APB_MISC_BASE + 0x8e8, 16, 18, 9), /* CFGD0_DLYIN_TRM */
	field(TEGRA_APB_MISC_BASE + 0x8e8, 20, 22, 6), /* CFGD1_DLYIN_TRM */
	field(TEGRA_APB_MISC_BASE + 0x8e8, 24, 26, 3), /* CFGD2_DLYIN_TRM */
	field(TEGRA_APB_MISC_BASE + 0x8e8, 28, 30, 0), /* CFGD3_DLYIN_TRM */
	field(TEGRA_APB_MISC_BASE + 0x8e8, 3, 3, 12), /* XM2CFGD_CTT_HIZ_EN */
	field(TEGRA_APB_MISC_BASE + 0x8e8, 2, 2, 13), /* XM2CFGD_PREEMP_EN */
	field(TEGRA_APB_MISC_BASE + 0x8e8, 0, 0, 14), /* CM2CFGD_RX_FT_REC_EN */
};

struct pmc_scratch_reg {
	const struct pmc_scratch_field *fields;
	void __iomem *scratch_addr;
	int num_fields;
};

#define scratch(offs, field_list)					\
	{								\
		.scratch_addr = IO_ADDRESS(TEGRA_PMC_BASE) + offs,	\
		.fields = field_list,					\
		.num_fields = ARRAY_SIZE(field_list),			\
	}

static const struct pmc_scratch_reg scratch[] __initdata = {
	scratch(PMC_SCRATCH3, pllx),
	scratch(PMC_SCRATCH5, emc_0),
	scratch(PMC_SCRATCH6, emc_1),
	scratch(PMC_SCRATCH7, emc_2),
	scratch(PMC_SCRATCH8, emc_3),
	scratch(PMC_SCRATCH9, emc_4),
	scratch(PMC_SCRATCH10, emc_5),
	scratch(PMC_SCRATCH11, emc_6),
	scratch(PMC_SCRATCH12, emc_dqsib_dly),
	scratch(PMC_SCRATCH13, emc_quse_dly),
	scratch(PMC_SCRATCH14, emc_clktrim),
	scratch(PMC_SCRATCH15, emc_autocal_fbio),
	scratch(PMC_SCRATCH16, emc_autocal_interval),
	scratch(PMC_SCRATCH17, emc_cfgs),
	scratch(PMC_SCRATCH18, emc_adr_cfg1),
	scratch(PMC_SCRATCH19, emc_digital_dll),
	scratch(PMC_SCRATCH20, emc_dqs_clktrim),
	scratch(PMC_SCRATCH21, emc_dq_clktrim),
	scratch(PMC_SCRATCH22, emc_dll_xform_dqs),
	scratch(PMC_SCRATCH23, emc_odt_rw),
	scratch(PMC_SCRATCH25, arbitration_xbar),
	scratch(PMC_SCRATCH35, emc_zcal),
	scratch(PMC_SCRATCH36, emc_ctt_term),
	scratch(PMC_SCRATCH40, xm2_cfgd),
};

void __init tegra2_lp0_suspend_init(void)
{
	int i;
	int j;
	unsigned int v;
	unsigned int r;

	for (i = 0; i < ARRAY_SIZE(scratch); i++) {
		r = 0;

		for (j = 0; j < scratch[i].num_fields; j++) {
			v = readl(IO_ADDRESS(scratch[i].fields[j].addr));
			v >>= scratch[i].fields[j].shift_src;
			v &= scratch[i].fields[j].mask;
			v <<= scratch[i].fields[j].shift_dst;
			r |= v;
		}

		__raw_writel(r, scratch[i].scratch_addr);
	}
	wmb();
}

#ifdef CONFIG_PM_SLEEP

struct tegra_io_dpd *tegra_io_dpd_get(struct device *dev)
{
	return NULL;
}
EXPORT_SYMBOL(tegra_io_dpd_get);

void tegra_io_dpd_enable(struct tegra_io_dpd *hnd)
{
	return;
}
EXPORT_SYMBOL(tegra_io_dpd_enable);

void tegra_io_dpd_disable(struct tegra_io_dpd *hnd)
{
	return;
}
EXPORT_SYMBOL(tegra_io_dpd_disable);

#endif

int tegra_io_dpd_init(void)
{
	return 0;
}
EXPORT_SYMBOL(tegra_io_dpd_init);

void tegra_bl_io_dpd_cleanup()
{
}
EXPORT_SYMBOL(tegra_bl_io_dpd_cleanup);

