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

#include "ia_css_types.h"
#include "sh_css_defs.h"
#ifndef IA_CSS_NO_DEBUG
#include "ia_css_debug.h"
#endif
#include "sh_css_frac.h"

#include "ia_css_xnr4_output_blend.host.h"

const struct ia_css_xnr4_output_blend_config default_xnr4_output_blend_config = {
	0, /* Output blend bypass value - luma */
	0 /* Output blend bypass value - chroma */
};

void ia_css_xnr4_output_blend_encode(
	struct sh_css_isp_xnr4_output_blend_params *to,
	const struct ia_css_xnr4_output_blend_config *from,
	unsigned size)
{
	(void)size;
	to->m_bypass_mf_y  = from->m_bypass_mf_y;
	to->m_bypass_mf_c  = from->m_bypass_mf_c;
}
