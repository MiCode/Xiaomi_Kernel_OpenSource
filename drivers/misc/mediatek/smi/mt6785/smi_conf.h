/*
 * Copyright (C) 2018 MediaTek Inc.
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

#ifndef __SMI_CONF_H__
#define __SMI_CONF_H__

#include <soc/mediatek/smi.h>
#include "smi_hw.h"
#include "../smi_reg.h"
#include "../smi_conf_dbg.h"

#define SMI_SCEN_NUM		(1)
#define SMI_ESL_INIT		(0)
#define SMI_ESL_VPWFD		(SMI_ESL_INIT)
#define SMI_ESL_VR4K		(SMI_ESL_INIT)
#define SMI_ESL_ICFP		(SMI_ESL_INIT)

static u32 smi_scen_map[SMI_BWC_SCEN_CNT] = {
	SMI_ESL_INIT, SMI_ESL_INIT, SMI_ESL_INIT,
	SMI_ESL_INIT, SMI_ESL_INIT, SMI_ESL_INIT,
	SMI_ESL_VPWFD, SMI_ESL_VPWFD, SMI_ESL_VPWFD,
	SMI_ESL_VPWFD, SMI_ESL_VPWFD,
	SMI_ESL_VR4K, SMI_ESL_VR4K, SMI_ESL_VR4K, SMI_ESL_VR4K,
	SMI_ESL_ICFP, SMI_ESL_ICFP, SMI_ESL_ICFP, SMI_ESL_INIT
};

static u32 smi_larb_cmd_gp_en_port[SMI_LARB_NUM][2] = {
	{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
};

static u32 smi_larb_bw_thrt_en_port[SMI_LARB_NUM][2] = { /* non-HRT */
	{0, 0}, {5, SMI_LARB1_PORT_NUM}, {0, SMI_LARB2_PORT_NUM},
	{0, SMI_LARB3_PORT_NUM}, {0, 0}, {0, SMI_LARB5_PORT_NUM},
	{0, 0}, {0, 0},
};

/* conf */
#define SMI_LARB0_CONF_NUM	(11)
struct mtk_smi_pair smi_larb0_conf_pair[SMI_LARB0_CONF_NUM] = {
	{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},
	{SMI_LARB_WRR_PORT(0), 0xb}, {SMI_LARB_WRR_PORT(1), 0xb},
	{SMI_LARB_WRR_PORT(2), 0xb}, {SMI_LARB_WRR_PORT(3), 0x5},
	{SMI_LARB_WRR_PORT(4), 0x5}, {SMI_LARB_WRR_PORT(5), 0xb},
	{SMI_LARB_WRR_PORT(6), 0xb}, {SMI_LARB_WRR_PORT(7), 0xb},
	{SMI_LARB_WRR_PORT(8), 0xb},
};

#define SMI_LARB1_CONF_NUM	(16)
struct mtk_smi_pair smi_larb1_conf_pair[SMI_LARB1_CONF_NUM] = {
	{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},
	{SMI_LARB_WRR_PORT(0), 0xb}, {SMI_LARB_WRR_PORT(1), 0xb},
	{SMI_LARB_WRR_PORT(2), 0x5}, {SMI_LARB_WRR_PORT(3), 0x5},
	{SMI_LARB_WRR_PORT(4), 0xb}, {SMI_LARB_WRR_PORT(5), 0x1},
	{SMI_LARB_WRR_PORT(6), 0x1}, {SMI_LARB_WRR_PORT(7), 0x1},
	{SMI_LARB_WRR_PORT(8), 0x1}, {SMI_LARB_WRR_PORT(9), 0x1},
	{SMI_LARB_WRR_PORT(10), 0x1}, {SMI_LARB_WRR_PORT(11), 0x1},
	{SMI_LARB_WRR_PORT(12), 0x1}, {SMI_LARB_WRR_PORT(13), 0x4},
};

#define SMI_LARB2_CONF_NUM	(2)
struct mtk_smi_pair smi_larb2_conf_pair[SMI_LARB2_CONF_NUM] = {
	{SMI_LARB_CMD_THRT_CON, 0x300256}, {SMI_LARB_SW_FLAG, 0x1},
};

#define SMI_LARB3_CONF_NUM	(2)
struct mtk_smi_pair smi_larb3_conf_pair[SMI_LARB3_CONF_NUM] = {
	{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},
};

#define SMI_COMM_CONF_NUM	(6)
struct mtk_smi_pair smi_comm_conf_pair[SMI_COMM_CONF_NUM] = {
	{SMI_L1LEN, 0xb}, {SMI_M4U_TH, 0xe100e10}, {SMI_FIFO_TH1, 0x506090a},
	{SMI_FIFO_TH2, 0x506090a}, {SMI_DCM, 0x4f1}, {SMI_DUMMY, 0x1},
};

u32 smi_conf_pair_num[SMI_LARB_NUM + 1] = {
	SMI_LARB0_CONF_NUM, SMI_LARB1_CONF_NUM, SMI_LARB2_CONF_NUM,
	SMI_LARB3_CONF_NUM, SMI_LARB2_CONF_NUM, SMI_LARB2_CONF_NUM,
	SMI_LARB3_CONF_NUM, SMI_LARB3_CONF_NUM,
	SMI_COMM_CONF_NUM,
};

struct mtk_smi_pair *smi_conf_pair[SMI_LARB_NUM + 1] = {
	smi_larb0_conf_pair, smi_larb1_conf_pair, smi_larb2_conf_pair,
	smi_larb3_conf_pair, smi_larb2_conf_pair, smi_larb2_conf_pair,
	smi_larb3_conf_pair, smi_larb3_conf_pair,
	smi_comm_conf_pair,
};

/* scen: INIT */
struct mtk_smi_pair smi_larb0_init_pair[SMI_LARB0_PORT_NUM] = {
	{SMI_LARB_OSTDL_PORT(0), 0x2}, {SMI_LARB_OSTDL_PORT(1), 0x2},
	{SMI_LARB_OSTDL_PORT(2), 0x1}, {SMI_LARB_OSTDL_PORT(3), 0x1f},
	{SMI_LARB_OSTDL_PORT(4), 0x1}, {SMI_LARB_OSTDL_PORT(5), 0x1},
	{SMI_LARB_OSTDL_PORT(6), 0xa}, {SMI_LARB_OSTDL_PORT(7), 0xa},
	{SMI_LARB_OSTDL_PORT(8), 0x28},
};

struct mtk_smi_pair smi_larb1_init_pair[SMI_LARB1_PORT_NUM] = {
	{SMI_LARB_OSTDL_PORT(0), 0x2}, {SMI_LARB_OSTDL_PORT(1), 0x1},
	{SMI_LARB_OSTDL_PORT(2), 0x1c}, {SMI_LARB_OSTDL_PORT(3), 0x1c},
	{SMI_LARB_OSTDL_PORT(4), 0xa}, {SMI_LARB_OSTDL_PORT(5), 0x1},
	{SMI_LARB_OSTDL_PORT(6), 0x1}, {SMI_LARB_OSTDL_PORT(7), 0xd},
	{SMI_LARB_OSTDL_PORT(8), 0xd}, {SMI_LARB_OSTDL_PORT(9), 0xe},
	{SMI_LARB_OSTDL_PORT(10), 0x1}, {SMI_LARB_OSTDL_PORT(11), 0xe},
	{SMI_LARB_OSTDL_PORT(12), 0x1}, {SMI_LARB_OSTDL_PORT(13), 0x28},
};

struct mtk_smi_pair smi_larb2_init_pair[SMI_LARB2_PORT_NUM] = {
	{SMI_LARB_OSTDL_PORT(0), 0x18}, {SMI_LARB_OSTDL_PORT(1), 0x1},
	{SMI_LARB_OSTDL_PORT(2), 0x8}, {SMI_LARB_OSTDL_PORT(3), 0x1},
	{SMI_LARB_OSTDL_PORT(4), 0x1}, {SMI_LARB_OSTDL_PORT(5), 0x1},
	{SMI_LARB_OSTDL_PORT(6), 0x1}, {SMI_LARB_OSTDL_PORT(7), 0x1},
	{SMI_LARB_OSTDL_PORT(8), 0x1}, {SMI_LARB_OSTDL_PORT(9), 0x2},
	{SMI_LARB_OSTDL_PORT(10), 0x1}, {SMI_LARB_OSTDL_PORT(11), 0x1},
};

struct mtk_smi_pair smi_larb3_init_pair[SMI_LARB3_PORT_NUM] = {
	{SMI_LARB_OSTDL_PORT(0), 0x1}, {SMI_LARB_OSTDL_PORT(1), 0x3},
	{SMI_LARB_OSTDL_PORT(2), 0x2}, {SMI_LARB_OSTDL_PORT(3), 0x1},
	{SMI_LARB_OSTDL_PORT(4), 0x1}, {SMI_LARB_OSTDL_PORT(5), 0x1},
	{SMI_LARB_OSTDL_PORT(6), 0x1}, {SMI_LARB_OSTDL_PORT(7), 0x4},
	{SMI_LARB_OSTDL_PORT(8), 0x2}, {SMI_LARB_OSTDL_PORT(9), 0x1},
	{SMI_LARB_OSTDL_PORT(10), 0x4}, {SMI_LARB_OSTDL_PORT(11), 0x1},
	{SMI_LARB_OSTDL_PORT(12), 0x1}, {SMI_LARB_OSTDL_PORT(13), 0x1},
	{SMI_LARB_OSTDL_PORT(14), 0x1}, {SMI_LARB_OSTDL_PORT(15), 0x4},
	{SMI_LARB_OSTDL_PORT(16), 0xb}, {SMI_LARB_OSTDL_PORT(17), 0x13},
	{SMI_LARB_OSTDL_PORT(18), 0x14},
};

struct mtk_smi_pair smi_larb4_init_pair[SMI_LARB4_PORT_NUM] = {
	{SMI_LARB_OSTDL_PORT(0), 0x1}, {SMI_LARB_OSTDL_PORT(1), 0x1},
	{SMI_LARB_OSTDL_PORT(2), 0x1},
};

struct mtk_smi_pair smi_larb5_init_pair[SMI_LARB5_PORT_NUM] = {
	{SMI_LARB_OSTDL_PORT(0), 0xc}, {SMI_LARB_OSTDL_PORT(1), 0x1},
	{SMI_LARB_OSTDL_PORT(2), 0x4}, {SMI_LARB_OSTDL_PORT(3), 0x4},
	{SMI_LARB_OSTDL_PORT(4), 0x1}, {SMI_LARB_OSTDL_PORT(5), 0x2},
	{SMI_LARB_OSTDL_PORT(6), 0x2}, {SMI_LARB_OSTDL_PORT(7), 0x6},
	{SMI_LARB_OSTDL_PORT(8), 0x1}, {SMI_LARB_OSTDL_PORT(9), 0x9},
	{SMI_LARB_OSTDL_PORT(10), 0x2}, {SMI_LARB_OSTDL_PORT(11), 0x2},
	{SMI_LARB_OSTDL_PORT(12), 0x2}, {SMI_LARB_OSTDL_PORT(13), 0x1},
	{SMI_LARB_OSTDL_PORT(14), 0x6}, {SMI_LARB_OSTDL_PORT(15), 0x9},
	{SMI_LARB_OSTDL_PORT(16), 0x1}, {SMI_LARB_OSTDL_PORT(17), 0x1},
	{SMI_LARB_OSTDL_PORT(18), 0x6}, {SMI_LARB_OSTDL_PORT(19), 0x4},
	{SMI_LARB_OSTDL_PORT(20), 0x4}, {SMI_LARB_OSTDL_PORT(21), 0x1},
	{SMI_LARB_OSTDL_PORT(22), 0x1}, {SMI_LARB_OSTDL_PORT(23), 0x1},
	{SMI_LARB_OSTDL_PORT(24), 0x1}, {SMI_LARB_OSTDL_PORT(25), 0x2},
};

struct mtk_smi_pair smi_larb6_init_pair[SMI_LARB6_PORT_NUM] = {
	{SMI_LARB_OSTDL_PORT(0), 0x1f}, {SMI_LARB_OSTDL_PORT(1), 0x14},
	{SMI_LARB_OSTDL_PORT(2), 0x2}, {SMI_LARB_OSTDL_PORT(3), 0x2},
	{SMI_LARB_OSTDL_PORT(4), 0x2}, {SMI_LARB_OSTDL_PORT(5), 0x1},
	{SMI_LARB_OSTDL_PORT(6), 0x16}, {SMI_LARB_OSTDL_PORT(7), 0x8},
	{SMI_LARB_OSTDL_PORT(8), 0x2}, {SMI_LARB_OSTDL_PORT(9), 0x2},
	{SMI_LARB_OSTDL_PORT(10), 0x2}, {SMI_LARB_OSTDL_PORT(11), 0x1},
	{SMI_LARB_OSTDL_PORT(12), 0xa}, {SMI_LARB_OSTDL_PORT(13), 0xa},
	{SMI_LARB_OSTDL_PORT(14), 0x4}, {SMI_LARB_OSTDL_PORT(15), 0x2},
	{SMI_LARB_OSTDL_PORT(16), 0x2}, {SMI_LARB_OSTDL_PORT(17), 0x2},
	{SMI_LARB_OSTDL_PORT(18), 0x2}, {SMI_LARB_OSTDL_PORT(19), 0x2},
	{SMI_LARB_OSTDL_PORT(20), 0x2}, {SMI_LARB_OSTDL_PORT(21), 0x2},
	{SMI_LARB_OSTDL_PORT(22), 0x2}, {SMI_LARB_OSTDL_PORT(23), 0x2},
	{SMI_LARB_OSTDL_PORT(24), 0x2}, {SMI_LARB_OSTDL_PORT(25), 0x1},
	{SMI_LARB_OSTDL_PORT(26), 0x1}, {SMI_LARB_OSTDL_PORT(27), 0x1},
	{SMI_LARB_OSTDL_PORT(28), 0x1}, {SMI_LARB_OSTDL_PORT(29), 0x1},
	{SMI_LARB_OSTDL_PORT(30), 0x1},
};

struct mtk_smi_pair smi_larb7_init_pair[SMI_LARB7_PORT_NUM] = {
	{SMI_LARB_OSTDL_PORT(0), 0x1f}, {SMI_LARB_OSTDL_PORT(1), 0x14},
	{SMI_LARB_OSTDL_PORT(2), 0x1f}, {SMI_LARB_OSTDL_PORT(3), 0x1f},
	{SMI_LARB_OSTDL_PORT(4), 0x14},
};

struct mtk_smi_pair smi_comm_init_pair[SMI_LARB_NUM + 1] = {
	{SMI_L1ARB(0), 0x0}, {SMI_L1ARB(1), 0x0}, {SMI_L1ARB(2), 0x0},
	{SMI_L1ARB(3), 0x0}, {SMI_L1ARB(4), 0x0}, {SMI_L1ARB(5), 0x0},
	{SMI_L1ARB(6), 0x0}, {SMI_L1ARB(7), 0x0},
};

/* scen: ALL */
struct mtk_smi_pair *smi_larb0_scen_pair[SMI_SCEN_NUM] = {
	smi_larb0_init_pair,
};

struct mtk_smi_pair *smi_larb1_scen_pair[SMI_SCEN_NUM] = {
	smi_larb1_init_pair,
};

struct mtk_smi_pair *smi_larb2_scen_pair[SMI_SCEN_NUM] = {
	smi_larb2_init_pair,
};

struct mtk_smi_pair *smi_larb3_scen_pair[SMI_SCEN_NUM] = {
	smi_larb3_init_pair,
};

struct mtk_smi_pair *smi_larb4_scen_pair[SMI_SCEN_NUM] = {
	smi_larb4_init_pair,
};

struct mtk_smi_pair *smi_larb5_scen_pair[SMI_SCEN_NUM] = {
	smi_larb5_init_pair,
};

struct mtk_smi_pair *smi_larb6_scen_pair[SMI_SCEN_NUM] = {
	smi_larb6_init_pair,
};

struct mtk_smi_pair *smi_larb7_scen_pair[SMI_SCEN_NUM] = {
	smi_larb7_init_pair,
};

struct mtk_smi_pair *smi_comm_scen_pair[SMI_SCEN_NUM] = {
	smi_comm_init_pair,
};

u32 smi_scen_pair_num[SMI_LARB_NUM + 1] = {
	SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM, SMI_LARB2_PORT_NUM,
	SMI_LARB3_PORT_NUM, SMI_LARB4_PORT_NUM, SMI_LARB5_PORT_NUM,
	SMI_LARB6_PORT_NUM, SMI_LARB7_PORT_NUM,
	SMI_COMM_MASTER_NUM,
};

struct mtk_smi_pair **smi_scen_pair[SMI_LARB_NUM + 1] = {
	smi_larb0_scen_pair, smi_larb1_scen_pair, smi_larb2_scen_pair,
	smi_larb3_scen_pair, smi_larb4_scen_pair, smi_larb5_scen_pair,
	smi_larb6_scen_pair, smi_larb7_scen_pair,
	smi_comm_scen_pair,
};
#endif
