/*
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2010-2014, The Linux Foundation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/* Acknowledgements:
   * This file is based on msm_serial.c, originally
   * Written by Robert Love <rlove@google.com> */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>

#include "uim_hs_lite.h"

#define UIM_DRIVER_NAME			"msm_uim"
#define UIM_DEVICE_NAME			"uim"
#define UIM_MAX_DEVICES			4

/* SIM clock = TCXO / 5 */
#define UIM_SIM_CLK			3840000
/* UART clock = (SIM_CLK * 4 * 16) / 372 */
#define UIM_UART_CLK			660645

/*
 * Per device information
 */

#define UIM_BUF_SIZE			256

struct uim_buf {
	unsigned char buf[UIM_BUF_SIZE];
	unsigned int head;
	unsigned int tail;
};

struct uim_hsl_port {
	char name[8];
	struct cdev cdev;
	struct device *device;
	dev_t dev_num;

	spinlock_t lock;
	struct mutex mutex;
	atomic_t ref_count;
	wait_queue_head_t rx_wq;
	wait_queue_head_t tx_wq;

	resource_size_t mapbase;
	resource_size_t mapsize;
	unsigned char __iomem *membase;
	unsigned int irq;
	unsigned int simclk;
	unsigned int uartclk;
	unsigned int fifosize;
	unsigned int tx_timeout;

	/* internal state */
	unsigned int imr;
	unsigned int simcfg;
	unsigned int old_snap_state;

	unsigned int swap_data;
	unsigned int invert_data;
	unsigned int reset;

	/* buffer */
	struct uim_buf txbuf;
	struct uim_buf rxbuf;

	/* counter */
	struct {
		unsigned int tx;
		unsigned int rx;
		unsigned int rx_overrun;
		unsigned int rx_discard;
		unsigned int brk;
		unsigned int frame;
		unsigned int irq;
	} count;

	/* debugfs */
	struct dentry *debugfs_dir;
	struct dentry *debugfs_stat;
	struct dentry *debugfs_status;

	/* clock */
	struct clk *clk;
	struct clk *pclk;
	struct clk *sclk;

	int clk_enable_count;
	u32 bus_perf_client;
	struct msm_bus_scale_pdata *bus_scale_table;

	/* device tree */
	unsigned uim_1v8;
	int uim_clk_gpio;
	int uim_data_gpio;
	int uim_card_detect_gpio;
	int uim_reset_gpio;
};

static struct dentry *debug_base;
static struct class *uim_class;
static int major;

/*
 * Primitive I/O
 */

static inline void uim_hsl_write(struct uim_hsl_port *port,
	unsigned int val, unsigned int off)
{
	__iowmb();
	__raw_writel_no_log((__force __u32)cpu_to_le32(val),
		port->membase + off);
}

static inline unsigned int uim_hsl_read(struct uim_hsl_port *port,
	unsigned int off)
{
	unsigned int v = le32_to_cpu((__force __le32)__raw_readl_no_log(
		port->membase + off));
	__iormb();
	return v;
}

/*
 * Clock
 */

static int uim_hsl_clk_enable(struct uim_hsl_port *port, int enable)
{
	int ret;

	if (enable) {
		port->clk_enable_count++;

		clk_set_rate(port->clk, port->uartclk);
		clk_set_rate(port->sclk, port->simclk);

		ret = clk_prepare_enable(port->clk);
		if (ret)
			goto err;
		ret = clk_prepare_enable(port->pclk);
		if (ret)
			goto err_pclk;
		ret = clk_prepare_enable(port->sclk);
		if (ret)
			goto err_sclk;
	} else {
		port->clk_enable_count--;
		clk_disable_unprepare(port->clk);
		clk_disable_unprepare(port->pclk);
		clk_disable_unprepare(port->sclk);
	}

	return 0;

err_sclk:
	clk_disable_unprepare(port->pclk);
err_pclk:
	clk_disable_unprepare(port->clk);
err:
	port->clk_enable_count--;

	return ret;
}

/*
 * GPIO
 */

static int uim_hsl_config_uim_gpios(struct uim_hsl_port *port)
{
	int i, ret;

	struct {
		int gpio;
		char *name;
	} gpio_list[] = {
		{ port->uim_clk_gpio, "UIM_CLK" },
		{ port->uim_data_gpio, "UIM_DATA" },
		{ port->uim_card_detect_gpio, "UIM_CARD_DETECT" },
		{ port->uim_reset_gpio, "UIM_RESET" },
	};

	for (i = 0; i < ARRAY_SIZE(gpio_list); ++i) {
		if (!gpio_is_valid(gpio_list[i].gpio))
			continue;
		ret = gpio_request(gpio_list[i].gpio,
			gpio_list[i].name);
		if (unlikely(ret)) {
			pr_err("gpio request failed for:%d\n",
				gpio_list[i].gpio);
			while (--i >= 0)
				if (gpio_is_valid(gpio_list[i].gpio))
					gpio_free(gpio_list[i].gpio);
			return ret;
		}
	}

	return 0;
}

static int uim_hsl_unconfig_uim_gpios(struct uim_hsl_port *port)
{
	int i;
	int gpio_list[] = {
		port->uim_clk_gpio,
		port->uim_data_gpio,
		port->uim_card_detect_gpio,
	};

	for (i = 0; i < ARRAY_SIZE(gpio_list); ++i)
		if (gpio_is_valid(gpio_list[i]))
			gpio_free(gpio_list[i]);

	return 0;
}


/*
 * Device interface
 */

static struct uim_hsl_port *uim_file_to_port(struct file *file)
{
	return (struct uim_hsl_port *) file->private_data;
}

static int uim_buf_is_empty(struct uim_buf *buf)
{
	return (buf->head == buf->tail);
}

static int uim_buf_is_full(struct uim_buf *buf)
{
	return (((buf->tail + 1) % UIM_BUF_SIZE) == buf->head);
}

static unsigned int uim_buf_count_data(struct uim_buf *buf)
{
	unsigned int tail;

	tail = buf->tail;
	if (tail < buf->head)
		tail += UIM_BUF_SIZE;
	return tail - buf->head;
}

static unsigned int uim_buf_count_free(struct uim_buf *buf)
{
	unsigned int data = uim_buf_count_data(buf);
	return UIM_BUF_SIZE - data - 1;
}

static unsigned int uim_buf_push(struct uim_buf *buf, unsigned char c)
{
	if (uim_buf_is_full(buf))
		return -EBUSY;
	buf->buf[buf->tail] = c;
	if (++buf->tail == UIM_BUF_SIZE)
		buf->tail = 0;

	return 0;
}

static unsigned char uim_buf_pop(struct uim_buf *buf)
{
	unsigned char c;

	if (uim_buf_is_empty(buf))
		return 0;

	c = buf->buf[buf->head];
	if (++buf->head == UIM_BUF_SIZE)
		buf->head = 0;

	return c;
}

static void uim_hsl_wait_for_xmitr(struct uim_hsl_port *port)
{
	unsigned int count = 0;

	if (!(uim_hsl_read(port, UARTDM_SR_REG) & UARTDM_SR_TXEMT_BMSK)) {
		while (!(uim_hsl_read(port, UARTDM_ISR_REG) &
			UARTDM_ISR_TX_READY_BMSK) &&
		       !(uim_hsl_read(port, UARTDM_SR_REG) &
			UARTDM_SR_TXEMT_BMSK)) {
			udelay(1);
			cpu_relax();
			if (++count == port->tx_timeout)
				panic("MSM HSL uim_hsl_wait_for_xmitr is stuck!");
		}
		uim_hsl_write(port, CLEAR_TX_READY, UARTDM_CR_REG);
	}
}

static void uim_hsl_stop_tx(struct uim_hsl_port *port)
{
	port->imr &= ~UARTDM_ISR_TXLEV_BMSK;
	uim_hsl_write(port, port->imr, UARTDM_IMR_REG);
}

static void uim_hsl_start_tx(struct uim_hsl_port *port)
{
	port->imr |= UARTDM_ISR_TXLEV_BMSK;
	uim_hsl_write(port, port->imr, UARTDM_IMR_REG);
}

static void uim_irq_rx(struct uim_hsl_port *port, unsigned int misr)
{
	unsigned int sr;
	int count;
	int len;

	if ((uim_hsl_read(port, UARTDM_SR_REG) & UARTDM_SR_OVERRUN_BMSK)) {
		uim_hsl_write(port, RESET_ERROR_STATUS, UARTDM_CR_REG);
		port->count.rx_overrun++;
	}

	if (misr & UARTDM_ISR_RXSTALE_BMSK) {
		count = uim_hsl_read(port, UARTDM_RX_TOTAL_SNAP_REG) -
			port->old_snap_state;
		port->old_snap_state = 0;
	} else {
		count = 4 * (uim_hsl_read(port, UARTDM_RFWR_REG));
		port->old_snap_state += count;
	}

	while (count > 0) {
		unsigned int c;

		sr = uim_hsl_read(port, UARTDM_SR_REG);
		if ((sr & UARTDM_SR_RXRDY_BMSK) == 0) {
			port->old_snap_state -= count;
			break;
		}
		c = uim_hsl_read(port, UARTDM_RF_REG);
		if (sr & UARTDM_SR_RX_BREAK_BMSK)
			port->count.brk++;
		else if (sr & UARTDM_SR_PAR_FRAME_BMSK)
			port->count.frame++;

		len = (count > 4) ? 4 : count;
		count -= len;

		while (len-- > 0) {
			if (uim_buf_push(&port->rxbuf, c & 0xff))
				port->count.rx_discard++;
			else
				port->count.rx++;
			c >>= 8;
		}
	}

	wake_up_interruptible(&port->rx_wq);
}

static void uim_irq_tx(struct uim_hsl_port *port)
{
	int sent_tx;
	int tx_count;
	int x;
	unsigned int tf_pointer = 0;

	tx_count = uim_buf_count_data(&port->txbuf);
	if (tx_count >= port->fifosize)
		tx_count = port->fifosize;

	if (tx_count) {
		uim_hsl_wait_for_xmitr(port);
		uim_hsl_write(port, tx_count, UARTDM_NCF_TX_REG);
		uim_hsl_read(port, UARTDM_NCF_TX_REG);
	} else {
		uim_hsl_stop_tx(port);
		return;
	}

	while (tf_pointer < tx_count)  {
		if (unlikely(!(uim_hsl_read(port, UARTDM_SR_REG) &
			       UARTDM_SR_TXRDY_BMSK)))
			continue;
		switch (tx_count - tf_pointer) {
		case 1:
			x = uim_buf_pop(&port->txbuf);
			port->count.tx++;
			break;
		case 2:
			x = uim_buf_pop(&port->txbuf);
			x |= (uim_buf_pop(&port->txbuf) << 8);
			port->count.tx += 2;
			break;
		case 3:
			x = uim_buf_pop(&port->txbuf);
			x |= (uim_buf_pop(&port->txbuf) << 8);
			x |= (uim_buf_pop(&port->txbuf) << 16);
			port->count.tx += 3;
			break;
		default:
			x = uim_buf_pop(&port->txbuf);
			x |= (uim_buf_pop(&port->txbuf) << 8);
			x |= (uim_buf_pop(&port->txbuf) << 16);
			x |= (uim_buf_pop(&port->txbuf) << 24);
			port->count.tx += 4;
			break;
		}
		uim_hsl_write(port, x, UARTDM_TF_REG);
		tf_pointer += 4;
		sent_tx = 1;
	}

	wake_up_interruptible(&port->tx_wq);

	if (uim_buf_is_empty(&port->txbuf))
		uim_hsl_stop_tx(port);
}

irqreturn_t uim_irq(int irq, void *dev_id)
{
	struct uim_hsl_port *port = dev_id;
	unsigned int misr;
	unsigned long flags;

	spin_lock_irqsave(&port->lock, flags);
	port->count.irq++;

	misr = uim_hsl_read(port, UARTDM_MISR_REG);
	uim_hsl_write(port, 0, UARTDM_IMR_REG);

	if (misr & (UARTDM_ISR_RXSTALE_BMSK | UARTDM_ISR_RXLEV_BMSK)) {
		uim_irq_rx(port, misr);
		if (misr & (UARTDM_ISR_RXSTALE_BMSK))
			uim_hsl_write(port, RESET_STALE_INT, UARTDM_CR_REG);
		uim_hsl_write(port, 6500, UARTDM_DMRX_REG);
		uim_hsl_write(port, STALE_EVENT_ENABLE, UARTDM_CR_REG);
	}
	if (misr & UARTDM_ISR_TXLEV_BMSK)
		uim_irq_tx(port);
	if (misr & UARTDM_ISR_DELTA_CTS_BMSK)
		uim_hsl_write(port, RESET_CTS, UARTDM_CR_REG);

	uim_hsl_write(port, port->imr, UARTDM_IMR_REG);
	spin_unlock_irqrestore(&port->lock, flags);

	return IRQ_HANDLED;
}

static void uim_hsl_reset(struct uim_hsl_port *port)
{
	/* reset everything */
	uim_hsl_write(port, RESET_RX, UARTDM_CR_REG);
	uim_hsl_write(port, RESET_TX, UARTDM_CR_REG);
	uim_hsl_write(port, RESET_ERROR_STATUS, UARTDM_CR_REG);
	uim_hsl_write(port, RESET_BREAK_INT, UARTDM_CR_REG);
	uim_hsl_write(port, RESET_CTS, UARTDM_CR_REG);
	uim_hsl_write(port, RFR_LOW, UARTDM_CR_REG);
}

static int uim_hsl_activate(struct uim_hsl_port *port)
{
	int retry = 3;
	unsigned int status;

	while (retry-- > 0) {
		uim_hsl_write(port, UART_UIM_CMD_RECOVER, UARTDM_UIM_CMD_REG);

		do {
			status = uim_hsl_read(port, UARTDM_UIM_IO_STATUS_REG);
		} while (status & UART_UIM_IO_STATUS_WIP);

		if (!(status & UART_UIM_IO_STATUS_DEACTIVATED))
			break;

		usleep_range(100, 120);
	}

	return (retry < 0) ? -EIO : 0;
}

static int uim_hsl_deactivate(struct uim_hsl_port *port)
{
	int retry = 3;
	unsigned int status;

	while (retry-- > 0) {
		uim_hsl_write(port, UART_UIM_CMD_DEACTIVATE,
					UARTDM_UIM_CMD_REG);

		do {
			status = uim_hsl_read(port, UARTDM_UIM_IO_STATUS_REG);
		} while (status & UART_UIM_IO_STATUS_WIP);

		if (status & UART_UIM_IO_STATUS_DEACTIVATED)
			break;

		usleep_range(100, 120);
	}

	return (retry < 0) ? -EIO : 0;
}

static int uim_hsl_startup(struct uim_hsl_port *port)
{
	int ret;
	unsigned int data, rfr_level;
	unsigned int watermark, rxstale, mr1, mr2;
	unsigned int status;
	int retry_activate = 3;

	/* GPIO */
	ret = uim_hsl_config_uim_gpios(port);
	if (ret)
		return ret;

	/* Clock */
	ret = uim_hsl_clk_enable(port, 1);
	if (ret) {
		pr_err("Error enabling clocks.\n");
		goto err_gpio;
	}

	if (clk_set_rate(port->clk, port->uartclk)) {
		pr_err("Error setting uartclk rate to %u\n", port->uartclk);
		goto err_clk;
	}

	if (clk_set_rate(port->sclk, port->simclk)) {
		pr_err("Error setting simclk rate to %u\n", port->simclk);
		goto err_clk;
	}

	/* Reset UIM */
	uim_hsl_write(port, UART_UIM_CFG_SW_RESET, UARTDM_UIM_CFG_REG);

	usleep_range(10, 12);

	/* Configure UIM */
	uim_hsl_write(port,
		UART_UIM_CFG_CARD_EVENTS_ENABLE |
		UART_UIM_CFG_PRESENT_POLARITY |
		(port->uim_1v8 ? UART_UIM_CFG_MODE18 : 0),
		UARTDM_UIM_CFG_REG);

	do {
		status = uim_hsl_read(port, UARTDM_UIM_IO_STATUS_REG);
	} while (status & UART_UIM_IO_STATUS_WIP);

	usleep_range(100, 120);

	/* Start SIM clock */
	port->simcfg =
		UART_SIM_CFG_STOP_BIT_LEN_N(2) |
		UART_SIM_CFG_SIM_CLK_ON |
		UART_SIM_CFG_SIM_CLK_STOP_HIGH |
		UART_SIM_CFG_MASK_RX |
		(port->swap_data ? UART_SIM_CFG_SWAP_D : 0) |
		(port->invert_data ? UART_SIM_CFG_INV_D : 0) |
		UART_SIM_CFG_SIM_SEL;

	uim_hsl_write(port, port->simcfg, UARTDM_SIM_CFG_REG);

	/* Check whether a card is plugged in */
	status = uim_hsl_read(port, UARTDM_UIM_IO_STATUS_REG);

	if ((status & UART_UIM_IO_STATUS_PRESENT) == 0) {
		pr_err("UIM card not detected.\n");
		ret = -ENODEV;
		goto err_clk;
	}

	usleep_range(100, 120);

	/* Activate */
	while (retry_activate-- > 0) {
		uim_hsl_write(port, UART_UIM_CMD_RECOVER, UARTDM_UIM_CMD_REG);

		do {
			status = uim_hsl_read(port, UARTDM_UIM_IO_STATUS_REG);
		} while (status & UART_UIM_IO_STATUS_WIP);

		if (!(status & UART_UIM_IO_STATUS_DEACTIVATED))
			break;
	}

	if (gpio_is_valid(port->uim_reset_gpio))
		gpio_free(port->uim_reset_gpio);

	/* Interrupt handler */
	ret = request_irq(port->irq, uim_irq, IRQF_TRIGGER_HIGH,
		port->name, port);
	if (unlikely(ret)) {
		pr_err("failed to request_irq\n");
		uim_hsl_unconfig_uim_gpios(port);
		goto err_clk;
	}

	/*
	 * set_baud_rate
	 */

	/* set automatic RFR level */
	if (likely(port->fifosize > 48))
		rfr_level = port->fifosize - 16;
	else
		rfr_level = port->fifosize;

	mr1 = uim_hsl_read(port, UARTDM_MR1_REG);
	mr1 &= ~UARTDM_MR1_AUTO_RFR_LEVEL1_BMSK;
	mr1 &= ~UARTDM_MR1_AUTO_RFR_LEVEL0_BMSK;
	mr1 |= UARTDM_MR1_AUTO_RFR_LEVEL1_BMSK & (rfr_level << 2);
	mr1 |= UARTDM_MR1_AUTO_RFR_LEVEL0_BMSK & rfr_level;
	mr1 &= ~(UARTDM_MR1_CTS_CTL_BMSK | UARTDM_MR1_RX_RDY_CTL_BMSK);
	uim_hsl_write(port, mr1, UARTDM_MR1_REG);

	/* Baud rate: 115200 bps */
	uim_hsl_write(port, UARTDM_CSR_28800, UARTDM_CSR_REG);

	mb();

	rxstale = 31;
	port->tx_timeout = (1000000000 / 115200) * 6;

	/* RX stale watermark */
	watermark = UARTDM_IPR_STALE_LSB_BMSK & rxstale;
	watermark |= UARTDM_IPR_STALE_TIMEOUT_MSB_BMSK & (rxstale << 2);
	uim_hsl_write(port, watermark, UARTDM_IPR_REG);

	/* Set RX watermark */
	watermark = (port->fifosize * 3) / 4;
	uim_hsl_write(port, watermark, UARTDM_RFWR_REG);

	/* set TX watermark */
	uim_hsl_write(port, 0, UARTDM_TFWR_REG);

	/* CR */
	uim_hsl_write(port, CR_PROTECTION_EN, UARTDM_CR_REG);
	uim_hsl_reset(port);

	data = UARTDM_CR_TX_EN_BMSK;
	data |= UARTDM_CR_RX_EN_BMSK;
	uim_hsl_write(port, data, UARTDM_CR_REG);
	uim_hsl_write(port, RESET_STALE_INT, UARTDM_CR_REG);

	/* turn on RX and CTS interrupts */
	port->imr = UARTDM_ISR_RXSTALE_BMSK
		| UARTDM_ISR_DELTA_CTS_BMSK | UARTDM_ISR_RXLEV_BMSK;
	uim_hsl_write(port, port->imr, UARTDM_IMR_REG);
	uim_hsl_write(port, 6500, UARTDM_DMRX_REG);
	uim_hsl_write(port, STALE_EVENT_ENABLE, UARTDM_CR_REG);

	/*
	 * set_termios
	 */

	/* Data: 8 bits, Stop: 2 bits, Parity: Even */
	mr2 = UARTDM_MR2_BITS_PER_CHAR_8 |
		UARTDM_MR2_STOP_BIT_TWO |
		UARTDM_MR2_PARITY_EVEN,
	uim_hsl_write(port, mr2, UARTDM_MR2_REG);

	/* No flow control */
	mr1 &= ~(UARTDM_MR1_CTS_CTL_BMSK | UARTDM_MR1_RX_RDY_CTL_BMSK);
	uim_hsl_write(port, mr1, UARTDM_MR1_REG);

	return 0;

err_clk:
	uim_hsl_clk_enable(port, 0);
err_gpio:
	uim_hsl_unconfig_uim_gpios(port);

	return ret;
}

static void uim_hsl_shutdown(struct uim_hsl_port *port)
{
	port->imr = 0;
	uim_hsl_write(port, 0, UARTDM_IMR_REG);

	free_irq(port->irq, port);

	/* Stop SIM clock */
	port->simcfg &= ~UART_SIM_CFG_SIM_SEL;
	uim_hsl_write(port, port->simcfg, UARTDM_SIM_CFG_REG);

	uim_hsl_clk_enable(port, 0);

	uim_hsl_unconfig_uim_gpios(port);
}

static int uim_dev_open(struct inode *inode, struct file *file)
{
	struct cdev *cdev = inode->i_cdev;
	struct uim_hsl_port *port =
		container_of(cdev, struct uim_hsl_port, cdev);
	int ret;

	file->private_data = port;

	if (atomic_inc_return(&port->ref_count) > 1) {
		ret = -EBUSY;
		goto err_count;
	}

	ret = uim_hsl_startup(port);
	if (unlikely(ret))
		goto err_count;

	return 0;

err_count:
	atomic_dec(&port->ref_count);

	return ret;
}

static int uim_dev_release(struct inode *inode, struct file *file)
{
	struct uim_hsl_port *port = uim_file_to_port(file);

	uim_hsl_shutdown(port);

	atomic_dec(&port->ref_count);

	return 0;
}

static ssize_t uim_dev_read(struct file *file, char __user *buf,
				size_t count, loff_t *pos)
{
	struct uim_hsl_port *port = uim_file_to_port(file);
	unsigned long flags;
	unsigned int avail;
	unsigned char *tmpbuf;
	unsigned int i;

	for (;;) {
		spin_lock_irqsave(&port->lock, flags);
		avail = uim_buf_count_data(&port->rxbuf);
		spin_unlock_irqrestore(&port->lock, flags);

		if (avail)
			break;

		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN;
		if (wait_event_interruptible(port->rx_wq,
			!uim_buf_is_empty(&port->rxbuf)) < 0)
			return 0;
	}

	if (count > avail)
		count = avail;

	tmpbuf = kmalloc(count, GFP_KERNEL);
	if (tmpbuf == NULL)
		return -ENOMEM;

	spin_lock_irqsave(&port->lock, flags);
	for (i = 0; i < count; ++i)
		tmpbuf[i] = uim_buf_pop(&port->rxbuf);
	spin_unlock_irqrestore(&port->lock, flags);

	if (copy_to_user(buf, tmpbuf, count))
		count = -EFAULT;

	kfree(tmpbuf);

	return count;
}

static ssize_t uim_dev_write(struct file *file, const char __user *buf,
				size_t count, loff_t *pos)
{
	struct uim_hsl_port *port = uim_file_to_port(file);
	unsigned long flags;
	unsigned int avail;
	unsigned char *tmpbuf;
	unsigned int i;

	do {
		spin_lock_irqsave(&port->lock, flags);
		avail = uim_buf_count_free(&port->txbuf);
		spin_unlock_irqrestore(&port->lock, flags);

		if (avail == 0) {
			if (file->f_flags & O_NONBLOCK)
				return -EAGAIN;
			if (wait_event_interruptible(port->tx_wq,
				!uim_buf_is_full(&port->txbuf)) < 0)
				return 0;
		}
	} while (avail == 0);

	if (count > avail)
		count = avail;

	tmpbuf = kmalloc(count, GFP_KERNEL);
	if (tmpbuf == NULL)
		return -ENOMEM;

	if (copy_from_user(tmpbuf, buf, count)) {
		kfree(tmpbuf);
		return -EFAULT;
	}

	spin_lock_irqsave(&port->lock, flags);
	for (i = 0; i < count; ++i)
		uim_buf_push(&port->txbuf, tmpbuf[i]);
	spin_unlock_irqrestore(&port->lock, flags);

	kfree(tmpbuf);

	uim_hsl_start_tx(port);

	return 0;
}

static unsigned int uim_dev_poll(struct file *file, poll_table *wait)
{
	struct uim_hsl_port *port = uim_file_to_port(file);
	unsigned mask = 0;
	unsigned long flags;
	int rx_empty;

	spin_lock_irqsave(&port->lock, flags);
	rx_empty = uim_buf_is_empty(&port->rxbuf);
	spin_unlock_irqrestore(&port->lock, flags);

	if (!rx_empty)
		mask |= POLLIN;

	if (mask == 0) {
		poll_wait(file, &port->rx_wq, wait);

		spin_lock_irqsave(&port->lock, flags);
		rx_empty = uim_buf_is_empty(&port->rxbuf);
		spin_unlock_irqrestore(&port->lock, flags);

		if (!rx_empty)
			mask |= POLLIN;
	}

	return mask;
}

static long uim_dev_ioctl(struct file *fp, unsigned int cmd,
		unsigned long arg)
{
	return 0;
}

static const struct file_operations uim_dev_fops = {
	.owner		= THIS_MODULE,
	.open		= uim_dev_open,
	.release	= uim_dev_release,
	.read		= uim_dev_read,
	.write		= uim_dev_write,
	.poll		= uim_dev_poll,
	.unlocked_ioctl	= uim_dev_ioctl,
	.llseek		= noop_llseek,
};

/*
 * Sysfs
 */

static ssize_t uim_sysfs_swap_get(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct uim_hsl_port *port = dev_get_drvdata(dev);

	return snprintf(buf, 3, "%d\n", port->swap_data);
}

static ssize_t uim_sysfs_swap_set(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct uim_hsl_port *port = dev_get_drvdata(dev);
	unsigned int swap;

	swap = (buf[0] == '0') ? 0 : 1;

	if (swap == port->swap_data)
		return count;

	port->swap_data = swap;
	if (swap)
		port->simcfg |= UART_SIM_CFG_SWAP_D;
	else
		port->simcfg &= UART_SIM_CFG_SWAP_D;

	uim_hsl_write(port, port->simcfg, UARTDM_SIM_CFG_REG);

	return count;
}

static DEVICE_ATTR(swap, S_IWUSR | S_IRUGO,
	uim_sysfs_swap_get, uim_sysfs_swap_set);

static ssize_t uim_sysfs_invert_get(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct uim_hsl_port *port = dev_get_drvdata(dev);

	return snprintf(buf, 3, "%d\n", port->invert_data);
}

static ssize_t uim_sysfs_invert_set(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct uim_hsl_port *port = dev_get_drvdata(dev);
	unsigned int invert;

	invert = (buf[0] == '0') ? 0 : 1;

	if (invert == port->invert_data)
		return count;

	port->invert_data = invert;
	if (invert)
		port->simcfg |= UART_SIM_CFG_INV_D;
	else
		port->simcfg &= UART_SIM_CFG_INV_D;

	uim_hsl_write(port, port->simcfg, UARTDM_SIM_CFG_REG);

	return count;
}

static DEVICE_ATTR(invert, S_IWUSR | S_IRUGO,
	uim_sysfs_invert_get, uim_sysfs_invert_set);

static ssize_t uim_sysfs_reset_get(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct uim_hsl_port *port = dev_get_drvdata(dev);

	return snprintf(buf, 3, "%d\n", port->reset);
}

static ssize_t uim_sysfs_reset_set(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct uim_hsl_port *port = dev_get_drvdata(dev);
	unsigned int reset;

	reset = (buf[0] == '0') ? 0 : 1;

	if (reset == port->reset)
		return count;

	if (reset)
		uim_hsl_deactivate(port);
	else
		uim_hsl_activate(port);

	port->reset = reset;

	return count;
}

static DEVICE_ATTR(reset, S_IWUSR | S_IRUGO,
	uim_sysfs_reset_get, uim_sysfs_reset_set);

/*
 * Debugfs
 */

static int uim_debug_status_show(struct seq_file *s, void *unused)
{
	struct uim_hsl_port *port = s->private;
	unsigned int val;

	val = uim_hsl_read(port, UARTDM_UIM_IO_STATUS_REG);

	seq_printf(s, "Card %s, %s\n",
		(val & UART_UIM_IO_STATUS_PRESENT) ? "Present" : "Absent",
		(val & UART_UIM_IO_STATUS_DEACTIVATED) ?
			"Deactivated" : "Activated");

	return 0;
}

static int uim_debug_status_open(struct inode *i, struct file *f)
{
	return single_open(f, uim_debug_status_show, i->i_private);
}

static const struct file_operations uim_debugfs_status_fops = {
	.open = uim_debug_status_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int uim_debug_stat_show(struct seq_file *s, void *unused)
{
	struct uim_hsl_port *port = s->private;
	char str[64];

	snprintf(str, sizeof(str),
		"TX: %u\n"
		"RX: %u\n"
		"RX overrun: %u\n"
		"RX discarded: %u\n"
		"Interrupts: %u\n",
		port->count.tx, port->count.rx,
		port->count.rx_overrun, port->count.rx_discard,
		port->count.irq);

	seq_puts(s, str);

	return 0;
}

static int uim_debug_stat_open(struct inode *i, struct file *f)
{
	return single_open(f, uim_debug_stat_show, i->i_private);
}

static const struct file_operations uim_debugfs_stat_fops = {
	.open = uim_debug_stat_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

/*
 * Driver interface
 */

static atomic_t msm_uim_next_id = ATOMIC_INIT(0);

static int msm_uim_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct uim_hsl_port *port;
	struct resource *uim_resource;
	int ret;
	dev_t dev;
	char name[16];

	if (node == NULL) {
		pr_err("Device tree is not available.\n");
		return -EINVAL;
	}

	if (atomic_read(&msm_uim_next_id) == UIM_MAX_DEVICES - 1) {
		pr_err("Cannot add more UIM device.\n");
		return -ENOMEM;
	}

	/* Allocate private data */
	port = devm_kzalloc(&pdev->dev, sizeof(*port), GFP_KERNEL);
	if (!port) {
		pr_err("Unable to allocate memory for a UIM device.\n");
		return -ENOMEM;
	}
	platform_set_drvdata(pdev, port);

	/* Read device tree */
	port->uim_1v8 = of_property_read_bool(node, "qcom,uim-1v8");

	port->uim_clk_gpio = of_get_named_gpio(node, "qcom,uim-clk-gpio", 0);

	port->uim_data_gpio = of_get_named_gpio(node, "qcom,uim-data-gpio", 0);

	port->uim_card_detect_gpio =
		of_get_named_gpio(node, "qcom,uim-card-detect-gpio", 0);

	port->uim_reset_gpio =
		of_get_named_gpio(node, "qcom,uim-reset-gpio", 0);

	uim_resource = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (unlikely(!uim_resource)) {
		dev_err(&pdev->dev, "Cannot find resource.\n");
		ret = -ENXIO;
		goto err;
	}
	port->mapbase = uim_resource->start;
	port->mapsize = uim_resource->end - uim_resource->start + 1;

	port->irq = platform_get_irq(pdev, 0);
	if (unlikely((int) port->irq < 0)) {
		dev_err(&pdev->dev, "Cannot find IRQ.\n");
		ret = -ENXIO;
		goto err;
	}

	/* Clock */
	port->clk = clk_get(&pdev->dev, "core_clk");
	if (unlikely(IS_ERR(port->clk))) {
		ret = PTR_ERR(port->clk);
		dev_err(&pdev->dev, "Error getting core clk\n");
		goto err;
	}

	port->pclk = clk_get(&pdev->dev, "iface_clk");
	if (unlikely(IS_ERR(port->pclk))) {
		ret = PTR_ERR(port->pclk);
		dev_err(&pdev->dev, "Error getting interface clk\n");
		goto err_clk;
	}

	port->sclk = clk_get(&pdev->dev, "sim_clk");
	if (unlikely(IS_ERR(port->sclk))) {
		ret = PTR_ERR(port->sclk);
		dev_err(&pdev->dev, "Error getting SIM clk\n");
		goto err_pclk;
	}

	if (unlikely(!request_mem_region(port->mapbase, port->mapsize,
			"uim"))) {
		dev_err(&pdev->dev, "Can't request mem region for uim.\n");
		ret = -EBUSY;
		goto err_sclk;
	}

	port->membase = ioremap(port->mapbase, port->mapsize);
	if (port->membase == NULL) {
		ret = -EIO;
		goto err_mem_region;
	}

	/* Allocate an ID */
	if (pdev->id == -1)
		pdev->id = atomic_inc_return(&msm_uim_next_id) - 1;

	port->simclk = UIM_SIM_CLK;
	port->uartclk = UIM_UART_CLK;
	port->fifosize = 64;

	port->imr = 0;
	port->simcfg = 0;
	port->old_snap_state = 0;

	port->swap_data = 0;
	port->invert_data = 0;
	port->reset = 0;

	spin_lock_init(&port->lock);
	mutex_init(&port->mutex);
	init_waitqueue_head(&port->rx_wq);
	init_waitqueue_head(&port->tx_wq);

	if (major == 0) {
		ret = alloc_chrdev_region(&dev, 0, UIM_MAX_DEVICES,
			UIM_DEVICE_NAME);
		if (IS_ERR_VALUE(ret)) {
			dev_err(&pdev->dev,
				"Unable to register a UIM device.\n");
			goto err_ioremap;
		}
		major = MAJOR(dev);
	}

	port->dev_num = MKDEV(major, pdev->id);
	cdev_init(&port->cdev, &uim_dev_fops);
	ret = cdev_add(&port->cdev, port->dev_num, 1);
	if (IS_ERR_VALUE(ret)) {
		dev_err(&pdev->dev, "Unable to add a UIM device.\n");
		goto err_ioremap;
	}

	snprintf(port->name, sizeof(port->name), "uim%d", pdev->id);
	port->device = device_create(uim_class, NULL, port->dev_num,
		port, port->name);
	if (IS_ERR(port->device)) {
		dev_err(&pdev->dev, "Unable to create a UIM device.\n");
		goto err_cdev_add;
	}

	/* Sysfs */
	ret = device_create_file(&pdev->dev, &dev_attr_swap);
	if (unlikely(ret))
		dev_err(&pdev->dev, "Can't create swap attribute\n");

	ret = device_create_file(&pdev->dev, &dev_attr_invert);
	if (unlikely(ret))
		dev_err(&pdev->dev, "Can't create invert attribute\n");

	ret = device_create_file(&pdev->dev, &dev_attr_reset);
	if (unlikely(ret))
		dev_err(&pdev->dev, "Can't create reset attribute\n");

	/* Debugfs */
	if (debug_base) {
		snprintf(name, sizeof(name), "uim%d", pdev->id);
		port->debugfs_dir = debugfs_create_dir(name, debug_base);
		if (IS_ERR_OR_NULL(port->debugfs_dir)) {
			dev_err(&pdev->dev,
				"Can't create a debugfs directory\n");
			port->debugfs_dir = NULL;
		} else {
			port->debugfs_stat = debugfs_create_file("stat",
				S_IRUGO, port->debugfs_dir, port,
				&uim_debugfs_stat_fops);
			port->debugfs_status = debugfs_create_file("status",
				S_IRUGO, port->debugfs_dir, port,
				&uim_debugfs_status_fops);
		}
	}

	return 0;

err_cdev_add:
	cdev_del(&port->cdev);
err_ioremap:
	iounmap(port->membase);
err_mem_region:
	release_mem_region(port->mapbase, port->mapsize);
err_sclk:
	clk_put(port->sclk);
err_pclk:
	clk_put(port->pclk);
err_clk:
	clk_put(port->clk);
err:
	platform_set_drvdata(pdev, NULL);
	kfree(port);

	return ret;
}

static int msm_uim_remove(struct platform_device *pdev)
{
	struct uim_hsl_port *port = platform_get_drvdata(pdev);

	device_del(port->device);
	cdev_del(&port->cdev);

	debugfs_remove_recursive(port->debugfs_dir);

	device_remove_file(&pdev->dev, &dev_attr_swap);
	device_remove_file(&pdev->dev, &dev_attr_invert);
	device_remove_file(&pdev->dev, &dev_attr_reset);

	iounmap(port->membase);

	release_mem_region(port->mapbase, port->mapsize);

	clk_put(port->sclk);
	clk_put(port->pclk);
	clk_put(port->clk);

	platform_set_drvdata(pdev, NULL);
	kfree(port);

	return 0;
}

static struct of_device_id msm_uim_match_table[] = {
	{
		.compatible = "qcom,uim",
	},
	{}
};

static struct platform_driver msm_uim_driver = {
	.probe = msm_uim_probe,
	.remove = msm_uim_remove,
	.driver = {
		.name = UIM_DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = msm_uim_match_table,
	},
};

/*
 * Kernel module interface
 */

static int __init msm_uim_init(void)
{
	int ret;

	uim_class = class_create(THIS_MODULE, UIM_DEVICE_NAME);
	if (IS_ERR(uim_class)) {
		pr_err("class_create() failed for uim_class\n");
		return PTR_ERR(uim_class);
	}

	debug_base = debugfs_create_dir("msm_uim_hsl", NULL);
	if (IS_ERR_OR_NULL(debug_base))
		debug_base = NULL;

	ret = platform_driver_register(&msm_uim_driver);
	if (ret) {
		debugfs_remove_recursive(debug_base);
		class_destroy(uim_class);
		return ret;
	}

	return 0;
}

static void __exit msm_uim_exit(void)
{
	if (major) {
		dev_t dev = MKDEV(major, 0);
		unregister_chrdev_region(dev, UIM_MAX_DEVICES);
	}

	debugfs_remove_recursive(debug_base);
	platform_driver_unregister(&msm_uim_driver);
	class_destroy(uim_class);
}

module_init(msm_uim_init);
module_exit(msm_uim_exit)

MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:msm_uim");
MODULE_DESCRIPTION("Driver for MSM HSUART UIM device");
