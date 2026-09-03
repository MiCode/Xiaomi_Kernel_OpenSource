/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef _SPRD_GEM_H_
#define _SPRD_GEM_H_

#include <drm/drm_gem.h>

struct sprd_gem_obj {
	struct drm_gem_object	base;
	dma_addr_t		dma_addr;
	struct sg_table		*sgtb;
	void			*vaddr;
	bool			need_iommu;
};

#define to_sprd_gem_obj(x)	container_of(x, struct sprd_gem_obj, base)

void sprd_gem_free_object(struct drm_gem_object *gem);
int sprd_gem_dumb_create(struct drm_file *file_priv, struct drm_device *dev,
			    struct drm_mode_create_dumb *args);
int sprd_gem_object_mmap(struct drm_gem_object *obj,
				   struct vm_area_struct *vma);
int sprd_gem_mmap(struct file *filp, struct vm_area_struct *vma);
int sprd_gem_prime_mmap(struct drm_gem_object *obj,
			 struct vm_area_struct *vma);
struct sg_table *sprd_gem_prime_get_sg_table(struct drm_gem_object *obj);
struct drm_gem_object *sprd_gem_prime_import_sg_table(struct drm_device *dev,
		struct dma_buf_attachment *attach, struct sg_table *sgtb);
int sprd_gem_prime_vmap(struct drm_gem_object *obj, struct dma_buf_map *map);
void sprd_gem_prime_vunmap(struct drm_gem_object *obj, struct dma_buf_map *map);

#endif /* _SPRD_GEM_H_ */
