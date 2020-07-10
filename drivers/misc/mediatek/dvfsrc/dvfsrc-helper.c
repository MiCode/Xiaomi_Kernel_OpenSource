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
#include <linux/soc/mediatek/mtk_dvfsrc.h>
#if IS_ENABLED(CONFIG_MTK_DRAMC)
#include <soc/mediatek/dramc.h>
#endif
#include "dvfsrc-helper.h"
#include "dvfsrc-common.h"

static struct mtk_dvfsrc *dvfsrc_drv;

/* OPP */
#define MT_DVFSRC_OPP(_num_vcore, _num_ddr, _opp_table)	\
{	\
	.num_vcore_opp = _num_vcore,	\
	.num_dram_opp = _num_ddr,	\
	.opps = _opp_table,	\
	.num_opp = ARRAY_SIZE(_opp_table),	\
}

int (*dvfsrc_query_opp_info)(u32 id);
void register_dvfsrc_opp_handler(int (*handler)(u32 id))
{
	dvfsrc_query_opp_info = handler;
}

int mtk_dvfsrc_query_opp_info(u32 id)
{
	if (dvfsrc_query_opp_info != NULL)
		return dvfsrc_query_opp_info(id);

	return 0;
}
EXPORT_SYMBOL(mtk_dvfsrc_query_opp_info);

int mtk_dvfsrc_vcore_uv_table(u32 opp)
{
	u32 opp_idx;

	if (!dvfsrc_drv)
		return 0;

	if (opp >= dvfsrc_drv->opp_desc->num_vcore_opp)
		return 0;

	opp_idx = dvfsrc_drv->opp_desc->num_vcore_opp - opp - 1;

	return dvfsrc_drv->vopp_uv_tlb[opp_idx];
}
EXPORT_SYMBOL(mtk_dvfsrc_vcore_uv_table);

static void dvfsrc_setup_opp_table(struct mtk_dvfsrc *dvfsrc)
{
	int i;
	struct dvfsrc_opp *opp;
	struct arm_smccc_res ares;

	for (i = 0; i < dvfsrc->opp_desc->num_vcore_opp; i++) {
		arm_smccc_smc(MTK_SIP_VCOREFS_CONTROL,
			MTK_SIP_VCOREFS_GET_VCORE_UV,
			i, 0, 0, 0, 0, 0,
			&ares);

		if (!ares.a0)
			dvfsrc->vopp_uv_tlb[i] = ares.a1;
	}

	for (i = 0; i < dvfsrc->opp_desc->num_vcore_opp; i++)
		dev_info(dvfsrc->dev, "dvfsrc vopp[%d] = %d\n",
			i, dvfsrc->vopp_uv_tlb[i]);

	for (i = 0; i < dvfsrc->opp_desc->num_opp; i++) {
		opp = &dvfsrc->opp_desc->opps[i];
		arm_smccc_smc(MTK_SIP_VCOREFS_CONTROL,
			MTK_SIP_VCOREFS_GET_VCORE_UV,
			opp->vcore_opp, 0, 0, 0, 0, 0,
			&ares);

		if (!ares.a0)
			opp->vcore_uv = ares.a1;

		arm_smccc_smc(MTK_SIP_VCOREFS_CONTROL,
			MTK_SIP_VCOREFS_GET_DRAM_FREQ,
			opp->dram_opp, 0, 0, 0, 0, 0,
			&ares);
		if (!ares.a0)
			opp->dram_khz = ares.a1;
	}
}

static int dvfsrc_query_info(u32 id)
{
	struct mtk_dvfsrc *dvfsrc = dvfsrc_drv;
	const struct dvfsrc_opp *opp;
	int ret = 0;
	int level = 0;

	ret = mtk_dvfsrc_query_info(dvfsrc->dev->parent,
		MTK_DVFSRC_CMD_CURR_LEVEL_QUERY, &level);

	if (ret || level >= dvfsrc->opp_desc->num_opp)
		return 0;

	opp = &dvfsrc->opp_desc->opps[level];

	switch (id) {
	case MTK_DVFSRC_NUM_DVFS_OPP:
		ret = dvfsrc->opp_desc->num_opp;
		break;
	case MTK_DVFSRC_NUM_DRAM_OPP:
		ret = dvfsrc->opp_desc->num_dram_opp;
		break;
	case MTK_DVFSRC_NUM_VCORE_OPP:
		ret = dvfsrc->opp_desc->num_vcore_opp;
		break;
	case MTK_DVFSRC_CURR_DVFS_OPP:
		ret = dvfsrc->opp_desc->num_opp
				- (level + 1);
		break;
	case MTK_DVFSRC_CURR_DRAM_OPP:
		ret = dvfsrc->opp_desc->num_dram_opp
				- (opp->dram_opp + 1);
		break;
	case MTK_DVFSRC_CURR_VCORE_OPP:
		ret = dvfsrc->opp_desc->num_vcore_opp
				- (opp->vcore_opp + 1);
		break;
	case MTK_DVFSRC_CURR_DVFS_LEVEL:
		ret = level;
		break;
	case MTK_DVFSRC_CURR_DRAM_KHZ:
		ret = opp->dram_khz;
		break;
	case MTK_DVFSRC_CURR_VCORE_UV:
		ret = opp->vcore_uv;
		break;
	}

	return ret;
}
static int mtk_dvfsrc_opp_setting(struct mtk_dvfsrc *dvfsrc)
{
	struct arm_smccc_res ares;

	arm_smccc_smc(MTK_SIP_VCOREFS_CONTROL, MTK_SIP_VCOREFS_GET_OPP_TYPE,
		0, 0, 0, 0, 0, 0,
		&ares);

	if (!ares.a0)
		dvfsrc->opp_type = ares.a1;
	else {
		dev_info(dvfsrc->dev, "get opp type fails\n");
		return ares.a0;
	}

	arm_smccc_smc(MTK_SIP_VCOREFS_CONTROL, MTK_SIP_VCOREFS_GET_FW_TYPE,
		0, 0, 0, 0, 0, 0,
		&ares);

	if (!ares.a0)
		dvfsrc->fw_type = ares.a1;
	else {
		dev_info(dvfsrc->dev, "get fw type fails\n");
		return ares.a0;
	}

	if (dvfsrc->opp_type > dvfsrc->dvd->num_opp_desc)
		return -EINVAL;

	dvfsrc->opp_desc = &dvfsrc->dvd->opps_desc[dvfsrc->opp_type];

	dvfsrc->vopp_uv_tlb = devm_kzalloc(dvfsrc->dev,
				dvfsrc->opp_desc->num_vcore_opp * sizeof(u32),
				GFP_KERNEL);
	if (!dvfsrc->vopp_uv_tlb)
		return -ENOMEM;

	dvfsrc_setup_opp_table(dvfsrc);

	return 0;
}

/* OPP END */
/* DEBUG */
int (*query_debug_info_handle)(u32 id);
void register_dvfsrc_debug_handler(int (*handler)(u32 id))
{
	query_debug_info_handle = handler;
}
EXPORT_SYMBOL(register_dvfsrc_debug_handler);

int mtk_dvfsrc_query_debug_info(u32 id)
{
	if (query_debug_info_handle != NULL)
		return query_debug_info_handle(id);

	return 0;
}
EXPORT_SYMBOL(mtk_dvfsrc_query_debug_info);

static int dvfsrc_query_debug_info(u32 id)
{
	struct mtk_dvfsrc *dvfsrc = dvfsrc_drv;
	const struct dvfsrc_config *config;
	int ret;

	config = dvfsrc_drv->dvd->config;
	ret = config->query_request(dvfsrc, id);

	return ret;
}

static char *dvfsrc_dump_info(struct mtk_dvfsrc *dvfsrc,
	char *p, u32 size)
{
	int vcore_uv = 0;
	char *buff_end = p + size;

	if (dvfsrc->vcore_power)
		vcore_uv = regulator_get_voltage(dvfsrc->vcore_power);

	p += snprintf(p, buff_end - p, "%-10s: %-8u uv\n",
			"Vcore", vcore_uv);
#if IS_ENABLED(CONFIG_MTK_DRAMC)
	p += snprintf(p, buff_end - p, "%-10s: %-8u khz\n",
			"DDR", mtk_dramc_get_data_rate() * 1000);
#endif
	p += snprintf(p, buff_end - p, "%-15s: %d\n",
			"FORCE_OPP_IDX",
			dvfsrc->force_opp_idx);
	p += snprintf(p, buff_end - p, "%-15s: %d\n",
			"CURR_DVFS_OPP",
			mtk_dvfsrc_query_opp_info(MTK_DVFSRC_CURR_DVFS_OPP));
	p += snprintf(p, buff_end - p, "%-15s: %d\n",
			"CURR_VCORE_OPP",
			mtk_dvfsrc_query_opp_info(MTK_DVFSRC_CURR_VCORE_OPP));
	p += snprintf(p, buff_end - p, "%-15s: %d\n",
			"CURR_DRAM_OPP",
			mtk_dvfsrc_query_opp_info(MTK_DVFSRC_CURR_DRAM_OPP));
	p += snprintf(p, buff_end - p, "\n");

	return p;
}

static void dvfsrc_force_opp(struct mtk_dvfsrc *dvfsrc, u32 opp)
{
	dvfsrc->force_opp_idx = opp;
	mtk_dvfsrc_send_request(dvfsrc->dev->parent,
		MTK_DVFSRC_CMD_FORCEOPP_REQUEST,
		opp);
}

static void mtk_dvfsrc_get_perf_bw(struct mtk_dvfsrc *dvfsrc,
	struct device_node *np)
{
	int i;

	for (i = 0; i < dvfsrc->num_perf; i++) {
		dvfsrc->perfs_peak_bw[i] =
			dvfsrc_get_required_opp_peak_bw(np, i);
	}
}

static int mtk_dvfsrc_debug_setting(struct mtk_dvfsrc *dvfsrc)
{
	struct device_node *np = dvfsrc->dev->of_node;

	dvfsrc->num_perf = of_count_phandle_with_args(np,
		   "required-opps", NULL);

	if (dvfsrc->num_perf > 0) {
		dvfsrc->perfs_peak_bw = devm_kzalloc(dvfsrc->dev,
			 dvfsrc->num_perf * sizeof(u32),
			GFP_KERNEL);

		if (!dvfsrc->perfs_peak_bw)
			return -ENOMEM;

		mtk_dvfsrc_get_perf_bw(dvfsrc, np);
	} else {
		dvfsrc->num_perf = 0;
	}

	dvfsrc->vcore_power =
		regulator_get_optional(dvfsrc->dev, "vcore");
	if (IS_ERR(dvfsrc->vcore_power)) {
		dev_info(dvfsrc->dev, "get vcore failed = %ld\n",
			PTR_ERR(dvfsrc->vcore_power));
		dvfsrc->vcore_power = NULL;
	}

	dvfsrc->dvfsrc_vcore_power =
		regulator_get_optional(dvfsrc->dev, "rc-vcore");
	if (IS_ERR(dvfsrc->dvfsrc_vcore_power)) {
		dev_info(dvfsrc->dev, "get dvfsrc_vcore failed = %ld\n",
			PTR_ERR(dvfsrc->dvfsrc_vcore_power));
		dvfsrc->dvfsrc_vcore_power = NULL;
	}

	dvfsrc->dvfsrc_vscp_power =
		regulator_get_optional(dvfsrc->dev, "rc-vscp");
	if (IS_ERR(dvfsrc->dvfsrc_vscp_power)) {
		dev_info(dvfsrc->dev, "get dvfsrc vscp failed = %ld\n",
			PTR_ERR(dvfsrc->dvfsrc_vscp_power));
		dvfsrc->dvfsrc_vscp_power = NULL;
	}

	dvfsrc->bw_path = of_icc_get(dvfsrc->dev, "icc-bw");
	if (IS_ERR(dvfsrc->bw_path)) {
		dev_info(dvfsrc->dev, "get icc-bw fail\n");
		dvfsrc->bw_path = NULL;
	}

	dvfsrc->hrt_path = of_icc_get(dvfsrc->dev, "icc-hrt-bw");
	if (IS_ERR(dvfsrc->hrt_path)) {
		dev_info(dvfsrc->dev, "get icc-hrt_bw fail\n");
		dvfsrc->hrt_path = NULL;
	}

	dvfsrc->perf_path = of_icc_get(dvfsrc->dev, "icc-perf-bw");
	if (IS_ERR(dvfsrc->hrt_path)) {
		dev_info(dvfsrc->dev, "get icc-perf_bw fail\n");
		dvfsrc->hrt_path = NULL;
	}

	dvfsrc->force_opp_idx =
		mtk_dvfsrc_query_opp_info(MTK_DVFSRC_NUM_DVFS_OPP);

	dvfsrc->force_opp = dvfsrc_force_opp;
	dvfsrc->dump_info = dvfsrc_dump_info;

	return 0;
}
/* DEBUG END*/

static struct dvfsrc_opp dvfsrc_opp_mt6873_lp4[] = {
	{0, 0, 0, 0},
	{1, 0, 0, 0},
	{2, 0, 0, 0},
	{3, 0, 0, 0},
	{0, 1, 0, 0},
	{1, 1, 0, 0},
	{2, 1, 0, 0},
	{3, 1, 0, 0},
	{0, 2, 0, 0},
	{1, 2, 0, 0},
	{2, 2, 0, 0},
	{3, 2, 0, 0},
	{1, 3, 0, 0},
	{2, 3, 0, 0},
	{3, 3, 0, 0},
	{1, 4, 0, 0},
	{2, 4, 0, 0},
	{3, 4, 0, 0},
	{2, 5, 0, 0},
	{3, 5, 0, 0},
	{3, 6, 0, 0},
};

static struct dvfsrc_opp_desc dvfsrc_opp_mt6873_desc[] = {
	MT_DVFSRC_OPP(4, 7, dvfsrc_opp_mt6873_lp4),
};

static const struct dvfsrc_debug_data mt6873_data = {
	.config = &mt6873_dvfsrc_config,
	.opps_desc = dvfsrc_opp_mt6873_desc,
	.num_opp_desc = ARRAY_SIZE(dvfsrc_opp_mt6873_desc),
};

static const struct of_device_id dvfsrc_helper_of_match[] = {
	{
		.compatible = "mediatek,mt6873-dvfsrc",
		.data = &mt6873_data,
	}, {
		/* sentinel */
	},
};

static int mtk_dvfsrc_helper_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	struct device *dev = &pdev->dev;
	struct platform_device *parent_dev;
	struct resource *res;
	struct mtk_dvfsrc *dvfsrc;
	int ret;

	match = of_match_node(dvfsrc_helper_of_match, dev->parent->of_node);
	if (!match) {
		dev_info(dev, "invalid compatible string\n");
		return -ENODEV;
	}

	parent_dev = to_platform_device(dev->parent);
	dvfsrc = devm_kzalloc(&pdev->dev, sizeof(*dvfsrc), GFP_KERNEL);
	if (!dvfsrc)
		return -ENOMEM;

	dvfsrc->dvd = match->data;
	dvfsrc->dev = &pdev->dev;

	res = platform_get_resource_byname(parent_dev,
			IORESOURCE_MEM, "dvfsrc");
	if (!res) {
		dev_info(dev, "resource not found\n");
		return -ENODEV;
	}

	dvfsrc->regs = devm_ioremap(&pdev->dev, res->start,
		resource_size(res));
	if (IS_ERR(dvfsrc->regs))
		return PTR_ERR(dvfsrc->regs);

	res = platform_get_resource_byname(parent_dev,
			IORESOURCE_MEM, "spm");
	if (res) {
		dvfsrc->spm_regs = devm_ioremap(&pdev->dev, res->start,
			resource_size(res));
		if (IS_ERR(dvfsrc->spm_regs))
			dvfsrc->spm_regs = NULL;
	}

	ret = mtk_dvfsrc_opp_setting(dvfsrc);
	if (ret) {
		dev_info(dev, "dvfsrc opp setting fail\n");
		return ret;
	}

	ret = mtk_dvfsrc_debug_setting(dvfsrc);
	if (ret) {
		dev_info(dev, "dvfsrc debug setting fail\n");
		return ret;
	}

	dvfsrc_register_sysfs(dev);
	register_dvfsrc_debug_handler(dvfsrc_query_debug_info);
	platform_set_drvdata(pdev, dvfsrc);
	dvfsrc_drv = dvfsrc;

	return 0;
}

static int mtk_dvfsrc_helper_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	dvfsrc_unregister_sysfs(dev);
	dvfsrc_drv = NULL;
	return 0;
}

static const struct of_device_id mtk_dvfsrc_helper_of_match[] = {
	{
		.compatible = "mediatek,dvfsrc-helper",
	}, {
		/* sentinel */
	},
};

static struct platform_driver mtk_dvfsrc_helper_driver = {
	.probe	= mtk_dvfsrc_helper_probe,
	.remove	= mtk_dvfsrc_helper_remove,
	.driver = {
		.name = "mtk-dvfsrc-helper",
		.of_match_table = of_match_ptr(mtk_dvfsrc_helper_of_match),
	},
};

static int __init mtk_dvfsrc_helper_init(void)
{
	return platform_driver_register(&mtk_dvfsrc_helper_driver);
}
late_initcall_sync(mtk_dvfsrc_helper_init)

static void __exit mtk_dvfsrc_helper_exit(void)
{
	platform_driver_unregister(&mtk_dvfsrc_helper_driver);
}
module_exit(mtk_dvfsrc_helper_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MTK DVFSRC helper driver");

