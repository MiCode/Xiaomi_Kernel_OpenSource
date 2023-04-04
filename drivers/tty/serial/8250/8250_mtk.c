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
#include <linux/delay.h>
#include "8250.h"

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
#define MTK_UART_DEBUG0	0x18
#define MTK_UART_IER_XOFFI	0x20	/* Enable XOFF character interrupt */
#define MTK_UART_IER_RTSI	0x40	/* Enable RTS Modem status interrupt */
#define MTK_UART_IER_CTSI	0x80	/* Enable CTS Modem status interrupt */
#define MTK_UART_SLEEP_REQ	0x2d	/* Sleep request register */
#define MTK_UART_SEND_SLEEP_REQ	0x1	/* Request uart to sleep */
#define MTK_UART_SLEEP_ACK	0x2e	/* Sleep ack register */
#define MTK_UART_SLEEP_ACK_IDLE	0x1	/* uart in idle state */
#define MTK_UART_RX_SEL		0x24	/* Uart rx pin sel */
#define MTK_UART_WAIT_ACK_TIMES	50
#define MTK_UART_FCR_RD		0x17	/* Fifo control register */
#define MTK_UART_DLL  0x24
#define MTK_UART_DLH  0x25
#define MTK_UART_FEATURE_SEL  0x27
#define MTK_UART_EFR    0x26
#define MTK_UART_GUARD		0x0f	/* Guard time added register */
#define MTK_UART_SLEEP_EN	0x12	/* Sleep Enable register */
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

#define MTK_UART_ESCAPE_CHAR	0x77	/* Escape char added under sw fc */
#define MTK_UART_RX_SIZE	0x8000
#define MTK_UART_TX_TRIGGER	1
#define MTK_UART_RX_TRIGGER	MTK_UART_RX_SIZE

#ifdef CONFIG_SERIAL_8250_DMA
enum dma_rx_status {
	DMA_RX_START = 0,
	DMA_RX_RUNNING = 1,
	DMA_RX_SHUTDOWN = 2,
};
#endif

struct mtk8250_reg {
	unsigned int ier;
	unsigned int mcr;
	unsigned int msr;
	unsigned int scr;
	unsigned int dll;
	unsigned int dlm;
	unsigned int lcr;
	unsigned int efr;
	unsigned int xon1;
	unsigned int xon2;
	unsigned int xoff1;
	unsigned int xoff2;
	unsigned int highspeed;
	unsigned int sample_count;
	unsigned int sample_point;
	unsigned int guard;
	unsigned int escape_dat;
	unsigned int escape_en;
	unsigned int sleep_en;
	unsigned int dma_en;
	unsigned int rxtri_ad;
	unsigned int fracdiv_l;
	unsigned int fracdiv_m;
	unsigned int fcr_rd;
	unsigned int rx_sel;
};
struct mtk8250_data {
	int			line;
	unsigned int		rx_pos;
	unsigned int		clk_count;
	struct clk		*uart_clk;
	struct clk		*bus_clk;
	struct uart_8250_dma	*dma;
	struct mtk8250_reg	reg;
#ifdef CONFIG_SERIAL_8250_DMA
	enum dma_rx_status	rx_status;
#endif
	int			rx_wakeup_irq;
};

/* flow control mode */
enum {
	MTK_UART_FC_NONE,
	MTK_UART_FC_SW,
	MTK_UART_FC_HW,
};

#ifdef CONFIG_SERIAL_8250_DMA
static void mtk8250_rx_dma(struct uart_8250_port *up);

static void mtk8250_dma_rx_complete(void *param)
{
	struct uart_8250_port *up = param;
	struct uart_8250_dma *dma = up->dma;
	struct mtk8250_data *data = up->port.private_data;
	struct tty_port *tty_port = &up->port.state->port;
	struct dma_tx_state state;
	int copied, total, cnt;
	unsigned char *ptr;
	unsigned long flags;

	if (data->rx_status == DMA_RX_SHUTDOWN)
		return;

	spin_lock_irqsave(&up->port.lock, flags);

	dmaengine_tx_status(dma->rxchan, dma->rx_cookie, &state);
	total = dma->rx_size - state.residue;
	cnt = total;

	if ((data->rx_pos + cnt) > dma->rx_size)
		cnt = dma->rx_size - data->rx_pos;

	ptr = (unsigned char *)(data->rx_pos + dma->rx_buf);
	copied = tty_insert_flip_string(tty_port, ptr, cnt);
	data->rx_pos += cnt;

	if (total > cnt) {
		ptr = (unsigned char *)(dma->rx_buf);
		cnt = total - cnt;
		copied += tty_insert_flip_string(tty_port, ptr, cnt);
		data->rx_pos = cnt;
	}

	up->port.icount.rx += copied;

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

	dma->rxconf.src_port_window_size	= dma->rx_size;
	dma->rxconf.src_addr				= dma->rx_addr;

	dma->txconf.dst_port_window_size	= UART_XMIT_SIZE;
	dma->txconf.dst_addr				= dma->tx_addr;

	serial_out(up, UART_FCR, UART_FCR_ENABLE_FIFO | UART_FCR_CLEAR_RCVR |
		UART_FCR_CLEAR_XMIT);
	serial_out(up, MTK_UART_DMA_EN,
		   MTK_UART_DMA_EN_RX | MTK_UART_DMA_EN_TX);

	serial_out(up, UART_LCR, UART_LCR_CONF_MODE_B);
	serial_out(up, UART_EFR, UART_EFR_ECB);
	serial_out(up, UART_LCR, lcr);

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

	return serial8250_do_shutdown(port);
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

		serial_out(up, UART_XON1, START_CHAR(port->state->port.tty));
		serial_out(up, UART_XOFF1, STOP_CHAR(port->state->port.tty));
		serial_out(up, MTK_UART_FEATURE_SEL, 0);
		mtk8250_disable_intrs(up, MTK_UART_IER_CTSI|MTK_UART_IER_RTSI);
		mtk8250_enable_intrs(up, MTK_UART_IER_XOFFI);
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

	serial8250_do_set_termios(port, termios, NULL);

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

	if (baud >= 115200) {
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

	if (data->clk_count == 0U) {
		dev_dbg(dev, "%s clock count is 0\n", __func__);
	} else {
			/* wait until UART in idle status */
		while
			(serial_in(up, MTK_UART_DEBUG0));
		clk_disable_unprepare(data->bus_clk);
		data->clk_count--;
	}

	return 0;
}

static int __maybe_unused mtk8250_runtime_resume(struct device *dev)
{
	struct mtk8250_data *data = dev_get_drvdata(dev);
	int err;

	if (data->clk_count > 0U) {
		dev_dbg(dev, "%s clock count is %d\n", __func__,
			data->clk_count);
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

	return 0;
}

#if IS_ENABLED(CONFIG_MTK_SPM_V4)
static void mtk8250_save_dev(struct device *dev)
{
	unsigned long flags;
	struct mtk8250_data *data = dev_get_drvdata(dev);
	struct mtk8250_reg *reg;
	struct uart_8250_port *up;

	if (data == NULL)
		return;
	reg = &data->reg;
	up = serial8250_get_port(data->line);
	if (up->port.dev == NULL)
		return;

	/* DLL may be changed by console write. To avoid this, use spinlock */
	spin_lock_irqsave(&up->port.lock, flags);
	reg->fcr_rd = serial_in(up, MTK_UART_FCR_RD);

	/*save baudrate */
	reg->highspeed = serial_in(up, MTK_UART_HIGHS);
	reg->fracdiv_l = serial_in(up, MTK_UART_FRACDIV_L);
	reg->fracdiv_m = serial_in(up, MTK_UART_FRACDIV_M);
	serial_out(up, UART_LCR, reg->lcr | UART_LCR_DLAB);
	reg->dll = serial_in(up, UART_DLL);
	reg->dlm = serial_in(up, UART_DLM);
	serial_out(up, UART_LCR, reg->lcr);

	serial_out(up, MTK_UART_FEATURE_SEL, 0x1);
	serial_out(up, MTK_UART_EFR, reg->efr);

	reg->dll = serial_in(up, MTK_UART_DLL);
	reg->dlm = serial_in(up, MTK_UART_DLH);
	serial_out(up, MTK_UART_FEATURE_SEL, 0x0);

	reg->sample_count = serial_in(up, MTK_UART_SAMPLE_COUNT);
	reg->sample_point = serial_in(up, MTK_UART_SAMPLE_POINT);
	reg->guard = serial_in(up, MTK_UART_GUARD);

	/* save flow control */
	reg->mcr = serial_in(up, UART_MCR);
	reg->ier = serial_in(up, UART_IER);
	reg->xon1 = serial_in(up, UART_XON1);
	reg->xon2 = serial_in(up, UART_XON2);
	reg->xoff1 = serial_in(up, UART_XOFF1);
	reg->xoff2 = serial_in(up, UART_XOFF2);
	reg->escape_dat = serial_in(up, MTK_UART_ESCAPE_DAT);
	reg->sleep_en = serial_in(up, MTK_UART_SLEEP_EN);

	/* save others */
	reg->escape_en = serial_in(up, MTK_UART_ESCAPE_EN);
	reg->msr = serial_in(up, UART_MSR);
	reg->scr = serial_in(up, UART_SCR);
	reg->dma_en = serial_in(up, MTK_UART_DMA_EN);
	reg->rxtri_ad = serial_in(up, MTK_UART_RXTRI_AD);
	reg->rx_sel = serial_in(up, MTK_UART_RX_SEL);
	spin_unlock_irqrestore(&up->port.lock, flags);
}
#endif

int mtk8250_request_to_sleep(void)
{
	int i = 0;
	int line = 0;
	int sleep_req;
	struct uart_8250_port *up;
	struct mtk8250_data *data;

	for (line = 0; line < CONFIG_SERIAL_8250_NR_UARTS; line++) {
		up = serial8250_get_port(line);

		if (up->port.dev == NULL)
			continue;
		else if (dev_get_drvdata(up->port.dev) == NULL)
			continue;

		data = dev_get_drvdata(up->port.dev);
		if (data->clk_count <= 0U)
			continue;

		/* request UART to sleep */
		sleep_req = serial_in(up, MTK_UART_SLEEP_REQ);
		serial_out(up, MTK_UART_SLEEP_REQ,
			sleep_req | MTK_UART_SEND_SLEEP_REQ);

		/* wait for UART to ACK */
		while (!(serial_in(up, MTK_UART_SLEEP_ACK)
			& MTK_UART_SLEEP_ACK_IDLE)) {
			if (i++ >= MTK_UART_WAIT_ACK_TIMES) {
				serial_out(up, MTK_UART_SLEEP_REQ, sleep_req);
				pr_info_ratelimited("UART%d SLEEP ACK Fail\n",
					line);
				return -EBUSY;
			}
			udelay(10);
		}
	}

	return 0;
}
EXPORT_SYMBOL(mtk8250_request_to_sleep);

int mtk8250_request_to_wakeup(void)
{
	int i = 0;
	int line = 0;
	int sleep_req;
	struct uart_8250_port *up;
	struct mtk8250_data *data;

	for (line = 0; line < CONFIG_SERIAL_8250_NR_UARTS; line++) {
		up = serial8250_get_port(line);

		if (up->port.dev == NULL)
			continue;
		else if (dev_get_drvdata(up->port.dev) == NULL)
			continue;

		data = dev_get_drvdata(up->port.dev);
		if (data->clk_count <= 0U)
			continue;

		/* wakeup uart */
		sleep_req = serial_in(up, MTK_UART_SLEEP_REQ);
		serial_out(up, MTK_UART_SLEEP_REQ,
			sleep_req & (~MTK_UART_SEND_SLEEP_REQ));

		/* wait for UART to ACK */
		while (serial_in(up, MTK_UART_SLEEP_ACK)
			& MTK_UART_SLEEP_ACK_IDLE) {
			if (i++ >= MTK_UART_WAIT_ACK_TIMES) {
				serial_out(up, MTK_UART_SLEEP_REQ, sleep_req);
				pr_debug("CANNOT GET UART%d WAKE ACK\n", line);
				return -EBUSY;
			}
			udelay(10);
		}
	}

	return 0;
}
EXPORT_SYMBOL(mtk8250_request_to_wakeup);

void mtk8250_restore_dev(void)
{
	unsigned long flags;
	int line = 0;
	struct uart_8250_port *up;
	struct mtk8250_data *data;
	struct mtk8250_reg *reg;

	for (line = 0; line < CONFIG_SERIAL_8250_NR_UARTS; line++) {
		up = serial8250_get_port(line);
		if (up->port.dev == NULL)
			continue;
		data = dev_get_drvdata(up->port.dev);
		if (data == NULL)
			continue;
		reg = &data->reg;

		if (!uart_console(&up->port))
			continue;

		mtk8250_runtime_resume(up->port.dev);
		spin_lock_irqsave(&up->port.lock, flags);

		serial_out(up, UART_FCR, reg->fcr_rd);

		/*restore baudrate */
		serial_out(up, MTK_UART_HIGHS, reg->highspeed);
		serial_out(up, MTK_UART_FRACDIV_L, reg->fracdiv_l);
		serial_out(up, MTK_UART_FRACDIV_M, reg->fracdiv_m);
		serial_out(up, UART_LCR, reg->lcr | UART_LCR_DLAB);
		serial_out(up, UART_DLL, reg->dll);
		serial_out(up, UART_DLM, reg->dlm);
		serial_out(up, UART_LCR, reg->lcr);

		serial_out(up, MTK_UART_FEATURE_SEL, 0x1);

		serial_out(up, MTK_UART_EFR, reg->efr);

		serial_out(up, MTK_UART_DLL, reg->dll);
		serial_out(up, MTK_UART_DLH, reg->dlm);
		serial_out(up, MTK_UART_FEATURE_SEL, 0x0);

		serial_out(up, MTK_UART_SAMPLE_COUNT, reg->sample_count);
		serial_out(up, MTK_UART_SAMPLE_POINT, reg->sample_point);
		serial_out(up, MTK_UART_GUARD, reg->guard);

		/* restore flow control */
		serial_out(up, UART_MCR, reg->mcr);
		serial_out(up, UART_IER, reg->ier);
		serial_out(up, UART_XON1, reg->xon1);
		serial_out(up, UART_XON2, reg->xon2);
		serial_out(up, UART_XOFF1, reg->xoff1);
		serial_out(up, UART_XOFF2, reg->xoff2);
		serial_out(up, MTK_UART_ESCAPE_DAT, reg->escape_dat);
		serial_out(up, MTK_UART_SLEEP_EN, reg->sleep_en);

		/* restore others */
		serial_out(up, MTK_UART_ESCAPE_EN, reg->escape_en);
		serial_out(up, UART_MSR, reg->msr);
		serial_out(up, UART_SCR, reg->scr);

		serial_out(up, MTK_UART_DMA_EN, reg->dma_en);
		serial_out(up, MTK_UART_RXTRI_AD, reg->rxtri_ad);
		serial_out(up, MTK_UART_FCR_RD, reg->fcr_rd);
		serial_out(up, MTK_UART_RX_SEL, reg->rx_sel);
		spin_unlock_irqrestore(&up->port.lock, flags);
	}
}
EXPORT_SYMBOL(mtk8250_restore_dev);

static int mtk8250_probe(struct platform_device *pdev)
{
	struct uart_8250_port uart = {};
	struct mtk8250_data *data;
	struct resource *regs;
	int irq, err, id;

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

	data->clk_count = 0;

	if (pdev->dev.of_node) {
		err = mtk8250_probe_of(pdev, &uart.port, data);
		if (err)
			return err;
	} else
		return -ENODEV;

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
#ifdef CONFIG_SERIAL_8250_DMA
	if (data->dma)
		uart.dma = data->dma;
#endif
	/* Get serial line which configed in DT aliases */
	id = of_alias_get_id(pdev->dev.of_node, "serial");
	if (id >= 0) {
		uart.port.line = id;
		data->line = id;
	}

	/* Disable Rate Fix function */
	writel(0x0, uart.port.membase +
			(MTK_UART_RATE_FIX << uart.port.regshift));

	platform_set_drvdata(pdev, data);

	pm_runtime_enable(&pdev->dev);
	err = mtk8250_runtime_resume(&pdev->dev);
	if (err)
		goto err_pm_disable;

	data->line = serial8250_register_8250_port(&uart);
	if (data->line < 0) {
		err = data->line;
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
#if IS_ENABLED(CONFIG_MTK_SPM_V4)
	struct uart_8250_port *up;
	up = serial8250_get_port(data->line);
	if (up->port.dev != NULL && uart_console(&up->port) == 1)
		mtk8250_save_dev(dev);
#endif
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

static const struct of_device_id mtk8250_of_match[] = {
	{ .compatible = "mediatek,mt6577-uart" },
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

	return early_serial8250_setup(device, NULL);
}

OF_EARLYCON_DECLARE(mtk8250, "mediatek,mt6577-uart", early_mtk8250_setup);
#endif

MODULE_AUTHOR("Matthias Brugger");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Mediatek 8250 serial port driver");
