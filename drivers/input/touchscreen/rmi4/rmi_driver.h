/*
 * Copyright (c) 2011, 2012 Synaptics Incorporated
 * Copyright (c) 2011 Unixphere
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

#ifndef _RMI_DRIVER_H
#define _RMI_DRIVER_H

#include <linux/ctype.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>

#define RMI_DRIVER_VERSION "1.7"

#define SYNAPTICS_INPUT_DEVICE_NAME "Synaptics RMI4 Touch Sensor"
#define SYNAPTICS_VENDOR_ID 0x06cb

#define CONFIG_RMI4_DEBUG 1

/* Sysfs related macros */

/* You must define FUNCTION_DATA and FNUM to use these functions. */
#if defined(FNUM) && defined(FUNCTION_DATA)

#define tricat(x, y, z) tricat_(x, y, z)

#define tricat_(x, y, z) x##y##z

#define show_union_struct_prototype(propname)\
static ssize_t tricat(rmi_fn_, FNUM, _##propname##_show)(\
					struct device *dev, \
					struct device_attribute *attr, \
					char *buf);\
\
static DEVICE_ATTR(propname, RMI_RO_ATTR,\
		tricat(rmi_fn_, FNUM, _##propname##_show), \
		NULL);

#define store_union_struct_prototype(propname)\
static ssize_t tricat(rmi_fn_, FNUM, _##propname##_store)(\
					struct device *dev,\
					struct device_attribute *attr,\
					const char *buf, size_t count);\
\
static DEVICE_ATTR(propname, RMI_WO_ATTR,\
		NULL,\
		tricat(rmi_fn_, FNUM, _##propname##_store));

#define show_store_union_struct_prototype(propname)\
static ssize_t tricat(rmi_fn_, FNUM, _##propname##_show)(\
					struct device *dev,\
					struct device_attribute *attr,\
					char *buf);\
\
static ssize_t tricat(rmi_fn_, FNUM, _##propname##_store)(\
					struct device *dev,\
					struct device_attribute *attr,\
					const char *buf, size_t count);\
\
static DEVICE_ATTR(propname, RMI_RW_ATTR,\
		tricat(rmi_fn_, FNUM, _##propname##_show),\
		tricat(rmi_fn_, FNUM, _##propname##_store));

#define simple_show_union_struct(regtype, propname, fmt)\
static ssize_t tricat(rmi_fn_, FNUM, _##propname##_show)(struct device *dev,\
				struct device_attribute *attr, char *buf) {\
	struct rmi_function_dev *fc;\
	struct FUNCTION_DATA *data;\
\
	fc = to_rmi_function_dev(dev);\
	data = fc->data;\
\
	return snprintf(buf, PAGE_SIZE, fmt,\
			data->regtype.propname);\
}

#define simple_show_union_struct2(regtype, reg_group, propname, fmt)\
static ssize_t tricat(rmi_fn_, FNUM, _##propname##_show)(struct device *dev,\
				struct device_attribute *attr, char *buf) {\
	struct rmi_function_dev *fc;\
	struct FUNCTION_DATA *data;\
\
	fc = to_rmi_function_dev(dev);\
	data = fc->data;\
\
	return snprintf(buf, PAGE_SIZE, fmt,\
			data->regtype.reg_group->propname);\
}

#define show_union_struct(regtype, reg_group, propname, fmt)\
static ssize_t tricat(rmi_fn_, FNUM, _##propname##_show)(\
					struct device *dev,\
					struct device_attribute *attr,\
					char *buf) {\
	struct rmi_function_dev *fc;\
	struct FUNCTION_DATA *data;\
	int result;\
\
	fc = to_rmi_function_dev(dev);\
	data = fc->data;\
\
	mutex_lock(&data->regtype##_mutex);\
	/* Read current regtype values */\
	result = rmi_read_block(fc->rmi_dev, data->regtype.reg_group->address,\
				(u8 *)data->regtype.reg_group,\
				sizeof(data->regtype.reg_group->regs));\
	mutex_unlock(&data->regtype##_mutex);\
	if (result < 0) {\
		dev_dbg(dev, "%s : Could not read regtype at 0x%x\\n",\
					__func__,\
					data->regtype.reg_group->address);\
		return result;\
	} \
	return snprintf(buf, PAGE_SIZE, fmt,\
			data->regtype.reg_group->propname);\
}

#define show_store_union_struct(regtype, reg_group, propname, fmt)\
show_union_struct(regtype, reg_group, propname, fmt)\
\
static ssize_t tricat(rmi_fn_, FNUM, _##propname##_store)(\
					struct device *dev,\
					struct device_attribute *attr,\
					const char *buf, size_t count) {\
	int result;\
	unsigned long val;\
	unsigned long old_val;\
	struct rmi_function_dev *fc;\
	struct FUNCTION_DATA *data;\
\
	fc = to_rmi_function_dev(dev);\
	data = fc->data;\
\
	/* need to convert the string data to an actual value */\
	result = strict_strtoul(buf, 10, &val);\
\
	/* if an error occured, return it */\
	if (result)\
		return result;\
	/* Check value maybe */\
\
	/* Read current regtype values */\
	mutex_lock(&data->regtype##_mutex);\
	result =\
	    rmi_read_block(fc->rmi_dev, data->regtype.reg_group->address,\
				(u8 *)data->regtype.reg_group,\
				sizeof(data->regtype.reg_group->regs));\
\
	if (result < 0) {\
		mutex_unlock(&data->regtype##_mutex);\
		dev_dbg(dev, "%s : Could not read regtype at 0x%x\\n",\
					 __func__,\
					data->regtype.reg_group->address);\
		return result;\
	} \
	/* if the current regtype registers are already set as we want them,\
	 * do nothing to them */\
	if (data->regtype.reg_group->propname == val) {\
		mutex_unlock(&data->regtype##_mutex);\
		return count;\
	} \
	/* Write the regtype back to the regtype register */\
	old_val = data->regtype.reg_group->propname;\
	data->regtype.reg_group->propname = val;\
	result =\
	    rmi_write_block(fc->rmi_dev, data->regtype.reg_group->address,\
				(u8 *)data->regtype.reg_group,\
				sizeof(data->regtype.reg_group->regs));\
	if (result < 0) {\
		dev_dbg(dev, "%s : Could not write regtype to 0x%x\\n",\
					__func__,\
					data->regtype.reg_group->address);\
		/* revert change to local value if value not written */\
		data->regtype.reg_group->propname = old_val;\
		mutex_unlock(&data->regtype##_mutex);\
		return result;\
	} \
	mutex_unlock(&data->regtype##_mutex);\
	return count;\
}


#define show_repeated_union_struct(regtype, reg_group, propname, fmt)\
static ssize_t tricat(rmi_fn_, FNUM, _##propname##_show)(struct device *dev,\
					struct device_attribute *attr,\
					char *buf) {\
	struct rmi_function_dev *fc;\
	struct FUNCTION_DATA *data;\
	int reg_length;\
	int result, size = 0;\
	char *temp;\
	int i;\
\
	fc = to_rmi_function_dev(dev);\
	data = fc->data;\
	mutex_lock(&data->regtype##_mutex);\
\
	/* Read current regtype values */\
	reg_length = data->regtype.reg_group->length;\
	result = rmi_read_block(fc->rmi_dev, data->regtype.reg_group->address,\
			(u8 *) data->regtype.reg_group->regs,\
			reg_length * sizeof(u8));\
	mutex_unlock(&data->regtype##_mutex);\
	if (result < 0) {\
		dev_dbg(dev, "%s : Could not read regtype at 0x%x\n"\
					"Data may be outdated.", __func__,\
					data->regtype.reg_group->address);\
	} \
	temp = buf;\
	for (i = 0; i < reg_length; i++) {\
		result = snprintf(temp, PAGE_SIZE - size, fmt " ",\
				data->regtype.reg_group->regs[i].propname);\
		if (result < 0) {\
			dev_err(dev, "%s : Could not write output.", __func__);\
			return result;\
		} \
		size += result;\
		temp += result;\
	} \
	result = snprintf(temp, PAGE_SIZE - size, "\n");\
	if (result < 0) {\
			dev_err(dev, "%s : Could not write output.", __func__);\
			return result;\
	} \
	return size + result;\
}

#define show_store_repeated_union_struct(regtype, reg_group, propname, fmt)\
show_repeated_union_struct(regtype, reg_group, propname, fmt)\
\
static ssize_t tricat(rmi_fn_, FNUM, _##propname##_store)(struct device *dev,\
				   struct device_attribute *attr,\
				   const char *buf, size_t count) {\
	struct rmi_function_dev *fc;\
	struct FUNCTION_DATA *data;\
	int reg_length;\
	int result;\
	const char *temp;\
	int i;\
	unsigned int newval;\
\
	fc = to_rmi_function_dev(dev);\
	data = fc->data;\
	mutex_lock(&data->regtype##_mutex);\
\
	/* Read current regtype values */\
\
	reg_length = data->regtype.reg_group->length;\
	result = rmi_read_block(fc->rmi_dev, data->regtype.reg_group->address,\
			(u8 *) data->regtype.reg_group->regs,\
			reg_length * sizeof(u8));\
\
	if (result < 0) {\
		dev_dbg(dev, "%s: Could not read regtype at %#06x. "\
					"Data may be outdated.", __func__,\
					data->regtype.reg_group->address);\
	} \
	\
	/* parse input */\
	temp = buf;\
	for (i = 0; i < reg_length; i++) {\
		if (sscanf(temp, fmt, &newval) == 1) {\
			data->regtype.reg_group->regs[i].propname = newval;\
		} else {\
			/* If we don't read a value for each position, abort, \
			 * restore previous values locally by rereading */\
			result = rmi_read_block(fc->rmi_dev, \
					data->regtype.reg_group->address,\
					(u8 *) data->regtype.reg_group->regs,\
					reg_length * sizeof(u8));\
\
			if (result < 0) {\
				dev_dbg(dev, "%s: Couldn't read regtype at "\
					"%#06x. Local data may be inaccurate", \
					__func__,\
					data->regtype.reg_group->address);\
			} \
			return -EINVAL;\
		} \
		/* move to next number */\
		while (*temp != 0) {\
			temp++;\
			if (isspace(*(temp - 1)) && !isspace(*temp))\
				break;\
		} \
	} \
	result = rmi_write_block(fc->rmi_dev, data->regtype.reg_group->address,\
			(u8 *) data->regtype.reg_group->regs,\
			reg_length * sizeof(u8));\
	mutex_unlock(&data->regtype##_mutex);\
	if (result < 0) {\
		dev_dbg(dev, "%s: Could not write new values to %#06x\n", \
				__func__, data->regtype.reg_group->address);\
		return result;\
	} \
	return count;\
}

/* Create templates for given types */
#define simple_show_union_struct_unsigned(regtype, propname)\
simple_show_union_struct(regtype, propname, "%u\n")

#define simple_show_union_struct_unsigned2(regtype, reg_group, propname)\
simple_show_union_struct2(regtype, reg_group, propname, "%u\n")

#define show_union_struct_unsigned(regtype, reg_group, propname)\
show_union_struct(regtype, reg_group, propname, "%u\n")

#define show_store_union_struct_unsigned(regtype, reg_group, propname)\
show_store_union_struct(regtype, reg_group, propname, "%u\n")

#define show_repeated_union_struct_unsigned(regtype, reg_group, propname)\
show_repeated_union_struct(regtype, reg_group, propname, "%u")

#define show_store_repeated_union_struct_unsigned(regtype, reg_group, propname)\
show_store_repeated_union_struct(regtype, reg_group, propname, "%u")

/* Remove access to raw format string versions */
/*#undef simple_show_union_struct
#undef show_union_struct_unsigned
#undef show_store_union_struct
#undef show_repeated_union_struct
#undef show_store_repeated_union_struct*/

#endif

#define GROUP(_attrs) { \
	.attrs = _attrs,  \
}

#define attrify(nm) (&dev_attr_##nm.attr)

#define PDT_PROPERTIES_LOCATION 0x00EF
#define BSR_LOCATION 0x00FE

struct pdt_properties {
	u8 reserved_1:6;
	u8 has_bsr:1;
	u8 reserved_2:1;
} __attribute__((__packed__));

struct rmi_driver_data {
	struct rmi_function_dev rmi_functions;
	struct rmi_device *rmi_dev;

	struct rmi_function_dev *f01_dev;
	bool f01_bootloader_mode;

	atomic_t attn_count;
	u32 irq_debug;
	int irq;
	int irq_flags;
	int num_of_irq_regs;
	int irq_count;
	unsigned long *irq_status;
	unsigned long *current_irq_mask;
	unsigned long *irq_mask_store;
	bool irq_stored;
	struct mutex irq_mutex;

	/* Following are used when polling. */
	struct hrtimer poll_timer;
	struct work_struct poll_work;
	ktime_t poll_interval;
	struct mutex pdt_mutex;
	struct pdt_properties pdt_props;
	u8 bsr;

	int board;
	int rev;

	bool enabled;
#ifdef CONFIG_PM
	bool suspended;
#if defined(CONFIG_HAS_EARLYSUSPEND)
	bool early_suspended;
#endif
	struct mutex suspend_mutex;

	void *pm_data;
	int (*pre_suspend) (const void *pm_data);
	int (*post_suspend) (const void *pm_data);
	int (*pre_resume) (const void *pm_data);
	int (*post_resume) (const void *pm_data);
#endif

#ifdef CONFIG_RMI4_DEBUG
	struct dentry *debugfs_delay;
	struct dentry *debugfs_phys;
	struct dentry *debugfs_reg_ctl;
	struct dentry *debugfs_reg;
	struct dentry *debugfs_irq;
	struct dentry *debugfs_attn_count;
	u16 reg_debug_addr;
	u8 reg_debug_size;
#endif

	void *data;
};

#define PDT_START_SCAN_LOCATION 0x00e9
#define PDT_END_SCAN_LOCATION	0x0005
#define RMI4_END_OF_PDT(id) ((id) == 0x00 || (id) == 0xff)

struct pdt_entry {
	u8 query_base_addr:8;
	u8 command_base_addr:8;
	u8 control_base_addr:8;
	u8 data_base_addr:8;
	u8 interrupt_source_count:3;
	u8 bits3and4:2;
	u8 function_version:2;
	u8 bit7:1;
	u8 function_number:8;
} __attribute__((__packed__));

static inline void copy_pdt_entry_to_fd(struct pdt_entry *pdt,
				 struct rmi_function_descriptor *fd,
				 u16 page_start)
{
	fd->query_base_addr = pdt->query_base_addr + page_start;
	fd->command_base_addr = pdt->command_base_addr + page_start;
	fd->control_base_addr = pdt->control_base_addr + page_start;
	fd->data_base_addr = pdt->data_base_addr + page_start;
	fd->function_number = pdt->function_number;
	fd->interrupt_source_count = pdt->interrupt_source_count;
	fd->function_version = pdt->function_version;
}

#ifdef	CONFIG_RMI4_FWLIB
extern void rmi4_fw_update(struct rmi_device *rmi_dev,
		struct pdt_entry *f01_pdt, struct pdt_entry *f34_pdt);
#else
#define rmi4_fw_update(rmi_dev, f01_pdt, f34_pdt) 0
#endif

extern struct rmi_driver rmi_sensor_driver;
extern struct rmi_function_driver rmi_f01_driver;

int rmi_register_sensor_driver(void);
void rmi_unregister_sensor_driver(void);

#endif
