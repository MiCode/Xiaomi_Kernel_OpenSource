// SPDX-License-Identifier: GPL-2.0
//
// Copyright (C) 2018 MediaTek Inc.
// Copyright (C) 2021 XiaoMi, Inc.

#include "mt6785-afe-common.h"
#include "mtk-scp-ultra-platform-mem-control.h"
#include "mtk-scp-ultra-common.h"

const int get_scp_ultra_memif_id(int scp_ultra_task_id)
{
	switch (scp_ultra_task_id) {
	case SCP_ULTRA_DL_DAI_ID:
		return MT6785_MEMIF_DL7;
	case SCP_ULTRA_UL_DAI_ID:
		return MT6785_MEMIF_VUL3;
	default:
		pr_info("%s(), error return -1\n", __func__);
		return -1;
	}
}

