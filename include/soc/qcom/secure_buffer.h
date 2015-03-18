/*
 * Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
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


#ifdef CONFIG_QCOM_SECURE_BUFFER
int msm_secure_table(struct sg_table *table);
int msm_unsecure_table(struct sg_table *table);
int msm_ion_hyp_assign_call(struct sg_table *table,
		u32 *source_vm_list, u32 source_list_size,
		u32 *dest_vm_list, u32 dest_list_size);
bool msm_secure_v2_is_supported(void);

#else
static inline int msm_secure_table(struct sg_table *table)
{
	return -EINVAL;
}
static inline int msm_unsecure_table(struct sg_table *table)
{
	return -EINVAL;
}
static inline int hyp_assign_call(struct sg_table *table,
		u32 *source_vm_list, u32 source_list_size,
		u32 *dest_vm_list, u32 dest_list_size);
{
	return -EINVAL;
}
static inline bool msm_secure_v2_is_supported(void)
{
	return false;
}
#endif
#endif
