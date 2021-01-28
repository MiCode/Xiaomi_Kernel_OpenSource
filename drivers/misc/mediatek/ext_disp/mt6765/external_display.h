/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _EXTD_DDP_H_
#define _EXTD_DDP_H_

#include "ddp_hal.h"
#include "ddp_manager.h"
#include "extd_info.h"
#include "disp_lcm.h"

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

enum EXTD_LCM_STATE {
	EXTD_LCM_NO_INIT = 0,
	EXTD_LCM_INITED,
	EXTD_LCM_RESUME,
	EXTD_LCM_SUSPEND
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
	unsigned int dirty;
};

struct EXTERNAL_DISPLAY_UTIL_FUNCS {
	void (*hdmi_video_format_config)(unsigned int layer_3d_format);
};
extern unsigned int dst_is_dsi;
void ext_disp_probe(void);
int ext_disp_init(char *lcm_name, unsigned int session);
int ext_disp_deinit(unsigned int session);
int ext_disp_suspend(unsigned int session);
int ext_disp_suspend_trigger(void *callback, unsigned int userdata,
			     unsigned int session);
int ext_disp_resume(unsigned int session);
enum EXT_DISP_PATH_MODE ext_disp_path_get_mode(unsigned int session);
void ext_disp_path_set_mode(enum EXT_DISP_PATH_MODE mode, unsigned int session);

void ext_disp_esd_check_lock(void);
void ext_disp_esd_check_unlock(void);
int ext_disp_esd_recovery(void);

unsigned int ext_disp_get_sess_id(void);
int ext_disp_frame_cfg_input(struct disp_frame_cfg_t *cfg);
int ext_disp_get_width(unsigned int session);
int ext_disp_get_height(unsigned int session);
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
void extd_disp_get_interface(struct disp_lcm_handle **plcm);
int ext_disp_get_max_layer(void);
void extd_disp_drv_set_util_funcs(const struct EXTERNAL_DISPLAY_UTIL_FUNCS
				  *util);
void _ext_cmdq_insert_wait_frame_done_token(void *handle);

extern int is_dim_layer(unsigned long mva);

int ext_disp_manual_lock(void);
int ext_disp_manual_unlock(void);
void _cmdq_start_extd_trigger_loop(void);
void _cmdq_stop_extd_trigger_loop(void);

#if defined(CONFIG_MTK_DUAL_DISPLAY_SUPPORT) &&	\
		(CONFIG_MTK_DUAL_DISPLAY_SUPPORT != 2)
/* defined in mtkfb.c should move to mtkfb.h*/
extern char ext_mtkfb_lcm_name[];

int external_display_setbacklight(unsigned int level);
enum EXTD_POWER_STATE ext_disp_get_state(void);
long ext_disp_wait_state(enum EXTD_POWER_STATE state, long timeout);
void *ext_disp_get_dpmgr_handle(void);
enum EXTD_POWER_STATE ext_disp_set_state(enum EXTD_POWER_STATE new_state);

#endif

#endif
