/* Copyright (c) 2013-2014, 2016-2019 The Linux Foundation. All rights reserved.
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
/**
 * of_batterydata_get_best_profile() - Find matching battery data device node
 * @batterydata_container_node: pointer to the battery-data container device
 *		node containing the profile nodes.
 * @batt_id_kohm: Battery ID in KOhms for which we want to find the profile.
 * @batt_type: Battery type which we want to force load the profile.
 *
 * This routine returns a device_node pointer to the closest match battery data
 * from device tree based on the battery id reading.
 */
struct device_node *of_batterydata_get_best_profile(
		const struct device_node *batterydata_container_node,
		int batt_id_kohm, const char *batt_type);

/**
 * of_batterydata_get_best_aged_profile() - Find best aged battery profile
 * @batterydata_container_node: pointer to the battery-data container device
 *		node containing the profile nodes.
 * @batt_id_kohm: Battery ID in KOhms for which we want to find the profile.
 * @batt_age_level: Battery age level.
 * @avail_age_level: Available battery age level.
 *
 * This routine returns a device_node pointer to the closest match battery data
 * from device tree based on the battery id reading and age level.
 */
struct device_node *of_batterydata_get_best_aged_profile(
		const struct device_node *batterydata_container_node,
		int batt_id_kohm, int batt_age_level, int *avail_age_level);

/**
 * of_batterydata_get_aged_profile_count() - Gets the number of aged profiles
 * @batterydata_node: pointer to the battery-data container device
 *		node containing the profile nodes.
 * @batt_id_kohm: Battery ID in KOhms for which we want to find the profile.
 * @count: Number of aged profiles available to support SOH based profile
 * loading.
 *
 * This routine returns zero if valid number of aged profiles are available.
 */
int of_batterydata_get_aged_profile_count(
		const struct device_node *batterydata_node,
		int batt_id_kohm, int *count);

/**
 * of_batterydata_read_soh_aged_profiles() - Reads the data from aged profiles
 * @batterydata_node: pointer to the battery-data container device
 *		node containing the profile nodes.
 * @batt_id_kohm: Battery ID in KOhms for which we want to find the profile.
 * @soh_data: SOH data from the profile if it is found to be valid.
 *
 * This routine returns zero if SOH data of aged profiles is valid.
 */
int of_batterydata_read_soh_aged_profiles(
		const struct device_node *batterydata_node,
		int batt_id_kohm, struct soh_range *soh_data);
#else
static inline int of_batterydata_read_data(struct device_node *container_node,
				struct bms_battery_data *batt_data,
				int batt_id_uv)
{
	return -ENXIO;
}
static inline struct device_node *of_batterydata_get_best_profile(
		const struct device_node *batterydata_container_node,
		int batt_id_kohm, const char *batt_type)
{
	return NULL;
}
static inline struct device_node *of_batterydata_get_best_aged_profile(
		const struct device_node *batterydata_container_node,
		int batt_id_kohm, u32 batt_age_level)
{
	return NULL;
}
static inline int of_batterydata_get_aged_profile_count(
		const struct device_node *batterydata_node,
		int batt_id_kohm, int *count)
{
	return -EINVAL;
}
static inline int of_batterydata_read_soh_aged_profiles(
		const struct device_node *batterydata_node,
		int batt_id_kohm, struct soh_range *soh_data)
{
	return -EINVAL;
}
#endif /* CONFIG_OF_BATTERYDATA */
