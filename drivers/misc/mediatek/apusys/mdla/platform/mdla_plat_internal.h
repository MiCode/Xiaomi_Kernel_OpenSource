/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __MDLA_PLAT_INTERNAL_H__
#define __MDLA_PLAT_INTERNAL_H__

int mdla_mt6779_init(struct platform_device *pdev);
void mdla_mt6779_deinit(struct platform_device *pdev);

int mdla_mt6873_init(struct platform_device *pdev);
void mdla_mt6873_deinit(struct platform_device *pdev);

int mdla_mt6885_init(struct platform_device *pdev);
void mdla_mt6885_deinit(struct platform_device *pdev);

int mdla_mt8195_init(struct platform_device *pdev);
void mdla_mt8195_deinit(struct platform_device *pdev);

#endif /* __MDLA_PLAT_INTERNAL_H__ */

