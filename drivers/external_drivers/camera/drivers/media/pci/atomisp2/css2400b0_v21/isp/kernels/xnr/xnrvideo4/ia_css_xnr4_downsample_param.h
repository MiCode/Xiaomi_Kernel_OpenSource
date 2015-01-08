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
#ifndef __IA_CSS_XNR4_DOWNSAMPLE_PARAM_H
#define __IA_CSS_XNR4_DOWNSAMPLE_PARAM_H

#include "isp/kernels/xnr/xnrvideo4/ia_css_xnr4_common_param.h"

/* XNR4 Algorithm Configuration */
#define XNR4_LUMA_FILTER_TAP    (7)
#define XNR4_CHROMA_FILTER_TAP  (3)
#define XNR4_LUMA_DS_FACTOR     (4)
#define XNR4_CHROMA_DS_FACTOR   (2)
#define MSD_DS_IN_PREC          (MSD_INP_BPP - 1)
#define MSD_DS_IN_PREC_MAX_VAL  ((1<<MSD_DS_IN_PREC)-1)
#define MSD_DS_COEF_PREC        (9)


/* XNR4 DS Parameters */
struct sh_css_isp_xnr4_params {
	uint8_t m_ds4_coeffs[(XNR4_LUMA_FILTER_TAP/2)+1];
	uint8_t m_ds2_coeffs[(XNR4_CHROMA_FILTER_TAP/2)+1];
};

#endif /* __IA_CSS_XNR4_DOWNSAMPLE_PARAM_H */
