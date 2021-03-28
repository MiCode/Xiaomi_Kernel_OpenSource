/*
 *  Copyright (C) 2021 Mediatek Technology Inc.
 *  Jeff_Chang <jeff_chang@richtek.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __RT5133_EVENT_H
#define __RT5133_EVENT_H

enum RT5133_IRQ_NUM {
	RT5133_IRQ_LDO1_OC,
	RT5133_IRQ_LDO1_PGB,
	RT5133_IRQ_LDO2_OC,
	RT5133_IRQ_LDO2_PGB,
	RT5133_IRQ_LDO3_OC,
	RT5133_IRQ_LDO3_PGB,
	RT5133_IRQ_LDO4_OC,
	RT5133_IRQ_LDO4_PGB,
	RT5133_IRQ_LDO5_OC,
	RT5133_IRQ_LDO5_PGB,
	RT5133_IRQ_LDO6_OC,
	RT5133_IRQ_LDO6_PGB,
	RT5133_IRQ_LDO7_OC,
	RT5133_IRQ_LDO7_PGB,
	RT5133_IRQ_LDO8_OC,
	RT5133_IRQ_LDO8_PGB,
	RT5133_IRQ_MAX,
};

typedef void (*RT5133_IRQ_FUNC_PTR)(void);

extern void rt5133_register_interrupt_callback(enum RT5133_IRQ_NUM intno,
					       RT5133_IRQ_FUNC_PTR IRQ_FUNC_PTR);

extern void rt5133_enable_interrupt(enum RT5133_IRQ_NUM intno, int en);

#endif /* __RT5133_EVENT_H */
