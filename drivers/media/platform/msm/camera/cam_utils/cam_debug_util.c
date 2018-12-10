// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2018, The Linux Foundataion. All rights reserved.
 */

#include <linux/io.h>
#include <linux/module.h>

#include "cam_debug_util.h"

static uint debug_mdl;
module_param(debug_mdl, uint, 0644);

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
		pr_info("CAM_DBG: %s: %s: %d: %s\n",
			cam_get_module_name(module_id),
			func, line, str_buffer);
		va_end(args);
	}
}
