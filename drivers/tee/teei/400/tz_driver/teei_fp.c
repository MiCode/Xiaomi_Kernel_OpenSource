// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
 * All Rights Reserved.
 *
 */
#include <linux/slab.h>
#include "teei_fp.h"
#include "teei_client_transfer_data.h"
#define IMSG_TAG "[tz_driver]"
#include <imsg_log.h>
#include <linux/vmalloc.h>

static struct TEEC_Context context;
static int context_initialized;
struct TEEC_UUID uuid_fp = { 0x511ead0a, 0x0000, 0x0000,
{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } };
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
