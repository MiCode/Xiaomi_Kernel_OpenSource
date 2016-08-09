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

#ifndef __SDE_IRQ_H__
#define __SDE_IRQ_H__

#include <linux/kernel.h>
#include <linux/irqdomain.h>

#include "msm_kms.h"

/**
 * sde_irq_controller - define MDSS level interrupt controller context
 * @enabled_mask:	enable status of MDSS level interrupt
 * @domain:		interrupt domain of this controller
 */
struct sde_irq_controller {
	unsigned long enabled_mask;
	struct irq_domain *domain;
};

/**
 * sde_irq_preinstall - perform pre-installation of MDSS IRQ handler
 * @kms:		pointer to kms context
 * @return:		none
 */
void sde_irq_preinstall(struct msm_kms *kms);

/**
 * sde_irq_postinstall - perform post-installation of MDSS IRQ handler
 * @kms:		pointer to kms context
 * @return:		0 if success; error code otherwise
 */
int sde_irq_postinstall(struct msm_kms *kms);

/**
 * sde_irq_uninstall - uninstall MDSS IRQ handler
 * @drm_dev:		pointer to kms context
 * @return:		none
 */
void sde_irq_uninstall(struct msm_kms *kms);

/**
 * sde_irq - MDSS level IRQ handler
 * @kms:		pointer to kms context
 * @return:		interrupt handling status
 */
irqreturn_t sde_irq(struct msm_kms *kms);

#endif /* __SDE_IRQ_H__ */
