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
#ifndef __IA_CSS_XNR4_COMMON_PARAM_H
#define __IA_CSS_XNR4_COMMON_PARAM_H

#include "type_support.h"
/* XNR4 (eXtreme Noise Reduction */

/* XNR4 Algorithm Configuration */
#define MSD_INP_BPP             (16)
#define MSD_INP_BPP_U		(MSD_INP_BPP - 1)
#define XNR4_STRIPE_SIZE	(2560)


/* Medium frequency Subkernel Configuration */
#define XNR4_MF_FILTER_SIZE		(5)
#define XNR4_MF_FILTER_DELAY_PIX	(XNR4_MF_FILTER_SIZE / 2) /* 2 pixel delay due to 5 tab filter */

#define XNR4_MF_FILTER_VRT_DELAY	(XNR4_MF_FILTER_DELAY_PIX) /* 2 lines of delay */
#define XNR4_MF_FILTER_HOR_DELAY	(1) /* 1 vector delay - to align delay to vector boundary */


#endif /* __IA_CSS_XNR4_COMMON_PARAM_H */
