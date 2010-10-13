/* Copyright (c) 2010-2011, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __SMB137B_H__
#define __SMB137B_H__

/**
 * struct smb137b_platform_data
 * structure to pass board specific information to the smb137b charger driver
 * @chgcurrent:	max current the smb137bchip can draw
 * @valid_n_gpio:		gpio to debounce insertion/removal
 * @chg_detection_config:	machine specific func to configure
 *				insertion/removal gpio line
 * @batt_mah_rating:		the battery current rating
 */
struct smb137b_platform_data {
	int valid_n_gpio;
	int (*chg_detection_config) (void);
	int batt_mah_rating;
};

void smb137b_otg_power(int on);

#endif
