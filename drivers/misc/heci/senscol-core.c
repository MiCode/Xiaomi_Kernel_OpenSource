/*
 * Sensor collection framework core
 *
 * Copyright (c) 2014-2015, Intel Corporation.
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/module.h>
#include <linux/senscol/senscol-core.h>
#include <linux/platform_device.h>
#include <linux/kobject.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include "hid-strings-def.h"
#include "platform-config.h"
#include <linux/miscdevice.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include "heci-hid.h"

struct list_head	senscol_impl_list;
struct list_head	senscol_sensors_list;
spinlock_t	senscol_lock;
spinlock_t	senscol_data_lock;
uint8_t	*senscol_data_buf;
unsigned	senscol_data_head, senscol_data_tail;
int	flush_asked = 0;
struct task_struct *user_task;

static ssize_t	sc_data_show(struct kobject *kobj, struct attribute *attr,
	char *buf);

static ssize_t	sc_data_store(struct kobject *kobj, struct attribute *attr,
	const char *buf, size_t size);

static ssize_t	sensors_data_read(struct file *f, struct kobject *kobj,
	struct bin_attribute *bin_attr, char *buf, loff_t offs, size_t size);

static ssize_t	sensors_data_write(struct file *f, struct kobject *kobj,
	struct bin_attribute *bin_attr, char *buf, loff_t offs, size_t size);

static ssize_t	sc_sensdef_show(struct kobject *kobj, struct attribute *attr,
	char *buf);

static ssize_t	sc_sensdef_store(struct kobject *kobj, struct attribute *attr,
	const char *buf, size_t size);

static struct platform_device	*sc_pdev;

wait_queue_head_t senscol_read_wait;

void senscol_send_ready_event(void)
{
	if (waitqueue_active(&senscol_read_wait))
		wake_up_interruptible(&senscol_read_wait);
}
EXPORT_SYMBOL(senscol_send_ready_event);

int senscol_reset_notify(void)
{

	struct siginfo si;
	int ret;

	memset(&si, 0, sizeof(struct siginfo));
	si.si_signo = SIGUSR1;
	si.si_code = SI_USER;

	if (user_task == NULL)
		return -EINVAL;

	ret = send_sig_info(SIGUSR1, &si, user_task);
	return ret;
}
EXPORT_SYMBOL(senscol_reset_notify);

const char *senscol_usage_to_name(unsigned usage)
{
	int i;

	for (i = 0; code_msg_arr[i].msg && code_msg_arr[i].code != usage; i++)
		;
	return	code_msg_arr[i].msg;
}
EXPORT_SYMBOL(senscol_usage_to_name);


unsigned senscol_name_to_usage(const char *name)
{
	int i;

	for (i = 0; code_msg_arr[i].msg &&
			strcmp(code_msg_arr[i].msg, name) != 0; ++i)
		;
	return	code_msg_arr[i].code;
}
EXPORT_SYMBOL(senscol_name_to_usage);


const char	*senscol_get_modifier(unsigned modif)
{
	uint32_t to4bits = modif >> 0xC;
	return	modifiers[to4bits];
}
EXPORT_SYMBOL(senscol_get_modifier);



#if 0
/*
 * data kobject attributes and handlers
 */

static struct attribute	sc_data_defattr_event = {
	.name = "event",
	.mode = (S_IRUGO)
};

static struct attribute	*sc_data_defattrs[] = {
	&sc_data_defattr_event,
	NULL
};
#endif

static void	sc_data_release(struct kobject *k)
{
	ISH_DBG_PRINT(KERN_ALERT "[senscol]: %s():+++\n", __func__);
}

static ssize_t	sc_data_show(struct kobject *kobj, struct attribute *attr,
	char *buf)
{
	ISH_DBG_PRINT(KERN_ALERT "[senscol]: %s(): +++ attr='%s'\n",
		__func__, attr->name);
	scnprintf(buf, PAGE_SIZE, "%s\n", attr->name);
	return	strlen(buf);
}

static ssize_t	sc_data_store(struct kobject *kobj, struct attribute *attr,
	const char *buf, size_t size)
{
	ISH_DBG_PRINT(KERN_ALERT
		"[senscol]: %s(): +++ attr='%s' buf='%s' size=%u\n", __func__,
		attr->name, buf, (unsigned)size);
	return	size;
}

const struct sysfs_ops	sc_data_sysfs_fops = {
	.show = sc_data_show,
	.store = sc_data_store
};

struct kobj_type	sc_data_kobj_type = {
	.release = sc_data_release,
	.sysfs_ops = &sc_data_sysfs_fops
	/*.default_attrs = sc_data_defattrs*/
};

struct bin_attribute	sensors_data_binattr = {
	.attr = {
		.name = "sensors_data",
		.mode = S_IRUGO
	},
	.size = 0,
	.read = sensors_data_read,
	.write = sensors_data_write
};

struct kobject	sc_data_kobj;

/*****************************************/


/*
 * sensor_def kobject type and handlers
 */
static struct attribute	sc_sensdef_defattr_name = {
	.name = "name",
	.mode = (S_IRUGO)
};

static struct attribute sc_sensdef_defattr_id = {
	.name = "id",
	.mode = (S_IRUGO)
};

static struct attribute sc_sensdef_defattr_usage_id = {
	.name = "usage_id",
	.mode = (S_IRUGO)
};

static struct attribute sc_sensdef_defattr_sample_size = {
	.name = "sample_size",
	.mode = (S_IRUGO)
};

static struct attribute sc_sensdef_defattr_flush = {
	.name = "flush",
	.mode = (S_IRUGO)
};

static struct attribute sc_sensdef_defattr_get_sample = {
	.name = "get_sample",
	.mode = (S_IRUGO)
};

struct attribute	*sc_sensdef_defattrs[] = {
	&sc_sensdef_defattr_name,
	&sc_sensdef_defattr_id,
	&sc_sensdef_defattr_usage_id,
	&sc_sensdef_defattr_sample_size,
	&sc_sensdef_defattr_flush,
	&sc_sensdef_defattr_get_sample,
	NULL
};

static void	sc_sensdef_release(struct kobject *k)
{
	ISH_DBG_PRINT(KERN_ALERT "[senscol]: %s():+++\n", __func__);
}

static ssize_t	sc_sensdef_show(struct kobject *kobj, struct attribute *attr,
	char *buf)
{
	ssize_t	rv;
	struct sensor_def	*sensdef;
	static char    tmp_buf[0x1000];
	unsigned long flags;

	ISH_DBG_PRINT(KERN_ALERT "[senscol]: %s(): +++ attr='%s'\n",
		__func__, attr->name);
	sensdef = container_of(kobj, struct sensor_def, kobj);
	buf[0] = '\0';
	if (!strcmp(attr->name, "id"))
		scnprintf(buf, PAGE_SIZE, "%08X\n", sensdef->id);
	else if (!strcmp(attr->name, "sample_size"))
		scnprintf(buf, PAGE_SIZE, "%u\n", sensdef->sample_size);
	else if (!strcmp(attr->name, "usage_id"))
		scnprintf(buf, PAGE_SIZE, "%08X\n", sensdef->usage_id);
	else if (!strcmp(attr->name, "name"))
		scnprintf(buf, PAGE_SIZE, "%s\n", sensdef->name);
	else if (!strcmp(attr->name, "flush"))
		/*if "sensdef" is activated in batch mode,
		mark it as asking flush*/
		if (sensdef->impl->batch_check(sensdef)) {
			spin_lock_irqsave(&senscol_lock, flags);
			flush_asked = 1;
			sensdef->flush_req = 1;
			spin_unlock_irqrestore(&senscol_lock, flags);
			sensdef->impl->get_sens_property(sensdef,
				sensdef->properties, tmp_buf, 0x1000);
			scnprintf(buf, PAGE_SIZE, "1\n");
		} else {
			uint32_t pseudo_event_id =
				sensdef->id | PSEUSO_EVENT_BIT;
			uint32_t pseudo_event_content = 0;
			pseudo_event_content |= FLUSH_CMPL_BIT;
			push_sample(pseudo_event_id, &pseudo_event_content);
			scnprintf(buf, PAGE_SIZE, "0\n");
		}
	else if (!strcmp(attr->name, "get_sample")) {
		rv = sensdef->impl->get_sample(sensdef);
		/* The sample will arrive to hid "raw event" func,
		and will be pushed to user via "push_sample" method */

		scnprintf(buf, PAGE_SIZE, "%d\n", !rv);
	}
	rv = strlen(buf) + 1;
	return	rv;
}

static ssize_t	sc_sensdef_store(struct kobject *kobj, struct attribute *attr,
	const char *buf, size_t size)
{
	ISH_DBG_PRINT(KERN_ALERT "[senscol]: %s():+++\n", __func__);
	return	-EINVAL;
}
const struct sysfs_ops	sc_sensdef_sysfs_fops = {
	.show = sc_sensdef_show,
	.store = sc_sensdef_store
};

struct kobj_type	sc_sensdef_kobj_type = {
	.release = sc_sensdef_release,
	.sysfs_ops = &sc_sensdef_sysfs_fops,
	.default_attrs = sc_sensdef_defattrs
};
/*****************************************/

/*
 * kobject type for empty sub-directories
 */
static struct attribute	*sc_subdir_defattrs[] = {
	NULL
};

static void	sc_subdir_release(struct kobject *k)
{
	ISH_DBG_PRINT(KERN_ALERT "[senscol]: %s():+++\n", __func__);
}

static ssize_t	sc_subdir_show(struct kobject *kobj, struct attribute *attr,
	char *buf)
{
	ISH_DBG_PRINT(KERN_ALERT "[senscol]: %s():+++\n", __func__);
	return	-EINVAL;
}

static ssize_t	sc_subdir_store(struct kobject *kobj, struct attribute *attr,
	const char *buf, size_t size)
{
	ISH_DBG_PRINT(KERN_ALERT "[senscol]: %s():+++\n", __func__);
	return	-EINVAL;
}

const struct sysfs_ops	sc_subdir_sysfs_fops = {
	.show = sc_subdir_show,
	.store = sc_subdir_store
};

struct kobj_type	sc_subdir_kobj_type = {
	.release = sc_subdir_release,
	.sysfs_ops = &sc_subdir_sysfs_fops,
	.default_attrs = sc_subdir_defattrs
};
/*****************************************/

/*
 * sensors 'data_field's kobject type
 */
static struct attribute	sc_datafield_defattr_usage_id = {
	.name = "usage_id",
	.mode = (S_IRUGO)
};

static struct attribute	sc_datafield_defattr_exp = {
	.name = "exponent",
	.mode = (S_IRUGO)
};

static struct attribute sc_datafield_defattr_len = {
	.name = "length",
	.mode = (S_IRUGO)
};

static struct attribute sc_datafield_defattr_unit = {
	.name = "unit",
	.mode = (S_IRUGO)
};

static struct attribute sc_datafield_defattr_index = {
	.name = "index",
	.mode = (S_IRUGO)
};

static struct attribute	sc_datafield_defattr_is_numeric = {
	.name = "is_numeric",
	.mode = (S_IRUGO)
};

struct attribute	*sc_datafield_defattrs[] = {
	&sc_datafield_defattr_usage_id,
	&sc_datafield_defattr_exp,
	&sc_datafield_defattr_len,
	&sc_datafield_defattr_unit,
	&sc_datafield_defattr_index,
	&sc_datafield_defattr_is_numeric,
	NULL
};

static void	sc_datafield_release(struct kobject *k)
{
	ISH_DBG_PRINT(KERN_ALERT "[senscol]: %s():+++\n", __func__);
}

static ssize_t	sc_datafield_show(struct kobject *kobj, struct attribute *attr,
	char *buf)
{
	ssize_t	rv;
	struct data_field	*dfield;

	ISH_DBG_PRINT(KERN_ALERT "[senscol]: %s(): +++ attr='%s'\n",
		__func__, attr->name);
	dfield = container_of(kobj, struct data_field, kobj);
	if (!strcmp(attr->name, "usage_id"))
		scnprintf(buf, PAGE_SIZE, "%08X\n", (unsigned)dfield->usage_id);
	else if (!strcmp(attr->name, "exponent"))
		scnprintf(buf, PAGE_SIZE, "%u\n", (unsigned)dfield->exp);
	else if (!strcmp(attr->name, "length"))
		scnprintf(buf, PAGE_SIZE, "%u\n", (unsigned)dfield->len);
	else if (!strcmp(attr->name, "unit"))
		scnprintf(buf, PAGE_SIZE, "%u\n", (unsigned)dfield->unit);
	else if (!strcmp(attr->name, "index"))
		scnprintf(buf, PAGE_SIZE, "%u\n", (unsigned)dfield->index);
	else if (!strcmp(attr->name, "is_numeric"))
		scnprintf(buf, PAGE_SIZE, "%u\n", (unsigned)dfield->is_numeric);

	rv = strlen(buf) + 1;
	return	rv;
}

static ssize_t	sc_datafield_store(struct kobject *kobj, struct attribute *attr,
	const char *buf, size_t size)
{
	ISH_DBG_PRINT(KERN_ALERT
		"[senscol]: %s(): +++ attr='%s' buf='%s' size=%u\n", __func__,
		attr->name, buf, (unsigned)size);
	return	-EINVAL;
}

const struct sysfs_ops	sc_datafield_sysfs_fops = {
	.show = sc_datafield_show,
	.store = sc_datafield_store
};

struct kobj_type	sc_datafield_kobj_type = {
	.release = sc_datafield_release,
	.sysfs_ops = &sc_datafield_sysfs_fops,
	.default_attrs = sc_datafield_defattrs
};
/*****************************************/


/*
 * sensors 'properties' kobject type
 */
/*
static struct attribute	sc_sensprop_defattr_unit = {
	.name = "unit",
	.mode = (S_IRUGO | S_IWUSR | S_IWGRP)
};
*/
static struct attribute sc_sensprop_defattr_value = {
	.name = "value",
	.mode = (S_IRUGO | S_IWUSR | S_IWGRP)
};

static struct attribute sc_sensprop_defattr_usage_id = {
	.name = "usage_id",
	.mode = (S_IRUGO | S_IWUSR | S_IWGRP)
};

struct attribute	*sc_sensprop_defattrs[] = {
/*	&sc_sensprop_defattr_unit,*/
	&sc_sensprop_defattr_value,
	&sc_sensprop_defattr_usage_id,
	NULL
};

static void	sc_sensprop_release(struct kobject *k)
{
	ISH_DBG_PRINT(KERN_ALERT "[senscol]: %s():+++\n", __func__);
}

static ssize_t	sc_sensprop_show(struct kobject *kobj, struct attribute *attr,
	char *buf)
{
	struct sens_property	*pfield;
	struct sensor_def	*sensor;
	int	rv = -EINVAL;

	/*
	 * We need "property_power_state" (=2), "property_reporting_state" (=2)
	 * and "property_report_interval" (in ms?)
	 */
	ISH_DBG_PRINT(KERN_ALERT "[senscol]: %s(): +++ attr='%s'\n",
		__func__, attr->name);
	pfield = container_of(kobj, struct sens_property, kobj);
	sensor = pfield->sensor;

	if (!strcmp(attr->name, "value"))
		rv = sensor->impl->get_sens_property(sensor, pfield, buf,
			0x1000);
	else if (!strcmp(attr->name, "usage_id"))
		scnprintf(buf, PAGE_SIZE, "%08X\n", pfield->usage_id & 0xFFFF);
	if (rv)
		return	rv;
	return	strlen(buf);
}

static ssize_t	sc_sensprop_store(struct kobject *kobj, struct attribute *attr,
	const char *buf, size_t size)
{
	struct sens_property	*pfield;
	struct sensor_def	*sensor;
	int	rv;

	/*
	 * TODO: stream down set property request and return size
	 * upon successful completion or error code
	 */
	ISH_DBG_PRINT(KERN_ALERT
		"[senscol]: %s(): +++ attr='%s' buf='%s' size=%u\n",
		__func__, attr->name, buf, (unsigned)size);
	if (strcmp(attr->name, "value"))
		return -EINVAL;

	pfield = container_of(kobj, struct sens_property, kobj);
	sensor = pfield->sensor;
	rv = sensor->impl->set_sens_property(sensor, pfield, buf);

	if (rv)
		return	rv;
	return	size;
}

const struct sysfs_ops	sc_sensprop_sysfs_fops = {
	.show = sc_sensprop_show,
	.store = sc_sensprop_store
};

struct kobj_type	sc_sensprop_kobj_type = {
	.release = sc_sensprop_release,
	.sysfs_ops = &sc_sensprop_sysfs_fops,
	.default_attrs = sc_sensprop_defattrs
};
/*****************************************/

static ssize_t	sensors_data_read(struct file *f, struct kobject *kobj,
	struct bin_attribute *bin_attr, char *buf, loff_t offs, size_t size)
{
	size_t	count;
	unsigned	cur;
	struct senscol_sample	*sample;
	unsigned long	flags;

	ISH_DBG_PRINT(KERN_ALERT "[senscol]: %s():+++\n", __func__);

	if (size > PAGE_SIZE)
		size = PAGE_SIZE;

	ISH_DBG_PRINT(KERN_ALERT
		"[senscol]: %s(): >>> offs=%u size=%u senscol_data_head=%u senscol_data_tail=%u\n",
		__func__, (unsigned)offs, (unsigned)size,
		(unsigned)senscol_data_head, (unsigned)senscol_data_tail);

	spin_lock_irqsave(&senscol_data_lock, flags);

	/*
	 * Count how much we may copy, keeping whole samples.
	 * Copy samples along the way
	 */
	count = 0;
	cur = senscol_data_head;
	while (cur != senscol_data_tail) {
		sample = (struct senscol_sample *)(senscol_data_buf + cur);
		if (count + sample->size > size)
			break;
		memcpy(buf + count, sample, sample->size);
		count += sample->size;
		cur += sample->size;
		if (cur > SENSCOL_DATA_BUF_LAST)
			cur = 0;
	}
	senscol_data_head = cur;

	spin_unlock_irqrestore(&senscol_data_lock, flags);

	if (count) {
		ISH_DBG_PRINT(KERN_ALERT
			"[senscol]: <<< %s(): senscol_data_head=%u senscol_data_tail=%u\n",
			__func__, senscol_data_head, senscol_data_tail);
		ISH_DBG_PRINT(KERN_ALERT
			"[senscol]: %s(): returning count=%u\n", __func__,
			(unsigned)count);
	}

	return	count;
}

static ssize_t	sensors_data_write(struct file *f, struct kobject *kobj,
	struct bin_attribute *bin_attr, char *buf, loff_t offs, size_t size)
{
	ISH_DBG_PRINT(KERN_ALERT "[senscol]: %s():+++\n", __func__);
	return	-EINVAL;
}

int	add_senscol_impl(struct senscol_impl *impl)
{
	unsigned long flags;

	spin_lock_irqsave(&senscol_lock, flags);
	list_add_tail(&impl->link, &senscol_impl_list);
	spin_unlock_irqrestore(&senscol_lock, flags);
	return	0;
}
EXPORT_SYMBOL(add_senscol_impl);

int	remove_senscol_impl(struct senscol_impl *impl)
{
	unsigned long flags;

	spin_lock_irqsave(&senscol_lock, flags);
	list_del(&impl->link);
	spin_unlock_irqrestore(&senscol_lock, flags);
	return	0;
}
EXPORT_SYMBOL(remove_senscol_impl);

/* Only allocates new sensor */
struct sensor_def *alloc_senscol_sensor(void)
{
	struct sensor_def *sens;

	sens = kzalloc(sizeof(struct sensor_def), GFP_KERNEL);
	return	sens;
}
EXPORT_SYMBOL(alloc_senscol_sensor);

/* Init sensor (don't call for initialized sensors */
void	init_senscol_sensor(struct sensor_def *sensor)
{
	if (!sensor)
		return;

	memset(sensor, 0, sizeof(*sensor));
	sensor->name = NULL;
	sensor->friendly_name = NULL;
	sensor->impl = NULL;
	sensor->data_fields = NULL;
	sensor->properties = NULL;
}
EXPORT_SYMBOL(init_senscol_sensor);

int remove_senscol_sensor(uint32_t id)
{
	unsigned long	flags;
	struct sensor_def	*sens, *next;
	int i;

	spin_lock_irqsave(&senscol_lock, flags);
	list_for_each_entry_safe(sens, next, &senscol_sensors_list, link) {
		if (sens->id == id) {
			list_del(&sens->link);

			spin_unlock_irqrestore(&senscol_lock, flags);
			for (i = 0; i < sens->num_properties; ++i)
				if (sens->properties[i].name) {
					kobject_put(&sens->properties[i].kobj);
					kobject_del(&sens->properties[i].kobj);
				}
			kfree(sens->properties);
			kobject_put(&sens->props_kobj);
			kobject_del(&sens->props_kobj);

			for (i = 0; i < sens->num_data_fields; ++i)
				if (sens->data_fields[i].name) {
					kobject_put(&sens->data_fields[i].kobj);
					kobject_del(&sens->data_fields[i].kobj);
				}
			kfree(sens->data_fields);
			kobject_put(&sens->data_fields_kobj);
			kobject_del(&sens->data_fields_kobj);
			kobject_put(&sens->kobj);
			kobject_del(&sens->kobj);

			kfree(sens);

			return 0;
		}
	}
	spin_unlock_irqrestore(&senscol_lock, flags);


	return -EINVAL;
}
EXPORT_SYMBOL(remove_senscol_sensor);

/*
 * Exposed sensor via sysfs, structure may be static
 *
 * The caller is responsible for setting all meaningful fields
 * (may call add_data_field() and add_sens_property() as needed)
 * We'll consider hiding senscol framework-specific fields
 * into opaque structures
 */

int	add_senscol_sensor(struct sensor_def *sensor)
{
	unsigned long	flags;
	char	sensor_name[256];	/* Enough for name "sensor_<NN>_def",
					 * if convention changes array size
					 * should be reviewed */
	int	i;
	int	rv;
	int	j;

	if (!sensor->name || !sensor->impl || !sensor->usage_id || !sensor->id)
		return	-EINVAL;

	spin_lock_irqsave(&senscol_lock, flags);
	list_add_tail(&sensor->link, &senscol_sensors_list);
	spin_unlock_irqrestore(&senscol_lock, flags);

	/*
	 * Create sysfs entries for this sensor
	 */

	/* Init and add sensor_def kobject */
	snprintf(sensor_name, sizeof(sensor_name), "sensor_%X_def", sensor->id);
	rv = kobject_init_and_add(&sensor->kobj, &sc_sensdef_kobj_type,
		&sc_pdev->dev.kobj, sensor_name);
	ISH_DBG_PRINT(KERN_ALERT
		"%s(): kobject_init_and_add() for 'data' returned %d\n",
		__func__, rv);
	if (rv) {
		rv = -EFAULT;
err_ret:
		kobject_put(&sensor->kobj);
		kobject_del(&sensor->kobj);
		return	rv;
	}

/*
 * Special attribute "friendly_name" is retired in favor
 * of generic property "property_friendly_name"
 */
#if 0
	/* If freiendly_name is given, add such attribute */
	memset(&attr, 0, sizeof(struct attribute));
	attr.name = "friendly_name";
	attr.mode = S_IRUGO;
	rv = sysfs_create_file(&sensor->kobj, &attr);
	ISH_DBG_PRINT(KERN_ALERT
		"%s(): sysfs_create_file() for 'friendly_name' returned %d\n",
		__func__, rv);
#endif

	/*
	 * Create kobjects without attributes for
	 * sensor_<NN>_def/data_fields and sensor_<NN>/properties
	 */
	rv = kobject_init_and_add(&sensor->data_fields_kobj,
		&sc_subdir_kobj_type, &sensor->kobj, "data_fields");
	if (rv) {
		rv = -EFAULT;
err_ret2:
		kobject_put(&sensor->data_fields_kobj);
		kobject_del(&sensor->data_fields_kobj);
		goto	err_ret;
	}

	rv = kobject_init_and_add(&sensor->props_kobj, &sc_subdir_kobj_type,
		&sensor->kobj, "properties");
	if (rv) {
		rv = -EFAULT;
err_ret3:
		kobject_put(&sensor->props_kobj);
		kobject_del(&sensor->props_kobj);
		goto	err_ret2;
	}

	/*
	 * Create kobjects for data_fields
	 */
	for (i = 0; i < sensor->num_data_fields; ++i)
		if (sensor->data_fields[i].name)
			for (j = i-1; j >= 0; --j)	/*use index as a temp
							variable*/
				if (sensor->data_fields[j].name &&
					!strcmp(sensor->data_fields[i].name,
						sensor->data_fields[j].name)) {
					if (!sensor->data_fields[j].index)
						sensor->data_fields[j].index++;
					sensor->data_fields[i].index =
						sensor->data_fields[j].index
									+ 1;
					break;
				}

	for (i = 0; i < sensor->num_data_fields; ++i) {
		if (sensor->data_fields[i].name) {
			if (sensor->data_fields[i].index) {
				char *p = kasprintf(GFP_KERNEL, "%s#%d",
					sensor->data_fields[i].name,
					sensor->data_fields[i].index-1);
				kfree(sensor->data_fields[i].name);
				sensor->data_fields[i].name = p;
			}

			/* Mark index */
			sensor->data_fields[i].index = i;

			rv = kobject_init_and_add(&sensor->data_fields[i].kobj,
				&sc_datafield_kobj_type,
				&sensor->data_fields_kobj,
				sensor->data_fields[i].name);
			ISH_DBG_PRINT(KERN_ALERT
				"%s(): kobject_init_and_add() for data_field '%s' returned %d\n",
				__func__, sensor->data_fields[i].name, rv);
		}
	}
	ISH_DBG_PRINT(KERN_ALERT "%s(): sample_size=%u\n",
		__func__, sensor->sample_size);

	/*
	 * Create kobjects for properties
	 */
	for (i = 0; i < sensor->num_properties; ++i) {
		if (sensor->properties[i].name) {
			rv = kobject_init_and_add(&sensor->properties[i].kobj,
				&sc_sensprop_kobj_type, &sensor->props_kobj,
				sensor->properties[i].name);
			ISH_DBG_PRINT(KERN_ALERT
				"%s(): kobject_init_and_add() for property '%s' returned %d\n",
				__func__, sensor->properties[i].name, rv);
		}
	}

	/* Sample size should be set by the caller to size of raw data */
	sensor->sample_size += offsetof(struct senscol_sample, data);

	return	0;
}
EXPORT_SYMBOL(add_senscol_sensor);

struct sensor_def	*get_senscol_sensor_by_name(const char *name)
{
	struct sensor_def	*sens, *next;
	unsigned long	flags;

	spin_lock_irqsave(&senscol_lock, flags);
	list_for_each_entry_safe(sens, next, &senscol_sensors_list, link) {
		if (!strcmp(sens->name, name)) {
			spin_unlock_irqrestore(&senscol_lock, flags);
			return	sens;
		}
	}

	spin_unlock_irqrestore(&senscol_lock, flags);
	return	NULL;
}
EXPORT_SYMBOL(get_senscol_sensor_by_name);

struct sensor_def	*get_senscol_sensor_by_id(uint32_t id)
{
	struct sensor_def	*sens, *next;
	unsigned long	flags;

	spin_lock_irqsave(&senscol_lock, flags);
	list_for_each_entry_safe(sens, next, &senscol_sensors_list, link) {
		if (sens->id == id) {
			spin_unlock_irqrestore(&senscol_lock, flags);
			return	sens;
		}
	}

	spin_unlock_irqrestore(&senscol_lock, flags);
	return	NULL;
}
EXPORT_SYMBOL(get_senscol_sensor_by_id);

/* Add data field to existing sensor */
int	add_data_field(struct sensor_def *sensor, struct data_field *data)
{
	struct data_field	*temp;

	temp = krealloc(sensor->data_fields,
		(sensor->num_data_fields + 1) * sizeof(struct data_field),
		GFP_KERNEL);
	if (!temp)
		return	-ENOMEM;

	data->sensor = sensor;
	memcpy(&temp[sensor->num_data_fields++], data,
		sizeof(struct data_field));
	sensor->data_fields = temp;
	return	0;
}
EXPORT_SYMBOL(add_data_field);

/* Add property to existing sensor */
int	add_sens_property(struct sensor_def *sensor, struct sens_property *prop)
{
	struct sens_property	*temp;

	temp = krealloc(sensor->properties,
		(sensor->num_properties + 1) * sizeof(struct sens_property),
		GFP_KERNEL);
	if (!temp)
		return	-ENOMEM;

	prop->sensor = sensor;		/* The needed backlink */
	memcpy(&temp[sensor->num_properties++], prop,
		sizeof(struct sens_property));
	sensor->properties = temp;
	return	0;
}
EXPORT_SYMBOL(add_sens_property);

/*
 * Push data sample in upstream buffer towards user-mode.
 * Sample's size is determined from the structure
 *
 * Samples are queued is a simple FIFO binary buffer with head and tail
 * pointers.
 * Additional fields if wanted to be communicated to user mode can be defined
 *
 * Returns 0 on success, negative error code on error
 */
int	push_sample(uint32_t id, void *sample)
{
	struct sensor_def	*sensor;
	unsigned long flags;
	unsigned char	sample_buf[1024];
	struct senscol_sample	*p_sample = (struct senscol_sample *)sample_buf;
	struct sensor_def pseudo_event_sensor;

	ISH_DBG_PRINT(KERN_ALERT "[senscol]: %s():+++\n", __func__);
	g_ish_print_log("%s() DATA from sensor #%x\n", __func__, id);

	if (!senscol_data_buf)
		return	-ENOMEM;

	ISH_DBG_PRINT(KERN_ALERT "[senscol]: %s(): senscol_data_buf=%p\n",
		__func__, senscol_data_buf);

	if (id & PSEUSO_EVENT_BIT) {
		pseudo_event_sensor.sample_size = sizeof(uint32_t) +
			offsetof(struct senscol_sample, data);
		sensor = &pseudo_event_sensor;
	} else
		sensor = get_senscol_sensor_by_id(id);

	if (!sensor)
		return	-ENODEV;

	ISH_DBG_PRINT(KERN_ALERT "[senscol]: %s(): sensor=%p\n",
		__func__, sensor);
	ISH_DBG_PRINT(KERN_ALERT
		"[senscol]: %s(): senscol_data_head=%u senscol_data_tail=%u sample_size=%u\n",
		__func__, senscol_data_head, senscol_data_tail,
		sensor->sample_size);

	spin_lock_irqsave(&senscol_data_lock, flags);

	/*
	 * TBD: when buffer overflows we may choose to drop
	 * the new data or oldest data.
	 */
	/* Here we drop the new data */
	if (senscol_data_head != senscol_data_tail &&
			(senscol_data_head - senscol_data_tail) %
			SENSCOL_DATA_BUF_SIZE <= sensor->sample_size) {
		spin_unlock_irqrestore(&senscol_data_lock, flags);
		ISH_DBG_PRINT(KERN_ALERT
			"[senscol]: %s(): dropping sample, senscol_data_head=%u senscol_data_tail=%u sample size=%u\n",
			__func__, senscol_data_head, senscol_data_tail,
			sensor->sample_size);
		return	-ENOMEM;
	}

	p_sample->id = id;
	p_sample->size = sensor->sample_size;
	memcpy(p_sample->data, sample,
		sensor->sample_size - offsetof(struct senscol_sample, data));

	memcpy(senscol_data_buf + senscol_data_tail, p_sample, p_sample->size);
	senscol_data_tail += sensor->sample_size;
	if (senscol_data_tail > SENSCOL_DATA_BUF_LAST)
		senscol_data_tail = 0;

	spin_unlock_irqrestore(&senscol_data_lock, flags);

	/* Fire event through "data/event" */
	ISH_DBG_PRINT(KERN_ALERT
		"[senscol] %s(): firing data-ready event senscol_data_head=%u senscol_data_tail=%u id=%08X sample_size=%u\n",
		__func__, senscol_data_head, senscol_data_tail, p_sample->id,
		sensor->sample_size);

	if (waitqueue_active(&senscol_read_wait))
		wake_up_interruptible(&senscol_read_wait);

	ISH_DBG_PRINT(KERN_ALERT "[senscol]: %s():--- 0\n", __func__);
	return	0;
}
EXPORT_SYMBOL(push_sample);


static int senscol_open(struct inode *inode, struct file *file)
{
	user_task = current;
	return	0;
}

static int senscol_release(struct inode *inode, struct file *file)
{
	return	0;
}

static ssize_t senscol_read(struct file *file, char __user *ubuf,
	size_t length, loff_t *offset)
{
	return	length;
}

static ssize_t senscol_write(struct file *file, const char __user *ubuf,
	size_t length, loff_t *offset)
{
	return	length;
}

static long senscol_ioctl(struct file *file, unsigned int cmd,
	unsigned long data)
{
	return	0;
}

static unsigned int senscol_poll(struct file *file, poll_table *wait)
{
	unsigned int mask = 0;
	unsigned long   flags;
	int	rd_ready = 0;

	poll_wait(file, &senscol_read_wait, wait);

	/* If read buffer is empty, wait again on senscol_read_wait */

	spin_lock_irqsave(&senscol_data_lock, flags);
	rd_ready = (senscol_data_head != senscol_data_tail);
	spin_unlock_irqrestore(&senscol_data_lock, flags);

	if (rd_ready)
		mask |= (POLLIN | POLLRDNORM);
	/*mask |= DEFAULT_POLLMASK|POLLERR|POLLPRI;*/
	return	mask;
}

/* flush callback */
void senscol_flush_cb(void)
{
	struct sensor_def	*sens, *next;
	unsigned long   flags;
	uint32_t pseudo_event_id;
	uint32_t pseudo_event_content = 0;

	spin_lock_irqsave(&senscol_lock, flags);
	if (!flush_asked) {
		spin_unlock_irqrestore(&senscol_lock, flags);
		return;
	}

	list_for_each_entry_safe(sens, next, &senscol_sensors_list, link) {
		if (sens->flush_req) {
			sens->flush_req = 0;
			pseudo_event_id = sens->id | PSEUSO_EVENT_BIT;
			pseudo_event_content |= FLUSH_CMPL_BIT;

			spin_unlock_irqrestore(&senscol_lock, flags);
			push_sample(pseudo_event_id, &pseudo_event_content);
			spin_lock_irqsave(&senscol_lock, flags);
		}
	}
	flush_asked = 0;
	spin_unlock_irqrestore(&senscol_lock, flags);
	return;
}


/*
 * file operations structure will be used for heci char device.
 */
static const struct file_operations senscol_fops = {
	.owner = THIS_MODULE,
	.read = senscol_read,
	.unlocked_ioctl = senscol_ioctl,
	.open = senscol_open,
	.release = senscol_release,
	.write = senscol_write,
	.poll = senscol_poll,
	.llseek = no_llseek
};

/*
 * Misc Device Struct
 */
static struct miscdevice  senscol_misc_device = {
		.name = "sensor-collection",
		.fops = &senscol_fops,
		.minor = MISC_DYNAMIC_MINOR,
};


static int __init senscol_init(void)
{
	int	rv;

	ISH_DBG_PRINT(KERN_ALERT "[senscol]: %s():+++\n", __func__);

	INIT_LIST_HEAD(&senscol_impl_list);
	INIT_LIST_HEAD(&senscol_sensors_list);
	spin_lock_init(&senscol_lock);
	spin_lock_init(&senscol_data_lock);
	init_waitqueue_head(&senscol_read_wait);

	/* Init data buffer */
	senscol_data_buf = kmalloc(SENSCOL_DATA_BUF_SIZE, GFP_KERNEL);
	if (!senscol_data_buf)
		return	-ENOMEM;
	ISH_DBG_PRINT(KERN_ALERT
		"[senscol] %s(): allocated senscol_data_buf of size %u\n",
		__func__, SENSCOL_DATA_BUF_SIZE);

	senscol_data_head = 0;
	senscol_data_tail = 0;

	/* Create sensor_collection platform device and default sysfs entries */
	sc_pdev = platform_device_register_simple("sensor_collection", -1,
		NULL, 0);
	if (IS_ERR(sc_pdev)) {
		ISH_DBG_PRINT(KERN_ERR
			"%s(): failed to create platform device sensor_collection\n",
			__func__);
		kfree(senscol_data_buf);
		return	-ENODEV;
	}

	senscol_misc_device.parent = &sc_pdev->dev;
	rv = misc_register(&senscol_misc_device);
	if (rv)
		return	rv;

	rv = kobject_init_and_add(&sc_data_kobj, &sc_data_kobj_type,
		&sc_pdev->dev.kobj, "data");
	ISH_DBG_PRINT(KERN_ALERT
		"%s(): kobject_init_and_add() for 'data' returned %d\n",
		__func__, rv);

	rv = sysfs_create_bin_file(&sc_data_kobj, &sensors_data_binattr);
	if (rv)
		ISH_DBG_PRINT(KERN_ERR
			"%s(): sysfs_create_bin_file() for 'sensors_data' returned %d\n",
			__func__, rv);

	register_flush_cb(senscol_flush_cb);

	return	0;
}

static void __exit senscol_exit(void)
{
	ISH_DBG_PRINT(KERN_ALERT "[senscol]: %s():+++\n", __func__);
	kfree(senscol_data_buf);
}


module_init(senscol_init);
module_exit(senscol_exit);

MODULE_DESCRIPTION("Sensor Collection framework core");
MODULE_AUTHOR("Intel Corporation");
MODULE_LICENSE("GPL");

