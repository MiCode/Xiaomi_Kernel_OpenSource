/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#ifndef __QCOM_IRQ_H
#define __QCOM_IRQ_H

#include <linux/irqdomain.h>

/**
 * struct qcom_irq_fwspec - qcom specific irq fwspec wrapper
 * @fwspec: irq fwspec
 * @mask: if true, keep the irq masked in the gpio controller
 *
 * Use this structure to communicate between the parent irq chip, MPM or PDC,
 * to the gpio chip, TLMM, about the gpio being allocated in the parent
 * and if the gpio chip should keep the line masked because the parent irq
 * chip is handling everything about the irq line.
 */
struct qcom_irq_fwspec {
	struct irq_fwspec fwspec;
	bool mask;
};

#endif
