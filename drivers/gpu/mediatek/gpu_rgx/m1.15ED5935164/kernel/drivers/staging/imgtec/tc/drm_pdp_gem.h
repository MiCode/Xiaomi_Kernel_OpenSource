/*
 * @File
 * @Codingstyle LinuxKernel
 * @Copyright   Copyright (c) Imagination Technologies Ltd. All Rights Reserved
 * @License     Dual MIT/GPLv2
 *
 * The contents of this file are subject to the MIT license as set out below.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * the GNU General Public License Version 2 ("GPL") in which case the provisions
 * of GPL are applicable instead of those above.
 *
 * If you wish to allow use of your version of this file only under the terms of
 * GPL, and not to allow others to use your version of this file under the terms
 * of the MIT license, indicate your decision by deleting the provisions above
 * and replace them with the notice and other provisions required by GPL as set
 * out in the file called "GPL-COPYING" included in this distribution. If you do
 * not delete the provisions above, a recipient may use your version of this file
 * under the terms of either the MIT license or GPL.
 *
 * This License is also included in this distribution in the file called
 * "MIT-COPYING".
 *
 * EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
 * PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 * BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#if !defined(__DRM_PDP_GEM_H__)
#define __DRM_PDP_GEM_H__

#include <linux/dma-buf.h>
#include <linux/version.h>

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 5, 0))
#include <drm/drm_device.h>
#include <drm/drm_file.h>
#else
#include <drm/drmP.h>
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0))
#include <drm/drm_gem.h>
#endif

#include "drm_pdp_drv.h"
#include "pvr_dma_resv.h"

struct pdp_gem_private;

struct pdp_gem_object {
	struct drm_gem_object base;

	/* Non-null if backing originated from this driver */
	struct drm_mm_node *vram;

	/* Non-null if backing was imported */
	struct sg_table *sgt;

	bool dma_map_export_host_addr;
	phys_addr_t cpu_addr;
	dma_addr_t dev_addr;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 2, 0))
	struct dma_resv _resv;
#endif
	struct dma_resv *resv;

	bool cpu_prep;
};

#define to_pdp_obj(obj) container_of(obj, struct pdp_gem_object, base)

struct pdp_gem_private *pdp_gem_init(struct drm_device *dev);

void pdp_gem_cleanup(struct pdp_gem_private *dev_priv);

/* ioctl functions */
int pdp_gem_object_create_ioctl_priv(struct drm_device *dev,
				     struct pdp_gem_private *gem_priv,
				     void *data,
				     struct drm_file *file);
int pdp_gem_object_mmap_ioctl(struct drm_device *dev, void *data,
			      struct drm_file *file);
int pdp_gem_object_cpu_prep_ioctl(struct drm_device *dev, void *data,
				  struct drm_file *file);
int pdp_gem_object_cpu_fini_ioctl(struct drm_device *dev, void *data,
				  struct drm_file *file);

/* drm driver functions */
struct drm_gem_object *pdp_gem_object_create(struct drm_device *dev,
					     struct pdp_gem_private *gem_priv,
					     size_t size,
					     u32 flags);

void pdp_gem_object_free_priv(struct pdp_gem_private *gem_priv,
			      struct drm_gem_object *obj);

struct dma_buf *pdp_gem_prime_export(
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
				     struct drm_device *dev,
#endif
				     struct drm_gem_object *obj,
				     int flags);

struct drm_gem_object *pdp_gem_prime_import(struct drm_device *dev,
					    struct dma_buf *dma_buf);

struct drm_gem_object *
pdp_gem_prime_import_sg_table(struct drm_device *dev,
			      struct dma_buf_attachment *attach,
			      struct sg_table *sgt);

int pdp_gem_dumb_create_priv(struct drm_file *file,
			     struct drm_device *dev,
			     struct pdp_gem_private *gem_priv,
			     struct drm_mode_create_dumb *args);

int pdp_gem_dumb_map_offset(struct drm_file *file, struct drm_device *dev,
			    uint32_t handle, uint64_t *offset);

/* vm operation functions */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0))
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 17, 0))
typedef int vm_fault_t;
#endif
vm_fault_t pdp_gem_object_vm_fault(struct vm_fault *vmf);
#else
int pdp_gem_object_vm_fault(struct vm_area_struct *vma, struct vm_fault *vmf);
#endif

/* internal interfaces */
struct dma_resv *pdp_gem_get_resv(struct drm_gem_object *obj);
u64 pdp_gem_get_dev_addr(struct drm_gem_object *obj);

#endif /* !defined(__DRM_PDP_GEM_H__) */
