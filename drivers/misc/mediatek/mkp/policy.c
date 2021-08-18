// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 * Author: Kuan-Ying Lee <Kuan-Ying.Lee@mediatek.com>
 */

#include "policy.h"

DEBUG_SET_LEVEL(DEBUG_LEVEL_ERR);

int __ro_after_init policy_ctrl[MKP_POLICY_NR];
uint32_t __ro_after_init mkp_policy_action[MKP_POLICY_NR] = {
	/* Policy Table */
	[MKP_POLICY_MKP]		= (NO_MAP_TO_DEVICE | NO_UPGRADE_TO_WRITE | NO_UPGRADE_TO_EXEC | HANDLE_PERMANENT | ACTION_PANIC),
	[MKP_POLICY_DRV]		= (NO_MAP_TO_DEVICE | ACTION_PANIC),
	[MKP_POLICY_SELINUX_STATE]  = (NO_MAP_TO_DEVICE | NO_UPGRADE_TO_WRITE | HANDLE_PERMANENT | ACTION_PANIC),
	[MKP_POLICY_SELINUX_AVC]	= (NO_MAP_TO_DEVICE | NO_UPGRADE_TO_WRITE | HANDLE_PERMANENT | SB_ENTRY_DISORDERED | ACTION_WARNING),
	[MKP_POLICY_TASK_CRED]	  	= (NO_MAP_TO_DEVICE | NO_UPGRADE_TO_WRITE | HANDLE_PERMANENT | SB_ENTRY_ORDERED | ACTION_WARNING),
	[MKP_POLICY_KERNEL_CODE]	= (NO_MAP_TO_DEVICE | NO_UPGRADE_TO_WRITE | HANDLE_PERMANENT | ACTION_WARNING),
	[MKP_POLICY_KERNEL_RODATA]  = (NO_MAP_TO_DEVICE | NO_UPGRADE_TO_WRITE | HANDLE_PERMANENT | ACTION_WARNING),
	[MKP_POLICY_KERNEL_PAGES]   = (0x0 | ACTION_NOTIFICATION),
	[MKP_POLICY_PGTABLE]		= (0x0),
	[MKP_POLICY_S1_MMU_CTRL]	= (0x0),
	[MKP_POLICY_FILTER_SMC_HVC]	= (0x0),
};
void __init set_policy(void)
{
	MKP_INFO("MKP: set policy start...\n");
	memset(policy_ctrl, 0, MKP_POLICY_NR*sizeof(int));

	/* default enable all policy */
	/* MKP default policies (0 ~ 15) */
	policy_ctrl[MKP_POLICY_MKP] = 1;			/* Policy ID for MKP itself */
	policy_ctrl[MKP_POLICY_DRV] = 1;			/* Policy ID for kernel drivers */
	policy_ctrl[MKP_POLICY_SELINUX_STATE] = 1;	/* Policy ID for selinux_state */
	policy_ctrl[MKP_POLICY_SELINUX_AVC] = 1;	/* Policy ID for selinux avc */
	policy_ctrl[MKP_POLICY_TASK_CRED] = 1;		/* Policy ID for task credential */
	policy_ctrl[MKP_POLICY_KERNEL_CODE] = 1;	/* Policy ID for kernel text */
	policy_ctrl[MKP_POLICY_KERNEL_RODATA] = 1;	/* Policy ID for kernel rodata */
	policy_ctrl[MKP_POLICY_KERNEL_PAGES] = 1;	/* Policy ID for other mapped kernel pages */
	policy_ctrl[MKP_POLICY_PGTABLE] = 1;		/* Policy ID for page table */
	policy_ctrl[MKP_POLICY_S1_MMU_CTRL] = 1;	/* Policy ID for stage-1 MMU control */
	policy_ctrl[MKP_POLICY_FILTER_SMC_HVC] = 1;	/* Policy ID for HVC/SMC call filtering */

	/* Policies for vendors start from here (16 ~ 31) */
	policy_ctrl[MKP_POLICY_VENDOR_START] = 1;
	MKP_INFO("MKP: set policy Done\n");
	return;
}

int __init set_ext_policy(uint32_t policy)
{
	if ((int)policy < 0 || policy >= MKP_POLICY_NR) {
		MKP_WARN("policy:%d is Out-of-bound\n", (int)policy);
		return -1;
	} else if (policy <= MKP_POLICY_DEFAULT_MAX) {
		MKP_WARN("policy:%u is duplicated\n", policy);
		return -1;
	}

	policy_ctrl[policy] = 1;
	return 0;
}

void handle_mkp_err_action(uint32_t policy)
{
	if (mkp_policy_action[policy] & ACTION_PANIC) {
		MKP_ERR("mkp error\n");
		// For now we use warn for panic.
		//WARN("1, mkp panic\n");
	} else if (mkp_policy_action[policy] & ACTION_WARNING)
		MKP_WARN("mkp warning\n");
	else
		MKP_INFO("mkp error\n");
}
