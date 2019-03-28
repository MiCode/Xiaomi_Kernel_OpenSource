/*
 * dbmdx-uart-common.h  --  DBMDX UART interface common functions
 *
 * Copyright (C) 2014 DSP Group
 * Copyright (C) 2019 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _DBMDX_UART_COMMON_H
#define _DBMDX_UART_COMMON_H

#ifdef CONFIG_PM_WAKELOCKS
#include <linux/pm_wakeup.h>
#endif

#define RETRY_COUNT				5

struct dbmdx_uart_data {
	const char	*uart_dev;
	unsigned int	software_flow_control;
	u32	read_chunk_size;
	u32	write_chunk_size;
	u8	read_buf[MAX_REQ_SIZE];
};

struct dbmdx_uart_private {
	struct platform_device		*pdev;
	struct dbmdx_uart_data		*pdata;
	struct device			*dev;
	struct chip_interface		chip;
#ifdef CONFIG_PM_WAKELOCKS
	struct wakeup_source		ps_nosuspend_wl;
#endif
	struct tty_struct		*tty;
	struct file			*fp;
	struct tty_ldisc		*ldisc;
	unsigned int			boot_baud_rate;
	int				boot_stop_bits;
	int				boot_parity;
	unsigned int			normal_baud_rate;
	int				normal_stop_bits;
	int				normal_parity;
	unsigned int			boot_lock_buffer_size;
	int				uart_open;
	atomic_t			stop_uart_probing;
	struct task_struct		*uart_probe_thread;
	struct completion		uart_done;
	u16				post_pll_div;
	u32				interface_enabled;
};


void uart_flush_rx_fifo(struct dbmdx_uart_private *p);
int uart_configure_tty(struct dbmdx_uart_private *p, u32 bps, int stop,
			      int parity, int flow);
ssize_t uart_read_data(struct dbmdx_private *p, void *buf, size_t len);
ssize_t uart_write_data_no_sync(struct dbmdx_private *p, const void *buf,
				       size_t len);
ssize_t uart_write_data(struct dbmdx_private *p, const void *buf,
			       size_t len);
ssize_t send_uart_cmd_vqe(struct dbmdx_private *p, u32 command,
			     u16 *response);
ssize_t send_uart_cmd_va(struct dbmdx_private *p, u32 command,
				   u16 *response);
int send_uart_cmd_boot(struct dbmdx_private *p, u32 command);
int uart_verify_boot_checksum(struct dbmdx_private *p,
	const void *checksum, size_t chksum_len);
int uart_verify_chip_id(struct dbmdx_private *p);
int uart_wait_for_ok(struct dbmdx_private *p);
int uart_wait_till_alive(struct dbmdx_private *p);
int uart_set_speed_host_only(struct dbmdx_private *p, int index);
int uart_set_speed(struct dbmdx_private *p, int index);
int uart_common_probe(struct platform_device *pdev, const char threadnamefmt[]);
int uart_common_remove(struct platform_device *pdev);
void uart_set_private_callbacks(struct dbmdx_uart_private *p);
void uart_interface_resume(struct dbmdx_uart_private *uart_p);
void uart_interface_suspend(struct dbmdx_uart_private *uart_p);

#endif
