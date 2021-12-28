/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (C) 2020 XiaoMi, Inc.
 */

#define pr_fmt(fmt)	"mi-disp-parse:[%s:%d] " fmt, __func__, __LINE__
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>

#include "mi_disp_print.h"
#include "dsi_panel.h"
#include "dsi_parser.h"
#include "mi_panel_id.h"
#include <linux/soc/qcom/smem.h>

#define DEFAULT_MAX_BRIGHTNESS_CLONE 4095
#define SMEM_SW_DISPLAY_LHBM_TABLE 498

int mi_dsi_panel_parse_esd_gpio_config(struct dsi_panel *panel)
{
	int rc = 0;
	struct dsi_parser_utils *utils = &panel->utils;
	struct mi_dsi_panel_cfg *mi_cfg = &panel->mi_cfg;

	mi_cfg->esd_err_irq_gpio = of_get_named_gpio_flags(
			utils->data, "mi,esd-err-irq-gpio",
			0, (enum of_gpio_flags *)&(mi_cfg->esd_err_irq_flags));
	if (gpio_is_valid(mi_cfg->esd_err_irq_gpio)) {
		mi_cfg->esd_err_irq = gpio_to_irq(mi_cfg->esd_err_irq_gpio);
		rc = gpio_request(mi_cfg->esd_err_irq_gpio, "esd_err_irq_gpio");
		if (rc)
			DISP_ERROR("Failed to request esd irq gpio %d, rc=%d\n",
				mi_cfg->esd_err_irq_gpio, rc);
		else
			gpio_direction_input(mi_cfg->esd_err_irq_gpio);
	} else {
		rc = -EINVAL;
	}

	return rc;
}

static void mi_dsi_panel_parse_round_corner_config(struct dsi_panel *panel)
{
	struct dsi_parser_utils *utils = &panel->utils;
	struct mi_dsi_panel_cfg *mi_cfg = &panel->mi_cfg;

	mi_cfg->ddic_round_corner_enabled =
			utils->read_bool(utils->data, "mi,ddic-round-corner-enabled");
	if (mi_cfg->ddic_round_corner_enabled)
		DISP_INFO("mi,ddic-round-corner-enabled is defined\n");
}

static void mi_dsi_panel_parse_lhbm_config(struct dsi_panel *panel)
{
	int rc = 0;
	int i  = 0, tmp = 0;
	size_t item_size;
	void *lhbm_ptr = NULL;

	struct dsi_parser_utils *utils = &panel->utils;
	struct mi_dsi_panel_cfg *mi_cfg = &panel->mi_cfg;

	mi_cfg->local_hbm_enabled =
			utils->read_bool(utils->data, "mi,local-hbm-enabled");
	if (mi_cfg->local_hbm_enabled)
		DISP_INFO("local hbm feature enabled\n");

	rc = utils->read_u32(utils->data, "mi,local-hbm-ui-ready-delay-num-frame",
			&mi_cfg->lhbm_ui_ready_delay_frame);
	if (rc)
		mi_cfg->lhbm_ui_ready_delay_frame = 0;
	DISP_INFO("local hbm ui_ready delay %d frame\n",
			mi_cfg->lhbm_ui_ready_delay_frame);

	mi_cfg->need_fod_animal_in_normal =
			utils->read_bool(utils->data, "mi,need-fod-animal-in-normal-enabled");
	if (mi_cfg->need_fod_animal_in_normal)
		DISP_INFO("need fod animal in normal enabled\n");


	mi_cfg->lhbm_g500_update_flag =
			utils->read_bool(utils->data, "mi,local-hbm-green-500nit-update-flag");
	if (mi_cfg->lhbm_g500_update_flag)
		DISP_INFO("mi,local-hbm-green-500nit-update-flag\n");

	mi_cfg->lhbm_w1000_update_flag =
			utils->read_bool(utils->data, "mi,local-hbm-white-1000nit-update-flag");
	if (mi_cfg->lhbm_w1000_update_flag)
		DISP_INFO("mi,local-hbm-white-1000nit-update-flag\n");

	mi_cfg->lhbm_w110_update_flag =
			utils->read_bool(utils->data, "mi,local-hbm-white-110nit-update-flag");
	if (mi_cfg->lhbm_w110_update_flag)
		DISP_INFO("mi,local-hbm-white-110nit-update-flag\n");

	mi_cfg->lhbm_alpha_ctrlaa =
			utils->read_bool(utils->data, "mi,local-hbm-alpha-ctrl-aa-area");
	if (mi_cfg->lhbm_alpha_ctrlaa)
		DISP_INFO("mi,local-hbm-alpha-ctrl-aa-area\n");

	rc = utils->read_u32(utils->data, "mi,fod-low-brightness-clone-threshold",
			&mi_cfg->fod_low_brightness_clone_threshold);
	if (rc) {
		mi_cfg->fod_low_brightness_clone_threshold = 0;
	}
	DISP_INFO("fod_low_brightness_clone_threshold=%d\n",
			mi_cfg->fod_low_brightness_clone_threshold);

	rc = utils->read_u32(utils->data, "mi,fod-low-brightness-lux-threshold",
			&mi_cfg->fod_low_brightness_lux_threshold);
	if (rc) {
		mi_cfg->fod_low_brightness_lux_threshold = 0;
	}
	DISP_INFO("fod_low_brightness_lux_threshold=%d\n", mi_cfg->fod_low_brightness_lux_threshold);

	if (mi_get_panel_id(panel->mi_cfg.mi_panel_id) == L3_PANEL_PA) {
		lhbm_ptr = qcom_smem_get(QCOM_SMEM_HOST_ANY, SMEM_SW_DISPLAY_LHBM_TABLE, &item_size);
		if (!IS_ERR(lhbm_ptr) && item_size > 0) {
			DSI_INFO("lhbm data size %d\n", item_size);
			memcpy(mi_cfg->lhbm_rgb_param, lhbm_ptr, item_size);
			for (i = 1; i < item_size; i += 2) {
				tmp = ((mi_cfg->lhbm_rgb_param[i-1]) << 8) | mi_cfg->lhbm_rgb_param[i];
				DSI_INFO("index %d = 0x%04X\n", i, tmp);
				if (tmp == 0x0000) {
					DSI_INFO("uefi read lhbm data failed, need kernel read!\n");
					mi_cfg->uefi_read_lhbm_success = false;
					break;
				} else {
					mi_cfg->uefi_read_lhbm_success = true;
				}
			}
		}
	}

}

static void mi_dsi_panel_parse_flat_config(struct dsi_panel *panel)
{
	struct dsi_parser_utils *utils = &panel->utils;
	struct mi_dsi_panel_cfg *mi_cfg = &panel->mi_cfg;

	mi_cfg->flat_sync_te = utils->read_bool(utils->data, "mi,flat-need-sync-te");
	if (mi_cfg->flat_sync_te)
		DISP_INFO("mi,flat-need-sync-te is defined\n");
	else
		DISP_DEBUG("mi,flat-need-sync-te is undefined\n");

#ifdef DISPLAY_FACTORY_BUILD
	mi_cfg->flat_sync_te = false;
#endif

	mi_cfg->flat_update_flag = utils->read_bool(utils->data, "mi,flat-update-flag");
	if (mi_cfg->flat_update_flag) {
		DISP_INFO("mi,flat-update-flag is defined\n");
	} else {
		DISP_DEBUG("mi,flat-update-flag is undefined\n");
	}
}

static int mi_dsi_panel_parse_dc_config(struct dsi_panel *panel)
{
	int rc = 0;
	struct dsi_parser_utils *utils = &panel->utils;
	struct mi_dsi_panel_cfg *mi_cfg = &panel->mi_cfg;

	rc = utils->read_u32(utils->data, "mi,mdss-dsi-panel-dc-type", &mi_cfg->dc_type);
	if (rc) {
		mi_cfg->dc_type = 1;
		DISP_INFO("default dc backlight type is %d\n", mi_cfg->dc_type);
	} else {
		DISP_INFO("dc backlight type %d \n", mi_cfg->dc_type);
	}

	mi_cfg->dc_update_flag = utils->read_bool(utils->data, "mi,dc-update-flag");
	if (mi_cfg->dc_update_flag) {
		DISP_INFO("mi,dc-update-flag is defined\n");
	} else {
		DISP_DEBUG("mi,dc-update-flag not defined\n");
	}
	rc = utils->read_u32(utils->data, "mi,mdss-dsi-panel-dc-threshold", &mi_cfg->dc_threshold);
	if (rc) {
		mi_cfg->dc_threshold = 440;
		DISP_INFO("default dc threshold is %d\n", mi_cfg->dc_threshold);
	} else {
		DISP_INFO("dc threshold is %d \n", mi_cfg->dc_threshold);
	}

	return rc;
}

static int mi_dsi_panel_parse_backlight_config(struct dsi_panel *panel)
{
	int rc = 0;
#ifdef DISPLAY_FACTORY_BUILD
	u32 val = 0;
#endif
	struct dsi_parser_utils *utils = &panel->utils;
	struct mi_dsi_panel_cfg *mi_cfg = &panel->mi_cfg;

	mi_cfg->bl_wait_frame = false;
	mi_cfg->bl_enable = true;

	rc = utils->read_u32(utils->data, "mi,panel-on-dimming-delay", &mi_cfg->panel_on_dimming_delay);
	if (rc) {
		mi_cfg->panel_on_dimming_delay = 0;
		DISP_INFO("mi,panel-on-dimming-delay not specified\n");
	} else {
		DISP_INFO("mi,panel-on-dimming-delay is %d\n", mi_cfg->panel_on_dimming_delay);
	}

	rc = utils->read_u32(utils->data, "mi,doze-hbm-dbv-level", &mi_cfg->doze_hbm_dbv_level);
	if (rc) {
		mi_cfg->doze_hbm_dbv_level = 0;
		DISP_INFO("mi,doze-hbm-dbv-level not specified\n");
	} else {
		DISP_INFO("mi,doze-hbm-dbv-level is %d\n", mi_cfg->doze_hbm_dbv_level);
	}

	rc = utils->read_u32(utils->data, "mi,doze-lbm-dbv-level", &mi_cfg->doze_lbm_dbv_level);
	if (rc) {
		mi_cfg->doze_lbm_dbv_level = 0;
		DISP_INFO("mi,doze-lbm-dbv-level not specified\n");
	} else {
		DISP_INFO("mi,doze-lbm-dbv-level is %d\n", mi_cfg->doze_lbm_dbv_level);
	}

	rc = utils->read_u32(utils->data, "mi,max-brightness-clone", &mi_cfg->max_brightness_clone);
	if (rc) {
		mi_cfg->max_brightness_clone = DEFAULT_MAX_BRIGHTNESS_CLONE;
	}
	DISP_INFO("max_brightness_clone=%d\n", mi_cfg->max_brightness_clone);

	rc = utils->read_u32(utils->data, "mi,normal-max-brightness-clone", &mi_cfg->normal_max_brightness_clone);
	if (rc) {
		mi_cfg->normal_max_brightness_clone = DEFAULT_MAX_BRIGHTNESS_CLONE;
	}
	DISP_INFO("normal_max_brightness_clone=%d\n", mi_cfg->normal_max_brightness_clone);

	mi_cfg->thermal_dimming_enabled = utils->read_bool(utils->data, "mi,thermal-dimming-flag");
	if (mi_cfg->thermal_dimming_enabled) {
		DISP_INFO("thermal_dimming enabled\n");
	}

#ifdef DISPLAY_FACTORY_BUILD
	rc = utils->read_u32(utils->data, "mi,mdss-dsi-fac-bl-max-level", &val);
	if (rc) {
		rc = 0;
		DSI_DEBUG("[%s] factory bl-max-level unspecified\n", panel->name);
	} else {
		panel->bl_config.bl_max_level = val;
	}

	rc = utils->read_u32(utils->data, "mi,mdss-fac-brightness-max-level",&val);
	if (rc) {
		rc = 0;
		DSI_DEBUG("[%s] factory brigheness-max-level unspecified\n", panel->name);
	} else {
		panel->bl_config.brightness_max_level = val;
	}
	DISP_INFO("bl_max_level is %d, brightness_max_level is %d\n",
		panel->bl_config.bl_max_level, panel->bl_config.brightness_max_level);
#endif

	return rc;
}

int mi_dsi_panel_parse_config(struct dsi_panel *panel)
{
	int rc = 0;
	struct dsi_parser_utils *utils = &panel->utils;
	struct mi_dsi_panel_cfg *mi_cfg = &panel->mi_cfg;

	rc = utils->read_u64(utils->data, "mi,panel-id", &mi_cfg->mi_panel_id);
	if (rc) {
		mi_cfg->mi_panel_id = 0;
		DISP_INFO("mi,panel-id not specified\n");
	} else {
		DISP_INFO("mi,panel-id is 0x%llx (%s)\n",
			mi_cfg->mi_panel_id, mi_get_panel_id_name(mi_cfg->mi_panel_id));
	}

	mi_dsi_panel_parse_round_corner_config(panel);
	mi_dsi_panel_parse_lhbm_config(panel);
	mi_dsi_panel_parse_flat_config(panel);
	rc = mi_dsi_panel_parse_dc_config(panel);
	rc |= mi_dsi_panel_parse_backlight_config(panel);

	return rc;
}

