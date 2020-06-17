/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __MDLA_PLAT_INTERNAL_H__
#define __MDLA_PLAT_INTERNAL_H__

int mdla_v1_0_init(struct platform_device *pdev);
void mdla_v1_0_deinit(struct platform_device *pdev);

int mdla_v1_7_init(struct platform_device *pdev);
void mdla_v1_7_deinit(struct platform_device *pdev);

int mdla_v1_5_init(struct platform_device *pdev);
void mdla_v1_5_deinit(struct platform_device *pdev);

int mdla_v2_0_init(struct platform_device *pdev);
void mdla_v2_0_deinit(struct platform_device *pdev);

#endif /* __MDLA_PLAT_INTERNAL_H__ */

