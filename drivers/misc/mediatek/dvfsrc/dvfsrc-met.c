// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/arm-smccc.h>
#include <linux/clk.h>
#include <linux/interconnect.h>
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

#include "dvfsrc-met.h"

static struct mtk_dvfsrc_met *dvfsrc_drv;

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
	struct mtk_dvfsrc_met *dvfsrc = dvfsrc_drv;

	if (dvfsrc) {
		met_vcorefs_info[INFO_OPP_IDX] =
			mtk_dvfsrc_query_opp_info(MTK_DVFSRC_CURR_DVFS_OPP);

		met_vcorefs_info[INFO_FREQ_IDX] =
			mtk_dvfsrc_query_opp_info(MTK_DVFSRC_CURR_DRAM_KHZ);

		met_vcorefs_info[INFO_VCORE_IDX] =
			mtk_dvfsrc_query_opp_info(MTK_DVFSRC_CURR_VCORE_UV);

		met_vcorefs_info[INFO_SPM_LEVEL_IDX] =
			dvfsrc->dvd->met->get_current_level(dvfsrc);
	}
	return met_vcorefs_info;
}
EXPORT_SYMBOL(vcorefs_get_opp_info);

int vcorefs_get_src_req_num(void)
{
	struct mtk_dvfsrc_met *dvfsrc = dvfsrc_drv;

	if (dvfsrc)
		return dvfsrc->dvd->met->dvfsrc_get_src_req_num();

	return 0;
}
EXPORT_SYMBOL(vcorefs_get_src_req_num);
char **vcorefs_get_src_req_name(void)
{
	struct mtk_dvfsrc_met *dvfsrc = dvfsrc_drv;

	if (dvfsrc)
		return dvfsrc->dvd->met->dvfsrc_get_src_req_name();

	return NULL;
}
EXPORT_SYMBOL(vcorefs_get_src_req_name);

unsigned int *vcorefs_get_src_req(void)
{
	struct mtk_dvfsrc_met *dvfsrc = dvfsrc_drv;

	if (dvfsrc)
		return dvfsrc->dvd->met->dvfsrc_get_src_req(dvfsrc);

	return NULL;
}
EXPORT_SYMBOL(vcorefs_get_src_req);
int get_cur_ddr_ratio(void)
{
	struct mtk_dvfsrc_met *dvfsrc = dvfsrc_drv;

	if (dvfsrc)
		return dvfsrc->dvd->met->dvfsrc_get_ddr_ratio(dvfsrc);

	return 0;
}
EXPORT_SYMBOL(get_cur_ddr_ratio);

static ssize_t dvfsrc_met_dump_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char *p = buf;
	char *buff_end = p + PAGE_SIZE;
	char **name;
	unsigned int *value;
	int i, res_num;

	p += snprintf(p, buff_end - p,
		"NUM_VCORE_OPP : %d\n",
		vcorefs_get_num_opp());

	res_num = vcorefs_get_opp_info_num();
	name = vcorefs_get_opp_info_name();
	value = vcorefs_get_opp_info();
	p += snprintf(p, buff_end - p,
		"NUM_OPP_INFO : %d\n", res_num);
	for (i = 0; i < res_num; i++) {
		p += snprintf(p, buff_end - p,
			"%s : %d\n",
			name[i], value[i]);
	}

	res_num = vcorefs_get_src_req_num();
	name = vcorefs_get_src_req_name();
	value = vcorefs_get_src_req();
	p += snprintf(p, buff_end - p,
		"NUM SRC_REQ: %d\n", res_num);
	if (name && value) {
		for (i = 0; i < res_num; i++) {
			p += snprintf(p, buff_end - p,
				"%s : %d\n",
				name[i], value[i]);
		}
	}

	return p - buf;
}
static DEVICE_ATTR_RO(dvfsrc_met_dump);

static struct attribute *dvfsrc_attrs[] = {
	&dev_attr_dvfsrc_met_dump.attr,
	NULL,
};

static struct attribute_group dvfsrc_met_attr_group = {
	.attrs = dvfsrc_attrs,
};

static int dvfsrc_met_register_sysfs(struct device *dev)
{
	int ret;

	ret = sysfs_create_group(&dev->kobj, &dvfsrc_met_attr_group);
	if (ret)
		return ret;

	ret = sysfs_create_link(&dev->parent->kobj, &dev->kobj,
		"met");

	return ret;
}

static void dvfsrc_met_unregister_sysfs(struct device *dev)
{
	sysfs_remove_link(&dev->parent->kobj, "met");
	sysfs_remove_group(&dev->kobj, &dvfsrc_met_attr_group);
}

static const struct dvfsrc_met_data mt6873_data = {
	.met = &mt6873_met_config,
	.version = 0x6873,
};

static const struct dvfsrc_met_data mt6853_data = {
	.met = &mt6873_met_config,
	.version = 0x6853,
};

static const struct dvfsrc_met_data mt6789_data = {
	.met = &mt6873_met_config,
	.version = 0x6789,
};

static const struct dvfsrc_met_data mt6885_data = {
	.met = &mt6873_met_config,
	.version = 0x6885,
};

static const struct dvfsrc_met_data mt6893_data = {
	.met = &mt6873_met_config,
	.version = 0x6893,
};

static const struct dvfsrc_met_data mt6833_data = {
	.met = &mt6873_met_config,
	.version = 0x6833,
};

static const struct dvfsrc_met_data mt6877_data = {
	.met = &mt6873_met_config,
	.version = 0x6877,
};

static const struct dvfsrc_met_data mt6983_data = {
	.met = &mt6983_met_config,
	.version = 0x6983,
};

static const struct dvfsrc_met_data mt6895_data = {
	.met = &mt6983_met_config,
	.version = 0x6895,
};

static const struct dvfsrc_met_data mt6879_data = {
	.met = &mt6983_met_config,
	.version = 0x6879,
};

static const struct dvfsrc_met_data mt6855_data = {
	.met = &mt6983_met_config,
	.version = 0x6855,
};

static const struct dvfsrc_met_data mt6768_data = {
	.met = &mt6768_met_config,
	.version = 0x6768,
};

static const struct dvfsrc_met_data mt6765_data = {
	.met = &mt6768_met_config,
	.version = 0x6765,
};
static const struct of_device_id dvfsrc_met_of_match[] = {
#if IS_ENABLED(CONFIG_MTK_DVFSRC_MET_MT6873)
	{
		.compatible = "mediatek,mt6873-dvfsrc",
		.data = &mt6873_data,
	}, {
		.compatible = "mediatek,mt6853-dvfsrc",
		.data = &mt6853_data,
	}, {
		.compatible = "mediatek,mt6789-dvfsrc",
		.data = &mt6789_data,
	}, {
		.compatible = "mediatek,mt6885-dvfsrc",
		.data = &mt6885_data,
	}, {
		.compatible = "mediatek,mt6893-dvfsrc",
		.data = &mt6893_data,
	}, {
		.compatible = "mediatek,mt6833-dvfsrc",
		.data = &mt6833_data,
	}, {
		.compatible = "mediatek,mt6877-dvfsrc",
		.data = &mt6877_data,
	}, {
		.compatible = "mediatek,mt6983-dvfsrc",
		.data = &mt6983_data,
	}, {
		.compatible = "mediatek,mt6895-dvfsrc",
		.data = &mt6895_data,
	}, {
		.compatible = "mediatek,mt6879-dvfsrc",
		.data = &mt6879_data,
	}, {
		.compatible = "mediatek,mt6855-dvfsrc",
		.data = &mt6855_data,
	},
#endif
#if IS_ENABLED(CONFIG_MTK_DVFSRC_MET_MT6768)
	{
		.compatible = "mediatek,mt6768-dvfsrc",
		.data = &mt6768_data,
	},
#endif
#if IS_ENABLED(CONFIG_MTK_DVFSRC_MET_MT6765)
	{
		.compatible = "mediatek,mt6765-dvfsrc",
		.data = &mt6765_data,
	},
#endif
	{
		/* sentinel */
	},
};

static int mtk_dvfsrc_met_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	struct device *dev = &pdev->dev;
	struct platform_device *parent_dev;
	struct resource *res;
	struct mtk_dvfsrc_met *dvfsrc;

	match = of_match_node(dvfsrc_met_of_match, dev->parent->of_node);
	if (!match) {
		dev_info(dev, "invalid compatible string\n");
		return -ENODEV;
	}

	dvfsrc = devm_kzalloc(&pdev->dev, sizeof(*dvfsrc), GFP_KERNEL);
	if (!dvfsrc)
		return -ENOMEM;

	parent_dev = to_platform_device(dev->parent);
	dvfsrc->dvd = match->data;
	dvfsrc->dev = &pdev->dev;

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

	dvfsrc->dvfsrc_vcore_power =
		regulator_get_optional(dvfsrc->dev, "rc-vcore");
	if (IS_ERR(dvfsrc->dvfsrc_vcore_power)) {
		dev_info(dvfsrc->dev, "get dvfsrc_vcore failed = %ld\n",
			PTR_ERR(dvfsrc->dvfsrc_vcore_power));
		dvfsrc->dvfsrc_vcore_power = NULL;
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

	dvfsrc_drv = dvfsrc;
	platform_set_drvdata(pdev, dvfsrc);
	dvfsrc_met_register_sysfs(dev);

	return 0;
}

static int mtk_dvfsrc_met_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	dvfsrc_met_unregister_sysfs(dev);
	dvfsrc_drv = NULL;
	return 0;
}

static const struct of_device_id mtk_dvfsrc_met_of_match[] = {
	{
		.compatible = "mediatek,dvfsrc-met",
	}, {
		/* sentinel */
	},
};

static struct platform_driver mtk_dvfsrc_met_driver = {
	.probe	= mtk_dvfsrc_met_probe,
	.remove	= mtk_dvfsrc_met_remove,
	.driver = {
		.name = "mtk-dvfsrc-met",
		.of_match_table = of_match_ptr(mtk_dvfsrc_met_of_match),
	},
};

int __init mtk_dvfsrc_met_init(void)
{
	return platform_driver_register(&mtk_dvfsrc_met_driver);
}

void __exit mtk_dvfsrc_met_exit(void)
{
	platform_driver_unregister(&mtk_dvfsrc_met_driver);
}

