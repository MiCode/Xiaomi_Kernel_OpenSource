/*
 * Copyright (C) 2007 Google, Inc.
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

#define USB_AHBBURST         (MSM_USB_BASE + 0x0090)
#define USB_AHBMODE          (MSM_USB_BASE + 0x0098)
#define USB_GENCONFIG        (MSM_USB_BASE + 0x009C)
#define USB_GENCONFIG2       (MSM_USB_BASE + 0x00A0)
#define USB_CAPLENGTH        (MSM_USB_BASE + 0x0100) /* 8 bit */
#define USB_HS_GPTIMER_BASE  (MSM_USB_BASE + 0x80)
#define USB_HS_APF_CTRL      (MSM_USB_BASE + 0x0380)

#define GENCFG2_SESS_VLD_CTRL_EN		BIT(7)
#define GENCFG2_LINESTATE_DIFF_WAKEUP_EN	BIT(12)
#define GENCFG2_SYS_CLK_HOST_DEV_GATE_EN	BIT(13)
#define GENCFG2_DPSE_DMSE_HV_INTR_EN		BIT(15)
#define GENCFG2_TX_BUF_PREFETCH_FIX_EN		BIT(26)

#define APF_CTRL_EN				BIT(0)

#define USB_USBCMD           (MSM_USB_BASE + 0x0140)
#define USB_USBSTS           (MSM_USB_BASE + 0x0144)
#define USB_PORTSC           (MSM_USB_BASE + 0x0184)
#define USB_OTGSC            (MSM_USB_BASE + 0x01A4)
#define USB_USBMODE          (MSM_USB_BASE + 0x01A8)
#define USB_PHY_CTRL         (MSM_USB_BASE + 0x0240)
#define USB_PHY_CTRL2        (MSM_USB_BASE + 0x0278)

#define USBCMD_RESET		2
#define USBCMD_SESS_VLD_CTRL	BIT(25)
#define USBCMD_FS_SELECT	BIT(27)

#define USB_USBINTR          (MSM_USB_BASE + 0x0148)
#define USB_FRINDEX          (MSM_USB_BASE + 0x014C)

#define USB_L1_EP_CTRL	     (MSM_USB_BASE + 0x0250)
#define USB_L1_CONFIG	     (MSM_USB_BASE + 0x0254)

#define L1_CONFIG_LPM_EN	BIT(4)
#define L1_CONFIG_REMOTE_WAKEUP BIT(5)
#define L1_CONFIG_GATE_SYS_CLK	BIT(7)
#define L1_CONFIG_PHY_LPM	BIT(10)
#define L1_CONFIG_PLL		BIT(11)

#define AHB2AHB_BYPASS		BIT(31)
#define AHB2AHB_BYPASS_BIT_MASK        BIT(31)
#define AHB2AHB_BYPASS_CLEAR   (0 << 31)

#define PORTSC_PTS_MASK         (3 << 30)
#define PORTSC_PTS_ULPI         (2 << 30)
#define PORTSC_PTS_SERIAL       (3 << 30)
#define PORTSC_SPRT             (1 << 25) /* phy suspend mode */
#define PORTSC_PHCD             (1 << 23) /* phy suspend mode */
#define PORTSC_PP               (1 << 12)
#define PORTSC_LS               (3 << 10)
#define PORTSC_LS_DM            (1 << 10)
#define PORTSC_SUSP_MASK        (1 << 7)  /* Port Suspend */
#define PORTSC_FPR_MASK         (1 << 6)
#define PORTSC_CSC              (1 << 1)
#define PORTSC_CCS              (1 << 0)

#define USB_ULPI_VIEWPORT    (MSM_USB_BASE + 0x0170)
#define ULPI_RUN              (1 << 30)
#define ULPI_WRITE            (1 << 29)
#define ULPI_READ             (0 << 29)
#define ULPI_SYNC_STATE       (1 << 27)
#define ULPI_ADDR(n)          (((n) & 255) << 16)
#define ULPI_DATA(n)          ((n) & 255)
#define ULPI_DATA_READ(n)     (((n) >> 8) & 255)

#define GENCONFIG_BAM_DISABLE (1 << 13)
#define GENCONFIG_TXFIFO_IDLE_FORCE_DISABLE (1 << 4)
#define GENCONFIG_ULPI_SERIAL_EN (1 << 5)

/* synopsys 28nm phy registers */
#define ULPI_PWR_CLK_MNG_REG	0x88
#define OTG_COMP_DISABLE	BIT(0)

/* ulpi manual dp registers */
#define ULPI_MISC_A			0x96
#define ULPI_MISC_A_VBUSVLDEXT		BIT(0)
#define ULPI_MISC_A_VBUSVLDEXTSEL	BIT(1)

#define ASYNC_INTR_CTRL         (1 << 29) /* Enable async interrupt */
#define ULPI_STP_CTRL           (1 << 30) /* Block communication with PHY */
#define PHY_RETEN               (1 << 1) /* PHY retention enable/disable */
#define PHY_IDHV_INTEN          (1 << 8) /* PHY ID HV interrupt */
#define PHY_OTGSESSVLDHV_INTEN  (1 << 9) /* PHY Session Valid HV int. */
#define PHY_CLAMP_DPDMSE_EN	(1 << 21) /* PHY mpm DP DM clamp enable */
#define PHY_POR_BIT_MASK	BIT(0)
#define PHY_POR_ASSERT		(1 << 0) /* USB2 28nm PHY POR ASSERT */
#define PHY_POR_DEASSERT	(0 << 0) /* USB2 28nm PHY POR DEASSERT */

/* OTG definitions */
#define OTGSC_INTSTS_MASK	(0x7f << 16)
#define OTGSC_IDPU		(1 << 5)
#define OTGSC_INTR_MASK		(0x7f << 24)
#define OTGSC_HADP		(1 << 6)
#define OTGSC_ID		(1 << 8)
#define OTGSC_BSV		(1 << 11)
#define OTGSC_IDIS		(1 << 16)
#define OTGSC_BSVIS		(1 << 19)
#define OTGSC_IDIE		(1 << 24)
#define OTGSC_BSVIE		(1 << 27)

/* USB PHY CSR registers and bit definitions */

#define USB_PHY_CSR_PHY_CTRL_COMMON0 (MSM_USB_PHY_CSR_BASE + 0x078)
#define SIDDQ BIT(2)

#define USB_PHY_CSR_PHY_CTRL1 (MSM_USB_PHY_CSR_BASE + 0x08C)
#define ID_HV_CLAMP_EN_N BIT(1)

#define USB_PHY_CSR_PHY_CTRL3 (MSM_USB_PHY_CSR_BASE + 0x094)
#define CLAMP_MPM_DPSE_DMSE_EN_N BIT(2)

#define USB2_PHY_USB_PHY_IRQ_CMD (MSM_USB_PHY_CSR_BASE + 0x0D0)
#define USB2_PHY_USB_PHY_INTERRUPT_SRC_STATUS (MSM_USB_PHY_CSR_BASE + 0x05C)

#define USB2_PHY_USB_PHY_INTERRUPT_STATUS0 (MSM_USB_PHY_CSR_BASE + 0x03C)
#define USB2_PHY_USB_PHY_INTERRUPT_STATUS1 (MSM_USB_PHY_CSR_BASE + 0x040)

#define USB2_PHY_USB_PHY_INTERRUPT_CLEAR0 (MSM_USB_PHY_CSR_BASE + 0x0DC)
#define USB2_PHY_USB_PHY_INTERRUPT_CLEAR1 (MSM_USB_PHY_CSR_BASE + 0x0E0)

#define USB2_PHY_USB_PHY_INTERRUPT_MASK0 (MSM_USB_PHY_CSR_BASE + 0x0D4)
#define USB2_PHY_USB_PHY_INTERRUPT_MASK1 (MSM_USB_PHY_CSR_BASE + 0x0D8)

#define USB_PHY_IDDIG_1_0 BIT(7)

#define USB_PHY_IDDIG_RISE_MASK BIT(0)
#define USB_PHY_IDDIG_FALL_MASK BIT(1)
#define USB_PHY_ID_MASK (USB_PHY_IDDIG_RISE_MASK | USB_PHY_IDDIG_FALL_MASK)

#endif /* __LINUX_USB_GADGET_MSM72K_UDC_H__ */
