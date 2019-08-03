/*
 * Copyright (c) 2016-2019, The Linux Foundation. All rights reserved.
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

#ifndef __MSM_DRV_H__
#define __MSM_DRV_H__

#include <linux/kernel.h>
#include <linux/clk.h>
#include <linux/cpufreq.h>
#include <linux/module.h>
#include <linux/component.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/iommu.h>
#include <linux/types.h>
#include <linux/of_graph.h>
#include <linux/of_device.h>
#include <linux/sde_io_util.h>
#include <linux/hashtable.h>
#include <asm/sizes.h>
#include <linux/kthread.h>

#ifndef CONFIG_OF
#include <mach/board.h>
#include <mach/socinfo.h>
#include <mach/iommu_domains.h>
#endif

#include <drm/drmP.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_plane_helper.h>
#include <drm/drm_fb_helper.h>
#include <drm/msm_drm.h>
#include <drm/drm_gem.h>

#include "sde_power_handle.h"

#define GET_MAJOR_REV(rev)		((rev) >> 28)
#define GET_MINOR_REV(rev)		(((rev) >> 16) & 0xFFF)
#define GET_STEP_REV(rev)		((rev) & 0xFFFF)

struct msm_kms;
struct msm_gpu;
struct msm_mmu;
struct msm_rd_state;
struct msm_perf_state;
struct msm_gem_submit;
struct msm_gem_address_space;
struct msm_gem_vma;

#define NUM_DOMAINS    4    /* one for KMS, then one per gpu core (?) */
#define MAX_CRTCS      8
#define MAX_PLANES     12
#define MAX_ENCODERS   8
#define MAX_BRIDGES    8
#define MAX_CONNECTORS 8

struct msm_file_private {
	struct msm_gem_address_space *aspace;
	struct list_head counters;
	rwlock_t queuelock;
	struct list_head submitqueues;
	int queueid;
};

enum msm_mdp_plane_property {
	/* blob properties, always put these first */
	PLANE_PROP_SCALER_V1,
	PLANE_PROP_SCALER_V2,
	PLANE_PROP_CSC_V1,
	PLANE_PROP_INFO,
	PLANE_PROP_SCALER_LUT_ED,
	PLANE_PROP_SCALER_LUT_CIR,
	PLANE_PROP_SCALER_LUT_SEP,
	PLANE_PROP_SKIN_COLOR,
	PLANE_PROP_SKY_COLOR,
	PLANE_PROP_FOLIAGE_COLOR,

	/* # of blob properties */
	PLANE_PROP_BLOBCOUNT,

	/* range properties */
	PLANE_PROP_ZPOS = PLANE_PROP_BLOBCOUNT,
	PLANE_PROP_ALPHA,
	PLANE_PROP_COLOR_FILL,
	PLANE_PROP_H_DECIMATE,
	PLANE_PROP_V_DECIMATE,
	PLANE_PROP_INPUT_FENCE,
	PLANE_PROP_HUE_ADJUST,
	PLANE_PROP_SATURATION_ADJUST,
	PLANE_PROP_VALUE_ADJUST,
	PLANE_PROP_CONTRAST_ADJUST,

	/* enum/bitmask properties */
	PLANE_PROP_ROTATION,
	PLANE_PROP_BLEND_OP,
	PLANE_PROP_SRC_CONFIG,
	PLANE_PROP_FB_TRANSLATION_MODE,

	/* total # of properties */
	PLANE_PROP_COUNT
};

enum msm_mdp_crtc_property {
	CRTC_PROP_INFO,

	/* # of blob properties */
	CRTC_PROP_BLOBCOUNT,

	/* range properties */
	CRTC_PROP_INPUT_FENCE_TIMEOUT = CRTC_PROP_BLOBCOUNT,
	CRTC_PROP_OUTPUT_FENCE,
	CRTC_PROP_OUTPUT_FENCE_OFFSET,
	CRTC_PROP_CORE_CLK,
	CRTC_PROP_CORE_AB,
	CRTC_PROP_CORE_IB,
	CRTC_PROP_SECURITY_LEVEL,

	/* total # of properties */
	CRTC_PROP_COUNT
};

enum msm_mdp_conn_property {
	/* blob properties, always put these first */
	CONNECTOR_PROP_SDE_INFO,
	CONNECTOR_PROP_HDR_INFO,
	CONNECTOR_PROP_HDR_CONTROL,

	/* # of blob properties */
	CONNECTOR_PROP_BLOBCOUNT,

	/* range properties */
	CONNECTOR_PROP_OUT_FB = CONNECTOR_PROP_BLOBCOUNT,
	CONNECTOR_PROP_RETIRE_FENCE,
	CONNECTOR_PROP_DST_X,
	CONNECTOR_PROP_DST_Y,
	CONNECTOR_PROP_DST_W,
	CONNECTOR_PROP_DST_H,
	CONNECTOR_PROP_PLL_DELTA,
	CONNECTOR_PROP_PLL_ENABLE,
	CONNECTOR_PROP_HDCP_VERSION,

	/* enum/bitmask properties */
	CONNECTOR_PROP_TOPOLOGY_NAME,
	CONNECTOR_PROP_TOPOLOGY_CONTROL,
	CONNECTOR_PROP_LP,
	CONNECTOR_PROP_HPD_OFF,

	/* total # of properties */
	CONNECTOR_PROP_COUNT
};

struct msm_vblank_ctrl {
	struct kthread_work work;
	struct list_head event_list;
	spinlock_t lock;
};

#define MAX_H_TILES_PER_DISPLAY 2

/**
 * enum msm_display_compression - compression method used for pixel stream
 * @MSM_DISPLAY_COMPRESS_NONE:     Pixel data is not compressed
 * @MSM_DISPLAY_COMPRESS_DSC:      DSC compresison is used
 * @MSM_DISPLAY_COMPRESS_FBC:      FBC compression is used
 */
enum msm_display_compression {
	MSM_DISPLAY_COMPRESS_NONE,
	MSM_DISPLAY_COMPRESS_DSC,
	MSM_DISPLAY_COMPRESS_FBC,
};

/**
 * enum msm_display_caps - features/capabilities supported by displays
 * @MSM_DISPLAY_CAP_VID_MODE:           Video or "active" mode supported
 * @MSM_DISPLAY_CAP_CMD_MODE:           Command mode supported
 * @MSM_DISPLAY_CAP_HOT_PLUG:           Hot plug detection supported
 * @MSM_DISPLAY_CAP_EDID:               EDID supported
 * @MSM_DISPLAY_CAP_SHARED:             Display is shared
 */
enum msm_display_caps {
	MSM_DISPLAY_CAP_VID_MODE	= BIT(0),
	MSM_DISPLAY_CAP_CMD_MODE	= BIT(1),
	MSM_DISPLAY_CAP_HOT_PLUG	= BIT(2),
	MSM_DISPLAY_CAP_EDID		= BIT(3),
	MSM_DISPLAY_CAP_SHARED		= BIT(4),
};

/**
 * struct msm_display_info - defines display properties
 * @intf_type:          DRM_MODE_CONNECTOR_ display type
 * @capabilities:       Bitmask of display flags
 * @num_of_h_tiles:     Number of horizontal tiles in case of split interface
 * @h_tile_instance:    Controller instance used per tile. Number of elements is
 *                      based on num_of_h_tiles
 * @is_connected:       Set to true if display is connected
 * @width_mm:           Physical width
 * @height_mm:          Physical height
 * @max_width:          Max width of display. In case of hot pluggable display
 *                      this is max width supported by controller
 * @max_height:         Max height of display. In case of hot pluggable display
 *                      this is max height supported by controller
 * @compression:        Compression supported by the display
 */
struct msm_display_info {
	int intf_type;
	uint32_t capabilities;

	uint32_t num_of_h_tiles;
	uint32_t h_tile_instance[MAX_H_TILES_PER_DISPLAY];

	bool is_connected;

	unsigned int width_mm;
	unsigned int height_mm;

	uint32_t max_width;
	uint32_t max_height;

	enum msm_display_compression compression;
};

/**
 * struct - msm_display_kickoff_params - info for display features at kickoff
 * @hdr_ctrl: HDR control info passed from userspace
 */
struct msm_display_kickoff_params {
	struct drm_msm_ext_panel_hdr_ctrl *hdr_ctrl;
};

/**
 * struct msm_drm_event - defines custom event notification struct
 * @base: base object required for event notification by DRM framework.
 * @event: event object required for event notification by DRM framework.
 * @info: contains information of DRM object for which events has been
 *        requested.
 * @data: memory location which contains response payload for event.
 */
struct msm_drm_event {
	struct drm_pending_event base;
	struct drm_event event;
	struct drm_msm_event_req info;
	u8 data[];
};

/* Commit thread specific structure */
struct msm_drm_commit {
	struct drm_device *dev;
	struct task_struct *thread;
	unsigned int crtc_id;
	struct kthread_worker worker;
};

#define MSM_GPU_MAX_RINGS 4

struct msm_drm_private {

	struct msm_kms *kms;

	struct sde_power_handle phandle;
	struct sde_power_client *pclient;

	/* subordinate devices, if present: */
	struct platform_device *gpu_pdev;

	/* possibly this should be in the kms component, but it is
	 * shared by both mdp4 and mdp5..
	 */
	struct hdmi *hdmi;

	/* eDP is for mdp5 only, but kms has not been created
	 * when edp_bind() and edp_init() are called. Here is the only
	 * place to keep the edp instance.
	 */
	struct msm_edp *edp;

	/* DSI is shared by mdp4 and mdp5 */
	struct msm_dsi *dsi[2];

	/* when we have more than one 'msm_gpu' these need to be an array: */
	struct msm_gpu *gpu;

	struct drm_fb_helper *fbdev;

	uint32_t completed_fence[MSM_GPU_MAX_RINGS];

	wait_queue_head_t fence_event;

	struct msm_rd_state *rd;
	struct msm_perf_state *perf;

	/* list of GEM objects: */
	struct list_head inactive_list;

	struct workqueue_struct *wq;

	/* callbacks deferred until bo is inactive: */
	struct list_head fence_cbs;

	/* crtcs pending async atomic updates: */
	uint32_t pending_crtcs;
	uint32_t pending_planes;
	wait_queue_head_t pending_crtcs_event;

	/* Registered address spaces.. currently this is fixed per # of
	 * iommu's.  Ie. one for display block and one for gpu block.
	 * Eventually, to do per-process gpu pagetables, we'll want one
	 * of these per-process.
	 */
	unsigned int num_aspaces;
	struct msm_gem_address_space *aspace[NUM_DOMAINS];

	unsigned int num_planes;
	struct drm_plane *planes[MAX_PLANES];

	unsigned int num_crtcs;
	struct drm_crtc *crtcs[MAX_CRTCS];

	struct msm_drm_commit disp_thread[MAX_CRTCS];

	unsigned int num_encoders;
	struct drm_encoder *encoders[MAX_ENCODERS];

	unsigned int num_bridges;
	struct drm_bridge *bridges[MAX_BRIDGES];

	unsigned int num_connectors;
	struct drm_connector *connectors[MAX_CONNECTORS];

	/* hash to store mm_struct to msm_mmu_notifier mappings */
	DECLARE_HASHTABLE(mn_hash, 7);
	/* protects mn_hash and the msm_mmu_notifier for the process */
	struct mutex mn_lock;

	/* Properties */
	struct drm_property *plane_property[PLANE_PROP_COUNT];
	struct drm_property *crtc_property[CRTC_PROP_COUNT];
	struct drm_property *conn_property[CONNECTOR_PROP_COUNT];

	/* Color processing properties for the crtc */
	struct drm_property **cp_property;

	/* VRAM carveout, used when no IOMMU: */
	struct {
		unsigned long size;
		dma_addr_t paddr;
		/* NOTE: mm managed at the page level, size is in # of pages
		 * and position mm_node->start is in # of pages:
		 */
		struct drm_mm mm;
		spinlock_t lock; /* Protects drm_mm node allocation/removal */
	} vram;

	struct msm_vblank_ctrl vblank_ctrl;

	/* saved atomic state during system suspend */
	struct drm_atomic_state *suspend_state;

	/* list of clients waiting for events */
	struct list_head client_event_list;

	/* update the flag when msm driver receives shutdown notification */
	bool shutdown_in_progress;
};

struct msm_format {
	uint32_t pixel_format;
};

/*
 * Some GPU targets can support multiple ringbuffers and preempt between them.
 * In order to do this without massive API changes we will steal two bits from
 * the top of the fence and use them to identify the ringbuffer, (0x00000001 for
 * riug 0, 0x40000001 for ring 1, 0x50000001 for ring 2, etc). If you are going
 * to do a fence comparision you have to make sure you are only comparing
 * against fences from the same ring, but since fences within a ringbuffer are
 * still contigious you can still use straight comparisons (i.e 0x40000001 is
 * older than 0x40000002). Mathmatically there will be 0x3FFFFFFF timestamps
 * per ring or ~103 days of 120 interrupts per second (two interrupts per frame
 * at 60 FPS).
 */
#define FENCE_RING(_fence) ((_fence >> 30) & 3)
#define FENCE(_ring, _fence) ((((_ring) & 3) << 30) | ((_fence) & 0x3FFFFFFF))

static inline bool COMPARE_FENCE_LTE(uint32_t a, uint32_t b)
{
	return ((FENCE_RING(a) == FENCE_RING(b)) && a <= b);
}

static inline bool COMPARE_FENCE_LT(uint32_t a, uint32_t b)
{
	return ((FENCE_RING(a) == FENCE_RING(b)) && a < b);
}

/* callback from wq once fence has passed: */
struct msm_fence_cb {
	struct work_struct work;
	uint32_t fence;
	void (*func)(struct msm_fence_cb *cb);
};

void __msm_fence_worker(struct work_struct *work);

#define INIT_FENCE_CB(_cb, _func)  do {                     \
		INIT_WORK(&(_cb)->work, __msm_fence_worker); \
		(_cb)->func = _func;                         \
	} while (0)

static inline bool msm_is_suspend_state(struct drm_device *dev)
{
	if (!dev || !dev->dev_private)
		return false;

	return ((struct msm_drm_private *)dev->dev_private)->suspend_state !=
		NULL;
}

int msm_atomic_commit(struct drm_device *dev,
		struct drm_atomic_state *state, bool async);

int msm_wait_fence(struct drm_device *dev, uint32_t fence,
		ktime_t *timeout, bool interruptible);
int msm_queue_fence_cb(struct drm_device *dev,
		struct msm_fence_cb *cb, uint32_t fence);
void msm_update_fence(struct drm_device *dev, uint32_t fence);

void msm_gem_unmap_vma(struct msm_gem_address_space *aspace,
		struct msm_gem_vma *vma, struct sg_table *sgt,
		void *priv, bool invalidated);
int msm_gem_map_vma(struct msm_gem_address_space *aspace,
		struct msm_gem_vma *vma, struct sg_table *sgt,
		void *priv, unsigned int flags);
int msm_gem_reserve_iova(struct msm_gem_address_space *aspace,
		struct msm_gem_vma *domain,
		uint64_t hostptr, uint64_t size);
void msm_gem_release_iova(struct msm_gem_address_space *aspace,
		struct msm_gem_vma *vma);

void msm_gem_address_space_put(struct msm_gem_address_space *aspace);

/* For GPU and legacy display */
struct msm_gem_address_space *
msm_gem_address_space_create(struct device *dev, struct iommu_domain *domain,
		int type, const char *name);
struct msm_gem_address_space *
msm_gem_address_space_create_instance(struct msm_mmu *parent, const char *name,
		uint64_t start, uint64_t end);

/* For SDE  display */
struct msm_gem_address_space *
msm_gem_smmu_address_space_create(struct device *dev, struct msm_mmu *mmu,
		const char *name);

void msm_gem_submit_free(struct msm_gem_submit *submit);
int msm_ioctl_gem_submit(struct drm_device *dev, void *data,
		struct drm_file *file);

int msm_gem_mmap_obj(struct drm_gem_object *obj,
			struct vm_area_struct *vma);
int msm_gem_mmap(struct file *filp, struct vm_area_struct *vma);
int msm_gem_fault(struct vm_area_struct *vma, struct vm_fault *vmf);
uint64_t msm_gem_mmap_offset(struct drm_gem_object *obj);
int msm_gem_get_iova(struct drm_gem_object *obj,
		struct msm_gem_address_space *aspace, uint64_t *iova);
uint64_t msm_gem_iova(struct drm_gem_object *obj,
		struct msm_gem_address_space *aspace);
struct page **msm_gem_get_pages(struct drm_gem_object *obj);
void msm_gem_put_pages(struct drm_gem_object *obj);
void msm_gem_put_iova(struct drm_gem_object *obj,
		struct msm_gem_address_space *aspace);
int msm_gem_dumb_create(struct drm_file *file, struct drm_device *dev,
		struct drm_mode_create_dumb *args);
int msm_gem_dumb_map_offset(struct drm_file *file, struct drm_device *dev,
		uint32_t handle, uint64_t *offset);
struct sg_table *msm_gem_prime_get_sg_table(struct drm_gem_object *obj);
void *msm_gem_prime_vmap(struct drm_gem_object *obj);
void msm_gem_prime_vunmap(struct drm_gem_object *obj, void *vaddr);
int msm_gem_prime_mmap(struct drm_gem_object *obj, struct vm_area_struct *vma);
struct reservation_object *msm_gem_prime_res_obj(struct drm_gem_object *obj);
struct drm_gem_object *msm_gem_prime_import_sg_table(struct drm_device *dev,
		struct dma_buf_attachment *attach, struct sg_table *sg);
int msm_gem_prime_pin(struct drm_gem_object *obj);
void msm_gem_prime_unpin(struct drm_gem_object *obj);
void *msm_gem_vaddr(struct drm_gem_object *obj);
int msm_gem_queue_inactive_cb(struct drm_gem_object *obj,
		struct msm_fence_cb *cb);
void msm_gem_move_to_active(struct drm_gem_object *obj,
		struct msm_gpu *gpu, bool write, uint32_t fence);
void msm_gem_move_to_inactive(struct drm_gem_object *obj);
int msm_gem_cpu_prep(struct drm_gem_object *obj, uint32_t op,
		ktime_t *timeout);
int msm_gem_cpu_fini(struct drm_gem_object *obj);
void msm_gem_free_object(struct drm_gem_object *obj);
int msm_gem_new_handle(struct drm_device *dev, struct drm_file *file,
		uint32_t size, uint32_t flags, uint32_t *handle);
struct drm_gem_object *msm_gem_new(struct drm_device *dev,
		uint32_t size, uint32_t flags);
struct drm_gem_object *msm_gem_new_locked(struct drm_device *dev,
		uint32_t size, uint32_t flags);
struct drm_gem_object *msm_gem_import(struct drm_device *dev,
		uint32_t size, struct sg_table *sgt, u32 flags);
void msm_gem_sync(struct drm_gem_object *obj, u32 op);
int msm_gem_svm_new_handle(struct drm_device *dev, struct drm_file *file,
		uint64_t hostptr, uint64_t size,
		uint32_t flags, uint32_t *handle);
struct drm_gem_object *msm_gem_svm_new(struct drm_device *dev,
		struct drm_file *file, uint64_t hostptr,
		uint64_t size, uint32_t flags);
void *msm_gem_kernel_new(struct drm_device *dev, uint32_t size,
		uint32_t flags, struct msm_gem_address_space *aspace,
		struct drm_gem_object **bo, uint64_t *iova);
void *msm_gem_kernel_new_locked(struct drm_device *dev, uint32_t size,
		uint32_t flags, struct msm_gem_address_space *aspace,
		struct drm_gem_object **bo, uint64_t *iova);
int msm_framebuffer_prepare(struct drm_framebuffer *fb,
		struct msm_gem_address_space *aspace);
void msm_framebuffer_cleanup(struct drm_framebuffer *fb,
		struct msm_gem_address_space *aspace);
uint32_t msm_framebuffer_iova(struct drm_framebuffer *fb,
		struct msm_gem_address_space *aspace, int plane);
struct drm_gem_object *msm_framebuffer_bo(struct drm_framebuffer *fb, int plane);
const struct msm_format *msm_framebuffer_format(struct drm_framebuffer *fb);
struct drm_framebuffer *msm_framebuffer_init(struct drm_device *dev,
		struct drm_mode_fb_cmd2 *mode_cmd, struct drm_gem_object **bos);
struct drm_framebuffer *msm_framebuffer_create(struct drm_device *dev,
		struct drm_file *file, struct drm_mode_fb_cmd2 *mode_cmd);

struct drm_fb_helper *msm_fbdev_init(struct drm_device *dev);

struct msm_gpu_submitqueue;
int msm_submitqueue_init(struct msm_file_private *ctx);
struct msm_gpu_submitqueue *msm_submitqueue_get(struct msm_file_private *ctx,
		u32 id);
int msm_submitqueue_create(struct msm_file_private *ctx, u32 prio,
		u32 flags, u32 *id);
int msm_submitqueue_query(struct msm_file_private *ctx, u32 id, u32 param,
		void __user *data, u32 len);
int msm_submitqueue_remove(struct msm_file_private *ctx, u32 id);
void msm_submitqueue_close(struct msm_file_private *ctx);

void msm_submitqueue_destroy(struct kref *kref);

struct hdmi;
int hdmi_modeset_init(struct hdmi *hdmi, struct drm_device *dev,
		struct drm_encoder *encoder);
void __init hdmi_register(void);
void __exit hdmi_unregister(void);

struct msm_edp;
void __init msm_edp_register(void);
void __exit msm_edp_unregister(void);
int msm_edp_modeset_init(struct msm_edp *edp, struct drm_device *dev,
		struct drm_encoder *encoder);

struct msm_dsi;
enum msm_dsi_encoder_id {
	MSM_DSI_VIDEO_ENCODER_ID = 0,
	MSM_DSI_CMD_ENCODER_ID = 1,
	MSM_DSI_ENCODER_NUM = 2
};

/* *
 * msm_send_crtc_notification - notify user-space clients of crtc events.
 * @crtc: crtc that is generating the event.
 * @event: event that needs to be notified.
 * @payload: payload for the event.
 */
void msm_send_crtc_notification(struct drm_crtc *crtc,
		struct drm_event *event, u8 *payload);
#ifdef CONFIG_DRM_MSM_DSI
void __init msm_dsi_register(void);
void __exit msm_dsi_unregister(void);
int msm_dsi_modeset_init(struct msm_dsi *msm_dsi, struct drm_device *dev,
		struct drm_encoder *encoders[MSM_DSI_ENCODER_NUM]);
#else
static inline void __init msm_dsi_register(void)
{
}
static inline void __exit msm_dsi_unregister(void)
{
}
static inline int msm_dsi_modeset_init(struct msm_dsi *msm_dsi,
		struct drm_device *dev,
		struct drm_encoder *encoders[MSM_DSI_ENCODER_NUM])
{
	return -EINVAL;
}
#endif

#ifdef CONFIG_DEBUG_FS
void msm_gem_describe(struct drm_gem_object *obj, struct seq_file *m);
void msm_gem_describe_objects(struct list_head *list, struct seq_file *m);
void msm_framebuffer_describe(struct drm_framebuffer *fb, struct seq_file *m);
int msm_debugfs_late_init(struct drm_device *dev);
int msm_rd_debugfs_init(struct drm_minor *minor);
void msm_rd_debugfs_cleanup(struct drm_minor *minor);
void msm_rd_dump_submit(struct msm_gem_submit *submit);
int msm_perf_debugfs_init(struct drm_minor *minor);
void msm_perf_debugfs_cleanup(struct drm_minor *minor);
#else
static inline int msm_debugfs_late_init(struct drm_device *dev) { return 0; }
static inline void msm_rd_dump_submit(struct msm_gem_submit *submit) {}
#endif

void __iomem *msm_ioremap(struct platform_device *pdev, const char *name,
		const char *dbgname);
void msm_iounmap(struct platform_device *dev, void __iomem *addr);
void msm_writel(u32 data, void __iomem *addr);
u32 msm_readl(const void __iomem *addr);

#define DBG(fmt, ...) DRM_DEBUG(fmt"\n", ##__VA_ARGS__)
#define VERB(fmt, ...) if (0) DRM_DEBUG(fmt"\n", ##__VA_ARGS__)

static inline bool fence_completed(struct drm_device *dev, uint32_t fence)
{
	struct msm_drm_private *priv = dev->dev_private;

	return priv->completed_fence[FENCE_RING(fence)] >= fence;
}

static inline int align_pitch(int width, int bpp)
{
	int bytespp = (bpp + 7) / 8;
	/* adreno needs pitch aligned to 32 pixels: */
	return bytespp * ALIGN(width, 32);
}

/* for the generated headers: */
#define INVALID_IDX(idx) ({BUG(); 0;})
#define fui(x)                ({BUG(); 0;})
#define util_float_to_half(x) ({BUG(); 0;})


#define FIELD(val, name) (((val) & name ## __MASK) >> name ## __SHIFT)

/* for conditionally setting boolean flag(s): */
#define COND(bool, val) ((bool) ? (val) : 0)

#endif /* __MSM_DRV_H__ */
