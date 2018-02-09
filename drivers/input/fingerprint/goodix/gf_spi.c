/*Simple synchronous userspace interface to SPI devices
 *
 * Copyright (C) 2006 SWAPP
 *     Andrea Paterniani <a.paterniani@swapp-eng.it>
 * Copyright (C) 2007 David Brownell (simplification, cleanup)
 * Copyright (C) 2018 XiaoMi, Inc.
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/input.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/compat.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <linux/ktime.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>
#include <linux/timer.h>
#include <linux/notifier.h>
#include <linux/fb.h>
#include <linux/pm_qos.h>
#include <linux/cpufreq.h>
#include <linux/wakelock.h>
#include "gf_spi.h"
#include "gf_common.h"

#if defined(USE_SPI_BUS)
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>
#elif defined(USE_PLATFORM_BUS)
#include <linux/platform_device.h>
#endif

#define GF_SPIDEV_NAME     "goodix,fingerprint"
/*device name after register in charater*/
#define GF_DEV_NAME            "goodix_fp"
#define	GF_INPUT_NAME	    "qwerty"	/*"goodix_fp" */

#define	CHRD_DRIVER_NAME	"goodix_fp_spi"
#define	CLASS_NAME		    "goodix_fp"

#define SPIDEV_MAJOR		214	/* assigned */
#define N_SPI_MINORS		32	/* ... up to 256 */


/*GF regs*/
#define GF_CHIP_ID_LO				0x0000
#define GF_CHIP_ID_HI				0x0002
#define GF_VENDOR_ID				0x0006
#define GF_IRQ_CTRL2				0x0124
#define GF_IRQ_CTRL3				0x0126

/*GF input keys*/
#define	GF_KEY_POWER	KEY_POWER
#define	GF_KEY_HOME		KEY_HOME
#define	GF_KEY_MENU		KEY_MENU
#define	GF_KEY_BACK		KEY_BACK
#define GF_UP_KEY		KEY_UP
#define GF_DOWN_KEY	    KEY_DOWN
#define GF_LEFT_KEY		KEY_LEFT
#define GF_RIGHT_KEY	KEY_RIGHT
#define	GF_KEY_FORCE    KEY_F9
#define GF_APP_SWITCH	KEY_F19
#define GF_KEY_F1       KEY_F1
#define GF_KEY_F2       KEY_F2
#define GF_KEY_F3       KEY_F3
/**************************debug******************************/
#define GF_DEBUG
/*#undef  GF_DEBUG*/

#ifdef GF_DEBUG
#define gf_dbg(fmt, args...) do { \
	pr_info("gf:" fmt, ##args);\
} while (0)
#define FUNC_ENTRY()  pr_info("gf:%s, entry\n", __func__)
#define FUNC_EXIT()  pr_info("gf:%s, exit\n", __func__)
#else
#define gf_dbg(fmt, args...)
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif

/*Global variables*/
static DECLARE_BITMAP(minors, N_SPI_MINORS);
static LIST_HEAD(device_list);
static DEFINE_MUTEX(device_list_lock);

static struct gf_dev gf;
static unsigned int bufsiz = 22180;
static unsigned char g_frame_buf[22180] = {0};
static unsigned short g_vendorID;
static unsigned int g_chipID;
static struct wake_lock fp_wakelock;
static unsigned int gf_spi_speed[GF_SPI_KEEP_SPEED] = {4800000, 4800000};

/******************* CLK Related Start ****************/
static long spi_clk_max_rate(struct clk *clk, unsigned long rate)
{
	long lowest_available, nearest_low, step_size, cur;
	long step_direction = -1;
	long guess = rate;
	int max_steps = 10;

	FUNC_ENTRY();
	cur = clk_round_rate(clk, rate);
	if (cur == rate)
		return rate;

	/* if we got here then: cur > rate */
	lowest_available = clk_round_rate(clk, 0);
	if (lowest_available > rate)
		return -EINVAL;

	step_size = (rate - lowest_available) >> 1;
	nearest_low = lowest_available;

	while (max_steps-- && step_size) {
		guess += step_size * step_direction;
		cur = clk_round_rate(clk, guess);

		if ((cur < rate) && (cur > nearest_low))
			nearest_low = cur;
		/*
		 * if we stepped too far, then start stepping in the other
		 * direction with half the step size
		 */
		if (((cur > rate) && (step_direction > 0))
				|| ((cur < rate) && (step_direction < 0))) {
			step_direction = -step_direction;
			step_size >>= 1;
		}
	}
	return nearest_low;
}

static void spi_clock_set(struct gf_dev *gf_dev, int speed)
{
	long rate;
	int rc;
	FUNC_ENTRY();

	rate = spi_clk_max_rate(gf_dev->core_clk, speed);
	if (rate < 0) {
		pr_info("%s: no match found for requested clock frequency:%d",
				__func__, speed);
		return;
	}

	rc = clk_set_rate(gf_dev->core_clk, rate);
}

#ifdef USE_SPI_BUS
static int gfspi_ioctl_clk_init(struct spi_device *spi, struct gf_dev *data)
{
	pr_debug("%s: enter\n", __func__);

	FUNC_ENTRY();
	data->clk_enabled = 0;
	data->core_clk = clk_get(&spi->dev, "core_clk");
	if (IS_ERR_OR_NULL(data->core_clk)) {
		pr_err("%s: fail to get core_clk\n", __func__);
		return -EPERM;
	}
	data->iface_clk = clk_get(&spi->dev, "iface_clk");
	if (IS_ERR_OR_NULL(data->iface_clk)) {
		pr_err("%s: fail to get iface_clk\n", __func__);
		clk_put(data->core_clk);
		data->core_clk = NULL;
		return -ENOENT;
	}
	return 0;
}

static int gfspi_ioctl_clk_enable(struct gf_dev *data)
{
	int err;

	pr_debug("%s: enter\n", __func__);
	FUNC_ENTRY();

	if (data->clk_enabled)
		return 0;

	err = clk_prepare_enable(data->core_clk);
	if (err) {
		pr_err("%s: fail to enable core_clk\n", __func__);
		return -EPERM;
	}

	err = clk_prepare_enable(data->iface_clk);
	if (err) {
		pr_err("%s: fail to enable iface_clk\n", __func__);
		clk_disable_unprepare(data->core_clk);
		return -ENOENT;
	}

	data->clk_enabled = 1;

	return 0;
}

static int gfspi_ioctl_clk_disable(struct gf_dev *data)
{
	FUNC_ENTRY();

	if (!data->clk_enabled)
		return 0;

	clk_disable_unprepare(data->core_clk);
	clk_disable_unprepare(data->iface_clk);
	data->clk_enabled = 0;

	return 0;
}

static int gfspi_ioctl_clk_uninit(struct gf_dev *data)
{
	pr_debug("%s: enter\n", __func__);

	FUNC_ENTRY();
	if (data->clk_enabled)
		gfspi_ioctl_clk_disable(data);

	if (!IS_ERR_OR_NULL(data->core_clk)) {
		clk_put(data->core_clk);
		data->core_clk = NULL;
	}

	if (!IS_ERR_OR_NULL(data->iface_clk)) {
		clk_put(data->iface_clk);
		data->iface_clk = NULL;
	}

	return 0;
}
#endif
/******************* CLK Related End ****************/

/******************* Enable/Disable IRQ Start ****************/
static void gf_enable_irq(struct gf_dev *gf_dev)
{
	FUNC_ENTRY();

	if (gf_dev->irq_enabled) {
		pr_warn("IRQ has been enabled.\n");
	} else {
		enable_irq(gf_dev->irq);
		gf_dev->irq_enabled = 1;
	}

	FUNC_EXIT();
}

static void gf_disable_irq(struct gf_dev *gf_dev)
{
	FUNC_ENTRY();

	if (gf_dev->irq_enabled) {
		gf_dev->irq_enabled = 0;
		disable_irq(gf_dev->irq);
	} else {
		pr_warn("IRQ has been disabled.\n");
	}

	FUNC_EXIT();
}
/******************* Enable/Disable IRQ End ****************/

void gf_spi_setup(struct gf_dev *gf_dev, enum gf_spi_transfer_speed speed)
{

	int ret = 0;

	pr_info("####### %s %d \n", __func__, __LINE__);
	if (speed == GF_SPI_KEEP_SPEED)
		return;
	gf_dev->spi->mode = SPI_MODE_0;
	gf_dev->spi->max_speed_hz = gf_spi_speed[speed];
	gf_dev->spi->bits_per_word = 8;
	ret = spi_setup(gf_dev->spi);
	pr_info("%s spi_setup ret = %d", __func__, ret);
}

static int gf_write_configs(struct gf_dev *gf_dev, struct gf_configs *config, int len)
{
	int cnt;
	int length = len;
	int ret = 0;
	for (cnt = 0; cnt < length; cnt++) {
		ret = gf_spi_write_word(gf_dev, config[cnt].addr, config[cnt].value);
		if (ret < 0) {
			pr_info("%s failed. \n", __func__);
			return ret;
		}
	}

	return 0;
}

static ssize_t gf_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	struct gf_dev *gf_dev = filp->private_data;
	int status = 0;
	int len = 0;

	if (buf == NULL || count > bufsiz) {
		pr_info("%s input parameters invalid. bufsiz = %d,count = %d \n",
				__func__, bufsiz, (int)count);
		return -EMSGSIZE;
	}

	len = gf_spi_read_data(gf_dev, 0xAAAA, count, g_frame_buf);
	status = copy_to_user(buf, g_frame_buf, count);
	if (status != 0) {
		pr_info("%s copy_to_user failed. status = %d \n", __func__, status);
		return -EFAULT;
	}
	return 0;
}


static long gf_ioctl(struct file *filp, unsigned int cmd,
		unsigned long arg)
{
	struct gf_dev *gf_dev = NULL;
	struct gf_ioc_transfer *ioc = NULL;
	struct gf_key gf_key = {0};
	u8 *tmpbuf = NULL;
	int ret = 0;
	int retval = 0;
	int err = 0;
	unsigned char command = 0;
	struct gf_configs *p_cfg = NULL;
	unsigned char cfg_len = 0;
	struct gf_mode_config *p_mode_cfg = NULL;
	enum gf_spi_transfer_speed speed;

	if (_IOC_TYPE(cmd) != GF_IOC_MAGIC)
		return -ENOTTY;
	/* Check access direction once here; don't repeat below.
	 * IOC_DIR is from the user perspective, while access_ok is
	 * from the kernel perspective; so they look reversed.
	 */
	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	if (err == 0 && _IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	if (err)
		return -EFAULT;

	gf_dev = (struct gf_dev *)filp->private_data;
	switch (cmd) {
	case GF_IOC_RW:
		ioc = kzalloc(sizeof(*ioc), GFP_KERNEL);
		if (ioc == NULL) {
			pr_info("kzalloc ioc failed. \n");
			retval = -ENOMEM;
			break;
		}

		/*copy command data from user to kernel.*/
		if (copy_from_user(ioc, (struct gf_ioc_transfer *)arg, sizeof(*ioc))) {
			pr_info("Failed to copy command from user to kernel.\n");
			retval = -EFAULT;
			break;
		}

		tmpbuf = kzalloc(ioc->len, GFP_KERNEL);
		if (tmpbuf == NULL) {
			pr_info("kzalloc buf failed. \n");
			retval = -ENOMEM;
			break;
		}
		if ((ioc->len > bufsiz) || (ioc->len == 0)) {
			pr_info("%s The request length[%d] is longer than supported maximum buffer length[%d].\n"
					, __func__, ioc->len, bufsiz);
			retval = -EMSGSIZE;
			break;
		}

		if (ioc->cmd == GF_R) {
			mutex_lock(&gf_dev->frame_lock);
			gf_spi_read_data(gf_dev, ioc->addr, ioc->len, tmpbuf);

			ret = copy_to_user((void __user *)ioc->buf, tmpbuf, ioc->len);
			mutex_unlock(&gf_dev->frame_lock);

			if (ret) {
				pr_info("%s Failed to copy data from kernel to user.\n", __func__);
				retval = -EFAULT;
				break;
			}
		} else if (ioc->cmd == GF_W) {
			ret = copy_from_user(tmpbuf, (void __user *)ioc->buf, ioc->len);
			if (ret) {
				pr_info("%s Failed to copy data from user to kernel.\n", __func__);
				retval = -EFAULT;
				break;
			}
			mutex_lock(&gf_dev->frame_lock);
			gf_spi_write_data(gf_dev, ioc->addr, ioc->len, tmpbuf);
			mutex_unlock(&gf_dev->frame_lock);
		} else {
			pr_warn("%s Error command for ioc->cmd.\n", __func__);
			retval = -EFAULT;
		}
		break;
	case GF_IOC_CMD:
		retval = __get_user(command, (u32 __user *)arg);
		mutex_lock(&gf_dev->frame_lock);
		gf_spi_send_cmd(gf_dev, &command, 1);
		mutex_unlock(&gf_dev->frame_lock);
		mdelay(1);
		break;
	case GF_IOC_CONFIG:
		p_mode_cfg = kzalloc(sizeof(*p_mode_cfg), GFP_KERNEL);
		if (p_mode_cfg == NULL) {
			pr_info("kzalloc p_mode_cfg failed. \n");
			retval = -ENOMEM;
			break;
		}

		if (copy_from_user(p_mode_cfg, (struct gf_mode_config *)arg, sizeof(*p_mode_cfg))) {
			pr_info("Failed to copy command from user to kernel.\n");
			retval = -EFAULT;
			break;
		}

		cfg_len = p_mode_cfg->cfg_len*sizeof(struct gf_configs);
		if (cfg_len > 0) {
			p_cfg = kzalloc(cfg_len, GFP_KERNEL);
			if (p_mode_cfg == NULL) {
				pr_info("kzalloc p_cfg failed. \n");
				retval = -ENOMEM;
				break;
			}
		} else {
			pr_info("err cfg_len = %d\n", cfg_len);
			retval = -EFAULT;
			break;
		}

		if (copy_from_user(p_cfg, p_mode_cfg->p_cfg, cfg_len)) {
			pr_info("Failed to copy command from user to kernel.\n");
			retval = -EFAULT;
			break;
		}
		gf_write_configs(gf_dev, p_cfg, p_mode_cfg->cfg_len);
		break;
	case GF_IOC_RESET:
		pr_info("%s GF_IOC_REINIT \n", __func__);

		gf_hw_reset(gf_dev, 0);
		break;
	case GF_IOC_ENABLE_IRQ:
		pr_info("%s ++++++++++++ GF_IOC_ENABLE_IRQ \n", __func__);
		gf_enable_irq(gf_dev);
		break;
	case GF_IOC_DISABLE_IRQ:
		pr_info("%s ------------ GF_IOC_DISABLE_IRQ \n", __func__);
		gf_disable_irq(gf_dev);
		break;
	case GF_IOC_SENDKEY:
		if (copy_from_user
				(&gf_key, (struct gf_key *)arg, sizeof(struct gf_key))) {
			pr_warn("Failed to copy data from user space,line=%d.\n", __LINE__);
			retval = -EFAULT;
			break;

		}
		input_report_key(gf_dev->input, gf_key.key, gf_key.value);
		input_sync(gf_dev->input);
		break;
	case GF_IOC_SETSPEED:
		retval = __get_user(speed, (u32 __user *)arg);
		gf_spi_setup(gf_dev, speed);
		break;
	default:
		pr_info("%s gf doesn't support this command.\n", __func__);
		pr_info("%s CMD = 0x%x,_IOC_DIR:0x%x,_IOC_TYPE:0x%x,IOC_NR:0x%x,IOC_SIZE:0x%x\n",
				__func__, cmd, _IOC_DIR(cmd), _IOC_TYPE(cmd), _IOC_NR(cmd), _IOC_SIZE(cmd));
		retval = -EFAULT;
		break;
	}

	if (tmpbuf != NULL) {
		kfree(tmpbuf);
		tmpbuf = NULL;
	}
	if (ioc != NULL) {
		kfree(ioc);
		ioc = NULL;
	}
	if (p_cfg != NULL) {
		kfree(p_cfg);
		p_cfg = NULL;
	}
	if (p_mode_cfg != NULL) {
		kfree(p_mode_cfg);
		p_mode_cfg = NULL;
	}
	return retval;
}

#ifdef CONFIG_COMPAT
	static long
gf_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	return gf_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
}
#endif /*CONFIG_COMPAT*/

static irqreturn_t gf_irq(int irq, void *handle)
{
	struct gf_dev *gf_dev = &gf;
	unsigned short irq_reg = 0;

	wake_lock(&fp_wakelock);
#ifdef GF_FASYNC
	gf_spi_read_word(gf_dev, GF_IRQ_CTRL3, &irq_reg);
	if (irq_reg != 0x08) {
		if (gf_dev->async)
			kill_fasync(&gf_dev->async, SIGIO, POLL_IN);
	}
#endif

	return IRQ_HANDLED;
}

static int gf_open(struct inode *inode, struct file *filp)
{
	struct gf_dev *gf_dev;
	int status = -ENXIO;
	int cnt = 0;
	unsigned short reg = 0;

	FUNC_ENTRY();

	mutex_lock(&device_list_lock);

	list_for_each_entry(gf_dev, &device_list, device_entry) {
		if (gf_dev->devt == inode->i_rdev) {
			gf_dbg("Found\n");
			status = 0;
			break;
		}
	}

	if (status == 0) {
		if (status == 0) {
			gf_dev->users++;
			filp->private_data = gf_dev;

			gf_hw_reset(gf_dev, 0);
			while (cnt < 5) {
				gf_spi_read_word(gf_dev, GF_IRQ_CTRL3, &reg);
				if (reg == 0x100 || reg == 0x400 || reg == 0x800) {
					gf_spi_write_word(gf_dev, GF_IRQ_CTRL2, reg);
					gf_dbg("reg = 0x%04x cnt = %d\n", reg, cnt);
					break;
				}
				cnt++;
			}

			nonseekable_open(inode, filp);
			gf_dbg("Succeed to open device. irq = %d, user = %d\n",
					gf_dev->irq, gf_dev->users);
			if (gf_dev->users == 1) {
				gf_enable_irq(gf_dev);
			}
		}
	} else {
		gf_dbg("No device for minor %d\n", iminor(inode));
	}

	mutex_unlock(&device_list_lock);
	FUNC_EXIT();
	return status;
}

#ifdef GF_FASYNC
static int gf_fasync(int fd, struct file *filp, int mode)
{
	struct gf_dev *gf_dev = filp->private_data;
	int ret;

	FUNC_ENTRY();
	ret = fasync_helper(fd, filp, mode, &gf_dev->async);
	FUNC_EXIT();
	gf_dbg("ret = %d\n", ret);
	return ret;
}
#endif

static int gf_release(struct inode *inode, struct file *filp)
{
	struct gf_dev *gf_dev;
	int status = 0;

	FUNC_ENTRY();
	mutex_lock(&device_list_lock);
	gf_dev = filp->private_data;
	filp->private_data = NULL;

	/*last close?? */
	gf_dev->users--;
	if (!gf_dev->users) {

		gf_dbg("disble_irq. irq = %d\n", gf_dev->irq);
		gf_disable_irq(gf_dev);
	}
	mutex_unlock(&device_list_lock);
	FUNC_EXIT();
	return status;
}

static const struct file_operations gf_fops = {
	.owner = THIS_MODULE,
	/* REVISIT switch to aio primitives, so that userspace
	 * gets more complete API coverage.  It'll simplify things
	 * too, except for the locking.
	 */
	.unlocked_ioctl = gf_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = gf_compat_ioctl,
#endif /*CONFIG_COMPAT*/
	.open = gf_open,
	.release = gf_release,
	.read = gf_read,
#ifdef GF_FASYNC
	.fasync = gf_fasync,
#endif
};

static void gf_reg_key_kernel(struct gf_dev *gf_dev)
{
	__set_bit(EV_KEY, gf_dev->input->evbit);
	__set_bit(GF_KEY_POWER, gf_dev->input->keybit);
	__set_bit(GF_KEY_HOME, gf_dev->input->keybit);
	__set_bit(GF_KEY_MENU, gf_dev->input->keybit);
	__set_bit(GF_KEY_BACK, gf_dev->input->keybit);
	__set_bit(GF_UP_KEY, gf_dev->input->keybit);
	__set_bit(GF_RIGHT_KEY, gf_dev->input->keybit);
	__set_bit(GF_LEFT_KEY, gf_dev->input->keybit);
	__set_bit(GF_DOWN_KEY, gf_dev->input->keybit);
	__set_bit(GF_KEY_FORCE, gf_dev->input->keybit);
	__set_bit(GF_APP_SWITCH, gf_dev->input->keybit);


	__set_bit(GF_KEY_F1, gf_dev->input->keybit);
	__set_bit(GF_KEY_F2, gf_dev->input->keybit);
	__set_bit(GF_KEY_F3, gf_dev->input->keybit);

	gf_dev->input->name = GF_INPUT_NAME;
	if (input_register_device(gf_dev->input))
		pr_warn("Failed to register GF as input device.\n");
}

static struct class *gf_class;
#if defined(USE_SPI_BUS)
static int gf_probe(struct spi_device *spi)
#elif defined(USE_PLATFORM_BUS)
static int gf_probe(struct platform_device *pdev)
#endif
{
	struct gf_dev *gf_dev = &gf;
	int status = -EINVAL;
	unsigned long minor;
	int ret;
	unsigned short chip_id_1 = 0;
	unsigned short chip_id_2 = 0;
	FUNC_ENTRY();
	/* Initialize the driver data */
	INIT_LIST_HEAD(&gf_dev->device_entry);
#if defined(USE_SPI_BUS)
	gf_dev->spi = spi;
#elif defined(USE_PLATFORM_BUS)
	gf_dev->spi = pdev;
#endif
	gf_dev->irq_gpio = -EINVAL;
	gf_dev->reset_gpio = -EINVAL;
	gf_dev->pwr_gpio = -EINVAL;
	gf_dev->device_available = 0;
	gf_dev->fb_black = 0;

	mutex_init(&gf_dev->buf_lock);
	mutex_init(&gf_dev->frame_lock);
	spin_lock_init(&gf_dev->spi_lock);

	if (gf_parse_dts(gf_dev))
		goto error;

	if (gf_power_on(gf_dev))
		goto error;

	if (gf_hw_reset(gf_dev, 0))
		goto error;

	/* If we can allocate a minor number, hook up this device.
	 * Reusing minors is fine so long as udev or mdev is working.
	 */
	mutex_lock(&device_list_lock);
	minor = find_first_zero_bit(minors, N_SPI_MINORS);
	if (minor < N_SPI_MINORS) {
		struct device *dev;

		gf_dev->devt = MKDEV(SPIDEV_MAJOR, minor);
		dev = device_create(gf_class, &gf_dev->spi->dev, gf_dev->devt,
				gf_dev, GF_DEV_NAME);
		status = IS_ERR(dev) ? PTR_ERR(dev) : 0;
	} else {
		dev_dbg(&gf_dev->spi->dev, "no minor number available!\n");
		status = -ENODEV;
	}

	if (status == 0) {
		set_bit(minor, minors);
		list_add(&gf_dev->device_entry, &device_list);
	} else {
		gf_dev->devt = 0;
	}
	mutex_unlock(&device_list_lock);
	wake_lock_init(&fp_wakelock, WAKE_LOCK_SUSPEND, "fp_wakelock");

	if (status == 0) {
		gf_dev->gBuffer = kzalloc(bufsiz + GF_RDATA_OFFSET, GFP_KERNEL);
		if (gf_dev->gBuffer == NULL) {
			return -ENOMEM;
		}

		/*input device subsystem */
		gf_dev->input = input_allocate_device();
		if (gf_dev->input == NULL) {
			dev_dbg(&gf_dev->input->dev,
					"Faile to allocate input device.\n");
			status = -ENOMEM;
		}
#ifdef USE_SPI_BUS
		/* Enable spi clock */
		if (gfspi_ioctl_clk_init(spi, gf_dev))
			goto gfspi_probe_clk_init_failed;

		if (gfspi_ioctl_clk_enable(gf_dev))
			goto gfspi_probe_clk_enable_failed;

		spi_clock_set(gf_dev, 9600000);
#else
		pr_info("Get the clk resource.\n");
		gf_dev->core_clk = clk_get(&pdev->dev, "core_clk");
		if (IS_ERR(gf_dev->core_clk)) {
			pr_warn("Failed to get core_clk.\n");
			goto error;
		}
		gf_dev->iface_clk = clk_get(&pdev->dev, "iface_clk");
		if (IS_ERR(gf_dev->iface_clk)) {
			pr_warn("Failed to get iface_clk.\n");
			goto error;
		}
#endif

		gf_reg_key_kernel(gf_dev);

		gf_spi_setup(gf_dev, GF_SPI_HIGH_SPEED);
		gf_dev->irq = gf_irq_num(gf_dev);
		ret = request_threaded_irq(gf_dev->irq, NULL, gf_irq,
				IRQF_TRIGGER_RISING | IRQF_ONESHOT,
				"gf", gf_dev);
		if (!ret) {
			pr_info("%s called enable_irq_wake.\n", __func__);
			enable_irq_wake(gf_dev->irq);

			gf_dev->irq_enabled = 1;
			gf_disable_irq(gf_dev);
		}
	}
	gf_spi_write_word(gf_dev, GF_IRQ_CTRL2, 0xFFFF); /*0x0124, 0x0400 clean reset INT*/

	/*Get vendorID */
	gf_spi_read_word(gf_dev, GF_VENDOR_ID, &g_vendorID);
	pr_info("%s vendorID = 0x%04X\n", __func__, g_vendorID);

	gf_spi_read_word(gf_dev, GF_CHIP_ID_LO, &chip_id_1);
	gf_spi_read_word(gf_dev, GF_CHIP_ID_HI, &chip_id_2);
	g_chipID = ((chip_id_2<<16)|(chip_id_1)) >> 8;
	pr_info("%s: chip_id_hi = 0x%x, chip_id_low = 0x%x, g_chipID is 0x%08x \n", __func__, chip_id_2, chip_id_1, g_chipID);

	pr_info("succeed to probe.\n");
	gf_dev->device_available = 1;
	return status;

error:
	gf_cleanup(gf_dev);
	gf_dev->device_available = 0;
	if (gf_dev->devt != 0) {
		pr_info("Err: status = %d\n", status);
		mutex_lock(&device_list_lock);
		list_del(&gf_dev->device_entry);
		device_destroy(gf_class, gf_dev->devt);
		clear_bit(MINOR(gf_dev->devt), minors);
		mutex_unlock(&device_list_lock);
#ifdef USE_SPI_BUS
gfspi_probe_clk_enable_failed:
		gfspi_ioctl_clk_uninit(gf_dev);
gfspi_probe_clk_init_failed:
#else
		if (gf_dev->iface_clk != NULL)
			clk_put(gf_dev->iface_clk);

		if (gf_dev->core_clk != NULL)
			clk_put(gf_dev->core_clk);

#endif
		if (gf_dev->input != NULL)
			input_unregister_device(gf_dev->input);


	}
	if (gf_dev->gBuffer != NULL) {
		kfree(gf_dev->gBuffer);
	}

	FUNC_EXIT();
	return status;
}

/*static int __devexit gf_remove(struct spi_device *spi)*/
#if defined(USE_SPI_BUS)
static int gf_remove(struct spi_device *spi)
#elif defined(USE_PLATFORM_BUS)
static int gf_remove(struct platform_device *pdev)
#endif
{
	struct gf_dev *gf_dev = &gf;
	FUNC_ENTRY();

	wake_lock_destroy(&fp_wakelock);
	/* make sure ops on existing fds can abort cleanly */
	if (gf_dev->irq)
		free_irq(gf_dev->irq, gf_dev);

	if (gf_dev->input != NULL)
		input_unregister_device(gf_dev->input);
	input_free_device(gf_dev->input);

	/* prevent new opens */
	mutex_lock(&device_list_lock);
	list_del(&gf_dev->device_entry);
	device_destroy(gf_class, gf_dev->devt);
	clear_bit(MINOR(gf_dev->devt), minors);

	gf_cleanup(gf_dev);
	gf_dev->device_available = 0;
	fb_unregister_client(&gf_dev->notifier);

#ifdef USE_SPI_BUS
	gfspi_ioctl_clk_uninit(gf_dev);
#else
	if (gf_dev->iface_clk != NULL)
		clk_put(gf_dev->iface_clk);

	if (gf_dev->core_clk != NULL)
		clk_put(gf_dev->core_clk);

#endif


	if (gf_dev->users == 0) {
		if (gf_dev->gBuffer)
			kfree(gf_dev->gBuffer);
	} else {
		pr_info("Not free_pages.\n");
	}

	mutex_unlock(&device_list_lock);

	FUNC_EXIT();
	return 0;
}

static struct of_device_id gx_match_table[] = {
	{.compatible = GF_SPIDEV_NAME,},
	{},
};

#if defined(USE_SPI_BUS)
static struct spi_driver gf_driver = {
#elif defined(USE_PLATFORM_BUS)
	static struct platform_driver gf_driver = {
#endif
		.driver = {
			.name = GF_DEV_NAME,
			.owner = THIS_MODULE,
#if defined(USE_SPI_BUS)

#endif
			.of_match_table = gx_match_table,
		},
		.probe = gf_probe,
		.remove = gf_remove,


	};

static int __init gf_init(void)
{
	int status;
	FUNC_ENTRY();

	/* Claim our 256 reserved device numbers.  Then register a class
	 * that will key udev/mdev to add/remove /dev nodes.  Last, register
	 * the driver which manages those device numbers.
	 */

	BUILD_BUG_ON(N_SPI_MINORS > 256);
	status = register_chrdev(SPIDEV_MAJOR, CHRD_DRIVER_NAME, &gf_fops);
	pr_info("SPIDEV_MAJOR = %d\n", SPIDEV_MAJOR);
	if (status < 0) {
		pr_warn("Failed to register char device!\n");
		FUNC_EXIT();
		return status;
	}
	gf_class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(gf_class)) {
		unregister_chrdev(SPIDEV_MAJOR, gf_driver.driver.name);
		pr_warn("Failed to create class.\n");
		FUNC_EXIT();
		return PTR_ERR(gf_class);
	}
#if defined(USE_PLATFORM_BUS)
	status = platform_driver_register(&gf_driver);
#elif defined(USE_SPI_BUS)
	status = spi_register_driver(&gf_driver);
#endif
	if (status < 0) {
		class_destroy(gf_class);
		unregister_chrdev(SPIDEV_MAJOR, gf_driver.driver.name);
		pr_warn("Failed to register SPI driver.\n");
	}

	pr_info(" status = 0x%x\n", status);
	FUNC_EXIT();
	return 0;
}

module_init(gf_init);

static void __exit gf_exit(void)
{
	FUNC_ENTRY();
#if defined(USE_PLATFORM_BUS)
	platform_driver_unregister(&gf_driver);
#elif defined(USE_SPI_BUS)
	spi_unregister_driver(&gf_driver);
#endif
	class_destroy(gf_class);
	unregister_chrdev(SPIDEV_MAJOR, gf_driver.driver.name);
	FUNC_EXIT();
}

module_exit(gf_exit);

MODULE_DESCRIPTION("User mode SPI device interface");
MODULE_LICENSE("GPL");
MODULE_ALIAS("spi:gf-spi");
