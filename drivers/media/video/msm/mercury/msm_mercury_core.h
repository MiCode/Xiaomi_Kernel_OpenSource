/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#ifndef MSM_MERCURY_CORE_H
#define MSM_MERCURY_CORE_H

#include <linux/interrupt.h>
#include "msm_mercury_hw.h"

#define msm_mercury_core_buf msm_mercury_hw_buf

irqreturn_t msm_mercury_core_irq(int irq_num, void *context);

void msm_mercury_core_irq_install(int (*irq_handler) (int, void *, void *));
void msm_mercury_core_irq_remove(void);

int msm_mercury_core_reset(void);
void msm_mercury_core_init(void);

#endif /* MSM_MERCURY_CORE_H */
