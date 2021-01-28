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

#include "dvfsrc.h"
#include "dvfsrc-opp.h"

#define MTK_DVFSRC_MET_SUPPORT 1

#define MTK_SIP_VCOREFS_KICK 1
#define MTK_SIP_VCOREFS_DRAM_TYPE 2
#define MTK_SIP_VCOREFS_VCORE_UV  4
#define MTK_SIP_VCOREFS_DRAM_FREQ 5

#define MT_DVFSRC_OPP(_num_vcore, _num_ddr, _opp_table)	\
{	\
	.num_vcore_opp = _num_vcore,	\
	.num_dram_opp = _num_ddr,	\
	.opps = _opp_table,	\
	.num_opp = ARRAY_SIZE(_opp_table),	\
}

static struct mtk_dvfsrc *dvfsrc_drv;

#if MTK_DVFSRC_MET_SUPPORT
enum met_info_index {
	INFO_OPP_IDX = 0,
	INFO_FREQ_IDX,
	INFO_VCORE_IDX,
	INFO_SPM_LEVEL_IDX,
	INFO_MAX,
};

static unsigned int met_vcorefs_info[INFO_MAX];
static char *met_info_name[INFO_MAX] = {
	"OPP",
	"FREQ",
	"VCORE",
	"x__SPM_LEVEL",
};

int vcorefs_get_num_opp(void)
{
	return  mtk_dvfsrc_query_opp_info(MTK_DVFSRC_NUM_DVFS_OPP);
}
EXPORT_SYMBOL(vcorefs_get_num_opp);

int vcorefs_get_opp_info_num(void)
{
	return INFO_MAX;
}
EXPORT_SYMBOL(vcorefs_get_opp_info_num);

char **vcorefs_get_opp_info_name(void)
{
	return met_info_name;
}
EXPORT_SYMBOL(vcorefs_get_opp_info_name);

unsigned int *vcorefs_get_opp_info(void)
{
	struct mtk_dvfsrc *dvfsrc = dvfsrc_drv;
	const struct dvfsrc_opp *opp;
	int level;

	if (dvfsrc) {
		level = dvfsrc->dvd->config->get_current_level(dvfsrc);
		opp = &dvfsrc->opp_desc->opps[level];

		met_vcorefs_info[INFO_OPP_IDX] =
			dvfsrc->opp_desc->num_opp - (level + 1);
		met_vcorefs_info[INFO_FREQ_IDX] = opp->dram_khz;
		met_vcorefs_info[INFO_VCORE_IDX] = opp->vcore_uv;
		met_vcorefs_info[INFO_SPM_LEVEL_IDX] =
			dvfsrc->dvd->config->get_current_rglevel(dvfsrc);
	}
	return met_vcorefs_info;
}
EXPORT_SYMBOL(vcorefs_get_opp_info);

int vcorefs_get_src_req_num(void)
{
	struct mtk_dvfsrc *dvfsrc = dvfsrc_drv;

	if (dvfsrc && dvfsrc->dvd->met) {
		if (dvfsrc->dvd->met->dvfsrc_get_src_req_num)
			return dvfsrc->dvd->met->dvfsrc_get_src_req_num();
		else
			return 0;
	}
	return 0;
}
EXPORT_SYMBOL(vcorefs_get_src_req_num);
char **vcorefs_get_src_req_name(void)
{
	struct mtk_dvfsrc *dvfsrc = dvfsrc_drv;

	if (dvfsrc && dvfsrc->dvd->met) {
		if (dvfsrc->dvd->met->dvfsrc_get_src_req_name)
			return dvfsrc->dvd->met->dvfsrc_get_src_req_name();
		else
			return NULL;
	}
	return NULL;
}
EXPORT_SYMBOL(vcorefs_get_src_req_name);
unsigned int *vcorefs_get_src_req(void)
{
	struct mtk_dvfsrc *dvfsrc = dvfsrc_drv;

	if (dvfsrc && dvfsrc->dvd->met) {
		if (dvfsrc->dvd->met->dvfsrc_get_src_req)
			return dvfsrc->dvd->met->dvfsrc_get_src_req(dvfsrc);
		else
			return NULL;
	}
	return NULL;
}
EXPORT_SYMBOL(vcorefs_get_src_req);
int get_cur_ddr_ratio(void)
{
	struct mtk_dvfsrc *dvfsrc = dvfsrc_drv;

	if (dvfsrc && dvfsrc->dvd->met) {
		if (dvfsrc->dvd->met->dvfsrc_get_ddr_ratio)
			return dvfsrc->dvd->met->dvfsrc_get_ddr_ratio(dvfsrc);
		else
			return 0;
	}

	return 0;
}
EXPORT_SYMBOL(get_cur_ddr_ratio);
#endif

static void dvfsrc_setup_opp_table(struct mtk_dvfsrc *dvfsrc)
{
	int i;
	struct dvfsrc_opp *opp;
	struct arm_smccc_res ares;

	for (i = 0; i < dvfsrc->opp_desc->num_opp; i++) {
		opp = &dvfsrc->opp_desc->opps[i];
		arm_smccc_smc(MTK_SIP_VCOREFS_CONTROL,
			MTK_SIP_VCOREFS_VCORE_UV,
			opp->vcore_opp, 0, 0, 0, 0, 0,
			&ares);

		if (!ares.a0)
			opp->vcore_uv = ares.a1;

		arm_smccc_smc(MTK_SIP_VCOREFS_CONTROL,
			MTK_SIP_VCOREFS_DRAM_FREQ,
			opp->dram_opp, 0, 0, 0, 0, 0,
			&ares);
		if (!ares.a0)
			opp->dram_khz = ares.a1;
	}
}

static int dvfsrc_query_opp_info(u32 id)
{
	struct mtk_dvfsrc *dvfsrc = dvfsrc_drv;
	const struct dvfsrc_opp *opp;
	int ret = 0;
	int level;

	if (!dvfsrc)
		return 0;

	level = dvfsrc->dvd->config->get_current_level(dvfsrc);
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
		dev_info(dev, "get dram type fails\n");
		return ares.a0;
	}

	arm_smccc_smc(MTK_SIP_VCOREFS_CONTROL, MTK_SIP_VCOREFS_KICK,
		0, 0, 0, 0, 0, 0,
		&ares);

	if (ares.a0) {
		dev_info(dev, "vcore_dvfs kick fail\n");
		return ares.a0;
	}

	if (dvfsrc->dram_type > dvfsrc->dvd->num_opp_desc)
		return -EINVAL;

	dvfsrc->opp_desc = &dvfsrc->dvd->opps_desc[dvfsrc->dram_type];
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
				dvfsrc_get_required_opp_performance_state(np, i);
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
	if (IS_ERR(dvfsrc->path)) {
		dev_info(dev, "get icc-bw-port fail\n");
		dvfsrc->path = NULL;
	}

	dvfsrc->vcore_power =
		regulator_get_optional(&pdev->dev, "vcore");
	if (IS_ERR(dvfsrc->vcore_power)) {
		dev_info(dev, "regulator_get vcore failed = %ld\n",
			PTR_ERR(dvfsrc->vcore_power));
		dvfsrc->vcore_power = NULL;
	}

	dvfsrc->dvfsrc_vcore_power =
		regulator_get_optional(&pdev->dev, "rc-vcore");
	if (IS_ERR(dvfsrc->dvfsrc_vcore_power)) {
		dev_info(dev, "regulator_get dvfsrc vcore failed = %ld\n",
			PTR_ERR(dvfsrc->dvfsrc_vcore_power));
		dvfsrc->dvfsrc_vcore_power = NULL;
	}

	dvfsrc->dvfsrc_vscp_power =
		regulator_get_optional(&pdev->dev, "rc-vscp");
	if (IS_ERR(dvfsrc->dvfsrc_vscp_power)) {
		dev_info(dev, "regulator_get dvfsrc vscp failed = %ld\n",
			PTR_ERR(dvfsrc->dvfsrc_vscp_power));
		dvfsrc->dvfsrc_vscp_power = NULL;
	}

	dvfsrc->force_opp_idx = dvfsrc->opp_desc->num_opp;
	dvfsrc_register_sysfs(dev);
	if (dvfsrc->dvd->setup_opp_table)
		dvfsrc->dvd->setup_opp_table(dvfsrc);

	if (dvfsrc->dvd->qos)
		dvfsrc->dvd->qos->qos_dvfsrc_init(dvfsrc);

	dvfsrc_drv = dvfsrc;

	register_dvfsrc_opp_handler(dvfsrc_query_opp_info);

	platform_set_drvdata(pdev, dvfsrc);

	return 0;
}

static struct dvfsrc_opp dvfsrc_opp_mt6779_lp4[] = {
	{0, 0, 0, 0},
	{0, 1, 0, 0},
	{0, 2, 0, 0},
	{0, 3, 0, 0},
	{1, 1, 0, 0},
	{1, 2, 0, 0},
	{1, 3, 0, 0},
	{1, 4, 0, 0},
	{2, 1, 0, 0},
	{2, 2, 0, 0},
	{2, 3, 0, 0},
	{2, 4, 0, 0},
	{2, 5, 0, 0},
};

static struct dvfsrc_opp_desc dvfsrc_opp_mt6779_desc[] = {
	MT_DVFSRC_OPP(3, 6, dvfsrc_opp_mt6779_lp4),
};

static const struct dvfsrc_debug_data mt6779_data = {
	.opps_desc = dvfsrc_opp_mt6779_desc,
	.num_opp_desc = ARRAY_SIZE(dvfsrc_opp_mt6779_desc),
	.config = &mt6779_dvfsrc_config,
	.met = &mt6779_met_config,
	.qos = &mt6779_qos_config,
	.setup_opp_table = dvfsrc_setup_opp_table,
};

static int mtk_dvfsrc_debug_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	dvfsrc_unregister_sysfs(dev);
	dvfsrc_drv = NULL;
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
late_initcall_sync(mtk_dvfsrc_debug_init)

static void __exit mtk_dvfsrc_debug_exit(void)
{
	platform_driver_unregister(&mtk_dvfsrc_debug_driver);
}
module_exit(mtk_dvfsrc_debug_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MTK DVFSRC debug driver");
