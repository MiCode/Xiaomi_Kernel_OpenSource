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
 */

#ifndef _CHV_DPIO_H_
#define _CHV_DPIO_H_

#include <core/vlv/vlv_dc_config.h>
#include <core/vlv/vlv_dc_regs.h>

void chv_dpio_update_clock(struct intel_pipeline *pipeline,
		struct intel_clock *clock);
void chv_dpio_update_channel(struct intel_pipeline *pipeline);
void chv_dpio_set_signal_levels(struct intel_pipeline *pipeline,
	u32 deemph_reg_value, u32 margin_reg_value);
void chv_dpio_lane_reset_en(struct intel_pipeline *pipeline, bool enable);
void chv_dpio_post_pll_disable(struct intel_pipeline *pipeline);
void chv_dpio_signal_levels(struct intel_pipeline *pipeline,
	u32 deemp, u32 margin);
void chv_dpio_edp_signal_levels(struct intel_pipeline *pipeline,
	u32 deemp, u32 margin);
void chv_dpio_hdmi_swing_levels(struct intel_pipeline *pipeline,
	u32 dotclock);
void vlv_dpio_signal_levels(struct intel_pipeline *pipeline,
	u32 deemp, u32 margin);
void chv_dpio_enable_staggering(struct intel_pipeline *pipeline,
	u32 dotclock);
#endif /* _CHV_DPIO_H_ */
