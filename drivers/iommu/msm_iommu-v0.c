/* Copyright (c) 2010-2015, The Linux Foundation. All rights reserved.
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
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/iommu.h>
#include <linux/clk.h>
#include <linux/scatterlist.h>

#include <asm/cacheflush.h>
#include <asm/sizes.h>

#include "msm_iommu_perfmon.h"
#include "msm_iommu_hw-v0.h"
#include "msm_iommu_priv.h"
#include <linux/qcom_iommu.h>
#include <linux/msm-bus.h>

#include <soc/qcom/smem.h>

/* Sharability attributes of MSM IOMMU mappings */
#define MSM_IOMMU_ATTR_NON_SH		0x0
#define MSM_IOMMU_ATTR_SH		0x4

/* Cacheability attributes of MSM IOMMU mappings */
#define MSM_IOMMU_ATTR_NONCACHED	0x0
#define MSM_IOMMU_ATTR_CACHED_WB_WA	0x1
#define MSM_IOMMU_ATTR_CACHED_WB_NWA	0x2
#define MSM_IOMMU_ATTR_CACHED_WT	0x3

static int msm_iommu_unmap_range(struct iommu_domain *domain, unsigned long va,
				 size_t len);

static inline void clean_pte(u32 *start, u32 *end,
			     int redirect)
{
	if (!redirect)
		dmac_flush_range(start, end);
}

/* bitmap of the page sizes currently supported */
#define MSM_IOMMU_PGSIZES	(SZ_4K | SZ_64K | SZ_1M | SZ_16M)

static int msm_iommu_tex_class[4];

DEFINE_MUTEX(msm_iommu_lock);

/**
 * Remote spinlock implementation based on Peterson's algorithm to be used
 * to synchronize IOMMU config port access between CPU and GPU.
 * This implements Process 0 of the spin lock algorithm. GPU implements
 * Process 1. Flag and turn is stored in shared memory to allow GPU to
 * access these.
 */
struct msm_iommu_remote_lock {
	int initialized;
	struct remote_iommu_petersons_spinlock *lock;
};

static struct msm_iommu_remote_lock msm_iommu_remote_lock;

#ifdef CONFIG_MSM_IOMMU_SYNC
static void _msm_iommu_remote_spin_lock_init(void)
{
	msm_iommu_remote_lock.lock = smem_find(SMEM_SPINLOCK_ARRAY, 32,
							0, SMEM_ANY_HOST_FLAG);
	memset(msm_iommu_remote_lock.lock, 0,
			sizeof(*msm_iommu_remote_lock.lock));
}

void msm_iommu_remote_p0_spin_lock(unsigned int need_lock)
{
	if (!need_lock)
		return;

	msm_iommu_remote_lock.lock->flag[PROC_APPS] = 1;
	msm_iommu_remote_lock.lock->turn = 1;

	smp_mb();

	while (msm_iommu_remote_lock.lock->flag[PROC_GPU] == 1 &&
	       msm_iommu_remote_lock.lock->turn == 1)
		cpu_relax();
}

void msm_iommu_remote_p0_spin_unlock(unsigned int need_lock)
{
	if (!need_lock)
		return;

	smp_mb();

	msm_iommu_remote_lock.lock->flag[PROC_APPS] = 0;
}
#endif

inline void msm_iommu_mutex_lock(void)
{
	mutex_lock(&msm_iommu_lock);
}

inline void msm_iommu_mutex_unlock(void)
{
	mutex_unlock(&msm_iommu_lock);
}

void *msm_iommu_lock_initialize(void)
{
	mutex_lock(&msm_iommu_lock);
	if (!msm_iommu_remote_lock.initialized) {
		msm_iommu_remote_lock_init();
		msm_iommu_remote_lock.initialized = 1;
	}
	mutex_unlock(&msm_iommu_lock);
	return msm_iommu_remote_lock.lock;
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

	if (drvdata->clk) {
		ret = clk_prepare_enable(drvdata->clk);
		if (ret)
			clk_disable_unprepare(drvdata->pclk);
	}

	if (ret)
		goto fail;

	if (drvdata->aclk) {
		ret = clk_prepare_enable(drvdata->aclk);
		if (ret) {
			clk_disable_unprepare(drvdata->clk);
			clk_disable_unprepare(drvdata->pclk);
		}
	}

fail:
	return ret;
}

static void __disable_clocks(struct msm_iommu_drvdata *drvdata)
{
	if (drvdata->aclk)
		clk_disable_unprepare(drvdata->aclk);
	if (drvdata->clk)
		clk_disable_unprepare(drvdata->clk);
	clk_disable_unprepare(drvdata->pclk);
}

static int __enable_regulators(struct msm_iommu_drvdata *drvdata)
{
	/* No need to do anything. IOMMUv0 is always on. */
	return 0;
}

static void __disable_regulators(struct msm_iommu_drvdata *drvdata)
{
	/* No need to do anything. IOMMUv0 is always on. */
}

static void *_iommu_lock_initialize(void)
{
	return msm_iommu_lock_initialize();
}

static void _iommu_lock_acquire(unsigned int need_extra_lock)
{
	msm_iommu_mutex_lock();
	msm_iommu_remote_spin_lock(need_extra_lock);
}

static void _iommu_lock_release(unsigned int need_extra_lock)
{
	msm_iommu_remote_spin_unlock(need_extra_lock);
	msm_iommu_mutex_unlock();
}

struct iommu_access_ops iommu_access_ops_v0 = {
	.iommu_power_on = __enable_regulators,
	.iommu_power_off = __disable_regulators,
	.iommu_bus_vote = apply_bus_vote,
	.iommu_clk_on = __enable_clocks,
	.iommu_clk_off = __disable_clocks,
	.iommu_lock_initialize = _iommu_lock_initialize,
	.iommu_lock_acquire = _iommu_lock_acquire,
	.iommu_lock_release = _iommu_lock_release,
};

static int __flush_iotlb_va(struct iommu_domain *domain, unsigned int va)
{
	struct msm_iommu_priv *priv = domain->priv;
	struct msm_iommu_drvdata *iommu_drvdata;
	struct msm_iommu_ctx_drvdata *ctx_drvdata;
	int ret = 0;
	int asid;

	list_for_each_entry(ctx_drvdata, &priv->list_attached, attached_elm) {
		if (!ctx_drvdata->pdev || !ctx_drvdata->pdev->dev.parent)
			BUG();

		iommu_drvdata = dev_get_drvdata(ctx_drvdata->pdev->dev.parent);
		if (!iommu_drvdata)
			BUG();

		ret = __enable_clocks(iommu_drvdata);
		if (ret)
			goto fail;

		msm_iommu_remote_spin_lock(iommu_drvdata->needs_rem_spinlock);

		asid = GET_CONTEXTIDR_ASID(iommu_drvdata->base,
					   ctx_drvdata->num);

		SET_TLBIVA(iommu_drvdata->base, ctx_drvdata->num,
			   asid | (va & TLBIVA_VA));
		mb();

		msm_iommu_remote_spin_unlock(iommu_drvdata->needs_rem_spinlock);

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
	int asid;

	list_for_each_entry(ctx_drvdata, &priv->list_attached, attached_elm) {
		if (!ctx_drvdata->pdev || !ctx_drvdata->pdev->dev.parent)
			BUG();

		iommu_drvdata = dev_get_drvdata(ctx_drvdata->pdev->dev.parent);
		if (!iommu_drvdata)
			BUG();

		ret = __enable_clocks(iommu_drvdata);
		if (ret)
			goto fail;

		msm_iommu_remote_spin_lock(iommu_drvdata->needs_rem_spinlock);

		asid = GET_CONTEXTIDR_ASID(iommu_drvdata->base,
					   ctx_drvdata->num);

		SET_TLBIASID(iommu_drvdata->base, ctx_drvdata->num, asid);
		mb();

		msm_iommu_remote_spin_unlock(iommu_drvdata->needs_rem_spinlock);

		__disable_clocks(iommu_drvdata);
	}
fail:
	return ret;
}

static void __reset_context(void __iomem *base, void __iomem *glb_base, int ctx)
{
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
	SET_PRRR(base, ctx, 0);
	SET_NMRR(base, ctx, 0);
	mb();
}

static void __program_context(struct msm_iommu_drvdata *iommu_drvdata,
			      int ctx, int ncb, phys_addr_t pgtable,
			      int redirect, int ttbr_split)
{
	void __iomem *base = iommu_drvdata->base;
	void __iomem *glb_base = iommu_drvdata->glb_base;
	unsigned int prrr, nmrr;
	int i, j, found;

	msm_iommu_remote_spin_lock(iommu_drvdata->needs_rem_spinlock);

	__reset_context(base, glb_base, ctx);

	/* Set up HTW mode */
	/* TLB miss configuration: perform HTW on miss */
	SET_TLBMCFG(base, ctx, 0x3);

	/* V2P configuration: HTW for access */
	SET_V2PCFG(base, ctx, 0x3);

	SET_TTBCR(base, ctx, ttbr_split);
	SET_TTBR0_PA(base, ctx, (pgtable >> TTBR0_PA_SHIFT));
	if (ttbr_split)
		SET_TTBR1_PA(base, ctx, (pgtable >> TTBR1_PA_SHIFT));

	/* Enable context fault interrupt */
	SET_CFEIE(base, ctx, 1);

	/* Stall access on a context fault and let the handler deal with it */
	SET_CFCFG(base, ctx, 1);

	/* Redirect all cacheable requests to L2 slave port. */
	SET_RCISH(base, ctx, 1);
	SET_RCOSH(base, ctx, 1);
	SET_RCNSH(base, ctx, 1);

	/* Turn on TEX Remap */
	SET_TRE(base, ctx, 1);

	/* Set TEX remap attributes */
	prrr = msm_iommu_get_prrr();
	nmrr = msm_iommu_get_nmrr();
	SET_PRRR(base, ctx, prrr);
	SET_NMRR(base, ctx, nmrr);

	/* Turn on BFB prefetch */
	SET_BFBDFE(base, ctx, 1);

	/* Configure page tables as inner-cacheable and shareable to reduce
	 * the TLB miss penalty.
	 */
	if (redirect) {
		SET_TTBR0_SH(base, ctx, 1);
		SET_TTBR1_SH(base, ctx, 1);

		SET_TTBR0_NOS(base, ctx, 1);
		SET_TTBR1_NOS(base, ctx, 1);

		SET_TTBR0_IRGNH(base, ctx, 0); /* WB, WA */
		SET_TTBR0_IRGNL(base, ctx, 1);

		SET_TTBR1_IRGNH(base, ctx, 0); /* WB, WA */
		SET_TTBR1_IRGNL(base, ctx, 1);

		SET_TTBR0_ORGN(base, ctx, 1); /* WB, WA */
		SET_TTBR1_ORGN(base, ctx, 1); /* WB, WA */
	}

	/* Find if this page table is used elsewhere, and re-use ASID */
	found = 0;
	for (i = 0; i < ncb; i++)
		if (GET_TTBR0_PA(base, i) == (pgtable >> TTBR0_PA_SHIFT) &&
		    i != ctx) {
			SET_CONTEXTIDR_ASID(base, ctx, \
					    GET_CONTEXTIDR_ASID(base, i));
			found = 1;
			break;
		}

	/* If page table is new, find an unused ASID */
	if (!found) {
		for (i = 0; i < ncb; i++) {
			found = 0;
			for (j = 0; j < ncb; j++) {
				if (GET_CONTEXTIDR_ASID(base, j) == i &&
				    j != ctx)
					found = 1;
			}

			if (!found) {
				SET_CONTEXTIDR_ASID(base, ctx, i);
				break;
			}
		}
		BUG_ON(found);
	}

	/* Enable the MMU */
	SET_M(base, ctx, 1);
	mb();

	msm_iommu_remote_spin_unlock(iommu_drvdata->needs_rem_spinlock);
}

#ifdef CONFIG_IOMMU_PGTABLES_L2
#define INITIAL_REDIRECT_VAL 1
#else
#define INITIAL_REDIRECT_VAL 0
#endif

static int msm_iommu_domain_init(struct iommu_domain *domain)
{
	struct msm_iommu_priv *priv = kzalloc(sizeof(*priv), GFP_KERNEL);

	if (!priv)
		goto fail_nomem;

	INIT_LIST_HEAD(&priv->list_attached);
	priv->pt.fl_table = (u32 *)__get_free_pages(GFP_KERNEL,
							  get_order(SZ_16K));

	if (!priv->pt.fl_table)
		goto fail_nomem;

	priv->pt.redirect = INITIAL_REDIRECT_VAL;

	memset(priv->pt.fl_table, 0, SZ_16K);
	domain->priv = priv;

	clean_pte(priv->pt.fl_table, priv->pt.fl_table + NUM_FL_PTE,
		  priv->pt.redirect);

	domain->geometry.aperture_start = 0;
	domain->geometry.aperture_end   = (1ULL << 32) - 1;
	domain->geometry.force_aperture = true;

	return 0;

fail_nomem:
	kfree(priv);
	return -ENOMEM;
}

static void msm_iommu_domain_destroy(struct iommu_domain *domain)
{
	struct msm_iommu_priv *priv;
	u32 *fl_table;
	int i;

	mutex_lock(&msm_iommu_lock);
	priv = domain->priv;
	domain->priv = NULL;

	if (priv) {
		fl_table = priv->pt.fl_table;

		for (i = 0; i < NUM_FL_PTE; i++)
			if ((fl_table[i] & 0x03) == FL_TYPE_TABLE)
				free_page((unsigned long) __va(((fl_table[i]) &
								FL_BASE_MASK)));

		free_pages((unsigned long)priv->pt.fl_table, get_order(SZ_16K));
		priv->pt.fl_table = NULL;
	}

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

	ret = apply_bus_vote(iommu_drvdata, 1);

	if (ret)
		goto unlock;

	ret = __enable_clocks(iommu_drvdata);
	if (ret)
		goto unlock;

	__program_context(iommu_drvdata,
			  ctx_drvdata->num, iommu_drvdata->ncb,
			  __pa(priv->pt.fl_table), priv->pt.redirect,
			  iommu_drvdata->ttbr_split);

	__disable_clocks(iommu_drvdata);
	list_add(&(ctx_drvdata->attached_elm), &priv->list_attached);

	ctx_drvdata->attached_domain = domain;

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

	msm_iommu_detached(dev->parent);

	mutex_lock(&msm_iommu_lock);
	priv = domain->priv;

	if (!priv || !dev)
		goto unlock;

	iommu_drvdata = dev_get_drvdata(dev->parent);
	ctx_drvdata = dev_get_drvdata(dev);

	if (!iommu_drvdata || !ctx_drvdata)
		goto unlock;

	--ctx_drvdata->attach_count;
	BUG_ON(ctx_drvdata->attach_count < 0);

	if (ctx_drvdata->attach_count > 0)
		goto unlock;

	ret = __enable_clocks(iommu_drvdata);
	if (ret)
		goto unlock;

	msm_iommu_remote_spin_lock(iommu_drvdata->needs_rem_spinlock);

	SET_TLBIASID(iommu_drvdata->base, ctx_drvdata->num,
		    GET_CONTEXTIDR_ASID(iommu_drvdata->base, ctx_drvdata->num));

	__reset_context(iommu_drvdata->base, iommu_drvdata->glb_base,
			ctx_drvdata->num);

	msm_iommu_remote_spin_unlock(iommu_drvdata->needs_rem_spinlock);

	__disable_clocks(iommu_drvdata);

	apply_bus_vote(iommu_drvdata, 0);

	list_del_init(&ctx_drvdata->attached_elm);
	ctx_drvdata->attached_domain = NULL;
unlock:
	mutex_unlock(&msm_iommu_lock);
}

static int __get_pgprot(int prot, int len)
{
	unsigned int pgprot;
	int tex;

	if (!(prot & (IOMMU_READ | IOMMU_WRITE))) {
		prot |= IOMMU_READ | IOMMU_WRITE;
		WARN_ONCE(1, "No attributes in iommu mapping; assuming RW\n");
	}

	if ((prot & IOMMU_WRITE) && !(prot & IOMMU_READ)) {
		prot |= IOMMU_READ;
		WARN_ONCE(1, "Write-only iommu mappings unsupported; falling back to RW\n");
	}

	if (prot & IOMMU_CACHE)
		tex = (pgprot_kernel >> 2) & 0x07;
	else
		tex = msm_iommu_tex_class[MSM_IOMMU_ATTR_NONCACHED];

	if (tex < 0 || tex > NUM_TEX_CLASS - 1)
		return 0;

	if (len == SZ_16M || len == SZ_1M) {
		pgprot = FL_SHARED;
		pgprot |= tex & 0x01 ? FL_BUFFERABLE : 0;
		pgprot |= tex & 0x02 ? FL_CACHEABLE : 0;
		pgprot |= tex & 0x04 ? FL_TEX0 : 0;
		pgprot |= FL_AP0 | FL_AP1;
		pgprot |= prot & IOMMU_WRITE ? 0 : FL_AP2;
	} else	{
		pgprot = SL_SHARED;
		pgprot |= tex & 0x01 ? SL_BUFFERABLE : 0;
		pgprot |= tex & 0x02 ? SL_CACHEABLE : 0;
		pgprot |= tex & 0x04 ? SL_TEX0 : 0;
		pgprot |= SL_AP0 | SL_AP1;
		pgprot |= prot & IOMMU_WRITE ? 0 : SL_AP2;
	}

	return pgprot;
}

static u32 *make_second_level(struct msm_iommu_priv *priv,
					u32 *fl_pte)
{
	u32 *sl;
	sl = (u32 *) __get_free_pages(GFP_KERNEL,
			get_order(SZ_4K));

	if (!sl) {
		pr_debug("Could not allocate second level table\n");
		goto fail;
	}
	memset(sl, 0, SZ_4K);
	clean_pte(sl, sl + NUM_SL_PTE, priv->pt.redirect);

	*fl_pte = ((((int)__pa(sl)) & FL_BASE_MASK) | \
			FL_TYPE_TABLE);

	clean_pte(fl_pte, fl_pte + 1, priv->pt.redirect);
fail:
	return sl;
}

static int sl_4k(u32 *sl_pte, phys_addr_t pa, unsigned int pgprot)
{
	int ret = 0;

	if (*sl_pte) {
		ret = -EBUSY;
		goto fail;
	}

	*sl_pte = (pa & SL_BASE_MASK_SMALL) | SL_NG | SL_SHARED
		| SL_TYPE_SMALL | pgprot;
fail:
	return ret;
}

static int sl_64k(u32 *sl_pte, phys_addr_t pa, unsigned int pgprot)
{
	int ret = 0;

	int i;

	for (i = 0; i < 16; i++)
		if (*(sl_pte+i)) {
			ret = -EBUSY;
			goto fail;
		}

	for (i = 0; i < 16; i++)
		*(sl_pte+i) = (pa & SL_BASE_MASK_LARGE) | SL_NG
				| SL_SHARED | SL_TYPE_LARGE | pgprot;

fail:
	return ret;
}


static inline int fl_1m(u32 *fl_pte, phys_addr_t pa, int pgprot)
{
	if (*fl_pte)
		return -EBUSY;

	*fl_pte = (pa & 0xFFF00000) | FL_NG | FL_TYPE_SECT | FL_SHARED
		| pgprot;

	return 0;
}


static inline int fl_16m(u32 *fl_pte, phys_addr_t pa, int pgprot)
{
	int i;
	int ret = 0;
	for (i = 0; i < 16; i++)
		if (*(fl_pte+i)) {
			ret = -EBUSY;
			goto fail;
		}
	for (i = 0; i < 16; i++)
		*(fl_pte+i) = (pa & 0xFF000000) | FL_SUPERSECTION
			| FL_TYPE_SECT | FL_SHARED | FL_NG | pgprot;
fail:
	return ret;
}

static int msm_iommu_map(struct iommu_domain *domain, unsigned long va,
			 phys_addr_t pa, size_t len, int prot)
{
	struct msm_iommu_priv *priv;
	u32 *fl_table;
	u32 *fl_pte;
	u32 fl_offset;
	u32 *sl_table;
	u32 *sl_pte;
	u32 sl_offset;
	unsigned int pgprot;
	int ret = 0;

	mutex_lock(&msm_iommu_lock);

	priv = domain->priv;
	if (!priv) {
		ret = -EINVAL;
		goto fail;
	}

	fl_table = priv->pt.fl_table;

	if (len != SZ_16M && len != SZ_1M &&
	    len != SZ_64K && len != SZ_4K) {
		pr_debug("Bad size: %d\n", len);
		ret = -EINVAL;
		goto fail;
	}

	if (!fl_table) {
		pr_debug("Null page table\n");
		ret = -EINVAL;
		goto fail;
	}

	pgprot = __get_pgprot(prot, len);

	if (!pgprot) {
		ret = -EINVAL;
		goto fail;
	}

	fl_offset = FL_OFFSET(va);	/* Upper 12 bits */
	fl_pte = fl_table + fl_offset;	/* int pointers, 4 bytes */

	if (len == SZ_16M) {
		ret = fl_16m(fl_pte, pa, pgprot);
		if (ret)
			goto fail;
		clean_pte(fl_pte, fl_pte + 16, priv->pt.redirect);
	}

	if (len == SZ_1M) {
		ret = fl_1m(fl_pte, pa, pgprot);
		if (ret)
			goto fail;
		clean_pte(fl_pte, fl_pte + 1, priv->pt.redirect);
	}

	/* Need a 2nd level table */
	if (len == SZ_4K || len == SZ_64K) {

		if (*fl_pte == 0) {
			if (make_second_level(priv, fl_pte) == NULL) {
				ret = -ENOMEM;
				goto fail;
			}
		}

		if (!(*fl_pte & FL_TYPE_TABLE)) {
			ret = -EBUSY;
			goto fail;
		}
	}

	sl_table = (u32 *) __va(((*fl_pte) & FL_BASE_MASK));
	sl_offset = SL_OFFSET(va);
	sl_pte = sl_table + sl_offset;

	if (len == SZ_4K) {
		ret = sl_4k(sl_pte, pa, pgprot);
		if (ret)
			goto fail;

		clean_pte(sl_pte, sl_pte + 1, priv->pt.redirect);
	}

	if (len == SZ_64K) {
		ret = sl_64k(sl_pte, pa, pgprot);
		if (ret)
			goto fail;
		clean_pte(sl_pte, sl_pte + 16, priv->pt.redirect);
	}

	ret = __flush_iotlb_va(domain, va);
fail:
	mutex_unlock(&msm_iommu_lock);
	return ret;
}

static size_t msm_iommu_unmap(struct iommu_domain *domain, unsigned long va,
			    size_t len)
{
	struct msm_iommu_priv *priv;
	u32 *fl_table;
	u32 *fl_pte;
	u32 fl_offset;
	u32 *sl_table;
	u32 *sl_pte;
	u32 sl_offset;
	int i, ret = 0;

	mutex_lock(&msm_iommu_lock);

	priv = domain->priv;

	if (!priv)
		goto fail;

	fl_table = priv->pt.fl_table;

	if (len != SZ_16M && len != SZ_1M &&
	    len != SZ_64K && len != SZ_4K) {
		pr_debug("Bad length: %d\n", len);
		goto fail;
	}

	if (!fl_table) {
		pr_debug("Null page table\n");
		goto fail;
	}

	fl_offset = FL_OFFSET(va);	/* Upper 12 bits */
	fl_pte = fl_table + fl_offset;	/* int pointers, 4 bytes */

	if (*fl_pte == 0) {
		pr_debug("First level PTE is 0\n");
		goto fail;
	}

	/* Unmap supersection */
	if (len == SZ_16M) {
		for (i = 0; i < 16; i++)
			*(fl_pte+i) = 0;

		clean_pte(fl_pte, fl_pte + 16, priv->pt.redirect);
	}

	if (len == SZ_1M) {
		*fl_pte = 0;

		clean_pte(fl_pte, fl_pte + 1, priv->pt.redirect);
	}

	sl_table = (u32 *) __va(((*fl_pte) & FL_BASE_MASK));
	sl_offset = SL_OFFSET(va);
	sl_pte = sl_table + sl_offset;

	if (len == SZ_64K) {
		for (i = 0; i < 16; i++)
			*(sl_pte+i) = 0;

		clean_pte(sl_pte, sl_pte + 16, priv->pt.redirect);
	}

	if (len == SZ_4K) {
		*sl_pte = 0;

		clean_pte(sl_pte, sl_pte + 1, priv->pt.redirect);
	}

	if (len == SZ_4K || len == SZ_64K) {
		int used = 0;

		for (i = 0; i < NUM_SL_PTE; i++)
			if (sl_table[i])
				used = 1;
		if (!used) {
			free_page((unsigned long)sl_table);
			*fl_pte = 0;

			clean_pte(fl_pte, fl_pte + 1, priv->pt.redirect);
		}
	}

	ret = __flush_iotlb_va(domain, va);

fail:
	mutex_unlock(&msm_iommu_lock);

	/* the IOMMU API requires us to return how many bytes were unmapped */
	len = ret ? 0 : len;
	return len;
}

static unsigned int get_phys_addr(struct scatterlist *sg)
{
	/*
	 * Try sg_dma_address first so that we can
	 * map carveout regions that do not have a
	 * struct page associated with them.
	 */
	unsigned int pa = sg_dma_address(sg);
	if (pa == 0)
		pa = sg_phys(sg);
	return pa;
}

static inline int is_fully_aligned(unsigned int va, phys_addr_t pa, size_t len,
				   int align)
{
	return  IS_ALIGNED(va, align) && IS_ALIGNED(pa, align)
		&& (len >= align);
}

static int check_range(u32 *fl_table, unsigned int va,
				 unsigned int len)
{
	unsigned int offset = 0;
	u32 *fl_pte;
	u32 fl_offset;
	u32 *sl_table;
	u32 sl_start, sl_end;
	int i;

	fl_offset = FL_OFFSET(va);	/* Upper 12 bits */
	fl_pte = fl_table + fl_offset;	/* int pointers, 4 bytes */

	while (offset < len) {
		if (*fl_pte & FL_TYPE_TABLE) {
			sl_start = SL_OFFSET(va);
			sl_table =  __va(((*fl_pte) & FL_BASE_MASK));
			sl_end = ((len - offset) / SZ_4K) + sl_start;

			if (sl_end > NUM_SL_PTE)
				sl_end = NUM_SL_PTE;

			for (i = sl_start; i < sl_end; i++) {
				if (sl_table[i] != 0) {
					pr_err("%08x - %08x already mapped\n",
						va, va + SZ_4K);
					return -EBUSY;
				}
				offset += SZ_4K;
				va += SZ_4K;
			}


			sl_start = 0;
		} else {
			if (*fl_pte != 0) {
				pr_err("%08x - %08x already mapped\n",
				       va, va + SZ_1M);
				return -EBUSY;
			}
			va += SZ_1M;
			offset += SZ_1M;
			sl_start = 0;
		}
		fl_pte++;
	}
	return 0;
}

static int msm_iommu_map_range(struct iommu_domain *domain, unsigned long va,
			       struct scatterlist *sg, size_t len,
			       int prot)
{
	unsigned int pa;
	unsigned int start_va = va;
	unsigned int offset = 0;
	u32 *fl_table;
	u32 *fl_pte;
	u32 fl_offset;
	u32 *sl_table = NULL;
	u32 sl_offset, sl_start;
	unsigned int chunk_size, chunk_offset = 0;
	int ret = 0;
	struct msm_iommu_priv *priv;
	unsigned int pgprot4k, pgprot64k, pgprot1m, pgprot16m;

	mutex_lock(&msm_iommu_lock);

	BUG_ON(len & (SZ_4K - 1));

	priv = domain->priv;
	fl_table = priv->pt.fl_table;

	pgprot4k = __get_pgprot(prot, SZ_4K);
	pgprot64k = __get_pgprot(prot, SZ_64K);
	pgprot1m = __get_pgprot(prot, SZ_1M);
	pgprot16m = __get_pgprot(prot, SZ_16M);

	if (!pgprot4k || !pgprot64k || !pgprot1m || !pgprot16m) {
		ret = -EINVAL;
		goto fail;
	}
	ret = check_range(fl_table, va, len);
	if (ret)
		goto fail;

	fl_offset = FL_OFFSET(va);	/* Upper 12 bits */
	fl_pte = fl_table + fl_offset;	/* int pointers, 4 bytes */
	pa = get_phys_addr(sg);

	while (offset < len) {
		chunk_size = SZ_4K;

		if (is_fully_aligned(va, pa, sg->length - chunk_offset,
				     SZ_16M))
			chunk_size = SZ_16M;
		else if (is_fully_aligned(va, pa, sg->length - chunk_offset,
					  SZ_1M))
			chunk_size = SZ_1M;
		/* 64k or 4k determined later */

		/* for 1M and 16M, only first level entries are required */
		if (chunk_size >= SZ_1M) {
			if (chunk_size == SZ_16M) {
				ret = fl_16m(fl_pte, pa, pgprot16m);
				if (ret)
					goto fail;
				clean_pte(fl_pte, fl_pte + 16,
					  priv->pt.redirect);
				fl_pte += 16;
			} else if (chunk_size == SZ_1M) {
				ret = fl_1m(fl_pte, pa, pgprot1m);
				if (ret)
					goto fail;
				clean_pte(fl_pte, fl_pte + 1,
					  priv->pt.redirect);
				fl_pte++;
			}

			offset += chunk_size;
			chunk_offset += chunk_size;
			va += chunk_size;
			pa += chunk_size;

			if (chunk_offset >= sg->length && offset < len) {
				chunk_offset = 0;
				sg = sg_next(sg);
				pa = get_phys_addr(sg);
			}
			continue;
		}
		/* for 4K or 64K, make sure there is a second level table */
		if (*fl_pte == 0) {
			if (!make_second_level(priv, fl_pte)) {
				ret = -ENOMEM;
				goto fail;
			}
		}
		if (!(*fl_pte & FL_TYPE_TABLE)) {
			ret = -EBUSY;
			goto fail;
		}
		sl_table = __va(((*fl_pte) & FL_BASE_MASK));
		sl_offset = SL_OFFSET(va);
		/* Keep track of initial position so we
		 * don't clean more than we have to
		 */
		sl_start = sl_offset;

		/* Build the 2nd level page table */
		while (offset < len && sl_offset < NUM_SL_PTE) {

			/* Map a large 64K page if the chunk is large enough and
			 * the pa and va are aligned
			 */

			if (is_fully_aligned(va, pa, sg->length - chunk_offset,
					     SZ_64K))
				chunk_size = SZ_64K;
			else
				chunk_size = SZ_4K;

			if (chunk_size == SZ_4K) {
				sl_4k(&sl_table[sl_offset], pa, pgprot4k);
				sl_offset++;
			} else {
				BUG_ON(sl_offset + 16 > NUM_SL_PTE);
				sl_64k(&sl_table[sl_offset], pa, pgprot64k);
				sl_offset += 16;
			}


			offset += chunk_size;
			chunk_offset += chunk_size;
			va += chunk_size;
			pa += chunk_size;

			if (chunk_offset >= sg->length && offset < len) {
				chunk_offset = 0;
				sg = sg_next(sg);
				pa = get_phys_addr(sg);
			}
		}

		clean_pte(sl_table + sl_start, sl_table + sl_offset,
				priv->pt.redirect);

		fl_pte++;
		sl_offset = 0;
	}
	__flush_iotlb(domain);
fail:
	mutex_unlock(&msm_iommu_lock);
	if (ret && offset > 0)
		msm_iommu_unmap_range(domain, start_va, offset);
	return ret;
}


static int msm_iommu_unmap_range(struct iommu_domain *domain, unsigned long va,
				 size_t len)
{
	unsigned int offset = 0;
	u32 *fl_table;
	u32 *fl_pte;
	u32 fl_offset;
	u32 *sl_table;
	u32 sl_start, sl_end;
	int used, i;
	struct msm_iommu_priv *priv;

	mutex_lock(&msm_iommu_lock);

	BUG_ON(len & (SZ_4K - 1));

	priv = domain->priv;
	fl_table = priv->pt.fl_table;

	fl_offset = FL_OFFSET(va);	/* Upper 12 bits */
	fl_pte = fl_table + fl_offset;	/* int pointers, 4 bytes */

	while (offset < len) {
		if (*fl_pte & FL_TYPE_TABLE) {
			sl_start = SL_OFFSET(va);
			sl_table =  __va(((*fl_pte) & FL_BASE_MASK));
			sl_end = ((len - offset) / SZ_4K) + sl_start;

			if (sl_end > NUM_SL_PTE)
				sl_end = NUM_SL_PTE;

			memset(sl_table + sl_start, 0, (sl_end - sl_start) * 4);
			clean_pte(sl_table + sl_start, sl_table + sl_end,
					priv->pt.redirect);

			offset += (sl_end - sl_start) * SZ_4K;
			va += (sl_end - sl_start) * SZ_4K;

			/* Unmap and free the 2nd level table if all mappings
			 * in it were removed. This saves memory, but the table
			 * will need to be re-allocated the next time someone
			 * tries to map these VAs.
			 */
			used = 0;

			/* If we just unmapped the whole table, don't bother
			 * seeing if there are still used entries left.
			 */
			if (sl_end - sl_start != NUM_SL_PTE)
				for (i = 0; i < NUM_SL_PTE; i++)
					if (sl_table[i]) {
						used = 1;
						break;
					}
			if (!used) {
				free_page((unsigned long)sl_table);
				*fl_pte = 0;

				clean_pte(fl_pte, fl_pte + 1,
					  priv->pt.redirect);
			}

			sl_start = 0;
		} else {
			*fl_pte = 0;
			clean_pte(fl_pte, fl_pte + 1, priv->pt.redirect);
			va += SZ_1M;
			offset += SZ_1M;
			sl_start = 0;
		}
		fl_pte++;
	}

	__flush_iotlb(domain);
	mutex_unlock(&msm_iommu_lock);
	return 0;
}

static phys_addr_t msm_iommu_iova_to_phys(struct iommu_domain *domain,
					  dma_addr_t va)
{
	struct msm_iommu_priv *priv;
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
	if (ret)
		goto fail;

	msm_iommu_remote_spin_lock(iommu_drvdata->needs_rem_spinlock);

	SET_V2PPR(base, ctx, va & V2Pxx_VA);

	mb();
	par = GET_PAR(base, ctx);

	/* We are dealing with a supersection */
	if (GET_NOFAULT_SS(base, ctx))
		ret = (par & 0xFF000000) | (va & 0x00FFFFFF);
	else	/* Upper 20 bits from PAR, lower 12 from VA */
		ret = (par & 0xFFFFF000) | (va & 0x00000FFF);

	if (GET_FAULT(base, ctx))
		ret = 0;

	msm_iommu_remote_spin_unlock(iommu_drvdata->needs_rem_spinlock);

	__disable_clocks(iommu_drvdata);
fail:
	mutex_unlock(&msm_iommu_lock);
	return ret;
}

static int msm_iommu_domain_has_cap(struct iommu_domain *domain,
				    unsigned long cap)
{
	return 0;
}

static void __print_ctx_regs(void __iomem *base, int ctx)
{
	unsigned int fsr = GET_FSR(base, ctx);
	pr_err("FAR    = %08x    PAR    = %08x\n",
	       GET_FAR(base, ctx), GET_PAR(base, ctx));
	pr_err("FSR    = %08x [%s%s%s%s%s%s%s%s%s%s]\n", fsr,
			(fsr & 0x02) ? "TF " : "",
			(fsr & 0x04) ? "AFF " : "",
			(fsr & 0x08) ? "APF " : "",
			(fsr & 0x10) ? "TLBMF " : "",
			(fsr & 0x20) ? "HTWDEEF " : "",
			(fsr & 0x40) ? "HTWSEEF " : "",
			(fsr & 0x80) ? "MHF " : "",
			(fsr & 0x10000) ? "SL " : "",
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

irqreturn_t msm_iommu_fault_handler(int irq, void *dev_id)
{
	struct msm_iommu_ctx_drvdata *ctx_drvdata = dev_id;
	struct msm_iommu_drvdata *drvdata;
	void __iomem *base;
	unsigned int fsr, num;
	int ret;

	mutex_lock(&msm_iommu_lock);
	BUG_ON(!ctx_drvdata);

	drvdata = dev_get_drvdata(ctx_drvdata->pdev->dev.parent);
	BUG_ON(!drvdata);

	base = drvdata->base;
	num = ctx_drvdata->num;

	ret = __enable_clocks(drvdata);
	if (ret)
		goto fail;

	msm_iommu_remote_spin_lock(drvdata->needs_rem_spinlock);

	fsr = GET_FSR(base, num);

	if (fsr) {
		if (!ctx_drvdata->attached_domain) {
			pr_err("Bad domain in interrupt handler\n");
			ret = -ENOSYS;
		} else
			ret = report_iommu_fault(ctx_drvdata->attached_domain,
						&ctx_drvdata->pdev->dev,
						GET_FAR(base, num), 0);

		if (ret == -ENOSYS) {
			pr_err("Unexpected IOMMU page fault!\n");
			pr_err("name    = %s\n", drvdata->name);
			pr_err("context = %s (%d)\n", ctx_drvdata->name, num);
			pr_err("Interesting registers:\n");
			__print_ctx_regs(base, num);
		}

		SET_FSR(base, num, fsr);
		/*
		 * Only resume fetches if the registered fault handler
		 * allows it
		 */
		if (ret != -EBUSY)
			SET_RESUME(base, num, 1);

		ret = IRQ_HANDLED;
	} else
		ret = IRQ_NONE;

	msm_iommu_remote_spin_unlock(drvdata->needs_rem_spinlock);

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

#ifdef CONFIG_IOMMU_PGTABLES_L2
static void __do_set_redirect(struct iommu_domain *domain, void *data)
{
	struct msm_iommu_priv *priv;
	int *no_redirect = data;

	mutex_lock(&msm_iommu_lock);
	priv = domain->priv;
	priv->pt.redirect = !(*no_redirect);
	mutex_unlock(&msm_iommu_lock);
}

static void __do_get_redirect(struct iommu_domain *domain, void *data)
{
	struct msm_iommu_priv *priv;
	int *no_redirect = data;

	mutex_lock(&msm_iommu_lock);
	priv = domain->priv;
	*no_redirect = !priv->pt.redirect;
	mutex_unlock(&msm_iommu_lock);
}

#else

static void __do_set_redirect(struct iommu_domain *domain, void *data)
{
}

static void __do_get_redirect(struct iommu_domain *domain, void *data)
{
}
#endif

static int msm_iommu_domain_set_attr(struct iommu_domain *domain,
				enum iommu_attr attr, void *data)
{
	switch (attr) {
	case DOMAIN_ATTR_COHERENT_HTW_DISABLE:
		__do_set_redirect(domain, data);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int msm_iommu_domain_get_attr(struct iommu_domain *domain,
				enum iommu_attr attr, void *data)
{
	switch (attr) {
	case DOMAIN_ATTR_COHERENT_HTW_DISABLE:
		__do_get_redirect(domain, data);
		break;
	default:
		return -EINVAL;
	}
	return 0;
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
	.domain_set_attr = msm_iommu_domain_set_attr,
	.domain_get_attr = msm_iommu_domain_get_attr,
};

static int __init get_tex_class(int icp, int ocp, int mt, int nos)
{
	int i = 0;
	unsigned int prrr = 0;
	unsigned int nmrr = 0;
	int c_icp, c_ocp, c_mt, c_nos;

	prrr = msm_iommu_get_prrr();
	nmrr = msm_iommu_get_nmrr();

	for (i = 0; i < NUM_TEX_CLASS; i++) {
		c_nos = PRRR_NOS(prrr, i);
		c_mt = PRRR_MT(prrr, i);
		c_icp = NMRR_ICP(nmrr, i);
		c_ocp = NMRR_OCP(nmrr, i);

		if (icp == c_icp && ocp == c_ocp && c_mt == mt && c_nos == nos)
			return i;
	}

	return -ENODEV;
}

static void __init setup_iommu_tex_classes(void)
{
	msm_iommu_tex_class[MSM_IOMMU_ATTR_NONCACHED] =
			get_tex_class(CP_NONCACHED, CP_NONCACHED, MT_NORMAL, 1);

	msm_iommu_tex_class[MSM_IOMMU_ATTR_CACHED_WB_WA] =
			get_tex_class(CP_WB_WA, CP_WB_WA, MT_NORMAL, 1);

	msm_iommu_tex_class[MSM_IOMMU_ATTR_CACHED_WB_NWA] =
			get_tex_class(CP_WB_NWA, CP_WB_NWA, MT_NORMAL, 1);

	msm_iommu_tex_class[MSM_IOMMU_ATTR_CACHED_WT] =
			get_tex_class(CP_WT, CP_WT, MT_NORMAL, 1);
}

static int __init msm_iommu_init(void)
{
	if (!msm_soc_version_supports_iommu_v0())
		return -ENODEV;

	msm_iommu_lock_initialize();

	setup_iommu_tex_classes();
	bus_set_iommu(&platform_bus_type, &msm_iommu_ops);
	return 0;
}

subsys_initcall(msm_iommu_init);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Stepan Moskovchenko <stepanm@codeaurora.org>");
