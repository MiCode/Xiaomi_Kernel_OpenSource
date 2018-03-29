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

#ifndef _H_DDP_INFO
#define _H_DDP_INFO
#include <linux/types.h>
#include "ddp_hal.h"
#include "../videox/DpDataType.h"
#include "lcm_drv.h"
#include "disp_event.h"
#include "ddp_ovl.h"
#include "../videox/disp_session.h"
typedef unsigned char UINT8;
typedef signed char INT8;
typedef unsigned short UINT16;
typedef unsigned int UINT32;
typedef signed int INT32;

typedef struct _OVL_CONFIG_STRUCT {
	unsigned int ovl_index;
	unsigned int layer;
	unsigned int layer_en;
	enum OVL_LAYER_SOURCE source;
	unsigned int fmt;
	unsigned long addr;
	unsigned long vaddr;
	unsigned int src_x;
	unsigned int src_y;
	unsigned int src_w;
	unsigned int src_h;
	unsigned int src_pitch;
	unsigned int dst_x;
	unsigned int dst_y;
	unsigned int dst_w;
	unsigned int dst_h;	/* clip region */
	unsigned int keyEn;
	unsigned int key;
	unsigned int aen;
	unsigned char alpha;

	unsigned int sur_aen;
	unsigned int src_alpha;
	unsigned int dst_alpha;

	unsigned int isTdshp;
	unsigned int isDirty;

	unsigned int buff_idx;
	unsigned int identity;
	unsigned int connected_type;
	unsigned int security;
	unsigned int yuv_range;
	unsigned int src_ori_x;
} OVL_CONFIG_STRUCT;

typedef struct _OVL_BASIC_STRUCT {
	unsigned int layer;
	unsigned int layer_en;
	unsigned int fmt;
	unsigned long addr;
	unsigned int src_w;
	unsigned int src_h;
	unsigned int src_pitch;
	unsigned int bpp;
} OVL_BASIC_STRUCT;

typedef struct _RDMA_BASIC_STRUCT {
	unsigned long addr;
	unsigned int src_w;
	unsigned int src_h;
	unsigned int bpp;
} RDMA_BASIC_STRUCT;

typedef struct _RDMA_CONFIG_STRUCT {
	unsigned idx;		/* instance index */
	DpColorFormat inputFormat;
	unsigned long address;
	unsigned pitch;
	unsigned width;
	unsigned height;
	DISP_BUFFER_TYPE security;
	unsigned int buf_offset;
} RDMA_CONFIG_STRUCT;

typedef struct _WDMA_CONFIG_STRUCT {
	unsigned srcWidth;
	unsigned srcHeight;	/* input */
	unsigned clipX;
	unsigned clipY;
	unsigned clipWidth;
	unsigned clipHeight;	/* clip */
	DpColorFormat outputFormat;
	unsigned long dstAddress;
	unsigned dstPitch;	/* output */
	unsigned int useSpecifiedAlpha;
	unsigned char alpha;
	DISP_BUFFER_TYPE security;
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
	unsigned int split_buf_offset;
#endif
} WDMA_CONFIG_STRUCT;

typedef struct {
	/* for ovl */
	bool ovl_dirty;
	bool rdma_dirty;
	bool wdma_dirty;
	bool dst_dirty;
	OVL_CONFIG_STRUCT ovl_config[4];
	RDMA_CONFIG_STRUCT rdma_config;
	WDMA_CONFIG_STRUCT wdma_config;
	LCM_PARAMS dispif_config;
	unsigned int lcm_bpp;
	unsigned int dst_w;
	unsigned int dst_h;
	unsigned int fps;
} disp_ddp_path_config;

struct SWITCH_MODE_INFO_STRUCT {
	unsigned int old_session;
	unsigned int old_mode;
	unsigned int cur_mode;
	unsigned int ext_req;
	unsigned int switching;
	unsigned int ext_sid;
};

typedef int (*ddp_module_notify) (DISP_MODULE_ENUM, DISP_PATH_EVENT);

typedef struct {
	DISP_MODULE_ENUM module;
	int (*init)(DISP_MODULE_ENUM module, void *handle);
	int (*deinit)(DISP_MODULE_ENUM module, void *handle);
	int (*config)(DISP_MODULE_ENUM module, disp_ddp_path_config *config, void *handle);
	int (*start)(DISP_MODULE_ENUM module, void *handle);
	int (*trigger)(DISP_MODULE_ENUM module, void *handle);
	int (*stop)(DISP_MODULE_ENUM module, void *handle);
	int (*reset)(DISP_MODULE_ENUM module, void *handle);
	int (*power_on)(DISP_MODULE_ENUM module, void *handle);
	int (*power_off)(DISP_MODULE_ENUM module, void *handle);
	int (*suspend)(DISP_MODULE_ENUM module, void *handle);
	int (*resume)(DISP_MODULE_ENUM module, void *handle);
	int (*is_idle)(DISP_MODULE_ENUM module);
	int (*is_busy)(DISP_MODULE_ENUM module);
	int (*dump_info)(DISP_MODULE_ENUM module, int level);
	int (*bypass)(DISP_MODULE_ENUM module, int bypass);
	int (*build_cmdq)(DISP_MODULE_ENUM module, void *cmdq_handle, CMDQ_STATE state);
	int (*set_lcm_utils)(DISP_MODULE_ENUM module, LCM_DRIVER *lcm_drv);
	int (*set_listener)(DISP_MODULE_ENUM module, ddp_module_notify notify);
	int (*cmd)(DISP_MODULE_ENUM module, int msg, unsigned long arg, void *handle);
	int (*ioctl)(DISP_MODULE_ENUM module, void *handle, unsigned int ioctl_cmd,
		      unsigned long *params);
} DDP_MODULE_DRIVER;

char *ddp_get_module_name(DISP_MODULE_ENUM module);
int ddp_get_module_max_irq_bit(DISP_MODULE_ENUM module);
unsigned int ddp_driver_init(void);

/* dsi */
extern DDP_MODULE_DRIVER ddp_driver_dsi0;
extern DDP_MODULE_DRIVER ddp_driver_dsi1;
extern DDP_MODULE_DRIVER ddp_driver_dsidual;
/* dpi */
extern DDP_MODULE_DRIVER ddp_driver_dpi0;
extern DDP_MODULE_DRIVER ddp_driver_dpi1;

/* ovl */
extern DDP_MODULE_DRIVER ddp_driver_ovl;
/* rdma */
extern DDP_MODULE_DRIVER ddp_driver_rdma;
/* wdma */
extern DDP_MODULE_DRIVER ddp_driver_wdma;
/* color */
extern DDP_MODULE_DRIVER ddp_driver_color;
/* aal */
extern DDP_MODULE_DRIVER ddp_driver_aal;
/* od */
extern DDP_MODULE_DRIVER ddp_driver_od;
/* ufoe */
extern DDP_MODULE_DRIVER ddp_driver_ufoe;
/* gamma */
extern DDP_MODULE_DRIVER ddp_driver_gamma;

/* split */
extern DDP_MODULE_DRIVER ddp_driver_split;

/* pwm */
extern DDP_MODULE_DRIVER ddp_driver_pwm;

extern DDP_MODULE_DRIVER *ddp_modules_driver[DISP_MODULE_NUM];


#endif
