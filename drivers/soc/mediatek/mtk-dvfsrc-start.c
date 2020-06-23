// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */
#include <linux/arm-smccc.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>

#define MTK_SIP_DVFSRC_START		0x01

/* We let dvfsrc working on parent main driver and setup
 * framework (interconnect, regulator, ..) for user register
 * and request, so we will lock high opp in that stage.
 * We will release this lock in this driver and let probe on
 * late_initsync stage. It send smc cmd MTK_SIP_DVFSRC_RUN
 * to release lock let dvfsrc free run.
 */

static int mtk_dvfsrc_start_probe(struct platform_device *pdev)
{
	struct arm_smccc_res ares;

	arm_smccc_smc(MTK_SIP_VCOREFS_CONTROL, MTK_SIP_DVFSRC_START, 0, 0, 0,
		0, 0, 0, &ares);

	return 0;
}

static struct platform_driver mtk_dvfsrc_run_drv = {
	.probe	= mtk_dvfsrc_start_probe,
	.driver = {
		.name	= "mtk-dvfsrc-start",
	},
};

static int __init mtk_dvfsrc_run_init(void)
{
	return platform_driver_register(&mtk_dvfsrc_run_drv);
}
late_initcall_sync(mtk_dvfsrc_run_init);

static void __exit mtk_dvfsrc_run_exit(void)
{
	platform_driver_unregister(&mtk_dvfsrc_run_drv);
}
module_exit(mtk_dvfsrc_run_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MTK DVFSRC enable free run driver");
