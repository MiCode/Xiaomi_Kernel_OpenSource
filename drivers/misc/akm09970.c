/* drivers/intput/misc/akm09970.c - akm09970 compass driver
 *
 * Copyright (c) 2018-2019, Linux Foundation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

//#define DEBUG
#define pr_fmt(fmt) "akm09970: %s: %d " fmt, __func__, __LINE__

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/regmap.h>
#include <linux/atomic.h>
#include <linux/of.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/ioctl.h>
#include <linux/kernel.h>
#include <linux/fcntl.h>
#include <linux/stat.h>
#include <linux/unistd.h>
#include <linux/initrd.h>
#include <linux/init.h>
#include <linux/string.h>
#include <asm/uaccess.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/hrtimer.h>
#include <linux/regulator/consumer.h>
#include "uapi/linux/akm09970.h"

#define CLEAR_IRQ_TIME 20

static DECLARE_WAIT_QUEUE_HEAD(poll_wait_queue);

struct akm09970_soc_ctrl {
	uint8_t chip_info[AKM_SENSOR_INFO_SIZE];
	uint8_t chip_data[AKM_SENSOR_DATA_SIZE];
	uint8_t measure_range;
	uint32_t measure_freq_hz;
	bool read_flag;

	atomic_t power_enabled;
	atomic_t data_ready;

	int gpio_reset;
	int gpio_irq;
	int irq;

	struct akm09970_platform_data pdata;

	dev_t dev_num;
	struct device *dev;
	struct cdev cdev;
	struct class *chr_class;
	struct device *chr_dev;

	struct device_node *of_node;

	struct i2c_client *client;

	struct pinctrl *pinctrl;
	struct pinctrl_state *gpio_state_active;
	struct pinctrl_state *gpio_state_suspend;

	struct regulator *vdd;

	struct hrtimer timer;

	struct work_struct report_work;
	struct workqueue_struct *work_queue;
};


static int akm09970_write_byte(struct i2c_client *client, u8 reg, u8 val)
{
	int rc;

	if (client->irq)
		disable_irq_nosync(client->irq);

	rc = i2c_smbus_write_byte_data(client, reg, val);

	if (client->irq)
		enable_irq(client->irq);

	return rc;
}

static int akm09970_set_reg_bits(struct i2c_client *client,
					int val, int shift, u8 mask, u8 reg)
{
	int data;

	data = i2c_smbus_read_byte_data(client, reg);
	if (data < 0)
		return data;

	data = (data & ~mask) | ((val << shift) & mask);
	pr_info("reg: 0x%x, data: 0x%x", reg, data);
	return akm09970_write_byte(client, reg, data);
}

static int akm09970_set_mode(
	struct akm09970_soc_ctrl *c_ctrl,
	uint8_t mode)
{
	int rc;

	rc = akm09970_set_reg_bits(c_ctrl->client, mode, AK09970_MODE_POS,
				AK09970_MODE_MSK, AK09970_MODE_REG);

	return rc;
}

static enum hrtimer_restart hrtimer_handler(struct hrtimer *timer_t)
{
	struct akm09970_soc_ctrl *c_ctrl = container_of(timer_t, struct akm09970_soc_ctrl, timer);

	if (!c_ctrl->read_flag)
		queue_work(c_ctrl->work_queue, &c_ctrl->report_work);

	return HRTIMER_NORESTART;
}

static void akm09970_reset(
	struct akm09970_soc_ctrl *c_ctrl)
{
	gpio_set_value(c_ctrl->gpio_reset, 0);
	udelay(50);
	gpio_set_value(c_ctrl->gpio_reset, 1);
	udelay(100);

	return;
}

static int akm09970_power_down(struct akm09970_soc_ctrl *c_ctrl)
{
	int rc = 0;

	if (atomic_read(&c_ctrl->power_enabled)) {
		gpio_set_value(c_ctrl->gpio_reset, 0);

		rc = regulator_disable(c_ctrl->vdd);
		if (rc) {
			pr_err("Regulator vdd disable failed rc=%d\n", rc);
		}
		atomic_set(&c_ctrl->power_enabled, 0);
		pr_debug("Power down successfully");
	}

	return rc;
}

static int akm09970_power_up(struct akm09970_soc_ctrl *c_ctrl)
{
	int rc = 0;

	if (!atomic_read(&c_ctrl->power_enabled)) {
		rc = regulator_enable(c_ctrl->vdd);
		if (rc) {
			pr_err("Regulator vdd enable failed rc=%d\n", rc);
			return rc;
		}
		udelay(20);
		akm09970_reset(c_ctrl);
		atomic_set(&c_ctrl->power_enabled, 1);
		pr_debug("Power down successfully");
	}

	return rc;
}

static int akm09970_active(struct akm09970_soc_ctrl *c_ctrl, bool on)
{
	int rc = 0;
	uint8_t mode = 0x00;

	pr_info("akm sensor %s\n", on ? "on" : "off");

	if (!atomic_read(&c_ctrl->power_enabled) && on) {
		rc = akm09970_power_up(c_ctrl);
		if (rc) {
			pr_err("Sensor power up fail!\n");
			return rc;
		}

		if (c_ctrl->measure_freq_hz >= 100)
			mode = AK09970_MODE_CONTINUOUS_100HZ;
		else if (c_ctrl->measure_freq_hz >= 50 && c_ctrl->measure_freq_hz < 100)
			mode = AK09970_MODE_CONTINUOUS_50HZ;
		else if (c_ctrl->measure_freq_hz >= 20 && c_ctrl->measure_freq_hz < 50)
			mode = AK09970_MODE_CONTINUOUS_20HZ;
		else
			mode = AK09970_MODE_CONTINUOUS_10HZ;

		c_ctrl->measure_range = 0;

		rc = akm09970_write_byte(c_ctrl->client, AK09970_MODE_REG, (mode | c_ctrl->measure_range));
		if (rc < 0) {
			pr_err("Failed to set mode and smr.");
			akm09970_power_down(c_ctrl);
			return rc;
		}
		pr_info("reg: 0x%x, data: 0x%x", AK09970_MODE_REG, (mode | c_ctrl->measure_range));

		enable_irq(c_ctrl->irq);
		hrtimer_start(&c_ctrl->timer,
			ktime_set(CLEAR_IRQ_TIME / MSEC_PER_SEC,
			(CLEAR_IRQ_TIME % MSEC_PER_SEC) * NSEC_PER_MSEC),
			HRTIMER_MODE_REL);
		c_ctrl->read_flag = false;
		pr_debug("Enable irq successfully");
	} else if (atomic_read(&c_ctrl->power_enabled) && !on) {
		disable_irq_nosync(c_ctrl->irq);
		cancel_work_sync(&c_ctrl->report_work);
		c_ctrl->read_flag = false;
		pr_err("Disable irq successfully");

		rc = akm09970_set_mode(c_ctrl, AK09970_MODE_POWERDOWN);
		if (rc)
			pr_warn("Failed to set to POWERDOWN mode.\n");

		akm09970_power_down(c_ctrl);
	} else {
		pr_info("The same power state, do nothing!");
	}

	return 0;
}

static int akm09970_read_data(struct akm09970_soc_ctrl *c_ctrl)
{
	int rc = 0;

	rc = i2c_smbus_read_i2c_block_data(c_ctrl->client, AK09970_REG_ST_XYZ, AKM_SENSOR_DATA_SIZE, c_ctrl->chip_data);
	if (rc < 0) {
		pr_err("read data failed!");
		return rc;
	}

	if (AKM_ERRADC_IS_HIGH(c_ctrl->chip_data[0])) {
		pr_err("ADC over run!\n");
		rc = -EIO;
	}

	if (AKM_ERRXY_IS_HIGH(c_ctrl->chip_data[1])) {
		pr_err("Errxy over run!\n");
		rc = -EIO;
	}

	return 0;
}

static void akm09970_dev_work_queue(struct work_struct *work)
{
	int rc = 0;
	struct akm09970_soc_ctrl *c_ctrl =
		container_of(work, struct akm09970_soc_ctrl, report_work);

	rc = akm09970_read_data(c_ctrl);
	if (rc < 0) {
		atomic_set(&c_ctrl->data_ready, 0);
		pr_warn("Failed to read data");
	} else {
		atomic_set(&c_ctrl->data_ready, 1);
		wake_up(&poll_wait_queue);
		c_ctrl->read_flag = true;
	}
}

static irqreturn_t akm09970_irq_handler(int irq, void *dev_id)
{
	struct akm09970_soc_ctrl *c_ctrl = dev_id;

	queue_work(c_ctrl->work_queue, &c_ctrl->report_work);

	return IRQ_HANDLED;
}

static int akm09970_release(struct inode *inp, struct file *filp)
{
	int rc = 0;

	return rc;
}

static int akm09970_open(struct inode *inp, struct file *filp)
{
	int rc = 0;
	struct akm09970_soc_ctrl *c_ctrl =
		container_of(inp->i_cdev, struct akm09970_soc_ctrl, cdev);

	filp->private_data = c_ctrl;

	pr_debug("open enter, irq = %d\n", c_ctrl->irq);

	return rc;
}

static ssize_t akm09970_write(struct file *filp, const char *buf, size_t len, loff_t *fseek)
{
	int rc = 0;

	return rc;
}

static ssize_t akm09970_read(struct file *filp, char *buf, size_t len, loff_t *fseek)
{
	int rc = 0;

	return rc;
}

static unsigned int akm09970_poll(struct file *filp, poll_table *wait)
{
	unsigned int mask = 0;
	struct akm09970_soc_ctrl *c_ctrl = filp->private_data;

	pr_debug("Poll enter\n");

	poll_wait(filp, &poll_wait_queue, wait);
	if (atomic_read(&c_ctrl->data_ready)) {
		atomic_set(&c_ctrl->data_ready, 0);
		mask = POLLIN | POLLRDNORM;
	}

	return mask;
}

static long akm09970_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int rc = 0;
	int i = 0;
	struct akm09970_soc_ctrl *c_ctrl = filp->private_data;

	if (NULL == c_ctrl) {
		pr_err("NULL");
		return -EFAULT;
	}

	if (_IOC_TYPE(cmd) != AKM_IOC_MAGIC) {
		pr_err("magic number worng");
		return -ENODEV;
	}

	if (_IOC_DIR(cmd) & _IOC_READ)
		rc = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		rc = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	if (rc) {
		pr_err("access failed");
		return -EFAULT;
	}

	if (_IOC_DIR(cmd) & _IOC_WRITE) {
		if (copy_from_user(&c_ctrl->pdata, (void __user *)arg, sizeof(struct akm09970_platform_data))) {
			pr_err("Copy data from user space failed");
			return -EFAULT;
		}
	}

	switch (cmd) {
	case AKM_IOC_SET_ACTIVE:
		pr_err("AKM_IOC_SET_ACTIVE, c_ctrl->pdata.sensor_state = %d\n", c_ctrl->pdata.sensor_state);
		rc = akm09970_active(c_ctrl, c_ctrl->pdata.sensor_state);
		break;
	case AKM_IOC_SET_MODE:
		pr_err("AKM_IOC_SET_MODE, c_ctrl->pdata.sensor_mode = %d\n", c_ctrl->pdata.sensor_mode);
		c_ctrl->measure_freq_hz = c_ctrl->pdata.sensor_mode;
		//rc = akm09970_set_mode(pctrl, mode);
		break;
	case AKM_IOC_GET_SENSSMR:
		pr_err("AKM_IOC_GET_SENSSMR, c_ctrl->measure_range = %d", c_ctrl->measure_range);
		c_ctrl->pdata.sensor_smr = c_ctrl->measure_range;
		break;
	case AKM_IOC_GET_SENSEDATA:
		pr_debug("AKM_IOC_GET_SENSEDATA");
		for (i = 0; i < AKM_SENSOR_DATA_SIZE; i++) {
			c_ctrl->pdata.data[i] = c_ctrl->chip_data[i];
			pr_debug("data%d = %d\n", i, c_ctrl->chip_data[i]);
		}
		break;
	default:
		pr_warn("unsupport cmd:0x%x\n", cmd);
		break;
	}

	if (_IOC_DIR(cmd) & _IOC_READ) {
		if (copy_to_user((void __user *)arg, &c_ctrl->pdata, sizeof(struct akm09970_platform_data))) {
			pr_err("Copy data from user space failed");
			return -EFAULT;
		}
	}

	return rc;
}

#ifdef CONFIG_COMPAT
static long akm09970_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	return akm09970_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
}
#endif

static const struct file_operations akm09970_fops = {
	.owner = THIS_MODULE,
	.open = akm09970_open,
	.release = akm09970_release,
	.read = akm09970_read,
	.write = akm09970_write,
	.unlocked_ioctl = akm09970_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = akm09970_compat_ioctl,
#endif
	.poll = akm09970_poll,
};


static int akm09970_check_device_id(struct akm09970_soc_ctrl *c_ctrl)
{
	int rc = 0;

	rc = akm09970_power_up(c_ctrl);
	if (rc < 0)
		return rc;

	rc = i2c_smbus_read_i2c_block_data(c_ctrl->client, AK09970_REG_WIA, AKM_SENSOR_INFO_SIZE, c_ctrl->chip_info);
	if (rc < 0)
		goto exit;

	if ((c_ctrl->chip_info[0] != AK09970_WIA1_VALUE) ||
			(c_ctrl->chip_info[1] != AK09970_WIA2_VALUE)) {
		pr_err("The device is not AKM Hall sensor\n");
		rc = -ENXIO;
	}

exit:
	akm09970_power_down(c_ctrl);

	return rc;
}

static ssize_t akm09970_chip_rev_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct akm09970_soc_ctrl *c_ctrl = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%x,%x\n", (char)c_ctrl->chip_info[0], (char)c_ctrl->chip_info[1]);
}
static DEVICE_ATTR(chip_rev, S_IRUGO, akm09970_chip_rev_show, NULL);

static struct device_attribute *akm_attrs[] = {
	&dev_attr_chip_rev,
};

static int akm09970_regulator_init(struct akm09970_soc_ctrl *c_ctrl, bool on)
{
	int rc;

	if (on) {
		c_ctrl->vdd = regulator_get(&c_ctrl->client->dev, "vdd");
		if (IS_ERR(c_ctrl->vdd)) {
			rc = PTR_ERR(c_ctrl->vdd);
			pr_err("Regulator get failed vdd rc=%d", rc);
			return rc;
		}

		if (regulator_count_voltages(c_ctrl->vdd) > 0) {
			rc = regulator_set_voltage(c_ctrl->vdd,
				AKM09970_VDD_MIN_UV, AKM09970_VDD_MAX_UV);
			if (rc) {
				pr_err("Regulator set failed vdd rc=%d",
					rc);
				regulator_put(c_ctrl->vdd);
				return rc;
			}
		}
	} else {
		if (regulator_count_voltages(c_ctrl->vdd) > 0)
			regulator_set_voltage(c_ctrl->vdd, 0, AKM09970_VDD_MAX_UV);

		regulator_put(c_ctrl->vdd);
	}

	return 0;
}

static int akm09970_gpio_config(struct akm09970_soc_ctrl *c_ctrl)
{
	int32_t rc = 0;

	rc = gpio_request_one(c_ctrl->gpio_reset, GPIOF_OUT_INIT_LOW, "akm09970-reset");
	if (rc < 0) {
		pr_err("Failed to request power enable GPIO %d", c_ctrl->gpio_reset);
		goto reset_gpio_req_err;
	}
	gpio_direction_output(c_ctrl->gpio_reset, 0);

	rc = gpio_request_one(c_ctrl->gpio_irq, GPIOF_IN, "akm09970-irq");
	if (rc < 0) {
		pr_err("Failed to request power enable GPIO %d", c_ctrl->gpio_irq);
		goto irq_gpio_req_err;
	} else {
		gpio_direction_input(c_ctrl->gpio_irq);
		c_ctrl->irq = gpio_to_irq(c_ctrl->gpio_irq);
		rc = request_threaded_irq(c_ctrl->irq, akm09970_irq_handler, NULL,
			IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
			"akm09970_irq", c_ctrl);
		if (rc < 0) {
			pr_err("Unable to request irq\n");
			goto irq_req_err;
		}
		disable_irq_nosync(c_ctrl->irq);
	}

	return 0;

irq_req_err:
	if (gpio_is_valid(c_ctrl->gpio_irq)) {
		if (c_ctrl->irq)
			free_irq(c_ctrl->irq, c_ctrl);
		gpio_free(c_ctrl->gpio_irq);
	}

irq_gpio_req_err:
	if (gpio_is_valid(c_ctrl->gpio_reset))
		gpio_free(c_ctrl->gpio_reset);

reset_gpio_req_err:
	return rc;
}


static int akm09970_pinctrl_select(struct akm09970_soc_ctrl *c_ctrl, bool state)
{
	int rc = 0;
	struct pinctrl_state *pins_state =
		state ? (c_ctrl->gpio_state_active) : (c_ctrl->gpio_state_suspend);

	rc = pinctrl_select_state(c_ctrl->pinctrl, pins_state);
	if (rc < 0)
		pr_err("Failed to select pins state %s\n",
			state ? "active" : "suspend");

	return rc;
}

static int akm09970_pinctrl_init(struct akm09970_soc_ctrl *c_ctrl)
{
	int rc = 0;
	struct device *dev = c_ctrl->dev;

	c_ctrl->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR_OR_NULL(c_ctrl->pinctrl)) {
		rc = PTR_ERR(c_ctrl->pinctrl);
		pr_err("Unable to acquire pinctrl %d", rc);
		goto err_pinctrl_get;
	}

	c_ctrl->gpio_state_active =
		pinctrl_lookup_state(c_ctrl->pinctrl, "akm09970_gpio_active");
	if (IS_ERR_OR_NULL(c_ctrl->gpio_state_active)) {
		pr_err("Cannot lookup active pinctrl state\n");
		rc = PTR_ERR(c_ctrl->gpio_state_active);
		goto err_pinctrl_lookup;
	}

	c_ctrl->gpio_state_suspend =
		pinctrl_lookup_state(c_ctrl->pinctrl, "akm09970_gpio_suspend");
	if (IS_ERR_OR_NULL(c_ctrl->gpio_state_suspend)) {
		pr_err("Cannot lookup suspend pinctrl state\n");
		rc = PTR_ERR(c_ctrl->gpio_state_suspend);
		goto err_pinctrl_lookup;
	}

	return 0;

err_pinctrl_lookup:
	devm_pinctrl_put(c_ctrl->pinctrl);
err_pinctrl_get:
	c_ctrl->pinctrl = NULL;

	return rc;
}

static int akm09970_parse_dt(struct device *dev, struct akm09970_soc_ctrl *c_ctrl)
{
	int rc = 0;

	c_ctrl->gpio_reset = of_get_named_gpio_flags(dev->of_node, "akm,gpio-reset", 0, NULL);
	if (!gpio_is_valid(c_ctrl->gpio_reset)) {
		pr_err("Gpio reset pin %d is invalid.", c_ctrl->gpio_reset);
		return -EINVAL;
	}

	c_ctrl->gpio_irq = of_get_named_gpio_flags(dev->of_node, "akm,gpio-irq", 0, NULL);
	if (!gpio_is_valid(c_ctrl->gpio_irq)) {
		pr_err("gpio irq pin %d is invalid.", c_ctrl->gpio_irq);
		return -EINVAL;
	}

	return rc;
}

static int akm09970_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int rc = 0;
	struct akm09970_soc_ctrl *c_ctrl = NULL;

	pr_debug("Probe enter");

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("check_functionality failed.");
		return -ENODEV;
	}

	/* Allocate memory for driver data */
	c_ctrl = devm_kzalloc(&client->dev, sizeof(struct akm09970_soc_ctrl), GFP_KERNEL);
	if (!c_ctrl) {
		pr_err("Alloc memory failed.");
		return -ENOMEM;
	}

	pr_debug("Start parse device tree");
	if (client->dev.of_node) {
		rc = akm09970_parse_dt(&client->dev, c_ctrl);
		if (rc < 0) {
			pr_err("Unable to parse platfrom data rc=%d\n", rc);
			goto exit;
		}
	}

	c_ctrl->dev = &client->dev;
	c_ctrl->client = client;
	i2c_set_clientdata(client, c_ctrl);

	rc = akm09970_pinctrl_init(c_ctrl);
	if (rc) {
		pr_err("Failed to initialize pinctrl\n");
		goto exit;
	} else {
		if (c_ctrl->pinctrl) {
			rc = akm09970_pinctrl_select(c_ctrl, true);
			if (rc < 0) {
				pr_err("Failed to select default pinstate %d\n", rc);
				goto err_select_pinctrl;
			}
		}
	}

	rc = akm09970_gpio_config(c_ctrl);
	if (rc < 0) {
		pr_err("Failed to config gpio\n");
		goto err_select_pinctrl;
	}

	rc = akm09970_regulator_init(c_ctrl, true);
	if (rc < 0)
		goto err_regulator_init;

	rc = akm09970_check_device_id(c_ctrl);
	if (rc < 0)
		goto err_check_device;

	atomic_set(&c_ctrl->power_enabled, 0);

	pr_err("IRQ is #%d.", c_ctrl->irq);

	hrtimer_init(&c_ctrl->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	c_ctrl->timer.function = hrtimer_handler;
	c_ctrl->work_queue = alloc_workqueue("akm09970_poll_work_queue",
		WQ_UNBOUND | WQ_MEM_RECLAIM | WQ_HIGHPRI, 1);
	INIT_WORK(&c_ctrl->report_work, akm09970_dev_work_queue);

	c_ctrl->chr_class = class_create(THIS_MODULE, AKM09970_CLASS_NAME);
	if (c_ctrl->chr_class == NULL) {
		pr_err("Failed to create class.\n");
		rc = -ENODEV;
		goto err_check_device;
	}

	rc = alloc_chrdev_region(&c_ctrl->dev_num, 0, 1, AKM09970_DRV_NAME);
	if (rc < 0) {
		pr_err("Failed to allocate chrdev region\n");
		goto err_destroy_class;
	}

	c_ctrl->chr_dev = device_create(c_ctrl->chr_class, NULL,
					c_ctrl->dev_num, c_ctrl, AKM09970_DRV_NAME);
	if (IS_ERR(c_ctrl->chr_dev)) {
		pr_err("Failed to create char device\n");
		rc = PTR_ERR(c_ctrl->chr_dev);
		goto err_unregister_chrdev;
	}

	cdev_init(&(c_ctrl->cdev), &akm09970_fops);
	c_ctrl->cdev.owner = THIS_MODULE;

	rc = cdev_add(&(c_ctrl->cdev), c_ctrl->dev_num, 1);
	if (rc < 0) {
		pr_err("Failed to add cdev\n");
		goto err_destroy_device;
	}

	rc = device_create_file(c_ctrl->chr_dev, akm_attrs[0]);
	if (rc < 0)
		pr_err("Failed to create debug file: %d\n", rc);

	pr_err("Probe exit");

	return 0;


err_destroy_device:
	if (c_ctrl->chr_dev)
		device_destroy(c_ctrl->chr_class, c_ctrl->dev_num);

err_unregister_chrdev:
	unregister_chrdev_region(c_ctrl->dev_num, 1);
err_destroy_class:
	if (c_ctrl->chr_class)
		class_destroy(c_ctrl->chr_class);

err_check_device:
	akm09970_regulator_init(c_ctrl, false);

err_regulator_init:
	if (gpio_is_valid(c_ctrl->gpio_irq)) {
		if (c_ctrl->irq)
			free_irq(c_ctrl->irq, c_ctrl);
		gpio_free(c_ctrl->gpio_irq);
	}

	if (gpio_is_valid(c_ctrl->gpio_reset))
		gpio_free(c_ctrl->gpio_reset);

err_select_pinctrl:
	if (c_ctrl->pinctrl) {
		devm_pinctrl_put(c_ctrl->pinctrl);
		c_ctrl->pinctrl = NULL;
	}

exit:
	return rc;
}

static int akm09970_remove(struct i2c_client *client)
{
	struct akm09970_soc_ctrl *c_ctrl = i2c_get_clientdata(client);

	cancel_work_sync(&c_ctrl->report_work);
	destroy_workqueue(c_ctrl->work_queue);

	if (&(c_ctrl->cdev))
		cdev_del(&(c_ctrl->cdev));

	if (c_ctrl->chr_dev)
		device_destroy(c_ctrl->chr_class, c_ctrl->dev_num);

	unregister_chrdev_region(c_ctrl->dev_num, 1);
	if (c_ctrl->chr_class)
		class_destroy(c_ctrl->chr_class);

	akm09970_power_down(c_ctrl);

	akm09970_regulator_init(c_ctrl, false);

	if (gpio_is_valid(c_ctrl->gpio_irq)) {
		if (c_ctrl->irq)
			free_irq(c_ctrl->irq, c_ctrl);
		gpio_free(c_ctrl->gpio_irq);
	}

	if (gpio_is_valid(c_ctrl->gpio_reset))
		gpio_free(c_ctrl->gpio_reset);

	if (c_ctrl->pinctrl) {
		devm_pinctrl_put(c_ctrl->pinctrl);
		c_ctrl->pinctrl = NULL;
	}

	pr_debug("Removed exit");

	return 0;
}

static const struct i2c_device_id akm09970_id[] = {
	{AKM09970_DRV_NAME, 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, akm09970_id);

static struct of_device_id akm09970_match_table[] = {
	{ .compatible = "akm,akm09970", },
	{ },
};

static struct i2c_driver akm09970_driver = {
	.driver = {
		.name   = AKM09970_DRV_NAME,
		.owner  = THIS_MODULE,
		.of_match_table = akm09970_match_table,
	},
	.probe		= akm09970_probe,
	.remove		= akm09970_remove,
	.id_table	= akm09970_id,
};

module_i2c_driver(akm09970_driver);

MODULE_AUTHOR("zhunengjin <zhunengjin@xiaomi.com>");
MODULE_DESCRIPTION("AKM compass driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("2.0");
