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

#ifndef __MTK_UART_INTF_H__
#define __MTK_UART_INTF_H__

#include "platform_uart.h"
#include <linux/platform_device.h>
/*---------------------------------------------------------------------------*/
/* fiq debugger */
/*---------------------------------------------------------------------------*/
int fiq_uart_getc(struct platform_device *pdev);
void fiq_uart_putc(struct platform_device *pdev, unsigned int c);
void fiq_uart_fixup(int uart_port);
irqreturn_t mt_debug_signal_irq(int irq, void *dev_id);
int mt_fiq_init(void *arg);
/*---------------------------------------------------------------------------*/
struct mtk_uart_setting *get_uart_default_settings(int idx);
#ifdef CONFIG_OF
void *get_apdma_uart0_base(void);
unsigned int get_uart_vfifo_irq_id(int idx);
void set_uart_default_settings(int idx);
#endif
unsigned long get_uart_evt_mask(int idx);
void mtk_uart_switch_tx_to_gpio(struct mtk_uart *uart);
void mtk_uart_switch_to_tx(struct mtk_uart *uart);
void mtk_uart_switch_rx_to_gpio(struct mtk_uart *uart);
void mtk_uart_switch_to_rx(struct mtk_uart *uart);
void set_uart_evt_mask(int idx, int value);
unsigned long get_uart_lsr_status(int idx);
void set_uart_lsr_status(int idx, int value);
unsigned char get_modem_status(int idx);
void dump_uart_reg(void);
void mtk_uart_dump_reg(char *s);
#if defined(ENABLE_CONSOLE_DEBUG)
void dump_console_reg(struct mtk_uart *uart, char *s);
#endif
void mtk_uart_console_setting_switch(struct mtk_uart *uart);
int mtk_uart_vfifo_is_empty(struct mtk_uart_vfifo *vfifo);
void mtk_uart_tx_vfifo_flush(struct mtk_uart *uart, int timeout);
int mtk_uart_vfifo_get_counts(struct mtk_uart_vfifo *vfifo);
void mtk_uart_dma_vfifo_tx_tasklet(unsigned long arg);
void mtk_uart_dma_vfifo_rx_tasklet(unsigned long arg);
int mtk_uart_vfifo_enable(struct mtk_uart *uart, struct mtk_uart_vfifo *vfifo);
int mtk_uart_vfifo_disable(struct mtk_uart *uart, struct mtk_uart_vfifo *vfifo);
void mtk_uart_vfifo_enable_tx_intr(struct mtk_uart *uart);
void mtk_uart_vfifo_disable_tx_intr(struct mtk_uart *uart);
void mtk_uart_vfifo_enable_rx_intr(struct mtk_uart *uart);
void mtk_uart_vfifo_disable_rx_intr(struct mtk_uart *uart);
unsigned int mtk_uart_write_allow(struct mtk_uart *uart);
void mtk_uart_enable_intrs(struct mtk_uart *uart, long mask);
void mtk_uart_disable_intrs(struct mtk_uart *uart, long mask);
int mtk_uart_vfifo_is_full(struct mtk_uart_vfifo *vfifo);
void mtk_uart_stop_dma(struct mtk_uart_dma *dma);
void mtk_uart_reset_dma(struct mtk_uart_dma *dma);
void mtk_uart_set_mode(struct mtk_uart *uart, int mode);
void mtk_uart_set_auto_baud(struct mtk_uart *uart);
void mtk_uart_baud_setting(struct mtk_uart *uart, int baudrate);
void mtk_uart_dma_setup(struct mtk_uart *uart, struct mtk_uart_dma *dma);
int mtk_uart_dma_start(struct mtk_uart *uart, struct mtk_uart_dma *dma);
void mtk_uart_vfifo_write_byte(struct mtk_uart *uart, unsigned int byte);
unsigned int mtk_uart_vfifo_read_byte(struct mtk_uart *uart);
unsigned int mtk_uart_read_status(struct mtk_uart *uart);
unsigned int mtk_uart_read_allow(struct mtk_uart *uart);
unsigned int mtk_uart_read_byte(struct mtk_uart *uart);
void mtk_uart_write_byte(struct mtk_uart *uart, unsigned int byte);
unsigned int mtk_uart_filter_line_status(struct mtk_uart *uart);
void mtk_uart_fifo_set_trig(struct mtk_uart *uart, int tx_level, int rx_level);
void mtk_uart_enable_sleep(struct mtk_uart *uart);
void mtk_uart_fifo_init(struct mtk_uart *uart);
void mtk_uart_fifo_flush(struct mtk_uart *uart);
int mtk_uart_data_ready(struct mtk_uart *uart);
void mtk_uart_config(struct mtk_uart *uart, int datalen, int stop, int parity);
void mtk_uart_set_flow_ctrl(struct mtk_uart *uart, int mode);
void mtk_uart_power_up(struct mtk_uart *uart);
void mtk_uart_power_down(struct mtk_uart *uart);
void mtk_uart_get_modem_status(struct mtk_uart *uart);
void mtk_uart_rx_pre_handler(struct mtk_uart *uart, int intrs);
int mtk_uart_get_interrupt(struct mtk_uart *uart);
void mtk_uart_intr_last_check(struct mtk_uart *uart, int intrs);
void mtk_uart_set_mctrl(struct uart_port *port, unsigned int mctrl);
unsigned int mtk_uart_get_mctrl(struct uart_port *port);
void mtk_uart_stop_rx(struct uart_port *port);
void mtk_uart_break_ctl(struct uart_port *port, int break_state);
void mtk_uart_save(struct mtk_uart *uart);
void mtk_uart_restore(void);
void mtk_uart_vfifo_clear_tx_intr(struct mtk_uart_vfifo *vfifo);
void mtk_uart_vfifo_clear_rx_intr(struct mtk_uart_vfifo *vfifo);
void mtk_uart_init_debug_spinlock(void);
void reset_tx_raw_data(struct mtk_uart *uart);
void mtk_uart_enable_dpidle(struct mtk_uart *uart);
void mtk_uart_disable_dpidle(struct mtk_uart *uart);
int  mtk_uart_plat_info_query(const char str[]);

#if !defined(CONFIG_MTK_LEGACY)
void set_uart_dma_clk(int port_id, struct clk *dma_clk);
void set_uart_pinctrl(int idx, struct pinctrl *ppinctrl);
#endif /* !defined(CONFIG_MTK_LEGACY) */

#endif				/* MTK_UART_INTF_H */
