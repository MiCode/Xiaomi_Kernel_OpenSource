/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2020 XiaoMi, Inc. All rights reserved.
 */

#ifndef _MI_DSI_PANEL_H_
#define _MI_DSI_PANEL_H_

#include <linux/types.h>
#include "dsi_defs.h"
#include "mi_disp_feature.h"
#include <linux/pm_wakeup.h>
#include <drm/mi_disp.h>

struct dsi_panel;

enum backlight_dimming_state {
	STATE_NONE,
	STATE_DIM_BLOCK,
	STATE_DIM_RESTORE,
	STATE_ALL
};

enum dimming_speed_state {
	DIMMING_NONE = 0,
	DIMMING_LOW_BL,
	DIMMING_MEDIUM_BL,
	DIMMING_NORMAL_BL,
	DIMMING_HIGH_BL,
	DIMMING_ALL
};

enum dimming_speed_switch {
	DIMMING_SPEED_SWITCH_OFF = 0,
	DIMMING_SPEED_SWITCH_ON,
	DIMMING_SPEED_SWITCH_DEFAULT,
	DIMMING_SPEED_SWITCH_ALL
};

enum dimming_speed {
	DIMMING_SPEED_0FRAME = 0,
	DIMMING_SPEED_4FRAME = 4,
	DIMMING_SPEED_8FRAME = 8,
	DIMMING_SPEED_ALL
};

enum panel_state {
	PANEL_STATE_OFF = 0,
	PANEL_STATE_ON,
	PANEL_STATE_DOZE_HIGH,
	PANEL_STATE_DOZE_LOW,
	PANEL_STATE_DOZE_TO_NORMAL,
	PANEL_STATE_MAX,
};

enum dc_lut_state {
	DC_LUT_60HZ,
	DC_LUT_120HZ,
	DC_LUT_MAX
};

enum dc_feature_type {
	TYPE_NONE = 0,
	TYPE_LUT_COMPATIBLE_BL,
	TYPE_CRC_SKIP_BL
};

/* Enter/Exit DC_LUT info */
struct dc_lut_cfg {
	bool update_done;
	u8 enter_dc_lut[DC_LUT_MAX][75];
	u8 exit_dc_lut[DC_LUT_MAX][75];
};

struct flat_mode_cfg {
	bool update_done;
	bool cur_flat_state;  /*only use when flat cmd need sync with te*/
	u8 flat_on_data[4];
	u8 flat_off_data[4];
};

struct mi_dsi_panel_cfg {
	struct dsi_panel *dsi_panel;

	/* xiaomi panel id */
	u64 mi_panel_id;

	/* xiaomi feature values */
	int feature_val[DISP_FEATURE_MAX];

	/* indicate esd check gpio and config irq */
	int esd_err_irq_gpio;
	int esd_err_irq;
	int esd_err_irq_flags;
	bool esd_err_enabled;

	/* brightness control */
	u32 last_bl_level;
	u32 last_no_zero_bl_level;
	/* set by user space */
	/* maybe less than @brightness_clone due to thermal limit */
	u32 user_brightness_clone;
	atomic_t real_brightness_clone;
	u32 max_brightness_clone;
	u32 normal_max_brightness_clone;
	u32 thermal_max_brightness_clone;
	bool thermal_dimming_enabled;

	/* AOD control */
	u32 doze_brightness;
	struct mutex doze_lock;
	struct wakeup_source *disp_wakelock;
	int doze_hbm_dbv_level;
	int doze_lbm_dbv_level;
	bool bl_wait_frame;
	bool bl_enable;

	/* Local HBM */
	bool local_hbm_enabled;
	u32 lhbm_ui_ready_delay_frame;
	bool need_fod_animal_in_normal;
	bool aod_to_normal_statue;
	bool in_fod_calibration;
	bool fod_low_brightness_allow;
	int fod_low_brightness_lux_threshold;
	int fod_low_brightness_clone_threshold;
	bool lhbm_g500_update_flag;
	bool lhbm_g500_updatedone;
	bool lhbm_alpha_ctrlaa;
	bool lhbm_ctrl_df_reg;
	bool lhbm_ctrl_b2_reg;
	bool lhbm_w1000_update_flag;
	bool lhbm_w1000_readbackdone;
	bool lhbm_w110_update_flag;
	bool lhbm_w110_readbackdone;
	bool uefi_read_lhbm_success;
	bool uefi_read_gray_scale_success;
	u8 lhbm_rgb_param[24];
	u8 gray_scale_info[42];
	u8 whitebuf_1000_gir_on[6];
	u8 whitebuf_1000_gir_off[6];
	u8 whitebuf_110_gir_on[6];
	u8 whitebuf_110_gir_off[6];
	u8 greenbuf_500nit[6];

	/* DDIC round corner */
	bool ddic_round_corner_enabled;

	/* DC */
	bool dc_feature_enable;
	bool dc_update_flag;
	enum dc_feature_type dc_type;
	u32 dc_threshold;
	struct dc_lut_cfg dc_cfg;
	u32 real_dc_state;

	/* flat mode */
	bool flat_sync_te;
	bool flat_update_flag;
	bool flat_update_gamma_zero;
	struct flat_mode_cfg flat_cfg;

    /* record the last refresh_rate */
	u32 last_refresh_rate;

	/* Dimming */
	u32 panel_on_dimming_delay;
	u32 dimming_state;
	bool dimming_need_update_speed;
	u8 dimming_speed_state;
	u32 dimming_node[5];

	/* Panel status */
	int panel_state;

	u8 panel_batch_number;
	bool panel_batch_number_read_done;
};

struct panel_batch_info
{
	u8 batch_number;       /* Panel batch number */
	char *batch_name;      /* Panel batch name */
};

extern const char *cmd_set_prop_map[DSI_CMD_SET_MAX];
extern const char *cmd_set_update_map[DSI_CMD_UPDATE_MAX];

int mi_dsi_panel_init(struct dsi_panel *panel);
int mi_dsi_panel_deinit(struct dsi_panel *panel);
int mi_dsi_acquire_wakelock(struct dsi_panel *panel);
int mi_dsi_release_wakelock(struct dsi_panel *panel);

bool is_target_fps_support_dc(struct dsi_panel *panel);

bool is_aod_and_panel_initialized(struct dsi_panel *panel);

bool is_backlight_set_skip(struct dsi_panel *panel, u32 bl_lvl);

void mi_dsi_panel_update_last_bl_level(struct dsi_panel *panel,
			int brightness);

void mi_dsi_panel_update_dimming_frame(struct dsi_panel *panel,
			u8 dimming_switch, u8 frame);

bool is_hbm_fod_on(struct dsi_panel *panel);

int mi_dsi_panel_esd_irq_ctrl(struct dsi_panel *panel,
			bool enable);

int mi_dsi_panel_esd_irq_ctrl_locked(struct dsi_panel *panel,
			bool enable);

int mi_dsi_print_51_backlight_log(struct dsi_panel *panel,
			struct dsi_cmd_desc *cmd);

int mi_dsi_panel_parse_cmd_sets_update(struct dsi_panel *panel,
		struct dsi_display_mode *mode);

int mi_dsi_panel_parse_gamma_config(struct dsi_panel *panel,
		struct dsi_display_mode *mode);

int mi_dsi_panel_update_cmd_set(struct dsi_panel *panel,
			struct dsi_display_mode *cur_mode, enum dsi_cmd_set_type type,
			struct dsi_cmd_update_info *info, u8 *payload, u32 size);

int mi_dsi_panel_write_cmd_set(struct dsi_panel *panel,
			struct dsi_panel_cmd_set *cmd_sets);

int mi_dsi_panel_read_batch_number(struct dsi_panel *panel);

bool mi_dsi_panel_is_need_tx_cmd(u32 feature_id);

int mi_dsi_panel_set_disp_param(struct dsi_panel *panel,
			struct disp_feature_ctl *ctl);

int mi_dsi_panel_get_disp_param(struct dsi_panel *panel,
			struct disp_feature_ctl *ctl);

ssize_t mi_dsi_panel_show_disp_param(struct dsi_panel *panel,
			char *buf, size_t size);

int mi_dsi_panel_set_doze_brightness(struct dsi_panel *panel,
			u32 doze_brightness);

int mi_dsi_panel_get_doze_brightness(struct dsi_panel *panel,
			u32 *doze_brightness);

int mi_dsi_panel_get_brightness(struct dsi_panel *panel,
			u32 *brightness);

int mi_dsi_panel_write_dsi_cmd(struct dsi_panel *panel,
			struct dsi_cmd_rw_ctl *ctl);

int mi_dsi_panel_write_dsi_cmd_set(struct dsi_panel *panel, int type);

ssize_t mi_dsi_panel_show_dsi_cmd_set_type(struct dsi_panel *panel,
			char *buf, size_t size);

int mi_dsi_panel_set_brightness_clone(struct dsi_panel *panel,
			u32 brightness_clone);

int mi_dsi_panel_get_brightness_clone(struct dsi_panel *panel,
			u32 *brightness_clone);

int mi_dsi_panel_get_max_brightness_clone(struct dsi_panel *panel,
			u32 *max_brightness_clone);

int mi_dsi_panel_set_dc_mode(struct dsi_panel *panel, bool enable);

int mi_dsi_panel_set_dc_mode_locked(struct dsi_panel *panel, bool enable);

int mi_dsi_panel_read_and_update_flat_param_locked(struct dsi_panel *panel);

int mi_dsi_panel_read_and_update_flat_param(struct dsi_panel *panel);

int mi_dsi_panel_read_and_update_dc_param_locked(struct dsi_panel *panel);

int mi_dsi_panel_read_and_update_dc_param(struct dsi_panel *panel);

int mi_dsi_panel_read_manufacturer_info_locked(struct dsi_panel *panel,
		u32 manufacturer_info_addr, char *rdbuf, int rdlen);

int mi_dsi_panel_read_manufacturer_info(struct dsi_panel *panel,
		u32 manufacturer_info_addr, char *rdbuf, int rdlen);


int mi_dsi_panel_read_lhbm_white_param_locked(struct dsi_panel *panel);

int mi_dsi_panel_read_lhbm_white_param(struct dsi_panel *panel);

int mi_dsi_panel_set_round_corner_locked(struct dsi_panel *panel,
			bool enable);

int mi_dsi_panel_set_round_corner(struct dsi_panel *panel,
			bool enable);

int mi_dsi_panel_switch_page_locked(struct dsi_panel *panel, u8 page_index);

int mi_dsi_panel_switch_page(struct dsi_panel *panel, u8 page_index);

int mi_dsi_update_switch_cmd(struct dsi_panel *panel);

int mi_dsi_update_nolp_cmd_B2reg(struct dsi_panel *panel,
			enum dsi_cmd_set_type type);

int mi_dsi_update_51_mipi_cmd(struct dsi_panel *panel,enum dsi_cmd_set_type type, int bl_lvl);

#endif /* _MI_DSI_PANEL_H_ */
