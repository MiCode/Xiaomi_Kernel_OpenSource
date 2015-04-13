/*
 * intel_soc_pmic_wcove.h - Header file for whiskey cove pmic
 *
 * Copyright (C) 2013, 2014 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * Author: Jenny TC <jenny.tc@intel.com>
 */

#ifndef __INTEL_PMIC_WCOVE_H__
#define __INTEL_PMIC_WCOVE_H__

extern struct i2c_adapter *wcove_pmic_i2c_adapter;

#define WC_IRQLVL1_ADDR		0x6E02
#define WC_IRQLVL1_MASK_ADDR	0x6E0E
#define WC_PWRSRC_ADDR		0x6E03
#define WC_MPWRSRC_ADDR		0x6E0F
#define WC_SPWRSRC_ADDR		0x6E1E
#define WC_LOWBATTDET0_ADDR	0x6E23
#define WC_LOWBATTDET1_ADDR	0x6E24
#define WC_BATTDETCTRL_ADDR	0x6EF0
#define WC_VBUSDETCTRL_ADDR	0x6E25
#define WC_VDCINDETCTRL_ADDR	0x6E25
#define WC_CHGRIRQ0_ADDR	0x6E0A
#define WC_MCHGRIRQ0_ADDR	0x6E17
#define WC_SCHGRIRQ0_ADDR	0x5E1A
#define WC_CHGRCTRL0_ADDR	0x5E16
#define WC_CHGRCTRL1_ADDR	0x5E17
#define WC_CHGRCTRL2_ADDR	0x5E18
#define WC_CHGRSTATUS_ADDR	0x5E19
#define WC_CHGDISCTRL_ADDR	0x5E2F
#define WC_THRMBATZONE_ADDR	0x4F22

#define WC_THRMIRQ1_ADDR	0x6E05
#define WC_MTHRMIRQ1_ADDR	0x6E12
#define WC_STHRMIRQ1_ADDR	0x4F1A

#define WC_THRMIRQ2_ADDR	0x6E06
#define WC_MTHRMIRQ2_ADDR	0x6E13
#define WC_STHRMIRQ2_ADDR	0x4F1B

#define WC_USBPATH_ADDR		0x5E19
#define WC_USBPHYCTRL_ADDR	0x5E07
#define WC_USBIDCTRL_ADDR	0x5E05
#define WC_USBIDEN_MASK		0x01
#define WC_USBIDSTAT_ADDR	0x00FF
#define WC_USBSRCDETSTATUS_ADDR	0x5E29

#define WC_DBGUSBBC1_ADDR	0x5FE0
#define WC_DBGUSBBC2_ADDR	0x5FE1
#define WC_DBGUSBBCSTAT_ADDR	0x5FE2

#define WC_WAKESRC_ADDR		0x6E22
#define WC_WAKESRC2_ADDR	0x6EE5
#define WC_CHRTTADDR_ADDR	0x5E22
#define WC_CHRTTDATA_ADDR	0x5E23

#define WC_THRMIRQ0_ADDR	0x6E04
#define WC_MTHRMIRQ0_ADDR	0x6E0D
#define WC_STHRMIRQ0_ADDR	0x4F19
#define WC_THRMIRQ1_ADDR	0x6E05
#define WC_MTHRMIRQ1_ADDR	0x6E12
#define WC_STHRMIRQ1_ADDR	0x4F1A
#define WC_THRMIRQ2_ADDR	0x6E06
#define WC_MTHRMIRQ2_ADDR	0x6E13
#define WC_STHRMIRQ2_ADDR	0x4F1B

#define WC_THRMZN0H_ADDR	0x4F44
#define WC_THRMZN0L_ADDR	0x4F45
#define WC_THRMZN1H_ADDR	0x4F46
#define WC_THRMZN1L_ADDR	0x4F47
#define WC_THRMZN2H_ADDR	0x4F48
#define WC_THRMZN2L_ADDR	0x4F49
#define WC_THRMZN3H_ADDR	0x4F4A
#define WC_THRMZN3L_ADDR	0x4F4B
#define WC_THRMZN4H_ADDR	0x4F4C
#define WC_THRMZN4L_ADDR	0x4F4D

#endif
