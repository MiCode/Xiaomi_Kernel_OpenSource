/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2017 MediaTek Inc.
 */

#ifndef __MT_MUSB_REG_H__
#define __MT_MUSB_REG_H__

#include "musb_debug.h"

#define MUSB_EP0_FIFOSIZE	64	/* This is non-configurable */

/*
 * MUSB Register bits
 */

/* POWER */
#define MUSB_POWER_ISOUPDATE	0x80
#define MUSB_POWER_SOFTCONN	0x40
#define MUSB_POWER_HSENAB	0x20
#define MUSB_POWER_HSMODE	0x10
#define MUSB_POWER_RESET	0x08
#define MUSB_POWER_RESUME	0x04
#define MUSB_POWER_SUSPENDM	0x02
#define MUSB_POWER_ENSUSPEND	0x01

/* INTRUSB */
#define MUSB_INTR_SUSPEND	0x01
#define MUSB_INTR_RESUME	0x02
#define MUSB_INTR_RESET		0x04
#define MUSB_INTR_BABBLE	0x04
#define MUSB_INTR_SOF		0x08
#define MUSB_INTR_CONNECT	0x10
#define MUSB_INTR_DISCONNECT	0x20
#define MUSB_INTR_SESSREQ	0x40
#define MUSB_INTR_VBUSERROR	0x80	/* For SESSION end */

/* DEVCTL */
#define MUSB_DEVCTL_BDEVICE	0x80
#define MUSB_DEVCTL_FSDEV	0x40
#define MUSB_DEVCTL_LSDEV	0x20
#define MUSB_DEVCTL_VBUS	0x18
#define MUSB_DEVCTL_VBUS_SHIFT	3
#define MUSB_DEVCTL_HM		0x04
#define MUSB_DEVCTL_HR		0x02
#define MUSB_DEVCTL_SESSION	0x01

/* MUSB ULPI VBUSCONTROL */
#define MUSB_ULPI_USE_EXTVBUS	0x01
#define MUSB_ULPI_USE_EXTVBUSIND 0x02
/* ULPI_REG_CONTROL */
#define MUSB_ULPI_REG_REQ	(1 << 0)
#define MUSB_ULPI_REG_CMPLT	(1 << 1)
#define MUSB_ULPI_RDN_WR	(1 << 2)

/* TESTMODE */
#define MUSB_TEST_FORCE_HOST	0x80
#define MUSB_TEST_FIFO_ACCESS	0x40
#define MUSB_TEST_FORCE_FS	0x20
#define MUSB_TEST_FORCE_HS	0x10
#define MUSB_TEST_PACKET	0x08
#define MUSB_TEST_K		0x04
#define MUSB_TEST_J		0x02
#define MUSB_TEST_SE0_NAK	0x01

/* TXTYPE */
#define MUSB_TXTYPE_TXSPEED		0xC0
#define MUSB_TXTYPE_TXSPEED_FULL	0x80

/* Allocate for double-packet buffering (effectively doubles assigned _SIZE) */
#define MUSB_FIFOSZ_DPB	0x10
/* Allocation size (8, 16, 32, ... 4096) */
#define MUSB_FIFOSZ_SIZE	0x0f

/* CSR0 */
#define MUSB_CSR0_FLUSHFIFO	0x0100
#define MUSB_CSR0_TXPKTRDY	0x0002
#define MUSB_CSR0_RXPKTRDY	0x0001

/* CSR0 in Peripheral mode */
#define MUSB_CSR0_P_SVDSETUPEND	0x0080
#define MUSB_CSR0_P_SVDRXPKTRDY	0x0040
#define MUSB_CSR0_P_SENDSTALL	0x0020
#define MUSB_CSR0_P_SETUPEND	0x0010
#define MUSB_CSR0_P_DATAEND	0x0008
#define MUSB_CSR0_P_SENTSTALL	0x0004

/* CSR0 in Host mode */
#define MUSB_CSR0_H_DIS_PING		0x0800
#define MUSB_CSR0_H_WR_DATATOGGLE	0x0400	/* Set to allow setting: */
#define MUSB_CSR0_H_DATATOGGLE		0x0200	/* Data toggle control */
#define MUSB_CSR0_H_NAKTIMEOUT		0x0080
#define MUSB_CSR0_H_STATUSPKT		0x0040
#define MUSB_CSR0_H_REQPKT		0x0020
#define MUSB_CSR0_H_ERROR		0x0010
#define MUSB_CSR0_H_SETUPPKT		0x0008
#define MUSB_CSR0_H_RXSTALL		0x0004

/* CSR0 bits to avoid zeroing (write zero clears, write 1 ignored) */
#define MUSB_CSR0_P_WZC_BITS	\
	(MUSB_CSR0_P_SENTSTALL)
#define MUSB_CSR0_H_WZC_BITS	\
	(MUSB_CSR0_H_NAKTIMEOUT | MUSB_CSR0_H_RXSTALL \
	| MUSB_CSR0_RXPKTRDY)

/* TxType/RxType */
#define MUSB_TYPE_SPEED		0xc0
#define MUSB_TYPE_SPEED_SHIFT	6
#define MUSB_TYPE_PROTO		0x30	/* Implicitly zero for ep0 */
#define MUSB_TYPE_PROTO_SHIFT	4
#define MUSB_TYPE_REMOTE_END	0xf	/* Implicitly zero for ep0 */

/* CONFIGDATA */
#define MUSB_CONFIGDATA_MPRXE		0x80	/* Auto bulk pkt combining */
#define MUSB_CONFIGDATA_MPTXE		0x40	/* Auto bulk pkt splitting */
#define MUSB_CONFIGDATA_BIGENDIAN	0x20
#define MUSB_CONFIGDATA_HBRXE		0x10	/* HB-ISO for RX */
#define MUSB_CONFIGDATA_HBTXE		0x08	/* HB-ISO for TX */
#define MUSB_CONFIGDATA_DYNFIFO		0x04	/* Dynamic FIFO sizing */
#define MUSB_CONFIGDATA_SOFTCONE	0x02	/* SoftConnect */
#define MUSB_CONFIGDATA_UTMIDW		0x01	/* Data width 0/1 => 8/16bits */

/* TXCSR in Peripheral and Host mode */
#define MUSB_TXCSR_AUTOSET		0x8000
#define MUSB_TXCSR_DMAENAB		0x1000
#define MUSB_TXCSR_FRCDATATOG		0x0800
#define MUSB_TXCSR_DMAMODE		0x0400
#define MUSB_TXCSR_CLRDATATOG		0x0040
#define MUSB_TXCSR_FLUSHFIFO		0x0008
#define MUSB_TXCSR_FIFONOTEMPTY		0x0002
#define MUSB_TXCSR_TXPKTRDY		0x0001

/* TXCSR in Peripheral mode */
#define MUSB_TXCSR_P_ISO		0x4000
#define MUSB_TXCSR_P_INCOMPTX		0x0080
#define MUSB_TXCSR_P_SENTSTALL		0x0020
#define MUSB_TXCSR_P_SENDSTALL		0x0010
#define MUSB_TXCSR_P_UNDERRUN		0x0004

/* TXCSR in Host mode */
#define MUSB_TXCSR_H_WR_DATATOGGLE	0x0200
#define MUSB_TXCSR_H_DATATOGGLE		0x0100
#define MUSB_TXCSR_H_NAKTIMEOUT		0x0080
#define MUSB_TXCSR_H_RXSTALL		0x0020
#define MUSB_TXCSR_H_ERROR		0x0004

/* TXCSR bits to avoid zeroing (write zero clears, write 1 ignored) */
#define MUSB_TXCSR_P_WZC_BITS	\
	(MUSB_TXCSR_P_INCOMPTX | MUSB_TXCSR_P_SENTSTALL \
	| MUSB_TXCSR_P_UNDERRUN | MUSB_TXCSR_FIFONOTEMPTY)
#define MUSB_TXCSR_H_WZC_BITS	\
	(MUSB_TXCSR_H_NAKTIMEOUT | MUSB_TXCSR_H_RXSTALL \
	| MUSB_TXCSR_H_ERROR | MUSB_TXCSR_FIFONOTEMPTY)

/* RXCSR in Peripheral and Host mode */
#define MUSB_RXCSR_AUTOCLEAR		0x8000
#define MUSB_RXCSR_DMAENAB		0x2000
#define MUSB_RXCSR_DISNYET		0x1000
#define MUSB_RXCSR_PID_ERR		0x1000
#define MUSB_RXCSR_DMAMODE		0x0800
#define MUSB_RXCSR_INCOMPRX		0x0100
#define MUSB_RXCSR_CLRDATATOG		0x0080
#define MUSB_RXCSR_FLUSHFIFO		0x0010
#define MUSB_RXCSR_DATAERROR		0x0008
#define MUSB_RXCSR_FIFOFULL		0x0002
#define MUSB_RXCSR_RXPKTRDY		0x0001

/* ALPS00798316, Enable DMA RxMode1 */
#define MUSB_EP_RXPKTCOUNT		0x0300
/* ALPS00798316, Enable DMA RxMode1 */

/* RXCSR in Peripheral mode */
#define MUSB_RXCSR_P_ISO		0x4000
#define MUSB_RXCSR_P_SENTSTALL		0x0040
#define MUSB_RXCSR_P_SENDSTALL		0x0020
#define MUSB_RXCSR_P_OVERRUN		0x0004

/* RXCSR in Host mode */
#define MUSB_RXCSR_H_AUTOREQ		0x4000
#define MUSB_RXCSR_H_WR_DATATOGGLE	0x0400
#define MUSB_RXCSR_H_DATATOGGLE		0x0200
#define MUSB_RXCSR_H_RXSTALL		0x0040
#define MUSB_RXCSR_H_REQPKT		0x0020
#define MUSB_RXCSR_H_ERROR		0x0004

/* RXCSR bits to avoid zeroing (write zero clears, write 1 ignored) */
#define MUSB_RXCSR_P_WZC_BITS	\
	(MUSB_RXCSR_P_SENTSTALL | MUSB_RXCSR_P_OVERRUN \
	| MUSB_RXCSR_RXPKTRDY)
#define MUSB_RXCSR_H_WZC_BITS	\
	(MUSB_RXCSR_H_RXSTALL | MUSB_RXCSR_H_ERROR \
	| MUSB_RXCSR_DATAERROR | MUSB_RXCSR_RXPKTRDY)

/* HUBADDR */
#define MUSB_HUBADDR_MULTI_TT		0x80

/*
 * Common USB registers
 */

#define MUSB_FADDR		0x00	/* 8-bit */
#define MUSB_POWER		0x01	/* 8-bit */

#define MUSB_INTRTX		0x02	/* 16-bit */
#define MUSB_INTRRX		0x04
#define MUSB_INTRTXE		0x06
#define MUSB_INTRRXE		0x08
#define MUSB_INTRUSB		0x0A	/* 8 bit */
#define MUSB_INTRUSBE		0x0B	/* 8 bit */
#define MUSB_FRAME		0x0C
#define MUSB_INDEX		0x0E	/* 8 bit */
#define MUSB_TESTMODE		0x0F	/* 8 bit */
#define MUSB_TXTYPE_EP0		0x1A    /* 8 bit */

#define MUSB_FIFO_OFFSET(epnum)	(0x20 + ((epnum) * 4))

/*
 * Additional Control Registers
 */

#define MUSB_DEVCTL		0x60	/* 8 bit */

#define MUSB_OPSTATE    0x620
#define MUSB_OPSTATE_HOST_WAIT_DEV 0x21
#define OTG_IDLE 0

/*
 * MD Direct Tethering related Registers
 */

#define MUSB_USB_MDL1INTM	0x744
#define MUSB_QIMCR			0xc08
#define MUSB_QIMSR			0xc0c
#define MUSB_USBGCSR		0xb00

/* These are always controlled through the INDEX register */
#define MUSB_TXFIFOSZ		0x62	/* 8-bit (see masks) */
#define MUSB_RXFIFOSZ		0x63	/* 8-bit (see masks) */
#define MUSB_TXFIFOADD		0x64	/* 16-bit offset shifted right 3 */
#define MUSB_RXFIFOADD		0x66	/* 16-bit offset shifted right 3 */

/* REVISIT: vctrl/vstatus: optional vendor utmi+phy register at 0x68 */
#define MUSB_HWVERS		0x6C	/* 8 bit */
#define MUSB_ULPI_BUSCONTROL	0x70	/* 8 bit */
#define MUSB_ULPI_INT_MASK	0x72	/* 8 bit */
#define MUSB_ULPI_INT_SRC	0x73	/* 8 bit */
#define MUSB_ULPI_REG_DATA	0x74	/* 8 bit */
#define MUSB_ULPI_REG_ADDR	0x75	/* 8 bit */
#define MUSB_ULPI_REG_CONTROL	0x76	/* 8 bit */
#define MUSB_ULPI_RAW_DATA	0x77	/* 8 bit */

#define MUSB_EPINFO		0x78	/* 8 bit */
#define MUSB_RAMINFO		0x79	/* 8 bit */
#define MUSB_LINKINFO		0x7a	/* 8 bit */
#define MUSB_VPLEN		0x7b	/* 8 bit */
#define MUSB_HS_EOF1		0x7c	/* 8 bit */
#define MUSB_FS_EOF1		0x7d	/* 8 bit */
#define MUSB_LS_EOF1		0x7e	/* 8 bit */

/* Offsets to endpoint registers */
#define MUSB_TXMAXP		0x00
#define MUSB_TXCSR		0x02
#define MUSB_CSR0		MUSB_TXCSR	/* Re-used for EP0 */
#define MUSB_RXMAXP		0x04
#define MUSB_RXCSR		0x06
#define MUSB_RXCOUNT		0x08
#define MUSB_COUNT0		MUSB_RXCOUNT	/* Re-used for EP0 */
#define MUSB_TXTYPE		0x0A
#define MUSB_TYPE0		MUSB_TXTYPE	/* Re-used for EP0 */
#define MUSB_TXINTERVAL		0x0B
#define MUSB_NAKLIMIT0		MUSB_TXINTERVAL	/* Re-used for EP0 */
#define MUSB_RXTYPE		0x0C
#define MUSB_RXINTERVAL		0x0D
#define MUSB_FIFOSIZE		0x0F
#define MUSB_CONFIGDATA		MUSB_FIFOSIZE	/* Re-used for EP0 */

/* Offsets to endpoint registers in indexed model (using INDEX register) */
#define MUSB_INDEXED_OFFSET(_epnum, _offset)	\
	(0x10 + (_offset))


#define MUSB_TXCSR_MODE	0x2000

/* "bus control"/target registers, for host side multipoint (external hubs) */
#define MUSB_TXFUNCADDR	0x0480
#define MUSB_TXHUBADDR	0x0482

#define MUSB_RXFUNCADDR	0x0484
#define MUSB_RXHUBADDR	0x0486

/* Toggle registers */
#define MUSB_RXTOG	0x0080
#define MUSB_RXTOGEN	0x0082
#define MUSB_TXTOG	0x0084
#define MUSB_TXTOGEN	0x0086

#define MUSB_BUSCTL_OFFSET(_epnum, _offset) \
	(0x80 + (8*(_epnum)) + (_offset))

/* MTK Software reset reg */
#define MUSB_SWRST 0x74
#define MUSB_SWRST_PHY_RST         (1<<7)
#define MUSB_SWRST_PHYSIG_GATE_HS  (1<<6)
#define MUSB_SWRST_PHYSIG_GATE_EN  (1<<5)
#define MUSB_SWRST_REDUCE_DLY      (1<<4)
#define MUSB_SWRST_UNDO_SRPFIX     (1<<3)
#define MUSB_SWRST_FRC_VBUSVALID   (1<<2)
#define MUSB_SWRST_SWRST           (1<<1)
#define MUSB_SWRST_DISUSBRESET     (1<<0)

#define USB_L1INTS (0x00a0)	/* USB level 1 interrupt status register */
#define USB_L1INTM (0x00a4)	/* USB level 1 interrupt mask register  */
#define USB_L1INTP (0x00a8)	/* USB level 1 interrupt polarity register  */

/* #define DMA_INTR (USB_BASE + 0x0200) */
#define DMA_INTR_UNMASK_CLR_OFFSET (16)
#define DMA_INTR_UNMASK_SET_OFFSET (24)
#define USB_DMA_REALCOUNT(chan) (0x0280+0x10*(chan))


/* ====================== */
/* USB interrupt register */
/* ====================== */

/* word access */
#define TX_INT_STATUS        (1<<0)
#define RX_INT_STATUS        (1<<1)
#define USBCOM_INT_STATUS    (1<<2)
#define DMA_INT_STATUS       (1<<3)
#define PSR_INT_STATUS       (1<<4)
#define QINT_STATUS          (1<<5)
#define QHIF_INT_STATUS      (1<<6)
#define DPDM_INT_STATUS      (1<<7)
#define VBUSVALID_INT_STATUS (1<<8)
#define IDDIG_INT_STATUS     (1<<9)
#define DRVVBUS_INT_STATUS   (1<<10)

#define VBUSVALID_INT_POL    (1<<8)
#define IDDIG_INT_POL        (1<<9)
#define DRVVBUS_INT_POL      (1<<10)

#define RESREG		0x700	/* Reserved Register */
#define HSTPWRDWN_OPT	(1<<0)	/* connection detection option */

/*
 * OTG 2.0 Registers
 */
#define OTG20_CSRL	0x730	/* OTG20 Related Control Register L */
#define OTG20_CSRH	0x731	/* OTG20 Related Control Register H */

/* OTG20 Related Control Register L */
/* Disable Host mode entering
 * C_OPM_HSUS state before entering suspend
 */
#define DIS_HSUS	(1<<7)

/* EN: FS idle of A device will
 * transfer to HFS_HSUS state first
 */
#define A_HFS_WHNP	(1<<6)
/* Disables B device entering
 * C_OPM_B_WTDIS states before
 * switching to host mode
 */
#define DIS_B_WTDIS	(1<<5)

/* EN: host-hs-suspend entering OPM_FS_WTCON state first
 * while receiving disconnect signal
 */
#define HHS_SUSP_DIS	(1<<4)
/* EN: Disables B device
 * charging VBUS function
 * for OTG2.0 feature
 */
#define DIS_CHARGE_VBUS	(1<<3)

/* EN: hsus mode of host initializing resuming interrupt
 * while receiving resume K as waiting for HNP
 */
#define HSUS_RESUME_INT	(1<<2)
/* EN: hnpsus-mode of host entering host-normal mode as
 * receiving resume K while waiting for HNP
 */
#define HSUS_RESUME	(1<<1)


#define OTG20_EN	(1<<0)	/* Enables OTG 2.0 feature */

/* OTG20 Related Control Register H */
/* Informs whether HW sends bus reset automatically
 * while B-device changes to host with HNP
 */

#define DIS_AUTORST	(1<<1)

/* EN: to decrease A
 * device connection
 * denounce waiting timing
 */
#define CON_DEB_SHORT (1<<0)
/* QMU Registers */
#ifdef CONFIG_MTK_MUSB_QMU_SUPPORT
#define MUSB_QMUBASE	(0x800)
#define MUSB_QISAR	(0xc00)
#define MUSB_QIMR	(0xc04)
#define MUSB_GPZCR (0xc34)
#endif

static inline void musb_write_txfifosz(void __iomem *mbase, u8 c_size)
{
	musb_writeb(mbase, MUSB_TXFIFOSZ, c_size);
}

static inline void musb_write_txfifoadd(void __iomem *mbase, u16 c_off)
{
	musb_writew(mbase, MUSB_TXFIFOADD, c_off);
}

static inline void musb_write_rxfifosz(void __iomem *mbase, u8 c_size)
{
	musb_writeb(mbase, MUSB_RXFIFOSZ, c_size);
}

static inline void musb_write_rxfifoadd(void __iomem *mbase, u16 c_off)
{
	musb_writew(mbase, MUSB_RXFIFOADD, c_off);
}

static inline void musb_write_ulpi_buscontrol(void __iomem *mbase, u8 val)
{
	musb_writeb(mbase, MUSB_ULPI_BUSCONTROL, val);
}

static inline u8 musb_read_txfifosz(void __iomem *mbase)
{
	return musb_readb(mbase, MUSB_TXFIFOSZ);
}

static inline u16 musb_read_txfifoadd(void __iomem *mbase)
{
	return musb_readw(mbase, MUSB_TXFIFOADD);
}

static inline u8 musb_read_rxfifosz(void __iomem *mbase)
{
	return musb_readb(mbase, MUSB_RXFIFOSZ);
}

static inline u16 musb_read_rxfifoadd(void __iomem *mbase)
{
	return musb_readw(mbase, MUSB_RXFIFOADD);
}

static inline u8 musb_read_ulpi_buscontrol(void __iomem *mbase)
{
	return musb_readb(mbase, MUSB_ULPI_BUSCONTROL);
}

static inline u8 musb_read_configdata(void __iomem *mbase)
{
	musb_writeb(mbase, MUSB_INDEX, 0);
	return musb_readb(mbase, 0x10 + MUSB_CONFIGDATA);
}

static inline u16 musb_read_hwvers(void __iomem *mbase)
{
	return musb_readw(mbase, MUSB_HWVERS);
}

static inline void musb_write_rxfunaddr(void __iomem *mbase,
						u8 epnum, u8 qh_addr_reg)
{
	musb_writew(mbase, MUSB_RXFUNCADDR + 8 * epnum, qh_addr_reg);
}

static inline void musb_write_rxhubaddr(void __iomem *mbase,
						u8 epnum, u8 qh_h_addr_reg)
{
	u16 rx_hub_port_addr = musb_readw(mbase, 0x0486 + 8 * epnum);

	rx_hub_port_addr &= 0xff00;
	rx_hub_port_addr |= qh_h_addr_reg;
	musb_writew(mbase, MUSB_RXHUBADDR + 8 * epnum, rx_hub_port_addr);
}

static inline void musb_write_rxhubport(void __iomem *mbase,
						u8 epnum, u8 qh_h_port_reg)
{
	u16 rx_hub_port_addr = musb_readw(mbase, 0x0486 + 8 * epnum);
	u16 rx_port_addr = (u16) qh_h_port_reg;

	rx_hub_port_addr &= 0x00ff;
	rx_hub_port_addr |= (rx_port_addr << 8);
	musb_writew(mbase, MUSB_RXHUBADDR + 8 * epnum, rx_hub_port_addr);
}

static inline void musb_write_txfunaddr(void __iomem *mbase,
						u8 epnum, u8 qh_addr_reg)
{
	u16 new_qh_addr_reg;
	unsigned char power, txtype;

	power = musb_readb(mbase, MUSB_POWER);
	txtype = musb_readb(mbase, MUSB_TXTYPE_EP0);

	DBG(4, "%s - ep%d,  power: 0x%X, txtype: 0x%X\n", __func__, epnum,
								power, txtype);
	if ((power & MUSB_POWER_HSMODE) &&
		((txtype & MUSB_TXTYPE_TXSPEED) == MUSB_TXTYPE_TXSPEED_FULL)) {

		new_qh_addr_reg = (qh_addr_reg | 0x100);

		DBG(4, "%s SPLIT TRANS ep%d 0x%X 0x%X\n", __func__, epnum,
							qh_addr_reg,
							new_qh_addr_reg);

		musb_writew(mbase, MUSB_TXFUNCADDR + 8 * epnum,
							new_qh_addr_reg);
	} else {
		musb_writew(mbase, MUSB_TXFUNCADDR + 8 * epnum, qh_addr_reg);
	}
}

static inline void musb_write_txhubaddr(void __iomem *mbase,
						u8 epnum, u8 qh_h_addr_reg)
{
	u16 tx_hub_port_addr = musb_readw(mbase, 0x0482 + 8 * epnum);

	tx_hub_port_addr &= 0xff00;
	tx_hub_port_addr |= qh_h_addr_reg;
	musb_writew(mbase, MUSB_TXHUBADDR + 8 * epnum, tx_hub_port_addr);
}

static inline void musb_write_txhubport(void __iomem *mbase,
						u8 epnum, u8 qh_h_port_reg)
{
	u16 tx_hub_port_addr = musb_readw(mbase, 0x0482 + 8 * epnum);
	u16 tx_port_addr = (u16) qh_h_port_reg;

	tx_hub_port_addr &= 0x00ff;
	tx_hub_port_addr |= (tx_port_addr << 8);
	musb_writew(mbase, MUSB_TXHUBADDR + 8 * epnum, tx_hub_port_addr);
}

#endif				/* __MUSB_REGS_H__ */
