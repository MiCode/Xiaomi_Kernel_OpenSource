/* Copyright (c) 2010, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/*
**  perftypes.h
**  DESCRIPTION
**  ksapi.ko function hooks header file
*/

#ifndef __PERFTYPES_H__
#define __PERFTYPES_H__

typedef void (*VPVF)(void);
typedef void (*VPULF)(unsigned long);
typedef void (*VPULULF)(unsigned long, unsigned long);

extern VPVF pp_interrupt_out_ptr;
extern VPVF pp_interrupt_in_ptr;
extern VPULF pp_process_remove_ptr;
extern void perf_mon_interrupt_in(void);
extern void perf_mon_interrupt_out(void);

#endif
