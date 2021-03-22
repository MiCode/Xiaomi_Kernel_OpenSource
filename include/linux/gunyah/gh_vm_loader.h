/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#ifndef _GH_VM_LOADER_H
#define _GH_VM_LOADER_H

struct gh_vm_loader_notif {
	u16 loader;
	enum gh_vm_names name_val;
	const char *vm_name;
};

int gh_vm_loader_register_notifier(struct notifier_block *nb);
int gh_vm_loader_unregister_notifier(struct notifier_block *nb);

/* Secure VM loader notifications */
#define GH_VM_LOADER_SEC_BEFORE_POWERUP		0x1
#define GH_VM_LOADER_SEC_AFTER_POWERUP		0x2
#define GH_VM_LOADER_SEC_POWERUP_FAIL		0x3
#define GH_VM_LOADER_SEC_VM_CRASH_EARLY		0x4
#define GH_VM_LOADER_SEC_VM_CRASH			0x5
#define GH_VM_LOADER_SEC_BEFORE_SHUTDOWN	0x6
#define GH_VM_LOADER_SEC_AFTER_SHUTDOWN		0x7
#define GH_VM_LOADER_SEC_SHUTDOWN_FAIL		0x8

#endif /* _GH_VM_LOADER_H */
