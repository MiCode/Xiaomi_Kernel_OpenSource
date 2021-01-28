/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
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
	SMI_BWC_SCEN_VENC,
	SMI_BWC_SCEN_SWDEC_VP,
	SMI_BWC_SCEN_VP,
	SMI_BWC_SCEN_VP_HIGH_FPS,
	SMI_BWC_SCEN_VP_HIGH_RESOLUTION,
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

/* MMDVFS */
enum mmdvfs_voltage_enum {
	MMDVFS_VOLTAGE_DEFAULT,
	MMDVFS_VOLTAGE_0 = MMDVFS_VOLTAGE_DEFAULT,
	MMDVFS_VOLTAGE_LOW = MMDVFS_VOLTAGE_0,
	MMDVFS_VOLTAGE_1,
	MMDVFS_VOLTAGE_HIGH = MMDVFS_VOLTAGE_1,
	MMDVFS_VOLTAGE_DEFAULT_STEP,
	MMDVFS_VOLTAGE_LOW_LOW,
	MMDVFS_VOLTAGE_COUNT
};

#define MMDVFS_CAMERA_MODE_FLAG_DEFAULT	1
#define MMDVFS_CAMERA_MODE_FLAG_PIP (1 << 1)
#define MMDVFS_CAMERA_MODE_FLAG_VFB (1 << 2)
#define MMDVFS_CAMERA_MODE_FLAG_EIS_2_0 (1 << 3)
#define MMDVFS_CAMERA_MODE_FLAG_IVHDR (1 << 4)
#define MMDVFS_CAMERA_MODE_FLAG_STEREO  (1 << 5)
#define MMDVFS_CAMERA_MODE_FLAG_MVHDR  (1 << 6)
#define MMDVFS_CAMERA_MODE_FLAG_ZVHDR  (1 << 7)
#define MMDVFS_CAMERA_MODE_FLAG_DUAL_ZOOM  (1 << 8)


struct MTK_MMDVFS_CMD {
	unsigned int type;
	enum MTK_SMI_BWC_SCEN scen;

	unsigned int sensor_size;
	unsigned int sensor_fps;
	unsigned int camera_mode;
	unsigned int boost_disable;
	unsigned int ddr_type;
	unsigned int step;
	unsigned int venc_size;
	unsigned int preview_size;

	unsigned int ret;
};

#define MTK_MMDVFS_CMD_TYPE_SET		0
#define MTK_MMDVFS_CMD_TYPE_QUERY	1
#define MTK_MMDVFS_CMD_TYPE_GET	2
#define MTK_MMDVFS_CMD_TYPE_CONFIG	3
#define MTK_MMDVFS_CMD_TYPE_STEP_SET 4
#define MTK_MMDVFS_CMD_TYPE_VPU_STEP_SET 10
#define MTK_MMDVFS_CMD_TYPE_VPU_STEP_GET 11


enum MTK_SMI_BWC_INFO_ID {
	SMI_BWC_INFO_CON_PROFILE = 0,
	SMI_BWC_INFO_SENSOR_SIZE,
	SMI_BWC_INFO_VIDEO_RECORD_SIZE,
	SMI_BWC_INFO_DISP_SIZE,
	SMI_BWC_INFO_TV_OUT_SIZE,
	SMI_BWC_INFO_FPS,
	SMI_BWC_INFO_VIDEO_ENCODE_CODEC,
	SMI_BWC_INFO_VIDEO_DECODE_CODEC,
	SMI_BWC_INFO_HW_OVL_LIMIT,
	SMI_BWC_INFO_CNT
};

struct MTK_SMI_BWC_INFO_SET {
	int       property;
	int       value1;
	int       value2;
};


struct MTK_SMI_BWC_MM_INFO {
	unsigned int flag;	/* Reserved */
	int concurrent_profile;
	int sensor_size[2];
	int video_record_size[2];
	int display_size[2];
	int tv_out_size[2];
	int fps;
	int video_encode_codec;
	int video_decode_codec;
	int hw_ovl_limit;
};
#define MTK_IOC_SMI_BWC_INFO_SET _IOWR('O', 28, struct MTK_SMI_BWC_INFO_SET)
#define MTK_IOC_SMI_BWC_INFO_GET _IOWR('O', 29, struct MTK_SMI_BWC_MM_INFO)
#define MTK_IOC_MMDVFS_CMD _IOW('O', 88, struct MTK_MMDVFS_CMD)

struct MTK_MMDVFS_QOS_CMD {
	unsigned int type;
	unsigned int max_cam_bw;
	unsigned int ret;
};

#define MTK_MMDVFS_QOS_CMD_TYPE_SET		0
#define MTK_IOC_MMDVFS_QOS_CMD \
	_IOW('O', 89, struct MTK_MMDVFS_QOS_CMD)


#endif
