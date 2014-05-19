/*
 * dollar_cove_charger.h: platform data struct for dollar cove charger
 *
 * Copyright (C) 2013 Intel Corporation
 * Ramakrishna Pallala <ramakrishna.pallala@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __DOLLAR_COVE_CHARGER_H__
#define __DOLLAR_COVE_CHARGER_H__

#include <linux/types.h>
#include <linux/power_supply.h>
#include <linux/power/battery_id.h>

struct dollarcove_chrg_pdata {
	char **supplied_to;
	size_t num_supplicants;
	struct power_supply_throttle *throttle_states;
	size_t num_throttle_states;
	unsigned long supported_cables;
	int otg_gpio;
	struct ps_batt_chg_prof *chg_profile;

	int max_cc;
	int max_cv;
	int def_cc;
	int def_cv;
	int def_ilim;
	int def_iterm;
	int def_max_temp;
	int def_min_temp;
};

#endif	/* __DOLLAR_COVE_CHARGER_H__ */
