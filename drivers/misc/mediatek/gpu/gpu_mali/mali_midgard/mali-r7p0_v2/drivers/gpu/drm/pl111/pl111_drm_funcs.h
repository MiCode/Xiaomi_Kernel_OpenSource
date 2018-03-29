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



/**
 * pl111_drm_funcs.h
 * Function prototypes for PL111 DRM
 */

#ifndef PL111_DRM_FUNCS_H_
#define PL111_DRM_FUNCS_H_

/* Platform Initialisation */
int pl111_drm_init(struct platform_device *dev);
void pl111_drm_exit(struct platform_device *dev);

/* KDS Callbacks */
void show_framebuffer_on_crtc_cb(void *cb1, void *cb2);
void release_kds_resource_and_display(struct pl111_drm_flip_resource *flip_res);

/* CRTC Functions */
struct pl111_drm_crtc *pl111_crtc_create(struct drm_device *dev);
struct pl111_drm_crtc *pl111_crtc_dummy_create(struct drm_device *dev);
void pl111_crtc_destroy(struct drm_crtc *crtc);

bool pl111_crtc_is_fb_currently_displayed(struct drm_device *dev,
					struct drm_framebuffer *fb);

int show_framebuffer_on_crtc(struct drm_crtc *crtc,
			struct drm_framebuffer *fb, bool page_flip,
			struct drm_pending_vblank_event *event);

/* Common IRQ handler */
void pl111_common_irq(struct pl111_drm_crtc *pl111_crtc);

int pl111_crtc_cursor_set(struct drm_crtc *crtc,
			   struct drm_file *file_priv,
			   uint32_t handle,
			   uint32_t width,
			   uint32_t height);
int pl111_crtc_cursor_move(struct drm_crtc *crtc,
			    int x, int y);

/* Connector Functions */
struct pl111_drm_connector *pl111_connector_create(struct drm_device *dev);
void pl111_connector_destroy(struct drm_connector *connector);
struct pl111_drm_connector *pl111_connector_dummy_create(struct drm_device
								*dev);

/* Encoder Functions */
struct pl111_drm_encoder *pl111_encoder_create(struct drm_device *dev,
						int possible_crtcs);
struct pl111_drm_encoder *pl111_encoder_dummy_create(struct drm_device *dev,
							int possible_crtcs);
void pl111_encoder_destroy(struct drm_encoder *encoder);

/* Frame Buffer Functions */
struct drm_framebuffer *pl111_fb_create(struct drm_device *dev,
					struct drm_file *file_priv,
					struct drm_mode_fb_cmd2 *mode_cmd);

/* VMA Functions */
int pl111_gem_fault(struct vm_area_struct *vma, struct vm_fault *vmf);
int pl111_gem_mmap(struct file *file_priv, struct vm_area_struct *vma);
struct page **get_pages(struct drm_gem_object *obj);
void put_pages(struct drm_gem_object *obj, struct page **pages);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0))
void pl111_drm_vm_open_locked(struct drm_device *dev,
                struct vm_area_struct *vma);
void pl111_gem_vm_open(struct vm_area_struct *vma);
void pl111_gem_vm_close(struct vm_area_struct *vma);
#endif

/* Suspend Functions */
int pl111_drm_resume(struct drm_device *dev);
int pl111_drm_suspend(struct drm_device *dev, pm_message_t state);

/* GEM Functions */
int pl111_dumb_create(struct drm_file *file_priv,
			struct drm_device *dev,
			struct drm_mode_create_dumb *args);
int pl111_dumb_destroy(struct drm_file *file_priv,
			struct drm_device *dev, uint32_t handle);
int pl111_dumb_map_offset(struct drm_file *file_priv,
			struct drm_device *dev, uint32_t handle,
			uint64_t *offset);
void pl111_gem_free_object(struct drm_gem_object *obj);

int pl111_bo_mmap(struct drm_gem_object *obj, struct pl111_gem_bo *bo,
			struct vm_area_struct *vma, size_t size);
void pl111_gem_sync_to_cpu(struct pl111_gem_bo *bo, int pgoff);
void pl111_gem_sync_to_dma(struct pl111_gem_bo *bo);

/* DMA BUF Functions */
struct drm_gem_object *pl111_gem_prime_import(struct drm_device *dev,
				       struct dma_buf *dma_buf);
int pl111_prime_handle_to_fd(struct drm_device *dev, struct drm_file *file_priv,
			uint32_t handle, uint32_t flags, int *prime_fd);
struct dma_buf *pl111_gem_prime_export(struct drm_device *dev,
				struct drm_gem_object *obj, int flags);

/* Pl111 Functions */
void show_framebuffer_on_crtc_cb_internal(struct pl111_drm_flip_resource
					*flip_res, struct drm_framebuffer *fb);
int clcd_disable(struct drm_crtc *crtc);
void do_flip_to_res(struct pl111_drm_flip_resource *flip_res);
int pl111_amba_probe(struct amba_device *dev, const struct amba_id *id);
int pl111_amba_remove(struct amba_device *dev);

int pl111_device_init(struct drm_device *dev);
void pl111_device_fini(struct drm_device *dev);

void pl111_convert_drm_mode_to_timing(struct drm_display_mode *mode,
					struct clcd_regs *timing);
void pl111_convert_timing_to_drm_mode(struct clcd_regs *timing,
					struct drm_display_mode *mode);
#endif /* PL111_DRM_FUNCS_H_ */
