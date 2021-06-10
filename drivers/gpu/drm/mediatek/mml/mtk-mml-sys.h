/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Ping-Hsun Wu <ping-hsun.wu@mediatek.com>
 */

#ifndef __MTK_MML_SYS_H__
#define __MTK_MML_SYS_H__

#include <linux/platform_device.h>
#include <linux/component.h>

struct mtk_mml_sys *mml_sys_create(struct platform_device *pdev,
	const struct component_ops *comp_ops);
void mml_sys_destroy(struct platform_device *pdev, struct mtk_mml_sys *sys,
	const struct component_ops *comp_ops);
int mml_sys_bind(struct device *dev, struct device *master,
	struct mtk_mml_sys *sys);
void mml_sys_unbind(struct device *dev, struct device *master,
	struct mtk_mml_sys *sys);

extern const struct of_device_id mtk_mml_of_ids[];

#endif	/* __MTK_MML_SYS_H__ */
