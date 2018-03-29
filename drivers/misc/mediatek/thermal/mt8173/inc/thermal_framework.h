/*
 * Thermal Framework Driver
 *
 * Copyright (C) 2011 Texas Instruments Incorporated - http://www.ti.com/
 * Author: Dan Murphy <DMurphy@ti.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
*/

#ifndef __LINUX_THERMAL_FRAMEWORK_H__
#define __LINUX_THERMAL_FRAMEWORK_H__

#define NUM_COOLERS 10
#include <linux/seq_file.h>

struct thermal_dev;
struct thermal_domain;

/**
 * struct thermal_dev_ops  - Structure for device operation call backs
 * @get_temp: A temp sensor call back to get the current temperature.
 *		temp is reported in milli degrees.
 * @set_temp_thresh: Update the temperature sensor thresholds.  This can be used
 *		to allow the sensor to only report changes when the thresholds
 *		have been crossed.
 * @set_temp_report_rate: Update the rate at which the temperature sensor
 *		reports the temperature change.  This API should return the
*		current measurement rate that the sensor is measuring at.
 * @cool_device: The cooling agent call back to process a list of cooling agents
 * @process_temp: The governors call back for processing a domain temperature
 *
 */
struct thermal_dev_ops {
	/* Sensor call backs */
	int (*report_temp)(struct thermal_dev *);
	int (*set_temp_thresh)(struct thermal_dev *temp_sensor, int min, int max);
	int (*set_hot_event)(struct thermal_dev *temp_sensor, int hot_event);
	int (*set_temp_report_rate)(struct thermal_dev *, int rate);
	/* Cooling agent call backs */
	int (*cool_device)(struct thermal_dev *, int temp);
	/* Governor call backs */
	int (*process_temp)(struct thermal_dev *gov,
			     struct list_head *cooling_list,
			     struct thermal_dev *temp_sensor, int temp);
	int (*process_hotspot_temp)(struct thermal_dev *tdev);
	int (*process_avg_temp)(struct thermal_dev *tdev);
	int (*process_zone)(struct thermal_dev *tdev);
#ifdef CONFIG_THERMAL_FRAMEWORK_DEBUG
	/* Debugging interface */
	int (*debug_report)(struct thermal_dev *, struct seq_file *s);
	int (*register_debug_entries)(struct thermal_dev *, struct dentry *d);
#endif
};

/**
 * struct thermal_cooling_action  - Structure for each action to reduce temp.
 * @priority: This action must be taken when there is a message with cooling
 *            level / priority equals to @priority
 * @reduction: The reduction from maximum value in percentage that needs
 *             to be taken when executing this action.
 */
struct thermal_cooling_action {
	unsigned int priority;
	unsigned int reduction;
	struct list_head node;
#ifdef CONFIG_THERMAL_FRAMEWORK_DEBUG
	struct dentry *d;
#endif
};

/**
 * struct thermal_dev  - Structure for each thermal device.
 * @name: The name of the device that is registering to the framework
 * @domain_name: The temperature domain that the thermal device represents
 * @dev: Device node
 * @dev_ops: The device specific operations for the sensor, governor and cooling
 *           agents.
 * @node: The list node of the
 * @index: The index of the device created.
 * @current_temp: The current temperature reported for the specific domain
 *
 */
struct thermal_dev {
	const char *name;
	const char *domain_name;
	struct device *dev;
	struct thermal_dev_ops *dev_ops;
	struct list_head cooling_actions;
	struct list_head node;
	int current_temp;
	struct thermal_domain *domain;
};

int thermal_sensor_register(struct thermal_dev *tdev);
extern int set_bat_charging_current_limit(int limit);
extern int get_bat_charging_current_level(void);
extern int MA_len;
#endif				/* __LINUX_THERMAL_FRAMEWORK_H__ */
