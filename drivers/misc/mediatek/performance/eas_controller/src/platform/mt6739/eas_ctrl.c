/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#define TAG "[Boost Controller]"

#include <asm/div64.h>
#include <linux/kernel.h>

#include <mach/mtk_cpufreq_api.h>
#include <mach/mtk_ppm_api.h>
#include <fpsgo_common.h>

/*
 * C/M balance
 * CPU freq	: 1235000 KHz
 */
static unsigned int cpu_freq_thres;

static inline int get_cluster_id(int bv, int *offset)
{
	int cid = -1;
	int model = 0;
	int v = bv;

	while (v >= 1000) {
		v -= 1000;
		model++;
	}

	switch (model) {
	/* linear boost model */
	case 3:
		/*
		 * though linear boost starts from 3100, it should not
		 * appear 3100 here due to recovery incremental, and
		 * expect the value starts from 3101.
		 */
		while (v > 100) {
			v -= 100;
			cid++;
		}
		break;

	case 0:
	case 1:
	case 2:
		cid = 0;
		break;

	default:
		break;
	}

	*offset = min(v, 100);
	return cid;
}

void update_pwd_tbl(void)
{
	long long max_freq;

	max_freq = mt_cpufreq_get_freq_by_idx(0, 0);
	if (max_freq <= 0) {
		cpu_freq_thres = 101;
		pr_debug(TAG "max_freq:%lld cpu_freq_thres:%u\n", max_freq,
			     cpu_freq_thres);
		return;
	}

	/*
	 * unit in KHz, and multiply 100 before dividing by @max_freq.
	 * then, the quotient will be ranging from 0 to 100, which can
	 * be a threshold to directly compare with boost value.
	 */
	cpu_freq_thres = 123500000U;
	do_div(cpu_freq_thres, (unsigned int)max_freq);

	pr_debug(TAG "max_freq:%lld cpu_freq_thres:%u\n", max_freq,
		     cpu_freq_thres);
}

int reduce_stall(int boost_value, int cpi_thres, int unlimit)
{
	unsigned int cpi;
	int cid;
	int offset;
	int vcore_opp = -1;

	cid = get_cluster_id(boost_value, &offset);
	if (cid < 0)
		goto def_ret;

	/* single cluster for mt6739 */
	if (cid != 0)
		goto def_ret;

	cpi = ppm_get_cluster_cpi(cid);
	fpsgo_systrace_c_fbt_gm(-400, cpi, "cpi");
	if (cpi < cpi_thres || offset < cpu_freq_thres)
		goto def_ret;

	/* mt6739 only has two gear */
	vcore_opp = 0;

	/* if @unlimit is not set, forbid highest oppidx 0 */
	if (!unlimit)
		vcore_opp = max(1, vcore_opp);

def_ret:
	return vcore_opp;
}
