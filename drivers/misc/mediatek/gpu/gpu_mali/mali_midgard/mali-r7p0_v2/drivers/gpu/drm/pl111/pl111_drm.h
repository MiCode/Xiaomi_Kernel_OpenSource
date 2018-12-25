/*
 *
 * (C) COPYRIGHT 2012-2013 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * A copy of the licence is included with the program, and can also be obtained
 * from Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 */



#ifndef _PL111_DRM_H_
#define _PL111_DRM_H_

#define DRIVER_AUTHOR    "ARM Ltd."
#define DRIVER_NAME      "pl111_drm"
#define DRIVER_DESC      "DRM module for PL111"
#define DRIVER_LICENCE   "GPL"
#define DRIVER_ALIAS     "platform:pl111_drm"
#define DRIVER_DATE      "20101111"
#define DRIVER_VERSION   "0.2"
#define DRIVER_MAJOR      2
#define DRIVER_MINOR      1
#define DRIVER_PATCHLEVEL 1

/*
 * Number of flips allowed in flight at any one time. Any more flips requested
 * beyond this value will cause the caller to block until earlier flips have
 * completed.
 *
 * For performance reasons, this must be greater than the number of buffers
 * used in the rendering pipeline. Note that the rendering pipeline can contain
 * different types of buffer, e.g.:
 * - 2 final framebuffers
 * - >2 geometry buffers for GPU use-cases
 * - >2 vertex buffers for GPU use-cases
 *
 * For example, a system using 5 geometry buffers could have 5 flips in flight,
 * and so NR_FLIPS_IN_FLIGHT_THRESHOLD must be 5 or greater.
 *
 * Whilst there may be more intermediate buffers (such as vertex/geometry) than
 * final framebuffers, KDS is used to ensure that GPU rendering waits for the
 * next off-screen buffer, so it doesn't overwrite an on-screen buffer and
 * produce tearing.
 */

/*
 * Here, we choose a conservative value. A lower value is most likely
 * suitable for GPU use-cases.
 */
#define NR_FLIPS_IN_FLIGHT_THRESHOLD 16

#define CLCD_IRQ_NEXTBASE_UPDATE (1u<<2)

struct pl111_drm_flip_resource;

struct pl111_gem_bo_dma {
	dma_addr_t fb_dev_addr;
	void *fb_cpu_addr;
};

struct pl111_gem_bo_shm {
	struct page **pages;
	dma_addr_t *dma_addrs;
};

struct pl111_gem_bo {
	struct drm_gem_object gem_object;
	u32 type;
	union {
		struct pl111_gem_bo_dma dma;
		struct pl111_gem_bo_shm shm;
	} backing_data;
	struct sg_table *sgt;
};

extern struct pl111_drm_dev_private priv;

struct pl111_drm_framebuffer {
	struct drm_framebuffer fb;
	struct pl111_gem_bo *bo;
};

struct pl111_drm_flip_resource {
#ifdef CONFIG_DMA_SHARED_BUFFER_USES_KDS
	/* This is the kds set associated to the dma_buf we want to flip */
	struct kds_resource_set *kds_res_set;
#endif
	struct drm_framebuffer *fb;
	struct drm_crtc *crtc;
	struct list_head link;
	bool page_flip;
	struct drm_pending_vblank_event *event;
};

struct pl111_drm_crtc {
	struct drm_crtc crtc;
	int crtc_index;

#ifdef CONFIG_DMA_SHARED_BUFFER_USES_KDS
	/* This protects "old_kds_res_set" and "displaying_fb" */
	spinlock_t current_displaying_lock;
	/*
	 * When a buffer is displayed its associated kds resource
	 * will be obtained and stored here. Every time a buffer
	 * flip is completed this old kds set is released and assigned
	 * the kds set of the new buffer.
	 */
	struct kds_resource_set *old_kds_res_set;
	/*
	 * Stores which frame buffer is currently being displayed by
	 * this CRTC or NULL if nothing is being displayed. It is used
	 * to tell whether we need to obtain a set of kds resources for
	 * exported buffer objects.
	 */
	struct drm_framebuffer *displaying_fb;
#endif
	struct drm_display_mode *new_mode;
	struct drm_display_mode *current_mode;
	int last_bpp;

	/*
	 * This spinlock protects "update_queue", "current_update_res"
	 * and calls to do_flip_to_res() which updates the CLCD base
	 * registers.
	 */
	spinlock_t base_update_lock;
	/*
	 * The resource that caused a base address update. Only one can be
	 * pending, hence it's != NULL if there's a pending update
	 */
	struct pl111_drm_flip_resource *current_update_res;
	/* Queue of things waiting to update the base address */
	struct list_head update_queue;

	void (*show_framebuffer_cb)(struct pl111_drm_flip_resource *flip_res,
				struct drm_framebuffer *fb);
};

struct pl111_drm_connector {
	struct drm_connector connector;
};

struct pl111_drm_encoder {
	struct drm_encoder encoder;
};

struct pl111_drm_dev_private {
	struct pl111_drm_crtc *pl111_crtc;

	struct amba_device *amba_dev;
	unsigned long mmio_start;
	__u32 mmio_len;
	void *regs;
	struct clk *clk;
#ifdef CONFIG_DMA_SHARED_BUFFER_USES_KDS
	struct kds_callback kds_cb;
	struct kds_callback kds_obtain_current_cb;
#endif
	/*
	 * Number of flips that were started in show_framebuffer_on_crtc(),
	 * but haven't completed yet - because we do deferred flipping
	 */
	atomic_t nr_flips_in_flight;
	wait_queue_head_t wait_for_flips;

	/*
	 * Used to prevent race between pl111_dma_buf_release and
	 * drm_gem_prime_handle_to_fd
	 */
	struct mutex export_dma_buf_lock;

	uint32_t number_crtcs;

	/* Cache for flip resources used to avoid kmalloc on each page flip */
	struct kmem_cache *page_flip_slab;
};

enum pl111_cursor_size {
	CURSOR_32X32,
	CURSOR_64X64
};

enum pl111_cursor_sync {
	CURSOR_SYNC_NONE,
	CURSOR_SYNC_VSYNC
};


/**
 * Buffer allocation function which is more flexible than dumb_create(),
 * it allows passing driver specific flags to control the kind of buffer
 * to be allocated.
 */
int pl111_drm_gem_create_ioctl(struct drm_device *dev, void *data,
				struct drm_file *file_priv);
 
/****** TODO MIDEGL-1718: this should be moved to uapi/include/drm/pl111_drm.h ********/

/* 
 * Parameters for different buffer objects:
 * bit [0]: backing storage
 *	(0 -> SHM)
 *	(1 -> DMA)
 * bit [2:1]: kind of mapping
 *	(0x0 -> uncached)
 *	(0x1 -> write combine)
 *	(0x2 -> cached)
 */
#define PL111_BOT_MASK		(0x7)
#define PL111_BOT_SHM		(0x0 << 0)
#define PL111_BOT_DMA		(0x1 << 0)
#define PL111_BOT_UNCACHED	(0x0 << 1)
#define PL111_BOT_WC		(0x1 << 1)
#define PL111_BOT_CACHED	(0x2 << 1)

/**
 * User-desired buffer creation information structure.
 *
 * @size: user-desired memory allocation size.
 *      - this size value would be page-aligned internally.
 * @flags: user request for setting memory type or cache attributes as a bit op
 *	- PL111_BOT_DMA / PL111_BOT_SHM
 *	- PL111_BOT_UNCACHED / PL111_BOT_WC / PL111_BOT_CACHED
 * @handle: returned a handle to created gem object.
 *      - this handle will be set by gem module of kernel side.
 */
struct drm_pl111_gem_create {
	uint32_t height;
	uint32_t width;
	uint32_t bpp;
	uint32_t flags;
	/* handle, pitch, size will be returned */
	uint32_t handle;
	uint32_t pitch;
	uint64_t size;
};

#define DRM_PL111_GEM_CREATE		0x00

#define DRM_IOCTL_PL111_GEM_CREATE	DRM_IOWR(DRM_COMMAND_BASE + \
			DRM_PL111_GEM_CREATE, struct drm_pl111_gem_create)
/****************************************************************************/

#define PL111_FB_FROM_FRAMEBUFFER(drm_fb) \
	(container_of(drm_fb, struct pl111_drm_framebuffer, fb))

#define PL111_BO_FROM_FRAMEBUFFER(drm_fb) \
	(container_of(drm_fb, struct pl111_drm_framebuffer, fb)->bo)

#define PL111_BO_FROM_GEM(gem_obj) \
	container_of(gem_obj, struct pl111_gem_bo, gem_object)

#define to_pl111_crtc(x) container_of(x, struct pl111_drm_crtc, crtc)

#define PL111_ENCODER_FROM_ENCODER(x) \
	container_of(x, struct pl111_drm_encoder, encoder)

#define PL111_CONNECTOR_FROM_CONNECTOR(x) \
	container_of(x, struct pl111_drm_connector, connector)

#include "pl111_drm_funcs.h"

#endif /* _PL111_DRM_H_ */
