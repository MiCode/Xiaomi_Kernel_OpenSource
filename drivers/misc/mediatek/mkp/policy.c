// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 * Author: Kuan-Ying Lee <Kuan-Ying.Lee@mediatek.com>
 */

#include "policy.h"

int policy_ctrl[MKP_POLICY_NR];
void __init set_policy(int *policy_ctrl)
{
	pr_info("MKP: set policy start...\n");
	memset(policy_ctrl, 0, MKP_POLICY_NR*sizeof(int));

	/* default enable all policy */
	/* MKP default policies (0 ~ 15) */
	policy_ctrl[MKP_POLICY_MKP] = 0;			/* Policy ID for MKP itself */
	policy_ctrl[MKP_POLICY_DRV] = 0;			/* Policy ID for kernel drivers */
	policy_ctrl[MKP_POLICY_SELINUX_STATE] = 1;	/* Policy ID for selinux_state */
	policy_ctrl[MKP_POLICY_SELINUX_AVC] = 1;	/* Policy ID for selinux avc */
	policy_ctrl[MKP_POLICY_TASK_CRED] = 1;		/* Policy ID for task credential */
	policy_ctrl[MKP_POLICY_KERNEL_CODE] = 0;	/* Policy ID for kernel text */
	policy_ctrl[MKP_POLICY_KERNEL_RODATA] = 0;	/* Policy ID for kernel rodata */
	policy_ctrl[MKP_POLICY_KERNEL_PAGES] = 1;	/* Policy ID for other mapped kernel pages */
	policy_ctrl[MKP_POLICY_PGTABLE] = 1;		/* Policy ID for page table */
	policy_ctrl[MKP_POLICY_S1_MMU_CTRL] = 1;	/* Policy ID for stage-1 MMU control */
	policy_ctrl[MKP_POLICY_FILTER_SMC_HVC] = 1;	/* Policy ID for HVC/SMC call filtering */

	/* Policies for vendors start from here (16 ~ 31) */
	policy_ctrl[MKP_POLICY_VENDOR_START] = 1;
	pr_info("MKP: set policy Done\n");
	return;
}
