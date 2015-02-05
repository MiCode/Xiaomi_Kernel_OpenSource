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
#include <linux/delay.h>
#include <intel_adf_device.h>
#include <core/common/dsi/dsi_config.h>
#include <core/common/dsi/dsi_pipe.h>
#include <core/common/intel_drrs.h>
#include <core/common/drm_modeinfo_ops.h>

/* Work function for DSI deferred work */
static void intel_mipi_drrs_work_fn(struct work_struct *__work)
{
	struct intel_mipi_drrs_work *work =
		container_of(to_delayed_work(__work),
			struct intel_mipi_drrs_work, work);
	struct intel_pipeline *pipeline = work->pipeline;
	struct dsi_config *config = pipeline->params.dsi.dsi_config;
	struct dsi_pipe *dsi_pipe = container_of(config,
						struct dsi_pipe, config);
	struct adf_drrs *adf_drrs = pipeline->drrs;
	struct dsi_mnp *dsi_mnp;
	struct drm_mode_modeinfo *prev_mode = NULL;
	bool fallback_attempt = false, lock;
	int ret, retry_cnt = 3;

init:
	if (work->target_rr_type == DRRS_HIGH_RR) {
		dsi_mnp = &dsi_pipe->drrs.mnp_list.mnp1;
	} else if (work->target_rr_type == DRRS_LOW_RR) {
		dsi_mnp = &dsi_pipe->drrs.mnp_list.mnp2;
	} else {
		pr_err("ADF: %s: Unknown refreshrate_type\n", __func__);
		return;
	}

	pr_debug("%s: Refresh rate Type: %d-->%d\n", __func__,
				adf_drrs->drrs_state.current_rr_type,
						work->target_rr_type);
	pr_debug("%s: Target RR: %d\n", __func__,
					work->target_mode->vrefresh);

retry:
	if (!dsi_pipe->drrs.platform_ops->configure_dsi_pll) {
		pr_err("ADF: %s: configure_dsi_pll cant be NULL\n", __func__);
		goto out;
	}

	ret = dsi_pipe->drrs.platform_ops->configure_dsi_pll(pipeline, dsi_mnp);
	if (ret == 0) {

		/* PLL Programming is successfull */
		mutex_lock(&adf_drrs->drrs_state.mutex);
		adf_drrs->drrs_state.current_rr_type = work->target_rr_type;
		mutex_unlock(&adf_drrs->drrs_state.mutex);

		pr_info("%s: Refresh Rate set to : %dHz\n", __func__,
						work->target_mode->vrefresh);

		lock = mutex_trylock(&config->ctx_lock);
		config->vbt_mode.vrefresh = work->target_mode->vrefresh;
		config->vbt_mode.clock = work->target_mode->clock;
		if (lock)
			mutex_unlock(&config->ctx_lock);

		/* TODO: Update watermark */

	} else if (ret == -ETIMEDOUT && retry_cnt) {

		/* Timed out. But still attempts are allowed */
		retry_cnt--;
		pr_debug("%s: Retry left ... <%d>\n", __func__, retry_cnt);
		goto retry;
	} else if (ret == -EACCES && !fallback_attempt) {

		/*
		 * PLL Didn't look for the programmed value
		 * fall back to prev mode
		 */
		pr_err("ADF: %s: Falling back to the previous DRRS state. %d->%d\n",
				__func__, work->target_rr_type,
				adf_drrs->drrs_state.current_rr_type);

		mutex_lock(&adf_drrs->drrs_state.mutex);
		adf_drrs->drrs_state.target_rr_type =
					adf_drrs->drrs_state.current_rr_type;
		mutex_unlock(&adf_drrs->drrs_state.mutex);

		work->target_rr_type = adf_drrs->drrs_state.target_rr_type;
		drm_modeinfo_destroy(work->target_mode);

		if (work->target_rr_type == DRRS_HIGH_RR)
			prev_mode = adf_drrs->panel_mode.fixed_mode;
		else if (work->target_rr_type == DRRS_LOW_RR)
			prev_mode = adf_drrs->panel_mode.downclock_mode;

		work->target_mode = drm_modeinfo_duplicate(prev_mode);
		fallback_attempt = true;
		goto init;
	} else {

		/*
		 * All attempts are failed or Fall back
		 * mode all didn't go through.
		 */
		if (fallback_attempt)
			pr_err("ADF: %s: DRRS State Fallback attempt failed\n",
								__func__);
		if (ret == -ETIMEDOUT)
			pr_err("ADF: %s: TIMEDOUT in all retry attempt\n",
								 __func__);
	}

out:
	drm_modeinfo_destroy(work->target_mode);
}

/* Whether DRRS_HR_STATE is pending in the dsi deferred work */
bool intel_dsi_is_drrs_hr_state_pending(struct intel_pipeline *pipeline)
{
	struct dsi_config *config = pipeline->params.dsi.dsi_config;
	struct dsi_pipe *dsi_pipe = container_of(config,
						struct dsi_pipe, config);
	struct intel_mipi_drrs_work *work = dsi_pipe->drrs.mipi_drrs_work;

	if (work_busy(&work->work.work) && work->target_rr_type == DRRS_HIGH_RR)
		return true;
	return false;
}

void intel_dsi_set_drrs_state(struct intel_pipeline *pipeline)
{
	struct adf_drrs *adf_drrs = pipeline->drrs;
	struct dsi_config *config = pipeline->params.dsi.dsi_config;
	struct dsi_pipe *dsi_pipe = container_of(config,
						struct dsi_pipe, config);
	struct drm_mode_modeinfo *target_mode =
				adf_drrs->panel_mode.target_mode;
	struct intel_mipi_drrs_work *work = dsi_pipe->drrs.mipi_drrs_work;
	unsigned int ret;

	ret = work_busy(&work->work.work);
	if (ret) {
		if (work->target_mode)
			if (work->target_mode->vrefresh ==
						target_mode->vrefresh) {
				pr_info("%s: Repeated request for %dHz\n",
					__func__, target_mode->vrefresh);
				return;
			}
		pr_debug("%s: Cancelling an queued/executing work\n", __func__);
		atomic_set(&work->abort_wait_loop, 1);
		cancel_delayed_work_sync(&work->work);
		atomic_set(&work->abort_wait_loop, 0);
		if (ret & WORK_BUSY_PENDING)
			drm_modeinfo_destroy(work->target_mode);

	}
	work->pipeline = pipeline;
	work->target_rr_type = adf_drrs->drrs_state.target_rr_type;
	work->target_mode = drm_modeinfo_duplicate(target_mode);

	schedule_delayed_work(&work->work, 0);
}

/* DSI deferred function init*/
int intel_dsi_drrs_deferred_work_init(struct intel_pipeline *pipeline)
{
	struct dsi_config *config = pipeline->params.dsi.dsi_config;
	struct dsi_pipe *dsi_pipe = container_of(config,
						struct dsi_pipe, config);
	struct intel_mipi_drrs_work *work;


	work = kzalloc(sizeof(struct intel_mipi_drrs_work), GFP_KERNEL);
	if (!work) {
		pr_err("ADF: %s: Failed to allocate mipi DRRS work structure\n",
								__func__);
		return -ENOMEM;
	}

	atomic_set(&work->abort_wait_loop, 0);
	INIT_DELAYED_WORK(&work->work, intel_mipi_drrs_work_fn);
	work->target_mode = NULL;

	dsi_pipe->drrs.mipi_drrs_work = work;
	return 0;
}

/* Based on the VBT's min supported vrefresh, downclock mode will be created */
struct drm_mode_modeinfo *
intel_dsi_calc_panel_downclock(struct adf_drrs *adf_drrs,
			struct drm_mode_modeinfo *fixed_mode)
{
	struct drm_mode_modeinfo *downclock_mode = NULL;

	if (adf_drrs->vbt.drrs_min_vrefresh == 0)
		return downclock_mode;

	/* Allocate */
	downclock_mode = drm_modeinfo_duplicate(fixed_mode);
	if (!downclock_mode) {
		pr_err("ADF: %s: No memory\n", __func__);
		return NULL;
	}

	downclock_mode->vrefresh = adf_drrs->vbt.drrs_min_vrefresh;
	pr_debug("%s: drrs_min_vrefresh = %u\n", __func__,
					downclock_mode->vrefresh);
	downclock_mode->clock = downclock_mode->vrefresh *
		downclock_mode->vtotal * downclock_mode->htotal / 1000;

	return downclock_mode;
}

/* Main init Enrty for DSI DRRS module */
int intel_dsi_drrs_init(struct intel_pipeline *pipeline)
{
	struct adf_drrs *adf_drrs = pipeline->drrs;
	struct dsi_config *config = pipeline->params.dsi.dsi_config;
	struct dsi_pipe *dsi_pipe = container_of(config,
						struct dsi_pipe, config);
	struct drm_mode_modeinfo *fixed_mode, *downclock_mode;
	int ret = 0;

	/* Modes Initialization */
	fixed_mode = drm_modeinfo_duplicate(&config->perferred_mode);
	if (!fixed_mode) {
		pr_err("ADF: %s: Failed to create fixed mode\n", __func__);
		return -ENOMEM;
	}

	if (fixed_mode->clock == 0 && fixed_mode->vrefresh == 0) {
		pr_err("ADF: %s: Invalid preferred_mode\n", __func__);
		return -EINVAL;
	}

	if (fixed_mode->vrefresh == 0)
		fixed_mode->vrefresh = drm_modeinfo_vrefresh(fixed_mode);

	adf_drrs->panel_mode.fixed_mode = fixed_mode;

	pr_debug("%s: Fixed_mode : ", __func__);
	drm_modeinfo_debug_printmodeline(adf_drrs->panel_mode.fixed_mode);

	downclock_mode = intel_dsi_calc_panel_downclock(adf_drrs,
					adf_drrs->panel_mode.fixed_mode);
	if (!downclock_mode) {
		pr_err("ADF: %s: downclock mode not Found\n", __func__);
		ret = -ENOMEM;
		goto out_err_2;
	}
	adf_drrs->panel_mode.downclock_mode = downclock_mode;

	pr_debug("%s: downclock_mode :\n", __func__);
	drm_modeinfo_debug_printmodeline(adf_drrs->panel_mode.downclock_mode);

	adf_drrs->panel_mode.target_mode = NULL;

	if (IS_VALLEYVIEW() || IS_CHERRYVIEW()) {
		dsi_pipe->drrs.platform_ops = vlv_dsi_drrs_ops_init();
	} else {
		pr_err("ADF: %s: Unsupported platform\n", __func__);
		ret = -EINVAL;
		goto out_err;
	}

	if (!dsi_pipe->drrs.platform_ops) {
		pr_err("ADF: %s: DSI platform ops not initialized\n", __func__);
		ret = -EINVAL;
		goto out_err;
	}

	/* Calculate mnp for fixed and downclock modes */
	if (dsi_pipe->drrs.platform_ops->mnp_calculate_for_pclk) {
		ret = dsi_pipe->drrs.platform_ops->mnp_calculate_for_pclk(
				pipeline, &dsi_pipe->drrs.mnp_list.mnp1,
				adf_drrs->panel_mode.fixed_mode->clock);
		if (ret < 0)
			goto out_err;

		ret = dsi_pipe->drrs.platform_ops->mnp_calculate_for_pclk(
				pipeline, &dsi_pipe->drrs.mnp_list.mnp2,
				adf_drrs->panel_mode.downclock_mode->clock);
		if (ret < 0)
			goto out_err;
	} else {
		pr_err("ADF: %s: mnp_calculate_for_pclk is NULL\n", __func__);
		ret = -EINVAL;
		goto out_err;
	}

	ret = intel_dsi_drrs_deferred_work_init(pipeline);
	if (ret < 0)
		goto out_err;

	if (adf_drrs->drrs_state.type == SEAMLESS_DRRS_SUPPORT) {

		/* In DSI SEAMLESS DRRS is a SW driven feature */
		adf_drrs->drrs_state.type = SEAMLESS_DRRS_SUPPORT_SW;

		/* TODO: Expose the DRRS capability */
	}
	return 0;

out_err:
	drm_modeinfo_destroy(adf_drrs->panel_mode.downclock_mode);
out_err_2:
	drm_modeinfo_destroy(adf_drrs->panel_mode.fixed_mode);

	return ret;
}

void intel_dsi_drrs_exit(struct intel_pipeline *pipeline)
{
	struct adf_drrs *adf_drrs = pipeline->drrs;
	struct dsi_config *config = pipeline->params.dsi.dsi_config;
	struct dsi_pipe *dsi_pipe = container_of(config,
						struct dsi_pipe, config);

	kfree(dsi_pipe->drrs.mipi_drrs_work);
	drm_modeinfo_destroy(adf_drrs->panel_mode.downclock_mode);
	drm_modeinfo_destroy(adf_drrs->panel_mode.fixed_mode);

	adf_drrs->drrs_state.type = DRRS_NOT_SUPPORTED;
}

struct drrs_encoder_ops drrs_dsi_ops = {
	.init = intel_dsi_drrs_init,
	.exit = intel_dsi_drrs_exit,
	.set_drrs_state = intel_dsi_set_drrs_state,
	.is_drrs_hr_state_pending = intel_dsi_is_drrs_hr_state_pending,
};

/* Call back Function for Intel_drrs module to get the dsi func ptr */
inline struct drrs_encoder_ops *intel_drrs_dsi_encoder_ops_init(void)
{
	return &drrs_dsi_ops;
}
