/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/unistd.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/errno.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/proc_fs.h>

#include "secmem.h"

/* only available for trustonic */
#include "mobicore_driver_api.h"
#include "tlsecmem_api.h"

#define DEFAULT_HANDLES_NUM (64)
#define MAX_OPEN_SESSIONS   (0xffffffff - 1)

#undef pr_fmt
#define pr_fmt(fmt) "[" KBUILD_MODNAME "] %s:%d: " fmt, __func__, __LINE__

struct secmem_handle {
#ifdef SECMEM_64BIT_PHYS_SUPPORT
	u64 id;
#else
	u32 id;
#endif
	u32 type;
};

struct secmem_context {
	spinlock_t lock;
	struct secmem_handle *handles;
	u32 handle_num;
};

static DEFINE_MUTEX(secmem_lock);
static const struct mc_uuid_t secmem_uuid = { TL_SECMEM_UUID };
static struct mc_session_handle secmem_session = { 0 };

static u32 secmem_session_ref;
static u32 secmem_devid = MC_DEVICE_ID_DEFAULT;
static struct tciMessage_t *secmem_tci;

#if defined(CONFIG_CMA) && defined(CONFIG_MTK_SSMR)
#define SECMEM_RECLAIM_DELAY 1000 /* ms */

static u32 secmem_region_ref;
static u32 is_region_ready;

static DEFINE_MUTEX(secmem_region_lock);

#ifdef SECMEM_64BIT_PHYS_SUPPORT
static int secmem_enable(u64 addr, u64 size);
#else
static int secmem_enable(u32 addr, u32 size);
#endif
static int secmem_disable(void);
static int secmem_region_release(void);
static int secmem_session_close(void);

static struct workqueue_struct *secmem_reclaim_wq;
static void secmem_reclaim_handler(struct work_struct *work);
static DECLARE_DELAYED_WORK(secmem_reclaim_work, secmem_reclaim_handler);

static void secmem_reclaim_handler(struct work_struct *work)
{
	mutex_lock(&secmem_region_lock);
	pr_debug("triggered!!\n");
	secmem_region_release();
	mutex_unlock(&secmem_region_lock);
}
#endif /* defined(CONFIG_CMA) && defined(CONFIG_MTK_SSMR) */

static int secmem_execute(u32 cmd, struct secmem_param *param)
{
	enum mc_result mc_ret;
#ifdef SECMEM_DEBUG_DUMP
	int len;
#endif

	mutex_lock(&secmem_lock);

	if (secmem_tci == NULL) {
		mutex_unlock(&secmem_lock);
		pr_err("secmem_tci not exist\n");
		return -ENODEV;
	}

	secmem_tci->cmd_secmem.header.commandId = (tciCommandId_t) cmd;
	secmem_tci->cmd_secmem.len = 0;
	secmem_tci->sec_handle = param->sec_handle;
	secmem_tci->alignment = param->alignment;
	secmem_tci->size = param->size;
	secmem_tci->refcount = param->refcount;
#ifdef SECMEM_DEBUG_DUMP
	secmem_tci->sender.id = param->id;
	secmem_tci->sender.name[0] = 0;
	if (param->owner[0] != 0) {
		len = param->owner_len > MAX_NAME_SZ ?
			MAX_NAME_SZ : param->owner_len;
		memcpy(secmem_tci->sender.name, param->owner, len);
		secmem_tci->sender.name[MAX_NAME_SZ - 1] = 0;
	}
#endif

	mc_ret = mc_notify(&secmem_session);

	if (mc_ret != MC_DRV_OK) {
		pr_err("mc_notify failed: %d\n", mc_ret);
		goto exit;
	}

	mc_ret = mc_wait_notification(&secmem_session, -1);

	if (mc_ret != MC_DRV_OK) {
		pr_err("mc_wait_notification failed: 0x%x\n", mc_ret);
		goto exit;
	}

	/* correct handle should be get after return from secure world. */
	param->sec_handle = secmem_tci->sec_handle;
	param->refcount = secmem_tci->refcount;
	param->alignment = secmem_tci->alignment;
	param->size = secmem_tci->size;

	if (RSP_ID(cmd) != secmem_tci->rsp_secmem.header.responseId) {
		pr_err("trustlet did not send a response: 0x%x\n",
			secmem_tci->rsp_secmem.header.responseId);
		mc_ret = MC_DRV_ERR_INVALID_RESPONSE;
		goto exit;
	}

	if (secmem_tci->rsp_secmem.header.returnCode != MC_DRV_OK) {
		pr_err("trustlet did not send a valid return code: 0x%x\n",
			secmem_tci->rsp_secmem.header.returnCode);
		mc_ret = secmem_tci->rsp_secmem.header.returnCode;
	}

exit:

	mutex_unlock(&secmem_lock);

	if (mc_ret != MC_DRV_OK)
		return -ENOSPC;

	return 0;
}

#ifdef SECMEM_64BIT_PHYS_SUPPORT
static int secmem_handle_register(struct secmem_context *ctx, u32 type, u64 id)
#else
static int secmem_handle_register(struct secmem_context *ctx, u32 type, u32 id)
#endif
{
	struct secmem_handle *handle;
	u32 i, num, nspace;

	spin_lock(&ctx->lock);

	num = ctx->handle_num;
	handle = ctx->handles;

	/* find empty space. */
	for (i = 0; i < num; i++, handle++) {
		if (handle->id == 0) {
			handle->id = id;
			handle->type = type;
			spin_unlock(&ctx->lock);
#if defined(CONFIG_CMA) && defined(CONFIG_MTK_SSMR)
			mutex_lock(&secmem_region_lock);
			secmem_region_ref++;
			mutex_unlock(&secmem_region_lock);
#endif
			return 0;
		}
	}

	/* try grow the space */
	nspace = num * 2;
	handle = (struct secmem_handle *)krealloc(ctx->handles,
					nspace * sizeof(struct secmem_handle),
					GFP_KERNEL);
	if (handle == NULL) {
		spin_unlock(&ctx->lock);
		return -ENOMEM;
	}
	ctx->handle_num = nspace;
	ctx->handles = handle;

	handle += num;

	memset(handle, 0, (nspace - num) * sizeof(struct secmem_handle));

	handle->id = id;
	handle->type = type;

	spin_unlock(&ctx->lock);

	return 0;
}

#ifdef SECMEM_64BIT_PHYS_SUPPORT
static void secmem_handle_unregister_check(struct secmem_context *ctx,
					   u32 type, u64 id)
#else
static void secmem_handle_unregister_check(struct secmem_context *ctx,
					   u32 type, u32 id)
#endif
{
	struct secmem_handle *handle;
	u32 i, num;

	spin_lock(&ctx->lock);

	num = ctx->handle_num;
	handle = ctx->handles;

	/* find empty space. */
	for (i = 0; i < num; i++, handle++) {
		if (handle->id == id) {
			if (handle->type != type) {
#ifdef SECMEM_64BIT_PHYS_SUPPORT
				pr_debug(
					"unref check result: type mismatched (%d!=%d), handle=0x%llx\n",
					_IOC_NR(handle->type), _IOC_NR(type),
					handle->id);
#else
				pr_debug(
					"unref check result: type mismatched (%d!=%d), handle=0x%x\n",
					_IOC_NR(handle->type), _IOC_NR(type),
					handle->id);
#endif
			}
			break;
		}
	}

	spin_unlock(&ctx->lock);
}

#ifdef SECMEM_64BIT_PHYS_SUPPORT
static int secmem_handle_unregister(struct secmem_context *ctx, u64 id)
#else
static int secmem_handle_unregister(struct secmem_context *ctx, u32 id)
#endif
{
	struct secmem_handle *handle;
	u32 i, num;

	spin_lock(&ctx->lock);

	num = ctx->handle_num;
	handle = ctx->handles;

	/* find the match handle */
	for (i = 0; i < num; i++, handle++) {
		if (handle->id == id) {
			memset(handle, 0, sizeof(struct secmem_handle));
			break;
		}
	}

	spin_unlock(&ctx->lock);

#if defined(CONFIG_CMA) && defined(CONFIG_MTK_SSMR)
	/* found a match */
	if (i != num) {
		mutex_lock(&secmem_region_lock);
		secmem_region_ref--;
		mutex_unlock(&secmem_region_lock);
	}
#endif

	return 0;
}

static int secmem_handle_cleanup(struct secmem_context *ctx)
{
	int ret = 0;
	u32 i, num, cmd = 0;
	struct secmem_handle *handle;
	struct secmem_param param = { 0 };

	spin_lock(&ctx->lock);

	num = ctx->handle_num;
	handle = ctx->handles;

	for (i = 0; i < num; i++, handle++) {
		if (handle->id != 0) {
			param.sec_handle = handle->id;
			switch (handle->type) {
			case SECMEM_MEM_ALLOC:
				cmd = CMD_SEC_MEM_UNREF;
				break;
			case SECMEM_MEM_REF:
				cmd = CMD_SEC_MEM_UNREF;
				break;
			case SECMEM_MEM_ALLOC_TBL:
				cmd = CMD_SEC_MEM_UNREF_TBL;
				break;
			default:
				pr_err("secmem_handle_cleanup: incorrect type=%d (ioctl:%d)\n",
					handle->type, _IOC_NR(handle->type));
				goto error;
			}
			spin_unlock(&ctx->lock);
			ret = secmem_execute(cmd, &param);
#ifdef SECMEM_64BIT_PHYS_SUPPORT
			pr_debug("secmem_handle_cleanup: id=0x%llx type=%d (ioctl:%d)\n",
				handle->id, handle->type,
				_IOC_NR(handle->type));
#else
			pr_debug("secmem_handle_cleanup: id=0x%x type=%d (ioctl:%d)\n",
				handle->id, handle->type,
				_IOC_NR(handle->type));
#endif
			spin_lock(&ctx->lock);
		}
	}

error:
	spin_unlock(&ctx->lock);

	return ret;
}

static int secmem_session_open(void)
{
	enum mc_result mc_ret = MC_DRV_OK;

	mutex_lock(&secmem_lock);

	do {
		/* sessions reach max numbers ? */
		if (secmem_session_ref > MAX_OPEN_SESSIONS) {
			pr_err("secmem_session > 0x%x\n", MAX_OPEN_SESSIONS);
			break;
		}

		if (secmem_session_ref > 0) {
			secmem_session_ref++;
			break;
		}

		/* open device */
		mc_ret = mc_open_device(secmem_devid);
		if (mc_ret != MC_DRV_OK) {
			pr_err("mc_open_device failed: %d\n", mc_ret);
			break;
		}

		/* allocating WSM for DCI */
		mc_ret = mc_malloc_wsm(secmem_devid, 0,
				       sizeof(struct tciMessage_t),
				       (uint8_t **) &secmem_tci, 0);
		if (mc_ret != MC_DRV_OK) {
			mc_close_device(secmem_devid);
			pr_err("mc_malloc_wsm failed: %d\n", mc_ret);
			break;
		}

		/* open session */
		secmem_session.device_id = secmem_devid;
		mc_ret = mc_open_session(&secmem_session, &secmem_uuid,
					 (uint8_t *) secmem_tci,
					 sizeof(struct tciMessage_t));

		if (mc_ret != MC_DRV_OK) {
			mc_free_wsm(secmem_devid, (uint8_t *) secmem_tci);
			mc_close_device(secmem_devid);
			secmem_tci = NULL;
			pr_err("mc_open_session failed: %d\n", mc_ret);
			break;
		}
		secmem_session_ref = 1;

	} while (0);

	pr_debug("secmem_session_open: ret=%d, ref=%d\n", mc_ret,
		 secmem_session_ref);

	mutex_unlock(&secmem_lock);

	if (mc_ret != MC_DRV_OK)
		return -ENXIO;

	return 0;
}

static int secmem_session_close(void)
{
	enum mc_result mc_ret = MC_DRV_OK;

	mutex_lock(&secmem_lock);

	do {
		/* session is already closed ? */
		if (secmem_session_ref == 0) {
			pr_debug("secmem_session already closed\n");
			break;
		}

		if (secmem_session_ref > 1) {
			secmem_session_ref--;
			break;
		}

		/* close session */
		mc_ret = mc_close_session(&secmem_session);
		if (mc_ret != MC_DRV_OK) {
			pr_err("mc_close_session failed: %d\n", mc_ret);
			break;
		}

		/* free WSM for DCI */
		mc_ret = mc_free_wsm(secmem_devid, (uint8_t *) secmem_tci);
		if (mc_ret != MC_DRV_OK) {
			pr_err("mc_free_wsm failed: %d\n", mc_ret);
			break;
		}
		secmem_tci = NULL;
		secmem_session_ref = 0;

		/* close device */
		mc_ret = mc_close_device(secmem_devid);
		if (mc_ret != MC_DRV_OK)
			pr_err("mc_close_device failed: %d\n", mc_ret);

	} while (0);

	pr_debug("secmem_session_close: ret=%d, ref=%d\n", mc_ret,
		 secmem_session_ref);

	mutex_unlock(&secmem_lock);

	if (mc_ret != MC_DRV_OK)
		return -ENXIO;

	return 0;

}

static int secmem_open(struct inode *inode, struct file *file)
{
	struct secmem_context *ctx;

	/* allocate session context */
	ctx = kmalloc(sizeof(struct secmem_context), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->handle_num = DEFAULT_HANDLES_NUM;
	ctx->handles = kzalloc(
			sizeof(struct secmem_handle) * DEFAULT_HANDLES_NUM,
			GFP_KERNEL);
	spin_lock_init(&ctx->lock);

	if (!ctx->handles) {
		kfree(ctx);
		return -ENOMEM;
	}

	/* open session */
	if (secmem_session_open() < 0) {
		kfree(ctx->handles);
		kfree(ctx);
		return -ENXIO;
	}

	file->private_data = (void *)ctx;

	return 0;
}

static int secmem_release(struct inode *inode, struct file *file)
{
	int ret = 0;
	struct secmem_context *ctx =
		(struct secmem_context *)file->private_data;

	if (ctx) {
		/* release session context */
		secmem_handle_cleanup(ctx);
		kfree(ctx->handles);
		kfree(ctx);
		file->private_data = NULL;

		/* close session */
		ret = secmem_session_close();
	}
	return ret;
}

#if defined(CONFIG_CMA) && defined(CONFIG_MTK_SSMR)
static int secmem_region_alloc(void)
{
	int ret;
	phys_addr_t pa = 0;
	unsigned long size = 0;

	/* already online */
	if (is_region_ready) {
		pr_debug("%s: secure memory already online\n", __func__);
		return 0;
	}

	/* allocate secure memory region */
#ifdef SECMEM_64BIT_PHYS_SUPPORT
	ret = secmem_region_offline64(&pa, &size);
#else
	ret = secmem_region_offline(&pa, &size);
#endif
	if (ret) {
		pr_err("%s: secmem_region_offline failed! ret=%d\n", __func__,
		       ret);
		return -1;
	}

	if (pa == 0 || size == 0) {
		pr_err("%s: invalid pa(0x%llx) or size(0x%lx)\n", __func__, pa,
		       size);
		return -1;
	}

	/* setup secure memory and enable protection */
	ret = secmem_enable(pa, size);
	if (ret) {
		/* free secure memory */
		secmem_region_online();
		pr_err("%s: secmem_enable failed! ret=%d\n", __func__, ret);
		return -1;
	}

	is_region_ready = 1;
	secmem_region_ref = 0;

#if defined(CONFIG_MTK_SVP_DISABLE_SODI)
	spm_enable_sodi(false);
#endif

	pr_debug("phyaddr=0x%llx sz=0x%lx region_online=%u region_ref=%u\n",
			pa, size, is_region_ready, secmem_region_ref);

	return 0;
}

static int secmem_region_release(void)
{
	int ret;

	/* already offline */
	if (is_region_ready == 0) {
		pr_debug("%s: secure memory already offline\n", __func__);
		return 0;
	}

	/* region has reference so abort the release */
	if (secmem_region_ref > 0) {
		pr_err("%s: aborted due to secmem_region_ref != 0 (%d)\n",
			__func__, secmem_region_ref);
		return -1;
	}

	/* disable protection and recalim secure memory */
	ret = secmem_disable();
	if (ret) {
		pr_err("%s: secmem_disable failed! ret=%d\n", __func__, ret);
		return -1;
	}

	ret = secmem_region_online();
	if (ret) {
		pr_err("%s: secmem_region_online failed! ret=%d\n",
			__func__, ret);
		return -1;
	}

	is_region_ready = 0;

#if defined(CONFIG_MTK_SVP_DISABLE_SODI)
	spm_enable_sodi(true);
#endif

	pr_debug("%s: done, region_online=%u\n", __func__,
		 is_region_ready);

	return 0;
}
#endif

static long secmem_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int err = 0;
	struct secmem_context *ctx = (struct secmem_context *)
				     file->private_data;
	struct secmem_param param;
#ifdef SECMEM_64BIT_PHYS_SUPPORT
	u64 handle;
#else
	u32 handle;
#endif

	if (_IOC_TYPE(cmd) != SECMEM_IOC_MAGIC)
		return -ENOTTY;
	if (_IOC_NR(cmd) > SECMEM_IOC_MAXNR)
		return -ENOTTY;

	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, (void __user *)arg,
				 _IOC_SIZE(cmd));
	if (_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ, (void __user *)arg,
				 _IOC_SIZE(cmd));

	if (err)
		return -EFAULT;

	err = copy_from_user(&param, (void *)arg, sizeof(param));

	if (err)
		return -EFAULT;

	switch (cmd) {
	case SECMEM_MEM_ALLOC:
		if (!(file->f_mode & FMODE_WRITE))
			return -EROFS;
#if defined(CONFIG_CMA) && defined(CONFIG_MTK_SSMR)
		cancel_delayed_work_sync(&secmem_reclaim_work);
		mutex_lock(&secmem_region_lock);
		if (!is_region_ready) {
			err = secmem_region_alloc();
			if (err) {
				mutex_unlock(&secmem_region_lock);
				break;
			}
		}
		mutex_unlock(&secmem_region_lock);
#endif
		err = secmem_execute(CMD_SEC_MEM_ALLOC, &param);
		if (!err)
			secmem_handle_register(ctx, SECMEM_MEM_ALLOC,
					       param.sec_handle);
		break;
	case SECMEM_MEM_REF:
		err = secmem_execute(CMD_SEC_MEM_REF, &param);
		if (!err)
			secmem_handle_register(ctx, SECMEM_MEM_REF,
					       param.sec_handle);
		break;
	case SECMEM_MEM_UNREF:
		handle = param.sec_handle;
		secmem_handle_unregister_check(ctx, SECMEM_MEM_ALLOC, handle);
		err = secmem_execute(CMD_SEC_MEM_UNREF, &param);
		if (!err)
			secmem_handle_unregister(ctx, handle);
		break;
	case SECMEM_MEM_ALLOC_TBL:
		if (!(file->f_mode & FMODE_WRITE))
			return -EROFS;
#if defined(CONFIG_CMA) && defined(CONFIG_MTK_SSMR)
		cancel_delayed_work_sync(&secmem_reclaim_work);
		mutex_lock(&secmem_region_lock);
		if (!is_region_ready) {
			err = secmem_region_alloc();
			if (err) {
				mutex_unlock(&secmem_region_lock);
				break;
			}
		}
		pr_debug("region_online=%u region_ref=%u\n",
				is_region_ready, secmem_region_ref);
		mutex_unlock(&secmem_region_lock);
#endif
		err = secmem_execute(CMD_SEC_MEM_ALLOC_TBL, &param);
		if (!err)
			secmem_handle_register(ctx, SECMEM_MEM_ALLOC_TBL,
					       param.sec_handle);
		break;
	case SECMEM_MEM_UNREF_TBL:
		handle = param.sec_handle;
		secmem_handle_unregister_check(ctx, SECMEM_MEM_ALLOC_TBL,
					       handle);
		err = secmem_execute(CMD_SEC_MEM_UNREF_TBL, &param);
		if (!err)
			secmem_handle_unregister(ctx, handle);
		break;
	case SECMEM_MEM_USAGE_DUMP:
		if (!(file->f_mode & FMODE_WRITE))
			return -EROFS;
		err = secmem_execute(CMD_SEC_MEM_USAGE_DUMP, &param);
		break;
#ifdef SECMEM_DEBUG_DUMP
	case SECMEM_MEM_DUMP_INFO:
		if (!(file->f_mode & FMODE_WRITE))
			return -EROFS;
		err = secmem_execute(CMD_SEC_MEM_DUMP_INFO, &param);
		break;
#endif
	default:
		return -ENOTTY;
	}

#if defined(CONFIG_CMA) && defined(CONFIG_MTK_SSMR)
	mutex_lock(&secmem_region_lock);
	if (is_region_ready == 1 && secmem_region_ref == 0) {
		pr_debug("queue secmem_reclaim_work!!\n");
		queue_delayed_work(secmem_reclaim_wq, &secmem_reclaim_work,
			msecs_to_jiffies(SECMEM_RECLAIM_DELAY));
	} else {
		pr_debug("cmd=%u region_online=%u region_ref=%u!!\n",
				_IOC_NR(cmd), is_region_ready,
				secmem_region_ref);
	}
	mutex_unlock(&secmem_region_lock);
#endif

	if (!err)
		err = copy_to_user((void *)arg, &param, sizeof(param));

	return err;
}

#if defined(CONFIG_CMA) && defined(CONFIG_MTK_SSMR)
#ifdef SECMEM_64BIT_PHYS_SUPPORT
static int secmem_enable(u64 addr, u64 size)
#else
static int secmem_enable(u32 addr, u32 size)
#endif
{
	int err = 0;
	struct secmem_param param = { 0 };

	if (secmem_session_open() < 0) {
		err = -ENXIO;
		goto end;
	}

	param.sec_handle = addr;
	param.size = size;
	err = secmem_execute(CMD_SEC_MEM_ENABLE, &param);

	secmem_session_close();

end:
	pr_debug("%s ret = %d\n", __func__, err);

	return err;
}

static int secmem_disable(void)
{
	int err = 0;
	struct secmem_param param = { 0 };

	if (secmem_session_open() < 0) {
		err = -ENXIO;
		goto end;
	}

	err = secmem_execute(CMD_SEC_MEM_DISABLE, &param);

	secmem_session_close();

end:
	pr_debug("%s ret = %d\n", __func__, err);

	return err;
}

#ifdef SECMEM_64BIT_PHYS_SUPPORT
int secmem_api_query(u64 *allocate_size)
#else
int secmem_api_query(u32 *allocate_size)
#endif
{
	int err = 0;
	struct secmem_param param = { 0 };

	if (secmem_session_open() < 0) {
		err = -ENXIO;
		goto end;
	}

	err = secmem_execute(CMD_SEC_MEM_ALLOCATED, &param);
	if (err) {
		*allocate_size = -1;
	} else {
		*allocate_size = param.size;
#ifdef CONFIG_MTK_ENG_BUILD
		if (*allocate_size)
			secmem_execute(CMD_SEC_MEM_DUMP_INFO, &param);
#endif
	}

	secmem_session_close();

end:
	pr_debug("%s ret = %d\n", __func__, err);

	return err;
}
EXPORT_SYMBOL(secmem_api_query);
#endif

#ifdef SECMEM_KERNEL_API
static int secmem_api_alloc_internal(u32 alignment, u32 size, u32 *refcount,
	u32 *sec_handle, uint8_t *owner, uint32_t id, uint32_t clean)
{
	int ret = 0;
	struct secmem_param param;
	u32 cmd = clean ? CMD_SEC_MEM_ALLOC_ZERO : CMD_SEC_MEM_ALLOC;

#if defined(CONFIG_CMA) && defined(CONFIG_MTK_SSMR)
	cancel_delayed_work_sync(&secmem_reclaim_work);
	mutex_lock(&secmem_region_lock);
	if (!is_region_ready)
		ret = secmem_region_alloc();

	if (is_region_ready)
		secmem_region_ref++;

	pr_debug("region_online=%u region_ref=%u\n",
			is_region_ready, secmem_region_ref);
	mutex_unlock(&secmem_region_lock);
	if (ret != 0)
		goto end;
#endif

	if (secmem_session_open() < 0) {
		ret = -ENXIO;
		goto end;
	}

	memset(&param, 0, sizeof(param));
	param.alignment = alignment;
	param.size = size;
	param.refcount = 0;
	param.sec_handle = 0;
	param.id = id;
	if (owner) {
		param.owner_len = strlen(owner) > MAX_NAME_SZ ?
					MAX_NAME_SZ : strlen(owner);
		memcpy(param.owner, owner, param.owner_len);
		param.owner[MAX_NAME_SZ - 1] = 0;
	}

	ret = secmem_execute(cmd, &param);

	secmem_session_close();

end:

	if (ret == 0) {
		*refcount = param.refcount;
		*sec_handle = param.sec_handle;
	} else {
#if defined(CONFIG_CMA) && defined(CONFIG_MTK_SSMR)
		mutex_lock(&secmem_region_lock);
		/*
		 * Decrease region_ref when session_open() and
		 * execute() failed.
		 */
		if (is_region_ready)
			secmem_region_ref--;
		mutex_unlock(&secmem_region_lock);
#endif
	}

#if defined(CONFIG_CMA) && defined(CONFIG_MTK_SSMR)
	pr_debug("align=0x%x size=0x%x id=0x%x clean=%d ret=%d refcnt=0x%x shndl=0x%x region_online=%u region_ref=%u\n",
		alignment, size, id, clean, ret, *refcount, *sec_handle,
		is_region_ready, secmem_region_ref);
#else
	pr_debug("%s: align: 0x%x, size 0x%x, id 0x%x, clean(%d), ret(%d), refcnt 0x%x, sec_handle 0x%x\n",
		__func__, alignment, size, id, clean, ret, *refcount,
		*sec_handle);
#endif

	return ret;
}

int secmem_api_alloc(u32 alignment, u32 size, u32 *refcount, u32 *sec_handle,
		     uint8_t *owner, uint32_t id)
{
	return secmem_api_alloc_internal(alignment, size, refcount, sec_handle,
					 owner, id, 0);
}
EXPORT_SYMBOL(secmem_api_alloc);

int secmem_api_alloc_zero(u32 alignment, u32 size, u32 *refcount,
			  u32 *sec_handle, uint8_t *owner, uint32_t id)
{
	return secmem_api_alloc_internal(alignment, size, refcount, sec_handle,
					 owner, id, 1);
}
EXPORT_SYMBOL(secmem_api_alloc_zero);

int secmem_api_unref(u32 sec_handle, uint8_t *owner, uint32_t id)
{
	int ret = 0;
	struct secmem_param param;

	if (secmem_session_open() < 0) {
		ret = -ENXIO;
		goto end;
	}

	memset(&param, 0, sizeof(param));
	param.sec_handle = sec_handle;
	param.id = id;
	if (owner) {
		param.owner_len = strlen(owner) > MAX_NAME_SZ ?
					MAX_NAME_SZ : strlen(owner);
		memcpy(param.owner, owner, param.owner_len);
		param.owner[MAX_NAME_SZ - 1] = 0;
	}

	ret = secmem_execute(CMD_SEC_MEM_UNREF, &param);

	secmem_session_close();

end:

#if defined(CONFIG_CMA) && defined(CONFIG_MTK_SSMR)
	if (ret == 0) {
		mutex_lock(&secmem_region_lock);
		if (is_region_ready == 1 && --secmem_region_ref == 0) {
			pr_debug("queue secmem_reclaim_work!!\n");
			queue_delayed_work(secmem_reclaim_wq,
				   &secmem_reclaim_work,
				   msecs_to_jiffies(SECMEM_RECLAIM_DELAY));
		} else {
			pr_debug("region_online=%u region_ref=%u\n",
			is_region_ready, secmem_region_ref);
		}
		mutex_unlock(&secmem_region_lock);
	}
#endif

#if defined(CONFIG_CMA) && defined(CONFIG_MTK_SSMR)
	pr_debug("ret=%d shndl=0x%x owner=%p id=0x%x region_online=%u region_ref=%u\n",
		ret, sec_handle, owner, id, is_region_ready,
		secmem_region_ref);
#else
	pr_debug("%s: ret %d, sec_handle 0x%x, owner %p, id 0x%x\n",
		__func__, ret, sec_handle, owner, id);
#endif

	return ret;
}
EXPORT_SYMBOL(secmem_api_unref);
#endif /* END OF SECMEM_KERNEL_API */

#ifdef CONFIG_MTK_ENG_BUILD
#include <mach/emi_mpu.h>
#include <mt-plat/mtk_secure_api.h>
static ssize_t secmem_write(struct file *file, const char __user *buffer,
			    size_t count, loff_t *data)
{
	char desc[32];
	int len = 0;
	char cmd[10];

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (sscanf(desc, "%1s", cmd) == 1) {
		if (!strcmp(cmd, "0")) {
			pr_info("[SECMEM] - test for secmem_region_release()\n");
#if defined(CONFIG_CMA) && defined(CONFIG_MTK_SSMR)
			mutex_lock(&secmem_region_lock);
			secmem_region_ref--;
			secmem_region_release();
			mutex_unlock(&secmem_region_lock);
#endif
		} else if (!strcmp(cmd, "1")) {
			pr_info("[SECMEM] - test for secmem_region_alloc()\n");
#if defined(CONFIG_CMA) && defined(CONFIG_MTK_SSMR)
			mutex_lock(&secmem_region_lock);
			secmem_region_alloc();
			secmem_region_ref++;
			mutex_unlock(&secmem_region_lock);
#endif
		} else if (!strcmp(cmd, "2")) {
#if defined(CONFIG_CMA) && defined(CONFIG_MTK_SSMR)
#ifdef SECMEM_64BIT_PHYS_SUPPORT
			u64 size = 0;
#else
			u32 size = 0;
#endif

			pr_info("[SECMEM] - test for secmem_api_query()\n");
			secmem_api_query(&size);
			pr_info("[SECMEM] - allocated : 0x%llx\n", (u64)size);
#endif
		} else if (!strcmp(cmd, "3")) {
#if defined(CONFIG_CMA) && defined(CONFIG_MTK_SSMR)
#ifdef SECMEM_64BIT_PHYS_SUPPORT
			u64 size = 0;
			u64 sec_handle = 0;
#else
			u32 size = 0;
			u32 sec_handle = 0;
#endif

			u32 refcount = 0;
			char owner[] = "secme_ut";

			pr_info("[SECMEM] - test for alloc-free\n");
#ifdef SECMEM_64BIT_PHYS_SUPPORT
			secmem_api_alloc(0x1000, 0x1000, (u32 *)&refcount,
					 (u32 *)&sec_handle, owner, 0);
#else
			secmem_api_alloc(0x1000, 0x1000, &refcount, &sec_handle,
					 owner, 0);
#endif
			secmem_api_query(&size);
			pr_info("[SECMEM] - after alloc : 0x%llx\n", (u64)size);
#ifdef SECMEM_64BIT_PHYS_SUPPORT
			secmem_api_unref((u32)sec_handle, owner, 0);
#else
			secmem_api_unref(sec_handle, owner, 0);
#endif
			secmem_api_query(&size);
			pr_info("[SECMEM] - after free : 0x%llx\n", (u64)size);
#endif
		} else if (!strcmp(cmd, "4")) {
#if defined(CONFIG_ARM_PSCI) || defined(CONFIG_MTK_PSCI)
			pr_info("[SECMEM] - test for command 2\n");
			tbase_trigger_aee_dump();
#endif
		}
	}

	return count;
}
#endif

static const struct file_operations secmem_fops = {
	.owner = THIS_MODULE,
	.open = secmem_open,
	.release = secmem_release,
	.unlocked_ioctl = secmem_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = secmem_ioctl,
#endif
#ifdef CONFIG_MTK_ENG_BUILD
	.write = secmem_write,
#else
	.write = NULL,
#endif
	.read = NULL,
};

static int __init secmem_init(void)
{
	proc_create("secmem0", 0664, NULL, &secmem_fops);

#if defined(CONFIG_CMA) && defined(CONFIG_MTK_SSMR)
	if (!secmem_reclaim_wq)
		secmem_reclaim_wq =
			create_singlethread_workqueue("secmem_reclaim");
#endif

#ifdef SECMEM_DEBUG_INTERFACE
	{
		unsigned int sec_mem_mpu_attr =
			SET_ACCESS_PERMISSON(FORBIDDEN, SEC_RW, SEC_RW,
					     FORBIDDEN);
		unsigned int set_mpu_ret = 0;

		set_mpu_ret =
			emi_mpu_set_region_protection(0xF6000000, 0xFFFFFFFF, 0,
						      sec_mem_mpu_attr);
		pr_debug(
			"[SECMEM] - test for set EMI MPU on region 0, ret:%d\n",
			set_mpu_ret);
	}
#endif

	return 0;
}
late_initcall(secmem_init);
