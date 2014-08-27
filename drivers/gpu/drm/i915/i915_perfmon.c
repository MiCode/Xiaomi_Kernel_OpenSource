/*
 * Copyright  2013 Intel Corporation
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
 *
 */
#include <drm/i915_drm.h>
#include "i915_drv.h"
#include "i915_trace.h"
#include "intel_drv.h"
#include "linux/wait.h"

/**
 * intel_enable_perfmon_interrupt - enable perfmon interrupt
 *
 */
static int intel_enable_perfmon_interrupt(struct drm_device *dev,
						int enable)
{
	struct drm_i915_private *dev_priv = (struct drm_i915_private *) dev->dev_private;
	unsigned long irqflags;

	if (!(IS_GEN7(dev)) && !(IS_GEN8(dev)))
		return -EINVAL;

	spin_lock_irqsave(&dev_priv->irq_lock, irqflags);
	if (enable)
		if (IS_GEN7(dev))
			ilk_enable_gt_irq(dev_priv,
				GT_RENDER_PERFMON_BUFFER_INTERRUPT);
		else
			gen8_enable_oa_interrupt(dev_priv,
				GT_RENDER_PERFMON_BUFFER_INTERRUPT);
	else
		if (IS_GEN7(dev))
			ilk_disable_gt_irq(dev_priv,
				GT_RENDER_PERFMON_BUFFER_INTERRUPT);
		else
			gen8_disable_oa_interrupt(dev_priv,
				GT_RENDER_PERFMON_BUFFER_INTERRUPT);

	spin_unlock_irqrestore(&dev_priv->irq_lock, irqflags);

	return 0;
}

/**
 * intel_wait_perfmon_interrupt - wait for perfmon buffer interrupt
 *
 * Blocks until perfmon buffer half full interrupt occurs or the wait
 * times out.
 */
static int intel_wait_perfmon_interrupt(struct drm_device *dev,
						int timeout_ms)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	int counter = atomic_read(&dev_priv->perfmon.buffer_interrupts);
	int ret = I915_PERFMON_IRQ_WAIT_OK;
	int time_left = 0;

	if (!(IS_GEN7(dev)) && !(IS_GEN8(dev)))
		return -EINVAL;

	time_left = wait_event_interruptible_timeout(
		dev_priv->perfmon.buffer_queue,
		atomic_read(&dev_priv->perfmon.buffer_interrupts) != counter,
		timeout_ms * HZ / 1000);

	if (time_left == 0)
		ret = I915_PERFMON_IRQ_WAIT_TIMEOUT;
	else if (time_left == -ERESTARTSYS)
		ret = I915_PERFMON_IRQ_WAIT_INTERRUPTED;
	else if (time_left < 0)
		ret = I915_PERFMON_IRQ_WAIT_FAILED;

	return ret;
}

/**
* intel_cancel_wait_perfmon_interrupt - wake up all threads waiting for
* perfmon buffer interrupt.
*
* All threads waiting for for perfmon buffer interrupt are
* woken up.
*/
static int intel_cancel_wait_perfmon_interrupt(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	if (!(IS_GEN7(dev)) && !(IS_GEN8(dev)))
		return -EINVAL;

	atomic_inc(&dev_priv->perfmon.buffer_interrupts);
	wake_up_all(&dev_priv->perfmon.buffer_queue);

	return 0;
}

/**
 * i915_get_render_hw_ctx_id
 *
 * Get render engine HW context ID for given context. This is the
 * representation of context in the HW. This is *not* context ID as referenced
 * by usermode. For legacy submission path this is logical ring context address.
 * For execlist this is the kernel managed context ID written to execlist
 * descriptor.
 */
static int  i915_get_render_hw_ctx_id(
	struct drm_i915_private *dev_priv,
	struct intel_context *ctx,
	__u32 *id)
{
	struct drm_i915_gem_object *ctx_obj =
		ctx->engine[RCS].state;

	if (!ctx_obj)
		return -ENOENT;

	*id = i915.enable_execlists ?
			intel_execlists_ctx_id(ctx_obj) :
			i915_gem_obj_ggtt_offset(ctx_obj) >> 12;

	return 0;
}

/**
 * i915_perfmon_get_hw_ctx_id
 *
 * Get HW context ID for given context ID and DRM file.
 */
static int i915_perfmon_get_hw_ctx_id(
	struct drm_device *dev,
	struct drm_file *file,
	struct drm_i915_perfmon_get_hw_ctx_id *ioctl_data)
{
	struct drm_i915_private *dev_priv =
		(struct drm_i915_private *) dev->dev_private;
	struct intel_context *ctx;
	struct drm_i915_file_private *file_priv = file->driver_priv;
	int ret;

	if (!HAS_HW_CONTEXTS(dev))
		return -ENODEV;

	ret = i915_mutex_lock_interruptible(dev);
	if (ret)
		return ret;

	ctx = i915_gem_context_get(file_priv, ioctl_data->ctx_id);
	if (IS_ERR_OR_NULL(ctx))
		ret = -ENOENT;
	else
		ret = i915_get_render_hw_ctx_id(dev_priv, ctx,
			&ioctl_data->hw_ctx_id);

	mutex_unlock(&dev->struct_mutex);
	return ret;
}

struct i915_perfmon_hw_ctx_list {
	__u32 *ids;
	__u32 capacity;
	__u32 size;
	__u32 iterations_left;
};

/**
 * process_context
 *
 * Check if context referenced by 'ptr' belongs to application with
 * provided process ID. If so, increment total number of contexts
 * found (list->size) and add context id to the list if
 * its capacity is not reached.
 */
static int process_context(struct drm_i915_private *dev_priv,
	struct intel_context *ctx,
	__u32 pid,
	struct i915_perfmon_hw_ctx_list *list)
{
	bool ctx_match;
	bool has_render_ring;
	__u32 id;

	if (list->iterations_left == 0)
		return 0;
	--list->iterations_left;

	ctx_match = (pid == pid_vnr(ctx->pid) ||
			 pid == 0 ||
			 ctx == dev_priv->ring[RCS].default_context);

	if (ctx_match) {
		has_render_ring =
			(0 == i915_get_render_hw_ctx_id(
				dev_priv, ctx, &id));
	}

	if (ctx_match && has_render_ring) {
		if (list->size < list->capacity)
			list->ids[list->size] = id;
		list->size++;
	}

	return 0;
}

/**
 * i915_perfmon_get_hw_ctx_ids
 *
 * Lookup the list of all contexts and return HW context IDs of those
 * belonging to provided process id.
 *
 * User specifies maximum number of IDs to be written to provided block of
 * memory: ioctl_data->count. Returned is the list of not more than
 * ioctl_data->count HW context IDs together with total number of matching
 * contexts found - potentially more than ioctl_data->count.
 *
 */
static int i915_perfmon_get_hw_ctx_ids(
	struct drm_device *dev,
	struct drm_i915_perfmon_get_hw_ctx_ids *ioctl_data)
{
	struct drm_i915_private *dev_priv =
		(struct drm_i915_private *) dev->dev_private;
	struct i915_perfmon_hw_ctx_list list;
	struct intel_context *ctx;
	unsigned int ids_to_copy;
	int ret;

	if (!HAS_HW_CONTEXTS(dev))
		return -ENODEV;

	if (ioctl_data->count > I915_PERFMON_MAX_HW_CTX_IDS)
		return -EINVAL;

	list.ids = kzalloc(
		ioctl_data->count * sizeof(__u32), GFP_KERNEL);
	if (!list.ids)
		return -ENOMEM;
	list.capacity = ioctl_data->count;
	list.size = 0;
	list.iterations_left = I915_PERFMON_MAX_HW_CTX_IDS;

	ret = i915_mutex_lock_interruptible(dev);
	if (ret)
		goto exit;

	list_for_each_entry(ctx, &dev_priv->context_list, link) {
		process_context(dev_priv, ctx, ioctl_data->pid, &list);
	}

	mutex_unlock(&dev->struct_mutex);

	/*
	 * After we searched all the contexts list.size is the total number
	 * of contexts matching the query. This is potentially more than
	 * the capacity of user buffer (list.capacity).
	 */
	ids_to_copy = min(list.size, list.capacity);
	if (copy_to_user(
		(__u32 __user *)(uintptr_t)ioctl_data->ids,
		list.ids,
		ids_to_copy * sizeof(__u32))) {
		ret = -EFAULT;
		goto exit;
	}

	/* Return total number of matching ids to the user. */
	ioctl_data->count = list.size;
exit:
	kfree(list.ids);
	return ret;
}

/**
 * i915_perfmon_open
 *
 * open perfmon for current file
 */
static int i915_perfmon_open(
	struct drm_file *file)
{
	struct drm_i915_file_private *file_priv = file->driver_priv;
	int ret = 0;

	if (!capable(CAP_SYS_ADMIN))
		ret = -EACCES;
	else
		file_priv->perfmon.opened = true;

	return ret;
}

/**
 * i915_perfmon_close
 *
 * close perfmon for current file
 */
static int i915_perfmon_close(
	struct drm_file *file)
{
	struct drm_i915_file_private *file_priv = file->driver_priv;

	file_priv->perfmon.opened = false;

	return 0;
}

/**
 * i915_perfmon_ioctl - performance monitoring support
 *
 * Main entry point to performance monitoring support
 * IOCTLs.
 */
int i915_perfmon_ioctl(struct drm_device *dev, void *data,
	struct drm_file *file)
{
	struct drm_i915_perfmon *perfmon = data;
	struct drm_i915_file_private *file_priv = file->driver_priv;
	int ret = 0;

	switch (perfmon->op) {
	case I915_PERFMON_SET_BUFFER_IRQS:
		ret = intel_enable_perfmon_interrupt(
				dev,
				perfmon->data.set_irqs.enable);
		break;

	case I915_PERFMON_WAIT_BUFFER_IRQS:
		if (perfmon->data.wait_irqs.timeout >
				I915_PERFMON_WAIT_IRQ_MAX_TIMEOUT_MS)
			ret =  -EINVAL;
		else
			perfmon->data.wait_irqs.ret_code =
				intel_wait_perfmon_interrupt(
					dev,
					perfmon->data.wait_irqs.timeout);
		break;

	case I915_PERFMON_CANCEL_WAIT_BUFFER_IRQS:
		ret = intel_cancel_wait_perfmon_interrupt(dev);
		break;

	case I915_PERFMON_OPEN:
		ret = i915_perfmon_open(file);
		break;

	case I915_PERFMON_CLOSE:
		ret = i915_perfmon_close(file);
		break;

	case I915_PERFMON_ENABLE_CONFIG:
		ret = -ENODEV;
		break;

	case I915_PERFMON_DISABLE_CONFIG:
		ret = -ENODEV;
		break;

	case I915_PERFMON_SET_CONFIG:
		ret = -ENODEV;
		break;

	case I915_PERFMON_LOAD_CONFIG:
		ret = -ENODEV;
		break;

	case I915_PERFMON_GET_HW_CTX_ID:
		ret = i915_perfmon_get_hw_ctx_id(
			dev,
			file,
			&perfmon->data.get_hw_ctx_id);
		break;

	case I915_PERFMON_GET_HW_CTX_IDS:
		if (!file_priv->perfmon.opened) {
			ret = -EACCES;
			break;
		}
		ret = i915_perfmon_get_hw_ctx_ids(
			dev,
			&perfmon->data.get_hw_ctx_ids);
		break;

	default:
		DRM_DEBUG("UNKNOWN OP\n");
		/* unknown operation */
		ret = -EINVAL;
		break;
	}

	return ret;
}

void i915_perfmon_setup(struct drm_i915_private *dev_priv)
{
	atomic_set(&dev_priv->perfmon.buffer_interrupts, 0);
	init_waitqueue_head(&dev_priv->perfmon.buffer_queue);
}

void i915_perfmon_cleanup(struct drm_i915_private *dev_priv)
{
}

