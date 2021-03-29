/*
 * Copyright (C) 2017 MediaTek Inc.
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

#ifndef __EXTD_DDP_H__
#define __EXTD_DDP_H__

#include "ddp_hal.h"
#include "ddp_manager.h"

#define ALIGN_TO(x, n)  (((x) + ((n) - 1)) & ~((n) - 1))


enum EXT_DISP_PATH_MODE {
	EXTD_DIRECT_LINK_MODE,
	EXTD_DECOUPLE_MODE,
	EXTD_SINGLE_LAYER_MODE,
	EXTD_RDMA_DPI_MODE
};

enum EXT_DISP_STATUS {
	EXT_DISP_STATUS_OK = 0,

	EXT_DISP_STATUS_NOT_IMPLEMENTED,
	EXT_DISP_STATUS_ALREADY_SET,
	EXT_DISP_STATUS_ERROR
};

enum EXTD_POWER_STATE {
	EXTD_DEINIT = 0,
	EXTD_INIT,
	EXTD_RESUME,
	EXTD_SUSPEND
};

struct ext_disp_input_config {
	unsigned int layer;
	unsigned int layer_en;
	unsigned int buff_source;
	unsigned int fmt;
	unsigned long addr;
	unsigned long addr_sub_u;
	unsigned long addr_sub_v;
	unsigned long vaddr;
	unsigned int src_x;
	unsigned int src_y;
	unsigned int src_w;
	unsigned int src_h;
	unsigned int src_pitch;
	unsigned int dst_x;
	unsigned int dst_y;
	unsigned int dst_w;
	unsigned int dst_h; /* clip region */
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
	unsigned int dirty;
};

struct EXTERNAL_DISPLAY_UTIL_FUNCS {
	void (*hdmi_video_format_config)(unsigned int layer_3d_format);
};

void ext_disp_probe(void);
int ext_disp_init(char *lcm_name, unsigned int session);
int ext_disp_deinit(unsigned int session);
int ext_disp_suspend(unsigned int session);
int ext_disp_suspend_trigger(void *callback, unsigned int userdata,
			     unsigned int session);
int ext_disp_resume(unsigned int session);
enum EXT_DISP_PATH_MODE ext_disp_path_get_mode(unsigned int session);
void ext_disp_path_set_mode(enum EXT_DISP_PATH_MODE mode, unsigned int session);
unsigned int ext_disp_get_sess_id(void);
int ext_disp_frame_cfg_input(struct disp_frame_cfg_t *cfg);
int ext_disp_get_width(void);
int ext_disp_get_height(void);
int ext_disp_is_alive(void);
int ext_disp_is_sleepd(void);
int ext_disp_wait_for_vsync(void *config, unsigned int session);
int ext_fence_release_callback(unsigned long userdata);
int ext_disp_trigger(int blocking, void *callback, unsigned int userdata,
		     unsigned int session);
int ext_disp_is_video_mode(void);
enum CMDQ_SWITCH ext_disp_cmdq_enabled(void);
int ext_disp_switch_cmdq(enum CMDQ_SWITCH use_cmdq);
int ext_disp_diagnose(void);
void ext_disp_get_curr_addr(unsigned long *input_curr_addr, int module);
int ext_disp_factory_test(int mode, void *config);
int ext_disp_get_handle(disp_path_handle *dp_handle,
			struct cmdqRecStruct **pHandle);
int ext_disp_set_ovl1_status(int status);
int ext_disp_set_lcm_param(struct LCM_PARAMS *pLCMParam);
enum EXTD_OVL_REQ_STATUS ext_disp_get_ovl_req_status(unsigned int session);
int ext_disp_path_change(enum EXTD_OVL_REQ_STATUS action, unsigned int session);
int ext_disp_wait_ovl_available(int ovl_num);
bool ext_disp_path_source_is_RDMA(unsigned int session);
int ext_disp_is_dim_layer(unsigned long mva);
int ext_disp_get_max_layer(void);
void
extd_disp_drv_set_util_funcs(const struct EXTERNAL_DISPLAY_UTIL_FUNCS *util);

extern int is_dim_layer(unsigned long mva);

#endif /* __EXTD_DDP_H__ */
