/* Copyright (c) 2012 Code Aurora Forum. All rights reserved.
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
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/iommu.h>
#include <linux/clk.h>
#include <linux/scatterlist.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regulator/consumer.h>
#include <asm/sizes.h>

#include <mach/iommu_hw-v2.h>
#include <mach/iommu.h>

#include "msm_iommu_pagetable.h"

/* bitmap of the page sizes currently supported */
#define MSM_IOMMU_PGSIZES	(SZ_4K | SZ_64K | SZ_1M | SZ_16M)

static DEFINE_MUTEX(msm_iommu_lock);

struct msm_priv {
	struct iommu_pt pt;
	struct list_head list_attached;
};

static int __enable_clocks(struct msm_iommu_drvdata *drvdata)
{
	int ret;

	ret = clk_prepare_enable(drvdata->pclk);
	if (ret)
		goto fail;

	if (drvdata->clk) {
		ret = clk_prepare_enable(drvdata->clk);
		if (ret)
			clk_disable_unprepare(drvdata->pclk);
	}
fail:
	return ret;
}

static void __disable_clocks(struct msm_iommu_drvdata *drvdata)
{
	if (drvdata->clk)
		clk_disable_unprepare(drvdata->clk);
	clk_disable_unprepare(drvdata->pclk);
}

static int __flush_iotlb_va(struct iommu_domain *domain, unsigned int va)
{
	struct msm_priv *priv = domain->priv;
	struct msm_iommu_drvdata *iommu_drvdata;
	struct msm_iommu_ctx_drvdata *ctx_drvdata;
	int ret = 0;
	int asid;

	list_for_each_entry(ctx_drvdata, &priv->list_attached, attached_elm) {
		BUG_ON(!ctx_drvdata->pdev || !ctx_drvdata->pdev->dev.parent);

		iommu_drvdata = dev_get_drvdata(ctx_drvdata->pdev->dev.parent);
		BUG_ON(!iommu_drvdata);


		ret = __enable_clocks(iommu_drvdata);
		if (ret)
			goto fail;

		asid = GET_CB_CONTEXTIDR_ASID(iommu_drvdata->base,
					   ctx_drvdata->num);

		SET_TLBIVA(iommu_drvdata->base, ctx_drvdata->num,
			   asid | (va & CB_TLBIVA_VA));
		mb();
		__disable_clocks(iommu_drvdata);
	}
fail:
	return ret;
}

static int __flush_iotlb(struct iommu_domain *domain)
{
	struct msm_priv *priv = domain->priv;
	struct msm_iommu_drvdata *iommu_drvdata;
	struct msm_iommu_ctx_drvdata *ctx_drvdata;
	int ret = 0;
	int asid;

	list_for_each_entry(ctx_drvdata, &priv->list_attached, attached_elm) {
		BUG_ON(!ctx_drvdata->pdev || !ctx_drvdata->pdev->dev.parent);

		iommu_drvdata = dev_get_drvdata(ctx_drvdata->pdev->dev.parent);
		BUG_ON(!iommu_drvdata);

		ret = __enable_clocks(iommu_drvdata);
		if (ret)
			goto fail;

		asid = GET_CB_CONTEXTIDR_ASID(iommu_drvdata->base,
					   ctx_drvdata->num);

		SET_TLBIASID(iommu_drvdata->base, ctx_drvdata->num, asid);
		mb();
		__disable_clocks(iommu_drvdata);
	}

fail:
	return ret;
}

static void __reset_iommu(void __iomem *base)
{
	int i;

	SET_ACR(base, 0);
	SET_NSACR(base, 0);
	SET_CR2(base, 0);
	SET_NSCR2(base, 0);
	SET_GFAR(base, 0);
	SET_GFSRRESTORE(base, 0);
	SET_TLBIALLNSNH(base, 0);
	SET_PMCR(base, 0);
	SET_SCR1(base, 0);
	SET_SSDR_N(base, 0, 0);

	for (i = 0; i < MAX_NUM_SMR; i++)
		SET_SMR_VALID(base, i, 0);

	mb();
}

static void __program_iommu(void __iomem *base)
{
	__reset_iommu(base);

	SET_CR0_SMCFCFG(base, 1);
	SET_CR0_USFCFG(base, 1);
	SET_CR0_STALLD(base, 1);
	SET_CR0_GCFGFIE(base, 1);
	SET_CR0_GCFGFRE(base, 1);
	SET_CR0_GFIE(base, 1);
	SET_CR0_GFRE(base, 1);
	SET_CR0_CLIENTPD(base, 0);
	mb();	/* Make sure writes complete before returning */
}

static void __reset_context(void __iomem *base, int ctx)
{
	SET_ACTLR(base, ctx, 0);
	SET_FAR(base, ctx, 0);
	SET_FSRRESTORE(base, ctx, 0);
	SET_NMRR(base, ctx, 0);
	SET_PAR(base, ctx, 0);
	SET_PRRR(base, ctx, 0);
	SET_SCTLR(base, ctx, 0);
	SET_TLBIALL(base, ctx, 0);
	SET_TTBCR(base, ctx, 0);
	SET_TTBR0(base, ctx, 0);
	SET_TTBR1(base, ctx, 0);
	mb();
}

static void __program_context(void __iomem *base, int ctx, int ncb,
				phys_addr_t pgtable, int redirect,
				u32 *sids, int len)
{
	unsigned int prrr, nmrr;
	unsigned int pn;
	int i, j, found, num = 0;

	__reset_context(base, ctx);

	pn = pgtable >> CB_TTBR0_ADDR_SHIFT;
	SET_TTBCR(base, ctx, 0);
	SET_CB_TTBR0_ADDR(base, ctx, pn);

	/* Enable context fault interrupt */
	SET_CB_SCTLR_CFIE(base, ctx, 1);

	/* Redirect all cacheable requests to L2 slave port. */
	SET_CB_ACTLR_BPRCISH(base, ctx, 1);
	SET_CB_ACTLR_BPRCOSH(base, ctx, 1);
	SET_CB_ACTLR_BPRCNSH(base, ctx, 1);

	/* Turn on TEX Remap */
	SET_CB_SCTLR_TRE(base, ctx, 1);

	/* Enable private ASID namespace */
	SET_CB_SCTLR_ASIDPNE(base, ctx, 1);

	/* Set TEX remap attributes */
	RCP15_PRRR(prrr);
	RCP15_NMRR(nmrr);
	SET_PRRR(base, ctx, prrr);
	SET_NMRR(base, ctx, nmrr);

	/* Configure page tables as inner-cacheable and shareable to reduce
	 * the TLB miss penalty.
	 */
	if (redirect) {
		SET_CB_TTBR0_S(base, ctx, 1);
		SET_CB_TTBR0_NOS(base, ctx, 1);
		SET_CB_TTBR0_IRGN1(base, ctx, 0); /* WB, WA */
		SET_CB_TTBR0_IRGN0(base, ctx, 1);
		SET_CB_TTBR0_RGN(base, ctx, 1);   /* WB, WA */
	}

	/* Program the M2V tables for this context */
	for (i = 0; i < len / sizeof(*sids); i++) {
		for (; num < MAX_NUM_SMR; num++)
			if (GET_SMR_VALID(base, num) == 0)
				break;
		BUG_ON(num >= MAX_NUM_SMR);

		SET_SMR_VALID(base, num, 1);
		SET_SMR_MASK(base, num, 0);
		SET_SMR_ID(base, num, sids[i]);

		/* Set VMID = 0 */
		SET_S2CR_N(base, num, 0);
		SET_S2CR_CBNDX(base, num, ctx);
		/* Set security bit override to be Non-secure */
		SET_S2CR_NSCFG(base, sids[i], 3);

		SET_CBAR_N(base, ctx, 0);
		/* Stage 1 Context with Stage 2 bypass */
		SET_CBAR_TYPE(base, ctx, 1);
		/* Route page faults to the non-secure interrupt */
		SET_CBAR_IRPTNDX(base, ctx, 1);
	}

       /* Find if this page table is used elsewhere, and re-use ASID */
	found = 0;
	for (i = 0; i < ncb; i++)
		if ((GET_CB_TTBR0_ADDR(base, i) == pn) && (i != ctx)) {
			SET_CB_CONTEXTIDR_ASID(base, ctx, \
					GET_CB_CONTEXTIDR_ASID(base, i));
			found = 1;
			break;
		}

	/* If page table is new, find an unused ASID */
	if (!found) {
		for (i = 0; i < ncb; i++) {
			found = 0;
			for (j = 0; j < ncb; j++) {
				if (GET_CB_CONTEXTIDR_ASID(base, j) == i &&
				    j != ctx)
					found = 1;
			}

			if (!found) {
				SET_CB_CONTEXTIDR_ASID(base, ctx, i);
				break;
			}
		}
		BUG_ON(found);
	}

	/* Enable the MMU */
	SET_CB_SCTLR_M(base, ctx, 1);
	mb();
}

static int msm_iommu_domain_init(struct iommu_domain *domain, int flags)
{
	struct msm_priv *priv;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		goto fail_nomem;

#ifdef CONFIG_IOMMU_PGTABLES_L2
	priv->pt.redirect = flags & MSM_IOMMU_DOMAIN_PT_CACHEABLE;
#endif

	INIT_LIST_HEAD(&priv->list_attached);
	if (msm_iommu_pagetable_alloc(&priv->pt))
		goto fail_nomem;

	domain->priv = priv;
	return 0;

fail_nomem:
	kfree(priv);
	return -ENOMEM;
}

static void msm_iommu_domain_destroy(struct iommu_domain *domain)
{
	struct msm_priv *priv;

	mutex_lock(&msm_iommu_lock);
	priv = domain->priv;
	domain->priv = NULL;

	if (priv)
		msm_iommu_pagetable_free(&priv->pt);

	kfree(priv);
	mutex_unlock(&msm_iommu_lock);
}

static int msm_iommu_ctx_attached(struct device *dev)
{
	struct platform_device *pdev;
	struct device_node *child;
	struct msm_iommu_ctx_drvdata *ctx;

	for_each_child_of_node(dev->of_node, child) {
		pdev = of_find_device_by_node(child);

		ctx = dev_get_drvdata(&pdev->dev);
		if (ctx->attached_domain) {
			of_node_put(child);
			return 1;
		}
	}

	return 0;
}

static int msm_iommu_attach_dev(struct iommu_domain *domain, struct device *dev)
{
	struct msm_priv *priv;
	struct msm_iommu_drvdata *iommu_drvdata;
	struct msm_iommu_ctx_drvdata *ctx_drvdata;
	struct msm_iommu_ctx_drvdata *tmp_drvdata;
	u32 sids[MAX_NUM_SMR];
	int len = 0, ret;

	mutex_lock(&msm_iommu_lock);

	priv = domain->priv;
	if (!priv || !dev) {
		ret = -EINVAL;
		goto fail;
	}

	iommu_drvdata = dev_get_drvdata(dev->parent);
	ctx_drvdata = dev_get_drvdata(dev);
	if (!iommu_drvdata || !ctx_drvdata) {
		ret = -EINVAL;
		goto fail;
	}

	if (!list_empty(&ctx_drvdata->attached_elm)) {
		ret = -EBUSY;
		goto fail;
	}

	list_for_each_entry(tmp_drvdata, &priv->list_attached, attached_elm)
		if (tmp_drvdata == ctx_drvdata) {
			ret = -EBUSY;
			goto fail;
		}

	of_get_property(dev->of_node, "qcom,iommu-ctx-sids", &len);
	BUG_ON(len >= sizeof(sids));
	if (of_property_read_u32_array(dev->of_node, "qcom,iommu-ctx-sids",
					sids, len / sizeof(*sids))) {
		ret = -EINVAL;
		goto fail;
	}

	ret = regulator_enable(iommu_drvdata->gdsc);
	if (ret)
		goto fail;

	ret = __enable_clocks(iommu_drvdata);
	if (ret) {
		regulator_disable(iommu_drvdata->gdsc);
		goto fail;
	}

	if (!msm_iommu_ctx_attached(dev->parent))
		__program_iommu(iommu_drvdata->base);

	__program_context(iommu_drvdata->base, ctx_drvdata->num,
		iommu_drvdata->ncb, __pa(priv->pt.fl_table),
		priv->pt.redirect, sids, len);
	__disable_clocks(iommu_drvdata);

	list_add(&(ctx_drvdata->attached_elm), &priv->list_attached);
	ctx_drvdata->attached_domain = domain;

fail:
	mutex_unlock(&msm_iommu_lock);
	return ret;
}

static void msm_iommu_detach_dev(struct iommu_domain *domain,
				 struct device *dev)
{
	struct msm_priv *priv;
	struct msm_iommu_drvdata *iommu_drvdata;
	struct msm_iommu_ctx_drvdata *ctx_drvdata;
	int ret;

	mutex_lock(&msm_iommu_lock);
	priv = domain->priv;
	if (!priv || !dev)
		goto fail;

	iommu_drvdata = dev_get_drvdata(dev->parent);
	ctx_drvdata = dev_get_drvdata(dev);
	if (!iommu_drvdata || !ctx_drvdata || !ctx_drvdata->attached_domain)
		goto fail;

	ret = __enable_clocks(iommu_drvdata);
	if (ret)
		goto fail;

	SET_TLBIASID(iommu_drvdata->base, ctx_drvdata->num,
		GET_CB_CONTEXTIDR_ASID(iommu_drvdata->base, ctx_drvdata->num));

	__reset_context(iommu_drvdata->base, ctx_drvdata->num);
	__disable_clocks(iommu_drvdata);

	regulator_disable(iommu_drvdata->gdsc);

	list_del_init(&ctx_drvdata->attached_elm);
	ctx_drvdata->attached_domain = NULL;

fail:
	mutex_unlock(&msm_iommu_lock);
}

static int msm_iommu_map(struct iommu_domain *domain, unsigned long va,
			 phys_addr_t pa, size_t len, int prot)
{
	struct msm_priv *priv;
	int ret = 0;

	mutex_lock(&msm_iommu_lock);

	priv = domain->priv;
	if (!priv) {
		ret = -EINVAL;
		goto fail;
	}

	ret = msm_iommu_pagetable_map(&priv->pt, va, pa, len, prot);
	if (ret)
		goto fail;

	ret = __flush_iotlb_va(domain, va);
fail:
	mutex_unlock(&msm_iommu_lock);
	return ret;
}

static size_t msm_iommu_unmap(struct iommu_domain *domain, unsigned long va,
			    size_t len)
{
	struct msm_priv *priv;
	int ret = -ENODEV;

	mutex_lock(&msm_iommu_lock);

	priv = domain->priv;
	if (!priv)
		goto fail;

	ret = msm_iommu_pagetable_unmap(&priv->pt, va, len);
	if (ret < 0)
		goto fail;

	ret = __flush_iotlb_va(domain, va);
fail:
	mutex_unlock(&msm_iommu_lock);

	/* the IOMMU API requires us to return how many bytes were unmapped */
	len = ret ? 0 : len;
	return len;
}

static int msm_iommu_map_range(struct iommu_domain *domain, unsigned int va,
			       struct scatterlist *sg, unsigned int len,
			       int prot)
{
	int ret;
	struct msm_priv *priv;

	mutex_lock(&msm_iommu_lock);

	priv = domain->priv;
	if (!priv) {
		ret = -EINVAL;
		goto fail;
	}

	ret = msm_iommu_pagetable_map_range(&priv->pt, va, sg, len, prot);
	if (ret)
		goto fail;

	__flush_iotlb(domain);
fail:
	mutex_unlock(&msm_iommu_lock);
	return ret;
}


static int msm_iommu_unmap_range(struct iommu_domain *domain, unsigned int va,
				 unsigned int len)
{
	struct msm_priv *priv;

	mutex_lock(&msm_iommu_lock);

	priv = domain->priv;
	msm_iommu_pagetable_unmap_range(&priv->pt, va, len);

	__flush_iotlb(domain);
	mutex_unlock(&msm_iommu_lock);
	return 0;
}

static phys_addr_t msm_iommu_iova_to_phys(struct iommu_domain *domain,
					  unsigned long va)
{
	struct msm_priv *priv;
	struct msm_iommu_drvdata *iommu_drvdata;
	struct msm_iommu_ctx_drvdata *ctx_drvdata;
	unsigned int par;
	void __iomem *base;
	phys_addr_t ret = 0;
	int ctx;

	mutex_lock(&msm_iommu_lock);

	priv = domain->priv;
	if (list_empty(&priv->list_attached))
		goto fail;

	ctx_drvdata = list_entry(priv->list_attached.next,
				 struct msm_iommu_ctx_drvdata, attached_elm);
	iommu_drvdata = dev_get_drvdata(ctx_drvdata->pdev->dev.parent);

	base = iommu_drvdata->base;
	ctx = ctx_drvdata->num;

	ret = __enable_clocks(iommu_drvdata);
	if (ret) {
		ret = 0;	/* 0 indicates translation failed */
		goto fail;
	}

	SET_ATS1PR(base, ctx, va & CB_ATS1PR_ADDR);
	mb();
	while (GET_CB_ATSR_ACTIVE(base, ctx))
		cpu_relax();

	par = GET_PAR(base, ctx);
	__disable_clocks(iommu_drvdata);

	if (par & CB_PAR_F) {
		ret = 0;
	} else {
		/* We are dealing with a supersection */
		if (ret & CB_PAR_SS)
			ret = (par & 0xFF000000) | (va & 0x00FFFFFF);
		else /* Upper 20 bits from PAR, lower 12 from VA */
			ret = (par & 0xFFFFF000) | (va & 0x00000FFF);
	}

fail:
	mutex_unlock(&msm_iommu_lock);
	return ret;
}

static int msm_iommu_domain_has_cap(struct iommu_domain *domain,
				    unsigned long cap)
{
	return 0;
}

static void print_ctx_regs(void __iomem *base, int ctx, unsigned int fsr)
{
	pr_err("FAR    = %08x    PAR    = %08x\n",
		 GET_FAR(base, ctx), GET_PAR(base, ctx));
	pr_err("FSR    = %08x [%s%s%s%s%s%s%s%s%s]\n", fsr,
			(fsr & 0x02) ? "TF " : "",
			(fsr & 0x04) ? "AFF " : "",
			(fsr & 0x08) ? "PF " : "",
			(fsr & 0x10) ? "EF " : "",
			(fsr & 0x20) ? "TLBMCF " : "",
			(fsr & 0x40) ? "TLBLKF " : "",
			(fsr & 0x80) ? "MHF " : "",
			(fsr & 0x40000000) ? "SS " : "",
			(fsr & 0x80000000) ? "MULTI " : "");

	pr_err("FSYNR0 = %08x    FSYNR1 = %08x\n",
		 GET_FSYNR0(base, ctx), GET_FSYNR1(base, ctx));
	pr_err("TTBR0  = %08x    TTBR1  = %08x\n",
		 GET_TTBR0(base, ctx), GET_TTBR1(base, ctx));
	pr_err("SCTLR  = %08x    ACTLR  = %08x\n",
		 GET_SCTLR(base, ctx), GET_ACTLR(base, ctx));
	pr_err("PRRR   = %08x    NMRR   = %08x\n",
		 GET_PRRR(base, ctx), GET_NMRR(base, ctx));
}

irqreturn_t msm_iommu_fault_handler_v2(int irq, void *dev_id)
{
	struct platform_device *pdev = dev_id;
	struct msm_iommu_drvdata *drvdata;
	struct msm_iommu_ctx_drvdata *ctx_drvdata;
	unsigned int fsr;
	int ret;

	mutex_lock(&msm_iommu_lock);

	BUG_ON(!pdev);

	drvdata = dev_get_drvdata(pdev->dev.parent);
	BUG_ON(!drvdata);

	ctx_drvdata = dev_get_drvdata(&pdev->dev);
	BUG_ON(!ctx_drvdata);

	ret = __enable_clocks(drvdata);
	if (ret) {
		ret = IRQ_NONE;
		goto fail;
	}

	fsr = GET_FSR(drvdata->base, ctx_drvdata->num);
	if (fsr) {
		if (!ctx_drvdata->attached_domain) {
			pr_err("Bad domain in interrupt handler\n");
			ret = -ENOSYS;
		} else
			ret = report_iommu_fault(ctx_drvdata->attached_domain,
				&ctx_drvdata->pdev->dev,
				GET_FAR(drvdata->base, ctx_drvdata->num), 0);

		if (ret == -ENOSYS) {
			pr_err("Unexpected IOMMU page fault!\n");
			pr_err("name = %s\n", drvdata->name);
			pr_err("context = %s (%d)\n", ctx_drvdata->name,
							ctx_drvdata->num);
			pr_err("Interesting registers:\n");
			print_ctx_regs(drvdata->base, ctx_drvdata->num, fsr);
		}

		SET_FSR(drvdata->base, ctx_drvdata->num, fsr);
		ret = IRQ_HANDLED;
	} else
		ret = IRQ_NONE;

	__disable_clocks(drvdata);
fail:
	mutex_unlock(&msm_iommu_lock);
	return ret;
}

static phys_addr_t msm_iommu_get_pt_base_addr(struct iommu_domain *domain)
{
	struct msm_priv *priv = domain->priv;
	return __pa(priv->pt.fl_table);
}

static struct iommu_ops msm_iommu_ops = {
	.domain_init = msm_iommu_domain_init,
	.domain_destroy = msm_iommu_domain_destroy,
	.attach_dev = msm_iommu_attach_dev,
	.detach_dev = msm_iommu_detach_dev,
	.map = msm_iommu_map,
	.unmap = msm_iommu_unmap,
	.map_range = msm_iommu_map_range,
	.unmap_range = msm_iommu_unmap_range,
	.iova_to_phys = msm_iommu_iova_to_phys,
	.domain_has_cap = msm_iommu_domain_has_cap,
	.get_pt_base_addr = msm_iommu_get_pt_base_addr,
	.pgsize_bitmap = MSM_IOMMU_PGSIZES,
};

static int __init msm_iommu_init(void)
{
	msm_iommu_pagetable_init();
	bus_set_iommu(&platform_bus_type, &msm_iommu_ops);
	return 0;
}

subsys_initcall(msm_iommu_init);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MSM SMMU v2 Driver");
