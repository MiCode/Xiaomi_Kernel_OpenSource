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
#include "dsi_display.h"

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

#define SDE_LK_EXIT_MAX_LOOP		20

static DEFINE_MUTEX(sde_splash_lock);

/*
 * In order to free reseved memory from bootup, and we are not
 * able to call the __init free functions, so we need to free
 * this memory by ourselves using the free_reserved_page() function.
 */
static void _sde_splash_free_bootup_memory_to_system(phys_addr_t phys,
						size_t size)
{
	unsigned long pfn_start, pfn_end, pfn_idx;

	memblock_free(phys, size);

	pfn_start = phys >> PAGE_SHIFT;
	pfn_end = (phys + size) >> PAGE_SHIFT;

	for (pfn_idx = pfn_start; pfn_idx < pfn_end; pfn_idx++)
		free_reserved_page(pfn_to_page(pfn_idx));
}

static int _sde_splash_parse_dt_get_lk_pool_node(struct drm_device *dev,
					struct sde_splash_info *sinfo)
{
	struct device_node *parent, *node;
	struct resource r;
	int ret = 0;

	if (!sinfo)
		return -EINVAL;

	parent = of_find_node_by_path("/reserved-memory");
	if (!parent)
		return -EINVAL;

	node = of_find_node_by_name(parent, "lk_pool");
	if (!node) {
		SDE_ERROR("mem reservation for lk_pool is not presented\n");
		ret = -EINVAL;
		goto parent_node_err;
	}

	/* find the mode */
	if (of_address_to_resource(node, 0, &r)) {
		ret = -EINVAL;
		goto child_node_err;
	}

	sinfo->lk_pool_paddr = (dma_addr_t)r.start;
	sinfo->lk_pool_size = r.end - r.start;

	DRM_INFO("lk_pool: addr:%pK, size:%pK\n",
			(void *)sinfo->lk_pool_paddr,
			(void *)sinfo->lk_pool_size);

child_node_err:
	of_node_put(node);

parent_node_err:
	of_node_put(parent);

	return ret;
}

static int _sde_splash_parse_dt_get_display_node(struct drm_device *dev,
					struct sde_splash_info *sinfo)
{
	unsigned long size = 0;
	dma_addr_t start;
	struct device_node *node;
	int ret = 0, i = 0, len = 0;

	/* get reserved memory for display module */
	if (of_get_property(dev->dev->of_node, "contiguous-region", &len))
		sinfo->splash_mem_num = len / sizeof(u32);
	else
		sinfo->splash_mem_num = 0;

	sinfo->splash_mem_paddr =
			kmalloc(sizeof(phys_addr_t) * sinfo->splash_mem_num,
				GFP_KERNEL);
	if (!sinfo->splash_mem_paddr) {
		SDE_ERROR("alloc splash_mem_paddr failed\n");
		return -ENOMEM;
	}

	sinfo->splash_mem_size =
			kmalloc(sizeof(size_t) * sinfo->splash_mem_num,
				GFP_KERNEL);
	if (!sinfo->splash_mem_size) {
		SDE_ERROR("alloc splash_mem_size failed\n");
		goto error;
	}

	sinfo->obj = kmalloc(sizeof(struct drm_gem_object *) *
				sinfo->splash_mem_num, GFP_KERNEL);
	if (!sinfo->obj) {
		SDE_ERROR("construct splash gem objects failed\n");
		goto error;
	}

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

			of_node_put(node);
		}
	}

	return ret;

error:
	kfree(sinfo->splash_mem_paddr);
	sinfo->splash_mem_paddr = NULL;

	kfree(sinfo->splash_mem_size);
	sinfo->splash_mem_size = NULL;

	return -ENOMEM;
}

static bool _sde_splash_lk_check(struct sde_hw_intr *intr)
{
	return (SDE_LK_RUNNING_VALUE == SDE_REG_READ(&intr->hw,
			SCRATCH_REGISTER_1)) ? true : false;
}

/**
 * _sde_splash_notify_lk_to_exit.
 *
 * Function to monitor LK's status and tell it to exit.
 */
static void _sde_splash_notify_lk_exit(struct sde_hw_intr *intr)
{
	int i = 0;

	/* first is to write exit signal to scratch register*/
	SDE_REG_WRITE(&intr->hw, SCRATCH_REGISTER_1, SDE_LK_SHUT_DOWN_VALUE);

	while ((SDE_LK_EXIT_VALUE !=
		SDE_REG_READ(&intr->hw, SCRATCH_REGISTER_1)) &&
					(++i < SDE_LK_EXIT_MAX_LOOP)) {
		DRM_INFO("wait for LK's exit");
		msleep(20);
	}

	if (i == SDE_LK_EXIT_MAX_LOOP)
		SDE_ERROR("Loop LK's exit failed\n");
}

static int _sde_splash_gem_new(struct drm_device *dev,
				struct sde_splash_info *sinfo)
{
	int i, ret;

	for (i = 0; i < sinfo->splash_mem_num; i++) {
		sinfo->obj[i] = msm_gem_new(dev,
				sinfo->splash_mem_size[i], MSM_BO_UNCACHED);

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

static int _sde_splash_get_pages(struct drm_gem_object *obj, phys_addr_t phys)
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

static void _sde_splash_destroy_gem_object(struct msm_gem_object *msm_obj)
{
	if (msm_obj->pages) {
		sg_free_table(msm_obj->sgt);
		kfree(msm_obj->sgt);
		drm_free_large(msm_obj->pages);
		msm_obj->pages = NULL;
	}
}

static void _sde_splash_destroy_splash_node(struct sde_splash_info *sinfo)
{
	kfree(sinfo->splash_mem_paddr);
	sinfo->splash_mem_paddr = NULL;

	kfree(sinfo->splash_mem_size);
	sinfo->splash_mem_size = NULL;
}

static void _sde_splash_get_connector_ref_cnt(struct sde_splash_info *sinfo,
					u32 *hdmi_cnt, u32 *dsi_cnt)
{
	mutex_lock(&sde_splash_lock);
	*hdmi_cnt = sinfo->hdmi_connector_cnt;
	*dsi_cnt = sinfo->dsi_connector_cnt;
	mutex_unlock(&sde_splash_lock);
}

static int _sde_splash_free_resource(struct msm_mmu *mmu,
		struct sde_splash_info *sinfo, enum splash_connector_type conn)
{
	struct msm_gem_object *msm_obj = to_msm_bo(sinfo->obj[conn]);

	if (!msm_obj)
		return -EINVAL;

	if (mmu->funcs && mmu->funcs->unmap)
		mmu->funcs->unmap(mmu, sinfo->splash_mem_paddr[conn],
				msm_obj->sgt, NULL);

	_sde_splash_free_bootup_memory_to_system(sinfo->splash_mem_paddr[conn],
						sinfo->splash_mem_size[conn]);

	_sde_splash_destroy_gem_object(msm_obj);

	return 0;
}

__ref int sde_splash_init(struct sde_power_handle *phandle, struct msm_kms *kms)
{
	struct sde_kms *sde_kms;
	struct sde_splash_info *sinfo;
	int i = 0;

	if (!phandle || !kms) {
		SDE_ERROR("invalid phandle/kms\n");
		return -EINVAL;
	}

	sde_kms = to_sde_kms(kms);
	sinfo = &sde_kms->splash_info;

	sinfo->dsi_connector_cnt = 0;
	sinfo->hdmi_connector_cnt = 0;

	sde_power_data_bus_bandwidth_ctrl(phandle,
		sde_kms->core_client, true);

	for (i = 0; i < sinfo->splash_mem_num; i++) {
		if (!memblock_is_reserved(sinfo->splash_mem_paddr[i])) {
			SDE_ERROR("failed to reserve memory\n");

			/* withdraw the vote when failed. */
			sde_power_data_bus_bandwidth_ctrl(phandle,
					sde_kms->core_client, false);

			return -EINVAL;
		}
	}

	return 0;
}

void sde_splash_destroy(struct sde_splash_info *sinfo,
			struct sde_power_handle *phandle,
			struct sde_power_client *pclient)
{
	struct msm_gem_object *msm_obj;
	int i = 0;

	if (!sinfo || !phandle || !pclient) {
		SDE_ERROR("invalid sde_kms/phandle/pclient\n");
		return;
	}

	for (i = 0; i < sinfo->splash_mem_num; i++) {
		msm_obj = to_msm_bo(sinfo->obj[i]);

		if (msm_obj)
			_sde_splash_destroy_gem_object(msm_obj);
	}

	sde_power_data_bus_bandwidth_ctrl(phandle, pclient, false);

	_sde_splash_destroy_splash_node(sinfo);
}

/*
 * sde_splash_parse_dt.
 * In the function, it will parse and reserve two kinds of memory node.
 * First is to get the reserved memory for display buffers.
 * Second is to get the memory node LK's code stack is running on.
 */
int sde_splash_parse_dt(struct drm_device *dev)
{
	struct msm_drm_private *priv = dev->dev_private;
	struct sde_kms *sde_kms;
	struct sde_splash_info *sinfo;

	if (!priv || !priv->kms) {
		SDE_ERROR("Invalid kms\n");
		return -EINVAL;
	}

	sde_kms = to_sde_kms(priv->kms);
	sinfo = &sde_kms->splash_info;

	if (_sde_splash_parse_dt_get_display_node(dev, sinfo)) {
		SDE_ERROR("get display node failed\n");
		return -EINVAL;
	}

	if (_sde_splash_parse_dt_get_lk_pool_node(dev, sinfo)) {
		SDE_ERROR("get LK pool node failed\n");
		return -EINVAL;
	}

	return 0;
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
		sinfo->lk_is_exited = false;
	} else {
		sinfo->handoff = false;
		sinfo->program_scratch_regs = false;
		sinfo->lk_is_exited = true;
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
	if (_sde_splash_gem_new(dev, sinfo))
		return -ENOMEM;

	/* second is to contruct sgt table for calling smmu map */
	for (i = 0; i < sinfo->splash_mem_num; i++) {
		if (_sde_splash_get_pages(sinfo->obj[i],
				sinfo->splash_mem_paddr[i]))
			return -ENOMEM;
	}

	for (i = 0; i < sinfo->splash_mem_num; i++) {
		msm_obj = to_msm_bo(sinfo->obj[i]);

		if (mmu->funcs && mmu->funcs->map) {
			ret = mmu->funcs->map(mmu, sinfo->splash_mem_paddr[i],
				msm_obj->sgt, IOMMU_READ | IOMMU_NOEXEC, NULL);

			if (!ret) {
				SDE_ERROR("Map blk %d @%pK failed.\n",
					i, (void *)sinfo->splash_mem_paddr[i]);
				return ret;
			}
		}
	}

	return ret ? 0 : -ENOMEM;
}

void sde_splash_setup_connector_count(struct sde_splash_info *sinfo,
					int connector_type)
{
	switch (connector_type) {
	case DRM_MODE_CONNECTOR_HDMIA:
		sinfo->hdmi_connector_cnt++;
		break;
	case DRM_MODE_CONNECTOR_DSI:
		sinfo->dsi_connector_cnt++;
		break;
	default:
		SDE_ERROR("invalid connector_type %d\n", connector_type);
	}
}

bool sde_splash_get_lk_complete_status(struct sde_splash_info *sinfo)
{
	bool ret = 0;

	mutex_lock(&sde_splash_lock);
	ret = !sinfo->handoff && !sinfo->lk_is_exited;
	mutex_unlock(&sde_splash_lock);

	return ret;
}

int sde_splash_clean_up_free_resource(struct msm_kms *kms,
				struct sde_power_handle *phandle,
				int connector_type, void *display)
{
	struct sde_kms *sde_kms;
	struct sde_splash_info *sinfo;
	struct msm_mmu *mmu;
	struct dsi_display *dsi_display = display;
	int ret = 0;
	int hdmi_conn_count = 0;
	int dsi_conn_count = 0;
	static const char *last_commit_display_type = "unknown";

	if (!phandle || !kms) {
		SDE_ERROR("invalid phandle/kms.\n");
		return -EINVAL;
	}

	sde_kms = to_sde_kms(kms);
	sinfo = &sde_kms->splash_info;
	if (!sinfo) {
		SDE_ERROR("%s(%d): invalid splash info\n", __func__, __LINE__);
		return -EINVAL;
	}

	_sde_splash_get_connector_ref_cnt(sinfo, &hdmi_conn_count,
						&dsi_conn_count);

	mutex_lock(&sde_splash_lock);
	if (hdmi_conn_count == 0 && dsi_conn_count == 0 &&
					!sinfo->lk_is_exited) {
		/* When both hdmi's and dsi's handoff are finished,
		 * 1. Destroy splash node objects.
		 * 2. Release the memory which LK's stack is running on.
		 * 3. Withdraw AHB data bus bandwidth voting.
		 */
		DRM_INFO("HDMI and DSI resource handoff is completed\n");

		sinfo->lk_is_exited = true;

		_sde_splash_destroy_splash_node(sinfo);

		_sde_splash_free_bootup_memory_to_system(sinfo->lk_pool_paddr,
							sinfo->lk_pool_size);

		sde_power_data_bus_bandwidth_ctrl(phandle,
				sde_kms->core_client, false);

		mutex_unlock(&sde_splash_lock);
		return 0;
	}

	mmu = sde_kms->aspace[0]->mmu;

	switch (connector_type) {
	case DRM_MODE_CONNECTOR_HDMIA:
		if (sinfo->hdmi_connector_cnt == 1) {
			sinfo->hdmi_connector_cnt--;

			ret = _sde_splash_free_resource(mmu,
					sinfo, SPLASH_HDMI);
		}
		break;
	case DRM_MODE_CONNECTOR_DSI:
		/*
		 * Basically, we have commits coming on two DSI connectors.
		 * So when releasing DSI resource, it's ensured that the
		 * coming commits should happen on different DSIs, to promise
		 * the handoff has finished on the two DSIs, then it's safe
		 * to release DSI resource, otherwise, problem happens when
		 * freeing memory, while DSI0 or DSI1 is still visiting
		 * the memory.
		 */
		if (strcmp(dsi_display->display_type, "unknown") &&
			strcmp(last_commit_display_type,
					dsi_display->display_type)) {
			if (sinfo->dsi_connector_cnt > 1)
				sinfo->dsi_connector_cnt--;
			else if (sinfo->dsi_connector_cnt == 1) {
				ret = _sde_splash_free_resource(mmu,
					sinfo, SPLASH_DSI);

				sinfo->dsi_connector_cnt--;
			}

			last_commit_display_type = dsi_display->display_type;
		}
		break;
	default:
		ret = -EINVAL;
		SDE_ERROR("%s: invalid connector_type %d\n",
				__func__, connector_type);
	}

	mutex_unlock(&sde_splash_lock);

	return ret;
}

/*
 * In below function, it will
 * 1. Notify LK to exit and wait for exiting is done.
 * 2. Set DOMAIN_ATTR_EARLY_MAP to 1 to enable stage 1 translation in iommu.
 */
int sde_splash_clean_up_exit_lk(struct msm_kms *kms)
{
	struct sde_splash_info *sinfo;
	struct msm_mmu *mmu;
	struct sde_kms *sde_kms = to_sde_kms(kms);
	int ret;

	sinfo = &sde_kms->splash_info;

	if (!sinfo) {
		SDE_ERROR("%s(%d): invalid splash info\n", __func__, __LINE__);
		return -EINVAL;
	}

	/* Monitor LK's status and tell it to exit. */
	mutex_lock(&sde_splash_lock);
	if (sinfo->program_scratch_regs) {
		if (_sde_splash_lk_check(sde_kms->hw_intr))
			_sde_splash_notify_lk_exit(sde_kms->hw_intr);

		sinfo->handoff = false;
		sinfo->program_scratch_regs = false;
	}
	mutex_unlock(&sde_splash_lock);

	if (!sde_kms->aspace[0] || !sde_kms->aspace[0]->mmu) {
		/* We do not return fault value here, to ensure
		 * flag "lk_is_exited" is set.
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

	return 0;
}
