/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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

#ifndef __MSM_CTI_PMU_IRQ_H
#define __MSM_CTI_PMU_IRQ_H

#include <linux/workqueue.h>

#ifdef CONFIG_MSM8994_V1_PMUIRQ_WA
void msm_enable_cti_pmu_workaround(struct work_struct *work);
struct coresight_cti *msm_get_cpu_cti(int cpu);
void msm_cti_pmu_irq_ack(int cpu);
#else
static inline void msm_enable_cti_pmu_workaround(struct work_struct *work) { }
static inline struct coresight_cti *msm_get_cpu_cti(int cpu) { return NULL; }
static inline void msm_cti_pmu_irq_ack(int cpu) { }
#endif
#endif
