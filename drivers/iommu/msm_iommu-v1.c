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
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/msm-bus.h>
#include <linux/platform_device.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/iommu.h>
#include <linux/msm-bus.h>
#include <linux/clk.h>
#include <linux/scatterlist.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regulator/consumer.h>
#include <linux/qcom_iommu.h>
#include <asm/sizes.h>

#include "msm_iommu_hw-v1.h"
#include "msm_iommu_priv.h"
#include "msm_iommu_perfmon.h"
#include "msm_iommu_pagetable.h"

#ifdef CONFIG_IOMMU_LPAE
/* bitmap of the page sizes currently supported */
#define MSM_IOMMU_PGSIZES	(SZ_4K | SZ_64K | SZ_2M | SZ_32M | SZ_1G)
#else
/* bitmap of the page sizes currently supported */
#define MSM_IOMMU_PGSIZES	(SZ_4K | SZ_64K | SZ_1M | SZ_16M)
#endif

#define IOMMU_MSEC_STEP		10
#define IOMMU_MSEC_TIMEOUT	5000


static DEFINE_MUTEX(msm_iommu_lock);
struct dump_regs_tbl_entry dump_regs_tbl[MAX_DUMP_REGS];

static int __enable_regulators(struct msm_iommu_drvdata *drvdata)
{
	int ret = 0;
	if (drvdata->gdsc) {
		ret = regulator_enable(drvdata->gdsc);
		if (ret)
			goto fail;

		if (drvdata->alt_gdsc)
			ret = regulator_enable(drvdata->alt_gdsc);

		if (ret) {
			regulator_disable(drvdata->gdsc);
			goto fail;
		}

	}
	++drvdata->powered_on;
fail:
	return ret;
}

static void __disable_regulators(struct msm_iommu_drvdata *drvdata)
{
	if (drvdata->alt_gdsc)
		regulator_disable(drvdata->alt_gdsc);

	if (drvdata->gdsc)
		regulator_disable(drvdata->gdsc);

	--drvdata->powered_on;
}

static int apply_bus_vote(struct msm_iommu_drvdata *drvdata, unsigned int vote)
{
	int ret = 0;

	if (drvdata->bus_client) {
		ret = msm_bus_scale_client_update_request(drvdata->bus_client,
							  vote);
		if (ret)
			pr_err("%s: Failed to vote for bus: %d\n", __func__,
				vote);
	}
	return ret;
}

static int __enable_clocks(struct msm_iommu_drvdata *drvdata)
{
	int ret;

	ret = clk_prepare_enable(drvdata->pclk);
	if (ret)
		goto fail;

	ret = clk_prepare_enable(drvdata->clk);
	if (ret)
		goto fail1;

	if (drvdata->aclk) {
		ret = clk_prepare_enable(drvdata->aclk);
		if (ret)
			goto fail2;
	}

	if (drvdata->aiclk) {
		ret = clk_prepare_enable(drvdata->aiclk);
		if (ret)
			goto fail3;
	}

	if (drvdata->clk_reg_virt) {
		unsigned int value;

		value = readl_relaxed(drvdata->clk_reg_virt);
		value &= ~0x1;
		writel_relaxed(value, drvdata->clk_reg_virt);
		/* Ensure clock is on before continuing */
		mb();
	}
	return 0;

fail3:
	if (drvdata->aclk)
		clk_disable_unprepare(drvdata->aclk);
fail2:
	clk_disable_unprepare(drvdata->clk);
fail1:
	clk_disable_unprepare(drvdata->pclk);
fail:
	return ret;
}

static void __disable_clocks(struct msm_iommu_drvdata *drvdata)
{
	if (drvdata->aiclk)
		clk_disable_unprepare(drvdata->aiclk);
	if (drvdata->aclk)
		clk_disable_unprepare(drvdata->aclk);
	clk_disable_unprepare(drvdata->clk);
	clk_disable_unprepare(drvdata->pclk);
}

static void _iommu_lock_acquire(unsigned int need_extra_lock)
{
	mutex_lock(&msm_iommu_lock);
}

static void _iommu_lock_release(unsigned int need_extra_lock)
{
	mutex_unlock(&msm_iommu_lock);
}

struct iommu_access_ops iommu_access_ops_v1 = {
	.iommu_power_on = __enable_regulators,
	.iommu_power_off = __disable_regulators,
	.iommu_bus_vote = apply_bus_vote,
	.iommu_clk_on = __enable_clocks,
	.iommu_clk_off = __disable_clocks,
	.iommu_lock_acquire = _iommu_lock_acquire,
	.iommu_lock_release = _iommu_lock_release,
};

#ifdef CONFIG_MSM_IOMMU_VBIF_CHECK

#define VBIF_XIN_HALT_CTRL0 0x200
#define VBIF_XIN_HALT_CTRL1 0x204
#define VBIF_AXI_HALT_CTRL0 0x208
#define VBIF_AXI_HALT_CTRL1 0x20C

static void __halt_vbif_xin(void __iomem *vbif_base)
{
	pr_err("Halting VBIF_XIN\n");
	writel_relaxed(0xFFFFFFFF, vbif_base + VBIF_XIN_HALT_CTRL0);
}

static void __dump_vbif_state(void __iomem *base, void __iomem *vbif_base)
{
	unsigned int reg_val;

	reg_val = readl_relaxed(base + MICRO_MMU_CTRL);
	pr_err("Value of SMMU_IMPLDEF_MICRO_MMU_CTRL = 0x%x\n", reg_val);

	reg_val = readl_relaxed(vbif_base + VBIF_XIN_HALT_CTRL0);
	pr_err("Value of VBIF_XIN_HALT_CTRL0 = 0x%x\n", reg_val);
	reg_val = readl_relaxed(vbif_base + VBIF_XIN_HALT_CTRL1);
	pr_err("Value of VBIF_XIN_HALT_CTRL1 = 0x%x\n", reg_val);
	reg_val = readl_relaxed(vbif_base + VBIF_AXI_HALT_CTRL0);
	pr_err("Value of VBIF_AXI_HALT_CTRL0 = 0x%x\n", reg_val);
	reg_val = readl_relaxed(vbif_base + VBIF_AXI_HALT_CTRL1);
	pr_err("Value of VBIF_AXI_HALT_CTRL1 = 0x%x\n", reg_val);
}

static int __check_vbif_state(struct msm_iommu_drvdata const *drvdata)
{
	phys_addr_t addr = (phys_addr_t) (drvdata->phys_base
			   - (phys_addr_t) 0x4000);
	void __iomem *base = ioremap(addr, 0x1000);
	int ret = 0;

	if (base) {
		__dump_vbif_state(drvdata->base, base);
		__halt_vbif_xin(drvdata->base);
		__dump_vbif_state(drvdata->base, base);
		iounmap(base);
	} else {
		pr_err("%s: Unable to ioremap\n", __func__);
		ret = -ENOMEM;
	}
	return ret;
}

static void check_halt_state(struct msm_iommu_drvdata const *drvdata)
{
	int res;
	unsigned int val;
	void __iomem *base = drvdata->base;
	char const *name = drvdata->name;

	pr_err("Timed out waiting for IOMMU halt to complete for %s\n", name);
	res = __check_vbif_state(drvdata);
	if (res)
		BUG();

	pr_err("Checking if IOMMU halt completed for %s\n", name);

	res = readl_tight_poll_timeout(
		GLB_REG(MICRO_MMU_CTRL, base), val,
			(val & MMU_CTRL_IDLE) == MMU_CTRL_IDLE, 5000000);

	if (res) {
		pr_err("Timed out (again) waiting for IOMMU halt to complete for %s\n",
			name);
	} else {
		pr_err("IOMMU halt completed. VBIF FIFO most likely not getting drained by master\n");
	}
	BUG();
}

static void check_tlb_sync_state(struct msm_iommu_drvdata const *drvdata,
				int ctx)
{
	int res;
	unsigned int val;
	void __iomem *base = drvdata->base;
	char const *name = drvdata->name;

	pr_err("Timed out waiting for TLB SYNC to complete for %s\n", name);
	res = __check_vbif_state(drvdata);
	if (res)
		BUG();

	pr_err("Checking if TLB sync completed for %s\n", name);

	res = readl_tight_poll_timeout(CTX_REG(CB_TLBSTATUS, base, ctx), val,
				(val & CB_TLBSTATUS_SACTIVE) == 0, 5000000);
	if (res) {
		pr_err("Timed out (again) waiting for TLB SYNC to complete for %s\n",
			name);
	} else {
		pr_err("TLB Sync completed. VBIF FIFO most likely not getting drained by master\n");
	}
	BUG();
}

#else

/*
 * For targets without VBIF or for targets with the VBIF check disabled
 * we directly just crash to capture the issue
 */
static void check_halt_state(struct msm_iommu_drvdata const *drvdata)
{
	BUG();
}

static void check_tlb_sync_state(struct msm_iommu_drvdata const *drvdata,
				int ctx)
{
	BUG();
}

#endif

void iommu_halt(struct msm_iommu_drvdata const *iommu_drvdata)
{
	if (iommu_drvdata->halt_enabled) {
		unsigned int val;
		void __iomem *base = iommu_drvdata->base;
		int res;

		SET_MICRO_MMU_CTRL_HALT_REQ(base, 1);
		res = readl_tight_poll_timeout(
			GLB_REG(MICRO_MMU_CTRL, base), val,
			     (val & MMU_CTRL_IDLE) == MMU_CTRL_IDLE, 5000000);

		if (res)
			check_halt_state(iommu_drvdata);
		/* Ensure device is idle before continuing */
		mb();
	}
}

void iommu_resume(const struct msm_iommu_drvdata *iommu_drvdata)
{
	if (iommu_drvdata->halt_enabled) {
		/*
		 * Ensure transactions have completed before releasing
		 * the halt
		 */
		mb();
		SET_MICRO_MMU_CTRL_HALT_REQ(iommu_drvdata->base, 0);
		/*
		 * Ensure write is complete before continuing to ensure
		 * we don't turn off clocks while transaction is still
		 * pending.
		 */
		mb();
	}
}

static void __sync_tlb(struct msm_iommu_drvdata *iommu_drvdata, int ctx)
{
	unsigned int val;
	unsigned int res;
	void __iomem *base = iommu_drvdata->cb_base;

	SET_TLBSYNC(base, ctx, 0);
	/* No barrier needed due to read dependency */

	res = readl_tight_poll_timeout(CTX_REG(CB_TLBSTATUS, base, ctx), val,
				(val & CB_TLBSTATUS_SACTIVE) == 0, 5000000);
	if (res)
		check_tlb_sync_state(iommu_drvdata, ctx);
}

static int __flush_iotlb_va(struct iommu_domain *domain, unsigned int va)
{
	struct msm_iommu_priv *priv = domain->priv;
	struct msm_iommu_drvdata *iommu_drvdata;
	struct msm_iommu_ctx_drvdata *ctx_drvdata;
	int ret = 0;

	list_for_each_entry(ctx_drvdata, &priv->list_attached, attached_elm) {
		BUG_ON(!ctx_drvdata->pdev || !ctx_drvdata->pdev->dev.parent);

		iommu_drvdata = dev_get_drvdata(ctx_drvdata->pdev->dev.parent);
		BUG_ON(!iommu_drvdata);


		ret = __enable_clocks(iommu_drvdata);
		if (ret)
			goto fail;

		SET_TLBIVA(iommu_drvdata->cb_base, ctx_drvdata->num,
			   ctx_drvdata->asid | (va & CB_TLBIVA_VA));
		mb();
		__sync_tlb(iommu_drvdata, ctx_drvdata->num);
		__disable_clocks(iommu_drvdata);
	}
fail:
	return ret;
}

static int __flush_iotlb(struct iommu_domain *domain)
{
	struct msm_iommu_priv *priv = domain->priv;
	struct msm_iommu_drvdata *iommu_drvdata;
	struct msm_iommu_ctx_drvdata *ctx_drvdata;
	int ret = 0;

	list_for_each_entry(ctx_drvdata, &priv->list_attached, attached_elm) {
		BUG_ON(!ctx_drvdata->pdev || !ctx_drvdata->pdev->dev.parent);

		iommu_drvdata = dev_get_drvdata(ctx_drvdata->pdev->dev.parent);
		BUG_ON(!iommu_drvdata);

		ret = __enable_clocks(iommu_drvdata);
		if (ret)
			goto fail;

		SET_TLBIASID(iommu_drvdata->cb_base, ctx_drvdata->num,
			     ctx_drvdata->asid);
		mb();
		__sync_tlb(iommu_drvdata, ctx_drvdata->num);
		__disable_clocks(iommu_drvdata);
	}

fail:
	return ret;
}

/*
 * May only be called for non-secure iommus
 */
static void __reset_iommu(struct msm_iommu_drvdata *iommu_drvdata)
{
	int i, smt_size;
	void __iomem *base = iommu_drvdata->base;

	/* SMMU_ACR is an implementation defined register.
	 * Resetting is not required for some implementation.
	 */
	if (iommu_drvdata->model != MMU_500)
		SET_ACR(base, 0);
	SET_CR2(base, 0);
	SET_GFAR(base, 0);
	SET_GFSRRESTORE(base, 0);
	SET_TLBIALLNSNH(base, 0);
	smt_size = GET_IDR0_NUMSMRG(base);

	for (i = 0; i < smt_size; i++)
		SET_SMR_VALID(base, i, 0);

	mb();
}

static void __reset_iommu_secure(struct msm_iommu_drvdata *iommu_drvdata)
{
	void __iomem *base = iommu_drvdata->base;

	if (iommu_drvdata->model != MMU_500)
		SET_NSACR(base, 0);
	SET_NSCR2(base, 0);
	SET_NSGFAR(base, 0);
	SET_NSGFSRRESTORE(base, 0);
	mb();
}

static void __program_iommu_secure(struct msm_iommu_drvdata *iommu_drvdata)
{
	void __iomem *base = iommu_drvdata->base;

	if (iommu_drvdata->model == MMU_500) {
		SET_NSACR_SMTNMC_BPTLBEN(base, 1);
		SET_NSACR_MMUDIS_BPTLBEN(base, 1);
		SET_NSACR_S2CR_BPTLBEN(base, 1);
	}
	SET_NSCR0_SMCFCFG(base, 1);
	SET_NSCR0_USFCFG(base, 1);
	SET_NSCR0_STALLD(base, 1);
	SET_NSCR0_GCFGFIE(base, 1);
	SET_NSCR0_GCFGFRE(base, 1);
	SET_NSCR0_GFIE(base, 1);
	SET_NSCR0_GFRE(base, 1);
	SET_NSCR0_CLIENTPD(base, 0);
}

/*
 * May only be called for non-secure iommus
 */
static void __program_iommu(struct msm_iommu_drvdata *drvdata)
{
	__reset_iommu(drvdata);

	if (!msm_iommu_get_scm_call_avail())
		__reset_iommu_secure(drvdata);

	if (drvdata->model == MMU_500) {
		SET_ACR_SMTNMC_BPTLBEN(drvdata->base, 1);
		SET_ACR_MMUDIS_BPTLBEN(drvdata->base, 1);
		SET_ACR_S2CR_BPTLBEN(drvdata->base, 1);
	}
	SET_CR0_SMCFCFG(drvdata->base, 1);
	SET_CR0_USFCFG(drvdata->base, 1);
	SET_CR0_STALLD(drvdata->base, 1);
	SET_CR0_GCFGFIE(drvdata->base, 1);
	SET_CR0_GCFGFRE(drvdata->base, 1);
	SET_CR0_GFIE(drvdata->base, 1);
	SET_CR0_GFRE(drvdata->base, 1);
	SET_CR0_CLIENTPD(drvdata->base, 0);

	if (!msm_iommu_get_scm_call_avail())
		__program_iommu_secure(drvdata);

	if (drvdata->smmu_local_base)
		writel_relaxed(0xFFFFFFFF, drvdata->smmu_local_base +
							SMMU_INTR_SEL_NS);

	mb(); /* Make sure writes complete before returning */
}

void program_iommu_bfb_settings(void __iomem *base,
			const struct msm_iommu_bfb_settings *bfb_settings)
{
	unsigned int i;
	if (bfb_settings)
		for (i = 0; i < bfb_settings->length; i++)
			SET_GLOBAL_REG(base, bfb_settings->regs[i],
					     bfb_settings->data[i]);

	mb(); /* Make sure writes complete before returning */
}

static void __reset_context(struct msm_iommu_drvdata *iommu_drvdata, int ctx)
{
	void __iomem *base = iommu_drvdata->cb_base;

	/* Don't set ACTLR to zero because if context bank is in
	 * bypass mode (say after iommu_detach), still this ACTLR
	 * value matters for micro-TLB caching.
	 */
	if (iommu_drvdata->model != MMU_500)
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

static void __release_smg(void __iomem *base)
{
	int i, smt_size;
	smt_size = GET_IDR0_NUMSMRG(base);

	/* Invalidate all SMGs */
	for (i = 0; i < smt_size; i++)
		if (GET_SMR_VALID(base, i))
			SET_SMR_VALID(base, i, 0);
}

#ifdef CONFIG_IOMMU_LPAE
static void msm_iommu_set_ASID(void __iomem *base, unsigned int ctx_num,
			       unsigned int asid)
{
	SET_CB_TTBR0_ASID(base, ctx_num, asid);
}
#else
static void msm_iommu_set_ASID(void __iomem *base, unsigned int ctx_num,
			       unsigned int asid)
{
	SET_CB_CONTEXTIDR_ASID(base, ctx_num, asid);
}
#endif

static void msm_iommu_assign_ASID(const struct msm_iommu_drvdata *iommu_drvdata,
				  struct msm_iommu_ctx_drvdata *curr_ctx,
				  struct msm_iommu_priv *priv)
{
	unsigned int found = 0;
	void __iomem *cb_base = iommu_drvdata->cb_base;
	unsigned int i;
	unsigned int ncb = iommu_drvdata->ncb;
	struct msm_iommu_ctx_drvdata *tmp_drvdata;

	/* Find if this page table is used elsewhere, and re-use ASID */
	if (!list_empty(&priv->list_attached)) {
		tmp_drvdata = list_first_entry(&priv->list_attached,
				struct msm_iommu_ctx_drvdata, attached_elm);

		++iommu_drvdata->asid[tmp_drvdata->asid - 1];
		curr_ctx->asid = tmp_drvdata->asid;
		found = 1;
	}

	/* If page table is new, find an unused ASID */
	if (!found) {
		for (i = 0; i < ncb; ++i) {
			if (iommu_drvdata->asid[i] == 0) {
				++iommu_drvdata->asid[i];
				curr_ctx->asid = i + 1;
				found = 1;
				break;
			}
		}
		BUG_ON(!found);
	}

	msm_iommu_set_ASID(cb_base, curr_ctx->num, curr_ctx->asid);
}

#ifdef CONFIG_IOMMU_LPAE
static void msm_iommu_setup_ctx(void __iomem *base, unsigned int ctx)
{
	SET_CB_TTBCR_EAE(base, ctx, 1); /* Extended Address Enable (EAE) */
}

static void msm_iommu_setup_memory_remap(void __iomem *base, unsigned int ctx)
{
	SET_CB_MAIR0(base, ctx, msm_iommu_get_mair0());
	SET_CB_MAIR1(base, ctx, msm_iommu_get_mair1());
}

static void msm_iommu_setup_pg_l2_redirect(void __iomem *base, unsigned int ctx)
{
	/*
	 * Configure page tables as inner-cacheable and shareable to reduce
	 * the TLB miss penalty.
	 */
	SET_CB_TTBCR_SH0(base, ctx, 3); /* Inner shareable */
	SET_CB_TTBCR_ORGN0(base, ctx, 1); /* outer cachable*/
	SET_CB_TTBCR_IRGN0(base, ctx, 1); /* inner cachable*/
	SET_CB_TTBCR_T0SZ(base, ctx, 0); /* 0GB-4GB */


	SET_CB_TTBCR_SH1(base, ctx, 3); /* Inner shareable */
	SET_CB_TTBCR_ORGN1(base, ctx, 1); /* outer cachable*/
	SET_CB_TTBCR_IRGN1(base, ctx, 1); /* inner cachable*/
	SET_CB_TTBCR_T1SZ(base, ctx, 0); /* TTBR1 not used */
}

#else

static void msm_iommu_setup_ctx(void __iomem *base, unsigned int ctx)
{
	/* Turn on TEX Remap */
	SET_CB_SCTLR_TRE(base, ctx, 1);
}

static void msm_iommu_setup_memory_remap(void __iomem *base, unsigned int ctx)
{
	SET_PRRR(base, ctx, msm_iommu_get_prrr());
	SET_NMRR(base, ctx, msm_iommu_get_nmrr());
}

static void msm_iommu_setup_pg_l2_redirect(void __iomem *base, unsigned int ctx)
{
	/* Configure page tables as inner-cacheable and shareable to reduce
	 * the TLB miss penalty.
	 */
	SET_CB_TTBR0_S(base, ctx, 1);
	SET_CB_TTBR0_NOS(base, ctx, 1);
	SET_CB_TTBR0_IRGN1(base, ctx, 0); /* WB, WA */
	SET_CB_TTBR0_IRGN0(base, ctx, 1);
	SET_CB_TTBR0_RGN(base, ctx, 1);   /* WB, WA */
}

#endif

static int program_m2v_table(struct device *dev, void __iomem *base)
{
	struct msm_iommu_ctx_drvdata *ctx_drvdata = dev_get_drvdata(dev);
	u32 *sids = ctx_drvdata->sids;
	unsigned int ctx = ctx_drvdata->num;
	int num = 0, i, smt_size;
	int len = ctx_drvdata->nsid;

	smt_size = GET_IDR0_NUMSMRG(base);
	/* Program the M2V tables for this context */
	for (i = 0; i < len / sizeof(*sids); i++) {
		for (; num < smt_size; num++)
			if (GET_SMR_VALID(base, num) == 0)
				break;
		BUG_ON(num >= smt_size);

		SET_SMR_VALID(base, num, 1);
		SET_SMR_MASK(base, num, 0);
		SET_SMR_ID(base, num, sids[i]);

		SET_S2CR_N(base, num, 0);
		SET_S2CR_CBNDX(base, num, ctx);
		SET_S2CR_MEMATTR(base, num, 0x0A);
		/* Set security bit override to be Non-secure */
		SET_S2CR_NSCFG(base, num, 3);
	}

	return 0;
}

static void program_all_m2v_tables(struct msm_iommu_drvdata *iommu_drvdata)
{
	device_for_each_child(iommu_drvdata->dev, iommu_drvdata->base,
						program_m2v_table);
}

static void __program_context(struct msm_iommu_drvdata *iommu_drvdata,
			      struct msm_iommu_ctx_drvdata *ctx_drvdata,
			      struct msm_iommu_priv *priv, bool is_secure,
			      bool program_m2v)
{
	phys_addr_t pn;
	void __iomem *base = iommu_drvdata->base;
	void __iomem *cb_base = iommu_drvdata->cb_base;
	unsigned int ctx = ctx_drvdata->num;
	phys_addr_t pgtable = __pa(priv->pt.fl_table);

	__reset_context(iommu_drvdata, ctx);
	msm_iommu_setup_ctx(cb_base, ctx);

	if (priv->pt.redirect)
		msm_iommu_setup_pg_l2_redirect(cb_base, ctx);

	msm_iommu_setup_memory_remap(cb_base, ctx);

	pn = pgtable >> CB_TTBR0_ADDR_SHIFT;
	SET_CB_TTBR0_ADDR(cb_base, ctx, pn);

	/* Enable context fault interrupt */
	SET_CB_SCTLR_CFIE(cb_base, ctx, 1);

	if (iommu_drvdata->model != MMU_500) {
		/* Redirect all cacheable requests to L2 slave port. */
		SET_CB_ACTLR_BPRCISH(cb_base, ctx, 1);
		SET_CB_ACTLR_BPRCOSH(cb_base, ctx, 1);
		SET_CB_ACTLR_BPRCNSH(cb_base, ctx, 1);
	}

	/* Enable private ASID namespace */
	SET_CB_SCTLR_ASIDPNE(cb_base, ctx, 1);

	if (!is_secure) {
		if (program_m2v)
			program_all_m2v_tables(iommu_drvdata);

		SET_CBAR_N(base, ctx, 0);

		/* Stage 1 Context with Stage 2 bypass */
		SET_CBAR_TYPE(base, ctx, 1);

		/* Route page faults to the non-secure interrupt */
		SET_CBAR_IRPTNDX(base, ctx, 1);

		/* Set VMID to non-secure HLOS */
		SET_CBAR_VMID(base, ctx, 3);

		/* Bypass is treated as inner-shareable */
		SET_CBAR_BPSHCFG(base, ctx, 2);

		/* Do not downgrade memory attributes */
		SET_CBAR_MEMATTR(base, ctx, 0x0A);

	}

	msm_iommu_assign_ASID(iommu_drvdata, ctx_drvdata, priv);

	/* Enable the MMU */
	SET_CB_SCTLR_M(cb_base, ctx, 1);
	mb();
}

static int msm_iommu_domain_init(struct iommu_domain *domain, int flags)
{
	struct msm_iommu_priv *priv;

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
	struct msm_iommu_priv *priv;

	mutex_lock(&msm_iommu_lock);
	priv = domain->priv;
	domain->priv = NULL;

	if (priv)
		msm_iommu_pagetable_free(&priv->pt);

	kfree(priv);
	mutex_unlock(&msm_iommu_lock);
}

static int msm_iommu_attach_dev(struct iommu_domain *domain, struct device *dev)
{
	struct msm_iommu_priv *priv;
	struct msm_iommu_drvdata *iommu_drvdata;
	struct msm_iommu_ctx_drvdata *ctx_drvdata;
	struct msm_iommu_ctx_drvdata *tmp_drvdata;
	int ret = 0;
	int is_secure;
	bool set_m2v = false;

	mutex_lock(&msm_iommu_lock);

	priv = domain->priv;
	if (!priv || !dev) {
		ret = -EINVAL;
		goto unlock;
	}

	iommu_drvdata = dev_get_drvdata(dev->parent);
	ctx_drvdata = dev_get_drvdata(dev);
	if (!iommu_drvdata || !ctx_drvdata) {
		ret = -EINVAL;
		goto unlock;
	}

	++ctx_drvdata->attach_count;

	if (ctx_drvdata->attach_count > 1)
		goto already_attached;

	if (!list_empty(&ctx_drvdata->attached_elm)) {
		ret = -EBUSY;
		goto unlock;
	}

	list_for_each_entry(tmp_drvdata, &priv->list_attached, attached_elm)
		if (tmp_drvdata == ctx_drvdata) {
			ret = -EBUSY;
			goto unlock;
		}

	is_secure = iommu_drvdata->sec_id != -1;

	ret = __enable_regulators(iommu_drvdata);
	if (ret)
		goto unlock;

	ret = apply_bus_vote(iommu_drvdata, 1);
	if (ret)
		goto unlock;

	ret = __enable_clocks(iommu_drvdata);
	if (ret) {
		__disable_regulators(iommu_drvdata);
		goto unlock;
	}

	/* We can only do this once */
	if (!iommu_drvdata->ctx_attach_count) {
		if (!is_secure) {
			iommu_halt(iommu_drvdata);
			__program_iommu(iommu_drvdata);
			iommu_resume(iommu_drvdata);
		} else {
			ret = msm_iommu_sec_program_iommu(
				iommu_drvdata, ctx_drvdata);
			if (ret) {
				__disable_regulators(iommu_drvdata);
				__disable_clocks(iommu_drvdata);
				goto unlock;
			}
		}
		program_iommu_bfb_settings(iommu_drvdata->base,
					   iommu_drvdata->bfb_settings);
		set_m2v = true;
	}

	iommu_halt(iommu_drvdata);

	__program_context(iommu_drvdata, ctx_drvdata, priv, is_secure, set_m2v);

	iommu_resume(iommu_drvdata);

	__disable_clocks(iommu_drvdata);

	list_add(&(ctx_drvdata->attached_elm), &priv->list_attached);
	ctx_drvdata->attached_domain = domain;
	++iommu_drvdata->ctx_attach_count;

already_attached:
	mutex_unlock(&msm_iommu_lock);

	msm_iommu_attached(dev->parent);
	return ret;
unlock:
	mutex_unlock(&msm_iommu_lock);
	return ret;
}

static void msm_iommu_detach_dev(struct iommu_domain *domain,
				 struct device *dev)
{
	struct msm_iommu_priv *priv;
	struct msm_iommu_drvdata *iommu_drvdata;
	struct msm_iommu_ctx_drvdata *ctx_drvdata;
	int ret;
	int is_secure;

	msm_iommu_detached(dev->parent);

	mutex_lock(&msm_iommu_lock);
	priv = domain->priv;
	if (!priv || !dev)
		goto unlock;

	iommu_drvdata = dev_get_drvdata(dev->parent);
	ctx_drvdata = dev_get_drvdata(dev);
	if (!iommu_drvdata || !ctx_drvdata || !ctx_drvdata->attached_domain)
		goto unlock;

	--ctx_drvdata->attach_count;
	BUG_ON(ctx_drvdata->attach_count < 0);

	if (ctx_drvdata->attach_count > 0)
		goto unlock;

	ret = __enable_clocks(iommu_drvdata);
	if (ret)
		goto unlock;

	is_secure = iommu_drvdata->sec_id != -1;

	SET_TLBIASID(iommu_drvdata->cb_base, ctx_drvdata->num,
					ctx_drvdata->asid);

	BUG_ON(iommu_drvdata->asid[ctx_drvdata->asid - 1] == 0);
	iommu_drvdata->asid[ctx_drvdata->asid - 1]--;
	ctx_drvdata->asid = -1;

	__reset_context(iommu_drvdata, ctx_drvdata->num);

	/*
	 * Only reset the M2V tables on the very last detach */
	if (!is_secure && iommu_drvdata->ctx_attach_count == 1) {
		iommu_halt(iommu_drvdata);
		__release_smg(iommu_drvdata->base);
		iommu_resume(iommu_drvdata);
	}

	__disable_clocks(iommu_drvdata);

	apply_bus_vote(iommu_drvdata, 0);

	__disable_regulators(iommu_drvdata);

	list_del_init(&ctx_drvdata->attached_elm);
	ctx_drvdata->attached_domain = NULL;
	BUG_ON(iommu_drvdata->ctx_attach_count == 0);
	--iommu_drvdata->ctx_attach_count;
unlock:
	mutex_unlock(&msm_iommu_lock);
}

static int msm_iommu_map(struct iommu_domain *domain, unsigned long va,
			 phys_addr_t pa, size_t len, int prot)
{
	struct msm_iommu_priv *priv;
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
	struct msm_iommu_priv *priv;
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
	struct msm_iommu_priv *priv;

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
	struct msm_iommu_priv *priv;

	mutex_lock(&msm_iommu_lock);

	priv = domain->priv;
	msm_iommu_pagetable_unmap_range(&priv->pt, va, len);

	__flush_iotlb(domain);
	mutex_unlock(&msm_iommu_lock);
	return 0;
}

#ifdef CONFIG_IOMMU_LPAE
static phys_addr_t msm_iommu_get_phy_from_PAR(unsigned long va, u64 par)
{
	phys_addr_t phy;
	/* Upper 28 bits from PAR, lower 12 from VA */
	phy = (par & 0xFFFFFFF000ULL) | (va & 0x00000FFF);
	return phy;
}
#else
static phys_addr_t msm_iommu_get_phy_from_PAR(unsigned long va, u64 par)
{
	phys_addr_t phy;

	/* We are dealing with a supersection */
	if (par & CB_PAR_SS)
		phy = (par & 0xFF000000) | (va & 0x00FFFFFF);
	else /* Upper 20 bits from PAR, lower 12 from VA */
		phy = (par & 0xFFFFF000) | (va & 0x00000FFF);

	return phy;
}
#endif

static phys_addr_t msm_iommu_iova_to_phys(struct iommu_domain *domain,
					  phys_addr_t va)
{
	struct msm_iommu_priv *priv;
	struct msm_iommu_drvdata *iommu_drvdata;
	struct msm_iommu_ctx_drvdata *ctx_drvdata;
	u64 par;
	void __iomem *base;
	phys_addr_t ret = 0;
	int ctx;
	int i;

	mutex_lock(&msm_iommu_lock);

	priv = domain->priv;
	if (list_empty(&priv->list_attached))
		goto fail;

	ctx_drvdata = list_entry(priv->list_attached.next,
				 struct msm_iommu_ctx_drvdata, attached_elm);
	iommu_drvdata = dev_get_drvdata(ctx_drvdata->pdev->dev.parent);

	if (iommu_drvdata->model == MMU_500) {
		ret = msm_iommu_iova_to_phys_soft(domain, va);
		mutex_unlock(&msm_iommu_lock);
		return ret;
	}

	base = iommu_drvdata->cb_base;
	ctx = ctx_drvdata->num;

	ret = __enable_clocks(iommu_drvdata);
	if (ret) {
		ret = 0;	/* 0 indicates translation failed */
		goto fail;
	}

	SET_ATS1PR(base, ctx, va & CB_ATS1PR_ADDR);
	mb();
	for (i = 0; i < IOMMU_MSEC_TIMEOUT; i += IOMMU_MSEC_STEP)
		if (GET_CB_ATSR_ACTIVE(base, ctx) == 0)
			break;
		else
			msleep(IOMMU_MSEC_STEP);

	if (i >= IOMMU_MSEC_TIMEOUT) {
		pr_err("%s: iova to phys timed out on %pa for %s (%s)\n",
			__func__, &va, iommu_drvdata->name, ctx_drvdata->name);
		ret = 0;
		goto fail;
	}

	par = GET_PAR(base, ctx);
	__disable_clocks(iommu_drvdata);

	if (par & CB_PAR_F) {
		unsigned int level = (par & CB_PAR_PLVL) >> CB_PAR_PLVL_SHIFT;
		pr_err("IOMMU translation fault!\n");
		pr_err("name = %s\n", iommu_drvdata->name);
		pr_err("context = %s (%d)\n", ctx_drvdata->name,
						ctx_drvdata->num);
		pr_err("Interesting registers:\n");
		pr_err("PAR = %16llx [%s%s%s%s%s%s%s%sPLVL%u %s]\n", par,
			(par & CB_PAR_F) ? "F " : "",
			(par & CB_PAR_TF) ? "TF " : "",
			(par & CB_PAR_AFF) ? "AFF " : "",
			(par & CB_PAR_PF) ? "PF " : "",
			(par & CB_PAR_EF) ? "EF " : "",
			(par & CB_PAR_TLBMCF) ? "TLBMCF " : "",
			(par & CB_PAR_TLBLKF) ? "TLBLKF " : "",
			(par & CB_PAR_ATOT) ? "ATOT " : "",
			level,
			(par & CB_PAR_STAGE) ? "S2 " : "S1 ");
		ret = 0;
	} else {
		ret = msm_iommu_get_phy_from_PAR(va, par);
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

#ifdef CONFIG_IOMMU_LPAE
static inline void print_ctx_mem_attr_regs(struct msm_iommu_context_reg regs[])
{
	pr_err("MAIR0   = %08x    MAIR1   = %08x\n",
		 regs[DUMP_REG_MAIR0].val, regs[DUMP_REG_MAIR1].val);
}
#else
static inline void print_ctx_mem_attr_regs(struct msm_iommu_context_reg regs[])
{
	pr_err("PRRR   = %08x    NMRR   = %08x\n",
		 regs[DUMP_REG_PRRR].val, regs[DUMP_REG_NMRR].val);
}
#endif

void print_ctx_regs(struct msm_iommu_context_reg regs[])
{
	uint32_t fsr = regs[DUMP_REG_FSR].val;
	u64 ttbr;
	enum dump_reg iter;

	pr_err("FAR    = %016llx\n",
		COMBINE_DUMP_REG(
			regs[DUMP_REG_FAR1].val,
			regs[DUMP_REG_FAR0].val));
	pr_err("PAR    = %016llx\n",
		COMBINE_DUMP_REG(
			regs[DUMP_REG_PAR1].val,
			regs[DUMP_REG_PAR0].val));
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
		 regs[DUMP_REG_FSYNR0].val, regs[DUMP_REG_FSYNR1].val);

	ttbr = COMBINE_DUMP_REG(regs[DUMP_REG_TTBR0_1].val,
				regs[DUMP_REG_TTBR0_0].val);
	if (regs[DUMP_REG_TTBR0_1].valid)
		pr_err("TTBR0  = %016llx\n", ttbr);
	else
		pr_err("TTBR0  = %016llx (32b)\n", ttbr);

	ttbr = COMBINE_DUMP_REG(regs[DUMP_REG_TTBR1_1].val,
				regs[DUMP_REG_TTBR1_0].val);

	if (regs[DUMP_REG_TTBR1_1].valid)
		pr_err("TTBR1  = %016llx\n", ttbr);
	else
		pr_err("TTBR1  = %016llx (32b)\n", ttbr);

	pr_err("SCTLR  = %08x    ACTLR  = %08x\n",
		 regs[DUMP_REG_SCTLR].val, regs[DUMP_REG_ACTLR].val);
	pr_err("CBAR  = %08x    CBFRSYNRA  = %08x\n",
		regs[DUMP_REG_CBAR_N].val, regs[DUMP_REG_CBFRSYNRA_N].val);
	print_ctx_mem_attr_regs(regs);

	for (iter = DUMP_REG_FIRST; iter < MAX_DUMP_REGS; ++iter)
		if (!regs[iter].valid)
			pr_err("NOTE: Value actually unknown for %s\n",
				dump_regs_tbl[iter].name);
}

static void __print_ctx_regs(struct msm_iommu_drvdata *drvdata, int ctx,
					unsigned int fsr)
{
	void __iomem *base = drvdata->base;
	void __iomem *cb_base = drvdata->cb_base;
	bool is_secure = drvdata->sec_id != -1;

	struct msm_iommu_context_reg regs[MAX_DUMP_REGS];
	unsigned int i;
	memset(regs, 0, sizeof(regs));

	for (i = DUMP_REG_FIRST; i < MAX_DUMP_REGS; ++i) {
		struct msm_iommu_context_reg *r = &regs[i];
		unsigned long regaddr = dump_regs_tbl[i].reg_offset;
		if (is_secure &&
			dump_regs_tbl[i].dump_reg_type != DRT_CTX_REG) {
			r->valid = 0;
			continue;
		}
		r->valid = 1;
		switch (dump_regs_tbl[i].dump_reg_type) {
		case DRT_CTX_REG:
			r->val = GET_CTX_REG(regaddr, cb_base, ctx);
			break;
		case DRT_GLOBAL_REG:
			r->val = GET_GLOBAL_REG(regaddr, base);
			break;
		case DRT_GLOBAL_REG_N:
			r->val = GET_GLOBAL_REG_N(regaddr, ctx, base);
			break;
		default:
			pr_info("Unknown dump_reg_type...\n");
			r->valid = 0;
			break;
		}
	}
	print_ctx_regs(regs);
}

static void print_global_regs(void __iomem *base, unsigned int gfsr)
{
	pr_err("GFAR    = %016llx\n", GET_GFAR(base));

	pr_err("GFSR    = %08x [%s%s%s%s%s%s%s%s%s%s]\n", gfsr,
			(gfsr & 0x01) ? "ICF " : "",
			(gfsr & 0x02) ? "USF " : "",
			(gfsr & 0x04) ? "SMCF " : "",
			(gfsr & 0x08) ? "UCBF " : "",
			(gfsr & 0x10) ? "UCIF " : "",
			(gfsr & 0x20) ? "CAF " : "",
			(gfsr & 0x40) ? "EF " : "",
			(gfsr & 0x80) ? "PF " : "",
			(gfsr & 0x40000000) ? "SS " : "",
			(gfsr & 0x80000000) ? "MULTI " : "");

	pr_err("GFSYNR0	= %08x\n", GET_GFSYNR0(base));
	pr_err("GFSYNR1	= %08x\n", GET_GFSYNR1(base));
	pr_err("GFSYNR2	= %08x\n", GET_GFSYNR2(base));
}

irqreturn_t msm_iommu_global_fault_handler(int irq, void *dev_id)
{
	struct platform_device *pdev = dev_id;
	struct msm_iommu_drvdata *drvdata;
	unsigned int gfsr;
	int ret;

	mutex_lock(&msm_iommu_lock);
	BUG_ON(!pdev);

	drvdata = dev_get_drvdata(&pdev->dev);
	BUG_ON(!drvdata);

	if (!drvdata->powered_on) {
		pr_err("Unexpected IOMMU global fault !!\n");
		pr_err("name = %s\n", drvdata->name);
		pr_err("Power is OFF. Can't read global fault information\n");
		ret = IRQ_HANDLED;
		goto fail;
	}

	if (drvdata->sec_id != -1) {
		pr_err("NON-secure interrupt from secure %s\n", drvdata->name);
		ret = IRQ_HANDLED;
		goto fail;
	}

	ret = __enable_clocks(drvdata);
	if (ret) {
		ret = IRQ_NONE;
		goto fail;
	}

	gfsr = GET_GFSR(drvdata->base);
	if (gfsr) {
		pr_err("Unexpected %s global fault !!\n", drvdata->name);
		print_global_regs(drvdata->base, gfsr);
		SET_GFSR(drvdata->base, gfsr);
		ret = IRQ_HANDLED;
	} else
		ret = IRQ_NONE;

	__disable_clocks(drvdata);
fail:
	mutex_unlock(&msm_iommu_lock);
	return ret;
}

irqreturn_t msm_iommu_fault_handler_v2(int irq, void *dev_id)
{
	struct platform_device *pdev = dev_id;
	struct msm_iommu_drvdata *drvdata;
	struct msm_iommu_ctx_drvdata *ctx_drvdata;
	unsigned int fsr;
	int ret;

	phys_addr_t pagetable_phys;
	u64 faulty_iova = 0;

	mutex_lock(&msm_iommu_lock);

	BUG_ON(!pdev);

	drvdata = dev_get_drvdata(pdev->dev.parent);
	BUG_ON(!drvdata);

	ctx_drvdata = dev_get_drvdata(&pdev->dev);
	BUG_ON(!ctx_drvdata);

	if (!drvdata->powered_on) {
		pr_err("Unexpected IOMMU page fault!\n");
		pr_err("name = %s\n", drvdata->name);
		pr_err("Power is OFF. Unable to read page fault information\n");
		/*
		 * We cannot determine which context bank caused the issue so
		 * we just return handled here to ensure IRQ handler code is
		 * happy
		 */
		ret = IRQ_HANDLED;
		goto fail;
	}

	ret = __enable_clocks(drvdata);
	if (ret) {
		ret = IRQ_NONE;
		goto fail;
	}

	fsr = GET_FSR(drvdata->cb_base, ctx_drvdata->num);
	if (fsr) {
		if (!ctx_drvdata->attached_domain) {
			pr_err("Bad domain in interrupt handler\n");
			ret = -ENOSYS;
		} else {
			faulty_iova =
				GET_FAR(drvdata->cb_base, ctx_drvdata->num);
			ret = report_iommu_fault(ctx_drvdata->attached_domain,
				&ctx_drvdata->pdev->dev,
				faulty_iova, 0);

		}
		if (ret == -ENOSYS) {
			pr_err("Unexpected IOMMU page fault!\n");
			pr_err("name = %s\n", drvdata->name);
			pr_err("context = %s (%d)\n", ctx_drvdata->name,
							ctx_drvdata->num);
			pr_err("Interesting registers:\n");
			__print_ctx_regs(drvdata,
					ctx_drvdata->num, fsr);

			if (ctx_drvdata->attached_domain) {
				pagetable_phys = msm_iommu_iova_to_phys_soft(
					ctx_drvdata->attached_domain,
					faulty_iova);
				pr_err("Page table in DDR shows PA = %x\n",
					(unsigned int) pagetable_phys);
			}
		}

		if (ret != -EBUSY)
			SET_FSR(drvdata->cb_base, ctx_drvdata->num, fsr);
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
	struct msm_iommu_priv *priv = domain->priv;
	return __pa(priv->pt.fl_table);
}

#define DUMP_REG_INIT(dump_reg, cb_reg, mbp, drt)		\
	do {							\
		dump_regs_tbl[dump_reg].reg_offset = cb_reg;	\
		dump_regs_tbl[dump_reg].name = #cb_reg;		\
		dump_regs_tbl[dump_reg].must_be_present = mbp;	\
		dump_regs_tbl[dump_reg].dump_reg_type = drt;	\
	} while (0)

static void msm_iommu_build_dump_regs_table(void)
{
	DUMP_REG_INIT(DUMP_REG_FAR0,	CB_FAR,       1, DRT_CTX_REG);
	DUMP_REG_INIT(DUMP_REG_FAR1,	CB_FAR + 4,   1, DRT_CTX_REG);
	DUMP_REG_INIT(DUMP_REG_PAR0,	CB_PAR,       1, DRT_CTX_REG);
	DUMP_REG_INIT(DUMP_REG_PAR1,	CB_PAR + 4,   1, DRT_CTX_REG);
	DUMP_REG_INIT(DUMP_REG_FSR,	CB_FSR,       1, DRT_CTX_REG);
	DUMP_REG_INIT(DUMP_REG_FSYNR0,	CB_FSYNR0,    1, DRT_CTX_REG);
	DUMP_REG_INIT(DUMP_REG_FSYNR1,	CB_FSYNR1,    1, DRT_CTX_REG);
	DUMP_REG_INIT(DUMP_REG_TTBR0_0,	CB_TTBR0,     1, DRT_CTX_REG);
	DUMP_REG_INIT(DUMP_REG_TTBR0_1,	CB_TTBR0 + 4, 0, DRT_CTX_REG);
	DUMP_REG_INIT(DUMP_REG_TTBR1_0,	CB_TTBR1,     1, DRT_CTX_REG);
	DUMP_REG_INIT(DUMP_REG_TTBR1_1,	CB_TTBR1 + 4, 0, DRT_CTX_REG);
	DUMP_REG_INIT(DUMP_REG_SCTLR,	CB_SCTLR,     1, DRT_CTX_REG);
	DUMP_REG_INIT(DUMP_REG_ACTLR,	CB_ACTLR,     1, DRT_CTX_REG);
	DUMP_REG_INIT(DUMP_REG_PRRR,	CB_PRRR,      1, DRT_CTX_REG);
	DUMP_REG_INIT(DUMP_REG_NMRR,	CB_NMRR,      1, DRT_CTX_REG);
	DUMP_REG_INIT(DUMP_REG_CBAR_N,	CBAR,         1, DRT_GLOBAL_REG_N);
	DUMP_REG_INIT(DUMP_REG_CBFRSYNRA_N, CBFRSYNRA, 1, DRT_GLOBAL_REG_N);
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
	msm_iommu_build_dump_regs_table();

	return 0;
}

subsys_initcall(msm_iommu_init);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MSM SMMU v2 Driver");
