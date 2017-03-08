/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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
#include <asm/sizes.h>
#include <linux/kthread.h>

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
struct msm_mdss;
struct msm_rd_state;
struct msm_perf_state;
struct msm_gem_submit;
struct msm_fence_context;
struct msm_fence_cb;

#define NUM_DOMAINS    4    /* one for KMS, then one per gpu core (?) */
#define MAX_CRTCS      8
#define MAX_PLANES     20
#define MAX_ENCODERS   8
#define MAX_BRIDGES    8
#define MAX_CONNECTORS 8

struct msm_file_private {
	/* currently we don't do anything useful with this.. but when
	 * per-context address spaces are supported we'd keep track of
	 * the context's page-tables here.
	 */
	int dummy;
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
	PLANE_PROP_EXCL_RECT_V1,

	/* enum/bitmask properties */
	PLANE_PROP_ROTATION,
	PLANE_PROP_BLEND_OP,
	PLANE_PROP_SRC_CONFIG,

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
	CRTC_PROP_DIM_LAYER_V1,
	CRTC_PROP_CORE_CLK,
	CRTC_PROP_CORE_AB,
	CRTC_PROP_CORE_IB,

	/* total # of properties */
	CRTC_PROP_COUNT
};

enum msm_mdp_conn_property {
	/* blob properties, always put these first */
	CONNECTOR_PROP_SDE_INFO,

	/* # of blob properties */
	CONNECTOR_PROP_BLOBCOUNT,

	/* range properties */
	CONNECTOR_PROP_OUT_FB = CONNECTOR_PROP_BLOBCOUNT,
	CONNECTOR_PROP_RETIRE_FENCE,
	CONNECTOR_PROP_DST_X,
	CONNECTOR_PROP_DST_Y,
	CONNECTOR_PROP_DST_W,
	CONNECTOR_PROP_DST_H,

	/* enum/bitmask properties */
	CONNECTOR_PROP_TOPOLOGY_NAME,
	CONNECTOR_PROP_TOPOLOGY_CONTROL,

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
 */
enum msm_display_caps {
	MSM_DISPLAY_CAP_VID_MODE	= BIT(0),
	MSM_DISPLAY_CAP_CMD_MODE	= BIT(1),
	MSM_DISPLAY_CAP_HOT_PLUG	= BIT(2),
	MSM_DISPLAY_CAP_EDID		= BIT(3),
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
 * @is_primary:         Set to true if display is primary display
 * @frame_rate:		Display frame rate
 * @prefill_lines:	prefill lines based on porches.
 * @vtotal:		display vertical total
 * @jitter:		display jitter configuration
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

	bool is_primary;
	uint32_t frame_rate;
	uint32_t prefill_lines;
	uint32_t vtotal;
	uint32_t jitter;

	enum msm_display_compression compression;
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
	u8 data[];
};

/* Commit thread specific structure */
struct msm_drm_commit {
	struct drm_device *dev;
	struct task_struct *thread;
	unsigned int crtc_id;
	struct kthread_worker worker;
};

struct msm_drm_private {

	struct drm_device *dev;

	struct msm_kms *kms;

	struct sde_power_handle phandle;
	struct sde_power_client *pclient;

	/* subordinate devices, if present: */
	struct platform_device *gpu_pdev;

	/* top level MDSS wrapper device (for MDP5 only) */
	struct msm_mdss *mdss;

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
	struct msm_file_private *lastctx;

	struct drm_fb_helper *fbdev;

	struct msm_rd_state *rd;
	struct msm_perf_state *perf;

	/* list of GEM objects: */
	struct list_head inactive_list;

	struct workqueue_struct *wq;

	/* crtcs pending async atomic updates: */
	uint32_t pending_crtcs;
	wait_queue_head_t pending_crtcs_event;

	/* registered MMUs: */
	unsigned int num_mmus;
	struct msm_mmu *mmus[NUM_DOMAINS];

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
	} vram;

	struct notifier_block vmap_notifier;
	struct shrinker shrinker;

	struct msm_vblank_ctrl vblank_ctrl;

	/* task holding struct_mutex.. currently only used in submit path
	 * to detect and reject faults from copy_from_user() for submit
	 * ioctl.
	 */
	struct task_struct *struct_mutex_task;

	/* list of clients waiting for events */
	struct list_head client_event_list;

	/* whether registered and drm_dev_unregister should be called */
	bool registered;
};

struct msm_format {
	uint32_t pixel_format;
};

int msm_atomic_check(struct drm_device *dev,
		     struct drm_atomic_state *state);
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

int msm_atomic_commit(struct drm_device *dev,
		struct drm_atomic_state *state, bool nonblock);

int msm_register_mmu(struct drm_device *dev, struct msm_mmu *mmu);
void msm_unregister_mmu(struct drm_device *dev, struct msm_mmu *mmu);

void msm_gem_submit_free(struct msm_gem_submit *submit);
int msm_ioctl_gem_submit(struct drm_device *dev, void *data,
		struct drm_file *file);

void msm_gem_shrinker_init(struct drm_device *dev);
void msm_gem_shrinker_cleanup(struct drm_device *dev);

int msm_gem_mmap_obj(struct drm_gem_object *obj,
			struct vm_area_struct *vma);
int msm_gem_mmap(struct file *filp, struct vm_area_struct *vma);
int msm_gem_fault(struct vm_area_struct *vma, struct vm_fault *vmf);
uint64_t msm_gem_mmap_offset(struct drm_gem_object *obj);
int msm_gem_get_iova_locked(struct drm_gem_object *obj, int id,
		uint32_t *iova);
int msm_gem_get_iova(struct drm_gem_object *obj, int id, uint32_t *iova);
uint32_t msm_gem_iova(struct drm_gem_object *obj, int id);
struct page **msm_gem_get_pages(struct drm_gem_object *obj);
void msm_gem_put_pages(struct drm_gem_object *obj);
void msm_gem_put_iova(struct drm_gem_object *obj, int id);
int msm_gem_dumb_create(struct drm_file *file, struct drm_device *dev,
		struct drm_mode_create_dumb *args);
int msm_gem_dumb_map_offset(struct drm_file *file, struct drm_device *dev,
		uint32_t handle, uint64_t *offset);
struct sg_table *msm_gem_prime_get_sg_table(struct drm_gem_object *obj);
void *msm_gem_prime_vmap(struct drm_gem_object *obj);
void msm_gem_prime_vunmap(struct drm_gem_object *obj, void *vaddr);
int msm_gem_prime_mmap(struct drm_gem_object *obj, struct vm_area_struct *vma);
struct drm_gem_object *msm_gem_prime_import_sg_table(struct drm_device *dev,
		struct dma_buf_attachment *attach, struct sg_table *sg);
int msm_gem_prime_pin(struct drm_gem_object *obj);
void msm_gem_prime_unpin(struct drm_gem_object *obj);
void *msm_gem_get_vaddr_locked(struct drm_gem_object *obj);
void *msm_gem_get_vaddr(struct drm_gem_object *obj);
void msm_gem_put_vaddr_locked(struct drm_gem_object *obj);
void msm_gem_put_vaddr(struct drm_gem_object *obj);
int msm_gem_madvise(struct drm_gem_object *obj, unsigned madv);
void msm_gem_purge(struct drm_gem_object *obj);
void msm_gem_vunmap(struct drm_gem_object *obj);
int msm_gem_sync_object(struct drm_gem_object *obj,
		struct msm_fence_context *fctx, bool exclusive);
void msm_gem_move_to_active(struct drm_gem_object *obj,
		struct msm_gpu *gpu, bool exclusive, struct fence *fence);
void msm_gem_move_to_inactive(struct drm_gem_object *obj);
int msm_gem_cpu_prep(struct drm_gem_object *obj, uint32_t op, ktime_t *timeout);
int msm_gem_cpu_fini(struct drm_gem_object *obj);
void msm_gem_free_object(struct drm_gem_object *obj);
int msm_gem_new_handle(struct drm_device *dev, struct drm_file *file,
		uint32_t size, uint32_t flags, uint32_t *handle);
struct drm_gem_object *msm_gem_new(struct drm_device *dev,
		uint32_t size, uint32_t flags);
struct drm_gem_object *msm_gem_import(struct drm_device *dev,
		struct dma_buf *dmabuf, struct sg_table *sgt);

int msm_framebuffer_prepare(struct drm_framebuffer *fb, int id);
void msm_framebuffer_cleanup(struct drm_framebuffer *fb, int id);
uint32_t msm_framebuffer_iova(struct drm_framebuffer *fb, int id, int plane);
struct drm_gem_object *msm_framebuffer_bo(struct drm_framebuffer *fb, int plane);
const struct msm_format *msm_framebuffer_format(struct drm_framebuffer *fb);
struct drm_framebuffer *msm_framebuffer_init(struct drm_device *dev,
		const struct drm_mode_fb_cmd2 *mode_cmd, struct drm_gem_object **bos);
struct drm_framebuffer *msm_framebuffer_create(struct drm_device *dev,
		struct drm_file *file, const struct drm_mode_fb_cmd2 *mode_cmd);

struct drm_fb_helper *msm_fbdev_init(struct drm_device *dev);
void msm_fbdev_free(struct drm_device *dev);

struct hdmi;
int msm_hdmi_modeset_init(struct hdmi *hdmi, struct drm_device *dev,
		struct drm_encoder *encoder);
void __init msm_hdmi_register(void);
void __exit msm_hdmi_unregister(void);

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

void __init msm_mdp_register(void);
void __exit msm_mdp_unregister(void);

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

static inline unsigned long timeout_to_jiffies(const ktime_t *timeout)
{
	ktime_t now = ktime_get();
	unsigned long remaining_jiffies;

	if (ktime_compare(*timeout, now) < 0) {
		remaining_jiffies = 0;
	} else {
		ktime_t rem = ktime_sub(*timeout, now);
		struct timespec ts = ktime_to_timespec(rem);
		remaining_jiffies = timespec_to_jiffies(&ts);
	}

	return remaining_jiffies;
}

#endif /* __MSM_DRV_H__ */
