/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/of.h>
#include <linux/of_address.h>

#include <mtk_spm_internal.h>
#include <mtk_idle_internal.h>
//#include <ddp_pwm.h>

#include <mt-plat/mtk_secure_api.h>
#include <mtk_spm_reg.h>

#define IDLE_TAG     "[name:spm&]Power/swap"
#define idle_err(fmt, args...)		printk_deferred(IDLE_TAG fmt, ##args)

#define NF_CLKMUX_PASS_CRITERIA     8
#define NF_CLKMUX_COND_SET          9 /* NF_CLKMUX_PASS_CRITERIA + 1 */

enum subsys_id {
	SYS_DIS = 0,
	SYS_MFG,
	SYS_ISP,
	SYS_VCODEC,
	NR_SYSS__,
};

/*
 * Variable Declarations
 */
void __iomem *infrasys_base;
void __iomem *mmsys_base;
void __iomem *imgsys_base;
void __iomem *mfgsys_base;
void __iomem *vencsys_base;
void __iomem *topcksys_base;        /* TOPCKSYS_REG, CLK_CFG */

void __iomem *sleepsys_base;
void __iomem  *apmixed_base_in_idle;

/* Idle handler on/off */
int idle_switch[NR_TYPES] = {
	1,	/* dpidle switch */
	1,	/* soidle3 switch */
	1,	/* soidle switch */
	1,	/* rgidle switch */
};

unsigned int dpidle_blocking_stat[NR_GRPS][32];

unsigned int idle_condition_mask[NR_TYPES][NR_GRPS] = {
	/* dpidle_condition_mask */
	[IDLE_TYPE_DP] = {
		0x00040802,	/* INFRA0 */
		0x03AFB900,	/* INFRA1 */
		0x000000C5,	/* INFRA2 */
		0xFFFFFC1B,	/* MMSYS0 */
		0x00003FFF,	/* MMSYS1 */
		0xBEF000B8, /* PWR_STATE */
	},
	/* soidle3_condition_mask */
	[IDLE_TYPE_SO3] = {
		0x02040802,	/* INFRA0 */
		0x03AFB900,	/* INFRA1 */
		0x000000D1,	/* INFRA2 */
		0xFFFFFC1B,	/* MMSYS0 */
		0x00003FFF,	/* MMSYS1 */
		0xBEF000B0, /* PWR_STATE */
	},
	/* soidle_condition_mask */
	[IDLE_TYPE_SO] = {
		0x00040802,	/* INFRA0 */
		0x03AFB900,	/* INFRA1 */
		0x000000C1,	/* INFRA2 */
		0x000DFC00,	/* MMSYS0 */
		0x00000D70,	/* MMSYS1 */
		0xBEF000B0, /* PWR_STATE */
	},
	/* rgidle_condition_mask */
	[IDLE_TYPE_RG] = {
		0, 0, 0, 0, 0, 0},
};

unsigned int soidle3_pll_condition_mask[NR_PLLS] = {
	1, /* UNIVPLL */
	0, /* MFGPLL */
	1, /* MSDCPLL */
	0, /* TVDPLL */
	0, /* MMPLL */
};

/*
 * Support up to `NF_CLKMUX_PASS_CRITERIA` pass conditions
 * (set num, val_0, val_1, ..., val_n-1)
 */
unsigned int clkmux_condition_mask[NF_CLKMUX][NF_CLKMUX_COND_SET] = {
	/* CLK_CFG_0 */
	[CLKMUX_CAM]
		= { 2,
			0x80, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00 },
	[CLKMUX_IMG]
		= { 2,
			0x80, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00 },
	[CLKMUX_MM]
		= { 2,
			0x80, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00 },
	[CLKMUX_AXI]
		= { 0,
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00 }, /* skip */

	/* CLK_CFG_1 */
	[CLKMUX_IPU_IF]
		= { 2,
			0x80, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00 },
	[CLKMUX_DSP2]
		= { 2,
			0x80, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00 },
	[CLKMUX_DSP1]
		= { 2,
			0x80, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00 },
	[CLKMUX_DSP]
		= { 2,
			0x80, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00 },

	/* CLK_CFG_2 */
	[CLKMUX_CAMTG2]
		= { 2,
			0x80, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00 },
	[CLKMUX_CAMTG]
		= { 2,
			0x80, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00 },
	[CLKMUX_MFG_52M]
		= { 2,
			0x80, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00 },
	[CLKMUX_MFG]
		= { 2,
			0x80, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00 },

	/* CLK_CFG_3 */
	[CLKMUX_SPI]
		= { 0,
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00 }, /* skip */
	[CLKMUX_UART]
		= { 2,
			0x80, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00 },
	[CLKMUX_CAMTG4]
		= { 2,
			0x80, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00 },
	[CLKMUX_CAMTG3]
		= { 2,
			0x80, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00 },

	/* CLK_CFG_4 */
	[CLKMUX_MSDC30_2]
		= { 2,
			0x80, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00 },
	[CLKMUX_MSDC30_1]
		= { 2,
			0x80, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00 },
	[CLKMUX_MSDC50_0]
		= { 2,
			0x80, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00 },
	[CLKMUX_MSDC50_0_HCLK]
		= { 2,
			0x80, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00 },

	/* CLK_CFG_5 */
	[CLKMUX_PWRAP_ULPOSC]
		= { 0,
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00 }, /* skip */
	[CLKMUX_PMICSPI]
		= { 2,
			0x80, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00 },
	[CLKMUX_AUD_INTBUS]
		= { 2,
			0x80, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00 },
	[CLKMUX_AUDIO]
		= { 2,
			0x80, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00 },

	/* CLK_CFG_6 */
	[CLKMUX_SCAM]
		= { 0,
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00 }, /* skip */
	[CLKMUX_DPI0]
		= { 2,
			0x80, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00 },
	[CLKMUX_SSPM]
		= { 0,
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00 }, /* skip */
	[CLKMUX_ATB]
		= { 2,
			0x80, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00 },

	/* CLK_CFG_7 */
	[CLKMUX_SPM]
		= { 0,
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00 },
	[CLKMUX_SSUSB_XHCI]
		= { 2,
			0x80, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00 },
	[CLKMUX_USB_TOP]
		= { 2,
			0x80, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00 },
	[CLKMUX_DISP_PWM]
		= { 5,
			0x80, 0x00, 0x02, 0x03,
			0x04, 0x00, 0x00, 0x00 }, /* power OFF, 26M, ULPOSC */

	/* CLK_CFG_8 */
	[CLKMUX_DXCC]
		= { 0,
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00 }, /* skip */
	[CLKMUX_SENINF]
		= { 2,
			0x80, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00 },
	[CLKMUX_SCP]
		= { 2,
			0x80, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00 },
	[CLKMUX_I2C]
		= { 2,
			0x80, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00 },

	/* CLK_CFG_9 */
	[CLKMUX_UFS]
		= { 2,
			0x80, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00 },
	[CLKMUX_AES_UFSFDE]
		= { 0,
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00 }, /* skip */
	[CLKMUX_AUD_ENGEN2]
		= { 2,
			0x80, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00 },
	[CLKMUX_AUD_ENGEN1]
		= { 2,
			0x80, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00 },

	/* CLK_CFG_10 */
	[CLKMUX_RSV_0]
		= { 0,
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00 }, /* skip */
	[CLKMUX_RSV_1]
		= { 0,
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00 }, /* skip */
	[CLKMUX_AUD_2]
		= { 0,
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00 },
	[CLKMUX_AUD_1]
		= { 0,
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00 },
};

static const char *idle_name[NR_TYPES] = {
	"dpidle",
	"soidle3",
	"soidle",
	"rgidle",
};

static const char *reason_name[NR_REASONS] = {
	"by_frm",
	"by_cpu",
	"by_srr",
	"by_ufs",
	"by_tee",
	"by_clk",
	"by_dcs",
	"by_dis",
	"by_pwm",
	"by_pll",
	"by_boot"
};

static const char *cg_group_name[NR_GRPS] = {
	"INFRA0",
	"INFRA1",
	"INFRA2",
	"MMSYS0",
	"MMSYS1",
	"PWR_STATE",
};

/*
 * Weak functions
 */
void __attribute__((weak)) msdc_clk_status(int *status)
{
	*status = 0;
}

bool __attribute__((weak)) disp_pwm_is_osc(void)
{
	return false;
}

/*
 * Function Definitions
 */
const char *mtk_get_idle_name(int id)
{
	WARN_ON(INVALID_IDLE_ID(id));
	if ((id >= 0) && (id < NR_TYPES))
		return idle_name[id];
	return "Invalid_Name";
}

const char *mtk_get_reason_name(int id)
{
	WARN_ON(INVALID_REASON_ID(id));
	if ((id >= 0) && (id < NR_REASONS))
		return reason_name[id];
	return "Invalid_Name";
}

const char *mtk_get_cg_group_name(int id)
{
	WARN_ON(INVALID_GRP_ID(id));
	if ((id >= 0) && (id < NR_GRPS))
		return cg_group_name[id];
	return "Invalid_Name";
}

static int sys_is_on(enum subsys_id id)
{
	u32 pwr_sta_mask[] = {
		DIS_PWR_STA_MASK,
		MFG_PWR_STA_MASK,
		ISP_PWR_STA_MASK,
		VEN_PWR_STA_MASK,
	};

	u32 mask = pwr_sta_mask[id];
	u32 sta = idle_readl(SPM_PWR_STATUS);
	u32 sta_s = idle_readl(SPM_PWR_STATUS_2ND);

	/* if (id >= NR_SYSS__) */
		/* BUG(); */

	return (sta & mask) && (sta_s & mask);
}

static int is_clkmux_on(int id)
{
	unsigned int clkcfg = 0;
	int reg_idx = id / 4;
	int sub_idx = id % 4;
	unsigned int mask[4] = {0x80000000, 0x00800000, 0x00008000, 0x00000080};

	clkcfg = idle_readl(CLK_CFG(reg_idx));

	return !(clkcfg & mask[sub_idx]);
}

static int is_pll_on(int id)
{
	return idle_readl(APMIXEDSYS(0x230 + id * 0x10)) & 0x1;
}

static void get_all_clock_state(u32 clks[NR_GRPS])
{
	int i;

	for (i = 0; i < NR_GRPS; i++)
		clks[i] = 0;

	clks[CG_INFRA_0] = ~idle_readl(INFRA_SW_CG_0_STA);      /* INFRA0 */
	clks[CG_INFRA_1] = ~idle_readl(INFRA_SW_CG_1_STA);      /* INFRA1 */
	clks[CG_INFRA_2] = ~idle_readl(INFRA_SW_CG_2_STA);      /* INFRA2 */

	/* MTCMOS DIS and MM_PLL */
	if (sys_is_on(SYS_DIS) && is_clkmux_on(CLKMUX_MM)) {
		clks[CG_MMSYS0] = (~idle_readl(DISP_CG_CON_0));
		clks[CG_MMSYS1] = (~idle_readl(DISP_CG_CON_1));
	}

	clks[CG_PWR_STATE] = idle_readl(SPM_PWR_STATUS);
}

static inline void
	mtk_idle_check_cg_internal(
		unsigned int block_mask[NR_TYPES][NF_CG_STA_RECORD],
		int idle_type)
{
	int a, b;

	for (a = 0; a < NR_GRPS; a++) {
		for (b = 0; b < 32; b++) {
			if (block_mask[idle_type][a] & (1 << b))
				dpidle_blocking_stat[a][b] += 1;
		}
	}
}

bool mtk_idle_check_secure_cg(
		unsigned int block_mask[NR_TYPES][NF_CG_STA_RECORD])
{
	int ret = 0;
	int i;

	ret = SMC_CALL(MTK_SIP_KERNEL_CHECK_SECURE_CG, 0, 0, 0);

	if (ret)
		for (i = 0; i < NR_TYPES; i++)
			if (idle_switch[i])
				block_mask[i][CG_INFRA_0] |= 0x08000000;

	return !ret;
}

bool mtk_idle_check_cg(unsigned int block_mask[NR_TYPES][NF_CG_STA_RECORD])
{
	bool ret = true;
	int i, j;
	unsigned int sta;
	u32 clks[NR_GRPS];

/* deprecated: msdc driver use resource request api instead */
#if 0
	msdc_clk_status(&sd_mask);
#endif

	get_all_clock_state(clks);

	sta = idle_readl(SPM_PWR_STATUS);

	for (i = 0; i < NR_TYPES; i++) {
		if (idle_switch[i]) {
/* deprecated: msdc driver use resource request instead */
#if 0
			/* SD status */
			if (sd_mask) {
				block_mask[i][CG_INFRA_0] |= sd_mask;
				block_mask[i][NR_GRPS] |= 0x1;
			}
#endif
			/* CG status */
			for (j = 0; j < NR_GRPS; j++) {
				block_mask[i][j] =
					idle_condition_mask[i][j] & clks[j];
				if (block_mask[i][j])
					block_mask[i][NR_GRPS] |= 0x2;
			}
			if (i == IDLE_TYPE_DP)
				mtk_idle_check_cg_internal(block_mask,
								IDLE_TYPE_DP);

			/* mtcmos */
			if (i == IDLE_TYPE_DP && !dpidle_by_pass_pg) {
				unsigned int flag = DP_PWR_STA_MASK;

				if (sta & flag) {
					block_mask[i][NR_GRPS + 0] |= 0x4;
					block_mask[i][NR_GRPS + 1] =
								(sta & flag);
				}
			}
			if ((i == IDLE_TYPE_SO || i == IDLE_TYPE_SO3) &&
				!soidle_by_pass_pg) {
				unsigned int flag = SO_PWR_STA_MASK;

				if (sta & flag) {
					block_mask[i][NR_GRPS + 0] |= 0x4;
					block_mask[i][NR_GRPS + 1] =
								(sta & flag);
				}
			}
			if (block_mask[i][NR_GRPS])
				ret = false;
		}
	}

	return ret;
}

static const char *pll_name[NR_PLLS] = {
	"UNIVPLL",
	"MFGPLL",
	"MSDCPLL",
	"TVDPLL",
	"MMPLL",
};

const char *mtk_get_pll_group_name(int id)
{
	if (id >= 0 && id < NR_PLLS)
		return pll_name[id];
	else
		return NULL;
}

bool mtk_idle_check_pll(unsigned int *condition_mask, unsigned int *block_mask)
{
	int i, j;

	memset(block_mask, 0, NR_PLLS * sizeof(unsigned int));

	for (i = 0; i < NR_PLLS; i++) {
		if (is_pll_on(i) & condition_mask[i]) {
			for (j = 0; j < NR_PLLS; j++)
				block_mask[j] =
					is_pll_on(j) & condition_mask[j];
			return false;
		}
	}

	return true;
}

#if 0
/* No need to get audio base */
static int __init get_base_from_matching_node(
		const struct of_device_id *ids, void __iomem **pbase, int idx,
		const char *cmp)
{
	struct device_node *node;

	node = of_find_matching_node(NULL, ids);
	if (!node) {
		idle_err("node '%s' not found!\n", cmp);
		/* TODO: BUG() */
	}

	*pbase = of_iomap(node, idx);
	if (!(*pbase)) {
		idle_err("node '%s' cannot iomap!\n", cmp);
		/* TODO: BUG() */
	}

	return 0;
}
#endif

static int __init get_base_from_node(
	const char *cmp, void __iomem **pbase, int idx)
{
	struct device_node *node;

	node = of_find_compatible_node(NULL, NULL, cmp);

	if (!node) {
		idle_err("node '%s' not found!\n", cmp);
		/* TODO: BUG() */
	}

	*pbase = of_iomap(node, idx);
	if (!(*pbase)) {
		idle_err("node '%s' cannot iomap!\n", cmp);
		/* TODO: BUG() */
	}

	return 0;
}

void __init iomap_init(void)
{
	get_base_from_node("mediatek,infracfg_ao", &infrasys_base, 0);
	get_base_from_node("mediatek,mmsys_config", &mmsys_base, 0);
	get_base_from_node("mediatek,imgsys", &imgsys_base, 0);
	get_base_from_node("mediatek,mfgcfg", &mfgsys_base, 0);
	get_base_from_node("mediatek,venc_gcon", &vencsys_base, 0);
	get_base_from_node("mediatek,apmixed", &apmixed_base_in_idle, 0);
	get_base_from_node("mediatek,sleep", &sleepsys_base, 0);
	get_base_from_node("mediatek,topckgen", &topcksys_base, 0);
}

bool mtk_idle_disp_is_pwm_rosc(void)
{
	return disp_pwm_is_osc();
}

u32 get_spm_idle_flags1(void)
{
	u32 idle_flags1 = 0;

	idle_flags1 |= SPM_FLAG1_ENABLE_CPU_SLEEP_VOLT;

	return idle_flags1;
}

static void get_all_clkcfg_state(u32 clkcfgs[NF_CLK_CFG])
{
	int i;

	for (i = 0; i < NF_CLK_CFG; i++)
		clkcfgs[i] = idle_readl(CLK_CFG(i));
}

#define MUX_OFF_MASK    0x80
#define MUX_ON_MASK     0x8F

bool mtk_idle_check_clkmux(
	int idle_type, unsigned int block_mask[NR_TYPES][NF_CLK_CFG])
{
	u32 clkcfgs[NF_CLK_CFG] = {0};
	int i, k;
	int idx;
	int offset;
	u32 masks[4]  = {0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF};
	int shifts[4] = {24, 16, 8, 0};
	u32 clkmux_val;

	bool result = false;
	bool final_result = true;

	int check_num;
	u32 check_val;
	u32 mux_check_mask;

	get_all_clkcfg_state(clkcfgs);

	/* Check each clkmux setting */
	for (i = 0; i < NF_CLKMUX; i++) {

		result     = false;
		idx        = i / 4;
		offset     = i % 4;
		clkmux_val = ((clkcfgs[idx] & masks[offset]) >> shifts[offset]);

		check_num = clkmux_condition_mask[i][0];

		if (check_num == 0)
			result = true;

		for (k = 0; k < check_num; k++) {

			check_val  = clkmux_condition_mask[i][1 + k];

			mux_check_mask = (k == 0) ? MUX_OFF_MASK : MUX_ON_MASK;

			if ((clkmux_val & mux_check_mask) == check_val) {
				result = true;
				break;
			}
		}

		if (result == false) {

			final_result = false;

			block_mask[idle_type][idx] |=
				(clkmux_val << shifts[offset]);
		}
	}

	return final_result;
}

bool mtk_idle_check_vcore_cond(void)
{
	uint32_t val = 0;
	bool ret = false;

	/* All PLLs in check list have to power down */
	val = idle_readl(APLL1_CON0);

	if ((val & 0x00000001))
		goto RET;

	val = idle_readl(APLL2_CON0);

	if ((val & 0x00000001))
		goto RET;

	ret = true;

RET:
	return ret;
}
