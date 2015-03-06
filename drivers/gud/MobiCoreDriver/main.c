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
#include <linux/device.h>
#include <linux/module.h>
#include <linux/ioctl.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/stat.h>

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
#include "nqueue.h"
#include "mcp.h"
#include "api.h"

#include "build_tag.h"

#define NQ_NUM_ELEMS      16
#if defined(MC_CRYPTO_CLOCK_MANAGEMENT) && defined(MC_USE_DEVICE_TREE)

#include <linux/platform_device.h>
#endif

/* Length of notification queues */
static uint16_t txq_length = NQ_NUM_ELEMS;
static uint16_t rxq_length = NQ_NUM_ELEMS;

/* Define a MobiCore device structure for use with dev_debug() etc */
struct device_driver mcd_debug_name = {.name = "<t-base"};

struct device mcd_debug_subname = {.driver = &mcd_debug_name};

/* Need to discover a chrdev region for the driver */
static dev_t mc_dev_user;
struct cdev mc_user_cdev;
/* Device class for the driver assigned major */
static struct class *mc_device_class;

struct mc_device_ctx g_ctx = {.mcd = &mcd_debug_subname};

/* List of temporary notifications */
struct notif_t {
	uint32_t sid;
	int32_t error;
	struct list_head list;
};

/*
 * Find a session object in the list of closing sessions and increment its
 * reference counter.
 * return: pointer to the object, NULL if not found.
 */
struct tbase_session *mc_ref_session(uint32_t session_id)
{
	struct tbase_session *session = NULL, *candidate;

	mutex_lock(&g_ctx.closing_lock);
	list_for_each_entry(candidate, &g_ctx.closing_sess, list) {
		if (candidate->id == session_id) {
			session = candidate;
			session_get(session);
			break;
		}
	}

	mutex_unlock(&g_ctx.closing_lock);
	return session;
}

/*
 * Wake a session up on receiving a notification from SWd.
 */
void mc_wakeup_session(uint32_t session_id, int32_t payload)
{
	struct tbase_client *client;
	struct tbase_session *session = NULL;
	struct notif_t *notif;

	/* Look for up-and-running session */
	mutex_lock(&g_ctx.clients_lock);
	list_for_each_entry(client, &g_ctx.clients, list) {
		session = client_ref_session(client, session_id);
		if (session)
			break;
	}

	mutex_unlock(&g_ctx.clients_lock);

	if (session) {
		session_notify_nwd(session, payload);
		client_unref_session(session);
		return;
	}

	/* Look for closing session */
	session = mc_ref_session(session_id);
	if (session) {
		/* Treat only "closing" notif, ie payload!=0 */
		if (payload)
			/* Note: the worker is responsible for session_put */
			schedule_work(&session->close_work);
		else
			session_put(session);
		return;
	}

	/* None of the above => session is opening and not yet in the main list.
	 * Store notif in a temporary list. Session will pick it up on due time.
	 */
	notif = kzalloc(sizeof(*notif), GFP_KERNEL);
	if (unlikely(!notif)) {
		MCDRV_DBG_WARN(
		"Out of memory, missed notif with payload %d for session %u",
		payload, session_id);
		return;
	}

	notif->sid = session_id;
	notif->error = payload;
	INIT_LIST_HEAD(&notif->list);

	mutex_lock(&g_ctx.clients_lock);
	list_add_tail(&notif->list, &g_ctx.temp_nq);
	mutex_unlock(&g_ctx.clients_lock);
}

/*
 * Get notifications that arrived before session object was visible.
 * Notifications are cleared after reading.
 */
bool mc_read_notif(uint32_t session_id, int32_t *p_error)
{
	bool nq_flag = false;
	struct notif_t *notif, *next;

	mutex_lock(&g_ctx.clients_lock);

	/* Scan the whole list */
	list_for_each_entry_safe(notif, next, &g_ctx.temp_nq, list) {
		if (notif->sid != session_id)
			continue;

		/* Read + clear the notification */
		nq_flag = true;
		*p_error = notif->error;
		list_del(&notif->list);
		kfree(notif);

		/*
		 * error means session died, stop scanning
		 * (other notifs might belong to new session with re-used id...)
		 */
		if (0 != *p_error)
			break;
	}

	mutex_unlock(&g_ctx.clients_lock);

	return nq_flag;
}

/*
 * Get client object from file pointer
 */
static inline struct tbase_client *get_client(struct file *file)
{
	return (struct tbase_client *)file->private_data;
}

/*
 * Add a client to the list of clients and reference it
 */
void mc_add_client(struct tbase_client *client)
{
	mutex_lock(&g_ctx.clients_lock);
	list_add_tail(&client->list, &g_ctx.clients);
	mutex_unlock(&g_ctx.clients_lock);
}

/*
 * Close a client and delete all its cbuf + session objects.
 */
int mc_remove_client(struct tbase_client *client)
{
	struct tbase_client *candidate, *next;
	int err = -ENODEV;

	/* Remove client from list so that nobody can reference it anymore */
	mutex_lock(&g_ctx.clients_lock);

	list_for_each_entry_safe(candidate, next, &g_ctx.clients, list) {
		if (candidate == client) {
			list_del_init(&candidate->list); /* use init version */
			err = 0;
			break;
		}
	}
	mutex_unlock(&g_ctx.clients_lock);

	return err;
}

/*
 * Reference a client.
 * ie make sure it exists and protect it from deletion.
 */
bool mc_ref_client(struct tbase_client *client)
{
	struct tbase_client *candidate;
	bool ret = false;

	mutex_lock(&g_ctx.clients_lock);

	list_for_each_entry(candidate, &g_ctx.clients, list) {
		if (candidate == client) {
			client_get(client);
			ret = true;
			break;
		}
	}
	mutex_unlock(&g_ctx.clients_lock);

	return ret;
}

/*
 * Un-reference client.
 * Client will be deleted if reference drops to 0.
 * No lookup since client may have been removed from list.
 */
void mc_unref_client(struct tbase_client *client)
{
	if (likely(client))
		client_put(client);
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

	if (WARN(!client, "No client data available")) {
		ret = -EPROTO;
		goto out;
	}

	if (ioctl_check_pointer(id, uarg)) {
		ret = -EFAULT;
		goto out;
	}

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
				       sess.tcilen, sess.is_gp_uuid);
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
		ret = api_map_wsm(client, map.sid, (void *)(uintptr_t)map.buf,
				  map.len, &map.sva, &map.slen);
		if (ret)
			break;

		/* Fill in return struct */
		if (copy_to_user(uarg, &map, sizeof(map))) {
			ret = -EFAULT;
			api_unmap_wsm(client, map.sid,
				      (void *)(uintptr_t)map.buf, map.sva,
				      map.slen);
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

		ret = api_unmap_wsm(client, map.sid, (void *)(uintptr_t)map.buf,
				    map.sva, map.slen);
		break;
	}
	case MC_IO_ERR: {
		struct mc_ioctl_geterr *uerr = (struct mc_ioctl_geterr *)uarg;
		uint32_t sid;
		int32_t error;

		if (get_user(sid, &uerr->sid)) {
			ret = -EFAULT;
			break;
		}

		ret = api_get_session_error(client, sid, &error);
		if (ret)
			break;

		/* Fill in return struct */
		if (put_user(error, &uerr->value)) {
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
	case MC_IO_DR_VERSION:
		ret = put_user(mc_dev_get_version(), uarg);
		break;
	default:
		MCDRV_ERROR("unsupported cmd=0x%x", id);
		ret = -ENOIOCTLCMD;
		break;

	} /* end switch(id) */

out:
	mobicore_log_read();
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
	return api_close_device(client);
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

static inline int create_devices(void)
{
	int err = 0;
	struct device *dev;
	dev_t mc_dev_admin;

	cdev_init(&mc_user_cdev, &mc_user_fops);

	mc_device_class = class_create(THIS_MODULE, "mobicore");
	if (IS_ERR(mc_device_class)) {
		MCDRV_ERROR("failed to create device class");
		return PTR_ERR(mc_device_class);
	}

	/* Firstly create the ADMIN node */
	err = admin_dev_init(mc_device_class, &mc_dev_admin);
	if (err < 0) {
		MCDRV_ERROR("failed to init mobicore device");
		goto fail_admin_dev_init;
	}

	mc_dev_user = MKDEV(MAJOR(mc_dev_admin), 1);

	/* Then the user node */
	err = cdev_add(&mc_user_cdev, mc_dev_user, 1);
	if (err) {
		MCDRV_ERROR("user device register failed");
		goto fail_user_add;
	}
	mc_user_cdev.owner = THIS_MODULE;
	dev = device_create(mc_device_class, NULL, mc_dev_user, NULL,
			    MC_USER_DEVNODE);
	if (!IS_ERR(dev))
		return err;

	err = PTR_ERR(dev);
	cdev_del(&mc_user_cdev);

fail_user_add:
	admin_dev_cleanup(mc_device_class);
fail_admin_dev_init:
	class_destroy(mc_device_class);
	MCDRV_DBG("failed with %d", err);
	return err;
}

static void devices_cleanup(void)
{
	device_destroy(mc_device_class, mc_dev_user);
	cdev_del(&mc_user_cdev);

	admin_dev_cleanup(mc_device_class);

	class_destroy(mc_device_class);
}

/*
 * This function is called the kernel during startup or by a insmod command.
 * This device is installed and registered as cdev, then interrupt and
 * queue handling is set up
 */
static int __init mobicore_init(void)
{
	struct mc_version_info version_info;
	int err = 0;

	dev_set_name(g_ctx.mcd, "mcd");

	/* Do not remove or change the following trace.
	 * The string "MobiCore" is used to detect if <t-base is in of the image
	 */
	dev_info(g_ctx.mcd, "MobiCore mcDrvModuleApi version is %i.%i\n",
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

	init_rwsem(&g_ctx.mcp_lock);

	/* init lists and locks */
	INIT_LIST_HEAD(&g_ctx.clients);
	INIT_LIST_HEAD(&g_ctx.temp_nq);
	mutex_init(&g_ctx.clients_lock);

	INIT_LIST_HEAD(&g_ctx.closing_sess);
	mutex_init(&g_ctx.closing_lock);

	/* Init plenty of nice features */
	err = mc_fastcall_init();
	if (err)
		goto fail_fastcall_init;

	err = mc_pm_initialize();
	if (err) {
		MCDRV_ERROR("Power Management init failed!");
		goto fail_pm_init;
	}

	err = mc_pm_clock_initialize();
	if (err) {
		MCDRV_ERROR("Clock init failed!");
		goto fail_clock_init;
	}

	g_ctx.mcore_client = api_open_device(true);
	if (!g_ctx.mcore_client) {
		err = -ENOMEM;
		goto fail_create_client;
	}

	err = nqueue_init(txq_length, rxq_length);
	if (err)
		goto fail_nqueue_init;

	err = mc_dev_sched_init();
	if (err)
		goto fail_mc_device_sched_init;

	err = create_devices();
	if (err)
		goto fail_create_devs;

	err = irq_handler_init();
	if (err) {
		MCDRV_ERROR("Interrupts init failed!");
		goto fail_irq_init;
	}

	err = mobicore_log_setup();
	if (err) {
		MCDRV_ERROR("Log init failed!");
		goto fail_log_init;
	}

	err = mcp_get_version(&version_info);
	if (err)
		goto fail_mcp_cmd;

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

	switch (version_info.version_mci) {
	case MC_VERSION(1, 0):	/* 302 */
		g_ctx.f_mem_ext = true;
		g_ctx.f_ta_auth = true;
	case MC_VERSION(0, 7):
		g_ctx.f_timeout = true;
	case MC_VERSION(0, 6):	/* 301 */
		break;
	default:
		pr_err("Unsupported MCI version 0x%08x",
		       version_info.version_mci);
		goto fail_mcp_cmd;
	}

	return 0;

fail_mcp_cmd:
	mobicore_log_free();
fail_log_init:
	irq_handler_exit();
fail_irq_init:
	devices_cleanup();
fail_create_devs:
	mc_dev_sched_cleanup();
fail_mc_device_sched_init:
	nqueue_cleanup();
fail_nqueue_init:
	api_close_device(g_ctx.mcore_client);
	g_ctx.mcore_client = NULL;
fail_create_client:
	mc_pm_clock_finalize();
fail_clock_init:
	mc_pm_free();
fail_pm_init:
	mc_fastcall_cleanup();
fail_fastcall_init:
	return err;
}

/*
 * This function removes this device driver from the Linux device manager .
 */
static void mobicore_exit(void)
{
	MCDRV_DBG("enter");

	devices_cleanup();
	mc_dev_sched_cleanup();
	nqueue_cleanup();
	api_close_device(g_ctx.mcore_client);
	g_ctx.mcore_client = NULL;
	mc_pm_clock_finalize();
	mc_pm_free();
	irq_handler_exit();
	mobicore_log_free();
	mc_fastcall_cleanup();

	MCDRV_DBG("exit");
}

#if defined(MC_CRYPTO_CLOCK_MANAGEMENT) && defined(MC_USE_DEVICE_TREE)

static int mcd_probe(struct platform_device *pdev)
{
	g_ctx.mcd->of_node = pdev->dev.of_node;
	mobicore_init();
	return 0;
}

static int mcd_remove(struct platform_device *pdev)
{
	return 0;
}

static int mcd_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int mcd_resume(struct platform_device *pdev)
{
	return 0;
}

static struct of_device_id mcd_match[] = {
	{
		.compatible = "qcom,mcd",
	},
	{}
};

static struct platform_driver mc_plat_driver = {
	.probe = mcd_probe,
	.remove = mcd_remove,
	.suspend = mcd_suspend,
	.resume = mcd_resume,
	.driver = {
		.name = "mcd",
		.owner = THIS_MODULE,
		.of_match_table = mcd_match,
	},
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

#else

module_init(mobicore_init);
module_exit(mobicore_exit);

#endif

MODULE_AUTHOR("Trustonic Limited");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MobiCore driver");
