/*
 * Copyright (C) 2018 MediaTek Inc.
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

#include <linux/trusty/trusty_ipc.h>
#include "gz_sec_storage_ut.h"

int test_SecureStorageBasic(void *args)
{
	int rc;

	struct tipc_k_handle h;

	char cmd[] = "RUN";

	rc = tipc_k_connect(&h, SS_UT_SRV_NAME);
	if (rc) {
		pr_debug("failed to connect to %s\n", SS_UT_SRV_NAME);
		return rc;
	}

	rc = tipc_k_write(&h, cmd, sizeof(cmd), O_RDWR);
	if (rc != sizeof(cmd)) {
		pr_debug("failed to write TIPC message, rc=%d, expected=%lu\n",
			 rc, sizeof(cmd));
		tipc_k_disconnect(&h);
		return rc;
	}

	tipc_k_disconnect(&h);
	return 0;
}
