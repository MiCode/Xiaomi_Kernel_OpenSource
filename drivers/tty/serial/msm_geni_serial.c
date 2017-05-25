/*
 * Copyright (c) 2017, The Linux foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/bitmap.h>
#include <linux/bitops.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/console.h>
#include <linux/io.h>
#include <linux/ipc_logging.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/qcom-geni-se.h>
#include <linux/serial.h>
#include <linux/serial_core.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>

/* UART specific GENI registers */
#define SE_UART_LOOPBACK_CFG		(0x22C)
#define SE_UART_TX_TRANS_CFG		(0x25C)
#define SE_UART_TX_WORD_LEN		(0x268)
#define SE_UART_TX_STOP_BIT_LEN		(0x26C)
#define SE_UART_TX_TRANS_LEN		(0x270)
#define SE_UART_RX_TRANS_CFG		(0x280)
#define SE_UART_RX_WORD_LEN		(0x28C)
#define SE_UART_RX_STALE_CNT		(0x294)
#define SE_UART_TX_PARITY_CFG		(0x2A4)
#define SE_UART_RX_PARITY_CFG		(0x2A8)
#define SE_UART_MANUAL_RFR		(0x2AC)

/* SE_UART_LOOPBACK_CFG */
#define NO_LOOPBACK		(0)
#define TX_RX_LOOPBACK		(0x1)
#define CTS_RFR_LOOPBACK	(0x2)
#define CTSRFR_TXRX_LOOPBACK	(0x3)

/* SE_UART_TRANS_CFG */
#define UART_TX_PAR_EN		(BIT(0))
#define UART_CTS_MASK		(BIT(1))

/* SE_UART_TX_WORD_LEN */
#define TX_WORD_LEN_MSK		(GENMASK(9, 0))

/* SE_UART_TX_STOP_BIT_LEN */
#define TX_STOP_BIT_LEN_MSK	(GENMASK(23, 0))
#define TX_STOP_BIT_LEN_1	(0)
#define TX_STOP_BIT_LEN_1_5	(1)
#define TX_STOP_BIT_LEN_2	(2)

/* SE_UART_TX_TRANS_LEN */
#define TX_TRANS_LEN_MSK	(GENMASK(23, 0))

/* SE_UART_RX_TRANS_CFG */
#define UART_RX_INS_STATUS_BIT	(BIT(2))
#define UART_RX_PAR_EN		(BIT(3))

/* SE_UART_RX_WORD_LEN */
#define RX_WORD_LEN_MASK	(GENMASK(9, 0))

/* SE_UART_RX_STALE_CNT */
#define RX_STALE_CNT		(GENMASK(23, 0))

/* SE_UART_TX_PARITY_CFG/RX_PARITY_CFG */
#define PAR_CALC_EN		(BIT(0))
#define PAR_MODE_MSK		(GENMASK(2, 1))
#define PAR_MODE_SHFT		(1)
#define PAR_EVEN		(0x00)
#define PAR_ODD			(0x01)
#define PAR_SPACE		(0x10)
#define PAR_MARK		(0x11)

/* SE_UART_MANUAL_RFR register fields */
#define UART_MANUAL_RFR_EN	(BIT(31))
#define UART_RFR_NOT_READY	(BIT(1))
#define UART_RFR_READY		(BIT(0))

/* UART M_CMD OP codes */
#define UART_START_TX		(0x1)
#define UART_START_BREAK	(0x4)
#define UART_STOP_BREAK		(0x5)
/* UART S_CMD OP codes */
#define UART_START_READ		(0x1)
#define UART_PARAM		(0x1)

#define UART_OVERSAMPLING	(32)
#define STALE_TIMEOUT		(16)
#define DEFAULT_BITS_PER_CHAR	(10)
#define GENI_UART_NR_PORTS	(15)
#define GENI_UART_CONS_PORTS	(1)
#define DEF_FIFO_DEPTH_WORDS	(16)
#define DEF_TX_WM		(2)
#define DEF_FIFO_WIDTH_BITS	(32)
#define UART_CORE2X_VOTE	(10000)

#define WAKEBYTE_TIMEOUT_MSEC	(2000)
#define IPC_LOG_PWR_PAGES	(2)
#define IPC_LOG_MISC_PAGES	(2)
#define IPC_LOG_TX_RX_PAGES	(3)
#define DATA_BYTES_PER_LINE	(32)

#define IPC_LOG_MSG(ctx, x...) do { \
	if (ctx) \
		ipc_log_string(ctx, x); \
} while (0)

struct msm_geni_serial_port {
	struct uart_port uport;
	char name[20];
	unsigned int tx_fifo_depth;
	unsigned int tx_fifo_width;
	unsigned int rx_fifo_depth;
	unsigned int tx_wm;
	unsigned int rx_wm;
	unsigned int rx_rfr;
	int xfer_mode;
	struct dentry *dbg;
	bool port_setup;
	unsigned int *rx_fifo;
	int (*handle_rx)(struct uart_port *uport,
			unsigned int rx_fifo_wc,
			unsigned int rx_last_byte_valid,
			unsigned int rx_last);
	struct device *wrapper_dev;
	struct se_geni_rsc serial_rsc;
	int loopback;
	int wakeup_irq;
	unsigned char wakeup_byte;
	struct wakeup_source geni_wake;
	void *ipc_log_tx;
	void *ipc_log_rx;
	void *ipc_log_pwr;
	void *ipc_log_misc;
	unsigned int cur_baud;
	int ioctl_count;
};

static const struct uart_ops msm_geni_serial_pops;
static struct uart_driver msm_geni_console_driver;
static struct uart_driver msm_geni_serial_hs_driver;
static int handle_rx_console(struct uart_port *uport,
			unsigned int rx_fifo_wc,
			unsigned int rx_last_byte_valid,
			unsigned int rx_last);
static int handle_rx_hs(struct uart_port *uport,
			unsigned int rx_fifo_wc,
			unsigned int rx_last_byte_valid,
			unsigned int rx_last);
static unsigned int msm_geni_serial_tx_empty(struct uart_port *port);
static int msm_geni_serial_power_on(struct uart_port *uport);
static void msm_geni_serial_power_off(struct uart_port *uport);
static int msm_geni_serial_poll_bit(struct uart_port *uport,
				int offset, int bit_field, bool set);

static atomic_t uart_line_id = ATOMIC_INIT(0);

#define GET_DEV_PORT(uport) \
	container_of(uport, struct msm_geni_serial_port, uport)

static struct msm_geni_serial_port msm_geni_console_port;
static struct msm_geni_serial_port msm_geni_serial_ports[GENI_UART_NR_PORTS];

static void msm_geni_serial_config_port(struct uart_port *uport, int cfg_flags)
{
	if (cfg_flags & UART_CONFIG_TYPE)
		uport->type = PORT_MSM;
}

static ssize_t msm_geni_serial_loopback_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct msm_geni_serial_port *port = platform_get_drvdata(pdev);

	return snprintf(buf, sizeof(int), "%d\n", port->loopback);
}

static ssize_t msm_geni_serial_loopback_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct msm_geni_serial_port *port = platform_get_drvdata(pdev);

	if (kstrtoint(buf, 0, &port->loopback)) {
		dev_err(dev, "Invalid input\n");
		return -EINVAL;
	}
	return size;
}

static DEVICE_ATTR(loopback, 0644, msm_geni_serial_loopback_show,
					msm_geni_serial_loopback_store);

static void dump_ipc(void *ipc_ctx, char *prefix, char *string,
						u64 addr, int size)

{
	char buf[DATA_BYTES_PER_LINE * 2];
	int len = 0;

	if (!ipc_ctx)
		return;
	len = min(size, DATA_BYTES_PER_LINE);
	hex_dump_to_buffer(string, len, DATA_BYTES_PER_LINE, 1, buf,
						sizeof(buf), false);
	ipc_log_string(ipc_ctx, "%s[0x%.10x:%d] : %s", prefix,
					(unsigned int)addr, size, buf);
}

static bool check_tx_active(struct uart_port *uport)
{
	/*
	 * Poll if the GENI STATUS bit for TX is cleared. If the bit is
	 * clear (poll condition met), return false, meaning tx isn't active
	 * else return true. So return not of the poll return.
	 */
	return !msm_geni_serial_poll_bit(uport, SE_GENI_STATUS,
					M_GENI_CMD_ACTIVE, false);
}

static int vote_clock_on(struct uart_port *uport)
{
	int ret = 0;
	struct msm_geni_serial_port *port = GET_DEV_PORT(uport);
	int usage_count = atomic_read(&uport->dev->power.usage_count);

	if (!pm_runtime_enabled(uport->dev)) {
		dev_err(uport->dev, "RPM not available.Can't enable clocks\n");
		ret = -EPERM;
		return ret;
	}
	ret = msm_geni_serial_power_on(uport);
	if (ret) {
		dev_err(uport->dev, "Failed to vote clock on\n");
		return ret;
	}
	port->ioctl_count++;
	__pm_relax(&port->geni_wake);
	IPC_LOG_MSG(port->ipc_log_pwr, "%s rpm %d ioctl %d\n",
				__func__, usage_count, port->ioctl_count);
	return 0;
}

static int vote_clock_off(struct uart_port *uport)
{
	struct msm_geni_serial_port *port = GET_DEV_PORT(uport);
	int ret = 0;
	int usage_count = atomic_read(&uport->dev->power.usage_count);

	if (!pm_runtime_enabled(uport->dev)) {
		dev_err(uport->dev, "RPM not available.Can't enable clocks\n");
		ret = -EPERM;
		return ret;
	}
	/* Check on going Tx. Don't block on this for now. */
	if (check_tx_active(uport))
		dev_warn(uport->dev, "%s: Vote off called during active Tx",
								__func__);
	if (!port->ioctl_count) {
		dev_warn(uport->dev, "%s:Imbalanced vote off ioctl %d\n",
						 __func__, usage_count);
		IPC_LOG_MSG(port->ipc_log_pwr,
				"%s:Imbalanced vote_off from userspace rpm%d",
				__func__, usage_count);
		return 0;
	}
	port->ioctl_count--;
	msm_geni_serial_power_off(uport);
	IPC_LOG_MSG(port->ipc_log_pwr, "%s rpm %d ioctl %d\n",
				__func__, usage_count, port->ioctl_count);
	return 0;
};

static int msm_geni_serial_ioctl(struct uart_port *uport, unsigned int cmd,
						unsigned long arg)
{
	int ret = -ENOIOCTLCMD;

	switch (cmd) {
	case TIOCPMGET: {
		ret = vote_clock_on(uport);
		break;
	}
	case TIOCPMPUT: {
		ret = vote_clock_off(uport);
		break;
	}
	case TIOCPMACT: {
		ret = !pm_runtime_status_suspended(uport->dev);
		break;
	}
	default:
		break;
	}
	return ret;
}

static void msm_geni_serial_break_ctl(struct uart_port *uport, int ctl)
{
	if (!uart_console(uport) && pm_runtime_status_suspended(uport->dev)) {
		dev_err(uport->dev, "%s Device suspended,vote clocks on.\n",
							__func__);
		return;
	}

	if (ctl) {
		check_tx_active(uport);
		geni_setup_m_cmd(uport->membase, UART_START_BREAK, 0);
	} else {
		geni_setup_m_cmd(uport->membase, UART_STOP_BREAK, 0);
	}
	/* Ensure break start/stop command is setup before returning.*/
	mb();
}

static unsigned int msm_geni_cons_get_mctrl(struct uart_port *uport)
{
	return TIOCM_DSR | TIOCM_CAR | TIOCM_CTS;
}

static unsigned int msm_geni_serial_get_mctrl(struct uart_port *uport)
{
	u32 geni_ios = 0;
	unsigned int mctrl = TIOCM_DSR | TIOCM_CAR;

	if (pm_runtime_status_suspended(uport->dev))
		return TIOCM_DSR | TIOCM_CAR | TIOCM_CTS;

	geni_ios = geni_read_reg_nolog(uport->membase, SE_GENI_IOS);
	if (!(geni_ios & IO2_DATA_IN))
		mctrl |= TIOCM_CTS;

	return mctrl;
}

static void msm_geni_cons_set_mctrl(struct uart_port *uport,
							unsigned int mctrl)
{
}

static void msm_geni_serial_set_mctrl(struct uart_port *uport,
							unsigned int mctrl)
{
	u32 uart_manual_rfr = 0;

	if (pm_runtime_status_suspended(uport->dev)) {
		dev_info(uport->dev, "%sDevice suspended,vote clocks on\n",
						__func__);
		return;
	}
	if (!(mctrl & TIOCM_RTS))
		uart_manual_rfr |= (UART_MANUAL_RFR_EN | UART_RFR_NOT_READY);
	geni_write_reg_nolog(uart_manual_rfr, uport->membase,
							SE_UART_MANUAL_RFR);
	/* Write to flow control must complete before return to client*/
	mb();
}

static const char *msm_geni_serial_get_type(struct uart_port *uport)
{
	return "MSM";
}

static struct msm_geni_serial_port *get_port_from_line(int line,
						bool is_console)
{
	struct msm_geni_serial_port *port = NULL;

	if (is_console) {
		if ((line < 0) || (line >= GENI_UART_CONS_PORTS))
			port = ERR_PTR(-ENXIO);
		port = &msm_geni_console_port;
	} else {
		if ((line < 0) || (line >= GENI_UART_NR_PORTS))
			return ERR_PTR(-ENXIO);
		port = &msm_geni_serial_ports[line];
	}

	return port;
}

static int msm_geni_serial_power_on(struct uart_port *uport)
{
	int ret = 0;

	ret = pm_runtime_get_sync(uport->dev);
	if (ret < 0) {
		pm_runtime_put_noidle(uport->dev);
		pm_runtime_set_suspended(uport->dev);
		return ret;
	}
	return 0;
}

static void msm_geni_serial_power_off(struct uart_port *uport)
{
	pm_runtime_put_sync(uport->dev);
}

static int msm_geni_serial_poll_bit(struct uart_port *uport,
				int offset, int bit_field, bool set)
{
	int iter = 0;
	unsigned int reg;
	bool met = false;
	struct msm_geni_serial_port *port = NULL;
	bool cond = false;
	unsigned int baud = 115200;
	unsigned int fifo_bits = DEF_FIFO_DEPTH_WORDS * DEF_FIFO_WIDTH_BITS;
	unsigned long total_iter = 1000;


	if (uport->private_data && !uart_console(uport)) {
		port = GET_DEV_PORT(uport);
		baud = (port->cur_baud ? port->cur_baud : 115200);
		fifo_bits = port->tx_fifo_depth * port->tx_fifo_width;
		/*
		 * Total polling iterations based on FIFO worth of bytes to be
		 * sent at current baud .Add a little fluff to the wait.
		 */
		total_iter = ((fifo_bits * USEC_PER_SEC) / baud);
		total_iter += 50;
	}

	while (iter < total_iter) {
		reg = geni_read_reg_nolog(uport->membase, offset);
		cond = reg & bit_field;
		if (cond == set) {
			met = true;
			break;
		}
		udelay(10);
		iter++;
	}
	return met;
}

static void msm_geni_serial_setup_tx(struct uart_port *uport,
				unsigned int xmit_size)
{
	u32 m_cmd = 0;

	geni_write_reg_nolog(xmit_size, uport->membase, SE_UART_TX_TRANS_LEN);
	m_cmd |= (UART_START_TX << M_OPCODE_SHFT);
	geni_write_reg_nolog(m_cmd, uport->membase, SE_GENI_M_CMD0);
	/*
	 * Writes to enable the primary sequencer should go through before
	 * exiting this function.
	 */
	mb();
}

static void msm_geni_serial_poll_cancel_tx(struct uart_port *uport)
{
	int done = 0;
	unsigned int irq_clear = M_CMD_DONE_EN;

	done = msm_geni_serial_poll_bit(uport, SE_GENI_M_IRQ_STATUS,
						M_CMD_DONE_EN, true);
	if (!done) {
		geni_write_reg_nolog(M_GENI_CMD_ABORT, uport->membase,
					SE_GENI_M_CMD_CTRL_REG);
		irq_clear |= M_CMD_ABORT_EN;
		msm_geni_serial_poll_bit(uport, SE_GENI_M_IRQ_STATUS,
							M_CMD_ABORT_EN, true);
	}
	geni_write_reg_nolog(irq_clear, uport->membase, SE_GENI_M_IRQ_CLEAR);
}

static void msm_geni_serial_abort_rx(struct uart_port *uport)
{
	unsigned int irq_clear = S_CMD_DONE_EN;

	geni_abort_s_cmd(uport->membase);
	/* Ensure this goes through before polling. */
	mb();
	irq_clear |= S_CMD_ABORT_EN;
	msm_geni_serial_poll_bit(uport, SE_GENI_S_CMD_CTRL_REG,
					S_GENI_CMD_ABORT, false);
	geni_write_reg_nolog(irq_clear, uport->membase, SE_GENI_S_IRQ_CLEAR);
}

#ifdef CONFIG_CONSOLE_POLL
static int msm_geni_serial_get_char(struct uart_port *uport)
{
	unsigned int rx_fifo;
	unsigned int m_irq_status;
	unsigned int s_irq_status;

	if (!(msm_geni_serial_poll_bit(uport, SE_GENI_M_IRQ_STATUS,
			M_SEC_IRQ_EN, true)))
		return -ENXIO;

	m_irq_status = geni_read_reg_nolog(uport->membase,
						SE_GENI_M_IRQ_STATUS);
	s_irq_status = geni_read_reg_nolog(uport->membase,
						SE_GENI_S_IRQ_STATUS);
	geni_write_reg_nolog(m_irq_status, uport->membase,
						SE_GENI_M_IRQ_CLEAR);
	geni_write_reg_nolog(s_irq_status, uport->membase,
						SE_GENI_S_IRQ_CLEAR);

	if (!(msm_geni_serial_poll_bit(uport, SE_GENI_RX_FIFO_STATUS,
			RX_FIFO_WC_MSK, true)))
		return -ENXIO;

	/*
	 * Read the Rx FIFO only after clearing the interrupt registers and
	 * getting valid RX fifo status.
	 */
	mb();
	rx_fifo = geni_read_reg_nolog(uport->membase, SE_GENI_RX_FIFOn);
	rx_fifo &= 0xFF;
	return rx_fifo;
}

static void msm_geni_serial_poll_put_char(struct uart_port *uport,
					unsigned char c)
{
	int b = (int) c;
	struct msm_geni_serial_port *port = GET_DEV_PORT(uport);

	geni_write_reg_nolog(port->tx_wm, uport->membase,
					SE_GENI_TX_WATERMARK_REG);
	msm_geni_serial_setup_tx(uport, 1);
	if (!msm_geni_serial_poll_bit(uport, SE_GENI_M_IRQ_STATUS,
				M_TX_FIFO_WATERMARK_EN, true))
		WARN_ON(1);
	geni_write_reg_nolog(b, uport->membase, SE_GENI_TX_FIFOn);
	geni_write_reg_nolog(M_TX_FIFO_WATERMARK_EN, uport->membase,
							SE_GENI_M_IRQ_CLEAR);
	/*
	 * Ensure FIFO write goes through before polling for status but.
	 */
	mb();
	msm_geni_serial_poll_cancel_tx(uport);
}
#endif

#if defined(CONFIG_SERIAL_CORE_CONSOLE) || defined(CONFIG_CONSOLE_POLL)
static void msm_geni_serial_wr_char(struct uart_port *uport, int ch)
{
	geni_write_reg_nolog(ch, uport->membase, SE_GENI_TX_FIFOn);
	/*
	 * Ensure FIFO write clear goes through before
	 * next iteration.
	 */
	mb();

}

static void
__msm_geni_serial_console_write(struct uart_port *uport, const char *s,
				unsigned int count)
{
	int new_line = 0;
	int i;
	int bytes_to_send = count;
	int fifo_depth = DEF_FIFO_DEPTH_WORDS;
	int tx_wm = DEF_TX_WM;

	for (i = 0; i < count; i++) {
		if (s[i] == '\n')
			new_line++;
	}

	bytes_to_send += new_line;
	geni_write_reg_nolog(tx_wm, uport->membase,
					SE_GENI_TX_WATERMARK_REG);
	msm_geni_serial_setup_tx(uport, bytes_to_send);
	i = 0;
	while (i < count) {
		u32 chars_to_write = 0;
		u32 avail_fifo_bytes = (fifo_depth - tx_wm);
		/*
		 * If the WM bit never set, then the Tx state machine is not
		 * in a valid state, so break, cancel/abort any existing
		 * command. Unfortunately the current data being written is
		 * lost.
		 */
		while (!msm_geni_serial_poll_bit(uport, SE_GENI_M_IRQ_STATUS,
						M_TX_FIFO_WATERMARK_EN, true))
			break;
		chars_to_write = min((unsigned int)(count - i),
							avail_fifo_bytes);
		if ((chars_to_write << 1) > avail_fifo_bytes)
			chars_to_write = (avail_fifo_bytes >> 1);
		uart_console_write(uport, (s + i), chars_to_write,
						msm_geni_serial_wr_char);
		geni_write_reg_nolog(M_TX_FIFO_WATERMARK_EN, uport->membase,
							SE_GENI_M_IRQ_CLEAR);
		/* Ensure this goes through before polling for WM IRQ again.*/
		mb();
		i += chars_to_write;
	}
	msm_geni_serial_poll_cancel_tx(uport);
}

static void msm_geni_serial_console_write(struct console *co, const char *s,
			      unsigned int count)
{
	struct uart_port *uport;
	struct msm_geni_serial_port *port;
	int locked = 1;
	unsigned long flags;

	WARN_ON(co->index < 0 || co->index >= GENI_UART_NR_PORTS);

	port = get_port_from_line(co->index, true);
	if (IS_ERR_OR_NULL(port))
		return;

	uport = &port->uport;
	if (oops_in_progress)
		locked = spin_trylock_irqsave(&uport->lock, flags);
	else
		spin_lock_irqsave(&uport->lock, flags);

	if (locked) {
		__msm_geni_serial_console_write(uport, s, count);
		spin_unlock_irqrestore(&uport->lock, flags);
	}
}

static int handle_rx_console(struct uart_port *uport,
			unsigned int rx_fifo_wc,
			unsigned int rx_last_byte_valid,
			unsigned int rx_last)
{
	int i, c;
	unsigned char *rx_char;
	struct tty_port *tport;
	struct msm_geni_serial_port *msm_port = GET_DEV_PORT(uport);

	tport = &uport->state->port;
	for (i = 0; i < rx_fifo_wc; i++) {
		int bytes = 4;

		*(msm_port->rx_fifo) =
			geni_read_reg_nolog(uport->membase, SE_GENI_RX_FIFOn);
		rx_char = (unsigned char *)msm_port->rx_fifo;

		if (i == (rx_fifo_wc - 1)) {
			if (rx_last && rx_last_byte_valid)
				bytes = rx_last_byte_valid;
		}
		for (c = 0; c < bytes; c++) {
			char flag = TTY_NORMAL;
			int sysrq;

			uport->icount.rx++;
			sysrq = uart_handle_sysrq_char(uport, rx_char[c]);
			if (!sysrq)
				tty_insert_flip_char(tport, rx_char[c], flag);
		}
	}
	tty_flip_buffer_push(tport);
	return 0;
}
#else
static int handle_rx_console(struct uart_port *uport,
			unsigned int rx_fifo_wc,
			unsigned int rx_last_byte_valid,
			unsigned int rx_last)
{
	return -EPERM;
}

#endif /* (CONFIG_SERIAL_CORE_CONSOLE) || defined(CONFIG_CONSOLE_POLL)) */

static void msm_geni_serial_start_tx(struct uart_port *uport)
{
	unsigned int geni_m_irq_en;
	struct msm_geni_serial_port *msm_port = GET_DEV_PORT(uport);

	if (!msm_geni_serial_tx_empty(uport))
		return;

	if (!uart_console(uport) && pm_runtime_status_suspended(uport->dev)) {
		dev_err(uport->dev, "%s.Device is suspended.\n", __func__);
		return;
	}

	geni_m_irq_en = geni_read_reg_nolog(uport->membase, SE_GENI_M_IRQ_EN);
	geni_m_irq_en |= M_TX_FIFO_WATERMARK_EN;

	geni_write_reg_nolog(msm_port->tx_wm, uport->membase,
						SE_GENI_TX_WATERMARK_REG);
	geni_write_reg_nolog(geni_m_irq_en, uport->membase, SE_GENI_M_IRQ_EN);
	/* Geni command setup/irq enables should complete before returning.*/
	mb();
}

static void msm_geni_serial_stop_tx(struct uart_port *uport)
{
	unsigned int geni_m_irq_en;
	unsigned int geni_status;
	struct msm_geni_serial_port *port = GET_DEV_PORT(uport);

	if (!uart_console(uport) && pm_runtime_status_suspended(uport->dev))
		return;

	geni_m_irq_en = geni_read_reg_nolog(uport->membase, SE_GENI_M_IRQ_EN);
	geni_m_irq_en &= ~(M_TX_FIFO_WATERMARK_EN | M_CMD_DONE_EN);
	geni_write_reg_nolog(0, uport->membase, SE_GENI_TX_WATERMARK_REG);
	geni_write_reg_nolog(geni_m_irq_en, uport->membase, SE_GENI_M_IRQ_EN);

	geni_status = geni_read_reg_nolog(uport->membase,
						SE_GENI_STATUS);
	/* Possible stop tx is called multiple times. */
	if (!(geni_status & M_GENI_CMD_ACTIVE))
		return;

	geni_cancel_m_cmd(uport->membase);
	if (!msm_geni_serial_poll_bit(uport, SE_GENI_M_IRQ_STATUS,
						M_CMD_CANCEL_EN, true)) {
		geni_abort_m_cmd(uport->membase);
		msm_geni_serial_poll_bit(uport, SE_GENI_M_IRQ_STATUS,
						M_CMD_ABORT_EN, true);
		geni_write_reg_nolog(M_CMD_ABORT_EN, uport->membase,
							SE_GENI_M_IRQ_CLEAR);
	}
	geni_write_reg_nolog(M_CMD_CANCEL_EN, uport, SE_GENI_M_IRQ_CLEAR);
	IPC_LOG_MSG(port->ipc_log_misc, "%s\n", __func__);
}

static void msm_geni_serial_start_rx(struct uart_port *uport)
{
	unsigned int geni_s_irq_en;
	unsigned int geni_m_irq_en;
	unsigned long cfg0, cfg1;
	unsigned int rxstale = DEFAULT_BITS_PER_CHAR * STALE_TIMEOUT;
	unsigned int geni_status;
	struct msm_geni_serial_port *port = GET_DEV_PORT(uport);

	if (!uart_console(uport) && pm_runtime_status_suspended(uport->dev))
		return;

	geni_status = geni_read_reg_nolog(uport->membase, SE_GENI_STATUS);
	if (geni_status & S_GENI_CMD_ACTIVE)
		msm_geni_serial_abort_rx(uport);
	geni_s_irq_en = geni_read_reg_nolog(uport->membase,
						SE_GENI_S_IRQ_EN);
	geni_m_irq_en = geni_read_reg_nolog(uport->membase,
						SE_GENI_M_IRQ_EN);
	geni_s_irq_en |= S_RX_FIFO_WATERMARK_EN | S_RX_FIFO_LAST_EN;
	geni_m_irq_en |= M_RX_FIFO_WATERMARK_EN | M_RX_FIFO_LAST_EN;
	se_get_packing_config(8, 4, false, &cfg0, &cfg1);
	geni_write_reg_nolog(cfg0, uport->membase, SE_GENI_RX_PACKING_CFG0);
	geni_write_reg_nolog(cfg1, uport->membase, SE_GENI_RX_PACKING_CFG1);
	geni_write_reg_nolog(rxstale, uport->membase, SE_UART_RX_STALE_CNT);
	geni_setup_s_cmd(uport->membase, UART_START_READ, 0);
	geni_write_reg_nolog(geni_s_irq_en, uport->membase, SE_GENI_S_IRQ_EN);
	geni_write_reg_nolog(geni_m_irq_en, uport->membase, SE_GENI_M_IRQ_EN);
	/*
	 * Ensure the writes to the secondary sequencer and interrupt enables
	 * go through.
	 */
	mb();
	IPC_LOG_MSG(port->ipc_log_misc, "%s\n", __func__);
}

static void msm_geni_serial_stop_rx(struct uart_port *uport)
{
	unsigned int geni_s_irq_en;
	unsigned int geni_m_irq_en;
	unsigned int geni_status;

	if (!uart_console(uport) && pm_runtime_status_suspended(uport->dev))
		return;

	geni_s_irq_en = geni_read_reg_nolog(uport->membase,
						SE_GENI_S_IRQ_EN);
	geni_m_irq_en = geni_read_reg_nolog(uport->membase,
						SE_GENI_M_IRQ_EN);
	geni_s_irq_en &= ~(S_RX_FIFO_WATERMARK_EN | S_RX_FIFO_LAST_EN);
	geni_m_irq_en &= ~(M_RX_FIFO_WATERMARK_EN | M_RX_FIFO_LAST_EN);

	geni_write_reg_nolog(geni_s_irq_en, uport->membase, SE_GENI_S_IRQ_EN);
	geni_write_reg_nolog(geni_m_irq_en, uport->membase, SE_GENI_M_IRQ_EN);

	geni_status = geni_read_reg_nolog(uport->membase, SE_GENI_STATUS);
	/* Possible stop rx is called multiple times. */
	if (!(geni_status & S_GENI_CMD_ACTIVE))
		return;
	msm_geni_serial_abort_rx(uport);
}

static int handle_rx_hs(struct uart_port *uport,
			unsigned int rx_fifo_wc,
			unsigned int rx_last_byte_valid,
			unsigned int rx_last)
{
	unsigned char *rx_char;
	struct tty_port *tport;
	struct msm_geni_serial_port *msm_port = GET_DEV_PORT(uport);
	int ret;
	int rx_bytes = 0;

	rx_bytes = (msm_port->tx_fifo_width * (rx_fifo_wc - 1)) >> 3;
	rx_bytes += ((rx_last && rx_last_byte_valid) ?
			rx_last_byte_valid : msm_port->tx_fifo_width >> 3);

	tport = &uport->state->port;
	ioread32_rep((uport->membase + SE_GENI_RX_FIFOn), msm_port->rx_fifo,
								rx_fifo_wc);

	rx_char = (unsigned char *)msm_port->rx_fifo;
	ret = tty_insert_flip_string(tport, rx_char, rx_bytes);
	if (ret != rx_bytes) {
		dev_err(uport->dev, "%s: ret %d rx_bytes %d\n", __func__,
								ret, rx_bytes);
		WARN_ON(1);
	}
	uport->icount.rx += ret;
	tty_flip_buffer_push(tport);
	dump_ipc(msm_port->ipc_log_rx, "Rx", (char *)msm_port->rx_fifo, 0,
								rx_bytes);
	return ret;
}

static int msm_geni_serial_handle_rx(struct uart_port *uport)
{
	int ret = 0;
	unsigned int rx_fifo_status;
	unsigned int rx_fifo_wc = 0;
	unsigned int rx_last_byte_valid = 0;
	unsigned int rx_last = 0;
	struct tty_port *tport;
	struct msm_geni_serial_port *port = GET_DEV_PORT(uport);

	tport = &uport->state->port;
	rx_fifo_status = geni_read_reg_nolog(uport->membase,
				SE_GENI_RX_FIFO_STATUS);
	rx_fifo_wc = rx_fifo_status & RX_FIFO_WC_MSK;
	rx_last_byte_valid = ((rx_fifo_status & RX_LAST_BYTE_VALID_MSK) >>
						RX_LAST_BYTE_VALID_SHFT);
	rx_last = rx_fifo_status & RX_LAST;
	if (rx_fifo_wc)
		port->handle_rx(uport, rx_fifo_wc, rx_last_byte_valid,
								rx_last);
	return ret;
}

static int msm_geni_serial_handle_tx(struct uart_port *uport)
{
	int ret = 0;
	struct msm_geni_serial_port *msm_port = GET_DEV_PORT(uport);
	struct circ_buf *xmit = &uport->state->xmit;
	unsigned int avail_fifo_bytes = 0;
	unsigned int bytes_remaining = 0;
	int i = 0;
	unsigned int tx_fifo_status;
	unsigned int xmit_size;
	unsigned int fifo_width_bytes =
		(uart_console(uport) ? 1 : (msm_port->tx_fifo_width >> 3));

	tx_fifo_status = geni_read_reg_nolog(uport->membase,
					SE_GENI_TX_FIFO_STATUS);
	if (uart_circ_empty(xmit) && !tx_fifo_status) {
		msm_geni_serial_stop_tx(uport);
		goto exit_handle_tx;
	}

	avail_fifo_bytes = (msm_port->tx_fifo_depth - msm_port->tx_wm) *
							fifo_width_bytes;
	xmit_size = uart_circ_chars_pending(xmit);
	if (xmit_size > (UART_XMIT_SIZE - xmit->tail))
		xmit_size = UART_XMIT_SIZE - xmit->tail;
	if (xmit_size > avail_fifo_bytes)
		xmit_size = avail_fifo_bytes;

	if (!xmit_size)
		goto exit_handle_tx;

	msm_geni_serial_setup_tx(uport, xmit_size);

	bytes_remaining = xmit_size;
	dump_ipc(msm_port->ipc_log_tx, "Tx", (char *)&xmit->buf[xmit->tail], 0,
								xmit_size);
	while (i < xmit_size) {
		unsigned int tx_bytes;
		unsigned int buf = 0;
		int c;

		tx_bytes = ((bytes_remaining < fifo_width_bytes) ?
					bytes_remaining : fifo_width_bytes);

		for (c = 0; c < tx_bytes ; c++)
			buf |= (xmit->buf[xmit->tail + c] << (c * 8));
		geni_write_reg_nolog(buf, uport->membase, SE_GENI_TX_FIFOn);
		xmit->tail = (xmit->tail + tx_bytes) & (UART_XMIT_SIZE - 1);
		i += tx_bytes;
		uport->icount.tx += tx_bytes;
		bytes_remaining -= tx_bytes;
		/* Ensure FIFO write goes through */
		wmb();
	}
	msm_geni_serial_poll_cancel_tx(uport);
	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(uport);
exit_handle_tx:
	return ret;
}

static irqreturn_t msm_geni_serial_isr(int isr, void *dev)
{
	unsigned int m_irq_status;
	unsigned int s_irq_status;
	struct uart_port *uport = dev;
	unsigned long flags;

	spin_lock_irqsave(&uport->lock, flags);
	if (uart_console(uport) && uport->suspended)
		goto exit_geni_serial_isr;
	if (!uart_console(uport) && pm_runtime_status_suspended(uport->dev))
		goto exit_geni_serial_isr;
	m_irq_status = geni_read_reg_nolog(uport->membase,
						SE_GENI_M_IRQ_STATUS);
	s_irq_status = geni_read_reg_nolog(uport->membase,
						SE_GENI_S_IRQ_STATUS);
	geni_write_reg_nolog(m_irq_status, uport->membase,
						SE_GENI_M_IRQ_CLEAR);
	geni_write_reg_nolog(s_irq_status, uport->membase,
						SE_GENI_S_IRQ_CLEAR);

	if ((m_irq_status & M_ILLEGAL_CMD_EN)) {
		WARN_ON(1);
		goto exit_geni_serial_isr;
	}

	if ((s_irq_status & S_RX_FIFO_WATERMARK_EN) ||
		(s_irq_status & S_RX_FIFO_LAST_EN)) {
		msm_geni_serial_handle_rx(uport);
	}

	if ((m_irq_status & M_TX_FIFO_WATERMARK_EN))
		msm_geni_serial_handle_tx(uport);

exit_geni_serial_isr:
	spin_unlock_irqrestore(&uport->lock, flags);
	return IRQ_HANDLED;
}

static irqreturn_t msm_geni_wakeup_isr(int isr, void *dev)
{
	struct uart_port *uport = dev;
	struct msm_geni_serial_port *port = GET_DEV_PORT(uport);
	struct tty_struct *tty;
	unsigned long flags;

	spin_lock_irqsave(&uport->lock, flags);
	if (port->wakeup_byte) {
		tty = uport->state->port.tty;
		tty_insert_flip_char(tty->port, port->wakeup_byte, TTY_NORMAL);
		IPC_LOG_MSG(port->ipc_log_rx, "%s: Inject 0x%x\n",
					__func__, port->wakeup_byte);
		tty_flip_buffer_push(tty->port);
	}
	__pm_wakeup_event(&port->geni_wake, WAKEBYTE_TIMEOUT_MSEC);
	IPC_LOG_MSG(port->ipc_log_misc, "%s:Holding Wake Lock for %d ms\n",
					__func__, WAKEBYTE_TIMEOUT_MSEC);
	spin_unlock_irqrestore(&uport->lock, flags);
	return IRQ_HANDLED;
}

static int get_tx_fifo_size(struct msm_geni_serial_port *port)
{
	struct uart_port *uport;

	if (!port)
		return -ENODEV;

	uport = &port->uport;
	port->tx_fifo_depth = get_tx_fifo_depth(uport->membase);
	if (!port->tx_fifo_depth) {
		dev_err(uport->dev, "%s:Invalid TX FIFO depth read\n",
								__func__);
		return -ENXIO;
	}

	port->tx_fifo_width = get_tx_fifo_width(uport->membase);
	if (!port->tx_fifo_width) {
		dev_err(uport->dev, "%s:Invalid TX FIFO width read\n",
								__func__);
		return -ENXIO;
	}

	port->rx_fifo_depth = get_rx_fifo_depth(uport->membase);
	if (!port->rx_fifo_depth) {
		dev_err(uport->dev, "%s:Invalid RX FIFO depth read\n",
								__func__);
		return -ENXIO;
	}

	uport->fifosize =
		((port->tx_fifo_depth * port->tx_fifo_width) >> 3);
	return 0;
}

static void set_rfr_wm(struct msm_geni_serial_port *port)
{
	/*
	 * Set RFR (Flow off) to FIFO_DEPTH - 2.
	 * RX WM level at 50% RX_FIFO_DEPTH.
	 * TX WM level at 10% TX_FIFO_DEPTH.
	 */
	port->rx_rfr = port->rx_fifo_depth - 2;
	port->rx_wm = port->rx_fifo_depth >>  1;
	port->tx_wm = 2;
}

static void msm_geni_serial_shutdown(struct uart_port *uport)
{
	struct msm_geni_serial_port *msm_port = GET_DEV_PORT(uport);
	unsigned long flags;

	/* Stop the console before stopping the current tx */
	if (uart_console(uport))
		console_stop(uport->cons);

	spin_lock_irqsave(&uport->lock, flags);
	msm_geni_serial_stop_tx(uport);
	msm_geni_serial_stop_rx(uport);
	spin_unlock_irqrestore(&uport->lock, flags);

	disable_irq(uport->irq);
	free_irq(uport->irq, msm_port);
	if (uart_console(uport)) {
		se_geni_resources_off(&msm_port->serial_rsc);
	} else {
		if (msm_port->wakeup_irq > 0) {
			disable_irq(msm_port->wakeup_irq);
			free_irq(msm_port->wakeup_irq, msm_port);
		}
		__pm_relax(&msm_port->geni_wake);
		msm_geni_serial_power_off(uport);
	}
	IPC_LOG_MSG(msm_port->ipc_log_misc, "%s\n", __func__);
}

static int msm_geni_serial_port_setup(struct uart_port *uport)
{
	int ret = 0;
	struct msm_geni_serial_port *msm_port = GET_DEV_PORT(uport);
	unsigned long cfg0, cfg1;


	set_rfr_wm(msm_port);
	if (!uart_console(uport)) {
		/* For now only assume FIFO mode. */
		msm_port->xfer_mode = FIFO_MODE;
		se_get_packing_config(8, 4, false, &cfg0, &cfg1);
		geni_write_reg_nolog(cfg0, uport->membase,
						SE_GENI_TX_PACKING_CFG0);
		geni_write_reg_nolog(cfg1, uport->membase,
						SE_GENI_TX_PACKING_CFG1);
	} else {
		/*
		 * Make an unconditional cancel on the main sequencer to reset
		 * it else we could end up in data loss scenarios.
		 */
		msm_port->xfer_mode = FIFO_MODE;
		msm_geni_serial_poll_cancel_tx(uport);
		se_get_packing_config(8, 1, false, &cfg0, &cfg1);
		geni_write_reg_nolog(cfg0, uport->membase,
						SE_GENI_TX_PACKING_CFG0);
		geni_write_reg_nolog(cfg1, uport->membase,
						SE_GENI_TX_PACKING_CFG1);
	}
	ret = geni_se_init(uport->membase, msm_port->rx_wm, msm_port->rx_rfr);
	if (ret) {
		dev_err(uport->dev, "%s: Fail\n", __func__);
		goto exit_portsetup;
	}

	ret = geni_se_select_mode(uport->membase, msm_port->xfer_mode);
	if (ret)
		goto exit_portsetup;

	msm_port->port_setup = true;
	/*
	 * Ensure Port setup related IO completes before returning to
	 * framework.
	 */
	mb();
	if (!uart_console(uport)) {
		char name[30];

		memset(name, 0, sizeof(name));
		if (!msm_port->ipc_log_rx) {
			scnprintf(name, sizeof(name), "%s%s",
					dev_name(uport->dev), "_rx");
			msm_port->ipc_log_rx = ipc_log_context_create(
					IPC_LOG_TX_RX_PAGES, name, 0);
			if (!msm_port->ipc_log_rx)
				dev_info(uport->dev, "Err in Rx IPC Log\n");
		}
		memset(name, 0, sizeof(name));
		if (!msm_port->ipc_log_tx) {
			scnprintf(name, sizeof(name), "%s%s",
					dev_name(uport->dev), "_tx");
			msm_port->ipc_log_tx = ipc_log_context_create(
					IPC_LOG_TX_RX_PAGES, name, 0);
			if (!msm_port->ipc_log_tx)
				dev_info(uport->dev, "Err in Tx IPC Log\n");
		}
		memset(name, 0, sizeof(name));
		if (!msm_port->ipc_log_pwr) {
			scnprintf(name, sizeof(name), "%s%s",
					dev_name(uport->dev), "_pwr");
			msm_port->ipc_log_pwr = ipc_log_context_create(
					IPC_LOG_PWR_PAGES, name, 0);
			if (!msm_port->ipc_log_pwr)
				dev_info(uport->dev, "Err in Pwr IPC Log\n");
		}
		memset(name, 0, sizeof(name));
		if (!msm_port->ipc_log_misc) {
			scnprintf(name, sizeof(name), "%s%s",
					dev_name(uport->dev), "_misc");
			msm_port->ipc_log_misc = ipc_log_context_create(
					IPC_LOG_MISC_PAGES, name, 0);
			if (!msm_port->ipc_log_misc)
				dev_info(uport->dev, "Err in Misc IPC Log\n");
		}

	}
exit_portsetup:
	return ret;
}

static int msm_geni_serial_startup(struct uart_port *uport)
{
	int ret = 0;
	struct msm_geni_serial_port *msm_port = GET_DEV_PORT(uport);

	scnprintf(msm_port->name, sizeof(msm_port->name), "msm_serial_geni%d",
				uport->line);

	if (likely(!uart_console(uport))) {
		ret = msm_geni_serial_power_on(&msm_port->uport);
		if (ret)
			goto exit_startup;
	}

	if (unlikely(get_se_proto(uport->membase) != UART)) {
		dev_err(uport->dev, "%s: Invalid FW %d loaded.\n",
				 __func__, get_se_proto(uport->membase));
		ret = -ENXIO;
		disable_irq(uport->irq);
		free_irq(uport->irq, msm_port);
		goto exit_startup;
	}

	if (!msm_port->port_setup) {
		if (msm_geni_serial_port_setup(uport))
			goto exit_startup;
	}

	get_tx_fifo_size(msm_port);
	msm_geni_serial_start_rx(uport);
	/*
	 * Ensure that all the port configuration writes complete
	 * before returning to the framework.
	 */
	mb();
	ret = request_irq(uport->irq, msm_geni_serial_isr, IRQF_TRIGGER_HIGH,
			msm_port->name, msm_port);
	if (unlikely(ret)) {
		dev_err(uport->dev, "%s: Failed to get IRQ ret %d\n",
							__func__, ret);
		goto exit_startup;
	}

	if (msm_port->wakeup_irq > 0) {
		ret = request_threaded_irq(msm_port->wakeup_irq, NULL,
				msm_geni_wakeup_isr,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				"hs_uart_wakeup", uport);
		if (unlikely(ret)) {
			dev_err(uport->dev, "%s:Failed to get WakeIRQ ret%d\n",
								__func__, ret);
			goto exit_startup;
		}
		disable_irq(msm_port->wakeup_irq);
	}
	IPC_LOG_MSG(msm_port->ipc_log_misc, "%s\n", __func__);
exit_startup:
	return ret;
}

static int get_clk_cfg(unsigned long clk_freq, unsigned long *ser_clk)
{
	unsigned long root_freq[] = {7372800, 14745600, 19200000, 29491200,
		32000000, 48000000, 64000000, 80000000, 96000000, 100000000};
	int i;
	int match = -1;

	for (i = 0; i < ARRAY_SIZE(root_freq); i++) {
		if (clk_freq > root_freq[i])
			continue;

		if (!(root_freq[i] % clk_freq)) {
			match = i;
			break;
		}
	}
	if (match != -1)
		*ser_clk = root_freq[match];
	else
		pr_err("clk_freq %ld\n", clk_freq);
	return match;
}

static void geni_serial_write_term_regs(struct uart_port *uport, u32 loopback,
		u32 tx_trans_cfg, u32 tx_parity_cfg, u32 rx_trans_cfg,
		u32 rx_parity_cfg, u32 bits_per_char, u32 stop_bit_len,
		u32 s_clk_cfg)
{
	geni_write_reg_nolog(loopback, uport->membase, SE_UART_LOOPBACK_CFG);
	geni_write_reg_nolog(tx_trans_cfg, uport->membase,
							SE_UART_TX_TRANS_CFG);
	geni_write_reg_nolog(tx_parity_cfg, uport->membase,
							SE_UART_TX_PARITY_CFG);
	geni_write_reg_nolog(rx_trans_cfg, uport->membase,
							SE_UART_RX_TRANS_CFG);
	geni_write_reg_nolog(rx_parity_cfg, uport->membase,
							SE_UART_RX_PARITY_CFG);
	geni_write_reg_nolog(bits_per_char, uport->membase,
							SE_UART_TX_WORD_LEN);
	geni_write_reg_nolog(bits_per_char, uport->membase,
							SE_UART_RX_WORD_LEN);
	geni_write_reg_nolog(stop_bit_len, uport->membase,
						SE_UART_TX_STOP_BIT_LEN);
	geni_write_reg_nolog(s_clk_cfg, uport->membase, GENI_SER_M_CLK_CFG);
	geni_write_reg_nolog(s_clk_cfg, uport->membase, GENI_SER_S_CLK_CFG);
}

static int get_clk_div_rate(unsigned int baud, unsigned long *desired_clk_rate)
{
	unsigned long ser_clk;
	int dfs_index;
	int clk_div = 0;

	*desired_clk_rate = baud * UART_OVERSAMPLING;
	dfs_index = get_clk_cfg(*desired_clk_rate, &ser_clk);
	if (dfs_index < 0) {
		pr_err("%s: Can't find matching DFS entry for baud %d\n",
								__func__, baud);
		clk_div = -EINVAL;
		goto exit_get_clk_div_rate;
	}

	clk_div = ser_clk / *desired_clk_rate;
	*desired_clk_rate = ser_clk;
exit_get_clk_div_rate:
	return clk_div;
}

static void msm_geni_serial_set_termios(struct uart_port *uport,
				struct ktermios *termios, struct ktermios *old)
{
	unsigned int baud;
	unsigned int bits_per_char = 0;
	unsigned int tx_trans_cfg;
	unsigned int tx_parity_cfg;
	unsigned int rx_trans_cfg;
	unsigned int rx_parity_cfg;
	unsigned int stop_bit_len;
	unsigned int clk_div;
	unsigned long ser_clk_cfg = 0;
	struct msm_geni_serial_port *port = GET_DEV_PORT(uport);
	unsigned long clk_rate;

	/* baud rate */
	baud = uart_get_baud_rate(uport, termios, old, 300, 4000000);
	port->cur_baud = baud;
	clk_div = get_clk_div_rate(baud, &clk_rate);
	if (clk_div <= 0)
		goto exit_set_termios;

	uport->uartclk = clk_rate;
	clk_set_rate(port->serial_rsc.se_clk, clk_rate);
	ser_clk_cfg |= SER_CLK_EN;
	ser_clk_cfg |= (clk_div << CLK_DIV_SHFT);

	/* parity */
	tx_trans_cfg = geni_read_reg_nolog(uport->membase,
							SE_UART_TX_TRANS_CFG);
	tx_parity_cfg = geni_read_reg_nolog(uport->membase,
							SE_UART_TX_PARITY_CFG);
	rx_trans_cfg = geni_read_reg_nolog(uport->membase,
							SE_UART_RX_TRANS_CFG);
	rx_parity_cfg = geni_read_reg_nolog(uport->membase,
							SE_UART_RX_PARITY_CFG);
	if (termios->c_cflag & PARENB) {
		tx_trans_cfg |= UART_TX_PAR_EN;
		rx_trans_cfg |= UART_RX_PAR_EN;
		tx_parity_cfg |= PAR_CALC_EN;
		rx_parity_cfg |= PAR_CALC_EN;
		if (termios->c_cflag & PARODD) {
			tx_parity_cfg |= PAR_ODD;
			rx_parity_cfg |= PAR_ODD;
		} else if (termios->c_cflag & CMSPAR) {
			tx_parity_cfg |= PAR_SPACE;
			rx_parity_cfg |= PAR_SPACE;
		} else {
			tx_parity_cfg |= PAR_EVEN;
			rx_parity_cfg |= PAR_EVEN;
		}
	} else {
		tx_trans_cfg &= ~UART_TX_PAR_EN;
		rx_trans_cfg &= ~UART_RX_PAR_EN;
		tx_parity_cfg &= ~PAR_CALC_EN;
		rx_parity_cfg &= ~PAR_CALC_EN;
	}

	/* bits per char */
	switch (termios->c_cflag & CSIZE) {
	case CS5:
		bits_per_char = 5;
		break;
	case CS6:
		bits_per_char = 6;
		break;
	case CS7:
		bits_per_char = 7;
		break;
	case CS8:
	default:
		bits_per_char = 8;
		break;
	}


	/* stop bits */
	if (termios->c_cflag & CSTOPB)
		stop_bit_len = TX_STOP_BIT_LEN_2;
	else
		stop_bit_len = TX_STOP_BIT_LEN_1;

	/* flow control, clear the CTS_MASK bit if using flow control. */
	if (termios->c_cflag & CRTSCTS)
		tx_trans_cfg &= ~UART_CTS_MASK;
	else
		tx_trans_cfg |= UART_CTS_MASK;
	/* status bits to ignore */

	geni_serial_write_term_regs(uport, port->loopback, tx_trans_cfg,
		tx_parity_cfg, rx_trans_cfg, rx_parity_cfg, bits_per_char,
		stop_bit_len, ser_clk_cfg);
	IPC_LOG_MSG(port->ipc_log_misc, "%s: baud %d\n", __func__, baud);
	IPC_LOG_MSG(port->ipc_log_misc, "Tx: trans_cfg%d parity %d\n",
						tx_trans_cfg, tx_parity_cfg);
	IPC_LOG_MSG(port->ipc_log_misc, "Rx: trans_cfg%d parity %d",
						rx_trans_cfg, rx_parity_cfg);
	IPC_LOG_MSG(port->ipc_log_misc, "BitsChar%d stop bit%d\n",
				bits_per_char, stop_bit_len);
exit_set_termios:
	return;

}

static unsigned int msm_geni_serial_tx_empty(struct uart_port *uport)
{
	unsigned int tx_fifo_status;
	unsigned int is_tx_empty = 1;

	tx_fifo_status = geni_read_reg_nolog(uport->membase,
					SE_GENI_TX_FIFO_STATUS);
	if (tx_fifo_status)
		is_tx_empty = 0;

	return is_tx_empty;
}

#if defined(CONFIG_SERIAL_CORE_CONSOLE) || defined(CONFIG_CONSOLE_POLL)
static int __init msm_geni_console_setup(struct console *co, char *options)
{
	struct uart_port *uport;
	struct msm_geni_serial_port *dev_port;
	int baud = 115200;
	int bits = 8;
	int parity = 'n';
	int flow = 'n';
	int ret = 0;

	if (unlikely(co->index >= GENI_UART_NR_PORTS  || co->index < 0))
		return -ENXIO;

	dev_port = get_port_from_line(co->index, true);
	if (IS_ERR_OR_NULL(dev_port)) {
		ret = PTR_ERR(dev_port);
		pr_err("Invalid line %d(%d)\n", co->index, ret);
		return ret;
	}

	uport = &dev_port->uport;

	if (unlikely(!uport->membase))
		return -ENXIO;

	if (se_geni_resources_on(&dev_port->serial_rsc))
		WARN_ON(1);

	if (unlikely(get_se_proto(uport->membase) != UART)) {
		se_geni_resources_off(&dev_port->serial_rsc);
		return -ENXIO;
	}

	if (!dev_port->port_setup)
		msm_geni_serial_port_setup(uport);

	if (options)
		uart_parse_options(options, &baud, &parity, &bits, &flow);

	return uart_set_options(uport, co, baud, parity, bits, flow);
}

static void
msm_geni_serial_early_console_write(struct console *con, const char *s,
			unsigned int n)
{
	struct earlycon_device *dev = con->data;

	__msm_geni_serial_console_write(&dev->port, s, n);
}

static int __init
msm_geni_serial_earlycon_setup(struct earlycon_device *dev,
		const char *opt)
{
	struct uart_port *uport = &dev->port;
	int ret = 0;
	u32 tx_trans_cfg = 0;
	u32 tx_parity_cfg = 0;
	u32 rx_trans_cfg = 0;
	u32 rx_parity_cfg = 0;
	u32 stop_bit = 0;
	u32 rx_stale = 0;
	u32 bits_per_char = 0;
	u32 s_clk_cfg = 0;
	u32 baud = 115200;
	u32 clk_div;
	unsigned long clk_rate;
	unsigned long cfg0, cfg1;

	if (!uport->membase) {
		ret = -ENOMEM;
		goto exit_geni_serial_earlyconsetup;
	}

	if (get_se_proto(uport->membase) != UART) {
		ret = -ENXIO;
		goto exit_geni_serial_earlyconsetup;
	}

	/*
	 * Ignore Flow control.
	 * Disable Tx Parity.
	 * Don't check Parity during Rx.
	 * Disable Rx Parity.
	 * n = 8.
	 * Stop bit = 0.
	 * Stale timeout in bit-time (3 chars worth).
	 */
	tx_trans_cfg |= UART_CTS_MASK;
	tx_parity_cfg = 0;
	rx_trans_cfg = 0;
	rx_parity_cfg = 0;
	bits_per_char = 0x8;
	stop_bit = 0;
	rx_stale = 0x18;
	clk_div = get_clk_div_rate(baud, &clk_rate);
	if (clk_div <= 0) {
		ret = -EINVAL;
		goto exit_geni_serial_earlyconsetup;
	}

	s_clk_cfg |= SER_CLK_EN;
	s_clk_cfg |= (clk_div << CLK_DIV_SHFT);

	/*
	 * Make an unconditional cancel on the main sequencer to reset
	 * it else we could end up in data loss scenarios.
	 */
	msm_geni_serial_poll_cancel_tx(uport);
	msm_geni_serial_abort_rx(uport);
	se_get_packing_config(8, 1, false, &cfg0, &cfg1);
	geni_se_init(uport->membase, (DEF_FIFO_DEPTH_WORDS >> 1),
					(DEF_FIFO_DEPTH_WORDS - 2));
	geni_se_select_mode(uport->membase, FIFO_MODE);
	geni_write_reg_nolog(cfg0, uport->membase, SE_GENI_TX_PACKING_CFG0);
	geni_write_reg_nolog(cfg1, uport->membase, SE_GENI_TX_PACKING_CFG1);
	geni_write_reg_nolog(tx_trans_cfg, uport->membase,
							SE_UART_TX_TRANS_CFG);
	geni_write_reg_nolog(tx_parity_cfg, uport->membase,
							SE_UART_TX_PARITY_CFG);
	geni_write_reg_nolog(rx_trans_cfg, uport->membase,
							SE_UART_RX_TRANS_CFG);
	geni_write_reg_nolog(rx_parity_cfg, uport->membase,
							SE_UART_RX_PARITY_CFG);
	geni_write_reg_nolog(bits_per_char, uport->membase,
							SE_UART_TX_WORD_LEN);
	geni_write_reg_nolog(bits_per_char, uport->membase,
							SE_UART_RX_WORD_LEN);
	geni_write_reg_nolog(stop_bit, uport->membase, SE_UART_TX_STOP_BIT_LEN);
	geni_write_reg_nolog(s_clk_cfg, uport->membase, GENI_SER_M_CLK_CFG);
	geni_write_reg_nolog(s_clk_cfg, uport->membase, GENI_SER_S_CLK_CFG);

	dev->con->write = msm_geni_serial_early_console_write;
	dev->con->setup = NULL;
	/*
	 * Ensure that the early console setup completes before
	 * returning.
	 */
	mb();
exit_geni_serial_earlyconsetup:
	return ret;
}
OF_EARLYCON_DECLARE(msm_geni_serial, "qcom,msm-geni-uart",
		msm_geni_serial_earlycon_setup);

static int console_register(struct uart_driver *drv)
{
	return uart_register_driver(drv);
}
static void console_unregister(struct uart_driver *drv)
{
	uart_unregister_driver(drv);
}

static struct console cons_ops = {
	.name = "ttyMSM",
	.write = msm_geni_serial_console_write,
	.device = uart_console_device,
	.setup = msm_geni_console_setup,
	.flags = CON_PRINTBUFFER,
	.index = -1,
	.data = &msm_geni_console_driver,
};

static struct uart_driver msm_geni_console_driver = {
	.owner = THIS_MODULE,
	.driver_name = "msm_geni_console",
	.dev_name = "ttyMSM",
	.nr =  GENI_UART_NR_PORTS,
	.cons = &cons_ops,
};
#else
static int console_register(struct uart_driver *drv)
{
	return 0;
}

static void console_unregister(struct uart_driver *drv)
{
}
#endif /* defined(CONFIG_SERIAL_CORE_CONSOLE) || defined(CONFIG_CONSOLE_POLL) */

static void msm_geni_serial_debug_init(struct uart_port *uport)
{
	struct msm_geni_serial_port *msm_port = GET_DEV_PORT(uport);

	msm_port->dbg = debugfs_create_dir(dev_name(uport->dev), NULL);
	if (IS_ERR_OR_NULL(msm_port->dbg))
		dev_err(uport->dev, "Failed to create dbg dir\n");
}

static const struct uart_ops msm_geni_console_pops = {
	.tx_empty = msm_geni_serial_tx_empty,
	.stop_tx = msm_geni_serial_stop_tx,
	.start_tx = msm_geni_serial_start_tx,
	.stop_rx = msm_geni_serial_stop_rx,
	.set_termios = msm_geni_serial_set_termios,
	.startup = msm_geni_serial_startup,
	.config_port = msm_geni_serial_config_port,
	.shutdown = msm_geni_serial_shutdown,
	.type = msm_geni_serial_get_type,
	.set_mctrl = msm_geni_cons_set_mctrl,
	.get_mctrl = msm_geni_cons_get_mctrl,
#ifdef CONFIG_CONSOLE_POLL
	.poll_get_char	= msm_geni_serial_get_char,
	.poll_put_char	= msm_geni_serial_poll_put_char,
#endif
};

static const struct uart_ops msm_geni_serial_pops = {
	.tx_empty = msm_geni_serial_tx_empty,
	.stop_tx = msm_geni_serial_stop_tx,
	.start_tx = msm_geni_serial_start_tx,
	.stop_rx = msm_geni_serial_stop_rx,
	.set_termios = msm_geni_serial_set_termios,
	.startup = msm_geni_serial_startup,
	.config_port = msm_geni_serial_config_port,
	.shutdown = msm_geni_serial_shutdown,
	.type = msm_geni_serial_get_type,
	.set_mctrl = msm_geni_serial_set_mctrl,
	.get_mctrl = msm_geni_serial_get_mctrl,
	.break_ctl = msm_geni_serial_break_ctl,
	.flush_buffer = NULL,
	.ioctl = msm_geni_serial_ioctl,
};

static const struct of_device_id msm_geni_device_tbl[] = {
#if defined(CONFIG_SERIAL_CORE_CONSOLE) || defined(CONFIG_CONSOLE_POLL)
	{ .compatible = "qcom,msm-geni-console",
			.data = (void *)&msm_geni_console_driver},
#endif
	{ .compatible = "qcom,msm-geni-serial-hs",
			.data = (void *)&msm_geni_serial_hs_driver},
	{},
};

static int msm_geni_serial_probe(struct platform_device *pdev)
{
	int ret = 0;
	int line;
	struct msm_geni_serial_port *dev_port;
	struct uart_port *uport;
	struct resource *res;
	struct uart_driver *drv;
	const struct of_device_id *id;
	bool is_console = false;
	struct platform_device *wrapper_pdev;
	struct device_node *wrapper_ph_node;

	id = of_match_device(msm_geni_device_tbl, &pdev->dev);
	if (id) {
		dev_dbg(&pdev->dev, "%s: %s\n", __func__, id->compatible);
		drv = (struct uart_driver *)id->data;
	} else {
		dev_err(&pdev->dev, "%s: No matching device found", __func__);
		return -ENODEV;
	}

	if (pdev->dev.of_node) {
		if (drv->cons)
			line = of_alias_get_id(pdev->dev.of_node, "serial");
		else
			line = of_alias_get_id(pdev->dev.of_node, "hsuart");
	} else {
		line = pdev->id;
	}

	if (line < 0)
		line = atomic_inc_return(&uart_line_id) - 1;

	if ((line < 0) || (line >= GENI_UART_NR_PORTS))
		return -ENXIO;
	is_console = (drv->cons ? true : false);
	dev_port = get_port_from_line(line, is_console);
	if (IS_ERR_OR_NULL(dev_port)) {
		ret = PTR_ERR(dev_port);
		dev_err(&pdev->dev, "Invalid line %d(%d)\n",
					line, ret);
		goto exit_geni_serial_probe;
	}

	uport = &dev_port->uport;

	/* Don't allow 2 drivers to access the same port */
	if (uport->private_data) {
		ret = -ENODEV;
		goto exit_geni_serial_probe;
	}

	uport->dev = &pdev->dev;

	wrapper_ph_node = of_parse_phandle(pdev->dev.of_node,
					"qcom,wrapper-core", 0);
	if (IS_ERR_OR_NULL(wrapper_ph_node)) {
		ret = PTR_ERR(wrapper_ph_node);
		goto exit_geni_serial_probe;
	}
	wrapper_pdev = of_find_device_by_node(wrapper_ph_node);
	of_node_put(wrapper_ph_node);
	if (IS_ERR_OR_NULL(wrapper_pdev)) {
		ret = PTR_ERR(wrapper_pdev);
		goto exit_geni_serial_probe;
	}
	dev_port->wrapper_dev = &wrapper_pdev->dev;
	dev_port->serial_rsc.wrapper_dev = &wrapper_pdev->dev;
	ret = geni_se_resources_init(&dev_port->serial_rsc, UART_CORE2X_VOTE,
					(DEFAULT_SE_CLK * DEFAULT_BUS_WIDTH));
	if (ret)
		goto exit_geni_serial_probe;

	if (of_property_read_u8(pdev->dev.of_node, "qcom,wakeup-byte",
					&dev_port->wakeup_byte))
		dev_info(&pdev->dev, "No Wakeup byte specified\n");

	dev_port->serial_rsc.se_clk = devm_clk_get(&pdev->dev, "se-clk");
	if (IS_ERR(dev_port->serial_rsc.se_clk)) {
		ret = PTR_ERR(dev_port->serial_rsc.se_clk);
		dev_err(&pdev->dev, "Err getting SE Core clk %d\n", ret);
		goto exit_geni_serial_probe;
	}

	dev_port->serial_rsc.m_ahb_clk = devm_clk_get(&pdev->dev, "m-ahb");
	if (IS_ERR(dev_port->serial_rsc.m_ahb_clk)) {
		ret = PTR_ERR(dev_port->serial_rsc.m_ahb_clk);
		dev_err(&pdev->dev, "Err getting M AHB clk %d\n", ret);
		goto exit_geni_serial_probe;
	}

	dev_port->serial_rsc.s_ahb_clk = devm_clk_get(&pdev->dev, "s-ahb");
	if (IS_ERR(dev_port->serial_rsc.s_ahb_clk)) {
		ret = PTR_ERR(dev_port->serial_rsc.s_ahb_clk);
		dev_err(&pdev->dev, "Err getting S AHB clk %d\n", ret);
		goto exit_geni_serial_probe;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "se_phys");
	if (!res) {
		ret = -ENXIO;
		dev_err(&pdev->dev, "Err getting IO region\n");
		goto exit_geni_serial_probe;
	}

	uport->mapbase = res->start;
	uport->membase = devm_ioremap(&pdev->dev, res->start,
						resource_size(res));
	if (!uport->membase) {
		ret = -ENOMEM;
		dev_err(&pdev->dev, "Err IO mapping serial iomem");
		goto exit_geni_serial_probe;
	}

	dev_port->serial_rsc.geni_pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR_OR_NULL(dev_port->serial_rsc.geni_pinctrl)) {
		dev_err(&pdev->dev, "No pinctrl config specified!\n");
		ret = PTR_ERR(dev_port->serial_rsc.geni_pinctrl);
		goto exit_geni_serial_probe;
	}
	dev_port->serial_rsc.geni_gpio_active =
		pinctrl_lookup_state(dev_port->serial_rsc.geni_pinctrl,
							PINCTRL_DEFAULT);
	if (IS_ERR_OR_NULL(dev_port->serial_rsc.geni_gpio_active)) {
		dev_err(&pdev->dev, "No default config specified!\n");
		ret = PTR_ERR(dev_port->serial_rsc.geni_gpio_active);
		goto exit_geni_serial_probe;
	}
	dev_port->serial_rsc.geni_gpio_sleep =
		pinctrl_lookup_state(dev_port->serial_rsc.geni_pinctrl,
							PINCTRL_SLEEP);
	if (IS_ERR_OR_NULL(dev_port->serial_rsc.geni_gpio_sleep)) {
		dev_err(&pdev->dev, "No sleep config specified!\n");
		ret = PTR_ERR(dev_port->serial_rsc.geni_gpio_sleep);
		goto exit_geni_serial_probe;
	}

	wakeup_source_init(&dev_port->geni_wake, dev_name(&pdev->dev));
	dev_port->tx_fifo_depth = DEF_FIFO_DEPTH_WORDS;
	dev_port->rx_fifo_depth = DEF_FIFO_DEPTH_WORDS;
	dev_port->tx_fifo_width = DEF_FIFO_WIDTH_BITS;
	uport->fifosize =
		((dev_port->tx_fifo_depth * dev_port->tx_fifo_width) >> 3);

	uport->irq = platform_get_irq(pdev, 0);
	if (uport->irq < 0) {
		ret = uport->irq;
		dev_err(&pdev->dev, "Failed to get IRQ %d\n", ret);
		goto exit_geni_serial_probe;
	}

	/* Optional to use the Rx pin as wakeup irq */
	dev_port->wakeup_irq = platform_get_irq(pdev, 1);
	if ((dev_port->wakeup_irq < 0 && !is_console))
		dev_info(&pdev->dev, "No wakeup IRQ configured\n");

	uport->private_data = (void *)drv;
	platform_set_drvdata(pdev, dev_port);
	if (is_console) {
		dev_port->handle_rx = handle_rx_console;
		dev_port->rx_fifo = devm_kzalloc(uport->dev, sizeof(u32),
								GFP_KERNEL);
	} else {
		dev_port->handle_rx = handle_rx_hs;
		dev_port->rx_fifo = devm_kzalloc(uport->dev,
				sizeof(dev_port->rx_fifo_depth * sizeof(u32)),
								GFP_KERNEL);
		pm_runtime_set_suspended(&pdev->dev);
		pm_runtime_enable(&pdev->dev);
	}

	dev_info(&pdev->dev, "Serial port%d added.FifoSize %d is_console%d\n",
				line, uport->fifosize, is_console);
	device_create_file(uport->dev, &dev_attr_loopback);
	msm_geni_serial_debug_init(uport);
	dev_port->port_setup = false;
	return uart_add_one_port(drv, uport);

exit_geni_serial_probe:
	return ret;
}

static int msm_geni_serial_remove(struct platform_device *pdev)
{
	struct msm_geni_serial_port *port = platform_get_drvdata(pdev);
	struct uart_driver *drv =
			(struct uart_driver *)port->uport.private_data;

	wakeup_source_trash(&port->geni_wake);
	uart_remove_one_port(drv, &port->uport);
	return 0;
}


#ifdef CONFIG_PM
static int msm_geni_serial_runtime_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct msm_geni_serial_port *port = platform_get_drvdata(pdev);
	int ret = 0;

	ret = se_geni_resources_off(&port->serial_rsc);
	if (ret) {
		dev_err(dev, "%s: Error ret %d\n", __func__, ret);
		goto exit_runtime_suspend;
	}
	if (port->wakeup_irq > 0)
		enable_irq(port->wakeup_irq);
	IPC_LOG_MSG(port->ipc_log_pwr, "%s:\n", __func__);
exit_runtime_suspend:
	return ret;
}

static int msm_geni_serial_runtime_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct msm_geni_serial_port *port = platform_get_drvdata(pdev);
	int ret = 0;

	if (port->wakeup_irq > 0)
		disable_irq(port->wakeup_irq);
	ret = se_geni_resources_on(&port->serial_rsc);
	if (ret) {
		dev_err(dev, "%s: Error ret %d\n", __func__, ret);
		goto exit_runtime_resume;
	}
	IPC_LOG_MSG(port->ipc_log_pwr, "%s:\n", __func__);
exit_runtime_resume:
	return ret;
}

static int msm_geni_serial_sys_suspend_noirq(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct msm_geni_serial_port *port = platform_get_drvdata(pdev);
	struct uart_port *uport = &port->uport;

	if (uart_console(uport)) {
		uart_suspend_port((struct uart_driver *)uport->private_data,
					uport);
	} else {
		if (!pm_runtime_status_suspended(dev)) {
			dev_info(dev, "%s: Is still active\n", __func__);
			return -EBUSY;
		}
	}
	return 0;
}

static int msm_geni_serial_sys_resume_noirq(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct msm_geni_serial_port *port = platform_get_drvdata(pdev);
	struct uart_port *uport = &port->uport;

	if (uart_console(uport)) {
		se_geni_resources_on(&port->serial_rsc);
		uart_resume_port((struct uart_driver *)uport->private_data,
									uport);
	}
	return 0;
}
#else
static int msm_geni_serial_runtime_suspend(struct device *dev)
{
	return 0;
}

static int msm_geni_serial_runtime_resume(struct device *dev)
{
	return 0;
}

static int msm_geni_serial_sys_suspend_noirq(struct device *dev)
{
	return 0;
}

static int msm_geni_serial_sys_resume_noirq(struct device *dev)
{
	return 0;
}
#endif

static const struct dev_pm_ops msm_geni_serial_pm_ops = {
	.runtime_suspend = msm_geni_serial_runtime_suspend,
	.runtime_resume = msm_geni_serial_runtime_resume,
	.suspend_noirq = msm_geni_serial_sys_suspend_noirq,
	.resume_noirq = msm_geni_serial_sys_resume_noirq,
};

static const struct of_device_id msm_geni_serial_match_table[] = {
	{ .compatible = "qcom,msm-geni-uart"},
	{},
};

static struct platform_driver msm_geni_serial_platform_driver = {
	.remove = msm_geni_serial_remove,
	.probe = msm_geni_serial_probe,
	.driver = {
		.name = "msm_geni_serial",
		.of_match_table = msm_geni_serial_match_table,
		.pm = &msm_geni_serial_pm_ops,
	},
};


static struct uart_driver msm_geni_serial_hs_driver = {
	.owner = THIS_MODULE,
	.driver_name = "msm_geni_serial_hs",
	.dev_name = "ttyHS",
	.nr =  GENI_UART_NR_PORTS,
};

static int __init msm_geni_serial_init(void)
{
	int ret = 0;
	int i;

	for (i = 0; i < GENI_UART_NR_PORTS; i++) {
		msm_geni_serial_ports[i].uport.iotype = UPIO_MEM;
		msm_geni_serial_ports[i].uport.ops = &msm_geni_serial_pops;
		msm_geni_serial_ports[i].uport.flags = UPF_BOOT_AUTOCONF;
		msm_geni_serial_ports[i].uport.line = i;
	}

	for (i = 0; i < GENI_UART_CONS_PORTS; i++) {
		msm_geni_console_port.uport.iotype = UPIO_MEM;
		msm_geni_console_port.uport.ops = &msm_geni_console_pops;
		msm_geni_console_port.uport.flags = UPF_BOOT_AUTOCONF;
		msm_geni_console_port.uport.line = i;
	}

	ret = console_register(&msm_geni_console_driver);
	if (ret)
		return ret;

	ret = uart_register_driver(&msm_geni_serial_hs_driver);
	if (ret) {
		uart_unregister_driver(&msm_geni_console_driver);
		return ret;
	}

	ret = platform_driver_register(&msm_geni_serial_platform_driver);
	if (ret) {
		console_unregister(&msm_geni_console_driver);
		uart_unregister_driver(&msm_geni_serial_hs_driver);
		return ret;
	}

	pr_info("%s: Driver initialized", __func__);
	return ret;
}
module_init(msm_geni_serial_init);

static void __exit msm_geni_serial_exit(void)
{
	platform_driver_unregister(&msm_geni_serial_platform_driver);
	uart_unregister_driver(&msm_geni_serial_hs_driver);
	console_unregister(&msm_geni_console_driver);
}
module_exit(msm_geni_serial_exit);

MODULE_DESCRIPTION("Serial driver for GENI based QTI serial cores");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("tty:msm_geni_geni_serial");
