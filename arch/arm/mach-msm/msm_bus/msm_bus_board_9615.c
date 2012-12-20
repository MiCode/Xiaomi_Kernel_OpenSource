/* Copyright (c) 2011, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/module.h>
#include <mach/msm_bus.h>
#include <mach/msm_bus_board.h>
#include <mach/board.h>
#include <mach/rpm.h>
#include "msm_bus_core.h"

#define NMASTERS 14
#define NSLAVES 12
#define NFAB_9615 2

enum msm_bus_fabric_tiered_slave_type {
	MSM_BUS_TIERED_SLAVE_EBI1_CH0 = 1,
};

enum msm_bus_9615_master_ports_type {
	MSM_BUS_MASTER_PORT_SPS = 0,
	MSM_BUS_MASTER_PORT_APSS_PROC,
	MSM_BUS_MASTER_PORT_ADM_PORT0,
	MSM_BUS_MASTER_PORT_ADM_PORT1,
	MSM_BUS_MASTER_PORT_LPASS_PROC,
	MSM_BUS_MASTER_PORT_MSS,
	MSM_BUS_MASTER_PORT_MSS_SW_PROC,
	MSM_BUS_MASTER_PORT_MSS_FW_PROC,
	MSM_BUS_MASTER_PORT_LPASS,
	MSM_BUS_SYSTEM_MASTER_PORT_CPSS_FPB,
	MSM_BUS_SYSTEM_MASTER_PORT_SYSTEM_FPB,
	MSM_BUS_SYSTEM_MASTER_PORT_ADM_AHB_CI,
};

enum msm_bus_9615_slave_ports_type {
	MSM_BUS_SLAVE_PORT_SPS = 0,
	MSM_BUS_SLAVE_PORT_EBI1_CH0,
	MSM_BUS_SLAVE_PORT_APSS_L2,
	MSM_BUS_SLAVE_PORT_SYSTEM_IMEM,
	MSM_BUS_SLAVE_PORT_CORESIGHT,
	MSM_BUS_SLAVE_PORT_APSS,
	MSM_BUS_SLAVE_PORT_MSS,
	MSM_BUS_SLAVE_PORT_LPASS,
	MSM_BUS_SYSTEM_SLAVE_PORT_CPSS_FPB,
	MSM_BUS_SYSTEM_SLAVE_PORT_SYSTEM_FPB,
};

static int tier2[] = {MSM_BUS_BW_TIER2,};
static uint32_t master_iids[NMASTERS];
static uint32_t slave_iids[NSLAVES];

static int sport_ebi1_ch0[] = {MSM_BUS_SLAVE_PORT_EBI1_CH0,};
static int sport_apss_l2[] = {MSM_BUS_SLAVE_PORT_APSS_L2,};

static int tiered_slave_ebi1_ch0[] = {MSM_BUS_TIERED_SLAVE_EBI1_CH0,};

static int mport_sps[] = {MSM_BUS_MASTER_PORT_SPS,};
static int mport_apss_proc[] = {MSM_BUS_MASTER_PORT_APSS_PROC,};
static int mport_adm_port0[] = {MSM_BUS_MASTER_PORT_ADM_PORT0,};
static int mport_adm_port1[] = {MSM_BUS_MASTER_PORT_ADM_PORT1,};
static int mport_mss[] = {MSM_BUS_MASTER_PORT_MSS,};
static int mport_lpass_proc[] = {MSM_BUS_MASTER_PORT_LPASS_PROC,};
static int mport_mss_sw_proc[] = {MSM_BUS_MASTER_PORT_MSS_SW_PROC,};
static int mport_mss_fw_proc[] = {MSM_BUS_MASTER_PORT_MSS_FW_PROC,};
static int mport_lpass[] = {MSM_BUS_MASTER_PORT_LPASS,};
static int system_mport_adm_ahb_ci[] = {MSM_BUS_SYSTEM_MASTER_PORT_ADM_AHB_CI,};
static int mport_system_fpb[] = {MSM_BUS_SYSTEM_MASTER_PORT_SYSTEM_FPB,};
static int system_mport_cpss_fpb[] = {MSM_BUS_SYSTEM_MASTER_PORT_CPSS_FPB,};

static int system_sport_system_fpb[] = {MSM_BUS_SYSTEM_SLAVE_PORT_SYSTEM_FPB,};
static int system_sport_cpss_fpb[] = {MSM_BUS_SYSTEM_SLAVE_PORT_CPSS_FPB,};
static int sport_sps[] = {MSM_BUS_SLAVE_PORT_SPS,};
static int sport_system_imem[] = {MSM_BUS_SLAVE_PORT_SYSTEM_IMEM,};
static int sport_coresight[] = {MSM_BUS_SLAVE_PORT_CORESIGHT,};
static int sport_apss[] = {MSM_BUS_SLAVE_PORT_APSS,};
static int sport_mss[] = {MSM_BUS_SLAVE_PORT_MSS,};
static int sport_lpass[] = {MSM_BUS_SLAVE_PORT_LPASS,};

static struct msm_bus_node_info system_fabric_info[]  = {
	{
		.id = MSM_BUS_MASTER_SPS,
		.masterp = mport_sps,
		.num_mports = ARRAY_SIZE(mport_sps),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
	},
	{
		.id = MSM_BUS_MASTER_ADM_PORT0,
		.masterp = mport_adm_port0,
		.num_mports = ARRAY_SIZE(mport_adm_port0),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
	},
	{
		.id = MSM_BUS_MASTER_ADM_PORT1,
		.masterp = mport_adm_port1,
		.num_mports = ARRAY_SIZE(mport_adm_port1),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
	},
	{
		.id = MSM_BUS_MASTER_LPASS_PROC,
		.masterp = mport_lpass_proc,
		.num_mports = ARRAY_SIZE(mport_lpass_proc),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
	},
	{
		.id = MSM_BUS_MASTER_MSS,
		.masterp = mport_mss,
		.num_mports = ARRAY_SIZE(mport_mss),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
	},
	{
		.id = MSM_BUS_MASTER_AMPSS_M0,
		.masterp = mport_apss_proc,
		.num_mports = ARRAY_SIZE(mport_apss_proc),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
	},
	{
		.id = MSM_BUS_MASTER_MSS_SW_PROC,
		.masterp = mport_mss_sw_proc,
		.num_mports = ARRAY_SIZE(mport_mss_sw_proc),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
	},
	{
		.id = MSM_BUS_MASTER_MSS_FW_PROC,
		.masterp = mport_mss_fw_proc,
		.num_mports = ARRAY_SIZE(mport_mss_fw_proc),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
	},
	{
		.id = MSM_BUS_MASTER_LPASS,
		.masterp = mport_lpass,
		.num_mports = ARRAY_SIZE(mport_lpass),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
	},
	{
		.id = MSM_BUS_MASTER_ADM0_CI,
		.masterp = system_mport_adm_ahb_ci,
		.num_mports = ARRAY_SIZE(system_mport_adm_ahb_ci),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
	},
	{
		.id = MSM_BUS_FAB_SYSTEM_FPB,
		.gateway = 1,
		.slavep = system_sport_system_fpb,
		.num_sports = ARRAY_SIZE(system_sport_system_fpb),
		.masterp = mport_system_fpb,
		.num_mports = ARRAY_SIZE(mport_system_fpb),
		.buswidth = 4,
	},
	{
		.id = MSM_BUS_FAB_CPSS_FPB,
		.gateway = 1,
		.slavep = system_sport_cpss_fpb,
		.num_sports = ARRAY_SIZE(system_sport_cpss_fpb),
		.masterp = system_mport_cpss_fpb,
		.num_mports = ARRAY_SIZE(system_mport_cpss_fpb),
		.buswidth = 4,
	},
	{
		.id = MSM_BUS_SLAVE_SPS,
		.slavep = sport_sps,
		.num_sports = ARRAY_SIZE(sport_sps),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.slaveclk[DUAL_CTX] = "dfab_clk",
		.slaveclk[ACTIVE_CTX] = "dfab_a_clk",
	},
	{
		.id = MSM_BUS_SLAVE_EBI_CH0,
		.slavep = sport_ebi1_ch0,
		.num_sports = ARRAY_SIZE(sport_ebi1_ch0),
		.tier = tiered_slave_ebi1_ch0,
		.num_tiers = ARRAY_SIZE(tiered_slave_ebi1_ch0),
		.buswidth = 8,
		.slaveclk[DUAL_CTX] = "mem_clk",
		.slaveclk[ACTIVE_CTX] = "mem_a_clk",
	},
	{
		.id = MSM_BUS_SLAVE_SYSTEM_IMEM,
		.slavep = sport_system_imem,
		.num_sports = ARRAY_SIZE(sport_system_imem),
		.buswidth = 8,
	},
	{
		.id = MSM_BUS_SLAVE_CORESIGHT,
		.slavep = sport_coresight,
		.num_sports = ARRAY_SIZE(sport_coresight),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
	},
	{
		.id = MSM_BUS_SLAVE_AMPSS,
		.slavep = sport_apss,
		.num_sports = ARRAY_SIZE(sport_apss),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
	},
	{
		.id = MSM_BUS_SLAVE_AMPSS_L2,
		.slavep = sport_apss_l2,
		.num_sports = ARRAY_SIZE(sport_apss_l2),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
	},
	{
		.id = MSM_BUS_SLAVE_MSS,
		.slavep = sport_mss,
		.num_sports = ARRAY_SIZE(sport_mss),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
	},
	{
		.id = MSM_BUS_SLAVE_LPASS,
		.slavep = sport_lpass,
		.num_sports = ARRAY_SIZE(sport_lpass),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
	},
};

static void msm_bus_board_assign_iids(struct msm_bus_fabric_registration
	*fabreg, int fabid)
{
	int i;
	for (i = 0; i < fabreg->len; i++) {
		if (!fabreg->info[i].gateway) {
			fabreg->info[i].priv_id = fabid + fabreg->info[i].id;
			if (fabreg->info[i].id < SLAVE_ID_KEY)
				master_iids[fabreg->info[i].id] =
					fabreg->info[i].priv_id;
			else
				slave_iids[fabreg->info[i].id - (SLAVE_ID_KEY)]
					= fabreg->info[i].priv_id;
		} else
			fabreg->info[i].priv_id = fabreg->info[i].id;
	}
}

static int msm_bus_board_9615_get_iid(int id)
{
	if ((id < SLAVE_ID_KEY && id >= NMASTERS) ||
		id >= (SLAVE_ID_KEY + NSLAVES)) {
		MSM_BUS_ERR("Cannot get iid. Invalid id %d passed\n", id);
		return -EINVAL;
	}

	return ((id < SLAVE_ID_KEY) ? master_iids[id] : slave_iids[id -
		SLAVE_ID_KEY]);
}

static struct msm_bus_board_algorithm msm_bus_board_algo = {
	.board_nfab = NFAB_9615,
	.get_iid = msm_bus_board_9615_get_iid,
	.assign_iids = msm_bus_board_assign_iids,
};

struct msm_bus_fabric_registration msm_bus_9615_sys_fabric_pdata = {
	.id = MSM_BUS_FAB_SYSTEM,
	.name = "msm_sys_fab",
	system_fabric_info,
	ARRAY_SIZE(system_fabric_info),
	.ahb = 0,
	.fabclk[DUAL_CTX] = "bus_clk",
	.fabclk[ACTIVE_CTX] = "bus_a_clk",
	.haltid = MSM_RPM_ID_SYS_FABRIC_CFG_HALT_0,
	.offset = MSM_RPM_ID_SYSTEM_FABRIC_ARB_0,
	.nmasters = 12,
	.nslaves = 10,
	.ntieredslaves = 1,
	.board_algo = &msm_bus_board_algo,
};

struct msm_bus_fabric_registration msm_bus_9615_def_fab_pdata = {
	.id = MSM_BUS_FAB_DEFAULT,
	.name = "msm_def_fab",
	.ahb = 1,
	.nmasters = 0,
	.nslaves = 0,
	.ntieredslaves = 0,
	.board_algo = &msm_bus_board_algo,
};

int msm_bus_board_rpm_get_il_ids(uint16_t id[])
{
	return -ENXIO;
}

void msm_bus_board_init(struct msm_bus_fabric_registration *pdata)
{
}
