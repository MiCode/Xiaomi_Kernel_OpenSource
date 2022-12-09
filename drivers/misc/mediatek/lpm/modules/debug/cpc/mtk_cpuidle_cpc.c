// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/notifier.h>

#include <lpm.h>
#include <mtk_cpuidle_sysfs.h>
#include "mtk_cpupm_dbg.h"
#include "mtk_cpuidle_cpc.h"

#define PROF_DEV_NAME_SIZE 8
void mtk_cpc_prof_lat_dump(char **ToUserBuf, size_t *size)
{
	char *p = *ToUserBuf;
	size_t sz = *size;
	int i, j;
	unsigned int enabled, dev_num;
	unsigned long dev_name_val;
	char dev_name[PROF_DEV_NAME_SIZE];
	unsigned int off_cnt = 0, off_avg_us = 0;
	unsigned int off_max_us = 0, off_min_us = 0;
	unsigned int on_cnt = 0, on_avg_us = 0;
	unsigned int on_max_us = 0, on_min_us = 0;

	enabled = (unsigned int)cpc_smc_prof(CPC_PROF_IS_ENABLED, 0);

	mtk_dbg_cpuidle_log("\n==== CPC Profile: %s ====\n",
				enabled ? "Enabled" : "Disabled");

	if (!enabled) {
		*ToUserBuf = p;
		*size = sz;
		return;
	}

	dev_num = (unsigned int)cpc_smc_prof(CPC_PROF_DEV_NUM, 0);

	for (i = 0; i < dev_num; i++) {
		dev_name_val = cpc_smc_prof(CPC_PROF_DEV_NAME, i);

		for (j = 0; j < (PROF_DEV_NAME_SIZE - 1); ++j)
			dev_name[j] = ((dev_name_val >> (j<<3)) & 0xFF);
		dev_name[j] = '\0';

		mtk_dbg_cpuidle_log("%s\n", dev_name);

		off_cnt = (unsigned int)cpc_smc_prof(CPC_PROF_OFF_CNT, i);

		if (off_cnt) {
			off_avg_us =
				(unsigned int)cpc_smc_prof(CPC_PROF_OFF_AVG, i);

			off_max_us =
				(unsigned int)cpc_smc_prof(CPC_PROF_OFF_MAX, i);

			off_min_us =
				(unsigned int)cpc_smc_prof(CPC_PROF_OFF_MIN, i);

			mtk_dbg_cpuidle_log(
				"\toff : avg = %2dus, max = %3dus, min = %3dus, cnt = %d\n",
				off_avg_us, off_max_us, off_min_us, off_cnt);
		} else {
			mtk_dbg_cpuidle_log("\toff : None\n");
		}

		on_cnt = (unsigned int)cpc_smc_prof(CPC_PROF_ON_CNT, i);

		if (on_cnt) {
			on_avg_us =
				(unsigned int)cpc_smc_prof(CPC_PROF_ON_AVG, i);

			on_max_us =
				(unsigned int)cpc_smc_prof(CPC_PROF_ON_MAX, i);

			on_min_us =
				(unsigned int)cpc_smc_prof(CPC_PROF_ON_MIN, i);

			mtk_dbg_cpuidle_log(
				"\ton : avg = %2dus, max = %3dus, min = %3dus, cnt = %d\n",
				on_avg_us, on_max_us, on_min_us, on_cnt);
		} else {
			mtk_dbg_cpuidle_log("\ton  : None\n");
		}

	}

	*ToUserBuf = p;
	*size = sz;
}

void mtk_cpc_prof_start(void)
{
	mtk_cpupm_block();

	cpc_smc_prof(CPC_PROF_ENABLE, 1);

	mtk_cpupm_allow();
}

void mtk_cpc_prof_stop(void)
{
	mtk_cpupm_block();

	cpc_smc_prof(CPC_PROF_ENABLE, 0);

	mtk_cpupm_allow();
}
