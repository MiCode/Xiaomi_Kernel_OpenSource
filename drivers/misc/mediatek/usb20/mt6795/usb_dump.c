/*
 * f_acm.c -- USB CDC serial (ACM) function driver
 *
 * Copyright (C) 2003 Al Borchers (alborchers@steinerpoint.com)
 * Copyright (C) 2008 by David Brownell
 * Copyright (C) 2008 by Nokia Corporation
 * Copyright (C) 2009 by Samsung Electronics
 * Author: Michal Nazarewicz (m.nazarewicz@samsung.com)
 *
 * This software is distributed under the terms of the GNU General
 * Public License ("GPL") as published by the Free Software Foundation,
 * either version 2 of that License or (at your option) any later version.
 */

/* #define VERBOSE_DEBUG */

#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/device.h>
/* Move to android.c */
/*#include <linux/usb/android_composite.h>*/
#include <linux/musb/musb_core.h>
#include <linux/musb/musb_gadget.h>
#include <linux/musb/mtk_musb.h>
#include <linux/usb/cdc.h>
#include <linux/console.h>
#include <linux/tty.h>
#include <linux/vmalloc.h>
#include <linux/kthread.h>
#include <linux/delay.h>
//#include <linux/musb/usb_console.h>

#define CONSOLE_BUF (128*1024)
/*
extern u16 time_out_count ;
extern u8 power_reg1 ;
extern u8 devctl_reg1;
extern u8 power_reg2 ;
extern u8 devctl_reg2 ;
extern u8 power_reg3 ;
extern u8 devctl_reg3;
*/
extern char *usb_buf;
extern volatile u32 buf_len;
extern u8 data_ep_num;
extern char *usb_buf_readp;

extern struct usb_ep *acm_in_ep;
extern volatile bool cdc_set_contr;
//extern volatile bool usb_connected;
//extern volatile bool com_opend;	// Whether USB com port is opend
extern bool gadget_is_ready;
extern struct musb *mtk_musb;

//extern void __iomem	*USB_BASE;

//unsigned long USB_BASE_TEMP = (unsigned long)USB_BASE;

extern u32 send_data(u32 ep_num,u8 *pbuffer,u32 data_len);
extern void format_and_send_string(const char *s,unsigned int count);
extern struct tty_driver *usb_console_device(struct console *co, int *index);

#define MUSB_IECSR (0x0010)
#ifdef CONFIG_OF
#define usb_readb(x)		__raw_readb(((unsigned long)mtk_musb->mregs)+(x))
#define usb_readw(x)		__raw_readw(((unsigned long)mtk_musb->mregs)+(x))
#define usb_readl(x)		__raw_readl(((unsigned long)mtk_musb->mregs)+(x))
#define usb_writeb(x,y)		__raw_writeb(x,(((unsigned long)mtk_musb->mregs)+(y)))
#define usb_writew(x,y)		__raw_writew(x,(((unsigned long)mtk_musb->mregs)+(y)))
#define usb_writel(x,y)		__raw_writel(x,(((unsigned long)mtk_musb->mregs)+(y)))
#else
#define usb_readb(x)		__raw_readb(USB_BASE+(x))
#define usb_readw(x)		__raw_readw(USB_BASE+(x))
#define usb_readl(x)		__raw_readl(USB_BASE+(x))
#define usb_writeb(x,y)		__raw_writeb(x,(USB_BASE+(y)))
#define usb_writew(x,y)		__raw_writew(x,(USB_BASE+(y)))
#define usb_writel(x,y)		__raw_writel(x,(USB_BASE+(y)))
#endif

#define NOTIFY_EP 7
#define DATA_EP 8
#define BULK_P_SIZE 512

static u8 ep0_max_size = 64;
static u32 data_ep_max_size = 512;
u8 data_ep_num = DATA_EP;
u8 notify_ep_num = NOTIFY_EP;
char *usb_buf = NULL;
char *usb_buf_readp = NULL;

volatile u32 buf_len = 0;

struct usb_ep *acm_in_ep = NULL;

/*
 * This CDC ACM function support just wraps control functions and
 * notifications around the generic serial-over-usb code.
 *
 * Because CDC ACM is standardized by the USB-IF, many host operating
 * systems have drivers for it.  Accordingly, ACM is the preferred
 * interop solution for serial-port type connections.  The control
 * models are often not necessary, and in any case don't do much in
 * this bare-bones implementation.
 *
 * Note that even MS-Windows has some support for ACM.  However, that
 * support is somewhat broken because when you use ACM in a composite
 * device, having multiple interfaces confuses the poor OS.  It doesn't
 * seem to understand CDC Union descriptors.  The new "association"
 * descriptors (roughly equivalent to CDC Unions) may sometimes help.
 */
static bool com_opend = false;		// Whether USB com port is opend
//static bool usb_connected = true;	// Whether USB is connected
volatile bool cdc_set_contr = false;

//module_param(com_opend,bool,0644);
//module_param(usb_connected,bool,0644);

bool gadget_is_ready = false;
static char *usb_hs_config_desc = NULL;
static char *usb_fs_config_desc = NULL;
bool is_high_speed = false;
static struct usb_cdc_line_coding cdc_line =
{
	0x000E1000,
	0,
	0,
	0x8,
};

struct acm_ep_descs {
	struct usb_endpoint_descriptor	*in;
	struct usb_endpoint_descriptor	*out;
	struct usb_endpoint_descriptor	*notify;
};

/* notification endpoint uses smallish and infrequent fixed-size messages */

#define GS_LOG2_NOTIFY_INTERVAL	5	/* 1 << 5 == 32 msec */
#define GS_NOTIFY_MAXPACKET	10	/* notification + 2 bytes */

/*device descriptor*/

#define USBCOM_DEVICE_CLASS	0x02
#define USBCOM_DEVICE_SUBCLASS	0x00
#define USBCOM_DEVICE_PROTOCOL	0x00
#define COM_VENDOR_ID		0x0E8D
#define COM_PRO_ID		0x2000

static struct usb_device_descriptor device_desc =
{
	.bLength		= sizeof(device_desc),
	.bDescriptorType	= USB_DT_DEVICE,
	.bcdUSB			= __constant_cpu_to_le16(0x0200),
	.bDeviceClass		= USB_CLASS_MISC,	//USB_CLASS_COMM,
	.bDeviceSubClass	= 2,	//USBCOM_DEVICE_SUBCLASS,
	.bDeviceProtocol	= 1,	//USBCOM_DEVICE_PROTOCOL,
	.bMaxPacketSize0	= 64,
	.idVendor		= __constant_cpu_to_le16(COM_VENDOR_ID),
	.idProduct		= __constant_cpu_to_le16(COM_PRO_ID),
	.bcdDevice		= __constant_cpu_to_le16(0xffff),
	.iManufacturer		= 0,
	.iProduct		= 0,
	.iSerialNumber		= 0,
	.bNumConfigurations	= 1,
};

/*
static struct usb_cdc_notification cdc_notify = {
	.bmRequestType = 0xA1,
	.bNotificationType = 0x20,
	.wValue = 0,
	.wIndex = 0,
	.wLength = 2,
};
*/

static struct usb_config_descriptor config_desc = {

	sizeof (config_desc),
	USB_DT_CONFIG,
	(sizeof (struct usb_config_descriptor)) +
	(sizeof (struct usb_interface_descriptor) * 2) +
	(sizeof (struct usb_interface_assoc_descriptor)) +
	(sizeof (struct usb_cdc_header_desc)) +
	(sizeof (struct usb_cdc_call_mgmt_descriptor)) +
	(sizeof (struct usb_cdc_acm_descriptor)) +
	(sizeof (struct usb_endpoint_descriptor) * 3) +
	(sizeof(struct usb_cdc_union_desc)),
	2,
	1,
	0,
	0xc0,
	0xFA,
};


/* interface and class descriptors: */
static struct usb_interface_assoc_descriptor
	acm_iad_descriptor = {
	.bLength =		sizeof acm_iad_descriptor,
	.bDescriptorType =	USB_DT_INTERFACE_ASSOCIATION,
	.bFirstInterface =	0,
	/* .bFirstInterface =	DYNAMIC, */
	.bInterfaceCount =	2,	// control + data
	.bFunctionClass =	USB_CLASS_COMM,
	.bFunctionSubClass =	USB_CDC_SUBCLASS_ACM,
	.bFunctionProtocol =	USB_CDC_PROTO_NONE,
	/* .iFunction =		DYNAMIC */
};


static struct usb_interface_descriptor acm_control_interface_desc = {
	.bLength =		USB_DT_INTERFACE_SIZE,
	.bDescriptorType =	USB_DT_INTERFACE,
	.bInterfaceNumber =	0,
	/* .bInterfaceNumber = DYNAMIC */
	.bNumEndpoints =	1,
	.bInterfaceClass =	USB_CLASS_COMM,
	.bInterfaceSubClass =	USB_CDC_SUBCLASS_ACM,
	.bInterfaceProtocol =	USB_CDC_ACM_PROTO_AT_V25TER,
	/* .iInterface = DYNAMIC */
};

static struct usb_interface_descriptor acm_data_interface_desc = {
	.bLength =		USB_DT_INTERFACE_SIZE,
	.bDescriptorType =	USB_DT_INTERFACE,
	.bInterfaceNumber =	1,
	/* .bInterfaceNumber = DYNAMIC */
	.bNumEndpoints =	2,
	.bInterfaceClass =	USB_CLASS_CDC_DATA,
	.bInterfaceSubClass =	0,
	.bInterfaceProtocol =	0,
	/* .iInterface = DYNAMIC */
};

static struct usb_cdc_header_desc acm_header_desc = {
	.bLength =		sizeof(acm_header_desc),
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubType =	USB_CDC_HEADER_TYPE,
	.bcdCDC =		cpu_to_le16(0x0110),
};

static struct usb_cdc_call_mgmt_descriptor
	acm_call_mgmt_descriptor = {
	.bLength =		sizeof(acm_call_mgmt_descriptor),
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubType =	USB_CDC_CALL_MANAGEMENT_TYPE,
	.bmCapabilities =	0,
	.bDataInterface =	1,
};

static struct usb_cdc_acm_descriptor acm_descriptor = {
	.bLength =		sizeof(acm_descriptor),
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubType =	USB_CDC_ACM_TYPE,
	.bmCapabilities =	USB_CDC_CAP_LINE,
};

static struct usb_cdc_union_desc acm_union_desc = {
	.bLength =		sizeof(acm_union_desc),
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubType =	USB_CDC_UNION_TYPE,
	.bMasterInterface0 =	0,
	.bSlaveInterface0 =	1,
};


/* notify endpoint use EP7,data EP usb EP8*/
/* full speed support: */

static struct usb_endpoint_descriptor acm_fs_notify_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_IN|NOTIFY_EP,
	.bmAttributes =		USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize =	cpu_to_le16(GS_NOTIFY_MAXPACKET),
	.bInterval =		1 << GS_LOG2_NOTIFY_INTERVAL,
};

static struct usb_endpoint_descriptor acm_fs_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_IN|DATA_EP,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(64),
};

static struct usb_endpoint_descriptor acm_fs_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_OUT|DATA_EP,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(64),
};

static struct usb_descriptor_header *acm_fs_function[] = {
	(struct usb_descriptor_header *) &acm_iad_descriptor,
	(struct usb_descriptor_header *) &acm_control_interface_desc,
	(struct usb_descriptor_header *) &acm_header_desc,
	(struct usb_descriptor_header *) &acm_call_mgmt_descriptor,
	(struct usb_descriptor_header *) &acm_descriptor,
	(struct usb_descriptor_header *) &acm_union_desc,
	(struct usb_descriptor_header *) &acm_fs_notify_desc,
	(struct usb_descriptor_header *) &acm_data_interface_desc,
	(struct usb_descriptor_header *) &acm_fs_in_desc,
	(struct usb_descriptor_header *) &acm_fs_out_desc,
	NULL,
};

/* high speed support: */

static struct usb_endpoint_descriptor acm_hs_notify_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_IN|NOTIFY_EP,
	.bmAttributes =		USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize =	cpu_to_le16(GS_NOTIFY_MAXPACKET),
	.bInterval =		GS_LOG2_NOTIFY_INTERVAL+4,
};

static struct usb_endpoint_descriptor acm_hs_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_IN|DATA_EP,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(512),
};

static struct usb_endpoint_descriptor acm_hs_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_OUT|DATA_EP,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(512),
};

static struct usb_descriptor_header *acm_hs_function[] = {
	(struct usb_descriptor_header *) &acm_iad_descriptor,
	(struct usb_descriptor_header *) &acm_control_interface_desc,
	(struct usb_descriptor_header *) &acm_header_desc,
	(struct usb_descriptor_header *) &acm_call_mgmt_descriptor,
	(struct usb_descriptor_header *) &acm_descriptor,
	(struct usb_descriptor_header *) &acm_union_desc,
	(struct usb_descriptor_header *) &acm_hs_notify_desc,
	(struct usb_descriptor_header *) &acm_data_interface_desc,
	(struct usb_descriptor_header *) &acm_hs_in_desc,
	(struct usb_descriptor_header *) &acm_hs_out_desc,
	NULL,
};

/* string descriptors: */

//#define ACM_CTRL_IDX	0
//#define ACM_DATA_IDX	1
//#define ACM_IAD_IDX	2

/* static strings, in UTF-8 */
/*
static struct usb_string acm_string_defs[] = {
	[ACM_CTRL_IDX].s = "CDC Abstract Control Model (ACM)",
	[ACM_DATA_IDX].s = "CDC ACM Data",
	[ACM_IAD_IDX ].s = "CDC Serial",
	{  *//* ZEROES END LIST *//* },
};
*/

/*
static struct usb_gadget_strings acm_string_table = {
	.language =		0x0409,*/	/* en-us */
/*	.strings =		acm_string_defs,
};
*/

/*
static struct usb_gadget_strings *acm_strings[] = {
	&acm_string_table,
	NULL,
};
*/

/*
 * config usb
 */
static void usb_write_fifo(u16 len, const u8 *src, u8 ep_num)
{
	void __iomem *fifo = (void __iomem *)MUSB_FIFO_OFFSET(ep_num);
	if (likely((0x01 & (unsigned long) src) == 0)) {
		u16 index = 0;

		/* best case is 32bit-aligned source address */
		if ((0x02 & (unsigned long) src) == 0) {
			if (len >= 4) {
				writesl(USB_BASE_TEMP+fifo, src + index, len >> 2);
				index += len & ~0x03;
			}
			if (len & 0x02) {
				usb_writew(*(u16 *)&src[index],fifo + 0 );
				index += 2;
			}
		} else {
			if (len >= 2) {
				writesw(USB_BASE_TEMP+fifo, src + index, len >> 1);
				index += len & ~0x01;
			}
		}
		if (len & 0x01)
			usb_writeb(src[index],fifo + 0);
	} else  {
		/* byte aligned */
		writesb(USB_BASE_TEMP+fifo, src, len);
	}
}

/*
 * Unload an endpoint's FIFO
 */
static void usb_read_fifo(u16 len, u8 *dst,u8 ep_num)
{
	void __iomem *fifo = (void __iomem *)MUSB_FIFO_OFFSET(ep_num);

	/* we can't assume unaligned writes work */
	if (likely((0x01 & (unsigned long) dst) == 0)) {
		u16 index = 0;

		/* best case is 32bit-aligned destination address */
		if ((0x02 & (unsigned long) dst) == 0) {
			if (len >= 4) {
				readsl(USB_BASE_TEMP+fifo, dst, len >> 2);
				index = len & ~0x03;
			}
			if (len & 0x02) {
				*(u16 *)&dst[index] = usb_readw(fifo+0);
				index += 2;
			}
		} else {
			if (len >= 2) {
				readsw(USB_BASE_TEMP+fifo, dst, len >> 1);
				index = len & ~0x01;
			}
		}
		if (len & 0x01)
			dst[index] = usb_readb(fifo+0);
	} else {
		/* byte aligned */
		readsb(USB_BASE_TEMP+fifo, dst, len);
	}

}

/*
 * send data function,
 * will send all data in the buffer,return send data count
 */
static void copy_config_desc(char *buf,bool high_speed)
{
	struct usb_descriptor_header **acm_desc = NULL;
	if(buf == NULL)
		return;
	if(high_speed)
		acm_desc = acm_hs_function;
	else
		acm_desc = acm_fs_function;
	DBG(0,"start copy config descriptor\n");
	memcpy(buf,(char*)&config_desc,sizeof(config_desc));
	buf = buf + sizeof(config_desc);
	DBG(0,"start copy interface desciptore\n");
	while(*acm_desc != NULL)
	{
		DBG(0,"start copy interface desciptore %d\n",(*acm_desc)->bLength);
		memcpy(buf,(char*)(*acm_desc),(*acm_desc)->bLength);
		buf += (*acm_desc)->bLength;
		acm_desc++;
	}
}



/*
 * send/receive data function,
 * will send/receive all data in the buffer,return send/receive data count
 */
static void recv_data(u32 ep_num,u8 *pbuffer,u16 data_len)
{

	u16 rxcsr = 0;
	u16 len = 0;
	u16 count = 0;
	if(!pbuffer) {
		return;
	}
	usb_writeb(ep_num,MUSB_INDEX);
	while(count < data_len) {


		if(ep_num != 0)
		{
			//1:polling wait for RxPktRy
			rxcsr = usb_readw(MUSB_IECSR + MUSB_RXCSR);
			if((rxcsr & MUSB_RXCSR_RXPKTRDY)==0 ) {
				continue;
			}
			len = min(usb_readw(MUSB_IECSR+MUSB_RXCOUNT),data_len);
			count += len;
			usb_read_fifo(len,pbuffer,ep_num);
			rxcsr = usb_readw(MUSB_IECSR + MUSB_RXCSR);
			rxcsr &=(~MUSB_RXCSR_RXPKTRDY);
			usb_writew(rxcsr,MUSB_IECSR+MUSB_RXCSR);	// clear rx packet ready
			while(usb_readw(MUSB_IECSR+MUSB_RXCSR)&MUSB_RXCSR_RXPKTRDY)
				;	//wait for EP0 tx success

		} else {
			//1:polling wait for RxPktRy
			rxcsr = usb_readw(MUSB_IECSR + MUSB_CSR0);
			if((rxcsr & MUSB_CSR0_RXPKTRDY)==0 ) {
				continue;
			}
			len = min(usb_readw(MUSB_IECSR+MUSB_COUNT0),data_len);
			count += len;
			usb_read_fifo(len,pbuffer,ep_num);
			if(count == data_len)
				rxcsr |= MUSB_CSR0_P_DATAEND;	// this means packet is the last packet
			rxcsr |= MUSB_CSR0_P_SVDRXPKTRDY;	//set tx packet ready to start send data
			usb_writew(rxcsr,MUSB_IECSR + MUSB_CSR0);
			while(usb_readw(MUSB_IECSR+MUSB_CSR0)&MUSB_CSR0_RXPKTRDY)
				;	//wait for EP0 tx success

		}

	}

	return;
}
#define IS_DISCONNECT (usb_readb(MUSB_INTRUSB)&MUSB_INTR_DISCONNECT)
#define IS_SUSPEND (usb_readb(MUSB_INTRUSB)&MUSB_INTR_SUSPEND)
#define IS_VBUS_VALID (usb_readb(MUSB_DEVCTL)& MUSB_DEVCTL_VBUS)
/*
u16 time_out_count = 0;
u8 power_reg1 = 0;
u8 devctl_reg1 =0;
u8 power_reg2 = 0;
u8 devctl_reg2 =0;
u8 power_reg3 = 0;
u8 devctl_reg3 =0;
*/
static int time_out = 50;

/* module_param(time_out,int,0664); */
u32 send_data(u32 ep_num,u8 *pbuffer,u32 data_len)
{
	u16 csr;
	u32 count = 0;
	u32 write_bytes = 0;
	int i = 0;
//	int time_out = 50;

	bool send_zero = false;
	usb_writeb(ep_num,MUSB_INDEX);
	if(ep_num == 0)
	{
		csr = usb_readw(MUSB_IECSR + MUSB_CSR0);

		while(count < data_len)
		{
			if((data_len - count) < ep0_max_size)
			{
				write_bytes = data_len - count;
			} else {
				write_bytes = ep0_max_size;
			}
			usb_write_fifo(write_bytes,pbuffer + count,ep_num);
			count = count + write_bytes;
			if(count == data_len)
				csr |= MUSB_CSR0_P_DATAEND;	// this means packet is the last packet
			csr |= MUSB_CSR0_TXPKTRDY;	//set tx packet ready to start send data
			usb_writew(csr,MUSB_IECSR + MUSB_CSR0);
			while(usb_readw(MUSB_IECSR+MUSB_CSR0)&MUSB_CSR0_TXPKTRDY)
				;	//wait for EP0 tx success
		}
	} else {
		csr = usb_readw(MUSB_IECSR + MUSB_TXCSR);
		if((count%data_ep_max_size) == 0)
			send_zero = true;
		while(count < data_len)
		{
			i=0;
			while((usb_readw(MUSB_IECSR+MUSB_TXCSR)&MUSB_TXCSR_TXPKTRDY))
				;	//if some data has been in fifo before call printk, we must wait for tx complete
			if(i >= time_out ||IS_DISCONNECT || IS_SUSPEND)
			{
				printk ("count = 0x%x, IS_DISCONNECT=%d, IS_SUSPEND=%d \n", count,IS_DISCONNECT, IS_SUSPEND);
				//usb_writew(MUSB_RXCSR_FLUSHFIFO,MUSB_IECSR+MUSB_RXCSR);
				com_opend = false;
				cdc_set_contr = false;
				/*
				time_out_count ++;
				time_out_count = time_out_count*i;
				power_reg1 = usb_readb(MUSB_POWER);
				devctl_reg1 = usb_readb(MUSB_DEVCTL);
				*/
				//return count; //when usb is plug out or reenumeration during printk, data in tx fifo can't send successful
				//so time out will occur and we just return in this case.
			}

			if((data_len - count) < data_ep_max_size)
				write_bytes = data_len - count;
			else
				write_bytes = data_ep_max_size;

			usb_write_fifo(write_bytes,pbuffer + count,ep_num);

			count = count + write_bytes;
			csr |= MUSB_TXCSR_TXPKTRDY;	//set tx packet ready to start send data
			usb_writew(csr,MUSB_IECSR + MUSB_TXCSR);
			i = 0;
			while((usb_readw(MUSB_IECSR+MUSB_TXCSR)&MUSB_TXCSR_TXPKTRDY))
				;
			if(i >= time_out ||IS_DISCONNECT || IS_SUSPEND)
			{
				usb_writew(MUSB_RXCSR_FLUSHFIFO,MUSB_IECSR+MUSB_RXCSR);
				com_opend = false;
				cdc_set_contr = false;
				/*
				time_out_count ++;
					time_out_count = time_out_count*i;
					power_reg2 = usb_readb(MUSB_POWER);
					devctl_reg2 = usb_readb(MUSB_DEVCTL);
				*/
				return count;
			}

		}
#if 0
		if(send_zero)
		{
			i = 0;
			csr = usb_readw(MUSB_IECSR + MUSB_TXCSR);
			csr |= MUSB_TXCSR_TXPKTRDY;	//set tx packet ready to start send data
			usb_writew(csr,MUSB_IECSR + MUSB_TXCSR);
			while((usb_readw(MUSB_IECSR+MUSB_TXCSR)&MUSB_TXCSR_TXPKTRDY)) ;
			if(i >= time_out ||IS_DISCONNECT || IS_SUSPEND)
			{
				usb_writew(MUSB_RXCSR_FLUSHFIFO,MUSB_IECSR+MUSB_RXCSR);
				com_opend = false;
				cdc_set_contr = false;
				/*
				time_out_count ++;
				time_out_count = time_out_count*i;
				power_reg3 = usb_readb(MUSB_POWER);
				devctl_reg1 = usb_readb(MUSB_DEVCTL);
				*/
				return count;
			}
		}
#endif
		usb_writew(1<<data_ep_num,MUSB_INTRTX);	//clear the interrupt which assert by sent data

	}
	return count;
}

static void ep0_hand_shake(void)
{
	u32 csr = 0;
	usb_writeb(0,MUSB_INDEX);
	csr = usb_readw(MUSB_IECSR+MUSB_CSR0);
	csr &= (~MUSB_CSR0_P_SENTSTALL);
	csr |= (MUSB_CSR0_P_DATAEND| MUSB_CSR0_P_SVDRXPKTRDY);
	usb_writew(csr,MUSB_IECSR + MUSB_CSR0);
}

static bool send_descriptor(struct usb_ctrlrequest* pudr)
{
	u8 *data_tx = NULL;
	u16 wLength = 0;
	u16 wtype = pudr->wValue;
	bool fRet = TRUE;
	u16 csr0;
	DBG(0,"get descriptor %x\n",wtype);
	switch (wtype >> 8) {
	case USB_DT_DEVICE:
		data_tx = (u8 *)&device_desc;
		wLength = sizeof(device_desc);
		break;
	case USB_DT_CONFIG:
		if(is_high_speed)
			data_tx = usb_hs_config_desc;
		else
			data_tx = usb_fs_config_desc;
		wLength = min(pudr->wLength, config_desc.wTotalLength);
		break;
	case USB_DT_STRING:
		fRet = FALSE;
		break;
	default:
		fRet = FALSE;
		break;
	}

	usb_writeb(0,MUSB_INDEX);
	//Clear RxPktRdy
	csr0 = usb_readw(MUSB_IECSR+MUSB_CSR0);
	csr0 &= (~MUSB_CSR0_P_SENTSTALL);
	if(csr0 & MUSB_CSR0_RXPKTRDY)
	{
		csr0 |= MUSB_CSR0_P_SVDRXPKTRDY;
	}
	usb_writew(csr0,MUSB_IECSR+MUSB_CSR0);	// reponsible for setup stage, then start the data stage
	while(usb_readw(MUSB_IECSR+MUSB_CSR0)&MUSB_CSR0_RXPKTRDY)
		;
	if (fRet) {
		if(data_tx !=NULL) {

			send_data(0,data_tx,wLength);
		} else {
		}
	}
	return TRUE;
}
static void process_standard_request(struct usb_ctrlrequest* pudr)
{

	switch(pudr->bRequest)   {
	case USB_REQ_GET_STATUS:

		if (pudr->bRequestType == 0x80) {
			u8 device_status[2]={0,0};	//bus-power and remote wakeup is disabled
			send_data(0,device_status,2);
		} else if (pudr->bRequestType == 0xC0) {
			//printk("serial::USB_REQUEST_GET_STATUS_DEVICE 0xc0\r\n");
		} else if (pudr->bRequestType == 0x00) {
			//printk("serial::USB_REQUEST_GET_STATUS_DEVICE 0x00\r\n");
		}

		break;

	case USB_REQ_CLEAR_FEATURE:

		break;

	case USB_REQ_SET_FEATURE:
		break;

	case USB_REQ_SET_ADDRESS:

		ep0_hand_shake();
		usb_writeb(0,MUSB_INDEX);
		while((usb_readw(MUSB_INTRTX)&0x1)==0)
			;
		usb_writeb((u8)pudr->wValue,MUSB_FADDR);

		break;

	case USB_REQ_GET_DESCRIPTOR:

		send_descriptor(pudr);
		break;

	case USB_REQ_SET_DESCRIPTOR:

		break;

	case USB_REQ_GET_CONFIGURATION:

		break;

	case USB_REQ_SET_CONFIGURATION:
		ep0_hand_shake();
		break;

	case USB_REQ_GET_INTERFACE:

		break;

	case USB_REQ_SET_INTERFACE:

		break;
	case USB_TYPE_CLASS:
		break;
	default:
		;
	}
}
static void process_class_request(struct usb_ctrlrequest* request)
{


	switch (request->bRequest)
	{
	case USB_CDC_REQ_SET_LINE_CODING:
		recv_data(0,(u8 *)&cdc_line,sizeof (struct usb_cdc_line_coding));
		break;
	case USB_CDC_REQ_GET_LINE_CODING:
		send_data(0,(u8 *)&cdc_line,sizeof (struct usb_cdc_line_coding));
		if(cdc_set_contr == true)
			com_opend = true;
		break;
	case USB_CDC_REQ_SET_CONTROL_LINE_STATE:
		ep0_hand_shake();
		if(request->wValue & 1)
			cdc_set_contr = true;	// this command means com port is opend??????need to check is connect or disconnect command

		break;
	case USB_CDC_REQ_SEND_BREAK:	/* do nothing */
		ep0_hand_shake();
		break;
	default:
		return;
	}
	return;
}


static void process_setup_packet(struct usb_ctrlrequest* pudr)
{

	if (((pudr->bRequestType) & USB_TYPE_MASK) == USB_TYPE_STANDARD) {
		process_standard_request(pudr);
	} else if(((pudr->bRequestType) & USB_TYPE_MASK) == USB_TYPE_CLASS) {
		process_class_request(pudr);
	}
}

static void usb_reset(void)
{
	u16 swrst = 0;

	//setup EP FIFO
	usb_writeb(DATA_EP,MUSB_INDEX);

	usb_writew(BULK_P_SIZE,MUSB_IECSR + MUSB_RXMAXP);
	usb_writeb(6,MUSB_RXFIFOSZ);	//0x200 <--> 6
	usb_writew(0x8,MUSB_RXFIFOADD);	//Actual Size is 8 times as what be written, first 64 bytes are used by EP0

	usb_writew(BULK_P_SIZE,MUSB_IECSR + MUSB_TXMAXP);
	usb_writeb(6,MUSB_TXFIFOSZ);	//0x200 <--> 6
	usb_writew(72,MUSB_TXFIFOADD);

	//flush FIFO
	usb_writeb(0,MUSB_INDEX);
	usb_writew(MUSB_CSR0_FLUSHFIFO | MUSB_CSR0_P_WZC_BITS,MUSB_IECSR+MUSB_CSR0);	// flush ep0 fifo

	usb_writeb(NOTIFY_EP,MUSB_INDEX);

	usb_writew(64,MUSB_IECSR + MUSB_TXMAXP);
	usb_writeb(3,MUSB_TXFIFOSZ);	//0x200 <--> 6
	usb_writew(144,MUSB_TXFIFOADD);

	usb_writew(MUSB_TXCSR_FLUSHFIFO | MUSB_TXCSR_CLRDATATOG,MUSB_IECSR+MUSB_TXCSR);	// flush notify ep fifo

	usb_writeb(DATA_EP,MUSB_INDEX);
	usb_writew(MUSB_RXCSR_FLUSHFIFO | MUSB_RXCSR_CLRDATATOG,MUSB_IECSR+MUSB_RXCSR);	// flush data rx ep fifo
	usb_writew(MUSB_TXCSR_FLUSHFIFO | MUSB_TXCSR_CLRDATATOG,MUSB_IECSR+MUSB_TXCSR);	// flush data tx ep fifo


	swrst = usb_readw(MUSB_SWRST);
	swrst |= (MUSB_SWRST_DISUSBRESET | MUSB_SWRST_SWRST);
	usb_writew(swrst, MUSB_SWRST);

	if(usb_readb(MUSB_POWER) & MUSB_POWER_HSMODE)
	{
		is_high_speed = true;
		data_ep_max_size = 64;
	} else {
		is_high_speed = false;
		data_ep_max_size = 512;
	}

	//DBG(0,"usb is hight speed  %d %x\n",is_high_speed,usb_readb(MUSB_POWER));
	printk("usb is hight speed  %d %x\n",is_high_speed,usb_readb(MUSB_POWER));
}

static void config_usb_acm(void)
{

	u8 power = 0;
	int i = 0;
	u8 usb_comm_int = 0;
	u16 usb_tx_int = 0;

	u8 ep0_fifo;
	struct usb_ctrlrequest udr;
	usb_phy_recover();	// phy init
	//do some preparation
	usb_writew(0,MUSB_INTRRXE);
	usb_writew(0,MUSB_INTRTXE);
	usb_writeb(0,MUSB_INTRUSBE);	// disable all interrupt

	//now we can connect usb

	power = usb_readb(MUSB_POWER);
	power |= MUSB_POWER_SOFTCONN | MUSB_POWER_HSENAB | MUSB_POWER_ENSUSPEND;
	usb_writeb(power, MUSB_POWER);
	while(1) {
		i++;
		mdelay(5);
		usb_comm_int = usb_readb(MUSB_INTRUSB);
		usb_writeb(usb_comm_int,MUSB_INTRUSB);
		if(usb_comm_int & MUSB_INTR_RESET) {
			usb_reset();
			break;
		}
	}

	i = 0;
	while(1) {
		u16 csr0 = 0;
		usb_tx_int = usb_readw(MUSB_INTRTX);
		usb_writew(usb_tx_int,MUSB_INTRTX);
		if((usb_tx_int & 0x1) != 0)	// EP0 has something to do
		{
			usb_writeb(0,MUSB_INDEX);
			csr0 = usb_readw(MUSB_IECSR+MUSB_CSR0);
			if(csr0 & MUSB_CSR0_RXPKTRDY) {
				//we receive setup Packet,read it out
				ep0_fifo = usb_readb(MUSB_IECSR + MUSB_COUNT0);
				if(ep0_fifo != 8) {
					DBG(0,"serial::Error,setup Packet size!=8 =%d\r\n",ep0_fifo);
				}
				usb_read_fifo(8,(u8 *)&udr,0);
				process_setup_packet(&udr);
			}
		}
		if(com_opend== TRUE) {
			break;
		}
	}

}
/*-------------------------------------------------------------------------*/
void format_and_send_string(const char *s,unsigned int count)
{
	int real_send_count =0;
	int si =0;
	char* buf_start = (char *)s;
	char* buf_end = (char *)s + count;
	for(si=0; si<count; si++)
	{
		// traverse whole string and find the \n character; if found, send the string before this character and send a \r character
		//  then continue traverse to the end
		real_send_count++;
		if(buf_start[si] == '\n')
		{
			send_data(data_ep_num,buf_start,real_send_count);
			send_data(data_ep_num,"\r",1);
			buf_start = buf_start + si +1;
			real_send_count = 0;
		}
	}
	if(buf_start != NULL)	// if this string dosn't end with \n character, just send it.
		send_data(data_ep_num,buf_start,buf_end-buf_start);
}

/*
void usb_console_write(struct console *co, const char *s,unsigned int count) {
	//int i;

	unsigned long flags;
	// usb_connected = upmu_is_chr_det();

	// usb_connected = true;

	local_irq_save(flags);
	gadget_is_ready = true;
	if (usb_connected)
	{
		if (com_opend)
		{
			if(gadget_is_ready)
				data_ep_num = to_musb_ep(acm_in_ep)->current_epnum;
			else
				data_ep_num = DATA_EP;
			format_and_send_string(s,count);
		} else if (!gadget_is_ready) { // if gadget driver is not ready, we will enumration and wait com port open
			data_ep_num = DATA_EP;
			config_usb_acm();
			if(com_opend)
			format_and_send_string(s,count);
		} else {
			int i = 0;
			if(usb_buf == NULL) {
				usb_buf = kmalloc(CONSOLE_BUF,GFP_ATOMIC); // in interrupt content, can't sleep
				usb_buf_readp = usb_buf;
			}

			if(usb_buf == NULL) {
				local_irq_restore(flags);
				return;
			}

			for(i = 0;i < count; i++) {
				usb_buf[(buf_len)%CONSOLE_BUF] = s[i];
				buf_len++;
				if(s[i] == '\n')
				{
					usb_buf[(buf_len)%CONSOLE_BUF] = '\r';
					buf_len++;
				}
			}
			if(buf_len > CONSOLE_BUF) {
				usb_buf_readp = &usb_buf[(buf_len)%CONSOLE_BUF];
			}
		}
	}
	local_irq_restore(flags);
}
*/

/*---------------------------------------------------------------------------*/
static int usb_console_setup(struct console *co, char *options)
{
	usb_hs_config_desc = kmalloc(config_desc.wTotalLength,GFP_KERNEL);
	usb_fs_config_desc = kmalloc(config_desc.wTotalLength,GFP_KERNEL);
	if(usb_hs_config_desc == NULL || usb_fs_config_desc == NULL)
		return -1;
	copy_config_desc(usb_hs_config_desc,true);
	copy_config_desc(usb_fs_config_desc,false);
	return 0;	// don't do anyting in usb console setup
}


/*
static struct console usb_console =
{
	.name		= "ttyGS",
*/
/*don't configure UART4 as console*/
/*
	.write	= usb_console_write,
	.setup		= usb_console_setup,
	.device		= NULL,
	.flags		= CON_PRINTBUFFER,
	.index		= -1,
	.data		= NULL,
};
*/


int usb_init(void)
{
//	usb_console_setup (NULL, NULL) ;
	config_usb_acm ();
	return 0;
}
unsigned int Serial_Recv (unsigned char *pData, unsigned int size, unsigned int timeout)
{
	recv_data (data_ep_num, pData, size);
	return size;
}
unsigned int Serial_Send (unsigned char *pData, unsigned int size)
{

	return send_data (data_ep_num, pData, size);
}
void USBDumpInit (void)
{
	usb_console_setup (NULL, NULL);
}
int usb_get_max_send_size (void)
{
	return 64*1024;
}

EXPORT_SYMBOL (usb_init);
EXPORT_SYMBOL (Serial_Recv);
EXPORT_SYMBOL (Serial_Send);
EXPORT_SYMBOL (USBDumpInit);
EXPORT_SYMBOL (usb_get_max_send_size);
