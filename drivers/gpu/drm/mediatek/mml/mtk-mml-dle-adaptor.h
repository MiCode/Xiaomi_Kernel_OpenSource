/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Ping-Hsun Wu <ping-hsun.wu@mediatek.com>
 */

#ifndef __MTK_MML_DLE_ADAPTOR_H__
#define __MTK_MML_DLE_ADAPTOR_H__

#include <linux/device.h>
#include <mtk_drm_ddp_comp.h>

#include "mtk-mml.h"

struct mml_dle_ctx;
struct mml_task;
struct mml_comp;

struct mml_dle_param {
	/* [in]set true if direct-link uses dual pipe */
	bool dual;

	/* submit done callback api */
	void (*submit_cb)(const struct mml_task *task, void *cb_param);
};

/*
 * mml_dle_get_context - Get mml dle context to configure mml.
 *
 * dev:		The mml driver device pointer. Component driver must save
 *		the master device on binding.
 * @dl:		Direct-link parameters. See struct.
 *
 * Return:	The dle context pointer to represent mml driver instance.
 *
 */
struct mml_dle_ctx *mml_dle_get_context(struct device *dev,
					struct mml_dle_param *dl);

/*
 * mml_dle_put_context - Release mml dle context and related cached info
 * inside this context.
 *
 * @ctx:	The dle context instance.
 */
void mml_dle_put_context(struct mml_dle_ctx *ctx);

/*
 * mml_dle_config - configure mml job
 *
 * @ctx:	Context of mml dle adaptor. Get by mml_dle_get_context API.
 * @submit:	Frame info which want mml driver to configure.
 * @cb_param:	The parameter used in submit done callback (if registered).
 *
 * Return:	Result of config. In value < 0 case job did not send to mml
 *		driver core.
 */
s32 mml_dle_config(struct mml_dle_ctx *ctx, struct mml_submit *submit,
	void *cb_param);

/*
 * mml_ddp_comp_init - initialize ddp component to drm
 *
 * @dev:	Device of component.
 * @ddp_comp:	The ddp component that will be initialized.
 * @mml_comp:	The mml component of device.
 * @funcs:	The functions of ddp component.
 *
 * Return:	Result of ddp component initialization.
 *		In case value < 0 means initialization fail.
 */
int mml_ddp_comp_init(struct device *dev,
		      struct mtk_ddp_comp *ddp_comp, struct mml_comp *mml_comp,
		      const struct mtk_ddp_comp_funcs *funcs);

/*
 * mml_ddp_comp_register - register ddp component to drm
 *
 * @drm:	Device of drm.
 * @comp:	The component that will be reigstered to drm.
 *
 * Return:	Result of component register.
 *		In case value < 0 means registration fail.
 */
int mml_ddp_comp_register(struct drm_device *drm, struct mtk_ddp_comp *comp);

/*
 * mml_ddp_comp_unregister - unregister ddp component to drm
 *
 * @drm:	Device of drm.
 * @comp:	The component that was reigstered to drm.
 *
 * Return:	None
 */
void mml_ddp_comp_unregister(struct drm_device *drm, struct mtk_ddp_comp *comp);

#endif	/* __MTK_MML_DLE_ADAPTOR_H__ */
