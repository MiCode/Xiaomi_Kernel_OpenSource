/* -*- mode: c; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* vi: set ts=8 sw=8 sts=8: */
/*************************************************************************/ /*!
@File
@Codingstyle    LinuxKernel
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/

#if !defined(__DRM_PDP_GEM_H__)
#define __DRM_PDP_GEM_H__

#include <linux/version.h>
#include <drm/drmP.h>

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0))
#include <drm/drm_gem.h>
#endif

struct pdp_gem_private;

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
void pdp_gem_object_free_priv(struct pdp_gem_private *gem_priv,
			      struct drm_gem_object *obj);

struct dma_buf *pdp_gem_prime_export(struct drm_device *dev,
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
int pdp_gem_object_vm_fault(struct vm_fault *vmf);
#else
int pdp_gem_object_vm_fault(struct vm_area_struct *vma, struct vm_fault *vmf);
#endif

/* internal interfaces */
struct reservation_object *pdp_gem_get_resv(struct drm_gem_object *obj);
u64 pdp_gem_get_dev_addr(struct drm_gem_object *obj);

#endif /* !defined(__DRM_PDP_GEM_H__) */
