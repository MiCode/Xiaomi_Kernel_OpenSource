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

#ifndef __IA_CSS_CTC2_TYPES_H
#define __IA_CSS_CTC2_TYPES_H

/** Chroma Tone Control configuration.
*
*  ISP block: CTC2 (CTC by polygonal approximation)
* (ISP1: CTC1 (CTC by look-up table) is used.)
*  ISP2: CTC2 is used.
*  ISP261: CTC2 (CTC by Fast Approximate Distance)
*/
struct ia_css_ctc2_config {

	/**< Gains by Y(Luma) at Y =0.0,Y_X1, Y_X2, Y_X3, Y_X4 and Y_X5
	*   --default/ineffective value: 4096(0.5f)
	*/
	uint16_t y_y0;
	uint16_t y_y1;
	uint16_t y_y2;
	uint16_t y_y3;
	uint16_t y_y4;
	uint16_t y_y5;
	/** 1st-4th  kneepoints by Y(Luma) --default/ineffective value:n/a
	*   requirement: 0.0 < y_x1 < y_x2 <y _x3 < y_x4 < 1.0
	*/
	uint16_t y_x1;
	uint16_t y_x2;
	uint16_t y_x3;
	uint16_t y_x4;
	/** Gains by UV(Chroma) under threholds uv_x0 and uv_x1
	*   --default/ineffective value: 4096(0.5f)
	*/
	uint16_t uv_y0;
	uint16_t uv_y1;
	/** Minimum and Maximum Thresholds by UV(Chroma)- uv_x0 and uv_x1
	*   --default/ineffective value: n/a
	*/
	uint16_t uv_x0;
	uint16_t uv_x1;
	};

#endif /* __IA_CSS_CTC2_TYPES_H */
