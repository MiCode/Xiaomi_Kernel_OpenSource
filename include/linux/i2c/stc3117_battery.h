/*
 * STC3117 fuel gauge - Platform Device Driver
 *
 *  Copyright (C) 2011 STMicroelectronics.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __STC3117_BATTERY_H_
#define __STC3117_BATTERY_H_

struct stc311x_platform_data {
	int (*battery_online)(void);
	int (*charger_online)(void);
	int (*charger_enable)(void);
	int (*power_supply_register)(struct device *parent, struct power_supply *psy);
	void (*power_supply_unregister)(struct power_supply *psy);

	int Vmode;       /* 1=Voltage mode, 0=mixed mode */
  	int Alm_SOC;     /* SOC alm level %*/
  	int Alm_Vbat;    /* Vbat alm level mV*/
  	int CC_cnf;      /* nominal CC_cnf */
  	int VM_cnf;      /* nominal VM cnf */
	int Rint;		 /*nominal Rint*/
  	int Cnom;        /* nominal capacity in mAh */
  	int Rsense;      /* sense resistor mOhms*/
  	int RelaxCurrent; /* current for relaxation in mA (< C/20) */
  	int Adaptive;     /* 1=Adaptive mode enabled, 0=Adaptive mode disabled */
  	int CapDerating[7];   /* capacity derating in 0.1%, for temp = 60, 40, 25, 10,   0, -10 °C,-20°C */
  	int OCVValue[16];    /* OCV curve adjustment */
  	int (*ExternalTemperature) (void); /*External temperature fonction, return °C*/
  	int ForceExternalTemperature; /* 1=External temperature, 0=STC3115 temperature */
};

#endif



