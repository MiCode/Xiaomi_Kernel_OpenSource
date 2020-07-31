// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: owen.chen <owen.chen@mediatek.com>
 */

/*
 * @file    mtk-srclken-rc-spm.c
 * @brief   Driver for subys request resource control of each pmic
 *
 */

#include <linux/device.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/types.h>

#include "mtk-srclken-rc-hw.h"
#include "mtk-srclken-rc-common.h"

#define ADDR_NOT_VALID	0xffff
#define SRCLKEN_CFG(ofs)	(cfg_base + ofs)
#define SRCLKEN_STA(ofs)	(sta_base + ofs)

#if defined(CONFIG_OF)
#define SRCLKEN_RC_CFG				(0x000)
#define RC_CENTRAL_CFG1				(0x004)
#define RC_CENTRAL_CFG2				(0x008)
#define RC_CMD_ARB_CFG				(0x00C)
#define RC_PMIC_RCEN_ADDR			(0x010)
#define RC_PMIC_RCEN_SET_CLR_ADDR		(0x014)
#define RC_DCXO_FPM_CFG				(0x018)
#define RC_CENTRAL_CFG3				(0x01C)
#define RC_M00_SRCLKEN_CFG			(0x020)
#define RC_SRCLKEN_SW_CON_CFG			(0x054)
#define RC_CENTRAL_CFG4				(0x058)
#define RC_PROTOCOL_CHK_CFG			(0x060)
#define RC_DEBUG_CFG				(0x064)
#define RC_MISC_0				(0x0B4)
#define RC_SPM_CTL				(0x0B8)
#define SUBSYS_INTF_CFG				(0x0BC)

#define RC_FSM_STA_0				(0x000)
#define RC_CMD_STA_0				(0x004)
#define RC_CMD_STA_1				(0x008)
#define RC_SPI_STA_0				(0x00C)
#define RC_PI_PO_STA_0				(0x010)
#define RC_M00_REQ_STA_0			(0x014)
#define DBG_TRACE_0_LSB				(0x050)
#define DBG_TRACE_0_MSB				(0x054)
#define TIMER_LATCH_0_LSB			(0x098)
#define TIMER_LATCH_0_MSB			(0x09C)
#endif	/* CONFIG_OF */

#define TRACE_NUM			8

static void __iomem *cfg_base;
static void __iomem *sta_base;

struct offset {
	uint32_t lsb[TRACE_NUM];
	uint32_t msb[TRACE_NUM];
};

struct ext_reg {
	bool cfg_ext[CFG_NUM];
	bool sta_ext[STA_NUM];
};

static struct offset offset;
static struct ext_reg ext_reg;

static uint32_t cfg_id[CFG_NUM] = {
	[PWR_CFG] = SRCLKEN_RC_CFG,
	[CENTRAL_1_CFG] = RC_CENTRAL_CFG1,
	[CENTRAL_2_CFG] = RC_CENTRAL_CFG2,
	[CMD_ARB_CFG] = RC_CMD_ARB_CFG,
	[PMIC_CFG] = RC_PMIC_RCEN_ADDR,
	[PMIC_SETCLR_CFG] = RC_PMIC_RCEN_SET_CLR_ADDR,
	[DCXO_FPM_CFG] = RC_DCXO_FPM_CFG,
	[CENTRAL_3_CFG] = RC_CENTRAL_CFG3,
	[SUBSYS_CFG] = RC_M00_SRCLKEN_CFG,
	[SW_CON_CFG] = RC_SRCLKEN_SW_CON_CFG,
	[CENTRAL_4_CFG] = RC_CENTRAL_CFG4,
	[PROTOCOL_CHK_CFG] = RC_PROTOCOL_CHK_CFG,
	[DBG_CFG] = RC_DEBUG_CFG,
	[MISC_CFG] = RC_MISC_0,
	[SPM_CFG] = RC_SPM_CTL,
	[SUB_INTF_CFG] = SUBSYS_INTF_CFG,
};

static uint32_t sta_id[STA_NUM] = {
	[FSM_STA] = RC_FSM_STA_0,
	[CMD_0_STA] = RC_CMD_STA_0,
	[CMD_1_STA] = RC_CMD_STA_1,
	[SPI_STA] = RC_SPI_STA_0,
	[PIPO_STA] = RC_PI_PO_STA_0,
	[SUBSYS_STA] = RC_M00_REQ_STA_0,
	[DBG_TRACE_L_STA] = DBG_TRACE_0_LSB,
	[DBG_TRACE_M_STA] = DBG_TRACE_0_MSB,
	[TIMER_LATCH_L_STA] = TIMER_LATCH_0_LSB,
	[TIMER_LATCH_M_STA] = TIMER_LATCH_0_MSB,
};

static int set_subsys_cfg(uint32_t id, uint32_t mode, uint32_t req)
{
	uint64_t bit_mask = SW_SRCLKEN_RC_MSK << SW_SRCLKEN_RC_SHFT;
	uint32_t result = 0;

	if (id >= MAX_SYS_NUM || id < 0) {
		pr_notice("req_subsys is not available\n");
		return SRCLKEN_INVALID_ID;
	}

	if ((mode != HW_MODE) && (mode != SW_MODE)) {
		pr_notice("req_mode is not allowed\n");
		return SRCLKEN_INVALID_MODE;
	}

	if ((req != OFF_REQ) && (req != NO_REQ) &&
			(req != FPM_REQ) && (req != BBLPM_REQ)) {
		pr_notice("req_type is not allowed\n");
		return SRCLKEN_INVALID_REQ;
	}

	if (req == NO_REQ)
		req = (srclken_read(SRCLKEN_CFG(RC_M00_SRCLKEN_CFG + 4 * id)) &
				(FPM_REQ | BBLPM_REQ));

	srclken_write(SRCLKEN_CFG(RC_M00_SRCLKEN_CFG + 4 * id),
			(srclken_read(SRCLKEN_CFG(RC_M00_SRCLKEN_CFG + 4 * id))
			& ~(bit_mask)) | req | mode);

	result = srclken_read(SRCLKEN_CFG(RC_M00_SRCLKEN_CFG + 4 * id));
	if ((result & bit_mask) == (mode | req))
		return SRCLKEN_SUCCESS;

	pr_err("%s: read back value err.(0x%x)", __func__, result);

	return SRCLKEN_SET_CTRL_FAIL;
}

static int get_subsys_cfg(uint32_t idx, uint32_t *ret_val)
{
	if (idx >= MAX_SYS_NUM || idx < 0) {
		pr_notice("req_subsys is not available\n");
		return SRCLKEN_INVALID_ID;
	}

	*ret_val = srclken_read(SRCLKEN_CFG(RC_M00_SRCLKEN_CFG + 4 * idx));

	return SRCLKEN_SUCCESS;
}

static int get_cfg_reg(uint32_t id, uint32_t *ret_val)
{
	if (id >= CFG_NUM || id < 0) {
		pr_notice("id is not available\n");
		return SRCLKEN_INVALID_ID;
	}

	if (id >= CFG_EXT_START && (!ext_reg.cfg_ext[id] || !ext_reg.cfg_ext[id])) {
		pr_notice("req_register is not available\n");
		return SRCLKEN_INVLAID_REG;
	}

	*ret_val = srclken_read(SRCLKEN_CFG(cfg_id[id]));

	return SRCLKEN_SUCCESS;
}

static int get_sta_reg(uint32_t id, uint32_t *ret_val)
{
	if (id >= STA_NUM || id < 0) {
		pr_notice("id is not available\n");
		return SRCLKEN_INVALID_ID;
	}

	if (id >= STA_EXT_START && (!ext_reg.sta_ext[id] || !ext_reg.sta_ext[id])) {
		pr_notice("req_register is not available\n");
		return SRCLKEN_INVLAID_REG;
	}

	*ret_val = srclken_read(SRCLKEN_STA(sta_id[id]));

	return SRCLKEN_SUCCESS;
}

static int get_subsys_sta(uint32_t idx, uint32_t *ret_val)
{
	if (idx >= MAX_SYS_NUM || idx < 0) {
		pr_notice("req_subsys is not available\n");
		return SRCLKEN_INVALID_ID;
	}

	*ret_val = srclken_read(SRCLKEN_STA(RC_M00_REQ_STA_0 + 4 * idx));

	return SRCLKEN_SUCCESS;
}

static int get_trace_sta(uint32_t idx, uint32_t *ret_val1, uint32_t *ret_val2)
{
	unsigned int loffset;
	unsigned int moffset;

	if (idx >= TRACE_NUM || idx < 0) {
		pr_notice("req_subsys is not available\n");
		return SRCLKEN_INVALID_ID;
	}

	loffset = idx * 8 + offset.lsb[idx];
	moffset = idx * 8 + offset.msb[idx];

	*ret_val1 = srclken_read(SRCLKEN_STA(DBG_TRACE_0_LSB + loffset));
	*ret_val2 = srclken_read(SRCLKEN_STA(DBG_TRACE_0_MSB + moffset));

	return SRCLKEN_SUCCESS;
}

static int get_timer_latch(uint32_t idx, uint32_t *ret_val1, uint32_t *ret_val2)
{
	if (!ext_reg.sta_ext[TIMER_LATCH_L_STA] || !ext_reg.sta_ext[TIMER_LATCH_M_STA]) {
		pr_notice("req_register is not available\n");
		return SRCLKEN_INVLAID_REG;
	}

	if (idx >= TRACE_NUM || idx < 0) {
		pr_notice("req_subsys is not available\n");
		return SRCLKEN_INVALID_ID;
	}

	*ret_val1 = srclken_read(SRCLKEN_STA(TIMER_LATCH_0_LSB + idx * 8));
	*ret_val2 = srclken_read(SRCLKEN_STA(TIMER_LATCH_0_MSB + idx * 8));

	return SRCLKEN_SUCCESS;
}

static struct srclken_ops srclken_spm_ops = {
	.set_subsys_cfg = set_subsys_cfg,
	.get_subsys_cfg = get_subsys_cfg,
	.get_cfg_reg = get_cfg_reg,
	.get_sta_reg = get_sta_reg,
	.get_subsys_sta = get_subsys_sta,
	.get_trace_sta = get_trace_sta,
	.get_timer_latch = get_timer_latch,
};

static int mtk_srclken_spm_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	int ext_num = 0;
	int ret = 0;
	int i, j;

	cfg_base = of_iomap(node, 0);
	if (IS_ERR(cfg_base))
		return PTR_ERR(cfg_base);

	pr_info("cfg base: 0x%pR\n", cfg_base);

	sta_base = of_iomap(node, 1);
	if (IS_ERR(sta_base))
		return PTR_ERR(sta_base);

	pr_info("sta base: 0x%pR\n", sta_base);

	/* check each extend register exist or not */
	ext_num = of_property_count_strings(node, "cfg-ext");
	for (i = 0; i < ext_num; i++) {
		const char *comp;

		ret = of_property_read_string_index(node, "cfg-ext", i, &comp);
		for (j = CFG_EXT_START; j < CFG_EXT_END; j++) {
			if (!strcmp(comp, cfg_n[j])) {
				ext_reg.cfg_ext[j] = true;
				break;
			}
		}
	}

	ext_num = of_property_count_strings(node, "sta-ext");
	for (i = 0; i < ext_num; i++) {
		const char *comp;

		ret = of_property_read_string_index(node, "sta-ext", i, &comp);
		for (j = STA_EXT_START; j < STA_EXT_END; j++) {
			if (!strcmp(comp, sta_n[j])) {
				ext_reg.sta_ext[j] = true;
				break;
			}
		}
	}

	/* get trace offset for workaround */
	for (i = 0; i < TRACE_NUM; i++) {
		ret = of_property_read_u32_index(node, "trace-l-offset",
				i, &offset.lsb[i]);
		if (ret)
			pr_err("%s: cannot get trace lsb property(%d)\n",
					__func__, ret);

		ret = of_property_read_u32_index(node, "trace-m-offset",
				i, &offset.msb[i]);
		if (ret)
			pr_err("%s: cannot get trace msb property(%d)\n",
					__func__, ret);
	}

	set_srclken_ops(&srclken_spm_ops);

	return 0;
}

static const struct platform_device_id mtk_srclken_spm_ids[] = {
	{"mtk-srclken-spm", 0},
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(platform, mtk_srclken_spm_ids);

static const struct of_device_id mtk_srclken_spm_of_match[] = {
	{
		.compatible = "mediatek,srclken-spm",
	},
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, mtk_srclken_spm_of_match);

static struct platform_driver mtk_srclken_spm_driver = {
	.driver = {
		.name = "mtk-srclken-spm",
		.of_match_table = of_match_ptr(mtk_srclken_spm_of_match),
	},
	.probe = mtk_srclken_spm_probe,
	.id_table = mtk_srclken_spm_ids,
};

module_platform_driver(mtk_srclken_spm_driver);
MODULE_AUTHOR("Owen Chen <owen.chen@mediatek.com>");
MODULE_DESCRIPTION("SOC Driver for MediaTek SRCLKEN RC SPM");
MODULE_LICENSE("GPL");
