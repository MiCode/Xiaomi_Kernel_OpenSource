/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __FIQ_SMP_CALL_H
#define __FIQ_SMP_CALL_H

typedef void (*fiq_smp_call_func_t) (void *info, void *regs, void *svc_sp);

int fiq_smp_call_function(fiq_smp_call_func_t func, void *info, int wait);

#endif				/* !__FIQ_SMP_CALL_H */
