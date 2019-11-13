/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2012-2014,2018-2019 The Linux Foundation. All rights reserved.
 */


#ifndef _DT_BINDINGS_QCOM_SPMI_VADC_H
#define _DT_BINDINGS_QCOM_SPMI_VADC_H

/* Voltage ADC channels */
#define VADC_USBIN				0x00
#define VADC_DCIN				0x01
#define VADC_VCHG_SNS				0x02
#define VADC_SPARE1_03				0x03
#define VADC_USB_ID_MV				0x04
#define VADC_VCOIN				0x05
#define VADC_VBAT_SNS				0x06
#define VADC_VSYS				0x07
#define VADC_DIE_TEMP				0x08
#define VADC_REF_625MV				0x09
#define VADC_REF_1250MV				0x0a
#define VADC_CHG_TEMP				0x0b
#define VADC_SPARE1				0x0c
#define VADC_SPARE2				0x0d
#define VADC_GND_REF				0x0e
#define VADC_VDD_VADC				0x0f

#define VADC_P_MUX1_1_1				0x10
#define VADC_P_MUX2_1_1				0x11
#define VADC_P_MUX3_1_1				0x12
#define VADC_P_MUX4_1_1				0x13
#define VADC_P_MUX5_1_1				0x14
#define VADC_P_MUX6_1_1				0x15
#define VADC_P_MUX7_1_1				0x16
#define VADC_P_MUX8_1_1				0x17
#define VADC_P_MUX9_1_1				0x18
#define VADC_P_MUX10_1_1			0x19
#define VADC_P_MUX11_1_1			0x1a
#define VADC_P_MUX12_1_1			0x1b
#define VADC_P_MUX13_1_1			0x1c
#define VADC_P_MUX14_1_1			0x1d
#define VADC_P_MUX15_1_1			0x1e
#define VADC_P_MUX16_1_1			0x1f

#define VADC_P_MUX1_1_3				0x20
#define VADC_P_MUX2_1_3				0x21
#define VADC_P_MUX3_1_3				0x22
#define VADC_P_MUX4_1_3				0x23
#define VADC_P_MUX5_1_3				0x24
#define VADC_P_MUX6_1_3				0x25
#define VADC_P_MUX7_1_3				0x26
#define VADC_P_MUX8_1_3				0x27
#define VADC_P_MUX9_1_3				0x28
#define VADC_P_MUX10_1_3			0x29
#define VADC_P_MUX11_1_3			0x2a
#define VADC_P_MUX12_1_3			0x2b
#define VADC_P_MUX13_1_3			0x2c
#define VADC_P_MUX14_1_3			0x2d
#define VADC_P_MUX15_1_3			0x2e
#define VADC_P_MUX16_1_3			0x2f

#define VADC_LR_MUX1_BAT_THERM			0x30
#define VADC_LR_MUX2_BAT_ID			0x31
#define VADC_LR_MUX3_XO_THERM			0x32
#define VADC_LR_MUX4_AMUX_THM1			0x33
#define VADC_LR_MUX5_AMUX_THM2			0x34
#define VADC_LR_MUX6_AMUX_THM3			0x35
#define VADC_LR_MUX7_HW_ID			0x36
#define VADC_LR_MUX8_AMUX_THM4			0x37
#define VADC_LR_MUX9_AMUX_THM5			0x38
#define VADC_LR_MUX10_USB_ID			0x39
#define VADC_AMUX_PU1				0x3a
#define VADC_AMUX_PU2				0x3b
#define VADC_LR_MUX3_BUF_XO_THERM		0x3c

#define VADC_LR_MUX1_PU1_BAT_THERM		0x70
#define VADC_LR_MUX2_PU1_BAT_ID			0x71
#define VADC_LR_MUX3_PU1_XO_THERM		0x72
#define VADC_LR_MUX4_PU1_AMUX_THM1		0x73
#define VADC_LR_MUX5_PU1_AMUX_THM2		0x74
#define VADC_LR_MUX6_PU1_AMUX_THM3		0x75
#define VADC_LR_MUX7_PU1_AMUX_HW_ID		0x76
#define VADC_LR_MUX8_PU1_AMUX_THM4		0x77
#define VADC_LR_MUX9_PU1_AMUX_THM5		0x78
#define VADC_LR_MUX10_PU1_AMUX_USB_ID		0x79
#define VADC_LR_MUX3_BUF_PU1_XO_THERM		0x7c

#define VADC_LR_MUX1_PU2_BAT_THERM		0xb0
#define VADC_LR_MUX2_PU2_BAT_ID			0xb1
#define VADC_LR_MUX3_PU2_XO_THERM		0xb2
#define VADC_LR_MUX4_PU2_AMUX_THM1		0xb3
#define VADC_LR_MUX5_PU2_AMUX_THM2		0xb4
#define VADC_LR_MUX6_PU2_AMUX_THM3		0xb5
#define VADC_LR_MUX7_PU2_AMUX_HW_ID		0xb6
#define VADC_LR_MUX8_PU2_AMUX_THM4		0xb7
#define VADC_LR_MUX9_PU2_AMUX_THM5		0xb8
#define VADC_LR_MUX10_PU2_AMUX_USB_ID		0xb9
#define VADC_LR_MUX3_BUF_PU2_XO_THERM		0xbc

#define VADC_LR_MUX1_PU1_PU2_BAT_THERM		0xf0
#define VADC_LR_MUX2_PU1_PU2_BAT_ID		0xf1
#define VADC_LR_MUX3_PU1_PU2_XO_THERM		0xf2
#define VADC_LR_MUX4_PU1_PU2_AMUX_THM1		0xf3
#define VADC_LR_MUX5_PU1_PU2_AMUX_THM2		0xf4
#define VADC_LR_MUX6_PU1_PU2_AMUX_THM3		0xf5
#define VADC_LR_MUX7_PU1_PU2_AMUX_HW_ID		0xf6
#define VADC_LR_MUX8_PU1_PU2_AMUX_THM4		0xf7
#define VADC_LR_MUX9_PU1_PU2_AMUX_THM5		0xf8
#define VADC_LR_MUX10_PU1_PU2_AMUX_USB_ID	0xf9
#define VADC_LR_MUX3_BUF_PU1_PU2_XO_THERM	0xfc

/* ADC channels for SPMI VADC5*/

#define ADC_REF_GND				0x00
#define ADC_1P25VREF				0x01
#define ADC_VREF_VADC				0x02
#define ADC_VREF_VADC_DIV_3			0x82
#define ADC_VPH_PWR				0x83
#define ADC_VBAT_SNS				0x84
#define ADC_VCOIN				0x85
#define ADC_DIE_TEMP				0x06
#define ADC_USB_IN_I				0x07
#define ADC_USB_IN_V_16				0x08
#define ADC_CHG_TEMP				0x09
#define ADC_BAT_THERM				0x0a
#define ADC_BAT_ID				0x0b
#define ADC_XO_THERM				0x0c
#define ADC_AMUX_THM1				0x0d
#define ADC_AMUX_THM2				0x0e
#define ADC_AMUX_THM3				0x0f
#define ADC_AMUX_THM4				0x10
#define ADC_AMUX_THM5				0x11
#define ADC_GPIO1				0x12
#define ADC_GPIO2				0x13
#define ADC_GPIO3				0x14
#define ADC_GPIO4				0x15
#define ADC_GPIO5				0x16
#define ADC_GPIO6				0x17
#define ADC_GPIO7				0x18
#define ADC_SBUx				0x99
#define ADC_MID_CHG_DIV6			0x1e
#define ADC_OFF					0xff

/* 30k pull-up1 */
#define ADC_BAT_THERM_PU1			0x2a
#define ADC_BAT_ID_PU1				0x2b
#define ADC_XO_THERM_PU1			0x2c
#define ADC_AMUX_THM1_PU1			0x2d
#define ADC_AMUX_THM2_PU1			0x2e
#define ADC_AMUX_THM3_PU1			0x2f
#define ADC_AMUX_THM4_PU1			0x30
#define ADC_AMUX_THM5_PU1			0x31
#define ADC_GPIO1_PU1				0x32
#define ADC_GPIO2_PU1				0x33
#define ADC_GPIO3_PU1				0x34
#define ADC_GPIO4_PU1				0x35
#define ADC_GPIO5_PU1				0x36
#define ADC_GPIO6_PU1				0x37
#define ADC_GPIO7_PU1				0x38
#define ADC_SBUx_PU1				0x39

/* 100k pull-up2 */
#define ADC_BAT_THERM_PU2			0x4a
#define ADC_BAT_ID_PU2				0x4b
#define ADC_XO_THERM_PU2			0x4c
#define ADC_AMUX_THM1_PU2			0x4d
#define ADC_AMUX_THM2_PU2			0x4e
#define ADC_AMUX_THM3_PU2			0x4f
#define ADC_AMUX_THM4_PU2			0x50
#define ADC_AMUX_THM5_PU2			0x51
#define ADC_GPIO1_PU2				0x52
#define ADC_GPIO2_PU2				0x53
#define ADC_GPIO3_PU2				0x54
#define ADC_GPIO4_PU2				0x55
#define ADC_GPIO5_PU2				0x56
#define ADC_GPIO6_PU2				0x57
#define ADC_GPIO7_PU2				0x58
#define ADC_SBUx_PU2				0x59

/* 400k pull-up3 */
#define ADC_BAT_THERM_PU3			0x6a
#define ADC_BAT_ID_PU3				0x6b
#define ADC_XO_THERM_PU3			0x6c
#define ADC_AMUX_THM1_PU3			0x6d
#define ADC_AMUX_THM2_PU3			0x6e
#define ADC_AMUX_THM3_PU3			0x6f
#define ADC_AMUX_THM4_PU3			0x70
#define ADC_AMUX_THM5_PU3			0x71
#define ADC_GPIO1_PU3				0x72
#define ADC_GPIO2_PU3				0x73
#define ADC_GPIO3_PU3				0x74
#define ADC_GPIO4_PU3				0x75
#define ADC_GPIO5_PU3				0x76
#define ADC_GPIO6_PU3				0x77
#define ADC_GPIO7_PU3				0x78
#define ADC_SBUx_PU3				0x79

/* 1/3 Divider */
#define ADC_GPIO1_DIV3				0x92
#define ADC_GPIO2_DIV3				0x93
#define ADC_GPIO3_DIV3				0x94
#define ADC_GPIO4_DIV3				0x95
#define ADC_GPIO5_DIV3				0x96
#define ADC_GPIO6_DIV3				0x97
#define ADC_GPIO7_DIV3				0x98
#define ADC_SBUx_DIV3				0x99

/* Current and combined current/voltage channels */
#define ADC_INT_EXT_ISENSE			0xa1
#define ADC_PARALLEL_ISENSE			0xa5
#define ADC_CUR_REPLICA_VDS			0xa7
#define ADC_CUR_SENS_BATFET_VDS_OFFSET		0xa9
#define ADC_CUR_SENS_REPLICA_VDS_OFFSET		0xab
#define ADC_EXT_SENS_OFFSET			0xad

#define ADC_INT_EXT_ISENSE_VBAT_VDATA		0xb0
#define ADC_INT_EXT_ISENSE_VBAT_IDATA		0xb1
#define ADC_EXT_ISENSE_VBAT_VDATA		0xb2
#define ADC_EXT_ISENSE_VBAT_IDATA		0xb3
#define ADC_PARALLEL_ISENSE_VBAT_VDATA		0xb4
#define ADC_PARALLEL_ISENSE_VBAT_IDATA		0xb5

#define ADC_MAX_CHANNEL				0xc0

/* VADC scale function index */
#define ADC_SCALE_DEFAULT			0x0
#define ADC_SCALE_THERM_100K_PULLUP		0x1
#define ADC_SCALE_PMIC_THERM			0x2
#define ADC_SCALE_XOTHERM			0x3
#define ADC_SCALE_PMI_CHG_TEMP			0x4
#define ADC_SCALE_HW_CALIB_DEFAULT		0x5
#define ADC_SCALE_HW_CALIB_THERM_100K_PULLUP	0x6
#define ADC_SCALE_HW_CALIB_XOTHERM		0x7
#define ADC_SCALE_HW_CALIB_PMIC_THERM		0x8
#define ADC_SCALE_HW_CALIB_CUR			0x9
#define ADC_SCALE_HW_CALIB_PM5_CHG_TEMP		0xA
#define ADC_SCALE_HW_CALIB_PM5_SMB_TEMP		0xB
#define ADC_SCALE_HW_CALIB_BATT_THERM_100K	0xC
#define ADC_SCALE_HW_CALIB_BATT_THERM_30K	0xD
#define ADC_SCALE_HW_CALIB_BATT_THERM_400K	0xE
#define ADC_SCALE_HW_CALIB_PM5_SMB1398_TEMP	0xF

#endif /* _DT_BINDINGS_QCOM_SPMI_VADC_H */
