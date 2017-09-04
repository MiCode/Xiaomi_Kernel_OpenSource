/*
 * Copyright (C) 2013 Red Hat
 * Author: Rob Clark <robdclark@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __MSM_GEM_H__
#define __MSM_GEM_H__

#include <linux/kref.h>
#include <linux/reservation.h>
#include <linux/mmu_notifier.h>
#include <linux/interval_tree.h>
#include "msm_drv.h"

/* Additional internal-use only BO flags: */
#define MSM_BO_STOLEN        0x10000000    /* try to use stolen/splash memory */
#define MSM_BO_LOCKED        0x20000000    /* Pages have been securely locked */
#define MSM_BO_SVM           0x40000000    /* bo is SVM */

struct msm_gem_address_space {
	const char *name;
	struct msm_mmu *mmu;
	struct kref kref;
	struct drm_mm mm;
	spinlock_t lock; /* Protects drm_mm node allocation/removal */
	u64 va_len;
};

struct msm_gem_vma {
	/* Node used by the GPU address space, but not the SDE address space */
	struct drm_mm_node node;
	struct msm_gem_address_space *aspace;
	uint64_t iova;
	struct list_head list;
};

struct msm_gem_object {
	struct drm_gem_object base;

	uint32_t flags;

	/* And object is either:
	 *  inactive - on priv->inactive_list
	 *  active   - on one one of the gpu's active_list..  well, at
	 *     least for now we don't have (I don't think) hw sync between
	 *     2d and 3d one devices which have both, meaning we need to
	 *     block on submit if a bo is already on other ring
	 *
	 */
	struct list_head mm_list;
	struct msm_gpu *gpu;     /* non-null if active */
	uint32_t read_fence, write_fence;

	/* Transiently in the process of submit ioctl, objects associated
	 * with the submit are on submit->bo_list.. this only lasts for
	 * the duration of the ioctl, so one bo can never be on multiple
	 * submit lists.
	 */
	struct list_head submit_entry;

	struct page **pages;
	struct sg_table *sgt;
	void *vaddr;

	struct list_head domains;

	/* normally (resv == &_resv) except for imported bo's */
	struct reservation_object *resv;
	struct reservation_object _resv;

	/* For physically contiguous buffers.  Used when we don't have
	 * an IOMMU.  Also used for stolen/splashscreen buffer.
	 */
	struct drm_mm_node *vram_node;
	struct mutex lock; /* Protects resources associated with bo */
};
#define to_msm_bo(x) container_of(x, struct msm_gem_object, base)

struct msm_mmu_notifier {
	struct mmu_notifier mn;
	struct mm_struct *mm; /* mm_struct owning the mmu notifier mn */
	struct hlist_node node;
	struct rb_root svm_tree; /* interval tree holding all svm bos */
	spinlock_t svm_tree_lock; /* Protects svm_tree*/
	struct msm_drm_private *msm_dev;
	struct kref refcount;
};

struct msm_gem_svm_object {
	struct msm_gem_object msm_obj_base;
	uint64_t hostptr;
	struct mm_struct *mm; /* mm_struct the svm bo belongs to */
	struct interval_tree_node svm_node;
	struct msm_mmu_notifier *msm_mn;
	struct list_head lnode;
	/* bo has been unmapped on CPU, cannot be part of GPU submits */
	bool invalid;
};

#define to_msm_svm_obj(x) \
	((struct msm_gem_svm_object *) \
	 container_of(x, struct msm_gem_svm_object, msm_obj_base))


static inline bool is_active(struct msm_gem_object *msm_obj)
{
	return msm_obj->gpu != NULL;
}

static inline uint32_t msm_gem_fence(struct msm_gem_object *msm_obj,
		uint32_t op)
{
	uint32_t fence = 0;

	if (op & MSM_PREP_READ)
		fence = msm_obj->write_fence;
	if (op & MSM_PREP_WRITE)
		fence = max(fence, msm_obj->read_fence);

	return fence;
}

/* Internal submit flags */
#define SUBMIT_FLAG_SKIP_HANGCHECK 0x00000001

/* Created per submit-ioctl, to track bo's and cmdstream bufs, etc,
 * associated with the cmdstream submission for synchronization (and
 * make it easier to unwind when things go wrong, etc).  This only
 * lasts for the duration of the submit-ioctl.
 */
struct msm_gem_submit {
	struct drm_device *dev;
	struct msm_gem_address_space *aspace;
	struct list_head node;   /* node in ring submit list */
	struct list_head bo_list;
	struct ww_acquire_ctx ticket;
	uint32_t fence;
	int ring;
	u32 flags;
	uint64_t profile_buf_iova;
	struct drm_msm_gem_submit_profile_buffer *profile_buf;
	bool secure;
	struct msm_gpu_submitqueue *queue;
	int tick_index;
	unsigned int nr_cmds;
	unsigned int nr_bos;
	struct {
		uint32_t type;
		uint32_t size;  /* in dwords */
		uint64_t iova;
		uint32_t idx;   /* cmdstream buffer idx in bos[] */
	} *cmd;  /* array of size nr_cmds */
	struct {
		uint32_t flags;
		struct msm_gem_object *obj;
		uint64_t iova;
	} bos[0];
};

#endif /* __MSM_GEM_H__ */
