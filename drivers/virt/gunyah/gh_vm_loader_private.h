/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#ifndef _GH_VM_LOADER_PRIVATE_H
#define _GH_VM_LOADER_PRIVATE_H

#include <linux/gunyah/gh_vm_loader.h>

struct gh_vm_struct {
	u16 type;
	struct gh_vm_loader_info *loader_info;
	void *loader_data;
	struct gh_vm_loader_name_map *name_map;
	struct notifier_block rm_nb;
	struct mutex vm_lock;
	bool vm_created;
};

const char *gh_vm_loader_get_name(struct gh_vm_struct *vm_struct);
enum gh_vm_names gh_vm_loader_get_name_val(struct gh_vm_struct *vm_struct);
void gh_vm_loader_set_loader_data(struct gh_vm_struct *vm_struct, void *data);
void *gh_vm_loader_get_loader_data(struct gh_vm_struct *vm_struct);
void
gh_vm_loader_notify_clients(struct gh_vm_struct *vm_struct, unsigned long val);
void gh_vm_loader_destroy_vm(struct gh_vm_struct *vm_struct);

struct gh_vm_loader_info {
	const struct file_operations *vm_fops;
	int (*gh_vm_loader_init)(void);
	void (*gh_vm_loader_exit)(void);
	int (*gh_vm_loader_vm_init)(struct gh_vm_struct *vm_struct);
	void (*gh_vm_loader_rm_notifier)(struct gh_vm_struct *vm_struct,
					unsigned long cmd, void *data);
};

extern struct gh_vm_loader_info gh_vm_sec_loader_info;

#endif /* _GH_VM_LOADER_PRIVATE_H */
