// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/arm-smccc.h>
#include <linux/clk.h>
#include <linux/fb.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <linux/soc/mediatek/mtk_dvfsrc.h>
#include <linux/soc/mediatek/mtk-pm-qos.h>

#include "dvfsrc.h"
#include "dvfsrc-common.h"
#include <dvfsrc-exp.h>

#define MT_DVFSRC_OPP(_num_vcore, _num_ddr, _opp_table)	\
{	\
	.num_vcore_opp = _num_vcore,	\
	.num_dram_opp = _num_ddr,	\
	.opps = _opp_table,	\
	.num_opp = ARRAY_SIZE(_opp_table),	\
}

static struct mtk_dvfsrc_up *dvfsrc_up_drv;

#if defined(CONFIG_MACH_MT6761) || defined(CONFIG_MACH_MT6765)
/* gps workarund function */
static struct mtk_pm_qos_request gps_ddr_req;
#define MTK_SIP_VCOREFS_DVFS_HOPPING 19

static void spm_freq_hopping_cmd(int gps_on)
{
	struct arm_smccc_res ares;

	arm_smccc_smc(MTK_SIP_VCOREFS_CONTROL,
		MTK_SIP_VCOREFS_DVFS_HOPPING,
		gps_on, 0, 0, 0, 0, 0,
		&ares);
}
static void dvfsrc_enable_dvfs_gps_hopping(int gps_on)
{
	struct mtk_dvfsrc_up *dvfsrc = dvfsrc_up_drv;

	if (dvfsrc->fw_type == 2)
		return;

	mtk_pm_qos_update_request(&gps_ddr_req, DDR_OPP_0);
	spm_freq_hopping_cmd(!!gps_on);
	mtk_pm_qos_update_request(&gps_ddr_req, DDR_OPP_UNREQ);
}
#endif
static void dvfsrc_setup_opp_table(struct mtk_dvfsrc_up *dvfsrc)
{
	int i;
	struct dvfsrc_opp *opp;
	struct arm_smccc_res ares;

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
	struct mtk_dvfsrc_up *dvfsrc = dvfsrc_up_drv;
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

static ssize_t dvfsrc_opp_table_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int i, j;
	char *p = buf;
	char *buff_end = p + PAGE_SIZE;
	struct mtk_dvfsrc_up *dvfsrc = dev_get_drvdata(dev);

	p += snprintf(p, buff_end - p,
		"NUM_VCORE_OPP : %d\n",
		dvfsrc->opp_desc->num_vcore_opp);
	p += snprintf(p, buff_end - p,
		"NUM_DDR_OPP : %d\n",
		dvfsrc->opp_desc->num_dram_opp);
	p += snprintf(p, buff_end - p,
		"NUM_DVFSRC_OPP : %d\n\n",
		dvfsrc->opp_desc->num_opp);

	for (i = 0; i < dvfsrc->opp_desc->num_opp; i++) {
		j = dvfsrc->opp_desc->num_opp - (i + 1);
		p += snprintf(p, buff_end - p,
			"[OPP%-2d]: %-8u uv %-8u khz\n",
			i,
			dvfsrc->opp_desc->opps[j].vcore_uv,
			dvfsrc->opp_desc->opps[j].dram_khz);
	}

	return p - buf;
}
static DEVICE_ATTR_RO(dvfsrc_opp_table);

static struct attribute *dvfsrc_attrs[] = {
	&dev_attr_dvfsrc_opp_table.attr,
	NULL,
};

static struct attribute_group dvfsrc_up_attr_group = {
	.attrs = dvfsrc_attrs,
};

static int dvfsrc_up_register_sysfs(struct device *dev)
{
	int ret;

	ret = sysfs_create_group(&dev->kobj, &dvfsrc_up_attr_group);
	if (ret)
		return ret;

	ret = sysfs_create_link(&dev->parent->kobj, &dev->kobj,
		"dvfsrc-up");

	return ret;
}

static void dvfsrc_up_unregister_sysfs(struct device *dev)
{
	sysfs_remove_link(&dev->parent->kobj, "dvfsrc-up");
	sysfs_remove_group(&dev->kobj, &dvfsrc_up_attr_group);
}

static void dvfsrc_cm_ddr_request(u32 level)
{
	struct mtk_dvfsrc_up *dvfsrc = dvfsrc_up_drv;

	mtk_dvfsrc_send_request(dvfsrc->dev->parent,
		MTK_DVFSRC_CMD_EXT_DRAM_REQUEST,
		level);
}

static void dvfsrc_update_fb_action(bool blank)
{
	struct arm_smccc_res ares;

	arm_smccc_smc(MTK_SIP_VCOREFS_CONTROL,
		MTK_SIP_VCOREFS_FB_ACTION,
		blank, 0, 0, 0, 0, 0,
		&ares);
}
static int dvfsrc_fb_notifier_call(struct notifier_block *self,
		unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int blank;

	if (event != FB_EVENT_BLANK)
		return 0;

	blank = *(int *)evdata->data;
	switch (blank) {
	case FB_BLANK_UNBLANK:
		dvfsrc_update_fb_action(false);
		break;
	case FB_BLANK_POWERDOWN:
		dvfsrc_update_fb_action(true);
		break;
	default:
		break;
	}
	return 0;
}

static struct notifier_block dvfsrc_fb_notifier = {
	.notifier_call = dvfsrc_fb_notifier_call,
};

static int mtk_dvfsrc_up_probe(struct platform_device *pdev)
{
	struct arm_smccc_res ares;
	struct device *dev = &pdev->dev;
	struct mtk_dvfsrc_up *dvfsrc;

	dvfsrc = devm_kzalloc(&pdev->dev, sizeof(*dvfsrc), GFP_KERNEL);
	if (!dvfsrc)
		return -ENOMEM;

	dvfsrc->dvd = of_device_get_match_data(&pdev->dev);
	dvfsrc->dev = &pdev->dev;

	arm_smccc_smc(MTK_SIP_VCOREFS_CONTROL, MTK_SIP_VCOREFS_GET_OPP_TYPE,
		0, 0, 0, 0, 0, 0,
		&ares);

	if (!ares.a0)
		dvfsrc->opp_type = ares.a1;
	else {
		dev_info(dev, "get opp type fails\n");
		return ares.a0;
	}

	arm_smccc_smc(MTK_SIP_VCOREFS_CONTROL, MTK_SIP_VCOREFS_GET_FW_TYPE,
		0, 0, 0, 0, 0, 0,
		&ares);

	if (!ares.a0)
		dvfsrc->fw_type = ares.a1;
	else {
		dev_info(dev, "get fw type fails\n");
		return ares.a0;
	}

	arm_smccc_smc(MTK_SIP_VCOREFS_CONTROL, MTK_SIP_VCOREFS_KICK,
		0, 0, 0, 0, 0, 0,
		&ares);

	if (ares.a0) {
		dev_info(dev, "vcore_dvfs kick fail\n");
		return ares.a0;
	}

	if (dvfsrc->opp_type > dvfsrc->dvd->num_opp_desc)
		return -EINVAL;

	dvfsrc->opp_desc = &dvfsrc->dvd->opps_desc[dvfsrc->opp_type];

	if (dvfsrc->dvd->setup_opp_table)
		dvfsrc->dvd->setup_opp_table(dvfsrc);

	if (dvfsrc->dvd->qos)
		dvfsrc->dvd->qos->qos_dvfsrc_init(dvfsrc);

	if (dvfsrc->dvd->fb_act_enable) {
		if (fb_register_client(&dvfsrc_fb_notifier))
			dev_info(dev, "unable to register fb\n");
	}

	dvfsrc_up_drv = dvfsrc;

	register_dvfsrc_opp_handler(dvfsrc_query_info);
	register_dvfsrc_cm_ddr_handler(dvfsrc_cm_ddr_request);
#if defined(CONFIG_MACH_MT6761) || defined(CONFIG_MACH_MT6765)
	mtk_pm_qos_add_request(&gps_ddr_req,
		MTK_PM_QOS_DDR_OPP, DDR_OPP_UNREQ);
	register_dvfsrc_hopping_handler(dvfsrc_enable_dvfs_gps_hopping);
#endif
	platform_set_drvdata(pdev, dvfsrc);
	dvfsrc_up_register_sysfs(dev);

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

static const struct dvfsrc_up_data mt6779_data = {
	.opps_desc = dvfsrc_opp_mt6779_desc,
	.num_opp_desc = ARRAY_SIZE(dvfsrc_opp_mt6779_desc),
	.qos = &mt6779_qos_config,
	.setup_opp_table = dvfsrc_setup_opp_table,
};

static struct dvfsrc_opp dvfsrc_opp_mt6761[] = {
	{0, 0, 0, 0},
	{1, 0, 0, 0},
	{1, 0, 0, 0},
	{2, 0, 0, 0},
	{2, 1, 0, 0},
	{2, 0, 0, 0},
	{2, 1, 0, 0},
	{2, 1, 0, 0},
	{3, 1, 0, 0},
	{3, 2, 0, 0},
	{3, 1, 0, 0},
	{3, 2, 0, 0},
	{3, 1, 0, 0},
	{3, 2, 0, 0},
	{3, 2, 0, 0},
	{3, 2, 0, 0},
};

static struct dvfsrc_opp_desc dvfsrc_opp_mt6761_desc[] = {
	MT_DVFSRC_OPP(4, 3, dvfsrc_opp_mt6761),
};

static const struct dvfsrc_up_data mt6761_data = {
	.fb_act_enable = true,
	.opps_desc = dvfsrc_opp_mt6761_desc,
	.num_opp_desc = ARRAY_SIZE(dvfsrc_opp_mt6761_desc),
	.qos = &mt6761_qos_config,
	.setup_opp_table = dvfsrc_setup_opp_table,
};

static int mtk_dvfsrc_up_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	dvfsrc_up_unregister_sysfs(dev);
	dvfsrc_up_drv = NULL;
	return 0;
}

static const struct of_device_id mtk_dvfsrc_up_of_match[] = {
	{
		.compatible = "mediatek,mt6779-dvfsrc-up",
		.data = &mt6779_data,
	}, {
		.compatible = "mediatek,mt6761-dvfsrc-up",
		.data = &mt6761_data,
	}, {
		.compatible = "mediatek,mt6765-dvfsrc-up",
		.data = &mt6761_data,
	}, {
		/* sentinel */
	},
};

static struct platform_driver mtk_dvfsrc_up_driver = {
	.probe	= mtk_dvfsrc_up_probe,
	.remove	= mtk_dvfsrc_up_remove,
	.driver = {
		.name = "mtk-dvfsrc-up",
		.of_match_table = of_match_ptr(mtk_dvfsrc_up_of_match),
	},
};

static int __init mtk_dvfsrc_up_init(void)
{
	return platform_driver_register(&mtk_dvfsrc_up_driver);
}
late_initcall_sync(mtk_dvfsrc_up_init)

static void __exit mtk_dvfsrc_up_exit(void)
{
	platform_driver_unregister(&mtk_dvfsrc_up_driver);
}
module_exit(mtk_dvfsrc_up_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MTK DVFSRC up driver");
