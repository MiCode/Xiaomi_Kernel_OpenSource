/*
 * drivers/serial/msm_serial.c - driver for msm7k serial device and console
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2010-2014, The Linux Foundation. All rights reserved.
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

/* Acknowledgements:
 * This file is based on msm_serial.c, originally
 * Written by Robert Love <rlove@google.com>  */

#define pr_fmt(fmt) "%s: " fmt, __func__

#if defined(CONFIG_SERIAL_MSM_HSL_CONSOLE) && defined(CONFIG_MAGIC_SYSRQ)
#define SUPPORT_SYSRQ
#endif

#include <linux/atomic.h>
#include <linux/hrtimer.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/irq.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/console.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial_core.h>
#include <linux/serial.h>
#include <linux/nmi.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/gpio.h>
#include <linux/debugfs.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/wakelock.h>
#include <linux/types.h>
#include <asm/byteorder.h>
#include <linux/platform_data/qcom-serial_hs_lite.h>
#include <linux/msm-bus.h>
#include "msm_serial_hs_hwreg.h"

/*
 * There are 3 different kind of UART Core available on MSM.
 * High Speed UART (i.e. Legacy HSUART), GSBI based HSUART
 * and BSLP based HSUART.
 */
enum uart_core_type {
	LEGACY_HSUART,
	GSBI_HSUART,
	BLSP_HSUART,
};

/*
 * UART can be used in 2-wire or 4-wire mode.
 * Use uart_func_mode to set 2-wire or 4-wire mode.
 */
enum uart_func_mode {
	UART_TWO_WIRE, /* can't support HW Flow control. */
	UART_FOUR_WIRE,/* can support HW Flow control. */
};

struct msm_hsl_port {
	struct uart_port	uart;
	char			name[16];
	struct clk		*clk;
	struct clk		*pclk;
	struct dentry		*loopback_dir;
	unsigned int		imr;
	unsigned int		*uart_csr_code;
	unsigned int            *gsbi_mapbase;
	unsigned int            *mapped_gsbi;
	unsigned int            old_snap_state;
	unsigned long		ver_id;
	int			tx_timeout;
	struct mutex		clk_mutex;
	enum uart_core_type	uart_type;
	enum uart_func_mode	func_mode;
	struct wake_lock	port_open_wake_lock;
	int			clk_enable_count;
	u32			bus_perf_client;
	/* BLSP UART required BUS Scaling data */
	struct msm_bus_scale_pdata *bus_scale_table;
};

#define UARTDM_VERSION_11_13	0
#define UARTDM_VERSION_14	1

#define UART_TO_MSM(uart_port)	((struct msm_hsl_port *) uart_port)
#define is_console(port)	((port)->cons && \
				(port)->cons->index == (port)->line)

static const unsigned int regmap[][UARTDM_LAST] = {
	[UARTDM_VERSION_11_13] = {
		[UARTDM_MR1] = UARTDM_MR1_ADDR,
		[UARTDM_MR2] = UARTDM_MR2_ADDR,
		[UARTDM_IMR] = UARTDM_IMR_ADDR,
		[UARTDM_SR] = UARTDM_SR_ADDR,
		[UARTDM_CR] = UARTDM_CR_ADDR,
		[UARTDM_CSR] = UARTDM_CSR_ADDR,
		[UARTDM_IPR] = UARTDM_IPR_ADDR,
		[UARTDM_ISR] = UARTDM_ISR_ADDR,
		[UARTDM_RX_TOTAL_SNAP] = UARTDM_RX_TOTAL_SNAP_ADDR,
		[UARTDM_TFWR] = UARTDM_TFWR_ADDR,
		[UARTDM_RFWR] = UARTDM_RFWR_ADDR,
		[UARTDM_RF] = UARTDM_RF_ADDR,
		[UARTDM_TF] = UARTDM_TF_ADDR,
		[UARTDM_MISR] = UARTDM_MISR_ADDR,
		[UARTDM_DMRX] = UARTDM_DMRX_ADDR,
		[UARTDM_NCF_TX] = UARTDM_NCF_TX_ADDR,
		[UARTDM_DMEN] = UARTDM_DMEN_ADDR,
		[UARTDM_TXFS] = UARTDM_TXFS_ADDR,
		[UARTDM_RXFS] = UARTDM_RXFS_ADDR,
	},
	[UARTDM_VERSION_14] = {
		[UARTDM_MR1] = 0x0,
		[UARTDM_MR2] = 0x4,
		[UARTDM_IMR] = 0xb0,
		[UARTDM_SR] = 0xa4,
		[UARTDM_CR] = 0xa8,
		[UARTDM_CSR] = 0xa0,
		[UARTDM_IPR] = 0x18,
		[UARTDM_ISR] = 0xb4,
		[UARTDM_RX_TOTAL_SNAP] = 0xbc,
		[UARTDM_TFWR] = 0x1c,
		[UARTDM_RFWR] = 0x20,
		[UARTDM_RF] = 0x140,
		[UARTDM_TF] = 0x100,
		[UARTDM_MISR] = 0xac,
		[UARTDM_DMRX] = 0x34,
		[UARTDM_NCF_TX] = 0x40,
		[UARTDM_DMEN] = 0x3c,
		[UARTDM_TXFS] = 0x4c,
		[UARTDM_RXFS] = 0x50,
	},
};

static struct of_device_id msm_hsl_match_table[] = {
	{	.compatible = "qcom,msm-lsuart-v14",
		.data = (void *)UARTDM_VERSION_14
	},
	{}
};

#ifdef CONFIG_SERIAL_MSM_HSL_CONSOLE
static int get_console_state(struct uart_port *port);
#else
static inline int get_console_state(struct uart_port *port) { return -ENODEV; };
#endif

static struct dentry *debug_base;
static inline void wait_for_xmitr(struct uart_port *port);
static inline void msm_hsl_write(struct uart_port *port,
				 unsigned int val, unsigned int off)
{
	__iowmb();
	__raw_writel_no_log((__force __u32)cpu_to_le32(val),
		port->membase + off);
}
static inline unsigned int msm_hsl_read(struct uart_port *port,
		     unsigned int off)
{
	unsigned int v = le32_to_cpu((__force __le32)__raw_readl_no_log(
		port->membase + off));
	__iormb();
	return v;
}

static unsigned int msm_serial_hsl_has_gsbi(struct uart_port *port)
{
	return (UART_TO_MSM(port)->uart_type == GSBI_HSUART);
}

/**
 * set_gsbi_uart_func_mode: Check the currently used GSBI UART mode
 * and set the new required GSBI UART Mode if it is different.
 * @port: uart port
 */
static void set_gsbi_uart_func_mode(struct uart_port *port)
{
	struct msm_hsl_port *msm_hsl_port = UART_TO_MSM(port);
	unsigned int set_gsbi_uart_mode = GSBI_PROTOCOL_I2C_UART;
	unsigned int cur_gsbi_uart_mode;

	if (msm_hsl_port->func_mode == UART_FOUR_WIRE)
		set_gsbi_uart_mode = GSBI_PROTOCOL_UART;

	if (msm_hsl_port->pclk)
		clk_prepare_enable(msm_hsl_port->pclk);

	/* Read current used GSBI UART Mode and set only if it is different. */
	cur_gsbi_uart_mode = ioread32(msm_hsl_port->mapped_gsbi +
					GSBI_CONTROL_ADDR);
	if ((cur_gsbi_uart_mode & set_gsbi_uart_mode) != set_gsbi_uart_mode)
		/*
		 * Programmed GSBI based UART protocol mode i.e. I2C/UART
		 * Shared Mode or UART Mode.
		 */
		iowrite32(set_gsbi_uart_mode,
			msm_hsl_port->mapped_gsbi + GSBI_CONTROL_ADDR);

	if (msm_hsl_port->pclk)
		clk_disable_unprepare(msm_hsl_port->pclk);
}

/**
 * msm_hsl_config_uart_tx_rx_gpios - Configures UART Tx and RX GPIOs
 * @port: uart port
 */
static int msm_hsl_config_uart_tx_rx_gpios(struct uart_port *port)
{
	struct platform_device *pdev = to_platform_device(port->dev);
	const struct msm_serial_hslite_platform_data *pdata =
					pdev->dev.platform_data;
	int ret;

	if (pdata) {
		ret = gpio_request(pdata->uart_tx_gpio,
				"UART_TX_GPIO");
		if (unlikely(ret)) {
			pr_err("gpio request failed for:%d\n",
					pdata->uart_tx_gpio);
			goto exit_uart_config;
		}

		ret = gpio_request(pdata->uart_rx_gpio, "UART_RX_GPIO");
		if (unlikely(ret)) {
			pr_err("gpio request failed for:%d\n",
					pdata->uart_rx_gpio);
			gpio_free(pdata->uart_tx_gpio);
			goto exit_uart_config;
		}
	} else {
		pr_err("Pdata is NULL.\n");
		ret = -EINVAL;
	}

exit_uart_config:
	return ret;
}

/**
 * msm_hsl_unconfig_uart_tx_rx_gpios: Unconfigures UART Tx and RX GPIOs
 * @port: uart port
 */
static void msm_hsl_unconfig_uart_tx_rx_gpios(struct uart_port *port)
{
	struct platform_device *pdev = to_platform_device(port->dev);
	const struct msm_serial_hslite_platform_data *pdata =
					pdev->dev.platform_data;

	if (pdata) {
		gpio_free(pdata->uart_tx_gpio);
		gpio_free(pdata->uart_rx_gpio);
	} else {
		pr_err("Error:Pdata is NULL.\n");
	}
}

/**
 * msm_hsl_config_uart_hwflow_gpios: Configures UART HWFlow GPIOs
 * @port: uart port
 */
static int msm_hsl_config_uart_hwflow_gpios(struct uart_port *port)
{
	struct platform_device *pdev = to_platform_device(port->dev);
	const struct msm_serial_hslite_platform_data *pdata =
				pdev->dev.platform_data;
	int ret = -EINVAL;

	if (pdata) {
		ret = gpio_request(pdata->uart_cts_gpio,
					"UART_CTS_GPIO");
		if (unlikely(ret)) {
			pr_err("gpio request failed for:%d\n",
					pdata->uart_cts_gpio);
			goto exit_config_uart;
		}

		ret = gpio_request(pdata->uart_rfr_gpio,
					"UART_RFR_GPIO");
		if (unlikely(ret)) {
			pr_err("gpio request failed for:%d\n",
				pdata->uart_rfr_gpio);
			gpio_free(pdata->uart_cts_gpio);
			goto exit_config_uart;
		}
	} else {
		pr_err("Error: Pdata is NULL.\n");
	}

exit_config_uart:
	return ret;
}

/**
 * msm_hsl_unconfig_uart_hwflow_gpios: Unonfigures UART HWFlow GPIOs
 * @port: uart port
 */
static void msm_hsl_unconfig_uart_hwflow_gpios(struct uart_port *port)
{
	struct platform_device *pdev = to_platform_device(port->dev);
	const struct msm_serial_hslite_platform_data *pdata =
					pdev->dev.platform_data;

	if (pdata) {
		gpio_free(pdata->uart_cts_gpio);
		gpio_free(pdata->uart_rfr_gpio);
	} else {
		pr_err("Error: Pdata is NULL.\n");
	}

}

/**
 * msm_hsl_config_uart_gpios: Configures UART GPIOs and returns success or
 * Failure
 * @port: uart port
 */
static int msm_hsl_config_uart_gpios(struct uart_port *port)
{
	struct msm_hsl_port *msm_hsl_port = UART_TO_MSM(port);
	int ret;

	/* Configure UART Tx and Rx GPIOs */
	ret = msm_hsl_config_uart_tx_rx_gpios(port);
	if (!ret) {
		if (msm_hsl_port->func_mode == UART_FOUR_WIRE) {
			/*if 4-wire uart, configure CTS and RFR GPIOs */
			ret = msm_hsl_config_uart_hwflow_gpios(port);
			if (ret)
				msm_hsl_unconfig_uart_tx_rx_gpios(port);
		}
	} else {
		msm_hsl_unconfig_uart_tx_rx_gpios(port);
	}

	return ret;
}

/**
 * msm_hsl_unconfig_uart_gpios: Unconfigures UART GPIOs
 * @port: uart port
 */
static void msm_hsl_unconfig_uart_gpios(struct uart_port *port)
{
	struct msm_hsl_port *msm_hsl_port = UART_TO_MSM(port);

	msm_hsl_unconfig_uart_tx_rx_gpios(port);
	if (msm_hsl_port->func_mode == UART_FOUR_WIRE)
		msm_hsl_unconfig_uart_hwflow_gpios(port);
}
static int get_line(struct platform_device *pdev)
{
	struct msm_hsl_port *msm_hsl_port = platform_get_drvdata(pdev);
	return msm_hsl_port->uart.line;
}

static int bus_vote(uint32_t client, int vector)
{
	int ret = 0;

	if (!client)
		return ret;

	pr_debug("Voting for bus scaling:%d\n", vector);

	ret = msm_bus_scale_client_update_request(client, vector);
	if (ret)
		pr_err("Failed to request bus bw vector %d\n", vector);

	return ret;
}

static int clk_en(struct uart_port *port, int enable)
{
	struct msm_hsl_port *msm_hsl_port = UART_TO_MSM(port);
	int ret = 0;

	if (enable) {

		msm_hsl_port->clk_enable_count++;
		ret = bus_vote(msm_hsl_port->bus_perf_client,
				!!msm_hsl_port->clk_enable_count);
		if (ret)
			goto err;
		ret = clk_prepare_enable(msm_hsl_port->clk);
		if (ret)
			goto err_bus;
		if (msm_hsl_port->pclk) {
			ret = clk_prepare_enable(msm_hsl_port->pclk);
			if (ret)
				goto err_clk_disable;
		}
	} else {

		msm_hsl_port->clk_enable_count--;
		clk_disable_unprepare(msm_hsl_port->clk);
		if (msm_hsl_port->pclk)
			clk_disable_unprepare(msm_hsl_port->pclk);
		ret = bus_vote(msm_hsl_port->bus_perf_client,
				!!msm_hsl_port->clk_enable_count);
	}

	return ret;

err_clk_disable:
	clk_disable_unprepare(msm_hsl_port->clk);
err_bus:
	bus_vote(msm_hsl_port->bus_perf_client,
			!!(msm_hsl_port->clk_enable_count - 1));
err:
	msm_hsl_port->clk_enable_count--;
	return ret;
}
static int msm_hsl_loopback_enable_set(void *data, u64 val)
{
	struct msm_hsl_port *msm_hsl_port = data;
	struct uart_port *port = &(msm_hsl_port->uart);
	unsigned int vid;
	unsigned long flags;
	int ret = 0;

	ret = clk_set_rate(msm_hsl_port->clk, port->uartclk);
	if (!ret) {
		clk_en(port, 1);
	} else {
		pr_err("Error: setting uartclk rate as %u\n",
						port->uartclk);
		return -EINVAL;
	}

	vid = msm_hsl_port->ver_id;
	if (val) {
		spin_lock_irqsave(&port->lock, flags);
		ret = msm_hsl_read(port, regmap[vid][UARTDM_MR2]);
		ret |= UARTDM_MR2_LOOP_MODE_BMSK;
		msm_hsl_write(port, ret, regmap[vid][UARTDM_MR2]);
		spin_unlock_irqrestore(&port->lock, flags);
	} else {
		spin_lock_irqsave(&port->lock, flags);
		ret = msm_hsl_read(port, regmap[vid][UARTDM_MR2]);
		ret &= ~UARTDM_MR2_LOOP_MODE_BMSK;
		msm_hsl_write(port, ret, regmap[vid][UARTDM_MR2]);
		spin_unlock_irqrestore(&port->lock, flags);
	}

	clk_en(port, 0);
	return 0;
}
static int msm_hsl_loopback_enable_get(void *data, u64 *val)
{
	struct msm_hsl_port *msm_hsl_port = data;
	struct uart_port *port = &(msm_hsl_port->uart);
	unsigned long flags;
	int ret = 0;

	ret = clk_set_rate(msm_hsl_port->clk, port->uartclk);
	if (!ret) {
		clk_en(port, 1);
	} else {
		pr_err("Error setting uartclk rate as %u\n",
						port->uartclk);
		return -EINVAL;
	}

	spin_lock_irqsave(&port->lock, flags);
	ret = msm_hsl_read(port, regmap[msm_hsl_port->ver_id][UARTDM_MR2]);
	spin_unlock_irqrestore(&port->lock, flags);
	clk_en(port, 0);

	*val = (ret & UARTDM_MR2_LOOP_MODE_BMSK) ? 1 : 0;
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(loopback_enable_fops, msm_hsl_loopback_enable_get,
			msm_hsl_loopback_enable_set, "%llu\n");
/*
 * msm_serial_hsl debugfs node: <debugfs_root>/msm_serial_hsl/loopback.<id>
 * writing 1 turns on internal loopback mode in HW. Useful for automation
 * test scripts.
 * writing 0 disables the internal loopback mode. Default is disabled.
 */
static void msm_hsl_debugfs_init(struct msm_hsl_port *msm_uport,
								int id)
{
	char node_name[15];

	snprintf(node_name, sizeof(node_name), "loopback.%d", id);
	msm_uport->loopback_dir = debugfs_create_file(node_name,
					S_IRUGO | S_IWUSR,
					debug_base,
					msm_uport,
					&loopback_enable_fops);

	if (IS_ERR_OR_NULL(msm_uport->loopback_dir))
		pr_err("Cannot create loopback.%d debug entry", id);
}
static void msm_hsl_stop_tx(struct uart_port *port)
{
	struct msm_hsl_port *msm_hsl_port = UART_TO_MSM(port);

	msm_hsl_port->imr &= ~UARTDM_ISR_TXLEV_BMSK;
	msm_hsl_write(port, msm_hsl_port->imr,
		regmap[msm_hsl_port->ver_id][UARTDM_IMR]);
}

static void msm_hsl_start_tx(struct uart_port *port)
{
	struct msm_hsl_port *msm_hsl_port = UART_TO_MSM(port);

	if (port->suspended) {
		pr_err("%s: System is in Suspend state\n", __func__);
		return;
	}
	msm_hsl_port->imr |= UARTDM_ISR_TXLEV_BMSK;
	msm_hsl_write(port, msm_hsl_port->imr,
		regmap[msm_hsl_port->ver_id][UARTDM_IMR]);
}

static void msm_hsl_stop_rx(struct uart_port *port)
{
	struct msm_hsl_port *msm_hsl_port = UART_TO_MSM(port);

	msm_hsl_port->imr &= ~(UARTDM_ISR_RXLEV_BMSK |
			       UARTDM_ISR_RXSTALE_BMSK);
	msm_hsl_write(port, msm_hsl_port->imr,
		regmap[msm_hsl_port->ver_id][UARTDM_IMR]);
}

static void msm_hsl_enable_ms(struct uart_port *port)
{
	struct msm_hsl_port *msm_hsl_port = UART_TO_MSM(port);

	msm_hsl_port->imr |= UARTDM_ISR_DELTA_CTS_BMSK;
	msm_hsl_write(port, msm_hsl_port->imr,
		regmap[msm_hsl_port->ver_id][UARTDM_IMR]);
}

static void handle_rx(struct uart_port *port, unsigned int misr)
{
	struct tty_struct *tty = port->state->port.tty;
	unsigned int vid;
	unsigned int sr;
	int count = 0;
	struct msm_hsl_port *msm_hsl_port = UART_TO_MSM(port);

	vid = msm_hsl_port->ver_id;
	/*
	 * Handle overrun. My understanding of the hardware is that overrun
	 * is not tied to the RX buffer, so we handle the case out of band.
	 */
	if ((msm_hsl_read(port, regmap[vid][UARTDM_SR]) &
				UARTDM_SR_OVERRUN_BMSK)) {
		port->icount.overrun++;
		tty_insert_flip_char(tty->port, 0, TTY_OVERRUN);
		msm_hsl_write(port, RESET_ERROR_STATUS,
			regmap[vid][UARTDM_CR]);
	}

	if (misr & UARTDM_ISR_RXSTALE_BMSK) {
		count = msm_hsl_read(port,
			regmap[vid][UARTDM_RX_TOTAL_SNAP]) -
			msm_hsl_port->old_snap_state;
		msm_hsl_port->old_snap_state = 0;
	} else {
		count = 4 * (msm_hsl_read(port, regmap[vid][UARTDM_RFWR]));
		msm_hsl_port->old_snap_state += count;
	}

	/* and now the main RX loop */
	while (count > 0) {
		unsigned int c;
		char flag = TTY_NORMAL;

		sr = msm_hsl_read(port, regmap[vid][UARTDM_SR]);
		if ((sr & UARTDM_SR_RXRDY_BMSK) == 0) {
			msm_hsl_port->old_snap_state -= count;
			break;
		}
		c = msm_hsl_read(port, regmap[vid][UARTDM_RF]);
		if (sr & UARTDM_SR_RX_BREAK_BMSK) {
			port->icount.brk++;
			if (uart_handle_break(port))
				continue;
		} else if (sr & UARTDM_SR_PAR_FRAME_BMSK) {
			port->icount.frame++;
		} else {
			port->icount.rx++;
		}

		/* Mask conditions we're ignorning. */
		sr &= port->read_status_mask;
		if (sr & UARTDM_SR_RX_BREAK_BMSK)
			flag = TTY_BREAK;
		else if (sr & UARTDM_SR_PAR_FRAME_BMSK)
			flag = TTY_FRAME;

		/* TODO: handle sysrq */
		/* if (!uart_handle_sysrq_char(port, c)) */
		tty_insert_flip_string(tty->port, (char *) &c,
				       (count > 4) ? 4 : count);
		count -= 4;
	}

	tty_flip_buffer_push(tty->port);
}

static void handle_tx(struct uart_port *port)
{
	struct circ_buf *xmit = &port->state->xmit;
	int sent_tx;
	int tx_count;
	int x;
	unsigned int tf_pointer = 0;
	unsigned int vid;

	vid = UART_TO_MSM(port)->ver_id;
	tx_count = uart_circ_chars_pending(xmit);

	if (tx_count > (UART_XMIT_SIZE - xmit->tail))
		tx_count = UART_XMIT_SIZE - xmit->tail;
	if (tx_count >= port->fifosize)
		tx_count = port->fifosize;

	/* Handle x_char */
	if (port->x_char) {
		wait_for_xmitr(port);
		msm_hsl_write(port, tx_count + 1, regmap[vid][UARTDM_NCF_TX]);
		msm_hsl_read(port, regmap[vid][UARTDM_NCF_TX]);
		msm_hsl_write(port, port->x_char, regmap[vid][UARTDM_TF]);
		port->icount.tx++;
		port->x_char = 0;
	} else if (tx_count) {
		wait_for_xmitr(port);
		msm_hsl_write(port, tx_count, regmap[vid][UARTDM_NCF_TX]);
		msm_hsl_read(port, regmap[vid][UARTDM_NCF_TX]);
	}
	if (!tx_count) {
		msm_hsl_stop_tx(port);
		return;
	}

	while (tf_pointer < tx_count)  {
		if (unlikely(!(msm_hsl_read(port, regmap[vid][UARTDM_SR]) &
			       UARTDM_SR_TXRDY_BMSK)))
			continue;
		switch (tx_count - tf_pointer) {
		case 1: {
			x = xmit->buf[xmit->tail];
			port->icount.tx++;
			break;
		}
		case 2: {
			x = xmit->buf[xmit->tail]
				| xmit->buf[xmit->tail+1] << 8;
			port->icount.tx += 2;
			break;
		}
		case 3: {
			x = xmit->buf[xmit->tail]
				| xmit->buf[xmit->tail+1] << 8
				| xmit->buf[xmit->tail + 2] << 16;
			port->icount.tx += 3;
			break;
		}
		default: {
			x = *((int *)&(xmit->buf[xmit->tail]));
			port->icount.tx += 4;
			break;
		}
		}
		msm_hsl_write(port, x, regmap[vid][UARTDM_TF]);
		xmit->tail = ((tx_count - tf_pointer < 4) ?
			      (tx_count - tf_pointer + xmit->tail) :
			      (xmit->tail + 4)) & (UART_XMIT_SIZE - 1);
		tf_pointer += 4;
		sent_tx = 1;
	}

	if (uart_circ_empty(xmit))
		msm_hsl_stop_tx(port);

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(port);

}

static void handle_delta_cts(struct uart_port *port)
{
	unsigned int vid = UART_TO_MSM(port)->ver_id;

	msm_hsl_write(port, RESET_CTS, regmap[vid][UARTDM_CR]);
	port->icount.cts++;
	wake_up_interruptible(&port->state->port.delta_msr_wait);
}

static irqreturn_t msm_hsl_irq(int irq, void *dev_id)
{
	struct uart_port *port = dev_id;
	struct msm_hsl_port *msm_hsl_port = UART_TO_MSM(port);
	unsigned int vid;
	unsigned int misr;
	unsigned long flags;

	spin_lock_irqsave(&port->lock, flags);
	vid = msm_hsl_port->ver_id;
	misr = msm_hsl_read(port, regmap[vid][UARTDM_MISR]);
	/* disable interrupt */
	msm_hsl_write(port, 0, regmap[vid][UARTDM_IMR]);

	if (misr & (UARTDM_ISR_RXSTALE_BMSK | UARTDM_ISR_RXLEV_BMSK)) {
		handle_rx(port, misr);
		if (misr & (UARTDM_ISR_RXSTALE_BMSK))
			msm_hsl_write(port, RESET_STALE_INT,
					regmap[vid][UARTDM_CR]);
		msm_hsl_write(port, 6500, regmap[vid][UARTDM_DMRX]);
		msm_hsl_write(port, STALE_EVENT_ENABLE, regmap[vid][UARTDM_CR]);
	}
	if (misr & UARTDM_ISR_TXLEV_BMSK)
		handle_tx(port);

	if (misr & UARTDM_ISR_DELTA_CTS_BMSK)
		handle_delta_cts(port);

	/* restore interrupt */
	msm_hsl_write(port, msm_hsl_port->imr, regmap[vid][UARTDM_IMR]);
	spin_unlock_irqrestore(&port->lock, flags);

	return IRQ_HANDLED;
}

static unsigned int msm_hsl_tx_empty(struct uart_port *port)
{
	unsigned int ret;
	unsigned int vid = UART_TO_MSM(port)->ver_id;

	ret = (msm_hsl_read(port, regmap[vid][UARTDM_SR]) &
	       UARTDM_SR_TXEMT_BMSK) ? TIOCSER_TEMT : 0;
	return ret;
}

static void msm_hsl_reset(struct uart_port *port)
{
	unsigned int vid = UART_TO_MSM(port)->ver_id;

	/* reset everything */
	msm_hsl_write(port, RESET_RX, regmap[vid][UARTDM_CR]);
	msm_hsl_write(port, RESET_TX, regmap[vid][UARTDM_CR]);
	msm_hsl_write(port, RESET_ERROR_STATUS, regmap[vid][UARTDM_CR]);
	msm_hsl_write(port, RESET_BREAK_INT, regmap[vid][UARTDM_CR]);
	msm_hsl_write(port, RESET_CTS, regmap[vid][UARTDM_CR]);
	msm_hsl_write(port, RFR_LOW, regmap[vid][UARTDM_CR]);
}

static unsigned int msm_hsl_get_mctrl(struct uart_port *port)
{
	return TIOCM_CAR | TIOCM_CTS | TIOCM_DSR | TIOCM_RTS;
}

static void msm_hsl_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
	unsigned int vid = UART_TO_MSM(port)->ver_id;
	unsigned int mr;
	unsigned int loop_mode;

	mr = msm_hsl_read(port, regmap[vid][UARTDM_MR1]);

	if (!(mctrl & TIOCM_RTS)) {
		mr &= ~UARTDM_MR1_RX_RDY_CTL_BMSK;
		msm_hsl_write(port, mr, regmap[vid][UARTDM_MR1]);
		msm_hsl_write(port, RFR_HIGH, regmap[vid][UARTDM_CR]);
	} else {
		mr |= UARTDM_MR1_RX_RDY_CTL_BMSK;
		msm_hsl_write(port, mr, regmap[vid][UARTDM_MR1]);
	}

	loop_mode = TIOCM_LOOP & mctrl;
	if (loop_mode) {
		mr = msm_hsl_read(port, regmap[vid][UARTDM_MR2]);
		mr |= UARTDM_MR2_LOOP_MODE_BMSK;
		msm_hsl_write(port, mr, regmap[vid][UARTDM_MR2]);

		/* Reset TX */
		msm_hsl_reset(port);

		/* Turn on Uart Receiver & Transmitter*/
		msm_hsl_write(port, UARTDM_CR_RX_EN_BMSK
		      | UARTDM_CR_TX_EN_BMSK, regmap[vid][UARTDM_CR]);
	}
}

static void msm_hsl_break_ctl(struct uart_port *port, int break_ctl)
{
	unsigned int vid = UART_TO_MSM(port)->ver_id;

	if (break_ctl)
		msm_hsl_write(port, START_BREAK, regmap[vid][UARTDM_CR]);
	else
		msm_hsl_write(port, STOP_BREAK, regmap[vid][UARTDM_CR]);
}

/**
 * msm_hsl_set_baud_rate: set requested baud rate
 * @port: uart port
 * @baud: baud rate to set (in bps)
 */
static void msm_hsl_set_baud_rate(struct uart_port *port,
						unsigned int baud)
{
	unsigned int baud_code, rxstale, watermark;
	unsigned int data;
	unsigned int vid;
	struct msm_hsl_port *msm_hsl_port = UART_TO_MSM(port);

	switch (baud) {
	case 300:
		baud_code = UARTDM_CSR_75;
		rxstale = 1;
		break;
	case 600:
		baud_code = UARTDM_CSR_150;
		rxstale = 1;
		break;
	case 1200:
		baud_code = UARTDM_CSR_300;
		rxstale = 1;
		break;
	case 2400:
		baud_code = UARTDM_CSR_600;
		rxstale = 1;
		break;
	case 4800:
		baud_code = UARTDM_CSR_1200;
		rxstale = 1;
		break;
	case 9600:
		baud_code = UARTDM_CSR_2400;
		rxstale = 2;
		break;
	case 14400:
		baud_code = UARTDM_CSR_3600;
		rxstale = 3;
		break;
	case 19200:
		baud_code = UARTDM_CSR_4800;
		rxstale = 4;
		break;
	case 28800:
		baud_code = UARTDM_CSR_7200;
		rxstale = 6;
		break;
	case 38400:
		baud_code = UARTDM_CSR_9600;
		rxstale = 8;
		break;
	case 57600:
		baud_code = UARTDM_CSR_14400;
		rxstale = 16;
		break;
	case 115200:
		baud_code = UARTDM_CSR_28800;
		rxstale = 31;
		break;
	case 230400:
		baud_code = UARTDM_CSR_57600;
		rxstale = 31;
		break;
	case 460800:
		baud_code = UARTDM_CSR_115200;
		rxstale = 31;
		break;
	case 4000000:
	case 3686400:
	case 3200000:
	case 3500000:
	case 3000000:
	case 2500000:
	case 1500000:
	case 1152000:
	case 1000000:
	case 921600:
		baud_code = 0xff;
		rxstale = 31;
		break;
	default: /*115200 baud rate */
		baud_code = UARTDM_CSR_28800;
		rxstale = 31;
		break;
	}

	vid = msm_hsl_port->ver_id;
	msm_hsl_write(port, baud_code, regmap[vid][UARTDM_CSR]);

	/*
	 * uart baud rate depends on CSR and MND Values
	 * we are updating CSR before and then calling
	 * clk_set_rate which updates MND Values. Hence
	 * dsb requires here.
	 */
	mb();

	/*
	 * Check requested baud rate and for higher baud rate than 460800,
	 * calculate required uart clock frequency and set the same.
	 */
	if (baud > 460800)
		port->uartclk = baud * 16;
	else
		port->uartclk = 7372800;

	if (clk_set_rate(msm_hsl_port->clk, port->uartclk)) {
		pr_err("Error: setting uartclk rate %u\n", port->uartclk);
		WARN_ON(1);
		return;
	}

	/* Set timeout to be ~600x the character transmit time */
	msm_hsl_port->tx_timeout = (1000000000 / baud) * 6;

	/* RX stale watermark */
	watermark = UARTDM_IPR_STALE_LSB_BMSK & rxstale;
	watermark |= UARTDM_IPR_STALE_TIMEOUT_MSB_BMSK & (rxstale << 2);
	msm_hsl_write(port, watermark, regmap[vid][UARTDM_IPR]);

	/* Set RX watermark
	 * Configure Rx Watermark as 3/4 size of Rx FIFO.
	 * RFWR register takes value in Words for UARTDM Core
	 * whereas it is consider to be in Bytes for UART Core.
	 * Hence configuring Rx Watermark as 48 Words.
	 */
	watermark = (port->fifosize * 3) / 4;
	msm_hsl_write(port, watermark, regmap[vid][UARTDM_RFWR]);

	/* set TX watermark */
	msm_hsl_write(port, 0, regmap[vid][UARTDM_TFWR]);

	msm_hsl_write(port, CR_PROTECTION_EN, regmap[vid][UARTDM_CR]);
	msm_hsl_reset(port);

	data = UARTDM_CR_TX_EN_BMSK;
	data |= UARTDM_CR_RX_EN_BMSK;
	/* enable TX & RX */
	msm_hsl_write(port, data, regmap[vid][UARTDM_CR]);

	msm_hsl_write(port, RESET_STALE_INT, regmap[vid][UARTDM_CR]);
	/* turn on RX and CTS interrupts */
	msm_hsl_port->imr = UARTDM_ISR_RXSTALE_BMSK
		| UARTDM_ISR_DELTA_CTS_BMSK | UARTDM_ISR_RXLEV_BMSK;
	msm_hsl_write(port, msm_hsl_port->imr, regmap[vid][UARTDM_IMR]);
	msm_hsl_write(port, 6500, regmap[vid][UARTDM_DMRX]);
	msm_hsl_write(port, STALE_EVENT_ENABLE, regmap[vid][UARTDM_CR]);
}

static void msm_hsl_init_clock(struct uart_port *port)
{
	clk_en(port, 1);
}

static void msm_hsl_deinit_clock(struct uart_port *port)
{
	clk_en(port, 0);
}

static int msm_hsl_startup(struct uart_port *port)
{
	struct msm_hsl_port *msm_hsl_port = UART_TO_MSM(port);
	struct platform_device *pdev = to_platform_device(port->dev);
	const struct msm_serial_hslite_platform_data *pdata =
					pdev->dev.platform_data;
	unsigned int data, rfr_level;
	unsigned int vid;
	int ret;
	unsigned long flags;

	snprintf(msm_hsl_port->name, sizeof(msm_hsl_port->name),
		 "msm_serial_hsl%d", port->line);

	if (!(is_console(port)) || (!port->cons) ||
		(port->cons && (!(port->cons->flags & CON_ENABLED)))) {

		if (msm_serial_hsl_has_gsbi(port))
			set_gsbi_uart_func_mode(port);

		if (pdata && pdata->use_pm)
			wake_lock(&msm_hsl_port->port_open_wake_lock);

		if (pdata && pdata->config_gpio) {
			ret = msm_hsl_config_uart_gpios(port);
			if (ret) {
				msm_hsl_unconfig_uart_gpios(port);
				goto release_wakelock;
			}
		}
	}

	/*
	 * Set RFR Level as 3/4 of UARTDM FIFO Size
	 * i.e. 48 Words = 192 bytes as Rx FIFO is 64 words ( 256 bytes).
	 */
	if (likely(port->fifosize > 48))
		rfr_level = port->fifosize - 16;
	else
		rfr_level = port->fifosize;

	spin_lock_irqsave(&port->lock, flags);

	vid = msm_hsl_port->ver_id;
	/* set automatic RFR level */
	data = msm_hsl_read(port, regmap[vid][UARTDM_MR1]);
	data &= ~UARTDM_MR1_AUTO_RFR_LEVEL1_BMSK;
	data &= ~UARTDM_MR1_AUTO_RFR_LEVEL0_BMSK;
	data |= UARTDM_MR1_AUTO_RFR_LEVEL1_BMSK & (rfr_level << 2);
	data |= UARTDM_MR1_AUTO_RFR_LEVEL0_BMSK & rfr_level;
	msm_hsl_write(port, data, regmap[vid][UARTDM_MR1]);
	spin_unlock_irqrestore(&port->lock, flags);

	ret = request_irq(port->irq, msm_hsl_irq, IRQF_TRIGGER_HIGH,
			  msm_hsl_port->name, port);
	if (unlikely(ret)) {
		pr_err("failed to request_irq\n");
		msm_hsl_unconfig_uart_gpios(port);
		goto release_wakelock;
	}

	return ret;

release_wakelock:
	if (pdata && pdata->use_pm)
		wake_unlock(&msm_hsl_port->port_open_wake_lock);

	return ret;
}

static void msm_hsl_shutdown(struct uart_port *port)
{
	struct msm_hsl_port *msm_hsl_port = UART_TO_MSM(port);
	struct platform_device *pdev = to_platform_device(port->dev);
	const struct msm_serial_hslite_platform_data *pdata =
					pdev->dev.platform_data;

	msm_hsl_port->imr = 0;
	/* disable interrupts */
	msm_hsl_write(port, 0, regmap[msm_hsl_port->ver_id][UARTDM_IMR]);

	free_irq(port->irq, port);

	if (!(is_console(port)) || (!port->cons) ||
		(port->cons && (!(port->cons->flags & CON_ENABLED)))) {
		/* Free UART GPIOs */
		if (pdata && pdata->config_gpio)
			msm_hsl_unconfig_uart_gpios(port);

		if (pdata && pdata->use_pm)
			wake_unlock(&msm_hsl_port->port_open_wake_lock);
	}
}

static void msm_hsl_set_termios(struct uart_port *port,
				struct ktermios *termios,
				struct ktermios *old)
{
	unsigned int baud, mr;
	unsigned int vid;
	struct msm_hsl_port *msm_hsl_port = UART_TO_MSM(port);

	if (!termios->c_cflag)
		return;

	mutex_lock(&msm_hsl_port->clk_mutex);

	/*
	 * Calculate and set baud rate
	 * 300 is the minimum and 4 Mbps is the maximum baud rate
	 * supported by driver.
	 */
	baud = uart_get_baud_rate(port, termios, old, 200, 4000000);

	/*
	 * Due to non-availability of 3.2 Mbps baud rate as standard baud rate
	 * with TTY/serial core. Map 200 BAUD to 3.2 Mbps
	 */
	if (baud == 200)
		baud = 3200000;

	msm_hsl_set_baud_rate(port, baud);

	vid = UART_TO_MSM(port)->ver_id;
	/* calculate parity */
	mr = msm_hsl_read(port, regmap[vid][UARTDM_MR2]);
	mr &= ~UARTDM_MR2_PARITY_MODE_BMSK;
	if (termios->c_cflag & PARENB) {
		if (termios->c_cflag & PARODD)
			mr |= ODD_PARITY;
		else if (termios->c_cflag & CMSPAR)
			mr |= SPACE_PARITY;
		else
			mr |= EVEN_PARITY;
	}

	/* calculate bits per char */
	mr &= ~UARTDM_MR2_BITS_PER_CHAR_BMSK;
	switch (termios->c_cflag & CSIZE) {
	case CS5:
		mr |= FIVE_BPC;
		break;
	case CS6:
		mr |= SIX_BPC;
		break;
	case CS7:
		mr |= SEVEN_BPC;
		break;
	case CS8:
	default:
		mr |= EIGHT_BPC;
		break;
	}

	/* calculate stop bits */
	mr &= ~(STOP_BIT_ONE | STOP_BIT_TWO);
	if (termios->c_cflag & CSTOPB)
		mr |= STOP_BIT_TWO;
	else
		mr |= STOP_BIT_ONE;

	/* set parity, bits per char, and stop bit */
	msm_hsl_write(port, mr, regmap[vid][UARTDM_MR2]);

	/* calculate and set hardware flow control */
	mr = msm_hsl_read(port, regmap[vid][UARTDM_MR1]);
	mr &= ~(UARTDM_MR1_CTS_CTL_BMSK | UARTDM_MR1_RX_RDY_CTL_BMSK);
	if (termios->c_cflag & CRTSCTS) {
		mr |= UARTDM_MR1_CTS_CTL_BMSK;
		mr |= UARTDM_MR1_RX_RDY_CTL_BMSK;
	}
	msm_hsl_write(port, mr, regmap[vid][UARTDM_MR1]);

	/* Configure status bits to ignore based on termio flags. */
	port->read_status_mask = 0;
	if (termios->c_iflag & INPCK)
		port->read_status_mask |= UARTDM_SR_PAR_FRAME_BMSK;
	if (termios->c_iflag & (BRKINT | PARMRK))
		port->read_status_mask |= UARTDM_SR_RX_BREAK_BMSK;

	uart_update_timeout(port, termios->c_cflag, baud);

	mutex_unlock(&msm_hsl_port->clk_mutex);
}

static const char *msm_hsl_type(struct uart_port *port)
{
	return "MSM";
}

static void msm_hsl_release_port(struct uart_port *port)
{
	struct msm_hsl_port *msm_hsl_port = UART_TO_MSM(port);
	struct platform_device *pdev = to_platform_device(port->dev);
	struct resource *uart_resource;
	resource_size_t size;

	uart_resource = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						     "uartdm_resource");
	if (!uart_resource)
		uart_resource = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (unlikely(!uart_resource))
		return;
	size = uart_resource->end - uart_resource->start + 1;

	release_mem_region(port->mapbase, size);
	iounmap(port->membase);
	port->membase = NULL;

	if (msm_serial_hsl_has_gsbi(port)) {
		iowrite32(GSBI_PROTOCOL_IDLE, msm_hsl_port->mapped_gsbi +
			  GSBI_CONTROL_ADDR);
		iounmap(msm_hsl_port->mapped_gsbi);
		msm_hsl_port->mapped_gsbi = NULL;
	}
}

static int msm_hsl_request_port(struct uart_port *port)
{
	struct msm_hsl_port *msm_hsl_port = UART_TO_MSM(port);
	struct platform_device *pdev = to_platform_device(port->dev);
	struct resource *uart_resource;
	struct resource *gsbi_resource;
	resource_size_t size;

	uart_resource = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						     "uartdm_resource");
	if (!uart_resource)
		uart_resource = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (unlikely(!uart_resource)) {
		pr_err("can't get uartdm resource\n");
		return -ENXIO;
	}
	size = uart_resource->end - uart_resource->start + 1;

	if (unlikely(!request_mem_region(port->mapbase, size,
					 "msm_serial_hsl"))) {
		pr_err("can't get mem region for uartdm\n");
		return -EBUSY;
	}

	port->membase = ioremap(port->mapbase, size);
	if (!port->membase) {
		release_mem_region(port->mapbase, size);
		return -EBUSY;
	}

	if (msm_serial_hsl_has_gsbi(port)) {
		gsbi_resource = platform_get_resource_byname(pdev,
							     IORESOURCE_MEM,
							     "gsbi_resource");
		if (!gsbi_resource)
			gsbi_resource = platform_get_resource(pdev,
						IORESOURCE_MEM, 1);
		if (unlikely(!gsbi_resource)) {
			pr_err("can't get gsbi resource\n");
			return -ENXIO;
		}

		size = gsbi_resource->end - gsbi_resource->start + 1;
		msm_hsl_port->mapped_gsbi = ioremap(gsbi_resource->start,
						    size);
		if (!msm_hsl_port->mapped_gsbi) {
			return -EBUSY;
		}
	}

	return 0;
}

static void msm_hsl_config_port(struct uart_port *port, int flags)
{
	if (flags & UART_CONFIG_TYPE) {
		port->type = PORT_MSM;
		if (msm_hsl_request_port(port))
			return;
	}

	/* Configure required GSBI based UART protocol. */
	if (msm_serial_hsl_has_gsbi(port))
		set_gsbi_uart_func_mode(port);
}

static int msm_hsl_verify_port(struct uart_port *port,
			       struct serial_struct *ser)
{
	if (unlikely(ser->type != PORT_UNKNOWN && ser->type != PORT_MSM))
		return -EINVAL;
	if (unlikely(port->irq != ser->irq))
		return -EINVAL;
	return 0;
}

static void msm_hsl_power(struct uart_port *port, unsigned int state,
			  unsigned int oldstate)
{
	int ret;
	struct msm_hsl_port *msm_hsl_port = UART_TO_MSM(port);
	struct platform_device *pdev = to_platform_device(port->dev);
	const struct msm_serial_hslite_platform_data *pdata =
					pdev->dev.platform_data;

	switch (state) {
	case 0:
		ret = clk_set_rate(msm_hsl_port->clk, port->uartclk);
		if (ret)
			pr_err("Error setting UART clock rate to %u\n",
							port->uartclk);
		clk_en(port, 1);
		break;
	case 3:
		clk_en(port, 0);
		if (pdata && pdata->set_uart_clk_zero) {
			ret = clk_set_rate(msm_hsl_port->clk, 0);
			if (ret)
				pr_err("Error setting UART clock rate to zero.\n");
		}
		break;
	default:
		pr_err("Unknown PM state %d\n", state);
	}
}

static struct uart_ops msm_hsl_uart_pops = {
	.tx_empty = msm_hsl_tx_empty,
	.set_mctrl = msm_hsl_set_mctrl,
	.get_mctrl = msm_hsl_get_mctrl,
	.stop_tx = msm_hsl_stop_tx,
	.start_tx = msm_hsl_start_tx,
	.stop_rx = msm_hsl_stop_rx,
	.enable_ms = msm_hsl_enable_ms,
	.break_ctl = msm_hsl_break_ctl,
	.startup = msm_hsl_startup,
	.shutdown = msm_hsl_shutdown,
	.set_termios = msm_hsl_set_termios,
	.type = msm_hsl_type,
	.release_port = msm_hsl_release_port,
	.request_port = msm_hsl_request_port,
	.config_port = msm_hsl_config_port,
	.verify_port = msm_hsl_verify_port,
	.pm = msm_hsl_power,
};

static struct msm_hsl_port msm_hsl_uart_ports[] = {
	{
		.uart = {
			.iotype = UPIO_MEM,
			.ops = &msm_hsl_uart_pops,
			.flags = UPF_BOOT_AUTOCONF,
			.fifosize = 64,
			.line = 0,
		},
	},
	{
		.uart = {
			.iotype = UPIO_MEM,
			.ops = &msm_hsl_uart_pops,
			.flags = UPF_BOOT_AUTOCONF,
			.fifosize = 64,
			.line = 1,
		},
	},
	{
		.uart = {
			.iotype = UPIO_MEM,
			.ops = &msm_hsl_uart_pops,
			.flags = UPF_BOOT_AUTOCONF,
			.fifosize = 64,
			.line = 2,
		},
	},
};

#define UART_NR	ARRAY_SIZE(msm_hsl_uart_ports)

static inline struct uart_port *get_port_from_line(unsigned int line)
{
	return &msm_hsl_uart_ports[line].uart;
}

static unsigned int msm_hsl_console_state[8];

static void dump_hsl_regs(struct uart_port *port)
{
	struct msm_hsl_port *msm_hsl_port = UART_TO_MSM(port);
	unsigned int vid = msm_hsl_port->ver_id;
	unsigned int sr, isr, mr1, mr2, ncf, txfs, rxfs, con_state;

	sr = msm_hsl_read(port, regmap[vid][UARTDM_SR]);
	isr = msm_hsl_read(port, regmap[vid][UARTDM_ISR]);
	mr1 = msm_hsl_read(port, regmap[vid][UARTDM_MR1]);
	mr2 = msm_hsl_read(port, regmap[vid][UARTDM_MR2]);
	ncf = msm_hsl_read(port, regmap[vid][UARTDM_NCF_TX]);
	txfs = msm_hsl_read(port, regmap[vid][UARTDM_TXFS]);
	rxfs = msm_hsl_read(port, regmap[vid][UARTDM_RXFS]);
	con_state = get_console_state(port);

	msm_hsl_console_state[0] = sr;
	msm_hsl_console_state[1] = isr;
	msm_hsl_console_state[2] = mr1;
	msm_hsl_console_state[3] = mr2;
	msm_hsl_console_state[4] = ncf;
	msm_hsl_console_state[5] = txfs;
	msm_hsl_console_state[6] = rxfs;
	msm_hsl_console_state[7] = con_state;

	pr_info("Timeout: %d uS\n", msm_hsl_port->tx_timeout);
	pr_info("SR:  %08x\n", sr);
	pr_info("ISR: %08x\n", isr);
	pr_info("MR1: %08x\n", mr1);
	pr_info("MR2: %08x\n", mr2);
	pr_info("NCF: %08x\n", ncf);
	pr_info("TXFS: %08x\n", txfs);
	pr_info("RXFS: %08x\n", rxfs);
	pr_info("Console state: %d\n", con_state);
}

/*
 *  Wait for transmitter & holding register to empty
 *  Derived from wait_for_xmitr in 8250 serial driver by Russell King  */
static inline void wait_for_xmitr(struct uart_port *port)
{
	struct msm_hsl_port *msm_hsl_port = UART_TO_MSM(port);
	unsigned int vid = msm_hsl_port->ver_id;
	int count = 0;

	if (!(msm_hsl_read(port, regmap[vid][UARTDM_SR]) &
			UARTDM_SR_TXEMT_BMSK)) {
		while (!(msm_hsl_read(port, regmap[vid][UARTDM_ISR]) &
			UARTDM_ISR_TX_READY_BMSK) &&
		       !(msm_hsl_read(port, regmap[vid][UARTDM_SR]) &
			UARTDM_SR_TXEMT_BMSK)) {
			udelay(1);
			touch_nmi_watchdog();
			cpu_relax();
			if (++count == msm_hsl_port->tx_timeout) {
				pr_info("%s: UART TX Stuck, Resetting TX\n",
								 __func__);
				msm_hsl_write(port, RESET_TX,
					regmap[vid][UARTDM_CR]);
				mb();
				dump_hsl_regs(port);
				break;
			}
		}
		msm_hsl_write(port, CLEAR_TX_READY, regmap[vid][UARTDM_CR]);
	}
}

#ifdef CONFIG_SERIAL_MSM_HSL_CONSOLE
static void msm_hsl_console_putchar(struct uart_port *port, int ch)
{
	unsigned int vid = UART_TO_MSM(port)->ver_id;

	wait_for_xmitr(port);
	msm_hsl_write(port, 1, regmap[vid][UARTDM_NCF_TX]);
	/*
	 * Dummy read to add 1 AHB clock delay to fix UART hardware bug.
	 * Bug: Delay required on TX-transfer-init. after writing to
	 * NO_CHARS_FOR_TX register.
	 */
	msm_hsl_read(port, regmap[vid][UARTDM_SR]);
	msm_hsl_write(port, ch, regmap[vid][UARTDM_TF]);
}

static void msm_hsl_console_write(struct console *co, const char *s,
				  unsigned int count)
{
	struct uart_port *port;
	struct msm_hsl_port *msm_hsl_port;
	unsigned int vid;
	int locked;

	BUG_ON(co->index < 0 || co->index >= UART_NR);

	port = get_port_from_line(co->index);
	msm_hsl_port = UART_TO_MSM(port);
	vid = msm_hsl_port->ver_id;

	/* not pretty, but we can end up here via various convoluted paths */
	if (port->sysrq || oops_in_progress)
		locked = spin_trylock(&port->lock);
	else {
		locked = 1;
		spin_lock(&port->lock);
	}
	msm_hsl_write(port, 0, regmap[vid][UARTDM_IMR]);
	uart_console_write(port, s, count, msm_hsl_console_putchar);
	msm_hsl_write(port, msm_hsl_port->imr, regmap[vid][UARTDM_IMR]);
	if (locked == 1)
		spin_unlock(&port->lock);
}

static int msm_hsl_console_setup(struct console *co, char *options)
{
	struct uart_port *port;
	unsigned int vid;
	int baud = 0, flow, bits, parity, mr2;
	int ret;

	if (unlikely(co->index >= UART_NR || co->index < 0))
		return -ENXIO;

	port = get_port_from_line(co->index);
	vid = UART_TO_MSM(port)->ver_id;

	if (unlikely(!port->membase))
		return -ENXIO;

	port->cons = co;

	pm_runtime_get_noresume(port->dev);

#ifndef CONFIG_PM_RUNTIME
	msm_hsl_init_clock(port);
#endif
	pm_runtime_resume(port->dev);

	if (options)
		uart_parse_options(options, &baud, &parity, &bits, &flow);

	bits = 8;
	parity = 'n';
	flow = 'n';
	msm_hsl_write(port, UARTDM_MR2_BITS_PER_CHAR_8 | STOP_BIT_ONE,
		      regmap[vid][UARTDM_MR2]);	/* 8N1 */

	if (baud < 300 || baud > 115200)
		baud = 115200;
	msm_hsl_set_baud_rate(port, baud);

	ret = uart_set_options(port, co, baud, parity, bits, flow);

	mr2 = msm_hsl_read(port, regmap[vid][UARTDM_MR2]);
	mr2 |= UARTDM_MR2_RX_ERROR_CHAR_OFF;
	mr2 |= UARTDM_MR2_RX_BREAK_ZERO_CHAR_OFF;
	msm_hsl_write(port, mr2, regmap[vid][UARTDM_MR2]);

	msm_hsl_reset(port);
	/* Enable transmitter */
	msm_hsl_write(port, CR_PROTECTION_EN, regmap[vid][UARTDM_CR]);
	msm_hsl_write(port, UARTDM_CR_TX_EN_BMSK, regmap[vid][UARTDM_CR]);

	msm_hsl_write(port, 1, regmap[vid][UARTDM_NCF_TX]);
	msm_hsl_read(port, regmap[vid][UARTDM_NCF_TX]);

	pr_info("console setup on port #%d\n", port->line);

	return ret;
}

static struct uart_driver msm_hsl_uart_driver;

static struct console msm_hsl_console = {
	.name = "ttyHSL",
	.write = msm_hsl_console_write,
	.device = uart_console_device,
	.setup = msm_hsl_console_setup,
	.flags = CON_PRINTBUFFER,
	.index = -1,
	.data = &msm_hsl_uart_driver,
};

#define MSM_HSL_CONSOLE	(&msm_hsl_console)
/*
 * get_console_state - check the per-port serial console state.
 * @port: uart_port structure describing the port
 *
 * Return the state of serial console availability on port.
 * return 1: If serial console is enabled on particular UART port.
 * return 0: If serial console is disabled on particular UART port.
 */
static int get_console_state(struct uart_port *port)
{
	if (is_console(port) && (port->cons->flags & CON_ENABLED))
		return 1;
	else
		return 0;
}

/* show_msm_console - provide per-port serial console state. */
static ssize_t show_msm_console(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int enable;
	struct uart_port *port;

	struct platform_device *pdev = to_platform_device(dev);
	port = get_port_from_line(get_line(pdev));

	enable = get_console_state(port);

	return snprintf(buf, sizeof(enable), "%d\n", enable);
}

/*
 * set_msm_console - allow to enable/disable serial console on port.
 *
 * writing 1 enables serial console on UART port.
 * writing 0 disables serial console on UART port.
 */
static ssize_t set_msm_console(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int enable, cur_state;
	struct uart_port *port;

	struct platform_device *pdev = to_platform_device(dev);
	port = get_port_from_line(get_line(pdev));

	cur_state = get_console_state(port);
	enable = buf[0] - '0';

	if (enable == cur_state)
		return count;

	switch (enable) {
	case 0:
		pr_debug("Calling stop_console\n");
		console_stop(port->cons);
		pr_debug("Calling unregister_console\n");
		unregister_console(port->cons);
		pm_runtime_put_sync(&pdev->dev);
		pm_runtime_disable(&pdev->dev);
		/*
		 * Disable UART Core clk
		 * 3 - to disable the UART clock
		 * Thid parameter is not used here, but used in serial core.
		 */
		msm_hsl_power(port, 3, 1);
		break;
	case 1:
		pr_debug("Calling register_console\n");
		/*
		 * Disable UART Core clk
		 * 0 - to enable the UART clock
		 * Thid parameter is not used here, but used in serial core.
		 */
		msm_hsl_power(port, 0, 1);
		pm_runtime_enable(&pdev->dev);
		register_console(port->cons);
		break;
	default:
		return -EINVAL;
	}

	return count;
}
static DEVICE_ATTR(console, S_IWUSR | S_IRUGO, show_msm_console,
						set_msm_console);
#else
#define MSM_HSL_CONSOLE	NULL
#endif

static struct uart_driver msm_hsl_uart_driver = {
	.owner = THIS_MODULE,
	.driver_name = "msm_serial_hsl",
	.dev_name = "ttyHSL",
	.nr = UART_NR,
	.cons = MSM_HSL_CONSOLE,
};

static struct msm_serial_hslite_platform_data
		*msm_hsl_dt_to_pdata(struct platform_device *pdev)
{
	int ret;
	struct device_node *node = pdev->dev.of_node;
	struct msm_serial_hslite_platform_data *pdata;

	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		pr_err("unable to allocate memory for platform data\n");
		return ERR_PTR(-ENOMEM);
	}

	ret = of_property_read_u32(node, "qcom,config-gpio",
				&pdata->config_gpio);
	if (ret && ret != -EINVAL) {
		pr_err("Error with config_gpio property.\n");
		return ERR_PTR(ret);
	}

	if (pdata->config_gpio) {
		pdata->uart_tx_gpio = of_get_named_gpio(node,
					"qcom,tx-gpio", 0);
		if (pdata->uart_tx_gpio < 0)
				return ERR_PTR(pdata->uart_tx_gpio);

		pdata->uart_rx_gpio = of_get_named_gpio(node,
					"qcom,rx-gpio", 0);
		if (pdata->uart_rx_gpio < 0)
				return ERR_PTR(pdata->uart_rx_gpio);

		/* check if 4-wire UART, then get cts/rfr GPIOs. */
		if (pdata->config_gpio == 4) {
			pdata->uart_cts_gpio = of_get_named_gpio(node,
						"qcom,cts-gpio", 0);
			if (pdata->uart_cts_gpio < 0)
				return ERR_PTR(pdata->uart_cts_gpio);

			pdata->uart_rfr_gpio = of_get_named_gpio(node,
						"qcom,rfr-gpio", 0);
			if (pdata->uart_rfr_gpio < 0)
				return ERR_PTR(pdata->uart_rfr_gpio);
		}
	}

	pdata->use_pm = of_property_read_bool(node, "qcom,use-pm");

	return pdata;
}

static atomic_t msm_serial_hsl_next_id = ATOMIC_INIT(0);

static int msm_serial_hsl_probe(struct platform_device *pdev)
{
	struct msm_hsl_port *msm_hsl_port;
	struct resource *uart_resource;
	struct resource *gsbi_resource;
	struct uart_port *port;
	struct msm_serial_hslite_platform_data *pdata;
	const struct of_device_id *match;
	u32 line;
	int ret;

	if (pdev->id == -1)
		pdev->id = atomic_inc_return(&msm_serial_hsl_next_id) - 1;

	/* Use line (ttyHSLx) number from pdata or device tree if specified */
	pdata = pdev->dev.platform_data;
	if (pdata)
		line = pdata->line;
	else
		line = pdev->id;

	/* Use line number from device tree alias if present */
	if (pdev->dev.of_node) {
		dev_dbg(&pdev->dev, "device tree enabled\n");
		ret = of_alias_get_id(pdev->dev.of_node, "serial");
		if (ret >= 0)
			line = ret;

		pdata = msm_hsl_dt_to_pdata(pdev);
		if (IS_ERR(pdata))
			return PTR_ERR(pdata);

		pdev->dev.platform_data = pdata;
	}

	if (unlikely(line < 0 || line >= UART_NR))
		return -ENXIO;

	pr_info("detected port #%d (ttyHSL%d)\n", pdev->id, line);

	port = get_port_from_line(line);
	port->dev = &pdev->dev;
	port->uartclk = 7372800;
	msm_hsl_port = UART_TO_MSM(port);

	msm_hsl_port->clk = clk_get(&pdev->dev, "core_clk");
	if (unlikely(IS_ERR(msm_hsl_port->clk))) {
		ret = PTR_ERR(msm_hsl_port->clk);
		if (ret != -EPROBE_DEFER)
			pr_err("Error getting clk\n");
		return ret;
	}

	/* Interface clock is not required by all UART configurations.
	 * GSBI UART and BLSP UART needs interface clock but Legacy UART
	 * do not require interface clock. Hence, do not fail probe with
	 * iface clk_get failure.
	 */
	msm_hsl_port->pclk = clk_get(&pdev->dev, "iface_clk");
	if (unlikely(IS_ERR(msm_hsl_port->pclk))) {
		ret = PTR_ERR(msm_hsl_port->pclk);
		if (ret == -EPROBE_DEFER) {
			clk_put(msm_hsl_port->clk);
			return ret;
		} else {
			msm_hsl_port->pclk = NULL;
		}
	}

	/* Identify UART functional mode as 2-wire or 4-wire. */
	if (pdata && pdata->config_gpio == 4)
		msm_hsl_port->func_mode = UART_FOUR_WIRE;
	else
		msm_hsl_port->func_mode = UART_TWO_WIRE;

	match = of_match_device(msm_hsl_match_table, &pdev->dev);
	if (!match) {
		msm_hsl_port->ver_id = UARTDM_VERSION_11_13;
	} else {
		msm_hsl_port->ver_id = (unsigned long)match->data;
		/*
		 * BLSP based UART configuration is available with
		 * UARTDM v14 Revision. Hence set uart_type as UART_BLSP.
		 */
		msm_hsl_port->uart_type = BLSP_HSUART;

		msm_hsl_port->bus_scale_table = msm_bus_cl_get_pdata(pdev);
		if (!msm_hsl_port->bus_scale_table) {
			pr_err("Bus scaling is disabled\n");
		} else {
			msm_hsl_port->bus_perf_client =
				msm_bus_scale_register_client(
					msm_hsl_port->bus_scale_table);
			if (IS_ERR(&msm_hsl_port->bus_perf_client)) {
				pr_err("Bus client register failed.\n");
				ret = -EINVAL;
				goto err;
			}
		}
	}

	gsbi_resource =	platform_get_resource_byname(pdev,
						     IORESOURCE_MEM,
						     "gsbi_resource");
	if (!gsbi_resource)
		gsbi_resource = platform_get_resource(pdev, IORESOURCE_MEM, 1);

	if (gsbi_resource)
		msm_hsl_port->uart_type = GSBI_HSUART;
	else
		msm_hsl_port->uart_type = LEGACY_HSUART;


	uart_resource = platform_get_resource_byname(pdev,
						     IORESOURCE_MEM,
						     "uartdm_resource");
	if (!uart_resource)
		uart_resource = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (unlikely(!uart_resource)) {
		pr_err("getting uartdm_resource failed\n");
		return -ENXIO;
	}
	port->mapbase = uart_resource->start;

	port->irq = platform_get_irq(pdev, 0);
	if (unlikely((int)port->irq < 0)) {
		pr_err("getting irq failed\n");
		return -ENXIO;
	}

	device_set_wakeup_capable(&pdev->dev, 1);
	platform_set_drvdata(pdev, port);
	pm_runtime_enable(port->dev);
#ifdef CONFIG_SERIAL_MSM_HSL_CONSOLE
	ret = device_create_file(&pdev->dev, &dev_attr_console);
	if (unlikely(ret))
		pr_err("Can't create console attribute\n");
#endif
	msm_hsl_debugfs_init(msm_hsl_port, get_line(pdev));
	mutex_init(&msm_hsl_port->clk_mutex);
	if (pdata && pdata->use_pm)
		wake_lock_init(&msm_hsl_port->port_open_wake_lock,
				WAKE_LOCK_SUSPEND,
				"msm_serial_hslite_port_open");

	/* Temporarily increase the refcount on the GSBI clock to avoid a race
	 * condition with the earlyprintk handover mechanism.
	 */
	if (msm_hsl_port->pclk)
		clk_prepare_enable(msm_hsl_port->pclk);
	ret = uart_add_one_port(&msm_hsl_uart_driver, port);
	if (msm_hsl_port->pclk)
		clk_disable_unprepare(msm_hsl_port->pclk);

err:
	return ret;
}

static int msm_serial_hsl_remove(struct platform_device *pdev)
{
	struct msm_hsl_port *msm_hsl_port = platform_get_drvdata(pdev);
	const struct msm_serial_hslite_platform_data *pdata =
					pdev->dev.platform_data;
	struct uart_port *port;

	port = get_port_from_line(get_line(pdev));
#ifdef CONFIG_SERIAL_MSM_HSL_CONSOLE
	device_remove_file(&pdev->dev, &dev_attr_console);
#endif
	pm_runtime_put_sync(&pdev->dev);
	pm_runtime_disable(&pdev->dev);

	if (pdata && pdata->use_pm)
		wake_lock_destroy(&msm_hsl_port->port_open_wake_lock);

	device_set_wakeup_capable(&pdev->dev, 0);
	platform_set_drvdata(pdev, NULL);
	mutex_destroy(&msm_hsl_port->clk_mutex);
	uart_remove_one_port(&msm_hsl_uart_driver, port);

	clk_put(msm_hsl_port->pclk);
	clk_put(msm_hsl_port->clk);
	debugfs_remove(msm_hsl_port->loopback_dir);

	return 0;
}

#ifdef CONFIG_PM
static int msm_serial_hsl_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct uart_port *port;
	port = get_port_from_line(get_line(pdev));

	if (port) {

		if (is_console(port))
			msm_hsl_deinit_clock(port);

		uart_suspend_port(&msm_hsl_uart_driver, port);
		if (device_may_wakeup(dev))
			enable_irq_wake(port->irq);
	}

	return 0;
}

static int msm_serial_hsl_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct uart_port *port;
	port = get_port_from_line(get_line(pdev));

	if (port) {

		uart_resume_port(&msm_hsl_uart_driver, port);
		if (device_may_wakeup(dev))
			disable_irq_wake(port->irq);

		if (is_console(port))
			msm_hsl_init_clock(port);
	}

	return 0;
}
#else
#define msm_serial_hsl_suspend NULL
#define msm_serial_hsl_resume NULL
#endif

static int msm_hsl_runtime_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct uart_port *port;
	port = get_port_from_line(get_line(pdev));

	dev_dbg(dev, "pm_runtime: suspending\n");
	msm_hsl_deinit_clock(port);
	return 0;
}

static int msm_hsl_runtime_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct uart_port *port;
	port = get_port_from_line(get_line(pdev));

	dev_dbg(dev, "pm_runtime: resuming\n");
	msm_hsl_init_clock(port);
	return 0;
}

static struct dev_pm_ops msm_hsl_dev_pm_ops = {
	.suspend = msm_serial_hsl_suspend,
	.resume = msm_serial_hsl_resume,
	.runtime_suspend = msm_hsl_runtime_suspend,
	.runtime_resume = msm_hsl_runtime_resume,
};

static struct platform_driver msm_hsl_platform_driver = {
	.probe = msm_serial_hsl_probe,
	.remove = msm_serial_hsl_remove,
	.driver = {
		.name = "msm_serial_hsl",
		.owner = THIS_MODULE,
		.pm = &msm_hsl_dev_pm_ops,
		.of_match_table = msm_hsl_match_table,
	},
};

static int __init msm_serial_hsl_init(void)
{
	int ret;

	ret = uart_register_driver(&msm_hsl_uart_driver);
	if (unlikely(ret))
		return ret;

	debug_base = debugfs_create_dir("msm_serial_hsl", NULL);
	if (IS_ERR_OR_NULL(debug_base))
		pr_err("Cannot create debugfs dir\n");

	ret = platform_driver_register(&msm_hsl_platform_driver);
	if (unlikely(ret))
		uart_unregister_driver(&msm_hsl_uart_driver);

	pr_info("driver initialized\n");

	return ret;
}

static void __exit msm_serial_hsl_exit(void)
{
	debugfs_remove_recursive(debug_base);
#ifdef CONFIG_SERIAL_MSM_HSL_CONSOLE
	unregister_console(&msm_hsl_console);
#endif
	platform_driver_unregister(&msm_hsl_platform_driver);
	uart_unregister_driver(&msm_hsl_uart_driver);
}

module_init(msm_serial_hsl_init);
module_exit(msm_serial_hsl_exit);

MODULE_DESCRIPTION("Driver for msm HSUART serial device");
MODULE_LICENSE("GPL v2");
