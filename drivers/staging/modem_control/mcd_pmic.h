/*
 * linux/drivers/modem_control/mcd_pmic.h
 *
 * Version 1.0
 *
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
 *
 * Contact: Ranquet Guillaume <guillaumex.ranquet@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 */

#ifndef _MDM_PMIC_H
#define _MDM_PMIC_H

#define PMIC_MODEMCTRL_REG_RESET 0x08
#define PMIC_MODEMCTRL_REG_SDWN_SHIFT 2
#define PMIC_MODEMCTRL_REG_MODEMOFF_SHIFT 0

#define TSDWN2OFF	1000
#define TSDWN2ON	50

int pmic_io_init(void *data);
int pmic_io_power_on_mdm(void *data);
int pmic_io_power_on_mdm2(void *data);
int pmic_io_power_on_ctp_mdm(void *data);
int pmic_io_power_off_mdm(void *data);
int pmic_io_power_off_mdm2(void *data);
int pmic_io_cleanup(void *data);
int pmic_io_get_early_pwr_on(void *data);
int pmic_io_get_early_pwr_off(void *data);
#endif
