// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 TRUSTONIC LIMITED
 */

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/reboot.h>
#include <linux/suspend.h>

#include "public/mc_user.h"
#include "public/mc_admin.h"		/* MC_ADMIN_DEVNODE */

#include "platform.h"			/* MC_PM_RUNTIME */
#include "main.h"
#include "arm.h"
#include "admin.h"
#include "user.h"
#include "iwp.h"
#include "mcp.h"
#include "nq.h"
#include "client.h"
#include "xen_be.h"
#include "xen_fe.h"
#include "build_tag.h"

/* Default entry for our driver in device tree */
#ifndef MC_DEVICE_PROPNAME
#define MC_DEVICE_PROPNAME "trustonic,mobicore"
#endif

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

static struct {
	/* Device tree compatibility */
	bool use_platform_driver;
	/* TEE start return code mutex */
	struct mutex start_mutex;
	/* TEE start return code */
	int start_ret;
#ifdef MC_PM_RUNTIME
	/* Whether hibernation succeeded */
	bool did_hibernate;
	/* Reboot notifications */
	struct notifier_block reboot_notifier;
	/* PM notifications */
	struct notifier_block pm_notifier;
#endif
	/* Devices */
	dev_t device;
	struct class *class;
	/* Admin device */
	struct cdev admin_cdev;
	/* User device */
	dev_t user_dev;
	struct cdev user_cdev;
	/* Debug counters */
	struct mutex struct_counters_buf_mutex;
	char struct_counters_buf[256];
	int struct_counters_buf_len;
} main_ctx;

static int mobicore_start(void);
static void mobicore_stop(void);

static bool mobicore_ready;
bool is_mobicore_ready(void)
{
	return mobicore_ready;
}

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

static inline void kasnprintf_buf_reset(struct kasnprintf_buf *buf)
{
	kfree(buf->buf);
	buf->buf = NULL;
	buf->size = 0;
	buf->off = 0;
}

ssize_t debug_generic_read(struct file *file, char __user *user_buf,
			   size_t count, loff_t *ppos,
			   int (*function)(struct kasnprintf_buf *buf))
{
	struct kasnprintf_buf *buf = file->private_data;
	int ret = 0;

	mutex_lock(&buf->mutex);
	/* Add/update buffer */
	if (!*ppos) {
		kasnprintf_buf_reset(buf);
		ret = function(buf);
		if (ret < 0) {
			kasnprintf_buf_reset(buf);
			goto end;
		}
	}

	ret = simple_read_from_buffer(user_buf, count, ppos, buf->buf,
				      buf->off);

end:
	mutex_unlock(&buf->mutex);
	return ret;
}

int debug_generic_open(struct inode *inode, struct file *file)
{
	struct kasnprintf_buf *buf;

	file->private_data = kzalloc(sizeof(*buf), GFP_KERNEL);
	if (!file->private_data)
		return -ENOMEM;

	buf = file->private_data;
	mutex_init(&buf->mutex);
	buf->gfp = GFP_KERNEL;
	return 0;
}

int debug_generic_release(struct inode *inode, struct file *file)
{
	struct kasnprintf_buf *buf = file->private_data;

	if (!buf)
		return 0;

	kasnprintf_buf_reset(buf);
	kfree(buf);
	return 0;
}

static ssize_t debug_structs_read(struct file *file, char __user *user_buf,
				  size_t count, loff_t *ppos)
{
	return debug_generic_read(file, user_buf, count, ppos,
				  clients_debug_structs);
}

static const struct file_operations debug_structs_ops = {
	.read = debug_structs_read,
	.llseek = default_llseek,
	.open = debug_generic_open,
	.release = debug_generic_release,
};

static ssize_t debug_struct_counters_read(struct file *file,
					  char __user *user_buf,
					  size_t count, loff_t *ppos)
{
	if (!*ppos) {
		int ret;

		mutex_lock(&main_ctx.struct_counters_buf_mutex);
		ret = snprintf(main_ctx.struct_counters_buf,
			       sizeof(main_ctx.struct_counters_buf),
			       "clients:  %d\n"
			       "cbufs:    %d\n"
			       "cwsms:    %d\n"
			       "sessions: %d\n"
			       "swsms:    %d\n"
			       "mmus:     %d\n"
			       "maps:     %d\n"
			       "slots:    %d\n"
			       "xen maps: %d\n"
			       "xen fes:  %d\n",
			       atomic_read(&g_ctx.c_clients),
			       atomic_read(&g_ctx.c_cbufs),
			       atomic_read(&g_ctx.c_cwsms),
			       atomic_read(&g_ctx.c_sessions),
			       atomic_read(&g_ctx.c_wsms),
			       atomic_read(&g_ctx.c_mmus),
			       atomic_read(&g_ctx.c_maps),
			       atomic_read(&g_ctx.c_slots),
			       atomic_read(&g_ctx.c_xen_maps),
			       atomic_read(&g_ctx.c_xen_fes));
		mutex_unlock(&main_ctx.struct_counters_buf_mutex);
		if (ret > 0)
			main_ctx.struct_counters_buf_len = ret;
	}

	return simple_read_from_buffer(user_buf, count, ppos,
				       main_ctx.struct_counters_buf,
				       main_ctx.struct_counters_buf_len);
}

static const struct file_operations debug_struct_counters_ops = {
	.read = debug_struct_counters_read,
	.llseek = default_llseek,
};

static inline int device_user_init(void)
{
	struct device *dev;
	int ret = 0;

	main_ctx.user_dev = MKDEV(MAJOR(main_ctx.device), 1);
	/* Create the user node */
	mc_user_init(&main_ctx.user_cdev);
	ret = cdev_add(&main_ctx.user_cdev, main_ctx.user_dev, 1);
	if (ret) {
		mc_dev_err(ret, "user cdev_add failed");
		return ret;
	}

	main_ctx.user_cdev.owner = THIS_MODULE;
	dev = device_create(main_ctx.class, NULL, main_ctx.user_dev, NULL,
			    MC_USER_DEVNODE);
	if (IS_ERR(dev)) {
		ret = PTR_ERR(dev);
		cdev_del(&main_ctx.user_cdev);
		mc_dev_err(ret, "user device_create failed");
		return ret;
	}

	/* Create debugfs structs entry */
	debugfs_create_file("structs", 0400, g_ctx.debug_dir, NULL,
			    &debug_structs_ops);

	return 0;
}

static inline void device_user_exit(void)
{
	device_destroy(main_ctx.class, main_ctx.user_dev);
	cdev_del(&main_ctx.user_cdev);
}

#ifdef MC_PM_RUNTIME
static int reboot_notifier(struct notifier_block *nb, unsigned long event,
			   void *dummy)
{
	switch (event) {
	case SYS_HALT:
	case SYS_POWER_OFF:
		main_ctx.did_hibernate = true;
		break;
	}

	return 0;
}

static int suspend_notifier(struct notifier_block *nb, unsigned long event,
			    void *dummy)
{
	int ret = 0;

	main_ctx.did_hibernate = false;
	switch (event) {
	case PM_SUSPEND_PREPARE:
		return nq_suspend();
	case PM_POST_SUSPEND:
		return nq_resume();
#ifdef TRUSTONIC_HIBERNATION_SUPPORT
	case PM_HIBERNATION_PREPARE:
		/* Try to stop the TEE nicely (ignore failure) */
		nq_stop();
		break;
	case PM_POST_HIBERNATION:
		if (main_ctx.did_hibernate) {
			/* Really did hibernate */
			client_cleanup();
			main_ctx.start_ret = TEE_START_NOT_TRIGGERED;
			return mobicore_start();
		}

		/* Did not hibernate, just restart the TEE */
		ret = nq_start();
#endif
	}

	return ret;
}
#endif /* MC_PM_RUNTIME */

static inline int check_version(void)
{
	struct mc_version_info version_info;
	int ret;

	/* Must be called before creating the user device node to avoid race */
	ret = mcp_get_version(&version_info);
	if (ret)
		return ret;

	/* CMP version is meaningless in this case and is thus not printed */
	mc_dev_info("\n"
		    "    product_id        = %s\n"
		    "    version_mci       = 0x%08x\n"
		    "    version_so        = 0x%08x\n"
		    "    version_mclf      = 0x%08x\n"
		    "    version_container = 0x%08x\n"
		    "    version_mc_config = 0x%08x\n"
		    "    version_tl_api    = 0x%08x\n"
		    "    version_dr_api    = 0x%08x\n"
		    "    version_nwd       = 0x%08x\n",
		    version_info.product_id,
		    version_info.version_mci,
		    version_info.version_so,
		    version_info.version_mclf,
		    version_info.version_container,
		    version_info.version_mc_config,
		    version_info.version_tl_api,
		    version_info.version_dr_api,
		    version_info.version_nwd);

	/* Determine which features are supported */
	if (version_info.version_mci != MC_VERSION(1, 7)) {
		ret = -EHOSTDOWN;
		mc_dev_err(ret, "TEE incompatible with this driver");
		return ret;
	}

	return 0;
}

static int mobicore_start_domu(void)
{
	mutex_lock(&main_ctx.start_mutex);
	if (main_ctx.start_ret != TEE_START_NOT_TRIGGERED)
		goto end;

	/* Must be called before creating the user device node to avoid race */
	main_ctx.start_ret = check_version();
	if (main_ctx.start_ret)
		goto end;

	main_ctx.start_ret = device_user_init();
end:
	mutex_unlock(&main_ctx.start_mutex);
	return main_ctx.start_ret;
}

static int mobicore_start(void)
{
	int ret;

	mutex_lock(&main_ctx.start_mutex);
	if (main_ctx.start_ret != TEE_START_NOT_TRIGGERED)
		goto got_ret;

	ret = nq_start();
	if (ret) {
		mc_dev_err(ret, "NQ start failed");
		goto err_nq;
	}

	ret = mcp_start();
	if (ret) {
		mc_dev_err(ret, "MCP start failed");
		goto err_mcp;
	}

	ret = iwp_start();
	if (ret) {
		mc_dev_err(ret, "IWP start failed");
		goto err_iwp;
	}

	/* Must be called before creating the user device node to avoid race */
	ret = check_version();
	if (ret)
		goto err_version;

#ifdef MC_PM_RUNTIME
	main_ctx.reboot_notifier.notifier_call = reboot_notifier;
	ret = register_reboot_notifier(&main_ctx.reboot_notifier);
	if (ret) {
		mc_dev_err(ret, "reboot notifier registration failed");
		goto err_pm_notif;
	}

	main_ctx.pm_notifier.notifier_call = suspend_notifier;
	ret = register_pm_notifier(&main_ctx.pm_notifier);
	if (ret) {
		unregister_reboot_notifier(&main_ctx.reboot_notifier);
		mc_dev_err(ret, "PM notifier register failed");
		goto err_pm_notif;
	}
#endif

	if (is_xen_dom0()) {
		ret = xen_be_init();
		if (ret)
			goto err_xen_be;
	}

	ret = device_user_init();
	if (ret)
		goto err_device_user;

	main_ctx.start_ret = 0;
	mobicore_ready = true;
	goto got_ret;

err_device_user:
	if (is_xen_dom0())
		xen_be_exit();
err_xen_be:
#ifdef MC_PM_RUNTIME
	unregister_reboot_notifier(&main_ctx.reboot_notifier);
	unregister_pm_notifier(&main_ctx.pm_notifier);
err_pm_notif:
#endif
err_version:
	iwp_stop();
err_iwp:
	mcp_stop();
err_mcp:
	nq_stop();
err_nq:
	main_ctx.start_ret = ret;
got_ret:
	mutex_unlock(&main_ctx.start_mutex);
	return main_ctx.start_ret;
}

static void mobicore_stop(void)
{
	device_user_exit();
	if (is_xen_dom0())
		xen_be_exit();

	if (!is_xen_domu()) {
#ifdef MC_PM_RUNTIME
		unregister_reboot_notifier(&main_ctx.reboot_notifier);
		unregister_pm_notifier(&main_ctx.pm_notifier);
#endif
		iwp_stop();
		mcp_stop();
		nq_stop();
	}
}

int mc_wait_tee_start(void)
{
	int ret;

	while (!is_mobicore_ready())
		ssleep(1);

	mutex_lock(&main_ctx.start_mutex);
	while (main_ctx.start_ret == TEE_START_NOT_TRIGGERED) {
		mutex_unlock(&main_ctx.start_mutex);
		ssleep(1);
		mutex_lock(&main_ctx.start_mutex);
	}

	ret = main_ctx.start_ret;
	mutex_unlock(&main_ctx.start_mutex);
	return ret;
}

static inline int device_common_init(void)
{
	int ret;

	ret = alloc_chrdev_region(&main_ctx.device, 0, 2, "trustonic_tee");
	if (ret) {
		mc_dev_err(ret, "alloc_chrdev_region failed");
		return ret;
	}

	main_ctx.class = class_create(THIS_MODULE, "trustonic_tee");
	if (IS_ERR(main_ctx.class)) {
		ret = PTR_ERR(main_ctx.class);
		mc_dev_err(ret, "class_create failed");
		unregister_chrdev_region(main_ctx.device, 2);
		return ret;
	}

	return 0;
}

static inline void device_common_exit(void)
{
	class_destroy(main_ctx.class);
	unregister_chrdev_region(main_ctx.device, 2);
}

static inline int device_admin_init(void)
{
	struct device *dev;
	int ret = 0;

	/* Create the ADMIN node */
	ret = mc_admin_init(&main_ctx.admin_cdev, mobicore_start,
			    mobicore_stop);
	if (ret)
		goto err_init;

	ret = cdev_add(&main_ctx.admin_cdev, main_ctx.device, 1);
	if (ret) {
		mc_dev_err(ret, "admin cdev_add failed");
		goto err_cdev;
	}

	main_ctx.admin_cdev.owner = THIS_MODULE;
	dev = device_create(main_ctx.class, NULL, main_ctx.device, NULL,
			    MC_ADMIN_DEVNODE);
	if (IS_ERR(dev)) {
		ret = PTR_ERR(dev);
		mc_dev_err(ret, "admin device_create failed");
		goto err_device;
	}

	return 0;

err_device:
	cdev_del(&main_ctx.admin_cdev);
err_cdev:
	mc_admin_exit();
err_init:
	return ret;
}

static inline void device_admin_exit(void)
{
	device_destroy(main_ctx.class, main_ctx.device);
	cdev_del(&main_ctx.admin_cdev);
	mc_admin_exit();
}

/*
 * This function is called by the kernel during startup or by a insmod command.
 * This device is installed and registered as cdev, then interrupt and
 * queue handling is set up
 */
static int mobicore_probe(struct platform_device *pdev)
{
	int ret = 0;

	if (pdev)
		g_ctx.mcd->of_node = pdev->dev.of_node;

#ifdef MOBICORE_COMPONENT_BUILD_TAG
	mc_dev_info("MobiCore %s", MOBICORE_COMPONENT_BUILD_TAG);
#endif
	/* Hardware does not support ARM TrustZone -> Cannot continue! */
	if (!is_xen_domu() && !has_security_extensions()) {
		ret = -ENODEV;
		mc_dev_err(ret, "Hardware doesn't support ARM TrustZone!");
		return ret;
	}

	/* Running in secure mode -> Cannot load the driver! */
	if (is_secure_mode()) {
		ret = -ENODEV;
		mc_dev_err(ret, "Running in secure MODE!");
		return ret;
	}

	/* Make sure we can create debugfs entries */
	g_ctx.debug_dir = debugfs_create_dir("trustonic_tee", NULL);

	/* Initialize debug counters */
	atomic_set(&g_ctx.c_clients, 0);
	atomic_set(&g_ctx.c_cbufs, 0);
	atomic_set(&g_ctx.c_cwsms, 0);
	atomic_set(&g_ctx.c_sessions, 0);
	atomic_set(&g_ctx.c_wsms, 0);
	atomic_set(&g_ctx.c_mmus, 0);
	atomic_set(&g_ctx.c_maps, 0);
	atomic_set(&g_ctx.c_slots, 0);
	atomic_set(&g_ctx.c_xen_maps, 0);
	atomic_set(&g_ctx.c_xen_fes, 0);
	main_ctx.start_ret = TEE_START_NOT_TRIGGERED;
	mutex_init(&main_ctx.start_mutex);
	mutex_init(&main_ctx.struct_counters_buf_mutex);
	/* Create debugfs info entries */
	debugfs_create_file("structs_counters", 0400, g_ctx.debug_dir, NULL,
			    &debug_struct_counters_ops);

	/* Initialize common API layer */
	client_init();

	/* Initialize plenty of nice features */
	ret = nq_init();
	if (ret) {
		mc_dev_err(ret, "NQ init failed");
		goto fail_nq_init;
	}

	ret = mcp_init();
	if (ret) {
		mc_dev_err(ret, "MCP init failed");
		goto err_mcp;
	}

	ret = iwp_init();
	if (ret) {
		mc_dev_err(ret, "IWP init failed");
		goto err_iwp;
	}

	ret = device_common_init();
	if (ret)
		goto err_common;

	if (!is_xen_domu()) {
		/* Admin dev is for the daemon to communicate with the driver */
		ret = device_admin_init();
		if (ret)
			goto err_admin;

#ifndef MC_DELAYED_TEE_START
		ret = mobicore_start();
#endif
		if (ret)
			goto err_start;
	}

	return 0;

err_start:
	device_admin_exit();
err_admin:
	device_common_exit();
err_common:
	iwp_exit();
err_iwp:
	mcp_exit();
err_mcp:
	nq_exit();
fail_nq_init:
	debugfs_remove_recursive(g_ctx.debug_dir);
	return ret;
}

static int mobicore_probe_not_of(void)
{
	return mobicore_probe(NULL);
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

static int __init mobicore_init(void)
{
	dev_set_name(g_ctx.mcd, "TEE");
	/*
	 * Do not remove or change the following trace.
	 * The string "MobiCore" is used to detect if the TEE is in of the image
	 */
	mc_dev_info("MobiCore mcDrvModuleApi version is %d.%d",
		    MCDRVMODULEAPI_VERSION_MAJOR,
		    MCDRVMODULEAPI_VERSION_MINOR);

	/* In a Xen DomU, just register the front-end */
	if (is_xen_domu())
		return xen_fe_init(mobicore_probe_not_of, mobicore_start_domu);

	main_ctx.use_platform_driver =
		of_find_compatible_node(NULL, NULL, MC_DEVICE_PROPNAME);
	if (main_ctx.use_platform_driver)
		return platform_driver_register(&mc_plat_driver);

	return mobicore_probe_not_of();
}

static void __exit mobicore_exit(void)
{
	if (is_xen_domu())
		xen_fe_exit();

	if (main_ctx.use_platform_driver)
		platform_driver_unregister(&mc_plat_driver);

	if (!is_xen_domu())
		device_admin_exit();

	device_common_exit();
	iwp_exit();
	mcp_exit();
	nq_exit();
	debugfs_remove_recursive(g_ctx.debug_dir);
}

module_init(mobicore_init);
module_exit(mobicore_exit);

MODULE_AUTHOR("Trustonic Limited");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MobiCore driver");
