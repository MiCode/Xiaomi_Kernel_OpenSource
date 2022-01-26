/*
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/sched.h>

#include <ut_drv.h>
#include <tz_dcih.h>
#include <tz_dcih_test.h>

#define IMSG_TAG "[tz_driver]"

#include <imsg_log.h>

#include <teei_client_main.h>
#include "../teei_fp/fp_func.h"


static uint32_t imsg_log_level = IMSG_LOG_LEVEL;
static DEFINE_MUTEX(drv_load_mutex);
unsigned long spi_ready_flag;

#ifdef CONFIG_MICROTRUST_TZDRIVER_DYNAMICAL_DEBUG
uint32_t tzdriver_dynamical_debug_flag;
#endif

static ssize_t imsg_log_test_show(struct device *cd,
			struct device_attribute *attr, char *buf)
{
	IMSG_PROFILE_S("LOG_TEST");

	IMSG_ENTER();

	IMSG_TRACE("Trace message\n");
	IMSG_DEBUG("Debug message\n");
	IMSG_INFO("Information message\n");
	IMSG_WARN("Warning message\n");
	IMSG_ERROR("Error message\n");

	IMSG_LEAVE();

	IMSG_PROFILE_E("LOG_TEST");

	return 0;
}

static DEVICE_ATTR_RO(imsg_log_test);

uint32_t get_imsg_log_level(void)
{
	return imsg_log_level;
}

static ssize_t imsg_log_level_show(struct device *cd,
			struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", get_imsg_log_level());
}

#if defined(CONFIG_MICROTRUST_DEBUG)
static void set_imsg_log_level(uint32_t lv)
{
	imsg_log_level = lv;
}

static ssize_t imsg_log_level_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t len)
{
	unsigned long new;
	int ret;

	ret = kstrtoul(buf, 0, &new);
	if (ret < 0)
		return ret;

	set_imsg_log_level(new);
	return len;
}

static DEVICE_ATTR_RW(imsg_log_level);
#else
static DEVICE_ATTR_RO(imsg_log_level);
#endif

#define DRIVER_LOADER_HOSTNAME "bta_loader"
#define UUID_STRING_LENGTH 32

static struct TEEC_Context ut_drv_context;
static bool is_context_init;
static LIST_HEAD(ut_drv_list);

/*
 * This function is a workaround solution to fix sscanf() parsing string issue.
 * sscanf() cannot correctly parsing an UUID string directly.
 * We need to separate the UUID string into chunks and then deal with them.
 */
static size_t hex_str_to_value(const char *s, size_t bytes, void *val)
{
	char tmp_buf[10] = {0};
	int ret = 0;

	memcpy(tmp_buf, s, bytes);

	switch (bytes) {
	case 8:
		ret = sscanf(tmp_buf, "%08x", (uint32_t *)val);
		break;
	case 4:
		ret = sscanf(tmp_buf, "%04hx", (uint16_t *)val);
		break;
	case 2:
		ret = sscanf(tmp_buf, "%02hhx", (uint8_t *)val);
		break;
	}

	if (ret != 1)
		return 0;

	return bytes;
}

static void str_to_uuid(struct TEEC_UUID *uuid, const char *buf)
{
	int i = 0;
	const char *s = buf;

	s += hex_str_to_value(s, 8, &uuid->timeLow);
	s += hex_str_to_value(s, 4, &uuid->timeMid);
	s += hex_str_to_value(s, 4, &uuid->timeHiAndVersion);

	for (i = 0; i < 8; i++)
		s += hex_str_to_value(s, 2, &uuid->clockSeqAndNode[i]);
}

static inline void uuid_to_str(struct TEEC_UUID *uuid, char *buf)
{
	snprintf(buf, UUID_STRING_LENGTH,
			"%08x%04x%04x%02x%02x%02x%02x%02x%02x%02x%02x",
			uuid->timeLow, uuid->timeMid,
			uuid->timeHiAndVersion,
			uuid->clockSeqAndNode[0], uuid->clockSeqAndNode[1],
			uuid->clockSeqAndNode[2], uuid->clockSeqAndNode[3],
			uuid->clockSeqAndNode[4], uuid->clockSeqAndNode[5],
			uuid->clockSeqAndNode[6], uuid->clockSeqAndNode[7]);
}

static inline void print_uuid(struct TEEC_UUID *uuid)
{
	IMSG_DEBUG("uuid: %08x-%04x-%04x-%02x%02x%02x%02x%02x%02x%02x%02x\n",
			 uuid->timeLow, uuid->timeMid, uuid->timeHiAndVersion,
			 uuid->clockSeqAndNode[0], uuid->clockSeqAndNode[1],
			 uuid->clockSeqAndNode[2], uuid->clockSeqAndNode[3],
			 uuid->clockSeqAndNode[4], uuid->clockSeqAndNode[5],
			 uuid->clockSeqAndNode[6], uuid->clockSeqAndNode[7]);
}

static bool is_uuid_equal(const struct TEEC_UUID *uuid1,
					const struct TEEC_UUID *uuid2)
{
	return !memcmp(uuid1, uuid2, sizeof(struct TEEC_UUID));
}

struct ut_drv_entry *find_ut_drv_entry_by_uuid(struct TEEC_UUID *uuid)
{
	struct ut_drv_entry *tmp_entry;

	list_for_each_entry(tmp_entry, &ut_drv_list, list)
		if (is_uuid_equal(&tmp_entry->uuid, uuid))
			return tmp_entry;

	return NULL;
}

struct ut_drv_entry *find_ut_drv_entry_by_driver_id(uint32_t driver_id)
{
	struct ut_drv_entry *tmp_entry;

	list_for_each_entry(tmp_entry, &ut_drv_list, list)
		if (tmp_entry->driver_id == driver_id)
			return tmp_entry;

	return NULL;
}

static int open_driver_session(struct ut_drv_entry *ut_drv)
{
	struct TEEC_UUID *driver_uuid = &ut_drv->uuid;
	struct TEEC_Session *session = &ut_drv->session;
	unsigned int res;

	res = TEEC_OpenSession(&ut_drv_context, session,
				driver_uuid, TEEC_LOGIN_PUBLIC,
				NULL, NULL, NULL);

	if (res != TEEC_SUCCESS) {
		IMSG_DEBUG("failed to load driver, res 0x%0x\n", res);
		return -EINVAL;
	}

	return 0;
}

static int get_driver_id(struct ut_drv_entry *ut_drv)
{
	struct TEEC_Operation op = {0};
	struct ut_drv_param driver_param;
	unsigned int res;

	driver_param.cmd_id = UT_DRV_GET_DRIVER_ID;

	prepare_params(&op, (void *)&driver_param,
					sizeof(struct ut_drv_param));

	res = TEEC_InvokeCommand(&ut_drv->session,
					driver_param.cmd_id, &op, NULL);
	if (res != TEEC_SUCCESS) {
		int ret = get_result(&op);

		IMSG_ERROR("failed to get id by UUID %08x res 0x%x ret 0x%x\n",
				ut_drv->uuid.timeLow, res, ret);
		return ret;
	}

	ut_drv->driver_id = get_result(&op);

	return 0;
}

static int load_ut_drv(struct TEEC_UUID *uuid, unsigned int flags)
{
	int res;
	int ret = 0;
	struct ut_drv_entry *new_entry, *tmp_entry;
	struct TEEC_UUID spi_uuid = { 0x93feffcc, 0xd8ca, 0x11e7,
					{ 0x96, 0xc7, 0xc7, 0xa2,
						0x1a, 0xcb, 0x49, 0x32 } };

	if (!is_teei_ready()) {
		IMSG_WARN("TEE is not ready\n");
		return -EBUSY;
	}

	mutex_lock(&drv_load_mutex);

	if (!is_context_init) {
		res = TEEC_InitializeContext(DRIVER_LOADER_HOSTNAME,
						&ut_drv_context);
		if (res != TEEC_SUCCESS) {
			IMSG_ERROR("failed to initialize context 0x%x\n", res);
			ret = -EINVAL;
			goto exit;
		}

		is_context_init = true;
	}

	tmp_entry = find_ut_drv_entry_by_uuid(uuid);
	if (tmp_entry) {
		IMSG_INFO("driver already loaded\n");
		goto exit;
	}

	new_entry = kzalloc(sizeof(struct ut_drv_entry), GFP_KERNEL);
	if (!new_entry) {
		IMSG_ERROR("failed to allocate memory for ut_drv_entry\n");
		ret = -ENOMEM;
		goto exit;
	}

	memcpy(&new_entry->uuid, uuid, sizeof(struct TEEC_UUID));
	new_entry->driver_id = 0;

	res = open_driver_session(new_entry);
	if (res != TEEC_SUCCESS) {
		ret = -EIO;
		goto fail;
	}
	if (flags == TEEI_DRV) {
		res = get_driver_id(new_entry);
		if (res != TEEC_SUCCESS) {
			ret = -EIO;
			goto fail_get_driver_id;
		}

		IMSG_DEBUG("load driver successfully, driver_id 0x%x\n",
				new_entry->driver_id);
	}
	list_add_tail(&new_entry->list, &ut_drv_list);

	if (is_uuid_equal(&new_entry->uuid, &spi_uuid)) {
		IMSG_DEBUG("spi driver is loaded successful!\n");
		spi_ready_flag = 1;
		wake_up(&__wait_spi_wq);
	}

	goto exit;

fail_get_driver_id:
	TEEC_CloseSession(&new_entry->session);
fail:
	kfree(new_entry);
exit:
	mutex_unlock(&drv_load_mutex);
	return ret;
}

int tz_load_drv(struct TEEC_UUID *uuid)
{
	return load_ut_drv(uuid, TEEI_DRV);
}
int tz_load_ta_by_str(const char *buf)
{
	struct TEEC_UUID uuid;
	size_t len = strlen(buf);
	int res;

	if (len < UUID_STRING_LENGTH) {
		IMSG_ERROR("bad UUID length, buf '%s' len %zd\n",
				buf, len);
		return -EINVAL;
	}

	str_to_uuid(&uuid, buf);
	print_uuid(&uuid);

	res = load_ut_drv(&uuid, TEEI_TA);
	if (res)
		IMSG_DEBUG("load secure ta failed(uuid: %s)\n",
				buf);

	return res;
}
int tz_load_drv_by_str(const char *buf)
{
	struct TEEC_UUID uuid;
	size_t len = strlen(buf);
	int res;

	if (len < UUID_STRING_LENGTH) {
		IMSG_ERROR("bad UUID length, buf '%s' len %zd\n",
				buf, len);
		return -EINVAL;
	}

	str_to_uuid(&uuid, buf);
	print_uuid(&uuid);

	res = load_ut_drv(&uuid, TEEI_DRV);
	if (res)
		IMSG_DEBUG("load secure driver failed(uuid: %s)\n",
				buf);

	return res;
}

static ssize_t load_ut_drv_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t len)
{
	int ret;
	struct TEEC_UUID uuid;

	if (len < UUID_STRING_LENGTH) {
		IMSG_ERROR("bad UUID length, buf '%s' len %zd\n", buf, len);
		return len;
	}

	str_to_uuid(&uuid, buf);
	print_uuid(&uuid);

	ret = load_ut_drv(&uuid, TEEI_DRV);
	if (ret)
		IMSG_ERROR("failed to load ut driver, ret %d\n", ret);

	return len;
}

static DEVICE_ATTR_WO(load_ut_drv);

static int unload_ut_drv(struct TEEC_UUID *uuid)
{
	struct ut_drv_entry *entry;

	mutex_lock(&drv_load_mutex);

	entry = find_ut_drv_entry_by_uuid(uuid);
	if (!entry) {
		IMSG_INFO("driver not found\n");
		goto exit;
	}

	TEEC_CloseSession(&entry->session);

	list_del(&entry->list);

	IMSG_DEBUG("unload driver successfully, driver_id 0x%x\n",
							entry->driver_id);

	kfree(entry);

exit:
	mutex_unlock(&drv_load_mutex);
	return 0;
}

int tz_unload_drv(struct TEEC_UUID *uuid)
{
	return unload_ut_drv(uuid);
}

static ssize_t unload_ut_drv_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t len)
{
	struct TEEC_UUID uuid;
	int ret;

	if (len < UUID_STRING_LENGTH) {
		IMSG_ERROR("bad UUID length, buf '%s' len %zd\n", buf, len);
		return len;
	}

	str_to_uuid(&uuid, buf);

	ret = unload_ut_drv(&uuid);
	if (ret)
		IMSG_ERROR("failed to unload ut driver, ret %d\n", ret);

	return len;
}
static DEVICE_ATTR_WO(unload_ut_drv);

static ssize_t list_ut_drv_show(struct device *cd,
				struct device_attribute *attr, char *buf)
{
	struct ut_drv_entry *entry;
	char uuid_str[UUID_STRING_LENGTH] = {0};
	char *s = buf;

	list_for_each_entry(entry, &ut_drv_list, list) {
		uuid_to_str(&entry->uuid, uuid_str);
		s += sprintf(s, "%s\n", uuid_str);
	}

	return (ssize_t)(s - buf);
}
static DEVICE_ATTR_RO(list_ut_drv);

#ifdef CONFIG_MICROTRUST_TEST_DRIVERS

#define TEST_DRIVER_ID 0x77000012

static ssize_t dcih_notify_test_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	char *s = buf;
	int ret;

	ret = get_dcih_notify_test_result();

	s += sprintf(s, "%d", ret);

	return (ssize_t)(s - buf);
}

static ssize_t dcih_notify_test_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t len)
{
	start_dcih_notify_test(TEST_DRIVER_ID);

	return len;
}
static DEVICE_ATTR_RW(dcih_notify_test);

static ssize_t dcih_wait_notify_test_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	char *s = buf;
	int ret;

	ret = get_dcih_wait_notify_test_result();

	s += sprintf(s, "%d", ret);

	return (ssize_t)(s - buf);
}

static ssize_t dcih_wait_notify_test_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t len)
{
	start_dcih_wait_notify_test(TEST_DRIVER_ID);

	return len;
}
static DEVICE_ATTR_RW(dcih_wait_notify_test);

#endif

static int notify_ree_result = -EINVAL;
static DEFINE_MUTEX(notify_ree_result_mutex);

static ssize_t notify_ree_dci_handler_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	char *s = buf;

	mutex_lock(&notify_ree_result_mutex);
	IMSG_DEBUG("notify_ree_result %d\n", notify_ree_result);
	s += sprintf(s, "%d\n", notify_ree_result);
	notify_ree_result = -EINVAL;
	mutex_unlock(&notify_ree_result_mutex);

	return (ssize_t)(s - buf);
}

static ssize_t notify_ree_dci_handler_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t len)
{
	uint32_t driver_id;

	hex_str_to_value(buf, 8, &driver_id);
	IMSG_DEBUG("driver_id: 0x%x\n", driver_id);

	mutex_lock(&notify_ree_result_mutex);
	notify_ree_result = tz_notify_ree_handler(driver_id);
	mutex_unlock(&notify_ree_result_mutex);

	return len;
}
static DEVICE_ATTR_RW(notify_ree_dci_handler);

#ifndef CONFIG_MICROTRUST_DYNAMIC_CORE
static ssize_t current_bind_cpu_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	char *s = buf;
	int cpu = get_current_cpuid();

	s += sprintf(s, "%d\n", cpu);
	return (ssize_t)(s - buf);
}
static DEVICE_ATTR_RO(current_bind_cpu);
#endif

#ifdef CONFIG_MICROTRUST_TZDRIVER_DYNAMICAL_DEBUG
static ssize_t tzdriver_dynamical_debug_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char *s = buf;

	s += sprintf(s, "%d\n", tzdriver_dynamical_debug_flag);
	return (ssize_t)(s - buf);
}

static ssize_t tzdriver_dynamical_debug_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t len)
{
	uint32_t value;

	hex_str_to_value(buf, 8, &value);

	if ((value == 0) || (value == 1))
		tzdriver_dynamical_debug_flag = value;

	return len;
}
static DEVICE_ATTR_RW(tzdriver_dynamical_debug);
#endif

static struct device_attribute *attr_list[] = {
		&dev_attr_imsg_log_level,
		&dev_attr_imsg_log_test,
		&dev_attr_load_ut_drv,
		&dev_attr_unload_ut_drv,
		&dev_attr_list_ut_drv,
#ifdef CONFIG_MICROTRUST_TEST_DRIVERS
		&dev_attr_dcih_notify_test,
		&dev_attr_dcih_wait_notify_test,
#endif
		&dev_attr_notify_ree_dci_handler,
#ifndef CONFIG_MICROTRUST_DYNAMIC_CORE
		&dev_attr_current_bind_cpu,
#endif
#ifdef CONFIG_MICROTRUST_TZDRIVER_DYNAMICAL_DEBUG
		&dev_attr_tzdriver_dynamical_debug,
#endif
		NULL
};

void remove_sysfs(struct platform_device *pdev)
{
	int i;

	if (is_context_init)
		TEEC_FinalizeContext(&ut_drv_context);

	for (i = 0; attr_list[i]; i++)
		device_remove_file(&pdev->dev, attr_list[i]);
}

int init_sysfs(struct platform_device *pdev)
{
	int res;
	int i;

	for (i = 0; attr_list[i]; i++) {
		res = device_create_file(&pdev->dev, attr_list[i]);
		if (res) {
			IMSG_ERROR("failed to create sysfs entry: %s\n",
						attr_list[i]->attr.name);
			break;
		}
	}

	if (res)
		remove_sysfs(pdev);

	return res;
}
