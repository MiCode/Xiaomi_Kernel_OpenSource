/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _DSI_PANEL_MI_H_
#define _DSI_PANEL_MI_H_

#include <linux/of_device.h>
#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/errno.h>
#include <linux/backlight.h>

#include <drm/drmP.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include <drm/drm_crtc_helper.h>
#include <linux/clk.h>
#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <linux/component.h>
#include <linux/irq.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_graph.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <video/mipi_display.h>
#include <video/videomode.h>
#include <linux/soc/mediatek/mtk-cmdq.h>
#if defined(CONFIG_MACH_MT6873)
#include <linux/ratelimit.h>
#endif

#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_crtc.h"
#include "mtk_drm_drv.h"
#include "mtk_drm_helper.h"
#include "mtk_mipi_tx.h"
#include "mtk_dump.h"
#include "mtk_log.h"
#include "mtk_drm_lowpower.h"
#include "mtk_drm_mmp.h"

#include "mi_disp_feature.h"

#define DSI_READ_WRITE_PANEL_DEBUG 1

#define DEFAULT_FOD_OFF_DIMMING_DELAY     170
#define DEFAULT_FOD_OFF_ENTER_AOD_DELAY   300

enum bkl_dimming_state {
	STATE_NONE,
	STATE_DIM_BLOCK,
	STATE_DIM_RESTORE,
	STATE_ALL
};

enum dsi_gamma_cmd_set_type {
	DSI_CMD_SET_MI_GAMMA_SWITCH_60HZ = 0,
	DSI_CMD_SET_MI_GAMMA_SWITCH_90HZ,
	DSI_CMD_SET_MI_GAMMA_SWITCH_MAX
};

struct dsi_cmd_desc {
	struct mipi_dsi_msg msg;
	bool last_command;
	u32  post_wait_ms;
};

struct t_condition_wq {
	wait_queue_head_t wq;
	atomic_t condition;
};

struct mtk_dsi_mgr {
	struct mtk_dsi *master;
	struct mtk_dsi *slave;
};

struct mi_dsi_panel_cfg {
	struct mtk_dsi *dsi;

	/* xiaomi panel id */
	u64 panel_id;

	/* xiaomi feature values */
	int feature_val[DISP_FEATURE_MAX];

	/* bl_is_big_endian indicate brightness value
	 * high byte to 1st parameter, low byte to 2nd parameter
	 * eg: 0x51 { 0x03, 0xFF } ->
	 * u8 payload[2] = { brightness >> 8, brightness & 0xff}
	 */
	bool bl_is_big_endian;
	u32 last_bl_level;

	/* indicate refresh frequency Fps gpio */
	int disp_rate_gpio;

	/* gamma read */
	bool gamma_update_flag;

	/* indicate esd check gpio and config irq */
	int esd_err_irq_gpio;
	int esd_err_irq;
	int esd_err_irq_flags;
	bool esd_err_enabled;

	u32 doze_brightness;
	/* Some panel nolp command is different according to current doze brightness set,
	 * But sometimes doze brightness change to DOZE_TO_NORMAL before nolp. So this
	 * doze_brightness_backup will save doze_brightness and only change to DOZE_TO_NORMAL by nolp.
	 */
	u32 doze_brightness_backup;

	bool hbm_51_ctl_flag;
	int hbm_on_51_index;
	int hbm_off_51_index;
	int hbm_fod_on_51_index;
	int hbm_fod_off_51_index;
	int hbm_fod_bl_lvl;
	int hbm_bl_min_lvl;
	int hbm_bl_max_lvl;
	int hbm_brightness_flag;

	bool in_fod_calibration;

	u32 panel_on_dimming_delay;

	/* AOD Nolp code customized*/
	bool aod_nolp_command_enabled;

	bool fod_hbm_layer_enabled;
	bool fod_anim_layer_enabled;
	u32 fod_ui_ready;

	bool delay_before_fod_hbm_on;
	bool delay_before_fod_hbm_off;

	u32 dimming_state;

	bool aod_bl_51ctl;
	bool dfps_bl_ctrl;
	u32 dfps_bl_threshold;

	u32 dc_type;
	u32 dc_threshold;
	u32 brightness_clone;
	u32 real_brightness_clone;
	u32 max_brightness_clone;
	u32 thermal_max_brightness_clone;

	bool local_hbm_enabled;
	int local_hbm_on_1000nit_51_index;
	int local_hbm_off_to_hbm_51_index;
	u32 fod_low_brightness_clone_threshold;
	u32 fod_low_brightness_lux_threshold;
	int local_hbm_target;

	int doze_hbm_dbv_level;
	int doze_lbm_dbv_level;

	bool fod_backlight_flag;
	bool fod_hbm_flag;
	bool normal_hbm_flag;
	bool dc_flag;
};

struct mtk_dsi {
	struct mtk_ddp_comp ddp_comp;
	struct device *dev;
	struct mipi_dsi_host host;
	struct drm_encoder encoder;
	struct drm_connector conn;
	struct drm_panel *panel;
	struct mtk_panel_ext *ext;
	struct cmdq_pkt_buffer cmdq_buf;
	struct drm_bridge *bridge;
	struct phy *phy;
	bool is_slave;
	struct mtk_dsi *slave_dsi;
	struct mtk_dsi *master_dsi;

	void __iomem *regs;

	struct clk *engine_clk;
	struct clk *digital_clk;
	struct clk *hs_clk;

	u32 data_rate;

	unsigned long mode_flags;
	enum mipi_dsi_pixel_format format;
	unsigned int lanes;
	struct videomode vm;
	int clk_refcnt;
	bool output_en;
	bool doze_enabled;
	u32 irq_data;
	wait_queue_head_t irq_wait_queue;
	struct mtk_dsi_driver_data *driver_data;

	struct t_condition_wq enter_ulps_done;
	struct t_condition_wq exit_ulps_done;
	struct t_condition_wq te_rdy;
	struct t_condition_wq frame_done;
	unsigned int hs_trail;
	unsigned int hs_prpr;
	unsigned int hs_zero;
	unsigned int lpx;
	unsigned int ta_get;
	unsigned int ta_sure;
	unsigned int ta_go;
	unsigned int da_hs_exit;
	unsigned int cont_det;
	unsigned int clk_zero;
	unsigned int clk_hs_prpr;
	unsigned int clk_hs_exit;
	unsigned int clk_hs_post;

	unsigned int vsa;
	unsigned int vbp;
	unsigned int vfp;
	unsigned int hsa_byte;
	unsigned int hbp_byte;
	unsigned int hfp_byte;

	bool mipi_hopping_sta;
	bool panel_osc_hopping_sta;

	bool fod_backlight_flag;
	bool fod_hbm_flag;
	bool normal_hbm_flag;
	bool dc_flag;
	bool doze_flag;

	int  doze_state;
	struct mutex dsi_lock;

	unsigned int data_phy_cycle;
	/* for Panel Master dcs read/write */
	struct mipi_dsi_device *dev_for_PM;
	struct mi_dsi_panel_cfg mi_cfg;
};


/* Enter/Exit DC_LUT info */
struct dc_cfg {
	bool read_done;
	bool update_done;
	u32 update_d2_index;
	u8 enter_dc_lut[75];
	u8 exit_dc_lut[75];
};

struct greenish_gamma_cfg {
	u32 index_1st_param;
	u32 index_2nd_param;
	u32 index_3rd_param;
	u32 index_4th_param;
	u32 index_5th_param;
	u32 index_6th_param;

	u32 greenish_gamma_update_offset;
	u32 greenish_gamma_update_param_count;

	bool gamma_update_done;
};

typedef struct brightness_alpha {
	uint32_t brightness;
	uint32_t alpha;
} brightness_alpha;

#define to_mtk_dsi(x)  container_of(x, struct mtk_dsi, conn)

bool dsi_panel_initialized(struct drm_panel *panel);
struct mtk_panel_ext *mtk_dsi_get_panel_ext(struct mtk_ddp_comp *comp);
void display_utc_time_marker(char *annotation);
ssize_t dsi_panel_write_mipi_reg(char *buf, size_t count);
ssize_t  dsi_panel_read_mipi_reg(char *buf);


ssize_t led_i2c_reg_write(struct drm_connector *connector, char *buf, size_t count);
ssize_t led_i2c_reg_read(struct drm_connector *connector, char *buf);

ssize_t dsi_display_get_panel_info(struct drm_connector *connector, char *buf);
ssize_t dsi_panel_set_disp_param(struct drm_connector* connector, u32 param);
ssize_t dsi_panel_get_disp_param(char *buf);
ssize_t dsi_display_get_dynamic_fps(struct drm_connector *connector, char *buf);
ssize_t dsi_display_set_doze_brightness(struct drm_connector *connector,
			int doze_brightness);

int mi_dsi_panel_get_fps(struct mtk_dsi *dsi,
			u32 *fps);

int mi_dsi_panel_get_panel_info(struct mtk_dsi *dsi, char *buf);

int mi_dsi_panel_set_disp_param(struct mtk_dsi *dsi, struct disp_feature_ctl *ctl);

ssize_t mi_dsi_panel_get_disp_param(struct mtk_dsi *dsi,
			char *buf, size_t size);

int mi_dsi_panel_set_doze_brightness(struct mtk_dsi *dsi,
			int doze_brightness);
int mi_dsi_panel_get_doze_brightness(struct mtk_dsi *dsi,
			u32 *doze_brightness);
int mi_dsi_panel_set_brightness(struct mtk_dsi *dsi,
			int brightness);
int mi_dsi_panel_get_brightness(struct mtk_dsi *dsi,
			u32 *brightness);
int mi_dsi_panel_set_disp_param(struct mtk_dsi *dsi, struct disp_feature_ctl *ctl);

ssize_t mi_dsi_panel_write_mipi_reg(char *buf);

ssize_t  mi_dsi_panel_read_mipi_reg(char *buf);

int mi_dsi_panel_write_dsi_cmd(struct dsi_cmd_rw_ctl *ctl);

bool is_backlight_set_skip(struct mtk_dsi *dsi, u32 bl_lvl);

int mi_dsi_panel_get_max_brightness_clone(struct mtk_dsi *dsi,
			u32 *max_brightness_clone);

void mi_disp_cfg_init(struct mtk_dsi *dsi);

#endif /* _DSI_PANEL_MI_H_ */
