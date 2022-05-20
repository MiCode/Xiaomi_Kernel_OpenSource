/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __AOV_RPMSG_H__
#define __AOV_RPMSG_H__

#include <linux/types.h>
#include "apusys_core.h"
#include "npu_scp_ipi.h"

int scp_mdw_handler(struct npu_scp_ipi_param *recv_msg);

int aov_rpmsg_init(struct apusys_core_info *info);

void aov_rpmsg_exit(void);

#endif // __AOV_RPMSG_H__
