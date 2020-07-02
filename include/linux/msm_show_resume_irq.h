/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#ifndef __MSM_SHOW_RESUME_IRQ_H
#define __MSM_SHOW_RESUME_IRQ_H

#ifdef CONFIG_QCOM_SHOW_RESUME_IRQ
extern int msm_show_resume_irq_mask;
#else
#define msm_show_resume_irq_mask 0
#endif

#endif


