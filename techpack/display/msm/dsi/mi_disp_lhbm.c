#include "mi_disp_lhbm.h"

struct disp_lhbm *g_disp_lhbm = NULL;
static atomic_t fod_work_status = ATOMIC_INIT(FOD_WORK_INIT);
static atomic_t touch_current_status = ATOMIC_INIT(-1);
static atomic_t touch_last_status = ATOMIC_INIT(-1);

extern void sde_crtc_fod_ui_ready(struct dsi_display *display, int type, int value);

int mi_get_disp_id(struct dsi_display *display)
{
	if (!strncmp(display->display_type, "primary", 7))
		return MI_DISP_PRIMARY;
	else
		return MI_DISP_SECONDARY;
}

static int mi_disp_fod_thread_create(struct disp_lhbm *dl_ptr, int disp_id)
{
	int ret = 0;
	struct disp_thread *dt_ptr = NULL;
	struct sched_param param = { 0 };

	param.sched_priority = 16;

	dt_ptr = &dl_ptr->fod_thread;
	dt_ptr->dd_ptr = &dl_ptr->d_display[disp_id];

	kthread_init_worker(&dt_ptr->worker);
	dt_ptr->thread = kthread_run(kthread_worker_fn,
			&dt_ptr->worker, "disp_fod:%d", disp_id);

	ret = sched_setscheduler(dt_ptr->thread, SCHED_FIFO, &param);
	if (ret)
		pr_err("%s display thread priority update failed: %d\n", LHBM_TAG, ret);

	if (IS_ERR(dt_ptr->thread)) {
		pr_err("%s failed to create disp_feature kthread\n", LHBM_TAG);
		dt_ptr->thread = NULL;
	}
	pr_info("%s create disp_fod:%d kthread success\n", LHBM_TAG, disp_id);

	return ret;
}

static int mi_get_fod_lhbm_target_brightness(struct dsi_display *display)
{
	int target = LOCAL_LHBM_TARGET_BRIGHTNESS_WHITE_1000NIT;

	if(display->panel->mi_cfg.fp_status == HEART_RATE_START) {
		target = LOCAL_LHBM_TARGET_BRIGHTNESS_GREEN_500NIT;
	} else if (display->panel->mi_cfg.fod_lhbm_low_brightness_enabled && display->panel->mi_cfg.fod_lhbm_low_brightness_allow){
		target = LOCAL_LHBM_TARGET_BRIGHTNESS_WHITE_110NIT;
	}

	if(display->panel->mi_cfg.fp_status == ENROLL_START) {
		target = LOCAL_LHBM_TARGET_BRIGHTNESS_WHITE_1000NIT;
	}

	return target;
}

static int mi_dsi_panel_set_fod_lhbm(struct dsi_panel *panel, int lhbm_target)
{
	int rc = 0;

	if (!panel) {
		pr_err("%s invalid params\n", LHBM_TAG);
		return -EINVAL;
	}
	if (!panel->panel_initialized){
		pr_err("%s Panel not initialized!\n", LHBM_TAG);
		return -ENODEV;
	}

	if (lhbm_target == LOCAL_LHBM_TARGET_BRIGHTNESS_NONE) {
		if (panel->mi_cfg.local_hbm_cur_status == true)	{
			rc = dsi_panel_set_disp_param(panel, DISPPARAM_HBM_FOD_OFF);
		}
	} else {
		if (panel->mi_cfg.local_hbm_cur_status == false) {
			rc = dsi_panel_set_disp_param(panel, DISPPARAM_HBM_FOD_ON|lhbm_target);
			if(panel->mi_cfg.in_aod == true) {
				pr_info("%s in aod status delay 30 ms lhbm on\n", LHBM_TAG);
				mdelay(70);
			}
		}
	}
	return rc;
}

static int mi_sde_connector_fod_lhbm(struct drm_connector *connector, bool from_touch, int fod_btn)
{
	int rc = 0;
	struct sde_connector *c_conn;
	struct dsi_display *display;
	bool btn_down;
	int lhbm_target;
	struct dsi_panel_mi_cfg *mi_cfg;

	if (!connector) {
		pr_err("%s invalid connector ptr\n", LHBM_TAG);
		return -EINVAL;
	}

	c_conn = to_sde_connector(connector);

	if (c_conn->connector_type != DRM_MODE_CONNECTOR_DSI) {
		pr_err("%s not DRM_MODE_CONNECTOR_DSI\n", LHBM_TAG);
		return -EINVAL;
	}

	display = (struct dsi_display *) c_conn->display;
	if (!display || !display->panel) {
		pr_err("%s invalid display/panel ptr\n", LHBM_TAG);
		return -EINVAL;
	}

	if (mi_get_disp_id(display) != MI_DISP_PRIMARY) {
		pr_err("%s not MI_DISP_PRIMARY\n", LHBM_TAG);
		return -EINVAL;
	}

	mi_cfg = &display->panel->mi_cfg;
	btn_down = (fod_btn == 1);

	pr_info("%s dsi_mi_sde_connector_fod_lhbm=%d\n", LHBM_TAG, btn_down);
	if (btn_down) {
		if (!mi_cfg->pending_lhbm_state && !from_touch) {
			rc = -EINVAL;
			pr_info("%s LHBM on from display skip\n", LHBM_TAG);
		} else {
			lhbm_target = mi_get_fod_lhbm_target_brightness(display);
			rc = mi_dsi_panel_set_fod_lhbm(display->panel, lhbm_target);
			display->panel->mi_cfg.lhbm_target = lhbm_target;
			if (!rc) {
				mi_cfg->pending_lhbm_state = 0;
			} else if (rc == -ENODEV) {
				pr_info("%s LHBM on !panel_initialized rc=%d\n", LHBM_TAG, rc);
				mi_cfg->pending_lhbm_state = 1;
			} else {
				pr_err("%s LHBM on failed rc=%d\n", LHBM_TAG, rc);
			}
		}
	} else {
		rc = mi_dsi_panel_set_fod_lhbm(display->panel, LOCAL_LHBM_TARGET_BRIGHTNESS_NONE);
		display->panel->mi_cfg.lhbm_target = LOCAL_LHBM_TARGET_BRIGHTNESS_NONE;
		mi_cfg->pending_lhbm_state = 0;
		if (rc) {
			pr_err("%s LHBM off failed rc=%d\n", LHBM_TAG, rc);
		}
	}

	return rc;
}

static void mi_disp_set_fod_work_handler(struct kthread_work *work)
{
	int rc = 0;
	struct fod_work_data *fod_data = NULL;
	struct disp_work *cur_work = container_of(work,
					struct disp_work, work);

	fod_data = (struct fod_work_data *)(cur_work->data);

	if (!fod_data || !fod_data->display) {
		pr_err("%s invalid params\n", LHBM_TAG);
		return;
	}

	if (fod_data->from_touch) {
		atomic_set(&fod_work_status, FOD_WORK_DOING);
		do {
			pr_debug("%s from touch, current(%d),last(%d)\n",
				LHBM_TAG, atomic_read(&touch_current_status), atomic_read(&touch_last_status));
			atomic_set(&touch_current_status, atomic_read(&touch_last_status));
			if (atomic_read(&touch_current_status) == 0) {
				//mi_sde_connector_fod_lhbm_notify(fod_data->display->drm_conn,
					//atomic_read(&touch_current_status));
				sde_crtc_fod_ui_ready(fod_data->display, 2, atomic_read(&touch_current_status));
			}

			rc = mi_sde_connector_fod_lhbm(fod_data->display->drm_conn, true, atomic_read(&touch_current_status));

			if (atomic_read(&touch_current_status) == 1) {
				if (rc) {
					pr_err("%s LHBM on failed rc=%d, not notify\n", LHBM_TAG, rc);
				} else {
					//mi_sde_connector_fod_lhbm_notify(fod_data->display->drm_conn,
						//atomic_read(&touch_current_status));
					sde_crtc_fod_ui_ready(fod_data->display, 2, atomic_read(&touch_current_status));
				}
			}
		} while (atomic_read(&touch_current_status) != atomic_read(&touch_last_status));
		atomic_set(&fod_work_status, FOD_WORK_DONE);
	} else {
		pr_debug("%s not from touch, fod_btn(%d)\n", LHBM_TAG, fod_data->fod_btn);
		if (fod_data->fod_btn == 0) {
			sde_crtc_fod_ui_ready(fod_data->display, 2, fod_data->fod_btn);
		}

		rc = mi_sde_connector_fod_lhbm(fod_data->display->drm_conn, false, fod_data->fod_btn);

		if (fod_data->fod_btn == 1) {
			if (rc) {
				pr_err("%s LHBM on failed rc=%d, not notify\n", LHBM_TAG, rc);
			} else {
				sde_crtc_fod_ui_ready(fod_data->display, 2, fod_data->fod_btn);
			}
		}
	}

	kfree(cur_work);
	pr_debug("%s fod work handler done\n", LHBM_TAG);
}

int mi_disp_set_fod_queue_work(u32 fod_btn, bool from_touch)
{
	struct disp_lhbm *dl_ptr = g_disp_lhbm;
	struct disp_work *cur_work;
	struct dsi_display *display = NULL;
	struct fod_work_data *fod_data;
	int fp_status = FINGERPRINT_NONE;
	struct dsi_panel_mi_cfg *mi_cfg;
	static bool ignore_fod_btn = false;

#ifdef CONFIG_FACTORY_BUILD
	return 0;
#endif

	if (!dl_ptr) {
		pr_err("%s invalid params\n", LHBM_TAG);
		return -EINVAL;
	}

	display = (struct dsi_display *)dl_ptr->d_display[MI_DISP_PRIMARY].display;
	if (!display || !display->panel) {
		pr_err("%s invalid params\n", LHBM_TAG);
		return -EINVAL;
	}

	mi_cfg = &display->panel->mi_cfg;
	fp_status = mi_cfg->fp_status;

	if (from_touch) {
		if (atomic_read(&fod_work_status) == FOD_WORK_DOING) {
			pr_debug("%s work doing: from touch current(%d), new fod_btn(%d), skip\n",
				LHBM_TAG, atomic_read(&touch_current_status), fod_btn);
			atomic_set(&touch_last_status, fod_btn);
			return 0;
		} else {
			if (ignore_fod_btn) {
				if (fod_btn == 1) {
					return 0;
				} else {
					ignore_fod_btn = false;
					pr_info("%s clear ignore state\n", LHBM_TAG);
					return 0;
				}
			}

			if (atomic_read(&touch_current_status) == fod_btn) {
				pr_debug("%s from touch fod_btn(%d), skip\n", LHBM_TAG, fod_btn);
				return 0;
			} else {
				mutex_lock(&display->display_lock);
				if (display->panel->power_mode == SDE_MODE_DPMS_ON && atomic_read(&touch_current_status) == 0
							&& fod_btn == 1 && !mi_cfg->fod_anim_layer_enabled) {
					pr_info("%s ignore fod_btn due to fod anim is disable!\n", LHBM_TAG);
					ignore_fod_btn = true;
					mutex_unlock(&display->display_lock);
					return 0;
				}
				mutex_unlock(&display->display_lock);

				atomic_set(&touch_last_status, fod_btn);
				pr_debug("%s from touch fod_btn=%d\n", LHBM_TAG, fod_btn);
			}
		}
	}

	fp_status = display->panel->mi_cfg.fp_status;
	if (fp_status == ENROLL_STOP || fp_status == AUTH_STOP || fp_status == HEART_RATE_STOP) {
		if (fod_btn == 1) {
			pr_info("%s fp_state=%d, skip\n", LHBM_TAG, fp_status);
			return 0;
		}
	}

	cur_work = kzalloc(sizeof(*cur_work) + sizeof(*fod_data), GFP_ATOMIC);
	if (!cur_work)
		return -ENOMEM;

	fod_data = (struct fod_work_data *)((u8 *)cur_work + sizeof(struct disp_work));
	fod_data->display = display;
	fod_data->fod_btn = fod_btn;
	fod_data->from_touch = from_touch;

	kthread_init_work(&cur_work->work, mi_disp_set_fod_work_handler);
	cur_work->dd_ptr = &dl_ptr->d_display[MI_DISP_PRIMARY];
	//cur_work->wq = &dl_ptr->fod_pending_wq;
	cur_work->data = fod_data;

	pr_info("%s fod_queue_work: fod_btn(%d)\n", LHBM_TAG, fod_btn);
	kthread_queue_work(&dl_ptr->fod_thread.worker, &cur_work->work);

	return 0;
}

EXPORT_SYMBOL_GPL(mi_disp_set_fod_queue_work);

int mi_disp_lhbm_attach_primary_dsi_display(struct dsi_display *display)
{
	struct disp_lhbm *dl_ptr = NULL;
	struct disp_display *dd_ptr = NULL;
	int ret = 0;

	if (!strncmp(display->display_type, "primary", 7)) {
		dl_ptr = kzalloc(sizeof(struct disp_lhbm), GFP_KERNEL);
		if (!dl_ptr) {
			pr_err("%s can not allocate Buffer\n", LHBM_TAG);
			return -ENOMEM;
		}

		dd_ptr = &dl_ptr->d_display[MI_DISP_PRIMARY];
		dd_ptr->display = (void *)display;

		ret = mi_disp_fod_thread_create(dl_ptr, MI_DISP_PRIMARY);
		if (ret) {
			kfree(dl_ptr);
			pr_err("%s failed to create fod kthread\n", LHBM_TAG);
			goto err_exit;
		}

		g_disp_lhbm = dl_ptr;

		pr_info("%s lhbm attach primary_dsi_display success\n", LHBM_TAG);
	} else {
		pr_debug("%s is not primary_dsi_display\n", LHBM_TAG);
	}

err_exit:
	return ret;
}
