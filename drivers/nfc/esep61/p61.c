/*
 * Copyright (C) 2010 Trusted Logic S.A.
 * Copyright (C) 2016 XiaoMi, Inc.
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

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
#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include <linux/spi/spi.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/timer.h>

#define P61_DBG_LEVEL 0

#define TIMER_ENABLE 0
#define IRQ_ENABLE 1
#if P61_DBG_LEVEL
#define NFC_DBG_MSG(msg...) printk(KERN_ERR "[NFC PN61] :  " msg);
#else
#define NFC_DBG_MSG(msg...)
#endif

#define NFC_ERR_MSG(msg...) printk(KERN_ERR "[NFC PN61] : " msg);

#define P61_IRQ   24
#define P61_RST   62
#define P61_PWR   16
#define P61_CS   90

#define P61_mosi   0
#define P61_somi   1
#define P61_clk   3

#define NFC_p61_rst	GPIO_CFG(P61_RST, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA)

#define P61_MAGIC 0xE8
/*
 * PN544 power control via ioctl
 * PN544_SET_PWR(0): power off
 * PN544_SET_PWR(1): power on
 * PN544_SET_PWR(2): reset and power on with firmware download enabled */
#define P61_SET_PWR _IOW(P61_MAGIC, 0x01, unsigned int)
#define MAX_BUFFER_SIZE 4096
int  sendFrame(struct file *filp, const char __user data[],  char mode, int count);
int  sendChainedFrame(struct file *filp, const unsigned char __user data[], int count);
static int p61_dev_close(void);
unsigned char *apduBuffer;
unsigned char *apduBuffer1;
unsigned char *gRecvBuff;
unsigned char *checksum;
int apduBufferidx = 0;
int apduBufferlen = 256;
const char PH_SCAL_T1_CHAINING = 0x20;
const char PH_SCAL_T1_SINGLE_FRAME = 0x00;
const char PH_SCAL_T1_R_BLOCK = 0x80;
const char PH_SCAL_T1_S_BLOCK = 0xC0;
const char PH_SCAL_T1_HEADER_SIZE_NO_NAD = 0x02;
static unsigned char seqCounterCard;
static unsigned char seqCounterTerm = 1;
short ifs = 254;
short headerSize = 3;
unsigned char sof = 0xA5;
unsigned char csSize = 1;
static unsigned char array[];
const char C_TRANSMIT_NO_STOP_CONDITION = 0x01;
const char C_TRANSMIT_NO_START_CONDITION = 0x02;
const char C_TRANSMIT_NORMAL_SPI_OPERATION = 0x04;

typedef struct respData {
	unsigned char *data;
	int len;
} respData_t;

respData_t *gStRecvData = NULL;
unsigned char *gSendframe = NULL;
unsigned char *gDataPackage = NULL;
unsigned char *data1 = NULL;

#define MEM_CHUNK_SIZE (256)
unsigned char *lastFrame = NULL;
int lastFrameLen;
void init(void);
void setAddress(short address);
void setBitrate(short bitrate);
int nativeSetAddress(short address);
int nativeSetBitrate(short bitrate);
unsigned char helperComputeLRC(unsigned char data[], int offset, int length);
void receiveAcknowledge(struct file *filp);
void receiveAndCheckChecksum(struct file *filp, short rPcb, short rLen, unsigned char data[], int len);
respData_t  *receiveHeader(struct file *filp);
respData_t  *receiveFrame(struct file *filp, short rPcb, short rLen);
int send(struct file *filp, unsigned char **data, unsigned char mode, int len);
int receive(struct file *filp, unsigned char **data, int len, unsigned char mode);
respData_t  *receiveChainedFrame(struct file *filp, short rPcb, short rLen);
void sendAcknowledge(struct file *filp);
static ssize_t p61_dev_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *offset);
static ssize_t p61_dev_read(struct file *filp, char __user *buf,
		size_t count, loff_t *offset);
static ssize_t p61_dev_receiveData(struct file *filp, char __user *buf,
		size_t count, loff_t *offset);
/*static ssize_t p61_dev_sendData(struct file *filp, const char __user *buf,
		size_t count, loff_t *offset);
	*/
static respData_t *p61_dev_receiveData_internal(struct file *filp);

poll_table *wait;

struct p61_dev{
	wait_queue_head_t       read_wq;
	struct mutex            read_mutex;
	struct spi_device       *spi;
	struct miscdevice       p61_device;
	char *rp, *wp;
	unsigned int            irq_gpio;
	bool                    irq_enabled;
	spinlock_t              irq_enabled_lock;
	wait_queue_head_t *inq, *outq;
	int buffersize;
	struct spi_message msg;
	struct spi_transfer transfer;
	bool firm_gpio;
	bool ven_gpio;
};

struct p61_control {
	struct spi_message msg;
	struct spi_transfer transfer;
	unsigned char *tx_buff;
	unsigned char *rx_buff;
};


static int timer_expired;
#ifdef TIMER_ENABLE
static struct timer_list recovery_timer;
static int timer_started;
static void p61_disable_irq(struct p61_dev *p61_dev);
void my_timer_callback(unsigned long data)
{
	timer_expired = 1;
}

static int start_timer(void)
{
	int ret;
	setup_timer(&recovery_timer, my_timer_callback, 0);
	ret = mod_timer(&recovery_timer, jiffies + msecs_to_jiffies(2000));
	if (ret)
		printk(KERN_INFO "Error in mod_timer\n");
	else
		timer_started = 1;

	return 0;
}

void cleanup_timer(void)
{
	int ret;

	ret = del_timer(&recovery_timer);
	return;
}
#endif

static int p61_dev_open(struct inode *inode, struct file *filp)
{
	struct p61_dev *p61_dev = container_of(filp->private_data,
			struct p61_dev,
			p61_device);
	filp->private_data = p61_dev;
	gRecvBuff = (unsigned char *)kmalloc(300, GFP_KERNEL);
	gStRecvData = (respData_t *)kmalloc(sizeof(respData_t), GFP_KERNEL);
	checksum = (unsigned char *)kmalloc(csSize, GFP_KERNEL);
	gSendframe = (unsigned char *)kmalloc(300, GFP_KERNEL);
	gDataPackage = (unsigned char *)kmalloc(300, GFP_KERNEL);
	apduBuffer = (unsigned char *)kmalloc(apduBufferlen, GFP_KERNEL);
	data1 = (unsigned char *)kmalloc(1, GFP_KERNEL);
	NFC_DBG_MSG("%s :  Major No: %d, Minor No: %d\n", __func__, imajor(inode), iminor(inode));
	return 0;
}

static int p61_dev_close(void)
{
	if (checksum != NULL)
	kfree(checksum);
	if (gRecvBuff != NULL)
	kfree(gRecvBuff);
	if (gStRecvData != NULL)
	kfree(gStRecvData);
	if (gSendframe != NULL)
	kfree(gSendframe);
	if (gDataPackage != NULL)
	kfree(gDataPackage);
	if (apduBuffer != NULL)
	kfree(apduBuffer);
	if (data1 != NULL)
	kfree(data1);
	return 0;
}

void SpiReadSingle(struct spi_device *spi, unsigned char *pTxbuf, unsigned char *pbuf, unsigned int length)
{
	while (length) {
		length--;
		spi_write_then_read(spi, pTxbuf, 1, pbuf, 1);
		pbuf++;
		pTxbuf++;
	}
}



static unsigned int p61_dev_poll (struct file *filp, struct poll_table_struct *buf)
{
	int mask = -1;
	int left = 0;
	struct p61_dev *p61_dev = filp->private_data;
	NFC_DBG_MSG(KERN_INFO  "p61_dev_poll called  \n");
	left = (p61_dev->rp + p61_dev->buffersize - p61_dev->wp) % (p61_dev->buffersize);
	poll_wait(filp, p61_dev->inq, wait);
	if (p61_dev->rp != p61_dev->wp)
		mask |= POLLIN|POLLRDNORM;
	if (left != 1)
		mask |= POLLOUT|POLLRDNORM;
	return mask;
}

/**
     * Entry point function for receiving data. Based on the PCB byte this function
     * either receives a single frame or a chained frame.
     *
     */
/*static ssize_t p61_dev_receiveData(struct file *filp, char *buf,
		size_t count, loff_t *offset)
		*/
static ssize_t p61_dev_receiveData(struct file *filp, char __user *buf,
		size_t count, loff_t *offset)
{
	respData_t *rsp = p61_dev_receiveData_internal(filp);


	count = rsp->len;

	if (0 < count) {

		if (copy_to_user(buf, rsp->data, count)) {
			NFC_DBG_MSG("%s : failed to copy to user space\n", __func__);
			return -EFAULT;
		}
	}

	return count;
}

static respData_t *p61_dev_receiveData_internal(struct file *filp)
{
	short rPcb = 0;
	short rLen = 0;
	respData_t *header = NULL;
	respData_t *respData = NULL;
	unsigned char *wtx = NULL;
	unsigned char *data = NULL;

	int len = 0;
	int len1 = 0;
	int stat_timer;
Start:
	NFC_DBG_MSG(KERN_INFO  "receiveData -Enter\n");


	header = (respData_t *)receiveHeader(filp);
	if (header == NULL) {
		NFC_ERR_MSG(KERN_ALERT "ERROR:Failed to receive header data\n");
		return NULL;
	}
	rPcb = header->data[0];
	rLen = (short) (header->data[1] & 0xFF);
	NFC_DBG_MSG(KERN_ALERT "receive header data rPcb = 0x%x , rLen = %d\n", rPcb, rLen);

	if ((rPcb & PH_SCAL_T1_S_BLOCK) == PH_SCAL_T1_S_BLOCK) {
		NFC_DBG_MSG(KERN_ALERT "receiveDatav - WTX requested\n");
		data = gRecvBuff;
		len = 1;
		NFC_DBG_MSG(KERN_ALERT "receiveDatav - WTX1 requested\n");
		receive(filp, &data, len, C_TRANSMIT_NO_STOP_CONDITION | C_TRANSMIT_NO_START_CONDITION);
		NFC_DBG_MSG(KERN_ALERT "receiveDatav - WTX2 requested\n");
		receiveAndCheckChecksum(filp, rPcb, rLen, data, len);
		NFC_DBG_MSG(KERN_ALERT "receiveDatav - WTX3 requested\n");
		NFC_DBG_MSG(KERN_ALERT "value is %x %x", data[0], data[1]);
		memset(gRecvBuff, 0, 300);
		wtx = gRecvBuff;
		wtx[0] = 0x00;
		wtx[1] = 0xE3;
		wtx[2] = 0x01;
		wtx[3] = 0x01;
		wtx[4] = 0xE3;
		len1 = 5;
		udelay(1000);
		send(filp, &wtx, C_TRANSMIT_NORMAL_SPI_OPERATION, len1);
		udelay(1000);



#ifdef TIMER_ENABLE
		stat_timer = start_timer();

		goto Start;
	}


	if ((rPcb & PH_SCAL_T1_R_BLOCK) == PH_SCAL_T1_R_BLOCK) {
		memset(data1, 0, 1);
		len1 = 1;
		receiveAndCheckChecksum(filp, rPcb, rLen, data1, len1);
		udelay(1000);
		send(filp, &lastFrame, C_TRANSMIT_NORMAL_SPI_OPERATION, lastFrameLen);
		udelay(1000);
		goto Start;

	}


	if ((rPcb & PH_SCAL_T1_CHAINING) == PH_SCAL_T1_CHAINING) {
		NFC_DBG_MSG(KERN_ALERT "Chained Frame Requested\n");
		return receiveChainedFrame(filp, rPcb, rLen);
	} else {
		NFC_DBG_MSG(KERN_ALERT "receiveFrame Requested\n");
		respData = receiveFrame(filp, rPcb, rLen);
		NFC_DBG_MSG(KERN_ALERT "***************** 0x%x \n", respData->data[0]);
		return respData;
	}
#endif
	return NULL;
}
/**
    * This function is used to receive a single T = 1 frame
    *
    * @param rPcb
    *            PCB field of the current frame
    * @param rLen
    *            LEN field of the current frame
    * @param filp
    * 			 File pointer
    */

respData_t *receiveFrame(struct file *filp, short rPcb, short rLen)
{

	int status = 0;
	respData_t *respData = NULL;
	NFC_DBG_MSG(KERN_ALERT "receiveFrame -Enter\n");
	respData = gStRecvData;
	respData->data = gRecvBuff;
	respData->len = rLen;

	seqCounterCard = (seqCounterCard ^ 1);


	status = receive(filp, &(respData->data), respData->len, C_TRANSMIT_NO_STOP_CONDITION | C_TRANSMIT_NO_START_CONDITION);

	receiveAndCheckChecksum(filp, rPcb, rLen, respData->data, respData->len);

	NFC_DBG_MSG(KERN_ALERT "receiveFrame -Exit\n");

	return respData;
}

/**
     * This function is used to receive a chained frame.
     *
     * @param rPcb
     *            PCB field of the current frame
     * @param rLen
     *            LEN field of the current frame
     * @param filp
     *            File pointer
     */

respData_t *receiveChainedFrame(struct file *filp, short rPcb, short rLen)
{
	respData_t *data_rec = NULL;
	respData_t *header = NULL ;
	respData_t *respData = NULL;
	respData_t *apdbuff = NULL;
	NFC_DBG_MSG(KERN_ALERT "receiveChainedFrame -Enter\n");

	do {

		NFC_DBG_MSG(KERN_ALERT "p61_dev_read - test4 count [0x%x] \n", rLen);
		data_rec = receiveFrame(filp, rPcb, rLen);

		memcpy((apduBuffer+apduBufferidx), data_rec->data, data_rec->len);


		apduBufferidx += data_rec->len;


		udelay(1000);
		sendAcknowledge(filp);
		udelay(1000);

		header = receiveHeader(filp);

		rPcb = header->data[0];
		rLen = (header->data[1] & 0xFF);

	} while ((rPcb & PH_SCAL_T1_CHAINING) == PH_SCAL_T1_CHAINING);


	respData = receiveFrame(filp, rPcb, rLen);
	memcpy(apduBuffer+apduBufferidx, respData->data, respData->len);

	apduBufferidx += respData->len;


	apdbuff = (respData_t *)kmalloc(sizeof(respData_t), GFP_KERNEL);
	if (apdbuff == NULL) {
		NFC_ERR_MSG(KERN_ALERT "receiveChainedFrame 2-KMALLOC FAILED!!!\n");
		return NULL;
	}

	apdbuff->data = (unsigned char *)kmalloc(apduBufferidx, GFP_KERNEL);
	if (apdbuff->data == NULL) {
		NFC_ERR_MSG(KERN_ALERT "receiveChainedFrame 3-KMALLOC FAILED!!!\n");
		return NULL;
	}
	memcpy(apdbuff->data, apduBuffer, apduBufferidx);
	apdbuff->len = apduBufferidx;

	NFC_DBG_MSG(KERN_ALERT "receiveChainedFrame -Exit\n");
	return apdbuff;
}
/**
    * This function is used to send an acknowledge for an received I frame
    * in chaining mode.
    *
    */
void sendAcknowledge(struct file *filp)
{
	unsigned char *ack = NULL;
	NFC_DBG_MSG(KERN_ALERT "sendAcknowledge - Enter\n");
	ack = gSendframe;

	NFC_DBG_MSG(KERN_ALERT "seqCounterCard value is [0x%x]\n", seqCounterCard);
	ack[0] = 0x00;
	ack[1] = (unsigned char)(PH_SCAL_T1_R_BLOCK | (unsigned char)(seqCounterCard << 4));
	ack[2] = 0x00;
	ack[3] = helperComputeLRC(ack, 0, (sizeof(ack) / sizeof(ack[0])) - 2);

	send(filp, &ack, C_TRANSMIT_NORMAL_SPI_OPERATION, sizeof(ack)/sizeof(ack[0]));

	NFC_DBG_MSG(KERN_ALERT "sendAcknowledge - Exit\n");

}
/**
    * This function sends either a chained frame or a single T = 1 frame
    *
    * @param buf
    *            the data to be send
    *
    */
/*static ssize_t p61_dev_sendData(struct file *filp, unsigned char *buf,
		size_t count, loff_t *offset)
		*/
static ssize_t p61_dev_sendData(struct file *filp, const char __user *buf,
		size_t count, loff_t *offset)
{
	int ret = -1;
	int i;
	init();
	printk("p61_dev_sendData %d - Enter \n", (int)count);
	if (count <= ifs) {
		for (i = 0; i <= count; i++) {
			NFC_ERR_MSG("p61_dev_sendData : buf%d[0x%x]", i, buf[i]);
		}
		ret = sendFrame(filp, buf, PH_SCAL_T1_SINGLE_FRAME, count);
		printk("Vaue of count_status is %d \n", ret);
	} else
		ret = sendChainedFrame(filp, buf, count);

	NFC_DBG_MSG(KERN_INFO "p61_dev_sendData: count_status is %d \n", ret);
	return ret;
}

static long  p61_dev_ioctl(struct file *filp, unsigned int cmd,
				unsigned long arg)
{
	int ret;
	struct p61_dev *p61_dev = NULL;
	uint8_t buf[100];

	p61_dev = filp->private_data;
	p61_dev->ven_gpio = P61_RST;
	printk("p61_dev_ioctl enter\n");

	switch (cmd) {
	case P61_SET_PWR:
		NFC_DBG_MSG(KERN_ALERT "P61_SET_PWR-Enter P61_RST = 0x%x\n", P61_RST);
		if (arg == 2) {
			printk("p61_dev_ioctl   download firmware \n");
			/* power on with firmware download (requires hw reset)*/
			gpio_direction_output(P61_RST, 1);
			NFC_DBG_MSG(KERN_ALERT "p61_dev_ioctl-1\n");
			msleep(20);
			gpio_direction_output(P61_RST, 0);
			NFC_DBG_MSG(KERN_ALERT "p61_dev_ioctl-0\n");
			msleep(50);
			ret = spi_read(p61_dev->spi, (void *) buf, sizeof(buf));
			msleep(50);
			gpio_direction_output(P61_RST, 1);
			NFC_DBG_MSG(KERN_ALERT "p61_dev_ioctl-1 \n");
			msleep(20);

		} else if (arg == 1) {
			printk("p61_dev_ioctl   power on \n");
			/* power on */
			NFC_DBG_MSG(KERN_ALERT "p61_dev_ioctl-1 (arg = 1)\n");
			gpio_direction_output(P61_RST, 1);

		} else  if (arg == 0) {
			printk("p61_dev_ioctl   power off \n");
			/* power off */
			NFC_DBG_MSG(KERN_ALERT "p61_dev_ioctl-0 (arg = 0)\n");
			gpio_direction_output(P61_RST, 0);
			udelay(100);
		} else {
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}



/**
     * This function is used to send a chained frame.
     *
     * @param data
     *            the data to be send
     */
int sendChainedFrame(struct file *filp, const unsigned char data[], int len)
{
	int count_status = 0 ;
	int length = len;
	int offset = 0;
	int ret = 0;
	unsigned char *lastDataPackage = NULL;
	unsigned char *dataPackage = NULL;
	NFC_DBG_MSG(KERN_INFO "sendChainedFrame - Enter\n");
	dataPackage = gDataPackage;
	do {
		NFC_DBG_MSG(KERN_INFO "sendChainedFrame \n");

		memcpy(&dataPackage[0], &data[offset], ifs);

		count_status = sendFrame(filp, dataPackage, PH_SCAL_T1_CHAINING, ifs);
		if (count_status == 0) {
			NFC_ERR_MSG(KERN_INFO "ERROR1: Failed to send Frame\n");
			return -EPERM;
		}
		receiveAcknowledge(filp);
		udelay(1000);
		length = length - ifs;
		offset = offset + ifs;
		ret += count_status;
	} while (length > ifs);

	lastDataPackage = gDataPackage;
	memcpy(&lastDataPackage[0], &data[offset], length);

	count_status = sendFrame(filp, lastDataPackage, PH_SCAL_T1_SINGLE_FRAME, length);

	if (count_status == 0) {
		NFC_ERR_MSG(KERN_INFO "ERROR2:Failed to send Frame\n");
		return -EPERM;
	}
	NFC_DBG_MSG(KERN_INFO "sendChainedFrame - Exit\n");
	ret += count_status;
	return ret;
}
/**
     * This function is used to receive an Acknowledge of an I frame
     *
     */
void receiveAcknowledge(struct file *filp)
{
	respData_t *header = NULL;
	short rPcb = 0;
	short rLen = 0;
	int len = 1;
	unsigned char *cs = NULL;
	NFC_DBG_MSG(KERN_INFO "receiveAcknowledge - Enter\n");
	cs = gRecvBuff;
	header = (respData_t *) receiveHeader(filp);
	rPcb = (header->data[0] & 0xFF);
	rLen = (header->data[1] & 0xFF);
	receiveAndCheckChecksum(filp, rPcb, rLen, cs, len);
	NFC_DBG_MSG(KERN_ALERT "receiveAcknowledge - Exit\n");
}

/**
 * This function is used to receive the header of the next T = 1 frame.
 * If no data is available the function polls the data line as long as it receives the
 * start of the header and then receives the entire header.
 *
 */
respData_t *receiveHeader(struct file *filp)
{
	int count_status = 0;

	respData_t *header = NULL;
	unsigned char *r_frame = NULL;
	int len = 1;
	NFC_DBG_MSG(KERN_ALERT "receiveHeader - Enter\n");
	header = gStRecvData;
	header->data = gRecvBuff;
	header->len = PH_SCAL_T1_HEADER_SIZE_NO_NAD;
	count_status = receive(filp, &gRecvBuff, len, C_TRANSMIT_NO_STOP_CONDITION);
	NFC_DBG_MSG(KERN_ALERT "sof is :0x%x\n", gRecvBuff[0]);

#ifdef TIMER_ENABLE
again:
#endif

	while (gRecvBuff[0] != sof) {
		NFC_DBG_MSG(KERN_ALERT "SOF not found\n");

		count_status = receive(filp, &gRecvBuff, len, C_TRANSMIT_NO_STOP_CONDITION | C_TRANSMIT_NO_START_CONDITION);
		NFC_DBG_MSG(KERN_ALERT "in While SOF is : 0x%x \n", gRecvBuff[0]);
	}
#ifdef TIMER_ENABLE
	if (timer_started) {
		timer_started = 0;
		cleanup_timer();
	}
	if (timer_expired == 1) {
		memset(gSendframe, 0, 300);
		r_frame = gSendframe;
		r_frame[0] = 0x00;
		r_frame[1] = 0x00;
		r_frame[2] = 0x00;
		r_frame[3] = 0x00;
		timer_started = 0;
		timer_expired = 0;
		cleanup_timer();

		send(filp, &r_frame, C_TRANSMIT_NORMAL_SPI_OPERATION, 4);
		goto again;
	}
#endif
	NFC_DBG_MSG(KERN_ALERT "SOF FOUND\n");

	count_status = receive(filp, &(header->data), header->len, C_TRANSMIT_NO_STOP_CONDITION | C_TRANSMIT_NO_START_CONDITION);
	NFC_DBG_MSG(KERN_ALERT "receiveHeader -Exit\n");

	return header;
}
/**
     * This function is used to receive and check the checksum of the T = 1 frame.
     *
     * @param rPcb
     *            PCB field of the current frame
     * @param rLen
     *            LEN field of the current frame
     * @param data
     *            DATA field of the current frame
     * @param dataLength
     *
     * @param filp
     * 			  File pointer
     *
     */
void receiveAndCheckChecksum(struct file *filp, short rPcb, short rLen, unsigned char data[], int dataLength)
{
	int lrc = rPcb ^ rLen;
	int receivedCs = 0;
	int expectedCs = 0;
	NFC_DBG_MSG(KERN_INFO "receiveAndCheckChecksum -Enter\n");

	dataLength = dataLength - csSize;


	expectedCs = lrc ^ helperComputeLRC(data, 0, dataLength);


	receive(filp, &checksum, csSize, C_TRANSMIT_NO_START_CONDITION);

	receivedCs = checksum[0];


	if (expectedCs != receivedCs)
		NFC_DBG_MSG(KERN_INFO "Checksum error \n");

	NFC_DBG_MSG(KERN_INFO "receiveAndCheckChecksum -Exit\n");
}
/**
     * Basic send function which directly calls the spi bird wrapper function
     *
     * @param data
     *            the data to be send
     *
     */
int send(struct file *filp, unsigned char **data, unsigned char mode, int len)
{
	int count = 0;
	NFC_DBG_MSG(KERN_ALERT "send - Enter\n");
	NFC_DBG_MSG(KERN_ALERT "send - len = %d\n", len);


	count = p61_dev_write(filp, *data, len, 0x00);

	if (count == 0) {
		NFC_ERR_MSG(KERN_ALERT "ERROR:Failed to send data to device\n");
		return -EPERM;
	}
	return count;
}

int receive(struct file *filp, unsigned char **data, int len, unsigned char mode)
{
	static int count_status;
	NFC_DBG_MSG(KERN_ALERT "receive -Enter\n");
	count_status = p61_dev_read(filp, *data, len, 0x00);
	if (count_status == 0 && len != 0) {
		NFC_ERR_MSG(KERN_ALERT "ERROR:Failed to receive data from device\n");
		return -EPERM;
	}

	NFC_DBG_MSG(KERN_ALERT "receive -Exit\n");

	return count_status;
}

static ssize_t p61_dev_write(struct file *filp, const char *buf,
		size_t count, loff_t *offset)
{
	int ret = -1;
	struct p61_dev  *p61_dev = NULL;
#if P61_DBG_LEVEL
	int i;
	for (i = 0; i <= count; i++) {
		NFC_DBG_MSG("p61_dev_write : buf%d[0x%x]", i, buf[i]);
	}
#endif

	NFC_ERR_MSG(KERN_ALERT "p61_dev_write -Enter\n");
	gpio_direction_output(P61_CS, 0);
	p61_dev = filp->private_data;
	/* Write data */
	ret = spi_write(p61_dev->spi, buf, count);
	NFC_DBG_MSG ("spi_write status = %d\n", ret);
	if (ret < 0) {
		ret = -EIO;
	}

	NFC_DBG_MSG(KERN_ALERT "p61_dev_write -Exit\n");
	udelay(1000);
	gpio_direction_output(P61_CS, 1);
	return count;
}

static ssize_t p61_dev_read(struct file *filp, char  *buf,
		size_t count, loff_t *offset)
{
	int ret = -1;
	struct p61_dev *p61_dev = filp->private_data;
	NFC_DBG_MSG(KERN_ALERT "p61_dev_read - Enter \n");
	gpio_direction_output(P61_CS, 0);
	mutex_lock(&p61_dev->read_mutex);
	NFC_ERR_MSG(KERN_ALERT "p61_dev_read - aquried mutex - calling spi_read \n");
	NFC_ERR_MSG(KERN_ALERT "p61_dev_read - test1 count [0x%x] \n", (int)count);
	/** Read data */
#ifdef IRQ_ENABLE
	NFC_DBG_MSG(KERN_ALERT "************ Test11 *****************\n");
	if (!gpio_get_value(p61_dev->irq_gpio)) {
		while (1) {
			NFC_DBG_MSG(KERN_ALERT "************ Test1 *****************\n");
			NFC_DBG_MSG(" %s inside while(1) \n", __FUNCTION__);
			p61_dev->irq_enabled = true;
			enable_irq(p61_dev->spi->irq);
			ret = wait_event_interruptible(p61_dev->read_wq, !p61_dev->irq_enabled);
			p61_disable_irq(p61_dev);
			if (ret) {
				NFC_DBG_MSG("p61_disable_irq() : Failed\n");
				goto fails;
			}
			NFC_DBG_MSG(KERN_ALERT "************ Test2 *****************\n");
			if (gpio_get_value(p61_dev->irq_gpio))
				break;

			NFC_DBG_MSG("%s: spurious interrupt detected\n", __func__);
		}
	}

	NFC_DBG_MSG(KERN_ALERT "************  gpio already high read data Test11 *****************\n");
#endif
	NFC_DBG_MSG(KERN_ALERT "************ Test3 *****************\n");
	ret = spi_read (p61_dev->spi, (void *) buf, count);

#if P61_DBG_LEVEL
	int i;
	for (i = 0; i <= count; i++) {
		NFC_DBG_MSG(" == WAC == buf%d[0x%x]", i, buf[i]);
	}
#endif

	if (0 > ret) {
		NFC_ERR_MSG(KERN_ALERT "spi_read returns -1 \n");
		goto fails;
	}

	NFC_DBG_MSG(KERN_ALERT "Read ret %d \n", ret);
	mutex_unlock(&p61_dev->read_mutex);
	gpio_direction_output(P61_CS, 1);

	if (0 == ret) {
		ret = count;
	}

	return ret;

	fails:
	mutex_unlock(&p61_dev->read_mutex);
	gpio_direction_output(P61_CS, 1);
	return ret;
}

/**
 * This function is used to send a single T = 1 frame.
 *
 * @param data
 *            the data to be send
 * @param mode
 *            used to signal chaining
 *
 */
int sendFrame(struct file *filp, const char data[], char mode, int count)
{
	int count_status = 0;
	int len = count + headerSize + csSize;
	unsigned char *frame = NULL;
	NFC_ERR_MSG(KERN_INFO "sendFrame - Enter\n");
	frame = gSendframe;

	seqCounterTerm = (unsigned char)(seqCounterTerm ^ 1);


	frame[0] = 0x00;
	frame[1] = (unsigned char)(mode | (unsigned char) (seqCounterTerm << 6));
	frame[2] = (unsigned char)(count);

	memcpy((frame+3), data, count);

	frame[count + headerSize] = (unsigned char)helperComputeLRC(frame, 0, count + headerSize - 1);
	lastFrame = frame;
	lastFrameLen = len;
	count_status = send(filp, &frame, C_TRANSMIT_NORMAL_SPI_OPERATION, len);

	if (count_status == 0) {
		NFC_ERR_MSG(KERN_ALERT "ERROR:Failed to send device\n");
		return -EPERM;
	}

	NFC_ERR_MSG(KERN_INFO  "sendFrame ret = %d - Exit\n", count_status);
	return count_status;
}
/**
     * Helper function to compute the LRC.
     *
     * @param data
     *            the data array
     * @param offset
     *            offset into the data array
     * @param length
     *            length value
     *
     */
unsigned char helperComputeLRC(unsigned char data[], int offset, int length)
{
	int LRC = 0;
	int i = 0;
	NFC_DBG_MSG(KERN_INFO  "helperComputeLRC - Enter\n");
	for (i = offset; i <= length; i++)
		LRC = LRC ^ data[i];
	NFC_DBG_MSG(KERN_INFO  "LRC Value is  %x \n", LRC);
	return (unsigned char)LRC;
}
/**
    * This function initializes the T = 1 module
    *
    */
void init()
{
	NFC_DBG_MSG(KERN_INFO  "init - Enter\n");
	apduBufferidx = 0;
	setAddress(0);
	setBitrate(100);

	NFC_DBG_MSG(KERN_INFO  "init - Exit\n");
}

void setAddress(short address)
{
	int stat = 0;
	NFC_DBG_MSG(KERN_INFO  "setAddress -Enter\n");
	stat = nativeSetAddress(address);

	if (stat != 0)
		NFC_ERR_MSG(KERN_INFO  "set address failed.\n");

	NFC_DBG_MSG(KERN_INFO  "setAddress -Exit\n");
}

void setBitrate(short bitrate)
{
	int stat = 0;
	NFC_DBG_MSG(KERN_INFO  "setBitrate -Enter\n");
	stat = nativeSetBitrate(bitrate);

	if (stat != 0)
		NFC_ERR_MSG(KERN_INFO  "set bitrate failed.\n");

	NFC_DBG_MSG(KERN_INFO  "setBitrate -Exit\n");
}

int nativeSetAddress(short address)
{
	NFC_DBG_MSG(KERN_INFO  "nativeSetAddress -Enter\n");
	return 0;
}

int nativeSetBitrate(short bitrate)
{
	NFC_DBG_MSG(KERN_INFO  "nativeSetBitrate -Enter\n");

	return 0;
}

#ifdef IRQ_ENABLE
static void p61_disable_irq(struct p61_dev *p61_dev)
{
	unsigned long flags;

	NFC_DBG_MSG("Entry : %s\n", __FUNCTION__);
	spin_lock_irqsave(&p61_dev->irq_enabled_lock, flags);
	if (p61_dev->irq_enabled) {
		disable_irq_nosync(p61_dev->spi->irq);
		p61_dev->irq_enabled = false;
	}
	spin_unlock_irqrestore(&p61_dev->irq_enabled_lock, flags);
	NFC_DBG_MSG("Exit : %s\n", __FUNCTION__);
}

static irqreturn_t p61_dev_irq_handler(int irq, void *dev_id)
{
	struct p61_dev *p61_dev = dev_id;

	NFC_DBG_MSG("Entry : %s\n", __FUNCTION__);
	p61_disable_irq(p61_dev);

	/* Wake up waiting readers */
	wake_up(&p61_dev->read_wq);

	NFC_DBG_MSG("Exit : %s\n", __FUNCTION__);
	return IRQ_HANDLED;
}
#endif

static inline void p61_set_data(struct spi_device *spi, void *data)
{
	dev_set_drvdata(&spi->dev, data);
}

static const struct file_operations p61_dev_fops = {
		.owner = THIS_MODULE,
		.llseek = no_llseek,
		.read  = p61_dev_receiveData,
		.write = p61_dev_sendData,
		.open  = p61_dev_open,
		.poll = p61_dev_poll,
		.unlocked_ioctl = p61_dev_ioctl,
		.compat_ioctl = p61_dev_ioctl,
};

static int p61_probe(struct spi_device *spi)
{
	int ret = 0;
	struct p61_dev *p61_dev = NULL;
	unsigned int irq_flags;
	printk("P61 with irq without log Entry : %s\n", __FUNCTION__);

	NFC_DBG_MSG("chip select : %d , bus number = %d \n", spi->chip_select, spi->master->bus_num);
	p61_dev = kzalloc(sizeof(*p61_dev), GFP_KERNEL);
	if (p61_dev == NULL) {
		NFC_ERR_MSG("failed to allocate memory for module data\n");
		ret = -ENOMEM;
		goto err_exit;
	}

	ret = gpio_request(P61_CS, "p61 cs");
	if (ret < 0) {
		NFC_ERR_MSG("p61 gpio cs request failed = 0x%x\n", P61_CS);
		goto fail_gpio;
	}
	NFC_ERR_MSG("gpio_request returned = 0x%x\n", ret);
	ret = gpio_direction_output(P61_CS, 1);
	if (ret < 0) {
		NFC_ERR_MSG("p61 gpio cs request failed gpio = 0x%x\n", P61_CS);
		goto fail_gpio;
	}
	NFC_ERR_MSG("gpio_direction_output returned = 0x%x\n", ret);

	ret = gpio_request(P61_RST, "p61 reset");
	if (ret < 0) {
		NFC_ERR_MSG("p61 gpio reset request failed = 0x%x\n", P61_RST);
		goto fail_gpio;
	}

	NFC_ERR_MSG("gpio_request returned = 0x%x\n", ret);
	ret = gpio_direction_output(P61_RST, 1);
	if (ret < 0) {
		NFC_ERR_MSG("p61 gpio rst request failed gpio = 0x%x\n", P61_RST);
		goto fail_gpio;
	}
	NFC_ERR_MSG("gpio_direction_output returned = 0x%x\n", ret);

#ifdef IRQ_ENABLE
	ret = gpio_request(P61_IRQ, "p61 irq");
	if (ret < 0) {
		NFC_ERR_MSG("p61 gpio request failed gpio = 0x%x\n", P61_IRQ);
		goto err_exit0;
	}
	ret = gpio_direction_input(P61_IRQ);
	if (ret < 0) {
		NFC_ERR_MSG("p61 gpio request failed gpio = 0x%x\n", P61_IRQ);
		goto err_exit0;
	}
#endif

	spi->bits_per_word = 8;
	spi->mode = SPI_MODE_0;
	spi->max_speed_hz = 7000000;

	ret = spi_setup(spi);
	if (ret < 0) {
		NFC_ERR_MSG("failed to do spi_setup()\n");
		goto err_exit0;
	}

	p61_dev->spi = spi;
	p61_dev->p61_device.minor = MISC_DYNAMIC_MINOR;
	p61_dev->p61_device.name = "p61";
	p61_dev->p61_device.fops = &p61_dev_fops;
	p61_dev->p61_device.parent = &spi->dev;

	p61_dev->ven_gpio = P61_RST;
	gpio_set_value(P61_RST, 1);
	msleep(20);
	printk("p61_dev->rst_gpio = %d\n ", P61_RST);
#ifdef IRQ_ENABLE
	p61_dev->irq_gpio = P61_IRQ;
#endif

	p61_set_data(spi, p61_dev);
	/* init mutex and queues */
	init_waitqueue_head(&p61_dev->read_wq);
	mutex_init(&p61_dev->read_mutex);

#ifdef IRQ_ENABLE
	spin_lock_init(&p61_dev->irq_enabled_lock);
#endif

	ret = misc_register(&p61_dev->p61_device);
	if (ret < 0) {
		NFC_ERR_MSG("misc_register failed! %d\n", ret);
		goto err_exit0;
	}
#ifdef IRQ_ENABLE
	p61_dev->spi->irq = gpio_to_irq(P61_IRQ);

	if (p61_dev->spi->irq < 0) {
		NFC_ERR_MSG("gpio_to_irq request failed gpio = 0x%x\n", P61_IRQ);
		goto err_exit0;
	}
	p61_dev->irq_enabled = true;
	irq_flags = IRQF_TRIGGER_RISING | IRQF_ONESHOT;

	ret = request_irq(p61_dev->spi->irq, p61_dev_irq_handler,
						irq_flags, p61_dev->p61_device.name, p61_dev);
	if (ret) {
		NFC_ERR_MSG("request_irq failed\n");
		goto err_exit0;
	}
	p61_disable_irq(p61_dev);
#endif

	NFC_DBG_MSG("Exit : %s\n", __FUNCTION__);
	return ret;



err_exit0:
	mutex_destroy(&p61_dev->read_mutex);
	if (p61_dev != NULL)
		kfree(p61_dev);
fail_gpio:
	gpio_free(P61_RST);
err_exit:
	return ret;
}
static inline void *p61_get_data(const struct spi_device *spi)
{
	return dev_get_drvdata(&spi->dev);
}

static int p61_remove(struct spi_device *spi)
{
	struct p61_dev *p61_dev = p61_get_data(spi);
	NFC_DBG_MSG("Entry : %s\n", __FUNCTION__);
	NFC_DBG_MSG(KERN_INFO  " %s ::  name : %s ", __FUNCTION__, p61_dev->p61_device.name);

	free_irq(p61_dev->spi->irq, p61_dev);
	mutex_destroy(&p61_dev->read_mutex);
	misc_deregister(&p61_dev->p61_device);
	gpio_free(P61_IRQ);
	gpio_free(P61_RST);
	if (p61_dev != NULL)
		kfree(p61_dev);
	NFC_DBG_MSG("Exit : %s\n", __FUNCTION__);
	return 0;
}
static struct of_device_id p61_of_match_table[] = {
	{ .compatible = "p61,nxp-nfc",},
	{ },
};
static struct spi_driver p61_driver = {
		.driver = {
				.name  = "p61",
				.bus   = &spi_bus_type,
				.owner = THIS_MODULE,
				.of_match_table = p61_of_match_table,
		},
		.probe   = p61_probe,
		.remove  = p61_remove,


};
static int __init p61_dev_init(void)
{
	NFC_DBG_MSG("Entry : %s\n", __FUNCTION__);
	return spi_register_driver(&p61_driver);
	NFC_DBG_MSG("Exit : %s\n", __FUNCTION__);
}
module_init(p61_dev_init);

static void __exit p61_dev_exit(void)
{
	int val = -1;
	NFC_DBG_MSG("Entry : %s\n", __FUNCTION__);
	val = p61_dev_close();
	if (0 > val)
		NFC_ERR_MSG("Falied free the memory : %s\n", __FUNCTION__);
	spi_unregister_driver(&p61_driver);
	NFC_DBG_MSG("Exit : %s\n", __FUNCTION__);
}
module_exit(p61_dev_exit);

MODULE_AUTHOR("MANJUNATHA VENKATESH");
MODULE_DESCRIPTION("NFC P61 SPI driver");
MODULE_LICENSE("GPL");
