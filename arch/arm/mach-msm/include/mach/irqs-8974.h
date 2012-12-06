/* Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
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

#ifndef __ASM_ARCH_MSM_IRQS_8974_H
#define __ASM_ARCH_MSM_IRQS_8974_H

/* MSM ACPU Interrupt Numbers */

#define INT_ARMQC_PERFMON			(GIC_PPI_START + 7)
/* PPI 15 is unused */

#define APCC_QGICL2PERFMONIRPTREQ	(GIC_SPI_START + 1)
#define SC_SICL2PERFMONIRPTREQ		APCC_QGICL2PERFMONIRPTREQ

#endif

