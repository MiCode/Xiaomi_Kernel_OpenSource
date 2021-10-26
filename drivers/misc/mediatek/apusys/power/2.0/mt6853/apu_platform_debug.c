/*
 * Copyright (C) 2019 MediaTek Inc.
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

#include <linux/types.h>
#include <linux/seq_file.h>
#include <linux/platform_device.h>
#include <linux/vmalloc.h>

#include "apusys_power_ctl.h"
#include "hal_config_power.h"
#include "apu_log.h"
#include "apu_power_api.h"
#include "apusys_power.h"
#include "apu_platform_debug.h"
#ifdef APUPWR_TAG_TP
#include "apu_power_tag.h"
#endif

/* length of buffer to save freq/volt of each buck_domain */
#define INFO_LENGTH		15

/**
 * add_separte() - Add separater when showing freq/volt info
 * @s: the seq_file
 *
 * The out put will be "-----------"
 */
static  inline void add_separte(struct seq_file *s, char *separate)
{
	seq_puts(s, "\n");
	seq_printf(s, separate);
	seq_puts(s, "\n");
}

void apu_power_dump_opp_table(struct seq_file *s)
{
	int opp_num;
	int bd;
	int line_size = 0;
	char info[INFO_LENGTH];
	char *separate = NULL;

	memset(info, 0, sizeof(info));

	/* print header and calcuate line size */
	line_size += snprintf(info, INFO_LENGTH, "|opp|");
	seq_printf(s, info);
	for (bd = 0 ; bd < APUSYS_BUCK_DOMAIN_NUM; bd++) {
		line_size += snprintf(info, INFO_LENGTH,
			 "%13s|", buck_domain_str[bd]);
		seq_printf(s, info);
		memset(info, 0, sizeof(info));
	}

	/* add separator line */
	separate = vzalloc(line_size);
	memset(separate, '-', line_size - 1);
	add_separte(s, separate);

	/* show opp info, including freq and voltage */
	for (opp_num = 0 ; opp_num < APUSYS_MAX_NUM_OPPS ; opp_num++) {
		seq_printf(s, "|%3d|", opp_num);
		for (bd = 0 ; bd < APUSYS_BUCK_DOMAIN_NUM; bd++) {
			memset(info, 0, sizeof(info));
			snprintf(info, INFO_LENGTH, "%3dMhz(%3dmv)|",
				apusys_opps.opps[opp_num][bd].freq / 1000,
				apusys_opps.opps[opp_num][bd].voltage / 1000);
				seq_printf(s, info);
		}
		add_separte(s, separate);
	}

	/* release separator line array */
	vfree(separate);
}

int apu_power_dump_curr_status(struct seq_file *s, int oneline_str)
{
	struct apu_power_info info = {0};

	info.id = 0;
	info.type = 1;

	hal_config_power(PWR_CMD_GET_POWER_INFO, VPU0, &info);

	// for thermal request, we print vpu and mdla freq
	if (oneline_str) {
		seq_printf(s, "%03u,%03u\n",
			((info.rpc_intf_rdy >> 2) & 0x1) ? info.dsp1_freq : 0,
			((info.rpc_intf_rdy >> 3) & 0x1) ? info.dsp2_freq : 0);

		return 0;
	}

	seq_printf(s,
		"|curr| vpu0| vpu1|conn|vcore|\n");
	seq_printf(s,
		"|freq| %03u | %03u| %03u | %03u |\n",
		info.dsp1_freq, info.dsp2_freq,
		info.dsp_freq,
		info.ipuif_freq);

	seq_printf(s,
		"| clk| dsp1| dsp2| dsp| ipuif|\n(unit: MHz)\n\n");

	seq_printf(s, "vvpu:%u(mV), vcore:%u(mV), vsram:%u(mV)\n",
		   info.vvpu, info.vcore, info.vsram);

	seq_puts(s, "\n");
	seq_printf(s,
	"rpc_intf_rdy:0x%x, spm_wakeup:0x%x\nvcore_cg_con:0x%x, conn_cg_con:0x%x\nvpu0_cg_con:0x%x, vpu1_cg_con:0x%x\n",
		info.rpc_intf_rdy, info.spm_wakeup,
		info.vcore_cg_stat, info.conn_cg_stat,
		info.vpu0_cg_stat, info.vpu1_cg_stat);

	seq_puts(s, "\n");
	return 0;
}

int apusys_power_fail_show(struct seq_file *s, void *unused)
{
	char log_str[128];

	snprintf(log_str, sizeof(log_str),
		"v[%u,%u,%u]f[%u,%u,%u,%u]r[%x,%x,%x,%x,%x,%x]t[%lu.%06lu]",
		power_fail_record.pwr_info.vvpu,
		power_fail_record.pwr_info.vcore,
		power_fail_record.pwr_info.vsram,
		power_fail_record.pwr_info.dsp_freq,
		power_fail_record.pwr_info.dsp1_freq,
		power_fail_record.pwr_info.dsp2_freq,
		power_fail_record.pwr_info.ipuif_freq,
		power_fail_record.pwr_info.spm_wakeup,
		power_fail_record.pwr_info.rpc_intf_rdy,
		power_fail_record.pwr_info.vcore_cg_stat,
		power_fail_record.pwr_info.conn_cg_stat,
		power_fail_record.pwr_info.vpu0_cg_stat,
		power_fail_record.pwr_info.vpu1_cg_stat,
		power_fail_record.time_sec, power_fail_record.time_nsec);

	seq_printf(s, "%s\n", log_str);

#ifdef APUPWR_TAG_TP
	seq_puts(s, "\n");
	seq_puts(s, "======== Tags ========\n");
	apupwr_tags_show(s);
#endif

	return 0;
}
