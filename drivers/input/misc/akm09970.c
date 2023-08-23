/* drivers/intput/misc/akm09970.c - akm09970 compass driver
 *
 * Copyright (c) 2014-2015, Linux Foundation. All rights reserved.
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

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/freezer.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/fcntl.h>
#include <linux/poll.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/seq_file.h>
#include <linux/pwm.h>
#include <linux/mm.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/hrtimer.h>
#include <linux/debugfs.h>
#include <uapi/misc/akm09970.h>

#define AKM_DRDY_TIMEOUT_MS		100
#define AKM_DEFAULT_MEASURE_HZ	10
#define AKM_I2C_NAME			"akm09970"

#define MAKE_S16(U8H, U8L) \
	(int16_t)(((uint16_t)(U8H) << 8) | (uint16_t)(U8L))

/* POWER SUPPLY VOLTAGE RANGE */
#define AKM09970_VDD_MIN_UV 1800000
#define AKM09970_VDD_MAX_UV 1800000

#define PWM_PERIOD_DEFAULT_NS 1000000


struct akm09970_soc_ctrl {
	struct i2c_client *client;
	struct input_dev *input;
	struct miscdevice miscdev;
	struct dentry *debugfs;

	struct device_node *of_node;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pinctrl_default;
//	struct pinctrl_state *pinctrl_sleep;

	struct regulator *vdd;
	int gpio_rstn;
	int gpio_irq;

	int power_enabled;

	struct work_struct report_work;
	struct workqueue_struct *work_queue;

	struct mutex soc_mutex;
	wait_queue_head_t drdy_wq;
	atomic_t drdy;

	int irq;

	uint8_t sense_info[AKM_SENSOR_INFO_SIZE];
//	uint8_t sense_conf[AKM_SENSOR_CONF_SIZE];
	uint8_t sense_data[AKM_SENSOR_DATA_SIZE];
	int32_t magnet_data[3];
	uint8_t layout;

	uint32_t measure_freq_hz;
	uint8_t measure_range;
};


static int akm_write_byte(struct i2c_client *client, u8 reg, u8 val)
{
	int rc;

	/* As per specification, disable irq in between register writes */
	if (client->irq)
		disable_irq_nosync(client->irq);

	rc = i2c_smbus_write_byte_data(client, reg, val);

	if (client->irq)
		enable_irq(client->irq);

	return rc;
}

static int akm_set_reg_bits(struct i2c_client *client,
					int val, int shift, u8 mask, u8 reg)
{
	int data;

	data = i2c_smbus_read_byte_data(client, reg);
	if (data < 0)
		return data;

	data = (data & ~mask) | ((val << shift) & mask);
	pr_info("reg: 0x%x, data: 0x%x", reg, data);
	return akm_write_byte(client, reg, data);
}

static int akm_set_smr(
	struct akm09970_soc_ctrl *pctrl,
	uint8_t mode)
{
	int rc;

	rc = akm_set_reg_bits(pctrl->client, mode, AK09970_SMR_MODE_POS,
				AK09970_SMR_MODE_MSK, AK09970_SMR_MODE_REG);

	return rc;
}

static int akm_set_mode(
	struct akm09970_soc_ctrl *pctrl,
	uint8_t mode)
{
	int rc;

	rc = akm_set_reg_bits(pctrl->client, mode, AK09970_MODE_POS,
				AK09970_MODE_MSK, AK09970_MODE_REG);

	return rc;
}

static void akm_reset(
	struct akm09970_soc_ctrl *pctrl)
{
	gpio_set_value(pctrl->gpio_rstn, 0);
	udelay(20);
	gpio_set_value(pctrl->gpio_rstn, 1);
	udelay(100);

	atomic_set(&pctrl->drdy, 0);
	return;
}

static int akm_power_down(struct akm09970_soc_ctrl *pctrl)
{
	int rc = 0;

	if (pctrl->power_enabled) {
		gpio_set_value(pctrl->gpio_rstn, 0);

		rc = regulator_disable(pctrl->vdd);
		if (rc) {
			pr_err("Regulator vdd disable failed rc=%d\n", rc);
		}
		pr_debug("power down");
		atomic_set(&pctrl->drdy, 0);
	}

	return rc;
}

static int akm_power_up(struct akm09970_soc_ctrl *pctrl)
{
	int rc = 0;

	if (!pctrl->power_enabled) {
		rc = regulator_enable(pctrl->vdd);
		if (rc) {
			pr_err("Regulator vdd enable failed rc=%d\n", rc);
			return rc;
		}
		pr_info("power up");

		akm_reset(pctrl);

		return rc;
	}

	return rc;
}

/* This function will block a process until the latest measurement
 * data is available.
 */
static int akm_get_data(
	struct akm09970_soc_ctrl *pctrl,
	int *rbuf,
	int size)
{
	long timeout;

	/* Block! */
	timeout = wait_event_interruptible_timeout(
			pctrl->drdy_wq,
			atomic_read(&pctrl->drdy),
			msecs_to_jiffies(AKM_DRDY_TIMEOUT_MS));

	if (!timeout) {
		pr_err("wait_event timeout");
		return -EIO;
	}

	memcpy(rbuf, pctrl->magnet_data, sizeof(int) * size);
	atomic_set(&pctrl->drdy, 0);

	return 0;
}

static int akm_active(struct akm09970_soc_ctrl *pctrl, bool on)
{
	int rc = 0;
	uint8_t mode;

	pr_info("akm sensor %s\n", on ? "on" : "off");

	if (!pctrl->power_enabled && on) {
		pctrl->power_enabled = true;
		rc = akm_power_up(pctrl);
		if (rc) {
			pr_err("Sensor power up fail!\n");
			pctrl->power_enabled = false;
			return rc;
		}

		if (pctrl->measure_freq_hz >= 100)
			mode = AK09970_MODE_CONTINUOUS_100HZ;
		else if (pctrl->measure_freq_hz >= 50 && pctrl->measure_freq_hz < 100)
			mode = AK09970_MODE_CONTINUOUS_50HZ;
		else if (pctrl->measure_freq_hz >= 20 && pctrl->measure_freq_hz < 50)
			mode = AK09970_MODE_CONTINUOUS_20HZ;
		else
			mode = AK09970_MODE_CONTINUOUS_10HZ;
		rc = akm_set_mode(pctrl, mode);
		if (rc < 0) {
			pr_err("Failed to set to mode(%d)", mode);
			pctrl->power_enabled = false;
			return rc;
		}
		pctrl->measure_range = 0;
		rc = akm_set_smr(pctrl, pctrl->measure_range);
		if (rc < 0) {
			pr_err("Failed to set smr.");
			pctrl->power_enabled = false;
			return rc;
		}
		pr_debug("enable irq");
		enable_irq(pctrl->client->irq);
	} else if (pctrl->power_enabled && !on) {
		pctrl->power_enabled = false;

		disable_irq_nosync(pctrl->client->irq);
		cancel_work_sync(&pctrl->report_work);
		pr_debug("disable irq");

		rc = akm_set_mode(pctrl, AK09970_MODE_POWERDOWN);
		if (rc)
			pr_warn("Failed to set to POWERDOWN mode.\n");

		akm_power_down(pctrl);
	} else {
		pr_info("power state is the same, do nothing!");
	}

	return 0;
}

static long akm09970_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int rc = 0;
	uint8_t active = 0;
	uint8_t mode = AK09970_MODE_POWERDOWN;
	int dat_buf[3];/* for GET_DATA */
	int state = 1;
	struct akm09970_soc_ctrl *pctrl = NULL;

	pctrl = container_of(filp->private_data, struct akm09970_soc_ctrl, miscdev);

	if (pctrl == NULL)
		return -EFAULT;

//	mutex_lock(&pctrl->soc_mutex);

	switch (cmd) {
	case AKM_IOC_SET_ACTIVE:
		pr_debug("AKM_IOC_SET_ACTIVE.");
		if (copy_from_user(&active, (uint8_t *)arg, sizeof(uint8_t))) {
			pr_err("Failed to active status from user to kernel\n");
			rc = -EFAULT;
			break;
		}
		rc = akm_active(pctrl, !!active);
		break;
	case AKM_IOC_SET_MODE:
		pr_debug("AKM_IOC_SET_MODE.");
		if (copy_from_user(&mode, (uint8_t *)arg, sizeof(uint8_t))) {
			pr_err("Failed to copy mode from user to kernel\n");
			rc = -EFAULT;
			break;
		}
		pctrl->measure_freq_hz = mode;
		//rc = akm_set_mode(pctrl, mode);
		break;
	case AKM_IOC_SET_PRESS:
		pr_debug("AKM_IOC_SET_PRESS.");
		input_event(pctrl->input, EV_KEY, KEY_BACK, state);
		input_sync(pctrl->input);
		input_event(pctrl->input, EV_KEY, KEY_BACK, !state);
		input_sync(pctrl->input);
		break;
	case AKM_IOC_GET_SENSSMR:
		if (copy_to_user((void __user *)arg, &pctrl->measure_range, sizeof(uint8_t))) {
			pr_err("copy_to_user failed.");
			return -EFAULT;
		}
		break;
	case AKM_IOC_GET_SENSEDATA:
		pr_info("AKM_IOC_GET_SENSEDATA.");
		rc = akm_get_data(pctrl, dat_buf, 3);
		if (rc < 0)
			return rc;
		if (copy_to_user((void __user *)arg, dat_buf, sizeof(dat_buf))) {
			pr_err("copy_to_user failed.");
			return -EFAULT;
		}
		break;
	default:
		pr_warn("unsupport cmd:0x%x\n", cmd);
		break;
	}

//	mutex_unlock(&pctrl->soc_mutex);
	return rc;
}

#ifdef CONFIG_COMPAT
static long akm09970_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	return akm09970_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
}
#endif

static const struct file_operations akm09970_fops = {
	.owner =	THIS_MODULE,
	.unlocked_ioctl = akm09970_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = akm09970_compat_ioctl,
#endif
};

/* akm get and report data functions */
static int akm_read_sense_data(
	struct akm09970_soc_ctrl *pctrl,
	uint8_t *rbuf,
	int size)
{
	uint8_t buffer[AKM_SENSOR_DATA_SIZE];
	int rc;

	/* Read data */
	rc = i2c_smbus_read_i2c_block_data(pctrl->client, AK09970_REG_ST_XYZ, AKM_SENSOR_DATA_SIZE, buffer);
	if (rc < 0) {
		pr_err("read data failed!");
		return rc;
	}

	/* ERRADC is */
	if (AKM_ERRADC_IS_HIGH(buffer[0])) {
		pr_err("ADC over run!\n");
		rc = -EIO;
	}

	/* ERRXY is */
	if (AKM_ERRXY_IS_HIGH(buffer[1])) {
		pr_err("errxy over run!\n");
		rc = -EIO;
	}

	/* Check ST bit */
	if (!(AKM_DRDY_IS_HIGH(buffer[1]))) {
		pr_info("DRDY is low. Use last value.");
	} else {
		memcpy(rbuf, buffer, size);
		atomic_set(&pctrl->drdy, 1);

		wake_up(&pctrl->drdy_wq);
	}

	return 0;
}

static int akm_report_data(struct akm09970_soc_ctrl *pctrl)
{
	int rc = 0;
	int tmp, mag_x, mag_y, mag_z;

	rc = akm_read_sense_data(pctrl, pctrl->sense_data, AKM_SENSOR_DATA_SIZE);
	if (rc) {
		pr_err("Get data failed.");
		return -EIO;
	}

	mag_x = MAKE_S16(pctrl->sense_data[2], pctrl->sense_data[3]);
	mag_y = MAKE_S16(pctrl->sense_data[4], pctrl->sense_data[5]);
	mag_z = MAKE_S16(pctrl->sense_data[6], pctrl->sense_data[7]);

	//pr_debug("mag_x:%d mag_y:%d mag_z:%d\n", mag_x, mag_y, mag_z);
	pr_debug("raw data: 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x",
			pctrl->sense_data[0], pctrl->sense_data[1], pctrl->sense_data[2], pctrl->sense_data[3],
			pctrl->sense_data[4], pctrl->sense_data[5], pctrl->sense_data[6], pctrl->sense_data[7]);

	switch (pctrl->layout) {
	case 0:
	case 1:
		/* Fall into the default direction */
		break;
	case 2:
		tmp = mag_x;
		mag_x = mag_y;
		mag_y = -tmp;
		break;
	case 3:
		mag_x = -mag_x;
		mag_y = -mag_y;
		break;
	case 4:
		tmp = mag_x;
		mag_x = -mag_y;
		mag_y = tmp;
		break;
	case 5:
		mag_x = -mag_x;
		mag_z = -mag_z;
		break;
	case 6:
		tmp = mag_x;
		mag_x = mag_y;
		mag_y = tmp;
		mag_z = -mag_z;
		break;
	case 7:
		mag_y = -mag_y;
		mag_z = -mag_z;
		break;
	case 8:
		tmp = mag_x;
		mag_x = -mag_y;
		mag_y = -tmp;
		mag_z = -mag_z;
		break;
	}

	input_report_abs(pctrl->input, ABS_X, mag_x);
	input_report_abs(pctrl->input, ABS_Y, mag_y);
	input_report_abs(pctrl->input, ABS_Z, mag_z);

	pctrl->magnet_data[0] = mag_x;
	pctrl->magnet_data[1] = mag_y;
	pctrl->magnet_data[2] = mag_z;

	input_sync(pctrl->input);

//	enable_irq(pctrl->client->irq);

	return 0;
}

static void akm_dev_input_work(struct work_struct *work)
{
	int rc = 0;
	struct akm09970_soc_ctrl *pctrl;

	pctrl = container_of(work, struct akm09970_soc_ctrl, report_work);

	rc = akm_report_data(pctrl);
	if (rc < 0)
		pr_warn("Failed to report data.");
}

static irqreturn_t akm_irq(int irq, void *handle)
{
	struct akm09970_soc_ctrl *pctrl = handle;

//	disable_irq_nosync(pctrl->client->irq);
	queue_work(pctrl->work_queue, &pctrl->report_work);

	pr_debug("enter");
	return IRQ_HANDLED;
}

int akm_input_open(struct input_dev *dev)
{
	pr_info("enter!!!!!");
	return 0;
}

void akm_input_close(struct input_dev *dev)
{
	pr_info("enter!!!!!");
	return;
}

static int akm09970_input_register(
	struct input_dev **input)
{
	int rc = 0;

	/* Declare input device */
	*input = input_allocate_device();
	if (!*input)
		return -ENOMEM;

	/* Setup input device */
	set_bit(EV_ABS, (*input)->evbit);
	input_set_abs_params(*input, ABS_X,
			-32767, 32767, 0, 0);
	input_set_abs_params(*input, ABS_Y,
			-32767, 32767, 0, 0);
	input_set_abs_params(*input, ABS_Z,
			-32767, 32767, 0, 0);

	set_bit(EV_KEY, (*input)->evbit);
	input_set_capability(*input, EV_KEY, KEY_BACK);
	/* Set name */
	(*input)->name = AKM_INPUT_DEVICE_NAME;
	(*input)->open = akm_input_open;
	(*input)->close = akm_input_close;

	/* Register */
	rc = input_register_device(*input);
	if (rc) {
		input_free_device(*input);
		return rc;
	}

	return rc;
}

static int akm_i2c_check_device(
	struct i2c_client *client)
{
	int rc = 0;
	struct akm09970_soc_ctrl *pctrl = i2c_get_clientdata(client);

	rc = i2c_smbus_read_i2c_block_data(client, AK09970_REG_WIA, AKM_SENSOR_INFO_SIZE, pctrl->sense_info);
	if (rc < 0) {
		dev_err(&client->dev, "%s(), I2C access failed: %d", __func__, rc);
		return rc;
	}

	/* Check read data */
	if ((pctrl->sense_info[0] != AK09970_WIA1_VALUE) ||
			(pctrl->sense_info[1] != AK09970_WIA2_VALUE)) {
		pr_err("The device is not AKM Compass.");
		return -ENXIO;
	}

	return rc;
}

static int akm09970_regulator_init(struct akm09970_soc_ctrl *pctrl, bool on)
{
	int rc;

	if (on) {
		pctrl->vdd = regulator_get(&pctrl->client->dev, "vdd");
		if (IS_ERR(pctrl->vdd)) {
			rc = PTR_ERR(pctrl->vdd);
			pr_err("Regulator get failed vdd rc=%d", rc);
			return rc;
		}

		if (regulator_count_voltages(pctrl->vdd) > 0) {
			rc = regulator_set_voltage(pctrl->vdd,
				AKM09970_VDD_MIN_UV, AKM09970_VDD_MAX_UV);
			if (rc) {
				pr_err("Regulator set failed vdd rc=%d",
					rc);
				regulator_put(pctrl->vdd);
				return rc;
			}
		}
	} else {
		if (regulator_count_voltages(pctrl->vdd) > 0)
			regulator_set_voltage(pctrl->vdd, 0,
				AKM09970_VDD_MAX_UV);

		regulator_put(pctrl->vdd);
	}

	return 0;
}

static int akm09970_gpio_config(struct akm09970_soc_ctrl *pctrl)
{
	int32_t rc = 0;

	rc = gpio_request_one(pctrl->gpio_rstn, GPIOF_OUT_INIT_LOW, "akm09970-rstn");
	if (rc < 0) {
		pr_err("Failed to request power enable GPIO %d", pctrl->gpio_rstn);
		goto fail0;
	}
	gpio_direction_output(pctrl->gpio_rstn, 0);

	rc = gpio_request_one(pctrl->gpio_irq, GPIOF_IN, "akm09970-irq");
	if (rc < 0) {
		pr_err("Failed to request power enable GPIO %d", pctrl->gpio_irq);
		goto fail1;
	}
	gpio_direction_input(pctrl->gpio_irq);

	return 0;

fail1:
	if (gpio_is_valid(pctrl->gpio_rstn))
		gpio_free(pctrl->gpio_rstn);
fail0:
	return rc;
}

static int akm09970_pinctrl_init(struct akm09970_soc_ctrl *pctrl)
{
	int rc = 0;
	/* Get pinctrl if target uses pinctrl */
	pctrl->pinctrl = devm_pinctrl_get(&pctrl->client->dev);

	if (IS_ERR_OR_NULL(pctrl->pinctrl)) {
		rc = PTR_ERR(pctrl->pinctrl);
		pr_err("Target does not use pinctrl %d", rc);
		goto err_pinctrl_get;
	}

	pctrl->pinctrl_default
		= pinctrl_lookup_state(pctrl->pinctrl, "default");
	if (IS_ERR_OR_NULL(pctrl->pinctrl_default)) {
		rc = PTR_ERR(pctrl->pinctrl_default);
		pr_err("Can not lookup default pinstate %d", rc);
		goto err_pinctrl_lookup;
	}
/*
	pctrl->pinctrl_sleep
		= pinctrl_lookup_state(pctrl->pinctrl, "sleep");
	if (IS_ERR_OR_NULL(pctrl->pinctrl_sleep)) {
		rc = PTR_ERR(pctrl->pinctrl_sleep);
		pr_err("Can not lookup default pinstate %d", rc);
		goto err_pinctrl_lookup;
	}*/

	return 0;

err_pinctrl_lookup:
	devm_pinctrl_put(pctrl->pinctrl);
err_pinctrl_get:
	pctrl->pinctrl = NULL;

	return rc;
}

int akm09970_parse_dt(struct device *dev,
				struct akm09970_soc_ctrl *pctrl)
{
	int rc = 0;

	pctrl->gpio_rstn = of_get_named_gpio_flags(dev->of_node, "akm,gpio_rstn", 0, NULL);
	if (!gpio_is_valid(pctrl->gpio_rstn)) {
		pr_err("gpio reset pin %d is invalid.",
			pctrl->gpio_rstn);
		return -EINVAL;
	}

	pctrl->gpio_irq = of_get_named_gpio_flags(dev->of_node, "akm,gpio_irq", 0, NULL);
	if (!gpio_is_valid(pctrl->gpio_irq)) {
		pr_err("gpio irq pin %d is invalid.",
			pctrl->gpio_irq);
		return -EINVAL;
	}

	pctrl->client->irq = gpio_to_irq(pctrl->gpio_irq);

	rc = of_property_read_u32(dev->of_node, "akm,measure-freq-hz", &pctrl->measure_freq_hz);
	if (rc < 0) {
		pr_info("akm,measure-freq-hz not set, use default.");
		pctrl->measure_freq_hz = AKM_DEFAULT_MEASURE_HZ;
	}

	return 0;
}

static ssize_t sensor_id_read(struct file *filp, char __user *ubuf,
		size_t cnt, loff_t *ppos)
{
	char data[8] = {0};
	struct akm09970_soc_ctrl *pctrl = filp->private_data;

	snprintf(data, sizeof(data), "0x%02x%02x\n", pctrl->sense_info[1], pctrl->sense_info[0]);

	return simple_read_from_buffer(ubuf, cnt, ppos, data, strlen(data));
}

static const struct file_operations sensor_id_fops = {
	.open	= simple_open,
	.read	= sensor_id_read,
};

static int akm_reg_dbg_show(struct seq_file *s, void *p)
{
	struct akm09970_soc_ctrl *pctrl = s->private;
	int rc = 0;
	uint8_t buffer[8] = {0};

	rc = i2c_smbus_read_i2c_block_data(pctrl->client, AK09970_REG_ST_XYZ, 8, buffer);
	if (rc < 0) {
		pr_err("read data failed!");
		return 0;
	}
	seq_printf(s, "reg[0x17]: %02x %02x %02x %02x %02x %02x %02x %02x\n",
		buffer[0], buffer[1], buffer[2], buffer[3],
		buffer[4], buffer[5], buffer[6], buffer[7]);

	rc = i2c_smbus_read_i2c_block_data(pctrl->client, AK09970_REG_CNTL1, 2, buffer);
	if (rc < 0) {
		pr_err("read data failed!");
		return 0;
	}
	seq_printf(s, "reg[0x20]: %02x %02x\n", buffer[0], buffer[1]);

	rc = i2c_smbus_read_i2c_block_data(pctrl->client, AK09970_REG_CNTL2, 1, buffer);
	if (rc < 0) {
		pr_err("read data failed!");
		return 0;
	}

	seq_printf(s, "reg[0x21]: %02x\n", buffer[0]);

	return 0;
}

static int akm_reg_dbg_open(struct inode *inode, struct file *file)
{
	return single_open(file, akm_reg_dbg_show, inode->i_private);
}

static const struct file_operations dump_reg_fops = {
	.owner		= THIS_MODULE,
	.open		= akm_reg_dbg_open,
	.llseek		= seq_lseek,
	.read		= seq_read,
	.release	= single_release,
};

int akm09970_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int rc = 0;
	struct akm09970_soc_ctrl *pctrl = NULL;

	dev_info(&client->dev, "%s start probe.", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("check_functionality failed.");
		return -ENODEV;
	}

	/* Allocate memory for driver data */
	pctrl = kzalloc(sizeof(struct akm09970_soc_ctrl), GFP_KERNEL);
	if (!pctrl) {
		pr_err("memory allocation failed.");
		rc = -ENOMEM;
		goto exit1;
	}

	/* set client data */
	pctrl->client = client;
	i2c_set_clientdata(client, pctrl);

	/* parse dt */
	pr_debug("start parse dt.");
	if (client->dev.of_node) {
		rc = akm09970_parse_dt(&client->dev, pctrl);
		if (rc < 0) {
			pr_err("Unable to parse platfrom data rc=%d\n", rc);
			goto exit2;
		}
	}

	/* set pintrl */
	rc = akm09970_pinctrl_init(pctrl);
	if (!rc && pctrl->pinctrl) {
		rc = pinctrl_select_state(pctrl->pinctrl, pctrl->pinctrl_default);
		if (rc < 0) {
			pr_err("Failed to select default pinstate %d\n", rc);
		}
	} else {
		pr_err("Failed to init pinctrl\n");
	}

	/* gpio config */
	rc = akm09970_gpio_config(pctrl);
	if (rc < 0) {
		pr_err("Failed to config gpio\n");
		goto exit2;
	}

	/* check connection */
	rc = akm09970_regulator_init(pctrl, 1);
	if (rc < 0)
		goto exit2;
	rc = akm_power_up(pctrl);
	if (rc < 0)
		goto exit3;

	rc = akm_i2c_check_device(client);
	if (rc < 0) {
		dev_err(&client->dev, "%s check device failed: %d", __func__, rc);
		goto exit4;
	}

	rc = akm09970_input_register(&pctrl->input);
	if (rc) {
		pr_err("input_dev register failed %d", __func__, rc);
		goto exit4;
	}
	input_set_drvdata(pctrl->input, pctrl);

	pctrl->irq = client->irq;
	pr_debug("IRQ is #%d.", pctrl->irq);

	/* init report work queue */
	pctrl->work_queue = alloc_workqueue("akm_poll_work",
		WQ_UNBOUND | WQ_MEM_RECLAIM | WQ_HIGHPRI, 1);
	INIT_WORK(&pctrl->report_work, akm_dev_input_work);

	if (pctrl->irq) {
//		rc = request_irq(client->irq, akm_irq, IRQF_TRIGGER_FALLING, client->name, pctrl);
		rc = request_threaded_irq(
				client->irq,
				NULL,
				akm_irq,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				client->name,
				pctrl);
		if (rc < 0) {
			pr_err("request irq failed.");
			goto exit5;
		}
	}
	disable_irq_nosync(pctrl->client->irq);

	pctrl->miscdev.minor = MISC_DYNAMIC_MINOR;
	pctrl->miscdev.name = AKM_MISCDEV_NAME;
	pctrl->miscdev.fops = &akm09970_fops;
	pctrl->miscdev.parent = &pctrl->client->dev;
	rc = misc_register(&pctrl->miscdev);
	if (rc) {
		pr_err("misc register failed!");
		goto exit6;
	}

	akm_power_down(pctrl);

	mutex_init(&pctrl->soc_mutex);
	init_waitqueue_head(&pctrl->drdy_wq);
	atomic_set(&pctrl->drdy, 0);

	pctrl->debugfs = debugfs_create_dir("akm", NULL);
	if (pctrl->debugfs) {
		debugfs_create_u32("measure_freq_hz", 0664, pctrl->debugfs,
				&pctrl->measure_freq_hz);

		debugfs_create_file("sensor_id", 0444, pctrl->debugfs, pctrl,
				&sensor_id_fops);
		debugfs_create_file("dump", 0444, pctrl->debugfs, pctrl,
				&dump_reg_fops);
	}

	dev_info(&client->dev, "%s successfully probed.", __func__);
	return 0;

exit6:
	if (pctrl->irq)
		free_irq(pctrl->irq, pctrl);
	if (gpio_is_valid(pctrl->irq))
		gpio_free(pctrl->irq);
exit5:
	input_unregister_device(pctrl->input);
exit4:
	akm_power_down(pctrl);
exit3:
	akm09970_regulator_init(pctrl, 0);
exit2:
	kfree(pctrl);
exit1:
	return rc;
}

static int akm09970_remove(struct i2c_client *client)
{
	struct akm09970_soc_ctrl *pctrl = i2c_get_clientdata(client);

	cancel_work_sync(&pctrl->report_work);
	destroy_workqueue(pctrl->work_queue);

	mutex_destroy(&pctrl->soc_mutex);

	if (akm_power_down(pctrl))
		pr_err("power set failed.");
	if (akm09970_regulator_init(pctrl, 0))
		pr_err("power deinit failed.");

	misc_deregister(&pctrl->miscdev);

	if (pctrl->irq)
		free_irq(pctrl->irq, pctrl);

	input_unregister_device(pctrl->input);

	kfree(pctrl);

	pr_debug("successfully removed.");
	return 0;
}

static const struct i2c_device_id akm09970_id[] = {
	{AKM_I2C_NAME, 0 },
	{ }
};

static struct of_device_id akm09970_match_table[] = {
	{ .compatible = "ak,ak09970", },
	{ .compatible = "akm,akm09970", },
	{ },
};

static struct i2c_driver akm09970_driver = {
	.probe		= akm09970_probe,
	.remove		= akm09970_remove,
	.id_table	= akm09970_id,
	.driver = {
		.name	= AKM_I2C_NAME,
		.owner  = THIS_MODULE,
		.of_match_table = akm09970_match_table,
	},
};

static int __init akm09970_init(void)
{
	return i2c_add_driver(&akm09970_driver);
}

static void __exit akm09970_exit(void)
{
	i2c_del_driver(&akm09970_driver);
}

module_init(akm09970_init);
module_exit(akm09970_exit);

MODULE_AUTHOR("ran fei <ranfei@xiaomi.com>");
MODULE_DESCRIPTION("AKM compass driver");
MODULE_LICENSE("GPL");
