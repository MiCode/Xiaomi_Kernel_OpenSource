/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef _MT_PMIC_IPI_H_
#define _MT_PMIC_IPI_H_

/* SSPM v2 */
unsigned int pmic_ipi_config_interface(unsigned int RegNum, unsigned int val,
				       unsigned int MASK, unsigned int SHIFT,
				       unsigned char _unused);

#endif /* _MT_PMIC_IPI_H_*/

