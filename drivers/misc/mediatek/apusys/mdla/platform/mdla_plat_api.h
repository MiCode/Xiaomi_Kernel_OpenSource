/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __MDLA_PLAT_API_H__
#define __MDLA_PLAT_API_H__

#include <linux/of_device.h>

unsigned int mdla_plat_get_core_num(void);
bool mdla_plat_sw_preemption_support(void);

#ifdef CONFIG_OF
const struct of_device_id *mdla_plat_get_device(void);
#else
#define mdla_plat_get_device() NULL
#endif

int mdla_plat_init(struct platform_device *pdev);
void mdla_plat_deinit(struct platform_device *pdev);

#endif /* __MDLA_PLAT_API_H__ */
