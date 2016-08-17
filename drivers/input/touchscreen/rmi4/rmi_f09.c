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
#include <linux/kernel.h>
#include <linux/rmi.h>
#include <linux/input.h>
#include <linux/slab.h>

#define QUERY_BASE_INDEX 1
#define MAX_LEN 256

/* data specific to fn $09 that needs to be kept around */
struct f09_query {
	u8 Limit_Register_Count;
	union {
		struct {
			u8 Result_Register_Count:3;
			u8 Reserved:3;
			u8 InternalLimits:1;
			u8 HostTestEn:1;
		};
		u8 f09_bist_query1;
	};
};

struct f09_control {
	/* test1 */
	u8 Test1LimitLo;
	u8 Test1LimitHi;
	u8 Test1LimitDiff;
	/* test2 */
	u8 Test2LimitLo;
	u8 Test2LimitHi;
	u8 Test2LimitDiff;
};

struct f09_data {
	u8 TestNumberControl;
	u8 Overall_BIST_Result;
	u8 TestResult1;
	u8 TestResult2;
	u8 Transmitter_Number;

	union {
		struct {
			u8 Receiver_Number:6;
			u8 Limit_Failure_Code:2;
		};
		u8 f09_bist_data2;
	};
};

struct f09_cmd {
	union {
		struct {
			u8 RunBIST:1;
		};
		u8 f09_bist_cmd0;
	};
};

struct rmi_fn_09_data {
	struct f09_query query;
};

static ssize_t rmi_f09_Limit_Register_Count_show(struct device *dev,
				      struct device_attribute *attr, char *buf);

static ssize_t rmi_f09_HostTestEn_show(struct device *dev,
				      struct device_attribute *attr, char *buf);

static ssize_t rmi_f09_HostTestEn_store(struct device *dev,
				      struct device_attribute *attr,
					const char *buf, size_t count);

static ssize_t rmi_f09_Result_Register_Count_show(struct device *dev,
				      struct device_attribute *attr, char *buf);
#if defined(RMI_SYS_ATTR)
static ssize_t rmi_f09_InternalLimits_show(struct device *dev,
				      struct device_attribute *attr, char *buf);

static ssize_t rmi_f09_Overall_BIST_Result_show(struct device *dev,
				      struct device_attribute *attr, char *buf);

static ssize_t rmi_f09_Overall_BIST_Result_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count);
#endif

static struct device_attribute attrs[] = {
	__ATTR(Limit_Register_Count, RMI_RO_ATTR,
	       rmi_f09_Limit_Register_Count_show, rmi_store_error),
	__ATTR(HostTestEn, RMI_RW_ATTR,
	       rmi_f09_HostTestEn_show, rmi_f09_HostTestEn_store),
	__ATTR(InternalLimits, RMI_RO_ATTR,
	       rmi_f09_Limit_Register_Count_show, rmi_store_error),
	__ATTR(Result_Register_Count, RMI_RO_ATTR,
	       rmi_f09_Result_Register_Count_show, rmi_store_error),
};

static int rmi_f09_init(struct rmi_function_container *fc)
{
	struct rmi_device *rmi_dev = fc->rmi_dev;
	struct rmi_device_platform_data *pdata;
	struct rmi_fn_09_data  *f09;
	u8 query_base_addr;
	int rc;
	int attr_count = 0;
	int retval = 0;

	dev_info(&fc->dev, "Intializing F09 values.");

	f09 = kzalloc(sizeof(struct rmi_fn_09_data), GFP_KERNEL);
	if (!f09) {
		dev_err(&fc->dev, "Failed to allocate rmi_fn_09_data.\n");
		retval = -ENOMEM;
		goto error_exit;
	}
	fc->data = f09;

	pdata = to_rmi_platform_data(rmi_dev);
	query_base_addr = fc->fd.query_base_addr;

	/* initial all default values for f09 query here */
	rc = rmi_read_block(rmi_dev, query_base_addr,
		(u8 *)&f09->query, sizeof(f09->query));
	if (rc < 0) {
		dev_err(&fc->dev, "Failed to read query register."
			" from 0x%04x\n", query_base_addr);
		goto error_exit;
	}

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
	return 0;

error_exit:
	dev_err(&fc->dev, "An error occured in F09 init!\n");
	for (attr_count--; attr_count >= 0; attr_count--)
		sysfs_remove_file(&fc->dev.kobj,
				  &attrs[attr_count].attr);
	kfree(f09);
	return retval;
}

static void rmi_f09_remove(struct rmi_function_container *fc)
{
	struct rmi_fn_09_data *data = fc->data;
	if (data) {
		kfree(&data->query.Limit_Register_Count);
		kfree(&data->query.f09_bist_query1);
	}
	kfree(fc->data);
}

static struct rmi_function_handler function_handler = {
	.func = 0x09,
	.init = rmi_f09_init,
	.remove = rmi_f09_remove
};

static int __init rmi_f09_module_init(void)
{
	int error;

	error = rmi_register_function_driver(&function_handler);
	if (error < 0) {
		pr_err("%s: register failed!\n", __func__);
		return error;
	}

	return 0;
}

static void rmi_f09_module_exit(void)
{
	rmi_unregister_function_driver(&function_handler);
}


static ssize_t rmi_f09_Limit_Register_Count_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct rmi_function_container *fc;
	struct rmi_fn_09_data *data;

	fc = to_rmi_function_container(dev);
	data = fc->data;
	return snprintf(buf, PAGE_SIZE, "%u\n",
			data->query.Limit_Register_Count);
}

static ssize_t rmi_f09_HostTestEn_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct rmi_function_container *fc;
	struct rmi_fn_09_data *data;

	fc = to_rmi_function_container(dev);
	data = fc->data;
	return snprintf(buf, PAGE_SIZE, "%u\n",
			data->query.HostTestEn);
}

static ssize_t rmi_f09_HostTestEn_store(struct device *dev,
				      struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct rmi_function_container *fc;
	struct rmi_fn_09_data *data;
	unsigned int new_value;
	int result;

	fc = to_rmi_function_container(dev);
	data = fc->data;
	if (sscanf(buf, "%u", &new_value) != 1) {
		dev_err(dev,
		"%s: Error - HostTestEn_store has an "
		"invalid len.\n",
		__func__);
		return -EINVAL;
	}

	if (new_value < 0 || new_value > 1) {
		dev_err(dev, "%s: Invalid HostTestEn bit %s.", __func__, buf);
		return -EINVAL;
	}
	data->query.HostTestEn = new_value;
	result = rmi_write(fc->rmi_dev, fc->fd.query_base_addr,
		data->query.HostTestEn);
	if (result < 0) {
		dev_err(dev, "%s : Could not write HostTestEn_store to 0x%x\n",
				__func__, fc->fd.query_base_addr);
		return result;
	}

	return count;

}

#if defined(RMI_SYS_ATTR)
 ssize_t rmi_f09_InternalLimits_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct rmi_function_container *fc;
	struct rmi_fn_09_data *data;

	fc = to_rmi_function_container(dev);
	data = fc->data;
	return snprintf(buf, PAGE_SIZE, "%u\n",
			data->query.InternalLimits);
}
#endif

static ssize_t rmi_f09_Result_Register_Count_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct rmi_function_container *fc;
	struct rmi_fn_09_data *data;

	fc = to_rmi_function_container(dev);
	data = fc->data;
	return snprintf(buf, PAGE_SIZE, "%u\n",
			data->query.Result_Register_Count);
}

module_init(rmi_f09_module_init);
module_exit(rmi_f09_module_exit);

MODULE_AUTHOR("Allie Xiong <axiong@Synaptics.com>");
MODULE_DESCRIPTION("RMI F09 module");
MODULE_LICENSE("GPL");
