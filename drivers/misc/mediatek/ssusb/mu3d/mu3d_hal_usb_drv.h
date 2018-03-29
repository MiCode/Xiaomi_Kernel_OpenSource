/*
 * Copyright (C) 2015 MediaTek Inc.
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


#ifndef MTK_USB_DRV_H
#define MTK_USB_DRV_H
#include "musb_core.h"
/* #include "mu3d_hal_comm.h" */


#define MAX_EP_NUM		8	/* each Tx / Rx */
/* #define USB_BUF_SIZE    65536 */
#define MAX_SLOT		(2-1)

#ifdef SUPPORT_U3
/* EP0, TX, RX has separate SRAMs */
#define USB_TX_FIFO_START_ADDRESS  0
#define USB_RX_FIFO_START_ADDRESS  0
#else
/* EP0, TX, RX share one SRAM. 0-63 bytes are reserved for EP0 */
#define USB_TX_FIFO_START_ADDRESS  (64)
#define USB_RX_FIFO_START_ADDRESS  (64+512*MAX_EP_NUM)
#endif

#ifndef IRQ_USB_MC_NINT_CODE
#define	IRQ_USB_MC_NINT_CODE	8
#endif
#ifndef IRQ_USB_DMA_NINT_CODE
#define IRQ_USB_DMA_NINT_CODE	9
#endif

/* IN, OUT pipe index for ep_number */
typedef enum {
	USB_TX = 0,
	USB_RX
} USB_DIR;

/* CTRL, BULK, INTR, ISO endpoint */
typedef enum {
	USB_CTRL = 0,
	USB_BULK = 2,
	USB_INTR = 3,
	USB_ISO = 1
} TRANSFER_TYPE;

typedef enum {
	SSUSB_SPEED_INACTIVE = 0,
	SSUSB_SPEED_FULL = 1,
	SSUSB_SPEED_HIGH = 3,
	SSUSB_SPEED_SUPER = 4,
} USB_SPEED;

typedef enum {
	EP0_IDLE = 0,
	EP0_TX,
	EP0_RX,
} EP0_STATE;

struct usb_ep_setting {
	TRANSFER_TYPE transfer_type;
	u32 fifoaddr;
	u32 fifosz;
	u32 maxp;
	USB_DIR dir;
	u32 enabled;
};

#if 0
struct USB_TEST_SETTING {
	USB_SPEED speed;
	struct usb_ep_setting ep_setting[2 * MAX_EP_NUM + 1];
};
#endif
 /*
  * USB directions
  *
  * This bit flag is used in endpoint descriptors' bEndpointAddress field.
  * It's also one of three fields in control requests bRequestType.
  */
#define USB_DIR_OUT			0	/* to device */
#define USB_DIR_IN			0x80	/* to host */
#define USB_DIR_MASK		0x80	/* to host */

 /*
  * USB request types
  */

#define USB_TYPE_MASK			(0x03 << 5)
#define USB_TYPE_STANDARD		(0x00 << 5)
#define USB_TYPE_CLASS			(0x01 << 5)
#define USB_TYPE_VENDOR			(0x02 << 5)
#define USB_TYPE_RESERVED		(0x03 << 5)


 /*
  * Standard requests
  */
#define USB_REQ_GET_STATUS		        0x00
#define USB_REQ_CLEAR_FEATURE	        0x01
#define USB_REQ_SET_FEATURE		        0x03
#define USB_REQ_SET_ADDRESS		        0x05
#define USB_REQ_GET_DESCRIPTOR		    0x06
#define USB_REQ_SET_DESCRIPTOR		    0x07
#define USB_REQ_GET_CONFIGURATION	    0x08
#define USB_REQ_SET_CONFIGURATION	    0x09
#define USB_REQ_GET_INTERFACE		    0x0A
#define USB_REQ_SET_INTERFACE		    0x0B
#define USB_REQ_SYNCH_FRAME             0x0C
#define USB_REQ_EP0_IN_STALL			0xFD
#define USB_REQ_EP0_OUT_STALL			0xFE
#define USB_REQ_EP0_STALL				0xFF


void mu3d_hal_ssusb_en(struct musb *musb);
void mu3d_hal_ssusb_dis(struct musb *musb);
void mu3d_hal_rst_dev(struct musb *musb);
int mu3d_hal_check_clk_sts(struct musb *musb);
int mu3d_hal_link_up(struct musb *musb, int latch_val);
void mu3d_hal_initr_dis(struct musb *musb);
void mu3d_hal_clear_intr(struct musb *musb);
void mu3d_hal_system_intr_en(struct musb *musb);
int mu3d_hal_read_fifo_burst(struct musb *musb, int ep_num, u8 *buf);
int mu3d_hal_read_fifo(struct musb_hw_ep *hw_ep, int ep_num, u8 *buf);
int mu3d_hal_write_fifo_burst(struct musb *musb, int ep_num, int length, u8 *buf, int maxp);
int mu3d_hal_write_fifo(struct musb_hw_ep *hw_ep, int ep_num, int length, u8 *buf, int maxp);
void mu3d_hal_ep_enable(struct musb *musb, int ep_num, USB_DIR dir, TRANSFER_TYPE type, int maxp,
			int interval, int slot, int burst, int mult);
void mu3d_hal_u2dev_connect(struct musb *musb);
void mu3d_hal_u2dev_disconn(struct musb *musb);
void mu3d_hal_u3dev_en(struct musb *musb);
void mu3d_hal_u3dev_dis(struct musb *musb);
void mu3d_hal_unfigured_ep(struct musb *musb);
void mu3d_hal_unfigured_ep_num(struct musb *musb, int ep_num, USB_DIR dir);

/* void mu3d_hal_set_speed(struct musb *musb, USB_SPEED usb_speed); */
/* void mu3d_hal_det_speed(struct musb *musb, USB_SPEED speed, u8 det_speed); */
/* void mu3d_hal_dft_reg(struct musb *musb); */

#endif				/* USB_DRV_H */
