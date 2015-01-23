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

#include "ia_css_xnr4_radial_metric.host.h"

const struct ia_css_xnr4_radial_metric_config default_xnr4_radial_metric_config = {
	0, /* X radial reset value */
	0, /* Y radial reset value */
	0, /* X^2 radial reset value */
	0, /* Y^2 radial reset value */
	1, /* Enable radial computation */
	127 /* Radial metric normalization factor */
};

void ia_css_xnr4_radial_metric_encode(
	struct sh_css_isp_xnr4_radial_metric_params *to,
	const struct ia_css_xnr4_radial_metric_config *from,
	unsigned size)
{
	(void)size;
	to->m_rad_Xreset  = from->m_rad_Xreset;
	to->m_rad_Yreset  = from->m_rad_Yreset;
	to->m_rad_X2reset = from->m_rad_X2reset;
	to->m_rad_Y2reset = from->m_rad_Y2reset;
	to->m_rad_enable  = from->m_rad_enable;
	to->m_rad_inv_r2  = from->m_rad_inv_r2;
}


