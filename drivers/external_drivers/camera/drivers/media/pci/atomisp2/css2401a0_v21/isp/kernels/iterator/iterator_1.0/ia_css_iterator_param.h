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

#ifndef __IA_CSS_ITERATOR_PARAM_H
#define __IA_CSS_ITERATOR_PARAM_H

#include "ia_css_types.h" /* ia_css_resolution */
#include "ia_css_frame_public.h" /* ia_css_frame_info */
#include "ia_css_frame_comm.h" /* ia_css_frame_sp_info */

struct ia_css_iterator_configuration {
	const struct ia_css_frame_info *input_info;
	const struct ia_css_frame_info *internal_info;
	const struct ia_css_frame_info *output_info;
	const struct ia_css_frame_info *vf_info;
	const struct ia_css_resolution *dvs_envelope;
};

struct sh_css_isp_iterator_isp_config {
	struct ia_css_frame_sp_info input_info;
	struct ia_css_frame_sp_info internal_info;
	struct ia_css_frame_sp_info output_info;
	struct ia_css_frame_sp_info vf_info;
	struct ia_css_sp_resolution dvs_envelope;
};

#endif /* __IA_CSS_ITERATOR_PARAM_H */
