/*
 * Copyright (c) 2011-2015, The Linux Foundation. All rights reserved.
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
/*
 * Qualcomm TSENS Header file
 *
 */

#ifndef __MSM_TSENS_H
#define __MSM_TSENS_H

#define TSENS_MAX_SENSORS		11
#define TSENS_MTC_ZONE_LOG_SIZE		6
#define TSENS_NUM_MTC_ZONES_SUPPORT	3
#define TSENS_ZONEMASK_PARAMS		3
#define TSENS_ZONELOG_PARAMS		1
#define TSENS_MTC_ZONE_HISTORY_SIZE	3

struct tsens_device {
	uint32_t			sensor_num;
};

#if defined(CONFIG_THERMAL_TSENS8974)
/**
 * tsens_is_ready() - Clients can use this API to check if the TSENS device
 *		is ready and clients can start requesting temperature reads.
 * @return:	Returns true if device is ready else returns -EPROBE_DEFER
 *		for clients to check back after a time duration.
 */
int tsens_is_ready(void);
/**
 * tsens_tm_init_driver() - Early initialization for clients to read
 *		TSENS temperature.
 */
int __init tsens_tm_init_driver(void);
/**
 * tsens_get_hw_id_mapping() - Mapping software or sensor ID with the physical
 *		TSENS sensor. On certain cases where there are more number of
 *		controllers the sensor ID is used to map the clients software ID
 *		with the physical HW sensors used by the driver.
 * @sensors_sw_id:	Client ID.
 * @sensor_hw_num:	Sensor client ID passed by the driver. This ID is used
 *			by the driver to map it to the physical HW sensor
 *			number.
 * @return:	If the device is not present returns -EPROBE_DEFER
 *		for clients to check back after a time duration.
 *		0 on success else error code on error.
 */
int tsens_get_hw_id_mapping(int sensor_sw_id, int *sensor_hw_num);
/**
 * tsens_get_max_sensor_num() - Get the total number of active TSENS sensors.
 *		The total number received by the client is across multiple
 *		TSENS controllers if present.
 * @tsens_num_sensors: Total number of sensor result to be stored.
 */
int tsens_get_max_sensor_num(uint32_t *tsens_num_sensors);
/**
 * tsens_set_mtc_zone_sw_mask() - Mask the MTC threshold level of a zone.
 *		SW can force the MTC to stop issuing throttling commands that
 *		correspond to each MTC threshold level by writing the
 *		corresponding bit in register at any time.
 * @zone: Zone ID.
 * @th1_enable : Value corresponding to the threshold level.
 * @th2_enable : Value corresponding to the threshold level.
 */
int tsens_set_mtc_zone_sw_mask(unsigned int zone , unsigned int th1_enable,
				unsigned int th2_enable);
/**
 * tsens_get_mtc_zone_log() - Get the log of last 6 commands sent to pulse
 *		swallower of a zone.
 * zone: Zone ID
 * @zone_log: Log commands result to be stored.
 */
int tsens_get_mtc_zone_log(unsigned int zone , void *zone_log);
/**
 * tsens_mtc_reset_history_counter() - Reset history of throttling commands
 *		sent to pulse swallower. Tsens controller issues clock
 *		throttling commands to Pulse swallower to perform HW
 *		based clock throttling. Reset the history counter of a zone.
 * @zone: Zone ID.
 */
int tsens_mtc_reset_history_counter(unsigned int zone);
/**
 * tsens_get_mtc_zone_history() - Get the history of throttling commands sent
 *		to pulse swallower. Tsens controller issues clock throttling
 *		commands to Pulse swallower to perform HW based clock
 *		throttling.
 * @zone: Zone ID
 * @zone_hist: Commands history result to be stored.
 */
int tsens_get_mtc_zone_history(unsigned int zone , void *zone_hist);
/**
 * tsens_get_temp() - Obtain the TSENS temperature for the respective sensor.
 *
 * @dev:	Sensor number for which client wants the TSENS temperature
 *		reading. The ID passed by the sensor could be the sensor ID
 *		which the driver translates to internally to read the
 *		respective physical HW sensor from the controller.
 * @temp:	temperature result to be stored.
 * @return:	If the device is not present returns -EPROBE_DEFER
 *		for clients to check back after a time duration.
 *		0 on success else error code on error.
 */
int tsens_get_temp(struct tsens_device *dev, int *temp);
#else
static inline int tsens_is_ready(void)
{ return -ENXIO; }
static inline int __init tsens_tm_init_driver(void)
{ return -ENXIO; }
static inline int tsens_get_hw_id_mapping(
				int sensor_sw_id, int *sensor_hw_num)
{ return -ENXIO; }
static inline int tsens_get_max_sensor_num(uint32_t *tsens_num_sensors)
{ return -ENXIO; }
static inline int tsens_set_mtc_zone_sw_mask(unsigned int zone ,
				unsigned int th1_enable ,
				unsigned int th2_enable)
{ return -ENXIO; }
static inline int tsens_get_mtc_zone_log(unsigned int zone , void *zone_log)
{ return -ENXIO; }
static inline int tsens_mtc_reset_history_counter(unsigned int zone)
{ return -ENXIO; }
static inline int tsens_get_temp(struct tsens_device *dev,
						int *temp)
{ return -ENXIO; }
static inline int tsens_get_mtc_zone_history(unsigned int zone, void *zone_hist)
{ return -ENXIO; }
#endif

#endif /*MSM_TSENS_H */
