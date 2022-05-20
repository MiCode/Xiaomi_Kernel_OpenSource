/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __AOV_RECOVERY_H__
#define __AOV_RECOVERY_H__

#include <linux/types.h>
#include "apusys_core.h"
#include "npu_scp_ipi.h"

enum aov_apu_recovery_status {
	AOV_APU_INIT = 0,
	AOV_APU_RECOVERING,
	AOV_APU_RECOVER_DONE,
};

int aov_recovery_handler(struct npu_scp_ipi_param *recv_msg);

int aov_recovery_init(struct apusys_core_info *info);

void aov_recovery_exit(void);

#endif // __AOV_RECOVERY_H__
