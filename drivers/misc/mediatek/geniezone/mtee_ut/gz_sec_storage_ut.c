// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 * GenieZone (hypervisor-based seucrity platform) enables hardware protected
 * and isolated security execution environment, includes
 * 1. GZ hypervisor
 * 2. Hypervisor-TEE OS (built-in Trusty OS)
 * 3. Drivers (ex: debug, communication and interrupt) for GZ and
 *    hypervisor-TEE OS
 * 4. GZ and hypervisor-TEE and GZ framework (supporting multiple TEE
 *    ecosystem, ex: M-TEE, Trusty, GlobalPlatform, ...)
 */


#include <gz-trusty/trusty_ipc.h>
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
