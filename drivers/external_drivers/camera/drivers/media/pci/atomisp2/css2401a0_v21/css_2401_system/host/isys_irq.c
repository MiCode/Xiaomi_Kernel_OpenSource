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

#include <system_local.h>
#include "device_access.h"
#include "assert_support.h"
#include "ia_css_debug.h"
#include "isys_irq.h"

#ifndef __INLINE_ISYS2401_IRQ__
/*
 * Include definitions for isys irq private functions. isys_irq.h includes
 * declarations of these functions by including isys_irq_public.h.
 */
#include "isys_irq_private.h"
#endif

/** Public interface */
STORAGE_CLASS_ISYS2401_IRQ_C void isys_irqc_status_enable(
	const isys_irq_ID_t	isys_irqc_id)
{
	assert(isys_irqc_id < N_ISYS_IRQ_ID);

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "Setting irq mask for port %u\n", isys_irqc_id);
	isys_irqc_reg_store(isys_irqc_id, ISYS_IRQ_MASK_REG_IDX, ISYS_IRQ_MASK_REG_VALUE);
	isys_irqc_reg_store(isys_irqc_id, ISYS_IRQ_CLEAR_REG_IDX, ISYS_IRQ_CLEAR_REG_VALUE);
	isys_irqc_reg_store(isys_irqc_id, ISYS_IRQ_ENABLE_REG_IDX, ISYS_IRQ_ENABLE_REG_VALUE);
}
