/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __MTK_PANEL_EXT_H__
#define __MTK_PANEL_EXT_H__

#include <drm/drm_panel.h>

#define RT_MAX_NUM 10
#define ESD_CHECK_NUM 3
#define MAX_TX_CMD_NUM 20
#define MAX_RX_CMD_NUM 20
#define READ_DDIC_SLOT_NUM (4 * MAX_RX_CMD_NUM)
#define MAX_DYN_CMD_NUM 20


struct mtk_dsi;
struct cmdq_pkt;
struct mtk_panel_para_table {
	u8 count;
	u8 para_list[64];
};

/*
 *	DSI data type:
 *	DSI_DCS_WRITE_SHORT_PACKET_NO_PARAM		0x05
 *	DSI_DCS_WRITE_SHORT_PACKET_1_PARAM		0x15
 *	DSI_DCS_WRITE_LONG_PACKET					0x39
 *	DSI_DCS_READ_NO_PARAM						0x06

 *	DSI_GERNERIC_WRITE_SHORT_NO_PARAM			0x03
 *	DSI_GERNERIC_WRITE_SHORT_1_PARAM			0x13
 *	DSI_GERNERIC_WRITE_SHORT_1_PARAM			0x23
 *	DSI_GERNERIC_WRITE_LONG_PACKET				0x29
 *	DSI_GERNERIC_READ_NO_PARAM					0x04
 *	DSI_GERNERIC_READ_1_PARAM					0x14
 *	DSI_GERNERIC_READ_2_PARAM					0x24
 */

/**
 * struct mtk_ddic_dsi_msg - MTK write/read DDIC RG cmd buffer
 * @channel: virtual channel id
 * @flags: flags controlling this message transmission
 * @type: payload data type array
 * @tx_len: length of @tx_buf
 * @tx_buf: data array to be written
 * @tx_cmd_num: tx cmd number
 * @rx_len: length of @rx_buf
 * @rx_buf: data array to be read, or NULL
 * @rx_cmd_num: rx cmd number
 */
struct mtk_ddic_dsi_msg {
	u8 channel;
	u16 flags;

	u8 type[MAX_TX_CMD_NUM];
	size_t tx_len[MAX_TX_CMD_NUM];
	const void *tx_buf[MAX_TX_CMD_NUM];
	size_t tx_cmd_num;

	size_t rx_len[MAX_RX_CMD_NUM];
	void *rx_buf[MAX_RX_CMD_NUM];
	size_t rx_cmd_num;
};

struct DSI_RX_DATA_REG {
	unsigned char byte0;
	unsigned char byte1;
	unsigned char byte2;
	unsigned char byte3;
};

typedef void (*dcs_write_gce) (struct mtk_dsi *dsi, struct cmdq_pkt *handle,
				const void *data, size_t len);
typedef void (*dcs_grp_write_gce) (struct mtk_dsi *dsi, struct cmdq_pkt *handle,
				struct mtk_panel_para_table *para_table,
				unsigned int para_size);
typedef int (*panel_tch_rst) (void);

enum MTK_PANEL_OUTPUT_MODE {
	MTK_PANEL_SINGLE_PORT = 0x0,
	MTK_PANEL_DSC_SINGLE_PORT,
	MTK_PANEL_DUAL_PORT,
};

struct esd_check_item {
	unsigned char cmd;
	unsigned char count;
	unsigned char para_list[RT_MAX_NUM];
	unsigned char mask_list[RT_MAX_NUM];
};

enum MTK_PANEL_MODE_SWITCH_STAGE {
	BEFORE_DSI_POWERDOWN,
	AFTER_DSI_POWERON,
};

enum MIPITX_PHY_PORT {
	MIPITX_PHY_PORT_0 = 0,
	MIPITX_PHY_PORT_1,
	MIPITX_PHY_PORT_NUM
};

enum MIPITX_PHY_LANE_SWAP {
	MIPITX_PHY_LANE_0 = 0,
	MIPITX_PHY_LANE_1,
	MIPITX_PHY_LANE_2,
	MIPITX_PHY_LANE_3,
	MIPITX_PHY_LANE_CK,
	MIPITX_PHY_LANE_RX,
	MIPITX_PHY_LANE_NUM
};

enum FPS_CHANGE_INDEX {
	DYNFPS_NOT_DEFINED = 0,
	DYNFPS_DSI_VFP = 1,
	DYNFPS_DSI_HFP = 2,
	DYNFPS_DSI_MIPI_CLK = 4,
};

struct mtk_panel_dsc_params {
	unsigned int enable;
	unsigned int ver; /* [7:4] major [3:0] minor */
	unsigned int slice_mode;
	unsigned int rgb_swap;
	unsigned int dsc_cfg;
	unsigned int rct_on;
	unsigned int bit_per_channel;
	unsigned int dsc_line_buf_depth;
	unsigned int bp_enable;
	unsigned int bit_per_pixel;
	unsigned int pic_height; /* need to check */
	unsigned int pic_width;  /* need to check */
	unsigned int slice_height;
	unsigned int slice_width;
	unsigned int chunk_size;
	unsigned int xmit_delay;
	unsigned int dec_delay;
	unsigned int scale_value;
	unsigned int increment_interval;
	unsigned int decrement_interval;
	unsigned int line_bpg_offset;
	unsigned int nfl_bpg_offset;
	unsigned int slice_bpg_offset;
	unsigned int initial_offset;
	unsigned int final_offset;
	unsigned int flatness_minqp;
	unsigned int flatness_maxqp;
	unsigned int rc_model_size;
	unsigned int rc_edge_factor;
	unsigned int rc_quant_incr_limit0;
	unsigned int rc_quant_incr_limit1;
	unsigned int rc_tgt_offset_hi;
	unsigned int rc_tgt_offset_lo;
};
struct mtk_dsi_phy_timcon {
	unsigned int hs_trail;
	unsigned int hs_prpr;
	unsigned int hs_zero;
	unsigned int lpx;
	unsigned int ta_get;
	unsigned int ta_sure;
	unsigned int ta_go;
	unsigned int da_hs_exit;
	unsigned int clk_trail;
	unsigned int cont_det;
	unsigned int da_hs_sync;
	unsigned int clk_zero;
	unsigned int clk_hs_prpr;
	unsigned int clk_hs_exit;
	unsigned int clk_hs_post;
};

struct dynamic_mipi_params {
	unsigned int switch_en;
	unsigned int pll_clk;
	unsigned int data_rate;

	unsigned int vsa;
	unsigned int vbp;
	unsigned int vfp;
	unsigned int vfp_lp_dyn;

	unsigned int hsa;
	unsigned int hbp;
	unsigned int hfp;
};

struct dfps_switch_cmd {
	unsigned int src_fps;
	unsigned int cmd_num;
	unsigned char para_list[64];
};

struct dynamic_fps_params {
	unsigned int switch_en;
	unsigned int vact_timing_fps;
	unsigned int data_rate;
	struct dfps_switch_cmd dfps_cmd_table[MAX_DYN_CMD_NUM];
};

struct mtk_panel_params {
	unsigned int pll_clk;
	unsigned int data_rate;
	struct mtk_dsi_phy_timcon phy_timcon;
	unsigned int vfp_low_power;
	struct dynamic_mipi_params dyn;
	struct dynamic_fps_params dyn_fps;
	unsigned int cust_esd_check;
	unsigned int esd_check_enable;
	struct esd_check_item lcm_esd_check_table[ESD_CHECK_NUM];
	unsigned int ssc_disable;
	unsigned int ssc_range;
	int lcm_color_mode;
	unsigned int min_luminance;
	unsigned int average_luminance;
	unsigned int max_luminance;
	unsigned int round_corner_en;
	unsigned int corner_pattern_height;
	unsigned int corner_pattern_height_bot;
	unsigned int corner_pattern_tp_size;
	unsigned int corner_pattern_tp_size_l;
	unsigned int corner_pattern_tp_size_r;
	void *corner_pattern_lt_addr;
	void *corner_pattern_lt_addr_l;
	void *corner_pattern_lt_addr_r;
	unsigned int physical_width_um;
	unsigned int physical_height_um;
	unsigned int lane_swap_en;
	unsigned int is_cphy;
	enum MIPITX_PHY_LANE_SWAP
		lane_swap[MIPITX_PHY_PORT_NUM][MIPITX_PHY_LANE_NUM];
	struct mtk_panel_dsc_params dsc_params;
	unsigned int output_mode;
	unsigned int lcm_cmd_if;
	unsigned int hbm_en_time;
	unsigned int hbm_dis_time;
	unsigned int lcm_index;
	unsigned int wait_sof_before_dec_vfp;
	unsigned int doze_delay;

	//Settings for LFR Function:
	unsigned int lfr_enable;
	unsigned int lfr_minimum_fps;
};

struct mtk_panel_ext {
	struct mtk_panel_funcs *funcs;
	struct mtk_panel_params *params;
};

struct mtk_panel_ctx {
	struct drm_panel *panel;
	struct mtk_panel_ext *ext;

	struct list_head list;
};

struct mtk_panel_funcs {
	int (*set_backlight_cmdq)(void *dsi_drv, dcs_write_gce cb,
		void *handle, unsigned int level);
	int (*set_aod_light_mode)(void *dsi_drv, dcs_write_gce cb,
		void *handle, unsigned int mode);
	int (*set_backlight_grp_cmdq)(void *dsi_drv, dcs_grp_write_gce cb,
		void *handle, unsigned int level);
	int (*reset)(struct drm_panel *panel, int on);
	int (*ata_check)(struct drm_panel *panel);
	int (*ext_param_set)(struct drm_panel *panel, unsigned int mode);
	int (*ext_param_get)(struct mtk_panel_params *ext_para,
		unsigned int mode);
	int (*mode_switch)(struct drm_panel *panel, unsigned int cur_mode,
		unsigned int dst_mode, enum MTK_PANEL_MODE_SWITCH_STAGE stage);
	int (*get_virtual_heigh)(void);
	int (*get_virtual_width)(void);
	/**
	 * @doze_enable_start:
	 *
	 * Call the @doze_enable_start before starting AOD mode.
	 * The LCM off may add here to avoid panel show unexpected
	 * content when switching to specific panel low power mode.
	 */
	int (*doze_enable_start)(struct drm_panel *panel,
		void *dsi_drv, dcs_write_gce cb, void *handle);

	/**
	 * @doze_enable:
	 *
	 * Call the @doze_enable starts AOD mode.
	 */
	int (*doze_enable)(struct drm_panel *panel,
		void *dsi_drv, dcs_write_gce cb, void *handle);

	/**
	 * @doze_disable:
	 *
	 * Call the @doze_disable before ending AOD mode.
	 */
	int (*doze_disable)(struct drm_panel *panel,
		void *dsi_drv, dcs_write_gce cb, void *handle);

	/**
	 * @doze_post_disp_on:
	 *
	 * In some situation, the LCM off may set in @doze_enable & @disable.
	 * After LCM switch to the new mode stable, system call
	 * @doze_post_disp_on to turn on panel.
	 */
	int (*doze_post_disp_on)(struct drm_panel *panel,
		void *dsi_drv, dcs_write_gce cb, void *handle);

	/**
	 * @doze_area:
	 *
	 * Send the panel area in command here.
	 */
	int (*doze_area)(struct drm_panel *panel,
		void *dsi_drv, dcs_write_gce cb, void *handle);

	/**
	 * @doze_get_mode_flags:
	 *
	 * If CV switch is needed for doze mode, fill the mode_flags in this
	 * function for both CMD and VDO mode.
	 */
	unsigned long (*doze_get_mode_flags)(
		struct drm_panel *panel, int aod_en);

	int (*hbm_set_cmdq)(struct drm_panel *panel, void *dsi_drv,
			    dcs_write_gce cb, void *handle, bool en);
	void (*hbm_get_state)(struct drm_panel *panel, bool *state);
	void (*hbm_get_wait_state)(struct drm_panel *panel, bool *wait);
	bool (*hbm_set_wait_state)(struct drm_panel *panel, bool wait);
};

void mtk_panel_init(struct mtk_panel_ctx *ctx);
void mtk_panel_add(struct mtk_panel_ctx *ctx);
void mtk_panel_remove(struct mtk_panel_ctx *ctx);
int mtk_panel_attach(struct mtk_panel_ctx *ctx, struct drm_panel *panel);
int mtk_panel_detach(struct mtk_panel_ctx *ctx);
struct mtk_panel_ext *find_panel_ext(struct drm_panel *panel);
int mtk_panel_ext_create(struct device *dev,
			 struct mtk_panel_params *ext_params,
			 struct mtk_panel_funcs *ext_funcs,
			 struct drm_panel *panel);
int mtk_panel_tch_handle_reg(struct drm_panel *panel);
void **mtk_panel_tch_handle_init(void);
int mtk_panel_tch_rst(struct drm_panel *panel);

#endif
