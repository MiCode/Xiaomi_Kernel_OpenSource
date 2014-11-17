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

#ifndef __IA_CSS_CTC2_HOST_H
#define __IA_CSS_CTC2_HOST_H

#include "ia_css_ctc2_param.h"
#include "ia_css_ctc2_types.h"

extern const struct ia_css_ctc2_config default_ctc2_config;

/*Encode Functions to translate parameters from userspace into ISP space*/

void ia_css_ctc2_vmem_encode(struct ia_css_isp_ctc2_vmem_params *to,
			     const struct ia_css_ctc2_config *from);

void ia_css_ctc2_dmem_encode(struct ia_css_isp_ctc2_dmem_params *to,
			     struct ia_css_ctc2_config *from);

#endif /* __IA_CSS_CTC2_HOST_H */
