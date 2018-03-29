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

#ifndef _MT_DCM_H
#define _MT_DCM_H

#define CPU_DCM			(1U << 0)
#define IFR_DCM			(1U << 1)
#define PER_DCM			(1U << 2)
#define SMI_DCM			(1U << 3)
#define EMI_DCM			(1U << 4)
#define DIS_DCM			(1U << 5)
#define ISP_DCM			(1U << 6)
#define VDE_DCM			(1U << 7)
#define ALL_DCM			(CPU_DCM | IFR_DCM | PER_DCM | SMI_DCM | \
				EMI_DCM | DIS_DCM | ISP_DCM | VDE_DCM)
#define NR_DCMS			(0x8)

#define BUS_DCM_FREQ_DEFAULT	(0x0)
#define BUS_DCM_FREQ_DIV_16     (0x1)
#define BUS_DCM_FREQ_DIV_8      (0x2)
#define BUS_DCM_FREQ_DIV_4      (0x4)
#define BUS_DCM_FREQ_DIV_2      (0x8)
#define BUS_DCM_FREQ_DIV_1      (0x10)

#define BUS_DCM_AUDIO		(1)

extern void dcm_enable(unsigned int type);
extern void dcm_disable(unsigned int type);

extern void disable_cpu_dcm(void);
extern void enable_cpu_dcm(void);

extern int bus_dcm_set_freq_div(unsigned int, unsigned int);

extern void bus_dcm_enable(void);
extern void bus_dcm_disable(void);

extern void disable_infra_dcm(void);
extern void restore_infra_dcm(void);

extern void disable_peri_dcm(void);
extern void restore_peri_dcm(void);

extern void mt_dcm_init(void);

#endif
