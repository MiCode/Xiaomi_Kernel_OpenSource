// SPDX-License-Identifier: GPL-2.0
//
// Copyright (C) 2018 MediaTek Inc.

#include "mt6779-afe-common.h"
#include "mtk-scp-spk-platform-mem-control.h"
#include "mtk-scp-spk-common.h"

const int get_scp_spk_memif_id(int scp_spk_task_id)
{
	switch (scp_spk_task_id) {
	case SCP_SPK_DL_DAI_ID:
		return MT6779_MEMIF_DL1;
	case SCP_SPK_IV_DAI_ID:
		return MT6779_MEMIF_AWB2;
	case SCP_SPK_MDUL_DAI_ID:
		return MT6779_MEMIF_AWB;
	default:
		pr_err("%s(), error return -1\n", __func__);
		return -1;
	}
}

