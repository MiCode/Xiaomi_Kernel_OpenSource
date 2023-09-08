// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
#include <linux/moduleparam.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/msm-geni-se.h>
#include <linux/serial.h>
#include <linux/serial_core.h>
#include <linux/slab.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/ioctl.h>
#include <linux/pinctrl/consumer.h>
#include <linux/dma-mapping.h>
#include <uapi/linux/msm_geni_serial.h>

static bool con_enabled = IS_ENABLED(CONFIG_SERIAL_MSM_GENI_CONSOLE_DEFAULT_ENABLED);
module_param(con_enabled, bool, 0644);

/* UART specific GENI registers */
#define SE_UART_LOOPBACK_CFG		(0x22C)
#define SE_GENI_CFG_REG80		(0x240)
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
#define PAR_SPACE		(0x02)
#define PAR_MARK		(0x03)

/* SE_UART_MANUAL_RFR register fields */
#define UART_MANUAL_RFR_EN	(BIT(31))
#define UART_RFR_NOT_READY	(BIT(1))
#define UART_RFR_READY		(BIT(0))

/* UART M_CMD OP codes */
#define UART_START_TX		(0x1)
#define UART_START_BREAK	(0x4)
#define UART_STOP_BREAK		(0x5)

/* UART S_CMD OP codes */
#define UART_START_READ			(0x1)
#define UART_PARAM			(0x1)
/* When set character with framing error is not written in RX fifo */
#define UART_PARAM_SKIP_FRAME_ERR_CHAR	(BIT(5))
/* When set break character is not written in RX fifo */
#define UART_PARAM_SKIP_BREAK_CHAR	(BIT(6))
#define UART_PARAM_RFR_OPEN		(BIT(7))

/* UART DMA Rx GP_IRQ_BITS */
#define UART_DMA_RX_PARITY_ERR	BIT(5)
#define UART_DMA_RX_FRAMING_ERR	BIT(6)
#define UART_DMA_RX_ERRS	(GENMASK(6, 5))
#define UART_DMA_RX_BREAK	(GENMASK(8, 7))

#define UART_OVERSAMPLING	(32)
#define STALE_TIMEOUT		(16)
#define STALE_COUNT		(DEFAULT_BITS_PER_CHAR * STALE_TIMEOUT)
#define SEC_TO_USEC		(1000000)
#define SYSTEM_DELAY		(500) /* 500 usec */
#define STALE_DELAY_MAX		(10000) /* 10 msec */
#define DEFAULT_BITS_PER_CHAR	(10)
#define GENI_UART_NR_PORTS	(6)
#define GENI_UART_CONS_PORTS	(1)
#define DEF_FIFO_DEPTH_WORDS	(16)
#define DEF_TX_WM		(2)
#define DEF_FIFO_WIDTH_BITS	(32)

#define WAKEBYTE_TIMEOUT_MSEC	(2000) /* 2 Seconds */
#define WAIT_XFER_MAX_ITER	(2)
#define WAIT_XFER_MAX_TIMEOUT_US	(150)
#define WAIT_XFER_MIN_TIMEOUT_US	(100)
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

#define IPC_LOG_MSG(ctx, x...) ipc_log_string(ctx, x)

#define DMA_RX_BUF_SIZE		(2048)
#define UART_CONSOLE_RX_WM	(2)

#define EDGE_COUNT_IRQ_TIMEOUT_MSECS	(100)
#define CREATE_TRACE_POINTS
#include "serial_trace.h"

/* FTRACE Logging */
static void __ftrace_dbg(struct device *dev, const char *fmt, ...)
{
	struct va_format vaf = {
		.fmt = fmt,
	};
	va_list args;

	va_start(args, fmt);
	vaf.va = &args;
	trace_serial_info(dev_name(dev), &vaf);
	va_end(args);
}

#define UART_LOG_DBG(ctxt, dev, fmt...) do {		\
	ipc_log_string(ctxt, fmt);		\
	ftrace_dbg(dev, fmt);			\
} while (0)					\

#define ftrace_dbg(dev, fmt, ...)			\
	__ftrace_dbg(dev, fmt, ##__VA_ARGS__)	\

/**
 * enum uart_error_code: Various error codes used by driver
 * @UART_ERROR_DEFAULT: Default error code
 * @UART_ERROR_INVALID_FW_LOADED: used when invalid fw is downloaded
 * @UART_ERROR_CLK_GET_FAIL: used when unable to get core se clocks
 * @UART_ERROR_SE_CLK_RATE_FIND_FAIL: used when unable to get requested
 *  clock rate
 * @UART_ERROR_SE_RESOURCES_INIT_FAIL: used when serial engine resources
 *  init failed
 * @UART_ERROR_SE_RESOURCES_ON_FAIL: used when serial engine resources on
 *  failed
 * @UART_ERROR_SE_RESOURCES_OFF_FAIL: used when serial engine resources off
 *  failed
 * @UART_ERROR_TX_DMA_MAP_FAIL: used when dma preparation for tx fails
 * @UART_ERROR_TX_CANCEL_FAIL: used when Tx cancel command fails
 * @UART_ERROR_TX_ABORT_FAIL: used when Tx abort command fails
 * @UART_ERROR_TX_FSM_RESET_FAIL: used when Tx FSM reset fails
 * @UART_ERROR_RX_CANCEL_FAIL: used when Rx cancel command fails
 * @UART_ERROR_RX_ABORT_FAIL: used when Rx abort command fails
 * @UART_ERROR_RX_FSM_RESET_FAIL: used when Rx FSM reset fails
 * @UART_ERROR_RX_TTY_INSERT_FAIL: used when there is error in inserting
 *  block of characters in tty flip buffer
 * @UART_ERROR_ILLEGAL_INTERRUPT: used when command with illegal opcode
 *  is encountered by hardware
 * @UART_ERROR_BUFFER_OVERRUN: used when HW writes to a full RX FIFO
 * @UART_ERROR_RX_PARITY_ERROR: used when Rx parity error encountered
 * @UART_ERROR_RX_BREAK_ERROR: used when Rx break error encountered
 * @UART_ERROR_RX_SBE_ERROR: used when AHB bus error encountered during
 *  DMA Rx transaction
 * @SOC_ERROR_START_TX_IOS_SOC_RFR_HIGH: used when SOC RFR is high
 *  and SOC is not ready to receive data
 * @UART_ERROR_FLOW_OFF: used to indicate when UART is not ready to
 *  receive data and flow is turned off
 * @UART_ERROR_RX_FRAMING_ERR: used when Rx framing error encountered
 */
enum uart_error_code {
	UART_ERROR_DEFAULT = 0,
	UART_ERROR_INVALID_FW_LOADED = 1,
	UART_ERROR_CLK_GET_FAIL = 2,
	UART_ERROR_SE_CLK_RATE_FIND_FAIL = 3,
	UART_ERROR_SE_RESOURCES_INIT_FAIL = 4,
	UART_ERROR_SE_RESOURCES_ON_FAIL = 5,
	UART_ERROR_SE_RESOURCES_OFF_FAIL = 6,
	UART_ERROR_TX_DMA_MAP_FAIL = 7,
	UART_ERROR_TX_CANCEL_FAIL = 8,
	UART_ERROR_TX_ABORT_FAIL = 9,
	UART_ERROR_TX_FSM_RESET_FAIL = 10,
	UART_ERROR_RX_CANCEL_FAIL = 11,
	UART_ERROR_RX_ABORT_FAIL = 12,
	UART_ERROR_RX_FSM_RESET_FAIL = 13,
	UART_ERROR_RX_TTY_INSERT_FAIL = 14,
	UART_ERROR_ILLEGAL_INTERRUPT = 15,
	UART_ERROR_BUFFER_OVERRUN = 16,
	UART_ERROR_RX_PARITY_ERROR = 17,
	UART_ERROR_RX_BREAK_ERROR = 18,
	UART_ERROR_RX_SBE_ERROR = 19,
	SOC_ERROR_START_TX_IOS_SOC_RFR_HIGH = 20,
	UART_ERROR_FLOW_OFF = 21,
	UART_ERROR_RX_FRAMING_ERR = 22,

	/* keep last */
	UART_ERROR_CODE_MAX,
};

struct msm_geni_serial_ver_info {
	int hw_major_ver;
	int hw_minor_ver;
	int hw_step_ver;
	int m_fw_ver;
	int s_fw_ver;
};

#ifdef CONFIG_CONSOLE_POLL
/* Number of bytes configured per FIFO word */
#define BYTES_PER_FIFO_WORD 4

struct msm_geni_private_data {
	unsigned int poll_cached_bytes;
	unsigned int poll_cached_bytes_cnt;
};
#endif /* CONFIG_CONSOLE_POLL */

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
	void *ipc_log_new;
	void *console_log;
	void *ipc_log_irqstatus;
	unsigned int cur_baud;
	int ioctl_count;
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
	bool wakeup_enabled;
	struct completion m_cmd_timeout;
	struct completion s_cmd_timeout;
	spinlock_t rx_lock;
	atomic_t is_clock_off;
	enum uart_error_code uart_error;
	unsigned long ser_clk_cfg;
#ifdef CONFIG_CONSOLE_POLL
	struct msm_geni_private_data private_data;
#endif
	atomic_t check_wakeup_byte;
	struct workqueue_struct *wakeup_irq_wq;
	struct delayed_work wakeup_irq_dwork;
	struct completion wakeup_comp;
	atomic_t flush_buffers;
	bool shutdown_in_progress;
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
static bool handle_rx_dma_xfer(u32 s_irq_status, struct uart_port *uport);
static void msm_geni_serial_allow_rx(struct msm_geni_serial_port *port);
static int uart_line_id;

#define GET_DEV_PORT(uport) \
	container_of(uport, struct msm_geni_serial_port, uport)

static struct msm_geni_serial_port msm_geni_console_port;
static struct msm_geni_serial_port msm_geni_serial_ports[GENI_UART_NR_PORTS];
static void msm_geni_serial_handle_isr(struct uart_port *uport,
				unsigned long *flags, bool is_irq_masked);

/*
 * The below API is required to pass UART error code to BT HOST.
 */
static void msm_geni_update_uart_error_code(struct msm_geni_serial_port *port,
		enum uart_error_code uart_error_code)
{
	if (!port->is_console) {
		port->uart_error = uart_error_code;
		UART_LOG_DBG(port->ipc_log_misc, port->uport.dev,
				"%s uart_error_code %d", __func__, port->uart_error);
	}
}

/*
 * The below API is used to enable and disable serial clock divider.
 */
static void msm_geni_enable_disable_se_clk(struct uart_port *uport, bool enable)
{
	struct msm_geni_serial_port *msm_port = GET_DEV_PORT(uport);

	IPC_LOG_MSG(msm_port->ipc_log_misc, "%s: enable:%d ser_clk_cfg:0x%x\n",
			__func__, enable, msm_port->ser_clk_cfg);

	if (enable) {
		geni_write_reg_nolog(msm_port->ser_clk_cfg, uport->membase, GENI_SER_M_CLK_CFG);
		geni_write_reg_nolog(msm_port->ser_clk_cfg, uport->membase, GENI_SER_S_CLK_CFG);
		geni_read_reg_nolog(uport->membase, GENI_SER_M_CLK_CFG);
	} else {
		geni_write_reg_nolog(0x1, uport->membase, GENI_SER_M_CLK_CFG);
		geni_write_reg_nolog(0x1, uport->membase, GENI_SER_S_CLK_CFG);
		geni_read_reg_nolog(uport->membase, GENI_SER_M_CLK_CFG);
	}
}

/*
 * The below API is required to check if uport->lock (spinlock)
 * is taken by the serial layer or not. If the lock is not taken
 * then we can rely on the isr to be fired and if the lock is taken
 * by the serial layer then we need to poll for the interrupts.
 *
 * Returns true(1) if spinlock is already taken by framework (serial layer)
 * Return false(0) if spinlock is not taken by framework.
 */
static bool msm_geni_serial_spinlocked(struct uart_port *uport)
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
	unsigned int geni_dma_tx_irq_en, geni_dma_rx_irq_en;
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

	if (!uart_console(uport)) {
		geni_m_irq_en = geni_read_reg_nolog(uport->membase,
				SE_GENI_M_IRQ_EN);
		geni_s_irq_en = geni_read_reg_nolog(uport->membase,
				SE_GENI_S_IRQ_EN);
		geni_dma_tx_irq_en = geni_read_reg_nolog(uport->membase,
				SE_DMA_TX_IRQ_EN);
		geni_dma_rx_irq_en = geni_read_reg_nolog(uport->membase,
				SE_DMA_RX_IRQ_EN);
		UART_LOG_DBG(port->ipc_log_irqstatus, uport->dev,
			"%s: M_IRQ_EN:0x%x S_IRQ_EN:0x%x TX_IRQ_EN:0x%x RX_IRQ_EN:0x%x\n",
			__func__, geni_m_irq_en, geni_s_irq_en,
			geni_dma_tx_irq_en, geni_dma_rx_irq_en);
	}
}

/* Try disabling interrupts in order to do polling in an atomic contexts. */
static bool msm_serial_try_disable_interrupts(struct uart_port *uport)
{
	unsigned int geni_m_irq_en, geni_s_irq_en;
	unsigned int geni_dma_tx_irq_en, geni_dma_rx_irq_en;
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

	if (!uart_console(uport)) {
		geni_m_irq_en = geni_read_reg_nolog(uport->membase,
				SE_GENI_M_IRQ_EN);
		geni_s_irq_en = geni_read_reg_nolog(uport->membase,
				SE_GENI_S_IRQ_EN);
		geni_dma_tx_irq_en = geni_read_reg_nolog(uport->membase,
				SE_DMA_TX_IRQ_EN);
		geni_dma_rx_irq_en = geni_read_reg_nolog(uport->membase,
				SE_DMA_RX_IRQ_EN);
		UART_LOG_DBG(port->ipc_log_irqstatus, uport->dev,
			"%s: M_IRQ_EN:0x%x S_IRQ_EN:0x%x TX_IRQ_EN:0x%x RX_IRQ_EN:0x%x\n",
			__func__, geni_m_irq_en, geni_s_irq_en,
			geni_dma_tx_irq_en, geni_dma_rx_irq_en);
	}

	return true;
}

/*
 * We need to poll for interrupt if we are in an atomic context
 * as serial framework might be taking spinlocks and depend on the isr
 * in a non-atomic context. This API decides wheather to poll for
 * interrupt or depend on the isr based on in_atomic() call.
 */
static bool geni_wait_for_cmd_done(struct uart_port *uport, bool is_irq_masked)
{
	struct msm_geni_serial_port *msm_port = GET_DEV_PORT(uport);
	unsigned long timeout = POLL_ITERATIONS;
	unsigned long flags = 0;

	/*
	 * We need to do polling if spinlock is taken
	 * by framework as we cannot rely on ISR.
	 */
	if (!uart_console(uport))
		UART_LOG_DBG(msm_port->ipc_log_misc, uport->dev,
				"%s polling:%d\n", __func__, is_irq_masked);
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

	return scnprintf(buf, sizeof(int), "%d\n", port->loopback);
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

static void dump_ipc(struct uart_port *uport, void *ipc_ctx, char *prefix,
			char *string, u64 addr, int size)

{
	char buf[DATA_BYTES_PER_LINE * 2];
	char data[DATA_BYTES_PER_LINE * 3];
	int len = 0;

	len = min(size, DATA_BYTES_PER_LINE);
	hex_dump_to_buffer(string, len, DATA_BYTES_PER_LINE, 1, buf,
						sizeof(buf), false);
	scnprintf(data, sizeof(data), "%s[0x%.10x:%d] : %s", prefix, (unsigned int)addr, size, buf);
	UART_LOG_DBG(ipc_ctx, uport->dev, "%s : %s\n", __func__, data);
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

static int wait_for_transfers_inflight(struct uart_port *uport)
{
	int iter = 0;
	struct msm_geni_serial_port *port = GET_DEV_PORT(uport);
	unsigned int geni_status;
	u32 rx_len_in = 0;

	geni_status = geni_read_reg_nolog(uport->membase, SE_GENI_STATUS);
	/* Possible stop rx is called before this. */
	if (!(geni_status & S_GENI_CMD_ACTIVE))
		return 0;

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
		rx_len_in =
			geni_read_reg_nolog(uport->membase, SE_DMA_RX_LEN_IN);
		if (rx_len_in) {
			UART_LOG_DBG(port->ipc_log_misc, uport->dev,
				"%s: Bailout rx_len_in is set %d\n", __func__, rx_len_in);
			return -EBUSY;
		}
		geni_se_dump_dbg_regs(&port->serial_rsc,
				uport->membase, port->ipc_log_misc);
	}
	return 0;
}

static int vote_clock_on(struct uart_port *uport)
{
	struct msm_geni_serial_port *port = GET_DEV_PORT(uport);
	int usage_count;
	int ret = 0;

	if (port->ioctl_count) {
		UART_LOG_DBG(port->ipc_log_pwr, uport->dev,
			     "%s clock already on\n", __func__);
		return ret;
	}
	ret = msm_geni_serial_power_on(uport);
	if (ret) {
		dev_err(uport->dev, "Failed to vote clock on\n");
		return ret;
	}
	atomic_set(&port->check_wakeup_byte, 0);
	complete(&port->wakeup_comp);
	port->ioctl_count++;
	usage_count = atomic_read(&uport->dev->power.usage_count);
	UART_LOG_DBG(port->ipc_log_pwr, uport->dev,
		     "%s :%s ioctl:%d usage_count:%d\n",
		     __func__, current->comm, port->ioctl_count, usage_count);
	return 0;
}

static int vote_clock_off(struct uart_port *uport)
{
	struct msm_geni_serial_port *port = GET_DEV_PORT(uport);
	int usage_count;
	int ret = 0;

	if (!pm_runtime_enabled(uport->dev)) {
		dev_err(uport->dev, "RPM not available.Can't enable clocks\n");
		return -EPERM;
	}
	if (!port->ioctl_count) {
		dev_warn(uport->dev, "%s:Imbalanced vote off ioctl %d\n",
						__func__, port->ioctl_count);
		UART_LOG_DBG(port->ipc_log_pwr, uport->dev,
			"%s Imbalanced vote_off from userspace. %d",
			__func__, port->ioctl_count);
		return -EPERM;
	}
	ret = wait_for_transfers_inflight(uport);
	if (ret) {
		UART_LOG_DBG(port->ipc_log_pwr, uport->dev,
			"%s wait_for_transfer_inflight return ret: %d", __func__, ret);
		return -EAGAIN;
	}

	port->ioctl_count--;
	msm_geni_serial_power_off(uport);
	usage_count = atomic_read(&uport->dev->power.usage_count);
	UART_LOG_DBG(port->ipc_log_pwr, uport->dev, "%s:%s ioctl:%d usage_count:%d\n",
		__func__, current->comm, port->ioctl_count, usage_count);
	return 0;
};

static int msm_geni_serial_ioctl(struct uart_port *uport, unsigned int cmd,
						unsigned long arg)
{
	struct msm_geni_serial_port *port = GET_DEV_PORT(uport);
	int ret = -ENOIOCTLCMD;
	enum uart_error_code uart_error;

	switch (cmd) {
	case MSM_GENI_SERIAL_TIOCPMGET: {
		ret = vote_clock_on(uport);
		break;
	}
	case MSM_GENI_SERIAL_TIOCPMPUT: {
		ret = vote_clock_off(uport);
		break;
	}
	case MSM_GENI_SERIAL_TIOCPMACT: {
		ret = !pm_runtime_status_suspended(uport->dev);
		break;
	}
	case MSM_GENI_SERIAL_TIOCFAULT: {
		uart_error = port->uart_error;
		port->uart_error = UART_ERROR_DEFAULT;
		UART_LOG_DBG(port->ipc_log_misc, uport->dev,
			"%s TIOCFAULT - uart_error_set %d new_uart_error %d",
			__func__, uart_error, port->uart_error);
		ret = uart_error;

		if (!device_pending_suspend(uport))
			geni_se_dump_dbg_regs(&port->serial_rsc,
					      uport->membase, port->ipc_log_misc);
		/* Do not use previous log file from this issue point */
		port->ipc_log_rx = port->ipc_log_new;
		port->ipc_log_tx = port->ipc_log_new;
		port->ipc_log_misc = port->ipc_log_new;
		port->ipc_log_pwr = port->ipc_log_new;
		port->ipc_log_irqstatus = port->ipc_log_new;
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
	int ret = 0;

	if (!uart_console(uport) && device_pending_suspend(uport)) {
		UART_LOG_DBG(port->ipc_log_misc, uport->dev,
			"%s.Device is suspended, %s\n",
			__func__, current->comm);
		return;
	}

	if (ctl) {
		ret = wait_for_transfers_inflight(uport);
		if (ret)
			UART_LOG_DBG(port->ipc_log_misc, uport->dev,
				  "%s.wait_for_transfer_inflight return ret: %d\n",
				__func__, ret);
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
		UART_LOG_DBG(port->ipc_log_misc, uport->dev,
				"%s.Device is suspended, %s\n",
				__func__, current->comm);
		return TIOCM_DSR | TIOCM_CAR | TIOCM_CTS;
	}

	geni_ios = geni_read_reg_nolog(uport->membase, SE_GENI_IOS);
	if (!(geni_ios & IO2_DATA_IN))
		mctrl |= TIOCM_CTS;
	else
		msm_geni_update_uart_error_code(port, SOC_ERROR_START_TX_IOS_SOC_RFR_HIGH);

	if (!port->manual_flow)
		mctrl |= TIOCM_RTS;

	UART_LOG_DBG(port->ipc_log_misc, uport->dev, "%s: geni_ios:0x%x, mctrl:0x%x\n",
		__func__, geni_ios, mctrl);
	return mctrl;
}

static void msm_geni_cons_set_mctrl(struct uart_port *uport,
							unsigned int mctrl)
{
}

/*
 * msm_geni_serial_set_mctrl() - Configures control lines of uart port
 *
 * @uport: pointer to uart port
 * @mctrl: contains control line configuration
 *
 * Return: None
 */
static void msm_geni_serial_set_mctrl(struct uart_port *uport,
				      unsigned int mctrl)
{
	u32 uart_manual_rfr = 0;
	struct msm_geni_serial_port *port = GET_DEV_PORT(uport);

	if (device_pending_suspend(uport)) {
		UART_LOG_DBG(port->ipc_log_misc, uport->dev,
			     "%s.Device is suspended, %s: mctrl=0x%x\n",
			     __func__, current->comm, mctrl);
		return;
	}

	if (!(mctrl & TIOCM_RTS)) {
		uart_manual_rfr |= (UART_MANUAL_RFR_EN);
		geni_write_reg_nolog(uart_manual_rfr, port->uport.membase,
				     SE_UART_MANUAL_RFR);
		/* UART FW needs delay per HW experts recommendation */
		udelay(10);
		uart_manual_rfr |= (UART_RFR_NOT_READY);
		geni_write_reg_nolog(uart_manual_rfr, port->uport.membase,
				     SE_UART_MANUAL_RFR);
		port->manual_flow = true;
	} else {
		geni_write_reg_nolog(uart_manual_rfr, uport->membase,
				     SE_UART_MANUAL_RFR);
		port->manual_flow = false;
	}

	/* Write to flow control must complete before return to client*/
	mb();

	if (port->manual_flow) {
		/* Set error code UART_ERROR_FLOW_OFF indicating manual rfr
		 * is enabled
		 */
		msm_geni_update_uart_error_code(port, UART_ERROR_FLOW_OFF);
	} else if (port->uart_error == UART_ERROR_FLOW_OFF) {
		/* Reset the prev err code since manual rfr is now disabled */
		msm_geni_update_uart_error_code(port, UART_ERROR_DEFAULT);
	}

	UART_LOG_DBG(port->ipc_log_misc, uport->dev,
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

			UART_LOG_DBG(port->ipc_log_pwr, uport->dev,
					"%s:Manual resume\n", __func__);
			pm_runtime_disable(uport->dev);
			ret = msm_geni_serial_runtime_resume(uport->dev);
			if (ret) {
				UART_LOG_DBG(port->ipc_log_pwr, uport->dev,
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
			UART_LOG_DBG(port->ipc_log_pwr, uport->dev, "%s Err\n", __func__);
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
		UART_LOG_DBG(port->ipc_log_pwr, uport->dev, "%s: Usage Count is already 0\n",
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
	struct msm_geni_serial_port *port = GET_DEV_PORT(uport);
	struct msm_geni_private_data *private_data = &port->private_data;
	unsigned int rx_fifo;
	unsigned int m_irq_status;
	unsigned int s_irq_status;
	unsigned int word_count, rx_fifo_status;

	if (!private_data->poll_cached_bytes_cnt) {
		m_irq_status = geni_read_reg_nolog(uport->membase,
						   SE_GENI_M_IRQ_STATUS);
		s_irq_status = geni_read_reg_nolog(uport->membase,
						   SE_GENI_S_IRQ_STATUS);
		geni_write_reg_nolog(m_irq_status, uport->membase,
				     SE_GENI_M_IRQ_CLEAR);
		geni_write_reg_nolog(s_irq_status, uport->membase,
				     SE_GENI_S_IRQ_CLEAR);
		/*
		 * Read the Rx FIFO only after clearing the interrupt registers and
		 * getting valid RX fifo status.
		 */
		rx_fifo_status = geni_read_reg_nolog(uport->membase,
						     SE_GENI_RX_FIFO_STATUS);

		word_count = rx_fifo_status & RX_FIFO_WC_MSK;
		if (!word_count)
			return NO_POLL_CHAR;
		if (word_count == 1 &&  (rx_fifo_status & RX_LAST))
			/*
			 * NOTE: If RX_LAST_BYTE_VALID is 0 it needs to be
			 * treated as if it was BYTES_PER_FIFO_WORD.
			 */
			private_data->poll_cached_bytes_cnt =
				(rx_fifo_status & RX_LAST_BYTE_VALID_MSK) >>
				RX_LAST_BYTE_VALID_SHFT;

		if (private_data->poll_cached_bytes_cnt == 0)
			private_data->poll_cached_bytes_cnt = BYTES_PER_FIFO_WORD;
		private_data->poll_cached_bytes =
			geni_read_reg_nolog(uport->membase, SE_GENI_RX_FIFOn);
	}

	private_data->poll_cached_bytes_cnt--;
	rx_fifo = private_data->poll_cached_bytes & 0xff;
	private_data->poll_cached_bytes >>= 8;
	return rx_fifo;
}

static void msm_geni_serial_poll_put_char(struct uart_port *uport,
					  unsigned char c)
{
	int b = (int) c;

	geni_write_reg_nolog(DEF_TX_WM, uport->membase,
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
	msm_geni_serial_poll_tx_done(uport);
}
#endif /* CONFIG_CONSOLE_POLL */

#if IS_ENABLED(CONFIG_SERIAL_MSM_GENI_CONSOLE) || \
					IS_ENABLED(CONFIG_CONSOLE_POLL)
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
			geni_se_dump_dbg_regs(&port->serial_rsc, uport->membase,
					      port->ipc_log_misc);
			reinit_completion(&port->m_cmd_timeout);
			geni_abort_m_cmd(uport->membase);
			timeout = geni_wait_for_cmd_done(uport, true);
			if (timeout) {
				IPC_LOG_MSG(port->console_log,
				"%s: tx abort failed 0x%x\n", __func__,
				geni_read_reg_nolog(uport->membase,
				SE_GENI_STATUS));
				geni_se_dump_dbg_regs(&port->serial_rsc,
						      uport->membase,
						      port->ipc_log_misc);
			}

			msm_geni_serial_allow_rx(port);
			geni_write_reg(FORCE_DEFAULT, uport->membase,
					GENI_FORCE_DEFAULT_REG);
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

#endif /* (CONFIG_SERIAL_MSM_GENI_CONSOLE) || defined(CONFIG_CONSOLE_POLL)) */

static int msm_geni_serial_prep_dma_tx(struct uart_port *uport)
{
	struct msm_geni_serial_port *msm_port = GET_DEV_PORT(uport);
	struct circ_buf *xmit = &uport->state->xmit;
	unsigned int xmit_size;
	unsigned int dma_dbg;
	bool timeout, is_irq_masked;
	int ret = 0;

	if (atomic_read(&msm_port->flush_buffers))
		return -EIO;
	xmit_size = uart_circ_chars_pending(xmit);
	if (xmit_size < WAKEUP_CHARS)
		uart_write_wakeup(uport);

	if (xmit_size > (UART_XMIT_SIZE - xmit->tail))
		xmit_size = UART_XMIT_SIZE - xmit->tail;

	if (!xmit_size)
		return -EPERM;

	dump_ipc(uport, msm_port->ipc_log_tx, "DMA Tx",
		 (char *)&xmit->buf[xmit->tail], 0, xmit_size);
	msm_geni_serial_setup_tx(uport, xmit_size);
	ret = geni_se_tx_dma_prep(msm_port->wrapper_dev, uport->membase,
			&xmit->buf[xmit->tail], xmit_size, &msm_port->tx_dma);

	if (!ret) {
		msm_port->xmit_size = xmit_size;
	} else {
		UART_LOG_DBG(msm_port->ipc_log_misc, uport->dev,
		    "%s: TX DMA map Fail %d\n", __func__, ret);

		msm_geni_update_uart_error_code(msm_port, UART_ERROR_TX_DMA_MAP_FAIL);

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

			UART_LOG_DBG(msm_port->ipc_log_misc, uport->dev,
			"%s: tx_cancel failed 0x%x\n", __func__,
			geni_read_reg_nolog(uport->membase, SE_GENI_STATUS));
			msm_geni_update_uart_error_code(msm_port, UART_ERROR_TX_CANCEL_FAIL);
			geni_se_dump_dbg_regs(&msm_port->serial_rsc, uport->membase,
					      msm_port->ipc_log_misc);

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
				UART_LOG_DBG(msm_port->ipc_log_misc, uport->dev,
				"%s: tx abort failed 0x%x\n", __func__,
				geni_read_reg_nolog(uport->membase,
							SE_GENI_STATUS));
				msm_geni_update_uart_error_code(msm_port, UART_ERROR_TX_ABORT_FAIL);
				geni_se_dump_dbg_regs(&msm_port->serial_rsc, uport->membase,
						      msm_port->ipc_log_misc);
			} else {
				/* Reset the CANCEL error code if abort is success */
				msm_geni_update_uart_error_code(msm_port, UART_ERROR_DEFAULT);
			}

			msm_geni_serial_allow_rx(msm_port);
			geni_write_reg(FORCE_DEFAULT, uport->membase,
					GENI_FORCE_DEFAULT_REG);
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
				if (timeout) {
					UART_LOG_DBG(msm_port->ipc_log_misc, uport->dev,
					"%s: tx fsm reset failed\n", __func__);
					msm_geni_update_uart_error_code(msm_port,
						UART_ERROR_TX_FSM_RESET_FAIL);
					geni_se_dump_dbg_regs(&msm_port->serial_rsc, uport->membase,
							      msm_port->ipc_log_misc);
				} else {
					/* Reset Cancel/Abort error code if FSM reset is success */
					msm_geni_update_uart_error_code(msm_port,
									UART_ERROR_DEFAULT);
				}
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

	/* when start_tx is called with UART clocks OFF return. */
	if (uart_console(uport) && (uport->suspended || atomic_read(&msm_port->is_clock_off))) {
		IPC_LOG_MSG(msm_port->console_log,
			"%s. Console in suspend state\n", __func__);
		return;
	}

	if (!uart_console(uport) && !pm_runtime_active(uport->dev)) {
		UART_LOG_DBG(msm_port->ipc_log_misc, uport->dev,
			"%s.Putting in async RPM vote\n", __func__);
		pm_runtime_get(uport->dev);
		goto exit_start_tx;
	}

	if (!uart_console(uport)) {
		UART_LOG_DBG(msm_port->ipc_log_misc, uport->dev,
			"%s.Power on.\n", __func__);
		pm_runtime_get(uport->dev);
	}
	/*
	 * If flush has been triggered earlier from userspace and port is
	 * still active(not yet closed) then reset the flush_buffers flag
	 */
	if (atomic_read(&msm_port->flush_buffers))
		atomic_set(&msm_port->flush_buffers, 0);

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

		if (msm_geni_serial_prep_dma_tx(uport) == -EPERM) {
			UART_LOG_DBG(msm_port->ipc_log_tx, uport->dev, "%s: tx_en=0,\n",
								__func__);
			goto exit_start_tx;
		}
	}
	return;
check_flow_ctrl:
	geni_ios = geni_read_reg_nolog(uport->membase, SE_GENI_IOS);
	if (++ios_log_limit % 5 == 0) {
		UART_LOG_DBG(msm_port->ipc_log_misc, uport->dev, "%s: ios: 0x%x\n",
						__func__, geni_ios);

		/* check if SOC RFR is high and set the error code */
		if (geni_ios & IO2_DATA_IN)
			msm_geni_update_uart_error_code(msm_port,
				SOC_ERROR_START_TX_IOS_SOC_RFR_HIGH);

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

	UART_LOG_DBG(port->ipc_log_misc, uport->dev,
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
		UART_LOG_DBG(port->ipc_log_misc, uport->dev, "%s: tx_cancel failed 0x%x\n",
		__func__, geni_read_reg_nolog(uport->membase, SE_GENI_STATUS));
		msm_geni_update_uart_error_code(port, UART_ERROR_TX_CANCEL_FAIL);
		geni_se_dump_dbg_regs(&port->serial_rsc, uport->membase,
				      port->ipc_log_misc);
		port->m_cmd_done = false;
		reinit_completion(&port->m_cmd_timeout);
		geni_abort_m_cmd(uport->membase);

		timeout = geni_wait_for_cmd_done(uport, is_irq_masked);
		if (timeout) {
			IPC_LOG_MSG(port->console_log,
				"%s: tx abort failed 0x%x\n", __func__,
			geni_read_reg_nolog(uport->membase, SE_GENI_STATUS));
			UART_LOG_DBG(port->ipc_log_misc, uport->dev,
				"%s: tx abort failed 0x%x\n", __func__,
					geni_read_reg_nolog(uport->membase, SE_GENI_STATUS));
			msm_geni_update_uart_error_code(port, UART_ERROR_TX_ABORT_FAIL);
			geni_se_dump_dbg_regs(&port->serial_rsc, uport->membase,
					      port->ipc_log_misc);
		}
		msm_geni_serial_allow_rx(port);
		geni_write_reg(FORCE_DEFAULT, uport->membase,
					GENI_FORCE_DEFAULT_REG);
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
			if (timeout) {
				UART_LOG_DBG(port->ipc_log_misc, uport->dev,
				"%s: tx fsm reset failed\n", __func__);
				msm_geni_update_uart_error_code(port, UART_ERROR_TX_FSM_RESET_FAIL);
			}
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
		UART_LOG_DBG(port->ipc_log_misc, uport->dev, "%s:Removing vote\n", __func__);
		msm_geni_serial_power_off(uport);
	}

	geni_status = geni_read_reg_nolog(uport->membase, SE_GENI_STATUS);
	UART_LOG_DBG(port->ipc_log_misc, uport->dev, "%s: End GENI:0x%x\n",
		    __func__, geni_status);
}

static void msm_geni_serial_stop_tx(struct uart_port *uport)
{
	struct msm_geni_serial_port *port = GET_DEV_PORT(uport);

	if (!uart_console(uport) && device_pending_suspend(uport)) {
		dev_err(uport->dev, "%s.Device is suspended.\n", __func__);
		UART_LOG_DBG(port->ipc_log_misc, uport->dev,
				"%s.Device is suspended.\n", __func__);
		return;
	}
	stop_tx_sequencer(uport);
}

static void start_rx_sequencer(struct uart_port *uport)
{
	unsigned int geni_status;
	struct msm_geni_serial_port *port = GET_DEV_PORT(uport);

	if (port->startup_in_progress)
		return;

	geni_status = geni_read_reg_nolog(uport->membase, SE_GENI_STATUS);
	UART_LOG_DBG(port->ipc_log_misc, uport->dev, "%s: geni_status 0x%x\n",
		    __func__, geni_status);

	if (geni_status & S_GENI_CMD_ACTIVE) {
		if (port->xfer_mode == SE_DMA) {
			UART_LOG_DBG(port->ipc_log_misc, uport->dev,
				"%s: mapping rx dma GENI: 0x%x\n",
				__func__, geni_status);
			geni_se_rx_dma_start(uport->membase, DMA_RX_BUF_SIZE,
								&port->rx_dma);
		}
		msm_geni_serial_stop_rx(uport);
	}

	if (port->xfer_mode == SE_DMA) {
		UART_LOG_DBG(port->ipc_log_misc, uport->dev,
			"%s. mapping rx dma\n", __func__);
		geni_se_rx_dma_start(uport->membase, DMA_RX_BUF_SIZE,
							&port->rx_dma);
	}

	/* Start RX with the RFR_OPEN to keep RFR in always ready state. */
	geni_setup_s_cmd(uport->membase, UART_START_READ, UART_PARAM_RFR_OPEN);
	msm_geni_serial_enable_interrupts(uport);

	/* Ensure that the above writes go through */
	mb();
	geni_status = geni_read_reg_nolog(uport->membase, SE_GENI_STATUS);
	UART_LOG_DBG(port->ipc_log_misc, uport->dev,
		"%s: geni_status 0x%x, dma_dbg:0x%x\n", __func__,
		geni_status, geni_read_reg(uport->membase, SE_DMA_DEBUG_REG0));
}

static void msm_geni_serial_start_rx(struct uart_port *uport)
{
	struct msm_geni_serial_port *port = GET_DEV_PORT(uport);

	if (!uart_console(uport) && device_pending_suspend(uport)) {
		dev_err(uport->dev, "%s.Device is suspended.\n", __func__);
		UART_LOG_DBG(port->ipc_log_misc, uport->dev,
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
		UART_LOG_DBG(port->ipc_log_misc, port->uport.dev,
			"%s: Manual Flow Enabled, HW Flow OFF rfr = 0x%x\n",
			__func__, uart_manual_rfr);
		msm_geni_update_uart_error_code(port, UART_ERROR_FLOW_OFF);
	} else {
		geni_write_reg_nolog(0, port->uport.membase,
						SE_UART_MANUAL_RFR);
		/* Ensure that the manual flow off writes go through */
		mb();
		uart_manual_rfr = geni_read_reg_nolog(port->uport.membase,
							SE_UART_MANUAL_RFR);
		UART_LOG_DBG(port->ipc_log_misc, port->uport.dev,
			"%s: Manual Flow Disabled, HW Flow ON rfr = 0x%x\n",
						__func__, uart_manual_rfr);
		if (port->uart_error == UART_ERROR_FLOW_OFF)
			msm_geni_update_uart_error_code(port, UART_ERROR_DEFAULT);
	}
}

static int stop_rx_sequencer(struct uart_port *uport)
{
	unsigned int geni_status;
	bool timeout, is_irq_masked;
	struct msm_geni_serial_port *port = GET_DEV_PORT(uport);
	unsigned long flags = 0;
	bool is_rx_active;
	u32 dma_rx_status, s_irq_status, stale_delay;
	int usage_count;

	UART_LOG_DBG(port->ipc_log_misc, uport->dev, "%s\n", __func__);

	geni_status = geni_read_reg_nolog(uport->membase, SE_GENI_STATUS);
	/* Possible stop rx is called multiple times. */
	if (!(geni_status & S_GENI_CMD_ACTIVE)) {
		UART_LOG_DBG(port->ipc_log_misc, uport->dev,
			"%s: RX is Inactive, geni_sts: 0x%x\n",
						__func__, geni_status);
		return 0;
	}

	if (!uart_console(uport)) {
		/*
		 * Enable SW flow control and pull RFR line HIGH before doing stop_rx.
		 * This is to prevent any rx data to come into the uart rx fifo
		 * when stop_rx is being done.
		 */
		msm_geni_serial_set_manual_flow(false, port);
		/*
		 * Wait for the stale timeout to happen if there is any data
		 * pending in the rx fifo.
		 * Have a safety factor of 2 to include the interrupt and
		 * system latencies, add 500usec delay for interrupt latency
		 * or system delay.
		 * This will help to handle incoming rx data in stop_rx_sequencer
		 * for interrupt latency or system delay cases.
		 */
		stale_delay = (STALE_COUNT * SEC_TO_USEC) / port->cur_baud;
		stale_delay = (2 * stale_delay) + SYSTEM_DELAY;
		if (stale_delay > STALE_DELAY_MAX)
			stale_delay = STALE_DELAY_MAX;
		UART_LOG_DBG(port->ipc_log_misc, uport->dev,
			     "stale_delay = %d usecs\n", stale_delay);
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
			UART_LOG_DBG(port->ipc_log_misc, uport->dev, "%s: Interrupt delay\n",
					__func__);
			handle_rx_dma_xfer(s_irq_status, uport);
			if (pm_runtime_enabled(uport->dev) && !port->ioctl_count) {
				usage_count = atomic_read(&uport->dev->power.usage_count);
				UART_LOG_DBG(port->ipc_log_misc, uport->dev,
					"%s: Abort Stop Rx, extend the PM timer, usage_count:%d\n",
					__func__, usage_count);
				pm_runtime_mark_last_busy(uport->dev);
				return -EBUSY;
			}
		}
	}

	UART_LOG_DBG(port->ipc_log_misc, uport->dev, "%s: Start 0x%x\n",
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
	UART_LOG_DBG(port->ipc_log_misc, uport->dev,
		"%s: geni_status 0x%x, dma_dbg:0x%x\n", __func__,
		geni_status, geni_read_reg(uport->membase, SE_DMA_DEBUG_REG0));

	if (timeout || is_rx_active) {
		UART_LOG_DBG(port->ipc_log_misc, uport->dev,
			    "%s cancel failed timeout:%d is_rx_active:%d 0x%x\n",
			    __func__, timeout, is_rx_active, geni_status);
		IPC_LOG_MSG(port->console_log,
				"%s cancel failed timeout:%d is_rx_active:%d 0x%x\n",
				__func__, timeout, is_rx_active, geni_status);

		msm_geni_update_uart_error_code(port,
			UART_ERROR_RX_CANCEL_FAIL);

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
			UART_LOG_DBG(port->ipc_log_misc, uport->dev,
				"%s abort fail timeout:%d is_rx_active:%d 0x%x\n",
				__func__, timeout, is_rx_active, geni_status);
			IPC_LOG_MSG(port->console_log,
				"%s abort fail timeout:%d is_rx_active:%d 0x%x\n",
				 __func__, timeout, is_rx_active, geni_status);

			msm_geni_update_uart_error_code(port,
				UART_ERROR_RX_ABORT_FAIL);

			geni_se_dump_dbg_regs(&port->serial_rsc,
				uport->membase, port->ipc_log_misc);
		} else {
			/* Reset the CANCEL error code if abort is success */
			msm_geni_update_uart_error_code(port, UART_ERROR_DEFAULT);
		}

		/*
		 * Do not override client requested manual_flow using set_mctrl
		 * else driver can override client configured flow.
		 */
		if (!uart_console(uport) && !port->manual_flow)
			msm_geni_serial_allow_rx(port);
		geni_write_reg(FORCE_DEFAULT, uport->membase,
					GENI_FORCE_DEFAULT_REG);

		if (port->xfer_mode == SE_DMA) {
			port->s_cmd_done = false;
			reinit_completion(&port->s_cmd_timeout);
			geni_write_reg_nolog(1, uport->membase,
						SE_DMA_RX_FSM_RST);

			timeout = geni_wait_for_cmd_done(uport,
							 is_irq_masked);
			if (timeout) {
				UART_LOG_DBG(port->ipc_log_misc, uport->dev,
				"%s: rx fsm reset failed\n", __func__);
				msm_geni_update_uart_error_code(port, UART_ERROR_RX_FSM_RESET_FAIL);

				geni_se_dump_dbg_regs(&port->serial_rsc,
					uport->membase, port->ipc_log_misc);

			} else {
				/* Reset Cancel/Abort error code if FSM reset is success */
				msm_geni_update_uart_error_code(port, UART_ERROR_DEFAULT);
			}
		}
	}
	/* Enable the interrupts once the cancel operation is done. */
	msm_geni_serial_enable_interrupts(uport);
	port->s_cmd = false;

exit_rx_seq:
	/*
	 * Do not override client requested manual_flow using set_mctrl
	 * else driver can override client configured flow.
	 */
	if (!uart_console(uport) && !port->manual_flow)
		msm_geni_serial_set_manual_flow(true, port);

	geni_status = geni_read_reg_nolog(uport->membase, SE_GENI_STATUS);
	UART_LOG_DBG(port->ipc_log_misc, uport->dev,
		"%s: End geni_status : 0x%x dma_dbg:0x%x\n", __func__,
		geni_status, geni_read_reg(uport->membase, SE_DMA_DEBUG_REG0));

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
		UART_LOG_DBG(port->ipc_log_misc, uport->dev,
				"%s.Device is suspended.\n", __func__);
		return;
	}
	ret = stop_rx_sequencer(uport);
	if (ret)
		UART_LOG_DBG(port->ipc_log_misc, uport->dev, "%s: stop rx failed %d\n",
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
	dump_ipc(uport, msm_port->ipc_log_rx, "Rx", (char *)msm_port->rx_fifo, 0,
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
	pending = active ? msm_port->cur_tx_remaining :
				uart_circ_chars_pending(xmit);

	/* All data has been transmitted and acknowledged as received */
	if (!pending && !tx_fifo_status && done)
		goto exit_handle_tx;

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
	irq_en = geni_read_reg_nolog(uport->membase, SE_GENI_M_IRQ_EN);
	if (!msm_port->cur_tx_remaining)
		/* Clear WM interrupt post each transfer completion */
		geni_write_reg_nolog(irq_en & ~M_TX_FIFO_WATERMARK_EN,
					uport->membase, SE_GENI_M_IRQ_EN);

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(uport);
	return 0;
}

/*
 * msm_geni_find_wakeup_byte() - Checks if wakeup byte is present
 * in rx buffer
 *
 * @uport: pointer to uart port
 * @size: size of rx data
 *
 * Return: true if wakeup byte found else false
 */
static bool msm_geni_find_wakeup_byte(struct uart_port *uport, int size)
{
	struct msm_geni_serial_port *port = GET_DEV_PORT(uport);
	unsigned char *buf = (unsigned char *)port->rx_buf;

	if (buf[0] == port->wakeup_byte) {
		UART_LOG_DBG(port->ipc_log_rx, uport->dev,
			     "%s Found wakeup byte\n", __func__);
		atomic_set(&port->check_wakeup_byte, 0);
		return true;
	}
	dump_ipc(uport, port->ipc_log_rx, "Dropped Rx", buf, 0, size);

	return false;
}

static void check_rx_buf(char *buf, struct uart_port *uport, int size)
{
	struct msm_geni_serial_port *msm_port = GET_DEV_PORT(uport);
	unsigned int rx_data;
	bool fault = false;

	rx_data = *(u32 *)buf;
	/* check for first 4 bytes of RX data for faulty zero pattern */
	if (rx_data == 0x0) {
		if (size <= 4) {
			fault = true;
		} else {
			/*
			 * check for last 4 bytes of data in RX buffer for
			 * faulty pattern
			 */
			if (memcmp(buf+(size-4), "\x0\x0\x0\x0", 4) == 0)
				fault = true;
		}

		if (fault) {
			UART_LOG_DBG(msm_port->ipc_log_rx, uport->dev,
				"%s RX Invalid packet\n", __func__);
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
		if (drop_rx) {
			rx_bytes = geni_read_reg_nolog(uport->membase, SE_DMA_RX_LEN_IN);
			dump_ipc(uport, msm_port->ipc_log_rx,
				 "Dropping Error Rx Data",
				 (char *)msm_port->rx_buf, 0, rx_bytes);
		}
		UART_LOG_DBG(msm_port->ipc_log_misc, uport->dev,
			    "%s: GENI: 0x%x\n", __func__, geni_status);
		return 0;
	}

	if (unlikely(!msm_port->rx_buf)) {
		UART_LOG_DBG(msm_port->ipc_log_rx, uport->dev, "%s: NULL Rx_buf\n",
								__func__);
		return 0;
	}

	rx_bytes = geni_read_reg_nolog(uport->membase, SE_DMA_RX_LEN_IN);
	if (unlikely(!rx_bytes)) {
		UART_LOG_DBG(msm_port->ipc_log_rx, uport->dev, "%s: Size %d\n",
					__func__, rx_bytes);
		return 0;
	}

	/* Check RX buffer data for faulty pattern*/
	check_rx_buf((char *)msm_port->rx_buf, uport, rx_bytes);

	if (drop_rx) {
		dump_ipc(uport, msm_port->ipc_log_rx,
			 "Dropping Error Rx Data",
			 (char *)msm_port->rx_buf, 0, rx_bytes);
		return 0;
	}

	if (atomic_read(&msm_port->check_wakeup_byte)) {
		ret = msm_geni_find_wakeup_byte(uport, rx_bytes);
		if (!ret) {
			/* wakeup byte not found, drop the rx data */
			UART_LOG_DBG(msm_port->ipc_log_rx, uport->dev,
				     "%s dropping Rx data as wakeup byte not found in %d bytes\n",
				     __func__, rx_bytes);
			memset(msm_port->rx_buf, 0, rx_bytes);
			return 0;
		}
	}

	tport = &uport->state->port;
	ret = tty_insert_flip_string(tport, (unsigned char *)(msm_port->rx_buf), rx_bytes);
	if (ret != rx_bytes) {
		UART_LOG_DBG(msm_port->ipc_log_rx, uport->dev, "%s: ret %d rx_bytes %d\n",
			     __func__, ret, rx_bytes);
		msm_geni_update_uart_error_code(msm_port, UART_ERROR_RX_TTY_INSERT_FAIL);
		WARN_ON_ONCE(1);
	}
	uport->icount.rx += ret;
	tty_flip_buffer_push(tport);
	dump_ipc(uport, msm_port->ipc_log_rx, "DMA Rx", (char *)msm_port->rx_buf, 0, rx_bytes);
	/*
	 * DMA_DONE interrupt doesn't confirm that the DATA is copied to
	 * DDR memory, sometimes we are queuing the stale data from previous
	 * transfer to tty flip_buffer, adding memset to zero
	 * change to idenetify such scenario.
	 */
	memset(msm_port->rx_buf, 0, rx_bytes);
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
			UART_LOG_DBG(msm_port->ipc_log_misc, uport->dev,
				"%s.Tx sent out, Power off\n", __func__);
			msm_geni_serial_power_off(uport);
		}
		uart_write_wakeup(uport);
	}
	return 0;
}

static bool handle_tx_fifo_xfer(u32 m_irq_status, struct uart_port *uport)
{
	bool ret = false;
	u32 geni_status = geni_read_reg_nolog(uport->membase, SE_GENI_STATUS);
	u32 m_irq_en = geni_read_reg_nolog(uport->membase, SE_GENI_M_IRQ_EN);

	if ((m_irq_status & m_irq_en) &
	    (M_TX_FIFO_WATERMARK_EN | M_CMD_DONE_EN))
		msm_geni_serial_handle_tx(uport,
				m_irq_status & M_CMD_DONE_EN,
				geni_status & M_GENI_CMD_ACTIVE);

	if (m_irq_status & (M_CMD_CANCEL_EN | M_CMD_ABORT_EN))
		ret = true;

	return ret;
}

static bool handle_rx_fifo_xfer(u32 s_irq_status, struct uart_port *uport,
				unsigned long *flags, bool is_irq_masked)
{
	bool ret = false;
	bool drop_rx = false;
	struct tty_port *tport = &uport->state->port;
	struct msm_geni_serial_port *msm_port = GET_DEV_PORT(uport);

	if (s_irq_status & (S_GP_IRQ_0_EN | S_GP_IRQ_1_EN)) {
		if (s_irq_status & S_GP_IRQ_0_EN)
			uport->icount.parity++;
		UART_LOG_DBG(msm_port->ipc_log_misc, uport->dev,
			"%s.sirq 0x%x parity:%d\n",
			__func__, s_irq_status, uport->icount.parity);
		drop_rx = true;
	} else if (s_irq_status & (S_GP_IRQ_2_EN | S_GP_IRQ_3_EN)) {
		uport->icount.brk++;
		UART_LOG_DBG(msm_port->ipc_log_misc, uport->dev,
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
		ret = true;
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

	return ret;
}

static bool handle_tx_dma_xfer(u32 m_irq_status, struct uart_port *uport)
{
	struct msm_geni_serial_port *msm_port = GET_DEV_PORT(uport);
	bool ret = false;
	u32 dma_tx_status = geni_read_reg_nolog(uport->membase,
							SE_DMA_TX_IRQ_STAT);

	if (dma_tx_status) {
		geni_write_reg_nolog(dma_tx_status, uport->membase,
					SE_DMA_TX_IRQ_CLR);

		if (dma_tx_status & (TX_RESET_DONE | TX_GENI_CANCEL_IRQ))
			return true;

		if (dma_tx_status & TX_DMA_DONE) {
			msm_geni_serial_handle_dma_tx(uport);

			if (msm_port->uart_error == SOC_ERROR_START_TX_IOS_SOC_RFR_HIGH) {
				/* Reset SOC_RFR_HIGH error code if DMA TX is success */
				msm_geni_update_uart_error_code(msm_port, UART_ERROR_DEFAULT);
			}
		}
	}

	if (m_irq_status & (M_CMD_CANCEL_EN | M_CMD_ABORT_EN))
		ret = true;

	return ret;
}

static bool handle_rx_dma_xfer(u32 s_irq_status, struct uart_port *uport)
{
	bool ret = false;
	bool drop_rx = false;
	struct msm_geni_serial_port *msm_port = GET_DEV_PORT(uport);
	u32 dma_rx_status;
	unsigned long lock_flags;

	spin_lock_irqsave(&msm_port->rx_lock, lock_flags);
	dma_rx_status = geni_read_reg_nolog(uport->membase, SE_DMA_RX_IRQ_STAT);

	if (dma_rx_status) {
		geni_write_reg_nolog(dma_rx_status, uport->membase,
				     SE_DMA_RX_IRQ_CLR);

		if (dma_rx_status & RX_RESET_DONE) {
			UART_LOG_DBG(msm_port->ipc_log_misc, uport->dev,
				     "%s Rx Reset done dma_rx_status=0x%x\n",
				     __func__, dma_rx_status);
			ret = true;
		}

		if (dma_rx_status & UART_DMA_RX_PARITY_ERR) {
			uport->icount.parity++;
			msm_geni_update_uart_error_code(msm_port,
							UART_ERROR_RX_PARITY_ERROR);
			UART_LOG_DBG(msm_port->ipc_log_misc, uport->dev,
				     "%s dma_rx_status:0x%x Rx Parity error:%d\n",
				     __func__, dma_rx_status,
				     uport->icount.parity);
			drop_rx = true;
		}

		if (dma_rx_status & UART_DMA_RX_FRAMING_ERR) {
			uport->icount.frame++;
			msm_geni_update_uart_error_code(msm_port,
							UART_ERROR_RX_FRAMING_ERR);
			UART_LOG_DBG(msm_port->ipc_log_misc, uport->dev,
				     "%s dma_rx_status:0x%x Rx Framing error:%d\n",
				     __func__, dma_rx_status,
				     uport->icount.frame);
			geni_se_dump_dbg_regs(&msm_port->serial_rsc, uport->membase,
					      msm_port->ipc_log_misc);
			drop_rx = true;
		}

		if (dma_rx_status & UART_DMA_RX_BREAK) {
			uport->icount.brk++;
			UART_LOG_DBG(msm_port->ipc_log_misc, uport->dev,
				     "%s dma_rx_status:0x%x Rx Break error:%d\n",
				     __func__, dma_rx_status, uport->icount.brk);
			msm_geni_update_uart_error_code(msm_port,
							UART_ERROR_RX_BREAK_ERROR);
			geni_se_dump_dbg_regs(&msm_port->serial_rsc, uport->membase,
					      msm_port->ipc_log_misc);
			drop_rx = true;
		}

		if (dma_rx_status & RX_EOT || dma_rx_status & RX_DMA_DONE) {
			msm_geni_serial_handle_dma_rx(uport, drop_rx);
			if (!(dma_rx_status & RX_GENI_CANCEL_IRQ)) {
				UART_LOG_DBG(msm_port->ipc_log_misc, uport->dev,
					     "%s mapping rx dma\n", __func__);
				geni_se_rx_dma_start(uport->membase,
						     DMA_RX_BUF_SIZE,
						     &msm_port->rx_dma);
			} else {
				UART_LOG_DBG(msm_port->ipc_log_misc, uport->dev,
					     "%s not mapping rx dma\n", __func__);
			}
		}

		if (dma_rx_status & RX_SBE) {
			UART_LOG_DBG(msm_port->ipc_log_misc, uport->dev,
				     "%s dma_rx_status:0x%x\n", __func__,
				     dma_rx_status);
			msm_geni_update_uart_error_code(msm_port,
							UART_ERROR_RX_SBE_ERROR);
			WARN_ON(1);
		}

		if (dma_rx_status & (RX_EOT | RX_GENI_CANCEL_IRQ | RX_DMA_DONE))
			ret = true;
	}

	if (s_irq_status & (S_CMD_CANCEL_EN | S_CMD_ABORT_EN))
		ret = true;

	spin_unlock_irqrestore(&msm_port->rx_lock, lock_flags);
	return ret;
}

/*
 * msm_geni_serial_clear_irqs() - clear the IRQs if they are coming after port close
 *
 * @uport: pointer to uart port
 *
 * Return: None
 */
static void msm_geni_serial_clear_irqs(struct uart_port *uport)
{
	u32 dma_tx_status, dma_rx_status;

	dma_tx_status = geni_read_reg_nolog(uport->membase, SE_DMA_TX_IRQ_STAT);
	if (dma_tx_status)
		geni_write_reg_nolog(dma_tx_status, uport->membase, SE_DMA_TX_IRQ_CLR);

	dma_rx_status = geni_read_reg_nolog(uport->membase, SE_DMA_RX_IRQ_STAT);
	if (dma_rx_status)
		geni_write_reg_nolog(dma_rx_status, uport->membase, SE_DMA_RX_IRQ_CLR);
}

static void msm_geni_serial_handle_isr(struct uart_port *uport,
				       unsigned long *flags,
				       bool is_irq_masked)
{
	unsigned int m_irq_status;
	unsigned int s_irq_status;
	unsigned int dma_tx_status;
	unsigned int dma_rx_status;
	unsigned int dma;
	struct msm_geni_serial_port *msm_port = GET_DEV_PORT(uport);
	struct tty_port *tport = &uport->state->port;
	struct tty_struct *tty = uport->state->port.tty;
	bool s_cmd_done = false;
	bool m_cmd_done = false;

	if (uart_console(uport) && atomic_read(&msm_port->is_clock_off)) {
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
	if ((m_irq_status & M_ILLEGAL_CMD_EN)) {
		if (uart_console(uport))
			IPC_LOG_MSG(msm_port->console_log,
				"%s.Illegal interrupt. sirq 0x%x mirq:0x%x\n",
				 __func__, s_irq_status, m_irq_status);
		else {
			msm_geni_update_uart_error_code(msm_port, UART_ERROR_ILLEGAL_INTERRUPT);
			WARN_ON(1);
		}
		goto exit_geni_serial_isr;
	}

	if (m_irq_status & (M_IO_DATA_ASSERT_EN | M_IO_DATA_DEASSERT_EN))
		uport->icount.cts++;

	if (s_irq_status & S_RX_FIFO_WR_ERR_EN) {
		uport->icount.overrun++;
		tty_insert_flip_char(tport, 0, TTY_OVERRUN);
		UART_LOG_DBG(msm_port->ipc_log_misc, uport->dev,
			"%s.sirq 0x%x buf_overrun:%d\n",
			__func__, s_irq_status, uport->icount.buf_overrun);
		msm_geni_update_uart_error_code(msm_port, UART_ERROR_BUFFER_OVERRUN);
	}

	dma = geni_read_reg_nolog(uport->membase, SE_GENI_DMA_MODE_EN);
	if (!dma) {
		m_cmd_done = handle_tx_fifo_xfer(m_irq_status, uport);
		s_cmd_done = handle_rx_fifo_xfer(s_irq_status, uport, flags,
							is_irq_masked);
	} else {
		dma_tx_status = geni_read_reg_nolog(uport->membase,
							SE_DMA_TX_IRQ_STAT);
		dma_rx_status = geni_read_reg_nolog(uport->membase,
							SE_DMA_RX_IRQ_STAT);

		if (m_irq_status || s_irq_status ||
			dma_tx_status || dma_rx_status) {
			UART_LOG_DBG(msm_port->ipc_log_irqstatus, uport->dev,
				"%s: sirq:0x%x mirq:0x%x dma_txirq:0x%x dma_rxirq:0x%x is_irq_masked:%d\n",
				__func__, s_irq_status, m_irq_status,
				dma_tx_status, dma_rx_status, is_irq_masked);
		}

		/* uport->state->port.tty pointer initialized as part of
		 * UART port_open. Adding check to ensure tty should have
		 * a valid value before using.
		 */
		if (tty) {
			m_cmd_done = handle_tx_dma_xfer(m_irq_status, uport);
			s_cmd_done = handle_rx_dma_xfer(s_irq_status, uport);
		} else {
			UART_LOG_DBG(msm_port->ipc_log_irqstatus, uport->dev,
				     "Port is closed!\n");
			msm_geni_serial_clear_irqs(uport);

		}
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

/*
 * msm_geni_wakeup_work() - Worker function invoked by wakeup isr,
 * powers on uart for data transfer and power off after
 * WAKEBYTE_TIMEOUT_MSEC(2secs)
 *
 * @work: pointer to work structure
 *
 * Return: None
 */
static void msm_geni_wakeup_work(struct work_struct *work)
{
	struct msm_geni_serial_port *port;
	struct uart_port *uport;

	port = container_of(work, struct msm_geni_serial_port,
			    wakeup_irq_dwork.work);
	if (!atomic_read(&port->check_wakeup_byte))
		return;
	uport = &port->uport;
	reinit_completion(&port->wakeup_comp);
	if (msm_geni_serial_power_on(uport)) {
		atomic_set(&port->check_wakeup_byte, 0);
		UART_LOG_DBG(port->ipc_log_rx, uport->dev,
			     "%s:Failed to power on\n", __func__);
		return;
	}
	/* wait to receive wakeup byte in rx path */
	if (!wait_for_completion_timeout(&port->wakeup_comp,
					 msecs_to_jiffies(WAKEBYTE_TIMEOUT_MSEC))) {
		UART_LOG_DBG(port->ipc_log_rx, uport->dev,
			     "%s completion of wakeup_comp task timedout %dmsec\n",
			     __func__, WAKEBYTE_TIMEOUT_MSEC);
		/* Check if port is closed during the task timeout time */
		if (!uport->state->port.tty)
			return;
	}
	msm_geni_serial_power_off(uport);
}

static irqreturn_t msm_geni_wakeup_isr(int isr, void *dev)
{
	struct uart_port *uport = dev;
	struct msm_geni_serial_port *port = GET_DEV_PORT(uport);
	struct tty_struct *tty;
	unsigned long flags;

	UART_LOG_DBG(port->ipc_log_rx, uport->dev, "%s\n", __func__);
	spin_lock_irqsave(&uport->lock, flags);
	if (atomic_read(&port->check_wakeup_byte)) {
		spin_unlock_irqrestore(&uport->lock, flags);
		return IRQ_HANDLED;
	}
	tty = uport->state->port.tty;
	/* uport->state->port.tty pointer initialized as part of
	 * UART port_open. Adding null check to ensure tty should
	 * have a valid value before dereference it in wakeup_isr.
	 */
	if (!tty) {
		UART_LOG_DBG(port->ipc_log_rx, uport->dev,
			     "%s: Unexpected wakeup ISR\n", __func__);
		WARN_ON_ONCE(1);
		spin_unlock_irqrestore(&uport->lock, flags);
		return IRQ_HANDLED;
	}

	atomic_set(&port->check_wakeup_byte, 1);
	queue_delayed_work(port->wakeup_irq_wq, &port->wakeup_irq_dwork, 0);
	spin_unlock_irqrestore(&uport->lock, flags);
	UART_LOG_DBG(port->ipc_log_rx, uport->dev, "%s: End\n", __func__);
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

/*
 * msm_geni_serial_flush() - Stops any pending tx transactions
 *
 * @uport: pointer to uart port
 *
 * Return: None
 */
static void msm_geni_serial_flush(struct uart_port *uport)
{
	struct msm_geni_serial_port *port = GET_DEV_PORT(uport);

	atomic_set(&port->flush_buffers, 1);
	msm_geni_serial_stop_tx(uport);
}

static void msm_geni_serial_shutdown(struct uart_port *uport)
{
	struct msm_geni_serial_port *msm_port = GET_DEV_PORT(uport);
	int ret;

	UART_LOG_DBG(msm_port->ipc_log_misc, uport->dev, "%s:\n", __func__);
	msm_port->shutdown_in_progress = true;
	/* Stop the console before stopping the current tx */
	if (uart_console(uport)) {
		console_stop(uport->cons);
		disable_irq(uport->irq);
	} else {
		if (!msm_port->ioctl_count)
			msm_geni_serial_power_on(uport);

		msm_geni_serial_stop_tx(uport);

		if (msm_port->ioctl_count) {
			UART_LOG_DBG(msm_port->ipc_log_pwr, uport->dev,
				     "%s: IOCTL vote present. Resetting ioctl count\n", __func__);
			msm_port->ioctl_count = 0;
		}

		ret = pm_runtime_put_sync_suspend(uport->dev);
		if (ret)
			UART_LOG_DBG(msm_port->ipc_log_pwr, uport->dev,
				     "%s: Failed to suspend:%d\n", __func__, ret);

		atomic_set(&msm_port->check_wakeup_byte, 0);

		if (!IS_ERR_OR_NULL(msm_port->serial_rsc.geni_gpio_shutdown)) {
			ret = pinctrl_select_state(
				msm_port->serial_rsc.geni_pinctrl,
				msm_port->serial_rsc.geni_gpio_shutdown);
			if (ret)
				IPC_LOG_MSG(msm_port->ipc_log_misc,
				      "%s: Error %d pinctrl_select_state\n",
					__func__, ret);
		}

		/* Reset UART error to default during port_close() */
		msm_port->uart_error = UART_ERROR_DEFAULT;
		atomic_set(&msm_port->flush_buffers, 0);
	}
	msm_port->shutdown_in_progress = false;
	UART_LOG_DBG(msm_port->ipc_log_misc, uport->dev, "%s: End\n", __func__);
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
		se_config_packing(uport->membase, 8, 4, false);
		geni_write_reg_nolog(0x431c, uport->membase, SE_GENI_CFG_REG80);
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

	UART_LOG_DBG(msm_port->ipc_log_misc, uport->dev, "%s: Start\n", __func__);

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
			UART_LOG_DBG(msm_port->ipc_log_misc, uport->dev,
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

exit_startup:
	if (likely(!uart_console(uport)))
		msm_geni_serial_power_off(&msm_port->uport);
	msm_port->startup_in_progress = false;
	UART_LOG_DBG(msm_port->ipc_log_misc, uport->dev, "%s: ret:%d\n", __func__, ret);

	return ret;
}

static void geni_serial_write_term_regs(struct uart_port *uport, u32 loopback,
		u32 tx_trans_cfg, u32 tx_parity_cfg, u32 rx_trans_cfg,
		u32 rx_parity_cfg, u32 bits_per_char, u32 stop_bit_len)
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
	if (!uart_console(uport))
		se_config_packing(uport->membase, bits_per_char, 4, false);
}

static void msm_geni_serial_termios_cfg(struct uart_port *uport,
					struct ktermios *termios)
{

	u32 bits_per_char = 0;
	u32 stop_bit_len;
	u32 tx_trans_cfg = geni_read_reg_nolog(uport->membase,
						SE_UART_TX_TRANS_CFG);
	u32 tx_parity_cfg = geni_read_reg_nolog(uport->membase,
						SE_UART_TX_PARITY_CFG);
	u32 rx_trans_cfg = geni_read_reg_nolog(uport->membase,
						SE_UART_RX_TRANS_CFG);
	u32 rx_parity_cfg = geni_read_reg_nolog(uport->membase,
						SE_UART_RX_PARITY_CFG);
	struct msm_geni_serial_port *port = GET_DEV_PORT(uport);

	if (termios->c_cflag & PARENB) {
		tx_trans_cfg |= UART_TX_PAR_EN;
		rx_trans_cfg |= UART_RX_PAR_EN;
		tx_parity_cfg |= PAR_CALC_EN;
		rx_parity_cfg |= PAR_CALC_EN;
		if (termios->c_cflag & PARODD) {
			tx_parity_cfg |= PAR_ODD << PAR_MODE_SHFT;
			rx_parity_cfg |= PAR_ODD << PAR_MODE_SHFT;
		} else if (termios->c_cflag & CMSPAR) {
			tx_parity_cfg |= PAR_SPACE << PAR_MODE_SHFT;
			rx_parity_cfg |= PAR_SPACE << PAR_MODE_SHFT;
		} else {
			tx_parity_cfg |= PAR_EVEN << PAR_MODE_SHFT;
			rx_parity_cfg |= PAR_EVEN << PAR_MODE_SHFT;
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
		msm_geni_serial_set_manual_flow(true, port);
	} else {
		tx_trans_cfg |= UART_CTS_MASK;
		msm_geni_serial_set_manual_flow(false, port);
		/* status bits to ignore */
	}

	geni_serial_write_term_regs(uport, port->loopback, tx_trans_cfg,
		tx_parity_cfg, rx_trans_cfg, rx_parity_cfg, bits_per_char,
		stop_bit_len);

	UART_LOG_DBG(port->ipc_log_misc, uport->dev, "Tx: trans_cfg%d parity %d\n",
						tx_trans_cfg, tx_parity_cfg);
	UART_LOG_DBG(port->ipc_log_misc, uport->dev, "Rx: trans_cfg%d parity %d\n",
						rx_trans_cfg, rx_parity_cfg);
	UART_LOG_DBG(port->ipc_log_misc, uport->dev, "BitsChar%d stop bit%d\n",
				bits_per_char, stop_bit_len);
}

static void msm_geni_serial_set_termios(struct uart_port *uport,
				struct ktermios *termios, struct ktermios *old)
{
	int ret;
	unsigned int baud;
	unsigned int clk_div;
	unsigned long ser_clk_cfg = 0;
	struct msm_geni_serial_port *port = GET_DEV_PORT(uport);
	unsigned long clk_rate;
	unsigned long desired_rate;
	unsigned int clk_idx;
	int uart_sampling;
	int clk_freq_diff;

	/* QUP_2.5.0 and older RUMI has sampling rate as 32 */
	if (IS_ENABLED(CONFIG_SERIAL_MSM_GENI_HALF_SAMPLING) &&
		port->rumi_platform && port->is_console) {
		geni_write_reg_nolog(0x21, uport->membase, GENI_SER_M_CLK_CFG);
		geni_write_reg_nolog(0x21, uport->membase, GENI_SER_S_CLK_CFG);
		geni_read_reg_nolog(uport->membase, GENI_SER_M_CLK_CFG);
	}

	if (!uart_console(uport)) {
		int ret;

		UART_LOG_DBG(port->ipc_log_misc, uport->dev, "%s: start\n", __func__);
		ret = msm_geni_serial_power_on(uport);

		if (ret) {
			UART_LOG_DBG(port->ipc_log_misc, uport->dev,
				"%s: Failed to vote clock on:%d\n",
				__func__, ret);
			return;
		}
	}
	msm_geni_serial_stop_rx(uport);
	/* baud rate */
	baud = uart_get_baud_rate(uport, termios, old, 300, 4000000);
	port->cur_baud = baud;

	/* sampling is halved for QUP versions >= 2.5 */
	uart_sampling = UART_OVERSAMPLING;
	if ((port->ver_info.hw_major_ver >= 3) || ((port->ver_info.hw_major_ver >= 2) &&
		(port->ver_info.hw_minor_ver >= 5)))
		uart_sampling /= 2;

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
		msm_geni_update_uart_error_code(port, UART_ERROR_SE_CLK_RATE_FIND_FAIL);
		goto exit_set_termios;
	}

	clk_div = DIV_ROUND_UP(clk_rate, desired_rate);
	if (!clk_div)
		goto exit_set_termios;

	clk_freq_diff =  (desired_rate - (clk_rate / clk_div));
	if (clk_freq_diff)
		UART_LOG_DBG(port->ipc_log_misc, uport->dev,
			"%s src_clk freq_diff:%d baud:%d clk_rate:%d clk_div:%d\n",
			__func__, clk_freq_diff, baud, clk_rate, clk_div);

	uport->uartclk = clk_rate;
	ret = clk_set_rate(port->serial_rsc.se_clk, clk_rate);
	if (ret) {
		dev_err(uport->dev, "Error setting clock rate\n");
		IPC_LOG_MSG(port->ipc_log_misc, "%s: SE clock set_rate error:%d\n", __func__, ret);
		WARN_ON(1);
	}
	ser_clk_cfg |= SER_CLK_EN;
	ser_clk_cfg |= (clk_div << CLK_DIV_SHFT);

	if (likely(baud))
		uart_update_timeout(uport, termios->c_cflag, baud);

	port->ser_clk_cfg = ser_clk_cfg;
	msm_geni_enable_disable_se_clk(uport, true);

	msm_geni_serial_termios_cfg(uport, termios);
	UART_LOG_DBG(port->ipc_log_misc, uport->dev, "%s: baud %d ser_clk_cfg:0x%x\n",
			__func__, baud, port->ser_clk_cfg);
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

#if IS_ENABLED(CONFIG_SERIAL_MSM_GENI_CONSOLE) || \
						IS_ENABLED(CONFIG_CONSOLE_POLL)
static int msm_geni_console_setup(struct console *co, char *options)
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
#endif /* (CONFIG_SERIAL_MSM_GENI_CONSOLE) || defined(CONFIG_CONSOLE_POLL) */

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
		memset(name, 0, sizeof(name));
		if (!msm_port->ipc_log_irqstatus) {
			scnprintf(name, sizeof(name), "%s%s",
					dev_name(uport->dev), "_irqstatus");
			msm_port->ipc_log_irqstatus = ipc_log_context_create(
					IPC_LOG_MISC_PAGES, name, 0);
			if (!msm_port->ipc_log_irqstatus)
				dev_info(uport->dev, "Err in irqstatus IPC Log\n");
		}

		/* New set of UART IPC log to avoid overwrite of logging */
		memset(name, 0, sizeof(name));
		if (!msm_port->ipc_log_new) {
			scnprintf(name, sizeof(name), "%s%s",
				  dev_name(uport->dev), "_new");
			msm_port->ipc_log_new = ipc_log_context_create(
						IPC_LOG_MISC_PAGES, name, 0);
			if (!msm_port->ipc_log_new)
				dev_info(uport->dev, "Err with New IPC Log\n");
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

	if (new_state == UART_PM_STATE_ON && old_state == UART_PM_STATE_OFF) {
		se_geni_resources_on(&msm_port->serial_rsc);
		msm_geni_enable_disable_se_clk(uport, true);
		atomic_set(&msm_port->is_clock_off, 0);
		/* Enable Interrupt */
		IPC_LOG_MSG(msm_port->console_log, "%s Enable IRQ\n", __func__);
		enable_irq(uport->irq);
	} else if (new_state == UART_PM_STATE_OFF &&
			old_state == UART_PM_STATE_ON) {
		atomic_set(&msm_port->is_clock_off, 1);
		/* Disable Interrupt */
		IPC_LOG_MSG(msm_port->console_log, "%s Disable IRQ\n", __func__);
		disable_irq(uport->irq);
		msm_geni_enable_disable_se_clk(uport, false);
		se_geni_resources_off(&msm_port->serial_rsc);
	}
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
	.flush_buffer = msm_geni_serial_flush,
	.ioctl = msm_geni_serial_ioctl,
};

static const struct of_device_id msm_geni_device_tbl[] = {
#if IS_ENABLED(CONFIG_SERIAL_MSM_GENI_CONSOLE) || \
						IS_ENABLED(CONFIG_CONSOLE_POLL)
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
	int len = (sizeof(struct msm_geni_serial_ver_info) * 2);
	char fwver[20], hwver[20];

	/* clks_on/off only for HSUART, as console remains actve */
	if (!msm_port->is_console) {
		/* Enable clock & set clk divider value */
		msm_port->ser_clk_cfg = 0x21;

		se_geni_clks_on(&msm_port->serial_rsc);
		msm_geni_enable_disable_se_clk(uport, true);
	}
	/* Basic HW and FW info */
	if (unlikely(get_se_proto(uport->membase) != UART)) {
		dev_err(uport->dev, "%s: Invalid FW %d loaded.\n",
			 __func__, get_se_proto(uport->membase));
		ret = -ENXIO;
		goto exit_ver_info;
	}
	msm_port->serial_rsc.proto = UART;
	msm_port->ver_info.m_fw_ver = get_se_m_fw(uport->membase);
	msm_port->ver_info.s_fw_ver = get_se_s_fw(uport->membase);
	scnprintf(fwver, len, "FW Ver:0x%x%x", msm_port->ver_info.m_fw_ver,
			msm_port->ver_info.s_fw_ver);
	UART_LOG_DBG(msm_port->ipc_log_misc, uport->dev,
		"%s: FW Ver: %s\n", __func__, fwver);

	hw_ver = geni_se_qupv3_hw_version(msm_port->wrapper_dev,
		&msm_port->ver_info.hw_major_ver,
		&msm_port->ver_info.hw_minor_ver,
		&msm_port->ver_info.hw_step_ver);
	if (hw_ver)
		dev_err(uport->dev, "%s:Err getting HW version %d\n",
						__func__, hw_ver);
	else {
		scnprintf(hwver, len, "HW Ver:%x.%x.%x",
			msm_port->ver_info.hw_major_ver,
			msm_port->ver_info.hw_minor_ver,
			msm_port->ver_info.hw_step_ver);
		UART_LOG_DBG(msm_port->ipc_log_misc,
			uport->dev, "%s: HW Ver: %s\n", __func__, hwver);
	}

	msm_geni_serial_enable_interrupts(uport);
exit_ver_info:
	if (!msm_port->is_console) {
		msm_geni_enable_disable_se_clk(uport, false);
		se_geni_clks_off(&msm_port->serial_rsc);
	}
	return ret;
}

static int msm_geni_serial_get_irq_pinctrl(struct platform_device *pdev,
					struct msm_geni_serial_port *dev_port)
{
	int ret  = 0;
	struct uart_port *uport = &dev_port->uport;

	/* Optional to use the Rx pin as wakeup irq */
	dev_port->wakeup_irq = platform_get_irq(pdev, 1);
	if ((dev_port->wakeup_irq < 0 && !dev_port->is_console))
		dev_info(&pdev->dev, "No wakeup IRQ configured\n");

	dev_port->serial_rsc.geni_pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR_OR_NULL(dev_port->serial_rsc.geni_pinctrl)) {
		dev_err(&pdev->dev, "No pinctrl config specified!\n");
		return PTR_ERR(dev_port->serial_rsc.geni_pinctrl);
	}

	if (!dev_port->is_console) {
		if (IS_ERR_OR_NULL(pinctrl_lookup_state(dev_port->serial_rsc.geni_pinctrl,
				PINCTRL_SHUTDOWN))) {
			dev_info(&pdev->dev, "No Shutdown config specified\n");
		} else {
			dev_port->serial_rsc.geni_gpio_shutdown =
			pinctrl_lookup_state(dev_port->serial_rsc.geni_pinctrl,
							PINCTRL_SHUTDOWN);
		}
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
			return PTR_ERR(dev_port->serial_rsc.geni_gpio_active);
		}
	}

	dev_port->serial_rsc.geni_gpio_sleep =
		pinctrl_lookup_state(dev_port->serial_rsc.geni_pinctrl,
						PINCTRL_SLEEP);
	if (IS_ERR_OR_NULL(dev_port->serial_rsc.geni_gpio_sleep)) {
		dev_err(&pdev->dev, "No sleep config specified!\n");
		return PTR_ERR(dev_port->serial_rsc.geni_gpio_sleep);
	}

	uport->irq = platform_get_irq(pdev, 0);
	if (uport->irq < 0) {
		ret = uport->irq;
		dev_err(&pdev->dev, "Failed to get IRQ %d\n", ret);
		return ret;
	}

	dev_port->name = devm_kasprintf(uport->dev, GFP_KERNEL,
					"msm_serial_geni%d", uport->line);
	irq_set_status_flags(uport->irq, IRQ_NOAUTOEN);
	ret = devm_request_irq(uport->dev, uport->irq, msm_geni_serial_isr,
				IRQF_TRIGGER_HIGH, dev_port->name, uport);
	if (ret) {
		dev_err(uport->dev, "%s: Failed to get IRQ ret %d\n",
							__func__, ret);
		return ret;
	}

	if (dev_port->wakeup_irq > 0) {
		dev_port->wakeup_irq_wq = alloc_workqueue("%s", WQ_HIGHPRI, 1,
							  dev_name(uport->dev));
		if (!dev_port->wakeup_irq_wq)
			return -ENOMEM;
		INIT_DELAYED_WORK(&dev_port->wakeup_irq_dwork, msm_geni_wakeup_work);
		irq_set_status_flags(dev_port->wakeup_irq, IRQ_NOAUTOEN);
		ret = devm_request_irq(uport->dev, dev_port->wakeup_irq,
					msm_geni_wakeup_isr,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					"hs_uart_wakeup", uport);
		if (unlikely(ret)) {
			dev_err(uport->dev, "%s:Failed to get WakeIRQ ret%d\n",
				__func__, ret);
			destroy_workqueue(dev_port->wakeup_irq_wq);
			return ret;
		}
		dev_port->wakeup_enabled = false;
	}

	return ret;
}
static int msm_geni_serial_get_clk(struct platform_device *pdev,
					struct msm_geni_serial_port *dev_port)
{
	int ret = 0;

	dev_port->serial_rsc.se_clk = devm_clk_get(&pdev->dev, "se-clk");
	if (IS_ERR(dev_port->serial_rsc.se_clk)) {
		ret = PTR_ERR(dev_port->serial_rsc.se_clk);
		dev_err(&pdev->dev, "Err getting SE Core clk %d\n", ret);
		msm_geni_update_uart_error_code(dev_port, UART_ERROR_CLK_GET_FAIL);
		return ret;
	}

	dev_port->serial_rsc.m_ahb_clk = devm_clk_get(&pdev->dev, "m-ahb");
	if (IS_ERR(dev_port->serial_rsc.m_ahb_clk)) {
		ret = PTR_ERR(dev_port->serial_rsc.m_ahb_clk);
		dev_err(&pdev->dev, "Err getting M AHB clk %d\n", ret);
		msm_geni_update_uart_error_code(dev_port, UART_ERROR_CLK_GET_FAIL);
		return ret;
	}

	dev_port->serial_rsc.s_ahb_clk = devm_clk_get(&pdev->dev, "s-ahb");
	if (IS_ERR(dev_port->serial_rsc.s_ahb_clk)) {
		ret = PTR_ERR(dev_port->serial_rsc.s_ahb_clk);
		dev_err(&pdev->dev, "Err getting S AHB clk %d\n", ret);
		msm_geni_update_uart_error_code(dev_port, UART_ERROR_CLK_GET_FAIL);
		return ret;
	}

	return ret;
}
static int msm_geni_serial_read_dtsi(struct platform_device *pdev,
					struct msm_geni_serial_port *dev_port)
{
	int ret = 0;
	struct uart_port *uport = &dev_port->uport;
	struct resource *res;
	bool is_console = dev_port->is_console;
	struct platform_device *wrapper_pdev;
	struct device_node *wrapper_ph_node;
	u32 wake_char = 0;

	wrapper_ph_node = of_parse_phandle(pdev->dev.of_node,
					"qcom,wrapper-core", 0);
	if (IS_ERR_OR_NULL(wrapper_ph_node))
		return PTR_ERR(wrapper_ph_node);

	wrapper_pdev = of_find_device_by_node(wrapper_ph_node);
	of_node_put(wrapper_ph_node);
	if (IS_ERR_OR_NULL(wrapper_pdev))
		return PTR_ERR(wrapper_pdev);

	dev_port->wrapper_dev = &wrapper_pdev->dev;
	dev_port->serial_rsc.wrapper_dev = &wrapper_pdev->dev;
	dev_port->serial_rsc.ctrl_dev = &pdev->dev;

	if (is_console)
		ret = geni_se_resources_init(&dev_port->serial_rsc,
			UART_CONSOLE_CORE2X_VOTE,
			(DEFAULT_SE_CLK * DEFAULT_BUS_WIDTH));
	else
		ret = geni_se_resources_init(&dev_port->serial_rsc,
			UART_CORE2X_VOTE,
			(DEFAULT_SE_CLK * DEFAULT_BUS_WIDTH));

	if (ret) {
		msm_geni_update_uart_error_code(dev_port, UART_ERROR_SE_RESOURCES_INIT_FAIL);
		return ret;
	}

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

	ret = msm_geni_serial_get_clk(pdev, dev_port);
	if (ret)
		return ret;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "se_phys");
	if (!res) {
		dev_err(&pdev->dev, "Err getting IO region\n");
		return -ENXIO;
	}

	uport->mapbase = res->start;
	uport->membase = devm_ioremap(&pdev->dev, res->start,
						resource_size(res));
	if (!uport->membase) {
		dev_err(&pdev->dev, "Err IO mapping serial iomem\n");
		return -ENOMEM;
	}

	ret = msm_geni_serial_get_irq_pinctrl(pdev, dev_port);
	if (ret)
		return ret;

	if (!is_console) {
		dev_port->geni_wake = wakeup_source_register(uport->dev,
						dev_name(&pdev->dev));
		if (!dev_port->geni_wake) {
			dev_err(&pdev->dev,
				"Failed to register wakeup_source\n");
			return -ENOMEM;
		}
	}

	return ret;
}

static int msm_geni_serial_probe(struct platform_device *pdev)
{
	int ret = 0;
	int line;
	struct msm_geni_serial_port *dev_port;
	struct uart_port *uport;
	struct uart_driver *drv;
	const struct of_device_id *id;
	bool is_console = false;

	id = of_match_device(msm_geni_device_tbl, &pdev->dev);
	if (!id) {
		dev_err(&pdev->dev, "%s: No matching device found\n",
				__func__);
		return -ENODEV;
	}
	dev_dbg(&pdev->dev, "%s: %s\n", __func__, id->compatible);
	drv = (struct uart_driver *)id->data;

	if (pdev->dev.of_node) {
		if (drv->cons) {
			line = of_alias_get_id(pdev->dev.of_node, "serial");
			if (line < 0)
				line = 0;
		} else {
			line = of_alias_get_id(pdev->dev.of_node, "hsuart");
			if (line < 0)
				line = uart_line_id;
			else /* for non alias hsuart use uart_line_id in get_port_from_line */
				line = uart_line_id;
		}
	} else {
		line = pdev->id;
	}

	is_console = (drv->cons ? true : false);
	dev_port = get_port_from_line(line, is_console);
	if (IS_ERR_OR_NULL(dev_port)) {
		ret = PTR_ERR(dev_port);
		dev_err(&pdev->dev, "Invalid line ret:%d\n", ret);
		goto exit_geni_serial_probe;
	}

	dev_port->is_console = is_console;
	if (drv->cons && !con_enabled) {
		dev_err(&pdev->dev, "%s, Console Disabled\n", __func__);
		platform_set_drvdata(pdev, dev_port);
		return 0;
	}

	uport = &dev_port->uport;

	/* Don't allow 2 drivers to access the same port */
	if (uport->private_data) {
		ret = -ENODEV;
		goto exit_geni_serial_probe;
	}

	uport->dev = &pdev->dev;

	ret = msm_geni_serial_read_dtsi(pdev, dev_port);
	if (ret)
		goto exit_geni_serial_probe;

	if (!drv->cons)
		uart_line_id++;

	dev_port->tx_fifo_depth = DEF_FIFO_DEPTH_WORDS;
	dev_port->rx_fifo_depth = DEF_FIFO_DEPTH_WORDS;
	dev_port->tx_fifo_width = DEF_FIFO_WIDTH_BITS;
	uport->fifosize =
		((dev_port->tx_fifo_depth * dev_port->tx_fifo_width) >> 3);
	/* Complete signals to handle cancel cmd completion */
	init_completion(&dev_port->m_cmd_timeout);
	init_completion(&dev_port->s_cmd_timeout);

	init_completion(&dev_port->wakeup_comp);
	uport->private_data = (void *)drv;
	platform_set_drvdata(pdev, dev_port);
	ret = msm_geni_serial_get_ver_info(uport);
	if (ret) {
		dev_err(&pdev->dev, "Failed to Read FW ver: %d\n", ret);
		goto exit_geni_serial_probe;
	}

	if (is_console) {
		dev_port->handle_rx = handle_rx_console;
		dev_port->rx_fifo = devm_kzalloc(uport->dev, sizeof(u32),
						 GFP_KERNEL);
		if (!dev_port->rx_fifo) {
			ret = -ENOMEM;
			goto exit_geni_serial_probe;
		}
	} else {
		dev_port->handle_rx = handle_rx_hs;
		dev_port->rx_fifo =
			devm_kzalloc(uport->dev, (dev_port->rx_fifo_depth *
				     sizeof(u32)), GFP_KERNEL);
		if (!dev_port->rx_fifo) {
			ret = -ENOMEM;
			goto exit_geni_serial_probe;
		}
		pm_runtime_set_suspended(&pdev->dev);
		pm_runtime_set_autosuspend_delay(&pdev->dev, 150);
		pm_runtime_use_autosuspend(&pdev->dev);
		pm_runtime_enable(&pdev->dev);
	}

	if (IS_ENABLED(CONFIG_SERIAL_MSM_GENI_HALF_SAMPLING) &&
			dev_port->rumi_platform && dev_port->is_console) {
		/* No ver info available, if do later then RUMI console fails */
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

	dev_port->uart_error = UART_ERROR_DEFAULT;

	/*
	 * In abrupt kill scenarios, previous state of the uart causing runtime
	 * resume, lead to spinlock bug in stop_rx_sequencer, so initializing it
	 * before
	 */
	if (!dev_port->is_console)
		spin_lock_init(&dev_port->rx_lock);

	ret = uart_add_one_port(drv, uport);
	if (ret)
		dev_err(&pdev->dev, "Failed to register uart_port: %d\n", ret);

exit_geni_serial_probe:
	UART_LOG_DBG(dev_port->ipc_log_misc, &pdev->dev, "%s: ret:%d\n",
		__func__, ret);
	return ret;
}

static int msm_geni_serial_remove(struct platform_device *pdev)
{
	struct msm_geni_serial_port *port = platform_get_drvdata(pdev);
	struct uart_driver *drv =
			(struct uart_driver *)port->uport.private_data;

	/* Platform driver is registered for console and when console
	 * is disabled from cmdline simply return success.
	 */
	if (port->is_console && !con_enabled)
		return 0;
	if (port->wakeup_irq > 0)
		destroy_workqueue(port->wakeup_irq_wq);
	if (!uart_console(&port->uport)) {
		wakeup_source_unregister(port->geni_wake);
		port->geni_wake = NULL;
	}
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
	UART_LOG_DBG(port->ipc_log_misc, port->uport.dev, "%s(): rfr = 0x%x\n",
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

	UART_LOG_DBG(port->ipc_log_pwr, dev, "%s: Start\n", __func__);
	/* Flow off from UART only for In band sleep(IBS)
	 * Avoid manual RFR FLOW ON for Out of band sleep(OBS).
	 */
	if (port->wakeup_byte && port->wakeup_irq)
		msm_geni_serial_set_manual_flow(false, port);
	/* If shutdown is in progress stop rx sequencer and
	 * disable the clocks, don't don't check for wakeup byte.
	 */
	if (!port->shutdown_in_progress) {
		ret = wait_for_transfers_inflight(&port->uport);
		if (ret) {
			UART_LOG_DBG(port->ipc_log_misc, dev,
				     "%s: wait_for_transfer_inflight return ret:%d\n",
				     __func__, ret);
			/* Flow on from UART only for In band sleep(IBS)
			 * Avoid manual RFR FLOW ON for Out of band sleep(OBS)
			 */
			if (port->wakeup_byte && port->wakeup_irq)
				msm_geni_serial_allow_rx(port);
			return -EBUSY;
		}
	}
	/*
	 * Stop Rx.
	 * Disable Interrupt
	 * Resources off
	 */
	ret = stop_rx_sequencer(&port->uport);
	if (ret) {
		UART_LOG_DBG(port->ipc_log_pwr, dev, "%s: stop rx failed %d\n",
							__func__, ret);
		/* Flow on from UART only for In band sleep(IBS)
		 * Avoid manual RFR FLOW ON for Out of band sleep(OBS)
		 */
		if (port->wakeup_byte && port->wakeup_irq)
			msm_geni_serial_allow_rx(port);
		return -EBUSY;
	}

	geni_status = geni_read_reg_nolog(port->uport.membase, SE_GENI_STATUS);
	if ((geni_status & M_GENI_CMD_ACTIVE))
		stop_tx_sequencer(&port->uport);

	disable_irq(port->uport.irq);

	/*
	 * Flow on from UART only for In band sleep(IBS)
	 * Avoid manual RFR FLOW ON for Out of band sleep(OBS).
	 * Above before stop_rx disabled the flow so we need to enable it here
	 * Make sure wake up interrupt is enabled before RFR is made low
	 */
	if (port->wakeup_byte && port->wakeup_irq && !port->shutdown_in_progress)
		msm_geni_serial_allow_rx(port);

	msm_geni_enable_disable_se_clk(&port->uport, false);
	ret = se_geni_resources_off(&port->serial_rsc);
	if (ret) {
		dev_err(dev, "%s: Error ret %d\n", __func__, ret);
		msm_geni_update_uart_error_code(port, UART_ERROR_SE_RESOURCES_OFF_FAIL);
		goto exit_runtime_suspend;
	}

	/*
	 * If shutdown is not in progress, check if port
	 * is in open state before enabling wakeup_irq
	 */
	if (!port->shutdown_in_progress &&
	    port->wakeup_irq > 0 && port->uport.state->port.tty) {
		atomic_set(&port->check_wakeup_byte, 0);
		enable_irq(port->wakeup_irq);
		port->wakeup_enabled = true;
		ret = irq_set_irq_wake(port->wakeup_irq, 1);
		if (unlikely(ret))
			dev_err(dev, "%s:Failed to set IRQ wake:%d\n",
				__func__, ret);
	}
	UART_LOG_DBG(port->ipc_log_pwr, dev, "%s: End\n", __func__);
	__pm_relax(port->geni_wake);
exit_runtime_suspend:
	return ret;
}

static int msm_geni_serial_runtime_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct msm_geni_serial_port *port = platform_get_drvdata(pdev);
	int ret = 0;

	UART_LOG_DBG(port->ipc_log_pwr, dev, "%s: Start\n", __func__);

	__pm_stay_awake(port->geni_wake);
	/*
	 * check for wakeup_enabled before disabling the wakeup_irq as
	 * this might be disabled from shutdown as well.
	 */
	if (port->wakeup_irq > 0 && port->wakeup_enabled &&
	    port->uport.state->port.tty) {
		ret = irq_set_irq_wake(port->wakeup_irq, 0);
		if (unlikely(ret))
			dev_err(dev, "%s:Failed to unset IRQ wake:%d\n",
				__func__, ret);
		disable_irq(port->wakeup_irq);
		port->wakeup_enabled = false;
	}

	UART_LOG_DBG(port->ipc_log_pwr, dev, "%s: Enabling Resources\n", __func__);
	/*
	 * Resources On.
	 * Start Rx.
	 * Auto RFR.
	 * Enable IRQ.
	 */
	ret = se_geni_resources_on(&port->serial_rsc);
	if (ret) {
		dev_err(dev, "%s: Error ret %d\n", __func__, ret);
		msm_geni_update_uart_error_code(port, UART_ERROR_SE_RESOURCES_ON_FAIL);
		__pm_relax(port->geni_wake);
		goto exit_runtime_resume;
	}
	msm_geni_enable_disable_se_clk(&port->uport, true);

	/* Don't start the RX sequencer during shutdown */
	if (!port->shutdown_in_progress)
		start_rx_sequencer(&port->uport);
	/* Ensure that the Rx is running before enabling interrupts */
	mb();
	/* Enable interrupt */
	enable_irq(port->uport.irq);

	UART_LOG_DBG(port->ipc_log_pwr, dev, "%s: End\n", __func__);
exit_runtime_resume:
	return ret;
}

static int msm_geni_serial_sys_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct msm_geni_serial_port *port = platform_get_drvdata(pdev);
	struct uart_port *uport = &port->uport;

	/* Platform driver is registered for console and when console
	 * is disabled from cmdline simply return success.
	 */
	if (port->is_console && !con_enabled) {
		return 0;
	} else if (uart_console(uport)) {
		IPC_LOG_MSG(port->console_log, "%s start\n", __func__);
		uart_suspend_port((struct uart_driver *)uport->private_data,
					uport);
		IPC_LOG_MSG(port->console_log, "%s\n", __func__);
	} else {
		struct uart_state *state = uport->state;
		struct tty_port *tty_port = &state->port;

		mutex_lock(&tty_port->mutex);
		if (!pm_runtime_status_suspended(dev)) {
			dev_err(dev, "%s:Active userspace vote; ioctl_cnt %d\n",
					__func__, port->ioctl_count);
			UART_LOG_DBG(port->ipc_log_pwr, dev,
				"%s:Active userspace vote; ioctl_cnt %d\n",
					__func__, port->ioctl_count);
			mutex_unlock(&tty_port->mutex);
			return -EBUSY;
		}
		UART_LOG_DBG(port->ipc_log_pwr, dev, "%s\n", __func__);
		mutex_unlock(&tty_port->mutex);
	}
	return 0;
}

static int msm_geni_serial_sys_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct msm_geni_serial_port *port = platform_get_drvdata(pdev);
	struct uart_port *uport = &port->uport;

	if (uart_console(uport) &&
	    console_suspend_enabled && uport->suspended) {
		IPC_LOG_MSG(port->console_log, "%s start\n", __func__);
		uart_resume_port((struct uart_driver *)uport->private_data,
									uport);
		IPC_LOG_MSG(port->console_log, "%s\n", __func__);
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

static int msm_geni_serial_sys_suspend(struct device *dev)
{
	return 0;
}

static int msm_geni_serial_sys_resume(struct device *dev)
{
	return 0;
}
#endif

static const struct dev_pm_ops msm_geni_serial_pm_ops = {
	.runtime_suspend = msm_geni_serial_runtime_suspend,
	.runtime_resume = msm_geni_serial_runtime_resume,
	.suspend = msm_geni_serial_sys_suspend,
	.resume = msm_geni_serial_sys_resume,
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

	ret = uart_register_driver(&msm_geni_serial_hs_driver);
	if (ret)
		return ret;

	if (con_enabled) {
		ret = console_register(&msm_geni_console_driver);
		if (ret) {
			uart_unregister_driver(&msm_geni_serial_hs_driver);
			return ret;
		}
	}

	ret = platform_driver_register(&msm_geni_serial_platform_driver);
	if (ret) {
		if (con_enabled)
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
