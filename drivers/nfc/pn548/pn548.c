/*
 * Copyright (C) 2011-2015 NXP Semiconductors.
 * Copyright (C) 2016 XiaoMi, Inc.
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/of_gpio.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include <linux/wakelock.h>

#include <pn548.h>

#define MAX_BUFFER_SIZE 512
#define _A1_PN66T_

static struct wake_lock fieldon_wl;
struct pn548_dev	{
	wait_queue_head_t	read_wq;
	struct mutex		read_mutex;
	struct device		*dev;
	struct i2c_client	*client;
	struct miscdevice	pn548_device;
	unsigned int 		ven_gpio;
	unsigned int 		firm_gpio;
	unsigned int		irq_gpio;

	bool			irq_enabled;
	spinlock_t		irq_enabled_lock;
	bool			do_reading;
	struct wake_lock   wl;
	bool cancel_read;
};

static void pn548_disable_irq(struct pn548_dev *pn548_dev)
{
	unsigned long flags;

	spin_lock_irqsave(&pn548_dev->irq_enabled_lock, flags);
	if (pn548_dev->irq_enabled) {
		disable_irq_nosync(pn548_dev->client->irq);
		pn548_dev->irq_enabled = false;
	}
	spin_unlock_irqrestore(&pn548_dev->irq_enabled_lock, flags);
}

static void pn548_enable_irq(struct pn548_dev *pn548_dev)
{
	unsigned long flags;

	spin_lock_irqsave(&pn548_dev->irq_enabled_lock, flags);
	if (false == pn548_dev->irq_enabled) {
		pn548_dev->irq_enabled = true;
		enable_irq(pn548_dev->client->irq);
	}
	spin_unlock_irqrestore(&pn548_dev->irq_enabled_lock, flags);
}

static irqreturn_t pn548_dev_irq_handler(int irq, void *dev_id)
{
	struct pn548_dev *pn548_dev = dev_id;


	if (gpio_get_value(pn548_dev->irq_gpio) != 1)
		return IRQ_HANDLED;

	pn548_disable_irq(pn548_dev);

	pn548_dev->do_reading = 1;

	/* Wake up waiting readers */
	wake_up(&pn548_dev->read_wq);
	wake_lock(&pn548_dev->wl);

	return IRQ_HANDLED;
}

static ssize_t pn548_dev_read(struct file *filp, char __user *buf,
		size_t count, loff_t *offset)
{
	struct pn548_dev *pn548_dev = filp->private_data;
	char tmp[MAX_BUFFER_SIZE];
	int ret;



	if (count > MAX_BUFFER_SIZE)
		count = MAX_BUFFER_SIZE;




	mutex_lock(&pn548_dev->read_mutex);
	if (!gpio_get_value(pn548_dev->irq_gpio)) {
		if (filp->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			goto fail;
		}

		pn548_dev->do_reading = 0;
		pn548_enable_irq(pn548_dev);
		if (!gpio_get_value(pn548_dev->irq_gpio)) {
			ret = wait_event_interruptible(pn548_dev->read_wq,
					pn548_dev->do_reading);

			if (ret)
				goto fail;

			pn548_disable_irq(pn548_dev);


			if (pn548_dev->cancel_read) {
				pn548_dev->cancel_read = false;
				ret = -1;
				goto fail;
			}
		}
	}

	/* Read data */
	ret = i2c_master_recv(pn548_dev->client, tmp, count);
	mutex_unlock(&pn548_dev->read_mutex);
	wake_unlock(&pn548_dev->wl);


	if (((tmp[0] & 0xff) == 0x61) && ((tmp[1] & 0xff) == 0x07) && ((tmp[2] & 0xff) == 0x01))
		wake_lock_timeout(&fieldon_wl, msecs_to_jiffies(3*1000));

	if (ret < 0) {
		pr_err("%s: PN548 i2c_master_recv returned %d\n", __func__, ret);
		return ret;
	}
	if (ret > count) {
		pr_err("%s: received too many bytes from i2c (%d)\n",
			__func__, ret);
		return -EIO;
	}

	if (copy_to_user(buf, tmp, ret)) {
		pr_warning("%s : failed to copy to user space\n", __func__);
		return -EFAULT;
	}
	return ret;

fail:
	mutex_unlock(&pn548_dev->read_mutex);
	wake_unlock(&pn548_dev->wl);
	return ret;
}

static ssize_t pn548_dev_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *offset)
{
	struct pn548_dev  *pn548_dev = filp->private_data;
	char tmp[MAX_BUFFER_SIZE];
	int ret;



	if (count > MAX_BUFFER_SIZE)
		count = MAX_BUFFER_SIZE;

	if (copy_from_user(tmp, buf, count)) {
		pr_err("%s : failed to copy from user space\n", __func__);
		return -EFAULT;
	}


	/* Write data */
	ret = i2c_master_send(pn548_dev->client, tmp, count);
	if (ret != count) {
		pr_err("%s : i2c_master_send returned %d\n", __func__, ret);
		ret = -EIO;
	}
	return ret;
}

static long pn548_dev_ioctl(struct file *filp,
			    unsigned int cmd, unsigned long arg)
{
	struct pn548_dev *pn548_dev = filp->private_data;



	switch (cmd) {
	case PN548_SET_PWR:
		if (arg == 2) {
			/* power on with firmware download (requires hw reset)
			 */

			gpio_set_value(pn548_dev->ven_gpio, 1);
			gpio_set_value(pn548_dev->firm_gpio, 1);
			msleep(60);
			gpio_set_value(pn548_dev->ven_gpio, 0);
			msleep(60);
			gpio_set_value(pn548_dev->ven_gpio, 1);
			msleep(60);
		} else if (arg == 1) {
			/* power on */

			gpio_set_value(pn548_dev->firm_gpio, 0);
			gpio_set_value(pn548_dev->ven_gpio, 1);
			if (gpio_get_value(pn548_dev->ven_gpio) != 1)
				pr_err("NFC: ven_gpio != 1\n");
			irq_set_irq_wake(pn548_dev->client->irq, 1);
			msleep(20);
		} else  if (arg == 0) {
			/* power off */

			gpio_set_value(pn548_dev->firm_gpio, 0);
			gpio_set_value(pn548_dev->ven_gpio, 0);
			if (gpio_get_value(pn548_dev->ven_gpio) != 0)
				pr_err("NFC: ven_gpio != 1\n");
			irq_set_irq_wake(pn548_dev->client->irq, 0);
			msleep(60);
		} else if (arg == 3) {

			pn548_dev->cancel_read = true;
			pn548_dev->do_reading = 1;
			wake_up(&pn548_dev->read_wq);
		} else {
			pr_err("%s bad arg %lu\n", __func__, arg);
			return -EINVAL;
		}
		break;
	default:
		pr_err("%s bad ioctl %u\n", __func__, cmd);
		return -EINVAL;
	}

	return 0;
}

static int iOpenFailCnt;
static int pn548_dev_open(struct inode *inode, struct file *filp)
{
	int ret = 0;
	struct pn548_dev *pn548_dev = container_of(filp->private_data,
						struct pn548_dev,
						pn548_device);
	char cmd_rest_nci[] = {0x20, 0x00, 0x01, 0x01};

	filp->private_data = pn548_dev;

	pn548_dev_ioctl(filp, PN548_SET_PWR, 1);
	pn548_dev_ioctl(filp, PN548_SET_PWR, 0);
	pn548_dev_ioctl(filp, PN548_SET_PWR, 1);
	if (4 != i2c_master_send(pn548_dev->client, cmd_rest_nci, 4)) {
		iOpenFailCnt += 1;
		if (iOpenFailCnt > 2)
			ret = -1;
	} else {
		iOpenFailCnt = 0;
	}


	return ret;
}

static const struct file_operations pn548_dev_fops = {
	.owner	= THIS_MODULE,
	.llseek	= no_llseek,
	.read	= pn548_dev_read,
	.write	= pn548_dev_write,
	.open	= pn548_dev_open,
	.unlocked_ioctl	= pn548_dev_ioctl,
	.compat_ioctl = pn548_dev_ioctl,
};

static int pn548_parse_dt(struct device *dev,
			 struct pn548_i2c_platform_data *pdata)
{
	struct device_node *np = dev->of_node;
	int ret = 0;

	ret = of_get_named_gpio(np, "nxp-nfc-548,irq-gpio", 0);
	if (ret < 0) {
		pr_err("failed to get \"nxp,irq_gpio\"\n");
		goto err;
	}
	pdata->irq_gpio = ret;

	ret = of_get_named_gpio(np, "nxp-nfc-548,firm-gpio", 0);
	if (ret < 0) {
		pr_err("failed to get \"nxp,dwld_en\"\n");
		goto err;
	}
	pdata->firm_gpio = ret;

	ret = of_get_named_gpio(np, "nxp-nfc-548,ven-gpio", 0);
	if (ret < 0) {
		pr_err("failed to get \"nxp,ven\"\n");
		goto err;
	}
	pdata->ven_gpio = ret;

err:
	return ret;
}
#ifndef _A1_PN66T_
static struct clk *clk_rf;
static int pn548_clk_enable(struct device *dev)
{
	int ret = -1;


	clk_rf = clk_get(dev, "nfc_clk");
	if (IS_ERR(clk_rf)) {
		pr_err("nfc: failed to get nfc_clk\n");
		return ret;
	}


	ret = clk_prepare(clk_rf);
	if (ret) {
		pr_err("nfc: failed to call clk_prepare\n");
		return ret;
	}

	return ret;
}

static void pn548_clk_disable(void)
{

	clk_unprepare(clk_rf);
	clk_put(clk_rf);
	clk_rf = NULL;
}
#endif

static int pn548_gpio_request(struct device *dev,
				struct pn548_i2c_platform_data *pdata)
{
	int ret;
	ret = gpio_request(pdata->irq_gpio, "pn548_irq");
	if (ret)
		goto err_irq;
	ret = gpio_direction_input(pdata->irq_gpio);
	if (ret)
		goto err_irq;

	gpio_free(pdata->firm_gpio);
	ret = gpio_request(pdata->firm_gpio, "pn548_fw");
	if (ret)
		goto err_fwdl_en;
	ret = gpio_direction_output(pdata->firm_gpio, 0);
	if (ret)
		goto err_fwdl_en;

	ret = gpio_request(pdata->ven_gpio, "pn548_ven");
	if (ret)
		goto err_ven;
	ret = gpio_direction_output(pdata->ven_gpio, 0);
	if (ret)
		goto err_ven;
	return 0;

err_ven:
	gpio_free(pdata->firm_gpio);
err_fwdl_en:
	gpio_free(pdata->irq_gpio);
err_irq:

	pr_err("%s: gpio request err %d\n", __func__, ret);
	return ret;
}

static void pn548_gpio_release(struct pn548_i2c_platform_data *pdata)
{
	gpio_free(pdata->ven_gpio);
	gpio_free(pdata->irq_gpio);
	gpio_free(pdata->firm_gpio);
#ifndef _A1_PN66T_

	pn548_clk_disable();
#endif
}

static int pn548_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int ret = 0;
	struct pn548_i2c_platform_data *platform_data;
	struct pn548_dev *pn548_dev;



	platform_data = kzalloc(sizeof(struct pn548_i2c_platform_data),
				GFP_KERNEL);

	if (platform_data == NULL) {
		pr_err("%s : nfc probe failed\n", __func__);
		return  -ENODEV;
		goto err_platform_data;
	}

	ret = pn548_parse_dt(&client->dev, platform_data);
	if (ret < 0) {
		pr_err("failed to parse device tree: %d\n", ret);
		goto err_parse_dt;
	}


	ret = pn548_gpio_request(&client->dev, platform_data);
	if (ret) {
		pr_err("failed to request gpio\n");
		goto err_gpio_request;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s: i2c check failed\n", __func__);
		ret = -ENODEV;
		goto err_i2c;
	}

	pn548_dev = kzalloc(sizeof(*pn548_dev), GFP_KERNEL);
	if (pn548_dev == NULL) {
		pr_err("failed to allocate memory for module data\n");
		ret = -ENOMEM;
		goto err_exit;
	}

	pn548_dev->irq_gpio = platform_data->irq_gpio;
	pn548_dev->ven_gpio  = platform_data->ven_gpio;
	pn548_dev->firm_gpio  = platform_data->firm_gpio;

	pn548_dev->client   = client;
	pn548_dev->dev = &client->dev;
	pn548_dev->do_reading = 0;

	/* Initialise mutex and work queue */
	init_waitqueue_head(&pn548_dev->read_wq);
	mutex_init(&pn548_dev->read_mutex);
	spin_lock_init(&pn548_dev->irq_enabled_lock);

	/*Initialise wake lock*/
	wake_lock_init(&pn548_dev->wl, WAKE_LOCK_SUSPEND, "nfc_locker");
	wake_lock_init(&fieldon_wl, WAKE_LOCK_SUSPEND, "nfc_locker");
	pn548_dev->pn548_device.minor = MISC_DYNAMIC_MINOR;
	pn548_dev->pn548_device.name = "pn548";
	pn548_dev->pn548_device.fops = &pn548_dev_fops;

	ret = misc_register(&pn548_dev->pn548_device);
	if (ret) {
		pr_err("%s : misc_register failed\n", __FILE__);
		goto err_misc_register;
	}


#ifndef _A1_PN66T_
	ret = pn548_clk_enable(&client->dev);
#endif
	/* request irq.  the irq is set whenever the chip has data available
	 * for reading.  it is cleared when all data has been read.
	 */

	pn548_dev->irq_enabled = true;
	gpio_direction_input(pn548_dev->irq_gpio);
	ret = request_irq(client->irq, pn548_dev_irq_handler,
			  IRQF_TRIGGER_RISING, client->name, pn548_dev);
	if (ret) {
		dev_err(&client->dev, "request_irq failed\n");
		goto err_request_irq_failed;
	}
	pn548_disable_irq(pn548_dev);
	device_init_wakeup(&client->dev, true);
	device_set_wakeup_capable(&client->dev, true);
	i2c_set_clientdata(client, pn548_dev);

	return 0;

err_request_irq_failed:
	misc_deregister(&pn548_dev->pn548_device);
err_misc_register:
	mutex_destroy(&pn548_dev->read_mutex);
	kfree(pn548_dev);
err_exit:
err_i2c:
	pn548_gpio_release(platform_data);
err_gpio_request:
err_parse_dt:
	kfree(platform_data);
err_platform_data:
	pr_err("%s: err %d\n", __func__, ret);
	return ret;
}

static int pn548_remove(struct i2c_client *client)
{
	struct pn548_dev *pn548_dev;

	pr_info("%s ++ \n", __func__);
	pn548_dev = i2c_get_clientdata(client);
	free_irq(client->irq, pn548_dev);
	misc_deregister(&pn548_dev->pn548_device);
	mutex_destroy(&pn548_dev->read_mutex);
	wake_lock_destroy(&pn548_dev->wl);
	wake_lock_destroy(&fieldon_wl);
	gpio_free(pn548_dev->irq_gpio);
	gpio_free(pn548_dev->ven_gpio);
	gpio_free(pn548_dev->firm_gpio);

	kfree(pn548_dev);

	return 0;
}

static int pn548_suspend(struct device *device)
{
	struct i2c_client *client = to_i2c_client(device);
	pr_err("%s ++ \n", __func__);

	if (device_may_wakeup(&client->dev))
		enable_irq_wake(client->irq);
	return 0;
}
static int pn548_resume(struct device *device)
{
	struct i2c_client *client = to_i2c_client(device);
	pr_err("%s -- \n", __func__);
	if (device_may_wakeup(&client->dev))
		disable_irq_wake(client->irq);

	return 0;
}
static const struct dev_pm_ops nfc_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(pn548_suspend, pn548_resume)
};

static const struct i2c_device_id pn548_id_table[] = {
	{ "pn548", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, pn548_id_table);

static struct of_device_id pn548_match_table[] = {
	{ .compatible = "nxp,nfc-548", },
	{ },
};

static struct i2c_driver pn548_driver = {
	.driver = {
		.name = "pn548",
		.owner = THIS_MODULE,
		.of_match_table = pn548_match_table,
		.pm = &nfc_pm_ops,
	},
	.probe = pn548_probe,
	.remove = pn548_remove,
	.id_table	= pn548_id_table,
};

/*
 * module load/unload record keeping
 */

static int __init pn548_dev_init(void)
{
	pr_info("%s : start\n", __func__);
	return i2c_add_driver(&pn548_driver);
}
module_init(pn548_dev_init);

static void __exit pn548_dev_exit(void)
{
	pr_info("Unloading pn548 driver\n");
	i2c_del_driver(&pn548_driver);
}
module_exit(pn548_dev_exit);

MODULE_AUTHOR("SERI");
MODULE_DESCRIPTION("NFC pn548 driver");
MODULE_LICENSE("GPL");
