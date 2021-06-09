// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt) "mem_buf_vm: " fmt

#include <linux/of.h>
#include <linux/xarray.h>
#include <soc/qcom/secure_buffer.h>
#include "mem-buf-dev.h"

#define DEVNAME "mem_buf_vm"
#define NUM_MEM_BUF_VM_MINORS 128

static dev_t mem_buf_vm_devt;
static struct class *mem_buf_vm_class;

/*
 * VM objects have the same lifetime as this module.
 */
static DEFINE_XARRAY_ALLOC(mem_buf_vm_minors);
static DEFINE_XARRAY(mem_buf_vms);
int current_vmid;

#define PERIPHERAL_VM(_uname, _lname)		\
static struct mem_buf_vm vm_ ## _lname = {	\
	.name = "qcom," #_lname,		\
	.vmid = VMID_ ## _uname,		\
	.gh_id = GH_VM_MAX,			\
	.allowed_api = MEM_BUF_API_HYP_ASSIGN,	\
}

PERIPHERAL_VM(CP_TOUCH, cp_touch);
PERIPHERAL_VM(CP_BITSTREAM, cp_bitstream);
PERIPHERAL_VM(CP_PIXEL, cp_pixel);
PERIPHERAL_VM(CP_NON_PIXEL, cp_non_pixel);
PERIPHERAL_VM(CP_CAMERA, cp_camera);
PERIPHERAL_VM(CP_SEC_DISPLAY, cp_sec_display);
PERIPHERAL_VM(CP_SPSS_SP, cp_spss_sp);
PERIPHERAL_VM(CP_CAMERA_PREVIEW, cp_camera_preview);
PERIPHERAL_VM(CP_SPSS_SP_SHARED, cp_spss_sp_shared);
PERIPHERAL_VM(CP_SPSS_HLOS_SHARED, cp_spss_hlos_shared);
PERIPHERAL_VM(CP_CDSP, cp_cdsp);
PERIPHERAL_VM(CP_APP, cp_app);

static struct mem_buf_vm vm_trusted_vm = {
	.name = "qcom,trusted_vm",
	/* Vmid via dynamic lookup */
	.gh_id = GH_TRUSTED_VM,
	.allowed_api = MEM_BUF_API_GUNYAH,
};

static struct mem_buf_vm vm_hlos = {
	.name = "qcom,hlos",
	.vmid = VMID_HLOS,
	.gh_id = GH_VM_MAX,
	.allowed_api = MEM_BUF_API_HYP_ASSIGN | MEM_BUF_API_GUNYAH,
};

struct mem_buf_vm *pdata_array[] = {
	&vm_trusted_vm,
	&vm_hlos,
	&vm_cp_touch,
	&vm_cp_bitstream,
	&vm_cp_pixel,
	&vm_cp_non_pixel,
	&vm_cp_camera,
	&vm_cp_sec_display,
	&vm_cp_spss_sp,
	&vm_cp_camera_preview,
	&vm_cp_spss_sp_shared,
	&vm_cp_spss_hlos_shared,
	&vm_cp_cdsp,
	&vm_cp_app,
	NULL,
};

/*
 * Opening this file acquires a refcount on vm->dev's kobject - see
 * chrdev_open(). So private data won't be free'd out from
 * under us.
 */
static int mem_buf_vm_open(struct inode *inode, struct file *file)
{
	struct mem_buf_vm *vm;

	vm = container_of(inode->i_cdev, struct mem_buf_vm, cdev);
	file->private_data = vm;
	return 0;
}

static const struct file_operations mem_buf_vm_fops = {
	.open = mem_buf_vm_open,
};

/*
 * This is more complicated because we need to deal with the case where
 * the vmid belongs to a cpu-based vm. And we don't get a notification
 * when a cpu-based vm is created/destroyed, so we can't add them to our
 * xarray.
 */
static struct mem_buf_vm *find_vm_by_vmid(int vmid)
{
	struct mem_buf_vm *vm;
	enum gh_vm_names vm_name;
	gh_vmid_t gh_vmid;
	unsigned long idx;
	int ret;

	vm = xa_load(&mem_buf_vms, vmid);
	if (vm)
		return vm;

	for (vm_name = GH_PRIMARY_VM; vm_name < GH_VM_MAX; vm_name++) {
		ret = gh_rm_get_vmid(vm_name, &gh_vmid);
		if (ret)
			return ERR_PTR(ret);

		if (gh_vmid == vmid)
			break;
	}

	if (vm_name == GH_VM_MAX)
		return ERR_PTR(-EINVAL);

	xa_for_each(&mem_buf_vm_minors, idx, vm) {
		if (vm->gh_id == vm_name)
			return vm;
	}
	WARN_ON(1);
	return ERR_PTR(-EINVAL);
}

int mem_buf_vm_get_backend_api(int *vmids, unsigned int nr_acl_entries)
{
	struct mem_buf_vm *vm;
	u32 allowed_api = U32_MAX;
	int i;

	for (i = 0; i < nr_acl_entries; i++) {
		vm = find_vm_by_vmid(vmids[i]);
		if (IS_ERR(vm)) {
			pr_err_ratelimited("No vm with vmid=0x%x\n", vmids[i]);
			return PTR_ERR(vm);
		}

		allowed_api &= vm->allowed_api;
	}

	if (!allowed_api) {
		pr_err_ratelimited("Vms have no common backend API\n");
		return -EINVAL;
	}

	/* Prefer hyp assign since it has fewer limitations */
	if (allowed_api & MEM_BUF_API_HYP_ASSIGN)
		return MEM_BUF_API_HYP_ASSIGN;
	else
		return MEM_BUF_API_GUNYAH;
}

int mem_buf_fd_to_vmid(int fd)
{
	int ret = -EINVAL;
	struct mem_buf_vm *vm;
	struct file *file;
	gh_vmid_t vmid;

	file = fget(fd);
	if (!file)
		return -EINVAL;

	if (file->f_op != &mem_buf_vm_fops) {
		pr_err_ratelimited("Invalid vm file type\n");
		fput(file);
		return -EINVAL;
	}

	vm = file->private_data;
	if (vm->gh_id == GH_VM_MAX) {
		fput(file);
		return vm->vmid;
	}

	ret = gh_rm_get_vmid(vm->gh_id, &vmid);
	if (ret)
		pr_err("gh_rm_get_vmid %d failed\n", vm->gh_id);
	fput(file);
	return ret ? ret : vmid;
}
EXPORT_SYMBOL(mem_buf_fd_to_vmid);

static void mem_buf_vm_device_release(struct device *dev)
{
	struct mem_buf_vm *vm;

	vm = container_of(dev, struct mem_buf_vm, dev);
	kfree(vm);
}

/*
 * caller must fill in all fields of new_vm except for cdev & dev.
 */
static int mem_buf_vm_add(struct mem_buf_vm *new_vm)
{
	struct mem_buf_vm *vm;
	struct device *dev;
	int minor, ret;
	unsigned long idx;

	xa_for_each(&mem_buf_vm_minors, idx, vm) {
		if (!strcmp(vm->name, new_vm->name)) {
			pr_err("duplicate vm %s\n", vm->name);
			ret = -EINVAL;
			goto err_duplicate;
		}
	}

	ret = xa_alloc(&mem_buf_vm_minors, &minor, new_vm,
		       XA_LIMIT(0, NUM_MEM_BUF_VM_MINORS - 1), GFP_KERNEL);
	if (ret < 0) {
		pr_err("no more minors\n");
		goto err_devt;
	}

	cdev_init(&new_vm->cdev, &mem_buf_vm_fops);
	dev = &new_vm->dev;
	device_initialize(dev);
	dev->devt = MKDEV(MAJOR(mem_buf_vm_devt), minor);
	dev->class = mem_buf_vm_class;
	dev->parent = NULL;
	dev->release = mem_buf_vm_device_release;
	dev_set_drvdata(dev, new_vm);
	dev_set_name(dev, "%s", new_vm->name);

	if (new_vm->gh_id == GH_VM_MAX) {
		ret = xa_err(xa_store(&mem_buf_vms, new_vm->vmid, new_vm, GFP_KERNEL));
		if (ret)
			goto err_xa_store;
	}

	ret = cdev_device_add(&new_vm->cdev, dev);
	if (ret) {
		pr_err("Adding cdev %s failed\n", new_vm->name);
		goto err_cdev_add;
	}
	return 0;

err_cdev_add:
	xa_erase(&mem_buf_vms, new_vm->vmid);
err_xa_store:
	xa_erase(&mem_buf_vm_minors, minor);
	put_device(dev);
err_devt:
err_duplicate:
	return ret;
}

static int mem_buf_vm_add_pdata(struct mem_buf_vm *pdata)
{
	struct mem_buf_vm *vm;
	int ret;

	vm = kmemdup(pdata, sizeof(*vm), GFP_KERNEL);
	if (!vm)
		return -ENOMEM;

	ret = mem_buf_vm_add(vm);
	if (ret) {
		kfree(vm);
		return ret;
	}
	return 0;
}

static int mem_buf_vm_add_self(void)
{
	struct mem_buf_vm *vm, *self;
	int ret;

	vm = find_vm_by_vmid(current_vmid);
	if (IS_ERR(vm))
		return PTR_ERR(vm);

	self = kzalloc(sizeof(*self), GFP_KERNEL);
	if (!self)
		return -ENOMEM;

	/* Create an aliased name */
	self->name = "qcom,self";
	self->vmid = vm->vmid;
	self->gh_id = vm->gh_id;
	self->allowed_api = vm->allowed_api;

	ret = mem_buf_vm_add(self);
	if (ret) {
		kfree(self);
		return ret;
	}
	return 0;
}

static char *mem_buf_vm_devnode(struct device *dev, umode_t *mode)
{
	return kasprintf(GFP_KERNEL, "mem_buf_vm/%s", dev_name(dev));
}

static int mem_buf_vm_put_class_device_cb(struct device *dev, void *data)
{
	struct mem_buf_vm *vm = container_of(dev, struct mem_buf_vm, dev);

	cdev_device_del(&vm->cdev, dev);
	return 0;
}

int mem_buf_vm_init(struct device *dev)
{
	struct mem_buf_vm **p;
	int ret, vmid;

	ret = of_property_read_u32(dev->of_node, "qcom,vmid", &vmid);
	if (ret) {
		dev_err(dev, "missing qcom,vmid property\n");
		return ret;
	}
	current_vmid = vmid;

	ret = alloc_chrdev_region(&mem_buf_vm_devt, 0, NUM_MEM_BUF_VM_MINORS,
				DEVNAME);
	if (ret)
		return ret;

	mem_buf_vm_class = class_create(THIS_MODULE, DEVNAME);
	if (IS_ERR(mem_buf_vm_class)) {
		ret = PTR_ERR(mem_buf_vm_class);
		goto err_class_create;
	}
	mem_buf_vm_class->devnode = mem_buf_vm_devnode;

	for (p = pdata_array; *p; p++) {
		ret = mem_buf_vm_add_pdata(*p);
		if (ret)
			goto err_pdata;
	}

	ret = mem_buf_vm_add_self();
	if (ret)
		goto err_self;

	return 0;

err_self:
err_pdata:
	xa_destroy(&mem_buf_vms);
	xa_destroy(&mem_buf_vm_minors);
	class_for_each_device(mem_buf_vm_class, NULL, NULL,
		mem_buf_vm_put_class_device_cb);
	class_destroy(mem_buf_vm_class);
err_class_create:
	unregister_chrdev_region(mem_buf_vm_devt, NUM_MEM_BUF_VM_MINORS);
	return ret;
}

void mem_buf_vm_exit(void)
{
	xa_destroy(&mem_buf_vms);
	xa_destroy(&mem_buf_vm_minors);
	class_for_each_device(mem_buf_vm_class, NULL, NULL,
		mem_buf_vm_put_class_device_cb);
	class_destroy(mem_buf_vm_class);
	unregister_chrdev_region(mem_buf_vm_devt, NUM_MEM_BUF_VM_MINORS);
}
