/*
 * Copyright (C) 2015, Intel Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Author:
 * Ramalingam C <ramalingam.c@intel.com>
 */

#include <drm/i915_drm.h>
#include <intel_adf_device.h>
#include <linux/delay.h>
#include <core/common/intel_drrs.h>
#include <core/common/drm_modeinfo_ops.h>

void intel_set_drrs_state(struct intel_pipeline *pipeline)
{
	struct adf_drrs *drrs = pipeline->drrs;
	struct drrs_info *drrs_state = &drrs->drrs_state;
	struct drm_mode_modeinfo *target_mode;
	int refresh_rate;

	if (!drrs->has_drrs) {
		pr_err("ADF: %s: DRRS is not supported on this pipe\n",
								__func__);
		return;
	}

	target_mode = drrs->panel_mode.target_mode;
	if (target_mode == NULL) {
		pr_err("ADF: %s: target_mode cannot be NULL\n", __func__);
		return;
	}
	refresh_rate = target_mode->vrefresh;

	if (refresh_rate <= 0) {
		pr_err("ADF: %s: Refresh rate shud be positive non-zero.<%d>\n",
							__func__, refresh_rate);
		return;
	}

	if (drrs_state->target_rr_type >= DRRS_MAX_RR) {
		pr_err("ADF: %s: Unknown refresh_rate_type\n", __func__);
		return;
	}

	if (drrs_state->target_rr_type == drrs_state->current_rr_type &&
			drrs_state->current_rr_type != DRRS_MEDIA_RR) {
		pr_info("ADF: %s: Requested for previously set RR. Ignoring\n",
								__func__);
		return;
	}

	/* TODO: If the display is not active return here */

	drrs->encoder_ops->set_drrs_state(pipeline);

	if (drrs_state->type != SEAMLESS_DRRS_SUPPORT_SW) {
		if (drrs_state->current_rr_type == DRRS_MEDIA_RR &&
				drrs_state->target_rr_type == DRRS_HIGH_RR)
			drrs->resume_idleness_detection = true;

		drrs_state->current_rr_type = drrs_state->target_rr_type;

		pr_info("ADF: %s: Refresh Rate set to : %dHz\n", __func__,
								refresh_rate);
	}
}

static inline bool
is_media_playback_drrs_in_progress(struct drrs_info *drrs_state)
{
	return drrs_state->current_rr_type == DRRS_MEDIA_RR ||
			drrs_state->target_rr_type == DRRS_MEDIA_RR;
}

static void intel_idleness_drrs_work_fn(struct work_struct *__work)
{
	struct intel_idleness_drrs_work *work =
		container_of(to_delayed_work(__work),
				struct intel_idleness_drrs_work, work);
	struct intel_pipeline *pipeline = work->pipeline;
	struct adf_drrs *drrs = pipeline->drrs;
	struct drrs_panel_mode *panel_mode = &drrs->panel_mode;

	/* TODO: If DRRS is not supported on clone mode act here */
	if (panel_mode->target_mode != NULL)
		pr_err("ADF: %s: FIXME: We shouldn't be here\n", __func__);

	mutex_lock(&drrs->drrs_state.mutex);
	if (is_media_playback_drrs_in_progress(&drrs->drrs_state)) {
		mutex_unlock(&drrs->drrs_state.mutex);
		return;
	}

	panel_mode->target_mode = panel_mode->downclock_mode;
	drrs->drrs_state.target_rr_type = DRRS_LOW_RR;

	intel_set_drrs_state(work->pipeline);

	panel_mode->target_mode = NULL;
	mutex_unlock(&drrs->drrs_state.mutex);
}

static void intel_cancel_idleness_drrs_work(struct adf_drrs *drrs)
{
	if (drrs->idleness_drrs_work == NULL)
		return;

	cancel_delayed_work_sync(&drrs->idleness_drrs_work->work);
	drrs->panel_mode.target_mode = NULL;
}

static void intel_enable_idleness_drrs(struct intel_pipeline *pipeline)
{
	struct adf_drrs *drrs = pipeline->drrs;
	bool force_enable_drrs = false;

	if (!drrs || !drrs->has_drrs)
		return;

	intel_cancel_idleness_drrs_work(drrs);
	mutex_lock(&drrs->drrs_state.mutex);

	if (is_media_playback_drrs_in_progress(&drrs->drrs_state)) {
		mutex_unlock(&drrs->drrs_state.mutex);
		return;
	}

	/* Capturing the deferred request for disable_drrs */
	if (drrs->drrs_state.type == SEAMLESS_DRRS_SUPPORT_SW &&
				drrs->encoder_ops->is_drrs_hr_state_pending) {
		if (drrs->encoder_ops->is_drrs_hr_state_pending(pipeline))
				force_enable_drrs = true;
	}

	if (drrs->drrs_state.current_rr_type != DRRS_LOW_RR ||
							force_enable_drrs) {
		drrs->idleness_drrs_work->pipeline = pipeline;

		/*
		 * Delay the actual enabling to let pageflipping cease and the
		 * display to settle before starting DRRS
		 */
		schedule_delayed_work(&drrs->idleness_drrs_work->work,
			msecs_to_jiffies(drrs->idleness_drrs_work->interval));
	}
	mutex_unlock(&drrs->drrs_state.mutex);
}

void intel_disable_idleness_drrs(struct intel_pipeline *pipeline)
{
	struct adf_drrs *drrs = pipeline->drrs;
	struct drrs_panel_mode *panel_mode;

	if (!drrs || !drrs->has_drrs)
		return;

	panel_mode = &drrs->panel_mode;

	/* as part of disable DRRS, reset refresh rate to HIGH_RR */
	if (drrs->drrs_state.current_rr_type == DRRS_LOW_RR) {
		intel_cancel_idleness_drrs_work(drrs);

		mutex_lock(&drrs->drrs_state.mutex);
		if (is_media_playback_drrs_in_progress(&drrs->drrs_state)) {
			mutex_unlock(&drrs->drrs_state.mutex);
			return;
		}

		if (panel_mode->target_mode != NULL)
			pr_err("ADF: %s: FIXME: We shouldn't be here\n",
								__func__);

		panel_mode->target_mode = panel_mode->fixed_mode;
		drrs->drrs_state.target_rr_type = DRRS_HIGH_RR;
		intel_set_drrs_state(pipeline);
		panel_mode->target_mode = NULL;
		mutex_unlock(&drrs->drrs_state.mutex);
	}
}

/* Stops and Starts the Idlenes detection */
void intel_restart_idleness_drrs(struct intel_pipeline *pipeline)
{
	struct adf_drrs *drrs = pipeline->drrs;

	if (!drrs || !drrs->has_drrs)
		return;

	if (is_media_playback_drrs_in_progress(&drrs->drrs_state))
		return;

	/* TODO: Find clone mode here and act on it*/

	intel_disable_idleness_drrs(pipeline);

	/* re-enable idleness detection */
	intel_enable_idleness_drrs(pipeline);
}

/*
 * Handles the userspace request for MEDIA_RR.
 */
int intel_media_playback_drrs_configure(struct intel_pipeline *pipeline,
					struct drm_mode_modeinfo *mode)
{
	struct adf_drrs *drrs = pipeline->drrs;
	struct drrs_info *drrs_state = &drrs->drrs_state;
	struct drrs_panel_mode *panel_mode = &drrs->panel_mode;
	int refresh_rate = mode->vrefresh;

	if (!drrs || !drrs->has_drrs) {
		pr_err("ADF: %s: DRRS is not supported\n", __func__);
		return -EPERM;
	}

	if (refresh_rate < panel_mode->downclock_mode->vrefresh &&
			refresh_rate > panel_mode->fixed_mode->vrefresh) {
		pr_err("ADF: %s: Invalid refresh_rate\n", __func__);
		return -EINVAL;
	}

	if (!is_media_playback_drrs_in_progress(&drrs->drrs_state))
		intel_cancel_idleness_drrs_work(drrs);

	mutex_lock(&drrs_state->mutex);

	if (refresh_rate == panel_mode->fixed_mode->vrefresh) {
		if (drrs_state->current_rr_type == DRRS_MEDIA_RR) {

			/* DRRS_MEDIA_RR -> DRRS_HIGH_RR */
			if (panel_mode->target_mode)
				drm_modeinfo_destroy(panel_mode->target_mode);
			panel_mode->target_mode = panel_mode->fixed_mode;
			drrs_state->target_rr_type = DRRS_HIGH_RR;
		} else {

			/*
			 * Invalid Media Playback DRRS request.
			 * Resume the Idleness Detection
			 */
			pr_err("ADF: %s: Invalid Entry req for mode DRRS_MEDIA_RR\n",
								__func__);
			mutex_unlock(&drrs_state->mutex);
			intel_restart_idleness_drrs(pipeline);
			return 0;
		}
	} else {

		/* TODO: Check for cloned mode and respond accordingly */
		drrs_state->target_rr_type = DRRS_MEDIA_RR;

		if (drrs_state->current_rr_type == DRRS_MEDIA_RR) {

			/* Refresh rate change in Media playback DRRS */
			if (refresh_rate == panel_mode->target_mode->vrefresh) {
				pr_debug("ADF: %s: Request for current RR.<%d>\n",
					__func__,
					panel_mode->target_mode->vrefresh);
				mutex_unlock(&drrs_state->mutex);
				return 0;
			}
			panel_mode->target_mode->vrefresh = refresh_rate;
		} else {

			/* Entering MEDIA Playback DRRS state */
			panel_mode->target_mode = drm_modeinfo_duplicate(mode);
		}

		panel_mode->target_mode->clock = mode->vrefresh * mode->vtotal *
							mode->htotal / 1000;
	}

	pr_debug("ADF: %s: cur_rr_type: %d, target_rr_type: %d, target_rr: %d\n",
				__func__, drrs_state->current_rr_type,
				drrs_state->target_rr_type,
				panel_mode->target_mode->vrefresh);

	intel_set_drrs_state(pipeline);
	mutex_unlock(&drrs_state->mutex);

	if (drrs->resume_idleness_detection)
		intel_restart_idleness_drrs(pipeline);
	return 0;
}

/* Idleness detection logic is initialized */
int intel_adf_drrs_idleness_detection_init(struct intel_pipeline *pipeline)
{
	struct intel_idleness_drrs_work *work;

	work = kzalloc(sizeof(struct intel_idleness_drrs_work), GFP_KERNEL);
	if (!work) {
		pr_err("ADF: %s: Failed to allocate DRRS work structure\n",
								__func__);
		return -ENOMEM;
	}

	pipeline->drrs->is_clone = false;
	work->interval = DRRS_IDLENESS_INTERVAL_MS;
	INIT_DELAYED_WORK(&work->work, intel_idleness_drrs_work_fn);

	pipeline->drrs->idleness_drrs_work = work;
	return 0;
}

/*
 * intel_drrs_init : General entry for DRRS Unit. Called for each PIPE.
 */
int intel_drrs_init(struct intel_pipeline *pipeline)
{
	struct adf_drrs *drrs = pipeline->drrs;
	int pipe_type = -1, gen, ret = 0;

	gen = intel_adf_get_platform_id();

	/*
	 * DRRS will be extended to all gen 7+ platforms
	 * gen 8 is considered as CHV here
	 */
	if (gen <= 6 || gen > 8) {
		pr_err("ADF: %s: DRRS is not enabled on Gen %d\n",
							__func__, gen);
		return -EPERM;
	}

	if (!drrs) {
		pipeline->drrs = kzalloc(sizeof(*drrs), GFP_KERNEL);
		if (!pipeline->drrs) {
			pr_err("ADF: %s: adf_drrs allocation failed\n",
								__func__);
			return -ENOMEM;
		}
		drrs = pipeline->drrs;
	} else {
		pr_err("ADF: %s: drrs is already allocated for this pipe\n",
								__func__);
		return -EINVAL;
	}

	if (IS_VALLEYVIEW() || IS_CHERRYVIEW())
		pipe_type = vlv_pipeline_to_pipe_type(pipeline);

	if (pipe_type == INTEL_PIPE_DSI) {
		drrs->encoder_ops = intel_drrs_dsi_encoder_ops_init();
	} else {
		pr_err("ADF: %s: Unsupported PIPE Type\n", __func__);
		ret = -EINVAL;
		goto err_out;
	}

	if (!drrs->encoder_ops) {
		pr_err("ADF: %s: Encoder ops not initialized\n", __func__);
		ret = -EINVAL;
		goto err_out;
	}

	drrs->vbt.drrs_type = intel_get_vbt_drrs_support();

	/*
	 * This min_vrefresh from VBT is essential for MIPI DRRS.
	 * But not for the eDP like panel, where EDID will provide
	 * the min_vrefresh.
	 */
	drrs->vbt.drrs_min_vrefresh = intel_get_vbt_drrs_min_vrefresh();

	/* First check if DRRS is enabled from VBT struct */
	if (drrs->vbt.drrs_type != SEAMLESS_DRRS_SUPPORT) {
		pr_info("ADF: %s: VBT doesn't support SEAMLESS DRRS\n",
								__func__);
		ret = -EPERM;
		goto err_out;
	}

	if (!drrs->encoder_ops->init || !drrs->encoder_ops->exit ||
					!drrs->encoder_ops->set_drrs_state) {
		pr_err("ADF: %s: Essential func ptrs are NULL\n", __func__);
		ret = -EINVAL;
		goto err_out;
	}

	ret = drrs->encoder_ops->init(pipeline);
	if (ret < 0) {
		pr_err("ADF: %s: Encoder DRRS init failed\n", __func__);
		goto err_out;
	}

	ret = intel_adf_drrs_idleness_detection_init(pipeline);
	if (ret < 0) {
		drrs->encoder_ops->exit(pipeline);
		goto err_out;
	}

	/* SEAMLESS DRRS is supported and downclock mode also exist */
	drrs->has_drrs = true;

	mutex_init(&drrs->drrs_state.mutex);

	drrs->resume_idleness_detection = false;
	drrs->drrs_state.type = drrs->vbt.drrs_type;
	drrs->drrs_state.current_rr_type = DRRS_HIGH_RR;
	pr_info("ADF: %s: SEAMLESS DRRS supported on this panel.\n", __func__);
	return 0;

err_out:
	kfree(pipeline->drrs);
	pipeline->drrs = NULL;
	return ret;
}

void intel_drrs_exit(struct intel_pipeline *pipeline)
{
	struct adf_drrs *drrs = pipeline->drrs;

	if (!drrs)
		return;

	drrs->has_drrs = false;

	kfree(drrs->idleness_drrs_work);
	drrs->encoder_ops->exit(pipeline);
	kfree(pipeline->drrs);

}
