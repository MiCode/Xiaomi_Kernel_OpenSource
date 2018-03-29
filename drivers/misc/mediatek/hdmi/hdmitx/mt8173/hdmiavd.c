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

#ifdef CONFIG_MTK_INTERNAL_HDMI_SUPPORT

#include "hdmiavd.h"
#include "hdmictrl.h"
#include "hdmihdcp.h"

void av_hdmiset(enum AV_D_HDMI_DRV_SET_TYPE_T e_set_type, const void *pv_set_info,
		unsigned char z_set_info_len)
{
	HDMI_AV_INFO_T *prAvInf;

	prAvInf = (HDMI_AV_INFO_T *) pv_set_info;

	switch (e_set_type) {

	case HDMI_SET_TURN_OFF_TMDS:
		vTmdsOnOffAndResetHdcp(prAvInf->fgHdmiTmdsEnable);
		break;

	case HDMI_SET_VPLL:
		vChangeVpll(prAvInf->e_resolution, prAvInf->e_deep_color_bit);
		break;


	case HDMI_SET_VIDEO_RES_CHG:
		vChgHDMIVideoResolution(prAvInf->e_resolution, prAvInf->e_video_color_space,
					prAvInf->e_hdmi_fs, prAvInf->e_deep_color_bit);
		break;

	case HDMI_SET_AUDIO_CHG_SETTING:
		vChgHDMIAudioOutput(prAvInf->e_hdmi_fs, prAvInf->e_resolution,
				    prAvInf->e_deep_color_bit);
		break;

	case HDMI_SET_HDCP_INITIAL_AUTH:
		vHDCPInitAuth();
		break;

	case HDMI_SET_VIDEO_COLOR_SPACE:

		break;

	case HDMI_SET_SOFT_NCTS:
		vChgtoSoftNCTS(prAvInf->e_resolution, prAvInf->u1audiosoft, prAvInf->e_hdmi_fs,
			       prAvInf->e_deep_color_bit);
		break;

	case HDMI_SET_HDCP_OFF:
		vDisableHDCP(prAvInf->u1hdcponoff);
		break;

	case HDMI2_SET_TURN_OFF_TMDS:
		vTmds2OnOffAndResetHdcp(prAvInf->fgHdmiTmdsEnable);
		break;

	case HDMI2_SET_VPLL:
		vChangeV2pll(prAvInf->e_resolution, prAvInf->e_deep_color_bit);
		break;


	case HDMI2_SET_VIDEO_RES_CHG:
		vChgHDMI2VideoResolution(prAvInf->e_resolution, prAvInf->e_video_color_space,
					 prAvInf->e_hdmi_fs, prAvInf->e_deep_color_bit);
		break;

	case HDMI2_SET_AUDIO_CHG_SETTING:
		vChgHDMI2AudioOutput(prAvInf->e_hdmi_fs, prAvInf->e_resolution,
				     prAvInf->e_deep_color_bit);
		break;

	case HDMI2_SET_HDCP_INITIAL_AUTH:
		vHDCP2InitAuth();
		break;

	case HDMI2_SET_VIDEO_COLOR_SPACE:

		break;

	case HDMI2_SET_SOFT_NCTS:
		vChgtoSoftNCTS2(prAvInf->e_resolution, prAvInf->u1audiosoft, prAvInf->e_hdmi_fs,
				prAvInf->e_deep_color_bit);
		break;

	case HDMI2_SET_HDCP_OFF:
		vDisableHDCP2(prAvInf->u1hdcponoff);
		break;

	default:
		break;
	}

}
#endif
