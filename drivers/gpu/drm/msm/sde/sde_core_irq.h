/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __SDE_CORE_IRQ_H__
#define __SDE_CORE_IRQ_H__

#include "sde_kms.h"
#include "sde_hw_interrupts.h"

/**
 * sde_core_irq_preinstall - perform pre-installation of core IRQ handler
 * @sde_kms:		SDE handle
 * @return:		none
 */
void sde_core_irq_preinstall(struct sde_kms *sde_kms);

/**
 * sde_core_irq_postinstall - perform post-installation of core IRQ handler
 * @sde_kms:		SDE handle
 * @return:		0 if success; error code otherwise
 */
int sde_core_irq_postinstall(struct sde_kms *sde_kms);

/**
 * sde_core_irq_uninstall - uninstall core IRQ handler
 * @sde_kms:		SDE handle
 * @return:		none
 */
void sde_core_irq_uninstall(struct sde_kms *sde_kms);

/**
 * sde_core_irq - core IRQ handler
 * @sde_kms:		SDE handle
 * @return:		interrupt handling status
 */
irqreturn_t sde_core_irq(struct sde_kms *sde_kms);

/**
 * sde_core_irq_idx_lookup - IRQ helper function for lookup irq_idx from HW
 *                      interrupt mapping table.
 * @sde_kms:		SDE handle
 * @intr_type:		SDE HW interrupt type for lookup
 * @instance_idx:	SDE HW block instance defined in sde_hw_mdss.h
 * @return:		irq_idx or -EINVAL when fail to lookup
 */
int sde_core_irq_idx_lookup(
		struct sde_kms *sde_kms,
		enum sde_intr_type intr_type,
		uint32_t instance_idx);

/**
 * sde_core_irq_enable - IRQ helper function for enabling one or more IRQs
 * @sde_kms:		SDE handle
 * @irq_idxs:		Array of irq index
 * @irq_count:		Number of irq_idx provided in the array
 * @return:		0 for success enabling IRQ, otherwise failure
 *
 * This function increments count on each enable and decrements on each
 * disable.  Interrupts is enabled if count is 0 before increment.
 */
int sde_core_irq_enable(
		struct sde_kms *sde_kms,
		int *irq_idxs,
		uint32_t irq_count);

/**
 * sde_core_irq_disable - IRQ helper function for disabling one of more IRQs
 * @sde_kms:		SDE handle
 * @irq_idxs:		Array of irq index
 * @irq_count:		Number of irq_idx provided in the array
 * @return:		0 for success disabling IRQ, otherwise failure
 *
 * This function increments count on each enable and decrements on each
 * disable.  Interrupts is disabled if count is 0 after decrement.
 */
int sde_core_irq_disable(
		struct sde_kms *sde_kms,
		int *irq_idxs,
		uint32_t irq_count);

/**
 * sde_core_irq_read - IRQ helper function for reading IRQ status
 * @sde_kms:		SDE handle
 * @irq_idx:		irq index
 * @clear:		True to clear the irq after read
 * @return:		non-zero if irq detected; otherwise no irq detected
 */
u32 sde_core_irq_read(
		struct sde_kms *sde_kms,
		int irq_idx,
		bool clear);

/**
 * sde_core_irq_register_callback - For registering callback function on IRQ
 *                             interrupt
 * @sde_kms:		SDE handle
 * @irq_idx:		irq index
 * @irq_cb:		IRQ callback structure, containing callback function
 *			and argument. Passing NULL for irq_cb will unregister
 *			the callback for the given irq_idx
 *			This must exist until un-registration.
 * @return:		0 for success registering callback, otherwise failure
 *
 * This function supports registration of multiple callbacks for each interrupt.
 */
int sde_core_irq_register_callback(
		struct sde_kms *sde_kms,
		int irq_idx,
		struct sde_irq_callback *irq_cb);

/**
 * sde_core_irq_unregister_callback - For unregistering callback function on IRQ
 *                             interrupt
 * @sde_kms:		SDE handle
 * @irq_idx:		irq index
 * @irq_cb:		IRQ callback structure, containing callback function
 *			and argument. Passing NULL for irq_cb will unregister
 *			the callback for the given irq_idx
 *			This must match with registration.
 * @return:		0 for success registering callback, otherwise failure
 *
 * This function supports registration of multiple callbacks for each interrupt.
 */
int sde_core_irq_unregister_callback(
		struct sde_kms *sde_kms,
		int irq_idx,
		struct sde_irq_callback *irq_cb);

#endif /* __SDE_CORE_IRQ_H__ */
