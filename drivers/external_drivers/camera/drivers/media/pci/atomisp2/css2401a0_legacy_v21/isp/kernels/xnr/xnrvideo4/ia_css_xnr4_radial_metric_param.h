/*
 * INTEL CONFIDENTIAL
 *
 * Copyright (C) 2010 - 2015 Intel Corporation.
 * All Rights Reserved.
 *
 * The source code contained or described herein and all documents
 * related to the source code ("Material") are owned by Intel Corporation
 * or licensors. Title to the Material remains with Intel
 * Corporation or its licensors. The Material contains trade
 * secrets and proprietary and confidential information of Intel or its
 * licensors. The Material is protected by worldwide copyright
 * and trade secret laws and treaty provisions. No part of the Material may
 * be used, copied, reproduced, modified, published, uploaded, posted,
 * transmitted, distributed, or disclosed in any way without Intel's prior
 * express written permission.
 *
 * No License under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or
 * delivery of the Materials, either expressly, by implication, inducement,
 * estoppel or otherwise. Any license under such intellectual property rights
 * must be express and approved by Intel in writing.
 */
#ifndef __IA_CSS_XNR4_RADIAL_METRIC_PARAM_H
#define __IA_CSS_XNR4_RADIAL_METRIC_PARAM_H

#include "isp/kernels/xnr/xnrvideo4/ia_css_xnr4_common_param.h"


/* Radial Metric Subkernel Configuration */

#define XNR4_RM_FILTER_VRT_DELAY	(0) /* no delay */
#define XNR4_RM_FILTER_HOR_DELAY	(0) /* no delay */

#define LOG2_XNR4_RM_FACTOR	(2)	/* Log2(XNR4_RM_FACTOR) = Log2(4)*/
#define XNR4_RM_FACTOR		(1 << LOG2_XNR4_RM_FACTOR)
#define XNR4_RM_FACTOR_SQR	(1 << (2 * LOG2_XNR4_RM_FACTOR))
#define MSD_RAD_MAX_R2_PREC	(26)
#define MSD_RAD_INV_PREC	(15)
#define MSD_RAD_CU_PREC		(15)

/* XNR4 Radial Metric Parameters */
struct sh_css_isp_xnr4_radial_metric_params {
	int16_t m_rad_Xreset; /* X radial reset value */
	int16_t m_rad_Yreset; /* Y radial reset value */
	uint32_t m_rad_X2reset; /* X^2 radial reset value */
	uint32_t m_rad_Y2reset; /* Y^2 radial reset value */
	uint8_t m_rad_enable; /* Enable radial computation */
	uint16_t m_rad_inv_r2; /* Radial metric normalization factor */
};


#endif /* __IA_CSS_XNR4_RADIAL_METRIC_PARAM_H */
