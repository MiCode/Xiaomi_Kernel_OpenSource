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
#include <core/vlv/vlv_dc_config.h>
#include <core/vlv/vlv_pri_plane.h>
#include <core/vlv/vlv_sp_plane.h>

#define VLV_ID(pipe, plane) ((pipe * VLV_MAX_PLANES) + plane)

static const struct intel_dc_attachment chv_allowed_attachments[] = {
	{
		.pipe_id = PIPE_A,
		.plane_id = PRIMARY_PLANE,
	},
	{
		.pipe_id = PIPE_A,
		.plane_id = SPRITE_A,
	},
	{
		.pipe_id = PIPE_A,
		.plane_id = SPRITE_B,
	},
	{
		.pipe_id = PIPE_B,
		.plane_id = SECONDARY_PLANE,
	},
	{
		.pipe_id = PIPE_B,
		.plane_id = SPRITE_C,
	},
	{
		.pipe_id = PIPE_B,
		.plane_id = SPRITE_D,
	},
	{
		.pipe_id = PIPE_C,
		.plane_id = TERTIARY_PLANE,
	},
	{
		.pipe_id = PIPE_C,
		.plane_id = SPRITE_E,
	},
	{
		.pipe_id = PIPE_C,
		.plane_id = SPRITE_F,
	}
};

static const struct intel_dc_attachment vlv_allowed_attachments[] = {
	{
		.pipe_id = PIPE_A,
		.plane_id = PRIMARY_PLANE,
	},
	{
		.pipe_id = PIPE_A,
		.plane_id = SPRITE_A,
	},
	{
		.pipe_id = PIPE_A,
		.plane_id = SPRITE_B,
	},
	{
		.pipe_id = PIPE_B,
		.plane_id = SECONDARY_PLANE,
	},
	{
		.pipe_id = PIPE_B,
		.plane_id = SPRITE_C,
	},
	{
		.pipe_id = PIPE_B,
		.plane_id = SPRITE_D,
	}
};

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

static int vlv_display_encoder_init(struct vlv_dc_config *vlv_config, int pipe,
				    u8 disp_no)
{
	struct dsi_pipe *dsi_pipe = NULL;
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
				&disp->pplane.base, pipe, intel_pipeline);
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
	struct vlv_pipeline *disp = NULL;

	disp = &vlv_config->pipeline[disp_no];
	switch (type) {
	case INTEL_PIPE_DSI:
		dsi_port = &disp->port.dsi_port;
		vlv_dsi_port_init(dsi_port, port, pipe);
		break;
	default:
		break;
	}

	return 0;
}

static int vlv_initialize_disp(struct vlv_dc_config *vlv_config,
			enum pipe pipe, enum intel_pipe_type type,
			enum port port, u8 disp_no)
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

	plane = pipe * VLV_MAX_PLANES;

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
	}

	vlv_pipe = &disp->pipe;
	err = vlv_pipe_init(vlv_pipe, (enum pipe) pipe);

	/* FIXME: update from attachment */
	pll = &disp->pll;
	err = vlv_pll_init(pll, type, (enum pipe) pipe, port);

	/* Initialize port */
	vlv_initialize_port(vlv_config, pipe, port, type, disp_no);

	/* Initialize encoder */
	vlv_display_encoder_init(vlv_config, pipe, disp_no);

	return err;
}

struct intel_dc_config *vlv_get_dc_config(struct pci_dev *pdev, u32 id)
{
	struct vlv_dc_config *config;
	struct intel_dc_memory *memory;
	union child_device_config *child_dev = NULL;
	int dev_num;
	int dvo_port;
	int devtype;
	int err;
	u8 display_no = 0;
	u16 port;
	int i, lfp_pipe = 0;
	enum pipe pipe = PIPE_A;

	if (!pdev)
		return ERR_PTR(-EINVAL);

	config = kzalloc(sizeof(struct vlv_dc_config), GFP_KERNEL);
	if (!config) {
		dev_err(&pdev->dev, "failed to alloc memory\n");
		return ERR_PTR(-ENOMEM);
	}

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

	/* create and add memory */
	/*
	 * TODO: add gem config or get the gem struct here and register as a
	 * interface with adf y using intel_dc_config_add_memory();
	 */
	memory = kzalloc(sizeof(struct intel_dc_memory), GFP_KERNEL);
	config->base.memory = memory;

	/* create and add power */
	/*
	 * TODO: add dpms config over here and register with adf using
	 * intel_dc_config_add_power();
	 */
	intel_get_vbt_disp_conf((void **)&child_dev, &dev_num);

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
						display_no++);
			} else if (devtype & DEVICE_TYPE_EDP_BITS) {
				pipe = PIPE_B;
				vlv_initialize_disp(config, pipe,
						INTEL_PIPE_EDP,
						dvo_port - DVO_PORT_CRT,
						display_no++);
			}
			lfp_pipe = pipe;
		}
	}

	/* To check the EPF */
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
						dvo_port, display_no++);
			else
				vlv_initialize_disp(config, pipe,
						INTEL_PIPE_HDMI,
						dvo_port, display_no++);
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
