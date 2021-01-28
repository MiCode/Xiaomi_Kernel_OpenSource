// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2017 MediaTek Inc.
 */


#include <linux/string.h>

#include "mmdvfs_plat.h"

#ifdef QOS_BOUND_DETECT
#include "mtk_qos_sram.h"
#endif

#include "smi_pmqos.h"

#undef pr_fmt
#define pr_fmt(fmt) "[mmdvfs][plat]" fmt

#define THERMAL_MASK 0x1	// 0b'001
#define THERMAL_OFFSET 0
#define CAM_MASK 0x6		// 0b'110
#define CAM_OFFSET 1
#define LIMIT_LEVEL1 0x7	// 0b'111
#define LIMIT_LEVEL2 0x5	// 0b'101
#define LIMIT_LEVEL3 0x3	// 0b'011

void mmdvfs_update_limit_config(enum mmdvfs_limit_source source,
	u32 source_value, u32 *limit_value, u32 *limit_level)
{
	if (source == MMDVFS_LIMIT_THERMAL)
		*limit_value = (*limit_value & ~THERMAL_MASK) |
			((source_value << THERMAL_OFFSET) & THERMAL_MASK);
	else if (source == MMDVFS_LIMIT_CAM)
		*limit_value = (*limit_value & ~CAM_MASK) |
			((source_value << CAM_OFFSET) & CAM_MASK);

	if ((*limit_value & LIMIT_LEVEL1) == LIMIT_LEVEL1)
		*limit_level = 1;
	else if ((*limit_value & LIMIT_LEVEL2) == LIMIT_LEVEL2)
		*limit_level = 2;
	else if ((*limit_value & LIMIT_LEVEL3) == LIMIT_LEVEL3)
		*limit_level = 3;
	else
		*limit_level = 0;
}
#define MDP_START 5
#define LARB_MDP_ID 1
#define LARB_VENC_ID 3
#define LARB_IMG1_ID 5
#define LARB_IMG2_ID 8
#define LARB_CAM1_ID 9
#define LARB_CAM2_ID 10
#define LARB_CAM3_ID 11
#define COMM_MDP_PORT 1
#define COMM_VENC_PORT 3
#define COMM_IMG1_PORT 4
#define COMM_IMG2_PORT 5
#define COMM_CAM1_PORT 6
#define COMM_CAM2_PORT 7

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

	if (larb_update & (1 << COMM_IMG1_PORT | 1 << COMM_IMG2_PORT)) {
		bw = larb_req[LARB_IMG1_ID].total_bw_data +
			larb_req[LARB_IMG2_ID].total_bw_data;
		qos_sram_write(MM_SMI_IMG, bw);
	}

	if (larb_update & (1 << COMM_CAM1_PORT | 1 << COMM_CAM2_PORT)) {
		bw = larb_req[LARB_CAM1_ID].total_bw_data +
			larb_req[LARB_CAM2_ID].total_bw_data +
			larb_req[LARB_CAM3_ID].total_bw_data;
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
		&larb_req[LARB_CAM1_ID].larb_list, larb_node) {
		if (enum_req->master_id == SMI_PORT_CCUI
			|| enum_req->master_id == SMI_PORT_CCUO)
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

