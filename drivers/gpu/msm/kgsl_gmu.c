/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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
#include <linux/types.h>
#include <linux/device.h>
#include <linux/iommu.h>
#include <linux/of_platform.h>
#include <linux/msm-bus.h>
#include <linux/msm-bus-board.h>
#include <linux/pm_opp.h>
#include <linux/io.h>
#include <soc/qcom/cmd-db.h>

#include "kgsl_device.h"
#include "kgsl_gmu.h"
#include "kgsl_hfi.h"
#include "a6xx_reg.h"
#include "adreno.h"

#undef MODULE_PARAM_PREFIX
#define MODULE_PARAM_PREFIX "kgsl_gmu."

static bool nogmu;
module_param(nogmu, bool, 0444);
MODULE_PARM_DESC(nogmu, "Disable the GMU");

#define GMU_CONTEXT_USER		0
#define GMU_CONTEXT_KERNEL		1
#define GMU_KERNEL_ENTRIES		8

enum gmu_iommu_mem_type {
	GMU_CACHED_CODE,
	GMU_CACHED_DATA,
	GMU_NONCACHED_KERNEL,
	GMU_NONCACHED_USER
};

/*
 * GMU virtual memory mapping definitions
 */
struct gmu_vma {
	unsigned int noncached_ustart;
	unsigned int noncached_usize;
	unsigned int noncached_kstart;
	unsigned int noncached_ksize;
	unsigned int cached_dstart;
	unsigned int cached_dsize;
	unsigned int cached_cstart;
	unsigned int cached_csize;
	unsigned int image_start;
};

struct gmu_iommu_context {
	const char *name;
	struct device *dev;
	struct iommu_domain *domain;
};

#define HFIMEM_SIZE SZ_16K

#define DUMPMEM_SIZE SZ_16K

/* Define target specific GMU VMA configurations */
static const struct gmu_vma vma = {
	/* Noncached user segment */
	0x80000000, SZ_1G,
	/* Noncached kernel segment */
	0x60000000, SZ_512M,
	/* Cached data segment */
	0x44000, (SZ_256K-SZ_16K),
	/* Cached code segment */
	0x0, (SZ_256K-SZ_16K),
	/* FW image */
	0x0,
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

/*
 * kgsl_gmu_isenabled() - Check if there is a GMU and it is enabled
 * @device: Pointer to the KGSL device that owns the GMU
 *
 * Check if a GMU has been found and successfully probed. Also
 * check that the feature flag to use a GMU is enabled. Returns
 * true if both of these conditions are met, otherwise false.
 */
bool kgsl_gmu_isenabled(struct kgsl_device *device)
{
	struct gmu_device *gmu = &device->gmu;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	if (!nogmu && gmu->pdev &&
		ADRENO_FEATURE(adreno_dev, ADRENO_GPMU))
		return true;
	return false;
}

static int _gmu_iommu_fault_handler(struct device *dev,
		unsigned long addr, int flags, const char *name)
{
	char *fault_type = "unknown";

	if (flags & IOMMU_FAULT_TRANSLATION)
		fault_type = "translation";
	else if (flags & IOMMU_FAULT_PERMISSION)
		fault_type = "permission";

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

	domain = gmu_ctx[ctx_id].domain;

	md->hostptr = dma_alloc_attrs(&gmu->pdev->dev, (size_t) md->size,
		&md->physaddr, GFP_KERNEL, 0);

	if (md->hostptr == NULL)
		return -ENOMEM;

	ret = iommu_map(domain, md->gmuaddr,
			md->physaddr, md->size,
			attrs);

	if (ret) {
		dev_err(&gmu->pdev->dev,
				"gmu map err: gaddr=0x%016llX, paddr=0x%pa\n",
				md->gmuaddr, &(md->physaddr));
		free_gmu_mem(gmu, md);
	}

	return ret;
}

/*
 * allocate_gmu_image() - allocates & maps memory for FW image, the size
 * shall come from the loaded f/w file. Firmware image size shall be
 * less than code cache size. Otherwise, FW may experience performance issue.
 * @gmu: Pointer to GMU device
 * @size: Requested allocation size
 */
int allocate_gmu_image(struct gmu_device *gmu, unsigned int size)
{
	struct gmu_memdesc *md = &gmu->fw_image;

	if (size > vma.cached_csize) {
		dev_err(&gmu->pdev->dev,
			"GMU firmware size too big: %d\n", size);
		return -EINVAL;
	}

	md->size = size;
	md->gmuaddr = vma.image_start;
	md->attr = GMU_CACHED_CODE;

	return alloc_and_map(gmu, GMU_CONTEXT_KERNEL, md, IOMMU_READ);
}

/*
 * allocate_gmu_kmem() - allocates and maps GMU kernel shared memory
 * @gmu: Pointer to GMU device
 * @size: Requested size
 * @attrs: IOMMU mapping attributes
 */
static struct gmu_memdesc *allocate_gmu_kmem(struct gmu_device *gmu,
		unsigned int size, unsigned int attrs)
{
	struct gmu_memdesc *md;
	int ret, entry_idx = find_first_zero_bit(
			&gmu_kmem_bitmap, GMU_KERNEL_ENTRIES);

	size = PAGE_ALIGN(size);

	if (size > SZ_1M || size == 0) {
		dev_err(&gmu->pdev->dev,
			"Requested %d bytes of GMU kernel memory, max=1MB\n",
			size);
		return ERR_PTR(-EINVAL);
	}

	if (entry_idx >= GMU_KERNEL_ENTRIES) {
		dev_err(&gmu->pdev->dev,
				"Ran out of GMU kernel mempool slots\n");
		return ERR_PTR(-EINVAL);
	}

	/* Allocate GMU virtual memory */
	md = &gmu_kmem_entries[entry_idx];
	md->gmuaddr = vma.noncached_kstart + (entry_idx * SZ_1M);
	set_bit(entry_idx, &gmu_kmem_bitmap);
	md->attr = GMU_NONCACHED_KERNEL;
	md->size = size;

	ret = alloc_and_map(gmu, GMU_CONTEXT_KERNEL, md, attrs);

	if (ret) {
		clear_bit(entry_idx, &gmu_kmem_bitmap);
		md->gmuaddr = 0;
		return ERR_PTR(ret);
	}

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
int gmu_iommu_init(struct gmu_device *gmu, struct device_node *node)
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
void gmu_kmem_close(struct gmu_device *gmu)
{
	int i;
	struct gmu_memdesc *md = &gmu->fw_image;
	struct gmu_iommu_context *ctx = &gmu_ctx[GMU_CONTEXT_KERNEL];

	/* Free GMU image memory */
	free_gmu_mem(gmu, md);

	/* Unmap image memory */
	iommu_unmap(ctx->domain,
			gmu->fw_image.gmuaddr,
			gmu->fw_image.size);


	gmu->hfi_mem = NULL;
	gmu->dump_mem = NULL;

	/* Unmap all memories in GMU kernel memory pool */
	for (i = 0; i < GMU_KERNEL_ENTRIES; i++) {
		struct gmu_memdesc *memptr = &gmu_kmem_entries[i];

		if (memptr->gmuaddr)
			iommu_unmap(ctx->domain, memptr->gmuaddr, memptr->size);
	}

	/* Free GMU shared kernel memory */
	for (i = 0; i < GMU_KERNEL_ENTRIES; i++) {
		md = &gmu_kmem_entries[i];
		free_gmu_mem(gmu, md);
		clear_bit(i, &gmu_kmem_bitmap);
	}

	/* Detach the device from SMMU context bank */
	iommu_detach_device(ctx->domain, ctx->dev);

	/* free kernel mem context */
	iommu_domain_free(ctx->domain);
}

void gmu_memory_close(struct gmu_device *gmu)
{
	gmu_kmem_close(gmu);
	/* Free user memory context */
	iommu_domain_free(gmu_ctx[GMU_CONTEXT_USER].domain);

}

/*
 * gmu_memory_probe() - probe GMU IOMMU context banks and allocate memory
 * to share with GMU in kernel mode.
 * @gmu: Pointer to GMU device
 * @node: Pointer to GMU device node
 */
int gmu_memory_probe(struct gmu_device *gmu, struct device_node *node)
{
	int ret;

	ret = gmu_iommu_init(gmu, node);
	if (ret)
		return ret;

	/* Allocates & maps memory for HFI */
	gmu->hfi_mem = allocate_gmu_kmem(gmu, HFIMEM_SIZE,
				(IOMMU_READ | IOMMU_WRITE));
	if (IS_ERR(gmu->hfi_mem)) {
		ret = PTR_ERR(gmu->hfi_mem);
		goto err_ret;
	}

	/* Allocates & maps GMU crash dump memory */
	gmu->dump_mem = allocate_gmu_kmem(gmu, DUMPMEM_SIZE,
				(IOMMU_READ | IOMMU_WRITE));
	if (IS_ERR(gmu->dump_mem)) {
		ret = PTR_ERR(gmu->dump_mem);
		goto err_ret;
	}

	return 0;
err_ret:
	gmu_memory_close(gmu);
	return ret;
}

/*
 * gmu_dcvs_set() - request GMU to change GPU frequency and/or bandwidth.
 * @gmu: Pointer to GMU device
 * @gpu_pwrlevel: index to GPU DCVS table used by KGSL
 * @bus_level: index to GPU bus table used by KGSL
 *
 * The function converts GPU power level and bus level index used by KGSL
 * to index being used by GMU/RPMh.
 */
int gmu_dcvs_set(struct gmu_device *gmu,
		unsigned int gpu_pwrlevel, unsigned int bus_level)
{
	struct kgsl_device *device = container_of(gmu, struct kgsl_device, gmu);
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	int perf_idx = INVALID_DCVS_IDX, bw_idx = INVALID_DCVS_IDX;
	int ret;

	if (gpu_pwrlevel < gmu->num_gpupwrlevels - 1)
		perf_idx = gmu->num_gpupwrlevels - gpu_pwrlevel - 1;

	if (bus_level < gmu->num_bwlevels && bus_level > 0)
		bw_idx = bus_level;

	if ((perf_idx == INVALID_DCVS_IDX) &&
		(bw_idx == INVALID_DCVS_IDX))
		return -EINVAL;

	if (ADRENO_QUIRK(adreno_dev, ADRENO_QUIRK_HFI_USE_REG)) {
		ret = gpudev->rpmh_gpu_pwrctrl(adreno_dev,
			GMU_DCVS_NOHFI, perf_idx, bw_idx);

		if (ret) {
			dev_err_ratelimited(&gmu->pdev->dev,
				"Failed to set GPU perf idx %d, bw idx %d\n",
				perf_idx, bw_idx);

			adreno_set_gpu_fault(adreno_dev, ADRENO_GMU_FAULT);
			adreno_dispatcher_schedule(device);
		}

		return ret;
	}

	return hfi_send_dcvs_vote(gmu, perf_idx, bw_idx, ACK_NONBLOCK);
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
		if (arc->val[arc->num - 1] >= arc->val[arc->num])
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
static int setup_volt_dependency_tbl(struct arc_vote_desc *votes,
		struct rpmh_arc_vals *pri_rail, struct rpmh_arc_vals *sec_rail,
		unsigned int *vlvl, unsigned int num_entries)
{
	int i, j, k;
	uint16_t cur_vlvl;
	bool found_match;

	/* i tracks current KGSL GPU frequency table entry
	 * j tracks second rail voltage table entry
	 * k tracks primary rail voltage table entry
	 */
	for (i = 0; i < num_entries; i++) {
		found_match = false;

		/* Look for a primary rail voltage that matches a VLVL level */
		for (k = 0; k < pri_rail->num; k++) {
			if (pri_rail->val[k] == vlvl[i]) {
				votes[i].pri_idx = k;
				votes[i].vlvl = vlvl[i];
				cur_vlvl = vlvl[i];
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
					j + 1 == sec_rail->num) {
				votes[i].sec_idx = j;
				break;
			}
		}
	}
	return 0;
}

/*
 * rpmh_arc_votes_init() - initialized RPMh votes needed for rails voltage
 * scaling by GMU.
 * @gmu: Pointer to GMU device
 * @pri_rail: Pointer to primary power rail VLVL table
 * @sec_rail: Pointer to second/dependent power rail VLVL table
 *	of pri_rail VLVL table
 * @type: the type of the primary rail, GPU or GMU
 */
static int rpmh_arc_votes_init(struct gmu_device *gmu,
		struct rpmh_arc_vals *pri_rail,
		struct rpmh_arc_vals *sec_rail,
		unsigned int type)
{
	struct device *dev;
	struct kgsl_device *device = container_of(gmu, struct kgsl_device, gmu);
	unsigned int num_freqs;
	struct arc_vote_desc *votes;
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
		vlvl_tbl[i] = dev_pm_opp_get_voltage(opp) - 1;
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
		return ret;

	build_rpmh_bw_votes(&votes->ddr_votes, gmu->num_bwlevels, hdl);

	/*
	 *Query CNOC TCS command set for each use case defined in cnoc bw table
	 */
	ret = msm_bus_scale_query_tcs_cmd_all(&hdl, gmu->ccl);
	if (ret)
		return ret;

	build_rpmh_bw_votes(&votes->cnoc_votes, gmu->num_cnocbwlevels, hdl);

	kfree(usecases);

	return 0;
}

int gmu_rpmh_init(struct gmu_device *gmu, struct kgsl_pwrctrl *pwr)
{
	struct rpmh_arc_vals gfx_arc, cx_arc, mx_arc;
	int ret;

	/* Populate BW vote table */
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

	ret = rpmh_arc_votes_init(gmu, &gfx_arc, &mx_arc, GPU_ARC_VOTE);
	if (ret)
		return ret;

	return rpmh_arc_votes_init(gmu, &cx_arc, &mx_arc, GMU_ARC_VOTE);
}

static irqreturn_t gmu_irq_handler(int irq, void *data)
{
	struct gmu_device *gmu = data;
	struct kgsl_device *device = container_of(gmu, struct kgsl_device, gmu);
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	unsigned int status = 0;

	adreno_read_gmureg(ADRENO_DEVICE(device),
			ADRENO_REG_GMU_AO_HOST_INTERRUPT_STATUS, &status);
	adreno_write_gmureg(ADRENO_DEVICE(device),
			ADRENO_REG_GMU_AO_HOST_INTERRUPT_CLR, status);

	/* Ignore GMU_INT_RSCC_COMP and GMU_INT_DBD WAKEUP interrupts */
	if (status & GMU_INT_WDOG_BITE) {
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

static irqreturn_t hfi_irq_handler(int irq, void *data)
{
	struct kgsl_hfi *hfi = data;
	struct gmu_device *gmu = container_of(hfi, struct gmu_device, hfi);
	struct kgsl_device *device = container_of(gmu, struct kgsl_device, gmu);
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	unsigned int status = 0;

	adreno_read_gmureg(ADRENO_DEVICE(device),
			ADRENO_REG_GMU_GMU2HOST_INTR_INFO, &status);
	adreno_write_gmureg(ADRENO_DEVICE(device),
			ADRENO_REG_GMU_GMU2HOST_INTR_CLR, status);

	if (status & HFI_IRQ_MSGQ_MASK)
		tasklet_hi_schedule(&hfi->tasklet);
	if (status & HFI_IRQ_CM3_FAULT_MASK) {
		dev_err_ratelimited(&gmu->pdev->dev,
				"GMU CM3 fault interrupt received\n");
		adreno_set_gpu_fault(adreno_dev, ADRENO_GMU_FAULT);
		adreno_dispatcher_schedule(device);
	}
	if (status & ~HFI_IRQ_MASK)
		dev_err_ratelimited(&gmu->pdev->dev,
				"Unhandled HFI interrupts 0x%lx\n",
				status & ~HFI_IRQ_MASK);

	return IRQ_HANDLED;
}

static int gmu_pwrlevel_probe(struct gmu_device *gmu, struct device_node *node)
{
	struct device_node *pwrlevel_node, *child;

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

static int gmu_reg_probe(struct gmu_device *gmu, const char *name, bool is_gmu)
{
	struct resource *res;

	res = platform_get_resource_byname(gmu->pdev, IORESOURCE_MEM, name);
	if (res == NULL) {
		dev_err(&gmu->pdev->dev,
				"platform_get_resource %s failed\n", name);
		return -EINVAL;
	}

	if (res->start == 0 || resource_size(res) == 0) {
		dev_err(&gmu->pdev->dev,
				"dev %d %s invalid register region\n",
				gmu->pdev->dev.id, name);
		return -EINVAL;
	}

	if (is_gmu) {
		gmu->reg_phys = res->start;
		gmu->reg_len = resource_size(res);
		gmu->reg_virt = devm_ioremap(&gmu->pdev->dev, res->start,
				resource_size(res));

		if (gmu->reg_virt == NULL) {
			dev_err(&gmu->pdev->dev, "GMU regs ioremap failed\n");
			return -ENODEV;
		}

	} else {
		gmu->pdc_reg_virt = devm_ioremap(&gmu->pdev->dev, res->start,
				resource_size(res));
		if (gmu->pdc_reg_virt == NULL) {
			dev_err(&gmu->pdev->dev, "PDC regs ioremap failed\n");
			return -ENODEV;
		}
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

static int gmu_gpu_bw_probe(struct gmu_device *gmu)
{
	struct kgsl_device *device = container_of(gmu, struct kgsl_device, gmu);
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

static int gmu_irq_probe(struct gmu_device *gmu)
{
	int ret;
	struct kgsl_hfi *hfi = &gmu->hfi;

	hfi->hfi_interrupt_num = platform_get_irq_byname(gmu->pdev,
			"kgsl_hfi_irq");
	ret = devm_request_irq(&gmu->pdev->dev,
			hfi->hfi_interrupt_num,
			hfi_irq_handler, IRQF_TRIGGER_HIGH,
			"HFI", hfi);
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
			"GMU", gmu);
	if (ret)
		dev_err(&gmu->pdev->dev, "request_irq(%d) failed: %d\n",
				gmu->gmu_interrupt_num, ret);

	return ret;
}

static void gmu_irq_enable(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct gmu_device *gmu = &device->gmu;
	struct kgsl_hfi *hfi = &gmu->hfi;

	/* Clear any pending IRQs before unmasking on GMU */
	adreno_write_gmureg(adreno_dev, ADRENO_REG_GMU_GMU2HOST_INTR_CLR,
			0xFFFFFFFF);
	adreno_write_gmureg(adreno_dev, ADRENO_REG_GMU_AO_HOST_INTERRUPT_CLR,
			0xFFFFFFFF);

	/* Unmask needed IRQs on GMU */
	adreno_write_gmureg(adreno_dev, ADRENO_REG_GMU_GMU2HOST_INTR_MASK,
			(unsigned int) ~HFI_IRQ_MASK);
	adreno_write_gmureg(adreno_dev, ADRENO_REG_GMU_AO_HOST_INTERRUPT_MASK,
			(unsigned int) ~GMU_AO_INT_MASK);

	/* Enable all IRQs on host */
	enable_irq(hfi->hfi_interrupt_num);
	enable_irq(gmu->gmu_interrupt_num);
}

static void gmu_irq_disable(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct gmu_device *gmu = &device->gmu;
	struct kgsl_hfi *hfi = &gmu->hfi;

	/* Disable all IRQs on host */
	disable_irq(gmu->gmu_interrupt_num);
	disable_irq(hfi->hfi_interrupt_num);

	/* Mask all IRQs on GMU */
	adreno_write_gmureg(adreno_dev, ADRENO_REG_GMU_AO_HOST_INTERRUPT_MASK,
			0xFFFFFFFF);
	adreno_write_gmureg(adreno_dev, ADRENO_REG_GMU_GMU2HOST_INTR_MASK,
			0xFFFFFFFF);

	/* Clear any pending IRQs before disabling */
	adreno_write_gmureg(adreno_dev, ADRENO_REG_GMU_AO_HOST_INTERRUPT_CLR,
			0xFFFFFFFF);
	adreno_write_gmureg(adreno_dev, ADRENO_REG_GMU_GMU2HOST_INTR_CLR,
			0xFFFFFFFF);
}

/* Do not access any GMU registers in GMU probe function */
int gmu_probe(struct kgsl_device *device)
{
	struct device_node *node;
	struct gmu_device *gmu = &device->gmu;
	struct gmu_memdesc *mem_addr = NULL;
	struct kgsl_hfi *hfi = &gmu->hfi;
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	int i = 0, ret = -ENXIO;

	node = of_find_compatible_node(device->pdev->dev.of_node,
			NULL, "qcom,gpu-gmu");

	if (node == NULL)
		return ret;

	device->gmu.pdev = of_find_device_by_node(node);

	/* Set up GMU regulators */
	ret = gmu_regulators_probe(gmu, node);
	if (ret)
		goto error;

	/* Set up GMU clocks */
	ret = gmu_clocks_probe(gmu, node);
	if (ret)
		goto error;

	/* Set up GMU IOMMU and shared memory with GMU */
	ret = gmu_memory_probe(&device->gmu, node);
	if (ret)
		goto error;
	mem_addr = gmu->hfi_mem;

	/* Map and reserve GMU CSRs registers */
	ret = gmu_reg_probe(gmu, "kgsl_gmu_reg", true);
	if (ret)
		goto error;

	ret = gmu_reg_probe(gmu, "kgsl_gmu_pdc_reg", false);
	if (ret)
		goto error;

	gmu->gmu2gpu_offset = (gmu->reg_phys - device->reg_phys) >> 2;

	/* Initialize HFI and GMU interrupts */
	ret = gmu_irq_probe(gmu);
	if (ret)
		goto error;

	/* Don't enable GMU interrupts until GMU started */
	/* We cannot use gmu_irq_disable because it writes registers */
	disable_irq(gmu->gmu_interrupt_num);
	disable_irq(hfi->hfi_interrupt_num);

	tasklet_init(&hfi->tasklet, hfi_receiver, (unsigned long)gmu);
	INIT_LIST_HEAD(&hfi->msglist);
	spin_lock_init(&hfi->msglock);

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
	ret = gmu_gpu_bw_probe(gmu);
	if (ret)
		goto error;

	/* Initialize GMU CNOC b/w levels configuration */
	ret = gmu_cnoc_bw_probe(gmu);
	if (ret)
		goto error;

	/* Populates RPMh configurations */
	ret = gmu_rpmh_init(gmu, pwr);
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

	/* disable LM during boot time */
	clear_bit(ADRENO_LM_CTRL, &adreno_dev->pwrctrl_flag);
	return 0;

error:
	gmu_remove(device);
	return ret;
}



static int gmu_enable_clks(struct gmu_device *gmu)
{
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

	set_bit(GMU_CLK_ON, &gmu->flags);
	return 0;
}

static int gmu_disable_clks(struct gmu_device *gmu)
{
	int j = 0;

	if (IS_ERR_OR_NULL(gmu->clks[0]))
		return 0;

	while ((j < MAX_GMU_CLKS) && gmu->clks[j]) {
		clk_disable_unprepare(gmu->clks[j]);
		j++;
	}

	clear_bit(GMU_CLK_ON, &gmu->flags);
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

	dev_err(&gmu->pdev->dev, "GMU CX gdsc off timeout");
	return -ETIMEDOUT;
}

int gmu_suspend(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	struct gmu_device *gmu = &device->gmu;

	if (!test_bit(GMU_CLK_ON, &gmu->flags))
		return 0;

	/* Pending message in all queues are abandoned */
	hfi_stop(gmu);
	clear_bit(GMU_HFI_ON, &gmu->flags);
	gmu_irq_disable(device);

	if (gpudev->rpmh_gpu_pwrctrl(adreno_dev, GMU_SUSPEND, 0, 0))
		return -EINVAL;

	gmu_disable_clks(gmu);
	gmu_disable_gdsc(gmu);
	dev_err(&gmu->pdev->dev, "Suspended GMU\n");
	return 0;
}

void gmu_snapshot(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct gmu_device *gmu = &device->gmu;

	/* Mask so there's no interrupt caused by NMI */
	adreno_write_gmureg(adreno_dev,
			ADRENO_REG_GMU_GMU2HOST_INTR_MASK, 0xFFFFFFFF);

	/* Make sure the interrupt is masked before causing it */
	wmb();
	adreno_write_gmureg(adreno_dev,
		ADRENO_REG_GMU_NMI_CONTROL_STATUS, 0);
	adreno_write_gmureg(adreno_dev,
		ADRENO_REG_GMU_CM3_CFG, (1 << 9));

	/* Wait for the NMI to be handled */
	wmb();
	udelay(100);
	kgsl_device_snapshot(device, NULL, true);

	adreno_write_gmureg(adreno_dev,
			ADRENO_REG_GMU_GMU2HOST_INTR_CLR, 0xFFFFFFFF);
	adreno_write_gmureg(adreno_dev,
			ADRENO_REG_GMU_GMU2HOST_INTR_MASK,
			(unsigned int) ~HFI_IRQ_MASK);

	gmu->fault_count++;
}

static void gmu_change_gpu_pwrlevel(struct kgsl_device *device,
	unsigned int new_level) {

	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	unsigned int old_level = pwr->active_pwrlevel;

	/*
	 * Update the level according to any thermal,
	 * max/min, or power constraints.
	 */
	new_level = kgsl_pwrctrl_adjust_pwrlevel(device, new_level);

	/*
	 * If thermal cycling is required and the new level hits the
	 * thermal limit, kick off the cycling.
	 */
	kgsl_pwrctrl_set_thermal_cycle(device, new_level);

	pwr->active_pwrlevel = new_level;
	pwr->previous_pwrlevel = old_level;

	/* Request adjusted DCVS level */
	kgsl_clk_set_rate(device, pwr->active_pwrlevel);
}

/* To be called to power on both GPU and GMU */
int gmu_start(struct kgsl_device *device)
{
	int ret = 0;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	struct gmu_device *gmu = &device->gmu;
	unsigned int boot_state = GMU_WARM_BOOT;

	switch (device->state) {
	case KGSL_STATE_INIT:
	case KGSL_STATE_SUSPEND:
		WARN_ON(test_bit(GMU_CLK_ON, &gmu->flags));
		gmu_enable_gdsc(gmu);
		gmu_enable_clks(gmu);
		gmu_irq_enable(device);

		/* Vote for 300MHz DDR for GMU to init */
		ret = msm_bus_scale_client_update_request(gmu->pcl,
				pwr->pwrlevels[pwr->default_pwrlevel].bus_freq);
		if (ret)
			dev_err(&gmu->pdev->dev,
				"Failed to allocate gmu b/w: %d\n", ret);

		ret = gpudev->rpmh_gpu_pwrctrl(adreno_dev, GMU_FW_START,
				GMU_COLD_BOOT, 0);
		if (ret)
			goto error_gmu;

		ret = hfi_start(gmu, GMU_COLD_BOOT);
		if (ret)
			goto error_gmu;

		/* Request default DCVS level */
		gmu_change_gpu_pwrlevel(device, pwr->default_pwrlevel);
		msm_bus_scale_client_update_request(gmu->pcl, 0);
		break;

	case KGSL_STATE_SLUMBER:
		WARN_ON(test_bit(GMU_CLK_ON, &gmu->flags));
		gmu_enable_gdsc(gmu);
		gmu_enable_clks(gmu);
		gmu_irq_enable(device);

		/*
		 * If unrecovered is set that means last
		 * wakeup from SLUMBER state failed. Use GMU
		 * and HFI boot state as COLD as this is a
		 * boot after RESET.
		 */
		if (gmu->unrecovered)
			boot_state = GMU_COLD_BOOT;

		ret = gpudev->rpmh_gpu_pwrctrl(adreno_dev, GMU_FW_START,
				boot_state, 0);
		if (ret)
			goto error_gmu;

		ret = hfi_start(gmu, boot_state);
		if (ret)
			goto error_gmu;

		gmu_change_gpu_pwrlevel(device, pwr->default_pwrlevel);
		break;

	case KGSL_STATE_RESET:
		if (test_bit(ADRENO_DEVICE_HARD_RESET, &adreno_dev->priv) ||
			test_bit(GMU_FAULT, &gmu->flags)) {
			gmu_suspend(device);
			gmu_enable_gdsc(gmu);
			gmu_enable_clks(gmu);
			gmu_irq_enable(device);

			ret = gpudev->rpmh_gpu_pwrctrl(
				adreno_dev, GMU_FW_START, GMU_COLD_BOOT, 0);
			if (ret)
				goto error_gmu;


			ret = hfi_start(gmu, GMU_COLD_BOOT);
			if (ret)
				goto error_gmu;

			/* Send DCVS level prior to reset*/
			gmu_change_gpu_pwrlevel(device,
				pwr->default_pwrlevel);
		} else {
			/* GMU fast boot */
			hfi_stop(gmu);

			ret = gpudev->rpmh_gpu_pwrctrl(adreno_dev, GMU_FW_START,
					GMU_COLD_BOOT, 0);
			if (ret)
				goto error_gmu;

			ret = hfi_start(gmu, GMU_WARM_BOOT);
			if (ret)
				goto error_gmu;
		}
		break;
	default:
		break;
	}

	/* Clear unrecovered as GMU start is successful */
	gmu->unrecovered = false;
	return ret;

error_gmu:
	if (ADRENO_QUIRK(adreno_dev, ADRENO_QUIRK_HFI_USE_REG))
		gpudev->oob_clear(adreno_dev,
				OOB_BOOT_SLUMBER_CLEAR_MASK);
	gmu_snapshot(device);
	return ret;
}

/* Caller shall ensure GPU is ready for SLUMBER */
void gmu_stop(struct kgsl_device *device)
{
	struct gmu_device *gmu = &device->gmu;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	int ret = 0;

	if (!test_bit(GMU_CLK_ON, &gmu->flags))
		return;

	/* Wait for the lowest idle level we requested */
	if (gpudev->wait_for_lowest_idle &&
			gpudev->wait_for_lowest_idle(adreno_dev))
		goto error;

	ret = gpudev->rpmh_gpu_pwrctrl(adreno_dev, GMU_NOTIFY_SLUMBER, 0, 0);
	if (ret)
		goto error;

	if (gpudev->wait_for_gmu_idle &&
			gpudev->wait_for_gmu_idle(adreno_dev))
		goto error;

	/* Pending message in all queues are abandoned */
	hfi_stop(gmu);
	clear_bit(GMU_HFI_ON, &gmu->flags);
	gmu_irq_disable(device);

	gpudev->rpmh_gpu_pwrctrl(adreno_dev, GMU_FW_STOP, 0, 0);
	gmu_disable_clks(gmu);
	gmu_disable_gdsc(gmu);

	msm_bus_scale_client_update_request(gmu->pcl, 0);
	return;

error:
	/*
	 * The power controller will change state to SLUMBER anyway
	 * Set GMU_FAULT flag to indicate to power contrller
	 * that hang recovery is needed to power on GPU
	 */
	set_bit(GMU_FAULT, &gmu->flags);
	dev_err(&gmu->pdev->dev, "Failed to stop GMU\n");
	gmu_snapshot(device);
}

void gmu_remove(struct kgsl_device *device)
{
	struct gmu_device *gmu = &device->gmu;
	struct kgsl_hfi *hfi = &gmu->hfi;
	int i = 0;

	if (!device->gmu.pdev)
		return;

	tasklet_kill(&hfi->tasklet);

	gmu_stop(device);
	gmu_irq_disable(device);

	while ((i < MAX_GMU_CLKS) && gmu->clks[i]) {
		gmu->clks[i] = NULL;
		i++;
	}

	if (gmu->gmu_interrupt_num) {
		devm_free_irq(&gmu->pdev->dev,
				gmu->gmu_interrupt_num, gmu);
		gmu->gmu_interrupt_num = 0;
	}

	if (hfi->hfi_interrupt_num) {
		devm_free_irq(&gmu->pdev->dev,
				hfi->hfi_interrupt_num, hfi);
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

	if (gmu->pdc_reg_virt) {
		devm_iounmap(&gmu->pdev->dev, gmu->pdc_reg_virt);
		gmu->pdc_reg_virt = NULL;
	}

	if (gmu->reg_virt) {
		devm_iounmap(&gmu->pdev->dev, gmu->reg_virt);
		gmu->reg_virt = NULL;
	}

	if (gmu->hfi_mem || gmu->dump_mem)
		gmu_memory_close(&device->gmu);

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

	device->gmu.pdev = NULL;
}

/*
 * adreno_gmu_fenced_write() - Check if there is a GMU and it is enabled
 * @adreno_dev: Pointer to the Adreno device device that owns the GMU
 * @offset: 32bit register enum that is to be written
 * @val: The value to be written to the register
 * @fence_mask: The value to poll the fence status register
 *
 * Check the WRITEDROPPED0/1 bit in the FENCE_STATUS regsiter to check if
 * the write to the fenced register went through. If it didn't then we retry
 * the write until it goes through or we time out.
 */
int adreno_gmu_fenced_write(struct adreno_device *adreno_dev,
		enum adreno_regs offset, unsigned int val,
		unsigned int fence_mask)
{
	unsigned int status, i;
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	unsigned int reg_offset = gpudev->reg_offsets->offsets[offset];

	adreno_writereg(adreno_dev, offset, val);

	if (!kgsl_gmu_isenabled(KGSL_DEVICE(adreno_dev)))
		return 0;

	for (i = 0; i < GMU_WAKEUP_RETRY_MAX; i++) {
		adreno_read_gmureg(adreno_dev, ADRENO_REG_GMU_AHB_FENCE_STATUS,
			&status);

		/*
		 * If !writedropped0/1, then the write to fenced register
		 * was successful
		 */
		if (!(status & fence_mask))
			return 0;
		/* Wait a small amount of time before trying again */
		udelay(GMU_WAKEUP_DELAY_US);

		/* Try to write the fenced register again */
		adreno_writereg(adreno_dev, offset, val);
	}

	dev_err(adreno_dev->dev.dev,
		"GMU fenced register write timed out: reg 0x%x\n", reg_offset);
	return -ETIMEDOUT;
}
