/*
 * Copyright (c) 2013-2015 TRUSTONIC LIMITED
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
#include <asm/pgtable.h>

#include <linux/highmem.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/ioctl.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/stat.h>
#include <linux/debugfs.h>

#include "public/mc_linux.h"

#include "main.h"
#include "fastcall.h"
#include "arm.h"
#include "mmu.h"
#include "scheduler.h"
#include "pm.h"
#include "debug.h"
#include "logging.h"
#include "admin.h"
#include "mcp.h"
#include "session.h"
#include "client.h"
#include "api.h"

#include "build_tag.h"

/* Define a MobiCore device structure for use with dev_debug() etc */
static struct device_driver driver = {
	.name = "Trustonic"
};

static struct device device = {
	.driver = &driver
};

struct mc_device_ctx g_ctx = {
	.mcd = &device
};

/* device admin */
static dev_t mc_dev_admin;
/* device user */
static dev_t mc_dev_user;

/* Need to discover a chrdev region for the driver */
static struct cdev mc_user_cdev;
/* Device class for the driver assigned major */
static struct class *mc_device_class;

/*
 * Get client object from file pointer
 */
static inline struct tbase_client *get_client(struct file *file)
{
	return (struct tbase_client *)file->private_data;
}

/*
 * Callback for system mmap()
 */
static int mc_fd_user_mmap(struct file *file, struct vm_area_struct *vmarea)
{
	struct tbase_client *client = get_client(file);
	uint32_t len = (uint32_t)(vmarea->vm_end - vmarea->vm_start);

	/* Alloc contiguous buffer for this client */
	return api_malloc_cbuf(client, len, NULL, vmarea);
}

/*
 * Check r/w access to referenced memory
 */
static inline int ioctl_check_pointer(unsigned int cmd, int __user *uarg)
{
	int err = 0;

	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, uarg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ, uarg, _IOC_SIZE(cmd));

	if (err)
		return -EFAULT;

	return 0;
}

/*
 * Callback for system ioctl()
 * Implement most of ClientLib API functions
 * @file	pointer to file
 * @cmd		command
 * @arg		arguments
 *
 * Returns 0 for OK and an errno in case of error
 */
static long mc_fd_user_ioctl(struct file *file, unsigned int id,
			     unsigned long arg)
{
	struct tbase_client *client = get_client(file);
	int __user *uarg = (int __user *)arg;
	int ret = -EINVAL;

	MCDRV_DBG("%u from %s", _IOC_NR(id), current->comm);

	if (WARN(!client, "No client data available"))
		return -EPROTO;

	if (ioctl_check_pointer(id, uarg))
		return -EFAULT;

	switch (id) {
	case MC_IO_FREEZE:
		/* Freeze the client */
		ret = api_freeze_device(client);
		break;

	case MC_IO_OPEN_SESSION: {
		struct mc_ioctl_open_sess sess;

		if (copy_from_user(&sess, uarg, sizeof(sess))) {
			ret = -EFAULT;
			break;
		}

		ret = api_open_session(client, &sess.sid, &sess.uuid, sess.tci,
				       sess.tcilen, sess.is_gp_uuid,
				       &sess.identity);
		if (ret)
			break;

		if (copy_to_user(uarg, &sess, sizeof(sess))) {
			ret = -EFAULT;
			api_close_session(client, sess.sid);
			break;
		}
		break;
	}
	case MC_IO_OPEN_TRUSTLET: {
		struct mc_ioctl_open_trustlet ta_desc;

		if (copy_from_user(&ta_desc, uarg, sizeof(ta_desc))) {
			ret = -EFAULT;
			break;
		}

		/* Call internal api */
		ret = api_open_trustlet(client, &ta_desc.sid, ta_desc.spid,
					ta_desc.buffer, ta_desc.tlen,
					ta_desc.tci, ta_desc.tcilen);
		if (ret)
			break;

		if (copy_to_user(uarg, &ta_desc, sizeof(ta_desc))) {
			ret = -EFAULT;
			api_close_session(client, ta_desc.sid);
			break;
		}
		break;
	}
	case MC_IO_CLOSE_SESSION: {
		uint32_t sid = (uint32_t)arg;

		ret = api_close_session(client, sid);
		break;
	}
	case MC_IO_NOTIFY: {
		uint32_t sid = (uint32_t)arg;

		ret = api_notify(client, sid);
		break;
	}
	case MC_IO_WAIT: {
		struct mc_ioctl_wait wait;

		if (copy_from_user(&wait, uarg, sizeof(wait))) {
			ret = -EFAULT;
			break;
		}
		ret = api_wait_notification(client, wait.sid, wait.timeout);
		break;
	}
	case MC_IO_MAP: {
		struct mc_ioctl_map map;

		if (copy_from_user(&map, uarg, sizeof(map))) {
			ret = -EFAULT;
			break;
		}
		ret = api_map_wsms(client, map.sid, map.bufs);
		if (ret)
			break;

		/* Fill in return struct */
		if (copy_to_user(uarg, &map, sizeof(map))) {
			ret = -EFAULT;
			api_unmap_wsms(client, map.sid, map.bufs);
			break;
		}
		break;
	}
	case MC_IO_UNMAP: {
		struct mc_ioctl_map map;

		if (copy_from_user(&map, uarg, sizeof(map))) {
			ret = -EFAULT;
			break;
		}

		ret = api_unmap_wsms(client, map.sid, map.bufs);
		break;
	}
	case MC_IO_ERR: {
		struct mc_ioctl_geterr *uerr = (struct mc_ioctl_geterr *)uarg;
		uint32_t sid;
		int32_t exit_code;

		if (get_user(sid, &uerr->sid)) {
			ret = -EFAULT;
			break;
		}

		ret = api_get_session_exitcode(client, sid, &exit_code);
		if (ret)
			break;

		/* Fill in return struct */
		if (put_user(exit_code, &uerr->value)) {
			ret = -EFAULT;
			break;
		}

		break;
	}
	case MC_IO_VERSION: {
		struct mc_version_info version_info;

		ret = mcp_get_version(&version_info);
		if (ret)
			break;

		if (copy_to_user(uarg, &version_info, sizeof(version_info)))
			ret = -EFAULT;

		break;
	}
	case MC_IO_DR_VERSION: {
		uint32_t version = MC_VERSION(MCDRVMODULEAPI_VERSION_MAJOR,
					      MCDRVMODULEAPI_VERSION_MINOR);

		ret = put_user(version, uarg);
		break;
	}
	default:
		MCDRV_ERROR("unsupported cmd=0x%x", id);
		ret = -ENOIOCTLCMD;
	}

	return ret;
}

/*
 * Callback for system open()
 * A set of internal client data are created and initialized.
 *
 * @inode
 * @file
 * Returns 0 if OK or -ENOMEM if no allocation was possible.
 */
static int mc_fd_user_open(struct inode *inode, struct file *file)
{
	struct tbase_client *client;

	MCDRV_DBG("from %s", current->comm);

	/* Create client */
	client = api_open_device(false);
	if (!client)
		return -ENOMEM;

	/* Store client in user file */
	file->private_data = client;
	return 0;
}

/*
 * Callback for system close()
 * The client object is freed.
 * @inode
 * @file
 * Returns 0
 */
static int mc_fd_user_release(struct inode *inode, struct file *file)
{
	struct tbase_client *client = get_client(file);

	MCDRV_DBG("from %s", current->comm);

	if (WARN(!client, "No client data available"))
		return -EPROTO;

	/* Detach client from user file */
	file->private_data = NULL;

	/* Destroy client, including remaining sessions */
	api_close_device(client);
	return 0;
}

static const struct file_operations mc_user_fops = {
	.owner = THIS_MODULE,
	.open = mc_fd_user_open,
	.release = mc_fd_user_release,
	.unlocked_ioctl = mc_fd_user_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = mc_fd_user_ioctl,
#endif
	.mmap = mc_fd_user_mmap,
};

int kasnprintf(struct kasnprintf_buf *buf, const char *fmt, ...)
{
	va_list args;
	int max_size = buf->size - buf->off;
	int i;

	va_start(args, fmt);
	i = vsnprintf(buf->buf + buf->off, max_size, fmt, args);
	if (i >= max_size) {
		int new_size = PAGE_ALIGN(buf->size + i + 1);
		char *new_buf = krealloc(buf->buf, new_size, buf->gfp);

		if (!new_buf) {
			i = -ENOMEM;
		} else {
			buf->buf = new_buf;
			buf->size = new_size;
			max_size = buf->size - buf->off;
			i = vsnprintf(buf->buf + buf->off, max_size, fmt, args);
		}
	}

	if (i > 0)
		buf->off += i;

	va_end(args);
	return i;
}

static ssize_t debug_info_read(struct file *file, char __user *user_buf,
			       size_t count, loff_t *ppos)
{
	/* Add/update buffer */
	if (!file->private_data || !*ppos) {
		struct kasnprintf_buf *buf, *old_buf;
		int ret;

		buf = kzalloc(GFP_KERNEL, sizeof(*buf));
		if (!buf)
			return -ENOMEM;

		buf->gfp = GFP_KERNEL;
		ret = api_info(buf);
		if (ret < 0) {
			kfree(buf);
			return ret;
		}

		old_buf = file->private_data;
		file->private_data = buf;
		kfree(old_buf);
	}

	if (file->private_data) {
		struct kasnprintf_buf *buf = file->private_data;

		return simple_read_from_buffer(user_buf, count, ppos, buf->buf,
					       buf->off);
	}

	return 0;
}

static int debug_info_release(struct inode *inode, struct file *file)
{
	kfree(file->private_data);
	return 0;
}

static const struct file_operations mc_debug_info_ops = {
	.read = debug_info_read,
	.llseek = default_llseek,
	.release = debug_info_release,
};

static inline int device_admin_init(int (*tee_start_cb)(void))
{
	int ret = 0;

	cdev_init(&mc_user_cdev, &mc_user_fops);

	mc_device_class = class_create(THIS_MODULE, "trustonic_tee");
	if (IS_ERR(mc_device_class)) {
		MCDRV_ERROR("failed to create device class");
		return PTR_ERR(mc_device_class);
	}

	/* Create the ADMIN node */
	ret = mc_admin_init(mc_device_class, &mc_dev_admin, tee_start_cb);
	if (ret < 0) {
		MCDRV_ERROR("failed to init mobicore device");
		class_destroy(mc_device_class);
		return ret;
	}
	return 0;
}

static inline int device_user_init(void)
{
	int ret = 0;
	struct device *dev;

	mc_dev_user = MKDEV(MAJOR(mc_dev_admin), 1);
	/* Create the user node */
	ret = cdev_add(&mc_user_cdev, mc_dev_user, 1);
	if (ret) {
		MCDRV_ERROR("user device register failed");
		goto err_cdev_add;
	}
	mc_user_cdev.owner = THIS_MODULE;
	dev = device_create(mc_device_class, NULL, mc_dev_user, NULL,
			    MC_USER_DEVNODE);
	if (IS_ERR(dev)) {
		ret = PTR_ERR(dev);
		goto err_device_create;
	}

	/* Create debugfs info entry */
	debugfs_create_file("info", 0400, g_ctx.debug_dir, NULL,
			    &mc_debug_info_ops);

	return 0;

err_device_create:
	cdev_del(&mc_user_cdev);
err_cdev_add:
	mc_admin_exit(mc_device_class);
	class_destroy(mc_device_class);
	MCDRV_DBG("failed with %d", ret);
	return ret;
}

static void devices_exit(void)
{
	device_destroy(mc_device_class, mc_dev_user);
	cdev_del(&mc_user_cdev);
	mc_admin_exit(mc_device_class);
	class_destroy(mc_device_class);
}

static inline int mobicore_start(void)
{
	int ret;
	struct mc_version_info version_info;

	ret = mcp_start();
	if (ret) {
		MCDRV_ERROR("TEE start failed");
		goto err_mcp;
	}

	ret = mc_logging_start();
	if (ret) {
		MCDRV_ERROR("Log start failed");
		goto err_log;
	}

	ret = mc_scheduler_start();
	if (ret) {
		MCDRV_ERROR("Scheduler start failed");
		goto err_sched;
	}

	ret = mc_pm_start();
	if (ret) {
		MCDRV_ERROR("Power Management start failed");
		goto err_pm;
	}

	ret = mcp_get_version(&version_info);
	if (ret)
		goto err_mcp_cmd;

	MCDRV_DBG("\n"
		  "    product_id        = %s\n"
		  "    version_so        = 0x%x\n"
		  "    version_mci       = 0x%x\n"
		  "    version_mclf      = 0x%x\n"
		  "    version_container = 0x%x\n"
		  "    version_mc_config = 0x%x\n"
		  "    version_tl_api    = 0x%x\n"
		  "    version_dr_api    = 0x%x\n"
		  "    version_cmp       = 0x%x\n",
		  version_info.product_id,
		  version_info.version_mci,
		  version_info.version_so,
		  version_info.version_mclf,
		  version_info.version_container,
		  version_info.version_mc_config,
		  version_info.version_tl_api,
		  version_info.version_dr_api,
		  version_info.version_cmp);

	if (MC_VERSION_MAJOR(version_info.version_mci) > 1) {
		pr_err("MCI version %d.%d is too recent for this driver",
		       MC_VERSION_MAJOR(version_info.version_mci),
		       MC_VERSION_MINOR(version_info.version_mci));
		goto err_version;
	}

	if ((MC_VERSION_MAJOR(version_info.version_mci) == 0) &&
	    (MC_VERSION_MINOR(version_info.version_mci) < 6)) {
		pr_err("MCI version %d.%d is too old for this driver",
		       MC_VERSION_MAJOR(version_info.version_mci),
		       MC_VERSION_MINOR(version_info.version_mci));
		goto err_version;
	}

	dev_info(g_ctx.mcd, "MobiCore MCI version is %d.%d\n",
		 MC_VERSION_MAJOR(version_info.version_mci),
		 MC_VERSION_MINOR(version_info.version_mci));

	/* Determine which features are supported */
	switch (version_info.version_mci) {
	case MC_VERSION(1, 2):	/* 310 */
		g_ctx.f_client_login = true;
		/* Fall through */
	case MC_VERSION(1, 1):
		g_ctx.f_multimap = true;
		/* Fall through */
	case MC_VERSION(1, 0):	/* 302 */
		g_ctx.f_mem_ext = true;
		g_ctx.f_ta_auth = true;
		/* Fall through */
	case MC_VERSION(0, 7):
		g_ctx.f_timeout = true;
		/* Fall through */
	case MC_VERSION(0, 6):	/* 301 */
		break;
	}

	ret = device_user_init();
	if (ret)
		goto err_create_dev_user;

	return 0;

err_create_dev_user:
err_version:
err_mcp_cmd:
	mc_pm_stop();
err_pm:
	mc_scheduler_stop();
err_sched:
	mc_logging_stop();
err_log:
	mcp_stop();
err_mcp:
	return ret;
}

static inline void mobicore_stop(void)
{
	mc_pm_stop();
	mc_scheduler_stop();
	mc_logging_stop();
	mcp_stop();
}

/*
 * This function is called by the kernel during startup or by a insmod command.
 * This device is installed and registered as cdev, then interrupt and
 * queue handling is set up
 */
static int mobicore_init(void)
{
	int err = 0;

	dev_set_name(g_ctx.mcd, "TEE");

	/* Do not remove or change the following trace.
	 * The string "MobiCore" is used to detect if <t-base is in of the image
	 */
	dev_info(g_ctx.mcd, "MobiCore mcDrvModuleApi version is %d.%d\n",
		 MCDRVMODULEAPI_VERSION_MAJOR, MCDRVMODULEAPI_VERSION_MINOR);
#ifdef MOBICORE_COMPONENT_BUILD_TAG
	dev_info(g_ctx.mcd, "MobiCore %s\n", MOBICORE_COMPONENT_BUILD_TAG);
#endif
	/* Hardware does not support ARM TrustZone -> Cannot continue! */
	if (!has_security_extensions()) {
		MCDRV_ERROR("Hardware doesn't support ARM TrustZone!");
		return -ENODEV;
	}

	/* Running in secure mode -> Cannot load the driver! */
	if (is_secure_mode()) {
		MCDRV_ERROR("Running in secure MODE!");
		return -ENODEV;
	}

	/* Init common API layer */
	api_init();

	/* Init plenty of nice features */
	err = mc_fastcall_init();
	if (err) {
		MCDRV_ERROR("Fastcall support init failed!");
		goto fail_fastcall_init;
	}

	err = mcp_init();
	if (err) {
		MCDRV_ERROR("MCP init failed!");
		goto fail_mcp_init;
	}

	err = mc_logging_init();
	if (err) {
		MCDRV_ERROR("Log init failed!");
		goto fail_log_init;
	}

	/* The scheduler is the first to create a debugfs entry */
	g_ctx.debug_dir = debugfs_create_dir("trustonic_tee", NULL);
	err = mc_scheduler_init();
	if (err) {
		MCDRV_ERROR("Scheduler init failed!");
		goto fail_mc_device_sched_init;
	}

	/*
	* Create admin dev so that daemon can already communicate with
	* the driver
	*/
	err = device_admin_init(mobicore_start);
	if (err)
		goto fail_creat_dev_admin;

		return 0;

fail_creat_dev_admin:
	mc_scheduler_exit();
fail_mc_device_sched_init:
	debugfs_remove(g_ctx.debug_dir);
	mc_logging_exit();
fail_log_init:
	mcp_exit();
fail_mcp_init:
	mc_fastcall_exit();
fail_fastcall_init:
	return err;
}

/*
 * This function removes this device driver from the Linux device manager .
 */
static void mobicore_exit(void)
{
	MCDRV_DBG("enter");

	devices_exit();
	mobicore_stop();
	mc_scheduler_exit();
	mc_logging_exit();
	mcp_exit();
	mc_fastcall_exit();
	debugfs_remove_recursive(g_ctx.debug_dir);

	MCDRV_DBG("exit");
}

/* Linux Driver Module Macros */

#ifdef MC_DEVICE_PROPNAME

static int mobicore_probe(struct platform_device *pdev)
{
	g_ctx.mcd->of_node = pdev->dev.of_node;
	mobicore_init();
	return 0;
}

static const struct of_device_id of_match_table[] = {
	{ .compatible = MC_DEVICE_PROPNAME },
	{ }
};

static struct platform_driver mc_plat_driver = {
	.probe = mobicore_probe,
	.driver = {
		.name = "mcd",
		.owner = THIS_MODULE,
		.of_match_table = of_match_table,
	}
};

static int mobicore_register(void)
{
	return platform_driver_register(&mc_plat_driver);
}

static void mobicore_unregister(void)
{
	platform_driver_unregister(&mc_plat_driver);
	mobicore_exit();
}

module_init(mobicore_register);
module_exit(mobicore_unregister);

#else /* MC_DEVICE_PROPNAME */

module_init(mobicore_init);
module_exit(mobicore_exit);

#endif /* !MC_DEVICE_PROPNAME */

MODULE_AUTHOR("Trustonic Limited");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MobiCore driver");
