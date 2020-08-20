/* Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
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
#include <linux/module.h>
#include <linux/device.h>
#include <linux/iommu.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/of_platform.h>
#include <linux/msm-bus.h>
#include <linux/msm-bus-board.h>
#include <linux/pm_opp.h>
#include <soc/qcom/cmd-db.h>
#include <dt-bindings/regulator/qcom,rpmh-regulator.h>

#include "kgsl_device.h"
#include "kgsl_gmu.h"
#include "kgsl_hfi.h"
#include "adreno.h"

#undef MODULE_PARAM_PREFIX
#define MODULE_PARAM_PREFIX "kgsl."

static bool noacd;
module_param(noacd, bool, 0444);
MODULE_PARM_DESC(noacd, "Disable GPU ACD");

#define GMU_CONTEXT_USER		0
#define GMU_CONTEXT_KERNEL		1
#define GMU_KERNEL_ENTRIES		16

#define GMU_CM3_CFG_NONMASKINTR_SHIFT    9

struct gmu_iommu_context {
	const char *name;
	struct device *dev;
	struct iommu_domain *domain;
};

#define DUMPMEM_SIZE SZ_16K

#define DUMMY_SIZE   SZ_4K

/* Define target specific GMU VMA configurations */
static const struct gmu_vma_entry {
	unsigned int start;
	unsigned int size;
} gmu_vma[] = {
	[GMU_ITCM] = { .start = 0x00000, .size = SZ_16K },
	[GMU_ICACHE] = { .start = 0x04000, .size = (SZ_256K - SZ_16K) },
	[GMU_DTCM] = { .start = 0x40000, .size = SZ_16K },
	[GMU_DCACHE] = { .start = 0x44000, .size = (SZ_256K - SZ_16K) },
	[GMU_NONCACHED_KERNEL] = { .start = 0x60000000, .size = SZ_512M },
	[GMU_NONCACHED_USER] = { .start = 0x80000000, .size = SZ_1G },
};

struct gmu_iommu_context gmu_ctx[] = {
	[GMU_CONTEXT_USER] = { .name = "gmu_user" },
	[GMU_CONTEXT_KERNEL] = { .name = "gmu_kernel" }
};

/*
 * There are a few static memory buffers that are allocated and mapped at boot
 * time for GMU to function. The buffers are permanent (not freed) after
 * GPU boot. The size of the buffers are constant and not expected to change.
 *
 * We define an array and a simple allocator to keep track of the currently
 * active SMMU entries of GMU kernel mode context. Each entry is assigned
 * a unique address inside GMU kernel mode address range. The addresses
 * are assigned sequentially and aligned to 1MB each.
 *
 */
static struct gmu_memdesc gmu_kmem_entries[GMU_KERNEL_ENTRIES];
static unsigned long gmu_kmem_bitmap;
static unsigned int num_uncached_entries;

static void gmu_snapshot(struct kgsl_device *device);
static void gmu_remove(struct kgsl_device *device);

unsigned int gmu_get_memtype_base(struct gmu_device *gmu,
		enum gmu_mem_type type)
{
	return gmu_vma[type].start;
}

static int _gmu_iommu_fault_handler(struct device *dev,
		unsigned long addr, int flags, const char *name)
{
	char *fault_type = "unknown";

	if (flags & IOMMU_FAULT_TRANSLATION)
		fault_type = "translation";
	else if (flags & IOMMU_FAULT_PERMISSION)
		fault_type = "permission";
	else if (flags & IOMMU_FAULT_EXTERNAL)
		fault_type = "external";
	else if (flags & IOMMU_FAULT_TRANSACTION_STALLED)
		fault_type = "transaction stalled";

	dev_err(dev, "GMU fault addr = %lX, context=%s (%s %s fault)\n",
			addr, name,
			(flags & IOMMU_FAULT_WRITE) ? "write" : "read",
			fault_type);

	return 0;
}

static int gmu_kernel_fault_handler(struct iommu_domain *domain,
		struct device *dev, unsigned long addr, int flags, void *token)
{
	return _gmu_iommu_fault_handler(dev, addr, flags, "gmu_kernel");
}

static int gmu_user_fault_handler(struct iommu_domain *domain,
		struct device *dev, unsigned long addr, int flags, void *token)
{
	return _gmu_iommu_fault_handler(dev, addr, flags, "gmu_user");
}

static void free_gmu_mem(struct gmu_device *gmu,
		struct gmu_memdesc *md)
{
	/* Free GMU image memory */
	if (md->hostptr)
		dma_free_attrs(&gmu->pdev->dev, (size_t) md->size,
				(void *)md->hostptr, md->physaddr, 0);
	memset(md, 0, sizeof(*md));
}

static int alloc_and_map(struct gmu_device *gmu, unsigned int ctx_id,
		struct gmu_memdesc *md, unsigned int attrs)
{
	int ret;
	struct iommu_domain *domain;

	if (md->mem_type == GMU_ITCM || md->mem_type == GMU_DTCM)
		return 0;

	domain = gmu_ctx[ctx_id].domain;

	md->hostptr = dma_alloc_attrs(&gmu->pdev->dev, (size_t) md->size,
		&md->physaddr, GFP_KERNEL, 0);

	if (md->hostptr == NULL)
		return -ENOMEM;

	ret = iommu_map(domain, md->gmuaddr, md->physaddr, md->size, attrs);

	if (ret) {
		dev_err(&gmu->pdev->dev,
				"gmu map err: gaddr=0x%016llX, paddr=0x%pa\n",
				md->gmuaddr, &(md->physaddr));
		free_gmu_mem(gmu, md);
	}

	return ret;
}

struct gmu_memdesc *gmu_get_memdesc(unsigned int addr, unsigned int size)
{
	int i;
	struct gmu_memdesc *mem;

	for (i = 0; i < GMU_KERNEL_ENTRIES; i++) {
		if (!test_bit(i, &gmu_kmem_bitmap))
			continue;

		mem = &gmu_kmem_entries[i];

		if (addr >= mem->gmuaddr &&
				(addr + size < mem->gmuaddr + mem->size))
			return mem;
	}

	return NULL;
}

/*
 * allocate_gmu_kmem() - allocates and maps uncached GMU kernel shared memory
 * @gmu: Pointer to GMU device
 * @size: Requested size
 * @attrs: IOMMU mapping attributes
 */
static struct gmu_memdesc *allocate_gmu_kmem(struct gmu_device *gmu,
		enum gmu_mem_type mem_type, unsigned int size,
		unsigned int attrs)
{
	struct gmu_memdesc *md;
	int ret = 0;
	int entry_idx = find_first_zero_bit(
			&gmu_kmem_bitmap, GMU_KERNEL_ENTRIES);

	if (entry_idx >= GMU_KERNEL_ENTRIES) {
		dev_err(&gmu->pdev->dev,
				"Ran out of GMU kernel mempool slots\n");
		return ERR_PTR(-EINVAL);
	}

	switch (mem_type) {
	case GMU_NONCACHED_KERNEL:
		size = PAGE_ALIGN(size);
		if (size > SZ_1M || size == 0) {
			dev_err(&gmu->pdev->dev,
					"Invalid uncached GMU memory req %d\n",
					size);
			return ERR_PTR(-EINVAL);
		}

		md = &gmu_kmem_entries[entry_idx];
		md->gmuaddr = gmu_vma[mem_type].start +
			(num_uncached_entries * SZ_1M);
		set_bit(entry_idx, &gmu_kmem_bitmap);
		md->size = size;
		md->mem_type = mem_type;
		break;

	case GMU_DCACHE:
		md = &gmu_kmem_entries[entry_idx];
		md->gmuaddr = gmu_vma[mem_type].start;
		set_bit(entry_idx, &gmu_kmem_bitmap);
		md->size = size;
		md->mem_type = mem_type;
		break;

	case GMU_ICACHE:
		md = &gmu_kmem_entries[entry_idx];
		md->gmuaddr = gmu_vma[mem_type].start;
		set_bit(entry_idx, &gmu_kmem_bitmap);
		md->size = size;
		md->mem_type = mem_type;
		break;

	case GMU_ITCM:
		md = &gmu_kmem_entries[entry_idx];
		md->gmuaddr = gmu_vma[mem_type].start;
		set_bit(entry_idx, &gmu_kmem_bitmap);
		md->size = size;
		md->mem_type = mem_type;
		break;

	case GMU_DTCM:
		md = &gmu_kmem_entries[entry_idx];
		md->gmuaddr = gmu_vma[mem_type].start;
		set_bit(entry_idx, &gmu_kmem_bitmap);
		md->size = size;
		md->mem_type = mem_type;
		break;

	default:
		dev_err(&gmu->pdev->dev,
				"Invalid memory type (%d) requested\n",
				mem_type);
			return ERR_PTR(-EINVAL);
	};

	ret = alloc_and_map(gmu, GMU_CONTEXT_KERNEL, md, attrs);
	if (ret) {
		clear_bit(entry_idx, &gmu_kmem_bitmap);
		md->gmuaddr = 0;
		return ERR_PTR(ret);
	}

	if (mem_type == GMU_NONCACHED_KERNEL)
		num_uncached_entries++;

	return md;
}

static int gmu_iommu_cb_probe(struct gmu_device *gmu,
		struct gmu_iommu_context *ctx,
		struct device_node *node)
{
	struct platform_device *pdev = of_find_device_by_node(node);
	struct device *dev;
	int ret;

	dev = &pdev->dev;
	of_dma_configure(dev, node);

	ctx->dev = dev;
	ctx->domain = iommu_domain_alloc(&platform_bus_type);
	if (ctx->domain == NULL) {
		dev_err(&gmu->pdev->dev, "gmu iommu fail to alloc %s domain\n",
			ctx->name);
		return -ENODEV;
	}

	ret = iommu_attach_device(ctx->domain, dev);
	if (ret) {
		dev_err(&gmu->pdev->dev, "gmu iommu fail to attach %s device\n",
			ctx->name);
		iommu_domain_free(ctx->domain);
		ctx->domain = NULL;
	}

	return ret;
}

static struct {
	const char *compatible;
	int index;
	iommu_fault_handler_t hdlr;
} cbs[] = {
	{ "qcom,smmu-gmu-user-cb",
		GMU_CONTEXT_USER,
		gmu_user_fault_handler,
	},
	{ "qcom,smmu-gmu-kernel-cb",
		GMU_CONTEXT_KERNEL,
		gmu_kernel_fault_handler,
	},
};

/*
 * gmu_iommu_init() - probe IOMMU context banks used by GMU
 * and attach GMU device
 * @gmu: Pointer to GMU device
 * @node: Pointer to GMU device node
 */
static int gmu_iommu_init(struct gmu_device *gmu, struct device_node *node)
{
	struct device_node *child;
	struct gmu_iommu_context *ctx = NULL;
	int ret, i;

	of_platform_populate(node, NULL, NULL, &gmu->pdev->dev);

	for (i = 0; i < ARRAY_SIZE(cbs); i++) {
		child = of_find_compatible_node(node, NULL, cbs[i].compatible);
		if (child) {
			ctx = &gmu_ctx[cbs[i].index];
			ret = gmu_iommu_cb_probe(gmu, ctx, child);
			if (ret)
				return ret;
			iommu_set_fault_handler(ctx->domain,
					cbs[i].hdlr, ctx);
			}
		}

	for (i = 0; i < ARRAY_SIZE(gmu_ctx); i++) {
		if (gmu_ctx[i].domain == NULL) {
			dev_err(&gmu->pdev->dev,
				"Missing GMU %s context bank node\n",
				gmu_ctx[i].name);
			return -EINVAL;
		}
	}

	return 0;
}

/*
 * gmu_kmem_close() - free all kernel memory allocated for GMU and detach GMU
 * from IOMMU context banks.
 * @gmu: Pointer to GMU device
 */
static void gmu_kmem_close(struct gmu_device *gmu)
{
	int i;
	struct gmu_memdesc *md;
	struct gmu_iommu_context *ctx = &gmu_ctx[GMU_CONTEXT_KERNEL];

	gmu->hfi_mem = NULL;
	gmu->persist_mem = NULL;
	gmu->icache_mem = NULL;
	gmu->dcache_mem = NULL;
	gmu->dump_mem = NULL;
	gmu->gmu_log = NULL;

	if (!ctx->domain)
		return;

	/* Unmap and free all memories in GMU kernel memory pool */
	for (i = 0; i < GMU_KERNEL_ENTRIES; i++) {
		if (!test_bit(i, &gmu_kmem_bitmap))
			continue;

		md = &gmu_kmem_entries[i];

		if (md->gmuaddr && md->mem_type != GMU_ITCM &&
				md->mem_type != GMU_DTCM)
			iommu_unmap(ctx->domain, md->gmuaddr, md->size);

		free_gmu_mem(gmu, md);

		clear_bit(i, &gmu_kmem_bitmap);
	}

	/* Detach the device from SMMU context bank */
	iommu_detach_device(ctx->domain, ctx->dev);

	/* free kernel mem context */
	iommu_domain_free(ctx->domain);
	ctx->domain = NULL;
}

static void gmu_memory_close(struct gmu_device *gmu)
{
	struct gmu_iommu_context *ctx = &gmu_ctx[GMU_CONTEXT_USER];

	gmu_kmem_close(gmu);

	if (ctx->domain) {
		/* Detach the device from SMMU context bank */
		iommu_detach_device(ctx->domain, ctx->dev);

		/* Free user memory context */
		iommu_domain_free(ctx->domain);
		ctx->domain = NULL;
	}
}

/*
 * gmu_memory_probe() - probe GMU IOMMU context banks and allocate memory
 * to share with GMU in kernel mode.
 * @device: Pointer to KGSL device
 * @gmu: Pointer to GMU device
 * @node: Pointer to GMU device node
 */
static int gmu_memory_probe(struct kgsl_device *device,
		struct gmu_device *gmu, struct device_node *node)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct gmu_memdesc *md;
	int ret;

	ret = gmu_iommu_init(gmu, node);
	if (ret)
		return ret;

	/* Reserve a memdesc for ITCM. No actually memory allocated */
	md = allocate_gmu_kmem(gmu, GMU_ITCM, gmu_vma[GMU_ITCM].size, 0);
	if (IS_ERR(md)) {
		ret = PTR_ERR(md);
		goto err_ret;
	}

	/* Reserve a memdesc for DTCM. No actually memory allocated */
	md = allocate_gmu_kmem(gmu, GMU_DTCM, gmu_vma[GMU_DTCM].size, 0);
	if (IS_ERR(md)) {
		ret = PTR_ERR(md);
		goto err_ret;
	}

	/* Allocates & maps memory for WB DUMMY PAGE */
	/* Must be the first alloc */
	if (IS_ERR_OR_NULL(gmu->persist_mem))
		gmu->persist_mem = allocate_gmu_kmem(gmu, GMU_NONCACHED_KERNEL,
				DUMMY_SIZE,
				(IOMMU_READ | IOMMU_WRITE | IOMMU_PRIV));
	if (IS_ERR(gmu->persist_mem)) {
		ret = PTR_ERR(gmu->persist_mem);
		goto err_ret;
	}

	/* Allocates & maps memory for DCACHE */
	if (IS_ERR_OR_NULL(gmu->dcache_mem))
		gmu->dcache_mem = allocate_gmu_kmem(gmu, GMU_DCACHE,
				gmu_vma[GMU_DCACHE].size,
				(IOMMU_READ | IOMMU_WRITE | IOMMU_PRIV));
	if (IS_ERR(gmu->dcache_mem)) {
		ret = PTR_ERR(gmu->dcache_mem);
		goto err_ret;
	}

	/* Allocates & maps memory for ICACHE */
	if (IS_ERR_OR_NULL(gmu->icache_mem))
		gmu->icache_mem = allocate_gmu_kmem(gmu, GMU_ICACHE,
				gmu_vma[GMU_ICACHE].size,
				(IOMMU_READ | IOMMU_WRITE | IOMMU_PRIV));
	if (IS_ERR(gmu->icache_mem)) {
		ret = PTR_ERR(gmu->icache_mem);
		goto err_ret;
	}

	/* Allocates & maps memory for HFI */
	if (IS_ERR_OR_NULL(gmu->hfi_mem))
		gmu->hfi_mem = allocate_gmu_kmem(gmu, GMU_NONCACHED_KERNEL,
				HFIMEM_SIZE, (IOMMU_READ | IOMMU_WRITE));
	if (IS_ERR(gmu->hfi_mem)) {
		ret = PTR_ERR(gmu->hfi_mem);
		goto err_ret;
	}

	/* Allocates & maps GMU crash dump memory */
	if (adreno_is_a630(adreno_dev) || adreno_is_a615_family(adreno_dev)) {
		if (IS_ERR_OR_NULL(gmu->dump_mem))
			gmu->dump_mem = allocate_gmu_kmem(gmu,
					GMU_NONCACHED_KERNEL,
					DUMPMEM_SIZE,
					(IOMMU_READ | IOMMU_WRITE));
		if (IS_ERR(gmu->dump_mem)) {
			ret = PTR_ERR(gmu->dump_mem);
			goto err_ret;
		}
	}

	/* GMU master log */
	if (IS_ERR_OR_NULL(gmu->gmu_log))
		gmu->gmu_log = allocate_gmu_kmem(gmu, GMU_NONCACHED_KERNEL,
				LOGMEM_SIZE,
				(IOMMU_READ | IOMMU_WRITE | IOMMU_PRIV));
	if (IS_ERR(gmu->gmu_log)) {
		ret = PTR_ERR(gmu->gmu_log);
		goto err_ret;
	}

	return 0;
err_ret:
	gmu_memory_close(gmu);
	return ret;
}

/*
 * gmu_dcvs_set() - request GMU to change GPU frequency and/or bandwidth.
 * @device: Pointer to the device
 * @gpu_pwrlevel: index to GPU DCVS table used by KGSL
 * @bus_level: index to GPU bus table used by KGSL
 *
 * The function converts GPU power level and bus level index used by KGSL
 * to index being used by GMU/RPMh.
 */
static int gmu_dcvs_set(struct kgsl_device *device,
		unsigned int gpu_pwrlevel, unsigned int bus_level)
{
	int ret = 0;
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct gmu_dev_ops *gmu_dev_ops = GMU_DEVICE_OPS(device);
	struct hfi_gx_bw_perf_vote_cmd req = {
		.ack_type = DCVS_ACK_BLOCK,
		.freq = INVALID_DCVS_IDX,
		.bw = INVALID_DCVS_IDX,
	};


	/* If GMU has not been started, save it */
	if (!test_bit(GMU_HFI_ON, &device->gmu_core.flags)) {
		/* store clock change request */
		set_bit(GMU_DCVS_REPLAY, &device->gmu_core.flags);
		return 0;
	}

	/* Do not set to XO and lower GPU clock vote from GMU */
	if ((gpu_pwrlevel != INVALID_DCVS_IDX) &&
			(gpu_pwrlevel >= gmu->num_gpupwrlevels - 1))
		return -EINVAL;

	if (gpu_pwrlevel < gmu->num_gpupwrlevels - 1)
		req.freq = gmu->num_gpupwrlevels - gpu_pwrlevel - 1;

	if (bus_level < gmu->num_bwlevels && bus_level > 0)
		req.bw = bus_level;

	/* GMU will vote for slumber levels through the sleep sequence */
	if ((req.freq == INVALID_DCVS_IDX) &&
		(req.bw == INVALID_DCVS_IDX)) {
		clear_bit(GMU_DCVS_REPLAY, &device->gmu_core.flags);
		return 0;
	}

	if (ADRENO_QUIRK(adreno_dev, ADRENO_QUIRK_HFI_USE_REG))
		ret = gmu_dev_ops->rpmh_gpu_pwrctrl(adreno_dev,
			GMU_DCVS_NOHFI, req.freq, req.bw);
	else if (test_bit(GMU_HFI_ON, &device->gmu_core.flags))
		ret = hfi_send_req(gmu, H2F_MSG_GX_BW_PERF_VOTE, &req);

	if (ret) {
		dev_err_ratelimited(&gmu->pdev->dev,
			"Failed to set GPU perf idx %d, bw idx %d\n",
			req.freq, req.bw);
		gmu_snapshot(device);
	}

	/* indicate actual clock change */
	clear_bit(GMU_DCVS_REPLAY, &device->gmu_core.flags);
	return ret;
}

struct rpmh_arc_vals {
	unsigned int num;
	uint16_t val[MAX_GX_LEVELS];
};

static const char gfx_res_id[] = "gfx.lvl";
static const char cx_res_id[] = "cx.lvl";
static const char mx_res_id[] = "mx.lvl";

enum rpmh_vote_type {
	GPU_ARC_VOTE = 0,
	GMU_ARC_VOTE,
	INVALID_ARC_VOTE,
};

static const char debug_strs[][8] = {
	[GPU_ARC_VOTE] = "gpu",
	[GMU_ARC_VOTE] = "gmu",
};

/*
 * rpmh_arc_cmds() - query RPMh command database for GX/CX/MX rail
 * VLVL tables. The index of table will be used by GMU to vote rail
 * voltage.
 *
 * @gmu: Pointer to GMU device
 * @arc: Pointer to RPMh rail controller (ARC) voltage table
 * @res_id: Pointer to 8 char array that contains rail name
 */
static int rpmh_arc_cmds(struct gmu_device *gmu,
		struct rpmh_arc_vals *arc, const char *res_id)
{
	unsigned int len;

	len = cmd_db_get_aux_data_len(res_id);
	if (len == 0)
		return -EINVAL;

	if (len > (MAX_GX_LEVELS << 1)) {
		dev_err(&gmu->pdev->dev,
			"gfx cmddb size %d larger than alloc buf %d of %s\n",
			len, (MAX_GX_LEVELS << 1), res_id);
		return -EINVAL;
	}

	cmd_db_get_aux_data(res_id, (uint8_t *)arc->val, len);

	/*
	 * cmd_db_get_aux_data() gives us a zero-padded table of
	 * size len that contains the arc values. To determine the
	 * number of arc values, we loop through the table and count
	 * them until we get to the end of the buffer or hit the
	 * zero padding.
	 */
	for (arc->num = 1; arc->num < (len >> 1); arc->num++) {
		if (arc->val[arc->num - 1] != 0 &&  arc->val[arc->num] == 0)
			break;
	}

	return 0;
}

/*
 * setup_volt_dependency_tbl() - set up GX->MX or CX->MX rail voltage
 * dependencies. Second rail voltage shall be equal to or higher than
 * primary rail voltage. VLVL table index was used by RPMh for PMIC
 * voltage setting.
 * @votes: Pointer to a ARC vote descriptor
 * @pri_rail: Pointer to primary power rail VLVL table
 * @sec_rail: Pointer to second/dependent power rail VLVL table
 * @vlvl: Pointer to VLVL table being used by GPU or GMU driver, a subset
 *	of pri_rail VLVL table
 * @num_entries: Valid number of entries in table pointed by "vlvl" parameter
 */
static int setup_volt_dependency_tbl(uint32_t *votes,
		struct rpmh_arc_vals *pri_rail, struct rpmh_arc_vals *sec_rail,
		unsigned int *vlvl, unsigned int num_entries)
{
	int i, j, k;
	uint16_t cur_vlvl;
	bool found_match;

	/* i tracks current KGSL GPU frequency table entry
	 * j tracks secondary rail voltage table entry
	 * k tracks primary rail voltage table entry
	 */
	for (i = 0; i < num_entries; i++) {
		found_match = false;

		/* Look for a primary rail voltage that matches a VLVL level */
		for (k = 0; k < pri_rail->num; k++) {
			if (pri_rail->val[k] >= vlvl[i]) {
				cur_vlvl = pri_rail->val[k];
				found_match = true;
				break;
			}
		}

		/* If we did not find a matching VLVL level then abort */
		if (!found_match)
			return -EINVAL;

		/*
		 * Look for a secondary rail index whose VLVL value
		 * is greater than or equal to the VLVL value of the
		 * corresponding index of the primary rail
		 */
		for (j = 0; j < sec_rail->num; j++) {
			if (sec_rail->val[j] >= cur_vlvl ||
					j + 1 == sec_rail->num)
				break;
		}

		if (j == sec_rail->num)
			j = 0;

		votes[i] = ARC_VOTE_SET(k, j, cur_vlvl);
	}

	return 0;
}

/*
 * rpmh_arc_votes_init() - initialized RPMh votes needed for rails voltage
 * scaling by GMU.
 * @device: Pointer to KGSL device
 * @gmu: Pointer to GMU device
 * @pri_rail: Pointer to primary power rail VLVL table
 * @sec_rail: Pointer to second/dependent power rail VLVL table
 *	of pri_rail VLVL table
 * @type: the type of the primary rail, GPU or GMU
 */
static int rpmh_arc_votes_init(struct kgsl_device *device,
		struct gmu_device *gmu, struct rpmh_arc_vals *pri_rail,
		struct rpmh_arc_vals *sec_rail, unsigned int type)
{
	struct device *dev;
	unsigned int num_freqs;
	uint32_t *votes;
	unsigned int vlvl_tbl[MAX_GX_LEVELS];
	unsigned int *freq_tbl;
	int i, ret;
	struct dev_pm_opp *opp;

	if (type == GPU_ARC_VOTE) {
		num_freqs = gmu->num_gpupwrlevels;
		votes = gmu->rpmh_votes.gx_votes;
		freq_tbl = gmu->gpu_freqs;
		dev = &device->pdev->dev;
	} else if (type == GMU_ARC_VOTE) {
		num_freqs = gmu->num_gmupwrlevels;
		votes = gmu->rpmh_votes.cx_votes;
		freq_tbl = gmu->gmu_freqs;
		dev = &gmu->pdev->dev;
	} else {
		return -EINVAL;
	}

	if (num_freqs > pri_rail->num) {
		dev_err(&gmu->pdev->dev,
			"%s defined more DCVS levels than RPMh can support\n",
			debug_strs[type]);
		return -EINVAL;
	}

	memset(vlvl_tbl, 0, sizeof(vlvl_tbl));
	for (i = 0; i < num_freqs; i++) {
		/* Hardcode VLVL for 0 because it is not registered in OPP */
		if (freq_tbl[i] == 0) {
			vlvl_tbl[i] = 0;
			continue;
		}

		/* Otherwise get the value from the OPP API */
		opp = dev_pm_opp_find_freq_exact(dev, freq_tbl[i], true);
		if (IS_ERR(opp)) {
			dev_err(&gmu->pdev->dev,
				"Failed to find opp freq %d of %s\n",
				freq_tbl[i], debug_strs[type]);
			return PTR_ERR(opp);
		}

		/* Values from OPP framework are offset by 1 */
		vlvl_tbl[i] = dev_pm_opp_get_voltage(opp)
				- RPMH_REGULATOR_LEVEL_OFFSET;
		dev_pm_opp_put(opp);
	}

	ret = setup_volt_dependency_tbl(votes,
			pri_rail, sec_rail, vlvl_tbl, num_freqs);

	if (ret)
		dev_err(&gmu->pdev->dev, "%s rail volt failed to match DT freqs\n",
				debug_strs[type]);

	return ret;
}

/*
 * build_rpmh_bw_votes() - build TCS commands to vote for bandwidth.
 * Each command sets frequency of a node along path to DDR or CNOC.
 * @rpmh_vote: Pointer to RPMh vote needed by GMU to set BW via RPMh
 * @num_usecases: Number of BW use cases (or BW levels)
 * @handle: Provided by bus driver. It contains TCS command sets for
 * all BW use cases of a bus client.
 */
static void build_rpmh_bw_votes(struct gmu_bw_votes *rpmh_vote,
		unsigned int num_usecases, struct msm_bus_tcs_handle handle)
{
	struct msm_bus_tcs_usecase *tmp;
	int i, j;

	for (i = 0; i < num_usecases; i++) {
		tmp = &handle.usecases[i];
		for (j = 0; j < tmp->num_cmds; j++) {
			if (!i) {
			/*
			 * Wait bitmask and TCS command addresses are
			 * same for all bw use cases. To save data volume
			 * exchanged between driver and GMU, only
			 * transfer bitmasks and TCS command addresses
			 * of first set of bw use case
			 */
				rpmh_vote->cmds_per_bw_vote = tmp->num_cmds;
				rpmh_vote->cmds_wait_bitmask =
						tmp->cmds[j].complete ?
						rpmh_vote->cmds_wait_bitmask
						| BIT(i)
						: rpmh_vote->cmds_wait_bitmask
						& (~BIT(i));
				rpmh_vote->cmd_addrs[j] = tmp->cmds[j].addr;
			}
			rpmh_vote->cmd_data[i][j] = tmp->cmds[j].data;
		}
	}
}

static void build_bwtable_cmd_cache(struct gmu_device *gmu)
{
	struct hfi_bwtable_cmd *cmd = &gmu->hfi.bwtbl_cmd;
	struct rpmh_votes_t *votes = &gmu->rpmh_votes;
	unsigned int i, j;

	cmd->hdr = 0xFFFFFFFF;
	cmd->bw_level_num = gmu->num_bwlevels;
	cmd->cnoc_cmds_num = votes->cnoc_votes.cmds_per_bw_vote;
	cmd->cnoc_wait_bitmask = votes->cnoc_votes.cmds_wait_bitmask;
	cmd->ddr_cmds_num = votes->ddr_votes.cmds_per_bw_vote;
	cmd->ddr_wait_bitmask = votes->ddr_votes.cmds_wait_bitmask;

	for (i = 0; i < cmd->ddr_cmds_num; i++)
		cmd->ddr_cmd_addrs[i] = votes->ddr_votes.cmd_addrs[i];

	for (i = 0; i < cmd->bw_level_num; i++)
		for (j = 0; j < cmd->ddr_cmds_num; j++)
			cmd->ddr_cmd_data[i][j] =
				votes->ddr_votes.cmd_data[i][j];

	for (i = 0; i < cmd->cnoc_cmds_num; i++)
		cmd->cnoc_cmd_addrs[i] =
			votes->cnoc_votes.cmd_addrs[i];

	for (i = 0; i < MAX_CNOC_LEVELS; i++)
		for (j = 0; j < cmd->cnoc_cmds_num; j++)
			cmd->cnoc_cmd_data[i][j] =
				votes->cnoc_votes.cmd_data[i][j];
}

static int gmu_acd_probe(struct gmu_device *gmu, struct device_node *node)
{
	struct hfi_acd_table_cmd *cmd = &gmu->hfi.acd_tbl_cmd;
	struct device_node *acd_node;

	acd_node = of_find_node_by_name(node, "qcom,gpu-acd-table");
	if (!acd_node)
		return -ENODEV;

	cmd->hdr = 0xFFFFFFFF;
	cmd->version = HFI_ACD_INIT_VERSION;
	cmd->enable_by_level = 0;
	cmd->stride = 0;
	cmd->num_levels = 0;

	of_property_read_u32(acd_node, "qcom,acd-stride", &cmd->stride);
	if (!cmd->stride || cmd->stride > MAX_ACD_STRIDE)
		return -EINVAL;

	of_property_read_u32(acd_node, "qcom,acd-num-levels", &cmd->num_levels);
	if (!cmd->num_levels || cmd->num_levels > MAX_ACD_NUM_LEVELS)
		return -EINVAL;

	of_property_read_u32(acd_node, "qcom,acd-enable-by-level",
			&cmd->enable_by_level);
	if (hweight32(cmd->enable_by_level) != cmd->num_levels)
		return -EINVAL;

	return of_property_read_u32_array(acd_node, "qcom,acd-data",
			cmd->data, cmd->stride * cmd->num_levels);
}

/*
 * gmu_bus_vote_init - initialized RPMh votes needed for bw scaling by GMU.
 * @gmu: Pointer to GMU device
 * @pwr: Pointer to KGSL power controller
 */
static int gmu_bus_vote_init(struct gmu_device *gmu, struct kgsl_pwrctrl *pwr)
{
	struct msm_bus_tcs_usecase *usecases;
	struct msm_bus_tcs_handle hdl;
	struct rpmh_votes_t *votes = &gmu->rpmh_votes;
	int ret;

	usecases  = kcalloc(gmu->num_bwlevels, sizeof(*usecases), GFP_KERNEL);
	if (!usecases)
		return -ENOMEM;

	hdl.num_usecases = gmu->num_bwlevels;
	hdl.usecases = usecases;

	/*
	 * Query TCS command set for each use case defined in GPU b/w table
	 */
	ret = msm_bus_scale_query_tcs_cmd_all(&hdl, gmu->pcl);
	if (ret)
		goto out;

	build_rpmh_bw_votes(&votes->ddr_votes, gmu->num_bwlevels, hdl);

	/*
	 *Query CNOC TCS command set for each use case defined in cnoc bw table
	 */
	ret = msm_bus_scale_query_tcs_cmd_all(&hdl, gmu->ccl);
	if (ret)
		goto out;

	build_rpmh_bw_votes(&votes->cnoc_votes, gmu->num_cnocbwlevels, hdl);

	build_bwtable_cmd_cache(gmu);

out:
	kfree(usecases);

	return ret;
}

static int gmu_rpmh_init(struct kgsl_device *device,
		struct gmu_device *gmu, struct kgsl_pwrctrl *pwr)
{
	struct rpmh_arc_vals gfx_arc, cx_arc, mx_arc;
	int ret;

	/* Initialize BW tables */
	ret = gmu_bus_vote_init(gmu, pwr);
	if (ret)
		return ret;

	/* Populate GPU and GMU frequency vote table */
	ret = rpmh_arc_cmds(gmu, &gfx_arc, gfx_res_id);
	if (ret)
		return ret;

	ret = rpmh_arc_cmds(gmu, &cx_arc, cx_res_id);
	if (ret)
		return ret;

	ret = rpmh_arc_cmds(gmu, &mx_arc, mx_res_id);
	if (ret)
		return ret;

	ret = rpmh_arc_votes_init(device, gmu, &gfx_arc, &mx_arc, GPU_ARC_VOTE);
	if (ret)
		return ret;

	return rpmh_arc_votes_init(device, gmu, &cx_arc, &mx_arc, GMU_ARC_VOTE);
}

static void send_nmi_to_gmu(struct adreno_device *adreno_dev)
{
	u32 val;

	/* Mask so there's no interrupt caused by NMI */
	adreno_write_gmureg(adreno_dev,
			ADRENO_REG_GMU_GMU2HOST_INTR_MASK, 0xFFFFFFFF);

	/* Make sure the interrupt is masked before causing it */
	wmb();
	adreno_write_gmureg(adreno_dev,
		ADRENO_REG_GMU_NMI_CONTROL_STATUS, 0);

	adreno_read_gmureg(adreno_dev, ADRENO_REG_GMU_CM3_CFG, &val);
	val |= 1 << GMU_CM3_CFG_NONMASKINTR_SHIFT;
	adreno_write_gmureg(adreno_dev, ADRENO_REG_GMU_CM3_CFG, val);

	/* Make sure the NMI is invoked before we proceed*/
	wmb();
}

static irqreturn_t gmu_irq_handler(int irq, void *data)
{
	struct kgsl_device *device = data;
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	unsigned int mask, status = 0;

	adreno_read_gmureg(ADRENO_DEVICE(device),
			ADRENO_REG_GMU_AO_HOST_INTERRUPT_STATUS, &status);
	adreno_write_gmureg(ADRENO_DEVICE(device),
			ADRENO_REG_GMU_AO_HOST_INTERRUPT_CLR, status);

	/* Ignore GMU_INT_RSCC_COMP and GMU_INT_DBD WAKEUP interrupts */
	if (status & GMU_INT_WDOG_BITE) {
		/* Temporarily mask the watchdog interrupt to prevent a storm */
		adreno_read_gmureg(adreno_dev,
				ADRENO_REG_GMU_AO_HOST_INTERRUPT_MASK, &mask);
		adreno_write_gmureg(adreno_dev,
				ADRENO_REG_GMU_AO_HOST_INTERRUPT_MASK,
				(mask | GMU_INT_WDOG_BITE));

		send_nmi_to_gmu(adreno_dev);
		/*
		 * There is sufficient delay for the GMU to have finished
		 * handling the NMI before snapshot is taken, as the fault
		 * worker is scheduled below.
		 */

		dev_err_ratelimited(&gmu->pdev->dev,
				"GMU watchdog expired interrupt received\n");
		adreno_set_gpu_fault(adreno_dev, ADRENO_GMU_FAULT);
		adreno_dispatcher_schedule(device);
	}
	if (status & GMU_INT_HOST_AHB_BUS_ERR)
		dev_err_ratelimited(&gmu->pdev->dev,
				"AHB bus error interrupt received\n");
	if (status & GMU_INT_FENCE_ERR) {
		unsigned int fence_status;

		adreno_read_gmureg(ADRENO_DEVICE(device),
			ADRENO_REG_GMU_AHB_FENCE_STATUS, &fence_status);
		dev_err_ratelimited(&gmu->pdev->dev,
			"FENCE error interrupt received %x\n", fence_status);
	}

	if (status & ~GMU_AO_INT_MASK)
		dev_err_ratelimited(&gmu->pdev->dev,
				"Unhandled GMU interrupts 0x%lx\n",
				status & ~GMU_AO_INT_MASK);

	return IRQ_HANDLED;
}

static int gmu_pwrlevel_probe(struct gmu_device *gmu, struct device_node *node)
{
	int ret;
	struct device_node *pwrlevel_node, *child;

	/* Add the GMU OPP table if we define it */
	if (of_find_property(gmu->pdev->dev.of_node,
			"operating-points-v2", NULL)) {
		ret = dev_pm_opp_of_add_table(&gmu->pdev->dev);
		if (ret) {
			dev_err(&gmu->pdev->dev,
					"Unable to set the GMU OPP table: %d\n",
					ret);
			return ret;
		}
	}

	pwrlevel_node = of_find_node_by_name(node, "qcom,gmu-pwrlevels");
	if (pwrlevel_node == NULL) {
		dev_err(&gmu->pdev->dev, "Unable to find 'qcom,gmu-pwrlevels'\n");
		return -EINVAL;
	}

	gmu->num_gmupwrlevels = 0;

	for_each_child_of_node(pwrlevel_node, child) {
		unsigned int index;

		if (of_property_read_u32(child, "reg", &index))
			return -EINVAL;

		if (index >= MAX_CX_LEVELS) {
			dev_err(&gmu->pdev->dev, "gmu pwrlevel %d is out of range\n",
				index);
			continue;
		}

		if (index >= gmu->num_gmupwrlevels)
			gmu->num_gmupwrlevels = index + 1;

		if (of_property_read_u32(child, "qcom,gmu-freq",
					&gmu->gmu_freqs[index]))
			return -EINVAL;
	}

	return 0;
}

static int gmu_reg_probe(struct kgsl_device *device)
{
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);
	struct resource *res;

	res = platform_get_resource_byname(gmu->pdev, IORESOURCE_MEM,
			"kgsl_gmu_reg");
	if (res == NULL) {
		dev_err(&gmu->pdev->dev,
			"platform_get_resource kgsl_gmu_reg failed\n");
		return -EINVAL;
	}

	if (res->start == 0 || resource_size(res) == 0) {
		dev_err(&gmu->pdev->dev,
				"dev %d kgsl_gmu_reg invalid register region\n",
				gmu->pdev->dev.id);
		return -EINVAL;
	}

	gmu->reg_phys = res->start;
	gmu->reg_len = resource_size(res);
	device->gmu_core.reg_virt = devm_ioremap(&gmu->pdev->dev,
			res->start, resource_size(res));
	if (device->gmu_core.reg_virt == NULL) {
		dev_err(&gmu->pdev->dev, "kgsl_gmu_reg ioremap failed\n");
		return -ENODEV;
	}

	return 0;
}

static int gmu_clocks_probe(struct gmu_device *gmu, struct device_node *node)
{
	const char *cname;
	struct property *prop;
	struct clk *c;
	int i = 0;

	of_property_for_each_string(node, "clock-names", prop, cname) {
		c = devm_clk_get(&gmu->pdev->dev, cname);

		if (IS_ERR(c)) {
			dev_err(&gmu->pdev->dev,
				"dt: Couldn't get GMU clock: %s\n", cname);
			return PTR_ERR(c);
		}

		if (i >= MAX_GMU_CLKS) {
			dev_err(&gmu->pdev->dev,
				"dt: too many GMU clocks defined\n");
			return -EINVAL;
		}

		gmu->clks[i++] = c;
	}

	return 0;
}

static int gmu_gpu_bw_probe(struct kgsl_device *device, struct gmu_device *gmu)
{
	struct msm_bus_scale_pdata *bus_scale_table;

	bus_scale_table = msm_bus_cl_get_pdata(device->pdev);
	if (bus_scale_table == NULL) {
		dev_err(&gmu->pdev->dev, "dt: cannot get bus table\n");
		return -ENODEV;
	}

	gmu->num_bwlevels = bus_scale_table->num_usecases;
	gmu->pcl = msm_bus_scale_register_client(bus_scale_table);
	if (!gmu->pcl) {
		dev_err(&gmu->pdev->dev, "dt: cannot register bus client\n");
		return -ENODEV;
	}

	return 0;
}

static int gmu_cnoc_bw_probe(struct gmu_device *gmu)
{
	struct msm_bus_scale_pdata *cnoc_table;

	cnoc_table = msm_bus_cl_get_pdata(gmu->pdev);
	if (cnoc_table == NULL) {
		dev_err(&gmu->pdev->dev, "dt: cannot get cnoc table\n");
		return -ENODEV;
	}

	gmu->num_cnocbwlevels = cnoc_table->num_usecases;
	gmu->ccl = msm_bus_scale_register_client(cnoc_table);
	if (!gmu->ccl) {
		dev_err(&gmu->pdev->dev, "dt: cannot register cnoc client\n");
		return -ENODEV;
	}

	return 0;
}

static int gmu_regulators_probe(struct gmu_device *gmu,
		struct device_node *node)
{
	const char *name;
	struct property *prop;
	struct device *dev = &gmu->pdev->dev;
	int ret = 0;

	of_property_for_each_string(node, "regulator-names", prop, name) {
		if (!strcmp(name, "vddcx")) {
			gmu->cx_gdsc = devm_regulator_get(dev, name);
			if (IS_ERR(gmu->cx_gdsc)) {
				ret = PTR_ERR(gmu->cx_gdsc);
				dev_err(dev, "dt: GMU couldn't get CX gdsc\n");
				gmu->cx_gdsc = NULL;
				return ret;
			}
		} else if (!strcmp(name, "vdd")) {
			gmu->gx_gdsc = devm_regulator_get(dev, name);
			if (IS_ERR(gmu->gx_gdsc)) {
				ret = PTR_ERR(gmu->gx_gdsc);
				dev_err(dev, "dt: GMU couldn't get GX gdsc\n");
				gmu->gx_gdsc = NULL;
				return ret;
			}
		} else {
			dev_err(dev, "dt: Unknown GMU regulator: %s\n", name);
			return -ENODEV;
		}
	}

	return 0;
}

static int gmu_irq_probe(struct kgsl_device *device, struct gmu_device *gmu)
{
	int ret;
	struct kgsl_hfi *hfi = &gmu->hfi;

	hfi->hfi_interrupt_num = platform_get_irq_byname(gmu->pdev,
			"kgsl_hfi_irq");
	ret = devm_request_irq(&gmu->pdev->dev,
			hfi->hfi_interrupt_num,
			hfi_irq_handler, IRQF_TRIGGER_HIGH,
			"HFI", device);
	if (ret) {
		dev_err(&gmu->pdev->dev, "request_irq(%d) failed: %d\n",
				hfi->hfi_interrupt_num, ret);
		return ret;
	}

	gmu->gmu_interrupt_num = platform_get_irq_byname(gmu->pdev,
			"kgsl_gmu_irq");
	ret = devm_request_irq(&gmu->pdev->dev,
			gmu->gmu_interrupt_num,
			gmu_irq_handler, IRQF_TRIGGER_HIGH,
			"GMU", device);
	if (ret)
		dev_err(&gmu->pdev->dev, "request_irq(%d) failed: %d\n",
				gmu->gmu_interrupt_num, ret);

	return ret;
}

struct mbox_message {
	uint32_t len;
	void *msg;
};

static void gmu_aop_send_acd_state(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);
	struct kgsl_mailbox *mailbox = &gmu->mailbox;
	struct mbox_message msg;
	char msg_buf[33];
	bool state = test_bit(ADRENO_ACD_CTRL, &adreno_dev->pwrctrl_flag);
	int ret;

	if (!mailbox->client)
		return;

	if (state == mailbox->enabled)
		return;

	msg.len = scnprintf(msg_buf, sizeof(msg_buf),
			"{class: gpu, res: acd, value: %d}", state);
	msg.msg = msg_buf;

	ret = mbox_send_message(mailbox->channel, &msg);
	if (ret < 0) {
		dev_err(&gmu->pdev->dev,
				"AOP mbox send message failed: %d\n", ret);
		return;
	}

	mailbox->enabled = state;
}

static void gmu_aop_mailbox_destroy(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);
	struct kgsl_mailbox *mailbox = &gmu->mailbox;

	if (!mailbox->client)
		return;

	/* Turn off ACD in AOP */
	clear_bit(ADRENO_ACD_CTRL, &adreno_dev->pwrctrl_flag);
	gmu_aop_send_acd_state(device);

	mbox_free_channel(mailbox->channel);
	mailbox->channel = NULL;

	kfree(mailbox->client);
	mailbox->client = NULL;
}

static int gmu_aop_mailbox_init(struct kgsl_device *device,
		struct gmu_device *gmu)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct kgsl_mailbox *mailbox = &gmu->mailbox;

	if (adreno_is_a640v2(adreno_dev) && (!adreno_dev->speed_bin))
		return 0;

	mailbox->client = kzalloc(sizeof(*mailbox->client), GFP_KERNEL);
	if (!mailbox->client)
		return -ENOMEM;

	mailbox->client->dev = &gmu->pdev->dev;
	mailbox->client->tx_block = true;
	mailbox->client->tx_tout = 1000;
	mailbox->client->knows_txdone = false;

	mailbox->channel = mbox_request_channel(mailbox->client, 0);
	if (IS_ERR(mailbox->channel)) {
		kfree(mailbox->client);
		mailbox->client = NULL;
		return PTR_ERR(mailbox->channel);
	}

	set_bit(ADRENO_ACD_CTRL, &adreno_dev->pwrctrl_flag);

	return 0;
}

static int gmu_acd_set(struct kgsl_device *device, unsigned int val)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);

	if (!gmu->mailbox.client)
		return -EINVAL;

	/* Don't do any unneeded work if ACD is already in the correct state */
	if (val == test_bit(ADRENO_ACD_CTRL, &adreno_dev->pwrctrl_flag))
		return 0;

	return kgsl_change_flag(device, ADRENO_ACD_CTRL,
			&adreno_dev->pwrctrl_flag);
}

/* Do not access any GMU registers in GMU probe function */
static int gmu_probe(struct kgsl_device *device, struct device_node *node)
{
	struct gmu_device *gmu;
	struct gmu_memdesc *mem_addr = NULL;
	struct kgsl_hfi *hfi;
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	int i = 0, ret = -ENXIO;

	gmu = kzalloc(sizeof(struct gmu_device), GFP_KERNEL);

	if (gmu == NULL)
		return -ENOMEM;

	device->gmu_core.ptr = (void *)gmu;
	hfi = &gmu->hfi;
	gmu->load_mode = TCM_BOOT;
	gmu->ver = ~0U;

	gmu->pdev = of_find_device_by_node(node);
	of_dma_configure(&gmu->pdev->dev, node);

	/* Set up GMU regulators */
	ret = gmu_regulators_probe(gmu, node);
	if (ret)
		goto error;

	/* Set up GMU clocks */
	ret = gmu_clocks_probe(gmu, node);
	if (ret)
		goto error;

	/* Set up GMU IOMMU and shared memory with GMU */
	ret = gmu_memory_probe(device, gmu, node);
	if (ret)
		goto error;
	mem_addr = gmu->hfi_mem;

	/* Map and reserve GMU CSRs registers */
	ret = gmu_reg_probe(device);
	if (ret)
		goto error;

	device->gmu_core.gmu2gpu_offset =
			(gmu->reg_phys - device->reg_phys) >> 2;
	device->gmu_core.reg_len = gmu->reg_len;

	/* Initialize HFI and GMU interrupts */
	ret = gmu_irq_probe(device, gmu);
	if (ret)
		goto error;

	/* Don't enable GMU interrupts until GMU started */
	/* We cannot use irq_disable because it writes registers */
	disable_irq(gmu->gmu_interrupt_num);
	disable_irq(hfi->hfi_interrupt_num);

	tasklet_init(&hfi->tasklet, hfi_receiver, (unsigned long) gmu);
	hfi->kgsldev = device;

	/* Retrieves GMU/GPU power level configurations*/
	ret = gmu_pwrlevel_probe(gmu, node);
	if (ret)
		goto error;

	gmu->num_gpupwrlevels = pwr->num_pwrlevels;

	for (i = 0; i < gmu->num_gpupwrlevels; i++) {
		int j = gmu->num_gpupwrlevels - 1 - i;

		gmu->gpu_freqs[i] = pwr->pwrlevels[j].gpu_freq;
	}

	/* Initializes GPU b/w levels configuration */
	ret = gmu_gpu_bw_probe(device, gmu);
	if (ret)
		goto error;

	/* Initialize GMU CNOC b/w levels configuration */
	ret = gmu_cnoc_bw_probe(gmu);
	if (ret)
		goto error;

	/* Populates RPMh configurations */
	ret = gmu_rpmh_init(device, gmu, pwr);
	if (ret)
		goto error;

	hfi_init(&gmu->hfi, mem_addr, HFI_QUEUE_SIZE);

	/* Set up GMU idle states */
	if (ADRENO_FEATURE(adreno_dev, ADRENO_MIN_VOLT))
		gmu->idle_level = GPU_HW_MIN_VOLT;
	else if (ADRENO_FEATURE(adreno_dev, ADRENO_HW_NAP))
		gmu->idle_level = GPU_HW_NAP;
	else if (ADRENO_FEATURE(adreno_dev, ADRENO_IFPC))
		gmu->idle_level = GPU_HW_IFPC;
	else if (ADRENO_FEATURE(adreno_dev, ADRENO_SPTP_PC))
		gmu->idle_level = GPU_HW_SPTP_PC;
	else
		gmu->idle_level = GPU_HW_ACTIVE;

	if (ADRENO_FEATURE(adreno_dev, ADRENO_ACD) && !noacd) {
		if (!gmu_acd_probe(gmu, node)) {
			/* Init the AOP mailbox if we have a valid ACD table */
			ret = gmu_aop_mailbox_init(device, gmu);
			if (ret)
				dev_err(&gmu->pdev->dev,
					"AOP mailbox init failed: %d\n", ret);
		} else
			dev_err(&gmu->pdev->dev,
				"ACD probe failed: missing or invalid table\n");
	}

	if (ADRENO_FEATURE(adreno_dev, ADRENO_LM))
		set_bit(ADRENO_LM_CTRL, &adreno_dev->pwrctrl_flag);

	set_bit(GMU_ENABLED, &device->gmu_core.flags);
	device->gmu_core.dev_ops = &adreno_a6xx_gmudev;

	return 0;

error:
	gmu_remove(device);
	return ret;
}

static int gmu_enable_clks(struct kgsl_device *device)
{
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);
	int ret, j = 0;

	if (IS_ERR_OR_NULL(gmu->clks[0]))
		return -EINVAL;

	ret = clk_set_rate(gmu->clks[0], gmu->gmu_freqs[DEFAULT_GMU_FREQ_IDX]);
	if (ret) {
		dev_err(&gmu->pdev->dev, "fail to set default GMU clk freq %d\n",
				gmu->gmu_freqs[DEFAULT_GMU_FREQ_IDX]);
		return ret;
	}

	while ((j < MAX_GMU_CLKS) && gmu->clks[j]) {
		ret = clk_prepare_enable(gmu->clks[j]);
		if (ret) {
			dev_err(&gmu->pdev->dev,
					"fail to enable gpucc clk idx %d\n",
					j);
			return ret;
		}
		j++;
	}

	set_bit(GMU_CLK_ON, &device->gmu_core.flags);
	return 0;
}

static int gmu_disable_clks(struct kgsl_device *device)
{
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);
	int j = 0;

	if (IS_ERR_OR_NULL(gmu->clks[0]))
		return 0;

	while ((j < MAX_GMU_CLKS) && gmu->clks[j]) {
		clk_disable_unprepare(gmu->clks[j]);
		j++;
	}

	clear_bit(GMU_CLK_ON, &device->gmu_core.flags);
	return 0;

}

static int gmu_enable_gdsc(struct gmu_device *gmu)
{
	int ret;

	if (IS_ERR_OR_NULL(gmu->cx_gdsc))
		return 0;

	ret = regulator_enable(gmu->cx_gdsc);
	if (ret)
		dev_err(&gmu->pdev->dev,
			"Failed to enable GMU CX gdsc, error %d\n", ret);

	return ret;
}

#define CX_GDSC_TIMEOUT	5000	/* ms */
static int gmu_disable_gdsc(struct kgsl_device *device)
{
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);
	int ret;
	unsigned long t;

	if (IS_ERR_OR_NULL(gmu->cx_gdsc))
		return 0;

	ret = regulator_disable(gmu->cx_gdsc);
	if (ret) {
		dev_err(&gmu->pdev->dev,
			"Failed to disable GMU CX gdsc, error %d\n", ret);
		return ret;
	}

	/*
	 * After GX GDSC is off, CX GDSC must be off
	 * Voting off alone from GPU driver cannot
	 * Guarantee CX GDSC off. Polling with 5s
	 * timeout to ensure
	 */
	t = jiffies + msecs_to_jiffies(CX_GDSC_TIMEOUT);
	do {
		if (!gmu_core_dev_cx_is_on(device))
			return 0;
		usleep_range(10, 100);

	} while (!(time_after(jiffies, t)));

	if (!gmu_core_dev_cx_is_on(device))
		return 0;

	dev_err(&gmu->pdev->dev, "GMU CX gdsc off timeout");
	return -ETIMEDOUT;
}

static int gmu_suspend(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct gmu_dev_ops *gmu_dev_ops = GMU_DEVICE_OPS(device);
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);

	if (!test_bit(GMU_CLK_ON, &device->gmu_core.flags))
		return 0;

	/* Pending message in all queues are abandoned */
	gmu_dev_ops->irq_disable(device);
	hfi_stop(gmu);

	if (gmu_dev_ops->rpmh_gpu_pwrctrl(adreno_dev, GMU_SUSPEND, 0, 0))
		return -EINVAL;

	gmu_disable_clks(device);

	if (ADRENO_QUIRK(adreno_dev, ADRENO_QUIRK_CX_GDSC))
		regulator_set_mode(gmu->cx_gdsc, REGULATOR_MODE_IDLE);

	gmu_disable_gdsc(device);

	if (ADRENO_QUIRK(adreno_dev, ADRENO_QUIRK_CX_GDSC))
		regulator_set_mode(gmu->cx_gdsc, REGULATOR_MODE_NORMAL);

	dev_err(&gmu->pdev->dev, "Suspended GMU\n");
	return 0;
}

static void gmu_snapshot(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct gmu_dev_ops *gmu_dev_ops = GMU_DEVICE_OPS(device);
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);

	send_nmi_to_gmu(adreno_dev);
	/* Wait for the NMI to be handled */
	udelay(100);
	kgsl_device_snapshot(device, NULL, true);

	adreno_write_gmureg(adreno_dev,
			ADRENO_REG_GMU_GMU2HOST_INTR_CLR, 0xFFFFFFFF);
	adreno_write_gmureg(adreno_dev,
			ADRENO_REG_GMU_GMU2HOST_INTR_MASK,
			~(gmu_dev_ops->gmu2host_intr_mask));

	gmu->fault_count++;
}

/* To be called to power on both GPU and GMU */
static int gmu_start(struct kgsl_device *device)
{
	int ret = 0;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct gmu_dev_ops *gmu_dev_ops = GMU_DEVICE_OPS(device);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);

	switch (device->state) {
	case KGSL_STATE_INIT:
	case KGSL_STATE_SUSPEND:
		WARN_ON(test_bit(GMU_CLK_ON, &device->gmu_core.flags));

		gmu_aop_send_acd_state(device);

		gmu_enable_gdsc(gmu);
		gmu_enable_clks(device);
		gmu_dev_ops->irq_enable(device);

		/* Vote for 300MHz DDR for GMU to init */
		ret = msm_bus_scale_client_update_request(gmu->pcl,
				pwr->pwrlevels[pwr->default_pwrlevel].bus_freq);
		if (ret)
			dev_err(&gmu->pdev->dev,
				"Failed to allocate gmu b/w: %d\n", ret);

		ret = gmu_dev_ops->rpmh_gpu_pwrctrl(adreno_dev, GMU_FW_START,
				GMU_COLD_BOOT, 0);
		if (ret)
			goto error_gmu;

		ret = hfi_start(device, gmu, GMU_COLD_BOOT);
		if (ret)
			goto error_gmu;

		/* Request default DCVS level */
		kgsl_pwrctrl_set_default_gpu_pwrlevel(device);
		msm_bus_scale_client_update_request(gmu->pcl, 0);
		break;

	case KGSL_STATE_SLUMBER:
		WARN_ON(test_bit(GMU_CLK_ON, &device->gmu_core.flags));

		gmu_aop_send_acd_state(device);

		gmu_enable_gdsc(gmu);
		gmu_enable_clks(device);
		gmu_dev_ops->irq_enable(device);

		ret = gmu_dev_ops->rpmh_gpu_pwrctrl(adreno_dev, GMU_FW_START,
				GMU_COLD_BOOT, 0);
		if (ret)
			goto error_gmu;

		ret = hfi_start(device, gmu, GMU_COLD_BOOT);
		if (ret)
			goto error_gmu;

		kgsl_pwrctrl_set_default_gpu_pwrlevel(device);
		break;

	case KGSL_STATE_RESET:
		gmu_suspend(device);

		gmu_aop_send_acd_state(device);

		gmu_enable_gdsc(gmu);
		gmu_enable_clks(device);
		gmu_dev_ops->irq_enable(device);

		ret = gmu_dev_ops->rpmh_gpu_pwrctrl(
				adreno_dev, GMU_FW_START, GMU_COLD_BOOT, 0);
		if (ret)
			goto error_gmu;


		ret = hfi_start(device, gmu, GMU_COLD_BOOT);
		if (ret)
			goto error_gmu;

		/* Send DCVS level prior to reset*/
		kgsl_pwrctrl_set_default_gpu_pwrlevel(device);
		break;
	default:
		break;
	}

	return ret;

error_gmu:
	if (ADRENO_QUIRK(adreno_dev, ADRENO_QUIRK_HFI_USE_REG))
		gmu_dev_ops->oob_clear(adreno_dev, oob_boot_slumber);
	gmu_core_snapshot(device);
	return ret;
}

/* Caller shall ensure GPU is ready for SLUMBER */
static void gmu_stop(struct kgsl_device *device)
{
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct gmu_dev_ops *gmu_dev_ops = GMU_DEVICE_OPS(device);
	int ret = 0;

	if (!test_bit(GMU_CLK_ON, &device->gmu_core.flags))
		return;

	/* Wait for the lowest idle level we requested */
	if (gmu_dev_ops->wait_for_lowest_idle &&
			gmu_dev_ops->wait_for_lowest_idle(adreno_dev))
		goto error;

	ret = gmu_dev_ops->rpmh_gpu_pwrctrl(adreno_dev,
			GMU_NOTIFY_SLUMBER, 0, 0);
	if (ret)
		goto error;

	if (gmu_dev_ops->wait_for_gmu_idle &&
			gmu_dev_ops->wait_for_gmu_idle(adreno_dev))
		goto error;

	/* Pending message in all queues are abandoned */
	gmu_dev_ops->irq_disable(device);
	hfi_stop(gmu);

	gmu_dev_ops->rpmh_gpu_pwrctrl(adreno_dev, GMU_FW_STOP, 0, 0);
	gmu_disable_clks(device);
	gmu_disable_gdsc(device);

	msm_bus_scale_client_update_request(gmu->pcl, 0);
	return;

error:
	/*
	 * The power controller will change state to SLUMBER anyway
	 * Set GMU_FAULT flag to indicate to power contrller
	 * that hang recovery is needed to power on GPU
	 */
	set_bit(GMU_FAULT, &device->gmu_core.flags);
	dev_err(&gmu->pdev->dev, "Failed to stop GMU\n");
	gmu_core_snapshot(device);
}

static void gmu_remove(struct kgsl_device *device)
{
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);
	struct kgsl_hfi *hfi;
	int i = 0;

	if (gmu == NULL || gmu->pdev == NULL)
		return;

	hfi = &gmu->hfi;

	tasklet_kill(&hfi->tasklet);

	gmu_stop(device);

	gmu_aop_mailbox_destroy(device);

	while ((i < MAX_GMU_CLKS) && gmu->clks[i]) {
		gmu->clks[i] = NULL;
		i++;
	}

	if (gmu->gmu_interrupt_num) {
		devm_free_irq(&gmu->pdev->dev,
				gmu->gmu_interrupt_num, device);
		gmu->gmu_interrupt_num = 0;
	}

	if (hfi->hfi_interrupt_num) {
		devm_free_irq(&gmu->pdev->dev,
				hfi->hfi_interrupt_num, device);
		hfi->hfi_interrupt_num = 0;
	}

	if (gmu->ccl) {
		msm_bus_scale_unregister_client(gmu->ccl);
		gmu->ccl = 0;
	}

	if (gmu->pcl) {
		msm_bus_scale_unregister_client(gmu->pcl);
		gmu->pcl = 0;
	}

	if (gmu->fw_image) {
		release_firmware(gmu->fw_image);
		gmu->fw_image = NULL;
	}

	gmu_memory_close(gmu);

	for (i = 0; i < MAX_GMU_CLKS; i++) {
		if (gmu->clks[i]) {
			devm_clk_put(&gmu->pdev->dev, gmu->clks[i]);
			gmu->clks[i] = NULL;
		}
	}

	if (gmu->gx_gdsc) {
		devm_regulator_put(gmu->gx_gdsc);
		gmu->gx_gdsc = NULL;
	}

	if (gmu->cx_gdsc) {
		devm_regulator_put(gmu->cx_gdsc);
		gmu->cx_gdsc = NULL;
	}

	device->gmu_core.flags = 0;
	device->gmu_core.ptr = NULL;
	gmu->pdev = NULL;
	kfree(gmu);
}

static bool gmu_regulator_isenabled(struct kgsl_device *device)
{
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);

	return (gmu->gx_gdsc &&	regulator_is_enabled(gmu->gx_gdsc));
}

static bool gmu_is_initialized(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct gmu_dev_ops *gmu_dev_ops = GMU_DEVICE_OPS(device);
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);
	bool ret;

	gmu_enable_gdsc(gmu);
	gmu_enable_clks(device);

	ret = gmu_dev_ops->is_initialized(adreno_dev);

	gmu_disable_clks(device);
	gmu_disable_gdsc(device);

	return ret;
}

struct gmu_core_ops gmu_ops = {
	.probe = gmu_probe,
	.remove = gmu_remove,
	.start = gmu_start,
	.stop = gmu_stop,
	.dcvs_set = gmu_dcvs_set,
	.snapshot = gmu_snapshot,
	.regulator_isenabled = gmu_regulator_isenabled,
	.suspend = gmu_suspend,
	.acd_set = gmu_acd_set,
	.is_initialized = gmu_is_initialized,
};
