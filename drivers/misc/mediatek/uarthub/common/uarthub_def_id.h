/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef UARTHUB_DEF_ID_H
#define UARTHUB_DEF_ID_H

#define GET_BIT_MASK(value, mask) \
	((value) & (mask))
#define SET_BIT_MASK(pdest, value, mask) \
	(*(pdest) = (GET_BIT_MASK(*(pdest), ~(mask)) | GET_BIT_MASK(value, mask)))
#define UARTHUB_SET_BIT(REG, BITVAL) \
	(*((unsigned int *)(REG)) |= ((unsigned int)(BITVAL)))
#define UARTHUB_CLR_BIT(REG, BITVAL) \
	((*(unsigned int *)(REG)) &= ~((unsigned int)(BITVAL)))
#define UARTHUB_REG_READ(addr) \
	(*((unsigned int *)(addr)))
#define UARTHUB_REG_READ_BIT(addr, BITVAL) \
	(*((unsigned int *)(addr)) & ((unsigned int)(BITVAL)))
#define UARTHUB_REG_WRITE(addr, data) do {\
	writel(data, (void *)addr); \
	mb(); /* make sure register access in order */ \
} while (0)
#define UARTHUB_REG_WRITE_MASK(reg, data, mask) {\
	unsigned int val = UARTHUB_REG_READ(reg); \
	SET_BIT_MASK(&val, data, mask); \
	UARTHUB_REG_WRITE(reg, val); \
}

/* uarthub_base_remap_addr */
#define UARTHUB_CMM_BASE_ADDR(_baseaddr)                (_baseaddr+0x000)
#define UARTHUB_DEV_0_BASE_ADDR(_baseaddr)              (_baseaddr+0x100)
#define UARTHUB_DEV_1_BASE_ADDR(_baseaddr)              (_baseaddr+0x200)
#define UARTHUB_DEV_2_BASE_ADDR(_baseaddr)              (_baseaddr+0x300)
#define UARTHUB_INTFHUB_BASE_ADDR(_baseaddr)            (_baseaddr+0x400)

/* INTFHUB for dev0, intfhub_base_remap_addr */
#define UARTHUB_INTFHUB_DEV0_STA(_baseaddr)             (_baseaddr+0x0)
#define UARTHUB_INTFHUB_DEV0_STA_SET(_baseaddr)         (_baseaddr+0x4)
#define UARTHUB_INTFHUB_DEV0_STA_CLR(_baseaddr)         (_baseaddr+0x8)
#define UARTHUB_INTFHUB_DEV0_RX_ERR_CRC_STA(_baseaddr)  (_baseaddr+0x10)
#define UARTHUB_INTFHUB_DEV1_RX_ERR_CRC_STA(_baseaddr)  (_baseaddr+0x14)
#define UARTHUB_INTFHUB_DEV2_RX_ERR_CRC_STA(_baseaddr)  (_baseaddr+0x18)
#define UARTHUB_INTFHUB_DEV0_PKT_CNT(_baseaddr)         (_baseaddr+0x1c)
#define UARTHUB_INTFHUB_DEV0_CRC_STA(_baseaddr)         (_baseaddr+0x20)
#define UARTHUB_INTFHUB_DEV0_IRQ_STA(_baseaddr)         (_baseaddr+0x30)
#define UARTHUB_INTFHUB_DEV0_IRQ_CLR(_baseaddr)         (_baseaddr+0x34)
#define UARTHUB_INTFHUB_DEV0_IRQ_MASK(_baseaddr)        (_baseaddr+0x38)

/* INTFHUB for dev1, intfhub_base_remap_addr */
#define UARTHUB_INTFHUB_DEV1_STA(_baseaddr)             (_baseaddr+0x40)
#define UARTHUB_INTFHUB_DEV1_STA_SET(_baseaddr)         (_baseaddr+0x44)
#define UARTHUB_INTFHUB_DEV1_STA_CLR(_baseaddr)         (_baseaddr+0x48)
#define UARTHUB_INTFHUB_DEV1_PKT_CNT(_baseaddr)         (_baseaddr+0x50)
#define UARTHUB_INTFHUB_DEV1_CRC_STA(_baseaddr)         (_baseaddr+0x54)

/* INTFHUB for dev2, intfhub_base_remap_addr */
#define UARTHUB_INTFHUB_DEV2_STA(_baseaddr)             (_baseaddr+0x80)
#define UARTHUB_INTFHUB_DEV2_STA_SET(_baseaddr)         (_baseaddr+0x84)
#define UARTHUB_INTFHUB_DEV2_STA_CLR(_baseaddr)         (_baseaddr+0x88)
#define UARTHUB_INTFHUB_DEV2_PKT_CNT(_baseaddr)         (_baseaddr+0x90)
#define UARTHUB_INTFHUB_DEV2_CRC_STA(_baseaddr)         (_baseaddr+0x94)

/* INTFHUB for SSPM, intfhub_base_remap_addr */
#define UARTHUB_INTFHUB_CON0(_baseaddr)                 (_baseaddr+0xc0)
#define UARTHUB_INTFHUB_CON1(_baseaddr)                 (_baseaddr+0xc4)
#define UARTHUB_INTFHUB_CON2(_baseaddr)                 (_baseaddr+0xc8)
#define UARTHUB_INTFHUB_CON3(_baseaddr)                 (_baseaddr+0xcc)
#define UARTHUB_INTFHUB_IRQ_STA(_baseaddr)              (_baseaddr+0xd0)
#define UARTHUB_INTFHUB_IRQ_CLR(_baseaddr)              (_baseaddr+0xd4)
#define UARTHUB_INTFHUB_IRQ_MASK(_baseaddr)             (_baseaddr+0xd8)
#define UARTHUB_INTFHUB_STA0(_baseaddr)                 (_baseaddr+0xe0)
#define UARTHUB_INTFHUB_LOOPBACK(_baseaddr)             (_baseaddr+0xe4)
#define UARTHUB_INTFHUB_CON4(_baseaddr)                 (_baseaddr+0xf0)
#define UARTHUB_INTFHUB_DBG(_baseaddr)                  (_baseaddr+0xf4)

/* UART_X CODA
 * - cmm_base_remap_addr
 * - dev0_base_remap_addr
 * - dev1_base_remap_addr
 * - dev2_base_remap_addr
 */
#define UARTHUB_RBR_THR(_baseaddr)                      (_baseaddr+0x0)
#define UARTHUB_IER(_baseaddr)                          (_baseaddr+0x4)
#define UARTHUB_IIR_FCR(_baseaddr)                      (_baseaddr+0x8)
#define UARTHUB_LCR(_baseaddr)                          (_baseaddr+0xc)
#define UARTHUB_MCR(_baseaddr)                          (_baseaddr+0x10)
#define UARTHUB_LSR(_baseaddr)                          (_baseaddr+0x14)
#define UARTHUB_MSR(_baseaddr)                          (_baseaddr+0x18)
#define UARTHUB_SCR(_baseaddr)                          (_baseaddr+0x1c)
#define UARTHUB_AUTOBAUD_EN(_baseaddr)                  (_baseaddr+0x20)
#define UARTHUB_HIGHSPEED(_baseaddr)                    (_baseaddr+0x24)
#define UARTHUB_SAMPLE_COUNT(_baseaddr)                 (_baseaddr+0x28)
#define UARTHUB_SAMPLE_POINT(_baseaddr)                 (_baseaddr+0x2c)
#define UARTHUB_AUTOBAUD_REG(_baseaddr)                 (_baseaddr+0x30)
#define UARTHUB_RATEFIX_AD(_baseaddr)                   (_baseaddr+0x34)
#define UARTHUB_AUTOBAUDSAMPLE(_baseaddr)               (_baseaddr+0x38)
#define UARTHUB_GUARD(_baseaddr)                        (_baseaddr+0x3c)
#define UARTHUB_ESCAPE_DAT(_baseaddr)                   (_baseaddr+0x40)
#define UARTHUB_ESCAPE_EN(_baseaddr)                    (_baseaddr+0x44)
#define UARTHUB_SLEEP_EN(_baseaddr)                     (_baseaddr+0x48)
#define UARTHUB_DMA_EN(_baseaddr)                       (_baseaddr+0x4c)
#define UARTHUB_RXTRI_AD(_baseaddr)                     (_baseaddr+0x50)
#define UARTHUB_FRACDIV_L(_baseaddr)                    (_baseaddr+0x54)
#define UARTHUB_FRACDIV_M(_baseaddr)                    (_baseaddr+0x58)
#define UARTHUB_FCR_RD(_baseaddr)                       (_baseaddr+0x5c)
#define UARTHUB_TX_FIFO_OFFSET(_baseaddr)               (_baseaddr+0x70)
#define UARTHUB_RX_FIFO_OFFSET(_baseaddr)               (_baseaddr+0x7c)
#define UARTHUB_RTO_CFG(_baseaddr)                      (_baseaddr+0x88)
#define UARTHUB_DLL(_baseaddr)                          (_baseaddr+0x90)
#define UARTHUB_DLM(_baseaddr)                          (_baseaddr+0x94)
#define UARTHUB_EFR(_baseaddr)                          (_baseaddr+0x98)
#define UARTHUB_FEATURE_SEL(_baseaddr)                  (_baseaddr+0x9c)
#define UARTHUB_XON1(_baseaddr)                         (_baseaddr+0xa0)
#define UARTHUB_XON2(_baseaddr)                         (_baseaddr+0xa4)
#define UARTHUB_XOFF1(_baseaddr)                        (_baseaddr+0xa8)
#define UARTHUB_XOFF2(_baseaddr)                        (_baseaddr+0xac)
#define UARTHUB_USB_RX_SEL(_baseaddr)                   (_baseaddr+0xb0)
#define UARTHUB_SLEEP_REQ(_baseaddr)                    (_baseaddr+0xb4)
#define UARTHUB_SLEEP_ACK(_baseaddr)                    (_baseaddr+0xb8)
#define UARTHUB_SPM_SEL(_baseaddr)                      (_baseaddr+0xbc)

#endif /* UARTHUB_DEF_ID_H */
