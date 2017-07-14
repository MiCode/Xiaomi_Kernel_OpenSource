/* Copyright (c) 2017, The Linux Foundataion. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/io.h>
#include "cam_debug_util.h"

static const char *cam_debug_module_id_to_name(unsigned int module_id)
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
		name = "CAM_CRM";
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
	default:
		name = "CAM";
		break;
	}

	return name;
}

void cam_debug_log(unsigned int module_id, enum cam_debug_level dbg_level,
	const char *func, const int line, const char *fmt, ...)
{
	char str_buffer[STR_BUFFER_MAX_LENGTH];
	const char *module_name;
	va_list args;

	va_start(args, fmt);
	vsnprintf(str_buffer, STR_BUFFER_MAX_LENGTH, fmt, args);
	va_end(args);

	module_name = cam_debug_module_id_to_name(module_id);

	switch (dbg_level) {
	case CAM_LEVEL_INFO:
		pr_info("CAM_INFO: %s: %s: %d: %s\n",
			module_name, func, line, str_buffer);
		break;
	case CAM_LEVEL_WARN:
		pr_warn("CAM_WARN: %s: %s: %d: %s\n",
			module_name, func, line, str_buffer);
		break;
	case CAM_LEVEL_ERR:
		pr_err("CAM_ERR: %s: %s: %d: %s\n",
			module_name, func, line, str_buffer);
		break;
	case CAM_LEVEL_DBG:
		pr_info("CAM_DBG: %s: %s: %d: %s\n",
			module_name, func, line, str_buffer);
		break;
	default:
		break;
	}
}
