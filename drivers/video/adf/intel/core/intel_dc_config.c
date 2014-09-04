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

#include "core/intel_dc_config.h"

static const struct intel_dc_config_entry g_dc_configs[] = {
	{
		.id = 0,
		.get_dc_config = vlv_get_dc_config,
		.destroy_dc_config = vlv_dc_config_destroy,
	},
};

int intel_dc_component_init(struct intel_dc_component *component,
	struct device *dev, u8 idx, const char *name)
{
	if (!component)
		return -EINVAL;

	component->dev = dev;
	component->idx = idx;
	component->name = name;

	return 0;
}

void intel_dc_component_destroy(struct intel_dc_component *component)
{
	if (component)
		memset(component, 0, sizeof(*component));
}

#define INTEL_DC_CHECK(dev, obj) ({	\
	if (!obj) {	\
		dev_err(dev, "%s: %s is missing\n", __func__, #obj);	\
		return -EINVAL;		\
	}	\
})

static int intel_plane_validate(const struct intel_plane *plane)
{
	INTEL_DC_CHECK(plane->base.dev, plane->caps);
	INTEL_DC_CHECK(plane->base.dev, plane->ops);
	INTEL_DC_CHECK(plane->base.dev, plane->ops->validate);
	INTEL_DC_CHECK(plane->base.dev, plane->ops->flip);
	INTEL_DC_CHECK(plane->base.dev, plane->ops->enable);
	INTEL_DC_CHECK(plane->base.dev, plane->ops->disable);

	return 0;
}

static int intel_pipe_validate(const struct intel_pipe *pipe)
{
	/*verify pipe type*/
	if (pipe->type != INTEL_PIPE_DSI && pipe->type != INTEL_PIPE_HDMI) {
		dev_err(pipe->base.dev, "%s: Invalid pipe type %d\n", __func__,
			pipe->type);
		return -EINVAL;
	}

	INTEL_DC_CHECK(pipe->base.dev, pipe->primary_plane);
	INTEL_DC_CHECK(pipe->base.dev, pipe->ops);
	INTEL_DC_CHECK(pipe->base.dev, pipe->ops->get_modelist);
	INTEL_DC_CHECK(pipe->base.dev, pipe->ops->get_preferred_mode);
	INTEL_DC_CHECK(pipe->base.dev, pipe->ops->dpms);
	INTEL_DC_CHECK(pipe->base.dev, pipe->ops->modeset);
	INTEL_DC_CHECK(pipe->base.dev, pipe->ops->get_screen_size);
	INTEL_DC_CHECK(pipe->base.dev, pipe->ops->is_screen_connected);
	INTEL_DC_CHECK(pipe->base.dev, pipe->ops->get_supported_events);
	INTEL_DC_CHECK(pipe->base.dev, pipe->ops->set_event);
	INTEL_DC_CHECK(pipe->base.dev, pipe->ops->get_events);
	INTEL_DC_CHECK(pipe->base.dev, pipe->ops->is_screen_connected);
	INTEL_DC_CHECK(pipe->base.dev, pipe->ops->get_vsync_counter);

	return 0;
}

#ifndef CONFIG_ADF_INTEL_VLV
static int intel_dc_memory_validate(const struct intel_dc_memory *memory)
{
	if (!memory->total_pages) {
		dev_err(memory->dev, "%s: Invalid total pages\n", __func__);
		return -EINVAL;
	}

	if (memory->total_pages != memory->alloc_pages + memory->free_pages) {
		dev_err(memory->dev, "%s: pages mismatch\n", __func__);
		return -EINVAL;
	}

	INTEL_DC_CHECK(memory->dev, memory->ops);
	INTEL_DC_CHECK(memory->dev, memory->ops->import);
	INTEL_DC_CHECK(memory->dev, memory->ops->free);

	return 0;
}
#endif

static int intel_dc_config_validate(const struct intel_dc_config *config)
{
	int err;
	u8 i;

	if (!config->planes || !config->n_planes) {
		dev_err(config->dev, "%s: no planes found\n", __func__);
		return -EINVAL;
	}

	if (!config->pipes || !config->n_pipes) {
		dev_err(config->dev, "%s: no pipes found\n", __func__);
		return -EINVAL;
	}

	if (!config->allowed_attachments || !config->n_allowed_attachments) {
		dev_err(config->dev, "%s: no allowed attachments found\n",
			__func__);
		return -EINVAL;
	}

	/*check integrity of all planes*/
	for (i = 0; i < config->n_planes; i++) {
		err = intel_plane_validate(config->planes[i]);
		if (err) {
			dev_err(config->dev, "%s: invalid plane %d\n",
				__func__, i);
			return -EINVAL;
		}
	}

	/*check integrity of all pipes*/
	for (i = 0; i < config->n_pipes; i++) {
		err = intel_pipe_validate(config->pipes[i]);
		if (err) {
			dev_err(config->dev, "%s: invalid pipe %d\n",
				__func__, i);
			return -EINVAL;
		}
	}

#ifndef CONFIG_ADF_INTEL_VLV
	/*check memory*/
	INTEL_DC_CHECK(config->dev, config->memory);
	err = intel_dc_memory_validate(config->memory);
	if (err) {
		dev_err(config->dev, "%s: invalid DC memory", __func__);
		return -EINVAL;
	}
#endif

	return 0;
}

void intel_dc_config_add_plane(struct intel_dc_config *config,
	struct intel_plane *plane, u8 idx)
{
	if (!config || !plane)
		return;

	if (config->planes[idx])
		return;

	config->planes[idx] = plane;
	config->n_planes++;
}

void intel_dc_config_add_pipe(struct intel_dc_config *config,
	struct intel_pipe *pipe, u8 idx)
{
	if (!config || !pipe) {
		dev_err(config->dev, "%s: config or pipe is NULL\n", __func__);
		return;
	}

	if (config->pipes[idx]) {
		dev_err(config->dev, "%s: pipe already added\n", __func__);
		return;
	}

	config->pipes[idx] = pipe;
	config->n_pipes++;
}

int intel_dc_config_init(struct intel_dc_config *config, struct device *dev,
	u32 id, size_t n_planes, size_t n_pipes,
	const struct intel_dc_attachment *allowed_attachments,
	size_t n_allowed_attachments)
{
	struct intel_plane **planes;
	struct intel_pipe **pipes;
	int err;

	if (!config || !dev || !n_planes || !n_pipes || !allowed_attachments ||
		!n_allowed_attachments)
		return -EINVAL;

	if (n_planes > INTEL_DC_MAX_PLANE_COUNT)
		return -EINVAL;

	if (n_pipes > INTEL_DC_MAX_PIPE_COUNT)
		return -EINVAL;

	memset(config, 0, sizeof(*config));

	planes = kzalloc(n_planes * sizeof(*planes), GFP_KERNEL);
	if (!planes)
		return -ENOMEM;

	pipes = kzalloc(n_pipes * sizeof(*pipes), GFP_KERNEL);
	if (!pipes) {
		err = -ENOMEM;
		goto err;
	}

	config->dev = dev;
	config->id = id;
	config->planes = planes;
	config->n_planes = 0;
	config->pipes = pipes;
	config->n_pipes = 0;
	config->allowed_attachments = allowed_attachments;
	config->n_allowed_attachments = n_allowed_attachments;
	return 0;
err:
	kfree(planes);
	return err;
}

void intel_dc_config_destroy(struct intel_dc_config *config)
{
	if (config) {
		config->dev = NULL;
		kfree(config->planes);
		kfree(config->pipes);
		memset(config, 0, sizeof(*config));
	}
}

struct intel_dc_config *intel_adf_get_dc_config(struct pci_dev *pdev, u32 id)
{
	struct intel_dc_config *config = NULL;
	int err;
	int i;

	for (i = 0; i < ARRAY_SIZE(g_dc_configs); i++) {
		if (g_dc_configs[i].id != id)
			continue;
		if (g_dc_configs[i].get_dc_config) {
			config = g_dc_configs[i].get_dc_config(pdev, id);
			break;
		}
	}

	if (IS_ERR(config)) {
		dev_err(&pdev->dev, "%s: failed to get dc config\n", __func__);
		return ERR_PTR(-EINVAL);
	}

	/*validate this config*/
	err = intel_dc_config_validate(config);
	if (err)
		return ERR_PTR(err);

	return config;
}

void intel_adf_destroy_config(struct intel_dc_config *config)
{
	u32 id;
	int i;

	if (config) {
		id = config->id;
		for (i = 0; i < ARRAY_SIZE(g_dc_configs); i++) {
			if (g_dc_configs[i].id == id &&
				g_dc_configs[i].destroy_dc_config) {
				g_dc_configs[i].destroy_dc_config(config);
				break;
			}
		}
	}
}
