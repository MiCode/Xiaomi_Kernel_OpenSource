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

#include "dsi_panel.h"
#include "dsi_parser.h"

#include "mi_disp_print.h"
#include <linux/soc/qcom/smem.h>

#define SMEM_SW_DISPLAY_DEMURA_TABLE 498
#define SMEM_SW_DISPLAY_LHBM_TABLE 499

#define DEFAULT_HBM_BL_MIN_LEVEL 1
#define DEFAULT_HBM_BL_MAX_LEVEL 2047
#define DEFAULT_MAX_BRIGHTNESS_CLONE 4095

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

static int mi_dsi_panel_parse_hbm_51_ctl_config(struct dsi_panel *panel)
{
	int rc = 0;
	struct dsi_parser_utils *utils = &panel->utils;
	struct mi_dsi_panel_cfg *mi_cfg = &panel->mi_cfg;

	mi_cfg->hbm_51_ctl_flag = utils->read_bool(utils->data, "mi,hbm-51-ctl-flag");
	if (mi_cfg->hbm_51_ctl_flag) {
		DISP_INFO("mi,hbm-51-ctl-flag is defined\n");
		rc = utils->read_u32(utils->data, "mi,hbm-on-51-index", &mi_cfg->hbm_on_51_index);
		if (rc) {
			mi_cfg->hbm_on_51_index = -1;
			DISP_INFO("mi,hbm-on-51-index not specified\n");
		} else {
			DISP_INFO("mi,hbm-on-51-index is %d\n", mi_cfg->hbm_on_51_index);
		}

		rc = utils->read_u32(utils->data, "mi,hbm-off-51-index", &mi_cfg->hbm_off_51_index);
		if (rc) {
			mi_cfg->hbm_off_51_index = -1;
			DISP_INFO("mi,hbm-off-51-index not specified\n");
		} else {
			DISP_INFO("mi,hbm-off-51-index is %d\n", mi_cfg->hbm_off_51_index);
		}

		rc = utils->read_u32(utils->data, "mi,hbm-fod-on-51-index", &mi_cfg->hbm_fod_on_51_index);
		if (rc) {
			mi_cfg->hbm_fod_on_51_index = -1;
			DISP_INFO("mi,hbm-fod-on-51-index not specified\n");
		} else {
			DISP_INFO("mi,hbm-fod-on-51-index is %d\n", mi_cfg->hbm_fod_on_51_index);
		}

		rc = utils->read_u32(utils->data, "mi,hbm-fod-off-51-index", &mi_cfg->hbm_fod_off_51_index);
		if (rc) {
			mi_cfg->hbm_fod_off_51_index = -1;
			DISP_INFO("mi,hbm-fod-off-51-index not specified\n");
		} else {
			DISP_INFO("mi,hbm-fod-off-51-index is %d\n", mi_cfg->hbm_fod_off_51_index);
		}

		rc = utils->read_u32(utils->data, "mi,hbm-fod-bl-level", &mi_cfg->hbm_fod_bl_lvl);
		if (rc) {
			mi_cfg->hbm_fod_bl_lvl = DEFAULT_HBM_BL_MAX_LEVEL;
			DISP_INFO("mi,hbm-fod-bl-level not specified, default:%d\n", DEFAULT_HBM_BL_MAX_LEVEL);
		} else {
			DISP_INFO("mi,hbm-fod-bl-level is %d\n", mi_cfg->hbm_fod_bl_lvl);
		}

		rc = utils->read_u32(utils->data, "mi,hbm-bl-min-level", &mi_cfg->hbm_bl_min_lvl);
		if (rc) {
			mi_cfg->hbm_bl_min_lvl = DEFAULT_HBM_BL_MIN_LEVEL;
			DISP_INFO("mi,hbm-bl-min-level not specified, default:%d\n", DEFAULT_HBM_BL_MIN_LEVEL);
		} else {
			DISP_INFO("mi,hbm-bl-min-level is %d\n", mi_cfg->hbm_bl_min_lvl);
		}

		rc = utils->read_u32(utils->data, "mi,hbm-bl-max-level", &mi_cfg->hbm_bl_max_lvl);
		if (rc) {
			mi_cfg->hbm_bl_max_lvl = DEFAULT_HBM_BL_MAX_LEVEL;
			DISP_INFO("mi,hbm-bl-max-level not specified, default:%d\n", DEFAULT_HBM_BL_MAX_LEVEL);
		} else {
			DISP_INFO("mi,hbm-bl-max-level is %d\n", mi_cfg->hbm_bl_max_lvl);
		}

		rc = utils->read_u32(utils->data, "mi,mdss-dsi-panel-hbm-brightness", &mi_cfg->hbm_brightness_flag);
		if (rc) {
			mi_cfg->hbm_brightness_flag = 0;
			DISP_INFO("mi,mdss-dsi-panel-hbm-brightness not specified, default:%d\n", mi_cfg->hbm_brightness_flag);
		} else {
			DISP_INFO("mi,mdss-dsi-panel-hbm-brightness is %d\n", mi_cfg->hbm_brightness_flag);
		}
	} else {
		DISP_DEBUG("mi,hbm-51-ctl-flag not defined\n");
	}

	return rc;
}

static int mi_dsi_panel_parse_local_hbm_config(struct dsi_panel *panel)
{
	int rc = 0;
	struct dsi_parser_utils *utils = &panel->utils;
	struct mi_dsi_panel_cfg *mi_cfg = &panel->mi_cfg;

	mi_cfg->local_hbm_enabled = utils->read_bool(utils->data, "mi,local-hbm-enabled");
	if (mi_cfg->local_hbm_enabled) {
		DISP_INFO("local_hbm_enabled\n");
		rc = utils->read_u32(utils->data, "mi,local-hbm-on-1000nit-51-index", &mi_cfg->local_hbm_on_1000nit_51_index);
		if (rc) {
			mi_cfg->local_hbm_on_1000nit_51_index = -1;
			DISP_INFO("mi,local-hbm-on-1000nit-51-index not specified\n");
		} else {
			DISP_INFO("mi,local-hbm-on-1000nit-51-index is %d\n", mi_cfg->local_hbm_on_1000nit_51_index);
		}

		rc = utils->read_u32(utils->data, "mi,local-hbm-off-to-hbm-51-index", &mi_cfg->local_hbm_off_to_hbm_51_index);
		if (rc) {
			mi_cfg->local_hbm_off_to_hbm_51_index = -1;
			DISP_INFO("mi,local-hbm-off-to-hbm-51-index not specified\n");
		} else {
			DISP_INFO("mi,local-hbm-off-to-hbm-51-index is %d\n", mi_cfg->local_hbm_off_to_hbm_51_index);
		}

		rc = utils->read_u32(utils->data, "mi,local-hbm-off-to-normal-51-index", &mi_cfg->local_hbm_off_to_normal_51_index);
		if (rc) {
			mi_cfg->local_hbm_off_to_normal_51_index = -1;
			DISP_INFO("mi,local-hbm-off-to-normal-51-index not specified\n");
		} else {
			DISP_INFO("mi,local-hbm-off-to-normal-51-index is %d\n", mi_cfg->local_hbm_off_to_normal_51_index);
		}

		rc = utils->read_u32(utils->data, "mi,local-hbm-on-87-index", &mi_cfg->local_hbm_on_87_index);
		if (rc) {
			mi_cfg->local_hbm_on_87_index = -1;
			DISP_INFO("mi,local-hbm-on-87-index not specified\n");
		} else {
			DISP_INFO("mi,local-hbm-on-87-index is %d\n", mi_cfg->local_hbm_on_87_index);
		}

		rc = utils->read_u32(utils->data, "mi,local-hbm-hlpm-on-87-index", &mi_cfg->local_hbm_hlpm_on_87_index);
		if (rc) {
			mi_cfg->local_hbm_hlpm_on_87_index = -1;
			DISP_INFO("mi,local-hbm-hlpm-on-87-index not specified\n");
		} else {
			DISP_INFO("mi,local-hbm-hlpm-on-87-index is %d\n", mi_cfg->local_hbm_hlpm_on_87_index);
		}

		rc = utils->read_u32(utils->data, "mi,panel-cup-dbi-reg-index", &mi_cfg->cup_dbi_reg_index);
		if (rc) {
			mi_cfg->cup_dbi_reg_index = -1;
			DISP_INFO("mi,panel-cup-dbi-reg-index not specified\n");
		} else {
			DISP_INFO("mi,panel-cup-dbi-reg-index is %d\n", mi_cfg->cup_dbi_reg_index);
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
	} else {
		DISP_DEBUG("mi,local-hbm-enabled not defined\n");
	}

	return rc;
}

static int mi_dsi_panel_parse_flatmode_config(struct dsi_panel *panel)
{
	int rc = 0;
	struct dsi_parser_utils *utils = &panel->utils;
	struct mi_dsi_panel_cfg *mi_cfg = &panel->mi_cfg;

	mi_cfg->flatmode_update_flag = utils->read_bool(utils->data,
			"mi,flatmode-update-flag");
	if (mi_cfg->flatmode_update_flag) {
		DISP_INFO("mi,flatmode-update-flag is defined\n");
		rc = utils->read_u32(utils->data, "mi,flatmode-on-b9-index",
				&mi_cfg->flatmode_cfg.update_index);
		if (rc) {
			mi_cfg->flatmode_cfg.update_index = -1;
			DISP_INFO("failed to get mi,flatmode-on-b9-index\n");
		}
	} else {
		DISP_DEBUG("mi,flatmode-update-flag not defined\n");
	}

	return rc;
}

static int mi_dsi_panel_parse_dc_config(struct dsi_panel *panel)
{
	int rc = 0;
	struct dsi_parser_utils *utils = &panel->utils;
	struct mi_dsi_panel_cfg *mi_cfg = &panel->mi_cfg;
	u32 index;

	rc = utils->read_u32(utils->data, "mi,mdss-dsi-panel-dc-type", &mi_cfg->dc_type);
	if (rc) {
		mi_cfg->dc_type = 1;
		DISP_INFO("default dc backlight type is %d\n", mi_cfg->dc_type);
	} else {
		DISP_INFO("dc backlight type %d \n", mi_cfg->dc_type);
	}

	rc = utils->read_u32(utils->data, "mi,mdss-dsi-panel-dc-threshold", &mi_cfg->dc_threshold);
	if (rc) {
		mi_cfg->dc_threshold = 440;
		DISP_INFO("default dc backlight type is %d\n", mi_cfg->dc_threshold);
	} else {
		DISP_INFO("dc backlight type %d \n", mi_cfg->dc_threshold);
	}

	mi_cfg->dc_update_flag = utils->read_bool(utils->data, "mi,dc-update-flag");
	if (mi_cfg->dc_update_flag) {
		DISP_INFO("mi,dc-update-flag is defined\n");
		rc = utils->read_u32(utils->data, "mi,dc-on-60hz-d2-index",
				&index);
		if (rc) {
			mi_cfg->dc_cfg.dc_on_index[DC_LUT_60HZ] = -1;
			DISP_ERROR("failed to parse mi,dc-on-60hz-d2-index config\n");
		} else {
			mi_cfg->dc_cfg.dc_on_index[DC_LUT_60HZ] = index;
			DISP_INFO("mi,dc-on-60hz-d2-index is %d\n", index);
		}
		rc = utils->read_u32(utils->data, "mi,dc-off-60hz-d2-index",
				&index);
		if (rc) {
			mi_cfg->dc_cfg.dc_off_index[DC_LUT_60HZ] = -1;
			DISP_ERROR("failed to parse mi,dc-on-60hz-d2-index config\n");
		} else {
			mi_cfg->dc_cfg.dc_off_index[DC_LUT_60HZ] = index;
			DISP_INFO("mi,dc-on-60hz-d2-index is %d\n", index);
		}

		rc = utils->read_u32(utils->data, "mi,dc-on-120hz-d4-index",
				&index);
		if (rc) {
			mi_cfg->dc_cfg.dc_on_index[DC_LUT_120HZ] = -1;
			DISP_ERROR("failed to parse mi,dc-on-120hz-d4-index config\n");
		} else {
			mi_cfg->dc_cfg.dc_on_index[DC_LUT_120HZ] = index;
			DISP_INFO("mi,dc-on-120hz-d4-index is %d\n", index);
		}
		rc = utils->read_u32(utils->data, "mi,dc-off-120hz-d4-index",
				&index);
		if (rc) {
			mi_cfg->dc_cfg.dc_off_index[DC_LUT_120HZ] = -1;
			DISP_ERROR("failed to parse mi,dc-off-120hz-d4-index config\n");
		} else {
			mi_cfg->dc_cfg.dc_off_index[DC_LUT_120HZ] = index;
			DISP_INFO("mi,dc-off-120hz-d4-index is %d\n", index);
		}
	} else {
		DISP_DEBUG("mi,dc-update-flag not defined\n");
	}

	return rc;
}

int mi_dsi_panel_parse_config(struct dsi_panel *panel)
{
	int rc = 0;
	struct dsi_parser_utils *utils = &panel->utils;
	struct mi_dsi_panel_cfg *mi_cfg = &panel->mi_cfg;
	size_t item_size;
	void * demura_ptr = NULL;
	void *lhbm_ptr = NULL;
	mi_cfg->dsi_panel = panel;

	mi_cfg->bl_is_big_endian= utils->read_bool(utils->data,
			"mi,mdss-dsi-bl-dcs-big-endian-type");

	rc = utils->read_u64(utils->data, "mi,panel-id", &mi_cfg->panel_id);
	if (rc) {
		mi_cfg->panel_id = 0;
		DISP_INFO("mi,panel-id not specified\n");
	} else {
		DISP_INFO("mi,panel-id is 0x%llx\n", mi_cfg->panel_id);
		if (mi_cfg->panel_id == 0x4B3800420200) {
			demura_ptr = qcom_smem_get(QCOM_SMEM_HOST_ANY, SMEM_SW_DISPLAY_DEMURA_TABLE, &item_size);
			if (!IS_ERR(demura_ptr) && item_size > 0) {
				DSI_INFO("demura data size %d\n", item_size);
				memcpy(mi_cfg->demura_data, demura_ptr, item_size);
			}
		}
		if (mi_cfg->panel_id == 0x4B394200420200) {
			lhbm_ptr = qcom_smem_get(QCOM_SMEM_HOST_ANY, SMEM_SW_DISPLAY_LHBM_TABLE, &item_size);
			if (!IS_ERR(lhbm_ptr) && item_size > 0) {
				DSI_INFO("lhbm data size %d\n", item_size);
				memcpy(mi_cfg->lhbm_cfg.lhbm_rgb_param, lhbm_ptr, item_size);
			}
		}
	}

	mi_dsi_panel_parse_hbm_51_ctl_config(panel);
	mi_dsi_panel_parse_local_hbm_config(panel);
	mi_dsi_panel_parse_flatmode_config(panel);
	mi_dsi_panel_parse_dc_config(panel);

	rc = utils->read_u32(utils->data, "mi,panel-on-dimming-delay", &mi_cfg->panel_on_dimming_delay);
	if (rc) {
		mi_cfg->panel_on_dimming_delay = 0;
		DISP_INFO("mi,panel-on-dimming-delay not specified\n");
	} else {
		DISP_INFO("mi,panel-on-dimming-delay is %d\n", mi_cfg->panel_on_dimming_delay);
	}

        rc = utils->read_u32(utils->data, "mi,panel-status-check-interval", &mi_cfg->panel_status_check_interval);
        if (rc) {
                mi_cfg->panel_status_check_interval = 5000;
                DISP_INFO("mi,panel-status-check-interval not specified\n");
        } else {
                DISP_INFO("mi,panel-status-check-interval %d\n", mi_cfg->panel_status_check_interval);
        }

	mi_cfg->aod_nolp_command_enabled = utils->read_bool(utils->data, "mi,aod-nolp-command-enabled");
	if (mi_cfg->aod_nolp_command_enabled) {
		DISP_INFO("mi aod-nolp-command-enabled\n");
	}

        mi_cfg->timming_switch_wait_for_te = utils->read_bool(utils->data, "mi,timming-switch-wait-for-te-flag");
        if (mi_cfg->timming_switch_wait_for_te) {
                DISP_INFO("mi timming-switch-wait-for-te-flag get\n");
        }

	mi_cfg->delay_before_fod_hbm_on = utils->read_bool(utils->data, "mi,delay-before-fod-hbm-on");
	if (mi_cfg->delay_before_fod_hbm_on) {
		DISP_INFO("delay before fod hbm on.\n");
	}

	mi_cfg->delay_before_fod_hbm_off = utils->read_bool(utils->data, "mi,delay-before-fod-hbm-off");
	if (mi_cfg->delay_before_fod_hbm_off) {
		DISP_INFO("delay before fod hbm off.\n");
	}

	mi_cfg->dfps_bl_ctrl = utils->read_bool(utils->data, "mi,mdss-dsi-panel-bl-dfps-enabled");
	if (mi_cfg->dfps_bl_ctrl) {
		rc = utils->read_u32(utils->data, "mi,mdss-dsi-panel-bl-dfps-switch-threshold", &mi_cfg->dfps_bl_threshold);
		if (rc) {
			mi_cfg->dfps_bl_threshold = 0;
			DISP_INFO("mi,mdss-dsi-panel-bl-dfps-switch-threshold\n");
		} else {
			DISP_INFO("mi,mdss-dsi-panel-bl-dfps-switch-threshold is %d\n", mi_cfg->dfps_bl_threshold);
		}
	}

	mi_cfg->aod_bl_51ctl = utils->read_bool(utils->data, "mi,aod-bl-51ctl-flag");
	rc = utils->read_u32(utils->data, "mi,aod-hbm-51-index", &mi_cfg->aod_hbm_51_index);
	if (rc) {
		mi_cfg->aod_hbm_51_index = -1;
		DISP_INFO("mi,aod-hbm-51-index not specified\n");
	} else {
		DISP_INFO("mi,aod-hbm-51-index is %d\n", mi_cfg->aod_hbm_51_index);
	}
	rc = utils->read_u32(utils->data, "mi,aod-lbm-51-index", &mi_cfg->aod_lbm_51_index);
	if (rc) {
		mi_cfg->aod_lbm_51_index = -1;
		DISP_INFO("mi,aod-lbm-51-index not specified\n");
	} else {
		DISP_INFO("mi,aod-lbm-51-index is %d\n", mi_cfg->aod_lbm_51_index);
	}

	mi_cfg->fp_display_on_optimize = utils->read_bool(utils->data, "mi,fp-display-on-optimize-flag");
	if (mi_cfg->fp_display_on_optimize) {
		DISP_INFO("fp_display_on_optimize enabled\n");
	}

	mi_cfg->thermal_dimming = utils->read_bool(utils->data, "mi,thermal-dimming-flag");
	if (mi_cfg->thermal_dimming) {
		DISP_INFO("thermal_dimming enabled\n");
	}

	rc = utils->read_u32(utils->data, "mi,fod-low-brightness-clone-threshold", &mi_cfg->fod_low_brightness_clone_threshold);
	if (rc) {
		mi_cfg->fod_low_brightness_clone_threshold = 0;
	}
	DISP_INFO("fod_low_brightness_clone_threshold=%d\n", mi_cfg->fod_low_brightness_clone_threshold);

	rc = utils->read_u32(utils->data, "mi,fod-low-brightness-lux-threshold", &mi_cfg->fod_low_brightness_lux_threshold);
	if (rc) {
		mi_cfg->fod_low_brightness_lux_threshold = 0;
	}
	DISP_INFO("fod_low_brightness_lux_threshold=%d\n", mi_cfg->fod_low_brightness_lux_threshold);

	/* sensor lux init to 50000 to avoid sensor not update value */
	mi_cfg->feature_val[DISP_FEATURE_SENSOR_LUX] = 50000;

	mi_cfg->fod_low_brightness_allow = false;

	rc = utils->read_u32(utils->data, "mi,fod-full-screen-hbm", &mi_cfg->fod_type);
	if (rc) {
		mi_cfg->fod_type = 0;
		DISP_INFO("default fod type is %d\n", mi_cfg->fod_type);
	} else {
		DISP_INFO("fod type %d \n", mi_cfg->fod_type);
	}

	mi_cfg->demura_comp = utils->read_bool(utils->data, "mi,mdss-dsi-panel-bl-demura-enabled");
	if (mi_cfg->demura_comp) {
		mi_cfg->demura_bl_num = utils->count_u32_elems(utils->data, "mi,mdss-dsi-panel-bl-demura");
		if (mi_cfg->demura_bl_num > 0 && mi_cfg->demura_bl_num <= DEMURA_BL_LEVEL_MAX) {
			rc = utils->read_u32_array(utils->data,
					"mi,mdss-dsi-panel-bl-demura", mi_cfg->demura_bl, mi_cfg->demura_bl_num);
			if (rc) {
				mi_cfg->demura_comp = 0;
				mi_cfg->demura_bl_num = 0;
			} else
				DISP_INFO("demura dbv configure\n");
		}
	}
	rc = utils->read_u32(utils->data, "mi,max-brightness-clone", &mi_cfg->max_brightness_clone);
	if (rc) {
		mi_cfg->max_brightness_clone = DEFAULT_MAX_BRIGHTNESS_CLONE;
	}
	DISP_INFO("max_brightness_clone=%d\n", mi_cfg->max_brightness_clone);

	rc = utils->read_u32(utils->data, "mi,aod-exit-delay-time", &mi_cfg->aod_exit_delay_time);
	if (rc)
		mi_cfg->aod_exit_delay_time = 0;

	DISP_INFO("aod exit delay %d\n", mi_cfg->aod_exit_delay_time);

	rc = utils->read_u32(utils->data, "mi,panel-hbm-backlight-threshold", &mi_cfg->hbm_backlight_threshold);
	if (rc)
		mi_cfg->hbm_backlight_threshold = 8192;
	DISP_INFO("panel hbm backlight threshold %d\n", mi_cfg->hbm_backlight_threshold);

	mi_cfg->doze_to_off_command_enabled = utils->read_bool(utils->data,
			"mi,panel-aod-to-off-command-need-enabled");
	if (mi_cfg->doze_to_off_command_enabled) {
		DISP_INFO("mi,panel-aod-to-off-command-need-enabled\n");
	}

	mi_cfg->bl_51ctl_32bit = utils->read_bool(utils->data, "mi,bl-51ctl-32bit-flag");
	rc = utils->read_u32_array(utils->data,
				"mi,aod-brightness", mi_cfg->aod_bl_val, AOD_LEVEL_MAX);
	if (rc)
		memset(mi_cfg->aod_bl_val, 0, AOD_LEVEL_MAX*sizeof(u32));

	mi_cfg->lhbm_update_flag = utils->read_bool(utils->data,
			"mi,lhbm-update-flag");
	if (mi_cfg->lhbm_update_flag) {
		DISP_INFO("mi,lhbm-update-flag\n");
	}

	return rc;
}

