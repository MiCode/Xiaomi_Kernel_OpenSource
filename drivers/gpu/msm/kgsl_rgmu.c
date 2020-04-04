// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/clk-provider.h>
#include <linux/component.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/regulator/consumer.h>

#include "adreno.h"
#include "kgsl_device.h"
#include "kgsl_rgmu.h"
#include "kgsl_util.h"

#define RGMU_CLK_FREQ 200000000

static int rgmu_irq_probe(struct kgsl_device *device)
{
	struct rgmu_device *rgmu = KGSL_RGMU_DEVICE(device);
	int ret;

	ret = kgsl_request_irq(rgmu->pdev, "kgsl_oob", oob_irq_handler, device);
	if (ret < 0)
		return ret;

	rgmu->oob_interrupt_num  = ret;

	ret = kgsl_request_irq(rgmu->pdev,
		"kgsl_rgmu", rgmu_irq_handler, device);
	if (ret < 0)
		return ret;

	rgmu->rgmu_interrupt_num = ret;
	return 0;
}

static int rgmu_regulators_probe(struct rgmu_device *rgmu)
{
	int ret = 0;

	rgmu->cx_gdsc = devm_regulator_get(&rgmu->pdev->dev, "vddcx");
	if (IS_ERR(rgmu->cx_gdsc)) {
		ret = PTR_ERR(rgmu->cx_gdsc);
		if (ret != -EPROBE_DEFER)
			dev_err(&rgmu->pdev->dev,
				"Couldn't get CX gdsc error:%d\n", ret);
		return ret;
	}

	rgmu->gx_gdsc = devm_regulator_get(&rgmu->pdev->dev, "vdd");
	if (IS_ERR(rgmu->gx_gdsc)) {
		ret = PTR_ERR(rgmu->gx_gdsc);
		if (ret != -EPROBE_DEFER)
			dev_err(&rgmu->pdev->dev,
				"Couldn't get GX gdsc error:%d\n", ret);
	}

	return ret;
}

static int rgmu_clocks_probe(struct rgmu_device *rgmu, struct device_node *node)
{
	int ret;

	ret = devm_clk_bulk_get_all(&rgmu->pdev->dev, &rgmu->clks);
	if (ret < 0)
		return ret;

	rgmu->num_clks = ret;

	rgmu->gpu_clk = kgsl_of_clk_by_name(rgmu->clks, ret, "core");
	if (!rgmu->gpu_clk) {
		dev_err(&rgmu->pdev->dev, "The GPU clock isn't defined\n");
		return -ENODEV;
	}

	rgmu->rgmu_clk = kgsl_of_clk_by_name(rgmu->clks, ret, "gmu");
	if (!rgmu->rgmu_clk) {
		dev_err(&rgmu->pdev->dev, "The RGMU clock isn't defined\n");
		return -ENODEV;
	}

	return 0;
}

static void rgmu_disable_clks(struct kgsl_device *device)
{
	struct rgmu_device *rgmu = KGSL_RGMU_DEVICE(device);
	struct gmu_dev_ops *gmu_dev_ops = GMU_DEVICE_OPS(device);
	int  ret;

	/* Check GX GDSC is status */
	if (gmu_dev_ops->gx_is_on(device)) {

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

		if (gmu_dev_ops->gx_is_on(device))
			dev_err(&rgmu->pdev->dev, "gx is stuck on\n");
	}

	clk_bulk_disable_unprepare(rgmu->num_clks, rgmu->clks);

	clear_bit(GMU_CLK_ON, &device->gmu_core.flags);
}

static int rgmu_enable_clks(struct kgsl_device *device)
{
	int ret;
	struct rgmu_device *rgmu = KGSL_RGMU_DEVICE(device);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;

	ret = clk_set_rate(rgmu->rgmu_clk, RGMU_CLK_FREQ);
	if (ret) {
		dev_err(&rgmu->pdev->dev, "Couldn't set the RGMU clock\n");
		return ret;
	}

	ret = clk_set_rate(rgmu->gpu_clk,
		pwr->pwrlevels[pwr->default_pwrlevel].gpu_freq);
	if (ret) {
		dev_err(&rgmu->pdev->dev, "Couldn't set the GPU clock\n");
		return ret;
	}

	ret = clk_bulk_prepare_enable(rgmu->num_clks, rgmu->clks);
	if (ret) {
		dev_err(&rgmu->pdev->dev, "Failed to enable RGMU clocks\n");
		return ret;
	}

	set_bit(GMU_CLK_ON, &device->gmu_core.flags);
	return 0;
}

static int rgmu_disable_gdsc(struct kgsl_device *device)
{
	struct rgmu_device *rgmu = KGSL_RGMU_DEVICE(device);

	/* Wait up to 5 seconds for the regulator to go off */
	if (kgsl_regulator_disable_wait(rgmu->cx_gdsc, 5000))
		return 0;

	dev_err(&rgmu->pdev->dev, "RGMU CX gdsc off timeout\n");
	return -ETIMEDOUT;
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

	/*
	 * Halt RGMU execution so that GX will not
	 * be collapsed while dumping snapshot.
	 */
	gmu_dev_ops->halt_execution(device);

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
	struct gmu_dev_ops *gmu_dev_ops = GMU_DEVICE_OPS(device);

	if (!test_bit(GMU_CLK_ON, &device->gmu_core.flags))
		return;

	/* Wait for the lowest idle level we requested */
	if (gmu_dev_ops->wait_for_lowest_idle(device))
		goto error;

	gmu_dev_ops->rpmh_gpu_pwrctrl(device,
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
	rgmu_snapshot(device);
}

static struct gmu_core_ops rgmu_ops;

/* Do not access any RGMU registers in RGMU probe function */
static int rgmu_probe(struct kgsl_device *device, struct platform_device *pdev)
{
	struct rgmu_device *rgmu;
	struct resource *res;
	int ret = -ENXIO;

	rgmu = devm_kzalloc(&pdev->dev, sizeof(*rgmu), GFP_KERNEL);

	if (rgmu == NULL)
		return -ENOMEM;

	rgmu->pdev = pdev;

	/* Set up RGMU regulators */
	ret = rgmu_regulators_probe(rgmu);
	if (ret)
		return ret;

	/* Set up RGMU clocks */
	ret = rgmu_clocks_probe(rgmu, pdev->dev.of_node);
	if (ret)
		return ret;

	/* Map and reserve RGMU CSRs registers */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "kgsl_rgmu");
	if (!res) {
		dev_err(&pdev->dev, "The RGMU register region isn't defined\n");
		return -ENODEV;
	}

	device->gmu_core.gmu2gpu_offset = (res->start - device->reg_phys) >> 2;
	device->gmu_core.reg_len = resource_size(res);
	device->gmu_core.reg_virt = devm_ioremap_resource(&pdev->dev, res);

	if (IS_ERR(device->gmu_core.reg_virt)) {
		dev_err(&pdev->dev, "Unable to map the RGMU registers\n");
		return PTR_ERR(device->gmu_core.reg_virt);
	}

	device->gmu_core.ptr = (void *)rgmu;

	/* Initialize OOB and RGMU interrupts */
	ret = rgmu_irq_probe(device);
	if (ret)
		return ret;

	/* Don't enable RGMU interrupts until RGMU started */
	/* We cannot use rgmu_irq_disable because it writes registers */
	disable_irq(rgmu->rgmu_interrupt_num);
	disable_irq(rgmu->oob_interrupt_num);

	/* Set up RGMU idle states */
	if (ADRENO_FEATURE(ADRENO_DEVICE(device), ADRENO_IFPC))
		rgmu->idle_level = GPU_HW_IFPC;
	else
		rgmu->idle_level = GPU_HW_ACTIVE;

	set_bit(GMU_ENABLED, &device->gmu_core.flags);
	device->gmu_core.core_ops = &rgmu_ops;
	device->gmu_core.dev_ops = &adreno_a6xx_rgmudev;

	return 0;
}

static int rgmu_suspend(struct kgsl_device *device)
{
	struct gmu_dev_ops *gmu_dev_ops = GMU_DEVICE_OPS(device);

	if (!test_bit(GMU_CLK_ON, &device->gmu_core.flags))
		return 0;

	gmu_dev_ops->irq_disable(device);

	if (gmu_dev_ops->rpmh_gpu_pwrctrl(device, GMU_SUSPEND, 0, 0))
		return -EINVAL;

	rgmu_disable_clks(device);
	rgmu_disable_gdsc(device);
	return 0;
}

static int rgmu_init(struct kgsl_device *device)
{
	struct gmu_dev_ops *ops = GMU_DEVICE_OPS(device);
	int ret;

	ret = ops->load_firmware(device);
	if (ret)
		return ret;

	return 0;
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
		/* Fall-thru */
	case KGSL_STATE_INIT:
	case KGSL_STATE_SUSPEND:
	case KGSL_STATE_SLUMBER:
		rgmu_enable_gdsc(rgmu);
		rgmu_enable_clks(device);
		gmu_dev_ops->irq_enable(device);
		ret = gmu_dev_ops->rpmh_gpu_pwrctrl(device,
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
		int pwrlevel, int bus_level)
{
	struct rgmu_device *rgmu = KGSL_RGMU_DEVICE(device);
	int ret;
	unsigned long rate;

	if (pwrlevel == INVALID_DCVS_IDX)
		return -EINVAL;

	rate = device->pwrctrl.pwrlevels[pwrlevel].gpu_freq;

	ret = clk_set_rate(rgmu->gpu_clk, rate);
	if (ret)
		dev_err(&rgmu->pdev->dev, "Couldn't set the GPU clock\n");

	return ret;
}

static bool rgmu_regulator_isenabled(struct kgsl_device *device)
{
	struct rgmu_device *rgmu = KGSL_RGMU_DEVICE(device);

	return (rgmu->gx_gdsc && regulator_is_enabled(rgmu->gx_gdsc));
}

static void rgmu_remove(struct kgsl_device *device)
{
	rgmu_stop(device);

	memset(&device->gmu_core, 0, sizeof(device->gmu_core));
}

static struct gmu_core_ops rgmu_ops = {
	.init = rgmu_init,
	.start = rgmu_start,
	.stop = rgmu_stop,
	.dcvs_set = rgmu_dcvs_set,
	.snapshot = rgmu_snapshot,
	.regulator_isenabled = rgmu_regulator_isenabled,
	.suspend = rgmu_suspend,
};

static int kgsl_rgmu_bind(struct device *dev, struct device *master, void *data)
{
	struct kgsl_device *device = dev_get_drvdata(master);

	return rgmu_probe(device, to_platform_device(dev));
}

static void kgsl_rgmu_unbind(struct device *dev, struct device *master,
		void *data)
{
	struct kgsl_device *device = dev_get_drvdata(master);

	rgmu_remove(device);
}

static const struct component_ops kgsl_rgmu_ops = {
	.bind = kgsl_rgmu_bind,
	.unbind = kgsl_rgmu_unbind,
};

static int kgsl_rgmu_probe(struct platform_device *pdev)
{
	return component_add(&pdev->dev, &kgsl_rgmu_ops);
}

static int kgsl_rgmu_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &kgsl_rgmu_ops);
	return 0;
}

static const struct of_device_id kgsl_rgmu_match_table[] = {
	{ .compatible = "qcom,gpu-rgmu" },
	{ },
};

struct platform_driver kgsl_rgmu_driver = {
	.probe = kgsl_rgmu_probe,
	.remove = kgsl_rgmu_remove,
	.driver = {
		.name = "kgsl-rgmu",
		.of_match_table = kgsl_rgmu_match_table,
	},
};
