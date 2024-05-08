// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Simple synchronous userspace interface to SPI devices
 *
 * Copyright (C) 2006 SWAPP
 *	Andrea Paterniani <a.paterniani@swapp-eng.it>
 * Copyright (C) 2007 David Brownell (simplification, cleanup)
 */

#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/property.h>
#include <linux/slab.h>
#include <linux/compat.h>

#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>

#include <linux/uaccess.h>

#if IS_ENABLED(CONFIG_BRCM_XGBE)
	#include <linux/delay.h>
	#include "spi_common.h"
#endif

/*
 * This supports access to SPI devices using normal userspace I/O calls.
 * Note that while traditional UNIX/POSIX I/O semantics are half duplex,
 * and often mask message boundaries, full SPI support requires full duplex
 * transfers.  There are several kinds of internal message boundaries to
 * handle chipselect management and other protocol options.
 *
 * SPI has a character major number assigned.  We allocate minor numbers
 * dynamically using a bitmask.  You must use hotplug tools, such as udev
 * (or mdev with busybox) to create and destroy the /dev/spidevB.C device
 * nodes, since there is no fixed association of minor numbers with any
 * particular SPI bus or device.
 */
#define SPIDEV_MAJOR			153	/* assigned */
#define N_SPI_MINORS			32	/* ... up to 256 */

static DECLARE_BITMAP(minors, N_SPI_MINORS);

static_assert(N_SPI_MINORS > 0 && N_SPI_MINORS <= 256);

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
#define SPI_MODE_MASK		(SPI_MODE_X_MASK | SPI_CS_HIGH \
				| SPI_LSB_FIRST | SPI_3WIRE | SPI_LOOP \
				| SPI_NO_CS | SPI_READY | SPI_TX_DUAL \
				| SPI_TX_QUAD | SPI_TX_OCTAL | SPI_RX_DUAL \
				| SPI_RX_QUAD | SPI_RX_OCTAL \
				| SPI_RX_CPHA_FLIP)

#if IS_ENABLED(CONFIG_BRCM_XGBE)
	static void acd_init_port0(void);
	static void acd_init_port3(void);
#endif

struct spidev_data {
	dev_t			devt;
	spinlock_t		spi_lock;
	struct spi_device	*spi;
	struct list_head	device_entry;

	/* TX/RX buffers are NULL unless this device is open (users > 0) */
	struct mutex		buf_lock;
	unsigned		users;
	u8			*tx_buffer;
	u8			*rx_buffer;
	u32			speed_hz;
};

static LIST_HEAD(device_list);
static DEFINE_MUTEX(device_list_lock);

static unsigned bufsiz = 4096;
module_param(bufsiz, uint, S_IRUGO);
MODULE_PARM_DESC(bufsiz, "data bytes in biggest supported SPI message");

#if IS_ENABLED(CONFIG_BRCM_XGBE)
	static struct spidev_data *g_spidev2 = NULL;
#endif

/*-------------------------------------------------------------------------*/

static ssize_t
spidev_sync(struct spidev_data *spidev, struct spi_message *message)
{
	int status;
	struct spi_device *spi;

	spin_lock_irq(&spidev->spi_lock);
	spi = spidev->spi;
	spin_unlock_irq(&spidev->spi_lock);

	if (spi == NULL)
		status = -ESHUTDOWN;
	else
		status = spi_sync(spi, message);

	if (status == 0)
		status = message->actual_length;

	return status;
}

static inline ssize_t
spidev_sync_write(struct spidev_data *spidev, size_t len)
{
	struct spi_transfer	t = {
			.tx_buf		= spidev->tx_buffer,
			.len		= len,
			.speed_hz	= spidev->speed_hz,
		};
	struct spi_message	m;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	return spidev_sync(spidev, &m);
}

static inline ssize_t
spidev_sync_read(struct spidev_data *spidev, size_t len)
{
	struct spi_transfer	t = {
			.rx_buf		= spidev->rx_buffer,
			.len		= len,
			.speed_hz	= spidev->speed_hz,
		};
	struct spi_message	m;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	return spidev_sync(spidev, &m);
}

/*-------------------------------------------------------------------------*/

/* Read-only message with current device setup */
static ssize_t
spidev_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	struct spidev_data	*spidev;
	ssize_t			status;

	/* chipselect only toggles at start or end of operation */
	if (count > bufsiz)
		return -EMSGSIZE;

	spidev = filp->private_data;

	mutex_lock(&spidev->buf_lock);
	status = spidev_sync_read(spidev, count);
	if (status > 0) {
		unsigned long	missing;

		missing = copy_to_user(buf, spidev->rx_buffer, status);
		if (missing == status)
			status = -EFAULT;
		else
			status = status - missing;
	}
	mutex_unlock(&spidev->buf_lock);

	return status;
}

/* Write-only message with current device setup */
static ssize_t
spidev_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *f_pos)
{
	struct spidev_data	*spidev;
	ssize_t			status;
	unsigned long		missing;

	/* chipselect only toggles at start or end of operation */
	if (count > bufsiz)
		return -EMSGSIZE;

	spidev = filp->private_data;

	mutex_lock(&spidev->buf_lock);
	missing = copy_from_user(spidev->tx_buffer, buf, count);
	if (missing == 0)
		status = spidev_sync_write(spidev, count);
	else
		status = -EFAULT;
	mutex_unlock(&spidev->buf_lock);

	return status;
}

static int spidev_message(struct spidev_data *spidev,
		struct spi_ioc_transfer *u_xfers, unsigned n_xfers)
{
	struct spi_message	msg;
	struct spi_transfer	*k_xfers;
	struct spi_transfer	*k_tmp;
	struct spi_ioc_transfer *u_tmp;
	unsigned		n, total, tx_total, rx_total;
	u8			*tx_buf, *rx_buf;
	int			status = -EFAULT;

	spi_message_init(&msg);
	k_xfers = kcalloc(n_xfers, sizeof(*k_tmp), GFP_KERNEL);
	if (k_xfers == NULL)
		return -ENOMEM;

	/* Construct spi_message, copying any tx data to bounce buffer.
	 * We walk the array of user-provided transfers, using each one
	 * to initialize a kernel version of the same transfer.
	 */
	tx_buf = spidev->tx_buffer;
	rx_buf = spidev->rx_buffer;
	total = 0;
	tx_total = 0;
	rx_total = 0;
	for (n = n_xfers, k_tmp = k_xfers, u_tmp = u_xfers;
			n;
			n--, k_tmp++, u_tmp++) {
		/* Ensure that also following allocations from rx_buf/tx_buf will meet
		 * DMA alignment requirements.
		 */
		unsigned int len_aligned = ALIGN(u_tmp->len, ARCH_KMALLOC_MINALIGN);

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
			rx_total += len_aligned;
			if (rx_total > bufsiz) {
				status = -EMSGSIZE;
				goto done;
			}
			k_tmp->rx_buf = rx_buf;
			rx_buf += len_aligned;
		}
		if (u_tmp->tx_buf) {
			/* this transfer needs space in TX bounce buffer */
			tx_total += len_aligned;
			if (tx_total > bufsiz) {
				status = -EMSGSIZE;
				goto done;
			}
			k_tmp->tx_buf = tx_buf;
			if (copy_from_user(tx_buf, (const u8 __user *)
						(uintptr_t) u_tmp->tx_buf,
					u_tmp->len))
				goto done;
			tx_buf += len_aligned;
		}

		k_tmp->cs_change = !!u_tmp->cs_change;
		k_tmp->tx_nbits = u_tmp->tx_nbits;
		k_tmp->rx_nbits = u_tmp->rx_nbits;
		k_tmp->bits_per_word = u_tmp->bits_per_word;
		k_tmp->delay.value = u_tmp->delay_usecs;
		k_tmp->delay.unit = SPI_DELAY_UNIT_USECS;
		k_tmp->speed_hz = u_tmp->speed_hz;
		k_tmp->word_delay.value = u_tmp->word_delay_usecs;
		k_tmp->word_delay.unit = SPI_DELAY_UNIT_USECS;
		if (!k_tmp->speed_hz)
			k_tmp->speed_hz = spidev->speed_hz;
#ifdef VERBOSE
		dev_dbg(&spidev->spi->dev,
			"  xfer len %u %s%s%s%dbits %u usec %u usec %uHz\n",
			k_tmp->len,
			k_tmp->rx_buf ? "rx " : "",
			k_tmp->tx_buf ? "tx " : "",
			k_tmp->cs_change ? "cs " : "",
			k_tmp->bits_per_word ? : spidev->spi->bits_per_word,
			k_tmp->delay.value,
			k_tmp->word_delay.value,
			k_tmp->speed_hz ? : spidev->spi->max_speed_hz);
#endif
		spi_message_add_tail(k_tmp, &msg);
	}

	status = spidev_sync(spidev, &msg);
	if (status < 0)
		goto done;

	/* copy any rx data out of bounce buffer */
	for (n = n_xfers, k_tmp = k_xfers, u_tmp = u_xfers;
			n;
			n--, k_tmp++, u_tmp++) {
		if (u_tmp->rx_buf) {
			if (copy_to_user((u8 __user *)
					(uintptr_t) u_tmp->rx_buf, k_tmp->rx_buf,
					u_tmp->len)) {
				status = -EFAULT;
				goto done;
			}
		}
	}
	status = total;

done:
	kfree(k_xfers);
	return status;
}

#if IS_ENABLED(CONFIG_BRCM_XGBE)
static int spidev_open_kern(void)
{
	int			status = -ENXIO;

	mutex_lock(&device_list_lock);

	if (!g_spidev2->tx_buffer) {
		g_spidev2->tx_buffer = kmalloc(bufsiz, GFP_KERNEL);
		if (!g_spidev2->tx_buffer) {
			dev_dbg(&g_spidev2->spi->dev, "open/ENOMEM\n");
			status = -ENOMEM;
			goto err_find_dev;
		}
	}

	if (!g_spidev2->rx_buffer) {
		g_spidev2->rx_buffer = kmalloc(bufsiz, GFP_KERNEL);
		if (!g_spidev2->rx_buffer) {
			dev_dbg(&g_spidev2->spi->dev, "open/ENOMEM\n");
			status = -ENOMEM;
			goto err_alloc_rx_buf;
		}
	}

	g_spidev2->users++;
	mutex_unlock(&device_list_lock);
	return 0;

err_alloc_rx_buf:
	kfree(g_spidev2->tx_buffer);
	g_spidev2->tx_buffer = NULL;
err_find_dev:
	mutex_unlock(&device_list_lock);
	return status;
}

static int spidev_release_kern(void)
{
	int			dofree;

	spin_lock_irq(&g_spidev2->spi_lock);
	/* ... after we unbound from the underlying device? */
	dofree = (g_spidev2->spi == NULL);
	spin_unlock_irq(&g_spidev2->spi_lock);

	/* last close? */
	g_spidev2->users--;
	if (!g_spidev2->users) {

		kfree(g_spidev2->tx_buffer);
		g_spidev2->tx_buffer = NULL;

		kfree(g_spidev2->rx_buffer);
		g_spidev2->rx_buffer = NULL;

		if (dofree)
			kfree(g_spidev2);
		else
			g_spidev2->speed_hz = g_spidev2->spi->max_speed_hz;
	}
#ifdef CONFIG_SPI_SLAVE
	if (!dofree)
		spi_slave_abort(g_spidev2->spi);
#endif
	mutex_unlock(&device_list_lock);

	return 0;
}

static int spidev_message_kern(struct spidev_data *spidev,
		struct spi_ioc_transfer *u_xfers, unsigned n_xfers)
{
	struct spi_message	msg;
	struct spi_transfer	*k_xfers;
	struct spi_transfer	*k_tmp;
	struct spi_ioc_transfer *u_tmp;
	unsigned		n, total, tx_total, rx_total;
	u8			*tx_buf, *rx_buf;
	int			status = -EFAULT;

	spi_message_init(&msg);
	k_xfers = kcalloc(n_xfers, sizeof(*k_tmp), GFP_KERNEL);

	if (k_xfers == NULL)
		return -ENOMEM;

	/* Construct spi_message, copying any tx data to bounce buffer.
	 * We walk the array of user-provided transfers, using each one
	 * to initialize a kernel version of the same transfer.
	 */
	tx_buf = spidev->tx_buffer;
	rx_buf = spidev->rx_buffer;
	total = 0;
	tx_total = 0;
	rx_total = 0;
	for (n = n_xfers, k_tmp = k_xfers, u_tmp = u_xfers;
			n;
			n--, k_tmp++, u_tmp++) {
		/* Ensure that also following allocations from rx_buf/tx_buf will meet
		 * DMA alignment requirements.
		 */
		unsigned int len_aligned = ALIGN(u_tmp->len, ARCH_KMALLOC_MINALIGN);

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
			rx_total += len_aligned;
			if (rx_total > bufsiz) {
				status = -EMSGSIZE;
				goto done;
			}
			k_tmp->rx_buf = rx_buf;
			rx_buf += len_aligned;
		}
		if (u_tmp->tx_buf) {
			/* this transfer needs space in TX bounce buffer */
			tx_total += len_aligned;
			if (tx_total > bufsiz) {
				status = -EMSGSIZE;
				goto done;
			}
			k_tmp->tx_buf = tx_buf;
			memcpy(tx_buf, (void*)(uintptr_t)(u_tmp->tx_buf), u_tmp->len);
			tx_buf += len_aligned;
		}

		k_tmp->cs_change = !!u_tmp->cs_change;
		k_tmp->tx_nbits = u_tmp->tx_nbits;
		k_tmp->rx_nbits = u_tmp->rx_nbits;
		k_tmp->bits_per_word = u_tmp->bits_per_word;
		k_tmp->delay.value = u_tmp->delay_usecs;
		k_tmp->delay.unit = SPI_DELAY_UNIT_USECS;
		k_tmp->speed_hz = u_tmp->speed_hz;
		k_tmp->word_delay.value = u_tmp->word_delay_usecs;
		k_tmp->word_delay.unit = SPI_DELAY_UNIT_USECS;
		if (!k_tmp->speed_hz)
			k_tmp->speed_hz = spidev->speed_hz;
#ifdef VERBOSE
		dev_dbg(&spidev->spi->dev,
			" xfer len %u %s%s%s%dbits %u usec %u usec %uHz\n",
			k_tmp->len,
			k_tmp->rx_buf ? "rx " : "",
			k_tmp->tx_buf ? "tx " : "",
			k_tmp->cs_change ? "cs " : "",
			k_tmp->bits_per_word ? : spidev->spi->bits_per_word,
			k_tmp->delay.value,
			k_tmp->word_delay.value,
			k_tmp->speed_hz ? : spidev->spi->max_speed_hz);
#endif
		spi_message_add_tail(k_tmp, &msg);
	}

	status = spidev_sync(spidev, &msg);
	if (status < 0)
		goto done;

	/* copy any rx data out of bounce buffer */
	for (n = n_xfers, k_tmp = k_xfers, u_tmp = u_xfers;
			n;
			n--, k_tmp++, u_tmp++) {
		if (u_tmp->rx_buf) {
			memcpy((void*)(uintptr_t)(u_tmp->rx_buf), k_tmp->rx_buf, u_tmp->len);
		}
	}
	status = total;

done:
	kfree(k_xfers);
	return status;
}
#endif

static struct spi_ioc_transfer *
spidev_get_ioc_message(unsigned int cmd, struct spi_ioc_transfer __user *u_ioc,
		unsigned *n_ioc)
{
	u32	tmp;

	/* Check type, command number and direction */
	if (_IOC_TYPE(cmd) != SPI_IOC_MAGIC
			|| _IOC_NR(cmd) != _IOC_NR(SPI_IOC_MESSAGE(0))
			|| _IOC_DIR(cmd) != _IOC_WRITE)
		return ERR_PTR(-ENOTTY);

	tmp = _IOC_SIZE(cmd);
	if ((tmp % sizeof(struct spi_ioc_transfer)) != 0)
		return ERR_PTR(-EINVAL);
	*n_ioc = tmp / sizeof(struct spi_ioc_transfer);
	if (*n_ioc == 0)
		return NULL;

	/* copy into scratch area */
	return memdup_user(u_ioc, tmp);
}

static long
spidev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int			retval = 0;
	struct spidev_data	*spidev;
	struct spi_device	*spi;
	u32			tmp;
	unsigned		n_ioc;
	struct spi_ioc_transfer	*ioc;

	/* Check type and command number */
	if (_IOC_TYPE(cmd) != SPI_IOC_MAGIC)
		return -ENOTTY;

	/* guard against device removal before, or while,
	 * we issue this ioctl.
	 */
	spidev = filp->private_data;
	spin_lock_irq(&spidev->spi_lock);
	spi = spi_dev_get(spidev->spi);
	spin_unlock_irq(&spidev->spi_lock);

	if (spi == NULL)
		return -ESHUTDOWN;

	/* use the buffer lock here for triple duty:
	 *  - prevent I/O (from us) so calling spi_setup() is safe;
	 *  - prevent concurrent SPI_IOC_WR_* from morphing
	 *    data fields while SPI_IOC_RD_* reads them;
	 *  - SPI_IOC_MESSAGE needs the buffer locked "normally".
	 */
	mutex_lock(&spidev->buf_lock);

	switch (cmd) {
	/* read requests */
	case SPI_IOC_RD_MODE:
	case SPI_IOC_RD_MODE32:
		tmp = spi->mode;

		{
			struct spi_controller *ctlr = spi->controller;

			if (ctlr->use_gpio_descriptors && ctlr->cs_gpiods &&
			    ctlr->cs_gpiods[spi->chip_select])
				tmp &= ~SPI_CS_HIGH;
		}

		if (cmd == SPI_IOC_RD_MODE)
			retval = put_user(tmp & SPI_MODE_MASK,
					  (__u8 __user *)arg);
		else
			retval = put_user(tmp & SPI_MODE_MASK,
					  (__u32 __user *)arg);
		break;
	case SPI_IOC_RD_LSB_FIRST:
		retval = put_user((spi->mode & SPI_LSB_FIRST) ?  1 : 0,
					(__u8 __user *)arg);
		break;
	case SPI_IOC_RD_BITS_PER_WORD:
		retval = put_user(spi->bits_per_word, (__u8 __user *)arg);
		break;
	case SPI_IOC_RD_MAX_SPEED_HZ:
		retval = put_user(spidev->speed_hz, (__u32 __user *)arg);
		break;

	/* write requests */
	case SPI_IOC_WR_MODE:
	case SPI_IOC_WR_MODE32:
		if (cmd == SPI_IOC_WR_MODE)
			retval = get_user(tmp, (u8 __user *)arg);
		else
			retval = get_user(tmp, (u32 __user *)arg);
		if (retval == 0) {
			struct spi_controller *ctlr = spi->controller;
			u32	save = spi->mode;

			if (tmp & ~SPI_MODE_MASK) {
				retval = -EINVAL;
				break;
			}

			if (ctlr->use_gpio_descriptors && ctlr->cs_gpiods &&
			    ctlr->cs_gpiods[spi->chip_select])
				tmp |= SPI_CS_HIGH;

			tmp |= spi->mode & ~SPI_MODE_MASK;
			spi->mode = tmp & SPI_MODE_USER_MASK;
			retval = spi_setup(spi);
			if (retval < 0)
				spi->mode = save;
			else
				dev_dbg(&spi->dev, "spi mode %x\n", tmp);
		}
		break;
	case SPI_IOC_WR_LSB_FIRST:
		retval = get_user(tmp, (__u8 __user *)arg);
		if (retval == 0) {
			u32	save = spi->mode;

			if (tmp)
				spi->mode |= SPI_LSB_FIRST;
			else
				spi->mode &= ~SPI_LSB_FIRST;
			retval = spi_setup(spi);
			if (retval < 0)
				spi->mode = save;
			else
				dev_dbg(&spi->dev, "%csb first\n",
						tmp ? 'l' : 'm');
		}
		break;
	case SPI_IOC_WR_BITS_PER_WORD:
		retval = get_user(tmp, (__u8 __user *)arg);
		if (retval == 0) {
			u8	save = spi->bits_per_word;

			spi->bits_per_word = tmp;
			retval = spi_setup(spi);
			if (retval < 0)
				spi->bits_per_word = save;
			else
				dev_dbg(&spi->dev, "%d bits per word\n", tmp);
		}
		break;
	case SPI_IOC_WR_MAX_SPEED_HZ: {
		u32 save;

		retval = get_user(tmp, (__u32 __user *)arg);
		if (retval)
			break;
		if (tmp == 0) {
			retval = -EINVAL;
			break;
		}

		save = spi->max_speed_hz;

		spi->max_speed_hz = tmp;
		retval = spi_setup(spi);
		if (retval == 0) {
			spidev->speed_hz = tmp;
			dev_dbg(&spi->dev, "%d Hz (max)\n", spidev->speed_hz);
		}

		spi->max_speed_hz = save;
		break;
	}
	default:
		/* segmented and/or full-duplex I/O request */
		/* Check message and copy into scratch area */
		ioc = spidev_get_ioc_message(cmd,
				(struct spi_ioc_transfer __user *)arg, &n_ioc);
		if (IS_ERR(ioc)) {
			retval = PTR_ERR(ioc);
			break;
		}
		if (!ioc)
			break;	/* n_ioc is also 0 */

		/* translate to spi_message, execute */
		retval = spidev_message(spidev, ioc, n_ioc);
		kfree(ioc);
		break;
	}

	mutex_unlock(&spidev->buf_lock);
	spi_dev_put(spi);
	return retval;
}

#ifdef CONFIG_COMPAT
static long
spidev_compat_ioc_message(struct file *filp, unsigned int cmd,
		unsigned long arg)
{
	struct spi_ioc_transfer __user	*u_ioc;
	int				retval = 0;
	struct spidev_data		*spidev;
	struct spi_device		*spi;
	unsigned			n_ioc, n;
	struct spi_ioc_transfer		*ioc;

	u_ioc = (struct spi_ioc_transfer __user *) compat_ptr(arg);

	/* guard against device removal before, or while,
	 * we issue this ioctl.
	 */
	spidev = filp->private_data;
	spin_lock_irq(&spidev->spi_lock);
	spi = spi_dev_get(spidev->spi);
	spin_unlock_irq(&spidev->spi_lock);

	if (spi == NULL)
		return -ESHUTDOWN;

	/* SPI_IOC_MESSAGE needs the buffer locked "normally" */
	mutex_lock(&spidev->buf_lock);

	/* Check message and copy into scratch area */
	ioc = spidev_get_ioc_message(cmd, u_ioc, &n_ioc);
	if (IS_ERR(ioc)) {
		retval = PTR_ERR(ioc);
		goto done;
	}
	if (!ioc)
		goto done;	/* n_ioc is also 0 */

	/* Convert buffer pointers */
	for (n = 0; n < n_ioc; n++) {
		ioc[n].rx_buf = (uintptr_t) compat_ptr(ioc[n].rx_buf);
		ioc[n].tx_buf = (uintptr_t) compat_ptr(ioc[n].tx_buf);
	}

	/* translate to spi_message, execute */
	retval = spidev_message(spidev, ioc, n_ioc);
	kfree(ioc);

done:
	mutex_unlock(&spidev->buf_lock);
	spi_dev_put(spi);
	return retval;
}

static long
spidev_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	if (_IOC_TYPE(cmd) == SPI_IOC_MAGIC
			&& _IOC_NR(cmd) == _IOC_NR(SPI_IOC_MESSAGE(0))
			&& _IOC_DIR(cmd) == _IOC_WRITE)
		return spidev_compat_ioc_message(filp, cmd, arg);

	return spidev_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
}
#else
#define spidev_compat_ioctl NULL
#endif /* CONFIG_COMPAT */

static int spidev_open(struct inode *inode, struct file *filp)
{
	struct spidev_data	*spidev = NULL, *iter;
	int			status = -ENXIO;

	mutex_lock(&device_list_lock);

	list_for_each_entry(iter, &device_list, device_entry) {
		if (iter->devt == inode->i_rdev) {
			status = 0;
			spidev = iter;
			break;
		}
	}

	if (!spidev) {
		pr_debug("spidev: nothing for minor %d\n", iminor(inode));
		goto err_find_dev;
	}

	if (!spidev->tx_buffer) {
		spidev->tx_buffer = kmalloc(bufsiz, GFP_KERNEL);
		if (!spidev->tx_buffer) {
			status = -ENOMEM;
			goto err_find_dev;
		}
	}

	if (!spidev->rx_buffer) {
		spidev->rx_buffer = kmalloc(bufsiz, GFP_KERNEL);
		if (!spidev->rx_buffer) {
			status = -ENOMEM;
			goto err_alloc_rx_buf;
		}
	}

	spidev->users++;
	filp->private_data = spidev;
	stream_open(inode, filp);

	mutex_unlock(&device_list_lock);
	return 0;

err_alloc_rx_buf:
	kfree(spidev->tx_buffer);
	spidev->tx_buffer = NULL;
err_find_dev:
	mutex_unlock(&device_list_lock);
	return status;
}

static int spidev_release(struct inode *inode, struct file *filp)
{
	struct spidev_data	*spidev;
	int			dofree;

	mutex_lock(&device_list_lock);
	spidev = filp->private_data;
	filp->private_data = NULL;

	spin_lock_irq(&spidev->spi_lock);
	/* ... after we unbound from the underlying device? */
	dofree = (spidev->spi == NULL);
	spin_unlock_irq(&spidev->spi_lock);

	/* last close? */
	spidev->users--;
	if (!spidev->users) {

		kfree(spidev->tx_buffer);
		spidev->tx_buffer = NULL;

		kfree(spidev->rx_buffer);
		spidev->rx_buffer = NULL;

		if (dofree)
			kfree(spidev);
		else
			spidev->speed_hz = spidev->spi->max_speed_hz;
	}
#ifdef CONFIG_SPI_SLAVE
	if (!dofree)
		spi_slave_abort(spidev->spi);
#endif
	mutex_unlock(&device_list_lock);

	return 0;
}

static const struct file_operations spidev_fops = {
	.owner =	THIS_MODULE,
	/* REVISIT switch to aio primitives, so that userspace
	 * gets more complete API coverage.  It'll simplify things
	 * too, except for the locking.
	 */
	.write =	spidev_write,
	.read =		spidev_read,
	.unlocked_ioctl = spidev_ioctl,
	.compat_ioctl = spidev_compat_ioctl,
	.open =		spidev_open,
	.release =	spidev_release,
	.llseek =	no_llseek,
};

/*-------------------------------------------------------------------------*/

/* The main reason to have this class is to make mdev/udev create the
 * /dev/spidevB.C character device nodes exposing our userspace API.
 * It also simplifies memory management.
 */

static struct class *spidev_class;

static const struct spi_device_id spidev_spi_ids[] = {
	{ .name = "dh2228fv" },
	{ .name = "ltc2488" },
	{ .name = "sx1301" },
	{ .name = "bk4" },
	{ .name = "dhcom-board" },
	{ .name = "m53cpld" },
	{ .name = "spi-petra" },
	{ .name = "spi-authenta" },
	{},
};
MODULE_DEVICE_TABLE(spi, spidev_spi_ids);

/*
 * spidev should never be referenced in DT without a specific compatible string,
 * it is a Linux implementation thing rather than a description of the hardware.
 */
static int spidev_of_check(struct device *dev)
{
	if (device_property_match_string(dev, "compatible", "spidev") < 0)
		return 0;

	dev_err(dev, "spidev listed directly in DT is not supported\n");
	return -EINVAL;
}

static const struct of_device_id spidev_dt_ids[] = {
	{ .compatible = "rohm,dh2228fv", .data = &spidev_of_check },
	{ .compatible = "lineartechnology,ltc2488", .data = &spidev_of_check },
	{ .compatible = "semtech,sx1301", .data = &spidev_of_check },
	{ .compatible = "lwn,bk4", .data = &spidev_of_check },
	{ .compatible = "dh,dhcom-board", .data = &spidev_of_check },
	{ .compatible = "menlo,m53cpld", .data = &spidev_of_check },
	{ .compatible = "cisco,spi-petra", .data = &spidev_of_check },
	{ .compatible = "micron,spi-authenta", .data = &spidev_of_check },
	{ .compatible = "qcom,spi-msm-codec-slave", .data = &spidev_of_check },
	{ .compatible = "brcm,bcm89272", .data = &spidev_of_check },
	{ .compatible = "qcom,spidev0", .data = &spidev_of_check },
	{ .compatible = "qcom,spidev1", .data = &spidev_of_check },
	{ .compatible = "qcom,spidev3", .data = &spidev_of_check },
	{},
};
MODULE_DEVICE_TABLE(of, spidev_dt_ids);

/* Dummy SPI devices not to be used in production systems */
static int spidev_acpi_check(struct device *dev)
{
	dev_warn(dev, "do not use this driver in production systems!\n");
	return 0;
}

static const struct acpi_device_id spidev_acpi_ids[] = {
	/*
	 * The ACPI SPT000* devices are only meant for development and
	 * testing. Systems used in production should have a proper ACPI
	 * description of the connected peripheral and they should also use
	 * a proper driver instead of poking directly to the SPI bus.
	 */
	{ "SPT0001", (kernel_ulong_t)&spidev_acpi_check },
	{ "SPT0002", (kernel_ulong_t)&spidev_acpi_check },
	{ "SPT0003", (kernel_ulong_t)&spidev_acpi_check },
	{},
};
MODULE_DEVICE_TABLE(acpi, spidev_acpi_ids);

/*-------------------------------------------------------------------------*/

static int spidev_probe(struct spi_device *spi)
{
	int (*match)(struct device *dev);
	struct spidev_data	*spidev;
	int			status;
	unsigned long		minor;

	match = device_get_match_data(&spi->dev);
	if (match) {
		status = match(&spi->dev);
		if (status)
			return status;
	}

	/* Allocate driver data */
	spidev = kzalloc(sizeof(*spidev), GFP_KERNEL);
	if (!spidev)
		return -ENOMEM;

	/* Initialize the driver data */
	spidev->spi = spi;
	spin_lock_init(&spidev->spi_lock);
	mutex_init(&spidev->buf_lock);

	INIT_LIST_HEAD(&spidev->device_entry);

	/* If we can allocate a minor number, hook up this device.
	 * Reusing minors is fine so long as udev or mdev is working.
	 */
	mutex_lock(&device_list_lock);
	minor = find_first_zero_bit(minors, N_SPI_MINORS);
	if (minor < N_SPI_MINORS) {
		struct device *dev;

		spidev->devt = MKDEV(SPIDEV_MAJOR, minor);
		dev = device_create(spidev_class, &spi->dev, spidev->devt,
				    spidev, "spidev%d.%d",
				    spi->master->bus_num, spi->chip_select);
		status = PTR_ERR_OR_ZERO(dev);
	} else {
		dev_dbg(&spi->dev, "no minor number available!\n");
		status = -ENODEV;
	}
	if (status == 0) {
		set_bit(minor, minors);
		list_add(&spidev->device_entry, &device_list);
	}
	mutex_unlock(&device_list_lock);

	spidev->speed_hz = spi->max_speed_hz;

	if (status == 0)
		spi_set_drvdata(spi, spidev);
	else
		kfree(spidev);

#if IS_ENABLED(CONFIG_BRCM_XGBE)
	if (strcmp(spi->dev.of_node->full_name, "spidev@2") == 0) {
		g_spidev2 = spidev;
	}
#endif

	return status;
}

static void spidev_remove(struct spi_device *spi)
{
	struct spidev_data	*spidev = spi_get_drvdata(spi);

	/* prevent new opens */
	mutex_lock(&device_list_lock);
	/* make sure ops on existing fds can abort cleanly */
	spin_lock_irq(&spidev->spi_lock);
	spidev->spi = NULL;
	spin_unlock_irq(&spidev->spi_lock);

	list_del(&spidev->device_entry);
	device_destroy(spidev_class, spidev->devt);
	clear_bit(MINOR(spidev->devt), minors);
	if (spidev->users == 0)
		kfree(spidev);
	mutex_unlock(&device_list_lock);

#if IS_ENABLED(CONFIG_BRCM_XGBE)
	g_spidev2 = NULL;
#endif
}

static struct spi_driver spidev_spi_driver = {
	.driver = {
		.name =		"spidev",
		.of_match_table = spidev_dt_ids,
		.acpi_match_table = spidev_acpi_ids,
	},
	.probe =	spidev_probe,
	.remove =	spidev_remove,
	.id_table =	spidev_spi_ids,

	/* NOTE:  suspend/resume methods are not necessary here.
	 * We don't do anything except pass the requests to/from
	 * the underlying controller.  The refrigerator handles
	 * most issues; the controller driver handles the rest.
	 */
};

/*-------------------------------------------------------------------------*/

#if IS_ENABLED(CONFIG_BRCM_XGBE)
/*------------------------------ sysfs operation start ---------------------------*/
static int aps_spi_read(unsigned char *wr_buf, int wr_len, unsigned char *rd_buf, int rd_len)
{
	struct spi_ioc_transfer xfer[2];
	int status = 0;
	memset(xfer, 0, sizeof (xfer));

	xfer[0].tx_buf = (unsigned long)wr_buf;
	xfer[0].len = wr_len;
	xfer[0].rx_buf = (unsigned long long)NULL;

	xfer[1].tx_buf = (unsigned long long)NULL;
	xfer[1].len = rd_len;
	xfer[1].rx_buf = (unsigned long)rd_buf;

	if (g_spidev2 != NULL) {
		status = spidev_message_kern(g_spidev2, xfer, 2);
	}

	return status;
}

static int spi_read16(unsigned int addr, unsigned short *rd_val)
{
	unsigned char buf[MAX_BUF_SZ];
	unsigned char val[2];
	int len = 0;
	int i, status;

	//opcode
	buf[len++] = SPI_OPCODE_PHYADDR(g_spi_id) | SPI_OPCODE_RD | SPI_OPCODE_NO_INC | SPI_OPCODE_RD_WAIT_2 | SPI_OPCODE_TX_SZ_16;
	buf[len++] = (addr >> 24) & 0xff;
	buf[len++] = (addr >> 16) & 0xff;
	buf[len++] = (addr >> 8) & 0xff;
	buf[len++] = (addr & 0xff);
	/* wait states as per opcode */
	for (i =0; i < ((buf[0] & SPI_OPCODE_RD_WAIT_MASK) >> SPI_OPCODE_RD_WAIT_SHIFT) * 2; i++)
		buf[len++] = 0x0;

	status = aps_spi_read(&buf[0], len, (unsigned char*)val, sizeof(unsigned short));

	*rd_val = ((unsigned int)val[0] << 8UL) | val[1];
	return status > 0 ? 0 : status;
}

static int aps_spi_write(unsigned char *buf, int len)
{
	struct spi_ioc_transfer xfer[1];
	int status = 0;

	memset(xfer, 0, sizeof (xfer));

	xfer[0].tx_buf = (unsigned long long)buf;
	xfer[0].len = len;
	xfer[0].rx_buf = (unsigned long long)NULL;

	if (g_spidev2 != NULL) {
		status = spidev_message_kern(g_spidev2, xfer, 1);
	}

	return status;
}

static int spi_write16(unsigned int addr, unsigned short data)
{
	unsigned char txbuf[MAX_BUF_SZ];
	int len = 0;
	int status;

	//opcode
	txbuf[len++] = SPI_OPCODE_PHYADDR(g_spi_id) | SPI_OPCODE_WR | SPI_OPCODE_NO_INC | SPI_OPCODE_RD_WAIT_0 | SPI_OPCODE_TX_SZ_16;
	txbuf[len++] = (addr >> 24) & 0xff;
	txbuf[len++] = (addr >> 16) & 0xff;
	txbuf[len++] = (addr >> 8) & 0xff;
	txbuf[len++] = (addr & 0xff);
	txbuf[len++] = (data >> 8) & 0xff;
	txbuf[len++] = (data & 0xff);

	status = aps_spi_write(&txbuf[0], len);
	return status > 0 ? 0 : status;
}

static void acd_init_port0(void)
{
	unsigned int addr;
	unsigned short data;

	spidev_open_kern();

	addr = 0x49030000 + 0x1E04;
	data = 0x0001;
	spi_write16(addr, data);

	addr = 0x49030000 + 0x2E0E;
	data = 0x0202;
	spi_write16(addr, data);     // ACD_EXPC7

	addr = 0x49030000 + 0x2E10;
	data = 0x7F50;
	spi_write16(addr, data);     // ACD_EXPC8

	addr = 0x49030000 + 0x2E12;
	data = 0x2C22;
	spi_write16(addr, data);     // ACD_EXPC9

	addr = 0x49030000 + 0x2E14;
	data = 0x5252;
	spi_write16(addr, data);     // ACD_EXPCA

	addr = 0x49030000 + 0x2E16;
	data = 0x0000;
	spi_write16(addr, data);     // ACD_EXPCB

	addr = 0x49030000 + 0x2E18;
	data = 0x0014;
	spi_write16(addr, data);     // ACD_EXPCC

	addr = 0x49030000 + 0x2E1C;
	data = 0x1CA3;
	spi_write16(addr, data);     // ACD_EXPCE

	addr = 0x49030000 + 0x2E1E;
	data = 0x0206;
	spi_write16(addr, data);     // ACD_EXPCF

	addr = 0x49030000 + 0x2E20;
	data = 0x0010;
	spi_write16(addr, data);     // ACD_EXPE0

	addr = 0x49030000 + 0x2E22;
	data = 0x0D0D;
	spi_write16(addr, data);     // ACD_EXPE1

	addr = 0x49030000 + 0x2E24;
	data = 0x0000;
	spi_write16(addr, data);     // ACD_EXPE2

	addr = 0x49030000 + 0x2E26;
	data = 0x7700;
	spi_write16(addr, data);     // ACD_EXPE3

	addr = 0x49030000 + 0x2E28;
	data = 0x0000;
	spi_write16(addr, data);     // ACD_EXPE4

	addr = 0x49030000 + 0x2E2E;
	data = 0x0000;
	spi_write16(addr, data);     // ACD_EXPE7

	addr = 0x49030000 + 0x2E3E;
	data = 0x409F;
	spi_write16(addr, data);     // ACD_EXPEF

	addr = 0x49030000 + 0x2E1A;
	data = 0x1129;
	spi_write16(addr, data);     // ACD_EXPCD

	addr = 0x49030000 + 0x2E1A;
	data = 0x0129;
	spi_write16(addr, data);     // ACD_EXPCD

	addr = 0x49030000 + 0x2E20;
	data = 0x0000;
	spi_write16(addr, data);     // ACD_EXPE0

	addr = 0x49030000 + 0x2E22;
	data = 0x0000;
	spi_write16(addr, data);     // ACD_EXPE1

	addr = 0x49030000 + 0x2E24;
	data = 0x0000;
	spi_write16(addr, data);     // ACD_EXPE2

	addr = 0x49030000 + 0x2E26;
	data = 0x0000;
	spi_write16(addr, data);     // ACD_EXPE3

	addr = 0x49030000 + 0x2E28;
	data = 0x0000;
	spi_write16(addr, data);     // ACD_EXPE4

	addr = 0x49030000 + 0x2E2E;
	data = 0x0000;
	spi_write16(addr, data);     // ACD_EXPE7

	addr = 0x49030000 + 0x2E3E;
	data = 0x0000;
	spi_write16(addr, data);     // ACD_EXPEF

	addr = 0x49030000 + 0x2E20;
	data = 0x3619;
	spi_write16(addr, data);     // ACD_EXPE0

	addr = 0x49030000 + 0x2E22;
	data = 0x343A;
	spi_write16(addr, data);     // ACD_EXPE1

	addr = 0x49030000 + 0x2E24;
	data = 0x0000;
	spi_write16(addr, data);     // ACD_EXPE2

	addr = 0x49030000 + 0x2E26;
	data = 0x0000;
	spi_write16(addr, data);     // ACD_EXPE3

	addr = 0x49030000 + 0x2E28;
	data = 0x8000;
	spi_write16(addr, data);     // ACD_EXPE4

	addr = 0x49030000 + 0x2E2A;
	data = 0x000E;
	spi_write16(addr, data);     // ACD_EXPE5

	addr = 0x49030000 + 0x2E2E;
	data = 0x0000;
	spi_write16(addr, data);     // ACD_EXPE7

	addr = 0x49030000 + 0x2E32;
	data = 0x0400;
	spi_write16(addr, data);     // ACD_EXPE9

	addr = 0x49030000 + 0x2E3A;
	data = 0x0000;
	spi_write16(addr, data);     // ACD_EXPED

	addr = 0x49030000 + 0x2E3E;
	data = 0xA2BF;
	spi_write16(addr, data);     // ACD_EXPEF

	addr = 0x49030000 + 0x2E1A;
	data = 0x1129;
	spi_write16(addr, data);     // ACD_EXPCD

	addr = 0x49030000 + 0x2E1A;
	data = 0x0129;
	spi_write16(addr, data);     // ACD_EXPCD

	addr = 0x49030000 + 0x2E20;
	data = 0x0000;
	spi_write16(addr, data);     // ACD_EXPE0

	addr = 0x49030000 + 0x2E22;
	data = 0x0000;
	spi_write16(addr, data);     // ACD_EXPE1

	addr = 0x49030000 + 0x2E24;
	data = 0x0000;
	spi_write16(addr, data);     // ACD_EXPE2

	addr = 0x49030000 + 0x2E26;
	data = 0x0000;
	spi_write16(addr, data);     // ACD_EXPE3

	addr = 0x49030000 + 0x2E28;
	data = 0x0000;
	spi_write16(addr, data);     // ACD_EXPE4

	addr = 0x49030000 + 0x2E2A;
	data = 0x0000;
	spi_write16(addr, data);     // ACD_EXPE5

	addr = 0x49030000 + 0x2E2E;
	data = 0x0000;
	spi_write16(addr, data);     // ACD_EXPE7

	addr = 0x49030000 + 0x2E30;
	data = 0x0000;
	spi_write16(addr, data);     // ACD_EXPE8

	addr = 0x49030000 + 0x2E32;
	data = 0x0000;
	spi_write16(addr, data);     // ACD_EXPE9

	addr = 0x49030000 + 0x2E3A;
	data = 0x0000;
	spi_write16(addr, data);     // ACD_EXPED

	addr = 0x49030000 + 0x2E3E;
	data = 0x0000;
	spi_write16(addr, data);     // ACD_EXPEF

	spidev_release_kern();
}

static void acd_init_port3(void)
{
	unsigned int addr;
	unsigned short data;

	spidev_open_kern();

	addr = 0x49CF254E;
	data = 0xA01A;
	spi_write16(addr, data);

	addr = 0x49CF2550;
	data = 0x0000;
	spi_write16(addr, data);

	addr = 0x49CF2552;
	data = 0x00EF;
	spi_write16(addr, data);

	addr = 0x49CF2558;
	data = 0x0200;
	spi_write16(addr, data);

	addr = 0x49CF255C;
	data = 0x4000;
	spi_write16(addr, data);

	addr = 0x49CF255E;
	data = 0x3000;
	spi_write16(addr, data);

	addr = 0x49CF2560;
	data = 0x0015;
	spi_write16(addr, data);

	addr = 0x49CF2562;
	data = 0x0D0D;
	spi_write16(addr, data);

	addr = 0x49CF2564;
	data = 0x0000;
	spi_write16(addr, data);

	addr = 0x49CF2566;
	data = 0x7700;
	spi_write16(addr, data);

	addr = 0x49CF2568;
	data = 0x0000;
	spi_write16(addr, data);

	addr = 0x49CF256E;
	data = 0x00A0;
	spi_write16(addr, data);

	addr = 0x49CF257E;
	data = 0x409F;
	spi_write16(addr, data);

	addr = 0x49CF255A;
	data = 0x1000;
	spi_write16(addr, data);

	addr = 0x49CF255A;
	data = 0x0000;
	spi_write16(addr, data);

	addr = 0x49CF2560;
	data = 0x0000;
	spi_write16(addr, data);

	addr = 0x49CF2562;
	data = 0x0000;
	spi_write16(addr, data);

	addr = 0x49CF2564;
	data = 0x0000;
	spi_write16(addr, data);

	addr = 0x49CF2566;
	data = 0x0000;
	spi_write16(addr, data);

	addr = 0x49CF2568;
	data = 0x0000;
	spi_write16(addr, data);

	addr = 0x49CF256E;
	data = 0x0000;
	spi_write16(addr, data);

	addr = 0x49CF257E;
	data = 0x0000;
	spi_write16(addr, data);

	addr = 0x49CF2560;
	data = 0x3600;
	spi_write16(addr, data);

	addr = 0x49CF2562;
	data = 0x343A;
	spi_write16(addr, data);

	addr = 0x49CF2564;
	data = 0x0000;
	spi_write16(addr, data);

	addr = 0x49CF2566;
	data = 0x0000;
	spi_write16(addr, data);

	addr = 0x49CF2568;
	data = 0x8000;
	spi_write16(addr, data);

	addr = 0x49CF256A;
	data = 0x000E;
	spi_write16(addr, data);

	addr = 0x49CF256E;
	data = 0x0000;
	spi_write16(addr, data);

	addr = 0x49CF2572;
	data = 0x0400;
	spi_write16(addr, data);

	addr = 0x49CF257A;
	data = 0x0000;
	spi_write16(addr, data);

	addr = 0x49CF257E;
	data = 0xA3BF;
	spi_write16(addr, data);

	addr = 0x49CF255A;
	data = 0x1000;
	spi_write16(addr, data);

	addr = 0x49CF255A;
	data = 0x0000;
	spi_write16(addr, data);

	addr = 0x49CF2560;
	data = 0x0000;
	spi_write16(addr, data);

	addr = 0x49CF2562;
	data = 0x0000;
	spi_write16(addr, data);

	addr = 0x49CF2564;
	data = 0x0000;
	spi_write16(addr, data);

	addr = 0x49CF2566;
	data = 0x0000;
	spi_write16(addr, data);

	addr = 0x49CF2568;
	data = 0x0000;
	spi_write16(addr, data);

	addr = 0x49CF256A;
	data = 0x0000;
	spi_write16(addr, data);

	addr = 0x49CF256E;
	data = 0x0000;
	spi_write16(addr, data);

	addr = 0x49CF2570;
	data = 0x0000;
	spi_write16(addr, data);

	addr = 0x49CF2572;
	data = 0x0000;
	spi_write16(addr, data);

	addr = 0x49CF257A;
	data = 0x0000;
	spi_write16(addr, data);

	addr = 0x49CF257E;
	data = 0x0000;
	spi_write16(addr, data);

	addr = 0x49CF2614;
	data = 0x2000;
	spi_write16(addr, data);

	addr = 0x49CF2600;
	data = 0x8001;
	spi_write16(addr, data);

	addr = 0x49CF2602;
	data = 0x9428;
	spi_write16(addr, data);

	spidev_release_kern();
}

static ssize_t phy_bcm89272_port0_dut_status_show(struct device_driver *drv, char *buf)
{
	unsigned short read_data, data;
	unsigned int read_addr = 0x49032E02, addr = 0x49032E00;
	int status, link_status = 0;

	acd_init_port0();
	status = spidev_open_kern();
	if (status) {
		return scnprintf(buf, PAGE_SIZE, "spidev_open_kern fail: %d\n", status);
	}

	// write 0x49032E00
	status = spi_read16(addr, &data);
	if (status) {
		goto err_done;
	}
	SET_BIT_ENABLE(data, 10);
	status = spi_write16(addr, data);
	if (status) {
		goto err_done;
	}
	data = 0;
	status = spi_read16(addr, &data);
	if (status) {
		goto err_done;
	}
	SET_BIT_ENABLE(data, 15);
	SET_BIT_ENABLE(data, 9);
	status = spi_write16(addr, data);
	if (status) {
		goto err_done;
	}

	// read 0x49032E02
	msleep(1000);
	status = spi_read16(read_addr, (&read_data));
	if (status) {
		goto err_done;
	}

	// reset switch
	data = 0xFFFF;
	status = spi_write16(0x4A820024, data);

	status = spidev_release_kern();
	if (status) {
		return scnprintf(buf, PAGE_SIZE, "spidev_release_kern fail: %d\n", status);
	}
	link_status = (read_data >> 12) & 0x0000000F;

	return scnprintf(buf, PAGE_SIZE, "port0 dut status: 0x%x\n", link_status);

err_done:
	spidev_release_kern();
	return scnprintf(buf, PAGE_SIZE, "show port0 dut status fail: %d\n", link_status);
}
static DRIVER_ATTR_RO(phy_bcm89272_port0_dut_status);

static ssize_t phy_bcm89272_port3_dut_status_show(struct device_driver *drv, char *buf)
{
	unsigned short read_data, data;
	unsigned int read_addr = 0x49CF2542, addr = 0x49CF2540;
	int status, link_status = 0;

	acd_init_port3();
	status = spidev_open_kern();
	if (status) {
		return scnprintf(buf, PAGE_SIZE, "spidev_open_kern fail: %d\n", status);
	}

	// write 0x49CF2540
	status = spi_read16(addr, &data);
	if (status) {
		goto err_done;
	}
	SET_BIT_ENABLE(data, 6);
	status = spi_write16(addr, data);
	if (status) {
		goto err_done;
	}
	data = 0;
	status = spi_write16(addr, data);
	if (status) {
		goto err_done;
	}
	status = spi_read16(addr, &data);
	if (status) {
		goto err_done;
	}
	SET_BIT_ENABLE(data, 13);
	status = spi_write16(addr, data);
	if (status) {
		goto err_done;
	}
	status = spi_read16(addr, &data);
	if (status) {
		goto err_done;
	}
	SET_BIT_ENABLE(data, 13);
	SET_BIT_ENABLE(data, 15);
	status = spi_write16(addr, data);
	if (status) {
		goto err_done;
	}

	status = spi_read16(addr, &data);
	if (status) {
		goto err_done;
	}
	SET_BIT_ENABLE(data, 10);
	SET_BIT_ENABLE(data, 13);
	status = spi_write16(addr, data);
	if (status) {
		goto err_done;
	}

	status = spi_read16(addr, &data);
	if (status) {
		goto err_done;
	}
	SET_BIT_ENABLE(data, 10);
	SET_BIT_ENABLE(data, 13);
	SET_BIT_ENABLE(data, 15);
	status = spi_write16(addr, data);
	if (status) {
		goto err_done;
	}

	status = spi_read16(addr, &data);
	if (status) {
		goto err_done;
	}
	SET_BIT_ENABLE(data, 10);
	SET_BIT_ENABLE(data, 15);
	status = spi_write16(addr, data);
	if (status) {
		goto err_done;
	}

	// read 0x49CF2542
	msleep(1000);
	status = spi_read16(read_addr, (&read_data));
	if (status) {
		goto err_done;
	}

	// reset switch
	data = 0xFFFF;
	status = spi_write16(0x4A820024, data);

	status = spidev_release_kern();
	if (status) {
		return scnprintf(buf, PAGE_SIZE, "spidev_release_kern fail: %d\n", status);
	}
	link_status = (read_data >> 12) & 0xF;

	return scnprintf(buf, PAGE_SIZE, "port3 dut status: 0x%x\n", link_status);

err_done:
	spidev_release_kern();
	return scnprintf(buf, PAGE_SIZE, "show port3 dut status fail: %d\n", link_status);
}
static DRIVER_ATTR_RO(phy_bcm89272_port3_dut_status);

static ssize_t phy_bcm89272_port0_link_status_show(struct device_driver *drv, char *buf)
{
	unsigned short read_data;
	unsigned int read_addr = 0x4B000100;
	int status, link_status = 0;

	status = spidev_open_kern();
	if (status) {
		return scnprintf(buf, PAGE_SIZE, "spidev_open_kern fail: %d\n", status);
	}

	// read 0x4B000100
	status = spi_read16(read_addr, (&read_data));
	if (status) {
		goto err_done;
	}

	status = spidev_release_kern();
	if (status) {
		return scnprintf(buf, PAGE_SIZE, "spidev_release_kern fail: %d\n", status);
	}
	link_status = GET_BIT(read_data, 0);
	if (link_status == ACD_PHY_LINK_LINKED) {
		return scnprintf(buf, PAGE_SIZE, "link status: linked, data: 0x%x\n", link_status);
	} else {
		return scnprintf(buf, PAGE_SIZE, "link status: unlink, data: 0x%x\n", link_status);
	}

err_done:
	spidev_release_kern();
	return scnprintf(buf, PAGE_SIZE, "show link status fail: %d\n", link_status);
}
static DRIVER_ATTR_RO(phy_bcm89272_port0_link_status);

static ssize_t phy_bcm89272_port0_sqi_status_show(struct device_driver *drv, char *buf)
{
	unsigned short read_data, data;
	unsigned int read_addr = 0x490300A4, addr = 0x49032016;
	unsigned int sqi_status;
	int status = 0;

	status = spidev_open_kern();
	if (status) {
		return scnprintf(buf, PAGE_SIZE, "spidev_open_kern fail: %d\n", status);
	}

	status = spi_read16(addr, &data);
	if (status) {
		goto err_done;
	}
	SET_BIT_DISABLE(data, 13);
	status = spi_write16(addr, data);
	if (status) {
		goto err_done;
	}

	status = spi_read16(read_addr, &read_data);
	if (status) {
		goto err_done;
	}
	sqi_status = (read_data >> 1) & 0x7;

err_done:
	spidev_release_kern();
	if (status) {
		return scnprintf(buf, PAGE_SIZE, "get sqi status err: %d\n", status);
	}
	return scnprintf(buf, PAGE_SIZE, "SQI status: 0x%x\n", sqi_status);
}
static DRIVER_ATTR_RO(phy_bcm89272_port0_sqi_status);

static ssize_t phy_bcm89272_port0_work_mode_show(struct device_driver *drv, char *buf)
{
	unsigned short read_data;
	unsigned int read_addr = 0x49021068, mode;
	int status;

	status = spidev_open_kern();
	if (status) {
		return scnprintf(buf, PAGE_SIZE, "spidev_open_kern fail: %d\n", status);
	}

	status = spi_read16(read_addr, &read_data);
	if (status) {
		spidev_release_kern();
		return scnprintf(buf, PAGE_SIZE, "spi_read16 fail: %d\n", status);
	}

	mode = GET_BIT(read_data, 14);

	spidev_release_kern();

	if (mode == 1) {
		return scnprintf(buf, PAGE_SIZE, "work mode: master\n");
	} else if (mode == 0) {
		return scnprintf(buf, PAGE_SIZE, "work mode: slave\n");
	} else {
		return scnprintf(buf, PAGE_SIZE, "work mode unknow: 0x%x\n", mode);
	}
}

static ssize_t phy_bcm89272_port0_work_mode_store(struct device_driver *drv, const char *buf, size_t count)
{
	unsigned short data;
	unsigned int addr = 0x49021068;
	int status;

	status = spidev_open_kern();
	if (status) {
		goto err_done;
	}

	status = spi_read16(addr, &data);
	if (status) {
		goto err_done;
	}

	if (buf[0] == '1') {
		SET_BIT_ENABLE(data, 14);
	} else if (buf[0] == '0') {
		SET_BIT_DISABLE(data, 14);
	}
	status = spi_write16(addr, data);
	if (status) {
		goto err_done;
	}

err_done:
	spidev_release_kern();
	return count;
}
static DRIVER_ATTR_RW(phy_bcm89272_port0_work_mode);

static ssize_t phy_bcm89272_port3_link_status_show(struct device_driver *drv, char *buf)
{
	unsigned short read_data;
	unsigned int read_addr = 0x4B000100;
	int status, link_status = 0;

	status = spidev_open_kern();
	if (status) {
		return scnprintf(buf, PAGE_SIZE, "spidev_open_kern fail: %d\n", status);
	}

	// // read 0x4B000100
	status = spi_read16(read_addr, (&read_data));
	if (status) {
		goto err_done;
	}

	status = spidev_release_kern();
	if (status) {
		return scnprintf(buf, PAGE_SIZE, "spidev_release_kern fail: %d\n", status);
	}
	link_status = GET_BIT(read_data, 3);
	if (link_status == ACD_PHY_LINK_LINKED) {
		return scnprintf(buf, PAGE_SIZE, "link status: linked, data: 0x%x\n", link_status);
	} else {
		return scnprintf(buf, PAGE_SIZE, "link status: unlink, data: 0x%x\n", link_status);
	}

err_done:
	spidev_release_kern();
	return scnprintf(buf, PAGE_SIZE, "show link status fail: %d\n", link_status);
}
static DRIVER_ATTR_RO(phy_bcm89272_port3_link_status);

static ssize_t phy_bcm89272_port3_sqi_status_show(struct device_driver *drv, char *buf)
{
	unsigned short read_data, data;
	unsigned int read_addr = 0x49CF22E8, addr = 0x49CF2050;
	unsigned int sqi_status;
	int status = 0;

	status = spidev_open_kern();
	if (status) {
		return scnprintf(buf, PAGE_SIZE, "spidev_open_kern fail: %d\n", status);
	}

	status = spi_read16(addr, &data);
	if (status) {
		goto err_done;
	}
	data = 0x0C30; // enable DSP clock
	status = spi_write16(addr, data);
	if (status) {
		goto err_done;
	}

	status = spi_read16(read_addr, &read_data);
	if (status) {
		goto err_done;
	}
	sqi_status = (read_data >> 1) & 0x7;

err_done:
	spidev_release_kern();
	if (status) {
		return scnprintf(buf, PAGE_SIZE, "get sqi status err: %d\n", status);
	}
	return scnprintf(buf, PAGE_SIZE, "SQI status: 0x%x\n", sqi_status);
}
static DRIVER_ATTR_RO(phy_bcm89272_port3_sqi_status);

static ssize_t phy_bcm89272_port3_work_mode_show(struct device_driver *drv, char *buf)
{
	unsigned short read_data;
	unsigned int read_addr = 0x49C21068, mode;
	int status;

	status = spidev_open_kern();
	if (status) {
		return scnprintf(buf, PAGE_SIZE, "spidev_open_kern fail: %d\n", status);
	}

	status = spi_read16(read_addr, &read_data);
	if (status) {
		spidev_release_kern();
		return scnprintf(buf, PAGE_SIZE, "spi_read16 fail: %d\n", status);
	}

	mode = GET_BIT(read_data, 14);

	spidev_release_kern();

	if (mode == 1) {
		return scnprintf(buf, PAGE_SIZE, "work mode: master\n");
	} else if (mode == 0) {
		return scnprintf(buf, PAGE_SIZE, "work mode: slave\n");
	} else {
		return scnprintf(buf, PAGE_SIZE, "work mode unknow: 0x%x\n", mode);
	}
}

static ssize_t phy_bcm89272_port3_work_mode_store(struct device_driver *drv, const char *buf, size_t count)
{
	unsigned short data;
	unsigned int addr = 0x49C21068;
	int status;

	status = spidev_open_kern();
	if (status) {
		goto err_done;
	}

	status = spi_read16(addr, &data);
	if (status) {
		goto err_done;
	}

	if (buf[0] == '1') {
		SET_BIT_ENABLE(data, 14);
	} else if (buf[0] == '0') {
		SET_BIT_DISABLE(data, 14);
	}
	status = spi_write16(addr, data);
	if (status) {
		goto err_done;
	}

err_done:
	spidev_release_kern();
	return count;
}
static DRIVER_ATTR_RW(phy_bcm89272_port3_work_mode);

/*
 * test mode for port[0-5]
 */
static ssize_t phy_bcm89272_test_for_1G(struct device_driver *drv, const char *buf, size_t count, int testMode)
{
	// 1G port only
	unsigned short data;
	unsigned int addr;
	int status;

	if (buf[0] != '0') {
		return count;
	}

	status = spidev_open_kern();
	if (status) {
		goto err_done;
	}

	// DSP_TOP_PHSHFT_CONTROL
	addr = 0x49030A14;
	status = spi_read16(addr, &data);
	if (status) {
		goto err_done;
	}
	SET_BIT_ENABLE(data, 6);
	status = spi_write16(addr, data);
	if (status) {
		goto err_done;
	}

	// AUTONEG_IEEE_AUTONEG_BASET1_AN_CONTROL
	addr = 0x490E0400;
	data = 0;
	status = spi_write16(addr, data);
	if (status) {
		goto err_done;
	}

	// LINK_SYNC_CONTROL_A
	addr = 0x49031E04;
	status = spi_read16(addr, &data);
	if (status) {
		goto err_done;
	}
	SET_BIT_ENABLE(data, 0);
	SET_BIT_ENABLE(data, 1);
	status = spi_write16(addr, data);
	if (status) {
		goto err_done;
	}

	// PMA_PMD_IEEE_BASET1_PMA_PMD_CONTROL
	addr = 0x49021068;
	status = spi_read16(addr, &data);
	if (status) {
		goto err_done;
	}
	SET_BIT_ENABLE(data, 0);
	if (testMode != TestMode_IB) {
		SET_BIT_ENABLE(data, 14);
	}
	SET_BIT_ENABLE(data, 15);
	status = spi_write16(addr, data);
	if (status) {
		goto err_done;
	}

	if (testMode != TestMode_IB) {
		// PMA_PMD_IEEE_BASE1000T1_TEST_MODE_CONTROL
		addr = 0x49021208;
		status = spi_read16(addr, &data);
		if (status) {
			goto err_done;
		}

		switch(testMode)
		{
		case TestMode_2:
			SET_BIT_ENABLE(data, 14);
			break;
		case TestMode_4:
			SET_BIT_ENABLE(data, 15);
			break;
		case TestMode_5:
			SET_BIT_ENABLE(data, 13);
			SET_BIT_ENABLE(data, 15);
			break;
		case TestMode_6:
			SET_BIT_ENABLE(data, 14);
			SET_BIT_ENABLE(data, 15);
			break;
		default:
			goto err_done;
		}

		status = spi_write16(addr, data);
		if (status) {
			goto err_done;
		}
	}

err_done:
	spidev_release_kern();
	return count;
}

static ssize_t phy_bcm89272_test_tvco_for_1G(struct device_driver *drv, const char *buf, size_t count)
{
	unsigned short data;
	unsigned int addr;
	int status;

	status = spidev_open_kern();
	if (status) {
		goto err_done;
	}

	// WDT_WdogControl: Disable Watchdog first
	addr = 0x40145008;
	data = 0;
	status = spi_write16(addr, data);
	if (status) {
		goto err_done;
	}

	// AFE_DIG_PLL_TEST
	addr = 0x49038060;
	status = spi_read16(addr, &data);
	if (status) {
		goto err_done;
	}

	SET_BIT_ENABLE(data, 0);
	SET_BIT_ENABLE(data, 1);

	status = spi_write16(addr, data);
	if (status) {
		goto err_done;
	}

	// PMA_PMD_IEEE_CONTROL_REG1
	addr = 0x49020000;
	status = spi_read16(addr, &data);
	if (status) {
		goto err_done;
	}

	SET_BIT_ENABLE(data, 1);
	SET_BIT_ENABLE(data, 6);

	status = spi_write16(addr, data);
	if (status) {
		goto err_done;
	}

err_done:
	spidev_release_kern();
	return count;
}

static ssize_t phy_bcm89272_test_mode1_store(struct device_driver *drv, const char *buf, size_t count)
{
	unsigned short data;
	unsigned int addr1, addr2; // BRPHYx_CL45DEV7_AUTONEG_BASET1_AN_CONTROL & BRPHYx_CL45DEV1_PMD_IEEE_TEST_T1
	int status;

	switch (buf[0])
	{
	case '1':
		addr1 = 0x494E0400;
		addr2 = 0x4942106C;
		break;
	case '2':
		addr1 = 0x498E0400;
		addr2 = 0x4982106C;
		break;
	case '3':
		addr1 = 0x49CE0400;
		addr2 = 0x49C2106C;
		break;
	case '4':
		addr1 = 0x4A0E0400;
		addr2 = 0x4A02106C;
		break;
	case '5':
		addr1 = 0x4A4E0400;
		addr2 = 0x4A42106C;
		break;

	default:
		return count;
	}

	status = spidev_open_kern();
	if (status) {
		goto err_done;
	}

	data = 0;
	status = spi_write16(addr1, data);
	if (status) {
		goto err_done;
	}

	status = spi_read16(addr2, &data);
	if (status) {
		goto err_done;
	}

	SET_BIT_ENABLE(data, 13);

	status = spi_write16(addr2, data);
	if (status) {
		goto err_done;
	}

err_done:
	spidev_release_kern();
	return count;
}
static DRIVER_ATTR_WO(phy_bcm89272_test_mode1);

static ssize_t phy_bcm89272_test_mode2_store(struct device_driver *drv, const char *buf, size_t count)
{
	unsigned short data;
	unsigned int addr1, addr2; // BRPHYx_CL45DEV7_AUTONEG_BASET1_AN_CONTROL & BRPHYx_CL45DEV1_PMD_IEEE_TEST_T1
	int status;

	switch (buf[0])
	{
	case '0':
		return phy_bcm89272_test_for_1G(drv, buf, count, TestMode_2);
	case '1':
		addr1 = 0x494E0400;
		addr2 = 0x4942106C;
		break;
	case '2':
		addr1 = 0x498E0400;
		addr2 = 0x4982106C;
		break;
	case '3':
		addr1 = 0x49CE0400;
		addr2 = 0x49C2106C;
		break;
	case '4':
		addr1 = 0x4A0E0400;
		addr2 = 0x4A02106C;
		break;
	case '5':
		addr1 = 0x4A4E0400;
		addr2 = 0x4A42106C;
		break;

	default:
		return count;
	}

	status = spidev_open_kern();
	if (status) {
		goto err_done;
	}

	data = 0;
	status = spi_write16(addr1, data);
	if (status) {
		goto err_done;
	}

	status = spi_read16(addr2, &data);
	if (status) {
		goto err_done;
	}

	SET_BIT_ENABLE(data, 14);

	status = spi_write16(addr2, data);
	if (status) {
		goto err_done;
	}

err_done:
	spidev_release_kern();
	return count;
}
static DRIVER_ATTR_WO(phy_bcm89272_test_mode2);

static ssize_t phy_bcm89272_test_mode4_store(struct device_driver *drv, const char *buf, size_t count)
{
	unsigned short data;
	unsigned int addr1, addr2; // BRPHYx_CL45DEV7_AUTONEG_BASET1_AN_CONTROL & BRPHYx_CL45DEV1_PMD_IEEE_TEST_T1
	int status;

	switch (buf[0])
	{
	case '0':
		return phy_bcm89272_test_for_1G(drv, buf, count, TestMode_4);
	case '1':
		addr1 = 0x494E0400;
		addr2 = 0x4942106C;
		break;
	case '2':
		addr1 = 0x498E0400;
		addr2 = 0x4982106C;
		break;
	case '3':
		addr1 = 0x49CE0400;
		addr2 = 0x49C2106C;
		break;
	case '4':
		addr1 = 0x4A0E0400;
		addr2 = 0x4A02106C;
		break;
	case '5':
		addr1 = 0x4A4E0400;
		addr2 = 0x4A42106C;
		break;

	default:
		return count;
	}

	status = spidev_open_kern();
	if (status) {
		goto err_done;
	}

	data = 0;
	status = spi_write16(addr1, data);
	if (status) {
		goto err_done;
	}

	status = spi_read16(addr2, &data);
	if (status) {
		goto err_done;
	}

	SET_BIT_ENABLE(data, 15);

	status = spi_write16(addr2, data);
	if (status) {
		goto err_done;
	}

err_done:
	spidev_release_kern();
	return count;
}
static DRIVER_ATTR_WO(phy_bcm89272_test_mode4);

static ssize_t phy_bcm89272_test_mode5_store(struct device_driver *drv, const char *buf, size_t count)
{
	unsigned short data;
	unsigned int addr1, addr2; // BRPHYx_CL45DEV7_AUTONEG_BASET1_AN_CONTROL & BRPHYx_CL45DEV1_PMD_IEEE_TEST_T1
	int status;

	switch (buf[0])
	{
	case '0':
		return phy_bcm89272_test_for_1G(drv, buf, count, TestMode_5);
	case '1':
		addr1 = 0x494E0400;
		addr2 = 0x4942106C;
		break;
	case '2':
		addr1 = 0x498E0400;
		addr2 = 0x4982106C;
		break;
	case '3':
		addr1 = 0x49CE0400;
		addr2 = 0x49C2106C;
		break;
	case '4':
		addr1 = 0x4A0E0400;
		addr2 = 0x4A02106C;
		break;
	case '5':
		addr1 = 0x4A4E0400;
		addr2 = 0x4A42106C;
		break;

	default:
		return count;
	}

	status = spidev_open_kern();
	if (status) {
		goto err_done;
	}

	data = 0;
	status = spi_write16(addr1, data);
	if (status) {
		goto err_done;
	}

	status = spi_read16(addr2, &data);
	if (status) {
		goto err_done;
	}

	SET_BIT_ENABLE(data, 13);
	SET_BIT_ENABLE(data, 15);

	status = spi_write16(addr2, data);
	if (status) {
		goto err_done;
	}

err_done:
	spidev_release_kern();
	return count;
}
static DRIVER_ATTR_WO(phy_bcm89272_test_mode5);

static ssize_t phy_bcm89272_test_mode6_store(struct device_driver *drv, const char *buf, size_t count)
{
	return phy_bcm89272_test_for_1G(drv, buf, count, TestMode_6);
}
static DRIVER_ATTR_WO(phy_bcm89272_test_mode6);

static ssize_t phy_bcm89272_test_tvco_store(struct device_driver *drv, const char *buf, size_t count)
{
	unsigned short data;
	unsigned int addr = 0x4A4F2022; // BRPHY0_GPHY_CORE_SHD1C_01_ Px
	int status;

	status = spidev_open_kern();
	if (status) {
		goto err_done;
	}

	status = spi_read16(addr, &data);
	if (status) {
		goto err_done;
	}

	switch (buf[0])
	{
	case '0':
		return phy_bcm89272_test_tvco_for_1G(drv, buf, count);
	case '1':
		SET_BIT_ENABLE(data, 0);
		SET_BIT_ENABLE(data, 2);
		SET_BIT_ENABLE(data, 4);
		SET_BIT_ENABLE(data, 10);
		break;
	case '2':
		SET_BIT_ENABLE(data, 0);
		SET_BIT_ENABLE(data, 2);
		SET_BIT_ENABLE(data, 5);
		SET_BIT_ENABLE(data, 10);
		break;
	case '3':
		SET_BIT_ENABLE(data, 0);
		SET_BIT_ENABLE(data, 2);
		SET_BIT_ENABLE(data, 4);
		SET_BIT_ENABLE(data, 5);
		SET_BIT_ENABLE(data, 10);
		break;
	case '4':
		SET_BIT_ENABLE(data, 0);
		SET_BIT_ENABLE(data, 2);
		SET_BIT_ENABLE(data, 6);
		SET_BIT_ENABLE(data, 10);
		break;
	case '5':
		SET_BIT_ENABLE(data, 0);
		SET_BIT_ENABLE(data, 2);
		SET_BIT_ENABLE(data, 10);
		break;

	default:
		goto err_done;
	}

	status = spi_write16(addr, data);
	if (status) {
		goto err_done;
	}

err_done:
	spidev_release_kern();
	return count;
}
static DRIVER_ATTR_WO(phy_bcm89272_test_tvco);

static ssize_t phy_bcm89272_test_impedance_balance_store(struct device_driver *drv, const char *buf, size_t count)
{
	return phy_bcm89272_test_for_1G(drv, buf, count, TestMode_IB);
}
static DRIVER_ATTR_WO(phy_bcm89272_test_impedance_balance);

static ssize_t phy_bcm89272_PN_polarity_show(struct device_driver *drv, char *buf)
{
	unsigned short read_data;
	unsigned int read_addr = 0x49021202, mode;
	int status;

	status = spidev_open_kern();
	if (status) {
		return scnprintf(buf, PAGE_SIZE, "spidev_open_kern fail: %d\n", status);
	}

	status = spi_read16(read_addr, &read_data);
	if (status) {
		spidev_release_kern();
		return scnprintf(buf, PAGE_SIZE, "spi_read16 fail: %d\n", status);
	}

	mode = GET_BIT(read_data, 2);

	spidev_release_kern();

	if (mode == 1) {
		return scnprintf(buf, PAGE_SIZE, "PN polarity: reversed\n");
	} else {
		return scnprintf(buf, PAGE_SIZE, "PN polarity: unreversed\n");
	}
}
static DRIVER_ATTR_RO(phy_bcm89272_PN_polarity);

static ssize_t phy_bcm89272_loopback_state_show(struct device_driver *drv, char *buf)
{
	unsigned short read_data;
	unsigned int read_addr = 0x49060000, state;
	int status;

	status = spidev_open_kern();
	if (status) {
		return scnprintf(buf, PAGE_SIZE, "spidev_open_kern fail: %d\n", status);
	}

	status = spi_read16(read_addr, &read_data);
	if (status) {
		spidev_release_kern();
		return scnprintf(buf, PAGE_SIZE, "spi_read16 fail: %d\n", status);
	}

	state = GET_BIT(read_data, 14);

	spidev_release_kern();

	if (state == 1) {
		return scnprintf(buf, PAGE_SIZE, "loopback: Enable\n");
	} else {
		return scnprintf(buf, PAGE_SIZE, "loopback: Disable\n");
	}
}

static ssize_t phy_bcm89272_loopback_state_store(struct device_driver *drv, const char *buf, size_t count)
{
	unsigned short data;
	unsigned int addr = 0x49060000;
	int status;

	status = spidev_open_kern();
	if (status) {
		goto err_done;
	}

	status = spi_read16(addr, &data);
	if (status) {
		goto err_done;
	}

	if (buf[0] == '1') {
		SET_BIT_ENABLE(data, 14);
	} else if (buf[0] == '0') {
		SET_BIT_DISABLE(data, 14);
	}
	status = spi_write16(addr, data);
	if (status) {
		goto err_done;
	}

err_done:
	spidev_release_kern();
	return count;
}
static DRIVER_ATTR_RW(phy_bcm89272_loopback_state);

static ssize_t phy_bcm89272_external_loopback_state_show(struct device_driver *drv, char *buf)
{
	unsigned short read_data;
	unsigned int read_addr = 0x49032650, state;
	int status;

	status = spidev_open_kern();
	if (status) {
		return scnprintf(buf, PAGE_SIZE, "spidev_open_kern fail: %d\n", status);
	}

	status = spi_read16(read_addr, &read_data);
	if (status) {
		spidev_release_kern();
		return scnprintf(buf, PAGE_SIZE, "spi_read16 fail: %d\n", status);
	}

	state = GET_BIT(read_data, 15);

	spidev_release_kern();

	if (state == 1) {
		return scnprintf(buf, PAGE_SIZE, "external loopback: Enable\n");
	} else {
		return scnprintf(buf, PAGE_SIZE, "external loopback: Disable\n");
	}
}

static ssize_t phy_bcm89272_external_loopback_state_store(struct device_driver *drv, const char *buf, size_t count)
{
	unsigned short data;
	unsigned int addr = 0x49032650;
	int status;

	status = spidev_open_kern();
	if (status) {
		goto err_done;
	}

	status = spi_read16(addr, &data);
	if (status) {
		goto err_done;
	}

	if (buf[0] == '1') {
		SET_BIT_ENABLE(data, 15);
	} else if (buf[0] == '0') {
		SET_BIT_DISABLE(data, 15);
	}
	status = spi_write16(addr, data);
	if (status) {
		goto err_done;
	}

err_done:
	spidev_release_kern();
	return count;
}
static DRIVER_ATTR_RW(phy_bcm89272_external_loopback_state);

static int diagnosis_sysfs_init(void) {
	int status;
	status = driver_create_file(&(spidev_spi_driver.driver), &driver_attr_phy_bcm89272_port0_link_status);
	if (status < 0) {
		goto err_done;
	}
	status = driver_create_file(&(spidev_spi_driver.driver), &driver_attr_phy_bcm89272_port0_sqi_status);
	if (status < 0) {
		goto err_done;
	}
	status = driver_create_file(&(spidev_spi_driver.driver), &driver_attr_phy_bcm89272_port0_work_mode);
	if (status < 0) {
		goto err_done;
	}
	status = driver_create_file(&(spidev_spi_driver.driver), &driver_attr_phy_bcm89272_port3_link_status);
	if (status < 0) {
		goto err_done;
	}
	status = driver_create_file(&(spidev_spi_driver.driver), &driver_attr_phy_bcm89272_port3_sqi_status);
	if (status < 0) {
		goto err_done;
	}
	status = driver_create_file(&(spidev_spi_driver.driver), &driver_attr_phy_bcm89272_port3_work_mode);
		if (status < 0) {
		goto err_done;
	}
	status = driver_create_file(&(spidev_spi_driver.driver), &driver_attr_phy_bcm89272_test_mode1);
	if (status < 0) {
		goto err_done;
	}
	status = driver_create_file(&(spidev_spi_driver.driver), &driver_attr_phy_bcm89272_test_mode2);
	if (status < 0) {
		goto err_done;
	}
	status = driver_create_file(&(spidev_spi_driver.driver), &driver_attr_phy_bcm89272_test_mode4);
	if (status < 0) {
		goto err_done;
	}
	status = driver_create_file(&(spidev_spi_driver.driver), &driver_attr_phy_bcm89272_test_mode5);
	if (status < 0) {
		goto err_done;
	}
	status = driver_create_file(&(spidev_spi_driver.driver), &driver_attr_phy_bcm89272_test_mode6);
	if (status < 0) {
		goto err_done;
	}
	status = driver_create_file(&(spidev_spi_driver.driver), &driver_attr_phy_bcm89272_test_tvco);
	if (status < 0) {
		goto err_done;
	}
	status = driver_create_file(&(spidev_spi_driver.driver), &driver_attr_phy_bcm89272_test_impedance_balance);
	if (status < 0) {
		goto err_done;
	}
	status = driver_create_file(&(spidev_spi_driver.driver), &driver_attr_phy_bcm89272_port0_dut_status);
	if (status < 0) {
		goto err_done;
	}
	status = driver_create_file(&(spidev_spi_driver.driver), &driver_attr_phy_bcm89272_port3_dut_status);
	if (status < 0) {
		goto err_done;
	}
	status = driver_create_file(&(spidev_spi_driver.driver), &driver_attr_phy_bcm89272_PN_polarity);
	if (status < 0) {
		goto err_done;
	}
	status = driver_create_file(&(spidev_spi_driver.driver), &driver_attr_phy_bcm89272_loopback_state);
	if (status < 0) {
		goto err_done;
	}
	status = driver_create_file(&(spidev_spi_driver.driver), &driver_attr_phy_bcm89272_external_loopback_state);
err_done:
	if (status < 0) {
		status = -ENOENT;
	}
	return status;
}
/*---------------------------- sysfs operation end ------------------------------*/
#endif

static int __init spidev_init(void)
{
	int status;

	/* Claim our 256 reserved device numbers.  Then register a class
	 * that will key udev/mdev to add/remove /dev nodes.  Last, register
	 * the driver which manages those device numbers.
	 */
	status = register_chrdev(SPIDEV_MAJOR, "spi", &spidev_fops);
	if (status < 0)
		return status;

	spidev_class = class_create(THIS_MODULE, "spidev");
	if (IS_ERR(spidev_class)) {
		unregister_chrdev(SPIDEV_MAJOR, spidev_spi_driver.driver.name);
		return PTR_ERR(spidev_class);
	}

	status = spi_register_driver(&spidev_spi_driver);
	if (status < 0) {
		class_destroy(spidev_class);
		unregister_chrdev(SPIDEV_MAJOR, spidev_spi_driver.driver.name);
	}

#if IS_ENABLED(CONFIG_BRCM_XGBE)
	status = diagnosis_sysfs_init();
#endif
	return status;
}
module_init(spidev_init);

static void __exit spidev_exit(void)
{
	spi_unregister_driver(&spidev_spi_driver);
	class_destroy(spidev_class);
	unregister_chrdev(SPIDEV_MAJOR, spidev_spi_driver.driver.name);
}
module_exit(spidev_exit);

MODULE_AUTHOR("Andrea Paterniani, <a.paterniani@swapp-eng.it>");
MODULE_DESCRIPTION("User mode SPI device interface");
MODULE_LICENSE("GPL");
MODULE_ALIAS("spi:spidev");
