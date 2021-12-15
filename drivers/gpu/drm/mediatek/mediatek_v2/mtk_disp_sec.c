// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/soc/mediatek/mtk-cmdq-ext.h>

#include <drm/drm_crtc.h>
#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_drv.h"
#include "mtk_log.h"
#include "mtk_dump.h"


#include "cmdq-sec.h"
#include "cmdq-sec-iwc-common.h"
#include "cmdq-sec-mailbox.h"


bool mtk_crtc_with_cmdq_secure(struct drm_crtc *crtc)
{
#ifndef DRM_CMDQ_DISABLE
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	if (mtk_crtc->gce_obj.client[CLIENT_SEC_CFG])
		return true;
#endif
	return false;
}

static void mtk_crtc_secure_cb(struct cmdq_cb_data data)
{
	struct mtk_cmdq_cb_data *cb_data = data.data;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(cb_data->crtc);

	if (!mtk_crtc->sec_cmdq_handle) {
		DDPPR_ERR("%s: secure sec_cmdq_handle already stopped\n", __func__);
		return;
	}
	DDPINFO("%s\n", __func__);

	cmdq_pkt_destroy(mtk_crtc->sec_cmdq_handle);
	mtk_crtc->sec_cmdq_handle = NULL;
	kfree(cb_data);
}

int mtk_crtc_cmdq_secure_init(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_cmdq_cb_data *cb_data;
	int crtc_idx = drm_crtc_index(crtc);

	if (!mtk_crtc_with_cmdq_secure(crtc)) {
		DDPINFO("%s:%d, gce_obj.client[CLIENT_SEC_CFG] is NULL\n",
			__func__, __LINE__);
		return false;
	}

	cb_data = kmalloc(sizeof(*cb_data), GFP_KERNEL);
	if (!cb_data) {
		DDPPR_ERR("%s:%d, cb data creation failed\n",
			__func__, __LINE__);
		return false;
	}

	DDPINFO("%s: crtc_idx %d regs:0x%p\n", __func__, crtc_idx,
		mtk_crtc->gce_obj.client[CLIENT_SEC_CFG]);

	cb_data->crtc = crtc;
	mtk_crtc->sec_cmdq_handle =
		cmdq_pkt_create(mtk_crtc->gce_obj.client[CLIENT_SEC_CFG]);

	cmdq_sec_pkt_set_data(mtk_crtc->sec_cmdq_handle, 0, 0,
		CMDQ_SEC_DEBUG, CMDQ_METAEX_TZMP);
	cmdq_sec_pkt_set_mtee(mtk_crtc->sec_cmdq_handle, true);
	cmdq_pkt_finalize_loop(mtk_crtc->sec_cmdq_handle);
	if (cmdq_pkt_flush_threaded(mtk_crtc->sec_cmdq_handle,
		mtk_crtc_secure_cb, cb_data) < 0) {
		DDPPR_ERR("Failed to flush user_cmd\n");
		return false;
	}
	return true;
}

int mtk_crtc_cmdq_secure_end(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	if (!mtk_crtc->sec_cmdq_handle) {
		DDPPR_ERR("%s cmdq_handle is NULL\n", __func__);
		return false;
	}

	if (!mtk_crtc_with_cmdq_secure(crtc)) {
		DDPINFO("%s:%d, gce_obj.client[CLIENT_SEC_CFG] is NULL\n",
			__func__, __LINE__);
		return false;
	}

	cmdq_sec_mbox_stop(mtk_crtc->gce_obj.client[CLIENT_SEC_CFG]);
	return true;
}

int mtk_drm_enable_secure_domain_enable(struct drm_crtc *crtc,
			struct cmdq_pkt *handle, resource_size_t dummy_larb)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	DDPINFO("%s\n", __func__);

	cmdq_pkt_write(handle, NULL, dummy_larb, BIT(1), BIT(1));
	cmdq_pkt_set_event(handle, mtk_crtc->tzmp_disp_sec_wait);
	cmdq_pkt_wfe(handle, mtk_crtc->tzmp_disp_sec_set);

	return true;
}

int mtk_drm_enable_secure_domain_disable(struct drm_crtc *crtc,
			struct cmdq_pkt *handle, resource_size_t dummy_larb)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	DDPINFO("%s\n", __func__);

	cmdq_pkt_write(handle, NULL, dummy_larb, 0, BIT(1));
	cmdq_pkt_set_event(handle, mtk_crtc->tzmp_disp_sec_wait);
	cmdq_pkt_wfe(handle, mtk_crtc->tzmp_disp_sec_set);

	return true;
}

static int mtk_drm_disp_sec_cb_event(int value, struct drm_crtc *crtc,
			struct cmdq_pkt *handle, resource_size_t dummy_larb)
{
	DDPINFO("%s\n", __func__);
	switch (value) {
	case DISP_SEC_START:
		return mtk_crtc_cmdq_secure_init(crtc);
	case DISP_SEC_STOP:
		return mtk_crtc_cmdq_secure_end(crtc);
	case DISP_SEC_ENABLE:
		return mtk_drm_enable_secure_domain_enable(crtc, handle, dummy_larb);
	case DISP_SEC_DISABLE:
		return mtk_drm_enable_secure_domain_disable(crtc, handle, dummy_larb);
	default:
		return false;
	}
}


static int __init mtk_disp_sec_init(void)
{
	void **ret;

	DDPMSG("%s+\n", __func__);
	ret = mtk_drm_disp_sec_cb_init();
	*ret = (void *) mtk_drm_disp_sec_cb_event;
	DDPMSG("%s-\n", __func__);
	return 0;
}

static void __exit mtk_disp_sec_exit(void)
{
	DDPMSG("%s\n", __func__);
}

module_init(mtk_disp_sec_init);
module_exit(mtk_disp_sec_exit);

MODULE_AUTHOR("Aaron Chung <Aaron.Chung@mediatek.com>");
MODULE_DESCRIPTION("MTK DRM secure Display");
MODULE_LICENSE("GPL v2");

