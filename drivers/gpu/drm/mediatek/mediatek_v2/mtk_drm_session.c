// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <drm/drm_gem.h>
#include <linux/dma-buf.h>
#include <drm/mediatek_drm.h>

#include "mtk_drm_drv.h"
#include "mtk_drm_session.h"
#include "mtk_drm_mmp.h"

static DEFINE_MUTEX(disp_session_lock);

int mtk_drm_session_create(struct drm_device *dev,
			   struct drm_mtk_session *config)
{
	int ret = 0;
	int is_session_inited = 0;
	struct mtk_drm_private *private = dev->dev_private;
	unsigned int session =
		MAKE_MTK_SESSION(config->type, config->device_id);
	int i, idx = -1;

	if (config->type < MTK_SESSION_PRIMARY ||
			config->type > MTK_SESSION_SP) {
		DDPPR_ERR("%s create session type abnormal: %u,\n",
			__func__, config->type);
		return -EINVAL;
	}
	/* 1.To check if this session exists already */
	mutex_lock(&disp_session_lock);
	for (i = 0; i < MAX_SESSION_COUNT; i++) {
		if (private->session_id[i] == session) {
			is_session_inited = 1;
			idx = i;
			DDPPR_ERR("[DRM] create session is exited:0x%x\n",
				  session);
			break;
		}
	}

	if (is_session_inited == 1) {
		config->session_id = session;
		goto done;
	}

	if (idx == -1)
		idx = config->type - 1;

	/* 1.To check if support this session (mode,type,dev) */
	/* 2. Create this session */
	if (idx != -1) {
		config->session_id = session;
	      private
		->session_id[idx] = session;
	      private
		->num_sessions = idx + 1;
		DDPINFO("[DRM] New session:0x%x, idx:%d\n", session, idx);
	} else {
		DDPPR_ERR("[DRM] Invalid session creation request\n");
		ret = -1;
	}
done:
	mutex_unlock(&disp_session_lock);

	if (mtk_drm_helper_get_opt(private->helper_opt,
		MTK_DRM_OPT_VDS_PATH_SWITCH) &&
		(MTK_SESSION_TYPE(session) == MTK_SESSION_MEMORY)) {
		enum MTK_DRM_HELPER_OPT helper_opt;

		private->need_vds_path_switch = 1;
		private->vds_path_switch_dirty = 1;
		private->vds_path_switch_done = 0;
		private->vds_path_enable = 0;

		DDPMSG("Switch vds: crtc2 vds session create\n");
		/* Close RPO */
		mtk_drm_helper_set_opt_by_name(private->helper_opt,
			"MTK_DRM_OPT_RPO", 0);
		helper_opt =
			mtk_drm_helper_name_to_opt(private->helper_opt,
				"MTK_DRM_OPT_RPO");
		mtk_update_layering_opt_by_disp_opt(helper_opt, 0);
		mtk_set_layering_opt(LYE_OPT_RPO, 0);
	}

	DDPINFO("[DRM] new session done\n");
	return ret;
}

int mtk_session_get_mode(struct drm_device *dev, struct drm_crtc *crtc)
{
	int crtc_idx = drm_crtc_index(crtc);
	struct mtk_drm_private *private = dev->dev_private;
	int session_mode = private->session_mode;
	const struct mtk_session_mode_tb *mode_tb = private->data->mode_tb;

	if (!mode_tb[session_mode].en)
		return -EINVAL;
	return mode_tb[session_mode].ddp_mode[crtc_idx];
}

int mtk_session_set_mode(struct drm_device *dev, unsigned int session_mode)
{
	int i;
	struct mtk_drm_private *private = dev->dev_private;
	const struct mtk_session_mode_tb *mode_tb = private->data->mode_tb;
	unsigned int session_id;

	mutex_lock(&private->commit.lock);
	if (session_mode >= MTK_DRM_SESSION_NUM) {
		DDPPR_ERR("%s Invalid session mode:%d\n",
			  __func__, session_mode);
		goto error;
	}

	if (!mode_tb[session_mode].en) {
		DDPPR_ERR("%s Invalid mode_tb[%d].en = %d\n",
			  __func__, session_mode, mode_tb[session_mode].en);
		goto error;
	}

	if (session_mode == private->session_mode)
		goto success;

	DRM_MMP_EVENT_START(set_mode, private->session_mode,
			session_mode);

	DDPMSG("%s from %u to %u\n", __func__,
		private->session_mode, session_mode);

	if (of_property_read_bool(private->mmsys_dev->of_node,
				"enable_output_int_switch")) {
		DDPMSG("%s ignore set mode because of output switch\n", __func__);
		private->session_mode = session_mode;
		goto success;
	}

	if (mtk_drm_helper_get_opt(private->helper_opt,
		MTK_DRM_OPT_VDS_PATH_SWITCH) &&
		(private->session_mode == MTK_DRM_SESSION_DOUBLE_DL) &&
		(session_mode == MTK_DRM_SESSION_DL)) {
		enum MTK_DRM_HELPER_OPT helper_opt;

		private->need_vds_path_switch = 0;
		private->vds_path_switch_done = 0;
		private->vds_path_enable = 0;

		/* Open RPO */
		mtk_drm_helper_set_opt_by_name(private->helper_opt,
			"MTK_DRM_OPT_RPO", 1);
		helper_opt =
			mtk_drm_helper_name_to_opt(private->helper_opt,
				"MTK_DRM_OPT_RPO");
		mtk_update_layering_opt_by_disp_opt(helper_opt, 1);
		mtk_set_layering_opt(LYE_OPT_RPO, 1);

		/* OVL0_2l switch back to main path */
		DDPMSG("Switch vds: crtc2 vds set ddp mode to DL\n");
		mtk_need_vds_path_switch(private->crtc[0]);
	}

	/* has memory session. need disconnect wdma from cwb*/
	session_id = (private->crtc[2]) ? mtk_get_session_id(private->crtc[2]) : -1;
	if (session_id != -1)
		mtk_crtc_cwb_path_disconnect(private->crtc[0]);

	/* For releasing HW resource purpose, the ddp mode should
	 * switching reversely in some situation.
	 * CRTC2 -> CRTC1 ->CRTC0
	 */
	if (session_mode == MTK_DRM_SESSION_DC_MIRROR ||
	    private->session_mode == MTK_DRM_SESSION_TRIPLE_DL) {
		DRM_MMP_MARK(set_mode, 1, 0);
		for (i = MAX_CRTC - 1; i >= 0; i--) {
			if (private->crtc[i])
				mtk_crtc_path_switch(
					private->crtc[i],
					mode_tb[session_mode].ddp_mode[i], 1);
		}
	} else {
		DRM_MMP_MARK(set_mode, 1, 1);
		for (i = 0; i < MAX_CRTC; i++) {
			if (private->crtc[i])
				mtk_crtc_path_switch(
					private->crtc[i],
					mode_tb[session_mode].ddp_mode[i], 1);
		}
	}

	/* has no memory session. need disconnect wdma from cwb*/
	if (session_id == -1) {
		private->need_cwb_path_disconnect = false;
		private->cwb_is_preempted = false;
	}

	private->session_mode = session_mode;
	DRM_MMP_EVENT_END(set_mode, private->session_mode,
			session_mode);

success:
	mutex_unlock(&private->commit.lock);
	return 0;

error:
	mutex_unlock(&private->commit.lock);
	return -EINVAL;
}

int mtk_drm_session_destroy(struct drm_device *dev,
			    struct drm_mtk_session *config)
{
	int ret = -1;
	unsigned int session = config->session_id;
	struct mtk_drm_private *private = dev->dev_private;
	int i;

	DDPINFO("disp_destroy_session, 0x%x", config->session_id);

	/* 1.To check if this session exists already, and remove it */
	mutex_lock(&disp_session_lock);
	for (i = 0; i < MAX_SESSION_COUNT; i++) {
		if (private->session_id[i] == session) {
			private->session_id[i] = 0;
			ret = 0;
			break;
		}
	}

	mutex_unlock(&disp_session_lock);

	/* 2. Destroy this session */
	if (ret == 0)
		DDPINFO("Destroy session(0x%x)\n", session);
	else
		DDPPR_ERR("session(0x%x) does not exists\n", session);

	return ret;
}

int mtk_get_session_id(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_private *private;
	int session_id = -1, id, i;

	id = drm_crtc_index(crtc);
	private = mtk_crtc->base.dev->dev_private;
	for (i = 0; i < MAX_SESSION_COUNT; i++) {
		if ((id + 1) == MTK_SESSION_TYPE(private->session_id[i])) {
			session_id = private->session_id[i];
			break;
		}
	}
	return session_id;
}

int mtk_drm_session_create_ioctl(struct drm_device *dev, void *data,
				 struct drm_file *file_priv)
{
	int ret = 0;
	struct drm_mtk_session *config = data;

	if (mtk_drm_session_create(dev, config) != 0)
		ret = -EFAULT;

	return ret;
}

int mtk_drm_session_destroy_ioctl(struct drm_device *dev, void *data,
				  struct drm_file *file_priv)
{
	int ret = 0;
	struct drm_mtk_session *config = data;

	if (mtk_drm_session_destroy(dev, config) != 0)
		ret = -EFAULT;

	return ret;
}
