/*
 * MUSB OTG driver register defines
 *
 * Copyright 2005 Mentor Graphics Corporation
 * Copyright (C) 2005-2006 by Texas Instruments
 * Copyright (C) 2006-2007 Nokia Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef __MUSBFSH_REGS_H__
#define __MUSBFSH_REGS_H__

#define MUSBFSH_EP0_FIFOSIZE	64	/* This is non-configurable */

/*
 * MUSB Register bits
 */

/* POWER */
#define MUSBFSH_POWER_ISOUPDATE	0x80
#define MUSBFSH_POWER_SOFTCONN	0x40
#define MUSBFSH_POWER_HSENAB	0x20
#define MUSBFSH_POWER_HSMODE	0x10
#define MUSBFSH_POWER_RESET	0x08
#define MUSBFSH_POWER_RESUME	0x04
#define MUSBFSH_POWER_SUSPENDM	0x02
#define MUSBFSH_POWER_ENSUSPEND	0x01

/* INTRUSB */
#define MUSBFSH_INTR_SUSPEND	0x01
#define MUSBFSH_INTR_RESUME	0x02
#define MUSBFSH_INTR_RESET		0x04
#define MUSBFSH_INTR_BABBLE	0x04
#define MUSBFSH_INTR_SOF		0x08
#define MUSBFSH_INTR_CONNECT	0x10
#define MUSBFSH_INTR_DISCONNECT	0x20
#define MUSBFSH_INTR_SESSREQ	0x40
#define MUSBFSH_INTR_VBUSERROR	0x80	/* For SESSION end */

/* DEVCTL */
#define MUSBFSH_DEVCTL_BDEVICE	0x80
#define MUSBFSH_DEVCTL_FSDEV	0x40
#define MUSBFSH_DEVCTL_LSDEV	0x20
#define MUSBFSH_DEVCTL_VBUS	0x18
#define MUSBFSH_DEVCTL_VBUS_SHIFT	3
#define MUSBFSH_DEVCTL_HM		0x04
#define MUSBFSH_DEVCTL_HR		0x02
#define MUSBFSH_DEVCTL_SESSION	0x01

/* MUSB ULPI VBUSCONTROL */
#define MUSBFSH_ULPI_USE_EXTVBUS	0x01
#define MUSBFSH_ULPI_USE_EXTVBUSIND 0x02
/* ULPI_REG_CONTROL */
#define MUSBFSH_ULPI_REG_REQ	(1 << 0)
#define MUSBFSH_ULPI_REG_CMPLT	(1 << 1)
#define MUSBFSH_ULPI_RDN_WR	(1 << 2)

/* TESTMODE */
#define MUSBFSH_TEST_FORCE_HOST	0x80
#define MUSBFSH_TEST_FIFO_ACCESS	0x40
#define MUSBFSH_TEST_FORCE_FS	0x20
#define MUSBFSH_TEST_FORCE_HS	0x10
#define MUSBFSH_TEST_PACKET	0x08
#define MUSBFSH_TEST_K		0x04
#define MUSBFSH_TEST_J		0x02
#define MUSBFSH_TEST_SE0_NAK	0x01

/* Allocate for double-packet buffering (effectively doubles assigned _SIZE) */
#define MUSBFSH_FIFOSZ_DPB	0x10
/* Allocation size (8, 16, 32, ... 4096) */
#define MUSBFSH_FIFOSZ_SIZE	0x0f

/* CSR0 */
#define MUSBFSH_CSR0_FLUSHFIFO	0x0100
#define MUSBFSH_CSR0_TXPKTRDY	0x0002
#define MUSBFSH_CSR0_RXPKTRDY	0x0001

/* CSR0 in Peripheral mode */
#define MUSBFSH_CSR0_P_SVDSETUPEND	0x0080
#define MUSBFSH_CSR0_P_SVDRXPKTRDY	0x0040
#define MUSBFSH_CSR0_P_SENDSTALL	0x0020
#define MUSBFSH_CSR0_P_SETUPEND	0x0010
#define MUSBFSH_CSR0_P_DATAEND	0x0008
#define MUSBFSH_CSR0_P_SENTSTALL	0x0004

/* CSR0 in Host mode */
#define MUSBFSH_CSR0_H_DIS_PING		0x0800
#define MUSBFSH_CSR0_H_WR_DATATOGGLE	0x0400	/* Set to allow setting: */
#define MUSBFSH_CSR0_H_DATATOGGLE		0x0200	/* Data toggle control */
#define MUSBFSH_CSR0_H_NAKTIMEOUT		0x0080
#define MUSBFSH_CSR0_H_STATUSPKT		0x0040
#define MUSBFSH_CSR0_H_REQPKT		0x0020
#define MUSBFSH_CSR0_H_ERROR		0x0010
#define MUSBFSH_CSR0_H_SETUPPKT		0x0008
#define MUSBFSH_CSR0_H_RXSTALL		0x0004

/* CSR0 bits to avoid zeroing (write zero clears, write 1 ignored) */
#define MUSBFSH_CSR0_P_WZC_BITS	\
	(MUSBFSH_CSR0_P_SENTSTALL)
#define MUSBFSH_CSR0_H_WZC_BITS	\
	(MUSBFSH_CSR0_H_NAKTIMEOUT | MUSBFSH_CSR0_H_RXSTALL \
	| MUSBFSH_CSR0_RXPKTRDY)

/* TxType/RxType */
#define MUSBFSH_TYPE_SPEED		0xc0
#define MUSBFSH_TYPE_SPEED_SHIFT	6
#define MUSBFSH_TYPE_PROTO		0x30	/* Implicitly zero for ep0 */
#define MUSBFSH_TYPE_PROTO_SHIFT	4
#define MUSBFSH_TYPE_REMOTE_END	0xf	/* Implicitly zero for ep0 */

/* CONFIGDATA */
#define MUSBFSH_CONFIGDATA_MPRXE		0x80	/* Auto bulk pkt combining */
#define MUSBFSH_CONFIGDATA_MPTXE		0x40	/* Auto bulk pkt splitting */
#define MUSBFSH_CONFIGDATA_BIGENDIAN	0x20
#define MUSBFSH_CONFIGDATA_HBRXE		0x10	/* HB-ISO for RX */
#define MUSBFSH_CONFIGDATA_HBTXE		0x08	/* HB-ISO for TX */
#define MUSBFSH_CONFIGDATA_DYNFIFO		0x04	/* Dynamic FIFO sizing */
#define MUSBFSH_CONFIGDATA_SOFTCONE	0x02	/* SoftConnect */
#define MUSBFSH_CONFIGDATA_UTMIDW		0x01	/* Data width 0/1 => 8/16bits */

/* TXCSR in Peripheral and Host mode */
#define MUSBFSH_TXCSR_AUTOSET		0x8000
#define MUSBFSH_TXCSR_DMAENAB		0x1000
#define MUSBFSH_TXCSR_FRCDATATOG		0x0800
#define MUSBFSH_TXCSR_DMAMODE		0x0400
#define MUSBFSH_TXCSR_CLRDATATOG		0x0040
#define MUSBFSH_TXCSR_FLUSHFIFO		0x0008
#define MUSBFSH_TXCSR_FIFONOTEMPTY		0x0002
#define MUSBFSH_TXCSR_TXPKTRDY		0x0001

/* TXCSR in Peripheral mode */
#define MUSBFSH_TXCSR_P_ISO		0x4000
#define MUSBFSH_TXCSR_P_INCOMPTX		0x0080
#define MUSBFSH_TXCSR_P_SENTSTALL		0x0020
#define MUSBFSH_TXCSR_P_SENDSTALL		0x0010
#define MUSBFSH_TXCSR_P_UNDERRUN		0x0004

/* TXCSR in Host mode */
#define MUSBFSH_TXCSR_H_WR_DATATOGGLE	0x0200
#define MUSBFSH_TXCSR_H_DATATOGGLE		0x0100
#define MUSBFSH_TXCSR_H_NAKTIMEOUT		0x0080
#define MUSBFSH_TXCSR_H_RXSTALL		0x0020
#define MUSBFSH_TXCSR_H_ERROR		0x0004

/* TXCSR bits to avoid zeroing (write zero clears, write 1 ignored) */
#define MUSBFSH_TXCSR_P_WZC_BITS	\
	(MUSBFSH_TXCSR_P_INCOMPTX | MUSBFSH_TXCSR_P_SENTSTALL \
	| MUSBFSH_TXCSR_P_UNDERRUN | MUSBFSH_TXCSR_FIFONOTEMPTY)
#define MUSBFSH_TXCSR_H_WZC_BITS	\
	(MUSBFSH_TXCSR_H_NAKTIMEOUT | MUSBFSH_TXCSR_H_RXSTALL \
	| MUSBFSH_TXCSR_H_ERROR | MUSBFSH_TXCSR_FIFONOTEMPTY)

/* RXCSR in Peripheral and Host mode */
#define MUSBFSH_RXCSR_AUTOCLEAR		0x8000
#define MUSBFSH_RXCSR_DMAENAB		0x2000
#define MUSBFSH_RXCSR_DISNYET		0x1000
#define MUSBFSH_RXCSR_PID_ERR		0x1000
#define MUSBFSH_RXCSR_DMAMODE		0x0800
#define MUSBFSH_RXCSR_INCOMPRX		0x0100
#define MUSBFSH_RXCSR_CLRDATATOG		0x0080
#define MUSBFSH_RXCSR_FLUSHFIFO		0x0010
#define MUSBFSH_RXCSR_DATAERROR		0x0008
#define MUSBFSH_RXCSR_FIFOFULL		0x0002
#define MUSBFSH_RXCSR_RXPKTRDY		0x0001

/* RXCSR in Peripheral mode */
#define MUSBFSH_RXCSR_P_ISO		0x4000
#define MUSBFSH_RXCSR_P_SENTSTALL		0x0040
#define MUSBFSH_RXCSR_P_SENDSTALL		0x0020
#define MUSBFSH_RXCSR_P_OVERRUN		0x0004

/* RXCSR in Host mode */
#define MUSBFSH_RXCSR_H_AUTOREQ		0x4000
#define MUSBFSH_RXCSR_H_WR_DATATOGGLE	0x0400
#define MUSBFSH_RXCSR_H_DATATOGGLE		0x0200
#define MUSBFSH_RXCSR_H_RXSTALL		0x0040
#define MUSBFSH_RXCSR_H_REQPKT		0x0020
#define MUSBFSH_RXCSR_H_ERROR		0x0004

/* RXCSR bits to avoid zeroing (write zero clears, write 1 ignored) */
#define MUSBFSH_RXCSR_P_WZC_BITS	\
	(MUSBFSH_RXCSR_P_SENTSTALL | MUSBFSH_RXCSR_P_OVERRUN \
	| MUSBFSH_RXCSR_RXPKTRDY)
#define MUSBFSH_RXCSR_H_WZC_BITS	\
	(MUSBFSH_RXCSR_H_RXSTALL | MUSBFSH_RXCSR_H_ERROR \
	| MUSBFSH_RXCSR_DATAERROR | MUSBFSH_RXCSR_RXPKTRDY)

/* HUBADDR */
#define MUSBFSH_HUBADDR_MULTI_TT		0x80

/*
 * Common USB registers
 */

#define MUSBFSH_FADDR		0x00	/* 8-bit */
#define MUSBFSH_POWER		0x01	/* 8-bit */

#define MUSBFSH_INTRTX		0x02	/* 16-bit */
#define MUSBFSH_INTRRX		0x04
#define MUSBFSH_INTRTXE		0x06
#define MUSBFSH_INTRRXE		0x08
#define MUSBFSH_INTRUSB		0x0A	/* 8 bit */
#define MUSBFSH_INTRUSBE		0x0B	/* 8 bit */
#define MUSBFSH_FRAME		0x0C
#define MUSBFSH_INDEX		0x0E	/* 8 bit */
#define MUSBFSH_TESTMODE		0x0F	/* 8 bit */

/* Get offset for a given FIFO from musbfsh->mregs */
#define MUSBFSH_FIFO_OFFSET(epnum)	(0x20 + ((epnum) * 4))

/*
 * Additional Control Registers
 */

#define MUSBFSH_DEVCTL		0x60	/* 8 bit */

/* These are always controlled through the INDEX register */
#define MUSBFSH_TXFIFOSZ		0x62	/* 8-bit (see masks) */
#define MUSBFSH_RXFIFOSZ		0x63	/* 8-bit (see masks) */
#define MUSBFSH_TXFIFOADD		0x64	/* 16-bit offset shifted right 3 */
#define MUSBFSH_RXFIFOADD		0x66	/* 16-bit offset shifted right 3 */

#define MUSBFSH_EPINFO		0x78	/* 8 bit */
#define MUSBFSH_RAMINFO		0x79	/* 8 bit */
#define MUSBFSH_LINKINFO		0x7a	/* 8 bit */
#define MUSBFSH_VPLEN		0x7b	/* 8 bit */
#define MUSBFSH_HS_EOF1		0x7c	/* 8 bit */
#define MUSBFSH_FS_EOF1		0x7d	/* 8 bit */
#define MUSBFSH_LS_EOF1		0x7e	/* 8 bit */

#define MUSBFSH_RXTOG		0x80	/* 16 bit */
#define MUSBFSH_RXTOGEN		0x82	/* 16 bit */
#define MUSBFSH_TXTOG		0x84	/* 16 bit */
#define MUSBFSH_TXTOGEN		0x86	/* 16 bit */

/* Offsets to endpoint registers */
#define MUSBFSH_TXMAXP		0x00
#define MUSBFSH_TXCSR		0x02
#define MUSBFSH_CSR0		MUSBFSH_TXCSR	/* Re-used for EP0 */
#define MUSBFSH_RXMAXP		0x04
#define MUSBFSH_RXCSR		0x06
#define MUSBFSH_RXCOUNT		0x08
#define MUSBFSH_COUNT0		MUSBFSH_RXCOUNT	/* Re-used for EP0 */
#define MUSBFSH_TXTYPE		0x0A
#define MUSBFSH_TYPE0		MUSBFSH_TXTYPE	/* Re-used for EP0 */
#define MUSBFSH_TXINTERVAL		0x0B
#define MUSBFSH_NAKLIMIT0		MUSBFSH_TXINTERVAL	/* Re-used for EP0 */
#define MUSBFSH_RXTYPE		0x0C
#define MUSBFSH_RXINTERVAL		0x0D
#define MUSBFSH_FIFOSIZE		0x0F
#define MUSBFSH_CONFIGDATA		MUSBFSH_FIFOSIZE	/* Re-used for EP0 */

/* Offsets to endpoint registers in indexed model (using INDEX register) */
#define MUSBFSH_INDEXED_OFFSET(_epnum, _offset)	\
	(0x10 + (_offset))

/* Offsets to endpoint registers in flat models */
#define MUSBFSH_FLAT_OFFSET(_epnum, _offset)	\
	(0x100 + (0x10*(_epnum)) + (_offset))

#define MUSBFSH_TXCSR_MODE			0x2000

/* "bus control"/target registers, for host side multipoint (external hubs) */
#define MUSBFSH_TXFUNCADDR		0x0480
#define MUSBFSH_TXHUBADDR		0x0482

#define MUSBFSH_RXFUNCADDR		0x0484
#define MUSBFSH_RXHUBADDR		0x0486

#define MUSBFSH_BUSCTL_OFFSET(_epnum, _offset) \
	(0x80 + (8*(_epnum)) + (_offset))

static inline void musbfsh_write_txfifosz(void __iomem *mbase, u8 c_size)
{
	musbfsh_writeb(mbase, MUSBFSH_TXFIFOSZ, c_size);
}

static inline void musbfsh_write_txfifoadd(void __iomem *mbase, u16 c_off)
{
	musbfsh_writew(mbase, MUSBFSH_TXFIFOADD, c_off);
}

static inline void musbfsh_write_rxfifosz(void __iomem *mbase, u8 c_size)
{
	musbfsh_writeb(mbase, MUSBFSH_RXFIFOSZ, c_size);
}

static inline void  musbfsh_write_rxfifoadd(void __iomem *mbase, u16 c_off)
{
	musbfsh_writew(mbase, MUSBFSH_RXFIFOADD, c_off);
}

static inline u8 musbfsh_read_txfifosz(void __iomem *mbase)
{
	return musbfsh_readb(mbase, MUSBFSH_TXFIFOSZ);
}

static inline u16 musbfsh_read_txfifoadd(void __iomem *mbase)
{
	return musbfsh_readw(mbase, MUSBFSH_TXFIFOADD);
}

static inline u8 musbfsh_read_rxfifosz(void __iomem *mbase)
{
	return musbfsh_readb(mbase, MUSBFSH_RXFIFOSZ);
}

static inline u16  musbfsh_read_rxfifoadd(void __iomem *mbase)
{
	return musbfsh_readw(mbase, MUSBFSH_RXFIFOADD);
}

static inline u8 musbfsh_read_configdata(void __iomem *mbase)
{
	musbfsh_writeb(mbase, MUSBFSH_INDEX, 0);
	return musbfsh_readb(mbase, 0x10 + MUSBFSH_CONFIGDATA);
}

static inline void __iomem *musbfsh_read_target_reg_base(u8 i, void __iomem *mbase)
{
	return (MUSBFSH_BUSCTL_OFFSET(i, 0) + mbase);
}

static inline void musbfsh_write_rxfunaddr(void __iomem *mbase, u8 epnum,
		u8 qh_addr_reg)
{
	musbfsh_writew(mbase, MUSBFSH_RXFUNCADDR+8*epnum, qh_addr_reg);
}

static inline void musbfsh_write_rxhubaddr(void __iomem *mbase, u8 epnum,
		u8 qh_h_addr_reg)
{
    u16 rx_hub_port_addr = musbfsh_readw(mbase,MUSBFSH_RXHUBADDR+8*epnum);
    rx_hub_port_addr &= 0xff00;
    rx_hub_port_addr |= qh_h_addr_reg;
	musbfsh_writew(mbase, MUSBFSH_RXHUBADDR+8*epnum, rx_hub_port_addr);
}

static inline void musbfsh_write_rxhubport(void __iomem *mbase, u8 epnum,
		u8 qh_h_port_reg)
{
	u16 rx_hub_port_addr = musbfsh_readw(mbase,MUSBFSH_RXHUBADDR+8*epnum);
    u16 rx_port_addr = (u16)qh_h_port_reg;
    rx_hub_port_addr &= 0x00ff;
    rx_hub_port_addr |= (rx_port_addr<<8);
	musbfsh_writew(mbase, MUSBFSH_RXHUBADDR+8*epnum, rx_hub_port_addr);
}

static inline void  musbfsh_write_txfunaddr(void __iomem *mbase, u8 epnum,
		u8 qh_addr_reg)
{
	musbfsh_writew(mbase, MUSBFSH_TXFUNCADDR+8*epnum, qh_addr_reg);
}

static inline void  musbfsh_write_txhubaddr(void __iomem *mbase, u8 epnum,
		u8 qh_h_addr_reg)
{
	u16 tx_hub_port_addr = musbfsh_readw(mbase,MUSBFSH_TXHUBADDR+8*epnum);
    tx_hub_port_addr &= 0xff00;
    tx_hub_port_addr |= qh_h_addr_reg;
	musbfsh_writew(mbase, MUSBFSH_TXHUBADDR+8*epnum, tx_hub_port_addr);
}

static inline void  musbfsh_write_txhubport(void __iomem *mbase, u8 epnum,
		u8 qh_h_port_reg)
{
	u16 tx_hub_port_addr = musbfsh_readw(mbase,MUSBFSH_TXHUBADDR+8*epnum);
    u16 tx_port_addr = (u16)qh_h_port_reg;
    tx_hub_port_addr &= 0x00ff;
    tx_hub_port_addr |= (tx_port_addr<<8);
	musbfsh_writew(mbase, MUSBFSH_TXHUBADDR+8*epnum, tx_hub_port_addr);
}

#endif	/* __MUSBFSH_REGS_H__ */
