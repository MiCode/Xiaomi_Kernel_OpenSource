/*
 * Copyright (C) 2014, Intel Corporation.
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
 */

#include <linux/kernel.h>
#include <linux/delay.h>

#include <core/vlv/vlv_dc_config.h>
#include <core/common/dsi/dsi_pipe.h>
#include <core/intel_dc_config.h>
#include <intel_adf.h>
#include <core/vlv/vlv_dc_regs.h>
#include <core/common/dsi/dsi_pipe.h>
#include <core/vlv/vlv_pm.h>
#include <drm/drmP.h>
#include <drm/i915_drm.h>

bool vlv_wait_for_vblank(struct intel_pipeline *pipeline)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct vlv_pipe *pipe = &disp->pipe;

	return vlv_pipe_wait_for_vblank(pipe);
}

void vlv_evade_vblank(struct intel_pipeline *pipeline,
	struct drm_mode_modeinfo *mode, bool *wait_for_vblank)
{

	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct vlv_pipe *pipe = &disp->pipe;

	vlv_pipe_evade_vblank(pipe, mode, wait_for_vblank);
}

u32 vlv_num_planes_enabled(struct intel_pipeline *pipeline)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	u32 count = 0, i;

	if (vlv_pri_is_enabled(&disp->pplane))
		count++;

	/* 2 sprite planes for both vlv/chv */
	for (i = 0; i < VLV_NUM_SPRITES; i++) {
		if (vlv_sp_plane_is_enabled(&disp->splane[i]))
			count++;
	}

	return count;
}

bool vlv_update_maxfifo_status(struct intel_pipeline *pipeline, bool enable)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct vlv_pm *pm = &disp->pm;

	return vlv_pm_update_maxfifo_status(pm, enable);

}

void vlv_program_pm(struct intel_pipeline *pipeline)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct vlv_pm *pm = &disp->pm;
	int num_planes;

	num_planes = vlv_num_planes_enabled(pipeline);
	vlv_pm_program_values(pm, num_planes);
}

u32 vlv_dsi_prepare_on(struct intel_pipeline *pipeline,
		struct drm_mode_modeinfo *mode)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct vlv_pll *pll = &disp->pll;
	struct vlv_pipe *pipe = &disp->pipe;
	struct vlv_dsi_port *dsi_port = &disp->port.dsi_port;
	struct dsi_config *config = pipeline->params.dsi.dsi_config;
	u32 err = 0;

	if (disp->type == INTEL_PIPE_DSI) {
		/*
		 * MIPI being special has extra call to pipeline
		 * enable sequence for pll enable is here, later
		 * call will ensure rest of components powered up
		 */

		/* pll enable */
		/* need port, mode for pll enable */
		pll->config = pipeline->params.dsi.dsi_config;

		vlv_dsi_pll_enable(pll, mode);

		err = vlv_pipe_wait_for_pll_lock(pipe);
		if (err)
			goto out;

		vlv_dsi_port_set_device_ready(dsi_port);

		vlv_dsi_port_prepare(dsi_port, mode, config);

		/* need to make panel calls so return to common code */
	}

out:
	return err;
}

u32 vlv_port_enable(struct intel_pipeline *pipeline,
		struct drm_mode_modeinfo *mode)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct vlv_dsi_port *dsi_port = &disp->port.dsi_port;
	struct dsi_config *config = pipeline->params.dsi.dsi_config;
	struct dsi_context *intel_dsi = &config->ctx;

	if (disp->type == INTEL_PIPE_DSI) {
		/* DSI PORT */
		vlv_dsi_port_enable(dsi_port, intel_dsi->port_bits);
		/* enable will be done in next call for dsi */
	}
	return 0;
}

/*
 * DSI is a special beast that requires 3 calls to pipeline
 *	1) setup pll : dsi_prepare_on
 *	2) setup port: dsi_pre_pipeline_on
 *	3) enable port, pipe, plane etc : pipeline_on
 * this is because of the panel calls needed to be performed
 * between these operations and hence we return to common code
 * to make these calls.
 */
u32 vlv_pipeline_on(struct intel_pipeline *pipeline,
		struct drm_mode_modeinfo *mode)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct vlv_pri_plane *pplane = &disp->pplane;
	struct vlv_pipe *pipe = &disp->pipe;
	struct vlv_plane_params plane_params;
	u32 err = 0;

	/* pll enable */
	/* need port, mode for pll enable */

	/* pf enable */

	/* port enable */
	vlv_port_enable(pipeline, mode);
	/* wait for dpio phystatus ready */

	vlv_pipe_program_timings(pipe, mode);

	/* pipe enable */
	vlv_pipe_enable(pipe, mode);

	/* FIXME: create func to update plane registers */
	vlv_pri_update_params(pplane, &plane_params);

	vlv_pipe_vblank_on(pipe);
	if (err != true)
		pr_err("ADF: %s: enable vblank failed\n", __func__);
	else
		/*
		 * Reset the last success value (bool true) to zero else
		 * this will give caller an illusion of failure
		 */
		 err = 0;

	/*
	 * FIXME: enable dpst call once dpst is fixed
	 * vlv_dpst_pipeline_on(disp->dpst, mode);
	 */

	/* Program the watermarks */
	vlv_program_pm(pipeline);

	return err;
}

u32 vlv_dsi_pre_pipeline_on(struct intel_pipeline *pipeline,
		struct drm_mode_modeinfo *mode)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct vlv_dsi_port *port = &disp->port.dsi_port;
	u32 err = 0;

	if (disp->type == INTEL_PIPE_DSI) {
		err = vlv_dsi_port_pre_enable(port, mode,
			pipeline->params.dsi.dsi_config);
	}

	return err;
}

u32 vlv_dsi_post_pipeline_off(struct intel_pipeline *pipeline)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct vlv_pll *pll = &disp->pll;
	struct vlv_dsi_port *dsi_port = &disp->port.dsi_port;
	u32 err = 0;
	err = vlv_dsi_port_wait_for_fifo_empty(dsi_port);

	err = vlv_dsi_port_clear_device_ready(dsi_port);

	err = vlv_dsi_pll_disable(pll);

	return err;
}

/* generic function to be called for any operations after disable is done */
u32 vlv_post_pipeline_off(struct intel_pipeline *pipeline)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	u32 err = 0;

	switch (disp->type) {
	case INTEL_PIPE_DSI:
		err = vlv_dsi_post_pipeline_off(pipeline);
		break;
	case INTEL_PIPE_EDP:
	case INTEL_PIPE_DP:
	default:
		err = -EINVAL;
		break;
	}

	return err;
}

u32 vlv_port_disable(struct intel_pipeline *pipeline)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct vlv_dsi_port *dsi_port = NULL;
	u32 err = 0;

	switch (disp->type) {
	case INTEL_PIPE_DSI:
		dsi_port = &disp->port.dsi_port;
		err = vlv_dsi_port_disable(dsi_port,
			pipeline->params.dsi.dsi_config);
		/*
		 * pll is disabled in the next call to
		 * vlv_post_pipeline_off
		 */
		break;
	case INTEL_PIPE_EDP:
	case INTEL_PIPE_DP:
	default:
		err = -EINVAL;
		break;
	}

	return err;
}

u32 vlv_pipeline_off(struct intel_pipeline *pipeline)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct vlv_pri_plane *pplane = NULL;
	struct vlv_sp_plane *splane = NULL;
	struct vlv_pipe *pipe = NULL;
	struct vlv_pll *pll = NULL;
	u32 err = 0, i = 0;

	/* Disable DPST */
	/* FIXME: vlv_dpst_pipeline_off(); */

	/* Also check for pending flip and the vblank off  */

	pplane = &disp->pplane;
	pipe = &disp->pipe;

	for (i = 0; i < 2; i++) {
		splane = &disp->splane[i];
		splane->base.ops->disable(&splane->base);
	}

	pplane->base.ops->disable(&pplane->base);

	vlv_pipe_vblank_off(pipe);

	/* pipe */
	err = vlv_pipe_disable(pipe);
	if (err != 0)
		goto out;

	/* port disable */
	err = vlv_port_disable(pipeline);
	if (err != 0)
		goto out;

	if (disp->type == INTEL_PIPE_DSI)
		goto out;

	/* pll */
	err = vlv_pll_disable(pll);
	if (err != 0)
		goto out;

	/* TODO: watermark */
out:
	return err;
}

bool vlv_is_vid_mode(struct intel_pipeline *pipeline)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct vlv_dsi_port *dsi_port = NULL;
	bool ret = false;

	if (disp->type == INTEL_PIPE_DSI) {
		dsi_port = &disp->port.dsi_port;
		ret = vlv_dsi_port_is_vid_mode(dsi_port);
	}

	return ret;
}

bool vlv_can_be_disabled(struct intel_pipeline *pipeline)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct vlv_dsi_port *dsi_port = NULL;
	bool ret = false;

	if (disp->type == INTEL_PIPE_DSI) {
		dsi_port = &disp->port.dsi_port;
		ret = vlv_dsi_port_can_be_disabled(dsi_port);
	}

	return ret;
}

bool vlv_is_screen_connected(struct intel_pipeline *pipeline)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	bool ret = false;

	switch (disp->type) {
	case INTEL_PIPE_DSI:
		/* FIXME: call dsi port */
		ret = true;
		break;
	case INTEL_PIPE_DP:
	case INTEL_PIPE_EDP:
		break;
	default:
		break;
	}

	return ret;
}

/* DSI specific calls  */
void vlv_cmd_hs_mode_enable(struct intel_pipeline *pipeline, bool enable)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct vlv_dsi_port *dsi_port = NULL;

	if (disp->type == INTEL_PIPE_DSI) {
		dsi_port = &disp->port.dsi_port;
		vlv_dsi_port_cmd_hs_mode_enable(dsi_port, enable);
	}
}

int vlv_cmd_vc_dcs_write(struct intel_pipeline *pipeline, int channel,
		const u8 *data, int len)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct vlv_dsi_port *dsi_port = NULL;
	int err = 0;

	if (disp->type == INTEL_PIPE_DSI) {
		dsi_port = &disp->port.dsi_port;
		err = vlv_dsi_port_cmd_vc_dcs_write(dsi_port,
			channel, data, len);
	}

	return err;
}

int vlv_cmd_vc_generic_write(struct intel_pipeline *pipeline, int channel,
			const u8 *data, int len)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct vlv_dsi_port *dsi_port = NULL;
	int err = 0;

	if (disp->type == INTEL_PIPE_DSI) {
		dsi_port = &disp->port.dsi_port;
		err = vlv_dsi_port_cmd_vc_generic_write(dsi_port,
			channel, data, len);
	}

	return err;
}

int vlv_cmd_vc_dcs_read(struct intel_pipeline *pipeline, int channel,
		u8 dcs_cmd, u8 *buf, int buflen)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct vlv_dsi_port *dsi_port = NULL;
	int err = 0;

	if (disp->type == INTEL_PIPE_DSI) {
		dsi_port = &disp->port.dsi_port;
		err = vlv_dsi_port_cmd_vc_dcs_read(dsi_port, channel,
			dcs_cmd, buf, buflen);
	}

	return err;
}

int vlv_cmd_vc_generic_read(struct intel_pipeline *pipeline, int channel,
		u8 *reqdata, int reqlen, u8 *buf, int buflen)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct vlv_dsi_port *dsi_port = NULL;
	int err = 0;

	if (disp->type == INTEL_PIPE_DSI) {
		dsi_port = &disp->port.dsi_port;
		err = vlv_dsi_port_cmd_vc_generic_read(dsi_port, channel,
			reqdata, reqlen, buf, buflen);
	}

	return err;
}

int vlv_cmd_dpi_send_cmd(struct intel_pipeline *pipeline, u32 cmd, bool hs)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct vlv_dsi_port *dsi_port = NULL;
	int err = 0;

	if (disp->type == INTEL_PIPE_DSI) {
		dsi_port = &disp->port.dsi_port;
		err = vlv_dsi_port_cmd_dpi_send_cmd(dsi_port, cmd, hs);
	}

	return err;
}

u32 vlv_get_event(struct intel_pipeline *pipeline, u32 *event)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct vlv_pipe *pipe = &disp->pipe;
	struct vlv_pm *pm = &disp->pm;
	u32 ret = 0;

	ret = vlv_pipe_get_event(pipe, event);

	if (ret == 0)
		ret = vlv_pm_flush_values(pm, *event);

	return ret;
}

u32 vlv_set_event(struct intel_pipeline *pipeline, u32 event, bool enabled)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct vlv_pipe *pipe = &disp->pipe;

	return vlv_pipe_set_event(pipe, event, enabled);
}

bool vlv_is_plane_enabled(struct intel_pipeline *pipeline,
		struct intel_plane *plane)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct vlv_pri_plane *pri_plane = &disp->pplane;
	struct vlv_sp_plane *sp1_plane = &disp->splane[0];
	struct vlv_sp_plane *sp2_plane = &disp->splane[1];
	bool ret = false;

	if (&pri_plane->base == plane)
		ret = vlv_pri_is_enabled(pri_plane);
	else if (&sp1_plane->base == plane)
		ret = vlv_sp_plane_is_enabled(sp1_plane);
	else if (&sp2_plane->base == plane)
		ret = vlv_sp_plane_is_enabled(sp2_plane);

	return ret;
}
