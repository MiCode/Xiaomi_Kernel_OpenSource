/*
* Simple driver for Ricktek 6A laser diode driver chip
* Copyright (C) 2018 Xiaomi Corp.
* Copyright (C) 2019 XiaoMi, Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
*/

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
#include <uapi/linux/rtmv20.h>


#define FACTORY_TEST

#define DRV_NAME "rtmv20"
#define RTMV_CLASS_NAME "rtmv"

#define RTMV20_BUSY_BIT_POS	1

/* REG_DEVINFO */
#define CHIP_REV_MASK		(0x0F)
#define VENDOR_ID_MASK		(0xF0)
#define VENDOR_ID_SHIFT		4
#define VENDOR_ID_RICHTEK	8

/* REG_EN_CTRL */
#define LD_ENABLE_SHIFT		(0)
#define SEQMODE_EN_SHIFT	(1)
#define FSIN_OUT_SHIFT		(2)
#define LD_OUT_SHIFT		(3)
#define INP_CTRL_SHIFT		(4)
#define ES_EN_SHIFT		(6)
#define FSIN_ENABLE_SHIFT	(7)

#define FACTORY_TEST_NONE	0
#define FACTORY_TEST_FLOOD	1
#define FACTORY_TEST_PROJECTOR	2

static DECLARE_WAIT_QUEUE_HEAD(poll_wait_queue);

struct rtmv20_chip_data {
	dev_t dev_num;
	struct device *dev;
	struct cdev cdev;
	struct class *chr_class;
	struct device *chr_dev;

	struct rtmv20_platform_data *pdata;
	struct regmap *regmap;
	struct mutex lock;

	int ic_irq;
	int ito_irq;
	int strobe_irq;
	unsigned int chip_rev;
	unsigned long flags;
	atomic_t running;
	atomic_t strobe_count;
	atomic_t strobe_event;
	atomic_t ito_event;
	atomic_t ic_event;

#ifdef FACTORY_TEST
	unsigned int test_mode;
#endif

	/* pinctrl */
	struct pinctrl *pinctrl;
	struct pinctrl_state *gpio_state_active;
	struct pinctrl_state *gpio_state_suspend;

	unsigned int ic_error; //ic irq/status reg is reset after irq ,so need add this value
};

static int rtmv20_read_chip_rev(struct rtmv20_chip_data *chip)
{
	unsigned int dev_info, chip_rev, vendor_id;
	int ret;

	if (!atomic_read(&chip->running)) {
		dev_err(chip->dev, "RTMV2.0 has not been powered on\n");
		return -ENOTSUPP;
	}

	/* read device id */
	ret = regmap_read(chip->regmap, REG_DEVINFO, &dev_info);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to read REG_DEVINFO register\n");
		return ret;
	}

	vendor_id = (dev_info & VENDOR_ID_MASK) >> VENDOR_ID_SHIFT;
	if (vendor_id != VENDOR_ID_RICHTEK) {
		dev_err(chip->dev, "Invalid vendor id 0x%02X\n", vendor_id);
		return -ENODEV;
	}

	chip_rev = (dev_info & CHIP_REV_MASK);
	dev_info(chip->dev, "RTMV2.0 chip revision = %d\n", chip_rev);

	chip->chip_rev = chip_rev;
	return ret;
}

#ifdef FACTORY_TEST
static int rtmv20_switch_test_mode(struct rtmv20_chip_data *chip,
		unsigned int mode)
{
	unsigned int val = 0;
	int ret = 0;

	if (mode == FACTORY_TEST_FLOOD)
		val = (1 << FSIN_ENABLE_SHIFT) | (2 << INP_CTRL_SHIFT) |
			(1 << FSIN_OUT_SHIFT);
	else if (mode == FACTORY_TEST_PROJECTOR)
		val = (1 << LD_OUT_SHIFT) | (1 << LD_ENABLE_SHIFT);

	ret = regmap_write(chip->regmap, REG_EN_CTRL, val);
	if (ret) {
		dev_err(chip->dev, "Failed to write REG_EN_CTRL register\n");
		return ret;
	}

	chip->test_mode = mode;
	return 0;
}
#endif

static int rtmv20_power_up(struct rtmv20_chip_data *chip)
{
	struct rtmv20_platform_data *pdata = chip->pdata;

	if (gpio_is_valid(pdata->enable_gpio)) {
		if (gpio_is_valid(pdata->ito_detect_gpio) &&
				!gpio_get_value(pdata->ito_detect_gpio)) {
			dev_err(chip->dev, "ITO BROKEN? POWER UP FAILED!\n");
			return -EPERM;
		} else {
			gpio_set_value(pdata->enable_gpio, 1);
			dev_dbg(chip->dev, "Enable success\n");
		}
	}

	atomic_cmpxchg(&chip->ic_event, 1, 0);

	atomic_set(&chip->running, 1);
	atomic_set(&chip->strobe_count, 0);
	atomic_set(&chip->strobe_event, 0);

	return 0;
}

static void rtmv20_power_down(struct rtmv20_chip_data *chip)
{
	if (gpio_is_valid(chip->pdata->enable_gpio))
		gpio_set_value(chip->pdata->enable_gpio, 0);

	atomic_set(&chip->running, 0);
	atomic_set(&chip->strobe_count, 0);
	atomic_set(&chip->strobe_event, 0);
}

static irqreturn_t rtmv20_ito_irq_handler(int ito_irq, void *dev_id)
{
	struct rtmv20_chip_data *chip = dev_id;

	dev_err(chip->dev, "ITO BROKEN?? SHUTDOWN RTMV20!\n");

	rtmv20_power_down(chip);

	atomic_set(&chip->ito_event, 1);

	wake_up(&poll_wait_queue);

	return IRQ_HANDLED;
}

static void rtmv20_reset_ic(void *dev)
{
	struct rtmv20_chip_data *chip = dev;

	dev_err(chip->dev, "rtmv20_reset_ic called!\n");
	regmap_write(chip->regmap, REG_EN_CTRL, 0);
	if (gpio_is_valid(chip->pdata->enable_gpio)) {
		gpio_set_value(chip->pdata->enable_gpio, 0);
		msleep(1);  // sleep 1ms
		gpio_set_value(chip->pdata->enable_gpio, 1);
	} else {
		dev_err(chip->dev, "rtmv20_reset_ic fail!\n");
	}
}

static irqreturn_t rtmv20_ic_error_irq_handler(int ic_irq, void *dev_id)
{
	struct rtmv20_chip_data *chip = dev_id;
	unsigned int irq_val = 0;
	static unsigned int irq_cnt;

	regmap_read(chip->regmap, REG_LD_IRQ, &irq_val);
	if  (irq_val != 0) {
		dev_err(chip->dev, "ic error! rtmv_irq: RTMV20 EXCEPTION!irq=0x%x, cnt=%d\n", irq_val, irq_cnt++);
		if (irq_val & RTMV_IRQ_OTP_EVT)
			dev_err(chip->dev, "ic error:RTMV_IRQ_OTP_EVT\n");
		if (irq_val & RTMV_IRQ_SHORT_EVT)
			dev_err(chip->dev, "ic error:RTMV_IRQ_SHORT_EVT\n");
		if (irq_val & RTMV_IRQ_OPEN_EVT)
			dev_err(chip->dev, "ic error:RTMV_IRQ_OPEN_EVT\n");
		if (irq_val & RTMV_IRQ_LBP_EVT)
			dev_err(chip->dev, "ic error:RTMV_IRQ_LBP_EVT\n");
		if (irq_val & RTMV_IRQ_OCP_EVT)
			dev_err(chip->dev, "ic error:RTMV_IRQ_OCP_EVT\n");

		// bit 0~4 need reset rtmv ic, bit 5~7 will not trigger IRQ
		rtmv20_reset_ic(dev_id);

		chip->ic_error = irq_val;
		atomic_set(&chip->ic_event, 1);
		wake_up(&poll_wait_queue);
	}

	return IRQ_HANDLED;
}

static irqreturn_t rtmv20_strobe_irq_handler(int strobe_irq, void *dev_id)
{
	struct rtmv20_chip_data *chip = dev_id;

	atomic_inc(&chip->strobe_count);

	atomic_set(&chip->strobe_event, 1);

	wake_up(&poll_wait_queue);

	return IRQ_HANDLED;
}

static int rtmv20_release(struct inode *inp, struct file *filp)
{
	struct rtmv20_chip_data *chip =
		container_of(inp->i_cdev, struct rtmv20_chip_data, cdev);

	disable_irq_nosync(chip->ic_irq);
	disable_irq_nosync(chip->strobe_irq);

	rtmv20_power_down(chip);

	clear_bit(RTMV20_BUSY_BIT_POS, &chip->flags);

	return 0;
}

static int rtmv20_open(struct inode *inp, struct file *filp)
{
	struct rtmv20_chip_data *chip =
		container_of(inp->i_cdev, struct rtmv20_chip_data, cdev);

	//if (test_and_set_bit(RTMV20_BUSY_BIT_POS, &chip->flags))
	//	return -EBUSY;

	filp->private_data = chip;

	enable_irq(chip->ic_irq);
	enable_irq(chip->strobe_irq);

	return 0;
}

static ssize_t rtmv20_write(struct file *filp, const char *buf, size_t len, loff_t *fseek)
{
	int rc = 0;

	return rc;
}

static ssize_t rtmv20_read(struct file *filp, char *buf, size_t len, loff_t *fseek)
{
	int rc = 0;
	struct rtmv20_chip_data *chip = filp->private_data;
	rtmv20_report_data report_data;

	report_data.state = atomic_read(&chip->running);
	report_data.ito_event = atomic_read(&chip->ito_event);
	report_data.ic_event = atomic_read(&chip->ic_event);
	report_data.strobe_event = (unsigned int)atomic_read(&chip->strobe_count);

	if (report_data.ic_event) {
		report_data.ic_exception = chip->ic_error;
		// dev_err(chip->dev, "rtmv20_read, !report_data.ic_exception=%d\n",report_data.ic_exception);
		atomic_set(&chip->ic_event, 0);  //reset ic event after read
		chip->ic_error = 0;
	}

	if (copy_to_user(buf, &report_data, sizeof(rtmv20_report_data)) != 0)
		dev_err(chip->dev, "Copy to user space failed!\n");

	return rc;
}

static unsigned int rtmv20_poll(struct file *filp, poll_table *wait)
{
	unsigned int mask = 0;
	struct rtmv20_chip_data *chip = filp->private_data;

	dev_dbg(chip->dev, "poll enter\n");

	poll_wait(filp, &poll_wait_queue, wait);
	if (atomic_read(&chip->strobe_event) || atomic_read(&chip->ito_event) || atomic_read(&chip->ic_event)) {
		atomic_set(&chip->strobe_event, 0);
		mask = POLLIN | POLLRDNORM;
	}

	return mask;
}

static int rtmv20_data_write(struct rtmv20_chip_data *chip, void *data)
{
	int rc = 0;
	int i = 0;
	rtmv20_data *reg_data = (rtmv20_data *)data;

	if ((NULL == data) || (NULL == reg_data)) {
		dev_err(chip->dev, "Invalied data pointer\n");
		return -EFAULT;
	}

	for (i = 0; i < reg_data->size; i++) {
		dev_dbg(chip->dev, "num = %d, addr = 0x%x, data = 0x%x\n", i, reg_data->reg_data[i].reg_addr, reg_data->reg_data[i].reg_data);
		rc = regmap_write(chip->regmap, reg_data->reg_data[i].reg_addr, reg_data->reg_data[i].reg_data);
		if (rc) {
			dev_err(chip->dev, "Write register failed, addr = 0x%x, data = 0x%x\n", reg_data->reg_data[i].reg_addr, reg_data->reg_data[i].reg_data);
		}
	}
	dev_dbg(chip->dev, "Write register success\n");

	chip->test_mode = 0;

	return rc;
}

static int rtmv20_data_read(void)
{
	int rc = 0;


	return rc;
}

static long rtmv20_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int rc = 0;
	struct rtmv20_chip_data *chip = filp->private_data;
	rtmv20_data reg_data;
	rtmv20_info laser_info;

	dev_dbg(chip->dev, "[%s] ioctl_cmd = %u\n", __func__, cmd);

	if (_IOC_TYPE(cmd) != RTMV_IOC_MAGIC)
		return -ENODEV;

	if (_IOC_DIR(cmd) & _IOC_READ)
		rc = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		rc = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	if (rc)
		return -EFAULT;

	if (_IOC_DIR(cmd) & _IOC_WRITE) {
		if (copy_from_user(&reg_data, (void __user *)arg, sizeof(reg_data))) {
			dev_err(chip->dev, "Copy data from user space failed\n");
			return -EFAULT;
		}
	}

	switch (cmd) {
	case RTMV_IOC_PWR_UP:
		rc = rtmv20_power_up(chip);
		if (rc < 0)
			dev_err(chip->dev, "Power up IC failed\n");
		break;
	case RTMV_IOC_PWR_DOWN:
		rtmv20_power_down(chip);
		break;
	case RTMV_IOC_WRITE_DATA:
		rc = rtmv20_data_write(chip, &reg_data);
		break;
	case RTMV_IOC_READ_DATA:
		rc = rtmv20_data_read();
		break;
	case RTMV_IOC_READ_INFO:
	{
		unsigned int val, val0, val_es_en, val_es;
		unsigned int ret = 0;

		ret += regmap_read(chip->regmap, REG_EN_CTRL, &val);
		laser_info.laser_enable = ((val & 0x9) == 0x9) ? 1 : 0;

		ret += regmap_read(chip->regmap, REG_LD_CTRL1, &val);
		laser_info.laser_current = val * 30;  // mA

		ret += regmap_read(chip->regmap, REG_PULSE_WIDTH1, &val);
		ret += regmap_read(chip->regmap, REG_PULSE_WIDTH2, &val0);
		laser_info.laser_pulse = ((val & 0x3f) << 8) + (val0 & 0xff); //ms*1000

		// ES check
		ret += regmap_read(chip->regmap, REG_EN_CTRL, &val_es_en);
		if (val_es_en & (1 << 6)) {
			ret += regmap_read(chip->regmap, REG_ES_LD_CTRL1, &val);
			val_es = val * 30;
			if (laser_info.laser_current > val_es)
				laser_info.laser_current = val_es;

			ret += regmap_read(chip->regmap, REG_ES_PULSE_WIDTH1, &val);
			ret += regmap_read(chip->regmap, REG_ES_PULSE_WIDTH2, &val0);
			val_es = ((val & 0x3f) << 8) + (val0 & 0xff);
			if (laser_info.laser_pulse > val_es)
				laser_info.laser_pulse = val_es;
		}

		ret += regmap_read(chip->regmap, REG_LD_STAT, &val);
		laser_info.laser_error = val & 0x1f;

		ret += regmap_read(chip->regmap, REG_FSIN2_CTRL3, &val);
		laser_info.flood_pulse = val * 40;

		ret += regmap_read(chip->regmap, REG_EN_CTRL, &val);
		laser_info.flood_enable = ((val & 0x84) == 0x84) ? 1 : 0;

		if (copy_to_user((void __user *)arg, &laser_info, sizeof(laser_info)) != 0 || ret != 0)
			dev_err(chip->dev, "copy to user failed!\n");
		break;
	}
	default:
		break;
	}

	return rc;
}

#ifdef CONFIG_COMPAT
static long rtmv20_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	return rtmv20_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
}
#endif

/*
 * file_operations entity
*/
static const struct file_operations rtmv20_fops =
{
	.owner = THIS_MODULE,
	.release = rtmv20_release,
	.open = rtmv20_open,
	.read = rtmv20_read,
	.write = rtmv20_write,
	.unlocked_ioctl = rtmv20_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = rtmv20_compat_ioctl,
#endif
	.poll = rtmv20_poll,
};

static ssize_t rtmv20_chip_rev_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct rtmv20_chip_data *chip = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", chip->chip_rev);
}

static ssize_t rtmv20_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct rtmv20_chip_data *chip = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&chip->running));
}

static ssize_t rtmv20_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int rc = 0;
	unsigned int val;
	struct rtmv20_chip_data *chip = dev_get_drvdata(dev);

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;

	if (val == 0)
		rtmv20_power_down(chip);
	else
		rtmv20_power_up(chip);

	return count;
}

#ifdef FACTORY_TEST
static ssize_t rtmv20_test_mode_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct rtmv20_chip_data *chip = dev_get_drvdata(dev);

	if (chip->test_mode == FACTORY_TEST_NONE)
		return snprintf(buf, PAGE_SIZE, "NONE\n");
	else if (chip->test_mode == FACTORY_TEST_FLOOD)
		return snprintf(buf, PAGE_SIZE, "FLOOD\n");
	else if (chip->test_mode == FACTORY_TEST_PROJECTOR)
		return snprintf(buf, PAGE_SIZE, "PROJECTOR\n");
	else
		return snprintf(buf, PAGE_SIZE, "UNKNOWN\n");
}

static ssize_t rtmv20_test_mode_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int rc = 0;
	unsigned int val;
	struct rtmv20_chip_data *chip = dev_get_drvdata(dev);

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;

	if (val > FACTORY_TEST_PROJECTOR) {
		dev_err(chip->dev, "Invalid test mode\n");
		val = FACTORY_TEST_NONE;
	}

	rc = rtmv20_switch_test_mode(chip, val);
	if (rc)
		dev_err(chip->dev, "Switch test mode failed\n");

	return count;
}

static ssize_t rtmv20_ld_current_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	unsigned int val, current_ma;
	int ret = 0;
	struct rtmv20_chip_data *chip = dev_get_drvdata(dev);

	ret = regmap_read(chip->regmap, REG_LD_CTRL1, &val);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to read REG_LD_CTRL1: %d\n", ret);
		return ret;
	}

	if (val > 200)
		val = 200;

	current_ma = 30 * val;	/* 30mA per step */
	return snprintf(buf, PAGE_SIZE, "%umA\n", current_ma);
}

static ssize_t rtmv20_ld_current_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	unsigned int current_ma, val;
	struct rtmv20_chip_data *chip = dev_get_drvdata(dev);

	ret = kstrtouint(buf, 0, &current_ma);
	if (ret)
		return ret;

	if (current_ma > 6000)
		current_ma = 6000;	/* Max supported current is 6A */

	val = current_ma / 30;	/* 30mA per step */
	ret = regmap_write(chip->regmap, REG_LD_CTRL1, val);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to write REG_LD_CTRL1: %d\n", ret);
		return ret;
	}

	return count;
}

static ssize_t rtmv20_ld_width_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	unsigned int val1, val2, width;
	int ret = 0;
	struct rtmv20_chip_data *chip = dev_get_drvdata(dev);

	ret = regmap_read(chip->regmap, REG_PULSE_WIDTH1, &val1);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to read REG_PULSE_WIDTH1: %d\n", ret);
		return ret;
	}

	ret = regmap_read(chip->regmap, REG_PULSE_WIDTH2, &val2);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to read REG_PULSE_WIDTH2: %d\n", ret);
		return ret;
	}

	width = (val1 << 8) | val2;
	if (width > 10000)
		width = 10000;
	return snprintf(buf, PAGE_SIZE, "%uus\n", width);
}

static ssize_t rtmv20_ld_width_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int val1, val2, width;
	int ret = 0;
	struct rtmv20_chip_data *chip = dev_get_drvdata(dev);

	ret = kstrtouint(buf, 0, &width);
	if (ret)
		return ret;

	if (width > 10000)
		width = 10000;	/* Max supported width is 10ms */

	val1 = (width >> 8) & 0xFF;
	val2 = width & 0xFF;

	ret = regmap_write(chip->regmap, REG_PULSE_WIDTH1, val1);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to write REG_PULSE_WIDTH1: %d\n", ret);
		return ret;
	}

	ret = regmap_write(chip->regmap, REG_PULSE_WIDTH2, val2);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to write REG_PULSE_WIDTH2: %d\n", ret);
		return ret;
	}

	return count;
}

static ssize_t rtmv20_flood_width_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	unsigned int val, width;
	int ret = 0;
	struct rtmv20_chip_data *chip = dev_get_drvdata(dev);

	ret = regmap_read(chip->regmap, REG_FSIN2_CTRL3, &val);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to read REG_FSIN2_CTRL3: %d\n", ret);
		return ret;
	}

	width = val * 40;	/* 40us per step */
	if (width > 10000)
		width = 10000;

	return snprintf(buf, PAGE_SIZE, "%uus\n", width);
}

static ssize_t rtmv20_flood_width_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int val, width;
	int ret = 0;
	struct rtmv20_chip_data *chip = dev_get_drvdata(dev);

	ret = kstrtouint(buf, 0, &width);
	if (ret)
		return ret;

	if (width > 10000)
		width = 10000;	/* Max supported width is 10ms */

	val = width / 40;	/* 40us per step */

	ret = regmap_write(chip->regmap, REG_FSIN2_CTRL3, val);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to write REG_FSIN2_CTRL3: %d\n", ret);
		return ret;
	}

	return count;
}

static ssize_t rtmv20_reg_opt_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	#define RTMV_REG_NUM 45

	unsigned int reg_rtmv[RTMV_REG_NUM] = {0};
	unsigned int ret = 0;
	struct rtmv20_chip_data *chip = dev_get_drvdata(dev);
	unsigned int i = 0;
	unsigned int j = 0;

	for (i = 0; i < RTMV_REG_NUM - 2; i++) {
		ret += regmap_read(chip->regmap, i, &reg_rtmv[i]);
	}
	ret += regmap_read(chip->regmap, REG_LD_STAT, &reg_rtmv[RTMV_REG_NUM - 2]);
	ret += regmap_read(chip->regmap, REG_LD_MASK, &reg_rtmv[RTMV_REG_NUM - 1]);

	// print reg to kernel log
	for (i = 0; i < RTMV_REG_NUM; i++) {
		if (i <= 0x29)
			j = i;
		else
			 j = 0x30 + (i - 0x2a) * 0x10;

		dev_err(chip->dev, "0x%x:0x%x,", j, reg_rtmv[i]);
	}

	if (ret != 0)
		dev_err(chip->dev, "rtmv reg dump fail!\n");
	else
		dev_err(chip->dev, "rtmv reg dump success!\n");

	return snprintf(buf, PAGE_SIZE, "reg dump\n");
}

static ssize_t rtmv20_reg_opt_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int val, addr;
	int ret = 0;
	struct rtmv20_chip_data *chip = dev_get_drvdata(dev);

	ret = sscanf(buf, "0x%x 0x%x", &addr, &val);
	if (ret == 0)
		dev_err(chip->dev, "rtmv20_reg_opt_store, reg=0x%x,val=0x%x.\n", addr, val);

	if (addr > 0x29 && addr != 0x30 && addr != 0x40 && addr != 0x50) {
		dev_err(chip->dev, "rtmv20_reg_opt_store, addr invalid:0x%x\n", addr);
		return count;
	}

	ret = regmap_write(chip->regmap, addr, val);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to write reg:0x%x, val:0x%x, ret:%d\n", addr, val, ret);
		return ret;
	}
	regmap_read(chip->regmap, addr, &val);
	dev_err(chip->dev, "rtmv20_reg_opt_store, reg:0x%x, val_after_set:0x%x.\n", addr, val);

	return count;
}


#endif

static DEVICE_ATTR(chip_rev, S_IRUGO, rtmv20_chip_rev_show, NULL);
static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR, rtmv20_enable_show, rtmv20_enable_store);
#ifdef FACTORY_TEST
static DEVICE_ATTR(test_mode, S_IRUGO | S_IWUSR, rtmv20_test_mode_show, rtmv20_test_mode_store);
static DEVICE_ATTR(ld_current, S_IRUGO | S_IWUSR, rtmv20_ld_current_show, rtmv20_ld_current_store);
static DEVICE_ATTR(ld_width, S_IRUGO | S_IWUSR, rtmv20_ld_width_show, rtmv20_ld_width_store);
static DEVICE_ATTR(flood_width, S_IRUGO | S_IWUSR, rtmv20_flood_width_show, rtmv20_flood_width_store);
static DEVICE_ATTR(reg_opt, S_IRUGO | S_IWUSR, rtmv20_reg_opt_show, rtmv20_reg_opt_store);
#endif

static struct attribute *rtmv_attrs[] = {
	&dev_attr_chip_rev.attr,
	&dev_attr_enable.attr,
#ifdef FACTORY_TEST
	&dev_attr_test_mode.attr,
	&dev_attr_ld_current.attr,
	&dev_attr_ld_width.attr,
	&dev_attr_flood_width.attr,
	&dev_attr_reg_opt.attr,
#endif
	NULL
};

static const struct attribute_group rtmv_attr_group = {
	.attrs = rtmv_attrs,
};

static int rtmv20_init_pinctrl(struct rtmv20_chip_data *chip)
{
	struct device *dev = chip->dev;
	int rc = 0;

	chip->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR_OR_NULL(chip->pinctrl)) {
		dev_err(dev, "Unable to acquire pinctrl\n");
		chip->pinctrl = NULL;
		return 0;
	}

	chip->gpio_state_active =
		pinctrl_lookup_state(chip->pinctrl, "rtmv20_gpio_active");
	if (IS_ERR_OR_NULL(chip->gpio_state_active)) {
		dev_err(dev, "Cannot lookup active pinctrl state\n");
		rc = PTR_ERR(chip->gpio_state_active);
		goto err;
	}

	chip->gpio_state_suspend =
		pinctrl_lookup_state(chip->pinctrl, "rtmv20_gpio_suspend");
	if (IS_ERR_OR_NULL(chip->gpio_state_suspend)) {
		dev_err(dev, "Cannot lookup suspend pinctrl state\n");
		rc = PTR_ERR(chip->gpio_state_suspend);
		goto err;
	}

	return 0;

err:
	devm_pinctrl_put(chip->pinctrl);
	chip->pinctrl = NULL;
	return rc;
}

static int rtmv20_pinctrl_select(struct rtmv20_chip_data *chip, bool state)
{
	int rc = 0;
	struct pinctrl_state *pins_state =
		state ? (chip->gpio_state_active) : (chip->gpio_state_suspend);
	struct device *dev = chip->dev;

	rc = pinctrl_select_state(chip->pinctrl, pins_state);
	if (rc < 0)
		dev_err(dev, "Failed to select pins state %s\n",
			state ? "active" : "suspend");

	return rc;
}

#ifdef CONFIG_OF
static struct rtmv20_platform_data *rtmv20_parse_dt(struct i2c_client *client)
{
	struct rtmv20_platform_data *pdata = NULL;
	struct device_node *np = client->dev.of_node;

	if (!np)
		return ERR_PTR(-ENOENT);

	pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	pdata->enable_gpio = of_get_named_gpio(np, "rtmv20,enable-gpio", 0);
	pdata->ito_detect_gpio = of_get_named_gpio(np, "rtmv20,ito-detect-gpio", 0);
	pdata->irq_gpio = of_get_named_gpio(np, "rtmv20,irq-gpio", 0);
	pdata->strobe_irq_gpio = of_get_named_gpio(np, "rtmv20,strobe-irq-gpio", 0);

	return pdata;
}
#endif

static const struct regmap_config rtmv20_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = REG_MAX,
};

static int rtmv20_i2c_probe(struct i2c_client *client,
					const struct i2c_device_id *id)
{
	int rc = 0;
	struct rtmv20_chip_data *chip;
	struct rtmv20_platform_data *pdata;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "i2c check functionality fail\n");
		return -EOPNOTSUPP;
	}

	chip = devm_kzalloc(&client->dev, sizeof(struct rtmv20_chip_data),
				GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

#ifdef CONFIG_OF
	if (client->dev.of_node) {
		pdata = rtmv20_parse_dt(client);
		if (IS_ERR(pdata)) {
			dev_err(&client->dev, "Failed to parse devicetree\n");
			return -ENODEV;
		}
	} else
		pdata = dev_get_platdata(&client->dev);
#else
	pdata = dev_get_platdata(&client->dev);
#endif

	if (!pdata) {
		dev_err(&client->dev, "Platform data is needed!\n");
		return -ENODATA;
	}

	chip->dev = &client->dev;
	chip->pdata = pdata;

	chip->regmap = devm_regmap_init_i2c(client, &rtmv20_regmap);
	if (IS_ERR(chip->regmap)) {
		rc = PTR_ERR(chip->regmap);
		dev_err(&client->dev, "Failed to allocate regmap: %d\n",
				rc);
		return rc;
	}

	rc = rtmv20_init_pinctrl(chip);
	if (rc) {
		dev_err(&client->dev, "Failed to initialize pinctrl\n");
		return rc;
	} else {
		if (chip->pinctrl) {
			rc = rtmv20_pinctrl_select(chip, true);
			if (rc) {
				dev_err(&client->dev,
					"Failed to select pinctrl\n");
				return rc;
			}
		}
	}

	if (gpio_is_valid(pdata->enable_gpio)) {
		rc = gpio_request(pdata->enable_gpio, "rtmv20_en");
		if (rc < 0) {
			dev_err(&client->dev, "Unable to request gpio[%d]\n",
					pdata->enable_gpio);
			goto err_pinctrl_sleep;
		}

		rc = gpio_direction_output(pdata->enable_gpio, 0);
		if (rc < 0) {
			dev_err(&client->dev,
				"Unable to set enable_gpio to output\n");
			goto err_free_enable_gpio;
		}
	}

	if (gpio_is_valid(pdata->ito_detect_gpio)) {
		rc = gpio_request(pdata->ito_detect_gpio, "rtmv20_ito");
		if (rc < 0) {
			dev_err(&client->dev, "Unable to request gpio[%d]\n",
					pdata->ito_detect_gpio);
			goto err_free_enable_gpio;
		}

		rc = gpio_direction_input(pdata->ito_detect_gpio);
		if (rc < 0) {
			dev_err(&client->dev,
				"Unable to set ito detect gpio to input\n");
			goto err_free_ito_det_gpio;
		}

		chip->ito_irq = gpio_to_irq(pdata->ito_detect_gpio);
		rc = request_threaded_irq(chip->ito_irq, NULL, rtmv20_ito_irq_handler,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				"rtmv20_ito", chip);
		if (rc < 0) {
			dev_err(&client->dev, "Unable to request irq\n");
			goto err_free_ito_det_gpio;
		}
	}
	disable_irq_nosync(chip->ito_irq);

	/*IC Exception IRQ*/
	if (gpio_is_valid(pdata->irq_gpio)) {
		rc = gpio_request(pdata->irq_gpio, "rtmv20_irq");
		if (rc < 0) {
			dev_err(&client->dev, "Unable to request gpio[%d]\n",
					pdata->irq_gpio);
			goto err_free_ito_det_irq;
		}

		rc = gpio_direction_input(pdata->irq_gpio);
		if (rc < 0) {
			dev_err(&client->dev,
				"Unable to set ito detect gpio to input\n");
			goto err_free_irq_gpio;
		}

		chip->ic_irq = gpio_to_irq(pdata->irq_gpio);
		rc = request_threaded_irq(chip->ic_irq, NULL, rtmv20_ic_error_irq_handler,
			IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
			"rtmv20_irq", chip);
		if (rc < 0) {
			dev_err(&client->dev, "Unable to request irq\n");
			goto err_free_irq_gpio;
		}
	}
	disable_irq_nosync(chip->ic_irq);

	/*IR Sensor Strobe IRQ*/
	if (gpio_is_valid(pdata->strobe_irq_gpio)) {
		rc = gpio_request(pdata->strobe_irq_gpio, "rtmv20_strobe_irq");
		if (rc < 0) {
			dev_err(&client->dev, "Unable to request gpio[%d]\n",
					pdata->strobe_irq_gpio);
			goto err_free_irq;
		}

		rc = gpio_direction_input(pdata->strobe_irq_gpio);
		if (rc < 0) {
			dev_err(&client->dev,
				"Unable to set ito detect gpio to input\n");
			goto err_free_strobe_gpio;
		}

		chip->strobe_irq = gpio_to_irq(pdata->strobe_irq_gpio);
		rc = request_threaded_irq(chip->strobe_irq, rtmv20_strobe_irq_handler, NULL,
			IRQF_TRIGGER_RISING | IRQF_ONESHOT,
			"rtmv20_strobe_irq", chip);
		if (rc < 0) {
			dev_err(&client->dev, "Unable to request irq\n");
			goto err_free_strobe_gpio;
		}
	}
	disable_irq_nosync(chip->strobe_irq);

	/* Read chip revision */
	rtmv20_power_up(chip);
	msleep(10);
	rc = rtmv20_read_chip_rev(chip);
	rtmv20_power_down(chip);
	if (rc < 0) {
		dev_err(&client->dev, "Failed to read chip revision\n");
		goto err_free_strobe_irq;
	}

	mutex_init(&chip->lock);
	i2c_set_clientdata(client, chip);

	rc = sysfs_create_group(&client->dev.kobj, &rtmv_attr_group);
	if (rc < 0) {
		dev_err(&client->dev, "Failed to create sysfs group: %d\n",
			rc);
		goto err_mutex_destroy;
	}

	chip->chr_class = class_create(THIS_MODULE, RTMV_CLASS_NAME);
	if (chip->chr_class == NULL) {
		dev_err(&client->dev, "Failed to create class.\n");
		rc = -ENODEV;
		goto err_free_sysfs_group;
	}

	rc = alloc_chrdev_region(&chip->dev_num, 0, 1, DRV_NAME);
	if (rc < 0) {
		dev_err(&client->dev, "Failed to allocate chrdev region\n");
		goto err_destroy_class;
	}

	chip->chr_dev = device_create(chip->chr_class, NULL,
					chip->dev_num, chip, DRV_NAME);
	if (IS_ERR(chip->chr_dev)) {
		dev_err(&client->dev, "Failed to create char device\n");
		rc = PTR_ERR(chip->chr_dev);
		goto err_unregister_chrdev;
	}

	cdev_init(&(chip->cdev), &rtmv20_fops);
	chip->cdev.owner = THIS_MODULE;

	rc = cdev_add(&(chip->cdev), chip->dev_num, 1);
	if (rc < 0) {
		dev_err(&client->dev, "Failed to add cdev\n");
		goto err_destroy_device;
	}

	atomic_set(&chip->ito_event, 0);

	return 0;

err_destroy_device:
	if (chip->chr_dev)
		device_destroy(chip->chr_class, chip->dev_num);

err_unregister_chrdev:
	unregister_chrdev_region(chip->dev_num, 1);
err_destroy_class:
	if (chip->chr_class)
		class_destroy(chip->chr_class);
err_free_sysfs_group:
	sysfs_remove_group(&client->dev.kobj, &rtmv_attr_group);

err_mutex_destroy:
	mutex_destroy(&chip->lock);

err_free_strobe_irq:
	if (gpio_is_valid(pdata->strobe_irq_gpio))
		free_irq(chip->strobe_irq, chip);
err_free_strobe_gpio:
	if (gpio_is_valid(pdata->strobe_irq_gpio))
		gpio_free(pdata->strobe_irq_gpio);

err_free_irq:
	if (gpio_is_valid(pdata->irq_gpio))
		free_irq(chip->ic_irq, chip);
err_free_irq_gpio:
	if (gpio_is_valid(pdata->irq_gpio))
		gpio_free(pdata->irq_gpio);

err_free_ito_det_irq:
	if (gpio_is_valid(pdata->ito_detect_gpio))
		free_irq(chip->ito_irq, chip);
err_free_ito_det_gpio:
	if (gpio_is_valid(pdata->ito_detect_gpio))
		gpio_free(pdata->ito_detect_gpio);

err_free_enable_gpio:
	if (gpio_is_valid(pdata->enable_gpio)) {
		gpio_set_value(pdata->enable_gpio, 0);
		gpio_free(pdata->enable_gpio);
	}

err_pinctrl_sleep:
	if (chip->pinctrl) {
		if (rtmv20_pinctrl_select(chip, false) < 0)
			dev_err(&client->dev,
				"Failed to select suspend pinstate\n");
		devm_pinctrl_put(chip->pinctrl);
	}

	return rc;
}

static int rtmv20_i2c_remove(struct i2c_client *client)
{
	struct rtmv20_chip_data *chip = i2c_get_clientdata(client);
	if (&chip->cdev)
		cdev_del(&(chip->cdev));

	if (chip->chr_dev)
		device_destroy(chip->chr_class, chip->dev_num);

	unregister_chrdev_region(chip->dev_num, 1);
	if (chip->chr_class)
		class_destroy(chip->chr_class);

	sysfs_remove_group(&client->dev.kobj, &rtmv_attr_group);

	mutex_destroy(&chip->lock);

	if (gpio_is_valid(chip->pdata->enable_gpio)) {
		gpio_set_value(chip->pdata->enable_gpio, 0);
		gpio_free(chip->pdata->enable_gpio);
	}

	if (gpio_is_valid(chip->pdata->strobe_irq_gpio)) {
		free_irq(chip->strobe_irq, chip);
		gpio_free(chip->pdata->strobe_irq_gpio);
	}

	if (gpio_is_valid(chip->pdata->irq_gpio)) {
		free_irq(chip->ic_irq, chip);
		gpio_free(chip->pdata->irq_gpio);
	}

	if (gpio_is_valid(chip->pdata->ito_detect_gpio)) {
		free_irq(chip->ito_irq, chip);
		gpio_free(chip->pdata->ito_detect_gpio);
	}

	rtmv20_pinctrl_select(chip, false);

	return 0;
}

static const struct i2c_device_id rtmv20_id[] = {
	{RTMV20_NAME, 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, rtmv20_id);

static struct i2c_driver rtmv20_driver = {
	.driver = {
		.name = RTMV20_NAME,
		.owner = THIS_MODULE,
	},
	.probe  = rtmv20_i2c_probe,
	.remove = rtmv20_i2c_remove,
	.id_table = rtmv20_id,
};

module_i2c_driver(rtmv20_driver);

MODULE_DESCRIPTION("Richtek RTMV2.0 laser diode driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Tao Jun <taojun@xiaomi.com>");
MODULE_VERSION("2.0");
