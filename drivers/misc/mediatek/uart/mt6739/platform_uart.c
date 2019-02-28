/*
 * (C) Copyright 2008
 * MediaTek <www.mediatek.com>
 * MingHsien Hsieh <minghsien.hsieh@mediatek.com>
 *
 * MTK UART Driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
/*---------------------------------------------------------------------------*/
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial_core.h>
#include <linux/serial.h>

#if defined(CONFIG_MTK_LEGACY) && !defined(CONFIG_FPGA_EARLY_PORTING)
#include "mach/mt_gpio.h"
#include <cust_gpio_usage.h>
#endif /* defined(CONFIG_MTK_LEGACY) && !defined (CONFIG_FPGA_EARLY_PORTING)*/

#include <linux/delay.h>
#include "include/mtk_uart.h"
#include "include/mtk_uart_intf.h"
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/ratelimit.h>
#include <mt-plat/mtk_lpae.h>
#include "mtk_spm_resource_req.h"

#if !defined(CONFIG_MTK_LEGACY)
/* #include <mach/mt_typedefs.h> */

struct pinctrl *ppinctrl_uart[UART_NR];
/* pinctrl-names from dtsi.GPIO operations: rx set, rx clear, tx set, tx clear */
char *uart_gpio_cmds[UART_NR][4] = {
	{"uart0_rx_set", "uart0_rx_clear", "uart0_tx_set", "uart0_tx_clear"},
	{"uart1_rx_set", "uart1_rx_clear", "uart1_tx_set", "uart1_tx_clear"},
#if 0
#if !defined(CONFIG_FPGA_EARLY_PORTING)
	{"uart2_rx_set", "uart2_rx_clear", "uart2_tx_set", "uart2_tx_clear"},
	{"uart3_rx_set", "uart3_rx_clear", "uart3_tx_set", "uart3_tx_clear"},
#endif				/* !defined (CONFIG_FPGA_EARLY_PORTING) */
#endif
};

void set_uart_pinctrl(int idx, struct pinctrl *ppinctrl)
{
	pr_debug("[UART%d][CCF]%s(%d,%p), UART_NR:%d\n", idx, __func__, idx, ppinctrl, UART_NR);
	if (idx >= UART_NR)
		return;
	ppinctrl_uart[idx] = ppinctrl;
}
#endif				/* !defined(CONFIG_MTK_LEGACY) */

#if !defined(CONFIG_FPGA_EARLY_PORTING)
/* struct clk *clk_uart_main; */
struct clk *clk_uart_dma;
void set_uart_dma_clk(int idx, struct clk *dma_clk)
{
	pr_debug("[UART%d][CCF]enabled clk_uart_dma:%p\n", idx, dma_clk);
	clk_uart_dma = dma_clk;
}
#endif

#ifdef ENABLE_RAW_DATA_DUMP
static void save_tx_raw_data(struct mtk_uart *uart, void *addr);
static void reset_rx_raw_data(struct mtk_uart *uart);
static void save_rx_raw_data(struct mtk_uart *uart, const unsigned char *chars, size_t size);
volatile unsigned int stop_update;
unsigned int curr_record = -1;
volatile unsigned int curr_idx;
#define			RECORD_NUMBER	10
#define			RECORD_LENGTH	1032
unsigned char uart_history[RECORD_NUMBER][RECORD_LENGTH];
unsigned int uart_history_cnt[RECORD_NUMBER];
spinlock_t tx_history_lock, rx_history_lock;

unsigned int curr_rx_record = -1;
volatile unsigned int curr_rx_idx;
unsigned char uart_rx_history[RECORD_NUMBER][RECORD_LENGTH];
unsigned int uart_rx_history_cnt[RECORD_NUMBER];
#endif
/*---------------------------------------------------------------------------*/
static struct mtk_uart_setting mtk_uart_default_settings[] = {
	{
	 /* .tx_mode = UART_NON_DMA, .rx_mode = UART_RX_VFIFO_DMA, .dma_mode = UART_DMA_MODE_0, */
	 .tx_mode = UART_TX_VFIFO_DMA, .rx_mode = UART_RX_VFIFO_DMA, .dma_mode = UART_DMA_MODE_0,
	 /* .tx_mode = UART_NON_DMA, .rx_mode = UART_NON_DMA, .dma_mode = UART_DMA_MODE_0, */
	 .tx_trig = UART_FCR_TXFIFO_1B_TRI, .rx_trig = UART_FCR_RXFIFO_12B_TRI,

	 /* .uart_base = AP_UART0_BASE, .irq_num = UART0_IRQ_BIT_ID, .irq_sen = MT_LEVEL_SENSITIVE, */
	 .sysrq = FALSE, .hw_flow = TRUE, .vff = TRUE,
	 },
	{
	 .tx_mode = UART_NON_DMA, .rx_mode = UART_NON_DMA, .dma_mode = UART_DMA_MODE_0,
	 .tx_trig = UART_FCR_TXFIFO_1B_TRI, .rx_trig = UART_FCR_RXFIFO_12B_TRI,

	 /* .uart_base = AP_UART1_BASE, .irq_num = UART1_IRQ_BIT_ID, .irq_sen = MT_LEVEL_SENSITIVE, */
	 .sysrq = FALSE, .hw_flow = TRUE, .vff = TRUE,
	 },
#if 0
	{
	 .tx_mode = UART_TX_VFIFO_DMA, .rx_mode = UART_RX_VFIFO_DMA, .dma_mode = UART_DMA_MODE_0,
	 .tx_trig = UART_FCR_TXFIFO_1B_TRI, .rx_trig = UART_FCR_RXFIFO_12B_TRI,

	 /* .uart_base = AP_UART2_BASE, .irq_num = UART2_IRQ_BIT_ID, .irq_sen = MT_LEVEL_SENSITIVE, */
	 .set_bit = PDN_FOR_UART3, .clr_bit = PDN_FOR_UART3, .pll_id = PDN_FOR_UART3,
	 .sysrq = FALSE, .hw_flow = FALSE, .vff = TRUE,	/* UART3 */
	 },
	{
	 .tx_mode = UART_NON_DMA, .rx_mode = UART_NON_DMA, .dma_mode = UART_DMA_MODE_0,
	 .tx_trig = UART_FCR_TXFIFO_1B_TRI, .rx_trig = UART_FCR_RXFIFO_12B_TRI,

	 /* .uart_base = AP_UART3_BASE, .irq_num = UART3_IRQ_BIT_ID, .irq_sen = MT_LEVEL_SENSITIVE, */
	 .set_bit = PDN_FOR_UART4, .clr_bit = PDN_FOR_UART4, .pll_id = PDN_FOR_UART4,
	 .sysrq = FALSE, .hw_flow = FALSE, .vff = FALSE,	/* UART4 */
	 },
#endif
};

/*---------------------------------------------------------------------------*/
static unsigned long mtk_uart_evt_mask[] = {
	DBG_EVT_NONE,
	DBG_EVT_NONE,
	DBG_EVT_NONE,
	DBG_EVT_NONE,
};

/*---------------------------------------------------------------------------*/
static unsigned long mtk_uart_lsr_status[] = {
	0,			/* UART1 */
	0,			/* UART2 */
	0,			/* UART3 */
	0,			/* UART4 */
};

/*---------------------------------------------------------------------------*/
#if defined(CONFIG_MTK_SERIAL_MODEM_TEST)
    /* #define HW_MISC     (CONFIG_BASE+0x0020)    // mtk does NOT has this register */
    /* unsigned char mask[UART_NR] = { 1 << 3, 1 << 4, 1 << 5, 1 << 6}; */
static unsigned int modem_uart[UART_NR] = { 1, 0, 0, 1 };
#endif
/*---------------------------------------------------------------------------*/
/* uart control blocks */
/*---------------------------------------------------------------------------*/
struct mtk_uart_setting *get_uart_default_settings(int idx)
{
	return &mtk_uart_default_settings[idx];
}

/*---------------------------------------------------------------------------*/
void set_uart_default_settings(int idx)
{
	struct device_node *node = NULL;
	unsigned int irq_info[3] = { 0, 0, 0 };
	u32 phys_base;

	switch (idx) {
	case 0:
		node = of_find_node_by_name(NULL, "apuart0");
		break;
	case 1:
		node = of_find_node_by_name(NULL, "apuart1");
		break;
#if (UART_NR > 2)
	case 2:
		node = of_find_node_by_name(NULL, "apuart2");
		break;
#endif
#if (UART_NR > 3)
	case 3:
		node = of_find_node_by_name(NULL, "apuart3");
		break;
#endif
	default:
		break;
	}

	if (node) {
		/* iomap registers */
		mtk_uart_default_settings[idx].uart_base = (unsigned long)of_iomap(node, 0);
		/* get IRQ ID */
		mtk_uart_default_settings[idx].irq_num = irq_of_parse_and_map(node, 0);
	}

	/* phys registers */
	if (of_property_read_u32_index(node, "reg", 0, &phys_base))
		pr_debug("[UART%d] get phys regs from DTS fail!!\n", idx);

	mtk_uart_default_settings[idx].uart_phys_base = phys_base;

	/* get the interrupt line behaviour */
	if (of_property_read_u32_array(node, "interrupts", irq_info, ARRAY_SIZE(irq_info)))
		pr_debug("[UART%d] get irq flags from DTS fail!!\n", idx);

	mtk_uart_default_settings[idx].irq_flags = (unsigned long)irq_info[2];
	pr_debug("[UART%d] phys_regs=0x%lx, regs=0x%lx, irq=%d, irq_flags=0x%lx\n", idx,
		 mtk_uart_default_settings[idx].uart_phys_base, mtk_uart_default_settings[idx].uart_base,
		 mtk_uart_default_settings[idx].irq_num, mtk_uart_default_settings[idx].irq_flags);
}

/*---------------------------------------------------------------------------*/
void *get_apdma_uart0_base(void)
{
	struct device_node *node = NULL;
	void *base;

	node = of_find_node_by_name(NULL, "apuart0");
	base = of_iomap(node, 1);

	pr_debug("[UART] apdma uart0 base=0x%p\n", base);
	return base;
}

/*---------------------------------------------------------------------------*/
unsigned int get_uart_vfifo_irq_id(int idx)
{
	struct device_node *node = NULL;
	unsigned int irq_id;

	switch (idx) {
	case 0:
	case 1:
		node = of_find_node_by_name(NULL, "apuart0");
		break;
	case 2:
	case 3:
		node = of_find_node_by_name(NULL, "apuart1");
		break;
	case 4:
	case 5:
		node = of_find_node_by_name(NULL, "apuart2");
		break;
	case 6:
	case 7:
		node = of_find_node_by_name(NULL, "apuart3");
		break;
	default:
		break;
	}
	if (idx % 2 == 0)
		irq_id = irq_of_parse_and_map(node, 1);
	else
		irq_id = irq_of_parse_and_map(node, 2);
	pr_debug("[UART_DMA%d] irq=%d\n", idx, irq_id);

	return irq_id;
}

/*---------------------------------------------------------------------------*/
unsigned long get_uart_evt_mask(int idx)
{
	return mtk_uart_evt_mask[idx];
}

/*---------------------------------------------------------------------------*/
void set_uart_evt_mask(int idx, int value)
{
	mtk_uart_evt_mask[idx] = value;
}

/*---------------------------------------------------------------------------*/
unsigned long get_uart_lsr_status(int idx)
{
	return mtk_uart_lsr_status[idx];
}

/*---------------------------------------------------------------------------*/
void set_uart_lsr_status(int idx, int value)
{
	mtk_uart_lsr_status[idx] = value;
}

/*---------------------------------------------------------------------------*/
unsigned int get_modem_uart(int idx)
{
#if defined(CONFIG_MTK_SERIAL_MODEM_TEST)
	return modem_uart[idx];
#else
	return 0;
#endif
}

/*---------------------------------------------------------------------------*/
#ifdef UART_FCR_USING_SW_BACK_UP
static inline void __write_fcr_register(struct mtk_uart *uart, u32 data)
{
	unsigned long base = uart->base;

	uart->fcr_back_up = data & (~(3 << 1));
	reg_sync_writel(data, UART_FCR);
}

static inline void sync_write_fcr_register(struct mtk_uart *uart, u32 data)
{
	unsigned long base = uart->base;

	uart->fcr_back_up = data & (~(3 << 1));
	reg_sync_writel(data, UART_FCR);
}

static inline u32 __read_fcr_register(struct mtk_uart *uart)
{
	return uart->fcr_back_up;
}

static inline void __set_fcr_register(struct mtk_uart *uart, u32 mask)
{
	unsigned long base = uart->base;
	u32 new_setting = (uart->fcr_back_up) | mask;

	uart->fcr_back_up = new_setting & (~(3 << 1));
	reg_sync_writel(new_setting, UART_FCR);
}

static inline void __clr_fcr_register(struct mtk_uart *uart, u32 mask)
{
	unsigned long base = uart->base;
	u32 new_setting = (uart->fcr_back_up) & (~mask);

	uart->fcr_back_up = new_setting & (~(3 << 1));
	reg_sync_writel(new_setting, UART_FCR);
}
#else
static inline void __write_fcr_register(struct mtk_uart *uart, u32 data)
{
	unsigned long base = uart->base;

	reg_sync_writel(data, UART_FCR);
}

static inline void sync_write_fcr_register(struct mtk_uart *uart, u32 data)
{
	unsigned long base = uart->base;

	reg_sync_writel(data, UART_FCR);
}

static inline u32 __read_fcr_register(struct mtk_uart *uart)
{
	unsigned long base = uart->base;

	return UART_READ32(UART_FCR_RD);
}

static inline void __set_fcr_register(struct mtk_uart *uart, u32 mask)
{
	unsigned long base = uart->base;
	u32 new_setting = UART_READ32(UART_FCR_RD) | mask;

	reg_sync_writel(new_setting, UART_FCR);
}

static inline void __clr_fcr_register(struct mtk_uart *uart, u32 mask)
{
	unsigned long base = uart->base;
	u32 new_setting = UART_READ32(UART_FCR_RD) & (~mask);

	reg_sync_writel(new_setting, UART_FCR);
}
#endif				/* End of UART_FCR_USING_SW_BACK_UP */
/*---------------------------------------------------------------------------*/
static inline void dump_reg(struct mtk_uart *uart, const char *caller)
{
#ifdef ENABLE_DEBUG
	unsigned long flags;
	unsigned long base = uart->base;
	u32 lcr = UART_READ32(UART_LCR);
	u32 uratefix = UART_READ32(UART_RATE_FIX_AD);
	u32 uhspeed = UART_READ32(UART_HIGHSPEED);
	u32 usamplecnt = UART_READ32(UART_SAMPLE_COUNT);
	u32 usamplepnt = UART_READ32(UART_SAMPLE_POINT);
	u32 udll, udlh;
	u32 ier = UART_READ32(UART_IER);

	spin_lock_irqsave(&mtk_console_lock, flags);
	reg_sync_writel((lcr | UART_LCR_DLAB), UART_LCR);
	udll = UART_READ32(UART_DLL);
	udlh = UART_READ32(UART_DLH);
	mb();			/* make sure the DLL/DLH have been read */
	reg_sync_writel(lcr, UART_LCR);	/* DLAB end */
	spin_unlock_irqrestore(&mtk_console_lock, flags);
	mb();

	MSG(CFG, "%s: RATEFIX(%02X); HSPEED(%02X); CNT(%02X); PNT(%02X); DLH(%02X), DLL(%02X), IER(%02X)\n",
	    caller, uratefix, uhspeed, usamplecnt, usamplepnt, udlh, udll, ier);
#endif
}

void dump_uart_reg(void)
{
	struct mtk_uart *uart;
	unsigned int i;
	unsigned long base;
	u32 lsr, escape_en;

	for (i = 0; i < UART_NR; i++) {
		uart = &mtk_uarts[i];
		base = uart->base;
		if (uart->poweron_count > 0) {
			lsr = UART_READ32(UART_LSR);
			escape_en = UART_READ32(UART_ESCAPE_EN);
			pr_debug("[UART%d] LSR=0x%x   ESCAPE_EN=0x%x\n", uart->nport, lsr, escape_en);
		} else
			pr_debug("[UART%d] clock is off\n", uart->nport);
	}

}

/*---------------------------------------------------------------------------*/
void mtk_uart_console_setting_switch(struct mtk_uart *uart)
{
#ifdef CONFIG_MTK_SERIAL_CONSOLE
	/* if(uart->nport == 0){ */  /* UART1 as log port */
	uart->setting->tx_mode = UART_NON_DMA;
	uart->setting->rx_mode = UART_NON_DMA;
	uart->tx_mode = UART_NON_DMA;
	uart->rx_mode = UART_NON_DMA;
	mtk_uart_enable_dpidle(uart);
	/* } */
#endif
}

/******************************************************************************
 * Virtual FIFO implementation
******************************************************************************/
#if defined(ENABLE_VFIFO)
/*---------------------------------------------------------------------------*/
int mtk_uart_vfifo_enable(struct mtk_uart *uart, struct mtk_uart_vfifo *vfifo)
{
	unsigned long base = uart->base;

	if (!vfifo) {
		MSG(ERR, "null\n");
		return -EINVAL;
	} else if (vfifo->type != UART_RX_VFIFO && vfifo->type != UART_TX_VFIFO) {
		MSG(ERR, "unknown type: %d\n", vfifo->type);
		return -EINVAL;
	}
	/*
	 * NOTE: For FCR is a read only register reason,
	 *       special read/write/set/clr function need to use
	 */
	/*UART_SET_BITS(UART_FCR_FIFO_INIT, UART_FCR);*/
	/*UART_CLR_BITS(UART_FCR_DMA1, UART_FCR);*/
	__set_fcr_register(uart, UART_FCR_FIFO_INIT);
	__clr_fcr_register(uart, UART_FCR_DMA1);

	if (vfifo->type == UART_RX_VFIFO)
		UART_SET_BITS(UART_RX_DMA_EN | UART_TO_CNT_AUTORST, UART_DMA_EN);
	else if (vfifo->type == UART_TX_VFIFO)
		UART_SET_BITS(UART_TX_DMA_EN, UART_DMA_EN);
	mb();
	return 0;
}

/*---------------------------------------------------------------------------*/
int mtk_uart_vfifo_disable(struct mtk_uart *uart, struct mtk_uart_vfifo *vfifo)
{
	unsigned long base = uart->base;

	if (!vfifo) {
		MSG(ERR, "null\n");
		return -EINVAL;
	} else if (vfifo->type != UART_RX_VFIFO && vfifo->type != UART_TX_VFIFO) {
		MSG(ERR, "unknown type: %d\n", vfifo->type);
		return -EINVAL;
	} else if (vfifo->type == UART_RX_VFIFO) {
		UART_CLR_BITS(UART_RX_DMA_EN | UART_TO_CNT_AUTORST, UART_DMA_EN);
	} else if (vfifo->type == UART_TX_VFIFO) {
		UART_CLR_BITS(UART_TX_DMA_EN, UART_DMA_EN);
	}
	mb();
	return 0;
}

/*---------------------------------------------------------------------------*/
void mtk_uart_vfifo_enable_tx_intr(struct mtk_uart *uart)
{
	reg_sync_writel(VFF_TX_INT_EN_B, VFF_INT_EN(uart->tx_vfifo->base));
}

/*---------------------------------------------------------------------------*/
void mtk_uart_vfifo_disable_tx_intr(struct mtk_uart *uart)
{
	reg_sync_writel(0x00, VFF_INT_EN(uart->tx_vfifo->base));
}

/*---------------------------------------------------------------------------*/
void mtk_uart_vfifo_clear_tx_intr(struct mtk_uart_vfifo *vfifo)
{
	reg_sync_writel(0x00, VFF_INT_FLAG(vfifo->base));
}

/*---------------------------------------------------------------------------*/
void mtk_uart_vfifo_clear_rx_intr(struct mtk_uart_vfifo *vfifo)
{
	reg_sync_writel(0x03, VFF_INT_FLAG(vfifo->base));
}

/*---------------------------------------------------------------------------*/
void mtk_uart_vfifo_enable_rx_intr(struct mtk_uart *uart)
{
	reg_sync_writel(VFF_RX_INT_EN0_B | VFF_RX_INT_EN1_B, VFF_INT_EN(uart->rx_vfifo->base));
}

/*---------------------------------------------------------------------------*/
void mtk_uart_vfifo_disable_rx_intr(struct mtk_uart *uart)
{
	reg_sync_writel(0x00, VFF_INT_EN(uart->rx_vfifo->base));
}

/*---------------------------------------------------------------------------*/
int mtk_uart_vfifo_is_full(struct mtk_uart_vfifo *vfifo)
{
	return (UART_READ32(VFF_LEFT_SIZE(vfifo->base)) <= 16) ? (1) : (0);
}

/*---------------------------------------------------------------------------*/
int mtk_uart_vfifo_is_empty(struct mtk_uart_vfifo *vfifo)
{
	return (UART_READ32(VFF_VALID_SIZE(vfifo->base)) == 0) ? (1) : (0);
}

/*---------------------------------------------------------------------------*/
void mtk_uart_vfifo_write_byte(struct mtk_uart *uart, unsigned int byte)
{
	void *addr, *base = uart->tx_vfifo->base;
	unsigned int wpt = UART_READ32(VFF_WPT(base));

	addr = (void *)((wpt & 0xffff) + uart->tx_vfifo->addr);
	reg_sync_writeb((unsigned char)byte, addr);
	mb();			/* make sure write point updated after VFIFO written. */
#ifdef ENABLE_RAW_DATA_DUMP
	save_tx_raw_data(uart, addr);
#endif
	if ((wpt & 0xffff) == (UART_READ32(VFF_LEN(base)) - 1))
		reg_sync_writel((~wpt) & 0x10000, VFF_WPT(base));
	else
		reg_sync_writel(wpt + 1, VFF_WPT(base));
}

/*---------------------------------------------------------------------------*/
unsigned int mtk_uart_vfifo_read_byte(struct mtk_uart *uart)
{
	void *addr, *base = uart->rx_vfifo->base;
	unsigned int ch;

	addr = (void *)(UART_READ16(VFF_RPT(base)) + uart->rx_vfifo->addr);
	ch = UART_READ8(addr);
	mb();			/* make sure read point updated after VFIFO read. */
	if (UART_READ16(VFF_RPT(base)) == (UART_READ32(VFF_LEN(base)) - 1))
		reg_sync_writel(~(UART_READ32(VFF_RPT(base))) & 0x10000, VFF_RPT(base));
	else
		reg_sync_writel(UART_READ32(VFF_RPT(base)) + 1, VFF_RPT(base));

	return ch;
}

/*---------------------------------------------------------------------------*/
int mtk_uart_vfifo_get_counts(struct mtk_uart_vfifo *vfifo)
{
	return UART_READ32(VFF_VALID_SIZE(vfifo->base));
}

/*---------------------------------------------------------------------------*/
void mtk_uart_tx_vfifo_flush(struct mtk_uart *uart, int timeout)
{
	struct mtk_uart_dma *dma = &uart->dma_tx;
	struct mtk_uart_vfifo *vfifo = dma->vfifo;
	void *base = vfifo->base;

#ifdef ENABE_HRTIMER_FLUSH
	if (UART_READ32(VFF_FLUSH(base)) == 0) {
		reg_sync_writel(VFF_FLUSH_B, VFF_FLUSH(base));
		if (!timeout)
			hrtimer_try_to_cancel(&vfifo->flush);
		MSG(MSC, "flush [%5X.%5X]\n", UART_READ32(VFF_RPT(base)), UART_READ32(VFF_WPT(base)));
	} else {
		/*the ns used to transfer the data in TX VFIFO */
		u32 size = UART_READ32(VFF_VALID_SIZE(base));
		s64 t = size * 10 * (NSEC_PER_SEC / uart->baudrate);
		ktime_t cur = ktime_get();
		ktime_t nxt = ktime_add_ns(cur, t);

		hrtimer_try_to_cancel(&vfifo->flush);
		hrtimer_start(&vfifo->flush, nxt, HRTIMER_MODE_ABS);
#if defined(ENABLE_VFIFO_DEBUG)
		{
			struct timespec a = ktime_to_timespec(cur);
			struct timespec b = ktime_to_timespec(nxt);

			MSG(MSC, "start: [%ld %ld] [%ld %ld] [%d %lld]\n",
			    a.tv_sec, a.tv_nsec, b.tv_sec, b.tv_nsec, size, t);
		}
#endif
	}
#else
	if (UART_READ32(VFF_FLUSH(base)) == 0) {
		reg_sync_writel(VFF_FLUSH_B, VFF_FLUSH(base));
		MSG(MSC, "flush [%5X.%5X]\n", UART_READ32(VFF_RPT(base)), UART_READ32(VFF_WPT(base)));
	}
#endif				/* ENABE_HRTIMER_FLUSH */
}

/*---------------------------------------------------------------------------*/
static void mtk_uart_dma_vfifo_tx_tasklet_byte(unsigned long arg)
{
	struct mtk_uart *uart = (struct mtk_uart *)arg;
	struct uart_port *port = &uart->port;
	/* struct mtk_uart_dma *dma = &uart->dma_tx; */
	struct mtk_uart_vfifo *vfifo = uart->tx_vfifo;
	struct circ_buf *xmit = &port->state->xmit;
	unsigned int len, count, size, left, chk = 0;
	ktime_t begin, end;
	struct timespec a, b;

	size = CIRC_CNT_TO_END(xmit->head, xmit->tail, UART_XMIT_SIZE);
	left = vfifo->size - mtk_uart_vfifo_get_counts(vfifo);
	left = (left > 16) ? (left - 16) : (0);	/*prevent from CPU lock */
	len = count = left < size ? left : size;

	if (!len) {
		chk = 1;
		MSG(DMA, ">>>>> zero size <<<<<\n");
	}

	DGBUF_INIT(vfifo);
	begin = ktime_get();
	a = ktime_to_timespec(begin);
	while (len--) {
		/*DMA limitation.
		 *  Workaround: Polling flush bit to zero, set 1s timeout
		 */
		while (UART_READ32(VFF_FLUSH(vfifo->base))) {
			end = ktime_get();
			b = ktime_to_timespec(end);
			if ((b.tv_sec - a.tv_sec) > 1 || ((b.tv_sec - a.tv_sec) == 1 && b.tv_nsec > a.tv_nsec)) {
				pr_debug("[UART%d] Polling flush timeout\n", port->line);
				return;
			}
		}
		DGBUF_PUSH_CH(vfifo, (char)xmit->buf[xmit->tail]);
		uart->write_byte(uart, xmit->buf[xmit->tail]);
		xmit->tail = (xmit->tail + 1) & (UART_XMIT_SIZE - 1);
		port->icount.tx++;
	}
#if defined(ENABLE_VFIFO_DEBUG)
	if (UART_DEBUG_EVT(DBG_EVT_DMA) && UART_DEBUG_EVT(DBG_EVT_BUF)) {
		char str[4] = { 0 };

		if (count >= 4) {
			str[0] = vfifo->cur->dat[0];
			str[1] = vfifo->cur->dat[1];
			str[2] = vfifo->cur->dat[vfifo->cur->idx - 2];
			str[3] = vfifo->cur->dat[vfifo->cur->idx - 1];
		} else {
			int idx;

			for (idx = 0; idx < count; idx++)
				str[idx] = vfifo->cur->dat[idx];
			for (; idx < 4; idx++)
				str[idx] = 0;
		}
		MSG(DMA, "TX[%4d]: %4d/%4d [%05X-%05X] (%02X %02X .. %02X %02X)\n",
		    size, count, left, UART_READ32(VFF_WPT(vfifo->base)), UART_READ32(VFF_RPT(vfifo->base)),
		    str[0], str[1], str[2], str[3]);
	} else {
		MSG(DMA, "TX[%4d]: %4d/%4d [%05X-%05X]\n",
		    size, count, left, UART_READ32(VFF_WPT(vfifo->base)), UART_READ32(VFF_RPT(vfifo->base)));
	}
#endif

#if defined(ENABLE_VFIFO_DEBUG)
	if (UART_DEBUG_EVT(DBG_EVT_DAT) && UART_DEBUG_EVT(DBG_EVT_BUF)) {
		int i;

		pr_debug("[UART%d_TX] %4d bytes:", uart->nport, vfifo->cur->idx);
		for (i = 0; i < vfifo->cur->idx; i++) {
			if (i % 16 == 0)
				pr_debug("\n");
			pr_debug("%.2x ", (unsigned char)vfifo->cur->dat[i]);
		}
		pr_debug("\n");
	}
#endif

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(port);

}

/*---------------------------------------------------------------------------*/
/*static int mtk_uart_vfifo_write_string(struct mtk_uart *uart, const unsigned char *chars, size_t size)
*{
*    void *addr, *base = uart->tx_vfifo->base;
*    unsigned int wpt = UART_READ32(VFF_WPT(base));
*    unsigned int num_to_end;
*
*    addr = (void*)((wpt&0xffff)+uart->tx_vfifo->addr);
*    num_to_end = UART_READ32(VFF_LEN(base)) - (wpt&0xffff);
*    if(num_to_end >= size){
*	memcpy(addr, chars, size);
*	mb();                                                  //make sure write point updated after VFIFO written.
*	reg_sync_writel( wpt+(unsigned int)size, VFF_WPT(base));
*    }else{
*	memcpy(addr, chars, num_to_end);
*	memcpy(uart->tx_vfifo->addr, &chars[num_to_end], (unsigned int)size - num_to_end);
*	mb();                                                  //make sure write point updated after VFIFO written.
*	wpt = ((~wpt)&0x10000)+ (unsigned int)size - num_to_end;
*	reg_sync_writel(wpt, VFF_WPT(base));
*    }
*
*    return size;
*}
*/
/*---------------------------------------------------------------------------*/
/*static void mtk_uart_dma_vfifo_tx_tasklet_str(unsigned long arg)
*{
*    struct mtk_uart *uart = (struct mtk_uart *)arg;
*    struct uart_port   *port = &uart->port;
*    //struct mtk_uart_dma *dma = &uart->dma_tx;
*    struct mtk_uart_vfifo *vfifo = uart->tx_vfifo;
*    struct circ_buf    *xmit = &port->state->xmit;
*    unsigned int len, count, size, left, chk = 0;
*
*    size = CIRC_CNT_TO_END(xmit->head, xmit->tail, UART_XMIT_SIZE);
*    left = vfifo->size - mtk_uart_vfifo_get_counts(vfifo);
*    left = (left > 16) ? (left-16) : (0);
*    len  = count = left < size ? left : size;
*
*    if (!len) {
*	chk = 1;
*	MSG(DMA,">>>>> zero size <<<<<\n");
*    }
*
*    DGBUF_INIT(vfifo);
*
*    mtk_uart_vfifo_write_string(uart, &xmit->buf[xmit->tail], size);
*    DGBUF_PUSH_STR(vfifo, &xmit->buf[xmit->tail], size);
*    xmit->tail = (xmit->tail+size) & (UART_XMIT_SIZE - 1);
*    port->icount.tx += size;
*
*#if defined(ENABLE_VFIFO_DEBUG)
*    if (UART_DEBUG_EVT(DBG_EVT_DMA) && UART_DEBUG_EVT(DBG_EVT_BUF)) {
*	char str[4] = {0};
*	if (count >= 4) {
*	    str[0] = vfifo->cur->dat[0];
*	    str[1] = vfifo->cur->dat[1];
*	    str[2] = vfifo->cur->dat[vfifo->cur->idx-2];
*	    str[3] = vfifo->cur->dat[vfifo->cur->idx-1];
*	} else {
*	    int idx;
*	    for (idx = 0; idx < count; idx++)
*		str[idx] = vfifo->cur->dat[idx];
*	    for (; idx < 4; idx++)
*		str[idx] = 0;
*	}
*	MSG(DMA, "TX[%4d]: %4d/%4d [%05X-%05X] (%02X %02X .. %02X %02X)\n",
*	    size, count, left, UART_READ32(VFF_WPT(vfifo->base)), UART_READ32(VFF_RPT(vfifo->base)),
*	    str[0], str[1], str[2], str[3]);
*    } else {
*	MSG(DMA, "TX[%4d]: %4d/%4d [%05X-%05X]\n",
*	    size, count, left, UART_READ32(VFF_WPT(vfifo->base)), UART_READ32(VFF_RPT(vfifo->base)));
*    }
*#endif
*
*#if defined(ENABLE_VFIFO_DEBUG)
*    if (UART_DEBUG_EVT(DBG_EVT_DAT) && UART_DEBUG_EVT(DBG_EVT_BUF)) {
*	int i;
*	pr_debug("[UART%d_TX] %4d bytes:", uart->nport, vfifo->cur->idx);
*	for (i = 0; i < vfifo->cur->idx; i++) {
*	    if (i % 16 == 0)
*		pr_debug("\n");
*	    pr_debug("%.2x ", (unsigned char)vfifo->cur->dat[i]);
*	}
*	pr_debug("\n");
*    }
*#endif
*
*    if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
*	uart_write_wakeup(port);
*
*}
*/
/*---------------------------------------------------------------------------*/
void mtk_uart_dma_vfifo_tx_tasklet(unsigned long arg)
{
	struct mtk_uart *uart = (struct mtk_uart *)arg;
	struct uart_port *port = &uart->port;
	struct mtk_uart_dma *dma = &uart->dma_tx;
	struct mtk_uart_vfifo *vfifo = uart->tx_vfifo;
	struct circ_buf *xmit = &port->state->xmit;
	int txcount = port->icount.tx;
	void *base = vfifo->base;
	unsigned long flags;

	spin_lock_irqsave(&vfifo->iolock, flags);
	atomic_inc(&vfifo->entry);

	while (UART_READ32(VFF_LEFT_SIZE(base)) >= vfifo->trig) {
		/* deal with x_char first */
		if (unlikely(port->x_char)) {
			MSG(INFO, "detect x_char!!\n");
			uart->write_byte(uart, port->x_char);
			port->icount.tx++;
			port->x_char = 0;
			break;
		}
		if (uart_circ_empty(xmit) || uart_tx_stopped(port)) {
			uart->pending_tx_reqs = 0;
			atomic_set(&dma->free, 1);
			complete(&dma->done);
			break;
		}
		mtk_uart_dma_vfifo_tx_tasklet_byte(arg);
	}
	if (txcount != port->icount.tx) {
		mtk_uart_vfifo_enable_tx_intr(uart);
		mtk_uart_tx_vfifo_flush(uart, 0);
	}

	atomic_dec(&vfifo->entry);
	spin_unlock_irqrestore(&vfifo->iolock, flags);
}

/*---------------------------------------------------------------------------*/
/*static void mtk_uart_dma_vfifo_rx_tasklet_byte(unsigned long arg)
*{
*    struct mtk_uart *uart = (struct mtk_uart*)arg;
*    struct uart_port   *port = &uart->port;
*    struct mtk_uart_vfifo *vfifo = uart->rx_vfifo;
*    struct tty_struct *tty = uart->port.state->port.tty;
*    int count, left;
*    unsigned int ch, flag, status;
*    unsigned long flags;
*
*    MSG_FUNC_ENTRY();
*
*    count = left = mtk_uart_vfifo_get_counts(vfifo);
*
*    spin_lock_irqsave(&port->lock, flags);
*
*    DGBUF_INIT(vfifo);
*    while (!mtk_uart_vfifo_is_empty(vfifo) && count > 0) {
*
*	status = uart->read_status(uart);
*	status = mtk_uart_filter_line_status(uart);
*
*	ch = uart->read_byte(uart);
*	flag = TTY_NORMAL;
*
*	if (status & UART_LSR_BI) {
*	    MSG(INFO, "Break Interrupt!!!\n");
*	    port->icount.brk++;
*	    if (uart_handle_break(port))
*		continue;
*	    flag = TTY_BREAK;
*	} else if (status & UART_LSR_PE) {
*	    MSG(INFO, "Parity Error!!!\n");
*	    port->icount.parity++;
*	    flag = TTY_PARITY;
*	} else if (status & UART_LSR_FE) {
*	    MSG(INFO, "Frame Error!!!\n");
*	    port->icount.frame++;
*	    flag = TTY_FRAME;
*	} else if (status & UART_LSR_OE) {
*	    MSG(INFO, "Overrun!!!\n");
*	    port->icount.overrun++;
*	    flag = TTY_OVERRUN;
*	}
*	port->icount.rx++;
*	count--;
*	DGBUF_PUSH_CH(vfifo, ch);
*	if (!tty_insert_flip_char(tty, ch, flag))
*	    MSG(ERR, "tty_insert_flip_char: no space\n");
*    }
*    tty_flip_buffer_push(tty);
*
*#if defined(ENABLE_VFIFO_DEBUG)
*    if (UART_DEBUG_EVT(DBG_EVT_DMA) && UART_DEBUG_EVT(DBG_EVT_BUF)) {
*	char str[4] = {0};
*	if (count >= 4) {
*	    str[0] = vfifo->cur->dat[0];
*	    str[1] = vfifo->cur->dat[1];
*	    str[2] = vfifo->cur->dat[vfifo->cur->idx-2];
*	    str[3] = vfifo->cur->dat[vfifo->cur->idx-1];
*	} else {
*	    int idx;
*	    for (idx = 0; idx < count; idx++)
*		str[idx] = vfifo->cur->dat[idx];
*	    for (; idx < 4; idx++)
*		str[idx] = 0;
*	}
*	MSG(DMA, "RX[%4d]: %4d bytes from VFIFO [%4d] (%02X %02X .. %02X %02X) %d\n",
*	    left, left - count, mtk_uart_vfifo_get_counts(vfifo), str[0], str[1], str[2], str[3],
*	    UART_READ32(VFF_VALID_SIZE(vfifo->base)));
*    } else {
*	MSG(DMA, "RX[%4d]: %4d bytes from VFIFO [%4d] %d\n",
*	    left, left - count, mtk_uart_vfifo_get_counts(vfifo),
*	    UART_READ32(VFF_VALID_SIZE(vfifo->base)));
*    }
*#endif
*
*    spin_unlock_irqrestore(&port->lock, flags);
*
*#if defined(ENABLE_VFIFO_DEBUG)
*   if (UART_DEBUG_EVT(DBG_EVT_DAT) && UART_DEBUG_EVT(DBG_EVT_BUF)) {
*	int i;
*	pr_debug("[UART%d_RX] %4d bytes:", uart->nport, vfifo->cur->idx);
*
*	for (i = 0; i < vfifo->cur->idx; i++) {
*	    if (i % 16 == 0)
*		pr_debug("\n");
*	    pr_debug("%.2x ", (unsigned char)vfifo->cur->dat[i]);
*	}
*	pr_debug("\n");
*
*    }
*#endif
*}
*/
/*---------------------------------------------------------------------------*/
/* A duplicate of tty_insert_flip_string.                                    */
/* The only difference is the function will accept one extra variable for    */
/* indicating the current line status.                                       */
/*---------------------------------------------------------------------------*/
static int mtk_uart_tty_insert_flip_string(struct mtk_uart *uart, const unsigned char *chars, size_t size)
{
	struct tty_struct *tty = uart->port.state->port.tty;
	struct uart_port *port = &uart->port;
	u32 status, flag;
	int copied = 0;

	status = uart->read_status(uart);
	status = mtk_uart_filter_line_status(uart);

	flag = TTY_NORMAL;
	/* error handling routine */
	if (status & UART_LSR_BI) {
		MSG(ERR, "Break Interrupt!!\n");
		port->icount.brk++;
		if (uart_handle_break(port))
			return 0;
		flag = TTY_BREAK;
	} else if (status & UART_LSR_PE) {
		MSG(ERR, "Parity Error!!!\n");
		port->icount.parity++;
		flag = TTY_PARITY;
	} else if (status & UART_LSR_FE) {
		MSG(ERR, "Frame Error!!!\n");
		port->icount.frame++;
		flag = TTY_FRAME;
	} else if (status & UART_LSR_OE) {
		MSG(ERR, "Overrun!!!\n");
		port->icount.overrun++;
		flag = TTY_OVERRUN;
	}
#ifdef ENABLE_RAW_DATA_DUMP
	save_rx_raw_data(uart, chars, size);
#endif

	if (flag == TTY_NORMAL) {
		copied = tty_insert_flip_string(tty->port, chars, size);
	} else {
		MSG(ERR, "error occurs\n");
		copied += tty_insert_flip_string(tty->port, chars, size - 1);
		copied += tty_insert_flip_char(tty->port, chars[size - 1], flag);
	}
	port->icount.rx += copied;
	return copied;
}

/*---------------------------------------------------------------------------*/
static void mtk_uart_dma_vfifo_rx_tasklet_str(unsigned long arg)
{
	struct mtk_uart *uart = (struct mtk_uart *)arg;
	struct uart_port *port = &uart->port;
	struct mtk_uart_vfifo *vfifo = uart->rx_vfifo;
	struct tty_struct *tty = uart->port.state->port.tty;
	int count, left;
	unsigned int rxptr, txptr, txreg, rxreg;
	unsigned long flags;
	unsigned char *ptr;
	void *base = vfifo->base;

	MSG_FUNC_ENTRY();

	spin_lock_irqsave(&port->lock, flags);

	rxreg = UART_READ32(VFF_RPT(base));
	txreg = UART_READ32(VFF_WPT(base));
	rxptr = rxreg & 0x0000FFFF;
	txptr = txreg & 0x0000FFFF;
	count = left = ((rxreg ^ txreg) & 0x00010000) ? (txptr + vfifo->size - rxptr) : (txptr - rxptr);

	DGBUF_INIT(vfifo);

#ifdef ENABLE_RAW_DATA_DUMP
	reset_rx_raw_data(uart);
#endif

	if ((rxptr + count) <= txptr) {
		ptr = (unsigned char *)(rxptr + vfifo->addr);
		mtk_uart_tty_insert_flip_string(uart, ptr, count);
		DGBUF_PUSH_STR(vfifo, ptr, count);
	} else {
		ptr = (unsigned char *)(rxptr + vfifo->addr);
		mtk_uart_tty_insert_flip_string(uart, ptr, vfifo->size - rxptr);
		DGBUF_PUSH_STR(vfifo, ptr, vfifo->size - rxptr);
		if (txptr) {
			ptr = (unsigned char *)(vfifo->addr);
			mtk_uart_tty_insert_flip_string(uart, ptr, txptr);
			DGBUF_PUSH_STR(vfifo, ptr, txptr);
		}
	}
	mb();			/* make sure read point updated after VFIFO read. */
	reg_sync_writel(txreg, VFF_RPT(base));
	tty_flip_buffer_push(tty->port);

#if defined(ENABLE_VFIFO_DEBUG)
	if (UART_DEBUG_EVT(DBG_EVT_DMA) && UART_DEBUG_EVT(DBG_EVT_BUF)) {
		char str[4] = { 0 };

		if (count >= 4) {
			str[0] = vfifo->cur->dat[0];
			str[1] = vfifo->cur->dat[1];
			str[2] = vfifo->cur->dat[vfifo->cur->idx - 2];
			str[3] = vfifo->cur->dat[vfifo->cur->idx - 1];
		} else {
			int idx;

			for (idx = 0; idx < count; idx++)
				str[idx] = vfifo->cur->dat[idx];
			for (; idx < 4; idx++)
				str[idx] = 0;
		}
		MSG(DMA, "RX[%4d]: [%5X..%5X] [%5X..%5X] (%02X %02X .. %02X %02X) [%d]\n",
		    left, rxreg, txreg, UART_READ32(VFF_RPT(base)), UART_READ32(VFF_WPT(base)),
		    str[0], str[1], str[2], str[3], UART_READ32(VFF_FLUSH(base)));
	} else {
		MSG(DMA, "RX[%4d]: [%5X..%5X] [%5X..%5X] [%d] [%4X.%4X]\n",
		    left, rxreg, txreg, UART_READ32(VFF_RPT(base)), UART_READ32(VFF_WPT(base)),
		    UART_READ32(VFF_FLUSH(base)), UART_READ32(VFF_VALID_SIZE(base)), UART_READ32(VFF_LEFT_SIZE(base)));
	}
#endif
	spin_unlock_irqrestore(&port->lock, flags);

#if defined(ENABLE_VFIFO_DEBUG)
	if (UART_DEBUG_EVT(DBG_EVT_DAT) && UART_DEBUG_EVT(DBG_EVT_BUF)) {
		int i;

		pr_debug("[UART%d_RX] %4d bytes:", uart->nport, vfifo->cur->idx);

		for (i = 0; i < vfifo->cur->idx; i++) {
			if (i % 16 == 0)
				pr_debug("\n");
			pr_debug("%.2x ", (unsigned char)vfifo->cur->dat[i]);
		}
		pr_debug("\n");

	}
#endif
}

/*---------------------------------------------------------------------------*/
void mtk_uart_dma_vfifo_rx_tasklet(unsigned long arg)
{				/*the function will be called through dma irq or tasklet_schedule */
	struct mtk_uart *uart = (struct mtk_uart *)arg;
	struct mtk_uart_vfifo *vfifo = uart->rx_vfifo;
	unsigned long flags;

	MSG(DMA, "%d, %x, %x\n", uart->read_allow(uart), UART_READ32(VFF_VALID_SIZE(vfifo->base)), vfifo->trig);
	spin_lock_irqsave(&vfifo->iolock, flags);
	atomic_inc(&vfifo->entry);

	if (uart->read_allow(uart))
		mtk_uart_dma_vfifo_rx_tasklet_str(arg);

	atomic_dec(&vfifo->entry);
	spin_unlock_irqrestore(&vfifo->iolock, flags);
}

/*---------------------------------------------------------------------------*/
void mtk_uart_dma_setup(struct mtk_uart *uart, struct mtk_uart_dma *dma)
{
	void *base;

	if (!dma)
		return;

	if (dma->mode == UART_RX_VFIFO_DMA || dma->mode == UART_TX_VFIFO_DMA) {
		if (!dma->vfifo) {
			MSG(ERR, "null\n");
			return;
		}
		base = dma->vfifo->base;
		reg_sync_writel(dma->vfifo->dmahd, VFF_ADDR(base));
		reg_sync_writel(dma->vfifo->trig, VFF_THRE(base));
		reg_sync_writel(dma->vfifo->size, VFF_LEN(base));
		if (enable_4G())
			reg_sync_writel(0x01, VFF_4G_DRAM_SUPPORT(base));

		if (dma->vfifo->type == UART_RX_VFIFO)
			/* reg_sync_writel(VFF_RX_INT_EN0_B, VFF_INT_EN(base)); */
			reg_sync_writel(VFF_RX_INT_EN0_B | VFF_RX_INT_EN1_B, VFF_INT_EN(base));
		mb();
	}
}

/*---------------------------------------------------------------------------*/
int mtk_uart_dma_start(struct mtk_uart *uart, struct mtk_uart_dma *dma)
{
	void *base;

	MSG_FUNC_ENTRY();

	if (!dma)
		return -1;

	if (!atomic_read(&dma->free))
		return -1;

	if (dma->mode == UART_TX_VFIFO_DMA || dma->mode == UART_RX_VFIFO_DMA) {
		if (!dma->vfifo) {
			MSG(ERR, "null\n");
			return -EINVAL;
		}
		base = dma->vfifo->base;
		reg_sync_writel(VFF_INT_FLAG_CLR_B, VFF_INT_FLAG(base));
		reg_sync_writel(VFF_EN_B, VFF_EN(base));

		if (UART_READ32(VFF_EN(base)) != VFF_EN_B)
			MSG(ERR, "Start DMA fail\n");
	}

	atomic_set(&dma->free, 0);
	init_completion(&dma->done);

	return 0;
}

/*---------------------------------------------------------------------------*/
void mtk_uart_stop_dma(struct mtk_uart_dma *dma)
{
	int polling_cnt = 0;
	struct mtk_uart *uart = dma->uart;
	void *base;

	if (dma->mode == UART_RX_VFIFO_DMA || dma->mode == UART_TX_VFIFO_DMA) {
		MSG(DMA, "stop dma (%d)\n", dma->mode);
		if (!dma->vfifo) {
			MSG(ERR, "null\n");
			return;
		}
		base = dma->vfifo->base;

		/*set flush as 1 -> wait until flush is 0 */
		reg_sync_writel(VFF_FLUSH_CLR_B, VFF_FLUSH(base));
		while (UART_READ32(VFF_FLUSH(base))) {
			polling_cnt++;
			if (polling_cnt > 10000) {
				pr_debug("mtk_uart_stop_dma: polling VFF_FLUSH fail VFF_DEBUG_STATUS=0x%x\n",
					UART_READ32(VFF_DEBUG_STATUS(base)));
				break;
			}
		}

		polling_cnt = 0;
		/*set stop as 1 -> wait until en is 0 -> set stop as 0 */
		reg_sync_writel(VFF_STOP_B, VFF_STOP(base));
		while (UART_READ32(VFF_EN(base))) {
			polling_cnt++;
			if (polling_cnt > 10000) {
				pr_debug("mtk_uart_stop_dma: polling VFF_EN fail VFF_DEBUG_STATUS=0x%x\n",
					UART_READ32(VFF_DEBUG_STATUS(base)));
				break;
			}
		}
		reg_sync_writel(VFF_STOP_CLR_B, VFF_STOP(base));

		reg_sync_writel(VFF_INT_EN_CLR_B, VFF_INT_EN(base));
		reg_sync_writel(VFF_INT_FLAG_CLR_B, VFF_INT_FLAG(base));
	} else {
		MSG(ERR, "unknown mode: %d\n", dma->mode);
	}
}

/*---------------------------------------------------------------------------*/
void mtk_uart_reset_dma(struct mtk_uart_dma *dma)
{
	struct mtk_uart *uart = dma->uart;
	void *base;

	if (dma->mode == UART_RX_VFIFO_DMA || dma->mode == UART_TX_VFIFO_DMA) {
		if (!dma->vfifo) {
			MSG(ERR, "null\n");
			return;
		}
		base = dma->vfifo->base;
		/* mt65xx_req_vff_dma(dma->vfifo->ch, NULL, NULL); */
		reg_sync_writel(0, VFF_ADDR(base));
		reg_sync_writel(0, VFF_THRE(base));
		reg_sync_writel(0, VFF_LEN(base));
		/*set warm_rst as 1 -> wait until en is 0 */
		reg_sync_writel(VFF_WARM_RST_B, VFF_RST(base));
		while (UART_READ32(VFF_EN(base)))
			;

		/* Reset write point for tx dma */
		if (dma->mode == UART_TX_VFIFO_DMA)
			reg_sync_writel(0, VFF_WPT(base));
		else if (dma->mode == UART_RX_VFIFO_DMA)
			reg_sync_writel(0, VFF_RPT(base));
	} else
		MSG(ERR, "unknown mode: %d\n", dma->mode);
}
#endif				/*defined(ENABLE_VFIFO) */
/*---------------------------------------------------------------------------*/
void mtk_uart_fifo_init(struct mtk_uart *uart)
{
	/*
	 * NOTE: For FCR is a read only register reason,
	 *       special read/write/set/clr function need to use
	 */
	/* UART_SET_BITS(UART_FCR_FIFO_INIT, UART_FCR); */
	__set_fcr_register(uart, UART_FCR_FIFO_INIT);
	mb();
}

/*---------------------------------------------------------------------------*/
void mtk_uart_fifo_flush(struct mtk_uart *uart)
{
	/*
	 * NOTE: For FCR is a read only register reason,
	 *       special read/write/set/clr function need to use
	 */
	/* UART_SET_BITS(UART_FCR_CLRR | UART_FCR_CLRT, UART_FCR); */
	__set_fcr_register(uart, UART_FCR_CLRR | UART_FCR_CLRT);
	mb();
}

/*---------------------------------------------------------------------------*/
int mtk_uart_data_ready(struct mtk_uart *uart)
{
	if ((uart->read_status(uart) & UART_LSR_DR))
		return 1;
	else
		return 0;
}

/*---------------------------------------------------------------------------*/
void mtk_uart_fifo_set_trig(struct mtk_uart *uart, int tx_level, int rx_level)
{
	unsigned long base = uart->base;
	unsigned long tmp1;
	unsigned long flags;

	tmp1 = UART_READ32(UART_LCR);
	spin_lock_irqsave(&mtk_console_lock, flags);
	reg_sync_writel(0xbf, UART_LCR);
	UART_SET_BITS(UART_EFR_EN, UART_EFR);
	reg_sync_writel(tmp1, UART_LCR);
	spin_unlock_irqrestore(&mtk_console_lock, flags);
	MSG(INFO, "%s(EFR) =  %04X\n", __func__, UART_READ32(UART_EFR));

	/*
	 * NOTE: For FCR is a read only register reason,
	 *       special read/write/set/clr function need to use
	 */
	/* reg_sync_writel(UART_FCR_FIFO_INIT|tx_level|rx_level, UART_FCR); */
	sync_write_fcr_register(uart, UART_FCR_FIFO_INIT | tx_level | rx_level);
}

/*---------------------------------------------------------------------------*/
void mtk_uart_set_mode(struct mtk_uart *uart, int mode)
{
	/*
	 * NOTE: For FCR is a read only register reason,
	 *       special read/write/set/clr function need to use
	 */
	if (mode == UART_DMA_MODE_0) {
		/* UART_CLR_BITS(UART_FCR_DMA1, UART_FCR); */
		__clr_fcr_register(uart, UART_FCR_DMA1);
	} else if (mode == UART_DMA_MODE_1) {
		/* UART_SET_BITS(UART_FCR_DMA1, UART_FCR); */
		__set_fcr_register(uart, UART_FCR_DMA1);
	}
	mb();
}

/*---------------------------------------------------------------------------*/
void mtk_uart_set_auto_baud(struct mtk_uart *uart)
{
	unsigned long base = uart->base;

	MSG_FUNC_ENTRY();

	switch (uart->sysclk) {
	case MTK_SYSCLK_13:
		reg_sync_writel(UART_AUTOBADUSAM_13M, UART_AUTOBAUD_SAMPLE);
		break;
	case MTK_SYSCLK_26:
		reg_sync_writel(UART_AUTOBADUSAM_26M, UART_AUTOBAUD_SAMPLE);
		break;
	case MTK_SYSCLK_52:
		reg_sync_writel(UART_AUTOBADUSAM_52M, UART_AUTOBAUD_SAMPLE);
		break;
	default:
		pr_notice("SYSCLK = %ldMHZ doesn't support autobaud\n", uart->sysclk);
		return;
	}
	reg_sync_writel(0x01, UART_AUTOBAUD_EN);	/* Enable Auto Baud */
}

/*---------------------------------------------------------------------------*/
static void mtk_uart_cal_baud(struct mtk_uart *uart, int baudrate, int highspeed)
{
	unsigned long base = uart->base;
	u32 remainder, uartclk = 0, divisor = 0;
	u32 lcr = UART_READ32(UART_LCR);
	unsigned long flags;

#ifdef UART_USING_FIX_CLK_ENABLE
	if (baudrate <= 1000000) {	/* Using 16.25 fix clock */
		uartclk = uart->sysclk >> 2;
		reg_sync_writel(0x03, UART_RATE_FIX_AD);
	} else {		/* >1M, Using 65 clock */
		uartclk = uart->sysclk;
		reg_sync_writel(0x00, UART_RATE_FIX_AD);
	}
	if (highspeed == 3)
		UART_SET_BITS(UART_MCR_DCM_EN, UART_MCR);	/* Enable UART DCM */
	else
		UART_CLR_BITS(UART_MCR_DCM_EN, UART_MCR);	/* Disable UART DCM */
#else				/* UART_Fix_clk_DISABLE */
	uartclk = uart->sysclk;
	reg_sync_writel(0x00, UART_RATE_FIX_AD);
#endif				/* UART_USING_FIX_CLK_ENABLE */

	spin_lock_irqsave(&mtk_console_lock, flags);
	if (highspeed == 0) {
		/* uartclk = uart->sysclk; */
		/* reg_sync_writel(0x00, UART_RATE_FIX_AD); */
		reg_sync_writel(0x00, UART_HIGHSPEED);	/*divider is 16 */
		divisor = (uartclk >> 4) / (u32) baudrate;
		remainder = (uartclk >> 4) % (u32) baudrate;
		if (remainder >= (u32) (baudrate * 8))
			divisor += 1;
		reg_sync_writel(lcr | UART_LCR_DLAB, UART_LCR);
		reg_sync_writel((divisor & 0xFF), UART_DLL);
		reg_sync_writel(((divisor >> 8) & 0xFF), UART_DLH);
		reg_sync_writel(lcr, UART_LCR);
	} else if (highspeed == 1) {
		/* uartclk = uart->sysclk; */
		/* reg_sync_writel(0x00, UART_RATE_FIX_AD); */
		reg_sync_writel(0x01, UART_HIGHSPEED);	/*divider is 8 */
		divisor = (uartclk >> 3) / (u32) baudrate;
		remainder = (uartclk >> 3) % (u32) baudrate;
		if (remainder >= (u32) (baudrate * 4))
			divisor += 1;
		reg_sync_writel(lcr | UART_LCR_DLAB, UART_LCR);
		reg_sync_writel((divisor & 0xFF), UART_DLL);
		reg_sync_writel(((divisor >> 8) & 0xFF), UART_DLH);
		reg_sync_writel(lcr, UART_LCR);
	} else if (highspeed == 2) {
		/* uartclk = uart->sysclk; */
		/* reg_sync_writel(0x00, UART_RATE_FIX_AD); */
		reg_sync_writel(0x02, UART_HIGHSPEED);	/*divider is 4 */
		divisor = (uartclk >> 2) / (u32) baudrate;
		remainder = (uartclk >> 2) % (u32) baudrate;
		if (remainder >= (u32) (baudrate * 2))
			divisor += 1;
		reg_sync_writel(lcr | UART_LCR_DLAB, UART_LCR);
		reg_sync_writel((divisor & 0x00FF), UART_DLL);
		reg_sync_writel(((divisor >> 8) & 0x00FF), UART_DLH);
		reg_sync_writel(lcr, UART_LCR);
	} else if (highspeed == 3) {
		u32 sample_count, sample_point, high_div, tmp;
#if defined(ENABLE_FRACTIONAL)
		u32 fraction;
		u16 fraction_L_mapping[] = { 0, 1, 0x5, 0x15, 0x55, 0x57, 0x57, 0x77, 0x7F, 0xFF, 0xFF };
		u16 fraction_M_mapping[] = { 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0x3 };
#endif

		/* uartclk = uart->sysclk; */
		/* reg_sync_writel(0x00, UART_RATE_FIX_AD); */
		reg_sync_writel(0x03, UART_HIGHSPEED);
		tmp = (uartclk) / (u32) baudrate;
		high_div = (tmp >> 8) + 1;
		divisor = (uartclk) / (baudrate * high_div);

#if defined(ENABLE_FRACTIONAL)
		fraction = ((uartclk * 10) / baudrate) % 10;	/*count fraction to set fractoin register */
		reg_sync_writel(fraction_L_mapping[fraction], UART_FRACDIV_L);
		reg_sync_writel(fraction_M_mapping[fraction], UART_FRACDIV_M);
#else
		remainder = (uartclk) % (baudrate * high_div);
		if (remainder >= ((baudrate * high_div) >> 1))
			divisor += 1;
#endif

		sample_count = divisor - 1;
		sample_point = (sample_count - 1) >> 1;
		reg_sync_writel(lcr | UART_LCR_DLAB, UART_LCR);
		reg_sync_writel((high_div & 0x00FF), UART_DLL);
		reg_sync_writel(((high_div >> 8) & 0x00FF), UART_DLH);
		reg_sync_writel(lcr, UART_LCR);
		reg_sync_writel(sample_count, UART_SAMPLE_COUNT);
		reg_sync_writel(sample_point, UART_SAMPLE_POINT);
		/*
		 * NOTICE: We found some chip, that is using lower clock, may not have enough time to check stop bit.
		 * In order to improve compatibility, the guard time register
		 * is enabled which is used to extend the stop bit.
		 */
		if (baudrate >= 3000000)
			reg_sync_writel(0x12, UART_GUARD);
	}
	spin_unlock_irqrestore(&mtk_console_lock, flags);

	MSG(CFG, "BaudRate = %d, SysClk = %d, Divisor = %d, %04X/%04X\n", baudrate, uartclk, divisor,
	    UART_READ32(UART_IER), UART_READ32(UART_LCR));
	dump_reg(uart, __func__);
	mb();			/*to ensure the setting is written */
}

/*---------------------------------------------------------------------------*/
void mtk_uart_baud_setting(struct mtk_uart *uart, int baudrate)
{
	u32 uartclk;
#if defined(CONFIG_FPGA_EARLY_PORTING)
	u32 tmp_div;
#endif
	uartclk = uart->sysclk;

#if defined(CONFIG_FPGA_EARLY_PORTING)
	tmp_div = (uartclk) / (unsigned int)baudrate;
	if (tmp_div > 255)
		mtk_uart_cal_baud(uart, baudrate, 2);
	else
		mtk_uart_cal_baud(uart, baudrate, 3);
#else
	/* Fix clock, using new settings */
#ifdef UART_USING_FIX_CLK_ENABLE
	if (baudrate < 115200)
		mtk_uart_cal_baud(uart, baudrate, 0);
	else
		mtk_uart_cal_baud(uart, baudrate, 3);
#else				/* UART_Fix_Clock_DISABLE */
	if (baudrate <= 115200)
		mtk_uart_cal_baud(uart, baudrate, 0);
	else if (baudrate <= 460800)
		mtk_uart_cal_baud(uart, baudrate, 2);
	else
		mtk_uart_cal_baud(uart, baudrate, 3);
#endif				/* End of UART_DCM_CONFIG */
#endif
}

/*---------------------------------------------------------------------------*/
#if defined(ENABLE_DEBUG)
/*---------------------------------------------------------------------------*/
static u32 UART_READ_EFR(struct mtk_uart *uart)
{
	unsigned long base = uart->base;
	u32 efr, lcr = UART_READ32(UART_LCR);
	unsigned long flags;

	spin_lock_irqsave(&mtk_console_lock, flags);
	reg_sync_writel(0xbf, UART_LCR);
	efr = UART_READ32(UART_EFR);
	reg_sync_writel(lcr, UART_LCR);
	spin_unlock_irqrestore(&mtk_console_lock, flags);
	return efr;
}

/*---------------------------------------------------------------------------*/
#endif
/*---------------------------------------------------------------------------*/
void mtk_uart_set_flow_ctrl(struct mtk_uart *uart, int mode)
{
	unsigned long base = uart->base, old;
	unsigned int tmp = UART_READ32(UART_LCR);
	unsigned long flags;

	MSG(CFG, "%s: %04X\n", __func__, UART_READ_EFR(uart));

	spin_lock_irqsave(&mtk_console_lock, flags);
	switch (mode) {
	case UART_FC_NONE:
		reg_sync_writel(UART_ESCAPE_CH, UART_ESCAPE_DAT);
		reg_sync_writel(0x00, UART_ESCAPE_EN);
		reg_sync_writel(0xbf, UART_LCR);
		old = UART_READ32(UART_EFR);
		old &= ~(UART_EFR_AUTO_RTSCTS | UART_EFR_XON12_XOFF12);
		reg_sync_writel(old, UART_EFR);
		reg_sync_writel(tmp, UART_LCR);
		mtk_uart_disable_intrs(uart, UART_IER_XOFFI | UART_IER_RTSI | UART_IER_CTSI);
		break;
	case UART_FC_HW:
		reg_sync_writel(UART_ESCAPE_CH, UART_ESCAPE_DAT);
		reg_sync_writel(0x00, UART_ESCAPE_EN);
		UART_SET_BITS(UART_MCR_RTS, UART_MCR);
		reg_sync_writel(0xbf, UART_LCR);
		/*disable all flow control setting */
		old = UART_READ32(UART_EFR);
		old &= ~(UART_EFR_AUTO_RTSCTS | UART_EFR_XON12_XOFF12);
		reg_sync_writel(old, UART_EFR);
		/*enable hw flow control */
		old = UART_READ32(UART_EFR);
		reg_sync_writel(old | UART_EFR_AUTO_RTSCTS, UART_EFR);
		reg_sync_writel(tmp, UART_LCR);
		mtk_uart_disable_intrs(uart, UART_IER_XOFFI);
		mtk_uart_enable_intrs(uart, UART_IER_CTSI | UART_IER_RTSI);
		break;
	case UART_FC_SW:	/*MTK software flow control */
		reg_sync_writel(UART_ESCAPE_CH, UART_ESCAPE_DAT);
		reg_sync_writel(0x01, UART_ESCAPE_EN);
		reg_sync_writel(0xbf, UART_LCR);
		/*dsiable all flow control setting */
		old = UART_READ32(UART_EFR);
		old &= ~(UART_EFR_AUTO_RTSCTS | UART_EFR_XON12_XOFF12);
		reg_sync_writel(old, UART_EFR);
		/*enable sw flow control */
		old = UART_READ32(UART_EFR);
		reg_sync_writel(old | UART_EFR_XON1_XOFF1, UART_EFR);
		reg_sync_writel(START_CHAR(uart->port.state->port.tty), UART_XON1);
		reg_sync_writel(STOP_CHAR(uart->port.state->port.tty), UART_XOFF1);
		reg_sync_writel(tmp, UART_LCR);
		mtk_uart_disable_intrs(uart, UART_IER_CTSI | UART_IER_RTSI);
		mtk_uart_enable_intrs(uart, UART_IER_XOFFI);
		break;
	}
	spin_unlock_irqrestore(&mtk_console_lock, flags);
	mb();			/*to ensure the setting is written */
	uart->fctl_mode = mode;
}

/*---------------------------------------------------------------------------*/
void mtk_uart_power_up(struct mtk_uart *uart)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	struct mtk_uart_setting *setting;
	int clk_en_ret = 0;
	static DEFINE_RATELIMIT_STATE(ratelimit, 5 * HZ,  5);

	if (!uart || uart->nport >= UART_NR)
		return;

	setting = uart->setting;

	if (uart->poweron_count > 0) {
		MSG(FUC, "%s(%d)\n", __func__, uart->poweron_count);
	} else {
#ifdef POWER_FEATURE
		clk_en_ret = clk_enable(setting->clk_uart_main);
		if (clk_en_ret) {
			pr_notice("[UART%d][CCF]enable clk_uart_main failed. ret:%d, clk_main:%p\n", uart->nport,
			       clk_en_ret, setting->clk_uart_main);
		} else {
			if (__ratelimit(&ratelimit)) {
				pr_debug("[UART%d][CCF]enabled clk_uart_main:%p\n", uart->nport,
					setting->clk_uart_main);
			}
			if ((uart != console_port)
			    && (uart->tx_mode == UART_TX_VFIFO_DMA || uart->rx_mode == UART_RX_VFIFO_DMA)) {
				clk_en_ret = clk_enable(clk_uart_dma);
				if (clk_en_ret) {
					pr_notice("[UART%d][CCF]enable clk_uart_main failed. ret:%d, clk_dma:%p\n",
					       uart->nport, clk_en_ret, clk_uart_dma);
				} else {
					pr_debug("[UART%d][CCF]enabled clk_uart_dma:%p\n", uart->nport, clk_uart_dma);
				}
			}
		}
		uart->poweron_count++;
#endif
	}
	MSG(FUC, "%s(%d) => up\n", __func__, uart->poweron_count);
#endif				/* End of CONFIG_FPGA_EARLY_PORTING */
}

/*---------------------------------------------------------------------------*/
void mtk_uart_power_down(struct mtk_uart *uart)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	struct mtk_uart_setting *setting;
	static DEFINE_RATELIMIT_STATE(ratelimit, 5 * HZ,  5);

	setting = uart->setting;

	if (uart->nport >= UART_NR)
		return;

	if (uart->poweron_count == 0) {
		MSG(FUC, "%s(%d)\n", __func__, uart->poweron_count);
	} else {
#ifdef POWER_FEATURE
		if (__ratelimit(&ratelimit)) {
			pr_debug("[UART%d][CCF]disable clk_uart_main:%p\n", uart->nport,
				setting->clk_uart_main);
		}
		clk_disable(setting->clk_uart_main);
		if ((uart != console_port)
		    && (uart->tx_mode == UART_TX_VFIFO_DMA || uart->rx_mode == UART_RX_VFIFO_DMA)) {
			clk_disable(clk_uart_dma);
			pr_debug("[UART%d][CCF]disable clk_uart_dma:%p\n", uart->nport, clk_uart_dma);
		}
		uart->poweron_count--;
#endif
		MSG(FUC, "%s(%d) => dn\n", __func__, uart->poweron_count);
	}
#endif				/* End of CONFIG_FPGA_EARLY_PORTING */
}

/*---------------------------------------------------------------------------*/
void mtk_uart_config(struct mtk_uart *uart, int datalen, int stop, int parity)
{
	unsigned long base = uart->base;
	unsigned int val = 0;

	switch (datalen) {
	case 5:
		val |= UART_WLS_5;
		break;
	case 6:
		val |= UART_WLS_6;
		break;
	case 7:
		val |= UART_WLS_7;
		break;
	case 8:
	default:
		val |= UART_WLS_8;
		break;
	}

	if (stop == 2 || (datalen == 5 && stop == 1))
		val |= UART_2_STOP;

	if (parity == 1)
		val |= UART_ODD_PARITY;
	else if (parity == 2)
		val |= UART_EVEN_PARITY;

	reg_sync_writel(val, UART_LCR);
}

/*---------------------------------------------------------------------------*/
unsigned int mtk_uart_read_status(struct mtk_uart *uart)
{
	unsigned long base = uart->base;

	uart->line_status = UART_READ32(UART_LSR);
	return uart->line_status;
}

/*---------------------------------------------------------------------------*/
unsigned int mtk_uart_read_allow(struct mtk_uart *uart)
{
	return uart->line_status & UART_LSR_DR;
}

/*---------------------------------------------------------------------------*/
/* Note:
 * 1. FIFO mode:
 *    -THRE=1 : when free space in FIFO is reduced blow its trigger level
 *    -THRE=0 : when free space in FIFO is more than its trigger level
 * 2. non-FIFO mode:
 *    -THRE=1 : when tx holding register is empty
 *    -THRE=0 : when tx holding register is not empty
 */
unsigned int mtk_uart_write_allow(struct mtk_uart *uart)
{
	unsigned long base = uart->base;

	return UART_READ32(UART_LSR) & UART_LSR_THRE;
}

/*---------------------------------------------------------------------------*/
void mtk_uart_enable_intrs(struct mtk_uart *uart, long mask)
{				/*assume UART_EFR_EN is on */
	unsigned long base = uart->base;

	UART_SET_BITS(mask, UART_IER);
	mb();
}

/*---------------------------------------------------------------------------*/
void mtk_uart_disable_intrs(struct mtk_uart *uart, long mask)
{				/*assume UART_EFR_EN is on */
	unsigned long base = uart->base;

	UART_CLR_BITS(mask, UART_IER);
	mb();
}

/*---------------------------------------------------------------------------*/
unsigned int mtk_uart_read_byte(struct mtk_uart *uart)
{
	unsigned long base = uart->base;

	return UART_READ32(UART_RBR);
}

/*---------------------------------------------------------------------------*/
void mtk_uart_write_byte(struct mtk_uart *uart, unsigned int byte)
{
	unsigned long base = uart->base;

	reg_sync_writel(byte, UART_THR);
}

/*---------------------------------------------------------------------------*/
void mtk_uart_usb_rx_sel(unsigned int uart_port, unsigned int enable)
{
	unsigned long base = mtk_uart_default_settings[uart_port - 1].uart_base;

	reg_sync_writel(enable, UART_RX_SEL);
}

/*---------------------------------------------------------------------------*/
unsigned int mtk_uart_filter_line_status(struct mtk_uart *uart)
{
	struct uart_port *port = &uart->port;
	unsigned int status;
	unsigned int lsr = uart->line_status;

	mtk_uart_lsr_status[uart->nport] |= lsr;
	status = UART_LSR_BI | UART_LSR_PE | UART_LSR_FE | UART_LSR_OE;

#ifdef ENABLE_DEBUG
	if ((lsr & UART_LSR_BI) || (lsr & UART_LSR_PE) || (lsr & UART_LSR_FE) || (lsr & UART_LSR_OE)) {
		MSG(ERR, "LSR: BI=%d, FE=%d, PE=%d, OE=%d, DR=%d\n",
		    (lsr & UART_LSR_BI) >> 4, (lsr & UART_LSR_FE) >> 3,
		    (lsr & UART_LSR_PE) >> 2, (lsr & UART_LSR_OE) >> 1, lsr & UART_LSR_DR);
	}
#endif
	status &= port->read_status_mask;
	status &= ~port->ignore_status_mask;
	status &= lsr;

	return status;
}

/*---------------------------------------------------------------------------*/
int mtk_uart_get_interrupt(struct mtk_uart *uart)
{
	unsigned int intrs;
	unsigned long base = uart->base;

	intrs = UART_READ32(UART_IIR);
	return intrs;
}

/*---------------------------------------------------------------------------*/
void mtk_uart_intr_last_check(struct mtk_uart *uart, int intrs)
{
}

/*---------------------------------------------------------------------------*/
void mtk_uart_get_modem_status(struct mtk_uart *uart)
{
	unsigned long base = uart->base;
	struct uart_port *port = &uart->port;
	unsigned int status, delta;

	status = UART_READ32(UART_MSR);
	status &= UART_MSR_DSR | UART_MSR_CTS | UART_MSR_DCD | UART_MSR_RI;

	MSG(INFO, "MSR: DCD(%d), RI(%d), DSR(%d), CTS(%d)\n",
	    status & UART_MSR_DCD ? 1 : 0,
	    status & UART_MSR_RI ? 1 : 0, status & UART_MSR_DSR ? 1 : 0, status & UART_MSR_CTS ? 1 : 0);

	delta = status ^ uart->old_status;

	if (!delta)
		return;

	if (uart->ms_enable) {
		if (delta & UART_MSR_DCD)
			uart_handle_dcd_change(port, status & UART_MSR_DCD);
		if (delta & UART_MSR_CTS)
			uart_handle_cts_change(port, status & UART_MSR_CTS);
		if (delta & UART_MSR_DSR)
			port->icount.dsr++;
		if (delta & UART_MSR_RI)
			port->icount.rng++;
	}

	uart->old_status = status;
}

/*---------------------------------------------------------------------------*/
void mtk_uart_rx_pre_handler(struct mtk_uart *uart, int intrs)
{
	unsigned long base = uart->base;
	u32 tmp, lsr_status;

	if (intrs == UART_IIR_CTI) {
		/* IMPORTANT: this is a fix for HW Bug.
		 * Without the function call, the RX data timeout interrupt will be
		 * triggered again and again.Hence, the purpose of this function call
		 * is to clear Rx data timeout interrupt
		 */
		tmp = UART_READ32(UART_DMA_EN);
#if defined(ENABLE_VFIFO)
		MSG(DMA, "rx timeout: %x, %4d\n", tmp, mtk_uart_vfifo_get_counts(uart->rx_vfifo));
#endif
		/* mtk_uart_dma_vfifo_rx_tasklet((unsigned long)uart); */
	} else if ((intrs == UART_IIR_RLS) && !uart->read_allow(uart)) {
		tmp = UART_READ32(UART_LSR);
		MSG(DMA, "LSR=%X\n", tmp);
		lsr_status = get_uart_lsr_status(uart->nport);
		lsr_status |= tmp;
		set_uart_lsr_status(uart->nport, lsr_status);
	} else {
#if defined(ENABLE_VFIFO)
		MSG(DMA, "RX = %4d, [%4x]\n", mtk_uart_vfifo_get_counts(uart->rx_vfifo), intrs);
#endif
	}
}

/* set the modem control lines. */
void mtk_uart_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
	struct mtk_uart *uart = (struct mtk_uart *)port;
	unsigned long base = uart->base;
	unsigned int val;

	val = UART_READ32(UART_MCR);

	if (mctrl & TIOCM_DTR)
		val |= UART_MCR_DTR;
	else
		val &= ~UART_MCR_DTR;

	if (mctrl & TIOCM_RTS)
		val |= UART_MCR_RTS;
	else
		val &= ~UART_MCR_RTS;

	if (mctrl & TIOCM_OUT1)
		val |= UART_MCR_OUT1;
	else
		val &= ~UART_MCR_OUT1;

	if (mctrl & TIOCM_OUT2)
		val |= UART_MCR_OUT2;
	else
		val &= ~UART_MCR_OUT2;

	if (mctrl & TIOCM_LOOP)
		val |= UART_MCR_LOOP;
	else
		val &= ~UART_MCR_LOOP;

	reg_sync_writel(val, UART_MCR);

	MSG(CFG, "MCR: DTR(%d), RTS(%d), OUT1(%d), OUT2(%d), LOOP(%d)\n",
	    val & UART_MCR_DTR ? 1 : 0,
	    val & UART_MCR_RTS ? 1 : 0,
	    val & UART_MCR_OUT1 ? 1 : 0, val & UART_MCR_OUT2 ? 1 : 0, val & UART_MCR_LOOP ? 1 : 0);
}

/*---------------------------------------------------------------------------*/
/* return the current state of modem contrl inputs */
unsigned int mtk_uart_get_mctrl(struct uart_port *port)
{
	struct mtk_uart *uart = (struct mtk_uart *)port;
	unsigned long base = uart->base;
	unsigned int status;
	unsigned int result = 0;

	status = UART_READ32(UART_MSR);

	MSG(INFO, "MSR: DCD(%d), RI(%d), DSR(%d), CTS(%d)\n",
	    status & UART_MSR_DCD ? 1 : 0,
	    status & UART_MSR_RI ? 1 : 0, status & UART_MSR_DSR ? 1 : 0, status & UART_MSR_CTS ? 1 : 0);

	if (status & UART_MSR_DCD)
		result |= TIOCM_CAR;	/* DCD. (data carrier detect) */
	if (status & UART_MSR_RI)
		result |= TIOCM_RI;
	if (status & UART_MSR_DSR)
		result |= TIOCM_DSR;
	if (status & UART_MSR_CTS)
		result |= TIOCM_CTS;

	status = UART_READ32(UART_MCR);

	MSG(INFO, "MSR: OUT1(%d), OUT2(%d), LOOP(%d)\n",
	    status & UART_MCR_OUT1 ? 1 : 0, status & UART_MCR_OUT2 ? 1 : 0, status & UART_MCR_LOOP ? 1 : 0);

	if (status & UART_MCR_OUT2)
		result |= TIOCM_OUT2;
	if (status & UART_MCR_OUT1)
		result |= TIOCM_OUT1;
	if (status & UART_MCR_LOOP)
		result |= TIOCM_LOOP;

	return result;
}

/*---------------------------------------------------------------------------*/
/* stop receiving characters
 * note: port->lock has been taken by serial core layer
 */
void mtk_uart_stop_rx(struct uart_port *port)
{
	struct mtk_uart *uart = (struct mtk_uart *)port;
	struct mtk_uart_dma *dma = &uart->dma_rx;

	MSG_FUNC_ENTRY();
	if (uart->rx_mode == UART_NON_DMA) {
		mtk_uart_disable_intrs(uart, UART_IER_ERBFI);
	} else {
#if defined(ENABLE_VFIFO)
		/* According to serial_core.c, stop_rx is to stop interrupt
		 * Hence, RX received interrupt and dma interrupt is clear
		 */
		mtk_uart_disable_intrs(uart, UART_IER_ERBFI);
		reg_sync_writel(VFF_INT_EN_CLR_B, VFF_INT_EN(dma->vfifo->base));
		atomic_set(&dma->free, 1);
		complete(&dma->done);
#endif
	}
	uart->rx_stop = 1;
}

/*---------------------------------------------------------------------------*/
/* control the transmission of a break signal */
void mtk_uart_break_ctl(struct uart_port *port, int break_state)
{
	struct mtk_uart *uart = (struct mtk_uart *)port;
	unsigned long base = uart->base;

	unsigned long flags;

	MSG_FUNC_ENTRY();

	spin_lock_irqsave(&port->lock, flags);

	if (break_state)
		UART_SET_BITS(UART_LCR_BREAK, UART_LCR);
	else
		UART_CLR_BITS(UART_LCR_BREAK, UART_LCR);
	mb();
	spin_unlock_irqrestore(&port->lock, flags);
}

/*---------------------------------------------------------------------------*/
#ifdef ATE_FACTORY_ENABLE
void mtk_uart_is_ate_factory_mode(struct mtk_uart *uart)
{
	if ((uart->nport == 0) && (get_boot_mode() == ATE_FACTORY_MODE)) {
		unsigned long base = uart->base;
		/* MD may set these bit, reset it */
		UART_CLR_BITS(UART_RX_DMA_EN | UART_TO_CNT_AUTORST | UART_TX_DMA_EN, UART_DMA_EN);
		mb();
	}
}
#endif				/* ATE_FACTORY_ENABLE */
/*---------------------------------------------------------------------------*/
void mtk_uart_enable_sleep(struct mtk_uart *uart)
{
	unsigned long base = uart->base;

	reg_sync_writel(0x1, UART_SLEEP_EN);
	pr_debug("SLEEP_EN = 0x%x\n", UART_READ32(UART_SLEEP_EN));
}

/*---------------------------------------------------------------------------*/

#ifdef ENABLE_RAW_DATA_DUMP
void mtk_uart_init_debug_spinlock(void)
{
	spin_lock_init(&tx_history_lock);
	spin_lock_init(&rx_history_lock);
}

void reset_tx_raw_data(struct mtk_uart *uart)
{
	unsigned long flags;

	if (uart->nport == 2) {
		spin_lock_irqsave(&tx_history_lock, flags);
		if (!stop_update) {
			curr_record++;
			curr_idx = 0;
			if (curr_record >= RECORD_NUMBER)
				curr_record = 0;
			uart_history_cnt[curr_record] = 0;
			for (curr_idx = 0; curr_idx < RECORD_LENGTH; curr_idx++)
				uart_history[curr_record][curr_idx] = 0;
			curr_idx = 0;
		}
		spin_unlock_irqrestore(&tx_history_lock, flags);
	}
}

static void save_tx_raw_data(struct mtk_uart *uart, void *addr)
{
	unsigned long flags;

	if (uart->nport == 2) {
		spin_lock_irqsave(&tx_history_lock, flags);
		if (!stop_update) {
			if (curr_idx < RECORD_LENGTH) {
				uart_history[curr_record][curr_idx] = UART_READ8(addr);
				curr_idx++;
				uart_history_cnt[curr_record] = curr_idx;
			}
		}
		spin_unlock_irqrestore(&tx_history_lock, flags);
	}
}

static void reset_rx_raw_data(struct mtk_uart *uart)
{
	unsigned long flags;

	if (uart->nport == 2) {
		spin_lock_irqsave(&rx_history_lock, flags);
		if (!stop_update) {
			curr_rx_record++;
			curr_rx_idx = 0;
			if (curr_rx_record >= RECORD_NUMBER)
				curr_rx_record = 0;
			uart_rx_history_cnt[curr_rx_record] = 0;
			for (curr_rx_idx = 0; curr_rx_idx < RECORD_LENGTH; curr_rx_idx++)
				uart_rx_history[curr_rx_record][curr_rx_idx] = 0;
			curr_rx_idx = 0;
		}
		spin_unlock_irqrestore(&rx_history_lock, flags);
	}
}

static void save_rx_raw_data(struct mtk_uart *uart, const unsigned char *chars, size_t size)
{
	unsigned long flags;
	int i;

	if (uart->nport == 2) {
		spin_lock_irqsave(&rx_history_lock, flags);
		if (!stop_update) {
			for (i = 0; (curr_rx_idx < RECORD_LENGTH) && (i < size); i++, curr_rx_idx++)
				uart_rx_history[curr_rx_record][curr_rx_idx] = chars[i];
			uart_rx_history_cnt[curr_rx_record] = curr_rx_idx;
		}
		spin_unlock_irqrestore(&rx_history_lock, flags);
	}
}

void stop_log(void)
{
	unsigned long flags;
	unsigned long rx_flags;

	spin_lock_irqsave(&tx_history_lock, flags);
	spin_lock_irqsave(&rx_history_lock, rx_flags);
	stop_update = 1;
	spin_unlock_irqrestore(&rx_history_lock, rx_flags);
	spin_unlock_irqrestore(&tx_history_lock, flags);
}
EXPORT_SYMBOL(stop_log);

void dump_uart_history(void)
{
	int i, j;
	unsigned long flags;
	unsigned long rx_flags;
	int curr, rx_curr;

	spin_lock_irqsave(&tx_history_lock, flags);
	spin_lock_irqsave(&rx_history_lock, rx_flags);
	stop_update = 1;
	spin_unlock_irqrestore(&rx_history_lock, rx_flags);
	spin_unlock_irqrestore(&tx_history_lock, flags);
	curr = curr_record + 1;
	if (curr >= RECORD_NUMBER)
		curr = 0;
	rx_curr = curr_rx_record + 1;
	if (rx_curr >= RECORD_NUMBER)
		rx_curr = 0;

	for (i = 0; i < RECORD_NUMBER; i++) {
		pr_debug("\nTX rec%03d:", i);
		for (j = 0; j < uart_history_cnt[curr]; j++) {
			if ((j % 0xF) == 0)
				pr_debug("\n");
			pr_debug("%02x ", uart_history[curr][j]);
		}
		msleep(20);
		curr++;
		if (curr >= RECORD_NUMBER)
			curr = 0;
	}
	for (i = 0; i < RECORD_NUMBER; i++) {
		pr_debug("\nRX rec%03d:", i);
		for (j = 0; j < uart_rx_history_cnt[rx_curr]; j++) {
			if ((j % 0xF) == 0)
				pr_debug("\n");
			pr_debug("%02x ", uart_rx_history[rx_curr][j]);
		}
		msleep(20);
		rx_curr++;
		if (rx_curr >= RECORD_NUMBER)
			rx_curr = 0;
	}
}
EXPORT_SYMBOL(dump_uart_history);
#else
void stop_log(void)
{
	/* dummy API */
}

void dump_uart_history(void)
{
	/* dummy API */
}
#endif

/*---------------------------------------------------------------------------*/
void mtk_uart_save(struct mtk_uart *uart)
{
#ifdef CONFIG_PM
	unsigned long base;
	unsigned long flags;

	/* UART never power on, no need save */
	if (uart->poweron_count == 0)
		return;

	base = uart->base;

	/* DLL may be changed by console write. To avoid this, use spinlock */
	spin_lock_irqsave(&mtk_console_lock, flags);
	uart->registers.lcr = UART_READ32(UART_LCR);

	reg_sync_writel(0xbf, UART_LCR);
	uart->registers.efr = UART_READ32(UART_EFR);
	reg_sync_writel(uart->registers.lcr, UART_LCR);
	uart->registers.fcr = UART_READ32(UART_FCR_RD);

	/* baudrate */
	uart->registers.highspeed = UART_READ32(UART_HIGHSPEED);
	uart->registers.fracdiv_l = UART_READ32(UART_FRACDIV_L);
	uart->registers.fracdiv_m = UART_READ32(UART_FRACDIV_M);
	reg_sync_writel(uart->registers.lcr | UART_LCR_DLAB, UART_LCR);
	uart->registers.dll = UART_READ32(UART_DLL);
	uart->registers.dlh = UART_READ32(UART_DLH);
	reg_sync_writel(uart->registers.lcr, UART_LCR);
	uart->registers.sample_count = UART_READ32(UART_SAMPLE_COUNT);
	uart->registers.sample_point = UART_READ32(UART_SAMPLE_POINT);
	uart->registers.guard = UART_READ32(UART_GUARD);

	/* flow control */
	uart->registers.escape_en = UART_READ32(UART_ESCAPE_EN);
	uart->registers.mcr = UART_READ32(UART_MCR);
	uart->registers.ier = UART_READ32(UART_IER);

	uart->registers.rx_sel = UART_READ32(UART_RX_SEL);

	spin_unlock_irqrestore(&mtk_console_lock, flags);
#endif
}

void mtk_uart_restore(void)
{
#ifdef CONFIG_PM
	unsigned long base;
	unsigned long flags;
	struct mtk_uart *uart;
	int i;

	for (i = 0; i < UART_NR; i++) {
		uart = &mtk_uarts[i];
		base = uart->base;

		mtk_uart_power_up(uart);
		spin_lock_irqsave(&mtk_console_lock, flags);
		reg_sync_writel(0xbf, UART_LCR);
		reg_sync_writel(uart->registers.efr, UART_EFR);
		reg_sync_writel(uart->registers.lcr, UART_LCR);
		reg_sync_writel(uart->registers.fcr, UART_FCR);

		/* baudrate */
		reg_sync_writel(uart->registers.highspeed, UART_HIGHSPEED);
		reg_sync_writel(uart->registers.fracdiv_l, UART_FRACDIV_L);
		reg_sync_writel(uart->registers.fracdiv_m, UART_FRACDIV_M);
		reg_sync_writel(uart->registers.lcr | UART_LCR_DLAB, UART_LCR);
		reg_sync_writel(uart->registers.dll, UART_DLL);
		reg_sync_writel(uart->registers.dlh, UART_DLH);
		reg_sync_writel(uart->registers.lcr, UART_LCR);
		reg_sync_writel(uart->registers.sample_count, UART_SAMPLE_COUNT);
		reg_sync_writel(uart->registers.sample_point, UART_SAMPLE_POINT);
		reg_sync_writel(uart->registers.guard, UART_GUARD);

		/* flow control */
		reg_sync_writel(uart->registers.escape_en, UART_ESCAPE_EN);
		reg_sync_writel(uart->registers.mcr, UART_MCR);
		reg_sync_writel(uart->registers.ier, UART_IER);

		reg_sync_writel(uart->registers.rx_sel, UART_RX_SEL);

		spin_unlock_irqrestore(&mtk_console_lock, flags);

		if (uart != console_port)
			mtk_uart_power_down(uart);

	}
#endif
}

#if !defined(CONFIG_MTK_LEGACY) && !defined(CONFIG_FPGA_EARLY_PORTING)
void switch_uart_gpio(int uartport, int gpioopid)
{
	struct pinctrl *ppinctrl = NULL;
	struct pinctrl_state *pins_uart = NULL;
	static DEFINE_RATELIMIT_STATE(ratelimit, 5 * HZ,  5);

	if ((uartport >= UART_NR) || (uartport > 3)) {
		pr_notice("[UART%d][PinC]%s: port error!!\n", uartport, __func__);
		return;
	}

	ppinctrl = ppinctrl_uart[uartport];
	if (IS_ERR(ppinctrl)) {
		pr_notice("[UART%d][PinC]%s get pinctrl fail!! err:%ld\n", uartport, __func__, PTR_ERR(ppinctrl));
		return;
	}

	pins_uart = pinctrl_lookup_state(ppinctrl, uart_gpio_cmds[uartport][gpioopid]);


	if (IS_ERR(pins_uart)) {
		if (__ratelimit(&ratelimit)) {
			pr_notice("[UART%d][PinC]%s pinctrl_lockup(%d, %s) fail!! pctrl:%p, err:%ld\n", uartport,
				__func__, uartport, uart_gpio_cmds[uartport][gpioopid], ppinctrl, PTR_ERR(pins_uart));
		}
		return;
	}

	pinctrl_select_state(ppinctrl, pins_uart);

}
#endif /* !defined(CONFIG_MTK_LEGACY) && !defined(CONFIG_FPGA_EARLY_PORTING) */

void mtk_uart_switch_tx_to_gpio(struct mtk_uart *uart)
{
#if defined(CONFIG_PM) && !defined(CONFIG_FPGA_EARLY_PORTING) && !defined(CONFIG_MTK_LEGACY)
	int uart_gpio_op = 0;	/* URAT RX SET */
#endif /* defined(CONFIG_PM) && !defined(CONFIG_FPGA_EARLY_PORTING) && !defined(CONFIG_MTK_LEGACY) */
	int uartport = uart->nport;

	/* pr_debug("[UART]%s port:0x%x\n", __func__, uartport); */

	if (uartport > 3) {
		pr_notice("[UART%d] %s fail!! port:%d", uartport, __func__, uartport);
		return;
	}
#ifdef CONFIG_PM
#ifndef CONFIG_FPGA_EARLY_PORTING
#if defined(CONFIG_MTK_LEGACY)
	switch (uart->nport) {
	case 0:
#ifdef GPIO_UART_UTXD0_PIN
		mt_set_gpio_out(GPIO_UART_UTXD0_PIN, GPIO_OUT_ONE);
		mt_set_gpio_mode(GPIO_UART_UTXD0_PIN, GPIO_UART_UTXD0_PIN_M_GPIO);
#else
		/*pr_debug("GPIO_UART_UTXD0_PIN is not properly set\n");*/
#endif
		break;
	case 1:
#ifdef GPIO_UART_UTXD1_PIN
		mt_set_gpio_out(GPIO_UART_UTXD1_PIN, GPIO_OUT_ONE);
		mt_set_gpio_mode(GPIO_UART_UTXD1_PIN, GPIO_UART_UTXD1_PIN_M_GPIO);
#else
		/*pr_debug("GPIO_UART_UTXD1_PIN is not properly set\n");*/
#endif
		break;
	case 2:
#ifdef GPIO_UART_UTXD2_PIN
		mt_set_gpio_out(GPIO_UART_UTXD2_PIN, GPIO_OUT_ONE);
		mt_set_gpio_mode(GPIO_UART_UTXD2_PIN, GPIO_UART_UTXD2_PIN_M_GPIO);
#else
		/*pr_debug("GPIO_UART_UTXD2_PIN is not properly set\n");*/
#endif
		break;
	case 3:
#ifdef GPIO_UART_UTXD3_PIN
		mt_set_gpio_out(GPIO_UART_UTXD3_PIN, GPIO_OUT_ONE);
		mt_set_gpio_mode(GPIO_UART_UTXD3_PIN, GPIO_UART_UTXD3_PIN_M_GPIO);
#else
		/*pr_debug("GPIO_UART_UTXD3_PIN is not properly set\n");*/
#endif
		break;
	default:
		break;
	}
#else /* defined(CONFIG_MTK_LEGACY)*/
	/*pr_debug("[UART%d][PinC]%s call switch_uart_gpio(%d, %d)\n", uartport, __func__, uartport, uart_gpio_op);*/
	switch_uart_gpio(uartport, uart_gpio_op);
#endif /* defined(CONFIG_MTK_LEGACY) */

#endif
#endif
}

/*---------------------------------------------------------------------------*/
void mtk_uart_switch_to_tx(struct mtk_uart *uart)
{
#if defined(CONFIG_PM) && !defined(CONFIG_FPGA_EARLY_PORTING) && !defined(CONFIG_MTK_LEGACY)
	int uart_gpio_op = 0;	/* URAT RX SET */
#endif /* defined(CONFIG_PM) && !defined(CONFIG_FPGA_EARLY_PORTING) && !defined(CONFIG_MTK_LEGACY) */
	int uartport = uart->nport;

	/* pr_debug("[UART]%s port:0x%x\n", __func__, uartport);*/

	if (uartport > 3) {
		pr_notice("[UART%d] %s fail!! port:%d", uartport, __func__, uartport);
		return;
	}
#ifdef CONFIG_PM
#ifndef CONFIG_FPGA_EARLY_PORTING
#if defined(CONFIG_MTK_LEGACY)
	switch (uart->nport) {
	case 0:
#ifdef GPIO_UART_UTXD0_PIN
		mt_set_gpio_mode(GPIO_UART_UTXD0_PIN, GPIO_UART_UTXD0_PIN_M_UTXD);
#else
		/*pr_debug("GPIO_UART_UTXD0_PIN is not properly set p2\n");*/
#endif
		break;
	case 1:
#ifdef GPIO_UART_UTXD1_PIN
		mt_set_gpio_mode(GPIO_UART_UTXD1_PIN, GPIO_UART_UTXD1_PIN_M_UTXD);
#else
		/*pr_debug("GPIO_UART_UTXD1_PIN is not properly set p2\n");*/
#endif
		break;
	case 2:
#ifdef GPIO_UART_UTXD2_PIN
		mt_set_gpio_mode(GPIO_UART_UTXD2_PIN, GPIO_UART_UTXD2_PIN_M_UTXD);
#else
		/*pr_debug("GPIO_UART_UTXD2_PIN is not properly set p2\n");*/
#endif
		break;
	case 3:
#ifdef GPIO_UART_UTXD3_PIN
		mt_set_gpio_mode(GPIO_UART_UTXD3_PIN, GPIO_UART_UTXD3_PIN_M_UTXD);
#else
		/*pr_debug("GPIO_UART_UTXD3_PIN is not properly set p3\n");*/
#endif
		break;
	default:
		break;
	}
#else /* defined(CONFIG_MTK_LEGACY) */
	/*pr_debug("[UART%d][PinC]%s call switch_uart_gpio(%d, %d)\n", uartport, __func__, uartport, uart_gpio_op);*/
	switch_uart_gpio(uartport, uart_gpio_op);
#endif /* defined(CONFIG_MTK_LEGACY) */
#endif
#endif
}

/*---------------------------------------------------------------------------*/
void mtk_uart_switch_rx_to_gpio(struct mtk_uart *uart)
{
#if defined(CONFIG_PM) && !defined(CONFIG_FPGA_EARLY_PORTING) && !defined(CONFIG_MTK_LEGACY)
	int uart_gpio_op = 1;	/* URAT RX Clear */
#endif /* !defined(CONFIG_MTK_LEGACY) */
	int uartport = uart->nport;

	/* pr_debug("[UART]%s port:0x%x\n", __func__, uartport); */

	if (uartport > 3) {
		pr_notice("[UART%d] %s fail!! port:%d", uartport, __func__, uartport);
		return;
	}
#ifdef CONFIG_PM
#ifndef CONFIG_FPGA_EARLY_PORTING
#if defined(CONFIG_MTK_LEGACY)
	switch (uart->nport) {
	case 0:
#ifdef GPIO_UART_URXD0_PIN
		mt_set_gpio_mode(GPIO_UART_URXD0_PIN, GPIO_UART_URXD0_PIN_M_GPIO);
#else
		/*pr_debug("GPIO_UART_URXD0_PIN is not properly set\n");*/
#endif
		break;
	case 1:
#ifdef GPIO_UART_URXD1_PIN
		mt_set_gpio_mode(GPIO_UART_URXD1_PIN, GPIO_UART_URXD1_PIN_M_GPIO);
#else
		/*pr_debug("GPIO_UART_URXD1_PIN is not properly set\n");*/
#endif
		break;
	case 2:
#ifdef GPIO_UART_URXD2_PIN
		mt_set_gpio_mode(GPIO_UART_URXD2_PIN, GPIO_UART_URXD2_PIN_M_GPIO);
#else
		/*pr_debug("GPIO_UART_URXD2_PIN is not properly set\n");*/
#endif
		break;
	case 3:
#ifdef GPIO_UART_URXD3_PIN
		mt_set_gpio_mode(GPIO_UART_URXD3_PIN, GPIO_UART_URXD3_PIN_M_GPIO);
#else
		/*pr_debug("GPIO_UART_URXD3_PIN is not properly set\n");*/
#endif
		break;
	default:
		break;
	}
#else /* defined(CONFIG_MTK_LEGACY) */
	/*pr_debug("[UART%d][PinC]%s call switch_uart_gpio(%d, %d)\n", uartport, __func__, uartport, uart_gpio_op);*/
	switch_uart_gpio(uartport, uart_gpio_op);
#endif /* defined(CONFIG_MTK_LEGACY) */

#endif
#endif
}

/*---------------------------------------------------------------------------*/
void mtk_uart_switch_to_rx(struct mtk_uart *uart)
{
#if defined(CONFIG_PM) && !defined(CONFIG_FPGA_EARLY_PORTING) && !defined(CONFIG_MTK_LEGACY)
	int uart_gpio_op = 0;	/* URAT RX SET */
#endif  /* !defined(CONFIG_MTK_LEGACY) */
	int uartport = uart->nport;

	/* pr_debug("[UART]%s port:0x%x\n", __func__, uartport);*/

	if (uartport > 3) {
		pr_notice("[UART%d] %s fail!! port:%d", uartport, __func__, uartport);
		return;
	}
#ifdef CONFIG_PM
#ifndef CONFIG_FPGA_EARLY_PORTING
#if defined(CONFIG_MTK_LEGACY)
	switch (uartport) {
	case 0:
#ifdef GPIO_UART_URXD0_PIN
		mt_set_gpio_mode(GPIO_UART_URXD0_PIN, GPIO_UART_URXD0_PIN_M_URXD);
#else				/* GPIO_UART_URXD0_PIN */
		/*pr_debug("GPIO_UART_URXD0_PIN is not properly set p2\n");*/
#endif				/* GPIO_UART_URXD0_PIN */
		break;

	case 1:
#ifdef GPIO_UART_URXD1_PIN
		mt_set_gpio_mode(GPIO_UART_URXD1_PIN, GPIO_UART_URXD1_PIN_M_URXD);
#else				/* GPIO_UART_URXD1_PIN */
		/*pr_debug("GPIO_UART_URXD1_PIN is not properly set p2\n");*/
#endif				/* GPIO_UART_URXD1_PIN */
		break;

	case 2:
#ifdef GPIO_UART_URXD2_PIN
		mt_set_gpio_mode(GPIO_UART_URXD2_PIN, GPIO_UART_URXD2_PIN_M_URXD);
#else
		/*pr_debug("GPIO_UART_URXD2_PIN is not properly set p2\n");*/
#endif
		break;

	case 3:
#ifdef GPIO_UART_URXD3_PIN
		mt_set_gpio_mode(GPIO_UART_URXD3_PIN, GPIO_UART_URXD3_PIN_M_URXD);
#else
		/*pr_debug("GPIO_UART_URXD3_PIN is not properly set p2\n");*/
#endif
		break;
	default:
		break;
	}
#else /* defined(CONFIG_MTK_LEGACY) */
	/*pr_debug("[UART%d][PinC]%s call switch_uart_gpio(%d, %d)\n", uartport, __func__, uartport, uart_gpio_op);*/
	switch_uart_gpio(uartport, uart_gpio_op);
#endif /* defined(CONFIG_MTK_LEGACY) */
#endif
#endif
}

/*---------------------------------------------------------------------------*/
void mtk_uart_enable_dpidle(struct mtk_uart *uart)
{
/* FIX-ME early porting */
#ifndef CONFIG_FPGA_EARLY_PORTING
	spm_resource_req(SPM_RESOURCE_USER_UART, SPM_RESOURCE_RELEASE);
#endif
}

/*---------------------------------------------------------------------------*/
void mtk_uart_disable_dpidle(struct mtk_uart *uart)
{
/* FIX-ME early porting */
#ifndef CONFIG_FPGA_EARLY_PORTING
	spm_resource_req(SPM_RESOURCE_USER_UART, SPM_RESOURCE_ALL);
#endif
}

/*---------------------------------------------------------------------------*/
int mtk_uart_plat_info_query(const char str[])
{
	return 0;
}
