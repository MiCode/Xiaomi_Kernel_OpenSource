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
 */

#include <linux/io.h>
#include <linux/types.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/memory_alloc.h>
#include <linux/module.h>
#include <mach/iommu.h>
#include <mach/iommu_domains.h>
#include <mach/msm_subsystem_map.h>

struct msm_buffer_node {
	struct rb_node rb_node_all_buffer;
	struct rb_node rb_node_paddr;
	struct msm_mapped_buffer *buf;
	unsigned long length;
	unsigned int *subsystems;
	unsigned int nsubsys;
	unsigned int phys;
};

static struct rb_root buffer_root;
static struct rb_root phys_root;
DEFINE_MUTEX(msm_buffer_mutex);

static unsigned long subsystem_to_domain_tbl[] = {
	VIDEO_DOMAIN,
	VIDEO_DOMAIN,
	CAMERA_DOMAIN,
	DISPLAY_READ_DOMAIN,
	DISPLAY_WRITE_DOMAIN,
	ROTATOR_SRC_DOMAIN,
	ROTATOR_DST_DOMAIN,
	0xFFFFFFFF
};

static struct msm_buffer_node *find_buffer(void *key)
{
	struct rb_root *root = &buffer_root;
	struct rb_node *p = root->rb_node;

	mutex_lock(&msm_buffer_mutex);

	while (p) {
		struct msm_buffer_node *node;

		node = rb_entry(p, struct msm_buffer_node, rb_node_all_buffer);
		if (node->buf->vaddr) {
			if (key < node->buf->vaddr)
				p = p->rb_left;
			else if (key > node->buf->vaddr)
				p = p->rb_right;
			else {
				mutex_unlock(&msm_buffer_mutex);
				return node;
			}
		} else {
			if (key < (void *)node->buf)
				p = p->rb_left;
			else if (key > (void *)node->buf)
				p = p->rb_right;
			else {
				mutex_unlock(&msm_buffer_mutex);
				return node;
			}
		}
	}
	mutex_unlock(&msm_buffer_mutex);
	return NULL;
}

static struct msm_buffer_node *find_buffer_phys(unsigned int phys)
{
	struct rb_root *root = &phys_root;
	struct rb_node *p = root->rb_node;

	mutex_lock(&msm_buffer_mutex);

	while (p) {
		struct msm_buffer_node *node;

		node = rb_entry(p, struct msm_buffer_node, rb_node_paddr);
		if (phys < node->phys)
			p = p->rb_left;
		else if (phys > node->phys)
			p = p->rb_right;
		else {
			mutex_unlock(&msm_buffer_mutex);
			return node;
		}
	}
	mutex_unlock(&msm_buffer_mutex);
	return NULL;

}

static int add_buffer(struct msm_buffer_node *node)
{
	struct rb_root *root = &buffer_root;
	struct rb_node **p = &root->rb_node;
	struct rb_node *parent = NULL;
	void *key;

	if (node->buf->vaddr)
		key = node->buf->vaddr;
	else
		key = node->buf;

	mutex_lock(&msm_buffer_mutex);
	while (*p) {
		struct msm_buffer_node *tmp;
		parent = *p;

		tmp = rb_entry(parent, struct msm_buffer_node,
						rb_node_all_buffer);

		if (tmp->buf->vaddr) {
			if (key < tmp->buf->vaddr)
				p = &(*p)->rb_left;
			else if (key > tmp->buf->vaddr)
				p = &(*p)->rb_right;
			else {
				WARN(1, "tried to add buffer twice! buf = %p"
					" vaddr = %p iova = %p", tmp->buf,
					tmp->buf->vaddr,
					tmp->buf->iova);
				mutex_unlock(&msm_buffer_mutex);
				return -EINVAL;

			}
		} else {
			if (key < (void *)tmp->buf)
				p = &(*p)->rb_left;
			else if (key > (void *)tmp->buf)
				p = &(*p)->rb_right;
			else {
				WARN(1, "tried to add buffer twice! buf = %p"
					" vaddr = %p iova = %p", tmp->buf,
					tmp->buf->vaddr,
					tmp->buf->iova);
				mutex_unlock(&msm_buffer_mutex);
				return -EINVAL;
			}
		}
	}
	rb_link_node(&node->rb_node_all_buffer, parent, p);
	rb_insert_color(&node->rb_node_all_buffer, root);
	mutex_unlock(&msm_buffer_mutex);
	return 0;
}

static int add_buffer_phys(struct msm_buffer_node *node)
{
	struct rb_root *root = &phys_root;
	struct rb_node **p = &root->rb_node;
	struct rb_node *parent = NULL;

	mutex_lock(&msm_buffer_mutex);
	while (*p) {
		struct msm_buffer_node *tmp;
		parent = *p;

		tmp = rb_entry(parent, struct msm_buffer_node, rb_node_paddr);

			if (node->phys < tmp->phys)
				p = &(*p)->rb_left;
			else if (node->phys > tmp->phys)
				p = &(*p)->rb_right;
			else {
				WARN(1, "tried to add buffer twice! buf = %p"
					" vaddr = %p iova = %p", tmp->buf,
					tmp->buf->vaddr,
					tmp->buf->iova);
				mutex_unlock(&msm_buffer_mutex);
				return -EINVAL;

			}
	}
	rb_link_node(&node->rb_node_paddr, parent, p);
	rb_insert_color(&node->rb_node_paddr, root);
	mutex_unlock(&msm_buffer_mutex);
	return 0;
}

static int remove_buffer(struct msm_buffer_node *victim_node)
{
	struct rb_root *root = &buffer_root;

	if (!victim_node)
		return -EINVAL;

	mutex_lock(&msm_buffer_mutex);
	rb_erase(&victim_node->rb_node_all_buffer, root);
	mutex_unlock(&msm_buffer_mutex);
	return 0;
}

static int remove_buffer_phys(struct msm_buffer_node *victim_node)
{
	struct rb_root *root = &phys_root;

	if (!victim_node)
		return -EINVAL;

	mutex_lock(&msm_buffer_mutex);
	rb_erase(&victim_node->rb_node_paddr, root);
	mutex_unlock(&msm_buffer_mutex);
	return 0;
}

static unsigned long msm_subsystem_get_domain_no(int subsys_id)
{
	if (subsys_id > INVALID_SUBSYS_ID && subsys_id <= MAX_SUBSYSTEM_ID &&
	    subsys_id < ARRAY_SIZE(subsystem_to_domain_tbl))
		return subsystem_to_domain_tbl[subsys_id];
	else
		return subsystem_to_domain_tbl[MAX_SUBSYSTEM_ID];
}

static unsigned long msm_subsystem_get_partition_no(int subsys_id)
{
	switch (subsys_id) {
	case MSM_SUBSYSTEM_VIDEO_FWARE:
		return VIDEO_FIRMWARE_POOL;
	case MSM_SUBSYSTEM_VIDEO:
		return VIDEO_MAIN_POOL;
	case MSM_SUBSYSTEM_CAMERA:
	case MSM_SUBSYSTEM_DISPLAY:
	case MSM_SUBSYSTEM_ROTATOR:
		return GEN_POOL;
	default:
		return 0xFFFFFFFF;
	}
}

phys_addr_t msm_subsystem_check_iova_mapping(int subsys_id, unsigned long iova)
{
	struct iommu_domain *subsys_domain;

	if (!msm_use_iommu())
		/*
		 * If there is no iommu, Just return the iova in this case.
		 */
		return iova;

	subsys_domain = msm_get_iommu_domain(msm_subsystem_get_domain_no
								(subsys_id));

	return iommu_iova_to_phys(subsys_domain, iova);
}
EXPORT_SYMBOL(msm_subsystem_check_iova_mapping);

struct msm_mapped_buffer *msm_subsystem_map_buffer(unsigned long phys,
						unsigned int length,
						unsigned int flags,
						int *subsys_ids,
						unsigned int nsubsys)
{
	struct msm_mapped_buffer *buf, *err;
	struct msm_buffer_node *node;
	int i = 0, j = 0, ret;
	unsigned long iova_start = 0, temp_phys, temp_va = 0;
	struct iommu_domain *d = NULL;
	int map_size = length;

	if (!((flags & MSM_SUBSYSTEM_MAP_KADDR) ||
		(flags & MSM_SUBSYSTEM_MAP_IOVA))) {
		pr_warn("%s: no mapping flag was specified. The caller"
			" should explicitly specify what to map in the"
			" flags.\n", __func__);
		err = ERR_PTR(-EINVAL);
		goto outret;
	}

	buf = kzalloc(sizeof(*buf), GFP_ATOMIC);
	if (!buf) {
		err = ERR_PTR(-ENOMEM);
		goto outret;
	}

	node = kzalloc(sizeof(*node), GFP_ATOMIC);
	if (!node) {
		err = ERR_PTR(-ENOMEM);
		goto outkfreebuf;
	}

	node->phys = phys;

	if (flags & MSM_SUBSYSTEM_MAP_KADDR) {
		struct msm_buffer_node *old_buffer;

		old_buffer = find_buffer_phys(phys);

		if (old_buffer) {
			WARN(1, "%s: Attempting to map %lx twice in the kernel"
				" virtual space. Don't do that!\n", __func__,
				phys);
			err = ERR_PTR(-EINVAL);
			goto outkfreenode;
		}

		if (flags & MSM_SUBSYSTEM_MAP_CACHED)
			buf->vaddr = ioremap(phys, length);
		else if (flags & MSM_SUBSYSTEM_MAP_KADDR)
			buf->vaddr = ioremap_nocache(phys, length);
		else {
			pr_warn("%s: no cachability flag was indicated. Caller"
				" must specify a cachability flag.\n",
				__func__);
			err = ERR_PTR(-EINVAL);
			goto outkfreenode;
		}

		if (!buf->vaddr) {
			pr_err("%s: could not ioremap\n", __func__);
			err = ERR_PTR(-EINVAL);
			goto outkfreenode;
		}

		if (add_buffer_phys(node)) {
			err = ERR_PTR(-EINVAL);
			goto outiounmap;
		}
	}

	if ((flags & MSM_SUBSYSTEM_MAP_IOVA) && subsys_ids) {
		int min_align;

		length = round_up(length, SZ_4K);

		if (flags & MSM_SUBSYSTEM_MAP_IOMMU_2X)
			map_size = 2 * length;
		else
			map_size = length;

		buf->iova = kzalloc(sizeof(unsigned long)*nsubsys, GFP_ATOMIC);
		if (!buf->iova) {
			err = ERR_PTR(-ENOMEM);
			goto outremovephys;
		}

		/*
		 * The alignment must be specified as the exact value wanted
		 * e.g. 8k alignment must pass (0x2000 | other flags)
		 */
		min_align = flags & ~(SZ_4K - 1);

		for (i = 0; i < nsubsys; i++) {
			unsigned int domain_no, partition_no;

			if (!msm_use_iommu()) {
				buf->iova[i] = phys;
				continue;
			}

			d = msm_get_iommu_domain(
				msm_subsystem_get_domain_no(subsys_ids[i]));

			if (!d) {
				pr_err("%s: could not get domain for subsystem"
					" %d\n", __func__, subsys_ids[i]);
				continue;
			}

			domain_no = msm_subsystem_get_domain_no(subsys_ids[i]);
			partition_no = msm_subsystem_get_partition_no(
								subsys_ids[i]);

			ret = msm_allocate_iova_address(domain_no,
						partition_no,
						map_size,
						max(min_align, SZ_4K),
						&iova_start);

			if (ret) {
				pr_err("%s: could not allocate iova address\n",
					__func__);
				continue;
			}

			temp_phys = phys;
			temp_va = iova_start;
			for (j = length; j > 0; j -= SZ_4K,
					temp_phys += SZ_4K,
					temp_va += SZ_4K) {
				ret = iommu_map(d, temp_va, temp_phys,
						SZ_4K,
						(IOMMU_READ | IOMMU_WRITE));
				if (ret) {
					pr_err("%s: could not map iommu for"
						" domain %p, iova %lx,"
						" phys %lx\n", __func__, d,
						temp_va, temp_phys);
					err = ERR_PTR(-EINVAL);
					goto outdomain;
				}
			}
			buf->iova[i] = iova_start;

			if (flags & MSM_SUBSYSTEM_MAP_IOMMU_2X)
				msm_iommu_map_extra
					(d, temp_va, length, SZ_4K,
					(IOMMU_READ | IOMMU_WRITE));
		}

	}

	node->buf = buf;
	node->subsystems = subsys_ids;
	node->length = map_size;
	node->nsubsys = nsubsys;

	if (add_buffer(node)) {
		err = ERR_PTR(-EINVAL);
		goto outiova;
	}

	return buf;

outiova:
	if (flags & MSM_SUBSYSTEM_MAP_IOVA)
		iommu_unmap(d, temp_va, SZ_4K);
outdomain:
	if (flags & MSM_SUBSYSTEM_MAP_IOVA) {
		/* Unmap the rest of the current domain, i */
		for (j -= SZ_4K, temp_va -= SZ_4K;
			j > 0; temp_va -= SZ_4K, j -= SZ_4K)
			iommu_unmap(d, temp_va, SZ_4K);

		/* Unmap all the other domains */
		for (i--; i >= 0; i--) {
			unsigned int domain_no, partition_no;
			if (!msm_use_iommu())
				continue;
			domain_no = msm_subsystem_get_domain_no(subsys_ids[i]);
			partition_no = msm_subsystem_get_partition_no(
								subsys_ids[i]);

			temp_va = buf->iova[i];
			for (j = length; j > 0; j -= SZ_4K,
						temp_va += SZ_4K)
				iommu_unmap(d, temp_va, SZ_4K);
			msm_free_iova_address(buf->iova[i], domain_no,
					partition_no, length);
		}

		kfree(buf->iova);
	}

outremovephys:
	if (flags & MSM_SUBSYSTEM_MAP_KADDR)
		remove_buffer_phys(node);
outiounmap:
	if (flags & MSM_SUBSYSTEM_MAP_KADDR)
		iounmap(buf->vaddr);
outkfreenode:
	kfree(node);
outkfreebuf:
	kfree(buf);
outret:
	return err;
}
EXPORT_SYMBOL(msm_subsystem_map_buffer);

int msm_subsystem_unmap_buffer(struct msm_mapped_buffer *buf)
{
	struct msm_buffer_node *node;
	int i, j, ret;
	unsigned long temp_va;

	if (IS_ERR_OR_NULL(buf))
		goto out;

	if (buf->vaddr)
		node = find_buffer(buf->vaddr);
	else
		node = find_buffer(buf);

	if (!node)
		goto out;

	if (node->buf != buf) {
		pr_err("%s: caller must pass in the same buffer structure"
			" returned from map_buffer when freeding\n", __func__);
		goto out;
	}

	if (buf->iova) {
		if (msm_use_iommu())
			for (i = 0; i < node->nsubsys; i++) {
				struct iommu_domain *subsys_domain;
				unsigned int domain_no, partition_no;

				subsys_domain = msm_get_iommu_domain(
						msm_subsystem_get_domain_no(
						node->subsystems[i]));

				domain_no = msm_subsystem_get_domain_no(
							node->subsystems[i]);
				partition_no = msm_subsystem_get_partition_no(
							node->subsystems[i]);

				temp_va = buf->iova[i];
				for (j = node->length; j > 0; j -= SZ_4K,
					temp_va += SZ_4K) {
					ret = iommu_unmap(subsys_domain,
							temp_va,
							SZ_4K);
					WARN(ret, "iommu_unmap returned a "
						" non-zero value.\n");
				}
				msm_free_iova_address(buf->iova[i], domain_no,
						partition_no, node->length);
			}
		kfree(buf->iova);

	}

	if (buf->vaddr) {
		remove_buffer_phys(node);
		iounmap(buf->vaddr);
	}

	remove_buffer(node);
	kfree(node);
	kfree(buf);

	return 0;
out:
	return -EINVAL;
}
EXPORT_SYMBOL(msm_subsystem_unmap_buffer);
