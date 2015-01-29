/* Release Version: irci_master_20150128_1925 */
/* Release Version: irci_master_20150128_1925 */
/*
 * INTEL CONFIDENTIAL
 *
 * Copyright (C) 2010 - 2013 Intel Corporation.
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

#include "ia_css_hdr.host.h"

void
ia_css_hdr_init_config(
	struct sh_css_isp_hdr_params *to,
	const struct ia_css_hdr_config *from,
	unsigned size)
{
	int i;
	(void)size;

	for (i = 0; i < HDR_NUM_INPUT_FRAMES - 1; i++) {
		to->irradiance.match_shift[i] = from->irradiance.match_shift[i];
		to->irradiance.match_mul[i]   = from->irradiance.match_mul[i];
		to->irradiance.thr_low[i]     = from->irradiance.thr_low[i];
		to->irradiance.thr_high[i]    = from->irradiance.thr_high[i];
		to->irradiance.thr_coeff[i]   = from->irradiance.thr_coeff[i];
		to->irradiance.thr_shift[i]   = from->irradiance.thr_shift[i];
	}
	to->irradiance.test_irr    = from->irradiance.test_irr;
	to->irradiance.weight_bpp  = from->irradiance.weight_bpp;

	to->deghost.test_deg    = from->deghost.test_deg;
	to->exclusion.test_excl = from->exclusion.test_excl;
}
