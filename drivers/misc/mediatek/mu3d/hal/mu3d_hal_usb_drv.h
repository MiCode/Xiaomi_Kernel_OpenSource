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

#include "mu3d_hal_hw.h"
#undef EXTERN

#ifdef _MTK_USB_DRV_EXT_
#define EXTERN
#else
#define EXTERN \
extern
#endif



#define MAX_EP_NUM		8	/* 4 Tx and 4 Rx */
#define USB_BUF_SIZE    65536
#define MAX_SLOT		(2-1)

/*EP0, TX, RX has separate SRAMs*/
#define USB_TX_FIFO_START_ADDRESS  0
#define USB_RX_FIFO_START_ADDRESS  0

#ifndef IRQ_USB_MC_NINT_CODE
#define	IRQ_USB_MC_NINT_CODE	8
#endif
#ifndef IRQ_USB_DMA_NINT_CODE
#define IRQ_USB_DMA_NINT_CODE	9
#endif

/* IN, OUT pipe index for ep_number */
enum USB_DIR {
	USB_TX = 0,
	USB_RX
};

/* CTRL, BULK, INTR, ISO endpoint */
enum TRANSFER_TYPE {
	USB_CTRL = 0,
	USB_BULK = 2,
	USB_INTR = 3,
	USB_ISO = 1
};

enum USB_SPEED {
	SSUSB_SPEED_INACTIVE = 0,
	SSUSB_SPEED_FULL = 1,
	SSUSB_SPEED_HIGH = 3,
	SSUSB_SPEED_SUPER = 4,
};

enum EP0_STATE {
	EP0_IDLE = 0,
	EP0_TX,
	EP0_RX,
};

struct USB_EP_SETTING {
	enum TRANSFER_TYPE transfer_type;
	unsigned int fifoaddr;
	unsigned int fifosz;
	unsigned int maxp;
	enum USB_DIR dir;
	unsigned char enabled;
};

struct USB_REQ {
	unsigned char *buf;
	/* unsigned char* dma_adr; */
	dma_addr_t dma_adr;
	unsigned int actual;
	unsigned int count;
	unsigned int currentCount;
	unsigned int complete;
	unsigned int needZLP;
	unsigned int transferCount;
};

struct USB_TEST_SETTING {
	enum USB_SPEED speed;
	struct USB_EP_SETTING ep_setting[2 * MAX_EP_NUM + 1];
};

/*=============================================
*
*		USB 3  test
*
*=============================================
*/

/* #define NUM_TXENDPS                 4 */
/* #define NUM_RXENDPS                 4 */
/* #define NUM_EPS                     (NUM_TXENDPS + NUM_RXENDPS + 1) */

/* #define MGC_END0_FIFOSIZE           64 */
/* #define MGC_RX_DMA_ENABLE_LEVEL     32 */

/* #define IsDbf                       0x00000001 */
/* #define IsTx                        0x00000002 */
/* #define IsHalt                      0x00000004 */
/* #define IsEnabled                   0x80000000 */

/* #define REQUEST_START_TX            0x1 */
/* #define REQUEST_START_RX            0x2 */


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




EXTERN struct USB_REQ g_u3d_req[2 * MAX_EP_NUM + 1];
EXTERN struct USB_TEST_SETTING g_u3d_setting;
EXTERN unsigned int g_TxFIFOadd;
EXTERN unsigned int g_RxFIFOadd;


EXTERN struct USB_REQ *mu3d_hal_get_req(int ep_num, enum USB_DIR dir);
EXTERN void mu3d_hal_pdn_dis(void);
EXTERN void mu3d_hal_ssusb_en(void);
EXTERN void _ex_mu3d_hal_ssusb_en(void);
EXTERN void mu3d_hal_rst_dev(void);
EXTERN int mu3d_hal_check_clk_sts(void);
EXTERN int mu3d_hal_link_up(int latch_val);
EXTERN void mu3d_hal_initr_dis(void);
EXTERN void mu3d_hal_clear_intr(void);
EXTERN void mu3d_hal_system_intr_en(void);
EXTERN void _ex_mu3d_hal_system_intr_en(void);
EXTERN int mu3d_hal_read_fifo_burst(int ep_num, unsigned char *buf);
EXTERN int mu3d_hal_read_fifo(int ep_num, unsigned char *buf);
EXTERN int mu3d_hal_write_fifo_burst(int ep_num, int length, unsigned char *buf,
					   int maxp);
EXTERN int mu3d_hal_write_fifo(int ep_num, int length, unsigned char *buf,
				     int maxp);
EXTERN void _ex_mu3d_hal_ep_enable(unsigned char ep_num, enum USB_DIR dir, enum TRANSFER_TYPE type,
				   int maxp, char interval, char slot, char burst, char mult);
EXTERN void mu3d_hal_ep_enable(unsigned char ep_num, enum USB_DIR dir, enum TRANSFER_TYPE type, int maxp,
			       char interval, char slot, char burst, char mult);
EXTERN void mu3d_hal_resume(void);
EXTERN void mu3d_hal_u2dev_connect(void);
EXTERN void mu3d_hal_u2dev_disconn(void);
EXTERN void mu3d_hal_u3dev_en(void);
EXTERN void mu3d_hal_u3dev_dis(void);
EXTERN void mu3d_hal_unfigured_ep(void);
EXTERN void mu3d_hal_unfigured_ep_num(unsigned char ep_num, enum USB_DIR dir);
EXTERN void mu3d_hal_set_speed(enum USB_SPEED usb_speed);
EXTERN void mu3d_hal_det_speed(enum USB_SPEED speed, unsigned char det_speed);
EXTERN void mu3d_hal_pdn_cg_en(void);
EXTERN void mu3d_hal_pdn_ip_port(unsigned char on, unsigned char touch_dis, unsigned char u3, unsigned char u2);
EXTERN void mu3d_hal_dft_reg(void);

#undef EXTERN

#ifdef SUPPORT_U3
extern unsigned int musb_hal_speed;
#endif

#endif				/* USB_DRV_H */
