/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/of.h>
#include <linux/batterydata-lib.h>

#ifdef CONFIG_OF_BATTERYDATA
/**
 * of_batterydata_read_data() - Populate battery data from the device tree
 * @container_node: pointer to the battery-data container device node
 *		containing the profile nodes.
 * @batt_data: pointer to an allocated bms_battery_data structure that the
 *		loaded profile will be written to.
 * @batt_id_uv: ADC voltage of the battery id line used to differentiate
 *		between different battery profiles. If there are multiple
 *		battery data in the device tree, the one with the closest
 *		battery id resistance will be automatically loaded.
 *
 * This routine loads the closest match battery data from device tree based on
 * the battery id reading. Then, it will try to load all the relevant data from
 * the device tree battery data profile.
 *
 * If any of the lookup table pointers are NULL, this routine will skip trying
 * to read them.
 */
int of_batterydata_read_data(struct device_node *container_node,
				struct bms_battery_data *batt_data,
				int batt_id_uv);
#else
static inline int of_batterydata_read_data(struct device_node *container_node,
				struct bms_battery_data *batt_data,
				int batt_id_uv);
{
	return -ENXIO;
}
#endif /* CONFIG_OF_QPNP */
