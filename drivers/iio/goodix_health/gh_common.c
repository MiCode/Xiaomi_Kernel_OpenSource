/* Goodix's health driver
 *
 * 2010 - 2021 Goodix Technology.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/fb.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#endif

#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

#include <linux/clk.h>
#include <net/sock.h>

#include "gh_common.h"

/**************************defination******************************/
#define GH_DEV_NAME "goodix_health"
#define GH_DEV_MAJOR 0	/* assigned */

#define GH_CLASS_NAME "goodix_health"
#define GH_INPUT_NAME "gh-keys"

#define GH_LINUX_VERSION "V1.01.04"


#define MAX_NL_MSG_LEN 16

/*************************************************************/

/* debug log setting */
u8 g_debug_level = DEBUG_LOG;

/* align=2, 2 bytes align */
/* align=4, 4 bytes align */
/* align=8, 8 bytes align */
#define ROUND_UP(x, align)		((x+(align-1))&~(align-1))

/*************************************************************/
static LIST_HEAD(device_list);
static DEFINE_MUTEX(device_list_lock);

static unsigned int bufsiz = (1024 * 1024 + 10);
module_param(bufsiz, uint, S_IRUGO);
MODULE_PARM_DESC(bufsiz, "maximum data bytes for SPI message");

#ifdef CONFIG_OF
static const struct of_device_id gh_of_match[] = {
	{ .compatible = GH_COMPATILBE, },
	{},
};
MODULE_DEVICE_TABLE(of, gh_of_match);
#endif

/* for netlink use */
static int pid = 0;

static u8 g_vendor_id = 0;

/* -------------------------------------------------------------------- */
/* timer function */
/* -------------------------------------------------------------------- */
#define TIME_START	   0
#define TIME_STOP	   1

static long int prev_time, cur_time;

long int kernel_time(unsigned int step)
{
	cur_time = ktime_to_us(ktime_get());
	if (step == TIME_START) {
		prev_time = cur_time;
		return 0;
	} else if (step == TIME_STOP) {
		gh_debug(DEBUG_LOG, "%s, use: %ld us\n", __func__, (cur_time - prev_time));
		return cur_time - prev_time;
	}
	prev_time = cur_time;
	return -1;
}

static void gh_enable_irq(struct gh_device *gh_dev)
{
	if (1 == gh_dev->irq_count) {
		gh_debug(ERR_LOG, "%s, irq already enabled\n", __func__);
	} else {
		enable_irq(gh_dev->irq);
		gh_dev->irq_count = 1;
		gh_debug(DEBUG_LOG, "%s enable interrupt!\n", __func__);
	}
}

static void gh_disable_irq(struct gh_device *gh_dev)
{
	if (0 == gh_dev->irq_count) {
		gh_debug(ERR_LOG, "%s, irq already disabled\n", __func__);
	} else {
		disable_irq(gh_dev->irq);
		gh_dev->irq_count = 0;
		gh_debug(DEBUG_LOG, "%s disable interrupt!\n", __func__);
	}
}


/* -------------------------------------------------------------------- */
/* netlink functions */
/* -------------------------------------------------------------------- */
void gh_netlink_send(struct gh_device *gh_dev, const int command)
{
	struct nlmsghdr *nlh = NULL;
	struct sk_buff *skb = NULL;
	int ret;

	//gh_debug(INFO_LOG, "[%s] : enter, send command %d\n", __func__, command);
	if (NULL == gh_dev->nl_sk) {
		gh_debug(ERR_LOG, "[%s] : invalid socket\n", __func__);
		return;
	}

	if (0 == pid) {
		gh_debug(ERR_LOG, "[%s] : invalid native process pid\n", __func__);
		return;
	}

	/*alloc data buffer for sending to native*/
	/*malloc data space at least 1500 bytes, which is ethernet data length*/
	skb = alloc_skb(MAX_NL_MSG_LEN, GFP_ATOMIC);
	if (skb == NULL) {
		return;
	}

	nlh = nlmsg_put(skb, 0, 0, 0, MAX_NL_MSG_LEN, 0);
	if (!nlh) {
		gh_debug(ERR_LOG, "[%s] : nlmsg_put failed\n", __func__);
		kfree_skb(skb);
		return;
	}

	NETLINK_CB(skb).portid = 0;
	NETLINK_CB(skb).dst_group = 0;

	*(char *)NLMSG_DATA(nlh) = command;
	ret = netlink_unicast(gh_dev->nl_sk, skb, pid, MSG_DONTWAIT);
	if (ret == 0) {
		gh_debug(ERR_LOG, "[%s] : send failed\n", __func__);
		return;
	}

	//gh_debug(INFO_LOG, "[%s] : send done, data length is %d\n", __func__, ret);
}

static void gh_netlink_recv(struct sk_buff *__skb)
{
	struct sk_buff *skb = NULL;
	struct nlmsghdr *nlh = NULL;
	char str[128];

	gh_debug(INFO_LOG, "[%s] : enter \n", __func__);

	skb = skb_get(__skb);
	if (skb == NULL) {
		gh_debug(ERR_LOG, "[%s] : skb_get return NULL\n", __func__);
		return;
	}

	/* presume there is 5byte payload at leaset */
	if (skb->len >= NLMSG_SPACE(0)) {
		nlh = nlmsg_hdr(skb);
		memcpy(str, NLMSG_DATA(nlh), sizeof(str));
		pid = nlh->nlmsg_pid;
		gh_debug(INFO_LOG, "[%s] : pid: %d, msg: %s\n", __func__, pid, str);

	} else {
		gh_debug(ERR_LOG, "[%s] : not enough data length\n", __func__);
	}

	kfree_skb(skb);
}

static int gh_netlink_init(struct gh_device *gh_dev)
{
	struct netlink_kernel_cfg cfg;

	memset(&cfg, 0, sizeof(struct netlink_kernel_cfg));
	cfg.input = gh_netlink_recv;

	gh_dev->nl_sk = netlink_kernel_create(&init_net, GH_NETLINK_ROUTE, &cfg);
	if (gh_dev->nl_sk == NULL) {
		gh_debug(ERR_LOG, "[%s] : netlink create failed\n", __func__);
		return -1;
	}

	gh_debug(INFO_LOG, "[%s] : netlink create success\n", __func__);
	return 0;
}

static int gh_netlink_destroy(struct gh_device *gh_dev)
{
	if (gh_dev->nl_sk != NULL) {
		netlink_kernel_release(gh_dev->nl_sk);
		gh_dev->nl_sk = NULL;
		return 0;
	}

	gh_debug(ERR_LOG, "[%s] : no netlink socket yet\n", __func__);
	return -1;
}

/* -------------------------------------------------------------------- */
/* file operation function */
/* -------------------------------------------------------------------- */
static ssize_t gh_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	gh_debug(ERR_LOG, "%s: Not support, please use ioctl\n", __func__);
	return -EFAULT;
}

static ssize_t gh_write(struct file *filp, const char __user *buf,
			size_t count, loff_t *f_pos)
{
	gh_debug(ERR_LOG, "%s: Not support, please use ioctl\n", __func__);
	return -EFAULT;
}

static irqreturn_t gh_irq(int irq, void *handle)
{
	struct gh_device *gh_dev = (struct gh_device *)handle;

	gh_netlink_send(gh_dev, GH_NETLINK_IRQ);

	return IRQ_HANDLED;
}


static long gh_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct gh_device *gh_dev = NULL;
	u8 reset_value = 0;

	int retval = 0;

	u8 netlink_route = GH_NETLINK_ROUTE;
	struct gh_ioc_chip_info info;

	//FUNC_ENTRY();
	if (_IOC_TYPE(cmd) != GH_IOC_MAGIC)
		return -EINVAL;

	/* Check access direction once here; don't repeat below.
	* IOC_DIR is from the user perspective, while access_ok is
	* from the kernel perspective; so they look reversed.
	*/
	if (_IOC_DIR(cmd) & _IOC_READ)
		retval = !access_ok((void __user *)arg, _IOC_SIZE(cmd));

	if (retval == 0 && _IOC_DIR(cmd) & _IOC_WRITE)
		retval = !access_ok((void __user *)arg, _IOC_SIZE(cmd));

	if (retval)
		return -EINVAL;

	gh_dev = (struct gh_device *)filp->private_data;
	if (!gh_dev) {
		gh_debug(ERR_LOG, "%s: gh_dev IS NULL ======\n", __func__);
		return -EINVAL;
	}

	switch (cmd) {
	case GH_IOC_INIT:
		gh_debug(INFO_LOG, "%s: GH_IOC_INIT gh init======\n", __func__);
		gh_debug(INFO_LOG, "%s: Linux Version %s\n", __func__, GH_LINUX_VERSION);

		if (copy_to_user((void __user *)arg, (void *)&netlink_route, sizeof(u8))) {
			retval = -EFAULT;
			break;
		}

		if (gh_dev->system_status) {
			gh_debug(INFO_LOG, "%s: system re-started======\n", __func__);
			break;
		}

		//gh_dev->irq_count = 1;
		gh_dev->sig_count = 0;
		gh_dev->system_status = 1;

		gh_debug(INFO_LOG, "%s: gh init finished======\n", __func__);
		break;

	case GH_IOC_CHIP_INFO:
		if (copy_from_user(&info, (struct gh_ioc_chip_info *)arg, sizeof(struct gh_ioc_chip_info))) {
			retval = -EFAULT;
			break;
		}
		g_vendor_id = info.vendor_id;

		gh_debug(INFO_LOG, "%s: vendor_id 0x%x\n", __func__, g_vendor_id);
		gh_debug(INFO_LOG, "%s: mode 0x%x\n", __func__, info.mode);
		gh_debug(INFO_LOG, "%s: operation 0x%x\n", __func__, info.operation);
		break;

	case GH_IOC_EXIT:
		gh_debug(INFO_LOG, "%s: GH_IOC_EXIT ======\n", __func__);
		gh_disable_irq(gh_dev);
		if (gh_dev->irq) {
			free_irq(gh_dev->irq, gh_dev);
			gh_dev->irq_count = 0;
			gh_dev->irq = 0;
		}

		gh_dev->system_status = 0;
		gh_debug(INFO_LOG, "%s: gh exit finished ======\n", __func__);
		break;

	case GH_IOC_RESET:
		gh_debug(INFO_LOG, "%s: chip reset command\n", __func__);
		gh_hw_reset(gh_dev, 0);
		break;

	case GH_IOC_ENABLE_IRQ:
		gh_debug(INFO_LOG, "%s: GH_IOC_ENABLE_IRQ ======\n", __func__);
		gh_enable_irq(gh_dev);
		break;

	case GH_IOC_DISABLE_IRQ:
		gh_debug(INFO_LOG, "%s: GH_IOC_DISABLE_IRQ ======\n", __func__);
		gh_disable_irq(gh_dev);
		break;

	case GH_IOC_ENABLE_POWER:
		gh_debug(INFO_LOG, "%s: GH_IOC_ENABLE_POWER ======\n", __func__);
		gh_hw_power_enable(gh_dev, 1);
		break;

	case GH_IOC_DISABLE_POWER:
		gh_debug(INFO_LOG, "%s: GH_IOC_DISABLE_POWER ======\n", __func__);
		gh_hw_power_enable(gh_dev, 0);
		break;

	case GH_IOC_ENTER_SLEEP_MODE:
		gh_debug(INFO_LOG, "%s: GH_IOC_ENTER_SLEEP_MODE ======\n", __func__);
		break;

	case GH_IOC_REMOVE:
		gh_debug(INFO_LOG, "%s: GH_IOC_REMOVE ======\n", __func__);
		break;

	case GH_IOC_TRANSFER_RAW_CMD:
		mutex_lock(&gh_dev->buf_lock);
		retval = gh_ioctl_transfer_raw_cmd(gh_dev, arg, bufsiz);
		mutex_unlock(&gh_dev->buf_lock);
		break;

	case GH_IOC_SPI_INIT_CFG_CMD:
		break;

    case GH_IOC_SET_RESET_VALUE:
        if (copy_from_user(&reset_value, (u8 *)arg, sizeof(u8))) {
            retval = -EFAULT;
            break;
        }
        gh_debug(INFO_LOG, "%s: GH_IOC_SET_RESET_VALUE value = %d ======\n", __func__, reset_value);
        gh_hw_set_reset_value(gh_dev, reset_value);
        break;
	default:
		gh_debug(ERR_LOG, "gh doesn't support this command(%x)\n", cmd);
		break;
	}

	//FUNC_EXIT();
	return retval;
}

#ifdef CONFIG_COMPAT
static long gh_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int retval = 0;

	retval = filp->f_op->unlocked_ioctl(filp, cmd, arg);

	return retval;
}
#endif

static unsigned int gh_poll(struct file *filp, struct poll_table_struct *wait)
{
	gh_debug(ERR_LOG, "Not support poll opertion in TEE version\n");
	return -EFAULT;
}

/* -------------------------------------------------------------------- */
/* device function */
/* -------------------------------------------------------------------- */
static int gh_open(struct inode *inode, struct file *filp)
{
	struct gh_device *gh_dev = NULL;
	int status = -ENXIO;


	FUNC_ENTRY();
	mutex_lock(&device_list_lock);
	list_for_each_entry(gh_dev, &device_list, device_entry) {
		if (gh_dev->devno == inode->i_rdev) {
			gh_debug(INFO_LOG, "%s, Found\n", __func__);
			status = 0;
			break;
		}
	}
	mutex_unlock(&device_list_lock);

	if (status == 0) {
		filp->private_data = gh_dev;
		nonseekable_open(inode, filp);
		gh_debug(INFO_LOG, "%s, Success to open device. irq = %d\n", __func__, gh_dev->irq);
	} else {
		gh_debug(ERR_LOG, "%s, No device for minor %d\n", __func__, iminor(inode));
	}
	FUNC_EXIT();
	return status;
}

static int gh_release(struct inode *inode, struct file *filp)
{
	struct gh_device *gh_dev = NULL;
	int    status = 0;

	FUNC_ENTRY();
	gh_dev = filp->private_data;
	if (gh_dev->irq)
		gh_disable_irq(gh_dev);
	gh_dev->need_update = 0;
	FUNC_EXIT();
	return status;
}

static const struct file_operations gh_fops = {
	.owner =	THIS_MODULE,
	/* REVISIT switch to aio primitives, so that userspace
	* gets more complete API coverage.	It'll simplify things
	* too, except for the locking.
	*/
	.write =	gh_write,
	.read = 	gh_read,
	.unlocked_ioctl = gh_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = gh_compat_ioctl,
#endif
	.open = 	gh_open,
	.release =	gh_release,
	.poll	= gh_poll,
};

/*-------------------------------------------------------------------------*/

#if (GH_SUPPORT_BUS_SPI == GH_SUPPORT_BUS)
static int gh_probe(struct spi_device *spi)
#elif (GH_SUPPORT_BUS_I2C == GH_SUPPORT_BUS)
static int gh_probe(struct i2c_client *client, const struct i2c_device_id *id)
#endif
{
	struct gh_device *gh_dev = NULL;
	struct device *dev;
	int retval = 0;
	int status = -EINVAL;

	FUNC_ENTRY();

	/* Allocate driver data */
	gh_dev = kzalloc(sizeof(struct gh_device), GFP_KERNEL);
	if (!gh_dev) {
		status = -ENOMEM;
		goto err;
	}

	spin_lock_init(&gh_dev->spi_lock);
	mutex_init(&gh_dev->buf_lock);
	mutex_init(&gh_dev->release_lock);

	INIT_LIST_HEAD(&gh_dev->device_entry);

	gh_dev->device_count	 = 0;
	gh_dev->probe_finish	 = 0;
	gh_dev->system_status	 = 0;
	gh_dev->need_update 	 = 0;

	/*setup gh configurations.*/
	gh_debug(INFO_LOG, "%s, ghealth test Setting gh device configuration==========\n", __func__);

	/* Initialize the driver data */
#if (GH_SUPPORT_BUS_SPI == GH_SUPPORT_BUS)
	gh_dev->spi = spi;
	spi_set_drvdata(spi, gh_dev);
       /* allocate buffer for SPI transfer */
       gh_dev->spi_buffer = kzalloc(bufsiz, GFP_KERNEL);
       if (gh_dev->spi_buffer == NULL) {
               gh_debug(ERR_LOG,"%s:kzalloc bufsize failed",__func__);
               status = -ENOMEM;
               goto err_buf;
       }
	gh_spi_setup_conf(gh_dev, 4);
	dev = &spi->dev;
	/*init transfer buffer*/
	gh_init_transfer_buffer();
#elif (GH_SUPPORT_BUS_I2C == GH_SUPPORT_BUS)
	gh_dev->client = client;
	dev = &client->dev;
	i2c_set_clientdata(client, gh_dev);
#endif

	gh_dev->irq = 0;

	/* get gpio info from dts or defination */
	gh_get_gpio_dts_info(gh_dev);
	//gh_get_sensor_dts_info();

	/*enable the power*/
	gh_hw_power_enable(gh_dev, 1);


	/* create class */
	gh_dev->class = class_create(THIS_MODULE, GH_CLASS_NAME);
	if (IS_ERR(gh_dev->class)) {
		gh_debug(ERR_LOG, "%s, Failed to create class.\n", __func__);
		status = -ENODEV;
		goto err_class;
	}

	/* get device no */
	if (GH_DEV_MAJOR > 0) {
		gh_dev->devno = MKDEV(GH_DEV_MAJOR, gh_dev->device_count++);
		status = register_chrdev_region(gh_dev->devno, 1, GH_DEV_NAME);
	} else {
		status = alloc_chrdev_region(&gh_dev->devno, gh_dev->device_count++, 1, GH_DEV_NAME);
	}
	if (status < 0) {
		gh_debug(ERR_LOG, "%s, Failed to alloc devno.\n", __func__);
		goto err_devno;
	} else {
		gh_debug(INFO_LOG, "%s, major=%d, minor=%d\n", __func__, MAJOR(gh_dev->devno), MINOR(gh_dev->devno));
	}

	/* create device */
	gh_dev->device = device_create(gh_dev->class, dev, gh_dev->devno, gh_dev, GH_DEV_NAME);
	if (IS_ERR(gh_dev->device)) {
		gh_debug(ERR_LOG, "%s, Failed to create device.\n", __func__);
		status = -ENODEV;
		goto err_device;
	} else {
		mutex_lock(&device_list_lock);
		list_add(&gh_dev->device_entry, &device_list);
		mutex_unlock(&device_list_lock);
		gh_debug(INFO_LOG, "%s, device create success.\n", __func__);
	}

	/* cdev init and add */
	cdev_init(&gh_dev->cdev, &gh_fops);
	gh_dev->cdev.owner = THIS_MODULE;
	status = cdev_add(&gh_dev->cdev, gh_dev->devno, 1);
	if (status) {
		gh_debug(ERR_LOG, "%s, Failed to add cdev.\n", __func__);
		goto err;
	}

	gh_irq_gpio_cfg(gh_dev);
	retval = request_threaded_irq(gh_dev->irq, NULL, gh_irq,
			IRQF_TRIGGER_RISING | IRQF_ONESHOT, "goodix_fp_irq", gh_dev);
	if (!retval)
		gh_debug(INFO_LOG, "%s irq thread request success!\n", __func__);
	else
		gh_debug(ERR_LOG, "%s irq thread request failed, retval=%d\n", __func__, retval);

	gh_dev->irq_count = 1;
	gh_disable_irq(gh_dev);

	/* netlink interface init */
	status = gh_netlink_init(gh_dev);
	if (status == -1) {
		mutex_lock(&gh_dev->release_lock);
		input_unregister_device(gh_dev->input);
		gh_dev->input = NULL;
		mutex_unlock(&gh_dev->release_lock);
		goto err_input;
	}

	gh_dev->probe_finish = 1;
	gh_dev->is_sleep_mode = 0;
	gh_debug(INFO_LOG, "%s health goodix probe finished\n", __func__);

	FUNC_EXIT();
	return 0;

err_input:
	cdev_del(&gh_dev->cdev);

err_device:
	unregister_chrdev_region(gh_dev->devno, 1);

err_devno:
	class_destroy(gh_dev->class);

err_class:
	gh_hw_power_enable(gh_dev, 0);
#if (GH_SUPPORT_BUS_SPI == GH_SUPPORT_BUS)
	kfree(gh_dev->spi_buffer);
err_buf:
#endif
	mutex_destroy(&gh_dev->buf_lock);
	mutex_destroy(&gh_dev->release_lock);
#if (GH_SUPPORT_BUS_SPI == GH_SUPPORT_BUS)
	spi_set_drvdata(spi, NULL);
	gh_dev->spi = NULL;
#elif (GH_SUPPORT_BUS_I2C == GH_SUPPORT_BUS)
	i2c_set_clientdata(client, NULL);
	gh_dev->client = NULL;
#endif
	kfree(gh_dev);
	gh_dev = NULL;
err:

	FUNC_EXIT();
	return status;
}

#if (GH_SUPPORT_BUS_SPI == GH_SUPPORT_BUS)
static int gh_remove(struct spi_device *spi)
{
	struct gh_device *gh_dev = spi_get_drvdata(spi);
#elif (GH_SUPPORT_BUS_I2C == GH_SUPPORT_BUS)
static int gh_remove(struct i2c_client *client)
{
	struct gh_device *gh_dev = i2c_get_clientdata(client);
#endif

	FUNC_ENTRY();

	/* make sure ops on existing fds can abort cleanly */
	if (gh_dev->irq) {
		free_irq(gh_dev->irq, gh_dev);
		gh_dev->irq_count = 0;
		gh_dev->irq = 0;
	}



	mutex_lock(&gh_dev->release_lock);
#if (GH_SUPPORT_BUS_SPI == GH_SUPPORT_BUS)
	if (gh_dev->spi_buffer != NULL) {
		kfree(gh_dev->spi_buffer);
		gh_dev->spi_buffer = NULL;
	}
	spi_set_drvdata(spi, NULL);
	gh_dev->spi = NULL;
	gh_free_transfer_buffer();
#elif (GH_SUPPORT_BUS_I2C == GH_SUPPORT_BUS)
	i2c_set_clientdata(client, NULL);
	gh_dev->client = NULL;
#endif
	mutex_unlock(&gh_dev->release_lock);

	gh_netlink_destroy(gh_dev);
	cdev_del(&gh_dev->cdev);

	device_destroy(gh_dev->class, gh_dev->devno);
	list_del(&gh_dev->device_entry);

	unregister_chrdev_region(gh_dev->devno, 1);
	class_destroy(gh_dev->class);
	gh_hw_power_enable(gh_dev, 0);

	spin_lock_irq(&gh_dev->spi_lock);
	spin_unlock_irq(&gh_dev->spi_lock);

	mutex_destroy(&gh_dev->buf_lock);
	mutex_destroy(&gh_dev->release_lock);

	kfree(gh_dev);
	FUNC_EXIT();
	return 0;
}


#if (GH_SUPPORT_BUS_SPI == GH_SUPPORT_BUS)

static struct spi_driver gh_spi_driver = {
	.driver = {
		.name = GH_DEV_NAME,
		.bus = &spi_bus_type,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = gh_of_match,
#endif
	},
	.probe = gh_probe,
	.remove = gh_remove,
};

static int __init gh_init(void)
{
	int status = 0;

	FUNC_ENTRY();

	status = spi_register_driver(&gh_spi_driver);
	if (status < 0) {
		gh_debug(ERR_LOG, "%s, Failed to register SPI driver.\n", __func__);
		return -EINVAL;
	}

	FUNC_EXIT();
	return status;
}

static void __exit gh_exit(void)
{
	FUNC_ENTRY();
	spi_unregister_driver(&gh_spi_driver);
	FUNC_EXIT();
}

#elif (GH_SUPPORT_BUS_I2C == GH_SUPPORT_BUS)

static const struct i2c_device_id gh_device_id[] = {
	{ GH_DEV_NAME, 0 },
	{ }
};

static struct i2c_driver gh_i2c_driver = {
	.probe		= gh_probe,
	.remove		= gh_remove,
	.id_table	= gh_device_id,
	.driver = {
		.name	  = GH_DEV_NAME,
		.owner	  = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = gh_of_match,
#endif
	},
};

static int __init gh_init(void)
{
	pr_info("gh30x driver installing..\n");
	return i2c_add_driver(&gh_i2c_driver);
}

static void __exit gh_exit(void)
{
	pr_info("gh30x driver exit\n");
	i2c_del_driver(&gh_i2c_driver);
}

#endif

module_init(gh_init);
module_exit(gh_exit);


MODULE_AUTHOR("goodix");
MODULE_DESCRIPTION("Goodix health driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("ghealth_driver:SPI/I2C");
