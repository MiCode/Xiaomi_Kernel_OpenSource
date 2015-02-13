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

#include <linux/pci.h>
#include <core/intel_dc_config.h>
#include <core/common/dsi/dsi_pipe.h>
#include <core/common/hdmi/gen_hdmi_pipe.h>
#include <core/common/dp/gen_dp_pipe.h>
#include <core/vlv/vlv_dc_config.h>
#include <core/vlv/vlv_pri_plane.h>
#include <core/vlv/vlv_sp_plane.h>
#include <core/vlv/chv_dc_regs.h>

/* only PIPE_C should use DPIO, A & B shares DPIO_2  */
#define CHV_DPIO(pipe) (((pipe == PIPE_C) ? IOSF_PORT_DPIO : IOSF_PORT_DPIO_2))
#define VLV_ID(pipe, plane) ((pipe * VLV_MAX_PLANES) + plane)

int chv_pipe_offsets[] = {
	PIPE_A_OFFSET,
	PIPE_B_OFFSET,
	CHV_PIPE_C_OFFSET
};

int chv_trans_offsets[] = {
	TRANSCODER_A_OFFSET,
	TRANSCODER_B_OFFSET,
	CHV_TRANSCODER_C_OFFSET,
};

int chv_palette_offsets[] = {
	PALETTE_A_OFFSET,
	PALETTE_B_OFFSET,
	CHV_PALETTE_C_OFFSET
};

int chv_cursor_offsets[] = {
	CURSOR_A_OFFSET,
	CURSOR_B_OFFSET,
	CHV_CURSOR_C_OFFSET
};


static const struct intel_dc_attachment chv_allowed_attachments[] = {
	{
		.pipe_id = PIPE_A,
		.plane_id = PRIMARY_PLANE,
		.pll_id = PLL_A,
	},
	{
		.pipe_id = PIPE_A,
		.plane_id = SPRITE_A,
		.pll_id = PLL_A,
	},
	{
		.pipe_id = PIPE_A,
		.plane_id = SPRITE_B,
		.pll_id = PLL_A,
	},
	{
		.pipe_id = PIPE_B,
		.plane_id = SECONDARY_PLANE,
		.pll_id = PLL_B,
	},
	{
		.pipe_id = PIPE_B,
		.plane_id = SPRITE_C,
		.pll_id = PLL_B,
	},
	{
		.pipe_id = PIPE_B,
		.plane_id = SPRITE_D,
		.pll_id = PLL_B,
	},
	{
		.pipe_id = PIPE_C,
		.plane_id = TERTIARY_PLANE,
		.pll_id = PLL_C,
	},
	{
		.pipe_id = PIPE_C,
		.plane_id = SPRITE_E,
		.pll_id = PLL_C,
	},
	{
		.pipe_id = PIPE_C,
		.plane_id = SPRITE_F,
		.pll_id = PLL_C,
	}
};

static const struct intel_dc_attachment vlv_allowed_attachments[] = {
	{
		.pipe_id = PIPE_A,
		.plane_id = PRIMARY_PLANE,
		.pll_id = PLL_A,
	},
	{
		.pipe_id = PIPE_A,
		.plane_id = SPRITE_A,
		.pll_id = PLL_A,
	},
	{
		.pipe_id = PIPE_A,
		.plane_id = SPRITE_B,
		.pll_id = PLL_A,
	},
	{
		.pipe_id = PIPE_B,
		.plane_id = SECONDARY_PLANE,
		.pll_id = PLL_B,
	},
	{
		.pipe_id = PIPE_B,
		.plane_id = SPRITE_C,
		.pll_id = PLL_B,
	},
	{
		.pipe_id = PIPE_B,
		.plane_id = SPRITE_D,
		.pll_id = PLL_B,
	}
};

void vlv_update_pipe_status(struct intel_dc_config *config,
		u8 pipe, bool enabled)
{
	struct vlv_dc_config *vlv_config = to_vlv_dc_config(config);

	if (enabled)
		vlv_config->status.pipe_plane_status |= (1 << (31 - pipe));
	else
		vlv_config->status.pipe_plane_status &= ~(1 << (31 - pipe));
}

void vlv_update_plane_status(struct intel_dc_config *config,
		u8 plane, bool enabled)
{
	struct vlv_dc_config *vlv_config = to_vlv_dc_config(config);

	if (enabled)
		vlv_config->status.pipe_plane_status |= (1 << plane);
	else
		vlv_config->status.pipe_plane_status &= ~(1 << plane);
}

void vlv_dc_config_destroy(struct intel_dc_config *config)
{
	struct vlv_dc_config *vlv_config = to_vlv_dc_config(config);
	struct vlv_pri_plane *pplane;
	struct vlv_sp_plane *splane;
	struct dsi_pipe *dsi_pipe;
	int pipe;

	if (!config || !vlv_config)
		return;

	for (pipe = 0; pipe < vlv_config->max_pipes; pipe++) {
		if (vlv_config->pipeline[pipe].type == INTEL_PIPE_DSI) {
			dsi_pipe = &vlv_config->pipeline[pipe].gen.dsi;
			dsi_pipe_destroy(dsi_pipe);
		} else if (vlv_config->pipeline[pipe].type == INTEL_PIPE_HDMI) {
			/* FIXME:
			 * HDMI Pipe deinit
			 */
		} else
			pr_err("ADF: %s: Unknown pipe type\n", __func__);

		pplane = &vlv_config->pipeline[pipe].pplane;
		vlv_pri_plane_destroy(pplane);
		splane = &vlv_config->pipeline[pipe].splane[0];
		vlv_sp_plane_destroy(splane);
		splane = &vlv_config->pipeline[pipe].splane[1];
		vlv_sp_plane_destroy(splane);
	}

	intel_dc_config_destroy(config);
	/* FIXME: uncomment when enabled */
	/* vlv_dpst_teardown(); */
	kfree(config);

	return;
}

static void vlv_attach_intel_pipeplane(struct vlv_pipeline *disp, u8
					   plane_per_pipe)
{
	struct intel_plane *intel_plane = NULL;
	struct intel_pipe *intel_pipe = NULL;
	int i = 0;

	/*
	 * In this platform the plane and pipe are fixed and cannot be
	 * moved across to a different pipe, hence set the attachment by
	 * default over here
	 */
	if (disp->type == INTEL_PIPE_DSI)
		intel_pipe = &disp->gen.dsi.base;
	else if (disp->type == INTEL_PIPE_DP || disp->type == INTEL_PIPE_EDP)
		intel_pipe = &disp->gen.dp.base;
	else if (disp->type == INTEL_PIPE_HDMI)
		intel_pipe = &disp->gen.hdmi.base;

	if (intel_pipe != NULL) {
		/* Attach Primary plane to intel Pipe */
		intel_plane = &disp->pplane.base;
		if (intel_plane->ops->attach)
			intel_plane->ops->attach(intel_plane, intel_pipe);

		/* Attach Sprite[n] to the intel pipe */
		for (i = 0; i < (plane_per_pipe - 1); i++) {
			intel_plane = &disp->splane[i].base;
			if (intel_plane->ops->attach)
				intel_plane->ops->attach(intel_plane,
							 intel_pipe);
		}
	}
	return;
}

static int vlv_display_encoder_init(struct vlv_dc_config *vlv_config, int pipe,
				int port, u8 disp_no)
{
	struct dsi_pipe *dsi_pipe = NULL;
	struct dp_pipe *dp_pipe = NULL;
	struct hdmi_pipe *hdmi_pipe = NULL;
	struct intel_pipeline *intel_pipeline;
	struct vlv_pipeline *disp = &vlv_config->pipeline[disp_no];
	int err;
	u8 *n_pipes;
	pr_err("ADF: %s\n", __func__);

	n_pipes = &vlv_config->base.n_pipes;

	/* Initialize interface PIPE */
	if (disp->type == INTEL_PIPE_DSI) {
		dsi_pipe = &disp->gen.dsi;
		intel_pipeline = &disp->base;
		err = dsi_pipe_init(dsi_pipe, vlv_config->base.dev,
				&disp->pplane.base, pipe, intel_pipeline, port);
		if (err) {
			dev_err(vlv_config->base.dev,
				"%s: failed to init pipe(%d)\n", __func__, err);
			return err;
		}

		intel_dc_config_add_pipe(&vlv_config->base,
					&dsi_pipe->base, *n_pipes);

		/* FIXME: uncomment when dpst is enabled with redesign*/
		/* vlv_dpst_init(&config->base);*/

		disp->dpst = &vlv_config->dpst;
	} else if ((disp->type == INTEL_PIPE_DP) ||
		    (disp->type == INTEL_PIPE_EDP)) {
		dp_pipe = &disp->gen.dp;
		intel_pipeline = &disp->base;

		err = dp_pipe_init(dp_pipe, vlv_config->base.dev,
			&disp->pplane.base, pipe, intel_pipeline, disp->type);
		if (err) {
			dev_err(vlv_config->base.dev,
				"%s: failed to init pipe for DP(%d)\n",
					__func__, err);
			return err;
		}

		intel_dc_config_add_pipe(&vlv_config->base,
			&dp_pipe->base, *n_pipes);
	} else if (disp->type == INTEL_PIPE_HDMI) {
		hdmi_pipe = &disp->gen.hdmi;
		intel_pipeline = &disp->base;
		err = hdmi_pipe_init(hdmi_pipe, vlv_config->base.dev,
				   &disp->pplane.base, pipe, intel_pipeline);
		if (err) {
			dev_err(vlv_config->base.dev,
				"%s: failed to init pipe for HDMI(%d)\n",
					__func__, err);
			return err;
		}
		intel_dc_config_add_pipe(&vlv_config->base,
					 &hdmi_pipe->base, *n_pipes);
	} else {
		pr_err("ADF: %s: unsupported pipe type = %d\n",
				__func__, disp->type);
		err = -EINVAL;
	}

	return err;
}

static int vlv_initialize_port(struct vlv_dc_config *vlv_config,
			int pipe, int port, int type, u8 disp_no)
{
	struct vlv_dsi_port *dsi_port = NULL;
	struct vlv_dp_port *dp_port = NULL;
	struct vlv_hdmi_port *hdmi_port = NULL;
	struct vlv_pipeline *disp = NULL;
	struct dsi_pipe *dsi_pipe = NULL;
	struct dsi_context *dsi_ctx = NULL;
	enum port port_no;

	disp = &vlv_config->pipeline[disp_no];
	switch (type) {
	case INTEL_PIPE_DSI:
		dsi_pipe = &disp->gen.dsi;
		dsi_ctx = &dsi_pipe->config.ctx;
		for_each_dsi_port(port_no, dsi_ctx->ports) {
			dsi_port = &disp->port.dsi_port[port_no];
			vlv_dsi_port_init(dsi_port, port_no, pipe);
		}
		break;
	case INTEL_PIPE_DP:
	case INTEL_PIPE_EDP:
		dp_port = &disp->port.dp_port;
		vlv_dp_port_init(dp_port, port, pipe, type,
		vlv_config->base.dev);
		break;
	case INTEL_PIPE_HDMI:
		hdmi_port = &disp->port.hdmi_port;
		vlv_hdmi_port_init(hdmi_port, port, pipe);
		break;
	default:
		pr_err("ADF: %s: Invalid display type\n", __func__);
		return -EINVAL;
		break;
	}

	return 0;
}

static void scaler_ratio_init(struct vlv_pipeline *disp, enum pipe pipe)
{
	/*
	 * Initialize  mpo per pipeline for different supported
	 * platforms and planes
	 */
	if ((intel_adf_get_platform_id() == gen_cherryview) &&
	    STEP_FROM(disp->dc_stepping, STEP_B0) && (pipe == PIPE_B)) {

		disp->mpo.max_hsr = CHV_MPO_MAX_HSR;
		disp->mpo.max_vsr = CHV_MPO_MAX_VSR;
		disp->mpo.max_hvsr = CHV_MPO_MAX_HVSR;

		disp->mpo.min_src_w = CHV_MPO_MIN_SRC_W;
		disp->mpo.min_src_h = CHV_MPO_MIN_SRC_H;
		disp->mpo.max_src_w = CHV_MPO_MAX_SRC_W;
		disp->mpo.max_src_h = CHV_MPO_MAX_SRC_H;

		disp->mpo.min_dst_w = CHV_MPO_MIN_DST_W;
		disp->mpo.min_dst_h = CHV_MPO_MIN_DST_H;
		disp->mpo.max_dst_w = CHV_MPO_MAX_DST_W;
		disp->mpo.max_dst_h = CHV_MPO_MAX_DST_H;
	}
	return;
}

static int vlv_initialize_disp(struct vlv_dc_config *vlv_config,
			enum pipe pipe, enum intel_pipe_type type,
			enum port port, u8 disp_no, u16 stepping)
{
	struct vlv_pri_plane *pplane;
	struct vlv_sp_plane *splane;
	struct vlv_pipeline *disp = NULL;
	struct vlv_pipe *vlv_pipe = NULL;
	struct vlv_pm *vlv_pm = NULL;
	struct vlv_pll *pll = NULL;
	int err;
	u8 *n_planes;
	u8 plane;

	if (!vlv_config) {
		dev_err(vlv_config->base.dev, "%s:invalid config", __func__);
		return -EINVAL;
	}

	if (pipe >= vlv_config->max_pipes) {
		dev_err(vlv_config->base.dev, "%s:invalid pipe", __func__);
		return -EINVAL;
	}

	n_planes = &vlv_config->base.n_planes;

	disp = &vlv_config->pipeline[disp_no];

	disp->type = type;
	disp->disp_no = disp_no;
	disp->config = vlv_config;

	/* Initialising each pipeline stepping id */
	disp->dc_stepping = stepping;

	plane = pipe * VLV_MAX_PLANES;

	disp->type = type;

	if (IS_CHERRYVIEW())
		disp->dpio_id = CHV_DPIO(pipe);

	/* Initialize the plane */
	pplane = &disp->pplane;
	err = vlv_pri_plane_init(pplane, &disp->base,
		vlv_config->base.dev, plane);
	if (err) {
		dev_err(vlv_config->base.dev,
			"%s: failed to init pri plane, %d\n", __func__, err);
		return err;
	}
	intel_dc_config_add_plane(&vlv_config->base, &pplane->base,
				*n_planes);

	/* Initialize first sprite */
	splane = &disp->splane[0];
	err = vlv_sp_plane_init(splane, &disp->base,
			vlv_config->base.dev, plane + 1);
	if (err) {
		dev_err(vlv_config->base.dev,
			"%s: failed to init sprite plane, %d\n",
			__func__, err);
		return err;
	}
	intel_dc_config_add_plane(&vlv_config->base, &splane->base,
				*n_planes);

	/* Initialize second sprite */
	splane = &disp->splane[1];
	err = vlv_sp_plane_init(splane, &disp->base,
			vlv_config->base.dev, plane + 2);
	if (err) {
		dev_err(vlv_config->base.dev,
				"%s: failed to init sprite plane, %d\n",
			__func__, err);
		return err;
	}
	intel_dc_config_add_plane(&vlv_config->base, &splane->base,
				*n_planes);
	vlv_pm = &disp->pm;

	if (vlv_pm_init(vlv_pm, (enum pipe) pipe) == false) {
		dev_err(vlv_config->base.dev,
			"%s: failed to init pm for pipe %d\n",
			__func__, pipe);
		return err;
	}

	vlv_pipe = &disp->pipe;
	err = vlv_pipe_init(vlv_pipe, (enum pipe) pipe);

	vlv_attach_intel_pipeplane(disp, CHV_MAX_PLANES);

	/* Initialize mpo per pipeline */
	scaler_ratio_init(disp, (enum pipe) pipe);

	/* FIXME: update from attachment */
	pll = &disp->pll;
	err = vlv_pll_init(pll, type, (enum pipe) pipe, port);

	if (type == INTEL_PIPE_DSI) {
		/*
		 * For DSI we are calling the vlv_display_encoder_init() first
		 * because we need to set the port bit mask which gets used
		 * in vlv_initialize_port() to assign the proper register
		 * address offsets for DSI.
		 */

		/* Initialize encoder */
		vlv_display_encoder_init(vlv_config, pipe, port, disp_no);

		/* Initialize port */
		vlv_initialize_port(vlv_config, pipe, port, type, disp_no);
	} else if ((type == INTEL_PIPE_HDMI) || (type == INTEL_PIPE_EDP) ||
				(type == INTEL_PIPE_DP)) {

		/* Initialize port */
		vlv_initialize_port(vlv_config, pipe, port, type, disp_no);

		/* Initialize encoder */
		vlv_display_encoder_init(vlv_config, pipe, port, disp_no);
	}

	return err;
}

static u16 chv_dc_get_stepping(struct pci_dev *pdev)
{
	u16 stepping = 0;
	u8 rev_id;

	if (!pdev) {
		pr_err("ADF: %s Null parameter\n", __func__);
		return 0;
	}

	rev_id = pdev->revision;

	stepping = ((rev_id & CHV_PCI_MINOR_STEP_MASK)
					>> CHV_PCI_MINOR_STEP_SHIFT) + '0';
	if ((rev_id & CHV_PCI_STEP_SEL_MASK) >> CHV_PCI_STEP_SEL_SHIFT)
		stepping = stepping + ('K' << 8);
	else
		stepping = stepping + ('A' << 8);

	stepping = stepping + (((rev_id & CHV_PCI_MAJOR_STEP_MASK)
					>> CHV_PCI_MAJOR_STEP_SHIFT) << 8);

	pr_info("ADF %s CHV stepping id = 0x%x\n", __func__, stepping);
	return stepping;
}

static void vlv_reset_pipeline_params(struct vlv_dc_config *config)
{
	int i;

	/* Clean up planes */
	vlv_pri_plane_destroy(&config->pipeline[0].pplane);
	vlv_sp_plane_destroy(&config->pipeline[0].splane[0]);
	vlv_sp_plane_destroy(&config->pipeline[0].splane[1]);

	for (i = 0; i < config->base.n_planes; i++)
		config->base.planes[i] = NULL;
	config->base.pipes[0] = NULL;

	config->base.n_pipes = 0;
	config->base.n_planes = 0;
}

static void vlv_disable_efp_dp(struct vlv_dc_config *config, u16 stepping)
{
	int i, val;
	enum pipe pipe;
	enum port port;
	enum intel_pipe_type type;
	struct intel_pipeline *pipeline;
	struct vlv_pipeline *disp;
	bool found = false;
	int dp_port_reg[] = {DP_B, DP_C, DP_D};

	/*
	 * The scratch pad register does not reflect the reality
	 * in case of EFPs. Hence, loop through all port control
	 * registers to properly find out whic port/pipe is
	 * being used by BIOS; and then disable them.
	 */
	for (i = 0; i < ARRAY_SIZE(dp_port_reg); i++) {
		val = REG_READ(VLV_DISPLAY_BASE + dp_port_reg[i]);
		if (val & (1 << 31)) {
			port = PORT_B + i;
			type = INTEL_PIPE_DP;

			/* Extract bits[16:17] to get the selected pipe */
			pipe = (val >> 16) & 0x3;
			found = true;
			break;
		}
	}

	if (!found) {
		pr_err("EFP: No DP found\n");
		return;
	}

	pr_info("Disabling EFP: pipe:%d port:%d type:%d\n", pipe, port, type);
	vlv_initialize_disp(config, pipe, type, port, 0, stepping);
	pipeline = &config->pipeline[0].base;
	chv_pipeline_off(pipeline);

	disp = to_vlv_pipeline(pipeline);
	vlv_dp_port_destroy(&disp->port.dp_port);
	intel_pipe_destroy(&disp->gen.dp.base);

	vlv_reset_pipeline_params(config);
}

static void vlv_disable_efp_hdmi(struct vlv_dc_config *config, u16 stepping)
{
	int i, val;
	enum pipe pipe;
	enum port port;
	enum intel_pipe_type type;
	bool found = false;
	int hdmi_port_reg[] = {CHV_PORTB_CTRL, CHV_PORTC_CTRL, CHV_PORTD_CTRL};
	struct intel_pipeline *pipeline;
	struct vlv_pipeline *disp;

	/*
	 * The scratch pad register does not reflect the reality
	 * in case of EFPs. Hence, loop through all port control
	 * registers to properly find out whic port/pipe is
	 * being used by BIOS; and then disable them.
	 */
	for (i = 0; i < ARRAY_SIZE(hdmi_port_reg); i++) {
		val = REG_READ(hdmi_port_reg[i]);
		if (val & (1 << 31)) {
			port = PORT_B + i;
			type = INTEL_PIPE_HDMI;
			/* Extract bits[24:25] to get the selected pipe */
			pipe = (val >> 24) & 0x3;
			found = true;
			break;
		}
	}

	if (!found) {
		pr_err("EFP: No HDMI found\n");
		return;
	}

	pr_info("Disabling EFP: pipe:%d port:%d type:%d\n", pipe, port, type);
	vlv_initialize_disp(config, pipe, type, port, 0, stepping);

	pipeline = &config->pipeline[0].base;
	chv_pipeline_off(pipeline);

	disp = to_vlv_pipeline(pipeline);
	hdmi_pipe_destroy(&disp->gen.hdmi);
	intel_pipe_destroy(&disp->gen.hdmi.base);

	vlv_reset_pipeline_params(config);
}

static void vlv_disable_lfp(enum pipe pipe, struct vlv_dc_config *config,
			union child_device_config *child_dev,
			int dev_num, u16 stepping)
{
	int i, dvo_port, devtype;
	enum port port = 0;
	enum intel_pipe_type type = 0;
	bool is_lfp = false;
	struct intel_pipeline *pipeline;
	struct vlv_pipeline *disp;
	struct dsi_pipe *dsi;

	for (i = 0; i <= dev_num; i++) {
		devtype = child_dev[i].common.device_type;
		is_lfp = devtype & DEVICE_TYPE_INTERNAL_CONNECTOR;

		/* Since we disabled that one LFP, break */
		if (is_lfp)
			break;
	}

	if (!is_lfp)
		return;

	dvo_port = child_dev[i].common.dvo_port;

	if (devtype & DEVICE_TYPE_MIPI_OUTPUT) {
		type = INTEL_PIPE_DSI;
		port = dvo_port - DVO_PORT_MIPIA;
		vlv_initialize_disp(config, pipe, type, port, 0 , stepping);
		pipeline = &config->pipeline[0].base;
		disp = to_vlv_pipeline(pipeline);
		dsi = &disp->gen.dsi;

		dsi->base.ops->dpms(&dsi->base, DRM_MODE_DPMS_OFF);
		dsi_pipe_destroy(dsi);
		intel_pipe_destroy(&dsi->base);

		vlv_reset_pipeline_params(config);
	} else if (devtype & DEVICE_TYPE_EDP_BITS) {
		type = INTEL_PIPE_EDP;
		port = dvo_port - DVO_PORT_CRT;
		vlv_initialize_disp(config, pipe, type, port, 0, stepping);
		pipeline = &config->pipeline[0].base;

		/* Turn off backlight/pps for eDP before pipeline */
		vlv_dp_backlight_seq(pipeline, false);
		vlv_dp_panel_power_seq(pipeline, false);
		chv_pipeline_off(pipeline);

		disp = to_vlv_pipeline(pipeline);
		vlv_dp_port_destroy(&disp->port.dp_port);
		intel_pipe_destroy(&disp->gen.dp.base);

		vlv_reset_pipeline_params(config);
	}

	pr_debug("Disabled LFP: pipe:%d port:%d type:%d\n", pipe, port, type);
}

/* Disables the displays enabled by BIOS */
static void vlv_disable_displays(struct vlv_dc_config *config,
			union child_device_config *child_dev,
			int dev_num, u16 stepping)
{
	enum pipe pipe;
	int tmp, val = REG_READ(SWF00);

	pr_info("%s:scratch pad register[%x]:%x\n", __func__, SWF00, val);

	/*
	 * The scratch pad registers do not report correct
	 * information for EFPs. Hence use port control
	 * registers to find out which EFP is actually
	 * enabled, and used by BIOS.
	 */
	for (pipe = PIPE_A; pipe < PIPE_C; pipe++) {
		tmp = SWF00_PIPE_MASK & (val >> (SWF00_PIPE_BITS * pipe));
		if (tmp & SWF00_LFP_ACTIVE_MASK) {
			vlv_disable_lfp(pipe, config, child_dev,
					dev_num, stepping);
		}

		if (tmp & SWF00_EFP_ACTIVE_MASK) {
			vlv_disable_efp_hdmi(config, stepping);
			vlv_disable_efp_dp(config, stepping);
		}
	}
	pr_debug("Disabling displays enabled by BIOS: Done\n");
}

static int valleyview_get_vco(void)
{
	int hpll_freq, vco_freq[] = { 800, 1600, 2000, 2400 };

	/* Obtain SKU information */
	hpll_freq = vlv_cck_read(CCK_FUSE_REG) &
		CCK_FUSE_HPLL_FREQ_MASK;

	return vco_freq[hpll_freq];
}

static void vlv_update_cdclk(int cdclk)
{
	u32 cmd, val, vco;

	vco = valleyview_get_vco();
	cmd = DIV_ROUND_CLOSEST((vco << 1), cdclk) - 1;

	pr_info("Obtained cmd %d for cdclk %d\n", cmd, cdclk);

	val = vlv_punit_read(PUNIT_REG_DSPFREQ);
	val &= ~DSPFREQGUAR_MASK_CHV;
	val |= (cmd << DSPFREQGUAR_SHIFT_CHV);
	vlv_punit_write(PUNIT_REG_DSPFREQ, val);
	if (wait_for((vlv_punit_read(PUNIT_REG_DSPFREQ) &
		      DSPFREQSTAT_MASK_CHV) == (cmd << DSPFREQSTAT_SHIFT_CHV),
		     50)) {
		pr_err("timed out waiting for CDclk change\n");
		return;
	}

	/* Adjust the GMBUS frequency also */
	REG_WRITE(GMBUSFREQ_VLV, cdclk);
}

/* Get current cd clock */
u32 vlv_get_cdclk(void)
{
	u32 cur_cdclk, vco;
	u32 divider;

	vco =  valleyview_get_vco();
	divider = vlv_cck_read(CCK_DISPLAY_CLOCK_CONTROL);
	divider &= 0xf;

	cur_cdclk = (vco << 1) / (divider + 1);
	return cur_cdclk;
}

static void vlv_update_global_params(void)
{
	/* TODO:
	 * 1. Add method to Calculate CDclk.
	 * 2. When programming CDclk > CZCLK, set PFI credits
	 * For now, hard-code to max possible value that works
	 * i.e. 320 MHZ.
	 */
	vlv_update_cdclk(320);
}

struct intel_dc_config *vlv_get_dc_config(struct pci_dev *pdev, u32 id)
{
	struct vlv_dc_config *config;
	union child_device_config *child_dev = NULL;
	int dev_num;
	int dvo_port;
	int devtype;
	int err;
	u8 display_no = 0;
	u16 port;
	int i, lfp_pipe = 0;
	enum pipe pipe = PIPE_A;
	u16 stepping = 0;

	if (!pdev)
		return ERR_PTR(-EINVAL);

	config = kzalloc(sizeof(struct vlv_dc_config), GFP_KERNEL);
	if (!config) {
		dev_err(&pdev->dev, "failed to alloc memory\n");
		return ERR_PTR(-ENOMEM);
	}

	/* Detect stepping of CHV display controller */
	if (IS_CHERRYVIEW())
		stepping = chv_dc_get_stepping(pdev);

	/* Init config */
	if (id == gen_cherryview) {
		config->max_planes = sizeof(chv_allowed_attachments)/
			sizeof(chv_allowed_attachments[0]);
		config->max_pipes = config->max_planes / 3;

		err = intel_dc_config_init(&config->base, &pdev->dev, 0,
				config->max_planes, config->max_pipes,
				&chv_allowed_attachments[0],
				ARRAY_SIZE(chv_allowed_attachments));

	} else {
		config->max_planes = sizeof(vlv_allowed_attachments)/
			sizeof(vlv_allowed_attachments[0]);
		config->max_pipes = config->max_planes / 3;

		err = intel_dc_config_init(&config->base, &pdev->dev, 0,
				config->max_planes, config->max_pipes,
				&vlv_allowed_attachments[0],
				ARRAY_SIZE(vlv_allowed_attachments));
	}

	if (err) {
		dev_err(&pdev->dev, "failed to inintialize dc config\n");
		goto err;
	}

	/* create and add power */
	/*
	 * TODO: add dpms config over here and register with adf using
	 * intel_dc_config_add_power();
	 */
	intel_get_vbt_disp_conf((void **)&child_dev, &dev_num);

	mutex_init(&config->dpio_lock);

	/* Disable all displays enabled by BIOS */
	vlv_disable_displays(config, child_dev, dev_num, stepping);

	/* Now that all displays are off, update CDclk, PLL etc, if needed */
	vlv_update_global_params();

	/*
	 * LFP
	 *      if MIPI A --> PIPEA
	 *              MIPI C --> PIPEB
	 *      if eDP  ---> PIPE B
	 * EFP
	 *      if port D --> PIPE C
	 *      else PIPE A or PIPE B
	 */

	/* To check for the LPF first */
	for (i = 0; i <= dev_num; i++) {
		dvo_port = child_dev[i].common.dvo_port;
		devtype = child_dev[i].common.device_type;
		if (devtype & DEVICE_TYPE_INTERNAL_CONNECTOR) {
			if  (devtype & DEVICE_TYPE_MIPI_OUTPUT) {
				if (dvo_port == DVO_PORT_MIPIA)
					pipe = PIPE_A;
				else
					pipe = PIPE_B;

				/*
				 * For MIPI dvo port value will be 21-23
				 * so we have to subtract 21(DVO_PORT_MIPIA)
				 * from this value to get the port
				 */
				vlv_initialize_disp(config, pipe,
						INTEL_PIPE_DSI,
						(dvo_port - DVO_PORT_MIPIA),
						display_no++, stepping);
			} else if (devtype & DEVICE_TYPE_EDP_BITS) {
				pipe = PIPE_B;
				vlv_initialize_disp(config, pipe,
						INTEL_PIPE_EDP,
						dvo_port - DVO_PORT_CRT,
						display_no++, stepping);
			}
			lfp_pipe = pipe;
		}
	}

	/* To check the EFP */
	for (i = 0; i <= dev_num; i++) {
		dvo_port = child_dev[i].common.dvo_port;
		devtype = child_dev[i].common.device_type;

		if (devtype == DEVICE_TYPE_HDMI ||
				devtype == DEVICE_TYPE_DP_HDMI_DVI
				|| devtype == DEVICE_TYPE_DP) {

			/* Selecting the pipe */
			if ((dvo_port == DVO_PORT_HDMIB) ||
					(dvo_port == DVO_PORT_HDMIC) ||
					(dvo_port == DVO_PORT_DPB) ||
					(dvo_port == DVO_PORT_DPC)) {
				if (lfp_pipe != PIPE_A)
					pipe = PIPE_A;
				else
					pipe = PIPE_B;

			} else {
				pipe = PIPE_C;
			}

			/* Setting the port number */
			if (devtype == DEVICE_TYPE_DP)
				port = dvo_port - DVO_PORT_CRT;

			/* Selecting the PIPE type */
			if (devtype == DEVICE_TYPE_DP_HDMI_DVI
					|| devtype == DEVICE_TYPE_DP)
				/*
				 * If device type is HDMI over DP
				 * or DP then selecting
				 * the pipe type as DP
				 */
				vlv_initialize_disp(config, pipe,
						INTEL_PIPE_DP,
						dvo_port, display_no++,
						stepping);
			else
				vlv_initialize_disp(config, pipe,
						INTEL_PIPE_HDMI,
						dvo_port, display_no++,
						stepping);
		}
	}

	return &config->base;
err:
	vlv_dc_config_destroy(&config->base);
	return ERR_PTR(err);
}



/* FIXME: TEMP till dpst is enabled */
u32 vlv_dpst_context(struct intel_pipeline *pipeline, unsigned long args)
{
	return 0;
}

u32 vlv_dpst_irq_handler(struct intel_pipeline *pipeline)
{
	return 0;
}
