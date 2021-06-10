/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#ifndef _MTK_SMI_H_
#define _MTK_SMI_H_

#define MTK_SMI_MAJOR_NUMBER 190

#define MTK_IOW(num, dtype)     _IOW('O', num, dtype)
#define MTK_IOR(num, dtype)     _IOR('O', num, dtype)
#define MTK_IOWR(num, dtype)    _IOWR('O', num, dtype)
#define MTK_IO(num)             _IO('O', num)

/* -------------------------------------------------------------------------- */
#define MTK_CONFIG_MM_MAU       MTK_IOW(10, unsigned long)


struct MTK_MAU_CONFIG {
	int larb;		/* 0~4: the larb you want to monitor */
	int entry;		/* 0~2: the mau entry to use */
	unsigned int port_msk;	/* port mask to be monitored */
	int virt;		/* 1: monitor va (this port is using m4u);  */
				/* 0: monitor pa (this port is not using m4u) */
	int monitor_read;     /* monitor read transaction 1-enable, 0-disable */
	int monitor_write;   /* monitor write transaction 1-enable, 0-disable */
	unsigned int start;	/* start address to monitor */
	unsigned int end;	/* end address to monitor */
};

int mau_config(struct MTK_MAU_CONFIG *pMauConf);
int mau_dump_status(int larb);

/* ------------------------------------------------------------------------- */
enum MTK_SMI_BWC_SCEN {
	SMI_BWC_SCEN_NORMAL,
	SMI_BWC_SCEN_VR,
	SMI_BWC_SCEN_SWDEC_VP,
	SMI_BWC_SCEN_VP,
	SMI_BWC_SCEN_VP_4KOSD,
	SMI_BWC_SCEN_VP_HIGH_FPS,
	SMI_BWC_SCEN_VP_HIGH_RESOLUTION,
	SMI_BWC_SCEN_VR_SLOW,
	SMI_BWC_SCEN_MM_GPU,
	SMI_BWC_SCEN_WFD,
	SMI_BWC_SCEN_VENC,
	SMI_BWC_SCEN_ICFP,
	SMI_BWC_SCEN_UI_IDLE,
	SMI_BWC_SCEN_VSS,
	SMI_BWC_SCEN_FORCE_MMDVFS,
	SMI_BWC_SCEN_HDMI,
	SMI_BWC_SCEN_HDMI4K,
	SMI_BWC_SCEN_VPMJC,
	SMI_BWC_SCEN_N3D,
	SMI_BWC_SCEN_CNT
};

/* MMDVFS */
enum mmdvfs_voltage_enum {
	MMDVFS_VOLTAGE_DEFAULT,
	MMDVFS_VOLTAGE_0 = MMDVFS_VOLTAGE_DEFAULT,
	MMDVFS_VOLTAGE_LOW = MMDVFS_VOLTAGE_0,
	MMDVFS_VOLTAGE_1,
	MMDVFS_VOLTAGE_HIGH = MMDVFS_VOLTAGE_1,
	MMDVFS_VOLTAGE_DEFAULT_STEP,
	MMDVFS_VOLTAGE_COUNT
};

struct MTK_SMI_BWC_CONFIG {
	enum MTK_SMI_BWC_SCEN scenario;
	int b_on_off;	/* 0 : exit this scenario , 1 : enter this scenario */
};

struct MTK_SMI_BWC_STATE {
	unsigned int *hwc_max_pixel;
};

struct MTK_SMI_BWC_REGISTER_SET {
	unsigned int address;
	unsigned int value;
};

struct MTK_SMI_BWC_REGISTER_GET {
	unsigned int address;
	unsigned int *return_address;
};

#define MMDVFS_CAMERA_MODE_FLAG_DEFAULT	1
#define MMDVFS_CAMERA_MODE_FLAG_PIP (1 << 1)
#define MMDVFS_CAMERA_MODE_FLAG_VFB (1 << 2)
#define MMDVFS_CAMERA_MODE_FLAG_EIS_2_0 (1 << 3)
#define MMDVFS_CAMERA_MODE_FLAG_IVHDR (1 << 4)
#define MMDVFS_CAMERA_MODE_FLAG_STEREO  (1 << 5)

struct MTK_MMDVFS_CMD {
	unsigned int type;
	enum MTK_SMI_BWC_SCEN scen;

	unsigned int sensor_size;
	unsigned int sensor_fps;
	unsigned int camera_mode;

	unsigned int venc_size;

	unsigned int ret;
};

#define MTK_MMDVFS_CMD_TYPE_SET		0
#define MTK_MMDVFS_CMD_TYPE_QUERY	1

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

#define MTK_IOC_SPC_CONFIG          MTK_IOW(20, unsigned long)
#define MTK_IOC_SPC_DUMP_REG        MTK_IOW(21, unsigned long)
#define MTK_IOC_SPC_DUMP_STA        MTK_IOW(22, unsigned long)
#define MTK_IOC_SPC_CMD             MTK_IOW(23, unsigned long)
#define MTK_IOC_SMI_BWC_CONFIG      MTK_IOW(24, struct MTK_SMI_BWC_CONFIG)
#define MTK_IOC_SMI_BWC_STATE       MTK_IOWR(25, struct MTK_SMI_BWC_STATE)
#define MTK_IOC_SMI_BWC_REGISTER_SET    \
				MTK_IOWR(26, struct MTK_SMI_BWC_REGISTER_SET)
#define MTK_IOC_SMI_BWC_REGISTER_GET    \
				MTK_IOWR(27, struct MTK_SMI_BWC_REGISTER_GET)

/* For BWC.MM property setting */
#define MTK_IOC_SMI_BWC_INFO_SET    MTK_IOWR(28, struct MTK_SMI_BWC_INFO_SET)
/* For BWC.MM property get */
#define MTK_IOC_SMI_BWC_INFO_GET    MTK_IOWR(29, struct MTK_SMI_BWC_MM_INFO)

/* GMP end */

#define MTK_IOC_SMI_DUMP_LARB       MTK_IOWR(66, unsigned int)
#define MTK_IOC_SMI_DUMP_COMMON     MTK_IOWR(67, unsigned int)
#define MTK_IOC_MMDVFS_CMD	    MTK_IOW(88, struct MTK_MMDVFS_CMD)


enum SPC_PROT_T {
	SPC_PROT_NO_PROT = 0,
	SPC_PROT_SEC_RW_ONLY,
	SPC_PROT_SEC_RW_NONSEC_R,
	SPC_PROT_NO_ACCESS,
};


struct MTK_SPC_CONFIG {
	enum SPC_PROT_T domain_0_prot;
	enum SPC_PROT_T domain_1_prot;
	enum SPC_PROT_T domain_2_prot;
	enum SPC_PROT_T domain_3_prot;
	unsigned int start;	/* start address to monitor */
	unsigned int end;	/* end address to monitor */
};

void spc_config(struct MTK_SPC_CONFIG *pCfg);
unsigned int spc_status_check(void);
unsigned int spc_dump_reg(void);
unsigned int spc_register_isr(void *dev);
unsigned int spc_clear_irq(void);
int spc_test(int code);
int MTK_SPC_Init(void *dev);

#define MMDVFS_ENABLE_DEFAULT_STEP_QUERY
#define MMDVFS_MMCLOCK_NOTIFICATION
/* MMDVFS kernel API */
extern int mmdvfs_set_step(enum MTK_SMI_BWC_SCEN scenario,
			    enum mmdvfs_voltage_enum step);
extern int mmdvfs_is_default_step_need_perf(void);
extern void mmdvfs_mm_clock_switch_notify(int is_before, int is_to_high);
#endif
