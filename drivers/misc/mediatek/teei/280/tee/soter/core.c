/*
 * Copyright (c) 2017, MicroTrust
 * Copyright (c) 2015, Linaro Limited
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/version.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <tee_drv.h>
#include <linux/types.h>
#include <linux/uaccess.h>

#define IMSG_TAG "[tz_driver]"
#include <imsg_log.h>

#include "soter_private.h"
#include "soter_smc.h"

#define SOTER_SHM_NUM_PRIV_PAGES	1

static struct reserved_mem *reserved_mem;
static atomic_t is_shm_pool_available = ATOMIC_INIT(0);
static DECLARE_COMPLETION(shm_pool_registered);

int teei_new_capi_init(void)
{
	if (reserved_mem) {
		int ret = soter_register_shm_pool(
				reserved_mem->base, reserved_mem->size);

		if (!ret) {
			atomic_set(&is_shm_pool_available, 1);
			complete_all(&shm_pool_registered);
		}
		return ret;
	}

	IMSG_ERROR("capi reserve memory is NULL!\n");

	return -ENOMEM;
}

static void soter_get_version(struct tee_device *teedev,
			      struct tee_ioctl_version_data *vers)
{
	struct tee_ioctl_version_data v = {
		.impl_id = 0x0,
		.impl_caps = 0x0,
		.gen_caps = TEE_GEN_CAP_GP,
	};
	*vers = v;
}

static int soter_open(struct tee_context *ctx)
{
	struct soter_context_data *ctxdata;
	int ret;

	ctxdata = kzalloc(sizeof(*ctxdata), GFP_KERNEL);
	if (!ctxdata)
		return -ENOMEM;

	mutex_init(&ctxdata->mutex);
	INIT_LIST_HEAD(&ctxdata->sess_list);

	ctx->data = ctxdata;

	if (!atomic_read(&is_shm_pool_available)) {
		ret = wait_for_completion_interruptible(&shm_pool_registered);
		if (ret == -ERESTARTSYS)
			return -EINTR;
	}

	return 0;
}

static void soter_release(struct tee_context *ctx)
{
	struct soter_context_data *ctxdata = ctx->data;
	struct tee_shm *shm;
	struct optee_msg_arg *arg = NULL;
	phys_addr_t parg;
	struct soter_session *sess;
	struct soter_session *sess_tmp;

	if (!ctxdata)
		return;

	shm = tee_shm_alloc(ctx, sizeof(struct optee_msg_arg), TEE_SHM_MAPPED);
	if (!IS_ERR(shm)) {
		arg = tee_shm_get_va(shm, 0);
		/*
		 * If va2pa fails for some reason, we can't call
		 * soter_close_session(), only free the memory. Secure OS
		 * will leak sessions and finally refuse more sessions, but
		 * we will at least let normal world reclaim its memory.
		 */
		if (!IS_ERR(arg))
			tee_shm_va2pa(shm, arg, &parg);
	}

	list_for_each_entry_safe(sess, sess_tmp, &ctxdata->sess_list,
				list_node) {
		list_del(&sess->list_node);
		if (!IS_ERR_OR_NULL(arg)) {
			memset(arg, 0, sizeof(*arg));
			arg->cmd = OPTEE_MSG_CMD_CLOSE_SESSION;
			arg->session = sess->session_id;
			soter_do_call_with_arg(ctx, parg);
		}
		kfree(sess);
	}
	kfree(ctxdata);

	if (!IS_ERR(shm))
		tee_shm_free(shm);

	ctx->data = NULL;
}

static struct tee_driver_ops soter_ops = {
	.get_version = soter_get_version,
	.open = soter_open,
	.release = soter_release,
	.open_session = soter_open_session,
	.close_session = soter_close_session,
	.invoke_func = soter_invoke_func,
	.cancel_req = soter_cancel_func,
};

static struct tee_desc soter_desc = {
	.name = "soter-clnt",
	.ops = &soter_ops,
	.owner = THIS_MODULE,
};

static struct soter_priv *soter_priv;

static struct tee_shm_pool *
soter_config_shm_memremap(void **memremaped_shm)
{
	struct tee_shm_pool *pool;
	unsigned long vaddr;
	phys_addr_t paddr;
	size_t size;
	void *va;
	struct tee_shm_pool_mem_info priv_info;
	struct tee_shm_pool_mem_info dmabuf_info;

	if (!reserved_mem) {
		IMSG_ERROR("cannot find reserved memory in device tree\n");
		return ERR_PTR(-EINVAL);
	}

	paddr = reserved_mem->base;
	size = reserved_mem->size;
	IMSG_INFO("reserved memory @ 0x%llx, size %zx\n",
		(unsigned long long)paddr, size);

	if (size < 2 * SOTER_SHM_NUM_PRIV_PAGES * PAGE_SIZE) {
		IMSG_ERROR("too small shared memory area\n");
		return ERR_PTR(-EINVAL);
	}

	va = ioremap_cache(paddr, size);

	if (!va) {
		IMSG_ERROR("shared memory ioremap failed\n");
		return ERR_PTR(-EINVAL);
	}
	vaddr = (unsigned long)va;

	priv_info.vaddr = vaddr;
	priv_info.paddr = paddr;
	priv_info.size = SOTER_SHM_NUM_PRIV_PAGES * PAGE_SIZE;
	dmabuf_info.vaddr = vaddr + SOTER_SHM_NUM_PRIV_PAGES * PAGE_SIZE;
	dmabuf_info.paddr = paddr + SOTER_SHM_NUM_PRIV_PAGES * PAGE_SIZE;
	dmabuf_info.size = size - SOTER_SHM_NUM_PRIV_PAGES * PAGE_SIZE;

	pool = tee_shm_pool_alloc_res_mem(&priv_info, &dmabuf_info);
	if (IS_ERR(pool))
		goto out;

	*memremaped_shm = va;
out:
	return pool;
}

static void soter_remove(struct soter_priv *soter)
{
	/*
	 * The device has to be unregistered before we can free the
	 * other resources.
	 */
	tee_device_unregister(soter->teedev);

	tee_shm_pool_free(soter->pool);
	mutex_destroy(&soter->call_queue.mutex);

	kfree(soter);
}

static int __init soter_driver_init(void)
{
	struct tee_shm_pool *pool = NULL;
	struct tee_device *teedev = NULL;
	void *memremaped_shm = NULL;
	int rc;

	soter_priv = kzalloc(sizeof(*soter_priv), GFP_KERNEL);
	if (!soter_priv) {
		rc = -ENOMEM;
		goto err;
	}

	pool = soter_config_shm_memremap(&memremaped_shm);
	if (IS_ERR(pool))
		return PTR_ERR(pool);
	soter_priv->pool = pool;
	soter_priv->memremaped_shm = memremaped_shm;

	teedev = tee_device_alloc(&soter_desc, NULL, pool, soter_priv);
	if (IS_ERR(teedev)) {
		rc = PTR_ERR(teedev);
		goto err;
	}
	soter_priv->teedev = teedev;

	rc = tee_device_register(teedev);
	if (rc)
		goto err;

	return 0;

err:
	if (soter_priv) {
		/*
		 * tee_device_unregister() is safe to call even if the
		 * devices hasn't been registered with
		 * tee_device_register() yet.
		 */
		tee_device_unregister(soter_priv->teedev);
		kfree(soter_priv);
	}
	if (pool)
		tee_shm_pool_free(pool);
	return rc;
}
module_init(soter_driver_init);

static void __exit soter_driver_exit(void)
{
	if (soter_priv)
		soter_remove(soter_priv);
	soter_priv = NULL;
}
module_exit(soter_driver_exit);

static int __init shared_mem_pool_setup(struct reserved_mem *rmem)
{
	reserved_mem = rmem;
	return 0;
}
RESERVEDMEM_OF_DECLARE(soter_shared_mem, "microtrust,shared_mem",
						shared_mem_pool_setup);

MODULE_AUTHOR("Microtrust");
MODULE_DESCRIPTION("Soter driver");
MODULE_SUPPORTED_DEVICE("");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL v2");
