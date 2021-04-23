/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 *
 */

#ifndef __HH_COMMON_H
#define __HH_COMMON_H

#include <linux/types.h>

/* Common Haven types */
typedef u16 hh_vmid_t;
typedef u32 hh_rm_msgid_t;
typedef u32 hh_virq_handle_t;
typedef u32 hh_label_t;
typedef u32 hh_memparcel_handle_t;
typedef u64 hh_capid_t;
typedef u64 hh_dbl_flags_t;

struct hh_vminfo {
	u8 *guid;
	char *uri;
	char *name;
	char *sign_auth;
};

/* Common Haven macros */
#define HH_CAPID_INVAL	U64_MAX

enum hh_vm_names {
	/*
	 * HH_SELF_VM is an alias for VMID 0. Useful for RM APIs which allow
	 * operations on current VM such as console
	 */
	HH_SELF_VM,
	HH_PRIMARY_VM,
	HH_TRUSTED_VM,
	HH_CPUSYS_VM,
	HH_VM_MAX
};

#endif
