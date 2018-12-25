/*
 * Copyright (C) 2017 MediaTek Inc.
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

#ifndef __MTK_PLAT_UART_H__
#define __MTK_PLAT_UART_H__

#include <asm/irq.h>
#include <linux/irq.h>

#if !defined(CONFIG_MTK_CLKMGR)
#include <linux/clk.h>
#endif				/* !defined(CONFIG_MTK_CLKMGR) */
#include <linux/pinctrl/consumer.h>

/******************************************************************************
 * Function Configuration
 *****************************************************************************/
#define ENABLE_DEBUG
#define ENABLE_VFIFO
/* Fix-me: marked for early porting */
#define POWER_FEATURE		/*control power-on/power-off */
#define ENABLE_FRACTIONAL
#define ENABLE_SYSFS
/* #define UART_USING_FIX_CLK_ENABLE */
#define UART_FCR_USING_SW_BACK_UP
#define ENABLE_UART_SLEEP
/*---------------------------------------------------------------------------*/
#if defined(WMT_PLAT_ALPS)
#define ENABLE_RAW_DATA_DUMP
#endif
/*---------------------------------------------------------------------------*/
#if defined(ENABLE_VFIFO) && defined(ENABLE_DEBUG)
#define ENABLE_VFIFO_DEBUG
#endif
/******************************************************************************
 * MACRO & CONSTANT
 *****************************************************************************/
#define DBG_TAG                     "[UART] "
#define CFG_UART_AUTOBAUD           0
/*---------------------------------------------------------------------------*/
#define UART_VFIFO_SIZE             8192
#define UART_VFIFO_ALERT_LEN        0x3f
/*---------------------------------------------------------------------------*/
#define UART_MAX_TX_PENDING         1024
/*---------------------------------------------------------------------------*/
#define UART_MAJOR                  204
#define UART_MINOR                  209
#if defined(CONFIG_FPGA_EARLY_PORTING)
#define UART_NR                     2
#else
#define UART_NR                     2
#endif

#ifndef FALSE
#define FALSE (0)
#endif
#ifndef TRUE
#define TRUE  (1)
#endif
/*---------------------------------------------------------------------------*/
#define WAIT_UART_ACK_TIMES			10
/*---------------------------------------------------------------------------*/
#define IRQF_LEVEL_TRIGGER_POLARITY IRQF_TRIGGER_LOW
#define IRQF_EDGE_TRIGGER_POLARITY IRQF_TRIGGER_FALLING
/*---- Macro defination remap -----------------------------------------------*/
/*------ IRQ Section -----------------------*/
#ifndef CONFIG_OF
#define UART1_IRQ_ID	UART0_IRQ_BIT_ID
#define UART2_IRQ_ID	UART1_IRQ_BIT_ID
#define UART3_IRQ_ID	UART2_IRQ_BIT_ID
#define UART4_IRQ_ID	UART3_IRQ_BIT_ID

#define UART1_VFF_TX_IRQ_ID  AP_DMA_UART0_TX_IRQ_BIT_ID
#define UART1_VFF_RX_IRQ_ID  AP_DMA_UART0_RX_IRQ_BIT_ID
#define UART2_VFF_TX_IRQ_ID  AP_DMA_UART1_TX_IRQ_BIT_ID
#define UART2_VFF_RX_IRQ_ID  AP_DMA_UART1_RX_IRQ_BIT_ID
#define UART3_VFF_TX_IRQ_ID  AP_DMA_UART2_TX_IRQ_BIT_ID
#define UART3_VFF_RX_IRQ_ID  AP_DMA_UART2_RX_IRQ_BIT_ID
#endif
/*------ PDN Section -----------------------*/
#if defined(CONFIG_MTK_CLKMGR) && !defined(CONFIG_FPGA_EARLY_PORTING)
#define PDN_FOR_UART1   MT_CG_INFRA_UART0
#define PDN_FOR_UART2   MT_CG_INFRA_UART1
/*
 *#define PDN_FOR_UART3   MT_CG_INFRA_UART2
 *#define PDN_FOR_UART4   MT_CG_INFRA_UART3
 */
#define PDN_FOR_DMA     MT_CG_INFRA_APDMA
#endif

#if (defined(CONFIG_FIQ_DEBUGGER_CONSOLE) && defined(CONFIG_FIQ_DEBUGGER))
#define DEFAULT_FIQ_UART_PORT           (3)
#endif

/* For ATE factory feature */
#ifdef ATE_FACTORY_ENABLE
#define ATE_FACTORY_MODE     6
#endif
/*---------------------------------------------------------------------------*/
#define MTK_SYSCLK_65            65000000
#define MTK_SYSCLK_49_4        49400000
#define MTK_SYSCLK_58_5          58500000
#define MTK_SYSCLK_52            52000000
#define MTK_SYSCLK_26            26000000
#define MTK_SYSCLK_13            13000000
#define MTK_SYSCLK_6144          61440000
#define MTK_SYSCLK_3072          30720000
#define MTK_SYSCLK_1536          15360000
/*---------------------------------------------------------------------------*/
/* FIXME: MT6593 FPGA porting*/
#ifdef CONFIG_FPGA_EARLY_PORTING
#ifdef FIX_TO_26M
#define UART_SYSCLK                 MTK_SYSCLK_26
#else
#define UART_SYSCLK                 10000000
#endif
#else
#define UART_SYSCLK                 MTK_SYSCLK_26
#endif

/*---------------------------------------------------------------------------*/
#define ISWEXT          (0x80000000)	/* extended bit in c_iflag */
/*---------------------------------------------------------------------------*/
/* the size definition of VFF */
#define C_UART1_VFF_TX_SIZE (1024)	/* the size must be 8-byte alignment */
#define C_UART1_VFF_RX_SIZE (1024)	/* the size must be 8-byte alignment */
#define C_UART2_VFF_TX_SIZE (8192)	/* the size must be 8-byte alignment */
#define C_UART2_VFF_RX_SIZE (8192)	/* the size must be 8-byte alignment */
#define C_UART3_VFF_TX_SIZE (8192)	/* the size must be 8-byte alignment */
#define C_UART3_VFF_RX_SIZE (8192)	/* the size must be 8-byte alignment */
#define C_UART4_VFF_TX_SIZE (1024)	/* the size must be 8-byte alignment */
#define C_UART4_VFF_RX_SIZE (1024)	/* the size must be 8-byte alignment */
/******************************************************************************
 * LOG SETTING
 *****************************************************************************/
/* Debug message event */
#define DBG_EVT_NONE        0x00000000	/* No event */
#define DBG_EVT_DMA         0x00000001	/* DMA related event */
#define DBG_EVT_INT         0x00000002	/* UART INT event */
#define DBG_EVT_CFG         0x00000004	/* UART CFG event */
#define DBG_EVT_FUC         0x00000008	/* Function event */
#define DBG_EVT_INFO        0x00000010	/* information event */
#define DBG_EVT_ERR         0x00000020	/* Error event */
#define DBG_EVT_DAT         0x00000040	/* data dump to uart */
#define DBG_EVT_BUF         0x00000080	/* data dump to buffer */
#define DBG_EVT_MSC         0x00000100	/* misc log */
#define DBG_EVT_ALL         0xffffffff
/*---------------------------------------------------------------------------*/
#ifdef ENABLE_DEBUG
/*---------------------------------------------------------------------------*/
#define MSG(evt, fmt, args...) \
do { \
	if ((DBG_EVT_##evt) & uart->evt_mask) { \
		const char *s = #evt; \
	if (DBG_EVT_##evt & DBG_EVT_ERR) \
		pr_debug("  [UART%d]:%c:%4d: " fmt, \
		   uart->nport, s[0], __LINE__, ##args); \
	else \
		pr_debug("  [UART%d]:%c: " fmt, uart->nport, s[0], ##args); \
	} \
} while (0)
/*---------------------------------------------------------------------------*/
#define UART_DEBUG_EVT(evt)    ((evt) & uart->evt_mask)
/*---------------------------------------------------------------------------*/
#define MSG_FUNC_ENTRY(f)       MSG(FUC, "%s\n", __func__)
#define MSG_RAW                 pr_debug
/*---------------------------------------------------------------------------*/
#else				/* release mode: only enable error log */
#define MSG(evt, fmt, args...)  MSG##evt(fmt, ##args)
#define MSGERR(fmt, args...)\
	pr_debug("  [UART%d]:E:%4d: " fmt, uart->nport, __LINE__, ##args)
#define MSGDMA(fmt, args...)
#define MSGCFG(fmt, args...)
#define MSGFUC(fmt, args...)
#define MSGINFO(fmt, args...)
#define MSGDAT(fmt, args...)
#define MSGMSC(fmt, args...)
#define MSG_RAW(fmt, args...)
#define MSG_FUNC_ENTRY(f)       do {} while (0)
#endif /**/
#define MSG_ERR(fmt, args...)\
	pr_debug("[UARTX]:E:%4d: " fmt, __LINE__, ##args)
#define MSG_TRC(fmt, args...)\
	pr_debug("[UARTX]:T: " fmt, ##args)
#define DEV_TRC(fmt, args...)\
	pr_debug("[UART%d]:T: " fmt, uart->nport, ##args)
/*---------------------------------------------------------------------------*/
#define DRV_NAME                "mtk-uart"
/*---------------------------------------------------------------------------*/
/******************************************************************************
 * ENUM & STRUCT
 *****************************************************************************/
/* uart port ids */
enum {
	UART_PORT0 = 0,
	UART_PORT1,
	UART_PORT2,
	UART_PORT3,
	UART_PORT_NUM,
};
/*---------------------------------------------------------------------------*/
#define UART_FIFO_SIZE              (16)
/*---------------------------------------------------------------------------*/
#define UART_RBR             (unsigned long)(base+0x00)	/* Read only */
#define UART_THR             (unsigned long)(base+0x00)	/* Write only */
#define UART_IER             (unsigned long)(base+0x04)
#define UART_IIR             (unsigned long)(base+0x08)	/* Read only */
#define UART_FCR             (unsigned long)(base+0x08)	/* Write only */
#define UART_LCR             (unsigned long)(base+0x0c)
#define UART_MCR             (unsigned long)(base+0x10)
#define UART_LSR             (unsigned long)(base+0x14)
#define UART_MSR             (unsigned long)(base+0x18)
#define UART_SCR             (unsigned long)(base+0x1c)
#define UART_DLL             (unsigned long)(base+0x00)	/* when LCR.DLAB = 1 */
#define UART_DLH             (unsigned long)(base+0x04)	/* when LCR.DLAB = 1 */
#define UART_EFR             (unsigned long)(base+0x08)	/* when LCR = 0xbf */
#define UART_XON1            (unsigned long)(base+0x10)	/* when LCR = 0xbf */
#define UART_XON2            (unsigned long)(base+0x14)	/* when LCR = 0xbf */
#define UART_XOFF1           (unsigned long)(base+0x18)	/* when LCR = 0xbf */
#define UART_XOFF2           (unsigned long)(base+0x1c)	/* when LCR = 0xbf */
#define UART_AUTOBAUD_EN     (unsigned long)(base+0x20)
#define UART_HIGHSPEED       (unsigned long)(base+0x24)
#define UART_SAMPLE_COUNT    (unsigned long)(base+0x28)
#define UART_SAMPLE_POINT    (unsigned long)(base+0x2c)
#define UART_AUTOBAUD_REG    (unsigned long)(base+0x30)
#define UART_RATE_FIX_AD     (unsigned long)(base+0x34)
#define UART_AUTOBAUD_SAMPLE (unsigned long)(base+0x38)
#define UART_GUARD           (unsigned long)(base+0x3c)
#define UART_ESCAPE_DAT      (unsigned long)(base+0x40)
#define UART_ESCAPE_EN       (unsigned long)(base+0x44)
#define UART_SLEEP_EN        (unsigned long)(base+0x48)
#define UART_DMA_EN          (unsigned long)(base+0x4c)
#define UART_RXTRI_AD        (unsigned long)(base+0x50)
#define UART_FRACDIV_L       (unsigned long)(base+0x54)
#define UART_FRACDIV_M       (unsigned long)(base+0x58)
#define UART_FCR_RD          (unsigned long)(base+0x5C)
#define UART_ACTIVE_EN       (unsigned long)(base+0x60)
#define UART_RX_SEL          (unsigned long)(base+0xB0)
#define UART_SLEEP_REQ       (unsigned long)(base+0xB4)
#define UART_SLEEP_ACK       (unsigned long)(base+0xB8)

/* system level, not related to hardware */
#define UST_DUMMY_READ              (1 << 31)
/*---------------------------------------------------------------------------*/
/* For MT6589, both RX and TX will not use port to send or receive data */
/* IER */
/* RX buffer conatins data int. */
#define UART_IER_ERBFI              (1 << 0)
/* TX FIFO threshold trigger int. */
#define UART_IER_ETBEI              (1 << 1)
/* BE, FE, PE, or OE int. */
#define UART_IER_ELSI               (1 << 2)
/* CTS change (DCTS) int. */
#define UART_IER_EDSSI              (1 << 3)
/* When set "1", enable flow control triggered */
/* by RX FIFO full when VFIFO_EN is set. */
#define UART_IER_VFF_FC_EN          (1 << 4)
#define UART_IER_XOFFI              (1 << 5)
#define UART_IER_RTSI               (1 << 6)
#define UART_IER_CTSI               (1 << 7)

#define UART_IER_ALL_INTS (UART_IER_ERBFI|UART_IER_ETBEI|UART_IER_ELSI|\
	UART_IER_EDSSI|UART_IER_XOFFI|UART_IER_RTSI|UART_IER_CTSI)
#define UART_IER_HW_NORMALINTS\
	(UART_IER_ERBFI|UART_IER_ELSI|UART_IER_EDSSI|UART_IER_VFF_FC_EN)
#define UART_IER_HW_ALLINTS\
	(UART_IER_ERBFI|UART_IER_ETBEI|UART_IER_ELSI|UART_IER_EDSSI)
/*---------------------------------------------------------------------------*/
/* FCR */
#define UART_FCR_FIFOE              (1 << 0)
#define UART_FCR_CLRR               (1 << 1)
#define UART_FCR_CLRT               (1 << 2)
#define UART_FCR_DMA1               (1 << 3)
#define UART_FCR_RXFIFO_1B_TRI      (0 << 6)
#define UART_FCR_RXFIFO_6B_TRI      (1 << 6)
#define UART_FCR_RXFIFO_12B_TRI     (2 << 6)
#define UART_FCR_RXFIFO_RX_TRI      (3 << 6)
#define UART_FCR_TXFIFO_1B_TRI      (0 << 4)
#define UART_FCR_TXFIFO_4B_TRI      (1 << 4)
#define UART_FCR_TXFIFO_8B_TRI      (2 << 4)
#define UART_FCR_TXFIFO_14B_TRI     (3 << 4)

#define UART_FCR_FIFO_INIT	(UART_FCR_FIFOE|UART_FCR_CLRR|UART_FCR_CLRT)
#define UART_FCR_NORMAL         (UART_FCR_FIFO_INIT | \
				     UART_FCR_TXFIFO_4B_TRI| \
				     UART_FCR_RXFIFO_12B_TRI)
/*---------------------------------------------------------------------------*/
/* LCR */
#define UART_LCR_BREAK              (1 << 6)
#define UART_LCR_DLAB               (1 << 7)

#define UART_WLS_5                  (0 << 0)
#define UART_WLS_6                  (1 << 0)
#define UART_WLS_7                  (2 << 0)
#define UART_WLS_8                  (3 << 0)
#define UART_WLS_MASK               (3 << 0)

#define UART_1_STOP                 (0 << 2)
#define UART_2_STOP                 (1 << 2)
#define UART_1_5_STOP               (1 << 2)	/* Only when WLS=5 */
#define UART_STOP_MASK              (1 << 2)

#define UART_NONE_PARITY            (0 << 3)
#define UART_ODD_PARITY             (0x1 << 3)
#define UART_EVEN_PARITY            (0x3 << 3)
#define UART_MARK_PARITY            (0x5 << 3)
#define UART_SPACE_PARITY           (0x7 << 3)
#define UART_PARITY_MASK            (0x7 << 3)
/*---------------------------------------------------------------------------*/
/* MCR */
#define UART_MCR_DTR                (1 << 0)
#define UART_MCR_RTS                (1 << 1)
#define UART_MCR_OUT1               (1 << 2)
#define UART_MCR_OUT2               (1 << 3)
#define UART_MCR_LOOP               (1 << 4)
#define UART_MCR_DCM_EN             (1 << 5)	/* MT6589 move to bit5 */
#define UART_MCR_XOFF               (1 << 7)	/* read only */
#define UART_MCR_NORMAL	            (UART_MCR_DTR|UART_MCR_RTS)
/*---------------------------------------------------------------------------*/
/* LSR */
#define UART_LSR_DR                 (1 << 0)
#define UART_LSR_OE                 (1 << 1)
#define UART_LSR_PE                 (1 << 2)
#define UART_LSR_FE                 (1 << 3)
#define UART_LSR_BI                 (1 << 4)
#define UART_LSR_THRE               (1 << 5)
#define UART_LSR_TEMT               (1 << 6)
#define UART_LSR_FIFOERR            (1 << 7)
/*---------------------------------------------------------------------------*/
/* MSR */
#define UART_MSR_DCTS               (1 << 0)
#define UART_MSR_DDSR               (1 << 1)
#define UART_MSR_TERI               (1 << 2)
#define UART_MSR_DDCD               (1 << 3)
#define UART_MSR_CTS                (1 << 4)
#define UART_MSR_DSR                (1 << 5)
#define UART_MSR_RI                 (1 << 6)
#define UART_MSR_DCD                (1 << 7)
/*---------------------------------------------------------------------------*/
/* EFR */
#define UART_EFR_EN                 (1 << 4)
#define UART_EFR_AUTO_RTS           (1 << 6)
#define UART_EFR_AUTO_CTS           (1 << 7)
#define UART_EFR_SW_CTRL_MASK       (0xf << 0)

#define UART_EFR_NO_SW_CTRL         (0)
#define UART_EFR_NO_FLOW_CTRL       (0)
#define UART_EFR_AUTO_RTSCTS        (UART_EFR_AUTO_RTS|UART_EFR_AUTO_CTS)
/* TX/RX XON1/XOFF1 flow control */
#define UART_EFR_XON1_XOFF1         (0xa)
/* TX/RX XON2/XOFF2 flow control */
#define UART_EFR_XON2_XOFF2         (0x5)
/* TX/RX XON1,2/XOFF1,2 flow control */
#define UART_EFR_XON12_XOFF12       (0xf)

#define UART_EFR_XON1_XOFF1_MASK    (0xa)
#define UART_EFR_XON2_XOFF2_MASK    (0x5)
/*---------------------------------------------------------------------------*/
/* IIR (Read Only) */
#define UART_IIR_NO_INT_PENDING     (0x01)
#define UART_IIR_RLS                (0x06) /* Receiver Line Status */
#define UART_IIR_RDA                (0x04) /* Receive Data Available */
#define UART_IIR_CTI                (0x0C) /* Character Timeout Indicator */
/* Transmit Holding Register Empty */
#define UART_IIR_THRE               (0x02)
#define UART_IIR_MS                 (0x00) /* Check Modem Status Register */
#define UART_IIR_SW_FLOW_CTRL       (0x10) /* Receive XOFF characters */
#define UART_IIR_HW_FLOW_CTRL       (0x20) /* CTS or RTS Rising Edge */
#define UART_IIR_FIFO_EN            (0xc0)
#define UART_IIR_INT_MASK           (0x3f)
/*---------------------------------------------------------------------------*/
/* RateFix */
#define UART_RATE_FIX               (1 << 0)
/* #define UART_AUTORATE_FIX           (1 << 1) */
#define UART_FREQ_SEL               (1 << 1)

#define UART_RATE_FIX_13M           (1 << 0) /* means UARTclk = APBclk / 4 */
#define UART_AUTORATE_FIX_13M       (1 << 1)
#define UART_FREQ_SEL_13M           (1 << 2)
#define UART_RATE_FIX_ALL_13M       (UART_RATE_FIX_13M|UART_AUTORATE_FIX_13M| \
					UART_FREQ_SEL_13M)

#define UART_RATE_FIX_26M           (0 << 0) /* means UARTclk = APBclk / 2 */
#define UART_AUTORATE_FIX_26M       (0 << 1)
#define UART_FREQ_SEL_26M           (0 << 2)

#define UART_RATE_FIX_16M25         (UART_FREQ_SEL|UART_RATE_FIX)

#define UART_RATE_FIX_32M5          (UART_RATE_FIX)
/*---------------------------------------------------------------------------*/
/* Autobaud sample */
#define UART_AUTOBADUSAM_13M         7
#define UART_AUTOBADUSAM_26M        15
#define UART_AUTOBADUSAM_52M        31
/* #define UART_AUTOBADUSAM_52M        29 */ /* CHECKME! 28 or 29 ? */
#define UART_AUTOBAUDSAM_58_5M      31	/* CHECKME! 31 or 32 ? */
/*---------------------------------------------------------------------------*/
/* DMA enable */
#define UART_RX_DMA_EN              (1 << 0)
#define UART_TX_DMA_EN              (1 << 1)
#define UART_TO_CNT_AUTORST         (1 << 2)
/*---------------------------------------------------------------------------*/
/* Escape character*/
#define UART_ESCAPE_CH              0x77
/*---------------------------------------------------------------------------*/
/* Request UART to sleep*/
#define UART_CLK_OFF_REQ			(1 << 0)
/*---------------------------------------------------------------------------*/
/* UART sleep ack*/
#define UART_CLK_OFF_ACK			(1 << 0)
/*---------------------------------------------------------------------------*/
/* Debugging */
struct uart_iir_reg {
	u32 NINT:1;
	u32 ID:5;
	u32 FIFOE:2;
	u32 dummy:24;
};
/*---------------------------------------------------------------------------*/
#ifndef CONFIG_OF
#define VFF_BASE_CH_S           (6)
#define VFF_BASE_CH(n)          (AP_DMA_BASE+0x0080*(n+1+VFF_BASE_CH_S))
#endif
#define VFF_INT_FLAG(_b)        (_b+0x0000)
#define VFF_INT_EN(_b)          (_b+0x0004)
#define VFF_EN(_b)              (_b+0x0008)
#define VFF_RST(_b)             (_b+0x000C)
#define VFF_STOP(_b)            (_b+0x0010)
#define VFF_FLUSH(_b)           (_b+0x0014)
#define VFF_ADDR(_b)            (_b+0x001C)
#define VFF_LEN(_b)             (_b+0x0024)
#define VFF_THRE(_b)            (_b+0x0028)
#define VFF_WPT(_b)             (_b+0x002C)
#define VFF_RPT(_b)             (_b+0x0030)
#define VFF_W_INT_BUF_SIZE(_b)  (_b+0x0034)
#define VFF_INT_BUF_SIZE(_b)    (_b+0x0038)
#define VFF_VALID_SIZE(_b)      (_b+0x003C)
#define VFF_LEFT_SIZE(_b)       (_b+0x0040)
#define VFF_DEBUG_STATUS(_b)    (_b+0x0050)
#define VFF_4G_DRAM_SUPPORT(_b)	(_b+0x0054)
#define VFF_VPORT_BASE          0xF7070000
#define VFF_VPORT_CH(id)        (VFF_VPORT_BASE + (id) * 0x00000080)
/*---------------------------------------------------------------------------*/
/*VFF_INT_FLAG */
#define VFF_RX_INT_FLAG0_B      (1 << 0) /*rx_vff_valid_size >= rx_vff_thre */
/* when UART issues flush to DMA and all data in*/
/* UART VFIFO is transferred to VFF */
#define VFF_RX_INT_FLAG1_B      (1 << 1)
#define VFF_TX_INT_FLAG0_B      (1 << 0) /*tx_vff_left_size >= tx_vff_thrs */
#define VFF_INT_FLAG_CLR_B      (0 << 0)
/*VFF_INT_EN*/
#define VFF_RX_INT_EN0_B        (1 << 0) /*rx_vff_valid_size >= rx_vff_thre */
/* when UART issues flush to DMA and all data in*/
/* UART VFIFO is transferred to VFF */
#define VFF_RX_INT_EN1_B        (1 << 1)
#define VFF_TX_INT_EN_B         (1 << 0) /*tx_vff_left_size >= tx_vff_thrs */
#define VFF_INT_EN_CLR_B        (0 << 0)
/*VFF_RST*/
#define VFF_WARM_RST_B          (1 << 0)
#define VFF_HARD_RST_B          (1 << 1)
/*VFF_EN*/
#define VFF_EN_B                (1 << 0)
/*VFF_STOP*/
#define VFF_STOP_B              (1 << 0)
#define VFF_STOP_CLR_B          (0 << 0)
/*VFF_FLUSH*/
#define VFF_FLUSH_B             (1 << 0)
#define VFF_FLUSH_CLR_B         (0 << 0)
/* tx_vff_left_size >= tx_vff_thrs */
#define VFF_TX_THRE(n)          ((n)*7/8)
/* trigger level of rx vfifo */
#define VFF_RX_THRE(n)          ((n)*3/4)
/*---------------------------------------------------------------------------*/
#endif				/* MTK_UART_H */
