/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2020, The Linux Foundation. All rights reserved. */

#include <dt-bindings/thermal/thermal.h>

#ifndef _DT_BINDINGS_QTI_THERMAL_H
#define _DT_BINDINGS_QTI_THERMAL_H

#define THERMAL_MAX_LIMIT	(THERMAL_NO_LIMIT - 1)
#define AGGREGATE_COEFF_VALUE	0
#define AGGREGATE_MAX_VALUE	1
#define AGGREGATE_MIN_VALUE	2

#define QMI_PA			0
#define QMI_PA_1		1
#define QMI_PA_2		2
#define QMI_QFE_PA_0		3
#define QMI_QFE_WTR_0		4
#define QMI_MODEM_TSENS		5
#define QMI_QFE_MMW_0		6
#define QMI_QFE_MMW_1		7
#define QMI_QFE_MMW_2		8
#define QMI_QFE_MMW_3		9
#define QMI_XO_THERM		10
#define QMI_QFE_PA_MDM		11
#define QMI_QFE_PA_WTR		12
#define QMI_QFE_MMW_STREAMER_0	13
#define QMI_QFE_MMW_0_MOD	14
#define QMI_QFE_MMW_1_MOD	15
#define QMI_QFE_MMW_2_MOD	16
#define QMI_QFE_MMW_3_MOD	17
#define QMI_QFE_RET_PA_0	18
#define QMI_QFE_WTR_PA_0	19
#define QMI_QFE_WTR_PA_1	20
#define QMI_QFE_WTR_PA_2	21
#define QMI_QFE_WTR_PA_3	22
#define QMI_SYS_THERM_1		23
#define QMI_SYS_THERM_2		24
#define QMI_MODEM_TSENS_1	25
#define QMI_MMW_PA1		26
#define QMI_MMW_PA2		27
#define QMI_MMW_PA3		28
#define QMI_SDR_MMW		29
#define QMI_MSM_SKIN	30
#define QMI_BEAMER_N_THERM	31
#define QMI_BEAMER_E_THERM	32
#define QMI_BEAMER_W_THERM	33
#define QMI_QFE_RET_PA0_FR1	34
#define QMI_QFE_WTR_PA0_FR1	35
#define QMI_QFE_WTR_PA1_FR1	36
#define QMI_QFE_WTR_PA2_FR1	37
#define QMI_QFE_WTR_PA3_FR1	38
#define QMI_QFE_WTR0_FR1	39

#define QMI_MODEM_INST_ID	0x0
#define QMI_ADSP_INST_ID	0x1
#define QMI_CDSP_INST_ID	0x43
#define QMI_SLPI_INST_ID	0x53
#define QMI_MODEM_NR_INST_ID	0x64

#endif
