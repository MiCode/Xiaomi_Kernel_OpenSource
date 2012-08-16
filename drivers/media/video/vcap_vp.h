/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef VCAP_VP_H
#define VCAP_VP_H

#include <linux/interrupt.h>

#include <media/vcap_v4l2.h>

#define VCAP_VP_INT_STATUS (VCAP_BASE + 0x404)
#define VCAP_VP_INT_CLEAR (VCAP_BASE + 0x40C)

#define VCAP_VP_SW_RESET (VCAP_BASE + 0x410)
#define VCAP_VP_INTERRUPT_ENABLE (VCAP_BASE + 0x408)

#define VCAP_VP_FILM_PROJECTION_T0 (VCAP_BASE + 0x50C)
#define VCAP_VP_FILM_PROJECTION_T2 (VCAP_BASE + 0x508)
#define VCAP_VP_FILM_PAST_MAX_PROJ (VCAP_BASE + 0x510)
#define VCAP_VP_FILM_PAST_MIN_PROJ (VCAP_BASE + 0x514)
#define VCAP_VP_FILM_SEQUENCE_HIST (VCAP_BASE + 0x504)
#define VCAP_VP_FILM_MODE_STATE (VCAP_BASE + 0x500)

#define VCAP_VP_BAL_VMOTION_STATE (VCAP_BASE + 0x690)
#define VCAP_VP_REDUCT_AVG_MOTION (VCAP_BASE + 0x610)
#define VCAP_VP_REDUCT_AVG_MOTION2 (VCAP_BASE + 0x614)

#define VCAP_VP_NR_AVG_LUMA (VCAP_BASE + 0x608)
#define VCAP_VP_NR_AVG_CHROMA (VCAP_BASE + 0x60C)
#define VCAP_VP_NR_CTRL_LUMA (VCAP_BASE + 0x600)
#define VCAP_VP_NR_CTRL_CHROMA (VCAP_BASE + 0x604)

#define VCAP_VP_BAL_AVG_BLEND (VCAP_BASE + 0x694)
#define VCAP_VP_VMOTION_HIST (VCAP_BASE + 0x6F8)

#define VCAP_VP_MOTION_EST_ADDR (VCAP_BASE + 0x4E0)
#define VCAP_VP_FILM_ANALYSIS_CONFIG (VCAP_BASE + 0x520)
#define VCAP_VP_FILM_STATE_CONFIG (VCAP_BASE + 0x524)

#define VCAP_VP_FVM_CONFIG (VCAP_BASE + 0x550)
#define VCAP_VP_FILM_ANALYSIS_CONFIG2 (VCAP_BASE + 0x52C)
#define VCAP_VP_MIXED_ANALYSIS_CONFIG (VCAP_BASE + 0x530)

#define VCAP_VP_SPATIAL_CONFIG (VCAP_BASE + 0x580)
#define VCAP_VP_SPATIAL_CONFIG2 (VCAP_BASE + 0x584)
#define VCAP_VP_SPATIAL_CONFIG3 (VCAP_BASE + 0x588)
#define VCAP_VP_TEMPORAL_CONFIG (VCAP_BASE + 0x5C0)

#define VCAP_VP_PIXEL_DIFF_CONFIG (VCAP_BASE + 0x6FC)
#define VCAP_VP_H_FREQ_CONFIG (VCAP_BASE + 0x528)
#define VCAP_VP_NR_CONFIG (VCAP_BASE + 0x620)
#define VCAP_VP_NR_LUMA_CONFIG (VCAP_BASE + 0x624)
#define VCAP_VP_NR_CHROMA_CONFIG (VCAP_BASE + 0x628)
#define VCAP_VP_BAL_CONFIG (VCAP_BASE + 0x680)
#define VCAP_VP_BAL_MOTION_CONFIG (VCAP_BASE + 0x684)
#define VCAP_VP_BAL_LIGHT_COMB (VCAP_BASE + 0x688)
#define VCAP_VP_BAL_VMOTION_CONFIG (VCAP_BASE + 0x68C)

#define VCAP_VP_NR_CONFIG2 (VCAP_BASE + 0x484)
#define VCAP_VP_FRAME_SIZE (VCAP_BASE + 0x48C)
#define VCAP_VP_SPLIT_SCRN_CTRL (VCAP_BASE + 0x750)

#define VCAP_VP_IN_CONFIG (VCAP_BASE + 0x480)
#define VCAP_VP_OUT_CONFIG (VCAP_BASE + 0x488)

#define VCAP_VP_T2_Y_BASE_ADDR (VCAP_BASE + 0x4C0)
#define VCAP_VP_T2_C_BASE_ADDR (VCAP_BASE + 0x4C4)
#define VCAP_VP_OUT_Y_BASE_ADDR (VCAP_BASE + 0x4CC)
#define VCAP_VP_OUT_C_BASE_ADDR (VCAP_BASE + 0x4D0)
#define VCAP_VP_OUT_CR_BASE_ADDR (VCAP_BASE + 0x4D4)

#define VCAP_VP_CTRL (VCAP_BASE + 0x4D8)

#define VCAP_VP_T1_Y_BASE_ADDR (VCAP_BASE + 0x4A8)
#define VCAP_VP_T1_C_BASE_ADDR (VCAP_BASE + 0x4Ac)
#define VCAP_VP_NR_T2_Y_BASE_ADDR (VCAP_BASE + 0x4B4)
#define VCAP_VP_NR_T2_C_BASE_ADDR (VCAP_BASE + 0x4B8)

#define VP_PIC_DONE (0x1 << 0)
#define VP_MODE_CHANGE (0x1 << 8)

#define VP_NR_MAX_WINDOW 120
#define VP_NR_MAX_RATIO  16

#define BITS_MASK(start, num_of_bits) \
	(((1 << (num_of_bits)) - 1) << (start))

#define BITS_VALUE(x, start, num_of_bits)  \
	(((x) & BITS_MASK(start, num_of_bits)) >> (start))

irqreturn_t vp_handler(struct vcap_dev *dev);
int config_vp_format(struct vcap_client_data *c_data);
void vp_stop_capture(struct vcap_client_data *c_data);
int init_motion_buf(struct vcap_client_data *c_data);
void deinit_motion_buf(struct vcap_client_data *c_data);
int init_nr_buf(struct vcap_client_data *c_data);
void deinit_nr_buf(struct vcap_client_data *c_data);
int nr_s_param(struct vcap_client_data *c_data, struct nr_param *param);
void nr_g_param(struct vcap_client_data *c_data, struct nr_param *param);
void s_default_nr_val(struct nr_param *param);
int kickoff_vp(struct vcap_client_data *c_data);
int continue_vp(struct vcap_client_data *c_data);
int vp_dummy_event(struct vcap_client_data *c_data);

#endif
