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

#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_graph.h>
#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_of.h>

#include <linux/device.h>

#include "mtk_drm_helper.h"
#include "mtk_log.h"

static struct mtk_drm_helper help_info[] = {
	{MTK_DRM_OPT_STAGE, 0, "MTK_DRM_OPT_STAGE"},       /* must enable */
	{MTK_DRM_OPT_USE_CMDQ, 0, "MTK_DRM_OPT_USE_CMDQ"}, /* must enable */
	{MTK_DRM_OPT_USE_M4U, 0, "MTK_DRM_OPT_USE_M4U"},   /* must enable */

	/* low power option start */
	{MTK_DRM_OPT_SODI_SUPPORT, 0, "MTK_DRM_OPT_SODI_SUPPORT"},
	{MTK_DRM_OPT_IDLE_MGR, 0, "MTK_DRM_OPT_IDLE_MGR"},
	{MTK_DRM_OPT_IDLEMGR_SWTCH_DECOUPLE, 0,
	 "MTK_DRM_OPT_IDLEMGR_SWTCH_DECOUPLE"},
	{MTK_DRM_OPT_IDLEMGR_BY_REPAINT, 0, "MTK_DRM_OPT_IDLEMGR_BY_REPAINT"},
	{MTK_DRM_OPT_IDLEMGR_ENTER_ULPS, 0, "MTK_DRM_OPT_IDLEMGR_ENTER_ULPS"},
	{MTK_DRM_OPT_IDLEMGR_KEEP_LP11, 0, "MTK_DRM_OPT_IDLEMGR_KEEP_LP11"},
	{MTK_DRM_OPT_DYNAMIC_RDMA_GOLDEN_SETTING, 0,
	 "MTK_DRM_OPT_DYNAMIC_RDMA_GOLDEN_SETTING"},
	{MTK_DRM_OPT_IDLEMGR_DISABLE_ROUTINE_IRQ, 0,
	 "MTK_DRM_OPT_IDLEMGR_DISABLE_ROUTINE_IRQ"},
	{MTK_DRM_OPT_MET_LOG, 0, "MTK_DRM_OPT_MET_LOG"},
	/* low power option end */

	{MTK_DRM_OPT_USE_PQ, 0, "MTK_DRM_OPT_USE_PQ"},
	{MTK_DRM_OPT_ESD_CHECK_RECOVERY, 1, "MTK_DRM_OPT_ESD_CHECK_RECOVERY"},
	{MTK_DRM_OPT_ESD_CHECK_SWITCH, 0, "MTK_DRM_OPT_ESD_CHECK_SWITCH"},
	{MTK_DRM_OPT_PRESENT_FENCE, 1, "MTK_DRM_OPT_PRESENT_FENCE"},
	{MTK_DRM_OPT_RDMA_UNDERFLOW_AEE, 0, "MTK_DRM_OPT_RDMA_UNDERFLOW_AEE"},
	{MTK_DRM_OPT_DSI_UNDERRUN_AEE, 0, "MTK_DRM_OPT_DSI_UNDERRUN_AEE"},
	{MTK_DRM_OPT_HRT, 1, "MTK_DRM_OPT_HRT"},
	/* HRT_MODE, 0 -> legacy PMQOS, 1 -> HRT BW */
	{MTK_DRM_OPT_HRT_MODE, 0, "MTK_DRM_OPT_HRT_MODE"},
	{MTK_DRM_OPT_DELAYED_TRIGGER, 0, "MTK_DRM_OPT_DELAYED_TRIGGER"},
	{MTK_DRM_OPT_OVL_EXT_LAYER, 0, "MTK_DRM_OPT_OVL_EXT_LAYER"},
	{MTK_DRM_OPT_AOD, 0, "MTK_DRM_OPT_AOD"},
	{MTK_DRM_OPT_RPO, 0, "MTK_DRM_OPT_RPO"},
	{MTK_DRM_OPT_DUAL_PIPE, 0, "MTK_DRM_OPT_DUAL_PIPE"},
	{MTK_DRM_OPT_DC_BY_HRT, 0, "MTK_DRM_OPT_DC_BY_HRT"},
	{MTK_DRM_OPT_OVL_WCG, 1, "MTK_DRM_OPT_OVL_WCG"},
	{MTK_DRM_OPT_OVL_SBCH, 0, "MTK_DRM_OPT_OVL_SBCH"},
	{MTK_DRM_OPT_COMMIT_NO_WAIT_VBLANK, 0,
	 "MTK_DRM_OPT_COMMIT_NO_WAIT_VBLANK"},
	{MTK_DRM_OPT_MET, 0, "MTK_DRM_OPT_MET"},
	{MTK_DRM_OPT_REG_PARSER_RAW_DUMP, 0, "MTK_DRM_OPT_REG_PARSER_RAW_DUMP"},
	{MTK_DRM_OPT_VP_PQ, 0, "MTK_DRM_OPT_VP_PQ"},
	{MTK_DRM_OPT_GAME_PQ, 0, "MTK_DRM_OPT_GAME_PQ"},
	{MTK_DRM_OPT_MMPATH, 0, "MTK_DRM_OPT_MMPATH"},
	{MTK_DRM_OPT_HBM, 0, "MTK_DRM_OPT_HBM"},
	{MTK_DRM_OPT_LAYER_REC, 0, "MTK_DRM_OPT_LAYER_REC"},
	{MTK_DRM_OPT_CLEAR_LAYER, 0, "MTK_DRM_OPT_CLEAR_LAYER"},
	{MTK_DRM_OPT_VDS_PATH_SWITCH, 0, "MTK_DRM_OPT_VDS_PATH_SWITCH"},
};

static const char *mtk_drm_helper_opt_spy(struct mtk_drm_helper *helper_opt,
					  enum MTK_DRM_HELPER_OPT option)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(help_info); i++) {
		if (helper_opt[i].opt == option)
			return helper_opt[i].desc;
	}

	return "unknown option!!";
}

enum MTK_DRM_HELPER_OPT
mtk_drm_helper_name_to_opt(struct mtk_drm_helper *helper_opt, const char *name)
{
	int i;

	for (i = 0; i < MTK_DRM_OPT_NUM; i++) {
		const char *opt_name = mtk_drm_helper_opt_spy(helper_opt, i);

		if (strcmp(name, opt_name) == 0)
			return i;
	}
	DDPINFO("%s: unknown name: %s\n", __func__, name);
	return MTK_DRM_OPT_NUM;
}

int mtk_drm_helper_set_opt(struct mtk_drm_helper *helper_opt,
			   enum MTK_DRM_HELPER_OPT option, int value)
{
	unsigned int i;

	if (option >= MTK_DRM_OPT_NUM) {
		DDPINFO("wrong option: %d\n", option);
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(help_info); i++) {
		if (helper_opt[i].opt == option && helper_opt[i].val != value) {
			DDPMSG("Set Option %d(%s) from (%d) to (%d)\n", option,
			       mtk_drm_helper_opt_spy(helper_opt, option),
			       mtk_drm_helper_get_opt(helper_opt, option),
			       value);

			helper_opt[i].val = value;
		}
	}

	return 0;
}

int mtk_drm_helper_set_opt_by_name(struct mtk_drm_helper *helper_opt,
				   const char *name, int value)
{
	enum MTK_DRM_HELPER_OPT opt;

	opt = mtk_drm_helper_name_to_opt(helper_opt, name);
	if (opt >= MTK_DRM_OPT_NUM)
		return -1;

	return mtk_drm_helper_set_opt(helper_opt, opt, value);
}

int mtk_drm_helper_get_opt(struct mtk_drm_helper *helper_opt,
			   enum MTK_DRM_HELPER_OPT option)
{
	int ret = 0;
	int i;

	if (option >= MTK_DRM_OPT_NUM) {
		DDPINFO("%s: option invalid %d\n", __func__, option);
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(help_info); i++) {
		if (helper_opt[i].opt == option)
			return helper_opt[i].val;
	}

	return ret;
}

void mtk_drm_helper_init(struct device *dev, struct mtk_drm_helper **helper_opt)
{
	int i, value, index, ret;
	struct mtk_drm_helper *tmp_opt;

	tmp_opt = kmalloc(sizeof(help_info), GFP_KERNEL);
	if (!tmp_opt) {
		DDPPR_ERR("helper info creation failed\n");
		return;
	}

	memcpy(tmp_opt, help_info, sizeof(help_info));
	for (i = 0; i < MTK_DRM_OPT_NUM; i++) {
		index = of_property_match_string(dev->of_node, "helper-name",
						 help_info[i].desc);
		if (index < 0)
			value = 0;
		else {
			ret = of_property_read_u32_index(
				dev->of_node, "helper-value", index, &value);
			if (ret < 0)
				value = 0;
		}
		tmp_opt[i].val = value;
		DDPINFO("%s %d\n", tmp_opt[i].desc, tmp_opt[i].val);
	}
	*helper_opt = tmp_opt;
}

int mtk_drm_helper_get_opt_list(struct mtk_drm_helper *helper_opt,
				char *stringbuf, int buf_len)
{
	int len = 0;
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(help_info); i++) {
		if (stringbuf != NULL && buf_len > 0)
			len += scnprintf(stringbuf + len, buf_len - len,
					 "Option: [%d][%s] Value: [%d]\n", i,
					 mtk_drm_helper_opt_spy(helper_opt, i),
					 mtk_drm_helper_get_opt(helper_opt, i));
	}

	return len;
}
