// SPDX-License-Identifier: GPL-2.0
/*
 *  of-thermal.c - Generic Thermal Management device tree support.
 *
 *  Copyright (C) 2013 Texas Instruments
 *  Copyright (C) 2013 Eduardo Valentin <eduardo.valentin@ti.com>
 *  Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/thermal.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/err.h>
#include <linux/export.h>
#include <linux/string.h>
#include <linux/list.h>

#include "thermal_core.h"

/***   Private data structures to represent thermal device tree data ***/

/**
 * struct __thermal_cooling_bind_param - a cooling device for a trip point
 * @cooling_device: a pointer to identify the referred cooling device
 * @min: minimum cooling state used at this trip point
 * @max: maximum cooling state used at this trip point
 */

struct __thermal_cooling_bind_param {
	struct device_node *cooling_device;
	unsigned long min;
	unsigned long max;
};

/**
 * struct __thermal_bind_param - a match between trip and cooling device
 * @tcbp: a pointer to an array of cooling devices
 * @count: number of elements in array
 * @trip_id: the trip point index
 * @usage: the percentage (from 0 to 100) of cooling contribution
 */

struct __thermal_bind_params {
	struct __thermal_cooling_bind_param *tcbp;
	unsigned int count;
	unsigned int trip_id;
	unsigned int usage;
};

#ifdef CONFIG_QTI_THERMAL
enum __sensor_aggregation {
	SENSOR_AGGREGATE_COEFF = 0,
	SENSOR_AGGREGATE_MAX,
	SENSOR_AGGREGATE_MIN,
	SENSOR_AGGREGATE_NR,
};

/* Early declaration */
struct __thermal_zone;

/**
 * struct __sensor_param - Holds individual sensor data
 * @dev: device pointer of the sensor device
 * @sensor_data: sensor driver private data passed as input argument
 * @ops: sensor driver ops
 * @trip_high: last trip high value programmed in the sensor driver
 * @trip_low: last trip low value programmed in the sensor driver
 * @lock: mutex lock acquired before updating the trip temperatures
 * @tz_list: list of thermal zones referencing this sensors
 * @tz_cnt: thermal zone count
 */
struct __sensor_param {
	struct device *dev;
	void *sensor_data;
	const struct thermal_zone_of_device_ops *ops;
	int trip_high, trip_low;
	struct mutex lock;
	unsigned int tz_cnt;
	struct __thermal_zone **tz_list;
};

/**
 * struct __thermal_zone - internal representation of a thermal zone
 * @mode: current thermal zone device mode (enabled/disabled)
 * @passive_delay: polling interval while passive cooling is activated
 * @polling_delay: zone polling interval
 * @ntrips: number of trip points
 * @trips: an array of trip points (0..ntrips - 1)
 * @num_tbps: number of thermal bind params
 * @tbps: an array of thermal bind params (0..num_tbps - 1)
 * @tzd: thermal zone device pointer for this sensor
 * @default_disable: Keep the thermal zone disabled by default
 * @coeff: coefficient array to be used to compute the temperature
 * @num_sensor: number of sensors referenced by this thermal zone
 * @sen_aggregate: sensor aggregation method to be used for multiple sensors
 * @registered_sensors: registered sensor mask
 * @senps: sensor related parameters
 */

struct __thermal_zone {
	enum thermal_device_mode mode;
	int passive_delay;
	int polling_delay;

	/* trip data */
	int ntrips;
	struct thermal_trip *trips;

	/* cooling binding data */
	int num_tbps;
	struct __thermal_bind_params *tbps;

	struct thermal_zone_device *tzd;
	bool default_disable;

	/* sensor interface */
	int *coeff;
	int num_sensor;
	enum __sensor_aggregation sen_aggregate;
	unsigned long registered_sensors;

	struct __sensor_param **senps;
};

static int of_thermal_aggregate_trip_types(struct thermal_zone_device *tz,
		struct __sensor_param *senps,
		unsigned int trip_type_mask, int *low, int *high);

/***   DT thermal zone device callbacks   ***/

static int of_thermal_get_temp(struct thermal_zone_device *tz,
			       int *temp)
{
	struct __thermal_zone *data = tz->devdata;
	int idx = 0;
	int agg_temp = 0;

	if (data->mode == THERMAL_DEVICE_DISABLED) {
		*temp = THERMAL_TEMP_INVALID;
		return 0;
	}

	for (idx = 0; idx < data->num_sensor; idx++) {
		int temp_read = 0;

		if (!data->senps[idx] ||
			!data->senps[idx]->ops->get_temp)
			return -EINVAL;

		data->senps[idx]->ops->get_temp(data->senps[idx]->sensor_data,
						&temp_read);
		switch (data->sen_aggregate) {
		case SENSOR_AGGREGATE_COEFF:
			if (idx == 0)
				agg_temp = data->coeff[data->num_sensor];
			agg_temp += temp_read * data->coeff[idx];
			break;
		case SENSOR_AGGREGATE_MAX:
			if (idx == 0)
				agg_temp = INT_MIN;
			agg_temp = (agg_temp > temp_read) ?
					agg_temp : temp_read;
			break;
		case SENSOR_AGGREGATE_MIN:
			if (idx == 0)
				agg_temp = INT_MAX;
			agg_temp = (agg_temp < temp_read) ?
					agg_temp : temp_read;
			break;
		case SENSOR_AGGREGATE_NR:
		default:
			return -EINVAL;
		}
	}
	*temp = agg_temp;

	return 0;
}

static int of_thermal_set_trips(struct thermal_zone_device *tz,
				int inp_low, int inp_high)
{
	struct __thermal_zone *data = tz->devdata;
	int high = INT_MAX, low = INT_MIN, ret = 0;

	if (!data->senps[0] || !data->senps[0]->ops->set_trips)
		return -EINVAL;

	if (data->mode == THERMAL_DEVICE_DISABLED)
		return ret;

	mutex_lock(&data->senps[0]->lock);
	of_thermal_aggregate_trip_types(tz, data->senps[0],
					GENMASK(THERMAL_TRIP_CRITICAL, 0),
					&low, &high);
	data->senps[0]->trip_low = low;
	data->senps[0]->trip_high = high;
	ret = data->senps[0]->ops->set_trips(data->senps[0]->sensor_data,
						low, high);
	mutex_unlock(&data->senps[0]->lock);

	return ret;
}
#else
/**
 * struct __thermal_zone - internal representation of a thermal zone
 * @mode: current thermal zone device mode (enabled/disabled)
 * @passive_delay: polling interval while passive cooling is activated
 * @polling_delay: zone polling interval
 * @slope: slope of the temperature adjustment curve
 * @offset: offset of the temperature adjustment curve
 * @ntrips: number of trip points
 * @trips: an array of trip points (0..ntrips - 1)
 * @num_tbps: number of thermal bind params
 * @tbps: an array of thermal bind params (0..num_tbps - 1)
 * @sensor_data: sensor private data used while reading temperature and trend
 * @ops: set of callbacks to handle the thermal zone based on DT
 */
struct __thermal_zone {
	enum thermal_device_mode mode;
	int passive_delay;
	int polling_delay;
	int slope;
	int offset;

	/* trip data */
	int ntrips;
	struct thermal_trip *trips;

	/* cooling binding data */
	int num_tbps;
	struct __thermal_bind_params *tbps;

	/* sensor interface */
	void *sensor_data;
	const struct thermal_zone_of_device_ops *ops;
};

/***   DT thermal zone device callbacks   ***/

static int of_thermal_get_temp(struct thermal_zone_device *tz,
			       int *temp)
{
	struct __thermal_zone *data = tz->devdata;

	if (!data->ops->get_temp)
		return -EINVAL;

	return data->ops->get_temp(data->sensor_data, temp);
}

static int of_thermal_set_trips(struct thermal_zone_device *tz,
				int low, int high)
{
	struct __thermal_zone *data = tz->devdata;

	if (!data->ops || !data->ops->set_trips)
		return -EINVAL;

	return data->ops->set_trips(data->sensor_data, low, high);
}
#endif
/**
 * of_thermal_get_ntrips - function to export number of available trip
 *			   points.
 * @tz: pointer to a thermal zone
 *
 * This function is a globally visible wrapper to get number of trip points
 * stored in the local struct __thermal_zone
 *
 * Return: number of available trip points, -ENODEV when data not available
 */
int of_thermal_get_ntrips(struct thermal_zone_device *tz)
{
	struct __thermal_zone *data = tz->devdata;

	if (!data || IS_ERR(data))
		return -ENODEV;

	return data->ntrips;
}
EXPORT_SYMBOL_GPL(of_thermal_get_ntrips);

/**
 * of_thermal_is_trip_valid - function to check if trip point is valid
 *
 * @tz:	pointer to a thermal zone
 * @trip:	trip point to evaluate
 *
 * This function is responsible for checking if passed trip point is valid
 *
 * Return: true if trip point is valid, false otherwise
 */
bool of_thermal_is_trip_valid(struct thermal_zone_device *tz, int trip)
{
	struct __thermal_zone *data = tz->devdata;

	if (!data || trip >= data->ntrips || trip < 0)
		return false;

	return true;
}
EXPORT_SYMBOL_GPL(of_thermal_is_trip_valid);

/**
 * of_thermal_get_trip_points - function to get access to a globally exported
 *				trip points
 *
 * @tz:	pointer to a thermal zone
 *
 * This function provides a pointer to trip points table
 *
 * Return: pointer to trip points table, NULL otherwise
 */
const struct thermal_trip *
of_thermal_get_trip_points(struct thermal_zone_device *tz)
{
	struct __thermal_zone *data = tz->devdata;

	if (!data)
		return NULL;

	return data->trips;
}
EXPORT_SYMBOL_GPL(of_thermal_get_trip_points);

/**
 * of_thermal_set_emul_temp - function to set emulated temperature
 *
 * @tz:	pointer to a thermal zone
 * @temp:	temperature to set
 *
 * This function gives the ability to set emulated value of temperature,
 * which is handy for debugging
 *
 * Return: zero on success, error code otherwise
 */
#ifdef CONFIG_QTI_THERMAL
static int of_thermal_set_emul_temp(struct thermal_zone_device *tz,
				    int temp)
{
	struct __thermal_zone *data = tz->devdata;

	if (!data->senps[0] || !data->senps[0]->ops->set_emul_temp)
		return -EINVAL;

	return data->senps[0]->ops->set_emul_temp(data->senps[0]->sensor_data,
						temp);
}

static int of_thermal_get_trend(struct thermal_zone_device *tz, int trip,
				enum thermal_trend *trend)
{
	struct __thermal_zone *data = tz->devdata;

	if (!data->senps[0] || !data->senps[0]->ops->get_trend)
		return -EINVAL;

	return data->senps[0]->ops->get_trend(data->senps[0]->sensor_data,
					   trip, trend);
}
#else
static int of_thermal_set_emul_temp(struct thermal_zone_device *tz,
				    int temp)
{
	struct __thermal_zone *data = tz->devdata;

	return data->ops->set_emul_temp(data->sensor_data, temp);
}

static int of_thermal_get_trend(struct thermal_zone_device *tz, int trip,
				enum thermal_trend *trend)
{
	struct __thermal_zone *data = tz->devdata;

	if (!data->ops->get_trend)
		return -EINVAL;

	return data->ops->get_trend(data->sensor_data, trip, trend);
}
#endif

static int of_thermal_bind(struct thermal_zone_device *thermal,
			   struct thermal_cooling_device *cdev)
{
	struct __thermal_zone *data = thermal->devdata;
	struct __thermal_bind_params *tbp;
	struct __thermal_cooling_bind_param *tcbp;
	int i, j;

	if (!data || IS_ERR(data))
		return -ENODEV;

	/* find where to bind */
	for (i = 0; i < data->num_tbps; i++) {
		tbp = data->tbps + i;

		for (j = 0; j < tbp->count; j++) {
			tcbp = tbp->tcbp + j;

			if (tcbp->cooling_device == cdev->np) {
				int ret;

				ret = thermal_zone_bind_cooling_device(thermal,
						tbp->trip_id, cdev,
						tcbp->max,
						tcbp->min,
						tbp->usage);
				if (ret)
					return ret;
			}
		}
	}

	return 0;
}

static int of_thermal_unbind(struct thermal_zone_device *thermal,
			     struct thermal_cooling_device *cdev)
{
	struct __thermal_zone *data = thermal->devdata;
	struct __thermal_bind_params *tbp;
	struct __thermal_cooling_bind_param *tcbp;
	int i, j;

	if (!data || IS_ERR(data))
		return -ENODEV;

	/* find where to unbind */
	for (i = 0; i < data->num_tbps; i++) {
		tbp = data->tbps + i;

		for (j = 0; j < tbp->count; j++) {
			tcbp = tbp->tcbp + j;

			if (tcbp->cooling_device == cdev->np) {
				int ret;

				ret = thermal_zone_unbind_cooling_device(thermal,
							tbp->trip_id, cdev);
				if (ret)
					return ret;
			}
		}
	}

	return 0;
}

static int of_thermal_get_mode(struct thermal_zone_device *tz,
			       enum thermal_device_mode *mode)
{
	struct __thermal_zone *data = tz->devdata;

	*mode = data->mode;

	return 0;
}

static int of_thermal_set_mode(struct thermal_zone_device *tz,
			       enum thermal_device_mode mode)
{
	struct __thermal_zone *data = tz->devdata;

	mutex_lock(&tz->lock);

	if (mode == THERMAL_DEVICE_ENABLED) {
#ifdef CONFIG_QTI_THERMAL
		if (!tz->polling_delay)
			tz->polling_delay = data->polling_delay;
		else
			data->polling_delay = tz->polling_delay;
		if (!tz->passive_delay)
			tz->passive_delay = data->passive_delay;
		else
			data->passive_delay = tz->passive_delay;
#else
		tz->polling_delay = data->polling_delay;
		tz->passive_delay = data->passive_delay;
#endif
	} else {
		tz->polling_delay = 0;
		tz->passive_delay = 0;
	}

	mutex_unlock(&tz->lock);

	data->mode = mode;
	thermal_zone_device_update(tz, THERMAL_EVENT_UNSPECIFIED);

	return 0;
}

static int of_thermal_get_trip_type(struct thermal_zone_device *tz, int trip,
				    enum thermal_trip_type *type)
{
	struct __thermal_zone *data = tz->devdata;

	if (trip >= data->ntrips || trip < 0)
		return -EDOM;

	*type = data->trips[trip].type;

	return 0;
}

static int of_thermal_get_trip_temp(struct thermal_zone_device *tz, int trip,
				    int *temp)
{
	struct __thermal_zone *data = tz->devdata;

	if (trip >= data->ntrips || trip < 0)
		return -EDOM;

	*temp = data->trips[trip].temperature;

	return 0;
}

#ifdef CONFIG_QTI_THERMAL
static int of_thermal_set_trip_temp(struct thermal_zone_device *tz, int trip,
				    int temp)
{
	struct __thermal_zone *data = tz->devdata;

	if (trip >= data->ntrips || trip < 0)
		return -EDOM;

	if (data->num_sensor == 1 && data->senps[0] &&
			data->senps[0]->ops->set_trip_temp) {
		int ret;

		ret = data->senps[0]->ops->set_trip_temp(
				data->senps[0]->sensor_data, trip, temp);
		if (ret)
			return ret;
	}

	/* thermal framework should take care of data->mask & (1 << trip) */
	data->trips[trip].temperature = temp;

	return 0;
}
#else
static int of_thermal_set_trip_temp(struct thermal_zone_device *tz, int trip,
				    int temp)
{
	struct __thermal_zone *data = tz->devdata;

	if (trip >= data->ntrips || trip < 0)
		return -EDOM;

	if (data->ops->set_trip_temp) {
		int ret;

		ret = data->ops->set_trip_temp(data->sensor_data, trip, temp);
		if (ret)
			return ret;
	}

	/* thermal framework should take care of data->mask & (1 << trip) */
	data->trips[trip].temperature = temp;

	return 0;
}
#endif

static int of_thermal_get_trip_hyst(struct thermal_zone_device *tz, int trip,
				    int *hyst)
{
	struct __thermal_zone *data = tz->devdata;

	if (trip >= data->ntrips || trip < 0)
		return -EDOM;

	*hyst = data->trips[trip].hysteresis;

	return 0;
}

static int of_thermal_set_trip_hyst(struct thermal_zone_device *tz, int trip,
				    int hyst)
{
	struct __thermal_zone *data = tz->devdata;

	if (trip >= data->ntrips || trip < 0)
		return -EDOM;

	/* thermal framework should take care of data->mask & (1 << trip) */
	data->trips[trip].hysteresis = hyst;

	return 0;
}

static int of_thermal_get_crit_temp(struct thermal_zone_device *tz,
				    int *temp)
{
	struct __thermal_zone *data = tz->devdata;
	int i;

	for (i = 0; i < data->ntrips; i++)
		if (data->trips[i].type == THERMAL_TRIP_CRITICAL) {
			*temp = data->trips[i].temperature;
			return 0;
		}

	return -EINVAL;
}

#ifdef CONFIG_QTI_THERMAL
static int of_thermal_aggregate_trip_types(struct thermal_zone_device *tz,
		struct __sensor_param *senps, unsigned int trip_type_mask,
		int *low, int *high)
{
	int min = INT_MIN;
	int max = INT_MAX;
	int tt, th, trip, idx;
	int temp = tz->temperature;
	struct thermal_zone_device *zone = NULL;
	struct __thermal_zone *data = NULL;
	enum thermal_trip_type type = 0;

	for (idx = 0; idx < senps->tz_cnt; idx++) {
		data = senps->tz_list[idx];
		zone = data->tzd;
		if (data->mode == THERMAL_DEVICE_DISABLED
			|| data->num_sensor > 1)
			continue;
		for (trip = 0; trip < data->ntrips; trip++) {
			of_thermal_get_trip_type(zone, trip, &type);
			if (!(BIT(type) & trip_type_mask))
				continue;

			tt = data->trips[trip].temperature;
			if (tt > temp && tt < max)
				max = tt;
			th = tt - data->trips[trip].hysteresis;
			if (th < temp && th > min)
				min = th;
		}
	}

	*high = max;
	*low = min;

	return 0;
}

static bool of_thermal_is_trips_triggered(struct thermal_zone_device *tz,
		int temp)
{
	int tt, th, trip, last_temp;
	struct __thermal_zone *data = tz->devdata;
	bool triggered = false;

	mutex_lock(&tz->lock);
	last_temp = tz->temperature;
	for (trip = 0; trip < data->ntrips; trip++) {
		tt = data->trips[trip].temperature;
		if (temp >= tt && last_temp < tt) {
			triggered = true;
			break;
		}
		th = tt - data->trips[trip].hysteresis;
		if (temp <= th && last_temp > th) {
			triggered = true;
			break;
		}
	}
	mutex_unlock(&tz->lock);

	return triggered;
}

static int find_sensor_index(struct device *dev,
			struct __thermal_zone *tz)
{
	int idx = 0;

	for (idx = 0; idx < tz->num_sensor; idx++) {
		if (tz->senps[idx]->dev == dev)
			break;
	}
	if (idx >= tz->num_sensor) {
		dev_err(dev, "No device pointer match with the zone:%s\n",
				tz->tzd->type);
		idx = -ENODEV;
	}

	return idx;
}

/*
 * of_thermal_aggregate_trip - aggregate trip temperatures across sibling
 *				thermal zones.
 * @dev: a valid struct device pointer of a sensor device. Must contain
 *       a valid .of_node, for the sensor node.
 * @tzd: pointer to the primary thermal zone.
 * @type: the thermal trip type to be aggregated upon
 * @low: the low trip threshold which the most lesser than the @temp
 * @high: the high trip threshold which is the least greater than the @temp
 */
int of_thermal_aggregate_trip(struct device *dev,
				struct thermal_zone_device *tzd,
				enum thermal_trip_type type,
				int *low, int *high)
{
	struct __thermal_zone *tz = tzd->devdata;
	int idx = 0;

	idx = find_sensor_index(dev, tz);
	if (idx < 0)
		return idx;

	if (type <= THERMAL_TRIP_CRITICAL)
		return of_thermal_aggregate_trip_types(tzd, tz->senps[idx],
							BIT(type), low, high);

	return -EINVAL;
}
EXPORT_SYMBOL(of_thermal_aggregate_trip);

static void handle_thermal_trip(struct device *dev,
		struct thermal_zone_device *tzd,
		bool temp_valid, int trip_temp)
{
	struct thermal_zone_device *zone;
	struct __thermal_zone *data = tzd->devdata;
	int idx = 0;
	struct __sensor_param *sens_param = NULL;
	bool notify = false;
	unsigned long tz_status_mask = 0;

	idx = find_sensor_index(dev, data);
	if (idx < 0)
		return;
	sens_param = data->senps[idx];

	for (idx = 0; idx < sens_param->tz_cnt; idx++) {
		data = sens_param->tz_list[idx];
		zone = data->tzd;
		if (data->mode == THERMAL_DEVICE_DISABLED ||
			data->num_sensor > 1)
			continue;
		if (!temp_valid) {
			thermal_zone_device_update(zone,
				THERMAL_EVENT_UNSPECIFIED);
		} else {
			set_bit(idx, &tz_status_mask);
			if (!of_thermal_is_trips_triggered(zone, trip_temp))
				continue;
			notify = true;
			thermal_zone_device_update_temp(zone,
				THERMAL_EVENT_UNSPECIFIED, trip_temp);
		}
	}

	/*
	 * It is better to notify at least one thermal zone if trip is violated
	 * for none.
	 */
	if (temp_valid && !notify) {
		idx = find_first_bit(&tz_status_mask, sens_param->tz_cnt);
		if (idx < sens_param->tz_cnt) {
			data = sens_param->tz_list[idx];
			zone = data->tzd;
			thermal_zone_device_update_temp(zone,
				THERMAL_EVENT_UNSPECIFIED, trip_temp);
		}
	}
}

/*
 * of_thermal_handle_trip_temp - Handle thermal trip from sensors
 *
 * @dev: a valid struct device pointer of a sensor device. Must contain
 *       a valid .of_node, for the sensor node.
 * @tz: pointer to the primary thermal zone.
 * @trip_temp: The temperature
 */
void of_thermal_handle_trip_temp(struct device *dev,
		struct thermal_zone_device *tz,
		int trip_temp)
{
	return handle_thermal_trip(dev, tz, true, trip_temp);
}
EXPORT_SYMBOL(of_thermal_handle_trip_temp);

/*
 * of_thermal_handle_trip - Handle thermal trip from sensors
 *
 * @dev: a valid struct device pointer of a sensor device. Must contain
 *       a valid .of_node, for the sensor node.
 * @tz: pointer to the primary thermal zone.
 */
void of_thermal_handle_trip(struct device *dev,
		struct thermal_zone_device *tz)
{
	return handle_thermal_trip(dev, tz, false, 0);
}
EXPORT_SYMBOL(of_thermal_handle_trip);
#endif

static struct thermal_zone_device_ops of_thermal_ops = {
	.get_mode = of_thermal_get_mode,
	.set_mode = of_thermal_set_mode,

	.get_trip_type = of_thermal_get_trip_type,
	.get_trip_temp = of_thermal_get_trip_temp,
	.set_trip_temp = of_thermal_set_trip_temp,
	.get_trip_hyst = of_thermal_get_trip_hyst,
	.set_trip_hyst = of_thermal_set_trip_hyst,
	.get_crit_temp = of_thermal_get_crit_temp,

	.bind = of_thermal_bind,
	.unbind = of_thermal_unbind,
};

/***   sensor API   ***/
#ifdef CONFIG_QTI_THERMAL
static struct thermal_zone_device *
thermal_zone_of_add_sensor(struct device_node *zone,
			   struct device_node *sensor,
			   struct __sensor_param *sens_param, int idx)
{
	struct thermal_zone_device *tzd;
	struct __thermal_zone *tz;
	struct __thermal_zone **tz_list_new;

	tzd = thermal_zone_get_zone_by_name(zone->name);
	if (IS_ERR(tzd))
		return ERR_PTR(-EPROBE_DEFER);

	tz = tzd->devdata;

	if (!sens_param->ops)
		return ERR_PTR(-EINVAL);

	mutex_lock(&tzd->lock);
	tz->senps[idx] = sens_param;
	bitmap_set(&tz->registered_sensors, idx, 1);
	sens_param->tz_cnt++;
	tz_list_new = krealloc(sens_param->tz_list,
			sens_param->tz_cnt * sizeof(struct __thermal_zone *),
			GFP_KERNEL);
	if (!tz_list_new)
		goto add_sensor_exit;
	sens_param->tz_list = tz_list_new;
	sens_param->tz_list[sens_param->tz_cnt - 1] = tz;

	if (bitmap_weight(&tz->registered_sensors, tz->num_sensor)
			< tz->num_sensor) {
		goto add_sensor_exit;
	}

	tzd->ops->get_temp = of_thermal_get_temp;
	tzd->ops->get_trend = of_thermal_get_trend;

	/*
	 * The thermal zone core will calculate the window if they have set the
	 * optional set_trips pointer.
	 */
	if (tz->num_sensor == 1 && sens_param->ops->set_trips)
		tzd->ops->set_trips = of_thermal_set_trips;

	if (sens_param->ops->set_emul_temp)
		tzd->ops->set_emul_temp = of_thermal_set_emul_temp;

add_sensor_exit:
	mutex_unlock(&tzd->lock);

	return tzd;
}

/**
 * thermal_zone_of_sensor_register - registers a sensor to a DT thermal zone
 * @dev: a valid struct device pointer of a sensor device. Must contain
 *       a valid .of_node, for the sensor node.
 * @sensor_id: a sensor identifier, in case the sensor IP has more
 *             than one sensors
 * @data: a private pointer (owned by the caller) that will be passed
 *        back, when a temperature reading is needed.
 * @ops: struct thermal_zone_of_device_ops *. Must contain at least .get_temp.
 *
 * This function will search the list of thermal zones described in device
 * tree and look for the zone that refer to the sensor device pointed by
 * @dev->of_node as temperature providers. For the zone pointing to the
 * sensor node, the sensor will be added to the DT thermal zone device.
 *
 * The thermal zone temperature is provided by the @get_temp function
 * pointer. When called, it will have the private pointer @data back.
 *
 * The thermal zone temperature trend is provided by the @get_trend function
 * pointer. When called, it will have the private pointer @data back.
 *
 * Return: On success returns a valid struct thermal_zone_device,
 * otherwise, it returns a corresponding ERR_PTR(). Incase there are multiple
 * thermal zones referencing the same sensor, the return value will be
 * thermal_zone_device pointer of the first thermal zone. Caller must
 * check the return value with help of IS_ERR() helper.
 */
struct thermal_zone_device *
thermal_zone_of_sensor_register(struct device *dev, int sensor_id, void *data,
				const struct thermal_zone_of_device_ops *ops)
{
	struct device_node *np, *child, *sensor_np;
	struct thermal_zone_device *tzd = ERR_PTR(-ENODEV);
	struct __sensor_param *sens_param = NULL;

	np = of_find_node_by_name(NULL, "thermal-zones");
	if (!np)
		return ERR_PTR(-ENODEV);

	if (!dev || !dev->of_node) {
		of_node_put(np);
		return ERR_PTR(-EINVAL);
	}

	sens_param = devm_kzalloc(dev, sizeof(*sens_param), GFP_KERNEL);
	if (!sens_param) {
		of_node_put(np);
		return ERR_PTR(-ENOMEM);
	}
	sens_param->dev = dev;
	sens_param->sensor_data = data;
	sens_param->ops = ops;
	sens_param->trip_high = INT_MAX;
	sens_param->trip_low = INT_MIN;
	mutex_init(&sens_param->lock);
	sensor_np = of_node_get(dev->of_node);

	for_each_available_child_of_node(np, child) {
		struct of_phandle_args sensor_specs;
		int ret, id, idx, count;
		struct __thermal_zone *tz;

		count = of_count_phandle_with_args(child, "thermal-sensors",
						"#thermal-sensor-cells");
		for (idx = 0; idx < count; idx++) {
			ret = of_parse_phandle_with_args(child,
						"thermal-sensors",
						"#thermal-sensor-cells",
						idx, &sensor_specs);
			if (ret)
				continue;

			if (sensor_specs.args_count >= 1) {
				id = sensor_specs.args[0];
				WARN(sensor_specs.args_count > 1,
				     "%pKOFn: too many cells in sensor specifier %d\n",
				     sensor_specs.np, sensor_specs.args_count);
			} else {
				id = 0;
			}

			if (sensor_specs.np == sensor_np && id == sensor_id) {
				tzd = thermal_zone_of_add_sensor(child,
							sensor_np, sens_param,
							idx);
				if (!IS_ERR(tzd)) {
					tz = tzd->devdata;
					if ((bitmap_weight(
						&tz->registered_sensors,
						tz->num_sensor) ==
						tz->num_sensor) &&
						!tz->default_disable) {
						tzd->ops->set_mode(tzd,
							THERMAL_DEVICE_ENABLED);
						}
				}
			}
			of_node_put(sensor_specs.np);
		}
	}
	of_node_put(sensor_np);
	of_node_put(np);

	if (!sens_param->tz_cnt || !sens_param->tz_list) {
		devm_kfree(dev, sens_param);
		return ERR_PTR(-ENODEV);
	}
	return sens_param->tz_list[0]->tzd;
}
EXPORT_SYMBOL(thermal_zone_of_sensor_register);

/**
 * thermal_zone_of_sensor_unregister - unregisters a sensor from a DT
 *					thermal zone
 * @dev: a valid struct device pointer of a sensor device. Must contain
 *       a valid .of_node, for the sensor node.
 * @tzd: a pointer to struct thermal_zone_device where the sensor is registered.
 *
 * This function removes the sensor callbacks and private data from the
 * thermal zone device registered with thermal_zone_of_sensor_register()
 * API. It will also silent the zone by remove the .get_temp() and .get_trend()
 * thermal zone device callbacks.
 */
void thermal_zone_of_sensor_unregister(struct device *dev,
				       struct thermal_zone_device *tzd)
{
	struct __thermal_zone *tz, *pos;
	struct thermal_zone_device *pos_tzd;
	struct __sensor_param *sens_param = NULL;
	int idx = 0, idy = 0;

	if (!dev || !tzd || !tzd->devdata)
		return;

	tz = tzd->devdata;

	/* no __thermal_zone, nothing to be done */
	if (!tz)
		return;

	idx = find_sensor_index(dev, tz);
	if (idx < 0)
		return;
	sens_param = tz->senps[idx];

	for (idx = 0; idx < sens_param->tz_cnt; idx++) {
		pos = sens_param->tz_list[idx];
		pos_tzd = pos->tzd;

		idy = find_sensor_index(dev, pos);
		if (idy < 0)
			continue;
		pos_tzd->ops->set_mode(pos_tzd, THERMAL_DEVICE_DISABLED);
		mutex_lock(&pos_tzd->lock);
		bitmap_clear(&pos->registered_sensors, idy, 1);
		pos->senps[idy] = NULL;
		if (bitmap_weight(&pos->registered_sensors, pos->num_sensor)) {
			mutex_unlock(&pos_tzd->lock);
			continue;
		}
		pos_tzd->ops->get_temp = NULL;
		pos_tzd->ops->get_trend = NULL;
		pos_tzd->ops->set_emul_temp = NULL;
		mutex_unlock(&pos_tzd->lock);
	}
	kfree(sens_param->tz_list);
}
EXPORT_SYMBOL(thermal_zone_of_sensor_unregister);
#else
static struct thermal_zone_device *
thermal_zone_of_add_sensor(struct device_node *zone,
			   struct device_node *sensor, void *data,
			   const struct thermal_zone_of_device_ops *ops)
{
	struct thermal_zone_device *tzd;
	struct __thermal_zone *tz;

	tzd = thermal_zone_get_zone_by_name(zone->name);
	if (IS_ERR(tzd))
		return ERR_PTR(-EPROBE_DEFER);

	tz = tzd->devdata;

	if (!ops)
		return ERR_PTR(-EINVAL);

	mutex_lock(&tzd->lock);
	tz->ops = ops;
	tz->sensor_data = data;

	tzd->ops->get_temp = of_thermal_get_temp;
	tzd->ops->get_trend = of_thermal_get_trend;

	/*
	 * The thermal zone core will calculate the window if they have set the
	 * optional set_trips pointer.
	 */
	if (ops->set_trips)
		tzd->ops->set_trips = of_thermal_set_trips;

	if (ops->set_emul_temp)
		tzd->ops->set_emul_temp = of_thermal_set_emul_temp;

	mutex_unlock(&tzd->lock);

	return tzd;
}

/**
 * thermal_zone_of_sensor_register - registers a sensor to a DT thermal zone
 * @dev: a valid struct device pointer of a sensor device. Must contain
 *       a valid .of_node, for the sensor node.
 * @sensor_id: a sensor identifier, in case the sensor IP has more
 *             than one sensors
 * @data: a private pointer (owned by the caller) that will be passed
 *        back, when a temperature reading is needed.
 * @ops: struct thermal_zone_of_device_ops *. Must contain at least .get_temp.
 *
 * This function will search the list of thermal zones described in device
 * tree and look for the zone that refer to the sensor device pointed by
 * @dev->of_node as temperature providers. For the zone pointing to the
 * sensor node, the sensor will be added to the DT thermal zone device.
 *
 * The thermal zone temperature is provided by the @get_temp function
 * pointer. When called, it will have the private pointer @data back.
 *
 * The thermal zone temperature trend is provided by the @get_trend function
 * pointer. When called, it will have the private pointer @data back.
 *
 * TODO:
 * 01 - This function must enqueue the new sensor instead of using
 * it as the only source of temperature values.
 *
 * 02 - There must be a way to match the sensor with all thermal zones
 * that refer to it.
 *
 * Return: On success returns a valid struct thermal_zone_device,
 * otherwise, it returns a corresponding ERR_PTR(). Caller must
 * check the return value with help of IS_ERR() helper.
 */
struct thermal_zone_device *
thermal_zone_of_sensor_register(struct device *dev, int sensor_id, void *data,
				const struct thermal_zone_of_device_ops *ops)
{
	struct device_node *np, *child, *sensor_np;
	struct thermal_zone_device *tzd = ERR_PTR(-ENODEV);

	np = of_find_node_by_name(NULL, "thermal-zones");
	if (!np)
		return ERR_PTR(-ENODEV);

	if (!dev || !dev->of_node) {
		of_node_put(np);
		return ERR_PTR(-EINVAL);
	}

	sensor_np = of_node_get(dev->of_node);

	for_each_available_child_of_node(np, child) {
		struct of_phandle_args sensor_specs;
		int ret, id;

		/* For now, thermal framework supports only 1 sensor per zone */
		ret = of_parse_phandle_with_args(child, "thermal-sensors",
						 "#thermal-sensor-cells",
						 0, &sensor_specs);
		if (ret)
			continue;

		if (sensor_specs.args_count >= 1) {
			id = sensor_specs.args[0];
			WARN(sensor_specs.args_count > 1,
			     "%pKOFn: too many cells in sensor specifier %d\n",
			     sensor_specs.np, sensor_specs.args_count);
		} else {
			id = 0;
		}

		if (sensor_specs.np == sensor_np && id == sensor_id) {
			tzd = thermal_zone_of_add_sensor(child, sensor_np,
							 data, ops);
			if (!IS_ERR(tzd))
				tzd->ops->set_mode(tzd, THERMAL_DEVICE_ENABLED);

			of_node_put(sensor_specs.np);
			of_node_put(child);
			goto exit;
		}
		of_node_put(sensor_specs.np);
	}
exit:
	of_node_put(sensor_np);
	of_node_put(np);

	return tzd;
}
EXPORT_SYMBOL_GPL(thermal_zone_of_sensor_register);

/**
 * thermal_zone_of_sensor_unregister - unregisters a sensor from a DT thermal zone
 * @dev: a valid struct device pointer of a sensor device. Must contain
 *       a valid .of_node, for the sensor node.
 * @tzd: a pointer to struct thermal_zone_device where the sensor is registered.
 *
 * This function removes the sensor callbacks and private data from the
 * thermal zone device registered with thermal_zone_of_sensor_register()
 * API. It will also silent the zone by remove the .get_temp() and .get_trend()
 * thermal zone device callbacks.
 *
 * TODO: When the support to several sensors per zone is added, this
 * function must search the sensor list based on @dev parameter.
 *
 */
void thermal_zone_of_sensor_unregister(struct device *dev,
				       struct thermal_zone_device *tzd)
{
	struct __thermal_zone *tz;

	if (!dev || !tzd || !tzd->devdata)
		return;

	tz = tzd->devdata;

	/* no __thermal_zone, nothing to be done */
	if (!tz)
		return;

	mutex_lock(&tzd->lock);
	tzd->ops->get_temp = NULL;
	tzd->ops->get_trend = NULL;
	tzd->ops->set_emul_temp = NULL;

	tz->ops = NULL;
	tz->sensor_data = NULL;
	mutex_unlock(&tzd->lock);
}
EXPORT_SYMBOL_GPL(thermal_zone_of_sensor_unregister);
#endif

static void devm_thermal_zone_of_sensor_release(struct device *dev, void *res)
{
	thermal_zone_of_sensor_unregister(dev,
					  *(struct thermal_zone_device **)res);
}

static int devm_thermal_zone_of_sensor_match(struct device *dev, void *res,
					     void *data)
{
	struct thermal_zone_device **r = res;

	if (WARN_ON(!r || !*r))
		return 0;

	return *r == data;
}

/**
 * devm_thermal_zone_of_sensor_register - Resource managed version of
 *				thermal_zone_of_sensor_register()
 * @dev: a valid struct device pointer of a sensor device. Must contain
 *       a valid .of_node, for the sensor node.
 * @sensor_id: a sensor identifier, in case the sensor IP has more
 *	       than one sensors
 * @data: a private pointer (owned by the caller) that will be passed
 *	  back, when a temperature reading is needed.
 * @ops: struct thermal_zone_of_device_ops *. Must contain at least .get_temp.
 *
 * Refer thermal_zone_of_sensor_register() for more details.
 *
 * Return: On success returns a valid struct thermal_zone_device,
 * otherwise, it returns a corresponding ERR_PTR(). Caller must
 * check the return value with help of IS_ERR() helper.
 * Registered thermal_zone_device device will automatically be
 * released when device is unbounded.
 */
struct thermal_zone_device *devm_thermal_zone_of_sensor_register(
	struct device *dev, int sensor_id,
	void *data, const struct thermal_zone_of_device_ops *ops)
{
	struct thermal_zone_device **ptr, *tzd;

	ptr = devres_alloc(devm_thermal_zone_of_sensor_release, sizeof(*ptr),
			   GFP_KERNEL);
	if (!ptr)
		return ERR_PTR(-ENOMEM);

	tzd = thermal_zone_of_sensor_register(dev, sensor_id, data, ops);
	if (IS_ERR(tzd)) {
		devres_free(ptr);
		return tzd;
	}

	*ptr = tzd;
	devres_add(dev, ptr);

	return tzd;
}
EXPORT_SYMBOL_GPL(devm_thermal_zone_of_sensor_register);

/**
 * devm_thermal_zone_of_sensor_unregister - Resource managed version of
 *				thermal_zone_of_sensor_unregister().
 * @dev: Device for which which resource was allocated.
 * @tzd: a pointer to struct thermal_zone_device where the sensor is registered.
 *
 * This function removes the sensor callbacks and private data from the
 * thermal zone device registered with devm_thermal_zone_of_sensor_register()
 * API. It will also silent the zone by remove the .get_temp() and .get_trend()
 * thermal zone device callbacks.
 * Normally this function will not need to be called and the resource
 * management code will ensure that the resource is freed.
 */
void devm_thermal_zone_of_sensor_unregister(struct device *dev,
					    struct thermal_zone_device *tzd)
{
	WARN_ON(devres_release(dev, devm_thermal_zone_of_sensor_release,
			       devm_thermal_zone_of_sensor_match, tzd));
}
EXPORT_SYMBOL_GPL(devm_thermal_zone_of_sensor_unregister);

/***   functions parsing device tree nodes   ***/

/**
 * thermal_of_populate_bind_params - parse and fill cooling map data
 * @np: DT node containing a cooling-map node
 * @__tbp: data structure to be filled with cooling map info
 * @trips: array of thermal zone trip points
 * @ntrips: number of trip points inside trips.
 *
 * This function parses a cooling-map type of node represented by
 * @np parameter and fills the read data into @__tbp data structure.
 * It needs the already parsed array of trip points of the thermal zone
 * in consideration.
 *
 * Return: 0 on success, proper error code otherwise
 */
static int thermal_of_populate_bind_params(struct device_node *np,
					   struct __thermal_bind_params *__tbp,
					   struct thermal_trip *trips,
					   int ntrips)
{
	struct of_phandle_args cooling_spec;
	struct __thermal_cooling_bind_param *__tcbp;
	struct device_node *trip;
	int ret, i, count;
	u32 prop;

	/* Default weight. Usage is optional */
	__tbp->usage = THERMAL_WEIGHT_DEFAULT;
	ret = of_property_read_u32(np, "contribution", &prop);
	if (ret == 0)
		__tbp->usage = prop;

	trip = of_parse_phandle(np, "trip", 0);
	if (!trip) {
		pr_err("missing trip property\n");
		return -ENODEV;
	}

	/* match using device_node */
	for (i = 0; i < ntrips; i++)
		if (trip == trips[i].np) {
			__tbp->trip_id = i;
			break;
		}

	if (i == ntrips) {
		ret = -ENODEV;
		goto end;
	}

	count = of_count_phandle_with_args(np, "cooling-device",
					   "#cooling-cells");
	if (count <= 0) {
		pr_err("Add a cooling_device property with at least one device\n");
		ret = -ENOENT;
		goto end;
	}

	__tcbp = kcalloc(count, sizeof(*__tcbp), GFP_KERNEL);
	if (!__tcbp) {
		ret = -ENOMEM;
		goto end;
	}

	for (i = 0; i < count; i++) {
		ret = of_parse_phandle_with_args(np, "cooling-device",
				"#cooling-cells", i, &cooling_spec);
		if (ret < 0) {
			pr_err("Invalid cooling-device entry\n");
			goto free_tcbp;
		}

		__tcbp[i].cooling_device = cooling_spec.np;

		if (cooling_spec.args_count >= 2) { /* at least min and max */
			__tcbp[i].min = cooling_spec.args[0];
			__tcbp[i].max = cooling_spec.args[1];
		} else {
			pr_err("wrong reference to cooling device, missing limits\n");
		}
	}

	__tbp->tcbp = __tcbp;
	__tbp->count = count;

	goto end;

free_tcbp:
	for (i = i - 1; i >= 0; i--)
		of_node_put(__tcbp[i].cooling_device);
	kfree(__tcbp);
end:
	of_node_put(trip);

	return ret;
}

/**
 * It maps 'enum thermal_trip_type' found in include/linux/thermal.h
 * into the device tree binding of 'trip', property type.
 */
static const char * const trip_types[] = {
	[THERMAL_TRIP_ACTIVE]	= "active",
	[THERMAL_TRIP_PASSIVE]	= "passive",
	[THERMAL_TRIP_HOT]	= "hot",
	[THERMAL_TRIP_CRITICAL]	= "critical",
};

/**
 * thermal_of_get_trip_type - Get phy mode for given device_node
 * @np:	Pointer to the given device_node
 * @type: Pointer to resulting trip type
 *
 * The function gets trip type string from property 'type',
 * and store its index in trip_types table in @type,
 *
 * Return: 0 on success, or errno in error case.
 */
static int thermal_of_get_trip_type(struct device_node *np,
				    enum thermal_trip_type *type)
{
	const char *t;
	int err, i;

	err = of_property_read_string(np, "type", &t);
	if (err < 0)
		return err;

	for (i = 0; i < ARRAY_SIZE(trip_types); i++)
		if (!strcasecmp(t, trip_types[i])) {
			*type = i;
			return 0;
		}

	return -ENODEV;
}

/**
 * thermal_of_populate_trip - parse and fill one trip point data
 * @np: DT node containing a trip point node
 * @trip: trip point data structure to be filled up
 *
 * This function parses a trip point type of node represented by
 * @np parameter and fills the read data into @trip data structure.
 *
 * Return: 0 on success, proper error code otherwise
 */
static int thermal_of_populate_trip(struct device_node *np,
				    struct thermal_trip *trip)
{
	int prop;
	int ret;

	ret = of_property_read_u32(np, "temperature", &prop);
	if (ret < 0) {
		pr_err("missing temperature property\n");
		return ret;
	}
	trip->temperature = prop;

	ret = of_property_read_u32(np, "hysteresis", &prop);
	if (ret < 0) {
		pr_err("missing hysteresis property\n");
		return ret;
	}
	trip->hysteresis = prop;

	ret = thermal_of_get_trip_type(np, &trip->type);
	if (ret < 0) {
		pr_err("wrong trip type property\n");
		return ret;
	}

	/* Required for cooling map matching */
	trip->np = np;
	of_node_get(np);

	return 0;
}

/**
 * thermal_of_build_thermal_zone - parse and fill one thermal zone data
 * @np: DT node containing a thermal zone node
 *
 * This function parses a thermal zone type of node represented by
 * @np parameter and fills the read data into a __thermal_zone data structure
 * and return this pointer.
 *
 * TODO: Missing properties to parse: thermal-sensor-names
 *
 * Return: On success returns a valid struct __thermal_zone,
 * otherwise, it returns a corresponding ERR_PTR(). Caller must
 * check the return value with help of IS_ERR() helper.
 */
#ifdef CONFIG_QTI_THERMAL
static struct __thermal_zone
__init *thermal_of_build_thermal_zone(struct device_node *np)
{
	struct device_node *child = NULL, *gchild;
	struct __thermal_zone *tz;
	int ret, i;
	u32 prop;

	if (!np) {
		pr_err("no thermal zone np\n");
		return ERR_PTR(-EINVAL);
	}

	tz = kzalloc(sizeof(*tz), GFP_KERNEL);
	if (!tz)
		return ERR_PTR(-ENOMEM);

	ret = of_property_read_u32(np, "polling-delay-passive", &prop);
	if (ret < 0) {
		pr_err("%pKOFn: missing polling-delay-passive property\n", np);
		goto free_tz;
	}
	tz->passive_delay = prop;

	ret = of_property_read_u32(np, "polling-delay", &prop);
	if (ret < 0) {
		pr_err("%pKOFn: missing polling-delay property\n", np);
		goto free_tz;
	}
	tz->polling_delay = prop;

	tz->default_disable = of_property_read_bool(np,
					"disable-thermal-zone");

	ret = of_count_phandle_with_args(np, "thermal-sensors",
						"#thermal-sensor-cells");
	if (ret < 0) {
		pr_err("%pKOFn: missing thermal-sensors property\n", np);
		goto free_tz;
	}
	tz->num_sensor = ret;
	bitmap_zero(&tz->registered_sensors, tz->num_sensor);

	if (tz->num_sensor > 1) {
		ret = of_property_read_u32(np, "sensor-aggregation", &prop);
		if (ret < 0) {
			pr_err("%pKOFn: missing sensor-aggregation property\n",
					np);
			goto free_tz;
		}
		if (prop >= SENSOR_AGGREGATE_NR) {
			pr_err("%pKOFn: invalid aggregation value:%d\n", np,
					prop);
			goto free_tz;
		}
		tz->sen_aggregate = prop;
	}

	tz->coeff = kcalloc(tz->num_sensor + 1, sizeof(*tz->coeff), GFP_KERNEL);
	if (!tz->coeff) {
		ret = -ENOMEM;
		goto free_tz;
	}
	ret = of_property_read_u32_array(np, "coefficients", tz->coeff,
					tz->num_sensor + 1);
	if (ret < 0) {
		for (i = 0; i < tz->num_sensor; i++)
			tz->coeff[i] = 1;
		tz->coeff[tz->num_sensor] = 0;
	}

	tz->senps = kcalloc(tz->num_sensor, sizeof(struct __sensor_param *),
				GFP_KERNEL);
	if (!tz->senps) {
		ret = -ENOMEM;
		goto free_coeff;
	}

	/* trips */
	child = of_get_child_by_name(np, "trips");

	/* No trips provided */
	if (!child)
		goto finish;

	tz->ntrips = of_get_child_count(child);
	if (tz->ntrips == 0) /* must have at least one child */
		goto finish;

	tz->trips = kcalloc(tz->ntrips, sizeof(*tz->trips), GFP_KERNEL);
	if (!tz->trips) {
		ret = -ENOMEM;
		goto free_senps;
	}

	i = 0;
	for_each_child_of_node(child, gchild) {
		ret = thermal_of_populate_trip(gchild, &tz->trips[i++]);
		if (ret)
			goto free_trips;
	}

	of_node_put(child);

	/* cooling-maps */
	child = of_get_child_by_name(np, "cooling-maps");

	/* cooling-maps not provided */
	if (!child)
		goto finish;

	tz->num_tbps = of_get_child_count(child);
	if (tz->num_tbps == 0)
		goto finish;

	tz->tbps = kcalloc(tz->num_tbps, sizeof(*tz->tbps), GFP_KERNEL);
	if (!tz->tbps) {
		ret = -ENOMEM;
		goto free_trips;
	}

	i = 0;
	for_each_child_of_node(child, gchild) {
		ret = thermal_of_populate_bind_params(gchild, &tz->tbps[i++],
						      tz->trips, tz->ntrips);
		if (ret)
			goto free_tbps;
	}

finish:
	of_node_put(child);
	tz->mode = THERMAL_DEVICE_DISABLED;

	return tz;

free_tbps:
	for (i = i - 1; i >= 0; i--) {
		struct __thermal_bind_params *tbp = tz->tbps + i;
		int j;

		for (j = 0; j < tbp->count; j++)
			of_node_put(tbp->tcbp[j].cooling_device);

		kfree(tbp->tcbp);
	}

	kfree(tz->tbps);
free_trips:
	for (i = 0; i < tz->ntrips; i++)
		of_node_put(tz->trips[i].np);
	kfree(tz->trips);
	of_node_put(gchild);
free_senps:
	kfree(tz->senps);
free_coeff:
	kfree(tz->coeff);
free_tz:
	kfree(tz);
	of_node_put(child);

	return ERR_PTR(ret);
}
#else
static struct __thermal_zone
__init *thermal_of_build_thermal_zone(struct device_node *np)
{
	struct device_node *child = NULL, *gchild;
	struct __thermal_zone *tz;
	int ret, i;
	u32 prop, coef[2];

	if (!np) {
		pr_err("no thermal zone np\n");
		return ERR_PTR(-EINVAL);
	}

	tz = kzalloc(sizeof(*tz), GFP_KERNEL);
	if (!tz)
		return ERR_PTR(-ENOMEM);

	ret = of_property_read_u32(np, "polling-delay-passive", &prop);
	if (ret < 0) {
		pr_err("%pKOFn: missing polling-delay-passive property\n", np);
		goto free_tz;
	}
	tz->passive_delay = prop;

	ret = of_property_read_u32(np, "polling-delay", &prop);
	if (ret < 0) {
		pr_err("%pKOFn: missing polling-delay property\n", np);
		goto free_tz;
	}
	tz->polling_delay = prop;

	/*
	 * REVIST: for now, the thermal framework supports only
	 * one sensor per thermal zone. Thus, we are considering
	 * only the first two values as slope and offset.
	 */
	ret = of_property_read_u32_array(np, "coefficients", coef, 2);
	if (ret == 0) {
		tz->slope = coef[0];
		tz->offset = coef[1];
	} else {
		tz->slope = 1;
		tz->offset = 0;
	}

	/* trips */
	child = of_get_child_by_name(np, "trips");

	/* No trips provided */
	if (!child)
		goto finish;

	tz->ntrips = of_get_child_count(child);
	if (tz->ntrips == 0) /* must have at least one child */
		goto finish;

	tz->trips = kcalloc(tz->ntrips, sizeof(*tz->trips), GFP_KERNEL);
	if (!tz->trips) {
		ret = -ENOMEM;
		goto free_tz;
	}

	i = 0;
	for_each_child_of_node(child, gchild) {
		ret = thermal_of_populate_trip(gchild, &tz->trips[i++]);
		if (ret)
			goto free_trips;
	}

	of_node_put(child);

	/* cooling-maps */
	child = of_get_child_by_name(np, "cooling-maps");

	/* cooling-maps not provided */
	if (!child)
		goto finish;

	tz->num_tbps = of_get_child_count(child);
	if (tz->num_tbps == 0)
		goto finish;

	tz->tbps = kcalloc(tz->num_tbps, sizeof(*tz->tbps), GFP_KERNEL);
	if (!tz->tbps) {
		ret = -ENOMEM;
		goto free_trips;
	}

	i = 0;
	for_each_child_of_node(child, gchild) {
		ret = thermal_of_populate_bind_params(gchild, &tz->tbps[i++],
						      tz->trips, tz->ntrips);
		if (ret)
			goto free_tbps;
	}

finish:
	of_node_put(child);
	tz->mode = THERMAL_DEVICE_DISABLED;

	return tz;

free_tbps:
	for (i = i - 1; i >= 0; i--) {
		struct __thermal_bind_params *tbp = tz->tbps + i;
		int j;

		for (j = 0; j < tbp->count; j++)
			of_node_put(tbp->tcbp[j].cooling_device);

		kfree(tbp->tcbp);
	}

	kfree(tz->tbps);
free_trips:
	for (i = 0; i < tz->ntrips; i++)
		of_node_put(tz->trips[i].np);
	kfree(tz->trips);
	of_node_put(gchild);
free_tz:
	kfree(tz);
	of_node_put(child);

	return ERR_PTR(ret);
}
#endif

static inline void of_thermal_free_zone(struct __thermal_zone *tz)
{
	struct __thermal_bind_params *tbp;
	int i, j;

	for (i = 0; i < tz->num_tbps; i++) {
		tbp = tz->tbps + i;

		for (j = 0; j < tbp->count; j++)
			of_node_put(tbp->tcbp[j].cooling_device);

		kfree(tbp->tcbp);
	}

	kfree(tz->tbps);
	for (i = 0; i < tz->ntrips; i++)
		of_node_put(tz->trips[i].np);
	kfree(tz->trips);
#ifdef CONFIG_QTI_THERMAL
	kfree(tz->coeff);
	kfree(tz->senps);
#endif
	kfree(tz);
}

/**
 * of_parse_thermal_zones - parse device tree thermal data
 *
 * Initialization function that can be called by machine initialization
 * code to parse thermal data and populate the thermal framework
 * with hardware thermal zones info. This function only parses thermal zones.
 * Cooling devices and sensor devices nodes are supposed to be parsed
 * by their respective drivers.
 *
 * Return: 0 on success, proper error code otherwise
 *
 */
int __init of_parse_thermal_zones(void)
{
	struct device_node *np, *child;
	struct __thermal_zone *tz;
	struct thermal_zone_device_ops *ops;

	np = of_find_node_by_name(NULL, "thermal-zones");
	if (!np) {
		pr_debug("unable to find thermal zones\n");
		return 0; /* Run successfully on systems without thermal DT */
	}

	for_each_available_child_of_node(np, child) {
		struct thermal_zone_device *zone;
		struct thermal_zone_params *tzp;
		int i, mask = 0;
		u32 prop;
#ifdef CONFIG_QTI_THERMAL
		const char *governor_name;
#endif

		tz = thermal_of_build_thermal_zone(child);
		if (IS_ERR(tz)) {
			pr_err("failed to build thermal zone %pKOFn: %ld\n",
			       child,
			       PTR_ERR(tz));
			continue;
		}

		ops = kmemdup(&of_thermal_ops, sizeof(*ops), GFP_KERNEL);
		if (!ops)
			goto exit_free;

		tzp = kzalloc(sizeof(*tzp), GFP_KERNEL);
		if (!tzp) {
			kfree(ops);
			goto exit_free;
		}

		/* No hwmon because there might be hwmon drivers registering */
		tzp->no_hwmon = true;

		if (!of_property_read_u32(child, "sustainable-power", &prop))
			tzp->sustainable_power = prop;

		for (i = 0; i < tz->ntrips; i++)
			mask |= 1 << i;

#ifdef CONFIG_QTI_THERMAL
		if (!of_property_read_string(child, "thermal-governor",
						&governor_name))
			strlcpy(tzp->governor_name, governor_name,
					THERMAL_NAME_LENGTH);
		/* these two are left for temperature drivers to use */
		tzp->slope = tz->coeff[0];
		tzp->offset = tz->coeff[1];
#else
		tzp->slope = tz->slope;
		tzp->offset = tz->offset;
#endif

		zone = thermal_zone_device_register(child->name, tz->ntrips,
						    mask, tz,
						    ops, tzp,
						    tz->passive_delay,
						    tz->polling_delay);
#ifdef CONFIG_QTI_THERMAL
		if (IS_ERR(zone)) {
			pr_err("Failed to build %pKOFn zone %ld\n", child,
			       PTR_ERR(zone));
			kfree(tzp);
			kfree(ops);
			of_thermal_free_zone(tz);
			/* attempting to build remaining zones still */
			continue;
		}
		tz->tzd = zone;
#else
		if (IS_ERR(zone)) {
			pr_err("Failed to build %pKOFn zone %ld\n", child,
			       PTR_ERR(zone));
			kfree(tzp);
			kfree(ops);
			of_thermal_free_zone(tz);
			/* attempting to build remaining zones still */
		}
#endif
	}
	of_node_put(np);

	return 0;

exit_free:
	of_node_put(child);
	of_node_put(np);
	of_thermal_free_zone(tz);

	/* no memory available, so free what we have built */
	of_thermal_destroy_zones();

	return -ENOMEM;
}

/**
 * of_thermal_destroy_zones - remove all zones parsed and allocated resources
 *
 * Finds all zones parsed and added to the thermal framework and remove them
 * from the system, together with their resources.
 *
 */
void of_thermal_destroy_zones(void)
{
	struct device_node *np, *child;

	np = of_find_node_by_name(NULL, "thermal-zones");
	if (!np) {
		pr_debug("unable to find thermal zones\n");
		return;
	}

	for_each_available_child_of_node(np, child) {
		struct thermal_zone_device *zone;

		zone = thermal_zone_get_zone_by_name(child->name);
		if (IS_ERR(zone))
			continue;

		thermal_zone_device_unregister(zone);
		kfree(zone->tzp);
		kfree(zone->ops);
		of_thermal_free_zone(zone->devdata);
	}
	of_node_put(np);
}
