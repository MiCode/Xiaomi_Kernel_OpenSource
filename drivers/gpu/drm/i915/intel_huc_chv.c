/*
 * Copyright © 2014 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */
#include <linux/firmware.h>
#include <drm/drmP.h>
#include <drm/i915_drm.h>
#include "i915_drv.h"

#define I915_HUC_UCODE_CHV "i915/huc_gen8.bin"
MODULE_FIRMWARE(I915_HUC_UCODE_CHV);

#define HUC_ERROR_OUT(a) \
	{\
		DRM_ERROR("HuC: %s\n", #a);\
		goto out;\
	}

static int i915_gem_object_write(struct drm_i915_gem_object *obj,
				 const void *data, const size_t size)
{
	struct sg_table *sg;
	size_t bytes;
	int ret;

	ret = i915_gem_object_set_to_cpu_domain(obj, true);
	if (ret)
		return ret;

	ret = i915_gem_object_get_pages(obj);
	if (ret)
		return ret;

	i915_gem_object_pin_pages(obj);

	sg = obj->pages;

	bytes = sg_copy_from_buffer(sg->sgl, sg->nents,
				    (void *)data, (size_t)size);

	i915_gem_object_unpin_pages(obj);

	if (WARN(bytes != size,
		"Failed to upload data (completed %zu bytes out of %zu total)",
		 bytes, size)) {
		i915_gem_object_put_pages(obj);
		return -EIO;
	}

	ret = i915_gem_object_set_to_gtt_domain(obj, false);
	if (ret)
		return ret;

	return 0;
}

static struct drm_i915_gem_object *create_fw_obj(struct drm_device *dev,
		const struct firmware *fw, u32 *fw_size)
{
	struct drm_i915_private *dev_priv = NULL;
	struct drm_i915_gem_object *obj = NULL;
	const void *fw_data = NULL;
	int ret = 0;

	if (!fw)
		HUC_ERROR_OUT("Null fw object");

	dev_priv = dev->dev_private;

	fw_data = fw->data;
	*fw_size = fw->size;

	obj = i915_gem_alloc_object(dev, round_up(*fw_size, PAGE_SIZE));
	if (!obj)
		HUC_ERROR_OUT("Failed allocation");

	ret = i915_gem_obj_ggtt_pin(obj, 4096, 0);
	if (ret)
		HUC_ERROR_OUT("Failed to pin");

	ret = i915_gem_object_write(obj, fw_data, *fw_size);
	if (ret) {
		i915_gem_object_ggtt_unpin(obj);
		HUC_ERROR_OUT("Failed to write");
	}

out:
	if (ret && obj) {
		drm_gem_object_unreference(&obj->base);
		obj = NULL;
	}

	release_firmware(fw);
	return obj;
}

static struct drm_i915_gem_object *create_huc_batch(struct drm_device *dev,
	struct drm_i915_gem_object *fw_obj, u32 fw_size)

{
	#define CHV_DMA_GUC_OFFSET 0xc340
	#define   CHV_GEN8_DMA_GUC_OFFSET (0x80000)
	#define   CHV_GUC_OFFSET_VALID (1)
	#define CHV_DMA_GUC_SIZE 0xc050

	#define FIRMWARE_ADDR	12
	#define FIRMWARE_SIZE	19

	static u32 load_cmds[] = {
		0x11000001, 0x0001229C, 0x01000100, 0x11000001,
		0x000320f0, 0x42004200, 0x13400002, 0x00000004,
		0x00000000, 0x00000000, 0x00000000, 0x76000005,
		0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x7608000B, 0xC0000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x13000000,
		0x00000004, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x05000000,
	};

	struct drm_i915_private *dev_priv = NULL;
	struct drm_i915_gem_object *obj = NULL;
	int ret = 0;

	dev_priv = dev->dev_private;

	/* Setup needed before we can load the firmware */
	I915_WRITE(GEN6_MBCTL, I915_READ(GEN6_MBCTL) |
			GEN6_MBCTL_ENABLE_BOOT_FETCH);

	POSTING_READ(GEN6_MBCTL);

	I915_WRITE(CHV_DMA_GUC_OFFSET, CHV_GEN8_DMA_GUC_OFFSET);
	POSTING_READ(CHV_DMA_GUC_OFFSET);

	I915_WRITE(CHV_DMA_GUC_SIZE, 0);
	POSTING_READ(CHV_DMA_GUC_SIZE);

	obj = i915_gem_alloc_object(dev, 4096);
	if (!obj)
		HUC_ERROR_OUT("Failed allocation");

	ret = i915_gem_obj_ggtt_pin(obj, 4096, 0);
	if (ret)
		HUC_ERROR_OUT("Failed to pin");

	load_cmds[FIRMWARE_ADDR] |= i915_gem_obj_ggtt_offset(fw_obj);
	load_cmds[FIRMWARE_SIZE] |= fw_size;

	ret = i915_gem_object_write(obj, load_cmds, sizeof(load_cmds));
	if (ret) {
		i915_gem_object_ggtt_unpin(obj);
		HUC_ERROR_OUT("Failed to write");
	}
out:
	if (ret && obj) {
		drm_gem_object_unreference(&obj->base);
		obj = NULL;
	}

	return obj;
}

static void finish_chv_huc_load(const struct firmware *fw, void *context)
{
	#define CHV_VCS_GFX_MODE 0x1229C

	struct drm_i915_private *dev_priv = context;
	struct drm_device *dev = dev_priv->dev;
	struct drm_i915_gem_object *batch_obj = NULL;
	struct drm_i915_gem_object *fw_obj = NULL;
	struct intel_engine_cs *ring;
	struct intel_context *ctx;
	struct intel_ringbuffer *ringbuf;
	u32 seqno;
	u32 fw_size;
	int ret = 0;

	if (!fw) {
		DRM_ERROR("HuC: Null fw. Check fw binary file is present\n");
		return;
	}

	dev_priv = dev->dev_private;

	intel_runtime_pm_get(dev_priv);

	ret = i915_mutex_lock_interruptible(dev);
	if (ret) {
		DRM_ERROR("HuC: Unable to acquire mutex\n");
		intel_runtime_pm_put(dev_priv);
		return;
	}

	I915_WRITE(CHV_VCS_GFX_MODE, 0x00010001);

	fw_obj = create_fw_obj(dev, fw, &fw_size);
	if (!fw_obj)
		HUC_ERROR_OUT("Null fw obj");

	batch_obj = create_huc_batch(dev, fw_obj, fw_size);
	if (!batch_obj)
		HUC_ERROR_OUT("Null batch obj");

	ring = &dev_priv->ring[VCS];

	ctx = ring->default_context;
	if (IS_ERR(ctx))
		HUC_ERROR_OUT("No context");

	ringbuf = ctx->engine[ring->id].ringbuf;
	if (!ringbuf)
		HUC_ERROR_OUT("No ring obj");

	ret = ring->emit_bb_start(ringbuf,
			i915_gem_obj_ggtt_offset(batch_obj),
			I915_DISPATCH_SECURE);
	if (ret)
		HUC_ERROR_OUT("Failed to emit batch");

	i915_vma_move_to_active(i915_gem_obj_to_ggtt(batch_obj), ring);

	ret = __i915_add_request(ring, NULL, batch_obj, &seqno);
	if (ret)
		HUC_ERROR_OUT("Failed to add request");

	ret = i915_wait_seqno(ring, seqno);
	if (ret)
		HUC_ERROR_OUT("Batch didn't finish executing");

out:
	I915_WRITE(CHV_VCS_GFX_MODE, 0x00010000);

	if (fw_obj) {
		i915_gem_object_ggtt_unpin(fw_obj);
		drm_gem_object_unreference(&fw_obj->base);
	}
	if (batch_obj) {
		i915_gem_object_ggtt_unpin(batch_obj);
		drm_gem_object_unreference(&batch_obj->base);
	}

	mutex_unlock(&dev->struct_mutex);
	intel_runtime_pm_put(dev_priv);
	return;
}

/* On CHV only, call this function to load HuC firmware. It makes an
 * asychronous request to the request_firmware api and returns
 * immediately. finish_chv_huc_load is called when fw is available and
 * does the real work of loading the HuC.
 */
void intel_chv_huc_load(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = NULL;
	int ret;

	dev_priv = dev->dev_private;

	if (!IS_CHERRYVIEW(dev))
		return;

	ret = request_firmware_nowait(THIS_MODULE, true, I915_HUC_UCODE_CHV,
				&dev_priv->dev->pdev->dev,
				GFP_KERNEL, dev_priv, finish_chv_huc_load);
	if (ret)
		HUC_ERROR_OUT("Failed request_firmware_nowait call");
out:
	return;
}

