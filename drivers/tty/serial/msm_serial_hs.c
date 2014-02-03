/* drivers/serial/msm_serial_hs.c
 *
 * MSM 7k High speed uart driver
 *
 * Copyright (c) 2008 Google Inc.
 * Copyright (c) 2007-2014, The Linux Foundation. All rights reserved.
 * Modified: Nick Pelly <npelly@google.com>
 *
 * All source code in this file is licensed under the following license
 * except where indicated.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * Has optional support for uart power management independent of linux
 * suspend/resume:
 *
 * RX wakeup.
 * UART wakeup can be triggered by RX activity (using a wakeup GPIO on the
 * UART RX pin). This should only be used if there is not a wakeup
 * GPIO on the UART CTS, and the first RX byte is known (for example, with the
 * Bluetooth Texas Instruments HCILL protocol), since the first RX byte will
 * always be lost. RTS will be asserted even while the UART is off in this mode
 * of operation. See msm_serial_hs_platform_data.rx_wakeup_irq.
 */

#include <linux/module.h>

#include <linux/serial.h>
#include <linux/serial_core.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/atomic.h>
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include <linux/tty_flip.h>
#include <linux/wait.h>
#include <linux/sysfs.h>
#include <linux/stat.h>
#include <linux/device.h>
#include <linux/wakelock.h>
#include <linux/debugfs.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <asm/atomic.h>
#include <asm/irq.h>

#include <mach/hardware.h>
#include <mach/dma.h>
#include <mach/sps.h>
#include <mach/msm_serial_hs.h>
#include <mach/msm_bus.h>
#include <mach/msm_ipc_logging.h>
#include "msm_serial_hs_hwreg.h"
#define UART_SPS_CONS_PERIPHERAL 0
#define UART_SPS_PROD_PERIPHERAL 1

static void *ipc_msm_hs_log_ctxt;
#define IPC_MSM_HS_LOG_PAGES 5

/* If the debug_mask gets set to FATAL_LEV,
 * a fatal error has happened and further IPC logging
 * is disabled so that this problem can be detected
 */
enum {
	FATAL_LEV = 0U,
	ERR_LEV = 1U,
	WARN_LEV = 2U,
	INFO_LEV = 3U,
	DBG_LEV = 4U,
};

/* Default IPC log level INFO */
static int hs_serial_debug_mask = INFO_LEV;
module_param_named(debug_mask, hs_serial_debug_mask,
		   int, S_IRUGO | S_IWUSR | S_IWGRP);

#define MSM_HS_DBG(x...) do { \
	if (hs_serial_debug_mask >= DBG_LEV) { \
		if (ipc_msm_hs_log_ctxt) \
			ipc_log_string(ipc_msm_hs_log_ctxt, x); \
	} \
} while (0)

#define MSM_HS_INFO(x...) do { \
	if (hs_serial_debug_mask >= INFO_LEV) {\
		if (ipc_msm_hs_log_ctxt) \
			ipc_log_string(ipc_msm_hs_log_ctxt, x); \
	} \
} while (0)

/* warnings and errors show up on console always */
#define MSM_HS_WARN(x...) do { \
	pr_warn(x); \
	if (ipc_msm_hs_log_ctxt && hs_serial_debug_mask >= WARN_LEV) \
		ipc_log_string(ipc_msm_hs_log_ctxt, x); \
} while (0)

/* ERROR condition in the driver sets the hs_serial_debug_mask
 * to ERR_FATAL level, so that this message can be seen
 * in IPC logging. Further errors continue to log on the console
 */
#define MSM_HS_ERR(x...) do { \
	pr_err(x); \
	if (ipc_msm_hs_log_ctxt && hs_serial_debug_mask >= ERR_LEV) { \
		ipc_log_string(ipc_msm_hs_log_ctxt, x); \
		hs_serial_debug_mask = FATAL_LEV; \
	} \
} while (0)
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

enum flush_reason {
	FLUSH_NONE,
	FLUSH_DATA_READY,
	FLUSH_DATA_INVALID,  /* values after this indicate invalid data */
	FLUSH_IGNORE,
	FLUSH_STOP,
	FLUSH_SHUTDOWN,
};

enum msm_hs_clk_states_e {
	MSM_HS_CLK_PORT_OFF,     /* port not in use */
	MSM_HS_CLK_OFF,          /* clock disabled */
	MSM_HS_CLK_REQUEST_OFF,  /* disable after TX and RX flushed */
	MSM_HS_CLK_ON,           /* clock enabled */
};

/* Track the forced RXSTALE flush during clock off sequence.
 * These states are only valid during MSM_HS_CLK_REQUEST_OFF */
enum msm_hs_clk_req_off_state_e {
	CLK_REQ_OFF_START,
	CLK_REQ_OFF_RXSTALE_ISSUED,
	CLK_REQ_OFF_FLUSH_ISSUED,
	CLK_REQ_OFF_RXSTALE_FLUSHED,
};

/* SPS data structures to support HSUART with BAM
 * @sps_pipe - This struct defines BAM pipe descriptor
 * @sps_connect - This struct defines a connection's end point
 * @sps_register - This struct defines a event registration parameters
 */
struct msm_hs_sps_ep_conn_data {
	struct sps_pipe *pipe_handle;
	struct sps_connect config;
	struct sps_register_event event;
};

struct msm_hs_tx {
	unsigned int tx_ready_int_en;  /* ok to dma more tx */
	unsigned int dma_in_flight;    /* tx dma in progress */
	enum flush_reason flush;
	wait_queue_head_t wait;
	struct msm_dmov_cmd xfer;
	dmov_box *command_ptr;
	u32 *command_ptr_ptr;
	dma_addr_t mapped_cmd_ptr;
	dma_addr_t mapped_cmd_ptr_ptr;
	int tx_count;
	dma_addr_t dma_base;
	struct tasklet_struct tlet;
	struct msm_hs_sps_ep_conn_data cons;
};

struct msm_hs_rx {
	enum flush_reason flush;
	struct msm_dmov_cmd xfer;
	dma_addr_t cmdptr_dmaaddr;
	dmov_box *command_ptr;
	u32 *command_ptr_ptr;
	dma_addr_t mapped_cmd_ptr;
	wait_queue_head_t wait;
	dma_addr_t rbuffer;
	unsigned char *buffer;
	unsigned int buffer_pending;
	struct dma_pool *pool;
	struct wake_lock wake_lock;
	struct delayed_work flip_insert_work;
	struct tasklet_struct tlet;
	struct msm_hs_sps_ep_conn_data prod;
	bool rx_cmd_queued;
	bool rx_cmd_exec;
};
enum buffer_states {
	NONE_PENDING = 0x0,
	FIFO_OVERRUN = 0x1,
	PARITY_ERROR = 0x2,
	CHARS_NORMAL = 0x4,
};

/* optional low power wakeup, typically on a GPIO RX irq */
struct msm_hs_wakeup {
	int irq;  /* < 0 indicates low power wakeup disabled */
	unsigned char ignore;  /* bool */

	/* bool: inject char into rx tty on wakeup */
	unsigned char inject_rx;
	char rx_to_inject;
};

struct msm_hs_port {
	struct uart_port uport;
	unsigned long imr_reg;  /* shadow value of UARTDM_IMR */
	struct clk *clk;
	struct clk *pclk;
	struct msm_hs_tx tx;
	struct msm_hs_rx rx;
	/* gsbi uarts have to do additional writes to gsbi memory */
	/* block and top control status block. The following pointers */
	/* keep a handle to these blocks. */
	unsigned char __iomem	*mapped_gsbi;
	int dma_tx_channel;
	int dma_rx_channel;
	int dma_tx_crci;
	int dma_rx_crci;
	struct hrtimer clk_off_timer;  /* to poll TXEMT before clock off */
	ktime_t clk_off_delay;
	enum msm_hs_clk_states_e clk_state;
	enum msm_hs_clk_req_off_state_e clk_req_off_state;
	atomic_t clk_count;
	struct msm_hs_wakeup wakeup;
	struct wake_lock dma_wake_lock;  /* held while any DMA active */

	struct dentry *loopback_dir;
	struct work_struct clock_off_w; /* work for actual clock off */
	struct workqueue_struct *hsuart_wq; /* hsuart workqueue */
	struct mutex clk_mutex; /* mutex to guard against clock off/clock on */
	struct work_struct disconnect_rx_endpoint; /* disconnect rx_endpoint */
	bool tty_flush_receive;
	enum uart_core_type uart_type;
	u32 bam_handle;
	resource_size_t bam_mem;
	int bam_irq;
	unsigned char __iomem *bam_base;
	unsigned int bam_tx_ep_pipe_index;
	unsigned int bam_rx_ep_pipe_index;
	/* struct sps_event_notify is an argument passed when triggering a
	 * callback event object registered for an SPS connection end point.
	 */
	struct sps_event_notify notify;
	/* bus client handler */
	u32 bus_perf_client;
	/* BLSP UART required BUS Scaling data */
	struct msm_bus_scale_pdata *bus_scale_table;
	bool rx_discard_flush_issued;
	int rx_count_callback;
	bool rx_bam_inprogress;
	unsigned int *reg_ptr;
	wait_queue_head_t bam_disconnect_wait;

};

unsigned int regmap_nonblsp[UART_DM_LAST] = {
		[UART_DM_MR1] = UARTDM_MR1_ADDR,
		[UART_DM_MR2] = UARTDM_MR2_ADDR,
		[UART_DM_IMR] = UARTDM_IMR_ADDR,
		[UART_DM_SR] = UARTDM_SR_ADDR,
		[UART_DM_CR] = UARTDM_CR_ADDR,
		[UART_DM_CSR] = UARTDM_CSR_ADDR,
		[UART_DM_IPR] = UARTDM_IPR_ADDR,
		[UART_DM_ISR] = UARTDM_ISR_ADDR,
		[UART_DM_RX_TOTAL_SNAP] = UARTDM_RX_TOTAL_SNAP_ADDR,
		[UART_DM_TFWR] = UARTDM_TFWR_ADDR,
		[UART_DM_RFWR] = UARTDM_RFWR_ADDR,
		[UART_DM_RF] = UARTDM_RF_ADDR,
		[UART_DM_TF] = UARTDM_TF_ADDR,
		[UART_DM_MISR] = UARTDM_MISR_ADDR,
		[UART_DM_DMRX] = UARTDM_DMRX_ADDR,
		[UART_DM_NCF_TX] = UARTDM_NCF_TX_ADDR,
		[UART_DM_DMEN] = UARTDM_DMEN_ADDR,
		[UART_DM_TXFS] = UARTDM_TXFS_ADDR,
		[UART_DM_RXFS] = UARTDM_RXFS_ADDR,
		[UART_DM_RX_TRANS_CTRL] = UARTDM_RX_TRANS_CTRL_ADDR,
};

unsigned int regmap_blsp[UART_DM_LAST] = {
		[UART_DM_MR1] = 0x0,
		[UART_DM_MR2] = 0x4,
		[UART_DM_IMR] = 0xb0,
		[UART_DM_SR] = 0xa4,
		[UART_DM_CR] = 0xa8,
		[UART_DM_CSR] = 0xa0,
		[UART_DM_IPR] = 0x18,
		[UART_DM_ISR] = 0xb4,
		[UART_DM_RX_TOTAL_SNAP] = 0xbc,
		[UART_DM_TFWR] = 0x1c,
		[UART_DM_RFWR] = 0x20,
		[UART_DM_RF] = 0x140,
		[UART_DM_TF] = 0x100,
		[UART_DM_MISR] = 0xac,
		[UART_DM_DMRX] = 0x34,
		[UART_DM_NCF_TX] = 0x40,
		[UART_DM_DMEN] = 0x3c,
		[UART_DM_TXFS] = 0x4c,
		[UART_DM_RXFS] = 0x50,
		[UART_DM_RX_TRANS_CTRL] = 0xcc,
};

static struct of_device_id msm_hs_match_table[] = {
	{ .compatible = "qcom,msm-hsuart-v14",
	  .data = regmap_blsp
	},
	{
	  .compatible = "qcom,msm-hsuart-v13",
	  .data = regmap_nonblsp
	},
	{}
};


#define MSM_UARTDM_BURST_SIZE 16   /* DM burst size (in bytes) */
#define UARTDM_TX_BUF_SIZE UART_XMIT_SIZE
#define UARTDM_RX_BUF_SIZE 512
#define RETRY_TIMEOUT 5
#define UARTDM_NR 256
#define BAM_PIPE_MIN 0
#define BAM_PIPE_MAX 11
#define BUS_SCALING 1
#define BUS_RESET 0
#define RX_FLUSH_COMPLETE_TIMEOUT 300 /* In jiffies */
#define BLSP_UART_CLK_FMAX 63160000

static struct dentry *debug_base;
static struct platform_driver msm_serial_hs_platform_driver;
static struct uart_driver msm_hs_driver;
static struct uart_ops msm_hs_ops;
static void msm_hs_start_rx_locked(struct uart_port *uport);
static void msm_serial_hs_rx_tlet(unsigned long tlet_ptr);
static void flip_insert_work(struct work_struct *work);
static void msm_hs_bus_voting(struct msm_hs_port *msm_uport, unsigned int vote);
static struct msm_hs_port *msm_hs_get_hs_port(int port_index);

#define UARTDM_TO_MSM(uart_port) \
	container_of((uart_port), struct msm_hs_port, uport)


static int msm_hs_ioctl(struct uart_port *uport, unsigned int cmd,
						unsigned long arg)
{
	int ret = 0, state = 1;
	enum msm_hs_clk_states_e clk_state;
	unsigned long flags;
	struct msm_hs_port *msm_uport = UARTDM_TO_MSM(uport);

	switch (cmd) {
	case MSM_ENABLE_UART_CLOCK: {
		MSM_HS_DBG("%s():ENABLE UART CLOCK: cmd=%d\n", __func__, cmd);
		msm_hs_request_clock_on(&msm_uport->uport);
		break;
	}
	case MSM_DISABLE_UART_CLOCK: {
		MSM_HS_DBG("%s():DISABLE UART CLOCK: cmd=%d\n", __func__, cmd);
		msm_hs_request_clock_off(&msm_uport->uport);
		break;
	}
	case MSM_GET_UART_CLOCK_STATUS: {
		/* Return value 0 - UART CLOCK is OFF
		 * Return value 1 - UART CLOCK is ON */
		MSM_HS_DBG("%s():GET UART CLOCK STATUS: cmd=%d\n", __func__, cmd);
		spin_lock_irqsave(&msm_uport->uport.lock, flags);
		clk_state = msm_uport->clk_state;
		spin_unlock_irqrestore(&msm_uport->uport.lock, flags);
		if (clk_state <= MSM_HS_CLK_OFF)
			state = 0;
		ret = state;
		break;
	}
	default: {
		MSM_HS_DBG("%s():Unknown cmd specified: cmd=%d\n", __func__, cmd);
		ret = -ENOIOCTLCMD;
		break;
	}
	}

	return ret;
}

static int msm_hs_clock_vote(struct msm_hs_port *msm_uport)
{
	int rc = 0;

	if (1 == atomic_inc_return(&msm_uport->clk_count)) {
		msm_hs_bus_voting(msm_uport, BUS_SCALING);
		/* Turn on core clk and iface clk */
		rc = clk_prepare_enable(msm_uport->clk);
		if (rc) {
			dev_err(msm_uport->uport.dev,
				"%s: Could not turn on core clk [%d]\n",
				__func__, rc);
			return rc;
		}

		if (msm_uport->pclk) {
			rc = clk_prepare_enable(msm_uport->pclk);
			if (rc) {
				clk_disable_unprepare(msm_uport->clk);
				dev_err(msm_uport->uport.dev,
					"%s: Could not turn on pclk [%d]\n",
					__func__, rc);
				return rc;
			}
		}
		msm_uport->clk_state = MSM_HS_CLK_ON;
		MSM_HS_DBG("%s: Clock ON successful\n", __func__);
	}


	return rc;
}

static void msm_hs_clock_unvote(struct msm_hs_port *msm_uport)
{
	int rc = atomic_read(&msm_uport->clk_count);

	if (rc <= 0) {
		WARN(rc, "msm_uport->clk_count < 0!");
		dev_err(msm_uport->uport.dev,
			"%s: Clocks count invalid  [%d]\n", __func__, rc);
		return;
	}

	rc = atomic_dec_return(&msm_uport->clk_count);
	if (0 == rc) {
		/* Turn off the core clk and iface clk*/
		clk_disable_unprepare(msm_uport->clk);
		if (msm_uport->pclk)
			clk_disable_unprepare(msm_uport->pclk);
		/* Unvote the PNOC clock */
		msm_hs_bus_voting(msm_uport, BUS_RESET);
		msm_uport->clk_state = MSM_HS_CLK_OFF;
		MSM_HS_DBG("%s: Clock OFF successful\n", __func__);
	}
}

/* Check if the uport line number matches with user id stored in pdata.
 * User id information is stored during initialization. This function
 * ensues that the same device is selected */

static struct msm_hs_port *get_matching_hs_port(struct platform_device *pdev)
{
	struct msm_serial_hs_platform_data *pdata = pdev->dev.platform_data;
	struct msm_hs_port *msm_uport = msm_hs_get_hs_port(pdev->id);

	if ((!msm_uport) || (msm_uport->uport.line != pdev->id
	   && msm_uport->uport.line != pdata->userid)) {
		MSM_HS_ERR("uport line number mismatch!");
		WARN_ON(1);
		return NULL;
	}

	return msm_uport;
}

static ssize_t show_clock(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	int state = 1;
	ssize_t ret = 0;
	enum msm_hs_clk_states_e clk_state;
	unsigned long flags;
	struct platform_device *pdev = container_of(dev, struct
						    platform_device, dev);
	struct msm_hs_port *msm_uport = get_matching_hs_port(pdev);

	/* This check should not fail */
	if (msm_uport) {
		spin_lock_irqsave(&msm_uport->uport.lock, flags);
		clk_state = msm_uport->clk_state;
		spin_unlock_irqrestore(&msm_uport->uport.lock, flags);

		if (clk_state <= MSM_HS_CLK_OFF)
			state = 0;
		ret = snprintf(buf, PAGE_SIZE, "%d\n", state);
	}

	return ret;
}

static ssize_t set_clock(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	int state;
	ssize_t ret = 0;
	struct platform_device *pdev = container_of(dev, struct
						    platform_device, dev);
	struct msm_hs_port *msm_uport = get_matching_hs_port(pdev);

	/* This check should not fail */
	if (msm_uport) {
		state = buf[0] - '0';
		switch (state) {
		case 0:
			msm_hs_request_clock_off(&msm_uport->uport);
			ret = count;
			break;
		case 1:
			msm_hs_request_clock_on(&msm_uport->uport);
			ret = count;
			break;
		default:
			ret = -EINVAL;
		}
	}
	return ret;
}

static DEVICE_ATTR(clock, S_IWUSR | S_IRUGO, show_clock, set_clock);

static inline unsigned int use_low_power_wakeup(struct msm_hs_port *msm_uport)
{
	return (msm_uport->wakeup.irq > 0);
}

static inline int is_gsbi_uart(struct msm_hs_port *msm_uport)
{
	/* assume gsbi uart if gsbi resource found in pdata */
	return ((msm_uport->mapped_gsbi != NULL));
}
static unsigned int is_blsp_uart(struct msm_hs_port *msm_uport)
{
	return (msm_uport->uart_type == BLSP_HSUART);
}

static void msm_hs_bus_voting(struct msm_hs_port *msm_uport, unsigned int vote)
{
	int ret;

	if (is_blsp_uart(msm_uport) && msm_uport->bus_perf_client) {
		MSM_HS_DBG("Bus voting:%d\n", vote);
		ret = msm_bus_scale_client_update_request(
				msm_uport->bus_perf_client, vote);
		if (ret)
			MSM_HS_ERR("%s(): Failed for Bus voting: %d\n",
							__func__, vote);
	}
}

static inline unsigned int msm_hs_read(struct uart_port *uport,
				       unsigned int index)
{
	struct msm_hs_port *msm_uport = UARTDM_TO_MSM(uport);
	unsigned int offset;

	offset = *(msm_uport->reg_ptr + index);

	return readl_relaxed(uport->membase + offset);
}

static inline void msm_hs_write(struct uart_port *uport, unsigned int index,
				 unsigned int value)
{
	struct msm_hs_port *msm_uport = UARTDM_TO_MSM(uport);
	unsigned int offset;

	offset = *(msm_uport->reg_ptr + index);
	writel_relaxed(value, uport->membase + offset);
}

static int sps_rx_disconnect(struct sps_pipe *sps_pipe_handler)
{
	struct sps_connect config;
	int ret;

	ret = sps_get_config(sps_pipe_handler, &config);
	if (ret) {
		pr_err("%s: sps_get_config() failed ret %d\n", __func__, ret);
		return ret;
	}
	config.options |= SPS_O_POLL;
	ret = sps_set_config(sps_pipe_handler, &config);
	if (ret) {
		pr_err("%s: sps_set_config() failed ret %d\n", __func__, ret);
		return ret;
	}
	return sps_disconnect(sps_pipe_handler);
}

static void hex_dump_ipc(char *prefix, char *string, int size)
{
	char linebuf[512];

	hex_dump_to_buffer(string, size, 16, 1, linebuf, sizeof(linebuf), 1);
	MSM_HS_DBG("%s : %s", prefix, linebuf);
}

/*
 * This API read and provides UART Core registers information.
*/
static void dump_uart_hs_registers(struct msm_hs_port *msm_uport)
{
	struct uart_port *uport = &(msm_uport->uport);
	if (msm_uport->clk_state != MSM_HS_CLK_ON) {
		MSM_HS_WARN("%s: Failed.Clocks are OFF\n", __func__);
		return;
	}

	MSM_HS_DBG(
	"MR1:%x MR2:%x TFWR:%x RFWR:%x DMEN:%x IMR:%x MISR:%x NCF_TX:%x\n",
	msm_hs_read(uport, UART_DM_MR1),
	msm_hs_read(uport, UART_DM_MR2),
	msm_hs_read(uport, UART_DM_TFWR),
	msm_hs_read(uport, UART_DM_RFWR),
	msm_hs_read(uport, UART_DM_DMEN),
	msm_hs_read(uport, UART_DM_IMR),
	msm_hs_read(uport, UART_DM_MISR),
	msm_hs_read(uport, UART_DM_NCF_TX));
	MSM_HS_INFO("SR:%x ISR:%x DMRX:%x RX_SNAP:%x TXFS:%x RXFS:%x\n",
	msm_hs_read(uport, UART_DM_SR),
	msm_hs_read(uport, UART_DM_ISR),
	msm_hs_read(uport, UART_DM_DMRX),
	msm_hs_read(uport, UART_DM_RX_TOTAL_SNAP),
	msm_hs_read(uport, UART_DM_TXFS),
	msm_hs_read(uport, UART_DM_RXFS));
	MSM_HS_DBG("clk_req_state:0x%x rx.flush:%u\n",
				msm_uport->clk_req_off_state,
					msm_uport->rx.flush);
	MSM_HS_DBG("clk_state:%d", msm_uport->clk_state);
}

static void msm_hs_release_port(struct uart_port *port)
{
	struct msm_hs_port *msm_uport = UARTDM_TO_MSM(port);
	struct platform_device *pdev = to_platform_device(port->dev);
	struct resource *gsbi_resource;
	resource_size_t size;

	if (is_gsbi_uart(msm_uport)) {
		iowrite32(GSBI_PROTOCOL_IDLE, msm_uport->mapped_gsbi +
			  GSBI_CONTROL_ADDR);
		gsbi_resource = platform_get_resource_byname(pdev,
							     IORESOURCE_MEM,
							     "gsbi_resource");
		if (unlikely(!gsbi_resource))
			return;

		size = resource_size(gsbi_resource);
		release_mem_region(gsbi_resource->start, size);
		iounmap(msm_uport->mapped_gsbi);
		msm_uport->mapped_gsbi = NULL;
	}
}

static int msm_hs_request_port(struct uart_port *port)
{
	struct msm_hs_port *msm_uport = UARTDM_TO_MSM(port);
	struct platform_device *pdev = to_platform_device(port->dev);
	struct resource *gsbi_resource;
	resource_size_t size;

	gsbi_resource = platform_get_resource_byname(pdev,
						     IORESOURCE_MEM,
						     "gsbi_resource");
	if (gsbi_resource) {
		size = resource_size(gsbi_resource);
		if (unlikely(!request_mem_region(gsbi_resource->start, size,
						 "msm_serial_hs")))
			return -EBUSY;
		msm_uport->mapped_gsbi = ioremap(gsbi_resource->start,
						 size);
		if (!msm_uport->mapped_gsbi) {
			release_mem_region(gsbi_resource->start, size);
			return -EBUSY;
		}
	}
	/* no gsbi uart */
	return 0;
}

static int msm_serial_loopback_enable_set(void *data, u64 val)
{
	struct msm_hs_port *msm_uport = data;
	struct uart_port *uport = &(msm_uport->uport);
	unsigned long flags;
	int ret = 0;

	msm_hs_clock_vote(msm_uport);

	if (val) {
		spin_lock_irqsave(&uport->lock, flags);
		ret = msm_hs_read(uport, UART_DM_MR2);
		if (is_blsp_uart(msm_uport))
			ret |= (UARTDM_MR2_LOOP_MODE_BMSK |
				UARTDM_MR2_RFR_CTS_LOOP_MODE_BMSK);
		else
			ret |= UARTDM_MR2_LOOP_MODE_BMSK;
		msm_hs_write(uport, UART_DM_MR2, ret);
		spin_unlock_irqrestore(&uport->lock, flags);
	} else {
		spin_lock_irqsave(&uport->lock, flags);
		ret = msm_hs_read(uport, UART_DM_MR2);
		if (is_blsp_uart(msm_uport))
			ret &= ~(UARTDM_MR2_LOOP_MODE_BMSK |
				UARTDM_MR2_RFR_CTS_LOOP_MODE_BMSK);
		else
			ret &= ~UARTDM_MR2_LOOP_MODE_BMSK;
		msm_hs_write(uport, UART_DM_MR2, ret);
		spin_unlock_irqrestore(&uport->lock, flags);
	}
	/* Calling CLOCK API. Hence mb() requires here. */
	mb();

	msm_hs_clock_unvote(msm_uport);
	return 0;
}

static int msm_serial_loopback_enable_get(void *data, u64 *val)
{
	struct msm_hs_port *msm_uport = data;
	struct uart_port *uport = &(msm_uport->uport);
	unsigned long flags;
	int ret = 0;

	msm_hs_clock_vote(msm_uport);

	spin_lock_irqsave(&uport->lock, flags);
	ret = msm_hs_read(&msm_uport->uport, UART_DM_MR2);
	spin_unlock_irqrestore(&uport->lock, flags);

	msm_hs_clock_unvote(msm_uport);

	*val = (ret & UARTDM_MR2_LOOP_MODE_BMSK) ? 1 : 0;

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(loopback_enable_fops, msm_serial_loopback_enable_get,
			msm_serial_loopback_enable_set, "%llu\n");

/*
 * msm_serial_hs debugfs node: <debugfs_root>/msm_serial_hs/loopback.<id>
 * writing 1 turns on internal loopback mode in HW. Useful for automation
 * test scripts.
 * writing 0 disables the internal loopback mode. Default is disabled.
 */
static void __devinit msm_serial_debugfs_init(struct msm_hs_port *msm_uport,
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
		MSM_HS_ERR("%s(): Cannot create loopback.%d debug entry",
							__func__, id);
}

static int __devexit msm_hs_remove(struct platform_device *pdev)
{

	struct msm_hs_port *msm_uport;
	struct device *dev;

	if (pdev->id < 0 || pdev->id >= UARTDM_NR) {
		MSM_HS_ERR(KERN_ERR "Invalid plaform device ID = %d\n", pdev->id);
		return -EINVAL;
	}

	msm_uport = get_matching_hs_port(pdev);
	if (!msm_uport)
		return -EINVAL;

	dev = msm_uport->uport.dev;
	sysfs_remove_file(&pdev->dev.kobj, &dev_attr_clock.attr);
	debugfs_remove(msm_uport->loopback_dir);

	dma_unmap_single(dev, msm_uport->rx.mapped_cmd_ptr, sizeof(dmov_box),
			 DMA_TO_DEVICE);
	dma_pool_free(msm_uport->rx.pool, msm_uport->rx.buffer,
		      msm_uport->rx.rbuffer);
	dma_pool_destroy(msm_uport->rx.pool);

	dma_unmap_single(dev, msm_uport->rx.cmdptr_dmaaddr, sizeof(u32),
			 DMA_TO_DEVICE);
	dma_unmap_single(dev, msm_uport->tx.mapped_cmd_ptr_ptr, sizeof(u32),
			 DMA_TO_DEVICE);
	dma_unmap_single(dev, msm_uport->tx.mapped_cmd_ptr, sizeof(dmov_box),
			 DMA_TO_DEVICE);

	wake_lock_destroy(&msm_uport->rx.wake_lock);
	wake_lock_destroy(&msm_uport->dma_wake_lock);
	destroy_workqueue(msm_uport->hsuart_wq);
	mutex_destroy(&msm_uport->clk_mutex);

	uart_remove_one_port(&msm_hs_driver, &msm_uport->uport);
	clk_put(msm_uport->clk);
	if (msm_uport->pclk)
		clk_put(msm_uport->pclk);

	/* Free the tx resources */
	kfree(msm_uport->tx.command_ptr);
	kfree(msm_uport->tx.command_ptr_ptr);

	/* Free the rx resources */
	kfree(msm_uport->rx.command_ptr);
	kfree(msm_uport->rx.command_ptr_ptr);

	iounmap(msm_uport->uport.membase);

	return 0;
}

static int msm_hs_init_clk(struct uart_port *uport)
{
	int ret;
	struct msm_hs_port *msm_uport = UARTDM_TO_MSM(uport);

	/* Set up the MREG/NREG/DREG/MNDREG */
	ret = clk_set_rate(msm_uport->clk, uport->uartclk);
	if (ret) {
		MSM_HS_WARN("Error setting clock rate on UART\n");
		return ret;
	}

	ret = msm_hs_clock_vote(msm_uport);
	if (ret) {
		MSM_HS_ERR("Error could not turn on UART clk\n");
		return ret;
	}

	return 0;
}


/* Connect a UART peripheral's SPS endpoint(consumer endpoint)
 *
 * Also registers a SPS callback function for the consumer
 * process with the SPS driver
 *
 * @uport - Pointer to uart uport structure
 *
 * @return - 0 if successful else negative value.
 *
 */

static int msm_hs_spsconnect_tx(struct uart_port *uport)
{
	int ret;
	struct msm_hs_port *msm_uport = UARTDM_TO_MSM(uport);
	struct msm_hs_tx *tx = &msm_uport->tx;
	struct sps_pipe *sps_pipe_handle = tx->cons.pipe_handle;
	struct sps_connect *sps_config = &tx->cons.config;
	struct sps_register_event *sps_event = &tx->cons.event;

	/* Establish connection between peripheral and memory endpoint */
	ret = sps_connect(sps_pipe_handle, sps_config);
	if (ret) {
		MSM_HS_ERR("msm_serial_hs: sps_connect() failed for tx!!\n"
		"pipe_handle=0x%x ret=%d", (u32)sps_pipe_handle, ret);
		return ret;
	}
	/* Register callback event for EOT (End of transfer) event. */
	ret = sps_register_event(sps_pipe_handle, sps_event);
	if (ret) {
		MSM_HS_ERR("msm_serial_hs: sps_connect() failed for tx!!\n"
		"pipe_handle=0x%x ret=%d", (u32)sps_pipe_handle, ret);
		goto reg_event_err;
	}
	return 0;

reg_event_err:
	sps_disconnect(sps_pipe_handle);
	return ret;
}

/* Connect a UART peripheral's SPS endpoint(producer endpoint)
 *
 * Also registers a SPS callback function for the producer
 * process with the SPS driver
 *
 * @uport - Pointer to uart uport structure
 *
 * @return - 0 if successful else negative value.
 *
 */

static int msm_hs_spsconnect_rx(struct uart_port *uport)
{
	int ret;
	struct msm_hs_port *msm_uport = UARTDM_TO_MSM(uport);
	struct msm_hs_rx *rx = &msm_uport->rx;
	struct sps_pipe *sps_pipe_handle = rx->prod.pipe_handle;
	struct sps_connect *sps_config = &rx->prod.config;
	struct sps_register_event *sps_event = &rx->prod.event;

	/* Establish connection between peripheral and memory endpoint */
	ret = sps_connect(sps_pipe_handle, sps_config);
	if (ret) {
		MSM_HS_ERR("msm_serial_hs: sps_connect() failed for rx!!\n"
		"pipe_handle=0x%x ret=%d", (u32)sps_pipe_handle, ret);
		return ret;
	}
	/* Register callback event for DESC_DONE event. */
	ret = sps_register_event(sps_pipe_handle, sps_event);
	if (ret) {
		MSM_HS_ERR("msm_serial_hs: sps_connect() failed for rx!!\n"
		"pipe_handle=0x%x ret=%d", (u32)sps_pipe_handle, ret);
		goto reg_event_err;
	}
	return 0;

reg_event_err:
	sps_disconnect(sps_pipe_handle);
	return ret;
}

/*
 * programs the UARTDM_CSR register with correct bit rates
 *
 * Interrupts should be disabled before we are called, as
 * we modify Set Baud rate
 * Set receive stale interrupt level, dependant on Bit Rate
 * Goal is to have around 8 ms before indicate stale.
 * roundup (((Bit Rate * .008) / 10) + 1
 */
static void msm_hs_set_bps_locked(struct uart_port *uport,
			       unsigned int bps)
{
	unsigned long rxstale;
	unsigned long data;
	struct msm_hs_port *msm_uport = UARTDM_TO_MSM(uport);

	switch (bps) {
	case 300:
		msm_hs_write(uport, UART_DM_CSR, 0x00);
		rxstale = 1;
		break;
	case 600:
		msm_hs_write(uport, UART_DM_CSR, 0x11);
		rxstale = 1;
		break;
	case 1200:
		msm_hs_write(uport, UART_DM_CSR, 0x22);
		rxstale = 1;
		break;
	case 2400:
		msm_hs_write(uport, UART_DM_CSR, 0x33);
		rxstale = 1;
		break;
	case 4800:
		msm_hs_write(uport, UART_DM_CSR, 0x44);
		rxstale = 1;
		break;
	case 9600:
		msm_hs_write(uport, UART_DM_CSR, 0x55);
		rxstale = 2;
		break;
	case 14400:
		msm_hs_write(uport, UART_DM_CSR, 0x66);
		rxstale = 3;
		break;
	case 19200:
		msm_hs_write(uport, UART_DM_CSR, 0x77);
		rxstale = 4;
		break;
	case 28800:
		msm_hs_write(uport, UART_DM_CSR, 0x88);
		rxstale = 6;
		break;
	case 38400:
		msm_hs_write(uport, UART_DM_CSR, 0x99);
		rxstale = 8;
		break;
	case 57600:
		msm_hs_write(uport, UART_DM_CSR, 0xaa);
		rxstale = 16;
		break;
	case 76800:
		msm_hs_write(uport, UART_DM_CSR, 0xbb);
		rxstale = 16;
		break;
	case 115200:
		msm_hs_write(uport, UART_DM_CSR, 0xcc);
		rxstale = 31;
		break;
	case 230400:
		msm_hs_write(uport, UART_DM_CSR, 0xee);
		rxstale = 31;
		break;
	case 460800:
		msm_hs_write(uport, UART_DM_CSR, 0xff);
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
		msm_hs_write(uport, UART_DM_CSR, 0xff);
		rxstale = 31;
		break;
	default:
		msm_hs_write(uport, UART_DM_CSR, 0xff);
		/* default to 9600 */
		bps = 9600;
		rxstale = 2;
		break;
	}
	/*
	 * uart baud rate depends on CSR and MND Values
	 * we are updating CSR before and then calling
	 * clk_set_rate which updates MND Values. Hence
	 * dsb requires here.
	 */
	mb();
	if (bps > 460800) {
		uport->uartclk = bps * 16;
		if (is_blsp_uart(msm_uport)) {
			/* BLSP based UART supports maximum clock frequency
			 * of 63.16 Mhz. With this (63.16 Mhz) clock frequency
			 * UART can support baud rate of 3.94 Mbps which is
			 * equivalent to 4 Mbps.
			 * UART hardware is robust enough to handle this
			 * deviation to achieve baud rate ~4 Mbps.
			 */
			if (bps == 4000000)
				uport->uartclk = BLSP_UART_CLK_FMAX;
		}
	} else {
		uport->uartclk = 7372800;
	}

	if (clk_set_rate(msm_uport->clk, uport->uartclk)) {
		MSM_HS_WARN("Error setting clock rate on UART\n");
		WARN_ON(1);
	}

	data = rxstale & UARTDM_IPR_STALE_LSB_BMSK;
	data |= UARTDM_IPR_STALE_TIMEOUT_MSB_BMSK & (rxstale << 2);

	msm_hs_write(uport, UART_DM_IPR, data);
	/*
	 * It is suggested to do reset of transmitter and receiver after
	 * changing any protocol configuration. Here Baud rate and stale
	 * timeout are getting updated. Hence reset transmitter and receiver.
	 */
	msm_hs_write(uport, UART_DM_CR, RESET_TX);
	msm_hs_write(uport, UART_DM_CR, RESET_RX);
}


static void msm_hs_set_std_bps_locked(struct uart_port *uport,
			       unsigned int bps)
{
	unsigned long rxstale;
	unsigned long data;

	switch (bps) {
	case 9600:
		msm_hs_write(uport, UART_DM_CSR, 0x99);
		rxstale = 2;
		break;
	case 14400:
		msm_hs_write(uport, UART_DM_CSR, 0xaa);
		rxstale = 3;
		break;
	case 19200:
		msm_hs_write(uport, UART_DM_CSR, 0xbb);
		rxstale = 4;
		break;
	case 28800:
		msm_hs_write(uport, UART_DM_CSR, 0xcc);
		rxstale = 6;
		break;
	case 38400:
		msm_hs_write(uport, UART_DM_CSR, 0xdd);
		rxstale = 8;
		break;
	case 57600:
		msm_hs_write(uport, UART_DM_CSR, 0xee);
		rxstale = 16;
		break;
	case 115200:
		msm_hs_write(uport, UART_DM_CSR, 0xff);
		rxstale = 31;
		break;
	default:
		msm_hs_write(uport, UART_DM_CSR, 0x99);
		/* default to 9600 */
		bps = 9600;
		rxstale = 2;
		break;
	}

	data = rxstale & UARTDM_IPR_STALE_LSB_BMSK;
	data |= UARTDM_IPR_STALE_TIMEOUT_MSB_BMSK & (rxstale << 2);

	msm_hs_write(uport, UART_DM_IPR, data);
}


/*
 * termios :  new ktermios
 * oldtermios:  old ktermios previous setting
 *
 * Configure the serial port
 */
static void msm_hs_set_termios(struct uart_port *uport,
				   struct ktermios *termios,
				   struct ktermios *oldtermios)
{
	unsigned int bps;
	unsigned long data;
	int ret;
	unsigned int c_cflag = termios->c_cflag;
	struct msm_hs_port *msm_uport = UARTDM_TO_MSM(uport);
	struct msm_hs_rx *rx = &msm_uport->rx;
	struct sps_pipe *sps_pipe_handle = rx->prod.pipe_handle;

	if (msm_uport->clk_state != MSM_HS_CLK_ON) {
		MSM_HS_WARN("%s: Failed.Clocks are OFF\n", __func__);
		return;
	}
	mutex_lock(&msm_uport->clk_mutex);
	msm_hs_write(uport, UART_DM_IMR, 0);

	MSM_HS_DBG("Entering %s\n", __func__);
	dump_uart_hs_registers(msm_uport);

	/* Clear the Rx Ready Ctl bit - This ensures that
	* flow control lines stop the other side from sending
	* data while we change the parameters
	*/
	data = msm_hs_read(uport, UART_DM_MR1);
	data &= ~UARTDM_MR1_RX_RDY_CTL_BMSK;
	msm_hs_write(uport, UART_DM_MR1, data);
	/* set RFR_N to high */
	msm_hs_write(uport, UART_DM_CR, RFR_HIGH);

	/*
	 * Disable Rx channel of UARTDM
	 * DMA Rx Stall happens if enqueue and flush of Rx command happens
	 * concurrently. Hence before changing the baud rate/protocol
	 * configuration and sending flush command to ADM, disable the Rx
	 * channel of UARTDM.
	 * Note: should not reset the receiver here immediately as it is not
	 * suggested to do disable/reset or reset/disable at the same time.
	 */
	data = msm_hs_read(uport, UART_DM_DMEN);
	if (is_blsp_uart(msm_uport)) {
		/* Disable UARTDM RX BAM Interface */
		data &= ~UARTDM_RX_BAM_ENABLE_BMSK;
	} else {
		data &= ~UARTDM_RX_DM_EN_BMSK;
	}

	msm_hs_write(uport, UART_DM_DMEN, data);

	/* 300 is the minimum baud support by the driver  */
	bps = uart_get_baud_rate(uport, termios, oldtermios, 200, 4000000);

	/* Temporary remapping  200 BAUD to 3.2 mbps */
	if (bps == 200)
		bps = 3200000;

	uport->uartclk = clk_get_rate(msm_uport->clk);
	if (!uport->uartclk)
		msm_hs_set_std_bps_locked(uport, bps);
	else
		msm_hs_set_bps_locked(uport, bps);

	data = msm_hs_read(uport, UART_DM_MR2);
	data &= ~UARTDM_MR2_PARITY_MODE_BMSK;
	/* set parity */
	if (PARENB == (c_cflag & PARENB)) {
		if (PARODD == (c_cflag & PARODD)) {
			data |= ODD_PARITY;
		} else if (CMSPAR == (c_cflag & CMSPAR)) {
			data |= SPACE_PARITY;
		} else {
			data |= EVEN_PARITY;
		}
	}

	/* Set bits per char */
	data &= ~UARTDM_MR2_BITS_PER_CHAR_BMSK;

	switch (c_cflag & CSIZE) {
	case CS5:
		data |= FIVE_BPC;
		break;
	case CS6:
		data |= SIX_BPC;
		break;
	case CS7:
		data |= SEVEN_BPC;
		break;
	default:
		data |= EIGHT_BPC;
		break;
	}
	/* stop bits */
	if (c_cflag & CSTOPB) {
		data |= STOP_BIT_TWO;
	} else {
		/* otherwise 1 stop bit */
		data |= STOP_BIT_ONE;
	}
	data |= UARTDM_MR2_ERROR_MODE_BMSK;
	/* write parity/bits per char/stop bit configuration */
	msm_hs_write(uport, UART_DM_MR2, data);

	uport->ignore_status_mask = termios->c_iflag & INPCK;
	uport->ignore_status_mask |= termios->c_iflag & IGNPAR;
	uport->ignore_status_mask |= termios->c_iflag & IGNBRK;

	uport->read_status_mask = (termios->c_cflag & CREAD);


	/* Set Transmit software time out */
	uart_update_timeout(uport, c_cflag, bps);

	msm_hs_write(uport, UART_DM_CR, RESET_RX);
	msm_hs_write(uport, UART_DM_CR, RESET_TX);
	/* Issue TX BAM Start IFC command */
	msm_hs_write(uport, UART_DM_CR, START_TX_BAM_IFC);

	if (msm_uport->rx.flush == FLUSH_NONE) {
		wake_lock(&msm_uport->rx.wake_lock);
		msm_uport->rx.flush = FLUSH_DATA_INVALID;
		/*
		 * Before using dmov APIs make sure that
		 * previous writel are completed. Hence
		 * dsb requires here.
		 */
		mb();
		if (is_blsp_uart(msm_uport)) {
			if (msm_uport->rx_bam_inprogress)
				ret = wait_event_timeout(msm_uport->rx.wait,
					msm_uport->rx_bam_inprogress == false,
					RX_FLUSH_COMPLETE_TIMEOUT);
			ret = sps_rx_disconnect(sps_pipe_handle);
			if (ret)
				MSM_HS_ERR("%s(): sps_disconnect failed\n",
							__func__);
			msm_hs_spsconnect_rx(uport);
			msm_uport->rx.flush = FLUSH_IGNORE;
			msm_serial_hs_rx_tlet((unsigned long) &rx->tlet);
		} else {
			msm_uport->rx_discard_flush_issued = true;
			/* do discard flush */
			msm_dmov_flush(msm_uport->dma_rx_channel, 0);
			MSM_HS_DBG("%s(): wainting for flush completion.\n",
								__func__);
			ret = wait_event_timeout(msm_uport->rx.wait,
				msm_uport->rx_discard_flush_issued == false,
				RX_FLUSH_COMPLETE_TIMEOUT);
			if (!ret)
				MSM_HS_ERR("%s(): Discard flush pending.\n",
								__func__);
		}
	}

	/* Configure HW flow control
	 * UART Core would see status of CTS line when it is sending data
	 * to remote uart to confirm that it can receive or not.
	 * UART Core would trigger RFR if it is not having any space with
	 * RX FIFO.
	 */
	/* Pulling RFR line high */
	msm_hs_write(uport, UART_DM_CR, RFR_LOW);
	data = msm_hs_read(uport, UART_DM_MR1);
	data &= ~(UARTDM_MR1_CTS_CTL_BMSK | UARTDM_MR1_RX_RDY_CTL_BMSK);
		data |= UARTDM_MR1_CTS_CTL_BMSK;
		data |= UARTDM_MR1_RX_RDY_CTL_BMSK;
	msm_hs_write(uport, UART_DM_MR1, data);

	msm_hs_write(uport, UART_DM_IMR, msm_uport->imr_reg);
	mb();
	mutex_unlock(&msm_uport->clk_mutex);
	MSM_HS_DBG("Exit %s\n", __func__);
	dump_uart_hs_registers(msm_uport);
}

/*
 *  Standard API, Transmitter
 *  Any character in the transmit shift register is sent
 */
unsigned int msm_hs_tx_empty(struct uart_port *uport)
{
	unsigned int data;
	unsigned int ret = 0;
	struct msm_hs_port *msm_uport = UARTDM_TO_MSM(uport);

	msm_hs_clock_vote(msm_uport);
	data = msm_hs_read(uport, UART_DM_SR);
	msm_hs_clock_unvote(msm_uport);
	MSM_HS_DBG("%s(): SR Reg Read 0x%x", __func__, data);

	if (data & UARTDM_SR_TXEMT_BMSK)
		ret = TIOCSER_TEMT;

	return ret;
}
EXPORT_SYMBOL(msm_hs_tx_empty);

/*
 *  Standard API, Stop transmitter.
 *  Any character in the transmit shift register is sent as
 *  well as the current data mover transfer .
 */
static void msm_hs_stop_tx_locked(struct uart_port *uport)
{
	struct msm_hs_port *msm_uport = UARTDM_TO_MSM(uport);

	msm_uport->tx.tx_ready_int_en = 0;
}

/* Disconnect BAM RX Endpoint Pipe Index from workqueue context*/
static void hsuart_disconnect_rx_endpoint_work(struct work_struct *w)
{
	struct msm_hs_port *msm_uport = container_of(w, struct msm_hs_port,
						disconnect_rx_endpoint);
	struct msm_hs_rx *rx = &msm_uport->rx;
	struct sps_pipe *sps_pipe_handle = rx->prod.pipe_handle;
	int ret = 0;

	ret = sps_rx_disconnect(sps_pipe_handle);
	if (ret)
		MSM_HS_ERR("%s(): sps_disconnect failed\n", __func__);

	wake_lock_timeout(&msm_uport->rx.wake_lock, HZ / 2);
	msm_uport->rx.flush = FLUSH_SHUTDOWN;
	MSM_HS_DBG("%s: Calling Completion\n", __func__);
	wake_up(&msm_uport->bam_disconnect_wait);
	MSM_HS_DBG("%s: Done Completion\n", __func__);
	wake_up(&msm_uport->rx.wait);
}

/*
 *  Standard API, Stop receiver as soon as possible.
 *
 *  Function immediately terminates the operation of the
 *  channel receiver and any incoming characters are lost. None
 *  of the receiver status bits are affected by this command and
 *  characters that are already in the receive FIFO there.
 */
static void msm_hs_stop_rx_locked(struct uart_port *uport)
{
	struct msm_hs_port *msm_uport = UARTDM_TO_MSM(uport);
	unsigned int data;

	MSM_HS_DBG("In %s():\n", __func__);
	if (msm_uport->clk_state != MSM_HS_CLK_OFF) {
		/* disable dlink */
		data = msm_hs_read(uport, UART_DM_DMEN);
		if (is_blsp_uart(msm_uport))
			data &= ~UARTDM_RX_BAM_ENABLE_BMSK;
		else
			data &= ~UARTDM_RX_DM_EN_BMSK;
		msm_hs_write(uport, UART_DM_DMEN, data);

		/* calling DMOV or CLOCK API. Hence mb() */
		mb();
	}
	/* Disable the receiver */
	if (msm_uport->rx.flush == FLUSH_NONE) {
		wake_lock(&msm_uport->rx.wake_lock);
		if (is_blsp_uart(msm_uport)) {
			msm_uport->rx.flush = FLUSH_STOP;
			/* workqueue for BAM rx endpoint disconnect */
			queue_work(msm_uport->hsuart_wq,
				&msm_uport->disconnect_rx_endpoint);
		} else {
			/* do discard flush */
			msm_dmov_flush(msm_uport->dma_rx_channel, 0);
		}
	}
	if (!is_blsp_uart(msm_uport) && msm_uport->rx.flush != FLUSH_SHUTDOWN)
		msm_uport->rx.flush = FLUSH_STOP;

}

/*  Transmit the next chunk of data */
static void msm_hs_submit_tx_locked(struct uart_port *uport)
{
	int left;
	int tx_count;
	int aligned_tx_count;
	dma_addr_t src_addr;
	dma_addr_t aligned_src_addr;
	u32 flags = SPS_IOVEC_FLAG_EOT | SPS_IOVEC_FLAG_INT;
	struct msm_hs_port *msm_uport = UARTDM_TO_MSM(uport);
	struct msm_hs_tx *tx = &msm_uport->tx;
	struct circ_buf *tx_buf = &msm_uport->uport.state->xmit;
	struct sps_pipe *sps_pipe_handle;

	if (uart_circ_empty(tx_buf) || uport->state->port.tty->stopped) {
		msm_hs_stop_tx_locked(uport);
		if (msm_uport->clk_state == MSM_HS_CLK_REQUEST_OFF) {
			MSM_HS_DBG("%s(): Clock off requested calling WQ",
								__func__);
			queue_work(msm_uport->hsuart_wq,
						&msm_uport->clock_off_w);
		}
		return;
	}

	tx->dma_in_flight = 1;

	tx_count = uart_circ_chars_pending(tx_buf);

	if (UARTDM_TX_BUF_SIZE < tx_count)
		tx_count = UARTDM_TX_BUF_SIZE;

	left = UART_XMIT_SIZE - tx_buf->tail;

	if (tx_count > left)
		tx_count = left;
	MSM_HS_DBG("%s(): [UART_TX]<%d>\n", __func__, tx_count);
	hex_dump_ipc("HSUART write: ", &tx_buf->buf[tx_buf->tail], tx_count);
	src_addr = tx->dma_base + tx_buf->tail;
	/* Mask the src_addr to align on a cache
	 * and add those bytes to tx_count */
	aligned_src_addr = src_addr & ~(dma_get_cache_alignment() - 1);
	aligned_tx_count = tx_count + src_addr - aligned_src_addr;

	dma_sync_single_for_device(uport->dev, aligned_src_addr,
			aligned_tx_count, DMA_TO_DEVICE);

	if (is_blsp_uart(msm_uport))
		tx->tx_count = tx_count;
	else {
		tx->command_ptr->num_rows =
				(((tx_count + 15) >> 4) << 16) |
				((tx_count + 15) >> 4);
		tx->command_ptr->src_row_addr = src_addr;

		dma_sync_single_for_device(uport->dev, tx->mapped_cmd_ptr,
				sizeof(dmov_box), DMA_TO_DEVICE);

		*tx->command_ptr_ptr = CMD_PTR_LP |
				DMOV_CMD_ADDR(tx->mapped_cmd_ptr);
		/* Save tx_count to use in Callback */
		tx->tx_count = tx_count;
		msm_hs_write(uport, UART_DM_NCF_TX, tx_count);
		msm_uport->imr_reg &= ~UARTDM_ISR_TX_READY_BMSK;
		msm_hs_write(uport, UART_DM_IMR, msm_uport->imr_reg);
		/* Calling next DMOV API. Hence mb() here. */
		mb();

	}

	msm_uport->tx.flush = FLUSH_NONE;

	if (is_blsp_uart(msm_uport)) {
		sps_pipe_handle = tx->cons.pipe_handle;
		/* Queue transfer request to SPS */
		sps_transfer_one(sps_pipe_handle, src_addr, tx_count,
					msm_uport, flags);
	} else {
		dma_sync_single_for_device(uport->dev, tx->mapped_cmd_ptr_ptr,
			sizeof(u32), DMA_TO_DEVICE);

		msm_dmov_enqueue_cmd(msm_uport->dma_tx_channel, &tx->xfer);

	}
	MSM_HS_DBG("%s:Enqueue Tx Cmd\n", __func__);
	dump_uart_hs_registers(msm_uport);
}

/* Start to receive the next chunk of data */
static void msm_hs_start_rx_locked(struct uart_port *uport)
{
	struct msm_hs_port *msm_uport = UARTDM_TO_MSM(uport);
	struct msm_hs_rx *rx = &msm_uport->rx;
	struct sps_pipe *sps_pipe_handle;
	u32 flags = SPS_IOVEC_FLAG_INT;
	unsigned int buffer_pending = msm_uport->rx.buffer_pending;
	unsigned int data;

	if (msm_uport->clk_state != MSM_HS_CLK_ON) {
		MSM_HS_WARN("%s: Failed.Clocks are OFF\n", __func__);
		return;
	}
	if (rx->rx_cmd_exec) {
		MSM_HS_DBG("%s: Rx Cmd got executed, wait for rx_tlet\n",
								 __func__);
		rx->flush = FLUSH_IGNORE;
		return;
	}
	msm_uport->rx.buffer_pending = 0;
	if (buffer_pending && hs_serial_debug_mask)
		MSM_HS_ERR("Error: rx started in buffer state = %x",
		       buffer_pending);

	msm_hs_write(uport, UART_DM_CR, RESET_STALE_INT);
	msm_hs_write(uport, UART_DM_DMRX, UARTDM_RX_BUF_SIZE);
	msm_hs_write(uport, UART_DM_CR, STALE_EVENT_ENABLE);
	/*
	 * Enable UARTDM Rx Interface as previously it has been
	 * disable in set_termios before configuring baud rate.
	 */
	data = msm_hs_read(uport, UART_DM_DMEN);
	if (is_blsp_uart(msm_uport)) {
		/* Enable UARTDM Rx BAM Interface */
		data |= UARTDM_RX_BAM_ENABLE_BMSK;
	} else {
		data |= UARTDM_RX_DM_EN_BMSK;
	}

	msm_hs_write(uport, UART_DM_DMEN, data);
	msm_hs_write(uport, UART_DM_IMR, msm_uport->imr_reg);
	/* Calling next DMOV API. Hence mb() here. */
	mb();

	if (is_blsp_uart(msm_uport)) {
		/*
		 * RX-transfer will be automatically re-activated
		 * after last data of previous transfer was read.
		 */
		data = (RX_STALE_AUTO_RE_EN | RX_TRANS_AUTO_RE_ACTIVATE |
					RX_DMRX_CYCLIC_EN);
		msm_hs_write(uport, UART_DM_RX_TRANS_CTRL, data);
		/* Issue RX BAM Start IFC command */
		msm_hs_write(uport, UART_DM_CR, START_RX_BAM_IFC);
		mb();
	}

	msm_uport->rx.flush = FLUSH_NONE;

	if (is_blsp_uart(msm_uport)) {
		msm_uport->rx_bam_inprogress = true;
		sps_pipe_handle = rx->prod.pipe_handle;
		/* Queue transfer request to SPS */
		sps_transfer_one(sps_pipe_handle, rx->rbuffer,
			UARTDM_RX_BUF_SIZE, msm_uport, flags);
		msm_uport->rx_bam_inprogress = false;
		msm_uport->rx.rx_cmd_queued = true;
		wake_up(&msm_uport->rx.wait);
	}
	MSM_HS_DBG("%s:Enqueue Rx Cmd\n", __func__);
	dump_uart_hs_registers(msm_uport);
}

static void flip_insert_work(struct work_struct *work)
{
	unsigned long flags;
	int retval;
	struct msm_hs_port *msm_uport =
		container_of(work, struct msm_hs_port,
			     rx.flip_insert_work.work);
	struct tty_struct *tty = msm_uport->uport.state->port.tty;

	spin_lock_irqsave(&msm_uport->uport.lock, flags);
	if (msm_uport->rx.buffer_pending == NONE_PENDING) {
		if (hs_serial_debug_mask)
			MSM_HS_ERR("Error: No buffer pending in %s",
			       __func__);
		return;
	}
	if (msm_uport->rx.buffer_pending & FIFO_OVERRUN) {
		retval = tty_insert_flip_char(tty, 0, TTY_OVERRUN);
		if (retval)
			msm_uport->rx.buffer_pending &= ~FIFO_OVERRUN;
	}
	if (msm_uport->rx.buffer_pending & PARITY_ERROR) {
		retval = tty_insert_flip_char(tty, 0, TTY_PARITY);
		if (retval)
			msm_uport->rx.buffer_pending &= ~PARITY_ERROR;
	}
	if (msm_uport->rx.buffer_pending & CHARS_NORMAL) {
		int rx_count, rx_offset;
		rx_count = (msm_uport->rx.buffer_pending & 0xFFFF0000) >> 16;
		rx_offset = (msm_uport->rx.buffer_pending & 0xFFD0) >> 5;
		retval = tty_insert_flip_string(tty, msm_uport->rx.buffer +
						rx_offset, rx_count);
		msm_uport->rx.buffer_pending &= (FIFO_OVERRUN |
						 PARITY_ERROR);
		if (retval != rx_count)
			msm_uport->rx.buffer_pending |= CHARS_NORMAL |
				retval << 8 | (rx_count - retval) << 16;
	}
	if (msm_uport->rx.buffer_pending)
		schedule_delayed_work(&msm_uport->rx.flip_insert_work,
				      msecs_to_jiffies(RETRY_TIMEOUT));
	else
		if ((msm_uport->clk_state == MSM_HS_CLK_ON) &&
		    (msm_uport->rx.flush <= FLUSH_IGNORE)) {
		MSM_HS_WARN("Pending buffers cleared,restarting\n");
			msm_hs_start_rx_locked(&msm_uport->uport);
		}
	spin_unlock_irqrestore(&msm_uport->uport.lock, flags);
	tty_flip_buffer_push(tty);
}

static void msm_serial_hs_rx_tlet(unsigned long tlet_ptr)
{
	int retval;
	int rx_count = 0;
	unsigned long status;
	unsigned long flags;
	unsigned int error_f = 0;
	struct uart_port *uport;
	struct msm_hs_port *msm_uport;
	unsigned int flush;
	struct tty_struct *tty;
	struct sps_event_notify *notify;
	struct msm_hs_rx *rx;
	struct sps_pipe *sps_pipe_handle;
	u32 sps_flags = SPS_IOVEC_FLAG_INT;

	msm_uport = container_of((struct tasklet_struct *)tlet_ptr,
				 struct msm_hs_port, rx.tlet);
	uport = &msm_uport->uport;
	tty = uport->state->port.tty;
	notify = &msm_uport->notify;
	rx = &msm_uport->rx;

	msm_uport->rx.rx_cmd_queued = false;
	msm_uport->rx.rx_cmd_exec = false;

	status = msm_hs_read(uport, UART_DM_SR);

	spin_lock_irqsave(&uport->lock, flags);

	if (!is_blsp_uart(msm_uport))
		msm_hs_write(uport, UART_DM_CR, STALE_EVENT_DISABLE);

	MSM_HS_DBG("In %s\n", __func__);
	dump_uart_hs_registers(msm_uport);

	/* overflow is not connect to data in a FIFO */
	if (unlikely((status & UARTDM_SR_OVERRUN_BMSK) &&
		     (uport->read_status_mask & CREAD))) {
		retval = tty_insert_flip_char(tty, 0, TTY_OVERRUN);
		if (!retval)
			msm_uport->rx.buffer_pending |= TTY_OVERRUN;
		uport->icount.buf_overrun++;
		error_f = 1;
	}

	if (!(uport->ignore_status_mask & INPCK))
		status = status & ~(UARTDM_SR_PAR_FRAME_BMSK);

	if (unlikely(status & UARTDM_SR_PAR_FRAME_BMSK)) {
		/* Can not tell difference between parity & frame error */
		if (hs_serial_debug_mask)
			MSM_HS_WARN("msm_serial_hs: parity error\n");
		uport->icount.parity++;
		error_f = 1;
		if (!(uport->ignore_status_mask & IGNPAR)) {
			retval = tty_insert_flip_char(tty, 0, TTY_PARITY);
			if (!retval)
				msm_uport->rx.buffer_pending |= TTY_PARITY;
		}
	}

	if (unlikely(status & UARTDM_SR_RX_BREAK_BMSK)) {
		if (hs_serial_debug_mask)
			MSM_HS_WARN("msm_serial_hs: Rx break\n");
		uport->icount.brk++;
		error_f = 1;
		if (!(uport->ignore_status_mask & IGNBRK)) {
			retval = tty_insert_flip_char(tty, 0, TTY_BREAK);
			if (!retval)
				msm_uport->rx.buffer_pending |= TTY_BREAK;
		}
	}

	if (error_f) {
		if (msm_uport->clk_state == MSM_HS_CLK_ON)
			msm_hs_write(uport, UART_DM_CR, RESET_ERROR_STATUS);
		else
			MSM_HS_WARN("%s: Failed.Clocks are OFF\n", __func__);
	}
	flush = msm_uport->rx.flush;
	if (flush == FLUSH_IGNORE)
		if (!msm_uport->rx.buffer_pending) {
			MSM_HS_DBG("%s: calling start_rx_locked\n", __func__);
			msm_hs_start_rx_locked(uport);
		}
	if (flush >= FLUSH_DATA_INVALID)
		goto out;

	if (is_blsp_uart(msm_uport)) {
		rx_count = msm_uport->rx_count_callback;
	} else {
		if (msm_uport->clk_state == MSM_HS_CLK_ON) {
			rx_count = msm_hs_read(uport, UART_DM_RX_TOTAL_SNAP);
			/* order the read of rx.buffer */
			rmb();
		} else
			MSM_HS_WARN("%s: Failed.Clocks are OFF\n", __func__);
	}

	MSM_HS_DBG("%s():[UART_RX]<%d>\n", __func__, rx_count);
	hex_dump_ipc("HSUART Read: ", msm_uport->rx.buffer, rx_count);
	if (0 != (uport->read_status_mask & CREAD)) {
		retval = tty_insert_flip_string(tty, msm_uport->rx.buffer,
						rx_count);
		if (retval != rx_count) {
			MSM_HS_DBG("%s(): retval %d rx_count %d", __func__,
					retval, rx_count);
			msm_uport->rx.buffer_pending |= CHARS_NORMAL |
				retval << 5 | (rx_count - retval) << 16;
		}
	}
	if (!msm_uport->rx.buffer_pending && !msm_uport->rx.rx_cmd_queued) {
		if (is_blsp_uart(msm_uport)) {
			msm_uport->rx.flush = FLUSH_NONE;
			msm_uport->rx_bam_inprogress = true;
			sps_pipe_handle = rx->prod.pipe_handle;
			MSM_HS_DBG("Queing bam descriptor\n");
			/* Queue transfer request to SPS */
			sps_transfer_one(sps_pipe_handle, rx->rbuffer,
				UARTDM_RX_BUF_SIZE, msm_uport, sps_flags);
			msm_uport->rx_bam_inprogress = false;
			msm_uport->rx.rx_cmd_queued = true;
			wake_up(&msm_uport->rx.wait);

		} else
			msm_hs_start_rx_locked(uport);
	}
out:
	if (msm_uport->rx.buffer_pending) {
		MSM_HS_WARN("tty buffer exhausted.Stalling\n");
		schedule_delayed_work(&msm_uport->rx.flip_insert_work
				      , msecs_to_jiffies(RETRY_TIMEOUT));
	}
	/* release wakelock in 500ms, not immediately, because higher layers
	 * don't always take wakelocks when they should */
	wake_lock_timeout(&msm_uport->rx.wake_lock, HZ / 2);
	/* tty_flip_buffer_push() might call msm_hs_start(), so unlock */
	spin_unlock_irqrestore(&uport->lock, flags);
	if (flush < FLUSH_DATA_INVALID)
		tty_flip_buffer_push(tty);
}

/* Enable the transmitter Interrupt */
static void msm_hs_start_tx_locked(struct uart_port *uport )
{
	struct msm_hs_port *msm_uport = UARTDM_TO_MSM(uport);

	if (msm_uport->clk_state != MSM_HS_CLK_ON) {
		MSM_HS_WARN("%s: Failed.Clocks are OFF\n", __func__);
	}
	if (msm_uport->tx.tx_ready_int_en == 0) {
		if (!is_blsp_uart(msm_uport))
			msm_uport->tx.tx_ready_int_en = 1;
		if (msm_uport->tx.dma_in_flight == 0)
			msm_hs_submit_tx_locked(uport);
	}
}

/**
 * Callback notification from SPS driver
 *
 * This callback function gets triggered called from
 * SPS driver when requested SPS data transfer is
 * completed.
 *
 */

static void msm_hs_sps_tx_callback(struct sps_event_notify *notify)
{
	struct msm_hs_port *msm_uport =
		(struct msm_hs_port *)
		((struct sps_event_notify *)notify)->user;

	msm_uport->notify = *notify;
	MSM_HS_DBG("%s: ev_id=%d, addr=0x%x, size=0x%x, flags=0x%x, line=%d\n",
			__func__, notify->event_id,
	notify->data.transfer.iovec.addr,
	notify->data.transfer.iovec.size,
	notify->data.transfer.iovec.flags,
	msm_uport->uport.line);

	tasklet_schedule(&msm_uport->tx.tlet);
}

/*
 *  This routine is called when we are done with a DMA transfer
 *
 *  This routine is registered with Data mover when we set
 *  up a Data Mover transfer. It is called from Data mover ISR
 *  when the DMA transfer is done.
 */
static void msm_hs_dmov_tx_callback(struct msm_dmov_cmd *cmd_ptr,
					unsigned int result,
					struct msm_dmov_errdata *err)
{
	struct msm_hs_port *msm_uport;

	msm_uport = container_of(cmd_ptr, struct msm_hs_port, tx.xfer);
	if (msm_uport->tx.flush == FLUSH_STOP)
		/* DMA FLUSH unsuccesfful */
		WARN_ON(!(result & DMOV_RSLT_FLUSH));
	else
		/* DMA did not finish properly */
		WARN_ON(!(result & DMOV_RSLT_DONE));

	tasklet_schedule(&msm_uport->tx.tlet);
}

static void msm_serial_hs_tx_tlet(unsigned long tlet_ptr)
{
	unsigned long flags;
	struct msm_hs_port *msm_uport = container_of((struct tasklet_struct *)
				tlet_ptr, struct msm_hs_port, tx.tlet);
	struct uart_port *uport = &msm_uport->uport;
	struct circ_buf *tx_buf = &uport->state->xmit;
	struct msm_hs_tx *tx = &msm_uport->tx;

	/*
	 * Do the work buffer related work in BAM
	 * mode that is equivalent to legacy mode
	 */

	if (!msm_uport->tty_flush_receive)
		tx_buf->tail = (tx_buf->tail +
		tx->tx_count) & ~UART_XMIT_SIZE;
	else
		msm_uport->tty_flush_receive = false;

	tx->dma_in_flight = 0;

	uport->icount.tx += tx->tx_count;

	/*
	 * Calling to send next chunk of data
	 * If the circ buffer is empty, we stop
	 * If the clock off was requested, the clock
	 * off sequence is kicked off
	 */
	 msm_hs_submit_tx_locked(uport);

	if (uart_circ_chars_pending(tx_buf) < WAKEUP_CHARS)
		uart_write_wakeup(uport);

	spin_lock_irqsave(&(msm_uport->uport.lock), flags);
	if (msm_uport->tx.flush == FLUSH_STOP) {
		msm_uport->tx.flush = FLUSH_SHUTDOWN;
		wake_up(&msm_uport->tx.wait);
		spin_unlock_irqrestore(&(msm_uport->uport.lock), flags);
		return;
	}

	/* TX_READY_BMSK only if non BAM mode */
	if (!is_blsp_uart(msm_uport)) {
		msm_uport->imr_reg |= UARTDM_ISR_TX_READY_BMSK;
		msm_hs_write(&(msm_uport->uport), UART_DM_IMR,
					msm_uport->imr_reg);
		/* Calling clk API. Hence mb() requires. */
		mb();
	}

	spin_unlock_irqrestore(&(msm_uport->uport.lock), flags);
	MSM_HS_DBG("In %s()\n", __func__);
	dump_uart_hs_registers(msm_uport);
}

/**
 * Callback notification from SPS driver
 *
 * This callback function gets triggered called from
 * SPS driver when requested SPS data transfer is
 * completed.
 *
 */

static void msm_hs_sps_rx_callback(struct sps_event_notify *notify)
{

	struct msm_hs_port *msm_uport =
		(struct msm_hs_port *)
		((struct sps_event_notify *)notify)->user;
	struct uart_port *uport;
	unsigned long flags;

	uport = &(msm_uport->uport);
	msm_uport->notify = *notify;
	MSM_HS_DBG("%s: sps ev_id=%d, addr=0x%x, size=0x%x, flags=0x%x\n",
		__func__, notify->event_id,
		notify->data.transfer.iovec.addr,
		notify->data.transfer.iovec.size,
		notify->data.transfer.iovec.flags);

	if (msm_uport->rx.flush == FLUSH_NONE) {
		spin_lock_irqsave(&uport->lock, flags);
		msm_uport->rx_count_callback = notify->data.transfer.iovec.size;
		msm_uport->rx.rx_cmd_exec = true;
		spin_unlock_irqrestore(&uport->lock, flags);
		tasklet_schedule(&msm_uport->rx.tlet);
	}
}

/*
 * This routine is called when we are done with a DMA transfer or the
 * a flush has been sent to the data mover driver.
 *
 * This routine is registered with Data mover when we set up a Data Mover
 *  transfer. It is called from Data mover ISR when the DMA transfer is done.
 */
static void msm_hs_dmov_rx_callback(struct msm_dmov_cmd *cmd_ptr,
					unsigned int result,
					struct msm_dmov_errdata *err)
{
	struct msm_hs_port *msm_uport;
	struct uart_port *uport;
	unsigned long flags;

	msm_uport = container_of(cmd_ptr, struct msm_hs_port, rx.xfer);
	uport = &(msm_uport->uport);

	MSM_HS_DBG("%s(): called result:%x\n", __func__, result);
	if (!(result & DMOV_RSLT_ERROR)) {
		if (result & DMOV_RSLT_FLUSH) {
			if (msm_uport->rx_discard_flush_issued) {
				spin_lock_irqsave(&uport->lock, flags);
				msm_uport->rx_discard_flush_issued = false;
				spin_unlock_irqrestore(&uport->lock, flags);
				wake_up(&msm_uport->rx.wait);
			}
		}
	}

	tasklet_schedule(&msm_uport->rx.tlet);
}

/*
 *  Standard API, Current states of modem control inputs
 *
 * Since CTS can be handled entirely by HARDWARE we always
 * indicate clear to send and count on the TX FIFO to block when
 * it fills up.
 *
 * - TIOCM_DCD
 * - TIOCM_CTS
 * - TIOCM_DSR
 * - TIOCM_RI
 *  (Unsupported) DCD and DSR will return them high. RI will return low.
 */
static unsigned int msm_hs_get_mctrl_locked(struct uart_port *uport)
{
	return TIOCM_DSR | TIOCM_CAR | TIOCM_CTS;
}

/*
 *  Standard API, Set or clear RFR_signal
 *
 * Set RFR high, (Indicate we are not ready for data), we disable auto
 * ready for receiving and then set RFR_N high. To set RFR to low we just turn
 * back auto ready for receiving and it should lower RFR signal
 * when hardware is ready
 */
void msm_hs_set_mctrl_locked(struct uart_port *uport,
				    unsigned int mctrl)
{
	unsigned int set_rts;
	unsigned int data;
	struct msm_hs_port *msm_uport = UARTDM_TO_MSM(uport);

	if (msm_uport->clk_state != MSM_HS_CLK_ON) {
		MSM_HS_WARN("%s:Failed.Clocks are OFF\n", __func__);
		return;
	}
	/* RTS is active low */
	set_rts = TIOCM_RTS & mctrl ? 0 : 1;

	data = msm_hs_read(uport, UART_DM_MR1);
	if (set_rts) {
		/*disable auto ready-for-receiving */
		data &= ~UARTDM_MR1_RX_RDY_CTL_BMSK;
		msm_hs_write(uport, UART_DM_MR1, data);
		/* set RFR_N to high */
		msm_hs_write(uport, UART_DM_CR, RFR_HIGH);
	} else {
		/* Enable auto ready-for-receiving */
		data |= UARTDM_MR1_RX_RDY_CTL_BMSK;
		msm_hs_write(uport, UART_DM_MR1, data);
	}
	mb();
}

void msm_hs_set_mctrl(struct uart_port *uport,
				    unsigned int mctrl)
{
	unsigned long flags;
	struct msm_hs_port *msm_uport = UARTDM_TO_MSM(uport);

	msm_hs_clock_vote(msm_uport);
	spin_lock_irqsave(&uport->lock, flags);
	msm_hs_set_mctrl_locked(uport, mctrl);
	spin_unlock_irqrestore(&uport->lock, flags);
	msm_hs_clock_unvote(msm_uport);
}
EXPORT_SYMBOL(msm_hs_set_mctrl);

/* Standard API, Enable modem status (CTS) interrupt  */
static void msm_hs_enable_ms_locked(struct uart_port *uport)
{
	struct msm_hs_port *msm_uport = UARTDM_TO_MSM(uport);

	if (msm_uport->clk_state != MSM_HS_CLK_ON) {
		MSM_HS_WARN("%s:Failed.Clocks are OFF\n", __func__);
		return;
	}

	/* Enable DELTA_CTS Interrupt */
	msm_uport->imr_reg |= UARTDM_ISR_DELTA_CTS_BMSK;
	msm_hs_write(uport, UART_DM_IMR, msm_uport->imr_reg);
	mb();

}

static void msm_hs_flush_buffer(struct uart_port *uport)
{
	struct msm_hs_port *msm_uport = UARTDM_TO_MSM(uport);

	if (msm_uport->tx.dma_in_flight)
		msm_uport->tty_flush_receive = true;
}

/*
 *  Standard API, Break Signal
 *
 * Control the transmission of a break signal. ctl eq 0 => break
 * signal terminate ctl ne 0 => start break signal
 */
static void msm_hs_break_ctl(struct uart_port *uport, int ctl)
{
	unsigned long flags;
	struct msm_hs_port *msm_uport = UARTDM_TO_MSM(uport);

	if (msm_uport->clk_state != MSM_HS_CLK_ON) {
		MSM_HS_WARN("%s: Failed.Clocks are OFF\n", __func__);
		return;
	}

	spin_lock_irqsave(&uport->lock, flags);
	msm_hs_write(uport, UART_DM_CR, ctl ? START_BREAK : STOP_BREAK);
	mb();
	spin_unlock_irqrestore(&uport->lock, flags);
}

static void msm_hs_config_port(struct uart_port *uport, int cfg_flags)
{
	unsigned long flags;
	struct msm_hs_port *msm_uport = UARTDM_TO_MSM(uport);

	if (cfg_flags & UART_CONFIG_TYPE) {
		uport->type = PORT_MSM;
		msm_hs_request_port(uport);
	}

	if (is_gsbi_uart(msm_uport)) {
		if (msm_uport->pclk)
			clk_prepare_enable(msm_uport->pclk);
		spin_lock_irqsave(&uport->lock, flags);
		iowrite32(GSBI_PROTOCOL_UART, msm_uport->mapped_gsbi +
			  GSBI_CONTROL_ADDR);
		spin_unlock_irqrestore(&uport->lock, flags);
		if (msm_uport->pclk)
			clk_disable_unprepare(msm_uport->pclk);
	}
}

/*  Handle CTS changes (Called from interrupt handler) */
static void msm_hs_handle_delta_cts_locked(struct uart_port *uport)
{
	struct msm_hs_port *msm_uport = UARTDM_TO_MSM(uport);

	if (msm_uport->clk_state != MSM_HS_CLK_ON) {
		MSM_HS_WARN("%s: Failed.Clocks are OFF\n", __func__);
		return;
	}
	/* clear interrupt */
	msm_hs_write(uport, UART_DM_CR, RESET_CTS);
	/* Calling CLOCK API. Hence mb() requires here. */
	mb();
	uport->icount.cts++;

	/* clear the IOCTL TIOCMIWAIT if called */
	wake_up_interruptible(&uport->state->port.delta_msr_wait);
}

/* check if the TX path is flushed, and if so clock off
 * returns 0 did not clock off, need to retry (still sending final byte)
 *        -1 did not clock off, do not retry
 *         1 if we clocked off
 */
static int msm_hs_check_clock_off(struct uart_port *uport)
{
	unsigned long sr_status;
	unsigned long flags;
	unsigned int data;
	struct msm_hs_port *msm_uport = UARTDM_TO_MSM(uport);
	struct circ_buf *tx_buf = &uport->state->xmit;

	mutex_lock(&msm_uport->clk_mutex);
	spin_lock_irqsave(&uport->lock, flags);

	MSM_HS_DBG("In %s:\n", __func__);
	/* Cancel if tx tty buffer is not empty, dma is in flight,
	 * or tx fifo is not empty
	 */
	if (msm_uport->clk_state != MSM_HS_CLK_REQUEST_OFF ||
	    !uart_circ_empty(tx_buf) || msm_uport->tx.dma_in_flight ||
	    msm_uport->imr_reg & UARTDM_ISR_TXLEV_BMSK) {
		spin_unlock_irqrestore(&uport->lock, flags);
		mutex_unlock(&msm_uport->clk_mutex);
		if (msm_uport->clk_state == MSM_HS_CLK_REQUEST_OFF) {
			msm_uport->clk_state = MSM_HS_CLK_ON;
			/* Pulling RFR line high */
			msm_hs_write(uport, UART_DM_CR, RFR_LOW);
			/* Enable auto RFR */
			data = msm_hs_read(uport, UART_DM_MR1);
			data |= UARTDM_MR1_RX_RDY_CTL_BMSK;
			msm_hs_write(uport, UART_DM_MR1, data);
			mb();
		}
		MSM_HS_DBG("%s(): clkstate %d", __func__, msm_uport->clk_state);
		return -1;
	}

	/* Make sure the uart is finished with the last byte,
	 * use BFamily Register
	 */
	sr_status = msm_hs_read(uport, UART_DM_SR);
	if (!(sr_status & UARTDM_SR_TXEMT_BMSK)) {
		spin_unlock_irqrestore(&uport->lock, flags);
		mutex_unlock(&msm_uport->clk_mutex);
		MSM_HS_DBG("%s(): SR TXEMT fail %lx", __func__, sr_status);
		return 0;  /* retry */
	}

	if (msm_uport->rx.flush != FLUSH_SHUTDOWN) {
		if (msm_uport->rx.flush == FLUSH_NONE) {
			msm_hs_stop_rx_locked(uport);
			if (!is_blsp_uart(msm_uport))
				msm_uport->rx_discard_flush_issued = true;
		}

		MSM_HS_DBG("%s: rx.flush %d clk_state %d\n", __func__,
			msm_uport->rx.flush, msm_uport->clk_state);
		spin_unlock_irqrestore(&uport->lock, flags);
		mutex_unlock(&msm_uport->clk_mutex);
		return 0;  /* come back later to really clock off */
	}

	spin_unlock_irqrestore(&uport->lock, flags);

	/* Pulling RFR line high */
	msm_hs_write(uport, UART_DM_CR, RFR_LOW);
	/* Enable auto RFR */
	data = msm_hs_read(uport, UART_DM_MR1);
	data |= UARTDM_MR1_RX_RDY_CTL_BMSK;
	msm_hs_write(uport, UART_DM_MR1, data);
	mb();

	/* we really want to clock off */
	msm_hs_clock_unvote(msm_uport);

	spin_lock_irqsave(&uport->lock, flags);
	if (use_low_power_wakeup(msm_uport)) {
		msm_uport->wakeup.ignore = 1;
		enable_irq(msm_uport->wakeup.irq);
	}
	wake_unlock(&msm_uport->dma_wake_lock);

	spin_unlock_irqrestore(&uport->lock, flags);

	mutex_unlock(&msm_uport->clk_mutex);

	return 1;
}

static void hsuart_clock_off_work(struct work_struct *w)
{
	struct msm_hs_port *msm_uport = container_of(w, struct msm_hs_port,
							clock_off_w);
	struct uart_port *uport = &msm_uport->uport;

	if (!msm_hs_check_clock_off(uport)) {
		hrtimer_start(&msm_uport->clk_off_timer,
				msm_uport->clk_off_delay,
				HRTIMER_MODE_REL);
	}
}

static enum hrtimer_restart msm_hs_clk_off_retry(struct hrtimer *timer)
{
	struct msm_hs_port *msm_uport = container_of(timer, struct msm_hs_port,
							clk_off_timer);

	queue_work(msm_uport->hsuart_wq, &msm_uport->clock_off_w);
	return HRTIMER_NORESTART;
}

static irqreturn_t msm_hs_isr(int irq, void *dev)
{
	unsigned long flags;
	unsigned long isr_status;
	struct msm_hs_port *msm_uport = (struct msm_hs_port *)dev;
	struct uart_port *uport = &msm_uport->uport;
	struct circ_buf *tx_buf = &uport->state->xmit;
	struct msm_hs_tx *tx = &msm_uport->tx;
	struct msm_hs_rx *rx = &msm_uport->rx;

	spin_lock_irqsave(&uport->lock, flags);

	isr_status = msm_hs_read(uport, UART_DM_MISR);
	MSM_HS_DBG("%s:UART_DM_MISR %lx", __func__, isr_status);
	dump_uart_hs_registers(msm_uport);

	/* Uart RX starting */
	if (isr_status & UARTDM_ISR_RXLEV_BMSK) {
		wake_lock(&rx->wake_lock);  /* hold wakelock while rx dma */
		MSM_HS_DBG("%s:UARTDM_ISR_RXLEV_BMSK\n", __func__);
		msm_uport->imr_reg &= ~UARTDM_ISR_RXLEV_BMSK;
		msm_hs_write(uport, UART_DM_IMR, msm_uport->imr_reg);
		/* Complete device write for IMR. Hence mb() requires. */
		mb();
	}
	/* Stale rx interrupt */
	if (isr_status & UARTDM_ISR_RXSTALE_BMSK) {
		msm_hs_write(uport, UART_DM_CR, STALE_EVENT_DISABLE);
		msm_hs_write(uport, UART_DM_CR, RESET_STALE_INT);
		/*
		 * Complete device write before calling DMOV API. Hence
		 * mb() requires here.
		 */
		mb();
		MSM_HS_DBG("%s:Stal Interrupt\n", __func__);

		if (!is_blsp_uart(msm_uport) && (rx->flush == FLUSH_NONE)) {
			rx->flush = FLUSH_DATA_READY;
			msm_dmov_flush(msm_uport->dma_rx_channel, 1);
		}
	}
	/* tx ready interrupt */
	if (isr_status & UARTDM_ISR_TX_READY_BMSK) {
		MSM_HS_DBG("%s: ISR_TX_READY Interrupt\n", __func__);
		/* Clear  TX Ready */
		msm_hs_write(uport, UART_DM_CR, CLEAR_TX_READY);

		if (msm_uport->clk_state == MSM_HS_CLK_REQUEST_OFF) {
			msm_uport->imr_reg |= UARTDM_ISR_TXLEV_BMSK;
			msm_hs_write(uport, UART_DM_IMR, msm_uport->imr_reg);
		}
		/*
		 * Complete both writes before starting new TX.
		 * Hence mb() requires here.
		 */
		mb();
		/* Complete DMA TX transactions and submit new transactions */

		/* Do not update tx_buf.tail if uart_flush_buffer already
		 * called in serial core
		 */
		if (!msm_uport->tty_flush_receive)
			tx_buf->tail = (tx_buf->tail +
					tx->tx_count) & ~UART_XMIT_SIZE;
		else
			msm_uport->tty_flush_receive = false;

		tx->dma_in_flight = 0;

		uport->icount.tx += tx->tx_count;
		if (tx->tx_ready_int_en)
			msm_hs_submit_tx_locked(uport);

		if (uart_circ_chars_pending(tx_buf) < WAKEUP_CHARS)
			uart_write_wakeup(uport);
	}
	if (isr_status & UARTDM_ISR_TXLEV_BMSK) {
		/* TX FIFO is empty */
		msm_uport->imr_reg &= ~UARTDM_ISR_TXLEV_BMSK;
		msm_hs_write(uport, UART_DM_IMR, msm_uport->imr_reg);
		/*
		 * Complete device write before starting clock_off request.
		 * Hence mb() requires here.
		 */
		mb();
		queue_work(msm_uport->hsuart_wq, &msm_uport->clock_off_w);
	}

	/* Change in CTS interrupt */
	if (isr_status & UARTDM_ISR_DELTA_CTS_BMSK)
		msm_hs_handle_delta_cts_locked(uport);

	spin_unlock_irqrestore(&uport->lock, flags);

	return IRQ_HANDLED;
}

/* The following two functions provide interfaces to get the underlying
 * port structure (struct uart_port or struct msm_hs_port) given
 * the port index. msm_hs_get_uart port is called by clients.
 * The function msm_hs_get_hs_port is for internal use
 */

struct uart_port *msm_hs_get_uart_port(int port_index)
{
	struct uart_state *state = msm_hs_driver.state + port_index;

	/* The uart_driver structure stores the states in an array.
	 * Thus the corresponding offset from the drv->state returns
	 * the state for the uart_port that is requested
	 */
	if (port_index == state->uart_port->line)
		return state->uart_port;

	return NULL;
}
EXPORT_SYMBOL(msm_hs_get_uart_port);

static struct msm_hs_port *msm_hs_get_hs_port(int port_index)
{
	struct uart_port *uport = msm_hs_get_uart_port(port_index);
	if (uport)
		return UARTDM_TO_MSM(uport);
	return NULL;
}

/* request to turn off uart clock once pending TX is flushed */
void msm_hs_request_clock_off(struct uart_port *uport) {
	unsigned long flags;
	struct msm_hs_port *msm_uport = UARTDM_TO_MSM(uport);
	int data;

	spin_lock_irqsave(&uport->lock, flags);
	if (msm_uport->clk_state == MSM_HS_CLK_ON) {
		msm_uport->clk_state = MSM_HS_CLK_REQUEST_OFF;
		data = msm_hs_read(uport, UART_DM_MR1);
		/*disable auto ready-for-receiving */
		data &= ~UARTDM_MR1_RX_RDY_CTL_BMSK;
		msm_hs_write(uport, UART_DM_MR1, data);
		mb();
		/* set RFR_N to high */
		msm_hs_write(uport, UART_DM_CR, RFR_HIGH);

		data = msm_hs_read(uport, UART_DM_SR);
		MSM_HS_DBG("%s(): TXEMT, queuing clock off work\n",
			__func__);
		queue_work(msm_uport->hsuart_wq, &msm_uport->clock_off_w);

		mb();
	}
	spin_unlock_irqrestore(&uport->lock, flags);
}
EXPORT_SYMBOL(msm_hs_request_clock_off);

void msm_hs_request_clock_on(struct uart_port *uport)
{
	struct msm_hs_port *msm_uport = UARTDM_TO_MSM(uport);
	unsigned long flags;
	unsigned int data;
	int ret = 0;

	mutex_lock(&msm_uport->clk_mutex);
	spin_lock_irqsave(&uport->lock, flags);

	if (msm_uport->clk_state == MSM_HS_CLK_REQUEST_OFF) {
		/* Pulling RFR line high */
		msm_hs_write(uport, UART_DM_CR, RFR_LOW);
		/* Enable auto RFR */
		data = msm_hs_read(uport, UART_DM_MR1);
		data |= UARTDM_MR1_RX_RDY_CTL_BMSK;
		msm_hs_write(uport, UART_DM_MR1, data);
		mb();
	}
	switch (msm_uport->clk_state) {
	case MSM_HS_CLK_OFF:
		wake_lock(&msm_uport->dma_wake_lock);
		if (use_low_power_wakeup(msm_uport))
			disable_irq_nosync(msm_uport->wakeup.irq);
		spin_unlock_irqrestore(&uport->lock, flags);

		ret = msm_hs_clock_vote(msm_uport);
		if (ret) {
			MSM_HS_INFO("Clock ON Failure"
			"For UART CLK Stalling HSUART\n");
			break;
		}

		spin_lock_irqsave(&uport->lock, flags);
		/* else fall-through */
	case MSM_HS_CLK_REQUEST_OFF:
		hrtimer_cancel(&msm_uport->clk_off_timer);
		if (msm_uport->rx.flush == FLUSH_STOP) {
			spin_unlock_irqrestore(&uport->lock, flags);
			MSM_HS_DBG("%s:Calling wait forxcompletion\n",
					__func__);
			ret = wait_event_timeout(msm_uport->bam_disconnect_wait,
				msm_uport->rx.flush == FLUSH_SHUTDOWN, 300);
			if (!ret)
				MSM_HS_ERR("BAM Disconnect not happened\n");
			spin_lock_irqsave(&uport->lock, flags);
			MSM_HS_DBG("%s:DONE wait for completion\n", __func__);
		}
		MSM_HS_DBG("%s:clock state %d\n\n", __func__,
				msm_uport->clk_state);
		if (msm_uport->clk_state == MSM_HS_CLK_REQUEST_OFF)
				msm_uport->clk_state = MSM_HS_CLK_ON;
		if (msm_uport->rx.flush == FLUSH_STOP ||
		    msm_uport->rx.flush == FLUSH_SHUTDOWN) {
			msm_hs_write(uport, UART_DM_CR, RESET_RX);
			data = msm_hs_read(uport, UART_DM_DMEN);
			if (is_blsp_uart(msm_uport))
				data |= UARTDM_RX_BAM_ENABLE_BMSK;
			else
				data |= UARTDM_RX_DM_EN_BMSK;
			msm_hs_write(uport, UART_DM_DMEN, data);
			/* Complete above device write. Hence mb() here. */
			mb();
		}

		MSM_HS_DBG("%s: rx.flush %d\n", __func__, msm_uport->rx.flush);
		if (msm_uport->rx.flush == FLUSH_SHUTDOWN) {
			if (is_blsp_uart(msm_uport)) {
				spin_unlock_irqrestore(&uport->lock, flags);
				msm_hs_spsconnect_rx(uport);
				spin_lock_irqsave(&uport->lock, flags);
			}
			msm_hs_start_rx_locked(uport);
		}
		if (msm_uport->rx.flush == FLUSH_STOP)
			msm_uport->rx.flush = FLUSH_IGNORE;

		break;
	case MSM_HS_CLK_ON:
		break;
	case MSM_HS_CLK_PORT_OFF:
		break;
	}

	spin_unlock_irqrestore(&uport->lock, flags);
	mutex_unlock(&msm_uport->clk_mutex);
}
EXPORT_SYMBOL(msm_hs_request_clock_on);

static irqreturn_t msm_hs_wakeup_isr(int irq, void *dev)
{
	unsigned int wakeup = 0;
	unsigned long flags;
	struct msm_hs_port *msm_uport = (struct msm_hs_port *)dev;
	struct uart_port *uport = &msm_uport->uport;
	struct tty_struct *tty = NULL;

	spin_lock_irqsave(&uport->lock, flags);
	if (msm_uport->clk_state == MSM_HS_CLK_OFF)  {
		/* ignore the first irq - it is a pending irq that occured
		 * before enable_irq()
		 */
		if (msm_uport->wakeup.ignore)
			msm_uport->wakeup.ignore = 0;
		else
			wakeup = 1;
	}

	if (wakeup) {
		/* the uart was clocked off during an rx, wake up and
		 * optionally inject char into tty rx
		 */
		spin_unlock_irqrestore(&uport->lock, flags);
		msm_hs_request_clock_on(uport);
		spin_lock_irqsave(&uport->lock, flags);
		if (msm_uport->wakeup.inject_rx) {
			tty = uport->state->port.tty;
			tty_insert_flip_char(tty,
					     msm_uport->wakeup.rx_to_inject,
					     TTY_NORMAL);
		}
	}

	spin_unlock_irqrestore(&uport->lock, flags);

	if (wakeup && msm_uport->wakeup.inject_rx)
		tty_flip_buffer_push(tty);
	return IRQ_HANDLED;
}

static const char *msm_hs_type(struct uart_port *port)
{
	return ("MSM HS UART");
}

/**
 * msm_hs_unconfig_uart_gpios: Unconfigures UART GPIOs
 * @uport: uart port
 */
static void msm_hs_unconfig_uart_gpios(struct uart_port *uport)
{
	struct platform_device *pdev = to_platform_device(uport->dev);
	const struct msm_serial_hs_platform_data *pdata =
					pdev->dev.platform_data;

	if (pdata) {
		if (gpio_is_valid(pdata->uart_tx_gpio))
			gpio_free(pdata->uart_tx_gpio);
		if (gpio_is_valid(pdata->uart_rx_gpio))
			gpio_free(pdata->uart_rx_gpio);
		if (gpio_is_valid(pdata->uart_cts_gpio))
			gpio_free(pdata->uart_cts_gpio);
		if (gpio_is_valid(pdata->uart_rfr_gpio))
			gpio_free(pdata->uart_rfr_gpio);
	} else {
		MSM_HS_ERR("Error:Pdata is NULL.\n");
	}
}

/**
 * msm_hs_config_uart_gpios - Configures UART GPIOs
 * @uport: uart port
 */
static int msm_hs_config_uart_gpios(struct uart_port *uport)
{
	struct platform_device *pdev = to_platform_device(uport->dev);
	const struct msm_serial_hs_platform_data *pdata =
					pdev->dev.platform_data;
	int ret = 0;

	if (pdata) {
		if (gpio_is_valid(pdata->uart_tx_gpio)) {
			ret = gpio_request(pdata->uart_tx_gpio,
							"UART_TX_GPIO");
			if (unlikely(ret)) {
				MSM_HS_ERR("gpio request failed for:%d\n",
					pdata->uart_tx_gpio);
				goto exit_uart_config;
			}
		}

		if (gpio_is_valid(pdata->uart_rx_gpio)) {
			ret = gpio_request(pdata->uart_rx_gpio,
							"UART_RX_GPIO");
			if (unlikely(ret)) {
				MSM_HS_ERR("gpio request failed for:%d\n",
					pdata->uart_rx_gpio);
				goto uart_tx_unconfig;
			}
		}

		if (gpio_is_valid(pdata->uart_cts_gpio)) {
			ret = gpio_request(pdata->uart_cts_gpio,
							"UART_CTS_GPIO");
			if (unlikely(ret)) {
				MSM_HS_ERR("gpio request failed for:%d\n",
					pdata->uart_cts_gpio);
				goto uart_rx_unconfig;
			}
		}

		if (gpio_is_valid(pdata->uart_rfr_gpio)) {
			ret = gpio_request(pdata->uart_rfr_gpio,
							"UART_RFR_GPIO");
			if (unlikely(ret)) {
				MSM_HS_ERR("gpio request failed for:%d\n",
					pdata->uart_rfr_gpio);
				goto uart_cts_unconfig;
			}
		}
	} else {
		MSM_HS_ERR("Pdata is NULL.\n");
		ret = -EINVAL;
	}
	return ret;

uart_cts_unconfig:
	if (gpio_is_valid(pdata->uart_cts_gpio))
		gpio_free(pdata->uart_cts_gpio);
uart_rx_unconfig:
	if (gpio_is_valid(pdata->uart_rx_gpio))
		gpio_free(pdata->uart_rx_gpio);
uart_tx_unconfig:
	if (gpio_is_valid(pdata->uart_tx_gpio))
		gpio_free(pdata->uart_tx_gpio);
exit_uart_config:
	return ret;
}

/* Called when port is opened */
static int msm_hs_startup(struct uart_port *uport)
{
	int ret;
	int rfr_level;
	unsigned long flags;
	unsigned int data;
	struct msm_hs_port *msm_uport = UARTDM_TO_MSM(uport);
	struct platform_device *pdev = to_platform_device(uport->dev);
	const struct msm_serial_hs_platform_data *pdata =
					pdev->dev.platform_data;
	struct circ_buf *tx_buf = &uport->state->xmit;
	struct msm_hs_tx *tx = &msm_uport->tx;
	struct msm_hs_rx *rx = &msm_uport->rx;
	struct sps_pipe *sps_pipe_handle_tx = tx->cons.pipe_handle;
	struct sps_pipe *sps_pipe_handle_rx = rx->prod.pipe_handle;

	rfr_level = uport->fifosize;
	if (rfr_level > 16)
		rfr_level -= 16;

	tx->dma_base = dma_map_single(uport->dev, tx_buf->buf, UART_XMIT_SIZE,
				      DMA_TO_DEVICE);

	wake_lock(&msm_uport->dma_wake_lock);
	/* turn on uart clk */
	ret = msm_hs_init_clk(uport);
	if (unlikely(ret)) {
		MSM_HS_ERR("Turning ON uartclk error\n");
		wake_unlock(&msm_uport->dma_wake_lock);
		return ret;
	}

	if (is_blsp_uart(msm_uport)) {
		ret = msm_hs_config_uart_gpios(uport);
		if (ret) {
			MSM_HS_ERR("Uart GPIO request failed\n");
			goto deinit_uart_clk;
		}
	} else {
		if (pdata && pdata->gpio_config)
			if (unlikely(pdata->gpio_config(1)))
				dev_err(uport->dev, "Cannot configure gpios\n");
	}

	/* SPS Connect for BAM endpoints */
	if (is_blsp_uart(msm_uport)) {
		/* SPS connect for TX */
		ret = msm_hs_spsconnect_tx(uport);
		if (ret) {
			MSM_HS_ERR("msm_serial_hs: SPS connect failed for TX");
			goto unconfig_uart_gpios;
		}

		/* SPS connect for RX */
		ret = msm_hs_spsconnect_rx(uport);
		if (ret) {
			MSM_HS_ERR("msm_serial_hs: SPS connect failed for RX");
			goto sps_disconnect_tx;
		}
	}

	msm_hs_write(uport, UARTDM_BCR_ADDR, 0x003F);
	/* Set auto RFR Level */
	data = msm_hs_read(uport, UART_DM_MR1);
	data &= ~UARTDM_MR1_AUTO_RFR_LEVEL1_BMSK;
	data &= ~UARTDM_MR1_AUTO_RFR_LEVEL0_BMSK;
	data |= (UARTDM_MR1_AUTO_RFR_LEVEL1_BMSK & (rfr_level << 2));
	data |= (UARTDM_MR1_AUTO_RFR_LEVEL0_BMSK & rfr_level);
	msm_hs_write(uport, UART_DM_MR1, data);

	/* Make sure RXSTALE count is non-zero */
	data = msm_hs_read(uport, UART_DM_IPR);
	if (!data) {
		data |= 0x1f & UARTDM_IPR_STALE_LSB_BMSK;
		msm_hs_write(uport, UART_DM_IPR, data);
	}

	if (is_blsp_uart(msm_uport)) {
		/* Enable BAM mode */
		data  = UARTDM_TX_BAM_ENABLE_BMSK | UARTDM_RX_BAM_ENABLE_BMSK;
	} else {
		/* Enable Data Mover Mode */
		data = UARTDM_TX_DM_EN_BMSK | UARTDM_RX_DM_EN_BMSK;
	}
	msm_hs_write(uport, UART_DM_DMEN, data);

	/* Reset TX */
	msm_hs_write(uport, UART_DM_CR, RESET_TX);
	msm_hs_write(uport, UART_DM_CR, RESET_RX);
	msm_hs_write(uport, UART_DM_CR, RESET_ERROR_STATUS);
	msm_hs_write(uport, UART_DM_CR, RESET_BREAK_INT);
	msm_hs_write(uport, UART_DM_CR, RESET_STALE_INT);
	msm_hs_write(uport, UART_DM_CR, RESET_CTS);
	msm_hs_write(uport, UART_DM_CR, RFR_LOW);
	/* Turn on Uart Receiver */
	msm_hs_write(uport, UART_DM_CR, UARTDM_CR_RX_EN_BMSK);

	/* Turn on Uart Transmitter */
	msm_hs_write(uport, UART_DM_CR, UARTDM_CR_TX_EN_BMSK);

	/* Initialize the tx */
	tx->tx_ready_int_en = 0;
	tx->dma_in_flight = 0;
	rx->rx_cmd_exec = false;
	msm_uport->tty_flush_receive = false;
	MSM_HS_DBG("%s: Setting tty_flush_receive to false\n", __func__);

	if (!is_blsp_uart(msm_uport)) {
		tx->xfer.complete_func = msm_hs_dmov_tx_callback;

		tx->command_ptr->cmd = CMD_LC |
			CMD_DST_CRCI(msm_uport->dma_tx_crci) | CMD_MODE_BOX;

		tx->command_ptr->src_dst_len = (MSM_UARTDM_BURST_SIZE << 16)
					   | (MSM_UARTDM_BURST_SIZE);

		tx->command_ptr->row_offset = (MSM_UARTDM_BURST_SIZE << 16);

		tx->command_ptr->dst_row_addr =
			msm_uport->uport.mapbase + UARTDM_TF_ADDR;

		msm_uport->imr_reg |= UARTDM_ISR_RXSTALE_BMSK;
	}

	/* Enable reading the current CTS, no harm even if CTS is ignored */
	msm_uport->imr_reg |= UARTDM_ISR_CURRENT_CTS_BMSK;

	/* TXLEV on empty TX fifo */
	msm_hs_write(uport, UART_DM_TFWR, 4);
	/*
	 * Complete all device write related configuration before
	 * queuing RX request. Hence mb() requires here.
	 */
	mb();

	if (use_low_power_wakeup(msm_uport)) {
		ret = irq_set_irq_wake(msm_uport->wakeup.irq, 1);
		if (unlikely(ret)) {
			MSM_HS_ERR("%s():Err setting wakeup irq\n", __func__);
			goto sps_disconnect_rx;
		}
	}

	ret = request_irq(uport->irq, msm_hs_isr, IRQF_TRIGGER_HIGH,
			  "msm_hs_uart", msm_uport);
	if (unlikely(ret)) {
		MSM_HS_ERR("%s():Error getting uart irq\n", __func__);
		goto free_wake_irq;
	}
	if (use_low_power_wakeup(msm_uport)) {

		ret = request_threaded_irq(msm_uport->wakeup.irq, NULL,
					msm_hs_wakeup_isr,
					IRQF_TRIGGER_FALLING,
					"msm_hs_wakeup", msm_uport);

		if (unlikely(ret)) {
			MSM_HS_ERR("%s():Err getting uart wakeup_irq\n", __func__);
			goto free_uart_irq;
		}
		disable_irq(msm_uport->wakeup.irq);
	}
	spin_lock_irqsave(&uport->lock, flags);

	msm_hs_start_rx_locked(uport);

	spin_unlock_irqrestore(&uport->lock, flags);

	pm_runtime_enable(uport->dev);

	return 0;

free_uart_irq:
	free_irq(uport->irq, msm_uport);
free_wake_irq:
	if (use_low_power_wakeup(msm_uport))
		irq_set_irq_wake(msm_uport->wakeup.irq, 0);
sps_disconnect_rx:
	if (is_blsp_uart(msm_uport))
		sps_disconnect(sps_pipe_handle_rx);
sps_disconnect_tx:
	if (is_blsp_uart(msm_uport))
		sps_disconnect(sps_pipe_handle_tx);
unconfig_uart_gpios:
	if (is_blsp_uart(msm_uport))
		msm_hs_unconfig_uart_gpios(uport);
deinit_uart_clk:
	msm_hs_clock_unvote(msm_uport);
	wake_unlock(&msm_uport->dma_wake_lock);

	return ret;
}

/* Initialize tx and rx data structures */
static int uartdm_init_port(struct uart_port *uport)
{
	int ret = 0;
	struct msm_hs_port *msm_uport = UARTDM_TO_MSM(uport);
	struct msm_hs_tx *tx = &msm_uport->tx;
	struct msm_hs_rx *rx = &msm_uport->rx;

	init_waitqueue_head(&rx->wait);
	init_waitqueue_head(&tx->wait);
	init_waitqueue_head(&msm_uport->bam_disconnect_wait);
	wake_lock_init(&rx->wake_lock, WAKE_LOCK_SUSPEND, "msm_serial_hs_rx");
	wake_lock_init(&msm_uport->dma_wake_lock, WAKE_LOCK_SUSPEND,
		       "msm_serial_hs_dma");

	tasklet_init(&rx->tlet, msm_serial_hs_rx_tlet,
			(unsigned long) &rx->tlet);
	tasklet_init(&tx->tlet, msm_serial_hs_tx_tlet,
			(unsigned long) &tx->tlet);

	rx->pool = dma_pool_create("rx_buffer_pool", uport->dev,
				   UARTDM_RX_BUF_SIZE, 16, 0);
	if (!rx->pool) {
		MSM_HS_ERR("%s(): cannot allocate rx_buffer_pool", __func__);
		ret = -ENOMEM;
		goto exit_tasket_init;
	}

	rx->buffer = dma_pool_alloc(rx->pool, GFP_KERNEL, &rx->rbuffer);
	if (!rx->buffer) {
		MSM_HS_ERR("%s(): cannot allocate rx->buffer", __func__);
		ret = -ENOMEM;
		goto free_pool;
	}

	/* Set up Uart Receive */
	if (is_blsp_uart(msm_uport))
		msm_hs_write(uport, UART_DM_RFWR, 32);
	else
		msm_hs_write(uport, UART_DM_RFWR, 0);

	INIT_DELAYED_WORK(&rx->flip_insert_work, flip_insert_work);

	if (is_blsp_uart(msm_uport))
		return ret;

	/* Allocate the command pointer. Needs to be 64 bit aligned */
	tx->command_ptr = kmalloc(sizeof(dmov_box), GFP_KERNEL | __GFP_DMA);
	if (!tx->command_ptr) {
		return -ENOMEM;
		goto free_rx_buffer;
	}

	tx->command_ptr_ptr = kmalloc(sizeof(u32), GFP_KERNEL | __GFP_DMA);
	if (!tx->command_ptr_ptr) {
		ret = -ENOMEM;
		goto free_tx_command_ptr;
	}

	tx->mapped_cmd_ptr = dma_map_single(uport->dev, tx->command_ptr,
					sizeof(dmov_box), DMA_TO_DEVICE);
	tx->mapped_cmd_ptr_ptr = dma_map_single(uport->dev,
						tx->command_ptr_ptr,
						sizeof(u32), DMA_TO_DEVICE);
	tx->xfer.cmdptr = DMOV_CMD_ADDR(tx->mapped_cmd_ptr_ptr);

	/* Allocate the command pointer. Needs to be 64 bit aligned */
	rx->command_ptr = kmalloc(sizeof(dmov_box), GFP_KERNEL | __GFP_DMA);
	if (!rx->command_ptr) {
		MSM_HS_ERR("%s(): cannot allocate rx->command_ptr", __func__);
		ret = -ENOMEM;
		goto free_tx_command_ptr_ptr;
	}

	rx->command_ptr_ptr = kmalloc(sizeof(u32), GFP_KERNEL | __GFP_DMA);
	if (!rx->command_ptr_ptr) {
		MSM_HS_ERR("%s(): cannot allocate rx->command_ptr_ptr",
			 __func__);
		ret = -ENOMEM;
		goto free_rx_command_ptr;
	}

	rx->command_ptr->num_rows = ((UARTDM_RX_BUF_SIZE >> 4) << 16) |
					 (UARTDM_RX_BUF_SIZE >> 4);

	rx->command_ptr->dst_row_addr = rx->rbuffer;

	rx->xfer.complete_func = msm_hs_dmov_rx_callback;

	rx->command_ptr->cmd = CMD_LC |
	    CMD_SRC_CRCI(msm_uport->dma_rx_crci) | CMD_MODE_BOX;

	rx->command_ptr->src_dst_len = (MSM_UARTDM_BURST_SIZE << 16)
					   | (MSM_UARTDM_BURST_SIZE);
	rx->command_ptr->row_offset =  MSM_UARTDM_BURST_SIZE;
	rx->command_ptr->src_row_addr = uport->mapbase + UARTDM_RF_ADDR;

	rx->mapped_cmd_ptr = dma_map_single(uport->dev, rx->command_ptr,
					    sizeof(dmov_box), DMA_TO_DEVICE);

	*rx->command_ptr_ptr = CMD_PTR_LP | DMOV_CMD_ADDR(rx->mapped_cmd_ptr);

	rx->cmdptr_dmaaddr = dma_map_single(uport->dev, rx->command_ptr_ptr,
					    sizeof(u32), DMA_TO_DEVICE);
	rx->xfer.cmdptr = DMOV_CMD_ADDR(rx->cmdptr_dmaaddr);

	return ret;

free_rx_command_ptr:
	kfree(rx->command_ptr);

free_tx_command_ptr_ptr:
	kfree(msm_uport->tx.command_ptr_ptr);
	dma_unmap_single(uport->dev, msm_uport->tx.mapped_cmd_ptr_ptr,
			sizeof(u32), DMA_TO_DEVICE);
	dma_unmap_single(uport->dev, msm_uport->tx.mapped_cmd_ptr,
			sizeof(dmov_box), DMA_TO_DEVICE);

free_tx_command_ptr:
	kfree(msm_uport->tx.command_ptr);

free_rx_buffer:
	dma_pool_free(msm_uport->rx.pool, msm_uport->rx.buffer,
			msm_uport->rx.rbuffer);

free_pool:
	dma_pool_destroy(msm_uport->rx.pool);

exit_tasket_init:
	wake_lock_destroy(&msm_uport->rx.wake_lock);
	wake_lock_destroy(&msm_uport->dma_wake_lock);
	tasklet_kill(&msm_uport->tx.tlet);
	tasklet_kill(&msm_uport->rx.tlet);
	return ret;
}

struct msm_serial_hs_platform_data
	*msm_hs_dt_to_pdata(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct msm_serial_hs_platform_data *pdata;
	int rx_to_inject, ret;

	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		MSM_HS_ERR("unable to allocate memory for platform data\n");
		return ERR_PTR(-ENOMEM);
	}

	/* UART TX GPIO */
	pdata->uart_tx_gpio = of_get_named_gpio(node,
					"qcom,tx-gpio", 0);
	if (pdata->uart_tx_gpio < 0)
		MSM_HS_DBG("uart_tx_gpio is not available\n");

	/* UART RX GPIO */
	pdata->uart_rx_gpio = of_get_named_gpio(node,
					"qcom,rx-gpio", 0);
	if (pdata->uart_rx_gpio < 0)
		MSM_HS_DBG("uart_rx_gpio is not available\n");

	/* UART CTS GPIO */
	pdata->uart_cts_gpio = of_get_named_gpio(node,
					"qcom,cts-gpio", 0);
	if (pdata->uart_cts_gpio < 0)
		MSM_HS_DBG("uart_cts_gpio is not available\n");

	/* UART RFR GPIO */
	pdata->uart_rfr_gpio = of_get_named_gpio(node,
					"qcom,rfr-gpio", 0);
	if (pdata->uart_rfr_gpio < 0)
		MSM_HS_DBG("uart_rfr_gpio is not available\n");

	pdata->inject_rx_on_wakeup = of_property_read_bool(node,
				"qcom,inject-rx-on-wakeup");

	if (pdata->inject_rx_on_wakeup) {
		ret = of_property_read_u32(node, "qcom,rx-char-to-inject",
						&rx_to_inject);
		if (ret < 0) {
			MSM_HS_ERR("Error: Rx_char_to_inject not specified.\n");
			return ERR_PTR(ret);
		}
		pdata->rx_to_inject = (char)rx_to_inject;
	}

	ret = of_property_read_u32(node, "qcom,bam-tx-ep-pipe-index",
				&pdata->bam_tx_ep_pipe_index);
	if (ret < 0) {
		MSM_HS_ERR("Error: Getting UART BAM TX EP Pipe Index.\n");
		return ERR_PTR(ret);
	}

	if (!(pdata->bam_tx_ep_pipe_index >= BAM_PIPE_MIN &&
		pdata->bam_tx_ep_pipe_index <= BAM_PIPE_MAX)) {
		MSM_HS_ERR("Error: Invalid UART BAM TX EP Pipe Index.\n");
		return ERR_PTR(-EINVAL);
	}

	ret = of_property_read_u32(node, "qcom,bam-rx-ep-pipe-index",
					&pdata->bam_rx_ep_pipe_index);
	if (ret < 0) {
		MSM_HS_ERR("Error: Getting UART BAM RX EP Pipe Index.\n");
		return ERR_PTR(ret);
	}

	if (!(pdata->bam_rx_ep_pipe_index >= BAM_PIPE_MIN &&
		pdata->bam_rx_ep_pipe_index <= BAM_PIPE_MAX)) {
		MSM_HS_ERR("Error: Invalid UART BAM RX EP Pipe Index.\n");
		return ERR_PTR(-EINVAL);
	}

	MSM_HS_DBG("tx_ep_pipe_index:%d rx_ep_pipe_index:%d\n"
		"tx_gpio:%d rx_gpio:%d rfr_gpio:%d cts_gpio:%d",
		pdata->bam_tx_ep_pipe_index, pdata->bam_rx_ep_pipe_index,
		pdata->uart_tx_gpio, pdata->uart_rx_gpio, pdata->uart_cts_gpio,
		pdata->uart_rfr_gpio);

	return pdata;
}


/**
 * Deallocate UART peripheral's SPS endpoint
 * @msm_uport - Pointer to msm_hs_port structure
 * @ep - Pointer to sps endpoint data structure
 */

static void msm_hs_exit_ep_conn(struct msm_hs_port *msm_uport,
				struct msm_hs_sps_ep_conn_data *ep)
{
	struct sps_pipe *sps_pipe_handle = ep->pipe_handle;
	struct sps_connect *sps_config = &ep->config;

	dma_free_coherent(msm_uport->uport.dev,
			sps_config->desc.size,
			&sps_config->desc.phys_base,
			GFP_KERNEL);
	sps_free_endpoint(sps_pipe_handle);
}


/**
 * Allocate UART peripheral's SPS endpoint
 *
 * This function allocates endpoint context
 * by calling appropriate SPS driver APIs.
 *
 * @msm_uport - Pointer to msm_hs_port structure
 * @ep - Pointer to sps endpoint data structure
 * @is_produce - 1 means Producer endpoint
 *             - 0 means Consumer endpoint
 *
 * @return - 0 if successful else negative value
 */

static int msm_hs_sps_init_ep_conn(struct msm_hs_port *msm_uport,
				struct msm_hs_sps_ep_conn_data *ep,
				bool is_producer)
{
	int rc = 0;
	struct sps_pipe *sps_pipe_handle;
	struct sps_connect *sps_config = &ep->config;
	struct sps_register_event *sps_event = &ep->event;

	/* Allocate endpoint context */
	sps_pipe_handle = sps_alloc_endpoint();
	if (!sps_pipe_handle) {
		MSM_HS_ERR("msm_serial_hs: sps_alloc_endpoint() failed!!\n"
			"is_producer=%d", is_producer);
		rc = -ENOMEM;
		goto out;
	}

	/* Get default connection configuration for an endpoint */
	rc = sps_get_config(sps_pipe_handle, sps_config);
	if (rc) {
		MSM_HS_ERR("msm_serial_hs: sps_get_config() failed!!\n"
		"pipe_handle=0x%x rc=%d", (u32)sps_pipe_handle, rc);
		goto get_config_err;
	}

	/* Modify the default connection configuration */
	if (is_producer) {
		/* For UART producer transfer, source is UART peripheral
		where as destination is system memory */
		sps_config->source = msm_uport->bam_handle;
		sps_config->destination = SPS_DEV_HANDLE_MEM;
		sps_config->mode = SPS_MODE_SRC;
		sps_config->src_pipe_index = msm_uport->bam_rx_ep_pipe_index;
		sps_config->dest_pipe_index = 0;
	} else {
		/* For UART consumer transfer, source is system memory
		where as destination is UART peripheral */
		sps_config->source = SPS_DEV_HANDLE_MEM;
		sps_config->destination = msm_uport->bam_handle;
		sps_config->mode = SPS_MODE_DEST;
		sps_config->src_pipe_index = 0;
		sps_config->dest_pipe_index = msm_uport->bam_tx_ep_pipe_index;
	}

	sps_config->options = SPS_O_EOT | SPS_O_DESC_DONE | SPS_O_AUTO_ENABLE;
	sps_config->event_thresh = 0x10;

	/* Allocate maximum descriptor fifo size */
	sps_config->desc.size = 65532;
	sps_config->desc.base = dma_alloc_coherent(msm_uport->uport.dev,
						sps_config->desc.size,
						&sps_config->desc.phys_base,
						GFP_KERNEL);
	if (!sps_config->desc.base) {
		rc = -ENOMEM;
		MSM_HS_ERR("msm_serial_hs: dma_alloc_coherent() failed!!\n");
		goto get_config_err;
	}
	memset(sps_config->desc.base, 0x00, sps_config->desc.size);

	sps_event->mode = SPS_TRIGGER_CALLBACK;

	if (is_producer) {
		sps_event->callback = msm_hs_sps_rx_callback;
	} else {
		sps_event->callback = msm_hs_sps_tx_callback;
	}

	sps_event->options = SPS_O_DESC_DONE | SPS_O_EOT;
	sps_event->user = (void *)msm_uport;

	/* Now save the sps pipe handle */
	ep->pipe_handle = sps_pipe_handle;
	MSM_HS_DBG("msm_serial_hs: success !! %s: pipe_handle=0x%x\n"
		"desc_fifo.phys_base=0x%llx\n",
		is_producer ? "READ" : "WRITE",
		(u32) sps_pipe_handle, (u64) sps_config->desc.phys_base);
	return 0;

get_config_err:
	sps_free_endpoint(sps_pipe_handle);
out:
	return rc;
}

/**
 * Initialize SPS HW connected with UART core
 *
 * This function register BAM HW resources with
 * SPS driver and then initialize 2 SPS endpoints
 *
 * msm_uport - Pointer to msm_hs_port structure
 *
 * @return - 0 if successful else negative value
 */

static int msm_hs_sps_init(struct msm_hs_port *msm_uport)
{
	int rc = 0;
	struct sps_bam_props bam = {0};
	u32 bam_handle;

	rc = sps_phy2h(msm_uport->bam_mem, &bam_handle);
	if (rc || !bam_handle) {
		bam.phys_addr = msm_uport->bam_mem;
		bam.virt_addr = msm_uport->bam_base;
		/*
		 * This event thresold value is only significant for BAM-to-BAM
		 * transfer. It's ignored for BAM-to-System mode transfer.
		 */
		bam.event_threshold = 0x10;	/* Pipe event threshold */
		bam.summing_threshold = 1;	/* BAM event threshold */

		/* SPS driver wll handle the UART BAM IRQ */
		bam.irq = (u32)msm_uport->bam_irq;
		bam.manage = SPS_BAM_MGR_DEVICE_REMOTE;

		MSM_HS_DBG("msm_serial_hs: bam physical base=0x%x\n",
							(u32)bam.phys_addr);
		MSM_HS_DBG("msm_serial_hs: bam virtual base=0x%x\n",
							(u32)bam.virt_addr);

		/* Register UART Peripheral BAM device to SPS driver */
		rc = sps_register_bam_device(&bam, &bam_handle);
		if (rc) {
			MSM_HS_ERR("msm_serial_hs: BAM device register failed\n");
			return rc;
		}
		MSM_HS_INFO("msm_serial_hs: BAM device registered. bam_handle=0x%x",
							msm_uport->bam_handle);
	}
	msm_uport->bam_handle = bam_handle;

	rc = msm_hs_sps_init_ep_conn(msm_uport, &msm_uport->rx.prod,
				UART_SPS_PROD_PERIPHERAL);
	if (rc) {
		MSM_HS_ERR("%s: Failed to Init Producer BAM-pipe", __func__);
		goto deregister_bam;
	}

	rc = msm_hs_sps_init_ep_conn(msm_uport, &msm_uport->tx.cons,
				UART_SPS_CONS_PERIPHERAL);
	if (rc) {
		MSM_HS_ERR("%s: Failed to Init Consumer BAM-pipe", __func__);
		goto deinit_ep_conn_prod;
	}
	return 0;

deinit_ep_conn_prod:
	msm_hs_exit_ep_conn(msm_uport, &msm_uport->rx.prod);
deregister_bam:
	sps_deregister_bam_device(msm_uport->bam_handle);
	return rc;
}

#define BLSP_UART_NR	12
static int deviceid[BLSP_UART_NR] = {0};
static atomic_t msm_serial_hs_next_id = ATOMIC_INIT(0);

static int __devinit msm_hs_probe(struct platform_device *pdev)
{
	int ret = 0, alias_num = -1;
	struct uart_port *uport;
	struct msm_hs_port *msm_uport;
	struct resource *core_resource;
	struct resource *bam_resource;
	struct resource *resource;
	int core_irqres, bam_irqres, wakeup_irqres;
	struct msm_serial_hs_platform_data *pdata = pdev->dev.platform_data;
	const struct of_device_id *match;
	unsigned long data;

	if (pdev->dev.of_node) {
		dev_dbg(&pdev->dev, "device tree enabled\n");
		pdata = msm_hs_dt_to_pdata(pdev);
		if (IS_ERR(pdata))
			return PTR_ERR(pdata);

		if (pdev->id == -1) {
			pdev->id = atomic_inc_return(&msm_serial_hs_next_id)-1;
			deviceid[pdev->id] = 1;
		}

		/* Use alias from device tree if present
		 * Alias is used as an optional property
		 */
		alias_num = of_alias_get_id(pdev->dev.of_node, "uart");
		if (alias_num >= 0) {
			/* If alias_num is between 0 and 11, check that it not
			 * equal to previous incremented pdev-ids. If it is
			 * equal to previous pdev.ids , fail deviceprobe.
			 */
			if (alias_num < BLSP_UART_NR) {
				if (deviceid[alias_num] == 0) {
					pdev->id = alias_num;
				} else {
					MSM_HS_ERR("alias_num=%d already used\n",
								alias_num);
					return -EINVAL;
				}
			} else {
				pdev->id = alias_num;
			}
		}

		pdev->dev.platform_data = pdata;
	}

	if (pdev->id < 0 || pdev->id >= UARTDM_NR) {
		MSM_HS_ERR("Invalid plaform device ID = %d\n", pdev->id);
		return -EINVAL;
	}

	msm_uport = devm_kzalloc(&pdev->dev, sizeof(struct msm_hs_port),
			GFP_KERNEL);
	msm_uport->uport.type = PORT_UNKNOWN;
	uport = &msm_uport->uport;
	uport->dev = &pdev->dev;

	match = of_match_device(msm_hs_match_table, &pdev->dev);
	if (match)
		msm_uport->reg_ptr = (unsigned int *)match->data;
	else if (is_gsbi_uart(msm_uport))
		msm_uport->reg_ptr = regmap_nonblsp;

	if (pdev->dev.of_node)
		msm_uport->uart_type = BLSP_HSUART;

	/* Get required resources for BAM HSUART */
	if (is_blsp_uart(msm_uport)) {
		core_resource = platform_get_resource_byname(pdev,
					IORESOURCE_MEM, "core_mem");
		bam_resource = platform_get_resource_byname(pdev,
					IORESOURCE_MEM, "bam_mem");
		core_irqres = platform_get_irq_byname(pdev, "core_irq");
		bam_irqres = platform_get_irq_byname(pdev, "bam_irq");
		wakeup_irqres = platform_get_irq_byname(pdev, "wakeup_irq");

		if (!core_resource) {
			MSM_HS_ERR("Invalid core HSUART Resources.\n");
			return -ENXIO;
		}

		if (!bam_resource) {
			MSM_HS_ERR("Invalid BAM HSUART Resources.\n");
			return -ENXIO;
		}

		if (!core_irqres) {
			MSM_HS_ERR("Invalid core irqres Resources.\n");
			return -ENXIO;
		}
		if (!bam_irqres) {
			MSM_HS_ERR("Invalid bam irqres Resources.\n");
			return -ENXIO;
		}
		if (!wakeup_irqres)
			MSM_HS_DBG("Wakeup irq not specified.\n");

		uport->mapbase = core_resource->start;

		uport->membase = ioremap(uport->mapbase,
					resource_size(core_resource));
		if (unlikely(!uport->membase)) {
			MSM_HS_ERR("UART Resource ioremap Failed.\n");
			return -ENOMEM;
		}
		msm_uport->bam_mem = bam_resource->start;
		msm_uport->bam_base = ioremap(msm_uport->bam_mem,
					resource_size(bam_resource));
		if (unlikely(!msm_uport->bam_base)) {
			MSM_HS_ERR("UART BAM Resource ioremap Failed.\n");
			iounmap(uport->membase);
			return -ENOMEM;
		}

		uport->irq = core_irqres;
		msm_uport->bam_irq = bam_irqres;
		pdata->wakeup_irq = wakeup_irqres;

		msm_uport->bus_scale_table = msm_bus_cl_get_pdata(pdev);
		if (!msm_uport->bus_scale_table) {
			MSM_HS_ERR("BLSP UART: Bus scaling is disabled.\n");
		} else {
			msm_uport->bus_perf_client =
				msm_bus_scale_register_client
					(msm_uport->bus_scale_table);
			if (IS_ERR(&msm_uport->bus_perf_client)) {
				MSM_HS_ERR("%s(): Bus client register failed.\n",
								__func__);
				ret = -EINVAL;
				goto unmap_memory;
			}
		}
	} else {

		resource = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (unlikely(!resource))
			return -ENXIO;
		uport->mapbase = resource->start;
		uport->membase = ioremap(uport->mapbase,
					resource_size(resource));
		if (unlikely(!uport->membase))
			return -ENOMEM;

		uport->irq = platform_get_irq(pdev, 0);
		if (unlikely((int)uport->irq < 0)) {
			MSM_HS_ERR("UART IRQ Failed.\n");
			iounmap(uport->membase);
			return -ENXIO;
		}
	}

	if (pdata == NULL)
		msm_uport->wakeup.irq = -1;
	else {
		msm_uport->wakeup.irq = pdata->wakeup_irq;
		msm_uport->wakeup.ignore = 1;
		msm_uport->wakeup.inject_rx = pdata->inject_rx_on_wakeup;
		msm_uport->wakeup.rx_to_inject = pdata->rx_to_inject;

		if (unlikely(msm_uport->wakeup.irq < 0)) {
			ret = -ENXIO;
			goto deregister_bus_client;
		}

		if (is_blsp_uart(msm_uport)) {
			msm_uport->bam_tx_ep_pipe_index =
					pdata->bam_tx_ep_pipe_index;
			msm_uport->bam_rx_ep_pipe_index =
					pdata->bam_rx_ep_pipe_index;
		}
	}

	if (!is_blsp_uart(msm_uport)) {

		resource = platform_get_resource_byname(pdev,
					IORESOURCE_DMA, "uartdm_channels");
		if (unlikely(!resource)) {
			ret =  -ENXIO;
			goto deregister_bus_client;
		}

		msm_uport->dma_tx_channel = resource->start;
		msm_uport->dma_rx_channel = resource->end;

		resource = platform_get_resource_byname(pdev,
					IORESOURCE_DMA, "uartdm_crci");
		if (unlikely(!resource)) {
			ret = -ENXIO;
			goto deregister_bus_client;
		}

		msm_uport->dma_tx_crci = resource->start;
		msm_uport->dma_rx_crci = resource->end;
	}

	uport->iotype = UPIO_MEM;
	uport->fifosize = 64;
	uport->ops = &msm_hs_ops;
	uport->flags = UPF_BOOT_AUTOCONF;
	uport->uartclk = 7372800;
	msm_uport->imr_reg = 0x0;

	msm_uport->clk = clk_get(&pdev->dev, "core_clk");
	if (IS_ERR(msm_uport->clk)) {
		ret = PTR_ERR(msm_uport->clk);
		goto deregister_bus_client;
	}

	msm_uport->pclk = clk_get(&pdev->dev, "iface_clk");
	/*
	 * Some configurations do not require explicit pclk control so
	 * do not flag error on pclk get failure.
	 */
	if (IS_ERR(msm_uport->pclk))
		msm_uport->pclk = NULL;

	ret = clk_set_rate(msm_uport->clk, uport->uartclk);
	if (ret) {
		MSM_HS_WARN("Error setting clock rate on UART\n");
		goto put_clk;
	}

	msm_uport->hsuart_wq = alloc_workqueue("k_hsuart",
					WQ_UNBOUND | WQ_MEM_RECLAIM, 1);
	if (!msm_uport->hsuart_wq) {
		MSM_HS_ERR("%s(): Unable to create workqueue hsuart_wq\n",
								__func__);
		ret =  -ENOMEM;
		goto put_clk;
	}

	INIT_WORK(&msm_uport->clock_off_w, hsuart_clock_off_work);

	/* Init work for sps_disconnect in stop_rx_locked */
	INIT_WORK(&msm_uport->disconnect_rx_endpoint,
				hsuart_disconnect_rx_endpoint_work);
	mutex_init(&msm_uport->clk_mutex);
	atomic_set(&msm_uport->clk_count, 0);


	/* Initialize SPS HW connected with UART core */
	if (is_blsp_uart(msm_uport)) {
		ret = msm_hs_sps_init(msm_uport);
		if (unlikely(ret)) {
			MSM_HS_ERR("SPS Initialization failed ! err=%d", ret);
			goto destroy_mutex;
		}
	}

	msm_hs_clock_vote(msm_uport);

	ret = uartdm_init_port(uport);
	if (unlikely(ret)) {
		goto err_clock;
	}

	/* configure the CR Protection to Enable */
	msm_hs_write(uport, UART_DM_CR, CR_PROTECTION_EN);

	/*
	 * Enable Command register protection before going ahead as this hw
	 * configuration makes sure that issued cmd to CR register gets complete
	 * before next issued cmd start. Hence mb() requires here.
	 */
	mb();

	/*
	* Set RX_BREAK_ZERO_CHAR_OFF and RX_ERROR_CHAR_OFF
	* so any rx_break and character having parity of framing
	* error don't enter inside UART RX FIFO.
	*/
	data = msm_hs_read(uport, UART_DM_MR2);
	data |= (UARTDM_MR2_RX_BREAK_ZERO_CHAR_OFF |
			UARTDM_MR2_RX_ERROR_CHAR_OFF);
	msm_hs_write(uport, UART_DM_MR2, data);
	mb();

	msm_uport->clk_state = MSM_HS_CLK_PORT_OFF;
	hrtimer_init(&msm_uport->clk_off_timer, CLOCK_MONOTONIC,
		     HRTIMER_MODE_REL);
	msm_uport->clk_off_timer.function = msm_hs_clk_off_retry;
	msm_uport->clk_off_delay = ktime_set(0, 1000000);  /* 1ms */

	ret = sysfs_create_file(&pdev->dev.kobj, &dev_attr_clock.attr);
	if (unlikely(ret))
		goto err_clock;

	msm_serial_debugfs_init(msm_uport, pdev->id);

	uport->line = pdev->id;
	if (pdata != NULL && pdata->userid && pdata->userid <= UARTDM_NR)
		uport->line = pdata->userid;
	ret = uart_add_one_port(&msm_hs_driver, uport);
	if (!ret) {
		msm_hs_clock_unvote(msm_uport);
		return ret;
	}

err_clock:
	msm_hs_clock_unvote(msm_uport);

destroy_mutex:
	mutex_destroy(&msm_uport->clk_mutex);
	destroy_workqueue(msm_uport->hsuart_wq);

put_clk:
	if (msm_uport->pclk)
		clk_put(msm_uport->pclk);

	if (msm_uport->clk)
		clk_put(msm_uport->clk);

deregister_bus_client:
	if (is_blsp_uart(msm_uport))
		msm_bus_scale_unregister_client(msm_uport->bus_perf_client);
unmap_memory:
	iounmap(uport->membase);
	if (is_blsp_uart(msm_uport))
		iounmap(msm_uport->bam_base);

	return ret;
}

static int __init msm_serial_hs_init(void)
{
	int ret;

	ipc_msm_hs_log_ctxt = ipc_log_context_create(IPC_MSM_HS_LOG_PAGES,
							"msm_serial_hs");
	if (!ipc_msm_hs_log_ctxt)
		MSM_HS_WARN("%s: error creating logging context", __func__);

	ret = uart_register_driver(&msm_hs_driver);
	if (unlikely(ret)) {
		MSM_HS_WARN("%s failed to load\n", __func__);
		return ret;
	}
	debug_base = debugfs_create_dir("msm_serial_hs", NULL);
	if (IS_ERR_OR_NULL(debug_base))
		MSM_HS_INFO("msm_serial_hs: Cannot create debugfs dir\n");

	ret = platform_driver_register(&msm_serial_hs_platform_driver);
	if (ret) {
		MSM_HS_ERR("%s failed to load\n", __FUNCTION__);
		debugfs_remove_recursive(debug_base);
		uart_unregister_driver(&msm_hs_driver);
		return ret;
	}

	MSM_HS_INFO("msm_serial_hs module loaded\n");
	return ret;
}

/*
 *  Called by the upper layer when port is closed.
 *     - Disables the port
 *     - Unhook the ISR
 */
static void msm_hs_shutdown(struct uart_port *uport)
{
	int ret;
	unsigned int data;
	unsigned long flags;
	struct msm_hs_port *msm_uport = UARTDM_TO_MSM(uport);
	struct platform_device *pdev = to_platform_device(uport->dev);
	const struct msm_serial_hs_platform_data *pdata =
				pdev->dev.platform_data;
	struct msm_hs_tx *tx = &msm_uport->tx;
	struct sps_pipe *sps_pipe_handle = tx->cons.pipe_handle;

	msm_hs_clock_vote(msm_uport);
	if (msm_uport->tx.dma_in_flight) {
		if (!is_blsp_uart(msm_uport)) {
			spin_lock_irqsave(&uport->lock, flags);
			/* disable UART TX interface to DM */
			data = msm_hs_read(uport, UART_DM_DMEN);
			data &= ~UARTDM_TX_DM_EN_BMSK;
			msm_hs_write(uport, UART_DM_DMEN, data);
			/* turn OFF UART Transmitter */
			msm_hs_write(uport, UART_DM_CR,
				UARTDM_CR_TX_DISABLE_BMSK);
			/* reset UART TX */
			msm_hs_write(uport, UART_DM_CR, RESET_TX);
			/* reset UART TX Error */
			msm_hs_write(uport, UART_DM_CR, RESET_TX_ERROR);
			msm_uport->tx.flush = FLUSH_STOP;
			spin_unlock_irqrestore(&uport->lock, flags);
			/* discard flush */
			msm_dmov_flush(msm_uport->dma_tx_channel, 0);
			ret = wait_event_timeout(msm_uport->tx.wait,
				msm_uport->tx.flush == FLUSH_SHUTDOWN, 100);
			if (!ret)
				MSM_HS_ERR("%s():HSUART TX Stalls.\n", __func__);
		} else {
			/* BAM Disconnect for TX */
			ret = sps_disconnect(sps_pipe_handle);
			if (ret)
				MSM_HS_ERR("%s(): sps_disconnect failed\n",
							__func__);
		}
	}
	tasklet_kill(&msm_uport->tx.tlet);
	BUG_ON(msm_uport->rx.flush < FLUSH_STOP);
	wait_event(msm_uport->rx.wait, msm_uport->rx.flush == FLUSH_SHUTDOWN);
	tasklet_kill(&msm_uport->rx.tlet);
	cancel_delayed_work_sync(&msm_uport->rx.flip_insert_work);
	flush_workqueue(msm_uport->hsuart_wq);
	pm_runtime_disable(uport->dev);

	/* Disable the transmitter */
	msm_hs_write(uport, UART_DM_CR, UARTDM_CR_TX_DISABLE_BMSK);
	/* Disable the receiver */
	msm_hs_write(uport, UART_DM_CR, UARTDM_CR_RX_DISABLE_BMSK);

	msm_uport->imr_reg = 0;
	msm_hs_write(uport, UART_DM_IMR, msm_uport->imr_reg);
	/*
	 * Complete all device write before actually disabling uartclk.
	 * Hence mb() requires here.
	 */
	mb();

	msm_hs_clock_unvote(msm_uport);
	if (msm_uport->clk_state != MSM_HS_CLK_OFF) {
		/* to balance clk_state */
		msm_hs_clock_unvote(msm_uport);
		wake_unlock(&msm_uport->dma_wake_lock);
	}

	msm_uport->clk_state = MSM_HS_CLK_PORT_OFF;
	dma_unmap_single(uport->dev, msm_uport->tx.dma_base,
			 UART_XMIT_SIZE, DMA_TO_DEVICE);

	if (use_low_power_wakeup(msm_uport))
		irq_set_irq_wake(msm_uport->wakeup.irq, 0);

	/* Free the interrupt */
	free_irq(uport->irq, msm_uport);
	if (use_low_power_wakeup(msm_uport))
		free_irq(msm_uport->wakeup.irq, msm_uport);

	if (is_blsp_uart(msm_uport)) {
		msm_hs_unconfig_uart_gpios(uport);
	} else {
		if (pdata && pdata->gpio_config)
			if (pdata->gpio_config(0))
				dev_err(uport->dev, "GPIO config error\n");
	}
}

static void __exit msm_serial_hs_exit(void)
{
	MSM_HS_INFO("msm_serial_hs module removed\n");
	debugfs_remove_recursive(debug_base);
	platform_driver_unregister(&msm_serial_hs_platform_driver);
	uart_unregister_driver(&msm_hs_driver);
}

static int msm_hs_runtime_idle(struct device *dev)
{
	/*
	 * returning success from idle results in runtime suspend to be
	 * called
	 */
	return 0;
}

static int msm_hs_runtime_resume(struct device *dev)
{
	struct platform_device *pdev = container_of(dev, struct
						    platform_device, dev);
	struct msm_hs_port *msm_uport = get_matching_hs_port(pdev);

	/* This check should not fail
	 * During probe, we set uport->line to either pdev->id or userid */
	if (msm_uport)
		msm_hs_request_clock_on(&msm_uport->uport);

	return 0;
}

static int msm_hs_runtime_suspend(struct device *dev)
{
	struct platform_device *pdev = container_of(dev, struct
						    platform_device, dev);
	struct msm_hs_port *msm_uport = get_matching_hs_port(pdev);

	/* This check should not fail
	 * During probe, we set uport->line to either pdev->id or userid */
	if (msm_uport)
		msm_hs_request_clock_off(&msm_uport->uport);
	return 0;
}

static const struct dev_pm_ops msm_hs_dev_pm_ops = {
	.runtime_suspend = msm_hs_runtime_suspend,
	.runtime_resume  = msm_hs_runtime_resume,
	.runtime_idle    = msm_hs_runtime_idle,
};


static struct platform_driver msm_serial_hs_platform_driver = {
	.probe	= msm_hs_probe,
	.remove = __devexit_p(msm_hs_remove),
	.driver = {
		.name = "msm_serial_hs",
		.pm   = &msm_hs_dev_pm_ops,
		.of_match_table = msm_hs_match_table,
	},
};

static struct uart_driver msm_hs_driver = {
	.owner = THIS_MODULE,
	.driver_name = "msm_serial_hs",
	.dev_name = "ttyHS",
	.nr = UARTDM_NR,
	.cons = 0,
};

static struct uart_ops msm_hs_ops = {
	.tx_empty = msm_hs_tx_empty,
	.set_mctrl = msm_hs_set_mctrl_locked,
	.get_mctrl = msm_hs_get_mctrl_locked,
	.stop_tx = msm_hs_stop_tx_locked,
	.start_tx = msm_hs_start_tx_locked,
	.stop_rx = msm_hs_stop_rx_locked,
	.enable_ms = msm_hs_enable_ms_locked,
	.break_ctl = msm_hs_break_ctl,
	.startup = msm_hs_startup,
	.shutdown = msm_hs_shutdown,
	.set_termios = msm_hs_set_termios,
	.type = msm_hs_type,
	.config_port = msm_hs_config_port,
	.release_port = msm_hs_release_port,
	.request_port = msm_hs_request_port,
	.flush_buffer = msm_hs_flush_buffer,
	.ioctl = msm_hs_ioctl,
};

module_init(msm_serial_hs_init);
module_exit(msm_serial_hs_exit);
MODULE_DESCRIPTION("High Speed UART Driver for the MSM chipset");
MODULE_VERSION("1.2");
MODULE_LICENSE("GPL v2");
