/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Ping-Hsun Wu <ping-hsun.wu@mediatek.com>
 */

#ifndef __MTK_MML_DRIVER_H__
#define __MTK_MML_DRIVER_H__

#include <linux/platform_device.h>
#include <linux/mailbox_client.h>
#include <linux/mailbox/mtk-cmdq-mailbox.h>
#include <linux/mailbox_controller.h>
#include <linux/of.h>

struct mml_comp;
struct mml_dev;
struct mml_drm_ctx;

struct platform_device *mml_get_plat_device(struct platform_device *pdev);

static inline int of_mml_count_comps(const struct device_node *np)
{
	return of_property_count_u32_elems(np, "comp-ids");
}

static inline int of_mml_read_comp_id_index(const struct device_node *np,
	u32 index, u32 *id)
{
	return of_property_read_u32_index(np, "comp-ids", index, id);
}

s32 mml_comp_init(struct platform_device *comp_pdev, struct mml_comp *comp);
s32 mml_subcomp_init(struct platform_device *comp_pdev,
	int subcomponent, struct mml_comp *comp);
s32 mml_register_comp(struct device *master, struct mml_comp *comp);
void mml_unregister_comp(struct device *master, struct mml_comp *comp);

struct mml_drm_ctx *mml_dev_get_drm_ctx(struct mml_dev *mml,
	struct mml_drm_ctx *(*ctx_create)(struct mml_dev *mml));
void mml_dev_put_drm_ctx(struct mml_dev *mml,
	void (*ctx_release)(struct mml_drm_ctx *ctx));

#endif	/* __MTK_MML_DRIVER_H__ */
