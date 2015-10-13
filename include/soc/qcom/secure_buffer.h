/*
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
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

#ifndef __MSM_SECURE_BUFFER_H__
#define __MSM_SECURE_BUFFER_H__

#include <linux/scatterlist.h>

#define VMID_HLOS 0x3
#define VMID_CP_TOUCH 0x8
#define VMID_CP_BITSTREAM 0x9
#define VMID_CP_PIXEL 0xA
#define VMID_CP_NON_PIXEL 0xB
#define VMID_CP_CAMERA 0xD
#define VMID_HLOS_FREE 0xE
#define VMID_MSS_MSA 0xF
#define VMID_MSS_NONMSA 0x10
#define VMID_CP_SEC_DISPLAY 0x11
#define VMID_CP_APP 0x12
#define VMID_INVAL -1
/*
 * if you add a secure VMID here make sure you update
 * msm_secure_vmid_to_string
 */

#define PERM_READ                       0x4
#define PERM_WRITE                      0x2
#define PERM_EXEC			0x1

#ifdef CONFIG_MSM_SECURE_BUFFER
int msm_secure_table(struct sg_table *table);
int msm_unsecure_table(struct sg_table *table);
int hyp_assign_table(struct sg_table *table,
			u32 *source_vm_list, int source_nelems,
			int *dest_vmids, int *dest_perms,
			int dest_nelems);
int hyp_assign_phys(phys_addr_t addr, u64 size,
			u32 *source_vmlist, int source_nelems,
			int *dest_vmids, int *dest_perms, int dest_nelems);
bool msm_secure_v2_is_supported(void);
const char *msm_secure_vmid_to_string(int secure_vmid);
#else
static inline int msm_secure_table(struct sg_table *table)
{
	return -ENOSYS;
}
static inline int msm_unsecure_table(struct sg_table *table)
{
	return -ENOSYS;
}
static inline int hyp_assign_table(struct sg_table *table,
			u32 *source_vm_list, int source_nelems,
			int *dest_vmids, int *dest_perms,
			int dest_nelems)
{
	return -ENOSYS;
}
static inline int hyp_assign_phys(phys_addr_t addr, u64 size,
			u32 *source_vmlist, int source_nelems,
			int *dest_vmids, int *dest_perms, int dest_nelems)
{
	return -ENOSYS;
}
static inline bool msm_secure_v2_is_supported(void)
{
	return false;
}
const char *msm_secure_vmid_to_string(int secure_vmid)
{
	return "N/A";
}
#endif
#endif
