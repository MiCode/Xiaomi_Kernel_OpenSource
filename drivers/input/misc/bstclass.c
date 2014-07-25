/*!
 * @section LICENSE
 * (C) Copyright 2013 Bosch Sensortec GmbH All Rights Reserved
 *
 * This software program is licensed subject to the GNU General
 * Public License (GPL).Version 2,June 1991,
 * available at http://www.fsf.org/copyleft/gpl.html
 *
 * @filename bstclass.c
 * @date     "Wed Feb 19 13:22:52 2014 +0800"
 * @id       "6d7c0bb"
 *
 * @brief
 * The core code of bst device driver
*/
#include <linux/init.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/poll.h>
#include <linux/mutex.h>
#include <linux/rcupdate.h>
#include <linux/compiler.h>
#include <linux/compat.h>
#include "bstclass.h"

static LIST_HEAD(bst_dev_list);

/*
 * bst_mutex protects access to both bst_dev_list and input_handler_list.
 * This also causes bst_[un]register_device and bst_[un]register_handler
 * be mutually exclusive which simplifies locking in drivers implementing
 * input handlers.
 */
static DEFINE_MUTEX(bst_mutex);


static void bst_dev_release(struct device *device)
{
	struct bst_dev *dev = to_bst_dev(device);
	if (NULL != dev)
		kfree(dev);
	module_put(THIS_MODULE);
}


#ifdef CONFIG_PM
static int bst_dev_suspend(struct device *dev)
{
	return 0;
}

static int bst_dev_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops bst_dev_pm_ops = {
	.suspend    = bst_dev_suspend,
	.resume     = bst_dev_resume,
	.poweroff   = bst_dev_suspend,
	.restore    = bst_dev_resume,
};
#endif /* CONFIG_PM */

static const struct attribute_group *bst_dev_attr_groups[] = {
	NULL
};

static struct device_type bst_dev_type = {
	.groups      = bst_dev_attr_groups,
	.release = bst_dev_release,
#ifdef CONFIG_PM
	.pm      = &bst_dev_pm_ops,
#endif
};



static char *bst_devnode(struct device *dev, umode_t *mode)
{
	return kasprintf(GFP_KERNEL, "%s", dev_name(dev));
}

struct class bst_class = {
	.name        = "bst",
	.owner       = THIS_MODULE,
	.devnode     = bst_devnode,
	.dev_release = bst_dev_release,
};
EXPORT_SYMBOL_GPL(bst_class);

/**
 * bst_allocate_device - allocate memory for new input device
 *
 * Returns prepared struct bst_dev or NULL.
 *
 * NOTE: Use bst_free_device() to free devices that have not been
 * registered; bst_unregister_device() should be used for already
 * registered devices.
 */
struct bst_dev *bst_allocate_device(void)
{
	struct bst_dev *dev;

	dev = kzalloc(sizeof(struct bst_dev), GFP_KERNEL);
	if (dev) {
		dev->dev.type = &bst_dev_type;
		dev->dev.class = &bst_class;
		device_initialize(&dev->dev);
		mutex_init(&dev->mutex);
		INIT_LIST_HEAD(&dev->node);
		__module_get(THIS_MODULE);
	}
	return dev;
}
EXPORT_SYMBOL(bst_allocate_device);



/**
 * bst_free_device - free memory occupied by bst_dev structure
 * @dev: input device to free
 *
 * This function should only be used if bst_register_device()
 * was not called yet or if it failed. Once device was registered
 * use bst_unregister_device() and memory will be freed once last
 * reference to the device is dropped.
 *
 * Device should be allocated by bst_allocate_device().
 *
 * NOTE: If there are references to the input device then memory
 * will not be freed until last reference is dropped.
 */
void bst_free_device(struct bst_dev *dev)
{
	if (dev)
		bst_put_device(dev);
}
EXPORT_SYMBOL(bst_free_device);

/**
 * bst_register_device - register device with input core
 * @dev: device to be registered
 *
 * This function registers device with input core. The device must be
 * allocated with bst_allocate_device() and all it's capabilities
 * set up before registering.
 * If function fails the device must be freed with bst_free_device().
 * Once device has been successfully registered it can be unregistered
 * with bst_unregister_device(); bst_free_device() should not be
 * called in this case.
 */
int bst_register_device(struct bst_dev *dev)
{
	const char *path;
	int error;


	/*
	 * If delay and period are pre-set by the driver, then autorepeating
	 * is handled by the driver itself and we don't do it in input.c.
	 */
	dev_set_name(&dev->dev, dev->name);

	error = device_add(&dev->dev);
	if (error)
		return error;

	path = kobject_get_path(&dev->dev.kobj, GFP_KERNEL);
	dev_dbg(&dev->dev, "%s as %s\n",
			dev->name ? dev->name : "Unspecified device",
			path ? path : "N/A");
	kfree(path);
	error = mutex_lock_interruptible(&bst_mutex);
	if (error) {
		device_del(&dev->dev);
		return error;
	}

	list_add_tail(&dev->node, &bst_dev_list);

	mutex_unlock(&bst_mutex);
	return 0;
}
EXPORT_SYMBOL(bst_register_device);

/**
 * bst_unregister_device - unregister previously registered device
 * @dev: device to be unregistered
 *
 * This function unregisters an input device. Once device is unregistered
 * the caller should not try to access it as it may get freed at any moment.
 */
void bst_unregister_device(struct bst_dev *dev)
{
	int error;

	error = mutex_lock_interruptible(&bst_mutex);
	list_del_init(&dev->node);
	if (!error)
		mutex_unlock(&bst_mutex);
	device_unregister(&dev->dev);
}
EXPORT_SYMBOL(bst_unregister_device);

static int bst_open_file(struct inode *inode, struct file *file)
{
	const struct file_operations *old_fops, *new_fops = NULL;
	int err;

	/*
	 * That's _really_ odd. Usually NULL ->open means "nothing special",
	 * not "no device". Oh, well...
	 */
	if (!new_fops || !new_fops->open) {
		fops_put(new_fops);
		err = -ENODEV;
		goto out;
	}

	old_fops = file->f_op;
	file->f_op = new_fops;

	err = new_fops->open(inode, file);
	if (err) {
		fops_put(file->f_op);
		file->f_op = fops_get(old_fops);
	}
	fops_put(old_fops);
out:
	return err;
}

static const struct file_operations bst_fops = {
	.owner = THIS_MODULE,
	.open = bst_open_file,
	/*.llseek = noop_llseek,*/
};

static int __init bst_init(void)
{
	int err;
	/*bst class register*/
	err = class_register(&bst_class);
	if (err) {
		pr_err("unable to register bst_dev class\n");
		return err;
	}
	return err;
}

static void __exit bst_exit(void)
{
	/*bst class*/
	class_unregister(&bst_class);
}

/*subsys_initcall(bst_init);*/

MODULE_AUTHOR("contact@bosch-sensortec.com");
MODULE_DESCRIPTION("BST CLASS CORE");
MODULE_LICENSE("GPL V2");

module_init(bst_init);
module_exit(bst_exit);
