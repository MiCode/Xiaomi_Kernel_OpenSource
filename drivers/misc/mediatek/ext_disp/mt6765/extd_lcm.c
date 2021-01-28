// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/***********************************************/
/*
 * Record the modify of dts, dtsi, configs & dws
 * If we support dual lcm, please modify the dts, dtsi, configs and
 *					dws files as the following content.
 *
 * dts: Add LCM1 GPIO (RESET & DSI_TE).
 *
 * dtsi: Add dsi_te1 node:
 *		dsi_te_1: dsi_te_1 {
 *			compatible = "mediatek, dsi_te_1-eint";
 *			status = "disabled";
 *		};
 *
 * configs:
 *	CONFIG_CUSTOM_KERNEL_LCM=
 *	"nt35595_fhd_dsi_cmd_truly_nt50358_extern
 *		nt35595_fhd_dsi_cmd_truly_nt50358_2th"
 *	CONFIG_MTK_DUAL_DISPLAY_SUPPORT=2
 *
 * dws: Add dsi_te1 EINT.
 */

/***************************************/
/***************************************/
#include "extd_info.h"

#if defined(CONFIG_MTK_DUAL_DISPLAY_SUPPORT) &&	\
	(CONFIG_MTK_DUAL_DISPLAY_SUPPORT == 2)
#include <linux/mm.h>


#include <linux/delay.h>

#include <linux/dma-mapping.h>
#include <linux/kthread.h>
#include <linux/vmalloc.h>

#include <linux/slab.h>
#include <linux/types.h>
#include <linux/atomic.h>
#include <linux/io.h>

#include "m4u.h"

#include "mtkfb_info.h"
#include "mtkfb.h"

#include "mtkfb_fence.h"
#include "display_recorder.h"

#include "disp_session.h"
#include "ddp_mmp.h"
#include "ddp_irq.h"

#include "extd_platform.h"
#include "extd_factory.h"
#include "extd_log.h"
#include "extd_utils.h"
#include "external_display.h"

/* ~~~~~~~~~~~~the static variable~~~~~~ */
static unsigned int ovl_layer_num;

/* ~~~~~~~~~the gloable variable~~~~~~~~~ */
LCM_PARAMS extd_interface_params;

/* ~~~~~~~~~~~~the definition~~~~~~~~~~ */

#define EXTD_LCM_SESSION_ID          (0x20003)
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
int lcm_get_dev_info(int is_sf, void *info)
{
	int ret = 0;

	if (is_sf == SF_GET_INFO) {
		struct disp_session_info *dispif_info =
		    (struct disp_session_info *)info;

		memset((void *)dispif_info, 0,
		       sizeof(struct disp_session_info));

		dispif_info->isOVLDisabled = (ovl_layer_num == 1) ? 1 : 0;
		dispif_info->maxLayerNum = ovl_layer_num;
		if (extd_interface_params.type == LCM_TYPE_DSI) {
			dispif_info->displayFormat =
			    extd_interface_params.dsi.data_format.format;
			dispif_info->displayHeight =
			    extd_interface_params.height;
			dispif_info->displayWidth = extd_interface_params.width;
			dispif_info->displayType = DISP_IF_TYPE_DSI1;
			if (extd_interface_params.dsi.mode == CMD_MODE)
				dispif_info->displayMode = DISP_IF_MODE_COMMAND;
			else
				dispif_info->displayMode = DISP_IF_MODE_VIDEO;
		}

		dispif_info->isHwVsyncAvailable = 1;
		dispif_info->vsyncFPS = 6000;
		dispif_info->physicalHeight = dispif_info->physicalWidth = 0;

		dispif_info->isConnected = 1;
	}

	return ret;
}

void lcm_set_layer_num(int layer_num)
{
	if (layer_num >= 0)
#ifdef FIX_EXTD_TO_OVL_PATH
		ovl_layer_num = FIX_EXTD_TO_OVL_PATH;
#else
		ovl_layer_num = layer_num;
#endif

}

int lcm_ioctl(unsigned int ioctl_cmd, int param1, int param2,
	      unsigned long *params)
{
	/* EXTDINFO("hdmi_ioctl ioctl_cmd:%d\n", ioctl_cmd); */
	int ret = 0;

	switch (ioctl_cmd) {
	case SET_LAYER_NUM_CMD:
		lcm_set_layer_num(param1);
		break;
	default:
		EXTDERR("%s unknown command\n", __func__);
		break;
	}

	return ret;
}

int lcm_post_init(void)
{
	struct disp_lcm_handle *plcm;
	LCM_PARAMS *lcm_param;

	EXTDFUNC();
	memset((void *)&extd_interface_params, 0, sizeof(LCM_PARAMS));

	extd_disp_get_interface((struct disp_lcm_handle **)&plcm);
	if (plcm && plcm->params && plcm->drv) {
		lcm_param = disp_lcm_get_params(plcm);
		if (lcm_param)
			memcpy(&extd_interface_params, lcm_param,
			       sizeof(LCM_PARAMS));
	}

	Extd_DBG_Init();
	EXTDINFO("%s done\n", __func__);
	return 0;
}
#endif

const struct EXTD_DRIVER *EXTD_LCM_Driver(void)
{
	static const struct EXTD_DRIVER extd_driver_lcm = {
#if defined(CONFIG_MTK_DUAL_DISPLAY_SUPPORT) &&	\
			(CONFIG_MTK_DUAL_DISPLAY_SUPPORT == 2)
		.post_init = lcm_post_init,
		.get_dev_info = lcm_get_dev_info,
		.ioctl = lcm_ioctl,
		.power_enable = NULL,
#else
		.init = 0,
#endif
	};

	return &extd_driver_lcm;
}
