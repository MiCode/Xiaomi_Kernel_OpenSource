/* Copyright (c) 2010-2013, The Linux Foundation. All rights reserved.
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
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/iommu.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>

#include <mach/iommu_perfmon.h>
#include <mach/iommu_hw-v0.h>
#include <mach/iommu.h>
#include <mach/msm_bus.h>

static struct of_device_id msm_iommu_v0_ctx_match_table[];
static struct iommu_access_ops *msm_iommu_access_ops;

static void msm_iommu_reset(void __iomem *base, void __iomem *glb_base, int ncb)
{
	int ctx;

	SET_RPUE(glb_base, 0);
	SET_RPUEIE(glb_base, 0);
	SET_ESRRESTORE(glb_base, 0);
	SET_TBE(glb_base, 0);
	SET_CR(glb_base, 0);
	SET_SPDMBE(glb_base, 0);
	SET_TESTBUSCR(glb_base, 0);
	SET_TLBRSW(glb_base, 0);
	SET_GLOBAL_TLBIALL(glb_base, 0);
	SET_RPU_ACR(glb_base, 0);
	SET_TLBLKCRWE(glb_base, 1);

	for (ctx = 0; ctx < ncb; ctx++) {
		SET_BPRCOSH(glb_base, ctx, 0);
		SET_BPRCISH(glb_base, ctx, 0);
		SET_BPRCNSH(glb_base, ctx, 0);
		SET_BPSHCFG(glb_base, ctx, 0);
		SET_BPMTCFG(glb_base, ctx, 0);
		SET_ACTLR(base, ctx, 0);
		SET_SCTLR(base, ctx, 0);
		SET_FSRRESTORE(base, ctx, 0);
		SET_TTBR0(base, ctx, 0);
		SET_TTBR1(base, ctx, 0);
		SET_TTBCR(base, ctx, 0);
		SET_BFBCR(base, ctx, 0);
		SET_PAR(base, ctx, 0);
		SET_FAR(base, ctx, 0);
		SET_TLBFLPTER(base, ctx, 0);
		SET_TLBSLPTER(base, ctx, 0);
		SET_TLBLKCR(base, ctx, 0);
		SET_CTX_TLBIALL(base, ctx, 0);
		SET_TLBIVA(base, ctx, 0);
		SET_PRRR(base, ctx, 0);
		SET_NMRR(base, ctx, 0);
		SET_CONTEXTIDR(base, ctx, 0);
	}
	mb();
}

static int __get_clocks(struct platform_device *pdev,
			struct msm_iommu_drvdata *drvdata,
			int needs_alt_core_clk)
{
	int ret = 0;

	drvdata->pclk = devm_clk_get(&pdev->dev, "iface_clk");
	if (IS_ERR(drvdata->pclk)) {
		ret = PTR_ERR(drvdata->pclk);
		drvdata->pclk = NULL;
		if (ret != -EPROBE_DEFER) {
			pr_err("Unable to get %s clock for %s IOMMU device\n",
				dev_name(&pdev->dev), drvdata->name);
		}
		goto fail;
	}

	drvdata->clk = devm_clk_get(&pdev->dev, "core_clk");

	if (!IS_ERR(drvdata->clk)) {
		if (clk_get_rate(drvdata->clk) == 0) {
			ret = clk_round_rate(drvdata->clk, 1000);
			clk_set_rate(drvdata->clk, ret);
		}
	} else {
		drvdata->clk = NULL;
	}

	if (needs_alt_core_clk) {
		drvdata->aclk = devm_clk_get(&pdev->dev, "alt_core_clk");
		if (IS_ERR(drvdata->aclk)) {
			ret = PTR_ERR(drvdata->aclk);
			goto fail;
		}
	}

	if (drvdata->aclk && clk_get_rate(drvdata->aclk) == 0) {
		ret = clk_round_rate(drvdata->aclk, 1000);
		clk_set_rate(drvdata->aclk, ret);
	}

	return 0;
fail:
	return ret;
}

#ifdef CONFIG_OF_DEVICE

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

static int msm_iommu_parse_dt(struct platform_device *pdev,
				struct msm_iommu_drvdata *drvdata)
{
	struct device_node *child;
	struct resource *r;
	u32 glb_offset = 0;
	int ret = 0;
	int needs_alt_core_clk;

	ret = __get_bus_vote_client(pdev, drvdata);

	if (ret)
		goto fail;

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!r) {
		pr_err("%s: Missing property reg\n", __func__);
		ret = -EINVAL;
		goto fail;
	}
	drvdata->base = devm_ioremap(&pdev->dev, r->start, resource_size(r));
	if (!drvdata->base) {
		pr_err("%s: Unable to ioremap %pr\n", __func__, r);
		ret = -ENOMEM;
		goto fail;
	}
	drvdata->glb_base = drvdata->base;

	if (!of_property_read_u32(pdev->dev.of_node, "qcom,glb-offset",
			&glb_offset)) {
		drvdata->glb_base += glb_offset;
	} else {
		pr_err("%s: Missing property qcom,glb-offset\n", __func__);
		ret = -EINVAL;
		goto fail;
	}

	for_each_child_of_node(pdev->dev.of_node, child)
		drvdata->ncb++;

	ret = of_property_read_string(pdev->dev.of_node, "label",
			&drvdata->name);
	if (ret) {
		pr_err("%s: Missing property label\n", __func__);
		ret = -EINVAL;
		goto fail;
	}

	needs_alt_core_clk = of_property_read_bool(pdev->dev.of_node,
						   "qcom,needs-alt-core-clk");

	ret = __get_clocks(pdev, drvdata, needs_alt_core_clk);

	if (ret)
		goto fail;

	drvdata->sec_id = -1;
	drvdata->ttbr_split = 0;

	ret = of_platform_populate(pdev->dev.of_node,
				   msm_iommu_v0_ctx_match_table,
				   NULL, &pdev->dev);
	if (ret) {
		pr_err("Failed to create iommu context device\n");
		goto fail;
	}

	return ret;

fail:
	__put_bus_vote_client(drvdata);
	return ret;
}

#else
static int msm_iommu_parse_dt(struct platform_device *pdev,
				struct msm_iommu_drvdata *drvdata)
{
	return 0;
}

static void __put_bus_vote_client(struct msm_iommu_drvdata *drvdata)
{

}

#endif

/*
 * Do a basic check of the IOMMU by performing an ATS operation
 * on context bank 0.
 */
static int iommu_sanity_check(struct msm_iommu_drvdata *drvdata)
{
	int par;
	int ret = 0;

	SET_M(drvdata->base, 0, 1);
	SET_PAR(drvdata->base, 0, 0);
	SET_V2PCFG(drvdata->base, 0, 1);
	SET_V2PPR(drvdata->base, 0, 0);
	mb();
	par = GET_PAR(drvdata->base, 0);
	SET_V2PCFG(drvdata->base, 0, 0);
	SET_M(drvdata->base, 0, 0);
	mb();

	if (!par) {
		pr_err("%s: Invalid PAR value detected\n", drvdata->name);
		ret = -ENODEV;
	}
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
	struct msm_iommu_dev *iommu_dev = pdev->dev.platform_data;
	int ret;

	drvdata = devm_kzalloc(&pdev->dev, sizeof(*drvdata), GFP_KERNEL);

	if (!drvdata) {
		ret = -ENOMEM;
		goto fail_mem;
	}

	if (pdev->dev.of_node) {
		ret = msm_iommu_parse_dt(pdev, drvdata);
		if (ret)
			goto fail;
	} else if (pdev->dev.platform_data) {
		struct resource *r, *r2;
		resource_size_t	len;

		ret = __get_clocks(pdev, drvdata, 0);

		if (ret)
			goto fail;

		r = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"physbase");

		if (!r) {
			ret = -ENODEV;
			goto fail;
		}

		len = resource_size(r);

		r2 = devm_request_mem_region(&pdev->dev, r->start,
					     len, r->name);
		if (!r2) {
			pr_err("Could not request memory region: %pr\n", r);
			ret = -EBUSY;
			goto fail;
		}

		drvdata->base = devm_ioremap(&pdev->dev, r2->start, len);

		if (!drvdata->base) {
			pr_err("Could not ioremap: %pr\n", r);
			ret = -EBUSY;
			goto fail;
		}
		/*
		 * Global register space offset for legacy IOMMUv1 hardware
		 * is always 0xFF000
		 */
		drvdata->glb_base = drvdata->base + 0xFF000;
		drvdata->name = iommu_dev->name;
		drvdata->dev = &pdev->dev;
		drvdata->ncb = iommu_dev->ncb;
		drvdata->ttbr_split = iommu_dev->ttbr_split;
	} else {
		ret = -ENODEV;
		goto fail;
	}

	drvdata->dev = &pdev->dev;

	msm_iommu_access_ops->iommu_clk_on(drvdata);

	msm_iommu_reset(drvdata->base, drvdata->glb_base, drvdata->ncb);

	ret = iommu_sanity_check(drvdata);
	if (ret)
		goto fail_clk;

	msm_iommu_access_ops->iommu_clk_off(drvdata);

	pr_info("device %s mapped at %p, with %d ctx banks\n",
		drvdata->name, drvdata->base, drvdata->ncb);

	msm_iommu_add_drv(drvdata);
	platform_set_drvdata(pdev, drvdata);

	pmon_info = msm_iommu_pm_alloc(&pdev->dev);
	if (pmon_info != NULL) {
		ret = msm_iommu_pmon_parse_dt(pdev, pmon_info);
		if (ret) {
			msm_iommu_pm_free(&pdev->dev);
			pr_info("%s: pmon not available.\n", drvdata->name);
		} else {
			pmon_info->iommu.base = drvdata->base;
			pmon_info->iommu.ops = msm_iommu_access_ops;
			pmon_info->iommu.hw_ops = iommu_pm_get_hw_ops_v0();
			pmon_info->iommu.iommu_name = drvdata->name;
			pmon_info->iommu.always_on = 1;
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

	return 0;

fail_clk:
	msm_iommu_access_ops->iommu_clk_off(drvdata);
fail:
	__put_bus_vote_client(drvdata);
fail_mem:
	return ret;
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
	int irq, ret;
	u32 nmid_array_size;
	u32 nmid;

	irq = platform_get_irq(pdev, 0);
	if (irq > 0) {
		ret = devm_request_threaded_irq(&pdev->dev, irq, NULL,
				msm_iommu_fault_handler,
				IRQF_ONESHOT | IRQF_SHARED,
				"msm_iommu_nonsecure_irq", ctx_drvdata);
		if (ret) {
			pr_err("Request IRQ %d failed with ret=%d\n", irq, ret);
			goto out;
		}
	}

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!r) {
		pr_err("Could not find reg property for context bank\n");
		ret = -EINVAL;
		goto out;
	}

	ret = of_address_to_resource(pdev->dev.parent->of_node, 0, &rp);
	if (ret) {
		pr_err("of_address_to_resource failed\n");
		ret = -EINVAL;
		goto out;
	}

	/* Calculate the context bank number using the base addresses. CB0
	 * starts at the base address.
	 */
	ctx_drvdata->num = ((r->start - rp.start) >> CTX_SHIFT);

	if (of_property_read_string(pdev->dev.of_node, "label",
					&ctx_drvdata->name)) {
		pr_err("Could not find label property\n");
		ret = -EINVAL;
		goto out;
	}

	if (!of_get_property(pdev->dev.of_node, "qcom,iommu-ctx-mids",
			     &nmid_array_size)) {
		pr_err("Could not find iommu-ctx-mids property\n");
		ret = -EINVAL;
		goto out;
	}
	if (nmid_array_size >= sizeof(ctx_drvdata->sids)) {
		pr_err("Too many mids defined - array size: %u, mids size: %u\n",
			nmid_array_size, sizeof(ctx_drvdata->sids));
		ret = -EINVAL;
		goto out;
	}
	nmid = nmid_array_size / sizeof(*ctx_drvdata->sids);

	if (of_property_read_u32_array(pdev->dev.of_node, "qcom,iommu-ctx-mids",
				       ctx_drvdata->sids, nmid)) {
		pr_err("Could not find iommu-ctx-mids property\n");
		ret = -EINVAL;
		goto out;
	}
	ctx_drvdata->nsid = nmid;

out:
	return ret;
}

static void __program_m2v_tables(struct msm_iommu_drvdata *drvdata,
				struct msm_iommu_ctx_drvdata *ctx_drvdata)
{
	int i;

	/* Program the M2V tables for this context */
	for (i = 0; i < ctx_drvdata->nsid; i++) {
		int sid = ctx_drvdata->sids[i];
		int num = ctx_drvdata->num;

		SET_M2VCBR_N(drvdata->glb_base, sid, 0);
		SET_CBACR_N(drvdata->glb_base, num, 0);

		/* Route page faults to the non-secure interrupt */
		SET_IRPTNDX(drvdata->glb_base, num, 1);

		/* Set VMID = 0 */
		SET_VMID(drvdata->glb_base, sid, 0);

		/* Set the context number for that SID to this context */
		SET_CBNDX(drvdata->glb_base, sid, num);

		/* Set SID associated with this context bank to 0 */
		SET_CBVMID(drvdata->glb_base, num, 0);

		/* Set the ASID for TLB tagging for this context to 0 */
		SET_CONTEXTIDR_ASID(drvdata->base, num, 0);

		/* Set security bit override to be Non-secure */
		SET_NSCFG(drvdata->glb_base, sid, 3);
	}
	mb();
}

static int msm_iommu_ctx_probe(struct platform_device *pdev)
{
	struct msm_iommu_drvdata *drvdata;
	struct msm_iommu_ctx_drvdata *ctx_drvdata = NULL;
	int i, ret, irq;
	if (!pdev->dev.parent) {
		ret = -EINVAL;
		goto fail;
	}

	drvdata = dev_get_drvdata(pdev->dev.parent);

	if (!drvdata) {
		ret = -EPROBE_DEFER;
		goto fail;
	}

	ctx_drvdata = devm_kzalloc(&pdev->dev, sizeof(*ctx_drvdata),
					GFP_KERNEL);
	if (!ctx_drvdata) {
		ret = -ENOMEM;
		goto fail;
	}

	ctx_drvdata->pdev = pdev;
	INIT_LIST_HEAD(&ctx_drvdata->attached_elm);
	platform_set_drvdata(pdev, ctx_drvdata);
	ctx_drvdata->attach_count = 0;

	if (pdev->dev.of_node) {
		ret = msm_iommu_ctx_parse_dt(pdev, ctx_drvdata);
		if (ret) {
			platform_set_drvdata(pdev, NULL);
			goto fail;
		}
	} else if (pdev->dev.platform_data) {
		struct msm_iommu_ctx_dev *c = pdev->dev.platform_data;

		ctx_drvdata->num = c->num;
		ctx_drvdata->name = c->name;

		for (i = 0;  i < MAX_NUM_MIDS; ++i) {
			if (c->mids[i] == -1) {
				ctx_drvdata->nsid = i;
				break;
			}
			ctx_drvdata->sids[i] = c->mids[i];
		}
		irq = platform_get_irq_byname(
					to_platform_device(pdev->dev.parent),
					"nonsecure_irq");
		if (irq < 0) {
			ret = -ENODEV;
			goto fail;
		}

		ret = devm_request_threaded_irq(&pdev->dev, irq, NULL,
					msm_iommu_fault_handler,
					IRQF_ONESHOT | IRQF_SHARED,
					"msm_iommu_nonsecure_irq", ctx_drvdata);

		if (ret) {
			pr_err("request_threaded_irq %d failed: %d\n", irq,
								       ret);
			goto fail;
		}
	} else {
		ret = -ENODEV;
		goto fail;
	}

	msm_iommu_access_ops->iommu_clk_on(drvdata);
	__program_m2v_tables(drvdata, ctx_drvdata);
	msm_iommu_access_ops->iommu_clk_off(drvdata);

	dev_info(&pdev->dev, "context %s using bank %d\n", ctx_drvdata->name,
							   ctx_drvdata->num);
	return 0;
fail:
	return ret;
}

static int __devexit msm_iommu_ctx_remove(struct platform_device *pdev)
{
	platform_set_drvdata(pdev, NULL);
	return 0;
}


static struct of_device_id msm_iommu_match_table[] = {
	{ .compatible = "qcom,msm-smmu-v0", },
	{}
};

static struct platform_driver msm_iommu_driver = {
	.driver = {
		.name	= "msm_iommu-v0",
		.of_match_table = msm_iommu_match_table,
	},
	.probe		= msm_iommu_probe,
	.remove		= __devexit_p(msm_iommu_remove),
};

static struct of_device_id msm_iommu_v0_ctx_match_table[] = {
	{ .compatible = "qcom,msm-smmu-v0-ctx", },
	{}
};

static struct platform_driver msm_iommu_ctx_driver = {
	.driver = {
		.name	= "msm_iommu_ctx",
		.of_match_table = msm_iommu_v0_ctx_match_table,
	},
	.probe		= msm_iommu_ctx_probe,
	.remove		= __devexit_p(msm_iommu_ctx_remove),
};

static int __init msm_iommu_driver_init(void)
{
	int ret;

	if (msm_soc_version_supports_iommu_v0()) {
		msm_set_iommu_access_ops(&iommu_access_ops_v0);
		msm_iommu_access_ops = msm_get_iommu_access_ops();
	}
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
MODULE_AUTHOR("Stepan Moskovchenko <stepanm@codeaurora.org>");
