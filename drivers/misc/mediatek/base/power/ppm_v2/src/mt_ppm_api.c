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


#include "mt_ppm_internal.h"

/* APIs */
void mt_ppm_set_dvfs_table(unsigned int cpu, struct cpufreq_frequency_table *tbl,
	unsigned int num, enum dvfs_table_type type)
{
	int i, j;

	FUNC_ENTER(FUNC_LV_API);

	for_each_ppm_clusters(i) {
		if (ppm_main_info.cluster_info[i].cpu_id == cpu) {
			/* return if table is existed */
			if (ppm_main_info.cluster_info[i].dvfs_tbl)
				return;

			ppm_lock(&ppm_main_info.lock);

			ppm_main_info.dvfs_tbl_type = type;
			ppm_main_info.cluster_info[i].dvfs_tbl = tbl;
			ppm_main_info.cluster_info[i].dvfs_opp_num = num;
			/* dump dvfs table */
			ppm_info("DVFS table type = %d\n", type);
			ppm_info("DVFS table of cluster %d:\n", ppm_main_info.cluster_info[i].cluster_id);
			for (j = 0; j < num; j++)
				ppm_info("%d: %d KHz\n", j, ppm_main_info.cluster_info[i].dvfs_tbl[j].frequency);

			/* data init after CPU segment is confirmed */
			if (i == ppm_main_info.cluster_num - 1)
				ppm_power_data_init();

			ppm_unlock(&ppm_main_info.lock);

			FUNC_EXIT(FUNC_LV_API);
			return;
		}
	}

	if (i == ppm_main_info.cluster_num)
		ppm_err("@%s: cpu_id(%d) not found!\n", __func__, cpu);

	FUNC_EXIT(FUNC_LV_API);
}

void mt_ppm_register_client(enum ppm_client client, void (*limit)(struct ppm_client_req req))
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

met_set_ppm_state_funcMET g_pSet_PPM_State;

void mt_set_ppm_state_registerCB(met_set_ppm_state_funcMET pCB)
{
	g_pSet_PPM_State = pCB;
}
EXPORT_SYMBOL(mt_set_ppm_state_registerCB);

void mt_ppm_set_5A_limit_throttle(bool enable)
{
	FUNC_ENTER(FUNC_LV_API);
#ifdef PPM_VPROC_5A_LIMIT_CHECK
	ppm_lock(&ppm_main_info.lock);
	if (!ppm_main_info.is_5A_limit_enable) {
		ppm_unlock(&ppm_main_info.lock);
		goto end;
	}
	ppm_main_info.is_5A_limit_on = enable;
	ppm_info("is_5A_limit_on = %d\n", ppm_main_info.is_5A_limit_on);
	ppm_unlock(&ppm_main_info.lock);

	mt_ppm_main();
end:
#endif
	FUNC_EXIT(FUNC_LV_API);
}

void mt_ppm_limit_freq_when_ke(void)
{
	struct ppm_client_req *c_req = &(ppm_main_info.client_req);
	int i;

	ppm_info("send freq limit %d KHz to DVFS when KE...\n", FREQ_LIMIT_FOR_KE);

	ppm_main_info.is_enabled = false;

	/* modify advise freq to DVFS */
	for (i = 0; i < c_req->cluster_num; i++) {
		c_req->cpu_limit[i].has_advise_freq = true;
		c_req->cpu_limit[i].advise_cpufreq_idx =
			ppm_main_freq_to_idx(i, FREQ_LIMIT_FOR_KE, CPUFREQ_RELATION_H);

		ppm_info("Result: [%d] --> (%d)(%d)(%d)(%d) (%d)(%d)(%d)(%d)\n",
			i,
			c_req->cpu_limit[i].min_cpufreq_idx,
			c_req->cpu_limit[i].max_cpufreq_idx,
			c_req->cpu_limit[i].min_cpu_core,
			c_req->cpu_limit[i].max_cpu_core,
			c_req->cpu_limit[i].has_advise_freq,
			c_req->cpu_limit[i].advise_cpufreq_idx,
			c_req->cpu_limit[i].has_advise_core,
			c_req->cpu_limit[i].advise_cpu_core
		);
	}

	/* send request to DVFS only */
	ppm_main_info.client_info[PPM_CLIENT_DVFS].limit_cb(*c_req);

	ppm_info("send limit done!\n");
}

