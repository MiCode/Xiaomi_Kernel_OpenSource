// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/cpufreq.h>
#include "mtk_ppm_internal.h"

/* APIs */

void mt_ppm_register_client(enum ppm_client client,
	void (*limit)(struct ppm_client_req req))
{
	FUNC_ENTER(FUNC_LV_API);

	ppm_lock(&ppm_main_info.lock);

	/* init client */
	switch (client) {
	case PPM_CLIENT_DVFS:
		ppm_main_info.client_info[client].name = "DVFS";
		break;
	case PPM_CLIENT_HOTPLUG:
		ppm_main_info.client_info[client].name = "HOTPLUG";
		break;
	default:
		ppm_main_info.client_info[client].name = "UNKNOWN";
		break;
	}
	ppm_main_info.client_info[client].client = client;
	ppm_main_info.client_info[client].limit_cb = limit;

	ppm_unlock(&ppm_main_info.lock);

	FUNC_EXIT(FUNC_LV_API);
}

