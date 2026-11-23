// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/bitmap.h>
#include <linux/bitops.h>
#include <linux/console.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/ioctl.h>
#include <linux/ipc_logging.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/msm_gpi.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/soc/qcom/geni-se.h>
#include <linux/qcom-geni-se-common.h>
#include <linux/serial.h>
#include <linux/serial_core.h>
#include <linux/slab.h>
#include <linux/suspend.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <uapi/linux/msm_geni_serial.h>

static bool con_enabled = IS_ENABLED(CONFIG_SERIAL_MSM_GENI_CONSOLE_DEFAULT_ENABLED);

/* UART specific GENI registers */
#define GENI_CFG_STATUS			(0x88)
#define SE_GENI_CFG_REG64		(0x200)
#define SE_GENI_CFG_REG67		(0x20C)
#define SE_UART_LOOPBACK_CFG		(0x22C)
#define SE_GENI_CFG_REG76		(0x230)
#define SE_GENI_CFG_REG80		(0x240)
#define SE_GENI_CFG_REG81		(0x244)
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
#define SE_UART_IO_MACRO_CTRL		(0x240)
#define SE_UART_IO3_VAL			(0x248)
#define SE_UART_TX_TRANS_CFG		(0x25C)
#define M_IRQ_ENABLE			(0x614)
#define M_CMD_ERR_STATUS		(0x624)
#define M_FW_ERR_STATUS			(0x628)
#define M_GP_LENGTH			(0x910)
#define S_GP_LENGTH			(0x914)
#define SE_HW_PARAM_2			(0xE2C)
#define SE_DMA_DEBUG_REG0		(0xE40)
#define SE_DMA_IF_EN			(0x004)
#define SE_GENI_GENERAL_CFG		(0x10)
#define SE_DMA_TX_MAX_BURST		(0xC5C)
#define SE_DMA_RX_MAX_BURST		(0xD5C)

/* SE_GENI_CFG_REG64 */
#define UART_SINGLE_WIRE_EN			BIT(18)

/* SE_HW_PARAM_2 */
#define UART_SINGLE_WIRE_SUPPORTED		BIT(19)

/* SE_UART_IO_MACRO_CTRL */
#define UART_IO_MACRO_IO2_SEL_MASK		GENMASK(5, 4)
#define UART_IO_MACRO_IO2_SEL_SHIFT		(0x4)
#define UART_IO_MACRO_IO3_SEL_MASK		GENMASK(7, 6)
#define UART_IO_MACRO_IO3_SEL_SHIFT		(0x6)
#define UART_IO_MACRO_RX_DATA_IN_SEL_MASK	GENMASK(9, 8)
#define UART_IO_MACRO_RX_DATA_IN_SEL_SHIFT	(0x8)

/* SE_GENI_CFG_SEQ_START */
#define UART_START_TRIGGER			BIT(0)

/* SE_GENI_CFG_STATUS */
#define UART_CFG_SEQ_DONE			BIT(1)

/* SE_GENI_CFG_REG87 */
#define M_SW_COMP3				BIT(3)

/* SE_GENI_CFG_REG81 */
#define GENI_SOUT0_PULL_UP_EN			BIT(0)

/* SE_GENI_CFG_REG76 */
#define TX_ENGINE_INV_EN			BIT(0)

/* SE_GENI_CFG_REG67 */
#define TX_DEFAULT_SOUT_VALUE			BIT(13)
#define TX_DEFAULT_SOE_VALUE			BIT(14)

/* UART mode */
#define OPEN_DRAIN_MODE		(0)
#define PUSH_PULL_MODE		(0x1)

/* UART QUP Lane type */
#define QUP_L2_LANE	(0x2)
#define QUP_L3_LANE	(0x3)

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
#define UART_RX_PAR_EN		(BIT(4))

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

/* UART KPI */
#define UART_KPI_TX_RX_INSTANCES 5

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

#define WAKEBYTE_TIMEOUT_MSEC	(2000)
#define WAIT_XFER_MAX_ITER	(2)
#define WAIT_XFER_MAX_TIMEOUT_US	(150)
#define WAIT_XFER_MIN_TIMEOUT_US	(100)
#define IPC_LOG_PWR_PAGES	(10)
#define IPC_LOG_MISC_PAGES	(30)
#define IPC_LOG_TX_RX_PAGES	(30)
#define DATA_BYTES_PER_LINE	(32)
#define MAX_LEN			(20)

/* TX Instances of DMA transmit for timestamp register */
#define TX_TRE_INSTANCES_1	1
#define TX_TRE_INSTANCES_2	2
#define TX_TRE_MAX_INSTANCES	3

/* RX Instance of DMA transmit for timestamp register */
#define RX_TRE_INSTANCES	5

/* Tx-Rx scatterlist instances */
#define TX_SG_INSTANCES		5
#define RX_SG_INSTANCES		6

/*Instances to store the DMA address for timestamp register*/
#define TS_DMA_INSTANCES_0	0
#define TS_DMA_INSTANCES_1	1
#define TS_DMA_MAX_INSTANCES	2


#define GENI_SE_DMA_DONE_EN		BIT(0)
#define GENI_SE_DMA_EOT_EN		BIT(1)
#define GENI_SE_DMA_IMMEDIATE_MODE	BIT(1)
#define SERIAL_DMA_ADDR_ALIGN_BYTE	(QUP_DMA_ADDR_ALIGN_BYTE)
#define SPLIT_DMA_TX_SIZE		(QUP_SPLIT_DMA_TRE_SIZE)
#define SERIAL_TX_DMA_BUF_SIZE_MAX	PAGE_SIZE
#define SERIAL_PREALLOC_BUF_SIZE	(SERIAL_TX_DMA_BUF_SIZE_MAX + SERIAL_DMA_ADDR_ALIGN_BYTE)


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

#define PINCTRL_DEFAULT "default"
#define PINCTRL_ACTIVE  "active"
#define PINCTRL_SLEEP   "sleep"
#define PINCTRL_SHUTDOWN    "shutdown"

/* In KHz */
#define DEFAULT_SE_CLK  19200
#define UART_CORE2X_VOTE    50000
#define UART_CONSOLE_CORE2X_VOTE    19200

#define DEFAULT_BUS_WIDTH   (4)

#define MIN_SUPPORTED_BAUD_RATE 300
#define MAX_SUPPORTED_BAUD_RATE 8000000

/* Required for polling for 100 msecs */
#define POLL_WAIT_TIMEOUT_MSEC	100

#if IS_ENABLED(CONFIG_MSM_GPI_DMA_DEBUG)
#define GSI_STOP_RX_TIMEOUT	2500
#else
#define GSI_STOP_RX_TIMEOUT	750
#endif

/* Required for polling for 25 msecs */
#define CMD_DONE_TIMEOUT_MSEC	25

/*
 * Number of iterrations required while polling
 * where each iterration has a delay of 100 usecs
 */
#define POLL_ITERATIONS		1000

#define IPC_LOG_MSG(ctx, x...) ipc_log_string(ctx, x)

#define DMA_RX_BUF_SIZE		(2048)
#define UART_CONSOLE_RX_WM	(2)
#define NUM_RX_BUF		4

#define SUSPEND_RETRY_COUNT	(50)

/*1.1 version specifies that is support both ioctl as well as sysfs*/
#define HS_UART_DRIVER_VERSION	"1.1"

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
 * @UART_ERROR_CONTENTION: used when contention encountered on one wire uart bus
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
	UART_ERROR_CONTENTION = 23,

	/* keep last */
	UART_ERROR_CODE_MAX,
};

/*
 * enum uart_port_state: Various port states used by driver
 * @UART_PORT_CLOSED_SHUTDOWN: used when port is either closed or shutdown
 * @UART_PORT_SHUTDOWN_IN_PROGRESS: used when port is about to get closed/shutdown
 * @UART_PORT_OPEN: used when port is open
 */
enum uart_port_state {
	UART_PORT_CLOSED_SHUTDOWN = 0,
	UART_PORT_SHUTDOWN_IN_PROGRESS = 1,
	UART_PORT_OPEN = 2,

	/* keep last */
	UART_PORT_STATE_MAX,
};

/**
 * struct uart_kpi_capture - container for xfer request information
 *
 * @xfer_req_sw: Help to capture Software time stamp for the TX
 * @xfer_req_hw: Help to capture Hardware time stamp for the TX
 * @xfer_req_comp: Help to capture the time stamp for TX req completion
 *
 * This struct used to capture the time stamps at different TX level
 * to help in getting the timer delta between the start and complete of transfer.
 *
 */
struct uart_kpi_capture {
	struct kpi_time xfer_req_sw;
	struct kpi_time xfer_req_hw;
	struct kpi_time xfer_req_comp;
};

struct msm_geni_serial_ver_info {
	int hw_major_ver;
	int hw_minor_ver;
	int hw_step_ver;
	int m_fw_ver;
	int s_fw_ver;
};

struct msm_geni_serial_rsc {
	struct device *ctrl_dev;
	struct device *wrapper_dev;
	struct clk *se_clk;
	struct clk *m_ahb_clk;
	struct clk *s_ahb_clk;
	struct pinctrl *geni_pinctrl;
	struct pinctrl_state *geni_gpio_shutdown;
	struct pinctrl_state *geni_gpio_active;
	struct pinctrl_state *geni_gpio_sleep;
	enum geni_se_protocol_type proto;
};

struct uart_gsi {
	struct dma_chan *tx_c;
	struct dma_chan *rx_c;
	struct msm_gpi_tre tx_cfg0_t;
	struct msm_gpi_tre rx_cfg0_t;
	struct msm_gpi_tre tx_go_t;
	struct msm_gpi_tre rx_go_t;
	struct msm_gpi_tre tx_t[TX_TRE_MAX_INSTANCES];
	struct msm_gpi_tre rx_t[RX_TRE_INSTANCES];
	dma_addr_t tx_ph;
	dma_addr_t rx_ph;
	struct msm_gpi_ctrl tx_ev;
	struct msm_gpi_ctrl rx_ev;
	struct scatterlist tx_sg[TX_SG_INSTANCES];
	struct scatterlist rx_sg[RX_SG_INSTANCES];
	struct dma_async_tx_descriptor *tx_desc;
	struct dma_async_tx_descriptor *rx_desc;
	struct msm_gpi_dma_async_tx_cb_param tx_cb;
	struct msm_gpi_dma_async_tx_cb_param rx_cb;
};

/**
 * struct uart_split_dma_tre - Structure to manage split DMA transfers for UART TX operations
 * @split_tx: boolean to indicate if the tx tre should be split
 * @immediate_dma_in_progress: boolean to indicate immediate transfer in progress
 * @aligned_tx_buf: pointer to aligned address of tx buf
 * @aligned_tx_buf_orig: pointer to allocated address of tx buf
 * @aligned_tx_dma_buf: pointer to dma address of aligned_tx_buf
 * @aligned_tx_dma_buf_orig: pointer to dma address of aligned_tx_buf_orig
 *
 */
struct uart_split_dma_tre {
	bool split_tx;
	bool immediate_dma_in_progress;
	void *aligned_tx_buf;
	void *aligned_tx_buf_orig;
	dma_addr_t aligned_tx_dma_buf;
	dma_addr_t aligned_tx_dma_buf_orig;
};

/*
 * struct one_wire_uart_config: This struct holds configuration details for the one-wire UART.
 *
 * @error: specifies if any one-wire UART errors have occurred
 * @qup_lane: specifies whether the lane used is L2 or L3
 * @output_mode: specifies the UART mode is open-drain or push-pull
 * @enable: specifies whether one-wire UART is enabled or not
 *
 */
struct one_wire_uart_config {
	int error;
	u32 qup_lane;
	bool output_mode;
	bool enable;
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
	unsigned int kpi_idx;
	unsigned int kpi_comp_idx;
	enum geni_se_xfer_mode xfer_mode;
	struct dentry *dbg;
	bool port_setup;
	unsigned int *rx_fifo;
	int (*handle_rx)(struct uart_port *uport,
			unsigned int rx_fifo_wc,
			unsigned int rx_last_byte_valid,
			unsigned int rx_last,
			bool drop_rx);
	struct device *wrapper_dev;
	struct msm_geni_serial_rsc serial_rsc;
	struct  geni_se se;
	dma_addr_t tx_dma;
	unsigned int xmit_size;
	void *rx_buf;
	void *rx_gsi_buf[5];
	dma_addr_t rx_dma;
	dma_addr_t dma_addr[4];
	int loopback;
	int uart_kpi;
	int wakeup_irq;
	unsigned char wakeup_byte;
	struct wakeup_source *geni_wake;
	void *ipc_log_tx;
	void *ipc_log_rx;
	void *ipc_log_pwr;
	void *ipc_log_misc;
	void *ipc_log_kpi;
	void *ipc_log_new;
	void *console_log;
	void *ipc_log_irqstatus;
	unsigned int cur_baud;
	int ioctl_count;
	bool manual_flow;
	struct msm_geni_serial_ver_info ver_info;
	u32 cur_tx_remaining;
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
	bool gsi_mode;
	struct uart_gsi *gsi;
	struct work_struct tx_xfer_work;
	struct work_struct rx_cancel_work;
	struct work_struct tx_cancel_work;
	struct workqueue_struct *tx_wq;
	struct workqueue_struct *rx_wq;
	struct completion xfer;
	struct completion tx_xfer;
	u8 rx_buf_idx;
	atomic_t stop_rx_inprogress;
	bool pm_auto_suspend_disable;
	atomic_t gsi_rx_done;
	int hs_uart_operation;
	atomic_t check_wakeup_byte;
	struct workqueue_struct *wakeup_irq_wq;
	struct delayed_work wakeup_irq_dwork;
	struct completion wakeup_comp;
	atomic_t flush_buffers;
	struct ktermios *current_termios;
	bool resuming_from_deep_sleep;
	enum uart_port_state port_state;
	struct uart_kpi_capture uart_kpi_tx[UART_KPI_TX_RX_INSTANCES];
	struct uart_kpi_capture uart_kpi_rx[UART_KPI_TX_RX_INSTANCES];
	struct uart_split_dma_tre split_dma_tre;
	dma_addr_t ts_dma[TS_DMA_MAX_INSTANCES];
	bool time_stamp;
	struct one_wire_uart_config one_wire_uart;
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
static unsigned char uart_line_id;
static int msm_geni_serial_config_baud_rate(struct uart_port *uport,
					    struct ktermios *termios,
					    unsigned int baud);
static int msm_geni_serial_ioctl(struct uart_port *uport, unsigned int cmd,
				 unsigned long arg);

#define GET_DEV_PORT(uport) \
	container_of(uport, struct msm_geni_serial_port, uport)

static struct msm_geni_serial_port msm_geni_console_port;
static struct msm_geni_serial_port msm_geni_serial_ports[GENI_UART_NR_PORTS];
static void msm_geni_serial_handle_isr(struct uart_port *uport,
				 unsigned long *flags, bool is_irq_masked);
static void msm_geni_uart_ev_cb(struct dma_chan *ch,
				struct msm_gpi_cb const *cb_str,
				void *ptr);
static void setup_config0_tre(struct uart_port *uport,
			      unsigned int bits_per_char,
			      unsigned int clk_div,
			      unsigned int stop_bit_len,
			      unsigned int tx_parity, bool cts_mask,
			      unsigned int rx_parity, unsigned int loopback);
static void msm_geni_uart_gsi_tx_cb(void *ptr);
static void msm_geni_uart_gsi_rx_cb(void *ptr);

/**
 * setup_timestamp_dma_tre() - Configure DMA TRE for timestamp register
 * @msm_port: Pointer to the serial port.
 * @tre: DMA transmit tre pointer
 * @ieot: IEOT (Interrupt End of Transfer) bit value
 * @chain: Chain bit of the tre configuration
 * @ts_dma: Pointer to store the DMA address for timestamp register
 *
 * This function configures a DMA TRE to read from the timestamp register
 * and store the value for accurate timing measurements during UART transfers.
 * The timestamp data is used for performance analysis and debugging.
 *
 * Return: 0 on success, negative error code on failure.
 */
int setup_timestamp_dma_tre(struct msm_geni_serial_port *msm_port, struct msm_gpi_tre *tre,
			    bool ieot, bool chain, dma_addr_t *ts_dma)
{
	int ret;
	struct uart_port *uport = &msm_port->uport;
	struct msm_geni_serial_port *port = GET_DEV_PORT(uport);

	if (!msm_port || !tre || !ts_dma)
		return -EINVAL;

	ret = geni_common_reg_dma_addr(uport->dev, ts_dma, QTIMER_BIN, TIMESTAMP_DATA_SIZE);
	if (ret) {
		UART_LOG_DBG(port->ipc_log_misc, uport->dev,
			     "Failed to get dma addr for timestamp register\n");
		return ret;
	}

	tre->dword[0] = MSM_GPI_DMA_W_BUFFER_TRE_DWORD0(*ts_dma);
	tre->dword[1] = MSM_GPI_DMA_W_BUFFER_TRE_DWORD1(*ts_dma);
	tre->dword[2] = MSM_GPI_DMA_W_BUFFER_TRE_DWORD2(TIMESTAMP_DATA_SIZE);
	tre->dword[3] = MSM_GPI_DMA_W_BUFFER_TRE_DWORD3(0, 0, ieot, 0, chain);

	UART_LOG_DBG(port->ipc_log_misc, uport->dev,
		     "Timestamp:TRE_DWORD0: 0x%x | TRE_DWORD1: 0x%x |\n"
		     "\t\tTRE_DWORD2: 0x%x | TRE_DWORD3: 0x%x\n",
		     tre->dword[0], tre->dword[1], tre->dword[2], tre->dword[3]);

	return ret;
}

/*
 * geni_se_dump_dbg_regs() - Dumps uart debug registers content for debug
 *
 * @uport: pointer to uart port
 *
 * Return: None
 */
void geni_se_dump_dbg_regs(struct uart_port *uport)
{
	u32 m_cmd0 = 0, m_irq_status = 0, s_cmd0 = 0;
	u32 s_irq_status = 0, geni_status = 0, geni_ios = 0;
	u32 dma_rx_irq = 0, dma_tx_irq = 0, rx_fifo_status = 0;
	u32 tx_fifo_status = 0, se_dma_dbg = 0, m_cmd_ctrl = 0;
	u32 se_dma_rx_len = 0, se_dma_rx_len_in = 0, se_dma_tx_len = 0;
	u32 se_dma_tx_len_in = 0, geni_m_irq_en = 0, geni_s_irq_en = 0;
	u32 geni_dma_tx_irq_en = 0, geni_dma_rx_irq_en = 0;
	u32 ser_m_clk_cfg = 0, ser_s_clk_cfg = 0, loopback_cfg = 0;
	u32 io_macro_ctrl = 0, io3_val = 0, tx_trans_cfg = 0;
	u32 rx_trans_cfg = 0, tx_word_len = 0, stop_bit_len = 0;
	u32 tx_trans_len = 0, rx_word_len = 0, rx_stale_cnt = 0;
	u32 tx_parity_cfg = 0, rx_parity_cfg = 0, manual_rfr = 0;
	u32 tx_watermark = 0, rx_watermark = 0, rx_watermark_rfr = 0;
	u32 se_geni_general_cfg = 0, m_cmd_err = 0, m_fw_err = 0;
	u32 rx_len_in = 0, m_gp_length  = 0, s_gp_length = 0;
	u32 dma_tx_ptr_l = 0, dma_tx_ptr_h = 0, dma_tx_attr = 0;
	u32 dma_tx_max_burst_size = 0, dma_rx_ptr_l = 0, dma_rx_ptr_h = 0;
	u32 dma_rx_attr = 0, dma_rx_max_burst_size = 0, dma_if_en = 0;
	u32 geni_clk_ctrl_ro = 0, fifo_if_disable_ro = 0;
	u32 geni_output_ctrl = 0, geni_fw_multilock_msa_ro = 0;
	u32 geni_clk_sel = 0, geni_m_irq_enable = 0;
	u32 se_gsi_event_en = 0, se_irq_en = 0;
	u32 dma_if_en_ro = 0, dma_general_cfg = 0, dma_qsb_trans_cfg = 0;

	struct msm_geni_serial_port *port = GET_DEV_PORT(uport);
	void __iomem *base = uport->membase;

	if (!pm_runtime_active(uport->dev)) {
		UART_LOG_DBG(port->ipc_log_misc, uport->dev,
			     "%s: Device is not in active state, Return\n", __func__);
		return;
	}

	m_cmd0 = geni_read_reg(base, SE_GENI_M_CMD0);
	m_irq_status = geni_read_reg(base, SE_GENI_M_IRQ_STATUS);
	s_cmd0 = geni_read_reg(base, SE_GENI_S_CMD0);
	s_irq_status = geni_read_reg(base, SE_GENI_S_IRQ_STATUS);
	geni_status = geni_read_reg(base, SE_GENI_STATUS);
	geni_ios = geni_read_reg(base, SE_GENI_IOS);
	dma_tx_irq = geni_read_reg(base, SE_DMA_TX_IRQ_STAT);
	dma_rx_irq = geni_read_reg(base, SE_DMA_RX_IRQ_STAT);
	rx_fifo_status = geni_read_reg(base, SE_GENI_RX_FIFO_STATUS);
	tx_fifo_status = geni_read_reg(base, SE_GENI_TX_FIFO_STATUS);
	se_dma_dbg = geni_read_reg(base, SE_DMA_DEBUG_REG0);
	m_cmd_ctrl = geni_read_reg(base, SE_GENI_M_CMD_CTRL_REG);
	se_dma_rx_len = geni_read_reg(base, SE_DMA_RX_LEN);
	se_dma_rx_len_in = geni_read_reg(base, SE_DMA_RX_LEN_IN);
	se_dma_tx_len = geni_read_reg(base, SE_DMA_TX_LEN);
	se_dma_tx_len_in = geni_read_reg(base, SE_DMA_TX_LEN_IN);
	geni_m_irq_en = geni_read_reg(base, SE_GENI_M_IRQ_EN);
	geni_s_irq_en = geni_read_reg(base, SE_GENI_S_IRQ_EN);
	geni_dma_tx_irq_en = geni_read_reg(base, SE_DMA_TX_IRQ_EN);
	geni_dma_rx_irq_en = geni_read_reg(base, SE_DMA_RX_IRQ_EN);
	ser_m_clk_cfg = geni_read_reg(base, GENI_SER_M_CLK_CFG);
	ser_s_clk_cfg = geni_read_reg(base, GENI_SER_S_CLK_CFG);
	loopback_cfg = geni_read_reg(base, SE_UART_LOOPBACK_CFG);
	io_macro_ctrl = geni_read_reg(base, SE_UART_IO_MACRO_CTRL);
	io3_val = geni_read_reg(base, SE_UART_IO3_VAL);
	tx_trans_cfg = geni_read_reg(base, SE_UART_TX_TRANS_CFG);
	rx_trans_cfg = geni_read_reg(base, SE_UART_RX_TRANS_CFG);
	tx_word_len = geni_read_reg(base, SE_UART_TX_WORD_LEN);
	stop_bit_len = geni_read_reg(base, SE_UART_TX_STOP_BIT_LEN);
	tx_trans_len = geni_read_reg(base, SE_UART_TX_TRANS_LEN);
	rx_word_len = geni_read_reg(base, SE_UART_RX_WORD_LEN);
	rx_stale_cnt = geni_read_reg(base, SE_UART_RX_STALE_CNT);
	tx_parity_cfg = geni_read_reg(base, SE_UART_TX_PARITY_CFG);
	rx_parity_cfg = geni_read_reg(base, SE_UART_RX_PARITY_CFG);
	manual_rfr = geni_read_reg(base, SE_UART_MANUAL_RFR);
	tx_watermark = geni_read_reg(base, SE_GENI_TX_WATERMARK_REG);
	rx_watermark = geni_read_reg(base, SE_GENI_RX_WATERMARK_REG);
	rx_watermark_rfr = geni_read_reg(base, SE_GENI_RX_RFR_WATERMARK_REG);
	se_geni_general_cfg = geni_read_reg(base, SE_GENI_GENERAL_CFG);
	m_cmd_err = geni_read_reg(base, M_CMD_ERR_STATUS);
	m_fw_err = geni_read_reg(base, M_FW_ERR_STATUS);
	rx_len_in = geni_read_reg(base, SE_DMA_RX_LEN_IN);
	m_gp_length  = geni_read_reg(base, M_GP_LENGTH);
	s_gp_length = geni_read_reg(base, S_GP_LENGTH);
	dma_tx_ptr_l = geni_read_reg(base, SE_DMA_TX_PTR_L);
	dma_tx_ptr_h = geni_read_reg(base, SE_DMA_TX_PTR_H);
	dma_tx_attr = geni_read_reg(base, SE_DMA_TX_ATTR);
	dma_tx_max_burst_size = geni_read_reg(base, SE_DMA_TX_MAX_BURST);
	dma_rx_ptr_l = geni_read_reg(base, SE_DMA_RX_PTR_L);
	dma_rx_ptr_h = geni_read_reg(base, SE_DMA_RX_PTR_H);
	dma_rx_attr = geni_read_reg(base, SE_DMA_RX_ATTR);
	dma_rx_max_burst_size = geni_read_reg(base, SE_DMA_RX_MAX_BURST);
	dma_if_en = geni_read_reg(base, SE_DMA_IF_EN);
	geni_clk_ctrl_ro = geni_read_reg(base, GENI_CLK_CTRL_RO);
	fifo_if_disable_ro = geni_read_reg(base, GENI_IF_DISABLE_RO);
	geni_output_ctrl = geni_read_reg(base, GENI_OUTPUT_CTRL);
	geni_fw_multilock_msa_ro = geni_read_reg(base, GENI_FW_MULTILOCK_MSA_RO);
	geni_clk_sel = geni_read_reg(base, SE_GENI_CLK_SEL);
	geni_m_irq_enable = geni_read_reg(base, M_IRQ_ENABLE);
	se_gsi_event_en = geni_read_reg(base, SE_GSI_EVENT_EN);
	se_irq_en = geni_read_reg(base, SE_IRQ_EN);
	dma_if_en_ro = geni_read_reg(base, DMA_IF_EN_RO);
	dma_general_cfg = geni_read_reg(base, DMA_GENERAL_CFG);
	dma_qsb_trans_cfg = geni_read_reg(base, SE_DMA_QSB_TRANS_CFG);

	UART_LOG_DBG(port->ipc_log_misc, uport->dev,
		     "%s: m_cmd0:0x%x, m_irq_status:0x%x, geni_status:0x%x, geni_ios:0x%x\n",
		     __func__, m_cmd0, m_irq_status, geni_status, geni_ios);
	UART_LOG_DBG(port->ipc_log_misc, uport->dev,
		     "dma_rx_irq:0x%x, dma_tx_irq:0x%x, rx_fifo_sts:0x%x, tx_fifo_sts:0x%x\n",
		     dma_rx_irq, dma_tx_irq, rx_fifo_status, tx_fifo_status);
	UART_LOG_DBG(port->ipc_log_misc, uport->dev,
		     "se_dma_dbg:0x%x, m_cmd_ctrl:0x%x, dma_rxlen:0x%x, dma_rxlen_in:0x%x\n",
		     se_dma_dbg, m_cmd_ctrl, se_dma_rx_len, se_dma_rx_len_in);
	UART_LOG_DBG(port->ipc_log_misc, uport->dev,
		     "dma_txlen:0x%x, dma_txlen_in:0x%x s_irq_status:0x%x\n",
		     se_dma_tx_len, se_dma_tx_len_in, s_irq_status);
	UART_LOG_DBG(port->ipc_log_misc, uport->dev,
		     "dma_txirq_en:0x%x, dma_rxirq_en:0x%x geni_m_irq_en:0x%x geni_s_irq_en:0x%x\n",
		     geni_dma_tx_irq_en, geni_dma_rx_irq_en, geni_m_irq_en,
		     geni_s_irq_en);
	UART_LOG_DBG(port->ipc_log_misc, uport->dev,
		     "ser_m_clk_cfg:0x%x, ser_s_clk_cfg:0x%x loopback_cfg:0x%x io_macro_ctrl:0x%x\n",
		     ser_m_clk_cfg, ser_s_clk_cfg, loopback_cfg,
		     io_macro_ctrl);
	UART_LOG_DBG(port->ipc_log_misc, uport->dev,
		     "io3_val:0x%x, tx_trans_cfg:0x%x rx_trans_cfg:0x%x tx_word_len:0x%x\n",
		     io3_val, tx_trans_cfg, rx_trans_cfg,
		     tx_word_len);
	UART_LOG_DBG(port->ipc_log_misc, uport->dev,
		     "stop_bit_len:0x%x, tx_trans_len:0x%x rx_word_len:0x%x rx_stale_cnt:0x%x\n",
		     stop_bit_len, tx_trans_len, rx_word_len,
		     rx_stale_cnt);
	UART_LOG_DBG(port->ipc_log_misc, uport->dev,
		     "tx_parity_cfg:0x%x, rx_parity_cfg:0x%x manual_rfr:0x%x tx_watermark:0x%x\n",
		     tx_parity_cfg, rx_parity_cfg, manual_rfr,
		     tx_watermark);
	UART_LOG_DBG(port->ipc_log_misc, uport->dev,
		     "rx_watermark:0x%x, rx_watermark_rfr:0x%x se_geni_general_cfg:0x%x m_cmd_err:0x%x\n",
		     rx_watermark, rx_watermark_rfr, se_geni_general_cfg,
		     m_cmd_err);
	UART_LOG_DBG(port->ipc_log_misc, uport->dev,
		     "m_fw_err:0x%x, rx_len_in:0x%x m_gp_length:0x%x s_gp_length:0x%x\n",
		     m_fw_err, rx_len_in, m_gp_length,
		     s_gp_length);
	UART_LOG_DBG(port->ipc_log_misc, uport->dev,
		     "dma_tx_ptr_l:0x%x, dma_tx_ptr_h:0x%x dma_tx_attr:0x%x dma_tx_max_burst_size:0x%x\n",
		     dma_tx_ptr_l, dma_tx_ptr_h, dma_tx_attr,
		     dma_tx_max_burst_size);
	UART_LOG_DBG(port->ipc_log_misc, uport->dev,
		     "dma_rx_ptr_l:0x%x, dma_rx_ptr_h:0x%x dma_rx_attr:0x%x dma_rx_max_burst_size:0x%x\n",
		     dma_rx_ptr_l, dma_rx_ptr_h, dma_rx_attr,
		     dma_rx_max_burst_size);
	UART_LOG_DBG(port->ipc_log_misc, uport->dev,
		     "dma_if_en:0x%x, geni_clk_ctrl:0x%x fifo_if_disable:0x%x\n",
		     dma_if_en, geni_clk_ctrl_ro, fifo_if_disable_ro);

	UART_LOG_DBG(port->ipc_log_misc, uport->dev,
		     "geni_output_ctrl:0x%x, geni_fw_multilock_msa_ro:0x%x\n",
		     geni_output_ctrl, geni_fw_multilock_msa_ro);

	UART_LOG_DBG(port->ipc_log_misc, uport->dev,
		     "geni_clk_sel:0x%x, geni_m_irq_enable:0x%x\n",
		      geni_clk_sel, geni_m_irq_enable);

	UART_LOG_DBG(port->ipc_log_misc, uport->dev,
		     "se_gsi_event_en:0x%x, se_irq_en:0x%x, dma_qsb_trans_cfg:0x%x\n",
		     se_gsi_event_en, se_irq_en, dma_qsb_trans_cfg);

	UART_LOG_DBG(port->ipc_log_misc, uport->dev,
		     "dma_if_en_ro:0x%x, dma_general_cfg:0x%x\n",
		     dma_if_en_ro, dma_general_cfg);

	if (IS_ENABLED(CONFIG_MSM_GPI_DMA) && port->xfer_mode == GENI_GPI_DMA &&
	    port->gsi && port->gsi->tx_c)
		gpi_dump_for_geni(port->gsi->tx_c);

}

int msm_geni_serial_resources_on(struct msm_geni_serial_port *port)
{
	int ret = 0;
	struct msm_geni_serial_rsc *rsc = &port->serial_rsc;
	unsigned long long start_time;

	start_time = geni_capture_start_time(&port->se, port->ipc_log_kpi,
					     __func__, port->uart_kpi);
	if (unlikely(!rsc))
		return -EINVAL;

	ret = pinctrl_select_state(rsc->geni_pinctrl, rsc->geni_gpio_active);
	if (ret) {
		UART_LOG_DBG(port->ipc_log_misc, port->uport.dev,
			"%s: Error %d pinctrl_select_state failed\n", __func__, ret);
		return ret;
	}

	UART_LOG_DBG(port->ipc_log_misc, port->uport.dev, "%s: geni_icc_enable\n", __func__);
	/* Set BW for register access */
	ret = geni_icc_enable(&port->se);
	if (ret) {
		UART_LOG_DBG(port->ipc_log_misc, port->uport.dev,
			     "%s: Error %d geni_icc_enable failed\n", __func__, ret);
		return ret;
	}

	UART_LOG_DBG(port->ipc_log_misc, port->uport.dev, "%s: ICC BW voting\n", __func__);
	ret = geni_icc_set_bw(&port->se);
	if (ret) {
		UART_LOG_DBG(port->ipc_log_misc, port->uport.dev,
			     "%s: Error %d ICC BW voting failed\n", __func__, ret);
		return ret;
	}

	UART_LOG_DBG(port->ipc_log_misc, port->uport.dev, "%s: geni_se_common_clks_on\n", __func__);
	ret = geni_se_common_clks_on(rsc->se_clk, rsc->m_ahb_clk, rsc->s_ahb_clk);
	if (ret) {
		UART_LOG_DBG(port->ipc_log_misc, port->uport.dev,
			    "%s: Error %d geni_se_common_clks_on failed\n", __func__, ret);
		return ret;
	}

	UART_LOG_DBG(port->ipc_log_misc, port->uport.dev, "%s: GENI_OUTPUT_CTRL write\n", __func__);
	/*
	 * Reprogram GENI output control register, to ensure QUP to Pad output path
	 * is retained post suspend. Added 10us delay to settle the write of the
	 * register as per HW team recommendation.
	 */
	geni_write_reg(0x7F,  port->uport.membase, GENI_OUTPUT_CTRL);
	udelay(10);
	geni_capture_stop_time(&port->se, port->ipc_log_kpi, __func__,
			       port->uart_kpi, start_time, 0, 0);

	return ret;
}

int msm_geni_serial_resources_off(struct msm_geni_serial_port *port)
{
	int ret = 0;
	struct msm_geni_serial_rsc *rsc = &port->serial_rsc;

	if (unlikely(!rsc))
		return -EINVAL;

	geni_se_common_clks_off(rsc->se_clk, rsc->m_ahb_clk, rsc->s_ahb_clk);

	/* Set BW for register access */
	ret = geni_icc_disable(&port->se);
	if (ret) {
		UART_LOG_DBG(port->ipc_log_misc, port->uport.dev,
			"%s: Error %d geni_icc_disable failed\n", __func__, ret);
		return ret;
	}

	if (port->port_state == UART_PORT_CLOSED_SHUTDOWN) {
		ret = pinctrl_select_state(rsc->geni_pinctrl, rsc->geni_gpio_shutdown);
		if (ret)
			UART_LOG_DBG(port->ipc_log_misc, port->uport.dev,
				     "%s: Error %d pinctrl shutdown state failed\n", __func__, ret);
	} else {
		ret = pinctrl_select_state(rsc->geni_pinctrl, rsc->geni_gpio_sleep);
		if (ret)
			UART_LOG_DBG(port->ipc_log_misc, port->uport.dev,
				     "%s: Error %d pinctrl sleep failed\n", __func__, ret);
	}
	return ret;
}

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

	UART_LOG_DBG(msm_port->ipc_log_misc, msm_port->uport.dev,
		"%s: enable:%d ser_clk_cfg:%lu\n", __func__, enable,
		msm_port->ser_clk_cfg);

	if (enable) {
		geni_write_reg(msm_port->ser_clk_cfg, uport->membase, GENI_SER_M_CLK_CFG);
		geni_write_reg(msm_port->ser_clk_cfg, uport->membase, GENI_SER_S_CLK_CFG);
		geni_read_reg(uport->membase, GENI_SER_M_CLK_CFG);
	} else {
		geni_write_reg(0x1, uport->membase, GENI_SER_M_CLK_CFG);
		geni_write_reg(0x1, uport->membase, GENI_SER_S_CLK_CFG);
		geni_read_reg(uport->membase, GENI_SER_M_CLK_CFG);
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

	geni_m_irq_en = geni_read_reg(uport->membase,
						SE_GENI_M_IRQ_EN);
	geni_s_irq_en = geni_read_reg(uport->membase,
						SE_GENI_S_IRQ_EN);

	geni_m_irq_en |= M_IRQ_BITS;
	geni_s_irq_en |= S_IRQ_BITS;

	/*
	 * Enable Rx Frame error & Rx Break error, Contention IRQ for one-wire UART,
	 * CTS interrupts only if Port is in open state.
	 */
	if (uport->state && uport->state->port.tty) {
		geni_m_irq_en |= (M_IO_DATA_DEASSERT_EN | M_IO_DATA_ASSERT_EN);
		geni_s_irq_en |= (S_GP_IRQ_1_EN | S_GP_IRQ_2_EN | S_GP_IRQ_3_EN);
		if (port->one_wire_uart.enable &&
		    port->one_wire_uart.output_mode == OPEN_DRAIN_MODE) {
			geni_m_irq_en |= M_GP_IRQ_0_EN;
			port->one_wire_uart.error = 0;
		}
	} else {
		geni_m_irq_en &= ~(M_IO_DATA_DEASSERT_EN | M_IO_DATA_ASSERT_EN |
				   M_RX_FIFO_LAST_EN);
		geni_s_irq_en &= ~(S_GP_IRQ_1_EN | S_GP_IRQ_2_EN | S_GP_IRQ_3_EN);
	}
	UART_LOG_DBG(port->ipc_log_irqstatus, uport->dev,
		     "%s: geni_m_irq_en = 0x%x geni_s_irq_en = 0x%x\n",
		     __func__, geni_m_irq_en, geni_s_irq_en);

	if (port->gsi_mode) {
		geni_m_irq_en &= ~M_RX_FIFO_WATERMARK_EN;
		geni_s_irq_en &= ~S_RX_FIFO_WATERMARK_EN;
	}

	geni_write_reg(geni_m_irq_en, uport->membase, SE_GENI_M_IRQ_EN);
	geni_write_reg(geni_s_irq_en, uport->membase, SE_GENI_S_IRQ_EN);
	if (port->xfer_mode == GENI_SE_DMA) {
		geni_write_reg(DMA_TX_IRQ_BITS, uport->membase,
							SE_DMA_TX_IRQ_EN_SET);
		geni_write_reg(DMA_RX_IRQ_BITS, uport->membase,
							SE_DMA_RX_IRQ_EN_SET);
	}

	if (!uart_console(uport)) {
		geni_m_irq_en = geni_read_reg(uport->membase,
				SE_GENI_M_IRQ_EN);
		geni_s_irq_en = geni_read_reg(uport->membase,
				SE_GENI_S_IRQ_EN);
		geni_dma_tx_irq_en = geni_read_reg(uport->membase,
				SE_DMA_TX_IRQ_EN);
		geni_dma_rx_irq_en = geni_read_reg(uport->membase,
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

	geni_m_irq_en = geni_read_reg(uport->membase, SE_GENI_M_IRQ_EN);
	geni_s_irq_en = geni_read_reg(uport->membase, SE_GENI_S_IRQ_EN);

	geni_m_irq_en &= ~M_IRQ_BITS;
	geni_s_irq_en &= ~S_IRQ_BITS;

	if (port->one_wire_uart.enable &&
	    port->one_wire_uart.output_mode == OPEN_DRAIN_MODE)
		geni_m_irq_en &= ~M_GP_IRQ_0_EN;

	geni_write_reg(geni_m_irq_en, uport->membase, SE_GENI_M_IRQ_EN);
	geni_write_reg(geni_s_irq_en, uport->membase, SE_GENI_S_IRQ_EN);

	if (port->xfer_mode == GENI_SE_DMA) {
		geni_write_reg(DMA_TX_IRQ_BITS, uport->membase,
							SE_DMA_TX_IRQ_EN_CLR);
		geni_write_reg(DMA_RX_IRQ_BITS, uport->membase,
							SE_DMA_RX_IRQ_EN_CLR);
	}

	if (!uart_console(uport)) {
		geni_m_irq_en = geni_read_reg(uport->membase,
				SE_GENI_M_IRQ_EN);
		geni_s_irq_en = geni_read_reg(uport->membase,
				SE_GENI_S_IRQ_EN);
		geni_dma_tx_irq_en = geni_read_reg(uport->membase,
				SE_DMA_TX_IRQ_EN);
		geni_dma_rx_irq_en = geni_read_reg(uport->membase,
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
		/* Waiting for 25 milli second for interrupt to be fired */
		if (msm_port->m_cmd)
			timeout = wait_for_completion_timeout
					(&msm_port->m_cmd_timeout,
				msecs_to_jiffies(CMD_DONE_TIMEOUT_MSEC));
		else if (msm_port->s_cmd)
			timeout = wait_for_completion_timeout
					(&msm_port->s_cmd_timeout,
				msecs_to_jiffies(CMD_DONE_TIMEOUT_MSEC));
	}
	return timeout ? 0 : 1;
}

static void msm_geni_serial_config_port(struct uart_port *uport, int cfg_flags)
{
	if (cfg_flags & UART_CONFIG_TYPE)
		uport->type = PORT_MSM;
}

/*
 * capture_kpi_show() - Prints the value stored in capture_kpi sysfs entry
 *
 * @dev: pointer to device
 * @attr: device attributes
 * @buf: buffer to store the capture_kpi_value
 *
 * Return: prints capture_kpi value
 */
static ssize_t capture_kpi_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct msm_geni_serial_port *port = platform_get_drvdata(pdev);

	return scnprintf(buf, sizeof(int), "%d\n", port->uart_kpi);
}

/*
 * capture_kpi_store() - store the capture_kpi sysfs value
 *
 * @uport: pointer to device
 * @attr: device attributes
 * @buf: buffer to store the capture_kpi_value
 * @size: returns the value of size.
 *
 * Return: Size copied in the buffer
 */
static ssize_t capture_kpi_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t size)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct msm_geni_serial_port *port = platform_get_drvdata(pdev);
	struct uart_port *uport;
	char name[35];

	uport = &port->uport;
	if (kstrtoint(buf, 0, &port->uart_kpi)) {
		dev_err(dev, "Invalid input\n");
		return -EINVAL;
	}

	/* ipc logs for kpi's measure */
	if (port->uart_kpi) {
		if (!port->ipc_log_kpi) {
			memset(name, 0, sizeof(name));
			scnprintf(name, sizeof(name), "%s%s", dev_name(uport->dev), "_kpi");
			port->ipc_log_kpi = ipc_log_context_create(IPC_LOG_KPI_PAGES, name, 0);
			if (!port->ipc_log_kpi)
				dev_info(uport->dev, "Err in KPI IPC Log\n");
		}
	}
	return size;
}

static DEVICE_ATTR_RW(capture_kpi);

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

/*
 * hs_uart_operation_show() - read last uart operation status value.
 *
 * @dev: pointer to device dev
 * @attr: attributes associatd with dev
 * @buf: output buffer
 *
 * Return: Size copied in the buffer
 */
static ssize_t hs_uart_operation_show(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct msm_geni_serial_port *port = platform_get_drvdata(pdev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", port->hs_uart_operation);
}

/*
 * hs_uart_operation_store() - write command in sysfs which further forward it to ioctl function.
 *
 * @dev: pointer to device dev
 * @attr: attributes associatd with dev
 * @buf: input buffer
 * @size: size of the buffer
 *
 * Return: Size copied in the buffer
 */
static ssize_t hs_uart_operation_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf,
				       size_t size)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct msm_geni_serial_port *port = platform_get_drvdata(pdev);
	u16 cmd = 0;
	int ret = -ENOIOCTLCMD;

	if (kstrtou16(buf, 16, &cmd)) {
		UART_LOG_DBG(port->ipc_log_misc, port->uport.dev, "%s: Invalid input\n", __func__);
		return -EINVAL;
	}
	UART_LOG_DBG(port->ipc_log_misc, port->uport.dev, "%s: cmd: %u\n", __func__, cmd);

	ret = msm_geni_serial_ioctl(&port->uport, cmd, 0);

	switch (cmd) {
	case MSM_GENI_SERIAL_TIOCFAULT:
	case MSM_GENI_SERIAL_TIOCPMACT:
		/*
		 * No need to check return value as it returns
		 * status value and it can be non-zero as well
		 */
		break;

	case MSM_GENI_SERIAL_TIOCTSSET:
		port->time_stamp = true;
		break;

	case MSM_GENI_SERIAL_TIOCTSCLR:
		port->time_stamp = false;
		break;

	default:
		if (ret) {
			UART_LOG_DBG(port->ipc_log_misc, port->uport.dev,
				     "%s: Can not perform operation cmd: 0x%x ret: %d\n",
				     __func__, cmd, ret);
			return -EINVAL;
		}
	}
	port->hs_uart_operation = ret;
	return size;
}

static DEVICE_ATTR_RW(hs_uart_operation);

/*
 * hs_uart_version_show() - read version of the driver which further defines driver
 * compatibility of ioctl and/or sysfs.
 *
 * @dev: pointer to device dev
 * @attr: attributes associatd with dev
 * @buf: output buffer
 *
 * Return: Size copied in the buffer
 */
static ssize_t hs_uart_version_show(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct msm_geni_serial_port *port = platform_get_drvdata(pdev);

	UART_LOG_DBG(port->ipc_log_misc, port->uport.dev, "%s: Version: %s\n",
		     __func__, HS_UART_DRIVER_VERSION);

	return scnprintf(buf, PAGE_SIZE, "%s\n", HS_UART_DRIVER_VERSION);
}

static DEVICE_ATTR_RO(hs_uart_version);

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

static bool check_transfers_inflight(struct uart_port *uport)
{
	bool xfer_on = false;
	bool tx_active = false;
	bool tx_fifo_status = false;
	bool m_cmd_active = false;
	bool rx_active = false;
	u32 rx_fifo_status = 0;
	struct msm_geni_serial_port *port = GET_DEV_PORT(uport);
	u32 geni_status = geni_read_reg(uport->membase,
						SE_GENI_STATUS);
	struct tty_port *tport = &uport->state->port;

	/* Possible stop tx is called multiple times. */
	m_cmd_active = geni_status & M_GENI_CMD_ACTIVE;
	if (port->xfer_mode == GENI_SE_DMA) {
		tx_fifo_status = port->tx_dma ? 1 : 0;
		rx_fifo_status =
			geni_read_reg(uport->membase, SE_DMA_RX_LEN_IN);
	} else {
		tx_fifo_status = geni_read_reg(uport->membase,
						SE_GENI_TX_FIFO_STATUS);
		rx_fifo_status = geni_read_reg(uport->membase,
						SE_GENI_RX_FIFO_STATUS);
	}
	tx_active = m_cmd_active || tx_fifo_status;
	rx_active =  rx_fifo_status ? true : false;

	if (rx_active || tx_active || !kfifo_is_empty(&tport->xmit_fifo))
		xfer_on = true;

	return xfer_on;
}

static int wait_for_transfers_inflight(struct uart_port *uport)
{
	int iter = 0;
	struct msm_geni_serial_port *port = GET_DEV_PORT(uport);
	unsigned int geni_status;
	u32 rx_len_in = 0;

	geni_status = geni_read_reg(uport->membase, SE_GENI_STATUS);
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
			geni_read_reg(uport->membase, SE_DMA_RX_LEN_IN);
		if (rx_len_in) {
			UART_LOG_DBG(port->ipc_log_misc, uport->dev,
				"%s: Bailout rx_len_in is set %d\n", __func__, rx_len_in);
			return -EBUSY;
		}
		geni_se_dump_dbg_regs(uport);
	}
	return 0;
}

static int vote_clock_on(struct uart_port *uport)
{
	struct msm_geni_serial_port *port = GET_DEV_PORT(uport);
	int usage_count;
	int ret = 0;
	unsigned long long start_time;

	start_time = geni_capture_start_time(&port->se, port->ipc_log_kpi,
					     __func__, port->uart_kpi);
	UART_LOG_DBG(port->ipc_log_pwr, uport->dev,
		     "Enter %s:%s ioctl_count:%d\n",
		     __func__, current->comm, port->ioctl_count);
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
	geni_capture_stop_time(&port->se, port->ipc_log_kpi,
			       __func__, port->uart_kpi, start_time, 0, 0);
	return 0;
}

static int vote_clock_off(struct uart_port *uport)
{
	struct msm_geni_serial_port *port = GET_DEV_PORT(uport);
	int usage_count;
	int ret = 0;
	unsigned long long start_time;

	start_time = geni_capture_start_time(&port->se, port->ipc_log_kpi,
					     __func__, port->uart_kpi);

	UART_LOG_DBG(port->ipc_log_pwr, uport->dev,
		     "Enter %s:%s ioctl_count:%d\n",
		     __func__, current->comm, port->ioctl_count);
	if (!pm_runtime_enabled(uport->dev) || !pm_runtime_active(uport->dev)) {
		dev_err(uport->dev,
			"RPM not available or not active, clock status read would fail\n");
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

	geni_capture_stop_time(&port->se, port->ipc_log_kpi,
			       __func__, port->uart_kpi, start_time, 0, 0);

	return 0;
};

static int msm_geni_serial_ioctl(struct uart_port *uport, unsigned int cmd,
						unsigned long arg)
{
	struct msm_geni_serial_port *port = GET_DEV_PORT(uport);
	int ret = -ENOIOCTLCMD;
	enum uart_error_code uart_error;

	UART_LOG_DBG(port->ipc_log_misc, uport->dev,
		     "%s:%s cmd 0x%x\n", __func__, current->comm, cmd);
	if (port->pm_auto_suspend_disable)
		return ret;

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

		if (port->ioctl_count)
			geni_se_dump_dbg_regs(uport);
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

	if (!uart_console(uport) && !pm_runtime_active(uport->dev)) {
		UART_LOG_DBG(port->ipc_log_misc, uport->dev,
			     "%s.Device is not in active state, %s\n", __func__, current->comm);
		return;
	}

	if (ctl) {
		ret = wait_for_transfers_inflight(uport);
		if (ret)
			UART_LOG_DBG(port->ipc_log_misc, uport->dev,
				  "%s.wait_for_transfer_inflight return ret: %d\n",
				__func__, ret);
		geni_se_setup_m_cmd(&port->se, UART_START_BREAK, 0);
	} else {
		geni_se_setup_m_cmd(&port->se, UART_STOP_BREAK, 0);
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

	if (!uart_console(uport) && !pm_runtime_active(uport->dev)) {
		UART_LOG_DBG(port->ipc_log_misc, uport->dev,
			     "%s.Device is not in active state, %s\n", __func__, current->comm);
		return mctrl | TIOCM_CTS;
	}

	geni_ios = geni_read_reg(uport->membase, SE_GENI_IOS);
	if (!(geni_ios & IO2_DATA_IN))
		mctrl |= TIOCM_CTS;
	else if (!uart_console(uport))
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

	if (!uart_console(uport) && !pm_runtime_active(uport->dev)) {
		UART_LOG_DBG(port->ipc_log_misc, uport->dev,
			     "%s.Device is not in active state, %s: mctrl=0x%x\n",
			     __func__, current->comm, mctrl);
		return;
	}

	if (!(mctrl & TIOCM_RTS)) {
		uart_manual_rfr |= (UART_MANUAL_RFR_EN | UART_RFR_NOT_READY);
		port->manual_flow = true;
	} else {
		port->manual_flow = false;
	}
	geni_write_reg(uart_manual_rfr, uport->membase,	SE_UART_MANUAL_RFR);

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

	if (pm_runtime_enabled(uport->dev)) {
		pm_runtime_mark_last_busy(uport->dev);
		pm_runtime_put_autosuspend(uport->dev);
	}
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
		reg = geni_read_reg(uport->membase, offset);
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

	geni_write_reg(xmit_size, uport->membase, SE_UART_TX_TRANS_LEN);
	m_cmd |= (UART_START_TX << M_OPCODE_SHFT);
	geni_write_reg(m_cmd, uport->membase, SE_GENI_M_CMD0);
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
			geni_write_reg(M_GENI_CMD_ABORT, uport->membase,
						SE_GENI_M_CMD_CTRL_REG);
			msm_geni_serial_poll_bit(uport, SE_GENI_M_IRQ_STATUS,
						M_CMD_ABORT_EN, true);
		}
	}
	irq_clear = geni_read_reg(uport->membase, SE_GENI_M_IRQ_STATUS);
	geni_write_reg(irq_clear, uport->membase, SE_GENI_M_IRQ_CLEAR);
}

static void msm_uart_log_tx_kpi(struct uart_port *uport)
{
	struct msm_geni_serial_port *msm_port = GET_DEV_PORT(uport);
	u64 comp_time, sw_time, exec_time;
	u32 len;

	if (!msm_port->uart_kpi)
		return;

	msm_port->uart_kpi_tx[msm_port->kpi_comp_idx].xfer_req_comp.len = msm_port->xmit_size;
	msm_port->uart_kpi_tx[msm_port->kpi_comp_idx].xfer_req_comp.time_stamp = sched_clock();

	len = msm_port->uart_kpi_tx[msm_port->kpi_comp_idx].xfer_req_comp.len;
	comp_time = msm_port->uart_kpi_tx[msm_port->kpi_comp_idx].xfer_req_comp.time_stamp;

	/* SW time */
	sw_time = msm_port->uart_kpi_tx[msm_port->kpi_comp_idx].xfer_req_sw.time_stamp;
	exec_time = comp_time - sw_time;
	UART_LOG_DBG(msm_port->ipc_log_kpi, uport->dev,
		     "%s: TX SW time: %llu nsec(%llu usec) for bytes: %d with freq: %d index: %d\n",
		     __func__, exec_time, (exec_time / 1000), len, msm_port->cur_baud,
		     msm_port->kpi_comp_idx);

	/* HW time */
	sw_time = msm_port->uart_kpi_tx[msm_port->kpi_comp_idx].xfer_req_hw.time_stamp;
	exec_time = comp_time - sw_time;
	UART_LOG_DBG(msm_port->ipc_log_kpi, uport->dev,
		     "%s: TX HW time: %llu nsec(%llu usec) for bytes: %d with freq: %d index: %d\n",
		     __func__, exec_time, (exec_time / 1000), len, msm_port->cur_baud,
		     msm_port->kpi_comp_idx);

	msm_port->kpi_comp_idx = (msm_port->kpi_comp_idx + 1) % UART_KPI_TX_RX_INSTANCES;
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

	m_irq_status = geni_read_reg(uport->membase,
						SE_GENI_M_IRQ_STATUS);
	s_irq_status = geni_read_reg(uport->membase,
						SE_GENI_S_IRQ_STATUS);
	geni_write_reg(m_irq_status, uport->membase,
						SE_GENI_M_IRQ_CLEAR);
	geni_write_reg(s_irq_status, uport->membase,
						SE_GENI_S_IRQ_CLEAR);

	if (!(msm_geni_serial_poll_bit(uport, SE_GENI_RX_FIFO_STATUS,
			RX_FIFO_WC_MSK, true)))
		return -ENXIO;

	/*
	 * Read the Rx FIFO only after clearing the interrupt registers and
	 * getting valid RX fifo status.
	 */
	mb();
	rx_fifo = geni_read_reg(uport->membase, SE_GENI_RX_FIFOn);
	rx_fifo &= 0xFF;
	return rx_fifo;
}

static void msm_geni_serial_poll_put_char(struct uart_port *uport,
					unsigned char c)
{
	int b = (int) c;
	struct msm_geni_serial_port *port = GET_DEV_PORT(uport);

	geni_write_reg(port->tx_wm, uport->membase,
					SE_GENI_TX_WATERMARK_REG);
	msm_geni_serial_setup_tx(uport, 1);
	if (!msm_geni_serial_poll_bit(uport, SE_GENI_M_IRQ_STATUS,
				M_TX_FIFO_WATERMARK_EN, true))
		WARN_ON(1);
	geni_write_reg(b, uport->membase, SE_GENI_TX_FIFOn);
	geni_write_reg(M_TX_FIFO_WATERMARK_EN, uport->membase,
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

#if IS_ENABLED(CONFIG_SERIAL_MSM_GENI_CONSOLE) || \
					IS_ENABLED(CONFIG_CONSOLE_POLL)
static void msm_geni_serial_wr_char(struct uart_port *uport, int ch)
{
	geni_write_reg(ch, uport->membase, SE_GENI_TX_FIFOn);
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
	geni_write_reg(tx_wm, uport->membase,
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
		geni_write_reg(M_TX_FIFO_WATERMARK_EN, uport->membase,
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
		geni_se_cancel_m_cmd(&port->se);

		/*
		 * console should be in polling mode. Hence directly pass true
		 * as argument for wait_for_cmd_done here to handle cancel tx
		 * in polling mode.
		 */
		timeout = geni_wait_for_cmd_done(uport, true);
		if (timeout) {
			IPC_LOG_MSG(port->console_log,
				"%s: tx_cancel failed 0x%x\n",
				__func__, geni_read_reg(uport->membase,
							SE_GENI_STATUS));
			geni_se_dump_dbg_regs(uport);
			reinit_completion(&port->m_cmd_timeout);
			geni_se_abort_m_cmd(&port->se);
			timeout = geni_wait_for_cmd_done(uport, true);
			if (timeout) {
				IPC_LOG_MSG(port->console_log,
				"%s: tx abort failed 0x%x\n", __func__,
				geni_read_reg(uport->membase, SE_GENI_STATUS));
				geni_se_dump_dbg_regs(uport);
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
		if (kfifo_len(&uport->state->port->xmit_fifo)) {
			irq_en = geni_read_reg(uport->membase,
						SE_GENI_M_IRQ_EN);
			geni_write_reg(irq_en | M_TX_FIFO_WATERMARK_EN,
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
			geni_read_reg(uport->membase, SE_GENI_RX_FIFOn);
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

static void msm_geni_uart_ev_cb(struct dma_chan *ch,
				struct msm_gpi_cb const *cb_str,
				void *ptr)
{
	struct msm_geni_serial_port *msm_port = ptr;
	u32 geni_stat = cb_str->status;

	UART_LOG_DBG(msm_port->ipc_log_misc, msm_port->uport.dev,
		     "%s: Start\n", __func__);
	switch (cb_str->cb_event) {
	case MSM_GPI_QUP_ERROR:
	case MSM_GPI_QUP_SW_ERROR:
	case MSM_GPI_QUP_MAX_EVENT:
	case MSM_GPI_QUP_CH_ERROR:
	case MSM_GPI_QUP_PENDING_EVENT:
	case MSM_GPI_QUP_EOT_DESC_MISMATCH:
		break;
	case MSM_GPI_QUP_NOTIFY:
		if (geni_stat & M_ILLEGAL_CMD_EN) {
			WARN_ON(1);
			goto exit_ev_cb;
		}
		if (geni_stat & M_CMD_OVERRUN_EN)
			goto exit_ev_cb;
		break;
	default:
		break;
	}

	if (cb_str->cb_event != MSM_GPI_QUP_NOTIFY)
		UART_LOG_DBG(msm_port->ipc_log_misc, msm_port->uport.dev,
			     "GSI QN err:0x%x, status:0x%x, err:%d\n",
			     cb_str->error_log.error_code,
			     geni_stat, cb_str->cb_event);
exit_ev_cb:
	UART_LOG_DBG(msm_port->ipc_log_misc, msm_port->uport.dev,
		     "%s: End\n", __func__);
}

static void setup_config0_tre(struct uart_port *uport,
			      unsigned int bits_per_char,
			      unsigned int clk_div, unsigned int stop_bit_len,
			      unsigned int tx_parity, bool cts_mask,
			      unsigned int rx_parity, unsigned int loopback)
{
	struct msm_geni_serial_port *msm_port = GET_DEV_PORT(uport);
	struct msm_gpi_tre *tx_cfg0 = &msm_port->gsi->tx_cfg0_t;
	struct msm_gpi_tre *rx_cfg0 = &msm_port->gsi->rx_cfg0_t;
	unsigned int char_size = bits_per_char - 5;
	unsigned int flags = (cts_mask << 2) | (loopback & 0x1);
	unsigned int rfr_lvl =  msm_port->rx_fifo_depth - 2;

	/* config0: Parity-4 for none, packing-101 */
	tx_cfg0->dword[0] = MSM_GPI_UART_CONFIG0_TRE_DWORD0(1, 0, flags,
							    4, stop_bit_len,
							    char_size);
	tx_cfg0->dword[1] = MSM_GPI_UART_CONFIG0_TRE_DWORD1(0, 0);
	tx_cfg0->dword[2] = MSM_GPI_UART_CONFIG0_TRE_DWORD2(0, clk_div);
	tx_cfg0->dword[3] = MSM_GPI_UART_CONFIG0_TRE_DWORD3(0, 0, 0, 0, 1);

	/* config0 */
	rx_cfg0->dword[0] = MSM_GPI_UART_CONFIG0_TRE_DWORD0(1, 0, flags,
							    4, stop_bit_len,
							    char_size);
	rx_cfg0->dword[1] = MSM_GPI_UART_CONFIG0_TRE_DWORD1(rfr_lvl,
							    STALE_COUNT);
	rx_cfg0->dword[2] = MSM_GPI_UART_CONFIG0_TRE_DWORD2(0, clk_div);
	rx_cfg0->dword[3] = MSM_GPI_UART_CONFIG0_TRE_DWORD3(0, 0, 0, 0, 1);

	msm_port->gsi->tx_cb.userdata = msm_port;
	msm_port->gsi->rx_cb.userdata = msm_port;
}

static void msm_geni_uart_gsi_tx_cb(void *ptr)
{
	struct msm_gpi_dma_async_tx_cb_param *tx_cb = ptr;
	struct msm_geni_serial_port *msm_port = tx_cb->userdata;
	struct uart_port *uport = &msm_port->uport;
	struct tty_port *tport = &uport->state->port;

	UART_LOG_DBG(msm_port->ipc_log_misc, msm_port->uport.dev,
		     "%s: Start\n", __func__);
	uart_xmit_advance(uport, msm_port->xmit_size);

	if (msm_port->split_dma_tre.immediate_dma_in_progress)
		msm_port->split_dma_tre.immediate_dma_in_progress = false;
	else
		geni_se_tx_dma_unprep(&msm_port->se, msm_port->tx_dma, msm_port->xmit_size);

	msm_uart_log_tx_kpi(uport);
	uport->icount.tx += msm_port->xmit_size;
	msm_port->tx_dma = (dma_addr_t)NULL;
	msm_port->xmit_size = 0;
	complete(&msm_port->tx_xfer);
	if (!kfifo_is_empty(&tport->xmit_fifo)) {
		queue_work(msm_port->tx_wq, &msm_port->tx_xfer_work);
		UART_LOG_DBG(msm_port->ipc_log_misc, msm_port->uport.dev,
			     "%s: End\n", __func__);
	} else {
		/*
		 * This will balance out the power vote put in during start_tx
		 * allowing the device to suspend.
		 */
		if (!uart_console(uport)) {
			UART_LOG_DBG(msm_port->ipc_log_misc,
				     msm_port->uport.dev,
				     "%s.Tx sent out, Power off\n", __func__);
			msm_geni_serial_power_off(uport);
		}
		uart_write_wakeup(uport);
	}
	UART_LOG_DBG(msm_port->ipc_log_misc, msm_port->uport.dev, "%s:End\n",
		     __func__);
}

static void msm_geni_uart_rx_queue_dma_tre(int index, struct uart_port *uport)
{
	struct msm_geni_serial_port *msm_port = GET_DEV_PORT(uport);
	dma_cookie_t rx_cookie;
	struct scatterlist rx_sg;

	UART_LOG_DBG(msm_port->ipc_log_misc, uport->dev,
		     "%s: Start\n", __func__);
	sg_init_table(&rx_sg, 1);
	sg_set_buf(&rx_sg, &msm_port->gsi->rx_t[index],
		   sizeof(msm_port->gsi->rx_t[index]));
	msm_port->gsi->rx_desc = dmaengine_prep_slave_sg(msm_port->gsi->rx_c,
							 &rx_sg, 1,
							 DMA_DEV_TO_MEM,
							 (DMA_PREP_INTERRUPT |
							 DMA_CTRL_ACK));
	if (!msm_port->gsi->rx_desc) {
		dev_err(uport->dev, "%s:Prep_slave_sg failed\n", __func__);
		return;
	}
	msm_port->gsi->rx_desc->callback = msm_geni_uart_gsi_rx_cb;
	msm_port->gsi->rx_desc->callback_param = &msm_port->gsi->rx_cb;

	rx_cookie = dmaengine_submit(msm_port->gsi->rx_desc);
	if (dma_submit_error(rx_cookie)) {
		pr_err("%s: dmaengine_submit failed (%d)\n", __func__, rx_cookie);
		dmaengine_terminate_all(msm_port->gsi->rx_c);
		return;
	}

	dma_async_issue_pending(msm_port->gsi->rx_c);
	UART_LOG_DBG(msm_port->ipc_log_misc, uport->dev,
		     "%s: End\n", __func__);
}

static void msm_geni_uart_gsi_rx_cb(void *ptr)
{
	struct msm_gpi_dma_async_tx_cb_param *rx_cb = ptr;
	struct msm_geni_serial_port *msm_port = rx_cb->userdata;
	struct uart_port *uport = &msm_port->uport;
	struct tty_port *tport = &uport->state->port;
	unsigned int rx_bytes = rx_cb->length;
	u8 idx = msm_port->rx_buf_idx;
	unsigned long long start_time;
	int ret;

	UART_LOG_DBG(msm_port->ipc_log_rx, uport->dev,
		     "%s: Start\n", __func__);
	start_time = geni_capture_start_time(&msm_port->se, msm_port->ipc_log_kpi,
					     __func__, msm_port->uart_kpi);

	if (!msm_port->rx_gsi_buf[idx]) {
		UART_LOG_DBG(msm_port->ipc_log_rx, uport->dev, "%s: Invalid Rx buffer\n", __func__);
		return;
	}

	ret = tty_insert_flip_string(tport, (unsigned char *)(msm_port->rx_gsi_buf[idx]), rx_bytes);
	if (ret != rx_bytes)
		UART_LOG_DBG(msm_port->ipc_log_rx, uport->dev,
			     "%s: ret %d rx_bytes %d\n", __func__,
			     ret, rx_bytes);

	uport->icount.rx += ret;
	tty_flip_buffer_push(tport);
	dump_ipc(uport, msm_port->ipc_log_rx, "GSI Rx",
		 (char *)msm_port->rx_gsi_buf[idx], 0, rx_bytes);

	msm_geni_uart_rx_queue_dma_tre(idx, uport);
	msm_port->rx_buf_idx = (idx + 1) % 4;
	geni_capture_stop_time(&msm_port->se, msm_port->ipc_log_kpi, __func__,
			       msm_port->uart_kpi, start_time, 0, 0);
	UART_LOG_DBG(msm_port->ipc_log_rx, uport->dev,
		     "%s: End\n", __func__);
}

static void msm_geni_deallocate_chan(struct uart_port *uport)
{
	struct msm_geni_serial_port *msm_port = GET_DEV_PORT(uport);

	if (msm_port->gsi->rx_c) {
		dma_release_channel(msm_port->gsi->rx_c);
		msm_port->gsi->rx_c = NULL;
	}

	if (msm_port->gsi->tx_c) {
		dma_release_channel(msm_port->gsi->tx_c);
		msm_port->gsi->tx_c = NULL;
	}
	UART_LOG_DBG(msm_port->ipc_log_misc, uport->dev, "DMA channel release done\n");
}

static int msm_geni_allocate_chan(struct uart_port *uport)
{
	struct msm_geni_serial_port *msm_port = GET_DEV_PORT(uport);
	int ret = 0;

	UART_LOG_DBG(msm_port->ipc_log_misc, uport->dev,
		     "%s: Start\n", __func__);
	if (!msm_port->gsi->rx_c) {
		msm_port->gsi->rx_c =
				dma_request_slave_channel(uport->dev, "rx");

		if (!msm_port->gsi->rx_c) {
			dev_err(uport->dev, "%s:Failed to allocate RX slv ch\n",
				__func__);
			ret = -EIO;
			goto out;
		}
		msm_port->gsi->rx_ev.init.callback = msm_geni_uart_ev_cb;
		msm_port->gsi->rx_ev.init.cb_param = msm_port;
		msm_port->gsi->rx_ev.cmd = MSM_GPI_INIT;
		msm_port->gsi->rx_c->private = &msm_port->gsi->rx_ev;
		ret = dmaengine_slave_config(msm_port->gsi->rx_c, NULL);
		if (ret) {
			dev_err(uport->dev, "Failed to Config Rx\n");
			dma_release_channel(msm_port->gsi->rx_c);
			goto out;
		}
	}

	if (!msm_port->gsi->tx_c) {
		msm_port->gsi->tx_c =
				dma_request_slave_channel(uport->dev, "tx");

		if (!msm_port->gsi->tx_c) {
			dev_err(uport->dev, "%s:Failed to allocate TX slv ch\n",
				__func__);
			dma_release_channel(msm_port->gsi->rx_c);
			msm_port->gsi->rx_c = NULL;
			ret = -EIO;
			goto out;
		}
		msm_port->gsi->tx_ev.init.callback = msm_geni_uart_ev_cb;
		msm_port->gsi->tx_ev.init.cb_param = msm_port;
		msm_port->gsi->tx_ev.cmd = MSM_GPI_INIT;
		msm_port->gsi->tx_c->private = &msm_port->gsi->tx_ev;
		ret = dmaengine_slave_config(msm_port->gsi->tx_c, NULL);
		if (ret) {
			dev_err(uport->dev, "Failed to Config Tx\n");
			msm_geni_deallocate_chan(uport);
			goto out;
		}
	}
out:
	UART_LOG_DBG(msm_port->ipc_log_misc, uport->dev,
		     "%s: End\n", __func__);
	return ret;
}

/**
 * msm_geni_serial_cleanup_timestamp_resources() - deallocates the dma buffers associated
 * with timstamp buffers
 * @port: pointer to msm_port.
 *
 * Return: none
 */
static void msm_geni_serial_cleanup_timestamp_resources(struct msm_geni_serial_port *port)
{
	if (port->time_stamp) {
		if (port->ts_dma[0]) {
			dma_unmap_resource(port->wrapper_dev, port->ts_dma[0],
					   TIMESTAMP_DATA_SIZE, DMA_TO_DEVICE, 0);
			port->ts_dma[0] = 0;
		}
		if (port->ts_dma[1]) {
			dma_unmap_resource(port->wrapper_dev, port->ts_dma[1],
					   TIMESTAMP_DATA_SIZE, DMA_TO_DEVICE, 0);
			port->ts_dma[1] = 0;
		}
		port->time_stamp = false;
	}
}

/**
 * msm_geni_serial_free_aligned_dma_buffers() - deallocates the dma buffers
 * @msm_port: pointer to msm_port.
 *
 * Return: none
 */
static void msm_geni_serial_free_aligned_dma_buffers(struct msm_geni_serial_port *msm_port)
{
	struct device *tx_dev = msm_port->wrapper_dev;

	if (msm_port->split_dma_tre.aligned_tx_buf_orig) {
		geni_se_common_iommu_free_buf(tx_dev,
					      &msm_port->split_dma_tre.aligned_tx_dma_buf_orig,
					      msm_port->split_dma_tre.aligned_tx_buf_orig,
					      SERIAL_PREALLOC_BUF_SIZE);
		msm_port->split_dma_tre.aligned_tx_buf = NULL;
		msm_port->split_dma_tre.aligned_tx_buf_orig = NULL;
	}
}

/**
 * msm_geni_serial_obtain_aligned_dma_buffers() - Obtains aligned dma buffers
 * @msm_port: pointer to msm_port.
 *
 * Return: none
 */
static void msm_geni_serial_obtain_aligned_dma_buffers(struct msm_geni_serial_port *msm_port)
{
	if (msm_port->split_dma_tre.aligned_tx_buf_orig)
		return;

	msm_port->split_dma_tre.aligned_tx_buf_orig = msm_port->split_dma_tre.aligned_tx_buf;
	msm_port->split_dma_tre.aligned_tx_dma_buf_orig =
				msm_port->split_dma_tre.aligned_tx_dma_buf;

	/* Ensure DMA buffer address is aligned to SERIAL_DMA_ADDR_ALIGN_BYTE bytes */
	if (!IS_ALIGNED(msm_port->split_dma_tre.aligned_tx_dma_buf, SERIAL_DMA_ADDR_ALIGN_BYTE)) {
		msm_port->split_dma_tre.aligned_tx_dma_buf =
			(dma_addr_t)ALIGN((uintptr_t)msm_port->split_dma_tre.aligned_tx_dma_buf,
			SERIAL_DMA_ADDR_ALIGN_BYTE);
		msm_port->split_dma_tre.aligned_tx_buf =
			(void *)ALIGN((uintptr_t)msm_port->split_dma_tre.aligned_tx_buf,
			SERIAL_DMA_ADDR_ALIGN_BYTE);
	}

	UART_LOG_DBG(msm_port->ipc_log_misc, msm_port->uport.dev,
		     "SERIAL aligned tx_buf addr:0x%p, dma addr:0x%p\n",
		     msm_port->split_dma_tre.aligned_tx_buf,
		     (void *)msm_port->split_dma_tre.aligned_tx_dma_buf);
}

/**
 * msm_geni_serial_align_tx_buf() - align the buffer to 16 bytes.
 * msm_port: pointer to msm_port.
 * buf: buffer.
 * xmit_size: transmit length.
 *
 * Return: 0 on success and error on failure.
 */
static int msm_geni_serial_align_tx_buf(struct msm_geni_serial_port *msm_port,
					void *buf, unsigned int xmit_size)
{
	if (xmit_size > SERIAL_TX_DMA_BUF_SIZE_MAX) {
		UART_LOG_DBG(msm_port->ipc_log_misc, msm_port->uport.dev,
			     "Warning: xmit size greater than max buffer size, using size %lu\n",
			     SERIAL_TX_DMA_BUF_SIZE_MAX);
		xmit_size = SERIAL_TX_DMA_BUF_SIZE_MAX;
	}

	msm_geni_serial_obtain_aligned_dma_buffers(msm_port);
	if (!msm_port->split_dma_tre.aligned_tx_buf_orig)
		return -ENOMEM;

	memcpy(msm_port->split_dma_tre.aligned_tx_buf, buf, xmit_size);

	return 0;
}

/**
 * msm_geni_serial_gsi_xfer_split_tx() - splits gsi tx transfer into multiple transfer
 * @msm_port: pointer to the msm_geni_serial_port.
 * @buf: data buffer to be transferred.
 * @xmit_size: data length.
 *
 * Return: 0 on success, or a negative error code upon failure.
 */
static int msm_geni_serial_gsi_xfer_split_tx(struct msm_geni_serial_port *msm_port,
					    void *buf, unsigned int xmit_size)
{
	int ret = 0;

	if (xmit_size >= SPLIT_DMA_TX_SIZE) {
		/* Align transmit size to 16 byte boundary */
		xmit_size = xmit_size & ~(SPLIT_DMA_TX_SIZE - 1);
		msm_port->xmit_size = xmit_size;
		msm_port->split_dma_tre.immediate_dma_in_progress = false;
		ret = geni_se_common_iommu_map_buf(msm_port->wrapper_dev, &msm_port->tx_dma,
						   buf, xmit_size, DMA_TO_DEVICE);
		if (ret) {
			UART_LOG_DBG(msm_port->ipc_log_misc, msm_port->uport.dev,
				     "Error %d Failed to map tx buffer\n", ret);
			msm_geni_deallocate_chan(&msm_port->uport);
			return ret;
		}

		if (!IS_ALIGNED(msm_port->tx_dma, SERIAL_DMA_ADDR_ALIGN_BYTE)) {
			ret = geni_se_common_iommu_unmap_buf(msm_port->wrapper_dev,
							     &msm_port->tx_dma, xmit_size,
							     DMA_TO_DEVICE);
			if (ret) {
				UART_LOG_DBG(msm_port->ipc_log_misc, msm_port->uport.dev,
				     "Error %d Failed to unmap tx buffer\n", ret);
				return ret;
			}

			ret = msm_geni_serial_align_tx_buf(msm_port, buf, xmit_size);
			if (ret) {
				UART_LOG_DBG(msm_port->ipc_log_misc, msm_port->uport.dev,
					     "Error Failed to obtain aligned tx buffers\n");
				return ret;
			}
			msm_port->gsi->tx_t[0].dword[0] =
				MSM_GPI_DMA_W_BUFFER_TRE_DWORD0(
					msm_port->split_dma_tre.aligned_tx_dma_buf
				);
			msm_port->gsi->tx_t[0].dword[1] =
				MSM_GPI_DMA_W_BUFFER_TRE_DWORD1(
					msm_port->split_dma_tre.aligned_tx_dma_buf
				);
			msm_port->gsi->tx_t[0].dword[2] =
				MSM_GPI_DMA_W_BUFFER_TRE_DWORD2(xmit_size);
			msm_port->gsi->tx_t[0].dword[3] =
				MSM_GPI_DMA_W_BUFFER_TRE_DWORD3(0, 0, 1, 0, 0);
		} else {
			msm_port->gsi->tx_t[0].dword[0] =
				MSM_GPI_DMA_W_BUFFER_TRE_DWORD0(msm_port->tx_dma);
			msm_port->gsi->tx_t[0].dword[1] =
				MSM_GPI_DMA_W_BUFFER_TRE_DWORD1(msm_port->tx_dma);
			msm_port->gsi->tx_t[0].dword[2] =
				MSM_GPI_DMA_W_BUFFER_TRE_DWORD2(xmit_size);
			msm_port->gsi->tx_t[0].dword[3] =
				MSM_GPI_DMA_W_BUFFER_TRE_DWORD3(0, 0, 1, 0, 0);
		}
	}  else  {
		if (xmit_size >= IMMEDIATE_DMA_LEN)
			msm_port->xmit_size = IMMEDIATE_DMA_LEN;
		else
			msm_port->xmit_size = xmit_size;

		msm_port->split_dma_tre.immediate_dma_in_progress = true;
		msm_port->gsi->tx_t[0].dword[0] = 0;
		msm_port->gsi->tx_t[0].dword[1] = 0;
		memcpy((u8 *)&msm_port->gsi->tx_t[0].dword[0], (u8 *)buf, msm_port->xmit_size);
		msm_port->gsi->tx_t[0].dword[2] =
			MSM_GPI_DMA_IMMEDIATE_TRE_DWORD2(msm_port->xmit_size);
		msm_port->gsi->tx_t[0].dword[3] = MSM_GPI_DMA_IMMEDIATE_TRE_DWORD3(0, 0, 1, 0, 0);
	}

	return ret;
}

static bool setup_timestamp_tre_pair(struct msm_geni_serial_port *msm_port,
				    struct uart_port *uport, int *index)
{
	int ret;

	/* First timestamp TRE */
	ret = setup_timestamp_dma_tre(msm_port, &msm_port->gsi->tx_t[TX_TRE_INSTANCES_1], 0, 1,
				      &msm_port->ts_dma[TS_DMA_INSTANCES_0]);
	if (ret) {
		dev_err(uport->dev, "Failed to set up first timestamp TRE ret: %d\n", ret);
		msm_geni_deallocate_chan(uport);
		return false;
	}
	sg_set_buf(&msm_port->gsi->tx_sg[(*index)++], &msm_port->gsi->tx_t[TX_TRE_INSTANCES_1],
		   sizeof(msm_port->gsi->tx_t[TX_TRE_INSTANCES_1]));

	/* Second timestamp TRE */
	ret = setup_timestamp_dma_tre(msm_port, &msm_port->gsi->tx_t[TX_TRE_INSTANCES_2], 1, 0,
				      &msm_port->ts_dma[TS_DMA_INSTANCES_1]);
	if (ret) {
		dev_err(uport->dev, "Failed to set up second timestamp TRE ret: %d\n", ret);
		dma_unmap_resource(msm_port->wrapper_dev, msm_port->ts_dma[TS_DMA_INSTANCES_0],
				   TIMESTAMP_DATA_SIZE, DMA_TO_DEVICE, 0);
		msm_port->ts_dma[TS_DMA_INSTANCES_0] = 0;
		msm_geni_deallocate_chan(uport);
		return false;
	}
	sg_set_buf(&msm_port->gsi->tx_sg[(*index)++], &msm_port->gsi->tx_t[TX_TRE_INSTANCES_2],
		   sizeof(msm_port->gsi->tx_t[TX_TRE_INSTANCES_2]));

	return true;
}

/**
 * msm_geni_uart_gsi_xfer_tx() - Setup GSI TX transfer with optional timestamp
 * @work: Pointer to work structure
 *
 * This function sets up a GSI TX transfer. When timestamp functionality is enabled,
 * it configures additional DMA TREs to capture timestamps at the beginning and end
 * of the transfer, which helps with performance analysis and debugging.
 * The timestamp data is collected from the QTIMER hardware block.
 *
 * Return: none
 */
static void msm_geni_uart_gsi_xfer_tx(struct work_struct *work)
{
	struct msm_geni_serial_port *msm_port = container_of(work,
						struct msm_geni_serial_port,
						tx_xfer_work);
	struct uart_port *uport = &msm_port->uport;
	struct tty_port *tport = &uport->state->port;
	dma_cookie_t tx_cookie;
	struct msm_gpi_tre *go_t = &msm_port->gsi->tx_go_t;
	struct device *tx_dev = msm_port->wrapper_dev;
	unsigned int xmit_size = 0;
	int ret = 0, index = 0, timeout;
	unsigned char *tail_ptr = NULL;
	bool skip_time_stamp = false;
	unsigned long long start_time;

	start_time = geni_capture_start_time(&msm_port->se, msm_port->ipc_log_kpi,
					     __func__, msm_port->uart_kpi);

	UART_LOG_DBG(msm_port->ipc_log_misc, uport->dev,
		     "%s: Start\n", __func__);

	xmit_size = kfifo_out_linear_ptr(&tport->xmit_fifo, &tail_ptr, UART_XMIT_SIZE);

	if (xmit_size < WAKEUP_CHARS)
		uart_write_wakeup(uport);

	if (!xmit_size || msm_port->tx_dma)
		return;

	/* Detect wrap-around and skip timestamp if partial data is being sent */
	if (msm_port->time_stamp && xmit_size < kfifo_len(&tport->xmit_fifo)) {
		UART_LOG_DBG(msm_port->ipc_log_misc, uport->dev,
			     "Wrap-around occurred. skipping timestamp.\n");
		skip_time_stamp = true;
	}

	dump_ipc(uport, msm_port->ipc_log_tx, "DMA Tx", tail_ptr, 0, xmit_size);

	ret = msm_geni_allocate_chan(uport);
	if (ret) {
		dev_err(uport->dev, "%s: Allocation of Channel failed:%d\n",
			__func__, ret);
		return;
	}

	sg_init_table(msm_port->gsi->tx_sg, (msm_port->time_stamp && !skip_time_stamp) ? 5 : 3);
	sg_set_buf(msm_port->gsi->tx_sg, &msm_port->gsi->tx_cfg0_t,
		   sizeof(msm_port->gsi->tx_cfg0_t));
	index++;
	go_t->dword[0] = MSM_GPI_UART_GO_TRE_DWORD0(0, 1);
	go_t->dword[1] = MSM_GPI_UART_GO_TRE_DWORD1;
	go_t->dword[2] = MSM_GPI_UART_GO_TRE_DWORD2;
	go_t->dword[3] = MSM_GPI_UART_GO_TRE_DWORD3(0, 0, 0, 0, 1);

	sg_set_buf(&msm_port->gsi->tx_sg[index++], go_t,
		   sizeof(*go_t));
	if (msm_port->split_dma_tre.split_tx) {
		ret = msm_geni_serial_gsi_xfer_split_tx(msm_port, tail_ptr, xmit_size);
		if (ret) {
			UART_LOG_DBG(msm_port->ipc_log_misc, uport->dev,
				     "Error %d Split gsi transfer failed\n", ret);
			goto exit_gsi_tx_xfer;
		}
	} else {
		ret = geni_se_common_iommu_map_buf(tx_dev, &msm_port->tx_dma, tail_ptr, xmit_size,
						   DMA_TO_DEVICE);
		if (!ret) {
			msm_port->xmit_size = xmit_size;
		} else {
			dev_err(uport->dev, "%s:Failed to allocate memory\n",
				__func__);
			msm_geni_deallocate_chan(uport);
			return;
		}
		msm_port->gsi->tx_t[0].dword[0] =
			MSM_GPI_DMA_W_BUFFER_TRE_DWORD0(msm_port->tx_dma);
		msm_port->gsi->tx_t[0].dword[1] =
			MSM_GPI_DMA_W_BUFFER_TRE_DWORD1(msm_port->tx_dma);
		msm_port->gsi->tx_t[0].dword[2] =
			MSM_GPI_DMA_W_BUFFER_TRE_DWORD2(xmit_size);
		msm_port->gsi->tx_t[0].dword[3] =
			MSM_GPI_DMA_W_BUFFER_TRE_DWORD3(0, 0,
							(!msm_port->time_stamp || skip_time_stamp),
							0,
							(msm_port->time_stamp && !skip_time_stamp));
	}

	sg_set_buf(&msm_port->gsi->tx_sg[index++], &msm_port->gsi->tx_t[0],
		   sizeof(msm_port->gsi->tx_t[0]));

	if (msm_port->time_stamp && !skip_time_stamp
	    && !setup_timestamp_tre_pair(msm_port, uport, &index))
		return;

	if (msm_port->uart_kpi) {
		msm_port->uart_kpi_tx[msm_port->kpi_idx].xfer_req_sw.len = msm_port->xmit_size;
		msm_port->uart_kpi_tx[msm_port->kpi_idx].xfer_req_sw.time_stamp = sched_clock();
	}

	msm_port->gsi->tx_desc = dmaengine_prep_slave_sg(msm_port->gsi->tx_c,
							 msm_port->gsi->tx_sg,
							 index, DMA_MEM_TO_DEV,
							 (DMA_PREP_INTERRUPT |
							 DMA_CTRL_ACK));
	if (!msm_port->gsi->tx_desc) {
		dev_err(uport->dev, "%s:TX descriptor prep failed\n",
			__func__);
		goto exit_gsi_tx_xfer;
	}

	msm_port->gsi->tx_desc->callback = msm_geni_uart_gsi_tx_cb;
	msm_port->gsi->tx_desc->callback_param = &msm_port->gsi->tx_cb;
	tx_cookie = dmaengine_submit(msm_port->gsi->tx_desc);
	if (dma_submit_error(tx_cookie)) {
		pr_err("%s: dmaengine_submit failed (%d)\n", __func__, tx_cookie);
		dmaengine_terminate_all(msm_port->gsi->tx_c);
		return;
	}
	reinit_completion(&msm_port->tx_xfer);
	dma_async_issue_pending(msm_port->gsi->tx_c);

	timeout = wait_for_completion_timeout(&msm_port->tx_xfer,
					      msecs_to_jiffies
					      (POLL_WAIT_TIMEOUT_MSEC));
	UART_LOG_DBG(msm_port->ipc_log_misc, uport->dev,
		     "%s: End\n", __func__);
	geni_capture_stop_time(&msm_port->se, msm_port->ipc_log_kpi, __func__,
			       msm_port->uart_kpi, start_time,
			       msm_port->xmit_size, msm_port->cur_baud);

	return;
exit_gsi_tx_xfer:
	if (!msm_port->split_dma_tre.immediate_dma_in_progress)
		geni_se_common_iommu_unmap_buf(tx_dev, &msm_port->tx_dma,
					       msm_port->xmit_size, DMA_TO_DEVICE);
	msm_geni_deallocate_chan(uport);
	UART_LOG_DBG(msm_port->ipc_log_misc, uport->dev,
		     "%s: Failed to prep Tx descriptor", __func__);
	geni_capture_stop_time(&msm_port->se, msm_port->ipc_log_kpi, __func__,
			       msm_port->uart_kpi, start_time, 0, 0);
}

static void msm_geni_uart_gsi_cancel_tx(struct work_struct *work)
{
	struct msm_geni_serial_port *msm_port = container_of(work,
			struct msm_geni_serial_port,
			tx_cancel_work);
	struct uart_port *uport = &msm_port->uport;
	struct tty_struct *tty = uport->state->port.tty;
	int ret;

	if (!tty && pm_runtime_enabled(uport->dev)) {
		ret = pm_runtime_get_sync(uport->dev);
		if (ret < 0) {
			UART_LOG_DBG(msm_port->ipc_log_pwr, uport->dev,
				     "Failed to resume ret=%d\n", ret);
			pm_runtime_put_noidle(uport->dev);
			pm_runtime_set_suspended(uport->dev);
			return;
		}
	}

	ret = dmaengine_terminate_all(msm_port->gsi->tx_c);
	if (ret)
		UART_LOG_DBG(msm_port->ipc_log_pwr, uport->dev,
			     "gpi terminate failed ret:%d\n", ret);

	if (!tty && pm_runtime_enabled(uport->dev)) {
		ret = pm_runtime_put_sync_suspend(uport->dev);
		if (ret < 0)
			UART_LOG_DBG(msm_port->ipc_log_pwr, uport->dev,
				     "Failed to suspend ret=%d\n", ret);
	}
}

static void msm_geni_uart_gsi_cancel_rx(struct work_struct *work)
{
	struct msm_geni_serial_port *msm_port = container_of(work,
						struct msm_geni_serial_port,
						rx_cancel_work);

	UART_LOG_DBG(msm_port->ipc_log_misc, msm_port->uport.dev,
		     "%s: Start\n", __func__);
	if (!atomic_read(&msm_port->gsi_rx_done)) {
		UART_LOG_DBG(msm_port->ipc_log_misc, msm_port->uport.dev,
			     "%s: gsi_rx not yet done\n", __func__);
		return;
	}
	if (msm_port->gsi->rx_c)
		dmaengine_terminate_all(msm_port->gsi->rx_c);
	complete(&msm_port->xfer);
	atomic_set(&msm_port->gsi_rx_done, 0);
	atomic_set(&msm_port->stop_rx_inprogress, 0);
	UART_LOG_DBG(msm_port->ipc_log_misc, msm_port->uport.dev,
		     "%s: End\n", __func__);
}

static int msm_geni_uart_gsi_xfer_rx(struct uart_port *uport)
{
	struct msm_geni_serial_port *msm_port = GET_DEV_PORT(uport);
	dma_cookie_t rx_cookie;
	struct msm_gpi_tre *go_t = &msm_port->gsi->rx_go_t;
	struct device *rx_dev = msm_port->wrapper_dev;
	int i, k, index = 0;
	unsigned long long start_time;

	start_time = geni_capture_start_time(&msm_port->se, msm_port->ipc_log_kpi,
					     __func__, msm_port->uart_kpi);
	UART_LOG_DBG(msm_port->ipc_log_misc, msm_port->uport.dev, "Start: GSI Rx xfer\n");
	if (!msm_port->port_setup) {
		dev_err(uport->dev, "%s: Port setup not yet done\n", __func__);
		return -EAGAIN;
	}

	if (msm_geni_allocate_chan(uport)) {
		dev_err(uport->dev, "%s: Allocation of Channel failed\n", __func__);
		return -ENOMEM;
	}
	sg_init_table(msm_port->gsi->rx_sg, 6);
	sg_set_buf(msm_port->gsi->rx_sg, &msm_port->gsi->rx_cfg0_t,
		   sizeof(msm_port->gsi->rx_cfg0_t));
	index++;

	go_t->dword[0] = MSM_GPI_UART_GO_TRE_DWORD0(0, 1);
	go_t->dword[1] = MSM_GPI_UART_GO_TRE_DWORD1;
	go_t->dword[2] = MSM_GPI_UART_GO_TRE_DWORD2;
	go_t->dword[3] = MSM_GPI_UART_GO_TRE_DWORD3(0, 0, 0, 0, 1);

	sg_set_buf(&msm_port->gsi->rx_sg[index++], go_t,
		   sizeof(msm_port->gsi->rx_go_t));

	for (i = 0; i < NUM_RX_BUF; i++) {
		msm_port->rx_gsi_buf[i] =
				geni_se_common_iommu_alloc_buf(rx_dev,
							       &msm_port->dma_addr[i],
							       DMA_RX_BUF_SIZE);
		if (IS_ERR_OR_NULL(msm_port->rx_gsi_buf[i])) {
			for (k = i; k > 0; k--) {
				geni_se_common_iommu_free_buf(rx_dev, &msm_port->dma_addr[k - 1],
						msm_port->rx_gsi_buf[k - 1], DMA_RX_BUF_SIZE);
				msm_port->rx_gsi_buf[k - 1] = NULL;
			}
			msm_geni_deallocate_chan(uport);
			return -EIO;
		}

		msm_port->gsi->rx_t[i].dword[0] =
			MSM_GPI_DMA_W_BUFFER_TRE_DWORD0(msm_port->dma_addr[i]);
		msm_port->gsi->rx_t[i].dword[1] =
			MSM_GPI_DMA_W_BUFFER_TRE_DWORD1(msm_port->dma_addr[i]);
		msm_port->gsi->rx_t[i].dword[2] =
			MSM_GPI_DMA_W_BUFFER_TRE_DWORD2(DMA_RX_BUF_SIZE);
		msm_port->gsi->rx_t[i].dword[3] =
			MSM_GPI_DMA_W_BUFFER_TRE_DWORD3(0, 0, 1, 1, 1);
		sg_set_buf(&msm_port->gsi->rx_sg[index++],
			   &msm_port->gsi->rx_t[i],
			   sizeof(msm_port->gsi->rx_t[i]));
	}

	/* Reset buffer index for new RX session */
	msm_port->rx_buf_idx = 0;
	msm_port->gsi->rx_desc = dmaengine_prep_slave_sg(msm_port->gsi->rx_c,
							 msm_port->gsi->rx_sg,
							 6, DMA_DEV_TO_MEM,
							 (DMA_PREP_INTERRUPT |
							 DMA_CTRL_ACK));
	if (!msm_port->gsi->rx_desc) {
		dev_err(uport->dev, "%s: Rx desc is failed\n", __func__);
		goto exit_gsi_xfer_rx;
	}

	msm_port->gsi->rx_desc->callback = msm_geni_uart_gsi_rx_cb;
	msm_port->gsi->rx_desc->callback_param = &msm_port->gsi->rx_cb;
	rx_cookie = dmaengine_submit(msm_port->gsi->rx_desc);
	if (dma_submit_error(rx_cookie)) {
		pr_err("%s: dmaengine_submit failed (%d)\n", __func__, rx_cookie);
		dmaengine_terminate_all(msm_port->gsi->rx_c);
		return -EINVAL;
	}
	dma_async_issue_pending(msm_port->gsi->rx_c);
	atomic_set(&msm_port->gsi_rx_done, 1);
	UART_LOG_DBG(msm_port->ipc_log_misc, msm_port->uport.dev, "End: GSI Rx xfer\n");
	geni_capture_stop_time(&msm_port->se, msm_port->ipc_log_kpi, __func__,
			       msm_port->uart_kpi, start_time, 0, 0);
	return 0;
exit_gsi_xfer_rx:
	for (i = 0; i < NUM_RX_BUF; i++) {
		geni_se_common_iommu_free_buf(rx_dev, &msm_port->dma_addr[i],
					      msm_port->rx_gsi_buf[i], DMA_RX_BUF_SIZE);
		msm_port->rx_gsi_buf[i] = NULL;
	}
	msm_geni_deallocate_chan(uport);
	atomic_set(&msm_port->gsi_rx_done, 0);
	UART_LOG_DBG(msm_port->ipc_log_misc, msm_port->uport.dev, "GSI Rx xfer failed\n");
	return -EIO;
}

/**
 * msm_geni_serial_tx_immediate_dma() - Initiate TX DMA immediate transfer on the serial engine
 * @se: Pointer to the concerned serial engine.
 * @len: Length of the TX buffer.
 *
 * This function is used to initiate immediate DMA TX transfer.
 */
void msm_geni_serial_tx_immediate_dma(struct geni_se *se, void *buf, size_t len)
{
	u32 val;
	u8 *ptr = (u8 *)buf;
	u64 data = 0;

	for (int i = 0; i < len; i++)
		data |= ((u64)ptr[i]) << (i * 8);

	val = (GENI_SE_DMA_DONE_EN | GENI_SE_DMA_EOT_EN);
	writel_relaxed(val, se->base  + SE_DMA_TX_IRQ_EN_SET);
	writel_relaxed(data & 0xffffffff, se->base + SE_DMA_TX_PTR_L);
	writel_relaxed(data >> 32, se->base + SE_DMA_TX_PTR_H);
	writel_relaxed(GENI_SE_DMA_IMMEDIATE_MODE | GENI_SE_DMA_EOT_BUF, se->base + SE_DMA_TX_ATTR);
	writel(len, se->base + SE_DMA_TX_LEN);
}

/**
 * msm_geni_handle_split_tx_transfer() - splits tx transfer into multiple transfer
 * @uport: pointer to the uart port.
 * @buf: data buffer to be transferred.
 * @xmit_size: data length.
 *
 * Return: 0 on success, or a negative error code upon failure.
 */
static int msm_geni_handle_split_tx_transfer(struct uart_port *uport, void *buf, size_t xmit_size)
{
	struct msm_geni_serial_port *msm_port = GET_DEV_PORT(uport);
	int ret = 0;

	if (xmit_size >= SPLIT_DMA_TX_SIZE) {
		/* Align transmit size to 16 byte boundary */
		xmit_size = xmit_size & ~(SPLIT_DMA_TX_SIZE - 1);
		ret = geni_se_common_iommu_map_buf(msm_port->wrapper_dev, &msm_port->tx_dma,
						   buf, xmit_size, DMA_TO_DEVICE);
		if (ret) {
			UART_LOG_DBG(msm_port->ipc_log_misc, msm_port->uport.dev,
				     "Error %d Failed to map tx buffer\n", ret);
			return ret;
		}

		if (!IS_ALIGNED(msm_port->tx_dma, SERIAL_DMA_ADDR_ALIGN_BYTE)) {
			ret = geni_se_common_iommu_unmap_buf(msm_port->wrapper_dev,
							     &msm_port->tx_dma, xmit_size,
							     DMA_TO_DEVICE);
			if (ret) {
				UART_LOG_DBG(msm_port->ipc_log_misc, msm_port->uport.dev,
				     "Error %d Failed to unmap tx buffer\n", ret);
				return ret;
			}

			ret = msm_geni_serial_align_tx_buf(msm_port, buf, xmit_size);
			if (ret) {
				UART_LOG_DBG(msm_port->ipc_log_misc, msm_port->uport.dev,
					     "Error Failed to obtain aligned tx buffers\n");
				return ret;
			}

			msm_geni_serial_setup_tx(uport, xmit_size);
			geni_se_tx_init_dma(&msm_port->se,
					    msm_port->split_dma_tre.aligned_tx_dma_buf,
					    xmit_size);

		} else {
			msm_geni_serial_setup_tx(uport, xmit_size);
			ret = geni_se_tx_dma_prep(&msm_port->se, buf, xmit_size, &msm_port->tx_dma);
		}

		if (!ret)
			msm_port->xmit_size = xmit_size;
	} else {
		if (xmit_size >= IMMEDIATE_DMA_LEN)
			msm_port->xmit_size = IMMEDIATE_DMA_LEN;
		else
			msm_port->xmit_size = xmit_size;

		msm_port->split_dma_tre.immediate_dma_in_progress = true;
		msm_geni_serial_setup_tx(uport, msm_port->xmit_size);
		msm_geni_serial_tx_immediate_dma(&msm_port->se, buf, msm_port->xmit_size);
	}

	return ret;
}
static int msm_geni_serial_prep_dma_tx(struct uart_port *uport)
{
	struct msm_geni_serial_port *msm_port = GET_DEV_PORT(uport);
	struct tty_port *tport = &uport->state->port;
	unsigned int xmit_size;
	unsigned int dma_dbg;
	bool timeout, is_irq_masked;
	int ret = 0;
	unsigned long long start_time;
	unsigned char *tail_ptr = NULL;

	start_time = geni_capture_start_time(&msm_port->se, msm_port->ipc_log_kpi,
					     __func__, msm_port->uart_kpi);
	if (atomic_read(&msm_port->flush_buffers))
		return -EIO;

	xmit_size = kfifo_out_linear_ptr(&tport->xmit_fifo, &tail_ptr, UART_XMIT_SIZE);

	if (xmit_size < WAKEUP_CHARS)
		uart_write_wakeup(uport);

	if (!xmit_size)
		return -EPERM;

	dump_ipc(uport, msm_port->ipc_log_tx, "DMA Tx", tail_ptr, 0, xmit_size);

	UART_LOG_DBG(msm_port->ipc_log_misc, uport->dev,
		     "%s: cts_count:%d\n", __func__, uport->icount.cts);

	if (msm_port->uart_kpi) {
		msm_port->uart_kpi_tx[msm_port->kpi_idx].xfer_req_hw.len = msm_port->xmit_size;
		msm_port->uart_kpi_tx[msm_port->kpi_idx].xfer_req_hw.time_stamp = sched_clock();
		msm_port->kpi_idx++;
		if (msm_port->kpi_idx >= UART_KPI_TX_RX_INSTANCES)
			msm_port->kpi_idx = 0;
	}

	if (msm_port->split_dma_tre.split_tx) {
		ret = msm_geni_handle_split_tx_transfer(uport, tail_ptr, xmit_size);
	} else {
		msm_geni_serial_setup_tx(uport, xmit_size);
		ret = geni_se_tx_dma_prep(&msm_port->se, tail_ptr, xmit_size, &msm_port->tx_dma);
		if (!ret)
			msm_port->xmit_size = xmit_size;
	}

	if (ret) {
		UART_LOG_DBG(msm_port->ipc_log_misc, uport->dev,
		    "%s: TX DMA map Fail %d\n", __func__, ret);

		msm_geni_update_uart_error_code(msm_port, UART_ERROR_TX_DMA_MAP_FAIL);

		geni_write_reg(0, uport->membase, SE_UART_TX_TRANS_LEN);
		msm_port->m_cmd_done = false;
		msm_port->m_cmd = true;
		reinit_completion(&msm_port->m_cmd_timeout);

		/*
		 * Try disabling interrupts before giving the
		 * cancel command as this might be in an atomic context.
		 */
		is_irq_masked = msm_serial_try_disable_interrupts(uport);
		geni_se_cancel_m_cmd(&msm_port->se);

		timeout = geni_wait_for_cmd_done(uport, is_irq_masked);
		if (timeout) {
			IPC_LOG_MSG(msm_port->console_log,
			"%s: tx_cancel fail 0x%x\n", __func__,
			geni_read_reg(uport->membase, SE_GENI_STATUS));

			UART_LOG_DBG(msm_port->ipc_log_misc, uport->dev,
			"%s: tx_cancel failed 0x%x\n", __func__,
			geni_read_reg(uport->membase, SE_GENI_STATUS));
			msm_geni_update_uart_error_code(msm_port, UART_ERROR_TX_CANCEL_FAIL);
			geni_se_dump_dbg_regs(uport);

			msm_port->m_cmd_done = false;
			reinit_completion(&msm_port->m_cmd_timeout);
			/* Give abort command as cancel command failed */
			geni_se_abort_m_cmd(&msm_port->se);

			timeout = geni_wait_for_cmd_done(uport,
							 is_irq_masked);
			if (timeout) {
				IPC_LOG_MSG(msm_port->console_log,
				"%s: tx abort failed 0x%x\n", __func__,
				geni_read_reg(uport->membase,
							SE_GENI_STATUS));
				UART_LOG_DBG(msm_port->ipc_log_misc, uport->dev,
				"%s: tx abort failed 0x%x\n", __func__,
				geni_read_reg(uport->membase,
							SE_GENI_STATUS));
				msm_geni_update_uart_error_code(msm_port, UART_ERROR_TX_ABORT_FAIL);
				geni_se_dump_dbg_regs(uport);
			} else {
				/* Reset the CANCEL error code if abort is success */
				msm_geni_update_uart_error_code(msm_port, UART_ERROR_DEFAULT);
			}
			msm_geni_serial_allow_rx(msm_port);
			geni_write_reg(FORCE_DEFAULT, uport->membase,
					GENI_FORCE_DEFAULT_REG);
		}

		if (msm_port->xfer_mode == GENI_SE_DMA) {
			dma_dbg = geni_read_reg(uport->membase,
							SE_DMA_DEBUG_REG0);
			if (dma_dbg & DMA_TX_ACTIVE) {
				msm_port->m_cmd_done = false;
				reinit_completion(&msm_port->m_cmd_timeout);
				geni_write_reg(1, uport->membase,
						SE_DMA_TX_FSM_RST);

				timeout = geni_wait_for_cmd_done(uport,
							is_irq_masked);
				if (timeout) {
					UART_LOG_DBG(msm_port->ipc_log_misc, uport->dev,
					"%s: tx fsm reset failed\n", __func__);
					msm_geni_update_uart_error_code(msm_port,
						UART_ERROR_TX_FSM_RESET_FAIL);
					geni_se_dump_dbg_regs(uport);
				} else {
					/* Reset Cancel/Abort error code if FSM reset is success */
					msm_geni_update_uart_error_code(msm_port,
									UART_ERROR_DEFAULT);
				}
			}

			if (msm_port->tx_dma) {
				geni_se_tx_dma_unprep(&msm_port->se,
					msm_port->tx_dma, msm_port->xmit_size);
				msm_port->tx_dma = (dma_addr_t)NULL;
			}
		}
		msm_port->xmit_size = 0;
		/* Enable the interrupts once the cancel operation is done. */
		msm_geni_serial_enable_interrupts(uport);
		msm_port->m_cmd = false;
	}

	geni_capture_stop_time(&msm_port->se, msm_port->ipc_log_kpi, __func__,
			       msm_port->uart_kpi, start_time,
			       msm_port->xmit_size, msm_port->cur_baud);
	return ret;
}

static void msm_geni_serial_start_tx(struct uart_port *uport)
{
	unsigned int geni_m_irq_en;
	struct msm_geni_serial_port *msm_port = GET_DEV_PORT(uport);
	unsigned int geni_status;
	unsigned int geni_ios;
	unsigned long long start_time;
	static unsigned int ios_log_limit;

	start_time = geni_capture_start_time(&msm_port->se, msm_port->ipc_log_kpi,
					     __func__, msm_port->uart_kpi);
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

	if (!uart_console(uport) && pm_runtime_enabled(uport->dev)) {
		if (msm_port->xfer_mode != GENI_GPI_DMA || !pm_runtime_active(uport->dev)) {
			UART_LOG_DBG(msm_port->ipc_log_misc, uport->dev,
				     "%s: Power on.\n", __func__);
			pm_runtime_get(uport->dev);
		}
	}
	/*
	 * If flush has been triggered earlier from userspace and port is
	 * still active(not yet closed) then reset the flush_buffers flag.
	 */
	if (atomic_read(&msm_port->flush_buffers))
		atomic_set(&msm_port->flush_buffers, 0);

	if (msm_port->xfer_mode == GENI_SE_FIFO) {
		geni_status = geni_read_reg(uport->membase,
						  SE_GENI_STATUS);
		if (geni_status & M_GENI_CMD_ACTIVE)
			goto check_flow_ctrl;

		if (!msm_geni_serial_tx_empty(uport))
			goto check_flow_ctrl;

		geni_m_irq_en = geni_read_reg(uport->membase,
						    SE_GENI_M_IRQ_EN);
		geni_m_irq_en |= (M_TX_FIFO_WATERMARK_EN | M_CMD_DONE_EN);

		geni_write_reg(msm_port->tx_wm, uport->membase,
						SE_GENI_TX_WATERMARK_REG);
		geni_write_reg(geni_m_irq_en, uport->membase,
							SE_GENI_M_IRQ_EN);
		/* Geni command setup should complete before returning.*/
		mb();
	} else if (msm_port->xfer_mode == GENI_SE_DMA) {
		if (msm_port->tx_dma || msm_port->split_dma_tre.immediate_dma_in_progress)
			goto check_flow_ctrl;

		if (msm_port->uart_kpi) {
			msm_port->uart_kpi_tx[msm_port->kpi_idx].xfer_req_sw.len =
									msm_port->xmit_size;
			msm_port->uart_kpi_tx[msm_port->kpi_idx].xfer_req_sw.time_stamp =
									sched_clock();
		}
		if (msm_geni_serial_prep_dma_tx(uport) == -EPERM) {
			UART_LOG_DBG(msm_port->ipc_log_tx, uport->dev, "%s: tx_en=0,\n",
				     __func__);
			goto exit_start_tx;
		}
	} else if (msm_port->xfer_mode == GENI_GPI_DMA) {
		if (msm_port->tx_dma || msm_port->split_dma_tre.immediate_dma_in_progress)
			goto check_flow_ctrl;
		queue_work(msm_port->tx_wq, &msm_port->tx_xfer_work);
	}
	geni_capture_stop_time(&msm_port->se, msm_port->ipc_log_kpi, __func__,
			       msm_port->uart_kpi, start_time,
			       msm_port->xmit_size, msm_port->cur_baud);
	return;
check_flow_ctrl:
	geni_ios = geni_read_reg(uport->membase, SE_GENI_IOS);
	/* check if SOC RFR is high and set the error code */
	if (geni_ios & IO2_DATA_IN)
		msm_geni_update_uart_error_code(msm_port,
						SOC_ERROR_START_TX_IOS_SOC_RFR_HIGH);
	if (++ios_log_limit % 5 == 0) {
		UART_LOG_DBG(msm_port->ipc_log_misc, uport->dev, "%s: ios: 0x%x\n",
						__func__, geni_ios);
		ios_log_limit = 0;
	}
	geni_capture_stop_time(&msm_port->se, msm_port->ipc_log_kpi, __func__,
			       msm_port->uart_kpi, start_time, 0, 0);
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

	geni_status = geni_read_reg(uport->membase, SE_GENI_STATUS);

	/* Possible stop tx is called multiple times. */
	if (!(geni_status & M_GENI_CMD_ACTIVE))
		return;

	if (port->xfer_mode == GENI_GPI_DMA) {
		queue_work(port->tx_wq, &port->tx_cancel_work);
		return;
	}

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
	geni_se_cancel_m_cmd(&port->se);

	timeout = geni_wait_for_cmd_done(uport, is_irq_masked);
	if (timeout) {
		IPC_LOG_MSG(port->console_log, "%s: tx_cancel failed 0x%x\n",
		__func__, geni_read_reg(uport->membase, SE_GENI_STATUS));
		UART_LOG_DBG(port->ipc_log_misc, uport->dev, "%s: tx_cancel failed 0x%x\n",
		__func__, geni_read_reg(uport->membase, SE_GENI_STATUS));
		msm_geni_update_uart_error_code(port, UART_ERROR_TX_CANCEL_FAIL);
		geni_se_dump_dbg_regs(uport);
		port->m_cmd_done = false;
		reinit_completion(&port->m_cmd_timeout);
		geni_se_abort_m_cmd(&port->se);

		timeout = geni_wait_for_cmd_done(uport, is_irq_masked);
		if (timeout) {
			IPC_LOG_MSG(port->console_log,
				"%s: tx abort failed 0x%x\n", __func__,
			geni_read_reg(uport->membase, SE_GENI_STATUS));
			UART_LOG_DBG(port->ipc_log_misc, uport->dev,
				"%s: tx abort failed 0x%x\n", __func__,
					geni_read_reg(uport->membase, SE_GENI_STATUS));
			msm_geni_update_uart_error_code(port, UART_ERROR_TX_ABORT_FAIL);
			geni_se_dump_dbg_regs(uport);
		}
		msm_geni_serial_allow_rx(port);
		geni_write_reg(FORCE_DEFAULT, uport->membase,
					GENI_FORCE_DEFAULT_REG);
	}

	if (port->xfer_mode == GENI_SE_DMA) {
		dma_dbg = geni_read_reg(uport->membase, SE_DMA_DEBUG_REG0);
		if (dma_dbg & DMA_TX_ACTIVE) {
			port->m_cmd_done = false;
			reinit_completion(&port->m_cmd_timeout);
			geni_write_reg(1, uport->membase,
						SE_DMA_TX_FSM_RST);

			timeout = geni_wait_for_cmd_done(uport,
							 is_irq_masked);
			if (timeout) {
				UART_LOG_DBG(port->ipc_log_misc, uport->dev,
					     "%s: tx fsm reset failed\n", __func__);
				msm_geni_update_uart_error_code(port,
								UART_ERROR_TX_FSM_RESET_FAIL);
			} else {
				/* Reset Cancel/Abort error code if FSM reset is success */
				msm_geni_update_uart_error_code(port,
								UART_ERROR_DEFAULT);
			}
		}

		if (port->tx_dma) {
			geni_se_tx_dma_unprep(&port->se,
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

	geni_status = geni_read_reg(uport->membase, SE_GENI_STATUS);
	UART_LOG_DBG(port->ipc_log_misc, uport->dev, "%s: End GENI:0x%x\n",
		    __func__, geni_status);
}

static void msm_geni_serial_stop_tx(struct uart_port *uport)
{
	struct msm_geni_serial_port *port = GET_DEV_PORT(uport);

	if (!uart_console(uport) && !pm_runtime_active(uport->dev)) {
		UART_LOG_DBG(port->ipc_log_misc, uport->dev,
			     "%s.Device is not in active state.\n", __func__);
		return;
	}
	stop_tx_sequencer(uport);
}

static void start_rx_sequencer(struct uart_port *uport)
{
	unsigned int geni_status;
	struct msm_geni_serial_port *port = GET_DEV_PORT(uport);
	u32 geni_se_param = (UART_PARAM_SKIP_FRAME_ERR_CHAR |
			     UART_PARAM_SKIP_BREAK_CHAR | UART_PARAM_RFR_OPEN);

	if (port->port_state == UART_PORT_CLOSED_SHUTDOWN)
		return;

	geni_status = geni_read_reg(uport->membase, SE_GENI_STATUS);
	UART_LOG_DBG(port->ipc_log_misc, uport->dev, "%s: geni_status 0x%x\n",
		     __func__, geni_status);

	if ((geni_status & S_GENI_CMD_ACTIVE) && port->xfer_mode == GENI_GPI_DMA) {
		return;
	} else if (port->xfer_mode == GENI_GPI_DMA) {
		UART_LOG_DBG(port->ipc_log_misc, uport->dev,
			     "%s: start xfer_rx\n", __func__);
		if (msm_geni_uart_gsi_xfer_rx(uport))
			UART_LOG_DBG(port->ipc_log_misc, uport->dev,
				     "%s: RX xfer is failed\n", __func__);
		geni_status = geni_read_reg(uport->membase, SE_GENI_STATUS);
		UART_LOG_DBG(port->ipc_log_misc, uport->dev,
			     "%s: xfer_rx done. geni_status:0x%x\n",
				__func__, geni_status);
		return;
	}
	if (geni_status & S_GENI_CMD_ACTIVE) {
		if (port->xfer_mode == GENI_SE_DMA) {
			UART_LOG_DBG(port->ipc_log_misc, uport->dev,
				"%s: mapping rx dma GENI: 0x%x\n",
				__func__, geni_status);
			geni_se_common_rx_dma_start(uport->membase, DMA_RX_BUF_SIZE,
								&port->rx_dma);
		}
		msm_geni_serial_stop_rx(uport);
	}

	if (port->xfer_mode == GENI_SE_DMA) {
		UART_LOG_DBG(port->ipc_log_misc, uport->dev,
			"%s. mapping rx dma\n", __func__);
		geni_se_common_rx_dma_start(uport->membase, DMA_RX_BUF_SIZE,
							&port->rx_dma);
	}

	/* Start RX with the RFR_OPEN to keep RFR in always ready state.
	 * Configure for character with Framing error & Break character
	 * is not written in RX fifo.
	 */
	geni_se_setup_s_cmd(&port->se, UART_START_READ, geni_se_param);
	msm_geni_serial_enable_interrupts(uport);

	/* Ensure that the above writes go through */
	mb();
	geni_status = geni_read_reg(uport->membase, SE_GENI_STATUS);
	UART_LOG_DBG(port->ipc_log_misc, uport->dev,
		"%s: geni_status 0x%x, dma_dbg:0x%x\n", __func__,
		geni_status, geni_read_reg(uport->membase, SE_DMA_DEBUG_REG0));
}

static void msm_geni_serial_start_rx(struct uart_port *uport)
{
	struct msm_geni_serial_port *port = GET_DEV_PORT(uport);

	if (!uart_console(uport) && !pm_runtime_active(uport->dev)) {
		UART_LOG_DBG(port->ipc_log_misc, uport->dev,
			     "%s.Device is not in active state.\n", __func__);
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
		geni_write_reg(uart_manual_rfr, port->uport.membase,
						SE_UART_MANUAL_RFR);
		/* UART FW needs delay per HW experts recommendation */
		udelay(10);

		uart_manual_rfr |= (UART_RFR_NOT_READY);
		geni_write_reg(uart_manual_rfr, port->uport.membase,
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
		geni_write_reg(0, port->uport.membase,
						SE_UART_MANUAL_RFR);
		/* Ensure that the manual flow off writes go through */
		mb();
		uart_manual_rfr = geni_read_reg(port->uport.membase,
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

	UART_LOG_DBG(port->ipc_log_misc, uport->dev, "%s %d\n", __func__, true);

	if (atomic_read(&port->stop_rx_inprogress)) {
		UART_LOG_DBG(port->ipc_log_misc, uport->dev,
			     "%s: already in progress, return\n", __func__);
		return -EBUSY;
	}
	atomic_set(&port->stop_rx_inprogress, 1);

	geni_status = geni_read_reg(uport->membase, SE_GENI_STATUS);
	/* Possible stop rx is called multiple times. */
	if (!(geni_status & S_GENI_CMD_ACTIVE)) {
		UART_LOG_DBG(port->ipc_log_misc, uport->dev,
			     "%s: RX is Inactive, geni_sts: 0x%x\n",
			     __func__, geni_status);
		atomic_set(&port->stop_rx_inprogress, 0);
		complete(&port->xfer);
		return 0;
	}

	if (port->gsi_mode) {
		if (!port->port_setup && !atomic_read(&port->gsi_rx_done)) {
			UART_LOG_DBG(port->ipc_log_misc, uport->dev,
				     "%s: Port setup not yet done\n", __func__);
			atomic_set(&port->stop_rx_inprogress, 0);
			return 0;
		}
		UART_LOG_DBG(port->ipc_log_misc, uport->dev,
			     "%s: Queue Rx Work\n", __func__);
		reinit_completion(&port->xfer);
		queue_work(port->rx_wq, &port->rx_cancel_work);
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

		dma_rx_status = geni_read_reg(uport->membase,
						SE_DMA_RX_IRQ_STAT);
		/* The transfer is completed at HW level and the completion
		 * interrupt is delayed. So process the transfer completion
		 * before issuing the cancel command to resolve the race
		 * btw cancel RX and completion interrupt.
		 */
		if (dma_rx_status) {
			s_irq_status = geni_read_reg(uport->membase,
							SE_GENI_S_IRQ_STATUS);
			geni_write_reg(s_irq_status, uport->membase,
							SE_GENI_S_IRQ_CLEAR);
			geni_se_dump_dbg_regs(uport);
			UART_LOG_DBG(port->ipc_log_misc, uport->dev, "%s: Interrupt delay\n",
					__func__);
			handle_rx_dma_xfer(s_irq_status, uport);
			if (pm_runtime_enabled(uport->dev) && !port->ioctl_count) {
				usage_count = atomic_read(&uport->dev->power.usage_count);
				UART_LOG_DBG(port->ipc_log_misc, uport->dev,
					"%s: Abort Stop Rx, extend the PM timer, usage_count:%d\n",
					__func__, usage_count);
				pm_runtime_mark_last_busy(uport->dev);
				atomic_set(&port->stop_rx_inprogress, 0);
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

	geni_se_cancel_s_cmd(&port->se);

	/*
	 * Ensure that the cancel goes through before polling for the
	 * cancel control bit.
	 */
	mb();
	timeout = geni_wait_for_cmd_done(uport, is_irq_masked);
	geni_status = geni_read_reg(uport->membase,
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
		geni_se_dump_dbg_regs(uport);

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

		/* Check if Cancel Interrupt arrived but irq is delayed */
		s_irq_status = geni_read_reg(uport->membase, SE_GENI_S_IRQ_STATUS);
		if (s_irq_status & S_CMD_CANCEL_EN) {
			/* Clear delayed Cancel IRQ */
			geni_write_reg(S_CMD_CANCEL_EN, uport->membase, SE_GENI_S_IRQ_CLEAR);
			UART_LOG_DBG(port->ipc_log_misc, uport->dev,
				     "%s Cancel Command succeeded 0x%x\n", __func__, s_irq_status);
			/* Reset the error code and skip abort operation */
			msm_geni_update_uart_error_code(port, UART_ERROR_DEFAULT);
			goto exit_enable_irq;
		}

		reinit_completion(&port->s_cmd_timeout);
		geni_se_abort_s_cmd(&port->se);
		/* Ensure this goes through before polling. */
		mb();

		timeout = geni_wait_for_cmd_done(uport, is_irq_masked);
		geni_status = geni_read_reg(uport->membase,
							SE_GENI_STATUS);
		is_rx_active = geni_status & S_GENI_CMD_ACTIVE;
		if (timeout || is_rx_active) {
			geni_status = geni_read_reg(uport->membase,
							SE_GENI_STATUS);
			UART_LOG_DBG(port->ipc_log_misc, uport->dev,
				"%s abort fail timeout:%d is_rx_active:%d 0x%x\n",
				__func__, timeout, is_rx_active, geni_status);
			IPC_LOG_MSG(port->console_log,
				"%s abort fail timeout:%d is_rx_active:%d 0x%x\n",
				 __func__, timeout, is_rx_active, geni_status);

			msm_geni_update_uart_error_code(port,
							UART_ERROR_RX_ABORT_FAIL);
			geni_se_dump_dbg_regs(uport);
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

		if (port->xfer_mode == GENI_SE_DMA) {
			port->s_cmd_done = false;
			reinit_completion(&port->s_cmd_timeout);
			geni_write_reg(1, uport->membase,
						SE_DMA_RX_FSM_RST);

			timeout = geni_wait_for_cmd_done(uport,
							 is_irq_masked);
			if (timeout) {
				UART_LOG_DBG(port->ipc_log_misc, uport->dev,
				"%s: rx fsm reset failed\n", __func__);
				msm_geni_update_uart_error_code(port, UART_ERROR_RX_FSM_RESET_FAIL);
				geni_se_dump_dbg_regs(uport);
			} else {
				/* Reset the CANCEL error code if abort is success */
				msm_geni_update_uart_error_code(port, UART_ERROR_DEFAULT);
			}
		}
	}
exit_enable_irq:
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

	geni_status = geni_read_reg(uport->membase, SE_GENI_STATUS);
	UART_LOG_DBG(port->ipc_log_misc, uport->dev,
		"%s: End geni_status : 0x%x dma_dbg:0x%x\n", __func__,
		geni_status, geni_read_reg(uport->membase, SE_DMA_DEBUG_REG0));

	complete(&port->xfer);
	atomic_set(&port->stop_rx_inprogress, 0);
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

	if (!uart_console(uport) && !pm_runtime_active(uport->dev)) {
		UART_LOG_DBG(port->ipc_log_misc, uport->dev,
			     "%s.Device is not in active state.\n", __func__);
		complete(&port->xfer);
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
	if (ret != rx_bytes)
		dev_err(uport->dev, "%s: ret %d rx_bytes %d\n", __func__, ret, rx_bytes);

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
	rx_fifo_status = geni_read_reg(uport->membase,
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
	struct tty_port *tport = &uport->state->port;
	int avail_fifo_bytes = 0;
	unsigned int bytes_remaining = 0;
	unsigned int pending;
	int i = 0;
	unsigned int tx_fifo_status;
	unsigned int xmit_size;
	unsigned int fifo_width_bytes =
		(uart_console(uport) ? 1 : (msm_port->tx_fifo_width >> 3));
	int irq_en;

	tx_fifo_status = geni_read_reg(uport->membase,
					SE_GENI_TX_FIFO_STATUS);

	/* Complete the current tx command before taking newly added data */
	pending = active ? msm_port->cur_tx_remaining : kfifo_len(&tport->xmit_fifo);

	/* All data has been transmitted and acknowledged as received */
	if (!pending && !tx_fifo_status && done)
		goto exit_handle_tx;

	avail_fifo_bytes = msm_port->tx_fifo_depth - (tx_fifo_status &
								TX_FIFO_WC);
	avail_fifo_bytes *= fifo_width_bytes;
	if (avail_fifo_bytes < 0)
		avail_fifo_bytes = 0;

	xmit_size = min_t(unsigned int, avail_fifo_bytes, pending);
	if (!xmit_size)
		goto exit_handle_tx;

	if (!msm_port->cur_tx_remaining) {
		msm_geni_serial_setup_tx(uport, pending);
		msm_port->cur_tx_remaining = pending;

		/* Re-enable WM interrupt when starting new transfer */
		irq_en = geni_read_reg(uport->membase, SE_GENI_M_IRQ_EN);
		if (!(irq_en & M_TX_FIFO_WATERMARK_EN))
			geni_write_reg(irq_en | M_TX_FIFO_WATERMARK_EN,
					uport->membase, SE_GENI_M_IRQ_EN);
	}

	bytes_remaining = xmit_size;
	while (i < xmit_size) {
		unsigned int tx_bytes;
		unsigned int buf = 0;

		tx_bytes = ((bytes_remaining < fifo_width_bytes) ?
					bytes_remaining : fifo_width_bytes);

		tx_bytes = kfifo_out(&tport->xmit_fifo, (unsigned char *)&buf, tx_bytes);

		geni_write_reg(buf, uport->membase, SE_GENI_TX_FIFOn);

		i += tx_bytes;
		bytes_remaining -= tx_bytes;
		uport->icount.tx += tx_bytes;
		msm_port->cur_tx_remaining -= tx_bytes;
		/* Ensure FIFO write goes through */
		wmb();
	}

	/*
	 * The tx fifo watermark is level triggered and latched. Though we had
	 * cleared it in qcom_geni_serial_isr it will have already reasserted
	 * so we must clear it again here after our writes.
	 */
	geni_write_reg(M_TX_FIFO_WATERMARK_EN, uport->membase,
						SE_GENI_M_IRQ_CLEAR);

exit_handle_tx:
	irq_en = geni_read_reg(uport->membase, SE_GENI_M_IRQ_EN);
	if (!msm_port->cur_tx_remaining)
		/* Clear WM interrupt post each transfer completion */
		geni_write_reg(irq_en & ~M_TX_FIFO_WATERMARK_EN,
					uport->membase, SE_GENI_M_IRQ_EN);

	if (kfifo_len(&tport->xmit_fifo) < WAKEUP_CHARS)
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
			geni_se_dump_dbg_regs(uport);
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
	unsigned int rx_bytes = 0, rx_bytes_copied = 0;
	struct tty_port *tport;
	int ret = 0, cnt = 0, offset = 0;
	unsigned char *rx_buf;
	unsigned int geni_status;
	unsigned long long start_time;

	start_time = geni_capture_start_time(&msm_port->se, msm_port->ipc_log_kpi,
					     __func__, msm_port->uart_kpi);
	geni_status = geni_read_reg(uport->membase, SE_GENI_STATUS);
	/* Possible stop rx is called */
	if (!(geni_status & S_GENI_CMD_ACTIVE)) {
		UART_LOG_DBG(msm_port->ipc_log_misc, uport->dev,
			     "%s: GENI: 0x%x\n", __func__, geni_status);
		return 0;
	}

	if (unlikely(!msm_port->rx_buf)) {
		UART_LOG_DBG(msm_port->ipc_log_rx, uport->dev, "%s: NULL Rx_buf\n", __func__);
		return 0;
	}

	rx_bytes = geni_read_reg(uport->membase, SE_DMA_RX_LEN_IN);
	if (unlikely(!rx_bytes)) {
		UART_LOG_DBG(msm_port->ipc_log_rx, uport->dev, "%s: Size %d\n", __func__, rx_bytes);
		return 0;
	}

	if (msm_port->wakeup_byte)
		/* Check RX buffer data for faulty pattern */
		check_rx_buf((char *)msm_port->rx_buf, uport, rx_bytes);

	if (drop_rx)
		return 0;

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
	dump_ipc(uport, msm_port->ipc_log_rx, "DMA Rx Before tty_insert_flip_string",
	(char *)msm_port->rx_buf, 0, rx_bytes);
	tport = &uport->state->port;
	ret = tty_insert_flip_string(tport, (unsigned char *)(msm_port->rx_buf), rx_bytes);
	rx_bytes_copied = ret;
	if (ret != rx_bytes) {
		UART_LOG_DBG(msm_port->ipc_log_rx, uport->dev,
			     "%s: ret %d rx_bytes %d\n", __func__, ret, rx_bytes);
		rx_buf = (unsigned char *)(msm_port->rx_buf);
		rx_buf += ret;
		/* Bytes still left to copy from rx buffer */
		rx_bytes = rx_bytes - ret;
		while (rx_bytes) {
			/*
			 * Allocation in tty layer can fail due to higher order page
			 * request, hence try copying in chunks of 256 bytes which will
			 * use zero order pages.
			 */
			cnt = rx_bytes < 256 ? rx_bytes : 256;
			UART_LOG_DBG(msm_port->ipc_log_rx, uport->dev,
				     "%s: To copy %d, try copying %d\n", __func__, rx_bytes, cnt);
			ret = tty_insert_flip_string(tport, &rx_buf[offset], cnt);
			if (ret != cnt) {
				UART_LOG_DBG(msm_port->ipc_log_rx, uport->dev,
					     "%s: Unable to copy %d bytes rx_bytes %d\n",
					     __func__, cnt, rx_bytes);
				msm_geni_update_uart_error_code(msm_port,
								UART_ERROR_RX_TTY_INSERT_FAIL);
				break;
			}
			offset += cnt;
			rx_bytes -= cnt;
			rx_bytes_copied += ret;
		}
	}

	uport->icount.rx += rx_bytes_copied;
	tty_flip_buffer_push(tport);
	dump_ipc(uport, msm_port->ipc_log_rx, "DMA Rx",
		 (char *)msm_port->rx_buf, 0, rx_bytes_copied);
	/*
	 * DMA_DONE interrupt doesn't confirm that the DATA is copied to
	 * DDR memory, sometimes we are queuing the stale data from previous
	 * transfer to tty flip_buffer, adding memset to zero
	 * change to idenetify such scenario.
	 */
	memset(msm_port->rx_buf, 0, rx_bytes_copied);

	if (msm_port->uart_error == UART_ERROR_RX_PARITY_ERROR ||
		msm_port->uart_error == UART_ERROR_RX_BREAK_ERROR ||
		msm_port->uart_error == UART_ERROR_RX_FRAMING_ERR)
		msm_geni_update_uart_error_code(msm_port, UART_ERROR_DEFAULT);

	geni_capture_stop_time(&msm_port->se, msm_port->ipc_log_kpi, __func__,
			       msm_port->uart_kpi, start_time, rx_bytes, msm_port->cur_baud);
	return rx_bytes_copied;

}

static int msm_geni_serial_handle_dma_tx(struct uart_port *uport)
{
	struct msm_geni_serial_port *msm_port = GET_DEV_PORT(uport);
	struct tty_port *tport = &uport->state->port;

	uart_xmit_advance(uport, msm_port->xmit_size);
	if (msm_port->tx_dma && !msm_port->split_dma_tre.immediate_dma_in_progress)
		geni_se_tx_dma_unprep(&msm_port->se, msm_port->tx_dma, msm_port->xmit_size);

	msm_uart_log_tx_kpi(uport);

	uport->icount.tx += msm_port->xmit_size;
	msm_port->tx_dma = (dma_addr_t)NULL;

	if (msm_port->split_dma_tre.immediate_dma_in_progress)
		msm_port->split_dma_tre.immediate_dma_in_progress = false;

	msm_port->xmit_size = 0;

	if (!kfifo_is_empty(&tport->xmit_fifo))
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
	u32 geni_status = geni_read_reg(uport->membase, SE_GENI_STATUS);
	u32 m_irq_en = geni_read_reg(uport->membase, SE_GENI_M_IRQ_EN);

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
	u32 dma_tx_status = geni_read_reg(uport->membase,
							SE_DMA_TX_IRQ_STAT);
	if (dma_tx_status) {
		geni_write_reg(dma_tx_status, uport->membase,
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
	unsigned long long start_time;

	start_time = geni_capture_start_time(&msm_port->se, msm_port->ipc_log_kpi,
					     __func__, msm_port->uart_kpi);
	spin_lock_irqsave(&msm_port->rx_lock, lock_flags);
	dma_rx_status = geni_read_reg(uport->membase, SE_DMA_RX_IRQ_STAT);
	if (dma_rx_status) {
		geni_write_reg(dma_rx_status, uport->membase, SE_DMA_RX_IRQ_CLR);

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
			geni_se_dump_dbg_regs(uport);
		}

		if (dma_rx_status & UART_DMA_RX_BREAK) {
			uport->icount.brk++;
			UART_LOG_DBG(msm_port->ipc_log_misc, uport->dev,
				     "%s dma_rx_status:0x%x Rx Break error:%d\n",
				     __func__, dma_rx_status, uport->icount.brk);
			msm_geni_update_uart_error_code(msm_port,
							UART_ERROR_RX_BREAK_ERROR);
			geni_se_dump_dbg_regs(uport);
		}

		if (dma_rx_status & RX_EOT || dma_rx_status & RX_DMA_DONE) {
			msm_geni_serial_handle_dma_rx(uport, drop_rx);
			if (!(dma_rx_status & RX_GENI_CANCEL_IRQ)) {
				UART_LOG_DBG(msm_port->ipc_log_misc, uport->dev,
					     "%s mapping rx dma\n", __func__);
				geni_se_common_rx_dma_start(uport->membase,
							    DMA_RX_BUF_SIZE,
							    &msm_port->rx_dma);
			} else {
				UART_LOG_DBG(msm_port->ipc_log_misc, uport->dev,
					     "%s not mapping rx dma\n", __func__);
			}
		}

		if (dma_rx_status & RX_SBE) {
			UART_LOG_DBG(msm_port->ipc_log_misc, uport->dev,
				     "%s dma_rx_status:0x%x\n", __func__, dma_rx_status);
			msm_geni_update_uart_error_code(msm_port, UART_ERROR_RX_SBE_ERROR);
		}

		if (dma_rx_status & (RX_EOT | RX_GENI_CANCEL_IRQ | RX_DMA_DONE))
			ret = true;
	}

	if (s_irq_status & (S_CMD_CANCEL_EN | S_CMD_ABORT_EN))
		ret = true;

	spin_unlock_irqrestore(&msm_port->rx_lock, lock_flags);
	geni_capture_stop_time(&msm_port->se, msm_port->ipc_log_kpi, __func__,
			       msm_port->uart_kpi, start_time, 0, 0);
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

	dma_tx_status = geni_read_reg(uport->membase, SE_DMA_TX_IRQ_STAT);
	if (dma_tx_status)
		geni_write_reg(dma_tx_status, uport->membase, SE_DMA_TX_IRQ_CLR);

	dma_rx_status = geni_read_reg(uport->membase, SE_DMA_RX_IRQ_STAT);
	if (dma_rx_status)
		geni_write_reg(dma_rx_status, uport->membase, SE_DMA_RX_IRQ_CLR);
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
	unsigned long long start_time;

	start_time = geni_capture_start_time(&msm_port->se, msm_port->ipc_log_kpi,
					     __func__, msm_port->uart_kpi);
	if (uart_console(uport) && atomic_read(&msm_port->is_clock_off)) {
		IPC_LOG_MSG(msm_port->console_log,
			"%s. Console in suspend state\n", __func__);
		goto exit_geni_serial_isr;
	}

	m_irq_status = geni_read_reg(uport->membase,
						SE_GENI_M_IRQ_STATUS);
	s_irq_status = geni_read_reg(uport->membase,
						SE_GENI_S_IRQ_STATUS);
	if (uart_console(uport))
		IPC_LOG_MSG(msm_port->console_log,
			"%s. sirq 0x%x mirq:0x%x\n", __func__, s_irq_status,
			m_irq_status);
	geni_write_reg(m_irq_status, uport->membase,
						SE_GENI_M_IRQ_CLEAR);
	geni_write_reg(s_irq_status, uport->membase,
						SE_GENI_S_IRQ_CLEAR);
	if (msm_port->one_wire_uart.enable &&
	    msm_port->one_wire_uart.output_mode == OPEN_DRAIN_MODE) {
		if (m_irq_status & M_GP_IRQ_0_EN)
			msm_port->one_wire_uart.error = UART_ERROR_CONTENTION;
	}

	if ((m_irq_status & M_ILLEGAL_CMD_EN)) {
		if (uart_console(uport))
			IPC_LOG_MSG(msm_port->console_log,
				    "%s Illegal interrupt sirq:0x%x mirq:0x%x\n",
				    __func__, s_irq_status, m_irq_status);
		else {
			msm_geni_update_uart_error_code(msm_port, UART_ERROR_ILLEGAL_INTERRUPT);
			UART_LOG_DBG(msm_port->ipc_log_misc, uport->dev,
				     "%s Illegal interrupt sirq:0x%x mirq:0x%x\n",
				     __func__, s_irq_status, m_irq_status);
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

	dma = geni_read_reg(uport->membase, SE_GENI_DMA_MODE_EN);
	if (!dma) {
		m_cmd_done = handle_tx_fifo_xfer(m_irq_status, uport);
		s_cmd_done = handle_rx_fifo_xfer(s_irq_status, uport, flags,
							is_irq_masked);
	} else {
		dma_tx_status = geni_read_reg(uport->membase,
							SE_DMA_TX_IRQ_STAT);
		dma_rx_status = geni_read_reg(uport->membase,
							SE_DMA_RX_IRQ_STAT);

		if (m_irq_status || s_irq_status ||
			dma_tx_status || dma_rx_status) {
			UART_LOG_DBG(msm_port->ipc_log_irqstatus, uport->dev,
				     "%s: sirq:0x%x mirq:0x%x dma_txirq:0x%x\n",
				      __func__, s_irq_status, m_irq_status, dma_tx_status);
			UART_LOG_DBG(msm_port->ipc_log_irqstatus, uport->dev,
				     "%s: dma_rxirq:0x%x is_irq_masked:%d cts_count:%d\n",
				      __func__, dma_rx_status, is_irq_masked, uport->icount.cts);
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
	geni_capture_stop_time(&msm_port->se, msm_port->ipc_log_kpi, __func__,
			       msm_port->uart_kpi, start_time, 0, 0);

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

	uport = &port->uport;
	UART_LOG_DBG(port->ipc_log_rx, uport->dev, "Wakeup work start\n");

	if (!atomic_read(&port->check_wakeup_byte))
		return;

	if (port->xfer_mode == GENI_GPI_DMA) {
		if (msm_geni_serial_power_on(uport))
			UART_LOG_DBG(port->ipc_log_pwr, uport->dev, "Failed to power on\n");
		return;
	}

	reinit_completion(&port->wakeup_comp);
	if (msm_geni_serial_power_on(uport)) {
		atomic_set(&port->check_wakeup_byte, 0);
		UART_LOG_DBG(port->ipc_log_rx, uport->dev,
			     "%s:Failed to power on\n", __func__);
		return;
	}
	/* wait to receive wakeup byte in rx path */
	if (!wait_for_completion_timeout(&port->wakeup_comp,
					 msecs_to_jiffies(WAKEBYTE_TIMEOUT_MSEC)))
		UART_LOG_DBG(port->ipc_log_rx, uport->dev,
			     "%s completion of wakeup_comp task timedout %dmsec\n",
			     __func__, WAKEBYTE_TIMEOUT_MSEC);
	msm_geni_serial_power_off(uport);
}

static irqreturn_t msm_geni_wakeup_isr(int isr, void *dev)
{
	struct uart_port *uport = dev;
	struct msm_geni_serial_port *port = GET_DEV_PORT(uport);
	struct tty_struct *tty;
	unsigned long flags;
	unsigned long long start_time;

	start_time = geni_capture_start_time(&port->se, port->ipc_log_kpi,
					     __func__, port->uart_kpi);
	UART_LOG_DBG(port->ipc_log_rx, uport->dev, "%s %d\n", __func__, true);

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
		spin_unlock_irqrestore(&uport->lock, flags);
		return IRQ_HANDLED;
	}

	atomic_set(&port->check_wakeup_byte, 1);
	queue_delayed_work(port->wakeup_irq_wq, &port->wakeup_irq_dwork, 0);
	spin_unlock_irqrestore(&uport->lock, flags);
	UART_LOG_DBG(port->ipc_log_rx, uport->dev, "%s: End %d\n", __func__, true);

	geni_capture_stop_time(&port->se, port->ipc_log_kpi, __func__,
			       port->uart_kpi, start_time, 0, 0);

	return IRQ_HANDLED;
}

static int get_tx_fifo_size(struct msm_geni_serial_port *port)
{
	struct uart_port *uport;

	if (!port)
		return -ENODEV;

	uport = &port->uport;
	port->tx_fifo_depth = geni_se_get_tx_fifo_depth(&port->se);
	if (!port->tx_fifo_depth) {
		dev_err(uport->dev, "%s:Invalid TX FIFO depth read\n",
								__func__);
		return -ENXIO;
	}

	port->tx_fifo_width = geni_se_get_tx_fifo_width(&port->se);
	if (!port->tx_fifo_width) {
		dev_err(uport->dev, "%s:Invalid TX FIFO width read\n",
								__func__);
		return -ENXIO;
	}

	port->rx_fifo_depth = geni_se_get_rx_fifo_depth(&port->se);
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
	struct device *rx_dev = msm_port->wrapper_dev;
	struct device *tx_dev = msm_port->wrapper_dev;
	int ret = 0, j = 0, i, timeout;
	unsigned long long start_time;
	int usage_count = atomic_read(&uport->dev->power.usage_count);

	UART_LOG_DBG(msm_port->ipc_log_misc, uport->dev, "%s: %d\n", __func__, true);
	msm_port->port_state = UART_PORT_SHUTDOWN_IN_PROGRESS;

	start_time = geni_capture_start_time(&msm_port->se, msm_port->ipc_log_kpi,
					     __func__, msm_port->uart_kpi);

	/* Stop the console before stopping the current tx */
	if (uart_console(uport)) {
		console_stop(uport->cons);
		disable_irq(uport->irq);
	} else {
		if (!usage_count || !pm_runtime_active(uport->dev))
			msm_geni_serial_power_on(uport);

		if (msm_port->xfer_mode == GENI_GPI_DMA) {
			/* Prevent device suspend */
			pm_runtime_forbid(uport->dev);

			/* From the framework every time the stop
			 * rx sequncer will be called before the closing
			 * of UART port and due to atomic context we can't
			 * use the wait_for_completion_timeout() api in
			 * stop_rx_sequencer() hence wait for completion
			 * of Rx channel reset
			 */
			UART_LOG_DBG(msm_port->ipc_log_misc, uport->dev,
					"%s: Stop Rx Engine\n", __func__);
			timeout = wait_for_completion_timeout
				(&msm_port->xfer,
				msecs_to_jiffies(GSI_STOP_RX_TIMEOUT));

			if (!timeout)
				UART_LOG_DBG(msm_port->ipc_log_misc,
					     uport->dev,
					     "%s: Timeout for Rx reset\n",
					     __func__);

			if (msm_port->gsi->rx_c) {
				UART_LOG_DBG(msm_port->ipc_log_misc,
					     uport->dev,
					     "%s:GSI DMA-Rx ch\n", __func__);
				if (msm_port->rx_wq)
					flush_workqueue(msm_port->rx_wq);

				for (i = 0; i < NUM_RX_BUF; i++) {
					if (msm_port->dma_addr[i]) {
						geni_se_common_iommu_free_buf(rx_dev,
								      &msm_port->dma_addr[i],
								      msm_port->rx_gsi_buf[i],
								      DMA_RX_BUF_SIZE);
						msm_port->rx_gsi_buf[i] = NULL;
					}
				}
				dma_release_channel(msm_port->gsi->rx_c);
				msm_port->gsi->rx_c = NULL;
				UART_LOG_DBG(msm_port->ipc_log_misc,
					     uport->dev, "%s: Rx unmap buf done\n",
					     __func__);
			}
			if (msm_port->gsi->tx_c) {
				UART_LOG_DBG(msm_port->ipc_log_misc,
					     uport->dev, "%s:GSI DMA-Tx ch\n",
					     __func__);
				if (msm_port->tx_wq)
					flush_workqueue(msm_port->tx_wq);

				if (msm_port->tx_dma) {
					geni_se_common_iommu_unmap_buf(tx_dev,
								       &msm_port->tx_dma,
								       msm_port->xmit_size,
								       DMA_TO_DEVICE);
					UART_LOG_DBG(msm_port->ipc_log_misc,
						     uport->dev, "%s: Tx unmap buf done\n",
						     __func__);
				}
				dma_release_channel(msm_port->gsi->tx_c);
				msm_port->gsi->tx_c = NULL;
			}

			/* Allow device to suspend */
			pm_runtime_allow(uport->dev);
		} else {
			msm_geni_serial_stop_tx(uport);
		}

		if (msm_port->pm_auto_suspend_disable)
			disable_irq(uport->irq);

		if (msm_port->ioctl_count) {
			UART_LOG_DBG(msm_port->ipc_log_pwr, uport->dev,
				     "%s: IOCTL vote present. Resetting ioctl count\n", __func__);
			msm_port->ioctl_count = 0;
		}

		if (pm_runtime_enabled(uport->dev)) {
			ret = pm_runtime_put_sync_suspend(uport->dev);
			if (ret < 0)
				UART_LOG_DBG(msm_port->ipc_log_pwr, uport->dev,
					     "%s: Failed to suspend ret=%d\n",
					     __func__, ret);
			if (ret == -EBUSY) {
				do {
					UART_LOG_DBG(msm_port->ipc_log_pwr, uport->dev,
						     "%s: Failed to suspend ret:%d retry:%d\n",
						     __func__, ret, j);
					/* Sleep for 50msecs and check if port is suspended */
					usleep_range(45000, 50000);
					if (!pm_runtime_active(uport->dev)) {
						UART_LOG_DBG(msm_port->ipc_log_pwr, uport->dev,
							     "%s Uport not active\n", __func__);
						ret = 0;
						break;
					}
					j++;
				} while (j <= SUSPEND_RETRY_COUNT);
			}
		}

		if (j == SUSPEND_RETRY_COUNT + 1) {
			if (!pm_runtime_active(uport->dev))
				UART_LOG_DBG(msm_port->ipc_log_pwr, uport->dev,
					     "%s Uport not active\n", __func__);
			else
				UART_LOG_DBG(msm_port->ipc_log_pwr, uport->dev,
					     "%s Error! Unable to put uport to Suspend\n",
					     __func__);
		}

		if (!IS_ERR_OR_NULL(msm_port->serial_rsc.geni_gpio_shutdown)) {
			ret = pinctrl_select_state(
				msm_port->serial_rsc.geni_pinctrl,
				msm_port->serial_rsc.geni_gpio_shutdown);
			if (ret)
				UART_LOG_DBG(msm_port->ipc_log_misc, uport->dev,
					     "%s: Error %d pinctrl_select_state\n",
					     __func__, ret);
		}

		/* Reset UART error to default during port_close() */
		msm_port->uart_error = UART_ERROR_DEFAULT;
		atomic_set(&msm_port->flush_buffers, 0);
		msm_port->current_termios = NULL;
		msm_port->rx_buf_idx = 0;
	}
	msm_port->port_state = UART_PORT_CLOSED_SHUTDOWN;
	geni_capture_stop_time(&msm_port->se, msm_port->ipc_log_kpi, __func__,
			       msm_port->uart_kpi, start_time, 0, 0);
	UART_LOG_DBG(msm_port->ipc_log_misc, uport->dev, "%s: End %d\n", __func__, ret);
}

/*
 * msm_geni_one_wire_uart_config() - Config the SE in one wire UART mode.
 *
 * @uport: pointer to uart port
 *
 * Return: 0 on suceess or negative error code upon failure.
 */
static int msm_geni_one_wire_uart_config(struct uart_port *uport)
{
	u32 uart_io_macro_reg = 0, geni_cfg_reg64 = 0, geni_cfg_reg67 = 0;
	u32 geni_cfg_reg76 = 0, geni_cfg_reg81 = 0, geni_cfg_reg87 = 0;
	u32 geni_cfg_seq_start = 0, geni_cfg_status = 0;
	u32 tx_trans_cfg = 0, iter = 0;
	struct msm_geni_serial_port *msm_port = GET_DEV_PORT(uport);

	uart_io_macro_reg = geni_read_reg(uport->membase, SE_UART_IO_MACRO_CTRL);

	if (msm_port->one_wire_uart.qup_lane == QUP_L3_LANE) {
		uart_io_macro_reg &= ~(UART_IO_MACRO_IO2_SEL_MASK);
		uart_io_macro_reg |= 0x1 << UART_IO_MACRO_IO3_SEL_SHIFT;
	} else {
		/* Use the lane2 for one wire UART communication */
		uart_io_macro_reg &= ~(UART_IO_MACRO_RX_DATA_IN_SEL_MASK);
		uart_io_macro_reg |= 0x2 << UART_IO_MACRO_RX_DATA_IN_SEL_SHIFT;
	}

	geni_write_reg(uart_io_macro_reg, uport->membase, SE_UART_IO_MACRO_CTRL);

	if (msm_port->one_wire_uart.output_mode == OPEN_DRAIN_MODE) {
		geni_cfg_reg67 = geni_read_reg(uport->membase, SE_GENI_CFG_REG67);
		geni_cfg_reg67 &= ~TX_DEFAULT_SOE_VALUE;
		geni_write_reg(geni_cfg_reg67, uport->membase, SE_GENI_CFG_REG67);
		/* Ensure that above register writes went through */
		geni_read_reg(uport->membase, SE_GENI_CFG_REG67);
		geni_cfg_reg67 &= ~TX_DEFAULT_SOUT_VALUE;
		geni_write_reg(geni_cfg_reg67, uport->membase, SE_GENI_CFG_REG67);
		geni_write_reg(0x1, uport->membase, GENI_FORCE_DEFAULT_REG);
	}

	geni_cfg_reg64 = geni_read_reg(uport->membase, SE_GENI_CFG_REG64);
	geni_cfg_reg64 |= UART_SINGLE_WIRE_EN;
	geni_write_reg(geni_cfg_reg64, uport->membase, SE_GENI_CFG_REG64);

	tx_trans_cfg = geni_read_reg(uport->membase, SE_UART_TX_TRANS_CFG);
	tx_trans_cfg |= UART_CTS_MASK;
	geni_write_reg(tx_trans_cfg, uport->membase, SE_UART_TX_TRANS_CFG);

	if (msm_port->one_wire_uart.output_mode == OPEN_DRAIN_MODE) {
		geni_cfg_reg76 = geni_read_reg(uport->membase, SE_GENI_CFG_REG76);
		geni_cfg_reg76 |= TX_ENGINE_INV_EN;
		geni_write_reg(geni_cfg_reg76, uport->membase, SE_GENI_CFG_REG76);

		geni_cfg_reg81 = geni_read_reg(uport->membase, SE_GENI_CFG_REG81);
		geni_cfg_reg81 |= GENI_SOUT0_PULL_UP_EN;
		geni_write_reg(geni_cfg_reg81, uport->membase, SE_GENI_CFG_REG81);

		geni_cfg_reg87 = geni_read_reg(uport->membase, SE_UART_TX_TRANS_CFG);
		geni_cfg_reg87 |= M_SW_COMP3;
		geni_write_reg(geni_cfg_reg87, uport->membase, SE_UART_TX_TRANS_CFG);
	}

	geni_cfg_seq_start = geni_read_reg(uport->membase, SE_GENI_CFG_SEQ_START);
	geni_cfg_seq_start |= UART_START_TRIGGER;
	geni_write_reg(geni_cfg_seq_start, uport->membase, SE_GENI_CFG_SEQ_START);

	geni_cfg_status = geni_read_reg(uport->membase, GENI_CFG_STATUS);

	/* Poll for UART CFG SEQ Done */
	while (iter < POLL_ITERATIONS) {
		geni_cfg_status = geni_read_reg(uport->membase, GENI_CFG_STATUS);
		if (geni_cfg_status & UART_CFG_SEQ_DONE)
			break;
		usleep_range(10, 20);
		iter++;
	}

	if (iter >= POLL_ITERATIONS) {
		UART_LOG_DBG(msm_port->ipc_log_misc, msm_port->uport.dev,
			     "%s: uart_io_macro_reg:0x%x\n", __func__, uart_io_macro_reg);
		UART_LOG_DBG(msm_port->ipc_log_misc, msm_port->uport.dev,
			     "geni_cfg_reg64:0x%x\n", geni_cfg_reg64);
		UART_LOG_DBG(msm_port->ipc_log_misc, msm_port->uport.dev,
			     "geni_cfg_reg67:0x%x\n", geni_cfg_reg67);
		UART_LOG_DBG(msm_port->ipc_log_misc, msm_port->uport.dev,
			     "geni_cfg_reg76:0x%x\n", geni_cfg_reg76);
		UART_LOG_DBG(msm_port->ipc_log_misc, msm_port->uport.dev,
			     "geni_cfg_reg81:0x%x\n", geni_cfg_reg81);
		UART_LOG_DBG(msm_port->ipc_log_misc, msm_port->uport.dev,
			     "geni_cfg_reg87:0x%x\n", geni_cfg_reg87);
		UART_LOG_DBG(msm_port->ipc_log_misc, msm_port->uport.dev,
			     "geni_cfg_seq_start: 0x%x\n", geni_cfg_seq_start);
		UART_LOG_DBG(msm_port->ipc_log_misc, msm_port->uport.dev,
			     "geni_cfg_status: 0x%x\n", geni_cfg_status);
		return -EAGAIN;
	}

	return 0;
}

static int msm_geni_serial_port_setup(struct uart_port *uport)
{
	int ret = 0;
	struct msm_geni_serial_port *msm_port = GET_DEV_PORT(uport);
	dma_addr_t dma_address;
	unsigned int rxstale = STALE_COUNT;
	unsigned long long start_time;

	start_time = geni_capture_start_time(&msm_port->se, msm_port->ipc_log_kpi,
					     __func__, msm_port->uart_kpi);
	set_rfr_wm(msm_port);
	geni_write_reg(rxstale, uport->membase, SE_UART_RX_STALE_CNT);
	if (msm_port->gsi_mode) {
		msm_port->xfer_mode = GENI_GPI_DMA;
	} else if (!uart_console(uport)) {
		/* For now only assume FIFO mode. */
		msm_port->xfer_mode = GENI_SE_DMA;
		/* TX packing */
		geni_se_config_packing(&msm_port->se, 8, 4, false, true, false);
		/* RX packing */
		geni_se_config_packing(&msm_port->se, 8, 4, false, false, true);
		geni_write_reg(0x431c, uport->membase, SE_GENI_CFG_REG80);
		if (!msm_port->rx_fifo) {
			ret = -ENOMEM;
			goto exit_portsetup;
		}

		if (!msm_port->rx_buf) {
			msm_port->rx_buf =
				geni_se_common_iommu_alloc_buf(msm_port->wrapper_dev,
							       &dma_address, DMA_RX_BUF_SIZE);
			if (!msm_port->rx_buf) {
				ret = -ENOMEM;
				goto exit_portsetup;
			}
			msm_port->rx_dma = dma_address;
		}
	} else {
		/*
		 * Make an unconditional cancel on the main sequencer to reset
		 * it else we could end up in data loss scenarios.
		 */
		msm_port->xfer_mode = GENI_SE_FIFO;
		msm_serial_try_disable_interrupts(uport);
		msm_geni_serial_poll_tx_done(uport);
		msm_geni_serial_enable_interrupts(uport);
		geni_se_config_packing(&msm_port->se, 8, 1, false, true, false);
		geni_se_config_packing(&msm_port->se, 8, 4, false, false, true);
	}

	if (msm_port->one_wire_uart.enable) {
		ret = msm_geni_one_wire_uart_config(uport);
		if (ret)
			goto exit_portsetup;
	}

	geni_se_init(&msm_port->se, msm_port->rx_wm, msm_port->rx_rfr);

	geni_se_select_mode(&msm_port->se, msm_port->xfer_mode);

	if (msm_port->split_dma_tre.split_tx) {
		msm_port->split_dma_tre.aligned_tx_buf =
			geni_se_common_iommu_alloc_buf(msm_port->wrapper_dev,
						       &msm_port->split_dma_tre.aligned_tx_dma_buf,
						       SERIAL_PREALLOC_BUF_SIZE);

		if (IS_ERR_OR_NULL(msm_port->split_dma_tre.aligned_tx_buf)) {
			ret = -ENOMEM;
			UART_LOG_DBG(msm_port->ipc_log_misc, msm_port->uport.dev,
				     "Error failed to allocate memory\n");
			goto exit_portsetup;
		}
	}
	msm_port->port_setup = true;
	/*
	 * Ensure Port setup related IO completes before returning to
	 * framework.
	 */
	mb();
	geni_capture_stop_time(&msm_port->se, msm_port->ipc_log_kpi, __func__,
			       msm_port->uart_kpi, start_time, 0, 0);
	return 0;

exit_portsetup:
	return ret;
}

static int msm_geni_serial_startup(struct uart_port *uport)
{
	int ret = 0;
	struct msm_geni_serial_port *msm_port = GET_DEV_PORT(uport);
	unsigned long long start_time;

	start_time = geni_capture_start_time(&msm_port->se, msm_port->ipc_log_kpi,
					     __func__, msm_port->uart_kpi);
	UART_LOG_DBG(msm_port->ipc_log_misc, uport->dev, "%s: Start %d\n", __func__, true);

	if (likely(!uart_console(uport))) {
		msm_port->resuming_from_deep_sleep = false;

		ret = msm_geni_serial_power_on(&msm_port->uport);
		if (ret) {
			dev_err(uport->dev, "%s:Failed to power on %d\n", __func__, ret);
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
	if (uart_console(uport) ||  msm_port->pm_auto_suspend_disable)
		enable_irq(uport->irq);
	msm_port->port_state = UART_PORT_OPEN;

	geni_capture_stop_time(&msm_port->se, msm_port->ipc_log_kpi, __func__,
			       msm_port->uart_kpi, start_time, 0, 0);
exit_startup:
	if (likely(!uart_console(uport)))
		msm_geni_serial_power_off(&msm_port->uport);
	UART_LOG_DBG(msm_port->ipc_log_misc, uport->dev, "%s: ret:%d\n", __func__, ret);

	return ret;
}

static void geni_serial_write_term_regs(struct uart_port *uport, u32 loopback,
		u32 tx_trans_cfg, u32 tx_parity_cfg, u32 rx_trans_cfg,
		u32 rx_parity_cfg, u32 bits_per_char, u32 stop_bit_len)
{
	struct msm_geni_serial_port *msm_port = GET_DEV_PORT(uport);

	geni_write_reg(loopback, uport->membase, SE_UART_LOOPBACK_CFG);
	geni_write_reg(tx_trans_cfg, uport->membase,
							SE_UART_TX_TRANS_CFG);
	geni_write_reg(tx_parity_cfg, uport->membase,
							SE_UART_TX_PARITY_CFG);
	geni_write_reg(rx_trans_cfg, uport->membase,
							SE_UART_RX_TRANS_CFG);
	geni_write_reg(rx_parity_cfg, uport->membase,
							SE_UART_RX_PARITY_CFG);
	geni_write_reg(bits_per_char, uport->membase,
							SE_UART_TX_WORD_LEN);
	geni_write_reg(bits_per_char, uport->membase,
							SE_UART_RX_WORD_LEN);
	geni_write_reg(stop_bit_len, uport->membase,
						SE_UART_TX_STOP_BIT_LEN);
	geni_se_config_packing(&msm_port->se, bits_per_char, 4, false, true, false);
	geni_se_config_packing(&msm_port->se, bits_per_char, 4, false, false, true);
}

static void msm_geni_serial_termios_cfg(struct uart_port *uport,
				struct ktermios *termios, int clk_div)
{
	u32 bits_per_char = 0;
	u32 stop_bit_len;
	u32 tx_trans_cfg = geni_read_reg(uport->membase,
						SE_UART_TX_TRANS_CFG);
	u32 tx_parity_cfg = geni_read_reg(uport->membase,
						SE_UART_TX_PARITY_CFG);
	u32 rx_trans_cfg = geni_read_reg(uport->membase,
						SE_UART_RX_TRANS_CFG);
	u32 rx_parity_cfg = geni_read_reg(uport->membase,
						SE_UART_RX_PARITY_CFG);
	struct msm_geni_serial_port *port = GET_DEV_PORT(uport);

	if (termios->c_cflag & PARENB) {
		tx_trans_cfg |= UART_TX_PAR_EN;
		rx_trans_cfg |= UART_RX_PAR_EN;
		tx_parity_cfg |= PAR_CALC_EN;
		rx_parity_cfg |= PAR_CALC_EN;

		tx_parity_cfg &= ~PAR_MODE_MSK;
		rx_parity_cfg &= ~PAR_MODE_MSK;

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

	if (port->gsi_mode) {
		setup_config0_tre(uport, bits_per_char, clk_div, stop_bit_len,
				  tx_parity_cfg, ~(tx_trans_cfg & UART_CTS_MASK),
				  rx_parity_cfg, port->loopback);
	} else {
		geni_serial_write_term_regs(uport, port->loopback, tx_trans_cfg,
					    tx_parity_cfg, rx_trans_cfg,
					    rx_parity_cfg, bits_per_char,
					    stop_bit_len);
	}

	if (termios->c_cflag & CRTSCTS) {
		geni_write_reg(0x0, uport->membase, SE_UART_MANUAL_RFR);
		UART_LOG_DBG(port->ipc_log_misc, uport->dev,
			"%s: Manual flow Disabled, HW Flow ON\n", __func__);
	}

	UART_LOG_DBG(port->ipc_log_misc, uport->dev, "Tx: trans_cfg%d parity %d\n",
						tx_trans_cfg, tx_parity_cfg);
	UART_LOG_DBG(port->ipc_log_misc, uport->dev, "Rx: trans_cfg%d parity %d\n",
						rx_trans_cfg, rx_parity_cfg);
	UART_LOG_DBG(port->ipc_log_misc, uport->dev, "BitsChar%d stop bit%d\n",
				bits_per_char, stop_bit_len);

	/* check if MSM CTS line signal is being ignored */
	if (tx_trans_cfg & UART_CTS_MASK)
		UART_LOG_DBG(port->ipc_log_misc, uport->dev,
			     "Check : MSM CTS line signal is being ignored during Tx\n");
}

/*
 * msm_geni_serial_reconfigure_baud_rate() - reconfigure the uart baud rate
 *
 * @uport: pointer to uart port
 *
 * Return: 0 on success else returns a error
 */
static int msm_geni_serial_reconfigure_baud_rate(struct uart_port *uport)
{
	struct msm_geni_serial_port *port = GET_DEV_PORT(uport);
	struct ktermios *termios = port->current_termios;
	int ret;
	unsigned int wait_time = POLL_WAIT_TIMEOUT_MSEC;
	unsigned long long start_time;

	start_time = geni_capture_start_time(&port->se, port->ipc_log_kpi,
					     __func__, port->uart_kpi);
	if (!termios)
		return -EINVAL;

	UART_LOG_DBG(port->ipc_log_misc, uport->dev, "%s: start %d\n", __func__, true);

	if (!uart_console(uport)) {
		ret = msm_geni_serial_power_on(uport);
		if (ret) {
			UART_LOG_DBG(port->ipc_log_misc, uport->dev,
				     "%s: Failed to vote clock on:%d\n", __func__, ret);
			return -EINVAL;
		}
	}

	if (port->tx_wq)
		flush_workqueue(port->tx_wq);

	if (port->rx_wq)
		flush_workqueue(port->rx_wq);

	reinit_completion(&port->xfer);
	msm_geni_serial_stop_rx(uport);

	if (!uart_console(uport)) {
		if (port->xfer_mode == GENI_GPI_DMA)
			wait_time = GSI_STOP_RX_TIMEOUT;
		if (!wait_for_completion_timeout(&port->xfer, msecs_to_jiffies(wait_time)))
			UART_LOG_DBG(port->ipc_log_misc, uport->dev,
				     "%s:Timeout for stop_rx\n", __func__);
	}

	ret = msm_geni_serial_config_baud_rate(uport, termios, port->cur_baud);
	if (!ret)
		UART_LOG_DBG(port->ipc_log_misc, uport->dev,
			     "%s: baud %d\n", __func__, port->cur_baud);

	msm_geni_serial_start_rx(uport);
	if (!uart_console(uport))
		msm_geni_serial_power_off(uport);
	geni_capture_stop_time(&port->se, port->ipc_log_kpi, __func__,
			       port->uart_kpi, start_time, 0, 0);
	return ret;
}

/*
 * msm_geni_serial_config_baud_rate() - Configure the baud rate
 *
 * @uport: pointer to uart port
 * @termios: pointer to termios structure
 * @baud: baud rate which need to be configured
 *
 * Return: 0 on success else returns a error
 */
static int msm_geni_serial_config_baud_rate(struct uart_port *uport,
					    struct ktermios *termios, unsigned int baud)
{
	int clk_div, ret;
	unsigned long ser_clk_cfg = 0;
	struct msm_geni_serial_port *port = GET_DEV_PORT(uport);
	unsigned long clk_rate;
	unsigned long desired_rate;
	unsigned int clk_idx;
	int uart_sampling;
	int clk_freq_diff;
	unsigned long long start_time;

	start_time = geni_capture_start_time(&port->se, port->ipc_log_kpi,
					     __func__, port->uart_kpi);
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
	ret = geni_se_clk_freq_match(&port->se, desired_rate,
			&clk_idx, &clk_rate, false);
	if (ret) {
		dev_err(uport->dev, "%s: Failed(%d) to find src clk for 0x%x\n",
				__func__, ret, baud);
		msm_geni_update_uart_error_code(port, UART_ERROR_SE_CLK_RATE_FIND_FAIL);
		return -EINVAL;
	}

	clk_div = DIV_ROUND_UP(clk_rate, desired_rate);
	if (clk_div <= 0)
		return -EINVAL;

	clk_freq_diff =  (desired_rate - (clk_rate / clk_div));
	if (clk_freq_diff)
		UART_LOG_DBG(port->ipc_log_misc, uport->dev,
			     "%s src_clk freq_diff:%d baud:%d clk_rate:%lu clk_div:%d\n",
			     __func__, clk_freq_diff, baud, clk_rate, clk_div);

	uport->uartclk = clk_rate;
	ret = clk_set_rate(port->serial_rsc.se_clk, clk_rate);
	if (ret) {
		dev_err(uport->dev, "Error setting clock rate\n");
		UART_LOG_DBG(port->ipc_log_misc, uport->dev,
			     "%s: SE clock set_rate error:%d\n", __func__, ret);
	}
	ser_clk_cfg |= SER_CLK_EN;
	ser_clk_cfg |= (clk_div << CLK_DIV_SHFT);

	if (likely(baud))
		uart_update_timeout(uport, termios->c_cflag, baud);

	port->ser_clk_cfg = ser_clk_cfg;
	msm_geni_enable_disable_se_clk(uport, true);

	msm_geni_serial_termios_cfg(uport, termios, clk_div);
	geni_capture_stop_time(&port->se, port->ipc_log_kpi, __func__,
			       port->uart_kpi, start_time, 0, 0);
	return 0;
}

static void msm_geni_serial_set_termios(struct uart_port *uport,
					struct ktermios *termios, const struct ktermios *old)
{
	unsigned int baud;
	struct msm_geni_serial_port *port = GET_DEV_PORT(uport);
	unsigned int poll_wait_time = POLL_WAIT_TIMEOUT_MSEC;
	unsigned long long start_time;

	start_time = geni_capture_start_time(&port->se, port->ipc_log_kpi,
					     __func__, port->uart_kpi);
	/* QUP_2.5.0 and older RUMI has sampling rate as 32 */
	if (IS_ENABLED(CONFIG_SERIAL_MSM_GENI_HALF_SAMPLING) &&
	    port->rumi_platform && port->is_console) {
		geni_write_reg(0x21, uport->membase, GENI_SER_M_CLK_CFG);
		geni_write_reg(0x21, uport->membase, GENI_SER_S_CLK_CFG);
		geni_read_reg(uport->membase, GENI_SER_M_CLK_CFG);
	}

	if (!uart_console(uport)) {
		int ret;

		ret = msm_geni_serial_power_on(uport);
		if (ret) {
			UART_LOG_DBG(port->ipc_log_misc, uport->dev,
				     "%s: Failed to vote clock on:%d\n",
				     __func__, ret);
			return;
		}
	}

	if (port->tx_wq)
		flush_workqueue(port->tx_wq);

	if (port->rx_wq)
		flush_workqueue(port->rx_wq);

	reinit_completion(&port->xfer);
	msm_geni_serial_stop_rx(uport);

	if (!uart_console(uport)) {
		if (port->xfer_mode == GENI_GPI_DMA)
			poll_wait_time = GSI_STOP_RX_TIMEOUT;
		if (!wait_for_completion_timeout(&port->xfer, msecs_to_jiffies(poll_wait_time)))
			UART_LOG_DBG(port->ipc_log_misc, uport->dev,
				     "%s:Timeout for stop_rx\n", __func__);
	}

	/* baud rate */
	baud = uart_get_baud_rate(uport, termios, old,
				  MIN_SUPPORTED_BAUD_RATE, MAX_SUPPORTED_BAUD_RATE);
	port->cur_baud = baud;
	if (msm_geni_serial_config_baud_rate(uport, termios, baud))
		goto exit_set_termios;

	if (!uart_console(uport))
		port->current_termios = termios;

	UART_LOG_DBG(port->ipc_log_misc, uport->dev,
		     "%s: baud %d\n", __func__, baud);
	geni_capture_stop_time(&port->se, port->ipc_log_kpi, __func__,
			       port->uart_kpi, start_time, 0, 0);
exit_set_termios:
	msm_geni_serial_start_rx(uport);
	if (!uart_console(uport))
		msm_geni_serial_power_off(uport);
}

static unsigned int msm_geni_serial_tx_empty(struct uart_port *uport)
{
	unsigned int tx_fifo_status;
	unsigned int is_tx_empty = 1;
	struct msm_geni_serial_port *port = GET_DEV_PORT(uport);

	if (!uart_console(uport) && !pm_runtime_active(uport->dev))
		return 1;

	if (port->xfer_mode == GENI_SE_DMA)
		tx_fifo_status = port->tx_dma ? 1 : 0;
	else
		tx_fifo_status = geni_read_reg(uport->membase,
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

	if (port->xfer_mode == GENI_SE_FIFO)
		ret = scnprintf(buf, sizeof("FIFO\n"), "FIFO\n");
	else if (port->xfer_mode == GENI_SE_DMA)
		ret = scnprintf(buf, sizeof("SE_DMA\n"), "SE_DMA\n");

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
		xfer_mode = GENI_SE_FIFO;
	} else if (strnstr(buf, "SE_DMA", strlen("SE_DMA"))) {
		xfer_mode = GENI_SE_DMA;
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
	geni_se_select_mode(&port->se, port->xfer_mode);
	spin_unlock_irqrestore(&uport->lock, flags);
	msm_geni_serial_start_rx(uport);
	msm_geni_serial_power_off(uport);

	return size;
}

static DEVICE_ATTR_RW(xfer_mode);

/**
 * one_wire_uart_error_show - Show the one-wire UART error value
 * @dev: The device associated with the platform device
 * @attr: The device attribute
 * @buf: Buffer to store the error value
 *
 * Return: The number of characters written to the buffer
 */

static ssize_t one_wire_uart_error_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct msm_geni_serial_port *port = platform_get_drvdata(pdev);
	int ret;

	ret = scnprintf(buf, sizeof(int), "%d\n", port->one_wire_uart.error);
	port->one_wire_uart.error = 0;

	return ret;
}

static DEVICE_ATTR_RO(one_wire_uart_error);

static ssize_t ver_info_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct msm_geni_serial_port *port = platform_get_drvdata(pdev);
	ssize_t ret = 0;
	int len = (sizeof(struct msm_geni_serial_ver_info) * 2);

	ret = scnprintf(buf, len, "FW ver=0x%x%x, HW ver=%d.%d.%d\n",
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

	if (msm_geni_serial_resources_on(dev_port))
		WARN_ON(1);

	if (unlikely(get_se_proto(uport->membase) != GENI_SE_UART)) {
		msm_geni_serial_resources_off(dev_por);
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

	if (new_state == UART_PM_STATE_ON && old_state == UART_PM_STATE_OFF) {
		msm_geni_serial_resources_on(msm_port);
		msm_geni_enable_disable_se_clk(uport, true);
		atomic_set(&msm_port->is_clock_off, 0);
	} else if (new_state == UART_PM_STATE_OFF &&
			old_state == UART_PM_STATE_ON) {
		atomic_set(&msm_port->is_clock_off, 1);
		msm_geni_enable_disable_se_clk(uport, false);
		msm_geni_serial_resources_off(msm_port);
	}
}

static void msm_geni_serial_hs_pm(struct uart_port *uport,
		unsigned int new_state, unsigned int old_state)
{
	struct msm_geni_serial_port *msm_port = GET_DEV_PORT(uport);

	/*
	 * This will get call for system suspend/resume and
	 * Applicable for hs-uart without runtime pm framework support.
	 */
	if (pm_runtime_enabled(uport->dev))
		return;

	/*
	 * Default PM State is UNDEFINED Setting it to OFF State.
	 * This will allow add one port to do resources on and off during probe
	 */
	if (old_state == UART_PM_STATE_UNDEFINED)
		old_state = UART_PM_STATE_OFF;
	if (new_state == UART_PM_STATE_ON && old_state == UART_PM_STATE_OFF) {
		msm_geni_serial_resources_on(msm_port);
		msm_geni_enable_disable_se_clk(uport, true);
		atomic_set(&msm_port->is_clock_off, 0);
	} else if (new_state == UART_PM_STATE_OFF &&
			old_state == UART_PM_STATE_ON) {
		atomic_set(&msm_port->is_clock_off, 1);
		msm_geni_enable_disable_se_clk(uport, false);
		msm_geni_serial_resources_off(msm_port);
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
	/* For HSUART nodes without IOCTL support */
	.pm = msm_geni_serial_hs_pm,
};

static const struct of_device_id msm_geni_device_tbl[] = {
#if IS_ENABLED(CONFIG_SERIAL_MSM_GENI_CONSOLE) || \
						IS_ENABLED(CONFIG_CONSOLE_POLL)
	{ .compatible = "qcom,msm-geni-console",
			.data = (void *)&msm_geni_console_driver},
#endif
	{ .compatible = "qcom,msm-geni-serial-hs",
			.data = (void *)&msm_geni_serial_hs_driver},
	{ .compatible = "qcom,msm-geni-one-wire-serial",
			.data = (void *)&msm_geni_serial_hs_driver},
	{}
};
MODULE_DEVICE_TABLE(of, msm_geni_device_tbl);

static void msm_geni_serial_init_gsi(struct uart_port *uport)
{
	struct msm_geni_serial_port *msm_port = GET_DEV_PORT(uport);
	int ret;

	ret = geni_icc_enable(&msm_port->se);
	if (ret) {
		UART_LOG_DBG(msm_port->ipc_log_misc, msm_port->uport.dev,
			     "%s: Error %d geni_icc_enable failed\n", __func__, ret);
		return;
	}

	ret = geni_icc_set_bw(&msm_port->se);
	if (ret) {
		UART_LOG_DBG(msm_port->ipc_log_misc, msm_port->uport.dev,
			     "%s: Error %d ICC BW voting failed\n", __func__, ret);
		return;
	}
	msm_port->gsi_mode = geni_read_reg(uport->membase,
					   GENI_IF_DISABLE_RO) & FIFO_IF_DISABLE;
	ret = geni_icc_disable(&msm_port->se);
	if (ret) {
		UART_LOG_DBG(msm_port->ipc_log_misc, msm_port->uport.dev,
			"%s: Error %d geni_icc_disable failed\n", __func__, ret);
		return;
	}

	dev_info(uport->dev, "gsi_mode:%d\n", msm_port->gsi_mode);
	if (msm_port->gsi_mode) {
		msm_port->gsi = devm_kzalloc(uport->dev, sizeof(*msm_port->gsi),
					     GFP_KERNEL);
		msm_port->xfer_mode = GENI_GPI_DMA;
		msm_port->tx_wq = alloc_workqueue("%s", WQ_UNBOUND | WQ_HIGHPRI, 1,
						  dev_name(uport->dev));
		msm_port->rx_wq = alloc_workqueue("%s", WQ_UNBOUND | WQ_HIGHPRI, 1,
						  dev_name(uport->dev));
		INIT_WORK(&msm_port->tx_xfer_work, msm_geni_uart_gsi_xfer_tx);
		INIT_WORK(&msm_port->rx_cancel_work,
			  msm_geni_uart_gsi_cancel_rx);
		INIT_WORK(&msm_port->tx_cancel_work,
			  msm_geni_uart_gsi_cancel_tx);

		atomic_set(&msm_port->gsi_rx_done, 0);
	}
}

static int msm_geni_se_clks_on_off(struct msm_geni_serial_port *msm_port, bool clk_on)
{
	int ret = 0;

	if (clk_on) {
		ret = geni_icc_enable(&msm_port->se);
		if (ret) {
			UART_LOG_DBG(msm_port->ipc_log_misc, msm_port->uport.dev,
				"%s: Error %d geni_icc_enable failed\n", __func__, ret);
			return ret;
		}

		ret = geni_icc_set_bw(&msm_port->se);
		if (ret) {
			UART_LOG_DBG(msm_port->ipc_log_misc, msm_port->uport.dev,
				"%s: Error %d ICC BW voting failed\n", __func__, ret);
			return ret;
		}

		geni_se_common_clks_on(msm_port->serial_rsc.se_clk,
			msm_port->serial_rsc.m_ahb_clk, msm_port->serial_rsc.s_ahb_clk);
		msm_geni_enable_disable_se_clk(&msm_port->uport, true);
	} else {
		msm_geni_enable_disable_se_clk(&msm_port->uport, false);
		geni_se_common_clks_off(msm_port->serial_rsc.se_clk,
					msm_port->serial_rsc.m_ahb_clk,
					msm_port->serial_rsc.s_ahb_clk);

		ret = geni_icc_disable(&msm_port->se);
		if (ret)
			UART_LOG_DBG(msm_port->ipc_log_misc, msm_port->uport.dev,
				     "%s: Error %d geni_icc_disable failed\n", __func__, ret);
	}
	return ret;
}

static int msm_geni_serial_get_ver_info(struct uart_port *uport)
{
	u32 hw_ver;
	int ret = 0;
	struct msm_geni_serial_port *msm_port = GET_DEV_PORT(uport);
	int len = (sizeof(struct msm_geni_serial_ver_info) * 2);
	char fwver[MAX_LEN];
	u32 se_hw_param2 = 0;

	/* clks_on/off only for HSUART, as console remains actve */
	if (!msm_port->is_console) {
		/* By default Enable clk divider value */
		msm_port->ser_clk_cfg = 0x21;
		msm_geni_se_clks_on_off(msm_port, true);
	}

	/* Basic HW and FW info */
	if (msm_port->one_wire_uart.enable) {
		se_hw_param2 = geni_read_reg(uport->membase, SE_HW_PARAM_2);
		if (!(se_hw_param2 & UART_SINGLE_WIRE_SUPPORTED)) {
			dev_err(uport->dev, "%s: SE doesn't support one wire uart mode\n",
				__func__);
			ret = -EINVAL;
			goto exit_ver_info;
		}
	}

	if (unlikely(geni_se_common_get_proto(uport->membase) != GENI_SE_UART)) {
		dev_err(uport->dev, "%s: Invalid FW %d loaded.\n",
			 __func__, geni_se_common_get_proto(uport->membase));
		ret = -ENXIO;
		goto exit_ver_info;
	}

	msm_port->serial_rsc.proto = GENI_SE_UART;
	msm_port->ver_info.m_fw_ver = geni_se_common_get_m_fw(uport->membase);
	msm_port->ver_info.s_fw_ver = geni_se_common_get_s_fw(uport->membase);
	scnprintf(fwver, len, "FW Ver:0x%x%x", msm_port->ver_info.m_fw_ver,
		  msm_port->ver_info.s_fw_ver);

	hw_ver = geni_se_get_qup_hw_version(&msm_port->se);
	UART_LOG_DBG(msm_port->ipc_log_misc, uport->dev,
		     "%s: FW Ver: %s HW Ver: 0x%x\n", __func__, fwver, hw_ver);

	geni_se_common_get_major_minor_num(hw_ver,
		&msm_port->ver_info.hw_major_ver,
		&msm_port->ver_info.hw_minor_ver,
		&msm_port->ver_info.hw_step_ver);

	msm_geni_serial_enable_interrupts(uport);

exit_ver_info:
	if (!msm_port->is_console)
		msm_geni_se_clks_on_off(msm_port, false);

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
		dev_port->wakeup_irq_wq = alloc_workqueue("%s", WQ_UNBOUND | WQ_HIGHPRI, 1,
							  dev_name(uport->dev));
		if (!dev_port->wakeup_irq_wq) {
			dev_err(uport->dev, "%s:WQ alloc failed for Wakeup IRQ\n",
					__func__);
			return -ENOMEM;
		}
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
	struct device *dev = &pdev->dev;

	dev_port->serial_rsc.se_clk = devm_clk_get(&pdev->dev, "se-clk");
	if (IS_ERR(dev_port->serial_rsc.se_clk)) {
		ret = PTR_ERR(dev_port->serial_rsc.se_clk);
		dev_err(&pdev->dev, "Err getting SE Core clk %d\n", ret);
		msm_geni_update_uart_error_code(dev_port, UART_ERROR_CLK_GET_FAIL);
		return ret;
	}

	dev_port->serial_rsc.m_ahb_clk = devm_clk_get(dev->parent, "m-ahb");
	if (IS_ERR(dev_port->serial_rsc.m_ahb_clk)) {
		ret = PTR_ERR(dev_port->serial_rsc.m_ahb_clk);
		dev_err(&pdev->dev, "Err getting M AHB clk %d\n", ret);
		msm_geni_update_uart_error_code(dev_port, UART_ERROR_CLK_GET_FAIL);
		return ret;
	}

	dev_port->serial_rsc.s_ahb_clk = devm_clk_get(dev->parent, "s-ahb");
	if (IS_ERR(dev_port->serial_rsc.s_ahb_clk)) {
		ret = PTR_ERR(dev_port->serial_rsc.s_ahb_clk);
		dev_err(&pdev->dev, "Err getting S AHB clk %d\n", ret);
		msm_geni_update_uart_error_code(dev_port, UART_ERROR_CLK_GET_FAIL);
		return ret;
	}

	return ret;
}

/**
 * geni_se_handle_common_resources: Load common resources.
 * @pdev: structure to platform driver.
 * @se_rsc: structure to geni_se.
 *
 * This function will read the clock vote values from
 * the DTSI file and configure and initialize the resources
 * accordingly. If the resources are not explicitly mentioned
 * in the DTSI, they will be initialized with default values.
 *
 * return: 0 on success else error code on failure.
 */
static int geni_se_handle_common_resources(struct platform_device *pdev, struct geni_se *se_rsc)
{
	int ret;

	ret = geni_se_get_common_resources(pdev, se_rsc);
	if (ret) {
		/* Error in loading vote values from DTS, try loading default vote values */
		ret = geni_se_common_resources_init(se_rsc, UART_CORE2X_VOTE,
						    APPS_PROC_TO_QUP_VOTE,
						    DEFAULT_SE_CLK * DEFAULT_BUS_WIDTH);
	}
	return ret;
}

/*
 * msm_geni_one_wire_serial_read_dtsi() - Read one wire uart dt properties
 *
 * @pdev: pointer to the platform device
 * @dev_port: pointer to the msm geni serial port
 *
 * Return: None
 */
static void msm_geni_one_wire_serial_read_dtsi(struct platform_device *pdev,
					       struct msm_geni_serial_port *dev_port)
{
	if (of_device_is_compatible(pdev->dev.of_node, "qcom,msm-geni-one-wire-serial")) {
		dev_port->one_wire_uart.enable = true;
		/* Set default Lane as L2 */
		dev_port->one_wire_uart.qup_lane = QUP_L2_LANE;
		if (!of_property_read_u32(pdev->dev.of_node, "qcom,qup-lane",
					  &dev_port->one_wire_uart.qup_lane)) {
			if (dev_port->one_wire_uart.qup_lane != QUP_L2_LANE &&
			    dev_port->one_wire_uart.qup_lane != QUP_L3_LANE)
				dev_port->one_wire_uart.qup_lane = QUP_L2_LANE;
		}
		dev_dbg(&pdev->dev, "Lane:0x%x used for one wire uart usecase\n",
			dev_port->one_wire_uart.qup_lane);
		dev_port->one_wire_uart.output_mode =
			of_property_read_bool(pdev->dev.of_node, "qcom,push-pull");
	}
}

static int msm_geni_serial_read_dtsi(struct platform_device *pdev,
					struct msm_geni_serial_port *dev_port)
{
	int ret = 0;
	struct uart_port *uport = &dev_port->uport;
	struct resource *res;
	bool is_console = dev_port->is_console;
	u32 wake_char = 0;

	dev_port->wrapper_dev = pdev->dev.parent;
	dev_port->serial_rsc.wrapper_dev = pdev->dev.parent;
	dev_port->serial_rsc.ctrl_dev = &pdev->dev;

	dev_port->se.dev = &pdev->dev;
	dev_port->se.wrapper = dev_get_drvdata(pdev->dev.parent);
	if (!dev_port->se.wrapper) {
		dev_err(&pdev->dev, "SE Wrapper is NULL, deferring probe\n");
		return -EPROBE_DEFER;
	}
	dev_port->se.clk = devm_clk_get(&pdev->dev, "se-clk");

	if (IS_ERR(dev_port->se.clk)) {
		ret = PTR_ERR(dev_port->se.clk);
		dev_err(&pdev->dev, "Err getting SE Core clk %d\n", ret);
		return ret;
	}

	msm_geni_one_wire_serial_read_dtsi(pdev, dev_port);

	if (!is_console)
		ret = geni_se_handle_common_resources(pdev, &dev_port->se);
	else
		ret = geni_se_common_resources_init(&dev_port->se, GENI_DEFAULT_BW,
						    GENI_DEFAULT_BW, GENI_DEFAULT_BW);
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
	dev_port->se.base = uport->membase;

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

	if (of_property_read_bool(pdev->dev.of_node, "qcom,split-tx-dma-tre")) {
		dev_port->split_dma_tre.split_tx = true;
		dev_dbg(&pdev->dev, "UART split Tx support is enabled\n");
	}

	return ret;
}

/*
 * msm_geni_check_stop_engine() - Check GENI status and stop the
 * primary/secondary sequencer if it is active.
 *
 * @uport: pointer to uart port
 *
 * Return: None
 */
static void msm_geni_check_stop_engine(struct uart_port *uport)
{
	struct msm_geni_serial_port *port = GET_DEV_PORT(uport);
	unsigned int geni_status, timeout = 0, is_irq_masked;

	geni_status = readl_relaxed(uport->membase + SE_GENI_STATUS);
	if (geni_status & M_GENI_CMD_ACTIVE) {
		port->m_cmd_done = false;
		port->m_cmd = true;
		reinit_completion(&port->m_cmd_timeout);
		is_irq_masked = msm_serial_try_disable_interrupts(uport);
		geni_se_cancel_m_cmd(&port->se);

		timeout = geni_wait_for_cmd_done(uport, is_irq_masked);
		if (timeout) {
			UART_LOG_DBG(port->ipc_log_misc, uport->dev,
				     "%s: TX Cancel cmd failed, geni_status:0x%x\n",
				     __func__, geni_read_reg(uport->membase, SE_GENI_STATUS));
			port->m_cmd_done = false;
			reinit_completion(&port->m_cmd_timeout);
			/* Give abort command as cancel command failed */
			geni_se_abort_m_cmd(&port->se);

			timeout = geni_wait_for_cmd_done(uport, is_irq_masked);
			if (timeout) {
				UART_LOG_DBG(port->ipc_log_misc, uport->dev,
					     "%s: TX abort cmd failed, geni_status:0x%x\n",
					     __func__, geni_read_reg(uport->membase,
					     SE_GENI_STATUS));
			} else {
				UART_LOG_DBG(port->ipc_log_misc, uport->dev,
					     "%s: TX abort cmd done\n", __func__);
			}
		} else {
			UART_LOG_DBG(port->ipc_log_misc, uport->dev,
				     "%s: TX Cancel cmd done\n", __func__);
		}
	} else if (geni_status & S_GENI_CMD_ACTIVE) {
		port->s_cmd_done = false;
		port->s_cmd = true;
		reinit_completion(&port->s_cmd_timeout);
		is_irq_masked = msm_serial_try_disable_interrupts(uport);
		geni_se_cancel_s_cmd(&port->se);

		timeout = geni_wait_for_cmd_done(uport, is_irq_masked);
		if (timeout) {
			UART_LOG_DBG(port->ipc_log_misc, uport->dev,
				     "%s: RX Cancel cmd failed, geni_status:0x%x\n",
				     __func__, geni_read_reg(uport->membase, SE_GENI_STATUS));
			port->s_cmd_done = false;
			reinit_completion(&port->s_cmd_timeout);
			/* Give abort command as cancel command failed */
			geni_se_abort_s_cmd(&port->se);

			timeout = geni_wait_for_cmd_done(uport, is_irq_masked);
			if (timeout) {
				UART_LOG_DBG(port->ipc_log_misc, uport->dev,
					     "%s: RX abort cmd failed, geni_status:0x%x\n",
					    __func__, geni_read_reg(uport->membase,
					    SE_GENI_STATUS));
			} else {
				UART_LOG_DBG(port->ipc_log_misc, uport->dev,
					     "%s: RX abort cmd done\n", __func__);
			}
		} else {
			UART_LOG_DBG(port->ipc_log_misc, uport->dev,
				     "%s: RX cancel cmd done\n", __func__);
		}
	}
}

static int msm_geni_serial_port_init(struct platform_device *pdev,
				     struct msm_geni_serial_port *dev_port)
{
	int ret = 0;
	struct uart_port *uport = &dev_port->uport;

	ret = msm_geni_serial_get_ver_info(uport);
	if (ret) {
		dev_err(&pdev->dev, "Failed to Read FW ver: %d\n", ret);
		return ret;
	}

	dev_port->tx_fifo_depth = DEF_FIFO_DEPTH_WORDS;
	dev_port->rx_fifo_depth = DEF_FIFO_DEPTH_WORDS;
	dev_port->tx_fifo_width = DEF_FIFO_WIDTH_BITS;
	uport->fifosize =
		((dev_port->tx_fifo_depth * dev_port->tx_fifo_width) >> 3);
	/* Complete signals to handle cancel cmd completion */
	init_completion(&dev_port->m_cmd_timeout);
	init_completion(&dev_port->s_cmd_timeout);
	init_completion(&dev_port->xfer);
	init_completion(&dev_port->tx_xfer);

	init_completion(&dev_port->wakeup_comp);
	platform_set_drvdata(pdev, dev_port);

	/*
	 * To Disable PM runtime API that will make ioctl based
	 * vote_clock_on/off optional and rely on system PM
	 */
	dev_port->pm_auto_suspend_disable =
		of_property_read_bool(pdev->dev.of_node, "qcom,auto-suspend-disable");

	if (dev_port->is_console) {
		dev_port->handle_rx = handle_rx_console;
		dev_port->rx_fifo = devm_kzalloc(uport->dev, sizeof(u32), GFP_KERNEL);
		if (!dev_port->rx_fifo)
			return -ENOMEM;
	} else {
		dev_port->handle_rx = handle_rx_hs;
		dev_port->rx_fifo = devm_kzalloc(uport->dev, (dev_port->rx_fifo_depth *
						 sizeof(u32)), GFP_KERNEL);
		if (!dev_port->rx_fifo)
			return -ENOMEM;
	}

	if (IS_ENABLED(CONFIG_SERIAL_MSM_GENI_HALF_SAMPLING) &&
		dev_port->rumi_platform && dev_port->is_console) {
		/* No ver info available, if do later then RUMI console fails */
		geni_write_reg(0x21, uport->membase, GENI_SER_M_CLK_CFG);
		geni_write_reg(0x21, uport->membase, GENI_SER_S_CLK_CFG);
		geni_read_reg(uport->membase, GENI_SER_M_CLK_CFG);
	}

	device_create_file(uport->dev, &dev_attr_loopback);
	device_create_file(uport->dev, &dev_attr_xfer_mode);
	device_create_file(uport->dev, &dev_attr_ver_info);
	device_create_file(uport->dev, &dev_attr_hs_uart_operation);
	device_create_file(uport->dev, &dev_attr_hs_uart_version);
	device_create_file(uport->dev, &dev_attr_capture_kpi);
	device_create_file(uport->dev, &dev_attr_one_wire_uart_error);

	return ret;
}

static int msm_geni_serial_probe(struct platform_device *pdev)
{
	int ret = 0;
	int line, i = 0;
	struct msm_geni_serial_port *dev_port;
	struct uart_port *uport;
	struct uart_driver *drv;
	const struct of_device_id *id;
	bool is_console = false;
	unsigned char prev_line_id;

	id = of_match_device(msm_geni_device_tbl, &pdev->dev);
	if (!id) {
		dev_err(&pdev->dev, "%s: No matching device found\n",
				__func__);
		return -ENODEV;
	}
	dev_dbg(&pdev->dev, "%s: %s\n", __func__, id->compatible);
	drv = (struct uart_driver *)id->data;

	prev_line_id = uart_line_id;
	if (pdev->dev.of_node) {
		if (drv->cons) {
			line = of_alias_get_id(pdev->dev.of_node, "serial");
			if (line < 0)
				line = 0;
		} else {
			if (uart_line_id >= (1 << GENI_UART_NR_PORTS) - 1) {
				dev_err(&pdev->dev,
					"All UART ports already initialized\n");
				return -ENODEV;
			}

			line = of_alias_get_id(pdev->dev.of_node, "hsuart");
			if (line < 0) {
				for (i = (GENI_UART_NR_PORTS - 1); i >= 0; i--) {
					if ((uart_line_id  & (1 << i)) == 0) {
						line = i;
						break;
					}
				}
			}

			if (uart_line_id & (1 << line)) {
				dev_err(&pdev->dev, "Already used line %d\n", line);
				return -ENODEV;
			}

			if (line >= 0 && line < GENI_UART_NR_PORTS)
				uart_line_id |= (1 << line) & 0xFF;
		}
	} else {
		line = pdev->id;
	}

	if (drv->cons)
		pr_info("boot_kpi: M - DRIVER GENI_CONSOLE_%d Init\n", line);
	else
		pr_info("boot_kpi: M - DRIVER GENI_HS_UART_%d Init\n", line);

	is_console = (drv->cons ? true : false);
	dev_port = get_port_from_line(line, is_console);
	if (IS_ERR_OR_NULL(dev_port)) {
		ret = PTR_ERR(dev_port);
		dev_err(&pdev->dev, "Invalid line %d(%d)\n", line, ret);
		goto exit_geni_serial_probe;
	}

	dev_port->port_state = UART_PORT_CLOSED_SHUTDOWN;
	dev_port->is_console = is_console;
	if (drv->cons && !con_enabled) {
		dev_err(&pdev->dev, "%s, Console Disabled\n", __func__);
		ret = pinctrl_pm_select_sleep_state(&pdev->dev);
		if (ret)
			dev_err(&pdev->dev,
				"failed to set pinctrl state to sleep %d\n", ret);
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
	uport->private_data = (void *)drv;

	ret = msm_geni_serial_read_dtsi(pdev, dev_port);
	if (ret)
		goto exit_geni_serial_probe;

	msm_geni_serial_debug_init(uport, is_console);
	ret = msm_geni_serial_port_init(pdev, dev_port);
	if (ret)
		goto exit_geni_serial_probe;

	dev_info(&pdev->dev, "Serial port: %d added.FifoSize: %d is_console: %d\n",
		 line, uport->fifosize, is_console);

	dev_port->port_setup = false;

	dev_port->uart_error = UART_ERROR_DEFAULT;
	/* Initialize the GSI mode */
	msm_geni_serial_init_gsi(uport);

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

	if (!dev_port->is_console) {
		msm_geni_se_clks_on_off(dev_port, true);
		msm_geni_check_stop_engine(uport);
		msm_geni_se_clks_on_off(dev_port, false);

		if (dev_port->pm_auto_suspend_disable) {
			pm_runtime_set_active(&pdev->dev);
			pm_runtime_forbid(&pdev->dev);
		} else {
			pm_runtime_set_suspended(&pdev->dev);
			pm_runtime_set_autosuspend_delay(&pdev->dev, 150);
			pm_runtime_use_autosuspend(&pdev->dev);
			pm_runtime_enable(&pdev->dev);
		}
	}

	/* Ignore dependencies on children by runtime PM framework */
	if (of_property_read_bool(pdev->dev.of_node, "qcom,suspend-ignore-children"))
		pm_suspend_ignore_children(uport->dev, true);

	if (is_console)
		pr_info("boot_kpi: M - DRIVER GENI_CONSOLE_%d Ready\n", line);
	else
		pr_info("boot_kpi: M - DRIVER GENI_HS_UART_%d Ready\n", line);

exit_geni_serial_probe:
	if (ret)
		uart_line_id = prev_line_id;
	UART_LOG_DBG(dev_port->ipc_log_misc, &pdev->dev, "%s: ret:%d\n",
		__func__, ret);
	return ret;
}

static void msm_geni_serial_remove(struct platform_device *pdev)
{
	struct msm_geni_serial_port *port = platform_get_drvdata(pdev);
	struct uart_driver *drv =
			(struct uart_driver *)port->uport.private_data;

	/* Platform driver is registered for console and when console
	 * is disabled from cmdline simply return success.
	 */
	if (port->is_console && !con_enabled)
		return;
	if (port->wakeup_irq > 0)
		destroy_workqueue(port->wakeup_irq_wq);
	if (!uart_console(&port->uport)) {
		wakeup_source_unregister(port->geni_wake);
		port->geni_wake = NULL;
	}
	if (port->pm_auto_suspend_disable)
		pm_runtime_allow(&pdev->dev);
	uart_remove_one_port(drv, &port->uport);

	if (port->gsi_mode) {
		/* Cancel specific works and wait for any running instance to finish */
		cancel_work_sync(&port->tx_xfer_work);
		cancel_work_sync(&port->tx_cancel_work);
		cancel_work_sync(&port->rx_cancel_work);

		if (port->gsi && port->gsi->tx_c)
			dmaengine_pause(port->gsi->tx_c);

		if (port->tx_wq) {
			destroy_workqueue(port->tx_wq);
			port->tx_wq = NULL;
		}
		if (port->rx_wq) {
			destroy_workqueue(port->rx_wq);
			port->rx_wq = NULL;
		}
		msm_geni_serial_cleanup_timestamp_resources(port);
		msm_geni_deallocate_chan(&port->uport);
	}
	if (port->rx_dma) {
		geni_se_common_iommu_free_buf(port->wrapper_dev, &port->rx_dma,
					port->rx_buf, DMA_RX_BUF_SIZE);
		port->rx_dma = (dma_addr_t)NULL;
		port->rx_buf = NULL;
	}
	if (port->split_dma_tre.split_tx)
		msm_geni_serial_free_aligned_dma_buffers(port);

	device_remove_file(port->uport.dev, &dev_attr_hs_uart_version);
	device_remove_file(port->uport.dev, &dev_attr_hs_uart_operation);
	device_remove_file(port->uport.dev, &dev_attr_loopback);
	device_remove_file(port->uport.dev, &dev_attr_xfer_mode);
	device_remove_file(port->uport.dev, &dev_attr_ver_info);
	device_remove_file(port->uport.dev, &dev_attr_capture_kpi);
	device_remove_file(port->uport.dev, &dev_attr_one_wire_uart_error);
	debugfs_remove(port->dbg);

	dev_dbg(&pdev->dev, "%s: driver removed\n", __func__);
}

/*
 * msm_geni_serial_driver_shutdown() - shutdown callback function for uart
 * This function will be called as part of device reboot or shutdown
 *
 * @pdev: pointer to platform device
 *
 * Return: None
 */
static void msm_geni_serial_driver_shutdown(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "%s called %d\n", __func__, true);
	msm_geni_serial_remove(pdev);
}

static void msm_geni_serial_allow_rx(struct msm_geni_serial_port *port)
{
	u32 uart_manual_rfr;

	uart_manual_rfr = (UART_MANUAL_RFR_EN | UART_RFR_READY);
	geni_write_reg(uart_manual_rfr, port->uport.membase,
						SE_UART_MANUAL_RFR);
	/* Ensure that the manual flow off writes go through */
	mb();
	uart_manual_rfr = geni_read_reg(port->uport.membase,
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
	int ret = 0, count = 0;
	unsigned long long start_time;
	u32 geni_status = geni_read_reg(port->uport.membase,
							SE_GENI_STATUS);

	UART_LOG_DBG(port->ipc_log_pwr, dev,
		"%s: Start geni_status : 0x%x\n", __func__, geni_status);

	start_time = geni_capture_start_time(&port->se, port->ipc_log_kpi,
					     __func__, port->uart_kpi);

	/* Flow off from UART only for In band sleep(IBS)
	 * Avoid manual RFR FLOW ON for Out of band sleep(OBS).
	 */
	if (port->wakeup_byte && port->wakeup_irq)
		msm_geni_serial_set_manual_flow(false, port);
	/* If shutdown is in progress stop rx sequencer and
	 * disable the clocks, don't check for wakeup byte.
	 */
	if (port->port_state == UART_PORT_OPEN) {
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
			ret = -EBUSY;
			goto exit_runtime_suspend;
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
		ret = -EBUSY;
		goto exit_runtime_suspend;
	}

	if (port->xfer_mode == GENI_GPI_DMA &&
	    !wait_for_completion_timeout(&port->xfer, msecs_to_jiffies(GSI_STOP_RX_TIMEOUT))) {
		UART_LOG_DBG(port->ipc_log_pwr, dev,
			     "%s: timeout for gsi cancel rx\n", __func__);
		return -ETIMEDOUT;
	}

	geni_status = geni_read_reg(port->uport.membase, SE_GENI_STATUS);
	if ((geni_status & M_GENI_CMD_ACTIVE))
		stop_tx_sequencer(&port->uport);

	disable_irq(port->uport.irq);

	/*
	 * Flow on from UART only for In band sleep(IBS)
	 * Avoid manual RFR FLOW ON for Out of band sleep(OBS).
	 * Above before stop_rx disabled the flow so we need to enable it here
	 * Make sure wake up interrupt is enabled before RFR is made low
	 */
	if (port->wakeup_byte && port->wakeup_irq && port->port_state == UART_PORT_OPEN)
		msm_geni_serial_allow_rx(port);

	/*
	 * stop_rx_sequencer can be invoked by framework via msm_geni_serial_stop_rx
	 * independently. Before disabling clocks wait for stop_rx_sequencer to
	 * complete to avoid unclocked register access
	 */
	while (atomic_read(&port->stop_rx_inprogress)) {
		mdelay(10);
		/* Poll for 100msecs */
		if (++count > 10) {
			/* Bailout since stop_rx_sequencer is still in progress */
			UART_LOG_DBG(port->ipc_log_pwr, dev,
				     "%s: return, stop_rx_seq busy\n", __func__);
			enable_irq(port->uport.irq);
			ret = -EBUSY;
			goto exit_runtime_suspend;
		}
	}
	if (count)
		UART_LOG_DBG(port->ipc_log_pwr, dev,
			     "%s: count=%d\n", __func__, count);

	msm_geni_enable_disable_se_clk(&port->uport, false);
	ret = msm_geni_serial_resources_off(port);
	if (ret) {
		dev_err(dev, "%s: Error ret %d\n", __func__, ret);
		msm_geni_update_uart_error_code(port, UART_ERROR_SE_RESOURCES_OFF_FAIL);
		goto exit_runtime_suspend;
	}

	/*
	 * If shutdown is not in progress, check if port
	 * is in open state before enabling wakeup_irq
	 */
	if (port->port_state == UART_PORT_OPEN &&
	    port->wakeup_irq > 0  && port->uport.state->port.tty) {
		atomic_set(&port->check_wakeup_byte, 0);
		enable_irq(port->wakeup_irq);
		port->wakeup_enabled = true;
		ret = irq_set_irq_wake(port->wakeup_irq, 1);
		if (unlikely(ret))
			dev_err(dev, "%s:Failed to set IRQ wake:%d\n",
				__func__, ret);
	}

	geni_capture_stop_time(&port->se, port->ipc_log_kpi, __func__,
			       port->uart_kpi, start_time, 0, 0);

	UART_LOG_DBG(port->ipc_log_pwr, dev, "%s: End %d\n", __func__, ret);
	__pm_relax(port->geni_wake);
exit_runtime_suspend:
	return ret;
}

static int msm_geni_serial_runtime_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct msm_geni_serial_port *port = platform_get_drvdata(pdev);
	int ret = 0;
	unsigned long long start_time;

	UART_LOG_DBG(port->ipc_log_pwr, dev, "%s: Start %d\n", __func__, true);

	start_time = geni_capture_start_time(&port->se, port->ipc_log_kpi,
					     __func__, port->uart_kpi);
	/*
	 * Do an unconditional relax followed by a stay awake in case the
	 * wake source is activated by the wakeup isr.
	 */
	__pm_relax(port->geni_wake);
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
	ret = msm_geni_serial_resources_on(port);
	if (ret) {
		dev_err(dev, "%s: Error ret %d\n", __func__, ret);
		msm_geni_update_uart_error_code(port, UART_ERROR_SE_RESOURCES_ON_FAIL);
		__pm_relax(port->geni_wake);
		goto exit_runtime_resume;
	}
	msm_geni_enable_disable_se_clk(&port->uport, true);

	/* Don't start the RX sequencer during shutdown */
	if (port->port_state == UART_PORT_OPEN)
		start_rx_sequencer(&port->uport);
	if (port->resuming_from_deep_sleep)
		msm_geni_serial_port_setup(&port->uport);

	/* Ensure that the Rx is running before enabling interrupts */
	mb();
	/* Enable interrupt */
	enable_irq(port->uport.irq);

	geni_capture_stop_time(&port->se, port->ipc_log_kpi, __func__,
			       port->uart_kpi, start_time, 0, 0);
	UART_LOG_DBG(port->ipc_log_pwr, dev, "%s: End %d\n", __func__, ret);
	if (port->resuming_from_deep_sleep) {
		msm_geni_serial_reconfigure_baud_rate(&port->uport);
		port->resuming_from_deep_sleep = false;
	}

exit_runtime_resume:
	return ret;
}

static int msm_geni_serial_sys_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct msm_geni_serial_port *port = platform_get_drvdata(pdev);
	struct uart_port *uport = &port->uport;
	unsigned long long start_time;

	/* Platform driver is registered for console and when console
	 * is disabled from cmdline simply return success.
	 */
	start_time = geni_capture_start_time(&port->se, port->ipc_log_kpi,
					     __func__, port->uart_kpi);
	if (port->is_console && !con_enabled) {
		return 0;
	} else if (uart_console(uport)) {
		IPC_LOG_MSG(port->console_log, "%s start %d\n", __func__, true);
		uart_suspend_port((struct uart_driver *)uport->private_data,
					uport);
		IPC_LOG_MSG(port->console_log, "%s end %d\n", __func__, true);
	} else if (port->pm_auto_suspend_disable) {
		UART_LOG_DBG(port->ipc_log_pwr, dev, "%s start %d\n", __func__, true);
		uart_suspend_port((struct uart_driver *)uport->private_data, uport);
		UART_LOG_DBG(port->ipc_log_pwr, dev, "%s end %d\n", __func__, true);
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
		UART_LOG_DBG(port->ipc_log_pwr, dev, "%s end %d\n", __func__, true);
		mutex_unlock(&tty_port->mutex);
	}
	geni_capture_stop_time(&port->se, port->ipc_log_kpi,
			       __func__, port->uart_kpi, start_time, 0, 0);
	return 0;
}

static int msm_geni_serial_sys_hib_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct msm_geni_serial_port *port = platform_get_drvdata(pdev);
	struct uart_port *uport = &port->uport;

	if (uart_console(uport)) {
		uart_resume_port((struct uart_driver *)uport->private_data, uport);
		/*
		 * For hibernation usecase clients for
		 * console UART won't call port setup during restore.
		 * Hence call port setup for console uart.
		 */
		msm_geni_serial_port_setup(uport);
	} else if (port->pm_auto_suspend_disable) {
		/*
		 * Peripheral register settings are lost during hibernation
		 * or deep sleep case so update setup flag such that port
		 * setup happens again during next session.
		 */
		port->port_setup = false;
		uart_resume_port((struct uart_driver *)uport->private_data, uport);
	} else {
		/*
		 * Peripheral register settings are lost during hibernation.
		 * Update setup flag such that port setup happens again
		 * during next session. Clients of HS-UART will close and
		 * open the port during hibernation.
		 */
		port->port_setup = false;
	}
	UART_LOG_DBG(port->ipc_log_pwr, dev, "%s: End %d\n", __func__, true);
	return 0;
}

static int msm_geni_serial_sys_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct msm_geni_serial_port *port = platform_get_drvdata(pdev);
	struct uart_port *uport = &port->uport;
	unsigned long long start_time;

	start_time = geni_capture_start_time(&port->se, port->ipc_log_kpi,
					     __func__, port->uart_kpi);

	UART_LOG_DBG(port->ipc_log_pwr, dev, "%s: System resume Start %d\n",
		     __func__, true);

	if (pm_suspend_target_state == PM_SUSPEND_MEM) {
		UART_LOG_DBG(port->ipc_log_pwr, dev,
			     "deepsleep: %s\n", __func__);

		if (!uart_console(uport))
			port->resuming_from_deep_sleep = true;

		geni_capture_stop_time(&port->se, port->ipc_log_kpi,
				       __func__, port->uart_kpi, start_time, 0, 0);

		return msm_geni_serial_sys_hib_resume(dev);
	}

	/* Platform driver is registered for console and when console
	 * is disabled from cmdline simply return success.
	 */
	if (port->is_console && !con_enabled) {
		return 0;
	} else if ((uart_console(uport) &&
	    console_suspend_enabled && uport->suspended) ||
		port->pm_auto_suspend_disable) {
		IPC_LOG_MSG(port->console_log, "%s start %d\n", __func__, true);
		uart_resume_port((struct uart_driver *)uport->private_data,
									uport);
		IPC_LOG_MSG(port->console_log, "%s end %d", __func__, true);
	}
	geni_capture_stop_time(&port->se, port->ipc_log_kpi,
			       __func__, port->uart_kpi, start_time, 0, 0);
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

static int msm_geni_serial_sys_hib_resume(struct device *dev)
{
	return 0;
}
#endif

static const struct dev_pm_ops msm_geni_serial_pm_ops = {
	.runtime_suspend = msm_geni_serial_runtime_suspend,
	.runtime_resume = msm_geni_serial_runtime_resume,
	.suspend = msm_geni_serial_sys_suspend,
	.resume = msm_geni_serial_sys_resume,
	.freeze = msm_geni_serial_sys_suspend,
	.restore = msm_geni_serial_sys_hib_resume,
	.thaw = msm_geni_serial_sys_hib_resume,
};

static struct platform_driver msm_geni_serial_platform_driver = {
	.remove = msm_geni_serial_remove,
	.shutdown = msm_geni_serial_driver_shutdown,
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

	return ret;
}
module_init(msm_geni_serial_init);

static void __exit msm_geni_serial_exit(void)
{
	platform_driver_unregister(&msm_geni_serial_platform_driver);
	if (con_enabled)
		console_unregister(&msm_geni_console_driver);
	uart_unregister_driver(&msm_geni_serial_hs_driver);
}
module_exit(msm_geni_serial_exit);

MODULE_DESCRIPTION("Serial driver for GENI based QTI serial cores");
MODULE_LICENSE("GPL");
MODULE_ALIAS("tty:msm_geni_geni_serial");

