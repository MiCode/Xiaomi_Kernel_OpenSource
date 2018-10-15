/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/device.h>
#include <linux/io.h>
#include <linux/of_platform.h>
#include <linux/clk-provider.h>

#include "kgsl_device.h"
#include "kgsl_rgmu.h"
#include "kgsl_gmu_core.h"
#include "kgsl_trace.h"
#include "adreno.h"

#define RGMU_CLK_FREQ 200000000

static int rgmu_irq_probe(struct kgsl_device *device)
{
	struct rgmu_device *rgmu = KGSL_RGMU_DEVICE(device);
	int ret;

	rgmu->oob_interrupt_num = platform_get_irq_byname(rgmu->pdev,
					"kgsl_oob");

	ret = devm_request_irq(&rgmu->pdev->dev,
				rgmu->oob_interrupt_num,
				oob_irq_handler, IRQF_TRIGGER_HIGH,
				"kgsl-oob", device);
	if (ret) {
		dev_err(&rgmu->pdev->dev,
				"Request kgsl-oob interrupt failed:%d\n", ret);
		return ret;
	}

	rgmu->rgmu_interrupt_num = platform_get_irq_byname(rgmu->pdev,
			"kgsl_rgmu");

	ret = devm_request_irq(&rgmu->pdev->dev,
			rgmu->rgmu_interrupt_num,
			rgmu_irq_handler, IRQF_TRIGGER_HIGH,
			"kgsl-rgmu", device);
	if (ret)
		dev_err(&rgmu->pdev->dev,
				"Request kgsl-rgmu interrupt failed:%d\n", ret);

	return ret;
}

static int rgmu_regulators_probe(struct rgmu_device *rgmu,
		struct device_node *node)
{
	int ret;

	rgmu->cx_gdsc = devm_regulator_get(&rgmu->pdev->dev, "vddcx");
	if (IS_ERR_OR_NULL(rgmu->cx_gdsc)) {
		ret = PTR_ERR(rgmu->cx_gdsc);
		dev_err(&rgmu->pdev->dev,
				"Couldn't get CX gdsc error:%d\n", ret);
		rgmu->cx_gdsc = NULL;
		return ret;
	}

	rgmu->gx_gdsc = devm_regulator_get(&rgmu->pdev->dev, "vdd");
	if (IS_ERR_OR_NULL(rgmu->gx_gdsc)) {
		ret = PTR_ERR(rgmu->gx_gdsc);
		dev_err(&rgmu->pdev->dev,
				"Couldn't get GX gdsc error:%d\n", ret);
		rgmu->gx_gdsc = NULL;
		return ret;
	}

	return 0;
}

static int rgmu_clocks_probe(struct rgmu_device *rgmu, struct device_node *node)
{
	const char *cname;
	struct property *prop;
	struct clk *c;
	int i = 0;

	of_property_for_each_string(node, "clock-names", prop, cname) {

		if (i >= ARRAY_SIZE(rgmu->clks)) {
			dev_err(&rgmu->pdev->dev,
				"dt: too many RGMU clocks defined\n");
			return -EINVAL;
		}

		c = devm_clk_get(&rgmu->pdev->dev, cname);
		if (IS_ERR_OR_NULL(c)) {
			dev_err(&rgmu->pdev->dev,
				"dt: Couldn't get clock: %s\n", cname);
			return PTR_ERR(c);
		}

		/* Remember the key clocks that we need to control later */
		if (!strcmp(cname, "core"))
			rgmu->gpu_clk = c;
		else if (!strcmp(cname, "gmu"))
			rgmu->rgmu_clk = c;

		rgmu->clks[i++] = c;
	}

	return 0;
}

static inline int rgmu_clk_set_rate(struct clk *grp_clk, unsigned int freq)
{
	int ret = clk_set_rate(grp_clk, freq);

	if (ret)
		pr_err("%s set freq %d failed:%d\n",
				__clk_get_name(grp_clk), freq, ret);

	return ret;
}


static void rgmu_disable_clks(struct kgsl_device *device)
{
	struct rgmu_device *rgmu = KGSL_RGMU_DEVICE(device);
	struct gmu_dev_ops *gmu_dev_ops = GMU_DEVICE_OPS(device);
	int j = 0, ret;

	/* Check GX GDSC is status */
	if (gmu_dev_ops->gx_is_on(ADRENO_DEVICE(device))) {

		if (IS_ERR_OR_NULL(rgmu->gx_gdsc))
			return;

		/*
		 * Switch gx gdsc control from RGMU to CPU. Force non-zero
		 * reference count in clk driver so next disable call will
		 * turn off the GDSC.
		 */
		ret = regulator_enable(rgmu->gx_gdsc);
		if (ret)
			dev_err(&rgmu->pdev->dev,
					"Fail to enable gx gdsc:%d\n", ret);

		ret = regulator_disable(rgmu->gx_gdsc);
		if (ret)
			dev_err(&rgmu->pdev->dev,
					"Fail to disable gx gdsc:%d\n", ret);

		if (gmu_dev_ops->gx_is_on(ADRENO_DEVICE(device)))
			dev_err(&rgmu->pdev->dev, "gx is stuck on\n");
	}

	for (j = 0; j < ARRAY_SIZE(rgmu->clks); j++)
		clk_disable_unprepare(rgmu->clks[j]);

	clear_bit(GMU_CLK_ON, &device->gmu_core.flags);
}

static int rgmu_enable_clks(struct kgsl_device *device)
{
	int ret, j = 0;
	struct rgmu_device *rgmu = KGSL_RGMU_DEVICE(device);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;

	if (IS_ERR_OR_NULL(rgmu->rgmu_clk) ||
			IS_ERR_OR_NULL(rgmu->gpu_clk))
		return -EINVAL;

	/* Let us set rgmu clk */
	ret = rgmu_clk_set_rate(rgmu->rgmu_clk, RGMU_CLK_FREQ);
	if (ret)
		return ret;

	/* Let us set gpu clk to default power level */
	ret = rgmu_clk_set_rate(rgmu->gpu_clk,
			rgmu->gpu_freqs[pwr->default_pwrlevel]);
	if (ret)
		return ret;

	for (j = 0; j < ARRAY_SIZE(rgmu->clks); j++) {
		ret = clk_prepare_enable(rgmu->clks[j]);
		if (ret) {
			dev_err(&rgmu->pdev->dev,
					"Fail(%d) to enable gpucc clk idx %d\n",
					ret, j);
			return ret;
		}
	}

	set_bit(GMU_CLK_ON, &device->gmu_core.flags);
	return 0;
}

static int rgmu_disable_gdsc(struct kgsl_device *device)
{
	struct rgmu_device *rgmu = KGSL_RGMU_DEVICE(device);
	int ret = 0;

	if (IS_ERR_OR_NULL(rgmu->cx_gdsc))
		return 0;

	ret = regulator_disable(rgmu->cx_gdsc);
	if (ret)
		dev_err(&rgmu->pdev->dev,
				"Failed to disable CX gdsc:%d\n", ret);

	return ret;
}

static int rgmu_enable_gdsc(struct rgmu_device *rgmu)
{
	int ret;

	if (IS_ERR_OR_NULL(rgmu->cx_gdsc))
		return 0;

	ret = regulator_enable(rgmu->cx_gdsc);
	if (ret)
		dev_err(&rgmu->pdev->dev,
			"Fail to enable CX gdsc:%d\n", ret);

	return ret;
}

static void rgmu_snapshot(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct gmu_dev_ops *gmu_dev_ops = GMU_DEVICE_OPS(device);
	struct rgmu_device *rgmu = KGSL_RGMU_DEVICE(device);

	/* Mask so there's no interrupt caused by NMI */
	adreno_write_gmureg(adreno_dev,
			ADRENO_REG_GMU_GMU2HOST_INTR_MASK, 0xFFFFFFFF);

	/* Make sure the interrupt is masked */
	wmb();

	kgsl_device_snapshot(device, NULL, true);

	adreno_write_gmureg(adreno_dev,
			ADRENO_REG_GMU_GMU2HOST_INTR_CLR, 0xFFFFFFFF);
	adreno_write_gmureg(adreno_dev,
			ADRENO_REG_GMU_GMU2HOST_INTR_MASK,
			~(gmu_dev_ops->gmu2host_intr_mask));

	rgmu->fault_count++;
}

/* Caller shall ensure GPU is ready for SLUMBER */
static void rgmu_stop(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct gmu_dev_ops *gmu_dev_ops = GMU_DEVICE_OPS(device);

	if (!test_bit(GMU_CLK_ON, &device->gmu_core.flags))
		return;

	/* Wait for the lowest idle level we requested */
	if (gmu_dev_ops->wait_for_lowest_idle(adreno_dev))
		goto error;

	gmu_dev_ops->rpmh_gpu_pwrctrl(adreno_dev,
			GMU_NOTIFY_SLUMBER, 0, 0);

	gmu_dev_ops->irq_disable(device);
	rgmu_disable_clks(device);
	rgmu_disable_gdsc(device);
	return;

error:

	/*
	 * The power controller will change state to SLUMBER anyway
	 * Set GMU_FAULT flag to indicate to power contrller
	 * that hang recovery is needed to power on GPU
	 */
	set_bit(GMU_FAULT, &device->gmu_core.flags);
	gmu_dev_ops->irq_disable(device);
	rgmu_snapshot(device);
}

/* Do not access any RGMU registers in RGMU probe function */
static int rgmu_probe(struct kgsl_device *device, struct device_node *node)
{
	struct rgmu_device *rgmu;
	struct platform_device *pdev = of_find_device_by_node(node);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	struct resource *res;
	int i, ret = -ENXIO;

	rgmu = devm_kzalloc(&pdev->dev, sizeof(*rgmu), GFP_KERNEL);

	if (rgmu == NULL)
		return -ENOMEM;

	rgmu->pdev = pdev;

	/* Set up RGMU regulators */
	ret = rgmu_regulators_probe(rgmu, node);
	if (ret)
		return ret;

	/* Set up RGMU clocks */
	ret = rgmu_clocks_probe(rgmu, node);
	if (ret)
		return ret;

	/* Map and reserve RGMU CSRs registers */
	res = platform_get_resource_byname(rgmu->pdev,
			IORESOURCE_MEM, "kgsl_rgmu");
	if (res == NULL) {
		dev_err(&rgmu->pdev->dev,
				"platform_get_resource failed\n");
		return -EINVAL;
	}

	if (res->start == 0 || resource_size(res) == 0) {
		dev_err(&rgmu->pdev->dev,
				"Register region is invalid\n");
		return -EINVAL;
	}

	rgmu->reg_phys = res->start;
	rgmu->reg_len = resource_size(res);
	device->gmu_core.reg_virt = devm_ioremap(&rgmu->pdev->dev, res->start,
			resource_size(res));

	if (device->gmu_core.reg_virt == NULL) {
		dev_err(&rgmu->pdev->dev, "Unable to remap rgmu registers\n");
		return -ENODEV;
	}

	device->gmu_core.gmu2gpu_offset =
			(rgmu->reg_phys - device->reg_phys) >> 2;
	device->gmu_core.reg_len = rgmu->reg_len;
	device->gmu_core.ptr = (void *)rgmu;

	/* Initialize OOB and RGMU interrupts */
	ret = rgmu_irq_probe(device);
	if (ret)
		return ret;

	/* Don't enable RGMU interrupts until RGMU started */
	/* We cannot use rgmu_irq_disable because it writes registers */
	disable_irq(rgmu->rgmu_interrupt_num);
	disable_irq(rgmu->oob_interrupt_num);

	/* Retrieves GPU power level configurations */
	for (i = 0; i < pwr->num_pwrlevels; i++)
		rgmu->gpu_freqs[i] = pwr->pwrlevels[i].gpu_freq;

	rgmu->num_gpupwrlevels = pwr->num_pwrlevels;

	/* Set up RGMU idle states */
	if (ADRENO_FEATURE(ADRENO_DEVICE(device), ADRENO_IFPC))
		rgmu->idle_level = GPU_HW_IFPC;
	else
		rgmu->idle_level = GPU_HW_ACTIVE;

	set_bit(GMU_ENABLED, &device->gmu_core.flags);
	device->gmu_core.dev_ops = &adreno_a6xx_rgmudev;

	return 0;
}

static int rgmu_suspend(struct kgsl_device *device)
{
	struct gmu_dev_ops *gmu_dev_ops = GMU_DEVICE_OPS(device);

	if (!test_bit(GMU_CLK_ON, &device->gmu_core.flags))
		return 0;

	gmu_dev_ops->irq_disable(device);

	if (gmu_dev_ops->rpmh_gpu_pwrctrl(ADRENO_DEVICE(device),
				GMU_SUSPEND, 0, 0))
		return -EINVAL;

	rgmu_disable_clks(device);
	return rgmu_disable_gdsc(device);
}

/* To be called to power on both GPU and RGMU */
static int rgmu_start(struct kgsl_device *device)
{
	int ret = 0;
	struct gmu_dev_ops *gmu_dev_ops = GMU_DEVICE_OPS(device);
	struct rgmu_device *rgmu = KGSL_RGMU_DEVICE(device);

	switch (device->state) {
	case KGSL_STATE_RESET:
		ret = rgmu_suspend(device);
		if (ret)
			goto error_rgmu;
	case KGSL_STATE_INIT:
	case KGSL_STATE_SUSPEND:
	case KGSL_STATE_SLUMBER:
		rgmu_enable_gdsc(rgmu);
		rgmu_enable_clks(device);
		gmu_dev_ops->irq_enable(device);
		ret = gmu_dev_ops->rpmh_gpu_pwrctrl(ADRENO_DEVICE(device),
				GMU_FW_START, GMU_COLD_BOOT, 0);
		if (ret)
			goto error_rgmu;
		break;
	}
	/* Request default DCVS level */
	kgsl_pwrctrl_set_default_gpu_pwrlevel(device);
	return 0;

error_rgmu:
	set_bit(GMU_FAULT, &device->gmu_core.flags);
	gmu_dev_ops->irq_disable(device);
	rgmu_snapshot(device);
	return ret;
}

/*
 * rgmu_dcvs_set() - Change GPU frequency and/or bandwidth.
 * @rgmu: Pointer to RGMU device
 * @pwrlevel: index to GPU DCVS table used by KGSL
 * @bus_level: index to GPU bus table used by KGSL
 *
 * The function converts GPU power level and bus level index used by KGSL
 * to index being used by GMU/RPMh.
 */
static int rgmu_dcvs_set(struct kgsl_device *device,
		unsigned int pwrlevel, unsigned int bus_level)
{
	struct rgmu_device *rgmu = KGSL_RGMU_DEVICE(device);

	if (pwrlevel == INVALID_DCVS_IDX)
		return -EINVAL;

	return rgmu_clk_set_rate(rgmu->gpu_clk,
			rgmu->gpu_freqs[pwrlevel]);

}

static bool rgmu_regulator_isenabled(struct kgsl_device *device)
{
	struct rgmu_device *rgmu = KGSL_RGMU_DEVICE(device);

	return (rgmu->gx_gdsc && regulator_is_enabled(rgmu->gx_gdsc));
}

struct gmu_core_ops rgmu_ops = {
	.probe = rgmu_probe,
	.remove = rgmu_stop,
	.start = rgmu_start,
	.stop = rgmu_stop,
	.dcvs_set = rgmu_dcvs_set,
	.snapshot = rgmu_snapshot,
	.regulator_isenabled = rgmu_regulator_isenabled,
};
