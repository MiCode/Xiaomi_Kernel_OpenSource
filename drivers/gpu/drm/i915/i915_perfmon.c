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
 * copy_entries
 *
 * Helper function to copy OA configuration entries to new destination.
 *
 * Source configuration is first validated. In case of success pointer to newly
 * allocated memory containing copy of source configuration is returned in *out.
 *
 */
static int copy_entries(
	struct drm_i915_perfmon_config *source,
	bool user,
	void **out)
{
	size_t size = 0;

	*out = NULL;

	/* basic validation of input */
	if (source->id == 0 || source->size == 0 || source->entries == NULL)
		return 0;

	if (source->size > I915_PERFMON_CONFIG_SIZE)
		return -EINVAL;

	size = source->size  * sizeof(struct drm_i915_perfmon_config_entry);

	*out = kzalloc(
		   size,
		   GFP_KERNEL);
	if (*out == NULL) {
		DRM_ERROR("failed to allocate configuration buffer\n");
		return -ENOMEM;
	}

	if (user) {
		int ret = copy_from_user(*out, source->entries, size);
		if (ret) {
			DRM_ERROR("failed to copy user provided config: %x\n",
					ret);
			kfree(*out);
			*out = NULL;
			return -EFAULT;
		}
	} else
		memcpy(*out, source->entries, size);

	return 0;
}

/**
 * i915_perfmon_copy_config
 *
 * Utility function to copy OA and GP configuration to its destination.
 *
 * This is first used when global configuration is set by the user by calling
 * I915_PERFMON_SET_CONFIG and then for the second time (optionally) when user
 * calls I915_PERFMON_LOAD_CONFIG to copy the configuration from global storage
 * to his context.
 *
 * 'user' boolean value indicates whether pointer to source config is provided
 * by usermode (I915_PERFMON_SET_CONFIG case).
 *
 * If both OA and GP config are provided (!= NULL) then either both are copied
 * to their respective locations or none of them (which is indicated by return
 * value != 0).
 *
 * target_oa and target_gp are assumed to be non-NULL.
 *
 */
static int i915_perfmon_copy_config(
	struct drm_i915_private *dev_priv,
	struct drm_i915_perfmon_config *target_oa,
	struct drm_i915_perfmon_config *target_gp,
	struct drm_i915_perfmon_config source_oa,
	struct drm_i915_perfmon_config source_gp,
	bool user)
{
	void *temp_oa = NULL;
	void *temp_gp = NULL;
	int ret = 0;

	BUG_ON(!mutex_is_locked(&dev_priv->perfmon.config.lock));

	/* copy configurations to temporary storage */
	ret = copy_entries(&source_oa, user, &temp_oa);
	if (ret)
		return ret;
	ret = copy_entries(&source_gp, user, &temp_gp);
	if (ret) {
		kfree(temp_oa);
		return ret;
	}

	/*
	 * Allocation and copy successful, free old config memory and swap
	 * pointers
	 */
	if (temp_oa) {
		kfree(target_oa->entries);
		target_oa->entries = temp_oa;
		target_oa->id = source_oa.id;
		target_oa->size = source_oa.size;
	}
	if (temp_gp) {
		kfree(target_gp->entries);
		target_gp->entries = temp_gp;
		target_gp->id = source_gp.id;
		target_gp->size = source_gp.size;
	}

	return 0;
}

/**
 * i915_perfmon_set_config
 *
 * Store OA/GP configuration for later use.
 *
 * Configuration content is not validated since it is provided by user who had
 * previously called Perfmon Open with sysadmin privilege level.
 *
 */
static int i915_perfmon_set_config(
	struct drm_device *dev,
	struct drm_i915_perfmon_set_config *args)
{
	struct drm_i915_private *dev_priv =
		(struct drm_i915_private *) dev->dev_private;
	int ret = 0;
	struct drm_i915_perfmon_config user_config_oa;
	struct drm_i915_perfmon_config user_config_gp;

	if (!(IS_GEN8(dev)))
		return -EINVAL;

	/* validate target */
	switch (args->target) {
	case I915_PERFMON_CONFIG_TARGET_CTX:
	case I915_PERFMON_CONFIG_TARGET_PID:
	case I915_PERFMON_CONFIG_TARGET_ALL:
		/* OK */
		break;
	default:
		DRM_DEBUG("invalid target\n");
		return -EINVAL;
	}

	/* setup input for i915_perfmon_copy_config */
	user_config_oa.id = args->oa.id;
	user_config_oa.size = args->oa.size;
	user_config_oa.entries =
		(struct drm_i915_perfmon_config_entry __user *)
			(uintptr_t)args->oa.entries;

	user_config_gp.id = args->gp.id;
	user_config_gp.size = args->gp.size;
	user_config_gp.entries =
		(struct drm_i915_perfmon_config_entry __user *)
			(uintptr_t)args->gp.entries;

	ret = mutex_lock_interruptible(&dev_priv->perfmon.config.lock);
	if (ret)
		return ret;

	if (!atomic_read(&dev_priv->perfmon.config.enable)) {
		ret = -EINVAL;
		goto unlock_perfmon;
	}

	ret = i915_perfmon_copy_config(dev_priv,
			&dev_priv->perfmon.config.oa,
			&dev_priv->perfmon.config.gp,
			user_config_oa, user_config_gp,
			true);

	if (ret)
		goto unlock_perfmon;

	dev_priv->perfmon.config.target = args->target;
	dev_priv->perfmon.config.pid = args->pid;

unlock_perfmon:
	mutex_unlock(&dev_priv->perfmon.config.lock);
	return ret;
}

/**
 * i915_perfmon_load_config
 *
 * Copy configuration from global storage to current context.
 *
 */
static int i915_perfmon_load_config(
	struct drm_device *dev,
	struct drm_file *file,
	struct drm_i915_perfmon_load_config *args)
{
	struct drm_i915_private *dev_priv =
		(struct drm_i915_private *) dev->dev_private;
	struct drm_i915_file_private *file_priv = file->driver_priv;
	struct intel_context *ctx;
	struct drm_i915_perfmon_config user_config_oa;
	struct drm_i915_perfmon_config user_config_gp;
	int ret;

	if (!(IS_GEN8(dev)))
		return -EINVAL;

	ret = i915_mutex_lock_interruptible(dev);
	if (ret)
		return ret;

	if (!atomic_read(&dev_priv->perfmon.config.enable)) {
		ret = -EINVAL;
		goto unlock_dev;
	}

	ctx = i915_gem_context_get(
				file_priv,
				args->ctx_id);

	if (IS_ERR_OR_NULL(ctx)) {
		DRM_DEBUG("invalid context\n");
		ret = -EINVAL;
		goto unlock_dev;
	}

	ret = mutex_lock_interruptible(&dev_priv->perfmon.config.lock);
	if (ret)
		goto unlock_dev;

	user_config_oa = dev_priv->perfmon.config.oa;
	user_config_gp = dev_priv->perfmon.config.gp;

	/*
	 * copy configuration to the context only if requested config ID matches
	 * device configuration ID
	 */
	if (!(args->oa_id != 0 &&
	      args->oa_id == dev_priv->perfmon.config.oa.id))
		user_config_oa.entries = NULL;
	if (!(args->gp_id != 0 &&
	     args->gp_id == dev_priv->perfmon.config.gp.id))
		user_config_gp.entries = NULL;

	ret = i915_perfmon_copy_config(dev_priv,
			&ctx->perfmon.config.oa.pending,
			&ctx->perfmon.config.gp.pending,
			dev_priv->perfmon.config.oa,
			dev_priv->perfmon.config.gp,
			false);

	if (ret)
		goto unlock_perfmon;

	/*
	 * return info about what is actualy set for submission in
	 * target context
	 */
	args->gp_id = ctx->perfmon.config.gp.pending.id;
	args->oa_id = ctx->perfmon.config.oa.pending.id;

unlock_perfmon:
	mutex_unlock(&dev_priv->perfmon.config.lock);
unlock_dev:
	mutex_unlock(&dev->struct_mutex);

	return ret;
}

static void *emit_dword(void *mem, __u32 cmd)
{
	iowrite32(cmd, mem);
	return ((__u32 *)mem) + 1;
}

static void *emit_load_register_imm(void *mem, __u32 reg, __u32 val)
{
	mem = emit_dword(mem, MI_NOOP);
	mem = emit_dword(mem, MI_LOAD_REGISTER_IMM(1));
	mem = emit_dword(mem, reg);
	mem = emit_dword(mem, val);
	return mem;
}

static void *emit_cs_stall_pipe_control(void *mem)
{
	mem = emit_dword(mem, GFX_OP_PIPE_CONTROL(6));
	mem = emit_dword(mem, PIPE_CONTROL_CS_STALL|PIPE_CONTROL_WRITE_FLUSH|
			      PIPE_CONTROL_GLOBAL_GTT_IVB);
	mem = emit_dword(mem, 0);
	mem = emit_dword(mem, 0);
	mem = emit_dword(mem, 0);
	mem = emit_dword(mem, 0);
	return mem;
}

int i915_perfmon_update_workaround_bb(struct drm_i915_private *dev_priv,
				      struct drm_i915_perfmon_config *config)
{
	const size_t commands_size = 6 + /* pipe control */
				     config->size * 4 + /* NOOP + LRI */
				     6 + /* pipe control */
				     1;  /* BB end */
	void *buffer_tail;
	unsigned int i = 0;
	int ret = 0;

	if (commands_size > PAGE_SIZE) {
		DRM_ERROR("OA cfg too long to fit into workarond BB\n");
		return -ENOSPC;
	}

	BUG_ON(!mutex_is_locked(&dev_priv->perfmon.config.lock));

	if (atomic_read(&dev_priv->perfmon.config.enable) == 0 ||
	    !dev_priv->rc6_wa_bb.obj) {
		DRM_ERROR("not ready to write WA BB commands\n");
		return -EINVAL;
	}

	ret = mutex_lock_interruptible(&dev_priv->rc6_wa_bb.lock);
	if (ret)
		return ret;

	if (!dev_priv->rc6_wa_bb.obj) {
		mutex_unlock(&dev_priv->rc6_wa_bb.lock);
		return 0;
	}

	/* disable RC6 WA BB */
	I915_WRITE(GEN8_RC6_WA_BB, 0x0);

	buffer_tail = dev_priv->rc6_wa_bb.address;
	buffer_tail = emit_cs_stall_pipe_control(buffer_tail);

	/* OA/NOA config */
	for (i = 0; i < config->size; i++)
		buffer_tail = emit_load_register_imm(
			buffer_tail,
			config->entries[i].offset,
			config->entries[i].value);

	buffer_tail = emit_cs_stall_pipe_control(buffer_tail);

	/* BB END */
	buffer_tail = emit_dword(buffer_tail, MI_BATCH_BUFFER_END);

	/* enable WA BB */
	I915_WRITE(GEN8_RC6_WA_BB, dev_priv->rc6_wa_bb.offset | 0x1);

	mutex_unlock(&dev_priv->rc6_wa_bb.lock);
	return 0;
}

static int allocate_wa_bb(struct drm_i915_private *dev_priv)
{
	int ret = 0;

	BUG_ON(!mutex_is_locked(&dev_priv->dev->struct_mutex));

	ret = mutex_lock_interruptible(&dev_priv->rc6_wa_bb.lock);
	if (ret)
		return ret;

	if (atomic_inc_return(&dev_priv->rc6_wa_bb.enable) > 1) {
		mutex_unlock(&dev_priv->rc6_wa_bb.lock);
		return 0;
	}

	BUG_ON(dev_priv->rc6_wa_bb.obj != NULL);

	dev_priv->rc6_wa_bb.obj = i915_gem_alloc_object(
						dev_priv->dev,
						PAGE_SIZE);
	if (!dev_priv->rc6_wa_bb.obj) {
		ret = -ENOMEM;
		goto unlock;
	}

	ret = i915_gem_obj_ggtt_pin(
			dev_priv->rc6_wa_bb.obj,
			PAGE_SIZE, PIN_MAPPABLE);

	if (ret) {
		drm_gem_object_unreference_unlocked(
			&dev_priv->rc6_wa_bb.obj->base);
		goto unlock;
	}

	ret = i915_gem_object_set_to_gtt_domain(dev_priv->rc6_wa_bb.obj,
						true);
	if (ret) {
		i915_gem_object_ggtt_unpin(dev_priv->rc6_wa_bb.obj);
		drm_gem_object_unreference_unlocked(
			&dev_priv->rc6_wa_bb.obj->base);
		goto unlock;
	}

	dev_priv->rc6_wa_bb.offset = i915_gem_obj_ggtt_offset(
						dev_priv->rc6_wa_bb.obj);

	dev_priv->rc6_wa_bb.address = ioremap_wc(
		dev_priv->gtt.mappable_base + dev_priv->rc6_wa_bb.offset,
		PAGE_SIZE);

	if (!dev_priv->rc6_wa_bb.address) {
		i915_gem_object_ggtt_unpin(dev_priv->rc6_wa_bb.obj);
		drm_gem_object_unreference_unlocked(
			&dev_priv->rc6_wa_bb.obj->base);
		ret =  -ENOMEM;
		goto unlock;
	}

	DRM_DEBUG("RC6 WA BB, offset %lx address %p, GGTT mapping: %s\n",
		  dev_priv->rc6_wa_bb.offset,
		  dev_priv->rc6_wa_bb.address,
		  dev_priv->rc6_wa_bb.obj->has_global_gtt_mapping ?
			"yes" : "no");

	memset(dev_priv->rc6_wa_bb.address, 0, PAGE_SIZE);

unlock:
	if (ret) {
		dev_priv->rc6_wa_bb.obj = NULL;
		dev_priv->rc6_wa_bb.offset = 0;
	}
	mutex_unlock(&dev_priv->rc6_wa_bb.lock);
	return ret;
}

static void deallocate_wa_bb(struct drm_i915_private *dev_priv)
{
	BUG_ON(!mutex_is_locked(&dev_priv->dev->struct_mutex));

	mutex_lock(&dev_priv->rc6_wa_bb.lock);

	if (atomic_read(&dev_priv->rc6_wa_bb.enable) == 0)
		goto unlock;

	if (atomic_dec_return(&dev_priv->rc6_wa_bb.enable) > 1)
		goto unlock;

	I915_WRITE(GEN8_RC6_WA_BB, 0);

	if (dev_priv->rc6_wa_bb.obj != NULL) {
		iounmap(dev_priv->rc6_wa_bb.address);
		i915_gem_object_ggtt_unpin(dev_priv->rc6_wa_bb.obj);
		drm_gem_object_unreference(&dev_priv->rc6_wa_bb.obj->base);
		dev_priv->rc6_wa_bb.obj = NULL;
		dev_priv->rc6_wa_bb.offset = 0;
	}
unlock:
	mutex_unlock(&dev_priv->rc6_wa_bb.lock);
}

/**
* i915_perfmon_config_enable_disable
*
* Enable/disable OA/GP configuration transport.
*/
static int i915_perfmon_config_enable_disable(
	struct drm_device *dev,
	int enable)
{
	int ret;
	struct drm_i915_private *dev_priv =
		(struct drm_i915_private *) dev->dev_private;

	if (!(IS_GEN8(dev)))
		return -EINVAL;

	ret = i915_mutex_lock_interruptible(dev_priv->dev);
	if (ret)
		return ret;

	ret = mutex_lock_interruptible(&dev_priv->perfmon.config.lock);
	if (ret) {
		mutex_unlock(&dev->struct_mutex);
		return ret;
	}

	if (enable) {
		ret = allocate_wa_bb(dev_priv);
		if (!ret &&
		    atomic_inc_return(&dev_priv->perfmon.config.enable) == 1) {
			dev_priv->perfmon.config.target =
				I915_PERFMON_CONFIG_TARGET_ALL;
			dev_priv->perfmon.config.oa.id = 0;
			dev_priv->perfmon.config.gp.id = 0;
		}
	} else if (atomic_read(&dev_priv->perfmon.config.enable)) {
		atomic_dec(&dev_priv->perfmon.config.enable);
		deallocate_wa_bb(dev_priv);
	}

	mutex_unlock(&dev_priv->perfmon.config.lock);
	mutex_unlock(&dev->struct_mutex);

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
		ret = i915_perfmon_config_enable_disable(dev, 1);
		break;

	case I915_PERFMON_DISABLE_CONFIG:
		ret = i915_perfmon_config_enable_disable(dev, 0);
		break;

	case I915_PERFMON_SET_CONFIG:
		if (!file_priv->perfmon.opened) {
			ret = -EACCES;
			break;
		}
		ret = i915_perfmon_set_config(
			dev,
			&perfmon->data.set_config);
		break;

	case I915_PERFMON_LOAD_CONFIG:
		ret = i915_perfmon_load_config(
			dev,
			file,
			&perfmon->data.load_config);
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
	atomic_set(&dev_priv->perfmon.config.enable, 0);
	dev_priv->perfmon.config.oa.entries = NULL;
	dev_priv->perfmon.config.gp.entries = NULL;
}

void i915_perfmon_cleanup(struct drm_i915_private *dev_priv)
{
	kfree(dev_priv->perfmon.config.oa.entries);
	kfree(dev_priv->perfmon.config.gp.entries);
}

void i915_perfmon_ctx_setup(struct intel_context *ctx)
{
	ctx->perfmon.config.oa.pending.entries = NULL;
	ctx->perfmon.config.gp.pending.entries = NULL;
}

void i915_perfmon_ctx_cleanup(struct intel_context *ctx)
{
	kfree(ctx->perfmon.config.oa.pending.entries);
	kfree(ctx->perfmon.config.gp.pending.entries);
}
