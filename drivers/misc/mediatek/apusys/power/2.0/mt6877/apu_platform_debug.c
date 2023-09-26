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
#include "apusys_power_reg.h"
#include "apu_platform_debug.h"
#ifdef APUPWR_TAG_TP
#include "apu_power_tag.h"
#endif

/* length of buffer to save freq/volt of each buck_domain */
#define INFO_LENGTH		18

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
	int ret = 0;

	memset(info, 0, sizeof(info));

	/* print header and calcuate line size */
	line_size += snprintf(info, INFO_LENGTH, "|opp|");
	seq_printf(s, info);
	for (bd = 0 ; bd < APUSYS_BUCK_DOMAIN_NUM; bd++) {
		line_size += snprintf(info, INFO_LENGTH,
			 "%14s|", buck_domain_str[bd]);
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
			ret = snprintf(info, INFO_LENGTH, "%3dMhz(%6d)|",
				apusys_opps.opps[opp_num][bd].freq / 1000,
				apusys_opps.opps[opp_num][bd].voltage);
				seq_printf(s, info);
			if (ret)
				goto out;
		}
		add_separte(s, separate);
	}
out:
	/* release separator line array */
	vfree(separate);
}

void _apu_power_dump_acc_status(struct seq_file *s,
				struct apu_power_info *info, int index)
{
	seq_printf(s,
		"| %3u | %3u | %3u | %3u | %3u | %3u | %3u | %3u | %3u | %3u | %3u | %3u |\n",
		index,
		(info->acc_status[index] & BIT(BIT_INVEN_OUT))        ? 1 : 0,
		(info->acc_status[index] & BIT(BIT_SEL_PARK_SRC_OUT)) ? 1 : 0,
		(info->acc_status[index] & BIT(BIT_SEL_APU))          ? 1 : 0,
		(info->acc_status[index] & BIT(BIT_SEL_APU_DIV2))     ? 1 : 0,
		(info->acc_status[index] & BIT(BIT_SEL_F26M))         ? 1 : 0,
		(info->acc_status[index] & BIT(BIT_SEL_PARK))         ? 1 : 0,
		(info->acc_status[index] & BIT(BIT_CGEN_OUT))         ? 1 : 0,
		(info->acc_status[index] & BIT(BIT_CGEN_APU))         ? 1 : 0,
		(info->acc_status[index] & BIT(BIT_CGEN_SOC))         ? 1 : 0,
		(info->acc_status[index] & BIT(BIT_CGEN_PARK))        ? 1 : 0,
		(info->acc_status[index] & BIT(BIT_CGEN_F26M))        ? 1 : 0);
}

int apu_power_dump_curr_status(struct seq_file *s, int oneline_str)
{
	struct apu_power_info info = {0};

	info.id = 0;
	info.type = 1;

	hal_config_power(PWR_CMD_GET_POWER_INFO, VPU0, &info);

	// for thermal request, we print vpu and mdla freq
	if (oneline_str) {
		seq_printf(s, "%03u,%03u,%03u\n",
			((info.rpc_intf_rdy >> 2) & 0x1) ? info.vpu0_freq : 0,
			((info.rpc_intf_rdy >> 3) & 0x1) ? info.vpu1_freq : 0,
			((info.rpc_intf_rdy >> 6) & 0x1) ? info.mdla0_freq : 0);

		return 0;
	}

	seq_printf(s,
		"|curr|  vpu0 |  vpu1 | mdla0 |  conn | iommu |\n");

	seq_printf(s,
		"| opp|   %u   |   %u   |   %u   |   %u   |   %u   |\n",
		apusys_opps.cur_opp_index[V_VPU0],
		apusys_opps.cur_opp_index[V_VPU1],
		apusys_opps.cur_opp_index[V_MDLA0],
		apusys_opps.cur_opp_index[V_APU_CONN],
		apusys_opps.cur_opp_index[V_TOP_IOMMU]);

	seq_printf(s,
		"|freq|  %03u  |  %03u  |  %03u  |  %03u  |  %03u  |\n",
		info.vpu0_freq, info.vpu1_freq,
		info.mdla0_freq, info.conn_freq,
		info.iommu_freq);

	seq_printf(s,
		"| clk| npupll| npupll| apupll|apupll1|apupll2|\n(unit: MHz)\n\n");

	seq_printf(s,
		"npupll:%u(MHz), apupll:%u(MHz), apupll1:%u(MHz), apupll2:%u(MHz)\n\n",
		info.npupll_freq, info.apupll_freq,
		info.apupll1_freq, info.apupll2_freq);

	seq_printf(s,
		"| acc | INV | SEL | SEL | SEL | SEL | SEL | CGEN| CGEN| CGEN| CGEN| CGEN|\n"
		"|     | OUT | PARK| APU | APU | F26M| PARK| OUT | APU | SOC | PARK| F26M|\n"
		"|     |     | OUT |     | DIV2|     |     |     |     |     |     |     |\n"
		"|-----------------------------------------------------------------------|\n");
	_apu_power_dump_acc_status(s, &info, 0);
	_apu_power_dump_acc_status(s, &info, 1);
	_apu_power_dump_acc_status(s, &info, 2);
	_apu_power_dump_acc_status(s, &info, 4);
	_apu_power_dump_acc_status(s, &info, 7);

	seq_printf(s, "vvpu:%u(mV), vcore:%u(mV), vsram:%u(mV)\n",
			info.vvpu, info.vcore, info.vsram);

	seq_puts(s, "\n");
	seq_printf(s,
	"rpc_intf_rdy:0x%x, spm_wakeup:0x%x\nvcore_cg_con:0x%x, conn_cg_con:0x%x, conn1_cg_con:0x%x\nvpu0_cg_con:0x%x, vpu1_cg_con:0x%x, mdla0_cg_con:0x%x\n",
		info.rpc_intf_rdy, info.spm_wakeup,
		info.vcore_cg_stat, info.conn_cg_stat,
		info.conn1_cg_stat, info.vpu0_cg_stat,
		info.vpu1_cg_stat, info.mdla0_cg_stat);

	seq_puts(s, "\n");
	return 0;
}

int apusys_power_fail_show(struct seq_file *s, void *unused)
{
	char log_str[128];
	int ret = 0;

	snprintf(log_str, sizeof(log_str),
		"v[%u,%u,%u]f[%u,%u,%u,%u,%u]r[%x,%x,%x,%x,%x,%x,%x,%x]t[%lu.%06lu]",
		power_fail_record.pwr_info.vvpu,
		power_fail_record.pwr_info.vcore,
		power_fail_record.pwr_info.vsram,
		power_fail_record.pwr_info.conn_freq,
		power_fail_record.pwr_info.vpu0_freq,
		power_fail_record.pwr_info.vpu1_freq,
		power_fail_record.pwr_info.mdla0_freq,
		power_fail_record.pwr_info.iommu_freq,
		power_fail_record.pwr_info.spm_wakeup,
		power_fail_record.pwr_info.rpc_intf_rdy,
		power_fail_record.pwr_info.vcore_cg_stat,
		power_fail_record.pwr_info.conn_cg_stat,
		power_fail_record.pwr_info.conn1_cg_stat,
		power_fail_record.pwr_info.vpu0_cg_stat,
		power_fail_record.pwr_info.vpu1_cg_stat,
		power_fail_record.pwr_info.mdla0_cg_stat,
		power_fail_record.time_sec, power_fail_record.time_nsec);

	seq_printf(s, "%s\n", log_str);
	if (ret <= 0)
		PWR_LOG_ERR("%s cannot print message\n", __func__);

#ifdef APUPWR_TAG_TP
	seq_puts(s, "\n");
	seq_puts(s, "======== Tags ========\n");
	apupwr_tags_show(s);
#endif

	return 0;
}
