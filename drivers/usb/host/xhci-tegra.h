/*
 * xhci-tegra.h - Nvidia xHCI host controller related data
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __XUSB_H
#define __XUSB_H

/* PADCTL BITS */
#define USB2_OTG_PAD_PORT_MASK(x) (0x3 << (2 * x))
#define USB2_OTG_PAD_PORT_OWNER_XUSB(x) (0x1 << (2 * x))
#define USB2_PORT_CAP_MASK(x) (0x3 << (4 * x))
#define USB2_PORT_CAP_HOST(x) (0x1 << (4 * x))
#define USB2_ULPI_PAD	(0x1 << 12)
#define USB2_ULPI_PAD_OWNER_XUSB	(0x1 << 12)
#define USB2_HSIC_PAD_P0_OWNER_XUSB	(0x1 << 14)
#define USB2_HSIC_PAD_P1_OWNER_XUSB	(0x1 << 15)
#define USB2_ULPI_PORT_CAP	(0x1 << 24)
#define SS_PORT_MAP_P0	(0x7 << 0)
#define SS_PORT_MAP_P1	(0x7 << 4)
#define SS_PORT_MAP_P0_USB2_PORT0	(0x0 << 0)
#define SS_PORT_MAP_P0_USB2_PORT1	(0x1 << 0)
#define USB2_OTG_HS_CURR_LVL (0x3F << 0)
#define USB2_OTG_HS_SLEW (0x3F << 6)
#define USB2_OTG_FS_SLEW (0x3 << 12)
#define USB2_OTG_LS_RSLEW (0x3 << 14)
#define USB2_OTG_LS_FSLEW (0x3 << 16)
#define USB2_OTG_PD (0x1 << 19)
#define USB2_OTG_PD2 (0x1 << 20)
#define USB2_OTG_PD_ZI (0x1 << 21)
#define USB2_OTG_PD_CHRP_FORCE_POWERUP (0x1 << 0)
#define USB2_OTG_PD_DISC_FORCE_POWERUP (0x1 << 1)
#define USB2_OTG_PD_DR (0x1 << 2)
#define USB2_OTG_TERM_RANGE_AD (0xF << 3)
#define USB2_OTG_HS_IREF_CAP (0x3 << 9)
#define USB2_BIAS_HS_SQUELCH_LEVEL (0x3 << 0)
#define USB2_BIAS_HS_DISCON_LEVEL (0x7 << 2)
#define HSIC_TX_SLEWP (0xF << 8)
#define HSIC_TX_SLEWN (0xF << 12)
#define IOPHY_USB3_RXWANDER (0xF << 4)
#define IOPHY_USB3_RXEQ (0xFFFF << 8)
#define IOPHY_USB3_CDRCNTL (0xFF << 24)
#define SNPS_OC_MAP_CTRL1 (0x7 << 0)
#define SNPS_OC_MAP_CTRL2 (0x7 << 3)
#define SNPS_OC_MAP_CTRL3 (0x7 << 6)
#define SNPS_CTRL1_OC_DETECTED_VBUS_PAD0 (0x4 << 0)
#define OC_DET_VBUS_ENABLE0_OC_MAP (0x7 << 10)
#define OC_DET_VBUS_ENABLE1_OC_MAP (0x7 << 13)
#define OC_DET_VBUS_EN0_OC_DETECTED_VBUS_PAD0 (0x4 << 10)
#define OC_DET_VBUS_EN1_OC_DETECTED_VBUS_PAD1 (0x5 << 13)
#define USB2_OC_MAP_PORT0 (0x7 << 0)
#define USB2_OC_MAP_PORT1 (0x7 << 3)
#define USB2_OC_MAP_PORT0_OC_DETECTED_VBUS_PAD0 (0x4 << 0)
#define USB2_OC_MAP_PORT1_OC_DETECTED_VBUS_PAD1 (0x5 << 3)

#define XUSB_CSB_MP_L2IMEMOP_TRIG				0x00101A14
#define XUSB_CSB_MP_APMAP					0x0010181C
#define XUSB_CSB_ARU_SCRATCH0				0x00100100

/* Nvidia Cfg Registers */

#define XUSB_CFG_1					0x00000004
#define XUSB_CFG_4					0x00000010
#define XUSB_CFG_16					0x00000040
#define XUSB_CFG_24					0x00000060
#define XUSB_CFG_FPCICFG					0x000000F8
#define XUSB_CFG_ARU_C11_CSBRANGE				0x0000041C
#define XUSB_CFG_ARU_SMI_INTR				0x00000428
#define XUSB_CFG_ARU_RST					0x0000042C
#define XUSB_CFG_ARU_SMI_INTR				0x00000428
#define XUSB_CFG_ARU_CONTEXT				0x0000043C
#define XUSB_CFG_ARU_FW_SCRATCH				0x00000440
#define XUSB_CFG_CSB_BASE_ADDR				0x00000800
#define XUSB_CFG_ARU_CONTEXT_HSFS_SPEED			0x00000480
#define XUSB_CFG_ARU_CONTEXT_HS_PLS			0x00000478
#define XUSB_CFG_ARU_CONTEXT_FS_PLS			0x0000047C
#define XUSB_CFG_ARU_CONTEXT_HSFS_SPEED			0x00000480
#define XUSB_CFG_ARU_CONTEXT_HSFS_PP			0x00000484
#define XUSB_CFG_CSB_BASE_ADDR				0x00000800

/* TODO: Do not have the definitions of below
 * registers.
 */

/* BAR0 Registers */
#define BAR0_XHCI_OP_PORTSC(i)                         (0x00000420+(i)*16)
#define BAR0_XHCI_OP_PORTSC_UTMIP_0                    1

/* IPFS Registers to save and restore  */
#define	IPFS_XUSB_HOST_MSI_BAR_SZ_0			0xC0
#define	IPFS_XUSB_HOST_MSI_AXI_BAR_ST_0			0xC4
#define	IPFS_XUSB_HOST_FPCI_BAR_ST_0			0xC8
#define	IPFS_XUSB_HOST_MSI_VEC0_0			0x100
#define	IPFS_XUSB_HOST_MSI_EN_VEC0_0			0x140
#define	IPFS_XUSB_HOST_CONFIGURATION_0			0x180
#define	IPFS_XUSB_HOST_FPCI_ERROR_MASKS_0		0x184
#define	IPFS_XUSB_HOST_INTR_MASK_0			0x188
#define	IPFS_XUSB_HOST_IPFS_INTR_ENABLE_0		0x198
#define	IPFS_XUSB_HOST_UFPCI_CONFIG_0			0x19C
#define	IPFS_XUSB_HOST_CLKGATE_HYSTERESIS_0		0x1BC
#define IPFS_XUSB_HOST_MCCIF_FIFOCTRL_0			0x1DC

/* IPFS bit definitions */
#define IPFS_EN_FPCI					(1 << 0)
#define IPFS_IP_INT_MASK				(1 << 16)

/* Nvidia MailBox Registers */

#define XUSB_CFG_ARU_MBOX_CMD				0xE4
#define XUSB_CFG_ARU_MBOX_DATA_IN				0xE8
#define XUSB_CFG_ARU_MBOX_DATA_OUT			0xEC
#define XUSB_CFG_ARU_MBOX_OWNER				0xF0

/* Nvidia Falcon Registers */
#define XUSB_FALC_CPUCTL					0x00000100
#define XUSB_FALC_BOOTVEC					0x00000104
#define XUSB_FALC_DMACTL					0x0000010C
#define XUSB_FALC_IMFILLRNG1					0x00000154
#define XUSB_FALC_IMFILLCTL					0x00000158
#define XUSB_FALC_CMEMBASE					0x00000160
#define XUSB_FALC_DMEMAPERT					0x00000164
#define XUSB_FALC_IMEMC_START					0x00000180
#define XUSB_FALC_IMEMD_START					0x00000184
#define XUSB_FALC_IMEMT_START					0x00000188
#define XUSB_FALC_ICD_CMD					0x00000200
#define XUSB_FALC_ICD_RDATA					0x0000020C
#define XUSB_FALC_SS_PVTPORTSC1					0x00116000
#define XUSB_FALC_SS_PVTPORTSC2					0x00116004
#define XUSB_FALC_SS_PVTPORTSC3					0x00116008
#define XUSB_FALC_HS_PVTPORTSC1					0x00116800
#define XUSB_FALC_HS_PVTPORTSC2					0x00116804
#define XUSB_FALC_HS_PVTPORTSC3					0x00116808
#define XUSB_FALC_FS_PVTPORTSC1					0x00117000
#define XUSB_FALC_FS_PVTPORTSC2					0x00117004
#define XUSB_FALC_FS_PVTPORTSC3					0x00117008

#define XUSB_FALC_STATE_HALTED					0x00000010
/* Nvidia mailbox constants */
#define MBOX_INT_EN			(1 << 31)
#define MBOX_XHCI_INT_EN	(1 << 30)
#define MBOX_SMI_INT_EN		(1 << 29)
#define MBOX_PME_INT_EN		(1 << 28)
#define MBOX_FALC_INT_EN	(1 << 27)

#define MBOX_OWNER_FW						1
#define MBOX_OWNER_SW						2
#define MBOX_OWNER_ID_MASK					0xFF

#define MBOX_CMD_TYPE_MASK					0xFF000000
#define MBOX_CMD_DATA_MASK					0x00FFFFFF
#define MBOX_CMD_STATUS_MASK				MBOX_CMD_TYPE_MASK
#define MBOX_CMD_RESULT_MASK				MBOX_CMD_DATA_MASK
#define MBOX_CMD_SHIFT						24
#define MBOX_SMI_INTR_EN					(1 << 3)

/* PMC Register */
#define PMC_SCRATCH34						0x124

#define TEGRA_POWERGATE_XUSBA	20
#define TEGRA_POWERGATE_XUSBB	21
#define TEGRA_POWERGATE_XUSBC	22

/* Nvidia Constants */
#define IMEM_BLOCK_SIZE						256

#define MEMAPERT_ENABLE						0x00000010
#define DMEMAPERT_ENABLE_INIT				0x00000000
#define CPUCTL_STARTCPU						0x00000002
#define L2IMEMOP_SIZE_SRC_OFFSET_SHIFT		8
#define L2IMEMOP_SIZE_SRC_OFFSET_MASK		0x3ff
#define L2IMEMOP_SIZE_SRC_COUNT_SHIFT		24
#define L2IMEMOP_SIZE_SRC_COUNT_MASK		0xff
#define L2IMEMOP_TRIG_LOAD_LOCKED_SHIFT	24
#define IMFILLRNG_TAG_MASK			0xffff
#define IMFILLRNG1_TAG_HI_SHIFT		16
#define APMAP_BOOTPATH						(1 << 31)
#define L2IMEM_INVALIDATE_ALL				0x40000000
#define L2IMEM_LOAD_LOCKED_RESULT			(0x11 << 24)
#define FW_SIZE_OFFSET						0x64
#define HSIC_PORT1	0
#define HSIC_PORT0	1
#define ULPI_PORT	2
#define OTG_PORT1	3
#define OTG_PORT2	4

/* Nvidia Host Controller Device and Vendor ID */
#define XUSB_USB_DID		0xE16
#define XUSB_USB_VID		0x10DE

/* Nvidia CSB MP Registers */
#define XUSB_CSB_MP_ILOAD_ATTR		0x00101A00
#define XUSB_CSB_MP_ILOAD_BASE_LO		0x00101A04
#define XUSB_CSB_MP_ILOAD_BASE_HI		0x00101A08
#define XUSB_CSB_MP_L2IMEMOP_SIZE		0x00101A10

/*Nvidia CFG registers */
#define XUSB_CFG_ARU_CONTEXT_HSFS_SPEED	0x00000480
#define XUSB_CFG_ARU_CONTEXT_HS_PLS		0x00000478
#define XUSB_CFG_ARU_CONTEXT_FS_PLS		0x0000047C
#define ARU_CONTEXT_HSFS_PP				0x00000484
#define ARU_ULPI_REGACCESS				0x474
#define ARU_ULPI_REGACCESS_ADDR_MASK	0xff00
#define ARU_ULPI_REGACCESS_CMD_MASK		0x1
#define ARU_ULPI_REGACCESS_DATA_MASK	0xff0000

/* XUSB PAD Ctl Registers START
 * Extracted and massaged from arxusb_padctl.h
 */
#define MBOX_OWNERSHIP_ERR	-1
#define PWR_GATE_ERR		-1
#define YES			1
#define NO			0
#define	SNPS		0
#define	XUSB		1
#define	DISABLED	1

#define BOOT_MEDIA_0			0x0
#define BOOT_MEDIA_ENABLE		(1 << 0)
#define BOOT_PORT(x)			(((x) & 0xf) << 1)

#define USB2_PAD_MUX_0						0x4
#define USB2_OTG_PAD_PORT0(x)		(((x) & 0x3) << 0)
#define USB2_OTG_PAD_PORT1(x)		(((x) & 0x3) << 2)
#define USB2_ULPI_PAD_PORT					(1 << 12)
#define USB2_HSIC_PAD_PORT0					(1 << 14)
#define USB2_HSIC_PAD_PORT1					(1 << 15)
#define FORCE_PCIE_PAD_IDDQ_DISABLE			(1 << 16)
#define FORCE_PCIE_PAD_IDDQ_DISABLE_MASK0	(1 << 17)
#define FORCE_PCIE_PAD_IDDQ_DISABLE_MASK1	(1 << 18)

#define USB2_PORT_CAP_0				0x8
#define PORT0_CAP(x)				(((x) & 0x3) << 0)
#define PORT0_INTERNAL				(1 << 2)
#define PORT0_REVERSE_ID			(1 << 3)
#define PORT1_CAP(x)				(((x) & 0x3) << 4)
#define PORT1_INTERNAL				(1 << 6)
#define PORT1_REVERSE_ID			(1 << 7)
#define ULPI_PORT_CAP				(1 << 24)
#define ULPI_MASTER					0
#define ULPI_PHY					1
#define ULPI_PORT_INTERNAL			(1 << 25)

#define SNPS_OC_MAP_0				0xc
#define CONTROLLER1_OC_PIN(x)		(((x) & 0x7) << 0)
#define CONTROLLER2_OC_PIN(x)		(((x) & 0x7) << 3)
#define CONTROLLER3_OC_PIN(x)		(((x) & 0x7) << 6)

#define USB2_OC_MAP_0				0x10
#define PORT0_OC_PIN(x)				(((x) & 0x7) << 0)
#define PORT1_OC_PIN(x)				(((x) & 0x7) << 3)

#define SS_PORT_MAP_0				0x14
#define PORT0_MAP(x)				(((x) & 0x7) << 0)
#define PORT1_MAP(x)				(((x) & 0x7) << 4)

#define OC_DET_0							0x18
#define SET_OC_DETECTED0					(1 << 0)
#define SET_OC_DETECTED1					(1 << 1)
#define SET_OC_DETECTED2					(1 << 2)
#define SET_OC_DETECTED3					(1 << 3)
#define VBUS_ENABLE0						(1 << 8)
#define VBUS_ENABLE1						(1 << 9)
#define VBUS_ENABLE0_OC_MAP(x)				(((x) & 0x7) << 10)
#define VBUS_ENABLE1_OC_MAP(x)				(((x) & 0x7) << 13)
#define  OC_VBUS_PAD0						(4)
#define  OC_VBUS_PAD1						(5)
#define  OC_DISABLE						(7)
#define OC_DETECTED0						(1 << 16)
#define OC_DETECTED1						(1 << 17)
#define OC_DETECTED2						(1 << 18)
#define OC_DETECTED3						(1 << 19)
#define OC_DETECTED_VBUS_PAD0				(1 << 20)
#define OC_DETECTED_VBUS_PAD1				(1 << 21)
#define OC_DETECTED_INTERRUPT_ENABLE0		(1 << 24)
#define OC_DETECTED_INTERRUPT_ENABLE1		(1 << 25)
#define OC_DETECTED_INTERRUPT_ENABLE2		(1 << 26)
#define OC_DETECTED_INTERRUPT_ENABLE3		(1 << 27)
#define OC_DETECTED_INTERRUPT_ENABLE_VBUSPAD0	(1 << 28)
#define OC_DETECTED_INTERRUPT_ENABLE_VBUSPAD1	(1 << 29)
#define ELPG_PROGRAM_0						0x1c
#define USB2_PORT0_WAKE_INTERRUPT_ENABLE	(1 << 0)
#define USB2_PORT1_WAKE_INTERRUPT_ENABLE	(1 << 1)
#define USB2_HSIC_PORT0_WAKE_INTERRUPT_ENABLE	(1 << 3)
#define USB2_HSIC_PORT1_WAKE_INTERRUPT_ENABLE	(1 << 4)
#define SS_PORT0_WAKE_INTERRUPT_ENABLE	(1 << 6)
#define SS_PORT1_WAKE_INTERRUPT_ENABLE	(1 << 7)
#define USB2_PORT0_WAKEUP_EVENT			(1 << 8)
#define USB2_PORT1_WAKEUP_EVENT			(1 << 9)
#define USB2_HSIC_PORT0_WAKEUP_EVENT	(1 << 11)
#define USB2_HSIC_PORT1_WAKEUP_EVENT	(1 << 12)
#define SS_PORT0_WAKEUP_EVENT		(1 << 14)
#define SS_PORT1_WAKEUP_EVENT		(1 << 15)
#define SSP0_ELPG_CLAMP_EN			(1 << 16)
#define SSP0_ELPG_CLAMP_EN_EARLY	(1 << 17)
#define SSP0_ELPG_VCORE_DOWN		(1 << 18)
#define SSP1_ELPG_CLAMP_EN			(1 << 20)
#define SSP1_ELPG_CLAMP_EN_EARLY	(1 << 21)
#define SSP1_ELPG_VCORE_DOWN		(1 << 22)

#define USB2_BATTERY_CHRG_OTGPAD0_0		0x20
#define USB2_BATTERY_CHRG_OTGPAD1_0		0x24
#define PD_CHG					(1 << 0)
#define VDCD_DET				(1 << 1)
#define VDCD_DET_ST_CHNG		(1 << 2)
#define VDCD_DET_CHNG_INTR_EN	(1 << 3)
#define VDCD_DET_FILTER_EN		(1 << 4)
#define VDAT_DET				(1 << 5)
#define VDAT_DET_ST_CHNG		(1 << 6)
#define VDAT_DET_CHNG_INTR_EN	(1 << 7)
#define VDAT_DET_FILTER_EN		(1 << 8)
#define OP_SINK_EN				(1 << 9)
#define OP_SRC_EN				(1 << 10)
#define ON_SINK_EN				(1 << 11)
#define ON_SRC_EN				(1 << 12)
#define OP_I_SRC_EN				(1 << 13)
#define USBOP_RPD				(1 << 14)
#define USBOP_RPU				(1 << 15)
#define USBON_RPD				(1 << 16)
#define USBON_RPU				(1 << 17)
#define ZIP						(1 << 18)
#define ZIP_ST_CHNG				(1 << 19)
#define ZIP_CHNG_INTR_EN		(1 << 20)
#define ZIP_FILTER_EN			(1 << 21)
#define ZIN						(1 << 22)
#define ZIN_ST_CHNG				(1 << 23)
#define ZIN_CHNG_INTR_EN		(1 << 24)
#define ZIN_FILTER_EN			(1 << 25)
#define DCD_DETECTED			(1 << 26)
#define DCD_INTR_EN				(1 << 27)
#define SRP_DETECT_EN			(1 << 28)
#define SRP_DETECTED			(1 << 29)
#define SRP_INTR_EN				(1 << 30)
#define GENERATE_SRP			(1 << 31)

#define USB2_BATTERY_CHRG_BIASPAD_0				0x28
#define PD_OTG							(1 << 0)
#define OTG_VBUS_SESS_VLD				(1 << 1)
#define OTG_VBUS_SESS_VLD_ST_CHNG		(1 << 2)
#define OTG_VBUS_SESS_VLD_CHNG_INTR_EN	(1 << 3)
#define VBUS_VLD				(1 << 4)
#define VBUS_VLD_ST_CHNG		(1 << 5)
#define VBUS_VLD_CHNG_INTR_EN	(1 << 6)
#define IDDIG					(1 << 8)
#define IDDIG_A					(1 << 9)
#define IDDIG_B					(1 << 10)
#define IDDIG_C					(1 << 11)
#define ID_CONNECT_STATUS		(1 << 12)
#define ID_CONNECT_ST_CHNG		(1 << 13)
#define ID_CONNECT_CHNG_INTR_EN	(1 << 14)
#define VBUS_SOURCE_SELECT(x)	(((x) & 0x3) << 15)
#define VBUS_OVERRIDE			(1 << 17)
#define ID_SOURCE_SELECT(x)		(((x) & 0x3) << 18)
#define ID_OVERRIDE				(1 << 20)

#define USB2_BATTERY_CHRG_TDCD_DBNC_TIMER_0	0x2c
#define TDCD_DBNC(x)			(((x) & 0x7ff) << 0)

#define IOPHY_PLL0_CTL1_0		0x30
#define PLL_IDDQ				(1 << 0)
#define PLL_RST_				(1 << 1)
#define PLL_EMULATION_RST_		(1 << 2)
#define PLL_PWR_OVRD			(1 << 3)
#define PLL_CKBUFPD_TR			(1 << 4)
#define PLL_CKBUFPD_TL			(1 << 5)
#define PLL_CKBUFPD_BR			(1 << 6)
#define PLL_CKBUFPD_BL			(1 << 7)
#define PLL_CKBUFPD_M			(1 << 8)
#define PLL_CKBUFPD_OVR			(1 << 9)
#define REFCLK_TERM100			(1 << 11)
#define REFCLK_SEL(x)			(((x) & 0xf) << 12)
#define PLL0_MODE				(1 << 16)
#define PLL0_LOCKDET			(1 << 19)
#define PLL0_REFCLK_NDIV(x)		(((x) & 0x3) << 20)
#define PLL1_MODE				(1 << 24)
#define PLL1_LOCKDET			(1 << 27)
#define PLL1_REFCLK_NDIV(x)		(((x) & 0x3) << 28)

#define IOPHY_PLL0_CTL2_0		0x34
#define XDIGCLK_SEL(x)			(((x) & 0x7) << 0)
#define XDIGCLK_EN				(1 << 3)
#define TXCLKREF_SEL			(1 << 4)
#define TXCLKREF_EN				(1 << 5)
#define REFCLKBUF_EN			(1 << 6)
#define XDIGCLK4P5_EN			(1 << 7)
#define TCLKOUT_SEL(x)			(((x) & 0xf) << 8)
#define TCLKOUT_EN				(1 << 12)
#define PLL_EMULATION_ON		(1 << 13)
#define PLL_BYPASS_EN			(1 << 15)
#define PLL0_CP_CNTL(x)			(((x) & 0xf) << 16)
#define PLL1_CP_CNTL(x)			(((x) & 0xf) << 20)
#define PLL_MISC_OUT(x)			(((x) & 0xff) << 24)

#define IOPHY_PLL0_CTL3_0		0x38
#define RCAL_CODE(x)			(((x) & 0x1f) << 0)
#define RCAL_BYPASS				(1 << 7)
#define RCAL_VAL(x)				(((x) & 0x1f) << 8)
#define RCAL_RESET				(1 << 14)
#define RCAL_DONE				(1 << 15)
#define PLL_BGAP_CNTL(x)		(((x) & 0x3) << 16)
#define PLL_BW_CNTL(x)			(((x) & 0x3f) << 20)
#define PLL_TEMP_CNTL(x)		(((x) & 0xf) << 28)

#define IOPHY_PLL0_CTL4_0		0x3c
#define PLL_MISC_CNTL(x)		(((x) & 0xfff) << 0)

#define IOPHY_USB3_PAD0_CTL_1_0	0x40
#define IOPHY_USB3_PAD1_CTL_1_0	0x44
#define USB3_RATE_MODE			(1 << 0)
#define USB3_TX_RATE(x)			(((x) & 0x3) << 1)
#define USB3_RX_RATE(x)			(((x) & 0x3) << 3)
#define TX_AMP(x)				(((x) & 0x3f) << 5)
#define TX_CMADJ(x)				(((x) & 0xf) << 11)
#define TX_DRV_CNTL(x)			(((x) & 0xf) << 15)

#define IOPHY_USB3_PAD0_CTL_2_0	0x48
#define IOPHY_USB3_PAD1_CTL_2_0	0x4c
#define TX_TERM_CNTL(x)			(((x) & 0x3) << 0)
#define RX_TERM_CNTL(x)			(((x) & 0x3) << 2)
#define RX_WANDER(x)			(((x) & 0xf) << 4)
#define RX_EQ(x)				(((x) & 0xffff) << 8)
#define CDR_CNTL(x)				(((x) & 0xff) << 24)

#define IOPHY_USB3_PAD0_CTL_3_0	0x50
#define EOM_CNTL(x)				(((x) & 0xffff) << 0)

#define IOPHY_USB3_PAD1_CTL_3_0	0x54
#define EOM_CNTL(x)				(((x) & 0xffff) << 0)

#define IOPHY_USB3_PAD0_CTL_4_0	0x58
#define DFE_CNTL(x)				(((x) & 0xffffffff) << 0)

#define IOPHY_USB3_PAD1_CTL_4_0	0x5c
#define DFE_CNTL(x)				(((x) & 0xffffffff) << 0)

#define IOPHY_MISC_PAD0_CTL_1_0	0x60
#define IOPHY_MISC_PAD1_CTL_1_0	0x64
#define IDDQ					(1 << 0)
#define IDDQ_OVRD				(1 << 1)
#define CKBUFPD					(1 << 2)
#define CKBUFPD_OVRD			(1 << 3)
#define TX_SLEEP(x)				(((x) & 0x3) << 4)
#define TX_DATA_READY			(1 << 6)
#define TX_DATA_EN				(1 << 7)
#define RX_SLEEP(x)				(((x) & 0x3) << 8)
#define RX_DATA_READY			(1 << 10)
#define RX_DATA_EN				(1 << 11)
#define RX_STAT_IDLE			(1 << 12)
#define TX_STAT_PRESENT			(1 << 13)
#define TX_RDET					(1 << 15)
#define TX_RATE(x)				(((x) & 0x3) << 16)
#define RX_RATE(x)				(((x) & 0x3) << 18)
#define TX_DIV(x)				(((x) & 0x3) << 20)
#define RX_DIV(x)				(((x) & 0x3) << 22)
#define RATE_MODE				(1 << 24)
#define RATE_MODE_OVRD			(1 << 25)
#define TX_PWR_OVRD				(1 << 26)
#define RX_PWR_OVRD				(1 << 27)

#define IOPHY_MISC_PAD0_CTL_2_0	0x68
#define IOPHY_MISC_PAD1_CTL_2_0	0x6c
#define NED_MODE(x)				(((x) & 0x3) << 0)
#define NED_LOOP				(1 << 2)
#define NEA_LOOP				(1 << 3)
#define FEA_MODE(x)				(((x) & 0x7) << 4)
#define FEA_LOOP				(1 << 7)
#define TX_DATA_MODE(x)			(((x) & 0x7) << 8)
#define FED_LOOP				(1 << 11)
#define TX_SYNC					(1 << 12)
#define RX_CDR_RESET			(1 << 13)
#define PRBS_ERROR				(1 << 24)
#define PRBS_CHK_EN				(1 << 25)
#define TEST_EN					(1 << 27)
#define SPARE_IN(x)				(((x) & 0x3) << 28)
#define SPARE_OUT(x)			(((x) & 0x3) << 30)

#define IOPHY_MISC_PAD0_CTL_3_0	0x70
#define IOPHY_MISC_PAD1_CTL_3_0	0x74
#define MISC_CNTL(x)			(((x) & 0xf) << 0)
#define TX_SEL_LOAD(x)			(((x) & 0xf) << 8)
#define TX_RDET_T(x)			(((x) & 0x3) << 12)
#define RX_IDLE_T(x)			(((x) & 0x3) << 14)
#define TX_RDET_BYP				(1 << 16)
#define RX_IDLE_BYP				(1 << 17)
#define RX_IDLE_MODE			(1 << 18)
#define RX_IDLE_MODE_OVRD		(1 << 19)
#define CDR_TEST(x)				(((x) & 0xfff) << 20)

#define IOPHY_MISC_PAD0_CTL_4_0		0x78
#define IOPHY_MISC_PAD1_CTL_4_0		0x7c
#define TX_BYP_OUT				(1 << 4)
#define TX_BYP_IN				(1 << 5)
#define TX_BYP_DIR				(1 << 6)
#define TX_BYP_EN				(1 << 7)
#define RX_BYP_IN				(1 << 8)
#define RX_BYP_OUT				(1 << 9)
#define RX_BYP_DIR				(1 << 10)
#define RX_BYP_EN				(1 << 11)
#define RX_BYP_MODE				(1 << 12)
#define TX_BYP_OVRD				(1 << 13)
#define AUX_IDDQ				(1 << 20)
#define AUX_IDDQ_OVRD			(1 << 21)
#define AUX_HOLD_EN				(1 << 22)
#define AUX_MODE_OVRD			(1 << 23)
#define AUX_TX_TERM_EN			(1 << 24)
#define AUX_TX_RDET_EN			(1 << 25)
#define AUX_TX_RDET_CLk_EN		(1 << 26)
#define AUX_TX_STAT_PRESENT		(1 << 27)
#define AUX_RX_TERM_EN			(1 << 28)
#define AUX_RX_IDLE_EN			(1 << 29)
#define AUX_RX_IDLE_MODE		(1 << 30)
#define AUX_RX_STAT_IDLE		(1 << 31)

#define IOPHY_MISC_PAD0_CTL_5_0		0x80
#define IOPHY_MISC_PAD1_CTL_5_0		0x84
#define DFE_TRAIN_EN			(1 << 0)
#define DFE_TRAIN_DONE			(1 << 1)
#define DFE_RESET				(1 << 3)
#define EOM_TRAIN_EN			(1 << 4)
#define EOM_TRAIN_DONE			(1 << 5)
#define EOM_EN					(1 << 7)
#define RX_QEYE_EN				(1 << 8)
#define RX_QEYE_OUT(x)			(((x) & 0xf) << 12)

#define IOPHY_MISC_PAD0_CTL_6_0		0x88
#define IOPHY_MISC_PAD1_CTL_6_0		0x8c
#define MISC_TEST(x)			(((x) & 0xffff) << 0)
#define MISC_OUT_SEL(x)			(((x) & 0xff) << 16)
#define MISC_OUT(x)				(((x) & 0xff) << 24)

#define USB2_OTG_PAD0_CTL_0_0	0x90
#define USB2_OTG_PAD1_CTL_0_0	0x94
#define HS_CURR_LEVEL(x)		(((x) & 0x3f) << 0)
#define HS_SLEW(x)				(((x) & 0x3f) << 6)
#define FS_SLEW(x)				(((x) & 0x3) << 12)
#define LS_RSLEW(x)				(((x) & 0x3) << 14)
#define LS_FSLEW(x)				(((x) & 0x3) << 16)
#define TERM_EN					(1 << 18)
#define PD						(1 << 19)
#define PD2						(1 << 20)
#define PD_ZI					(1 << 21)
#define DISCON_DETECT_METHOD	(1 << 22)
#define LSBIAS_SEL				(1 << 23)

#define USB2_OTG_PAD0_CTL_1_0	0x98
#define USB2_OTG_PAD1_CTL_1_0	0x9c
#define PD_CHRP_FORCE_POWERUP	(1 << 0)
#define PD_DISC_FORCE_POWERUP	(1 << 1)
#define PD_DR				(1 << 2)
#define TERM_RANGE_ADJ(x)	(((x) & 0xf) << 3)
#define SPARE(x)			(((x) & 0x3) << 7)
#define HS_IREF_CAP(x)		(((x) & 0x3) << 9)
#define RPU_RANGE_ADJ(x)	(((x) & 0x3) << 11)

#define USB2_BIAS_PAD_CTL_0_0	0xa0
#define HS_SQUELCH_LEVEL(x)	(((x) & 0x3) << 0)
#define HS_DISCON_LEVEL(x)	(((x) & 0x7) << 2)
#define HS_CHIRP_LEVEL(x)	(((x) & 0x3) << 5)
#define VBUS_LEVEL(x)		(((x) & 0x3) << 7)
#define TERM_OFFSET(x)		(((x) & 0x7) << 9)
#define BIAS_PD				(1 << 12)
#define PD_TRK				(1 << 13)
#define ADJRPU(x)			(((x) & 0x7) << 14)

#define USB2_BIAS_PAD_CTL_1_0	0xa4
#define RCTRL(x)			(((x) & 0xffff) << 0)
#define TCTRL(x)			(((x) & 0xffff0000) >> 16)

#define HSIC_PAD0_CTL_0_0	0xa8
#define HSIC_PAD1_CTL_0_0	0xac
#define TX_RTUNEP(x)		(((x) & 0xf) << 0)
#define TX_RTUNEN(x)		(((x) & 0xf) << 4)
#define TX_SLEWP(x)			(((x) & 0xf) << 8)
#define TX_SLEWN(x)			(((x) & 0xf) << 12)
#define HSIC_OPT(x)			(((x) & 0xf) << 16)

#define HSIC_PAD0_CTL_1_0	0xb0
#define HSIC_PAD1_CTL_1_0	0xb4
#define AUTO_TERM_EN		(1 << 0)
#define HSIC_IDDQ			(1 << 1)
#define PD_TX				(1 << 2)
#define PD_TRX				(1 << 3)
#define PD_RX				(1 << 4)
#define HSIC_PD_ZI			(1 << 5)
#define LPBK				(1 << 6)
#define RPD_DATA			(1 << 7)
#define RPD_STROBE			(1 << 8)
#define RPU_DATA			(1 << 9)
#define RPU_STROBE			(1 << 10)

#define HSIC_PAD0_CTL_2_0	0xb8
#define HSIC_PAD1_CTL_2_0	0xbc
#define RX_DATA_TRIM(x)		(((x) & 0xf) << 0)
#define RX_STROBE_TRIM(x)	(((x) & 0xf) << 4)
#define CALIOUT(x)			(((x) & 0xffff) << 16)

#define ULPI_LINK_TRIM_CONTROL_0	0xc0
#define DAT_TRIM_VAL(x)		(((x) & 0xff) << 0)
#define DAT_SEL_DEL0		(1 << 9)
#define DAT_SEL_DEL1		(1 << 10)
#define CTL_TRIM_VAL(x)		(((x) & 0xff) << 16)
#define CTL_SEL_DEL0		(1 << 24)
#define CTL_SEL_DEL1		(1 << 25)

#define ULPI_NULL_CLK_TRIM_CONTROL_0	0xc4
#define NULL_CLKOUT_TRIM_VAL(x)		(((x) & 0x1f) << 0)
#define NULL_LBKCLK_TRIM_VAL(x)		(((x) & 0x1f) << 8)

#define HSIC_STRB_TRIM_CONTROL_0	0xc8
#define STRB_TRIM_VAL(x)		(((x) & 0x3f) << 0)

#define WAKE_CTRL_0		0xcc
#define PORT0_FORCE_TX_RDET_CLK_ENABLE	(1 << 0)
#define PORT1_FORCE_TX_RDET_CLK_ENABLE	(1 << 1)

#define PM_SPARE_0				0xd0
#define OTG_PM_SPARE_BIT0		(1 << 0)
#define OTG_PM_SPARE_BIT1		(1 << 1)
#define OTG_PM_SPARE_BIT2		(1 << 2)
#define OTG_PM_SPARE_BIT3		(1 << 3)
#define ULPI_PM_SPARE_BIT0		(1 << 4)
#define ULPI_PM_SPARE_BIT1		(1 << 5)
#define ULPI_PM_SPARE_BIT2		(1 << 6)
#define ULPI_PM_SPARE_BIT3		(1 << 7)
#define HSIC_PM_SPARE_BIT0		(1 << 8)
#define HSIC_PM_SPARE_BIT1		(1 << 9)
#define HSIC_PM_SPARE_BIT2		(1 << 10)
#define HSIC_PM_SPARE_BIT3		(1 << 11)
/* XUSB PAD Ctl Registers END */

/*
 * FIXME: looks like no any .c requires below structure types
 * revisit and decide whether we can delete or not
 */
struct usb2_pad_port_map {
	u32 hsic_port0;
	u32 hsic_port1;
	u32 ulpi_port;
	u32 otg_port1;
	u32 otg_port0;
};

struct usb2_otg_caps {
	u32 port0_cap;
	u32 port0_internal;
	u32 port1_cap;
	u32 port1_internal;
};

struct usb2_ulpi_caps {
	u32	port_cap;
	u32 port_internal;
};

/* this is used to assign the SuperSpeed port mapping
 * to USB2.0 ports owned by XUSB, where the SuperSpeed ports inherit
 * their port capabilities from the USB2.0 ports they mapped to*/

struct usb2_ss_port_map {
	u32	port0;
	u32 port1;
};

struct hsic_pad0_ctl_0_vals {
	u32 tx_rtunep;
	u32 tx_rtunen;
	u32 tx_slewp;
	u32 tx_slewn;
	u32 hsic_opt;
};

struct hsic_pad0_ctl_1_vals {
	u32	tx_rtunep;
	u32 tx_rtunen;
	u32 tx_slewp;
	u32 tx_slewn;
	u32 hsic_opt;
};

struct snps_oc_map_0 {
	u32 controller1_oc;
	u32 controller2_oc;
	u32 controller3_oc;
};

struct usb2_oc_map_0 {
	u32 port0;
	u32 port1;
};

struct vbus_enable_oc_map {
	u32 vbus_en0;
	u32 vbus_en1;
};

#endif
