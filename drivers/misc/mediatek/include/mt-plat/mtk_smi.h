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

#ifndef __MTK_SMI_H__
#define __MTK_SMI_H__

#define MTK_SMI_MAJOR_NUMBER 190

enum MTK_SMI_BWC_SCEN {
	SMI_BWC_SCEN_NORMAL,
	SMI_BWC_SCEN_UI_IDLE,
	SMI_BWC_SCEN_VPMJC,
	SMI_BWC_SCEN_FORCE_MMDVFS,
	SMI_BWC_SCEN_HDMI,
	SMI_BWC_SCEN_HDMI4K,
	SMI_BWC_SCEN_WFD,
	SMI_BWC_SCEN_SWDEC_VP,
	SMI_BWC_SCEN_VP,
	SMI_BWC_SCEN_VP_HIGH_FPS,
	SMI_BWC_SCEN_VP_HIGH_RESOLUTION,
	SMI_BWC_SCEN_VENC,
	SMI_BWC_SCEN_VR,
	SMI_BWC_SCEN_VR_SLOW,
	SMI_BWC_SCEN_VSS,
	SMI_BWC_SCEN_CAM_PV,
	SMI_BWC_SCEN_CAM_CP,
	SMI_BWC_SCEN_ICFP,
	SMI_BWC_SCEN_MM_GPU,
	SMI_BWC_SCEN_CNT
};

static char *MTK_SMI_BWC_SCEN_NAME[SMI_BWC_SCEN_CNT] = {
	"SMI_BWC_SCEN_NORMAL", "SMI_BWC_SCEN_UI_IDLE", "SMI_BWC_SCEN_VPMJC",
	"SMI_BWC_SCEN_FORCE_MMDVFS", "SMI_BWC_SCEN_HDMI", "SMI_BWC_SCEN_HDMI4K",
	"SMI_BWC_SCEN_WFD", "SMI_BWC_SCEN_SWDEC_VP",
	/* libvcodec */
	"SMI_BWC_SCEN_VP", "SMI_BWC_SCEN_VP_HIGH_FPS",
	"SMI_BWC_SCEN_VP_HIGH_RESOLUTION", "SMI_BWC_SCEN_VENC",
	/* mtkcam */
	"SMI_BWC_SCEN_VR", "SMI_BWC_SCEN_VR_SLOW", "SMI_BWC_SCEN_VSS",
	"SMI_BWC_SCEN_CAM_PV", "SMI_BWC_SCEN_CAM_CP", "SMI_BWC_SCEN_ICFP",
	"SMI_BWC_SCEN_MM_GPU",
};

static inline char *smi_bwc_scen_name_get(const enum MTK_SMI_BWC_SCEN scen)
{
	if (scen < SMI_BWC_SCEN_CNT)
		return MTK_SMI_BWC_SCEN_NAME[scen];
	else
		return "SMI_BWC_SCEN_UNKNOWN";
}

struct MTK_SMI_BWC_CONF {
	u32 scen;
	u32 b_on;
};

#define MTK_IOC_SMI_BWC_CONF	_IOW('O', 24, struct MTK_SMI_BWC_CONF)
#endif
