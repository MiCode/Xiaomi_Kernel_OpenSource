/*
 * Copyright (c) 2017, The Linux Foundation. All rights reserved.
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
#include <linux/of_address.h>
#include <linux/debugfs.h>
#include <linux/memblock.h>

#include "msm_drv.h"
#include "msm_mmu.h"
#include "sde_kms.h"
#include "sde_hw_mdss.h"
#include "sde_hw_util.h"
#include "sde_hw_intf.h"
#include "sde_hw_catalog.h"

#define MDP_SSPP_TOP0_OFF		0x1000
#define DISP_INTF_SEL			0x004
#define SPLIT_DISPLAY_EN		0x2F4

/* scratch registers */
#define SCRATCH_REGISTER_0		0x014
#define SCRATCH_REGISTER_1		0x018
#define SCRATCH_REGISTER_2		0x01C

#define SDE_LK_RUNNING_VALUE		0xC001CAFE
#define SDE_LK_SHUT_DOWN_VALUE		0xDEADDEAD
#define SDE_LK_EXIT_VALUE		0xDEADBEEF

/*
 * In order to free reseved memory from bootup, and we are not
 * able to call the __init free functions, so we need to free
 * this memory by ourselves using the free_reserved_page() function.
 */
static int sde_splash_release_bootup_memory(phys_addr_t phys, size_t size)
{
	unsigned long pfn_start, pfn_end, pfn_idx;

	pfn_start = phys >> PAGE_SHIFT;
	pfn_end = (phys + size) >> PAGE_SHIFT;

	for (pfn_idx = pfn_start; pfn_idx < pfn_end; pfn_idx++)
		free_reserved_page(pfn_to_page(pfn_idx));

	return 0;
}

int sde_splash_reserve_memory(phys_addr_t phys, size_t size)
{
	return memblock_reserve(phys, size);
}

int sde_splash_parse_dt(struct drm_device *dev)
{
	struct msm_drm_private *priv = dev->dev_private;
	unsigned long size = 0;
	dma_addr_t start;
	struct device_node *node;
	struct sde_kms *sde_kms = to_sde_kms(priv->kms);
	struct sde_splash_info *sinfo;
	int ret = 0, i = 0, len = 0;

	sinfo = &sde_kms->splash_info;
	if (!sinfo)
		return -EINVAL;

	if (of_get_property(dev->dev->of_node, "contiguous-region", &len))
		sinfo->splash_mem_num = len/sizeof(u32);
	else
		sinfo->splash_mem_num = 0;

	sinfo->splash_mem_paddr =
			kmalloc(sizeof(phys_addr_t) * sinfo->splash_mem_num,
				GFP_KERNEL);

	sinfo->splash_mem_size =
			kmalloc(sizeof(size_t) * sinfo->splash_mem_num,
				GFP_KERNEL);
	if (!sinfo->splash_mem_paddr || !sinfo->splash_mem_size)
		return -ENOMEM;


	sinfo->obj = kmalloc(sizeof(struct drm_gem_object *) *
				sinfo->splash_mem_num, GFP_KERNEL);
	if (!sinfo->obj)
		return -ENOMEM;

	for (i = 0; i < sinfo->splash_mem_num; i++) {
		node = of_parse_phandle(dev->dev->of_node,
					"contiguous-region", i);

		if (node) {
			struct resource r;

			ret = of_address_to_resource(node, 0, &r);
			if (ret)
				return ret;

			size = r.end - r.start;
			start = (dma_addr_t)r.start;

			sinfo->splash_mem_paddr[i] = start;
			sinfo->splash_mem_size[i] = size;

			DRM_INFO("blk: %d, addr:%pK, size:%pK\n",
				i, (void *)sinfo->splash_mem_paddr[i],
				(void *)sinfo->splash_mem_size[i]);
		}

		of_node_put(node);
	}

	return ret;
}

int sde_splash_get_handoff_status(struct msm_kms *kms)
{
	uint32_t intf_sel = 0;
	uint32_t split_display = 0;
	uint32_t num_of_display_on = 0;
	uint32_t i = 0;
	struct sde_kms *sde_kms = to_sde_kms(kms);
	struct sde_rm *rm;
	struct sde_hw_blk_reg_map *c;
	struct sde_splash_info *sinfo;
	struct sde_mdss_cfg *catalog;

	sinfo = &sde_kms->splash_info;
	if (!sinfo) {
		SDE_ERROR("%s(%d): invalid splash info\n",
				__func__, __LINE__);
		return -EINVAL;
	}

	rm = &sde_kms->rm;

	if (!rm || !rm->hw_mdp) {
		SDE_ERROR("invalid rm.\n");
		return -EINVAL;
	}

	c = &rm->hw_mdp->hw;
	if (c) {
		intf_sel = SDE_REG_READ(c, DISP_INTF_SEL);
		split_display = SDE_REG_READ(c, SPLIT_DISPLAY_EN);
	}

	catalog = sde_kms->catalog;

	if (intf_sel != 0) {
		for (i = 0; i < catalog->intf_count; i++)
			if ((intf_sel >> i*8) & 0x000000FF)
				num_of_display_on++;

		/*
		 * For split display enabled - DSI0, DSI1 interfaces are
		 * considered as single display. So decrement
		 * 'num_of_display_on' by 1
		 */
		if (split_display)
			num_of_display_on--;
	}

	if (num_of_display_on) {
		sinfo->handoff = true;
		sinfo->program_scratch_regs = true;
	} else {
		sinfo->handoff = false;
		sinfo->program_scratch_regs = false;
	}

	return 0;
}

static bool sde_splash_lk_check(struct sde_hw_intr *intr)
{
	return (SDE_LK_RUNNING_VALUE == SDE_REG_READ(&intr->hw,
			SCRATCH_REGISTER_1)) ? true : false;
}

static int sde_splash_notify_lk_exit(struct sde_hw_intr *intr)
{
	int i = 0;

	/* first is to write exit signal to scratch register*/
	SDE_REG_WRITE(&intr->hw, SCRATCH_REGISTER_1, SDE_LK_SHUT_DOWN_VALUE);

	while ((SDE_LK_EXIT_VALUE !=
		SDE_REG_READ(&intr->hw, SCRATCH_REGISTER_1)) && (i++ < 20)) {
		DRM_INFO("wait for LK's exit");
		msleep(20);
	}

	return 0;
}

static int sde_splash_gem_new(struct drm_device *dev,
				struct sde_splash_info *sinfo)
{
	int i, ret;

	for (i = 0; i < sinfo->splash_mem_num; i++) {
		mutex_lock(&dev->struct_mutex);
		sinfo->obj[i] = msm_gem_new(dev,
				sinfo->splash_mem_size[i], MSM_BO_UNCACHED);
		mutex_unlock(&dev->struct_mutex);

		if (IS_ERR(sinfo->obj[i])) {
			ret = PTR_ERR(sinfo->obj[i]);
			SDE_ERROR("failed to allocate gem, ret=%d\n", ret);
			goto error;
		}
	}

	return 0;

error:
	for (i = 0; i < sinfo->splash_mem_num; i++) {
		if (sinfo->obj[i])
			msm_gem_free_object(sinfo->obj[i]);
		sinfo->obj[i] = NULL;
	}

	return ret;
}

static int sde_splash_get_pages(struct drm_gem_object *obj, phys_addr_t phys)
{
	struct msm_gem_object *msm_obj = to_msm_bo(obj);
	struct page **p;
	dma_addr_t paddr;
	int npages = obj->size >> PAGE_SHIFT;
	int i;

	p = drm_malloc_ab(npages, sizeof(struct page *));
	if (!p)
		return -ENOMEM;

	paddr = phys;

	for (i = 0; i < npages; i++) {
		p[i] = phys_to_page(paddr);
		paddr += PAGE_SIZE;
	}

	msm_obj->sgt = drm_prime_pages_to_sg(p, npages);
	if (IS_ERR(msm_obj->sgt)) {
		SDE_ERROR("failed to allocate sgt\n");
		return -ENOMEM;
	}

	msm_obj->pages = p;

	return 0;
}

int sde_splash_destroy(struct sde_splash_info *sinfo)
{
	int i = 0;
	struct msm_gem_object *msm_obj;

	kfree(sinfo->splash_mem_paddr);
	sinfo->splash_mem_paddr = NULL;

	kfree(sinfo->splash_mem_size);
	sinfo->splash_mem_size = NULL;

	for (i = 0; i < sinfo->splash_mem_num; i++) {
		msm_obj = to_msm_bo(sinfo->obj[i]);
		if (msm_obj->pages) {
			sg_free_table(msm_obj->sgt);
			kfree(msm_obj->sgt);
			drm_free_large(msm_obj->pages);
			msm_obj->pages = NULL;
		}
	}

	return 0;
}

int sde_splash_smmu_map(struct drm_device *dev, struct msm_mmu *mmu,
			struct sde_splash_info *sinfo)
{
	struct msm_gem_object *msm_obj;
	int i = 0, ret = 0;

	if (!mmu || !sinfo)
		return -EINVAL;

	/* first is to construct drm_gem_objects for splash memory */
	if (sde_splash_gem_new(dev, sinfo))
		return -ENOMEM;

	/* second is to contruct sgt table for calling smmu map */
	for (i = 0; i < sinfo->splash_mem_num; i++) {
		if (sde_splash_get_pages(sinfo->obj[i],
				sinfo->splash_mem_paddr[i]))
			return -ENOMEM;
	}

	for (i = 0; i < sinfo->splash_mem_num; i++) {
		msm_obj = to_msm_bo(sinfo->obj[i]);

		if (mmu->funcs && mmu->funcs->map) {
			ret = mmu->funcs->map(mmu, sinfo->splash_mem_paddr[i],
				msm_obj->sgt, IOMMU_READ | IOMMU_NOEXEC);

			if (ret) {
				SDE_ERROR("Map blk %d @%pK failed.\n",
					i, (void *)sinfo->splash_mem_paddr[i]);
				return ret;
			}
		}
	}

	return ret;
}

int sde_splash_smmu_unmap(struct msm_mmu *mmu, struct sde_splash_info *sinfo)
{
	struct msm_gem_object *msm_obj;
	int i = 0, ret = 0;

	for (i = 0; i < sinfo->splash_mem_num; i++) {
		msm_obj = to_msm_bo(sinfo->obj[i]);

		if (mmu->funcs && mmu->funcs->unmap) {
			ret = mmu->funcs->unmap(mmu,
			sinfo->splash_mem_paddr[i], msm_obj->sgt);

			/* We need to try the best efforts to unmap
			 * the memory, so even if unmap fails, we need
			 * to continue the reset unmap loop.
			 */
			if (ret)
				SDE_ERROR("Unmap blk %d @%pK failed.\n",
					i, (void *)sinfo->splash_mem_paddr[i]);
		}
	}

	return ret;
}

/*
 * In below cleanup, the steps are:
 * 1. Notify LK to exit and wait for exiting is done.
 * 2. Ummap the memory.
 * 3. Set DOMAIN_ATTR_EARLY_MAP to 1 to enable stage 1 translation in iommu.
 * 4. Free the reserved memory used by LK.
 */
int sde_splash_clean_up(struct msm_kms *kms)
{
	struct sde_splash_info *sinfo;
	struct msm_mmu *mmu;
	struct sde_kms *sde_kms = to_sde_kms(kms);
	int ret;
	int i = 0;

	sinfo = &sde_kms->splash_info;

	if (!sinfo) {
		SDE_ERROR("%s(%d): invalid splash info\n", __func__, __LINE__);
		return -EINVAL;
	}

	/* Monitor LK's status and tell it to exit. */
	if (sinfo->program_scratch_regs) {
		if (sde_splash_lk_check(sde_kms->hw_intr))
			sde_splash_notify_lk_exit(sde_kms->hw_intr);

		sinfo->handoff = false;
		sinfo->program_scratch_regs = false;
	}

	if (!sde_kms->aspace[0] || !sde_kms->aspace[0]->mmu) {
		/* We do not return fault value here, to ensure
		 * memory can be freed to system later.
		 */
		SDE_ERROR("invalid mmu\n");
		WARN_ON(1);
	} else {
		mmu = sde_kms->aspace[0]->mmu;

		/* After LK has exited, set early domain map attribute
		 * to 1 to enable stage 1 translation in iommu driver.
		 */
		if (mmu->funcs && mmu->funcs->set_property) {
			ret = mmu->funcs->set_property(mmu,
				DOMAIN_ATTR_EARLY_MAP, &sinfo->handoff);

			if (ret)
				SDE_ERROR("set_property failed\n");
		}
	}

	/* release reserved memory to syetem for other allocations */
	for (i = 0; i < sinfo->splash_mem_num; i++) {
		memblock_free(sinfo->splash_mem_paddr[i],
				sinfo->splash_mem_size[i]);

		sde_splash_release_bootup_memory(sinfo->splash_mem_paddr[i],
					sinfo->splash_mem_size[i]);
	}

	/* free splash obejcts */
	sde_splash_destroy(sinfo);

	return 0;
}
