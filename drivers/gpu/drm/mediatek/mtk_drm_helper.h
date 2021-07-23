/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _MTK_DRM_HELPER_H_
#define _MTK_DRM_HELPER_H_

enum MTK_DRM_HELPER_OPT {
	MTK_DRM_OPT_STAGE = 0,
	MTK_DRM_OPT_USE_CMDQ,
	MTK_DRM_OPT_USE_M4U,
	/* Begin: lowpower option*/
	MTK_DRM_OPT_SODI_SUPPORT,
	MTK_DRM_OPT_IDLE_MGR,
	MTK_DRM_OPT_IDLEMGR_SWTCH_DECOUPLE,
	MTK_DRM_OPT_IDLEMGR_BY_REPAINT,
	MTK_DRM_OPT_IDLEMGR_ENTER_ULPS,
	MTK_DRM_OPT_IDLEMGR_KEEP_LP11,
	MTK_DRM_OPT_DYNAMIC_RDMA_GOLDEN_SETTING,
	MTK_DRM_OPT_IDLEMGR_DISABLE_ROUTINE_IRQ,
	MTK_DRM_OPT_MET_LOG, /* for met */
	/* End: lowpower option */
	MTK_DRM_OPT_USE_PQ,
	MTK_DRM_OPT_ESD_CHECK_RECOVERY,
	MTK_DRM_OPT_ESD_CHECK_SWITCH,
	MTK_DRM_OPT_PRESENT_FENCE,
	MTK_DRM_OPT_RDMA_UNDERFLOW_AEE,
	MTK_DRM_OPT_DSI_UNDERRUN_AEE,
	MTK_DRM_OPT_HRT,
	MTK_DRM_OPT_HRT_MODE,
	MTK_DRM_OPT_DELAYED_TRIGGER,

	MTK_DRM_OPT_OVL_EXT_LAYER,
	MTK_DRM_OPT_AOD,
	MTK_DRM_OPT_RPO,
	MTK_DRM_OPT_DUAL_PIPE,
	MTK_DRM_OPT_DC_BY_HRT,
	MTK_DRM_OPT_OVL_WCG,
	/* OVL SBCH */
	MTK_DRM_OPT_OVL_SBCH,
	MTK_DRM_OPT_COMMIT_NO_WAIT_VBLANK,
	MTK_DRM_OPT_MET,
	MTK_DRM_OPT_REG_PARSER_RAW_DUMP,
	MTK_DRM_OPT_VP_PQ,
	MTK_DRM_OPT_GAME_PQ,
	MTK_DRM_OPT_MMPATH,
	MTK_DRM_OPT_HBM,
	MTK_DRM_OPT_LAYER_REC,
	MTK_DRM_OPT_CLEAR_LAYER,
	MTK_DRM_OPT_VDS_PATH_SWITCH,
	MTK_DRM_OPT_NUM
};

struct mtk_drm_helper {
	enum MTK_DRM_HELPER_OPT opt;
	unsigned int val;
	const char *desc;
};

enum MTK_DRM_HELPER_STAGE {
	MTK_DRM_HELPER_STAGE_EARLY_PORTING,
	MTK_DRM_HELPER_STAGE_BRING_UP,
	MTK_DRM_HELPER_STAGE_NORMAL
};

int mtk_drm_helper_get_opt(struct mtk_drm_helper *helper_opt,
			   enum MTK_DRM_HELPER_OPT option);
int mtk_drm_helper_set_opt(struct mtk_drm_helper *helper_opt,
			   enum MTK_DRM_HELPER_OPT option, int value);
int mtk_drm_helper_set_opt_by_name(struct mtk_drm_helper *helper_opt,
				   const char *name, int value);
int mtk_drm_helper_get_opt_list(struct mtk_drm_helper *helper_opt,
				char *stringbuf, int buf_len);
void mtk_drm_helper_init(struct device *dev,
			 struct mtk_drm_helper **helper_opt);

enum DISP_HELPER_STAGE disp_helper_get_stage(void);
const char *disp_helper_stage_spy(void);
enum MTK_DRM_HELPER_OPT
mtk_drm_helper_name_to_opt(struct mtk_drm_helper *helper_opt, const char *name);

#endif /* _MTK_DRM_HELPER_H_ */
