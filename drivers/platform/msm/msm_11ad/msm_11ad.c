/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/msm_pcie.h>
#include <asm/dma-iommu.h>
#include <linux/msm-bus.h>
#include <linux/iommu.h>
#include <linux/qcom_iommu.h>
#include <linux/version.h>
#include <linux/delay.h>
#include "wil_platform.h"
#include "msm_11ad.h"

#define WIGIG_VENDOR (0x1ae9)
#define WIGIG_DEVICE (0x0310)

#define SMMU_BASE	0x10000000 /* Device address range base */
#define SMMU_SIZE	0x40000000 /* Device address range size */

#define WIGIG_ENABLE_DELAY	50
#define PM_OPT_SUSPEND (MSM_PCIE_CONFIG_NO_CFG_RESTORE | \
			MSM_PCIE_CONFIG_LINKDOWN)
#define PM_OPT_RESUME MSM_PCIE_CONFIG_NO_CFG_RESTORE

struct device;

static const char * const gpio_en_name = "qcom,wigig-en";
static const char * const sleep_clk_en_name = "qcom,sleep-clk-en";

struct msm11ad_ctx {
	struct list_head list;
	struct device *dev; /* for platform device */
	int gpio_en; /* card enable */
	int sleep_clk_en; /* sleep clock enable for low PM management */

	/* pci device */
	u32 rc_index; /* PCIE root complex index */
	struct pci_dev *pcidev;
	struct pci_saved_state *pristine_state;

	/* SMMU */
	bool use_smmu; /* have SMMU enabled? */
	struct dma_iommu_mapping *mapping;

	/* bus frequency scaling */
	struct msm_bus_scale_pdata *bus_scale;
	u32 msm_bus_handle;
};

static LIST_HEAD(dev_list);

static struct msm11ad_ctx *pcidev2ctx(struct pci_dev *pcidev)
{
	struct msm11ad_ctx *ctx;

	list_for_each_entry(ctx, &dev_list, list) {
		if (ctx->pcidev == pcidev)
			return ctx;
	}
	return NULL;
}

static int ops_suspend(void *handle)
{
	int rc;
	struct msm11ad_ctx *ctx = handle;
	struct pci_dev *pcidev;

	pr_info("%s(%p)\n", __func__, handle);
	if (!ctx) {
		pr_err("No context\n");
		return -ENODEV;
	}
	pcidev = ctx->pcidev;
	rc = pci_save_state(pcidev);
	if (rc) {
		dev_err(ctx->dev, "pci_save_state failed :%d\n", rc);
		return rc;
	}
	rc = msm_pcie_pm_control(MSM_PCIE_SUSPEND, pcidev->bus->number,
				 pcidev, NULL, PM_OPT_SUSPEND);
	if (rc) {
		dev_err(ctx->dev, "msm_pcie_pm_control(SUSPEND) failed :%d\n",
			rc);
		return rc;
	}
	if (ctx->gpio_en >= 0)
		gpio_direction_output(ctx->gpio_en, 0);

	if (ctx->sleep_clk_en >= 0)
		gpio_direction_output(ctx->sleep_clk_en, 0);
	return rc;
}

static int ops_resume(void *handle)
{
	int rc;
	struct msm11ad_ctx *ctx = handle;
	struct pci_dev *pcidev;

	pr_info("%s(%p)\n", __func__, handle);
	if (!ctx) {
		pr_err("No context\n");
		return -ENODEV;
	}

	if (ctx->sleep_clk_en >= 0)
		gpio_direction_output(ctx->sleep_clk_en, 1);

	pcidev = ctx->pcidev;
	if (ctx->gpio_en >= 0) {
		gpio_direction_output(ctx->gpio_en, 1);
		msleep(WIGIG_ENABLE_DELAY);
	}

	rc = msm_pcie_pm_control(MSM_PCIE_RESUME, pcidev->bus->number,
				 pcidev, NULL, PM_OPT_RESUME);
	if (rc) {
		dev_err(ctx->dev, "msm_pcie_pm_control(RESUME) failed :%d\n",
			rc);
		goto err_disable_power;
	}
	rc = msm_pcie_recover_config(pcidev);
	if (rc) {
		dev_err(ctx->dev, "msm_pcie_recover_config failed :%d\n",
			rc);
		goto err_suspend_rc;
	}

	return 0;

err_suspend_rc:
	msm_pcie_pm_control(MSM_PCIE_SUSPEND, pcidev->bus->number,
			    pcidev, NULL, PM_OPT_SUSPEND);
err_disable_power:
	if (ctx->gpio_en >= 0)
		gpio_direction_output(ctx->gpio_en, 0);

	if (ctx->sleep_clk_en >= 0)
		gpio_direction_output(ctx->sleep_clk_en, 0);
	return rc;
}

static int msm_11ad_smmu_init(struct msm11ad_ctx *ctx)
{
	int disable_htw = 1;
	int atomic_ctx = 1;
	int rc;

	if (!ctx->use_smmu)
		return 0;

	ctx->mapping = arm_iommu_create_mapping(
			msm_iommu_get_bus(&ctx->pcidev->dev),
			SMMU_BASE, SMMU_SIZE);
	if (IS_ERR_OR_NULL(ctx->mapping)) {
		rc = PTR_ERR(ctx->mapping) ?: -ENODEV;
		dev_err(ctx->dev, "Failed to create IOMMU mapping (%d)\n", rc);
		return rc;
	}
	dev_info(ctx->dev, "IOMMU mapping created: %p\n", ctx->mapping);

	rc = iommu_domain_set_attr(ctx->mapping->domain,
				   DOMAIN_ATTR_COHERENT_HTW_DISABLE,
				   &disable_htw);
	if (rc) {
		/* This error can be ignored and not considered fatal,
		 * but let the users know this happened
		 */
		dev_err(ctx->dev, "Warning: disable coherent HTW failed (%d)\n",
			rc);
	}

	rc = iommu_domain_set_attr(ctx->mapping->domain,
				   DOMAIN_ATTR_ATOMIC,
				   &atomic_ctx);
	if (rc) {
		dev_err(ctx->dev, "Set atomic attribute to SMMU failed (%d)\n",
			rc);
		goto release_mapping;
	}

	rc = arm_iommu_attach_device(&ctx->pcidev->dev, ctx->mapping);
	if (rc) {
		dev_err(ctx->dev, "arm_iommu_attach_device failed (%d)\n", rc);
		goto release_mapping;
	}
	dev_info(ctx->dev, "attached to IOMMU\n");

	return 0;
release_mapping:
	arm_iommu_release_mapping(ctx->mapping);
	ctx->mapping = NULL;
	return rc;
}

static int msm_11ad_probe(struct platform_device *pdev)
{
	struct msm11ad_ctx *ctx;
	struct device *dev = &pdev->dev;
	struct device_node *of_node = dev->of_node;
	struct device_node *rc_node;
	struct pci_dev *pcidev = NULL;
	int rc;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->dev = dev;

	/*== parse ==*/

	/* Information pieces:
	 * - of_node stands for "wil6210":
	 *	wil6210: qcom,wil6210 {
	 *	compatible = "qcom,wil6210";
	 *	qcom,pcie-parent = <&pcie1>;
	 *	qcom,wigig-en = <&tlmm 94 0>; (ctx->gpio_en)
	 *	qcom,sleep-clk-en = <&pm8994_gpios 18 0>; (ctx->sleep_clk_en)
	 *	qcom,msm-bus,name = "wil6210";
	 *	qcom,msm-bus,num-cases = <2>;
	 *	qcom,msm-bus,num-paths = <1>;
	 *	qcom,msm-bus,vectors-KBps =
	 *		<100 512 0 0>,
	 *		<100 512 600000 800000>;
	 *	qcom,smmu-support;
	 *};
	 * rc_node stands for "qcom,pcie", selected entries:
	 * cell-index = <1>; (ctx->rc_index)
	 * iommus = <&anoc0_smmu>;
	 * qcom,smmu-exist;
	 */

	/* wigig-en is optional property */
	ctx->gpio_en = of_get_named_gpio(of_node, gpio_en_name, 0);
	if (ctx->gpio_en < 0)
		dev_warn(ctx->dev, "GPIO <%s> not found, enable GPIO not used\n",
			gpio_en_name);
	ctx->sleep_clk_en = of_get_named_gpio(of_node, sleep_clk_en_name, 0);
	if (ctx->sleep_clk_en < 0)
		dev_warn(ctx->dev, "GPIO <%s> not found, sleep clock not used\n",
			 sleep_clk_en_name);
	rc_node = of_parse_phandle(of_node, "qcom,pcie-parent", 0);
	if (!rc_node) {
		dev_err(ctx->dev, "Parent PCIE device not found\n");
		return -EINVAL;
	}
	rc = of_property_read_u32(rc_node, "cell-index", &ctx->rc_index);
	if (rc < 0) {
		dev_err(ctx->dev, "Parent PCIE device index not found\n");
		return -EINVAL;
	}
	ctx->use_smmu = of_property_read_bool(of_node, "qcom,smmu-support");
	ctx->bus_scale = msm_bus_cl_get_pdata(pdev);

	/*== execute ==*/
	/* turn device on */
	if (ctx->gpio_en >= 0) {
		rc = gpio_request(ctx->gpio_en, gpio_en_name);
		if (rc < 0) {
			dev_err(ctx->dev, "failed to request GPIO %d <%s>\n",
				ctx->gpio_en, gpio_en_name);
			goto out_req;
		}
		rc = gpio_direction_output(ctx->gpio_en, 1);
		if (rc < 0) {
			dev_err(ctx->dev, "failed to set GPIO %d <%s>\n",
				ctx->gpio_en, gpio_en_name);
			goto out_set;
		}
		msleep(WIGIG_ENABLE_DELAY);
	}

	/* enumerate it on PCIE */
	rc = msm_pcie_enumerate(ctx->rc_index);
	if (rc < 0) {
		dev_err(ctx->dev, "Parent PCIE enumeration failed\n");
		goto out_rc;
	}
	/* search for PCIE device in our domain */
	do {
		pcidev = pci_get_device(WIGIG_VENDOR, WIGIG_DEVICE, pcidev);
		if (!pcidev)
			break;

		if (pci_domain_nr(pcidev->bus) == ctx->rc_index)
			break;
	} while (true);
	if (!pcidev) {
		rc = -ENODEV;
		dev_err(ctx->dev, "Wigig device %4x:%4x not found\n",
			WIGIG_VENDOR, WIGIG_DEVICE);
		goto out_rc;
	}
	ctx->pcidev = pcidev;
	rc = pci_save_state(pcidev);
	if (rc) {
		dev_err(ctx->dev, "pci_save_state failed :%d\n", rc);
		goto out_rc;
	}
	ctx->pristine_state = pci_store_saved_state(pcidev);

	if (ctx->sleep_clk_en >= 0) {
		rc = gpio_request(ctx->sleep_clk_en, "msm_11ad");
		if (rc < 0) {
			dev_err(ctx->dev,
				"failed to request GPIO %d <%s>, sleep clock disabled\n",
				ctx->sleep_clk_en, sleep_clk_en_name);
			ctx->sleep_clk_en = -EINVAL;
		} else {
			gpio_direction_output(ctx->sleep_clk_en, 0);
		}
	}

	/* report */
	dev_info(ctx->dev, "msm_11ad discovered. %p {\n"
		 "  gpio_en = %d\n"
		 "  sleep_clk_en = %d\n"
		 "  rc_index = %d\n"
		 "  use_smmu = %d\n"
		 "  pcidev = %p\n"
		 "}\n", ctx, ctx->gpio_en, ctx->sleep_clk_en, ctx->rc_index,
		 ctx->use_smmu, ctx->pcidev);

	platform_set_drvdata(pdev, ctx);
	device_disable_async_suspend(&pcidev->dev);

	list_add_tail(&ctx->list, &dev_list);
	ops_suspend(ctx);

	return 0;
out_rc:
	if (ctx->gpio_en >= 0)
		gpio_direction_output(ctx->gpio_en, 0);
out_set:
	if (ctx->gpio_en >= 0)
		gpio_free(ctx->gpio_en);
out_req:
	ctx->gpio_en = -EINVAL;
	return rc;
}

static int msm_11ad_remove(struct platform_device *pdev)
{
	struct msm11ad_ctx *ctx = platform_get_drvdata(pdev);

	list_del(&ctx->list);
	dev_info(ctx->dev, "%s: pdev %p pcidev %p\n", __func__, pdev,
		 ctx->pcidev);
	kfree(ctx->pristine_state);

	msm_bus_cl_clear_pdata(ctx->bus_scale);
	pci_dev_put(ctx->pcidev);
	if (ctx->gpio_en >= 0) {
		gpio_direction_output(ctx->gpio_en, 0);
		gpio_free(ctx->gpio_en);
	}
	if (ctx->sleep_clk_en >= 0)
		gpio_free(ctx->sleep_clk_en);
	return 0;
}

static const struct of_device_id msm_11ad_of_match[] = {
	{ .compatible = "qcom,wil6210", },
	{},
};

static struct platform_driver msm_11ad_driver = {
	.driver = {
		.name = "msm_11ad",
		.of_match_table = msm_11ad_of_match,
	},
	.probe = msm_11ad_probe,
	.remove = msm_11ad_remove,
};
module_platform_driver(msm_11ad_driver);

/* hooks for the wil6210 driver */
static int ops_bus_request(void *handle, u32 kbps /* KBytes/Sec */)
{
	struct msm11ad_ctx *ctx = (struct msm11ad_ctx *)handle;
	int rc, i;
	int vote = 0; /* vote 0 in case requested kbps cannot be satisfied */
	struct msm_bus_paths *usecase;
	u32 usecase_kbps;
	u32 min_kbps = ~0;

	/* find the lowest usecase that is bigger than requested kbps */
	for (i = 0; i < ctx->bus_scale->num_usecases; i++) {
		usecase = &ctx->bus_scale->usecase[i];
		/* assume we have single path (vectors[0]). If we ever
		 * have multiple paths, need to define the behavior */
		usecase_kbps = div64_u64(usecase->vectors[0].ib, 1000);
		if (usecase_kbps >= kbps && usecase_kbps < min_kbps) {
			min_kbps = usecase_kbps;
			vote = i;
		}
	}

	rc = msm_bus_scale_client_update_request(ctx->msm_bus_handle, vote);
	if (rc)
		dev_err(ctx->dev,
			"Failed msm_bus voting. kbps=%d vote=%d, rc=%d\n",
			kbps, vote, rc);

	return rc;
}

static void ops_uninit(void *handle)
{
	struct msm11ad_ctx *ctx = (struct msm11ad_ctx *)handle;

	if (ctx->msm_bus_handle) {
		msm_bus_scale_unregister_client(ctx->msm_bus_handle);
		ctx->msm_bus_handle = 0;
	}

	if (ctx->use_smmu) {
		arm_iommu_detach_device(&ctx->pcidev->dev);
		arm_iommu_release_mapping(ctx->mapping);
		ctx->mapping = NULL;
	}
	ops_suspend(ctx);
}

void *msm_11ad_dev_init(struct device *dev, struct wil_platform_ops *ops)
{
	struct pci_dev *pcidev = to_pci_dev(dev);
	struct msm11ad_ctx *ctx = pcidev2ctx(pcidev);

	if (!ctx) {
		pr_err("Context not found for pcidev %p\n", pcidev);
		return NULL;
	}

	/* bus scale */
	ctx->msm_bus_handle =
		msm_bus_scale_register_client(ctx->bus_scale);
	if (!ctx->msm_bus_handle) {
		dev_err(ctx->dev, "Failed msm_bus registration\n");
		return NULL;
	}
	dev_info(ctx->dev, "msm_bus handle 0x%x\n", ctx->msm_bus_handle);
	/* smmu */
	if (msm_11ad_smmu_init(ctx)) {
		msm_bus_scale_unregister_client(ctx->msm_bus_handle);
		ctx->msm_bus_handle = 0;
		return NULL;
	}

	/* fill ops */
	memset(ops, 0, sizeof(*ops));
	ops->bus_request = ops_bus_request;
	ops->suspend = ops_suspend;
	ops->resume = ops_resume;
	ops->uninit = ops_uninit;

	return ctx;
}
EXPORT_SYMBOL(msm_11ad_dev_init);

int msm_11ad_modinit(void)
{
	struct msm11ad_ctx *ctx = list_first_entry_or_null(&dev_list,
							   struct msm11ad_ctx,
							   list);

	if (!ctx) {
		pr_err("Context not found\n");
		return -EINVAL;
	}

	if (ctx->pristine_state) {
		/* in old kernels, pci_load_saved_state() is not exported;
		 * so use pci_load_and_free_saved_state()
		 * and re-allocate ctx->saved_state again
		 */
		pci_load_and_free_saved_state(ctx->pcidev,
					      &ctx->pristine_state);
		ctx->pristine_state = pci_store_saved_state(ctx->pcidev);
	}

	return ops_resume(ctx);
}
EXPORT_SYMBOL(msm_11ad_modinit);

void msm_11ad_modexit(void)
{
}
EXPORT_SYMBOL(msm_11ad_modexit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Platform driver for qcom 11ad card");

