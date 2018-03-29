/*
 * Copyright (C) 2016 MediaTek Inc.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __PMIC_IRQ_H
#define __PMIC_IRQ_H

#include "mt_pmic_irq.h"

#define PMIC_INTERRUPT_WIDTH 16

#define PMIC_S_INT_GEN(_name)	\
	{	\
		.name =  #_name,	\
	}

/* pmic irq extern variable */
extern int interrupts_size;
extern struct pmic_interrupts interrupts[];

/* pmic irq extern functions */
extern void PMIC_EINT_SETTING(void);
void buck_oc_detect(void);

#endif /*--PMIC_IRQ_H--*/
