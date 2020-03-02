/*
 * Copyright (C) 2018 MediaTek Inc.
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

#ifndef __PMIC_IPI_SERVICE_ID_H__
#define __PMIC_IPI_SERVICE_ID_H__


/* Generated timestamp for comparison */
#define CODEGEN_DATECODE	20160309
#define CODEGEN_TIMECODE	152446

/*  PMIC IPI service */
#define PMIC_IPI_SERVICE_NUM	5

/*  Invalid IPI service ID */
#define INVALID_PMIC_IPI_FUNC	0xFFFFFFFF

/* RESERVED: Reserved */
#define PMIC_IPI_FUNC_RESERVED_START	0x00000000
#define PMIC_IPI_FUNC_RESERVED_RANGE	0x00000100

/* PMICWRAPPER: PMICWrapper functions */
#define PMIC_IPI_FUNC_PMICWRAPPER_START	0x00000100
#define PMIC_IPI_FUNC_PMICWRAPPER_RANGE	0x00000100

/* MAIN_PMIC: Main PMIC functions */
#define PMIC_IPI_FUNC_MAIN_PMIC_START	0x00000200
#define PMIC_IPI_FUNC_MAIN_PMIC_RANGE	0x00000100

/* SUB_PMIC: Sub PMIC functions */
#define PMIC_IPI_FUNC_SUB_PMIC_START	0x00000300
#define PMIC_IPI_FUNC_SUB_PMIC_EN       0x00000301
#define PMIC_IPI_FUNC_SUB_PMIC_RANGE	0x00000100

/* EXT_POWER_XXX: Ext. PMIC functions */
#define PMIC_IPI_FUNC_EXT_POWER_XXX_START	0x00000400
#define PMIC_IPI_FUNC_EXT_POWER_XXX_RANGE	0x00000100

#define MAIN_PMIC_WRITE_REGISTER		0x00000201
#define MAIN_PMIC_READ_REGISTER			0x00000202
#define MAIN_PMIC_REGULATOR			0x00000203
#define SUB_PMIC_CTRL				0x00000204
#define MT6311_FPWM				0x00000205
#define RT5738_FPWM				0x00000206

#endif	/* __PMIC_IPI_SERVICE_ID_H__ */
