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

#ifndef _SSUSB_HW_REGS_H_
#define _SSUSB_HW_REGS_H_


/* clock setting
   this setting is applied ONLY for DR FPGA
   please check integrator for your platform setting
 */
/* OSC 125MHz/2 = 62.5MHz, ceil(62.5) = 63 */
#define U3D_MAC_SYS_CK 63
/* OSC 20Mhz/2 = 10MHz */
#define U3D_MAC_REF_CK 10
/* U3D_PHY_REF_CK = U3D_MAC_REF_CK on ASIC */
/* On FPGA, these two clocks are separated */
#define U3D_PHY_REF_CK 48


/**
 *  @U3D register map
 */

/*
 * 0x1127_0000 for MAC register
 * relative to MAC base address (USB3_BASE defined in mt_reg_base.h)
 */
/* 4K for each, offset may differ from project to project. Please check integrator */
/* 0x0-0xFFF is for xhci; */
/* refer to 0x1127_1000 (SSUSB_DEV_BASE) */
#define SSUSB_DEV_BASE			(0x0000)
#define SSUSB_EPCTL_CSR_BASE	(0x0800)
#define SSUSB_USB3_MAC_CSR_BASE	(0x1400)
#define SSUSB_USB3_SYS_CSR_BASE	(0x1400)
#define SSUSB_USB2_CSR_BASE		(0x2400)

/*
 * 0x1128_0000 for sifslv register in Infra
 * relative to SIFSLV base address (USB3_SIF_BASE defined in mt_reg_base.h)
 */
#define SSUSB_SIFSLV_IPPC_BASE		(0x700)
/* #define SSUSB_SIFSLV_U2PHY_COM_BASE   (0x800) */
/* #define SSUSB_SIFSLV_U3PHYD_BASE      (0x900) */
/* #define SSUSB_SIFSLV_U2FREQ_BASE      (0xF00) */

/*
 * 0x1129_0000 for sifslv2 register in top_ao
 *  relative to SIFSLV base address (USB3_SIF2_BASE defined in mt_reg_base.h)
 */
#define SSUSB_SIFSLV_U2PHY_COM_BASE	(0x10800)
#define SSUSB_SIFSLV_U3PHYD_BASE	(0x10900)
#define SSUSB_SIFSLV_U2FREQ_BASE	(0x10F00)
#define SSUSB_SIFSLV_U2PHY_COM_SIV_B_BASE	(0x10800)
#define SSUSB_USB30_PHYA_SIV_B_BASE		(0x10B00)
#define SSUSB_SIFSLV_U3PHYA_DA_BASE		(0x10C00)
#define SSUSB_SIFSLV_SPLLC		(0x10000)

/*port1 refs. +0x800(refer to port0)*/
#define SSUSB_PORT1_BASE (0x800)	/*based on port0 */
#define SSUSB_SIFSLV_U2PHY_COM_1P_BASE	(0x11000)
#define SSUSB_SIFSLV_U3PHYD_1P_BASE	(0x11100)
#define SSUSB_SIFSLV_U2FREQ_1P_BASE	(0x11700)
#define SSUSB_SIFSLV_U2PHY_COM_SIV_B_1P_BASE	(0x11100)
#define SSUSB_USB30_PHYA_SIV_B_1P_BASE		(0x11300)
#define SSUSB_SIFSLV_U3PHYA_DA_1P_BASE		(0x11400)


#include "ssusb_dev_c_header.h"
#include "ssusb_epctl_csr_c_header.h"
/* usb3_mac / usb3_sys do not exist in U2 ONLY IP */
#include "ssusb_usb3_mac_csr_c_header.h"
#include "ssusb_usb3_sys_csr_c_header.h"
#include "ssusb_usb2_csr_c_header.h"
#include "ssusb_sifslv_ippc_c_header.h"

#endif				/* USB_HW_H */
