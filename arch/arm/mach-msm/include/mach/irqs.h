/*
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2008-2011, Code Aurora Forum. All rights reserved.
 * Author: Brian Swetland <swetland@google.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __ASM_ARCH_MSM_IRQS_H
#define __ASM_ARCH_MSM_IRQS_H

#define MSM_IRQ_BIT(irq)     (1 << ((irq) & 31))

#if defined(CONFIG_ARCH_MSM8960) || defined(CONFIG_ARCH_APQ8064) || \
	defined(CONFIG_ARCH_MSM8930)

#ifdef CONFIG_ARCH_MSM8960
#include "irqs-8960.h"
#endif

#ifdef CONFIG_ARCH_MSM8930
#include "irqs-8930.h"
#endif

#ifdef CONFIG_ARCH_APQ8064
#include "irqs-8064.h"
#endif

/* For now, use the maximum number of interrupts until a pending GIC issue
 * is sorted out */
#define NR_MSM_IRQS 288
#define NR_GPIO_IRQS 152
#define NR_PM8921_IRQS 256
#define NR_PM8821_IRQS 64
#define NR_TABLA_IRQS 49
#define NR_GPIO_EXPANDER_IRQS 8
#define NR_BOARD_IRQS (NR_PM8921_IRQS + NR_PM8821_IRQS + \
		NR_TABLA_IRQS + NR_GPIO_EXPANDER_IRQS)
#define NR_TLMM_MSM_DIR_CONN_IRQ 8 /*Need to Verify this Count*/
#define NR_MSM_GPIOS NR_GPIO_IRQS

#else

#if defined(CONFIG_ARCH_MSMCOPPER)
#include "irqs-copper.h"
#elif defined(CONFIG_ARCH_MSM9615)
#include "irqs-9615.h"
#elif defined(CONFIG_ARCH_MSM7X30)
#include "irqs-7x30.h"
#elif defined(CONFIG_ARCH_QSD8X50)
#include "irqs-8x50.h"
#include "sirc.h"
#elif defined(CONFIG_ARCH_MSM8X60)
#include "irqs-8x60.h"
#elif defined(CONFIG_ARCH_MSM7X01A) || defined(CONFIG_ARCH_MSM7X25) \
	|| defined(CONFIG_ARCH_MSM7X27)
#include "irqs-7xxx.h"
#elif defined(CONFIG_ARCH_FSM9XXX)
#include "irqs-fsm9xxx.h"
#include "sirc.h"
#else
#error "Unknown architecture specification"
#endif

#endif

#define NR_IRQS (NR_MSM_IRQS + NR_GPIO_IRQS + NR_BOARD_IRQS)
#define MSM_GPIO_TO_INT(n) (NR_MSM_IRQS + (n))
#define FIRST_GPIO_IRQ MSM_GPIO_TO_INT(0)
#define MSM_INT_TO_REG(base, irq) (base + irq / 32)

#endif
