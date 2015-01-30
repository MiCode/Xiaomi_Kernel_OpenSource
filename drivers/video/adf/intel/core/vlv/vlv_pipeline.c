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
#include <drm/drmP.h>
#include <drm/i915_drm.h>

#include <intel_adf.h>
#include <core/intel_dc_config.h>
#include <core/vlv/vlv_dc_config.h>
#include <core/vlv/vlv_dc_regs.h>
#include <core/common/dsi/dsi_pipe.h>
#include <core/common/hdmi/gen_hdmi_pipe.h>
#include <core/common/dp/gen_dp_pipe.h>
#include <core/vlv/vlv_pm.h>
#include <core/vlv/vlv_pll.h>
#include <core/vlv/dpio.h>

#define LINK_TO_DOT_CLK(x) ((x) * 27 * 100)

enum port vlv_get_connected_port(struct intel_pipe *intel_pipe)
{
	struct vlv_pipeline *vlv_pipeline =
		to_vlv_pipeline(intel_pipe->pipeline);

	/*
	 * This function is only for hot pluggable displays,
	 * like HDMI and DP. SO handle only these.
	 */
	if (intel_pipe->type == INTEL_PIPE_HDMI) {
		struct vlv_hdmi_port *port = &vlv_pipeline->port.hdmi_port;
		return port->port_id;
	}

	if (intel_pipe->type == INTEL_PIPE_DP) {
		struct vlv_dp_port *port = &vlv_pipeline->port.dp_port;
		return port->port_id;
	}

	pr_err("ADF: %s: invalid display type\n", __func__);
	return PORT_INVALID;
}

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
	struct vlv_dsi_port *dsi_port = NULL;
	struct dsi_config *config = pipeline->params.dsi.dsi_config;
	struct dsi_pipe *dsi_pipe = NULL;
	struct dsi_context *dsi_ctx = NULL;
	enum port port;
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

		dsi_pipe = &disp->gen.dsi;
		dsi_ctx = &dsi_pipe->config.ctx;
		for_each_dsi_port(port, dsi_ctx->ports) {
			dsi_port = &disp->port.dsi_port[port];
			vlv_dsi_port_set_device_ready(dsi_port);
			vlv_dsi_port_prepare(dsi_port, mode, config);
		}

		/* need to make panel calls so return to common code */
	}

out:
	return err;
}


u32 vlv_port_enable(struct intel_pipeline *pipeline,
		struct drm_mode_modeinfo *mode)
{
	u32 ret = 0;
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct dsi_config *config = pipeline->params.dsi.dsi_config;
	struct dsi_context *intel_dsi = &config->ctx;
	struct vlv_dsi_port *dsi_port = NULL;
	struct vlv_dp_port *dp_port = &disp->port.dp_port;
	enum port port;
	enum pipe pipe = disp->gen.dsi.config.pipe;
	u32 temp;

	if (disp->type == INTEL_PIPE_DSI) {
		if (intel_dsi->dual_link == DSI_DUAL_LINK_FRONT_BACK) {
			temp = REG_READ(VLV_CHICKEN_3);
			temp &= ~PIXEL_OVERLAP_CNT_MASK |
					intel_dsi->pixel_overlap <<
					PIXEL_OVERLAP_CNT_SHIFT;
			REG_WRITE(VLV_CHICKEN_3, temp);
		}

		for_each_dsi_port(port, intel_dsi->ports) {
			dsi_port = &disp->port.dsi_port[port];

			temp = REG_READ(dsi_port->offset);
			temp &= ~LANE_CONFIGURATION_MASK;
			temp &= ~DUAL_LINK_MODE_MASK;

			if (intel_dsi->ports ==
					((1 << PORT_A) | (1 << PORT_C))) {
				temp |= (intel_dsi->dual_link - 1)
						<< DUAL_LINK_MODE_SHIFT;
				temp |= pipe ? LANE_CONFIGURATION_DUAL_LINK_B :
						LANE_CONFIGURATION_DUAL_LINK_A;
			}

			/* DSI PORT */
			ret = vlv_dsi_port_enable(dsi_port, temp);
		}

		if (ret)
			pr_err("ADF: %s Enable DSI port failed\n", __func__);
	} else if ((disp->type == INTEL_PIPE_DP) ||
			(disp->type == INTEL_PIPE_EDP)) {
		chv_dpio_lane_reset_en(pipeline, true);
		vlv_dp_port_enable(dp_port, mode->flags, &pipeline->params);
		ret = vlv_pll_wait_for_port_ready(dp_port->port_id);
		if (ret)
			pr_err("%s:DP Port ready failed\n", __func__);
	} else if (disp->type == INTEL_PIPE_HDMI) {
		/* HDMI pre port enable */
		chv_dpio_hdmi_swing_levels(pipeline, mode->clock * 1000);
		chv_dpio_lane_reset_en(pipeline, true);
		ret = vlv_hdmi_port_enable(&disp->port.hdmi_port);
		if (ret)
			pr_err("ADF: HDMI: %s Enable port failed\n", __func__);

		ret = vlv_pll_wait_for_port_ready(
					disp->port.hdmi_port.port_id);
		if (ret)
			pr_info("ADF: HDMI: %s Port ready failed\n", __func__);
	}

	return ret;
}

u32 vlv_calc_multiplier(struct intel_pipeline *pipeline, u32 dotclock)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	u32 multiplier = 1;
	u32 i = 0;
	u32 temp = 0;

	if ((disp->type != INTEL_PIPE_DP) &&
			(disp->type != INTEL_PIPE_EDP))
		goto calc_exit;

	dotclock = LINK_TO_DOT_CLK(dotclock);

	for (i = 1; i <= 5; i++) {
		/* 3 is not allowed */
		if (i == 3)
			continue;

		temp = dotclock * i;

		if (temp >= 100000) {
			multiplier = i;
			break;
		}
	}

calc_exit:
	return multiplier;
}

/*
 * DSI is a special beast that requires 3 calls to pipeline
 * 1) setup pll : dsi_prepare_on
 * 2) setup port: dsi_pre_pipeline_on
 * 3) enable port, pipe, plane etc : pipeline_on
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
	struct vlv_pll *pll = &disp->pll;
	struct intel_clock clock;
	bool ret = 0;
	u32 err = 0, dotclock = 0, multiplier = 1;
	u8 bpp = 0;

	if (!mode) {
		pr_err("ADF: %s: mode=NULL\n", __func__);
		return -EINVAL;
	}

	pr_info("ADF: %s: mode=%s\n", __func__, mode->name);

	vlv_pll_disable(pll);
	/* pll enable */
	if (disp->type != INTEL_PIPE_DSI) {
		if ((disp->type == INTEL_PIPE_DP) ||
			(disp->type == INTEL_PIPE_EDP)) {
			dotclock = pipeline->params.dp.link_bw;
			mode->clock = dotclock;
			multiplier = vlv_calc_multiplier(pipeline, dotclock);

			/* Multiply by 10000 to conver to KHz */
			dotclock = LINK_TO_DOT_CLK(pipeline->params.dp.link_bw)
							* 10000;
		} else
			/* Convert MHz to KHz */
			dotclock = mode->clock * 1000;

		err = vlv_pll_program_timings(pll, mode, &clock, multiplier);
		if (err)
			pr_err("ADF: %s: clock calculation failed\n", __func__);
		if (disp->type == INTEL_PIPE_HDMI) {
			pr_err("HARDCODING clock for 19x10 HDMI\n");

			clock.p1 = 4;
			clock.p2 = 2;
			clock.m1 = 2;
			clock.m2 = 0x1d2cccc0;
			clock.n = 1;
			clock.vco = 0x9104f;
		}
		chv_dpio_update_clock(pipeline, &clock);
		chv_dpio_update_channel(pipeline);
		err = vlv_pll_enable(pll, mode);
		if (err) {
			pr_err("ADF: %s: clock calculation failed\n", __func__);
			goto out_on;
		}

		chv_dpio_enable_staggering(pipeline, dotclock);
	}

	/* port enable */
	err = vlv_port_enable(pipeline, mode);
	if (err)
		pr_err("ADF: %s: port enable failed\n", __func__);

	/* Program pipe timings */
	switch (disp->type) {
	case INTEL_PIPE_DSI:
		bpp = disp->gen.dsi.config.bpp;
		break;

	case INTEL_PIPE_HDMI:
		bpp = disp->base.params.hdmi.bpp;
		break;

	case INTEL_PIPE_DP:
	case INTEL_PIPE_EDP:
		vlv_pipe_program_m_n(pipe, disp->base.params.dp.m_n);
		bpp = disp->base.params.dp.bpp;
		break;
	}

	err = vlv_pipe_program_timings(pipe, mode, disp->type, bpp);
	if (err)
		pr_err("ADF: %s: program pipe failed\n", __func__);

	/* pipe enable */
	err = vlv_pipe_enable(pipe, mode);
	if (err)
		pr_err("ADF: %s: pipe enable failed\n", __func__);

	/* FIXME: create func to update plane registers */
	err = vlv_pri_update_params(pplane, &plane_params);
	if (err)
		pr_err("ADF: %s: update primary failed\n", __func__);

	ret = vlv_pipe_vblank_on(pipe);
	if (ret != true)
		pr_err("ADF: %s: enable vblank failed\n", __func__);

	/*
	 * FIXME: enable dpst call once dpst is fixed
	 * vlv_dpst_pipeline_on(disp->dpst, mode);
	 */

	/* Program the watermarks */
	vlv_program_pm(pipeline);

out_on:
	return err;
}

u32 vlv_dsi_pre_pipeline_on(struct intel_pipeline *pipeline,
		struct drm_mode_modeinfo *mode)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct vlv_dsi_port *dsi_port = NULL;
	struct dsi_pipe *dsi_pipe = NULL;
	struct dsi_context *dsi_ctx = NULL;
	enum port port;
	u32 err = 0;

	if (disp->type == INTEL_PIPE_DSI) {
		dsi_pipe = &disp->gen.dsi;
		dsi_ctx = &dsi_pipe->config.ctx;
		for_each_dsi_port(port, dsi_ctx->ports) {
			dsi_port = &disp->port.dsi_port[port];
			err = vlv_dsi_port_pre_enable(dsi_port, mode,
				pipeline->params.dsi.dsi_config);
		}
	}

	return err;
}

u32 vlv_dsi_post_pipeline_off(struct intel_pipeline *pipeline)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct vlv_pll *pll = &disp->pll;
	struct vlv_dsi_port *dsi_port = NULL;
	struct dsi_pipe *dsi_pipe = NULL;
	struct dsi_context *dsi_ctx = NULL;
	enum port port;

	dsi_pipe = &disp->gen.dsi;
	dsi_ctx = &dsi_pipe->config.ctx;
	for_each_dsi_port(port, dsi_ctx->ports) {
		dsi_port = &disp->port.dsi_port[port];
		vlv_dsi_port_wait_for_fifo_empty(dsi_port);
		vlv_dsi_port_clear_device_ready(dsi_port);
	}
	vlv_dsi_pll_disable(pll);

	return 0;
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

static inline u32 vlv_port_disable(struct intel_pipeline *pipeline)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct vlv_dsi_port *dsi_port = NULL;
	struct vlv_dp_port *dp_port = NULL;
	struct vlv_hdmi_port *hdmi_port = NULL;
	struct dsi_pipe *dsi_pipe = NULL;
	struct dsi_context *dsi_ctx = NULL;
	enum port port;
	u32 err = 0;

	switch (disp->type) {
	case INTEL_PIPE_DSI:
		dsi_pipe = &disp->gen.dsi;
		dsi_ctx = &dsi_pipe->config.ctx;
		for_each_dsi_port(port, dsi_ctx->ports) {
			dsi_port = &disp->port.dsi_port[port];
			err = vlv_dsi_port_disable(dsi_port,
					pipeline->params.dsi.dsi_config);
		}

		/*
		 * pll is disabled in the next call to
		 * vlv_post_pipeline_off
		 */
		break;
	case INTEL_PIPE_HDMI:
		hdmi_port = &disp->port.hdmi_port;
		err = vlv_hdmi_port_disable(hdmi_port);
		break;
	case INTEL_PIPE_EDP:
	case INTEL_PIPE_DP:
		dp_port =  &disp->port.dp_port;
		err = vlv_dp_port_disable(dp_port);
		chv_dpio_lane_reset_en(pipeline, false);
		break;
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

	if (IS_CHERRYVIEW() && disp->type != INTEL_PIPE_DSI)
		return chv_pipeline_off(pipeline);

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

	err = vlv_pipe_vblank_off(pipe);
	if (err != true) {
		pr_err("ADF: %s: vblank disable failed\n", __func__);
		goto out;
	}

	/* port disable */
	err = vlv_port_disable(pipeline);
	if (err != 0) {
		pr_err("ADF: %s: port disable failed\n", __func__);
		goto out;
	}

	err = vlv_pipe_disable(pipe);
	if (err != 0) {
		pr_err("ADF: %s: pipe disable failed\n", __func__);
		goto out;
	}
	if (disp->type == INTEL_PIPE_DSI)
		goto out;

	/* pll */
	err = vlv_pll_disable(pll);
	if (err != 0) {
		pr_err("ADF: %s: pll disable failed\n", __func__);
		goto out;
	}

	if (disp->type == INTEL_PIPE_HDMI)
		chv_dpio_lane_reset_en(pipeline, false);

	/* FIXME: disable water mark/ddl etc */

out:
	return err;
}

u32 chv_pipeline_off(struct intel_pipeline *pipeline)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct vlv_pipe *pipe = NULL;
	struct vlv_pri_plane *pplane = NULL;
	struct vlv_sp_plane *splane = NULL;
	struct vlv_pll *pll = NULL;
	u32 i = 0;
	u32 err = 0;

	if (!disp)
		return -EINVAL;

	pipe = &disp->pipe;
	pplane = &disp->pplane;

	/* Disable DPST */
	/* FIXME: vlv_dpst_pipeline_off(); */

	for (i = 0; i < 2; i++) {
		splane = &disp->splane[0];
		splane->base.ops->disable(&splane->base);
	}
	pplane->base.ops->disable(&pplane->base);

	/* Also check for pending flip and the vblank off  */
	vlv_pipe_vblank_off(pipe);

	/* port disable */
	err = vlv_port_disable(pipeline);
	if (err)
		pr_err("ADF: %s: port disable failed\n", __func__);

	/* pipe disable */
	err = vlv_pipe_disable(pipe);
	if (err)
		pr_err("ADF: %s: pipe disable failed\n", __func__);

	if (disp->type == INTEL_PIPE_DSI)
		goto out;

	/* pll */
	pll = &disp->pll;
	err = vlv_pll_disable(pll);
	if (err) {
		pr_err("ADF: %s: pll disable failed\n", __func__);
		goto out;
	}

	if (disp->type == INTEL_PIPE_HDMI)
		chv_dpio_lane_reset_en(pipeline, false);


	/* TODO: Disable pm here*/
out:
	pr_debug("%s: exit status %x\n", __func__, err);
	return err;
}

bool vlv_is_vid_mode(struct intel_pipeline *pipeline)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct vlv_dsi_port *dsi_port = NULL;
	struct dsi_pipe *dsi_pipe = NULL;
	struct dsi_context *dsi_ctx = NULL;
	enum port port;
	bool ret = false;

	if (disp->type == INTEL_PIPE_DSI) {
		dsi_pipe = &disp->gen.dsi;
		dsi_ctx = &dsi_pipe->config.ctx;
		for_each_dsi_port(port, dsi_ctx->ports) {
			dsi_port = &disp->port.dsi_port[port];
			ret = vlv_dsi_port_is_vid_mode(dsi_port);
			if (ret)
				return ret;
		}
	}

	return ret;
}

bool vlv_can_be_disabled(struct intel_pipeline *pipeline)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct vlv_dsi_port *dsi_port = NULL;
	struct dsi_pipe *dsi_pipe = NULL;
	struct dsi_context *dsi_ctx = NULL;
	enum port port;
	bool ret = false;

	if (disp->type == INTEL_PIPE_DSI) {
		dsi_pipe = &disp->gen.dsi;
		dsi_ctx = &dsi_pipe->config.ctx;
		for_each_dsi_port(port, dsi_ctx->ports) {
			dsi_port = &disp->port.dsi_port[port];
			ret = vlv_dsi_port_can_be_disabled(dsi_port);
			if (ret)
				return ret;
		}
	}

	return ret;
}

bool vlv_is_screen_connected(struct intel_pipeline *pipeline)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct vlv_dp_port *dp_port = NULL;
	bool ret = false;

	switch (disp->type) {
	case INTEL_PIPE_DSI:
		/* FIXME: call dsi port */
		ret = true;
		break;
	case INTEL_PIPE_DP:
	case INTEL_PIPE_EDP:
		dp_port = &disp->port.dp_port;
		ret = vlv_dp_port_is_screen_connected(dp_port);
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
	struct vlv_pll *pll = &disp->pll;

	if (disp->type == INTEL_PIPE_DSI) {
		dsi_port = &disp->port.dsi_port[pll->port_id];
		vlv_dsi_port_cmd_hs_mode_enable(dsi_port, enable);
	}
}

int vlv_cmd_vc_dcs_write(struct intel_pipeline *pipeline, int channel,
		const u8 *data, int len, enum port port)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct vlv_dsi_port *dsi_port = NULL;
	int err = 0;

	if (disp->type == INTEL_PIPE_DSI) {
		dsi_port = &disp->port.dsi_port[port];
		err = vlv_dsi_port_cmd_vc_dcs_write(dsi_port,
			channel, data, len);
	}

	return err;
}

int vlv_cmd_vc_generic_write(struct intel_pipeline *pipeline, int channel,
			const u8 *data, int len, enum port port)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct vlv_dsi_port *dsi_port = NULL;
	int err = 0;

	if (disp->type == INTEL_PIPE_DSI) {
		dsi_port = &disp->port.dsi_port[port];
		err = vlv_dsi_port_cmd_vc_generic_write(dsi_port,
			channel, data, len);
	}

	return err;
}

int vlv_cmd_vc_dcs_read(struct intel_pipeline *pipeline, int channel,
		u8 dcs_cmd, u8 *buf, int buflen, enum port port)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct vlv_dsi_port *dsi_port = NULL;
	int err = 0;

	if (disp->type == INTEL_PIPE_DSI) {
		dsi_port = &disp->port.dsi_port[port];
		err = vlv_dsi_port_cmd_vc_dcs_read(dsi_port, channel,
			dcs_cmd, buf, buflen);
	}

	return err;
}

int vlv_cmd_vc_generic_read(struct intel_pipeline *pipeline, int channel,
		u8 *reqdata, int reqlen, u8 *buf, int buflen, enum port port)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct vlv_dsi_port *dsi_port = NULL;
	int err = 0;

	if (disp->type == INTEL_PIPE_DSI) {
		dsi_port = &disp->port.dsi_port[port];
		err = vlv_dsi_port_cmd_vc_generic_read(dsi_port, channel,
			reqdata, reqlen, buf, buflen);
	}

	return err;
}

int vlv_cmd_dpi_send_cmd(struct intel_pipeline *pipeline, u32 cmd, bool hs)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct vlv_dsi_port *dsi_port = NULL;
	struct dsi_pipe *dsi_pipe = NULL;
	struct dsi_context *dsi_ctx = NULL;
	enum port port;
	int err = 0;

	dsi_pipe = &disp->gen.dsi;
	dsi_ctx = &dsi_pipe->config.ctx;
	for_each_dsi_port(port, dsi_ctx->ports) {
		dsi_port = &disp->port.dsi_port[port];
		err = vlv_dsi_port_cmd_dpi_send_cmd(dsi_port, cmd, hs);
	}

	return err;
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

u32 vlv_aux_transfer(struct intel_pipeline *pipeline,
		struct dp_aux_msg *msg)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct vlv_dp_port *dp_port = &disp->port.dp_port;
	u32 retry, size;
	u32 err = -EINVAL;

	size = msg->size;

	/*
	 * The specification doesn't give any recommendation on
	 * how often to retry native transactions, so retry 7 times
	 * like for I2C-over-AUX transactions.
	 */
	for (retry = 0; retry < 7; retry++) {
		err = vlv_dp_port_aux_transfer(dp_port, msg);
		if (20 < err) {
			if (err == -EBUSY)
				continue;
			return err;
		}

		switch (msg->reply & DP_AUX_NATIVE_REPLY_MASK) {
		case DP_AUX_NATIVE_REPLY_ACK:
			if (err < size) {
				pr_err("%s: Error ret_%d < size_%d\n",
					__func__, err, size);
				return -EPROTO;
			}
			return err;

		case DP_AUX_NATIVE_REPLY_NACK:
			pr_err("%s:recevied nack\n", __func__);
			return -EIO;

		case DP_AUX_NATIVE_REPLY_DEFER:
			pr_debug("%s:recevied defer\n", __func__);
			usleep_range(400, 500);
			break;
		}
	}
	pr_err("too many retries, giving up\n");
	return -EIO;
}

u32 vlv_set_link_pattern(struct intel_pipeline *pipeline, u8 train_pattern)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct vlv_dp_port *dp_port = &disp->port.dp_port;
	u32 ret = 0;

	ret = vlv_dp_port_set_link_pattern(dp_port, train_pattern);
	if (ret != 0) {
		pr_err("%s: Set link pattern failed\n", __func__);
		goto out_link;
	}

	switch (train_pattern & DP_TRAINING_PATTERN_MASK) {
	case DP_TRAINING_PATTERN_DISABLE:
		chv_dpio_lane_reset_en(pipeline, true);
		break;
	case DP_TRAINING_PATTERN_1:
		chv_dpio_lane_reset_en(pipeline, true);
		break;
	default:
		break;
	}

out_link:
	return ret;

}

u32 vlv_set_signal_levels(struct intel_pipeline *pipeline,
	struct link_params *params)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct vlv_dp_port *dp_port = &disp->port.dp_port;
	u32 deemp = 0, margin = 0;

	if (IS_CHERRYVIEW())
		return chv_set_signal_levels(pipeline, params);

	vlv_dp_port_set_signal_levels(dp_port, params, &deemp, &margin);
	vlv_dpio_signal_levels(pipeline, deemp, margin);

	return 0;
}

u32 chv_set_signal_levels(struct intel_pipeline *pipeline,
	struct link_params *params)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);

	if (disp->type == INTEL_PIPE_DP)
		chv_dpio_signal_levels(pipeline, params->vswing,
			params->preemp);
	else
		chv_dpio_edp_signal_levels(pipeline, params->vswing,
			params->preemp);
	return 0;
}

void vlv_get_adjust_train(struct intel_pipeline *pipeline,
	struct link_params *params)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct vlv_dp_port *dp_port = &disp->port.dp_port;
	vlv_dp_port_get_adjust_train(dp_port, params);
}

void vlv_get_max_vswing_preemp(struct intel_pipeline *pipeline,
	enum vswing_level *max_vswing, enum preemp_level *max_preemp)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct vlv_dp_port *dp_port = &disp->port.dp_port;
	vlv_dp_port_get_max_vswing_preemp(dp_port, max_vswing, max_preemp);
}


u32 vlv_dp_panel_power_seq(struct intel_pipeline *pipeline, bool enable)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct vlv_dp_port *dp_port = &disp->port.dp_port;
	u32 err = 0;

	if (disp->type == INTEL_PIPE_EDP) {
		err = vlv_dp_port_panel_power_seq(dp_port, enable);
		if (enable == false)
			chv_dpio_lane_reset_en(pipeline, false);
	}

	return err;
}

u32 vlv_dp_backlight_seq(struct intel_pipeline *pipeline, bool enable)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct vlv_dp_port *dp_port = &disp->port.dp_port;
	u32 err = 0;

	if (disp->type == INTEL_PIPE_EDP)
		err = vlv_dp_port_backlight_seq(dp_port, enable);
	return err;
}

struct i2c_adapter *vlv_get_i2c_adapter(struct intel_pipeline *pipeline)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct vlv_dp_port *dp_port = &disp->port.dp_port;

	if ((disp->type == INTEL_PIPE_EDP) ||
		(disp->type == INTEL_PIPE_DP))
		return vlv_dp_port_get_i2c_adapter(dp_port);
	else
		return NULL;
}

u32 vlv_dp_set_brightness(struct intel_pipeline *pipeline, int level)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct vlv_dp_port *port = &disp->port.dp_port;

	return vlv_dp_port_set_brightness(port, level);
}

u32 vlv_dp_get_brightness(struct intel_pipeline *pipeline)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct vlv_dp_port *port = &disp->port.dp_port;
	int level;

	level = vlv_dp_port_get_brightness(port);
	return level;

}
