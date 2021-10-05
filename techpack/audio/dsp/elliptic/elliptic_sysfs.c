#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <elliptic/elliptic_device.h>
#include <elliptic/elliptic_sysfs.h>
#include "elliptic_version.h"
#include <elliptic/elliptic_mixer_controls.h>


#define ELLIPTIC_DIAGNOSTICS_DATA_SECTION_COUNT 16
#define ELLIPTIC_CALIBRATION_MAX_DISPLAY_COUNT  96
#define ELLIPTIC_ML_DISPLAY_COUNT 16

#define elliptic_attr(_name) \
static struct kobj_attribute _name##_attr = {	\
	.attr	= {				\
		.name = __stringify(_name),	\
		.mode = 0644,			\
	},					\
	.show	= _name##_show,			\
	.store	= _name##_store,		\
}

#define elliptic_attr_ro(_name) \
static struct kobj_attribute _name##_attr = {	\
	.attr	= {				\
		.name = __stringify(_name),	\
		.mode = S_IRUGO,		\
	},					\
	.show	= _name##_show,			\
}

static int kobject_create_and_add_failed;
static int sysfs_create_group_failed;

extern struct elliptic_system_configuration_parameters_cache
	elliptic_system_configuration_cache;

static ssize_t calibration_store(struct kobject *dev,
	struct kobj_attribute *attr, const char *buf, size_t count) {

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

static ssize_t calibration_v2_store(struct kobject *dev,
	struct kobj_attribute *attr, const char *buf, size_t count) {

	ssize_t result;

	struct elliptic_shared_data_block *calibration_obj =
		elliptic_get_shared_obj(ELLIPTIC_OBJ_ID_CALIBRATION_V2_DATA);

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

static ssize_t diagnostics_store(struct kobject *dev,
	struct kobj_attribute *attr, const char *buf, size_t count) {

	ssize_t result;

	struct elliptic_shared_data_block *diagnostics_obj =
		elliptic_get_shared_obj(ELLIPTIC_OBJ_ID_DIAGNOSTICS_DATA);

	if (diagnostics_obj == NULL) {
		EL_PRINT_E("diagnostics_obj is NULL");
		return -EINVAL;
	}

	if (count > diagnostics_obj->size) {
		EL_PRINT_E("write length %zu larger than buffer", count);
		return 0;
	}

	memcpy(diagnostics_obj->buffer, buf, count);
	result = (ssize_t)count;
	return result;
}

static ssize_t ml_store(struct kobject *dev,
	struct kobj_attribute *attr, const char *buf, size_t count) {

	ssize_t result;

	struct elliptic_shared_data_block *ml_obj =
		elliptic_get_shared_obj(ELLIPTIC_OBJ_ID_ML_DATA);

	if (ml_obj == NULL) {
		EL_PRINT_E("ml_obj is NULL");
		return -EINVAL;
	}

	if (count > ml_obj->size) {
		EL_PRINT_E("write length %zu larger than buffer", count);
		return 0;
	}

	memcpy(ml_obj->buffer, buf, count);
	result = (ssize_t)count;
	return result;
}

static ssize_t calibration_show_core(struct kobject *dev,
	struct kobj_attribute *attr, char *buf, int pretty)
{
	ssize_t result;
	int length = 0;
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
	if (pretty) {
		if (caldata[0] == 0xDE &&
			caldata[1] == 0xAD) {
			length += snprintf(buf + length, PAGE_SIZE - length,
								"Calibration Data: not loaded");
		} else {
			length += snprintf(buf + length, PAGE_SIZE - length,
								"Calibration Data: ");
			for (i = 0; i < calibration_obj->size; ++i)
				length += snprintf(buf + length, PAGE_SIZE - length,
								"0x%02x ", caldata[i]);
		}
	} else {
		for (i = 0; i < calibration_obj->size; ++i)
			length += snprintf(buf + length, PAGE_SIZE - length,
								"0x%02x ", caldata[i]);
	}
	length += snprintf(buf + length, PAGE_SIZE - length, "\n\n");
	result = (ssize_t)length;
	return result;
}

static ssize_t calibration_show(struct kobject *dev,
	struct kobj_attribute *attr, char *buf)
{
	return calibration_show_core(dev, attr, buf, 0);
}

static ssize_t calibration_v2_show_core(struct kobject *dev,
	struct kobj_attribute *attr, char *buf, int pretty)
{
	ssize_t result;
	int length = 0;
	int i;
	uint8_t *caldata;

	struct elliptic_shared_data_block *calibration_obj =
		elliptic_get_shared_obj(ELLIPTIC_OBJ_ID_CALIBRATION_V2_DATA);

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
	if (pretty) {
		if (caldata[0] == 0xDE &&
			caldata[1] == 0xAD) {
			length += snprintf(buf + length, PAGE_SIZE - length,
								"Calibration Ext Data: not loaded");
		} else {
			int j = (ELLIPTIC_CALIBRATION_V2_DATA_SIZE>>2) - 1;

			length += snprintf(buf + length, PAGE_SIZE - length,
								"Calibration Ext Data: ");
			for (i = 0; i < ELLIPTIC_CALIBRATION_MAX_DISPLAY_COUNT; ++i)
				length += snprintf(buf + length, PAGE_SIZE - length,
								"0x%02x ", caldata[i]);
			length += snprintf(buf + length, PAGE_SIZE - length,
								"\nTruncated at %d",
								ELLIPTIC_CALIBRATION_MAX_DISPLAY_COUNT);
			length += snprintf(buf + length, PAGE_SIZE - length,
						"\nmisc: %u %u %u %u %u %u %u %u\n",
						caldata[j-7], caldata[j-6], caldata[j-5],
						caldata[j-4], caldata[j-3], caldata[j-2],
						caldata[j-1], caldata[j]);
		}
	} else {
		for (i = 0; i < calibration_obj->size; ++i)
			length += snprintf(buf + length, PAGE_SIZE - length,
								"0x%02x ", caldata[i]);
	}
	length += snprintf(buf + length, PAGE_SIZE - length, "\n\n");
	result = (ssize_t)length;
	return result;
}

static ssize_t calibration_v2_show(struct kobject *dev,
	struct kobj_attribute *attr, char *buf)
{
	return calibration_v2_show_core(dev, attr, buf, 0);
}

static ssize_t diagnostics_show_core(struct kobject *dev,
	struct kobj_attribute *attr, char *buf, int pretty)
{
	ssize_t result;
	int length = 0;
	uint32_t *data32;
	int i;

	struct elliptic_shared_data_block *diagnostics_obj =
		elliptic_get_shared_obj(ELLIPTIC_OBJ_ID_DIAGNOSTICS_DATA);

	if (kobject_create_and_add_failed)
		EL_PRINT_E("kobject_create_and_add_failed");

	if (sysfs_create_group_failed)
		EL_PRINT_E("sysfs_create_group_failed");

	if (diagnostics_obj == NULL) {
		EL_PRINT_E("diagnostics_obj is NULL");
		return -EINVAL;
	}

	if (diagnostics_obj->size > PAGE_SIZE) {
		EL_PRINT_E("diagnostics_obj->size > PAGE_SIZE");
		return -EINVAL;
	}

	length = 0;
	data32 = (uint32_t *)diagnostics_obj->buffer;

	if (pretty) {
		length += snprintf(buf + length, PAGE_SIZE - length,
							"Diagnostics:\n  counters:\n");
		for (i = 0; i < ELLIPTIC_DIAGNOSTICS_DATA_SECTION_COUNT; i++)
			length += snprintf(buf + length, PAGE_SIZE - length, "   %u %u %u %u\n",
				data32[4*i], data32[4*i+1], data32[4*i+2], data32[4*i+3]);
	} else {
		for (i = 0; i < (diagnostics_obj->size >> 4); ++i)
			length += snprintf(buf + length, PAGE_SIZE - length, "   %u %u %u %u\n",
				data32[4*i], data32[4*i+1], data32[4*i+2], data32[4*i+3]);
	}
	length += snprintf(buf + length, PAGE_SIZE - length, "\n\n");
	result = (ssize_t)length;
	return result;
}

static ssize_t diagnostics_show(struct kobject *dev,
	struct kobj_attribute *attr, char *buf)
{
	return diagnostics_show_core(dev, attr, buf, 0);
}

static ssize_t ml_show_core(struct kobject *dev,
	struct kobj_attribute *attr, char *buf, int pretty)
{
	ssize_t result;
	int length = 0;
	int i;
	uint32_t *mldata;

	struct elliptic_shared_data_block *ml_obj =
		elliptic_get_shared_obj(ELLIPTIC_OBJ_ID_ML_DATA);

	if (kobject_create_and_add_failed)
		EL_PRINT_E("kobject_create_and_add_failed");

	if (sysfs_create_group_failed)
		EL_PRINT_E("sysfs_create_group_failed");

	if (ml_obj == NULL) {
		EL_PRINT_E("ml_obj is NULL");
		return -EINVAL;
	}

	if (ml_obj->size > PAGE_SIZE) {
		EL_PRINT_E("ml_obj->size > PAGE_SIZE");
		return -EINVAL;
	}

	mldata = (uint32_t *)ml_obj->buffer;
	length = 0;
	if (pretty) {
		if (mldata[0] == 0x0 &&
			mldata[1] == 0x0) {
			length += snprintf(buf + length, PAGE_SIZE - length,
								"ML Data: not loaded");
		} else {
			length += snprintf(buf + length, PAGE_SIZE - length,
								"ML Data: ");
			for (i = 0; i < ELLIPTIC_ML_DISPLAY_COUNT; ++i)
				length += snprintf(buf + length, PAGE_SIZE - length,
								"0x%08x ", mldata[i]);
			length += snprintf(buf + length, PAGE_SIZE - length,
								"\nTruncated at %d",
								ELLIPTIC_ML_DISPLAY_COUNT);
		}
	} else {
		int values =  ml_obj->size >> 2;

		for (i = 0; i < values; ++i)
			length += snprintf(buf + length, PAGE_SIZE - length,
								"0x%08x ", mldata[i]);
	}
	length += snprintf(buf + length, PAGE_SIZE - length, "\n\n");
	result = (ssize_t)length;
	return result;
}

static ssize_t ml_show(struct kobject *dev,
	struct kobj_attribute *attr, char *buf)
{
	return ml_show_core(dev, attr, buf, 0);
}


static ssize_t version_show_core(struct kobject *dev,
	struct kobj_attribute *attr, char *buf, int pretty)
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

	if (pretty) {
		if (version_info->major == 0xDE &&
			version_info->minor == 0xAD) {
			length = snprintf(buf, PAGE_SIZE, "Version: unknown\n");
		} else {
			length = snprintf(buf, PAGE_SIZE, "Version: %d.%d.%d.%d\n",
				version_info->major, version_info->minor,
				version_info->build, version_info->revision);
		}
	} else {
		length = snprintf(buf, PAGE_SIZE, "%d.%d.%d.%d\n",
			version_info->major, version_info->minor,
			version_info->build, version_info->revision);
	}
	result = (ssize_t)length;
	return result;
}

static ssize_t version_show(struct kobject *dev,
	struct kobj_attribute *attr, char *buf)
{
	return version_show_core(dev, attr, buf, 0);
}


static ssize_t branch_show_core(struct kobject *dev,
	struct kobj_attribute *attr, char *buf, int pretty)
{
	int length = 0;

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
	if (pretty) {
		length = snprintf(buf, PAGE_SIZE - 1, "Branch: %s\n",
			(const char *)(branch_obj->buffer));
	} else {
		length = snprintf(buf, PAGE_SIZE - 1, "%s\n",
			(const char *)(branch_obj->buffer));
	}

	return (ssize_t)length;
}

static ssize_t branch_show(struct kobject *dev,
	struct kobj_attribute *attr, char *buf)
{
	return branch_show_core(dev, attr, buf, 0);
}

static ssize_t tag_show_core(struct kobject *dev,
	struct kobj_attribute *attr, char *buf, int pretty)
{
	int length = 0;

	struct elliptic_shared_data_block *tag_obj =
		elliptic_get_shared_obj(ELLIPTIC_OBJ_ID_TAG_INFO);

	if (tag_obj == NULL) {
		EL_PRINT_E("tag_obj not found");
		return 0;
	}

	if (tag_obj->size > PAGE_SIZE) {
		EL_PRINT_E("tag_obj->size > PAGE_SIZE");
		return -EINVAL;
	}
	if (pretty) {
		length = snprintf(buf, PAGE_SIZE - 1, "Tag: %s\n",
			(const char *)(tag_obj->buffer));
	} else {
		length = snprintf(buf, PAGE_SIZE - 1, "%s\n",
			(const char *)(tag_obj->buffer));
	}

	return (ssize_t)length;
}

static ssize_t tag_show(struct kobject *dev,
	struct kobj_attribute *attr, char *buf)
{
	return tag_show_core(dev, attr, buf, 0);
}

static ssize_t cache_show(char *buf, int pretty)
{
	struct elliptic_system_configuration_parameters_cache *cache =
				&elliptic_system_configuration_cache;

	int length = 0;

	length = snprintf(buf, PAGE_SIZE - 1, "Cache:\n");
	length += snprintf(buf + length, PAGE_SIZE - 1, "    mi:%d\n",
						cache->microphone_index);
	length += snprintf(buf + length, PAGE_SIZE - 1, "    om:%d\n",
						cache->operation_mode);
	length += snprintf(buf + length, PAGE_SIZE - 1, "   omf:%d\n",
						cache->operation_mode_flags);
	length += snprintf(buf + length, PAGE_SIZE - 1, "    cs:%d\n",
						cache->calibration_state);
	length += snprintf(buf + length, PAGE_SIZE - 1, "    cp:%d\n",
						cache->calibration_profile);
	length += snprintf(buf + length, PAGE_SIZE - 1, "    ug:%d\n",
						cache->ultrasound_gain);
	length += snprintf(buf + length, PAGE_SIZE - 1, "    ll:%d\n",
						cache->log_level);
	length += snprintf(buf + length, PAGE_SIZE - 1, "    es:%d\n",
						cache->engine_suspend);

	return (ssize_t)length;
}

static ssize_t opmode_show(struct kobject *dev,
	struct kobj_attribute *attr, char *buf)
{
	int length = 0;
	ssize_t result;

	struct elliptic_system_configuration_parameters_cache *cache =
				&elliptic_system_configuration_cache;

	length += snprintf(buf + length, PAGE_SIZE - 1, "%d\n",
							cache->operation_mode);
	result = (ssize_t)length;
	return result;
}

static ssize_t opmode_flags_show(struct kobject *dev,
	struct kobj_attribute *attr, char *buf)
{
	int length = 0;
	ssize_t result;
	struct elliptic_system_configuration_parameters_cache *cache =
				&elliptic_system_configuration_cache;

	length += snprintf(buf + length, PAGE_SIZE - 1, "%d\n",
							cache->operation_mode_flags);
	result = (ssize_t)length;
	return result;
}

static ssize_t driver_version_show(char *buf)
{
	int length = 0;

	length = snprintf(buf, PAGE_SIZE, "Driver version: %s-%s (%s)\n",
				build_name, build_number, build_source_version);

	return (ssize_t)length;
}

static ssize_t state_show(struct kobject *dev,
	struct kobj_attribute *attr, char *buf)
{
	int length = 0;

	length += driver_version_show(buf + length);
	length += version_show_core(dev, attr, buf + length, 1);
	if (length > PAGE_SIZE)
		return (ssize_t)0;
	length += branch_show_core(dev, attr, buf + length, 1);
	if (length > PAGE_SIZE)
		return (ssize_t)0;
	length += tag_show_core(dev, attr, buf + length, 1);
	if (length > PAGE_SIZE)
		return (ssize_t)0;
	length += calibration_show_core(dev, attr, buf + length, 1);
	if (length > PAGE_SIZE)
		return (ssize_t)0;
	length += calibration_v2_show_core(dev, attr, buf + length, 1);
	if (length > PAGE_SIZE)
		return (ssize_t)0;
	length += diagnostics_show_core(dev, attr, buf + length, 1);
	if (length > PAGE_SIZE)
		return (ssize_t)0;
	length += ml_show_core(dev, attr, buf + length, 1);
	if (length > PAGE_SIZE)
		return (ssize_t)0;
	length += cache_show(buf + length, 1);
	if (length > PAGE_SIZE)
		return (ssize_t)0;
	return (ssize_t)length;
}

elliptic_attr(calibration);
elliptic_attr(calibration_v2);
elliptic_attr(diagnostics);
elliptic_attr(ml);
elliptic_attr_ro(version);
elliptic_attr_ro(branch);
elliptic_attr_ro(state);
elliptic_attr_ro(tag);
elliptic_attr_ro(opmode);
elliptic_attr_ro(opmode_flags);

static struct attribute *elliptic_attrs[] = {
	&calibration_attr.attr,
	&version_attr.attr,
	&branch_attr.attr,
	&calibration_v2_attr.attr,
	&diagnostics_attr.attr,
	&state_attr.attr,
	&tag_attr.attr,
	&ml_attr.attr,
	&opmode_attr.attr,
	&opmode_flags_attr.attr,
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
