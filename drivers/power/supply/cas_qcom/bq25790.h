/*
 * BQ2560x battery charging driver
 *
 * Copyright (C) 2013 Texas Instruments
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef _LINUX_BQ25790_I2C_H
#define _LINUX_BQ25790_I2C_H

#include <linux/power_supply.h>


struct bq25790_charge_param {
	int vindpm;
	int iindpm;
	int ichg;
	int vreg;
};

struct bq25790_platform_data {
	struct bq25790_charge_param dcp;
	struct bq25790_charge_param hvdcp;
	int iprechg;
	int iterm;
	int otg_volt;
	int otg_current;
	int safe_timer_en;
	int safe_timer;
	int presafe_timer;
	int vac_ovp;
	int cell_num;
	int vsys_min;
};

enum int_mask0 {
	VBUS_PRESENT_MASK	= 0x01,
	AC1_PRESENT_MASK	= 0x02,
	AC2_PRESENT_MASK	= 0x04,
	PG_MASK			= 0x08,
	POOPRSRC_MASK		= 0x10,
	WD_MASK			= 0x20,
	VINDPM_MASK		= 0x40,
	IINDPM_MASK		= 0x80,
};

enum int_mask1 {
	BC12_DONE_MASK		= 0x01,
	VBAT_PRESENT_MASK	= 0x02,
	TREG_MASK		= 0x04,
	VBUS_MASK		= 0x10,
	ICO_MASK		= 0x40,
	CHG_MASK		= 0x80,
};

enum int_mask2 {
	TOPOFF_TMR_MASK		= 0x01,
	PRECHG_TMR_MASK		= 0x02,
	TRICHG_TMR_MASK		= 0x04,
	CHG_TMR_MASK		= 0x08,
	VSYS_MASK		= 0x10,
	ADC_DONE_MASK		= 0x20,
	DPDM_DONE_MASK		= 0x40,
};

enum int_mask3 {
	TS_HOT_MASK		= 0x01,
	TS_WARM_MASK		= 0x02,
	TS_COOL_MASK		= 0x04,
	TS_COLD_MASK		= 0x08,
	VBATOTG_LOW_MASK	= 0x10,
};

enum fault_mask0 {
	VAC1_OVP_MASK		= 0x01,
	VAC2_OVP_MASK		= 0x02,
	CONV_OCP_MASK		= 0x04,
	IBAT_OCP_MASK		= 0x08,
	IBUS_OCP_MASK		= 0x10,
	VBAT_OVP_MASK		= 0x20,
	VBUS_OVP_MASK		= 0x40,
	IBAT_REG_MASK		= 0x80,
};

enum fault_mask1 {
	TSHUT_MASK		= 0x01,
	OTG_UVP_MASK		= 0x02,
	OTG_OVP_MASK		= 0x04,
	VSYS_OVP_MASK		= 0x08,
	VSYS_SHORT_MASK		= 0x10,
};

enum adc_dis_mask {
	TDIE_ADC_DIS		= 0x02,
	TS_ADC_DIS		= 0x04,
	VSYS_ADC_DIS		= 0x08,
	IBUS_ADC_DIS		= 0x10,
	IBAT_ADC_DIS		= 0x20,
	VBUS_ADC_DIS		= 0x40,
	VBAT_ADC_DIS		= 0x80,
};

enum hvdcp3_type {
	HVDCP3_NONE = 0,
	HVDCP3_CLASSA_18W,
	HVDCP3_CLASSB_27W,
};

#define BQ25790_PPS_VINDPM_MV		4800000
#define BQ25790_PPS_IINDPM_MA		2000000
#define BQ25790_PPS_FCC			2000000
#define BQ25790_HVDCP_2_VINDPM_MV	8600000
#define BQ25790_HVDCP_2_IINDPM_MA	1500000
#define BQ25790_HVDCP_2_FCC		1500000
#define BQ25790_HVDCP_A_VINDPM_MV	8400000
#define BQ25790_HVDCP_A_IINDPM_MA	2000000
#define BQ25790_HVDCP_A_FCC		2000000
#define BQ25790_HVDCP_B_VINDPM_MV	8400000
#define BQ25790_HVDCP_B_IINDPM_MA	2000000
#define BQ25790_HVDCP_B_FCC		2000000
#define BQ25790_DCP_VINDPM_MV		4600000
#define BQ25790_DCP_IINDPM_MA		1800000
#define BQ25790_DCP_FCC			1000000
#define BQ25790_SDP_VINDPM_MV		4400000
#define BQ25790_SDP_IINDPM_MA		500000
#define BQ25790_SDP_FCC			250000
#define BQ25790_CDP_IINDPM_MA		1500000
#define BQ25790_CDP_FCC			750000
#define BQ25790_FFC_TAPER_FV		8940000
#define BQ25790_FFC_FV			8960000
#define BQ25790_FV			8900000
#define BQ25790_DELTA_FV		20000
#define BQ25790_DEFUALT_FCC		500000
#define BQ25790_WLS_IINDPM_MA	1500000
#define BQ25790_WLS_FCC			3000000

#define BQ25790_MTBF_VOTER		"BQ25790_MTBF_VOTER"
#define BQ25790_PROP_VOTER		"BQ25790_PROP_VOTER"
#define BQ25790_USER_VOTER		"BQ25790_USER_VOTER"
#define BQ25790_INIT_VOTER		"BQ25790_INIT_VOTER"
#define BQ25790_JEITA_VOTER		"BQ25790_JEITA_VOTER"
#define BQ25790_FC_VOTER		"BQ25790_FC_VOTER"
#define BQ25790_PRES_VOTER		"BQ25790_PRES_VOTER"
#define BQ25790_FULL_CHK_VOTER		"BQ25790_FULL_CHK_VOTER"

#define ARTI_VBUS_ENABLE		1
#define ARTI_VBUS_DISABLE		0
#define REVERSE_VBUS_ENABLE		1
#define REVERSE_VBUS_DISABLE		0

#define BQ25790_FULL_SOC		100

#define BQ25790_MAX_ICL			3000000
#define BQ25790_MAX_FCC			3000000

#endif
