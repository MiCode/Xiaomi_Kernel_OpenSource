/*
 * Support for Intel Camera Imaging ISP subsystem.
 *
 * Copyright (c) 2010 - 2014 Intel Corporation. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#ifndef __IA_CSS_BNR22_TYPES_H
#define __IA_CSS_BNR22_TYPES_H


/**
 * \brief BNR22 public parameters.
 * \details Struct with all parameters for the BNR2.2 kernel that can be set
 * from the CSS API.
 */
struct ia_css_bnr22_config {
	int32_t d_var_gain_r0;		/**Directional variance gain for R, in dark region. */
	int32_t d_var_gain_g0;		/**Directional variance gain for G, in dark region. */
	int32_t d_var_gain_b0;		/**Directional variance gain for B, in dark region. */
	int32_t d_var_gain_r1;		/**Directional variance gain for R, in bright region. */
	int32_t d_var_gain_g1;		/**Directional variance gain for G, in bright region. */
	int32_t d_var_gain_b1;		/**Directional variance gain for B, in bright region. */
	int32_t n_var_gain_r0;		/**Non-Directional variance gain for R, in dark region. */
	int32_t n_var_gain_g0;		/**Non-Directional variance gain for G, in dark region. */
	int32_t n_var_gain_b0;		/**Non-Directional variance gain for B, in dark region. */
	int32_t n_var_gain_r1;		/**Non-Directional variance gain for R, in bright region. */
	int32_t n_var_gain_g1;		/**Non-Directional variance gain for G, in bright region. */
	int32_t n_var_gain_b1;		/**Non-Directional variance gain for B, in bright region. */
	int32_t d_thres;		/**Threshold for directional filtering */
	int32_t d_thres_w;		/**Threshold width for directional filtering */
	int32_t var_offset_coef;	/**Variance offset coefficient noise subtraction level */
	int32_t d_gain;			/**Gain for directional coefficient */
	int32_t detail_threshold;	/**Threshold for low contrast texture control */
	int32_t detail_level_offset;	/**Offset value for low contrast texture control */
	int32_t d_var_th_min;		/**Minimum clipping value for directional variance*/
	int32_t d_var_th_max;		/**Maximum clipping value for diretional variance*/
	int32_t n_var_th_min;		/**Minimum clipping value for non-directional variance*/
	int32_t n_var_th_max;		/**Maximum clipping value for non-directional variance*/
};

#endif /* __IA_CSS_BNR22_TYPES_H */
