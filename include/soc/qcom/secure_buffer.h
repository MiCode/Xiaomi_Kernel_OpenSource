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


#define VMID_HLOS 0x3
#define VMID_CP_TOUCH 0x8
#define VMID_CP_BITSTREAM 0x9
#define VMID_CP_PIXEL 0xA
#define VMID_CP_NON_PIXEL 0xB
#define VMID_CP_CAMERA 0xD
#define VMID_HLOS_FREE 0xE
#define VMID_MSS_NONMSA 0x10
#define VMID_INVAL -1

#define PERM_READ                       0x4
#define PERM_WRITE                      0x2

static inline int msm_secure_table(struct sg_table *table)
{
	return -EINVAL;
}
static inline int msm_unsecure_table(struct sg_table *table)
{
	return -EINVAL;
}
int hyp_assign_table(struct sg_table *table,
			u32 *source_vm_list, int source_nelems,
			int *dest_vmids, int *dest_perms,
			int dest_nelems)
{
	return -EINVAL;
}
int hyp_assign_phys(phys_addr_t addr, u64 size,
			int *dest_vmids, int *dest_perms,
			int dest_nelems)
{
	return -EINVAL;
}
static inline bool msm_secure_v2_is_supported(void)
{
	return false;
}
#endif
