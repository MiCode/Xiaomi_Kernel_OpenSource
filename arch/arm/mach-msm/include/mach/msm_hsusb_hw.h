 /*
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2008-2009, The Linux Foundation. All rights reserved.
 * Author: Brian Swetland <swetland@google.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __LINUX_USB_GADGET_MSM72K_UDC_H__
#define __LINUX_USB_GADGET_MSM72K_UDC_H__

#define USB_ID               (MSM_USB_BASE + 0x0000)
#define USB_HWGENERAL        (MSM_USB_BASE + 0x0004)
#define USB_HWHOST           (MSM_USB_BASE + 0x0008)
#define USB_HWDEVICE         (MSM_USB_BASE + 0x000C)
#define USB_HWTXBUF          (MSM_USB_BASE + 0x0010)
#define USB_HWRXBUF          (MSM_USB_BASE + 0x0014)
#define USB_AHB_BURST        (MSM_USB_BASE + 0x0090)
#define USB_AHB_MODE         (MSM_USB_BASE + 0x0098)
#define USB_GEN_CONFIG       (MSM_USB_BASE + 0x009C)
#define USB_BAM_DISABLE      (1 << 13)
#define USB_ROC_AHB_MODE     (MSM_USB_BASE + 0x0090)
#define USB_SBUSCFG          (MSM_USB_BASE + 0x0090)

#define USB_CAPLENGTH        (MSM_USB_BASE + 0x0100) /* 8 bit */
#define USB_HCIVERSION       (MSM_USB_BASE + 0x0102) /* 16 bit */
#define USB_HCSPARAMS        (MSM_USB_BASE + 0x0104)
#define USB_HCCPARAMS        (MSM_USB_BASE + 0x0108)
#define USB_DCIVERSION       (MSM_USB_BASE + 0x0120) /* 16 bit */
#define USB_USBCMD           (MSM_USB_BASE + 0x0140)
#define USB_USBSTS           (MSM_USB_BASE + 0x0144)
#define USB_USBINTR          (MSM_USB_BASE + 0x0148)
#define USB_FRINDEX          (MSM_USB_BASE + 0x014C)
#define USB_DEVICEADDR       (MSM_USB_BASE + 0x0154)
#define USB_ENDPOINTLISTADDR (MSM_USB_BASE + 0x0158)
#define USB_BURSTSIZE        (MSM_USB_BASE + 0x0160)
#define USB_TXFILLTUNING     (MSM_USB_BASE + 0x0164)
#define USB_ULPI_VIEWPORT    (MSM_USB_BASE + 0x0170)
#define USB_ENDPTNAK         (MSM_USB_BASE + 0x0178)
#define USB_ENDPTNAKEN       (MSM_USB_BASE + 0x017C)
#define USB_PORTSC           (MSM_USB_BASE + 0x0184)
#define USB_OTGSC            (MSM_USB_BASE + 0x01A4)
#define USB_USBMODE          (MSM_USB_BASE + 0x01A8)
#define USB_ENDPTSETUPSTAT   (MSM_USB_BASE + 0x01AC)
#define USB_ENDPTPRIME       (MSM_USB_BASE + 0x01B0)
#define USB_ENDPTFLUSH       (MSM_USB_BASE + 0x01B4)
#define USB_ENDPTSTAT        (MSM_USB_BASE + 0x01B8)
#define USB_ENDPTCOMPLETE    (MSM_USB_BASE + 0x01BC)
#define USB_ENDPTCTRL(n)     (MSM_USB_BASE + 0x01C0 + (4 * (n)))


#define USBCMD_RESET   2
#define USBCMD_ATTACH  1
#define USBCMD_RS	(1 << 0) /* run/stop bit */
#define USBCMD_ATDTW   (1 << 14)
#define USBCMD_ITC(n)	(n << 16)
#define USBCMD_ITC_MASK (0xFF << 16)
#define ASYNC_INTR_CTRL	(1 << 29)
#define ULPI_STP_CTRL	(1 << 30)

#define USBMODE_DEVICE 2
#define USBMODE_HOST   3
#define USBMODE_VBUS	(1 << 5)	/* vbus power select */

/* Redefining SDIS bit as it defined incorrectly in ehci.h. */
#ifdef USBMODE_SDIS
#undef USBMODE_SDIS
#endif
#define USBMODE_SDIS	(1 << 4)	/* stream disable */

struct ept_queue_head {
    unsigned config;
    unsigned active; /* read-only */

    unsigned next;
    unsigned info;
    unsigned page0;
    unsigned page1;
    unsigned page2;
    unsigned page3;
    unsigned page4;
    unsigned reserved_0;

    unsigned char setup_data[8];

    unsigned reserved_1;
    unsigned reserved_2;
    unsigned reserved_3;
    unsigned reserved_4;
};

#define CONFIG_MAX_PKT(n)     ((n) << 16)
#define CONFIG_ZLT            (1 << 29)    /* stop on zero-len xfer */
#define CONFIG_IOS            (1 << 15)    /* IRQ on setup */
#define CONFIG_MULT           (3 << 30)
#define CONFIG_MULT_SHIFT     11

struct ept_queue_item {
    unsigned next;
    unsigned info;
    unsigned page0;
    unsigned page1;
    unsigned page2;
    unsigned page3;
    unsigned page4;
    unsigned reserved;
};

#define TERMINATE 1

#define INFO_BYTES(n)         ((n) << 16)
#define INFO_IOC              (1 << 15)
#define INFO_ACTIVE           (1 << 7)
#define INFO_HALTED           (1 << 6)
#define INFO_BUFFER_ERROR     (1 << 5)
#define INFO_TXN_ERROR        (1 << 3)


#define STS_NAKI              (1 << 16)  /* */
#define STS_SLI               (1 << 8)   /* R/WC - suspend state entered */
#define STS_SRI               (1 << 7)   /* R/WC - SOF recv'd */
#define STS_URI               (1 << 6)   /* R/WC - RESET recv'd */
#define STS_FRI               (1 << 3)   /* R/WC - Frame List Rollover */
#define STS_PCI               (1 << 2)   /* R/WC - Port Change Detect */
#define STS_UEI               (1 << 1)   /* R/WC - USB Error */
#define STS_UI                (1 << 0)   /* R/WC - USB Transaction Complete */


/* bits used in all the endpoint status registers */
#define EPT_TX(n) (1 << ((n) + 16))
#define EPT_RX(n) (1 << (n))


#define CTRL_TXE              (1 << 23)
#define CTRL_TXR              (1 << 22)
#define CTRL_TXI              (1 << 21)
#define CTRL_TXD              (1 << 17)
#define CTRL_TXS              (1 << 16)
#define CTRL_RXE              (1 << 7)
#define CTRL_RXR              (1 << 6)
#define CTRL_RXI              (1 << 5)
#define CTRL_RXD              (1 << 1)
#define CTRL_RXS              (1 << 0)

#define CTRL_TXT_MASK         (3 << 18)
#define CTRL_TXT_CTRL         (0 << 18)
#define CTRL_TXT_ISOCH        (1 << 18)
#define CTRL_TXT_BULK         (2 << 18)
#define CTRL_TXT_INT          (3 << 18)
#define CTRL_TXT_EP_TYPE_SHIFT 18

#define CTRL_RXT_MASK         (3 << 2)
#define CTRL_RXT_CTRL         (0 << 2)
#define CTRL_RXT_ISOCH        (1 << 2)
#define CTRL_RXT_BULK         (2 << 2)
#define CTRL_RXT_INT          (3 << 2)
#define CTRL_RXT_EP_TYPE_SHIFT 2

#define ULPI_CONFIG_REG		0x31
#if (defined(CONFIG_ARCH_MSM7X27) && !defined(CONFIG_ARCH_MSM7X27A)) \
					|| defined(CONFIG_ARCH_QSD8X50)
#define ULPI_DIGOUT_CTRL	0X31
#define ULPI_CDR_AUTORESET	(1 << 5)
#else
#define ULPI_DIGOUT_CTRL	0X36
#define ULPI_CDR_AUTORESET	(1 << 1)
#endif
#define ULPI_SE1_GATE		(1 << 2)
#define ULPI_CONFIG_REG1	0x30
#define ULPI_CONFIG_REG2	0X31
#define ULPI_CONFIG_REG3	0X32
#define ULPI_IFC_CTRL_CLR	0x09
#define ULPI_AMPLITUDE_MAX	0x0C
#define ULPI_OTG_CTRL		0x0B
#define ULPI_OTG_CTRL_CLR       0x0C
#define ULPI_INT_RISE_CLR       0x0F
#define ULPI_INT_FALL_CLR       0x12
#define ULPI_PRE_EMPHASIS_MASK	(3 << 4)
#define ULPI_HSDRVSLOPE_MASK	(0x0F)
#define ULPI_DRV_AMPL_MASK	(3 << 2)
#define ULPI_ONCLOCK	       (1 << 6)
#define ULPI_IDPU	      (1 << 0)
#define ULPI_HOST_DISCONNECT  (1 << 0)
#define ULPI_VBUS_VALID       (1 << 1)
#define ULPI_SESS_END         (1 << 3)
#define ULPI_ID_GND  	      (1 << 4)
#define ULPI_WAKEUP           (1 << 31)
#define ULPI_RUN              (1 << 30)
#define ULPI_WRITE            (1 << 29)
#define ULPI_READ             (0 << 29)
#define ULPI_STATE_NORMAL     (1 << 27)
#define ULPI_ADDR(n)          (((n) & 255) << 16)
#define ULPI_DATA(n)          ((n) & 255)
#define ULPI_DATA_READ(n)     (((n) >> 8) & 255)

/* USB_PORTSC bits for determining port speed */
#define PORTSC_PSPD_FS        (0 << 26)
#define PORTSC_PSPD_LS        (1 << 26)
#define PORTSC_PSPD_HS        (2 << 26)
#define PORTSC_PSPD_MASK      (3 << 26)


#define OTGSC_BSVIE            (1 << 27) /* R/W - BSV Interrupt Enable */
#define OTGSC_DPIE             (1 << 30) /* R/W - DataPulse Interrupt Enable */
#define OTGSC_1MSE             (1 << 29) /* R/W - 1ms Interrupt Enable */
#define OTGSC_BSEIE            (1 << 28) /* R/W - BSE Interrupt Enable */
#define OTGSC_ASVIE            (1 << 26) /* R/W - ASV Interrupt Enable */
#define OTGSC_ASEIE            (1 << 25) /* R/W - ASE Interrupt Enable */
#define OTGSC_IDIE             (1 << 24) /* R/W - ID Interrupt Enable */
#define OTGSC_BSVIS            (1 << 19) /* R/W - BSV Interrupt Status */
#define OTGSC_IDPU	       (1 << 5)
#define OTGSC_ID               (1 << 8)
#define OTGSC_IDIS             (1 << 16)
#define B_SESSION_VALID        (1 << 11)
#define OTGSC_INTR_MASK        (OTGSC_BSVIE | OTGSC_DPIE | OTGSC_1MSE | \
				OTGSC_BSEIE | OTGSC_ASVIE | OTGSC_ASEIE | \
				OTGSC_IDIE)
#define OTGSC_INTR_STS_MASK    (0x7f << 16)
#define CURRENT_CONNECT_STATUS (1 << 0)

#define PORTSC_FPR             (1 << 6)  /* R/W - State normal => suspend */
#define PORTSC_SUSP            (1 << 7)  /* Read - Port in suspend state */
#define PORTSC_LS              (3 << 10) /* Read - Port's Line status */
#define PORTSC_PHCD	       (1 << 23) /* phy suspend mode */
#define PORTSC_CCS	       (1 << 0)  /* current connect status */
#define PORTSC_PORT_RESET      0x00000100
#define PORTSC_PTS		(3 << 30)
#define PORTSC_PTS_ULPI		(2 << 30)
#define PORTSC_PTS_SERIAL	(3 << 30)

#define PORTSC_PORT_SPEED_FULL    0x00000000
#define PORTSC_PORT_SPEED_LOW     0x04000000
#define PORTSC_PORT_SPEED_HIGH    0x08000000
#define PORTSC_PORT_SPEED_MASK    0x0c000000

#define SBUSCFG_AHBBRST_INCR4	0x01
#define ULPI_USBINTR_ENABLE_RASING_C  0x0F
#define ULPI_USBINTR_ENABLE_FALLING_C 0x12
#define ULPI_USBINTR_STATUS           0x13
#define ULPI_USBINTR_ENABLE_RASING_S  0x0E
#define ULPI_USBINTR_ENABLE_FALLING_S 0x11
#define ULPI_SESSION_END_RAISE        (1 << 3)
#define ULPI_SESSION_END_FALL         (1 << 3)
#define ULPI_SESSION_VALID_RAISE      (1 << 2)
#define ULPI_SESSION_VALID_FALL       (1 << 2)
#define ULPI_VBUS_VALID_RAISE         (1 << 1)
#define ULPI_VBUS_VALID_FALL          (1 << 1)

#define ULPI_CHG_DETECT_REG     0x34
/* control charger detection by ULPI or externally */
#define ULPI_EXTCHGCTRL_65NM	(1 << 2)
#define ULPI_EXTCHGCTRL_180NM	(1 << 3)
/* charger detection power on control */
#define ULPI_CHGDETON           (1 << 1)
 /* enable charger detection */
#define ULPI_CHGDETEN           (1 << 0)
#define ULPI_CHGTYPE_65NM	(1 << 3)
#define ULPI_CHGTYPE_180NM	(1 << 4)

/* test mode support */
#define J_TEST			(0x0100)
#define K_TEST			(0x0200)
#define SE0_NAK_TEST		(0x0300)
#define TST_PKT_TEST		(0x0400)
#define PORTSC_PTC		(0xf << 16)
#define PORTSC_PTC_J_STATE	(0x01 << 16)
#define PORTSC_PTC_K_STATE	(0x02 << 16)
#define PORTSC_PTC_SE0_NAK	(0x03 << 16)
#define PORTSC_PTC_TST_PKT	(0x04 << 16)

#define USBH                     (1 << 15)
#define USB_PHY                  (1 << 18)

#define ULPI_DEBUG               0x15
#define ULPI_FUNC_CTRL_CLR       0x06
#define ULPI_SUSPENDM            (1 << 6)
#define ULPI_CLOCK_SUSPENDM     (1 << 3)
#define ULPI_CALIB_STS          (1 << 7)
#define ULPI_CALIB_VAL(x)       (x & 0x7C)
#endif /* __LINUX_USB_GADGET_MSM72K_UDC_H__ */
