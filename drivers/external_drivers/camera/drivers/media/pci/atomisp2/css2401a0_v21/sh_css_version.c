/*
 * Support for Intel Camera Imaging ISP subsystem.
 *
 * Copyright (c) 2010 - 2015 Intel Corporation. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#include "ia_css_version.h"
#include "ia_css_version_data.h"
#include <assert_support.h>
#include <string.h>
#include "ia_css_err.h"
#include "sh_css_firmware.h"

enum ia_css_err
ia_css_get_version(char *version, int max_size)
{
	if (max_size <= (int)strlen(CSS_VERSION_STRING) + (int)strlen(sh_css_get_fw_version()) + 5)
		return IA_CSS_ERR_INVALID_ARGUMENTS;
	assert(version != NULL);
	strcpy(version, CSS_VERSION_STRING);
	strcat(version, "FW:");
	strcat(version, sh_css_get_fw_version());
	strcat(version, "; ");
	return IA_CSS_SUCCESS;
}
