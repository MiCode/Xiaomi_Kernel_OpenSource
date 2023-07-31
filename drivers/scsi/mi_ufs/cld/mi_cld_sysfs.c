/*
 * mi_cld1.c
 *
 *  Created on: 2020-10-20
 *      Author: shane
 */

#include "mi_cld.h"
#include "../mi-ufshcd.h"
//#include "../mi_ufs_common_ops.h"

int ufscld_create_sysfs(struct ufscld_dev *cld);

void ufscld_remove_sysfs(struct ufscld_dev *cld)
{
	int ret;

	ret = kobject_uevent(&cld->kobj, KOBJ_REMOVE);
	INFO_MSG("kobject removed (%d)", ret);
	kobject_del(&cld->kobj);
}

static ssize_t ufscld_sysfs_show_trigger(struct ufscld_dev *cld, char *buf)
{
	INFO_MSG("cld_trigger %d", cld->cld_trigger);

	return snprintf(buf, PAGE_SIZE, "%d\n", cld->cld_trigger);
}

static ssize_t ufscld_sysfs_store_trigger(struct ufscld_dev *cld,
					  const char *buf, size_t count)
{
	unsigned long val;

	if (kstrtoul(buf, 0, &val))
		return -EINVAL;

	if (val != 0 && val != 1)
		return -EINVAL;

	INFO_MSG("cld_trigger %lu", val);

	if (val == cld->cld_trigger)
		return count;

	if (val)
		ufscld_trigger_on(cld);
	else
		ufscld_trigger_off(cld);

	return count;
}

static ssize_t ufscld_sysfs_show_trigger_interval(struct ufscld_dev *cld,
						  char *buf)
{
	INFO_MSG("cld_trigger_interval %d", cld->cld_trigger_delay);

	return snprintf(buf, PAGE_SIZE, "%d\n", cld->cld_trigger_delay);
}

static ssize_t ufscld_sysfs_store_trigger_interval(struct ufscld_dev *cld,
						   const char *buf,
						   size_t count)
{
	unsigned long val;

	if (kstrtoul(buf, 0, &val))
		return -EINVAL;

	if (val < CLD_TRIGGER_WORKER_DELAY_MS_MIN ||
	    val > CLD_TRIGGER_WORKER_DELAY_MS_MAX) {
		INFO_MSG("cld_trigger_interval (min) %4dms ~ (max) %4dms",
			 CLD_TRIGGER_WORKER_DELAY_MS_MIN,
			 CLD_TRIGGER_WORKER_DELAY_MS_MAX);
		return -EINVAL;
	}

	cld->cld_trigger_delay = (unsigned int)val;
	INFO_MSG("cld_trigger_interval %d", cld->cld_trigger_delay);

	return count;
}

static ssize_t ufscld_sysfs_show_debug(struct ufscld_dev *cld, char *buf)
{
	INFO_MSG("debug %d", cld->cld_debug);

	return snprintf(buf, PAGE_SIZE, "%d\n", cld->cld_debug);
}

static ssize_t ufscld_sysfs_show_cld_operation_status(struct ufscld_dev *cld, char *buf)
{
	int op_status;

	ufscld_get_operation_status(cld, (int *)&op_status);

	return snprintf(buf, PAGE_SIZE, "%d\n", (int)op_status);
}


static ssize_t ufscld_sysfs_store_debug(struct ufscld_dev *cld, const char *buf,
					size_t count)
{
	unsigned long val;

	if (kstrtoul(buf, 0, &val))
		return -EINVAL;

	if (val != 0 && val != 1)
		return -EINVAL;

	cld->cld_debug = val ? true : false;

	INFO_MSG("debug %d", cld->cld_debug);

	return count;
}

static ssize_t ufscld_sysfs_show_frag_level(struct ufscld_dev *cld, char *buf)
{
	int frag_level;

	ufscld_get_frag_level(cld, &frag_level);

	return snprintf(buf, PAGE_SIZE, "%d\n", frag_level);
}

static ssize_t ufscld_sysfs_show_block_suspend(struct ufscld_dev *cld,
					       char *buf)
{
	INFO_MSG("block suspend %d", cld->block_suspend);

	return snprintf(buf, PAGE_SIZE, "%d\n", cld->block_suspend);
}

static ssize_t ufscld_sysfs_store_block_suspend(struct ufscld_dev *cld,
						const char *buf, size_t count)
{
	unsigned long val;

	if (kstrtoul(buf, 0, &val))
		return -EINVAL;

	if (val != 0 && val != 1)
		return -EINVAL;

	INFO_MSG("cld_block_suspend %lu", val);

	if (val == cld->block_suspend)
		return count;

	if (val)
		ufscld_block_enter_suspend(cld);
	else
		ufscld_allow_enter_suspend(cld);

	cld->block_suspend = val ? true : false;

	return count;
}

static ssize_t ufscld_sysfs_show_auto_hibern8_enable(struct ufscld_dev *cld,
						     char *buf)
{
	INFO_MSG("HCI auto hibern8 %d", cld->is_auto_enabled);

	return snprintf(buf, PAGE_SIZE, "%d\n", cld->is_auto_enabled);
}

static ssize_t ufscld_sysfs_store_auto_hibern8_enable(struct ufscld_dev *cld,
						      const char *buf,
						      size_t count)
{
	unsigned long val;

	if (kstrtoul(buf, 0, &val))
		return -EINVAL;

	if (val != 0 && val != 1)
		return -EINVAL;

	ufscld_auto_hibern8_enable(cld, val);

	return count;
}

/* SYSFS DEFINE */
#define define_sysfs_ro(_name) __ATTR(_name, 0444,			\
				      ufscld_sysfs_show_##_name, NULL)
#define define_sysfs_rw(_name) __ATTR(_name, 0644,			\
				      ufscld_sysfs_show_##_name,	\
				      ufscld_sysfs_store_##_name)

static struct ufscld_sysfs_entry ufscld_sysfs_entries[] = {
	define_sysfs_ro(frag_level),
	define_sysfs_ro(cld_operation_status),

	define_sysfs_rw(trigger),
	define_sysfs_rw(trigger_interval),

	/* debug */
	define_sysfs_rw(debug),
	/* Attribute (RAW) */
	define_sysfs_rw(block_suspend),
	define_sysfs_rw(auto_hibern8_enable),
	__ATTR_NULL
};

static ssize_t ufscld_attr_show(struct kobject *kobj, struct attribute *attr,
				char *page)
{
	struct ufscld_sysfs_entry *entry;
	struct ufscld_dev *cld;
	ssize_t error;

	entry = container_of(attr, struct ufscld_sysfs_entry, attr);
	if (!entry->show)
		return -EIO;

	cld = container_of(kobj, struct ufscld_dev, kobj);
	if (ufscld_is_not_present(cld))
		return -ENODEV;

	mutex_lock(&cld->sysfs_lock);
	error = entry->show(cld, page);
	mutex_unlock(&cld->sysfs_lock);

	return error;
}

static ssize_t ufscld_attr_store(struct kobject *kobj, struct attribute *attr,
				 const char *page, size_t length)
{
	struct ufscld_sysfs_entry *entry;
	struct ufscld_dev *cld;
	ssize_t error;

	entry = container_of(attr, struct ufscld_sysfs_entry, attr);
	if (!entry->store)
		return -EIO;

	cld = container_of(kobj, struct ufscld_dev, kobj);
	if (ufscld_is_not_present(cld))
		return -ENODEV;

	mutex_lock(&cld->sysfs_lock);
	error = entry->store(cld, page, length);
	mutex_unlock(&cld->sysfs_lock);

	return error;
}

static const struct sysfs_ops ufscld_sysfs_ops = {
	.show = ufscld_attr_show,
	.store = ufscld_attr_store,
};

static struct kobj_type ufscld_ktype = {
	.sysfs_ops = &ufscld_sysfs_ops,
	.release = NULL,
};

 int ufscld_create_sysfs(struct ufscld_dev *cld)
{
	struct device *dev = cld->hba->dev;
	struct ufscld_sysfs_entry *entry;
	int err;

	cld->sysfs_entries = ufscld_sysfs_entries;

	kobject_init(&cld->kobj, &ufscld_ktype);
	mutex_init(&cld->sysfs_lock);

	INFO_MSG("ufscld creates sysfs ufscld %p dev->kobj %p",
		 &cld->kobj, &dev->kobj);

	err = kobject_add(&cld->kobj, kobject_get(&dev->kobj), "ufscld");
	if (!err) {
		for (entry = cld->sysfs_entries; entry->attr.name != NULL;
		     entry++) {
			INFO_MSG("ufscld sysfs attr creates: %s",
				 entry->attr.name);
			err = sysfs_create_file(&cld->kobj, &entry->attr);
			if (err) {
				ERR_MSG("create entry(%s) failed",
					entry->attr.name);
				goto kobj_del;
			}
		}
		kobject_uevent(&cld->kobj, KOBJ_ADD);
	} else {
		ERR_MSG("kobject_add failed");
	}

	return err;
kobj_del:
	err = kobject_uevent(&cld->kobj, KOBJ_REMOVE);
	INFO_MSG("kobject removed (%d)", err);
	kobject_del(&cld->kobj);
	return -EINVAL;
}
