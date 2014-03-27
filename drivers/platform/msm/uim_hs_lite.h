/* Copyright (c) 2011-2014 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */

#ifndef MSM_UIM_HWREG_H
#define MSM_UIM_HWREG_H

/* Register Addresses */
#define UARTDM_MR1_REG			0x000
#define UARTDM_MR2_REG			0x004
#define UARTDM_IPR_REG			0x018
#define UARTDM_TFWR_REG			0x01c
#define UARTDM_RFWR_REG			0x020
#define UARTDM_DMRX_REG			0x034
#define UARTDM_DMEN_REG			0x03c
#define UARTDM_NCF_TX_REG		0x040
#define UARTDM_TXFS_REG			0x04c
#define UARTDM_RXFS_REG			0x050
#define UARTDM_SIM_CFG_REG		0x080
#define UARTDM_CSR_REG			0x0a0
#define UARTDM_SR_REG			0x0a4
#define UARTDM_CR_REG			0x0a8
#define UARTDM_MISR_REG			0x0ac
#define UARTDM_IMR_REG			0x0b0
#define UARTDM_ISR_REG			0x0b4
#define UARTDM_RX_TOTAL_SNAP_REG	0x0bc
#define UARTDM_TF_REG			0x100
#define UARTDM_RF_REG			0x140
#define UARTDM_UIM_CFG_REG		0x180
#define UARTDM_UIM_CMD_REG		0x184
#define UARTDM_UIM_IO_STATUS_REG	0x188

/* SR */
#define UARTDM_SR_RX_BREAK_BMSK	        BIT(6)
#define UARTDM_SR_PAR_FRAME_BMSK	BIT(5)
#define UARTDM_SR_OVERRUN_BMSK		BIT(4)
#define UARTDM_SR_TXEMT_BMSK		BIT(3)
#define UARTDM_SR_TXRDY_BMSK		BIT(2)
#define UARTDM_SR_RXRDY_BMSK		BIT(0)

/* CR */
#define UARTDM_CR_TX_DISABLE_BMSK	BIT(3)
#define UARTDM_CR_RX_DISABLE_BMSK	BIT(1)
#define UARTDM_CR_TX_EN_BMSK		BIT(2)
#define UARTDM_CR_RX_EN_BMSK		BIT(0)

#define RESET_RX			0x10
#define RESET_TX			0x20
#define RESET_ERROR_STATUS		0x30
#define RESET_BREAK_INT			0x40
#define START_BREAK			0x50
#define STOP_BREAK			0x60
#define RESET_CTS			0x70
#define RESET_STALE_INT			0x80
#define RFR_LOW				0xD0
#define RFR_HIGH			0xE0
#define CR_PROTECTION_EN		0x100
#define STALE_EVENT_ENABLE		0x500
#define STALE_EVENT_DISABLE		0x600
#define FORCE_STALE_EVENT		0x400
#define CLEAR_TX_READY			0x300
#define RESET_TX_ERROR			0x800
#define RESET_TX_DONE			0x810

/* MR1 */
#define UARTDM_MR1_AUTO_RFR_LEVEL1_BMSK 0xffffff00
#define UARTDM_MR1_AUTO_RFR_LEVEL0_BMSK 0x3f
#define UARTDM_MR1_CTS_CTL_BMSK		0x40
#define UARTDM_MR1_RX_RDY_CTL_BMSK	0x80

/* MR2 */
#define UARTDM_MR2_BITS_PER_CHAR_8	(0x3 << 4)
#define UARTDM_MR2_STOP_BIT_TWO		(3 << 2)
#define UARTDM_MR2_PARITY_EVEN		(2)

#define UARTDM_MR2_RX_ERROR_CHAR_OFF		0x200
#define UARTDM_MR2_RX_BREAK_ZERO_CHAR_OFF	0x100

/* CSR */
#define UARTDM_CSR_28800		0xcc
#define UARTDM_CSR_115200		0xff

/* IPR */
#define UARTDM_IPR_STALE_TIMEOUT_MSB_BMSK 0xffffff80
#define UARTDM_IPR_STALE_LSB_BMSK	0x1f

/* ISR, IMR */
#define UARTDM_ISR_TX_READY_BMSK	BIT(7)
#define UARTDM_ISR_CURRENT_CTS_BMSK	BIT(6)
#define UARTDM_ISR_DELTA_CTS_BMSK	BIT(5)
#define UARTDM_ISR_RXLEV_BMSK		BIT(4)
#define UARTDM_ISR_RXSTALE_BMSK		BIT(3)
#define UARTDM_ISR_RXBREAK_BMSK		BIT(2)
#define UARTDM_ISR_RXHUNT_BMSK		BIT(1)
#define UARTDM_ISR_TXLEV_BMSK		BIT(0)

/* SIM_CFG */
#define UART_SIM_CFG_UIM_TX_MODE	(1 << 17)
#define UART_SIM_CFG_UIM_RX_MODE	(1 << 16)
#define UART_SIM_CFG_STOP_BIT_LEN_N(n)	((n) << 8)
#define UART_SIM_CFG_SIM_CLK_ON		(1 << 7)
#define UART_SIM_CFG_SIM_CLK_TD8_SEL	(1 << 6)
#define UART_SIM_CFG_SIM_CLK_STOP_HIGH	(1 << 5)
#define UART_SIM_CFG_MASK_RX		(1 << 3)
#define UART_SIM_CFG_SWAP_D		(1 << 2)
#define UART_SIM_CFG_INV_D		(1 << 1)
#define UART_SIM_CFG_SIM_SEL		(1 << 0)

/* UIM_CFG */
#define UART_UIM_CFG_SW_RESET		(1 << 13)
#define UART_UIM_CFG_MODE18		(1 << 12)
#define UART_UIM_CFG_CARD_EVENTS_ENABLE	(1 << 6)
#define UART_UIM_CFG_PRESENT_POLARITY	(1 << 5)

/* UIM_CMD */
#define UART_UIM_CMD_RECOVER		(1 << 1)
#define UART_UIM_CMD_DEACTIVATE		(1 << 0)

/* UIM_IO_STATUS */
#define UART_UIM_IO_STATUS_WIP		(1 << 2)
#define UART_UIM_IO_STATUS_DEACTIVATED	(1 << 1)
#define UART_UIM_IO_STATUS_PRESENT	(1 << 0)

#endif /* MSM_UIM_HWREG_H */
