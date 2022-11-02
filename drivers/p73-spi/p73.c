/*
 *   *
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 ******************************************************************************/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/irq.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include <linux/spi/spi.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/ktime.h>
#include <linux/regulator/consumer.h>
#include <linux/platform_data/spi-mt65xx.h>
#include "p73.h"
#include "../nfc/nxp-i2c/common_ese.h"

#define CLOCK_SPI 1
#ifdef CLOCK_SPI
extern void mt_spi_enable_master_clk(struct spi_device *spidev);
extern void mt_spi_disable_master_clk(struct spi_device *spidev);
#endif
#define DRAGON_P61 1


/* Macro to define SPI clock frequency */

#undef P61_SPI_CLOCK_7Mzh
#undef P61_SPI_CLOCK_13_3_Mzh
#undef P61_SPI_CLOCK_8Mzh
#undef P61_SPI_CLOCK_20Mzh
#define P61_SPI_CLOCK_8Mzh

#ifdef P61_SPI_CLOCK_13_3_Mzh
//#define P61_SPI_CLOCK 13300000L;Further debug needed
#define P61_SPI_CLOCK     19000000L;
#else
#ifdef P61_SPI_CLOCK_7Mzh
#define P61_SPI_CLOCK     7000000L;
#else
#ifdef P61_SPI_CLOCK_8Mzh
#define P61_SPI_CLOCK     8000000L;
#else
#ifdef P61_SPI_CLOCK_20Mzh
#define P61_SPI_CLOCK     20000000L;
#else
#ifdef P61_SPI_CLOCK_25Mzh
#define P61_SPI_CLOCK     25000000L;
#else
#define P61_SPI_CLOCK     4000000L;
#endif
#endif
#endif
#endif
#endif

/* size of maximum read/write buffer supported by driver */
#ifdef MAX_BUFFER_SIZE
#undef MAX_BUFFER_SIZE
#endif //MAX_BUFFER_SIZE
#define MAX_BUFFER_SIZE   780U

/* Different driver debug lever */
enum P61_DEBUG_LEVEL {
	P61_DEBUG_OFF,
	P61_FULL_DEBUG
};

#define READ_THROUGH_PUT 0x01
#define WRITE_THROUGH_PUT 0x02
#define MXAX_THROUGH_PUT_TIME 999000L
/* Variable to store current debug level request by ioctl */
static unsigned char debug_level;

#define P61_DBG_MSG(msg...)	printk(KERN_ERR "[NXP-P61] :  " msg);

#define P61_ERR_MSG(msg...)	printk(KERN_ERR "[NFC-P61] : " msg);

/* Device specific macro and structure */
struct p61_dev {
	wait_queue_head_t read_wq;	/* wait queue for read interrupt */
	struct mutex read_mutex;	/* read mutex */
	struct mutex write_mutex;	/* write mutex */
	struct spi_device *spi;	/* spi device structure */
	struct miscdevice p61_device;	/* char device as misc driver */
	unsigned char enable_poll_mode;	/* enable the poll mode */

	struct device *nfcc_device;	/*nfcc driver handle for driver to driver comm */
	struct nfc_dev *nfcc_data;
	const char *nfcc_name;
};

const char *nfcc_name_default = "qcom,nq-nci";
/*
const struct mtk_chip_config nfc_ctrdata = {
	//.rx_mlsb = 1,
	//.tx_mlsb = 1,

	.deassert_mode = 1,
};
*/

/* T==1 protocol specific global data */
const unsigned char SOF = 0xA5u;
struct p61_through_put {
	ktime_t rstart_tv;
	ktime_t rstop_tv;
	ktime_t wstart_tv;
	ktime_t wstop_tv;
	unsigned long total_through_put_wbytes;
	unsigned long total_through_put_rbytes;
	unsigned long total_through_put_rtime;
	unsigned long total_through_put_wtime;
	bool enable_through_put_measure;
};
static struct p61_through_put p61_through_put_t;

static void p61_start_throughput_measurement(unsigned int type);
static void p61_stop_throughput_measurement(unsigned int type, int no_of_bytes);
#ifdef CLOCK_SPI
static void nfc_spi_clk_enable(struct p61_dev *nfc_dev, u8 bonoff)
{
	static int count;

	if (bonoff) {
		if (count == 0) {
			pr_err("%s line:%d enable spi clk\n", __func__, __LINE__);
			mt_spi_enable_master_clk(nfc_dev->spi);
		}
		count++;
	} else {
		count--;
		if (count == 0) {
			pr_err("%s line:%d disable spi clk\n", __func__, __LINE__);
			mt_spi_disable_master_clk(nfc_dev->spi);
		} else if (count < 0) {
			count = 0;
		}
	}
}
#endif

static void p61_start_throughput_measurement(unsigned int type)
{
	if (type == READ_THROUGH_PUT) {
		memset(&p61_through_put_t.rstart_tv, 0x00, sizeof(ktime_t));
		p61_through_put_t.rstart_tv = ktime_get();
	} else if (type == WRITE_THROUGH_PUT) {
		memset(&p61_through_put_t.wstart_tv, 0x00, sizeof(ktime_t));
		p61_through_put_t.wstart_tv = ktime_get();

	} else {
		P61_DBG_MSG(KERN_ALERT " p61_start_throughput_measurement: wrong type = %d", type);
	}

}

static void p61_stop_throughput_measurement(unsigned int type, int no_of_bytes)
{
	if (type == READ_THROUGH_PUT) {
		memset(&p61_through_put_t.rstop_tv, 0x00, sizeof(ktime_t));
		p61_through_put_t.rstop_tv = ktime_get();
		p61_through_put_t.total_through_put_rbytes += no_of_bytes;
		p61_through_put_t.total_through_put_rtime += (long)ktime_to_ns(ktime_sub(
					p61_through_put_t.rstop_tv, p61_through_put_t.rstart_tv)) / 1000;

		if (p61_through_put_t.total_through_put_rtime >=
		    MXAX_THROUGH_PUT_TIME) {
			printk(KERN_ALERT " **************** Read Throughput: **************");
			printk(KERN_ALERT " No of Read Bytes = %ld",
			       p61_through_put_t.total_through_put_rbytes);
			printk(KERN_ALERT " Total Read Time (uSec) = %ld",
			       p61_through_put_t.total_through_put_rtime);
			p61_through_put_t.total_through_put_rbytes = 0;
			p61_through_put_t.total_through_put_rtime = 0;
			printk(KERN_ALERT " **************** Read Throughput: **************");
		}
		printk(KERN_ALERT " No of Read Bytes = %ld",
		       p61_through_put_t.total_through_put_rbytes);
		printk(KERN_ALERT " Total Read Time (uSec) = %ld",
		       p61_through_put_t.total_through_put_rtime);
	} else if (type == WRITE_THROUGH_PUT) {
		memset(&p61_through_put_t.wstop_tv, 0x00, sizeof(ktime_t));
		p61_through_put_t.wstop_tv = ktime_get();
		p61_through_put_t.total_through_put_wbytes += no_of_bytes;
		p61_through_put_t.total_through_put_wtime += (long)ktime_to_ns(ktime_sub(
					p61_through_put_t.wstop_tv, p61_through_put_t.wstart_tv)) / 1000;

		if (p61_through_put_t.total_through_put_wtime >=
		    MXAX_THROUGH_PUT_TIME) {
			printk(KERN_ALERT " **************** Write Throughput: **************");
			printk(KERN_ALERT " No of Write Bytes = %ld",
			       p61_through_put_t.total_through_put_wbytes);
			printk(KERN_ALERT " Total Write Time (uSec) = %ld",
			       p61_through_put_t.total_through_put_wtime);
			p61_through_put_t.total_through_put_wbytes = 0;
			p61_through_put_t.total_through_put_wtime = 0;
			printk(KERN_ALERT " **************** WRITE Throughput: **************");
		}
		printk(KERN_ALERT " No of Write Bytes = %ld",
		       p61_through_put_t.total_through_put_wbytes);
		printk(KERN_ALERT " Total Write Time (uSec) = %ld",
		       p61_through_put_t.total_through_put_wtime);
	} else {
		printk(KERN_ALERT " p61_stop_throughput_measurement: wrong type = %u", type);
	}
}

/**
 * \ingroup spi_driver
 * \brief Will be called on device close to release resources
 *
 * \param[in]       struct inode *
 * \param[in]       struct file *
 *
 * \retval 0 if ok.
 *
*/
static int ese_dev_release(struct inode *inode, struct file *filp)
{
	struct p61_dev *p61_dev = NULL;
	printk(KERN_ALERT "Enter %s: ESE driver release \n", __func__);
	p61_dev = filp->private_data;
	nfc_ese_pwr(p61_dev->nfcc_data, ESE_RST_PROT_DIS);
	printk(KERN_ALERT "Exit %s: ESE driver release \n", __func__);
	return 0;
}

/**
 * \ingroup spi_driver
 * \brief Called from SPI LibEse to initilaize the P61 device
 *
 * \param[in]       struct inode *
 * \param[in]       struct file *
 *
 * \retval 0 if ok.
 *
*/

static int p61_dev_open(struct inode *inode, struct file *filp)
{

	struct p61_dev
	*p61_dev = container_of(filp->private_data,
				struct p61_dev,
				p61_device);

	/* Find the NFC parent device if it exists. */
	if (p61_dev != NULL && p61_dev->nfcc_data == NULL) {
		struct device *nfc_dev = bus_find_device_by_name(&i2c_bus_type, NULL, p61_dev->nfcc_name);
		if (nfc_dev) {
		p61_dev->nfcc_data = dev_get_drvdata(nfc_dev);
			if (!p61_dev->nfcc_data) {
				P61_ERR_MSG("%s: cannot find NFC controller device data\n", __func__);
				put_device(nfc_dev);
				return -ENODEV;
			}
			P61_DBG_MSG("%s: NFC controller found\n", __func__);
			p61_dev->nfcc_device = nfc_dev;
		} else {
			P61_ERR_MSG("%s: cannot find NFC controller '%s' skip\n", __func__, p61_dev->nfcc_name);
		}
	}
	filp->private_data = p61_dev;
	P61_DBG_MSG("%s : Major No: %d, Minor No: %d\n", __func__, imajor(inode), iminor(inode));

	return 0;
}

/**
 * \ingroup spi_driver
 * \brief To configure the P61_SET_PWR/P61_SET_DBG/P61_SET_POLL
 * \n         P61_SET_PWR - hard reset (arg=2), soft reset (arg=1)
 * \n         P61_SET_DBG - Enable/Disable (based on arg value) the driver logs
 * \n         P61_SET_POLL - Configure the driver in poll (arg = 1), interrupt (arg = 0) based read operation
 * \param[in]       struct file *
 * \param[in]       unsigned int
 * \param[in]       unsigned long
 *
 * \retval 0 if ok.
 *
*/

static long p61_dev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct p61_dev *p61_dev = NULL;

	unsigned char buf[100];

	pr_info("p61_dev_ioctl-Enter %x arg = %ld\n", cmd, arg);
	p61_dev = filp->private_data;

	switch (cmd) {
	case P61_SET_PWR:
		if (arg == 2) {

		} else if (arg == 1) {
			P61_DBG_MSG(KERN_ALERT " Soft Reset");
			ret = spi_read(p61_dev->spi, (void *)buf, sizeof(buf));
			msleep(50);
		}
		break;

	case P61_SET_DBG:
		debug_level = (unsigned char)arg;
		P61_DBG_MSG(KERN_INFO "[NXP-P61] -  Debug level %d", debug_level);
		break;

	case P61_SET_POLL:

		p61_dev->enable_poll_mode = (unsigned char)arg;
		if (p61_dev->enable_poll_mode == 0) {
			P61_DBG_MSG(KERN_INFO "[NXP-P61] - IRQ Mode is set \n");
		} else {
			P61_DBG_MSG(KERN_INFO "[NXP-P61] - Poll Mode is set \n");
			p61_dev->enable_poll_mode = 1;
		}
		break;
	case P61_SET_SPM_PWR:
		P61_DBG_MSG(KERN_ALERT " P61_SET_SPM_PWR: enter");
		ret = nfc_ese_pwr(p61_dev->nfcc_data, arg);
		P61_DBG_MSG(KERN_ALERT " P61_SET_SPM_PWR: exit");
		break;
	case P61_GET_SPM_STATUS:
		P61_DBG_MSG(KERN_ALERT " P61_GET_SPM_STATUS: enter");
		ret = nfc_ese_pwr(p61_dev->nfcc_data, ESE_POWER_STATE);
		P61_DBG_MSG(KERN_ALERT " P61_GET_SPM_STATUS: exit");
		break;
	case P61_SET_DWNLD_STATUS:
		P61_DBG_MSG(KERN_ALERT " P61_SET_DWNLD_STATUS: enter");
		//ret = nfc_dev_ioctl(filp, PN544_SET_DWNLD_STATUS, arg);
		P61_DBG_MSG(KERN_ALERT " P61_SET_DWNLD_STATUS: =%lu exit", arg);
		break;
		/*
	case P61_SET_THROUGHPUT:
		   p61_through_put_t.enable_through_put_measure = true;
		   P61_DBG_MSG(KERN_INFO"[NXP-P61] -  P61_SET_THROUGHPUT enable %d", p61_through_put_t.enable_through_put_measure);
		   break;
	case P61_GET_ESE_ACCESS:
		P61_DBG_MSG(KERN_ALERT " P61_GET_ESE_ACCESS: enter");
		//ret = nfc_dev_ioctl(filp, P544_GET_ESE_ACCESS, arg);
		P61_DBG_MSG(KERN_ALERT " P61_GET_ESE_ACCESS ret: %d exit", ret);
		break;
	case P61_SET_POWER_SCHEME:
		P61_DBG_MSG(KERN_ALERT " P61_SET_POWER_SCHEME: enter");
		//ret = nfc_dev_ioctl(filp, P544_SET_POWER_SCHEME, arg);
		P61_DBG_MSG(KERN_ALERT " P61_SET_POWER_SCHEME ret: %d exit",
			    ret);
		break;
	case P61_INHIBIT_PWR_CNTRL:
		P61_DBG_MSG(KERN_ALERT " P61_INHIBIT_PWR_CNTRL: enter");
		//ret = nfc_dev_ioctl(filp, P544_SECURE_TIMER_SESSION, arg);
		P61_DBG_MSG(KERN_ALERT " P61_INHIBIT_PWR_CNTRL ret: %d exit",
			    ret);
		break;
		*/
	case ESE_PERFORM_COLD_RESET:
		P61_DBG_MSG(KERN_ALERT " ESE_PERFORM_COLD_RESET: enter");
		ret = nfc_ese_pwr(p61_dev->nfcc_data, ESE_CLD_RST);
		P61_DBG_MSG(KERN_ALERT " ESE_PERFORM_COLD_RESET ret: %d exit", ret);
		break;
	case PERFORM_RESET_PROTECTION:
		P61_DBG_MSG(KERN_ALERT " PERFORM_RESET_PROTECTION: enter");
		ret = nfc_ese_pwr(p61_dev->nfcc_data,
				  (arg == 1 ? ESE_RST_PROT_EN : ESE_RST_PROT_DIS));
		P61_DBG_MSG(KERN_ALERT " PERFORM_RESET_PROTECTION ret: %d exit", ret);
	break;
#ifdef CLOCK_SPI
	case ENBLE_SPI_CLK:
		nfc_spi_clk_enable(p61_dev, 1);
	break;
	case DISABLE_SPI_CLK:
		nfc_spi_clk_enable(p61_dev, 0);
	break;
#endif
	default:
		P61_DBG_MSG(KERN_ALERT " Error case");
		ret = -EINVAL;
	}

	pr_info("p61_dev_ioctl-exit %u arg = %lu\n", cmd, arg);
	return ret;
}

/**
 * \ingroup spi_driver
 * \brief Write data to P61 on SPI
 *
 * \param[in]       struct file *
 * \param[in]       const char *
 * \param[in]       size_t
 * \param[in]       loff_t *
 *
 * \retval data size
 *
*/

static ssize_t p61_dev_write(struct file *filp, const char *buf, size_t count, loff_t *offset)
{

	int ret = -1;
	struct p61_dev *p61_dev;
	unsigned char tx_buffer[MAX_BUFFER_SIZE];

	pr_info("p61_dev_write -Enter count %zu\n", count);

	p61_dev = filp->private_data;

	mutex_lock(&p61_dev->write_mutex);
	if (count > MAX_BUFFER_SIZE)
		count = MAX_BUFFER_SIZE;

	memset(&tx_buffer[0], 0, sizeof(tx_buffer));
	if (copy_from_user(&tx_buffer[0], &buf[0], count)) {
		P61_ERR_MSG("%s : failed to copy from user space\n", __func__);
		mutex_unlock(&p61_dev->write_mutex);
		return -EFAULT;
	}
	if (p61_through_put_t.enable_through_put_measure)
		p61_start_throughput_measurement(WRITE_THROUGH_PUT);
	/* Write data */
	ret = spi_write(p61_dev->spi, &tx_buffer[0], count);
	if (ret < 0) {
		ret = -EIO;
	} else {
		ret = count;
		if (p61_through_put_t.enable_through_put_measure)
			p61_stop_throughput_measurement(WRITE_THROUGH_PUT, ret);
	}

	mutex_unlock(&p61_dev->write_mutex);
	P61_DBG_MSG(KERN_ALERT "p61_dev_write ret %d- Exit \n", ret);
	return ret;
}

/**
 * \ingroup spi_driver
 * \brief Used to read data from P61 in Poll/interrupt mode configured using ioctl call
 *
 * \param[in]       struct file *
 * \param[in]       char *
 * \param[in]       size_t
 * \param[in]       loff_t *
 *
 * \retval read size
 *
*/

static ssize_t p61_dev_read(struct file *filp, char *buf, size_t count, loff_t *offset)
{
	int ret = -EIO;
	struct p61_dev *p61_dev = filp->private_data;
	int i = 0;
	unsigned char rx_buffer[MAX_BUFFER_SIZE] = { 0 };

	P61_DBG_MSG("p61_dev_read count %zu - Enter \n", count);

	mutex_lock(&p61_dev->read_mutex);
	if (count > MAX_BUFFER_SIZE) {
		count = MAX_BUFFER_SIZE;
	}

	memset(&rx_buffer[0], 0x00, sizeof(rx_buffer));

	if (p61_dev->enable_poll_mode) {
		P61_DBG_MSG(" %s Poll Mode Enabled \n", __FUNCTION__);

		P61_DBG_MSG(KERN_INFO "SPI_READ returned 0x%zu", count);
		ret = spi_read(p61_dev->spi, (void *)&rx_buffer[0], count);
		if (0 > ret) {
			P61_ERR_MSG(KERN_ALERT "spi_read failed [SOF] \n");
			goto fail;
		}
	} else {

		P61_DBG_MSG(" %s P61_IRQ_ENABLE not Enabled \n", __FUNCTION__);

		ret = spi_read(p61_dev->spi, (void *)&rx_buffer[0], count);
		if (0 > ret) {
			P61_DBG_MSG(KERN_INFO "SPI_READ returned 0x%x", ret);
			ret = -EIO;
			goto fail;
		}
	}

	if (p61_through_put_t.enable_through_put_measure)
		p61_start_throughput_measurement(READ_THROUGH_PUT);

	if (p61_through_put_t.enable_through_put_measure)
		p61_stop_throughput_measurement(READ_THROUGH_PUT, count);
	P61_DBG_MSG(KERN_INFO "total_count = %zu", count);

	if (copy_to_user(buf, &rx_buffer[0], count)) {
		P61_ERR_MSG("%s : failed to copy to user space\n", __func__);
		ret = -EFAULT;
		goto fail;
	}
	P61_DBG_MSG("p61_dev_read ret %d Exit\n", ret);
	P61_DBG_MSG("p61_dev_read rx_buffer %d Exit\n", rx_buffer[0]);
	for (i = 0; i < count; i++) {
		P61_ERR_MSG("p61_dev_read rx_buffer is %d\n ", rx_buffer[i]);
	}

	mutex_unlock(&p61_dev->read_mutex);

	return ret;

fail:
	P61_ERR_MSG("Error p61_dev_read ret %d Exit\n", ret);
	mutex_unlock(&p61_dev->read_mutex);
	return ret;
}

/**
 * \ingroup spi_driver
 * \brief It will configure the GPIOs required for soft reset, read interrupt & regulated power supply to P61.
 *
 * \param[in]       struct p61_spi_platform_data *
 * \param[in]       struct p61_dev *
 * \param[in]       struct spi_device *
 *
 * \retval 0 if ok.
 *
*/

static int p61_hw_setup(struct p61_spi_platform_data *platform_data,
			struct p61_dev *p61_dev, struct spi_device *spi)
{
	int ret = -1;

	P61_DBG_MSG("Entry : %s\n", __FUNCTION__);
	ret = 0;
	return ret;

}

/**
 * \ingroup spi_driver
 * \brief Set the P61 device specific context for future use.
 *
 * \param[in]       struct spi_device *
 * \param[in]       void *
 *
 * \retval void
 *
*/

static inline void p61_set_data(struct spi_device *spi, void *data)
{
	dev_set_drvdata(&spi->dev, data);
}

/**
 * \ingroup spi_driver
 * \brief Get the P61 device specific context.
 *
 * \param[in]       const struct spi_device *
 *
 * \retval Device Parameters
 *
*/

static inline void *p61_get_data(const struct spi_device *spi)
{
	return dev_get_drvdata(&spi->dev);
}

/* possible fops on the p61 device */
static const struct file_operations p61_dev_fops = {
	.owner = THIS_MODULE,
	.read = p61_dev_read,
	.write = p61_dev_write,
	.open = p61_dev_open,
	.release = ese_dev_release,
	.unlocked_ioctl = p61_dev_ioctl,
};

#if DRAGON_P61
static int p61_parse_dt(struct device *dev, struct p61_spi_platform_data *data)
{
	int errorno = 0;
	struct pinctrl * p61_pinctrl = NULL;
	struct pinctrl_state *pin_spi_mode = NULL;
	pr_info("%s: %d\n", __func__, errorno);

	if (IS_ERR_OR_NULL(dev)) {
        pr_err("device ptr err");
        return PTR_ERR(dev);
	}

	p61_pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR_OR_NULL(p61_pinctrl)) {
		pr_err("Failed to get pinctrl handler");
		return PTR_ERR(p61_pinctrl);
	}
	pin_spi_mode = pinctrl_lookup_state(p61_pinctrl,"nxp_ese_spi_mode");
	if (IS_ERR_OR_NULL(pin_spi_mode)) {
		errorno = PTR_ERR(pin_spi_mode);
		pr_err("Failed to get pinctrl state errorno:%d", errorno);
		return errorno;
	}

	errorno = pinctrl_select_state(p61_pinctrl,pin_spi_mode);
	if (errorno < 0)
		P61_DBG_MSG("p61_parse_dt Failed to select default, errorno:%d", errorno);

	return errorno;
}
#endif

/**
 * \ingroup spi_driver
 * \brief To probe for P61 SPI interface. If found initialize the SPI clock, bit rate & SPI mode.
          It will create the dev entry (P61) for user space.
 *
 * \param[in]       struct spi_device *
 *
 * \retval 0 if ok.
 *
*/

static int p61_probe(struct spi_device *spi)
{
	int ret = -1;
	struct p61_spi_platform_data *platform_data = NULL;
	struct p61_spi_platform_data platform_data1;
	struct p61_dev *p61_dev = NULL;
	struct device_node *np = spi->dev.of_node;
	pr_info("%s chip select : %d , bus number = %d \n", __FUNCTION__, spi->chip_select, spi->master->bus_num);

	ret = p61_parse_dt(&spi->dev, &platform_data1);
	if (ret) {
		pr_err("%s - Failed to parse DT\n", __func__);
		goto err_exit;
	}
	platform_data = &platform_data1;

	p61_dev = kzalloc(sizeof(*p61_dev), GFP_KERNEL);
	if (p61_dev == NULL) {
		P61_ERR_MSG("failed to allocate memory for module data\n");
		ret = -ENOMEM;
		goto err_exit;
	}
	ret = p61_hw_setup(platform_data, p61_dev, spi);
	if (ret < 0) {
		P61_ERR_MSG("Failed to p61_enable_P61_IRQ_ENABLE\n");
		goto err_exit0;
	}

	//spi->controller_data = (void *)&nfc_ctrdata;
	spi->bits_per_word = 8;
	spi->mode = SPI_MODE_0;
	spi->max_speed_hz = P61_SPI_CLOCK;
	//spi->chip_select = SPI_NO_CS;
	ret = spi_setup(spi);
	if (ret < 0) {
		P61_ERR_MSG("failed to do spi_setup()\n");
		goto err_exit0;
	}

	p61_dev->spi = spi;
	p61_dev->p61_device.minor = MISC_DYNAMIC_MINOR;
	p61_dev->p61_device.name = "p73";
	p61_dev->p61_device.fops = &p61_dev_fops;
	p61_dev->p61_device.parent = &spi->dev;


	dev_set_drvdata(&spi->dev, p61_dev);

	/* init mutex and queues */
	init_waitqueue_head(&p61_dev->read_wq);
	mutex_init(&p61_dev->read_mutex);
	mutex_init(&p61_dev->write_mutex);

	ret = of_property_read_string(np, "nxp,nfcc", &p61_dev->nfcc_name);
	if (ret < 0) {
		P61_DBG_MSG("%s: nxp,nfcc invalid(%d), set to default\n", __func__, ret);
		p61_dev->nfcc_name = nfcc_name_default;
	}
	pr_info("%s: device tree set '%s' as eSE power controller\n", __func__, p61_dev->nfcc_name);

	ret = misc_register(&p61_dev->p61_device);
	if (ret < 0) {
		P61_ERR_MSG("misc_register failed! %d\n", ret);
		goto err_exit0;
	}

	p61_dev->enable_poll_mode = 1;	/* Default poll read mode */
#ifdef CLOCK_SPI
	pr_info("%s now disable spi clk", __func__);
	nfc_spi_clk_enable(p61_dev, 0);
#endif
	P61_DBG_MSG("Exit : %s\n", __FUNCTION__);
	return ret;

err_exit0:
	mutex_destroy(&p61_dev->read_mutex);
	mutex_destroy(&p61_dev->write_mutex);
	if (p61_dev != NULL)
		kfree(p61_dev);
err_exit:
	P61_DBG_MSG("ERROR: Exit : %s ret %d\n", __FUNCTION__, ret);
	return ret;
}

static int p61_suspend(struct device *dev, pm_message_t state)
{
	P61_DBG_MSG("Entry : %s\n", __FUNCTION__);
	return 0;
}

static int p61_resume(struct device *dev)
{
	P61_DBG_MSG("Entry : %s\n", __FUNCTION__);
	return 0;
}

static void p61_shutdown(struct spi_device *spi)
{
	P61_DBG_MSG("Entry : %s\n", __FUNCTION__);
}

/**
 * \ingroup spi_driver
 * \brief Will get called when the device is removed to release the resources.
 *
 * \param[in]       struct spi_device
 *
 * \retval 0 if ok.
 *
*/

static int p61_remove(struct spi_device *spi)
{
	struct p61_dev *p61_dev = p61_get_data(spi);
	P61_DBG_MSG("Entry : %s\n", __FUNCTION__);
	if (p61_dev != NULL) {
		mutex_destroy(&p61_dev->read_mutex);
		misc_deregister(&p61_dev->p61_device);
		kfree(p61_dev);
	}
	P61_DBG_MSG("Exit : %s\n", __FUNCTION__);
	return 0;
}

#if DRAGON_P61
static struct of_device_id p61_dt_match[] = {
	{.compatible = "nxp,p61",},
	{}
};
#endif

static const struct spi_device_id p61_id[] = {
	{"p61", 0},
	{}
};

MODULE_DEVICE_TABLE(spi, p61_id);

static struct spi_driver p61_driver = {
	.driver = {
		.name = "p61",
		.bus = &spi_bus_type,
		.owner = THIS_MODULE,
		.suspend = p61_suspend,
		.resume = p61_resume,
#if DRAGON_P61
		.of_match_table = p61_dt_match,
#endif
	},
	.id_table = p61_id,
	.probe = p61_probe,
	.remove = p61_remove,
	.shutdown = p61_shutdown,
};

/**
 * \ingroup spi_driver
 * \brief Module init interface
 *
 * \param[in]       void
 *
 * \retval handle
 *
*/

static int __init p61_dev_init(void)
{
	int32_t ret = 0;
	debug_level = P61_FULL_DEBUG;

	P61_DBG_MSG("Entry : %s\n", __FUNCTION__);

	ret = spi_register_driver(&p61_driver);
	if (ret) {
		P61_DBG_MSG("nxp failed to add spi driver");
	}

	P61_DBG_MSG("Exit : %s\n", __FUNCTION__);
	return ret;

}
module_init(p61_dev_init);

/**
 * \ingroup spi_driver
 * \brief Module exit interface
 *
 * \param[in]       void
 *
 * \retval void
 *
*/

static void __exit p61_dev_exit(void)
{
	P61_DBG_MSG("Entry : %s\n", __FUNCTION__);

	spi_unregister_driver(&p61_driver);
}
module_exit(p61_dev_exit);

MODULE_ALIAS("spi:p61");
MODULE_AUTHOR("BHUPENDRA PAWAR");
MODULE_DESCRIPTION("NXP P61 SPI driver");
MODULE_LICENSE("GPL");

/** @} */
