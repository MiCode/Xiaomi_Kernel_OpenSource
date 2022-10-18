// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundataion. All rights reserved.
 */

#include <linux/io.h>
#include <linux/slab.h>
#include <linux/module.h>
#include "cam_trace.h"

#include "cam_debug_util.h"
#include <linux/gpio.h>
#include "cam_res_mgr_api.h"
#include <cam_soc_util.h>

unsigned long long debug_mdl;
module_param(debug_mdl, ullong, 0644);

/* 0x0 - only logs, 0x1 - only trace, 0x2 - logs + trace */
uint debug_type;
module_param(debug_type, uint, 0644);

uint debug_priority;
module_param(debug_priority, uint, 0644);

struct camera_debug_settings cam_debug;

/* xiaomi add hw trigger - begin */
static int cam_hw_trigger_cnt  = 5;
// cam_hw_trigger[5] = {gpio, value, module_id, gpio_module, delay}
static uint cam_hw_trigger[5] = {0, 0, 0, 0, 1000};
module_param_array(cam_hw_trigger, uint, &cam_hw_trigger_cnt, 0644);

int cam_debug_hw_trigger(unsigned int module_id, bool status)
{
	int rc = 0;
	int restore_value = 0;

	if (!status ||	0 == cam_hw_trigger[0] ||
			0 == cam_hw_trigger[4] ||
			!(cam_hw_trigger[2] & module_id))
		return rc;

	rc = gpio_request_one(cam_hw_trigger[0], cam_hw_trigger[3], "CAM_HW_TRIGGER");
	if (rc) {
		CAM_ERR(CAM_UTIL, "[cam_hw_trigger] GPIO %d:%s request failed, rc = %d",
			cam_hw_trigger[0], "CAM_HW_TRIGGER", rc);
		return rc;
	}

	restore_value = gpio_get_value_cansleep(cam_hw_trigger[0]);
	if (restore_value != 0 && restore_value != 1) {
		CAM_ERR(CAM_UTIL, "[cam_hw_trigger] Failed to get GPIO %d:%s status",
			cam_hw_trigger[0], "CAM_HW_TRIGGER");
		rc = -EIO;
		goto end_free;
	}

	if (restore_value == cam_hw_trigger[1]) {
		CAM_ERR(CAM_UTIL, "[cam_hw_trigger] GPIO %d:%s is already in state %d",
			cam_hw_trigger[0], "CAM_HW_TRIGGER", cam_hw_trigger[1]);
		goto end_free;
	}

	gpio_set_value_cansleep(cam_hw_trigger[0], cam_hw_trigger[1]);
	CAM_DBG(CAM_UTIL, "[cam_hw_trigger] %s success, GPIO:%d value:%d",
	CAM_DBG_MOD_NAME(module_id), cam_hw_trigger[0],
		cam_hw_trigger[1]);

	usleep_range(cam_hw_trigger[4], (cam_hw_trigger[4] + 100));

	gpio_set_value_cansleep(cam_hw_trigger[0], restore_value);
	CAM_DBG(CAM_UTIL, "[cam_hw_trigger] %s restore, GPIO:%d value:%d",
	CAM_DBG_MOD_NAME(module_id), cam_hw_trigger[0], restore_value);

end_free:
	gpio_free(cam_hw_trigger[0]);
	return rc;
}
/* xiaomi add hw trigger - end */

const struct camera_debug_settings *cam_debug_get_settings()
{
	return &cam_debug;
}

static int cam_debug_parse_cpas_settings(const char *setting, u64 value)
{
	if (!strcmp(setting, "camnoc_bw")) {
		cam_debug.cpas_settings.camnoc_bw = value;
	} else if (!strcmp(setting, "mnoc_hf_0_ab_bw")) {
		cam_debug.cpas_settings.mnoc_hf_0_ab_bw = value;
	} else if (!strcmp(setting, "mnoc_hf_0_ib_bw")) {
		cam_debug.cpas_settings.mnoc_hf_0_ib_bw = value;
	} else if (!strcmp(setting, "mnoc_hf_1_ab_bw")) {
		cam_debug.cpas_settings.mnoc_hf_1_ab_bw = value;
	} else if (!strcmp(setting, "mnoc_hf_1_ib_bw")) {
		cam_debug.cpas_settings.mnoc_hf_1_ib_bw = value;
	} else if (!strcmp(setting, "mnoc_sf_0_ab_bw")) {
		cam_debug.cpas_settings.mnoc_sf_0_ab_bw = value;
	} else if (!strcmp(setting, "mnoc_sf_0_ib_bw")) {
		cam_debug.cpas_settings.mnoc_sf_0_ib_bw = value;
	} else if (!strcmp(setting, "mnoc_sf_1_ab_bw")) {
		cam_debug.cpas_settings.mnoc_sf_1_ab_bw = value;
	} else if (!strcmp(setting, "mnoc_sf_1_ib_bw")) {
		cam_debug.cpas_settings.mnoc_sf_1_ib_bw = value;
	} else if (!strcmp(setting, "mnoc_sf_icp_ab_bw")) {
		cam_debug.cpas_settings.mnoc_sf_icp_ab_bw = value;
	} else if (!strcmp(setting, "mnoc_sf_icp_ib_bw")) {
		cam_debug.cpas_settings.mnoc_sf_icp_ib_bw = value;
	} else {
		CAM_ERR(CAM_UTIL, "Unsupported cpas sysfs entry");
		return -EINVAL;
	}

	return 0;
}

ssize_t cam_debug_sysfs_node_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int rc = 0;
	char *local_buf = NULL, *local_buf_temp = NULL;
	char *driver;
	char *setting = NULL;
	char *value_str = NULL;
	u64 value;

	CAM_INFO(CAM_UTIL, "Sysfs debug attr name:[%s] buf:[%s] bytes:[%d]",
		attr->attr.name, buf, count);
	local_buf = kmemdup(buf, (count + sizeof(char)), GFP_KERNEL);
	local_buf_temp = local_buf;
	driver = strsep(&local_buf, "#");
	if (!driver) {
		CAM_ERR(CAM_UTIL,
			"Invalid input driver name buf:[%s], count:%d",
			buf, count);
		goto error;
	}

	setting = strsep(&local_buf, "=");
	if (!setting) {
		CAM_ERR(CAM_UTIL, "Invalid input setting buf:[%s], count:%d",
			buf, count);
		goto error;
	}

	value_str = strsep(&local_buf, "=");
	if (!value_str) {
		CAM_ERR(CAM_UTIL, "Invalid input value buf:[%s], count:%d",
			buf, count);
		goto error;
	}

	rc = kstrtou64(value_str, 0, &value);
	if (rc < 0) {
		CAM_ERR(CAM_UTIL, "Error converting value:[%s], buf:[%s]",
			value_str, buf);
		goto error;
	}

	CAM_INFO(CAM_UTIL,
		"Processing sysfs store for driver:[%s], setting:[%s], value:[%llu]",
		driver, setting, value);

	if (!strcmp(driver, "cpas")) {
		rc = cam_debug_parse_cpas_settings(setting, value);
		if (rc)
			goto error;
	} else {
		CAM_ERR(CAM_UTIL, "Unsupported driver in camera debug node");
		goto error;
	}

	kfree(local_buf_temp);
	return count;

error:
	kfree(local_buf_temp);
	return -EPERM;
}

static inline void __cam_print_to_buffer(char *buf, const size_t buf_size, size_t *len,
	unsigned int tag, enum cam_debug_module_id module_id, const char *fmt, va_list args)
{
	size_t buf_len = *len;

	buf_len += scnprintf(buf + buf_len, (buf_size - buf_len), "\n%-8s: %s:\t",
			CAM_LOG_TAG_NAME(tag), CAM_DBG_MOD_NAME(module_id));
	buf_len += vscnprintf(buf + buf_len, (buf_size - buf_len), fmt, args);
	*len = buf_len;
}

void cam_print_to_buffer(char *buf, const size_t buf_size, size_t *len, unsigned int tag,
	unsigned long long module_id, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	__cam_print_to_buffer(buf, buf_size, len, tag, module_id, fmt, args);
	va_end(args);
}

static void __cam_print_log(int type, const char *fmt, va_list args)
{
	va_list args1, args2;

	va_copy(args1, args);
	va_copy(args2, args1);
	if ((type & CAM_PRINT_LOG) && (debug_type != 1))
		vprintk(fmt, args1);
	if ((type & CAM_PRINT_TRACE) && (debug_type != 0)) {
		/* skip the first character which is used by printk to identify the log level */
		trace_cam_log_debug(fmt + sizeof(KERN_INFO) - 1, &args2);
	}
	va_end(args2);
	va_end(args1);
}

void cam_print_log(int type, const char *fmt, ...)
{
	va_list args;

	if (!type)
		return;

	va_start(args, fmt);
	__cam_print_log(type, fmt, args);
	va_end(args);
}
