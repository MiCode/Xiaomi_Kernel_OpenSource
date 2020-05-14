/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */

#include "msm_ion_priv.h"

#ifndef _ION_SECURE_UTIL_H
#define _ION_SECURE_UTIL_H

int get_secure_vmid(unsigned long flags);
bool is_secure_vmid_valid(int vmid);
int ion_hyp_assign_sg(struct sg_table *sgt, int *dest_vm_list,
		      int dest_nelems, bool set_page_private);
int ion_hyp_unassign_sg(struct sg_table *sgt, int *source_vm_list,
			int source_nelems, bool clear_page_private);
int ion_hyp_unassign_sg_from_flags(struct sg_table *sgt, unsigned long flags,
				   bool set_page_private);
int ion_hyp_assign_sg_from_flags(struct sg_table *sgt, unsigned long flags,
				 bool set_page_private);
int ion_hyp_assign_from_flags(u64 base, u64 size, unsigned long flags);

bool hlos_accessible_buffer(struct ion_buffer *buffer);

bool is_secure_allocation(unsigned long flags);

#endif /* _ION_SECURE_UTIL_H */
