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

#ifndef __ISYS_IRQ_LOCAL_H__
#define __ISYS_IRQ_LOCAL_H__

#include <type_support.h>

#if defined(USE_INPUT_SYSTEM_VERSION_2401)

typedef struct isys_irqc_state_s isys_irqc_state_t;

struct isys_irqc_state_s {
	hrt_data edge;
	hrt_data mask;
	hrt_data status;
	hrt_data clear;
	hrt_data enable;
	hrt_data level_no;
};

#endif /* defined(USE_INPUT_SYSTEM_VERSION_2401) */

#endif	/* __ISYS_IRQ_LOCAL_H__ */
