/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/msm-bus.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/iommu.h>
#include <linux/interrupt.h>
#include <linux/msm-bus.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>

#include "msm_iommu_hw-v1.h"
#include <linux/qcom_iommu.h>
#include "msm_iommu_perfmon.h"

static struct of_device_id msm_iommu_ctx_match_table[];

#ifdef CONFIG_IOMMU_LPAE
static const char *BFB_REG_NODE_NAME = "qcom,iommu-lpae-bfb-regs";
static const char *BFB_DATA_NODE_NAME = "qcom,iommu-lpae-bfb-data";
#else
static const char *BFB_REG_NODE_NAME = "qcom,iommu-bfb-regs";
static const char *BFB_DATA_NODE_NAME = "qcom,iommu-bfb-data";
#endif

static int msm_iommu_parse_bfb_settings(struct platform_device *pdev,
				    struct msm_iommu_drvdata *drvdata)
{
	struct msm_iommu_bfb_settings *bfb_settings;
	u32 nreg, nval;
	int ret;

	/*
	 * It is not valid for a device to have the BFB_REG_NODE_NAME
	 * property but not the BFB_DATA_NODE_NAME property, and vice versa.
	 */
	if (!of_get_property(pdev->dev.of_node, BFB_REG_NODE_NAME, &nreg)) {
		if (of_get_property(pdev->dev.of_node, BFB_DATA_NODE_NAME,
				    &nval))
			return -EINVAL;
		return 0;
	}

	if (!of_get_property(pdev->dev.of_node, BFB_DATA_NODE_NAME, &nval))
		return -EINVAL;

	if (nreg >= sizeof(bfb_settings->regs))
		return -EINVAL;

	if (nval >= sizeof(bfb_settings->data))
		return -EINVAL;

	if (nval != nreg)
		return -EINVAL;

	bfb_settings = devm_kzalloc(&pdev->dev, sizeof(*bfb_settings),
				    GFP_KERNEL);
	if (!bfb_settings)
		return -ENOMEM;

	ret = of_property_read_u32_array(pdev->dev.of_node,
					 BFB_REG_NODE_NAME,
					 bfb_settings->regs,
					 nreg / sizeof(*bfb_settings->regs));
	if (ret)
		return ret;

	ret = of_property_read_u32_array(pdev->dev.of_node,
					 BFB_DATA_NODE_NAME,
					 bfb_settings->data,
					 nval / sizeof(*bfb_settings->data));
	if (ret)
		return ret;

	bfb_settings->length = nreg / sizeof(*bfb_settings->regs);

	drvdata->bfb_settings = bfb_settings;
	return 0;
}

static int __get_bus_vote_client(struct platform_device *pdev,
				  struct msm_iommu_drvdata *drvdata)
{
	int ret = 0;
	struct msm_bus_scale_pdata *bs_table;
	const char *dummy;

	/* Check whether bus scaling has been specified for this node */
	ret = of_property_read_string(pdev->dev.of_node, "qcom,msm-bus,name",
				      &dummy);
	if (ret)
		return 0;

	bs_table = msm_bus_cl_get_pdata(pdev);

	if (bs_table) {
		drvdata->bus_client = msm_bus_scale_register_client(bs_table);
		if (IS_ERR(&drvdata->bus_client)) {
			pr_err("%s(): Bus client register failed.\n", __func__);
			ret = -EINVAL;
		}
	}
	return ret;
}

static void __put_bus_vote_client(struct msm_iommu_drvdata *drvdata)
{
	msm_bus_scale_unregister_client(drvdata->bus_client);
	drvdata->bus_client = 0;
}

/*
 * CONFIG_IOMMU_NON_SECURE allows us to override the secure
 * designation of SMMUs in device tree. With this config enabled
 * all SMMUs will be programmed by this driver.
 */
#ifdef CONFIG_IOMMU_NON_SECURE
static inline void get_secure_id(struct device_node *node,
			  struct msm_iommu_drvdata *drvdata)
{
}

static inline void get_secure_ctx(struct device_node *node,
				  struct msm_iommu_drvdata *iommu_drvdata,
				  struct msm_iommu_ctx_drvdata *ctx_drvdata)
{
	ctx_drvdata->secure_context = 0;
}
#else

static inline int is_vfe_smmu(char const *iommu_name)
{
	return (strcmp(iommu_name, "vfe_iommu") == 0);
}

static void get_secure_id(struct device_node *node,
			  struct msm_iommu_drvdata *drvdata)
{
	if (msm_iommu_get_scm_call_avail()) {
		if (!is_vfe_smmu(drvdata->name) || is_vfe_secure())
			of_property_read_u32(node, "qcom,iommu-secure-id",
					     &drvdata->sec_id);
		else
			pr_info("vfe_iommu: Keeping vfe non-secure\n");
	}
}

static void get_secure_ctx(struct device_node *node,
			   struct msm_iommu_drvdata *iommu_drvdata,
			   struct msm_iommu_ctx_drvdata *ctx_drvdata)
{
	u32 secure_ctx = 0;

	if (msm_iommu_get_scm_call_avail()) {
		if (!is_vfe_smmu(iommu_drvdata->name) || is_vfe_secure()) {
			secure_ctx =
			of_property_read_bool(node, "qcom,secure-context");
		}
	}
	ctx_drvdata->secure_context = secure_ctx;
}
#endif

static int msm_iommu_parse_dt(struct platform_device *pdev,
				struct msm_iommu_drvdata *drvdata)
{
	struct device_node *child;
	int ret = 0;
	struct resource *r;

	drvdata->dev = &pdev->dev;

	ret = __get_bus_vote_client(pdev, drvdata);

	if (ret)
		goto fail;

	ret = msm_iommu_parse_bfb_settings(pdev, drvdata);
	if (ret)
		goto fail;

	for_each_child_of_node(pdev->dev.of_node, child)
		drvdata->ncb++;

	drvdata->asid = devm_kzalloc(&pdev->dev, drvdata->ncb * sizeof(int),
				     GFP_KERNEL);

	if (!drvdata->asid) {
		pr_err("Unable to get memory for asid array\n");
		ret = -ENOMEM;
		goto fail;
	}

	ret = of_property_read_string(pdev->dev.of_node, "label",
				      &drvdata->name);
	if (ret)
		goto fail;

	drvdata->sec_id = -1;
	get_secure_id(pdev->dev.of_node, drvdata);

	r = platform_get_resource_byname(pdev, IORESOURCE_MEM, "clk_base");
	if (r) {
		drvdata->clk_reg_virt = devm_ioremap(&pdev->dev, r->start,
						     resource_size(r));
		if (!drvdata->clk_reg_virt) {
			pr_err("Failed to map resource for iommu clk: %pr\n",
				r);
			ret = -ENOMEM;
			goto fail;
		}
	}

	drvdata->halt_enabled = of_property_read_bool(pdev->dev.of_node,
						      "qcom,iommu-enable-halt");

	ret = of_platform_populate(pdev->dev.of_node,
				   msm_iommu_ctx_match_table,
				   NULL, &pdev->dev);
	if (ret) {
		pr_err("Failed to create iommu context device\n");
		goto fail;
	}

	msm_iommu_add_drv(drvdata);
	return 0;

fail:
	__put_bus_vote_client(drvdata);
	return ret;
}

static int msm_iommu_pmon_parse_dt(struct platform_device *pdev,
					struct iommu_pmon *pmon_info)
{
	int ret = 0;
	int irq = platform_get_irq(pdev, 0);
	unsigned int cls_prop_size;

	if (irq > 0) {
		pmon_info->iommu.evt_irq = platform_get_irq(pdev, 0);

		ret = of_property_read_u32(pdev->dev.of_node,
					   "qcom,iommu-pmu-ngroups",
					   &pmon_info->num_groups);
		if (ret) {
			pr_err("Error reading qcom,iommu-pmu-ngroups\n");
			goto fail;
		}
		ret = of_property_read_u32(pdev->dev.of_node,
					   "qcom,iommu-pmu-ncounters",
					   &pmon_info->num_counters);
		if (ret) {
			pr_err("Error reading qcom,iommu-pmu-ncounters\n");
			goto fail;
		}

		if (!of_get_property(pdev->dev.of_node,
				     "qcom,iommu-pmu-event-classes",
				     &cls_prop_size)) {
			pr_err("Error reading qcom,iommu-pmu-event-classes\n");
			return -EINVAL;
		}

		pmon_info->event_cls_supported =
			   devm_kzalloc(&pdev->dev, cls_prop_size, GFP_KERNEL);

		if (!pmon_info->event_cls_supported) {
			pr_err("Unable to get memory for event class array\n");
			return -ENOMEM;
		}

		pmon_info->nevent_cls_supported = cls_prop_size / sizeof(u32);

		ret = of_property_read_u32_array(pdev->dev.of_node,
					"qcom,iommu-pmu-event-classes",
					pmon_info->event_cls_supported,
					pmon_info->nevent_cls_supported);
		if (ret) {
			pr_err("Error reading qcom,iommu-pmu-event-classes\n");
			return ret;
		}
	} else {
		pmon_info->iommu.evt_irq = -1;
		ret = irq;
	}

fail:
	return ret;
}

static int msm_iommu_probe(struct platform_device *pdev)
{
	struct iommu_pmon *pmon_info;
	struct msm_iommu_drvdata *drvdata;
	struct resource *r;
	int ret, needs_alt_core_clk, needs_alt_iface_clk;
	int global_cfg_irq, global_client_irq;
	u32 temp;

	drvdata = devm_kzalloc(&pdev->dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	r = platform_get_resource_byname(pdev, IORESOURCE_MEM, "iommu_base");
	if (!r)
		return -EINVAL;

	drvdata->base = devm_ioremap(&pdev->dev, r->start, resource_size(r));
	if (!drvdata->base)
		return -ENOMEM;

	drvdata->phys_base = r->start;

	r = platform_get_resource_byname(pdev, IORESOURCE_MEM,
					"smmu_local_base");
	if (r) {
		drvdata->smmu_local_base =
			devm_ioremap(&pdev->dev, r->start, resource_size(r));
		if (!drvdata->smmu_local_base)
			return -ENOMEM;
	}

	drvdata->glb_base = drvdata->base;

	if (of_device_is_compatible(pdev->dev.of_node, "qcom,msm-mmu-500"))
		drvdata->model = MMU_500;

	if (of_get_property(pdev->dev.of_node, "vdd-supply", NULL)) {

		drvdata->gdsc = devm_regulator_get(&pdev->dev, "vdd");
		if (IS_ERR(drvdata->gdsc))
			return PTR_ERR(drvdata->gdsc);

		drvdata->alt_gdsc = devm_regulator_get(&pdev->dev,
							"qcom,alt-vdd");
		if (IS_ERR(drvdata->alt_gdsc))
			drvdata->alt_gdsc = NULL;
	} else {
		pr_debug("Warning: No regulator specified for IOMMU\n");
	}

	drvdata->pclk = devm_clk_get(&pdev->dev, "iface_clk");
	if (IS_ERR(drvdata->pclk))
		return PTR_ERR(drvdata->pclk);

	drvdata->clk = devm_clk_get(&pdev->dev, "core_clk");
	if (IS_ERR(drvdata->clk))
		return PTR_ERR(drvdata->clk);

	needs_alt_core_clk = of_property_read_bool(pdev->dev.of_node,
						   "qcom,needs-alt-core-clk");
	if (needs_alt_core_clk) {
		drvdata->aclk = devm_clk_get(&pdev->dev, "alt_core_clk");
		if (IS_ERR(drvdata->aclk))
			return PTR_ERR(drvdata->aclk);
	}

	needs_alt_iface_clk = of_property_read_bool(pdev->dev.of_node,
						   "qcom,needs-alt-iface-clk");
	if (needs_alt_iface_clk) {
		drvdata->aiclk = devm_clk_get(&pdev->dev, "alt_iface_clk");
		if (IS_ERR(drvdata->aiclk))
			return PTR_ERR(drvdata->aiclk);
	}

	if (!of_property_read_u32(pdev->dev.of_node,
				"qcom,cb-base-offset",
				&temp))
		drvdata->cb_base = drvdata->base + temp;
	else
		drvdata->cb_base = drvdata->base + 0x8000;

	if (clk_get_rate(drvdata->clk) == 0) {
		ret = clk_round_rate(drvdata->clk, 1000);
		clk_set_rate(drvdata->clk, ret);
	}

	if (drvdata->aclk && clk_get_rate(drvdata->aclk) == 0) {
		ret = clk_round_rate(drvdata->aclk, 1000);
		clk_set_rate(drvdata->aclk, ret);
	}

	if (drvdata->aiclk && clk_get_rate(drvdata->aiclk) == 0) {
		ret = clk_round_rate(drvdata->aiclk, 1000);
		clk_set_rate(drvdata->aiclk, ret);
	}

	ret = msm_iommu_parse_dt(pdev, drvdata);
	if (ret)
		return ret;

	dev_info(&pdev->dev,
		"device %s (model: %d) mapped at %p, with %d ctx banks\n",
		drvdata->name, drvdata->model, drvdata->base, drvdata->ncb);

	platform_set_drvdata(pdev, drvdata);

	pmon_info = msm_iommu_pm_alloc(&pdev->dev);
	if (pmon_info != NULL) {
		ret = msm_iommu_pmon_parse_dt(pdev, pmon_info);
		if (ret) {
			msm_iommu_pm_free(&pdev->dev);
			pr_info("%s: pmon not available.\n", drvdata->name);
		} else {
			pmon_info->iommu.base = drvdata->base;
			pmon_info->iommu.ops = msm_get_iommu_access_ops();
			pmon_info->iommu.hw_ops = iommu_pm_get_hw_ops_v1();
			pmon_info->iommu.iommu_name = drvdata->name;
			ret = msm_iommu_pm_iommu_register(pmon_info);
			if (ret) {
				pr_err("%s iommu register fail\n",
								drvdata->name);
				msm_iommu_pm_free(&pdev->dev);
			} else {
				pr_debug("%s iommu registered for pmon\n",
						pmon_info->iommu.iommu_name);
			}
		}
	}

	global_cfg_irq =
		platform_get_irq_byname(pdev, "global_cfg_NS_irq");
	if (global_cfg_irq > 0) {
		ret = devm_request_threaded_irq(&pdev->dev, global_cfg_irq,
				NULL,
				msm_iommu_global_fault_handler,
				IRQF_ONESHOT | IRQF_SHARED |
				IRQF_TRIGGER_RISING,
				"msm_iommu_global_cfg_irq", pdev);
		if (ret < 0)
			pr_err("Request Global CFG IRQ %d failed with ret=%d\n",
					global_cfg_irq, ret);
	}

	global_client_irq =
		platform_get_irq_byname(pdev, "global_client_NS_irq");
	if (global_client_irq > 0) {
		ret = devm_request_threaded_irq(&pdev->dev, global_client_irq,
				NULL,
				msm_iommu_global_fault_handler,
				IRQF_ONESHOT | IRQF_SHARED |
				IRQF_TRIGGER_RISING,
				"msm_iommu_global_client_irq", pdev);
		if (ret < 0)
			pr_err("Request Global Client IRQ %d failed with ret=%d\n",
					global_client_irq, ret);
	}

	return 0;
}

static int msm_iommu_remove(struct platform_device *pdev)
{
	struct msm_iommu_drvdata *drv = NULL;

	msm_iommu_pm_iommu_unregister(&pdev->dev);
	msm_iommu_pm_free(&pdev->dev);

	drv = platform_get_drvdata(pdev);
	if (drv) {
		__put_bus_vote_client(drv);
		msm_iommu_remove_drv(drv);
		platform_set_drvdata(pdev, NULL);
	}
	return 0;
}

static int msm_iommu_ctx_parse_dt(struct platform_device *pdev,
				struct msm_iommu_ctx_drvdata *ctx_drvdata)
{
	struct resource *r, rp;
	int irq = 0, ret = 0;
	struct msm_iommu_drvdata *drvdata;
	u32 nsid;
	unsigned long cb_offset;

	drvdata = dev_get_drvdata(pdev->dev.parent);

	get_secure_ctx(pdev->dev.of_node, drvdata, ctx_drvdata);

	if (ctx_drvdata->secure_context) {
		irq = platform_get_irq(pdev, 1);
		if (irq > 0) {
			ret = devm_request_threaded_irq(&pdev->dev, irq, NULL,
					msm_iommu_secure_fault_handler_v2,
					IRQF_ONESHOT | IRQF_SHARED,
					"msm_iommu_secure_irq", pdev);
			if (ret) {
				pr_err("Request IRQ %d failed with ret=%d\n",
					irq, ret);
				return ret;
			}
		}
	} else {
		irq = platform_get_irq(pdev, 0);
		if (irq > 0) {
			ret = devm_request_threaded_irq(&pdev->dev, irq, NULL,
					msm_iommu_fault_handler_v2,
					IRQF_ONESHOT | IRQF_SHARED,
					"msm_iommu_nonsecure_irq", pdev);
			if (ret) {
				pr_err("Request IRQ %d failed with ret=%d\n",
					irq, ret);
				goto out;
			}
		}
	}

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!r) {
		ret = -EINVAL;
		goto out;
	}

	ret = of_address_to_resource(pdev->dev.parent->of_node, 0, &rp);
	if (ret)
		goto out;

	/* Calculate the context bank number using the base addresses.
	 * Typically CB0 base address is 0x8000 pages away if the number
	 * of CBs are <=8. So, assume the offset 0x8000 until mentioned
	 * explicitely.
	 */
	cb_offset = drvdata->cb_base - drvdata->base;
	ctx_drvdata->num = ((r->start - rp.start - cb_offset)
					>> CTX_SHIFT);

	if (of_property_read_string(pdev->dev.of_node, "label",
					&ctx_drvdata->name))
		ctx_drvdata->name = dev_name(&pdev->dev);

	if (!of_get_property(pdev->dev.of_node, "qcom,iommu-ctx-sids", &nsid)) {
		ret = -EINVAL;
		goto out;
	}
	if (nsid >= sizeof(ctx_drvdata->sids)) {
		ret = -EINVAL;
		goto out;
	}

	if (of_property_read_u32_array(pdev->dev.of_node, "qcom,iommu-ctx-sids",
				       ctx_drvdata->sids,
				       nsid / sizeof(*ctx_drvdata->sids))) {
		ret = -EINVAL;
		goto out;
	}
	ctx_drvdata->nsid = nsid;

	ctx_drvdata->asid = -1;
out:
	return ret;
}

static int msm_iommu_ctx_probe(struct platform_device *pdev)
{
	struct msm_iommu_ctx_drvdata *ctx_drvdata = NULL;
	int ret;

	if (!pdev->dev.parent)
		return -EINVAL;

	ctx_drvdata = devm_kzalloc(&pdev->dev, sizeof(*ctx_drvdata),
					GFP_KERNEL);
	if (!ctx_drvdata)
		return -ENOMEM;

	ctx_drvdata->pdev = pdev;
	INIT_LIST_HEAD(&ctx_drvdata->attached_elm);

	ret = msm_iommu_ctx_parse_dt(pdev, ctx_drvdata);
	if (!ret) {
		platform_set_drvdata(pdev, ctx_drvdata);

		dev_info(&pdev->dev, "context %s using bank %d\n",
			 ctx_drvdata->name, ctx_drvdata->num);
	}

	return ret;
}

static int msm_iommu_ctx_remove(struct platform_device *pdev)
{
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static struct of_device_id msm_iommu_match_table[] = {
	{ .compatible = "qcom,msm-smmu-v1", },
	{ .compatible = "qcom,msm-smmu-v2", },
	{}
};

static struct platform_driver msm_iommu_driver = {
	.driver = {
		.name	= "msm_iommu",
		.of_match_table = msm_iommu_match_table,
	},
	.probe		= msm_iommu_probe,
	.remove		= msm_iommu_remove,
};

static struct of_device_id msm_iommu_ctx_match_table[] = {
	{ .compatible = "qcom,msm-smmu-v1-ctx", },
	{ .compatible = "qcom,msm-smmu-v2-ctx", },
	{}
};

static struct platform_driver msm_iommu_ctx_driver = {
	.driver = {
		.name	= "msm_iommu_ctx",
		.of_match_table = msm_iommu_ctx_match_table,
	},
	.probe		= msm_iommu_ctx_probe,
	.remove		= msm_iommu_ctx_remove,
};

static int __init msm_iommu_driver_init(void)
{
	int ret;

	msm_iommu_check_scm_call_avail();

	msm_set_iommu_access_ops(&iommu_access_ops_v1);
	msm_iommu_sec_set_access_ops(&iommu_access_ops_v1);

	ret = platform_driver_register(&msm_iommu_driver);
	if (ret != 0) {
		pr_err("Failed to register IOMMU driver\n");
		goto error;
	}

	ret = platform_driver_register(&msm_iommu_ctx_driver);
	if (ret != 0) {
		pr_err("Failed to register IOMMU context driver\n");
		goto error;
	}

error:
	return ret;
}

static void __exit msm_iommu_driver_exit(void)
{
	platform_driver_unregister(&msm_iommu_ctx_driver);
	platform_driver_unregister(&msm_iommu_driver);
}

subsys_initcall(msm_iommu_driver_init);
module_exit(msm_iommu_driver_exit);

MODULE_LICENSE("GPL v2");
