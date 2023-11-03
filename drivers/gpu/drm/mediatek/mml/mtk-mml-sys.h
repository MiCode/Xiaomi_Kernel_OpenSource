/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Ping-Hsun Wu <ping-hsun.wu@mediatek.com>
 */

#ifndef __MTK_MML_SYS_H__
#define __MTK_MML_SYS_H__

#include <linux/platform_device.h>
#include <linux/component.h>

u16 mml_sys_get_reg_ready_sel(struct mml_comp *comp);
struct mml_sys *mml_sys_create(struct platform_device *pdev,
	const struct component_ops *comp_ops);
void mml_sys_destroy(struct platform_device *pdev, struct mml_sys *sys,
	const struct component_ops *comp_ops);
int mml_sys_bind(struct device *dev, struct device *master,
	struct mml_sys *sys, void *data);
void mml_sys_unbind(struct device *dev, struct device *master,
	struct mml_sys *sys, void *data);

/*
 * mml_set_uid - Call sspm to config uid/aid for mml
 *
 * @mml_scmi: status info for scmi
 *
 * Note: this API implement in mtk-mml-scmi.c to avoid conflict definition
 * with display headers.
 */
void mml_set_uid(void **mml_scmi);

/*
 * mml_sys_put_dle_ctx - Put dle context through mmlsys comp
 *
 * @mml:	mml driver private data
 */
void mml_sys_put_dle_ctx(void *mml);

extern const struct of_device_id mtk_mml_of_ids[];

#endif	/* __MTK_MML_SYS_H__ */
