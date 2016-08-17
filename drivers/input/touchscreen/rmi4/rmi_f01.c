/*
 * Copyright (c) 2011 Synaptics Incorporated
 * Copyright (c) 2011 Unixphere
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

#define DEBUG

#include <linux/kernel.h>
#include <linux/rmi.h>
#include <linux/slab.h>
#include "rmi_driver.h"

/* control register bits */
#define RMI_SLEEP_MODE_NORMAL (0x00)
#define RMI_SLEEP_MODE_SENSOR_SLEEP (0x01)
#define RMI_SLEEP_MODE_RESERVED0 (0x02)
#define RMI_SLEEP_MODE_RESERVED1 (0x03)

#define RMI_IS_VALID_SLEEPMODE(mode) \
	(mode >= RMI_SLEEP_MODE_NORMAL && mode <= RMI_SLEEP_MODE_RESERVED1)

union f01_device_commands {
	struct {
		u8 reset:1;
		u8 reserved:1;
	};
	u8 reg;
};

union f01_device_control {
	struct {
		u8 sleep_mode:2;
		u8 nosleep:1;
		u8 reserved:2;
		u8 charger_input:1;
		u8 report_rate:1;
		u8 configured:1;
	};
	u8 reg;
};

union f01_device_status {
	struct {
		u8 status_code:4;
		u8 reserved:2;
		u8 flash_prog:1;
		u8 unconfigured:1;
	};
	u8 reg;
};

union f01_basic_queries {
	struct {
		u8 manufacturer_id:8;

		u8 custom_map:1;
		u8 non_compliant:1;
		u8 q1_bit_2:1;
		u8 has_sensor_id:1;
		u8 has_charger_input:1;
		u8 has_adjustable_doze:1;
		u8 has_adjustable_doze_holdoff:1;
		u8 q1_bit_7:1;

		u8 productinfo_1:7;
		u8 q2_bit_7:1;
		u8 productinfo_2:7;
		u8 q3_bit_7:1;

		u8 year:5;
		u8 month:4;
		u8 day:5;
		u8 cp1:1;
		u8 cp2:1;
		u8 wafer_id1_lsb:8;
		u8 wafer_id1_msb:8;
		u8 wafer_id2_lsb:8;
		u8 wafer_id2_msb:8;
		u8 wafer_id3_lsb:8;
	};
	u8 regs[11];
};

struct f01_data {
	union f01_device_control device_control;
	union f01_basic_queries basic_queries;
	union f01_device_status device_status;
	u8 product_id[RMI_PRODUCT_ID_LENGTH+1];

#ifdef	CONFIG_PM
	bool suspended;
	bool old_nosleep;
#endif
};


static ssize_t rmi_fn_01_productinfo_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf);

static ssize_t rmi_fn_01_productid_show(struct device *dev,
					struct device_attribute *attr,
					char *buf);

static ssize_t rmi_fn_01_manufacturer_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf);

static ssize_t rmi_fn_01_datecode_show(struct device *dev,
				       struct device_attribute *attr,
				       char *buf);

static ssize_t rmi_fn_01_reportrate_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf);

static ssize_t rmi_fn_01_reportrate_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count);

static ssize_t rmi_fn_01_reset_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count);

static ssize_t rmi_fn_01_sleepmode_show(struct device *dev,
					struct device_attribute *attr,
					char *buf);

static ssize_t rmi_fn_01_sleepmode_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count);

static ssize_t rmi_fn_01_nosleep_show(struct device *dev,
				      struct device_attribute *attr,
				      char *buf);

static ssize_t rmi_fn_01_nosleep_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count);

static ssize_t rmi_fn_01_chargerinput_show(struct device *dev,
				      struct device_attribute *attr,
				      char *buf);

static ssize_t rmi_fn_01_chargerinput_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count);

static ssize_t rmi_fn_01_configured_show(struct device *dev,
				      struct device_attribute *attr,
				      char *buf);

static ssize_t rmi_fn_01_unconfigured_show(struct device *dev,
				      struct device_attribute *attr,
				      char *buf);

static ssize_t rmi_fn_01_flashprog_show(struct device *dev,
				      struct device_attribute *attr,
				      char *buf);

static ssize_t rmi_fn_01_statuscode_show(struct device *dev,
				      struct device_attribute *attr,
				      char *buf);

static struct device_attribute fn_01_attrs[] = {
	__ATTR(productinfo, RMI_RO_ATTR,
	       rmi_fn_01_productinfo_show, rmi_store_error),
	__ATTR(productid, RMI_RO_ATTR,
	       rmi_fn_01_productid_show, rmi_store_error),
	__ATTR(manufacturer, RMI_RO_ATTR,
	       rmi_fn_01_manufacturer_show, rmi_store_error),
	__ATTR(datecode, RMI_RO_ATTR,
	       rmi_fn_01_datecode_show, rmi_store_error),

	/* control register access */
	__ATTR(sleepmode, RMI_RO_ATTR,
	       rmi_fn_01_sleepmode_show, rmi_fn_01_sleepmode_store),
	__ATTR(nosleep, RMI_RO_ATTR,
	       rmi_fn_01_nosleep_show, rmi_fn_01_nosleep_store),
	__ATTR(chargerinput, RMI_RO_ATTR,
	       rmi_fn_01_chargerinput_show, rmi_fn_01_chargerinput_store),
	__ATTR(reportrate, RMI_RO_ATTR,
	       rmi_fn_01_reportrate_show, rmi_fn_01_reportrate_store),
	/* We make report rate RO, since the driver uses that to look for
	 * resets.  We don't want someone faking us out by changing that
	 * bit.
	 */
	__ATTR(configured, RMI_RO_ATTR,
	       rmi_fn_01_configured_show, rmi_store_error),

	/* Command register access. */
	__ATTR(reset, RMI_RO_ATTR,
	       rmi_show_error, rmi_fn_01_reset_store),

	/* STatus register access. */
	__ATTR(unconfigured, RMI_RO_ATTR,
	       rmi_fn_01_unconfigured_show, rmi_store_error),
	__ATTR(flashprog, RMI_RO_ATTR,
	       rmi_fn_01_flashprog_show, rmi_store_error),
	__ATTR(statuscode, RMI_RO_ATTR,
	       rmi_fn_01_statuscode_show, rmi_store_error),
};

/* Utility routine to set the value of a bit field in a register. */
int rmi_set_bit_field(struct rmi_device *rmi_dev,
		      unsigned short address,
		      unsigned char field_mask,
		      unsigned char bits)
{
	unsigned char reg_contents;
	int retval;

	retval = rmi_read(rmi_dev, address, &reg_contents);
	if (retval)
		return retval;
	reg_contents = (reg_contents & ~field_mask) | bits;
	retval = rmi_write(rmi_dev, address, reg_contents);
	if (retval == 1)
		return 0;
	else if (retval == 0)
		return -EIO;
	return retval;
}

static ssize_t rmi_fn_01_productinfo_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct f01_data *data = NULL;
	struct rmi_function_container *fc = to_rmi_function_container(dev);

	data = fc->data;

	return snprintf(buf, PAGE_SIZE, "0x%02x 0x%02x\n",
			data->basic_queries.productinfo_1,
			data->basic_queries.productinfo_2);
}

static ssize_t rmi_fn_01_productid_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct f01_data *data = NULL;
	struct rmi_function_container *fc = to_rmi_function_container(dev);

	data = fc->data;

	return snprintf(buf, PAGE_SIZE, "%s\n", data->product_id);
}

static ssize_t rmi_fn_01_manufacturer_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct f01_data *data = NULL;
	struct rmi_function_container *fc = to_rmi_function_container(dev);

	data = fc->data;

	return snprintf(buf, PAGE_SIZE, "0x%02x\n",
			data->basic_queries.manufacturer_id);
}

static ssize_t rmi_fn_01_datecode_show(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	struct f01_data *data = NULL;
	struct rmi_function_container *fc = to_rmi_function_container(dev);

	data = fc->data;

	return snprintf(buf, PAGE_SIZE, "20%02u-%02u-%02u\n",
			data->basic_queries.year,
			data->basic_queries.month,
			data->basic_queries.day);
}

static ssize_t rmi_fn_01_reset_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct rmi_function_container *fc = NULL;
	unsigned int reset;
	int retval = 0;
	/* Command register always reads as 0, so we can just use a local. */
	union f01_device_commands commands = {};

	fc = to_rmi_function_container(dev);

	if (sscanf(buf, "%u", &reset) != 1)
		return -EINVAL;
	if (reset < 0 || reset > 1)
		return -EINVAL;

	/* Per spec, 0 has no effect, so we skip it entirely. */
	if (reset) {
		commands.reset = 1;
		retval = rmi_write_block(fc->rmi_dev, fc->fd.command_base_addr,
				&commands.reg, sizeof(commands.reg));
		if (retval < 0) {
			dev_err(dev, "%s: failed to issue reset command, "
				"error = %d.", __func__, retval);
			return retval;
		}
	}

	return count;
}

static ssize_t rmi_fn_01_sleepmode_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct f01_data *data = NULL;
	struct rmi_function_container *fc = to_rmi_function_container(dev);

	data = fc->data;

	return snprintf(buf, PAGE_SIZE,
			"%d\n", data->device_control.sleep_mode);
}

static ssize_t rmi_fn_01_sleepmode_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf,
					 size_t count)
{
	struct f01_data *data = NULL;
	unsigned long new_value;
	int retval;
	struct rmi_function_container *fc = to_rmi_function_container(dev);

	data = fc->data;

	retval = strict_strtoul(buf, 10, &new_value);
	if (retval < 0 || !RMI_IS_VALID_SLEEPMODE(new_value)) {
		dev_err(dev, "%s: Invalid sleep mode %s.", __func__, buf);
		return -EINVAL;
	}

	dev_dbg(dev, "Setting sleep mode to %ld.", new_value);
	data->device_control.sleep_mode = new_value;
	retval = rmi_write_block(fc->rmi_dev, fc->fd.control_base_addr,
			&data->device_control.reg,
			sizeof(data->device_control.reg));
	if (retval >= 0)
		retval = count;
	else
		dev_err(dev, "Failed to write sleep mode, code %d.\n", retval);
	return retval;
}

static ssize_t rmi_fn_01_nosleep_show(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	struct f01_data *data = NULL;
	struct rmi_function_container *fc = to_rmi_function_container(dev);

	data = fc->data;

	return snprintf(buf, PAGE_SIZE, "%d\n", data->device_control.nosleep);
}

static ssize_t rmi_fn_01_nosleep_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf,
				       size_t count)
{
	struct f01_data *data = NULL;
	unsigned long new_value;
	int retval;
	struct rmi_function_container *fc = to_rmi_function_container(dev);

	data = fc->data;

	retval = strict_strtoul(buf, 10, &new_value);
	if (retval < 0 || new_value < 0 || new_value > 1) {
		dev_err(dev, "%s: Invalid nosleep bit %s.", __func__, buf);
		return -EINVAL;
	}

	data->device_control.nosleep = new_value;
	retval = rmi_write_block(fc->rmi_dev, fc->fd.control_base_addr,
			&data->device_control.reg,
			sizeof(data->device_control.reg));
	if (retval >= 0)
		retval = count;
	else
		dev_err(dev, "Failed to write nosleep bit.\n");
	return retval;
}

static ssize_t rmi_fn_01_chargerinput_show(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	struct f01_data *data = NULL;
	struct rmi_function_container *fc = to_rmi_function_container(dev);

	data = fc->data;

	return snprintf(buf, PAGE_SIZE, "%d\n",
			data->device_control.charger_input);
}

static ssize_t rmi_fn_01_chargerinput_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf,
				       size_t count)
{
	struct f01_data *data = NULL;
	unsigned long new_value;
	int retval;
	struct rmi_function_container *fc = to_rmi_function_container(dev);

	data = fc->data;

	retval = strict_strtoul(buf, 10, &new_value);
	if (retval < 0 || new_value < 0 || new_value > 1) {
		dev_err(dev, "%s: Invalid chargerinput bit %s.", __func__, buf);
		return -EINVAL;
	}

	data->device_control.charger_input = new_value;
	retval = rmi_write_block(fc->rmi_dev, fc->fd.control_base_addr,
			&data->device_control.reg,
			sizeof(data->device_control.reg));
	if (retval >= 0)
		retval = count;
	else
		dev_err(dev, "Failed to write chargerinput bit.\n");
	return retval;
}

static ssize_t rmi_fn_01_reportrate_show(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	struct f01_data *data = NULL;
	struct rmi_function_container *fc = to_rmi_function_container(dev);

	data = fc->data;

	return snprintf(buf, PAGE_SIZE, "%d\n",
			data->device_control.report_rate);
}

static ssize_t rmi_fn_01_reportrate_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf,
				       size_t count)
{
	struct f01_data *data = NULL;
	unsigned long new_value;
	int retval;
	struct rmi_function_container *fc = to_rmi_function_container(dev);

	data = fc->data;

	retval = strict_strtoul(buf, 10, &new_value);
	if (retval < 0 || new_value < 0 || new_value > 1) {
		dev_err(dev, "%s: Invalid reportrate bit %s.", __func__, buf);
		return -EINVAL;
	}

	data->device_control.report_rate = new_value;
	retval = rmi_write_block(fc->rmi_dev, fc->fd.control_base_addr,
			&data->device_control.reg,
			sizeof(data->device_control.reg));
	if (retval >= 0)
		retval = count;
	else
		dev_err(dev, "Failed to write reportrate bit.\n");
	return retval;
}

static ssize_t rmi_fn_01_configured_show(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	struct f01_data *data = NULL;
	struct rmi_function_container *fc = to_rmi_function_container(dev);

	data = fc->data;

	return snprintf(buf, PAGE_SIZE, "%d\n",
			data->device_control.configured);
}

static ssize_t rmi_fn_01_unconfigured_show(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	struct f01_data *data = NULL;
	struct rmi_function_container *fc = to_rmi_function_container(dev);

	data = fc->data;

	return snprintf(buf, PAGE_SIZE, "%d\n",
			data->device_status.unconfigured);
}

static ssize_t rmi_fn_01_flashprog_show(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	struct f01_data *data = NULL;
	struct rmi_function_container *fc = to_rmi_function_container(dev);

	data = fc->data;

	return snprintf(buf, PAGE_SIZE, "%d\n",
			data->device_status.flash_prog);
}

static ssize_t rmi_fn_01_statuscode_show(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	struct f01_data *data = NULL;
	struct rmi_function_container *fc = to_rmi_function_container(dev);

	data = fc->data;

	return snprintf(buf, PAGE_SIZE, "0x%02x\n",
			data->device_status.status_code);
}

int rmi_driver_f01_init(struct rmi_device *rmi_dev)
{
	struct rmi_driver_data *driver_data = rmi_get_driverdata(rmi_dev);
	struct rmi_function_container *fc = driver_data->f01_container;
	struct f01_data *data;
	int error;
	u8 temp;
	int attr_count;

	data = kzalloc(sizeof(struct f01_data), GFP_KERNEL);
	if (!data) {
		dev_err(&rmi_dev->dev, "Failed to allocate F01 data.\n");
		return -ENOMEM;
	}
	fc->data = data;

	/* Set the configured bit. */
	error = rmi_read_block(rmi_dev, fc->fd.control_base_addr,
			&data->device_control.reg,
			sizeof(data->device_control.reg));
	if (error < 0) {
		dev_err(&fc->dev, "Failed to read F01 control.\n");
		goto error_exit;
	}

	/* Sleep mode might be set as a hangover from a system crash or
	 * reboot without power cycle.  If so, clear it so the sensor
	 * is certain to function.
	 */
	if (data->device_control.sleep_mode != RMI_SLEEP_MODE_NORMAL) {
		dev_warn(&fc->dev,
			 "WARNING: Non-zero sleep mode found. Clearing...\n");
		data->device_control.sleep_mode = RMI_SLEEP_MODE_NORMAL;
	}

	data->device_control.configured = 1;
	error = rmi_write_block(rmi_dev, fc->fd.control_base_addr,
			&data->device_control.reg,
			sizeof(data->device_control.reg));
	if (error < 0) {
		dev_err(&fc->dev, "Failed to write F01 control.\n");
		goto error_exit;
	}

	/* dummy read in order to clear irqs */
	error = rmi_read(rmi_dev, fc->fd.data_base_addr + 1, &temp);
	if (error < 0) {
		dev_err(&fc->dev, "Failed to read Interrupt Status.\n");
		goto error_exit;
	}

	error = rmi_read_block(rmi_dev, fc->fd.query_base_addr,
				data->basic_queries.regs,
				sizeof(data->basic_queries.regs));
	if (error < 0) {
		dev_err(&fc->dev, "Failed to read device query registers.\n");
		goto error_exit;
	}
	driver_data->manufacturer_id = data->basic_queries.manufacturer_id;

	error = rmi_read_block(rmi_dev,
		fc->fd.query_base_addr + sizeof(data->basic_queries.regs),
		data->product_id, RMI_PRODUCT_ID_LENGTH);
	if (error < 0) {
		dev_err(&fc->dev, "Failed to read product ID.\n");
		goto error_exit;
	}
	data->product_id[RMI_PRODUCT_ID_LENGTH] = '\0';
	memcpy(driver_data->product_id, data->product_id,
	       sizeof(data->product_id));

	error = rmi_read_block(rmi_dev, fc->fd.data_base_addr,
			&data->device_status.reg,
			sizeof(data->device_status.reg));
	if (error < 0) {
		dev_err(&fc->dev, "Failed to read device status.\n");
		goto error_exit;
	}
	if (data->device_status.unconfigured) {
		dev_err(&fc->dev,
			"Device reset during configuration process, status: "
			"%#02x!\n", data->device_status.status_code);
		error = -EINVAL;
		goto error_exit;
	}
	/*
	**  attach the routines that handle sysfs interaction
	** Meaning:  Set up sysfs device attributes.
	*/
	for (attr_count = 0; attr_count < ARRAY_SIZE(fn_01_attrs);
			attr_count++) {
		if (sysfs_create_file(&fc->dev.kobj,
				      &fn_01_attrs[attr_count].attr) < 0) {
			dev_err(&fc->dev, "Failed to create sysfs file for %s.",
			       fn_01_attrs[attr_count].attr.name);
			error = -ENODEV;
			goto error_exit;
		}
	}

	return error;

 error_exit:
	kfree(data);
	return error;
}

#ifdef CONFIG_PM

static int rmi_f01_suspend(struct rmi_function_container *fc)
{
	struct rmi_device *rmi_dev = fc->rmi_dev;
	struct rmi_driver_data *driver_data = rmi_get_driverdata(rmi_dev);
	struct f01_data *data = driver_data->f01_container->data;
	int retval = 0;

	dev_dbg(&fc->dev, "Suspending...\n");
	if (data->suspended)
		return 0;

	data->old_nosleep = data->device_control.nosleep;
	data->device_control.nosleep = 0;
	data->device_control.sleep_mode = RMI_SLEEP_MODE_SENSOR_SLEEP;

	retval = rmi_write_block(rmi_dev,
			driver_data->f01_container->fd.control_base_addr,
			&data->device_control.reg,
			sizeof(data->device_control.reg));
	if (retval < 0) {
		dev_err(&fc->dev, "Failed to write sleep mode. "
			"Code: %d.\n", retval);
		data->device_control.nosleep = data->old_nosleep;
		data->device_control.sleep_mode = RMI_SLEEP_MODE_NORMAL;
	} else {
		data->suspended = true;
		retval = 0;
	}

	return retval;
}

static int rmi_f01_resume(struct rmi_function_container *fc)
{
	struct rmi_device *rmi_dev = fc->rmi_dev;
	struct rmi_driver_data *driver_data = rmi_get_driverdata(rmi_dev);
	struct f01_data *data = driver_data->f01_container->data;
	int retval = 0;

	dev_dbg(&fc->dev, "Resuming...\n");
	if (!data->suspended)
		return 0;

	data->device_control.nosleep = data->old_nosleep;
	data->device_control.sleep_mode = RMI_SLEEP_MODE_NORMAL;

	retval = rmi_write_block(rmi_dev,
			driver_data->f01_container->fd.control_base_addr,
			&data->device_control.reg,
			sizeof(data->device_control.reg));
	if (retval < 0)
		dev_err(&fc->dev, "Failed to restore normal operation. "
			"Code: %d.\n", retval);
	else {
		data->suspended = false;
		retval = 0;
	}

	return retval;
}
#endif /* CONFIG_PM */

static int rmi_f01_init(struct rmi_function_container *fc)
{
	return 0;
}

static int rmi_f01_attention(struct rmi_function_container *fc, u8 *irq_bits)
{
	struct rmi_device *rmi_dev = fc->rmi_dev;
	struct f01_data *data = fc->data;
	int error;

	error = rmi_read_block(rmi_dev, fc->fd.data_base_addr,
			&data->device_status.reg,
			sizeof(data->device_status.reg));
	if (error < 0) {
		dev_err(&fc->dev, "Failed to read device status.\n");
		return error;
	}

	/* TODO: Do we handle reset here or elsewhere? */
	if (data->device_status.unconfigured)
		dev_warn(&rmi_dev->dev, "Reset detected! Status code: %#04x.\n",
			data->device_status.status_code);
	return 0;
}

static struct rmi_function_handler function_handler = {
	.func = 0x01,
	.init = rmi_f01_init,
	.attention = rmi_f01_attention,
#ifdef	CONFIG_PM
	.suspend = rmi_f01_suspend,
	.resume = rmi_f01_resume,
#endif
};

static int __init rmi_f01_module_init(void)
{
	int error;

	error = rmi_register_function_driver(&function_handler);
	if (error < 0) {
		pr_err("%s: register failed!\n", __func__);
		return error;
	}

	return 0;
}

static void __exit rmi_f01_module_exit(void)
{
	rmi_unregister_function_driver(&function_handler);
}

module_init(rmi_f01_module_init);
module_exit(rmi_f01_module_exit);

MODULE_AUTHOR("Christopher Heiny <cheiny@synaptics.com>");
MODULE_DESCRIPTION("RMI F01 module");
MODULE_LICENSE("GPL");
