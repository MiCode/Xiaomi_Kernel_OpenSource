/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015-2019, The Linux Foundation. All rights reserved.
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

/**
 * sde_irq_update - enable/disable IRQ line
 * @kms:		pointer to kms context
 * @enable:		enable:true, disable:false
 */
void sde_irq_update(struct msm_kms *kms, bool enable);

#endif /* __SDE_IRQ_H__ */
