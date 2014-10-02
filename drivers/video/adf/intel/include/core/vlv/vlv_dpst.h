/*
 * Copyright (C) 2014, Intel Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Author:
 *	Deepak S <deepak.s@intel.com>
 */

#ifndef VLV_DPST_H
#define VLV_DPST_H

#define VLV_DISPLAY_BASE 0x180000

#define _PIPE(pipe, a, b) ((a) + (pipe)*((b)-(a)))

/* DPST related registers */
#define BLM_HIST_CTL	0x48260
#define IE_HISTOGRAM_ENABLE	(1<<31)
#define IE_MOD_TABLE_ENABLE	(1<<27)
#define HSV_INTENSITY_MODE	(1<<24)
#define ENHANCEMENT_MODE_MULT	(2<<13)
#define BIN_REG_FUNCTION_SELECT_IE	(1<<11)
#define BIN_REGISTER_INDEX_MASK	0x7F421
#define BLM_HIST_BIN	0x48264
#define BUSY_BIT	(1<<31)
#define BIN_COUNT_MASK_4M	0x3FFFFF
#define BIN_COUNT_MASK_16M	0xFFFFFF
#define BLM_HIST_GUARD	0x48268
#define HISTOGRAM_INTERRUPT_ENABLE	(1<<31)
#define HISTOGRAM_EVENT_STATUS	(1<<30)
#define HIST_BIN_COUNT	32

#define _VLV_BLC_HIST_CTL_A (VLV_DISPLAY_BASE + 0x61260)
#define _VLV_BLC_HIST_CTL_B (VLV_DISPLAY_BASE + 0x61360)
#define VLV_BLC_HIST_CTL(pipe) _PIPE(pipe, _VLV_BLC_HIST_CTL_A, \
				_VLV_BLC_HIST_CTL_B)

#define _VLV_BLC_HIST_BIN_A (VLV_DISPLAY_BASE + 0x61264)
#define _VLV_BLC_HIST_BIN_B (VLV_DISPLAY_BASE + 0x61364)
#define VLV_BLC_HIST_BIN(pipe) _PIPE(pipe, _VLV_BLC_HIST_BIN_A, \
				_VLV_BLC_HIST_BIN_B)

#define _VLV_BLC_HIST_GUARD_A	(VLV_DISPLAY_BASE + 0x61268)
#define _VLV_BLC_HIST_GUARD_B (VLV_DISPLAY_BASE + 0x61368)
#define VLV_BLC_HIST_GUARD(pipe) _PIPE(pipe, _VLV_BLC_HIST_GUARD_A, \
					_VLV_BLC_HIST_GUARD_B)

extern struct vlv_dc_config *config;

/* DPST information */
struct vlv_dpst_registers {
	uint32_t blm_hist_guard;
	uint32_t blm_hist_ctl;
	uint32_t blm_hist_bin;
	uint32_t blm_hist_bin_count_mask;
};

struct vlv_dpst {
	struct pid *pid;
	u32 pipe;
	u32 signal;
	u32 blc_adjustment;
	u32 gb_delay;
	u32 init_image_res;
	bool user_enable; /*user client wishes to enable */
	bool kernel_disable; /* kernel override wishes to disable */
	bool enabled; /* actual functional state */
	/* Indicates pipe mismatch between user mode and kernel */
	bool pipe_mismatch;
	/* Indicates that Display is off (could be power gated also) */
	bool display_off;
	struct {
		bool is_valid;
		u32 blc_adjustment;
	} saved;

	struct mutex ioctl_lock;
	struct vlv_dpst_registers reg;
};

extern void vlv_wait_for_vblank(int pipe);
void vlv_dpst_init(struct intel_dc_config *config);
void vlv_dpst_teardown(void);
void vlv_dpst_irq_handler(struct intel_pipe *pipe);
void vlv_dpst_display_on(void);
void vlv_dpst_display_off(void);
void vlv_dpst_set_brightness(struct intel_pipe *pipe, u32 brightness_val);
#endif


