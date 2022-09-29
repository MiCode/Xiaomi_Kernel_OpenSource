// SPDX-License-Identifier: GPL-2.0+
/*
 * Mediatek 8250 driver.
 *
 * Copyright (c) 2014 MundoReader S.L.
 * Author: Matthias Brugger <matthias.bgg@gmail.com>
 */
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/serial_8250.h>
#include <linux/serial_reg.h>
#include <linux/console.h>
#include <linux/dma-mapping.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/sched/clock.h>

#include "8250.h"
#if IS_ENABLED(CONFIG_MTK_UARTHUB)
#include "../../../misc/mediatek/uarthub/common/uarthub_drv_export.h"
#endif
#ifdef CONFIG_SERIAL_8250_DMA
#include "../../../dma/mediatek/mtk-uart-apdma.h"
#endif

#define MTK_UART_HIGHS		0x09	/* Highspeed register */
#define MTK_UART_SAMPLE_COUNT	0x0a	/* Sample count register */
#define MTK_UART_SAMPLE_POINT	0x0b	/* Sample point register */
#define MTK_UART_RATE_FIX	0x0d	/* UART Rate Fix Register */
#define MTK_UART_ESCAPE_DAT	0x10	/* Escape Character register */
#define MTK_UART_ESCAPE_EN	0x11	/* Escape Enable register */
#define MTK_UART_DMA_EN		0x13	/* DMA Enable register */
#define MTK_UART_RXTRI_AD	0x14	/* RX Trigger address */
#define MTK_UART_FRACDIV_L	0x15	/* Fractional divider LSB address */
#define MTK_UART_FRACDIV_M	0x16	/* Fractional divider MSB address */
#define MTK_UART_IER_XOFFI	0x20	/* Enable XOFF character interrupt */
#define MTK_UART_IER_RTSI	0x40	/* Enable RTS Modem status interrupt */
#define MTK_UART_IER_CTSI	0x80	/* Enable CTS Modem status interrupt */
#define MTK_UART_XON1		0X28
#define MTK_UART_XOFF1		0X2A
#define MTK_UART_XON2		0X29
#define MTK_UART_XOFF2		0X2B

#define MTK_UART_DEBUG0		0x18
#define MTK_UART_DEBUG1		0x19
#define MTK_UART_DEBUG2		0x1A
#define MTK_UART_DEBUG3		0x1B
#define MTK_UART_DEBUG4		0x1C
#define MTK_UART_DEBUG5		0x1D
#define MTK_UART_DEBUG6		0x1E
#define MTK_UART_DEBUG7		0x1F
#define MTK_UART_DEBUG8		0x20

#define MTK_UART_DLL  0x24
#define MTK_UART_DLH  0x25
#define MTK_UART_FEATURE_SEL  0x27
#define MTK_UART_EFR    0x26

#define MTK_UART_EFR_EN		0x10	/* Enable enhancement feature */
#define MTK_UART_EFR_RTS	0x40	/* Enable hardware rx flow control */
#define MTK_UART_EFR_CTS	0x80	/* Enable hardware tx flow control */
#define MTK_UART_EFR_NO_SW_FC	0x0	/* no sw flow control */
#define MTK_UART_EFR_XON1_XOFF1	0xa	/* XON1/XOFF1 as sw flow control */
#define MTK_UART_EFR_XON2_XOFF2	0x5	/* XON2/XOFF2 as sw flow control */
#define MTK_UART_EFR_SW_FC_MASK	0xf	/* Enable CTS Modem status interrupt */
#define MTK_UART_EFR_HW_FC	(MTK_UART_EFR_RTS | MTK_UART_EFR_CTS)
#define MTK_UART_DMA_EN_TX	0x2
#define MTK_UART_DMA_EN_RX	0x5

#define MTK_UART_ESCAPE_CHAR	0xdb	/* Escape char added under sw fc */
#define MTK_UART_RX_SIZE	0x8000
#define MTK_UART_TX_TRIGGER	1
#define MTK_UART_RX_TRIGGER	MTK_UART_RX_SIZE

#ifdef CONFIG_FPGA_EARLY_PORTING
#define MTK_UART_FPGA_CLK  10000000
#define MTK_UART_FPGA_BAUD 921600
#endif

#define MAX_POLLING_CNT 8
#define LOG_BUF_SIZE 30

#define MTK_UART_HUB_BAUD 12000000
#define UART_DUMP_RECORE_NUM 10
#define UART_DUMP_BUF_LEN PAGE_SIZE
#define CONFIG_UART_DATA_RECORD

#define UART_REG_READ(addr) \
	(*((unsigned int *)(addr)))

#define UART_REG_WRITE(addr, data) do {\
	writel(data, (void *)addr); \
	mb(); /* make sure register access in order */ \
} while (0)

#ifdef CONFIG_SERIAL_8250_DMA
enum dma_rx_status {
	DMA_RX_START = 0,
	DMA_RX_RUNNING = 1,
	DMA_RX_SHUTDOWN = 2,
};
#endif

struct mtk8250_dump {
	unsigned long long trans_time;
	unsigned int trans_len;
	int r_rx_pos;
	int r_copied;
	int port_id;
	unsigned long long tty_port_addr;
	unsigned char rec_buf[UART_DUMP_BUF_LEN];
};

struct mtk8250_info_dump {
	unsigned long long rec_total;
	struct mtk8250_dump rec[UART_DUMP_RECORE_NUM];
};

struct mtk8250_data {
	int			line;
	unsigned int		rx_pos;
	unsigned int		clk_count;
	struct clk		*uart_clk;
	struct clk		*bus_clk;
	struct uart_8250_dma	*dma;
#ifdef CONFIG_SERIAL_8250_DMA
	enum dma_rx_status	rx_status;
#endif
	int			rx_wakeup_irq;
	unsigned int   support_hub;
	unsigned int   hub_baud;
	struct mutex uart_mutex;
};

struct mtk8250_comp {
	unsigned int support_hub;
};

struct mtk8250_reg_data {
	unsigned int addr;
	unsigned int mask;
	unsigned int val;
	unsigned int toggle;
	unsigned int addr_sta;
};

struct mtk8250_reset_data {
	unsigned int addr;
	unsigned int addr_set;
	unsigned int mask_set;
	unsigned int val_set;
	unsigned int addr_clr;
	unsigned int mask_clr;
	unsigned int val_clr;
};
/* flow control mode */
enum {
	MTK_UART_FC_NONE,
	MTK_UART_FC_SW,
	MTK_UART_FC_HW,
};

unsigned int uart_reg_buf[LOG_BUF_SIZE];

static struct mtk8250_reg_data peri_wakeup = {0};
struct mtk8250_info_dump rx_record;

static struct mtk8250_reset_data peri_reset = {0};

static void mtk8250_clear_wakeup(void)
{
	/*clear wakeup*/
	void __iomem *peri_remap_wakeup = NULL;
	void __iomem *peri_remap_wakeup_sta = NULL;

	peri_remap_wakeup = ioremap(peri_wakeup.addr, 0x10);
	if (!peri_remap_wakeup) {
		pr_notice("[%s] peri_remap_wakeup(%x) ioremap fail\n",
			__func__, peri_wakeup.addr);
		return;
	}
	peri_remap_wakeup_sta = ioremap(peri_wakeup.addr_sta, 0x10);
	if (!peri_remap_wakeup_sta) {
		pr_notice("[%s] peri_remap_wakeup_sta(%x) ioremap fail\n",
			__func__, peri_wakeup.addr_sta);
		return;
	}

	UART_REG_WRITE(peri_remap_wakeup,
		((UART_REG_READ(peri_remap_wakeup)
			& (~peri_wakeup.mask)) | peri_wakeup.val));
	UART_REG_WRITE(peri_remap_wakeup,
		((UART_REG_READ(peri_remap_wakeup)
			& (~peri_wakeup.mask)) | peri_wakeup.toggle));
	pr_info("%s wakeup:0x%x sta:0x%x\n", __func__,
		(UART_REG_READ(peri_remap_wakeup)),
		(UART_REG_READ(peri_remap_wakeup_sta)));

	if (peri_remap_wakeup)
		iounmap(peri_remap_wakeup);

	if (peri_remap_wakeup_sta)
		iounmap(peri_remap_wakeup_sta);
}

static void mtk8250_reset_peri(void)
{
	void __iomem *peri_remap_reset = NULL;
	void __iomem *peri_remap_reset_set = NULL;
	void __iomem *peri_remap_reset_clr = NULL;

	peri_remap_reset = ioremap(peri_reset.addr, 0x10);
	if (!peri_remap_reset) {
		pr_notice("[%s] peri_reset(%x) ioremap fail\n",
				__func__, peri_reset.addr);
		return;
	}
	peri_remap_reset_set = ioremap(peri_reset.addr_set, 0x10);
	if (!peri_remap_reset_set) {
		pr_notice("[%s] peri_reset.addr_set(%x) ioremap fail\n",
				__func__, peri_reset.addr_set);
		return;
	}
	peri_remap_reset_clr = ioremap(peri_reset.addr_clr, 0x10);
	if (!peri_remap_reset_clr) {
		pr_notice("[%s] peri_reset.addr_clr(%x) ioremap fail\n",
				__func__, peri_reset.addr_clr);
		return;
	}

	UART_REG_WRITE(peri_remap_reset_set,
		((UART_REG_READ(peri_remap_reset)
			& (~peri_reset.mask_set)) | peri_reset.val_set));
	pr_info("%s peri_reset begin:0x%x\n", __func__, (UART_REG_READ(peri_remap_reset)));

	UART_REG_WRITE(peri_remap_reset_clr,
		((UART_REG_READ(peri_remap_reset)
			& (~peri_reset.mask_clr)) | peri_reset.val_clr));
	pr_info("%s peri_reset end:0x%x\n", __func__, (UART_REG_READ(peri_remap_reset)));

	if (peri_remap_reset)
		iounmap(peri_remap_reset);

	if (peri_remap_reset_set)
		iounmap(peri_remap_reset_set);

	if (peri_remap_reset_clr)
		iounmap(peri_remap_reset_clr);
}

#if IS_ENABLED(CONFIG_MTK_UARTHUB)
int mtk8250_uart_hub_get_host_fw_own_status(void)
{
	#if defined(KERNEL_UARTHUB_get_host_set_fw_own_status)
		return KERNEL_UARTHUB_get_host_set_fw_own_status();
	#else
		return 0;
	#endif
}
EXPORT_SYMBOL(mtk8250_uart_hub_get_host_fw_own_status);

int mtk8250_uart_hub_reset_flow_ctrl(void)
{
	#if defined(KERNEL_UARTHUB_reset_flow_control)
		return KERNEL_UARTHUB_reset_flow_control();
	#else
		return 0;
	#endif
}
EXPORT_SYMBOL(mtk8250_uart_hub_reset_flow_ctrl);

int mtk8250_uart_hub_get_host_wakeup_status(void)
{
	#if defined(KERNEL_UARTHUB_get_host_wakeup_status)
		return KERNEL_UARTHUB_get_host_wakeup_status();
	#else
		return 0;
	#endif
}
EXPORT_SYMBOL(mtk8250_uart_hub_get_host_wakeup_status);

int mtk8250_uart_hub_dev0_set_tx_request(void)
{
	#if defined(KERNEL_UARTHUB_dev0_set_tx_request)
		return KERNEL_UARTHUB_dev0_set_tx_request();
	#else
		return 0;
	#endif
}
EXPORT_SYMBOL(mtk8250_uart_hub_dev0_set_tx_request);

int mtk8250_uart_hub_dev0_set_rx_request(void)
{
	#if defined(KERNEL_UARTHUB_dev0_set_rx_request)
		return KERNEL_UARTHUB_dev0_set_rx_request();
	#else
		return 0;
	#endif
}
EXPORT_SYMBOL(mtk8250_uart_hub_dev0_set_rx_request);
int mtk8250_uart_hub_dev0_clear_tx_request(void)
{
	#if defined(KERNEL_UARTHUB_dev0_clear_tx_request)
		return KERNEL_UARTHUB_dev0_clear_tx_request();
	#else
		return 0;
	#endif
}
EXPORT_SYMBOL(mtk8250_uart_hub_dev0_clear_tx_request);
int mtk8250_uart_hub_dev0_clear_rx_request(void)
{
	/*clear ap uart*/
	mtk8250_clear_wakeup();

	#if defined(KERNEL_UARTHUB_dev0_clear_rx_request)
		return KERNEL_UARTHUB_dev0_clear_rx_request();
	#else
		return 0;
	#endif
}
EXPORT_SYMBOL(mtk8250_uart_hub_dev0_clear_rx_request);
int mtk8250_uart_hub_get_uart_cmm_rx_count(void)
{
	#if defined(KERNEL_UARTHUB_get_uart_cmm_rx_count)
		return KERNEL_UARTHUB_get_uart_cmm_rx_count();
	#else
		return 0;
	#endif
}
EXPORT_SYMBOL(mtk8250_uart_hub_get_uart_cmm_rx_count);

int mtk8250_uart_hub_dump_with_tag(const char *tag)
{
	#if defined(KERNEL_UARTHUB_dump_debug_info_with_tag)
		return KERNEL_UARTHUB_dump_debug_info_with_tag(tag);
	#else
		return 0;
	#endif
}
EXPORT_SYMBOL(mtk8250_uart_hub_dump_with_tag);

int mtk8250_uart_hub_fifo_ctrl(int ctrl)
{
	#if defined(KERNEL_UARTHUB_md_adsp_fifo_ctrl)
		return KERNEL_UARTHUB_md_adsp_fifo_ctrl(ctrl);
	#else
		return 0;
	#endif
}
EXPORT_SYMBOL(mtk8250_uart_hub_fifo_ctrl);

int mtk8250_uart_hub_assert_bit_ctrl(int ctrl)
{
	#if defined(KERNEL_UARTHUB_assert_state_ctrl)
		return KERNEL_UARTHUB_assert_state_ctrl(ctrl);
	#else
		return 0;
	#endif
}
EXPORT_SYMBOL(mtk8250_uart_hub_assert_bit_ctrl);

int mtk8250_uart_hub_dump(void)
{
	#if defined(KERNEL_UARTHUB_dump_debug_info)
		return KERNEL_UARTHUB_dump_debug_info();
	#else
		return 0;
	#endif
}
EXPORT_SYMBOL(mtk8250_uart_hub_dump);

int mtk8250_uart_hub_reset(void)
{
	#if defined(KERNEL_UARTHUB_sw_reset)
		return KERNEL_UARTHUB_sw_reset();
	#else
		return 0;
	#endif
}
EXPORT_SYMBOL(mtk8250_uart_hub_reset);

int mtk8250_uart_hub_register_cb(UARTHUB_IRQ_CB irq_callback)
{
	#if defined(KERNEL_UARTHUB_irq_register_cb)
		return KERNEL_UARTHUB_irq_register_cb(irq_callback);
	#else
		return 0;
	#endif
}
EXPORT_SYMBOL(mtk8250_uart_hub_register_cb);

int mtk8250_uart_hub_enable_bypass_mode(int bypass)
{
	#if defined(KERNEL_UARTHUB_bypass_mode_ctrl)
		return KERNEL_UARTHUB_bypass_mode_ctrl(bypass);
	#else
		return 0;
	#endif
}
EXPORT_SYMBOL(mtk8250_uart_hub_enable_bypass_mode);

int mtk8250_uart_hub_set_request(void)
{
	#if defined(KERNEL_UARTHUB_dev0_set_txrx_request)
		return KERNEL_UARTHUB_dev0_set_txrx_request();
	#else
		return 0;
	#endif
}
EXPORT_SYMBOL(mtk8250_uart_hub_set_request);

int mtk8250_uart_hub_clear_request(void)
{
	/*clear ap uart*/
	mtk8250_clear_wakeup();

	#if defined(KERNEL_UARTHUB_dev0_clear_txrx_request)
		return KERNEL_UARTHUB_dev0_clear_txrx_request();
	#else
		return 0;
	#endif
}
EXPORT_SYMBOL(mtk8250_uart_hub_clear_request);

int mtk8250_uart_hub_is_assert(void)
{
	#if defined(KERNEL_UARTHUB_is_assert_state)
		return KERNEL_UARTHUB_is_assert_state();
	#else
		return 0;
	#endif
}
EXPORT_SYMBOL(mtk8250_uart_hub_is_assert);

int mtk8250_uart_hub_is_ready(void)
{
	#if defined(KERNEL_UARTHUB_dev0_is_uarthub_ready)
		return KERNEL_UARTHUB_dev0_is_uarthub_ready();
	#else
		return 0;
	#endif
}
EXPORT_SYMBOL(mtk8250_uart_hub_is_ready);

#endif

#ifdef CONFIG_SERIAL_8250_DMA
static void mtk8250_save_uart_apdma_reg(struct dma_chan *chan, unsigned int *reg_buf)
{
	#if defined(KERNEL_mtk_save_uart_apdma_reg)
		KERNEL_mtk_save_uart_apdma_reg(chan, reg_buf);
	#endif
}

static void mtk8250_uart_apdma_data_dump(struct dma_chan *chan)
{
	#if defined(KERNEL_mtk_uart_apdma_data_dump)
		KERNEL_mtk_uart_apdma_data_dump(chan);
	#endif
}

static void mtk8250_uart_rx_setting(struct dma_chan *chan, int copied, int total)
{
	#if defined(KERNEL_mtk_uart_rx_setting)
		KERNEL_mtk_uart_rx_setting(chan, copied, total);
	#endif
}

static void mtk8250_uart_apdma_start_record(struct dma_chan *chan)
{
	#if defined(KERNEL_mtk_uart_apdma_start_record)
		KERNEL_mtk_uart_apdma_start_record(chan);
	#endif
}

static void mtk8250_uart_apdma_end_record(struct dma_chan *chan)
{
	#if defined(KERNEL_mtk_uart_apdma_end_record)
		KERNEL_mtk_uart_apdma_end_record(chan);
	#endif
}

static int mtk8250_uart_rx_dma(struct uart_8250_port *up)
{
	unsigned int iir = 0;
	unsigned int tmo = MAX_POLLING_CNT;

	while (tmo--) {
		iir = serial_in(up, UART_IIR);
		if (iir & UART_IIR_NO_INT)
			break;
	}
	serial_out(up, UART_IER, 0);
	return 0;
}

static void mtk8250_uart_get_apdma_rpt(struct dma_chan *chan, unsigned int *rpt)
{
	#if defined(KERNEL_mtk_uart_get_apdma_rpt)
		KERNEL_mtk_uart_get_apdma_rpt(chan, rpt);
	#endif
}
#endif

static void mtk_save_uart_reg(struct uart_8250_port *up, unsigned int *reg_buf)
{
	unsigned long flags;

	spin_lock_irqsave(&up->port.lock, flags);
	reg_buf[0] = serial_in(up, UART_LCR);
	serial_out(up, MTK_UART_FEATURE_SEL, 0x01);
	reg_buf[1] = serial_in(up, MTK_UART_HIGHS);
	reg_buf[2] = serial_in(up, MTK_UART_SAMPLE_COUNT);
	reg_buf[3] = serial_in(up, MTK_UART_SAMPLE_POINT);
	reg_buf[4] = serial_in(up, MTK_UART_DLL);
	reg_buf[5] = serial_in(up, MTK_UART_DLH);
	reg_buf[6] = serial_in(up, MTK_UART_FRACDIV_L);
	reg_buf[7] = serial_in(up, MTK_UART_FRACDIV_M);
	reg_buf[8] = serial_in(up, MTK_UART_EFR);
	reg_buf[9] = serial_in(up, MTK_UART_XON1);
	reg_buf[10] = serial_in(up, MTK_UART_XOFF1);
	serial_out(up, MTK_UART_FEATURE_SEL, 0x00);
	reg_buf[11] = serial_in(up, MTK_UART_DEBUG0);
	reg_buf[12] = serial_in(up, MTK_UART_DEBUG1);
	reg_buf[13] = serial_in(up, MTK_UART_DEBUG2);
	reg_buf[14] = serial_in(up, MTK_UART_DEBUG3);
	reg_buf[15] = serial_in(up, MTK_UART_DEBUG4);
	reg_buf[16] = serial_in(up, MTK_UART_DEBUG5);
	reg_buf[17] = serial_in(up, MTK_UART_DEBUG6);
	reg_buf[18] = serial_in(up, MTK_UART_DEBUG7);
	reg_buf[19] = serial_in(up, MTK_UART_DEBUG8);
	reg_buf[20] = serial_in(up, UART_IER);
	reg_buf[21] = serial_in(up, UART_IIR);
	reg_buf[22] = serial_in(up, UART_LSR);
	reg_buf[23] = serial_in(up, MTK_UART_DMA_EN);
	spin_unlock_irqrestore(&up->port.lock, flags);
}

void mtk8250_data_dump(void)
{
#ifdef CONFIG_UART_DATA_RECORD
	int idx = 0;
	int count = 0;

	if (rx_record.rec_total > UART_DUMP_RECORE_NUM)
		idx = (unsigned int)((rx_record.rec_total + 1) % UART_DUMP_RECORE_NUM);

	while (count < min_t(unsigned long long, UART_DUMP_RECORE_NUM, rx_record.rec_total)) {
		unsigned int cnt_ = 0;
		unsigned int cyc_ = 0;
		unsigned int len_ = rx_record.rec[idx].trans_len;
		unsigned char raw_buf[256 * 3 + 4];
		const unsigned char *ptr = rx_record.rec[idx].rec_buf;
		unsigned long long endtime = rx_record.rec[idx].trans_time;
		unsigned long ns = do_div(endtime, 1000000000);

		pr_info("[%s] [%5lu.%06lu] total=%llu,idx=%d,port_id=%d,len=%d,pos=%d,copy=%d\n",
			__func__, (unsigned long)endtime, ns / 1000,
			rx_record.rec_total, idx, rx_record.rec[idx].port_id, len_,
			rx_record.rec[idx].r_rx_pos, rx_record.rec[idx].r_copied);

		pr_info("[%s] [%5lu.%06lu] tty_port: %llu\n",
			__func__, (unsigned long)endtime, ns / 1000,
			rx_record.rec[idx].tty_port_addr);

		if (len_ > UART_DUMP_BUF_LEN)
			len_ = UART_DUMP_BUF_LEN;
		for (cyc_ = 0; cyc_ < len_;) {
			unsigned int cnt_min = (((len_ - cyc_) < 256) ? (len_ - cyc_) : 256);

			for (cnt_ = 0; cnt_ < cnt_min; cnt_++)
				(void)snprintf(raw_buf + 3 * cnt_, 4, "%02X ", ptr[cnt_ + cyc_]);
			raw_buf[3 * cnt_] = '\0';
			pr_info("[%d] data=%s\n", cyc_, raw_buf);
			cyc_ += 256;
		}
		count++;
		idx++;
		idx = idx%UART_DUMP_RECORE_NUM;
	}
#else
	pr_info("[%s] UART_DATA_RECORD is not config\n", __func__);
#endif
}

int mtk8250_uart_dump(struct tty_struct *tty)
{
	int ret = 0;
	struct uart_state *state = NULL;
	struct uart_port *port = NULL;
	struct uart_8250_port *up = NULL;
	struct mtk8250_data *data = NULL;
	unsigned int uart_reg_buf[LOG_BUF_SIZE];
	unsigned int apdma_rx_reg_buf[LOG_BUF_SIZE];
	unsigned int apdma_tx_reg_buf[LOG_BUF_SIZE];

	if (tty == NULL) {
		pr_info("[%s] para error. tty is NULL\n", __func__);
		ret = -EINVAL;
		goto err_exit;
	}
	state = tty->driver_data;
	if (state == NULL) {
		pr_info("[%s] para error. state is NULL\n", __func__);
		ret = -EINVAL;
		goto err_exit;
	}
	port = state->uart_port;
	if (port == NULL) {
		pr_info("[%s] para error. port is NULL\n", __func__);
		ret = -EINVAL;
		goto err_exit;
	}
	up = up_to_u8250p(port);
	if (up == NULL) {
		pr_info("[%s] para error. up is NULL\n", __func__);
		ret = -EINVAL;
		goto err_exit;
	}
	data = port->private_data;
	if (data == NULL) {
		pr_info("[%s] para error. data is NULL\n", __func__);
		ret = -EINVAL;
		goto err_exit;
	}
	mutex_lock(&data->uart_mutex);
	if (data->clk_count == 0) {
		pr_info("%s: clk_count = %d, clk close, please open ttys[%d]\n", __func__,
		data->clk_count, data->line);
		ret = -EINVAL;
		goto err_unlock_exit;
	}
	if (data->support_hub != 1) {
		pr_info("%s: current port is not hub port\n", __func__);
		ret = -EINVAL;
		goto err_unlock_exit;
	}

	memset(uart_reg_buf, 0, LOG_BUF_SIZE);
	memset(apdma_rx_reg_buf, 0, LOG_BUF_SIZE);
	memset(apdma_tx_reg_buf, 0, LOG_BUF_SIZE);
	mtk_save_uart_reg(up, uart_reg_buf);

#ifdef CONFIG_SERIAL_8250_DMA
	if ((up->dma == NULL) || (up->dma->rxchan == NULL) ||
		(up->dma->txchan == NULL)) {
		pr_info("[%s] para error. up->dma,rx,tx is NULL\n", __func__);
		ret = -EINVAL;
		goto err_unlock_exit;
	}
	mtk8250_save_uart_apdma_reg(up->dma->rxchan, apdma_rx_reg_buf);
	mtk8250_save_uart_apdma_reg(up->dma->txchan, apdma_tx_reg_buf);
#endif

#if IS_ENABLED(CONFIG_MTK_UARTHUB)
	mtk8250_uart_hub_dump_with_tag("8250_uarthub");
#endif

	pr_info("[%s] line=%d,lcr=0x%x,highs=0x%x,count=0x%x,point=0x%x,dll=0x%x,\n"
		"dlh=0x%x,fre_l=0x%x,fre_m=0x%x,efr=0x%x,xon1=0x%x,xoff1=0x%x\n",
		__func__, data->line,
		uart_reg_buf[0], uart_reg_buf[1], uart_reg_buf[2], uart_reg_buf[3],
		uart_reg_buf[4], uart_reg_buf[5], uart_reg_buf[6], uart_reg_buf[7],
		uart_reg_buf[8], uart_reg_buf[9], uart_reg_buf[10]);
	pr_info("[%s] 0x60=0x%x,0x64=0x%x,0x68=0x%x,0x6c=0x%x,\n"
		"0x70=0x%x,0x74=0x%x,0x78=0x%x,0x7c=0x%x,0x80=0x%x,\n"
		"ier=0x%x,iir=0x%x, LSR=0x%x, DMA_EN=0x%x\n",
		__func__, uart_reg_buf[11], uart_reg_buf[12], uart_reg_buf[13],
		uart_reg_buf[14], uart_reg_buf[15], uart_reg_buf[16],
		uart_reg_buf[17], uart_reg_buf[18], uart_reg_buf[19],
		uart_reg_buf[20], uart_reg_buf[21], uart_reg_buf[22],
		uart_reg_buf[23]);
#ifdef CONFIG_SERIAL_8250_DMA
	pr_info("[apdma_rx] int_flag=0x%x,int_en=0x%x,en=0x%x,flush=0x%x,addr=0x%x,\n"
		"len=0x%x,thre=0x%x,wpt=0x%x,rpt=0x%x,int_buf_size=0x%x\n"
		"valid_size=0x%x,left_size=0x%x,debug_stat=0x%x\n",
		apdma_rx_reg_buf[0], apdma_rx_reg_buf[1], apdma_rx_reg_buf[2],
		apdma_rx_reg_buf[3], apdma_rx_reg_buf[4], apdma_rx_reg_buf[5],
		apdma_rx_reg_buf[6], apdma_rx_reg_buf[7], apdma_rx_reg_buf[8],
		apdma_rx_reg_buf[9], apdma_rx_reg_buf[10], apdma_rx_reg_buf[11],
		apdma_rx_reg_buf[12]);
	pr_info("[apdma_tx] int_flag=0x%x,int_en=0x%x,en=0x%x,flush=0x%x,addr=0x%x,\n"
		"len=0x%x,thre=0x%x,wpt=0x%x,rpt=0x%x,int_buf_size=0x%x\n"
		"valid_size=0x%x,left_size=0x%x,debug_stat=0x%x\n",
		apdma_tx_reg_buf[0], apdma_tx_reg_buf[1], apdma_tx_reg_buf[2],
		apdma_tx_reg_buf[3], apdma_tx_reg_buf[4], apdma_tx_reg_buf[5],
		apdma_tx_reg_buf[6], apdma_tx_reg_buf[7], apdma_tx_reg_buf[8],
		apdma_tx_reg_buf[9], apdma_tx_reg_buf[10], apdma_tx_reg_buf[11],
		apdma_tx_reg_buf[12]);
	mtk8250_uart_apdma_data_dump(up->dma->rxchan);
	mtk8250_uart_apdma_data_dump(up->dma->txchan);
#endif

err_unlock_exit:
	mutex_unlock(&data->uart_mutex);

err_exit:
	mtk8250_data_dump();
	return ret;
}
EXPORT_SYMBOL(mtk8250_uart_dump);

void mtk8250_uart_start_record(struct tty_struct *tty)
{
	struct uart_state *state = NULL;
	struct uart_port *port = NULL;
	struct uart_8250_port *up = NULL;
	struct mtk8250_data *data = NULL;

	if (tty == NULL) {
		pr_info("[%s] para error. tty is NULL\n", __func__);
		return;
	}
	state = tty->driver_data;
	if (state == NULL) {
		pr_info("[%s] para error. state is NULL\n", __func__);
		return;
	}
	port = state->uart_port;
	if (port == NULL) {
		pr_info("[%s] para error. port is NULL\n", __func__);
		return;
	}
	up = up_to_u8250p(port);
	if (up == NULL) {
		pr_info("[%s] para error. up is NULL\n", __func__);
		return;
	}
	data = port->private_data;
	if (data == NULL) {
		pr_info("[%s] para error. data is NULL\n", __func__);
		return;
	}
	if (data->clk_count == 0) {
		pr_info("[%s] para error. clk_count = %d, clk close, please open ttys[%d]\n",
			__func__, data->clk_count, data->line);
		return;
	}

	mtk_save_uart_reg(up, uart_reg_buf);

#if IS_ENABLED(CONFIG_MTK_UARTHUB)
	KERNEL_UARTHUB_debug_dump_tx_rx_count("mtk8250_uarthub", DUMP0);
#endif

#ifdef CONFIG_SERIAL_8250_DMA
	if ((up->dma == NULL) || (up->dma->rxchan == NULL) ||
		(up->dma->txchan == NULL)) {
		pr_info("[%s] para error. up->dma,rx,tx is NULL\n", __func__);
		return;
	}
	mtk8250_uart_apdma_start_record(up->dma->rxchan);
	mtk8250_uart_apdma_start_record(up->dma->txchan);
#endif
}
EXPORT_SYMBOL(mtk8250_uart_start_record);


void mtk8250_uart_end_record(struct tty_struct *tty)
{
	struct uart_state *state = NULL;
	struct uart_port *port = NULL;
	struct uart_8250_port *up = NULL;
	struct mtk8250_data *data = NULL;
	unsigned int uart_dbg_reg[LOG_BUF_SIZE];

	if (tty == NULL) {
		pr_info("[%s] para error. tty is NULL\n", __func__);
		return;
	}
	state = tty->driver_data;
	if (state == NULL) {
		pr_info("[%s] para error. state is NULL\n", __func__);
		return;
	}
	port = state->uart_port;
	if (port == NULL) {
		pr_info("[%s] para error. port is NULL\n", __func__);
		return;
	}
	up = up_to_u8250p(port);
	if (up == NULL) {
		pr_info("[%s] para error. up is NULL\n", __func__);
		return;
	}
	data = port->private_data;
	if (data == NULL) {
		pr_info("[%s] para error. data is NULL\n", __func__);
		return;
	}
	if (data->clk_count == 0) {
		pr_info("[%s] para error. clk_count = %d, clk close, please open ttys[%d]\n",
			__func__, data->clk_count, data->line);
		return;
	}

	mtk_save_uart_reg(up, uart_dbg_reg);

	pr_info("[%s] start 0x60=0x%x,0x64=0x%x,0x68=0x%x,0x6c=0x%x,\n"
		"0x70=0x%x,0x74=0x%x,0x78=0x%x,0x7c=0x%x,0x80=0x%x,LSR=0x%x,\n"
		"DMA_EN=0x%x\n",
		__func__, uart_reg_buf[11], uart_reg_buf[12], uart_reg_buf[13],
		uart_reg_buf[14], uart_reg_buf[15], uart_reg_buf[16],
		uart_reg_buf[17], uart_reg_buf[18], uart_reg_buf[19],
		uart_reg_buf[22], uart_reg_buf[23]);

	pr_info("[%s] end 0x60=0x%x,0x64=0x%x,0x68=0x%x,0x6c=0x%x,\n"
		"0x70=0x%x,0x74=0x%x,0x78=0x%x,0x7c=0x%x,0x80=0x%x,,LSR=0x%x,\n"
		"DMA_EN=0x%x\n",
		__func__, uart_dbg_reg[11], uart_dbg_reg[12], uart_dbg_reg[13],
		uart_dbg_reg[14], uart_dbg_reg[15], uart_dbg_reg[16],
		uart_dbg_reg[17], uart_dbg_reg[18], uart_dbg_reg[19],
		uart_dbg_reg[22], uart_dbg_reg[23]);

#if IS_ENABLED(CONFIG_MTK_UARTHUB)
	KERNEL_UARTHUB_debug_dump_tx_rx_count("mtk8250_uarthub", DUMP1);
#endif

#ifdef CONFIG_SERIAL_8250_DMA
	if ((up->dma == NULL) || (up->dma->rxchan == NULL) ||
		(up->dma->txchan == NULL)) {
		pr_info("[%s] para error. up->dma,rx,tx is NULL\n", __func__);
		return;
	}

	mtk8250_uart_apdma_end_record(up->dma->rxchan);
	mtk8250_uart_apdma_end_record(up->dma->txchan);
#endif
}
EXPORT_SYMBOL(mtk8250_uart_end_record);


#ifdef CONFIG_SERIAL_8250_DMA
static void mtk8250_rx_dma(struct uart_8250_port *up);

static void mtk8250_dma_rx_complete(void *param)
{
	struct uart_8250_port *up = param;
	struct uart_8250_dma *dma = up->dma;
	struct mtk8250_data *data = up->port.private_data;
	struct tty_port *tty_port = &up->port.state->port;
	struct dma_tx_state state;
	int copied, total, cnt, copied_sec;
	unsigned char *ptr;
	unsigned long flags;
	unsigned int idx = 0;

	if (data->rx_status == DMA_RX_SHUTDOWN)
		return;

	spin_lock_irqsave(&up->port.lock, flags);

	dmaengine_tx_status(dma->rxchan, dma->rx_cookie, &state);
	total = dma->rx_size - state.residue;
	cnt = total;

	mtk8250_uart_get_apdma_rpt(dma->rxchan, &(data->rx_pos));

	if (data->support_hub == 1) {
		idx = (unsigned int)(rx_record.rec_total % UART_DUMP_RECORE_NUM);
		rx_record.rec_total++;
		rx_record.rec[idx].trans_len = total;
		rx_record.rec[idx].trans_time = sched_clock();
		rx_record.rec[idx].r_rx_pos = data->rx_pos;
		rx_record.rec[idx].port_id = data->line;
	}

	if ((data->rx_pos + cnt) > dma->rx_size)
		cnt = dma->rx_size - data->rx_pos;

	ptr = (unsigned char *)(data->rx_pos + dma->rx_buf);
	copied = tty_insert_flip_string(tty_port, ptr, cnt);
	data->rx_pos += copied;

#ifdef CONFIG_UART_DATA_RECORD
	if (data->support_hub == 1) {
		if (total <= UART_DUMP_BUF_LEN)
			memcpy(rx_record.rec[idx].rec_buf, ptr, cnt);
		else
			pr_info("[%s] total = %d, exceeds buf size: %d\n",
				__func__, total, UART_DUMP_BUF_LEN);
	}
#endif

	if ((copied == cnt) && (total > cnt)) {
		ptr = (unsigned char *)(dma->rx_buf);
#ifdef CONFIG_UART_DATA_RECORD
	if (data->support_hub == 1) {
		if (total <= UART_DUMP_BUF_LEN)
			memcpy(rx_record.rec[idx].rec_buf + cnt, ptr, total - cnt);
		else
			pr_info("[%s] total = %d, cnt = %d, exceeds buf size:%d\n",
				__func__, total, cnt, UART_DUMP_BUF_LEN);
	}
#endif
		cnt = total - cnt;
		copied_sec = tty_insert_flip_string(tty_port, ptr, cnt);
		data->rx_pos = copied_sec;
		copied += copied_sec;
	}

	if (data->support_hub == 1) {
		rx_record.rec[idx].r_copied = copied;
		rx_record.rec[idx].tty_port_addr = (unsigned long long)tty_port;
	}

	up->port.icount.rx += copied;
	mtk8250_uart_rx_setting(dma->rxchan, copied, total);

	tty_flip_buffer_push(tty_port);

	mtk8250_rx_dma(up);

	spin_unlock_irqrestore(&up->port.lock, flags);
}

static void mtk8250_rx_dma(struct uart_8250_port *up)
{
	struct uart_8250_dma *dma = up->dma;
	struct dma_async_tx_descriptor	*desc;

	desc = dmaengine_prep_slave_single(dma->rxchan, dma->rx_addr,
					   dma->rx_size, DMA_DEV_TO_MEM,
					   DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
	if (!desc) {
		pr_err("failed to prepare rx slave single\n");
		return;
	}

	desc->callback = mtk8250_dma_rx_complete;
	desc->callback_param = up;

	dma->rx_cookie = dmaengine_submit(desc);

	dma_async_issue_pending(dma->rxchan);
}

static void mtk8250_dma_enable(struct uart_8250_port *up)
{
	struct uart_8250_dma *dma = up->dma;
	struct mtk8250_data *data = up->port.private_data;
	int lcr = serial_in(up, UART_LCR);

	if (data->rx_status != DMA_RX_START)
		return;

	dma->rx_dma = mtk8250_uart_rx_dma;
	dma->rxconf.src_port_window_size	= dma->rx_size;
	dma->rxconf.src_addr				= dma->rx_addr;

	dma->txconf.dst_port_window_size	= UART_XMIT_SIZE;
	dma->txconf.dst_addr				= dma->tx_addr;

	serial_out(up, UART_FCR, UART_FCR_ENABLE_FIFO | UART_FCR_CLEAR_RCVR |
		UART_FCR_CLEAR_XMIT);
	serial_out(up, MTK_UART_DMA_EN,
		   MTK_UART_DMA_EN_RX | MTK_UART_DMA_EN_TX);

	serial_out(up, UART_LCR, UART_LCR_CONF_MODE_B);
	serial_out(up, MTK_UART_EFR, UART_EFR_ECB);
	serial_out(up, UART_LCR, lcr);
	serial_out(up, UART_IER, 0);

	if (dmaengine_slave_config(dma->rxchan, &dma->rxconf) != 0)
		pr_err("failed to configure rx dma channel\n");
	if (dmaengine_slave_config(dma->txchan, &dma->txconf) != 0)
		pr_err("failed to configure tx dma channel\n");

	data->rx_status = DMA_RX_RUNNING;
	data->rx_pos = 0;
	mtk8250_rx_dma(up);
}
#endif

static int mtk8250_startup(struct uart_port *port)
{
#ifdef CONFIG_SERIAL_8250_DMA
	struct uart_8250_port *up = up_to_u8250p(port);
	struct mtk8250_data *data = port->private_data;

	/* disable DMA for console */
	if (uart_console(port))
		up->dma = NULL;

	if (up->dma) {
		data->rx_status = DMA_RX_START;
		uart_circ_clear(&port->state->xmit);
	}
#endif
	memset(&port->icount, 0, sizeof(port->icount));

#if defined(KERNEL_UARTHUB_open)
	if (data->support_hub == 1) {
		mtk8250_reset_peri();
		pr_info("open uarthub if it is supported.\n");
		/*open UARTHUB*/
		KERNEL_UARTHUB_open();
	}
#endif
	return serial8250_do_startup(port);
}

static void mtk8250_shutdown(struct uart_port *port)
{
#ifdef CONFIG_SERIAL_8250_DMA
	struct uart_8250_port *up = up_to_u8250p(port);
	struct mtk8250_data *data = port->private_data;

	if (up->dma)
		data->rx_status = DMA_RX_SHUTDOWN;
#endif
	mutex_lock(&data->uart_mutex);
#if defined(KERNEL_UARTHUB_close)
	if (data->support_hub == 1) {
		KERNEL_UARTHUB_close();
		mtk8250_clear_wakeup();
	}
#endif

	serial8250_do_shutdown(port);
	mutex_unlock(&data->uart_mutex);
	return;
}

static void mtk8250_disable_intrs(struct uart_8250_port *up, int mask)
{
	serial_out(up, UART_IER, serial_in(up, UART_IER) & (~mask));
}

static void mtk8250_enable_intrs(struct uart_8250_port *up, int mask)
{
	serial_out(up, UART_IER, serial_in(up, UART_IER) | mask);
}

static void mtk8250_set_flow_ctrl(struct uart_8250_port *up, int mode)
{
	struct uart_port *port = &up->port;

	serial_out(up, MTK_UART_FEATURE_SEL, 1);
	serial_out(up, MTK_UART_EFR, UART_EFR_ECB);
	serial_out(up, MTK_UART_FEATURE_SEL, 0);

	switch (mode) {
	case MTK_UART_FC_NONE:
		serial_out(up, MTK_UART_ESCAPE_DAT, MTK_UART_ESCAPE_CHAR);
		serial_out(up, MTK_UART_ESCAPE_EN, 0x00);
		serial_out(up, MTK_UART_FEATURE_SEL, 1);
		serial_out(up, MTK_UART_EFR, serial_in(up, MTK_UART_EFR) &
			(~(MTK_UART_EFR_HW_FC | MTK_UART_EFR_SW_FC_MASK)));
		serial_out(up, MTK_UART_FEATURE_SEL, 0);
		mtk8250_disable_intrs(up, MTK_UART_IER_XOFFI |
			MTK_UART_IER_RTSI | MTK_UART_IER_CTSI);
		break;

	case MTK_UART_FC_HW:
		serial_out(up, MTK_UART_ESCAPE_DAT, MTK_UART_ESCAPE_CHAR);
		serial_out(up, MTK_UART_ESCAPE_EN, 0x00);
		serial_out(up, UART_MCR, UART_MCR_RTS);
		serial_out(up, MTK_UART_FEATURE_SEL, 1);

		/*enable hw flow control*/
		serial_out(up, MTK_UART_EFR, MTK_UART_EFR_HW_FC |
			(serial_in(up, MTK_UART_EFR) &
			(~(MTK_UART_EFR_HW_FC | MTK_UART_EFR_SW_FC_MASK))));

		serial_out(up, MTK_UART_FEATURE_SEL, 0);
		mtk8250_disable_intrs(up, MTK_UART_IER_XOFFI);
		mtk8250_enable_intrs(up, MTK_UART_IER_CTSI | MTK_UART_IER_RTSI);
		break;

	case MTK_UART_FC_SW:	/*MTK software flow control */
		serial_out(up, MTK_UART_ESCAPE_DAT, MTK_UART_ESCAPE_CHAR);
		serial_out(up, MTK_UART_ESCAPE_EN, 0x01);
		serial_out(up, MTK_UART_FEATURE_SEL, 1);

		/*enable sw flow control */
		serial_out(up, MTK_UART_EFR, MTK_UART_EFR_XON1_XOFF1 |
			(serial_in(up, MTK_UART_EFR) &
			(~(MTK_UART_EFR_HW_FC | MTK_UART_EFR_SW_FC_MASK))));
		if ((port != NULL) && (port->state != NULL) &&
			(port->state->port.tty != NULL)) {
			serial_out(up, MTK_UART_XON1,  START_CHAR(port->state->port.tty));
			serial_out(up, MTK_UART_XOFF1, STOP_CHAR(port->state->port.tty));
			serial_out(up, MTK_UART_XON2,  START_CHAR(port->state->port.tty));
			serial_out(up, MTK_UART_XOFF2, STOP_CHAR(port->state->port.tty));
		}
		serial_out(up, MTK_UART_FEATURE_SEL, 0);
		mtk8250_disable_intrs(up, MTK_UART_IER_CTSI|MTK_UART_IER_RTSI);
		break;
	default:
		break;
	}
}

static void
mtk8250_set_divisor(struct uart_port *port, unsigned int baud,
			unsigned int quot, unsigned int quot_frac)
{
	serial_port_out(port, MTK_UART_FEATURE_SEL, 1);
	serial_port_out(port, MTK_UART_DLL, quot & 0xff);
	serial_port_out(port, MTK_UART_DLH, (quot >> 8) & 0xff);
	serial_port_out(port, MTK_UART_FEATURE_SEL, 0);
}

static void
mtk8250_set_termios(struct uart_port *port, struct ktermios *termios,
			struct ktermios *old)
{
	unsigned short fraction_L_mapping[] = {
		0, 1, 0x5, 0x15, 0x55, 0x57, 0x57, 0x77, 0x7F, 0xFF, 0xFF
	};
	unsigned short fraction_M_mapping[] = {
		0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 3
	};
	struct uart_8250_port *up = up_to_u8250p(port);
	unsigned int baud, quot;
	unsigned int fraction = 0;
	unsigned long flags;
	int mode;
	struct mtk8250_data *data = port->private_data;

	if (data->clk_count == 0) {
		pr_info("debug: %s: clk_count = %d, please open ttys[%d]\n", __func__,
			data->clk_count, data->line);
		return;
	}

#ifdef CONFIG_SERIAL_8250_DMA
	if (up->dma) {
		if (uart_console(port)) {
			devm_kfree(up->port.dev, up->dma);
			up->dma = NULL;
		} else {
			mtk8250_dma_enable(up);
		}
	}
#endif

	/*
	 * Store the requested baud rate before calling the generic 8250
	 * set_termios method. Standard 8250 port expects bauds to be
	 * no higher than (uartclk / 16) so the baud will be clamped if it
	 * gets out of that bound. Mediatek 8250 port supports speed
	 * higher than that, therefore we'll get original baud rate back
	 * after calling the generic set_termios method and recalculate
	 * the speed later in this method.
	 */
	baud = tty_termios_baud_rate(termios);

	if (data->support_hub) {
		#if defined(KERNEL_UARTHUB_is_bypass_mode)
		pr_info("support_hub, check if bypass mode\n");
		/*To check bypass mode or multi-host mode*/
		if (!KERNEL_UARTHUB_is_bypass_mode()) {
			baud = data->hub_baud;
			pr_info("multi-host mode, update baud rate as 12M, baud:%d\n", baud);
		}
		#endif
	}

	serial8250_do_set_termios(port, termios, NULL);
#ifdef CONFIG_SERIAL_8250_DMA
	if (up->dma) {
		if (!uart_console(port)) {
			unsigned int tmo = MAX_POLLING_CNT;
			unsigned int iir = 0;

			while (tmo--) {
				iir = serial_port_in(port, UART_IIR);
				if (iir & UART_IIR_NO_INT)
					break;
			}
			up->ier = 0;
			serial_port_out(port, UART_IER, 0x0);
		}
	}
#endif

	tty_termios_encode_baud_rate(termios, baud, baud);

	/*
	 * Mediatek UARTs use an extra highspeed register (MTK_UART_HIGHS)
	 *
	 * We need to recalcualte the quot register, as the claculation depends
	 * on the vaule in the highspeed register.
	 *
	 * Some baudrates are not supported by the chip, so we use the next
	 * lower rate supported and update termios c_flag.
	 *
	 * If highspeed register is set to 3, we need to specify sample count
	 * and sample point to increase accuracy. If not, we reset the
	 * registers to their default values.
	 */
	baud = uart_get_baud_rate(port, termios, old,
				  port->uartclk / 16 / UART_DIV_MAX,
				  port->uartclk);

	if (data->support_hub) {
		#if defined(KERNEL_UARTHUB_is_bypass_mode)
		pr_info("support_hub, check if bypass mode\n");
		/*To check bypass mode or multi-host mode*/
		if (!KERNEL_UARTHUB_is_bypass_mode()) {
			baud = data->hub_baud;
		#if defined(KERNEL_UARTHUB_config_internal_baud_rate)
			/*configure UARTHUB baud rate*/
			KERNEL_UARTHUB_config_internal_baud_rate(0, baud);
		#endif
		#if defined(KERNEL_UARTHUB_config_external_baud_rate)
			KERNEL_UARTHUB_config_external_baud_rate(baud);
		#endif
			pr_info("multi-host mode, update baudrate as 12M and update hub baudrate, baud:%d\n"
							, baud);
		}
		#endif
	}

	if (baud < 115200) {
		serial_port_out(port, MTK_UART_HIGHS, 0x0);
		quot = uart_get_divisor(port, baud);
	} else {
		serial_port_out(port, MTK_UART_HIGHS, 0x3);
		quot = DIV_ROUND_UP(port->uartclk, 256 * baud);
	}

	/*
	 * Ok, we're now changing the port state.  Do it with
	 * interrupts disabled.
	 */
	spin_lock_irqsave(&port->lock, flags);

	/*
	 * Update the per-port timeout.
	 */
	uart_update_timeout(port, termios->c_cflag, baud);

	/* set DLAB we have cval saved in up->lcr from the call to the core */

	/* reset DLAB */

	if (baud == MTK_UART_HUB_BAUD) {
		serial_port_out(port, MTK_UART_SAMPLE_COUNT, 7);
		serial_port_out(port, MTK_UART_SAMPLE_POINT, 3);
		serial_port_out(port, MTK_UART_FRACDIV_L, 0xdb);
		serial_port_out(port, MTK_UART_FRACDIV_M, 0x1);

	} else if (baud >= 115200) {
		unsigned int tmp;

		tmp = (port->uartclk / (baud *  quot)) - 1;
		serial_port_out(port, MTK_UART_SAMPLE_COUNT, tmp);
		serial_port_out(port, MTK_UART_SAMPLE_POINT,
					(tmp >> 1) - 1);

		/*count fraction to set fractoin register */
		fraction = ((port->uartclk  * 100) / baud / quot) % 100;
		fraction = DIV_ROUND_CLOSEST(fraction, 10);
		serial_port_out(port, MTK_UART_FRACDIV_L,
						fraction_L_mapping[fraction]);
		serial_port_out(port, MTK_UART_FRACDIV_M,
						fraction_M_mapping[fraction]);
	} else {
		serial_port_out(port, MTK_UART_SAMPLE_COUNT, 0x00);
		serial_port_out(port, MTK_UART_SAMPLE_POINT, 0xff);
		serial_port_out(port, MTK_UART_FRACDIV_L, 0x00);
		serial_port_out(port, MTK_UART_FRACDIV_M, 0x00);
	}

	mtk8250_set_divisor(port, baud, quot, fraction);

	if ((termios->c_cflag & CRTSCTS) && (!(termios->c_iflag & CRTSCTS)))
		mode = MTK_UART_FC_HW;
	else if (termios->c_iflag & CRTSCTS)
		mode = MTK_UART_FC_SW;
	else
		mode = MTK_UART_FC_NONE;

	mtk8250_set_flow_ctrl(up, mode);
#ifdef CONFIG_SERIAL_8250_DMA
	if (up->dma && (!uart_console(port)))
		serial_port_out(port, UART_IER, 0x0);
#endif

	if (uart_console(port))
		up->port.cons->cflag = termios->c_cflag;

	spin_unlock_irqrestore(&port->lock, flags);
	/* Don't rewrite B0 */
	if (tty_termios_baud_rate(termios))
		tty_termios_encode_baud_rate(termios, baud, baud);
}

static int __maybe_unused mtk8250_runtime_suspend(struct device *dev)
{
	struct mtk8250_data *data = dev_get_drvdata(dev);
	struct uart_8250_port *up = serial8250_get_port(data->line);

	if (data->support_hub) {
		pr_info("[%s]:clock count is[%d] support_hub, skip disable clock\n",
		 __func__, data->clk_count);
	} else {
		if (data->clk_count == 0U) {
			dev_dbg(dev, "%s clock count is 0\n", __func__);
		} else {
			/* wait until UART in idle status */
			while
				(serial_in(up, MTK_UART_DEBUG0));

			clk_disable_unprepare(data->bus_clk);
			data->clk_count--;
		}
	}
	return 0;
}

static int __maybe_unused mtk8250_runtime_resume(struct device *dev)
{
	struct mtk8250_data *data = dev_get_drvdata(dev);
	int err;

	if (data->clk_count > 0U) {
		pr_info("[%s]: data->line[%d] clock count is %d\n", __func__,
			data->line, data->clk_count);
	} else {
		err = clk_prepare_enable(data->bus_clk);
		if (err) {
			dev_warn(dev, "Can't enable bus clock\n");
			return err;
		}
		data->clk_count++;
	}

	return 0;
}

static void
mtk8250_do_pm(struct uart_port *port, unsigned int state, unsigned int old)
{
	unsigned char efr = 0;
	struct uart_8250_port *up = up_to_u8250p(port);

	if (!state)
		if (!mtk8250_runtime_resume(port->dev))
			pm_runtime_get_sync(port->dev);

	serial8250_rpm_get(up);

	if (up->capabilities & UART_CAP_SLEEP) {
		if (up->capabilities & UART_CAP_EFR) {
			serial_out(up, MTK_UART_FEATURE_SEL, 1);
			efr = serial_in(up, MTK_UART_EFR);
			serial_out(up, MTK_UART_EFR, UART_EFR_ECB);
			serial_out(up, MTK_UART_FEATURE_SEL, 0);
		}
		serial_out(up, UART_IER, (state != 0) ? UART_IERX_SLEEP : 0);
		if (up->capabilities & UART_CAP_EFR) {
			serial_out(up, MTK_UART_FEATURE_SEL, 1);
			serial_out(up, MTK_UART_EFR, efr);
			serial_out(up, MTK_UART_FEATURE_SEL, 0);
		}
	}

	serial8250_rpm_put(up);

	if (state)
		if (!pm_runtime_put_sync_suspend(port->dev))
			mtk8250_runtime_suspend(port->dev);
}

#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_SERIAL_8250_DMA
static bool mtk8250_dma_filter(struct dma_chan *chan, void *param)
{
	return false;
}
#endif

static int mtk8250_probe_of(struct platform_device *pdev, struct uart_port *p,
			   struct mtk8250_data *data)
{
#ifdef CONFIG_SERIAL_8250_DMA
	int dmacnt;
#endif

	void __iomem *peri_remap_addr = NULL;
	unsigned int peri_addr = 0, peri_mask = 0, peri_val = 0;
	int index = -1;
	int uart_line = -1;
	int err;

	data->uart_clk = devm_clk_get(&pdev->dev, "baud");
	if (IS_ERR(data->uart_clk)) {
		/*
		 * For compatibility with older device trees try unnamed
		 * clk when no baud clk can be found.
		 */
		data->uart_clk = devm_clk_get(&pdev->dev, NULL);
		if (IS_ERR(data->uart_clk)) {
			dev_warn(&pdev->dev, "Can't get uart clock\n");
			return PTR_ERR(data->uart_clk);
		}

		return 0;
	}

		err = of_property_read_u32(pdev->dev.of_node, "uart_line", &uart_line);
		if (err < 0) {
			dev_info(&pdev->dev, "uart_line fail!!!\n");
		} else {
			data->line = uart_line;
			pr_info("probe_uart: data->line: %d:\n", data->line);
		}

	data->bus_clk = devm_clk_get(&pdev->dev, "bus");
	if (IS_ERR(data->bus_clk))
		return PTR_ERR(data->bus_clk);

	data->dma = NULL;
#ifdef CONFIG_SERIAL_8250_DMA
	dmacnt = of_property_count_strings(pdev->dev.of_node, "dma-names");
	if (dmacnt == 2) {
		data->dma = devm_kzalloc(&pdev->dev, sizeof(*data->dma),
					 GFP_KERNEL);
		if (!data->dma)
			return -ENOMEM;

		data->dma->fn = mtk8250_dma_filter;
		data->dma->rx_size = MTK_UART_RX_SIZE;
		data->dma->rxconf.src_maxburst = MTK_UART_RX_TRIGGER;
		data->dma->txconf.dst_maxburst = MTK_UART_TX_TRIGGER;
	}
#endif

	if (data->support_hub) {
		/*switch clock*/
		dev_info(&pdev->dev, "switch clock as 104M\n");
		index = of_property_read_u32_index(pdev->dev.of_node,
			"peri-clock-con", 0, &peri_addr);
		if (index) {
			pr_notice("[%s] get peri_addr fail\n", __func__);
			return -1;
		}

		index = of_property_read_u32_index(pdev->dev.of_node,
			"peri-clock-con", 1, &peri_mask);
		if (index) {
			pr_notice("[%s] get peri-clock-con_mask fail\n", __func__);
			return -1;
		}

		index = of_property_read_u32_index(pdev->dev.of_node,
			"peri-clock-con", 2, &peri_val);
		if (index) {
			pr_notice("[%s] get peri-clock-con_value fail\n", __func__);
			return -1;
		}

		peri_remap_addr = ioremap(peri_addr, 0x10);
		if (!peri_remap_addr) {
			pr_notice("[%s] peri_remap_addr(%x) ioremap fail\n", __func__, peri_addr);
			return -1;
		}
		UART_REG_WRITE(peri_remap_addr,
			((UART_REG_READ(peri_remap_addr) & (~peri_mask)) | peri_val));
		dev_info(&pdev->dev, "PERSYS set as 0x%x\n", (UART_REG_READ(peri_remap_addr)));

		/*parse baud*/
		index = of_property_read_u32_index(pdev->dev.of_node,
					"hub-baud", 0, &data->hub_baud);
		if (index) {
			pr_notice("[%s] get hub-baud fail\n", __func__);
			return -1;
		}

		/*parse wakeup*/
		index = of_property_read_u32_index(pdev->dev.of_node,
			"peri-wakeup", 0, &peri_wakeup.addr);
		if (index) {
			pr_notice("[%s] get peri-wakeup addr fail\n", __func__);
			return -1;
		}

		index = of_property_read_u32_index(pdev->dev.of_node,
			"peri-wakeup", 1, &peri_wakeup.mask);
		if (index) {
			pr_notice("[%s] get peri-wakeup_mask fail\n", __func__);
			return -1;
		}

		index = of_property_read_u32_index(pdev->dev.of_node,
			"peri-wakeup", 2, &peri_wakeup.val);
		if (index) {
			pr_notice("[%s] get peri-wakeup_value fail\n", __func__);
			return -1;
		}

		index = of_property_read_u32_index(pdev->dev.of_node,
			"peri-wakeup", 3, &peri_wakeup.toggle);
		if (index) {
			pr_notice("[%s] get peri-wakeup.toggle fail\n", __func__);
			return -1;
		}

		index = of_property_read_u32_index(pdev->dev.of_node,
			"peri-wakeup-sta", 0, &peri_wakeup.addr_sta);
		if (index) {
			pr_notice("[%s] get peri-wakeup-sta.addr-sta fail\n", __func__);
			return -1;
		}

		/*parse reset*/
		index = of_property_read_u32_index(pdev->dev.of_node,
			"peri-reset-set", 0, &peri_reset.addr_set);
		if (index) {
			pr_notice("[%s] get peri-reset-set.addr_set fail\n", __func__);
			return -1;
		}

		index = of_property_read_u32_index(pdev->dev.of_node,
			"peri-reset-set", 1, &peri_reset.mask_set);
		if (index) {
			pr_notice("[%s] get peri-reset-set.mask_set fail\n", __func__);
			return -1;
		}

		index = of_property_read_u32_index(pdev->dev.of_node,
			"peri-reset-set", 2, &peri_reset.val_set);
		if (index) {
			pr_notice("[%s] get peri-reset-set.val_set fail\n", __func__);
			return -1;
		}

		index = of_property_read_u32_index(pdev->dev.of_node,
			"peri-reset-clr", 0, &peri_reset.addr_clr);
		if (index) {
			pr_notice("[%s] get peri-reset-clr.addr_clr fail\n", __func__);
			return -1;
		}

		index = of_property_read_u32_index(pdev->dev.of_node,
			"peri-reset-clr", 1, &peri_reset.mask_clr);
		if (index) {
			pr_notice("[%s] get peri-reset-clr.mask_clr fail\n", __func__);
			return -1;
		}

		index = of_property_read_u32_index(pdev->dev.of_node,
			"peri-reset-clr", 2, &peri_reset.val_clr);
		if (index) {
			pr_notice("[%s] get peri-reset-clr.val_clr fail\n", __func__);
			return -1;
		}

		index = of_property_read_u32_index(pdev->dev.of_node,
			"peri-reset", 0, &peri_reset.addr);
		if (index) {
			pr_notice("[%s] get peri-reset.addr fail\n", __func__);
			return -1;
		}
	}
	return 0;
}
#endif

static int mtk8250_probe(struct platform_device *pdev)
{
	struct uart_8250_port uart = {};
	struct mtk8250_data *data;
	struct resource *regs;
	int irq, err;
	const struct mtk8250_comp *comp;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!regs) {
		dev_err(&pdev->dev, "no registers defined\n");
		return -EINVAL;
	}

	uart.port.membase = devm_ioremap(&pdev->dev, regs->start,
					 resource_size(regs));
	if (!uart.port.membase)
		return -ENOMEM;

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	comp = of_device_get_match_data(&pdev->dev);
	if (comp == NULL) {
		dev_info(&pdev->dev, "No compatiable data\n");
		data->support_hub = 0;
	} else
		data->support_hub = comp->support_hub;

	dev_info(&pdev->dev,
			"uart support uarthub: %d\n", data->support_hub);

	data->clk_count = 0;
	mutex_init(&data->uart_mutex);

#ifndef CONFIG_FPGA_EARLY_PORTING
	if (pdev->dev.of_node) {
		err = mtk8250_probe_of(pdev, &uart.port, data);
		if (err)
			return err;
	} else
		return -ENODEV;
#endif

	spin_lock_init(&uart.port.lock);
	uart.port.mapbase = regs->start;
	uart.port.irq = irq;
	uart.port.pm = mtk8250_do_pm;
	uart.port.type = PORT_16550;
	uart.port.flags = UPF_BOOT_AUTOCONF | UPF_FIXED_PORT;
	uart.port.dev = &pdev->dev;
	uart.port.iotype = UPIO_MEM32;
	uart.port.regshift = 2;
	uart.port.private_data = data;
	uart.port.shutdown = mtk8250_shutdown;
	uart.port.startup = mtk8250_startup;
	uart.port.set_termios = mtk8250_set_termios;
	uart.port.set_divisor = mtk8250_set_divisor;
	uart.port.uartclk = clk_get_rate(data->uart_clk);
#ifdef CONFIG_FPGA_EARLY_PORTING
	uart.port.uartclk = MTK_UART_FPGA_CLK;
#endif
#ifdef CONFIG_SERIAL_8250_DMA
	if (data->dma) {
		uart.dma = data->dma;
		uart.dma->rx_dma = mtk8250_uart_rx_dma;
	}
#endif

	platform_set_drvdata(pdev, data);
	if (data->support_hub) {
		if (clk_prepare_enable(data->bus_clk)) {
			pr_info("[%s]: line[%d], data->clk_count[%d], CLk enable fail!!\n",
				__func__, data->line, data->clk_count);
		}
	}

	pm_runtime_enable(&pdev->dev);
	err = mtk8250_runtime_resume(&pdev->dev);
	if (err)
		goto err_pm_disable;

	/* Disable Rate Fix function */
	writel(0x0, uart.port.membase +
			(MTK_UART_RATE_FIX << uart.port.regshift));

	err = serial8250_register_8250_port(&uart);
	if (err < 0) {
		pr_info("probe: err: %d: serial8250 register 8250 port fail!!!", err);
		goto err_pm_disable;
	}

	data->rx_wakeup_irq = platform_get_irq_optional(pdev, 1);

	return 0;

err_pm_disable:
	pm_runtime_disable(&pdev->dev);

	return err;
}

static int mtk8250_remove(struct platform_device *pdev)
{
	struct mtk8250_data *data = platform_get_drvdata(pdev);

	pm_runtime_get_sync(&pdev->dev);

	serial8250_unregister_port(data->line);

	pm_runtime_disable(&pdev->dev);
	pm_runtime_put_noidle(&pdev->dev);

	if (!pm_runtime_status_suspended(&pdev->dev))
		mtk8250_runtime_suspend(&pdev->dev);

	return 0;
}

static int __maybe_unused mtk8250_suspend(struct device *dev)
{
	struct mtk8250_data *data = dev_get_drvdata(dev);
	int irq = data->rx_wakeup_irq;
	int err;

	serial8250_suspend_port(data->line);

	pinctrl_pm_select_sleep_state(dev);
	if (irq >= 0) {
		err = enable_irq_wake(irq);
		if (err) {
			dev_err(dev,
				"failed to enable irq wake on IRQ %d: %d\n",
				irq, err);
			pinctrl_pm_select_default_state(dev);
			serial8250_resume_port(data->line);
			return err;
		}
	}

	return 0;
}

static int __maybe_unused mtk8250_resume(struct device *dev)
{
	struct mtk8250_data *data = dev_get_drvdata(dev);
	int irq = data->rx_wakeup_irq;

	if (irq >= 0)
		disable_irq_wake(irq);
	pinctrl_pm_select_default_state(dev);

	serial8250_resume_port(data->line);

	return 0;
}

static const struct dev_pm_ops mtk8250_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mtk8250_suspend, mtk8250_resume)
	SET_RUNTIME_PM_OPS(mtk8250_runtime_suspend, mtk8250_runtime_resume,
				NULL)
};

static const struct mtk8250_comp mt6985_comp = {
	.support_hub = 1
};

static const struct of_device_id mtk8250_of_match[] = {
	{ .compatible = "mediatek,mt6577-uart", .data = NULL },
	{ .compatible = "mediatek,mt6985-uart", .data = &mt6985_comp },
	{ /* Sentinel */ }
};
MODULE_DEVICE_TABLE(of, mtk8250_of_match);

static struct platform_driver mtk8250_platform_driver = {
	.driver = {
		.name		= "mt6577-uart",
		.pm		= &mtk8250_pm_ops,
		.of_match_table	= mtk8250_of_match,
	},
	.probe			= mtk8250_probe,
	.remove			= mtk8250_remove,
};
module_platform_driver(mtk8250_platform_driver);

#ifdef CONFIG_SERIAL_8250_CONSOLE
static int __init early_mtk8250_setup(struct earlycon_device *device,
					const char *options)
{
	if (!device->port.membase)
		return -ENODEV;

	device->port.iotype = UPIO_MEM32;
	device->port.regshift = 2;

#ifdef CONFIG_FPGA_EARLY_PORTING
	device->port.uartclk = MTK_UART_FPGA_CLK;
	device->baud = MTK_UART_FPGA_BAUD;
#endif

	return early_serial8250_setup(device, NULL);
}

#ifdef CONFIG_FPGA_EARLY_PORTING
EARLYCON_DECLARE(mtk8250, early_mtk8250_setup);
#endif

OF_EARLYCON_DECLARE(mtk8250, "mediatek,mt6577-uart", early_mtk8250_setup);
#endif

MODULE_AUTHOR("Matthias Brugger");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Mediatek 8250 serial port driver");
