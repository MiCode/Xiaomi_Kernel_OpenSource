/*
 * Support for Intel Camera Imaging ISP subsystem.
 * Copyright (c) 2015, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#ifndef __IA_CSS_EED1_8_TYPES_H
#define __IA_CSS_EED1_8_TYPES_H

#include "type_support.h"

/**
 * \brief EED1_8 public parameters.
 * \details Struct with all parameters for the EED1.8 kernel that can be set
 * from the CSS API.
 */

/* parameter list is based on ISP261 CSS API public parameter list_all.xlsx from 12-09-2014 */

/* Number of segments + 1 segment used in edge reliability enhancement
 * Ineffective: N/A
 * Default:	9
 */
#define IA_CSS_NUMBER_OF_DEW_ENHANCE_SEGMENTS	9


struct ia_css_eed1_8_config {
	int32_t rbzp_strength;	/**Strength of zipper reduction. */

	int32_t fcstrength;	/**Strength of false color reduction. */
	int32_t fcthres_0;	/**Threshold to prevent chroma coring due to nois or green disparity in dark region. */
	int32_t fcthres_1;	/**Threshold to prevent chroma coring due to nois or green disparity in bright region. */
	int32_t fc_sat_coef;	/**How much color saturation to maintain in high color saturation region. */
	int32_t fc_coring_prm;	/**Chroma coring coefficient for tint color suppression. */

	int32_t aerel_thres0;	/**Threshold for Non-Directional Reliability at dark region. */
	int32_t aerel_gain0;	/**Gain for Non-Directional Reliability at dark region. */
	int32_t aerel_thres1;	/**Threshold for Non-Directional Reliability at bright region. */
	int32_t aerel_gain1;	/**Gain for Non-Directional Reliability at bright region. */

	int32_t derel_thres0;	/**Threshold for Directional Reliability at dark region. */
	int32_t derel_gain0;	/**Gain for Directional Reliability at dark region. */
	int32_t derel_thres1;	/**Threshold for Directional Reliability at bright region. */
	int32_t derel_gain1;	/**Gain for Directional Reliability at bright region. */

	int32_t coring_pos0;	/**Positive Edge Coring Threshold in dark region. */
	int32_t coring_pos1;	/**Positive Edge Coring Threshold in bright region. */
	int32_t coring_neg0;	/**Negative Edge Coring Threshold in dark region. */
	int32_t coring_neg1;	/**Negative Edge Coring Threshold in bright region. */

	int32_t gain_exp;	/**Common Exponent of Gain. */
	int32_t gain_pos0;	/**Gain for Positive Edge in dark region. */
	int32_t gain_pos1;	/**Gain for Positive Edge in bright region. */
	int32_t gain_neg0;	/**Gain for Negative Edge in dark region. */
	int32_t gain_neg1;	/**Gain for Negative Edge in bright region. */

	int32_t pos_margin0;	/**Margin for Positive Edge in dark region. */
	int32_t pos_margin1;	/**Margin for Positive Edge in bright region. */
	int32_t neg_margin0;	/**Margin for Negative Edge in dark region. */
	int32_t neg_margin1;	/**Margin for Negative Edge in bright region. */

	int32_t dew_enhance_seg_x[IA_CSS_NUMBER_OF_DEW_ENHANCE_SEGMENTS];	/**Segment data for directional edge weight. */
	int32_t dew_enhance_seg_y[IA_CSS_NUMBER_OF_DEW_ENHANCE_SEGMENTS];	/**Segment data for directional edge weight. */
	int32_t dedgew_max;	/**Max Weight for Directional Edge. */
};

#endif /* __IA_CSS_EED1_8_TYPES_H */
