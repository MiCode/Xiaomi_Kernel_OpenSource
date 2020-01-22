// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */

#include <dt-bindings/regulator/qcom,rpmh-regulator-levels.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/io.h>
#include <linux/iommu.h>
#include <linux/mailbox_client.h>
#include <linux/msm-bus.h>
#include <linux/of_platform.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <soc/qcom/cmd-db.h>

#include "adreno.h"
#include "kgsl_device.h"
#include "kgsl_gmu.h"

#undef MODULE_PARAM_PREFIX
#define MODULE_PARAM_PREFIX "kgsl."

struct gmu_iommu_context {
	const char *name;
	struct device *dev;
	struct iommu_domain *domain;
};

#define DUMPMEM_SIZE SZ_16K

#define DUMMY_SIZE   SZ_4K

/* Define target specific GMU VMA configurations */

struct gmu_vma_entry {
	unsigned int start;
	unsigned int size;
};

static const struct gmu_vma_entry gmu_vma_legacy[] = {
	[GMU_ITCM] = { .start = 0x00000, .size = SZ_16K },
	[GMU_ICACHE] = { .start = 0x04000, .size = (SZ_256K - SZ_16K) },
	[GMU_DTCM] = { .start = 0x40000, .size = SZ_16K },
	[GMU_DCACHE] = { .start = 0x44000, .size = (SZ_256K - SZ_16K) },
	[GMU_NONCACHED_KERNEL] = { .start = 0x60000000, .size = SZ_512M },
	[GMU_NONCACHED_USER] = { .start = 0x80000000, .size = SZ_1G },
	[GMU_MEM_TYPE_MAX] = { .start = 0x0, .size = 0x0 },
};

static const struct gmu_vma_entry gmu_vma[] = {
	[GMU_ITCM] = { .start = 0x00000000, .size = SZ_16K },
	[GMU_CACHE] = { .start = SZ_16K, .size = (SZ_16M - SZ_16K) },
	[GMU_DTCM] = { .start = SZ_256M + SZ_16K, .size = SZ_16K },
	[GMU_DCACHE] = { .start = 0x0, .size = 0x0 },
	[GMU_NONCACHED_KERNEL] = { .start = 0x60000000, .size = SZ_512M },
	[GMU_NONCACHED_USER] = { .start = 0x80000000, .size = SZ_1G },
	[GMU_MEM_TYPE_MAX] = { .start = 0x0, .size = 0x0 },
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
 * a unique address inside GMU kernel mode address range.
 */
static unsigned int next_uncached_kernel_alloc;
static unsigned int next_uncached_user_alloc;

static void gmu_snapshot(struct kgsl_device *device);
static void gmu_remove(struct kgsl_device *device);

unsigned int gmu_get_memtype_base(struct gmu_device *gmu,
		enum gmu_mem_type type)
{
	return gmu->vma[type].start;
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

static int alloc_and_map(struct gmu_device *gmu, struct gmu_memdesc *md,
		unsigned int attrs)
{
	int ret;
	struct iommu_domain *domain;

	domain = gmu_ctx[md->ctx_idx].domain;

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

struct gmu_memdesc *gmu_get_memdesc(struct gmu_device *gmu,
		unsigned int addr, unsigned int size)
{
	int i;
	struct gmu_memdesc *mem;

	for (i = 0; i < GMU_KERNEL_ENTRIES; i++) {
		if (!test_bit(i, &gmu->kmem_bitmap))
			continue;

		mem = &gmu->kmem_entries[i];

		if (addr >= mem->gmuaddr &&
				(addr + size <= mem->gmuaddr + mem->size))
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
		enum gmu_mem_type mem_type, unsigned int addr,
		unsigned int size, unsigned int attrs)
{
	struct gmu_memdesc *md;
	int ret;
	int entry_idx = find_first_zero_bit(
			&gmu->kmem_bitmap, GMU_KERNEL_ENTRIES);

	if (entry_idx >= GMU_KERNEL_ENTRIES) {
		dev_err(&gmu->pdev->dev,
				"Ran out of GMU kernel mempool slots\n");
		return ERR_PTR(-EINVAL);
	}

	/* Non-TCM requests have page alignment requirement */
	if ((mem_type != GMU_ITCM) && (mem_type != GMU_DTCM) &&
			addr & (PAGE_SIZE - 1)) {
		dev_err(&gmu->pdev->dev,
				"Invalid alignment request 0x%X\n",
				addr);
		return ERR_PTR(-EINVAL);
	}

	md = &gmu->kmem_entries[entry_idx];
	set_bit(entry_idx, &gmu->kmem_bitmap);

	memset(md, 0, sizeof(*md));

	switch (mem_type) {
	case GMU_ITCM:
	case GMU_DTCM:
		/* Assign values and return without mapping */
		md->size = size;
		md->mem_type = mem_type;
		md->gmuaddr = addr;
		return md;

	case GMU_DCACHE:
	case GMU_ICACHE:
		size = PAGE_ALIGN(size);
		md->ctx_idx = GMU_CONTEXT_KERNEL;
		break;

	case GMU_NONCACHED_KERNEL:
		/* Set start address for first uncached kernel alloc */
		if (next_uncached_kernel_alloc == 0)
			next_uncached_kernel_alloc = gmu->vma[mem_type].start;

		if (addr == 0)
			addr = next_uncached_kernel_alloc;

		size = PAGE_ALIGN(size);
		md->ctx_idx = GMU_CONTEXT_KERNEL;
		break;

	case GMU_NONCACHED_USER:
		/* Set start address for first uncached user alloc */
		if (next_uncached_user_alloc == 0)
			next_uncached_user_alloc = gmu->vma[mem_type].start;

		if (addr == 0)
			addr = next_uncached_user_alloc;

		size = PAGE_ALIGN(size);
		md->ctx_idx = GMU_CONTEXT_USER;
		break;

	default:
		dev_err(&gmu->pdev->dev,
				"Invalid memory type (%d) requested\n",
				mem_type);
		clear_bit(entry_idx, &gmu->kmem_bitmap);
		return ERR_PTR(-EINVAL);
	}

	md->size = size;
	md->mem_type = mem_type;
	md->gmuaddr = addr;

	ret = alloc_and_map(gmu, md, attrs);
	if (ret) {
		clear_bit(entry_idx, &gmu->kmem_bitmap);
		return ERR_PTR(ret);
	}

	if (mem_type == GMU_NONCACHED_KERNEL)
		next_uncached_kernel_alloc = PAGE_ALIGN(md->gmuaddr + md->size);
	else if (mem_type == GMU_NONCACHED_USER)
		next_uncached_user_alloc = PAGE_ALIGN(md->gmuaddr + md->size);

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
	of_dma_configure(dev, node, true);

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
 * gmu_memory_close() - free all memory allocated for GMU and detach GMU
 * from IOMMU context banks.
 * @gmu: Pointer to GMU device
 */
static void gmu_memory_close(struct gmu_device *gmu)
{
	int i;
	struct gmu_memdesc *md;
	struct gmu_iommu_context *ctx;

	gmu->hfi_mem = NULL;
	gmu->dump_mem = NULL;
	gmu->gmu_log = NULL;
	gmu->preallocations = false;

	/* Unmap and free all memories in GMU kernel memory pool */
	for (i = 0; i < GMU_KERNEL_ENTRIES; i++) {
		if (!test_bit(i, &gmu->kmem_bitmap))
			continue;

		md = &gmu->kmem_entries[i];
		ctx = &gmu_ctx[md->ctx_idx];

		if (!ctx->domain)
			continue;

		if (md->gmuaddr && md->mem_type != GMU_ITCM &&
				md->mem_type != GMU_DTCM)
			iommu_unmap(ctx->domain, md->gmuaddr, md->size);

		free_gmu_mem(gmu, md);

		clear_bit(i, &gmu->kmem_bitmap);
	}

	for (i = 0; i < ARRAY_SIZE(gmu_ctx); i++) {
		ctx = &gmu_ctx[i];

		if (ctx->domain) {
			iommu_detach_device(ctx->domain, ctx->dev);
			iommu_domain_free(ctx->domain);
		}
	}
}

static enum gmu_mem_type gmu_get_blk_memtype(struct gmu_device *gmu,
		struct gmu_block_header *blk)
{
	int i;

	for (i = 0; i < GMU_MEM_TYPE_MAX; i++) {
		if (blk->addr >= gmu->vma[i].start &&
				blk->addr + blk->value <=
				gmu->vma[i].start + gmu->vma[i].size)
			return (enum gmu_mem_type)i;
	}

	return GMU_MEM_TYPE_MAX;
}

int gmu_prealloc_req(struct kgsl_device *device, struct gmu_block_header *blk)
{
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);
	enum gmu_mem_type type;
	struct gmu_memdesc *md;

	/* Check to see if this memdesc is already around */
	md = gmu_get_memdesc(gmu, blk->addr, blk->value);
	if (md)
		return 0;

	type = gmu_get_blk_memtype(gmu, blk);
	if (type >= GMU_MEM_TYPE_MAX)
		return -EINVAL;

	md = allocate_gmu_kmem(gmu, type, blk->addr, blk->value,
			(IOMMU_READ | IOMMU_WRITE | IOMMU_PRIV));
	if (IS_ERR(md))
		return PTR_ERR(md);

	gmu->preallocations = true;

	return 0;
}

/*
 * gmu_memory_probe() - probe GMU IOMMU context banks and allocate memory
 * to share with GMU in kernel mode.
 * @device: Pointer to KGSL device
 */
static int gmu_memory_probe(struct kgsl_device *device)
{
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	/* Allocates & maps memory for HFI */
	if (IS_ERR_OR_NULL(gmu->hfi_mem))
		gmu->hfi_mem = allocate_gmu_kmem(gmu, GMU_NONCACHED_KERNEL, 0,
				HFIMEM_SIZE, (IOMMU_READ | IOMMU_WRITE));
	if (IS_ERR(gmu->hfi_mem))
		return PTR_ERR(gmu->hfi_mem);

	/* Allocates & maps GMU crash dump memory */
	if (adreno_is_a630(adreno_dev) || adreno_is_a615_family(adreno_dev)) {
		if (IS_ERR_OR_NULL(gmu->dump_mem))
			gmu->dump_mem = allocate_gmu_kmem(gmu,
					GMU_NONCACHED_KERNEL, 0,
					DUMPMEM_SIZE,
					(IOMMU_READ | IOMMU_WRITE));
		if (IS_ERR(gmu->dump_mem))
			return PTR_ERR(gmu->dump_mem);
	}

	/* GMU master log */
	if (IS_ERR_OR_NULL(gmu->gmu_log))
		gmu->gmu_log = allocate_gmu_kmem(gmu, GMU_NONCACHED_KERNEL, 0,
				LOGMEM_SIZE,
				(IOMMU_READ | IOMMU_WRITE | IOMMU_PRIV));
	return PTR_ERR_OR_ZERO(gmu->gmu_log);
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
		ret = gmu_dev_ops->rpmh_gpu_pwrctrl(device,
			GMU_DCVS_NOHFI, req.freq, req.bw);
	else if (test_bit(GMU_HFI_ON, &device->gmu_core.flags))
		ret = hfi_send_req(gmu, H2F_MSG_GX_BW_PERF_VOTE, &req);

	if (ret) {
		dev_err_ratelimited(&gmu->pdev->dev,
			"Failed to set GPU perf idx %d, bw idx %d\n",
			req.freq, req.bw);

		/*
		 * We can be here in two situations. First, we send a dcvs
		 * hfi so gmu knows at what level it must bring up the gpu.
		 * If that fails, it is already being handled as part of
		 * gmu boot failures. The other reason why we are here is
		 * because we are trying to scale an active gpu. For this,
		 * we need to do inline snapshot and dispatcher based
		 * recovery.
		 */
		if (test_bit(ADRENO_DEVICE_STARTED, &adreno_dev->priv)) {
			gmu_core_snapshot(device);
			adreno_set_gpu_fault(adreno_dev, ADRENO_GMU_FAULT);
			adreno_set_gpu_fault(adreno_dev,
				ADRENO_GMU_FAULT_SKIP_SNAPSHOT);
			adreno_dispatcher_schedule(device);
		}
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

	memset(arc, 0, sizeof(*arc));

	len = cmd_db_read_aux_data_len(res_id);
	if (len == 0)
		return -EINVAL;

	if (len > (MAX_GX_LEVELS << 1)) {
		dev_err(&gmu->pdev->dev,
			"gfx cmddb size %d larger than alloc buf %d of %s\n",
			len, (MAX_GX_LEVELS << 1), res_id);
		return -EINVAL;
	}

	cmd_db_read_aux_data(res_id, (uint8_t *)arc->val, len);

	/*
	 * cmd_db_read_aux_data() gives us a zero-padded table of
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
		u16 *vlvl, unsigned int num_entries)
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


static int rpmh_gmu_arc_votes_init(struct gmu_device *gmu,
		struct rpmh_arc_vals *pri_rail, struct rpmh_arc_vals *sec_rail)
{
	/* Hardcoded values of GMU CX voltage levels */
	u16 gmu_cx_vlvl[] = { 0, RPMH_REGULATOR_LEVEL_MIN_SVS };

	return setup_volt_dependency_tbl(gmu->rpmh_votes.cx_votes, pri_rail,
						sec_rail, gmu_cx_vlvl, 2);
}

/*
 * rpmh_arc_votes_init() - initialized GX RPMh votes needed for rails
 * voltage scaling by GMU.
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
	unsigned int num_freqs;
	u16 vlvl_tbl[MAX_GX_LEVELS];
	unsigned int *freq_tbl;
	int i;
	struct dev_pm_opp *opp;

	if (type == GMU_ARC_VOTE)
		return rpmh_gmu_arc_votes_init(gmu, pri_rail, sec_rail);

	num_freqs = gmu->num_gpupwrlevels;
	freq_tbl = gmu->gpu_freqs;

	if (num_freqs > pri_rail->num || num_freqs > MAX_GX_LEVELS) {
		dev_err(&gmu->pdev->dev,
			"Defined more GPU DCVS levels than RPMh can support\n");
		return -EINVAL;
	}

	memset(vlvl_tbl, 0, sizeof(vlvl_tbl));

	/* Get the values from OPP API */
	for (i = 0; i < num_freqs; i++) {
		/* Hardcode VLVL 0 because it is not present in OPP */
		if (freq_tbl[i] == 0) {
			vlvl_tbl[i] = 0;
			continue;
		}

		opp = dev_pm_opp_find_freq_exact(&device->pdev->dev,
			freq_tbl[i], true);

		if (IS_ERR(opp)) {
			dev_err(&gmu->pdev->dev,
				"Failed to find opp freq %d for GPU\n",
				freq_tbl[i]);
			return PTR_ERR(opp);
		}

		vlvl_tbl[i] = dev_pm_opp_get_voltage(opp);
		dev_pm_opp_put(opp);
	}

	return setup_volt_dependency_tbl(gmu->rpmh_votes.gx_votes, pri_rail,
						sec_rail, vlvl_tbl, num_freqs);
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
						tmp->cmds[j].wait ?
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

		adreno_gmu_send_nmi(adreno_dev);
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
	struct msm_bus_scale_pdata *bus_scale_table =
		kgsl_get_bus_scale_table(device);

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

struct mbox_message {
	uint32_t len;
	void *msg;
};

static void gmu_aop_send_acd_state(struct kgsl_device *device, bool flag)
{
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);
	struct mbox_message msg;
	char msg_buf[33];
	int ret;

	if (!gmu->mailbox.client)
		return;

	msg.len = scnprintf(msg_buf, sizeof(msg_buf),
			"{class: gpu, res: acd, value: %d}", flag);
	msg.msg = msg_buf;

	ret = mbox_send_message(gmu->mailbox.channel, &msg);
	if (ret < 0)
		dev_err(&gmu->pdev->dev,
				"AOP mbox send message failed: %d\n", ret);
}

static void gmu_aop_mailbox_destroy(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);
	struct kgsl_mailbox *mailbox = &gmu->mailbox;

	if (!mailbox->client)
		return;

	mbox_free_channel(mailbox->channel);
	mailbox->channel = NULL;

	kfree(mailbox->client);
	mailbox->client = NULL;

	clear_bit(ADRENO_ACD_CTRL, &adreno_dev->pwrctrl_flag);
}

static int gmu_aop_mailbox_init(struct kgsl_device *device,
		struct gmu_device *gmu)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct kgsl_mailbox *mailbox = &gmu->mailbox;

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

	mutex_lock(&device->mutex);

	/* Power down the GPU before enabling or disabling ACD */
	kgsl_pwrctrl_change_state(device, KGSL_STATE_SUSPEND);

	if (val) {
		set_bit(ADRENO_ACD_CTRL, &adreno_dev->pwrctrl_flag);
		gmu_aop_send_acd_state(device, true);
	} else {
		clear_bit(ADRENO_ACD_CTRL, &adreno_dev->pwrctrl_flag);
		gmu_aop_send_acd_state(device, false);
	}

	kgsl_pwrctrl_change_state(device, KGSL_STATE_SLUMBER);

	mutex_unlock(&device->mutex);
	return 0;
}

static int gmu_tcm_init(struct gmu_device *gmu)
{
	struct gmu_memdesc *md;

	/* Reserve a memdesc for ITCM. No actually memory allocated */
	md = allocate_gmu_kmem(gmu, GMU_ITCM, gmu->vma[GMU_ITCM].start,
			gmu->vma[GMU_ITCM].size, 0);
	if (IS_ERR(md))
		return PTR_ERR(md);

	/* Reserve a memdesc for DTCM. No actually memory allocated */
	md = allocate_gmu_kmem(gmu, GMU_DTCM, gmu->vma[GMU_DTCM].start,
			gmu->vma[GMU_DTCM].size, 0);

	return PTR_ERR_OR_ZERO(md);
}

int gmu_cache_finalize(struct kgsl_device *device)
{
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);
	struct gmu_memdesc *md;

	/* Preallocations were made so no need to request all this memory */
	if (gmu->preallocations)
		return 0;

	md = allocate_gmu_kmem(gmu, GMU_ICACHE,
			gmu->vma[GMU_ICACHE].start, gmu->vma[GMU_ICACHE].size,
			(IOMMU_READ | IOMMU_WRITE | IOMMU_PRIV));
	if (IS_ERR(md))
		return PTR_ERR(md);

	if (!adreno_is_a650_family(ADRENO_DEVICE(device))) {
		md = allocate_gmu_kmem(gmu, GMU_DCACHE,
				gmu->vma[GMU_DCACHE].start,
				gmu->vma[GMU_DCACHE].size,
				(IOMMU_READ | IOMMU_WRITE | IOMMU_PRIV));
		if (IS_ERR(md))
			return PTR_ERR(md);
	}

	md = allocate_gmu_kmem(gmu, GMU_NONCACHED_KERNEL,
			0, DUMMY_SIZE,
			(IOMMU_READ | IOMMU_WRITE | IOMMU_PRIV));
	if (IS_ERR(md))
		return PTR_ERR(md);

	if (ADRENO_FEATURE(ADRENO_DEVICE(device), ADRENO_ECP)) {
		/* Allocation to account for future MEM_ALLOC buffers */
		md = allocate_gmu_kmem(gmu, GMU_NONCACHED_KERNEL,
				0, SZ_32K,
				(IOMMU_READ | IOMMU_WRITE | IOMMU_PRIV));
		if (IS_ERR(md))
			return PTR_ERR(md);
	}

	gmu->preallocations = true;

	return 0;
}

static void gmu_acd_probe(struct kgsl_device *device, struct gmu_device *gmu,
		struct device_node *node)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	struct hfi_acd_table_cmd *cmd = &gmu->hfi.acd_tbl_cmd;
	u32 acd_level, cmd_idx, numlvl = pwr->num_pwrlevels;
	int ret, i;

	if (!ADRENO_FEATURE(ADRENO_DEVICE(device), ADRENO_ACD))
		return;

	cmd->hdr = 0xFFFFFFFF;
	cmd->version = HFI_ACD_INIT_VERSION;
	cmd->stride = 1;
	cmd->enable_by_level = 0;

	for (i = 0, cmd_idx = 0; i < numlvl; i++) {
		acd_level = pwr->pwrlevels[numlvl - i - 1].acd_level;
		if (acd_level) {
			cmd->enable_by_level |= (1 << i);
			cmd->data[cmd_idx++] = acd_level;
		}
	}

	if (!cmd->enable_by_level)
		return;

	cmd->num_levels = cmd_idx;

	ret = gmu_aop_mailbox_init(device, gmu);
	if (ret)
		dev_err(&gmu->pdev->dev,
			"AOP mailbox init failed: %d\n", ret);
}

/* Do not access any GMU registers in GMU probe function */
static int gmu_probe(struct kgsl_device *device, struct device_node *node)
{
	struct gmu_device *gmu;
	struct kgsl_hfi *hfi;
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	int i = 0, ret = -ENXIO;

	gmu = kzalloc(sizeof(struct gmu_device), GFP_KERNEL);

	if (gmu == NULL)
		return -ENOMEM;

	gmu->pdev = of_find_device_by_node(node);
	if (!gmu->pdev) {
		kfree(gmu);
		return -EINVAL;
	}

	device->gmu_core.ptr = (void *)gmu;
	hfi = &gmu->hfi;
	gmu->load_mode = TCM_BOOT;

	of_dma_configure(&gmu->pdev->dev, node, true);

	/* Set up GMU regulators */
	ret = gmu_regulators_probe(gmu, node);
	if (ret)
		goto error;

	/* Set up GMU clocks */
	ret = gmu_clocks_probe(gmu, node);
	if (ret)
		goto error;

	/* Set up GMU IOMMU and shared memory with GMU */
	ret = gmu_iommu_init(gmu, node);
	if (ret)
		goto error;

	if (adreno_is_a650_family(adreno_dev))
		gmu->vma = gmu_vma;
	else
		gmu->vma = gmu_vma_legacy;

	ret = gmu_tcm_init(gmu);
	if (ret)
		goto error;

	/* Map and reserve GMU CSRs registers */
	ret = gmu_reg_probe(device);
	if (ret)
		goto error;

	device->gmu_core.gmu2gpu_offset =
			(gmu->reg_phys - device->reg_phys) >> 2;
	device->gmu_core.reg_len = gmu->reg_len;

	/* Initialize HFI and GMU interrupts */
	ret = kgsl_request_irq(gmu->pdev, "kgsl_hfi_irq",
			hfi_irq_handler, device);
	if (ret < 0)
		goto error;
	hfi->hfi_interrupt_num = ret;

	ret = kgsl_request_irq(gmu->pdev, "kgsl_gmu_irq",
			gmu_irq_handler, device);
	if (ret < 0)
		goto error;
	gmu->gmu_interrupt_num = ret;

	/* Don't enable GMU interrupts until GMU started */
	/* We cannot use irq_disable because it writes registers */
	disable_irq(gmu->gmu_interrupt_num);
	disable_irq(hfi->hfi_interrupt_num);

	tasklet_init(&hfi->tasklet, hfi_receiver, (unsigned long) gmu);
	hfi->kgsldev = device;

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

	gmu_acd_probe(device, gmu, node);

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

	ret = clk_set_rate(gmu->clks[0], GMU_FREQUENCY);
	if (ret) {
		dev_err(&gmu->pdev->dev, "fail to set default GMU clk freq %d\n",
				GMU_FREQUENCY);
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
static int gmu_disable_gdsc(struct gmu_device *gmu)
{
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
		if (!regulator_is_enabled(gmu->cx_gdsc))
			return 0;
		usleep_range(10, 100);

	} while (!(time_after(jiffies, t)));

	if (!regulator_is_enabled(gmu->cx_gdsc))
		return 0;

	dev_err(&gmu->pdev->dev, "GMU CX gdsc off timeout\n");
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

	if (gmu_dev_ops->rpmh_gpu_pwrctrl(device, GMU_SUSPEND, 0, 0))
		return -EINVAL;

	gmu_disable_clks(device);

	if (ADRENO_QUIRK(adreno_dev, ADRENO_QUIRK_CX_GDSC))
		regulator_set_mode(gmu->cx_gdsc, REGULATOR_MODE_IDLE);

	gmu_disable_gdsc(gmu);

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

	adreno_gmu_send_nmi(adreno_dev);
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

static int gmu_init(struct kgsl_device *device)
{
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);
	struct gmu_dev_ops *ops = GMU_DEVICE_OPS(device);
	int ret;

	ret = ops->load_firmware(device);
	if (ret)
		return ret;

	ret = gmu_memory_probe(device);
	if (ret)
		return ret;

	hfi_init(gmu);

	return 0;
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
		gmu_aop_send_acd_state(device, test_bit(ADRENO_ACD_CTRL,
					&adreno_dev->pwrctrl_flag));
	case KGSL_STATE_SUSPEND:
		WARN_ON(test_bit(GMU_CLK_ON, &device->gmu_core.flags));

		gmu_enable_gdsc(gmu);
		gmu_enable_clks(device);
		gmu_dev_ops->irq_enable(device);

		/* Vote for minimal DDR BW for GMU to init */
		ret = msm_bus_scale_client_update_request(gmu->pcl,
				pwr->pwrlevels[pwr->default_pwrlevel].bus_min);
		if (ret)
			dev_err(&gmu->pdev->dev,
				"Failed to allocate gmu b/w: %d\n", ret);

		ret = gmu_dev_ops->rpmh_gpu_pwrctrl(device, GMU_FW_START,
				GMU_COLD_BOOT, 0);
		if (ret)
			goto error_gmu;

		ret = hfi_start(device, gmu, GMU_COLD_BOOT);
		if (ret)
			goto error_gmu;

		/* Request default DCVS level */
		ret = kgsl_pwrctrl_set_default_gpu_pwrlevel(device);
		if (ret)
			goto error_gmu;

		msm_bus_scale_client_update_request(gmu->pcl, 0);
		break;

	case KGSL_STATE_SLUMBER:
		WARN_ON(test_bit(GMU_CLK_ON, &device->gmu_core.flags));

		gmu_enable_gdsc(gmu);
		gmu_enable_clks(device);
		gmu_dev_ops->irq_enable(device);

		ret = gmu_dev_ops->rpmh_gpu_pwrctrl(device, GMU_FW_START,
				GMU_COLD_BOOT, 0);
		if (ret)
			goto error_gmu;

		ret = hfi_start(device, gmu, GMU_COLD_BOOT);
		if (ret)
			goto error_gmu;

		ret = kgsl_pwrctrl_set_default_gpu_pwrlevel(device);
		if (ret)
			goto error_gmu;
		break;

	case KGSL_STATE_RESET:
		gmu_suspend(device);

		gmu_enable_gdsc(gmu);
		gmu_enable_clks(device);
		gmu_dev_ops->irq_enable(device);

		ret = gmu_dev_ops->rpmh_gpu_pwrctrl(
			device, GMU_FW_START, GMU_COLD_BOOT, 0);
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
		gmu_core_dev_oob_clear(device, oob_boot_slumber);
	gmu_core_snapshot(device);
	return ret;
}

/* Caller shall ensure GPU is ready for SLUMBER */
static void gmu_stop(struct kgsl_device *device)
{
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);
	struct gmu_dev_ops *gmu_dev_ops = GMU_DEVICE_OPS(device);
	int ret = 0;

	if (!test_bit(GMU_CLK_ON, &device->gmu_core.flags))
		return;

	/* Wait for the lowest idle level we requested */
	if (gmu_core_dev_wait_for_lowest_idle(device))
		goto error;

	ret = gmu_dev_ops->rpmh_gpu_pwrctrl(device,
			GMU_NOTIFY_SLUMBER, 0, 0);
	if (ret)
		goto error;

	if (gmu_dev_ops->wait_for_gmu_idle &&
			gmu_dev_ops->wait_for_gmu_idle(device))
		goto error;

	/* Pending message in all queues are abandoned */
	gmu_dev_ops->irq_disable(device);
	hfi_stop(gmu);

	gmu_dev_ops->rpmh_gpu_pwrctrl(device, GMU_FW_STOP, 0, 0);
	gmu_disable_clks(device);
	gmu_disable_gdsc(gmu);

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

	return (gmu->gx_gdsc && regulator_is_enabled(gmu->gx_gdsc));
}

struct gmu_core_ops gmu_ops = {
	.probe = gmu_probe,
	.remove = gmu_remove,
	.init = gmu_init,
	.start = gmu_start,
	.stop = gmu_stop,
	.dcvs_set = gmu_dcvs_set,
	.snapshot = gmu_snapshot,
	.regulator_isenabled = gmu_regulator_isenabled,
	.suspend = gmu_suspend,
	.acd_set = gmu_acd_set,
};
