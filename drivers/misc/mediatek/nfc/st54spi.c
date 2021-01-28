/*
 * Simple synchronous userspace interface to SPI devices
 *
 * Copyright (C) 2006 SWAPP
 *	Andrea Paterniani <a.paterniani@swapp-eng.it>
 * Copyright (C) 2007 David Brownell (simplification, cleanup)
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
/*
 * Modified by ST Microelectronics.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/compat.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/acpi.h>
#include <linux/pinctrl/consumer.h>

#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>

#include <linux/gpio.h>
#include <linux/of_gpio.h>

#include <linux/uaccess.h>

#define ST21NFCD_MTK 1
// #define WITH_SPI_CLK_MNGT 1

#ifdef ST21NFCD_MTK
#include <linux/platform_data/spi-mt65xx.h>
#endif // ST21NFCD_MTK

#include "st21nfc/st21nfc.h"

// #ifdef ST21NFCD_MTK
/*
 * Define WITH_SPI_CLK_MNGT for integrations
 * where the SPI clock needs to be enabled on request
 */
// #define WITH_SPI_CLK_MNGT 1
// #ifdef WITH_SPI_CLK_MNGT
// extern void mt_spi_enable_master_clk(struct spi_device *spidev);
// extern void mt_spi_disable_master_clk(struct spi_device *spidev);
// #endif  // WITH_SPI_CLK_MNGT
// #endif //ST21NFCD_MTK

/*
 * This supports access to SPI devices using normal userspace I/O calls.
 * Note that while traditional UNIX/POSIX I/O semantics are half duplex,
 * and often mask message boundaries, full SPI support requires full duplex
 * transfers.  There are several kinds of internal message boundaries to
 * handle chipselect management and other protocol options.
 *
 * SPI has a character major number assigned.  We allocate minor numbers
 * dynamically using a bitmask.  You must use hotplug tools, such as udev
 * (or mdev with busybox) to create and destroy the /dev/st54spi device
 * nodes, since there is no fixed association of minor numbers with any
 * particular SPI bus or device.
 */
//#define SPIDEV_MAJOR			0	/* dynamic */
static int spidev_major;
#define N_SPI_MINORS 1 /* ... up to 256 */

static DECLARE_BITMAP(minors, N_SPI_MINORS);

#define ST54SPI_IOC_RD_POWER _IOR(SPI_IOC_MAGIC, 99, __u32)
#define ST54SPI_IOC_WR_POWER _IOW(SPI_IOC_MAGIC, 99, __u32)

/* Bit masks for spi_device.mode management.  Note that incorrect
 * settings for some settings can cause *lots* of trouble for other
 * devices on a shared bus:
 *
 *  - CS_HIGH ... this device will be active when it shouldn't be
 *  - 3WIRE ... when active, it won't behave as it should
 *  - NO_CS ... there will be no explicit message boundaries; this
 *	is completely incompatible with the shared bus model
 *  - READY ... transfers may proceed when they shouldn't.
 *
 * REVISIT should changing those flags be privileged?
 */
#define SPI_MODE_MASK				\
	(SPI_CPHA | SPI_CPOL | SPI_CS_HIGH |	\
	SPI_LSB_FIRST | SPI_3WIRE | SPI_LOOP |	\
	SPI_NO_CS | SPI_READY | SPI_TX_DUAL |	\
	SPI_TX_QUAD | SPI_RX_DUAL | SPI_RX_QUAD)

struct st54spi_data {
	dev_t devt;
	spinlock_t spi_lock;
	struct spi_device *spi;
	struct spi_device *spi_reset;
	struct list_head device_entry;

	/* TX/RX buffers are NULL unless this device is open (users > 0) */
	struct mutex buf_lock;
	unsigned int users;
	u8 *tx_buffer;
	u8 *rx_buffer;
	u32 speed_hz;

	/* GPIO for SE_POWER_REQ / SE_nRESET */
	int power_or_nreset_gpio_mode;
	int power_or_nreset_gpio;
	int nfcc_needs_poweron;
	int sehal_needs_poweron;
	int se_is_poweron;
	struct pinctrl *pctrl;
	struct pinctrl_state *pctrl_mode_spi, *pctrl_mode_idle;
};

#define POWER_MODE_NONE -1
#define POWER_MODE_ST54H 0
#define POWER_MODE_ST54J 1

static LIST_HEAD(device_list);
static DEFINE_MUTEX(device_list_lock);

static unsigned int bufsiz = 4096;
module_param(bufsiz, uint, 0444);
MODULE_PARM_DESC(bufsiz, "data bytes in biggest supported SPI message");

static bool debug_enabled = true;
#define VERBOSE 1

/*-------------------------------------------------------------------------*/

static ssize_t st54spi_sync(
	struct st54spi_data *st54spi, struct spi_message *message)
{
	DECLARE_COMPLETION_ONSTACK(done);
	int status;
	struct spi_device *spi;

	spin_lock_irq(&st54spi->spi_lock);
	spi = st54spi->spi;
	spin_unlock_irq(&st54spi->spi_lock);

	if (spi == NULL)
		status = -ESHUTDOWN;
	else
		status = spi_sync(spi, message);

	if (status == 0)
		status = message->actual_length;

	return status;
}

static inline ssize_t st54spi_sync_write(
	struct st54spi_data *st54spi, size_t len)
{
	struct spi_transfer t = {
		.tx_buf = st54spi->tx_buffer,
		.len = len,
		.speed_hz = st54spi->speed_hz,
	};
	struct spi_message m;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	return st54spi_sync(st54spi, &m);
}

static inline ssize_t st54spi_sync_read(
	struct st54spi_data *st54spi, size_t len)
{
	struct spi_transfer t = {
		.rx_buf = st54spi->rx_buffer,
		.len = len,
		.speed_hz = st54spi->speed_hz,
	};
	struct spi_message m;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	return st54spi_sync(st54spi, &m);
}

/*-------------------------------------------------------------------------*/

/* Read-only message with current device setup */
static ssize_t st54spi_read(
	struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	struct st54spi_data *st54spi;
	ssize_t status = 0;

	/* chipselect only toggles at start or end of operation */
	if (count > bufsiz)
		return -EMSGSIZE;

	st54spi = filp->private_data;

	if (debug_enabled)
		dev_info(&st54spi->spi->dev, "st54spi Read: %d bytes\n", count);

	mutex_lock(&st54spi->buf_lock);
	status = st54spi_sync_read(st54spi, count);
	if (status > 0) {
		unsigned long missing;

		missing = copy_to_user(buf, st54spi->rx_buffer, status);
		if (missing == status)
			status = -EFAULT;
		else
			status = status - missing;
	}
	mutex_unlock(&st54spi->buf_lock);

	if (debug_enabled)
		dev_info(&st54spi->spi->dev, "st54spi Read: status: %d\n", status);

	return status;
}

/* Write-only message with current device setup */
static ssize_t st54spi_write(
	struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	struct st54spi_data *st54spi;
	ssize_t status = 0;
	unsigned long missing;

	/* chipselect only toggles at start or end of operation */
	if (count > bufsiz)
		return -EMSGSIZE;

	st54spi = filp->private_data;

	if (debug_enabled)
		dev_info(&st54spi->spi->dev, "st54spi Write: %d bytes\n", count);

	mutex_lock(&st54spi->buf_lock);
	missing = copy_from_user(st54spi->tx_buffer, buf, count);
	if (missing == 0)
		status = st54spi_sync_write(st54spi, count);
	else
		status = -EFAULT;
	mutex_unlock(&st54spi->buf_lock);

	if (debug_enabled)
		dev_info(&st54spi->spi->dev, "st54spi Write: status: %d\n", status);

	return status;
}

static int st54spi_message(
	struct st54spi_data *st54spi,
	struct spi_ioc_transfer *u_xfers,
	unsigned int n_xfers)
{
	struct spi_message msg;
	struct spi_transfer *k_xfers;
	struct spi_transfer *k_tmp;
	struct spi_ioc_transfer *u_tmp;
	unsigned int n, total, tx_total, rx_total;
	u8 *tx_buf, *rx_buf;
	int status = -EFAULT;

	spi_message_init(&msg);
	k_xfers = kcalloc(n_xfers, sizeof(*k_tmp), GFP_KERNEL);
	if (k_xfers == NULL)
		return -ENOMEM;

	/* Construct spi_message, copying any tx data to bounce buffer.
	 * We walk the array of user-provided transfers, using each one
	 * to initialize a kernel version of the same transfer.
	 */
	tx_buf = st54spi->tx_buffer;
	rx_buf = st54spi->rx_buffer;
	total = 0;
	tx_total = 0;
	rx_total = 0;
	for (n = n_xfers, k_tmp = k_xfers, u_tmp = u_xfers;
		n; n--, k_tmp++, u_tmp++) {
		k_tmp->len = u_tmp->len;

		total += k_tmp->len;
		/* Since the function returns the total length of transfers
		 * on success, restrict the total to positive int values to
		 * avoid the return value looking like an error.  Also check
		 * each transfer length to avoid arithmetic overflow.
		 */
		if (total > INT_MAX || k_tmp->len > INT_MAX) {
			status = -EMSGSIZE;
			goto done;
		}

		if (u_tmp->rx_buf) {
			/* this transfer needs space in RX bounce buffer */
			rx_total += k_tmp->len;
			if (rx_total > bufsiz) {
				status = -EMSGSIZE;
				goto done;
			}
			k_tmp->rx_buf = rx_buf;
			if (!access_ok(VERIFY_WRITE,
				(u8 __user *)(uintptr_t)u_tmp->rx_buf,
				u_tmp->len))
				goto done;
			rx_buf += k_tmp->len;
		}
		if (u_tmp->tx_buf) {
			/* this transfer needs space in TX bounce buffer */
			tx_total += k_tmp->len;
			if (tx_total > bufsiz) {
				status = -EMSGSIZE;
				goto done;
			}
			k_tmp->tx_buf = tx_buf;
			if (copy_from_user(tx_buf,
				(const u8 __user *)(uintptr_t)u_tmp->tx_buf,
				u_tmp->len))
				goto done;
			tx_buf += k_tmp->len;
		}

		k_tmp->cs_change = !!u_tmp->cs_change;
		k_tmp->tx_nbits = u_tmp->tx_nbits;
		k_tmp->rx_nbits = u_tmp->rx_nbits;
		k_tmp->bits_per_word = u_tmp->bits_per_word;
		k_tmp->delay_usecs = u_tmp->delay_usecs;
		k_tmp->speed_hz = u_tmp->speed_hz;
		if (!k_tmp->speed_hz)
			k_tmp->speed_hz = st54spi->speed_hz;
#ifdef VERBOSE
		dev_dbg(&st54spi->spi->dev, "  xfer len %u %s%s%s%dbits %u usec %uHz\n",
			u_tmp->len, u_tmp->rx_buf ? "rx " : "", u_tmp->tx_buf ? "tx " : "",
			u_tmp->cs_change ? "cs " : "",
			u_tmp->bits_per_word ?: st54spi->spi->bits_per_word,
			u_tmp->delay_usecs, u_tmp->speed_hz ?: st54spi->spi->max_speed_hz);
#endif
		spi_message_add_tail(k_tmp, &msg);
	}

	status = st54spi_sync(st54spi, &msg);
	if (status < 0)
		goto done;

	/* copy any rx data out of bounce buffer */
	rx_buf = st54spi->rx_buffer;
	for (n = n_xfers, u_tmp = u_xfers; n; n--, u_tmp++) {
		if (u_tmp->rx_buf) {
			if (__copy_to_user(
				(u8 __user *)(uintptr_t)u_tmp->rx_buf,
				rx_buf, u_tmp->len)) {
				status = -EFAULT;
				goto done;
			}
			rx_buf += u_tmp->len;
		}
	}
	status = total;

done:
	kfree(k_xfers);
	return status;
}

static struct spi_ioc_transfer *st54spi_get_ioc_message(
	unsigned int cmd,
	struct spi_ioc_transfer __user *u_ioc,
	unsigned int *n_ioc)
{
	struct spi_ioc_transfer *ioc;
	u32 tmp;

	/* Check type, command number and direction */
	if (_IOC_TYPE(cmd) != SPI_IOC_MAGIC ||
		_IOC_NR(cmd) != _IOC_NR(SPI_IOC_MESSAGE(0)) ||
		_IOC_DIR(cmd) != _IOC_WRITE)
		return ERR_PTR(-ENOTTY);

	tmp = _IOC_SIZE(cmd);
	if ((tmp % sizeof(struct spi_ioc_transfer)) != 0)
		return ERR_PTR(-EINVAL);

	*n_ioc = tmp / sizeof(struct spi_ioc_transfer);
	if (*n_ioc == 0)
		return NULL;

	/* copy into scratch area */
	ioc = kmalloc(tmp, GFP_KERNEL);
	if (!ioc)
		return ERR_PTR(-ENOMEM);

	if (__copy_from_user(ioc, u_ioc, tmp)) {
		kfree(ioc);
		return ERR_PTR(-EFAULT);
	}
	return ioc;
}

static void st54spi_power_off(struct st54spi_data *st54spi)
{
	int ret;

	if (debug_enabled)
		dev_info(&st54spi->spi->dev, "%s\n", __func__);

	// Set NSS pin as highZ (ST54H and ST54J).

	// Change NSS polarity to have NSS low.
	ret = pinctrl_select_state(st54spi->pctrl, st54spi->pctrl_mode_idle);

	if (ret < 0) {
		dev_info(&st54spi->spi->dev,
				"%s : change NSS management to High Z failed!\n", __func__);
	}

	// Set SE_PWR_REQ / SE_nRESET to low
	if (st54spi->power_or_nreset_gpio)
		gpio_set_value(st54spi->power_or_nreset_gpio, 0);

	// if ST54H block access to SPI in case this is done during a CLF reset
	if (st54spi->power_or_nreset_gpio_mode == POWER_MODE_ST54H) {
		// disallow access to SPI r/w
		if (st54spi->spi) {
			spin_lock_irq(&st54spi->spi_lock);
			st54spi->spi_reset = st54spi->spi;
			st54spi->spi = NULL;
			spin_unlock_irq(&st54spi->spi_lock);
		}

		// Give time to the CLF to detect falling SE_PWR_REQ
		// and pull down the line before continue.
		usleep_range(2000, 4500);
	}

#ifdef WITH_SPI_CLK_MNGT
	// no need for the SPI clock to be enabled.
	dev_info(&st54spi->spi->dev, "%s : disabling PMU clock of SPI subsystem\n", __func__);
	mt_spi_disable_master_clk(st54spi->spi);
#endif  // WITH_SPI_CLK_MNGT

	st54spi->se_is_poweron = 0;
}

static void st54spi_power_on(struct st54spi_data *st54spi)
{
	int ret;

	if (debug_enabled)
		dev_info(&st54spi->spi->dev, "%s\n", __func__);
#ifdef WITH_SPI_CLK_MNGT
	// the SPI clock needs to be enabled.
	dev_info(&st54spi->spi->dev, "%s : enabling PMU clock of SPI subsystem\n", __func__);
	mt_spi_enable_master_clk(st54spi->spi);
#endif  // WITH_SPI_CLK_MNGT

	// set SE_PWR_REQ / SE_nRESET to high and wait for CLF + eSE reaction
	if (st54spi->power_or_nreset_gpio) {
		gpio_set_value(st54spi->power_or_nreset_gpio, 1);
		usleep_range(1000, 1500);
	}

	// Set NSS pin for the SPI function.
	ret = pinctrl_select_state(st54spi->pctrl, st54spi->pctrl_mode_spi);

	if (ret < 0) {
		dev_info(&st54spi->spi->dev,
				"%s : change NSS management to SPI failed!\n", __func__);
	}

	usleep_range(4000, 5000);

	if (st54spi->power_or_nreset_gpio_mode == POWER_MODE_ST54H) {
		// re-allow SPI xfers
		if (st54spi->spi_reset) {
			spin_lock_irq(&st54spi->spi_lock);
			st54spi->spi = st54spi->spi_reset;
			st54spi->spi_reset = NULL;
			spin_unlock_irq(&st54spi->spi_lock);
		}
	}

	st54spi->se_is_poweron = 1;
}

static void st54spi_power_set(struct st54spi_data *st54spi, int val)
{
	if (!st54spi)
		return;

	if (debug_enabled)
		dev_info(&st54spi->spi->dev, "st54spi sehal pwr_req: %d\n", val);

	if (val) {
		st54spi->sehal_needs_poweron = 1;
		if (st54spi->se_is_poweron == 0)
			st54spi_power_on(st54spi);
	} else {
		st54spi->sehal_needs_poweron = 0;
		if ((st54spi->se_is_poweron == 1) &&
			(st54spi->nfcc_needs_poweron == 0))
			// we don t need power anymore
			st54spi_power_off(st54spi);
	}
}

static int st54spi_power_get(struct st54spi_data *st54spi)
{
	if (st54spi->power_or_nreset_gpio)
		return gpio_get_value(st54spi->power_or_nreset_gpio);
	return 0;
}

static long st54spi_ioctl(
	struct file *filp, unsigned int cmd, unsigned long arg)
{
	int err = 0;
	int retval = 0;
	struct st54spi_data *st54spi;
	struct spi_device *spi;
	u32 tmp;
	unsigned int n_ioc;
	struct spi_ioc_transfer *ioc;

	/* Check type and command number */
	if (_IOC_TYPE(cmd) != SPI_IOC_MAGIC)
		return -ENOTTY;

	/* Check access direction once here; don't repeat below.
	 * IOC_DIR is from the user perspective, while access_ok is
	 * from the kernel perspective; so they look reversed.
	 */
	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE,
			(void __user *)arg, _IOC_SIZE(cmd));
	if (err == 0 && _IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ,
			(void __user *)arg, _IOC_SIZE(cmd));
	if (err)
		return -EFAULT;

    /* guard against device removal before, or while,
     * we issue this ioctl.
     */
	st54spi = filp->private_data;
	spin_lock_irq(&st54spi->spi_lock);
	spi = spi_dev_get(st54spi->spi);
	spin_unlock_irq(&st54spi->spi_lock);

	if (debug_enabled)
		dev_info(&st54spi->spi->dev, "st54spi ioctl cmd %d\n", cmd);

	if (spi == NULL)
		return -ESHUTDOWN;

    /*  use the buffer lock here for triple duty:
     *  - prevent I/O (from us) so calling spi_setup() is safe;
     *  - prevent concurrent SPI_IOC_WR_* from morphing
     *    data fields while SPI_IOC_RD_* reads them;
     *  - SPI_IOC_MESSAGE needs the buffer locked "normally".
     */
	mutex_lock(&st54spi->buf_lock);

	switch (cmd) {
    /* read requests */
	case SPI_IOC_RD_MODE:
		retval = __put_user(
			spi->mode & SPI_MODE_MASK, (__u8 __user *)arg);
		break;
	case SPI_IOC_RD_MODE32:
		retval = __put_user(
			spi->mode & SPI_MODE_MASK, (__u32 __user *)arg);
		break;
	case SPI_IOC_RD_LSB_FIRST:
		retval = __put_user(
			(spi->mode & SPI_LSB_FIRST) ?
				1 : 0, (__u8 __user *)arg);
		break;
	case SPI_IOC_RD_BITS_PER_WORD:
		retval = __put_user(
			spi->bits_per_word, (__u8 __user *)arg);
		break;
	case SPI_IOC_RD_MAX_SPEED_HZ:
		retval = __put_user(
			st54spi->speed_hz, (__u32 __user *)arg);
		break;
	case ST54SPI_IOC_RD_POWER:
		retval = __put_user(
			st54spi_power_get(st54spi), (__u32 __user *)arg);
		break;

    /* write requests */
	case SPI_IOC_WR_MODE:
	case SPI_IOC_WR_MODE32:
		if (cmd == SPI_IOC_WR_MODE)
			retval = __get_user(tmp, (u8 __user *)arg);
		else
			retval = __get_user(tmp, (u32 __user *)arg);
		if (retval == 0) {
			u32 save = spi->mode;

			if (tmp & ~SPI_MODE_MASK) {
				retval = -EINVAL;
				break;
			}

			tmp |= spi->mode & ~SPI_MODE_MASK;
			spi->mode = (u16)tmp;
			retval = spi_setup(spi);
			if (retval < 0)
				spi->mode = save;
			else
				dev_dbg(&spi->dev, "spi mode %x\n", tmp);
		}
		break;
	case SPI_IOC_WR_LSB_FIRST:
		retval = __get_user(tmp, (__u8 __user *)arg);
		if (retval == 0) {
			u32 save = spi->mode;

			if (tmp)
				spi->mode |= SPI_LSB_FIRST;
			else
				spi->mode &= ~SPI_LSB_FIRST;
			retval = spi_setup(spi);
			if (retval < 0)
				spi->mode = save;
			else
				dev_dbg(&spi->dev,
					"%csb first\n", tmp ?
					'l' : 'm');
		}
		break;
	case SPI_IOC_WR_BITS_PER_WORD:
		retval = __get_user(tmp, (__u8 __user *)arg);
		if (retval == 0) {
			u8 save = spi->bits_per_word;

			spi->bits_per_word = tmp;
			retval = spi_setup(spi);
			if (retval < 0)
				spi->bits_per_word = save;
			else
				dev_dbg(&spi->dev, "%d bits per word\n", tmp);
		}
		break;
	case SPI_IOC_WR_MAX_SPEED_HZ:
		retval = __get_user(tmp, (__u32 __user *)arg);
		if (retval == 0) {
			u32 save = spi->max_speed_hz;

			spi->max_speed_hz = tmp;
			retval = spi_setup(spi);
			if (retval >= 0)
				st54spi->speed_hz = tmp;
			else
				dev_dbg(&spi->dev, "%d Hz (max)\n", tmp);
			spi->max_speed_hz = save;
		}
		break;
	case ST54SPI_IOC_WR_POWER:
		retval = __get_user(tmp, (__u32 __user *)arg);
		if (retval == 0) {
			st54spi_power_set(st54spi, tmp ? 1 : 0);
			dev_dbg(&spi->dev, "SE_POWER_REQ/SE_NRESET set: %d\n", tmp);
		}
		break;

	default:
		/* segmented and/or full-duplex I/O request */
		/* Check message and copy into scratch area */
		ioc = st54spi_get_ioc_message(
			cmd, (struct spi_ioc_transfer __user *)arg, &n_ioc);
		if (IS_ERR(ioc)) {
			retval = PTR_ERR(ioc);
			break;
		}
		if (!ioc)
			break; /* n_ioc is also 0 */

		/* translate to spi_message, execute */
		retval = st54spi_message(st54spi, ioc, n_ioc);
		kfree(ioc);
		break;
	}

	mutex_unlock(&st54spi->buf_lock);
	spi_dev_put(spi);

	if (debug_enabled)
		dev_info(&spi->dev, "st54spi ioctl retval %d\n", retval);

	return retval;
}

#ifdef CONFIG_COMPAT
static long st54spi_compat_ioc_message(
	struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct spi_ioc_transfer __user *u_ioc;
	int retval = 0;
	struct st54spi_data *st54spi;
	struct spi_device *spi;
	unsigned int n_ioc, n;
	struct spi_ioc_transfer *ioc;

	u_ioc = (struct spi_ioc_transfer __user *)compat_ptr(arg);
	if (!access_ok(VERIFY_READ, u_ioc, _IOC_SIZE(cmd)))
		return -EFAULT;

    /* guard against device removal before, or while,
     * we issue this ioctl.
     */
	st54spi = filp->private_data;
	spin_lock_irq(&st54spi->spi_lock);
	spi = spi_dev_get(st54spi->spi);
	spin_unlock_irq(&st54spi->spi_lock);

	if (debug_enabled)
		dev_info(&st54spi->spi->dev, "st54spi compat_ioctl cmd %d\n", cmd);
	if (spi == NULL)
		return -ESHUTDOWN;

	/* SPI_IOC_MESSAGE needs the buffer locked "normally" */
	mutex_lock(&st54spi->buf_lock);

	/* Check message and copy into scratch area */
	ioc = st54spi_get_ioc_message(cmd, u_ioc, &n_ioc);
	if (IS_ERR(ioc)) {
		retval = PTR_ERR(ioc);
		goto done;
	}
	if (!ioc)
		goto done; /* n_ioc is also 0 */

	/* Convert buffer pointers */
	for (n = 0; n < n_ioc; n++) {
		ioc[n].rx_buf = (uintptr_t)compat_ptr(ioc[n].rx_buf);
		ioc[n].tx_buf = (uintptr_t)compat_ptr(ioc[n].tx_buf);
	}

	/* translate to spi_message, execute */
	retval = st54spi_message(st54spi, ioc, n_ioc);
	kfree(ioc);

done:
	mutex_unlock(&st54spi->buf_lock);
	spi_dev_put(spi);
	if (debug_enabled)
		dev_info(&st54spi->spi->dev, "st54spi compat_ioctl retval %d\n", retval);
	return retval;
}

static long st54spi_compat_ioctl(
	struct file *filp, unsigned int cmd, unsigned long arg)
{
	if (_IOC_TYPE(cmd) == SPI_IOC_MAGIC &&
		_IOC_NR(cmd) == _IOC_NR(SPI_IOC_MESSAGE(0)) &&
		_IOC_DIR(cmd) == _IOC_WRITE)
		return st54spi_compat_ioc_message(filp, cmd, arg);

	return st54spi_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
}
#else
#define st54spi_compat_ioctl NULL
#endif /* CONFIG_COMPAT */

static int st54spi_open(struct inode *inode, struct file *filp)
{
	struct st54spi_data *st54spi;
	int status = -ENXIO;

	mutex_lock(&device_list_lock);

	list_for_each_entry(st54spi, &device_list, device_entry) {
		if (st54spi->devt == inode->i_rdev) {
			status = 0;
			break;
		}
	}

	if (status) {
		dev_dbg(&st54spi->spi->dev, "st54spi: nothing for minor %d\n", iminor(inode));
		goto err_find_dev;
	}

	// Authorize only 1 process to open the device.
	if (st54spi->users > 0) {
		dev_info(&st54spi->spi->dev, "%d: already open\n");
		mutex_unlock(&device_list_lock);
		return -EBUSY;
	}

	if (debug_enabled)
		dev_info(&st54spi->spi->dev, "st54spi: open\n");

	if (!st54spi->tx_buffer) {
		st54spi->tx_buffer = kmalloc(bufsiz, GFP_KERNEL);
		if (!st54spi->tx_buffer) {
			// dev_dbg(&st54spi->spi->dev, "open/ENOMEM\n");
			status = -ENOMEM;
			goto err_find_dev;
		}
	}

	if (!st54spi->rx_buffer) {
		st54spi->rx_buffer = kmalloc(bufsiz, GFP_KERNEL);
		if (!st54spi->rx_buffer) {
			// dev_dbg(&st54spi->spi->dev, "open/ENOMEM\n");
			status = -ENOMEM;
			goto err_alloc_rx_buf;
		}
	}

	st54spi->users++;
	filp->private_data = st54spi;
	nonseekable_open(inode, filp);

	mutex_unlock(&device_list_lock);

	if (debug_enabled)
		dev_info(&st54spi->spi->dev, "st54spi: open - force power on\n");
	st54spi_power_set(st54spi, 1);
	return 0;

err_alloc_rx_buf:
	kfree(st54spi->tx_buffer);
	st54spi->tx_buffer = NULL;
err_find_dev:
	mutex_unlock(&device_list_lock);
	return status;
}

static int st54spi_release(struct inode *inode, struct file *filp)
{
	struct st54spi_data *st54spi;

	mutex_lock(&device_list_lock);
	st54spi = filp->private_data;
	filp->private_data = NULL;

	if (debug_enabled)
		dev_info(&st54spi->spi->dev, "st54spi: release\n");

	/* last close? */
	st54spi->users--;
	if (!st54spi->users) {
		int dofree;

		if (debug_enabled)
			dev_info(&st54spi->spi->dev, "st54spi: release - may allow power off\n");

		st54spi_power_set(st54spi, 0);

		kfree(st54spi->tx_buffer);
		st54spi->tx_buffer = NULL;

		kfree(st54spi->rx_buffer);
		st54spi->rx_buffer = NULL;

		spin_lock_irq(&st54spi->spi_lock);
		if (st54spi->spi)
			st54spi->speed_hz = st54spi->spi->max_speed_hz;

		/* ... after we unbound from the underlying device? */
		dofree = ((st54spi->spi == NULL) &&
			(st54spi->spi_reset == NULL));
		spin_unlock_irq(&st54spi->spi_lock);

		if (dofree)
			kfree(st54spi);
	}
	mutex_unlock(&device_list_lock);

	return 0;
}

static const struct file_operations st54spi_fops = {
	.owner = THIS_MODULE,
    /* REVISIT switch to aio primitives, so that userspace
     * gets more complete API coverage.  It'll simplify things
     * too, except for the locking.
     */
	.write = st54spi_write,
	.read = st54spi_read,
	.unlocked_ioctl = st54spi_ioctl,
	.compat_ioctl = st54spi_compat_ioctl,
	.open = st54spi_open,
	.release = st54spi_release,
	.llseek = no_llseek,
};

/*-------------------------------------------------------------------------*/

/* The main reason to have this class is to make mdev/udev create the
 * /dev/st54spi character device nodes exposing our userspace API.
 * It also simplifies memory management.
 */

static struct class *st54spi_class;

static const struct of_device_id st54spi_dt_ids[] = {
	{.compatible = "st,st54spi"},
	{},
};
MODULE_DEVICE_TABLE(of, st54spi_dt_ids);

#ifdef CONFIG_ACPI

/* Dummy SPI devices not to be used in production systems */
#define SPIDEV_ACPI_DUMMY 1

static const struct acpi_device_id st54spi_acpi_ids[] = {
    /*
     * The ACPI SPT000* devices are only meant for development and
     * testing. Systems used in production should have a proper ACPI
     * description of the connected peripheral and they should also use
     * a proper driver instead of poking directly to the SPI bus.
     */
	{"SPT0001", SPIDEV_ACPI_DUMMY},
	{"SPT0002", SPIDEV_ACPI_DUMMY},
	{"SPT0003", SPIDEV_ACPI_DUMMY},
	{},
};
MODULE_DEVICE_TABLE(acpi, st54spi_acpi_ids);

static void st54spi_probe_acpi(struct spi_device *spi)
{
	const struct acpi_device_id *id;

	if (!has_acpi_companion(&spi->dev))
		return;

	id = acpi_match_device(st54spi_acpi_ids, &spi->dev);
	if (WARN_ON(!id))
		return;
}
#else
static inline void st54spi_probe_acpi(struct spi_device *spi) {}
#endif

/*-------------------------------------------------------------------------*/

static int st54spi_parse_dt(struct device *dev, struct st54spi_data *pdata)
{
	int r = 0;
	struct device_node *np = dev->of_node;

	np = of_find_compatible_node(NULL, NULL, "st,st54spi");

	if (np) {
		const char *power_mode;
#if (!defined(CONFIG_MTK_GPIO) || defined(CONFIG_MTK_GPIOLIB_STAND))
		r = of_get_named_gpio(np, "gpio-power_nreset-std", 0);
		if (r < 0)
			dev_info(dev, "%s: get ST54 failed (%d)", __FILE__, r);
		else
			pdata->power_or_nreset_gpio = r;
		r = 0;
#else
		of_property_read_u32_array(
			np, "gpio-power_nreset", &(pdata->power_or_nreset_gpio), 1);
#endif

		// Read power mode.
		power_mode = of_get_property(np, "power_mode", NULL);
		if (!power_mode) {
			dev_info(dev, "%s: Default power mode: ST54H\n", __FILE__);
			pdata->power_or_nreset_gpio_mode = POWER_MODE_ST54H;
		} else if (!strcmp(power_mode, "ST54J")) {
			dev_info(dev, "%s: Power mode: ST54J\n", __FILE__);
			pdata->power_or_nreset_gpio_mode = POWER_MODE_ST54J;
		} else if (!strcmp(power_mode, "ST54H")) {
			dev_info(dev, "%s: Power mode: ST54H\n", __FILE__);
			pdata->power_or_nreset_gpio_mode = POWER_MODE_ST54H;
		} else if (!strcmp(power_mode, "none")) {
			dev_info(dev, "%s: Power mode: none\n", __FILE__);
			pdata->power_or_nreset_gpio_mode = POWER_MODE_NONE;
		} else {
			dev_info(dev, "%s: Power mode unknown: %s\n", __FILE__, power_mode);
			return -1;
		}
	} else {
		dev_info(dev, "%s : get num err.\n", __func__);
		return -1;
	}

	// We need to use pinmux to control NSS
	pdata->pctrl = devm_pinctrl_get(dev);
	if (IS_ERR(pdata->pctrl)) {
		dev_info(dev, "%s: Unable to allocate pinctrl: %d\n",
				__FILE__, PTR_ERR(pdata->pctrl));
		return -1;
	}

	pdata->pctrl_mode_spi = pinctrl_lookup_state(pdata->pctrl, "pinctrl_state_mode_spi");
	if (IS_ERR(pdata->pctrl_mode_spi)) {
		dev_info(dev, "%s: Unable to find pinctrl_state_mode_spi: %d\n",
			__FILE__, PTR_ERR(pdata->pctrl_mode_spi));
		return -1;
	}

	pdata->pctrl_mode_idle = pinctrl_lookup_state(pdata->pctrl, "pinctrl_state_mode_idle");
	if (IS_ERR(pdata->pctrl_mode_idle)) {
		dev_info(dev, "%s: Unable to find pinctrl_state_mode_idle: %d\n",
			__FILE__, PTR_ERR(pdata->pctrl_mode_idle));
		return -1;
	}
	dev_info(dev, "[dsc]%s : pinctrl initialized\n", __func__);

	dev_info(dev, "[dsc]%s : get power_or_nreset_gpio[%d]\n",
				__func__, pdata->power_or_nreset_gpio);
	return r;
}

#ifndef MODULE
static void st54spi_st21nfc_cb(int dir, void *data)
{
	struct st54spi_data *st54spi = (struct st54spi_data *)data;

	if (!st54spi)
		return;
	dev_info(&st54spi->spi->dev, "%s : dir %d data %p\n", __func__, dir, st54spi);

	switch (dir) {
	case ST54SPI_CB_RESET_START:
		if (st54spi->se_is_poweron)
			st54spi_power_off(st54spi);
		break;

	case ST54SPI_CB_RESET_END:
		// wait for the CLF to boot once nRESET is released
		usleep_range(4000, 8000);

		st54spi_power_on(st54spi);
		break;

	case ST54SPI_CB_ESE_USED:
		st54spi->nfcc_needs_poweron = 1;
		if (st54spi->se_is_poweron == 0)
			st54spi_power_on(st54spi);
		break;

	case ST54SPI_CB_ESE_NOT_USED:
		st54spi->nfcc_needs_poweron = 0;
		if ((st54spi->se_is_poweron == 1) &&
			(st54spi->sehal_needs_poweron == 0))
			// we don t need power anymore
			st54spi_power_off(st54spi);
		break;
	}
}
#endif  // !MODULE

/* Change CS_TIME for ST54 */
#ifdef ST21NFCD_MTK
// Unit is 1/109.2 us.
static struct mtk_chip_config st54spi_chip_info = {
	.cs_setuptime = 2184, // 20 us
};
#endif

static int st54spi_probe(struct spi_device *spi)
{
	struct st54spi_data *st54spi;
	int status, ret;
	unsigned long minor;
#ifdef ST21NFCD_MTK
	struct mtk_chip_config *chip_config = spi->controller_data;
#endif

	/*
	 * st54spi should never be referenced in DT without a specific
	 * compatible string, it is a Linux implementation thing
	 * rather than a description of the hardware.
	 */

	st54spi_probe_acpi(spi);

	/* Allocate driver data */
	st54spi = kzalloc(sizeof(*st54spi), GFP_KERNEL);
	if (!st54spi)
		return -ENOMEM;

	/* Initialize the driver data */
	st54spi->spi = spi;
	spin_lock_init(&st54spi->spi_lock);
	mutex_init(&st54spi->buf_lock);

	INIT_LIST_HEAD(&st54spi->device_entry);

    /* If we can allocate a minor number, hook up this device.
     * Reusing minors is fine so long as udev or mdev is working.
     */
	mutex_lock(&device_list_lock);
	minor = find_first_zero_bit(minors, N_SPI_MINORS);
	if (minor < N_SPI_MINORS) {
		struct device *dev;

		st54spi->devt = MKDEV(spidev_major, minor);
		dev = device_create(st54spi_class, &spi->dev, st54spi->devt,
			// spidev, "spidev%d.%d",
			// spi->master->bus_num, spi->chip_select);
			st54spi, "st54spi");
		status = PTR_ERR_OR_ZERO(dev);
	} else {
		dev_dbg(&spi->dev, "no minor number available!\n");
		status = -ENODEV;
	}
	if (status == 0) {
		set_bit(minor, minors);
		list_add(&st54spi->device_entry, &device_list);
	}
	mutex_unlock(&device_list_lock);

	st54spi->speed_hz = spi->max_speed_hz;
	dev_dbg(&spi->dev, "st54spi->speed_hz=%d\n", st54spi->speed_hz);
	// {
	// /* fixed SPI clock speed: 109200000 */
	// int period = DIV_ROUND_UP(109200000, st54spi->speed_hz);

	// st54spi_chip_info.cs_idletime = period;
	// st54spi_chip_info.cs_holdtime = period;
	// }

#ifdef ST21NFCD_MTK
	// set timings for ST54
	if (chip_config == NULL) {
		spi->controller_data = (void *)&st54spi_chip_info;
		dev_dbg(&spi->dev, "Replaced chip_info!\n");
	} else {
		chip_config->cs_setuptime = st54spi_chip_info.cs_setuptime;
		chip_config->cs_idletime = st54spi_chip_info.cs_idletime;
		chip_config->cs_holdtime = st54spi_chip_info.cs_holdtime;
		dev_dbg(&spi->dev, "Added into chip_info!\n");
	}
#else
	dev_info(&spi->dev, "%s : TSU_NSS configuration be implemented!\n", __func__);
	// platform-specific method to configure the delay beween NSS slave
	// selection and the start of data transfer (clk).
	// If no specific method required, you can comment above line.
#endif

	if (status == 0)
		spi_set_drvdata(spi, st54spi);
	else
		kfree(st54spi);

	(void)st54spi_parse_dt(&spi->dev, st54spi);

	if (st54spi->power_or_nreset_gpio != 0) {
		int default_value = 0;


		ret = gpio_request(st54spi->power_or_nreset_gpio,
#if (!defined(CONFIG_MTK_GPIO) || defined(CONFIG_MTK_GPIOLIB_STAND))
			"gpio-power_nreset-std"
#else
			"gpio-power_nreset"
#endif
		);
		if (ret)
			dev_info(&spi->dev, "%s : power request failed (%d)\n",
				__FILE__, ret);

		dev_info(&spi->dev, "%s : power/nreset GPIO = %d\n", __func__,
			st54spi->power_or_nreset_gpio);
		ret = gpio_direction_output(st54spi->power_or_nreset_gpio, default_value);
		if (ret)
			dev_info(&spi->dev, "%s : reset direction_output failed\n", __FILE__);

		/* active high */
		gpio_set_value(st54spi->power_or_nreset_gpio, default_value);
	}

	if (st54spi->power_or_nreset_gpio_mode == POWER_MODE_ST54H) {
#ifndef MODULE
		dev_info(&spi->dev, "%s : Register with st21nfc driver, %p\n",
			__func__, st54spi);
		st21nfc_register_st54spi_cb(st54spi_st21nfc_cb, st54spi);
#else
		dev_info(&spi->dev, "%s : st54spi as module cannot use ST54H fully\n",
			__func__);
#endif
	}

	return status;
}

static int st54spi_remove(struct spi_device *spi)
{
	struct st54spi_data *st54spi = spi_get_drvdata(spi);

	if (st54spi->power_or_nreset_gpio_mode == POWER_MODE_ST54H) {
#ifndef MODULE
		dev_info(&st54spi->spi->dev, "%s : Unregister from st21nfc driver\n",
			__func__);
		st21nfc_unregister_st54spi_cb();
#endif
	}
    /* make sure ops on existing fds can abort cleanly */
	spin_lock_irq(&st54spi->spi_lock);
	st54spi->spi = NULL;
	st54spi->spi_reset = NULL;
	spin_unlock_irq(&st54spi->spi_lock);

	/* prevent new opens */
	mutex_lock(&device_list_lock);
	list_del(&st54spi->device_entry);
	device_destroy(st54spi_class, st54spi->devt);
	clear_bit(MINOR(st54spi->devt), minors);
	if (st54spi->users == 0)
		kfree(st54spi);

	mutex_unlock(&device_list_lock);

	return 0;
}

static struct spi_driver st54spi_spi_driver = {
	.driver = {
			.name = "st54spi",
			.of_match_table = of_match_ptr(st54spi_dt_ids),
			.acpi_match_table = ACPI_PTR(st54spi_acpi_ids),
		},
	.probe = st54spi_probe,
	.remove = st54spi_remove,

    /* NOTE:  suspend/resume methods are not necessary here.
     * We don't do anything except pass the requests to/from
     * the underlying controller.  The refrigerator handles
     * most issues; the controller driver handles the rest.
     */
};

/*-------------------------------------------------------------------------*/

static int __init st54spi_init(void)
{
	int status;

	pr_info("Loading st54spi driver\n");

	/* Claim our 256 reserved device numbers.  Then register a class
	 * that will key udev/mdev to add/remove /dev nodes.  Last, register
	 * the driver which manages those device numbers.
	 */
	BUILD_BUG_ON(N_SPI_MINORS > 256);
	spidev_major = __register_chrdev(0, 0, N_SPI_MINORS,
		"spi", &st54spi_fops);
	pr_info("Loading st54spi driver, major: %d\n", spidev_major);

	st54spi_class = class_create(THIS_MODULE, "spidev");
	if (IS_ERR(st54spi_class)) {
		unregister_chrdev(spidev_major, st54spi_spi_driver.driver.name);
		return PTR_ERR(st54spi_class);
	}

	status = spi_register_driver(&st54spi_spi_driver);
	if (status < 0) {
		class_destroy(st54spi_class);
		unregister_chrdev(spidev_major, st54spi_spi_driver.driver.name);
	}
	pr_info("Loading st54spi driver: %d\n", status);
	return status;
}
module_init(st54spi_init);

static void __exit st54spi_exit(void)
{
	spi_unregister_driver(&st54spi_spi_driver);
	class_destroy(st54spi_class);
	unregister_chrdev(spidev_major, st54spi_spi_driver.driver.name);
}
module_exit(st54spi_exit);

MODULE_AUTHOR("Andrea Paterniani, <a.paterniani@swapp-eng.it>");
MODULE_DESCRIPTION("User mode SPI device interface");
MODULE_LICENSE("GPL");
MODULE_ALIAS("spi:st54spi");
