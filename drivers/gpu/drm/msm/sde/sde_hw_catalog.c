/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/of_platform.h>
#include "sde_hw_catalog.h"
#include "sde_kms.h"

struct sde_mdss_hw_cfg_handler cfg_table[] = {
	{ .major = 1, .minor = 7, .cfg_init = sde_mdss_cfg_170_init},
};

static inline u32 _sde_parse_sspp_id(struct sde_mdss_cfg *cfg, const char *name)
{
	int i;

	for (i = 0; i < cfg->sspp_count; i++) {
		if (!strcmp(cfg->sspp[i].name, name))
			return cfg->sspp[i].id;
	}

	return SSPP_NONE;
}

static int _sde_parse_dt(struct device_node *np, struct sde_mdss_cfg *cfg)
{
	int rc = 0, i = 0;
	struct device_node *node = NULL;
	struct device_node *root_node = NULL;
	struct sde_vp_cfg *vp;
	struct sde_vp_sub_blks *vp_sub, *vp_sub_next;
	struct property *prop;
	const char *cname;

	root_node = of_get_child_by_name(np, "qcom,sde-plane-id-map");
	if (!root_node) {
		root_node = of_parse_phandle(np, "qcom,sde-plane-id-map", 0);
		if (!root_node) {
			SDE_ERROR("No entry present for qcom,sde-plane-id-map");
			rc = -EINVAL;
			goto end;
		}
	}

	for_each_child_of_node(root_node, node) {
		if (i >= MAX_LAYERS) {
			SDE_ERROR("num of nodes(%d) is bigger than max(%d)\n",
					i, MAX_LAYERS);
			rc = -EINVAL;
			goto end;
		}
		cfg->vp_count++;
		vp = &(cfg->vp[i]);
		vp->id = i;
		rc = of_property_read_string(node, "qcom,display-type",
						&(vp->display_type));
		if (rc) {
			SDE_ERROR("failed to read display-type, rc = %d\n", rc);
			goto end;
		}

		rc = of_property_read_string(node, "qcom,plane-type",
						&(vp->plane_type));
		if (rc) {
			SDE_ERROR("failed to read plane-type, rc = %d\n", rc);
			goto end;
		}

		INIT_LIST_HEAD(&vp->sub_blks);
		of_property_for_each_string(node, "qcom,plane-name",
						prop, cname) {
			vp_sub = kzalloc(sizeof(*vp_sub), GFP_KERNEL);
			if (!vp_sub) {
				rc = -ENOMEM;
				goto end;
			}
			vp_sub->sspp_id = _sde_parse_sspp_id(cfg, cname);
			list_add_tail(&vp_sub->list, &vp->sub_blks);
		}
		i++;
	}

end:
	if (rc && cfg->vp_count) {
		vp = &(cfg->vp[i]);
		for (i = 0; i < cfg->vp_count; i++) {
			list_for_each_entry_safe(vp_sub, vp_sub_next,
				&vp->sub_blks, list) {
				list_del(&vp_sub->list);
				kfree(vp_sub);
			}
		}
		memset(&(cfg->vp[0]), 0x00,
			sizeof(struct sde_vp_cfg) * MAX_LAYERS);
		cfg->vp_count = 0;
	}
	return rc;
}

void sde_hw_catalog_deinit(struct sde_mdss_cfg *cfg)
{
	struct sde_vp_sub_blks *vp_sub, *vp_sub_next;
	int i;

	for (i = 0; i < cfg->vp_count; i++) {
		list_for_each_entry_safe(vp_sub, vp_sub_next,
			&cfg->vp[i].sub_blks, list) {
			list_del(&vp_sub->list);
			kfree(vp_sub);
		}
	}
}

/**
 * sde_hw_catalog_init: Returns the catalog information for the
 * passed HW version
 * @major:  Major version of the MDSS HW
 * @minor: Minor version
 * @step: step version
 */
struct sde_mdss_cfg *sde_hw_catalog_init(struct drm_device *dev, u32 major,
	u32 minor, u32 step)
{
	int i;
	struct sde_mdss_cfg *cfg = NULL;

	if (!dev || !dev->dev) {
		SDE_ERROR("dev=%p or dev->dev is NULL\n", dev);
		return NULL;
	}

	for (i = 0; i < ARRAY_SIZE(cfg_table); i++) {
		if ((cfg_table[i].major == major) &&
		(cfg_table[i].minor == minor)) {
			cfg = cfg_table[i].cfg_init(step);
			break;
		}
	}

	if (cfg) {
		if (dev->dev->of_node) {
			_sde_parse_dt(dev->dev->of_node, cfg);
			return cfg;
		}
	}

	return ERR_PTR(-ENODEV);
}
