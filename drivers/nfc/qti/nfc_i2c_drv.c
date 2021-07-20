// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2021, The Linux Foundation. All rights reserved.
 */

#include "nfc_common.h"

/**
 * i2c_disable_irq()
 *
 * Check if interrupt is disabled or not
 * and disable interrupt
 *
 * Return: int
 */
int i2c_disable_irq(struct nfc_dev *dev)
{
	unsigned long flags;

	spin_lock_irqsave(&dev->i2c_dev.irq_enabled_lock, flags);
	if (dev->i2c_dev.irq_enabled) {
		disable_irq_nosync(dev->i2c_dev.client->irq);
		dev->i2c_dev.irq_enabled = false;
	}
	spin_unlock_irqrestore(&dev->i2c_dev.irq_enabled_lock, flags);

	return 0;
}

/**
 * i2c_enable_irq()
 *
 * Check if interrupt is enabled or not
 * and enable interrupt
 *
 * Return: int
 */
int i2c_enable_irq(struct nfc_dev *dev)
{
	unsigned long flags;

	spin_lock_irqsave(&dev->i2c_dev.irq_enabled_lock, flags);
	if (!dev->i2c_dev.irq_enabled) {
		dev->i2c_dev.irq_enabled = true;
		enable_irq(dev->i2c_dev.client->irq);
	}
	spin_unlock_irqrestore(&dev->i2c_dev.irq_enabled_lock, flags);

	return 0;
}

static irqreturn_t i2c_irq_handler(int irq, void *dev_id)
{
	struct nfc_dev *nfc_dev = dev_id;
	struct i2c_dev *i2c_dev = &nfc_dev->i2c_dev;

	if (device_may_wakeup(&i2c_dev->client->dev))
		pm_wakeup_event(&i2c_dev->client->dev, WAKEUP_SRC_TIMEOUT);

	i2c_disable_irq(nfc_dev);
	wake_up(&nfc_dev->read_wq);

	return IRQ_HANDLED;
}

int i2c_read(struct nfc_dev *dev, char *buf, size_t count)
{
	int ret;
	uint16_t i = 0;
	uint16_t disp_len = GET_IPCLOG_MAX_PKT_LEN(count);

	pr_debug("%s : reading %zu bytes.\n", __func__, count);
	/* Read data */

	ret = i2c_master_recv(dev->i2c_dev.client, buf, count);
	NFCLOG_IPC(dev, false, "%s of %d bytes, ret %d", __func__, count,
								ret);
	if (ret <= 0) {
		pr_err("%s: i2c_master_recv returned %d\n", __func__, ret);
		goto i2c_read_err;
	}
	if (ret > count) {
		pr_err("%s: received too many bytes from i2c (%d)\n",
		       __func__, ret);
		ret = -EIO;
	}

	for (i = 0; i < disp_len; i++)
		NFCLOG_IPC(dev, false, " %02x", buf[i]);

	/* delay for the slow nfc devices between susequent read  operation */
	usleep_range(1000, 1100);
i2c_read_err:
	return ret;
}

int i2c_write(struct nfc_dev *dev, const char *buf, size_t count,
						int max_retry_cnt)
{
	int ret = -EINVAL;
	int retry_cnt;
	uint16_t i = 0;
	uint16_t disp_len = GET_IPCLOG_MAX_PKT_LEN(count);

	pr_debug("%s : writing %zu bytes.\n", __func__, count);

	NFCLOG_IPC(dev, false, "%s sending %d B", __func__, count);

	for (i = 0; i < disp_len; i++)
		NFCLOG_IPC(dev, false, " %02x", buf[i]);

	for (retry_cnt = 1; retry_cnt <= max_retry_cnt; retry_cnt++) {

		ret = i2c_master_send(dev->i2c_dev.client, buf, count);
		NFCLOG_IPC(dev, false, "%s ret %d", __func__, ret);
		if (ret <= 0) {
			pr_warn("%s: write failed, Maybe in Standby Mode - Retry(%d)\n",
				__func__, retry_cnt);
			usleep_range(1000, 1100);
		} else if (ret == count)
			break;
	}
	return ret;
}

ssize_t nfc_i2c_dev_read(struct file *filp, char __user *buf,
			 size_t count, loff_t *offset)
{
	int ret;
	char *tmp = NULL;
	struct nfc_dev *nfc_dev = filp->private_data;
	struct i2c_dev *i2c_dev = &nfc_dev->i2c_dev;

	if (!nfc_dev)
		return -ENODEV;

	if (count > nfc_dev->kbuflen)
		count = nfc_dev->kbuflen;

	pr_debug("%s : reading   %zu bytes.\n", __func__, count);

	mutex_lock(&nfc_dev->read_mutex);

	if (!gpio_get_value(nfc_dev->gpio.irq)) {
		if (filp->f_flags & O_NONBLOCK) {
			pr_err(":f_falg has O_NONBLOCK. EAGAIN\n");
			ret = -EAGAIN;
			goto err;
		}
		while (1) {
			ret = 0;
			if (!i2c_dev->irq_enabled) {
				i2c_dev->irq_enabled = true;
				enable_irq(i2c_dev->client->irq);
			}
			if (!gpio_get_value(nfc_dev->gpio.irq)) {
				ret = wait_event_interruptible(nfc_dev->read_wq,
						       !i2c_dev->irq_enabled);

				if (ret) {
					pr_err("error wakeup of read wq\n");
					goto err;
				}
			}
			i2c_disable_irq(nfc_dev);

			if (gpio_get_value(nfc_dev->gpio.irq))
				break;

			if (!gpio_get_value(nfc_dev->gpio.ven)) {
				pr_info("%s: ven low in read !\n", __func__);
				ret = -ENODEV;
				goto err;
			}

			pr_warn("%s: spurious interrupt detected\n", __func__);
		}
	}

	tmp = nfc_dev->kbuf;
	if (!tmp) {
		pr_err("%s: device doesn't exist anymore\n", __func__);
		ret = -ENODEV;
		goto err;
	}
	memset(tmp, 0x00, count);

	/* Read data */
	ret = i2c_read(nfc_dev, tmp, count);
	if (ret <= 0) {
		pr_err("%s: i2c_master_recv returned %d\n", __func__, ret);
		goto err;
	}

	/* check if it's response of cold reset command
	 * NFC HAL process shouldn't receive this data as
	 * command was sent by SPI driver
	 */
	if (nfc_dev->cold_reset.rsp_pending
		&& (tmp[0] == COLD_RESET_RSP_GID)
		&& (tmp[1] == COLD_RESET_OID)) {
		read_cold_reset_rsp(nfc_dev, tmp);
		nfc_dev->cold_reset.rsp_pending = false;
		wake_up_interruptible(&nfc_dev->cold_reset.read_wq);
		mutex_unlock(&nfc_dev->read_mutex);
		/*
		 * NFC process doesn't know about cold reset command
		 * being sent as it was initiated by eSE process
		 * we shouldn't return any data to NFC process
		 */
		return 0;
	}
	if (copy_to_user(buf, tmp, ret)) {
		pr_warn("%s : failed to copy to user space\n", __func__);
		ret = -EFAULT;
	}

err:
	mutex_unlock(&nfc_dev->read_mutex);
	return ret;
}

ssize_t nfc_i2c_dev_write(struct file *filp, const char __user *buf,
			  size_t count, loff_t *offset)
{
	int ret;
	char *tmp = NULL;
	struct nfc_dev *nfc_dev = filp->private_data;

	if (!nfc_dev) {
		ret = -ENODEV;
		goto out;
	}

	if (count > nfc_dev->kbuflen) {
		pr_err("%s: out of memory\n", __func__);
		ret = -ENOMEM;
		goto out;
	}

	tmp = memdup_user(buf, count);
	if (IS_ERR(tmp)) {
		pr_err("%s: memdup_user failed\n", __func__);
		ret = PTR_ERR(tmp);
		goto out;
	}

	ret = i2c_write(nfc_dev, tmp, count, NO_RETRY);
	if (ret != count) {
		pr_err("%s: failed to write %d\n", __func__, ret);
		ret = -EIO;
		goto out_free;
	}

out_free:
	kfree(tmp);
out:
	return ret;
}

static const struct file_operations nfc_i2c_dev_fops = {
	.owner = THIS_MODULE,
	.llseek = no_llseek,
	.read = nfc_i2c_dev_read,
	.write = nfc_i2c_dev_write,
	.open = nfc_dev_open,
	.release = nfc_dev_close,
	.unlocked_ioctl = nfc_dev_ioctl,
};

int nfc_i2c_dev_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0;
	struct nfc_dev *nfc_dev = NULL;
	struct i2c_dev *i2c_dev = NULL;
	struct platform_gpio nfc_gpio;
	struct platform_ldo nfc_ldo;

	pr_debug("%s: enter\n", __func__);

	//retrieve details of gpios from dt

	ret = nfc_parse_dt(&client->dev, &nfc_gpio, &nfc_ldo, PLATFORM_IF_I2C);
	if (ret) {
		pr_err("%s : failed to parse dt\n", __func__);
		goto err;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s : need I2C_FUNC_I2C\n", __func__);
		ret = -ENODEV;
		goto err;
	}
	nfc_dev = kzalloc(sizeof(struct nfc_dev), GFP_KERNEL);
	if (nfc_dev == NULL) {
		ret = -ENOMEM;
		goto err;
	}
	nfc_dev->interface = PLATFORM_IF_I2C;
	nfc_dev->i2c_dev.client = client;
	i2c_dev = &nfc_dev->i2c_dev;
	nfc_dev->nfc_read = i2c_read;
	nfc_dev->nfc_write = i2c_write;
	nfc_dev->nfc_enable_intr = i2c_enable_irq;
	nfc_dev->nfc_disable_intr = i2c_disable_irq;

	ret = configure_gpio(nfc_gpio.ven, GPIO_OUTPUT);
	if (ret) {
		pr_err("%s: unable to request nfc reset gpio [%d]\n",
		       __func__, nfc_gpio.ven);
		goto err_free_nfc_dev;
	}
	ret = configure_gpio(nfc_gpio.irq, GPIO_IRQ);
	if (ret <= 0) {
		pr_err("%s: unable to request nfc irq gpio [%d]\n",
		       __func__, nfc_gpio.irq);
		goto err_free_ven;
	}
	client->irq = ret;
	ret = configure_gpio(nfc_gpio.dwl_req, GPIO_OUTPUT);
	if (ret) {
		pr_err("%s: unable to request nfc firm downl gpio [%d]\n",
		       __func__, nfc_gpio.dwl_req);
		goto err_free_irq;
	}

	ret = configure_gpio(nfc_gpio.clkreq, GPIO_INPUT);
	if (ret) {
		pr_err("%s: unable to request nfc clkreq gpio [%d]\n",
		       __func__, nfc_gpio.clkreq);
		goto err_free_dwl_req;
	}

	nfc_dev->gpio.ven = nfc_gpio.ven;
	nfc_dev->gpio.irq = nfc_gpio.irq;
	nfc_dev->gpio.dwl_req = nfc_gpio.dwl_req;
	nfc_dev->gpio.clkreq = nfc_gpio.clkreq;

	/* init mutex and queues */
	init_waitqueue_head(&nfc_dev->read_wq);
	mutex_init(&nfc_dev->read_mutex);
	mutex_init(&nfc_dev->dev_ref_mutex);
	spin_lock_init(&i2c_dev->irq_enabled_lock);

	ret = nfc_misc_probe(nfc_dev, &nfc_i2c_dev_fops, DEV_COUNT,
				NFC_CHAR_DEV_NAME, CLASS_NAME);
	if (ret) {
		pr_err("%s: nfc_misc_probe failed\n", __func__);
		goto err_mutex_destroy;
	}
	/* interrupt initializations */
	pr_info("%s : requesting IRQ %d\n", __func__, client->irq);
	i2c_dev->irq_enabled = true;
	ret = request_irq(client->irq, i2c_irq_handler,
			  IRQF_TRIGGER_HIGH, client->name, nfc_dev);
	if (ret) {
		pr_err("%s: request_irq failed\n", __func__);
		goto err_nfc_misc_remove;
	}
	i2c_disable_irq(nfc_dev);
	i2c_set_clientdata(client, nfc_dev);

	ret = nfc_ldo_config(&client->dev, nfc_dev);
	if (ret) {
		pr_err("LDO config failed\n");
		goto err_ldo_config_failed;
	}

	ret = nfcc_hw_check(nfc_dev);
	if (ret) {
		pr_err("nfc hw check failed ret %d\n", ret);
		goto err_nfcc_hw_check;
	}

	device_init_wakeup(&client->dev, true);
	i2c_dev->irq_wake_up = false;
	nfc_dev->is_ese_session_active = false;

	pr_info("%s success\n", __func__);
	return 0;

err_nfcc_hw_check:
	if (nfc_dev->reg) {
		nfc_ldo_unvote(nfc_dev);
		regulator_put(nfc_dev->reg);
	}
err_ldo_config_failed:
	free_irq(client->irq, nfc_dev);
err_nfc_misc_remove:
	nfc_misc_remove(nfc_dev, DEV_COUNT);
err_mutex_destroy:
	mutex_destroy(&nfc_dev->dev_ref_mutex);
	mutex_destroy(&nfc_dev->read_mutex);
	gpio_free(nfc_dev->gpio.clkreq);
err_free_dwl_req:
	gpio_free(nfc_dev->gpio.dwl_req);
err_free_irq:
	gpio_free(nfc_dev->gpio.irq);
err_free_ven:
	gpio_free(nfc_dev->gpio.ven);
err_free_nfc_dev:
	kfree(nfc_dev);
err:
	pr_err("%s: failed\n", __func__);
	return ret;
}

int nfc_i2c_dev_remove(struct i2c_client *client)
{
	int ret = 0;
	struct nfc_dev *nfc_dev = NULL;

	pr_info("%s: remove device\n", __func__);
	nfc_dev = i2c_get_clientdata(client);
	if (!nfc_dev) {
		pr_err("%s: device doesn't exist anymore\n", __func__);
		ret = -ENODEV;
		return ret;
	}

	gpio_set_value(nfc_dev->gpio.ven, 0);
	// HW dependent delay before LDO goes into LPM mode
	usleep_range(10000, 10100);
	if (nfc_dev->reg) {
		nfc_ldo_unvote(nfc_dev);
		regulator_put(nfc_dev->reg);
	}

	device_init_wakeup(&client->dev, false);
	free_irq(client->irq, nfc_dev);
	nfc_misc_remove(nfc_dev, DEV_COUNT);
	mutex_destroy(&nfc_dev->dev_ref_mutex);
	mutex_destroy(&nfc_dev->read_mutex);

	if (gpio_is_valid(nfc_dev->gpio.clkreq))
		gpio_free(nfc_dev->gpio.clkreq);

	if (gpio_is_valid(nfc_dev->gpio.dwl_req))
		gpio_free(nfc_dev->gpio.dwl_req);

	if (gpio_is_valid(nfc_dev->gpio.irq))
		gpio_free(nfc_dev->gpio.irq);

	if (gpio_is_valid(nfc_dev->gpio.ven))
		gpio_free(nfc_dev->gpio.ven);

	kfree(nfc_dev);
	return ret;
}

int nfc_i2c_dev_suspend(struct device *device)
{
	struct i2c_client *client = to_i2c_client(device);
	struct nfc_dev *nfc_dev = i2c_get_clientdata(client);
	struct i2c_dev *i2c_dev = NULL;

	if (!nfc_dev) {
		pr_err("%s: device doesn't exist anymore\n", __func__);
		return -ENODEV;
	}

	i2c_dev = &nfc_dev->i2c_dev;

	NFCLOG_IPC(nfc_dev, false, "%s: irq_enabled = %d", __func__,
							i2c_dev->irq_enabled);

	if (device_may_wakeup(&client->dev) && i2c_dev->irq_enabled) {
		if (!enable_irq_wake(client->irq))
			i2c_dev->irq_wake_up = true;
	}
	return 0;
}

int nfc_i2c_dev_resume(struct device *device)
{
	struct i2c_client *client = to_i2c_client(device);
	struct nfc_dev *nfc_dev = i2c_get_clientdata(client);
	struct i2c_dev *i2c_dev = NULL;

	if (!nfc_dev) {
		pr_err("%s: device doesn't exist anymore\n", __func__);
		return -ENODEV;
	}

	i2c_dev = &nfc_dev->i2c_dev;

	NFCLOG_IPC(nfc_dev, false, "%s: irq_wake_up = %d", __func__,
							i2c_dev->irq_wake_up);

	if (device_may_wakeup(&client->dev) && i2c_dev->irq_wake_up) {
		if (!disable_irq_wake(client->irq))
			i2c_dev->irq_wake_up = false;
	}
	return 0;
}

static const struct i2c_device_id nfc_i2c_dev_id[] = {
	{NFC_I2C_DEV_ID, 0},
	{}
};

static const struct of_device_id nfc_i2c_dev_match_table[] = {
	{.compatible = NFC_I2C_DRV_STR,},
	{}
};

static const struct dev_pm_ops nfc_i2c_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(nfc_i2c_dev_suspend, nfc_i2c_dev_resume)
};

static struct i2c_driver nfc_i2c_dev_driver = {
	.id_table = nfc_i2c_dev_id,
	.probe = nfc_i2c_dev_probe,
	.remove = nfc_i2c_dev_remove,
	.driver = {
		   .name = NFC_I2C_DRV_STR,
		   .pm = &nfc_i2c_dev_pm_ops,
		   .of_match_table = nfc_i2c_dev_match_table,
		   .probe_type = PROBE_PREFER_ASYNCHRONOUS,
		   },
};

MODULE_DEVICE_TABLE(of, nfc_i2c_dev_match_table);

static int __init nfc_i2c_dev_init(void)
{
	int ret = 0;

	ret = i2c_add_driver(&nfc_i2c_dev_driver);
	if (ret != 0)
		pr_err("NFC I2C add driver error ret %d\n", ret);
	return ret;
}

module_init(nfc_i2c_dev_init);

static void __exit nfc_i2c_dev_exit(void)
{
	pr_debug("Unloading NFC I2C driver\n");
	i2c_del_driver(&nfc_i2c_dev_driver);
}

module_exit(nfc_i2c_dev_exit);

MODULE_DESCRIPTION("QTI NFC I2C driver");
MODULE_LICENSE("GPL v2");
