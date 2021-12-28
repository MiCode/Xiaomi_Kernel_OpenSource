// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (C) 2020 XiaoMi, Inc.
 */

#define pr_fmt(fmt) "mi_sde_connector:[%s:%d] " fmt, __func__, __LINE__

#include <drm/sde_drm.h>
#include "msm_drv.h"
#include "sde_connector.h"
#include "sde_encoder.h"
#include "sde_trace.h"
#include "dsi_display.h"
#include "dsi_panel.h"

#include "mi_disp_print.h"
#include "mi_disp_feature.h"
#include "mi_dsi_panel.h"
#include "mi_dsi_display.h"
#include "mi_sde_connector.h"
#include "mi_disp_lhbm.h"

static irqreturn_t mi_esd_err_irq_handle(int irq, void *data)
{
	struct sde_connector *c_conn = (struct sde_connector *)data;
	struct dsi_display *display = c_conn ? (c_conn->display) : NULL;
	struct drm_event event;
	int power_mode;

	if (!display || !display->panel) {
		DISP_ERROR("invalid display/panel\n");
		return IRQ_HANDLED;
	}

	DISP_INFO("%s display esd irq trigging \n", display->display_type);

	if (c_conn->connector_type == DRM_MODE_CONNECTOR_DSI) {
		dsi_panel_acquire_panel_lock(display->panel);
		mi_dsi_panel_esd_irq_ctrl_locked(display->panel, false);

		if (!dsi_panel_initialized(display->panel)) {
			DISP_ERROR("%s display panel not initialized!\n",
					display->display_type);
			dsi_panel_release_panel_lock(display->panel);
			return IRQ_HANDLED;
		}

		if (atomic_read(&(display->panel->esd_recovery_pending))) {
			DISP_INFO("%s display ESD recovery already pending\n",
					display->display_type);
			dsi_panel_release_panel_lock(display->panel);
			return IRQ_HANDLED;
		}

		if (!c_conn->panel_dead) {
			atomic_set(&display->panel->esd_recovery_pending, 1);
		} else {
			DISP_INFO("%s display already notify PANEL_DEAD\n",
					display->display_type);
			dsi_panel_release_panel_lock(display->panel);
			return IRQ_HANDLED;
		}

		power_mode = display->panel->power_mode;
		DISP_INFO("%s display, power_mode (%s)\n", display->display_type,
			get_display_power_mode_name(power_mode));

		dsi_panel_release_panel_lock(display->panel);

		if (power_mode == SDE_MODE_DPMS_ON ||
			power_mode == SDE_MODE_DPMS_LP1) {
			_sde_connector_report_panel_dead(c_conn, false);
		} else {
			c_conn->panel_dead = true;
			event.type = DRM_EVENT_PANEL_DEAD;
			event.length = sizeof(bool);
			msm_mode_object_event_notify(&c_conn->base.base,
				c_conn->base.dev, &event, (u8 *)&c_conn->panel_dead);
			SDE_EVT32(SDE_EVTLOG_ERROR);
			DISP_ERROR("%s display esd irq check failed report"
				" PANEL_DEAD conn_id: %d enc_id: %d\n",
				display->display_type,
				c_conn->base.base.id, c_conn->encoder->base.id);
		}
	}

	return IRQ_HANDLED;

}


int mi_sde_connector_register_esd_irq(struct sde_connector *c_conn)
{
	struct dsi_display *display = c_conn ? (c_conn->display) : NULL;
	int rc = 0;

	/* register esd irq and enable it after panel enabled */
	if (c_conn->connector_type == DRM_MODE_CONNECTOR_DSI) {
		if (!display || !display->panel) {
			DISP_ERROR("invalid display/panel\n");
			return -EINVAL;
		}
		if (display->panel->mi_cfg.esd_err_irq_gpio > 0) {
			rc = request_threaded_irq(display->panel->mi_cfg.esd_err_irq,
				NULL, mi_esd_err_irq_handle,
				display->panel->mi_cfg.esd_err_irq_flags,
				"esd_err_irq", c_conn);
			if (rc) {
				DISP_ERROR("%s display register esd irq failed\n",
					display->display_type);
			} else {
				DISP_INFO("%s display register esd irq success\n",
					display->display_type);
				disable_irq(display->panel->mi_cfg.esd_err_irq);
			}
		}
	}

	return rc;
}

int mi_sde_connector_debugfs_esd_sw_trigger(void *display)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;
	struct drm_connector *connector = NULL;
	struct sde_connector *c_conn = NULL;
	struct drm_event event;
	struct dsi_panel *panel;
	int power_mode;

	if (!dsi_display || !dsi_display->panel || !dsi_display->drm_conn) {
		DISP_ERROR("invalid display/panel/drm_conn ptr\n");
		return -EINVAL;
	}

	panel = dsi_display->panel;

	connector = dsi_display->drm_conn;
	c_conn = to_sde_connector(connector);
	if (!c_conn) {
		DISP_ERROR("invalid sde_connector ptr\n");
		return -EINVAL;
	}

	if (!dsi_panel_initialized(dsi_display->panel)) {
		DISP_ERROR("Panel not initialized\n");
		return -EINVAL;
	}

	if (atomic_read(&(dsi_display->panel->esd_recovery_pending))) {
		DISP_INFO("[esd-test]ESD recovery already pending\n");
		return 0;
	}

	if (c_conn->panel_dead) {
		DISP_INFO("panel_dead is true, return!\n");
		return 0;
	}

	dsi_panel_acquire_panel_lock(panel);
	atomic_set(&dsi_display->panel->esd_recovery_pending, 1);
	dsi_panel_release_panel_lock(panel);

	c_conn->panel_dead = true;
	DISP_ERROR("[esd-test]esd irq check failed report PANEL_DEAD conn_id: %d enc_id: %d\n",
			c_conn->base.base.id, c_conn->encoder->base.id);

	power_mode = dsi_display->panel->power_mode;
	DISP_INFO("[esd-test]%s display enabled (%d), power_mode (%s)\n", dsi_display->display_type,
		dsi_display->enabled, get_display_power_mode_name(power_mode));
	if (dsi_display->enabled) {
		if (power_mode == SDE_MODE_DPMS_ON || power_mode == SDE_MODE_DPMS_LP1) {
			sde_encoder_display_failure_notification(c_conn->encoder, false);
		}
	}

	event.type = DRM_EVENT_PANEL_DEAD;
	event.length = sizeof(bool);
	msm_mode_object_event_notify(&c_conn->base.base,
			c_conn->base.dev, &event, (u8 *)&c_conn->panel_dead);

	return 0;
}


int mi_sde_connector_upadate_aod_status(struct drm_connector *connector,
		bool is_aod_exit)
{
	struct sde_connector *c_conn;
	struct sde_encoder_virt *sde_enc;
	struct dsi_display *display = NULL;

	if (!connector) {
		DISP_ERROR("invalid connector ptr\n");
		return -EINVAL;
	}

	c_conn = to_sde_connector(connector);
	sde_enc = to_sde_encoder_virt(connector->encoder);

	/* AOD set backlight logical */
	if (connector->connector_type == DRM_MODE_CONNECTOR_DSI) {
		display = (struct dsi_display *)c_conn->display;
		if (display) {
			if (is_aod_exit) {
				if (c_conn->lp_mode == SDE_MODE_DPMS_ON)
					display->panel->mi_cfg.bl_enable = true;
				if (!display->panel->mi_cfg.bl_wait_frame
					&& c_conn->lp_mode == SDE_MODE_DPMS_ON) {
					sde_enc->ready_kickoff = true;
					if (sde_enc->prepare_kickoff) {
						SDE_ATRACE_BEGIN("set_backlight_after_aod");
						dsi_panel_acquire_panel_lock(display->panel);
						dsi_panel_update_backlight(display->panel,
							display->panel->mi_cfg.last_bl_level);
						dsi_panel_release_panel_lock(display->panel);
						SDE_ATRACE_END("set_backlight_after_aod");
						DISP_INFO("backlight %d set after aod layer\n",
							display->panel->mi_cfg.last_bl_level);
						display->panel->mi_cfg.bl_wait_frame = true;
						sde_enc->ready_kickoff = false;
						sde_enc->prepare_kickoff = false;
					}
				} else {
					sde_enc->ready_kickoff = false;
				}
			}
		}
	}

	return 0;
}

int mi_sde_connector_update_layer_state(struct drm_connector *connector,
			u32 mi_layer_flags)
{
	struct sde_connector *c_conn;
	struct sde_encoder_virt *sde_enc;
	struct dsi_display *display = NULL;
	struct mi_layer_flags cur_flags;
	struct mi_layer_flags *last_flags = NULL;

	if (!connector) {
		DISP_ERROR("invalid connector ptr\n");
		return -EINVAL;
	}

	c_conn = to_sde_connector(connector);
	sde_enc = to_sde_encoder_virt(connector->encoder);
	last_flags = &c_conn->mi_layer_state.layer_flags;

	cur_flags.fod_overlay_flag = (mi_layer_flags & MI_LAYER_FOD_HBM_OVERLAY) ? true : false;
	cur_flags.fod_icon_flag = (mi_layer_flags & MI_LAYER_FOD_ICON) ? true : false;
	cur_flags.aod_flag = (mi_layer_flags & MI_LAYER_AOD) ? true : false;
	cur_flags.fod_anim_flag = (mi_layer_flags & MI_LAYER_FOD_ANIM) ? true : false;

	if (cur_flags.fod_overlay_flag != last_flags->fod_overlay_flag)
		DISP_INFO("layer hbm_overlay = %d\n", cur_flags.fod_overlay_flag);

	if (cur_flags.fod_icon_flag != last_flags->fod_icon_flag)
		DISP_INFO("layer gxzw_icon = %d\n", cur_flags.fod_icon_flag);

	if (cur_flags.aod_flag != last_flags->aod_flag)
		DISP_INFO("layer AOD = %d\n", cur_flags.aod_flag);

	if (cur_flags.fod_anim_flag != last_flags->fod_anim_flag) {
		DISP_INFO("layer gxzw_anim = %d\n", cur_flags.fod_anim_flag);
		if (connector->connector_type == DRM_MODE_CONNECTOR_DSI) {
			display = (struct dsi_display *)c_conn->display;
			if (display && mi_disp_lhbm_fod_enabled(display->panel)) {
				mi_disp_lhbm_animal_status_update(display, cur_flags.fod_anim_flag);
			}
		}
	}

	if (connector->connector_type == DRM_MODE_CONNECTOR_DSI) {
		display = (struct dsi_display *)c_conn->display;
		if (display)
			mi_sde_connector_upadate_aod_status(connector, !cur_flags.aod_flag);
	}

	if (connector->connector_type == DRM_MODE_CONNECTOR_DSI) {
		display = (struct dsi_display *)c_conn->display;
		if (display && mi_disp_lhbm_fod_enabled(display->panel)) {
			if (memcmp(&cur_flags, last_flags, sizeof(cur_flags)))
				mi_disp_lhbm_fod_update_layer_state(display, cur_flags);
		}
	}

	c_conn->mi_layer_state.layer_flags = cur_flags;

	return 0;
}

int mi_sde_connector_flat_fence(struct drm_connector *connector)
{
	int rc = 0;
	struct sde_connector *c_conn;
	struct dsi_display *dsi_display;
	struct mi_dsi_panel_cfg *mi_cfg;

	if (!connector) {
		DISP_ERROR("invalid connector ptr\n");
		return -EINVAL;
	}

	c_conn = to_sde_connector(connector);

	if (c_conn->connector_type != DRM_MODE_CONNECTOR_DSI)
		return 0;

	dsi_display = (struct dsi_display *) c_conn->display;
	if (!dsi_display || !dsi_display->panel) {
		DISP_ERROR("invalid display/panel ptr\n");
		return -EINVAL;
	}

	if (mi_get_disp_id(dsi_display->display_type) != MI_DISP_PRIMARY)
		return -EINVAL;

	dsi_panel_acquire_panel_lock(dsi_display->panel);

	mi_cfg = &dsi_display->panel->mi_cfg;

	if (mi_cfg->flat_sync_te) {
		if (mi_cfg->feature_val[DISP_FEATURE_FLAT_MODE] == FEATURE_ON &&
			mi_cfg->flat_cfg.cur_flat_state == FEATURE_OFF) {
			dsi_panel_tx_cmd_set(dsi_display->panel, DSI_CMD_SET_MI_FLAT_MODE_ON);
			sde_encoder_wait_for_event(c_conn->encoder,MSM_ENC_VBLANK);
			dsi_panel_tx_cmd_set(dsi_display->panel, DSI_CMD_SET_MI_FLAT_MODE_SEC_ON);
			mi_cfg->flat_cfg.cur_flat_state = FEATURE_ON;
			DISP_INFO("DSI_CMD_SET_MI_FLAT_MODE_ON sync with te");
		} else if (mi_cfg->feature_val[DISP_FEATURE_FLAT_MODE] == FEATURE_OFF &&
			mi_cfg->flat_cfg.cur_flat_state == FEATURE_ON) {
			dsi_panel_tx_cmd_set(dsi_display->panel, DSI_CMD_SET_MI_FLAT_MODE_OFF);
			sde_encoder_wait_for_event(c_conn->encoder,MSM_ENC_VBLANK);
			dsi_panel_tx_cmd_set(dsi_display->panel, DSI_CMD_SET_MI_FLAT_MODE_SEC_OFF);
			mi_cfg->flat_cfg.cur_flat_state = FEATURE_OFF;
			DISP_INFO("DSI_CMD_SET_MI_FLAT_MODE_OFF sync with te");
		}
	}

	dsi_panel_release_panel_lock(dsi_display->panel);

	return rc;
}


