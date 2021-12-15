// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/types.h>
#include <linux/seq_file.h>
#include <linux/platform_device.h>

#include "apusys_power_ctl.h"
#include "hal_config_power.h"
#include "apu_log.h"
#include "apu_power_api.h"
#include "apusys_power.h"
#include "apu_platform_debug.h"

void apu_power_dump_opp_table(struct seq_file *s)
{
	int opp_num;
	int buck_domain;
	int boost_val;

	seq_printf(s,
		"|opp| vpu0|boost| vpu1|boost| vpu2|boost| dla0|boost| dla1|boost| conn|iommu|ipuif|\n");
	seq_printf(s,
		"|---------------------------------------------------------------------------------|\n");
	for (opp_num = 0 ; opp_num < APUSYS_MAX_NUM_OPPS ; opp_num++) {
		seq_printf(s, "| %d |", opp_num);
		for (buck_domain = 0 ; buck_domain < APUSYS_BUCK_DOMAIN_NUM;
			buck_domain++) {

			if (buck_domain == V_APU_CONN
				|| buck_domain == V_TOP_IOMMU
				|| buck_domain == V_VCORE) {

				seq_printf(s, " %d |",
					apusys_opps.opps[opp_num][buck_domain]
					.freq / 1000);

			} else {
				boost_val = apusys_opp_to_boost_value(
					apusys_buck_domain_to_user[buck_domain],
					opp_num);

				seq_printf(s, " %d | %03d |",
					apusys_opps.opps[opp_num][buck_domain]
					.freq / 1000, boost_val);
			}
		}
		seq_printf(s,
			"\n|---------------------------------------------------------------------------------|\n");
	}
}

int apu_power_dump_curr_status(struct seq_file *s, int oneline_str)
{
	struct apu_power_info info = {0};
	uint64_t time;
	ulong  rem_nsec;

	time = sched_clock();
	rem_nsec = do_div(time, 1000000000);


	info.id = 0;
	info.type = 1;

	hal_config_power(PWR_CMD_GET_POWER_INFO, VPU0, &info);

	// for thermal request, we print vpu and mdla freq
	if (oneline_str) {
		seq_printf(s, "%03u,%03u,%03u,%03u,%03u\n",
			((info.rpc_intf_rdy >> 2) & 0x1) ? info.dsp1_freq : 0,
			((info.rpc_intf_rdy >> 3) & 0x1) ? info.dsp2_freq : 0,
			((info.rpc_intf_rdy >> 4) & 0x1) ? info.dsp3_freq : 0,
			((info.rpc_intf_rdy >> 6) & 0x1) ? info.dsp6_freq : 0,
			((info.rpc_intf_rdy >> 7) & 0x1) ? info.dsp6_freq : 0);

		return 0;
	}

	seq_printf(s,
		   "[%5lu.%06lu]|curr| vpu0| vpu1| vpu2|mdla0|mdla1| conn|iommu|vcore|\n| opp|",
		   (ulong)time, rem_nsec / 1000);

	seq_printf(s, "  %d  |", apusys_freq_to_opp(V_VPU0,
					info.dsp1_freq * info.dump_div));
	seq_printf(s, "  %d  |", apusys_freq_to_opp(V_VPU1,
					info.dsp2_freq * info.dump_div));
	seq_printf(s, "  %d  |", apusys_freq_to_opp(V_VPU2,
					info.dsp3_freq * info.dump_div));
	seq_printf(s, "  %d  |", apusys_freq_to_opp(V_MDLA0,
					info.dsp6_freq * info.dump_div));
	seq_printf(s, "  %d  |", apusys_freq_to_opp(V_MDLA1,
					info.dsp6_freq * info.dump_div));
	seq_printf(s, "  %d  |", apusys_freq_to_opp(V_APU_CONN,
					info.dsp_freq * info.dump_div));
	seq_printf(s, "  %d  |", apusys_freq_to_opp(V_TOP_IOMMU,
					info.dsp7_freq * info.dump_div));
	seq_printf(s, "  %d  |", apusys_freq_to_opp(V_VCORE,
					info.ipuif_freq * info.dump_div));
	seq_puts(s, "\n");

	seq_printf(s,
		"|freq| %03u | %03u | %03u | %03u | %03u | %03u | %03u | %03u |\n",
		info.dsp1_freq, info.dsp2_freq, info.dsp3_freq,
		info.dsp6_freq, info.dsp6_freq, info.dsp_freq,
		info.dsp7_freq, info.ipuif_freq);

	seq_printf(s,
		"| clk| dsp1| dsp2| dsp3| dsp6| dsp6|  dsp| dsp7|ipuif|\n(unit: MHz)\n\n");

	seq_printf(s, "vvpu:%u(mV), vmdla:%u(mV), vcore:%u(mV), vsram:%u(mV)\n",
			info.vvpu, info.vmdla, info.vcore, info.vsram);

	seq_puts(s, "\n");
	seq_printf(s,
	"rpc_intf_rdy:0x%x, spm_wakeup:0x%x\nvcore_cg_con:0x%x, conn_cg_con:0x%x\nvpu0_cg_con:0x%x, vpu1_cg_con:0x%x, vpu2_cg_con:0x%x\nmdla0_cg_con:0x%x, mdla1_cg_con:0x%x\n",
		info.rpc_intf_rdy, info.spm_wakeup,
		info.vcore_cg_stat, info.conn_cg_stat,
		info.vpu0_cg_stat, info.vpu1_cg_stat, info.vpu2_cg_stat,
		info.mdla0_cg_stat, info.mdla1_cg_stat);

	seq_puts(s, "\n");
	return 0;
}

int apusys_power_fail_show(struct seq_file *s, void *unused)
{
	char log_str[128];
	int ret = 0;

	ret = snprintf(log_str, sizeof(log_str),
		"v[%u,%u,%u,%u]f[%u,%u,%u,%u,%u,%u,%u]r[%x,%x,%x,%x,%x,%x,%x,%x,%x]t[%lu.%06lu]",
		power_fail_record.pwr_info.vvpu,
		power_fail_record.pwr_info.vmdla,
		power_fail_record.pwr_info.vcore,
		power_fail_record.pwr_info.vsram,
		power_fail_record.pwr_info.dsp_freq,
		power_fail_record.pwr_info.dsp1_freq,
		power_fail_record.pwr_info.dsp2_freq,
		power_fail_record.pwr_info.dsp3_freq,
		power_fail_record.pwr_info.dsp6_freq,
		power_fail_record.pwr_info.dsp7_freq,
		power_fail_record.pwr_info.ipuif_freq,
		power_fail_record.pwr_info.spm_wakeup,
		power_fail_record.pwr_info.rpc_intf_rdy,
		power_fail_record.pwr_info.vcore_cg_stat,
		power_fail_record.pwr_info.conn_cg_stat,
		power_fail_record.pwr_info.vpu0_cg_stat,
		power_fail_record.pwr_info.vpu1_cg_stat,
		power_fail_record.pwr_info.vpu2_cg_stat,
		power_fail_record.pwr_info.mdla0_cg_stat,
		power_fail_record.pwr_info.mdla1_cg_stat,
		power_fail_record.time_sec, power_fail_record.time_nsec);

	seq_printf(s, "%s\n", log_str);
	if (ret <= 0)
		PWR_LOG_ERR("%s cannot print message\n", __func__);
	return 0;
}
