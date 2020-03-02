/*
 * Copyright (C) 2017 MediaTek Inc.

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

#define PMIC_INT_WIDTH 16

#define PMIC_SP_IRQ_GEN(_used, _level_trig, _name)	\
	{	\
		.used = _used,	\
		.level_trig = _level_trig,	\
		.name = #_name,	\
	}

#define PMIC_SP_INTS_GEN(sp, _con_len, _sp_irqs, _top_int_bit)	\
	{	\
		.enable = MT6357_##sp##_INT_CON0,		\
		.mask = MT6357_##sp##_INT_MASK_CON0,	\
		.status = MT6357_##sp##_INT_STATUS0,	\
		.raw_status = MT6357_##sp##_INT_RAW_STATUS0,	\
		.con_len = _con_len,	\
		.int_offset = SP_##sp##_START,	\
		.sp_irqs = _sp_irqs,	\
		.top_int_bit = _top_int_bit,		\
	}

struct pmic_sp_irq {
	unsigned short used;
	unsigned short level_trig;
	const char *name;
	void (*callback)(void);
	void (*oc_callback)(enum PMIC_IRQ_ENUM intNo,
			    const char *name);
	unsigned int times;
};

struct pmic_sp_interrupt {
	unsigned int enable;
	unsigned int mask;
	unsigned int status;
	unsigned int raw_status;
	unsigned int con_len;
	unsigned int int_offset; /* start intNo of this subpack */
	struct pmic_sp_irq (*sp_irqs)[PMIC_INT_WIDTH];
	unsigned int top_int_bit;
};

#endif /*--PMIC_IRQ_H--*/
