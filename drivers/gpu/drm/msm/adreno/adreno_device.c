/*
 * Copyright (C) 2013-2014 Red Hat
 * Author: Rob Clark <robdclark@gmail.com>
 *
 * Copyright (c) 2014 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/of_platform.h>
#include "adreno_gpu.h"
#include "pwrctl.h"

#if defined(CONFIG_MSM_BUS_SCALING) && !defined(CONFIG_OF)
#  include <linux/kgsl.h>
#endif

#define ANY_ID 0xff

bool hang_debug = false;
MODULE_PARM_DESC(hang_debug, "Dump registers when hang is detected (can be slow!)");
module_param_named(hang_debug, hang_debug, bool, 0600);

struct msm_gpu *a3xx_gpu_init(struct drm_device *dev);
struct msm_gpu *a4xx_gpu_init(struct drm_device *dev);
struct msm_gpu *a5xx_gpu_init(struct drm_device *dev);

static const struct adreno_info gpulist[] = {
	{
		.rev   = ADRENO_REV(5, 3, 0, 0),
		.revn  = 530,
		.name  = "A530",
		.pm4fw = "a530v1_pm4.fw",
		.pfpfw = "a530v1_pfp.fw",
		.gmem  = SZ_1M,
		.init  = a5xx_gpu_init,
	}, {
		.rev   = ADRENO_REV(5, 3, 0, ANY_ID),
		.revn  = 530,
		.name  = "A530",
		.pm4fw = "a530_pm4.fw",
		.pfpfw = "a530_pfp.fw",
		.zap_name = "a530_zap",
		.regfw_name = "a530v3_seq.fw2",
		.gmem  = SZ_1M,
		.init  = a5xx_gpu_init,
	},
};

MODULE_FIRMWARE("a530_pm4.fw");
MODULE_FIRMWARE("a530_pfp.fw");

static inline bool _rev_match(uint8_t entry, uint8_t id)
{
	return (entry == ANY_ID) || (entry == id);
}

const struct adreno_info *adreno_info(struct adreno_rev rev)
{
	int i;

	/* identify gpu: */
	for (i = 0; i < ARRAY_SIZE(gpulist); i++) {
		const struct adreno_info *info = &gpulist[i];
		if (_rev_match(info->rev.core, rev.core) &&
				_rev_match(info->rev.major, rev.major) &&
				_rev_match(info->rev.minor, rev.minor) &&
				_rev_match(info->rev.patchid, rev.patchid))
			return info;
	}

	return NULL;
}

struct msm_gpu *adreno_load_gpu(struct drm_device *dev)
{
	struct msm_drm_private *priv = dev->dev_private;
	struct platform_device *pdev = priv->gpu_pdev;
	struct adreno_platform_config *config;
	struct adreno_rev rev;
	const struct adreno_info *info;
	struct msm_gpu *gpu = NULL;

	if (!pdev) {
		dev_err(dev->dev, "no adreno device\n");
		return NULL;
	}

	config = pdev->dev.platform_data;
	rev = config->rev;
	info = adreno_info(config->rev);

	if (!info) {
		dev_warn(dev->dev, "Unknown GPU revision: %u.%u.%u.%u\n",
				rev.core, rev.major, rev.minor, rev.patchid);
		return NULL;
	}

	DBG("Found GPU: %u.%u.%u.%u",  rev.core, rev.major,
			rev.minor, rev.patchid);

	gpu = info->init(dev);
	if (IS_ERR(gpu)) {
		dev_warn(dev->dev, "failed to load adreno gpu\n");
		gpu = NULL;
		/* not fatal */
	}

	if (gpu) {
		int ret;
		mutex_lock(&dev->struct_mutex);
		gpu->funcs->pm_resume(gpu);
		mutex_unlock(&dev->struct_mutex);
		ret = gpu->funcs->hw_init(gpu);
		if (ret) {
			dev_err(dev->dev, "gpu hw init failed: %d\n", ret);
			gpu->funcs->destroy(gpu);
			gpu = NULL;
		} else {
			/* give inactive pm a chance to kick in: */
			msm_gpu_retire(gpu);
		}
	}

	return gpu;
}


struct msm_iommu *get_gpu_iommu(struct platform_device *pdev)
{
	struct adreno_platform_config *platform_config;

	platform_config = pdev->dev.platform_data;
	return &platform_config->iommu;
}

void enable_iommu_clks(struct platform_device *pdev)
{
	int j;
	struct msm_iommu *iommu = get_gpu_iommu(pdev);

	for (j = 0; j < KGSL_IOMMU_MAX_CLKS; j++) {
		if (iommu->clks[j])
			clk_prepare_enable(iommu->clks[j]);
	}
}

void disable_iommu_clks(struct platform_device *pdev)
{
	int j;
	struct msm_iommu *iommu = get_gpu_iommu(pdev);

	for (j = 0; j < KGSL_IOMMU_MAX_CLKS; j++) {
		if (iommu->clks[j])
			clk_disable_unprepare(iommu->clks[j]);
	}
}

static const struct {
	int id;
	char *name;
} kgsl_iommu_cbs[] = {
	{ KGSL_IOMMU_CONTEXT_USER, "gfx3d_user", },
	{ KGSL_IOMMU_CONTEXT_SECURE, "gfx3d_secure" },
};

static int _adreno_iommu_cb_probe(
		struct msm_iommu *iommu, struct device_node *node)
{
	struct platform_device *pdev = of_find_device_by_node(node);
	struct msm_iommu_context *ctx;
	int i;

	for (i = 0; i < ARRAY_SIZE(kgsl_iommu_cbs); i++) {
		if (!strcmp(node->name, kgsl_iommu_cbs[i].name)) {
			int id = kgsl_iommu_cbs[i].id;

			ctx = &iommu->ctx[id];
			ctx->id = id;
			ctx->cb_num = -1;
			ctx->name = kgsl_iommu_cbs[i].name;

			break;
		}
	}

	if (ctx == NULL) {
		pr_err("dt: Unknown context label %s\n", node->name);
		return -EINVAL;
	}

	/* this property won't be found for all context banks */
	if (of_property_read_u32(node, "qcom,gpu-offset", &ctx->gpu_offset))
		ctx->gpu_offset = UINT_MAX;

	/* arm-smmu driver we'll have the right device pointer here. */
	if (of_find_property(node, "iommus", NULL))
		ctx->dev = &pdev->dev;
	else
		return -EINVAL;

	return 0;
}

static const struct {
	char *feature;
	int bit;
} kgsl_iommu_features[] = {
	{ "qcom,retention", KGSL_MMU_RETENTION },
	{ "qcom,global_pt", KGSL_MMU_GLOBAL_PAGETABLE },
	{ "qcom,hyp_secure_alloc", KGSL_MMU_HYP_SECURE_ALLOC },
	{ "qcom,force-32bit", KGSL_MMU_FORCE_32BIT },
	{ "qcom,coherent-htw", KGSL_MMU_COHERENT_HTW },
};

static int adreno_iommu_probe(struct platform_device *pdev)
{
	int i = 0;
	struct device_node *node;
	const char *cname;
	struct property *prop;
	u32 reg_val[2];
	struct device_node *child;
	struct adreno_platform_config *platform_config;
	struct msm_iommu *iommu;
	struct platform_device *smmupdev;

	platform_config = pdev->dev.platform_data;
	iommu = &platform_config->iommu;

	node = of_find_compatible_node(pdev->dev.of_node,
		NULL, "qcom,kgsl-smmu-v1");

	if (node == NULL)
		node = of_find_compatible_node(pdev->dev.of_node,
			NULL, "qcom,kgsl-smmu-v2");

	if (node == NULL)
		return -ENODEV;

	smmupdev = of_find_device_by_node(node);
	BUG_ON(smmupdev == NULL);

	if (of_device_is_compatible(node, "qcom,kgsl-smmu-v1"))
		iommu->version = 1;
	else
		iommu->version = 2;

	if (of_property_read_u32_array(node, "reg", reg_val, 2)) {
		pr_err("dt: Unable to read KGSL IOMMU register range\n");
		return -EINVAL;
	}
	iommu->regstart = reg_val[0];
	iommu->regsize = reg_val[1];

	/* Protecting the SMMU registers is mandatory */
	if (of_property_read_u32_array(node, "qcom,protect", reg_val, 2)) {
		pr_err("dt: no iommu protection range specified\n");
		return -EINVAL;
	}
	iommu->protect_reg_base = reg_val[0] / sizeof(u32);
	iommu->protect_reg_range = ilog2(reg_val[1] / sizeof(u32));

	of_property_for_each_string(node, "clock-names", prop, cname) {
		struct clk *c = devm_clk_get(&smmupdev->dev, cname);

		if (IS_ERR(c)) {
			pr_err("dt: Couldn't get clock: %s\n", cname);
			return -ENODEV;
		}
		if (i >= KGSL_IOMMU_MAX_CLKS) {
			pr_err("dt: too many clocks defined.\n");
			return -EINVAL;
		}

		iommu->clks[i] = c;
		++i;
	}
	enable_iommu_clks(pdev);

	if (of_property_read_u32(node, "qcom,micro-mmu-control",
		&iommu->micro_mmu_ctrl))
		iommu->micro_mmu_ctrl = UINT_MAX;

	/* Fill out the rest of the devices in the node */
	of_platform_populate(node, NULL, NULL, &pdev->dev);

	for_each_child_of_node(node, child) {
		int ret = 0;

		if (!of_device_is_compatible(child, "qcom,smmu-kgsl-cb"))
			continue;

		ret = _adreno_iommu_cb_probe(iommu, child);
		if (ret)
			return ret;
	}

	return 0;
}

static void set_gpu_pdev(struct drm_device *dev,
		struct platform_device *pdev)
{
	struct msm_drm_private *priv = dev->dev_private;
	priv->gpu_pdev = pdev;
}

static int of_parse_legacy_clk(struct adreno_platform_config *config,
			      struct device_node *node)
{
	struct device_node *child;
	u32 val;
	int ret;

	for_each_available_child_of_node(node, child) {
		if (of_device_is_compatible(child, "qcom,gpu-pwrlevels")) {
			struct device_node *pwrlvl;

			for_each_available_child_of_node(child, pwrlvl) {
				ret = of_property_read_u32(pwrlvl,
					"qcom,gpu-freq", &val);
				if (ret)
					return ret;
				config->fast_rate = max(config->fast_rate, val);
				config->slow_rate = min(config->slow_rate, val);
			}
		}
	}
	return 0;
}

int efuse_read_u32(struct adreno_platform_config *config,
			  unsigned int offset,
			  unsigned int *val)
{
	if (config->efuse_base == NULL)
		return -ENODEV;

	if (offset >= config->efuse_len)
		return -ERANGE;

	if (val != NULL) {
		*val = readl_relaxed(config->efuse_base + offset);
		/* Make sure memory is updated before returning */
		rmb();
	}

	return 0;
}

static int of_parse_pwrlevels(struct adreno_platform_config *config,
			      struct device_node *node)
{
	struct drmgsl_pwrctl *pwrctl = config->pwrctl;
	struct drmgsl_pwrlevel *level;
	struct device_node *child;
	unsigned int index;
	int i = 0;

	for_each_available_child_of_node(node, child) {
		level = pwrctl->pwrlevels + i;

		if (of_property_read_u32(child, "reg", &index))
			return -EINVAL;

		if (of_property_read_u32(child, "qcom,gpu-freq",
			&level->gpu_freq))
			return -EINVAL;

		config->fast_rate = max(config->fast_rate, level->gpu_freq);
		config->slow_rate = min(config->slow_rate, level->gpu_freq);

		if (of_property_read_u32(child, "qcom,bus-freq",
			&level->bus_freq))
			return -EINVAL;

		if (of_property_read_u32(child, "qcom,bus-min",
			&level->bus_min))
			level->bus_min = level->bus_freq;

		if (of_property_read_u32(child, "qcom,bus-max",
			&level->bus_max))
			level->bus_max = level->bus_freq;
	}

	return 0;
}

static int of_parse_pwrlevel_bin(struct device *dev,
				struct adreno_platform_config *config,
				struct device_node *node)
{
	struct device_node *child;
	struct drmgsl_pwrctl *pwrctl;
	struct drmgsl_pwrlevel *pwrlevels;
	unsigned int n;

	pwrctl = devm_kzalloc(dev, sizeof(*config->pwrctl),
				      GFP_KERNEL);
	if (!pwrctl)
		return -ENOMEM;

	for_each_available_child_of_node(node, child) {
		unsigned int bin;

		if (of_property_read_u32(child, "qcom,speed-bin", &bin))
			continue;

		if (bin != config->speed_bin)
			continue;

		n = of_get_child_count(child);
		if (n == 0)
			return -EINVAL;

		pwrlevels = devm_kmalloc_array(dev, n,
					       sizeof(*pwrlevels),
					       GFP_KERNEL);
		if (!pwrlevels)
			return -ENOMEM;

		pwrctl->level_num = n;
		pwrctl->pwrlevels = pwrlevels;
		config->pwrctl = pwrctl;
		return of_parse_pwrlevels(config, child);
	}
	return -EINVAL;
}

static int adreno_bind(struct device *dev, struct device *master, void *data)
{
	static struct adreno_platform_config config = {};
	struct device_node *root = dev->of_node;
	struct device_node *node;
	u32 val;
	int ret;

	ret = of_property_read_u32(root, "qcom,chipid", &val);
	if (ret) {
		dev_err(dev, "could not find chipid: %d\n", ret);
		return ret;
	}

	config.rev = ADRENO_REV((val >> 24) & 0xff,
			(val >> 16) & 0xff, (val >> 8) & 0xff, val & 0xff);

	/* find clock rates: */
	config.fast_rate = 0;
	config.slow_rate = ~0;

	node = of_find_node_by_name(root, "qcom,gpu-pwrlevel-bins");
	if (node == NULL)
		ret = of_parse_legacy_clk(&config, root);
	else
		ret = of_parse_pwrlevel_bin(dev, &config, node);

	if (ret || !config.fast_rate) {
		dev_err(dev, "could not find clk rates\n");
		return -ENXIO;
	}

	dev->platform_data = &config;
	set_gpu_pdev(dev_get_drvdata(master), to_platform_device(dev));
	return adreno_iommu_probe(to_platform_device(dev));
}

static void adreno_unbind(struct device *dev, struct device *master,
		void *data)
{
	set_gpu_pdev(dev_get_drvdata(master), NULL);
}

static const struct component_ops a3xx_ops = {
		.bind   = adreno_bind,
		.unbind = adreno_unbind,
};

static int adreno_probe(struct platform_device *pdev)
{
	return component_add(&pdev->dev, &a3xx_ops);
}

static int adreno_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &a3xx_ops);
	return 0;
}

static const struct of_device_id dt_match[] = {
	{ .compatible = "qcom,adreno-3xx" },
	/* for backwards compat w/ downstream kgsl DT files: */
	{ .compatible = "qcom,kgsl-3d0" },
	{}
};

static struct platform_driver adreno_driver = {
	.probe = adreno_probe,
	.remove = adreno_remove,
	.driver = {
		.name = "adreno",
		.of_match_table = dt_match,
	},
};

void __init adreno_register(void)
{
	platform_driver_register(&adreno_driver);
}

void __exit adreno_unregister(void)
{
	platform_driver_unregister(&adreno_driver);
}
