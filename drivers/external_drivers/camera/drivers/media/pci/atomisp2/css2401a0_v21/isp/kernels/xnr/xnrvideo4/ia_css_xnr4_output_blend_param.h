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
#ifndef __IA_CSS_XNR4_OUTPUT_BLEND_PARAM_H
#define __IA_CSS_XNR4_OUTPUT_BLEND_PARAM_H

#include "isp/kernels/xnr/xnrvideo4/ia_css_xnr4_common_param.h"
#include "isp/kernels/xnr/xnrvideo4/ia_css_xnr4_upsample_param.h"

/* Output Blend Subkernel Configuration */

/* TODO: get the delay from JBL */
#define XNR4_JBL_VER_DELAY			(3)
#define XNR4_JBL_HOR_DELAY			(1)

#define XNR4_OUTPUT_BLEND_VER_DELAY		((XNR4_JBL_VER_DELAY * XNR4_LUMA_DS_FACTOR) + \
						(XNR4_UPSCALE_VER_DELAY * XNR4_LUMA_DS_FACTOR))
#define XNR4_OUTPUT_BLEND_HOR_DELAY		((XNR4_JBL_HOR_DELAY * XNR4_LUMA_DS_FACTOR) + \
						(XNR4_UPSCALE_HOR_DELAY * XNR4_LUMA_DS_FACTOR))

/* XNR4 Output Blend Parameters */
struct sh_css_isp_xnr4_output_blend_params {
	uint16_t m_bypass_mf_y; /* Output blend bypass value - luma */
	uint16_t m_bypass_mf_c; /* Output blend bypass value - chroma */
};

#endif /* __IA_CSS_XNR4_RADIAL_METRIC_PARAM_H */
