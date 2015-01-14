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

#ifndef __ISYS_IRQ_PRIVATE_H__
#define __ISYS_IRQ_PRIVATE_H__

#include "isys_irq_global.h"
#include "isys_irq_local.h"

#if defined(USE_INPUT_SYSTEM_VERSION_2401)

/* -------------------------------------------------------+
 |             Native command interface (NCI)             |
 + -------------------------------------------------------*/

/**
* @brief Get the isys irq status.
* Refer to "isys_irq.h" for details.
*/
STORAGE_CLASS_ISYS2401_IRQ_C void isys_irqc_state_get(
	const isys_irq_ID_t	isys_irqc_id,
	isys_irqc_state_t *state)
{
	state->status   = isys_irqc_reg_load(isys_irqc_id, ISYS_IRQ_STATUS_REG_IDX);
	state->edge     = isys_irqc_reg_load(isys_irqc_id, ISYS_IRQ_EDGE_REG_IDX);
	state->mask     = isys_irqc_reg_load(isys_irqc_id, ISYS_IRQ_MASK_REG_IDX);
	state->clear    = isys_irqc_reg_load(isys_irqc_id, ISYS_IRQ_CLEAR_REG_IDX);
	state->enable   = isys_irqc_reg_load(isys_irqc_id, ISYS_IRQ_ENABLE_REG_IDX);
	state->level_no = isys_irqc_reg_load(isys_irqc_id, ISYS_IRQ_LEVEL_NO_REG_IDX);
}

/**
* @brief Dump the isys irq status.
* Refer to "isys_irq.h" for details.
*/
STORAGE_CLASS_ISYS2401_IRQ_C void isys_irqc_state_dump(
	const isys_irq_ID_t	isys_irqc_id,
	const isys_irqc_state_t *state)
{
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
		"isys irq controller id %d"
		"\n\tstatus 0x%x\n\tedge 0x%x\n\tmask 0x%x\n\tclear 0x%x\n\tenable 0x%x\n\tlevel_no 0x%x\n",
		isys_irqc_id,
		state->status, state->edge, state->mask, state->clear, state->enable, state->level_no);
}

/** end of NCI */

/* -------------------------------------------------------+
 |              Device level interface (DLI)              |
 + -------------------------------------------------------*/

/* Support functions */
STORAGE_CLASS_ISYS2401_IRQ_C void isys_irqc_reg_store(
	const isys_irq_ID_t	isys_irqc_id,
	const unsigned int	reg_idx,
	const hrt_data	value)
{
	unsigned int reg_addr;

	assert(isys_irqc_id < N_ISYS_IRQ_ID);

	reg_addr = ISYS_IRQ_BASE[isys_irqc_id] + (reg_idx * sizeof(hrt_data));
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
		"isys irq store at addr(0x%x) val(%u)\n", reg_addr, (unsigned int)value);

	ia_css_device_store_uint32(reg_addr, value);
}

STORAGE_CLASS_ISYS2401_IRQ_C hrt_data isys_irqc_reg_load(
	const isys_irq_ID_t	isys_irqc_id,
	const unsigned int	reg_idx)
{
	unsigned int reg_addr;
	hrt_data value;

	assert(isys_irqc_id < N_ISYS_IRQ_ID);

	reg_addr = ISYS_IRQ_BASE[isys_irqc_id] + (reg_idx * sizeof(hrt_data));
	value = ia_css_device_load_uint32(reg_addr);
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
		"isys irq load from addr(0x%x) val(%u)\n", reg_addr, (unsigned int)value);

	return value;
}

/** end of DLI */

#endif /* defined(USE_INPUT_SYSTEM_VERSION_2401) */

#endif	/* __ISYS_IRQ_PRIVATE_H__ */
