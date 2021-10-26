// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/module.h>
#include <linux/of_device.h>

#include <utilities/mdla_profile.h>
#include <utilities/mdla_debug.h>

#include "mdla_plat_internal.h"


static struct mdla_plat_drv mt6779_drv = {
	.init           = mdla_v1_0_init,
	.deinit         = mdla_v1_0_deinit,
	.sw_cfg         = 0,
	.klog           = (MDLA_DBG_CMD | MDLA_DBG_TIMEOUT),
	.timeout_ms     = 6000,
	.off_delay_ms   = 2000,
	.polling_cmd_ms = 5,
	.pmu_period_us  = 1000,
	.profile_ver    = PROF_V1,
};

static struct mdla_plat_drv mt6873_drv = {
	.init           = mdla_v1_x_init,
	.deinit         = mdla_v1_x_deinit,
	.sw_cfg         = BIT(CFG_NN_PMU_SUPPORT)
				| BIT(CFG_SW_PREEMPTION_SUPPORT),
	.klog           = (MDLA_DBG_CMD | MDLA_DBG_TIMEOUT),
	.timeout_ms     = 6000,
	.off_delay_ms   = 2000,
	.polling_cmd_ms = 5,
	.pmu_period_us  = 1000,
	.profile_ver    = PROF_V1,
};

static struct mdla_plat_drv mt6885_drv = {
	.init           = mdla_v1_x_init,
	.deinit         = mdla_v1_x_deinit,
	.sw_cfg         = BIT(CFG_NN_PMU_SUPPORT)
				| BIT(CFG_SW_PREEMPTION_SUPPORT),
	.klog           = (MDLA_DBG_CMD | MDLA_DBG_TIMEOUT),
	.timeout_ms     = 6000,
	.off_delay_ms   = 2000,
	.polling_cmd_ms = 5,
	.pmu_period_us  = 1000,
	.profile_ver    = PROF_V1,
};

static struct mdla_plat_drv mt6893_drv = {
	.init           = mdla_v1_x_init,
	.deinit         = mdla_v1_x_deinit,
	.sw_cfg         = BIT(CFG_NN_PMU_SUPPORT)
				| BIT(CFG_SW_PREEMPTION_SUPPORT),
	.klog           = (MDLA_DBG_CMD | MDLA_DBG_TIMEOUT),
	.timeout_ms     = 6000,
	.off_delay_ms   = 2000,
	.polling_cmd_ms = 5,
	.pmu_period_us  = 1000,
	.profile_ver    = PROF_V1,
};

static struct mdla_plat_drv mt6877_drv = {
	.init           = mdla_v2_0_init,
	.deinit         = mdla_v2_0_deinit,
	.sw_cfg         = BIT(CFG_NN_PMU_SUPPORT)
				| BIT(CFG_SW_PREEMPTION_SUPPORT),
	.klog           = (MDLA_DBG_CMD | MDLA_DBG_TIMEOUT),
	.timeout_ms     = 6000,
	.off_delay_ms   = 2000,
	.polling_cmd_ms = 5,
	.pmu_period_us  = 1000,
	.profile_ver    = PROF_V1,
};

static struct mdla_plat_drv rv_drv = {
	.init           = mdla_rv_init,
	.deinit         = mdla_rv_deinit,
	.sw_cfg         = BIT(CFG_MICRO_P_SUPPORT),
	.profile_ver    = PROF_NONE,
};


static const struct of_device_id mdla_of_match[] = {
	{ .compatible = "mediatek, mt6779-mdla", .data = &mt6779_drv},
	{ .compatible = "mediatek, mt6873-mdla", .data = &mt6873_drv},
	{ .compatible = "mediatek, mt6885-mdla", .data = &mt6885_drv},
	{ .compatible = "mediatek, mt6893-mdla", .data = &mt6893_drv},
	{ .compatible = "mediatek, mt6877-mdla", .data = &mt6877_drv},
	{ .compatible = "mediatek, mdla-rv", .data = &rv_drv},
	{ /* end of list */},
};
MODULE_DEVICE_TABLE(of, mdla_of_match);

const struct of_device_id *mdla_plat_get_device(void)
{
	return mdla_of_match;
}
