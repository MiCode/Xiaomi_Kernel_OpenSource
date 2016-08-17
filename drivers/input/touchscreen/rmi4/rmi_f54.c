
/*
 * Copyright (c) 2011 Synaptics Incorporated
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <linux/hrtimer.h>
#include <linux/kernel.h>
#include <linux/rmi.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/delay.h>
#include "rmi_driver.h"

/* Set this to 1 for raw hex dump of returned data. */
#define RAW_HEX 0
/* Set this to 1 for human readable dump of returned data. */
#define HUMAN_READABLE 0
/* The watchdog timer can be useful when debugging certain firmware related
 * issues.
 */
#define F54_WATCHDOG 1

#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 32)
#define KERNEL_VERSION_ABOVE_2_6_32 1
#endif

/* define fn $54 commands */
#define GET_REPORT                1
#define FORCE_CAL                 2

/* status */
#define BUSY 1
#define IDLE 0

/* Offsets for data */
#define RMI_F54_REPORT_DATA_OFFSET	3
#define RMI_F54_FIFO_OFFSET		1
#define RMI_F54_NUM_TX_OFFSET		1
#define RMI_F54_NUM_RX_OFFSET		0

/* Fixed sizes of reports */
#define RMI_54_FULL_RAW_CAP_MIN_MAX_SIZE 4
#define RMI_54_HIGH_RESISTANCE_SIZE 6

/* definitions for F54 Query Registers in ultra-portable unionstruct form */
struct f54_ad_query {
	/* query 0 */
	u8 number_of_receiver_electrodes;

	/* query 1 */
	u8 number_of_transmitter_electrodes;

	union {
		struct {
			/* query2 */
			u8 f54_ad_query2_b0__1:2;
			u8 has_baseline:1;
			u8 has_image8:1;
			u8 f54_ad_query2_b4__5:2;
			u8 has_image16:1;
			u8 f54_ad_query2_b7:1;
		};
		u8 f54_ad_query2;
	};

	/* query 3.0 and 3.1 */
	u16 clock_rate;

	/* query 4 */
	u8 touch_controller_family;

	/* query 5 */
	union {
		struct {
			u8 has_pixel_touch_threshold_adjustment:1;
			u8 f54_ad_query5_b1__7:7;
		};
		u8 f54_ad_query5;
	};

	/* query 6 */
	union {
		struct {
		u8 has_sensor_assignment:1;
		u8 has_interference_metric:1;
		u8 has_sense_frequency_control:1;
		u8 has_firmware_noise_mitigation:1;
		u8 f54_ad_query6_b4:1;
		u8 has_two_byte_report_rate:1;
		u8 has_one_byte_report_rate:1;
		u8 has_relaxation_control:1;
		};
		u8 f54_ad_query6;
	};

	/* query 7 */
	union {
		struct {
			u8 curve_compensation_mode:2;
			u8 f54_ad_query7_b2__7:6;
		};
		u8 f54_ad_query7;
	};

	/* query 8 */
	union {
		struct {
		u8 f54_ad_query2_b0:1;
		u8 has_iir_filter:1;
		u8 has_cmn_removal:1;
		u8 has_cmn_maximum:1;
		u8 has_pixel_threshold_hysteresis:1;
		u8 has_edge_compensation:1;
		u8 has_perf_frequency_noisecontrol:1;
		u8 f54_ad_query8_b7:1;
		};
		u8 f54_ad_query8;
	};

	u8 f54_ad_query9;
	u8 f54_ad_query10;
	u8 f54_ad_query11;

	/* query 12 */
	union {
		struct {
			u8 number_of_sensing_frequencies:4;
			u8 f54_ad_query12_b4__7:4;
		};
		u8 f54_ad_query12;
	};
};

/* define report types */
enum f54_report_types {
	/* The numbering should follow automatically, here for clarity */
	F54_8BIT_IMAGE = 1,
	F54_16BIT_IMAGE = 2,
	F54_RAW_16BIT_IMAGE = 3,
	F54_HIGH_RESISTANCE = 4,
	F54_TX_TO_TX_SHORT = 5,
	F54_RX_TO_RX1 = 7,
	F54_TRUE_BASELINE = 9,
	F54_FULL_RAW_CAP_MIN_MAX = 13,
	F54_RX_OPENS1 = 14,
	F54_TX_OPEN = 15,
	F54_TX_TO_GROUND = 16,
	F54_RX_TO_RX2 = 17,
	F54_RX_OPENS2 = 18,
	F54_FULL_RAW_CAP = 19,
	F54_FULL_RAW_CAP_RX_COUPLING_COMP = 20
};

/* data specific to fn $54 that needs to be kept around */
struct rmi_fn_54_data {
	struct f54_ad_query query;
	u8 cmd;
	enum f54_report_types report_type;
	u16 fifoindex;
	signed char status;
	bool no_auto_cal;
	/*
	 * May need to do something to make sure this reflects what is currently
	 * in data.
	 */
	unsigned int report_size;
	unsigned char *report_data;
	unsigned int bufsize;
	struct mutex data_mutex;
	struct lock_class_key data_key;
	struct mutex status_mutex;
	struct lock_class_key status_key;
#if F54_WATCHDOG
	struct hrtimer watchdog;
#endif
	struct rmi_function_container *fc;
	struct work_struct work;
};

/* sysfs functions */
static ssize_t rmi_fn_54_report_type_show(struct device *dev,
				struct device_attribute *attr, char *buf);

static ssize_t rmi_fn_54_report_type_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

static ssize_t rmi_fn_54_get_report_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count);

static ssize_t rmi_fn_54_force_cal_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count);

static ssize_t rmi_fn_54_status_show(struct device *dev,
				struct device_attribute *attr, char *buf);

#ifdef KERNEL_VERSION_ABOVE_2_6_32
static ssize_t rmi_fn_54_data_read(struct file *data_file, struct kobject *kobj,
#else
static ssize_t rmi_fn_54_data_read(struct kobject *kobj,
#endif
					struct bin_attribute *attributes,
					char *buf, loff_t pos, size_t count);

static ssize_t rmi_fn_54_num_rx_electrodes_show(struct device *dev,
				     struct device_attribute *attr, char *buf);

static ssize_t rmi_fn_54_num_tx_electrodes_show(struct device *dev,
				struct device_attribute *attr, char *buf);

static ssize_t rmi_fn_54_has_image16_show(struct device *dev,
				     struct device_attribute *attr, char *buf);

static ssize_t rmi_fn_54_has_image8_show(struct device *dev,
				struct device_attribute *attr, char *buf);

static ssize_t rmi_fn_54_has_baseline_show(struct device *dev,
				struct device_attribute *attr, char *buf);

static ssize_t rmi_fn_54_clock_rate_show(struct device *dev,
				struct device_attribute *attr, char *buf);


static ssize_t rmi_fn_54_touch_controller_family_show(struct device *dev,
				struct device_attribute *attr, char *buf);


static ssize_t rmi_fn_54_has_pixel_touch_threshold_adjustment_show(
		struct device *dev, struct device_attribute *attr, char *buf);

static ssize_t rmi_fn_54_has_sensor_assignment_show(struct device *dev,
			struct device_attribute *attr, char *buf);

static ssize_t rmi_fn_54_has_interference_metric_show(struct device *dev,
			struct device_attribute *attr, char *buf);

static ssize_t rmi_fn_54_has_sense_frequency_control_show(struct device *dev,
			struct device_attribute *attr, char *buf);

static ssize_t rmi_fn_54_has_firmware_noise_mitigation_show(struct device *dev,
			struct device_attribute *attr, char *buf);

static ssize_t rmi_fn_54_has_two_byte_report_rate_show(struct device *dev,
			struct device_attribute *attr, char *buf);

static ssize_t rmi_fn_54_has_one_byte_report_rate_show(struct device *dev,
			struct device_attribute *attr, char *buf);

static ssize_t rmi_fn_54_has_relaxation_control_show(struct device *dev,
			struct device_attribute *attr, char *buf);

static ssize_t rmi_fn_54_curve_compensation_mode_show(struct device *dev,
			struct device_attribute *attr, char *buf);

static ssize_t rmi_fn_54_has_iir_filter_show(struct device *dev,
			struct device_attribute *attr, char *buf);

static ssize_t rmi_fn_54_has_cmn_removal_show(struct device *dev,
			struct device_attribute *attr, char *buf);

static ssize_t rmi_fn_54_has_cmn_maximum_show(struct device *dev,
			struct device_attribute *attr, char *buf);

static ssize_t rmi_fn_54_has_pixel_threshold_hysteresis_show(struct device *dev,
			struct device_attribute *attr, char *buf);

static ssize_t rmi_fn_54_has_edge_compensation_show(struct device *dev,
			struct device_attribute *attr, char *buf);

static ssize_t rmi_fn_54_has_perf_frequency_noisecontrol_show(
		struct device *dev, struct device_attribute *attr, char *buf);

static ssize_t rmi_fn_54_number_of_sensing_frequencies_show(struct device *dev,
			struct device_attribute *attr, char *buf);

static ssize_t rmi_fn_54_no_auto_cal_show(struct device *dev,
				struct device_attribute *attr, char *buf);

static ssize_t rmi_fn_54_no_auto_cal_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

static ssize_t rmi_fn_54_fifoindex_show(struct device *dev,
				  struct device_attribute *attr, char *buf);

static ssize_t rmi_fn_54_fifoindex_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count);

static struct device_attribute attrs[] = {
	__ATTR(report_type, RMI_RO_ATTR,
		rmi_fn_54_report_type_show, rmi_fn_54_report_type_store),
	__ATTR(get_report, RMI_RO_ATTR,
		rmi_show_error, rmi_fn_54_get_report_store),
	__ATTR(force_cal, RMI_RO_ATTR,
		rmi_show_error, rmi_fn_54_force_cal_store),
	__ATTR(status, RMI_RO_ATTR,
		rmi_fn_54_status_show, rmi_store_error),
	__ATTR(num_rx_electrodes, RMI_RO_ATTR,
		rmi_fn_54_num_rx_electrodes_show, rmi_store_error),
	__ATTR(num_tx_electrodes, RMI_RO_ATTR,
		rmi_fn_54_num_tx_electrodes_show, rmi_store_error),
	__ATTR(has_image16, RMI_RO_ATTR,
		rmi_fn_54_has_image16_show, rmi_store_error),
	__ATTR(has_image8, RMI_RO_ATTR,
		rmi_fn_54_has_image8_show, rmi_store_error),
	__ATTR(has_baseline, RMI_RO_ATTR,
		rmi_fn_54_has_baseline_show, rmi_store_error),
	__ATTR(clock_rate, RMI_RO_ATTR,
		rmi_fn_54_clock_rate_show, rmi_store_error),
	__ATTR(touch_controller_family, RMI_RO_ATTR,
		rmi_fn_54_touch_controller_family_show, rmi_store_error),
	__ATTR(has_pixel_touch_threshold_adjustment, RMI_RO_ATTR,
		rmi_fn_54_has_pixel_touch_threshold_adjustment_show
							, rmi_store_error),
	__ATTR(has_sensor_assignment, RMI_RO_ATTR,
		rmi_fn_54_has_sensor_assignment_show, rmi_store_error),
	__ATTR(has_interference_metric, RMI_RO_ATTR,
		rmi_fn_54_has_interference_metric_show, rmi_store_error),
	__ATTR(has_sense_frequency_control, RMI_RO_ATTR,
		rmi_fn_54_has_sense_frequency_control_show, rmi_store_error),
	__ATTR(has_firmware_noise_mitigation, RMI_RO_ATTR,
		rmi_fn_54_has_firmware_noise_mitigation_show, rmi_store_error),
	__ATTR(has_two_byte_report_rate, RMI_RO_ATTR,
		rmi_fn_54_has_two_byte_report_rate_show, rmi_store_error),
	__ATTR(has_one_byte_report_rate, RMI_RO_ATTR,
		rmi_fn_54_has_one_byte_report_rate_show, rmi_store_error),
	__ATTR(has_relaxation_control, RMI_RO_ATTR,
		rmi_fn_54_has_relaxation_control_show, rmi_store_error),
	__ATTR(curve_compensation_mode, RMI_RO_ATTR,
		rmi_fn_54_curve_compensation_mode_show, rmi_store_error),
	__ATTR(has_iir_filter, RMI_RO_ATTR,
		rmi_fn_54_has_iir_filter_show, rmi_store_error),
	__ATTR(has_cmn_removal, RMI_RO_ATTR,
		rmi_fn_54_has_cmn_removal_show, rmi_store_error),
	__ATTR(has_cmn_maximum, RMI_RO_ATTR,
		rmi_fn_54_has_cmn_maximum_show, rmi_store_error),
	__ATTR(has_pixel_threshold_hysteresis, RMI_RO_ATTR,
		rmi_fn_54_has_pixel_threshold_hysteresis_show, rmi_store_error),
	__ATTR(has_edge_compensation, RMI_RO_ATTR,
		rmi_fn_54_has_edge_compensation_show, rmi_store_error),
	__ATTR(has_perf_frequency_noisecontrol, RMI_RO_ATTR,
	      rmi_fn_54_has_perf_frequency_noisecontrol_show, rmi_store_error),
	__ATTR(number_of_sensing_frequencies, RMI_RO_ATTR,
		rmi_fn_54_number_of_sensing_frequencies_show, rmi_store_error),
	__ATTR(no_auto_cal, RMI_RO_ATTR,
		rmi_fn_54_no_auto_cal_show, rmi_fn_54_no_auto_cal_store),
	__ATTR(fifoindex, RMI_RO_ATTR,
		rmi_fn_54_fifoindex_show, rmi_fn_54_fifoindex_store),
};

struct bin_attribute dev_rep_data = {
	.attr = {
		 .name = "rep_data",
		 .mode = RMI_RO_ATTR},
	.size = 0,
	.read = rmi_fn_54_data_read,
};

#if F54_WATCHDOG
static enum hrtimer_restart clear_status(struct hrtimer *timer);

static void clear_status_worker(struct work_struct *work);
#endif

static int rmi_f54_init(struct rmi_function_container *fc)
{
	struct rmi_fn_54_data *instance_data;
	int retval = 0;
	int attr_count = 0;

	dev_info(&fc->dev, "Intializing F54.");

	instance_data = kzalloc(sizeof(struct rmi_fn_54_data), GFP_KERNEL);
	if (!instance_data) {
		dev_err(&fc->dev, "Failed to allocate rmi_fn_54_data.\n");
		retval = -ENOMEM;
		goto error_exit;
	}
	fc->data = instance_data;
	instance_data->fc = fc;

#if F54_WATCHDOG
	/* Set up watchdog timer to catch unanswered get_report commands */
	hrtimer_init(&instance_data->watchdog, CLOCK_MONOTONIC,
							HRTIMER_MODE_REL);
	instance_data->watchdog.function = clear_status;

	/* work function to do unlocking */
	INIT_WORK(&instance_data->work, clear_status_worker);
#endif

	/* Read F54 Query Data */
	retval = rmi_read_block(fc->rmi_dev, fc->fd.query_base_addr,
		(u8 *)&instance_data->query, sizeof(instance_data->query));
	if (retval < 0) {
		dev_err(&fc->dev, "Could not read query registers"
			" from 0x%04x\n", fc->fd.query_base_addr);
		goto error_exit;
	}

	__mutex_init(&instance_data->data_mutex, "data_mutex",
		     &instance_data->data_key);

	__mutex_init(&instance_data->status_mutex, "status_mutex",
		     &instance_data->status_key);

	dev_dbg(&fc->dev, "Creating sysfs files.");
	/* Set up sysfs device attributes. */
	for (attr_count = 0; attr_count < ARRAY_SIZE(attrs); attr_count++) {
		if (sysfs_create_file
		    (&fc->dev.kobj, &attrs[attr_count].attr) < 0) {
			dev_err(&fc->dev, "Failed to create sysfs file for %s.",
			     attrs[attr_count].attr.name);
			retval = -ENODEV;
			goto error_exit;
		}
	}
	/* Binary sysfs file to report the data back */
	retval = sysfs_create_bin_file(&fc->dev.kobj, &dev_rep_data);
	if (retval < 0) {
		dev_err(&fc->dev, "Failed to create sysfs file for F54 data "
					"(error = %d).\n", retval);
		retval = -ENODEV;
		goto error_exit;
	}
	instance_data->status = IDLE;
	return retval;

error_exit:
	dev_err(&fc->dev, "An error occured in F54 init!\n");
	for (attr_count--; attr_count >= 0; attr_count--)
		sysfs_remove_file(&fc->dev.kobj,
				  &attrs[attr_count].attr);
	kfree(instance_data);
	return retval;
}

static void set_report_size(struct rmi_fn_54_data *data)
{
	u8 rx = data->query.number_of_receiver_electrodes;
	u8 tx = data->query.number_of_transmitter_electrodes;
	switch (data->report_type) {
	case F54_8BIT_IMAGE:
		data->report_size = rx * tx;
		break;
	case F54_16BIT_IMAGE:
	case F54_RAW_16BIT_IMAGE:
	case F54_TRUE_BASELINE:
	case F54_FULL_RAW_CAP:
	case F54_FULL_RAW_CAP_RX_COUPLING_COMP:
		data->report_size = 2 * rx * tx;
		break;
	case F54_HIGH_RESISTANCE:
		data->report_size = RMI_54_HIGH_RESISTANCE_SIZE;
		break;
	case F54_FULL_RAW_CAP_MIN_MAX:
		data->report_size = RMI_54_FULL_RAW_CAP_MIN_MAX_SIZE;
		break;
	case F54_TX_TO_TX_SHORT:
	case F54_TX_OPEN:
	case F54_TX_TO_GROUND:
		data->report_size =  (tx + 7) / 8;
		break;
	case F54_RX_TO_RX1:
	case F54_RX_OPENS1:
		if (rx < tx)
			data->report_size = 2 * rx * rx;
		else
			data->report_size = 2 * rx * tx;
		break;
	case F54_RX_TO_RX2:
	case F54_RX_OPENS2:
		if (rx <= tx)
			data->report_size = 0;
		else
			data->report_size = 2 * rx * (rx - tx);
		break;
	default:
		data->report_size = 0;
	}
}

int rmi_f54_attention(struct rmi_function_container *fc, u8 *irq_bits)
{
	struct rmi_driver *driver = fc->rmi_dev->driver;
	char fifo[2];
	struct rmi_fn_54_data *data = fc->data;
	int error = 0;

	set_report_size(data);
	if (data->report_size == 0) {
		dev_err(&fc->dev, "Invalid report type set in %s. "
				"This should never happen.\n", __func__);
		error = -EINVAL;
		goto error_exit;
	}
	/*
	 * We need to ensure the buffer is big enough. A Buffer size of 0 means
	 * that the buffer has not been allocated.
	 */
	if (data->bufsize < data->report_size) {
		mutex_lock(&data->data_mutex);
		if (data->bufsize > 0)
			kfree(data->report_data);
		data->report_data = kzalloc(data->report_size, GFP_KERNEL);
		if (!data->report_data) {
			dev_err(&fc->dev, "Failed to allocate report_data.\n");
			error = -ENOMEM;
			data->bufsize = 0;
			mutex_unlock(&data->data_mutex);
			goto error_exit;
		}
		data->bufsize = data->report_size;
		mutex_unlock(&data->data_mutex);
	}
	dev_vdbg(&fc->dev, "F54 Interrupt handler is running.\nSize: %d\n",
		 data->report_size);
	/*
	 * Read report type, fifo high, and fifo low
	 * error = rmi_read_multiple(rmifninfo->sensor,
	 *	rmifninfo->function_descriptor.data_base_addr ,
	 *	repfifo,3);
	 */
	/* Write 0 to fifohi and fifolo. */
	fifo[0] = 0;
	fifo[1] = 0;
	error = rmi_write_block(fc->rmi_dev, fc->fd.data_base_addr
				+ RMI_F54_FIFO_OFFSET, fifo,	sizeof(fifo));
	if (error < 0)
		dev_err(&fc->dev, "Failed to write fifo to zero!\n");
	else
		error = rmi_read_block(fc->rmi_dev,
			fc->fd.data_base_addr + RMI_F54_REPORT_DATA_OFFSET,
			data->report_data, data->report_size);
	if (error < 0)
		dev_err(&fc->dev, "F54 data read failed. Code: %d.\n", error);
	else if (error != data->report_size) {
		error = -EINVAL;
		goto error_exit;
	}
#if RAW_HEX
	int l;
	/* Debugging: Print out the file in hex. */
	pr_info("Report data (raw hex):\n");
	for (l = 0; l < data->report_size; l += 2) {
		pr_info("%03d: 0x%02x%02x\n", l/2,
			data->report_data[l+1], data->report_data[l]);
	}
#endif
#if HUMAN_READABLE
	/* Debugging: Print out file in human understandable image */
	switch (data->report_type) {
	case F54_16BIT_IMAGE:
	case F54_RAW_16BIT_IMAGE:
	case F54_TRUE_BASELINE:
	case F54_FULL_RAW_CAP:
	case F54_FULL_RAW_CAP_RX_COUPLING_COMP:
		pr_info("Report data (Image):\n");
		int i, j, k;
		char c[2];
		short s;
		k = 0;
		for (i = 0; i < data->query.number_of_transmitter_electrodes;
									i++) {
			for (j = 0; j <
			     data->query.number_of_receiver_electrodes; j++) {
				c[0] = data->report_data[k];
				c[1] = data->report_data[k+1];
				memcpy(&s, &c, 2);
				if (s < -64)
					printk(".");
				else if (s < 0)
					printk("-");
				else if (s > 64)
					printk("*");
				else if (s > 0)
					printk("+");
				else
					printk("0");
				k += 2;
			}
			pr_info("\n");
		}
		pr_info("EOF\n");
		break;
	default:
		pr_info("Report type %d debug image not supported",
							data->report_type);
	}
#endif
	error = IDLE;
error_exit:
	mutex_lock(&data->status_mutex);
	/* Turn back on other interupts, if it
	 * appears that we turned them off. */
	if (driver->restore_irq_mask) {
		dev_dbg(&fc->dev, "Restoring interupts!\n");
		driver->restore_irq_mask(fc->rmi_dev);
	} else {
		dev_err(&fc->dev, "No way to restore interrupts!\n");
	}
	data->status = error;
	mutex_unlock(&data->status_mutex);
	return data->status;
}


#if F54_WATCHDOG
static void clear_status_worker(struct work_struct *work)
{
	struct rmi_fn_54_data *data = container_of(work,
					struct rmi_fn_54_data, work);
	struct rmi_function_container *fc = data->fc;
	struct rmi_driver *driver = fc->rmi_dev->driver;
	char command;
	int result;

	mutex_lock(&data->status_mutex);
	if (data->status == BUSY) {
		pr_info("F54 Timout Occured: Determining status.\n");
		result = rmi_read_block(fc->rmi_dev, fc->fd.command_base_addr,
								&command, 1);
		if (result < 0) {
			dev_err(&fc->dev, "Could not read get_report register "
				"from 0x%04x\n", fc->fd.command_base_addr);
			data->status = -ETIMEDOUT;
		} else {
			if (command & GET_REPORT) {
				dev_warn(&fc->dev, "Report type unsupported!");
				data->status = -EINVAL;
			} else {
				data->status = -ETIMEDOUT;
			}
		}
		if (driver->restore_irq_mask) {
			dev_dbg(&fc->dev, "Restoring interupts!\n");
			driver->restore_irq_mask(fc->rmi_dev);
		} else {
			dev_err(&fc->dev, "No way to restore interrupts!\n");
		}
	}
	mutex_unlock(&data->status_mutex);
}

static enum hrtimer_restart clear_status(struct hrtimer *timer)
{
	struct rmi_fn_54_data *data = container_of(timer,
					struct rmi_fn_54_data, watchdog);
	schedule_work(&(data->work));
	return HRTIMER_NORESTART;
}
#endif

/* Check if report_type is valid */
static bool is_report_type_valid(enum f54_report_types reptype)
{
	/* Basic checks on report_type to ensure we write a valid type
	 * to the sensor.
	 * TODO: Check Query3 to see if some specific reports are
	 * available. This is currently listed as a reserved register.
	 */
	switch (reptype) {
	case F54_8BIT_IMAGE:
	case F54_16BIT_IMAGE:
	case F54_RAW_16BIT_IMAGE:
	case F54_HIGH_RESISTANCE:
	case F54_TX_TO_TX_SHORT:
	case F54_RX_TO_RX1:
	case F54_TRUE_BASELINE:
	case F54_FULL_RAW_CAP_MIN_MAX:
	case F54_RX_OPENS1:
	case F54_TX_OPEN:
	case F54_TX_TO_GROUND:
	case F54_RX_TO_RX2:
	case F54_RX_OPENS2:
	case F54_FULL_RAW_CAP:
	case F54_FULL_RAW_CAP_RX_COUPLING_COMP:
		return true;
		break;
	default:
		return false;
	}
}

/* SYSFS file show/store functions */
static ssize_t rmi_fn_54_report_type_show(struct device *dev,
				struct device_attribute *attr, char *buf) {
	struct rmi_function_container *fc;
	struct rmi_fn_54_data *instance_data;

	fc = to_rmi_function_container(dev);
	instance_data = fc->data;

	return snprintf(buf, PAGE_SIZE, "%u\n", instance_data->report_type);
}

static ssize_t rmi_fn_54_report_type_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count) {
	int result;
	unsigned long val;
	unsigned char data;
	struct rmi_function_container *fc;
	struct rmi_fn_54_data *instance_data;
	fc = to_rmi_function_container(dev);
	instance_data = fc->data;

	/* need to convert the string data to an actual value */
	result = strict_strtoul(buf, 10, &val);
	if (result)
		return result;
	if (!is_report_type_valid(val)) {
		dev_err(dev, "%s : Report type %d is invalid.\n",
					__func__, (u8) val);
		return -EINVAL;
	}
	mutex_lock(&instance_data->status_mutex);
	if (instance_data->status != BUSY) {
		instance_data->report_type = (enum f54_report_types)val;
		data = (char)val;
		/* Write the Report Type back to the first Block
		 * Data registers (F54_AD_Data0). */
		result =
		    rmi_write_block(fc->rmi_dev, fc->fd.data_base_addr,
								&data, 1);
		mutex_unlock(&instance_data->status_mutex);
		if (result < 0) {
			dev_err(dev, "%s : Could not write report type to"
				" 0x%x\n", __func__, fc->fd.data_base_addr);
			return result;
		}
		return count;
	} else {
		dev_err(dev, "%s : Report type cannot be changed in the middle"
				" of command.\n", __func__);
		mutex_unlock(&instance_data->status_mutex);
		return -EINVAL;
	}
}

static ssize_t rmi_fn_54_get_report_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count) {
	unsigned long val;
	int error, result;
	struct rmi_function_container *fc;
	struct rmi_fn_54_data *instance_data;
	struct rmi_driver *driver;
	u8 command;
	fc = to_rmi_function_container(dev);
	instance_data = fc->data;
	driver = fc->rmi_dev->driver;

	/* need to convert the string data to an actual value */
	error = strict_strtoul(buf, 10, &val);
	if (error)
		return error;
	/* Do nothing if not set to 1. This prevents accidental commands. */
	if (val != 1)
		return count;
	command = (unsigned char)GET_REPORT;
	/* Basic checks on report_type to ensure we write a valid type
	 * to the sensor.
	 * TODO: Check Query3 to see if some specific reports are
	 * available. This is currently listed as a reserved register.
	 */
	if (!is_report_type_valid(instance_data->report_type)) {
		dev_err(dev, "%s : Report type %d is invalid.\n",
				__func__, instance_data->report_type);
		return -EINVAL;
	}
	mutex_lock(&instance_data->status_mutex);
	if (instance_data->status != IDLE) {
		if (instance_data->status != BUSY) {
			dev_err(dev, "F54 status is in an abnormal state: 0x%x",
							instance_data->status);
		}
		mutex_unlock(&instance_data->status_mutex);
		return count;
	}
	/* Store interrupts */
	/* Do not exit if we fail to turn off interupts. We are likely
	 * to still get useful data. The report data can, however, be
	 * corrupted, and there may be unexpected behavior.
	 */
	dev_dbg(dev, "Storing and overriding interupts\n");
	if (driver->store_irq_mask)
		driver->store_irq_mask(fc->rmi_dev,
					fc->irq_mask);
	else
		dev_err(dev, "No way to store interupts!\n");
	instance_data->status = BUSY;

	/* small delay to avoid race condition in firmare. This value is a bit
	 * higher than absolutely necessary. Should be removed once issue is
	 * resolved in firmware. */

	mdelay(2);

	/* Write the command to the command register */
	result = rmi_write_block(fc->rmi_dev, fc->fd.command_base_addr,
						&command, 1);
	mutex_unlock(&instance_data->status_mutex);
	if (result < 0) {
		dev_err(dev, "%s : Could not write command to 0x%x\n",
				__func__, fc->fd.command_base_addr);
		return result;
	}
#if F54_WATCHDOG
	/* start watchdog timer */
	hrtimer_start(&instance_data->watchdog, ktime_set(1, 0),
							HRTIMER_MODE_REL);
#endif
	return count;
}

static ssize_t rmi_fn_54_force_cal_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count) {
	unsigned long val;
	int error, result;
	struct rmi_function_container *fc;
	struct rmi_fn_54_data *instance_data;
	struct rmi_driver *driver;
	u8 command;

	fc = to_rmi_function_container(dev);
	instance_data = fc->data;
	driver = fc->rmi_dev->driver;

	/* need to convert the string data to an actual value */
	error = strict_strtoul(buf, 10, &val);
	if (error)
		return error;
	/* Do nothing if not set to 1. This prevents accidental commands. */
	if (val != 1)
		return count;

	command = (unsigned char)FORCE_CAL;

	if (instance_data->status == BUSY)
		return -EBUSY;
	/* Write the command to the command register */
	result = rmi_write_block(fc->rmi_dev, fc->fd.command_base_addr,
						&command, 1);
	if (result < 0) {
		dev_err(dev, "%s : Could not write command to 0x%x\n",
				__func__, fc->fd.command_base_addr);
		return result;
	}
	return count;
}

static ssize_t rmi_fn_54_status_show(struct device *dev,
				struct device_attribute *attr, char *buf) {
	struct rmi_function_container *fc;
	struct rmi_fn_54_data *instance_data;

	fc = to_rmi_function_container(dev);
	instance_data = fc->data;

	return snprintf(buf, PAGE_SIZE, "%d\n", instance_data->status);
}

static ssize_t rmi_fn_54_num_rx_electrodes_show(struct device *dev,
				     struct device_attribute *attr, char *buf) {
	struct rmi_function_container *fc;
	struct rmi_fn_54_data *data;

	fc = to_rmi_function_container(dev);
	data = fc->data;

	return snprintf(buf, PAGE_SIZE, "%u\n",
				data->query.number_of_receiver_electrodes);
}

static ssize_t rmi_fn_54_num_tx_electrodes_show(struct device *dev,
				struct device_attribute *attr, char *buf) {
	struct rmi_function_container *fc;
	struct rmi_fn_54_data *data;

	fc = to_rmi_function_container(dev);
	data = fc->data;

	return snprintf(buf, PAGE_SIZE, "%u\n",
			data->query.number_of_transmitter_electrodes);
}

static ssize_t rmi_fn_54_has_image16_show(struct device *dev,
				struct device_attribute *attr, char *buf) {
	struct rmi_function_container *fc;
	struct rmi_fn_54_data *data;

	fc = to_rmi_function_container(dev);
	data = fc->data;

	return snprintf(buf, PAGE_SIZE, "%u\n",
			data->query.has_image16);
}

static ssize_t rmi_fn_54_has_image8_show(struct device *dev,
				struct device_attribute *attr, char *buf) {
	struct rmi_function_container *fc;
	struct rmi_fn_54_data *data;

	fc = to_rmi_function_container(dev);
	data = fc->data;

	return snprintf(buf, PAGE_SIZE, "%u\n",
			data->query.has_image8);
}

static ssize_t rmi_fn_54_has_baseline_show(struct device *dev,
				struct device_attribute *attr, char *buf) {
	struct rmi_function_container *fc;
	struct rmi_fn_54_data *data;

	fc = to_rmi_function_container(dev);
	data = fc->data;

	return snprintf(buf, PAGE_SIZE, "%u\n",
			data->query.has_baseline);
}

static ssize_t rmi_fn_54_clock_rate_show(struct device *dev,
				struct device_attribute *attr, char *buf) {
	struct rmi_function_container *fc;
	struct rmi_fn_54_data *data;

	fc = to_rmi_function_container(dev);
	data = fc->data;

	return snprintf(buf, PAGE_SIZE, "%u\n",
			data->query.clock_rate);
}


static ssize_t rmi_fn_54_touch_controller_family_show(struct device *dev,
				struct device_attribute *attr, char *buf) {
	struct rmi_function_container *fc;
	struct rmi_fn_54_data *data;

	fc = to_rmi_function_container(dev);
	data = fc->data;

	return snprintf(buf, PAGE_SIZE, "%u\n",
			data->query.touch_controller_family);
}


static ssize_t rmi_fn_54_has_pixel_touch_threshold_adjustment_show(
		struct device *dev, struct device_attribute *attr, char *buf) {
	struct rmi_function_container *fc;
	struct rmi_fn_54_data *data;

	fc = to_rmi_function_container(dev);
	data = fc->data;

	return snprintf(buf, PAGE_SIZE, "%u\n",
			data->query.has_pixel_touch_threshold_adjustment);
}

static ssize_t rmi_fn_54_has_sensor_assignment_show(struct device *dev,
				struct device_attribute *attr, char *buf) {
	struct rmi_function_container *fc;
	struct rmi_fn_54_data *data;

	fc = to_rmi_function_container(dev);
	data = fc->data;

	return snprintf(buf, PAGE_SIZE, "%u\n",
			data->query.has_sensor_assignment);
}

static ssize_t rmi_fn_54_has_interference_metric_show(struct device *dev,
				struct device_attribute *attr, char *buf) {
	struct rmi_function_container *fc;
	struct rmi_fn_54_data *data;

	fc = to_rmi_function_container(dev);
	data = fc->data;

	return snprintf(buf, PAGE_SIZE, "%u\n",
			data->query.has_interference_metric);
}

static ssize_t rmi_fn_54_has_sense_frequency_control_show(struct device *dev,
				struct device_attribute *attr, char *buf) {
	struct rmi_function_container *fc;
	struct rmi_fn_54_data *data;

	fc = to_rmi_function_container(dev);
	data = fc->data;

	return snprintf(buf, PAGE_SIZE, "%u\n",
			data->query.has_sense_frequency_control);
}

static ssize_t rmi_fn_54_has_firmware_noise_mitigation_show(struct device *dev,
				struct device_attribute *attr, char *buf) {
	struct rmi_function_container *fc;
	struct rmi_fn_54_data *data;

	fc = to_rmi_function_container(dev);
	data = fc->data;

	return snprintf(buf, PAGE_SIZE, "%u\n",
			data->query.has_firmware_noise_mitigation);
}

static ssize_t rmi_fn_54_has_two_byte_report_rate_show(struct device *dev,
				struct device_attribute *attr, char *buf) {
	struct rmi_function_container *fc;
	struct rmi_fn_54_data *data;

	fc = to_rmi_function_container(dev);
	data = fc->data;

	return snprintf(buf, PAGE_SIZE, "%u\n",
			data->query.has_two_byte_report_rate);
}

static ssize_t rmi_fn_54_has_one_byte_report_rate_show(struct device *dev,
				struct device_attribute *attr, char *buf) {
	struct rmi_function_container *fc;
	struct rmi_fn_54_data *data;

	fc = to_rmi_function_container(dev);
	data = fc->data;

	return snprintf(buf, PAGE_SIZE, "%u\n",
			data->query.has_one_byte_report_rate);
}

static ssize_t rmi_fn_54_has_relaxation_control_show(struct device *dev,
				struct device_attribute *attr, char *buf) {
	struct rmi_function_container *fc;
	struct rmi_fn_54_data *data;

	fc = to_rmi_function_container(dev);
	data = fc->data;

	return snprintf(buf, PAGE_SIZE, "%u\n",
			data->query.has_relaxation_control);
}

static ssize_t rmi_fn_54_curve_compensation_mode_show(struct device *dev,
				struct device_attribute *attr, char *buf) {
	struct rmi_function_container *fc;
	struct rmi_fn_54_data *data;

	fc = to_rmi_function_container(dev);
	data = fc->data;

	return snprintf(buf, PAGE_SIZE, "%u\n",
			data->query.curve_compensation_mode);
}

static ssize_t rmi_fn_54_has_iir_filter_show(struct device *dev,
				struct device_attribute *attr, char *buf) {
	struct rmi_function_container *fc;
	struct rmi_fn_54_data *data;

	fc = to_rmi_function_container(dev);
	data = fc->data;

	return snprintf(buf, PAGE_SIZE, "%u\n",
			data->query.has_iir_filter);
}

static ssize_t rmi_fn_54_has_cmn_removal_show(struct device *dev,
				struct device_attribute *attr, char *buf) {
	struct rmi_function_container *fc;
	struct rmi_fn_54_data *data;

	fc = to_rmi_function_container(dev);
	data = fc->data;

	return snprintf(buf, PAGE_SIZE, "%u\n",
			data->query.has_cmn_removal);
}

static ssize_t rmi_fn_54_has_cmn_maximum_show(struct device *dev,
				struct device_attribute *attr, char *buf) {
	struct rmi_function_container *fc;
	struct rmi_fn_54_data *data;

	fc = to_rmi_function_container(dev);
	data = fc->data;

	return snprintf(buf, PAGE_SIZE, "%u\n",
			data->query.has_cmn_maximum);
}

static ssize_t rmi_fn_54_has_pixel_threshold_hysteresis_show(struct device *dev,
				struct device_attribute *attr, char *buf) {
	struct rmi_function_container *fc;
	struct rmi_fn_54_data *data;

	fc = to_rmi_function_container(dev);
	data = fc->data;

	return snprintf(buf, PAGE_SIZE, "%u\n",
			data->query.has_pixel_threshold_hysteresis);
}

static ssize_t rmi_fn_54_has_edge_compensation_show(struct device *dev,
				struct device_attribute *attr, char *buf) {
	struct rmi_function_container *fc;
	struct rmi_fn_54_data *data;

	fc = to_rmi_function_container(dev);
	data = fc->data;

	return snprintf(buf, PAGE_SIZE, "%u\n",
			data->query.has_edge_compensation);
}

static ssize_t rmi_fn_54_has_perf_frequency_noisecontrol_show(
		struct device *dev, struct device_attribute *attr, char *buf) {
	struct rmi_function_container *fc;
	struct rmi_fn_54_data *data;

	fc = to_rmi_function_container(dev);
	data = fc->data;

	return snprintf(buf, PAGE_SIZE, "%u\n",
			data->query.has_perf_frequency_noisecontrol);
}

static ssize_t rmi_fn_54_number_of_sensing_frequencies_show(struct device *dev,
				struct device_attribute *attr, char *buf) {
	struct rmi_function_container *fc;
	struct rmi_fn_54_data *data;

	fc = to_rmi_function_container(dev);
	data = fc->data;

	return snprintf(buf, PAGE_SIZE, "%u\n",
			data->query.number_of_sensing_frequencies);
}


static ssize_t rmi_fn_54_no_auto_cal_show(struct device *dev,
				struct device_attribute *attr, char *buf) {
	struct rmi_function_container *fc;
	struct rmi_fn_54_data *instance_data;

	fc = to_rmi_function_container(dev);
	instance_data = fc->data;

	return snprintf(buf, PAGE_SIZE, "%u\n",
				instance_data->no_auto_cal ? 1 : 0);
}

static ssize_t rmi_fn_54_no_auto_cal_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count) {
	int result;
	unsigned long val;
	unsigned char data;
	struct rmi_function_container *fc;
	struct rmi_fn_54_data *instance_data;

	fc = to_rmi_function_container(dev);
	instance_data = fc->data;

	/* need to convert the string data to an actual value */
	result = strict_strtoul(buf, 10, &val);

	/* if an error occured, return it */
	if (result)
		return result;
	/* Do nothing if not 0 or 1. This prevents accidental commands. */
	if (val > 1)
		return count;
	/* Read current control values */
	result =
	    rmi_read_block(fc->rmi_dev, fc->fd.control_base_addr, &data, 1);

	/* if the current control registers are already set as we want them, do
	 * nothing to them */
	if ((data & 1) == val)
		return count;
	/* Write the control back to the control register (F54_AD_Ctrl0)
	 * Ignores everything but bit 0 */
	data = (data & ~1) | (val & 0x01); /* bit mask for lowest bit */
	result =
	    rmi_write_block(fc->rmi_dev, fc->fd.control_base_addr, &data, 1);
	if (result < 0) {
		dev_err(dev, "%s : Could not write control to 0x%x\n",
		       __func__, fc->fd.control_base_addr);
		return result;
	}
	/* update our internal representation iff the write succeeds */
	instance_data->no_auto_cal = (val == 1);
	return count;
}

static ssize_t rmi_fn_54_fifoindex_show(struct device *dev,
				  struct device_attribute *attr, char *buf) {
	struct rmi_function_container *fc;
	struct rmi_fn_54_data *instance_data;
	struct rmi_driver *driver;
	unsigned char temp_buf[2];
	int retval;

	fc = to_rmi_function_container(dev);
	instance_data = fc->data;
	driver = fc->rmi_dev->driver;

	/* Read fifoindex from device */
	retval = rmi_read_block(fc->rmi_dev,
				fc->fd.data_base_addr + RMI_F54_FIFO_OFFSET,
				temp_buf, ARRAY_SIZE(temp_buf));

	if (retval < 0) {
		dev_err(dev, "Could not read fifoindex from 0x%04x\n",
		       fc->fd.data_base_addr + RMI_F54_FIFO_OFFSET);
		return retval;
	}
	batohs(&instance_data->fifoindex, temp_buf);
	return snprintf(buf, PAGE_SIZE, "%u\n", instance_data->fifoindex);
}
static ssize_t rmi_fn_54_fifoindex_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf,
					size_t count)
{
	int error;
	unsigned long val;
	unsigned char data[2];
	struct rmi_function_container *fc;
	struct rmi_fn_54_data *instance_data;

	fc = to_rmi_function_container(dev);
	instance_data = fc->data;

	/* need to convert the string data to an actual value */
	error = strict_strtoul(buf, 10, &val);

	if (error)
		return error;

	instance_data->fifoindex = val;

	/* Write the FifoIndex back to the first data registers. */
	hstoba(data, (unsigned short)val);

	error = rmi_write_block(fc->rmi_dev,
				fc->fd.data_base_addr + RMI_F54_FIFO_OFFSET,
				data,
				ARRAY_SIZE(data));

	if (error < 0) {
		dev_err(dev, "%s : Could not write fifoindex to 0x%x\n",
		       __func__, fc->fd.data_base_addr + RMI_F54_FIFO_OFFSET);
		return error;
	}
	return count;
}

/* Provide access to last report */
#ifdef KERNEL_VERSION_ABOVE_2_6_32
static ssize_t rmi_fn_54_data_read(struct file *data_file, struct kobject *kobj,
#else
static ssize_t rmi_fn_54_data_read(struct kobject *kobj,
#endif
				struct bin_attribute *attributes,
				char *buf, loff_t pos, size_t count)
{
	struct device *dev;
	struct rmi_function_container *fc;
	struct rmi_fn_54_data *instance_data;

	dev = container_of(kobj, struct device, kobj);
	fc = to_rmi_function_container(dev);
	instance_data = fc->data;
	mutex_lock(&instance_data->data_mutex);
	if (count < instance_data->report_size) {
		dev_err(dev,
			"%s: F54 report size too large for buffer: %d."
				" Need at least: %d for Report type: %d.\n",
			__func__, count, instance_data->report_size,
			instance_data->report_type);
		mutex_unlock(&instance_data->data_mutex);
		return -EINVAL;
	}
	if (instance_data->report_data) {
		/* Copy data from instance_data to buffer */
		memcpy(buf, instance_data->report_data,
					instance_data->report_size);
		mutex_unlock(&instance_data->data_mutex);
		dev_dbg(dev, "%s: Presumably successful.", __func__);
		return instance_data->report_size;
	} else {
		dev_err(dev, "%s: F54 report_data does not exist!\n", __func__);
		mutex_unlock(&instance_data->data_mutex);
		return -EINVAL;
	}
}

static struct rmi_function_handler function_handler = {
	.func = 0x54,
	.init = rmi_f54_init,
	.attention = rmi_f54_attention
};

static int __init rmi_f54_module_init(void)
{
	int error;

	error = rmi_register_function_driver(&function_handler);
	if (error < 0) {
		pr_err("%s: register failed!\n", __func__);
		return error;
	}
	return 0;
}

static void rmi_f54_module_exit(void)
{
	rmi_unregister_function_driver(&function_handler);
}

module_init(rmi_f54_module_init);
module_exit(rmi_f54_module_exit);

MODULE_AUTHOR("Daniel Rosenberg <daniel.rosenberg@synaptics.com>");
MODULE_DESCRIPTION("RMI F54 module");
MODULE_LICENSE("GPL");
