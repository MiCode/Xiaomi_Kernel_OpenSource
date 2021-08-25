// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/anon_inodes.h>
#include <linux/file.h>

#include <linux/gunyah/gh_common.h>
#include <linux/gunyah/gh_rm_drv.h>
#include <linux/gunyah/gh_vm.h>

#include "gh_vm_loader_private.h"

struct gh_vm_loader_name_map {
	enum gh_vm_names val;
	const char *str;
};

struct gh_vm_struct vm_struct_ptr[GH_VM_MAX];

SRCU_NOTIFIER_HEAD_STATIC(gh_vm_loader_notifier);

static struct gh_vm_loader_name_map gh_vm_loader_name_map[] = {
	{GH_PRIMARY_VM, "pvm"},
	{GH_TRUSTED_VM, "trustedvm"},
	{GH_CPUSYS_VM, "cpusys_vm"},
};

static struct gh_vm_loader_info *gh_vm_loader_info[] = {
	[GH_VM_TYPE_SEC] = &gh_vm_sec_loader_info,
};

int gh_vm_loader_register_notifier(struct notifier_block *nb)
{
	return srcu_notifier_chain_register(&gh_vm_loader_notifier, nb);
}
EXPORT_SYMBOL(gh_vm_loader_register_notifier);

int gh_vm_loader_unregister_notifier(struct notifier_block *nb)
{
	return srcu_notifier_chain_unregister(&gh_vm_loader_notifier, nb);
}
EXPORT_SYMBOL(gh_vm_loader_unregister_notifier);

void
gh_vm_loader_notify_clients(struct gh_vm_struct *vm_struct, unsigned long val)
{
	struct gh_vm_loader_notif loader_notif;
	struct gh_vm_loader_name_map *name_map;

	if (!vm_struct)
		return;

	name_map = vm_struct->name_map;

	loader_notif.loader = vm_struct->type;
	loader_notif.name_val = name_map->val;
	loader_notif.vm_name = name_map->str;

	srcu_notifier_call_chain(&gh_vm_loader_notifier, val, &loader_notif);
}

enum gh_vm_names gh_vm_loader_get_name_val(struct gh_vm_struct *vm_struct)
{
	if (!vm_struct)
		return -EINVAL;

	return vm_struct->name_map->val;
}

const char *gh_vm_loader_get_name(struct gh_vm_struct *vm_struct)
{
	if (!vm_struct)
		return NULL;

	return vm_struct->name_map->str;
}

void gh_vm_loader_set_loader_data(struct gh_vm_struct *vm_struct, void *data)
{
	if (vm_struct)
		vm_struct->loader_data = data;
}

void *gh_vm_loader_get_loader_data(struct gh_vm_struct *vm_struct)
{
	if (!vm_struct)
		return NULL;

	return vm_struct->loader_data;
}

static void gh_vm_loader_init_vm_struct(struct gh_vm_struct *vm_struct)
{
	if (!vm_struct)
		return;

	mutex_lock(&vm_struct->vm_lock);
	vm_struct->vm_created = false;
	vm_struct->type = GH_VM_TYPES_MAX;
	vm_struct->loader_info = NULL;
	vm_struct->name_map = NULL;
	mutex_unlock(&vm_struct->vm_lock);
}

static int gh_vm_loader_rm_notifer_fn(struct notifier_block *nb,
					unsigned long cmd, void *data)
{
	struct gh_vm_loader_info *loader_info;
	struct gh_vm_struct *vm_struct;

	vm_struct = container_of(nb, struct gh_vm_struct, rm_nb);

	loader_info = vm_struct->loader_info;
	if (loader_info->gh_vm_loader_rm_notifier)
		loader_info->gh_vm_loader_rm_notifier(vm_struct, cmd, data);

	return NOTIFY_DONE;
}

static inline
struct gh_vm_loader_name_map *gh_vm_loader_get_map(const char *str)
{
	int map;
	int n = ARRAY_SIZE(gh_vm_loader_name_map);

	for (map = 0; map < n; map++) {
		if (!strcmp(str, gh_vm_loader_name_map[map].str))
			return &gh_vm_loader_name_map[map];
	}

	return NULL;
}

void gh_vm_loader_destroy_vm(struct gh_vm_struct *vm_struct)
{
	if (!vm_struct)
		return;

	gh_rm_unregister_notifier(&vm_struct->rm_nb);
	gh_vm_loader_init_vm_struct(vm_struct);
}

static struct gh_vm_struct *gh_vm_loader_create_vm(unsigned long arg)
{
	struct gh_vm_loader_name_map *name_map;
	struct gh_vm_loader_info *loader_info;
	struct gh_vm_struct *vm_struct;
	struct gh_vm_create vm_create;
	int ret;

	if (copy_from_user(&vm_create, (void __user *)arg, sizeof(vm_create)))
		return ERR_PTR(-EFAULT);

	if (vm_create.type >= GH_VM_TYPES_MAX) {
		pr_err("Invalid type: %u\n", vm_create.type);
		return ERR_PTR(-EINVAL);
	}

	loader_info = gh_vm_loader_info[vm_create.type];
	if (!loader_info || !loader_info->vm_fops) {
		pr_err("Loader info not found for type: %u\n", vm_create.type);
		return ERR_PTR(-EIO);
	}

	vm_create.name[GH_VM_NAME_MAX - 1] = '\0';

	/* Currently, we only allow static VMID allocations */
	name_map = gh_vm_loader_get_map(vm_create.name);
	if (!name_map) {
		pr_err("No VM map found for the name: %s\n", vm_create.name);
		return ERR_PTR(-EINVAL);
	}

	vm_struct = &vm_struct_ptr[name_map->val];
	mutex_lock(&vm_struct->vm_lock);

	if (!vm_struct->vm_created) {
		vm_struct->vm_created = true;
	} else {
		pr_err("VM %s already created\n", vm_create.name);
		ret = -EBUSY;
		mutex_unlock(&vm_struct->vm_lock);
		return ERR_PTR(ret);
	}

	vm_struct->type = vm_create.type;
	vm_struct->loader_info = loader_info;
	vm_struct->name_map = name_map;

	vm_struct->rm_nb.notifier_call = gh_vm_loader_rm_notifer_fn;
	ret = gh_rm_register_notifier(&vm_struct->rm_nb);
	if (ret) {
		mutex_unlock(&vm_struct->vm_lock);
		goto err_free_vm_struct;
	}

	mutex_unlock(&vm_struct->vm_lock);
	return vm_struct;

err_free_vm_struct:
	gh_vm_loader_init_vm_struct(vm_struct);
	return ERR_PTR(ret);
}

static long gh_vm_loader_dev_create_vm(unsigned long arg)
{
	struct gh_vm_loader_info *loader_info;
	struct gh_vm_struct *vm_struct;
	struct file *file;
	int fd, err;

	vm_struct = gh_vm_loader_create_vm(arg);
	if (IS_ERR_OR_NULL(vm_struct))
		return PTR_ERR(vm_struct);

	loader_info = vm_struct->loader_info;

	fd = get_unused_fd_flags(O_CLOEXEC);
	if (fd < 0) {
		err = fd;
		goto err_destroy_vm;
	}

	file = anon_inode_getfile("gh_vm_loader-vm", loader_info->vm_fops,
				vm_struct, O_RDWR);
	if (IS_ERR(file)) {
		err = PTR_ERR(file);
		goto err_put_fd;
	}

	if (loader_info->gh_vm_loader_vm_init) {
		err = loader_info->gh_vm_loader_vm_init(vm_struct);
		if (err) {
			/* The error path would .release() the struct file *
			 * of the loader and hence, the vm_struct would have
			 * been freed already. Therefore set vm_struct to
			 * NULL to avoid double-free
			 */
			vm_struct = NULL;
			goto err_fput_file;
		}
	}

	fd_install(fd, file);

	return fd;

err_fput_file:
	fput(file);
err_put_fd:
	put_unused_fd(fd);
err_destroy_vm:
	gh_vm_loader_destroy_vm(vm_struct);
	return err;
}

static long gh_vm_loader_api_version(void __user *arg)
{
	struct gh_vm_api_version vm_api_version;

	vm_api_version.major = GH_VM_API_MAJOR_VERSION;
	vm_api_version.minor = GH_VM_API_MINOR_VERSION;
	if (copy_to_user(arg, &vm_api_version, sizeof(vm_api_version)))
		return -EFAULT;

	return 0;
}

static long gh_vm_loader_dev_ioctl(struct file *filp,
					unsigned int cmd, unsigned long arg)
{
	long ret = -EINVAL;

	switch (cmd) {
	case GH_VM_GET_API_VERSION:
		return gh_vm_loader_api_version((void __user *)arg);
	case GH_VM_CREATE:
		return gh_vm_loader_dev_create_vm(arg);
	default:
		pr_err("Invalid ioctl\n");
		break;
	};

	return ret;
}

static const struct file_operations gh_vm_loader_dev_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = gh_vm_loader_dev_ioctl,
	.llseek = noop_llseek,
};

static struct miscdevice gh_vm_loader_dev = {
	.name = "gunyah",
	.minor = MISC_DYNAMIC_MINOR,
	.fops = &gh_vm_loader_dev_fops,
};

static int __init gh_vm_loader_init_loaders(void)
{
	struct gh_vm_loader_info *loader_info;
	int i, ret;

	for (i = 0; i < GH_VM_TYPES_MAX; i++) {
		loader_info = gh_vm_loader_info[i];
		if (!loader_info)
			continue;

		if (loader_info->gh_vm_loader_init) {
			ret = loader_info->gh_vm_loader_init();
			if (ret)
				goto err_module_exit;
		}
	}

	return 0;

err_module_exit:
	for (i--; i >= 0; i--) {
		loader_info = gh_vm_loader_info[i];
		if (!loader_info)
			continue;

		if (loader_info->gh_vm_loader_exit)
			loader_info->gh_vm_loader_exit();
	}

	return ret;
}

static void gh_vm_loader_exit_loaders(void)
{
	struct gh_vm_loader_info *loader_info;
	int i;

	for (i = 0; i < GH_VM_TYPES_MAX; i++) {
		loader_info = gh_vm_loader_info[i];
		if (!loader_info)
			continue;

		if (loader_info->gh_vm_loader_exit)
			loader_info->gh_vm_loader_exit();
	}
}

static int __init gh_vm_loader_init(void)
{
	int ret, map;

	ret = gh_vm_loader_init_loaders();
	if (ret) {
		pr_err("Failed to initialize the loaders\n");
		return ret;
	}

	ret = misc_register(&gh_vm_loader_dev);
	if (ret)
		goto err_exit_loaders;

	for (map = 0; map < GH_VM_MAX; map++) {
		mutex_init(&vm_struct_ptr[map].vm_lock);
		gh_vm_loader_init_vm_struct(&vm_struct_ptr[map]);
	}

	return 0;

err_exit_loaders:
	gh_vm_loader_exit_loaders();
	return ret;
}

static void __exit gh_vm_loader_exit(void)
{
	misc_deregister(&gh_vm_loader_dev);
	gh_vm_loader_exit_loaders();
}

module_init(gh_vm_loader_init);
module_exit(gh_vm_loader_exit);

MODULE_LICENSE("GPL v2");

