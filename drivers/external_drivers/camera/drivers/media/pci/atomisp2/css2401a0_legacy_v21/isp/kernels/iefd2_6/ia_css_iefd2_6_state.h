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

#ifndef _IA_CSS_IEFD2_6_STATE_H
#define _IA_CSS_IEFD2_6_STATE_H

#include "type_support.h"
#include "vmem.h" /* for VMEM_ARRAY*/
#include "iefd2_6_vssnlm.isp.h"
#include "iefd2_6.isp.h"

struct iefd2_6_vmem_state {
	/* State buffers required for main IEFD2_6 */
	VMEM_ARRAY(iefd2_6_input_lines[IEFD2_6_STATE_INPUT_BUFFER_HEIGHT], IEFD2_6_STATE_INPUT_BUFFER_WIDTH*ISP_NWAY);
	/* State buffers required for VSSNLM sub-kernel */
	VMEM_ARRAY(vssnlm_input_y[VSSNLM_STATE_INPUT_BUFFER_HEIGHT], VSSNLM_STATE_INPUT_BUFFER_WIDTH*ISP_NWAY);
	VMEM_ARRAY(vssnlm_input_diff_grad[1], VSSNLM_STATE_INPUT_BUFFER_WIDTH*ISP_NWAY);
};

#endif /* _IA_CSS_IEFD2_6_STATE_H */

