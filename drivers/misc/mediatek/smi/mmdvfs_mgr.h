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

#ifndef __MMDVFS_MGR_H__
#define __MMDVFS_MGR_H__

#include <linux/plist.h>
#include <mt-plat/mtk_smi.h>

/* from smi_configuration.h */
#define SMI_PARAM_DISABLE_MMDVFS				(0)
#define SMI_PARAM_DISABLE_FREQ_MUX				(1)
#define SMI_PARAM_DISABLE_FREQ_HOPPING			(1)
#define SMI_PARAM_DISABLE_FORCE_MMSYS_MAX_CLK	(1)

/* implement in smi_legacy.c */
int mmdvfs_scen_log_mask_get(void);
int mmdvfs_debug_level_get(void);

/* MMDVFS extern APIs */
extern void mmdvfs_init(struct MTK_SMI_BWC_MM_INFO *info);
extern void mmdvfs_handle_cmd(struct MTK_MMDVFS_CMD *cmd);
extern void mmdvfs_notify_scenario_enter(enum MTK_SMI_BWC_SCEN scen);
extern void mmdvfs_notify_scenario_exit(enum MTK_SMI_BWC_SCEN scen);
extern void mmdvfs_notify_scenario_concurrency(unsigned int u4Concurrency);
extern void mmdvfs_mhl_enable(int enable);
extern void mmdvfs_mjc_enable(int enable);

/* screen size */
extern unsigned int DISP_GetScreenWidth(void);
extern unsigned int DISP_GetScreenHeight(void);

#define MMDVFS_MMSYS_CLK_COUNT (3)
#define MMSYS_CLK_LOW (0)
#define MMSYS_CLK_HIGH (1)
#define MMSYS_CLK_MEDIUM (2)

#define MMDVFS_EVENT_OVL_SINGLE_LAYER_ENTER 0
#define MMDVFS_EVENT_OVL_SINGLE_LAYER_EXIT 1
#define MMDVFS_EVENT_UI_IDLE_ENTER 2
#define MMDVFS_EVENT_UI_IDLE_EXIT 3
#define MMDVFS_CLIENT_ID_ISP 0

#define QOS_ALL_SCENARIO 99
enum {
	MMDVFS_CAM_MON_SCEN = SMI_BWC_SCEN_CNT, MMDVFS_SCEN_MHL,
	MMDVFS_SCEN_MJC, MMDVFS_SCEN_DISP, MMDVFS_SCEN_ISP,
	MMDVFS_SCEN_VP_HIGH_RESOLUTION, MMDVFS_SCEN_VPU, MMDVFS_MGR,
	MMDVFS_SCEN_VPU_KERNEL, MMDVFS_PMQOS_ISP,
	MMDVFS_SCEN_VP_WFD, MMDVFS_SCEN_COUNT
};

#define LEGACY_CAM_SCENS ((1 << SMI_BWC_SCEN_VR) | \
	(1 << SMI_BWC_SCEN_VR_SLOW) |	\
	(1 << SMI_BWC_SCEN_VSS) |	\
	(1 << SMI_BWC_SCEN_CAM_PV) |	\
	(1 << SMI_BWC_SCEN_CAM_CP) |	\
	(1 << SMI_BWC_SCEN_ICFP) |	\
	(1 << MMDVFS_SCEN_ISP))

enum mmdvfs_vpu_clk {
		vpu_clk_0, vpu_clk_1, vpu_clk_2, vpu_clk_3
};

enum mmdvfs_vpu_if_clk {
		vpu_if_clk_0, vpu_if_clk_1, vpu_if_clk_2, vpu_if_clk_3
};

enum mmdvfs_vimvo_vol {
		vimvo_vol_0, vimvo_vol_1, vimvo_vol_2, vimvo_vol_3
};


struct mmdvfs_state_change_event {
	int scenario;
	int feature_flag;
	int sensor_size;
	int sensor_fps;
	int vcore_vol_step;
	int mmsys_clk_step;
	int vpu_clk_step;
	int vpu_if_clk_step;
	int vimvo_vol_step;
};

struct mmdvfs_pm_qos_request {
	struct plist_node node;   /* reserved for QoS function */
	int pm_qos_class;
	struct delayed_work work; /* reserved for timeout handling */
};

#define MMDVFS_EVENT_PREPARE_CALIBRATION_START 0
#define MMDVFS_EVENT_PREPARE_CALIBRATION_END 1

struct mmdvfs_prepare_event {
	int event_type;
};

typedef int (*clk_switch_cb)(
	int ori_mmsys_clk_mode, int update_mmsys_clk_mode);
typedef int (*vdec_ctrl_cb)(void);
typedef int (*mmdvfs_state_change_cb)(struct mmdvfs_state_change_event *event);
typedef int (*mmdvfs_prepare_cb)(struct mmdvfs_prepare_event *event);
/* MMDVFS V2 only APIs */
extern int mmdvfs_register_mmclk_switch_cb(clk_switch_cb notify_cb,
	int mmdvfs_client_id);
extern int mmdvfs_raise_mmsys_by_mux(void);

/* Extern from other module */
extern enum MTK_SMI_BWC_SCEN smi_get_current_profile(void);
extern int is_mmdvfs_freq_hopping_disabled(void);
extern int is_mmdvfs_freq_mux_disabled(void);
extern int is_force_max_mmsys_clk(void);
extern int is_force_camera_hpm(void);
extern int is_mmdvfs_disabled(void);
extern int force_always_on_mm_clks(void);
extern int mmdvfs_get_stable_isp_clk(void);
extern int get_mmdvfs_clk_mux_mask(void);


#define MMDVFS_PROFILE_UNKNOWN (0)
#define MMDVFS_PROFILE_VIN (1)
#define MMDVFS_PROFILE_CER (2)
#define MMDVFS_PROFILE_MER (3)
#define MMDVFS_PROFILE_EIG (4)
#define MMDVFS_PROFILE_LAF (5)
#define MMDVFS_PROFILE_BIA (6)

/* Macro used to resovling step setting ioctl command */
#define MMDVFS_CMD_STEP_LEN (8)
#define MMDVFS_CMD_STEP_MASK (0xFF)
#define MMDVFS_CMD_MMCLK_MASK (0xFF00)
#define MMDVFS_CMD_DDR_TYPE_AUTO_SELECT (0xFF)
#define MMDVFS_CMD_VPU_STEP_UNREQUEST (0xFFFF)


/* Backward compatible */
#define SMI_BWC_SCEN_120HZ MMDVFS_SCEN_DISP

/* mmdvfs display sizes */
#define MMDVFS_DISPLAY_SIZE_HD  (1280 * 832)
#define MMDVFS_DISPLAY_SIZE_FHD (1920 * 1216)

enum mmdvfs_lcd_size_enum {
	MMDVFS_LCD_SIZE_HD, MMDVFS_LCD_SIZE_FHD,
	MMDVFS_LCD_SIZE_WQHD, MMDVFS_LCD_SIZE_END_OF_ENUM
};

#ifndef CONFIG_MTK_SMI_EXT
#define mmdvfs_set_step(scenario, step)
#define mmdvfs_set_fine_step(scenario, step)
#else
int mmdvfs_set_step(
	enum MTK_SMI_BWC_SCEN scenario, enum mmdvfs_voltage_enum step);
int mmdvfs_set_fine_step(
	enum MTK_SMI_BWC_SCEN smi_scenario, int mmdvfs_fine_step);
#endif /* CONFIG_MTK_SMI_EXT */

extern int mmdvfs_get_mmdvfs_profile(void);
extern int is_mmdvfs_supported(void);
extern enum mmdvfs_lcd_size_enum mmdvfs_get_lcd_resolution(void);
extern int register_mmdvfs_state_change_cb(
	int mmdvfs_client_id, mmdvfs_state_change_cb func);
extern void mmdvfs_notify_prepare_action(
	struct mmdvfs_prepare_event *event);
extern int register_mmdvfs_prepare_cb(
	int mmdvfs_client_id, mmdvfs_prepare_cb func);

/* The following interface can only used by mmdvfs itself for */
/* default step configuration */
extern void mmdvfs_default_step_set(int default_step);
extern void mmdvfs_default_start_delayed_setting(void);
extern void mmdvfs_default_stop_delayed_setting(void);
extern void mmdvfs_debug_set_mmdvfs_clks_enabled(int clk_enable_request);
extern int mmdvfs_get_current_fine_step(void);


extern void mmdvfs_pm_qos_update_request(struct mmdvfs_pm_qos_request *req,
	u32 mmdvfs_pm_qos_class, u32 new_value);
extern void mmdvfs_pm_qos_remove_request(struct mmdvfs_pm_qos_request *req);
extern void mmdvfs_pm_qos_add_request(struct mmdvfs_pm_qos_request *req,
	u32 mmdvfs_pm_qos_class, u32 value);
extern u32 mmdvfs_qos_get_thres_count(
	struct mmdvfs_pm_qos_request *req, u32 mmdvfs_pm_qos_class);
extern u32 mmdvfs_qos_get_thres_value(
	struct mmdvfs_pm_qos_request *req, u32 mmdvfs_pm_qos_class,
	u32 thres_idx);
extern u32 mmdvfs_qos_get_cur_thres(
	struct mmdvfs_pm_qos_request *req, u32 mmdvfs_pm_qos_class);

#include "mmdvfs_config_util.h"

#endif /* __MMDVFS_MGR_H__ */
