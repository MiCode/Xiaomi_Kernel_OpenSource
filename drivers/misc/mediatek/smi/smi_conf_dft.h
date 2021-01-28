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

#ifndef __SMI_CONF_DFT_H__
#define __SMI_CONF_DFT_H__

#include <soc/mediatek/smi.h>
#include "smi_reg.h"
#include "smi_conf_dbg.h"

#define SMI_LARB_NUM		(SMI_LARB_NUM_MAX)
#define SMI_SCEN_NUM		1
#define SMI_ESL_INIT		0
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

#define SMI_COMM_SCEN_NUM	0
#define SMI_LARB0_PORT_NUM	0 /* SYS_DIS */

static u32 smi_larb_cmd_gr_en_port[SMI_LARB_NUM][2] = {
	{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
};

static u32 smi_larb_bw_thrt_en_port[SMI_LARB_NUM][2] = { /* non-HRT */
	{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
};

/* conf */
#define SMI_COMM_CONF_NUM	0
struct mtk_smi_pair smi_comm_conf_pair[SMI_COMM_CONF_NUM] = {
	/* {SMI_L1LEN, 0xb}, {SMI_DUMMY, 0x1} */
};

#define SMI_LARB0_CONF_NUM	0
struct mtk_smi_pair smi_larb0_conf_pair[SMI_LARB0_CONF_NUM] = {
	/* {SMI_LARB_SW_FLAG, 0x1}, {SMI_LARB_WRR_PORT(0), 0xb} */
};

u32 smi_conf_pair_num[SMI_LARB_NUM + 1] = {
	SMI_LARB0_CONF_NUM, SMI_LARB0_CONF_NUM, SMI_LARB0_CONF_NUM,
	SMI_LARB0_CONF_NUM, SMI_LARB0_CONF_NUM, SMI_LARB0_CONF_NUM,
	SMI_LARB0_CONF_NUM, SMI_LARB0_CONF_NUM,
	SMI_COMM_CONF_NUM
};

struct mtk_smi_pair *smi_conf_pair[SMI_LARB_NUM + 1] = {
	smi_larb0_conf_pair, smi_larb0_conf_pair, smi_larb0_conf_pair,
	smi_larb0_conf_pair, smi_larb0_conf_pair, smi_larb0_conf_pair,
	smi_larb0_conf_pair, smi_larb0_conf_pair,
	smi_comm_conf_pair
};

/* scen: INIT */
struct mtk_smi_pair smi_comm_init_pair[SMI_COMM_SCEN_NUM] = {
	/* {SMI_L1ARB(0), 0x1000}, {SMI_BUS_SEL, 0x0} */
};

struct mtk_smi_pair smi_larb0_init_pair[SMI_LARB0_PORT_NUM] = {
	/* {SMI_LARB_OSTDL_PORT(0), 0x1f} */
};

/* scen: ALL */
struct mtk_smi_pair *smi_comm_scen_pair[SMI_SCEN_NUM] = {
	smi_comm_init_pair
};

struct mtk_smi_pair *smi_larb0_scen_pair[SMI_SCEN_NUM] = {
	smi_larb0_init_pair
};

u32 smi_scen_pair_num[SMI_LARB_NUM + 1] = {
	SMI_LARB0_PORT_NUM, SMI_LARB0_PORT_NUM, SMI_LARB0_PORT_NUM,
	SMI_LARB0_PORT_NUM, SMI_LARB0_PORT_NUM, SMI_LARB0_PORT_NUM,
	SMI_LARB0_PORT_NUM, SMI_LARB0_PORT_NUM,
	SMI_COMM_SCEN_NUM
};

struct mtk_smi_pair **smi_scen_pair[SMI_LARB_NUM + 1] = {
	smi_larb0_scen_pair, smi_larb0_scen_pair, smi_larb0_scen_pair,
	smi_larb0_scen_pair, smi_larb0_scen_pair, smi_larb0_scen_pair,
	smi_larb0_scen_pair, smi_larb0_scen_pair,
	smi_comm_scen_pair
};
#endif
