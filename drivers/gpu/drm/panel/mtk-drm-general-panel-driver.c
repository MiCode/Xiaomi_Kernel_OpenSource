// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/backlight.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include <drm/drm_modes.h>
#include <linux/delay.h>
#include <drm/drm_connector.h>
#include <drm/drm_device.h>

#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_graph.h>
#include <linux/platform_device.h>

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mediatek_v2/mtk_panel_ext.h"
#include "../mediatek/mediatek_v2/mtk_drm_graphics_base.h"
#endif

#include "../mediatek/mediatek_v2/mtk_corner_pattern/mtk_data_hw_roundedpattern.h"

#include "../mediatek/mediatek_v2/mtk_dsi.h"

#include "../../../misc/mediatek/gate_ic/gate_i2c.h"

//#include "ktz8866.h"

/* enable this to check panel self -bist pattern */
/* #define PANEL_BIST_PATTERN */
/****************TPS65132***********/
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
//#include "lcm_i2c.h"

#define SUB_PATH_PANEL_NODE_NAME				"sub-path-panel"
#define LCM_INIT_CMD_NODE_NAME					"lcm-init-cmd"
#define LCM_DEINIT_CMD_NODE_NAME				"lcm-deinit-cmd"
#define BACKLGHT_MAX_LEVEL_NODE_NAME			"backlight-max-level"
#define BACKLIGHT_NODE_NAME						"backlight-setting-cmd"
#define LCM_DSI_LANE_NUM_NODE_NAME				"lcm-dsi-lane-num"
#define LCM_DSI_FORMAT_NODE_NAME				"lcm-dsi-format"
#define LCM_DSI_MODE_NODE_NAME					"lcm-dsi-modeflag"

#define EXT_PARAM_NODE_NAME						"panel-ext-param-setting"
#define EXT_PARAM_DATA_RATE_NODE_NAME			"ext-param-data_rate_khz"
#define EXT_PARAM_SSC_ENABLE_NODE_NAME			"ext-param-ssc_enable"
#define EXT_PARAM_BDG_SSC_ENABLE_NODE_NAME		"ext-param-bdg_ssc_enable"
#define EXT_PARAM_SSC_RANGE_NODE_NAME			"ext-param-ssc_range"
#define EXT_PARAM_LCM_COLOR_MODE_NODE_NAME		"ext-param-lcm_color_mode"
#define EXT_PARAM_MIN_LUM_NODE_NAME				"ext-param-min_luminance"
#define EXT_PARAM_AVER_LUM_NODE_NAME			"ext-param-average_luminance"
#define EXT_PARAM_MAX_LUM_NODE_NAME				"ext-param-max_luminance"
#define EXT_PARAM_RND_COR_EN_NODE_NAME			"ext-param-round_corner_en"
#define EXT_PARAM_COR_PAT_H_NODE_NAME			"ext-param-corner_pattern_height"
#define EXT_PARAM_COR_PAT_H_BOT_NODE_NAME		"ext-param-corner_pattern_height_bot"
#define EXT_PARAM_COR_PAT_TP_SIZE_NODE_NAME		"ext-param-corner_pattern_tp_size"
#define EXT_PARAM_COR_PAT_TP_SIZE_L_NODE_NAME	"ext-param-corner_pattern_tp_size_l"
#define EXT_PARAM_COR_PAT_TP_SIZE_R_NODE_NAME	"ext-param-corner_pattern_tp_size_r"
#define EXT_PARAM_COR_PAT_LT_ADDR_NODE_NAME		"ext-param-corner_pattern_lt_addr"
#define EXT_PARAM_COR_PAT_LT_ADDR_L_NODE_NAME	"ext-param-corner_pattern_lt_addr_l"
#define EXT_PARAM_COR_PAT_LT_ADDR_R_NODE_NAME	"ext-param-corner_pattern_lt_addr_r"
#define EXT_PARAM_PHY_W_UM_NODE_NAME			"ext-param-physical_width_um"
#define EXT_PARAM_PHY_H_UM_NODE_NAME			"ext-param-physical_height_um"

#define EXT_PARAM_PHY_TIM_NODE_NAME				"ext-param-phy_timcon_setting"
#define PHY_TIM_HS_TRAIL_NODE_NAME				"phy_timcon-hs_trail"
#define PHY_TIM_HS_PRPR_LNODE_NAME				"phy_timcon-hs_prpr"
#define PHY_TIM_HS_ZERO_NODE_NAME				"phy_timcon-hs_zero"
#define PHY_TIM_LPX_NODE_NAME					"phy_timcon-lpx"
#define PHY_TIM_TA_GET_NODE_NAME				"phy_timcon-ta_get"
#define PHY_TIM_TA_SURE_NODE_NAME				"phy_timcon-ta_sure"
#define PHY_TIM_TA_GO_NODE_NAME					"phy_timcon-ta_go"
#define PHY_TIM_DA_HS_EXIT_NODE_NAME			"phy_timcon-da_hs_exit"
#define PHY_TIM_CLK_TRAIL_NODE_NAME				"phy_timcon-clk_trail"
#define PHY_TIM_CONT_DET_NODE_NAME				"phy_timcon-cont_det"
#define PHY_TIM_DA_HS_SYNC_NODE_NAME			"phy_timcon-da_hs_sync"
#define PHY_TIM_CLK_ZERO_NODE_NAME				"phy_timcon-clk_zero"
#define PHY_TIM_CLK_HS_PRPR_NODE_NAME			"phy_timcon-clk_hs_prpr"
#define PHY_TIM_CLK_HS_EXIT_NODE_NAME			"phy_timcon-clk_hs_exit"
#define PHY_TIM_CLK_HS_POST_NODE_NAME			"phy_timcon-clk_hs_post"

#define EXT_PARAM_DYN_MIPI_PARAM_NODE_NAME		"ext-param-dyn_setting"
#define DYN_SWITCH_EN_NODE_NAME					"dyn-switch_en"
#define DYN_PLL_CLK_NODE_NAME					"dyn-pll_clk"
#define DYN_DATA_RATE_NODE_NAME					"dyn-data_rate"
#define DYN_VSA_NODE_NAME						"dyn-vsa"
#define DYN_VBP_NODE_NAME						"dyn-vbp"
#define DYN_VFP_NODE_NAME						"dyn-vfp"
#define DYN_VFP_LP_DYN_NODE_NAME				"dyn-vfp_lp_dyn"
#define DYN_HSA_NODE_NAME						"dyn-hsa"
#define DYN_HFP_NODE_NAME						"dyn-hfp"
#define DYN_HBP_NODE_NAME						"dyn-hbp"
#define DYN_MAX_VFP_FOR_MSYNC_DYN_NODE_NAME		"dyn-max_vfp_for_msync_dyn"

#define MAX_DYN_FPS_CMD_TABLE_NUM				8

#define EXT_PARAM_DYN_FPS_PARAM_NODE_NAME		"ext-param-dyn_fps_setting"
#define DYN_FPS_SWITCH_EN_NODE_NAME				"dyn_fps-switch_en"
#define DYN_FPS_VACT_TIMING_FPS_NODE_NAME		"dyn_fps-vact_timing_fps"
#define DYN_FPS_DATA_RATE_NODE_NAME				"dyn_fps-data_rate"
#define DYN_FPS_SWITC_CMD_0_NODE_NAME			"dyn_fps-switch_cmd_0"

#define EXT_PARAM_IS_CPHY_NODE_NAME				"ext-param-is_cphy"
#define EXT_PARAM_OUTPUT_MODE_NODE_NAME			"ext-param-output_mode"
#define EXT_PARAM_LCM_CMD_IF_NODE_NAME			"ext-param-lcm_cmd_if"
#define EXT_PARAM_HBM_EN_TIME_NODE_NAME			"ext-param-hbm_en_time"
#define EXT_PARAM_HBM_DIS_TIME_NODE_NAME		"ext-param-hbm_dis_time"
#define EXT_PARAM_LCM_INDEX_NODE_NAME			"ext-param-lcm_index"
#define EXT_PARAM_WAIT_SOF_BEFORE_DEC_VFP_NODE_NAME "ext-param-wait_sof_before_dec_vfp"
#define EXT_PARAM_DOZE_DELAY_NODE_NAME			"ext-param-doze_delay"
#define EXT_PARAM_LFR_ENABLE_NODE_NAME			"ext-param-lfr_enable"
#define EXT_PARAM_LFR_MINIMUM_FPS_NODE_NAME		"ext-param-lfr_minimum_fps"
#define EXT_PARAM_VFP_LP_NODE_NAME				"ext-param-vfp_lp"
#define EXT_PARAM_ESD_CHECK_EN_NODE_NAME		"ext-param-esd_check_enable"
#define EXT_PARAM_CUST_ESD_CHECK_NODE_NAME		"ext-param-cust_esd_check"
#define EXT_PARAM_CUST_ESD_CHECK_TABLE_NODE_NAME "ext-param-lcm_esd_check_table"
#define EXT_PARAM_LANE_SWAP_NODE_NAME			"ext-param-lane_swap_en"
#define EXT_PARAM_BDG_LANE_SWAP_NODE_NAME		"ext-param-bdg_lane_swap_en"
#define EXT_PARAM_LANE_SWAP_DATA_NODE_NAME		"ext-param-lane_swap_data"
#define EXT_PARAM_CPHY_LANE_SWAP_DATA_NODE_NAME	"ext-param-cphy_lane_swap_data"
#define EXT_PARAM_DSC_ENABLE_NODE_NAME			"ext-param-dsc_enable"
#define EXT_PARAM_BDG_DSC_ENABLE_NODE_NAME		"ext-param-bdg_dsc_enable"
#define EXT_PARAM_DSC_SLICE_MODE_NODE_NAME		"ext-param-dsc_slice_mode"
#define EXT_PARAM_DSC_PPS_NODE_NAME				"ext-param-dsc_pps"
#define EXT_PARAM_SET_AREA_TRIG_NODE_NAME		"ext-param-set_area_before_trigger"
#define EXT_PARAM_CMD_NULLPKT_LEN_NODE_NAME		"ext-param-cmd_null_pkt_len"
#define EXT_PARAM_CMD_NULLPKT_EN_NODE_NAME		"ext-param-cmd_null_pkt_en"
#define EXT_PARAM_LP_PERLINE_NODE_NAME			"ext-param-lp_perline_en"
#define EXT_PARAM_CHANG_FPS_VFP_SEND_CMD_NODE_NAME "ext-param-change_fps_vfp_send_cmd"
#define EXT_PARAM_VDO_PER_FRAME_LP_NODE_NAME	"ext-param-vdo_per_frame_lp_enable"

#define EXT_PARAM_DUAL_DSC_ENABLE_NODE_NAME		"ext-param-dual_dsc_enable"
#define EXT_PARAM_SKIP_UNNECESSARY_SWITCH_NODE_NAME "ext-param-skip_unnecessary_switch"
#define EXT_PARAM_PANEL_ROTATE_NODE_NAME		"ext-param-panel_rotate"
#define EXT_PARAM_SPR_OUT_MOD_NODE_NAME			"ext-param-spr_out_put_mode"
#define EXT_PARAM_SEND_TO_DDIC_NODE_NAME		"ext-param-send_cmd_to_ddic"

#define PANEL_MODE_NODE_NAME					"panel-mode-setting"
#define PANEL_WIDHT_NODE_NAME					"panel-mode-width"
#define PANEL_HEIGHT_NODE_NAME					"panel-mode-height"
#define PANEL_HFP_NODE_NAME						"panel-mode-hfp"
#define PANEL_HSA_NODE_NAME						"panel-mode-hsa"
#define PANEL_HBP_NODE_NAME						"panel-mode-hbp"
#define PANEL_VSA_NODE_NAME						"panel-mode-vsa"
#define PANEL_VBP_NODE_NAME						"panel-mode-vbp"
#define PANEL_VFP_NODE_NAME						"panel-mode-vfp"
#define PANEL_CLOCK_NODE_NAME					"panel-mode-clock"
#define PANEL_VREFRESH_NODE_NAME				"panel-mode-vrefresh"
#define PANEL_WIDTH_MM_NODE_NAME				"panel-mode-width_mm"
#define PANEL_HEIGHT_MM_NODE_NAME				"panel-mode-height_mm"

#define POWER_SUPPLY_NODE_NAME					"panel-pwr-supply"
#define POWER_NAME_NODE_NAME					"pwr-name"
#define POWER_CTRL_MODE							"pwr-ctrl-mode"
#define POWER_TYPE_GPIO							"gpio"
#define POWER_GPIO_HIGH_LEVEL					"pwr-high-level"
#define POWER_TYPE_PMIC							"pmic"
#define POWER_TYPE_REG							"regulator"
#define POWER_REG_SET_VOL						"pwr-set-voltage"
#define POWER_DELAY_TIME						"delay_ms"

#define RESET_GPIO_NODE_NAME					"reset"
#define RESET_SEQUENCE_NODE_NAME				"reset-sequence"

#define MAX_PWR_NODE_NAME_LEN					32
#define MAX_NAME_LEN							32

#define MAX_PWR_COUNT							4
#define MAX_RST_COUNT							8
#define MAX_EXT_PARAM_COUNT						4
#define MAX_CMD_DATA_LEN						128
#define MAX_BL_CMD_COUNT						4
#define MAX_DEINIT_CMD_COUNT					8
#define MAX_INIT_CMD_COUNT						512

struct dsi_setting {
	u32 lane_num;
	u32 format;
	u32 mode_flag;
};

struct ddic_cmd {
	u8 cmd_id;	// dsc data type
	u8 last;	// indicate this is an individual packet
	u8 vc;		// virtual channel number
	u8 ack;		// expect ack from client
	u8 delay_ms;// delay after send cmd
	u16 len;	// payload len
	u8 data[MAX_CMD_DATA_LEN];
};

enum PWR_MODE {
	PWR_MODE_REG = 0,
	PWR_MODE_GPIO,
	PWR_MODE_PMIC
};

struct lcm_power {
	char name[32];
	enum PWR_MODE mode;

	struct regulator *pwr_reg;
	u32 set_vol;
	struct gpio_desc *pwr_gpio;
	u32 gpio_high_level;

	u32 delay_ms;
};

struct reset_seq {
	u32 level;
	u32 delay_ms;
};

struct tag_videolfb {
	u64 fb_base;
	u32 islcmfound;
	u32 fps;
	u32 vram;
	char lcmname[32];
};


struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;

	u32 lcm_pwr_num;
	u32 bl_cmd_num;
	u32 bl_max_level;
	u32 init_cmd_num;
	u32 deinit_cmd_num;
	u32 ext_params_num;
	u32 disp_mode_num;
	u32 reset_seq_num;

	struct ddic_cmd lcm_init_cmd[MAX_INIT_CMD_COUNT];
	struct lcm_power lcm_pwr[MAX_PWR_COUNT];
	struct ddic_cmd lcm_bl_cmd[MAX_BL_CMD_COUNT];
	struct ddic_cmd lcm_deinit_cmd[MAX_DEINIT_CMD_COUNT];
	struct mtk_panel_params lcm_ext_params[MAX_EXT_PARAM_COUNT];
	struct drm_display_mode lcm_disp_mode[MAX_EXT_PARAM_COUNT];

	struct gpio_desc *reset_gpio;
	struct reset_seq rst_seq[MAX_RST_COUNT];
	struct dsi_setting dsi_set;

	bool prepared;
	bool enabled;

	int error;
};

static struct device_node *of_get_next_node_by_name
	(struct device_node *parent_node, struct device_node *prev_node, char *name)
{
	struct device_node *next_node = NULL;

	if (!parent_node) {
		pr_info("%s: error: parent NULL!\n", __func__);
		return NULL;
	}

	if (!prev_node) {
		next_node = of_get_child_by_name(parent_node, name);
	} else {
		parent_node = of_get_parent(prev_node);
		if (!parent_node) {
			pr_info("%s: error: get parent NULL!\n", __func__);
			return NULL;
		}
		do {
			next_node = of_get_next_child(parent_node, prev_node);
			if (!next_node) {
				pr_info("%s: next_node NULL!\n", __func__);
				of_node_put(parent_node);
				return NULL;
			}
			prev_node = next_node;
		} while (!of_node_name_eq(next_node, name));

		of_node_put(parent_node);
	}
	return next_node;
}

static int lcm_dsi_write(struct lcm *ctx, struct ddic_cmd cmd)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret = 0;

	if (ctx->error < 0)
		return -1;

	if (cmd.len > 128) {
		pr_info("%s: error: wrong cmd[0x%x], len=0x%x!\n", __func__, cmd.cmd_id, cmd.len);
		return -1;
	}
	//pr_info("cmd:[0x%x][0x%x][0x%x][0x%x][0x%x][0x%x][0x%x][0x%x]\n",
	//	cmd.cmd_id, cmd.last, cmd.vc, cmd.ack, cmd.delay_ms,
	//	cmd.len, cmd.data[0], cmd.data[1]);

	if (cmd.cmd_id == MIPI_DSI_DCS_SHORT_WRITE ||
		cmd.cmd_id == MIPI_DSI_DCS_SHORT_WRITE_PARAM ||
		cmd.cmd_id == MIPI_DSI_DCS_LONG_WRITE)
		ret = mipi_dsi_dcs_write_buffer(dsi, cmd.data, cmd.len);
	else if (cmd.cmd_id == MIPI_DSI_GENERIC_SHORT_WRITE_0_PARAM ||
		cmd.cmd_id == MIPI_DSI_GENERIC_SHORT_WRITE_1_PARAM ||
		cmd.cmd_id == MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM ||
		cmd.cmd_id == MIPI_DSI_GENERIC_LONG_WRITE)
		ret = mipi_dsi_generic_write(dsi, cmd.data, cmd.len);
	else if (cmd.cmd_id == MIPI_DSI_PICTURE_PARAMETER_SET) {
		if (cmd.len != 128) {
			pr_info("%s: error: wrong cmd[0x%x],len=0x%x!\n", cmd.cmd_id, cmd.len);
			return -1;
		}
		ret = mipi_dsi_picture_parameter_set(dsi,
			(struct drm_dsc_picture_parameter_set *)&cmd.data);
	} else {
		pr_info("%s: error: wrong cmd[0x%x],len=0x%x!\n", cmd.cmd_id, cmd.len);
		ctx->error = ret;
		return -1;
	}

	if (cmd.delay_ms)
		mdelay(cmd.delay_ms);

	if (ret < 0) {
		pr_info("error:  %zd writing seq: %ph\n", ret, cmd.data);
		ctx->error = ret;
		return ret;
	}

	return 0;
}

static int lcm_get_init_cmd_from_dts(struct lcm *ctx)
{
	u32 len = 0, ret = 0, i = 0;
	u8 *array;

	pr_info("%s +\n", __func__);
	ctx->init_cmd_num = 0;

	if (!of_get_property(ctx->dev->of_node, LCM_INIT_CMD_NODE_NAME, &len)) {
		pr_info("%s: get ctx->lcm_init_cmd fail!\n", __func__);
		return -1;
	}

	array = kzalloc(len, GFP_KERNEL);
	if (!array)
		return -1;

	ret = of_property_read_u8_array(ctx->dev->of_node, LCM_INIT_CMD_NODE_NAME, array, len);
	if (ret) {
		pr_info("%s:error: read init cmd fail!\n", __func__);
		return -1;
	}

	for (i = 0; i < len;) {
		//memset((void *)&cmd, 0 , sizeof(struct ddic_cmd));
		ctx->lcm_init_cmd[ctx->init_cmd_num].cmd_id = array[i];
		ctx->lcm_init_cmd[ctx->init_cmd_num].last = array[i + 1];
		ctx->lcm_init_cmd[ctx->init_cmd_num].vc = array[i + 2];
		ctx->lcm_init_cmd[ctx->init_cmd_num].ack = array[i + 3];
		ctx->lcm_init_cmd[ctx->init_cmd_num].delay_ms = array[i + 4];
		ctx->lcm_init_cmd[ctx->init_cmd_num].len = (array[i + 5] << 8) + array[i + 6];
		memcpy((void *)ctx->lcm_init_cmd[ctx->init_cmd_num].data, (void *)&array[i + 7],
			ctx->lcm_init_cmd[ctx->init_cmd_num].len);
		i += ctx->lcm_init_cmd[ctx->init_cmd_num].len + 7;
		ctx->init_cmd_num++;
	}
	kfree(array);

	pr_info("%s -\n", __func__);
	return 0;
}

static int lcm_get_deinit_cmd_from_dts(struct lcm *ctx)
{
	u32 len = 0, ret = 0, i = 0;
	u8 *array;

	pr_info("%s +\n", __func__);

	if (!of_get_property(ctx->dev->of_node, LCM_DEINIT_CMD_NODE_NAME, &len)) {
		pr_info("error: get ctx->lcm_deinit_cmd fail!\n");
		return -1;
	}

	array = kzalloc(len, GFP_KERNEL);
	if (!array)
		return -1;

	ret = of_property_read_u8_array(ctx->dev->of_node, LCM_DEINIT_CMD_NODE_NAME, array, len);
	if (ret) {
		pr_info("error: read deinit fail!\n");
		return -1;
	}

	ctx->deinit_cmd_num = 0;
	for (i = 0; i < len;) {
		ctx->lcm_deinit_cmd[ctx->deinit_cmd_num].cmd_id = array[i];
		ctx->lcm_deinit_cmd[ctx->deinit_cmd_num].last = array[i + 1];
		ctx->lcm_deinit_cmd[ctx->deinit_cmd_num].vc = array[i + 2];
		ctx->lcm_deinit_cmd[ctx->deinit_cmd_num].ack = array[i + 3];
		ctx->lcm_deinit_cmd[ctx->deinit_cmd_num].delay_ms = array[i + 4];
		ctx->lcm_deinit_cmd[ctx->deinit_cmd_num].len
			= (array[i + 5] << 8) + array[i + 6];
		memcpy((void *)ctx->lcm_deinit_cmd[ctx->deinit_cmd_num].data,
			(void *)&array[i + 7], ctx->lcm_deinit_cmd[ctx->deinit_cmd_num].len);
		i += ctx->lcm_deinit_cmd[ctx->deinit_cmd_num].len + 7;
		ctx->deinit_cmd_num++;
	}
	kfree(array);

	pr_info("%s -\n", __func__);

	return 0;
}

static int lcm_get_backlight_res_from_dts(struct lcm *ctx)
{
	u32 len = 0, ret = 0, i = 0;
	u32 read_value = 0;
	u8 *array;

	pr_info("%s +\n", __func__);

	ret = of_property_read_u32(ctx->dev->of_node,
		BACKLGHT_MAX_LEVEL_NODE_NAME, &read_value);
	if (!ret) {
		pr_info("max bl level = %d!\n", read_value);
		ctx->bl_max_level = read_value;
	} else {
		pr_info("backlight max level not set, backlight not control by lcm??\n");
		//return -1;
	}

	if (!of_get_property(ctx->dev->of_node, BACKLIGHT_NODE_NAME, &len)) {
		pr_info("backlight-setting-cmd not set!\n");
		return -1;
	}

	if (len != 10) {
		pr_info("error: not support multi backlight level cmd[%d]!\n", len);
		return -1;
	}

	array = kzalloc(len, GFP_KERNEL);
	if (!array)
		return -1;

	ret = of_property_read_u8_array(ctx->dev->of_node, BACKLIGHT_NODE_NAME, array, len);
	if (ret) {
		pr_info("%s:error: read baclight fail!\n", __func__);
		return -1;
	}

	ctx->bl_cmd_num = 0;
	for (i = 0; i < len;) {
		ctx->lcm_bl_cmd[ctx->bl_cmd_num].cmd_id = array[i];
		ctx->lcm_bl_cmd[ctx->bl_cmd_num].last = array[i + 1];
		ctx->lcm_bl_cmd[ctx->bl_cmd_num].vc = array[i + 2];
		ctx->lcm_bl_cmd[ctx->bl_cmd_num].ack = array[i + 3];
		ctx->lcm_bl_cmd[ctx->bl_cmd_num].delay_ms = array[i + 4];
		ctx->lcm_bl_cmd[ctx->bl_cmd_num].len = (array[i + 5] << 8) + array[i + 6];
		memcpy((void *)ctx->lcm_bl_cmd[ctx->bl_cmd_num].data,
			(void *)&array[i + 7], ctx->lcm_bl_cmd[ctx->bl_cmd_num].len);
		i += ctx->lcm_bl_cmd[ctx->bl_cmd_num].len + 7;
		ctx->bl_cmd_num++;
	}

	kfree(array);
	pr_info("%s -\n", __func__);
	return 0;
}

static int lcm_get_panel_ext_param_from_dts(struct lcm *ctx)
{
	u32 ret = 0, j = 0, k = 0, len = 0, w = 0, exp_len = 0;
	u32 read_value = 0;
	u64 long_addr = 0;
	char ext_param_name[] = EXT_PARAM_NODE_NAME;
	u8 *array;
	u32 *array_32;
	char dyn_fps_switch_cmd_name[] = DYN_FPS_SWITC_CMD_0_NODE_NAME;
	struct device_node *ext_node = NULL,
		*phy_timcon_node = NULL, *dyn_node = NULL, *dyn_fps_node = NULL;

	pr_info("%s +\n", __func__);
	ctx->ext_params_num = 0;

	while ((ext_node = of_get_next_node_by_name(ctx->dev->of_node,
		ext_node, ext_param_name))) {
		pr_info("%s find!\n", ext_param_name);

		//data rate setting
		ret = of_property_read_u32(ext_node, EXT_PARAM_DATA_RATE_NODE_NAME, &read_value);
		if (!ret) {
			ctx->lcm_ext_params[ctx->ext_params_num].data_rate_khz = read_value;
			ctx->lcm_ext_params[ctx->ext_params_num].data_rate = read_value / 1000;
			ctx->lcm_ext_params[ctx->ext_params_num].pll_clk = read_value / 2000;
			pr_info("data_rate = %d\n", read_value);
		} else {
			pr_info("error: read data_rate error!\n");
			return -1;
		}

		//phy_timcon
		phy_timcon_node = of_get_next_node_by_name(ext_node,
			NULL, EXT_PARAM_PHY_TIM_NODE_NAME);
		if (!phy_timcon_node) {
			pr_info("phy timcon not set!\n");
			//return -1;
		} else {
			ret = of_property_read_u32(phy_timcon_node,
				PHY_TIM_HS_TRAIL_NODE_NAME, &read_value);
			if (!ret) {
				ctx->lcm_ext_params[ctx->ext_params_num]
					.phy_timcon.hs_trail = read_value;
				pr_info("phy_timcon.hs_trail = %d\n", read_value);
			} else {
				pr_info("phy_timcon.hs_trail not set!\n");
				//return -1;
			}

			ret = of_property_read_u32(phy_timcon_node,
				PHY_TIM_HS_PRPR_LNODE_NAME, &read_value);
			if (!ret) {
				ctx->lcm_ext_params[ctx->ext_params_num]
					.phy_timcon.hs_prpr = read_value;
				pr_info("phy_timcon.hs_prpr = %d\n", read_value);
			} else {
				pr_info("phy_timcon.hs_prpr not set!\n");
				//return -1;
			}
			ret = of_property_read_u32(phy_timcon_node,
				PHY_TIM_HS_ZERO_NODE_NAME, &read_value);
			if (!ret) {
				ctx->lcm_ext_params[ctx->ext_params_num]
					.phy_timcon.hs_zero = read_value;
				pr_info("phy_timcon.hs_zero = %d\n", read_value);
			} else {
				pr_info("phy_timcon.hs_zero not set!\n");
				//return -1;
			}
			ret = of_property_read_u32(phy_timcon_node,
				PHY_TIM_LPX_NODE_NAME, &read_value);
			if (!ret) {
				ctx->lcm_ext_params[ctx->ext_params_num]
					.phy_timcon.lpx = read_value;
				pr_info("phy_timcon.lpx = %d\n", read_value);
			} else {
				pr_info("phy_timcon.lpx not set!\n");
				//return -1;
			}
			ret = of_property_read_u32(phy_timcon_node,
				PHY_TIM_TA_GET_NODE_NAME, &read_value);
			if (!ret) {
				ctx->lcm_ext_params[ctx->ext_params_num]
					.phy_timcon.ta_get = read_value;
				pr_info("phy_timcon.ta_get = %d\n", read_value);
			} else {
				pr_info("phy_timcon.ta_get not set!\n");
				//return -1;
			}
			ret = of_property_read_u32(phy_timcon_node,
				PHY_TIM_TA_SURE_NODE_NAME, &read_value);
			if (!ret) {
				ctx->lcm_ext_params[ctx->ext_params_num]
					.phy_timcon.ta_sure = read_value;
				pr_info("phy_timcon.ta_sure = %d\n", read_value);
			} else {
				pr_info("phy_timcon.ta_sure not set!\n");
				//return -1;
			}
			ret = of_property_read_u32(phy_timcon_node,
				PHY_TIM_TA_GO_NODE_NAME, &read_value);
			if (!ret) {
				ctx->lcm_ext_params[ctx->ext_params_num]
					.phy_timcon.ta_go = read_value;
				pr_info("phy_timcon.ta_go = %d\n", read_value);
			} else {
				pr_info("phy_timcon.ta_go not set!\n");
				//return -1;
			}
			ret = of_property_read_u32(phy_timcon_node,
				PHY_TIM_DA_HS_EXIT_NODE_NAME, &read_value);
			if (!ret) {
				ctx->lcm_ext_params[ctx->ext_params_num]
					.phy_timcon.da_hs_exit = read_value;
				pr_info("phy_timcon.da_hs_exit = %d\n", read_value);
			} else {
				pr_info("phy_timcon.da_hs_exit not set!\n");
				//return -1;
			}
			ret = of_property_read_u32(phy_timcon_node,
				PHY_TIM_CLK_TRAIL_NODE_NAME, &read_value);
			if (!ret) {
				ctx->lcm_ext_params[ctx->ext_params_num]
					.phy_timcon.clk_trail = read_value;
				pr_info("phy_timcon.clk_trail = %d\n", read_value);
			} else {
				pr_info("phy_timcon.clk_trail not set!\n");
				//return -1;
			}
			ret = of_property_read_u32(phy_timcon_node,
				PHY_TIM_CONT_DET_NODE_NAME, &read_value);
			if (!ret) {
				ctx->lcm_ext_params[ctx->ext_params_num]
					.phy_timcon.cont_det = read_value;
				pr_info("phy_timcon.cont_det = %d\n", read_value);
			} else {
				pr_info("phy_timcon.cont_det not set!\n");
				//return -1;
			}
			ret = of_property_read_u32(phy_timcon_node,
				PHY_TIM_DA_HS_SYNC_NODE_NAME, &read_value);
			if (!ret) {
				ctx->lcm_ext_params[ctx->ext_params_num]
					.phy_timcon.da_hs_sync = read_value;
				pr_info("phy_timcon.da_hs_sync = %d\n", read_value);
			} else {
				pr_info("phy_timcon.da_hs_sync not set!\n");
				//return -1;
			}
			ret = of_property_read_u32(phy_timcon_node,
				PHY_TIM_CLK_ZERO_NODE_NAME, &read_value);
			if (!ret) {
				ctx->lcm_ext_params[ctx->ext_params_num]
					.phy_timcon.clk_zero = read_value;
				pr_info("phy_timcon.clk_zero = %d\n", read_value);
			} else {
				pr_info("phy_timcon.clk_zero not set!\n");
				//return -1;
			}
			ret = of_property_read_u32(phy_timcon_node,
				PHY_TIM_CLK_HS_PRPR_NODE_NAME, &read_value);
			if (!ret) {
				ctx->lcm_ext_params[ctx->ext_params_num]
					.phy_timcon.clk_hs_prpr = read_value;
				pr_info("phy_timcon.clk_hs_prpr = %d\n", read_value);
			} else {
				pr_info("phy_timcon.clk_hs_prpr not set!\n");
				//return -1;
			}
			ret = of_property_read_u32(phy_timcon_node,
				PHY_TIM_CLK_HS_EXIT_NODE_NAME, &read_value);
			if (!ret) {
				ctx->lcm_ext_params[ctx->ext_params_num]
					.phy_timcon.clk_hs_exit = read_value;
				pr_info("phy_timcon.clk_hs_exit = %d\n", read_value);
			} else {
				pr_info("phy_timcon.clk_hs_exit not set!\n");
				//return -1;
			}
			ret = of_property_read_u32(phy_timcon_node,
				PHY_TIM_CLK_HS_POST_NODE_NAME, &read_value);
			if (!ret) {
				ctx->lcm_ext_params[ctx->ext_params_num]
					.phy_timcon.clk_hs_post = read_value;
				pr_info("phy_timcon.clk_hs_post = %d\n", read_value);
			} else {
				pr_info("phy_timcon.clk_hs_post not set!\n");
				//return -1;
			}
		}

		//dyn_fps
		dyn_fps_node = of_get_next_node_by_name(ext_node,
			NULL, EXT_PARAM_DYN_FPS_PARAM_NODE_NAME);
		if (!dyn_fps_node) {
			pr_info("dyn fps not set!\n");
			//return -1;
		} else {
			ret = of_property_read_u32(dyn_fps_node,
				DYN_FPS_SWITCH_EN_NODE_NAME, &read_value);
			if (!ret) {
				ctx->lcm_ext_params[ctx->ext_params_num]
					.dyn_fps.switch_en = read_value;
				pr_info("dyn_fps.switch_en = %d\n", read_value);
			} else {
				pr_info("dyn_fps.switch_en not set!\n");
				//return -1;
			}
			ret = of_property_read_u32(dyn_fps_node,
				DYN_FPS_VACT_TIMING_FPS_NODE_NAME, &read_value);
			if (!ret) {
				ctx->lcm_ext_params[ctx->ext_params_num]
					.dyn_fps.vact_timing_fps = read_value;
				pr_info("dyn_fps.vact_timing_fps = %d\n", read_value);
			} else {
				pr_info("dyn_fps.vact_timing_fps not set!\n");
				//return -1;
			}
			ret = of_property_read_u32(dyn_fps_node,
				DYN_FPS_DATA_RATE_NODE_NAME, &read_value);
			if (!ret) {
				ctx->lcm_ext_params[ctx->ext_params_num]
					.dyn_fps.data_rate = read_value;
				pr_info("dyn_fps.data_rate = %d\n", read_value);
			} else {
				pr_info("dyn_fps.data_rate not set!\n");
				//return -1;
			}
			for (j = 0; j < MAX_DYN_FPS_CMD_TABLE_NUM; j++) {
				dyn_fps_switch_cmd_name[strlen(dyn_fps_switch_cmd_name) - 1]
					= '0' + j;
				if (!of_get_property(dyn_fps_node, dyn_fps_switch_cmd_name, &len)) {
					pr_info("error: dyn fps cmd setting!\n");
					//return -1;
					break;
				}

				array_32 = kzalloc(len, GFP_KERNEL);
				if (!array_32)
					return -1;

				ret = of_property_read_u32_array(dyn_fps_node,
					dyn_fps_switch_cmd_name, array_32, len / sizeof(u32));
				if (ret) {
					pr_info("error: dyn fps table fail!\n");
					return -1;
				}

				for (k = 0; k < len / sizeof(u32);) {
					ctx->lcm_ext_params[ctx->ext_params_num]
						.dyn_fps.dfps_cmd_table[j].src_fps = array_32[k];
					ctx->lcm_ext_params[ctx->ext_params_num]
						.dyn_fps.dfps_cmd_table[j].cmd_num
						= array_32[k + 1];
					pr_info("dfps_cmd_table[%d].src_fps = 0x%x\n", j,
						ctx->lcm_ext_params[ctx->ext_params_num]
						.dyn_fps.dfps_cmd_table[j].src_fps);
					pr_info("dfps_cmd_table[%d].count = 0x%x\n", j,
						ctx->lcm_ext_params[ctx->ext_params_num]
						.dyn_fps.dfps_cmd_table[j].cmd_num);
					for (w = 0; w < array_32[k + 1]; w++) {
						ctx->lcm_ext_params[ctx->ext_params_num]
							.dyn_fps.dfps_cmd_table[j].para_list[w]
							= array_32[k + 2 + w];

						pr_info("dfps_cmd_table[%d].para_list[%d] = 0x%x\n",
							j, w,
							ctx->lcm_ext_params[ctx->ext_params_num]
							.dyn_fps.dfps_cmd_table[j].para_list[w]);
					}

					k += (w + 2);
				}
				kfree(array_32);
			}
		}

		//dyn
		dyn_node = of_get_next_node_by_name(ext_node,
			NULL, EXT_PARAM_DYN_MIPI_PARAM_NODE_NAME);
		if (!dyn_node) {
			pr_info("dyn not set!\n");
			//return -1;
		} else {
			ret = of_property_read_u32(dyn_node, DYN_SWITCH_EN_NODE_NAME, &read_value);
			if (!ret) {
				ctx->lcm_ext_params[ctx->ext_params_num].dyn.switch_en = read_value;
				pr_info("dyn.switch_en = %d\n", read_value);
			} else {
				pr_info("dyn.switch_en not set!\n");
				//return -1;
			}
			ret = of_property_read_u32(dyn_node, DYN_PLL_CLK_NODE_NAME, &read_value);
			if (!ret) {
				ctx->lcm_ext_params[ctx->ext_params_num].dyn.pll_clk = read_value;
				pr_info("dyn.pll_clk = %d\n", read_value);
			} else {
				pr_info("dyn.pll_clk not set!\n");
				//return -1;
			}
			ret = of_property_read_u32(dyn_node, DYN_DATA_RATE_NODE_NAME, &read_value);
			if (!ret) {
				ctx->lcm_ext_params[ctx->ext_params_num].dyn.data_rate = read_value;
				pr_info("dyn.data_rate = %d\n", read_value);
			} else {
				pr_info("dyn.data_rate not set!\n");
				//return -1;
			}
			ret = of_property_read_u32(dyn_node, DYN_VSA_NODE_NAME, &read_value);
			if (!ret) {
				ctx->lcm_ext_params[ctx->ext_params_num].dyn.vsa = read_value;
				pr_info("dyn.vsa = %d\n", read_value);
			} else {
				pr_info("dyn.vsa not set!\n");
				//return -1;
			}
			ret = of_property_read_u32(dyn_node, DYN_VBP_NODE_NAME, &read_value);
			if (!ret) {
				ctx->lcm_ext_params[ctx->ext_params_num].dyn.vbp = read_value;
				pr_info("dyn.vbp = %d\n", read_value);
			} else {
				pr_info("dyn.vbp not set!\n");
				//return -1;
			}
			ret = of_property_read_u32(dyn_node, DYN_VFP_NODE_NAME, &read_value);
			if (!ret) {
				ctx->lcm_ext_params[ctx->ext_params_num].dyn.vfp = read_value;
				pr_info("dyn.vfp = %d\n", read_value);
			} else {
				pr_info("dyn.vfp not set!\n");
				//return -1;
			}
			ret = of_property_read_u32(dyn_node, DYN_VFP_LP_DYN_NODE_NAME, &read_value);
			if (!ret) {
				ctx->lcm_ext_params[ctx->ext_params_num].dyn.vfp_lp_dyn
					= read_value;
				pr_info("dyn.vfp_lp_dyn = %d\n", read_value);
			} else {
				pr_info("dyn.vfp_lp_dyn not set!\n");
				//return -1;
			}
			ret = of_property_read_u32(dyn_node, DYN_HSA_NODE_NAME, &read_value);
			if (!ret) {
				ctx->lcm_ext_params[ctx->ext_params_num].dyn.hsa = read_value;
				pr_info("dyn.hsa = %d\n", read_value);
			} else {
				pr_info("dyn.hsa not set!\n");
				//return -1;
			}
			ret = of_property_read_u32(dyn_node, DYN_HFP_NODE_NAME, &read_value);
			if (!ret) {
				ctx->lcm_ext_params[ctx->ext_params_num].dyn.hfp = read_value;
				pr_info("dyn.hfp = %d\n", read_value);
			} else {
				pr_info("dyn.hfp not set!\n");
				//return -1;
			}
			ret = of_property_read_u32(dyn_node, DYN_HBP_NODE_NAME, &read_value);
			if (!ret) {
				ctx->lcm_ext_params[ctx->ext_params_num].dyn.hbp = read_value;
				pr_info("dyn.hbp = %d\n", read_value);
			} else {
				pr_info("dyn.hbp not set!\n");
				//return -1;
			}
			ret = of_property_read_u32(dyn_node,
				DYN_MAX_VFP_FOR_MSYNC_DYN_NODE_NAME, &read_value);
			if (!ret) {
				ctx->lcm_ext_params[ctx->ext_params_num].dyn.max_vfp_for_msync_dyn
					= read_value;
				pr_info("dyn.max_vfp_for_msync_dyn = %d\n", read_value);
			} else {
				pr_info("dyn.max_vfp_for_msync_dyn not set!\n");
				//return -1;
			}
		}

		//ssc enable
		ret = of_property_read_u32(ext_node, EXT_PARAM_SSC_ENABLE_NODE_NAME, &read_value);
		if (!ret) {
			ctx->lcm_ext_params[ctx->ext_params_num].ssc_enable = read_value;
			pr_info("ssc %sable!\n", read_value ? "en" : "dis");
		} else {
			pr_info("ssc enable not set!\n");
			//return -1;
		}

		//ssc_range
		if (ctx->lcm_ext_params[ctx->ext_params_num].ssc_enable) {
			ret = of_property_read_u32(ext_node,
				EXT_PARAM_SSC_RANGE_NODE_NAME, &read_value);
			if (!ret) {
				ctx->lcm_ext_params[ctx->ext_params_num].ssc_range = read_value;
				pr_info("ssc_range = %d\n", read_value);
			} else {
				pr_info("error:ssc_enable = 1 but ssc_range not set!\n");
				//return -1;
			}
		}

		//lcm_color_mode
		ret = of_property_read_u32(ext_node,
			EXT_PARAM_LCM_COLOR_MODE_NODE_NAME, &read_value);
		if (!ret) {
			ctx->lcm_ext_params[ctx->ext_params_num].lcm_color_mode = read_value;
			pr_info("lcm_color_mode = %d\n", read_value);
		} else {
			pr_info("lcm_color_mode not set!\n");
			//return -1;
		}

		//min_luminance
		ret = of_property_read_u32(ext_node, EXT_PARAM_MIN_LUM_NODE_NAME, &read_value);
		if (!ret) {
			ctx->lcm_ext_params[ctx->ext_params_num].min_luminance = read_value;
			pr_info("min_luminance = %d\n", read_value);
		} else {
			pr_info("min_luminance not set!\n");
			//return -1;
		}

		//average_luminance
		ret = of_property_read_u32(ext_node, EXT_PARAM_AVER_LUM_NODE_NAME, &read_value);
		if (!ret) {
			ctx->lcm_ext_params[ctx->ext_params_num].average_luminance = read_value;
			pr_info("average_luminance = %d\n", read_value);
		} else {
			pr_info("average_luminance not set!\n");
			//return -1;
		}

		//max_luminance
		ret = of_property_read_u32(ext_node, EXT_PARAM_MAX_LUM_NODE_NAME, &read_value);
		if (!ret) {
			ctx->lcm_ext_params[ctx->ext_params_num].max_luminance = read_value;
			pr_info("max_luminance = %d\n", read_value);
		} else {
			pr_info("max_luminance not set!\n");
			//return -1;
		}

		//round_corner_en
		ret = of_property_read_u32(ext_node, EXT_PARAM_RND_COR_EN_NODE_NAME, &read_value);
		if (!ret) {
			ctx->lcm_ext_params[ctx->ext_params_num].round_corner_en = read_value;
			pr_info("round_corner_en = %d\n", read_value);
		} else {
			pr_info("round_corner_en not set!\n");
			//return -1;
		}

		if (ctx->lcm_ext_params[ctx->ext_params_num].round_corner_en) {
			//corner_pattern_height
			ret = of_property_read_u32(ext_node,
				EXT_PARAM_COR_PAT_H_NODE_NAME, &read_value);
			if (!ret) {
				ctx->lcm_ext_params[ctx->ext_params_num].corner_pattern_height
					= read_value;
				pr_info("corner_pattern_height = %d\n", read_value);
			} else {
				pr_info("round_corner_en=1 but corner_pattern_height not set!\n");
				return -1;
			}

			//corner_pattern_height_bot
			ret = of_property_read_u32(ext_node,
				EXT_PARAM_COR_PAT_H_BOT_NODE_NAME, &read_value);
			if (!ret) {
				ctx->lcm_ext_params[ctx->ext_params_num].corner_pattern_height_bot
					= read_value;
				pr_info("corner_pattern_height_bot = %d\n", read_value);
			} else {
				pr_info("error: round_corner_en=1 but corner_pattern_height_bot not set!\n");
				return -1;
			}

			//corner_pattern_tp_size
			ret = of_property_read_u32(ext_node,
				EXT_PARAM_COR_PAT_TP_SIZE_NODE_NAME, &read_value);
			if (!ret) {
				ctx->lcm_ext_params[ctx->ext_params_num].corner_pattern_tp_size
					= read_value;
				pr_info("corner_pattern_tp_size = %d\n", read_value);
			} else {
				pr_info("error: round_corner_en=1 but corner_pattern_tp_size not set!\n");
				return -1;
			}

			//corner_pattern_tp_size_l
			ret = of_property_read_u32(ext_node,
				EXT_PARAM_COR_PAT_TP_SIZE_L_NODE_NAME, &read_value);
			if (!ret) {
				ctx->lcm_ext_params[ctx->ext_params_num].corner_pattern_tp_size_l
					= read_value;
				pr_info("corner_pattern_tp_size_l = %d\n", read_value);
			} else {
				pr_info("error: round_corner_en=1 but corner_pattern_tp_size_l not set!\n");
				return -1;
			}

			//corner_pattern_tp_size_r
			ret = of_property_read_u32(ext_node,
				EXT_PARAM_COR_PAT_TP_SIZE_R_NODE_NAME, &read_value);
			if (!ret) {
				ctx->lcm_ext_params[ctx->ext_params_num].corner_pattern_tp_size_r
					= read_value;
				pr_info("corner_pattern_tp_size_r = %d\n", read_value);
			} else {
				pr_info("error: round_corner_en=1 but corner_pattern_tp_size_r not set!\n");
				return -1;
			}

			//corner_pattern_lt_addr
			ret = of_property_read_u32(ext_node,
				EXT_PARAM_COR_PAT_LT_ADDR_NODE_NAME, &read_value);
			if (!ret) {
				long_addr = (u64)read_value;
				ctx->lcm_ext_params[ctx->ext_params_num].corner_pattern_lt_addr
					= (unsigned long *)long_addr;
				pr_info("corner_pattern_lt_addr = %d\n", read_value);
			} else {
				pr_info("error: round_corner_en=1 but corner_pattern_lt_addr not set!\n");
				return -1;
			}

			//corner_pattern_lt_addr_l
			ret = of_property_read_u32(ext_node,
				EXT_PARAM_COR_PAT_LT_ADDR_L_NODE_NAME, &read_value);
			if (!ret) {
				long_addr = (u64)read_value;
				ctx->lcm_ext_params[ctx->ext_params_num].corner_pattern_lt_addr_l
					= (unsigned long *)long_addr;
				pr_info("corner_pattern_lt_addr_l = %d\n", read_value);
			} else {
				pr_info("error: round_corner_en=1 but corner_pattern_lt_addr_l not set!\n");
				return -1;
			}

			//corner_pattern_lt_addr_r
			ret = of_property_read_u32(ext_node,
				EXT_PARAM_COR_PAT_LT_ADDR_R_NODE_NAME, &read_value);
			if (!ret) {
				long_addr = (u64)read_value;
				ctx->lcm_ext_params[ctx->ext_params_num].corner_pattern_lt_addr_r
					= (unsigned long *)long_addr;
				pr_info("corner_pattern_lt_addr_r = %d\n", read_value);
			} else {
				pr_info("error: round_corner_en=1 but corner_pattern_lt_addr_r not set!\n");
				return -1;
			}
		}

		//physical_width_um
		ret = of_property_read_u32(ext_node, EXT_PARAM_PHY_W_UM_NODE_NAME, &read_value);
		if (!ret) {
			ctx->lcm_ext_params[ctx->ext_params_num].physical_width_um = read_value;
			pr_info("physical_width_um = %d\n", read_value);
		} else {
			pr_info("physical_width_um not set!\n");
			//return -1;
		}

		//physical_height_um
		ret = of_property_read_u32(ext_node, EXT_PARAM_PHY_H_UM_NODE_NAME, &read_value);
		if (!ret) {
			ctx->lcm_ext_params[ctx->ext_params_num].physical_height_um = read_value;
			pr_info("physical_height_um = %d\n", read_value);
		} else {
			pr_info("physical_height_um not set!\n");
			//return -1;
		}

		//is_cphy
		ret = of_property_read_u32(ext_node, EXT_PARAM_IS_CPHY_NODE_NAME, &read_value);
		if (!ret) {
			ctx->lcm_ext_params[ctx->ext_params_num].is_cphy = read_value;
			pr_info("is_cphy = %d\n", read_value);
		} else {
			pr_info("is_cphy not set!\n");
			//return -1;
		}

		//output_mode
		ret = of_property_read_u32(ext_node, EXT_PARAM_OUTPUT_MODE_NODE_NAME, &read_value);
		if (!ret) {
			ctx->lcm_ext_params[ctx->ext_params_num].output_mode = read_value;
			pr_info("output_mode = %d\n", read_value);
		} else {
			pr_info("output_mode not set!\n");
			//return -1;
		}

		//lcm_cmd_if
		ret = of_property_read_u32(ext_node, EXT_PARAM_LCM_CMD_IF_NODE_NAME, &read_value);
		if (!ret) {
			ctx->lcm_ext_params[ctx->ext_params_num].lcm_cmd_if = read_value;
			pr_info("lcm_cmd_if = %d\n", read_value);
		} else {
			pr_info("lcm_cmd_if not set!\n");
			//return -1;
		}

		//hbm_en_time
		ret = of_property_read_u32(ext_node, EXT_PARAM_HBM_EN_TIME_NODE_NAME, &read_value);
		if (!ret) {
			ctx->lcm_ext_params[ctx->ext_params_num].hbm_en_time = read_value;
			pr_info("hbm_en_time = %d\n", read_value);
		} else {
			pr_info("hbm_en_time not set!\n");
			//return -1;
		}

		//hbm_dis_time
		ret = of_property_read_u32(ext_node, EXT_PARAM_HBM_DIS_TIME_NODE_NAME, &read_value);
		if (!ret) {
			ctx->lcm_ext_params[ctx->ext_params_num].hbm_dis_time = read_value;
			pr_info("hbm_dis_time = %d\n", read_value);
		} else {
			pr_info("hbm_dis_time not set!\n");
			//return -1;
		}

		//lcm_index
		ret = of_property_read_u32(ext_node, EXT_PARAM_LCM_INDEX_NODE_NAME, &read_value);
		if (!ret) {
			ctx->lcm_ext_params[ctx->ext_params_num].lcm_index = read_value;
			pr_info("lcm_index = %d\n", read_value);
		} else {
			pr_info("lcm_index not set!\n");
			//return -1;
		}

		//wait_sof_before_dec_vfp
		ret = of_property_read_u32(ext_node,
			EXT_PARAM_WAIT_SOF_BEFORE_DEC_VFP_NODE_NAME, &read_value);
		if (!ret) {
			ctx->lcm_ext_params[ctx->ext_params_num].wait_sof_before_dec_vfp
				= read_value;
			pr_info("wait_sof_before_dec_vfp = %d\n", read_value);
		} else {
			pr_info("wait_sof_before_dec_vfp not set!\n");
			//return -1;
		}

		//doze_delay
		ret = of_property_read_u32(ext_node, EXT_PARAM_DOZE_DELAY_NODE_NAME, &read_value);
		if (!ret) {
			ctx->lcm_ext_params[ctx->ext_params_num].doze_delay = read_value;
			pr_info("doze_delay = %d\n", read_value);
		} else {
			pr_info("doze_delay not set!\n");
			//return -1;
		}

		//lfr_enable
		ret = of_property_read_u32(ext_node, EXT_PARAM_LFR_ENABLE_NODE_NAME, &read_value);
		if (!ret) {
			ctx->lcm_ext_params[ctx->ext_params_num].lfr_enable = read_value;
			pr_info("lfr_enable = %d\n", read_value);
		} else {
			pr_info("lfr_enable not set!\n");
			//return -1;
		}

		//lfr_minimum_fps
		ret = of_property_read_u32(ext_node,
			EXT_PARAM_LFR_MINIMUM_FPS_NODE_NAME, &read_value);
		if (!ret) {
			ctx->lcm_ext_params[ctx->ext_params_num].lfr_minimum_fps = read_value;
			pr_info("lfr_minimum_fps = %d\n", read_value);
		} else {
			pr_info("lfr_minimum_fps not set!\n");
			//return -1;
		}

		//esd setting
		ret = of_property_read_u32(ext_node, EXT_PARAM_ESD_CHECK_EN_NODE_NAME, &read_value);
		if (!ret) {
			ctx->lcm_ext_params[ctx->ext_params_num].esd_check_enable = read_value;
			pr_info("ext_params[%d].esd_check_enable = %d\n",
				ctx->ext_params_num, read_value);
		} else {
			pr_info("esd check not set!\n");
			//return -1;
		}
		ret = of_property_read_u32(ext_node,
			EXT_PARAM_CUST_ESD_CHECK_NODE_NAME, &read_value);
		if (!ret) {
			ctx->lcm_ext_params[ctx->ext_params_num].cust_esd_check = read_value;
			pr_info("ext_params[%d].cust esd_check = %d\n",
				ctx->ext_params_num, read_value);
		} else {
			pr_info("cust esd check not set!\n");
			//return -1;
		}
		if (ctx->lcm_ext_params[ctx->ext_params_num].esd_check_enable) {
			if (!of_get_property(ext_node,
				EXT_PARAM_CUST_ESD_CHECK_TABLE_NODE_NAME, &len)) {
				pr_info("error: cust esd check enable but not get esd table setting!\n");
				return -1;
			}

			array = kzalloc(len, GFP_KERNEL);
			if (!array)
				return -1;

			ret = of_property_read_u8_array(ext_node,
				EXT_PARAM_CUST_ESD_CHECK_TABLE_NODE_NAME, array, len);
			if (ret) {
				pr_info("error: read esd table fail!\n");
				return -1;
			}
			for (j = 0; j < len;) {
				ctx->lcm_ext_params[ctx->ext_params_num]
					.lcm_esd_check_table[k].cmd = array[j];
				ctx->lcm_ext_params[ctx->ext_params_num]
					.lcm_esd_check_table[k].count = array[j + 1];
				ctx->lcm_ext_params[ctx->ext_params_num]
					.lcm_esd_check_table[k].para_list[0] = array[j + 2];
				ctx->lcm_ext_params[ctx->ext_params_num]
					.lcm_esd_check_table[k].mask_list[0] = array[j + 3];
				pr_info("lcm_esd_check_table[%d].cmd = 0x%x\n",
					ctx->ext_params_num, k,
					ctx->lcm_ext_params[ctx->ext_params_num]
					.lcm_esd_check_table[k].cmd);
				pr_info("lcm_esd_check_table[%d].count = 0x%x\n",
					ctx->ext_params_num, k,
					ctx->lcm_ext_params[ctx->ext_params_num]
					.lcm_esd_check_table[k].count);
				pr_info("lcm_esd_check_table[%d].para_list[0] = 0x%x\n",
					ctx->ext_params_num, k,
					ctx->lcm_ext_params[ctx->ext_params_num]
					.lcm_esd_check_table[k].para_list[0]);
				pr_info("lcm_esd_check_table[%d].mask_list[0] = 0x%x\n",
					ctx->ext_params_num, k,
					ctx->lcm_ext_params[ctx->ext_params_num]
					.lcm_esd_check_table[k].mask_list[0]);
				k += 1;
				j += 4;
			}
			kfree(array);
		}

		//vfp low power
		ret = of_property_read_u32(ext_node, EXT_PARAM_VFP_LP_NODE_NAME, &read_value);
		if (!ret) {
			ctx->lcm_ext_params[ctx->ext_params_num].vfp_low_power = read_value;
			pr_info("vfp low power = %d\n", read_value);
		} else {
			pr_info("vfp low power not set!\n");
			//return -1;
		}

		//lane swap setting
		ret = of_property_read_u32(ext_node, EXT_PARAM_LANE_SWAP_NODE_NAME, &read_value);
		if (!ret) {
			ctx->lcm_ext_params[ctx->ext_params_num].lane_swap_en = read_value;
			pr_info("ext_params[%d].lane_swap_en = %d\n",
				ctx->ext_params_num, read_value);
		} else {
			pr_info("lane swap not set!\n");
			//return -1;
		}
		if (ctx->lcm_ext_params[ctx->ext_params_num].lane_swap_en) {
			if (!of_get_property(ext_node,
				ctx->lcm_ext_params[ctx->ext_params_num].is_cphy ?
				EXT_PARAM_CPHY_LANE_SWAP_DATA_NODE_NAME
				 : EXT_PARAM_LANE_SWAP_DATA_NODE_NAME, &len)) {
				pr_info("error: lane swap enable but not get lane swap setting!\n");
				return -1;
			}

			exp_len = ctx->lcm_ext_params[ctx->ext_params_num].is_cphy ? 20 : 12;
			if (len != exp_len * sizeof(u32)) {
				pr_info("error: lane swap len error exp[%d]act[%d]\n",
					exp_len, len / sizeof(u32));
				return -1;
			}

			array_32 = kzalloc(len, GFP_KERNEL);
			if (!array_32)
				return -1;

			ret = of_property_read_u32_array(ext_node,
				ctx->lcm_ext_params[ctx->ext_params_num].is_cphy ?
				EXT_PARAM_CPHY_LANE_SWAP_DATA_NODE_NAME
				 : EXT_PARAM_LANE_SWAP_DATA_NODE_NAME, array_32, len / sizeof(u32));
			if (ret) {
				pr_info("read lane swap data fail!\n", __func__);
				return -1;
			}
			for (j = 0; j < exp_len / 2; j++) {
				ctx->lcm_ext_params[ctx->ext_params_num].lane_swap[0][j]
					= array_32[j];
				pr_info("ctx->ext_params[%d].lane_swap[0][%d] = %d\n",
					ctx->ext_params_num, j,
					ctx->lcm_ext_params[ctx->ext_params_num].lane_swap[0][j]);
			}
			for (j = 0; j < exp_len / 2; j++) {
				ctx->lcm_ext_params[ctx->ext_params_num].lane_swap[1][j]
					= array_32[j + exp_len];
				pr_info("ctx->ext_params[%d].lane_swap[1][%d] = %d\n",
					ctx->ext_params_num, j,
					ctx->lcm_ext_params[ctx->ext_params_num].lane_swap[1][j]);
			}
			kfree(array_32);
		}

		//dsc setting
		ret = of_property_read_u32(ext_node, EXT_PARAM_DSC_ENABLE_NODE_NAME, &read_value);
		if (!ret) {
			ctx->lcm_ext_params[ctx->ext_params_num].dsc_params.enable = read_value;
			pr_info("ctx->ext_params[%d].dsc_enable = %d\n",
				ctx->ext_params_num, read_value);
		} else {
			pr_info("dsc_enable not set!\n");
			//return -1;
		}
		ret = of_property_read_u32(ext_node,
			EXT_PARAM_DUAL_DSC_ENABLE_NODE_NAME, &read_value);
		if (!ret) {
			ctx->lcm_ext_params[ctx->ext_params_num].dsc_params.dual_dsc_enable
				= read_value;
			pr_info("ctx->ext_params[%d].dual_dsc_enable = %d\n",
				ctx->ext_params_num, read_value);
		} else {
			pr_info("dual_dsc_enable not set!\n");
			//return -1;
		}
		if (ctx->lcm_ext_params[ctx->ext_params_num].dsc_params.enable) {
			ret = of_property_read_u32(ext_node,
				EXT_PARAM_DSC_SLICE_MODE_NODE_NAME, &read_value);
			if (!ret) {
				ctx->lcm_ext_params[ctx->ext_params_num]
					.dsc_params.slice_mode = read_value;
				pr_info("ctx->ext_params[%d].dsc_params.slice_mode = %d\n",
					ctx->ext_params_num, read_value);
			} else {
				pr_info("error: read slice_mode error!\n");
				return -1;
			}
			if (!of_get_property(ext_node, EXT_PARAM_DSC_PPS_NODE_NAME, &len)) {
				pr_info("error: read dsc pps name error!\n");
				return -1;
			}

			if (len != 128)
				return -1;

			array = kzalloc(len, GFP_KERNEL);
			if (!array)
				return -1;

			ret = of_property_read_u8_array(ext_node,
				EXT_PARAM_DSC_PPS_NODE_NAME, array, len);
			if (ret) {
				pr_info("%s:error:  read dsc pps fail!\n", __func__);
				return -1;
			}
			ctx->lcm_ext_params[ctx->ext_params_num]
				.dsc_params.ver = array[0];
			ctx->lcm_ext_params[ctx->ext_params_num]
				.dsc_params.bit_per_channel = array[3] >> 4;
			ctx->lcm_ext_params[ctx->ext_params_num]
				.dsc_params.dsc_line_buf_depth = array[3] & 0xF;
			ctx->lcm_ext_params[ctx->ext_params_num]
				.dsc_params.bp_enable = (array[4] & 0x10) >> 4;
			ctx->lcm_ext_params[ctx->ext_params_num]
				.dsc_params.rct_on = (array[4] & 0x20) >> 5;
			ctx->lcm_ext_params[ctx->ext_params_num]
				.dsc_params.bit_per_pixel = array[5];
			ctx->lcm_ext_params[ctx->ext_params_num]
				.dsc_params.pic_height = (array[6] << 8) + array[7];
			ctx->lcm_ext_params[ctx->ext_params_num]
				.dsc_params.pic_width = (array[8] << 8) + array[9];
			ctx->lcm_ext_params[ctx->ext_params_num]
				.dsc_params.slice_height = (array[10] << 8) + array[11];
			ctx->lcm_ext_params[ctx->ext_params_num]
				.dsc_params.slice_width = (array[12] << 8) + array[13];
			ctx->lcm_ext_params[ctx->ext_params_num]
				.dsc_params.chunk_size = (array[14] << 8) + array[15];
			ctx->lcm_ext_params[ctx->ext_params_num]
				.dsc_params.xmit_delay = (array[16] << 8) + array[17];
			ctx->lcm_ext_params[ctx->ext_params_num]
				.dsc_params.dec_delay = (array[18] << 8) + array[19];
			ctx->lcm_ext_params[ctx->ext_params_num]
				.dsc_params.scale_value = (array[20] << 8) + array[21];
			ctx->lcm_ext_params[ctx->ext_params_num]
				.dsc_params.increment_interval = (array[22] << 8) + array[23];
			ctx->lcm_ext_params[ctx->ext_params_num]
				.dsc_params.decrement_interval = (array[24] << 8) + array[25];
			ctx->lcm_ext_params[ctx->ext_params_num]
				.dsc_params.line_bpg_offset = (array[26] << 8) + array[27];
			ctx->lcm_ext_params[ctx->ext_params_num]
				.dsc_params.nfl_bpg_offset = (array[28] << 8) + array[29];
			ctx->lcm_ext_params[ctx->ext_params_num]
				.dsc_params.slice_bpg_offset = (array[30] << 8) + array[31];
			ctx->lcm_ext_params[ctx->ext_params_num]
				.dsc_params.initial_offset = (array[32] << 8) + array[33];
			ctx->lcm_ext_params[ctx->ext_params_num]
				.dsc_params.final_offset = (array[34] << 8) + array[35];
			ctx->lcm_ext_params[ctx->ext_params_num]
				.dsc_params.flatness_minqp = array[36];
			ctx->lcm_ext_params[ctx->ext_params_num]
				.dsc_params.flatness_maxqp = array[37];
			ctx->lcm_ext_params[ctx->ext_params_num]
				.dsc_params.rc_model_size = (array[38] << 8) + array[39];
			ctx->lcm_ext_params[ctx->ext_params_num]
				.dsc_params.rc_edge_factor = array[40];
			ctx->lcm_ext_params[ctx->ext_params_num]
				.dsc_params.rc_quant_incr_limit0 = array[41];
			ctx->lcm_ext_params[ctx->ext_params_num]
				.dsc_params.rc_quant_incr_limit1 = array[42];
			ctx->lcm_ext_params[ctx->ext_params_num]
				.dsc_params.rc_tgt_offset_hi = (array[43] >> 0x4);
			ctx->lcm_ext_params[ctx->ext_params_num]
				.dsc_params.rc_tgt_offset_lo = (array[43] & 0xF);
			pr_info("ctx->ext_params[%d].dsc_params.ver = %d!\n",
				ctx->ext_params_num, ctx->lcm_ext_params[ctx->ext_params_num]
				.dsc_params.ver);
			pr_info("ctx->ext_params[%d].dsc_params.bit_per_channel = %d!\n",
				ctx->ext_params_num, ctx->lcm_ext_params[ctx->ext_params_num]
				.dsc_params.bit_per_channel);
			pr_info("ctx->ext_params[%d].dsc_params.dsc_line_buf_depth = %d!\n",
				ctx->ext_params_num, ctx->lcm_ext_params[ctx->ext_params_num]
				.dsc_params.dsc_line_buf_depth);
			pr_info("ctx->ext_params[%d].dsc_params.bp_enable = %d!\n",
				ctx->ext_params_num, ctx->lcm_ext_params[ctx->ext_params_num]
				.dsc_params.bp_enable);
			pr_info("ctx->ext_params[%d].dsc_params.rct_on = %d!\n",
				ctx->ext_params_num, ctx->lcm_ext_params[ctx->ext_params_num]
				.dsc_params.rct_on);
			pr_info("ctx->ext_params[%d].dsc_params.bit_per_pixel = %d!\n",
				ctx->ext_params_num, ctx->lcm_ext_params[ctx->ext_params_num]
				.dsc_params.bit_per_pixel);
			pr_info("ctx->ext_params[%d].dsc_params.pic_height = %d!\n",
				ctx->ext_params_num, ctx->lcm_ext_params[ctx->ext_params_num]
				.dsc_params.pic_height);
			pr_info("ctx->ext_params[%d].dsc_params.pic_width = %d!\n",
				ctx->ext_params_num, ctx->lcm_ext_params[ctx->ext_params_num]
				.dsc_params.pic_width);
			pr_info("ctx->ext_params[%d].dsc_params.chunk_size = %d!\n",
				ctx->ext_params_num, ctx->lcm_ext_params[ctx->ext_params_num]
				.dsc_params.chunk_size);
			pr_info("ctx->ext_params[%d].dsc_params.xmit_delay = %d!\n",
				ctx->ext_params_num, ctx->lcm_ext_params[ctx->ext_params_num]
				.dsc_params.xmit_delay);
			pr_info("ctx->ext_params[%d].dsc_params.dec_delay = %d!\n",
				ctx->ext_params_num, ctx->lcm_ext_params[ctx->ext_params_num]
				.dsc_params.dec_delay);
			pr_info("ctx->ext_params[%d].dsc_params.scale_value = %d!\n",
				ctx->ext_params_num, ctx->lcm_ext_params[ctx->ext_params_num]
				.dsc_params.scale_value);
			pr_info("ctx->ext_params[%d].dsc_params.increment_interval = %d!\n",
				ctx->ext_params_num, ctx->lcm_ext_params[ctx->ext_params_num]
				.dsc_params.increment_interval);
			pr_info("ctx->ext_params[%d].dsc_params.decrement_interval = %d!\n",
				ctx->ext_params_num, ctx->lcm_ext_params[ctx->ext_params_num]
				.dsc_params.decrement_interval);
			pr_info("ctx->ext_params[%d].dsc_params.line_bpg_offset = %d!\n",
				ctx->ext_params_num, ctx->lcm_ext_params[ctx->ext_params_num]
				.dsc_params.line_bpg_offset);
			pr_info("ctx->ext_params[%d].dsc_params.nfl_bpg_offset = %d!\n",
				ctx->ext_params_num, ctx->lcm_ext_params[ctx->ext_params_num]
				.dsc_params.nfl_bpg_offset);
			pr_info("ctx->ext_params[%d].dsc_params.slice_bpg_offset = %d!\n",
				ctx->ext_params_num, ctx->lcm_ext_params[ctx->ext_params_num]
				.dsc_params.slice_bpg_offset);
			pr_info("ctx->ext_params[%d].dsc_params.initial_offset = %d!\n",
				ctx->ext_params_num, ctx->lcm_ext_params[ctx->ext_params_num]
				.dsc_params.initial_offset);
			pr_info("ctx->ext_params[%d].dsc_params.final_offset = %d!\n",
				ctx->ext_params_num, ctx->lcm_ext_params[ctx->ext_params_num]
				.dsc_params.final_offset);
			pr_info("ctx->ext_params[%d].dsc_params.flatness_minqp = %d!\n",
				ctx->ext_params_num, ctx->lcm_ext_params[ctx->ext_params_num]
				.dsc_params.flatness_minqp);
			pr_info("ctx->ext_params[%d].dsc_params.flatness_maxqp = %d!\n",
				ctx->ext_params_num, ctx->lcm_ext_params[ctx->ext_params_num]
				.dsc_params.flatness_maxqp);
			pr_info("ctx->ext_params[%d].dsc_params.rc_model_size = %d!\n",
				ctx->ext_params_num, ctx->lcm_ext_params[ctx->ext_params_num]
				.dsc_params.rc_model_size);
			pr_info("ctx->ext_params[%d].dsc_params.rc_edge_factor = %d!\n",
				ctx->ext_params_num, ctx->lcm_ext_params[ctx->ext_params_num]
				.dsc_params.rc_edge_factor);
			pr_info("ctx->ext_params[%d].dsc_params.rc_quant_incr_limit0 = %d!\n",
				ctx->ext_params_num, ctx->lcm_ext_params[ctx->ext_params_num]
				.dsc_params.rc_quant_incr_limit0);
			pr_info("ctx->ext_params[%d].dsc_params.rc_quant_incr_limit1 = %d!\n",
				ctx->ext_params_num, ctx->lcm_ext_params[ctx->ext_params_num]
				.dsc_params.rc_quant_incr_limit1);
			pr_info("ctx->ext_params[%d].dsc_params.rc_tgt_offset_hi = %d!\n",
				ctx->ext_params_num, ctx->lcm_ext_params[ctx->ext_params_num]
				.dsc_params.rc_tgt_offset_hi);
			pr_info("ctx->ext_params[%d].dsc_params.rc_tgt_offset_lo = %d!\n",
				ctx->ext_params_num, ctx->lcm_ext_params[ctx->ext_params_num]
				.dsc_params.rc_tgt_offset_lo);
			for (j = 0; j < 14; j++) {
				ctx->lcm_ext_params[ctx->ext_params_num]
					.dsc_params.rc_buf_thresh[j] = array[44 + j];
				pr_info("ctx->ext_params[%d].dsc_params.rc_buf_thresh[%d] = %d!\n",
					ctx->ext_params_num, j,
					ctx->lcm_ext_params[ctx->ext_params_num]
					.dsc_params.rc_buf_thresh[j]);
			}
			for (j = 0; j < 15; j++) {
				read_value = (array[58 + j * 2] << 8) + array[59 + j * 2];
			    ctx->lcm_ext_params[ctx->ext_params_num]
					.dsc_params.rc_range_parameters[j].range_min_qp
					= read_value >> 11;
			    ctx->lcm_ext_params[ctx->ext_params_num]
					.dsc_params.rc_range_parameters[j].range_max_qp
					= (read_value & 0x07C0) >> 6;
			    ctx->lcm_ext_params[ctx->ext_params_num]
					.dsc_params.rc_range_parameters[j].range_bpg_offset
					= (read_value & 0x3F);
				pr_info(
					"ctx->ext_params[%d].dsc_params.rc_range_parameters[%d].range_min_qp = %d!\n",
					ctx->ext_params_num, j,
					ctx->lcm_ext_params[ctx->ext_params_num]
					.dsc_params.rc_range_parameters[j].range_min_qp);
				pr_info(
					"ctx->ext_params[%d].dsc_params.rc_range_parameters[%d].range_max_qp = %d!\n",
					ctx->ext_params_num, j,
					ctx->lcm_ext_params[ctx->ext_params_num]
					.dsc_params.rc_range_parameters[j].range_max_qp);
				pr_info(
					"ctx->ext_params[%d].dsc_params.rc_range_parameters[%d].range_bpg_offset = %d!\n",
					ctx->ext_params_num, j,
					ctx->lcm_ext_params[ctx->ext_params_num]
					.dsc_params.rc_range_parameters[j].range_bpg_offset);
			}
			ctx->lcm_ext_params[ctx->ext_params_num].dsc_params.dsc_cfg
				= (array[3] == 0x89) ? 0x22 : 0x828;
			pr_info("ctx->ext_params[%d].dsc_params.dsc_cfg = 0x%x!\n",
				ctx->ext_params_num,
				ctx->lcm_ext_params[ctx->ext_params_num].dsc_params.dsc_cfg);
			kfree(array);
		}
		//change_fps_by_vfp_send_cmd
		ret = of_property_read_u32(ext_node,
			EXT_PARAM_CHANG_FPS_VFP_SEND_CMD_NODE_NAME, &read_value);
		if (!ret) {
			ctx->lcm_ext_params[ctx->ext_params_num].change_fps_by_vfp_send_cmd
				= read_value;
			pr_info("ctx->ext_params[%d].change_fps_by_vfp_send_cmd = %d\n",
				ctx->ext_params_num, read_value);
		} else {
			pr_info("change_fps_by_vfp_send_cmd not set!\n");
			//return -1;
		}

		//vdo_per_frame_lp_enable
		ret = of_property_read_u32(ext_node,
			EXT_PARAM_VDO_PER_FRAME_LP_NODE_NAME, &read_value);
		if (!ret) {
			ctx->lcm_ext_params[ctx->ext_params_num].vdo_per_frame_lp_enable
				= read_value;
			pr_info("ctx->ext_params[%d].vdo_per_frame_lp_enable = %d\n",
				ctx->ext_params_num, read_value);
		} else {
			pr_info("vdo_per_frame_lp_enable not set!\n");
			//return -1;
		}

		//lp_perline_en
		ret = of_property_read_u32(ext_node, EXT_PARAM_LP_PERLINE_NODE_NAME, &read_value);
		if (!ret) {
			ctx->lcm_ext_params[ctx->ext_params_num].lp_perline_en = read_value;
			pr_info("ctx->ext_params[%d].lp_perline_en = %d\n",
				ctx->ext_params_num, read_value);
		} else {
			pr_info("lp_perline_en not set!\n");
			//return -1;
		}

		//cmd_null_pkt_en
		ret = of_property_read_u32(ext_node,
			EXT_PARAM_CMD_NULLPKT_EN_NODE_NAME, &read_value);
		if (!ret) {
			ctx->lcm_ext_params[ctx->ext_params_num].cmd_null_pkt_en = read_value;
			pr_info("ctx->ext_params[%d].cmd_null_pkt_en = %d\n",
				ctx->ext_params_num, read_value);
		} else {
			pr_info("cmd_null_pkt_en not set!\n");
			//return -1;
		}

		//cmd_null_pkt_len
		ret = of_property_read_u32(ext_node,
			EXT_PARAM_CMD_NULLPKT_LEN_NODE_NAME, &read_value);
		if (!ret) {
			ctx->lcm_ext_params[ctx->ext_params_num].cmd_null_pkt_len = read_value;
			pr_info("ctx->ext_params[%d].cmd_null_pkt_len = %d\n",
				ctx->ext_params_num, read_value);
		} else {
			pr_info("cmd_null_pkt_len not set!\n");
			//return -1;
		}

		//set_area_before_trigger
		ret = of_property_read_u32(ext_node,
			EXT_PARAM_SET_AREA_TRIG_NODE_NAME, &read_value);
		if (!ret) {
			ctx->lcm_ext_params[ctx->ext_params_num].set_area_before_trigger
				= read_value;
			pr_info("ctx->ext_params[%d].set_area_before_trigger = %d\n",
				ctx->ext_params_num, read_value);
		} else {
			pr_info("set_area_before_trigger not set!\n");
			//return -1;
		}
		//rotate
		ret = of_property_read_u32(ext_node, EXT_PARAM_PANEL_ROTATE_NODE_NAME, &read_value);
		if (!ret) {
			ctx->lcm_ext_params[ctx->ext_params_num].rotate = read_value;
			pr_info("ctx->ext_params[%d].rotate = %d\n",
				ctx->ext_params_num, read_value);
		} else {
			pr_info("rotate not set!\n");
			//return -1;
		}
		ctx->ext_params_num++;
	}

	of_node_put(ext_node);
	pr_info("%s -\n", __func__);
	return 0;
}

static int lcm_get_dsi_setting_from_dts(struct lcm *ctx)
{
	int ret = 0;
	u32 read_value = 0;

	pr_info("%s +\n", __func__);

	ret = of_property_read_u32(ctx->dev->of_node, LCM_DSI_LANE_NUM_NODE_NAME, &read_value);
	if (!ret) {
		ctx->dsi_set.lane_num = read_value;
		pr_info("dsi lane number = %d!\n", read_value);
	} else {
		pr_info("error: dsi lane number error!\n");
		return -1;
	}

	ret = of_property_read_u32(ctx->dev->of_node, LCM_DSI_FORMAT_NODE_NAME, &read_value);
	if (!ret) {
		ctx->dsi_set.format = read_value;
		pr_info("dsi format = %d!\n", read_value);
	} else {
		pr_info("error: dsi format error!\n");
		return -1;
	}

	ret = of_property_read_u32(ctx->dev->of_node, LCM_DSI_MODE_NODE_NAME, &read_value);
	if (!ret) {
		ctx->dsi_set.mode_flag = read_value;
		pr_info("dsi mode_flag = %d!\n", read_value);
	} else {
		pr_info("error: dsi mode_flag error!\n");
		return -1;
	}

	pr_info("%s -\n", __func__);
	return 0;
}

static int lcm_get_panel_mode_from_dts(struct lcm *ctx)
{
	int ret = 0;
	u32 read_value = 0;
	char mode_name[] = PANEL_MODE_NODE_NAME;
	struct device_node *mode_node = NULL;

	pr_info("%s +\n", __func__);

	ctx->disp_mode_num = 0;

	while ((mode_node = of_get_next_node_by_name(ctx->dev->of_node, mode_node, mode_name))) {
		pr_info("%s find!\n", mode_name);

		ret = of_property_read_u32(mode_node, PANEL_WIDHT_NODE_NAME, &read_value);
		if (!ret) {
			ctx->lcm_disp_mode[ctx->disp_mode_num].hdisplay = read_value;
			pr_info("disp_mode[%d].hdisplay = %d\n", ctx->disp_mode_num, read_value);
		} else {
			pr_info("error: read width error!\n");
			return -1;
		}

		ret = of_property_read_u32(mode_node, PANEL_HEIGHT_NODE_NAME, &read_value);
		if (!ret) {
			ctx->lcm_disp_mode[ctx->disp_mode_num].vdisplay = read_value;
			pr_info("disp_mode[%d].vdisplay = %d\n", ctx->disp_mode_num, read_value);
		} else {
			pr_info("error: read height error!\n");
			return -1;
		}

		ret = of_property_read_u32(mode_node, PANEL_HFP_NODE_NAME, &read_value);
		if (!ret) {
			ctx->lcm_disp_mode[ctx->disp_mode_num].hsync_start =
				read_value + ctx->lcm_disp_mode[ctx->disp_mode_num].hdisplay;
			pr_info("disp_mode[%d].hfp = %d\n", ctx->disp_mode_num, read_value);
		} else {
			pr_info("error: read hfp error!\n");
			return -1;
		}

		ret = of_property_read_u32(mode_node, PANEL_HSA_NODE_NAME, &read_value);
		if (!ret) {
			ctx->lcm_disp_mode[ctx->disp_mode_num].hsync_end =
				read_value + ctx->lcm_disp_mode[ctx->disp_mode_num].hsync_start;
			pr_info("disp_mode[%d].hsa = %d\n", ctx->disp_mode_num, read_value);
		} else {
			pr_info("error: read hsa error!\n");
			return -1;
		}

		ret = of_property_read_u32(mode_node, PANEL_HBP_NODE_NAME, &read_value);
		if (!ret) {
			ctx->lcm_disp_mode[ctx->disp_mode_num].htotal =
				read_value + ctx->lcm_disp_mode[ctx->disp_mode_num].hsync_end;
			pr_info("disp_mode[%d].hbp = %d\n", ctx->disp_mode_num, read_value);
		} else {
			pr_info("error: read hbp error!\n");
			return -1;
		}

		ret = of_property_read_u32(mode_node, PANEL_VFP_NODE_NAME, &read_value);
		if (!ret) {
			ctx->lcm_disp_mode[ctx->disp_mode_num].vsync_start =
				read_value + ctx->lcm_disp_mode[ctx->disp_mode_num].vdisplay;
			pr_info("disp_mode[%d].vfp = %d\n", ctx->disp_mode_num, read_value);
		} else {
			pr_info("error: read vfp error!\n");
			return -1;
		}

		ret = of_property_read_u32(mode_node, PANEL_VSA_NODE_NAME, &read_value);
		if (!ret) {
			ctx->lcm_disp_mode[ctx->disp_mode_num].vsync_end =
				read_value + ctx->lcm_disp_mode[ctx->disp_mode_num].vsync_start;
			pr_info("disp_mode[%d].vsa = %d\n", ctx->disp_mode_num, read_value);
		} else {
			pr_info("error: read vsa error!\n");
			return -1;
		}

		ret = of_property_read_u32(mode_node, PANEL_VBP_NODE_NAME, &read_value);
		if (!ret) {
			ctx->lcm_disp_mode[ctx->disp_mode_num].vtotal =
				read_value + ctx->lcm_disp_mode[ctx->disp_mode_num].vsync_end;
			pr_info("disp_mode[%d].vbp = %d\n", ctx->disp_mode_num, read_value);
		} else {
			pr_info("error: read vbp error!\n");
			return -1;
		}

		ret = of_property_read_u32(mode_node, PANEL_CLOCK_NODE_NAME, &read_value);
		if (!ret) {
			ctx->lcm_disp_mode[ctx->disp_mode_num].clock = read_value;
			pr_info("disp_mode[%d].clock = %d\n", ctx->disp_mode_num, read_value);
		} else {
			pr_info("error: read clock error!\n");
			return -1;
		}

		ret = of_property_read_u32(mode_node, PANEL_WIDTH_MM_NODE_NAME, &read_value);
		if (!ret) {
			ctx->lcm_disp_mode[ctx->disp_mode_num].width_mm = read_value;
			pr_info("disp_mode[%d].width_mm = %d\n", ctx->disp_mode_num, read_value);
		} else {
			pr_info("width_mm not set!\n");
			//return -1;
		}

		ret = of_property_read_u32(mode_node, PANEL_HEIGHT_MM_NODE_NAME, &read_value);
		if (!ret) {
			ctx->lcm_disp_mode[ctx->disp_mode_num].height_mm = read_value;
			pr_info("disp_mode[%d].height_mm = %d\n", ctx->disp_mode_num, read_value);
		} else {
			pr_info("height_mm not set!\n");
			//return -1;
		}

		ctx->disp_mode_num++;
	}
	of_node_put(mode_node);

	pr_info("%s -\n", __func__);
	return 0;
}

static int lcm_get_power_res_from_dts(struct lcm *ctx)
{
	int ret = 0;
	u32 read_value = 0;
	char pwr_name[] = POWER_SUPPLY_NODE_NAME;
	const char *read_str;
	struct device_node *pwr_node = NULL;

	pr_info("%s +\n", __func__);
	ctx->lcm_pwr_num = 0;

	while ((pwr_node = of_get_next_node_by_name(ctx->dev->of_node, pwr_node, pwr_name))) {
		pr_info("%s find!\n", pwr_node->name);

		if (!of_property_read_string(pwr_node, POWER_NAME_NODE_NAME,  &read_str)) {
			strncpy(ctx->lcm_pwr[ctx->lcm_pwr_num].name, read_str, strlen(read_str));
			pr_info("pwr_info[%d].name = %s!\n",
				ctx->lcm_pwr_num, ctx->lcm_pwr[ctx->lcm_pwr_num].name);
		} else {
			pr_info("error: read pwr-name error!\n");
			return -1;
		}

		if (!of_property_read_string(pwr_node, POWER_CTRL_MODE, &read_str)) {
			ctx->lcm_pwr[ctx->lcm_pwr_num].mode =
				(strcmp(read_str, POWER_TYPE_GPIO) == 0)
				? PWR_MODE_GPIO : (strcmp(read_str, POWER_TYPE_PMIC) == 0)
				? PWR_MODE_PMIC : PWR_MODE_REG;
			pr_info("pwr_info[%d].mode = %d!\n",
				ctx->lcm_pwr_num, ctx->lcm_pwr[ctx->lcm_pwr_num].mode);
		} else {
			pr_info("error: read pwr-ctrl-mode error!\n");
			return -1;
		}

		if (ctx->lcm_pwr[ctx->lcm_pwr_num].mode == PWR_MODE_REG) {
			ctx->lcm_pwr[ctx->lcm_pwr_num].pwr_reg =
				devm_regulator_get(ctx->dev, ctx->lcm_pwr[ctx->lcm_pwr_num].name);
			if (IS_ERR(ctx->lcm_pwr[ctx->lcm_pwr_num].pwr_reg)) {
				pr_info("get %s-regualtor fail!\n",
					ctx->lcm_pwr[ctx->lcm_pwr_num].name);
				return -EPROBE_DEFER;
			}

			ret = of_property_read_u32(pwr_node, POWER_REG_SET_VOL, &read_value);
			if (!ret) {
				ctx->lcm_pwr[ctx->lcm_pwr_num].set_vol = read_value;
				pr_info("pwr_info[%d].set_vol = %d\n",
					ctx->lcm_pwr_num, read_value);
			} else {
				pr_info("error: read pwr-set-voltage error!\n");
				return -1;
			}
		}

		if (ctx->lcm_pwr[ctx->lcm_pwr_num].mode == PWR_MODE_GPIO) {
			ret = of_property_read_u32(pwr_node, POWER_GPIO_HIGH_LEVEL, &read_value);
			if (!ret) {
				ctx->lcm_pwr[ctx->lcm_pwr_num].gpio_high_level = read_value;
				pr_info("pwr_info[%d].level = %d!\n", ctx->lcm_pwr_num, read_value);
			} else {
				pr_info("error: read pwr-high-level error!\n");
				return -1;
			}

			strcat(ctx->lcm_pwr[ctx->lcm_pwr_num].name, "-gpios");
			ctx->lcm_pwr[ctx->lcm_pwr_num].pwr_gpio =
				devm_fwnode_gpiod_get_index(ctx->dev, of_fwnode_handle(pwr_node),
				ctx->lcm_pwr[ctx->lcm_pwr_num].name, 0,
				ctx->lcm_pwr[ctx->lcm_pwr_num].gpio_high_level == 1
				? GPIOD_OUT_HIGH : GPIOD_OUT_LOW, NULL);
			if (IS_ERR(ctx->lcm_pwr[ctx->lcm_pwr_num].pwr_gpio)) {
				pr_info("get %s Fail!\n", ctx->lcm_pwr[ctx->lcm_pwr_num].name);
				return -1;
			}
		}

		ret = of_property_read_u32(pwr_node, POWER_DELAY_TIME, &read_value);
		if (!ret) {
			ctx->lcm_pwr[ctx->lcm_pwr_num].delay_ms = read_value;
			pr_info("pwr_info[%d].delay_ms = %d\n", ctx->lcm_pwr_num, read_value);
		} else {
			pr_info("error: read pwr-delay_ms error!\n");
			return -1;
		}

		ctx->lcm_pwr_num++;
		pr_info("ctx->lcm_pwr count = %d!\n", ctx->lcm_pwr_num);
	}

	of_node_put(pwr_node);

	pr_info("%s -\n", __func__);
	return 0;
}

static int lcm_get_reset_res_from_dts(struct lcm *ctx)
{
	u32 len = 0, size = 0, ret = 0, i = 0;
	u32 *array;

	pr_info("%s +\n", __func__);

	ctx->reset_gpio = devm_gpiod_get(ctx->dev, RESET_GPIO_NODE_NAME, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		pr_info("error: get reset-gpios fail!\n");
		return -1;
	}

	if (!of_get_property(ctx->dev->of_node, RESET_SEQUENCE_NODE_NAME, &len)) {
		pr_info("error: get reset sequence fail!\n");
		return -1;
	}

	if (len & 0x1) {
		pr_info("error: reset sequence need pair!\n");
		return -1;
	}

	len /= sizeof(u32);
	size = len * sizeof(u32);

	array = kzalloc(size, GFP_KERNEL);
	if (!array)
		return -1;

	ret = of_property_read_u32_array(ctx->dev->of_node, RESET_SEQUENCE_NODE_NAME, array, len);
	if (ret) {
		pr_info("%s:error:  read reset sequence fail!\n", __func__);
		return -1;
	}

	ctx->reset_seq_num = len / 2;
	for (i = 0; i < len / 2; i++) {
		ctx->rst_seq[i].level = array[i * 2];
		ctx->rst_seq[i].delay_ms = array[i * 2 + 1];
	}

	kfree(array);
	pr_info("%s -\n", __func__);
	return 0;
}

static int lcm_regulator_enable(struct regulator *reg, unsigned int vol)
{
	int ret = 0;

	if (!reg) {
		pr_info("error: regulator connot find\n");
		return -1;
	}

	ret = regulator_set_voltage(reg, vol, vol);
	if (ret) {
		pr_info("error: regulator set voltage fail\n");
		return ret;
	}

	if (vol == regulator_get_voltage(reg))
		pr_info("check vol pass!\n");
	else
		pr_info("error: check vol fail!\n");

	ret = regulator_enable(reg);
	if (ret)
		pr_info("error: regulator enable fail\n");

	return ret;
}

static int lcm_regulator_disable(struct regulator *reg)
{
	if (!reg) {
		pr_info("error: regulator connot find\n");
		return -1;
	}

	if (regulator_is_enabled(reg))
		if (regulator_disable(reg))
			pr_info("regulator disable fail\n");

	return 0;
}

int __attribute__ ((weak)) lcm_power_onoff_by_pmic(int on)
{
	pr_info("power control by pmic, must impment func:%s\n", __func__);
	return -1;
}

static void lcm_poweron(struct lcm *ctx)
{
	u32 i = 0;

	for (i = 0; i < ctx->lcm_pwr_num; i++) {
		if (ctx->lcm_pwr[i].mode == PWR_MODE_REG)
			lcm_regulator_enable(ctx->lcm_pwr[i].pwr_reg,
				ctx->lcm_pwr[i].set_vol);
		else if (ctx->lcm_pwr[i].mode == PWR_MODE_GPIO)
			gpiod_set_value(ctx->lcm_pwr[i].pwr_gpio,
				ctx->lcm_pwr[i].gpio_high_level);
		else
			lcm_power_onoff_by_pmic(1);

		mdelay(ctx->lcm_pwr[i].delay_ms);
	}
}

static void lcm_poweroff(struct lcm *ctx)
{
	int i;

	for (i = ctx->lcm_pwr_num - 1; i >= 0; i--) {
		if (ctx->lcm_pwr[i].mode == PWR_MODE_REG)
			lcm_regulator_disable(ctx->lcm_pwr[i].pwr_reg);
		else if (ctx->lcm_pwr[i].mode == PWR_MODE_GPIO)
			gpiod_set_value(ctx->lcm_pwr[i].pwr_gpio,
				!ctx->lcm_pwr[i].gpio_high_level);
		else
			lcm_power_onoff_by_pmic(0);

		mdelay(ctx->lcm_pwr[i].delay_ms);
	}
}

static inline struct lcm *panel_to_lcm(struct drm_panel *panel)
{
	return container_of(panel, struct lcm, panel);
}

static void lcm_panel_init(struct lcm *ctx)
{
	int i = 0;

	pr_info("%s+++\n", __func__);

	for (i = 0; i < ctx->reset_seq_num; i++) {
		gpiod_set_value(ctx->reset_gpio, ctx->rst_seq[i].level);
		mdelay(ctx->rst_seq[i].delay_ms);
	}
	for (i = 0; i < ctx->init_cmd_num; i++)
		lcm_dsi_write(ctx, ctx->lcm_init_cmd[i]);

	pr_info("%s---\n", __func__);
}

static int lcm_unprepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int i = 0;

	pr_info("%s +++\n", __func__);

	if (!ctx->prepared)
		return 0;

	for (i = 0; i < ctx->deinit_cmd_num; i++)
		lcm_dsi_write(ctx, ctx->lcm_deinit_cmd[i]);

	ctx->error = 0;
	ctx->prepared = false;

	gpiod_set_value(ctx->reset_gpio, 0);
	mdelay(20);

	lcm_poweroff(ctx);

	pr_info("%s ---\n", __func__);
	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	pr_info("%s\n", __func__);
	if (ctx->prepared)
		return 0;

	lcm_poweron(ctx);
	lcm_panel_init(ctx);

	ret = ctx->error;
	if (ret < 0) {
		pr_info("error! return\n");
		lcm_unprepare(panel);
	}

	ctx->prepared = true;

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_rst(panel);
#endif
	pr_info("%s---\n", __func__);

	return ret;
}

static int lcm_disable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("%s+++\n", __func__);

	if (!ctx->enabled)
		return 0;

	ctx->enabled = false;

	pr_info("%s---\n", __func__);

	return 0;
}

static int lcm_enable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("%s+++\n", __func__);

	if (ctx->enabled)
		return 0;

	ctx->enabled = true;

	pr_info("%s---\n", __func__);

	return 0;
}

static int panel_ata_check(struct drm_panel *panel)
{
	return 1;
}

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb, void *handle,
				 unsigned int level)
{
	int map_level = 0, i = 0;
	struct drm_panel *panel;
	struct lcm *ctx;

	panel = ((struct mtk_dsi *)dsi)->panel;
	ctx = panel_to_lcm(panel);

	if (!ctx->bl_max_level) {
		pr_info("backlight not control by lcm, return!\n");
		return -1;
	}
	map_level = level * 1024 / ctx->bl_max_level;
	pr_info("%s: level=%d, map_level=%d\n", __func__, level, map_level);

	for (i = 0; i < ctx->bl_cmd_num; i++) {
		if (ctx->lcm_bl_cmd[i].data[0] == 0x51) {
			ctx->lcm_bl_cmd[i].data[1] = map_level >> 8;
			ctx->lcm_bl_cmd[i].data[2] = map_level & 0xFF;
		}
		pr_info("[%x][%x][%x]\n", ctx->lcm_bl_cmd[i].data[0],
			ctx->lcm_bl_cmd[i].data[1], ctx->lcm_bl_cmd[i].data[2]);
		cb(dsi, handle, &ctx->lcm_bl_cmd[i].data[0], ctx->lcm_bl_cmd[i].len);
	}

	return 0;
}

static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct lcm *ctx = panel_to_lcm(panel);

	gpiod_set_value(ctx->reset_gpio, on);

	return 0;
}

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ata_check = panel_ata_check,
};

static int lcm_get_modes(struct drm_panel *panel,
	struct drm_connector *connector)
{
	struct drm_display_mode *mode;
	struct lcm *ctx = panel_to_lcm(panel);
	int i;

	pr_info("%s+++\n", __func__);

	for (i = 0; i < ctx->disp_mode_num; i++) {
		mode = drm_mode_duplicate(connector->dev, &ctx->lcm_disp_mode[i]);
		if (!mode) {
			pr_info("failed to add mode %ux%ux@%u\n",
					ctx->lcm_disp_mode[i].hdisplay,
					ctx->lcm_disp_mode[i].vdisplay,
					drm_mode_vrefresh(&ctx->lcm_disp_mode[i]));
			return -ENOMEM;
		}

		drm_mode_set_name(mode);
		mode->type = DRM_MODE_TYPE_DRIVER;
		if (i == 0)
			mode->type |= DRM_MODE_TYPE_PREFERRED;
		drm_mode_probed_add(connector, mode);
	}
	connector->display_info.width_mm = ctx->lcm_disp_mode[0].width_mm;
	connector->display_info.height_mm = ctx->lcm_disp_mode[0].height_mm;

	pr_info("%s---\n", __func__);

	return ctx->disp_mode_num;
}

static const struct drm_panel_funcs lcm_drm_funcs = {
	.disable = lcm_disable,
	.unprepare = lcm_unprepare,
	.prepare = lcm_prepare,
	.enable = lcm_enable,
	.get_modes = lcm_get_modes,
};

int parse_tag_videolfb(char *lcm_name)
{
	struct device_node *chosen_node;
	struct tag_videolfb *videolfb_tag = NULL;

	chosen_node = of_find_node_by_path("/chosen");

	if (chosen_node) {
		unsigned long size = 0;

		videolfb_tag = (struct tag_videolfb *)of_get_property(
			chosen_node, "atag,videolfb", (int *)&size);
		if (videolfb_tag) {
			strcpy(lcm_name, videolfb_tag->lcmname);
			return 0;
		}
		pr_info("error: [DT][videolfb] videolfb_tag not found\n");
		return -1;
	}

	pr_info("error: of_chosen not found\n");
	return -1;
}

static int is_sub_path_panel(struct mipi_dsi_device *dsi)
{
	return of_property_read_bool(dsi->dev.of_node,
					      SUB_PATH_PANEL_NODE_NAME);
}

static int is_panel_name_match(struct mipi_dsi_device *dsi)
{
	int ret = 0;
	const char *panel_name_cur;
	char panel_name_chosen[MAX_NAME_LEN] = {0};

	ret = parse_tag_videolfb(panel_name_chosen);
	if (!ret) {
		pr_info("chosen panel %s\n", panel_name_chosen);
	} else {
		pr_info("error: read chosen panel name error, used default!\n");
		return 1;
	}

	ret = of_property_read_string(dsi->dev.of_node, "panel-name", &panel_name_cur);
	if (!ret) {
		pr_info("current panel %s\n", panel_name_cur);
		if (strcmp(panel_name_chosen, panel_name_cur) == 0) {
			pr_info("name match! continue!\n");
			return 1;
		}
		pr_info("%s:skip probe due to not for current lcm!\n", __func__);
		return 0;
	}
	pr_info("error: current panel name not set, used default!\n");

	return 1;
}

static int lcm_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
//	struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;
	struct lcm *ctx;
	struct device_node *backlight;
	int ret;
	int i;

	pr_info("%s+, name:%s\n", __func__, dev->of_node->name);

	if (!is_sub_path_panel(dsi)) {
		if (!is_panel_name_match(dsi)) {
			pr_info("%s:skip probe due to not for current lcm!\n", __func__);
			return -ENODEV;
		}
		pr_info("find:%s!\n", dev->of_node->name);
	} else
		pr_info("sub path panel, used default!\n");

	ctx = devm_kzalloc(dev, sizeof(struct lcm), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;
	lcm_get_dsi_setting_from_dts(ctx);
	dsi->lanes = ctx->dsi_set.lane_num;
	dsi->format = ctx->dsi_set.format;
	dsi->mode_flags = ctx->dsi_set.mode_flag;

	backlight = of_parse_phandle(dev->of_node, "backlight", 0);
	if (backlight) {
		ctx->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);

		if (!ctx->backlight)
			return -EPROBE_DEFER;
	}
	ret = lcm_get_init_cmd_from_dts(ctx);
	if (ret) {
		pr_info("%s: get init cmd error:%d\n", __func__, ret);
		return ret;
	}

	ret = lcm_get_deinit_cmd_from_dts(ctx);
	if (ret) {
		pr_info("%s: get deinit cmd error:%d\n", __func__, ret);
		return ret;
	}

	ret = lcm_get_panel_mode_from_dts(ctx);
	if (ret) {
		pr_info("%s: get panel mode error:%d\n", __func__, ret);
		return ret;
	}

	ret = lcm_get_panel_ext_param_from_dts(ctx);
	if (ret) {
		pr_info("%s: get ext param error:%d\n", __func__, ret);
		return ret;
	}

	ret = lcm_get_reset_res_from_dts(ctx);
	if (ret) {
		pr_info("%s: get rst res error:%d\n", __func__, ret);
		return ret;
	}

	ret = lcm_get_backlight_res_from_dts(ctx);
	if (ret) {
		pr_info("get backlight resource fail, not control by lcm\n");
		ext_funcs.set_backlight_cmdq = NULL;
	} else {
		pr_info("add backlight  control by lcm\n");
		ext_funcs.set_backlight_cmdq = lcm_setbacklight_cmdq;
	}

	ret = lcm_get_power_res_from_dts(ctx);
	if (ret) {
		pr_info("%s: get power res error:%d\n", __func__, ret);
		return ret;
	}

	for (i = 0; i < ctx->lcm_pwr_num; i++) {
		if (ctx->lcm_pwr[i].mode == PWR_MODE_REG) {
			ret = lcm_regulator_enable(ctx->lcm_pwr[i].pwr_reg,
				ctx->lcm_pwr[i].set_vol);
			if (ret < 0) {
				dev_info(dev, "lcm power enable fail\n");
				return ret;
			}
		}
	}

	ctx->prepared = true;
	ctx->enabled = true;
	drm_panel_init(&ctx->panel, dev, &lcm_drm_funcs, DRM_MODE_CONNECTOR_DSI);

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);

#if defined(CONFIG_MTK_PANEL_EXT)
	ret = mtk_panel_ext_create(dev, &ctx->lcm_ext_params[0], &ext_funcs, &ctx->panel);
	if (ret < 0) {
		pr_info("%s error!\n", __func__);
		return ret;
	}
#endif

	pr_info("%s-\n", __func__);

	return ret;
}

static int lcm_remove(struct mipi_dsi_device *dsi)
{
	struct lcm *ctx = mipi_dsi_get_drvdata(dsi);
#if defined(CONFIG_MTK_PANEL_EXT)
	struct mtk_panel_ctx *ext_ctx = find_panel_ctx(&ctx->panel);
#endif

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);
#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_detach(ext_ctx);
	mtk_panel_remove(ext_ctx);
#endif

	return 0;
}

static const struct of_device_id lcm_of_match[] = {
	{
	    .compatible = "mtk,general,panel",
	},
	{}
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "panel-mtk-general",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("Henry Tu <henry.tu@mediatek.com>");
MODULE_DESCRIPTION("MTK General LCD Panel Driver");
MODULE_LICENSE("GPL v2");

