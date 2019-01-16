/*
 * kernel/power/tuxonice_sysfs.c
 *
 * Copyright (C) 2002-2010 Nigel Cunningham (nigel at tuxonice net)
 *
 * This file is released under the GPLv2.
 *
 * This file contains support for sysfs entries for tuning TuxOnIce.
 *
 * We have a generic handler that deals with the most common cases, and
 * hooks for special handlers to use.
 */

#include <linux/suspend.h>

#include "tuxonice_sysfs.h"
#include "tuxonice.h"
#include "tuxonice_storage.h"
#include "tuxonice_alloc.h"

static int toi_sysfs_initialised;

static void toi_initialise_sysfs(void);

static struct toi_sysfs_data sysfs_params[];

#define to_sysfs_data(_attr) container_of(_attr, struct toi_sysfs_data, attr)

static void toi_main_wrapper(void)
{
	toi_try_hibernate();
}

static ssize_t toi_attr_show(struct kobject *kobj, struct attribute *attr, char *page)
{
	struct toi_sysfs_data *sysfs_data = to_sysfs_data(attr);
	int len = 0;
	int full_prep = sysfs_data->flags & SYSFS_NEEDS_SM_FOR_READ;

	if (full_prep && toi_start_anything(0))
		return -EBUSY;

	if (sysfs_data->flags & SYSFS_NEEDS_SM_FOR_READ)
		toi_prepare_usm();

	switch (sysfs_data->type) {
	case TOI_SYSFS_DATA_CUSTOM:
		len = (sysfs_data->data.special.read_sysfs) ?
		    (sysfs_data->data.special.read_sysfs) (page, PAGE_SIZE)
		    : 0;
		break;
	case TOI_SYSFS_DATA_BIT:
		len = sprintf(page, "%d\n",
			      -test_bit(sysfs_data->data.bit.bit, sysfs_data->data.bit.bit_vector));
		break;
	case TOI_SYSFS_DATA_INTEGER:
		len = sprintf(page, "%d\n", *(sysfs_data->data.integer.variable));
		break;
	case TOI_SYSFS_DATA_LONG:
		len = sprintf(page, "%ld\n", *(sysfs_data->data.a_long.variable));
		break;
	case TOI_SYSFS_DATA_UL:
		len = sprintf(page, "%lu\n", *(sysfs_data->data.ul.variable));
		break;
	case TOI_SYSFS_DATA_STRING:
		len = sprintf(page, "%s\n", sysfs_data->data.string.variable);
		break;
	}

	if (sysfs_data->flags & SYSFS_NEEDS_SM_FOR_READ)
		toi_cleanup_usm();

	if (full_prep)
		toi_finish_anything(0);

	return len;
}

#define BOUND(_variable, _type) do { \
	if (*_variable < sysfs_data->data._type.minimum) \
		*_variable = sysfs_data->data._type.minimum; \
	else if (*_variable > sysfs_data->data._type.maximum) \
		*_variable = sysfs_data->data._type.maximum; \
} while (0)

static ssize_t toi_attr_store(struct kobject *kobj, struct attribute *attr,
			      const char *my_buf, size_t count)
{
	int assigned_temp_buffer = 0, result = count;
	struct toi_sysfs_data *sysfs_data = to_sysfs_data(attr);

	if (toi_start_anything((sysfs_data->flags & SYSFS_HIBERNATE_OR_RESUME)))
		return -EBUSY;

	((char *)my_buf)[count] = 0;

	if (sysfs_data->flags & SYSFS_NEEDS_SM_FOR_WRITE)
		toi_prepare_usm();

	switch (sysfs_data->type) {
	case TOI_SYSFS_DATA_CUSTOM:
		if (sysfs_data->data.special.write_sysfs)
			result = (sysfs_data->data.special.write_sysfs) (my_buf, count);
		break;
	case TOI_SYSFS_DATA_BIT:
		{
			unsigned long value;
			result = strict_strtoul(my_buf, 0, &value);
			if (result)
				break;
			if (value)
				set_bit(sysfs_data->data.bit.bit,
					(sysfs_data->data.bit.bit_vector));
			else
				clear_bit(sysfs_data->data.bit.bit,
					  (sysfs_data->data.bit.bit_vector));
		}
		break;
	case TOI_SYSFS_DATA_INTEGER:
		{
			long temp;
			result = strict_strtol(my_buf, 0, &temp);
			if (result)
				break;
			*(sysfs_data->data.integer.variable) = (int)temp;
			BOUND(sysfs_data->data.integer.variable, integer);
			break;
		}
	case TOI_SYSFS_DATA_LONG:
		{
			long *variable = sysfs_data->data.a_long.variable;
			result = strict_strtol(my_buf, 0, variable);
			if (result)
				break;
			BOUND(variable, a_long);
			break;
		}
	case TOI_SYSFS_DATA_UL:
		{
			unsigned long *variable = sysfs_data->data.ul.variable;
			result = strict_strtoul(my_buf, 0, variable);
			if (result)
				break;
			BOUND(variable, ul);
			break;
		}
		break;
	case TOI_SYSFS_DATA_STRING:
		{
			int copy_len = count;
			char *variable = sysfs_data->data.string.variable;

			if (sysfs_data->data.string.max_length &&
			    (copy_len > sysfs_data->data.string.max_length))
				copy_len = sysfs_data->data.string.max_length;

			if (!variable) {
				variable = (char *)toi_get_zeroed_page(31, TOI_ATOMIC_GFP);
				sysfs_data->data.string.variable = variable;
				assigned_temp_buffer = 1;
			}
			strncpy(variable, my_buf, copy_len);
			if (copy_len && my_buf[copy_len - 1] == '\n')
				variable[count - 1] = 0;
			variable[count] = 0;
		}
		break;
	}

	if (!result)
		result = count;

	/* Side effect routine? */
	if (result == count && sysfs_data->write_side_effect)
		sysfs_data->write_side_effect();

	/* Free temporary buffers */
	if (assigned_temp_buffer) {
		toi_free_page(31, (unsigned long)sysfs_data->data.string.variable);
		sysfs_data->data.string.variable = NULL;
	}

	if (sysfs_data->flags & SYSFS_NEEDS_SM_FOR_WRITE)
		toi_cleanup_usm();

	toi_finish_anything(sysfs_data->flags & SYSFS_HIBERNATE_OR_RESUME);

	return result;
}

static struct sysfs_ops toi_sysfs_ops = {
	.show = &toi_attr_show,
	.store = &toi_attr_store,
};

static struct kobj_type toi_ktype = {
	.sysfs_ops = &toi_sysfs_ops,
};

struct kobject *tuxonice_kobj;

/* Non-module sysfs entries.
 *
 * This array contains entries that are automatically registered at
 * boot. Modules and the console code register their own entries separately.
 */

static struct toi_sysfs_data sysfs_params[] = {
	SYSFS_CUSTOM("do_hibernate", SYSFS_WRITEONLY, NULL, NULL,
		     SYSFS_HIBERNATING, toi_main_wrapper),
	SYSFS_CUSTOM("do_resume", SYSFS_WRITEONLY, NULL, NULL,
		     SYSFS_RESUMING, toi_try_resume)
};

void remove_toi_sysdir(struct kobject *kobj)
{
	if (!kobj)
		return;

	kobject_put(kobj);
}

struct kobject *make_toi_sysdir(char *name)
{
	struct kobject *kobj = kobject_create_and_add(name, tuxonice_kobj);

	if (!kobj) {
		printk(KERN_INFO "TuxOnIce: Can't allocate kobject for sysfs " "dir!\n");
		return NULL;
	}

	kobj->ktype = &toi_ktype;

	return kobj;
}

/* toi_register_sysfs_file
 *
 * Helper for registering a new /sysfs/tuxonice entry.
 */

int toi_register_sysfs_file(struct kobject *kobj, struct toi_sysfs_data *toi_sysfs_data)
{
	int result;

	if (!toi_sysfs_initialised)
		toi_initialise_sysfs();

	result = sysfs_create_file(kobj, &toi_sysfs_data->attr);
	if (result)
		printk(KERN_INFO "TuxOnIce: sysfs_create_file for %s "
		       "returned %d.\n", toi_sysfs_data->attr.name, result);
	kobj->ktype = &toi_ktype;

	return result;
}
EXPORT_SYMBOL_GPL(toi_register_sysfs_file);

/* toi_unregister_sysfs_file
 *
 * Helper for removing unwanted /sys/power/tuxonice entries.
 *
 */
void toi_unregister_sysfs_file(struct kobject *kobj, struct toi_sysfs_data *toi_sysfs_data)
{
	sysfs_remove_file(kobj, &toi_sysfs_data->attr);
}
EXPORT_SYMBOL_GPL(toi_unregister_sysfs_file);

void toi_cleanup_sysfs(void)
{
	int i, numfiles = sizeof(sysfs_params) / sizeof(struct toi_sysfs_data);

	if (!toi_sysfs_initialised)
		return;

	for (i = 0; i < numfiles; i++)
		toi_unregister_sysfs_file(tuxonice_kobj, &sysfs_params[i]);

	kobject_put(tuxonice_kobj);
	toi_sysfs_initialised = 0;
}

/* toi_initialise_sysfs
 *
 * Initialise the /sysfs/tuxonice directory.
 */

static void toi_initialise_sysfs(void)
{
	int i;
	int numfiles = sizeof(sysfs_params) / sizeof(struct toi_sysfs_data);

	if (toi_sysfs_initialised)
		return;

	/* Make our TuxOnIce directory a child of /sys/power */
	tuxonice_kobj = kobject_create_and_add("tuxonice", power_kobj);
	if (!tuxonice_kobj)
		return;

	toi_sysfs_initialised = 1;

	for (i = 0; i < numfiles; i++)
		toi_register_sysfs_file(tuxonice_kobj, &sysfs_params[i]);
}

int toi_sysfs_init(void)
{
	toi_initialise_sysfs();
	return 0;
}

void toi_sysfs_exit(void)
{
	toi_cleanup_sysfs();
}
