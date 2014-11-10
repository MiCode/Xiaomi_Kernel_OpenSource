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
	}
};

void vlv_dc_config_destroy(struct intel_dc_config *config)
{
	struct vlv_dc_config *vlv_config = to_vlv_dc_config(config);
	struct vlv_pri_plane *pplane;
	struct vlv_sp_plane *splane;
	int pipe;

	if (!config)
		return;

	for (pipe = 0; pipe < MAX_PIPES; pipe++) {
		/* Do pipe deinit here one pipe init code is ready for DSI */
		pplane = &vlv_config->vdisp[pipe].pplane;
		vlv_pri_plane_destroy(pplane);
		splane = &vlv_config->vdisp[pipe].splane[0];
		vlv_sp_plane_destroy(splane);
		splane = &vlv_config->vdisp[pipe].splane[1];
		vlv_sp_plane_destroy(splane);
	}

	intel_dc_config_destroy(config);
	kfree(config);

	return;
}

static int vlv_initialize_disp(struct vlv_dc_config *vlv_config, int pipe,
		enum intel_pipe_type type)
{
	struct vlv_pri_plane *pplane;
	struct vlv_sp_plane *splane;
	int err;

	if (pipe > MAX_PIPES) {
		dev_err(vlv_config->base.dev, "%s:invalid pipe", __func__);
		return -EINVAL;
	}
	/* Initialize the plane */
	pplane = &vlv_config->vdisp[pipe].pplane;
	err = vlv_pri_plane_init(pplane, vlv_config->base.dev, PRIMARY_PLANE);
	if (err) {
		dev_err(vlv_config->base.dev,
			"%s: failed to init pri plane, %d\n", __func__, err);
		return err;
	}
	intel_dc_config_add_plane(&vlv_config->base, &pplane->base,
				  VLV_ID(pipe, VLV_PLANE));

	/* Initialize first sprite */
	splane = &vlv_config->vdisp[pipe].splane[0];
	err = vlv_sp_plane_init(splane, vlv_config->base.dev,
				pipe ? SPRITE_C : SPRITE_A);
	if (err) {
		dev_err(vlv_config->base.dev,
			"%s: failed to init sprite plane, %d\n", __func__, err);
		return err;
	}
	intel_dc_config_add_plane(&vlv_config->base, &splane->base,
				  VLV_ID(pipe, VLV_SPRITE1));

	/* Initialize second sprite */
	splane = &vlv_config->vdisp[pipe].splane[1];
	err = vlv_sp_plane_init(splane, vlv_config->base.dev,
				pipe ? SPRITE_D : SPRITE_B);
	if (err) {
		dev_err(vlv_config->base.dev,
				"%s: failed to init sprite plane, %d\n",
			__func__, err);
		return err;
	}
	intel_dc_config_add_plane(&vlv_config->base, &splane->base,
				  VLV_ID(pipe, VLV_SPRITE2));

	/* TBD: Initialize interface PIPE */

	return err;
}

struct intel_dc_config *vlv_get_dc_config(struct pci_dev *pdev, u32 id)
{
	struct vlv_dc_config *config;
	struct intel_dc_memory *memory;
	int err;

	if (!pdev)
		return ERR_PTR(-EINVAL);

	config = kzalloc(sizeof(struct vlv_dc_config), GFP_KERNEL);
	if (!config) {
		dev_err(&pdev->dev, "failed to alloc memory\n");
		return ERR_PTR(-ENOMEM);
	}
	/* Init config */
	err = intel_dc_config_init(&config->base, &pdev->dev, 0,
				   NUM_PLANES, VLV_N_PIPES,
				   &vlv_allowed_attachments[0],
				   ARRAY_SIZE(vlv_allowed_attachments));
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

	vlv_initialize_disp(config, PIPE_A, INTEL_PIPE_DSI);

	return &config->base;
err:
	vlv_dc_config_destroy(&config->base);
	return ERR_PTR(err);
}
