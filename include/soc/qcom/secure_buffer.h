/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015-2019, The Linux Foundation. All rights reserved.
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
	VMID_CP_CDSP = 0x2A,
	VMID_NAV = 0x2B,
	VMID_LAST,
	VMID_INVAL = -1
};

#define PERM_READ                       0x4
#define PERM_WRITE                      0x2
#define PERM_EXEC			0x1

struct dest_vm_and_perm_info {
	u32 vm;
	u32 perm;
	u64 ctx;
	u32 ctx_size;
};

struct mem_prot_info {
	phys_addr_t addr;
	u64 size;
};

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
bool msm_secure_v2_is_supported(void);
const char *msm_secure_vmid_to_string(int secure_vmid);
u32 msm_secure_get_vmid_perms(u32 vmid);

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

static inline bool msm_secure_v2_is_supported(void)
{
	return false;
}

static inline const char *msm_secure_vmid_to_string(int secure_vmid)
{
	return "N/A";
}

static inline u32 msm_secure_get_vmid_perms(u32 vmid)
{
	return 0;
}

#endif
#endif
