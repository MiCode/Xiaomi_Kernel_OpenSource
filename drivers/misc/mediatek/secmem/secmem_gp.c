/*
 * Copyright (c) 2014 - 2016 MediaTek Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
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

#include "tee_client_api.h"
#include "tlsecmem_api_gp.h"

#if defined(CONFIG_MICROTRUST_TEE_SUPPORT)
#include "isee_kernel_api.h"
#endif

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

static u32 secmem_session_ref;
static struct TEEC_Context context;
static struct TEEC_Session session;
static struct TEEC_SharedMemory wsm; /* world shared memory */

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
#endif

#define SECMEM_TL_GP_UUID \
	{ 0x08030000, 0x0000, 0x0000, \
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } }

#if defined(CONFIG_MICROTRUST_TEE_SUPPORT)
#define SECMEM_TL_GP_UUID_STRING "bta_loader"
#else
#define SECMEM_TL_GP_UUID_STRING "08030000000000000000000000000000"
#endif

#if defined(CONFIG_MICROTRUST_TEE_SUPPORT)
#define SECMEM_DRV_GP_UUID \
	{ 0x020f0000, 0x0000, 0x0000, \
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } }
#endif

static int secmem_execute(u32 cmd, struct secmem_param *param)
{
	int ret = TEEC_SUCCESS;
#ifdef SECMEM_DEBUG_DUMP
	int len;
#endif

	struct TEEC_Operation op;
	struct secmem_msg_t *msg;

	pr_debug("\n");

	mutex_lock(&secmem_lock);

	memset(wsm.buffer, 0, wsm.size);

	msg = wsm.buffer;
	msg->sec_handle = param->sec_handle;
	msg->alignment = param->alignment;
	msg->size = param->size;
	msg->refcount = param->refcount;
#ifdef SECMEM_DEBUG_DUMP
	msg->name[0] = 0;
	msg->id = param->id;
	if (param->owner[0] != 0) {
		len = param->owner_len > MAX_NAME_SZ ?
				MAX_NAME_SZ : param->owner_len;
		memcpy(msg->name, param->owner, len);
		msg->name[MAX_NAME_SZ - 1] = 0;
	}
#endif

	memset(&op, 0, sizeof(struct TEEC_Operation));
	/* Invoke the command */
	op.paramTypes = TEEC_PARAM_TYPES(
			TEEC_MEMREF_PARTIAL_INOUT,
			TEEC_NONE,
			TEEC_NONE,
			TEEC_NONE);
	op.params[0].memref.parent = &wsm;
	op.params[0].memref.offset = 0;
	op.params[0].memref.size = wsm.size;

	ret = TEEC_InvokeCommand(&session, cmd, &op, NULL);
	if (ret != TEEC_SUCCESS) {
		pr_err("TEEC_InvokeCommand failed: %x\n", ret);
		goto err_invoke_command;
	}

	param->sec_handle = msg->sec_handle;
	param->refcount = msg->refcount;
	param->alignment = msg->alignment;
	param->size = msg->size;

	pr_debug("shndl=0x%llx refcnt=%d align=0x%llx size=0x%llx\n",
		(u64)param->sec_handle, param->refcount, (u64)param->alignment,
		(u64)param->size);

err_invoke_command:
	mutex_unlock(&secmem_lock);

	pr_debug("\n");

	return ret;
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
	handle = (struct secmem_handle *)krealloc(
					ctx->handles,
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

/* TODO: remove this function since it seems useless */
#ifdef SECMEM_64BIT_PHYS_SUPPORT
static void secmem_handle_unregister_check(struct secmem_context *ctx, u32 type,
					   u64 id)
#else
static void secmem_handle_unregister_check(struct secmem_context *ctx, u32 type,
					   u32 id)
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
				pr_debug("type mismatch (%d!=%d) hndl=0x%llx\n",
					_IOC_NR(handle->type), _IOC_NR(type),
					(u64)handle->id);
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

	/* find empty space. */
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
				pr_err("incorrect type=%d (ioctl:%d)\n",
					handle->type, _IOC_NR(handle->type));
				goto error;
			}
			spin_unlock(&ctx->lock);
			ret = secmem_execute(cmd, &param);
			pr_debug("shndl=0x%llx type=%d (ioctl:%d) ret=%d\n",
				(u64)handle->id, handle->type,
				_IOC_NR(handle->type), ret);
			spin_lock(&ctx->lock);
		}
	}

error:
	spin_unlock(&ctx->lock);

	return ret;
}

static int secmem_session_open(void)
{
	int ret = 0;
	struct TEEC_UUID destination = SECMEM_TL_GP_UUID;
#if defined(CONFIG_MICROTRUST_TEE_SUPPORT)
	struct TEEC_UUID secmem_drv_uuid = SECMEM_DRV_GP_UUID;
#endif

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

#if defined(CONFIG_MICROTRUST_TEE_SUPPORT)
		ret = tz_load_drv(&secmem_drv_uuid);
		if (ret != TEEC_SUCCESS) {
			ERR("tz_load_drv failed: %x\n", ret);
			goto err_initialize_context;
		}
#endif

		pr_debug("Testing teec_initialize_context\n");

		ret = TEEC_InitializeContext(SECMEM_TL_GP_UUID_STRING,
					     &context);
		if (ret != TEEC_SUCCESS) {
			pr_err("teec_initialize_context failed: %x\n", ret);
			goto err_initialize_context;
		}

		pr_debug("Testing teec_register_shared_memory with get_zeroed_page\n");

		wsm.buffer = kmalloc(sizeof(struct secmem_msg_t), GFP_KERNEL);
		wsm.size = sizeof(struct secmem_msg_t);
		wsm.flags = TEEC_MEM_INPUT | TEEC_MEM_OUTPUT;
		memset(wsm.buffer, 0, wsm.size);

		ret = TEEC_RegisterSharedMemory(&context, &wsm);
		if (ret != TEEC_SUCCESS) {
			pr_err("TEEC_RegisterSharedMemory failed: %x\n", ret);
			goto err_register_shared_memory;
		}

		pr_debug("Testing TEEC_OpenSession\n");
		ret = TEEC_OpenSession(&context, &session, &destination,
				TEEC_LOGIN_PUBLIC, NULL, NULL, NULL);
		if (ret != TEEC_SUCCESS) {
			pr_err("TEEC_OpenSession failed: %x\n", ret);
			goto err_open_session;
		}

		secmem_session_ref = 1;

	} while (0);


	pr_debug("ret=%d ref=%d\n", ret, secmem_session_ref);

	mutex_unlock(&secmem_lock);
	return ret;

err_open_session:
	pr_err("TEEC_ReleaseSharedMemory\n");
	TEEC_ReleaseSharedMemory(&wsm);

err_register_shared_memory:
	pr_err("TEEC_FinalizeContext\n");
	kfree(wsm.buffer);
	TEEC_FinalizeContext(&context);

err_initialize_context:
	mutex_unlock(&secmem_lock);

	return ret;
}

static int secmem_session_close(void)
{
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

		pr_debug("Testing TEEC_CloseSession\n");
		TEEC_CloseSession(&session);

		pr_debug("Testing TEEC_ReleaseSharedMemory\n");
		TEEC_ReleaseSharedMemory(&wsm);

		/* kfree(NULL) is safe and
		 * this check is probably not required
		 */
		kfree(wsm.buffer);

		pr_debug("Testing TEEC_FinalizeContext\n");
		TEEC_FinalizeContext(&context);

		secmem_session_ref = 0;
	} while (0);

	pr_debug("ref=%d\n", secmem_session_ref);

	mutex_unlock(&secmem_lock);

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
	ctx->handles = kzalloc(sizeof(struct secmem_handle)*DEFAULT_HANDLES_NUM,
			       GFP_KERNEL);
	spin_lock_init(&ctx->lock);

	if (!ctx->handles) {
		kfree(ctx);
		return -ENOMEM;
	}

	/* open session */
#if defined(CONFIG_MICROTRUST_TEE_SUPPORT)
	while (!is_teei_ready()) {
		pr_debug("teei NOT ready!, sleep 1s\n");
		msleep(1000);
	}
#endif

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
		pr_debug("secure memory already online\n");
		return 0;
	}

	/* allocate secure memory region */
#ifdef SECMEM_64BIT_PHYS_SUPPORT
	ret = secmem_region_offline64(&pa, &size);
#else
	ret = secmem_region_offline(&pa, &size);
#endif
	if (ret) {
		pr_err("secmem_region_offline() failed! ret=%d\n", ret);
		return -1;
	}

	if (pa == 0 || size == 0) {
		pr_err("invalid pa(0x%llx) or size(0x%lx)\n", (u64)pa, size);
		return -1;
	}

	/* setup secure memory and enable protection */
	ret = secmem_enable(pa, size);
	if (ret) {
		/* free secure memory */
		secmem_region_online();
		pr_err("secmem_enable() failed! ret=%d\n", ret);
		return -1;
	}

	is_region_ready = 1;
	secmem_region_ref = 0;

#if defined(CONFIG_MTK_SVP_DISABLE_SODI)
	spm_enable_sodi(false);
#endif

	pr_debug("phyaddr=0x%llx sz=0x%lx rgn_on=%u rgn_ref=%u\n",
			(u64)pa, size, is_region_ready, secmem_region_ref);

	return 0;
}

static int secmem_region_release(void)
{
	int ret;

	/* already offline */
	if (is_region_ready == 0) {
		pr_debug("secure memory already offline\n");
		return 0;
	}

	/* region has reference so abort the release */
	if (secmem_region_ref > 0) {
		pr_err("aborted due to secmem_region_ref != 0 (%d)\n",
			secmem_region_ref);
		return -1;
	}

	/* disable protection and recalim secure memory */
	ret = secmem_disable();
	if (ret) {
		pr_err("secmem_disable() failed! ret=%d\n", ret);
		return -1;
	}

	ret = secmem_region_online();
	if (ret) {
		pr_err("secmem_region_online() failed! ret=%d\n", ret);
		return -1;
	}

	is_region_ready = 0;

#if defined(CONFIG_MTK_SVP_DISABLE_SODI)
	spm_enable_sodi(true);
#endif

	pr_debug("done, rgn_on=%u\n", is_region_ready);

	return 0;
}
#endif

static long secmem_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int err = 0;
	struct secmem_context *ctx =
		(struct secmem_context *)file->private_data;
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
		pr_debug("rgn_on=%u rgn_ref=%u\n",
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
		pr_debug("cmd=%u rgn_on=%u rgn_ref=%u!!\n",
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
	pr_debug("ret=%d\n", err);

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
	pr_debug("ret=%d\n", err);

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
	pr_debug("ret=%d\n", err);

	return err;
}
EXPORT_SYMBOL(secmem_api_query);
#endif /* CONFIG_CMA && CONFIG_MTK_SSMR */

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

	pr_debug("rgn_on=%u rgn_ref=%u\n",
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
		 * decrease region_ref when session_open() and
		 * execute() failed.
		 */
		if (is_region_ready)
			secmem_region_ref--;
		mutex_unlock(&secmem_region_lock);
#endif
	}

#if defined(CONFIG_CMA) && defined(CONFIG_MTK_SSMR)
	pr_debug("align=0x%x size=0x%x owner=%s id=0x%x clean=%d ret=%d refcnt=0x%x shndl=0x%x rgn_on=%u rgn_ref=%u\n",
		alignment, size, (owner ? (char *)owner : "NULL"), id, clean,
		ret, *refcount, *sec_handle, is_region_ready,
		secmem_region_ref);
#else
	pr_debug("align=0x%x size=0x%x id=0x%x clean=%d ret=%d refcnt=0x%x shndl=0x%x\n",
		alignment, size, id, clean, ret, *refcount, *sec_handle);
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
					   msecs_to_jiffies(
						SECMEM_RECLAIM_DELAY));
		} else {
			pr_debug("rgn_on=%u rgn_ref=%u\n",
			is_region_ready, secmem_region_ref);
		}
		mutex_unlock(&secmem_region_lock);
	}
#endif

#if defined(CONFIG_CMA) && defined(CONFIG_MTK_SSMR)
	pr_debug("ret=%d shndl=0x%x owner=%s id=0x%x rgn_on=%u rgn_ref=%u\n",
		ret, sec_handle, (owner ? (char *)owner : "NULL"), id,
		is_region_ready, secmem_region_ref);
#else
	pr_debug("ret=%d shndl=0x%x owner=%s id=0x%x\n", ret, sec_handle,
		 (owner ? (char *)owner : "NULL"), id);
#endif

	return ret;
}
EXPORT_SYMBOL(secmem_api_unref);
#endif /* END OF SECMEM_KERNEL_API */

static void secmem_unit_test(char *cmd)
{
#ifdef CONFIG_MTK_ENG_BUILD
	if (!cmd) {
		pr_err("cmd is NULL!\n");
		return;
	}
#if defined(CONFIG_CMA) && defined(CONFIG_MTK_SSMR)
	if (!strncmp(cmd, "0", 1)) {
		pr_info("test for secmem_region_release()\n");
		mutex_lock(&secmem_region_lock);
		secmem_region_ref--;
		secmem_region_release();
		mutex_unlock(&secmem_region_lock);
	} else if (!strncmp(cmd, "1", 1)) {
		pr_info("test for secmem_region_alloc()\n");
		mutex_lock(&secmem_region_lock);
		secmem_region_alloc();
		secmem_region_ref++;
		mutex_unlock(&secmem_region_lock);
	} else if (!strncmp(cmd, "2", 1)) {
#ifdef SECMEM_64BIT_PHYS_SUPPORT
		u64 size = 0;
#else
		u32 size = 0;
#endif

		pr_info("test for secmem_api_query()\n");
		secmem_api_query(&size);
		pr_info("allocated : 0x%llx\n", (u64)size);
	} else if (!strncmp(cmd, "3", 1)) {
#ifdef SECMEM_KERNEL_API
#ifdef SECMEM_64BIT_PHYS_SUPPORT
		u64 size = 0;
#else
		u32 size = 0;
#endif
		u32 sec_handle = 0;
		u32 refcount = 0;
		char owner[] = "secme_ut";

		pr_info("test for alloc-query-unref\n");
		secmem_api_alloc(0x1000, 0x1000, &refcount, &sec_handle,
				 owner, 0);
		secmem_api_query(&size);
		pr_info("after alloc : 0x%llx\n", (u64)size);
		secmem_api_unref(sec_handle, owner, 0);
		secmem_api_query(&size);
		pr_info("after unref : 0x%llx\n", (u64)size);
#endif /* SECMEM_KERNEL_API */
	}
#else /* CONFIG_CMA && CONFIG_MTK_SSMR */

#endif /* CONFIG_CMA || !CONFIG_MTK_SSMR */
#endif /* CONFIG_MT_ENG_BUILD */
}


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

	if (sscanf(desc, "%1s", cmd) == 1)
		secmem_unit_test(cmd);

	return count;
}

static const struct file_operations secmem_fops = {
	.owner = THIS_MODULE,
	.open = secmem_open,
	.release = secmem_release,
	.unlocked_ioctl = secmem_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = secmem_ioctl,
#endif
	.write = secmem_write,
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

	return 0;
}
late_initcall(secmem_init);
