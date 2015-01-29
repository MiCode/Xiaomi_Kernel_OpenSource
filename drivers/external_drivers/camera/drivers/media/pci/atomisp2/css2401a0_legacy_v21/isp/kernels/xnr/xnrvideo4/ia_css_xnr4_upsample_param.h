/*
 * INTEL CONFIDENTIAL
 *
 * Copyright (C) 2010 - 2014 Intel Corporation.
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
#ifndef _IA_CSS_XNR4_UPSAMPLE_PARAM_H_
#define _IA_CSS_XNR4_UPSAMPLE_PARAM_H_

#include "isp/kernels/xnr/xnrvideo4/ia_css_xnr4_downsample_param.h"

/* XNR4 UPSAMPLE Algorithm Configuration */
#define XNR4_LUMA_UP_FACTOR     (XNR4_LUMA_DS_FACTOR)
#define XNR4_CHROMA_UP_FACTOR   (XNR4_CHROMA_DS_FACTOR)

#define XNR4_UP_FILTER_TAP	4

#define XNR4_UPSCALE_HOR_DELAY  1
#define XNR4_UPSCALE_VER_DELAY  (XNR4_UP_FILTER_TAP / 2)

/* XNR4 UPSAMPLE4 COEFFS, SHIFT FACTORS
 * Upsample coefficients are fixed and are therefore compile time
 * resolved.
 */
/* Upsample X4 Phase 0 coeffs */
#define XNR4_UP4_COEFF_0_0	0
#define XNR4_UP4_COEFF_0_1	1
#define XNR4_UP4_COEFF_0_2	0
#define XNR4_UP4_COEFF_0_3	0
/* Upsample X4 Phase 1 coeffs */
#define XNR4_UP4_COEFF_1_0	-9
#define XNR4_UP4_COEFF_1_1	111
#define XNR4_UP4_COEFF_1_2	29
#define XNR4_UP4_COEFF_1_3	-3
/* Upsample X4 Phase 2 coeffs */
#define XNR4_UP4_COEFF_2_0	-1
#define XNR4_UP4_COEFF_2_1	9
#define XNR4_UP4_COEFF_2_2	9
#define XNR4_UP4_COEFF_2_3	-1
/* Upsample X4 Phase 3 coeffs */
#define XNR4_UP4_COEFF_3_0	-3
#define XNR4_UP4_COEFF_3_1	29
#define XNR4_UP4_COEFF_3_2	111
#define XNR4_UP4_COEFF_3_3	-9
/* Upsample X4 Shift factors for each tap */
#define XNR4_UP4_SHIFT_0	0
#define XNR4_UP4_SHIFT_1	7
#define XNR4_UP4_SHIFT_2	4
#define XNR4_UP4_SHIFT_3	7



#endif /*_IA_CSS_XNR4_UPSAMPLE_PARAM_H_ */
