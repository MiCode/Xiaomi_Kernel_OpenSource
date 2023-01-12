// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2020, The Linux Foundataion. All rights reserved.
 */

#include <linux/io.h>
#include <linux/slab.h>
#include <linux/module.h>
#include "cam_trace.h"

#include "cam_debug_util.h"

static uint debug_mdl;
module_param(debug_mdl, uint, 0644);

/* 0x0 - only logs, 0x1 - only trace, 0x2 - logs + trace */
static uint debug_type;
module_param(debug_type, uint, 0644);

struct camera_debug_settings cam_debug;

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

const char *cam_get_module_name(unsigned int module_id)
{
	const char *name = NULL;

	switch (module_id) {
	case CAM_CDM:
		name = "CAM-CDM";
		break;
	case CAM_CORE:
		name = "CAM-CORE";
		break;
	case CAM_CRM:
		name = "CAM-CRM";
		break;
	case CAM_CPAS:
		name = "CAM-CPAS";
		break;
	case CAM_ISP:
		name = "CAM-ISP";
		break;
	case CAM_SENSOR:
		name = "CAM-SENSOR";
		break;
	case CAM_SMMU:
		name = "CAM-SMMU";
		break;
	case CAM_SYNC:
		name = "CAM-SYNC";
		break;
	case CAM_ICP:
		name = "CAM-ICP";
		break;
	case CAM_JPEG:
		name = "CAM-JPEG";
		break;
	case CAM_FD:
		name = "CAM-FD";
		break;
	case CAM_LRME:
		name = "CAM-LRME";
		break;
	case CAM_FLASH:
		name = "CAM-FLASH";
		break;
	case CAM_ACTUATOR:
		name = "CAM-ACTUATOR";
		break;
	case CAM_CCI:
		name = "CAM-CCI";
		break;
	case CAM_CSIPHY:
		name = "CAM-CSIPHY";
		break;
	case CAM_EEPROM:
		name = "CAM-EEPROM";
		break;
	case CAM_UTIL:
		name = "CAM-UTIL";
		break;
	case CAM_CTXT:
		name = "CAM-CTXT";
		break;
	case CAM_HFI:
		name = "CAM-HFI";
		break;
	case CAM_OIS:
		name = "CAM-OIS";
		break;
	case CAM_IRQ_CTRL:
		name = "CAM-IRQ-CTRL";
		break;
	case CAM_MEM:
		name = "CAM-MEM";
		break;
	case CAM_PERF:
		name = "CAM-PERF";
		break;
	case CAM_REQ:
		name = "CAM-REQ";
		break;
	case CAM_CUSTOM:
		name = "CAM-CUSTOM";
		break;
	case CAM_OPE:
		name = "CAM-OPE";
		break;
	case CAM_PRESIL:
		name = "CAM-PRESIL";
		break;
	case CAM_RES:
		name = "CAM-RES";
		break;
	case CAM_IO_ACCESS:
		name = "CAM-IO-ACCESS";
		break;
	case CAM_SFE:
		name = "CAM-SFE";
		break;
	default:
		name = "CAM";
		break;
	}

	return name;
}

const char *cam_get_tag_name(unsigned int tag_id)
{
	const char *name = NULL;

	switch (tag_id) {
	case CAM_TYPE_TRACE:
		name = "CAM_TRACE";
		break;
	case CAM_TYPE_ERR:
		name = "CAM_ERR";
		break;
	case CAM_TYPE_WARN:
		name = "CAM_WARN";
		break;
	case CAM_TYPE_INFO:
		name = "CAM_INFO";
		break;
	case CAM_TYPE_DBG:
		name = "CAM_DBG";
		break;
	default:
		name = "CAM";
		break;
	}

	return name;
}

void cam_debug_log(unsigned int module_id, const char *func, const int line,
	const char *fmt, ...)
{
	char str_buffer[STR_BUFFER_MAX_LENGTH];
	va_list args;

	va_start(args, fmt);

	if (debug_mdl & module_id) {
		vsnprintf(str_buffer, STR_BUFFER_MAX_LENGTH, fmt, args);

		if ((debug_type == 0) || (debug_type == 2)) {
			pr_info("CAM_DBG: %s: %s: %d: %s\n",
				cam_get_module_name(module_id),
				func, line, str_buffer);
		}

		if ((debug_type == 1) || (debug_type == 2)) {
			char trace_buffer[STR_BUFFER_MAX_LENGTH];

			snprintf(trace_buffer, sizeof(trace_buffer),
				"%s: %s: %s: %d: %s",
				cam_get_tag_name(CAM_TYPE_DBG),
				cam_get_module_name(module_id),
				func, line, str_buffer);
			trace_cam_log_debug(trace_buffer);
		}
	}

	va_end(args);
}

void cam_debug_trace(unsigned int tag, unsigned int module_id,
	const char *func, const int line, const char *fmt, ...)
{
	char str_buffer[STR_BUFFER_MAX_LENGTH];
	va_list args;

	if ((tag == CAM_TYPE_TRACE) || (debug_type == 1) || (debug_type == 2)) {
		char trace_buffer[STR_BUFFER_MAX_LENGTH];

		va_start(args, fmt);

		vsnprintf(str_buffer, STR_BUFFER_MAX_LENGTH, fmt, args);

		snprintf(trace_buffer, sizeof(trace_buffer),
			"%s: %s: %s: %d: %s",
			cam_get_tag_name(tag), cam_get_module_name(module_id),
			func, line, str_buffer);
		trace_cam_log_debug(trace_buffer);

		va_end(args);
	}
}
