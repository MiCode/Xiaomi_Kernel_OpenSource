/*
 *  Copyright (C) 2020 Mediatek Technology Inc.
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

#ifndef __MT6317_EVENT_H
#define __MT6317_EVENT_H

enum MT6317_IRQ_NUM {
	MT6317_IRQ_LDO1_OC,
	MT6317_IRQ_LDO1_PGB,
	MT6317_IRQ_LDO2_OC,
	MT6317_IRQ_LDO2_PGB,
	MT6317_IRQ_LDO3_OC,
	MT6317_IRQ_LDO3_PGB,
	MT6317_IRQ_LDO4_OC,
	MT6317_IRQ_LDO4_PGB,
	MT6317_IRQ_LDO5_OC,
	MT6317_IRQ_LDO5_PGB,
	MT6317_IRQ_LDO6_OC,
	MT6317_IRQ_LDO6_PGB,
	MT6317_IRQ_LDO7_OC,
	MT6317_IRQ_LDO7_PGB,
	MT6317_IRQ_LDO8_OC,
	MT6317_IRQ_LDO8_PGB,
	MT6317_IRQ_MAX,
};

typedef void (*MT6317_IRQ_FUNC_PTR)(void);

extern void mt6317_register_interrupt_callback(enum MT6317_IRQ_NUM intno,
					       MT6317_IRQ_FUNC_PTR IRQ_FUNC_PTR);

extern void mt6317_enable_interrupt(enum MT6317_IRQ_NUM intno, int en);

#endif /* __MT6317_EVENT_H */
