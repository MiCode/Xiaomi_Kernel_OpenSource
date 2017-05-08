/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#include <sound/q6common.h>

struct q6common_ctl {
	bool instance_id_supported;
};

static struct q6common_ctl common;

void q6common_update_instance_id_support(bool supported)
{
	common.instance_id_supported = supported;
}
EXPORT_SYMBOL(q6common_update_instance_id_support);

bool q6common_is_instance_id_supported(void)
{
	return common.instance_id_supported;
}
EXPORT_SYMBOL(q6common_is_instance_id_supported);

