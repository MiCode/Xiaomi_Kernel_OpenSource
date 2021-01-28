/*
 * Copyright (C) 2016 ST Microelectronics S.A.
 * Copyright (C) 2010 Stollmann E+V GmbH
 * Copyright (C) 2010 Trusted Logic S.A.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */
#include "st21nfc.h"
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/version.h>

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>

/* Test for kernel version.
 * 3.18 => use DMA for I2C, no support for standardized
 * GPIO access in DTS.
 * 4.4 => the opposite.
 */
#include <linux/version.h>

#if (KERNEL_VERSION(4, 4, 0) > LINUX_VERSION_CODE)
// Legacy implementation, also used on recent kernels for legacy platforms
// such as (6580 and 6735)
# define KRNMTKLEGACY_I2C 1
# define KRNMTKLEGACY_CLK 1
# define KRNMTKLEGACY_GPIO 1
#endif
// Kernel 4.9 on some platforms is using legacy drivers (kernel-4.9-lc)
// I2C: CONFIG_MACH_MT6735 / 6735M / 6753 / 6580 / 6755 use legacy driver
// CLOCK: 4.9 has right includes, no need for special handling.
// GPIO : same as I2C -- we use the same condition at the moment.
//#if (defined(CONFIG_MACH_MT6735) || defined(CONFIG_MACH_MT6735M) ||
//    defined(CONFIG_MACH_MT6753) || defined(CONFIG_MACH_MT6580) ||
//	defined(CONFIG_MACH_MT6755))
// test on I2C special define instead of listing the platforms
#ifdef CONFIG_MTK_I2C_EXTENSION
# define KRNMTKLEGACY_I2C 1
# define KRNMTKLEGACY_GPIO 1
#endif

/* Set NO_MTK_CLK_MANAGEMENT if using xtal integration */
#ifndef NO_MTK_CLK_MANAGEMENT
# ifdef KRNMTKLEGACY_CLK
#  include <mt_clkbuf_ctl.h>
# else
#  include <mtk-clkbuf-bridge.h>
# endif
#endif

#define MAX_BUFFER_SIZE 260

#define DRIVER_VERSION "2.2.0.1"

/* define the active state of the WAKEUP pin */
#define ST21_IRQ_ACTIVE_HIGH 1
#define ST21_IRQ_ACTIVE_LOW 0

#define I2C_ID_NAME "st21nfc"

#ifdef KRNMTKLEGACY_I2C
#include <linux/dma-mapping.h>
#define NFC_CLIENT_TIMING 400		 /* I2C speed */
static char *I2CDMAWriteBuf; /*= NULL;*/ /* unnecessary initialise */
static unsigned int I2CDMAWriteBuf_pa;   /* = NULL; */
static char *I2CDMAReadBuf; /*= NULL;*/  /* unnecessary initialise */
static unsigned int I2CDMAReadBuf_pa;    /* = NULL; */
#endif					 /* KRNMTKLEGACY_I2C */

/* prototypes */
static irqreturn_t st21nfc_dev_irq_handler(int irq, void *dev_id);
/*
 * The platform data member 'polarity_mode' defines
 * how the wakeup pin is configured and handled.
 * it can take the following values :
 *	 IRQF_TRIGGER_RISING
 *   IRQF_TRIGGER_FALLING
 *   IRQF_TRIGGER_HIGH
 *   IRQF_TRIGGER_LOW
 */

struct st21nfc_platform {
	struct mutex read_mutex;
	struct i2c_client *client;
	int irq_gpio;
	int reset_gpio;
	int ena_gpio;
	int polarity_mode;
	int active_polarity; /* either 0 (low-active) or 1 (high-active)  */
};

/*  NFC IRQ */
static u32 nfc_irq;
static bool irqIsAttached;

static bool device_open; /* Is device open? */
static bool enable_debug_log;

struct st21nfc_dev {
	wait_queue_head_t read_wq;
	struct miscdevice st21nfc_device;
	bool irq_enabled;
	struct st21nfc_platform platform_data;
	spinlock_t irq_enabled_lock;
};

static int st21nfc_loc_set_polaritymode(struct st21nfc_dev *st21nfc_dev,
					int mode)
{

	struct i2c_client *client = st21nfc_dev->platform_data.client;
	unsigned int irq_type;
	int ret;

	if (enable_debug_log)
		pr_info("%s:%d mode %d", __FILE__, __LINE__, mode);

	st21nfc_dev->platform_data.polarity_mode = mode;
	/* setup irq_flags */
	switch (mode) {
	case IRQF_TRIGGER_RISING:
		irq_type = IRQ_TYPE_EDGE_RISING;
		st21nfc_dev->platform_data.active_polarity = 1;
		break;
	case IRQF_TRIGGER_FALLING:
		irq_type = IRQ_TYPE_EDGE_FALLING;
		st21nfc_dev->platform_data.active_polarity = 0;
		break;
	case IRQF_TRIGGER_HIGH:
		irq_type = IRQ_TYPE_LEVEL_HIGH;
		st21nfc_dev->platform_data.active_polarity = 1;
		break;
	case IRQF_TRIGGER_LOW:
		irq_type = IRQ_TYPE_LEVEL_LOW;
		st21nfc_dev->platform_data.active_polarity = 0;
		break;
	default:
		irq_type = IRQF_TRIGGER_FALLING;
		st21nfc_dev->platform_data.active_polarity = 0;
		break;
	}
	if (irqIsAttached) {
		free_irq(client->irq, st21nfc_dev);
		irqIsAttached = false;
	}
	ret = irq_set_irq_type(client->irq, irq_type);
	if (ret) {
		pr_err("%s : set_irq_type failed!!!!!!!\n", __FILE__);
		return -ENODEV;
	}

	/* request irq.  the irq is set whenever the chip has data available
	 * for reading.  it is cleared when all data has been read.
	 */
	if (enable_debug_log)
		pr_debug("%s : requesting IRQ %d\n", __func__, client->irq);

	st21nfc_dev->irq_enabled = true;

	ret = request_irq(client->irq, st21nfc_dev_irq_handler,
			st21nfc_dev->platform_data.polarity_mode,
			client->name, st21nfc_dev);

	if (!ret)
		irqIsAttached = true;

	if (enable_debug_log)
		pr_info("%s:%d ret %d", __FILE__, __LINE__, ret);
	return ret;
}

static void st21nfc_disable_irq(struct st21nfc_dev *st21nfc_dev)
{
	unsigned long flags;

	spin_lock_irqsave(&st21nfc_dev->irq_enabled_lock, flags);
	if (st21nfc_dev->irq_enabled) {
		disable_irq_nosync(st21nfc_dev->platform_data.client->irq);
		st21nfc_dev->irq_enabled = false;
	}
	spin_unlock_irqrestore(&st21nfc_dev->irq_enabled_lock, flags);
}

static irqreturn_t st21nfc_dev_irq_handler(int irq, void *dev_id)
{
	struct st21nfc_dev *st21nfc_dev = dev_id;

	st21nfc_disable_irq(st21nfc_dev);

	/* Wake up waiting readers */
	wake_up(&st21nfc_dev->read_wq);

	return IRQ_HANDLED;
}

static ssize_t st21nfc_dev_read(struct file *filp, char __user *buf,
				size_t count, loff_t *offset)
{
	struct timeval s1, s2, e;
	long t;
	struct st21nfc_dev *st21nfc_dev = container_of(
		filp->private_data, struct st21nfc_dev, st21nfc_device);
	char tmp[MAX_BUFFER_SIZE];
	int ret, pinlev;

	if (count > MAX_BUFFER_SIZE)
		count = MAX_BUFFER_SIZE;

	if (enable_debug_log)
		pr_debug("%s : reading %zu bytes.\n", __func__, count);

	pinlev = gpio_get_value(st21nfc_dev->platform_data.irq_gpio);
	if (((pinlev > 0) &&
		(st21nfc_dev->platform_data.active_polarity == 0)) ||
		((pinlev == 0) &&
		(st21nfc_dev->platform_data.active_polarity == 1))) {
		pr_info("%s : read called but no IRQ.\n", __func__);
		memset(tmp, 0x7E, count);
		if (copy_to_user(buf, tmp, count)) {
			pr_warn("%s : failed to copy to user space\n",
				__func__);
			return -EFAULT;
		}
		return count;
	}

	do_gettimeofday(&s1);
	mutex_lock(&st21nfc_dev->platform_data.read_mutex);
	do_gettimeofday(&s2);

/* Read data */
#ifdef KRNMTKLEGACY_I2C
	st21nfc_dev->platform_data.client->addr =
		(st21nfc_dev->platform_data.client->addr & I2C_MASK_FLAG);
	st21nfc_dev->platform_data.client->ext_flag |= I2C_DMA_FLAG;
	/* st21nfc_dev->platform_data.client->ext_flag |= I2C_DIRECTION_FLAG; */
	/* st21nfc_dev->platform_data.client->ext_flag |= I2C_A_FILTER_MSG; */
	st21nfc_dev->platform_data.client->timing = NFC_CLIENT_TIMING;

	/* Read data */
	ret = i2c_master_recv(st21nfc_dev->platform_data.client,
			(unsigned char *)(uintptr_t)I2CDMAReadBuf_pa,
			count);
#else
	ret = i2c_master_recv(st21nfc_dev->platform_data.client, tmp, count);
#endif
	mutex_unlock(&st21nfc_dev->platform_data.read_mutex);
	do_gettimeofday(&e);

	t = (e.tv_sec - s1.tv_sec) * USEC_PER_SEC;
	t += (e.tv_usec - s1.tv_usec);
	if (t >= 10000) {
		pr_err("%s: took over 10ms (%ld usec)\n", __func__, t);
		t = (e.tv_sec - s2.tv_sec) * USEC_PER_SEC;
		t += (e.tv_usec - s2.tv_usec);
		pr_err("%s: %ld usec spent in i2c_master_recv\n", __func__, t);
	}

	if (ret < 0) {
		pr_err("%s: i2c_master_recv returned %d\n", __func__, ret);
		return ret;
	}
	if (ret > count) {
		pr_err("%s: received too many bytes from i2c (%d)\n", __func__,
			ret);
		return -EIO;
	}
#ifdef KRNMTKLEGACY_I2C
	if (copy_to_user(buf, I2CDMAReadBuf, ret)) {
#else
	if (copy_to_user(buf, tmp, ret)) {
#endif
		pr_warn("%s : failed to copy to user space\n", __func__);
		return -EFAULT;
	}
	return ret;
}

static ssize_t st21nfc_dev_write(struct file *filp, const char __user *buf,
			size_t count, loff_t *offset)
{
	struct st21nfc_dev *st21nfc_dev;
#ifndef KRNMTKLEGACY_I2C
	char tmp[MAX_BUFFER_SIZE];
#endif
	int ret = count;

	st21nfc_dev = container_of(filp->private_data, struct st21nfc_dev,
				st21nfc_device);
	if (enable_debug_log) {
		pr_debug("%s: st21nfc_dev ptr %p\n", __func__, st21nfc_dev);
		pr_debug("%s : writing %zu bytes.\n", __func__, count);
	}

	if (count > MAX_BUFFER_SIZE)
		count = MAX_BUFFER_SIZE;

#ifdef KRNMTKLEGACY_I2C
	if (copy_from_user(I2CDMAWriteBuf, buf, count)) {
#else
	if (copy_from_user(tmp, buf, count)) {
#endif
		pr_err("%s : failed to copy from user space\n", __func__);
		return -EFAULT;
	}

/* Write data */
#ifdef KRNMTKLEGACY_I2C
	st21nfc_dev->platform_data.client->addr =
		(st21nfc_dev->platform_data.client->addr & I2C_MASK_FLAG);

	st21nfc_dev->platform_data.client->ext_flag |= I2C_DMA_FLAG;
	/* st21nfc_dev->platform_data.client->ext_flag |= I2C_DIRECTION_FLAG; */
	/* st21nfc_dev->platform_data.client->ext_flag |= I2C_A_FILTER_MSG; */
	st21nfc_dev->platform_data.client->timing = NFC_CLIENT_TIMING;

	ret = i2c_master_send(st21nfc_dev->platform_data.client,
				(unsigned char *)(uintptr_t)I2CDMAWriteBuf_pa,
				count);
#else
	ret = i2c_master_send(st21nfc_dev->platform_data.client, tmp, count);
#endif
	if (ret != count) {
		pr_err("%s : i2c_master_send returned %d\n", __func__, ret);
		ret = -EIO;
	}
	return ret;
}

static int st21nfc_dev_open(struct inode *inode, struct file *filp)
{
	int ret = 0;
	struct st21nfc_dev *st21nfc_dev = NULL;

	if (enable_debug_log)
		pr_info("%s:%d dev_open", __FILE__, __LINE__);

	if (device_open) {
		ret = -EBUSY;
		pr_err("%s : device already opened ret= %d\n", __func__, ret);
	} else {
		device_open = true;
		st21nfc_dev = container_of(filp->private_data,
					struct st21nfc_dev, st21nfc_device);

		if (enable_debug_log) {
			pr_debug("%s : %d,%d ", __func__, imajor(inode),
				iminor(inode));
			pr_debug("%s: st21nfc_dev ptr %p\n", __func__,
				st21nfc_dev);
		}
	}

#ifndef NO_MTK_CLK_MANAGEMENT
	/*If use XTAL mode, please remove this function "clk_buf_ctrl" to
	 *avoid additional power consumption.
	 */
	clk_buf_ctrl(CLK_BUF_NFC, true);
#endif

	return ret;
}

static int st21nfc_release(struct inode *inode, struct file *file)
{
#ifndef NO_MTK_CLK_MANAGEMENT
	/*If use XTAL mode, please remove this function "clk_buf_ctrl" to
	 *avoid additional power consumption.
	 */
	clk_buf_ctrl(CLK_BUF_NFC, false);
#endif

	device_open = false;
	if (enable_debug_log)
		pr_debug("%s : device_open  = %d\n", __func__, device_open);

	return 0;
}

static void (*st21nfc_st54spi_cb)(int, void *);
static void *st21nfc_st54spi_data;

void st21nfc_register_st54spi_cb(void (*cb)(int, void *), void *data)
{
	pr_info("%s\n", __func__);
	st21nfc_st54spi_cb = cb;
	st21nfc_st54spi_data = data;
}
void st21nfc_unregister_st54spi_cb(void)
{
	pr_info("%s\n", __func__);
	st21nfc_st54spi_cb = NULL;
	st21nfc_st54spi_data = NULL;
}

static long st21nfc_dev_ioctl(struct file *filp, unsigned int cmd,
			unsigned long arg)
{
	struct st21nfc_dev *st21nfc_dev = container_of(
		filp->private_data, struct st21nfc_dev, st21nfc_device);

	int ret = 0;
	u32 tmp;

	/* Check type and command number */
	if (_IOC_TYPE(cmd) != ST21NFC_MAGIC)
		return -ENOTTY;

	/* Check access direction once here; don't repeat below.
	 * IOC_DIR is from the user perspective, while access_ok is
	 * from the kernel perspective; so they look reversed.
	 */
	if (_IOC_DIR(cmd) & _IOC_READ)
		ret = !access_ok(VERIFY_WRITE,
				(void __user *)arg, _IOC_SIZE(cmd));
	if (ret == 0 && _IOC_DIR(cmd) & _IOC_WRITE)
		ret = !access_ok(VERIFY_READ,
				(void __user *)arg, _IOC_SIZE(cmd));
	if (ret)
		return -EFAULT;

	switch (cmd) {

	case ST21NFC_SET_POLARITY_FALLING:
	case ST21NFC_LEGACY_SET_POLARITY_FALLING:
		pr_info(" ### ST21NFC_SET_POLARITY_FALLING ###\n");
		st21nfc_loc_set_polaritymode(st21nfc_dev, IRQF_TRIGGER_FALLING);
		break;

	case ST21NFC_SET_POLARITY_RISING:
	case ST21NFC_LEGACY_SET_POLARITY_RISING:
		pr_info(" ### ST21NFC_SET_POLARITY_RISING ###\n");
		st21nfc_loc_set_polaritymode(st21nfc_dev, IRQF_TRIGGER_RISING);
		break;

	case ST21NFC_SET_POLARITY_LOW:
	case ST21NFC_LEGACY_SET_POLARITY_LOW:
		pr_info(" ### ST21NFC_SET_POLARITY_LOW ###\n");
		st21nfc_loc_set_polaritymode(st21nfc_dev, IRQF_TRIGGER_LOW);
		break;

	case ST21NFC_SET_POLARITY_HIGH:
	case ST21NFC_LEGACY_SET_POLARITY_HIGH:
		pr_info(" ### ST21NFC_SET_POLARITY_HIGH ###\n");
		st21nfc_loc_set_polaritymode(st21nfc_dev, IRQF_TRIGGER_HIGH);
		break;

	case ST21NFC_PULSE_RESET:
	case ST21NFC_LEGACY_PULSE_RESET:
		pr_info("%s Double Pulse Request\n", __func__);
		if (st21nfc_dev->platform_data.reset_gpio != 0) {
			if (st21nfc_st54spi_cb != 0)
				(*st21nfc_st54spi_cb)(ST54SPI_CB_RESET_START,
					st21nfc_st54spi_data);
			/* pulse low for 20 millisecs */
			pr_info("Pulse Request gpio is %d\n",
				st21nfc_dev->platform_data.reset_gpio);
			gpio_set_value(st21nfc_dev->platform_data.reset_gpio,
				0);
			msleep(20);
			gpio_set_value(st21nfc_dev->platform_data.reset_gpio,
				1);
			msleep(20);
			/* pulse low for 20 millisecs */
			gpio_set_value(st21nfc_dev->platform_data.reset_gpio,
				0);
			msleep(20);
			gpio_set_value(st21nfc_dev->platform_data.reset_gpio,
				1);
			pr_info("%s done Double Pulse Request\n", __func__);
			if (st21nfc_st54spi_cb != 0)
				(*st21nfc_st54spi_cb)(ST54SPI_CB_RESET_END,
					st21nfc_st54spi_data);
		}
		break;

	case ST21NFC_GET_WAKEUP:
	case ST21NFC_LEGACY_GET_WAKEUP:
		/* deliver state of Wake_up_pin as return value of ioctl */
		ret = gpio_get_value(st21nfc_dev->platform_data.irq_gpio);
		/*
		 * ret shall be equal to 1 if gpio level equals to polarity.
		 * Warning: depending on gpio_get_value implementation,
		 * it can returns a value different than 1 in case of high level
		 */
		if (((ret == 0) &&
			(st21nfc_dev->platform_data.active_polarity == 0)) ||
			((ret > 0) &&
			(st21nfc_dev->platform_data.active_polarity == 1))) {
			ret = 1;
		} else {
			ret = 0;
		}
		break;
	case ST21NFC_GET_POLARITY:
	case ST21NFC_LEGACY_GET_POLARITY:
		ret = st21nfc_dev->platform_data.polarity_mode;
		if (enable_debug_log)
			pr_debug("%s get polarity %d\n", __func__, ret);
		break;
	case ST21NFC_RECOVERY:
	case ST21NFC_LEGACY_RECOVERY:
		/* For ST21NFCD usage only */
		pr_info("%s Recovery Request\n", __func__);
		if (st21nfc_dev->platform_data.reset_gpio != 0) {
			if (irqIsAttached) {
				struct i2c_client *client =
					st21nfc_dev->platform_data.client;

				free_irq(client->irq, st21nfc_dev);
				irqIsAttached = false;
			}
			gpio_set_value(st21nfc_dev->platform_data.reset_gpio,
					0);
			msleep(20);
			gpio_set_value(st21nfc_dev->platform_data.reset_gpio,
					1);
			msleep(20);
			/* pulse low for 20 millisecs */
			gpio_set_value(st21nfc_dev->platform_data.reset_gpio,
					0);
			msleep(20);
			/* during the reset, force IRQ OUT as PU output instead
			 * of input in normal usage
			 */
			ret = gpio_direction_output(
				st21nfc_dev->platform_data.irq_gpio, 1);
			if (ret) {
				pr_err("%s : gpio_direction_output failed\n",
					__func__);
				ret = -ENODEV;
				break;
			}
			gpio_set_value(st21nfc_dev->platform_data.irq_gpio, 1);
			msleep(20);
			gpio_set_value(st21nfc_dev->platform_data.reset_gpio,
					1);
			pr_info("%s done double Pulse Request\n", __func__);
		}
		msleep(20);
		gpio_set_value(st21nfc_dev->platform_data.irq_gpio, 0);
		msleep(20);
		gpio_set_value(st21nfc_dev->platform_data.irq_gpio, 1);
		msleep(20);
		gpio_set_value(st21nfc_dev->platform_data.irq_gpio, 0);
		msleep(20);
		pr_info("%s Recovery procedure finished\n", __func__);
		ret = gpio_direction_input(st21nfc_dev->platform_data.irq_gpio);
		if (ret) {
			pr_err("%s : gpio_direction_input failed\n", __func__);
			ret = -ENODEV;
		}
		break;
	case ST21NFC_USE_ESE:
		ret = __get_user(tmp, (u32 __user *)arg);
		if (ret == 0) {
			if (st21nfc_st54spi_cb != 0)
				(*st21nfc_st54spi_cb)(tmp ? ST54SPI_CB_ESE_USED
					: ST54SPI_CB_ESE_NOT_USED,
					st21nfc_st54spi_data);
		}
		if (enable_debug_log)
			pr_debug("%s use ESE %d : %d\n", __func__, ret, tmp);
		break;
	default:
		pr_err("%s bad ioctl %u\n", __func__, cmd);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static unsigned int st21nfc_poll(struct file *file, poll_table *wait)
{
	struct st21nfc_dev *st21nfc_dev = container_of(
		file->private_data, struct st21nfc_dev, st21nfc_device);
	unsigned int mask = 0;
	int pinlev = 0;

	/* wait for Wake_up_pin == high  */
	poll_wait(file, &st21nfc_dev->read_wq, wait);

	pinlev = gpio_get_value(st21nfc_dev->platform_data.irq_gpio);

	if (((pinlev == 0) &&
		(st21nfc_dev->platform_data.active_polarity == 0)) ||
		((pinlev > 0) &&
		(st21nfc_dev->platform_data.active_polarity == 1))) {

		mask = POLLIN | POLLRDNORM; /* signal data avail */
		st21nfc_disable_irq(st21nfc_dev);
	} else {
		/* Wake_up_pin  is low. Activate ISR  */
		if (!st21nfc_dev->irq_enabled) {
			if (enable_debug_log)
				pr_debug("%s enable irq\n", __func__);
			st21nfc_dev->irq_enabled = true;
			enable_irq(st21nfc_dev->platform_data.client->irq);
		} else {
			if (enable_debug_log)
				pr_debug("%s irq already enabled\n", __func__);
		}
	}

	return mask;
}

#ifndef KRNMTKLEGACY_GPIO
static int st21nfc_platform_probe(struct platform_device *pdev)
{
	if (enable_debug_log)
		pr_debug("%s\n", __func__);
	return 0;
}

static int st21nfc_platform_remove(struct platform_device *pdev)
{
	if (enable_debug_log)
		pr_debug("%s\n", __func__);
	return 0;
}
#endif /* KRNMTKLEGACY_GPIO */

static const struct file_operations st21nfc_dev_fops = {
	.owner = THIS_MODULE,
	.llseek = no_llseek,
	.read = st21nfc_dev_read,
	.write = st21nfc_dev_write,
	.open = st21nfc_dev_open,
	.poll = st21nfc_poll,
	.release = st21nfc_release,
	.unlocked_ioctl = st21nfc_dev_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = st21nfc_dev_ioctl
#endif
};

static ssize_t i2c_addr_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);

	if (client != NULL)
		return sprintf(buf, "0x%.2x\n", client->addr);
	return 0;
} /* i2c_addr_show() */

static ssize_t i2c_addr_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{

	struct st21nfc_dev *data = dev_get_drvdata(dev);
	long new_addr = 0;

	if (data != NULL && data->platform_data.client != NULL) {
		if (!kstrtol(buf, 10, &new_addr)) {
			mutex_lock(&data->platform_data.read_mutex);
			data->platform_data.client->addr = new_addr;
			mutex_unlock(&data->platform_data.read_mutex);
			return count;
		}
		return -EINVAL;
	}
	return 0;
} /* i2c_addr_store() */

static ssize_t version_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", DRIVER_VERSION);
} /* version_show */

static DEVICE_ATTR_RW(i2c_addr);

static DEVICE_ATTR_RO(version);

static struct attribute *st21nfc_attrs[] = {
	&dev_attr_i2c_addr.attr, &dev_attr_version.attr, NULL,
};

static struct attribute_group st21nfc_attr_grp = {
	.attrs = st21nfc_attrs,
};

#ifdef CONFIG_OF
static int nfc_parse_dt(struct device *dev, struct st21nfc_platform_data *pdata)
{
	int r = 0;
	struct device_node *np = dev->of_node;

	np = of_find_compatible_node(NULL, NULL, "mediatek,nfc-gpio-v2");

	if (np) {
#if (!defined(CONFIG_MTK_GPIO) || defined(CONFIG_MTK_GPIOLIB_STAND))
		r = of_get_named_gpio(np, "gpio-rst-std", 0);
		if (r < 0)
			pr_err("%s: get NFC RST GPIO failed (%d)", __FILE__, r);
		else
			pdata->reset_gpio = r;

		r = of_get_named_gpio(np, "gpio-irq-std", 0);
		if (r < 0)
			pr_err("%s: get NFC IRQ GPIO failed (%d)", __FILE__, r);
		else
			pdata->irq_gpio = r;
		r = 0;
#else
		of_property_read_u32_array(np, "gpio-rst", &(pdata->reset_gpio),
					1);

		of_property_read_u32_array(np, "gpio-irq", &(pdata->irq_gpio),
					1);
#endif
	} else {
		if (enable_debug_log)
			pr_debug("%s : get gpio num err.\n", __func__);
		return -1;
	}

	pdata->polarity_mode = 0;
	pr_info("[dsc]%s : get reset_gpio[%d], irq_gpio[%d], polarity_mode[%d]\n",
		__func__, pdata->reset_gpio, pdata->irq_gpio,
		pdata->polarity_mode);
	return r;
}
#else
static int nfc_parse_dt(struct device *dev, struct st21nfc_platform_data *pdata)
{
	return 0;
}
#endif

static int st21nfc_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int ret;
	struct st21nfc_platform_data *platform_data;
	struct st21nfc_dev *st21nfc_dev;
	struct device_node *node;
	struct gpio_desc *desc;

#ifdef KRNMTKLEGACY_I2C
#ifdef CONFIG_64BIT
	I2CDMAWriteBuf = (char *)dma_alloc_coherent(
		&client->dev, MAX_BUFFER_SIZE, (dma_addr_t *)&I2CDMAWriteBuf_pa,
		GFP_KERNEL);
#else
	I2CDMAWriteBuf = (char *)dma_alloc_coherent(
		NULL, MAX_BUFFER_SIZE, (dma_addr_t *)&I2CDMAWriteBuf_pa,
		GFP_KERNEL);
#endif

	if (I2CDMAWriteBuf == NULL)
		pr_err("%s : failed to allocate dma buffer\n", __func__);
#ifdef CONFIG_64BIT
	I2CDMAReadBuf = (char *)dma_alloc_coherent(
		&client->dev, MAX_BUFFER_SIZE, (dma_addr_t *)&I2CDMAReadBuf_pa,
		GFP_KERNEL);
#else
	I2CDMAReadBuf = (char *)dma_alloc_coherent(
		NULL, MAX_BUFFER_SIZE, (dma_addr_t *)&I2CDMAReadBuf_pa,
		GFP_KERNEL);
#endif

	if (I2CDMAReadBuf == NULL)
		pr_err("%s : failed to allocate dma buffer\n", __func__);
	pr_debug("%s :I2CDMAWriteBuf_pa %d, I2CDMAReadBuf_pa,%d\n", __func__,
		I2CDMAWriteBuf_pa, I2CDMAReadBuf_pa);
#endif /* KRNMTKLEGACY_I2C */
	if (client->dev.of_node) {
		platform_data = devm_kzalloc(
			&client->dev, sizeof(struct st21nfc_platform_data),
			GFP_KERNEL);
		if (!platform_data)
			return -ENOMEM;

		pr_info("%s : Parse st21nfc DTS\n", __func__);
		ret = nfc_parse_dt(&client->dev, platform_data);
		if (ret) {
			pr_err("%s : ret =%d\n", __func__, ret);
			return ret;
		}
		pr_info("%s : Parsed st21nfc DTS %d %d\n", __func__,
			platform_data->reset_gpio, platform_data->irq_gpio);
	} else {
		platform_data = client->dev.platform_data;
		pr_err("%s : No st21nfc DTS\n", __func__);
	}
	if (!platform_data)
		return -EINVAL;

	dev_dbg(&client->dev, "nfc-nci probe: %s, inside nfc-nci flags = %x\n",
		__func__, client->flags);

	if (platform_data == NULL) {
		dev_err(&client->dev, "nfc-nci probe: failed\n");
		return -ENODEV;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s : need I2C_FUNC_I2C\n", __func__);
		return -ENODEV;
	}
	pr_info("%s : after i2c_check_functionality %d %d\n", __func__,
		platform_data->reset_gpio, platform_data->irq_gpio);

	st21nfc_dev = kzalloc(sizeof(*st21nfc_dev), GFP_KERNEL);
	if (st21nfc_dev == NULL) {
		ret = -ENOMEM;
		goto err_exit;
	}

	if (enable_debug_log)
		pr_debug("%s : dev_cb_addr %p\n", __func__, st21nfc_dev);
	pr_info("%s : dev_cb_addr %p\n", __func__, st21nfc_dev);

	/* store for later use */
	st21nfc_dev->platform_data.irq_gpio = platform_data->irq_gpio;
	st21nfc_dev->platform_data.ena_gpio = platform_data->ena_gpio;
	st21nfc_dev->platform_data.reset_gpio = platform_data->reset_gpio;
	st21nfc_dev->platform_data.polarity_mode = platform_data->polarity_mode;
	st21nfc_dev->platform_data.client = client;

	if (enable_debug_log) {
		pr_debug("%s gpio_request, ret is %d %d %d %d // %d %d %d %d\n",
			__func__, st21nfc_dev->platform_data.irq_gpio,
			st21nfc_dev->platform_data.ena_gpio,
			st21nfc_dev->platform_data.reset_gpio,
			st21nfc_dev->platform_data.polarity_mode,
			platform_data->irq_gpio, platform_data->ena_gpio,
			platform_data->reset_gpio,
			platform_data->polarity_mode);

		desc = gpio_to_desc(platform_data->irq_gpio);
		if (!desc)
			pr_debug("gpio_desc is null\n");
		else
			pr_debug("gpio_desc isn't null\n");

		if (gpio_is_valid(platform_data->irq_gpio))
			pr_debug("gpio number %d is valid\n",
				platform_data->irq_gpio);

		if (gpio_is_valid(platform_data->reset_gpio))
			pr_debug("gpio number %d is valid\n",
				platform_data->reset_gpio);
	}

	ret = gpio_request(platform_data->irq_gpio,
#if (!defined(CONFIG_MTK_GPIO) || defined(CONFIG_MTK_GPIOLIB_STAND))
			"gpio-irq-std"
#else
			"gpio-irq"
#endif
			);
	if (ret) {
		pr_err("%s : gpio_request failed\n", __FILE__);
		goto err_free_buffer;
	}
	pr_info("%s : IRQ GPIO = %d\n", __func__, platform_data->irq_gpio);
	ret = gpio_direction_input(platform_data->irq_gpio);
	if (ret) {
		pr_err("%s : gpio_direction_input failed\n", __FILE__);
		ret = -ENODEV;
		goto err_free_buffer;
	}

	st21nfc_dev->platform_data.client->irq = platform_data->irq_gpio;

	/* initialize irqIsAttached variable */
	irqIsAttached = false;

	/* initialize device_open variable */
	device_open = 0;

	/* handle optional RESET */
	if (platform_data->reset_gpio != 0) {
		ret = gpio_request(platform_data->reset_gpio,
#if (!defined(CONFIG_MTK_GPIO) || defined(CONFIG_MTK_GPIOLIB_STAND))
				"gpio-rst-std"
#else
				"gpio-rst"
#endif
				);
		if (ret) {
			pr_err("%s : reset gpio_request failed\n", __FILE__);
			ret = -ENODEV;
			goto err_free_buffer;
		}
		pr_info("%s : RST GPIO = %d\n", __func__,
			platform_data->reset_gpio);
		ret = gpio_direction_output(platform_data->reset_gpio, 1);
		if (ret) {
			pr_err("%s : reset gpio_direction_output failed\n",
				__FILE__);
			ret = -ENODEV;
			goto err_free_buffer;
		}
		/* low active */
		gpio_set_value(st21nfc_dev->platform_data.reset_gpio, 1);
	}

	/* set up optional ENA gpio */
	if (platform_data->ena_gpio != 0) {
		ret = gpio_request(platform_data->ena_gpio, "st21nfc_ena");
		if (ret) {
			pr_err("%s : ena gpio_request failed\n", __FILE__);
			ret = -ENODEV;
			goto err_free_buffer;
		}
		ret = gpio_direction_output(platform_data->ena_gpio, 1);
		if (ret) {
			pr_err("%s : ena gpio_direction_output failed\n",
				__FILE__);
			ret = -ENODEV;
			goto err_free_buffer;
		}
	}

	node = of_find_compatible_node(NULL, NULL, "mediatek,irq_nfc-eint");

	if (node) {

		nfc_irq = irq_of_parse_and_map(node, 0);

		client->irq = nfc_irq;

		pr_info("%s : MT IRQ GPIO = %d\n", __func__, client->irq);

		enable_irq_wake(client->irq);

	} else {
		pr_err("%s : can not find NFC eint compatible node\n",
			__func__);
	}
	/* init mutex and queues */
	init_waitqueue_head(&st21nfc_dev->read_wq);
	mutex_init(&st21nfc_dev->platform_data.read_mutex);
	spin_lock_init(&st21nfc_dev->irq_enabled_lock);

	st21nfc_dev->st21nfc_device.minor = MISC_DYNAMIC_MINOR;
	st21nfc_dev->st21nfc_device.name = I2C_ID_NAME;
	st21nfc_dev->st21nfc_device.fops = &st21nfc_dev_fops;
	st21nfc_dev->st21nfc_device.parent = &client->dev;

	i2c_set_clientdata(client, st21nfc_dev);
	ret = misc_register(&st21nfc_dev->st21nfc_device);
	if (ret) {
		pr_info("ret of misc_register:%d\n", ret);
		pr_err("%s : misc_register failed\n", __FILE__);
		goto err_misc_register;
	}

	if (sysfs_create_group(&client->dev.kobj, &st21nfc_attr_grp)) {
		pr_err("%s : sysfs_create_group failed\n", __FILE__);
		goto err_request_irq_failed;
	}
	st21nfc_disable_irq(st21nfc_dev);
	return 0;

err_request_irq_failed:
	misc_deregister(&st21nfc_dev->st21nfc_device);
err_misc_register:
	mutex_destroy(&st21nfc_dev->platform_data.read_mutex);
err_free_buffer:
	kfree(st21nfc_dev);
err_exit:
	gpio_free(platform_data->irq_gpio);
	if (platform_data->ena_gpio != 0)
		gpio_free(platform_data->ena_gpio);
	return ret;
}

static int st21nfc_remove(struct i2c_client *client)
{
	struct st21nfc_dev *st21nfc_dev;

#ifdef KRNMTKLEGACY_I2C
	if (I2CDMAWriteBuf) {
#ifdef CONFIG_64BIT
		dma_free_coherent(&client->dev, MAX_BUFFER_SIZE, I2CDMAWriteBuf,
				I2CDMAWriteBuf_pa);
#else
		dma_free_coherent(NULL, MAX_BUFFER_SIZE, I2CDMAWriteBuf,
				I2CDMAWriteBuf_pa);
#endif
		I2CDMAWriteBuf = NULL;
		I2CDMAWriteBuf_pa = 0;
	}

	if (I2CDMAReadBuf) {
#ifdef CONFIG_64BIT
		dma_free_coherent(&client->dev, MAX_BUFFER_SIZE, I2CDMAReadBuf,
				I2CDMAReadBuf_pa);
#else
		dma_free_coherent(NULL, MAX_BUFFER_SIZE, I2CDMAReadBuf,
				I2CDMAReadBuf_pa);
#endif
		I2CDMAReadBuf = NULL;
		I2CDMAReadBuf_pa = 0;
	}
#endif /* KRNMTKLEGACY_I2C */
	st21nfc_dev = i2c_get_clientdata(client);
	free_irq(client->irq, st21nfc_dev);
	misc_deregister(&st21nfc_dev->st21nfc_device);
	mutex_destroy(&st21nfc_dev->platform_data.read_mutex);
	gpio_free(st21nfc_dev->platform_data.irq_gpio);
	if (st21nfc_dev->platform_data.ena_gpio != 0)
		gpio_free(st21nfc_dev->platform_data.ena_gpio);
	kfree(st21nfc_dev);

	return 0;
}

static const struct i2c_device_id st21nfc_id[] = {{"st21nfc", 0}, {} };

#ifdef CONFIG_OF
static const struct of_device_id nfc_switch_of_match[] = {
	{.compatible = "mediatek,nfc"}, {},
};
#endif

static struct i2c_driver st21nfc_driver = {
	.id_table = st21nfc_id,
	.probe = st21nfc_probe,
	.remove = st21nfc_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = I2C_ID_NAME,
#ifdef CONFIG_OF
		.of_match_table = nfc_switch_of_match,
#endif
	},
};

#ifndef KRNMTKLEGACY_GPIO
/*  platform driver */
static const struct of_device_id nfc_dev_of_match[] = {
	{
	.compatible = "mediatek,nfc-gpio-v2",
	},
	{},
};

static struct platform_driver st21nfc_platform_driver = {
	.probe = st21nfc_platform_probe,
	.remove = st21nfc_platform_remove,
	.driver = {
		.name = I2C_ID_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = nfc_dev_of_match,
#endif
	},
};
#endif /* KRNMTKLEGACY_GPIO */

/* module load/unload record keeping */
static int __init st21nfc_dev_init(void)
{
	pr_info("Loading st21nfc driver\n");
#ifndef KRNMTKLEGACY_GPIO
	platform_driver_register(&st21nfc_platform_driver);
	if (enable_debug_log)
		pr_debug("Loading st21nfc i2c driver\n");
#endif
	return i2c_add_driver(&st21nfc_driver);
}

module_init(st21nfc_dev_init);

static void __exit st21nfc_dev_exit(void)
{
	pr_info("Unloading st21nfc driver\n");
	i2c_del_driver(&st21nfc_driver);
}

module_exit(st21nfc_dev_exit);

MODULE_AUTHOR("Norbert Kawulski");
MODULE_DESCRIPTION("NFC ST21NFC driver");
MODULE_VERSION(DRIVER_VERSION);
MODULE_LICENSE("GPL");
