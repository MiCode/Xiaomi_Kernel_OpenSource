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
#ifndef __IA_CSS_XNR4_DOWNSAMPLE_TYPES_H
#define __IA_CSS_XNR4_DOWNSAMPLE_TYPES_H

#include "ia_css_xnr4_downsample_param.h"

/** @file
* CSS-API header file for XNR4 parameters.
*/


/** XNR4 downsample configuration .  */
struct ia_css_xnr4_downsample_config {
	uint8_t m_ds4_coeffs[(XNR4_LUMA_FILTER_TAP/2)+1]; /** < Down sample filter coefficients,
				       used for symmetric 7tab filtering of luma
				       u[integer_bits].[8-integer_bits], [0,255] */
	uint8_t m_ds2_coeffs[(XNR4_CHROMA_FILTER_TAP/2)+1]; /** < Down sample filter coefficients,
				       used for symmetric 3tab filtering of chroma
				       u[integer_bits].[8-integer_bits], [0,255] */
};

#endif /* __IA_CSS_XNR4_DOWNSAMPLE_TYPES_H */
