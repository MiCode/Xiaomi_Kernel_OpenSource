/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __MDLA_PLAT_INTERNAL_H__
#define __MDLA_PLAT_INTERNAL_H__

#include <linux/platform_device.h>

enum PLAT_SW_CONFIG {
	CFG_DUMMY_PWR,
	CFG_DUMMY_MMU,
	CFG_DUMMY_MID,

	CFG_NN_PMU_SUPPORT,
	CFG_SW_PREEMPTION_SUPPORT,
	CFG_HW_PREEMPTION_SUPPORT,
	CFG_MICRO_P_SUPPORT,

	SW_CFG_MAX
};

struct mdla_plat_drv {
	int (*init)(struct platform_device *pdev);
	void (*deinit)(struct platform_device *pdev);
	unsigned int sw_cfg;
	unsigned int klog;
	unsigned int timeout_ms;
	unsigned int off_delay_ms;
	unsigned int polling_cmd_ms;
	unsigned int pmu_period_us;
	unsigned int rv_ver;
	int profile_ver;
};

int mdla_v1_0_init(struct platform_device *pdev);
void mdla_v1_0_deinit(struct platform_device *pdev);

int mdla_v1_x_init(struct platform_device *pdev);
void mdla_v1_x_deinit(struct platform_device *pdev);

int mdla_v2_0_init(struct platform_device *pdev);
void mdla_v2_0_deinit(struct platform_device *pdev);

int mdla_rv_init(struct platform_device *pdev);
void mdla_rv_deinit(struct platform_device *pdev);

#endif /* __MDLA_PLAT_INTERNAL_H__ */

