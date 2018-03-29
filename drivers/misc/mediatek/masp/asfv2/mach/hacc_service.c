/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/types.h>
#include "hacc_tee.h"


int masp_hal_get_uuid(unsigned int *uuid)
{
	uuid[0] = get_devinfo_with_index(12);
	uuid[1] = get_devinfo_with_index(13);
	uuid[2] = get_devinfo_with_index(12);
	uuid[3] = get_devinfo_with_index(13);

	return 0;
}

int masp_hal_sbc_enabled(void)
{
	return (get_devinfo_with_index(6) & 0x00000002) ? 1 : 0;
}
