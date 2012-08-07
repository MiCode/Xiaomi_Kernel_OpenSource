/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#ifndef __ASM_ARCH_MSM_IRQS_9625_H
#define __ASM_ARCH_MSM_IRQS_9625_H

/* MSM ACPU Interrupt Numbers */

/*
 * 0-15:  STI/SGI (software triggered/generated interrupts)
 * 16-31: PPI (private peripheral interrupts)
 * 32+:   SPI (shared peripheral interrupts)
 */

#define GIC_SPI_START 32

#define APCC_QGICL2PERFMONIRPTREQ	(GIC_SPI_START + 1)
#define SC_SICL2PERFMONIRPTREQ		APCC_QGICL2PERFMONIRPTREQ
#define TLMM_MSM_SUMMARY_IRQ		(GIC_SPI_START + 208)

#define NR_MSM_IRQS 288
#define NR_GPIO_IRQS 88
#define NR_BOARD_IRQS 0
#define NR_TLMM_MSM_DIR_CONN_IRQ 8 /*Need to Verify this Count*/
#define NR_MSM_GPIOS NR_GPIO_IRQS

#endif
