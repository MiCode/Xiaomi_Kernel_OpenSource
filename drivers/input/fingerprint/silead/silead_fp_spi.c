/*
 * @file   silead_fp_spi.c
 * @brief  silead spi device driver usually used in REE environment.
 *
 *
 * Copyright 2016-2021 Gigadevice/Silead Inc.
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 *
 * ------------------- Revision History ------------------------------
 * <author>      <date>   	   <version>     <desc>
 * Melvin cao    2021/01/29    0.1.0      	 Init version
 * Melvin cao    2021/04/06    0.1.1      	 Mode and speed etcs, set through apps
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/compat.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/pinctrl/consumer.h>
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>
#include <linux/uaccess.h>

#include "silead_fp.h"
#if defined(CONFIG_SILEAD_FP_PLATFORM) && defined(BSP_SIL_PLAT_MTK)
#ifdef CONFIG_MTK_CLKMGR
#include "mach/mt_clkmgr.h"
#else
#include <linux/clk.h>
#endif

/* MTK header */
#ifndef CONFIG_SPI_MT65XX
#include "mtk_spi.h"
#include "mtk_spi_hal.h"
#endif

#ifdef CONFIG_SPI_MT65XX
extern void mt_spi_enable_master_clk(struct spi_device *spidev);
extern void mt_spi_disable_master_clk(struct spi_device *spidev);
#endif
#endif

/* debug log level */
extern fp_debug_level_t sil_debug_level;

#ifdef SILFP_SPI_REE

#define SILEAD_SPI_VERSION "0.1.1"

#define SPI_MODE_MASK    (SPI_CPHA | SPI_CPOL | SPI_CS_HIGH \
                | SPI_LSB_FIRST | SPI_3WIRE | SPI_LOOP \
                | SPI_NO_CS | SPI_READY | SPI_TX_DUAL \
                | SPI_TX_QUAD | SPI_RX_DUAL | SPI_RX_QUAD)

struct silfp_ree_data {
    dev_t               devt;
    spinlock_t          spi_lock;
    struct spi_device   *spi;

    /* TX/RX buffers are NULL unless this device is open (users > 0) */
    struct mutex        buf_lock;
    unsigned            users;
    u8                  *tx_buffer;
    u8                  *rx_buffer;
    u32                 speed_hz;
    struct pinctrl      *pinctrl_gpios;
    struct pinctrl_state *pins_default;
};

static unsigned silfp_ree_bufsiz = 1*1024*1024;
static struct silfp_ree_data *m_ree_data = NULL;
struct device *m_ree_device;

module_param(silfp_ree_bufsiz, uint, S_IRUGO);
MODULE_PARM_DESC(silfp_ree_bufsiz, "data bytes in biggest supported SILFP REE SPI message");
static int silfp_ree_major;

/*-------------------------------------------------------------------------*/

static int silfp_ree_parse_spi_dts(struct silfp_ree_data* ree_data)
{
    struct device_node *node;
    struct platform_device *pdev = NULL;
    int ret = 0;

    do {
        node = of_find_compatible_node(NULL, NULL, "mediatek,spidev-pins");
        if (!node) {
            LOG_MSG_DEBUG(INFO_LOG, "platform device is null\n");
            ret = -1;
            break;
        }

        pdev = of_find_device_by_node(node);
        if (!pdev) {
            ret = -1;
            LOG_MSG_DEBUG(INFO_LOG, "platform device is null\n");
            break;
        }

        ree_data->pinctrl_gpios = devm_pinctrl_get(&pdev->dev);
        if (IS_ERR(ree_data->pinctrl_gpios)) {
            ret = -1;
            LOG_MSG_DEBUG(INFO_LOG, "can't find ree pinctrl\n");
            break;
        }
    } while (0);

    if (ret < 0) {
        LOG_MSG_DEBUG(ERR_LOG, "parse dts failed\n");
        return ret;
    }

    ree_data->pins_default = pinctrl_lookup_state(ree_data->pinctrl_gpios, "default");
    if (IS_ERR(ree_data->pins_default)) {
        ret = PTR_ERR(ree_data->pins_default);
        LOG_MSG_DEBUG(INFO_LOG, "%s can't find ree pinctrl default\n", __func__);
        return ret;
    }
    pinctrl_select_state(ree_data->pinctrl_gpios, ree_data->pins_default);

    LOG_MSG_DEBUG(INFO_LOG, "%s, get pinctrl success!\n", __func__);
    return 0;
}
static ssize_t
silfp_ree_sync(struct silfp_ree_data *ree_data, struct spi_message *message)
{
    int status;
    struct spi_device *spi;

    spin_lock_irq(&ree_data->spi_lock);
    spi = ree_data->spi;
    spin_unlock_irq(&ree_data->spi_lock);
    if (spi == NULL)
        status = -ESHUTDOWN;
    else
        status = spi_sync(spi, message);

    if (status == 0)
        status = message->actual_length;

    return status;
}

static inline ssize_t
silfp_ree_sync_write(struct silfp_ree_data *ree_data, size_t len)
{
    struct spi_transfer    t = {
        .tx_buf        = ree_data->tx_buffer,
        .len        = len,
        .speed_hz    = ree_data->speed_hz,
    };
    struct spi_message    m;

    spi_message_init(&m);
    spi_message_add_tail(&t, &m);
    return silfp_ree_sync(ree_data, &m);
}

static inline ssize_t
silfp_ree_sync_read(struct silfp_ree_data *ree_dev, size_t len)
{
    struct spi_transfer    t = {
        .rx_buf        = ree_dev->rx_buffer,
        .len        = len,
        .speed_hz    = ree_dev->speed_hz,
    };
    struct spi_message    m;

    spi_message_init(&m);
    spi_message_add_tail(&t, &m);
    return silfp_ree_sync(ree_dev, &m);
}

/*-------------------------------------------------------------------------*/

/* Read-only message with current device setup */
static ssize_t
silfp_ree_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    struct silfp_ree_data    *ree_data;
    ssize_t            status = 0;

    /* chipselect only toggles at start or end of operation */
    if (count > silfp_ree_bufsiz)
        return -EMSGSIZE;

    ree_data = filp->private_data;

    mutex_lock(&ree_data->buf_lock);
    status = silfp_ree_sync_read(ree_data, count);
    if (status > 0) {
        unsigned long    missing;

        missing = copy_to_user(buf, ree_data->rx_buffer, status);
        if (missing == status)
            status = -EFAULT;
        else
            status = status - missing;
    }
    mutex_unlock(&ree_data->buf_lock);

    return status;
}

/* Write-only message with current device setup */
static ssize_t
silfp_ree_write(struct file *filp, const char __user *buf,
                size_t count, loff_t *f_pos)
{
    struct silfp_ree_data    *ree_data;
    ssize_t            status = 0;
    unsigned long        missing;

    /* chipselect only toggles at start or end of operation */
    if (count > silfp_ree_bufsiz)
        return -EMSGSIZE;

    ree_data = filp->private_data;

    mutex_lock(&ree_data->buf_lock);
    missing = copy_from_user(ree_data->tx_buffer, buf, count);
    if (missing == 0)
        status = silfp_ree_sync_write(ree_data, count);
    else
        status = -EFAULT;
    mutex_unlock(&ree_data->buf_lock);

    return status;
}

static int silfp_ree_message(struct silfp_ree_data *ree_data,
                             struct spi_ioc_transfer *u_xfers, unsigned n_xfers)
{
    struct spi_message    msg;
    struct spi_transfer    *k_xfers;
    struct spi_transfer    *k_tmp;
    struct spi_ioc_transfer *u_tmp;
    unsigned        n, total, tx_total, rx_total;
    u8            *tx_buf, *rx_buf;
    int            status = -EFAULT;

    spi_message_init(&msg);
    k_xfers = kcalloc(n_xfers, sizeof(*k_tmp), GFP_KERNEL);
    if (k_xfers == NULL)
        return -ENOMEM;

    /* Construct spi_message, copying any tx data to bounce buffer.
     * We walk the array of user-provided transfers, using each one
     * to initialize a kernel version of the same transfer.
     */
    tx_buf = ree_data->tx_buffer;
    rx_buf = ree_data->rx_buffer;
    total = 0;
    tx_total = 0;
    rx_total = 0;
    for (n = n_xfers, k_tmp = k_xfers, u_tmp = u_xfers;
         n;
         n--, k_tmp++, u_tmp++) {
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
            if (rx_total > silfp_ree_bufsiz) {
                status = -EMSGSIZE;
                goto done;
            }
            k_tmp->rx_buf = rx_buf;
            if (!access_ok(VERIFY_WRITE, (u8 __user *)
                           (uintptr_t) u_tmp->rx_buf,
                           u_tmp->len))
                goto done;
            rx_buf += k_tmp->len;
        }
        if (u_tmp->tx_buf) {
            /* this transfer needs space in TX bounce buffer */
            tx_total += k_tmp->len;
            if (tx_total > silfp_ree_bufsiz) {
                status = -EMSGSIZE;
                goto done;
            }
            k_tmp->tx_buf = tx_buf;
            if (copy_from_user(tx_buf, (const u8 __user *)
                               (uintptr_t) u_tmp->tx_buf,
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
            k_tmp->speed_hz = ree_data->speed_hz;
#ifdef SILFP_REE_VERBOSE
        LOG_MSG_DEBUG(INFO_LOG, &ree_data->spi->dev,
                      "  xfer len %u %s%s%s%dbits %u usec %uHz\n",
                      u_tmp->len,
                      u_tmp->rx_buf ? "rx " : "",
                      u_tmp->tx_buf ? "tx " : "",
                      u_tmp->cs_change ? "cs " : "",
                      u_tmp->bits_per_word ? : ree_data->spi->bits_per_word,
                      u_tmp->delay_usecs,
                      u_tmp->speed_hz ? : ree_data->spi->max_speed_hz);
#endif
        spi_message_add_tail(k_tmp, &msg);
    }

    status = silfp_ree_sync(ree_data, &msg);
    if (status < 0)
        goto done;

    /* copy any rx data out of bounce buffer */
    rx_buf = ree_data->rx_buffer;
    for (n = n_xfers, u_tmp = u_xfers; n; n--, u_tmp++) {
        if (u_tmp->rx_buf) {
            if (copy_to_user((u8 __user *)
                             (uintptr_t) u_tmp->rx_buf, rx_buf,
                             u_tmp->len)) {
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

static struct spi_ioc_transfer *
silfp_ree_get_ioc_message(unsigned int cmd, struct spi_ioc_transfer __user *u_ioc,
                          unsigned *n_ioc)
{
    struct spi_ioc_transfer    *ioc;
    u32    tmp;

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
    ioc = kmalloc(tmp, GFP_KERNEL);
    if (!ioc)
        return ERR_PTR(-ENOMEM);

    if (__copy_from_user(ioc, u_ioc, tmp)) {
        kfree(ioc);
        return ERR_PTR(-EFAULT);
    }
    return ioc;
}

static long
silfp_ree_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    int            err = 0;
    int            retval = 0;
    struct silfp_ree_data    *ree_data;
    struct spi_device    *spi;
    u32            tmp;
    unsigned        n_ioc;
    struct spi_ioc_transfer    *ioc;

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
    ree_data = filp->private_data;
    spin_lock_irq(&ree_data->spi_lock);
    spi = spi_dev_get(ree_data->spi);
    spin_unlock_irq(&ree_data->spi_lock);

    if (spi == NULL)
        return -ESHUTDOWN;

    /* use the buffer lock here for triple duty:
     *  - prevent I/O (from us) so calling spi_setup() is safe;
     *  - prevent concurrent SPI_IOC_WR_* from morphing
     *    data fields while SPI_IOC_RD_* reads them;
     *  - SPI_IOC_MESSAGE needs the buffer locked "normally".
     */
    mutex_lock(&ree_data->buf_lock);

    switch (cmd) {
        /* read requests */
    case SPI_IOC_RD_MODE:
        retval = __put_user(spi->mode & SPI_MODE_MASK,
                            (__u8 __user *)arg);
        break;
    case SPI_IOC_RD_MODE32:
        retval = __put_user(spi->mode & SPI_MODE_MASK,
                            (__u32 __user *)arg);
        break;
    case SPI_IOC_RD_LSB_FIRST:
        retval = __put_user((spi->mode & SPI_LSB_FIRST) ?  1 : 0,
                            (__u8 __user *)arg);
        break;
    case SPI_IOC_RD_BITS_PER_WORD:
        retval = __put_user(spi->bits_per_word, (__u8 __user *)arg);
        break;
    case SPI_IOC_RD_MAX_SPEED_HZ:
        retval = __put_user(ree_data->speed_hz, (__u32 __user *)arg);
        break;

        /* write requests */
    case SPI_IOC_WR_MODE:
    case SPI_IOC_WR_MODE32:
        if (cmd == SPI_IOC_WR_MODE)
            retval = __get_user(tmp, (u8 __user *)arg);
        else
            retval = __get_user(tmp, (u32 __user *)arg);
        if (retval == 0) {
            u32    save = spi->mode;

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
            u32    save = spi->mode;

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
        retval = __get_user(tmp, (__u8 __user *)arg);
        if (retval == 0) {
            u8    save = spi->bits_per_word;

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
            u32    save = spi->max_speed_hz;

            spi->max_speed_hz = tmp;
            retval = spi_setup(spi);
            if (retval >= 0)
                ree_data->speed_hz = tmp;
            else
                dev_dbg(&spi->dev, "%d Hz (max)\n", tmp);
            spi->max_speed_hz = save;
        }
        break;

    default:
        /* segmented and/or full-duplex I/O request */
        /* Check message and copy into scratch area */
        ioc = silfp_ree_get_ioc_message(cmd,
                                        (struct spi_ioc_transfer __user *)arg, &n_ioc);
        if (IS_ERR(ioc)) {
            retval = PTR_ERR(ioc);
            break;
        }
        if (!ioc)
            break;    /* n_ioc is also 0 */

        /* translate to spi_message, execute */
        retval = silfp_ree_message(ree_data, ioc, n_ioc);

        kfree(ioc);
        break;
    }

    mutex_unlock(&ree_data->buf_lock);
    spi_dev_put(spi);
    return retval;
}

#ifdef CONFIG_COMPAT
static long
silfp_ree_compat_ioc_message(struct file *filp, unsigned int cmd,
                             unsigned long arg)
{
    struct spi_ioc_transfer __user    *u_ioc;
    int                retval = 0;
    struct silfp_ree_data        *ree_data;
    struct spi_device        *spi;
    unsigned            n_ioc, n;
    struct spi_ioc_transfer        *ioc;

    u_ioc = (struct spi_ioc_transfer __user *) compat_ptr(arg);
    if (!access_ok(VERIFY_READ, u_ioc, _IOC_SIZE(cmd)))
        return -EFAULT;

    /* guard against device removal before, or while,
     * we issue this ioctl.
     */
    ree_data = filp->private_data;
    spin_lock_irq(&ree_data->spi_lock);
    spi = spi_dev_get(ree_data->spi);
    spin_unlock_irq(&ree_data->spi_lock);

    if (spi == NULL)
        return -ESHUTDOWN;

    /* SPI_IOC_MESSAGE needs the buffer locked "normally" */
    mutex_lock(&ree_data->buf_lock);

    /* Check message and copy into scratch area */
    ioc = silfp_ree_get_ioc_message(cmd, u_ioc, &n_ioc);
    if (IS_ERR(ioc)) {
        retval = PTR_ERR(ioc);
        goto done;
    }
    if (!ioc)
        goto done;    /* n_ioc is also 0 */

    /* Convert buffer pointers */
    for (n = 0; n < n_ioc; n++) {
        ioc[n].rx_buf = (uintptr_t) compat_ptr(ioc[n].rx_buf);
        ioc[n].tx_buf = (uintptr_t) compat_ptr(ioc[n].tx_buf);
    }

    /* translate to spi_message, execute */
    retval = silfp_ree_message(ree_data, ioc, n_ioc);
    kfree(ioc);

done:
    mutex_unlock(&ree_data->buf_lock);
    spi_dev_put(spi);
    return retval;
}

static long
silfp_ree_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    if (_IOC_TYPE(cmd) == SPI_IOC_MAGIC
        && _IOC_NR(cmd) == _IOC_NR(SPI_IOC_MESSAGE(0))
        && _IOC_DIR(cmd) == _IOC_WRITE)
        return silfp_ree_compat_ioc_message(filp, cmd, arg);

    return silfp_ree_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
}
#else
#define silfp_ree_compat_ioctl NULL
#endif /* CONFIG_COMPAT */

static int silfp_ree_open(struct inode *inode, struct file *filp)
{
    struct silfp_ree_data    *ree_data;
    int            status = -ENXIO;

    if (!m_ree_data) {
        pr_debug("ree_data: nothing for minor %d\n", iminor(inode));
        goto err_find_dev;
    }
    ree_data = m_ree_data;

    if (!ree_data->tx_buffer) {
        ree_data->tx_buffer = kmalloc(silfp_ree_bufsiz, GFP_KERNEL);
        if (!ree_data->tx_buffer) {
            dev_dbg(&ree_data->spi->dev, "open/ENOMEM\n");
            status = -ENOMEM;
            goto err_find_dev;
        }
    }

    if (!ree_data->rx_buffer) {
        ree_data->rx_buffer = kmalloc(silfp_ree_bufsiz, GFP_KERNEL);
        if (!ree_data->rx_buffer) {
            dev_dbg(&ree_data->spi->dev, "open/ENOMEM\n");
            status = -ENOMEM;
            goto err_alloc_rx_buf;
        }
    }

    ree_data->users++;
    filp->private_data = ree_data;
    nonseekable_open(inode, filp);

    return 0;

err_alloc_rx_buf:
    kfree(ree_data->tx_buffer);
    ree_data->tx_buffer = NULL;
err_find_dev:
    return status;
}

static int silfp_ree_release(struct inode *inode, struct file *filp)
{
    struct silfp_ree_data    *ree_data;

    ree_data = filp->private_data;
    filp->private_data = NULL;

    /* last close? */
    ree_data->users--;
    if (!ree_data->users) {
        int        dofree;

        kfree(ree_data->tx_buffer);
        ree_data->tx_buffer = NULL;

        kfree(ree_data->rx_buffer);
        ree_data->rx_buffer = NULL;

        spin_lock_irq(&ree_data->spi_lock);
        if (ree_data->spi)
            ree_data->speed_hz = ree_data->spi->max_speed_hz;

        /* ... after we unbound from the underlying device? */
        dofree = (ree_data->spi == NULL);
        spin_unlock_irq(&ree_data->spi_lock);

        if (dofree)
            kfree(ree_data);
    }

    return 0;
}

#if defined(CONFIG_SILEAD_FP_PLATFORM) && defined(BSP_SIL_PLAT_MTK)
void silfp_spi_clk_enable(bool bonoff)
{
#ifdef CONFIG_MTK_CLKMGR
    if (bonoff) {
        enable_clock(MT_CG_PERI_SPI0, "spi");
    } else {
        disable_clock(MT_CG_PERI_SPI0, "spi");
    }
#else
    static int count;

    if (NULL == m_ree_data->spi) {
        LOG_MSG_DEBUG(INFO_LOG, "%s, spi is null, enable or disable clk failed.", __func__);
        return;
    }
    if (bonoff && (count == 0)) {
        mt_spi_enable_master_clk(m_ree_data->spi);
        count = 1;
        LOG_MSG_DEBUG(INFO_LOG, "%s, clock enable.", __func__);
    } else if ((count > 0) && !bonoff) {
        mt_spi_disable_master_clk(m_ree_data->spi);
        count = 0;
        LOG_MSG_DEBUG(INFO_LOG, "%s, clock disable.", __func__);
    }
#endif
}
EXPORT_SYMBOL(silfp_spi_clk_enable);
#endif

static const struct file_operations silfp_ree_fops = {
    .owner =    THIS_MODULE,
    /* REVISIT switch to aio primitives, so that userspace
     * gets more complete API coverage.  It'll simplify things
     * too, except for the locking.
     */
    .write =    silfp_ree_write,
    .read =        silfp_ree_read,
    .unlocked_ioctl = silfp_ree_ioctl,
    .compat_ioctl = silfp_ree_compat_ioctl,
    .open =        silfp_ree_open,
    .release =    silfp_ree_release,
    .llseek =    no_llseek,
};

static struct class *silfp_ree_class;

#ifdef CONFIG_OF
static const struct of_device_id silfp_ree_dt_ids[] = {
    { .compatible = "sil,silfp_ree" },
#if defined(CONFIG_SILEAD_FP_PLATFORM) && defined(BSP_SIL_PLAT_MTK)
    { .compatible = "sil,silead_fp-pins" },
#endif
    {},
};
MODULE_DEVICE_TABLE(of, silfp_ree_dt_ids);
#endif

static int silfp_ree_probe(struct spi_device *spi)
{
    struct silfp_ree_data    *ree_data;
    int            status;
    LOG_MSG_DEBUG(INFO_LOG, "silfp_ree_probe");
    /*
     * ree_data should never be referenced in DT without a specific
     * compatible string, it is a Linux implementation thing
     * rather than a description of the hardware.
     */
    if (spi->dev.of_node && !of_match_device(silfp_ree_dt_ids, &spi->dev)) {
        dev_err(&spi->dev, "buggy DT: ree_data listed directly in DT\n");
        WARN_ON(spi->dev.of_node &&
                !of_match_device(silfp_ree_dt_ids, &spi->dev));
    }

    /* Allocate driver data */
    ree_data = kzalloc(sizeof(*ree_data), GFP_KERNEL);
    if (!ree_data)
        return -ENOMEM;

    /* Initialize the driver data */
    ree_data->spi = spi;

    spin_lock_init(&ree_data->spi_lock);
    mutex_init(&ree_data->buf_lock);

    m_ree_device = device_create(silfp_ree_class, NULL, MKDEV(silfp_ree_major, 0), NULL, "silead_spi");
    status = PTR_ERR_OR_ZERO(m_ree_device);
    if (status == 0) {
        m_ree_data = ree_data;
    }

    silfp_ree_parse_spi_dts(ree_data);
    ree_data->speed_hz = spi->max_speed_hz;

    if (status == 0)
        spi_set_drvdata(spi, ree_data);
    else
        kfree(ree_data);

    return status;
}

static int silfp_ree_remove(struct spi_device *spi)
{
    struct silfp_ree_data    *ree_data = spi_get_drvdata(spi);

    /* make sure ops on existing fds can abort cleanly */
    spin_lock_irq(&ree_data->spi_lock);
    ree_data->spi = NULL;
    spin_unlock_irq(&ree_data->spi_lock);


    device_destroy(silfp_ree_class, ree_data->devt);
    if (ree_data->users == 0)
        kfree(ree_data);

    return 0;
}

static struct spi_driver silfp_ree_driver = {
    .driver = {
        .name =        "silfp_ree",
        .of_match_table = of_match_ptr(silfp_ree_dt_ids),
    },
    .probe =    silfp_ree_probe,
    .remove =    silfp_ree_remove,
};

static int __init silfp_ree_init(void)
{
    int status;

    LOG_MSG_DEBUG(INFO_LOG, "silfp_ree_init, version=%s\n", SILEAD_SPI_VERSION);
    silfp_ree_major = register_chrdev(0, "silead_spi", &silfp_ree_fops);
    if (silfp_ree_major < 0) {
        LOG_MSG_DEBUG(ERR_LOG, "register failed, silfp_ree_major = %d", silfp_ree_major);
        return silfp_ree_major;
    }

    silfp_ree_class = class_create(THIS_MODULE, "silead_spi");
    if (IS_ERR(silfp_ree_class)) {
        unregister_chrdev(silfp_ree_major, silfp_ree_driver.driver.name);
        LOG_MSG_DEBUG(ERR_LOG, "register silfp_ree_class");
        return PTR_ERR(silfp_ree_class);
    }

    status = spi_register_driver(&silfp_ree_driver);
    if (status < 0) {
        class_destroy(silfp_ree_class);
        unregister_chrdev(silfp_ree_major, silfp_ree_driver.driver.name);
        LOG_MSG_DEBUG(ERR_LOG, "spi_register_driver");
    }
    return status;
}


static void __exit silfp_ree_exit(void)
{
    LOG_MSG_DEBUG(INFO_LOG, "silfp_ree_exit");
    spi_unregister_driver(&silfp_ree_driver);
    device_unregister(m_ree_device);
    class_destroy(silfp_ree_class);
    unregister_chrdev(silfp_ree_major, silfp_ree_driver.driver.name);
}
module_init(silfp_ree_init);
module_exit(silfp_ree_exit);

MODULE_AUTHOR("Melvin cao@sileadinc");
MODULE_DESCRIPTION("SILEAD FP REE SPI device interface");
MODULE_LICENSE("Dual BSD/GPL");
#endif /* SILFP_SPI_REE */
