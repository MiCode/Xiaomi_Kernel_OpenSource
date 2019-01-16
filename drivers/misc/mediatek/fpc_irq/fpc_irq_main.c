/* Fingerprint Cards, Hybrid Touch sensor driver
 *
 * Copyright (c) 2014,2015 Fingerprint Cards AB <tech@fingerprints.com>
 * Copyright (C) 2018 XiaoMi, Inc.
 *
 *
 * Software license : "Dual BSD/GPL"
 * see <linux/module.h> and ./Documentation
 * for  details.
 *
*/

#define DEBUG

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>

#include <asm/siginfo.h>
#include <asm/uaccess.h>


#include <linux/spi/spi.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/kthread.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/rcupdate.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/version.h>
#include <linux/wakelock.h>

#include "fpc_irq_common.h"
#include "fpc_irq_ctrl.h"
#include "fpc_irq_supply.h"
#include "fpc_irq_pm.h"

#include <mach/mt_gpio.h>
#include <mach/mt_spi.h>
#include <mach/eint.h>
#include <cust_eint.h>
//#include <spi.h>

//#define FPC_WAKEUP_UEVENT

#ifndef CONFIG_OF
// #include <linux/xxxx/fpc_irq.h> // todo
#else
#include <linux/of.h>
#include "fpc_irq.h"
#endif
#include <linux/dev_info.h>
MODULE_AUTHOR("Fingerprint Cards AB <tech@fingerprints.com>");
MODULE_DESCRIPTION("FPC IRQ driver.");

MODULE_LICENSE("Dual BSD/GPL");

/* -------------------------------------------------------------------------- */
/* platform compatibility                                                     */
/* -------------------------------------------------------------------------- */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,8,0)
	#include <linux/interrupt.h>
	#include <linux/irqreturn.h>
	#include <linux/of_gpio.h>
#endif


/* -------------------------------------------------------------------------- */
/* global variables                                                           */
/* -------------------------------------------------------------------------- */


/* -------------------------------------------------------------------------- */
/* fpc data types                                                             */
/* -------------------------------------------------------------------------- */
struct fpc_irq_attribute {
	struct device_attribute attr;
	size_t offset;
};


/* -------------------------------------------------------------------------- */
/* fpc_irq driver constants                                                   */
/* -------------------------------------------------------------------------- */
#define FPC_IRQ_CLASS_NAME	"fpsensor_irq"
#define FPC_IRQ_WORKER_NAME	"fpc_irq_worker"

/* set '0' for dynamic assignment, or '> 0' for static assignment */
#define FPC_IRQ_MAJOR				0

/* -------------------------------------------------------------------------- */
/* function prototypes                                                        */
/* -------------------------------------------------------------------------- */
static int fpc_irq_init(void);

static void fpc_irq_exit(void);

static int fpc_irq_probe(struct platform_device *plat_dev);

static int fpc_irq_remove(struct platform_device *plat_dev);

static int fpc_irq_get_of_pdata(struct platform_device *dev,
				fpc_irq_pdata_t *pdata);

static int fpc_irq_platform_init(fpc_irq_data_t *fpc_irq_data,
				fpc_irq_pdata_t *pdata);

static int fpc_irq_platform_destroy(fpc_irq_data_t *fpc_irq_data);

static int fpc_irq_create_class(fpc_irq_data_t *fpc_irq_data);

static int fpc_irq_create_device(fpc_irq_data_t *fpc_irq_data);

static int fpc_irq_worker_init(fpc_irq_data_t *fpc_irq_data);

static int fpc_irq_worker_init(fpc_irq_data_t *fpc_irq_data);

static int fpc_irq_worker_goto_idle(fpc_irq_data_t *fpc_irq_data);

static int fpc_irq_worker_enable(fpc_irq_data_t *fpc_irq_data);

static int fpc_irq_worker_destroy(fpc_irq_data_t *fpc_irq_data);

static int fpc_irq_manage_sysfs_setup(fpc_irq_data_t *fpc_irq_data,
					bool create);

static int fpc_irq_manage_sysfs_pm(fpc_irq_data_t *fpc_irq_data,
					bool create);

int fpc_irq_wait_for_interrupt(fpc_irq_data_t *fpc_irq_data, int timeout);

void  fpc_irq_interrupt(void);

static ssize_t fpc_irq_show_attr_setup(struct device *dev,
					struct device_attribute *attr,
					char *buf);

static ssize_t fpc_irq_store_attr_setup(struct device *dev,
					struct device_attribute *attr,
					const char *buf,
					size_t count);

static ssize_t fpc_irq_show_attr_pm(struct device *dev,
				struct device_attribute *attr,
				char *buf);

static ssize_t fpc_irq_store_attr_pm(struct device *dev,
				struct device_attribute *attr,
				const char *buf,
				size_t count);

static int fpc_irq_worker_function(void *_fpc_irq_data);

static int fpc_irq_enable(fpc_irq_data_t *fpc_irq_data, int req_state);

extern void mt_spi_enable_clk(void);
extern void mt_spi_disable_clk(void);


/* -------------------------------------------------------------------------- */
/* External interface                                                         */
/* -------------------------------------------------------------------------- */
module_init(fpc_irq_init);
module_exit(fpc_irq_exit);

static struct platform_device *fpc_irq_platform_device;

static struct platform_driver fpc_irq_driver = {
	.driver	 = {
		.name		= FPC_IRQ_DEV_NAME,
		.owner		= THIS_MODULE,
	},
	.probe   = fpc_irq_probe,
	.remove  = fpc_irq_remove,
	.suspend = fpc_irq_pm_suspend,
	.resume  = fpc_irq_pm_resume
};

fpc_irq_data_t *fpc_irq = NULL;
//static struct wake_lock fpc_lock;
static int fpc_irq_device_count;
/* -------------------------------------------------------------------------- */
/* devfs                                                                      */
/* -------------------------------------------------------------------------- */
#define FPC_IRQ_ATTR(__grp, __field, __mode)				\
{									\
	.attr = __ATTR(__field, (__mode),				\
	fpc_irq_show_attr_##__grp,					\
	fpc_irq_store_attr_##__grp),					\
	.offset = offsetof(struct fpc_irq_##__grp, __field)		\
}

#define FPC_IRQ_DEV_ATTR(_grp, _field, _mode)				\
struct fpc_irq_attribute fpc_irq_attr_##_field =			\
					FPC_IRQ_ATTR(_grp, _field, (_mode))

#define DEVFS_MODE_RW (S_IWUSR|S_IWGRP|S_IWOTH|S_IRUSR|S_IRGRP|S_IROTH)
#define DEVFS_MODE_WO (S_IWUSR|S_IWGRP|S_IWOTH)
#define DEVFS_MODE_RO (S_IRUSR|S_IRGRP|S_IROTH)

static FPC_IRQ_DEV_ATTR(setup, dst_pid,		DEVFS_MODE_RW);
static FPC_IRQ_DEV_ATTR(setup, dst_signo,	DEVFS_MODE_RW);
static FPC_IRQ_DEV_ATTR(setup, enabled,		DEVFS_MODE_RW);
static FPC_IRQ_DEV_ATTR(setup,intr_enabled,		DEVFS_MODE_RW);
static FPC_IRQ_DEV_ATTR(setup,tac_init,		DEVFS_MODE_RW);
static FPC_IRQ_DEV_ATTR(setup,unlock_enabled,		DEVFS_MODE_RW);
static FPC_IRQ_DEV_ATTR(setup, test_trigger,	DEVFS_MODE_WO);
static FPC_IRQ_DEV_ATTR(setup, spi_clk_enable,	DEVFS_MODE_WO);
static FPC_IRQ_DEV_ATTR(setup, click_event,	DEVFS_MODE_WO);

static struct attribute *fpc_irq_setup_attrs[] = {
	&fpc_irq_attr_dst_pid.attr.attr,
	&fpc_irq_attr_dst_signo.attr.attr,
	&fpc_irq_attr_enabled.attr.attr,
	&fpc_irq_attr_intr_enabled.attr.attr,
	&fpc_irq_attr_tac_init.attr.attr,
	&fpc_irq_attr_unlock_enabled.attr.attr,
	&fpc_irq_attr_test_trigger.attr.attr,
    &fpc_irq_attr_spi_clk_enable.attr.attr,
	 &fpc_irq_attr_click_event.attr.attr,
	NULL
};

static const struct attribute_group fpc_irq_setup_attr_group = {
	.attrs = fpc_irq_setup_attrs,
	.name = "setup"
};

static FPC_IRQ_DEV_ATTR(pm, state,		DEVFS_MODE_RO);
static FPC_IRQ_DEV_ATTR(pm, supply_on,		DEVFS_MODE_RW);
static FPC_IRQ_DEV_ATTR(pm, hw_reset,		DEVFS_MODE_WO);
static FPC_IRQ_DEV_ATTR(pm, notify_enabled,	DEVFS_MODE_RW);
static FPC_IRQ_DEV_ATTR(pm, notify_ack,		DEVFS_MODE_WO);
static FPC_IRQ_DEV_ATTR(pm, wakeup_req,		DEVFS_MODE_WO);
static FPC_IRQ_DEV_ATTR(pm, irq_pin_val,	DEVFS_MODE_RO);

static struct attribute *fpc_irq_pm_attrs[] = {
	&fpc_irq_attr_state.attr.attr,
	&fpc_irq_attr_supply_on.attr.attr,
	&fpc_irq_attr_hw_reset.attr.attr,
	&fpc_irq_attr_notify_enabled.attr.attr,
	&fpc_irq_attr_notify_ack.attr.attr,
	&fpc_irq_attr_wakeup_req.attr.attr,
	&fpc_irq_attr_irq_pin_val.attr.attr,
	NULL
};

static const struct attribute_group fpc_irq_pm_attr_group = {
	.attrs = fpc_irq_pm_attrs,
	.name = "pm"
};

extern int check_valid_fpc(void);
/* -------------------------------------------------------------------------- */
/* function definitions                                                       */
/* -------------------------------------------------------------------------- */
static int fpc_irq_init(void)
{
	struct devinfo_struct *dev_fpc;
	printk(KERN_INFO "%s\n", __func__);
	dev_fpc = (struct devinfo_struct*)kmalloc(sizeof(struct devinfo_struct), GFP_KERNEL);
    
    dev_fpc->device_type = "Fingerprint";
    dev_fpc->device_vendor = "FPC";
    dev_fpc->device_ic = "1022";
    dev_fpc->device_version = DEVINFO_NULL;
    dev_fpc->device_module = DEVINFO_NULL;
    dev_fpc->device_info = DEVINFO_NULL;
    //LOG_INF("<%s:%d>devinfo_add[%d]dev[%x]\n", __func__, __LINE__, devinfo_add, dev_ofilm);
	
	if (check_valid_fpc() < 0)
	{
		dev_fpc->device_used = DEVINFO_UNUSED;
		DEVINFO_CHECK_ADD_DEVICE(dev_fpc);
		return EINVAL;
	}
	fpc_irq_platform_device = platform_device_register_simple(
							FPC_IRQ_DEV_NAME,
							0,
							NULL,
							0);

	if (IS_ERR(fpc_irq_platform_device))
		return PTR_ERR(fpc_irq_platform_device);
	dev_fpc->device_used = DEVINFO_USED;
	DEVINFO_CHECK_ADD_DEVICE(dev_fpc);
	return (platform_driver_register(&fpc_irq_driver) != 0)? EINVAL : 0;
}


/* -------------------------------------------------------------------------- */
static void fpc_irq_exit(void)
{
	printk(KERN_INFO "%s\n", __func__);
	
	platform_driver_unregister(&fpc_irq_driver);

	platform_device_unregister(fpc_irq_platform_device);
}

#ifdef FPC_WAKEUP_UEVENT
static void fpc_irq_uevent(struct work_struct *arg)
{
	fpc_irq_data_t *fpc_irq_data = fpc_irq;
	//int status;
	
	wake_lock(&fpc_irq_data->wake_lock);


	if(&fpc_irq_data->dev->kobj)
		{
		printk("%s change \n", __func__);
		kobject_uevent(&fpc_irq_data->dev->kobj, KOBJ_CHANGE);
		}


	wake_unlock(&fpc_irq_data->wake_lock);

}
#endif

/* -------------------------------------------------------------------------- */
static int fpc_irq_probe(struct platform_device *plat_dev)
{
	int error = 0;
	fpc_irq_data_t *fpc_irq_data = NULL;

	fpc_irq_pdata_t *pdata_ptr;
	fpc_irq_pdata_t pdata_of;
	
	//dev_info(&plat_dev->dev, "%s\n", __func__);
	printk( "%s enter\n", __func__);


	if (fpc_irq_check_instance(plat_dev->name) == 0)
	{
	        dev_err(&plat_dev->dev, "fpc_irq_check_instance failed in \n");
		//return 0;
        }
	fpc_irq_data = kzalloc(sizeof(*fpc_irq_data), GFP_KERNEL);

	if (!fpc_irq_data) {
		dev_err(&plat_dev->dev, "failed to allocate memory for struct fpc_irq_data\n");

		return -ENOMEM;
	}

	platform_set_drvdata(plat_dev, fpc_irq_data);

	fpc_irq_data->plat_dev = plat_dev;
	fpc_irq_data->dev = &plat_dev->dev;

	fpc_irq_data->pdata.irq_gpio = -EINVAL;
	fpc_irq_data->pdata.irq_no   = -EINVAL;
	fpc_irq_data->pdata.rst_gpio = -EINVAL;
	fpc_irq =  fpc_irq_data;
	fpc_irq_data->suspend_index = false;
	fpc_irq_data->init_index = false;
	fpc_irq_data->unlock_index = false;
	//fpc_irq_data->interrupt_sleep_index = flase;
	//wake_lock_init(&fpc_lock, WAKE_LOCK_SUSPEND, "fpc wakelock") ;

	init_waitqueue_head(&fpc_irq_data->wq_enable);
	init_waitqueue_head(&fpc_irq_data->wq_irq_return);

	//pdata_ptr = plat_dev->dev.platform_data;
	
	//if (!pdata_ptr) {
		error = fpc_irq_get_of_pdata(plat_dev, &pdata_of);
		pdata_ptr = (error) ? NULL : &pdata_of;
	//}

	if (error)
		goto err_1;

	if (!pdata_ptr) {
		dev_err(fpc_irq_data->dev,
				"%s: dev.platform_data is NULL.\n", __func__);
		error = -EINVAL;
	}

	if (error)
		goto err_1;

	error = fpc_irq_platform_init(fpc_irq_data, pdata_ptr);
	if (error)
		goto err_1;

	mt_set_gpio_out(FPC_RESET_GPIO, GPIO_OUT_ONE);
	udelay(100);
	mt_set_gpio_out(FPC_RESET_GPIO, GPIO_OUT_ZERO);
	mdelay(1);
	mt_set_gpio_out(FPC_RESET_GPIO, GPIO_OUT_ONE);
	udelay(1250);

	mt_set_gpio_out(FPC_RESET_GPIO, GPIO_OUT_ONE);
	udelay(100);
	mt_set_gpio_out(FPC_RESET_GPIO, GPIO_OUT_ZERO);
	mdelay(1);
	mt_set_gpio_out(FPC_RESET_GPIO, GPIO_OUT_ONE);
	udelay(1250);

	error = fpc_irq_create_class(fpc_irq_data);
	if (error)
		goto err_2;

	error = fpc_irq_manage_sysfs_setup(fpc_irq_data, true);
	if (error)
		goto err_3;

	error = fpc_irq_manage_sysfs_pm(fpc_irq_data, true);
	if (error)
		goto err_4;

       	error = fpc_irq_create_device(fpc_irq_data);
	if (error)
	{
		printk("fpc_create_device--error=%d\n",error);	
                goto err_5;
	}

	error = fpc_irq_supply_init(fpc_irq_data);
	if (error)
		goto err_6;

	error = fpc_irq_ctrl_init(fpc_irq_data, pdata_ptr);
	if (error)
		goto err_7;

	error = fpc_irq_pm_init(fpc_irq_data);
	if (error)
		goto err_8;

	error = fpc_irq_worker_init(fpc_irq_data);
	if (error)
		goto err_9;

#ifdef FPC_WAKEUP_UEVENT
        wake_lock_init(&fpc_irq_data->wake_lock, WAKE_LOCK_SUSPEND, "fingerprint_wakelock");
	INIT_WORK(&fpc_irq_data->irq_workthread, fpc_irq_uevent);
#endif

        //mt_spi_enable_clk();
	


	sema_init(&fpc_irq_data->mutex, 0);

	fpc_irq_data->setup.dst_pid      = -EINVAL;
	fpc_irq_data->setup.dst_signo    = -EINVAL;
	fpc_irq_data->setup.enabled      = false;
	fpc_irq_data->setup.test_trigger = 0;

	up(&fpc_irq_data->mutex);

	return 0;

err_9:
	fpc_irq_pm_destroy(fpc_irq_data);
err_8:
	fpc_irq_ctrl_destroy(fpc_irq_data);
err_7:
	fpc_irq_supply_destroy(fpc_irq_data);
err_6:
	if (!IS_ERR_OR_NULL(fpc_irq_data->dev))
		device_destroy(fpc_irq_data->class, fpc_irq_data->devno);
err_5:
	fpc_irq_manage_sysfs_pm(fpc_irq_data, false);
err_4:
	fpc_irq_manage_sysfs_setup(fpc_irq_data, false);
err_3:
	class_destroy(fpc_irq_data->class);
err_2:
	fpc_irq_platform_destroy(fpc_irq_data);
err_1:
	platform_set_drvdata(plat_dev, NULL);

	kfree(fpc_irq_data);

	return error;
}


/* -------------------------------------------------------------------------- */
static int fpc_irq_remove(struct platform_device *plat_dev)
{
	fpc_irq_data_t *fpc_irq_data;

	if (fpc_irq_check_instance(plat_dev->name) == 0)
		return 0;

	fpc_irq_data = platform_get_drvdata(plat_dev);

	fpc_irq_worker_destroy(fpc_irq_data);
//err_8:
	fpc_irq_pm_destroy(fpc_irq_data);
//err_7:
	fpc_irq_ctrl_destroy(fpc_irq_data);
//err_6:
	fpc_irq_supply_destroy(fpc_irq_data);
        device_destroy(fpc_irq_data->class, fpc_irq_data->devno);
//err_5:
	fpc_irq_manage_sysfs_pm(fpc_irq_data, false);
//err_4:
	fpc_irq_manage_sysfs_setup(fpc_irq_data, false);
//err_3:
	class_destroy(fpc_irq_data->class);
//err_2:
	fpc_irq_platform_destroy(fpc_irq_data);
//err_1:
	platform_set_drvdata(plat_dev, NULL);

	kfree(fpc_irq_data);

	return 0;
}


/* -------------------------------------------------------------------------- */

static int fpc_irq_get_of_pdata(struct platform_device *dev,
				fpc_irq_pdata_t *pdata)
{
	pdata->irq_gpio = FPC_IRQ_GPIO;  
//	pdata->irq_no   = FPC_INT_IRQNO;
    pdata->irq_no   = mt_gpio_to_irq(pdata->irq_gpio);
	pdata->rst_gpio = FPC_RESET_GPIO;

	return 0;
}


/* -------------------------------------------------------------------------- */
static int fpc_irq_platform_init(fpc_irq_data_t *fpc_irq_data,
				fpc_irq_pdata_t *pdata)
{
	int error = 0;
	
	//fpc1020->irq_gpio = pdata->irq_gpio;
	
	// set irq gpio
	fpc_irq_data->pdata.irq_gpio = pdata->irq_gpio;
	fpc_irq_data->pdata.irq_no =pdata->irq_no;
  	error = mt_set_gpio_mode(pdata->irq_gpio, GPIO_MODE_04);
	if (error != 0) {
		printk("mt_set_gpio_mode (eint) failed.error=%d\n",error);
	}

  	error = mt_set_gpio_dir(pdata->irq_gpio, GPIO_DIR_IN);
	if (error != 0) {
		printk("mt_set_gpio_dir (eint) failed.error=%d\n",error);
	}

  	error = mt_set_gpio_pull_enable(pdata->irq_gpio, GPIO_PULL_DISABLE);
	if (error != 0) {
		printk("mt_set_gpio_pull_enable (eint) failed.error=%d\n",error);
	}
	#if 1
	mt_eint_set_sens(pdata->irq_gpio, CUST_EINT_EDGE_SENSITIVE);
	mt_eint_set_polarity(pdata->irq_gpio, 1);
	mt_eint_set_hw_debounce(pdata->irq_gpio, 1);
//	mt_eint_registration(pdata->irq_no, CUST_EINT_FPC_TYPE, fpc_irq_interrupt,1);
        mt_eint_registration(pdata->irq_no, EINTF_TRIGGER_RISING, fpc_irq_interrupt,1);
        error = request_irq(fpc_irq_data->pdata.irq_no, fpc_irq_interrupt, IRQF_TRIGGER_RISING, "fpc_irq", fpc_irq_data);
	mt_eint_mask(pdata->irq_no);
	#endif
	
	return error;
}


/* -------------------------------------------------------------------------- */
static int fpc_irq_platform_destroy(fpc_irq_data_t *fpc_irq_data)
{
	dev_dbg(fpc_irq_data->dev, "%s\n", __func__);


	return 0;
}


/* -------------------------------------------------------------------------- */
static int fpc_irq_create_class(fpc_irq_data_t *fpc_irq_data)
{
	int error = 0;

	dev_dbg(fpc_irq_data->dev, "%s\n", __func__);

	fpc_irq_data->class = class_create(THIS_MODULE, FPC_IRQ_CLASS_NAME);

	if (IS_ERR(fpc_irq_data->class)) {
		dev_err(fpc_irq_data->dev, "failed to create class.\n");
		error = PTR_ERR(fpc_irq_data->class);
	}

	return error;
}

static int fpc_irq_create_device(fpc_irq_data_t *fpc_irq_data)
{
	int error = 0;

	if (FPC_IRQ_MAJOR > 0) {
		fpc_irq_data->devno = MKDEV(FPC_IRQ_MAJOR, fpc_irq_device_count++);

		error = register_chrdev_region(fpc_irq_data->devno,
						1,
						FPC_IRQ_DEV_NAME);
	} else {
		error = alloc_chrdev_region(&fpc_irq_data->devno,
					fpc_irq_device_count++,
					1,
					FPC_IRQ_DEV_NAME);
	}

	if (error < 0) {
		printk("%s: FAILED %d.\n", __func__, error);
		goto out;

	} else {
		printk("%s: major=%d, minor=%d\n",
						__func__,
						MAJOR(fpc_irq_data->devno),
						MINOR(fpc_irq_data->devno));
	}

	fpc_irq_data->dev = device_create(fpc_irq_data->class, NULL, fpc_irq_data->devno,
						NULL, "%s", FPC_IRQ_DEV_NAME);

	if (IS_ERR(fpc_irq_data->dev)) {
		printk("device_create failed.\n");
		error = PTR_ERR(fpc_irq_data->dev);
	}
out:
	return error;
}
/* -------------------------------------------------------------------------- */
static int fpc_irq_worker_init(fpc_irq_data_t *fpc_irq_data)
{
	int error = 0;

	dev_dbg(fpc_irq_data->dev, "%s\n", __func__);

	fpc_irq_data->idle_request = true;
	fpc_irq_data->term_request = false;

	sema_init(&fpc_irq_data->sem_active, 0);

	fpc_irq_data->worker_thread = kthread_run(
						fpc_irq_worker_function,
						fpc_irq_data,
						"%s", FPC_IRQ_WORKER_NAME);

	if (IS_ERR(fpc_irq_data->worker_thread)) {
		dev_err(fpc_irq_data->dev, "%s kthread_run failed.\n", __func__);
		error = (int)PTR_ERR(fpc_irq_data->worker_thread);
	}

	return error;
}


/* -------------------------------------------------------------------------- */
static int fpc_irq_worker_goto_idle(fpc_irq_data_t *fpc_irq_data)
{
	const int wait_idle_us = 100;

	fpc_irq_data->idle_request = true;

	if (down_trylock(&fpc_irq_data->sem_active) == 0) {
		dev_dbg(fpc_irq_data->dev, "%s : already idle\n", __func__);
	} else {
		dev_dbg(fpc_irq_data->dev, "%s : idle_request\n", __func__);

		while (down_trylock(&fpc_irq_data->sem_active)) {

			fpc_irq_data->idle_request = true;
			wake_up_interruptible(&fpc_irq_data->wq_enable);
			SLEEP_US(wait_idle_us);
		}
		dev_dbg(fpc_irq_data->dev, "%s : is idle\n", __func__);
		up (&fpc_irq_data->sem_active);
	}

	return 0;
}


/* -------------------------------------------------------------------------- */
static int fpc_irq_worker_enable(fpc_irq_data_t *fpc_irq_data)
{
	dev_dbg(fpc_irq_data->dev, "%s\n", __func__);
			
	fpc_irq_data->idle_request = false;
	fpc_irq_data->interrupt_done = false;

	wake_up_interruptible(&fpc_irq_data->wq_enable);

	return 0;
}


/* -------------------------------------------------------------------------- */
static int fpc_irq_worker_destroy(fpc_irq_data_t *fpc_irq_data)
{
	int error = 0;

	dev_dbg(fpc_irq_data->dev, "%s\n", __func__);

	if (fpc_irq_data->worker_thread) {
	
		fpc_irq_worker_goto_idle(fpc_irq_data);

		fpc_irq_data->term_request = true;
		wake_up_interruptible(&fpc_irq_data->wq_enable);

		kthread_stop(fpc_irq_data->worker_thread);
		fpc_irq_data->worker_thread = NULL;
	}
	return error;
}


/* -------------------------------------------------------------------------- */
static int fpc_irq_manage_sysfs_setup(fpc_irq_data_t *fpc_irq_data,
					bool create)
{
	int error = 0;

	if (create) {
		dev_dbg(fpc_irq_data->dev, "%s create\n", __func__);

		error = sysfs_create_group(&fpc_irq_data->dev->kobj,
					&fpc_irq_setup_attr_group);

		if (error) {
			dev_err(fpc_irq_data->dev,
				"sysf_create_group (setup) failed.\n");
			return error;
		}

	} else {
		dev_dbg(fpc_irq_data->dev, "%s remove\n", __func__);
	
		sysfs_remove_group(&fpc_irq_data->dev->kobj, &fpc_irq_setup_attr_group);
	}

	return error;
}


/* -------------------------------------------------------------------------- */
static int fpc_irq_manage_sysfs_pm(fpc_irq_data_t *fpc_irq_data,
					bool create)
{
	int error = 0;

	if (create) {
		dev_dbg(fpc_irq_data->dev, "%s create\n", __func__);

		error = sysfs_create_group(&fpc_irq_data->dev->kobj,
					&fpc_irq_pm_attr_group);

		if (error) {
			dev_err(fpc_irq_data->dev,
				"sysf_create_group (pm) failed.\n");
			return error;
		}

	} else {
		dev_dbg(fpc_irq_data->dev, "%s remove\n", __func__);
	
		sysfs_remove_group(&fpc_irq_data->dev->kobj, &fpc_irq_pm_attr_group);
	}

	return error;
}

/* -------------------------------------------------------------------------- */
int fpc_irq_wait_for_interrupt(fpc_irq_data_t *fpc_irq_data, int timeout)
{
	int result = 0;

	if (!timeout) {
		result = wait_event_interruptible(
				fpc_irq_data->wq_irq_return,
				fpc_irq_data->interrupt_done);
	} else {
		result = wait_event_interruptible_timeout(
				fpc_irq_data->wq_irq_return,
				fpc_irq_data->interrupt_done, timeout);
	}

	if (result < 0) {
		dev_err(fpc_irq_data->dev,
			 "wait_event_interruptible interrupted by signal (%d).\n", result);

		return result;
	}

	if (result || !timeout) {
		fpc_irq_data->interrupt_done = false;
		return 0;
	}

	return -ETIMEDOUT;
}


/* -------------------------------------------------------------------------- */
void  fpc_irq_interrupt(void)
{
	fpc_irq_data_t *fpc_irq_data = fpc_irq;
        //printk(" %s\n",__func__);
	//RM: LCT xuwenda 2016-07-04 Interruption of the printk log cause the spin_lock Exception
	// printk(" %s--fpc_irq_interrupt test\n",__func__);
	if (mt_get_gpio_in(fpc_irq_data->pdata.irq_gpio)) {
		fpc_irq_data->interrupt_done = true;
		 //wake_lock_timeout(&fpc_lock,HZ);
		wake_up_interruptible(&fpc_irq_data->wq_irq_return);
#ifdef FPC_WAKEUP_UEVENT
		if(fpc_irq_data->suspend_index)
		{
			fpc_irq_data->suspend_index =false;
			schedule_work(&fpc_irq_data->irq_workthread);
		}
#endif
		//return IRQ_HANDLED;
	}

	//return IRQ_NONE;
}


/* -------------------------------------------------------------------------- */
static ssize_t fpc_irq_show_attr_setup(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	fpc_irq_data_t *fpc_irq_data = fpc_irq;
	struct fpc_irq_attribute *fpc_attr;
	int val;

	fpc_attr = container_of(attr, struct fpc_irq_attribute, attr);

	if (fpc_attr->offset == offsetof(struct fpc_irq_setup, dst_pid))
		val = fpc_irq_data->setup.dst_pid;
	else if (fpc_attr->offset == offsetof(struct fpc_irq_setup, dst_signo))
		val = fpc_irq_data->setup.dst_signo;
	else if (fpc_attr->offset == offsetof(struct fpc_irq_setup, enabled))
		val = fpc_irq_data->setup.enabled;
	else if (fpc_attr->offset == offsetof(struct fpc_irq_setup, intr_enabled))
		val = fpc_irq_data->suspend_index;
	else if (fpc_attr->offset == offsetof(struct fpc_irq_setup, tac_init))
		val = fpc_irq_data->init_index;
	else if (fpc_attr->offset == offsetof(struct fpc_irq_setup, unlock_enabled))
		val = fpc_irq_data->unlock_index;
	else if (fpc_attr->offset == offsetof(struct fpc_irq_setup, spi_clk_enable))
		val = fpc_irq_data->setup.spi_clk_enable;
	else if (fpc_attr->offset == offsetof(struct fpc_irq_setup, click_event))
		val = -EPERM;
	else if (fpc_attr->offset == offsetof(struct fpc_irq_setup, test_trigger))
		val = -EPERM;
	else
		return -ENOENT;

	return scnprintf(buf, PAGE_SIZE, "%i\n", val);

}


/* -------------------------------------------------------------------------- */
static ssize_t fpc_irq_store_attr_setup(struct device *dev,
					struct device_attribute *attr,
					const char *buf,
					size_t count)
{
	int error = 0;
	fpc_irq_data_t *fpc_irq_data = fpc_irq;
	struct fpc_irq_attribute *fpc_attr;
	u64 val;
	
	error = kstrtou64(buf, 0, &val);

	fpc_attr = container_of(attr, struct fpc_irq_attribute, attr);

	if (!error) {
		if (fpc_attr->offset == offsetof(struct fpc_irq_setup, dst_pid))
			fpc_irq_data->setup.dst_pid = (pid_t)val;

		else if (fpc_attr->offset == offsetof(struct fpc_irq_setup, dst_signo))
			fpc_irq_data->setup.dst_signo = (int)val;

		else if (fpc_attr->offset == offsetof(struct fpc_irq_setup, enabled)) {
			mt_eint_unmask(fpc_irq_data->pdata.irq_no);
			fpc_irq_enable(fpc_irq_data, (int)val);
		}
		else if (fpc_attr->offset == offsetof(struct fpc_irq_setup, intr_enabled)) {
			//printk("fpc_irq_store_attr_setup enter\n");
			fpc_irq_data->suspend_index = true;
		}
		else if (fpc_attr->offset == offsetof(struct fpc_irq_setup, tac_init)) {
			printk("fpc_irq_store_attr_setup  tac_init=%d\n",(int)val);
			fpc_irq_data->init_index = val;	
		}
		else if (fpc_attr->offset == offsetof(struct fpc_irq_setup, unlock_enabled)) {
			printk("fpc_irq_store_attr_setup  unlock_enabled=%d\n",(int)val);
			fpc_irq_data->unlock_index = val;	
		}
		else if (fpc_attr->offset == offsetof(struct fpc_irq_setup, spi_clk_enable)) {
			printk("fpc_irq_store_attr_setup  spi_clk_enable=%d\n",(int)val);
			//fpc_irq_data->unlock_index = val;	
			if (val == 0)
				mt_spi_disable_clk();
			else
				mt_spi_enable_clk();
			fpc_irq_data->setup.spi_clk_enable = val;
		}
		else if (fpc_attr->offset == offsetof(struct fpc_irq_setup, test_trigger)) {

			fpc_irq_data->setup.test_trigger = (int)val;

			fpc_irq_send_signal(fpc_irq_data->dev,
						fpc_irq_data->setup.dst_pid,
						fpc_irq_data->setup.dst_signo,
						fpc_irq_data->setup.test_trigger
						);
		}
		else if (fpc_attr->offset == offsetof(struct fpc_irq_setup, click_event)) {
			printk("[FPC_event] %s, click event =%d\n", __func__,  (int)val);
			fpc_irq_click_event(fpc_irq_data, (int)val);
		}
		else
			return -ENOENT;

		return strnlen(buf, count);
	}

	return error;
}


/* -------------------------------------------------------------------------- */
static ssize_t fpc_irq_show_attr_pm(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	fpc_irq_data_t *fpc_irq_data = fpc_irq;
	struct fpc_irq_attribute *fpc_attr;
	int val;

	fpc_attr = container_of(attr, struct fpc_irq_attribute, attr);

	if (fpc_attr->offset == offsetof(struct fpc_irq_pm, state))
		val = fpc_irq_data->pm.state;
	else if (fpc_attr->offset == offsetof(struct fpc_irq_pm, supply_on))
		val = (fpc_irq_data->pm.supply_on) ? 1 : 0;
	else if (fpc_attr->offset == offsetof(struct fpc_irq_pm, hw_reset))
		val = -EPERM;
	else if (fpc_attr->offset == offsetof(struct fpc_irq_pm, notify_enabled))
		val = (fpc_irq_data->pm.notify_enabled) ? 1 : 0;
	else if (fpc_attr->offset == offsetof(struct fpc_irq_pm, notify_ack))
		val = -EPERM;
	else if (fpc_attr->offset == offsetof(struct fpc_irq_pm, wakeup_req))
		val = -EPERM; 
	else if (fpc_attr->offset == offsetof(struct fpc_irq_pm, irq_pin_val)) {
		val = mt_get_gpio_in(fpc_irq_data->pdata.irq_gpio);
		printk("[FPC GJL] %s, irq_gpio =%d\n", __func__,  (int)val);
	}
	else
		return -ENOENT;

	return scnprintf(buf, PAGE_SIZE, "%i\n", val);
}


/* -------------------------------------------------------------------------- */
static ssize_t fpc_irq_store_attr_pm(struct device *dev,
				struct device_attribute *attr,
				const char *buf,
				size_t count)
{
	int error = 0;
	fpc_irq_data_t *fpc_irq_data = fpc_irq;
	struct fpc_irq_attribute *fpc_attr;
	u64 val;

	error = kstrtou64(buf, 0, &val);

	fpc_attr = container_of(attr, struct fpc_irq_attribute, attr);

	if (!error) {
		if (fpc_attr->offset == offsetof(struct fpc_irq_pm, state))
			return -EPERM;
		if (fpc_attr->offset == offsetof(struct fpc_irq_pm, irq_pin_val))
			return -EPERM;


		else if (fpc_attr->offset == offsetof(struct fpc_irq_pm, supply_on)) {
			error = fpc_irq_supply_set(fpc_irq_data, (val != 0));
			if (error < 0)
				return -EIO;
		}
		else if (fpc_attr->offset == offsetof(struct fpc_irq_pm, hw_reset)) {
			error = fpc_irq_ctrl_hw_reset(fpc_irq_data);
			if (error < 0)
				return -EIO;
		}
		else if (fpc_attr->offset == offsetof(struct fpc_irq_pm, notify_enabled)) {
			fpc_irq_pm_notify_enable(fpc_irq_data, (int)val);
		}
		else if (fpc_attr->offset == offsetof(struct fpc_irq_pm, notify_ack)) {
			fpc_irq_pm_notify_ack(fpc_irq_data, (int)val);
		}
		else if (fpc_attr->offset == offsetof(struct fpc_irq_pm, wakeup_req)) {
			error = fpc_irq_pm_wakeup_req(fpc_irq_data);
			if (error < 0)
				return -EIO;
		}
		else
			return -ENOENT;

		return strnlen(buf, count);
	}

	return error;
}


/* -------------------------------------------------------------------------- */
static int fpc_irq_worker_function(void *_fpc_irq_data)
{
	int status;
	const int irq_timeout_ms = 10;
	fpc_irq_data_t *fpc_irq_data = _fpc_irq_data;

	while (!kthread_should_stop()) {

		up(&fpc_irq_data->sem_active);

		dev_dbg(fpc_irq_data->dev, "%s : waiting\n", __func__);

		wait_event_interruptible(fpc_irq_data->wq_enable,
				!fpc_irq_data->idle_request || fpc_irq_data->term_request);

		if (fpc_irq_data->term_request)
			continue;

		down(&fpc_irq_data->sem_active);

		if  (!fpc_irq_data->idle_request)
			dev_dbg(fpc_irq_data->dev, "%s : running\n", __func__);

		//enable_irq(fpc_irq_data->pdata.irq_no);
		mt_eint_unmask(fpc_irq_data->pdata.irq_no);

		while (!fpc_irq_data->idle_request) {

			status = fpc_irq_wait_for_interrupt(fpc_irq_data, irq_timeout_ms);

			if ((status >= 0) && (status != -ETIMEDOUT)) {

				fpc_irq_send_signal(
						fpc_irq_data->dev,
						fpc_irq_data->setup.dst_pid,
						fpc_irq_data->setup.dst_signo,
						FPC_IRQ_SIGNAL_TEST);
			}
		}

		//disable_irq(fpc_irq_data->pdata.irq_no);
		mt_eint_mask(fpc_irq_data->pdata.irq_no);
	}

	dev_dbg(fpc_irq_data->dev, "%s : exit\n", __func__);

	return 0;
}


/* -------------------------------------------------------------------------- */
static int fpc_irq_enable(fpc_irq_data_t *fpc_irq_data, int req_state)
{
	dev_dbg(fpc_irq_data->dev, "%s\n", __func__);

	if (req_state == 0) {
		if (fpc_irq_data->setup.enabled) {
			fpc_irq_worker_goto_idle(fpc_irq_data);
			fpc_irq_data->setup.enabled = 0;
		}
	} else {
		if (fpc_irq_data->setup.enabled == 0) {
			fpc_irq_worker_enable(fpc_irq_data);
			fpc_irq_data->setup.enabled = 1;
		}
	}
	return 0;
}


/* -------------------------------------------------------------------------- */
