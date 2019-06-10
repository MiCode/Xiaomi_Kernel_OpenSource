// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/arm-smccc.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/interconnect.h>
#include <linux/pm_domain.h>
#include <linux/pm_opp.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>

#include "dvfsrc-mt6779.h"

#define MTK_SIP_VCOREFS_DRAM_TYPE 2
static u32 dvfsrc_read(struct mtk_dvfsrc *dvfs, u32 reg, u32 offset)
{
	return readl(dvfs->regs + dvfs->dvd->regs[reg] + offset);
}

enum dvfsrc_regs {
	DVFSRC_BASIC_CONTROL,
	DVFSRC_SW_REQ1,
	DVFSRC_INT,
	DVFSRC_INT_EN,
	DVFSRC_SW_BW_0,
	DVFSRC_ISP_HRT,
	DVFSRC_DEBUG_STA_0,
	DVFSRC_VCORE_REQUEST,
	DVFSRC_CURRENT_LEVEL,
	DVFSRC_TARGET_LEVEL,
	DVFSRC_LAST,
	DVFSRC_RECORD_0,
	DVFSRC_DDR_REQUEST,
	DVSFRC_HRT_REQ_MD_URG,
	DVFSRC_HRT_REQ_MD_BW_0,
	DVFSRC_HRT_REQ_MD_BW_8,
};

static const int mt6779_regs[] = {
	[DVFSRC_BASIC_CONTROL] = 0x0,
	[DVFSRC_SW_REQ1] = 0x4,
	[DVFSRC_INT] = 0xC4,
	[DVFSRC_INT_EN] = 0xC8,
	[DVFSRC_SW_BW_0] = 0x260,
	[DVFSRC_ISP_HRT] = 0x290,
	[DVFSRC_DEBUG_STA_0] = 0x700,
	[DVFSRC_VCORE_REQUEST] = 0x6C,
	[DVFSRC_CURRENT_LEVEL] = 0xD44,
	[DVFSRC_TARGET_LEVEL] = 0xD48,
	[DVFSRC_LAST] = 0xB08,
	[DVFSRC_RECORD_0] = 0xB14,
	[DVFSRC_DDR_REQUEST] = 0xA00,
	[DVSFRC_HRT_REQ_MD_URG] = 0xA88,
	[DVFSRC_HRT_REQ_MD_BW_0] = 0xA8C,
	[DVFSRC_HRT_REQ_MD_BW_8] = 0xACC,
};

static u32 dvfsrc_get_total_emi_req(struct mtk_dvfsrc *dvfsrc)
{
	/* DVFSRC_DEBUG_STA_2 */
	return dvfsrc_read(dvfsrc, DVFSRC_DEBUG_STA_0, 0x8) & 0xFFF;
}
static u32 dvfsrc_get_scp_req(struct mtk_dvfsrc *dvfsrc)
{
	/* DVFSRC_DEBUG_STA_2 */
	return (dvfsrc_read(dvfsrc, DVFSRC_DEBUG_STA_0, 0x8) >> 14) & 0x1;
}

static u32 dvfsrc_get_hifi_scenario(struct mtk_dvfsrc *dvfsrc)
{
	/* DVFSRC_DEBUG_STA_2 */
	return (dvfsrc_read(dvfsrc, DVFSRC_DEBUG_STA_0, 0x8) >> 16) & 0xFF;
}

static u32 dvfsrc_get_hifi_vcore_gear(struct mtk_dvfsrc *dvfsrc)
{
	u32 hifi_scen;

	hifi_scen = __builtin_ffs(dvfsrc_get_hifi_scenario(dvfsrc));

	if (hifi_scen)
		return (dvfsrc_read(dvfsrc, DVFSRC_VCORE_REQUEST, 0xC) >>
			((hifi_scen - 1) * 4)) & 0xF;
	else
		return 0;

}
static u32 dvfsrc_get_hifi_ddr_gear(struct mtk_dvfsrc *dvfsrc)
{
	u32 hifi_scen;

	hifi_scen = __builtin_ffs(dvfsrc_get_hifi_scenario(dvfsrc));

	if (hifi_scen)
		return (dvfsrc_read(dvfsrc, DVFSRC_DDR_REQUEST, 0x14) >>
			((hifi_scen - 1) * 4)) & 0xF;
	else
		return 0;
}

static u32 dvfsrc_get_hifi_rising_ddr_gear(struct mtk_dvfsrc *dvfsrc)
{
	u32 offset;

	offset = 0x18 + 0x1C * dvfsrc_read(dvfsrc, DVFSRC_LAST, 0);
	/* DVFSRC_RECORD_0_6 */

	return (dvfsrc_read(dvfsrc, DVFSRC_RECORD_0, offset) >> 15) & 0x7;
}

static u32 dvfsrc_get_md_bw(struct mtk_dvfsrc *dvfsrc)
{
	u32 is_urgent, md_scen;
	u32 val;
	u32 index, shift;

	val = dvfsrc_read(dvfsrc, DVFSRC_DEBUG_STA_0, 0);
	is_urgent = (val >> 16) & 0x1;
	md_scen = val & 0xFFFF;

	if (is_urgent) {
		val = dvfsrc_read(dvfsrc, DVSFRC_HRT_REQ_MD_URG, 0) & 0x1F;
	} else {
		index = md_scen / 3;
		shift = (md_scen % 3) * 10;

		if (index > 10)
			return 0;

		if (index < 8) {
			val = dvfsrc_read(dvfsrc, DVFSRC_HRT_REQ_MD_BW_0,
				index * 4);
		} else {
			val = dvfsrc_read(dvfsrc, DVFSRC_HRT_REQ_MD_BW_8,
				(index - 8) * 4);
		}
		val = (val >> shift) & 0x3FF;
	}
	return val;
}

static u32 dvfsrc_get_md_rising_ddr_gear(struct mtk_dvfsrc *dvfsrc)
{
	u32 val;
	u32 last = dvfsrc_read(dvfsrc, DVFSRC_LAST, 0);

	/* DVFSRC_RECORD_0_6 */
	val = dvfsrc_read(dvfsrc, DVFSRC_RECORD_0, 0x18 + 0x1C * last);

	return (val >> 9) & 0x7;
}

static u32 dvfsrc_get_hrt_bw_ddr_gear(struct mtk_dvfsrc *dvfsrc)
{
	u32 val;
	u32 last = dvfsrc_read(dvfsrc, DVFSRC_LAST, 0);

	/* DVFSRC_RECORD_0_6 */
	val = dvfsrc_read(dvfsrc, DVFSRC_RECORD_0, 0x18 + 0x1C * last);

	return (val >> 2) & 0x7;
}

static char *dvfsrc_dump_info(struct mtk_dvfsrc *dvfsrc, char *p, u32 size)
{
	int vcore_uv = 0;
	char *buff_end = p + size;

	if (dvfsrc->vcore_power)
		vcore_uv = regulator_get_voltage(dvfsrc->vcore_power);

	p += snprintf(p, buff_end - p, "%-10s: %-8u uv\n",
			"Vcore", vcore_uv);
	p += snprintf(p, buff_end - p, "\n");

	return p;
}

static char *dvfsrc_dump_record(struct mtk_dvfsrc *dvfsrc, char *p, u32 size)
{
	int i, rec_offset, offset;
	char *buff_end = p + size;

	p += sprintf(p, "%-17s: 0x%08x\n",
			"DVFSRC_LAST",
			dvfsrc_read(dvfsrc, DVFSRC_LAST, 0));

	if (dvfsrc->dvd->ip_verion > 0)
		rec_offset = 0x20;
	else
		rec_offset = 0x1C;

	for (i = 0; i < 8; i++) {
		offset = i * rec_offset;
		p += snprintf(p, buff_end - p,
			"[%d]%-14s: %08x,%08x,%08x,%08x\n",
			i,
			"DVFSRC_REC 0~3",
			dvfsrc_read(dvfsrc, DVFSRC_RECORD_0, offset + 0x0),
			dvfsrc_read(dvfsrc, DVFSRC_RECORD_0, offset + 0x4),
			dvfsrc_read(dvfsrc, DVFSRC_RECORD_0, offset + 0x8),
			dvfsrc_read(dvfsrc, DVFSRC_RECORD_0, offset + 0xC));
		if (dvfsrc->dvd->ip_verion > 0) {
			p += snprintf(p, buff_end - p,
			"[%d]%-14s: %08x,%08x,%08x,%08x\n",
			i,
			"DVFSRC_REC 4~7",
			dvfsrc_read(dvfsrc, DVFSRC_RECORD_0, offset + 0x10),
			dvfsrc_read(dvfsrc, DVFSRC_RECORD_0, offset + 0x14),
			dvfsrc_read(dvfsrc, DVFSRC_RECORD_0, offset + 0x18),
			dvfsrc_read(dvfsrc, DVFSRC_RECORD_0, offset + 0x1C));
		} else {
			p += snprintf(p, buff_end - p,
			"[%d]%-14s: %08x,%08x,%08x\n",
			i,
			"DVFSRC_REC 4~6",
			dvfsrc_read(dvfsrc, DVFSRC_RECORD_0, offset + 0x10),
			dvfsrc_read(dvfsrc, DVFSRC_RECORD_0, offset + 0x14),
			dvfsrc_read(dvfsrc, DVFSRC_RECORD_0, offset + 0x18));
		}
	}
	p += snprintf(p, buff_end - p, "\n");

	return p;
}

static char *dvfsrc_dump_reg(struct mtk_dvfsrc *dvfsrc, char *p, u32 size)
{
	char *buff_end = p + size;

	p += snprintf(p, buff_end - p, "%-16s: 0x%08x\n",
		"BASIC_CONTROL",
		dvfsrc_read(dvfsrc, DVFSRC_BASIC_CONTROL, 0x0));
	p += snprintf(p, buff_end - p,
		"%-16s: %08x, %08x, %08x, %08x\n",
		"SW_REQ 1~4",
		dvfsrc_read(dvfsrc, DVFSRC_SW_REQ1, 0x0),
		dvfsrc_read(dvfsrc, DVFSRC_SW_REQ1, 0x4),
		dvfsrc_read(dvfsrc, DVFSRC_SW_REQ1, 0x8),
		dvfsrc_read(dvfsrc, DVFSRC_SW_REQ1, 0xC));

	p += snprintf(p, buff_end - p,
		"%-16s: %08x, %08x, %08x, %08x\n",
		"SW_REQ 5~8",
		dvfsrc_read(dvfsrc, DVFSRC_SW_REQ1, 0x10),
		dvfsrc_read(dvfsrc, DVFSRC_SW_REQ1, 0x14),
		dvfsrc_read(dvfsrc, DVFSRC_SW_REQ1, 0x18),
		dvfsrc_read(dvfsrc, DVFSRC_SW_REQ1, 0x1C));

	p += snprintf(p, buff_end - p, "%-16s: %d, %d, %d, %d, %d\n",
		"SW_BW_0~4",
		dvfsrc_read(dvfsrc, DVFSRC_SW_BW_0, 0x0),
		dvfsrc_read(dvfsrc, DVFSRC_SW_BW_0, 0x4),
		dvfsrc_read(dvfsrc, DVFSRC_SW_BW_0, 0x8),
		dvfsrc_read(dvfsrc, DVFSRC_SW_BW_0, 0xC),
		dvfsrc_read(dvfsrc, DVFSRC_SW_BW_0, 0x10));

	if (dvfsrc->dvd->ip_verion > 1)
		p += snprintf(p, buff_end - p, "%-16s: %d, %d\n",
		"SW_BW_5~6",
		dvfsrc_read(dvfsrc, DVFSRC_SW_BW_0, 0x14),
		dvfsrc_read(dvfsrc, DVFSRC_SW_BW_0, 0x18));


	p += snprintf(p, buff_end - p, "%-16s: 0x%08x\n",
		"ISP_HRT",
		dvfsrc_read(dvfsrc, DVFSRC_ISP_HRT, 0x0));

	p += snprintf(p, buff_end - p, "%-16s: 0x%08x, 0x%08x, 0x%08x\n",
		"DEBUG_STA",
		dvfsrc_read(dvfsrc, DVFSRC_DEBUG_STA_0, 0x0),
		dvfsrc_read(dvfsrc, DVFSRC_DEBUG_STA_0, 0x4),
		dvfsrc_read(dvfsrc, DVFSRC_DEBUG_STA_0, 0x8));

	p += snprintf(p, buff_end - p, "%-16s: 0x%08x\n",
		"DVFSRC_INT",
		dvfsrc_read(dvfsrc, DVFSRC_INT, 0x0));

	p += snprintf(p, buff_end - p, "%-16s: 0x%08x\n",
		"DVFSRC_INT_EN",
		dvfsrc_read(dvfsrc, DVFSRC_INT_EN, 0x0));

	p += snprintf(p, buff_end - p, "%-16s: 0x%02x\n",
		"TOTAL_EMI_REQ",
		dvfsrc_get_total_emi_req(dvfsrc));

	p += snprintf(p, buff_end - p, "%-16s: %d\n",
		"MD_RISING_REQ",
		dvfsrc_get_md_rising_ddr_gear(dvfsrc));

	p += snprintf(p, buff_end - p, "%-16s: 0x%08x\n",
		"MD_HRT_BW",
		dvfsrc_get_md_bw(dvfsrc));

	p += sprintf(p, "%-16s: %d\n",
		"HRT_BW_REQ",
		dvfsrc_get_hrt_bw_ddr_gear(dvfsrc));

	p += snprintf(p, buff_end - p, "%-16s: %d\n",
		"HIFI_VCORE_REQ",
		dvfsrc_get_hifi_vcore_gear(dvfsrc));

	p += snprintf(p, buff_end - p, "%-16s: %d\n",
		"HIFI_DDR_REQ",
		dvfsrc_get_hifi_ddr_gear(dvfsrc));

	p += snprintf(p, buff_end - p, "%-16s: %d\n",
		"HIFI_RISINGREQ",
		dvfsrc_get_hifi_rising_ddr_gear(dvfsrc));

	p += snprintf(p, buff_end - p, "%-16s: %d , 0x%08x\n",
			"SCP_VCORE_REQ",
			dvfsrc_get_scp_req(dvfsrc),
			dvfsrc_read(dvfsrc, DVFSRC_VCORE_REQUEST, 0x0));
	p += snprintf(p, buff_end - p, "%-16s: 0x%08x\n",
			"CURRENT_LEVEL",
			dvfsrc_read(dvfsrc, DVFSRC_CURRENT_LEVEL, 0x0));
	p += snprintf(p, buff_end - p, "%-16s: 0x%08x\n",
			"TARGET_LEVEL",
			dvfsrc_read(dvfsrc, DVFSRC_TARGET_LEVEL, 0x0));
	p += snprintf(p, buff_end - p, "\n");

	return p;
}

static int mtk_dvfsrc_debug_probe(struct platform_device *pdev)
{
	struct arm_smccc_res ares;
	struct device *dev = &pdev->dev;
	struct platform_device *parent_dev;
	struct resource *res;
	struct mtk_dvfsrc *dvfsrc;
	struct device_node *np = pdev->dev.of_node;
	int i;

	parent_dev = to_platform_device(dev->parent);
	if (!parent_dev)
		return -ENODEV;

	dvfsrc = devm_kzalloc(&pdev->dev, sizeof(*dvfsrc), GFP_KERNEL);
	if (!dvfsrc)
		return -ENOMEM;

	dvfsrc->dvd = of_device_get_match_data(&pdev->dev);
	dvfsrc->dev = &pdev->dev;

	arm_smccc_smc(MTK_SIP_VCOREFS_CONTROL, MTK_SIP_VCOREFS_DRAM_TYPE,
		0, 0, 0, 0, 0, 0,
		&ares);

	if (!ares.a0)
		dvfsrc->dram_type = ares.a1;
	else {
		dev_info(dev, "init fails: %lu\n", ares.a0);
		return ares.a0;
	}

	dvfsrc->num_perf = of_count_phandle_with_args(np,
		   "required-opps", NULL);

	if (dvfsrc->num_perf > 0) {
		dvfsrc->perfs = devm_kzalloc(&pdev->dev,
			 dvfsrc->num_perf * sizeof(int),
			GFP_KERNEL);

		if (!dvfsrc->perfs)
			return -ENOMEM;

		for (i = 0; i < dvfsrc->num_perf; i++) {
			dvfsrc->perfs[i] =
				of_get_required_opp_performance_state(np, i);
		}
	} else {
		dvfsrc->num_perf = 0;
	}

	res = platform_get_resource_byname(parent_dev,
			IORESOURCE_MEM, "dvfsrc");
	if (!res) {
		dev_info(dev, "dvfsrc debug resource not found\n");
		return -ENODEV;
	}

	dvfsrc->regs = devm_ioremap(&pdev->dev, res->start,
		resource_size(res));

	if (IS_ERR(dvfsrc->regs))
		return PTR_ERR(dvfsrc->regs);

	dvfsrc->path = of_icc_get(&pdev->dev, "icc-bw-port");
	if (IS_ERR(dvfsrc->path))
		return IS_ERR(dvfsrc->path);

	dvfsrc->vcore_power = regulator_get(&pdev->dev, "vcore");
	if (IS_ERR(dvfsrc->vcore_power)) {
		dev_info(dev, "regulator_get vcore failed = %d\n",
			dvfsrc->vcore_power);
		dvfsrc->vcore_power = NULL;
	}

	dvfsrc->dvfsrc_vcore_power = regulator_get(&pdev->dev, "rc-vcore");
	if (IS_ERR(dvfsrc->dvfsrc_vcore_power)) {
		dev_info(dev, "mtk_dvfsrc dvfsrc vcore failed = %d\n",
			dvfsrc->dvfsrc_vcore_power);
		dvfsrc->dvfsrc_vcore_power = NULL;
	}

	dvfsrc->dvfsrc_vscp_power = regulator_get(&pdev->dev, "rc-vscp");
	if (IS_ERR(dvfsrc->dvfsrc_vscp_power)) {
		dev_info(dev, "regulator_get dvfsrc vscp failed = %d\n",
			dvfsrc->dvfsrc_vscp_power);
		dvfsrc->dvfsrc_vscp_power = NULL;
	}

	mt6779_dvfsrc_register_sysfs(dev);

	platform_set_drvdata(pdev, dvfsrc);
	return 0;
}

static const struct dvfsrc_opp dvfsrc_opp_mt6779_lp4[] = {
	{0, 0, 650000, 819000},
	{0, 1, 650000, 1200000},
	{0, 2, 650000, 1534000},
	{0, 3, 650000, 2400000},
	{1, 1, 725000, 1200000},
	{1, 2, 725000, 1534000},
	{1, 3, 725000, 2400000},
	{1, 4, 725000, 3094000},
	{2, 1, 825000, 1200000},
	{2, 2, 825000, 1534000},
	{2, 3, 825000, 2400000},
	{2, 4, 825000, 3094000},
	{2, 5, 825000, 3733000},
};

static const struct dvfsrc_opp *dvfsrc_opp_mt6779[] = {
	[0] = dvfsrc_opp_mt6779_lp4,
};

static const struct dvfsrc_debug_data mt6779_data = {
	.opps = dvfsrc_opp_mt6779,
	.num_opp = ARRAY_SIZE(dvfsrc_opp_mt6779_lp4),
	.regs = mt6779_regs,
	.dump_info = dvfsrc_dump_info,
	.dump_record = dvfsrc_dump_record,
	.dump_reg = dvfsrc_dump_reg,
	.ip_verion = 0,
};

static int mtk_dvfsrc_debug_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	mt6779_dvfsrc_unregister_sysfs(dev);
	return 0;
}


static const struct of_device_id mtk_dvfsrc_debug_of_match[] = {
	{
		.compatible = "mediatek,mt6779-dvfsrc-debug",
		.data = &mt6779_data,
	}, {
		/* sentinel */
	},
};

static struct platform_driver mtk_dvfsrc_debug_driver = {
	.probe	= mtk_dvfsrc_debug_probe,
	.remove	= mtk_dvfsrc_debug_remove,
	.driver = {
		.name = "mtk-dvfsrc-debug",
		.of_match_table = of_match_ptr(mtk_dvfsrc_debug_of_match),
	},
};

static int __init mtk_dvfsrc_debug_init(void)
{
	return platform_driver_register(&mtk_dvfsrc_debug_driver);
}
late_initcall(mtk_dvfsrc_debug_init)

static void __exit mtk_dvfsrc_debug_exit(void)
{
	platform_driver_unregister(&mtk_dvfsrc_debug_driver);
}
module_exit(mtk_dvfsrc_debug_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MTK DVFSRC debug driver");
