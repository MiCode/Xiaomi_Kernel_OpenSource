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

#ifndef __MT_PMIC_IRQ_H
#define __MT_PMIC_IRQ_H

#define PMIC_M_INTS_GEN(adr, raw_adr, enA, maskA, interrupt)	\
	{	\
		.address =  adr,	\
		.raw_address = raw_adr,	\
		.en =  enA,	\
		.set =  (enA) + 2,	\
		.clear =  (enA) + 4,	\
		.mask =  maskA,	\
		.mask_set =	(maskA) + 2, \
		.mask_clear =  (maskA) + 4,	\
		.interrupts = interrupt,	\
	}

struct pmic_interrupt_bit {
	const char *name;
	void (*callback)(void);
	void (*oc_callback)(PMIC_IRQ_ENUM intNo, const char *);
	unsigned int times;
};

struct pmic_interrupts {
	unsigned int address;
	unsigned int raw_address;
	unsigned int en;
	unsigned int set;
	unsigned int clear;
	unsigned int mask;
	unsigned int mask_set;
	unsigned int mask_clear;
	struct pmic_interrupt_bit *interrupts;
};

#endif /*--PMIC_IRQ_H--*/
