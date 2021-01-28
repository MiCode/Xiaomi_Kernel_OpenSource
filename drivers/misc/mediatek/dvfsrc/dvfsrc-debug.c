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

#include "dvfsrc-debug.h"
#include "dvfsrc-common.h"
#include <aee.h>

static struct mtk_dvfsrc *dvfsrc_drv;

static int dvfsrc_query_debug_info(u32 id)
{
	struct mtk_dvfsrc *dvfsrc = dvfsrc_drv;
	const struct dvfsrc_config *config;
	int ret;

	config = dvfsrc_drv->dvd->config;
	ret = config->query_request(dvfsrc, id);

	return ret;
}
static void mtk_dvfsrc_get_perf_state(struct mtk_dvfsrc *dvfsrc,
	struct device_node *np)
{
	int i;

	for (i = 0; i < dvfsrc->num_perf; i++) {
		dvfsrc->perfs[i] =
			dvfsrc_get_required_opp_performance_state(np, i);
	}
}

static void dvfsrc_setup_vopp_table(struct mtk_dvfsrc *dvfsrc)
{
	int i;
	struct arm_smccc_res ares;
	u32 num_vopp;

	arm_smccc_smc(MTK_SIP_VCOREFS_CONTROL,
		MTK_SIP_VCOREFS_GET_NUM_V,
		0, 0, 0, 0, 0, 0,
		&ares);

	if (!ares.a0)
		dvfsrc->num_vopp = ares.a1;
	else
		return;

	num_vopp = dvfsrc->num_vopp;

	dvfsrc->vopp_uv_tlb = devm_kzalloc(dvfsrc->dev,
		num_vopp * sizeof(u32), GFP_KERNEL);

	if (!dvfsrc->vopp_uv_tlb)
		return;

	for (i = 0; i < num_vopp; i++) {
		arm_smccc_smc(MTK_SIP_VCOREFS_CONTROL,
			MTK_SIP_VCOREFS_GET_VCORE_UV,
			i, 0, 0, 0, 0, 0,
			&ares);

		if (!ares.a0)
			dvfsrc->vopp_uv_tlb[i] = ares.a1;
		else {
			dvfsrc->num_vopp = 0;
			break;
		}
	}

	for (i = 0; i < num_vopp; i++)
		dev_info(dvfsrc->dev, "dvfsrc gear uv[%d] = %d\n",
			i, dvfsrc->vopp_uv_tlb[i]);

}

static int dvfsrc_vcore_check(struct notifier_block *b,
				 unsigned long l, void *v)
{
	u32 gear = l;
	int vcore_uv = 0;
	int predict_uv;
	struct mtk_dvfsrc *dvfsrc;

	dvfsrc = container_of(b, struct mtk_dvfsrc, dvfsrc_vchk_notifier);

	if ((!dvfsrc->vcore_power) || (gear > dvfsrc->num_vopp))
		return NOTIFY_DONE;

	predict_uv = dvfsrc->vopp_uv_tlb[gear];
	vcore_uv = regulator_get_voltage(dvfsrc->vcore_power);

	if (vcore_uv < predict_uv) {
		dev_info(dvfsrc->dev, "VCORE CHECK FAIL= %d %d, %d\n",
			gear, vcore_uv, predict_uv);
		return NOTIFY_BAD;
	}

	return NOTIFY_DONE;
}

static void dvfsrc_vchk_notifier(struct mtk_dvfsrc *dvfsrc)
{
	dvfsrc->dvfsrc_vchk_notifier.notifier_call = dvfsrc_vcore_check;
	register_dvfsrc_vchk_notifier(&dvfsrc->dvfsrc_vchk_notifier);
}

static int mtk_dvfsrc_debug_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct platform_device *parent_dev;
	struct resource *res;
	struct mtk_dvfsrc *dvfsrc;
	struct device_node *np = pdev->dev.of_node;

	parent_dev = to_platform_device(dev->parent);
	dvfsrc = devm_kzalloc(&pdev->dev, sizeof(*dvfsrc), GFP_KERNEL);
	if (!dvfsrc)
		return -ENOMEM;

	dvfsrc->dvd = of_device_get_match_data(&pdev->dev);
	dvfsrc->dev = &pdev->dev;
	dvfsrc->num_perf = of_count_phandle_with_args(np,
		   "required-opps", NULL);

	if (dvfsrc->num_perf > 0) {
		dvfsrc->perfs = devm_kzalloc(&pdev->dev,
			 dvfsrc->num_perf * sizeof(int),
			GFP_KERNEL);

		if (!dvfsrc->perfs)
			return -ENOMEM;

		mtk_dvfsrc_get_perf_state(dvfsrc, np);
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

	res = platform_get_resource_byname(parent_dev,
			IORESOURCE_MEM, "spm");
	if (res) {
		dvfsrc->spm_regs = devm_ioremap(&pdev->dev, res->start,
			resource_size(res));
		if (IS_ERR(dvfsrc->spm_regs))
			dvfsrc->spm_regs = NULL;
	}

	dvfsrc->vcore_power =
		regulator_get_optional(&pdev->dev, "vcore");
	if (IS_ERR(dvfsrc->vcore_power)) {
		dev_info(dev, "get debug vcore failed = %ld\n",
			PTR_ERR(dvfsrc->vcore_power));
		dvfsrc->vcore_power = NULL;
	}

	if (!dvfsrc->dvd->pmqos_enable) {
		dvfsrc->dvfsrc_vcore_power =
			regulator_get_optional(&pdev->dev, "rc-vcore");
		if (IS_ERR(dvfsrc->dvfsrc_vcore_power)) {
			dev_info(dev, "get dvfsrc_vcore failed = %ld\n",
				PTR_ERR(dvfsrc->dvfsrc_vcore_power));
			dvfsrc->dvfsrc_vcore_power = NULL;
		}

		dvfsrc->dvfsrc_vscp_power =
			regulator_get_optional(&pdev->dev, "rc-vscp");
		if (IS_ERR(dvfsrc->dvfsrc_vscp_power)) {
			dev_info(dev, "get dvfsrc vscp failed = %ld\n",
				PTR_ERR(dvfsrc->dvfsrc_vscp_power));
			dvfsrc->dvfsrc_vscp_power = NULL;
		}

		dvfsrc->path = of_icc_get(&pdev->dev, "icc-bw-port");
		if (IS_ERR(dvfsrc->path)) {
			dev_info(dev, "get icc-bw-port fail\n");
			dvfsrc->path = NULL;
		}
	}

	dvfsrc->force_opp_idx =
		mtk_dvfsrc_query_opp_info(MTK_DVFSRC_NUM_DVFS_OPP);

	dvfsrc_register_sysfs(dev);

	dvfsrc_setup_vopp_table(dvfsrc);
	dvfsrc_vchk_notifier(dvfsrc);

	dvfsrc_drv = dvfsrc;

	register_dvfsrc_debug_handler(dvfsrc_query_debug_info);

	platform_set_drvdata(pdev, dvfsrc);

	return 0;
}

static const struct dvfsrc_debug_data mt6779_data = {
	.pmqos_enable = true,
	.config = &mt6779_dvfsrc_config,
};

static const struct dvfsrc_debug_data mt6761_data = {
	.pmqos_enable = true,
	.config = &mt6761_dvfsrc_config,
};

static int mtk_dvfsrc_debug_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_dvfsrc *dvfsrc = platform_get_drvdata(pdev);

	unregister_dvfsrc_vchk_notifier(&dvfsrc->dvfsrc_vchk_notifier);
	dvfsrc_unregister_sysfs(dev);
	dvfsrc_drv = NULL;
	return 0;
}


static const struct of_device_id mtk_dvfsrc_debug_of_match[] = {
	{
		.compatible = "mediatek,mt6779-dvfsrc-debug",
		.data = &mt6779_data,
	}, {
		.compatible = "mediatek,mt6761-dvfsrc-debug",
		.data = &mt6761_data,
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

