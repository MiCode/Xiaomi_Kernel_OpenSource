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
 */

#ifndef _I915_PERFMON_H_
#define _I915_PERFMON_H_

#include "drm.h"

#define I915_PERFMON_IOCTL_VERSION      5

#define I915_PERFMON_WAIT_IRQ_MAX_TIMEOUT_MS 10000

struct drm_i915_perfmon_set_buffer_irqs {
	__u32 enable;
};

enum I915_PERFMON_WAIT_IRQ_RET_CODE {
	I915_PERFMON_IRQ_WAIT_OK,
	I915_PERFMON_IRQ_WAIT_FAILED,
	I915_PERFMON_IRQ_WAIT_TIMEOUT,
	I915_PERFMON_IRQ_WAIT_INTERRUPTED,
};

struct drm_i915_perfmon_wait_irqs {
	__u32 timeout;		/* in ms */
	__u32 ret_code;
};

struct drm_i915_perfmon_config_entry {
	__u32 offset;
	__u32 value;
};

static const unsigned int I915_PERFMON_CONFIG_SIZE = 256;

/* Explicitly aligned to 8 bytes to avoid mismatch
   between 64-bit KM and 32-bit UM. */
typedef __u64 drm_i915_perfmon_shared_ptr __aligned(8);

struct drm_i915_perfmon_user_config {
	/* This is pointer to struct drm_i915_perfmon_config_entry.*/
	drm_i915_perfmon_shared_ptr entries;
	__u32 size;
	__u32 id;
};

enum DRM_I915_PERFMON_CONFIG_TARGET {
	I915_PERFMON_CONFIG_TARGET_CTX,
	I915_PERFMON_CONFIG_TARGET_PID,
	I915_PERFMON_CONFIG_TARGET_ALL,
};

struct drm_i915_perfmon_set_config {
	enum DRM_I915_PERFMON_CONFIG_TARGET target;
	struct drm_i915_perfmon_user_config oa;
	struct drm_i915_perfmon_user_config gp;
	__u32 pid;
};

struct drm_i915_perfmon_load_config {
	__u32 ctx_id;
	__u32 oa_id;
	__u32 gp_id;
};


static const unsigned int I915_PERFMON_MAX_HW_CTX_IDS = 1024;

struct drm_i915_perfmon_get_hw_ctx_ids {
	__u32 pid;
	__u32 count;
	 /* This is pointer to __u32. */
	drm_i915_perfmon_shared_ptr ids;
};


struct drm_i915_perfmon_get_hw_ctx_id {
	__u32 ctx_id;
	__u32 hw_ctx_id;
};

enum I915_PERFMON_IOCTL_OP {
	I915_PERFMON_SET_BUFFER_IRQS = 5,
	I915_PERFMON_WAIT_BUFFER_IRQS,
	I915_PERFMON_CANCEL_WAIT_BUFFER_IRQS,
	I915_PERFMON_OPEN,
	I915_PERFMON_CLOSE,
	I915_PERFMON_ENABLE_CONFIG,
	I915_PERFMON_DISABLE_CONFIG,
	I915_PERFMON_SET_CONFIG,
	I915_PERFMON_LOAD_CONFIG,
	I915_PERFMON_GET_HW_CTX_ID,
	I915_PERFMON_GET_HW_CTX_IDS,
};

struct drm_i915_perfmon {
	enum I915_PERFMON_IOCTL_OP op;
	union {
		struct drm_i915_perfmon_wait_irqs	wait_irqs;
		struct drm_i915_perfmon_set_buffer_irqs	set_irqs;
		struct drm_i915_perfmon_set_config	set_config;
		struct drm_i915_perfmon_load_config	load_config;
		struct drm_i915_perfmon_get_hw_ctx_id	get_hw_ctx_id;
		struct drm_i915_perfmon_get_hw_ctx_ids	get_hw_ctx_ids;
	} data;
};

#endif	/* _I915_PERFMON_H_ */
