/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2023-2028, The Linux Foundation. All rights reserved.
 */

#include "ntag_common.h"

/**
 * i2c_enable_irq()
 *
 * Check if interrupt is enabled or not
 * and enable interrupt
 *
 * Return: int
 */
int i2c_enable_irq(struct ntag_dev *dev)
{
	unsigned long flags;

	spin_lock_irqsave(&dev->i2c_dev.irq_enabled_lock, flags);
	if (!dev->i2c_dev.irq_enabled) {
		dev->i2c_dev.irq_enabled = true;
		enable_irq(dev->i2c_dev.client->irq);
		pr_info("NtagDrv: %s: ntag enable irq\n", __func__);
	}
	spin_unlock_irqrestore(&dev->i2c_dev.irq_enabled_lock, flags);

	return 0;
}

/**
 * i2c_disable_irq()
 *
 * Check if interrupt is disabled or not
 * and disable interrupt
 *
 * Return: int
 */
int i2c_disable_irq(struct ntag_dev *dev)
{
	unsigned long flags;

	spin_lock_irqsave(&dev->i2c_dev.irq_enabled_lock, flags);
	if (dev->i2c_dev.irq_enabled) {
		disable_irq_nosync(dev->i2c_dev.client->irq);
		dev->i2c_dev.irq_enabled = false;
		pr_info("NtagDrv: %s: ntag disable irq\n", __func__);
	}
	spin_unlock_irqrestore(&dev->i2c_dev.irq_enabled_lock, flags);

	return 0;
}

static void ntag_clear_irq_work(struct work_struct *work)
{
	struct ntag_dev *ntag_dev = container_of(work, struct ntag_dev,
			clear_irq_work.work);
	if (!ntag_dev)
		return;

	pr_info("NtagDrv: %s\n", __func__);
	ntag_clear_irq_register(ntag_dev);
}

static irqreturn_t i2c_irq_handler(int irq, void *dev_id)
{
	struct ntag_dev *ntag_dev = dev_id;
	struct i2c_dev *i2c_dev = &ntag_dev->i2c_dev;
	unsigned long flags;

	pr_info("NtagDrv: %s: ntag irq trigger\n", __func__);

	if (device_may_wakeup(&i2c_dev->client->dev))
		pm_wakeup_event(&i2c_dev->client->dev, WAKEUP_SRC_TIMEOUT);

	spin_lock_irqsave(&ntag_dev->i2c_dev.irq_enabled_lock, flags);
	if (ntag_dev->fasync_queue) {
		pr_info("NtagDrv: %s: ntag kill_fasync\n", __func__);
		kill_fasync(&(ntag_dev->fasync_queue), SIGIO, POLL_IN);
	}
	spin_unlock_irqrestore(&ntag_dev->i2c_dev.irq_enabled_lock, flags);

	schedule_delayed_work(&ntag_dev->clear_irq_work, msecs_to_jiffies(IRQ_CLEAR_DELAY));

	return IRQ_HANDLED;
}

void ntag_print_buffer(const unsigned char *buf, const int len, bool is_send)
{
	unsigned char output[MAX_BUFFER_SIZE*2 + 1];
	int i;

	for (i = 0; i < len; i++) {
		snprintf(output + i * 2, 3, "%02X", buf[i]);
	}
	if (is_send)
		pr_info("%s: SEND %3d > %s\n", __func__, len, output);
	else
		pr_info("%s: RECV %3d < %s\n", __func__, len, output);
}

int i2c_read(struct ntag_dev *ntag_dev, char *buf, size_t count)
{
	int ret = 0;
	pr_debug("NtagDrv: %s: reading %zu bytes.\n", __func__, count);

	if (count > MAX_BUFFER_SIZE)
		count = MAX_BUFFER_SIZE;

	memset(buf, 0x00, count);
	/* Read data */
	ret = i2c_master_recv(ntag_dev->i2c_dev.client, buf, count);
	if (ret <= 0) {
		pr_err("NtagDrv: %s: read failed ret %d\n", __func__, ret);
		return ret;
	}

	ntag_print_buffer(buf, count, false);
	return ret;
}

int i2c_write(struct ntag_dev *ntag_dev, const char *buf, size_t count)
{
	int ret = 0;
	pr_debug("NtagDrv: %s: writing %zu bytes.\n", __func__, count);

	if (count > MAX_BUFFER_SIZE)
		count = MAX_BUFFER_SIZE;

	/* Write data */
	ret = i2c_master_send(ntag_dev->i2c_dev.client, buf, count);
	if (ret <= 0) {
		pr_err("NtagDrv: %s: write failed ret %d\n", __func__, ret);
		return ret;
	} else if (ret != count) {
		pr_err("NtagDrv: %s: write failed (count mismatch) ret %d\n", __func__, ret);
		ret = -EIO;
		return ret;
	}

	ntag_print_buffer(buf, count, true);
	return ret;
}

ssize_t ntag_i2c_dev_read(struct file *filp, char __user *buf,
			 size_t count, loff_t *offset)
{
	int ret = 0;
	struct ntag_dev *ntag_dev = (struct ntag_dev *)filp->private_data;

	if (!ntag_dev)
		return -ENODEV;

	mutex_lock(&ntag_dev->read_mutex);
	ret = i2c_read(ntag_dev, ntag_dev->read_kbuf, count);
	if (ret > 0) {
		if (copy_to_user(buf, ntag_dev->read_kbuf, ret)) {
			pr_warn("NtagDrv: %s: failed to copy to user space\n", __func__);
			ret = -EFAULT;
		}
	}
	mutex_unlock(&ntag_dev->read_mutex);

	return ret;
}

ssize_t ntag_i2c_dev_write(struct file *filp, const char __user *buf,
			  size_t count, loff_t *offset)
{
	int ret;
	struct ntag_dev *ntag_dev = (struct ntag_dev *)filp->private_data;

	if (!ntag_dev)
		return -ENODEV;

	if (count > MAX_BUFFER_SIZE)
		count = MAX_BUFFER_SIZE;

	mutex_lock(&ntag_dev->write_mutex);
	if (copy_from_user(ntag_dev->write_kbuf, buf, count)) {
		pr_err("NtagDrv: %s: failed to copy from user space\n", __func__);
		mutex_unlock(&ntag_dev->write_mutex);
		return -EFAULT;
	}
	ret = i2c_write(ntag_dev, ntag_dev->write_kbuf, count);
	mutex_unlock(&ntag_dev->write_mutex);

	return ret;
}

static const struct file_operations ntag_i2c_dev_fops = {
	.owner = THIS_MODULE,
	.llseek = no_llseek,
	.read = ntag_i2c_dev_read,
	.write = ntag_i2c_dev_write,
	.open = ntag_dev_open,
	.release = ntag_dev_close,
	.fasync = ntag_irq_fasync,
};

#if  (KERNEL_VERSION(6, 3, 0) <= LINUX_VERSION_CODE)
int ntag_i2c_dev_probe(struct i2c_client *client)
#else
int ntag_i2c_dev_probe(struct i2c_client *client, const struct i2c_device_id *id)
#endif
{
	int ret = 0;
	struct ntag_dev *ntag_dev = NULL;
	struct i2c_dev *i2c_dev = NULL;
	struct platform_configs ntag_configs;
	struct platform_gpio *ntag_gpio = &ntag_configs.gpio;

	pr_info("NtagDrv: %s: enter\n", __func__);

	//retrieve details of gpios from dt
	ret = ntag_parse_dt(&client->dev, &ntag_configs);
	if (ret) {
		pr_err("NtagDrv: %s: failed to parse dt\n", __func__);
		goto err;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("NtagDrv: %s: need I2C_FUNC_I2C\n", __func__);
		ret = -ENODEV;
		goto err;
	}

	ntag_dev = kzalloc(sizeof(struct ntag_dev), GFP_KERNEL);
	if (ntag_dev == NULL) {
		ret = -ENOMEM;
		goto err;
	}

	ntag_dev->read_kbuf = kzalloc(MAX_BUFFER_SIZE, GFP_DMA | GFP_KERNEL);
	if (!ntag_dev->read_kbuf) {
		ret = -ENOMEM;
		goto err_free_ntag_dev;
	}

	ntag_dev->write_kbuf = kzalloc(MAX_BUFFER_SIZE, GFP_DMA | GFP_KERNEL);
	if (!ntag_dev->write_kbuf) {
		ret = -ENOMEM;
		goto err_free_read_kbuf;
	}

	ntag_dev->i2c_dev.client = client;
	i2c_dev = &ntag_dev->i2c_dev;
	ntag_dev->ntag_read = i2c_read;
	ntag_dev->ntag_write = i2c_write;
	ntag_dev->ntag_enable_intr = i2c_enable_irq;
	ntag_dev->ntag_disable_intr = i2c_disable_irq;

	if (gpio_is_valid(ntag_gpio->hpd)) {
		ret = configure_gpio(ntag_gpio->hpd, GPIO_OUTPUT);
		if (ret) {
			pr_err("NtagDrv: %s: unable to configure ntag hpd gpio [%d]\n",
					__func__, ntag_gpio->hpd);
			goto err_free_write_kbuf;
		}
	}

	ret = configure_gpio(ntag_gpio->irq, GPIO_IRQ);
	if (ret <= 0) {
		pr_err("NtagDrv: %s: unable to configure ntag irq gpio [%d]\n",
			__func__, ntag_gpio->irq);
		goto err_free_hpd;
	}
	client->irq = ret;

	/*copy the retrieved gpio details from DT */
	memcpy(&ntag_dev->configs, &ntag_configs, sizeof(struct platform_configs));

	/* init mutex and lock */
	mutex_init(&ntag_dev->dev_ref_mutex);
	mutex_init(&ntag_dev->read_mutex);
	mutex_init(&ntag_dev->write_mutex);
	spin_lock_init(&i2c_dev->irq_enabled_lock);

	INIT_DELAYED_WORK(&ntag_dev->clear_irq_work, ntag_clear_irq_work);

	ret = ntag_misc_register(ntag_dev, &ntag_i2c_dev_fops, DEV_COUNT,
			NTAG_CHAR_DEV_NAME, CLASS_NAME);
	if (ret) {
		pr_err("NtagDrv: %s: ntag_misc_register failed\n", __func__);
		goto err_mutex_destroy;
	}

	/* interrupt initializations */
	pr_info("NtagDrv: %s: requesting IRQ %d\n", __func__, client->irq);
	i2c_dev->irq_enabled = true;
	ret = request_irq(client->irq, i2c_irq_handler,
			IRQF_TRIGGER_FALLING, client->name, ntag_dev);
	if (ret) {
		pr_err("NtagDrv: %s: request_irq failed\n", __func__);
		goto err_ntag_misc_unregister;
	}
	i2c_enable_irq(ntag_dev);
	i2c_set_clientdata(client, ntag_dev);

	ret = ntag_ldo_config(&client->dev, ntag_dev);
	if (ret) {
		pr_err("NtagDrv: %s: LDO config failed\n", __func__);
		goto err_ldo_config_failed;
	}

	device_init_wakeup(&client->dev, true);
	i2c_dev->irq_wake_up = false;

	if (ntag_init(ntag_dev))
		pr_err("NtagDrv: %s: ntag_init failed\n", __func__);
	else
		pr_info("NtagDrv: %s: ntag_init success\n", __func__);

	pr_info("NtagDrv: %s: success\n", __func__);
	return 0;

err_ldo_config_failed:
	free_irq(client->irq, ntag_dev);
err_ntag_misc_unregister:
	ntag_misc_unregister(ntag_dev, DEV_COUNT);
err_mutex_destroy:
	mutex_destroy(&ntag_dev->dev_ref_mutex);
	mutex_destroy(&ntag_dev->read_mutex);
	mutex_destroy(&ntag_dev->write_mutex);
	gpio_free(ntag_gpio->irq);
err_free_hpd:
	if (gpio_is_valid(ntag_gpio->hpd)) {
		gpio_free(ntag_gpio->hpd);
	}
err_free_write_kbuf:
	kfree(ntag_dev->write_kbuf);
err_free_read_kbuf:
	kfree(ntag_dev->read_kbuf);
err_free_ntag_dev:
	kfree(ntag_dev);
err:
	pr_err("NtagDrv: %s: failed\n", __func__);
	return ret;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0))
void ntag_i2c_dev_remove(struct i2c_client *client)
{
	struct ntag_dev *ntag_dev = NULL;

	pr_info("NtagDrv: %s: remove device\n", __func__);
	ntag_dev = i2c_get_clientdata(client);
	if (!ntag_dev) {
		pr_err("NtagDrv: %s: device doesn't exist anymore\n", __func__);
		return;
	}

	if (ntag_dev->dev_ref_count > 0) {
		pr_err("NtagDrv: %s: device already in use\n", __func__);
		return;
	}

	// NTAG chip enter PD mode
	if (gpio_is_valid(ntag_dev->configs.gpio.hpd)) {
		// FM19511 HPD mode: 0
		// FM19510 HPD mode: 1
		gpio_set_value(ntag_dev->configs.gpio.hpd, 0);
		// HW dependent delay before LDO goes into LPM mode
		usleep_range(10000, 10100);
	}
	if (ntag_dev->reg) {
		ntag_ldo_unvote(ntag_dev);
		regulator_put(ntag_dev->reg);
	}

	cancel_delayed_work_sync(&ntag_dev->clear_irq_work);
	device_init_wakeup(&client->dev, false);
	free_irq(client->irq, ntag_dev);

	ntag_misc_unregister(ntag_dev, DEV_COUNT);
	mutex_destroy(&ntag_dev->dev_ref_mutex);

	if (gpio_is_valid(ntag_dev->configs.gpio.irq))
		gpio_free(ntag_dev->configs.gpio.irq);
	if (gpio_is_valid(ntag_dev->configs.gpio.hpd))
		gpio_free(ntag_dev->configs.gpio.hpd);

	kfree(ntag_dev->read_kbuf);
	kfree(ntag_dev->write_kbuf);
	kfree(ntag_dev);
}
#else
int ntag_i2c_dev_remove(struct i2c_client *client)
{
	int ret = 0;
	struct ntag_dev *ntag_dev = NULL;

	pr_info("NtagDrv: %s: remove device\n", __func__);
	ntag_dev = i2c_get_clientdata(client);
	if (!ntag_dev) {
		pr_err("NtagDrv: %s: device doesn't exist anymore\n", __func__);
		ret = -ENODEV;
		return ret;
	}

	if (ntag_dev->dev_ref_count > 0) {
		pr_err("NtagDrv: %s: device already in use\n", __func__);
		return -EBUSY;
	}

	// NTAG chip enter PD mode
	if (gpio_is_valid(ntag_dev->configs.gpio.hpd)) {
		// FM19511 HPD mode: 0
		// FM19510 HPD mode: 1
		gpio_set_value(ntag_dev->configs.gpio.hpd, 0);
		// HW dependent delay before LDO goes into LPM mode
		usleep_range(10000, 10100);
	}
	if (ntag_dev->reg) {
		ntag_ldo_unvote(ntag_dev);
		regulator_put(ntag_dev->reg);
	}

	cancel_delayed_work_sync(&ntag_dev->clear_irq_work);
	device_init_wakeup(&client->dev, false);
	free_irq(client->irq, ntag_dev);

	ntag_misc_unregister(ntag_dev, DEV_COUNT);
	mutex_destroy(&ntag_dev->dev_ref_mutex);

	if (gpio_is_valid(ntag_dev->configs.gpio.irq))
		gpio_free(ntag_dev->configs.gpio.irq);
	if (gpio_is_valid(ntag_dev->configs.gpio.hpd))
		gpio_free(ntag_dev->configs.gpio.hpd);

	kfree(ntag_dev->read_kbuf);
	kfree(ntag_dev->write_kbuf);
	kfree(ntag_dev);

	return ret;
}
#endif

int ntag_i2c_dev_suspend(struct device *device)
{
	struct i2c_client *client = to_i2c_client(device);
	struct ntag_dev *ntag_dev = i2c_get_clientdata(client);
	struct i2c_dev *i2c_dev = NULL;

	if (!ntag_dev) {
		pr_err("NtagDrv: %s: device doesn't exist anymore\n", __func__);
		return -ENODEV;
	}

	i2c_dev = &ntag_dev->i2c_dev;

	if (device_may_wakeup(&client->dev) && i2c_dev->irq_enabled) {
		if (!enable_irq_wake(client->irq))
			i2c_dev->irq_wake_up = true;
	}
	pr_debug("NtagDrv: %s: irq_wake_up = %d", __func__,
			i2c_dev->irq_wake_up);
	return 0;
}

int ntag_i2c_dev_resume(struct device *device)
{
	struct i2c_client *client = to_i2c_client(device);
	struct ntag_dev *ntag_dev = i2c_get_clientdata(client);
	struct i2c_dev *i2c_dev = NULL;

	if (!ntag_dev) {
		pr_err("NtagDrv: %s: device doesn't exist anymore\n", __func__);
		return -ENODEV;
	}

	i2c_dev = &ntag_dev->i2c_dev;

	if (device_may_wakeup(&client->dev) && i2c_dev->irq_wake_up) {
		if (!disable_irq_wake(client->irq))
			i2c_dev->irq_wake_up = false;
	}
	pr_debug("NtagDrv: %s: irq_wake_up = %d", __func__,
			i2c_dev->irq_wake_up);
	return 0;
}

static const struct of_device_id ntag_i2c_dev_match_table[] = {
	{.compatible = NTAG_I2C_DRV_STR,},
	{}
};

static const struct dev_pm_ops ntag_i2c_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(ntag_i2c_dev_suspend, ntag_i2c_dev_resume)
};

static struct i2c_driver ntag_i2c_dev_driver = {
	.probe = ntag_i2c_dev_probe,
	.remove = ntag_i2c_dev_remove,
	.driver = {
		.name = NTAG_I2C_DRV_STR,
		.pm = &ntag_i2c_dev_pm_ops,
		.of_match_table = ntag_i2c_dev_match_table,
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
	},
};

MODULE_DEVICE_TABLE(of, ntag_i2c_dev_match_table);

static int __init ntag_i2c_dev_init(void)
{
	int ret = 0;
	pr_info("NtagDrv: %s: loading FM NTAG I2C driver\n", __func__);
	ret = i2c_add_driver(&ntag_i2c_dev_driver);
	if (ret != 0)
		pr_err("NtagDrv: %s: NTAG I2C add driver error ret %d\n", __func__, ret);
	return ret;
}

module_init(ntag_i2c_dev_init);

static void __exit ntag_i2c_dev_exit(void)
{
	pr_info("NtagDrv: %s: Unloading FM NTAG I2C driver\n", __func__);
	i2c_del_driver(&ntag_i2c_dev_driver);
}

module_exit(ntag_i2c_dev_exit);

MODULE_DESCRIPTION("FM NTAG I2C driver");
MODULE_LICENSE("GPL v2");
