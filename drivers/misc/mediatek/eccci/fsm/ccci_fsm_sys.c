/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#include <linux/kobject.h>
#include "ccci_fsm_internal.h"
#include "ccci_fsm_sys.h"

#define CCCI_KOBJ_NAME "md"

struct mdee_info_collect mdee_collect;

void fsm_sys_mdee_info_notify(char *buf)
{
	spin_lock(&mdee_collect.mdee_info_lock);
	memset(mdee_collect.mdee_info, 0x0, AED_STR_LEN);
	snprintf(mdee_collect.mdee_info, AED_STR_LEN, "%s", buf);
	spin_unlock(&mdee_collect.mdee_info_lock);
}

static struct kobject fsm_kobj;

struct ccci_attribute {
	struct attribute attr;
	ssize_t (*show)(char *buf);
	ssize_t (*store)(const char *buf, size_t count);
};

#define CCCI_ATTR(_name, _mode, _show, _store)			\
static struct ccci_attribute ccci_attr_##_name = {		\
	.attr = {.name = __stringify(_name), .mode = _mode },	\
	.show = _show,						\
	.store = _store,					\
}

static void fsm_obj_release(struct kobject *kobj)
{
	CCCI_NORMAL_LOG(-1, FSM, "release fsm kobject\n");
}

static ssize_t ccci_attr_show(struct kobject *kobj,
				struct attribute *attr, char *buf)
{
	ssize_t len = 0;

	struct ccci_attribute *a
		= container_of(attr, struct ccci_attribute, attr);

	if (a->show)
		len = a->show(buf);

	return len;
}

static ssize_t ccci_attr_store(struct kobject *kobj,
		struct attribute *attr, const char *buf, size_t count)
{
	ssize_t len = 0;

	struct ccci_attribute *a
		= container_of(attr, struct ccci_attribute, attr);

	if (a->store)
		len = a->store(buf, count);

	return len;
}

static const struct sysfs_ops fsm_sysfs_ops = {
	.show = ccci_attr_show,
	.store = ccci_attr_store,
};

static ssize_t ccci_mdee_info_show(char *buf)
{
	int curr = 0;

	spin_lock(&mdee_collect.mdee_info_lock);
	curr = snprintf(buf, AED_STR_LEN, "%s\n", mdee_collect.mdee_info);
	spin_unlock(&mdee_collect.mdee_info_lock);

	return curr;
}

CCCI_ATTR(mdee, 0444, &ccci_mdee_info_show, NULL);

/* Sys -- Add to group */
static struct attribute *ccci_default_attrs[] = {
	&ccci_attr_mdee.attr,
	NULL
};

static struct kobj_type fsm_ktype = {
	.release = fsm_obj_release,
	.sysfs_ops = &fsm_sysfs_ops,
	.default_attrs = ccci_default_attrs
};

int fsm_sys_init(void)
{
	int ret = 0;

	spin_lock_init(&mdee_collect.mdee_info_lock);

	ret = kobject_init_and_add(&fsm_kobj, &fsm_ktype,
			kernel_kobj, CCCI_KOBJ_NAME);
	if (ret < 0) {
		kobject_put(&fsm_kobj);
		CCCI_ERROR_LOG(-1, FSM, "fail to add fsm kobject\n");
		return ret;
	}

	return ret;
}

