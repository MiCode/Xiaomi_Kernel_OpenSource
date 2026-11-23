#include <linux/init.h>
#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/types.h> 

#include "powerinsight_gpu_stats.h"
#include "powerinsight_modem_stats.h"
#include "utils/powerinsight_utils.h"
#include <linux/printk.h>
#include "powerinsight_plat.h"
#include "powerinsight_ioctl.h"

#define POWERINSIGHT_MAGIC 'k'

int powerinsight_register_module_ops(enum powerinsight_module_t module, void *mops)
{
	int ret = -EINVAL;

	switch (module) {
	case POWERINSIGHT_MODULE_GPU:
		ret = powerinsight_gpu_register_ops(mops);
		break;
	default:
		break;
	}

	return ret;
}
EXPORT_SYMBOL(powerinsight_register_module_ops);

int powerinsight_unregister_module_ops(enum powerinsight_module_t module)
{
	int ret = -EINVAL;

	switch (module) {
	case POWERINSIGHT_MODULE_GPU:
		ret = powerinsight_gpu_unregister_ops();
		break;
	default:
		break;
	}

	return ret;
}
EXPORT_SYMBOL(powerinsight_unregister_module_ops);

static long powerinsight_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int rc;
	void __user *argp = (void __user *)arg;

	rc = 0;
	switch (_IOC_TYPE(cmd)) {
	case POWERINSIGHT_IOC_GPU:
		rc = powerinsight_ioctl_gpu(cmd, argp);
		break;
	case POWERINSIGHT_IOC_MODEM:
		rc = powerinsight_ioctl_modem(cmd, argp);
		break;
	default:
		rc = -EINVAL;
		break;
	}
	return rc;
}

#ifdef CONFIG_COMPAT
static long powerinsight_compat_ioctl(struct file *filp,
			unsigned int cmd, unsigned long arg)
{
	return powerinsight_ioctl(filp, cmd, (unsigned long) compat_ptr(arg));
}
#endif

static int powerinsight_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int powerinsight_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static const struct file_operations powerinsight_device_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = powerinsight_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= powerinsight_compat_ioctl,
#endif
	.open = powerinsight_open,
	.release = powerinsight_release,
};

static struct miscdevice powerinsight_device = {
	.name = "powerinsight",
	.fops = &powerinsight_device_fops,
	.minor = MISC_DYNAMIC_MINOR,
};

static int __init powerinsight_init(void)
{
	int ret = 0;

	powerinsight_gpu_stats_init();
	powerinsight_modem_init();
	ret = misc_register(&powerinsight_device);
	if (ret) {
		pr_err("Failed to register powerinsight device (error: %d)\n", ret);
		return ret;
	}

	return 0;
}

static void __exit powerinsight_exit(void)
{
	powerinsight_gpu_stats_exit();
	powerinsight_modem_exit();
	misc_deregister(&powerinsight_device);
}

late_initcall_sync(powerinsight_init);
module_exit(powerinsight_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("xuyouwei@xiaomi.com");
MODULE_DESCRIPTION("PowerInsight");
