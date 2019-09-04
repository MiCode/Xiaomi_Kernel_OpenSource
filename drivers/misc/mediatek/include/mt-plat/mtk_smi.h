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

enum {SMI_IPI_INIT, SMI_IPI_ENABLE, NR_SMI_IPI,};

struct smi_ipi_data_s {
	u32 cmd;
	union {
		struct {
			u32 phys;
			u32 size;
		} ctrl;
		struct {
			u32 enable;
		} logger;
	} u;
};

/* For MMDVFS */
struct MTK_MMDVFS_QOS_CMD {
	unsigned int type;
	unsigned int max_cam_bw;
	unsigned int ret;
};

#define MTK_MMDVFS_QOS_CMD_TYPE_SET		0

#define MTK_IOC_MMDVFS_QOS_CMD \
	_IOW('O', 89, struct MTK_MMDVFS_QOS_CMD)

#endif
