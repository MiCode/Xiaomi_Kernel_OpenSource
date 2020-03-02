/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/completion.h>
#include <mach/eint.h>
#include <linux/gpio.h>
#include "musb_core.h"
#ifdef CONFIG_OF
#include <linux/of_address.h>
#endif
#define DRIVER_AUTHOR "Mediatek"
#define DRIVER_DESC "driver for OTG USB-IF test"
#define MUSB_OTG_CSR0 0x102
#define MUSB_OTG_COUNT0 0x108

#define TEST_DRIVER_NAME "mt_otg_test"

#define DX_DBG

#define TEST_IS_STOP    0xfff1
#define DEV_NOT_CONNECT 0xfff2
#define DEV_HNP_TIMEOUT 0xfff3
#define DEV_NOT_RESET   0xfff4

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_LICENSE("GPL");


/*for USB-IF OTG test*/
/*
 * when this func is called in EM, it will reset the USB hw.
 * and tester should not connet the uut to PC or connect a A-cable to it
 * macro for USB-IF for OTG driver
 */
#define OTG_CMD_E_ENABLE_VBUS       0x00
#define OTG_CMD_E_ENABLE_SRP        0x01
#define OTG_CMD_E_START_DET_SRP     0x02
#define OTG_CMD_E_START_DET_VBUS    0x03
#define OTG_CMD_P_A_UUT             0x04
#define OTG_CMD_P_B_UUT             0x05
#define HOST_CMD_TEST_SE0_NAK		0x6
#define HOST_CMD_TEST_J			0x7
#define HOST_CMD_TEST_K			0x8
#define HOST_CMD_TEST_PACKET		0x9
#define HOST_CMD_SUSPEND_RESUME		0xa
#define HOST_CMD_GET_DESCRIPTOR		0xb
#define HOST_CMD_SET_FEATURE		0xc
#define OTG_CMD_P_B_UUT_TD59		0xd
#define HOST_CMD_ENV_INIT				0xe
#define HOST_CMD_ENV_EXIT				0xf

#define OTG_MSG_DEV_NOT_SUPPORT     0x01
#define OTG_MSG_DEV_NOT_RESPONSE    0x02
#define OTG_MSG_HUB_NOT_SUPPORT     0x03

#define OTG_STOP_CMD    0x10
#define OTG_INIT_MSG    0x20

struct otg_message {
	spinlock_t lock;
	unsigned int msg;
};

static struct otg_message g_otg_message;
static atomic_t g_exec;

unsigned long usb_l1intm_store;
unsigned short usb_intrrxe_store;
unsigned short usb_intrtxe_store;
unsigned char usb_intrusbe_store;
unsigned long pericfg_base;
bool device_enumed;
bool set_hnp;
bool high_speed;
bool is_td_59;

struct completion stop_event;

void musb_otg_reset_usb(void)
{
	/* reset all of the USB IP, including PHY and MAC */
	unsigned int usb_reset;

	usb_reset = __raw_readl((void __iomem *)pericfg_base);
	usb_reset |= 1 << 29;
	__raw_writel(usb_reset, (void __iomem *)pericfg_base);
	mdelay(10);
	usb_reset &= ~(1 << 29);
	__raw_writel(usb_reset, (void __iomem *)pericfg_base);
	/* power on the USB */
	usb_phy_poweron();
	/* enable interrupt */
	musb_writel(mtk_musb->mregs, USB_L1INTM, 0x105);
	musb_writew(mtk_musb->mregs, MUSB_INTRTXE, 1);
	musb_writeb(mtk_musb->mregs, MUSB_INTRUSBE, 0xf7);
}

int musb_otg_env_init(void)
{
	u8 power;
	/* u8 intrusb; */
	/* step1: mask the PMU/PMIC EINT */
	mtk_musb->usb_if = true;
	/* workaround for PMIC charger detection */
	mtk_musb->is_host = true;
	/* mt65xx_eint_mask(EINT_CHR_DET_NUM); */

	pmic_chrdet_int_en(0);

	mt_usb_init_drvvbus();

	/* step5: make sure to power on the USB module */
	if (mtk_musb->power)
		mtk_musb->power = FALSE;

	musb_platform_enable(mtk_musb);
	/* step6: clear session bit */
	musb_writeb(mtk_musb->mregs, MUSB_DEVCTL, 0);
	/* step7: disable and enable usb interrupt */
	usb_l1intm_store = musb_readl(mtk_musb->mregs, USB_L1INTM);
	usb_intrrxe_store = musb_readw(mtk_musb->mregs, MUSB_INTRRXE);
	usb_intrtxe_store = musb_readw(mtk_musb->mregs, MUSB_INTRTXE);
	usb_intrusbe_store = musb_readb(mtk_musb->mregs, MUSB_INTRUSBE);

	musb_writel(mtk_musb->mregs, USB_L1INTM, 0);
	musb_writew(mtk_musb->mregs, MUSB_INTRRXE, 0);
	musb_writew(mtk_musb->mregs, MUSB_INTRTXE, 0);
	musb_writeb(mtk_musb->mregs, MUSB_INTRUSBE, 0);
	musb_writew(mtk_musb->mregs, MUSB_INTRRX, 0xffff);
	musb_writew(mtk_musb->mregs, MUSB_INTRTX, 0xffff);
	musb_writeb(mtk_musb->mregs, MUSB_INTRUSB, 0xff);
	free_irq(mtk_musb->nIrq, mtk_musb);
	musb_writel(mtk_musb->mregs, USB_L1INTM, 0x105);
	musb_writew(mtk_musb->mregs, MUSB_INTRTXE, 1);
	musb_writeb(mtk_musb->mregs, MUSB_INTRUSBE, 0xf7);
	/* setp8: set the index to 0 for ep0, maybe no need.
	 *  Designers said it is better not to use the index register.
	 */
	musb_writeb(mtk_musb->mregs, MUSB_INDEX, 0);
	/* setp9: init message */
	g_otg_message.msg = 0;
	spin_lock_init(&g_otg_message.lock);

	init_completion(&stop_event);
#ifdef DX_DBG
	power = musb_readb(mtk_musb->mregs, MUSB_POWER);
	DBG(0, "start the USB-IF test in EM,power=0x%x!\n", power);
#endif

	return 0;
}

int musb_otg_env_exit(void)
{
	DBG(0, "stop the USB-IF test in EM!\n");
	musb_writel(mtk_musb->mregs, USB_L1INTM, 0);
	musb_writew(mtk_musb->mregs, MUSB_INTRRXE, 0);
	musb_writew(mtk_musb->mregs, MUSB_INTRTXE, 0);
	musb_writeb(mtk_musb->mregs, MUSB_INTRUSBE, 0);
	musb_writew(mtk_musb->mregs, MUSB_INTRRX, 0xffff);
	musb_writew(mtk_musb->mregs, MUSB_INTRTX, 0xffff);
	musb_writeb(mtk_musb->mregs, MUSB_INTRUSB, 0xff);
	musb_writel(mtk_musb->mregs, USB_L1INTM, usb_l1intm_store);
	musb_writew(mtk_musb->mregs, MUSB_INTRRXE, usb_intrrxe_store);
	musb_writew(mtk_musb->mregs, MUSB_INTRTXE, usb_intrtxe_store);
	musb_writeb(mtk_musb->mregs, MUSB_INTRUSBE, usb_intrusbe_store);
	mtk_musb->usb_if = false;
	mtk_musb->is_host = false;
	pmic_chrdet_int_en(1);
	return 0;
}

void musb_otg_write_fifo(u16 len, u8 *buf)
{
	int i;

	DBG(0, "%s,len=%d\n", __func__, len);
	for (i = 0; i < len; i++)
		musb_writeb(mtk_musb->mregs, 0x20, *(buf + i));
}

void musb_otg_read_fifo(u16 len, u8 *buf)
{
	int i;

	DBG(0, "%s,len=%d\n", __func__, len);
	for (i = 0; i < len; i++)
		*(buf + i) = musb_readb(mtk_musb->mregs, 0x20);
}

unsigned int musb_polling_ep0_interrupt(void)
{
	unsigned short intrtx;

	DBG(0, "polling ep0 interrupt\n");
	do {
		intrtx = musb_readw(mtk_musb->mregs, MUSB_INTRTX);
		/* sync status */
		mb();
		musb_writew(mtk_musb->mregs, MUSB_INTRTX, intrtx);
		if (intrtx & 0x1) {	/* ep0 interrupt happen */
			DBG(0, "get ep0 interrupt,csr0=0x%x\n",
			    musb_readw(mtk_musb->mregs, MUSB_OTG_CSR0));
			break;
		}
		DBG(0, "polling ep0 interrupt,csr0=0x%x\n",
				musb_readb(mtk_musb->mregs, MUSB_OTG_CSR0));
		wait_for_completion_timeout(&stop_event, 1);
		if (atomic_read(&g_exec) == 0)
			return TEST_IS_STOP;
	} while (atomic_read(&g_exec) == 1);
	return 0;
}

void musb_h_setup(struct usb_ctrlrequest *setup)
{
	unsigned short csr0;

	DBG(0, "%s++\n", __func__);
	musb_otg_write_fifo(sizeof(struct usb_ctrlrequest), (u8 *) setup);
	csr0 = musb_readw(mtk_musb->mregs, MUSB_OTG_CSR0);
	DBG(0, "%s,csr0=0x%x\n", __func__, csr0);
	csr0 |= MUSB_CSR0_H_SETUPPKT | MUSB_CSR0_TXPKTRDY;
	musb_writew(mtk_musb->mregs, MUSB_OTG_CSR0, csr0);
	/* polling the Tx interrupt */
	if (musb_polling_ep0_interrupt())
		return;
	DBG(0, "%s--\n", __func__);
}

void musb_h_in_data(unsigned char *buf, u16 len)
{
	/* will receive all of the data in this transfer. */
	unsigned short csr0;
	u16 received = 0;
	bool bshort = false;

	DBG(0, "%s++\n", __func__);
	while ((received < len) && (!bshort)) {
		csr0 = musb_readw(mtk_musb->mregs, MUSB_OTG_CSR0);
		csr0 |= MUSB_CSR0_H_REQPKT;
		musb_writew(mtk_musb->mregs, MUSB_OTG_CSR0, csr0);
		if (musb_polling_ep0_interrupt())
			return;
		csr0 = musb_readw(mtk_musb->mregs, MUSB_OTG_CSR0);
		DBG(0, "csr0 = 0x%x!\n", csr0);
		if (csr0 & MUSB_CSR0_RXPKTRDY) {
			/* get the data from ep fifo */
			u8 count = musb_readb(mtk_musb->mregs, MUSB_OTG_COUNT0);

			if (count < 64)
				bshort = true;

			if (received + count > len) {
				DBG(0, "Data is too large\n");

			/* read FIFO until data end (maximum size of len) */
				musb_otg_read_fifo(len - received, buf);
			} else {
				musb_otg_read_fifo(count, buf + received);
			}

			received += count;
			csr0 &= ~MUSB_CSR0_RXPKTRDY;
			musb_writew(mtk_musb->mregs, MUSB_OTG_CSR0, csr0);
		} else
			DBG(0, "error, not receive the rxpktrdy interrupt!\n");
		DBG(0, "%s--\n", __func__);
	}
}

void musb_h_in_status(void)
{
	unsigned short csr0;

	DBG(0, "%s++\n", __func__);
	csr0 = musb_readw(mtk_musb->mregs, MUSB_OTG_CSR0);
	csr0 |= MUSB_CSR0_H_REQPKT | MUSB_CSR0_H_STATUSPKT;
	musb_writew(mtk_musb->mregs, MUSB_OTG_CSR0, csr0);
	if (musb_polling_ep0_interrupt())
		return;
	csr0 = musb_readw(mtk_musb->mregs, MUSB_OTG_CSR0);
	DBG(0, "csr0 = 0x%x!\n", csr0);

	if (csr0 & MUSB_CSR0_RXPKTRDY) {
		csr0 &= ~MUSB_CSR0_RXPKTRDY;
		/* whether this bit will be cleared auto,
		 * need to clear by sw??
		 */
		if (csr0 & MUSB_CSR0_H_STATUSPKT)
			csr0 &= ~MUSB_CSR0_H_STATUSPKT;
		musb_writew(mtk_musb->mregs, MUSB_OTG_CSR0, csr0);
	} else if (csr0 & MUSB_CSR0_H_RXSTALL) {
		DBG(0, "stall!\n");
		if (set_hnp) {
			DBG(0, "will pop up:DEV_NOT_RESPONSE!\n");
			g_otg_message.msg = OTG_MSG_DEV_NOT_RESPONSE;
			set_hnp = false;
			msleep(1000);
		}
	}
	DBG(0, "%s--\n", __func__);
}

void musb_h_out_status(void)
{
	unsigned short csr0;

	DBG(0, "%s++\n", __func__);
	csr0 = musb_readw(mtk_musb->mregs, MUSB_OTG_CSR0);
	csr0 |= MUSB_CSR0_H_STATUSPKT | MUSB_CSR0_TXPKTRDY;
	musb_writew(mtk_musb->mregs, MUSB_OTG_CSR0, csr0);
	if (musb_polling_ep0_interrupt())
		return;
#ifdef DX_DBG
	csr0 = musb_readw(mtk_musb->mregs, MUSB_OTG_CSR0);
	DBG(0, "csr0 = 0x%x!\n", csr0);
#endif
	DBG(0, "%s--\n", __func__);
}

void musb_d_reset(void)
{
	unsigned short swrst;

	swrst = musb_readw(mtk_musb->mregs, 0x74);
	swrst |= 0x2;
	musb_writew(mtk_musb->mregs, 0x74, swrst);
}

void musb_d_setup(struct usb_ctrlrequest *setup_packet, u16 len)
{
	musb_otg_read_fifo(len, (u8 *) setup_packet);
	DBG(0,
		"receive setup packet:0x%x 0x%x 0x%x 0x%x 0x%x\n",
		setup_packet->bRequest,
	    setup_packet->bRequestType,
	    setup_packet->wIndex,
	    setup_packet->wValue,
	    setup_packet->wLength);
}

void musb_d_out_data(struct usb_ctrlrequest *setup_packet)
{
	unsigned short csr0;

	static struct usb_device_descriptor device_descriptor = {
		0x12,
		0x01,
		0x0200,
		0x00,
		0x00,
		0x00,
		0x40,
		0x0951,
		0x1603,
		0x0200,
		0x01,
		0x02,
		0x03,
		0x01
	};
	static struct usb_config_descriptor configuration_descriptor = {
		0x09,
		0x02,
		0x0023,
		0x01,
		0x01,
		0x00,
		0x80,
		0x32
	};
	static struct usb_interface_descriptor interface_descriptor = {
		0x09,
		0x04,
		0x00,
		0x00,
		0x02,
		0x08,
		0x06,
		0x50,
		0x00
	};
	static struct usb_endpoint_descriptor endpoint_descriptor_in = {
		0x07,
		0x05,
		0x81,
		0x02,
		0x0200,
		0x00
	};
	static struct usb_endpoint_descriptor endpoint_descriptor_out = {
		0x07,
		0x05,
		0x02,
		0x02,
		0x0200,
		0x00
	};
	static struct usb_otg_descriptor usb_otg_descriptor = {
		0x03,
		0x09,
		0x03
	};

	if (setup_packet->wValue == 0x0100) {
		musb_otg_write_fifo(sizeof(struct usb_device_descriptor),
				    (u8 *) &device_descriptor);
	} else if (setup_packet->wValue == 0x0200) {
		if (setup_packet->wLength == 9) {
			musb_otg_write_fifo(
					sizeof(struct usb_config_descriptor),
					(u8 *) &configuration_descriptor);
		} else {
			musb_otg_write_fifo(
					sizeof(struct usb_config_descriptor),
					(u8 *) &configuration_descriptor);
			musb_otg_write_fifo(
					sizeof(struct usb_interface_descriptor),
					(u8 *) &interface_descriptor);
			musb_otg_write_fifo(7, (u8 *) &endpoint_descriptor_in);
			musb_otg_write_fifo(7, (u8 *) &endpoint_descriptor_out);
			musb_otg_write_fifo(sizeof(struct usb_otg_descriptor),
					(u8 *) &usb_otg_descriptor);
		}
	}
	csr0 = musb_readw(mtk_musb->mregs, MUSB_OTG_CSR0);
	csr0 |= MUSB_CSR0_TXPKTRDY | MUSB_CSR0_P_DATAEND;
	musb_writew(mtk_musb->mregs, MUSB_OTG_CSR0, csr0);
	if (musb_polling_ep0_interrupt())
		return;
}

unsigned int musb_polling_bus_interrupt(unsigned int intr)
{
	unsigned char intrusb;
	unsigned long timeout;

	if (intr == MUSB_INTR_CONNECT)
		timeout = jiffies + 15 * HZ;
	if (intr == (MUSB_INTR_CONNECT | MUSB_INTR_RESUME))
		timeout = jiffies + 1;
	if (intr == MUSB_INTR_RESET)
		timeout = jiffies + 2 * HZ;

	do {
		intrusb = musb_readb(mtk_musb->mregs, MUSB_INTRUSB);
		/* sync status */
		mb();
		musb_writeb(mtk_musb->mregs, MUSB_INTRUSB, intrusb);
		if (intrusb & intr) {
			DBG(0,
				"interrupt happen, intrusb=0x%x, intr=0x%x\n",
				intrusb, intr);
			break;
		}

		/* check the timeout */
		if ((intr == MUSB_INTR_CONNECT) &&
						time_after(jiffies, timeout)) {
			DBG(0, "time out for MUSB_INTR_CONNECT\n");
			return DEV_NOT_CONNECT;
		}
		if ((intr == (MUSB_INTR_CONNECT | MUSB_INTR_RESUME))
				&& time_after(jiffies, timeout)) {
			DBG(0,
				"time out for MUSB_INTR_CONNECT|MUSB_INTR_RESUME\n");
			return DEV_HNP_TIMEOUT;
		}
		if ((intr == MUSB_INTR_RESET) && time_after(jiffies, timeout)) {
			DBG(0, "time out for MUSB_INTR_RESET\n");
			return DEV_NOT_RESET;
		}
		/* delay for the interrupt */
		if (intr != MUSB_INTR_RESET) {
			wait_for_completion_timeout(&stop_event, 1);
			if (atomic_read(&g_exec) == 0)
				break;
		}
	} while (atomic_read(&g_exec) == 1);
	if (atomic_read(&g_exec) == 0) {
		DBG(0, "TEST_IS_STOP\n");
		return TEST_IS_STOP;
	}
	if (intrusb & MUSB_INTR_RESUME) {	/* for TD.4.8, remote wakeup */
		DBG(0, "MUSB_INTR_RESUME\n");
		return MUSB_INTR_RESUME;
	} else {
		return intrusb;
	}
}

void musb_h_suspend(void)
{
	unsigned char power;
	/* before suspend, should to send SOF for a while (USB-IF plan need) */
	/* mdelay(100); */
	power = musb_readb(mtk_musb->mregs, MUSB_POWER);
	DBG(0, "before suspend,power=0x%x\n", power);
	if (high_speed)
		power = 0x63;
	else
		power = 0x43;
	musb_writeb(mtk_musb->mregs, MUSB_POWER, power);
}

void musb_h_remote_wakeup(void)
{
	unsigned char power;

	msleep(25);
	power = musb_readb(mtk_musb->mregs, MUSB_POWER);
	power &= ~MUSB_POWER_RESUME;
	musb_writeb(mtk_musb->mregs, MUSB_POWER, power);
}

bool musb_h_reset(void)
{
	unsigned char power;

	power = musb_readb(mtk_musb->mregs, MUSB_POWER);
	power |= MUSB_POWER_RESET | MUSB_POWER_HSENAB;
	musb_writeb(mtk_musb->mregs, MUSB_POWER, power);
	msleep(60);
	power &= ~MUSB_POWER_RESET;
	musb_writeb(mtk_musb->mregs, MUSB_POWER, power);
	power = musb_readb(mtk_musb->mregs, MUSB_POWER);
	if (power & MUSB_POWER_HSMODE) {
		DBG(0, "the device is a hs device!\n");
		high_speed = true;
		return true;
	}
	DBG(0, "the device is a fs device!\n");
	high_speed = false;
	return false;
}

void musb_d_soft_connect(bool connect)
{
	unsigned char power;

	power = musb_readb(mtk_musb->mregs, MUSB_POWER);
	if (connect)
		power |= MUSB_POWER_SOFTCONN;
	else
		power &= ~MUSB_POWER_SOFTCONN;
	musb_writeb(mtk_musb->mregs, MUSB_POWER, power);
}

void musb_otg_set_session(bool set)
{
	unsigned char devctl = musb_readb(mtk_musb->mregs, MUSB_DEVCTL);

	if (set)
		devctl |= MUSB_DEVCTL_SESSION;
	else
		devctl &= ~MUSB_DEVCTL_SESSION;
	musb_writeb(mtk_musb->mregs, MUSB_DEVCTL, devctl);
}

void musb_h_enumerate(void)
{
	struct usb_ctrlrequest setup_packet;
	struct usb_device_descriptor device_descriptor;
	struct usb_config_descriptor configuration_descriptor;
	struct usb_otg_descriptor *otg_descriptor;
	unsigned char descriptor[65535];

	/* set address */
	musb_writew(mtk_musb->mregs, MUSB_TXFUNCADDR, 0);
	setup_packet.bRequestType = USB_DIR_OUT |
						USB_TYPE_STANDARD |
						USB_RECIP_DEVICE;
	setup_packet.bRequest = USB_REQ_SET_ADDRESS;
	setup_packet.wIndex = 0;
	setup_packet.wValue = 1;
	setup_packet.wLength = 0;
	musb_h_setup(&setup_packet);
	musb_h_in_status();
	musb_writew(mtk_musb->mregs, MUSB_TXFUNCADDR, 1);
	DBG(0, "set address OK!\n");
	/* get device descriptor */
	setup_packet.bRequestType = USB_DIR_IN |
						USB_TYPE_STANDARD |
						USB_RECIP_DEVICE;
	setup_packet.bRequest = USB_REQ_GET_DESCRIPTOR;
	setup_packet.wIndex = 0;
	setup_packet.wValue = 0x0100;
	setup_packet.wLength = 0x40;
	musb_h_setup(&setup_packet);
	musb_h_in_data((char *)&device_descriptor,
		sizeof(struct usb_device_descriptor));
	musb_h_out_status();

	if (device_descriptor.idProduct == 0x1234) {
		pr_debug("device pid not match!\n");
		g_otg_message.msg = OTG_MSG_DEV_NOT_SUPPORT;
		/* msleep(1000); */
	}

	DBG(0,
		"get device descriptor OK!device class=0x%x PID=0x%x VID=0x%x\n",
	    device_descriptor.bDeviceClass, device_descriptor.idProduct,
	    device_descriptor.idVendor);
	DBG(0,
		"get device descriptor OK!DescriptorType=0x%x DeviceSubClass=0x%x\n",
	    device_descriptor.bDescriptorType,
	    device_descriptor.bDeviceSubClass);
	/* get configuration descriptor */
	setup_packet.bRequestType = USB_DIR_IN
							| USB_TYPE_STANDARD
							| USB_RECIP_DEVICE;
	setup_packet.bRequest = USB_REQ_GET_DESCRIPTOR;
	setup_packet.wIndex = 0;
	setup_packet.wValue = 0x0200;
	setup_packet.wLength = 0x9;
	musb_h_setup(&setup_packet);
	musb_h_in_data((char *)&configuration_descriptor,
				sizeof(struct usb_config_descriptor));
	musb_h_out_status();
	DBG(0, "get configuration descriptor OK!\n");
	/* get all configuration descriptor */
	setup_packet.bRequestType = USB_DIR_IN
							| USB_TYPE_STANDARD
							| USB_RECIP_DEVICE;
	setup_packet.bRequest = USB_REQ_GET_DESCRIPTOR;
	setup_packet.wIndex = 0;
	setup_packet.wValue = 0x0200;
	setup_packet.wLength = configuration_descriptor.wTotalLength;
	musb_h_setup(&setup_packet);

	/*
	 * According to USB specification,
	 * the maximum length of wTotalLength is 65535 bytes
	 */
	if (configuration_descriptor.wTotalLength <= sizeof(descriptor))
		musb_h_in_data(descriptor,
				configuration_descriptor.wTotalLength);
	musb_h_out_status();
	DBG(0, "get all configuration descriptor OK!\n");
	/* get otg descriptor */
	otg_descriptor =
	    (struct usb_otg_descriptor *)
		(descriptor + configuration_descriptor.wTotalLength - 3);
	DBG(0, "otg descriptor::bLegth=%d,bDescriptorTye=%d,bmAttr=%d\n",
		otg_descriptor->bLength,
	    otg_descriptor->bDescriptorType, otg_descriptor->bmAttributes);
	if (otg_descriptor->bLength == 3 &&
		otg_descriptor->bDescriptorType == 9) {

		DBG(0, "get an otg descriptor!\n");
	} else {
		DBG(0, "not an otg device, will pop Unsupported Device\n");
		g_otg_message.msg = OTG_MSG_DEV_NOT_SUPPORT;
		msleep(1000);
	}
	/* set hnp, need before set_configuration */
	set_hnp = true;
	setup_packet.bRequestType = USB_DIR_OUT |
							USB_TYPE_STANDARD |
							USB_RECIP_DEVICE;
	setup_packet.bRequest = USB_REQ_SET_FEATURE;
	setup_packet.wIndex = 0;
	setup_packet.wValue = 0x3;	/* b_hnp_enable */
	setup_packet.wLength = 0;
	musb_h_setup(&setup_packet);
	musb_h_in_status();
	DBG(0, "set hnp OK!\n");
	/* set configuration */
	setup_packet.bRequestType = USB_DIR_OUT |
							USB_TYPE_STANDARD |
							USB_RECIP_DEVICE;
	setup_packet.bRequest = USB_REQ_SET_CONFIGURATION;
	setup_packet.wIndex = 0;
	setup_packet.wValue = configuration_descriptor.iConfiguration;
	setup_packet.wLength = 0;
	musb_h_setup(&setup_packet);
	musb_h_in_status();
	DBG(0, "set configuration OK!\n");
}

void musb_d_enumerated(void)
{
	unsigned char devctl;
	unsigned short csr0 = musb_readw(mtk_musb->mregs, MUSB_OTG_CSR0);

	DBG(0, "csr0=0x%x\n", csr0);
	if (csr0 & MUSB_CSR0_P_SETUPEND) {
		DBG(0, "SETUPEND\n");
		csr0 |= MUSB_CSR0_P_SVDSETUPEND;
		musb_writeb(mtk_musb->mregs, MUSB_OTG_CSR0, csr0);
		csr0 &= ~MUSB_CSR0_P_SVDSETUPEND;
	}
	if (csr0 & MUSB_CSR0_RXPKTRDY) {
		u8 count0;

		count0 = musb_readb(mtk_musb->mregs, MUSB_OTG_COUNT0);
		if (count0 == 8) {
			struct usb_ctrlrequest setup_packet;
			/* get the setup packet */
			musb_d_setup(&setup_packet, count0);

			if (setup_packet.bRequest ==
						USB_REQ_SET_ADDRESS) {
				device_enumed = false;
				csr0 |= MUSB_CSR0_P_SVDRXPKTRDY
						| MUSB_CSR0_P_DATAEND;
				/* clear the RXPKTRDY */
				musb_writew(mtk_musb->mregs,
						MUSB_OTG_CSR0, csr0);
				if (musb_polling_ep0_interrupt()) {
					DBG(0,
					    "B-UUT:when set address, do not detect ep0 interrupt\n");
					return;
				}
				musb_writeb(mtk_musb->mregs,
						MUSB_FADDR,
						(u8) setup_packet.wValue);
			} else if (setup_packet.bRequest ==
						USB_REQ_GET_DESCRIPTOR) {
				csr0 |= MUSB_CSR0_P_SVDRXPKTRDY;
				/* clear the RXPKTRDY */
				musb_writew(mtk_musb->mregs,
							MUSB_OTG_CSR0, csr0);
				/* device --> host */
				musb_d_out_data(&setup_packet);
			} else if (setup_packet.bRequest ==
						USB_REQ_SET_CONFIGURATION) {
				csr0 |= MUSB_CSR0_P_SVDRXPKTRDY
						| MUSB_CSR0_P_DATAEND;
				/* clear the RXPKTRDY */
				musb_writew(mtk_musb->mregs,
							MUSB_OTG_CSR0, csr0);
				if (musb_polling_ep0_interrupt())
					return;
				device_enumed = true;
				/* will set host_req for B-device */
				devctl = musb_readb(mtk_musb->mregs,
							MUSB_DEVCTL);
				if (devctl & MUSB_DEVCTL_BDEVICE) {
					devctl |= MUSB_DEVCTL_HR;
					musb_writeb(mtk_musb->mregs,
							MUSB_DEVCTL, devctl);
				}
			} else if (setup_packet.bRequest ==
							USB_REQ_SET_FEATURE) {
				csr0 |= MUSB_CSR0_P_SVDRXPKTRDY
						| MUSB_CSR0_P_DATAEND;
				/* clear the RXPKTRDY */
				musb_writew(mtk_musb->mregs,
						MUSB_OTG_CSR0, csr0);
				if (musb_polling_ep0_interrupt())
					return;
			}
		}
	}
}

void musb_otg_test_return(void)
{
}

static int musb_host_test_mode(unsigned char cmd);

void otg_cmd_a_uut(void)
{
	unsigned long timeout;
	unsigned char devctl;
	bool timeout_flag = false;
	unsigned int ret;
	unsigned char power;
	unsigned short csr0;
	unsigned char intrusb;
	unsigned short intrtx;

	/* polling the session req from B-OPT and start a new session */
	device_enumed = false;
TD_4_6:
	musb_otg_reset_usb();
	DBG(0, "A-UUT reset success\n");
	timeout = jiffies + 5 * HZ;
	musb_writeb(mtk_musb->mregs, MUSB_DEVCTL, 0);
	devctl = musb_readb(mtk_musb->mregs, MUSB_DEVCTL);
	while ((atomic_read(&g_exec) == 1) && (devctl & 0x18)) {
		DBG(0, "musb::not below session end!\n");
		msleep(100);
		if (time_after(jiffies, timeout)) {
			timeout_flag = true;
			return TEST_IS_STOP;
		}
		devctl = musb_readb(mtk_musb->mregs, MUSB_DEVCTL);
	}
	if (timeout_flag) {
		timeout_flag = false;
		musb_otg_reset_usb();
		BG(0,
			"timeout for below session end, after reset usb, devctl=0x%x\n",
		    musb_readb(mtk_musb->mregs, MUSB_DEVCTL));
	}
	BG(0, "polling session request,begin\n");
	ret = musb_polling_bus_interrupt(MUSB_INTR_SESSREQ);
	pBG(0, "polling session request,done,ret=0x%x\n", ret);
	if (ret == TEST_IS_STOP)
		return TEST_IS_STOP;
	/* session is set and VBUS will be out. */
	musb_otg_set_session(true);
#if 1
	power = musb_readb(mtk_musb->mregs, MUSB_POWER);
	power &= ~MUSB_POWER_SOFTCONN;
	musb_writeb(mtk_musb->mregs, MUSB_POWER, power);
#endif
	/* polling the connect interrupt from B-OPT */
	DBG(0, "polling connect interrupt,begin\n");
	ret = musb_polling_bus_interrupt(MUSB_INTR_CONNECT);
	DBG(0, "polling connect interrupt,done,ret=0x%x\n", ret);
	if (ret == TEST_IS_STOP)
		return TEST_IS_STOP;
	if (ret == DEV_NOT_CONNECT) {
		DBG(0, "device is not connected in 15s\n");
		g_otg_message.msg = OTG_MSG_DEV_NOT_RESPONSE;
		return TEST_IS_STOP;
	}
	DBG(0, "musb::connect interrupt is detected!\n");
	/* the test is fail because the reset starts less than100 ms
	 * from the B-OPT connect. the IF test needs
	 */
	msleep(100);
	/* reset the bus,check whether it is a hs device */
	musb_h_reset();		/* should last for more than 50ms, TD.4.2 */
	musb_h_enumerate();
	/* suspend the bus */
	csr0 = musb_readw(mtk_musb->mregs, MUSB_OTG_CSR0);
	DBG(0, "after enum B-OPT,csr0=0x%x\n", csr0);
	musb_h_suspend();

	/* polling the disconnect interrupt from B-OPT,
	 * and remote wakeup(TD.4.8)
	 */
	DBG(0, "polling disconnect or remote wakeup,begin\n");
	ret = musb_polling_bus_interrupt(MUSB_INTR_DISCONNECT
							| MUSB_INTR_RESUME);
	DBG(0, "polling disconnect or remote wakeup,done,ret=0x%x\n", ret);
	if (ret == TEST_IS_STOP)
		return TEST_IS_STOP;
	if (ret == MUSB_INTR_RESUME) {
		/* for TD4.8 */
		musb_h_remote_wakeup();
		/* maybe need to access the B-OPT, get device descriptor */
		if (atomic_read(&g_exec) == 1)
			wait_for_completion(&stop_event);
		return TEST_IS_STOP;
	}
	/* polling the reset interrupt from B-OPT */
	if (!(ret & MUSB_INTR_RESET)) {
		DBG(0, "polling reset for B-OPT,begin\n");
		ret = musb_polling_bus_interrupt(MUSB_INTR_RESET);
		DBG(0, "polling reset for B-OPT,done,ret=0x%x\n", ret);
		if (ret == TEST_IS_STOP)
			return TEST_IS_STOP;
		if (ret == DEV_NOT_RESET) {
			if (atomic_read(&g_exec) == 1)
				wait_for_completion(&stop_event);
			return TEST_IS_STOP;
		}
	}

	DBG(0, "after receive reset,devctl=0x%x,csr0=0x%x\n",
	    musb_readb(mtk_musb->mregs, MUSB_DEVCTL),
		musb_readw(mtk_musb->mregs, MUSB_OTG_CSR0));

	/* enumerate and polling the suspend interrupt form B-OPT */

	do {
		intrtx = musb_readw(mtk_musb->mregs, MUSB_INTRTX);
		/* sync status */
		mb();
		musb_writew(mtk_musb->mregs, MUSB_INTRTX, intrtx);
		intrusb = musb_readb(mtk_musb->mregs, MUSB_INTRUSB);
		/* sync status */
		mb();
		musb_writeb(mtk_musb->mregs, MUSB_INTRUSB, intrusb);
		if (intrtx || (intrusb & MUSB_INTR_SUSPEND)) {
			if (intrtx) {
				if (intrtx & 0x1)
					musb_d_enumerated();
			}
			if (intrusb) {
		/* maybe receive disconnect interrupt when the session is end */
				if (intrusb & MUSB_INTR_SUSPEND) {
					if (device_enumed) {
						/* return form the while loop */
						break;
					}
					/* TD.4.6 */
					musb_d_soft_connect(false);
					goto TD_4_6;
				}
			}
		} else
			wait_for_completion_timeout(&stop_event, 1);
		/* the enum will be repeated for 5 times */
	} while (atomic_read(&g_exec) == 1);
	if (atomic_read(&g_exec) == 0) {
		/* return form the switch-case */
		return TEST_IS_STOP;
	}
	DBG(0, "polling connect form B-OPT,begin\n");
	/* B-OPT will connect again 100ms after A disconnect */
	ret = musb_polling_bus_interrupt(MUSB_INTR_CONNECT);
	DBG(0, "polling connect form B-OPT,done,ret=0x%x\n", ret);
	if (ret == TEST_IS_STOP)
		return TEST_IS_STOP;
	musb_h_reset();		/* should reset bus again, TD.4.7 */
	wait_for_completion(&stop_event);
}

void otg_cmd_b_uut(void)
{
	unsigned long timeout;
	unsigned char devctl;
	bool timeout_flag = false;
	unsigned int ret;
	unsigned char power;
	unsigned char intrusb;
	unsigned short intrtx;

	musb_otg_reset_usb();
	/* The B-UUT issues an SRP to start a session with the A-OPT */
	musb_otg_set_session(true);
	/* 100ms after VBUS begins to decay the A-OPT powers VBUS */
	timeout = jiffies + 5 * HZ;
	devctl = musb_readb(mtk_musb->mregs, MUSB_DEVCTL);

	while (((devctl & MUSB_DEVCTL_VBUS) >> MUSB_DEVCTL_VBUS_SHIFT) < 0x3) {
		if (time_after(jiffies, timeout)) {
			timeout_flag = true;
			break;
		}
		msleep(100);
		devctl = musb_readb(mtk_musb->mregs, MUSB_DEVCTL);
	}
	if (timeout_flag) {
		DBG(0, "B-UUT set vbus timeout\n");
		g_otg_message.msg = OTG_MSG_DEV_NOT_RESPONSE;
		timeout_flag = false;
		return TEST_IS_STOP;
	}

	/* After detecting the VBUS, B-UUT should connect to the A_OPT */
	power = musb_readb(mtk_musb->mregs, MUSB_POWER);
	power |= MUSB_POWER_HSENAB;
	musb_writeb(mtk_musb->mregs, MUSB_POWER, power);
/* TD5_5: */
	musb_d_soft_connect(true);

	device_enumed = false;
	/* polling the reset single form the A-OPT */
	DBG(0, "polling reset form A-OPT,begin\n");
	ret = musb_polling_bus_interrupt(MUSB_INTR_RESET);
	DBG(0, "polling reset form A-OPT,done,ret=0x%x\n", ret);
	if (ret == TEST_IS_STOP)
		return TEST_IS_STOP;
	power = musb_readb(mtk_musb->mregs, MUSB_POWER);
	if (power & MUSB_POWER_HSMODE)
		high_speed = true;
	else
		high_speed = false;
	/* The A-OPT enumerates the B-UUT */
TD6_13:
	do {
		intrtx = musb_readw(mtk_musb->mregs, MUSB_INTRTX);
		/* sync status */
		mb();
		musb_writew(mtk_musb->mregs, MUSB_INTRTX, intrtx);
		intrusb = musb_readb(mtk_musb->mregs, MUSB_INTRUSB);
		/* sync status */
		mb();
		musb_writeb(mtk_musb->mregs, MUSB_INTRUSB, intrusb);
		if (intrtx || (intrusb & 0xf7)) {
			if (intrtx) {
				/* DBG(0,"B-enum,intrtx=0x%x\n",intrtx); */
				if (intrtx & 0x1)
					DBG(0, "ep0 interrupt\n");
				musb_d_enumerated();
			}
			if (intrusb) {
				if (intrusb & 0xf7)
					DBG(0,
						"B-enum,intrusb=0x%x,power=0x%x\n",
						intrusb,
						musb_readb(mtk_musb->mregs,
						MUSB_POWER));
				if ((device_enumed) &&
						(intrusb & MUSB_INTR_SUSPEND)) {
					DBG(0,
					    "suspend interrupt is received,power=0x%x,devctl=0x%x\n",
				    musb_readb(mtk_musb->mregs, MUSB_POWER),
				    musb_readb(mtk_musb->mregs, MUSB_DEVCTL));
					break;
				}
			}
		} else {
			DBG(0,
				"power=0x%x,devctl=0x%x,intrtx=0x%x,intrusb=0x%x\n",
			    musb_readb(mtk_musb->mregs, MUSB_POWER),
						musb_readb(mtk_musb->mregs,
						MUSB_DEVCTL),
			    musb_readw(mtk_musb->mregs,
						MUSB_INTRTX),
						musb_readb(mtk_musb->mregs,
						MUSB_INTRUSB));
			wait_for_completion_timeout(&stop_event, 1);
		}
	} while (atomic_read(&g_exec) == 1);
	if (atomic_read(&g_exec) == 0)
		return TEST_IS_STOP;
	DBG(0, "hnp start\n");
	if (intrusb & MUSB_INTR_RESUME)
		goto TD6_13;
	if (!(intrusb & MUSB_INTR_CONNECT)) {
		/* polling the connect from A-OPT, the UUT acts as host */
		DBG(0, "polling connect or resume form A-OPT,begin\n");
		ret = musb_polling_bus_interrupt(MUSB_INTR_CONNECT
							| MUSB_INTR_RESUME);
		DBG(0, "polling connect or resume form A-OPT,done,ret=0x%x\n",
			ret);
		if (ret == TEST_IS_STOP)
			return TEST_IS_STOP;
		if (ret == MUSB_INTR_RESUME)
			goto TD6_13;
		if (ret == DEV_HNP_TIMEOUT) {
			DBG(0, "B-UUT HNP timeout\n");
			devctl = musb_readb(mtk_musb->mregs, MUSB_DEVCTL);

			devctl &= ~MUSB_DEVCTL_HR;
			musb_writeb(mtk_musb->mregs, MUSB_DEVCTL, devctl);
			if (is_td_59)
				g_otg_message.msg = OTG_MSG_DEV_NOT_RESPONSE;
			return TEST_IS_STOP;
		}
	}
	/* reset the bus and check whether it is a hs device */
	musb_h_reset();
	musb_h_enumerate();
	/* suspend the bus */
	musb_h_suspend();
	/* polling the disconnect interrupt from A-OPT */
	DBG(0, "polling disconnect form A-OPT,begin\n");
	ret = musb_polling_bus_interrupt(MUSB_INTR_DISCONNECT);
	DBG(0, "polling disconnect form A-OPT,done,ret=0x%x\n", ret);

	if (ret == TEST_IS_STOP)
		return TEST_IS_STOP;
	DBG(0, "A-OPT is disconnected, UUT will be back to device\n");
	if (!(ret & MUSB_INTR_RESET)) {
		musb_d_soft_connect(true);
		/* polling the reset single form the A-OPT */
		DBG(0, "polling reset form A-OPT,begin\n");
		ret = musb_polling_bus_interrupt(MUSB_INTR_RESET);
		/* musb_d_reset (); */
		DBG(0, "polling reset form A-OPT,done,ret=0x%x\n", ret);
		if (ret == TEST_IS_STOP)
			return TEST_IS_STOP;
	}
	device_enumed = false;
	if (atomic_read(&g_exec) == 1)
		goto TD6_13;	/* TD5_5 */
	wait_for_completion(&stop_event);
}

int musb_otg_exec_cmd(unsigned int cmd)
{

	unsigned char devctl;
	unsigned char intrusb;
	unsigned char power;
	unsigned short csr0;
	unsigned int usb_l1intp;
	unsigned int usb_l1ints;

	if (!mtk_musb) {
		DBG(0, "mtk_musb is NULL,error!\n");
		return false;
	}

	switch (cmd) {
	case HOST_CMD_ENV_INIT:
		musb_otg_env_init();
		return 0;
	case HOST_CMD_ENV_EXIT:
		musb_otg_env_exit();
		return 0;
	}

	/* init */
	musb_writeb(mtk_musb->mregs, MUSB_POWER, 0x21);
	musb_writeb(mtk_musb->mregs, MUSB_DEVCTL, 0);
	msleep(300);

#ifdef DX_DBG
	devctl = musb_readb(mtk_musb->mregs, MUSB_DEVCTL);
	power = musb_readb(mtk_musb->mregs, MUSB_POWER);
	intrusb = musb_readb(mtk_musb->mregs, MUSB_INTRUSB);
	DBG(0, "1:cmd=%d,devctl=0x%x,power=0x%x,intrusb=0x%x\n",
						cmd, devctl, power, intrusb);
#endif
	musb_writew(mtk_musb->mregs, MUSB_INTRRX, 0xffff);
	musb_writew(mtk_musb->mregs, MUSB_INTRTX, 0xffff);
	musb_writeb(mtk_musb->mregs, MUSB_INTRUSB, 0xff);
	mdelay(10);
#ifdef DX_DBG
	devctl = musb_readb(mtk_musb->mregs, MUSB_DEVCTL);
	power = musb_readb(mtk_musb->mregs, MUSB_POWER);
	intrusb = musb_readb(mtk_musb->mregs, MUSB_INTRUSB);
	DBG(0, "2:cmd=%d,devctl=0x%x,power=0x%x,intrusb=0x%x\n",
						cmd, devctl, power, intrusb);
#endif
	high_speed = false;
	atomic_set(&g_exec, 1);

	DBG(0, "before exec:cmd=%d\n", cmd);

	switch (cmd) {
		/* electrical */
	case OTG_CMD_E_ENABLE_VBUS:
		DBG(0, "musb::enable VBUS!\n");
		musb_otg_set_session(true);
		musb_platform_set_vbus(mtk_musb, 1);
		while (atomic_read(&g_exec) == 1)
			msleep(100);
		musb_otg_set_session(false);
		musb_platform_set_vbus(mtk_musb, 0);
		break;
	case OTG_CMD_E_ENABLE_SRP:	/* need to clear session? */
		DBG(0, "musb::enable srp!\n");
		musb_otg_reset_usb();
		{
			u32 val = 0;

			val = USBPHY_READ32(0x6c);
			val = (val & ~(0xff<<0)) | (0x1<<0);
			USBPHY_WRITE32(0x6c, val);

			val = USBPHY_READ32(0x6c);
			val = (val & ~(0xff<<8)) | (0x1<<8);
			USBPHY_WRITE32(0x6c, val);
		}
		musb_writeb(mtk_musb->mregs, 0x7B, 1);
		musb_otg_set_session(true);
		while (atomic_read(&g_exec) == 1)
			msleep(100);
		musb_otg_set_session(false);
		break;
	case OTG_CMD_E_START_DET_SRP:
		/* need as a A-device */
		musb_writeb(mtk_musb->mregs, MUSB_DEVCTL, 0);
		devctl = musb_readb(mtk_musb->mregs, MUSB_DEVCTL);
		while ((atomic_read(&g_exec) == 1) && (devctl & 0x18)) {
			DBG(0, "musb::not below session end!\n");
			msleep(100);
			devctl = musb_readb(mtk_musb->mregs, MUSB_DEVCTL);
		}
		while ((atomic_read(&g_exec) == 1) && (!(devctl & 0x10))) {
			DBG(0, "musb::not above session end!\n");
			msleep(100);
			devctl = musb_readb(mtk_musb->mregs, MUSB_DEVCTL);
		}
		devctl |= MUSB_DEVCTL_SESSION;
		musb_writeb(mtk_musb->mregs, MUSB_DEVCTL, devctl);
		while (atomic_read(&g_exec) == 1)
			msleep(100);
		musb_writeb(mtk_musb->mregs, MUSB_DEVCTL, 0);
		break;
	case OTG_CMD_E_START_DET_VBUS:
		usb_l1intp = musb_readl(mtk_musb->mregs, USB_L1INTP);
		usb_l1intp &= ~(1 << 10);
		musb_writel(mtk_musb->mregs, USB_L1INTP, usb_l1intp);
		usb_l1ints = musb_readl(mtk_musb->mregs, USB_L1INTS);
		while ((usb_l1ints & (1 << 8)) == 0) {
			DBG(0, "musb::vbus is 0!\n");
			msleep(100);
			usb_l1ints = musb_readl(mtk_musb->mregs, USB_L1INTS);
		}
		DBG(0, "musb::vbus is detected!\n");
		power = musb_readb(mtk_musb->mregs, MUSB_POWER);
		power |= MUSB_POWER_SOFTCONN;
		musb_writeb(mtk_musb->mregs, MUSB_POWER, power);
		while (atomic_read(&g_exec) == 1)
			msleep(100);
		musb_writeb(mtk_musb->mregs, MUSB_POWER, 0x21);
		break;

	case OTG_CMD_P_B_UUT_TD59:
		is_td_59 = true;
		if (is_td_59)
			DBG(0, "TD5.9 will be tested!\n");
		break;

		/* protocal */
	case OTG_CMD_P_A_UUT:
		DBG(0, "A-UUT starts...\n");
		otg_cmd_a_uut();
		DBG(0, "the test as A-UUT is done\n");
		break;

	case OTG_CMD_P_B_UUT:
		DBG(0, "B-UUT starts...\n");
		otg_cmd_b_uut();
		DBG(0, "the test as B_UUT is done\n");
		break;

	case HOST_CMD_TEST_SE0_NAK:
	case HOST_CMD_TEST_J:
	case HOST_CMD_TEST_K:
	case HOST_CMD_TEST_PACKET:
	case HOST_CMD_SUSPEND_RESUME:
	case HOST_CMD_GET_DESCRIPTOR:
	case HOST_CMD_SET_FEATURE:
		musb_host_test_mode(cmd);
		while (atomic_read(&g_exec) == 1)
			msleep(100);
		break;
	}
	DBG(0, "%s--\n", __func__);
	return 0;

}

void musb_otg_stop_cmd(void)
{
	DBG(0, "%s++\n", __func__);
	atomic_set(&g_exec, 0);
	is_td_59 = false;
	complete(&stop_event);
}

unsigned int musb_otg_message(void)
{
	/* for EM to pop the message */
	unsigned int msg;

	msg = g_otg_message.msg;
	g_otg_message.msg = 0;
	return msg;
}

void musb_otg_message_cb(void)
{
	/* when the OK button is clicked on EM, this func is called. */
	spin_lock(&g_otg_message.lock);
	g_otg_message.msg = 0;
	spin_unlock(&g_otg_message.lock);
}

static int musb_otg_test_open(struct inode *inode, struct file *file)
{
	DBG(0, "%s++\n", __func__);
	return 0;
}

static int musb_otg_test_release(struct inode *inode, struct file *file)
{
	return 0;
}

ssize_t musb_otg_test_read(struct file *filp,
		char __user *buf, size_t count, loff_t *ppos)
{
	int ret = 0;
	unsigned int message = musb_otg_message();

	if (message)
		DBG(0, "%s:message=0x%x\n", __func__, message);
	if (put_user((unsigned int)message, (unsigned int *)buf))
		ret = -EFAULT;
	return ret;
}

ssize_t musb_otg_test_write(struct file *filp,
				const char __user *buf, size_t count,
			    loff_t *ppos)
{
	int ret = 0;
	unsigned char value;

	if (get_user(value, (unsigned char *)buf))
		ret = -EFAULT;
	else {
		if (value == OTG_STOP_CMD) {
			DBG(0, "%s::OTG_STOP_CMD\n", __func__);
			musb_otg_stop_cmd();
		} else if (value == OTG_INIT_MSG) {
			DBG(0, "%s::OTG_INIT_MSG\n", __func__);
			musb_otg_message_cb();
		} else {
			DBG(0, "musb_otg_test_write::the
				value is invalid,0x%x\n",
				value);
			ret = -EFAULT;
		}
	}
	return ret;
}

static long musb_otg_test_ioctl
		(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;

	DBG(0, "%s :cmd=0x%x\n", __func__, cmd);
	ret = musb_otg_exec_cmd(cmd);
	return (long)ret;
}


static const struct file_operations musb_otg_test_fops = {
	.owner = THIS_MODULE,
	.open = musb_otg_test_open,
	.release = musb_otg_test_release,
	.read = musb_otg_test_read,
	.write = musb_otg_test_write,
	.unlocked_ioctl = musb_otg_test_ioctl,
};

static struct miscdevice musb_otg_test_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	/* .minor = 254, */
	.name = TEST_DRIVER_NAME,
	.fops = &musb_otg_test_fops,
	.mode = 0666,
};


static const u8 musb_host_test_packet[53] = {
	/* implicit SYNC then DATA0 to start */

	/* JKJKJKJK x9 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* JJKKJJKK x8 */
	0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	/* JJJJKKKK x8 */
	0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee,
	/* JJJJJJJKKKKKKK x8 */
	0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	/* JJJJJJJK x8 */
	0x7f, 0xbf, 0xdf, 0xef, 0xf7, 0xfb, 0xfd,
	/* JKKKKKKK x10, JK */
	0xfc, 0x7e, 0xbf, 0xdf, 0xef, 0xf7, 0xfb, 0xfd, 0x7e
	    /* implicit CRC16 then EOP to end */
};

void musb_host_load_testpacket(struct musb *musb)
{
	unsigned short csr0 = musb_readw(musb->mregs, 0x102);

	DBG(0, "csr0=0x%x\n", csr0);
	musb->ignore_disconnect = 1;
	musb_otg_write_fifo(53, (u8 *) musb_host_test_packet);
}


void host_test_mode(struct musb *musb, unsigned int wIndex)
{
	unsigned char temp;
	unsigned char power;
	struct usb_ctrlrequest setup_packet;
	struct usb_device_descriptor device_descriptor;

	setup_packet.bRequestType = USB_DIR_IN |
					USB_TYPE_STANDARD | USB_RECIP_DEVICE;
	setup_packet.bRequest = USB_REQ_GET_DESCRIPTOR;
	setup_packet.wIndex = 0;
	setup_packet.wValue = 0x0100;
	setup_packet.wLength = 0x40;
	musb_otg_set_session(true);
	msleep(200);
	pr_debug("devctl = 0x%x\n", musb_readb(musb->mregs, MUSB_DEVCTL));
	switch (wIndex) {
	case HOST_CMD_TEST_SE0_NAK:
		DBG(0, "TEST_SE0_NAK\n");
		temp = MUSB_TEST_SE0_NAK;
		musb_writeb(musb->mregs, MUSB_TESTMODE, temp);

		break;
	case HOST_CMD_TEST_J:
		DBG(0, "TEST_J\n");
		temp = MUSB_TEST_J;
		musb_writeb(musb->mregs, MUSB_TESTMODE, temp);

		break;
	case HOST_CMD_TEST_K:
		DBG(0, "TEST_K\n");
		temp = MUSB_TEST_K;
		musb_writeb(musb->mregs, MUSB_TESTMODE, temp);

		break;
	case HOST_CMD_TEST_PACKET:
		DBG(0, "TEST_PACKET\n");
		temp = MUSB_TEST_PACKET;
		musb_host_load_testpacket(musb);
		musb_writeb(musb->mregs, MUSB_TESTMODE, temp);
		musb_writew(musb->mregs, 0x102, MUSB_CSR0_TXPKTRDY);
		break;

	case HOST_CMD_SUSPEND_RESUME:
		/* HS_HOST_PORT_SUSPEND_RESUME */
		DBG(0, "HS_HOST_PORT_SUSPEND_RESUME\n");
		msleep(5000);
		/* the host must continue sending SOFs for 15s */
		DBG(0, "please begin to trigger suspend!\n");
		msleep(10000);
		power = musb_readb(musb->mregs, MUSB_POWER);
		power |= MUSB_POWER_SUSPENDM | MUSB_POWER_ENSUSPEND;
		musb_writeb(musb->mregs, MUSB_POWER, power);
		msleep(5000);
		DBG(0, "please begin to trigger resume!\n");
		msleep(10000);
		power &= ~MUSB_POWER_SUSPENDM;
		power |= MUSB_POWER_RESUME;
		musb_writeb(musb->mregs, MUSB_POWER, power);
		mdelay(25);
		power &= ~MUSB_POWER_RESUME;
		musb_writeb(musb->mregs, MUSB_POWER, power);
		/* SOF continue */
		musb_h_setup(&setup_packet);
		break;
	case HOST_CMD_GET_DESCRIPTOR:
		/* SINGLE_STEP_GET_DEVICE_DESCRIPTOR setup */
		DBG(0, "SINGLE_STEP_GET_DEVICE_DESCRIPTOR\n");
		/* the host issues SOFs for 15s allowing the test engineer
		 * to raise the scope trigger just above the SOF voltage level.
		 */
		msleep(15000);
		musb_h_setup(&setup_packet);
		break;
	case HOST_CMD_SET_FEATURE:
		/* SINGLE_STEP_GET_DEVICE_DESCRIPTOR execute */
		DBG(0, "SINGLE_STEP_GET_DEVICE_DESCRIPTOR\n");
		/* get device descriptor */
		musb_h_setup(&setup_packet);
		msleep(15000);
		musb_h_in_data((char *)&device_descriptor,
			sizeof(struct usb_device_descriptor));
		musb_h_out_status();
		break;
	default:
		break;

	}
	/* while(1); */
}

static int musb_host_test_mode(unsigned char cmd)
{
	musb_platform_set_vbus(mtk_musb, 1);
	musb_otg_reset_usb();
	host_test_mode(mtk_musb, cmd);
	return 0;
}

static int __init musb_otg_test_init(void)
{
#ifdef CONFIG_OF
	struct device_node *np;

	np = of_find_compatible_node(NULL, NULL, "mediatek,PERICFG");
	if (!np)
		pr_debug("get PERICFG node fail");
	pericfg_base = (unsigned long)of_iomap(np, 0);
#else
	pericfg_base = PERICFG_BASE;
#endif
	misc_register(&musb_otg_test_dev);
	return 0;
}

static void __exit musb_otg_test_exit(void)
{
	misc_deregister(&musb_otg_test_dev);
}


module_init(musb_otg_test_init);
module_exit(musb_otg_test_exit);
