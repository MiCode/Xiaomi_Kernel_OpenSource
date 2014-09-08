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
 * i915_perfmon_ioctl - performance monitoring support
 *
 * Main entry point to performance monitoring support
 * IOCTLs.
 */
int i915_perfmon_ioctl(struct drm_device *dev, void *data,
	struct drm_file *file)
{
	struct drm_i915_perfmon *perfmon = data;
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
		ret = -ENODEV;
		break;

	case I915_PERFMON_CLOSE:
		ret = -ENODEV;
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
		ret = -ENODEV;
		break;

	case I915_PERFMON_GET_HW_CTX_IDS:
		ret = -ENODEV;
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

