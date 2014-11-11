/*
 * Copyright 2014 Intel Corporation
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
 * VA LINUX SYSTEMS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Author:
 *		Deepak S <deepak.s@intel.com>
 */

#ifndef I915_DPST_H
#define I915_DPST_H

/* Total number of DIET entries */
#define	DPST_DIET_ENTRY_COUNT	33

/* Value to reset image enhancement interrupt register */
#define DPST_RESET_IE		0x40004000

/* No dpst adjustment for backlight, i.e, 100% of the user specified
   backlight will be applied (dpst will not reduce the backlight). */
#define DPST_MAX_FACTOR		10000

/* Threshold that will generate interrupts when crossed */
#define DEFAULT_GUARDBAND_VAL 30

struct dpst_ie {
	enum dpst_diet_alg {
		i915_DPST_RGB_TRANSLATOR = 0,
		i915_DPST_YUV_ADDER,
		i915_DPST_HSV_MULTIPLIER
	} diet_algorithm;
	__u32  base_lut_index;	/* Base lut index (192 for legacy mode)*/
	__u32  factor_present[DPST_DIET_ENTRY_COUNT];
	__u32  factor_new[DPST_DIET_ENTRY_COUNT];
	__u32  factor_scalar;
};

struct dpst_ie_container {
	struct dpst_ie dpst_ie_st;
	__u32	dpst_blc_factor;
	__u32	pipe_n;
};

struct dpst_initialize_data {
	__u32 pipe_n;
	__u32 threshold_gb;
	__u32 gb_delay;
	__u32 hist_reg_values;
	__u32 image_res;
	__u32 sig_num;
};

struct dpst_histogram {
	__u16	event;
	__u32	status[32];
	__u32	threshold[12];
	__u32	gb_val;
	__u32	gb_int_delay;
	__u32   bkl_val;
	enum dpst_hist_mode {
		i915_DPST_YUV_LUMA_MODE = 0,
		i915_DPST_HSV_INTENSITY_MODE
	} hist_mode;
};

struct dpst_histogram_status_legacy {
	__u32	pipe_n;
	struct	dpst_histogram histogram_bins;
};

struct dpst_histogram_status {
	__u32	pipe_n;
	__u32	dpst_disable;
	struct	dpst_histogram histogram_bins;
};

struct dpst_initialize_context {
	enum dpst_call_type {
		DPST_ENABLE = 1,
		DPST_DISABLE,
		DPST_INIT_DATA,
		DPST_GET_BIN_DATA_LEGACY,
		DPST_APPLY_LUMA,
		DPST_RESET_HISTOGRAM_STATUS,
		DPST_GET_BIN_DATA
	} dpst_ioctl_type;
	union {
		struct dpst_initialize_data		init_data;
		struct dpst_ie_container		ie_container;
		struct dpst_histogram_status		hist_status;
		struct dpst_histogram_status_legacy	hist_status_legacy;
	};
};

#endif /* _UAPI_I915_DPST_H_ */
