/* Copyright (c) 2009-2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/ctype.h>
#include <linux/bitops.h>
#include <linux/io.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/string.h>

#include <mach/msm_iomap.h>
#include <mach/clk.h>

#include "clock.h"
#include "clock-local.h"
#include "clock-pcom.h"
#include "clock-voter.h"
#include "proc_comm.h"

#define REG_BASE(off) (MSM_CLK_CTL_BASE + (off))
#define REG(off) (MSM_CLK_CTL_SH2_BASE + (off))

/* Shadow-region 2 (SH2) registers. */
#define	QUP_I2C_NS_REG		REG(0x04F0)
#define CAM_NS_REG		REG(0x0374)
#define CAM_VFE_NS_REG		REG(0x0044)
#define CLK_HALT_STATEA_REG	REG(0x0108)
#define CLK_HALT_STATEB_REG	REG(0x010C)
#define CLK_HALT_STATEC_REG	REG(0x02D4)
#define CSI_NS_REG		REG(0x0174)
#define EMDH_NS_REG		REG(0x0050)
#define GLBL_CLK_ENA_2_SC_REG	REG(0x03C0)
#define GLBL_CLK_ENA_SC_REG	REG(0x03BC)
#define GLBL_CLK_STATE_2_REG	REG(0x037C)
#define GLBL_CLK_STATE_REG	REG(0x0004)
#define GRP_2D_NS_REG		REG(0x0034)
#define GRP_NS_REG		REG(0x0084)
#define HDMI_NS_REG		REG(0x0484)
#define I2C_2_NS_REG		REG(0x02D8)
#define I2C_NS_REG		REG(0x0068)
#define JPEG_NS_REG		REG(0x0164)
#define LPA_CORE_CLK_MA0_REG	REG(0x04F4)
#define LPA_CORE_CLK_MA2_REG	REG(0x04FC)
#define LPA_NS_REG		REG(0x02E8)
#define MDC_NS_REG		REG(0x007C)
#define MDP_LCDC_NS_REG		REG(0x0390)
#define MDP_NS_REG		REG(0x014C)
#define MDP_VSYNC_REG		REG(0x0460)
#define MFC_NS_REG		REG(0x0154)
#define MI2S_CODEC_RX_DIV_REG	REG(0x02EC)
#define MI2S_CODEC_TX_DIV_REG	REG(0x02F0)
#define MI2S_DIV_REG		REG(0x02E4)
#define MI2S_NS_REG		REG(0x02E0)
#define MI2S_RX_NS_REG		REG(0x0070)
#define MI2S_TX_NS_REG		REG(0x0078)
#define MIDI_NS_REG		REG(0x02D0)
#define PLL_ENA_REG		REG(0x0264)
#define PMDH_NS_REG		REG(0x008C)
#define SDAC_NS_REG		REG(0x009C)
#define SDCn_NS_REG(n)		REG(0x00A4+(0x8*((n)-1)))
#define SPI_NS_REG		REG(0x02C8)
#define TSIF_NS_REG		REG(0x00C4)
#define TV_NS_REG		REG(0x00CC)
#define UART1DM_NS_REG		REG(0x00D4)
#define UART2DM_NS_REG		REG(0x00DC)
#define UART2_NS_REG		REG(0x0464)
#define UART_NS_REG		REG(0x00E0)
#define USBH2_NS_REG		REG(0x046C)
#define USBH3_NS_REG		REG(0x0470)
#define USBH_MD_REG		REG(0x02BC)
#define USBH_NS_REG		REG(0x02C0)
#define VPE_NS_REG		REG(0x015C)

/* Registers in the base (non-shadow) region. */
#define CLK_TEST_BASE_REG	REG_BASE(0x011C)
#define CLK_TEST_2_BASE_REG	REG_BASE(0x0384)
#define MISC_CLK_CTL_BASE_REG	REG_BASE(0x0110)
#define PRPH_WEB_NS_BASE_REG	REG_BASE(0x0080)
#define PLL0_STATUS_BASE_REG	REG_BASE(0x0318)
#define PLL1_STATUS_BASE_REG	REG_BASE(0x0334)
#define PLL2_STATUS_BASE_REG	REG_BASE(0x0350)
#define PLL3_STATUS_BASE_REG	REG_BASE(0x036C)
#define PLL4_STATUS_BASE_REG	REG_BASE(0x0254)
#define PLL5_STATUS_BASE_REG	REG_BASE(0x0258)
#define PLL6_STATUS_BASE_REG	REG_BASE(0x04EC)
#define RINGOSC_CNT_BASE_REG	REG_BASE(0x00FC)
#define SH2_OWN_APPS1_BASE_REG	REG_BASE(0x040C)
#define SH2_OWN_APPS2_BASE_REG	REG_BASE(0x0414)
#define SH2_OWN_APPS3_BASE_REG	REG_BASE(0x0444)
#define SH2_OWN_GLBL_BASE_REG	REG_BASE(0x0404)
#define SH2_OWN_ROW1_BASE_REG	REG_BASE(0x041C)
#define SH2_OWN_ROW2_BASE_REG	REG_BASE(0x0424)
#define TCXO_CNT_BASE_REG	REG_BASE(0x00F8)
#define TCXO_CNT_DONE_BASE_REG	REG_BASE(0x00F8)


/* MUX source input identifiers. */
#define SRC_SEL_pll0		4 /* Modem PLL */
#define SRC_SEL_pll1		1 /* Global PLL */
#define SRC_SEL_pll3		3 /* Multimedia/Peripheral PLL or Backup PLL1 */
#define SRC_SEL_pll4		2 /* Display PLL */
#define SRC_SEL_SDAC_lpxo	5 /* Low-power XO for SDAC */
#define SRC_SEL_lpxo		6 /* Low-power XO */
#define SRC_SEL_tcxo		0 /* Used for rates from TCXO */
#define SRC_SEL_axi		0 /* Used for rates that sync to AXI */
#define SRC_SEL_gnd		7 /* No clock */

/* Clock declaration macros. */
#define MN_MODE_DUAL_EDGE	0x2
#define MD8(m, n)		(BVAL(15, 8, m) | BVAL(7, 0, ~(n)))
#define N8(msb, lsb, m, n)	(BVAL(msb, lsb, ~(n-m)) | BVAL(6, 5, \
					(MN_MODE_DUAL_EDGE * !!(n))))
#define MD16(m, n)		(BVAL(31, 16, m) | BVAL(15, 0, ~(n)))
#define N16(m, n)		(BVAL(31, 16, ~(n-m)) | BVAL(6, 5, \
					(MN_MODE_DUAL_EDGE * !!(n))))
#define SPDIV(s, d)		(BVAL(4, 3, d-1) | BVAL(2, 0, s))
#define SDIV(s, d)		(BVAL(6, 3, d-1) | BVAL(2, 0, s))
#define F_MASK_BASIC		(BM(6, 3)|BM(2, 0))
#define F_MASK_MND16		(BM(31, 16)|BM(6, 5)|BM(4, 3)|BM(2, 0))
#define F_MASK_MND8(m, l)	(BM(m, l)|BM(6, 5)|BM(4, 3)|BM(2, 0))

/*
 * Clock frequency definitions and macros
 */
#define F_BASIC(f, s, div, v) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
		.ns_val = SDIV(SRC_SEL_##s, div), \
		.sys_vdd = v, \
	}

#define F_MND16(f, s, div, m, n, v) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
		.md_val = MD16(m, n), \
		.ns_val = N16(m, n) | SPDIV(SRC_SEL_##s, div), \
		.mnd_en_mask = BIT(8) * !!(n), \
		.sys_vdd = v, \
	}

#define F_MND8(f, nmsb, nlsb, s, div, m, n, v) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
		.md_val = MD8(m, n), \
		.ns_val = N8(nmsb, nlsb, m, n) | SPDIV(SRC_SEL_##s, div), \
		.mnd_en_mask = BIT(8) * !!(n), \
		.sys_vdd = v, \
	}

static struct clk_ops soc_clk_ops_7x30;

#define PCOM_XO_DISABLE	0
#define PCOM_XO_ENABLE	1
#define PCOM_XO_TCXO	0
#define PCOM_XO_LPXO	1

static bool pcom_is_local(struct clk *clk)
{
	return false;
}

static int pcom_xo_enable(unsigned pcom_id, unsigned enable)
{
	/* TODO: Check return code in pcom_id */
	return msm_proc_comm(PCOM_CLKCTL_RPC_SRC_REQUEST, &pcom_id, &enable);
}

static int tcxo_clk_enable(struct clk *clk)
{
	return pcom_xo_enable(PCOM_XO_TCXO, PCOM_XO_ENABLE);
}

static void tcxo_clk_disable(struct clk *clk)
{
	pcom_xo_enable(PCOM_XO_TCXO, PCOM_XO_DISABLE);
}

static struct clk_ops clk_ops_tcxo = {
	.enable = tcxo_clk_enable,
	.disable = tcxo_clk_disable,
	.get_rate = fixed_clk_get_rate,
	.is_local = pcom_is_local,
};

static struct fixed_clk tcxo_clk = {
	.rate = 19200000,
	.c = {
		.dbg_name = "tcxo_clk",
		.ops = &clk_ops_tcxo,
		CLK_INIT(tcxo_clk.c),
	},
};

static int lpxo_clk_enable(struct clk *clk)
{
	return pcom_xo_enable(PCOM_XO_LPXO, PCOM_XO_ENABLE);
}

static void lpxo_clk_disable(struct clk *clk)
{
	pcom_xo_enable(PCOM_XO_LPXO, PCOM_XO_DISABLE);
}

static struct clk_ops clk_ops_lpxo = {
	.enable = lpxo_clk_enable,
	.disable = lpxo_clk_disable,
	.get_rate = fixed_clk_get_rate,
	.is_local = pcom_is_local,
};

static struct fixed_clk lpxo_clk = {
	.rate = 24576000,
	.c = {
		.dbg_name = "lpxo_clk",
		.ops = &clk_ops_lpxo,
		CLK_INIT(lpxo_clk.c),
	},
};

static struct pll_vote_clk pll1_clk = {
	.rate = 768000000,
	.en_reg = PLL_ENA_REG,
	.en_mask = BIT(1),
	.status_reg = PLL1_STATUS_BASE_REG,
	.parent = &tcxo_clk.c,
	.c = {
		.dbg_name = "pll1_clk",
		.ops = &clk_ops_pll_vote,
		CLK_INIT(pll1_clk.c),
	},
};

static struct pll_vote_clk pll2_clk = {
	.rate = 806400000, /* TODO: Support scaling */
	.en_reg = PLL_ENA_REG,
	.en_mask = BIT(2),
	.status_reg = PLL2_STATUS_BASE_REG,
	.parent = &tcxo_clk.c,
	.c = {
		.dbg_name = "pll2_clk",
		.ops = &clk_ops_pll_vote,
		CLK_INIT(pll2_clk.c),
	},
};

static struct pll_vote_clk pll3_clk = {
	.rate = 737280000,
	.en_reg = PLL_ENA_REG,
	.en_mask = BIT(3),
	.status_reg = PLL3_STATUS_BASE_REG,
	.parent = &lpxo_clk.c,
	.c = {
		.dbg_name = "pll3_clk",
		.ops = &clk_ops_pll_vote,
		CLK_INIT(pll3_clk.c),
	},
};

static struct pll_vote_clk pll4_clk = {
	.rate = 891000000,
	.en_reg = PLL_ENA_REG,
	.en_mask = BIT(4),
	.status_reg = PLL4_STATUS_BASE_REG,
	.parent = &lpxo_clk.c,
	.c = {
		.dbg_name = "pll4_clk",
		.ops = &clk_ops_pll_vote,
		CLK_INIT(pll4_clk.c),
	},
};

static struct clk_ops clk_ops_branch;

static struct clk_freq_tbl clk_tbl_axi[] = {
	F_RAW(1, &lpxo_clk.c, 0, 0, 0, 0, NOMINAL, NULL),
	F_END,
};

/* For global clocks to be on we must have GLBL_ROOT_ENA set */
static struct rcg_clk glbl_root_clk = {
	.b = {
		.ctl_reg = GLBL_CLK_ENA_SC_REG,
		.en_mask = BIT(29),
		.halt_check = NOCHECK,
	},
	.freq_tbl = clk_tbl_axi,
	.set_rate = set_rate_nop,
	.current_freq = &local_dummy_freq,
	.set_rate = set_rate_nop,
	.c = {
		.dbg_name = "glbl_root_clk",
		.ops = &soc_clk_ops_7x30,
		CLK_INIT(glbl_root_clk.c),
	},
};

/* AXI bridge clocks. */
static struct branch_clk axi_li_apps_clk = {
	.b = {
		.ctl_reg = GLBL_CLK_ENA_SC_REG,
		.en_mask = BIT(2),
		.halt_reg = GLBL_CLK_STATE_REG,
		.halt_check = HALT_VOTED,
		.halt_bit = 2,
	},
	.parent = &glbl_root_clk.c,
	.c = {
		.dbg_name = "axi_li_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(axi_li_apps_clk.c),
	},
};

static struct branch_clk axi_li_adsp_a_clk = {
	.b = {
		.ctl_reg = GLBL_CLK_ENA_2_SC_REG,
		.en_mask = BIT(14),
		.halt_reg = GLBL_CLK_STATE_2_REG,
		.halt_check = HALT_VOTED,
		.halt_bit = 14,
	},
	.parent = &axi_li_apps_clk.c,
	.c = {
		.dbg_name = "axi_li_adsp_a_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(axi_li_adsp_a_clk.c),
	},
};

static struct branch_clk axi_li_jpeg_clk = {
	.b = {
		.ctl_reg = GLBL_CLK_ENA_2_SC_REG,
		.en_mask = BIT(19),
		.halt_reg = GLBL_CLK_STATE_2_REG,
		.halt_check = HALT_VOTED,
		.halt_bit = 19,
	},
	.parent = &axi_li_apps_clk.c,
	.c = {
		.dbg_name = "axi_li_jpeg_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(axi_li_jpeg_clk.c),
	},
};

static struct branch_clk axi_li_vfe_clk = {
	.b = {
		.ctl_reg = GLBL_CLK_ENA_SC_REG,
		.en_mask = BIT(23),
		.halt_reg = GLBL_CLK_STATE_REG,
		.halt_check = HALT_VOTED,
		.halt_bit = 23,
	},
	.parent = &axi_li_apps_clk.c,
	.c = {
		.dbg_name = "axi_li_vfe_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(axi_li_vfe_clk.c),
	},
};

static struct branch_clk axi_mdp_clk = {
	.b = {
		.ctl_reg = GLBL_CLK_ENA_2_SC_REG,
		.en_mask = BIT(29),
		.halt_reg = GLBL_CLK_STATE_2_REG,
		.halt_check = HALT_VOTED,
		.halt_bit = 29,
	},
	.parent = &axi_li_apps_clk.c,
	.c = {
		.dbg_name = "axi_mdp_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(axi_mdp_clk.c),
	},
};

static struct branch_clk axi_li_vg_clk = {
	.b = {
		.ctl_reg = GLBL_CLK_ENA_SC_REG,
		.en_mask = BIT(3),
		.halt_reg = GLBL_CLK_STATE_REG,
		.halt_check = HALT_VOTED,
		.halt_bit = 3,
	},
	.parent = &glbl_root_clk.c,
	.c = {
		.dbg_name = "axi_li_vg_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(axi_li_vg_clk.c),
	},
};

static struct branch_clk axi_grp_2d_clk = {
	.b = {
		.ctl_reg = GLBL_CLK_ENA_SC_REG,
		.en_mask = BIT(21),
		.halt_reg = GLBL_CLK_STATE_REG,
		.halt_check = HALT_VOTED,
		.halt_bit = 21,
	},
	.parent = &axi_li_vg_clk.c,
	.c = {
		.dbg_name = "axi_grp_2d_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(axi_grp_2d_clk.c),
	},
};

static struct branch_clk axi_li_grp_clk = {
	.b = {
		.ctl_reg = GLBL_CLK_ENA_SC_REG,
		.en_mask = BIT(22),
		.halt_reg = GLBL_CLK_STATE_REG,
		.halt_check = HALT_VOTED,
		.halt_bit = 22,
	},
	.parent = &axi_li_vg_clk.c,
	.c = {
		.dbg_name = "axi_li_grp_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(axi_li_grp_clk.c),
	},
};

static struct branch_clk axi_mfc_clk = {
	.b = {
		.ctl_reg = GLBL_CLK_ENA_2_SC_REG,
		.en_mask = BIT(20),
		.halt_reg = GLBL_CLK_STATE_2_REG,
		.halt_check = HALT_VOTED,
		.halt_bit = 20,
	},
	.parent = &axi_li_vg_clk.c,
	.c = {
		.dbg_name = "axi_mfc_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(axi_mfc_clk.c),
	},
};

static struct branch_clk axi_rotator_clk = {
	.b = {
		.ctl_reg = GLBL_CLK_ENA_2_SC_REG,
		.en_mask = BIT(22),
		.halt_reg = GLBL_CLK_STATE_2_REG,
		.halt_check = HALT_VOTED,
		.halt_bit = 22,
		.reset_mask = P_AXI_ROTATOR_CLK,
	},
	.parent = &axi_li_vg_clk.c,
	.c = {
		.dbg_name = "axi_rotator_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(axi_rotator_clk.c),
	},
};

static struct branch_clk axi_vpe_clk = {
	.b = {
		.ctl_reg = GLBL_CLK_ENA_2_SC_REG,
		.en_mask = BIT(21),
		.halt_reg = GLBL_CLK_STATE_2_REG,
		.halt_check = HALT_VOTED,
		.halt_bit = 21,
	},
	.parent = &axi_li_vg_clk.c,
	.c = {
		.dbg_name = "axi_vpe_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(axi_vpe_clk.c),
	},
};

/* Peripheral bus clocks. */
static struct branch_clk adm_clk = {
	.b = {
		.ctl_reg = GLBL_CLK_ENA_SC_REG,
		.en_mask = BIT(5),
		.halt_reg = GLBL_CLK_STATE_REG,
		.halt_check = HALT_VOTED,
		.halt_bit = 5,
		.reset_mask = P_ADM_CLK,
	},
	.parent = &axi_li_apps_clk.c,
	.c = {
		.dbg_name = "adm_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(adm_clk.c),
	},
};

static struct branch_clk adm_p_clk = {
	.b = {
		.ctl_reg = GLBL_CLK_ENA_2_SC_REG,
		.en_mask = BIT(15),
		.halt_reg = GLBL_CLK_STATE_2_REG,
		.halt_check = HALT_VOTED,
		.halt_bit = 15,
	},
	.parent = &glbl_root_clk.c,
	.c = {
		.dbg_name = "adm_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(adm_p_clk.c),
	},
};

static struct branch_clk ce_clk = {
	.b = {
		.ctl_reg = GLBL_CLK_ENA_SC_REG,
		.en_mask = BIT(6),
		.halt_reg = GLBL_CLK_STATE_REG,
		.halt_check = HALT_VOTED,
		.halt_bit = 6,
		.reset_mask = P_CE_CLK,
	},
	.parent = &glbl_root_clk.c,
	.c = {
		.dbg_name = "ce_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(ce_clk.c),
	},
};

static struct branch_clk camif_pad_p_clk = {
	.b = {
		.ctl_reg = GLBL_CLK_ENA_SC_REG,
		.en_mask = BIT(9),
		.halt_reg = GLBL_CLK_STATE_REG,
		.halt_check = HALT_VOTED,
		.halt_bit = 9,
		.reset_mask = P_CAMIF_PAD_P_CLK,
	},
	.parent = &glbl_root_clk.c,
	.c = {
		.dbg_name = "camif_pad_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camif_pad_p_clk.c),
	},
};

static struct branch_clk csi0_p_clk = {
	.b = {
		.ctl_reg = GLBL_CLK_ENA_SC_REG,
		.en_mask = BIT(30),
		.halt_reg = GLBL_CLK_STATE_REG,
		.halt_check = HALT_VOTED,
		.halt_bit = 30,
		.reset_mask = P_CSI0_P_CLK,
	},
	.parent = &glbl_root_clk.c,
	.c = {
		.dbg_name = "csi0_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(csi0_p_clk.c),
	},
};

static struct branch_clk emdh_p_clk = {
	.b = {
		.ctl_reg = GLBL_CLK_ENA_2_SC_REG,
		.en_mask = BIT(3),
		.halt_reg = GLBL_CLK_STATE_2_REG,
		.halt_check = HALT_VOTED,
		.halt_bit = 3,
		.reset_mask = P_EMDH_P_CLK,
	},
	.parent = &glbl_root_clk.c,
	.c = {
		.dbg_name = "emdh_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(emdh_p_clk.c),
	},
};

static struct branch_clk grp_2d_p_clk = {
	.b = {
		.ctl_reg = GLBL_CLK_ENA_SC_REG,
		.en_mask = BIT(24),
		.halt_reg = GLBL_CLK_STATE_REG,
		.halt_check = HALT_VOTED,
		.halt_bit = 24,
		.reset_mask = P_GRP_2D_P_CLK,
	},
	.parent = &glbl_root_clk.c,
	.c = {
		.dbg_name = "grp_2d_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(grp_2d_p_clk.c),
	},
};

static struct branch_clk grp_3d_p_clk = {
	.b = {
		.ctl_reg = GLBL_CLK_ENA_2_SC_REG,
		.en_mask = BIT(17),
		.halt_reg = GLBL_CLK_STATE_2_REG,
		.halt_check = HALT_VOTED,
		.halt_bit = 17,
		.reset_mask = P_GRP_3D_P_CLK,
	},
	.parent = &glbl_root_clk.c,
	.c = {
		.dbg_name = "grp_3d_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(grp_3d_p_clk.c),
	},
};

static struct branch_clk jpeg_p_clk = {
	.b = {
		.ctl_reg = GLBL_CLK_ENA_2_SC_REG,
		.en_mask = BIT(24),
		.halt_reg = GLBL_CLK_STATE_2_REG,
		.halt_check = HALT_VOTED,
		.halt_bit = 24,
		.reset_mask = P_JPEG_P_CLK,
	},
	.parent = &glbl_root_clk.c,
	.c = {
		.dbg_name = "jpeg_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(jpeg_p_clk.c),
	},
};

static struct branch_clk lpa_p_clk = {
	.b = {
		.ctl_reg = GLBL_CLK_ENA_2_SC_REG,
		.en_mask = BIT(7),
		.halt_reg = GLBL_CLK_STATE_2_REG,
		.halt_check = HALT_VOTED,
		.halt_bit = 7,
		.reset_mask = P_LPA_P_CLK,
	},
	.parent = &glbl_root_clk.c,
	.c = {
		.dbg_name = "lpa_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(lpa_p_clk.c),
	},
};

static struct branch_clk mdp_p_clk = {
	.b = {
		.ctl_reg = GLBL_CLK_ENA_2_SC_REG,
		.en_mask = BIT(6),
		.halt_reg = GLBL_CLK_STATE_2_REG,
		.halt_check = HALT_VOTED,
		.halt_bit = 6,
		.reset_mask = P_MDP_P_CLK,
	},
	.parent = &glbl_root_clk.c,
	.c = {
		.dbg_name = "mdp_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mdp_p_clk.c),
	},
};

static struct branch_clk mfc_p_clk = {
	.b = {
		.ctl_reg = GLBL_CLK_ENA_2_SC_REG,
		.en_mask = BIT(26),
		.halt_reg = GLBL_CLK_STATE_2_REG,
		.halt_check = HALT_VOTED,
		.halt_bit = 26,
		.reset_mask = P_MFC_P_CLK,
	},
	.parent = &glbl_root_clk.c,
	.c = {
		.dbg_name = "mfc_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mfc_p_clk.c),
	},
};

static struct branch_clk pmdh_p_clk = {
	.b = {
		.ctl_reg = GLBL_CLK_ENA_2_SC_REG,
		.en_mask = BIT(4),
		.halt_reg = GLBL_CLK_STATE_2_REG,
		.halt_check = HALT_VOTED,
		.halt_bit = 4,
		.reset_mask = P_PMDH_P_CLK,
	},
	.parent = &glbl_root_clk.c,
	.c = {
		.dbg_name = "pmdh_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(pmdh_p_clk.c),
	},
};

static struct branch_clk rotator_imem_clk = {
	.b = {
		.ctl_reg = GLBL_CLK_ENA_2_SC_REG,
		.en_mask = BIT(23),
		.halt_reg = GLBL_CLK_STATE_2_REG,
		.halt_check = HALT_VOTED,
		.halt_bit = 23,
		.reset_mask = P_ROTATOR_IMEM_CLK,
	},
	.parent = &glbl_root_clk.c,
	.c = {
		.dbg_name = "rotator_imem_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(rotator_imem_clk.c),
	},
};

static struct branch_clk rotator_p_clk = {
	.b = {
		.ctl_reg = GLBL_CLK_ENA_2_SC_REG,
		.en_mask = BIT(25),
		.halt_reg = GLBL_CLK_STATE_2_REG,
		.halt_check = HALT_VOTED,
		.halt_bit = 25,
		.reset_mask = P_ROTATOR_P_CLK,
	},
	.parent = &glbl_root_clk.c,
	.c = {
		.dbg_name = "rotator_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(rotator_p_clk.c),
	},
};

static struct branch_clk sdc1_p_clk = {
	.b = {
		.ctl_reg = GLBL_CLK_ENA_SC_REG,
		.en_mask = BIT(7),
		.halt_reg = GLBL_CLK_STATE_REG,
		.halt_check = HALT_VOTED,
		.halt_bit = 7,
		.reset_mask = P_SDC1_P_CLK,
	},
	.parent = &glbl_root_clk.c,
	.c = {
		.dbg_name = "sdc1_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(sdc1_p_clk.c),
	},
};

static struct branch_clk sdc2_p_clk = {
	.b = {
		.ctl_reg = GLBL_CLK_ENA_SC_REG,
		.en_mask = BIT(8),
		.halt_reg = GLBL_CLK_STATE_REG,
		.halt_check = HALT_VOTED,
		.halt_bit = 8,
		.reset_mask = P_SDC2_P_CLK,
	},
	.parent = &glbl_root_clk.c,
	.c = {
		.dbg_name = "sdc2_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(sdc2_p_clk.c),
	},
};

static struct branch_clk sdc3_p_clk = {
	.b = {
		.ctl_reg = GLBL_CLK_ENA_SC_REG,
		.en_mask = BIT(27),
		.halt_reg = GLBL_CLK_STATE_REG,
		.halt_check = HALT_VOTED,
		.halt_bit = 27,
		.reset_mask = P_SDC3_P_CLK,
	},
	.parent = &glbl_root_clk.c,
	.c = {
		.dbg_name = "sdc3_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(sdc3_p_clk.c),
	},
};

static struct branch_clk sdc4_p_clk = {
	.b = {
		.ctl_reg = GLBL_CLK_ENA_SC_REG,
		.en_mask = BIT(28),
		.halt_reg = GLBL_CLK_STATE_REG,
		.halt_check = HALT_VOTED,
		.halt_bit = 28,
		.reset_mask = P_SDC4_P_CLK,
	},
	.parent = &glbl_root_clk.c,
	.c = {
		.dbg_name = "sdc4_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(sdc4_p_clk.c),
	},
};

static struct branch_clk spi_p_clk = {
	.b = {
		.ctl_reg = GLBL_CLK_ENA_2_SC_REG,
		.en_mask = BIT(10),
		.halt_reg = GLBL_CLK_STATE_2_REG,
		.halt_check = HALT_VOTED,
		.halt_bit = 10,
		.reset_mask = P_SPI_P_CLK,
	},
	.parent = &glbl_root_clk.c,
	.c = {
		.dbg_name = "spi_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(spi_p_clk.c),
	},
};

static struct branch_clk tsif_p_clk = {
	.b = {
		.ctl_reg = GLBL_CLK_ENA_SC_REG,
		.en_mask = BIT(18),
		.halt_reg = GLBL_CLK_STATE_REG,
		.halt_check = HALT_VOTED,
		.halt_bit = 18,
		.reset_mask = P_TSIF_P_CLK,
	},
	.parent = &glbl_root_clk.c,
	.c = {
		.dbg_name = "tsif_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(tsif_p_clk.c),
	},
};

static struct branch_clk uart1dm_p_clk = {
	.b = {
		.ctl_reg = GLBL_CLK_ENA_SC_REG,
		.en_mask = BIT(17),
		.halt_reg = GLBL_CLK_STATE_REG,
		.halt_check = HALT_VOTED,
		.halt_bit = 17,
	},
	.parent = &glbl_root_clk.c,
	.c = {
		.dbg_name = "uart1dm_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(uart1dm_p_clk.c),
	},
};

static struct branch_clk uart2dm_p_clk = {
	.b = {
		.ctl_reg = GLBL_CLK_ENA_SC_REG,
		.en_mask = BIT(26),
		.halt_reg = GLBL_CLK_STATE_REG,
		.halt_check = HALT_VOTED,
		.halt_bit = 26,
	},
	.parent = &glbl_root_clk.c,
	.c = {
		.dbg_name = "uart2dm_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(uart2dm_p_clk.c),
	},
};

static struct branch_clk usb_hs2_p_clk = {
	.b = {
		.ctl_reg = GLBL_CLK_ENA_2_SC_REG,
		.en_mask = BIT(8),
		.halt_reg = GLBL_CLK_STATE_2_REG,
		.halt_check = HALT_VOTED,
		.halt_bit = 8,
		.reset_mask = P_USB_HS2_P_CLK,
	},
	.parent = &glbl_root_clk.c,
	.c = {
		.dbg_name = "usb_hs2_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(usb_hs2_p_clk.c),
	},
};

static struct branch_clk usb_hs3_p_clk = {
	.b = {
		.ctl_reg = GLBL_CLK_ENA_2_SC_REG,
		.en_mask = BIT(9),
		.halt_reg = GLBL_CLK_STATE_2_REG,
		.halt_check = HALT_VOTED,
		.halt_bit = 9,
		.reset_mask = P_USB_HS3_P_CLK,
	},
	.parent = &glbl_root_clk.c,
	.c = {
		.dbg_name = "usb_hs3_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(usb_hs3_p_clk.c),
	},
};

static struct branch_clk usb_hs_p_clk = {
	.b = {
		.ctl_reg = GLBL_CLK_ENA_SC_REG,
		.en_mask = BIT(25),
		.halt_reg = GLBL_CLK_STATE_REG,
		.halt_check = HALT_VOTED,
		.halt_bit = 25,
		.reset_mask = P_USB_HS_P_CLK,
	},
	.parent = &glbl_root_clk.c,
	.c = {
		.dbg_name = "usb_hs_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(usb_hs_p_clk.c),
	},
};

static struct branch_clk vfe_p_clk = {
	.b = {
		.ctl_reg = GLBL_CLK_ENA_2_SC_REG,
		.en_mask = BIT(27),
		.halt_reg = GLBL_CLK_STATE_2_REG,
		.halt_check = HALT_VOTED,
		.halt_bit = 27,
		.reset_mask = P_VFE_P_CLK,
	},
	.parent = &glbl_root_clk.c,
	.c = {
		.dbg_name = "vfe_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(vfe_p_clk.c),
	},
};

static struct clk_freq_tbl clk_tbl_csi[] = {
	F_MND8(        0,  0,  0, gnd,  1, 0, 0, NONE),
	F_MND8(153600000, 24, 17, pll1, 2, 2, 5, NOMINAL),
	F_MND8(192000000, 24, 17, pll1, 4, 0, 0, NOMINAL),
	F_MND8(384000000, 24, 17, pll1, 2, 0, 0, NOMINAL),
	F_END,
};

static struct rcg_clk csi0_clk = {
	.b = {
		.ctl_reg = CSI_NS_REG,
		.en_mask = BIT(9),
		.halt_reg = CLK_HALT_STATEC_REG,
		.halt_bit = 17,
		.reset_mask = P_CSI0_CLK,
	},
	.ns_reg = CSI_NS_REG,
	.md_reg = CSI_NS_REG - 4,
	.ns_mask = F_MASK_MND8(24, 17),
	.root_en_mask = BIT(11),
	.freq_tbl = clk_tbl_csi,
	.current_freq = &local_dummy_freq,
	.set_rate = set_rate_mnd,
	.c = {
		.dbg_name = "csi0_clk",
		.ops = &soc_clk_ops_7x30,
		CLK_INIT(csi0_clk.c),
	},
};

static struct clk_freq_tbl clk_tbl_tcxo[] = {
	F_RAW(19200000, &tcxo_clk.c, 0, 0, 0, 0, NOMINAL, NULL),
	F_END,
};

static struct rcg_clk i2c_clk = {
	.b = {
		.ctl_reg = I2C_NS_REG,
		.en_mask = BIT(9),
		.halt_reg = CLK_HALT_STATEA_REG,
		.halt_bit = 15,
		.reset_mask = P_I2C_CLK,
	},
	.set_rate = set_rate_nop,
	.freq_tbl = clk_tbl_tcxo,
	.root_en_mask = BIT(11),
	.current_freq = &local_dummy_freq,
	.c = {
		.dbg_name = "i2c_clk",
		.ops = &soc_clk_ops_7x30,
		CLK_INIT(i2c_clk.c),
	},
};

static struct rcg_clk i2c_2_clk = {
	.b = {
		.ctl_reg = I2C_2_NS_REG,
		.en_mask = BIT(0),
		.halt_reg = CLK_HALT_STATEC_REG,
		.halt_bit = 2,
		.reset_mask = P_I2C_2_CLK,
	},
	.root_en_mask = BIT(2),
	.freq_tbl = clk_tbl_tcxo,
	.set_rate = set_rate_nop,
	.current_freq = &local_dummy_freq,
	.c = {
		.dbg_name = "i2c_2_clk",
		.ops = &soc_clk_ops_7x30,
		CLK_INIT(i2c_2_clk.c),
	},
};

static struct rcg_clk qup_i2c_clk = {
	.b = {
		.ctl_reg = QUP_I2C_NS_REG,
		.en_mask = BIT(0),
		.halt_reg = CLK_HALT_STATEB_REG,
		.halt_bit = 31,
		.reset_mask = P_QUP_I2C_CLK,
	},
	.root_en_mask = BIT(2),
	.freq_tbl = clk_tbl_tcxo,
	.set_rate = set_rate_nop,
	.current_freq = &local_dummy_freq,
	.c = {
		.dbg_name = "qup_i2c_clk",
		.ops = &soc_clk_ops_7x30,
		CLK_INIT(qup_i2c_clk.c),
	},
};

static struct rcg_clk uart1_clk = {
	.b = {
		.ctl_reg = UART_NS_REG,
		.en_mask = BIT(5),
		.halt_reg = CLK_HALT_STATEB_REG,
		.halt_bit = 7,
		.reset_mask = P_UART1_CLK,
	},
	.root_en_mask = BIT(4),
	.freq_tbl = clk_tbl_tcxo,
	.set_rate = set_rate_nop,
	.current_freq = &local_dummy_freq,
	.c = {
		.dbg_name = "uart1_clk",
		.ops = &soc_clk_ops_7x30,
		CLK_INIT(uart1_clk.c),
	},
};

static struct rcg_clk uart2_clk = {
	.b = {
		.ctl_reg = UART2_NS_REG,
		.en_mask = BIT(5),
		.halt_reg = CLK_HALT_STATEB_REG,
		.halt_bit = 5,
		.reset_mask = P_UART2_CLK,
	},
	.root_en_mask = BIT(4),
	.freq_tbl = clk_tbl_tcxo,
	.set_rate = set_rate_nop,
	.current_freq = &local_dummy_freq,
	.c = {
		.dbg_name = "uart2_clk",
		.ops = &soc_clk_ops_7x30,
		CLK_INIT(uart2_clk.c),
	},
};

static struct clk_freq_tbl clk_tbl_uartdm[] = {
	F_MND16(       0, gnd,  1,   0,   0, NONE),
	F_MND16( 3686400, pll3, 3,   3, 200, NOMINAL),
	F_MND16( 7372800, pll3, 3,   3, 100, NOMINAL),
	F_MND16(14745600, pll3, 3,   3,  50, NOMINAL),
	F_MND16(32000000, pll3, 3,  25, 192, NOMINAL),
	F_MND16(40000000, pll3, 3, 125, 768, NOMINAL),
	F_MND16(46400000, pll3, 3, 145, 768, NOMINAL),
	F_MND16(48000000, pll3, 3,  25, 128, NOMINAL),
	F_MND16(51200000, pll3, 3,   5,  24, NOMINAL),
	F_MND16(56000000, pll3, 3, 175, 768, NOMINAL),
	F_MND16(58982400, pll3, 3,   6,  25, NOMINAL),
	F_MND16(64000000, pll1, 4,   1,   3, NOMINAL),
	F_END,
};

static struct rcg_clk uart1dm_clk = {
	.b = {
		.ctl_reg = UART1DM_NS_REG,
		.en_mask = BIT(9),
		.halt_reg = CLK_HALT_STATEB_REG,
		.halt_bit = 6,
		.reset_mask = P_UART1DM_CLK,
	},
	.ns_reg = UART1DM_NS_REG,
	.md_reg = UART1DM_NS_REG - 4,
	.root_en_mask = BIT(11),
	.freq_tbl = clk_tbl_uartdm,
	.ns_mask = F_MASK_MND16,
	.current_freq = &local_dummy_freq,
	.set_rate = set_rate_mnd,
	.c = {
		.dbg_name = "uart1dm_clk",
		.ops = &soc_clk_ops_7x30,
		CLK_INIT(uart1dm_clk.c),
	},
};

static struct rcg_clk uart2dm_clk = {
	.b = {
		.ctl_reg = UART2DM_NS_REG,
		.en_mask = BIT(9),
		.halt_reg = CLK_HALT_STATEB_REG,
		.halt_bit = 23,
		.reset_mask = P_UART2DM_CLK,
	},
	.ns_reg = UART2DM_NS_REG,
	.md_reg = UART2DM_NS_REG - 4,
	.root_en_mask = BIT(11),
	.freq_tbl = clk_tbl_uartdm,
	.ns_mask = F_MASK_MND16,
	.set_rate = set_rate_mnd,
	.current_freq = &local_dummy_freq,
	.c = {
		.dbg_name = "uart2dm_clk",
		.ops = &soc_clk_ops_7x30,
		CLK_INIT(uart2dm_clk.c),
	},
};

static struct clk_freq_tbl clk_tbl_mdh[] = {
	F_BASIC(        0, gnd,   1, NONE),
	F_BASIC( 49150000, pll3, 15, NOMINAL),
	F_BASIC( 92160000, pll3,  8, NOMINAL),
	F_BASIC(122880000, pll3,  6, NOMINAL),
	F_BASIC(184320000, pll3,  4, NOMINAL),
	F_BASIC(245760000, pll3,  3, NOMINAL),
	F_BASIC(368640000, pll3,  2, NOMINAL),
	F_BASIC(384000000, pll1,  2, NOMINAL),
	F_BASIC(445500000, pll4,  2, NOMINAL),
	F_END,
};

static struct rcg_clk emdh_clk = {
	.b = {
		.ctl_reg = EMDH_NS_REG,
		.halt_check = DELAY,
		.reset_mask = P_EMDH_CLK,
	},
	.root_en_mask = BIT(11),
	.ns_reg = EMDH_NS_REG,
	.ns_mask = F_MASK_BASIC,
	.depends = &axi_li_adsp_a_clk.c,
	.set_rate = set_rate_nop,
	.freq_tbl = clk_tbl_mdh,
	.current_freq = &local_dummy_freq,
	.c = {
		.dbg_name = "emdh_clk",
		.flags = CLKFLAG_MIN | CLKFLAG_MAX,
		.ops = &soc_clk_ops_7x30,
		CLK_INIT(emdh_clk.c),
	},
};

static struct rcg_clk pmdh_clk = {
	.b = {
		.ctl_reg = PMDH_NS_REG,
		.halt_check = DELAY,
		.reset_mask = P_PMDH_CLK,
	},
	.root_en_mask = BIT(11),
	.ns_reg = PMDH_NS_REG,
	.ns_mask = F_MASK_BASIC,
	.depends = &axi_li_adsp_a_clk.c,
	.set_rate = set_rate_nop,
	.freq_tbl = clk_tbl_mdh,
	.current_freq = &local_dummy_freq,
	.c = {
		.dbg_name = "pmdh_clk",
		.flags = CLKFLAG_MIN | CLKFLAG_MAX,
		.ops = &soc_clk_ops_7x30,
		CLK_INIT(pmdh_clk.c),
	},
};

static struct clk_freq_tbl clk_tbl_grp[] = {
	F_BASIC( 24576000, lpxo,  1, NOMINAL),
	F_BASIC( 46080000, pll3, 16, NOMINAL),
	F_BASIC( 49152000, pll3, 15, NOMINAL),
	F_BASIC( 52662875, pll3, 14, NOMINAL),
	F_BASIC( 56713846, pll3, 13, NOMINAL),
	F_BASIC( 61440000, pll3, 12, NOMINAL),
	F_BASIC( 67025454, pll3, 11, NOMINAL),
	F_BASIC( 73728000, pll3, 10, NOMINAL),
	F_BASIC( 81920000, pll3,  9, NOMINAL),
	F_BASIC( 92160000, pll3,  8, NOMINAL),
	F_BASIC(105325714, pll3,  7, NOMINAL),
	F_BASIC(122880000, pll3,  6, NOMINAL),
	F_BASIC(147456000, pll3,  5, NOMINAL),
	F_BASIC(184320000, pll3,  4, NOMINAL),
	F_BASIC(192000000, pll1,  4, NOMINAL),
	F_BASIC(245760000, pll3,  3, HIGH),
	/* Sync to AXI. Hence this "rate" is not fixed. */
	F_RAW(1, &lpxo_clk.c, 0, BIT(14), 0, 0, NOMINAL, NULL),
	F_END,
};

static struct rcg_clk grp_2d_clk = {
	.b = {
		.ctl_reg = GRP_2D_NS_REG,
		.en_mask = BIT(7),
		.halt_reg = CLK_HALT_STATEA_REG,
		.halt_bit = 31,
		.reset_mask = P_GRP_2D_CLK,
	},
	.ns_reg = GRP_2D_NS_REG,
	.root_en_mask = BIT(11),
	.ns_mask = F_MASK_BASIC | (7 << 12),
	.set_rate = set_rate_nop,
	.freq_tbl = clk_tbl_grp,
	.depends = &axi_grp_2d_clk.c,
	.current_freq = &local_dummy_freq,
	.c = {
		.dbg_name = "grp_2d_clk",
		.ops = &soc_clk_ops_7x30,
		CLK_INIT(grp_2d_clk.c),
	},
};

static struct rcg_clk grp_3d_src_clk = {
	.ns_reg = GRP_NS_REG,
	.b = {
		.ctl_reg = GRP_NS_REG,
		.halt_check = NOCHECK,
	},
	.root_en_mask = BIT(11),
	.ns_mask = F_MASK_BASIC | (7 << 12),
	.set_rate = set_rate_nop,
	.freq_tbl = clk_tbl_grp,
	.depends = &axi_li_grp_clk.c,
	.current_freq = &local_dummy_freq,
	.c = {
		.dbg_name = "grp_3d_src_clk",
		.ops = &soc_clk_ops_7x30,
		CLK_INIT(grp_3d_src_clk.c),
	},
};

static struct branch_clk grp_3d_clk = {
	.b = {
		.ctl_reg = GRP_NS_REG,
		.en_mask = BIT(7),
		.halt_reg = CLK_HALT_STATEB_REG,
		.halt_bit = 18,
		.reset_mask = P_GRP_3D_CLK,
	},
	.parent = &grp_3d_src_clk.c,
	.c = {
		.dbg_name = "grp_3d_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(grp_3d_clk.c),
	},
};

static struct branch_clk imem_clk = {
	.b = {
		.ctl_reg = GRP_NS_REG,
		.en_mask = BIT(9),
		.halt_reg = CLK_HALT_STATEB_REG,
		.halt_bit = 19,
		.reset_mask = P_IMEM_CLK,
	},
	.parent = &grp_3d_src_clk.c,
	.c = {
		.dbg_name = "imem_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(imem_clk.c),
	},
};

static struct clk_freq_tbl clk_tbl_sdc1_3[] = {
	F_MND8(       0,  0,  0, gnd,  1,   0,  0,   NONE),
	F_MND8(  144000, 19, 12, lpxo, 1,   1,  171, NOMINAL),
	F_MND8(  400000, 19, 12, lpxo, 1,   2,  123, NOMINAL),
	F_MND8(16027000, 19, 12, pll3, 3,  14,  215, NOMINAL),
	F_MND8(17000000, 19, 12, pll3, 4,  19,  206, NOMINAL),
	F_MND8(20480000, 19, 12, pll3, 4,  23,  212, NOMINAL),
	F_MND8(24576000, 19, 12, lpxo, 1,   0,    0, NOMINAL),
	F_MND8(49152000, 19, 12, pll3, 3,   1,    5, NOMINAL),
	F_END,
};

static struct rcg_clk sdc1_clk = {
	.b = {
		.ctl_reg = SDCn_NS_REG(1),
		.en_mask = BIT(9),
		.halt_reg = CLK_HALT_STATEA_REG,
		.halt_bit = 1,
		.reset_mask = P_SDC1_CLK,
	},
	.ns_reg = SDCn_NS_REG(1),
	.md_reg = SDCn_NS_REG(1) - 4,
	.ns_mask = F_MASK_MND8(19, 12),
	.root_en_mask = BIT(11),
	.freq_tbl = clk_tbl_sdc1_3,
	.current_freq = &local_dummy_freq,
	.set_rate = set_rate_mnd,
	.c = {
		.dbg_name = "sdc1_clk",
		.ops = &soc_clk_ops_7x30,
		CLK_INIT(sdc1_clk.c),
	},
};

static struct rcg_clk sdc3_clk = {
	.b = {
		.ctl_reg = SDCn_NS_REG(3),
		.en_mask = BIT(9),
		.halt_reg = CLK_HALT_STATEB_REG,
		.halt_bit = 24,
		.reset_mask = P_SDC3_CLK,
	},
	.ns_reg = SDCn_NS_REG(3),
	.md_reg = SDCn_NS_REG(3) - 4,
	.ns_mask = F_MASK_MND8(19, 12),
	.root_en_mask = BIT(11),
	.freq_tbl = clk_tbl_sdc1_3,
	.current_freq = &local_dummy_freq,
	.set_rate = set_rate_mnd,
	.c = {
		.dbg_name = "sdc3_clk",
		.ops = &soc_clk_ops_7x30,
		CLK_INIT(sdc3_clk.c),
	},
};

static struct clk_freq_tbl clk_tbl_sdc2_4[] = {
	F_MND8(       0,  0,  0, gnd,  1,   0,  0,   NONE),
	F_MND8(  144000, 20, 13, lpxo, 1,   1,  171, NOMINAL),
	F_MND8(  400000, 20, 13, lpxo, 1,   2,  123, NOMINAL),
	F_MND8(16027000, 20, 13, pll3, 3,  14,  215, NOMINAL),
	F_MND8(17000000, 20, 13, pll3, 4,  19,  206, NOMINAL),
	F_MND8(20480000, 20, 13, pll3, 4,  23,  212, NOMINAL),
	F_MND8(24576000, 20, 13, lpxo, 1,   0,    0, NOMINAL),
	F_MND8(49152000, 20, 13, pll3, 3,   1,    5, NOMINAL),
	F_END,
};

static struct rcg_clk sdc2_clk = {
	.b = {
		.ctl_reg = SDCn_NS_REG(2),
		.en_mask = BIT(9),
		.halt_reg = CLK_HALT_STATEA_REG,
		.halt_bit = 0,
		.reset_mask = P_SDC2_CLK,
	},
	.ns_reg = SDCn_NS_REG(2),
	.md_reg = SDCn_NS_REG(2) - 4,
	.ns_mask = F_MASK_MND8(20, 13),
	.root_en_mask = BIT(11),
	.freq_tbl = clk_tbl_sdc2_4,
	.current_freq = &local_dummy_freq,
	.set_rate = set_rate_mnd,
	.c = {
		.dbg_name = "sdc2_clk",
		.ops = &soc_clk_ops_7x30,
		CLK_INIT(sdc2_clk.c),
	},
};

static struct rcg_clk sdc4_clk = {
	.b = {
		.ctl_reg = SDCn_NS_REG(4),
		.en_mask = BIT(9),
		.halt_reg = CLK_HALT_STATEB_REG,
		.halt_bit = 25,
		.reset_mask = P_SDC4_CLK,
	},
	.ns_reg = SDCn_NS_REG(4),
	.md_reg = SDCn_NS_REG(4) - 4,
	.ns_mask = F_MASK_MND8(20, 13),
	.root_en_mask = BIT(11),
	.freq_tbl = clk_tbl_sdc2_4,
	.current_freq = &local_dummy_freq,
	.set_rate = set_rate_mnd,
	.c = {
		.dbg_name = "sdc4_clk",
		.ops = &soc_clk_ops_7x30,
		CLK_INIT(sdc4_clk.c),
	},
};

static struct clk_freq_tbl clk_tbl_mdp_core[] = {
	F_BASIC( 24576000, lpxo,  1, NOMINAL),
	F_BASIC( 46080000, pll3, 16, NOMINAL),
	F_BASIC( 49152000, pll3, 15, NOMINAL),
	F_BASIC( 52663000, pll3, 14, NOMINAL),
	F_BASIC( 92160000, pll3,  8, NOMINAL),
	F_BASIC(122880000, pll3,  6, NOMINAL),
	F_BASIC(147456000, pll3,  5, NOMINAL),
	F_BASIC(153600000, pll1,  5, NOMINAL),
	F_BASIC(192000000, pll1,  4, HIGH),
	F_END,
};

static struct rcg_clk mdp_clk = {
	.b = {
		.ctl_reg = MDP_NS_REG,
		.en_mask = BIT(9),
		.halt_reg = CLK_HALT_STATEB_REG,
		.halt_bit = 16,
		.reset_mask = P_MDP_CLK,
	},
	.ns_reg = MDP_NS_REG,
	.root_en_mask = BIT(11),
	.ns_mask = F_MASK_BASIC,
	.depends = &axi_mdp_clk.c,
	.set_rate = set_rate_nop,
	.freq_tbl = clk_tbl_mdp_core,
	.current_freq = &local_dummy_freq,
	.c = {
		.dbg_name = "mdp_clk",
		.ops = &soc_clk_ops_7x30,
		CLK_INIT(mdp_clk.c),
	},
};

static struct clk_freq_tbl clk_tbl_mdp_lcdc[] = {
	F_MND16(       0, gnd,  1,   0,   0, NONE),
	F_MND16(24576000, lpxo, 1,   0,   0, NOMINAL),
	F_MND16(30720000, pll3, 4,   1,   6, NOMINAL),
	F_MND16(32768000, pll3, 3,   2,  15, NOMINAL),
	F_MND16(40960000, pll3, 2,   1,   9, NOMINAL),
	F_MND16(73728000, pll3, 2,   1,   5, NOMINAL),
	F_END,
};

static struct rcg_clk mdp_lcdc_pclk_clk = {
	.b = {
		.ctl_reg = MDP_LCDC_NS_REG,
		.en_mask = BIT(9),
		.halt_reg = CLK_HALT_STATEB_REG,
		.halt_bit = 28,
		.reset_mask = P_MDP_LCDC_PCLK_CLK,
	},
	.ns_reg = MDP_LCDC_NS_REG,
	.md_reg = MDP_LCDC_NS_REG - 4,
	.root_en_mask = BIT(11),
	.ns_mask = F_MASK_MND16,
	.set_rate = set_rate_mnd,
	.freq_tbl = clk_tbl_mdp_lcdc,
	.current_freq = &local_dummy_freq,
	.c = {
		.dbg_name = "mdp_lcdc_pclk_clk",
		.ops = &soc_clk_ops_7x30,
		CLK_INIT(mdp_lcdc_pclk_clk.c),
	},
};

static struct branch_clk mdp_lcdc_pad_pclk_clk = {
	.b = {
		.ctl_reg = MDP_LCDC_NS_REG,
		.en_mask = BIT(12),
		.halt_reg = CLK_HALT_STATEB_REG,
		.halt_bit = 29,
		.reset_mask = P_MDP_LCDC_PAD_PCLK_CLK,
	},
	.parent = &mdp_lcdc_pclk_clk.c,
	.c = {
		.dbg_name = "mdp_lcdc_pad_pclk_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mdp_lcdc_pad_pclk_clk.c),
	},
};

static struct clk_freq_tbl clk_tbl_mdp_vsync[] = {
	F_RAW(       0, &gnd_clk.c,  0, (0x3<<2), 0, 0, NONE,    NULL),
	F_RAW(24576000, &lpxo_clk.c, 0, (0x1<<2), 0, 0, NOMINAL, NULL),
	F_END,
};

static struct rcg_clk mdp_vsync_clk = {
	.b = {
		.ctl_reg = MDP_VSYNC_REG,
		.en_mask = BIT(0),
		.halt_reg = CLK_HALT_STATEB_REG,
		.halt_bit = 30,
		.reset_mask = P_MDP_VSYNC_CLK,
	},
	.ns_reg = MDP_VSYNC_REG,
	.ns_mask = BM(3, 2),
	.freq_tbl = clk_tbl_mdp_vsync,
	.set_rate = set_rate_nop,
	.current_freq = &local_dummy_freq,
	.c = {
		.dbg_name = "mdp_vsync_clk",
		.ops = &soc_clk_ops_7x30,
		CLK_INIT(mdp_vsync_clk.c),
	},
};

static struct clk_freq_tbl clk_tbl_mi2s_codec[] = {
	F_MND16(       0, gnd,  1,   0,   0, NONE),
	F_MND16( 2048000, lpxo, 4,   1,   3, NOMINAL),
	F_MND16(12288000, lpxo, 2,   0,   0, NOMINAL),
	F_END,
};

static struct rcg_clk mi2s_codec_rx_m_clk = {
	.b = {
		.ctl_reg = MI2S_RX_NS_REG,
		.en_mask = BIT(12),
		.halt_reg = CLK_HALT_STATEA_REG,
		.halt_bit = 12,
		.reset_mask = P_MI2S_CODEC_RX_M_CLK,
	},
	.ns_reg = MI2S_RX_NS_REG,
	.md_reg = MI2S_RX_NS_REG - 4,
	.root_en_mask = BIT(11),
	.ns_mask = F_MASK_MND16,
	.set_rate = set_rate_mnd,
	.freq_tbl = clk_tbl_mi2s_codec,
	.current_freq = &local_dummy_freq,
	.c = {
		.dbg_name = "mi2s_codec_rx_m_clk",
		.ops = &soc_clk_ops_7x30,
		CLK_INIT(mi2s_codec_rx_m_clk.c),
	},
};

static struct branch_clk mi2s_codec_rx_s_clk = {
	.b = {
		.ctl_reg = MI2S_RX_NS_REG,
		.en_mask = BIT(9),
		.halt_reg = CLK_HALT_STATEA_REG,
		.halt_bit = 13,
		.reset_mask = P_MI2S_CODEC_RX_S_CLK,
	},
	.parent = &mi2s_codec_rx_m_clk.c,
	.c = {
		.dbg_name = "mi2s_codec_rx_s_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mi2s_codec_rx_s_clk.c),
	},
};

static struct rcg_clk mi2s_codec_tx_m_clk = {
	.b = {
		.ctl_reg = MI2S_TX_NS_REG,
		.en_mask = BIT(12),
		.halt_reg = CLK_HALT_STATEC_REG,
		.halt_bit = 8,
		.reset_mask = P_MI2S_CODEC_TX_M_CLK,
	},
	.ns_reg = MI2S_TX_NS_REG,
	.md_reg = MI2S_TX_NS_REG - 4,
	.root_en_mask = BIT(11),
	.ns_mask = F_MASK_MND16,
	.set_rate = set_rate_mnd,
	.freq_tbl = clk_tbl_mi2s_codec,
	.current_freq = &local_dummy_freq,
	.c = {
		.dbg_name = "mi2s_codec_tx_m_clk",
		.ops = &soc_clk_ops_7x30,
		CLK_INIT(mi2s_codec_tx_m_clk.c),
	},
};

static struct branch_clk mi2s_codec_tx_s_clk = {
	.b = {
		.ctl_reg = MI2S_TX_NS_REG,
		.en_mask = BIT(9),
		.halt_reg = CLK_HALT_STATEA_REG,
		.halt_bit = 11,
		.reset_mask = P_MI2S_CODEC_TX_S_CLK,
	},
	.parent = &mi2s_codec_tx_m_clk.c,
	.c = {
		.dbg_name = "mi2s_codec_tx_s_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mi2s_codec_tx_s_clk.c),
	},
};

static struct clk_freq_tbl clk_tbl_mi2s[] = {
	F_MND16(       0, gnd,  1,   0,   0, NONE),
	F_MND16(12288000, lpxo, 2,   0,   0, NOMINAL),
	F_END,
};

static struct rcg_clk mi2s_m_clk = {
	.b = {
		.ctl_reg = MI2S_NS_REG,
		.en_mask = BIT(12),
		.halt_reg = CLK_HALT_STATEC_REG,
		.halt_bit = 4,
		.reset_mask = P_MI2S_M_CLK,
	},
	.ns_reg = MI2S_NS_REG,
	.md_reg = MI2S_NS_REG - 4,
	.root_en_mask = BIT(11),
	.ns_mask = F_MASK_MND16,
	.set_rate = set_rate_mnd,
	.freq_tbl = clk_tbl_mi2s,
	.current_freq = &local_dummy_freq,
	.c = {
		.dbg_name = "mi2s_m_clk",
		.ops = &soc_clk_ops_7x30,
		CLK_INIT(mi2s_m_clk.c),
	},
};

static struct branch_clk mi2s_s_clk = {
	.b = {
		.ctl_reg = MI2S_NS_REG,
		.en_mask = BIT(9),
		.halt_reg = CLK_HALT_STATEC_REG,
		.halt_bit = 3,
		.reset_mask = P_MI2S_S_CLK,
	},
	.parent = &mi2s_m_clk.c,
	.c = {
		.dbg_name = "mi2s_s_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mi2s_s_clk.c),
	},
};

static struct clk_freq_tbl clk_tbl_midi[] = {
	F_MND8(       0,  0,  0, gnd,  1,  0,  0, NONE),
	F_MND8(98304000, 19, 12, pll3, 3,  2,  5, NOMINAL),
	F_END,
};

static struct rcg_clk midi_clk = {
	.b = {
		.ctl_reg = MIDI_NS_REG,
		.en_mask = BIT(9),
		.halt_reg = CLK_HALT_STATEC_REG,
		.halt_bit = 1,
	},
	.ns_reg = MIDI_NS_REG,
	.md_reg = MIDI_NS_REG - 4,
	.ns_mask = F_MASK_MND8(19, 12),
	.root_en_mask = BIT(11),
	.freq_tbl = clk_tbl_midi,
	.current_freq = &local_dummy_freq,
	.set_rate = set_rate_mnd,
	.c = {
		.dbg_name = "midi_clk",
		.ops = &soc_clk_ops_7x30,
		CLK_INIT(midi_clk.c),
	},
};

#define F_SDAC(f, s, div, m, n, v) \
	{ \
		.freq_hz = f, \
		.md_val = MD16(m, n), \
		.ns_val = N16(m, n) | SPDIV(SRC_SEL_SDAC_##s, div), \
		.mnd_en_mask = BIT(8) * !!(n), \
		.sys_vdd = v, \
		.src_clk = &s##_clk.c, \
	}

static struct clk_freq_tbl clk_tbl_sdac[] = {
	F_SDAC( 256000, lpxo, 4,   1,    24, NOMINAL),
	F_SDAC( 352800, lpxo, 1, 147, 10240, NOMINAL),
	F_SDAC( 384000, lpxo, 4,   1,    16, NOMINAL),
	F_SDAC( 512000, lpxo, 4,   1,    12, NOMINAL),
	F_SDAC( 705600, lpxo, 1, 147,  5120, NOMINAL),
	F_SDAC( 768000, lpxo, 4,   1,     8, NOMINAL),
	F_SDAC(1024000, lpxo, 4,   1,     6, NOMINAL),
	F_SDAC(1411200, lpxo, 1, 147,  2560, NOMINAL),
	F_SDAC(1536000, lpxo, 4,   1,     4, NOMINAL),
	F_END,
};

static struct rcg_clk sdac_clk = {
	.b = {
		.ctl_reg = SDAC_NS_REG,
		.en_mask = BIT(9),
		.halt_reg = CLK_HALT_STATEA_REG,
		.halt_bit = 2,
		.reset_mask = P_SDAC_CLK,
	},
	.ns_reg = SDAC_NS_REG,
	.md_reg = SDAC_NS_REG - 4,
	.root_en_mask = BIT(11),
	.freq_tbl = clk_tbl_sdac,
	.ns_mask = F_MASK_MND16,
	.set_rate = set_rate_mnd,
	.current_freq = &local_dummy_freq,
	.c = {
		.dbg_name = "sdac_clk",
		.ops = &soc_clk_ops_7x30,
		CLK_INIT(sdac_clk.c),
	},
};

static struct branch_clk sdac_m_clk = {
	.b = {
		.ctl_reg = SDAC_NS_REG,
		.en_mask = BIT(12),
		.halt_reg = CLK_HALT_STATEB_REG,
		.halt_bit = 17,
		.reset_mask = P_SDAC_M_CLK,
	},
	.parent = &sdac_clk.c,
	.c = {
		.dbg_name = "sdac_m_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(sdac_m_clk.c),
	},
};

static struct clk_freq_tbl clk_tbl_tv[] = {
	F_MND8(       0,  0,  0, gnd,  1,  0,   0, NONE),
	F_MND8(27000000, 23, 16, pll4, 2,  2,  33, NOMINAL),
	F_MND8(74250000, 23, 16, pll4, 2,  1,   6, NOMINAL),
	F_END,
};

static struct rcg_clk tv_clk = {
	.ns_reg = TV_NS_REG,
	.b = {
		.ctl_reg = TV_NS_REG,
		.halt_check = NOCHECK,
	},
	.md_reg = TV_NS_REG - 4,
	.ns_mask = F_MASK_MND8(23, 16),
	.root_en_mask = BIT(11),
	.freq_tbl = clk_tbl_tv,
	.current_freq = &local_dummy_freq,
	.set_rate = set_rate_mnd,
	.c = {
		.dbg_name = "tv_clk",
		.ops = &soc_clk_ops_7x30,
		CLK_INIT(tv_clk.c),
	},
};

static struct branch_clk hdmi_clk = {
	.b = {
		.ctl_reg = HDMI_NS_REG,
		.en_mask = BIT(9),
		.halt_reg = CLK_HALT_STATEC_REG,
		.halt_bit = 7,
		.reset_mask = P_HDMI_CLK,
	},
	.parent = &tv_clk.c,
	.c = {
		.dbg_name = "hdmi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(hdmi_clk.c),
	},
};

static struct branch_clk tv_dac_clk = {
	.b = {
		.ctl_reg = TV_NS_REG,
		.en_mask = BIT(12),
		.halt_reg = CLK_HALT_STATEB_REG,
		.halt_bit = 27,
		.reset_mask = P_TV_DAC_CLK,
	},
	.parent = &tv_clk.c,
	.c = {
		.dbg_name = "tv_dac_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(tv_dac_clk.c),
	},
};

static struct branch_clk tv_enc_clk = {
	.b = {
		.ctl_reg = TV_NS_REG,
		.en_mask = BIT(9),
		.halt_reg = CLK_HALT_STATEB_REG,
		.halt_bit = 10,
		.reset_mask = P_TV_ENC_CLK,
	},
	.parent = &tv_clk.c,
	.c = {
		.dbg_name = "tv_enc_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(tv_enc_clk.c),
	},
};

/* Hacking root & branch into one param. */
static struct branch_clk tsif_ref_clk = {
	.b = {
		.ctl_reg = TSIF_NS_REG,
		.en_mask = BIT(9)|BIT(11),
		.halt_reg = CLK_HALT_STATEB_REG,
		.halt_bit = 11,
		.reset_mask = P_TSIF_REF_CLK,
	},
	.parent = &tv_clk.c,
	.c = {
		.dbg_name = "tsif_ref_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(tsif_ref_clk.c),
	},
};

static struct clk_freq_tbl clk_tbl_usb[] = {
	F_MND8(       0,  0,  0, gnd,  1,  0,  0,  NONE),
	F_MND8(60000000, 23, 16, pll1, 2,  5,  32, NOMINAL),
	F_END,
};

static struct rcg_clk usb_hs_src_clk = {
	.ns_reg = USBH_NS_REG,
	.b = {
		.ctl_reg = USBH_NS_REG,
		.halt_check = NOCHECK,
	},
	.md_reg = USBH_NS_REG - 4,
	.ns_mask = F_MASK_MND8(23, 16),
	.root_en_mask = BIT(11),
	.freq_tbl = clk_tbl_usb,
	.current_freq = &local_dummy_freq,
	.depends = &axi_li_adsp_a_clk.c,
	.set_rate = set_rate_mnd,
	.c = {
		.dbg_name = "usb_hs_src_clk",
		.ops = &soc_clk_ops_7x30,
		CLK_INIT(usb_hs_src_clk.c),
	},
};

static struct branch_clk usb_hs_clk = {
	.b = {
		.ctl_reg = USBH_NS_REG,
		.en_mask = BIT(9),
		.halt_reg = CLK_HALT_STATEB_REG,
		.halt_bit = 26,
		.reset_mask = P_USB_HS_CLK,
	},
	.c = {
		.dbg_name = "usb_hs_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(usb_hs_clk.c),
	},
};

static struct branch_clk usb_hs_core_clk = {
	.b = {
		.ctl_reg = USBH_NS_REG,
		.en_mask = BIT(13),
		.halt_reg = CLK_HALT_STATEA_REG,
		.halt_bit = 27,
		.reset_mask = P_USB_HS_CORE_CLK,
	},
	.parent = &usb_hs_src_clk.c,
	.c = {
		.dbg_name = "usb_hs_core_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(usb_hs_core_clk.c),
	},
};

static struct branch_clk usb_hs2_clk = {
	.b = {
		.ctl_reg = USBH2_NS_REG,
		.en_mask = BIT(9),
		.halt_reg = CLK_HALT_STATEB_REG,
		.halt_bit = 3,
		.reset_mask = P_USB_HS2_CLK,
	},
	.parent = &usb_hs_src_clk.c,
	.c = {
		.dbg_name = "usb_hs2_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(usb_hs2_clk.c),
	},
};

static struct branch_clk usb_hs2_core_clk = {
	.b = {
		.ctl_reg = USBH2_NS_REG,
		.en_mask = BIT(4),
		.halt_reg = CLK_HALT_STATEA_REG,
		.halt_bit = 28,
		.reset_mask = P_USB_HS2_CORE_CLK,
	},
	.parent = &usb_hs_src_clk.c,
	.c = {
		.dbg_name = "usb_hs2_core_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(usb_hs2_core_clk.c),
	},
};

static struct branch_clk usb_hs3_clk = {
	.b = {
		.ctl_reg = USBH3_NS_REG,
		.en_mask = BIT(9),
		.halt_reg = CLK_HALT_STATEB_REG,
		.halt_bit = 2,
		.reset_mask = P_USB_HS3_CLK,
	},
	.parent = &usb_hs_src_clk.c,
	.c = {
		.dbg_name = "usb_hs3_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(usb_hs3_clk.c),
	},
};

static struct branch_clk usb_hs3_core_clk = {
	.b = {
		.ctl_reg = USBH3_NS_REG,
		.en_mask = BIT(4),
		.halt_reg = CLK_HALT_STATEA_REG,
		.halt_bit = 29,
		.reset_mask = P_USB_HS3_CORE_CLK,
	},
	.parent = &usb_hs_src_clk.c,
	.c = {
		.dbg_name = "usb_hs3_core_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(usb_hs3_core_clk.c),
	},
};

static struct clk_freq_tbl clk_tbl_vfe_jpeg[] = {
	F_MND16( 24576000, lpxo, 1,   0,   0, NOMINAL),
	F_MND16( 36864000, pll3, 4,   1,   5, NOMINAL),
	F_MND16( 46080000, pll3, 4,   1,   4, NOMINAL),
	F_MND16( 61440000, pll3, 4,   1,   3, NOMINAL),
	F_MND16( 73728000, pll3, 2,   1,   5, NOMINAL),
	F_MND16( 81920000, pll3, 3,   1,   3, NOMINAL),
	F_MND16( 92160000, pll3, 4,   1,   2, NOMINAL),
	F_MND16( 98304000, pll3, 3,   2,   5, NOMINAL),
	F_MND16(105326000, pll3, 2,   2,   7, NOMINAL),
	F_MND16(122880000, pll3, 2,   1,   3, NOMINAL),
	F_MND16(147456000, pll3, 2,   2,   5, NOMINAL),
	F_MND16(153600000, pll1, 2,   2,   5, NOMINAL),
	F_MND16(192000000, pll1, 4,   0,   0, HIGH),
	F_END,
};

static struct rcg_clk jpeg_clk = {
	.b = {
		.ctl_reg = JPEG_NS_REG,
		.en_mask = BIT(9),
		.halt_reg = CLK_HALT_STATEB_REG,
		.halt_bit = 1,
		.reset_mask = P_JPEG_CLK,
	},
	.ns_reg = JPEG_NS_REG,
	.md_reg = JPEG_NS_REG - 4,
	.root_en_mask = BIT(11),
	.freq_tbl = clk_tbl_vfe_jpeg,
	.ns_mask = F_MASK_MND16,
	.set_rate = set_rate_mnd,
	.depends = &axi_li_jpeg_clk.c,
	.current_freq = &local_dummy_freq,
	.c = {
		.dbg_name = "jpeg_clk",
		.ops = &soc_clk_ops_7x30,
		CLK_INIT(jpeg_clk.c),
	},
};

static struct rcg_clk vfe_clk = {
	.b = {
		.ctl_reg = CAM_VFE_NS_REG,
		.en_mask = BIT(9),
		.halt_reg = CLK_HALT_STATEB_REG,
		.halt_bit = 0,
		.reset_mask = P_VFE_CLK,
	},
	.ns_reg = CAM_VFE_NS_REG,
	.md_reg = CAM_VFE_NS_REG - 4,
	.root_en_mask = BIT(13),
	.freq_tbl = clk_tbl_vfe_jpeg,
	.ns_mask = F_MASK_MND16,
	.set_rate = set_rate_mnd,
	.depends = &axi_li_vfe_clk.c,
	.current_freq = &local_dummy_freq,
	.c = {
		.dbg_name = "vfe_clk",
		.ops = &soc_clk_ops_7x30,
		CLK_INIT(vfe_clk.c),
	},
};

static struct branch_clk vfe_mdc_clk = {
	.b = {
		.ctl_reg = CAM_VFE_NS_REG,
		.en_mask = BIT(11),
		.halt_reg = CLK_HALT_STATEA_REG,
		.halt_bit = 9,
		.reset_mask = P_VFE_MDC_CLK,
	},
	.parent = &vfe_clk.c,
	.c = {
		.dbg_name = "vfe_mdc_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(vfe_mdc_clk.c),
	},
};

static struct branch_clk vfe_camif_clk = {
	.b = {
		.ctl_reg = CAM_VFE_NS_REG,
		.en_mask = BIT(15),
		.halt_reg = CLK_HALT_STATEC_REG,
		.halt_bit = 13,
		.reset_mask = P_VFE_CAMIF_CLK,
	},
	.parent = &vfe_clk.c,
	.c = {
		.dbg_name = "vfe_camif_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(vfe_camif_clk.c),
	},
};

static struct branch_clk csi0_vfe_clk = {
	.b = {
		.ctl_reg = CSI_NS_REG,
		.en_mask = BIT(15),
		.halt_reg = CLK_HALT_STATEC_REG,
		.halt_bit = 16,
		.reset_mask = P_CSI0_VFE_CLK,
	},
	.parent = &vfe_clk.c,
	.c = {
		.dbg_name = "csi0_vfe_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(csi0_vfe_clk.c),
	},
};

static struct clk_freq_tbl clk_tbl_cam[] = {
	F_MND16(       0, gnd,  1,   0,   0, NONE),
	F_MND16( 6000000, pll1, 4,   1,  32, NOMINAL),
	F_MND16( 8000000, pll1, 4,   1,  24, NOMINAL),
	F_MND16(12000000, pll1, 4,   1,  16, NOMINAL),
	F_MND16(16000000, pll1, 4,   1,  12, NOMINAL),
	F_MND16(19200000, pll1, 4,   1,  10, NOMINAL),
	F_MND16(24000000, pll1, 4,   1,   8, NOMINAL),
	F_MND16(32000000, pll1, 4,   1,   6, NOMINAL),
	F_MND16(48000000, pll1, 4,   1,   4, NOMINAL),
	F_MND16(64000000, pll1, 4,   1,   3, NOMINAL),
	F_END,
};

static struct rcg_clk cam_m_clk = {
	.b = {
		.ctl_reg = CAM_NS_REG,
		.halt_check = DELAY,
		.reset_mask = P_CAM_M_CLK,
	},
	.ns_reg = CAM_NS_REG,
	.md_reg = CAM_NS_REG - 4,
	.root_en_mask = BIT(9),
	.freq_tbl = clk_tbl_cam,
	.ns_mask = F_MASK_MND16,
	.set_rate = set_rate_mnd,
	.current_freq = &local_dummy_freq,
	.c = {
		.dbg_name = "cam_m_clk",
		.ops = &soc_clk_ops_7x30,
		CLK_INIT(cam_m_clk.c),
	},
};

static struct clk_freq_tbl clk_tbl_vpe[] = {
	F_MND8( 24576000, 22, 15, lpxo, 1,   0,   0, NOMINAL),
	F_MND8( 30720000, 22, 15, pll3, 4,   1,   6, NOMINAL),
	F_MND8( 61440000, 22, 15, pll3, 4,   1,   3, NOMINAL),
	F_MND8( 81920000, 22, 15, pll3, 3,   1,   3, NOMINAL),
	F_MND8(122880000, 22, 15, pll3, 3,   1,   2, NOMINAL),
	F_MND8(147456000, 22, 15, pll3, 1,   1,   5, NOMINAL),
	F_MND8(153600000, 22, 15, pll1, 1,   1,   5, NOMINAL),
	F_END,
};

static struct rcg_clk vpe_clk = {
	.b = {
		.ctl_reg = VPE_NS_REG,
		.en_mask = BIT(9),
		.halt_reg = CLK_HALT_STATEC_REG,
		.halt_bit = 10,
		.reset_mask = P_VPE_CLK,
	},
	.ns_reg = VPE_NS_REG,
	.md_reg = VPE_NS_REG - 4,
	.ns_mask = F_MASK_MND8(22, 15),
	.root_en_mask = BIT(11),
	.freq_tbl = clk_tbl_vpe,
	.current_freq = &local_dummy_freq,
	.depends = &axi_vpe_clk.c,
	.set_rate = set_rate_mnd,
	.c = {
		.dbg_name = "vpe_clk",
		.ops = &soc_clk_ops_7x30,
		CLK_INIT(vpe_clk.c),
	},
};


static struct clk_freq_tbl clk_tbl_mfc[] = {
	F_MND8( 24576000, 24, 17, lpxo, 1,   0,   0, NOMINAL),
	F_MND8( 30720000, 24, 17, pll3, 4,   1,   6, NOMINAL),
	F_MND8( 61440000, 24, 17, pll3, 4,   1,   3, NOMINAL),
	F_MND8( 81920000, 24, 17, pll3, 3,   1,   3, NOMINAL),
	F_MND8(122880000, 24, 17, pll3, 3,   1,   2, NOMINAL),
	F_MND8(147456000, 24, 17, pll3, 1,   1,   5, NOMINAL),
	F_MND8(153600000, 24, 17, pll1, 1,   1,   5, NOMINAL),
	F_MND8(170667000, 24, 17, pll1, 1,   2,   9, NOMINAL),
	F_END,
};

static struct rcg_clk mfc_clk = {
	.b = {
		.ctl_reg = MFC_NS_REG,
		.en_mask = BIT(9),
		.halt_reg = CLK_HALT_STATEC_REG,
		.halt_bit = 12,
		.reset_mask = P_MFC_CLK,
	},
	.ns_reg = MFC_NS_REG,
	.md_reg = MFC_NS_REG - 4,
	.ns_mask = F_MASK_MND8(24, 17),
	.root_en_mask = BIT(11),
	.freq_tbl = clk_tbl_mfc,
	.current_freq = &local_dummy_freq,
	.depends = &axi_mfc_clk.c,
	.set_rate = set_rate_mnd,
	.c = {
		.dbg_name = "mfc_clk",
		.ops = &soc_clk_ops_7x30,
		CLK_INIT(mfc_clk.c),
	},
};

static struct branch_clk mfc_div2_clk = {
	.b = {
		.ctl_reg = MFC_NS_REG,
		.en_mask = BIT(15),
		.halt_reg = CLK_HALT_STATEC_REG,
		.halt_bit = 11,
		.reset_mask = P_MFC_DIV2_CLK,
	},
	.parent = &mfc_clk.c,
	.c = {
		.dbg_name = "mfc_div2_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mfc_div2_clk.c),
	},
};

static struct clk_freq_tbl clk_tbl_spi[] = {
	F_MND8(       0,  0,  0, gnd,  1,   0,     0, NONE),
	F_MND8( 9963243, 19, 12, pll3, 4,   2,    37, NOMINAL),
	F_MND8(26331429, 19, 12, pll3, 4,   1,     7, NOMINAL),
	F_END,
};

static struct rcg_clk spi_clk = {
	.b = {
		.ctl_reg = SPI_NS_REG,
		.en_mask = BIT(9),
		.halt_reg = CLK_HALT_STATEC_REG,
		.halt_bit = 0,
		.reset_mask = P_SPI_CLK,
	},
	.ns_reg = SPI_NS_REG,
	.md_reg = SPI_NS_REG - 4,
	.ns_mask = F_MASK_MND8(19, 12),
	.root_en_mask = BIT(11),
	.freq_tbl = clk_tbl_spi,
	.current_freq = &local_dummy_freq,
	.set_rate = set_rate_mnd,
	.c = {
		.dbg_name = "spi_clk",
		.ops = &soc_clk_ops_7x30,
		CLK_INIT(spi_clk.c),
	},
};

static struct clk_freq_tbl clk_tbl_lpa_codec[] = {
	F_RAW(1, NULL, 0, 0, 0, 0, LOW, NULL), /* src MI2S_CODEC_RX */
	F_RAW(2, NULL, 0, 1, 0, 0, LOW, NULL), /* src ECODEC_CIF */
	F_RAW(3, NULL, 0, 2, 0, 0, LOW, NULL), /* src MI2S */
	F_RAW(4, NULL, 0, 3, 0, 0, LOW, NULL), /* src SDAC */
	F_END,
};

static struct rcg_clk lpa_codec_clk = {
	.b = {
		.ctl_reg = LPA_NS_REG,
		.en_mask = BIT(9),
		.halt_reg = CLK_HALT_STATEC_REG,
		.halt_bit = 6,
		.reset_mask = P_LPA_CODEC_CLK,
	},
	.ns_reg = LPA_NS_REG,
	.ns_mask = BM(1, 0),
	.set_rate = set_rate_nop,
	.freq_tbl = clk_tbl_lpa_codec,
	.current_freq = &local_dummy_freq,
	.c = {
		.dbg_name = "lpa_codec_clk",
		.ops = &soc_clk_ops_7x30,
		CLK_INIT(lpa_codec_clk.c),
	},
};

static struct clk_freq_tbl clk_tbl_mdc[] = {
	F_RAW(1, NULL, 0, 0, 0, 0, LOW, NULL),
	F_END
};

static struct rcg_clk mdc_clk = {
	.b = {
		.ctl_reg = MDC_NS_REG,
		.en_mask = BIT(9),
		.halt_reg = CLK_HALT_STATEA_REG,
		.halt_bit = 10,
		.reset_mask = P_MDC_CLK,
	},
	.ns_reg = MDC_NS_REG,
	.root_en_mask = BIT(11),
	.freq_tbl = clk_tbl_mdc,
	.current_freq = &local_dummy_freq,
	.set_rate = set_rate_nop,
	.c = {
		.dbg_name = "mdc_clk",
		.ops = &soc_clk_ops_7x30,
		CLK_INIT(mdc_clk.c),
	},
};

static struct branch_clk lpa_core_clk = {
	.b = {
		.ctl_reg = LPA_NS_REG,
		.en_mask = BIT(5),
		.halt_reg = CLK_HALT_STATEC_REG,
		.halt_bit = 5,
		.reset_mask = P_LPA_CORE_CLK,
	},
	.c = {
		.dbg_name = "lpa_core_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(lpa_core_clk.c),
	},
};

static DEFINE_CLK_PCOM(adsp_clk, ADSP_CLK, CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(codec_ssbi_clk,	CODEC_SSBI_CLK, CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(ebi1_clk, EBI1_CLK, CLKFLAG_SKIP_AUTO_OFF | CLKFLAG_MIN);
static DEFINE_CLK_PCOM(ebi1_fixed_clk, EBI1_FIXED_CLK, CLKFLAG_MIN |
						       CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(ecodec_clk, ECODEC_CLK, CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(gp_clk, GP_CLK, CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(uart3_clk, UART3_CLK, 0);
static DEFINE_CLK_PCOM(usb_phy_clk, USB_PHY_CLK, CLKFLAG_MIN);

static DEFINE_CLK_PCOM(p_grp_2d_clk, GRP_2D_CLK, CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(p_grp_2d_p_clk, GRP_2D_P_CLK, CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(p_hdmi_clk, HDMI_CLK, CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(p_jpeg_clk, JPEG_CLK, 0);
static DEFINE_CLK_PCOM(p_jpeg_p_clk, JPEG_P_CLK, 0);
static DEFINE_CLK_PCOM(p_lpa_codec_clk, LPA_CODEC_CLK, CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(p_lpa_core_clk, LPA_CORE_CLK, CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(p_lpa_p_clk, LPA_P_CLK, CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(p_mi2s_m_clk, MI2S_M_CLK, CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(p_mi2s_s_clk, MI2S_S_CLK, CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(p_mi2s_codec_rx_m_clk, MI2S_CODEC_RX_M_CLK,
		CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(p_mi2s_codec_rx_s_clk, MI2S_CODEC_RX_S_CLK,
		CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(p_mi2s_codec_tx_m_clk, MI2S_CODEC_TX_M_CLK,
		CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(p_mi2s_codec_tx_s_clk, MI2S_CODEC_TX_S_CLK,
		CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(p_sdac_clk, SDAC_CLK, 0);
static DEFINE_CLK_PCOM(p_sdac_m_clk, SDAC_M_CLK, 0);
static DEFINE_CLK_PCOM(p_vfe_clk, VFE_CLK, CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(p_vfe_camif_clk, VFE_CAMIF_CLK, CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(p_vfe_mdc_clk, VFE_MDC_CLK, CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(p_vfe_p_clk, VFE_P_CLK, 0);
static DEFINE_CLK_PCOM(p_grp_3d_clk, GRP_3D_CLK, CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(p_grp_3d_p_clk, GRP_3D_P_CLK, CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(p_imem_clk, IMEM_CLK, 0);
static DEFINE_CLK_PCOM(p_mdp_lcdc_pad_pclk_clk, MDP_LCDC_PAD_PCLK_CLK,
		CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(p_mdp_lcdc_pclk_clk, MDP_LCDC_PCLK_CLK,
		CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(p_mdp_p_clk, MDP_P_CLK, CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(p_mdp_vsync_clk, MDP_VSYNC_CLK, 0);
static DEFINE_CLK_PCOM(p_tsif_ref_clk, TSIF_REF_CLK, CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(p_tsif_p_clk, TSIF_P_CLK, CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(p_tv_dac_clk, TV_DAC_CLK, CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(p_tv_enc_clk, TV_ENC_CLK, CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(p_emdh_clk, EMDH_CLK, CLKFLAG_MIN | CLKFLAG_MAX);
static DEFINE_CLK_PCOM(p_emdh_p_clk, EMDH_P_CLK, 0);
static DEFINE_CLK_PCOM(p_i2c_clk, I2C_CLK, CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(p_i2c_2_clk, I2C_2_CLK, CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(p_mdc_clk, MDC_CLK, CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(p_pmdh_clk, PMDH_CLK, CLKFLAG_MIN | CLKFLAG_MAX);
static DEFINE_CLK_PCOM(p_pmdh_p_clk, PMDH_P_CLK, CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(p_sdc1_clk, SDC1_CLK, 0);
static DEFINE_CLK_PCOM(p_sdc1_p_clk, SDC1_P_CLK, 0);
static DEFINE_CLK_PCOM(p_sdc2_clk, SDC2_CLK, 0);
static DEFINE_CLK_PCOM(p_sdc2_p_clk, SDC2_P_CLK, 0);
static DEFINE_CLK_PCOM(p_sdc3_clk, SDC3_CLK, 0);
static DEFINE_CLK_PCOM(p_sdc3_p_clk, SDC3_P_CLK, 0);
static DEFINE_CLK_PCOM(p_sdc4_clk, SDC4_CLK, 0);
static DEFINE_CLK_PCOM(p_sdc4_p_clk, SDC4_P_CLK, 0);
static DEFINE_CLK_PCOM(p_uart2_clk, UART2_CLK, CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(p_usb_hs2_clk, USB_HS2_CLK, 0);
static DEFINE_CLK_PCOM(p_usb_hs2_core_clk, USB_HS2_CORE_CLK, 0);
static DEFINE_CLK_PCOM(p_usb_hs2_p_clk, USB_HS2_P_CLK, 0);
static DEFINE_CLK_PCOM(p_usb_hs3_clk, USB_HS3_CLK, 0);
static DEFINE_CLK_PCOM(p_usb_hs3_core_clk, USB_HS3_CORE_CLK, 0);
static DEFINE_CLK_PCOM(p_usb_hs3_p_clk, USB_HS3_P_CLK, 0);
static DEFINE_CLK_PCOM(p_qup_i2c_clk, QUP_I2C_CLK, CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(p_spi_clk, SPI_CLK, CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(p_spi_p_clk, SPI_P_CLK, CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(p_uart1_clk, UART1_CLK, 0);
static DEFINE_CLK_PCOM(p_uart1dm_clk, UART1DM_CLK, 0);
static DEFINE_CLK_PCOM(p_uart2dm_clk, UART2DM_CLK, CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(p_usb_hs_clk, USB_HS_CLK, 0);
static DEFINE_CLK_PCOM(p_usb_hs_core_clk, USB_HS_CORE_CLK, 0);
static DEFINE_CLK_PCOM(p_usb_hs_p_clk, USB_HS_P_CLK, 0);
static DEFINE_CLK_PCOM(p_cam_m_clk, CAM_M_CLK, CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(p_camif_pad_p_clk, CAMIF_PAD_P_CLK, 0);
static DEFINE_CLK_PCOM(p_csi0_clk, CSI0_CLK, CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(p_csi0_vfe_clk, CSI0_VFE_CLK, CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(p_csi0_p_clk, CSI0_P_CLK, CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(p_mdp_clk, MDP_CLK, 0);
static DEFINE_CLK_PCOM(p_mfc_clk, MFC_CLK, CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(p_mfc_div2_clk, MFC_DIV2_CLK, CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(p_mfc_p_clk, MFC_P_CLK, CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(p_vpe_clk, VPE_CLK, CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(p_adm_clk, ADM_CLK, CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(p_ce_clk, CE_CLK, CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(p_axi_rotator_clk, AXI_ROTATOR_CLK,
		CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(p_rotator_imem_clk, ROTATOR_IMEM_CLK, 0);
static DEFINE_CLK_PCOM(p_rotator_p_clk, ROTATOR_P_CLK, 0);

static DEFINE_CLK_VOTER(ebi_dtv_clk, &ebi1_fixed_clk.c);
static DEFINE_CLK_VOTER(ebi_kgsl_clk, &ebi1_fixed_clk.c);
static DEFINE_CLK_VOTER(ebi_lcdc_clk, &ebi1_fixed_clk.c);
static DEFINE_CLK_VOTER(ebi_mddi_clk, &ebi1_fixed_clk.c);
static DEFINE_CLK_VOTER(ebi_tv_clk, &ebi1_fixed_clk.c);
static DEFINE_CLK_VOTER(ebi_vcd_clk, &ebi1_fixed_clk.c);
static DEFINE_CLK_VOTER(ebi_vfe_clk, &ebi1_fixed_clk.c);
static DEFINE_CLK_VOTER(ebi_adm_clk, &ebi1_fixed_clk.c);

/*
 * SoC-specific functions required by clock-local driver
 */

/* Update the sys_vdd voltage given a level. */
static int msm7x30_update_sys_vdd(enum sys_vdd_level level)
{
	int rc, target_mv;
	static const int mv[NUM_SYS_VDD_LEVELS] = {
		[NONE...LOW] = 1000,
		[NOMINAL] = 1100,
		[HIGH]    = 1200,
	};

	target_mv = mv[level];
	rc = msm_proc_comm(PCOM_CLKCTL_RPC_MIN_MSMC1, &target_mv, NULL);
	if (rc)
		goto out;
	if (target_mv) {
		rc = -EINVAL;
		goto out;
	}
out:
	return rc;
}

#ifdef CONFIG_DEBUG_FS

#define CLK_TEST_2(s) (s)
#define CLK_TEST_HS(s) (0x4000 | ((s) << 8))
#define CLK_TEST_LS(s) (0x4D40 | (s))

struct measure_sel {
	u32 test_vector;
	struct clk *clk;
};

static struct measure_sel measure_mux[] = {
	{ CLK_TEST_2(0x03), &emdh_p_clk.c },
	{ CLK_TEST_2(0x04), &pmdh_p_clk.c },
	{ CLK_TEST_2(0x06), &mdp_p_clk.c },
	{ CLK_TEST_2(0x07), &lpa_p_clk.c },
	{ CLK_TEST_2(0x08), &usb_hs2_p_clk.c },
	{ CLK_TEST_2(0x09), &spi_clk.c },
	{ CLK_TEST_2(0x0A), &midi_clk.c },
	{ CLK_TEST_2(0x0B), &i2c_2_clk.c },
	{ CLK_TEST_2(0x0D), &mi2s_m_clk.c },
	{ CLK_TEST_2(0x0E), &lpa_core_clk.c },
	{ CLK_TEST_2(0x0F), &lpa_codec_clk.c },
	{ CLK_TEST_2(0x10), &usb_hs3_p_clk.c },
	{ CLK_TEST_2(0x11), &adm_p_clk.c },
	{ CLK_TEST_2(0x13), &hdmi_clk.c },
	{ CLK_TEST_2(0x14), &usb_hs_core_clk.c },
	{ CLK_TEST_2(0x15), &usb_hs2_core_clk.c },
	{ CLK_TEST_2(0x16), &usb_hs3_core_clk.c },
	{ CLK_TEST_2(0x17), &mi2s_codec_tx_s_clk.c },
	{ CLK_TEST_2(0x18), &spi_p_clk.c },
	{ CLK_TEST_2(0x1A), &camif_pad_p_clk.c },
	{ CLK_TEST_2(0x1C), &qup_i2c_clk.c },
	{ CLK_TEST_2(0x1F), &mfc_div2_clk.c },
	{ CLK_TEST_2(0x38), &mfc_clk.c },

	{ CLK_TEST_HS(0x00), &adm_clk.c },
	{ CLK_TEST_HS(0x01), &mdp_lcdc_pad_pclk_clk.c },
	{ CLK_TEST_HS(0x02), &mdp_lcdc_pclk_clk.c },
	{ CLK_TEST_HS(0x03), &axi_rotator_clk.c },
	{ CLK_TEST_HS(0x07), &axi_li_vg_clk.c },
	{ CLK_TEST_HS(0x09), &axi_li_apps_clk.c },
	{ CLK_TEST_HS(0x0E), &axi_li_jpeg_clk.c },
	{ CLK_TEST_HS(0x0F), &emdh_clk.c },
	{ CLK_TEST_HS(0x14), &mdp_clk.c },
	{ CLK_TEST_HS(0x15), &pmdh_clk.c },
	{ CLK_TEST_HS(0x19), &axi_grp_2d_clk.c },
	{ CLK_TEST_HS(0x1A), &axi_li_grp_clk.c },
	{ CLK_TEST_HS(0x1B), &axi_li_vfe_clk.c },
	{ CLK_TEST_HS(0x1C), &grp_2d_clk.c },
	{ CLK_TEST_HS(0x1E), &grp_3d_clk.c },
	{ CLK_TEST_HS(0x1F), &imem_clk.c },
	{ CLK_TEST_HS(0x20), &jpeg_clk.c },
	{ CLK_TEST_HS(0x24), &axi_li_adsp_a_clk.c },
	{ CLK_TEST_HS(0x26), &rotator_imem_clk.c },
	{ CLK_TEST_HS(0x27), &axi_vpe_clk.c },
	{ CLK_TEST_HS(0x2A), &axi_mfc_clk.c },
	{ CLK_TEST_HS(0x2B), &axi_mdp_clk.c },
	{ CLK_TEST_HS(0x2C), &vpe_clk.c },
	{ CLK_TEST_HS(0x30), &vfe_camif_clk.c },
	{ CLK_TEST_HS(0x31), &csi0_clk.c },
	{ CLK_TEST_HS(0x32), &csi0_vfe_clk.c },
	{ CLK_TEST_HS(0x33), &csi0_p_clk.c },

	{ CLK_TEST_LS(0x03), &ce_clk.c },
	{ CLK_TEST_LS(0x04), &cam_m_clk.c },
	{ CLK_TEST_LS(0x0C), &grp_2d_p_clk.c },
	{ CLK_TEST_LS(0x0D), &i2c_clk.c },
	{ CLK_TEST_LS(0x0E), &mi2s_codec_rx_m_clk.c },
	{ CLK_TEST_LS(0x0F), &mi2s_codec_rx_s_clk.c },
	{ CLK_TEST_LS(0x10), &mi2s_codec_tx_m_clk.c },
	{ CLK_TEST_LS(0x13), &mdp_vsync_clk.c },
	{ CLK_TEST_LS(0x15), &vfe_p_clk.c },
	{ CLK_TEST_LS(0x16), &mdc_clk.c },
	{ CLK_TEST_LS(0x17), &vfe_mdc_clk.c },
	{ CLK_TEST_LS(0x18), &usb_hs_p_clk.c },
	{ CLK_TEST_LS(0x1C), &uart1dm_p_clk.c },
	{ CLK_TEST_LS(0x1E), &jpeg_p_clk.c },
	{ CLK_TEST_LS(0x20), &sdac_clk.c },
	{ CLK_TEST_LS(0x21), &sdc1_p_clk.c },
	{ CLK_TEST_LS(0x22), &sdc1_clk.c },
	{ CLK_TEST_LS(0x23), &sdc2_p_clk.c },
	{ CLK_TEST_LS(0x24), &sdc2_clk.c },
	{ CLK_TEST_LS(0x25), &tsif_p_clk.c },
	{ CLK_TEST_LS(0x26), &sdac_m_clk.c },
	{ CLK_TEST_LS(0x27), &grp_3d_p_clk.c },
	{ CLK_TEST_LS(0x2A), &tsif_ref_clk.c },
	{ CLK_TEST_LS(0x2B), &tv_enc_clk.c },
	{ CLK_TEST_LS(0x2C), &tv_dac_clk.c },
	{ CLK_TEST_LS(0x2D), &rotator_p_clk.c },
	{ CLK_TEST_LS(0x2F), &uart1_clk.c },
	{ CLK_TEST_LS(0x30), &uart1dm_clk.c },
	{ CLK_TEST_LS(0x31), &uart2_clk.c },
	{ CLK_TEST_LS(0x33), &usb_hs2_clk.c },
	{ CLK_TEST_LS(0x34), &usb_hs3_clk.c },
	{ CLK_TEST_LS(0x35), &mfc_p_clk.c },
	{ CLK_TEST_LS(0x36), &vfe_clk.c },
	{ CLK_TEST_LS(0x39), &sdc3_p_clk.c },
	{ CLK_TEST_LS(0x3A), &sdc3_clk.c },
	{ CLK_TEST_LS(0x3B), &sdc4_p_clk.c },
	{ CLK_TEST_LS(0x3C), &sdc4_clk.c },
	{ CLK_TEST_LS(0x3D), &uart2dm_clk.c },
	{ CLK_TEST_LS(0x3E), &uart2dm_p_clk.c },
	{ CLK_TEST_LS(0x3F), &usb_hs_clk.c },
};

static struct measure_sel *find_measure_sel(struct clk *clk)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(measure_mux); i++)
		if (measure_mux[i].clk == clk)
			return &measure_mux[i];
	return NULL;
}

static int measure_clk_set_parent(struct clk *clk, struct clk *parent)
{
	struct measure_sel *p;
	unsigned long flags;

	if (!parent)
		return -EINVAL;

	p = find_measure_sel(parent);
	if (!p)
		return -EINVAL;

	spin_lock_irqsave(&local_clock_reg_lock, flags);

	/* Program test vector. */
	if (p->test_vector <= 0xFF) {
		/* Select CLK_TEST_2 */
		writel_relaxed(0x4D40, CLK_TEST_BASE_REG);
		writel_relaxed(p->test_vector, CLK_TEST_2_BASE_REG);
	} else
		writel_relaxed(p->test_vector, CLK_TEST_BASE_REG);

	spin_unlock_irqrestore(&local_clock_reg_lock, flags);

	return 0;
}

/* Sample clock for 'tcxo4_ticks' reference clock ticks. */
static u32 run_measurement(unsigned tcxo4_ticks)
{
	/* TCXO4_CNT_EN and RINGOSC_CNT_EN register values. */
	u32 reg_val_enable = readl_relaxed(MISC_CLK_CTL_BASE_REG) | 0x3;
	u32 reg_val_disable = reg_val_enable & ~0x3;

	/* Stop counters and set the TCXO4 counter start value. */
	writel_relaxed(reg_val_disable, MISC_CLK_CTL_BASE_REG);
	writel_relaxed(tcxo4_ticks, TCXO_CNT_BASE_REG);

	/* Run measurement and wait for completion. */
	writel_relaxed(reg_val_enable, MISC_CLK_CTL_BASE_REG);
	while (readl_relaxed(TCXO_CNT_DONE_BASE_REG) == 0)
		cpu_relax();

	/* Stop counters. */
	writel_relaxed(reg_val_disable, MISC_CLK_CTL_BASE_REG);

	return readl_relaxed(RINGOSC_CNT_BASE_REG);
}

/* Perform a hardware rate measurement for a given clock.
   FOR DEBUG USE ONLY: Measurements take ~15 ms! */
static unsigned measure_clk_get_rate(struct clk *clk)
{
	unsigned long flags;
	u32 regval, prph_web_reg_old;
	u64 raw_count_short, raw_count_full;
	unsigned ret;

	clk_enable(&tcxo_clk.c);

	spin_lock_irqsave(&local_clock_reg_lock, flags);

	/* Enable TCXO4 clock branch and root. */
	prph_web_reg_old = readl_relaxed(PRPH_WEB_NS_BASE_REG);
	regval = prph_web_reg_old | BIT(9) | BIT(11);
	writel_relaxed(regval, PRPH_WEB_NS_BASE_REG);

	/*
	 * The ring oscillator counter will not reset if the measured clock
	 * is not running.  To detect this, run a short measurement before
	 * the full measurement.  If the raw results of the two are the same
	 * then the clock must be off.
	 */

	/* Run a short measurement. (~1 ms) */
	raw_count_short = run_measurement(0x1000);
	/* Run a full measurement. (~14 ms) */
	raw_count_full = run_measurement(0x10000);

	/* Disable TCXO4 clock branch and root. */
	writel_relaxed(prph_web_reg_old, PRPH_WEB_NS_BASE_REG);

	spin_unlock_irqrestore(&local_clock_reg_lock, flags);

	/* Return 0 if the clock is off. */
	if (raw_count_full == raw_count_short)
		ret = 0;
	else {
		/* Compute rate in Hz. */
		raw_count_full = ((raw_count_full * 10) + 15) * 4800000;
		do_div(raw_count_full, ((0x10000 * 10) + 35));
		ret = raw_count_full;
	}

	clk_disable(&tcxo_clk.c);

	return ret;
}
#else /* !CONFIG_DEBUG_FS */
static int measure_clk_set_parent(struct clk *clk, struct clk *parent)
{
	return -EINVAL;
}

static unsigned measure_clk_get_rate(struct clk *clk)
{
	return 0;
}
#endif /* CONFIG_DEBUG_FS */

static struct clk_ops measure_clk_ops = {
	.set_parent = measure_clk_set_parent,
	.get_rate = measure_clk_get_rate,
	.is_local = local_clk_is_local,
};

static struct clk measure_clk = {
	.dbg_name = "measure_clk",
	.ops = &measure_clk_ops,
	CLK_INIT(measure_clk),
};

/* Implementation for clk_set_flags(). */
int soc_clk_set_flags(struct clk *clk, unsigned clk_flags)
{
	uint32_t regval, ret = 0;
	unsigned long flags;

	spin_lock_irqsave(&local_clock_reg_lock, flags);

	if (clk == &vfe_clk.c) {
		regval = readl_relaxed(CAM_VFE_NS_REG);
		/* Flag values chosen for backward compatibility
		 * with proc_comm remote clock control. */
		if (clk_flags == 0x00000100) {
			/* Select external source. */
			regval |= BIT(14);
		} else if (clk_flags == 0x00000200) {
			/* Select internal source. */
			regval &= ~BIT(14);
		} else
			ret = -EINVAL;

		writel_relaxed(regval, CAM_VFE_NS_REG);
		/* Make sure write is issued before returning. */
		mb();
	} else
		ret = -EPERM;

	spin_unlock_irqrestore(&local_clock_reg_lock, flags);

	return ret;
}

static int msm7x30_clk_reset(struct clk *clk, enum clk_reset_action action)
{
	/* reset_mask is actually a proc_comm id */
	unsigned id = to_rcg_clk(clk)->b.reset_mask;
	return pc_clk_reset(id, action);
}

static int soc_branch_clk_reset(struct clk *clk, enum clk_reset_action action)
{
	unsigned id = to_branch_clk(clk)->b.reset_mask;
	return pc_clk_reset(id, action);
}

/*
 * Clock ownership detection code
 */

enum {
	SH2_OWN_GLBL,
	SH2_OWN_APPS1,
	SH2_OWN_APPS2,
	SH2_OWN_ROW1,
	SH2_OWN_ROW2,
	SH2_OWN_APPS3,
	NUM_OWNERSHIP
};
static __initdata uint32_t ownership_regs[NUM_OWNERSHIP];

static void __init cache_ownership(void)
{
	ownership_regs[SH2_OWN_GLBL]  = readl_relaxed(SH2_OWN_GLBL_BASE_REG);
	ownership_regs[SH2_OWN_APPS1] = readl_relaxed(SH2_OWN_APPS1_BASE_REG);
	ownership_regs[SH2_OWN_APPS2] = readl_relaxed(SH2_OWN_APPS2_BASE_REG);
	ownership_regs[SH2_OWN_ROW1]  = readl_relaxed(SH2_OWN_ROW1_BASE_REG);
	ownership_regs[SH2_OWN_ROW2]  = readl_relaxed(SH2_OWN_ROW2_BASE_REG);
	ownership_regs[SH2_OWN_APPS3] = readl_relaxed(SH2_OWN_APPS3_BASE_REG);
}

static void __init print_ownership(void)
{
	pr_info("Clock ownership\n");
	pr_info("  GLBL  : %08x\n", ownership_regs[SH2_OWN_GLBL]);
	pr_info("  APPS  : %08x %08x %08x\n", ownership_regs[SH2_OWN_APPS1],
		ownership_regs[SH2_OWN_APPS2], ownership_regs[SH2_OWN_APPS3]);
	pr_info("  ROW   : %08x %08x\n", ownership_regs[SH2_OWN_ROW1],
		ownership_regs[SH2_OWN_ROW2]);
}

#define O(x) (&ownership_regs[(SH2_OWN_##x)])
#define OWN(r, b, name, clk, dev) \
	{ \
		.lk = CLK_LOOKUP(name, clk.c, dev), \
		.remote = &p_##clk.c, \
		.reg = O(r), \
		.bit = BIT(b), \
	}

static struct clk_local_ownership {
	struct clk_lookup lk;
	const u32 *reg;
	const u32 bit;
	struct clk *remote;
} ownership_map[] __initdata = {
	/* Sources */
	{ CLK_LOOKUP("pll1_clk",	pll1_clk.c,	"acpu") },
	{ CLK_LOOKUP("pll2_clk",	pll2_clk.c,	"acpu") },
	{ CLK_LOOKUP("pll3_clk",	pll3_clk.c,	"acpu") },
	{ CLK_LOOKUP("measure",		measure_clk,	"debug") },

	/* PCOM */
	{ CLK_LOOKUP("adsp_clk",	adsp_clk.c,	NULL) },
	{ CLK_LOOKUP("codec_ssbi_clk",	codec_ssbi_clk.c,	NULL) },
	{ CLK_LOOKUP("ebi1_clk",	ebi1_clk.c,	NULL) },
	{ CLK_LOOKUP("ebi1_fixed_clk",	ebi1_fixed_clk.c,	NULL) },
	{ CLK_LOOKUP("ecodec_clk",	ecodec_clk.c,	NULL) },
	{ CLK_LOOKUP("gp_clk",		gp_clk.c,	NULL) },
	{ CLK_LOOKUP("uart_clk",	uart3_clk.c,	"msm_serial.2") },
	{ CLK_LOOKUP("usb_phy_clk",	usb_phy_clk.c,	NULL) },

	/* Voters */
	{ CLK_LOOKUP("ebi1_dtv_clk",	ebi_dtv_clk.c,	NULL) },
	{ CLK_LOOKUP("ebi1_kgsl_clk",	ebi_kgsl_clk.c,	NULL) },
	{ CLK_LOOKUP("ebi1_lcdc_clk",	ebi_lcdc_clk.c,	NULL) },
	{ CLK_LOOKUP("ebi1_mddi_clk",	ebi_mddi_clk.c,	NULL) },
	{ CLK_LOOKUP("ebi1_tv_clk",	ebi_tv_clk.c,	NULL) },
	{ CLK_LOOKUP("ebi1_vcd_clk",	ebi_vcd_clk.c,	NULL) },
	{ CLK_LOOKUP("ebi1_vfe_clk",	ebi_vfe_clk.c,	NULL) },
	{ CLK_LOOKUP("ebi1_clk",	ebi_adm_clk.c,	"msm_dmov") },

	/*
	 * This is a many-to-one mapping because we don't know how the remote
	 * clock code has decided to handle the dependencies between clocks for
	 * a particular hardware block. We determine the ownership for all the
	 * clocks going into a block by checking the ownership bit of one
	 * register (usually the ns register).
	 */
	OWN(APPS1,  6, "grp_2d_clk",	grp_2d_clk,	NULL),
	OWN(APPS1,  6, "grp_2d_pclk",	grp_2d_p_clk,	NULL),
	OWN(APPS1, 31, "hdmi_clk",	hdmi_clk,	NULL),
	OWN(APPS1,  0, "jpeg_clk",	jpeg_clk,	NULL),
	OWN(APPS1,  0, "jpeg_pclk",	jpeg_p_clk,	NULL),
	OWN(APPS1, 23, "lpa_codec_clk", lpa_codec_clk,	NULL),
	OWN(APPS1, 23, "lpa_core_clk",	lpa_core_clk,	NULL),
	OWN(APPS1, 23, "lpa_pclk",	lpa_p_clk,	NULL),
	OWN(APPS1, 28, "mi2s_m_clk",	mi2s_m_clk,	NULL),
	OWN(APPS1, 28, "mi2s_s_clk",	mi2s_s_clk,	NULL),
	OWN(APPS1, 12, "mi2s_codec_rx_m_clk", mi2s_codec_rx_m_clk, NULL),
	OWN(APPS1, 12, "mi2s_codec_rx_s_clk", mi2s_codec_rx_s_clk, NULL),
	OWN(APPS1, 14, "mi2s_codec_tx_m_clk", mi2s_codec_tx_m_clk, NULL),
	OWN(APPS1, 14, "mi2s_codec_tx_s_clk", mi2s_codec_tx_s_clk, NULL),
	{ CLK_LOOKUP("midi_clk",        midi_clk.c,     NULL),
		O(APPS1), BIT(22) },
	OWN(APPS1, 26, "sdac_clk",	sdac_clk,	NULL),
	OWN(APPS1, 26, "sdac_m_clk",	sdac_m_clk,	NULL),
	OWN(APPS1,  8, "vfe_clk",	vfe_clk,	NULL),
	OWN(APPS1,  8, "vfe_camif_clk", vfe_camif_clk,	NULL),
	OWN(APPS1,  8, "vfe_mdc_clk",	vfe_mdc_clk,	NULL),
	OWN(APPS1,  8, "vfe_pclk",	vfe_p_clk,	NULL),

	OWN(APPS2,  0, "grp_clk",	grp_3d_clk,	NULL),
	OWN(APPS2,  0, "grp_pclk",	grp_3d_p_clk,	NULL),
	{ CLK_LOOKUP("grp_src_clk",     grp_3d_src_clk.c, NULL),
		O(APPS2), BIT(0), &p_grp_3d_clk.c },
	OWN(APPS2,  0, "imem_clk",	imem_clk,	NULL),
	OWN(APPS2,  4, "mdp_lcdc_pad_pclk_clk", mdp_lcdc_pad_pclk_clk, NULL),
	OWN(APPS2,  4, "mdp_lcdc_pclk_clk", mdp_lcdc_pclk_clk, NULL),
	OWN(APPS2,  4, "mdp_pclk",	mdp_p_clk,	NULL),
	OWN(APPS2, 28, "mdp_vsync_clk", mdp_vsync_clk,	NULL),
	OWN(APPS2,  5, "tsif_ref_clk",	tsif_ref_clk,	NULL),
	OWN(APPS2,  5, "tsif_pclk",	tsif_p_clk,	NULL),
	{ CLK_LOOKUP("tv_src_clk",      tv_clk.c,       NULL),
		O(APPS2), BIT(2), &p_tv_enc_clk.c },
	OWN(APPS2,  2, "tv_dac_clk",	tv_dac_clk,	NULL),
	OWN(APPS2,  2, "tv_enc_clk",	tv_enc_clk,	NULL),

	OWN(ROW1,  7, "emdh_clk",	emdh_clk,	"msm_mddi.1"),
	OWN(ROW1,  7, "emdh_pclk",	emdh_p_clk,	"msm_mddi.1"),
	OWN(ROW1, 11, "i2c_clk",	i2c_clk,	"msm_i2c.0"),
	OWN(ROW1, 12, "i2c_clk",	i2c_2_clk,	"msm_i2c.2"),
	OWN(ROW1, 17, "mdc_clk",	mdc_clk,	NULL),
	OWN(ROW1, 19, "mddi_clk",	pmdh_clk,	NULL),
	OWN(ROW1, 19, "mddi_pclk",	pmdh_p_clk,	NULL),
	OWN(ROW1, 23, "sdc_clk",	sdc1_clk,	"msm_sdcc.1"),
	OWN(ROW1, 23, "sdc_pclk",	sdc1_p_clk,	"msm_sdcc.1"),
	OWN(ROW1, 25, "sdc_clk",	sdc2_clk,	"msm_sdcc.2"),
	OWN(ROW1, 25, "sdc_pclk",	sdc2_p_clk,	"msm_sdcc.2"),
	OWN(ROW1, 27, "sdc_clk",	sdc3_clk,	"msm_sdcc.3"),
	OWN(ROW1, 27, "sdc_pclk",	sdc3_p_clk,	"msm_sdcc.3"),
	OWN(ROW1, 29, "sdc_clk",	sdc4_clk,	"msm_sdcc.4"),
	OWN(ROW1, 29, "sdc_pclk",	sdc4_p_clk,	"msm_sdcc.4"),
	OWN(ROW1,  0, "uart_clk",	uart2_clk,	"msm_serial.1"),
	OWN(ROW1,  2, "usb_hs2_clk",	usb_hs2_clk,	NULL),
	OWN(ROW1,  2, "usb_hs2_core_clk", usb_hs2_core_clk, NULL),
	OWN(ROW1,  2, "usb_hs2_pclk",	usb_hs2_p_clk,	NULL),
	OWN(ROW1,  4, "usb_hs3_clk",	usb_hs3_clk,	NULL),
	OWN(ROW1,  4, "usb_hs3_core_clk", usb_hs3_core_clk, NULL),
	OWN(ROW1,  4, "usb_hs3_pclk",	usb_hs3_p_clk,	NULL),

	OWN(ROW2,  3, "qup_clk",	qup_i2c_clk,	"qup_i2c.4"),
	OWN(ROW2,  1, "spi_clk",	spi_clk,	NULL),
	OWN(ROW2,  1, "spi_pclk",	spi_p_clk,	NULL),
	OWN(ROW2,  9, "uart_clk",	uart1_clk,	"msm_serial.0"),
	OWN(ROW2,  6, "uartdm_clk",	uart1dm_clk,	"msm_serial_hs.0"),
	OWN(ROW2,  8, "uartdm_clk",	uart2dm_clk,	"msm_serial_hs.1"),
	OWN(ROW2, 11, "usb_hs_clk",	usb_hs_clk,	NULL),
	OWN(ROW2, 11, "usb_hs_core_clk", usb_hs_core_clk, NULL),
	OWN(ROW2, 11, "usb_hs_pclk",	usb_hs_p_clk,	NULL),

	OWN(APPS3,  6, "cam_m_clk",	cam_m_clk,	NULL),
	OWN(APPS3,  6, "camif_pad_pclk", camif_pad_p_clk, NULL),
	OWN(APPS3, 11, "csi_clk",	csi0_clk,	NULL),
	OWN(APPS3, 11, "csi_vfe_clk",	csi0_vfe_clk,	NULL),
	OWN(APPS3, 11, "csi_pclk",	csi0_p_clk,	NULL),
	OWN(APPS3,  0, "mdp_clk",	mdp_clk,	NULL),
	OWN(APPS3,  2, "mfc_clk",	mfc_clk,	NULL),
	OWN(APPS3,  2, "mfc_div2_clk",	mfc_div2_clk,	NULL),
	OWN(APPS3,  2, "mfc_pclk",	mfc_p_clk,	NULL),
	OWN(APPS3,  4, "vpe_clk",	vpe_clk,	NULL),

	OWN(GLBL,  8, "adm_clk",	adm_clk,	NULL),
	{ CLK_LOOKUP("adm_pclk", adm_p_clk.c,	NULL),
		O(GLBL), BIT(13), &dummy_clk },
	OWN(GLBL,  8, "ce_clk",	ce_clk,		NULL),
	OWN(GLBL, 13, "rotator_clk",	axi_rotator_clk, NULL),
	OWN(GLBL, 13, "rotator_imem_clk", rotator_imem_clk, NULL),
	OWN(GLBL, 13, "rotator_pclk",	rotator_p_clk,	NULL),
	{ CLK_LOOKUP("uartdm_pclk",     uart1dm_p_clk.c, "msm_serial_hs.0"),
		O(GLBL), BIT(8), &dummy_clk },
	{ CLK_LOOKUP("uartdm_pclk",     uart2dm_p_clk.c, "msm_serial_hs.1"),
		O(GLBL), BIT(8), &dummy_clk },
};

static struct clk_lookup msm_clocks_7x30[ARRAY_SIZE(ownership_map)];

static void __init set_clock_ownership(void)
{
	unsigned i;
	struct clk_lookup *lk;

	for (i = 0; i < ARRAY_SIZE(ownership_map); i++) {
		const u32 *reg = ownership_map[i].reg;
		u32 bit = ownership_map[i].bit;
		struct clk *remote = ownership_map[i].remote;

		lk = &ownership_map[i].lk;
		memcpy(&msm_clocks_7x30[i], lk, sizeof(*lk));

		if (reg && !(*reg & bit))
			msm_clocks_7x30[i].clk = remote;
	}
}

/*
 * Miscellaneous clock register initializations
 */
static const struct reg_init {
	const void __iomem *reg;
	uint32_t mask;
	uint32_t val;
} ri_list[] __initconst = {
	/* Enable UMDX_P clock. Known to causes issues, so never turn off. */
	{GLBL_CLK_ENA_2_SC_REG, BIT(2), BIT(2)},

	/* Disable all the child clocks of USB_HS_SRC. */
	{ USBH_NS_REG, BIT(13) | BIT(9), 0 },
	{ USBH2_NS_REG, BIT(9) | BIT(4), 0 },
	{ USBH3_NS_REG, BIT(9) | BIT(4), 0 },

	{EMDH_NS_REG, BM(18, 17) , BVAL(18, 17, 0x3)}, /* RX div = div-4. */
	{PMDH_NS_REG, BM(18, 17), BVAL(18, 17, 0x3)}, /* RX div = div-4. */
	/* MI2S_CODEC_RX_S src = MI2S_CODEC_RX_M. */
	{MI2S_RX_NS_REG, BIT(14), 0x0},
	/* MI2S_CODEC_TX_S src = MI2S_CODEC_TX_M. */
	{MI2S_TX_NS_REG, BIT(14), 0x0},
	{MI2S_NS_REG, BIT(14), 0x0}, /* MI2S_S src = MI2S_M. */
	/* Allow DSP to decide the LPA CORE src. */
	{LPA_CORE_CLK_MA0_REG, BIT(0), BIT(0)},
	{LPA_CORE_CLK_MA2_REG, BIT(0), BIT(0)},
	{MI2S_CODEC_RX_DIV_REG, 0xF, 0xD}, /* MI2S_CODEC_RX_S div = div-8. */
	{MI2S_CODEC_TX_DIV_REG, 0xF, 0xD}, /* MI2S_CODEC_TX_S div = div-8. */
	{MI2S_DIV_REG, 0xF, 0x7}, /* MI2S_S div = div-8. */
	{MDC_NS_REG, 0x3, 0x3}, /* MDC src = external MDH src. */
	{SDAC_NS_REG, BM(15, 14), 0x0}, /* SDAC div = div-1. */
	/* Disable sources TCXO/5 & TCXO/6. UART1 src = TCXO*/
	{UART_NS_REG, BM(26, 25) | BM(2, 0), 0x0},
	/* HDMI div = div-1, non-inverted. tv_enc_src = tv_clk_src */
	{HDMI_NS_REG, 0x7, 0x0},
	{TV_NS_REG, BM(15, 14), 0x0}, /* tv_clk_src_div2 = div-1 */

	/* USBH core clocks src = USB_HS_SRC. */
	{USBH_NS_REG, BIT(15), BIT(15)},
	{USBH2_NS_REG, BIT(6), BIT(6)},
	{USBH3_NS_REG, BIT(6), BIT(6)},
};

/* Local clock driver initialization. */
void __init msm7x30_clock_init(void)
{
	int i;
	uint32_t val;

	cache_ownership();
	print_ownership();
	set_clock_ownership();

	soc_update_sys_vdd = msm7x30_update_sys_vdd;

	/* When we have no local clock control, the rest of the code in this
	 * function is a NOP since writes to shadow regions that we don't own
	 * are ignored. */

	clk_set_rate(&usb_hs_src_clk.c, clk_tbl_usb[1].freq_hz);

	for (i = 0; i < ARRAY_SIZE(ri_list); i++) {
		val = readl_relaxed(ri_list[i].reg);
		val &= ~ri_list[i].mask;
		val |= ri_list[i].val;
		writel_relaxed(val, ri_list[i].reg);
	}

	clk_set_rate(&i2c_clk.c, 19200000);
	clk_set_rate(&i2c_2_clk.c, 19200000);
	clk_set_rate(&qup_i2c_clk.c, 19200000);
	clk_set_rate(&uart1_clk.c, 19200000);
	clk_set_rate(&uart2_clk.c, 19200000);
	clk_set_rate(&mi2s_m_clk.c, 12288000);
	clk_set_rate(&midi_clk.c, 98304000);
	clk_set_rate(&mdp_vsync_clk.c, 24576000);
	clk_set_rate(&glbl_root_clk.c, 1);
	clk_set_rate(&mdc_clk.c, 1);
	/* Sync the LPA_CODEC clock to MI2S_CODEC_RX */
	clk_set_rate(&lpa_codec_clk.c, 1);
	/* Sync the GRP2D clock to AXI */
	clk_set_rate(&grp_2d_clk.c, 1);

	msm_clock_init(msm_clocks_7x30, ARRAY_SIZE(msm_clocks_7x30));
}

/*
 * Clock operation handler registration
 */
static struct clk_ops soc_clk_ops_7x30 = {
	.enable = rcg_clk_enable,
	.disable = rcg_clk_disable,
	.auto_off = rcg_clk_auto_off,
	.set_rate = rcg_clk_set_rate,
	.set_min_rate = rcg_clk_set_min_rate,
	.get_rate = rcg_clk_get_rate,
	.list_rate = rcg_clk_list_rate,
	.is_enabled = rcg_clk_is_enabled,
	.round_rate = rcg_clk_round_rate,
	.reset = msm7x30_clk_reset,
	.set_flags = soc_clk_set_flags,
	.is_local = local_clk_is_local,
	.get_parent = rcg_clk_get_parent,
};

static struct clk_ops clk_ops_branch = {
	.enable = branch_clk_enable,
	.disable = branch_clk_disable,
	.auto_off = branch_clk_auto_off,
	.is_enabled = branch_clk_is_enabled,
	.reset = soc_branch_clk_reset,
	.set_flags = soc_clk_set_flags,
	.is_local = local_clk_is_local,
	.get_parent = branch_clk_get_parent,
	.set_parent = branch_clk_set_parent,
};
