// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _MT_PMIC_IPI_H_
#define _MT_PMIC_IPI_H_

/* SSPM v2 */
unsigned int pmic_ipi_config_interface(unsigned int RegNum, unsigned int val,
				       unsigned int MASK, unsigned int SHIFT,
				       unsigned char _unused);

#endif /* _MT_PMIC_IPI_H_*/

