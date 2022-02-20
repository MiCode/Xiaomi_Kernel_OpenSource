/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _DT_BINDINGS_QCOM_SPMI_VADC_PM6450_H
#define _DT_BINDINGS_QCOM_SPMI_VADC_PM6450_H

#ifndef PM6450_SID
#define PM6450_SID					1
#endif

/* ADC channels for PM6450_ADC for PMIC7 */
#define PM6450_ADC7_REF_GND			(PM6450_SID << 8 | 0x0)
#define PM6450_ADC7_1P25VREF			(PM6450_SID << 8 | 0x01)
#define PM6450_ADC7_VREF_VADC			(PM6450_SID << 8 | 0x02)
#define PM6450_ADC7_DIE_TEMP			(PM6450_SID << 8 | 0x03)

#define PM6450_ADC7_AMUX1_GPIO2			(PM6450_SID << 8 | 0x0a)
#define PM6450_ADC7_AMUX2_GPIO3			(PM6450_SID << 8 | 0x0b)
#define PM6450_ADC7_AMUX3_GPIO4			(PM6450_SID << 8 | 0x0c)
#define PM6450_ADC7_AMUX4_GPIO5			(PM6450_SID << 8 | 0x0d)

/* 100k pull-up2 */
#define PM6450_ADC7_AMUX1_GPIO2_100K_PU		(PM6450_SID << 8 | 0x4a)
#define PM6450_ADC7_AMUX2_GPIO3_100K_PU		(PM6450_SID << 8 | 0x4b)
#define PM6450_ADC7_AMUX3_GPIO4_100K_PU		(PM6450_SID << 8 | 0x4c)
#define PM6450_ADC7_AMUX4_GPIO5_100K_PU		(PM6450_SID << 8 | 0x4d)

#endif /* _DT_BINDINGS_QCOM_SPMI_VADC_PM6450_H */
