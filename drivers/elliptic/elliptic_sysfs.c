#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include "elliptic_device.h"
#include "elliptic_sysfs.h"
#include "elliptic_mixer_controls.h"

static int kobject_create_and_add_failed;
static int sysfs_create_group_failed;

static ssize_t calibration_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count) {

	ssize_t result;

	struct elliptic_shared_data_block *calibration_obj =
		elliptic_get_shared_obj(ELLIPTIC_OBJ_ID_CALIBRATION_DATA);

	if (calibration_obj == NULL) {
		EL_PRINT_E("calibration_obj is NULL");
		return -EINVAL;
	}

	if (count > calibration_obj->size) {
		EL_PRINT_E("write length %zu larger than buffer", count);
		return 0;
	}

	memcpy(calibration_obj->buffer, buf, count);
	result = (ssize_t)count;
	return result;
}

static ssize_t calibration_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t result;
	int length;
	int i;
	uint8_t *caldata;

	struct elliptic_shared_data_block *calibration_obj =
		elliptic_get_shared_obj(ELLIPTIC_OBJ_ID_CALIBRATION_DATA);

	if (kobject_create_and_add_failed)
		EL_PRINT_E("kobject_create_and_add_failed");

	if (sysfs_create_group_failed)
		EL_PRINT_E("sysfs_create_group_failed");

	if (calibration_obj == NULL) {
		EL_PRINT_E("calibration_obj is NULL");
		return -EINVAL;
	}

	if (calibration_obj->size > PAGE_SIZE) {
		EL_PRINT_E("calibration_obj->size > PAGE_SIZE");
		return -EINVAL;
	}

	caldata = (uint8_t *)calibration_obj->buffer;
	length = 0;
	for (i = 0; i < calibration_obj->size; ++i)
		length += snprintf(buf + length, PAGE_SIZE - length, "0x%02x ",
			caldata[i]);

	length += snprintf(buf + length, PAGE_SIZE - length, "\n");
	result = (ssize_t)length;
	return result;
}

static ssize_t version_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t result;
	struct elliptic_engine_version_info *version_info;
	int length;

	struct elliptic_shared_data_block *version_obj =
		elliptic_get_shared_obj(ELLIPTIC_OBJ_ID_VERSION_INFO);

	if (kobject_create_and_add_failed)
		EL_PRINT_E("kobject_create_and_add_failed");

	if (sysfs_create_group_failed)
		EL_PRINT_E("sysfs_create_group_failed");

	if (version_obj == NULL) {
		EL_PRINT_E("version_obj is NULL");
		return -EINVAL;
	}

	if (version_obj->size > PAGE_SIZE) {
		EL_PRINT_E("version_obj->size > PAGE_SIZE");
		return -EINVAL;
	}

	version_info = (struct elliptic_engine_version_info *)
		version_obj->buffer;

	length = snprintf(buf, PAGE_SIZE, "%d.%d.%d.%d\n",
		version_info->major, version_info->minor, version_info->build,
		version_info->revision);

	result = (ssize_t)length;
	return result;
}

static ssize_t branch_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int length;

	struct elliptic_shared_data_block *branch_obj =
		elliptic_get_shared_obj(ELLIPTIC_OBJ_ID_BRANCH_INFO);

	if (branch_obj == NULL) {
		EL_PRINT_E("branch_obj not found");
		return 0;
	}

	if (branch_obj->size > PAGE_SIZE) {
		EL_PRINT_E("branch_obj->size > PAGE_SIZE");
		return -EINVAL;
	}

	length = snprintf(buf, PAGE_SIZE - 1, "%s\n",
		(const char *)(branch_obj->buffer));

	return (ssize_t)length;
}


static struct device_attribute calibration_attr = __ATTR_RW(calibration);
static struct device_attribute version_attr = __ATTR_RO(version);
static struct device_attribute branch_attr = __ATTR_RO(branch);

static struct attribute *elliptic_attrs[] = {
	&calibration_attr.attr,
	&version_attr.attr,
	&branch_attr.attr,
	NULL,
};

static struct attribute_group elliptic_attr_group = {
	.name = ELLIPTIC_SYSFS_ENGINE_FOLDER,
	.attrs = elliptic_attrs,
};

static struct kobject *elliptic_sysfs_kobj;

int elliptic_initialize_sysfs(void)
{
	int err;

	elliptic_sysfs_kobj = kobject_create_and_add(ELLIPTIC_SYSFS_ROOT_FOLDER,
		kernel_kobj->parent);

	if (!elliptic_sysfs_kobj) {
		kobject_create_and_add_failed = 1;
		EL_PRINT_E("failed to create kobj");
		return -ENOMEM;
	}

	err = sysfs_create_group(elliptic_sysfs_kobj, &elliptic_attr_group);

	if (err) {
		sysfs_create_group_failed = 1;
		EL_PRINT_E("failed to create sysfs group");
		kobject_put(elliptic_sysfs_kobj);
		return -ENOMEM;
	}

	return 0;
}

void elliptic_cleanup_sysfs(void)
{
	kobject_put(elliptic_sysfs_kobj);
}
