/*
 * HID Sensors Driver
 * Copyright (c) 2012, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program;
 *
 */


#include <linux/device.h>
#include <linux/hid.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/hid-sensor-ids.h>
#include <linux/hid-sensor-hub.h>
#include <linux/sched.h>
#include "hid-ids.h"

/*#define CONFIG_ISS_PATH_SENSCOL*/

/***************************************/
#if 1

#ifdef CONFIG_ISS_PATH_SENSCOL
#define SENSCOL		1
#else
#define SENSCOL		0
#endif

#ifdef CONFIG_ISS_PATH_IIO
#define IIO		1
#else
#define IIO		0
#endif

#else

#define SENSCOL		1
#define IIO		0

#endif
/***************************************/

#if SENSCOL
#include <linux/senscol/senscol-core.h>
#endif /*SENSCOL*/

/*IIO*/
#include <linux/mfd/core.h>

#define HID_SENSOR_HUB_ENUM_QUIRK	0x01

/**
 * struct sensor_hub_pending - Synchronous read pending information
 * @status:		Pending status true/false.
 * @ready:		Completion synchronization data.
 * @usage_id:		Usage id for physical device, E.g. Gyro usage id.
 * @attr_usage_id:	Usage Id of a field, E.g. X-AXIS for a gyro.
 * @raw_size:		Response size for a read request.
 * @raw_data:		Place holder for received response.
 */
struct sensor_hub_pending {
	bool status;
	struct completion ready;
	u32 usage_id;
	u32 attr_usage_id;
	int raw_size;
	u8  *raw_data;
};

/**
 * struct sensor_hub_data - Hold a instance data for a HID hub device
 * @hsdev:		Stored hid instance for current hub device.
 * @mutex:		Mutex to serialize synchronous request.
 * @lock:		Spin lock to protect pending request structure.
 * @pending:		Holds information of pending sync read request.
 * @dyn_callback_list:	Holds callback function
 * @dyn_callback_lock:	spin lock to protect callback list
 * @hid_sensor_hub_client_devs:	Stores all MFD cells for a hub instance.
 * @hid_sensor_client_cnt: Number of MFD cells, (no of sensors attached).
 */
struct sensor_hub_data {
	struct hid_sensor_hub_device *hsdev;
	struct mutex mutex;
	spinlock_t lock;
	struct sensor_hub_pending pending;
	struct list_head dyn_callback_list;
	spinlock_t dyn_callback_lock;
	struct mfd_cell *hid_sensor_hub_client_devs;
	int hid_sensor_client_cnt;
	unsigned long quirks;
	int sensor_hub_index;	/* Needed to identify sensor in a collection */
};

#define	MAX_HID_SENSOR_HUBS 32
static struct hid_device *hid_sensor_hubs[MAX_HID_SENSOR_HUBS];
static int	sensor_hub_count;

/**
 * struct hid_sensor_hub_callbacks_list - Stores callback list
 * @list:		list head.
 * @usage_id:		usage id for a physical device.
 * @usage_callback:	Stores registered callback functions.
 * @priv:		Private data for a physical device.
 */
struct hid_sensor_hub_callbacks_list {
	struct list_head list;
	u32 usage_id;
	struct hid_sensor_hub_callbacks *usage_callback;
	void *priv;
};

static struct hid_report *sensor_hub_report(int id, struct hid_device *hdev,
						int dir)
{
	struct hid_report *report;

	list_for_each_entry(report, &hdev->report_enum[dir].report_list, list) {
		if (report->id == id)
			return report;
	}
	hid_warn(hdev, "No report with id 0x%x found\n", id);

	return NULL;
}

static int sensor_hub_get_physical_device_count(
				struct hid_report_enum *report_enum)
{
	struct hid_report *report;
	struct hid_field *field;
	int cnt = 0;

	list_for_each_entry(report, &report_enum->report_list, list) {
		field = report->field[0];
		if (report->maxfield && field && field->physical)
			cnt++;
	}

	return cnt;
}

static void sensor_hub_fill_attr_info(
		struct hid_sensor_hub_attribute_info *info,
		s32 index, s32 report_id, struct hid_field *field)
{
	info->index = index;
	info->report_id = report_id;
	info->units = field->unit;
	info->unit_expo = field->unit_exponent;
	info->size = (field->report_size * field->report_count)/8;
	info->logical_minimum = field->logical_minimum;
	info->logical_maximum = field->logical_maximum;
}

static struct hid_sensor_hub_callbacks *sensor_hub_get_callback(
					struct hid_device *hdev,
					u32 usage_id, void **priv)
{
	struct hid_sensor_hub_callbacks_list *callback;
	struct sensor_hub_data *pdata = hid_get_drvdata(hdev);

	spin_lock(&pdata->dyn_callback_lock);
	list_for_each_entry(callback, &pdata->dyn_callback_list, list)
		if (callback->usage_id == usage_id) {
			*priv = callback->priv;
			spin_unlock(&pdata->dyn_callback_lock);
			return callback->usage_callback;
		}
	spin_unlock(&pdata->dyn_callback_lock);

	return NULL;
}

int sensor_hub_register_callback(struct hid_sensor_hub_device *hsdev,
			u32 usage_id,
			struct hid_sensor_hub_callbacks *usage_callback)
{
	struct hid_sensor_hub_callbacks_list *callback;
	struct sensor_hub_data *pdata = hid_get_drvdata(hsdev->hdev);

	spin_lock(&pdata->dyn_callback_lock);
	list_for_each_entry(callback, &pdata->dyn_callback_list, list)
		if (callback->usage_id == usage_id) {
			spin_unlock(&pdata->dyn_callback_lock);
			return -EINVAL;
		}
	callback = kzalloc(sizeof(*callback), GFP_ATOMIC);
	if (!callback) {
		spin_unlock(&pdata->dyn_callback_lock);
		return -ENOMEM;
	}
	callback->usage_callback = usage_callback;
	callback->usage_id = usage_id;
	callback->priv = NULL;
	list_add_tail(&callback->list, &pdata->dyn_callback_list);
	spin_unlock(&pdata->dyn_callback_lock);

	return 0;
}
EXPORT_SYMBOL_GPL(sensor_hub_register_callback);

int sensor_hub_remove_callback(struct hid_sensor_hub_device *hsdev,
				u32 usage_id)
{
	struct hid_sensor_hub_callbacks_list *callback;
	struct sensor_hub_data *pdata = hid_get_drvdata(hsdev->hdev);

	spin_lock(&pdata->dyn_callback_lock);
	list_for_each_entry(callback, &pdata->dyn_callback_list, list)
		if (callback->usage_id == usage_id) {
			list_del(&callback->list);
			kfree(callback);
			break;
		}
	spin_unlock(&pdata->dyn_callback_lock);

	return 0;
}
EXPORT_SYMBOL_GPL(sensor_hub_remove_callback);

int sensor_hub_set_feature(struct hid_sensor_hub_device *hsdev, u32 report_id,
				u32 field_index, s32 value)
{
	struct hid_report *report;
	struct sensor_hub_data *data = hid_get_drvdata(hsdev->hdev);
	int ret = 0;

	mutex_lock(&data->mutex);
	report = sensor_hub_report(report_id, hsdev->hdev, HID_FEATURE_REPORT);
	if (!report || (field_index >= report->maxfield)) {
		ret = -EINVAL;
		goto done_proc;
	}
	hid_set_field(report->field[field_index], 0, value);
	hid_hw_request(hsdev->hdev, report, HID_REQ_SET_REPORT);
	hid_hw_wait(hsdev->hdev);

done_proc:
	mutex_unlock(&data->mutex);

	return ret;
}
EXPORT_SYMBOL_GPL(sensor_hub_set_feature);

int sensor_hub_get_feature(struct hid_sensor_hub_device *hsdev, u32 report_id,
				u32 field_index, s32 *value)
{
	struct hid_report *report;
	struct sensor_hub_data *data = hid_get_drvdata(hsdev->hdev);
	int ret = 0;

	mutex_lock(&data->mutex);
	report = sensor_hub_report(report_id, hsdev->hdev, HID_FEATURE_REPORT);
	if (!report || (field_index >= report->maxfield) ||
	    report->field[field_index]->report_count < 1) {
		ret = -EINVAL;
		goto done_proc;
	}
	hid_hw_request(hsdev->hdev, report, HID_REQ_GET_REPORT);
	hid_hw_wait(hsdev->hdev);
	*value = report->field[field_index]->value[0];

done_proc:
	mutex_unlock(&data->mutex);

	return ret;
}
EXPORT_SYMBOL_GPL(sensor_hub_get_feature);


int sensor_hub_input_attr_get_raw_value(struct hid_sensor_hub_device *hsdev,
					u32 usage_id,
					u32 attr_usage_id, u32 report_id)
{
	struct sensor_hub_data *data = hid_get_drvdata(hsdev->hdev);
	unsigned long flags;
	struct hid_report *report;
	int ret_val = 0;

	mutex_lock(&data->mutex);
	memset(&data->pending, 0, sizeof(data->pending));
	init_completion(&data->pending.ready);
	data->pending.usage_id = usage_id;
	data->pending.attr_usage_id = attr_usage_id;
	data->pending.raw_size = 0;

	spin_lock_irqsave(&data->lock, flags);
	data->pending.status = true;
	spin_unlock_irqrestore(&data->lock, flags);
	report = sensor_hub_report(report_id, hsdev->hdev, HID_INPUT_REPORT);
	if (!report)
		goto err_free;

	hid_hw_request(hsdev->hdev, report, HID_REQ_GET_REPORT);
	wait_for_completion_interruptible_timeout(&data->pending.ready, HZ*5);
	switch (data->pending.raw_size) {
	case 1:
		ret_val = *(u8 *)data->pending.raw_data;
		break;
	case 2:
		ret_val = *(u16 *)data->pending.raw_data;
		break;
	case 4:
		ret_val = *(u32 *)data->pending.raw_data;
		break;
	default:
		ret_val = 0;
	}
	kfree(data->pending.raw_data);

err_free:
	data->pending.status = false;
	mutex_unlock(&data->mutex);

	return ret_val;
}
EXPORT_SYMBOL_GPL(sensor_hub_input_attr_get_raw_value);

int hid_sensor_get_usage_index(struct hid_sensor_hub_device *hsdev,
				u32 report_id, int field_index, u32 usage_id)
{
	struct hid_report *report;
	struct hid_field *field;
	int i;

	report = sensor_hub_report(report_id, hsdev->hdev, HID_FEATURE_REPORT);
	if (!report || (field_index >= report->maxfield))
		goto done_proc;

	field = report->field[field_index];
	for (i = 0; i < field->maxusage; ++i) {
		if (field->usage[i].hid == usage_id)
			return field->usage[i].usage_index;
	}

done_proc:
	return -EINVAL;
}
EXPORT_SYMBOL_GPL(hid_sensor_get_usage_index);

int sensor_hub_input_get_attribute_info(struct hid_sensor_hub_device *hsdev,
				u8 type,
				u32 usage_id,
				u32 attr_usage_id,
				struct hid_sensor_hub_attribute_info *info)
{
	int ret = -1;
	int i, j;
	int collection_index = -1;
	struct hid_report *report;
	struct hid_field *field;
	struct hid_report_enum *report_enum;
	struct hid_device *hdev = hsdev->hdev;

	/* Initialize with defaults */
	info->usage_id = usage_id;
	info->attrib_id = attr_usage_id;
	info->report_id = -1;
	info->index = -1;
	info->units = -1;
	info->unit_expo = -1;

	for (i = 0; i < hdev->maxcollection; ++i) {
		struct hid_collection *collection = &hdev->collection[i];
		if (usage_id == collection->usage) {
			collection_index = i;
			break;
		}
	}
	if (collection_index == -1)
		goto err_ret;

	report_enum = &hdev->report_enum[type];
	list_for_each_entry(report, &report_enum->report_list, list) {
		for (i = 0; i < report->maxfield; ++i) {
			field = report->field[i];
			if (field->physical == usage_id &&
				field->logical == attr_usage_id) {
				sensor_hub_fill_attr_info(info, i, report->id,
							  field);
				ret = 0;
			} else {
				for (j = 0; j < field->maxusage; ++j) {
					if (field->usage[j].hid ==
					attr_usage_id &&
					field->usage[j].collection_index ==
					collection_index) {
						sensor_hub_fill_attr_info(info,
							  i, report->id, field);
						ret = 0;
						break;
					}
				}
			}
			if (ret == 0)
				break;
		}
	}

	if (info->units == 0)
		info->units = HID_USAGE_SENSOR_UNITS_MILLISECOND;

err_ret:
	return ret;
}
EXPORT_SYMBOL_GPL(sensor_hub_input_get_attribute_info);

#ifdef CONFIG_PM
static int sensor_hub_suspend(struct hid_device *hdev, pm_message_t message)
{
	struct sensor_hub_data *pdata = hid_get_drvdata(hdev);
	struct hid_sensor_hub_callbacks_list *callback;

	hid_dbg(hdev, " sensor_hub_suspend\n");
	spin_lock(&pdata->dyn_callback_lock);
	list_for_each_entry(callback, &pdata->dyn_callback_list, list) {
		if (callback->usage_callback->suspend)
			callback->usage_callback->suspend(
					pdata->hsdev, callback->priv);
	}
	spin_unlock(&pdata->dyn_callback_lock);

	return 0;
}

static int sensor_hub_resume(struct hid_device *hdev)
{
	struct sensor_hub_data *pdata = hid_get_drvdata(hdev);
	struct hid_sensor_hub_callbacks_list *callback;

	hid_dbg(hdev, " sensor_hub_resume\n");
	spin_lock(&pdata->dyn_callback_lock);
	list_for_each_entry(callback, &pdata->dyn_callback_list, list) {
		if (callback->usage_callback->resume)
			callback->usage_callback->resume(
					pdata->hsdev, callback->priv);
	}
	spin_unlock(&pdata->dyn_callback_lock);

	return 0;
}

static int sensor_hub_reset_resume(struct hid_device *hdev)
{
	return 0;
}
#endif /*CONFIG_PM*/

static bool is_supported(int physical)
{
	if (physical == 0x200073)
		return true;
	if (physical == 0x200041)
		return true;
	if (physical == 0x200076)
		return true;
	if (physical == 0x200083)
		return true;

	return false;
}

/**************************** SENSCOL block: START ****************************/
#if SENSCOL
static int	senscol_impl_added;
static int	is_sens_data_field(unsigned usage);
static int get_field_index(struct hid_device *hdev, unsigned report_id,
	unsigned usage, int report_idx);

/* Get sensor's property by name */
static struct sens_property *get_prop_by_name(struct sensor_def *sensor,
	char *name)
{
	int	i;

	for (i = 0; i < sensor->num_properties; ++i)
		if (!strcmp(sensor->properties[i].name, name))
			return	&sensor->properties[i];

	return	NULL;
}

/* Get sensor's data field by name */
static struct data_field *get_data_field_by_name(struct sensor_def *sensor,
	char *name)
{
	int	i;

	for (i = 0; i < sensor->num_data_fields; ++i)
		if (!strcmp(sensor->data_fields[i].name, name))
			return	&sensor->data_fields[i];

	return	NULL;
}

static int get_field_index(struct hid_device *hdev, unsigned report_id,
	unsigned usage, int report_type)
{
	int i = 0;
	struct hid_report *report;

	report = sensor_hub_report(report_id, hdev,
		report_type /*HID_FEATURE_REPORT or HID_INPUT_REPORT*/);
	if (!report)
		return -1;

	for (i = 0; i < report->maxfield; ++i)
		if (report->field[i]->usage->hid == usage)
			return i;

	return -1;
}

/*
 * The reason for this _ex() function is broken semantics and existing
 * usage of sensor_hub_get_feature() that
 * doesn't allow anything with ->report_count > 1 to be delivered.
 * If that was fixed, existing callers would immediately buffer-overflow
 * if such feature was delivered
 * NOTES:
 *   - if ret != 0, contents of pvalue and count are undefined.
 *   - upon success, count is in s32 values (not in bytes)
 */
static int sensor_hub_get_feature_ex(struct hid_sensor_hub_device *hsdev,
	u32 report_id, u32 field_index, u32 *usage_id, s32 **pvalue,
	size_t *count, unsigned *is_string)
{
	struct hid_report *report;
	struct sensor_hub_data *data =  hid_get_drvdata(hsdev->hdev);
	int ret = 0;

	mutex_lock(&data->mutex);
	report = sensor_hub_report(report_id, hsdev->hdev, HID_FEATURE_REPORT);
	if (!report || (field_index >=  report->maxfield) ||
	    report->field[field_index]->report_count < 1) {
		ret = -EINVAL;
		goto done_proc;
	}
	hid_hw_request(hsdev->hdev, report, HID_REQ_GET_REPORT);
	hid_hw_wait(hsdev->hdev);
	*pvalue = report->field[field_index]->value;
	*count = report->field[field_index]->report_count;
	*usage_id = report->field[field_index]->usage->hid;
	*is_string = (report->field[field_index]->report_size == 16) &&
			(*count > 1);

done_proc:
	mutex_unlock(&data->mutex);

	return ret;
}

/* Get sensor hub device by index */
static struct sensor_hub_data	*get_sensor_hub_by_index(unsigned idx)
{
	int	i;
	struct sensor_hub_data	*sd;

	for (i = 0; i < sensor_hub_count; ++i) {
		if (!hid_sensor_hubs[i])
			continue;
		sd = hid_get_drvdata(hid_sensor_hubs[i]);
		if (!sd)
			continue;
		if (sd->sensor_hub_index == idx)
			return	sd;
	}

	return	NULL;
}

static int     hid_get_sens_property(struct sensor_def *sensor,
	const struct sens_property *prop, char *value, size_t val_buf_size)
{
	unsigned	idx;
	struct sensor_hub_data	*sd;
	char	buf[1024];		/* Enough for single property (?) */
	unsigned	report_id;
	int	field;
	uint32_t	usage_id;
	int32_t	*pval;
	size_t	count;
	unsigned is_string;
	int	rv;

	if (!sensor || !prop)
		return	-EINVAL;	/* input is invalid */

	/* sensor hub device */
	idx = sensor->id >> 16 & 0xFFFF;
	sd = get_sensor_hub_by_index(idx);
	if (!sd)
		return	-EINVAL;	/* sensor->id is bad */

	/* Report ID */
	report_id = sensor->id & 0xFFFF;

	/* Field index */
	field = get_field_index(sd->hsdev->hdev, report_id, prop->usage_id,
		HID_FEATURE_REPORT);
	if (field == -1)
		return	-EINVAL;	/* Something is still wrong */

	/* Get value */
	rv = sensor_hub_get_feature_ex(sd->hsdev, report_id, field, &usage_id,
		&pval, &count, &is_string);
	if (rv)
		return	rv;

	if  (is_string) {
		int	i;

		for (i = 0; i < count; ++i)
			buf[i] = (char)pval[i];
		buf[i] = '\0';
	} else {
		/* Verify output length */
		sprintf(buf, "%d", *pval);
	}

	if (strlen(buf) >= val_buf_size)
		return	-EMSGSIZE;
	strcpy(value, buf);
	return	0;
}

static int     hid_set_sens_property(struct sensor_def *sensor,
	const struct sens_property *prop, const char *value)
{
	unsigned	idx;
	struct sensor_hub_data	*sd;
	unsigned	report_id;
	int	field;
	int32_t	val;
	int	rv;

	if (!sensor || !prop)
		return	-EINVAL;	/* input is invalid */

	/* Value */
	rv = sscanf(value, " %d ", &val);
	if (rv != 1)
		return	-EINVAL;	/* Bad value */

	/* sensor hub device */
	idx = sensor->id >> 16 & 0xFFFF;
	sd = get_sensor_hub_by_index(idx);
	if (!sd)
		return	-EINVAL;	/* sensor->id is bad */

	/* Report ID */
	report_id = sensor->id & 0xFFFF;

	/* Field index */
	field = get_field_index(sd->hsdev->hdev, report_id, prop->usage_id,
		HID_FEATURE_REPORT);
	if (field == -1)
		return	-EINVAL;	/* Something is still wrong */

	/* Get value */
	rv = sensor_hub_set_feature(sd->hsdev, report_id, field, val);
	return	rv;
}

static int     hid_get_sample(struct sensor_def *sensor, void *sample_buf,
	size_t sample_buf_size)
{
	unsigned	idx;
	struct sensor_hub_data	*sd;
	unsigned	report_id;
	struct data_field *data_field;
	int32_t	val;

	/* sensor hub device */
	idx = sensor->id >> 16 & 0xFFFF;
	sd = get_sensor_hub_by_index(idx);
	if (!sd)
		return	-EINVAL;	/* sensor->id is bad */

	/* Report ID */
	report_id = sensor->id & 0xFFFF;

	/*
	 * Request an input report with the first data field,
	 * regardless of what it is
	 */
	data_field = &sensor->data_fields[0];
	val = sensor_hub_input_attr_get_raw_value(sd->hsdev, sensor->usage_id,
		data_field->usage_id, HID_INPUT_REPORT);
	if (!sd->pending.status)
		return	-EIO;

	/*
	 * Actual sample will be pushed by sensor_hub_raw_event().
	 * Invoke a short sleep in order to remove threads race condition and
	 * ensure that the sample is in senscol buffer
	 */
	schedule_timeout(2);

	return	0;
}

/* Check sensor is activated and in batch mode                  *
 * property_power_state =       2       hid_usage 0x200319      *
 * property_reporting_state =   2/5     hid_usage 0x200316      *
 * property_report_interval !=  0       hid_usage 0x20030e      *
 * property_report_interval_resolution != 0 hid_usage 0x20530e  *
 * return value:        0 - sensor is not activated in batch    *
 *                      1 - sensor is activated in batch        */
static int      hid_batch_check(struct sensor_def *sensor)
{
	unsigned idx;
	struct sensor_hub_data  *sd;
	unsigned report_id;
	struct hid_report *report;
	int field_idx;

	idx = sensor->id >> 16 & 0xFFFF;
	sd = get_sensor_hub_by_index(idx);
	if (!sd)
		return -EINVAL;
	report_id = sensor->id & 0xFFFF;
	report = sensor_hub_report(report_id, sd->hsdev->hdev,
		HID_FEATURE_REPORT);
	if (!report)
		return -EINVAL;

	/* property_power_state */
	field_idx = get_field_index(sd->hsdev->hdev, report_id, 0x200319,
		HID_FEATURE_REPORT);
	if (field_idx < 0)
		return -EINVAL;
	if (report->field[field_idx]->value[0] != 2)
		return 0;

	/* property_reporting_state */
	field_idx = get_field_index(sd->hsdev->hdev, report_id, 0x200316,
		HID_FEATURE_REPORT);
	if (field_idx < 0)
		return -EINVAL;
	if (report->field[field_idx]->value[0] != 2 &&
		report->field[field_idx]->value[0] != 5)
		return 0;

	/* property_report_interval */
	field_idx = get_field_index(sd->hsdev->hdev, report_id, 0x20030e,
		HID_FEATURE_REPORT);
	if (field_idx < 0)
		return -EINVAL;
	if (report->field[field_idx]->value[0] == 0)
		return 0;

	/* property_report_interval_resolution */
	field_idx = get_field_index(sd->hsdev->hdev, report_id, 0x20530e,
		HID_FEATURE_REPORT);
	if (field_idx < 0)
		return -EINVAL;
	if (report->field[field_idx]->value[0] == 0)
		return 0;

	return 1;
}

struct senscol_impl	hid_senscol_impl = {
	.get_sens_property = hid_get_sens_property,
	.set_sens_property = hid_set_sens_property,
	.get_sample = hid_get_sample,
	.batch_check = hid_batch_check
};

static int	is_sens_data_field(unsigned usage)
{
	if (usage >= 0x400 && usage <= 0x49F ||
			usage >= 0x4B0 && usage <= 0x4DF ||
			usage >= 0x4F0 && usage <= 0x4F7 ||
			usage >= 0x500 && usage <= 0x52F ||
			usage >= 0x540 && usage <= 0x57F ||
			usage >= 590 && usage <= 0x7FF)
		return	1;
	return	0;
}

static int	fill_data_field(struct hid_field *field, unsigned usage,
	struct sensor_def *senscol_sensor)
{
	struct data_field	data_field;
	char	*usage_name;
	int	rv;

	memset(&data_field, 0, sizeof(struct data_field));
	usage_name = senscol_usage_to_name(usage & 0xFFFF);
	if (usage_name)
		data_field.name = kasprintf(GFP_KERNEL, "%s", usage_name);
	else
		data_field.name = kasprintf(GFP_KERNEL, "data-%X", usage);
	if (!data_field.name)
		return	-ENOMEM;

	data_field.usage_id = usage;
	data_field.is_numeric = (field->flags & HID_MAIN_ITEM_VARIABLE);
	if (data_field.is_numeric) {
		if (field->unit_exponent > 7 ||
				field->unit_exponent < -8)
			data_field.exp = 0xFF;
		else if (field->unit_exponent >= 0)
			data_field.exp = field->unit_exponent;
		else
			data_field.exp = 0x10 - field->unit_exponent;
		data_field.unit = field->unit;
	}

	data_field.len = (field->report_size >> 3) * field->report_count;
	rv = add_data_field(senscol_sensor, &data_field);
	senscol_sensor->sample_size += (field->report_size >> 3) *
		field->report_count;

	return	rv;
}

#endif /*SENSCOL*/
/***************************** SENSCOL block: END *****************************/

/*
 * Handle raw report as sent by device
 */
static int sensor_hub_raw_event(struct hid_device *hdev,
		struct hid_report *report, u8 *raw_data, int size)
{
	int i;
	u8 *ptr;
	int sz;
	struct sensor_hub_data *pdata = hid_get_drvdata(hdev);
	unsigned long flags;
	struct hid_collection *collection = NULL;
	void *priv = NULL;
#if SENSCOL
	uint32_t	sensor_id;
	unsigned char	data_buf[1024];
	unsigned	sample_size;
#endif /*SENSCOL*/

/*#if IIO*/
	struct hid_sensor_hub_callbacks *callback = NULL;
/*#endif*/


	hid_dbg(hdev, "sensor_hub_raw_event report id:0x%x size:%d type:%d\n",
			 report->id, size, report->type);
	hid_dbg(hdev, "maxfield:%d\n", report->maxfield);
	if (report->type != HID_INPUT_REPORT)
		return 0;

	ptr = raw_data;
	ptr++; /* Skip report id */

	spin_lock_irqsave(&pdata->lock, flags);

#if SENSCOL
	/* make up senscol id */
	sensor_id = pdata->sensor_hub_index << 16 | report->id & 0xFFFF;
	sample_size = 0;
#endif /*SENSCOL*/

	for (i = 0; i < report->maxfield; ++i) {
		hid_dbg(hdev, "%d collection_index:%x hid:%x sz:%x\n",
				i, report->field[i]->usage->collection_index,
				report->field[i]->usage->hid,
				(report->field[i]->report_size *
					report->field[i]->report_count)/8);
		sz = (report->field[i]->report_size *
					report->field[i]->report_count)/8;
/*#if IIO*/
		if (pdata->pending.status && pdata->pending.attr_usage_id ==
				report->field[i]->usage->hid) {
			hid_dbg(hdev, "data was pending ...\n");
			pdata->pending.raw_data = kmemdup(ptr, sz, GFP_ATOMIC);
			if (pdata->pending.raw_data)
				pdata->pending.raw_size = sz;
			else
				pdata->pending.raw_size = 0;
			complete(&pdata->pending.ready);
		}
		collection = &hdev->collection[
				report->field[i]->usage->collection_index];
		hid_dbg(hdev, "collection->usage %x\n",
					collection->usage);
		callback = sensor_hub_get_callback(pdata->hsdev->hdev,
						report->field[i]->physical,
							&priv);
		if (callback && callback->capture_sample) {
			if (report->field[i]->logical)
				callback->capture_sample(pdata->hsdev,
					report->field[i]->logical, sz, ptr,
					callback->pdev);
			else
				callback->capture_sample(pdata->hsdev,
					report->field[i]->usage->hid, sz, ptr,
					callback->pdev);
		}
/*#endif*/

#if SENSCOL
		/* Prepare data for senscol sample */
		if (is_sens_data_field(report->field[i]->usage->hid & 0xFFFF)) {
			dev_dbg(&hdev->dev, "%s(): aggregating, sz=%u\n",
				__func__, sample_size);
			memcpy(data_buf + sample_size, ptr, sz);
			sample_size += sz;
		}
#endif /*SENSCOL*/
		/*
		 * If we want to add indication into raw stream that the last
		 * sample was synchronous, it's here: check for complete()
		 * condition above
		 */

		ptr += sz;
	}
/*#if IIO*/
	if (callback && collection && callback->send_event)
		callback->send_event(pdata->hsdev, collection->usage,
				callback->pdev);
/*endif*/
	spin_unlock_irqrestore(&pdata->lock, flags);

#if SENSCOL
	/* Upstream sample to sensor collection framework */
	dev_dbg(&hdev->dev, "%s(): calling push_sample, aggregated sample size is %u\n",
		__func__, sample_size);
	push_sample(sensor_id, data_buf);
#endif /*SENSCOL*/
	return 1;
}

int sensor_hub_device_open(struct hid_sensor_hub_device *hsdev)
{
	int ret = 0;
	struct sensor_hub_data *data =  hid_get_drvdata(hsdev->hdev);

	mutex_lock(&data->mutex);
	if (!hsdev->ref_cnt) {
		ret = hid_hw_open(hsdev->hdev);
		if (ret) {
			hid_err(hsdev->hdev, "failed to open hid device\n");
			mutex_unlock(&data->mutex);
			return ret;
		}
	}
	hsdev->ref_cnt++;
	mutex_unlock(&data->mutex);

	return ret;
}
EXPORT_SYMBOL_GPL(sensor_hub_device_open);

void sensor_hub_device_close(struct hid_sensor_hub_device *hsdev)
{
	struct sensor_hub_data *data =  hid_get_drvdata(hsdev->hdev);

	mutex_lock(&data->mutex);
	hsdev->ref_cnt--;
	if (!hsdev->ref_cnt)
		hid_hw_close(hsdev->hdev);
	mutex_unlock(&data->mutex);
}
EXPORT_SYMBOL_GPL(sensor_hub_device_close);

static __u8 *sensor_hub_report_fixup(struct hid_device *hdev, __u8 *rdesc,
		unsigned int *rsize)
{
	int index;
	struct sensor_hub_data *sd =  hid_get_drvdata(hdev);
	unsigned char report_block[] = {
				0x0a,  0x16, 0x03, 0x15, 0x00, 0x25, 0x05};
	unsigned char power_block[] = {
				0x0a,  0x19, 0x03, 0x15, 0x00, 0x25, 0x05};

	if (!(sd->quirks & HID_SENSOR_HUB_ENUM_QUIRK)) {
		hid_dbg(hdev, "No Enum quirks\n");
		return rdesc;
	}

	/* Looks for power and report state usage id and force to 1 */
	for (index = 0; index < *rsize; ++index) {
		if (((*rsize - index) > sizeof(report_block)) &&
			!memcmp(&rdesc[index], report_block,
						sizeof(report_block))) {
			rdesc[index + 4] = 0x01;
			index += sizeof(report_block);
		}
		if (((*rsize - index) > sizeof(power_block)) &&
			!memcmp(&rdesc[index], power_block,
						sizeof(power_block))) {
			rdesc[index + 4] = 0x01;
			index += sizeof(power_block);
		}
	}

	return rdesc;
}

static int sensor_hub_probe(struct hid_device *hdev,
				const struct hid_device_id *id)
{
	int ret;
	struct sensor_hub_data *sd;
	int i;
	char *name;
	struct hid_report *report, *freport;
	struct hid_report_enum *inp_report_enum, *feat_report_enum;
	struct hid_field *field, *feat_field;
	int dev_cnt;
	int	rv;
	struct sensor_def	*senscol_sensor;
	int	j;
	const char	*usage_name;

	sd = devm_kzalloc(&hdev->dev, sizeof(*sd), GFP_KERNEL);
	if (!sd) {
		hid_err(hdev, "cannot allocate Sensor data\n");
		return -ENOMEM;
	}
	sd->hsdev = devm_kzalloc(&hdev->dev, sizeof(*sd->hsdev), GFP_KERNEL);
	if (!sd->hsdev) {
		hid_err(hdev, "cannot allocate hid_sensor_hub_device\n");
		ret = -ENOMEM;
		goto err_free_hub;
	}
	hid_set_drvdata(hdev, sd);
	/* Keep array of HID sensor hubs for senscol_impl usage */
	hid_sensor_hubs[sensor_hub_count] = hdev;
	/* Need to count sensor hub devices for senscol ids */
	sd->sensor_hub_index = sensor_hub_count++;
	sd->quirks = id->driver_data;
	sd->hsdev->hdev = hdev;
	sd->hsdev->vendor_id = hdev->vendor;
	sd->hsdev->product_id = hdev->product;
	spin_lock_init(&sd->lock);
	spin_lock_init(&sd->dyn_callback_lock);
	mutex_init(&sd->mutex);
	ret = hid_parse(hdev);
	if (ret) {
		hid_err(hdev, "parse failed\n");
		goto err_free;
	}
	INIT_LIST_HEAD(&hdev->inputs);

	ret = hid_hw_start(hdev, 0);
	if (ret) {
		hid_err(hdev, "hw start failed\n");
		goto err_free;
	}
	INIT_LIST_HEAD(&sd->dyn_callback_list);
	sd->hid_sensor_client_cnt = 0;
	inp_report_enum = &hdev->report_enum[HID_INPUT_REPORT];
	feat_report_enum = &hdev->report_enum[HID_FEATURE_REPORT];

	dev_cnt = sensor_hub_get_physical_device_count(inp_report_enum);
	if (dev_cnt > HID_MAX_PHY_DEVICES) {
		hid_err(hdev, "Invalid Physical device count\n");
		ret = -EINVAL;
		goto err_stop_hw;
	}

#if IIO
	/* filter the unknown sensors */
	list_for_each_entry(report, &inp_report_enum->report_list, list) {
		dev_dbg(&hdev->dev, "Checking Report id:%x\n", report->id);
		field = report->field[0];
		if (report->maxfield && field && field->physical) {
			if (!is_supported(field->physical))
				dev_cnt--;
		}
	}
	dev_dbg(&hdev->dev, "%s(): Dev Count after  is %d\n",
		__func__, dev_cnt);
#endif /*IIO*/

#if SENSCOL
	/* Register senscol impl */
	if (!senscol_impl_added) {
		rv = add_senscol_impl(&hid_senscol_impl);
		dev_dbg(&hdev->dev, "%s(): add_senscol_impl() returned %d\n",
			__func__, rv);
		if (!rv)
			senscol_impl_added = 1;
	}
#endif /*SENSCOL*/
	sd->hid_sensor_hub_client_devs = kzalloc(dev_cnt *
						sizeof(struct mfd_cell),
						GFP_KERNEL);
	if (sd->hid_sensor_hub_client_devs == NULL) {
		hid_err(hdev, "Failed to allocate memory for mfd cells\n");
			ret = -ENOMEM;
			goto err_stop_hw;
	}

/* #if SENSCOL */
	/* Translate properties logical->physical if needed here,
	 * instead of hid-core */
	list_for_each_entry(freport, &feat_report_enum->report_list, list) {
		int	j;

		for (j = 0; j < freport->maxfield; ++j) {
			feat_field = freport->field[j];
			if (!(feat_field->flags & HID_MAIN_ITEM_VARIABLE))
				for (i = 0; i < feat_field->maxusage; ++i)
					feat_field->usage[i].hid =
						feat_field->logical;
		}
	}

	/* Translate  input fields logical->physical if needed here,
	 * instead of hid-core */
	list_for_each_entry(report, &inp_report_enum->report_list, list) {
		int	j;

		for (j = 0; j < report->maxfield; ++j) {
			field = report->field[j];
			if (!(field->flags & HID_MAIN_ITEM_VARIABLE))
				for (i = 0; i < field->maxusage; ++i)
					field->usage[i].hid = field->logical;
		}
	}
/* #endif */

	list_for_each_entry(report, &inp_report_enum->report_list, list) {
		hid_dbg(hdev, "Report id:%x\n", report->id);
		field = report->field[0];

		if (report->maxfield && field &&
				field->physical &&
				is_supported(field->physical)) {
			name = kasprintf(GFP_KERNEL, "HID-SENSOR-%x",
					field->physical);
			if (name == NULL) {
				hid_err(hdev, "Failed MFD device name\n");
					ret = -ENOMEM;
					goto err_free_names;
			}
			sd->hid_sensor_hub_client_devs[
				sd->hid_sensor_client_cnt].id =
					PLATFORM_DEVID_AUTO;
			sd->hid_sensor_hub_client_devs[
				sd->hid_sensor_client_cnt].name = name;
			sd->hid_sensor_hub_client_devs[
				sd->hid_sensor_client_cnt].platform_data =
					sd->hsdev;
			sd->hid_sensor_hub_client_devs[
				sd->hid_sensor_client_cnt].pdata_size =
					sizeof(*sd->hsdev);
			hid_dbg(hdev, "Adding %s:%p\n", name, sd);
			sd->hid_sensor_client_cnt++;
		}
#if SENSCOL
		/* Create senscol sensor from each report,
		 * regardles of is_supported() */
		senscol_sensor = alloc_senscol_sensor();
		if (!senscol_sensor) {
			dev_err(&hdev->dev,
				"%s(): failed to allocate sensor\n", __func__);
			break;
		}
		init_senscol_sensor(senscol_sensor);
		usage_name = senscol_usage_to_name(field->physical & 0xFFFF);
		if (usage_name)
			senscol_sensor->name = kasprintf(GFP_KERNEL,
				"%s", usage_name);
		else
			senscol_sensor->name = kasprintf(GFP_KERNEL,
				"custom-%X", field->physical);
		if (!senscol_sensor->name) {
			dev_err(&hdev->dev,
				"%s(): failed to allocate name\n",
				__func__);
			kfree(senscol_sensor);
			break;
		}
		senscol_sensor->usage_id = field->physical;
		senscol_sensor->id = sd->sensor_hub_index << 16 |
			report->id & 0xFFFF;
		senscol_sensor->impl = &hid_senscol_impl;
		senscol_sensor->sample_size = 0;

		/* Add properties */
		/* 1. find matching feature report */
		list_for_each_entry(freport,
				&feat_report_enum->report_list,
				list) {
			feat_field = freport->field[0];
			if (freport->maxfield && feat_field &&
					feat_field->physical &&
					(feat_field->physical ==
					senscol_sensor->usage_id) &&
					freport->id == report->id)
				break;
		}

		/*2. dump each prop field */
		for (i = 0; i < freport->maxfield; ++i) {
			struct sens_property	prop_field;

			dev_dbg(&hdev->dev,
				"%d collection_index:%x hid:%x sz:%x ",
				i,
				freport->field[i]->usage->
					collection_index,
				freport->field[i]->usage->hid,
				freport->field[i]->report_size / 8);

			dev_dbg(&hdev->dev, "report count: %u\n",
				freport->field[i]->report_count);

			memset(&prop_field, 0,
				sizeof(struct sens_property));
			prop_field.usage_id =
				freport->field[i]->usage->hid;
			usage_name = senscol_usage_to_name(
				prop_field.usage_id & 0xFFFF);
			if (usage_name)
				prop_field.name = kasprintf(GFP_KERNEL,
						"%s", usage_name);
			/* there is  a special case when the property
			 * is related to specific data field/
			 * set of fields */
			else {
				uint32_t modifier =
					prop_field.usage_id & 0xF000;
				uint32_t data_hid =
					prop_field.usage_id & 0x0FFF;
				usage_name = senscol_usage_to_name(
					data_hid);
				dev_dbg(&hdev->dev,
					"%s(): DATANAME %s\n",
					__func__, usage_name);
				if (!usage_name)
					prop_field.name =
						kasprintf(GFP_KERNEL,
						"unknown-%X",
						prop_field.usage_id && 0xFFFF);
				else {
					const char *modif_name =
						senscol_get_modifier(modifier);
					dev_dbg(&hdev->dev,
						"%s(): MODIFNAME %s\n",
						__func__, modif_name);
					prop_field.name =
						kasprintf(GFP_KERNEL,
						"%s_%s", usage_name,
						modif_name);
				}
			}
			prop_field.is_numeric =
				(freport->field[i]->flags &
				HID_MAIN_ITEM_VARIABLE) &&
				/* not string: not array of unsigned short */
				!(freport->field[i]->report_count > 1 &&
				freport->field[i]->report_size == 16);

			rv = add_sens_property(senscol_sensor,
				&prop_field);
			dev_dbg(&hdev->dev, "%s(): ", __func__);
			dev_dbg(&hdev->dev, "adding prop %s %s %d\n",
				prop_field.name, "returned",  rv);
		}

		/* Add data fields; Dump fields in this report.
		`maxfield' is upper-bound NON-INCLUSIVE */
		for (j = 0; j < report->maxfield; ++j) {

			dev_dbg(&hdev->dev, "%s(): ", __func__);
			dev_dbg(&hdev->dev,
				"%s=%d %s=%08X %s=%08X %s=%u %s=%u ",
				"field", j,
				"physical",  report->field[j]->physical,
				"logical", report->field[j]->logical,
				"maxusage", report->field[j]->maxusage,
				"report_type",
				report->field[j]->report_type);
			dev_dbg(&hdev->dev, "%s=%u %s=%d %s=%d %s=%d ",
				"report_size",
				report->field[j]->report_size >> 3,
				"logic_min",
				report->field[j]->logical_minimum,
				"logic_max",
				report->field[j]->logical_maximum,
				"phys_min",
				report->field[j]->physical_minimum);
			dev_dbg(&hdev->dev, "%s=%d %s=%d %s=%u %s=%d\n",
				"phys_max",
				report->field[j]->physical_maximum,
				"exp",
				report->field[j]->unit_exponent,
				"unit",
				report->field[j]->unit,
				"report_count",
				report->field[j]->report_count);
			dev_dbg(&hdev->dev, "%s(): usages --\n",
				__func__);

			field = report->field[j];
			/* Add data field */
			if (is_sens_data_field(field->usage[0].hid & 0xFFFF)) {
				rv = fill_data_field(field,
					field->usage[0].hid,
					senscol_sensor);
				if (rv == -ENOMEM)
					dev_err(&hdev->dev,
						"%s(): Failed to allocated data field for usage %08X\n",
						__func__,
						field->usage[0].hid);
			}
		}

		/* Add senscol_sensor */
		rv = add_senscol_sensor(senscol_sensor);
		dev_dbg(&hdev->dev,
			"%s(): add_senscol_sensor() returned %d\n",
			__func__, rv);
#endif /*SENSCOL*/
	}
#if IIO
	ret = mfd_add_devices(&hdev->dev, 0, sd->hid_sensor_hub_client_devs,
		sd->hid_sensor_client_cnt, NULL, 0, NULL);
	if (ret < 0)
		goto err_free_names;
#endif /*IIO*/
	return ret;

err_free_names:
	for (i = 0; i < sd->hid_sensor_client_cnt ; ++i)
		kfree(sd->hid_sensor_hub_client_devs[i].name);
	kfree(sd->hid_sensor_hub_client_devs);
err_stop_hw:
	hid_hw_stop(hdev);
err_free:
	/*kfree(sd->hsdev);*/
err_free_hub:
	/*kfree(sd);*/

	return ret;
}

static void sensor_hub_remove(struct hid_device *hdev)
{
	struct sensor_hub_data *data = hid_get_drvdata(hdev);
	unsigned long flags;
	int i;

	for (i = 0; i < sensor_hub_count; ++i)
		if (hid_sensor_hubs[i] == hdev) {
			hid_sensor_hubs[i] = NULL;
			break;
		}

	hid_dbg(hdev, " hardware removed\n");
	hid_hw_close(hdev);
	hid_hw_stop(hdev);
	spin_lock_irqsave(&data->lock, flags);
	if (data->pending.status)
		complete(&data->pending.ready);
	spin_unlock_irqrestore(&data->lock, flags);
	mfd_remove_devices(&hdev->dev);
	for (i = 0; i < data->hid_sensor_client_cnt ; ++i)
		kfree(data->hid_sensor_hub_client_devs[i].name);
	kfree(data->hid_sensor_hub_client_devs);
	hid_set_drvdata(hdev, NULL);
	mutex_destroy(&data->mutex);
}

static const struct hid_device_id sensor_hub_devices[] = {
	{ HID_DEVICE(HID_BUS_ANY, HID_GROUP_SENSOR_HUB, USB_VENDOR_ID_INTEL_0,
			USB_DEVICE_ID_INTEL_HID_SENSOR),
			.driver_data = HID_SENSOR_HUB_ENUM_QUIRK},
	{ HID_DEVICE(HID_BUS_ANY, HID_GROUP_SENSOR_HUB, USB_VENDOR_ID_INTEL_1,
			USB_DEVICE_ID_INTEL_HID_SENSOR),
			.driver_data = HID_SENSOR_HUB_ENUM_QUIRK},
	{ HID_DEVICE(HID_BUS_ANY, HID_GROUP_SENSOR_HUB, USB_VENDOR_ID_STM_0,
			USB_DEVICE_ID_STM_HID_SENSOR),
			.driver_data = HID_SENSOR_HUB_ENUM_QUIRK},
	{ HID_DEVICE(HID_BUS_ANY, HID_GROUP_SENSOR_HUB, USB_VENDOR_ID_STM_0,
			USB_DEVICE_ID_STM_HID_SENSOR_1),
			.driver_data = HID_SENSOR_HUB_ENUM_QUIRK},
	{ HID_DEVICE(HID_BUS_ANY, HID_GROUP_SENSOR_HUB, HID_ANY_ID,
		     HID_ANY_ID) },
	{ }
};
MODULE_DEVICE_TABLE(hid, sensor_hub_devices);

static struct hid_driver sensor_hub_driver = {
	.name = "hid-sensor-hub",
	.id_table = sensor_hub_devices,
	.probe = sensor_hub_probe,
	.remove = sensor_hub_remove,
	.raw_event = sensor_hub_raw_event,
	.report_fixup = sensor_hub_report_fixup,
#ifdef CONFIG_PM
	.suspend = sensor_hub_suspend,
	.resume = sensor_hub_resume,
	.reset_resume = sensor_hub_reset_resume,
#endif /*CONFIG_PM*/
};
module_hid_driver(sensor_hub_driver);

MODULE_DESCRIPTION("HID Sensor Hub driver");
MODULE_AUTHOR("Srinivas Pandruvada <srinivas.pandruvada@intel.com>");
MODULE_LICENSE("GPL");
