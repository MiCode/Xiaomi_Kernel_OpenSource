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

#ifndef __ISYS_IRQ_PUBLIC_H__
#define __ISYS_IRQ_PUBLIC_H__

#include "isys_irq_global.h"
#include "isys_irq_local.h"

#if defined(USE_INPUT_SYSTEM_VERSION_2401)

STORAGE_CLASS_ISYS2401_IRQ_H void isys_irqc_state_get(
	const isys_irq_ID_t	isys_irqc_id,
	isys_irqc_state_t	*state);

STORAGE_CLASS_ISYS2401_IRQ_H void isys_irqc_state_dump(
	const isys_irq_ID_t	isys_irqc_id,
	const isys_irqc_state_t *state);

STORAGE_CLASS_ISYS2401_IRQ_H void isys_irqc_reg_store(
	const isys_irq_ID_t	isys_irqc_id,
	const unsigned int	reg_idx,
	const hrt_data		value);

STORAGE_CLASS_ISYS2401_IRQ_H hrt_data isys_irqc_reg_load(
	const isys_irq_ID_t	isys_irqc_id,
	const unsigned int	reg_idx);

STORAGE_CLASS_ISYS2401_IRQ_H void isys_irqc_status_enable(
	const isys_irq_ID_t	isys_irqc_id);

#endif /* defined(USE_INPUT_SYSTEM_VERSION_2401) */

#endif	/* __ISYS_IRQ_PUBLIC_H__ */
