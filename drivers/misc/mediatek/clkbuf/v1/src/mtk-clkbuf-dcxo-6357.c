// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: ky.liu <ky.liu@mediatek.com>
 */

#include <linux/mfd/syscon.h>

#include "mtk-clkbuf-dcxo.h"
#include "mtk-clkbuf-dcxo-6357.h"
#include "mtk_clkbuf_common.h"

#define MT6357_SET_REG_BY_NAME(reg, name)			\
	SET_REG_BY_NAME(reg, MT6357_ ## name)

#define XO_NUM				MT6357_XO_NUM

/* No LDO register to dump on 6357 yet */

static int mt6357_debug_regs_index[] = {0, 8, 9, 11, 15, 16, 18, 19, 23};

static const char * const valid_cmd_type[] = {
	"ON",
	"OFF",
	"EN_BB",
	"SIG",
	"CO_BUF",
	"INIT",
	NULL,
};

struct mt6357_debug_regs {
	struct reg_t _dcxo_cw00;
	struct reg_t _dcxo_cw08;
	struct reg_t _dcxo_cw09;
	struct reg_t _dcxo_cw11;
	struct reg_t _dcxo_cw15;
	struct reg_t _dcxo_cw16;
	struct reg_t _dcxo_cw18;
	struct reg_t _dcxo_cw19;
	struct reg_t _dcxo_cw23;
};

/* Maybe more debug reg? */
static struct mt6357_debug_regs debug_reg = {
	SET_REG(dcxo_cw00, MT6357_DCXO_CW00, 0xFFFF, 0)
	SET_REG(dcxo_cw08, MT6357_DCXO_CW08, 0xFFFF, 0)
	SET_REG(dcxo_cw09, MT6357_DCXO_CW09, 0xFFFF, 0)
	SET_REG(dcxo_cw11, MT6357_DCXO_CW11, 0xFFFF, 0)
	SET_REG(dcxo_cw15, MT6357_DCXO_CW15, 0xFFFF, 0)
	SET_REG(dcxo_cw16, MT6357_DCXO_CW16, 0xFFFF, 0)
	SET_REG(dcxo_cw18, MT6357_DCXO_CW18, 0xFFFF, 0)
	SET_REG(dcxo_cw19, MT6357_DCXO_CW19, 0xFFFF, 0)
	SET_REG(dcxo_cw23, MT6357_DCXO_CW23, 0xFFFF, 0)
};

static int mt6357_dcxo_dump_reg_log(char *buf);

/* Update aux_sel later */
static struct xo_buf_t xo_bufs[XO_NUM] = {
	[XO_SOC] = {
		MT6357_SET_REG_BY_NAME(xo_mode, XO_EXTBUF1_MODE)
		MT6357_SET_REG_BY_NAME(xo_en, XO_EXTBUF1_EN_M)
		SET_REG(xo_en_auxout, MT6357_XO_STATIC_AUXOUT_ADDR, 0x1, 0)
		MT6357_SET_REG_BY_NAME(drv_curr, RG_XO_EXTBUF1_ISET)
		MT6357_SET_REG_BY_NAME(hwbblpm_msk, XO_EXTBUF1_BBLPM_EN_MASK)
		.xo_en_auxout_sel = 5,
		.in_use = 1,
	},
	[XO_WCN] = {
		MT6357_SET_REG_BY_NAME(xo_mode, XO_EXTBUF2_MODE)
		MT6357_SET_REG_BY_NAME(xo_en, XO_EXTBUF2_EN_M)
		SET_REG(xo_en_auxout, MT6357_XO_STATIC_AUXOUT_ADDR, 0x1, 6)
		MT6357_SET_REG_BY_NAME(drv_curr, RG_XO_EXTBUF2_ISET)
		MT6357_SET_REG_BY_NAME(hwbblpm_msk, XO_EXTBUF2_BBLPM_EN_MASK)
		.xo_en_auxout_sel = 5,
		.in_use = 1,
	},
	[XO_NFC] = {
		MT6357_SET_REG_BY_NAME(xo_mode, XO_EXTBUF3_MODE)
		MT6357_SET_REG_BY_NAME(xo_en, XO_EXTBUF3_EN_M)
		SET_REG(xo_en_auxout, MT6357_XO_STATIC_AUXOUT_ADDR, 0x1, 0)
		MT6357_SET_REG_BY_NAME(drv_curr, RG_XO_EXTBUF3_ISET)
		MT6357_SET_REG_BY_NAME(hwbblpm_msk, XO_EXTBUF3_BBLPM_EN_MASK)
		.xo_en_auxout_sel = 6,
		.in_use = 1,
	},
	[XO_CEL] = {
		MT6357_SET_REG_BY_NAME(xo_mode, XO_EXTBUF4_MODE)
		MT6357_SET_REG_BY_NAME(xo_en, XO_EXTBUF4_EN_M)
		SET_REG(xo_en_auxout, MT6357_XO_STATIC_AUXOUT_ADDR, 0x1, 6)
		MT6357_SET_REG_BY_NAME(drv_curr, RG_XO_EXTBUF4_ISET)
		MT6357_SET_REG_BY_NAME(hwbblpm_msk, XO_EXTBUF4_BBLPM_EN_MASK)
		.xo_en_auxout_sel = 6,
		.in_use = 1,
	},
	[XO_RSV1] = {
		.in_use = 0,
	},
	[XO_RSV2] = {
		MT6357_SET_REG_BY_NAME(xo_mode, XO_EXTBUF6_MODE)
		MT6357_SET_REG_BY_NAME(xo_en, XO_EXTBUF6_EN_M)
		SET_REG(xo_en_auxout, MT6357_XO_STATIC_AUXOUT_ADDR, 0x1, 0)
		MT6357_SET_REG_BY_NAME(drv_curr, RG_XO_EXTBUF6_ISET)
		MT6357_SET_REG_BY_NAME(hwbblpm_msk, XO_EXTBUF6_BBLPM_EN_MASK)
		.xo_en_auxout_sel = 7,
		.in_use = 0,
	},
	[XO_EXT] = {
		MT6357_SET_REG_BY_NAME(xo_mode, XO_EXTBUF7_MODE)
		MT6357_SET_REG_BY_NAME(xo_en, XO_EXTBUF7_EN_M)
		SET_REG(xo_en_auxout, MT6357_XO_STATIC_AUXOUT_ADDR, 0x1, 12)
		MT6357_SET_REG_BY_NAME(drv_curr, RG_XO_EXTBUF7_ISET)
		MT6357_SET_REG_BY_NAME(hwbblpm_msk, XO_EXTBUF7_BBLPM_EN_MASK)
		.xo_en_auxout_sel = 6,
		.in_use = 1,
	},
};

struct dcxo_hw mt6357_dcxo = {
	.hw = {
		.is_pmic = true,
	},
	.xo_num = XO_NUM,
	.bblpm_auxout_sel = 14,
	.xo_bufs = xo_bufs,
	MT6357_SET_REG_BY_NAME(static_aux_sel, XO_STATIC_AUXOUT_SEL)
	SET_REG(bblpm_auxout, MT6357_XO_STATIC_AUXOUT_ADDR, 0x1, 5)
	MT6357_SET_REG_BY_NAME(swbblpm_en, XO_BB_LPM_EN_M)
	MT6357_SET_REG_BY_NAME(hwbblpm_sel, XO_BB_LPM_EN_SEL)
	MT6357_SET_REG_BY_NAME(xo_cdac_fpm, XO_CDAC_FPM)
	MT6357_SET_REG_BY_NAME(xo_aac_fpm_swen, XO_AAC_FPM_SWEN)
	MT6357_SET_REG_BY_NAME(xo_heater_sel, RG_XO_HEATER_SEL)
	.ops = {
		.dcxo_dump_reg_log = mt6357_dcxo_dump_reg_log,
	},
	.valid_dcxo_cmd = valid_cmd_type,
};

static int mt6357_dcxo_dump_reg_log(char *buf)
{
	u32 val = 0;
	int len = 0, i;

	for (i = 0; i < sizeof(struct mt6357_debug_regs)/sizeof(struct reg_t); ++i) {
		if (clk_buf_read(&mt6357_dcxo.hw,
				((struct reg_t *)&debug_reg) + i, &val))
			goto DUMP_REG_LOG_FAILED;
		len += snprintf(buf + len, PAGE_SIZE - len, "DCXO_CW%2d=0x%x, ",
				mt6357_debug_regs_index[i], val);
	}
DUMP_REG_LOG_FAILED:
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");
	return len;
}
