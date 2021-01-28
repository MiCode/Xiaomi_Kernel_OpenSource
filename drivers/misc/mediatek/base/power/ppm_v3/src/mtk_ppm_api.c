/*
 * Copyright (C) 2016 MediaTek Inc.
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


#include "mtk_ppm_internal.h"

/* APIs */
void mt_ppm_set_dvfs_table(unsigned int cpu,
	struct cpufreq_frequency_table *tbl,
	unsigned int num, enum dvfs_table_type type)
{
	struct ppm_data *p = &ppm_main_info;
	int i, j;

	FUNC_ENTER(FUNC_LV_API);

	for_each_ppm_clusters(i) {
		if (p->cluster_info[i].cpu_id != cpu)
			continue;

		/* return if table is existed */
		if (p->cluster_info[i].dvfs_tbl)
			return;

		ppm_lock(&(p->lock));

		p->dvfs_tbl_type = type;
		p->cluster_info[i].dvfs_tbl = tbl;
		p->cluster_info[i].dvfs_opp_num = num;
		/* dump dvfs table */
		ppm_info("DVFS table type = %d\n", type);
		ppm_info("DVFS table of cluster %d:\n",
			p->cluster_info[i].cluster_id);
		for (j = 0; j < num; j++) {
			ppm_info("%d: %d KHz\n",
				j, p->cluster_info[i].dvfs_tbl[j].frequency);
		}

		ppm_unlock(&(p->lock));

		FUNC_EXIT(FUNC_LV_API);
		return;
	}

	if (i == p->cluster_num)
		ppm_err("@%s: cpu_id(%d) not found!\n", __func__, cpu);

	FUNC_EXIT(FUNC_LV_API);
}

void mt_ppm_register_client(enum ppm_client client,
	void (*limit)(struct ppm_client_req req))
{
	FUNC_ENTER(FUNC_LV_API);

	if (client < 0) {
		ppm_err("invalid client value: %d\n", client);
		return;
	}

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

