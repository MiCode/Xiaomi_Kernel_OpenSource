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

#define SMI_SCEN_NUM		(2)
#define SMI_ESL_INIT		(0)
#define SMI_ESL_VPWFD		(SMI_ESL_INIT)
#define SMI_ESL_ICFP		(1)
#define SMI_ESL_VR4K		(SMI_ESL_INIT)

static u32 smi_scen_map[SMI_BWC_SCEN_CNT] = {
	SMI_ESL_INIT, SMI_ESL_INIT, SMI_ESL_INIT,
	SMI_ESL_INIT, SMI_ESL_INIT, SMI_ESL_INIT,
	SMI_ESL_VPWFD, SMI_ESL_VPWFD, SMI_ESL_VPWFD,
	SMI_ESL_VPWFD, SMI_ESL_VPWFD,
	SMI_ESL_VR4K, SMI_ESL_VR4K, SMI_ESL_VR4K, SMI_ESL_VR4K,
	SMI_ESL_ICFP, SMI_ESL_ICFP, SMI_ESL_ICFP, SMI_ESL_INIT
};

static u32 smi_larb_cmd_gp_en_port[SMI_LARB_NUM][2] = {
	{0, 5}, {0, 0}, {0, 0}, {0, 0},
};

static u32 smi_larb_bw_thrt_en_port[SMI_LARB_NUM][2] = { /* non-HRT */
	{4, SMI_LARB0_PORT_NUM}, {0, SMI_LARB1_PORT_NUM},
	{0, SMI_LARB2_PORT_NUM}, {0, 0},
};

/* conf */
#define SMI_COMM_CONF_NUM	(8)
struct mtk_smi_pair smi_comm_conf_pair[SMI_COMM_CONF_NUM] = {
	{SMI_L1LEN, 0xb},
	{SMI_WRR_REG0, 0x30c30c},
	{SMI_DCM, 0x4fd},
	{SMI_Mx_RWULTRA_WRRy(1, 0, 0), 0x30c30c},
	{SMI_Mx_RWULTRA_WRRy(1, 1, 0), 0x30c30c},
	{SMI_Mx_RWULTRA_WRRy(2, 0, 0), 0x30c30c},
	{SMI_Mx_RWULTRA_WRRy(2, 1, 0), 0x30c30c},
	{SMI_DUMMY, 0x1},
};

#define SMI_LARB0_CONF_NUM	(8)
struct mtk_smi_pair smi_larb0_conf_pair[SMI_LARB0_CONF_NUM] = {
	{SMI_LARB_CMD_THRT_CON, 0x370223},
	{SMI_LARB_SW_FLAG, 0x1},
	{SMI_LARB_SPM_ULTRA_MASK, 0xffffffc0},
	{SMI_LARB_WRR_PORT(0), 0xb},
	{SMI_LARB_WRR_PORT(1), 0xb},
	{SMI_LARB_WRR_PORT(2), 0xb},
	{SMI_LARB_WRR_PORT(3), 0xb},
	{SMI_LARB_WRR_PORT(4), 0xb},
};

#define SMI_LARB1_CONF_NUM	(2)
struct mtk_smi_pair smi_larb1_conf_pair[SMI_LARB1_CONF_NUM] = {
	{SMI_LARB_CMD_THRT_CON, 0x370223},
	{SMI_LARB_SW_FLAG, 0x1},
};

#define SMI_LARB2_CONF_NUM	(3)
struct mtk_smi_pair smi_larb2_conf_pair[SMI_LARB2_CONF_NUM] = {
	{SMI_LARB_CMD_THRT_CON, 0x370223},
	{SMI_LARB_SW_FLAG, 0x1},
	{SMI_LARB_SPM_ULTRA_MASK, 0xffffc000},
};

u32 smi_conf_pair_num[SMI_LARB_NUM + 1] = {
	SMI_LARB0_CONF_NUM, SMI_LARB1_CONF_NUM,
	SMI_LARB2_CONF_NUM, SMI_LARB1_CONF_NUM,
	SMI_COMM_CONF_NUM,
};

struct mtk_smi_pair *smi_conf_pair[SMI_LARB_NUM + 1] = {
	smi_larb0_conf_pair, smi_larb1_conf_pair,
	smi_larb2_conf_pair, smi_larb1_conf_pair,
	smi_comm_conf_pair,
};

/* scen: INIT */
struct mtk_smi_pair smi_comm_init_pair[SMI_LARB_NUM] = {
	{SMI_L1ARB(0), 0x1ba5}, {SMI_L1ARB(1), 0x1000},
	{SMI_L1ARB(2), 0x15d3}, {SMI_L1ARB(3), 0x1000},
};

struct mtk_smi_pair smi_larb0_init_pair[SMI_LARB0_PORT_NUM] = {
	{SMI_LARB_OSTDL_PORT(0), 0x1f}, {SMI_LARB_OSTDL_PORT(1), 0x1f},
	{SMI_LARB_OSTDL_PORT(2), 0x1f}, {SMI_LARB_OSTDL_PORT(3), 0x7},
	{SMI_LARB_OSTDL_PORT(4), 0x7}, {SMI_LARB_OSTDL_PORT(5), 0x4},
	{SMI_LARB_OSTDL_PORT(6), 0x4}, {SMI_LARB_OSTDL_PORT(7), 0x1f},
};

struct mtk_smi_pair smi_larb1_init_pair[SMI_LARB1_PORT_NUM] = {
	{SMI_LARB_OSTDL_PORT(0), 0x3}, {SMI_LARB_OSTDL_PORT(1), 0x1},
	{SMI_LARB_OSTDL_PORT(2), 0x1}, {SMI_LARB_OSTDL_PORT(3), 0x1},
	{SMI_LARB_OSTDL_PORT(4), 0x1}, {SMI_LARB_OSTDL_PORT(5), 0x5},
	{SMI_LARB_OSTDL_PORT(6), 0x3}, {SMI_LARB_OSTDL_PORT(7), 0x1},
	{SMI_LARB_OSTDL_PORT(8), 0x1}, {SMI_LARB_OSTDL_PORT(9), 0x1},
	{SMI_LARB_OSTDL_PORT(10), 0x6},
};

struct mtk_smi_pair smi_larb2_init_pair[SMI_LARB2_PORT_NUM] = {
	{SMI_LARB_OSTDL_PORT(0), 0xc}, {SMI_LARB_OSTDL_PORT(1), 0x1},
	{SMI_LARB_OSTDL_PORT(2), 0x4}, {SMI_LARB_OSTDL_PORT(3), 0x4},
	{SMI_LARB_OSTDL_PORT(4), 0x1}, {SMI_LARB_OSTDL_PORT(5), 0x1},
	{SMI_LARB_OSTDL_PORT(6), 0x1}, {SMI_LARB_OSTDL_PORT(7), 0x1},
	{SMI_LARB_OSTDL_PORT(8), 0x1}, {SMI_LARB_OSTDL_PORT(9), 0x1},
	{SMI_LARB_OSTDL_PORT(10), 0x1}, {SMI_LARB_OSTDL_PORT(11), 0x1},
};

struct mtk_smi_pair smi_larb3_init_pair[SMI_LARB3_PORT_NUM] = {
	{SMI_LARB_OSTDL_PORT(0), 0x16}, {SMI_LARB_OSTDL_PORT(1), 0x14},
	{SMI_LARB_OSTDL_PORT(2), 0x2}, {SMI_LARB_OSTDL_PORT(3), 0x2},
	{SMI_LARB_OSTDL_PORT(4), 0x2}, {SMI_LARB_OSTDL_PORT(5), 0x2},
	{SMI_LARB_OSTDL_PORT(6), 0x4}, {SMI_LARB_OSTDL_PORT(7), 0x2},
	{SMI_LARB_OSTDL_PORT(8), 0x2}, {SMI_LARB_OSTDL_PORT(9), 0x2},
	{SMI_LARB_OSTDL_PORT(10), 0x2}, {SMI_LARB_OSTDL_PORT(11), 0x2},
	{SMI_LARB_OSTDL_PORT(12), 0x4}, {SMI_LARB_OSTDL_PORT(13), 0x4},
	{SMI_LARB_OSTDL_PORT(14), 0x4}, {SMI_LARB_OSTDL_PORT(15), 0x2},
	{SMI_LARB_OSTDL_PORT(16), 0x2}, {SMI_LARB_OSTDL_PORT(17), 0x2},
	{SMI_LARB_OSTDL_PORT(18), 0x2}, {SMI_LARB_OSTDL_PORT(19), 0x2},
	{SMI_LARB_OSTDL_PORT(20), 0x2},
};

/* scen: ICFP */
struct mtk_smi_pair smi_comm_icfp_pair[SMI_LARB_NUM] = {
	{SMI_L1ARB(0), 0x1327}, {SMI_L1ARB(1), 0x119e},
	{SMI_L1ARB(2), 0x1241}, {SMI_L1ARB(3), 0x12e6},
};

struct mtk_smi_pair smi_larb0_icfp_pair[SMI_LARB0_PORT_NUM] = {
	{SMI_LARB_OSTDL_PORT(0), 0x1f}, {SMI_LARB_OSTDL_PORT(1), 0x1f},
	{SMI_LARB_OSTDL_PORT(2), 0x1f}, {SMI_LARB_OSTDL_PORT(3), 0x7},
	{SMI_LARB_OSTDL_PORT(4), 0x7}, {SMI_LARB_OSTDL_PORT(5), 0x1},
	{SMI_LARB_OSTDL_PORT(6), 0xb}, {SMI_LARB_OSTDL_PORT(7), 0x1f},
};
/* {mdp_wdma0(5), 0x4}, {mdp_wrot0(6), 0x4} */

struct mtk_smi_pair smi_larb2_icfp_pair[SMI_LARB2_PORT_NUM] = {
	{SMI_LARB_OSTDL_PORT(0), 0x6}, {SMI_LARB_OSTDL_PORT(1), 0x1},
	{SMI_LARB_OSTDL_PORT(2), 0x2}, {SMI_LARB_OSTDL_PORT(3), 0x3},
	{SMI_LARB_OSTDL_PORT(4), 0x1}, {SMI_LARB_OSTDL_PORT(5), 0x1},
	{SMI_LARB_OSTDL_PORT(6), 0x1}, {SMI_LARB_OSTDL_PORT(7), 0x1},
	{SMI_LARB_OSTDL_PORT(8), 0x1}, {SMI_LARB_OSTDL_PORT(9), 0x1},
	{SMI_LARB_OSTDL_PORT(10), 0x1}, {SMI_LARB_OSTDL_PORT(11), 0x1},
};
/* {imgi(0), 0xc}, {img3o(2), 0x4}, {vipi(3), 0x4} */

struct mtk_smi_pair smi_larb3_icfp_pair[SMI_LARB3_PORT_NUM] = {
	{SMI_LARB_OSTDL_PORT(0), 0x14}, {SMI_LARB_OSTDL_PORT(1), 0x6},
	{SMI_LARB_OSTDL_PORT(2), 0x2}, {SMI_LARB_OSTDL_PORT(3), 0x2},
	{SMI_LARB_OSTDL_PORT(4), 0x2}, {SMI_LARB_OSTDL_PORT(5), 0x2},
	{SMI_LARB_OSTDL_PORT(6), 0x4}, {SMI_LARB_OSTDL_PORT(7), 0x2},
	{SMI_LARB_OSTDL_PORT(8), 0x2}, {SMI_LARB_OSTDL_PORT(9), 0x2},
	{SMI_LARB_OSTDL_PORT(10), 0x2}, {SMI_LARB_OSTDL_PORT(11), 0x2},
	{SMI_LARB_OSTDL_PORT(12), 0x4}, {SMI_LARB_OSTDL_PORT(13), 0x4},
	{SMI_LARB_OSTDL_PORT(14), 0x4}, {SMI_LARB_OSTDL_PORT(15), 0x2},
	{SMI_LARB_OSTDL_PORT(16), 0x2}, {SMI_LARB_OSTDL_PORT(17), 0x2},
	{SMI_LARB_OSTDL_PORT(18), 0x2}, {SMI_LARB_OSTDL_PORT(19), 0x2},
	{SMI_LARB_OSTDL_PORT(20), 0x2},
};
/* {imgo(0), 0x16}, {rrzo(1), 0x14} */

/* scen: ALL */
struct mtk_smi_pair *smi_comm_scen_pair[SMI_SCEN_NUM] = {
	smi_comm_init_pair, smi_comm_icfp_pair,
};

struct mtk_smi_pair *smi_larb0_scen_pair[SMI_SCEN_NUM] = {
	smi_larb0_init_pair, smi_larb0_icfp_pair,
};

struct mtk_smi_pair *smi_larb1_scen_pair[SMI_SCEN_NUM] = {
	smi_larb1_init_pair, smi_larb1_init_pair,
};

struct mtk_smi_pair *smi_larb2_scen_pair[SMI_SCEN_NUM] = {
	smi_larb2_init_pair, smi_larb2_icfp_pair,
};

struct mtk_smi_pair *smi_larb3_scen_pair[SMI_SCEN_NUM] = {
	smi_larb3_init_pair, smi_larb3_icfp_pair,
};

u32 smi_scen_pair_num[SMI_LARB_NUM + 1] = {
	SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM,
	SMI_LARB2_PORT_NUM, SMI_LARB3_PORT_NUM,
	SMI_LARB_NUM,
};

struct mtk_smi_pair **smi_scen_pair[SMI_LARB_NUM + 1] = {
	smi_larb0_scen_pair, smi_larb1_scen_pair,
	smi_larb2_scen_pair, smi_larb3_scen_pair,
	smi_comm_scen_pair,
};
#endif
