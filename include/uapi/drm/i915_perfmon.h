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

#define I915_PERFMON_IOCTL_VERSION           2

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

enum I915_PERFMON_IOCTL_OP {
	I915_PERFMON_SET_BUFFER_IRQS = 5,
	I915_PERFMON_WAIT_BUFFER_IRQS,
};

struct drm_i915_perfmon {
	enum I915_PERFMON_IOCTL_OP op;
	union {
		struct drm_i915_perfmon_wait_irqs	wait_irqs;
		struct drm_i915_perfmon_set_buffer_irqs set_irqs;
		__u32 reserved[64];
	} data;
};

#endif	/* _I915_PERFMON_H_ */
