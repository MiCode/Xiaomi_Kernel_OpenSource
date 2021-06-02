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

#ifndef USB_HW_H
#define USB_HW_H

#include <linux/types.h>

#define SW_VERSION "20130627"

/* U3D configuration */

/*This define for DVT OTG testing*/
#ifdef CONFIG_USBIF_COMPLIANCE
#define SUPPORT_OTG
#endif
/* This should be defined if superspeed is supported */
#define SUPPORT_U3
#ifdef SUPPORT_U3

#define U3D_DFT_SPEED SSUSB_SPEED_SUPER
#define U2_U3_SWITCH
/* #define U2_U3_SWITCH_AUTO */
#else
#define U3D_DFT_SPEED SSUSB_SPEED_HIGH
#endif

#ifndef CONFIG_USB_MU3D_DRV
#define POWER_SAVING_MODE
#endif

/* clock setting
 * this setting is applied ONLY for DR FPGA
 * please check integrator for your platform setting
 */
/* OSC 125MHz/2 = 62.5MHz, ceil(62.5) = 63 */
#define U3D_MAC_SYS_CK 63
/* OSC 20Mhz/2 = 10MHz */
/* #define U3D_MAC_REF_CK 10 */
#define U3D_MAC_REF_CK 26
/* U3D_PHY_REF_CK = U3D_MAC_REF_CK on ASIC */
/* On FPGA, these two clocks are separated */
#define U3D_PHY_REF_CK 26


#define PIO_MODE 1
#define DMA_MODE 2
#define QMU_MODE 3
#define BUS_MODE PIO_MODE

#ifdef CONFIG_USBIF_COMPLIANCE
#define EP0_BUS_MODE DMA_MODE
#else
#define EP0_BUS_MODE PIO_MODE
#endif


#define AUTOSET
#define AUTOCLEAR
#define BOUNDARY_4K
#define DIS_ZLP_CHECK_CRC32	/* disable check crc32 in zlp */

#define CS_12B 1
#define CS_16B 2

#if defined(CONFIG_MTK_MD_DIRECT_TETHERING_SUPPORT)
#define CHECKSUM_TYPE CS_12B
#else
#define CHECKSUM_TYPE CS_16B
#endif

#define U3D_COMMAND_TIMER 10

#if (CHECKSUM_TYPE == CS_16B)
#define CHECKSUM_LENGTH 16
#else
#define CHECKSUM_LENGTH 12
#endif

#define NO_ZLP 0
#define HW_MODE 1
#define GPD_MODE 2

#ifdef _USB_NORMAL_
#define TXZLP NO_ZLP
#else
#define TXZLP GPD_MODE
#endif

#define ISO_UPDATE_TEST 0
#define ISO_UPDATE_MODE 1

#define LPM_STRESS 0

/* USBIF , uevent */
/* #define USBIF_OTG_EVENT_DEV_CONN_TMOUT "DEV_CONN_TMOUT" */
/* #define USBIF_OTG_EVENT_NO_RESP_FOR_HNP_ENABLE "NO_RESP_FOR_HNP_ENABLE" */
/* #define USBIF_OTG_EVENT_HUB_NOT_SUPPORTED "HUB_NOT_SUPPORTED" */
/* #define USBIF_OTG_EVENT_DEV_NOT_SUPPORTED "DEV_NOT_SUPPORTED" */
#define USBIF_OTG_EVENT_HNP_FAILED "HNP_FAILED"
#define USBIF_OTG_EVENT_NO_RESP_FOR_SRP "NO_RESP_FOR_SRP"

/*EP number is hard code, not read from U3D_CAP_EPINFO*/
#define HARDCODE_EP

extern void __iomem *u3_base;
extern void __iomem *u3_ippc_base;

#ifdef CONFIG_FPGA_EARLY_PORTING
extern void __iomem *i2c_base;
extern u32 i2c_physical_base;
#endif

/**
 *  @U3D register map
 */

/*
 * 0x1127_0000 for MAC register
 */
/* 4K for each, offset may differ from project to project. Please check integrator */
#define SSUSB_DEV_BASE				(u3_base+0x1000)
#define SSUSB_EPCTL_CSR_BASE		(u3_base+0x1800)
#define SSUSB_USB3_MAC_CSR_BASE	(u3_base+0x2400)
#define SSUSB_USB3_SYS_CSR_BASE	(u3_base+0x2400)
#define SSUSB_USB2_CSR_BASE		(u3_base+0x3400)

/*
 * 0x1128_0000 for sifslv register in Infra
 */
#define SSUSB_SIFSLV_IPPC_BASE		(u3_ippc_base)

#include "ssusb_dev_c_header.h"
#include "ssusb_epctl_csr_c_header.h"
/* usb3_mac / usb3_sys do not exist in U2 ONLY IP */
#include "ssusb_usb3_mac_csr_c_header.h"
#include "ssusb_usb3_sys_csr_c_header.h"
#include "ssusb_usb2_csr_c_header.h"
#include "ssusb_sifslv_ippc_c_header.h"
#include "mtk-phy.h"

#ifdef EXT_VBUS_DET
#define FPGA_REG 0xf0008098
#define VBUS_RISE_BIT (1<<11)	/* W1C */
#define VBUS_FALL_BIT (1<<12)	/* W1C */
#define VBUS_MSK (VBUS_RISE_BIT | VBUS_FALL_BIT)
#define VBUS_RISE_IRQ 13
#define VBUS_FALL_IRQ 14
#endif
#define USB_IRQ 146


#define RISC_SIZE_1B 0x0
#define RISC_SIZE_2B 0x1
#define RISC_SIZE_4B 0x2


#define USB_FIFO(ep_num)				(U3D_FIFO0+ep_num*0x10)

#define USB_FIFOSZ_SIZE_8				(0x03)
#define USB_FIFOSZ_SIZE_16				(0x04)
#define USB_FIFOSZ_SIZE_32				(0x05)
#define USB_FIFOSZ_SIZE_64				(0x06)
#define USB_FIFOSZ_SIZE_128			    (0x07)
#define USB_FIFOSZ_SIZE_256			    (0x08)
#define USB_FIFOSZ_SIZE_512			    (0x09)
#define USB_FIFOSZ_SIZE_1024			(0x0A)
#define USB_FIFOSZ_SIZE_2048			(0x0B)
#define USB_FIFOSZ_SIZE_4096			(0x0C)
#define USB_FIFOSZ_SIZE_8192			(0x0D)
#define USB_FIFOSZ_SIZE_16384			(0x0E)
#define USB_FIFOSZ_SIZE_32768			(0x0F)


/* U3D_EP0CSR */
#define CSR0_SETUPEND					(0x00200000)	/* /removed, use SETUPENDISR */
#define CSR0_FLUSHFIFO					(0x01000000)	/* /removed */
#define CSR0_SERVICESETUPEND			(0x08000000)	/* /removed, W1C SETUPENDISR */
#define EP0_W1C_BITS					(~(EP0_RXPKTRDY | EP0_SETUPPKTRDY | EP0_SENTSTALL))
/* U3D_TX1CSR0 */
#define USB_TXCSR_FLUSHFIFO				(0x00100000)	/* removed */
#define TX_W1C_BITS				(~(TX_SENTSTALL))
/* USB_RXCSR */
#define USB_RXCSR_FLUSHFIFO			(0x00100000)	/* removed */
#define RX_W1C_BITS				(~(RX_SENTSTALL|RX_RXPKTRDY))


#define BIT0 (1<<0)
#define BIT16 (1<<16)

#define TYPE_BULK						(0x00)
#define TYPE_INT						(0x10)
#define TYPE_ISO						(0x20)
#define TYPE_MASK						(0x30)


/* QMU macros */
#define USB_QMU_RQCSR(n)				(U3D_RXQCSR1+0x0010*((n)-1))
#define USB_QMU_RQSAR(n)			(U3D_RXQSAR1+0x0010*((n)-1))
#define USB_QMU_RQCPR(n)				(U3D_RXQCPR1+0x0010*((n)-1))
#define USB_QMU_RQLDPR(n)				(U3D_RXQLDPR1+0x0010*((n)-1))
#define USB_QMU_TQCSR(n)				(U3D_TXQCSR1+0x0010*((n)-1))
#define USB_QMU_TQSAR(n)				(U3D_TXQSAR1+0x0010*((n)-1))
#define USB_QMU_TQCPR(n)				(U3D_TXQCPR1+0x0010*((n)-1))

#define QMU_Q_START					(0x00000001)
#define QMU_Q_RESUME					(0x00000002)
#define QMU_Q_STOP					(0x00000004)
#define QMU_Q_ACTIVE					(0x00008000)

#define QMU_TX_EN(n)					(BIT0<<(n))
#define QMU_RX_EN(n)					(BIT16<<(n))
#define QMU_TX_CS_EN(n)				(BIT0<<(n))
#define QMU_RX_CS_EN(n)				(BIT16<<(n))
#define QMU_TX_ZLP(n)					(BIT0<<(n))
#define QMU_RX_MULTIPLE(n)				(BIT16<<((n)-1))
#define QMU_RX_ZLP(n)					(BIT0<<(n))
#define QMU_RX_COZ(n)					(BIT16<<(n))

#define QMU_RX_EMPTY(n)				(BIT16<<(n))
#define QMU_TX_EMPTY(n)				(BIT0<<(n))
#define QMU_RX_DONE(n)				(BIT16<<(n))
#define QMU_TX_DONE(n)				(BIT0<<(n))

#define QMU_RX_ZLP_ERR(n)			(BIT16<<(n))
#define QMU_RX_EP_ERR(n)				(BIT0<<(n))
#define QMU_RX_LEN_ERR(n)			(BIT16<<(n))
#define QMU_RX_CS_ERR(n)				(BIT0<<(n))

#define QMU_TX_LEN_ERR(n)			(BIT16<<(n))
#define QMU_TX_CS_ERR(n)				(BIT0<<(n))

/**
 *  @MAC value Definition
 */

/* U3D_LINK_STATE_MACHINE */
#define	STATE_RESET						(0)
#define	STATE_DISABLE					(1)
#define	STATE_DISABLE_EXIT				(2)
#define	STATE_SS_INACTIVE_QUITE			(3)
#define	STATE_SS_INACTIVE_DISC_DETECT	(4)
#define	STATE_RX_DETECT_RESET			(5)
#define	STATE_RX_DETECT_ACTIVE			(6)
#define	STATE_RX_DETECT_QUITE			(7)
#define	STATE_POLLING_LFPS				(8)
#define	STATE_POLLING_RXEQ				(9)
#define	STATE_POLLING_ACTIVE			(10)
#define	STATE_POLLING_CONFIGURATION		(11)
#define	STATE_POLLING_IDLE				(12)
#define	STATE_U0_STATE					(13)
#define	STATE_U1_STATE					(14)
#define	STATE_U1_TX_PING				(15)
#define	STATE_U1_EXIT					(16)
#define	STATE_U2_STATE					(17)
#define	STATE_U2_DETECT					(18)
#define	STATE_U2_EXIT					(19)
#define	STATE_U3_STATE					(20)
#define	STATE_U3_DETECT					(21)
#define	STATE_U3_EXIT					(22)
#define	STATE_COMPLIANCE				(23)
#define	STATE_RECOVERY_ACTIVE			(24)
#define	STATE_RECOVERY_CONFIGURATION	(25)
#define	STATE_RECOVERY_IDLE				(26)
#define	STATE_LOOPBACK_ACTIVE_MASTER	(27)
#define	STATE_LOOPBACK_ACTIVE_SLAVE		(28)

/* TODO: remove these definitions */
#if 1
/* DEVICE_CONTROL */
#define USB_DEVCTL_SESSION  (0x1)
#define USB_DEVCTL_HOSTREQUEST (0x2)
#define USB_DEVCTL_HOSTMODE (0x4)
#define USB_DEVCTL_LS_DEV (0x5)
#define USB_DEVCTL_FS_DEV (0x6)
#define USB_DEVCTL_BDEVICE (0x80)
#define USB_DEVCTL_VBUSMASK (0x18)
#define USB_DEVCTL_VBUSVALID (0x18)
#define USB_DEVCTL_VBUS_OFFSET (0x3)
#endif

#endif				/* USB_HW_H */
