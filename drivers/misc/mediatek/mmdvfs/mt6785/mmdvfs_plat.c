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
 */

#include <linux/string.h>

#include "mmdvfs_plat.h"
#include "mtk_qos_sram.h"
#include "smi_pmqos.h"

#undef pr_fmt
#define pr_fmt(fmt) "[mmdvfs][plat]" fmt


#define MDP_START 5
#define LARB_MDP_ID 1
#define LARB_VENC_ID 3
#define LARB_IMG_ID 5
#define LARB_CAM_ID 6
#define COMM_MDP_PORT 1
#define COMM_VENC_PORT 3
#define COMM_IMG_PORT 4
#define COMM_CAM_PORT 7

#ifdef QOS_BOUND_DETECT
void mmdvfs_update_qos_sram(struct mm_larb_request larb_req[], u32 larb_update)
{

	u32 bw;
	struct mm_qos_request *enum_req = NULL;

	if (larb_update & (1 << COMM_MDP_PORT)) {
		bw = larb_req[LARB_MDP_ID].total_bw_data;
		list_for_each_entry(enum_req,
			&larb_req[LARB_MDP_ID].larb_list, larb_node) {
			if (SMI_PMQOS_PORT_MASK(
					enum_req->master_id) < MDP_START) {
				bw -= get_comp_value(enum_req->bw_value,
					enum_req->comp_type, true);
			}
		}
		qos_sram_write(MM_SMI_MDP, bw);
	}

	if (larb_update & (1 << COMM_VENC_PORT)) {
		bw = larb_req[LARB_VENC_ID].total_bw_data;
		qos_sram_write(MM_SMI_VENC, bw);
	}

	if (larb_update & (1 << COMM_IMG_PORT)) {
		bw = larb_req[LARB_IMG_ID].total_bw_data;
		qos_sram_write(MM_SMI_IMG, bw);
	}

	if (larb_update & (1 << COMM_CAM_PORT)) {
		bw = larb_req[LARB_CAM_ID].total_bw_data;
		qos_sram_write(MM_SMI_CAM, bw);
	}
}
#endif
static u32 log_common_port_ids;
static u32 log_larb_ids;
bool mmdvfs_log_larb_mmp(s32 common_port_id, s32 larb_id)
{
	if (common_port_id >= 0)
		return (1 << common_port_id) & log_common_port_ids;
	if (larb_id >= 0)
		return (1 << larb_id) & log_larb_ids;
	return false;
}

/* Return port number of CCU on SMI common */
inline u32 mmdvfs_get_ccu_smi_common_port(u32 master_id)
{
	return 6;
}

s32 get_ccu_hrt_bw(struct mm_larb_request larb_req[])
{
	struct mm_qos_request *enum_req = NULL;
	s32 bw = larb_req[SMI_PMQOS_LARB_DEC(get_virtual_port(
				VIRTUAL_CCU_COMMON))].total_hrt_data;

	list_for_each_entry(enum_req,
		&larb_req[LARB_CAM_ID].larb_list, larb_node) {
		if (enum_req->master_id == SMI_CCUI
			|| enum_req->master_id == SMI_CCUO
			|| enum_req->master_id == SMI_CCUG)
			bw += enum_req->hrt_value;
	}
	return bw;
}

s32 get_md_hrt_bw(void)
{
	return 1600;
}

s32 dram_write_weight(s32 val)
{
	return (val * 6 / 5);
}

