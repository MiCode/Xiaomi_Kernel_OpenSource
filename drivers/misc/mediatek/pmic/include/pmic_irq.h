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

#include <linux/platform_device.h>
#include "mtk_pmic_irq.h"

#ifndef PMIC_INT_WIDTH
#define PMIC_INT_WIDTH 16
#endif

#define PMIC_S_INT_GEN(_name)	\
	{	\
		.name =  #_name,	\
	}

struct pmic_irq_dbg_st {
	unsigned int dbg_id;
};

/* pmic irq extern functions */
extern void PMIC_EINT_SETTING(struct platform_device *pdev);
extern int pmic_irq_debug_init(struct dentry *debug_dir);
void buck_oc_detect(void);

#endif /*--PMIC_IRQ_H--*/
