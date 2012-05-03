/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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

#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/ioctl.h>
#include <linux/spinlock.h>
#include <linux/videodev2.h>
#include <linux/proc_fs.h>
#include <linux/vmalloc.h>

#include <media/v4l2-dev.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-device.h>

#include <linux/android_pmem.h>

#include "msm.h"

#ifdef CONFIG_MSM_CAMERA_DEBUG
#define D(fmt, args...) pr_debug("msm_isp: " fmt, ##args)
#else
#define D(fmt, args...) do {} while (0)
#endif

#define PAD_TO_WORD(a)	  (((a) + 3) & ~3)

#define __CONTAINS(r, v, l, field) ({			   \
	typeof(r) __r = r;				  \
	typeof(v) __v = v;				  \
	typeof(v) __e = __v + l;				\
	int res = __v >= __r->field &&			  \
		__e <= __r->field + __r->len;		   \
	res;							\
})

#define CONTAINS(r1, r2, field) ({			  \
	typeof(r2) __r2 = r2;				   \
	__CONTAINS(r1, __r2->field, __r2->len, field);	  \
})

#define IN_RANGE(r, v, field) ({				\
	typeof(r) __r = r;				  \
	typeof(v) __vv = v;				 \
	int res = ((__vv >= __r->field) &&		  \
		(__vv < (__r->field + __r->len)));	  \
	res;							\
})

#define OVERLAPS(r1, r2, field) ({			  \
	typeof(r1) __r1 = r1;				   \
	typeof(r2) __r2 = r2;				   \
	typeof(__r2->field) __v = __r2->field;		  \
	typeof(__v) __e = __v + __r2->len - 1;		  \
	int res = (IN_RANGE(__r1, __v, field) ||		\
		IN_RANGE(__r1, __e, field));				 \
	res;							\
})

static DEFINE_MUTEX(hlist_mut);

#ifdef CONFIG_ANDROID_PMEM
static int check_pmem_info(struct msm_pmem_info *info, int len)
{
	if (info->offset < len &&
		info->offset + info->len <= len &&
		info->planar0_off < len &&
		info->planar1_off < len)
		return 0;

	pr_err("%s: check failed: off %d len %d y %d cbcr %d (total len %d)\n",
						__func__,
						info->offset,
						info->len,
						info->planar0_off,
						info->planar1_off,
						len);
	return -EINVAL;
}
#endif

static int check_overlap(struct hlist_head *ptype,
				unsigned long paddr,
				unsigned long len)
{
	struct msm_pmem_region *region;
	struct msm_pmem_region t = { .paddr = paddr, .len = len };
	struct hlist_node *node;

	hlist_for_each_entry(region, node, ptype, list) {
		if (CONTAINS(region, &t, paddr) ||
			CONTAINS(&t, region, paddr) ||
			OVERLAPS(region, &t, paddr)) {
			CDBG(" region (PHYS %p len %ld)"
				" clashes with registered region"
				" (paddr %p len %ld)\n",
				(void *)t.paddr, t.len,
				(void *)region->paddr, region->len);
			return -EINVAL;
		}
	}

	return 0;
}

static int msm_pmem_table_add(struct hlist_head *ptype,
	struct msm_pmem_info *info, struct ion_client *client)
{
	unsigned long paddr;
#ifndef CONFIG_MSM_MULTIMEDIA_USE_ION
	unsigned long kvstart;
	struct file *file;
#endif
	int rc = -ENOMEM;

	unsigned long len;
	struct msm_pmem_region *region;

	region = kmalloc(sizeof(struct msm_pmem_region), GFP_KERNEL);
	if (!region)
		goto out;
#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
	region->handle = ion_import_fd(client, info->fd);
	if (IS_ERR_OR_NULL(region->handle))
		goto out1;
	if (ion_map_iommu(client, region->handle, CAMERA_DOMAIN, GEN_POOL,
				  SZ_4K, 0, &paddr, &len, UNCACHED, 0) < 0)
		goto out2;
#elif CONFIG_ANDROID_PMEM
	rc = get_pmem_file(info->fd, &paddr, &kvstart, &len, &file);
	if (rc < 0) {
		pr_err("%s: get_pmem_file fd %d error %d\n",
				__func__, info->fd, rc);
		goto out1;
	}
	region->file = file;
#else
	paddr = 0;
	file = NULL;
	kvstart = 0;
#endif
	if (!info->len)
		info->len = len;
	rc = check_pmem_info(info, len);
	if (rc < 0)
		goto out3;
	paddr += info->offset;
	len = info->len;

	if (check_overlap(ptype, paddr, len) < 0) {
		rc = -EINVAL;
		goto out3;
	}

	CDBG("%s: type %d, active flag %d, paddr 0x%lx, vaddr 0x%lx\n",
		__func__, info->type, info->active, paddr,
		(unsigned long)info->vaddr);

	INIT_HLIST_NODE(&region->list);
	region->paddr = paddr;
	region->len = len;
	memcpy(&region->info, info, sizeof(region->info));
	D("%s Adding region to list with type %d\n", __func__,
						region->info.type);
	D("%s pmem_stats address is 0x%p\n", __func__, ptype);
	hlist_add_head(&(region->list), ptype);

	return 0;
out3:
#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
	ion_unmap_iommu(client, region->handle, CAMERA_DOMAIN, GEN_POOL);
#endif
#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
out2:
	ion_free(client, region->handle);
#elif CONFIG_ANDROID_PMEM
	put_pmem_file(region->file);
#endif
out1:
	kfree(region);
out:
	return rc;
}

static int __msm_register_pmem(struct hlist_head *ptype,
			struct msm_pmem_info *pinfo, struct ion_client *client)
{
	int rc = 0;

	switch (pinfo->type) {
	case MSM_PMEM_AF:
	case MSM_PMEM_AEC:
	case MSM_PMEM_AWB:
	case MSM_PMEM_RS:
	case MSM_PMEM_CS:
	case MSM_PMEM_IHIST:
	case MSM_PMEM_SKIN:
	case MSM_PMEM_AEC_AWB:
		rc = msm_pmem_table_add(ptype, pinfo, client);
		break;

	default:
		rc = -EINVAL;
		break;
	}

	return rc;
}

static int __msm_pmem_table_del(struct hlist_head *ptype,
			struct msm_pmem_info *pinfo, struct ion_client *client)
{
	int rc = 0;
	struct msm_pmem_region *region;
	struct hlist_node *node, *n;

	switch (pinfo->type) {
	case MSM_PMEM_AF:
	case MSM_PMEM_AEC:
	case MSM_PMEM_AWB:
	case MSM_PMEM_RS:
	case MSM_PMEM_CS:
	case MSM_PMEM_IHIST:
	case MSM_PMEM_SKIN:
	case MSM_PMEM_AEC_AWB:
		hlist_for_each_entry_safe(region, node, n,
				ptype, list) {

			if (pinfo->type == region->info.type &&
				pinfo->vaddr == region->info.vaddr &&
				pinfo->fd == region->info.fd) {
				hlist_del(node);
#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
				ion_unmap_iommu(client, region->handle,
					CAMERA_DOMAIN, GEN_POOL);
				ion_free(client, region->handle);
#else
				put_pmem_file(region->file);
#endif
				kfree(region);
			}
		}
		break;

	default:
		rc = -EINVAL;
		break;
	}

	return rc;
}

/* return of 0 means failure */
uint8_t msm_pmem_region_lookup(struct hlist_head *ptype,
	int pmem_type, struct msm_pmem_region *reg, uint8_t maxcount)
{
	struct msm_pmem_region *region;
	struct msm_pmem_region *regptr;
	struct hlist_node *node, *n;

	uint8_t rc = 0;
	D("%s\n", __func__);
	regptr = reg;
	mutex_lock(&hlist_mut);
	hlist_for_each_entry_safe(region, node, n, ptype, list) {
		if (region->info.type == pmem_type && region->info.active) {
			*regptr = *region;
			rc += 1;
			if (rc >= maxcount)
				break;
			regptr++;
		}
	}
	D("%s finished, rc=%d\n", __func__, rc);
	mutex_unlock(&hlist_mut);
	return rc;
}

int msm_pmem_region_get_phy_addr(struct hlist_head *ptype,
	struct msm_mem_map_info *mem_map, int32_t *phyaddr)
{
	struct msm_pmem_region *region;
	struct hlist_node *node, *n;
	int pmem_type = mem_map->mem_type;
	int rc = -EFAULT;

	D("%s\n", __func__);
	*phyaddr = 0;
	mutex_lock(&hlist_mut);
	hlist_for_each_entry_safe(region, node, n, ptype, list) {
		if (region->info.type == pmem_type &&
			(uint32_t)region->info.vaddr == mem_map->cookie) {
			*phyaddr = (int32_t)region->paddr;
			rc = 0;
			break;
		}
	}
	D("%s finished, phy_addr = 0x%x, rc=%d\n", __func__, *phyaddr, rc);
	mutex_unlock(&hlist_mut);
	return rc;
}

uint8_t msm_pmem_region_lookup_2(struct hlist_head *ptype,
					int pmem_type,
					struct msm_pmem_region *reg,
					uint8_t maxcount)
{
	struct msm_pmem_region *region;
	struct msm_pmem_region *regptr;
	struct hlist_node *node, *n;
	uint8_t rc = 0;
	regptr = reg;
	mutex_lock(&hlist_mut);
	hlist_for_each_entry_safe(region, node, n, ptype, list) {
		D("Mio: info.type=%d, pmem_type = %d,"
						"info.active = %d\n",
		region->info.type, pmem_type, region->info.active);

		if (region->info.type == pmem_type && region->info.active) {
			D("info.type=%d, pmem_type = %d,"
							"info.active = %d,\n",
				region->info.type, pmem_type,
				region->info.active);
			*regptr = *region;
			region->info.type = MSM_PMEM_VIDEO;
			rc += 1;
			if (rc >= maxcount)
				break;
			regptr++;
		}
	}
	mutex_unlock(&hlist_mut);
	return rc;
}

unsigned long msm_pmem_stats_vtop_lookup(
				struct msm_cam_media_controller *mctl,
				unsigned long buffer,
				int fd)
{
	struct msm_pmem_region *region;
	struct hlist_node *node, *n;

	hlist_for_each_entry_safe(region, node, n,
	&mctl->stats_info.pmem_stats_list, list) {
		if (((unsigned long)(region->info.vaddr) == buffer) &&
						(region->info.fd == fd) &&
						region->info.active == 0) {
			region->info.active = 1;
			return region->paddr;
		}
	}

	return 0;
}

unsigned long msm_pmem_stats_ptov_lookup(
		struct msm_cam_media_controller *mctl,
		unsigned long addr, int *fd)
{
	struct msm_pmem_region *region;
	struct hlist_node *node, *n;

	hlist_for_each_entry_safe(region, node, n,
	&mctl->stats_info.pmem_stats_list, list) {
		if (addr == region->paddr && region->info.active) {
			/* offset since we could pass vaddr inside a
			 * registered pmem buffer */
			*fd = region->info.fd;
			region->info.active = 0;
			return (unsigned long)(region->info.vaddr);
		}
	}

	return 0;
}

int msm_register_pmem(struct hlist_head *ptype, void __user *arg,
					  struct ion_client *client)
{
	struct msm_pmem_info info;

	if (copy_from_user(&info, arg, sizeof(info))) {
		ERR_COPY_FROM_USER();
			return -EFAULT;
	}

	return __msm_register_pmem(ptype, &info, client);
}
EXPORT_SYMBOL(msm_register_pmem);

int msm_pmem_table_del(struct hlist_head *ptype, void __user *arg,
					   struct ion_client *client)
{
	struct msm_pmem_info info;

	if (copy_from_user(&info, arg, sizeof(info))) {
		ERR_COPY_FROM_USER();
		return -EFAULT;
	}

	return __msm_pmem_table_del(ptype, &info, client);
}
EXPORT_SYMBOL(msm_pmem_table_del);
