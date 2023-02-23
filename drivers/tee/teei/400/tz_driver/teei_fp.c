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
#include <linux/slab.h>
#include "teei_fp.h"
#include "teei_client_transfer_data.h"
#define IMSG_TAG "[tz_driver]"
#include <imsg_log.h>
#include <linux/vmalloc.h>

static struct TEEC_Context context;
static int context_initialized;
struct TEEC_UUID uuid_fp = { 0x7778c03f, 0xc30c, 0x4dd0,
{ 0xa3, 0x19, 0xea, 0x29, 0x64, 0x3d, 0x4d, 0x4b } };
EXPORT_SYMBOL(uuid_fp);
int send_fp_command(void *buffer, unsigned long size)
{
	int ret = 0;

	IMSG_INFO("TEEI start %s\n", __func__);

	if (buffer == NULL || size < 1)
		return -1;

	if (context_initialized == 0) {
		memset(&context, 0, sizeof(context));
		ret = ut_pf_gp_initialize_context(&context);
		if (ret) {
			IMSG_ERROR("Failed to initialize fp context ,err: %x",
			ret);
			goto release_1;
		}
		context_initialized = 1;
	}
        IMSG_INFO("uuid_fp %x\n", __func__, uuid_fp.timeLow );
	ret = ut_pf_gp_transfer_user_data(&context, &uuid_fp, 1, buffer, size);
	if (ret) {
		IMSG_ERROR("Failed to transfer data,err: %x", ret);
		goto release_2;
	}
release_2:
	if (ret) {
		ut_pf_gp_finalize_context(&context);
		context_initialized = 0;
	}
release_1:
	IMSG_INFO("TEEI end of %s\n", __func__);
	return ret;
}
