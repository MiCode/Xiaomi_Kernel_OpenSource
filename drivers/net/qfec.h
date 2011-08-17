/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/* qualcomm fast Ethernet controller HW description */

#ifndef _QFEC_EMAC_H_
# define _QFEC_EMAC_H_

# ifndef __KERNEL__
#   include "stdint.h"
# endif

# define MskBits(nBits, pos)     (((1 << nBits)-1)<<pos)

/* Rx/Tx Ethernet Buffer Descriptors
 *     status contains the ownership, status and receive length bits
 *     ctl    contains control and size bits for two buffers
 *     p_buf  contains a ptr to the data buffer
 *            MAC writes timestamp low into p_buf
 *     next   contains either ptr to 2nd buffer or next buffer-desc
 *            MAC writes timestamp high into next
 *
 *     status/ctl bit definition depend on RX or TX usage
 */


struct qfec_buf_desc {
	uint32_t            status;
	uint32_t            ctl;
	void               *p_buf;
	void               *next;
};

/* ownership bit operations */
# define BUF_OWN                     0x80000000 /* DMA owns buffer */
# define BUF_OWN_DMA                 BUF_OWN

/* RX buffer status bits */
# define BUF_RX_AFM               0x40000000 /* dest addr filt fail */

# define BUF_RX_FL                0x3fff0000 /* frame length */
# define BUF_RX_FL_GET(p)         ((p.status & BUF_RX_FL) >> 16)
# define BUF_RX_FL_SET(p, x) \
	(p.status = (p.status & ~BUF_RX_FL) | ((x << 16) & BUF_RX_FL))
# define BUF_RX_FL_GET_FROM_STATUS(status) \
				  (((status) & BUF_RX_FL) >> 16)

# define BUF_RX_ES                0x00008000 /* error summary */
# define BUF_RX_DE                0x00004000 /* error descriptor (es) */
# define BUF_RX_SAF               0x00002000 /* source addr filt fail */
# define BUF_RX_LE                0x00001000 /* length error */

# define BUF_RX_OE                0x00000800 /* overflow error (es) */
# define BUF_RX_VLAN              0x00000400 /* vlan tag */
# define BUF_RX_FS                0x00000200 /* first descriptor */
# define BUF_RX_LS                0x00000100 /* last  descriptor */

# define BUF_RX_IPC               0x00000080 /* cksum-err/giant-frame (es) */
# define BUF_RX_LC                0x00000040 /* late collision (es) */
# define BUF_RX_FT                0x00000020 /* frame type */
# define BUF_RX_RWT               0x00000010 /* rec watchdog timeout (es) */

# define BUF_RX_RE                0x00000008 /* rec error (es) */
# define BUF_RX_DBE               0x00000004 /* dribble bit err */
# define BUF_RX_CE                0x00000002 /* crc err (es) */
# define BUF_RX_CSE               0x00000001 /* checksum err */

# define BUF_RX_ERRORS  \
	(BUF_RX_DE  | BUF_RX_SAF | BUF_RX_LE  | BUF_RX_OE \
	| BUF_RX_IPC | BUF_RX_LC  | BUF_RX_RWT | BUF_RX_RE \
	| BUF_RX_DBE | BUF_RX_CE  | BUF_RX_CSE)

/* RX buffer control bits */
# define BUF_RX_DI                0x80000000 /* disable intrp on compl */
# define BUF_RX_RER               0x02000000 /* rec end of ring */
# define BUF_RX_RCH               0x01000000 /* 2nd addr chained */

# define BUF_RX_SIZ2              0x003ff800 /* buffer 2 size */
# define BUF_RX_SIZ2_GET(p)       ((p.control&BUF_RX_SIZ2) >> 11)

# define BUF_RX_SIZ               0x000007ff /* rx buf 1 size */
# define BUF_RX_SIZ_GET(p)        (p.ctl&BUF_RX_SIZ)

/* TX buffer status bits */
# define BUF_TX_TTSS              0x00020000 /* time stamp status */
# define BUF_TX_IHE               0x00010000 /* IP hdr err */

# define BUF_TX_ES                0x00008000 /* error summary */
# define BUF_TX_JT                0x00004000 /* jabber timeout (es) */
# define BUF_TX_FF                0x00002000 /* frame flushed (es) */
# define BUF_TX_PCE               0x00001000 /* payld cksum err */

# define BUF_TX_LOC               0x00000800 /* loss carrier (es) */
# define BUF_TX_NC                0x00000400 /* no carrier (es) */
# define BUF_TX_LC                0x00000200 /* late collision (es) */
# define BUF_TX_EC                0x00000100 /* excessive collision (es) */

# define BUF_TX_VLAN              0x00000080 /* VLAN frame */
# define BUF_TX_CC                MskBits(4, 3) /* collision count */
# define BUF_TX_CC_GET(p)         ((p.status&BUF_TX_CC)>>3)

# define BUF_TX_ED                0x00000004 /* excessive deferral (es) */
# define BUF_TX_UF                0x00000002 /* underflow err (es) */
# define BUF_TX_DB                0x00000001 /* deferred bit */

/* TX buffer control bits */
# define BUF_TX_IC                0x80000000 /* intrpt on compl */
# define BUF_TX_LS                0x40000000 /* last segment */
# define BUF_TX_FS                0x20000000 /* first segment */
# define BUF_TX_CIC               0x18000000 /* cksum insert control */
# define BUF_TX_CIC_SET(n)        (BUF_TX_CIC&(n<<27))

# define BUF_TX_DC                0x04000000 /* disable CRC */
# define BUF_TX_TER               0x02000000 /* end of ring */
# define BUF_TX_TCH               0x01000000 /* 2nd addr chained */

# define BUF_TX_DP                0x00800000 /* disable padding */
# define BUF_TX_TTSE              0x00400000 /* timestamp enable */

# define BUF_TX_SIZ2              0x003ff800 /* buffer 2 size */
# define BUF_TX_SIZ2_SET(n)       (BUF_TX_SIZ2(n<<11))

# define BUF_TX_SIZ               0x000007ff /* buffer 1 size */
# define BUF_TX_SIZ_SET(n)        (BUF_TX_SI1 & n)


/* Ethernet Controller Registers */
# define BUS_MODE_REG             0x1000

# define BUS_MODE_MB              0x04000000  /* mixed burst */
# define BUS_MODE_AAL             0x02000000  /* address alignment beats */
# define BUS_MODE_8XPBL           0x01000000  /*  */

# define BUS_MODE_USP             0x00800000  /* use separate PBL */
# define BUS_MODE_RPBL            0x007e0000  /* rxDMA PBL */
# define BUS_MODE_FB              0x00010000  /* fixed burst */

# define BUS_MODE_PR              0x0000c000  /* tx/rx priority */
# define BUS_MODE_PR4             0x0000c000  /* tx/rx priority 4:1 */
# define BUS_MODE_PR3             0x00008000  /* tx/rx priority 3:1 */
# define BUS_MODE_PR2             0x00004000  /* tx/rx priority 2:1 */
# define BUS_MODE_PR1             0x00000000  /* tx/rx priority 1:1 */

# define BUS_MODE_PBL             0x00003f00  /* programmable burst length */
# define BUS_MODE_PBLSET(n)       (BUS_MODE_PBL&(n<<8))

# define BUS_MODE_DSL             0x0000007c  /* descriptor skip length */
# define BUS_MODE_DSL_SET(n)      (BUS_MODE_DSL & (n << 2))

# define BUS_MODE_DA              0x00000002  /* DMA arbitration scheme  */
# define BUS_MODE_SWR             0x00000001  /* software reset */

#define BUS_MODE_REG_DEFAULT     (BUS_MODE_FB \
				| BUS_MODE_AAL \
				| BUS_MODE_PBLSET(16) \
				| BUS_MODE_DA \
				| BUS_MODE_DSL_SET(0))

# define TX_POLL_DEM_REG          0x1004      /* transmit poll demand */
# define RX_POLL_DEM_REG          0x1008      /* receive poll demand */

# define RX_DES_LST_ADR_REG       0x100c      /* receive buffer descriptor */
# define TX_DES_LST_ADR_REG       0x1010      /* transmit buffer descriptor */

# define STATUS_REG               0x1014

# define STATUS_REG_RSVRD_1       0xc0000000  /* reserved */
# define STATUS_REG_TTI           0x20000000  /* time-stamp trigger intrpt */
# define STATUS_REG_GPI           0x10000000  /* gmac PMT interrupt */

# define STATUS_REG_GMI           0x08000000  /* gmac MMC interrupt */
# define STATUS_REG_GLI           0x04000000  /* gmac line interface intrpt */

# define STATUS_REG_EB            0x03800000  /* error bits */
# define STATUS_REG_EB_DATA       0x00800000  /* error during data transfer */
# define STATUS_REG_EB_RDWR       0x01000000  /* error during rd/wr transfer */
# define STATUS_REG_EB_DESC       0x02000000  /* error during desc access */

# define STATUS_REG_TS            0x00700000  /* transmit process state */

# define STATUS_REG_TS_STOP       0x00000000  /*   stopped */
# define STATUS_REG_TS_FETCH_DESC 0x00100000  /*   fetching descriptor */
# define STATUS_REG_TS_WAIT       0x00200000  /*   waiting for status */
# define STATUS_REG_TS_READ       0x00300000  /*   reading host memory */
# define STATUS_REG_TS_TIMESTAMP  0x00400000  /*   timestamp write status */
# define STATUS_REG_TS_RSVRD      0x00500000  /*   reserved */
# define STATUS_REG_TS_SUSPEND    0x00600000  /*   desc-unavail/buffer-unflw */
# define STATUS_REG_TS_CLOSE      0x00700000  /*   closing desc */

# define STATUS_REG_RS            0x000e0000  /* receive process state */

# define STATUS_REG_RS_STOP       0x00000000  /*   stopped */
# define STATUS_REG_RS_FETCH_DESC 0x00020000  /*   fetching descriptor */
# define STATUS_REG_RS_RSVRD_1    0x00040000  /*   reserved */
# define STATUS_REG_RS_WAIT       0x00060000  /*   waiting for packet */
# define STATUS_REG_RS_SUSPEND    0x00080000  /*   desc unavail */
# define STATUS_REG_RS_CLOSE      0x000a0000  /*   closing desc */
# define STATUS_REG_RS_TIMESTAMP  0x000c0000  /*   timestamp write status */
# define STATUS_REG_RS_RSVRD_2    0x000e0000  /*   writing host memory */

# define STATUS_REG_NIS           0x00010000  /* normal intrpt   14|6|2|0 */
# define STATUS_REG_AIS           0x00008000  /* intrpts 13|10|9|8|7|5|4|3|1 */

# define STATUS_REG_ERI           0x00004000  /* early receive interrupt */
# define STATUS_REG_FBI           0x00002000  /* fatal bus error interrupt */
# define STATUS_REG_RSVRD_2       0x00001800  /* reserved */

# define STATUS_REG_ETI           0x00000400  /* early transmit interrupt */
# define STATUS_REG_RWT           0x00000200  /* receive watchdog timeout */
# define STATUS_REG_RPS           0x00000100  /* receive process stopped */

# define STATUS_REG_RU            0x00000080  /* receive buffer unavailable */
# define STATUS_REG_RI            0x00000040  /* receive interrupt */
# define STATUS_REG_UNF           0x00000020  /* transmit underflow */
# define STATUS_REG_OVF           0x00000010  /* receive overflow */

# define STATUS_REG_TJT           0x00000008  /* transmit jabber timeout */
# define STATUS_REG_TU            0x00000004  /* transmit buffer unavailable */
# define STATUS_REG_TPS           0x00000002  /* transmit process stopped */
# define STATUS_REG_TI            0x00000001  /* transmit interrupt */

# define STATUS_REG_AIS_BITS    (STATUS_REG_FBI | STATUS_REG_ETI \
				| STATUS_REG_RWT | STATUS_REG_RPS \
				| STATUS_REG_RU | STATUS_REG_UNF \
				| STATUS_REG_OVF | STATUS_REG_TJT \
				| STATUS_REG_TPS | STATUS_REG_AIS)

# define OPER_MODE_REG             0x1018

# define OPER_MODE_REG_DT          0x04000000 /* disab drop ip cksum err fr */
# define OPER_MODE_REG_RSF         0x02000000 /* rec store and forward */
# define OPER_MODE_REG_DFF         0x01000000 /* disable flush of rec frames */

# define OPER_MODE_REG_RFA2        0x00800000 /* thresh MSB for act flow-ctl */
# define OPER_MODE_REG_RFD2        0x00400000 /* thresh MSB deAct flow-ctl */
# define OPER_MODE_REG_TSF         0x00200000 /* tx store and forward */
# define OPER_MODE_REG_FTF         0x00100000 /* flush tx FIFO */

# define OPER_MODE_REG_RSVD1       0x000e0000 /* reserved */
# define OPER_MODE_REG_TTC         0x0001c000 /* transmit threshold control */
# define OPER_MODE_REG_TTC_SET(x)  (OPER_MODE_REG_TTC & (x << 14))
# define OPER_MODE_REG_ST          0x00002000 /* start/stop transmission cmd */

# define OPER_MODE_REG_RFD         0x00001800 /* thresh for deAct flow-ctl */
# define OPER_MODE_REG_RFA         0x00000600 /* threshold for act flow-ctl */
# define OPER_MODE_REG_EFC         0x00000100 /* enable HW flow-ctl */

# define OPER_MODE_REG_FEF         0x00000080 /* forward error frames */
# define OPER_MODE_REG_FUF         0x00000040 /* forward undersize good fr */
# define OPER_MODE_REG_RSVD2       0x00000020 /* reserved */
# define OPER_MODE_REG_RTC         0x00000018 /* receive threshold control */
# define OPER_MODE_REG_RTC_SET(x)  (OPER_MODE_REG_RTC & (x << 3))

# define OPER_MODE_REG_OSF         0x00000004 /* operate on second frame */
# define OPER_MODE_REG_SR          0x00000002 /* start/stop receive */
# define OPER_MODE_REG_RSVD3       0x00000001 /* reserved */


#define OPER_MODE_REG_DEFAULT    (OPER_MODE_REG_RSF \
				| OPER_MODE_REG_TSF \
				| OPER_MODE_REG_TTC_SET(5) \
				| OPER_MODE_REG_RTC_SET(1) \
				| OPER_MODE_REG_OSF)

# define INTRP_EN_REG              0x101c

# define INTRP_EN_REG_RSVD1        0xfffc0000 /* */
# define INTRP_EN_REG_NIE          0x00010000 /* normal intrpt summ enable */

# define INTRP_EN_REG_AIE          0x00008000 /* abnormal intrpt summary en */
# define INTRP_EN_REG_ERE          0x00004000 /* early receive intrpt enable */
# define INTRP_EN_REG_FBE          0x00002000 /* fatal bus error enable */

# define INTRP_EN_REG_RSVD2        0x00001800 /* */

# define INTRP_EN_REG_ETE          0x00000400 /* early tx intrpt enable */
# define INTRP_EN_REG_RWE          0x00000200 /* rx watchdog timeout enable */
# define INTRP_EN_REG_RSE          0x00000100 /* rx stopped enable */

# define INTRP_EN_REG_RUE          0x00000080 /* rx buf unavailable enable */
# define INTRP_EN_REG_RIE          0x00000040 /* rx interrupt enable */
# define INTRP_EN_REG_UNE          0x00000020 /* underflow interrupt enable */
# define INTRP_EN_REG_OVE          0x00000010 /* overflow interrupt enable */

# define INTRP_EN_REG_TJE          0x00000008 /* tx jabber timeout enable */
# define INTRP_EN_REG_TUE          0x00000004 /* tx buf unavailable enable */
# define INTRP_EN_REG_TSE          0x00000002 /* tx stopped enable */
# define INTRP_EN_REG_TIE          0x00000001 /* tx interrupt enable */

# define INTRP_EN_REG_All          (~(INTRP_EN_REG_RSVD1))

# define MIS_FR_REG                0x1020

# define MIS_FR_REG_FIFO_OVFL      0x10000000  /* fifo overflow */
# define MIS_FR_REG_FIFO_CNT       0x0FFE0000  /* fifo cnt */

# define MIS_FR_REG_MISS_OVFL      0x00010000  /* missed-frame overflow */
# define MIS_FR_REG_MISS_CNT       0x0000FFFF  /* missed-frame cnt */

# define RX_INTRP_WTCHDOG_REG      0x1024
# define AXI_BUS_MODE_REG          0x1028

# define AXI_BUS_MODE_EN_LPI       0x80000000  /* enable low power interface */
# define AXI_BUS_MODE_UNLK_MGC_PKT 0x40000000  /* unlock-magic-pkt/rem-wk-up */
# define AXI_BUS_MODE_WR_OSR_LMT   0x00F00000  /* max wr out stndg req limit */
# define AXI_BUS_MODE_RD_OSR_LMT   0x000F0000  /* max rd out stndg req limit */
# define AXI_BUS_MODE_AXI_AAL      0x00001000  /* address aligned beats */
# define AXI_BUS_MODE_BLEN256      0x00000080  /* axi burst length 256 */
# define AXI_BUS_MODE_BLEN128      0x00000040  /* axi burst length 128 */
# define AXI_BUS_MODE_BLEN64       0x00000020  /* axi burst length 64  */
# define AXI_BUS_MODE_BLEN32       0x00000010  /* axi burst length 32  */
# define AXI_BUS_MODE_BLEN16       0x00000008  /* axi burst length 16  */
# define AXI_BUS_MODE_BLEN8        0x00000004  /* axi burst length 8   */
# define AXI_BUS_MODE_BLEN4        0x00000002  /* axi burst length 4   */
# define AXI_BUS_MODE_UNDEF        0x00000001  /* axi undef burst length */

#define AXI_BUS_MODE_DEFAULT     (AXI_BUS_MODE_WR_OSR_LMT \
				| AXI_BUS_MODE_RD_OSR_LMT \
				| AXI_BUS_MODE_BLEN16 \
				| AXI_BUS_MODE_BLEN8 \
				| AXI_BUS_MODE_BLEN4)

# define AXI_STATUS_REG            0x102c

/*     0x1030-0x1044 reserved */
# define CUR_HOST_TX_DES_REG       0x1048
# define CUR_HOST_RX_DES_REG       0x104c
# define CUR_HOST_TX_BU_ADR_REG    0x1050
# define CUR_HOST_RX_BU_ADR_REG    0x1054

# define HW_FEATURE_REG            0x1058

# define MAC_CONFIG_REG            0x0000

# define MAC_CONFIG_REG_RSVD1      0xf8000000 /* */

# define MAC_CONFIG_REG_SFTERR     0x04000000 /* smii force tx error */
# define MAC_CONFIG_REG_CST        0x02000000 /* crc strip for type frame */
# define MAC_CONFIG_REG_TC         0x01000000 /* tx cfg in rgmii/sgmii/smii */

# define MAC_CONFIG_REG_WD         0x00800000 /* watchdog disable */
# define MAC_CONFIG_REG_JD         0x00400000 /* jabber disable */
# define MAC_CONFIG_REG_BE         0x00200000 /* frame burst enable */
# define MAC_CONFIG_REG_JE         0x00100000 /* jumbo frame enable */

# define MAC_CONFIG_REG_IFG        0x000e0000 /* inter frame gap, 96-(8*n) */
# define MAC_CONFIG_REG_DCRS       0x00010000 /* dis carrier sense during tx */

# define MAC_CONFIG_REG_PS         0x00008000 /* port select: 0/1 g/(10/100) */
# define MAC_CONFIG_REG_FES        0x00004000 /* speed 100 mbps */
# define MAC_CONFIG_REG_SPD        (MAC_CONFIG_REG_PS | MAC_CONFIG_REG_FES)
# define MAC_CONFIG_REG_SPD_1G     (0)
# define MAC_CONFIG_REG_SPD_100    (MAC_CONFIG_REG_PS | MAC_CONFIG_REG_FES)
# define MAC_CONFIG_REG_SPD_10     (MAC_CONFIG_REG_PS)
# define MAC_CONFIG_REG_SPD_SET(x) (MAC_CONFIG_REG_PS_FES & (x << 14))

# define MAC_CONFIG_REG_DO         0x00002000 /* disable receive own */
# define MAC_CONFIG_REG_LM         0x00001000 /* loopback mode */

# define MAC_CONFIG_REG_DM         0x00000800 /* (full) duplex mode */
# define MAC_CONFIG_REG_IPC        0x00000400 /* checksum offload */
# define MAC_CONFIG_REG_DR         0x00000200 /* disable retry */
# define MAC_CONFIG_REG_LUD        0x00000100 /* link up/down */

# define MAC_CONFIG_REG_ACS        0x00000080 /* auto pad/crc stripping */
# define MAC_CONFIG_REG_BL         0x00000060 /* back-off limit */
# define MAC_CONFIG_REG_BL_10      0x00000000 /*          10 */
# define MAC_CONFIG_REG_BL_8       0x00000020 /*          8  */
# define MAC_CONFIG_REG_BL_4       0x00000040 /*          4  */
# define MAC_CONFIG_REG_BL_1       0x00000060 /*          1  */
# define MAC_CONFIG_REG_DC         0x00000010 /* deferral check */

# define MAC_CONFIG_REG_TE         0x00000008 /* transmitter enable */
# define MAC_CONFIG_REG_RE         0x00000004 /* receiver enable */
# define MAC_CONFIG_REG_RSVD2      0x00000003 /* */

# define MAC_FR_FILTER_REG         0x0004

# define MAC_FR_FILTER_RA          0x80000000 /* receive all */

# define MAC_FR_FILTER_HPF         0x00000400 /* hash or perfect filter */
# define MAC_FR_FILTER_SAF         0x00000200 /* source addr filt en */
# define MAC_FR_FILTER_SAIF        0x00000100 /* SA inverse filter */
# define MAC_FR_FILTER_PCF_MASK    0x000000c0 /* pass control frames */
# define MAC_FR_FILTER_PCF_0       0x00000000 /*    */
# define MAC_FR_FILTER_PCF_1       0x00000040 /*    */
# define MAC_FR_FILTER_PCF_2       0x00000080 /*    */
# define MAC_FR_FILTER_PCF_3       0x000000c0 /*    */
# define MAC_FR_FILTER_DBF         0x00000020 /* disable broadcast frames */
# define MAC_FR_FILTER_PM          0x00000010 /* pass all multicast */
# define MAC_FR_FILTER_DAIF        0x00000008 /* DA inverse filtering */
# define MAC_FR_FILTER_HMC         0x00000004 /* hash multicast */
# define MAC_FR_FILTER_HUC         0x00000002 /* hash unicast */
# define MAC_FR_FILTER_PR          0x00000001 /* promiscuous mode */

# define HASH_TABLE_HIGH_REG       0x0008
# define HASH_TABLE_LOW_REG        0x000c

# define GMII_ADR_REG              0x0010

# define GMII_ADR_REG_PA           0x0000f800 /* addr bits */
# define GMII_ADR_REG_GR           0x000007c0 /* addr bits */
# define GMII_ADR_REG_RSVRD1       0x00000020 /* */
# define GMII_ADR_REG_CR           0x0000001c /* csr clock range */
# define GMII_ADR_REG_GW           0x00000002 /* gmii write */
# define GMII_ADR_REG_GB           0x00000001 /* gmii busy */

# define GMII_ADR_REG_ADR_SET(x)    (GMII_ADR_REG_PA & (x << 11))
# define GMII_ADR_REG_ADR_GET(x)    ((x & GMII_ADR_REG_PA) >> 11)

# define GMII_ADR_REG_REG_SET(x)    (GMII_ADR_REG_GR & (x << 6))
# define GMII_ADR_REG_REG_GET(x)    (((x & GMII_ADR_REG_GR) >> 6)

# define GMII_ADR_REG_CSR_SET(x)    (GMII_ADR_REG_CR & (x << 2))
# define GMII_ADR_REG_CSR_GET(x)    (((x & GMII_ADR_REG_CR) >> 2)

# define GMII_DATA_REG             0x0014

# define GMII_DATA_REG_DATA        0x0000ffff /* gmii data */

# define FLOW_CONTROL_REG          0x0018

# define FLOW_CONTROL_PT           0xFFFF0000 /* pause time */
# define FLOW_CONTROL_DZPQ         0x00000080 /* disable zero-quanta pause */
# define FLOW_CONTROL_PLT          0x00000030 /* pause level threshold */

# define FLOW_CONTROL_UP           0x00000008 /* unicast pause frame detect */
# define FLOW_CONTROL_RFE          0x00000004 /* receive flow control enable */
# define FLOW_CONTROL_TFE          0x00000002 /* transmit flow control enable */
# define FLOW_CONTROL_FCB          0x00000001 /* flow control busy (BPA) */

# define VLAN_TAG_REG              0x001c

# define VERSION_REG               0x0020

/* don't define these until HW if finished */
/* # define VERSION_USER              0x10 */
/* # define VERSION_QFEC              0x36 */

# define VERSION_REG_USER(x)       (0xFF & (x >> 8))
# define VERSION_REG_QFEC(x)       (0xFF & x)

# define DEBUG_REG                 0x0024

# define DEBUG_REG_RSVD1           0xfc000000 /* */
# define DEBUG_REG_TX_FIFO_FULL    0x02000000 /* Tx fifo full */
# define DEBUG_REG_TX_FIFO_NEMP    0x01000000 /* Tx fifo not empty */

# define DEBUG_REG_RSVD2           0x00800000 /* */
# define DEBUG_REG_TX_WR_ACTIVE    0x00400000 /* Tx fifo write ctrl active */

# define DEBUG_REG_TX_RD_STATE     0x00300000 /* Tx fifo rd ctrl state */
# define DEBUG_REG_TX_RD_IDLE      0x00000000 /*         idle */
# define DEBUG_REG_TX_RD_WAIT      0x00100000 /*         waiting for status */
# define DEBUG_REG_TX_RD_PASUE     0x00200000 /*         generating pause */
# define DEBUG_REG_TX_RD_WRTG      0x00300000 /*         wr stat flush fifo */

# define DEBUG_REG_TX_PAUSE        0x00080000 /* Tx in pause condition */

# define DEBUG_REG_TX_CTRL_STATE   0x00060000 /* Tx frame controller state */
# define DEBUG_REG_TX_CTRL_IDLE    0x00090000 /*         idle */
# define DEBUG_REG_TX_CTRL_WAIT    0x00020000 /*         waiting for status*/
# define DEBUG_REG_TX_CTRL_PAUSE   0x00040000 /*         generating pause */
# define DEBUG_REG_TX_CTRL_XFER    0x00060000 /*         transferring input */

# define DEBUG_REG_TX_ACTIVE       0x00010000 /* Tx actively transmitting */
# define DEBUG_REG_RSVD3           0x0000fc00 /* */

# define DEBUG_REG_RX_STATE        0x00000300 /* Rx fifo state */
# define DEBUG_REG_RX_EMPTY        0x00000000 /*         empty */
# define DEBUG_REG_RX_LOW          0x00000100 /*         below threshold */
# define DEBUG_REG_RX_HIGH         0x00000200 /*         above threshold */
# define DEBUG_REG_RX_FULL         0x00000300 /*         full */

# define DEBUG_REG_RSVD4           0x00000080 /* */

# define DEBUG_REG_RX_RD_STATE     0x00000060 /* Rx rd ctrl state */
# define DEBUG_REG_RX_RD_IDLE      0x00000000 /*         idle */
# define DEBUG_REG_RX_RD_RDG_FR    0x00000020 /*         reading frame data */
# define DEBUG_REG_RX_RD_RDG_STA   0x00000040 /*         reading status */
# define DEBUG_REG_RX_RD_FLUSH     0x00000060 /*         flush fr data/stat */

# define DEBUG_REG_RX_ACTIVE       0x00000010 /* Rx wr ctlr active */

# define DEBUG_REG_RSVD5           0x00000008 /* */
# define DEBUG_REG_SM_FIFO_RW_STA  0x00000006 /* small fifo rd/wr state */
# define DEBUG_REG_RX_RECVG        0x00000001 /* Rx actively receiving data */

# define REM_WAKEUP_FR_REG         0x0028
# define PMT_CTRL_STAT_REG         0x002c
/*   0x0030-0x0034 reserved */

# define INTRP_STATUS_REG          0x0038

# define INTRP_STATUS_REG_RSVD1    0x0000fc00 /* */
# define INTRP_STATUS_REG_TSI      0x00000200 /* time stamp int stat */
# define INTRP_STATUS_REG_RSVD2    0x00000100 /* */

# define INTRP_STATUS_REG_RCOI     0x00000080 /* rec checksum offload int */
# define INTRP_STATUS_REG_TI       0x00000040 /* tx int stat */
# define INTRP_STATUS_REG_RI       0x00000020 /* rx int stat */
# define INTRP_STATUS_REG_NI       0x00000010 /* normal int summary */

# define INTRP_STATUS_REG_PMTI     0x00000008 /* PMT int */
# define INTRP_STATUS_REG_ANC      0x00000004 /* auto negotiation complete */
# define INTRP_STATUS_REG_LSC      0x00000002 /* link status change */
# define INTRP_STATUS_REG_MII      0x00000001 /* rgMii/sgMii int */

# define INTRP_MASK_REG            0x003c

# define INTRP_MASK_REG_RSVD1      0xfc00     /* */
# define INTRP_MASK_REG_TSIM       0x0200     /* time stamp int mask */
# define INTRP_MASK_REG_RSVD2      0x01f0     /* */

# define INTRP_MASK_REG_PMTIM      0x0000     /* PMT int mask */
# define INTRP_MASK_REG_ANCM       0x0000     /* auto negotiation compl mask */
# define INTRP_MASK_REG_LSCM       0x0000     /* link status change mask */
# define INTRP_MASK_REG_MIIM       0x0000     /* rgMii/sgMii int mask */

# define MAC_ADR_0_HIGH_REG        0x0040
# define MAC_ADR_0_LOW_REG         0x0044
/* additional pairs of registers for MAC addresses 1-15 */

# define AN_CONTROL_REG            0x00c0

# define AN_CONTROL_REG_RSVRD1     0xfff80000 /* */
# define AN_CONTROL_REG_SGM_RAL    0x00040000 /* sgmii ral control */
# define AN_CONTROL_REG_LR         0x00020000 /* lock to reference */
# define AN_CONTROL_REG_ECD        0x00010000 /* enable comma detect */

# define AN_CONTROL_REG_RSVRD2     0x00008000 /* */
# define AN_CONTROL_REG_ELE        0x00004000 /* external loopback enable */
# define AN_CONTROL_REG_RSVRD3     0x00002000 /* */
# define AN_CONTROL_REG_ANE        0x00001000 /* auto negotiation enable */

# define AN_CONTROL_REG_RSRVD4     0x00000c00 /* */
# define AN_CONTROL_REG_RAN        0x00000200 /* restart auto negotiation */
# define AN_CONTROL_REG_RSVRD5     0x000001ff /* */

# define AN_STATUS_REG             0x00c4

# define AN_STATUS_REG_RSVRD1      0xfffffe00 /* */
# define AN_STATUS_REG_ES          0x00000100 /* extended status */
# define AN_STATUS_REG_RSVRD2      0x000000c0 /* */
# define AN_STATUS_REG_ANC         0x00000020 /* auto-negotiation complete */
# define AN_STATUS_REG_RSVRD3      0x00000010 /* */
# define AN_STATUS_REG_ANA         0x00000008 /* auto-negotiation ability */
# define AN_STATUS_REG_LS          0x00000004 /* link status */
# define AN_STATUS_REG_RSVRD4      0x00000003 /* */

# define AN_ADVERTISE_REG          0x00c8
# define AN_LNK_PRTNR_ABIL_REG     0x00cc
# define AN_EXPANDSION_REG         0x00d0
# define TBI_EXT_STATUS_REG        0x00d4

# define SG_RG_SMII_STATUS_REG     0x00d8

# define LINK_STATUS_REG           0x00d8

# define LINK_STATUS_REG_RSVRD1    0xffffffc0 /* */
# define LINK_STATUS_REG_FCD       0x00000020 /* false carrier detect */
# define LINK_STATUS_REG_JT        0x00000010 /* jabber timeout */
# define LINK_STATUS_REG_UP        0x00000008 /* link status */

# define LINK_STATUS_REG_SPD       0x00000006 /* link speed */
# define LINK_STATUS_REG_SPD_2_5   0x00000000 /* 10M   2.5M * 4 */
# define LINK_STATUS_REG_SPD_25    0x00000002 /* 100M   25M * 4 */
# define LINK_STATUS_REG_SPD_125   0x00000004 /* 1G    125M * 8 */

# define LINK_STATUS_REG_F_DUPLEX  0x00000001 /* full duplex */

/*     0x00dc-0x00fc reserved */

/* MMC Register Map is from     0x0100-0x02fc */
# define MMC_CNTRL_REG             0x0100
# define MMC_INTR_RX_REG           0x0104
# define MMC_INTR_TX_REG           0x0108
# define MMC_INTR_MASK_RX_REG      0x010C
# define MMC_INTR_MASK_TX_REG      0x0110

/*     0x0300-0x06fc reserved */

/* precision time protocol   time stamp registers */

# define TS_CTL_REG                 0x0700

# define TS_CTL_ATSFC               0x00080000
# define TS_CTL_TSENMAC             0x00040000

# define TS_CTL_TSCLKTYPE           0x00030000
# define TS_CTL_TSCLK_ORD           0x00000000
# define TS_CTL_TSCLK_BND           0x00010000
# define TS_CTL_TSCLK_ETE           0x00020000
# define TS_CTL_TSCLK_PTP           0x00030000

# define TS_CTL_TSMSTRENA           0x00008000
# define TS_CTL_TSEVNTENA           0x00004000
# define TS_CTL_TSIPV4ENA           0x00002000
# define TS_CTL_TSIPV6ENA           0x00001000

# define TS_CTL_TSIPENA             0x00000800
# define TS_CTL_TSVER2ENA           0x00000400
# define TS_CTL_TSCTRLSSR           0x00000200
# define TS_CTL_TSENALL             0x00000100

# define TS_CTL_TSADDREG            0x00000020
# define TS_CTL_TSTRIG              0x00000010

# define TS_CTL_TSUPDT              0x00000008
# define TS_CTL_TSINIT              0x00000004
# define TS_CTL_TSCFUPDT            0x00000002
# define TS_CTL_TSENA               0x00000001


# define TS_SUB_SEC_INCR_REG        0x0704
# define TS_HIGH_REG                0x0708
# define TS_LOW_REG                 0x070c
# define TS_HI_UPDT_REG             0x0710
# define TS_LO_UPDT_REG             0x0714
# define TS_APPEND_REG              0x0718
# define TS_TARG_TIME_HIGH_REG      0x071c
# define TS_TARG_TIME_LOW_REG       0x0720
# define TS_HIGHER_WD_REG           0x0724
# define TS_STATUS_REG              0x072c

/*     0x0730-0x07fc reserved */

# define MAC_ADR16_HIGH_REG        0x0800
# define MAC_ADR16_LOW_REG         0x0804
/* additional pairs of registers for MAC addresses 17-31 */

# define MAC_ADR_MAX             32


# define  QFEC_INTRP_SETUP               (INTRP_EN_REG_AIE    \
					| INTRP_EN_REG_FBE \
					| INTRP_EN_REG_RWE \
					| INTRP_EN_REG_RSE \
					| INTRP_EN_REG_RUE \
					| INTRP_EN_REG_UNE \
					| INTRP_EN_REG_OVE \
					| INTRP_EN_REG_TJE \
					| INTRP_EN_REG_TSE \
					| INTRP_EN_REG_NIE \
					| INTRP_EN_REG_RIE \
					| INTRP_EN_REG_TIE)

/*
 * ASIC Ethernet clock register definitions:
 *     address offsets and some register definitions
 */

# define EMAC_CLK_REG_BASE           0x94020000

/*
 * PHY clock PLL register locations
 */
# define ETH_MD_REG                  0x02A4
# define ETH_NS_REG                  0x02A8

/* definitions of NS_REG control bits
 */
# define ETH_NS_SRC_SEL              0x0007

# define ETH_NS_PRE_DIV_MSK          0x0018
# define ETH_NS_PRE_DIV(x)           (ETH_NS_PRE_DIV_MSK & (x << 3))

# define ETH_NS_MCNTR_MODE_MSK       0x0060
# define ETH_NS_MCNTR_MODE_BYPASS    0x0000
# define ETH_NS_MCNTR_MODE_SWALLOW   0x0020
# define ETH_NS_MCNTR_MODE_DUAL      0x0040
# define ETH_NS_MCNTR_MODE_SINGLE    0x0060

# define ETH_NS_MCNTR_RST            0x0080
# define ETH_NS_MCNTR_EN             0x0100

# define EMAC_PTP_NS_CLK_EN          0x0200
# define EMAC_PTP_NS_CLK_INV         0x0400
# define EMAC_PTP_NS_ROOT_EN         0x0800

/* clock sources
 */
# define CLK_SRC_TCXO                0x0
# define CLK_SRC_PLL_GLOBAL          0x1
# define CLK_SRC_PLL_ARM             0x2
# define CLK_SRC_PLL_QDSP6           0x3
# define CLK_SRC_PLL_EMAC            0x4
# define CLK_SRC_EXT_CLK2            0x5
# define CLK_SRC_EXT_CLK1            0x6
# define CLK_SRC_CORE_TEST           0x7

# define ETH_MD_M(x)                 (x << 16)
# define ETH_MD_2D_N(x)              ((~(x) & 0xffff))
# define ETH_NS_NM(x)                ((~(x) << 16) & 0xffff0000)

/*
 * PHY interface clock divider
 */
# define ETH_X_EN_NS_REG             0x02AC

# define ETH_RX_CLK_FB_INV           0x80
# define ETH_RX_CLK_FB_EN            0x40
# define ETH_TX_CLK_FB_INV           0x20
# define ETH_TX_CLK_FB_EN            0x10
# define ETH_RX_CLK_INV              0x08
# define ETH_RX_CLK_EN               0x04
# define ETH_TX_CLK_INV              0x02
# define ETH_TX_CLK_EN               0x01

# define ETH_X_EN_NS_DEFAULT \
	(ETH_RX_CLK_FB_EN | ETH_TX_CLK_FB_EN | ETH_RX_CLK_EN | ETH_TX_CLK_EN)

# define EMAC_PTP_MD_REG             0x02B0

/* PTP clock divider
 */
# define EMAC_PTP_NS_REG             0x02B4

/*
 * clock interface pin controls
 */
# define EMAC_NS_REG                 0x02B8

# define EMAC_RX_180_CLK_INV         0x2000
# define EMAC_RX_180_CLK_EN          0x1000
# define EMAC_RX_180_CLK_EN_INV      (EMAC_RX_180_CLK_INV | EMAC_RX_180_CLK_EN)

# define EMAC_TX_180_CLK_INV         0x0800
# define EMAC_TX_180_CLK_EN          0x0400
# define EMAC_TX_180_CLK_EN_INV      (EMAC_TX_180_CLK_INV | EMAC_TX_180_CLK_EN)

# define EMAC_REVMII_RX_CLK_INV      0x0200
# define EMAC_REVMII_RX_CLK_EN       0x0100

# define EMAC_RX_CLK_INV             0x0080
# define EMAC_RX_CLK_EN              0x0040

# define EMAC_REVMII_TX_CLK_INV      0x0020
# define EMAC_REVMII_TX_CLK_EN       0x0010

# define EMAC_TX_CLK_INV             0x0008
# define EMAC_TX_CLK_EN              0x0004

# define EMAC_RX_R_CLK_EN            0x0002
# define EMAC_TX_R_CLK_EN            0x0001

# define EMAC_NS_DEFAULT \
	(EMAC_RX_180_CLK_EN_INV | EMAC_TX_180_CLK_EN_INV \
	| EMAC_REVMII_RX_CLK_EN | EMAC_REVMII_TX_CLK_EN \
	| EMAC_RX_CLK_EN | EMAC_TX_CLK_EN \
	| EMAC_RX_R_CLK_EN | EMAC_TX_R_CLK_EN)

/*
 *
 */
# define EMAC_TX_FS_REG              0x02BC
# define EMAC_RX_FS_REG              0x02C0

/*
 * Ethernet controller PHY interface select
 */
# define EMAC_PHY_INTF_SEL_REG       0x18030

# define EMAC_PHY_INTF_SEL_MII       0x0
# define EMAC_PHY_INTF_SEL_RGMII     0x1
# define EMAC_PHY_INTF_SEL_REVMII    0x7
# define EMAC_PHY_INTF_SEL_MASK      0x7

/*
 * MDIO addresses
 */
# define EMAC_PHY_ADDR_REG           0x18034
# define EMAC_REVMII_PHY_ADDR_REG    0x18038

/*
 * clock routing
 */
# define EMAC_CLKMUX_SEL_REG         0x1803c

# define EMAC_CLKMUX_SEL_0           0x1
# define EMAC_CLKMUX_SEL_1           0x2


#endif
