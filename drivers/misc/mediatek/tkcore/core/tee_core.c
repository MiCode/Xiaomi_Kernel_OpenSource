/*
 * Copyright (c) 2015-2018 TrustKernel Incorporated
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/idr.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/uaccess.h>
#include <asm-generic/ioctl.h>
#include <linux/sched.h>
#include <linux/version.h>

#include "linux/tee_core.h"
#include "linux/tee_ioc.h"
#include <linux/tee_client_api.h>

#include "tee_core_priv.h"
#include "tee_sysfs.h"
#include "tee_shm.h"
#include "tee_supp_com.h"
#include "tee_tui.h"

#include "tee_ta_mgmt.h"
#include "tee_procfs.h"

#include "tee_fp_priv.h"
#include "tee_clkmgr_priv.h"

#include "pm.h"

static uint32_t nsdrv_feature_flags;

#if defined(CONFIG_ARM)

#include <asm/mach/map.h>
#include <linux/io.h>

void *__arm_ioremap(unsigned long phys_addr, size_t size, unsigned int mtype)
{
	phys_addr_t last_addr;
	unsigned long offset = phys_addr & ~PAGE_MASK;
	unsigned long pfn = __phys_to_pfn(phys_addr);

	last_addr = phys_addr + size - 1;
	if (!size || last_addr < phys_addr)
		return NULL;

	return __arm_ioremap_pfn(pfn, offset, size, mtype);
}

#endif
void *tee_map_cached_shm(unsigned long pa, size_t len)
{
#if defined(CONFIG_ARM64)
	return ioremap_cache(pa, len);
#elif defined(CONFIG_ARM)
	return __arm_ioremap(pa, len, MT_MEMORY_RW);
#else
#error "tee_map_cached_shm() not implemented for this platform"
#endif
}
EXPORT_SYMBOL(tee_map_cached_shm);

void tee_unmap_cached_shm(void *va)
{
	iounmap(va);
}
EXPORT_SYMBOL(tee_unmap_cached_shm);

#define _TEE_CORE_FW_VER "1:0.1"

static char *_tee_supp_app_name = "teed";

/* Store the class misc reference */
static struct class *misc_class;

static int device_match(struct device *device, const void *devname)
{
	int ret;
	struct tee *tee = dev_get_drvdata(device);

	WARN_ON(!tee);

	/*
	 * It shall always return
	 * 0 if tee is a null
	 * ptr
	 */
	if (tee == NULL)
		return 0;

	ret = strncmp(devname,
		tee->name, sizeof(tee->name));
	return ret == 0;
}

/*
 * For the kernel api.
 * Get a reference on a device tee from the device needed
 */
struct tee *tee_get_tee(const char *devname)
{
	struct device *device;

	if (!devname)
		return NULL;
	device = class_find_device(misc_class, NULL,
		(void *) devname, device_match);
	if (!device) {
		pr_err("can't find device [%s]\n",
			devname);
		return NULL;
	}

	return dev_get_drvdata(device);
}

void tee_inc_stats(struct tee_stats_entry *entry)
{
	entry->count++;
	if (entry->count > entry->max)
		entry->max = entry->count;
}

void tee_dec_stats(struct tee_stats_entry *entry)
{
	entry->count--;
}

int __tee_get(struct tee *tee)
{
	int ret = 0;
	int v;

	WARN_ON(!tee);

	v = atomic_inc_return(&tee->refcount);
	if (v == 1) {
		WARN_ON(!try_module_get(tee->ops->owner));
		get_device(tee->dev);

		if (tee->ops->start)
			ret = tee->ops->start(tee);

		if (ret) {
			pr_err("%s::start() failed, err=0x%x\n",
				tee->name, ret);
			put_device(tee->dev);
			module_put(tee->ops->owner);
			atomic_dec(&tee->refcount);
		}
	} else {
		dev_warn(_DEV(tee), "Unexpected tee->refcount: 0x%x\n", v);
		return -1;
	}

	return ret;
}
EXPORT_SYMBOL(__tee_get);


/**
 * tee_get - increases refcount of the tee
 * @tee:	[in]	tee to increase refcount of
 *
 * @note: If tee.ops.start() callback function is available,
 * it is called when refcount is equal at 1.
 */
int tee_get(struct tee *tee)
{
	int ret = 0;

	WARN_ON(!tee);

	if (atomic_inc_return(&tee->refcount) == 1) {
		pr_warn("unexpected refcount 1\n");
	} else {
		int count = (int) atomic_read(&tee->refcount);

		if (count > tee->max_refcount)
			tee->max_refcount = count;
	}
	return ret;
}

/**
 * tee_put - decreases refcount of the tee
 * @tee:	[in]	tee to reduce refcount of
 *
 * @note: If tee.ops.stop() callback function is available,
 * it is called when refcount is equal at 0.
 */
int tee_put(struct tee *tee)
{
	int ret = 0;
	int count;

	WARN_ON(!tee);

	if (atomic_dec_and_test(&tee->refcount)) {
		pr_warn("unexpected refcount: 0\n");
		/*
		 * tee should never be stopped
		 */
	}

	count = (int)atomic_read(&tee->refcount);
	return ret;
}

static int tee_supp_open(struct tee *tee)
{
	int ret = 0;

	WARN_ON(!tee->rpc);

	if (strncmp(_tee_supp_app_name, current->comm,
			strlen(_tee_supp_app_name)) == 0) {
		if (atomic_add_return(1, &tee->rpc->used) > 1) {
			ret = -EBUSY;
			pr_err("Only one teed is allowed\n");
			atomic_sub(1, &tee->rpc->used);
		}
	}

	return ret;
}

static void tee_supp_release(struct tee *tee)
{
	WARN_ON(!tee->rpc);

	if ((atomic_read(&tee->rpc->used) == 1) &&
			(strncmp(_tee_supp_app_name, current->comm,
					strlen(_tee_supp_app_name)) == 0))
		atomic_sub(1, &tee->rpc->used);
}

static int tee_ctx_open(struct inode *inode, struct file *filp)
{
	struct tee_context *ctx;
	struct tee *tee;
	int ret;

	tee = container_of(filp->private_data, struct tee, miscdev);

	WARN_ON(!tee);
	WARN_ON(tee->miscdev.minor != iminor(inode));

	ret = tee_supp_open(tee);
	if (ret)
		return ret;

	ctx = tee_context_create(tee);
	if (IS_ERR_OR_NULL(ctx))
		return PTR_ERR(ctx);

	ctx->usr_client = 1;
	filp->private_data = ctx;

	return 0;
}

static int tee_ctx_release(struct inode *inode, struct file *filp)
{
	struct tee_context *ctx = filp->private_data;
	struct tee *tee;

	if (!ctx)
		return -EINVAL;

	WARN_ON(!ctx->tee);
	tee = ctx->tee;
	WARN_ON(tee->miscdev.minor != iminor(inode));

	tee_context_destroy(ctx);
	tee_supp_release(tee);

	return 0;
}

static int tee_do_create_session(struct tee_context *ctx,
				 struct tee_cmd_io __user *u_cmd)
{
	int ret = -EINVAL;
	struct tee_cmd_io k_cmd;
	struct tee *tee;

	tee = ctx->tee;
	WARN_ON(!ctx->usr_client);


	if (copy_from_user(&k_cmd, (void *)u_cmd, sizeof(struct tee_cmd_io))) {
		pr_err("create_session: copy_from_user failed\n");
		goto exit;
	}

	if (k_cmd.fd_sess > 0) {
		pr_err("invalid fd_sess %d\n", k_cmd.fd_sess);
		goto exit;
	}

	if ((k_cmd.op == NULL) || (k_cmd.uuid == NULL) ||
		((k_cmd.data != NULL) && (k_cmd.data_size == 0)) ||
		((k_cmd.data == NULL) && (k_cmd.data_size != 0))) {
		pr_err("op or/and data parameters are not valid\n");
		goto exit;
	}

	ret = tee_session_create_fd(ctx, &k_cmd);
	put_user(k_cmd.err, &u_cmd->err);
	put_user(k_cmd.origin, &u_cmd->origin);
	if (ret)
		goto exit;

	put_user(k_cmd.fd_sess, &u_cmd->fd_sess);

exit:
	return ret;
}

static int tee_do_shm_alloc_perm(struct tee_context *ctx,
	struct tee_shm_io __user *u_shm)
{
	int ret = -EINVAL;
	struct tee_shm_io k_shm;
	struct tee *tee = ctx->tee;

	(void) tee;

	if (copy_from_user(&k_shm, (void *)u_shm, sizeof(struct tee_shm_io))) {
		pr_err("shm_alloc_perm: copy_from_user failed\n");
		goto exit;
	}

	ret = tee_shm_alloc_io_perm(ctx, &k_shm);
	if (ret)
		goto exit;

	put_user(k_shm.paddr, &u_shm->paddr);
	put_user(k_shm.fd_shm, &u_shm->fd_shm);
	put_user(k_shm.flags, &u_shm->flags);

exit:
	return ret;
}

static int tee_do_shm_alloc(struct tee_context *ctx,
	struct tee_shm_io __user *u_shm)
{
	int ret = -EINVAL;
	struct tee_shm_io k_shm;
	struct tee *tee = ctx->tee;

	WARN_ON(!ctx->usr_client);


	if (copy_from_user(&k_shm, (void *)u_shm, sizeof(struct tee_shm_io))) {
		pr_err("copy_from_user failed\n");
		goto exit;
	}

	if ((k_shm.buffer != NULL) || (k_shm.fd_shm != 0) ||
		((k_shm.flags & tee->shm_flags) == 0) ||
		(k_shm.registered != 0)) {
		pr_err(
			"shm parameters are not valid %p %d %08x %08x %d\n",
			(void *) k_shm.buffer,
			k_shm.fd_shm,
			(unsigned int) k_shm.flags,
			(unsigned int) tee->shm_flags,
			k_shm.registered);
		goto exit;
	}

	ret = tee_shm_alloc_io(ctx, &k_shm);
	if (ret)
		goto exit;

	put_user(k_shm.fd_shm, &u_shm->fd_shm);
	put_user(k_shm.flags, &u_shm->flags);

exit:
	return ret;
}

static int tee_do_get_fd_for_rpc_shm(struct tee_context *ctx,
	struct tee_shm_io __user *u_shm)
{
	int ret = -EINVAL;
	struct tee_shm_io k_shm;
	struct tee *tee = ctx->tee;


	WARN_ON(!ctx->usr_client);

	if (copy_from_user(&k_shm, (void *)u_shm, sizeof(struct tee_shm_io))) {
		pr_err("copy_from_user failed\n");
		goto exit;
	}

	if (k_shm.registered != 0) {
		pr_err("expecting shm to be unregistered\n");
		goto exit;
	}

	if ((k_shm.buffer == NULL) || (k_shm.size == 0) ||
		(k_shm.fd_shm != 0)) {
		pr_err("Invalid shm param. buffer: %p size: %u fd: %d\n",
			k_shm.buffer, k_shm.size, k_shm.fd_shm);
		goto exit;
	}

	if ((k_shm.flags & ~(tee->shm_flags)) ||
		((k_shm.flags & tee->shm_flags) == 0)) {
		pr_err(
			"Invalid shm flags: 0x%x expecting to be within 0x%x\n",
			k_shm.flags, tee->shm_flags);
		goto exit;
	}

	ret = tee_shm_fd_for_rpc(ctx, &k_shm);
	if (ret)
		goto exit;

	put_user(k_shm.fd_shm, &u_shm->fd_shm);

exit:

	return ret;
}

static int tee_tui_notify(uint32_t arg)
{
	if (teec_notify_event(arg))
		return 0;

	return -EINVAL;
}

static int tee_tui_wait(uint32_t __user *u_arg)
{
	int r;
	uint32_t cmd_id;

	r = teec_wait_cmd(&cmd_id);
	if (r)
		return r;

	if (copy_to_user(u_arg, &cmd_id, sizeof(cmd_id)))
		return -EFAULT;

	return 0;
}

static long tee_internal_ioctl(struct tee_context *ctx,
				unsigned int cmd,
				void __user *u_arg)
{
	int ret = -EINVAL;

	switch (cmd) {
	case TEE_OPEN_SESSION_IOC:
		ret = tee_do_create_session(ctx,
			(struct tee_cmd_io __user *) u_arg);
		break;

	case TEE_ALLOC_SHM_PERM_IOC:
		ret = tee_do_shm_alloc_perm(ctx,
			(struct tee_shm_io __user *) u_arg);
		break;

	case TEE_ALLOC_SHM_IOC:
		ret = tee_do_shm_alloc(ctx,
			(struct tee_shm_io __user *) u_arg);
		break;

	case TEE_GET_FD_FOR_RPC_SHM_IOC:
		ret = tee_do_get_fd_for_rpc_shm(ctx,
			(struct tee_shm_io __user *) u_arg);
		break;

	case TEE_TUI_NOTIFY_IOC:
		ret = tee_tui_notify(
			(uint32_t) (unsigned long) u_arg);
		break;

	case TEE_TUI_WAITCMD_IOC:
		ret = tee_tui_wait(
			(uint32_t __user *) u_arg);
		break;

	case TEE_INSTALL_TA_IOC:
		ret = tee_install_sp_ta(ctx, u_arg);
		break;

	case TEE_INSTALL_TA_RESP_IOC:
		ret = tee_install_sp_ta_response(
			ctx, u_arg);
		break;

	case TEE_DELETE_TA_IOC:
		ret = tee_delete_sp_ta(ctx, u_arg);
		break;

	case TEE_QUERY_DRV_FEATURE_IOC:
		if (u_arg) {
			pr_info("tkcoredrv: nsdrv feature = 0x%x\n",
					nsdrv_feature_flags);
			if (copy_to_user(u_arg, &nsdrv_feature_flags,
					sizeof(nsdrv_feature_flags))) {
				ret = -EFAULT;
			}
		} else
			ret = -EINVAL;

		break;

	default:
		pr_err("internal_ioctl: Unknown command: %u\n", cmd);
		ret = -EINVAL;
	}

	return ret;
}

#ifdef CONFIG_COMPAT

static int convert_compat_tee_shm(struct TEEC_SharedMemory __user *shm)
{
	if (unlikely(put_user(0, ((uint32_t __user *) &(shm->buffer)) + 1)))
		return -EFAULT;

	if (unlikely(put_user(0, ((uint32_t __user *) &(shm->size)) + 1)))
		return -EFAULT;

	if (unlikely(put_user(0, ((uint32_t __user *) &(shm->d.fd)) + 1)))
		return -EFAULT;

	return 0;
}

static int convert_compat_tee_param(union TEEC_Parameter __user *p,
	uint32_t type)
{
	struct TEEC_SharedMemory __user *p_shm;

	switch (type) {
	case TEEC_MEMREF_TEMP_INPUT:
	case TEEC_MEMREF_TEMP_OUTPUT:
	case TEEC_MEMREF_TEMP_INOUT:

		if (unlikely(put_user(0,
				((uint32_t __user *) &(p->tmpref.buffer)) + 1)))
			return -EFAULT;

		if (unlikely(put_user(0,
				((uint32_t __user *) &(p->tmpref.size)) + 1)))
			return -EFAULT;

		break;

	case TEEC_MEMREF_PARTIAL_INPUT:
	case TEEC_MEMREF_PARTIAL_OUTPUT:
	case TEEC_MEMREF_PARTIAL_INOUT:
	case TEEC_MEMREF_WHOLE:

		if (unlikely(put_user(0,
				((uint32_t __user *) &(p->memref.parent)) + 1)))
			return -EFAULT;

		if (unlikely(put_user(0,
				((uint32_t __user *) &(p->memref.size)) + 1)))
			return -EFAULT;

		if (unlikely(put_user(0,
				((uint32_t __user *) &(p->memref.offset)) + 1)))
			return -EFAULT;

		if ((copy_from_user(&p_shm, &p->memref.parent,
				sizeof(p_shm))))
			return -EFAULT;

		if (p_shm == NULL)
			break;

		if (convert_compat_tee_shm(p_shm))
			return -EFAULT;

		break;

	default:
		break;
	}

	return 0;
}

int convert_compat_tee_cmd(struct tee_cmd_io __user *u_cmd)
{
	uint32_t i;
	struct TEEC_Operation __user *p_op;
	uint32_t paramTypes;

	if (u_cmd == NULL)
		return -EINVAL;

	if (unlikely(put_user(0, ((uint32_t __user *) &u_cmd->uuid) + 1)))
		return -EFAULT;

	if (unlikely(put_user(0, ((uint32_t __user *) &u_cmd->data) + 1)))
		return -EFAULT;

	if (unlikely(put_user(0, ((uint32_t __user *) &u_cmd->op) + 1)))
		return -EFAULT;

	if (copy_from_user(&p_op, &u_cmd->op, sizeof(p_op)))
		return -EFAULT;

	if (p_op == NULL)
		return -EINVAL;

	if (copy_from_user(&paramTypes, (uint32_t __user *) &p_op->paramTypes,
			sizeof(p_op->paramTypes)))
		return -EFAULT;

	if (unlikely(put_user(0, ((uint32_t __user *) &p_op->session) + 1)))
		return -EFAULT;

	for (i = 0; i < TEEC_CONFIG_PAYLOAD_REF_COUNT; i++) {
		if (convert_compat_tee_param(&p_op->params[i],
			TEEC_PARAM_TYPE_GET(paramTypes, i))) {
			pr_err("bad param %u\n", i);
			return -EFAULT;
		}
	}

	return 0;
}

int convert_compat_tee_shm_io(struct tee_shm_io __user *shm_io)
{
	if (shm_io == NULL)
		return -EINVAL;

	if (unlikely(put_user(0, ((uint32_t __user *) &shm_io->buffer) + 1)))
		return -EFAULT;

	return 0;
}

int convert_compat_tee_spta_inst(struct tee_spta_inst_desc __user *spta)
{
	if (spta == NULL)
		return -EINVAL;

	if (unlikely(put_user(0,
		((uint32_t __user *) &spta->ta_binary) + 1)))
		return -EFAULT;

	if (unlikely(put_user(0,
		((uint32_t __user *) &spta->response_len) + 1)))
		return -EFAULT;

	return 0;
}

static long tee_compat_ioctl(struct file *filp, unsigned int cmd,
			unsigned long arg)
{
	struct tee_context *ctx = filp->private_data;
	void __user *u_arg;

	WARN_ON(!ctx);
	WARN_ON(!ctx->tee);

	if (is_compat_task())
		u_arg = compat_ptr(arg);
	else
		u_arg = (void __user *)arg;

	switch (cmd) {
	case TEE_OPEN_SESSION_IOC:
		if (convert_compat_tee_cmd((struct tee_cmd_io __user *) u_arg))
			return -EFAULT;
		break;

	case TEE_ALLOC_SHM_PERM_IOC:
	case TEE_ALLOC_SHM_IOC:
	case TEE_GET_FD_FOR_RPC_SHM_IOC:
		if (convert_compat_tee_shm_io(
				(struct tee_shm_io __user *) u_arg))
			return -EFAULT;
		break;

	case TEE_INSTALL_TA_IOC:
		if (convert_compat_tee_spta_inst(
			(struct tee_spta_inst_desc __user *) u_arg))
			return -EFAULT;
		break;

	default:
		break;
	}

	return tee_internal_ioctl(ctx, cmd, u_arg);
}
#endif

static long tee_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct tee_context *ctx = filp->private_data;

	WARN_ON(!ctx);
	WARN_ON(!ctx->tee);

	return tee_internal_ioctl(ctx, cmd, (void __user *) arg);
}

const struct file_operations tee_fops = {
	.owner = THIS_MODULE,
	.read = tee_supp_read,
	.write = tee_supp_write,
	.open = tee_ctx_open,
	.release = tee_ctx_release,
#ifdef CONFIG_COMPAT
	.compat_ioctl = tee_compat_ioctl,
#endif
	.unlocked_ioctl = tee_ioctl
};

static void tee_plt_device_release(struct device *dev)
{
	(void) dev;
}

static spinlock_t tee_idr_lock;
static struct idr tee_idr;

/* let caller to guarantee tee
 * and id are not NULL. using lock to protect
 */
int tee_core_alloc_uuid(void *ptr)
{
	int r;

	idr_preload(GFP_KERNEL);

	spin_lock(&tee_idr_lock);
	r = idr_alloc(&tee_idr, ptr, 1, 0, GFP_NOWAIT);
	if (r < 0)
		pr_err("Bad alloc tee_uuid. rv: %d\n",
			r);
	spin_unlock(&tee_idr_lock);

	idr_preload_end();

	return r;
}

void *tee_core_uuid2ptr(int id)
{
	return idr_find(&tee_idr, id);
}

/* let caller to guarantee tee
 *and id are not NULL
 */
void tee_core_free_uuid(int id)
{
	idr_remove(&tee_idr, id);
}

struct tee *tee_core_alloc(struct device *dev, char *name, int id,
	const struct tee_ops *ops, size_t len)
{
	struct tee *tee;

	if (!dev || !name || !ops ||
		!ops->open || !ops->close || !ops->alloc || !ops->free)
		return NULL;

	tee = devm_kzalloc(dev, sizeof(struct tee) + len, GFP_KERNEL);
	if (!tee) {
		tee = NULL;
		pr_err("core_alloc: kzalloc failed\n");
		return tee;
	}

	if (!dev->release)
		dev->release = tee_plt_device_release;

	tee->dev = dev;
	tee->id = id;
	tee->ops = ops;
	tee->priv = &tee[1];

	snprintf(tee->name, sizeof(tee->name), "%s", name);
	pr_info("TEE core: Alloc the misc device \"%s\" (id=%d)\n",
		tee->name, tee->id);

	tee->miscdev.parent = dev;
	tee->miscdev.minor = MISC_DYNAMIC_MINOR;
	tee->miscdev.name = tee->name;
	tee->miscdev.fops = &tee_fops;

	mutex_init(&tee->lock);
	atomic_set(&tee->refcount, 0);
	INIT_LIST_HEAD(&tee->list_ctx);
	INIT_LIST_HEAD(&tee->list_rpc_shm);

	tee->state = TEE_OFFLINE;
	tee->shm_flags = TEEC_MEM_INPUT | TEEC_MEM_OUTPUT | TEEC_MEM_NONSECURE;
	tee->test = 0;

	if ((tee_supp_init(tee))) {
		devm_kfree(dev, tee);
		return NULL;
	}

	return tee;
}
EXPORT_SYMBOL(tee_core_alloc);

int tee_core_free(struct tee *tee)
{
	if (tee) {
		tee_supp_deinit(tee);
		devm_kfree(tee->dev, tee);
	}
	return 0;
}
EXPORT_SYMBOL(tee_core_free);

int tee_core_add(struct tee *tee)
{
	int rc = 0;

	if (!tee)
		return -EINVAL;

	rc = misc_register(&tee->miscdev);
	if (rc != 0) {
		pr_err("misc_register() failed with ret = %d\n",
			rc);
		return rc;
	}

	dev_set_drvdata(tee->miscdev.this_device, tee);

	rc = tee_init_procfs(tee);
	if (rc) {
		misc_deregister(&tee->miscdev);
		return rc;
	}

	rc = tee_init_sysfs(tee);
	if (rc) {
		misc_deregister(&tee->miscdev);
		return rc;
	}

	/* Register a static reference on the class misc
	 * to allow finding device by class
	 */
	WARN_ON(!tee->miscdev.this_device->class);

	if (misc_class)
		WARN_ON(misc_class != tee->miscdev.this_device->class);
	else
		misc_class = tee->miscdev.this_device->class;

	pr_info(
		"TKCore misc: Register the misc device \"%s\" (id=%d,minor=%d)\n",
		dev_name(tee->miscdev.this_device), tee->id,
		tee->miscdev.minor);

	return rc;
}
EXPORT_SYMBOL(tee_core_add);

int tee_core_del(struct tee *tee)
{
	if (tee) {
		pr_info(
			"TEE Core: Destroy the misc device \"%s\" (id=%d)\n",
			dev_name(tee->miscdev.this_device), tee->id);

		tee_cleanup_sysfs(tee);

		if (tee->miscdev.minor != MISC_DYNAMIC_MINOR) {
			pr_info(
				"TEE Core: Deregister the misc device \"%s\" (id=%d)\n",
				dev_name(tee->miscdev.this_device), tee->id);
			misc_deregister(&tee->miscdev);
		}
	}

	tee_core_free(tee);

	return 0;
}
EXPORT_SYMBOL(tee_core_del);

static int __init tee_core_init(void)
{
	int r;

	pr_info("\nTEE Core Framework initialization (ver %s)\n",
		_TEE_CORE_FW_VER);

	r = tkcore_tee_pm_init();
	if (r) {
		pr_err("tkcore_tee_pm_init() failed with %d\n", r);
		return r;
	}

	spin_lock_init(&tee_idr_lock);
	idr_init(&tee_idr);

	tee_fp_init();
	tee_clkmgr_init();
	tee_ta_mgmt_init();

	return 0;
}

static void __exit tee_core_exit(void)
{
	pr_info("TEE Core Framework unregistered\n");

	tkcore_tee_pm_exit();

	tee_clkmgr_exit();
	tee_fp_exit();
}

#ifndef MODULE
rootfs_initcall(tee_core_init);
#else
module_init(tee_core_init);
#endif
module_exit(tee_core_exit);

MODULE_AUTHOR("TrustKernel");
MODULE_DESCRIPTION("TrustKernel TKCore TEEC v1.0");
MODULE_SUPPORTED_DEVICE("");
MODULE_VERSION(_TEE_CORE_FW_VER);
MODULE_LICENSE("GPL");
