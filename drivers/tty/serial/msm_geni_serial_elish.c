// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 */

#include <linux/bitmap.h>
#include <linux/bitops.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/console.h>
#include <linux/io.h>
#include <linux/ipc_logging.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/qcom-geni-se.h>
#include <linux/serial.h>
#include <linux/serial_core.h>
#include <linux/slab.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/ioctl.h>
#include <linux/pinctrl/consumer.h>
#include <linux/dma-mapping.h>

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
#define UART_PARAM_RFR_OPEN		(BIT(7))

/* UART DMA Rx GP_IRQ_BITS */
#define UART_DMA_RX_PARITY_ERR	BIT(5)
#define UART_DMA_RX_ERRS	(GENMASK(5, 6))
#define UART_DMA_RX_BREAK	(GENMASK(7, 8))

#define UART_OVERSAMPLING	(32)
#define STALE_TIMEOUT		(16)
#define STALE_COUNT		(DEFAULT_BITS_PER_CHAR * STALE_TIMEOUT)
#define SEC_TO_USEC		(1000000)
#define SYSTEM_DELAY		(500)
#define DEFAULT_BITS_PER_CHAR	(10)
#define GENI_UART_NR_PORTS	(15)
#define GENI_UART_CONS_PORTS	(1)
#define DEF_FIFO_DEPTH_WORDS	(16)
#define DEF_TX_WM		(2)
#define DEF_FIFO_WIDTH_BITS	(32)
#define UART_CORE2X_VOTE	(5000)
#define UART_CONSOLE_CORE2X_VOTE (960)

#define WAKEBYTE_TIMEOUT_MSEC	(2000)
#define WAIT_XFER_MAX_ITER	(2)
#define WAIT_XFER_MAX_TIMEOUT_US	(10000)
#define WAIT_XFER_MIN_TIMEOUT_US	(9000)
#define IPC_LOG_PWR_PAGES	(10)
#define IPC_LOG_MISC_PAGES	(30)
#define IPC_LOG_TX_RX_PAGES	(30)
#define DATA_BYTES_PER_LINE	(32)

#define M_IRQ_BITS		(M_RX_FIFO_WATERMARK_EN | M_RX_FIFO_LAST_EN |\
				M_CMD_CANCEL_EN | M_CMD_ABORT_EN |\
				M_IO_DATA_ASSERT_EN | M_IO_DATA_DEASSERT_EN)

#define S_IRQ_BITS		(S_RX_FIFO_WATERMARK_EN | S_RX_FIFO_LAST_EN |\
				S_CMD_CANCEL_EN | S_CMD_ABORT_EN)
#define DMA_TX_IRQ_BITS		(TX_RESET_DONE | TX_DMA_DONE |\
				TX_GENI_CANCEL_IRQ | TX_EOT | TX_SBE)
#define DMA_RX_IRQ_BITS		(RX_EOT | RX_GENI_CANCEL_IRQ |\
				RX_RESET_DONE | UART_DMA_RX_ERRS |\
				UART_DMA_RX_PARITY_ERR | UART_DMA_RX_BREAK |\
				RX_DMA_DONE | RX_SBE)

/* Required for polling for 100 msecs */
#define POLL_WAIT_TIMEOUT_MSEC	100

/*
 * Number of iterrations required while polling
 * where each iterration has a delay of 100 usecs
 */
#define POLL_ITERATIONS		1000

#define IPC_LOG_MSG(ctx, x...) do { \
	if (ctx) \
		ipc_log_string(ctx, x); \
} while (0)

#define DMA_RX_BUF_SIZE		(2048)
#define UART_CONSOLE_RX_WM	(2)

struct msm_geni_serial_ver_info {
	int hw_major_ver;
	int hw_minor_ver;
	int hw_step_ver;
	int m_fw_ver;
	int s_fw_ver;
};

struct msm_geni_serial_port {
	struct uart_port uport;
	const char *name;
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
			unsigned int rx_last,
			bool drop_rx);
	struct device *wrapper_dev;
	struct se_geni_rsc serial_rsc;
	dma_addr_t tx_dma;
	unsigned int xmit_size;
	void *rx_buf;
	dma_addr_t rx_dma;
	int loopback;
	int wakeup_irq;
	unsigned char wakeup_byte;
	struct wakeup_source *geni_wake;
	void *ipc_log_tx;
	void *ipc_log_rx;
	void *ipc_log_pwr;
	void *ipc_log_misc;
	void *console_log;
	void *ipc_log_single;
	void *ipc_log_irqstatus;
	unsigned int cur_baud;
	int ioctl_count;
	int edge_count;
	bool manual_flow;
	struct msm_geni_serial_ver_info ver_info;
	u32 cur_tx_remaining;
	bool startup_in_progress;
	bool is_console;
	bool rumi_platform;
	bool m_cmd_done;
	bool s_cmd_done;
	bool m_cmd;
	bool s_cmd;
	struct completion m_cmd_timeout;
	struct completion s_cmd_timeout;
	spinlock_t rx_lock;
};

static const struct uart_ops msm_geni_serial_pops;
static struct uart_driver msm_geni_console_driver;
static struct uart_driver msm_geni_serial_hs_driver;
static int handle_rx_console(struct uart_port *uport,
			unsigned int rx_fifo_wc,
			unsigned int rx_last_byte_valid,
			unsigned int rx_last,
			bool drop_rx);
static int handle_rx_hs(struct uart_port *uport,
			unsigned int rx_fifo_wc,
			unsigned int rx_last_byte_valid,
			unsigned int rx_last,
			bool drop_rx);
static unsigned int msm_geni_serial_tx_empty(struct uart_port *port);
static int msm_geni_serial_power_on(struct uart_port *uport);
static void msm_geni_serial_power_off(struct uart_port *uport);
static int msm_geni_serial_poll_bit(struct uart_port *uport,
				int offset, int bit_field, bool set);
static void msm_geni_serial_stop_rx(struct uart_port *uport);
static int msm_geni_serial_runtime_resume(struct device *dev);
static int msm_geni_serial_runtime_suspend(struct device *dev);
static int msm_geni_serial_get_ver_info(struct uart_port *uport);
static void msm_geni_serial_set_manual_flow(bool enable,
				struct msm_geni_serial_port *port);
static bool handle_rx_dma_xfer(u32 s_irq_status, struct uart_port *uport);
static int uart_line_id;

#define GET_DEV_PORT(uport) \
	container_of(uport, struct msm_geni_serial_port, uport)

static struct msm_geni_serial_port msm_geni_console_port;
static struct msm_geni_serial_port msm_geni_serial_ports[GENI_UART_NR_PORTS];
static void msm_geni_serial_handle_isr(struct uart_port *uport,
				unsigned long *flags, bool is_irq_masked);

/*
 * The below API is required to check if uport->lock (spinlock)
 * is taken by the serial layer or not. If the lock is not taken
 * then we can rely on the isr to be fired and if the lock is taken
 * by the serial layer then we need to poll for the interrupts.
 *
 * Returns true(1) if spinlock is already taken by framework (serial layer)
 * Return false(0) if spinlock is not taken by framework.
 */
static int msm_geni_serial_spinlocked(struct uart_port *uport)
{
	unsigned long flags;
	bool locked;

	locked = spin_trylock_irqsave(&uport->lock, flags);
	if (locked)
		spin_unlock_irqrestore(&uport->lock, flags);

	return !locked;
}

/*
 * We are enabling the interrupts once the polling operations
 * is completed.
 */
static void msm_geni_serial_enable_interrupts(struct uart_port *uport)
{
	unsigned int geni_m_irq_en, geni_s_irq_en;
	struct msm_geni_serial_port *port = GET_DEV_PORT(uport);

	geni_m_irq_en = geni_read_reg_nolog(uport->membase,
						SE_GENI_M_IRQ_EN);
	geni_s_irq_en = geni_read_reg_nolog(uport->membase,
						SE_GENI_S_IRQ_EN);

	geni_m_irq_en |= M_IRQ_BITS;
	geni_s_irq_en |= S_IRQ_BITS;

	geni_write_reg_nolog(geni_m_irq_en, uport->membase, SE_GENI_M_IRQ_EN);
	geni_write_reg_nolog(geni_s_irq_en, uport->membase, SE_GENI_S_IRQ_EN);
	if (port->xfer_mode == SE_DMA) {
		geni_write_reg_nolog(DMA_TX_IRQ_BITS, uport->membase,
							SE_DMA_TX_IRQ_EN_SET);
		geni_write_reg_nolog(DMA_RX_IRQ_BITS, uport->membase,
							SE_DMA_RX_IRQ_EN_SET);
	}
}

/* Try disabling interrupts in order to do polling in an atomic contexts. */
static bool msm_serial_try_disable_interrupts(struct uart_port *uport)
{
	unsigned int geni_m_irq_en, geni_s_irq_en;
	struct msm_geni_serial_port *port = GET_DEV_PORT(uport);

	/*
	 * We don't need to disable interrupts if spinlock is not taken
	 * by framework as we can rely on ISR.
	 */
	if (!msm_geni_serial_spinlocked(uport))
		return false;

	geni_m_irq_en = geni_read_reg_nolog(uport->membase, SE_GENI_M_IRQ_EN);
	geni_s_irq_en = geni_read_reg_nolog(uport->membase, SE_GENI_S_IRQ_EN);

	geni_m_irq_en &= ~M_IRQ_BITS;
	geni_s_irq_en &= ~S_IRQ_BITS;

	geni_write_reg_nolog(geni_m_irq_en, uport->membase, SE_GENI_M_IRQ_EN);
	geni_write_reg_nolog(geni_s_irq_en, uport->membase, SE_GENI_S_IRQ_EN);
	if (port->xfer_mode == SE_DMA) {
		geni_write_reg_nolog(DMA_TX_IRQ_BITS, uport->membase,
							SE_DMA_TX_IRQ_EN_CLR);
		geni_write_reg_nolog(DMA_RX_IRQ_BITS, uport->membase,
							SE_DMA_RX_IRQ_EN_CLR);
	}

	return true;
}

/*
 * We need to poll for interrupt if we are in an atomic context
 * as serial framework might be taking spinlocks and depend on the isr
 * in a non-atomic context. This API decides wheather to poll for
 * interrupt or depend on the isr based on in_atomic() call.
 */
bool geni_wait_for_cmd_done(struct uart_port *uport, bool is_irq_masked)
{
	struct msm_geni_serial_port *msm_port = GET_DEV_PORT(uport);
	unsigned long timeout = POLL_ITERATIONS;
	unsigned long flags = 0;

	/*
	 * We need to do polling if spinlock is taken
	 * by framework as we cannot rely on ISR.
	 */
	if (is_irq_masked) {
		/*
		 * Polling is done for 1000 iterrations with
		 * 10 usecs interval which in total accumulates
		 * to 10 msecs
		 */
		if (msm_port->m_cmd) {
			while (!msm_port->m_cmd_done && timeout > 0) {
				msm_geni_serial_handle_isr(uport, &flags, true);
				timeout--;
				udelay(100);
			}
		} else if (msm_port->s_cmd) {
			while (!msm_port->s_cmd_done && timeout > 0) {
				msm_geni_serial_handle_isr(uport, &flags, true);
				timeout--;
				udelay(100);
			}
		}
	} else {
		/* Waiting for 10 milli second for interrupt to be fired */
		if (msm_port->m_cmd)
			timeout = wait_for_completion_timeout
					(&msm_port->m_cmd_timeout,
				msecs_to_jiffies(POLL_WAIT_TIMEOUT_MSEC));
		else if (msm_port->s_cmd)
			timeout = wait_for_completion_timeout
					(&msm_port->s_cmd_timeout,
				msecs_to_jiffies(POLL_WAIT_TIMEOUT_MSEC));
	}

	return timeout ? 0 : 1;
}

static void msm_geni_serial_config_port(struct uart_port *uport, int cfg_flags)
{
	if (cfg_flags & UART_CONFIG_TYPE)
		uport->type = PORT_MSM;
}

static ssize_t loopback_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct msm_geni_serial_port *port = platform_get_drvdata(pdev);

	return snprintf(buf, sizeof(int), "%d\n", port->loopback);
}

static ssize_t loopback_store(struct device *dev,
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

static DEVICE_ATTR_RW(loopback);

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

static bool device_pending_suspend(struct uart_port *uport)
{
	int usage_count = atomic_read(&uport->dev->power.usage_count);

	return (pm_runtime_status_suspended(uport->dev) || !usage_count);
}

static bool check_transfers_inflight(struct uart_port *uport)
{
	bool xfer_on = false;
	bool tx_active = false;
	bool tx_fifo_status = false;
	bool m_cmd_active = false;
	bool rx_active = false;
	u32 rx_fifo_status = 0;
	struct msm_geni_serial_port *port = GET_DEV_PORT(uport);
	u32 geni_status = geni_read_reg_nolog(uport->membase,
						SE_GENI_STATUS);
	struct circ_buf *xmit = &uport->state->xmit;

	/* Possible stop tx is called multiple times. */
	m_cmd_active = geni_status & M_GENI_CMD_ACTIVE;
	if (port->xfer_mode == SE_DMA) {
		tx_fifo_status = port->tx_dma ? 1 : 0;
		rx_fifo_status =
			geni_read_reg_nolog(uport->membase, SE_DMA_RX_LEN_IN);
	} else {
		tx_fifo_status = geni_read_reg_nolog(uport->membase,
						SE_GENI_TX_FIFO_STATUS);
		rx_fifo_status = geni_read_reg_nolog(uport->membase,
						SE_GENI_RX_FIFO_STATUS);
	}
	tx_active = m_cmd_active || tx_fifo_status;
	rx_active =  rx_fifo_status ? true : false;

	if (rx_active || tx_active || !uart_circ_empty(xmit))
		xfer_on = true;

	return xfer_on;
}

static void wait_for_transfers_inflight(struct uart_port *uport)
{
	int iter = 0;
	struct msm_geni_serial_port *port = GET_DEV_PORT(uport);
	unsigned int geni_status;
	bool CTS, RX;

	geni_status = geni_read_reg_nolog(uport->membase, SE_GENI_STATUS);
	/* Possible stop rx is called before this. */
	if (!(geni_status & S_GENI_CMD_ACTIVE))
		return;

	while (iter < WAIT_XFER_MAX_ITER) {
		if (check_transfers_inflight(uport)) {
			usleep_range(WAIT_XFER_MIN_TIMEOUT_US,
					WAIT_XFER_MAX_TIMEOUT_US);
			iter++;
		} else {
			break;
		}
	}
	if (check_transfers_inflight(uport)) {
		u32 geni_status = geni_read_reg_nolog(uport->membase,
								SE_GENI_STATUS);
		u32 geni_ios = geni_read_reg_nolog(uport->membase, SE_GENI_IOS);
		u32 rx_fifo_status = geni_read_reg_nolog(uport->membase,
							SE_GENI_RX_FIFO_STATUS);
		u32 rx_dma =
			geni_read_reg_nolog(uport->membase, SE_DMA_RX_LEN_IN);
		CTS = geni_ios & BIT(1); // b[1] = UART CTS <- Peer RFR
		RX = geni_ios & BIT(0);  // b[0] = UART RX <- Peer TX

		IPC_LOG_MSG(port->ipc_log_misc,
		"%s: geni=0x%x rx_fifo=0x%x rx_dma=0x%x, CTS_IO=%d, RX_IO=%d\n",
		 __func__, geni_status, rx_fifo_status, rx_dma, CTS, RX);
	}
}

static int vote_clock_on(struct uart_port *uport)
{
	struct msm_geni_serial_port *port = GET_DEV_PORT(uport);
	int usage_count;
	int ret = 0;
	u32 geni_ios;

	ret = msm_geni_serial_power_on(uport);
	if (ret) {
		dev_err(uport->dev, "Failed to vote clock on\n");
		return ret;
	}
	port->ioctl_count++;
	usage_count = atomic_read(&uport->dev->power.usage_count);
	geni_ios = geni_read_reg_nolog(uport->membase, SE_GENI_IOS);
	IPC_LOG_MSG(port->ipc_log_pwr,
		"%s :%s ioctl:%d usage_count:%d edge-Count:%d geni_ios:0x%x\n",
		__func__, current->comm, port->ioctl_count,
		usage_count, port->edge_count, geni_ios);
	return 0;
}

static int vote_clock_off(struct uart_port *uport)
{
	struct msm_geni_serial_port *port = GET_DEV_PORT(uport);
	int usage_count;

	if (!pm_runtime_enabled(uport->dev)) {
		dev_err(uport->dev, "RPM not available.Can't enable clocks\n");
		return -EPERM;
	}
	if (!port->ioctl_count) {
		dev_warn(uport->dev, "%s:Imbalanced vote off ioctl %d\n",
						__func__, port->ioctl_count);
		IPC_LOG_MSG(port->ipc_log_pwr,
				"%s:Imbalanced vote_off from userspace. %d",
				__func__, port->ioctl_count);
		return -EPERM;
	}
	wait_for_transfers_inflight(uport);
	port->ioctl_count--;
	msm_geni_serial_power_off(uport);
	usage_count = atomic_read(&uport->dev->power.usage_count);
	IPC_LOG_MSG(port->ipc_log_pwr, "%s:%s ioctl:%d usage_count:%d\n",
		__func__, current->comm, port->ioctl_count, usage_count);
	return 0;
};

static int msm_geni_serial_ioctl(struct uart_port *uport, unsigned int cmd,
						unsigned long arg)
{
	int ret = -ENOIOCTLCMD;
	struct msm_geni_serial_port *port = GET_DEV_PORT(uport);

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
	case TIOCFAULT: {
		geni_se_dump_dbg_regs(&port->serial_rsc,
				uport->membase, port->ipc_log_misc);
		port->ipc_log_rx = port->ipc_log_single;
		port->ipc_log_tx = port->ipc_log_single;
		port->ipc_log_misc = port->ipc_log_single;
		port->ipc_log_pwr = port->ipc_log_single;
		ret = 0;
		break;
	}
	default:
		break;
	}
	return ret;
}

static void msm_geni_serial_break_ctl(struct uart_port *uport, int ctl)
{
	struct msm_geni_serial_port *port = GET_DEV_PORT(uport);

	if (!uart_console(uport) && device_pending_suspend(uport)) {
		IPC_LOG_MSG(port->ipc_log_misc,
				"%s.Device is suspended, %s\n",
				__func__, current->comm);
		return;
	}

	if (ctl) {
		wait_for_transfers_inflight(uport);
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
	struct msm_geni_serial_port *port = GET_DEV_PORT(uport);

	if (!uart_console(uport) && device_pending_suspend(uport)) {
		IPC_LOG_MSG(port->ipc_log_misc,
				"%s.Device is suspended, %s\n",
				__func__, current->comm);
		return TIOCM_DSR | TIOCM_CAR | TIOCM_CTS;
	}

	geni_ios = geni_read_reg_nolog(uport->membase, SE_GENI_IOS);
	if (!(geni_ios & IO2_DATA_IN))
		mctrl |= TIOCM_CTS;
	IPC_LOG_MSG(port->ipc_log_misc, "%s: geni_ios:0x%x, mctrl:0x%x\n",
			__func__, geni_ios, mctrl);
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
	struct msm_geni_serial_port *port = GET_DEV_PORT(uport);

	if (device_pending_suspend(uport)) {
		IPC_LOG_MSG(port->ipc_log_misc,
			"%s.Device is suspended, %s: mctrl=0x%x\n",
			 __func__, current->comm, mctrl);
		return;
	}
	if (!(mctrl & TIOCM_RTS)) {
		uart_manual_rfr |= (UART_MANUAL_RFR_EN | UART_RFR_NOT_READY);
		port->manual_flow = true;
	} else {
		port->manual_flow = false;
	}
	geni_write_reg_nolog(uart_manual_rfr, uport->membase,
							SE_UART_MANUAL_RFR);
	/* Write to flow control must complete before return to client*/
	mb();
	IPC_LOG_MSG(port->ipc_log_misc,
			"%s:%s, mctrl=0x%x, manual_rfr=0x%x, flow=%s\n",
			__func__, current->comm, mctrl, uart_manual_rfr,
			(port->manual_flow ? "OFF" : "ON"));
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
		/* Max 1 port supported as of now */
		if ((line < 0) || (line >= GENI_UART_CONS_PORTS))
			return ERR_PTR(-ENXIO);
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
	struct msm_geni_serial_port *port = GET_DEV_PORT(uport);

	if (!pm_runtime_enabled(uport->dev)) {
		if (pm_runtime_status_suspended(uport->dev)) {
			struct uart_state *state = uport->state;
			struct tty_port *tport = &state->port;
			int lock = mutex_trylock(&tport->mutex);

			IPC_LOG_MSG(port->ipc_log_pwr,
					"%s:Manual resume\n", __func__);
			pm_runtime_disable(uport->dev);
			ret = msm_geni_serial_runtime_resume(uport->dev);
			if (ret) {
				IPC_LOG_MSG(port->ipc_log_pwr,
					"%s:Manual RPM CB failed %d\n",
								__func__, ret);
			} else {
				pm_runtime_get_noresume(uport->dev);
				pm_runtime_set_active(uport->dev);
			}
			pm_runtime_enable(uport->dev);
			if (lock)
				mutex_unlock(&tport->mutex);
		}
	} else {
		ret = pm_runtime_get_sync(uport->dev);
		if (ret < 0) {
			IPC_LOG_MSG(port->ipc_log_pwr, "%s Err\n", __func__);
			WARN_ON_ONCE(1);
			pm_runtime_put_noidle(uport->dev);
			pm_runtime_set_suspended(uport->dev);
			return ret;
		}
	}
	return 0;
}

static void msm_geni_serial_power_off(struct uart_port *uport)
{
	struct msm_geni_serial_port *port = GET_DEV_PORT(uport);
	int usage_count = atomic_read(&uport->dev->power.usage_count);

	if (!usage_count) {
		IPC_LOG_MSG(port->ipc_log_pwr, "%s: Usage Count is already 0\n",
								__func__);
		return;
	}
	pm_runtime_mark_last_busy(uport->dev);
	pm_runtime_put_autosuspend(uport->dev);
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
		total_iter = ((fifo_bits * USEC_PER_SEC) / baud) / 10;
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

static void msm_geni_serial_poll_tx_done(struct uart_port *uport)
{
	int done = 0;
	unsigned int irq_clear = 0;

	done = msm_geni_serial_poll_bit(uport, SE_GENI_M_IRQ_STATUS,
						M_CMD_DONE_EN, true);
	if (!done) {
		/*
		 * Failure IPC logs are not added as this API is
		 * used by early console and it doesn't have log handle.
		 */
		geni_write_reg(M_GENI_CMD_CANCEL, uport->membase,
						SE_GENI_M_CMD_CTRL_REG);
		done = msm_geni_serial_poll_bit(uport, SE_GENI_M_IRQ_STATUS,
						M_CMD_CANCEL_EN, true);
		if (!done) {
			geni_write_reg_nolog(M_GENI_CMD_ABORT, uport->membase,
						SE_GENI_M_CMD_CTRL_REG);
			msm_geni_serial_poll_bit(uport, SE_GENI_M_IRQ_STATUS,
						M_CMD_ABORT_EN, true);
		}
	}

	irq_clear = geni_read_reg_nolog(uport->membase, SE_GENI_M_IRQ_STATUS);
	geni_write_reg_nolog(irq_clear, uport->membase, SE_GENI_M_IRQ_CLEAR);
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
	msm_serial_try_disable_interrupts(uport);
	msm_geni_serial_poll_tx_done(uport);
	msm_geni_serial_enable_interrupts(uport);
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
	msm_serial_try_disable_interrupts(uport);
	msm_geni_serial_poll_tx_done(uport);
	msm_geni_serial_enable_interrupts(uport);
}

static void msm_geni_serial_console_write(struct console *co, const char *s,
			      unsigned int count)
{
	struct uart_port *uport;
	struct msm_geni_serial_port *port;
	bool locked = true;
	unsigned long flags;
	unsigned int geni_status;
	bool timeout;
	bool is_irq_masked;
	int irq_en;

	/* Max 1 port supported as of now */
	WARN_ON(co->index < 0 || co->index >= GENI_UART_CONS_PORTS);

	port = get_port_from_line(co->index, true);
	if (IS_ERR_OR_NULL(port))
		return;

	uport = &port->uport;
	if (oops_in_progress)
		locked = spin_trylock_irqsave(&uport->lock, flags);
	else
		spin_lock_irqsave(&uport->lock, flags);

	geni_status = readl_relaxed(uport->membase + SE_GENI_STATUS);

	/* Cancel the current write to log the fault */
	if ((geni_status & M_GENI_CMD_ACTIVE) && !locked) {
		port->m_cmd_done = false;
		port->m_cmd = true;
		reinit_completion(&port->m_cmd_timeout);
		is_irq_masked = msm_serial_try_disable_interrupts(uport);
		geni_cancel_m_cmd(uport->membase);

		/*
		 * console should be in polling mode. Hence directly pass true
		 * as argument for wait_for_cmd_done here to handle cancel tx
		 * in polling mode.
		 */
		timeout = geni_wait_for_cmd_done(uport, true);
		if (timeout) {
			IPC_LOG_MSG(port->console_log,
				"%s: tx_cancel failed 0x%x\n",
				__func__, geni_read_reg_nolog(uport->membase,
							SE_GENI_STATUS));

			reinit_completion(&port->m_cmd_timeout);
			geni_abort_m_cmd(uport->membase);
			timeout = geni_wait_for_cmd_done(uport, true);
			if (timeout)
				IPC_LOG_MSG(port->console_log,
				"%s: tx abort failed 0x%x\n", __func__,
				geni_read_reg_nolog(uport->membase,
							SE_GENI_STATUS));
		}

		msm_geni_serial_enable_interrupts(uport);
		port->m_cmd = false;
	} else if ((geni_status & M_GENI_CMD_ACTIVE) &&
						!port->cur_tx_remaining) {
		/* It seems we can interrupt existing transfers unless all data
		 * has been sent, in which case we need to look for done first.
		 */
		msm_serial_try_disable_interrupts(uport);
		msm_geni_serial_poll_tx_done(uport);
		msm_geni_serial_enable_interrupts(uport);

		/* Enable WM interrupt for every new console write op */
		if (uart_circ_chars_pending(&uport->state->xmit)) {
			irq_en = geni_read_reg_nolog(uport->membase,
						SE_GENI_M_IRQ_EN);
			geni_write_reg_nolog(irq_en | M_TX_FIFO_WATERMARK_EN,
					uport->membase, SE_GENI_M_IRQ_EN);
		}
	}

	__msm_geni_serial_console_write(uport, s, count);

	if (port->cur_tx_remaining)
		msm_geni_serial_setup_tx(uport, port->cur_tx_remaining);

	if (locked)
		spin_unlock_irqrestore(&uport->lock, flags);
}

static int handle_rx_console(struct uart_port *uport,
			unsigned int rx_fifo_wc,
			unsigned int rx_last_byte_valid,
			unsigned int rx_last,
			bool drop_rx)
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
		if (drop_rx)
			continue;
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
	return 0;
}
#else
static int handle_rx_console(struct uart_port *uport,
			unsigned int rx_fifo_wc,
			unsigned int rx_last_byte_valid,
			unsigned int rx_last,
			bool drop_rx)
{
	return -EPERM;
}

#endif /* (CONFIG_SERIAL_CORE_CONSOLE) || defined(CONFIG_CONSOLE_POLL)) */

static int msm_geni_serial_prep_dma_tx(struct uart_port *uport)
{
	struct msm_geni_serial_port *msm_port = GET_DEV_PORT(uport);
	struct circ_buf *xmit = &uport->state->xmit;
	unsigned int xmit_size;
	unsigned int dma_dbg;
	bool timeout, is_irq_masked;
	int ret = 0;

	xmit_size = uart_circ_chars_pending(xmit);
	if (xmit_size < WAKEUP_CHARS)
		uart_write_wakeup(uport);

	if (xmit_size > (UART_XMIT_SIZE - xmit->tail))
		xmit_size = UART_XMIT_SIZE - xmit->tail;

	if (!xmit_size)
		return ret;

	dump_ipc(msm_port->ipc_log_tx, "DMA Tx",
		 (char *)&xmit->buf[xmit->tail], 0, xmit_size);
	msm_geni_serial_setup_tx(uport, xmit_size);
	ret = geni_se_tx_dma_prep(msm_port->wrapper_dev, uport->membase,
			&xmit->buf[xmit->tail], xmit_size, &msm_port->tx_dma);

	if (!ret) {
		msm_port->xmit_size = xmit_size;
	} else {
		IPC_LOG_MSG(msm_port->ipc_log_misc,
		    "%s: TX DMA map Fail %d\n", __func__, ret);

		geni_write_reg_nolog(0, uport->membase, SE_UART_TX_TRANS_LEN);
		msm_port->m_cmd_done = false;
		msm_port->m_cmd = true;
		reinit_completion(&msm_port->m_cmd_timeout);

		/*
		 * Try disabling interrupts before giving the
		 * cancel command as this might be in an atomic context.
		 */
		is_irq_masked = msm_serial_try_disable_interrupts(uport);
		geni_cancel_m_cmd(uport->membase);

		timeout = geni_wait_for_cmd_done(uport, is_irq_masked);
		if (timeout) {
			IPC_LOG_MSG(msm_port->console_log,
			"%s: tx_cancel fail 0x%x\n", __func__,
			geni_read_reg_nolog(uport->membase, SE_GENI_STATUS));

			IPC_LOG_MSG(msm_port->ipc_log_misc,
			"%s: tx_cancel failed 0x%x\n", __func__,
			geni_read_reg_nolog(uport->membase, SE_GENI_STATUS));

			msm_port->m_cmd_done = false;
			reinit_completion(&msm_port->m_cmd_timeout);
			/* Give abort command as cancel command failed */
			geni_abort_m_cmd(uport->membase);

			timeout = geni_wait_for_cmd_done(uport,
							 is_irq_masked);
			if (timeout) {
				IPC_LOG_MSG(msm_port->console_log,
				"%s: tx abort failed 0x%x\n", __func__,
				geni_read_reg_nolog(uport->membase,
							SE_GENI_STATUS));
				IPC_LOG_MSG(msm_port->ipc_log_misc,
				"%s: tx abort failed 0x%x\n", __func__,
				geni_read_reg_nolog(uport->membase,
							SE_GENI_STATUS));
			}
		}

		if (msm_port->xfer_mode == SE_DMA) {
			dma_dbg = geni_read_reg(uport->membase,
							SE_DMA_DEBUG_REG0);
			if (dma_dbg & DMA_TX_ACTIVE) {
				msm_port->m_cmd_done = false;
				reinit_completion(&msm_port->m_cmd_timeout);
				geni_write_reg_nolog(1, uport->membase,
						SE_DMA_TX_FSM_RST);

				timeout = geni_wait_for_cmd_done(uport,
							is_irq_masked);
				if (timeout)
					IPC_LOG_MSG(msm_port->ipc_log_misc,
					"%s: tx fsm reset failed\n", __func__);
			}

			if (msm_port->tx_dma) {
				geni_se_tx_dma_unprep(msm_port->wrapper_dev,
					msm_port->tx_dma, msm_port->xmit_size);
				msm_port->tx_dma = (dma_addr_t)NULL;
			}
		}
		msm_port->xmit_size = 0;
		/* Enable the interrupts once the cancel operation is done. */
		msm_geni_serial_enable_interrupts(uport);
		msm_port->m_cmd = false;
	}

	return ret;
}

static void msm_geni_serial_start_tx(struct uart_port *uport)
{
	unsigned int geni_m_irq_en;
	struct msm_geni_serial_port *msm_port = GET_DEV_PORT(uport);
	unsigned int geni_status;
	unsigned int geni_ios;
	static unsigned int ios_log_limit;

	if (!uart_console(uport) && !pm_runtime_active(uport->dev)) {
		IPC_LOG_MSG(msm_port->ipc_log_misc,
				"%s.Putting in async RPM vote\n", __func__);
		pm_runtime_get(uport->dev);
		goto exit_start_tx;
	}

	if (!uart_console(uport)) {
		IPC_LOG_MSG(msm_port->ipc_log_misc,
				"%s.Power on.\n", __func__);
		pm_runtime_get(uport->dev);
	}

	if (msm_port->xfer_mode == FIFO_MODE) {
		geni_status = geni_read_reg_nolog(uport->membase,
						  SE_GENI_STATUS);
		if (geni_status & M_GENI_CMD_ACTIVE)
			goto check_flow_ctrl;

		if (!msm_geni_serial_tx_empty(uport))
			goto check_flow_ctrl;

		geni_m_irq_en = geni_read_reg_nolog(uport->membase,
						    SE_GENI_M_IRQ_EN);
		geni_m_irq_en |= (M_TX_FIFO_WATERMARK_EN | M_CMD_DONE_EN);

		geni_write_reg_nolog(msm_port->tx_wm, uport->membase,
						SE_GENI_TX_WATERMARK_REG);
		geni_write_reg_nolog(geni_m_irq_en, uport->membase,
							SE_GENI_M_IRQ_EN);
		/* Geni command setup should complete before returning.*/
		mb();
	} else if (msm_port->xfer_mode == SE_DMA) {
		if (msm_port->tx_dma)
			goto check_flow_ctrl;

		msm_geni_serial_prep_dma_tx(uport);
	}
	return;
check_flow_ctrl:
	geni_ios = geni_read_reg_nolog(uport->membase, SE_GENI_IOS);
	if (++ios_log_limit % 5 == 0) {
		IPC_LOG_MSG(msm_port->ipc_log_misc, "%s: ios: 0x%08x\n",
						__func__, geni_ios);
		ios_log_limit = 0;
	}
exit_start_tx:
	if (!uart_console(uport))
		msm_geni_serial_power_off(uport);
}

static void stop_tx_sequencer(struct uart_port *uport)
{
	unsigned int geni_status;
	bool timeout, is_irq_masked;
	unsigned int dma_dbg;
	struct msm_geni_serial_port *port = GET_DEV_PORT(uport);

	geni_status = geni_read_reg_nolog(uport->membase, SE_GENI_STATUS);
	/* Possible stop tx is called multiple times. */
	if (!(geni_status & M_GENI_CMD_ACTIVE))
		return;

	IPC_LOG_MSG(port->ipc_log_misc,
		    "%s: Start GENI: 0x%x\n", __func__, geni_status);

	port->m_cmd_done = false;
	port->m_cmd = true;
	reinit_completion(&port->m_cmd_timeout);
	/*
	 * Try to mask the interrupts before giving the
	 * cancel command as this might be in an atomic context
	 * from framework driver.
	 */
	is_irq_masked = msm_serial_try_disable_interrupts(uport);
	geni_cancel_m_cmd(uport->membase);

	timeout = geni_wait_for_cmd_done(uport, is_irq_masked);
	if (timeout) {
		IPC_LOG_MSG(port->console_log, "%s: tx_cancel failed 0x%x\n",
		__func__, geni_read_reg_nolog(uport->membase, SE_GENI_STATUS));
		IPC_LOG_MSG(port->ipc_log_misc, "%s: tx_cancel failed 0x%x\n",
		__func__, geni_read_reg_nolog(uport->membase, SE_GENI_STATUS));

		port->m_cmd_done = false;
		reinit_completion(&port->m_cmd_timeout);
		geni_abort_m_cmd(uport->membase);

		timeout = geni_wait_for_cmd_done(uport, is_irq_masked);
		if (timeout) {
			IPC_LOG_MSG(port->console_log,
				"%s: tx abort failed 0x%x\n", __func__,
			geni_read_reg_nolog(uport->membase, SE_GENI_STATUS));
			IPC_LOG_MSG(port->ipc_log_misc,
				"%s: tx abort failed 0x%x\n", __func__,
			geni_read_reg_nolog(uport->membase, SE_GENI_STATUS));
		}
	}

	if (port->xfer_mode == SE_DMA) {
		dma_dbg = geni_read_reg(uport->membase, SE_DMA_DEBUG_REG0);
		if (dma_dbg & DMA_TX_ACTIVE) {
			port->m_cmd_done = false;
			reinit_completion(&port->m_cmd_timeout);
			geni_write_reg_nolog(1, uport->membase,
						SE_DMA_TX_FSM_RST);

			timeout = geni_wait_for_cmd_done(uport,
							 is_irq_masked);
			if (timeout)
				IPC_LOG_MSG(port->ipc_log_misc,
				"%s: tx fsm reset failed\n", __func__);
		}

		if (port->tx_dma) {
			geni_se_tx_dma_unprep(port->wrapper_dev,
					port->tx_dma, port->xmit_size);
			port->tx_dma = (dma_addr_t)NULL;
		}
	}
	/* Unmask the interrupts once the cancel operation is done. */
	msm_geni_serial_enable_interrupts(uport);
	port->m_cmd = false;
	port->xmit_size = 0;

	/*
	 * If we end up having to cancel an on-going Tx for non-console usecase
	 * then it means there was some unsent data in the Tx FIFO, consequently
	 * it means that there is a vote imbalance as we put in a vote during
	 * start_tx() that is removed only as part of a "done" ISR. To balance
	 * this out, remove the vote put in during start_tx().
	 */
	if (!uart_console(uport)) {
		IPC_LOG_MSG(port->ipc_log_misc, "%s:Removing vote\n", __func__);
		msm_geni_serial_power_off(uport);
	}

	geni_status = geni_read_reg_nolog(uport->membase, SE_GENI_STATUS);
	IPC_LOG_MSG(port->ipc_log_misc, "%s: End GENI:0x%x\n",
		    __func__, geni_status);
}

static void msm_geni_serial_stop_tx(struct uart_port *uport)
{
	struct msm_geni_serial_port *port = GET_DEV_PORT(uport);

	if (!uart_console(uport) && device_pending_suspend(uport)) {
		dev_err(uport->dev, "%s.Device is suspended.\n", __func__);
		IPC_LOG_MSG(port->ipc_log_misc,
				"%s.Device is suspended.\n", __func__);
		return;
	}
	stop_tx_sequencer(uport);
}

static void start_rx_sequencer(struct uart_port *uport)
{
	unsigned int geni_status;
	struct msm_geni_serial_port *port = GET_DEV_PORT(uport);
	u32 geni_se_param = UART_PARAM_RFR_OPEN;

	if (port->startup_in_progress)
		return;

	geni_status = geni_read_reg_nolog(uport->membase, SE_GENI_STATUS);
	IPC_LOG_MSG(port->ipc_log_misc, "%s: 0x%x\n",
		    __func__, geni_status);

	if (geni_status & S_GENI_CMD_ACTIVE) {
		if (port->xfer_mode == SE_DMA) {
			IPC_LOG_MSG(port->ipc_log_misc,
				"%s: GENI: 0x%x\n", __func__, geni_status);
			geni_se_rx_dma_start(uport->membase, DMA_RX_BUF_SIZE,
								&port->rx_dma);
		}
		msm_geni_serial_stop_rx(uport);
	}

	if (port->xfer_mode == SE_DMA)
		geni_se_rx_dma_start(uport->membase, DMA_RX_BUF_SIZE,
							&port->rx_dma);

	/* Start RX with the RFR_OPEN to keep RFR in always ready state */
	geni_setup_s_cmd(uport->membase, UART_START_READ, geni_se_param);
	msm_geni_serial_enable_interrupts(uport);

	/* Ensure that the above writes go through */
	mb();
	geni_status = geni_read_reg_nolog(uport->membase, SE_GENI_STATUS);
	IPC_LOG_MSG(port->ipc_log_misc, "%s: 0x%x, dma_dbg:0x%x\n", __func__,
		geni_status, geni_read_reg(uport->membase, SE_DMA_DEBUG_REG0));
}

static void msm_geni_serial_start_rx(struct uart_port *uport)
{
	struct msm_geni_serial_port *port = GET_DEV_PORT(uport);

	if (!uart_console(uport) && device_pending_suspend(uport)) {
		dev_err(uport->dev, "%s.Device is suspended.\n", __func__);
		IPC_LOG_MSG(port->ipc_log_misc,
				"%s.Device is suspended.\n", __func__);
		return;
	}
	start_rx_sequencer(&port->uport);
}

static void msm_geni_serial_set_manual_flow(bool enable,
					struct msm_geni_serial_port *port)
{
	u32 uart_manual_rfr = 0;

	if (!enable) {
		uart_manual_rfr |= (UART_MANUAL_RFR_EN);
		geni_write_reg_nolog(uart_manual_rfr, port->uport.membase,
						SE_UART_MANUAL_RFR);
		/* UART FW needs delay per HW experts recommendation */
		udelay(10);

		uart_manual_rfr |= (UART_RFR_NOT_READY);
		geni_write_reg_nolog(uart_manual_rfr, port->uport.membase,
						SE_UART_MANUAL_RFR);
		/*
		 * Ensure that the manual flow on writes go through before
		 * doing a stop_rx.
		 */
		mb();
		IPC_LOG_MSG(port->ipc_log_misc,
			"%s: Manual Flow Enabled, HW Flow OFF\n", __func__);
	} else {
		geni_write_reg_nolog(0, port->uport.membase,
						SE_UART_MANUAL_RFR);
		/* Ensure that the manual flow off writes go through */
		mb();
		uart_manual_rfr = geni_read_reg_nolog(port->uport.membase,
							SE_UART_MANUAL_RFR);
		IPC_LOG_MSG(port->ipc_log_misc,
			"%s: Manual Flow Disabled, HW Flow ON rfr = 0x%x\n",
						__func__, uart_manual_rfr);
	}
}

static int stop_rx_sequencer(struct uart_port *uport)
{
	unsigned int geni_status;
	bool timeout, is_irq_masked;
	struct msm_geni_serial_port *port = GET_DEV_PORT(uport);
	unsigned long flags = 0;
	bool is_rx_active;
	unsigned int stale_delay;
	u32 dma_rx_status, s_irq_status;

	IPC_LOG_MSG(port->ipc_log_misc, "%s\n", __func__);

	geni_status = geni_read_reg_nolog(uport->membase, SE_GENI_STATUS);
	/* Possible stop rx is called multiple times. */
	if (!(geni_status & S_GENI_CMD_ACTIVE)) {
		IPC_LOG_MSG(port->ipc_log_misc,
			"%s: RX is Inactive, geni_sts: 0x%x\n",
						__func__, geni_status);
		return 0;
	}

	if (!uart_console(uport)) {
		msm_geni_serial_set_manual_flow(false, port);
		/*
		 * Wait for the stale timeout to happen if there
		 * is any data pending in the rx fifo.
		 * Have a safety factor of 2 to include the interrupt
		 * and system latencies, add 500usec delay for interrupt
		 * latency or system delay.
		 */
		stale_delay = (STALE_COUNT * SEC_TO_USEC) / port->cur_baud;
		stale_delay = (2 * stale_delay) + SYSTEM_DELAY;
		udelay(stale_delay);

		dma_rx_status = geni_read_reg_nolog(uport->membase,
						SE_DMA_RX_IRQ_STAT);
		/* The transfer is completed at HW level and the completion
		 * interrupt is delayed. So process the transfer completion
		 * before issuing the cancel command to resolve the race
		 * btw cancel RX and completion interrupt.
		 */
		if (dma_rx_status) {
			s_irq_status = geni_read_reg_nolog(uport->membase,
							SE_GENI_S_IRQ_STATUS);
			geni_write_reg_nolog(s_irq_status, uport->membase,
							SE_GENI_S_IRQ_CLEAR);
			geni_se_dump_dbg_regs(&port->serial_rsc,
				uport->membase, port->ipc_log_misc);
			IPC_LOG_MSG(port->ipc_log_misc, "%s: Interrupt delay\n",
					 __func__);
			handle_rx_dma_xfer(s_irq_status, uport);
		}
	}

	IPC_LOG_MSG(port->ipc_log_misc, "%s: Start 0x%x\n",
		    __func__, geni_status);
	/*
	 * Try disabling interrupts before giving the
	 * cancel command as this might be in an atomic context.
	 */
	is_irq_masked = msm_serial_try_disable_interrupts(uport);

	port->s_cmd_done = false;
	port->s_cmd = true;
	reinit_completion(&port->s_cmd_timeout);

	geni_cancel_s_cmd(uport->membase);

	/*
	 * Ensure that the cancel goes through before polling for the
	 * cancel control bit.
	 */
	mb();
	timeout = geni_wait_for_cmd_done(uport, is_irq_masked);
	geni_status = geni_read_reg_nolog(uport->membase,
							SE_GENI_STATUS);
	is_rx_active = geni_status & S_GENI_CMD_ACTIVE;
	IPC_LOG_MSG(port->ipc_log_misc, "%s: 0x%x, dma_dbg:0x%x\n", __func__,
		geni_status, geni_read_reg(uport->membase, SE_DMA_DEBUG_REG0));
	if (timeout || is_rx_active) {
		IPC_LOG_MSG(port->ipc_log_misc,
			    "%s cancel failed timeout:%d is_rx_active:%d 0x%x\n",
			    __func__, timeout, is_rx_active, geni_status);
		IPC_LOG_MSG(port->console_log,
			    "%s cancel failed timeout:%d is_rx_active:%d 0x%x\n",
			    __func__, timeout, is_rx_active, geni_status);
		geni_se_dump_dbg_regs(&port->serial_rsc,
				uport->membase, port->ipc_log_misc);
		/*
		 * Possible that stop_rx is called from system resume context
		 * for console usecase. In early resume, irq remains disabled
		 * in the system. call msm_geni_serial_handle_isr to clear
		 * the interrupts.
		 */
		if (uart_console(uport) && !is_rx_active) {
			msm_geni_serial_handle_isr(uport, &flags, true);
			goto exit_rx_seq;
		}
		port->s_cmd_done = false;
		reinit_completion(&port->s_cmd_timeout);
		geni_abort_s_cmd(uport->membase);
		/* Ensure this goes through before polling. */
		mb();

		timeout = geni_wait_for_cmd_done(uport, is_irq_masked);
		geni_status = geni_read_reg_nolog(uport->membase,
							SE_GENI_STATUS);
		is_rx_active = geni_status & S_GENI_CMD_ACTIVE;
		if (timeout || is_rx_active) {
			geni_status = geni_read_reg_nolog(uport->membase,
							SE_GENI_STATUS);
			IPC_LOG_MSG(port->ipc_log_misc,
				"%s abort fail timeout:%d is_rx_active:%d 0x%x\n",
				__func__, timeout, is_rx_active, geni_status);
			IPC_LOG_MSG(port->console_log,
				"%s abort fail timeout:%d is_rx_active:%d 0x%x\n",
				 __func__, timeout, is_rx_active, geni_status);
			geni_se_dump_dbg_regs(&port->serial_rsc,
				uport->membase, port->ipc_log_misc);
		}

		if (port->xfer_mode == SE_DMA) {
			port->s_cmd_done = false;
			reinit_completion(&port->s_cmd_timeout);
			geni_write_reg_nolog(1, uport->membase,
						SE_DMA_RX_FSM_RST);

			timeout = geni_wait_for_cmd_done(uport,
							 is_irq_masked);
			if (timeout)
				IPC_LOG_MSG(port->ipc_log_misc,
				"%s: rx fsm reset failed\n", __func__);
		}
	}
	/* Enable the interrupts once the cancel operation is done. */
	msm_geni_serial_enable_interrupts(uport);
	port->s_cmd = false;

exit_rx_seq:
	if (!uart_console(uport))
		msm_geni_serial_set_manual_flow(true, port);

	geni_status = geni_read_reg_nolog(uport->membase, SE_GENI_STATUS);
	IPC_LOG_MSG(port->ipc_log_misc, "%s: End 0x%x dma_dbg:0x%x\n",
		    __func__, geni_status,
		    geni_read_reg(uport->membase, SE_DMA_DEBUG_REG0));

	is_rx_active = geni_status & S_GENI_CMD_ACTIVE;
	if (is_rx_active)
		return -EBUSY;
	else
		return 0;
}

static void msm_geni_serial_stop_rx(struct uart_port *uport)
{
	struct msm_geni_serial_port *port = GET_DEV_PORT(uport);
	int ret;

	if (!uart_console(uport) && device_pending_suspend(uport)) {
		IPC_LOG_MSG(port->ipc_log_misc,
				"%s.Device is suspended.\n", __func__);
		return;
	}
	ret = stop_rx_sequencer(uport);
	if (ret)
		IPC_LOG_MSG(port->ipc_log_misc, "%s: stop rx failed %d\n",
							__func__, ret);
}

static int handle_rx_hs(struct uart_port *uport,
			unsigned int rx_fifo_wc,
			unsigned int rx_last_byte_valid,
			unsigned int rx_last,
			bool drop_rx)
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
	if (drop_rx)
		return 0;

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

static int msm_geni_serial_handle_rx(struct uart_port *uport, bool drop_rx)
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
		ret = port->handle_rx(uport, rx_fifo_wc, rx_last_byte_valid,
						rx_last, drop_rx);
	return ret;
}

static int msm_geni_serial_handle_tx(struct uart_port *uport, bool done,
		bool active)
{
	struct msm_geni_serial_port *msm_port = GET_DEV_PORT(uport);
	struct circ_buf *xmit = &uport->state->xmit;
	int avail_fifo_bytes = 0;
	unsigned int bytes_remaining = 0;
	unsigned int pending;
	int i = 0;
	unsigned int tx_fifo_status;
	unsigned int xmit_size;
	unsigned int fifo_width_bytes =
		(uart_console(uport) ? 1 : (msm_port->tx_fifo_width >> 3));
	int temp_tail = 0;
	int irq_en;

	tx_fifo_status = geni_read_reg_nolog(uport->membase,
					SE_GENI_TX_FIFO_STATUS);

	/* Complete the current tx command before taking newly added data */
	if (active)
		pending = msm_port->cur_tx_remaining;
	else
		pending = uart_circ_chars_pending(xmit);

	/* All data has been transmitted and acknowledged as received */
	if (!pending && !tx_fifo_status && done) {
		msm_geni_serial_stop_tx(uport);
		goto exit_handle_tx;
	}

	avail_fifo_bytes = msm_port->tx_fifo_depth - (tx_fifo_status &
								TX_FIFO_WC);
	avail_fifo_bytes *= fifo_width_bytes;
	if (avail_fifo_bytes < 0)
		avail_fifo_bytes = 0;

	temp_tail = xmit->tail;
	xmit_size = min_t(unsigned int, avail_fifo_bytes, pending);
	if (!xmit_size)
		goto exit_handle_tx;

	if (!msm_port->cur_tx_remaining) {
		msm_geni_serial_setup_tx(uport, pending);
		msm_port->cur_tx_remaining = pending;

		/* Re-enable WM interrupt when starting new transfer */
		irq_en = geni_read_reg_nolog(uport->membase, SE_GENI_M_IRQ_EN);
		if (!(irq_en & M_TX_FIFO_WATERMARK_EN))
			geni_write_reg_nolog(irq_en | M_TX_FIFO_WATERMARK_EN,
					uport->membase, SE_GENI_M_IRQ_EN);
	}

	bytes_remaining = xmit_size;

	while (i < xmit_size) {
		unsigned int tx_bytes;
		unsigned int buf = 0;
		int c;

		tx_bytes = ((bytes_remaining < fifo_width_bytes) ?
					bytes_remaining : fifo_width_bytes);

		for (c = 0; c < tx_bytes ; c++) {
			buf |= (xmit->buf[temp_tail++] << (c * 8));
			temp_tail &= UART_XMIT_SIZE - 1;
		}

		geni_write_reg_nolog(buf, uport->membase, SE_GENI_TX_FIFOn);

		i += tx_bytes;
		bytes_remaining -= tx_bytes;
		uport->icount.tx += tx_bytes;
		msm_port->cur_tx_remaining -= tx_bytes;
		/* Ensure FIFO write goes through */
		wmb();
	}
	xmit->tail = temp_tail;

	/*
	 * The tx fifo watermark is level triggered and latched. Though we had
	 * cleared it in qcom_geni_serial_isr it will have already reasserted
	 * so we must clear it again here after our writes.
	 */
	geni_write_reg_nolog(M_TX_FIFO_WATERMARK_EN, uport->membase,
						SE_GENI_M_IRQ_CLEAR);

exit_handle_tx:
	if (!msm_port->cur_tx_remaining) {
		irq_en = geni_read_reg_nolog(uport->membase, SE_GENI_M_IRQ_EN);
		/* Clear WM interrupt post each transfer completion */
		if (irq_en & M_TX_FIFO_WATERMARK_EN)
			geni_write_reg_nolog(irq_en & ~M_TX_FIFO_WATERMARK_EN,
					uport->membase, SE_GENI_M_IRQ_EN);
	}

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(uport);
	return 0;
}

static void check_rx_buf(char *buf, struct uart_port *uport, int size)
{
	struct msm_geni_serial_port *msm_port = GET_DEV_PORT(uport);
	unsigned int rx_data;
	bool fault = false;

	rx_data = *(u32 *)buf;
	/* check for first 4 bytes of RX data for faulty zero pattern */
	if (rx_data == 0x0) {
		if (size <= 4)
			fault = true;
		else {
			/*
			 * check for last 4 bytes of data in RX buffer for
			 * faulty pattern
			 */
			if (memcmp(buf+(size-4), "\x0\x0\x0\x0", 4) == 0)
				fault = true;
		}

		if (fault) {
			IPC_LOG_MSG(msm_port->ipc_log_rx,
				"RX Invalid packet %s\n", __func__);
			geni_se_dump_dbg_regs(&msm_port->serial_rsc,
				uport->membase, msm_port->ipc_log_misc);
			/*
			 * Add 2 msecs delay in order for dma rx transfer
			 * to be actually completed.
			 */
			udelay(2000);
		}
	}
}

static int msm_geni_serial_handle_dma_rx(struct uart_port *uport, bool drop_rx)
{
	struct msm_geni_serial_port *msm_port = GET_DEV_PORT(uport);
	unsigned int rx_bytes = 0;
	struct tty_port *tport;
	int ret = 0;
	unsigned int geni_status;

	geni_status = geni_read_reg_nolog(uport->membase, SE_GENI_STATUS);
	/* Possible stop rx is called */
	if (!(geni_status & S_GENI_CMD_ACTIVE)) {
		IPC_LOG_MSG(msm_port->ipc_log_misc,
			    "%s: GENI: 0x%x\n", __func__, geni_status);
		return 0;
	}

	if (unlikely(!msm_port->rx_buf)) {
		IPC_LOG_MSG(msm_port->ipc_log_rx, "%s: NULL Rx_buf\n",
								__func__);
		return 0;
	}

	rx_bytes = geni_read_reg_nolog(uport->membase, SE_DMA_RX_LEN_IN);
	if (unlikely(!rx_bytes)) {
		IPC_LOG_MSG(msm_port->ipc_log_rx, "%s: Size %d\n",
					__func__, rx_bytes);
		goto exit_handle_dma_rx;
	}

	/* Check RX buffer data for faulty pattern*/
	check_rx_buf((char *)msm_port->rx_buf, uport, rx_bytes);

	if (drop_rx)
		goto exit_handle_dma_rx;

	tport = &uport->state->port;
	ret = tty_insert_flip_string(tport, (unsigned char *)(msm_port->rx_buf),
				     rx_bytes);
	if (ret != rx_bytes) {
		dev_err(uport->dev, "%s: ret %d rx_bytes %d\n", __func__,
								ret, rx_bytes);
		WARN_ON(1);
	}
	uport->icount.rx += ret;
	tty_flip_buffer_push(tport);
	dump_ipc(msm_port->ipc_log_rx, "DMA Rx", (char *)msm_port->rx_buf, 0,
								rx_bytes);

	/*
	 * DMA_DONE interrupt doesn't confirm that the DATA is copied to
	 * DDR memory, sometimes we are queuing the stale data from previous
	 * transfer to tty flip_buffer, adding memset to zero
	 * change to idenetify such scenario.
	 */
	memset(msm_port->rx_buf, 0, rx_bytes);
exit_handle_dma_rx:

	return ret;
}

static int msm_geni_serial_handle_dma_tx(struct uart_port *uport)
{
	struct msm_geni_serial_port *msm_port = GET_DEV_PORT(uport);
	struct circ_buf *xmit = &uport->state->xmit;

	xmit->tail = (xmit->tail + msm_port->xmit_size) & (UART_XMIT_SIZE - 1);
	geni_se_tx_dma_unprep(msm_port->wrapper_dev, msm_port->tx_dma,
				msm_port->xmit_size);
	uport->icount.tx += msm_port->xmit_size;
	msm_port->tx_dma = (dma_addr_t)NULL;
	msm_port->xmit_size = 0;

	if (!uart_circ_empty(xmit))
		msm_geni_serial_prep_dma_tx(uport);
	else {
		/*
		 * This will balance out the power vote put in during start_tx
		 * allowing the device to suspend.
		 */
		if (!uart_console(uport)) {
			IPC_LOG_MSG(msm_port->ipc_log_misc,
				"%s.Tx sent out, Power off\n", __func__);
			msm_geni_serial_power_off(uport);
		}
		uart_write_wakeup(uport);
	}
	return 0;
}

static bool handle_rx_dma_xfer(u32 s_irq_status, struct uart_port *uport)
{
	bool ret = false;
	bool drop_rx = false;
	struct msm_geni_serial_port *msm_port = GET_DEV_PORT(uport);
	u32 dma_rx_status;

	spin_lock(&msm_port->rx_lock);
	dma_rx_status = geni_read_reg_nolog(uport->membase,
						SE_DMA_RX_IRQ_STAT);

	if (dma_rx_status) {
		geni_write_reg_nolog(dma_rx_status, uport->membase,
					SE_DMA_RX_IRQ_CLR);

		if (dma_rx_status & RX_RESET_DONE) {
			IPC_LOG_MSG(msm_port->ipc_log_misc,
			"%s.Reset done.  0x%x.\n", __func__, dma_rx_status);
			ret = true;
			goto exit;
		}

		if (dma_rx_status & UART_DMA_RX_ERRS) {
			if (dma_rx_status & UART_DMA_RX_PARITY_ERR)
				uport->icount.parity++;
			IPC_LOG_MSG(msm_port->ipc_log_misc,
				"%s.Rx Errors.  0x%x parity:%d\n",
					__func__, dma_rx_status,
					uport->icount.parity);
			drop_rx = true;
		} else if (dma_rx_status & UART_DMA_RX_BREAK) {
			uport->icount.brk++;
			IPC_LOG_MSG(msm_port->ipc_log_misc,
				"%s.Rx Errors.  0x%x break:%d\n",
				__func__, dma_rx_status,
				uport->icount.brk);
		}

		if (dma_rx_status & RX_EOT ||
				dma_rx_status & RX_DMA_DONE) {
			msm_geni_serial_handle_dma_rx(uport,
						drop_rx);
			if (!(dma_rx_status & RX_GENI_CANCEL_IRQ)) {
				geni_se_rx_dma_start(uport->membase,
				DMA_RX_BUF_SIZE, &msm_port->rx_dma);
			} else {
				IPC_LOG_MSG(msm_port->ipc_log_misc,
				"%s. not mapping rx dma\n",
				__func__);
			}
		}
		if (dma_rx_status & RX_SBE) {
			IPC_LOG_MSG(msm_port->ipc_log_misc,
				"%s.Rx Errors.  0x%x\n",
				__func__, dma_rx_status);
			WARN_ON(1);
		}

		if (dma_rx_status & (RX_EOT | RX_GENI_CANCEL_IRQ | RX_DMA_DONE))
			ret = true;

	}

	if (s_irq_status & (S_CMD_CANCEL_EN | S_CMD_ABORT_EN))
		ret = true;

exit:
	spin_unlock(&msm_port->rx_lock);
	return ret;
}

static void msm_geni_serial_handle_isr(struct uart_port *uport,
				       unsigned long *flags,
				       bool is_irq_masked)
{
	unsigned int m_irq_status;
	unsigned int s_irq_status;
	unsigned int dma;
	unsigned int dma_tx_status;
	unsigned int dma_rx_status;
	unsigned int m_irq_en;
	unsigned int geni_status;
	struct msm_geni_serial_port *msm_port = GET_DEV_PORT(uport);
	struct tty_port *tport = &uport->state->port;
	bool drop_rx = false;
	bool s_cmd_done = false;
	bool m_cmd_done = false;

	if (uart_console(uport) && uport->suspended) {
		IPC_LOG_MSG(msm_port->console_log,
			"%s. Console in suspend state\n", __func__);
		goto exit_geni_serial_isr;
	}

	m_irq_status = geni_read_reg_nolog(uport->membase,
						SE_GENI_M_IRQ_STATUS);
	s_irq_status = geni_read_reg_nolog(uport->membase,
						SE_GENI_S_IRQ_STATUS);
	if (uart_console(uport))
		IPC_LOG_MSG(msm_port->console_log,
			"%s. sirq 0x%x mirq:0x%x\n", __func__, s_irq_status,
							m_irq_status);

	geni_write_reg_nolog(m_irq_status, uport->membase,
						SE_GENI_M_IRQ_CLEAR);
	geni_write_reg_nolog(s_irq_status, uport->membase,
						SE_GENI_S_IRQ_CLEAR);
	m_irq_en = geni_read_reg_nolog(uport->membase, SE_GENI_M_IRQ_EN);
	if ((m_irq_status & M_ILLEGAL_CMD_EN)) {
		if (uart_console(uport))
			IPC_LOG_MSG(msm_port->console_log,
				"%s.Illegal interrupt. sirq 0x%x mirq:0x%x\n",
				 __func__, s_irq_status, m_irq_status);
		else
			WARN_ON(1);
		goto exit_geni_serial_isr;
	}

	if (m_irq_status & (M_IO_DATA_ASSERT_EN | M_IO_DATA_DEASSERT_EN)) {
		uport->icount.cts++;
		IPC_LOG_MSG(msm_port->ipc_log_misc,
			"%s. cts counter:%d\n", __func__,
				uport->icount.cts);
	}

	if (s_irq_status & S_RX_FIFO_WR_ERR_EN) {
		uport->icount.overrun++;
		tty_insert_flip_char(tport, 0, TTY_OVERRUN);
		IPC_LOG_MSG(msm_port->ipc_log_misc,
			"%s.sirq 0x%x buf_overrun:%d\n",
			__func__, s_irq_status, uport->icount.buf_overrun);
	}

	dma = geni_read_reg_nolog(uport->membase, SE_GENI_DMA_MODE_EN);
	if (!dma) {
		geni_status = readl_relaxed(uport->membase + SE_GENI_STATUS);

		if ((m_irq_status & m_irq_en) &
		    (M_TX_FIFO_WATERMARK_EN | M_CMD_DONE_EN))
			msm_geni_serial_handle_tx(uport,
					m_irq_status & M_CMD_DONE_EN,
					geni_status & M_GENI_CMD_ACTIVE);

		if (m_irq_status & (M_CMD_CANCEL_EN | M_CMD_ABORT_EN))
			m_cmd_done = true;

		if (s_irq_status & (S_GP_IRQ_0_EN | S_GP_IRQ_1_EN)) {
			if (s_irq_status & S_GP_IRQ_0_EN)
				uport->icount.parity++;
			IPC_LOG_MSG(msm_port->ipc_log_misc,
				"%s.sirq 0x%x parity:%d\n",
				__func__, s_irq_status, uport->icount.parity);
			drop_rx = true;
		} else if (s_irq_status & (S_GP_IRQ_2_EN | S_GP_IRQ_3_EN)) {
			uport->icount.brk++;
			IPC_LOG_MSG(msm_port->ipc_log_misc,
				"%s.sirq 0x%x break:%d\n",
				__func__, s_irq_status, uport->icount.brk);
		}
		/*
		 * In case of stop_rx handling there is a chance
		 * for RX data can come in parallel. set drop_rx to
		 * avoid data push to framework from handle_rx_console()
		 * API for stop_rx case.
		 */
		if (s_irq_status & (S_CMD_CANCEL_EN | S_CMD_ABORT_EN)) {
			s_cmd_done = true;
			drop_rx = true;
		}

		if (s_irq_status & (S_RX_FIFO_WATERMARK_EN |
							S_RX_FIFO_LAST_EN)) {
			msm_geni_serial_handle_rx(uport, drop_rx);
			if (!drop_rx && !is_irq_masked) {
				spin_unlock_irqrestore(&uport->lock, *flags);
				tty_flip_buffer_push(tport);
				spin_lock_irqsave(&uport->lock, *flags);
			} else if (!drop_rx) {
				tty_flip_buffer_push(tport);
			}
		}
	} else {
		dma_tx_status = geni_read_reg_nolog(uport->membase,
							SE_DMA_TX_IRQ_STAT);
		dma_rx_status = geni_read_reg_nolog(uport->membase,
							SE_DMA_RX_IRQ_STAT);

		if (m_irq_status || s_irq_status ||
				dma_tx_status || dma_rx_status)
			IPC_LOG_MSG(msm_port->ipc_log_irqstatus,
					"%s: sirq:0x%x mirq:0x%x dma_txirq:0x%x dma_rxirq:0x%x\n",
				    __func__, s_irq_status, m_irq_status,
				    dma_tx_status, dma_rx_status);
		if (dma_tx_status) {

			geni_write_reg_nolog(dma_tx_status, uport->membase,
						SE_DMA_TX_IRQ_CLR);

			if (dma_tx_status & (TX_RESET_DONE |
						TX_GENI_CANCEL_IRQ))
				m_cmd_done = true;

			if ((dma_tx_status & TX_DMA_DONE) && !m_cmd_done)
				msm_geni_serial_handle_dma_tx(uport);
		}

		if (m_irq_status & (M_CMD_CANCEL_EN | M_CMD_ABORT_EN))
			m_cmd_done = true;

		if (dma_rx_status)
			s_cmd_done = handle_rx_dma_xfer(s_irq_status, uport);

		if (s_irq_status & (S_CMD_CANCEL_EN | S_CMD_ABORT_EN))
			s_cmd_done = true;
	}

exit_geni_serial_isr:
	if (m_cmd_done) {
		msm_port->m_cmd_done = true;
		complete(&msm_port->m_cmd_timeout);
	}

	if (s_cmd_done) {
		msm_port->s_cmd_done = true;
		complete(&msm_port->s_cmd_timeout);
	}
}

static irqreturn_t msm_geni_serial_isr(int isr, void *dev)
{
	struct uart_port *uport = dev;
	unsigned long flags;

	spin_lock_irqsave(&uport->lock, flags);
	msm_geni_serial_handle_isr(uport, &flags, false);
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
	IPC_LOG_MSG(port->ipc_log_rx, "%s: Edge-Count %d\n", __func__,
							port->edge_count);
	if (port->wakeup_byte && (port->edge_count == 2)) {
		tty = uport->state->port.tty;
		tty_insert_flip_char(tty->port, port->wakeup_byte, TTY_NORMAL);
		IPC_LOG_MSG(port->ipc_log_rx, "%s: Inject 0x%x\n",
					__func__, port->wakeup_byte);
		port->edge_count = 0;
		tty_flip_buffer_push(tty->port);
		__pm_wakeup_event(port->geni_wake, WAKEBYTE_TIMEOUT_MSEC);
	} else if (port->edge_count < 2) {
		port->edge_count++;
	}
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
	if (!uart_console(&port->uport))
		port->rx_wm = port->rx_fifo_depth >>  1;
	else
		port->rx_wm = UART_CONSOLE_RX_WM;
	port->tx_wm = 2;
}

static void msm_geni_serial_shutdown(struct uart_port *uport)
{
	struct msm_geni_serial_port *msm_port = GET_DEV_PORT(uport);
	int ret;

	IPC_LOG_MSG(msm_port->ipc_log_misc, "%s:\n", __func__);
	/* Stop the console before stopping the current tx */
	if (uart_console(uport)) {
		console_stop(uport->cons);
		disable_irq(uport->irq);
	} else {
		msm_geni_serial_power_on(uport);
		wait_for_transfers_inflight(uport);
		msm_geni_serial_stop_tx(uport);
	}

	if (!uart_console(uport)) {
		if (msm_port->ioctl_count) {
			int i;

			for (i = 0; i < msm_port->ioctl_count; i++) {
				IPC_LOG_MSG(msm_port->ipc_log_pwr,
				"%s IOCTL vote present. Forcing off\n",
								__func__);
				msm_geni_serial_power_off(uport);
			}
			msm_port->ioctl_count = 0;
		}

		ret = pm_runtime_put_sync_suspend(uport->dev);
		if (ret) {
			IPC_LOG_MSG(msm_port->ipc_log_pwr,
			"%s: Failed to suspend:%d\n", __func__, ret);
		}

		if (msm_port->wakeup_irq > 0) {
			irq_set_irq_wake(msm_port->wakeup_irq, 0);
			disable_irq(msm_port->wakeup_irq);
			free_irq(msm_port->wakeup_irq, uport);
		}
	}
	IPC_LOG_MSG(msm_port->ipc_log_misc, "%s: End\n", __func__);
}

static int msm_geni_serial_port_setup(struct uart_port *uport)
{
	int ret = 0;
	struct msm_geni_serial_port *msm_port = GET_DEV_PORT(uport);
	unsigned long cfg0, cfg1;
	dma_addr_t dma_address;
	unsigned int rxstale = STALE_COUNT;

	set_rfr_wm(msm_port);
	geni_write_reg_nolog(rxstale, uport->membase, SE_UART_RX_STALE_CNT);
	if (!uart_console(uport)) {
		/* For now only assume FIFO mode. */
		msm_port->xfer_mode = SE_DMA;
		se_get_packing_config(8, 4, false, &cfg0, &cfg1);
		geni_write_reg_nolog(cfg0, uport->membase,
						SE_GENI_TX_PACKING_CFG0);
		geni_write_reg_nolog(cfg1, uport->membase,
						SE_GENI_TX_PACKING_CFG1);
		geni_write_reg_nolog(cfg0, uport->membase,
						SE_GENI_RX_PACKING_CFG0);
		geni_write_reg_nolog(cfg1, uport->membase,
						SE_GENI_RX_PACKING_CFG1);
		if (!msm_port->rx_fifo) {
			ret = -ENOMEM;
			goto exit_portsetup;
		}

		msm_port->rx_buf =
			geni_se_iommu_alloc_buf(msm_port->wrapper_dev,
				&dma_address, DMA_RX_BUF_SIZE);
		if (!msm_port->rx_buf) {
			devm_kfree(uport->dev, msm_port->rx_fifo);
			msm_port->rx_fifo = NULL;
			ret = -ENOMEM;
			goto exit_portsetup;
		}
		msm_port->rx_dma = dma_address;
	} else {
		/*
		 * Make an unconditional cancel on the main sequencer to reset
		 * it else we could end up in data loss scenarios.
		 */
		msm_port->xfer_mode = FIFO_MODE;
		msm_serial_try_disable_interrupts(uport);
		msm_geni_serial_poll_tx_done(uport);
		msm_geni_serial_enable_interrupts(uport);
		se_get_packing_config(8, 1, false, &cfg0, &cfg1);
		geni_write_reg_nolog(cfg0, uport->membase,
						SE_GENI_TX_PACKING_CFG0);
		geni_write_reg_nolog(cfg1, uport->membase,
						SE_GENI_TX_PACKING_CFG1);
		se_get_packing_config(8, 4, false, &cfg0, &cfg1);
		geni_write_reg_nolog(cfg0, uport->membase,
						SE_GENI_RX_PACKING_CFG0);
		geni_write_reg_nolog(cfg1, uport->membase,
						SE_GENI_RX_PACKING_CFG1);
	}

	ret = geni_se_init(uport->membase, msm_port->rx_wm, msm_port->rx_rfr);
	if (ret) {
		dev_err(uport->dev, "%s: Fail\n", __func__);
		goto free_dma;
	}

	ret = geni_se_select_mode(uport->membase, msm_port->xfer_mode);
	if (ret)
		goto free_dma;

	msm_port->port_setup = true;
	/*
	 * Ensure Port setup related IO completes before returning to
	 * framework.
	 */
	mb();

	return 0;
free_dma:
	if (msm_port->rx_dma) {
		geni_se_iommu_free_buf(msm_port->wrapper_dev,
			&msm_port->rx_dma, msm_port->rx_buf, DMA_RX_BUF_SIZE);
		msm_port->rx_dma = (dma_addr_t)NULL;
	}
exit_portsetup:
	return ret;
}

static int msm_geni_serial_startup(struct uart_port *uport)
{
	int ret = 0;
	struct msm_geni_serial_port *msm_port = GET_DEV_PORT(uport);

	IPC_LOG_MSG(msm_port->ipc_log_misc, "%s:\n", __func__);

	msm_port->startup_in_progress = true;

	if (likely(!uart_console(uport))) {
		ret = msm_geni_serial_power_on(&msm_port->uport);
		if (ret) {
			dev_err(uport->dev, "%s:Failed to power on %d\n",
							__func__, ret);
			return ret;
		}
	}

	get_tx_fifo_size(msm_port);
	if (!msm_port->port_setup) {
		ret = msm_geni_serial_port_setup(uport);
		if (ret) {
			IPC_LOG_MSG(msm_port->ipc_log_misc,
				    "%s: port_setup Fail ret:%d\n",
				    __func__, ret);
			goto exit_startup;
		}
	}

	/*
	 * Ensure that all the port configuration writes complete
	 * before returning to the framework.
	 */
	mb();

	/* Console usecase requires irq to be in enable state after early
	 * console switch from probe to handle RX data. Hence enable IRQ
	 * from starup and disable it form shutdown APIs for cosnole case.
	 * BT HSUART usecase, IRQ will be enabled from runtime_resume()
	 * and disabled in runtime_suspend to avoid spurious interrupts
	 * after suspend.
	 */
	if (uart_console(uport))
		enable_irq(uport->irq);

	if (msm_port->wakeup_irq > 0) {
		ret = request_irq(msm_port->wakeup_irq, msm_geni_wakeup_isr,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				"hs_uart_wakeup", uport);
		if (unlikely(ret)) {
			dev_err(uport->dev, "%s:Failed to get WakeIRQ ret%d\n",
								__func__, ret);
			goto exit_startup;
		}
		disable_irq(msm_port->wakeup_irq);
		ret = irq_set_irq_wake(msm_port->wakeup_irq, 1);
		if (unlikely(ret)) {
			dev_err(uport->dev, "%s:Failed to set IRQ wake:%d\n",
					__func__, ret);
			goto exit_startup;
		}
	}
exit_startup:
	if (likely(!uart_console(uport)))
		msm_geni_serial_power_off(&msm_port->uport);
	msm_port->startup_in_progress = false;
	IPC_LOG_MSG(msm_port->ipc_log_misc, "%s: ret:%d\n", __func__, ret);

	return ret;
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
	geni_read_reg_nolog(uport->membase, GENI_SER_M_CLK_CFG);
}

#if defined(CONFIG_SERIAL_CORE_CONSOLE) || defined(CONFIG_CONSOLE_POLL)
static int get_clk_cfg(unsigned long clk_freq, unsigned long *ser_clk)
{
	unsigned long root_freq[] = {7372800, 14745600, 19200000, 29491200,
		32000000, 48000000, 64000000, 80000000, 96000000, 100000000,
		102400000, 112000000, 120000000, 128000000};
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
#endif

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
	int clk_div, ret;
	unsigned long ser_clk_cfg = 0;
	struct msm_geni_serial_port *port = GET_DEV_PORT(uport);
	unsigned long clk_rate;
	unsigned long desired_rate;
	unsigned int clk_idx;
	int uart_sampling;
	int clk_freq_diff;

	/* QUP_2.5.0 and older RUMI has sampling rate as 32 */
	if (port->rumi_platform && port->is_console) {
		geni_write_reg_nolog(0x21, uport->membase, GENI_SER_M_CLK_CFG);
		geni_write_reg_nolog(0x21, uport->membase, GENI_SER_S_CLK_CFG);
		geni_read_reg_nolog(uport->membase, GENI_SER_M_CLK_CFG);
	}

	if (!uart_console(uport)) {
		int ret = msm_geni_serial_power_on(uport);

		if (ret) {
			IPC_LOG_MSG(port->ipc_log_misc,
				"%s: Failed to vote clock on:%d\n",
							__func__, ret);
			return;
		}
	}
	msm_geni_serial_stop_rx(uport);
	/* baud rate */
	baud = uart_get_baud_rate(uport, termios, old, 300, 4000000);
	port->cur_baud = baud;
	uart_sampling = IS_ENABLED(CONFIG_SERIAL_MSM_GENI_HALF_SAMPLING) ?
				UART_OVERSAMPLING / 2 : UART_OVERSAMPLING;
	desired_rate = baud * uart_sampling;

	/*
	 * Request for nearest possible required frequency instead of the exact
	 * required frequency.
	 */
	ret = geni_se_clk_freq_match(&port->serial_rsc, desired_rate,
			&clk_idx, &clk_rate, false);
	if (ret) {
		dev_err(uport->dev, "%s: Failed(%d) to find src clk for 0x%x\n",
				__func__, ret, baud);
		goto exit_set_termios;
	}

	clk_div = DIV_ROUND_UP(clk_rate, desired_rate);
	if (clk_div <= 0)
		goto exit_set_termios;

	clk_freq_diff =  (desired_rate - (clk_rate / clk_div));
	if (clk_freq_diff)
		IPC_LOG_MSG(port->ipc_log_misc,
			"src_clk freq_diff:%d baud:%d clk_rate:%d clk_div:%d\n",
			clk_freq_diff, baud, clk_rate, clk_div);

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

	uport->status  &= ~(UPSTAT_AUTOCTS);
	/* stop bits */
	if (termios->c_cflag & CSTOPB)
		stop_bit_len = TX_STOP_BIT_LEN_2;
	else
		stop_bit_len = TX_STOP_BIT_LEN_1;

	/* flow control, clear the CTS_MASK bit if using flow control. */
	if (termios->c_cflag & CRTSCTS) {
		tx_trans_cfg &= ~UART_CTS_MASK;
		uport->status |= UPSTAT_AUTOCTS;
	}
	else
		tx_trans_cfg |= UART_CTS_MASK;
	/* status bits to ignore */

	if (likely(baud))
		uart_update_timeout(uport, termios->c_cflag, baud);

	geni_serial_write_term_regs(uport, port->loopback, tx_trans_cfg,
		tx_parity_cfg, rx_trans_cfg, rx_parity_cfg, bits_per_char,
		stop_bit_len, ser_clk_cfg);

	if (termios->c_cflag & CRTSCTS) {
		geni_write_reg_nolog(0x0, uport->membase, SE_UART_MANUAL_RFR);
		IPC_LOG_MSG(port->ipc_log_misc,
			"%s: Manual flow Disabled, HW Flow ON\n", __func__);
	}

	IPC_LOG_MSG(port->ipc_log_misc, "%s: baud %d\n", __func__, baud);
	IPC_LOG_MSG(port->ipc_log_misc, "Tx: trans_cfg%d parity %d\n",
						tx_trans_cfg, tx_parity_cfg);
	IPC_LOG_MSG(port->ipc_log_misc, "Rx: trans_cfg%d parity %d",
						rx_trans_cfg, rx_parity_cfg);
	IPC_LOG_MSG(port->ipc_log_misc, "BitsChar%d stop bit%d\n",
				bits_per_char, stop_bit_len);
exit_set_termios:
	msm_geni_serial_start_rx(uport);
	if (!uart_console(uport))
		msm_geni_serial_power_off(uport);
	return;

}

static unsigned int msm_geni_serial_tx_empty(struct uart_port *uport)
{
	unsigned int tx_fifo_status;
	unsigned int is_tx_empty = 1;
	struct msm_geni_serial_port *port = GET_DEV_PORT(uport);

	if (!uart_console(uport) && device_pending_suspend(uport))
		return 1;

	if (port->xfer_mode == SE_DMA)
		tx_fifo_status = port->tx_dma ? 1 : 0;
	else
		tx_fifo_status = geni_read_reg_nolog(uport->membase,
						SE_GENI_TX_FIFO_STATUS);
	if (tx_fifo_status)
		is_tx_empty = 0;

	return is_tx_empty;
}

static ssize_t xfer_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct msm_geni_serial_port *port = platform_get_drvdata(pdev);
	ssize_t ret = 0;

	if (port->xfer_mode == FIFO_MODE)
		ret = snprintf(buf, sizeof("FIFO\n"), "FIFO\n");
	else if (port->xfer_mode == SE_DMA)
		ret = snprintf(buf, sizeof("SE_DMA\n"), "SE_DMA\n");

	return ret;
}

static ssize_t xfer_mode_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct msm_geni_serial_port *port = platform_get_drvdata(pdev);
	struct uart_port *uport = &port->uport;
	int xfer_mode = port->xfer_mode;
	unsigned long flags;

	if (uart_console(uport))
		return -EOPNOTSUPP;

	if (strnstr(buf, "FIFO", strlen("FIFO"))) {
		xfer_mode = FIFO_MODE;
	} else if (strnstr(buf, "SE_DMA", strlen("SE_DMA"))) {
		xfer_mode = SE_DMA;
	} else {
		dev_err(dev, "%s: Invalid input %s\n", __func__, buf);
		return -EINVAL;
	}

	if (xfer_mode == port->xfer_mode)
		return size;

	msm_geni_serial_power_on(uport);
	msm_geni_serial_stop_tx(uport);
	msm_geni_serial_stop_rx(uport);
	spin_lock_irqsave(&uport->lock, flags);
	port->xfer_mode = xfer_mode;
	geni_se_select_mode(uport->membase, port->xfer_mode);
	spin_unlock_irqrestore(&uport->lock, flags);
	msm_geni_serial_start_rx(uport);
	msm_geni_serial_power_off(uport);

	return size;
}

static DEVICE_ATTR_RW(xfer_mode);

static ssize_t ver_info_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct msm_geni_serial_port *port = platform_get_drvdata(pdev);
	ssize_t ret = 0;
	int len = (sizeof(struct msm_geni_serial_ver_info) * 2);

	ret = snprintf(buf, len, "FW ver=0x%x%x, HW ver=%d.%d.%d\n",
		port->ver_info.m_fw_ver, port->ver_info.m_fw_ver,
		port->ver_info.hw_major_ver, port->ver_info.hw_minor_ver,
		port->ver_info.hw_step_ver);

	return ret;
}
static DEVICE_ATTR_RO(ver_info);

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

	/* Max 1 port supported as of now */
	if (unlikely(co->index >= GENI_UART_CONS_PORTS  || co->index < 0))
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

	if (!dev_port->port_setup) {
		msm_geni_serial_stop_rx(uport);
		msm_geni_serial_port_setup(uport);
	}

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

static void msm_geni_serial_cancel_rx(struct uart_port *uport)
{
	int done = 0;
	int i = 0;
	unsigned int irq_status;
	u32 rx_fifo_status;
	u32 rx_fifo_wc;
	u32 geni_status;

	geni_status = geni_read_reg_nolog(uport->membase, SE_GENI_STATUS);
	/* Possible thats stop rx is already done from UEFI end */
	if (!(geni_status & S_GENI_CMD_ACTIVE))
		return;

	geni_cancel_s_cmd(uport->membase);
	/* Ensure this goes through before polling. */
	mb();

	done = msm_geni_serial_poll_bit(uport, SE_GENI_S_IRQ_STATUS,
						S_CMD_CANCEL_EN, true);
	if (!done) {
		geni_abort_s_cmd(uport->membase);
		/* Ensure this goes through before polling. */
		mb();
		msm_geni_serial_poll_bit(uport, SE_GENI_S_IRQ_STATUS,
					S_CMD_ABORT_EN, false);
	} else if (msm_geni_serial_poll_bit(uport,
			SE_GENI_S_IRQ_STATUS, S_RX_FIFO_LAST_EN, true)) {
		rx_fifo_status = geni_read_reg_nolog(uport->membase,
						SE_GENI_RX_FIFO_STATUS);
		rx_fifo_wc = rx_fifo_status & RX_FIFO_WC_MSK;
		for (i = 0; i < rx_fifo_wc; i++)
			geni_read_reg_nolog(uport->membase,
							SE_GENI_RX_FIFOn);
	}

	irq_status = geni_read_reg_nolog(uport->membase,
						SE_GENI_S_IRQ_STATUS);
	geni_write_reg_nolog(irq_status, uport->membase, SE_GENI_S_IRQ_CLEAR);

	if (!done)
		geni_write_reg(FORCE_DEFAULT, uport->membase,
						GENI_FORCE_DEFAULT_REG);
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
	int clk_div;
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

	if (IS_ENABLED(CONFIG_SERIAL_MSM_GENI_HALF_SAMPLING))
		clk_div *= 2;

	s_clk_cfg |= SER_CLK_EN;
	s_clk_cfg |= (clk_div << CLK_DIV_SHFT);

	/*
	 * Here we need to poll for command done which indicates that
	 * the previous tx transfer is done. And if the command done interrupt
	 * is not getting set, then we need to cancel the command.
	 */
	msm_geni_serial_poll_tx_done(uport);

	/*
	 * Here cancel rx is done in polling mode as there is
	 * no isr support during early console time.
	 */
	msm_geni_serial_cancel_rx(uport);

	/* Only for earlyconsole */
	if (IS_ENABLED(CONFIG_SERIAL_MSM_GENI_HALF_SAMPLING)) {
		geni_write_reg_nolog(0x21, uport->membase, GENI_SER_M_CLK_CFG);
		geni_write_reg_nolog(0x21, uport->membase, GENI_SER_S_CLK_CFG);
		geni_read_reg_nolog(uport->membase, GENI_SER_M_CLK_CFG);
	}

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
	geni_read_reg_nolog(uport->membase, GENI_SER_M_CLK_CFG);

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
OF_EARLYCON_DECLARE(msm_geni_serial, "qcom,msm-geni-console",
		msm_geni_serial_earlycon_setup);

static int console_register(struct uart_driver *drv)
{

#ifdef CONFIG_FASTBOOT_CMD_CTRL_UART
	if (!is_early_cons_enabled) {
		pr_info("ignore console register\n");
		return 0;
	}
#endif

	return uart_register_driver(drv);
}
static void console_unregister(struct uart_driver *drv)
{

#ifdef CONFIG_FASTBOOT_CMD_CTRL_UART
	if (!is_early_cons_enabled) {
		pr_info("ignore console unregister\n");
		return;
	}
#endif

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
	.nr =  GENI_UART_CONS_PORTS,
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

static void msm_geni_serial_debug_init(struct uart_port *uport, bool console)
{
	struct msm_geni_serial_port *msm_port = GET_DEV_PORT(uport);
	char name[35];

	msm_port->dbg = debugfs_create_dir(dev_name(uport->dev), NULL);
	if (IS_ERR_OR_NULL(msm_port->dbg))
		dev_err(uport->dev, "Failed to create dbg dir\n");

	if (!console) {
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
		/* New set of UART IPC log for RX Invalid case */
		memset(name, 0, sizeof(name));
		if (!msm_port->ipc_log_single) {
			scnprintf(name, sizeof(name), "%s%s",
					dev_name(uport->dev), "_single");
			msm_port->ipc_log_single = ipc_log_context_create(
					IPC_LOG_MISC_PAGES, name, 0);
			if (!msm_port->ipc_log_single)
				dev_info(uport->dev, "Err in single IPC Log\n");
		}
		memset(name, 0, sizeof(name));
		if (!msm_port->ipc_log_irqstatus) {
			scnprintf(name, sizeof(name), "%s%s",
					dev_name(uport->dev), "_irqstatus");
			msm_port->ipc_log_irqstatus = ipc_log_context_create(
					IPC_LOG_MISC_PAGES, name, 0);
			if (!msm_port->ipc_log_irqstatus)
				dev_info(uport->dev, "Err in irqstatus IPC Log\n");
		}
	} else {
		memset(name, 0, sizeof(name));
		if (!msm_port->console_log) {
			scnprintf(name, sizeof(name), "%s%s",
					dev_name(uport->dev), "_console");
			msm_port->console_log = ipc_log_context_create(
					IPC_LOG_MISC_PAGES, name, 0);
			if (!msm_port->console_log)
				dev_info(uport->dev, "Err in Misc IPC Log\n");
		}
	}
}

static void msm_geni_serial_cons_pm(struct uart_port *uport,
		unsigned int new_state, unsigned int old_state)
{
	struct msm_geni_serial_port *msm_port = GET_DEV_PORT(uport);

	if (unlikely(!uart_console(uport)))
		return;

	if (new_state == UART_PM_STATE_ON && old_state == UART_PM_STATE_OFF)
		se_geni_resources_on(&msm_port->serial_rsc);
	else if (new_state == UART_PM_STATE_OFF &&
			old_state == UART_PM_STATE_ON)
		se_geni_resources_off(&msm_port->serial_rsc);
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
	.pm = msm_geni_serial_cons_pm,
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

static int msm_geni_serial_get_ver_info(struct uart_port *uport)
{
	int hw_ver, ret = 0;
	struct msm_geni_serial_port *msm_port = GET_DEV_PORT(uport);

	/*
	 * At this time early console is still active and transfers are
	 * in-coming. Make sure UART doesn't turn on/off clocks for
	 * console usecase.
	 */
	if (!msm_port->is_console)
		se_geni_clks_on(&msm_port->serial_rsc);
	/* Basic HW and FW info */
	if (unlikely(get_se_proto(uport->membase) != UART)) {
		dev_err(uport->dev, "%s: Invalid FW %d loaded.\n",
			 __func__, get_se_proto(uport->membase));
		ret = -ENXIO;
		goto exit_ver_info;
	}

	msm_port->ver_info.m_fw_ver = get_se_m_fw(uport->membase);
	msm_port->ver_info.s_fw_ver = get_se_s_fw(uport->membase);
	IPC_LOG_MSG(msm_port->ipc_log_misc, "%s: FW Ver:0x%x%x\n",
		__func__,
		msm_port->ver_info.m_fw_ver, msm_port->ver_info.s_fw_ver);

	hw_ver = geni_se_qupv3_hw_version(msm_port->wrapper_dev,
		&msm_port->ver_info.hw_major_ver,
		&msm_port->ver_info.hw_minor_ver,
		&msm_port->ver_info.hw_step_ver);
	if (hw_ver)
		dev_err(uport->dev, "%s:Err getting HW version %d\n",
						__func__, hw_ver);
	else
		IPC_LOG_MSG(msm_port->ipc_log_misc, "%s: HW Ver:%x.%x.%x\n",
			__func__, msm_port->ver_info.hw_major_ver,
			msm_port->ver_info.hw_minor_ver,
			msm_port->ver_info.hw_step_ver);

	msm_geni_serial_enable_interrupts(uport);
exit_ver_info:
	if (!msm_port->is_console)
		se_geni_clks_off(&msm_port->serial_rsc);
	return ret;
}

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
	u32 wake_char = 0;

	id = of_match_device(msm_geni_device_tbl, &pdev->dev);
	if (id) {
		dev_dbg(&pdev->dev, "%s: %s\n", __func__, id->compatible);
		drv = (struct uart_driver *)id->data;
	} else {
		dev_err(&pdev->dev, "%s: No matching device found\n",
								__func__);
		return -ENODEV;
	}

#ifdef CONFIG_FASTBOOT_CMD_CTRL_UART
	/*if earlycon is not enabled, we should ignore console
	driver prob*/
	if (!is_early_cons_enabled &&
		(!strcmp(id->compatible, "qcom,msm-geni-console"))) {
			pr_info("ignore cons prob\n");
			return -ENODEV;
	}
#endif

	if (pdev->dev.of_node) {
		if (drv->cons) {
			line = of_alias_get_id(pdev->dev.of_node, "serial");
			if (line < 0)
				line = 0;
		} else {
			line = of_alias_get_id(pdev->dev.of_node, "hsuart");
			if (line < 0)
				line = uart_line_id++;
			else
				uart_line_id++;
		}
	} else {
		line = pdev->id;
	}

	is_console = (drv->cons ? true : false);
	dev_port = get_port_from_line(line, is_console);
	if (IS_ERR_OR_NULL(dev_port)) {
		ret = PTR_ERR(dev_port);
		dev_err(&pdev->dev, "Invalid line %d(%d)\n",
					line, ret);
		goto exit_geni_serial_probe;
	}
	dev_port->is_console = is_console;

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

	if (is_console)
		ret = geni_se_resources_init(&dev_port->serial_rsc,
			UART_CONSOLE_CORE2X_VOTE,
			(DEFAULT_SE_CLK * DEFAULT_BUS_WIDTH));
	else
		ret = geni_se_resources_init(&dev_port->serial_rsc,
			UART_CORE2X_VOTE,
			(DEFAULT_SE_CLK * DEFAULT_BUS_WIDTH));

	if (ret)
		goto exit_geni_serial_probe;

	dev_port->serial_rsc.ctrl_dev = &pdev->dev;

	/* RUMI specific */
	dev_port->rumi_platform = of_property_read_bool(pdev->dev.of_node,
					"qcom,rumi_platform");

	if (of_property_read_u32(pdev->dev.of_node, "qcom,wakeup-byte",
					&wake_char)) {
		dev_dbg(&pdev->dev, "No Wakeup byte specified\n");
	} else {
		dev_port->wakeup_byte = (u8)wake_char;
		dev_info(&pdev->dev, "Wakeup byte 0x%x\n",
					dev_port->wakeup_byte);
	}

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
		dev_err(&pdev->dev, "Err IO mapping serial iomem\n");

		goto exit_geni_serial_probe;
	}

	/* Optional to use the Rx pin as wakeup irq */
	dev_port->wakeup_irq = platform_get_irq(pdev, 1);
	if ((dev_port->wakeup_irq < 0 && !is_console))
		dev_info(&pdev->dev, "No wakeup IRQ configured\n");

	dev_port->serial_rsc.geni_pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR_OR_NULL(dev_port->serial_rsc.geni_pinctrl)) {
		dev_err(&pdev->dev, "No pinctrl config specified!\n");
		ret = PTR_ERR(dev_port->serial_rsc.geni_pinctrl);
		goto exit_geni_serial_probe;
	}
	dev_port->serial_rsc.geni_gpio_active =
		pinctrl_lookup_state(dev_port->serial_rsc.geni_pinctrl,
							PINCTRL_ACTIVE);

	if (IS_ERR_OR_NULL(dev_port->serial_rsc.geni_gpio_active)) {
		/*
		 * Backward compatible : In case few chips doesn't have ACTIVE
		 * state defined.
		 */
		dev_port->serial_rsc.geni_gpio_active =
		pinctrl_lookup_state(dev_port->serial_rsc.geni_pinctrl,
							PINCTRL_DEFAULT);
		if (IS_ERR_OR_NULL(dev_port->serial_rsc.geni_gpio_active)) {
			dev_err(&pdev->dev, "No default config specified!\n");
			ret = PTR_ERR(dev_port->serial_rsc.geni_gpio_active);
			goto exit_geni_serial_probe;
		}
	}
	/*
	 * For clients who setup an Inband wakeup, leave the GPIO pins
	 * always connected to the core, else move the pins to their
	 * defined "sleep" state.
	 */
	if (dev_port->wakeup_irq > 0) {
		dev_port->serial_rsc.geni_gpio_sleep =
			dev_port->serial_rsc.geni_gpio_active;
	} else {
		dev_port->serial_rsc.geni_gpio_sleep =
			pinctrl_lookup_state(dev_port->serial_rsc.geni_pinctrl,
							PINCTRL_SLEEP);
		if (IS_ERR_OR_NULL(dev_port->serial_rsc.geni_gpio_sleep)) {
			dev_err(&pdev->dev, "No sleep config specified!\n");
			ret = PTR_ERR(dev_port->serial_rsc.geni_gpio_sleep);
			goto exit_geni_serial_probe;
		}
	}

	if (!is_console) {
		dev_port->geni_wake = wakeup_source_register(uport->dev,
						dev_name(&pdev->dev));
		if (!dev_port->geni_wake) {
			dev_err(&pdev->dev,
				"Failed to register wakeup_source\n");
			ret = -ENOMEM;
			goto exit_geni_serial_probe;
		}
	}

	dev_port->tx_fifo_depth = DEF_FIFO_DEPTH_WORDS;
	dev_port->rx_fifo_depth = DEF_FIFO_DEPTH_WORDS;
	dev_port->tx_fifo_width = DEF_FIFO_WIDTH_BITS;
	uport->fifosize =
		((dev_port->tx_fifo_depth * dev_port->tx_fifo_width) >> 3);
	/* Complete signals to handle cancel cmd completion */
	init_completion(&dev_port->m_cmd_timeout);
	init_completion(&dev_port->s_cmd_timeout);

	uport->irq = platform_get_irq(pdev, 0);
	if (uport->irq < 0) {
		ret = uport->irq;
		dev_err(&pdev->dev, "Failed to get IRQ %d\n", ret);
		goto exit_wakeup_unregister;
	}

	dev_port->name = devm_kasprintf(uport->dev, GFP_KERNEL,
					"msm_serial_geni%d", uport->line);
	irq_set_status_flags(uport->irq, IRQ_NOAUTOEN);
	ret = devm_request_irq(uport->dev, uport->irq, msm_geni_serial_isr,
				IRQF_TRIGGER_HIGH, dev_port->name, uport);
	if (ret) {
		dev_err(uport->dev, "%s: Failed to get IRQ ret %d\n",
							__func__, ret);
		goto exit_wakeup_unregister;
	}

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
		pm_runtime_set_autosuspend_delay(&pdev->dev, 150);
		pm_runtime_use_autosuspend(&pdev->dev);
		pm_runtime_enable(&pdev->dev);
	}

	if (IS_ENABLED(CONFIG_SERIAL_MSM_GENI_HALF_SAMPLING) &&
		dev_port->rumi_platform && dev_port->is_console) {
		geni_write_reg_nolog(0x21, uport->membase, GENI_SER_M_CLK_CFG);
		geni_write_reg_nolog(0x21, uport->membase, GENI_SER_S_CLK_CFG);
		geni_read_reg_nolog(uport->membase, GENI_SER_M_CLK_CFG);
	}

	dev_info(&pdev->dev, "Serial port%d added.FifoSize %d is_console%d\n",
				line, uport->fifosize, is_console);

	device_create_file(uport->dev, &dev_attr_loopback);
	device_create_file(uport->dev, &dev_attr_xfer_mode);
	device_create_file(uport->dev, &dev_attr_ver_info);
	msm_geni_serial_debug_init(uport, is_console);
	dev_port->port_setup = false;
	ret = msm_geni_serial_get_ver_info(uport);
	if (ret)
		goto exit_wakeup_unregister;

	ret = uart_add_one_port(drv, uport);
	if (ret)
		goto exit_wakeup_unregister;

	if (!uart_console(uport))
		spin_lock_init(&dev_port->rx_lock);

	IPC_LOG_MSG(dev_port->ipc_log_misc, "%s: port:%s irq:%d\n", __func__,
		    uport->name, uport->irq);

	return 0;

exit_wakeup_unregister:
	if (!is_console)
		wakeup_source_unregister(dev_port->geni_wake);
exit_geni_serial_probe:
	IPC_LOG_MSG(dev_port->ipc_log_misc, "%s: ret:%d\n", __func__, ret);
	return ret;
}

static int msm_geni_serial_remove(struct platform_device *pdev)
{
	struct msm_geni_serial_port *port = platform_get_drvdata(pdev);
	struct uart_driver *drv =
			(struct uart_driver *)port->uport.private_data;

	if (!uart_console(&port->uport))
		wakeup_source_unregister(port->geni_wake);
	uart_remove_one_port(drv, &port->uport);
	if (port->rx_dma) {
		geni_se_iommu_free_buf(port->wrapper_dev, &port->rx_dma,
					port->rx_buf, DMA_RX_BUF_SIZE);
		port->rx_dma = (dma_addr_t)NULL;
	}
	return 0;
}

static void msm_geni_serial_allow_rx(struct msm_geni_serial_port *port)
{
	u32 uart_manual_rfr;

	uart_manual_rfr = (UART_MANUAL_RFR_EN | UART_RFR_READY);
	geni_write_reg_nolog(uart_manual_rfr, port->uport.membase,
						SE_UART_MANUAL_RFR);
	/* Ensure that the manual flow off writes go through */
	mb();
	uart_manual_rfr = geni_read_reg_nolog(port->uport.membase,
						SE_UART_MANUAL_RFR);
	IPC_LOG_MSG(port->ipc_log_misc, "%s(): rfr = 0x%x\n",
					__func__, uart_manual_rfr);

	/* To give control of RFR back to HW */
	msm_geni_serial_set_manual_flow(true, port);
}

#ifdef CONFIG_PM
static int msm_geni_serial_runtime_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct msm_geni_serial_port *port = platform_get_drvdata(pdev);
	int ret = 0;
	u32 geni_status = geni_read_reg_nolog(port->uport.membase,
							SE_GENI_STATUS);

	wait_for_transfers_inflight(&port->uport);
	/*
	 * Manual RFR On.
	 * Stop Rx.
	 * Disable Interrupt
	 * Resources off
	 */
	ret = stop_rx_sequencer(&port->uport);
	if (ret) {
		IPC_LOG_MSG(port->ipc_log_pwr, "%s: stop rx failed %d\n",
							__func__, ret);
		return -EBUSY;
	}
	geni_status = geni_read_reg_nolog(port->uport.membase, SE_GENI_STATUS);

	if ((geni_status & M_GENI_CMD_ACTIVE))
		stop_tx_sequencer(&port->uport);

	disable_irq(port->uport.irq);

	/*
	 * Above stop_rx disabled the flow so we need to enable it here
	 * Make sure wake up interrupt is enabled before RFR is made low
	 */
	msm_geni_serial_allow_rx(port);

	ret = se_geni_resources_off(&port->serial_rsc);
	if (ret) {
		dev_err(dev, "%s: Error ret %d\n", __func__, ret);
		goto exit_runtime_suspend;
	}

	if (port->wakeup_irq > 0) {
		port->edge_count = 0;
		enable_irq(port->wakeup_irq);
	}
	IPC_LOG_MSG(port->ipc_log_pwr, "%s:\n", __func__);
	__pm_relax(port->geni_wake);
exit_runtime_suspend:
	return ret;
}

static int msm_geni_serial_runtime_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct msm_geni_serial_port *port = platform_get_drvdata(pdev);
	int ret = 0;

	/*
	 * Do an unconditional relax followed by a stay awake in case the
	 * wake source is activated by the wakeup isr.
	 */
	__pm_relax(port->geni_wake);
	__pm_stay_awake(port->geni_wake);
	if (port->wakeup_irq > 0)
		disable_irq(port->wakeup_irq);
	/*
	 * Resources On.
	 * Start Rx.
	 * Auto RFR.
	 * Enable IRQ.
	 */
	ret = se_geni_resources_on(&port->serial_rsc);
	if (ret) {
		dev_err(dev, "%s: Error ret %d\n", __func__, ret);
		__pm_relax(port->geni_wake);
		goto exit_runtime_resume;
	}
	start_rx_sequencer(&port->uport);
	/* Ensure that the Rx is running before enabling interrupts */
	mb();
	/* Enable interrupt */
	enable_irq(port->uport.irq);

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
		struct uart_state *state = uport->state;
		struct tty_port *tty_port = &state->port;

		mutex_lock(&tty_port->mutex);
		if (!pm_runtime_status_suspended(dev)) {
			dev_err(dev, "%s:Active userspace vote; ioctl_cnt %d\n",
					__func__, port->ioctl_count);
			IPC_LOG_MSG(port->ipc_log_pwr,
				"%s:Active userspace vote; ioctl_cnt %d\n",
					__func__, port->ioctl_count);
			mutex_unlock(&tty_port->mutex);
			return -EBUSY;
		}
		IPC_LOG_MSG(port->ipc_log_pwr, "%s\n", __func__);
		mutex_unlock(&tty_port->mutex);
	}
	return 0;
}

static int msm_geni_serial_sys_resume_noirq(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct msm_geni_serial_port *port = platform_get_drvdata(pdev);
	struct uart_port *uport = &port->uport;

	if (uart_console(uport) &&
	    console_suspend_enabled && uport->suspended) {
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

static struct platform_driver msm_geni_serial_platform_driver = {
	.remove = msm_geni_serial_remove,
	.probe = msm_geni_serial_probe,
	.driver = {
		.name = "msm_geni_serial",
		.of_match_table = msm_geni_device_tbl,
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

	pr_info("%s: Driver initialized\n", __func__);

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
