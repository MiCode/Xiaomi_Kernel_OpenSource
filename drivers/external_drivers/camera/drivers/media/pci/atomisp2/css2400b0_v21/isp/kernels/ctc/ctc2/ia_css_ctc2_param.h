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

#ifndef __IA_CSS_CTC2_PARAM_H
#define __IA_CSS_CTC2_PARAM_H

#define IA_CSS_CTC_COEF_SHIFT          13
#include "vmem.h" /* needed for VMEM_ARRAY */

/* CTC (Chroma Tone Control)ISP Parameters */

/*VMEM Luma params*/
struct ia_css_isp_ctc2_vmem_params {
	/**< Gains by Y(Luma) at Y = 0.0,Y_X1, Y_X2, Y_X3, Y_X4*/
	VMEM_ARRAY(y_x, ISP_VEC_NELEMS);
	/** kneepoints by Y(Luma) 0.0, y_x1, y_x2, y _x3, y_x4*/
	VMEM_ARRAY(y_y, ISP_VEC_NELEMS);
	/** Slopes of lines interconnecting
	 *  0.0 -> y_x1 -> y_x2 -> y _x3 -> y_x4 -> 1.0*/
	VMEM_ARRAY(e_y_slope, ISP_VEC_NELEMS);
};

/*DMEM Chroma params*/
struct ia_css_isp_ctc2_dmem_params {

	/** Gains by UV(Chroma) under kneepoints uv_x0 and uv_x1*/
	int32_t uv_y0;
	int32_t uv_y1;

	/** Kneepoints by UV(Chroma)- uv_x0 and uv_x1*/
	int32_t uv_x0;
	int32_t uv_x1;

	/** Slope of line interconnecting uv_x0 -> uv_x1*/
	int32_t uv_dydx;

};
#endif /* __IA_CSS_CTC2_PARAM_H */
