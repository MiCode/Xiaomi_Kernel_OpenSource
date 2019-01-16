/******************************************************************************
 * mt_gpio.c - MTKLinux GPIO Device Driver
 *
 * Copyright 2008-2009 MediaTek Co.,Ltd.
 *
 * DESCRIPTION:
 *     This file provid the other drivers GPIO relative functions
 *
 ******************************************************************************/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <generated/autoconf.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/ioctl.h>

#include <linux/types.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/atomic.h>
#include <linux/miscdevice.h>
#include <linux/mtgpio.h>	/* ioctl cmd */

#include <mach/mt_typedefs.h>

#include <mach/mt_gpio.h>
#include <mach/mt_gpio_core.h>
#include <mach/gpio_const.h>

/***********************/
struct mt_gpio_ops {
/* char name[MT_GPIO_MAX_NAME]; */
	int (*set_dir) (unsigned long pin, unsigned long dir);
	int (*get_dir) (unsigned long pin);
	int (*set_pull_enable) (unsigned long pin, unsigned long enable);
	int (*get_pull_enable) (unsigned long pin);
	int (*set_smt) (unsigned long pin, unsigned long enable);
	int (*get_smt) (unsigned long pin);
	int (*set_ies) (unsigned long pin, unsigned long enable);
	int (*get_ies) (unsigned long pin);
	int (*set_pull_select) (unsigned long pin, unsigned long select);
	int (*get_pull_select) (unsigned long pin);
	int (*set_inversion) (unsigned long pin, unsigned long enable);
	int (*get_inversion) (unsigned long pin);
	int (*set_out) (unsigned long pin, unsigned long output);
	int (*get_out) (unsigned long pin);
	int (*get_in) (unsigned long pin);
	int (*set_mode) (unsigned long pin, unsigned long mode);
	int (*get_mode) (unsigned long pin);
};

/*---------------------------------------------------------------------------*/
static struct mt_gpio_ops mt_base_ops = {
	.set_dir = mt_set_gpio_dir_base,
	.get_dir = mt_get_gpio_dir_base,
	.set_pull_enable = mt_set_gpio_pull_enable_base,
	.get_pull_enable = mt_get_gpio_pull_enable_base,
	.set_smt = mt_set_gpio_smt_base,	
	.get_smt = mt_get_gpio_smt_base,
	.set_ies = mt_set_gpio_ies_base,
	.get_ies = mt_get_gpio_ies_base,
	.set_pull_select = mt_set_gpio_pull_select_base,
	.get_pull_select = mt_get_gpio_pull_select_base,
	.set_inversion = mt_set_gpio_inversion_base,
	.get_inversion = mt_get_gpio_inversion_base,
	.set_out = mt_set_gpio_out_base,
	.get_out = mt_get_gpio_out_base,
	.get_in = mt_get_gpio_in_base,
	.set_mode = mt_set_gpio_mode_base,
	.get_mode = mt_get_gpio_mode_base,
};

static struct mt_gpio_ops mt_ext_ops = {
	.set_dir = mt_set_gpio_dir_ext,
	.get_dir = mt_get_gpio_dir_ext,
	.set_pull_enable = mt_set_gpio_pull_enable_ext,
	.get_pull_enable = mt_get_gpio_pull_enable_ext,
	.set_smt = mt_set_gpio_smt_ext,	
	.get_smt = mt_get_gpio_smt_ext,
	.set_ies = mt_set_gpio_ies_ext,
	.get_ies = mt_get_gpio_ies_ext,
	.set_pull_select = mt_set_gpio_pull_select_ext,
	.get_pull_select = mt_get_gpio_pull_select_ext,
	.set_inversion = mt_set_gpio_inversion_ext,
	.get_inversion = mt_get_gpio_inversion_ext,
	.set_out = mt_set_gpio_out_ext,
	.get_out = mt_get_gpio_out_ext,
	.get_in = mt_get_gpio_in_ext,
	.set_mode = mt_set_gpio_mode_ext,
	.get_mode = mt_get_gpio_mode_ext,
};

DEFINE_SPINLOCK(mt_gpio_lock);
struct mt_gpio_obj_t {
	atomic_t ref;
	dev_t devno;
	struct class *cls;
	struct device *dev;
	struct cdev chrdev;
	/* spinlock_t      lock; */
	struct miscdevice *misc;
	struct mt_gpio_ops *base_ops;
	struct mt_gpio_ops *ext_ops;
};
static struct mt_gpio_obj_t mt_gpio_obj = {
	.ref = ATOMIC_INIT(0),
	.cls = NULL,
	.dev = NULL,
	.base_ops = &mt_base_ops,
	.ext_ops = &mt_ext_ops,
	/* .lock = __SPIN_LOCK_UNLOCKED(die.lock), */
};

static struct mt_gpio_obj_t *mt_gpio = &mt_gpio_obj;

#define MT_GPIO_OPS_SET(pin, operation, arg) \
({   unsigned long flags;\
     u32 retval = 0;\
	mt_gpio_pin_decrypt(&pin);\
	spin_lock_irqsave(&mt_gpio_lock, flags);\
	switch (MT_GPIO_PLACE(pin)) {\
		case MT_BASE:\
			if ((mt_gpio->base_ops == NULL) || (mt_gpio->base_ops->operation == NULL)) {\
				GPIOERR("base access error, null point %d\n", (int)pin);\
				retval = -ERACCESS;\
			} else{\
				retval = mt_gpio->base_ops->operation(pin, arg);\
				if (retval < 0) \
					GPIOERR("base operation fail %d\n", (int)retval);\
			} \
			break;\
		case MT_EXT:\
			if ((mt_gpio->ext_ops == NULL) || (mt_gpio->ext_ops->operation == NULL)) {\
				GPIOERR("extention access error, null point %d\n", (int)pin);\
				retval = -ERWRAPPER;\
			} else{\
				retval = mt_gpio->ext_ops->operation(pin, arg);\
				if (retval < 0) \
					GPIOERR("ext operation fail %d\n", (int)retval);\
			} \
			break;\
		default:\
			GPIOERR("Parameter error: %d\n", (int)pin);\
			retval = -ERINVAL;\
			break;\
	} \
	spin_unlock_irqrestore(&mt_gpio_lock, flags);\
	retval; })

	/* GPIOLOG("%s(%d)\n","operation",pin); */
#define MT_GPIO_OPS_GET(pin, operation) \
({   u32 retval = 0;\
	mt_gpio_pin_decrypt(&pin);\
	switch (MT_GPIO_PLACE(pin)) {\
		case MT_BASE:\
			if ((mt_gpio->base_ops == NULL) || (mt_gpio->base_ops->operation == NULL)) {\
				GPIOERR("base access error, null point %d\n", (int)pin);\
				retval = -ERACCESS;\
			} else{\
				retval = mt_gpio->base_ops->operation(pin);\
				if (retval < 0) \
					GPIOERR("base operation fail %d\n", (int)retval);\
			} \
			break;\
		case MT_EXT:\
			if ((mt_gpio->ext_ops == NULL) || (mt_gpio->ext_ops->operation == NULL)) {\
				GPIOERR("extention access error, null point %d\n", (int)pin);\
				retval = -ERWRAPPER;\
			} else{\
				retval = mt_gpio->ext_ops->operation(pin);\
				if (retval < 0) \
					GPIOERR("ext operation fail %d\n", (int)retval);\
			} \
			break;\
		default:\
			GPIOERR("Parameter pin number error: %d\n", (int)pin);\
			retval = -ERINVAL;\
			break;\
	};\
	retval; })

int mt_set_gpio_dir(unsigned long pin, unsigned long dir)
{
	/* int ret=0; */
	if (dir >= GPIO_DIR_MAX) {
		GPIOERR("Parameter dir error: %d\n", (int)dir);
		return -ERINVAL;
	}

	return MT_GPIO_OPS_SET(pin, set_dir, dir);
}
EXPORT_SYMBOL(mt_set_gpio_dir);
/*---------------------------------------------------------------------------*/
int mt_get_gpio_dir(unsigned long pin)
{
	return MT_GPIO_OPS_GET(pin, get_dir);
}
EXPORT_SYMBOL(mt_get_gpio_dir);
/*---------------------------------------------------------------------------*/
int mt_set_gpio_pull_enable(unsigned long pin, unsigned long enable)
{
	if (enable >= GPIO_PULL_EN_MAX) {
		GPIOERR("Parameter enable error: %d\n", (int)enable);
		return -ERINVAL;
	}

	return MT_GPIO_OPS_SET(pin, set_pull_enable, enable);
}
EXPORT_SYMBOL(mt_set_gpio_pull_enable);
/*---------------------------------------------------------------------------*/
int mt_get_gpio_pull_enable(unsigned long pin)
{
	return MT_GPIO_OPS_GET(pin, get_pull_enable);
}
EXPORT_SYMBOL(mt_get_gpio_pull_enable);
/*---------------------------------------------------------------------------*/
int mt_set_gpio_smt(unsigned long pin, unsigned long enable)
{
	if (enable >= GPIO_SMT_MAX){
		GPIOERR("Parameter enable error: %d\n",(int)enable);
		return -ERINVAL;
	}
	return MT_GPIO_OPS_SET(pin,set_smt,enable);
}
EXPORT_SYMBOL(mt_set_gpio_smt);
/*---------------------------------------------------------------------------*/
int mt_get_gpio_smt(unsigned long pin)
{
	return MT_GPIO_OPS_GET(pin,get_smt);
}
EXPORT_SYMBOL(mt_get_gpio_smt);
/*---------------------------------------------------------------------------*/
int mt_set_gpio_ies(unsigned long pin, unsigned long enable)
{
	if (enable >= GPIO_IES_MAX) {
		GPIOERR("Parameter enable error: %d\n", (int)enable);
		return -ERINVAL;
	}
	return MT_GPIO_OPS_SET(pin, set_ies, enable);
}
EXPORT_SYMBOL(mt_set_gpio_ies);
/*---------------------------------------------------------------------------*/
int mt_get_gpio_ies(unsigned long pin)
{
	return MT_GPIO_OPS_GET(pin, get_ies);
}
EXPORT_SYMBOL(mt_get_gpio_ies);
/*---------------------------------------------------------------------------*/
int mt_set_gpio_pull_select(unsigned long pin, unsigned long select)
{
	if (select >= GPIO_PULL_MAX) {
		GPIOERR("Parameter select error: %d\n", (int)select);
		return -ERINVAL;
	}
	return MT_GPIO_OPS_SET(pin, set_pull_select, select);
}
EXPORT_SYMBOL(mt_get_gpio_pull_select);
/*---------------------------------------------------------------------------*/
int mt_get_gpio_pull_select(unsigned long pin)
{
	return MT_GPIO_OPS_GET(pin, get_pull_select);
}
EXPORT_SYMBOL(mt_set_gpio_pull_select);
/*---------------------------------------------------------------------------*/
int mt_set_gpio_inversion(unsigned long pin, unsigned long enable)
{
	if (enable >= GPIO_DATA_INV_MAX) {
		GPIOERR("Parameter enable error: %d\n", (int)enable);
		return -ERINVAL;
	}
	return MT_GPIO_OPS_SET(pin, set_inversion, enable);
}
EXPORT_SYMBOL(mt_set_gpio_inversion);
/*---------------------------------------------------------------------------*/
int mt_get_gpio_inversion(unsigned long pin)
{
	return MT_GPIO_OPS_GET(pin, get_inversion);
}
EXPORT_SYMBOL(mt_get_gpio_inversion);
/*---------------------------------------------------------------------------*/
int mt_set_gpio_out(unsigned long pin, unsigned long output)
{
	if (output >= GPIO_OUT_MAX) {
		GPIOERR("Parameter output error: %d\n", (int)output);
		return -ERINVAL;
	}
	return MT_GPIO_OPS_SET(pin, set_out, output);
}
EXPORT_SYMBOL(mt_set_gpio_out);
/*---------------------------------------------------------------------------*/
int mt_get_gpio_out(unsigned long pin)
{
	return MT_GPIO_OPS_GET(pin, get_out);
}
EXPORT_SYMBOL(mt_get_gpio_out);
/*---------------------------------------------------------------------------*/
int mt_get_gpio_in(unsigned long pin)
{
	return MT_GPIO_OPS_GET(pin, get_in);
}
EXPORT_SYMBOL(mt_get_gpio_in);
/*---------------------------------------------------------------------------*/
int mt_set_gpio_mode(unsigned long pin, unsigned long mode)
{
	if (mode >= GPIO_MODE_MAX) {
		GPIOERR("Parameter mode error: %d\n", (int)mode);
		return -ERINVAL;
	}
	return MT_GPIO_OPS_SET(pin, set_mode, mode);
}
EXPORT_SYMBOL(mt_set_gpio_mode);
/*---------------------------------------------------------------------------*/
int mt_get_gpio_mode(unsigned long pin)
{
	return MT_GPIO_OPS_GET(pin, get_mode);
}
EXPORT_SYMBOL(mt_get_gpio_mode);

/*****************************************************************************/
/* File operation                                                            */
/*****************************************************************************/
static int mt_gpio_open(struct inode *inode, struct file *file)
{
	struct mt_gpio_obj_t *obj = mt_gpio;
	GPIOFUC();

	if (obj == NULL) {
		GPIOERR("NULL pointer");
		return -EFAULT;
	}

	atomic_inc(&obj->ref);
	file->private_data = obj;
	return nonseekable_open(inode, file);
}

/*---------------------------------------------------------------------------*/
static int mt_gpio_release(struct inode *inode, struct file *file)
{
	struct mt_gpio_obj_t *obj = mt_gpio;

	GPIOFUC();

	if (obj == NULL) {
		GPIOERR("NULL pointer");
		return -EFAULT;
	}

	atomic_dec(&obj->ref);
	return RSUCCESS;
}

/*---------------------------------------------------------------------------*/
static long mt_gpio_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct mt_gpio_obj_t *obj = mt_gpio;
	long res;
	unsigned long pin;

	GPIOFUC();

	if (obj == NULL) {
		GPIOERR("NULL pointer");
		return -EFAULT;
	}

	switch (cmd) {
	case GPIO_IOCQMODE:
		{
			pin = (unsigned long)arg;
			res = GIO_INVALID_OBJ(obj) ? (-EACCES) : mt_get_gpio_mode(pin);
			break;
		}
	case GPIO_IOCTMODE0:
		{
			pin = (unsigned long)arg;
			res =
			    GIO_INVALID_OBJ(obj) ? (-EACCES) : mt_set_gpio_mode(pin, GPIO_MODE_00);
			break;
		}
	case GPIO_IOCTMODE1:
		{
			pin = (unsigned long)arg;
			res =
			    GIO_INVALID_OBJ(obj) ? (-EACCES) : mt_set_gpio_mode(pin, GPIO_MODE_01);
			break;
		}
	case GPIO_IOCTMODE2:
		{
			pin = (unsigned long)arg;
			res =
			    GIO_INVALID_OBJ(obj) ? (-EACCES) : mt_set_gpio_mode(pin, GPIO_MODE_02);
			break;
		}
	case GPIO_IOCTMODE3:
		{
			pin = (unsigned long)arg;
			res =
			    GIO_INVALID_OBJ(obj) ? (-EACCES) : mt_set_gpio_mode(pin, GPIO_MODE_03);
			break;
		}
	case GPIO_IOCQDIR:
		{
			pin = (unsigned long)arg;
			res = GIO_INVALID_OBJ(obj) ? (-EACCES) : mt_get_gpio_dir(pin);
			break;
		}
	case GPIO_IOCSDIRIN:
		{
			pin = (unsigned long)arg;
			res = GIO_INVALID_OBJ(obj) ? (-EACCES) : mt_set_gpio_dir(pin, GPIO_DIR_IN);
			break;
		}
	case GPIO_IOCSDIROUT:
		{
			pin = (unsigned long)arg;
			res = GIO_INVALID_OBJ(obj) ? (-EACCES) : mt_set_gpio_dir(pin, GPIO_DIR_OUT);
			break;
		}
	case GPIO_IOCQPULLEN:
		{
			pin = (unsigned long)arg;
			res = GIO_INVALID_OBJ(obj) ? (-EACCES) : mt_get_gpio_pull_enable(pin);
			break;
		}
	case GPIO_IOCSPULLENABLE:
		{
			pin = (unsigned long)arg;
			res = GIO_INVALID_OBJ(obj) ? (-EACCES) : mt_set_gpio_pull_enable(pin, TRUE);
			break;
		}
	case GPIO_IOCSPULLDISABLE:
		{
			pin = (unsigned long)arg;
			res =
			    GIO_INVALID_OBJ(obj) ? (-EACCES) : mt_set_gpio_pull_enable(pin, FALSE);
			break;
		}
	case GPIO_IOCQPULL:
		{
			pin = (unsigned long)arg;
			res = GIO_INVALID_OBJ(obj) ? (-EACCES) : mt_get_gpio_pull_select(pin);
			break;
		}
	case GPIO_IOCSPULLDOWN:
		{
			pin = (unsigned long)arg;
			res =
			    GIO_INVALID_OBJ(obj) ? (-EACCES) : mt_set_gpio_pull_select(pin,
										       GPIO_PULL_DOWN);
			break;
		}
	case GPIO_IOCSPULLUP:
		{
			pin = (unsigned long)arg;
			res =
			    GIO_INVALID_OBJ(obj) ? (-EACCES) : mt_set_gpio_pull_select(pin,
										       GPIO_PULL_UP);
			break;
		}
	case GPIO_IOCQINV:
		{
			pin = (unsigned long)arg;
			res = GIO_INVALID_OBJ(obj) ? (-EACCES) : mt_get_gpio_inversion(pin);
			break;
		}
	case GPIO_IOCSINVENABLE:
		{
			pin = (unsigned long)arg;
			res = GIO_INVALID_OBJ(obj) ? (-EACCES) : mt_set_gpio_inversion(pin, TRUE);
			break;
		}
	case GPIO_IOCSINVDISABLE:
		{
			pin = (unsigned long)arg;
			res = GIO_INVALID_OBJ(obj) ? (-EACCES) : mt_set_gpio_inversion(pin, FALSE);
			break;
		}
	case GPIO_IOCQDATAIN:
		{
			pin = (unsigned long)arg;
			res = GIO_INVALID_OBJ(obj) ? (-EFAULT) : mt_get_gpio_in(pin);
			break;
		}
	case GPIO_IOCQDATAOUT:
		{
			pin = (unsigned long)arg;
			res = GIO_INVALID_OBJ(obj) ? (-EACCES) : mt_get_gpio_out(pin);
			break;
		}
	case GPIO_IOCSDATALOW:
		{
			pin = (unsigned long)arg;
			res =
			    GIO_INVALID_OBJ(obj) ? (-EACCES) : mt_set_gpio_out(pin, GPIO_OUT_ZERO);
			break;
		}
	case GPIO_IOCSDATAHIGH:
		{
			pin = (unsigned long)arg;
			res = GIO_INVALID_OBJ(obj) ? (-EACCES) : mt_set_gpio_out(pin, GPIO_OUT_ONE);
			break;
		}
	default:
		{
			res = -EPERM;
			break;
		}
	}

	if (res == -EACCES)
		GPIOERR(" cmd = 0x%8X, invalid pointer\n", cmd);
	else if (res < 0)
		GPIOERR(" cmd = 0x%8X, err = %ld\n", cmd, res);
	return res;
}

/*---------------------------------------------------------------------------*/
static struct file_operations mt_gpio_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = mt_gpio_ioctl,
	#ifdef CONFIG_COMPAT
	.compat_ioctl   = mt_gpio_ioctl,
	#endif
	.open = mt_gpio_open,
	.release = mt_gpio_release,
};

/*----------------------------------------------------------------------------*/
static struct miscdevice mt_gpio_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "mtgpio",
	.fops = &mt_gpio_fops,
};

/*---------------------------------------------------------------------------*/
static int mt_gpio_probe(struct platform_device *dev)
{
	int err;
	struct miscdevice *misc = &mt_gpio_device;

#ifdef CONFIG_OF
	if (dev->dev.of_node) {
		/* Setup IO addresses */
		get_gpio_vbase(dev->dev.of_node);
	}

	get_io_cfg_vbase();
#endif
#ifdef CONFIG_MD32_SUPPORT
	md32_gpio_handle_init();
#endif

	/* printk(KERN_ALERT"[GPIO]%5d,<%s> gpio devices probe\n", __LINE__, __func__); */
	GPIOLOG("Registering GPIO device\n");

	if (!mt_gpio)
		GPIO_RETERR(-EACCES, "");
	mt_gpio->misc = misc;

	if ((err = misc_register(misc)))
		GPIOERR("register gpio\n");

	if ((err = mt_gpio_create_attr(misc->this_device)))
		GPIOERR("create attribute\n");

	dev_set_drvdata(misc->this_device, mt_gpio);

	return err;
}

/*---------------------------------------------------------------------------*/
static int mt_gpio_remove(struct platform_device *dev)
{
	struct mt_gpio_obj_t *obj = platform_get_drvdata(dev);
	int err;

	if ((err = mt_gpio_delete_attr(obj->misc->this_device)))
		GPIOERR("delete attr\n");

	if ((err = misc_deregister(obj->misc)))
		GPIOERR("deregister gpio\n");

	return err;
}

/*---------------------------------------------------------------------------*/
#ifdef CONFIG_PM
/*---------------------------------------------------------------------------*/
static int mtk_gpio_suspend(struct platform_device *pdev, pm_message_t state)
{
	int ret = 0;
	mt_gpio_suspend();
	return ret;
}

/*---------------------------------------------------------------------------*/
static int mtk_gpio_resume(struct platform_device *pdev)
{
	int ret = 0;
	mt_gpio_resume();
	return ret;
}

/*---------------------------------------------------------------------------*/
#endif				/*CONFIG_PM */
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
#ifdef CONFIG_OF
static const struct of_device_id apgpio_of_ids[] = {
	{ .compatible = "mediatek,GPIO", },
	{}
};
#endif

static struct platform_driver gpio_driver = {
	.probe = mt_gpio_probe,
	.remove = mt_gpio_remove,
#ifdef CONFIG_PM
	.suspend = mtk_gpio_suspend,
	.resume = mtk_gpio_resume,
#endif
	.driver = {
		.name = GPIO_DEVICE,
#ifdef CONFIG_OF
		.of_match_table = apgpio_of_ids,
#endif
		},
};

#ifdef CONFIG_OF
struct device_node *get_gpio_np(void)
{
    gpio_vbase.gpio_regs = NULL;
	struct device_node *np_gpio;
	np_gpio = of_find_compatible_node(NULL, NULL, apgpio_of_ids[0].compatible);
	if(np_gpio == NULL) {
		GPIOERR("GPIO device node is NULL\n");
		return NULL;
	}
    return np_gpio;
}
#endif

/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
static int __init mt_gpio_init(void)
{
	int ret = 0;
	GPIOLOG("version: %s\n", VERSION);

	ret = platform_driver_register(&gpio_driver);
	return ret;
}

/*---------------------------------------------------------------------------*/
static void __exit mt_gpio_exit(void)
{
	platform_driver_unregister(&gpio_driver);
	return;
}

/* void gpio_dump_regs(void) */
/* { */
/* return; */
/* } */
/*---------------------------------------------------------------------------*/
module_init(mt_gpio_init);
module_exit(mt_gpio_exit);
MODULE_AUTHOR("Ranran <ranran.lu@mediatek.com>");
MODULE_DESCRIPTION("MT General Purpose Driver (GPIO) $Revision$");
MODULE_LICENSE("GPL");
/*---------------------------------------------------------------------------*/
