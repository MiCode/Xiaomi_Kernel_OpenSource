/*************************************************************************
 * Copyright (C) 2015 Broadcom Corporation
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 ************************************************************************/

#include <linux/module.h>
#include <linux/ioport.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/sysrq.h>
#include <linux/console.h>
#include <linux/delay.h>
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>
#include <linux/kthread.h>
#include <linux/circ_buf.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/of_gpio.h>
#include <linux/poll.h>
#include <linux/uaccess.h>

#include <linux/suspend.h>
#include <linux/kernel.h>

#include <linux/gpio.h>

#include <linux/miscdevice.h>
#include <linux/time.h>
#include <linux/io.h>
#include <asm/irq.h>
#include <linux/kernel_stat.h>
#include <linux/pm_runtime.h>
#include <linux/spi/spi-geni-qcom.h>



#include "bbd.h"

#define WORD_BURST_SIZE			4
#define CONFIG_SPI_DMA_BYTES_PER_WORD	4
#define CONFIG_SPI_DMA_BITS_PER_WORD	32

#define SSI_MODE_STREAM		      0x00
#define SSI_MODE_DEBUG		      0x80

#define SSI_MODE_HALF_DUPLEX	  0x00
#define SSI_MODE_FULL_DUPLEX	  0x40

#define SSI_WRITE_TRANS		      0x00
#define SSI_READ_TRANS		      0x20

#define SSI_PCKT_1B_LENGTH        0
#define SSI_PCKT_2B_LENGTH        0x10

#define SSI_FLOW_CONTROL_DISABLED 0
#define SSI_FLOW_CONTROL_ENABLED  0x08

#define SSI_WRITE_HD (SSI_WRITE_TRANS | SSI_MODE_HALF_DUPLEX)
#define SSI_READ_HD  (SSI_READ_TRANS  | SSI_MODE_HALF_DUPLEX)

#define HSI_F_MOSI_CTRL_CNT_SHIFT  0
#define HSI_F_MOSI_CTRL_CNT_SIZE   3
#define HSI_F_MOSI_CTRL_CNT_MASK   0x07

#define HSI_F_MOSI_CTRL_SZE_SHIFT  3
#define HSI_F_MOSI_CTRL_SZE_SIZE   2
#define HSI_F_MOSI_CTRL_SZE_MASK   0x18

#define HSI_F_MOSI_CTRL_RSV_SHIFT  5
#define HSI_F_MOSI_CTRL_RSV_SIZE   1
#define HSI_F_MOSI_CTRL_RSV_MASK   0x20

#define HSI_F_MOSI_CTRL_PE_SHIFT   6
#define HSI_F_MOSI_CTRL_PE_SIZE    1
#define HSI_F_MOSI_CTRL_PE_MASK    0x40

#define HSI_F_MOSI_CTRL_PZC_SHIFT   0
#define HSI_F_MOSI_CTRL_PZC_SIZE    (HSI_F_MOSI_CTRL_CNT_SIZE + \
		HSI_F_MOSI_CTRL_SZE_SIZE + HSI_F_MOSI_CTRL_PE_SIZE)
#define HSI_F_MOSI_CTRL_PZC_MASK    (HSI_F_MOSI_CTRL_CNT_MASK \
		| HSI_F_MOSI_CTRL_SZE_MASK | HSI_F_MOSI_CTRL_PE_MASK)


#define DEBUG_TIME_STAT
#define CONFIG_TRANSFER_STAT


#define CONFIG_PACKET_RECEIVED (31*1024)

bool ssi_dbg = true;
bool ssi_dbg_pzc = true;
bool ssi_dbg_rng = true;


int  g_bcm_bitrate = 12000;

int  ssi_mode = 1;

int  ssi_len = 2;
int  ssi_fc = 3;


int  ssi_tx_fail = 1;


int  ssi_tx_fc_retries = 1;

int  ssi_tx_fc_retry_errors = 1;


struct bcm_spi_strm_protocol  {
	int  pckt_len;
	int  fc_len;
	int  ctrl_len;
	unsigned char  ctrl_byte;
	unsigned short frame_len;
} __attribute__((__packed__));


#ifdef CONFIG_TRANSFER_STAT
struct bcm_spi_transfer_stat  {
	int  len_255;
	int  len_1K;
	int  len_8K;
	int  len_32K;
	int  len_64K;
} __attribute__((__packed__));
#endif







#define BCM_SPI_READ_BUF_SIZE	(8*PAGE_SIZE)
#define BCM_SPI_WRITE_BUF_SIZE	(8*PAGE_SIZE)


#define MIN_SPI_FRAME_LEN 254
#define MAX_SPI_FRAME_LEN (BCM_SPI_READ_BUF_SIZE / 2)

struct bcm_ssi_tx_frame  {
	unsigned char cmd;
	unsigned char data[MAX_SPI_FRAME_LEN-1];
} __attribute__((__packed__));

struct bcm_ssi_rx_frame  {
	unsigned char status;
	unsigned char data[MAX_SPI_FRAME_LEN-1];
} __attribute__((__packed__));


struct bcm_spi_priv  {
	struct spi_device *spi;

	/* Char device stuff */
	struct miscdevice misc;
	bool busy;
	struct circ_buf read_buf;
	struct circ_buf write_buf;
	struct mutex rlock;			/* Lock for read_buf */
	struct mutex wlock;			/* Lock for write_buf */
	char _read_buf[BCM_SPI_READ_BUF_SIZE];
	char _write_buf[BCM_SPI_WRITE_BUF_SIZE];
	wait_queue_head_t poll_wait;		/* for poll */

	/* GPIO pins */
	int host_req;
	int mcu_req;
	int mcu_resp;
	int nstandby;

	/* IRQ and its control */
	atomic_t irq_enabled;
	spinlock_t irq_lock;

	/* Work */
	struct work_struct rxtx_work;
	struct work_struct start_tx;
	struct workqueue_struct *serial_wq;
	atomic_t suspending;


	/* SPI tx/rx buf */
	struct bcm_ssi_tx_frame *tx_buf;
	struct bcm_ssi_rx_frame *rx_buf;

	/* 4775 SPI tx/rx strm protocol */
	struct bcm_spi_strm_protocol tx_strm;
	struct bcm_spi_strm_protocol rx_strm;


#ifdef CONFIG_TRANSFER_STAT
	struct bcm_spi_transfer_stat trans_stat[2];
#endif
	struct pinctrl *ts_pinctrl;
	struct pinctrl_state *gpio_state_active;
	struct pinctrl_state *gpio_state_suspend;

	/* some chip-set(BCM4775) needs to skip sanity-checking */
	bool skip_sanity;

	/* To make sure  that interface is detected on chip side !=0 */
	unsigned long packet_received;
};

static struct bcm_spi_priv *g_bcm_gps;
static int bcm_spi_sync(struct bcm_spi_priv *priv,
	   void *tx_buf, void *rx_buf, int len, int bits_per_word);


#ifdef CONFIG_TRANSFER_STAT

static ssize_t bcm_4775_nstandby_show(struct device *dev,
	   struct device_attribute *attr,
	   char *buf)
{
	int value = 0;
	struct spi_device *spi = to_spi_device(dev);
	struct bcm_spi_priv *priv = (struct bcm_spi_priv *)spi_get_drvdata(spi);

	value = gpio_get_value(priv->nstandby);
	return snprintf(buf, 20, "%d\n", value);
}

static ssize_t bcm_4775_nstandby_store(struct device *dev,
	   struct device_attribute *attr,
	   const char *buf, size_t count)
{
	struct spi_device *spi = to_spi_device(dev);
	struct bcm_spi_priv *priv = (struct bcm_spi_priv *)spi_get_drvdata(spi);

	pr_err("[SSPBBD} bcm_4775_nstandby, buf is %s\n", buf);

	if (!strncmp("0", buf, 1))
		gpio_set_value(priv->nstandby, 0);
	else
		gpio_set_value(priv->nstandby, 1);
	return count;
}

static DEVICE_ATTR(nstandby, 0660,
	   bcm_4775_nstandby_show, bcm_4775_nstandby_store);

static void bcm_ssi_clear_trans_stat(struct bcm_spi_priv *priv)
{
	memset(priv->trans_stat, 0, sizeof(priv->trans_stat));
}

static void bcm_ssi_print_trans_stat(struct bcm_spi_priv *priv)
{
	return;
	/*char buf[512];
	char *p = buf;

	struct bcm_spi_transfer_stat *trans = &priv->trans_stat[0];
	p = buf;

	p += snprintf(p, sizeof(buf),
				 "SSPBBD TX:<255B=%d <1K=%d,<8K=%d,<32K=%d,<64K=%d",
				 trans->len_255, trans->len_1K, trans->len_8K,
				 trans->len_32K, trans->len_64K);
	//pr_info("%s\n",buf);

	trans = &priv->trans_stat[1];
	p = buf;
	p += snprintf(p, sizeof(buf),
				 "SSPBBD:RX: <255B=%d,<1K=%d,<8K=%d,<32K=%d,<64K=%d",
				 trans->len_255, trans->len_1K,
				 trans->len_8K, trans->len_32K, trans->len_64K);

	//pr_info("%s\n",buf);

	if (priv->tx_strm.ctrl_byte & SSI_FLOW_CONTROL_ENABLED) {
		p = buf;
		p += snprintf(p, sizeof(buf),
					 "SSPBBD fail=%d,retries=%d,retry errors=%d",
					 ssi_tx_fail-1,
					 ssi_tx_fc_retries-1,
					 ssi_tx_fc_retry_errors-1);
		//pr_info("%s\n",buf);
	}*/
}

static void bcm_ssi_calc_trans_stat(struct bcm_spi_transfer_stat *trans,
		unsigned short length)
{
	if (length <=  255)
		trans->len_255++;
	else if (length <=  1024)
		trans->len_1K++;
	else if (length <= (8*1024))
		trans->len_8K++;
	else if (length <= (32*1024))
		trans->len_32K++;
	else
		trans->len_64K++;
}
#endif

unsigned long m_ulRxBufferBlockSize[4] = {32, 256, 1024 * 2, 1024 * 16};






static int bcm_spi_open(struct inode *inode, struct file *filp)
{
	/* Initially, */
	struct bcm_spi_priv *priv = container_of(filp->private_data,
						struct bcm_spi_priv, misc);
	struct bcm_spi_strm_protocol *strm;
	unsigned long int flags;
	unsigned char fc_mask, len_mask, duplex_mask;





	if (priv->busy)
		return -EBUSY;

	priv->busy = true;

	/* Reset circ buffer */
	priv->read_buf.head = priv->read_buf.tail = 0;
	priv->write_buf.head = priv->write_buf.tail = 0;

	priv->packet_received = 0;

	/* Enable irq */
	spin_lock_irqsave(&priv->irq_lock, flags);
	if (!atomic_xchg(&priv->irq_enabled, 1))
		enable_irq(priv->spi->irq);

	spin_unlock_irqrestore(&priv->irq_lock, flags);

	enable_irq_wake(priv->spi->irq);

	filp->private_data = priv;

	strm = &priv->tx_strm;
	strm->pckt_len = ssi_len == 2 ? 2:1;
	len_mask = ssi_len == 2 ? SSI_PCKT_2B_LENGTH:SSI_PCKT_1B_LENGTH;
	duplex_mask = ssi_mode != 0 ?
				  SSI_MODE_FULL_DUPLEX : SSI_MODE_HALF_DUPLEX;

	fc_mask = SSI_FLOW_CONTROL_DISABLED;
	strm->fc_len = 0;
	if (ssi_fc) {
		fc_mask = SSI_FLOW_CONTROL_ENABLED;
		strm->fc_len = ssi_len == 2 ? 2:1;
	} else if (ssi_mode == 0) {
		strm->pckt_len = 0;
	}
	strm->ctrl_len = strm->pckt_len + strm->fc_len + 1;

	strm->frame_len = ssi_len == 2 ? MAX_SPI_FRAME_LEN : MIN_SPI_FRAME_LEN;
	strm->ctrl_byte = duplex_mask | SSI_MODE_STREAM
					| len_mask | SSI_WRITE_TRANS | fc_mask;


	/*p = buf;
	snprintf(p, sizeof(buf),
			"[SSPBBD]: tx ctrl %02X: total %d = len %d + fc %d + cmd 1",
			strm->ctrl_byte,
			strm->ctrl_len,
			strm->pckt_len,
			strm->fc_len);
	//pr_info("%s\n",buf);*/

	strm = &priv->rx_strm;
	strm->pckt_len = ssi_len == 2 ? 2:1;
	strm->fc_len = 0;
	strm->ctrl_len = strm->pckt_len + strm->fc_len + 1;
	strm->frame_len = ssi_len == 2 ? MAX_SPI_FRAME_LEN : MIN_SPI_FRAME_LEN;
	strm->ctrl_byte = duplex_mask | SSI_MODE_STREAM
					  | len_mask |
					  (ssi_mode == SSI_MODE_FULL_DUPLEX ?
					  SSI_WRITE_TRANS : SSI_READ_TRANS);


	/*p = buf;
	snprintf(p, sizeof(buf),
			"[SSPBBD]: rx ctrl %02X: total %d = len %d + fc %d + stat 1",
			strm->ctrl_byte,
			strm->ctrl_len,
			strm->pckt_len,
			strm->fc_len);
	//pr_info("%s\n",buf);*/

	{
#ifdef CONFIG_REG_IO
		unsigned int regval[8];
#endif
		/*p = buf;
		snprintf(p, sizeof(buf),
				"SSPBBD %d:%s Duplex %dB Len,%s FC,Len %u:tx %02X,rx %02X",
				g_bcm_bitrate,
				ssi_mode  != 0 ? "Full" : "Half",
				ssi_len       == 2 ? 2 : 1,
				ssi_fc  != 0 ? "with" : "w/o",
				strm->frame_len,
				priv->tx_strm.ctrl_byte,
				priv->rx_strm.ctrl_byte);
		//pr_info("%s\n",buf);*/
	}

#ifdef CONFIG_TRANSFER_STAT
	bcm_ssi_clear_trans_stat(priv);
#endif
	ssi_tx_fail       = 1;
	ssi_tx_fc_retries = 1;
	ssi_tx_fc_retry_errors = 1;

	return 0;
}


static int bcm_spi_release(struct inode *inode, struct file *filp)
{
	struct bcm_spi_priv *priv = filp->private_data;
	unsigned long int flags;


	priv->busy = false;

#ifdef CONFIG_TRANSFER_STAT
	bcm_ssi_print_trans_stat(priv);
#endif

	/* Disable irq */
	spin_lock_irqsave(&priv->irq_lock, flags);
	if (atomic_xchg(&priv->irq_enabled, 0))
		disable_irq_nosync(priv->spi->irq);

	spin_unlock_irqrestore(&priv->irq_lock, flags);

	disable_irq_wake(priv->spi->irq);


	return 0;
}

static ssize_t bcm_spi_read(struct file *filp, char __user *buf,
							size_t size,
							loff_t *ppos)
{
	struct bcm_spi_priv *priv = filp->private_data;
	struct circ_buf *circ = &priv->read_buf;
	size_t rd_size = 0;



	mutex_lock(&priv->rlock);

	/* Copy from circ buffer to user
	 * We may require 2 copies from [tail..end] and [end..head]
	 */
	do {
		size_t cnt_to_end = CIRC_CNT_TO_END(circ->head, circ->tail,
							BCM_SPI_READ_BUF_SIZE);
		size_t copied = min(cnt_to_end, size);

		WARN_ON(copy_to_user(buf + rd_size,
				(void *) circ->buf + circ->tail,
				copied));
		size -= copied;
		rd_size += copied;
		circ->tail = (circ->tail + copied) & (BCM_SPI_READ_BUF_SIZE-1);

	} while (size > 0 &&
			CIRC_CNT(circ->head, circ->tail,
			BCM_SPI_READ_BUF_SIZE));
	mutex_unlock(&priv->rlock);


	return rd_size;
}

static ssize_t bcm_spi_write(struct file *filp, const char __user *buf,
							 size_t size,
							 loff_t *ppos)
{
	struct bcm_spi_priv *priv = filp->private_data;
	struct circ_buf *circ = &priv->write_buf;
	size_t wr_size = 0;
	bool rxtx_work_run = true;



	mutex_lock(&priv->wlock);
	/* Copy from user into circ buffer
	 * We may require 2 copies from [tail..end] and [end..head]
	 */
	do {
		size_t space_to_end = CIRC_SPACE_TO_END(circ->head,
							circ->tail,
							BCM_SPI_WRITE_BUF_SIZE);
		size_t copied = min(space_to_end, size);


		WARN_ON(copy_from_user((void *) circ->buf + circ->head,
								buf +
								wr_size,
								copied));
		size -= copied;
		wr_size += copied;
		circ->head = (circ->head + copied) & (BCM_SPI_WRITE_BUF_SIZE-1);
	} while (size > 0 && CIRC_SPACE(circ->head, circ->tail,
			BCM_SPI_WRITE_BUF_SIZE));

	mutex_unlock(&priv->wlock);



	/* kick start rxtx thread */
	/* we don't want to queue work in suspending and shutdown */
	if (!atomic_read(&priv->suspending)) {
		/* Disable irq */
		unsigned long int flags;
		spin_lock_irqsave(&priv->irq_lock, flags);
		if (atomic_xchg(&priv->irq_enabled, 0))
			disable_irq_nosync(priv->spi->irq);
		else
			rxtx_work_run = false;

		spin_unlock_irqrestore(&priv->irq_lock, flags);

		if (rxtx_work_run == true) {

			queue_work(priv->serial_wq, &(priv->rxtx_work));

		}
	}
	return wr_size;
}

static unsigned int bcm_spi_poll(struct file *filp, poll_table *wait)
{
	struct bcm_spi_priv *priv = filp->private_data;
	struct circ_buf *rd_circ = &priv->read_buf;
	struct circ_buf *wr_circ = &priv->write_buf;
	unsigned int mask = 0;

	poll_wait(filp, &priv->poll_wait, wait);

	if (CIRC_CNT(rd_circ->head, rd_circ->tail, BCM_SPI_READ_BUF_SIZE))
		mask |= POLLIN;

	if (CIRC_SPACE(wr_circ->head, wr_circ->tail, BCM_SPI_WRITE_BUF_SIZE))
		mask |= POLLOUT;

	return mask;
}


static const struct file_operations bcm_spi_fops = {
	.owner          =  THIS_MODULE,
	.open           =  bcm_spi_open,
	.release        =  bcm_spi_release,
	.read           =  bcm_spi_read,
	.write          =  bcm_spi_write,
	.poll           =  bcm_spi_poll,
};










/*
 * bcm477x_hello - wakeup chip by toggling mcu_req while
 * monitoring mcu_resp to check if awake
 */
static bool bcm477x_hello(struct bcm_spi_priv *priv)
{
	int count = 0, retries = 0;

	gpio_set_value(priv->mcu_req, 1);
	while (!gpio_get_value(priv->mcu_resp)) {
		if (count++ > 100) {
			gpio_set_value(priv->mcu_req, 0);
			return false;
		}

		msleep(20);

		/*if awake, done */
		if (gpio_get_value(priv->mcu_resp))
			break;

		if (count%20 == 0 && retries++ < 3) {
			gpio_set_value(priv->mcu_req, 0);
			msleep(20);
			gpio_set_value(priv->mcu_req, 1);
			msleep(20);
		}
	}
	return true;
}

/*
 * bcm477x_bye - set mcu_req low to let chip go to sleep
 *
 */
static void bcm477x_bye(struct bcm_spi_priv *priv)
{
	gpio_set_value(priv->mcu_req, 0);
}







static unsigned short bcm_ssi_get_len(unsigned char ctrl_byte,
						unsigned char *data)
{
	unsigned short len;

	if (ctrl_byte & SSI_PCKT_2B_LENGTH)
		len = ((unsigned short)data[0] +
		((unsigned short)data[1] << 8));
	else
		len = (unsigned short)data[0];

	return len;
}

static void bcm_ssi_set_len(unsigned char ctrl_byte, unsigned char *data,
							unsigned short len)
{
	if (ctrl_byte & SSI_PCKT_2B_LENGTH) {
		data[0] = (unsigned char)(len & 0xff);
		data[1] = (unsigned char)((len >> 8)  & 0xff);
	} else
		data[0] = (unsigned char)len;
}

static void bcm_ssi_clr_len(unsigned char ctrl_byte, unsigned char *data)
{
	bcm_ssi_set_len(ctrl_byte, data, 0);
}


static int bcm_spi_sync(struct bcm_spi_priv *priv, void *tx_buf,
						void *rx_buf,
						int len,
						int bits_per_word)
{
	struct spi_message msg;
	struct spi_transfer xfer;
	int ret;

	/* Init */
	spi_message_init(&msg);
	memset(&xfer, 0, sizeof(xfer));
	spi_message_add_tail(&xfer, &msg);

	/* Setup */
	msg.spi = priv->spi;
	xfer.len = len;
	xfer.tx_buf = tx_buf;
	xfer.rx_buf = rx_buf;

	/* Sync */

	ret = spi_sync(msg.spi, &msg);


	if (ret)
		pr_err("[SSPBBD} spi_sync error, return=%d\n", ret);

	return ret;
}


static int bcm_ssi_tx(struct bcm_spi_priv *priv, int length)
{
	struct bcm_ssi_tx_frame *tx = priv->tx_buf;
	struct bcm_ssi_rx_frame *rx = priv->rx_buf;
	struct bcm_spi_strm_protocol *strm = &priv->tx_strm;
	int bits_per_word = (length >= 255) ? CONFIG_SPI_DMA_BITS_PER_WORD : 8;
	int ret, n;
	unsigned short Mwrite, Bwritten = 0;
	unsigned short bytes_to_write = (unsigned short)length;
	unsigned short Nread = 0;
	int retry_ctr = ssi_fc;

	Mwrite = bytes_to_write;
	Nread = 0;

	do {
		tx->cmd = strm->ctrl_byte;

		bytes_to_write = max(Mwrite, Nread);

		if (strm->pckt_len != 0)
			bcm_ssi_set_len(strm->ctrl_byte, tx->data, Mwrite);


		if (strm->fc_len != 0)
			bcm_ssi_clr_len(strm->ctrl_byte, tx->data +
							bytes_to_write +
							strm->pckt_len);

		ret = bcm_spi_sync(priv, tx, rx,
						   bytes_to_write +
						   strm->ctrl_len,
						   bits_per_word);

		if (ret) {
			ssi_tx_fail++;
			break;
		}

		Mwrite = bcm_ssi_get_len(strm->ctrl_byte, tx->data);

		if (strm->ctrl_byte & SSI_MODE_FULL_DUPLEX) {
			unsigned char *data_p = rx->data + strm->pckt_len;

			Nread = bcm_ssi_get_len(strm->ctrl_byte, rx->data);

			if (Mwrite < Nread) {
				/* Call BBD */
				bbd_parse_asic_data(data_p, Mwrite, NULL, priv);
				Nread -= Mwrite;
				bytes_to_write -= Mwrite;
				data_p += (Mwrite + strm->fc_len);
			}

			/* Call BBD */
			if (bytes_to_write != 0)
				bbd_parse_asic_data(data_p, Nread, NULL, priv);

#ifdef CONFIG_TRANSFER_STAT
			bcm_ssi_calc_trans_stat(&priv->trans_stat[1],
			Mwrite+Nread);
#endif
		}


		if (strm->fc_len != 0) {
			Bwritten = bcm_ssi_get_len(strm->ctrl_byte,
			&rx->data[strm->pckt_len + Mwrite]);

			if (Mwrite != Bwritten) {

				ssi_tx_fc_retries++;

				if (Mwrite < Bwritten) {

					ssi_tx_fc_retry_errors++;
					pr_err("[SSPBBD]: %s @ FC error %d\n",
						__func__,
						ssi_tx_fc_retry_errors-1);
					break;
				}
			}
			n = (int)(Mwrite - Bwritten);

			if (n == 0)
				break;

			Mwrite  -= Bwritten;
			memcpy(tx->data + strm->pckt_len,
				   tx->data + strm->pckt_len +
			       Bwritten, Mwrite);
		}

	} while (--retry_ctr > 0);

#ifdef CONFIG_TRANSFER_STAT
	bcm_ssi_calc_trans_stat(&priv->trans_stat[0], length);
#endif

	return ret;
}

static int bcm_ssi_rx(struct bcm_spi_priv *priv, size_t *length)
{
	struct bcm_ssi_tx_frame *tx = priv->tx_buf;
	struct bcm_ssi_rx_frame *rx = priv->rx_buf;
	struct bcm_spi_strm_protocol *strm = &priv->rx_strm;
	int fail = -1;
	unsigned short ctrl_len = strm->pckt_len+1;
	unsigned short payload_len;


#ifdef CONFIG_TRANSFER_STAT
	bcm_ssi_calc_trans_stat(&priv->trans_stat[1], *length);
#endif



	bcm_ssi_clr_len(strm->ctrl_byte, tx->data);
	tx->cmd = strm->ctrl_byte;
	rx->status = 0;

	if (bcm_spi_sync(priv, tx, rx, ctrl_len, 8))
		return fail;

	/* Check Sanity */
	if (false && rx->status) {
		pr_err("[SSPBBD] spi_sync error, status = 0x%02X\n",
			   rx->status);
		return fail;
	}

	payload_len = bcm_ssi_get_len(strm->ctrl_byte, rx->data);



	if (payload_len == 0) {
		payload_len = MIN_SPI_FRAME_LEN;
		pr_err("[SSPBBD] rx->len is still read to 0. set %d\n",
			   payload_len);
	}

	/* TODO: limit max payload to 254 because of exynos3 bug */
	{

		*length = min((unsigned short)(strm->frame_len-ctrl_len),
		payload_len);
		memset(tx->data, 0, *length + ctrl_len - 1);

		if (bcm_spi_sync(priv, tx, rx, *length+ctrl_len, 8))
			return fail;
	}

	/* Check Sanity */
	if (false && rx->status) {
		pr_err("[SSPBBD] spi_sync error, status = 0x%02X\n",
			   rx->status);
		return fail;
	}

	payload_len = bcm_ssi_get_len(strm->ctrl_byte, rx->data);
	if (payload_len < *length)
		*length = payload_len;

#ifdef CONFIG_TRANSFER_STAT
	bcm_ssi_calc_trans_stat(&priv->trans_stat[1], *length);
#endif

	return 0;
}

void bcm_on_packet_received(void *_priv, unsigned char *data,
							unsigned int size)
{
	struct bcm_spi_priv *priv = (struct bcm_spi_priv *)_priv;
	struct circ_buf *rd_circ = &priv->read_buf;
	size_t written = 0, avail = size;


	/* Copy into circ buffer */
	mutex_lock(&priv->rlock);
	do {
		size_t space_to_end = CIRC_SPACE_TO_END(rd_circ->head,
			rd_circ->tail,
			BCM_SPI_READ_BUF_SIZE);
		size_t copied = min(space_to_end, avail);

		memcpy((void *) rd_circ->buf + rd_circ->head,
			data + written, copied);
		avail -= copied;
		written += copied;
		rd_circ->head = (rd_circ->head + copied)
		& (BCM_SPI_READ_BUF_SIZE-1);

	} while (avail > 0 && CIRC_SPACE(rd_circ->head, rd_circ->tail,
			BCM_SPI_READ_BUF_SIZE));

	priv->packet_received += size;
	mutex_unlock(&priv->rlock);
	wake_up(&priv->poll_wait);

	if (avail > 0)
		pr_err("[SSPBBD]: input overrun error by %zd bytes!\n", avail);
}


static void bcm_start_tx_work(struct work_struct *work)
{
	struct bcm_spi_priv *priv = container_of(work,
			struct bcm_spi_priv, start_tx);

	spin_lock(&priv->irq_lock);




	queue_work(priv->serial_wq, &priv->rxtx_work);

	spin_unlock(&priv->irq_lock);

}


static void bcm_rxtx_work(struct work_struct *work)
{
	struct bcm_spi_priv *priv = container_of(work,
		struct bcm_spi_priv, rxtx_work);
	struct circ_buf *wr_circ = &priv->write_buf;
	struct bcm_spi_strm_protocol *strm = &priv->tx_strm;
	unsigned short rx_pckt_len = priv->rx_strm.pckt_len;


	if (!bcm477x_hello(priv)) {
		pr_err("[SSPBBD]: %s timeout!!\n", __func__);
		return;
	}



	do {
		int    ret;
		size_t avail = 0;

		/* Read first */
		ret = gpio_get_value(priv->host_req);



		if (ret) {
			/* Receive SSI frame */
			if (bcm_ssi_rx(priv, &avail))
				break;
			/* Call BBD */

			bbd_parse_asic_data(priv->rx_buf->data +
				rx_pckt_len, avail, NULL, priv);
		}

		/* Next, write */
		avail = CIRC_CNT(wr_circ->head,
			wr_circ->tail, BCM_SPI_WRITE_BUF_SIZE);



		if (avail) {
			size_t written = 0;

			mutex_lock(&priv->wlock);
			if (avail > (strm->frame_len - strm->ctrl_len))
				avail = strm->frame_len - strm->ctrl_len;
			written = 0;

			/* Copy from wr_circ the data */
			do {
				size_t cnt_to_end =
					CIRC_CNT_TO_END(wr_circ->head,
					wr_circ->tail,
					BCM_SPI_WRITE_BUF_SIZE);
					size_t copied = min(cnt_to_end, avail);

				memcpy(priv->tx_buf->data
					+ strm->pckt_len + written,
				    wr_circ->buf + wr_circ->tail,
				    copied);
				avail -= copied;
				written += copied;
				wr_circ->tail = (wr_circ->tail + copied) &
				(BCM_SPI_WRITE_BUF_SIZE-1);
			} while (avail > 0);


			/* Transmit SSI frame */
			ret = bcm_ssi_tx(priv, written);
			mutex_unlock(&priv->wlock);

			if (ret)
				break;

			wake_up(&priv->poll_wait);

		}

	} while (gpio_get_value(priv->host_req) || CIRC_CNT(wr_circ->head,
			wr_circ->tail, BCM_SPI_WRITE_BUF_SIZE));

	bcm477x_bye(priv);

	/* Enable irq */
	{
		unsigned long int flags;

		spin_lock_irqsave(&priv->irq_lock, flags);

		/* we dont' want to enable irq when going to suspending */
		if (!atomic_read(&priv->suspending))
			if (!atomic_xchg(&priv->irq_enabled, 1))
				enable_irq(priv->spi->irq);

		spin_unlock_irqrestore(&priv->irq_lock, flags);
	}


}







static irqreturn_t bcm_irq_handler(int irq, void *pdata)
{
	struct bcm_spi_priv *priv =
		(struct bcm_spi_priv *) pdata;

	if (!gpio_get_value(priv->host_req))
		return IRQ_HANDLED;
	/* Disable irq */
	spin_lock(&priv->irq_lock);
	if (atomic_xchg(&priv->irq_enabled, 0))
		disable_irq_nosync(priv->spi->irq);

	spin_unlock(&priv->irq_lock);

	/* we don't want to queue work in suspending and shutdown */
	if (!atomic_read(&priv->suspending)) {

		queue_work(priv->serial_wq, &priv->rxtx_work);
	}

	return IRQ_HANDLED;
}

static int gps_initialize_pinctrl(struct bcm_spi_priv *data)
{
	int ret = 0;
	struct device *dev = &data->spi->dev;

	/* Get pinctrl if target uses pinctrl */
	data->ts_pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR_OR_NULL(data->ts_pinctrl)) {
		pr_err("[SSPBBD],Target does not use pinctrl\n");
		ret = PTR_ERR(data->ts_pinctrl);
		data->ts_pinctrl = NULL;
		return ret;
	}

	data->gpio_state_active
		= pinctrl_lookup_state(data->ts_pinctrl, "gps_active");
	if (IS_ERR_OR_NULL(data->gpio_state_active)) {
		pr_err("[SSPBBD]Can not get ts default pinstate\n");
		ret = PTR_ERR(data->gpio_state_active);
		data->ts_pinctrl = NULL;
		return ret;
	}

	data->gpio_state_suspend
		= pinctrl_lookup_state(data->ts_pinctrl, "gps_suspend");
	if (IS_ERR_OR_NULL(data->gpio_state_suspend)) {
		pr_err("[SSPBBD]Can not get ts sleep pinstate\n");
		ret = PTR_ERR(data->gpio_state_suspend);
		data->ts_pinctrl = NULL;
		return ret;
	}

	return 0;
}

static int gps_pinctrl_select(struct bcm_spi_priv *data, bool on)
{
	int ret = 0;
	struct pinctrl_state *pins_state;

	pins_state = on ? data->gpio_state_active : data->gpio_state_suspend;
	if (!IS_ERR_OR_NULL(pins_state)) {
		ret = pinctrl_select_state(data->ts_pinctrl, pins_state);
		if (ret) {
			pr_err("[SSPBBD]can not set %s pins\n",
					on ? "gps_active" : "gps_suspend");
			return ret;
		}
	} else {
		pr_err("[SSPBBD]not a valid '%s' pinstate\n",
				on ? "gps_active" : "gps_suspend");
	}

	return ret;
}








static int bcm_spi_suspend(struct device *dev, pm_message_t state)
{
	struct spi_device *spi = to_spi_device(dev);
	struct bcm_spi_priv *priv = (struct bcm_spi_priv *)
		spi_get_drvdata(spi);
	unsigned long int flags;

	atomic_set(&priv->suspending, 1);
	/* Disable irq */
	spin_lock_irqsave(&priv->irq_lock, flags);
	if (atomic_xchg(&priv->irq_enabled, 0))
		disable_irq_nosync(spi->irq);
	spin_unlock_irqrestore(&priv->irq_lock, flags);
	flush_workqueue(priv->serial_wq);
	return 0;
}

static int bcm_spi_resume(struct device *dev)
{
	struct spi_device *spi = to_spi_device(dev);
	struct bcm_spi_priv *priv = (struct bcm_spi_priv *)
		spi_get_drvdata(spi);
	unsigned long int flags;

	atomic_set(&priv->suspending, 0);

	/* Enable irq */
	spin_lock_irqsave(&priv->irq_lock, flags);
	if (!atomic_xchg(&priv->irq_enabled, 1))
		enable_irq(spi->irq);
	spin_unlock_irqrestore(&priv->irq_lock, flags);
	return 0;
}

static void bcm_spi_shutdown(struct spi_device *spi)
{
	struct bcm_spi_priv *priv = (struct bcm_spi_priv *)
		spi_get_drvdata(spi);
	unsigned long int flags;

	atomic_set(&priv->suspending, 1);
	/* Disable irq */
	spin_lock_irqsave(&priv->irq_lock, flags);
	if (atomic_xchg(&priv->irq_enabled, 0))
		disable_irq_nosync(spi->irq);

	spin_unlock_irqrestore(&priv->irq_lock, flags);
	flush_workqueue(priv->serial_wq);
	destroy_workqueue(priv->serial_wq);
}

static int bcm_spi_probe(struct spi_device *spi)
{
	int host_req, mcu_req, mcu_resp, nstandby, gps_power = 0;
	struct bcm_spi_priv *priv;
	bool skip_sanity;
	bool legacy_patch = false;
	int ret;
	int error = 0;

	/* Check GPIO# */
#ifndef CONFIG_OF

#else
	if (!spi->dev.of_node) {
		pr_err("[SSPBBD]: Failed to find of_node\n");
		goto err_exit;
	}
#endif

	host_req = of_get_named_gpio(spi->dev.of_node, "ssp-host-req", 0);
	mcu_req  = of_get_named_gpio(spi->dev.of_node, "ssp-mcu-req", 0);
	mcu_resp = of_get_named_gpio(spi->dev.of_node, "ssp-mcu-resp", 0);
	gps_power = of_get_named_gpio(spi->dev.of_node, "gps,power_enable", 0);
	nstandby = of_get_named_gpio(spi->dev.of_node, "gps,nstandby", 0);
	skip_sanity = of_property_read_bool(spi->dev.of_node,
										"ssp-skip-sanity");
#ifdef CONFIG_SENSORS_BBD_LEGACY_PATCH
	legacy_patch = of_property_read_bool(spi->dev.of_node,
										 "ssp-legacy-patch");
#endif

	if (host_req < 0 ||
		mcu_req < 0 ||
		mcu_resp < 0 ||
		gps_power < 0 ||
		nstandby < 0) {
		pr_err("[SSPBBD]: GPIO value not correct\n");
		goto err_exit;
	}

	/* Check IRQ# */
	spi->irq = gpio_to_irq(host_req);
	if (spi->irq < 0) {
		pr_err("[SSPBBD]: irq=%d for host_req=%d not correct\n",
			    spi->irq, host_req);
		goto err_exit;
	}

	/* Config GPIO */
	ret = gpio_request(mcu_req, "MCU REQ");
	if (ret) {
		pr_err("[SSPBBD]: failed to request MCU REQ, ret:%d", ret);
		goto err_exit;
	}
	ret = gpio_direction_output(mcu_req, 0);
	if (ret) {
		pr_err("[SSPBBD]:set MCUREQ input mode,fail:%d", ret);
		goto err_exit;
	}
	ret = gpio_request(mcu_resp, "MCU RESP");
	if (ret) {
		pr_err("[SSPBBD]: failed to request MCU RESP, ret:%d", ret);
		goto err_exit;
	}
	ret = gpio_direction_input(mcu_resp);
	if (ret) {
		pr_err("SSPBBD MCU_RESP input mode fail:%d", ret);
		goto err_exit;
	}

	ret = gpio_request(gps_power, "GPS POWER");
	if (ret) {
		pr_err("SSPBBD failed request GPS POWER :%d", ret);
		goto err_exit;
	}
	ret = gpio_direction_output(gps_power, 1);
	if (ret) {
		pr_err("SSPBBD:set GPS POWER inputmode fail:%d", ret);
		goto err_exit;
	}
	ret = gpio_request(nstandby, "GPS NSTANDBY");
	if (ret) {
		pr_err("SSPBBD request GPS NSTANDBY fail %d", ret);
		goto err_exit;
	}
	ret = gpio_direction_output(nstandby, 0);
	if (ret) {
		pr_err("SSPBBD set GPS NSTANDBY as out mode fail %d", ret);
		goto err_exit;
	}

	/* Alloc everything */
	priv = kmalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		goto err_exit;

	memset(priv, 0, sizeof(*priv));

	priv->skip_sanity = skip_sanity;
	priv->spi = spi;

	priv->tx_buf = kmalloc(sizeof(struct bcm_ssi_tx_frame), GFP_KERNEL);
	priv->rx_buf = kmalloc(sizeof(struct bcm_ssi_rx_frame), GFP_KERNEL);
	if (!priv->tx_buf || !priv->rx_buf) {
		pr_err("[SSPBBD]:allocate xfer buffer fail,tx_buf=%p, rx_buf=%p\n",
				priv->tx_buf, priv->rx_buf);
		goto free_mem;
	}

	priv->serial_wq = alloc_workqueue("bcm477x_wq",
		WQ_HIGHPRI|WQ_UNBOUND|WQ_MEM_RECLAIM, 1);
	if (!priv->serial_wq) {
		pr_err("[SSPBBD]: Failed to allocate workqueue\n");
		goto free_mem;
	}
	/* Init - pinctrl */
	error = gps_initialize_pinctrl(priv);
	if (error || !priv->ts_pinctrl) {
		pr_err("[SSPBBD], Initialize pinctrl failed\n");
		goto free_wq;
	} else {
		error = gps_pinctrl_select(priv, true);
		if (error < 0) {
			pr_err("[SSPBBD]pinctrl_select failed\n");
			goto free_wq;
		}
	}

	/* Request IRQ */
	ret = request_irq(spi->irq, bcm_irq_handler,
		IRQF_TRIGGER_HIGH, "ttyBCM", priv);
	if (ret) {
		pr_err("[SSPBBD]: Failed to register TTY IRQ %d.\n", spi->irq);
		goto free_wq;
	}

	disable_irq(spi->irq);
	/* Register misc device */
	priv->misc.minor = MISC_DYNAMIC_MINOR;
	priv->misc.name = "ttyBCM";
	priv->misc.fops = &bcm_spi_fops;

	ret = misc_register(&priv->misc);
	if (ret) {
		pr_err("[SSPBBD]: Failed to register misc dev. err=%d\n", ret);
		goto free_irq;
	}

	/* Set driver data */
	spi_set_drvdata(spi, priv);

	/* Init - miscdev stuff */
	init_waitqueue_head(&priv->poll_wait);
	priv->read_buf.buf = priv->_read_buf;
	priv->write_buf.buf = priv->_write_buf;
	mutex_init(&priv->rlock);
	mutex_init(&priv->wlock);


	priv->busy = false;

	/* Init - work */
	INIT_WORK(&priv->rxtx_work, bcm_rxtx_work);
	INIT_WORK(&priv->start_tx, bcm_start_tx_work);

	/* Init - irq stuff */
	spin_lock_init(&priv->irq_lock);
	atomic_set(&priv->irq_enabled, 0);
	atomic_set(&priv->suspending, 0);

	/* Init - gpios */
	priv->host_req = host_req;
	priv->mcu_req  = mcu_req;
	priv->mcu_resp = mcu_resp;
	priv->nstandby = nstandby;

	/* Init - etc */


	g_bcm_gps = priv;
	/* Init BBD & SSP */
	bbd_init(&spi->dev, legacy_patch);
	if (device_create_file(&priv->spi->dev, &dev_attr_nstandby))
		pr_err("Unable to create sysfs 4775 nstandby entry");

	return 0;

free_irq:
	if (spi->irq)
		free_irq(spi->irq, priv);
free_wq:
	if (priv->serial_wq)
		destroy_workqueue(priv->serial_wq);
free_mem:
		kfree(priv->tx_buf);
		kfree(priv->rx_buf);
		kfree(priv);
err_exit:
	return -ENODEV;
}


static int bcm_spi_remove(struct spi_device *spi)
{
	struct bcm_spi_priv *priv = (struct bcm_spi_priv *)
		spi_get_drvdata(spi);
	unsigned long int flags;



	atomic_set(&priv->suspending, 1);

	/* Disable irq */
	spin_lock_irqsave(&priv->irq_lock, flags);
	if (atomic_xchg(&priv->irq_enabled, 0))
		disable_irq_nosync(spi->irq);

	spin_unlock_irqrestore(&priv->irq_lock, flags);

	/* Flush work */
	flush_workqueue(priv->serial_wq);
	destroy_workqueue(priv->serial_wq);
	if (priv->ts_pinctrl) {
		if (gps_pinctrl_select(priv, false) < 0)
			pr_err("[SSPBBD]Cannot get idle pinctrl state\n");
	}

	/* Free everything */

	free_irq(spi->irq, priv);
	device_remove_file(&priv->spi->dev, &dev_attr_nstandby);
	kfree(priv->tx_buf);
	kfree(priv->rx_buf);
	kfree(priv);

	g_bcm_gps = NULL;

	return 0;
}

void bcm477x_debug_info(const char *buf)
{
	int pin_ttyBCM, pin_MCU_REQ, pin_MCU_RESP;
	int irq_enabled, irq_count;

	if (g_bcm_gps) {
		pin_ttyBCM = gpio_get_value(g_bcm_gps->host_req);
		pin_MCU_REQ = gpio_get_value(g_bcm_gps->mcu_req);
		pin_MCU_RESP = gpio_get_value(g_bcm_gps->mcu_resp);

		irq_enabled = atomic_read(&g_bcm_gps->irq_enabled);
		irq_count = kstat_irqs_cpu(g_bcm_gps->spi->irq, 0);
	}
}

static const struct spi_device_id bcm_spi_id[] = {
	{"ssp", 0},
	{}
};
MODULE_DEVICE_TABLE(spi, bcm_spi_id);

#ifdef CONFIG_OF
static const struct of_device_id match_table[] = {
	{ .compatible = "ssp,bcm4775",},
	{},
};
#endif

static struct spi_driver bcm_spi_driver = {
	.id_table = bcm_spi_id,
	.probe = bcm_spi_probe,
	.remove = bcm_spi_remove,
	.shutdown = bcm_spi_shutdown,
	.driver = {
		.name = "ssp",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = match_table,
#endif
		.suspend = bcm_spi_suspend,
		.resume = bcm_spi_resume,
	},
};







static int __init bcm_spi_init(void)
{
	return spi_register_driver(&bcm_spi_driver);
}

static void __exit bcm_spi_exit(void)
{
	spi_unregister_driver(&bcm_spi_driver);
}

module_init(bcm_spi_init);
module_exit(bcm_spi_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("BCM SPI/SSI Driver");


