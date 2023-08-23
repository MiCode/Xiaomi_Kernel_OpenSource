/*
 * Copyright (c) 2015-2020, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __QCOM_SECURE_BUFFER_H__
#define __QCOM_SECURE_BUFFER_H__

#include <linux/scatterlist.h>

/*
 * if you add a secure VMID here make sure you update
 * msm_secure_vmid_to_string.
 * Make sure to keep the VMID_LAST as the last entry in the enum.
 * This is needed in ion to create a list and it's sized using VMID_LAST.
 */
enum vmid {
	VMID_HLOS = 0x3,
	VMID_CP_TOUCH = 0x8,
	VMID_CP_BITSTREAM = 0x9,
	VMID_CP_PIXEL = 0xA,
	VMID_CP_NON_PIXEL = 0xB,
	VMID_CP_CAMERA = 0xD,
	VMID_HLOS_FREE = 0xE,
	VMID_MSS_MSA = 0xF,
	VMID_MSS_NONMSA = 0x10,
	VMID_CP_SEC_DISPLAY = 0x11,
	VMID_CP_APP = 0x12,
	VMID_WLAN = 0x18,
	VMID_WLAN_CE = 0x19,
	VMID_CP_SPSS_SP = 0x1A,
	VMID_CP_CAMERA_PREVIEW = 0x1D,
	VMID_CP_SPSS_SP_SHARED = 0x22,
	VMID_CP_SPSS_HLOS_SHARED = 0x24,
	VMID_CP_CAMERA_ENCODE = 0x29,
	VMID_CP_CDSP = 0x2A,
	VMID_CP_DSP_EXT = 0x2E,
	VMID_NAV = 0x2B,
	VMID_LAST = 0x2F,
	VMID_INVAL = -1
};

#define PERM_READ                       0x4
#define PERM_WRITE                      0x2
#define PERM_EXEC			0x1

#ifdef CONFIG_QCOM_SECURE_BUFFER
int msm_secure_table(struct sg_table *table);
int msm_unsecure_table(struct sg_table *table);
int hyp_assign_table(struct sg_table *table,
			u32 *source_vm_list, int source_nelems,
			int *dest_vmids, int *dest_perms,
			int dest_nelems);

int try_hyp_assign_table(struct sg_table *table,
			 u32 *source_vm_list, int source_nelems,
			 int *dest_vmids, int *dest_perms,
			 int dest_nelems);

extern int hyp_assign_phys(phys_addr_t addr, u64 size,
			u32 *source_vmlist, int source_nelems,
			int *dest_vmids, int *dest_perms, int dest_nelems);


extern int cma_hyp_assign_phys(struct device *dev, u32 *source_vm_list,
				int source_nelems, int *dest_vmids,
					int *dest_perms, int dest_nelems);

bool msm_secure_v2_is_supported(void);
const char *msm_secure_vmid_to_string(int secure_vmid);
#else
static inline int msm_secure_table(struct sg_table *table)
{
	return -EINVAL;
}

static inline int msm_unsecure_table(struct sg_table *table)
{
	return -EINVAL;
}

static inline int hyp_assign_table(struct sg_table *table,
			u32 *source_vm_list, int source_nelems,
			int *dest_vmids, int *dest_perms,
			int dest_nelems)
{
	return -EINVAL;
}

static inline int try_hyp_assign_table(struct sg_table *table,
				       u32 *source_vm_list, int source_nelems,
				       int *dest_vmids, int *dest_perms,
				       int dest_nelems)
{
	return -EINVAL;
}

static inline int hyp_assign_phys(phys_addr_t addr, u64 size,
			u32 *source_vmlist, int source_nelems,
			int *dest_vmids, int *dest_perms, int dest_nelems)
{
	return -EINVAL;
}

static inline int cma_hyp_assign_phys(struct device *dev, u32 *source_vm_list,
				int source_nelems, int *dest_vmids,
					int *dest_perms, int dest_nelems)
{
	return -EINVAL;
}

static inline bool msm_secure_v2_is_supported(void)
{
	return false;
}

static inline const char *msm_secure_vmid_to_string(int secure_vmid)
{
	return "N/A";
}
#endif
#endif
