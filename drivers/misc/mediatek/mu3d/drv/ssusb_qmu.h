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

#ifndef __SSUSB_QMU_H__
#define __SSUSB_QMU_H__

#include "mu3d_hal_qmu_drv.h"

#ifdef USE_SSUSB_QMU

#undef EXTERN
#define EXTERN

/* Sanity CR check in */
void qmu_done_tasklet(unsigned long data);
void qmu_error_recovery(unsigned long data);
void qmu_exception_interrupt(struct musb *musb, unsigned int wQmuVal);

#endif
#endif
