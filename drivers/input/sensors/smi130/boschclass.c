/*!
 * @section LICENSE
 * (C) Copyright 2011~2016 Bosch Sensortec GmbH All Rights Reserved
 *
 * (C) Modification Copyright 2018 Robert Bosch Kft  All Rights Reserved
 *
 * This software program is licensed subject to the GNU General
 * Public License (GPL).Version 2,June 1991,
 * available at http://www.fsf.org/copyleft/gpl.html
 *
 * Special: Description of the Software:
 *
 * This software module (hereinafter called "Software") and any
 * information on application-sheets (hereinafter called "Information") is
 * provided free of charge for the sole purpose to support your application
 * work. 
 *
 * As such, the Software is merely an experimental software, not tested for
 * safety in the field and only intended for inspiration for further development 
 * and testing. Any usage in a safety-relevant field of use (like automotive,
 * seafaring, spacefaring, industrial plants etc.) was not intended, so there are
 * no precautions for such usage incorporated in the Software.
 * 
 * The Software is specifically designed for the exclusive use for Bosch
 * Sensortec products by personnel who have special experience and training. Do
 * not use this Software if you do not have the proper experience or training.
 * 
 * This Software package is provided as is and without any expressed or
 * implied warranties, including without limitation, the implied warranties of
 * merchantability and fitness for a particular purpose.
 * 
 * Bosch Sensortec and their representatives and agents deny any liability for
 * the functional impairment of this Software in terms of fitness, performance
 * and safety. Bosch Sensortec and their representatives and agents shall not be
 * liable for any direct or indirect damages or injury, except as otherwise
 * stipulated in mandatory applicable law.
 * The Information provided is believed to be accurate and reliable. Bosch
 * Sensortec assumes no responsibility for the consequences of use of such
 * Information nor for any infringement of patents or other rights of third
 * parties which may result from its use.
 * 
 *------------------------------------------------------------------------------
 * The following Product Disclaimer does not apply to the BSX4-HAL-4.1NoFusion Software 
 * which is licensed under the Apache License, Version 2.0 as stated above.  
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Product Disclaimer
 *
 * Common:
 *
 * Assessment of Products Returned from Field
 *
 * Returned products are considered good if they fulfill the specifications / 
 * test data for 0-mileage and field listed in this document.
 *
 * Engineering Samples
 * 
 * Engineering samples are marked with (e) or (E). Samples may vary from the
 * valid technical specifications of the series product contained in this
 * data sheet. Therefore, they are not intended or fit for resale to
 * third parties or for use in end products. Their sole purpose is internal
 * client testing. The testing of an engineering sample may in no way replace
 * the testing of a series product. Bosch assumes no liability for the use
 * of engineering samples. The purchaser shall indemnify Bosch from all claims
 * arising from the use of engineering samples.
 *
 * Intended use
 *
 * Provided that SMI130 is used within the conditions (environment, application,
 * installation, loads) as described in this TCD and the corresponding
 * agreed upon documents, Bosch ensures that the product complies with
 * the agreed properties. Agreements beyond this require
 * the written approval by Bosch. The product is considered fit for the intended
 * use when the product successfully has passed the tests
 * in accordance with the TCD and agreed upon documents.
 *
 * It is the responsibility of the customer to ensure the proper application
 * of the product in the overall system/vehicle.
 *
 * Bosch does not assume any responsibility for changes to the environment
 * of the product that deviate from the TCD and the agreed upon documents 
 * as well as all applications not released by Bosch
  *
 * The resale and/or use of products are at the purchaser’s own risk and 
 * responsibility. The examination and testing of the SMI130 
 * is the sole responsibility of the purchaser.
 *
 * The purchaser shall indemnify Bosch from all third party claims 
 * arising from any product use not covered by the parameters of 
 * this product data sheet or not approved by Bosch and reimburse Bosch 
 * for all costs and damages in connection with such claims.
 *
 * The purchaser must monitor the market for the purchased products,
 * particularly with regard to product safety, and inform Bosch without delay
 * of all security relevant incidents.
 *
 * Application Examples and Hints
 *
 * With respect to any application examples, advice, normal values
 * and/or any information regarding the application of the device,
 * Bosch hereby disclaims any and all warranties and liabilities of any kind,
 * including without limitation warranties of
 * non-infringement of intellectual property rights or copyrights
 * of any third party.
 * The information given in this document shall in no event be regarded 
 * as a guarantee of conditions or characteristics. They are provided
 * for illustrative purposes only and no evaluation regarding infringement
 * of intellectual property rights or copyrights or regarding functionality,
 * performance or error has been made.
 *
 * @filename boschclass.c
 * @date     2015/11/17 13:44
 * @Modification Date 2018/08/28 18:20
 * @id       "836294d"
 * @version  1.5.9
 *
 * @brief    
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
#include "boschclass.h"
#include "bs_log.h"

static LIST_HEAD(bosch_dev_list);

/*
 * bosch_mutex protects access to both bosch_dev_list and input_handler_list.
 * This also causes bosch_[un]register_device and bosch_[un]register_handler
 * be mutually exclusive which simplifies locking in drivers implementing
 * input handlers.
 */
static DEFINE_MUTEX(bosch_mutex);


static void bosch_dev_release(struct device *device)
{
	struct bosch_dev *dev = to_bosch_dev(device);
	if (NULL != dev)
		kfree(dev);
	module_put(THIS_MODULE);
}


#ifdef CONFIG_PM
static int bosch_dev_suspend(struct device *dev)
{
	return 0;
}

static int bosch_dev_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops bosch_dev_pm_ops = {
	.suspend    = bosch_dev_suspend,
	.resume     = bosch_dev_resume,
	.poweroff   = bosch_dev_suspend,
	.restore    = bosch_dev_resume,
};
#endif /* CONFIG_PM */

static const struct attribute_group *bosch_dev_attr_groups[] = {
	NULL
};

static struct device_type bosch_dev_type = {
	.groups      = bosch_dev_attr_groups,
	.release = bosch_dev_release,
#ifdef CONFIG_PM
	.pm      = &bosch_dev_pm_ops,
#endif
};



static char *bosch_devnode(struct device *dev, mode_t *mode)
{
	return kasprintf(GFP_KERNEL, "%s", dev_name(dev));
}

struct class bosch_class = {
	.name        = "bosch",
	.owner       = THIS_MODULE,
	.devnode     = (void*)bosch_devnode,
	.dev_release = bosch_dev_release,
};
EXPORT_SYMBOL_GPL(bosch_class);

/**
 * bosch_allocate_device - allocate memory for new input device
 *
 * Returns prepared struct bosch_dev or NULL.
 *
 * NOTE: Use bosch_free_device() to free devices that have not been
 * registered; bosch_unregister_device() should be used for already
 * registered devices.
 */
struct bosch_dev *bosch_allocate_device(void)
{
	struct bosch_dev *dev;

	dev = kzalloc(sizeof(struct bosch_dev), GFP_KERNEL);
	if (dev) {
		dev->dev.type = &bosch_dev_type;
		dev->dev.class = &bosch_class;
		device_initialize(&dev->dev);
		mutex_init(&dev->mutex);
		INIT_LIST_HEAD(&dev->node);
		__module_get(THIS_MODULE);
	}
	return dev;
}
EXPORT_SYMBOL(bosch_allocate_device);



/**
 * bosch_free_device - free memory occupied by bosch_dev structure
 * @dev: input device to free
 *
 * This function should only be used if bosch_register_device()
 * was not called yet or if it failed. Once device was registered
 * use bosch_unregister_device() and memory will be freed once last
 * reference to the device is dropped.
 *
 * Device should be allocated by bosch_allocate_device().
 *
 * NOTE: If there are references to the input device then memory
 * will not be freed until last reference is dropped.
 */
void bosch_free_device(struct bosch_dev *dev)
{
	if (dev)
		bosch_put_device(dev);
}
EXPORT_SYMBOL(bosch_free_device);

/**
 * bosch_register_device - register device with input core
 * @dev: device to be registered
 *
 * This function registers device with input core. The device must be
 * allocated with bosch_allocate_device() and all it's capabilities
 * set up before registering.
 * If function fails the device must be freed with bosch_free_device().
 * Once device has been successfully registered it can be unregistered
 * with bosch_unregister_device(); bosch_free_device() should not be
 * called in this case.
 */
int bosch_register_device(struct bosch_dev *dev)
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
	PINFO("%s as %s\n",
			dev->name ? dev->name : "Unspecified device",
			path ? path : "N/A");
	kfree(path);
	error = mutex_lock_interruptible(&bosch_mutex);
	if (error) {
		device_del(&dev->dev);
		return error;
	}

	list_add_tail(&dev->node, &bosch_dev_list);

	mutex_unlock(&bosch_mutex);
	return 0;
}
EXPORT_SYMBOL(bosch_register_device);

/**
 * bosch_unregister_device - unregister previously registered device
 * @dev: device to be unregistered
 *
 * This function unregisters an input device. Once device is unregistered
 * the caller should not try to access it as it may get freed at any moment.
 */
void bosch_unregister_device(struct bosch_dev *dev)
{
	int ret = 0;
	ret = mutex_lock_interruptible(&bosch_mutex);
	if(ret){
		return;
	}

	list_del_init(&dev->node);
	mutex_unlock(&bosch_mutex);
	device_unregister(&dev->dev);
}
EXPORT_SYMBOL(bosch_unregister_device);

static int __init bosch_init(void)
{
	int err;
	/*bosch class register*/
	err = class_register(&bosch_class);
	if (err) {
		pr_err("unable to register bosch_dev class\n");
		return err;
	}
	return err;
}

static void __exit bosch_exit(void)
{
	/*bosch class*/
	class_unregister(&bosch_class);
}

/*subsys_initcall(bosch_init);*/

MODULE_AUTHOR("contact@bosch-sensortec.com");
MODULE_DESCRIPTION("BST CLASS CORE");
MODULE_LICENSE("GPL V2");

module_init(bosch_init);
module_exit(bosch_exit);
