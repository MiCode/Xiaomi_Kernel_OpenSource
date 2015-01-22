/*
 * linux/drivers/modem_control/mcd_mdm.h
 *
 * Version 1.0
 *
 * This code includes definitions for IMC modems.
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

#ifndef _MDM_IMC_H
#define _MDM_IMC_H

int mcd_mdm_init(void *data);
int mcd_mdm_cold_boot(void *data);
int mcd_mdm_warm_reset(void *data, int rst);
int mcd_mdm_power_off(void *data);
int mcd_mdm_get_cflash_delay(void *data);
int mcd_mdm_get_wflash_delay(void *data);
int mcd_mdm_cleanup(void *data);
int mcd_mdm_cold_boot_ngff(void *data);
int mcd_mdm_power_off_ngff(void *data);
int mcd_mdm_cold_boot_2230(void *data);
int mcd_mdm_power_off_2230(void *data);

#endif				/* _MDM_IMC_H */
