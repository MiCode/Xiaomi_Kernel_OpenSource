/*
 * Copyright (c) 2011, The Linux Foundation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __ASM_ARCH_MSM_SERIAL_HSL_REGS_H
#define __ASM_ARCH_MSM_SERIAL_HSL_REGS_H

#ifdef CONFIG_MSM_HAS_DEBUG_UART_HS_V14
#define UARTDM_MR2_OFFSET	0x4
#define UARTDM_CSR_OFFSET	0xa0
#define UARTDM_SR_OFFSET	0xa4
#define UARTDM_CR_OFFSET	0xa8
#define UARTDM_ISR_OFFSET	0xb4
#define UARTDM_NCF_TX_OFFSET	0x40
#define UARTDM_TF_OFFSET	0x100
#else
#define UARTDM_MR2_OFFSET	0x4
#define UARTDM_CSR_OFFSET	0x8
#define UARTDM_SR_OFFSET	0x8
#define UARTDM_CR_OFFSET	0x10
#define UARTDM_ISR_OFFSET	0x14
#define UARTDM_NCF_TX_OFFSET	0x40
#define UARTDM_TF_OFFSET	0x70
#endif

#endif
