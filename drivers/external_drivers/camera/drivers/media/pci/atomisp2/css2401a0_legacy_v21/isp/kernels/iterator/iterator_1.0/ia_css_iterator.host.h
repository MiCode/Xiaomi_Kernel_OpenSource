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

#ifndef __IA_CSS_ITERATOR_HOST_H
#define __IA_CSS_ITERATOR_HOST_H

#include "ia_css_frame_public.h"
#include "ia_css_binary.h"
#include "ia_css_err.h"
#include "ia_css_iterator_param.h"

void
ia_css_iterator_config(
	struct sh_css_isp_iterator_isp_config *to,
	const struct ia_css_iterator_configuration *from,
	unsigned size);

enum ia_css_err
ia_css_iterator_configure(
	const struct ia_css_binary *binary,
	const struct ia_css_frame_info *in_info);

#endif /* __IA_CSS_ITERATOR_HOST_H */
