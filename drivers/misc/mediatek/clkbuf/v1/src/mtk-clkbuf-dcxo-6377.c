// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: ren-ting.wang <ren-ting.wang@mediatek.com>
 */

#include <linux/mfd/syscon.h>

#include "mtk-clkbuf-dcxo-6377.h"
#include "mtk_clkbuf_ctl.h"

#define MT6377_SET_REG_BY_NAME(reg, name)			\
	SET_REG_BY_NAME(reg, name)

#define SET_RAW_REG(reg, name)			\
	SET_REG(reg, name, 0xff, 0x0)

#define XO_NUM				7

static const char * const valid_cmd_type[] = {
	"ON",
	"OFF",
	"EN_BB",
	"SIG",
	"CO_BUF",
	"INIT",
	NULL,
};

struct mt6377_debug_regs {
	struct reg_t _dcxo_cw00;
	struct reg_t _dcxo_cw00_h;
	struct reg_t _dcxo_cw08;
	struct reg_t _dcxo_cw09_h;
	struct reg_t _dcxo_cw10;
	struct reg_t _dcxo_cw12;
	struct reg_t _dcxo_cw13;
	struct reg_t _dcxo_cw13_h;
	struct reg_t _dcxo_cw14;
	struct reg_t _dcxo_cw19;
	struct reg_t _dcxo_cw19_1;
	struct reg_t _dcxo_cw19_2;
};

static struct mt6377_debug_regs debug_reg = {
	SET_RAW_REG(dcxo_cw00, MT6377_DCXO_CW00)
	SET_RAW_REG(dcxo_cw00_h, MT6377_DCXO_CW00_H)
	SET_RAW_REG(dcxo_cw08, MT6377_DCXO_CW08)
	SET_RAW_REG(dcxo_cw09_h, MT6377_DCXO_CW09_H)
	SET_RAW_REG(dcxo_cw10, MT6377_DCXO_CW10)
	SET_RAW_REG(dcxo_cw12, MT6377_DCXO_CW12)
	SET_RAW_REG(dcxo_cw13, MT6377_DCXO_CW13)
	SET_RAW_REG(dcxo_cw13_h, MT6377_DCXO_CW13_H)
	SET_RAW_REG(dcxo_cw14, MT6377_DCXO_CW14)
	SET_RAW_REG(dcxo_cw19, MT6377_DCXO_CW19)
	SET_RAW_REG(dcxo_cw19_1, MT6377_DCXO_CW19_1)
	SET_RAW_REG(dcxo_cw19_2, MT6377_DCXO_CW19_2)
};

static int mt6377_dcxo_dump_reg_log(char *buf);

enum MT6377_XO_NAMES {
	XO_SOC = 0,
	XO_WCN,
	XO_NFC,
	XO_CEL = 3,
	XO_EXT = 6,
	MT6377_XO_NUM
};

static struct xo_buf_t xo_bufs[XO_NUM] = {
	[XO_SOC] = {
		MT6377_SET_REG_BY_NAME(xo_mode, XO_EXTBUF1_MODE)
		MT6377_SET_REG_BY_NAME(xo_en, XO_EXTBUF1_EN_M)
		SET_REG(xo_en_auxout, XO_STATIC_AUXOUT_H_ADDR, 0x1, 5)
		MT6377_SET_REG_BY_NAME(impedance, RG_XO_EXTBUF1_RSEL)
		MT6377_SET_REG_BY_NAME(de_sense, RG_XO_EXTBUF1_HD)
		MT6377_SET_REG_BY_NAME(rc_voter, XO_SOC_VOTE_L)
		MT6377_SET_REG_BY_NAME(hwbblpm_msk, XO_SOC_BBLPM_EN_MASK)
		.xo_en_auxout_sel = 6,
		.in_use = 1,
	},
	[XO_WCN] = {
		MT6377_SET_REG_BY_NAME(xo_mode, XO_EXTBUF2_MODE)
		MT6377_SET_REG_BY_NAME(xo_en, XO_EXTBUF2_EN_M)
		SET_REG(xo_en_auxout, XO_STATIC_AUXOUT_H_ADDR, 0x1, 3)
		MT6377_SET_REG_BY_NAME(impedance, RG_XO_EXTBUF2_RSEL)
		MT6377_SET_REG_BY_NAME(de_sense, RG_XO_EXTBUF2_SRSEL)
		MT6377_SET_REG_BY_NAME(rc_voter, XO_WCN_VOTE_L)
		MT6377_SET_REG_BY_NAME(hwbblpm_msk, XO_WCN_BBLPM_EN_MASK)
		.xo_en_auxout_sel = 6,
		.in_use = 1,
	},
	[XO_NFC] = {
		MT6377_SET_REG_BY_NAME(xo_mode, XO_EXTBUF3_MODE)
		MT6377_SET_REG_BY_NAME(xo_en, XO_EXTBUF3_EN_M)
		SET_REG(xo_en_auxout, XO_STATIC_AUXOUT_H_ADDR, 0x1, 1)
		MT6377_SET_REG_BY_NAME(impedance, RG_XO_EXTBUF3_RSEL)
		MT6377_SET_REG_BY_NAME(de_sense, RG_XO_EXTBUF3_HD)
		MT6377_SET_REG_BY_NAME(rc_voter, XO_NFC_VOTE_L)
		MT6377_SET_REG_BY_NAME(hwbblpm_msk, XO_NFC_BBLPM_EN_MASK)
		.xo_en_auxout_sel = 6,
		.in_use = 1,
	},
	[XO_CEL] = {
		MT6377_SET_REG_BY_NAME(xo_mode, XO_EXTBUF4_MODE)
		MT6377_SET_REG_BY_NAME(xo_en, XO_EXTBUF4_EN_M)
		SET_REG(xo_en_auxout, XO_STATIC_AUXOUT_L_ADDR, 0x1, 7)
		MT6377_SET_REG_BY_NAME(impedance, RG_XO_EXTBUF4_RSEL)
		MT6377_SET_REG_BY_NAME(de_sense, RG_XO_EXTBUF4_SRSEL)
		MT6377_SET_REG_BY_NAME(rc_voter, XO_CEL_VOTE_L)
		MT6377_SET_REG_BY_NAME(hwbblpm_msk, XO_CEL_BBLPM_EN_MASK)
		.xo_en_auxout_sel = 6,
		.in_use = 1,
	},
	[XO_EXT] = {
		MT6377_SET_REG_BY_NAME(xo_mode, XO_EXTBUF7_MODE)
		MT6377_SET_REG_BY_NAME(xo_en, XO_EXTBUF7_EN_M)
		SET_REG(xo_en_auxout, XO_STATIC_AUXOUT_L_ADDR, 0x1, 3)
		MT6377_SET_REG_BY_NAME(impedance, RG_XO_EXTBUF7_RSEL)
		MT6377_SET_REG_BY_NAME(de_sense, RG_XO_EXTBUF7_HD)
		MT6377_SET_REG_BY_NAME(rc_voter, XO_EXT_VOTE_L)
		MT6377_SET_REG_BY_NAME(hwbblpm_msk, XO_EXT_BBLPM_EN_MASK)
		.xo_en_auxout_sel = 6,
		.in_use = 1,
	},
};


/* Todo: Update functions below */
struct dcxo_hw mt6377_dcxo = {
	.hw = {
		.is_pmic = true,
	},
	.xo_num = XO_NUM,
	.bblpm_auxout_sel = 0x25,
	.xo_bufs = xo_bufs,
	MT6377_SET_REG_BY_NAME(static_aux_sel, XO_STATIC_AUXOUT_SEL)
	SET_REG(bblpm_auxout, XO_STATIC_AUXOUT_H_ADDR, 0x1, 0)
	MT6377_SET_REG_BY_NAME(swbblpm_en, XO_BB_LPM_EN_M)
	MT6377_SET_REG_BY_NAME(hwbblpm_sel, XO_BB_LPM_EN_SEL)
	SET_RAW_REG(dcxo_pmrc_en, MT6377_PMRC_CON0)
	.ops = {
		.dcxo_dump_reg_log = mt6377_dcxo_dump_reg_log,
	},
	.valid_dcxo_cmd = valid_cmd_type,
};
EXPORT_SYMBOL(mt6377_dcxo);

static int mt6377_dcxo_dump_reg_log(char *buf)
{
	u32 val = 0;
	int len = 0, i;

	for (i = 0; i < sizeof(struct mt6377_debug_regs)/sizeof(struct reg_t); ++i) {
		if (clk_buf_read(&mt6377_dcxo.hw,
				((struct reg_t *)&debug_reg) + i, &val))
			goto DUMP_REG_LOG_FAILED;
		len += snprintf(buf+len, PAGE_SIZE - len, "Addr 0x%x = 0x%x\n",
				((struct reg_t *)&debug_reg)[i].ofs, val);
	}
	return len;
DUMP_REG_LOG_FAILED:
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");
	return len;
}

static int mt6377_clkbuf_probe(struct platform_device *pdev)
{
	// Init clkbuf driver
	pr_notice("clkbuf init with mt6377_dcxo\n");
	dcxo = &mt6377_dcxo;
	return clkbuf_init(pdev);
}

static const struct of_device_id mt6377_clkbuf_of_match[] = {
	{
		.compatible = "mediatek,clock_buffer",
	},
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, mt6377_clkbuf_of_match);

static struct platform_driver mt6377_clkbuf_driver = {
	.driver = {
		.name = "mtk-clock-buffer",
		.of_match_table = of_match_ptr(mt6377_clkbuf_of_match),
		.pm = &clk_buf_suspend_ops,
	},
	.probe = mt6377_clkbuf_probe,
};

module_platform_driver(mt6377_clkbuf_driver);
MODULE_AUTHOR("ky.liu <ky.liu@mediatek.com>");
MODULE_DESCRIPTION("SOC Driver for MediaTek Clock Buffer");
MODULE_LICENSE("GPL v2");
