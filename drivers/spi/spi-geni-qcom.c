/*
 * Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/qcom-geni-se.h>
#include <linux/spi/spi.h>

#define SPI_NUM_CHIPSELECT	(4)
#define SPI_XFER_TIMEOUT_MS	(250)
#define SPI_OVERSAMPLING	(2)
/* SPI SE specific registers */
#define SE_SPI_CPHA		(0x224)
#define SE_SPI_LOOPBACK		(0x22C)
#define SE_SPI_CPOL		(0x230)
#define SE_SPI_DEMUX_OUTPUT_INV	(0x24C)
#define SE_SPI_DEMUX_SEL	(0x250)
#define SE_SPI_TRANS_CFG	(0x25C)
#define SE_SPI_WORD_LEN		(0x268)
#define SE_SPI_TX_TRANS_LEN	(0x26C)
#define SE_SPI_RX_TRANS_LEN	(0x270)
#define SE_SPI_PRE_POST_CMD_DLY	(0x274)
#define SE_SPI_DELAY_COUNTERS	(0x278)

/* SE_SPI_CPHA register fields */
#define CPHA			(BIT(0))

/* SE_SPI_LOOPBACK register fields */
#define LOOPBACK_ENABLE		(0x1)
#define NORMAL_MODE		(0x0)
#define LOOPBACK_MSK		(GENMASK(1, 0))

/* SE_SPI_CPOL register fields */
#define CPOL			(BIT(2))

/* SE_SPI_DEMUX_OUTPUT_INV register fields */
#define CS_DEMUX_OUTPUT_INV_MSK	(GENMASK(3, 0))

/* SE_SPI_DEMUX_SEL register fields */
#define CS_DEMUX_OUTPUT_SEL	(GENMASK(3, 0))

/* SE_SPI_TX_TRANS_CFG register fields */
#define CS_TOGGLE		(BIT(0))

/* SE_SPI_WORD_LEN register fields */
#define WORD_LEN_MSK		(GENMASK(9, 0))
#define MIN_WORD_LEN		(4)

/* SPI_TX/SPI_RX_TRANS_LEN fields */
#define TRANS_LEN_MSK		(GENMASK(23, 0))

/* M_CMD OP codes for SPI */
#define SPI_TX_ONLY		(1)
#define SPI_RX_ONLY		(2)
#define SPI_FULL_DUPLEX		(3)
#define SPI_TX_RX		(7)
#define SPI_CS_ASSERT		(8)
#define SPI_CS_DEASSERT		(9)
#define SPI_SCK_ONLY		(10)
/* M_CMD params for SPI */
#define SPI_PRE_CMD_DELAY	(0)
#define TIMESTAMP_BEFORE	(1)
#define FRAGMENTATION		(2)
#define TIMESTAMP_AFTER		(3)
#define POST_CMD_DELAY		(4)

struct spi_geni_master {
	struct se_geni_rsc spi_rsc;
	resource_size_t phys_addr;
	resource_size_t size;
	void __iomem *base;
	int irq;
	struct device *dev;
	int rx_fifo_depth;
	int tx_fifo_depth;
	int tx_fifo_width;
	int tx_wm;
	bool setup;
	u32 cur_speed_hz;
	int cur_word_len;
	unsigned int tx_rem_bytes;
	unsigned int rx_rem_bytes;
	struct spi_transfer *cur_xfer;
	struct completion xfer_done;
};

static struct spi_master *get_spi_master(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct spi_master *spi = platform_get_drvdata(pdev);

	return spi;
}

static int get_sclk(u32 speed_hz, unsigned long *sclk_freq)
{
	u32 root_freq[] = { 19200000 };

	*sclk_freq = root_freq[0];
	return 0;
}

static int do_spi_clk_cfg(u32 speed_hz, struct spi_geni_master *mas)
{
	unsigned long sclk_freq;
	int div = 0;
	int idx;
	struct se_geni_rsc *rsc = &mas->spi_rsc;
	u32 clk_sel = geni_read_reg(mas->base, SE_GENI_CLK_SEL);
	u32 m_clk_cfg = geni_read_reg(mas->base, GENI_SER_M_CLK_CFG);
	int ret;

	clk_sel &= ~CLK_SEL_MSK;
	m_clk_cfg &= ~CLK_DIV_MSK;

	idx = get_sclk(speed_hz, &sclk_freq);
	if (idx < 0)
		return -EINVAL;

	div = ((sclk_freq / SPI_OVERSAMPLING) / speed_hz);
	if (!div)
		return -EINVAL;

	clk_sel |= (idx & CLK_SEL_MSK);
	m_clk_cfg |= ((div << CLK_DIV_SHFT) | SER_CLK_EN);
	ret = clk_set_rate(rsc->se_clk, sclk_freq);
	if (ret)
		return ret;

	geni_write_reg(clk_sel, mas->base, SE_GENI_CLK_SEL);
	geni_write_reg(m_clk_cfg, mas->base, GENI_SER_M_CLK_CFG);
	return 0;
}

static void spi_setup_word_len(struct spi_geni_master *mas, u32 mode,
						int bits_per_word)
{
	int pack_words = mas->tx_fifo_width / bits_per_word;
	bool msb_first = (mode & SPI_LSB_FIRST) ? false : true;
	u32 word_len = geni_read_reg(mas->base, SE_SPI_WORD_LEN);

	word_len &= ~WORD_LEN_MSK;
	word_len |= ((bits_per_word - MIN_WORD_LEN) & WORD_LEN_MSK);
	se_config_packing(mas->base, bits_per_word, pack_words, msb_first);
	geni_write_reg(word_len, mas->base, SE_SPI_WORD_LEN);
}

static int spi_geni_prepare_message(struct spi_master *spi_mas,
					struct spi_message *spi_msg)
{
	struct spi_device *spi_slv = spi_msg->spi;
	struct spi_geni_master *mas = spi_master_get_devdata(spi_mas);
	u16 mode = spi_slv->mode;
	u32 loopback_cfg = geni_read_reg(mas->base, SE_SPI_LOOPBACK);
	u32 cpol = geni_read_reg(mas->base, SE_SPI_CPOL);
	u32 cpha = geni_read_reg(mas->base, SE_SPI_CPHA);
	u32 demux_sel = geni_read_reg(mas->base, SE_SPI_DEMUX_SEL);
	u32 demux_output_inv =
			geni_read_reg(mas->base, SE_SPI_DEMUX_OUTPUT_INV);
	int ret = 0;

	loopback_cfg &= ~LOOPBACK_MSK;
	cpol &= ~CPOL;
	cpha &= ~CPHA;
	demux_output_inv &= ~BIT(spi_slv->chip_select);

	if (mode & SPI_LOOP)
		loopback_cfg |= LOOPBACK_ENABLE;

	if (mode & SPI_CPOL)
		cpol |= CPOL;

	if (mode & SPI_CPHA)
		cpha |= CPHA;

	if (spi_slv->mode & SPI_CS_HIGH)
		demux_output_inv |= BIT(spi_slv->chip_select);

	demux_sel |= BIT(spi_slv->chip_select);
	mas->cur_speed_hz = spi_slv->max_speed_hz;
	mas->cur_word_len = spi_slv->bits_per_word;

	ret = do_spi_clk_cfg(mas->cur_speed_hz, mas);
	if (ret) {
		dev_err(&spi_mas->dev, "Err setting clks ret(%d) for %d\n",
							ret, mas->cur_speed_hz);
		goto prepare_message_exit;
	}
	spi_setup_word_len(mas, spi_slv->mode, spi_slv->bits_per_word);
	geni_write_reg(loopback_cfg, mas->base, SE_SPI_LOOPBACK);
	geni_write_reg(demux_sel, mas->base, SE_SPI_DEMUX_SEL);
	geni_write_reg(cpha, mas->base, SE_SPI_CPHA);
	geni_write_reg(cpol, mas->base, SE_SPI_CPOL);
	geni_write_reg(demux_output_inv, mas->base, SE_SPI_DEMUX_OUTPUT_INV);
	/* Ensure message level attributes are written before returning */
	mb();
prepare_message_exit:
	return ret;
}

static int spi_geni_unprepare_message(struct spi_master *spi_mas,
					struct spi_message *spi_msg)
{
	struct spi_geni_master *mas = spi_master_get_devdata(spi_mas);

	mas->cur_speed_hz = 0;
	mas->cur_word_len = 0;
	return 0;
}

static int spi_geni_prepare_transfer_hardware(struct spi_master *spi)
{
	struct spi_geni_master *mas = spi_master_get_devdata(spi);
	int ret = 0;

	ret = pm_runtime_get_sync(mas->dev);
	if (ret < 0) {
		dev_err(mas->dev, "Error enabling SE resources\n");
		pm_runtime_put_noidle(mas->dev);
		goto exit_prepare_transfer_hardware;
	} else {
		ret = 0;
	}

	if (unlikely(!mas->setup)) {
		int proto = get_se_proto(mas->base);

		if (unlikely(proto != SPI)) {
			dev_err(mas->dev, "Invalid proto %d\n", proto);
			return -ENXIO;
		}
		geni_se_init(mas->base, FIFO_MODE, 0x0,
						(mas->tx_fifo_depth - 2));
		mas->tx_fifo_depth = get_tx_fifo_depth(mas->base);
		mas->rx_fifo_depth = get_rx_fifo_depth(mas->base);
		mas->tx_fifo_width = get_tx_fifo_width(mas->base);
		/* Transmit an entire FIFO worth of data per IRQ */
		mas->tx_wm = 1;
		dev_dbg(mas->dev, "tx_fifo %d rx_fifo %d tx_width %d\n",
			mas->tx_fifo_depth, mas->rx_fifo_depth,
			mas->tx_fifo_width);
		mas->setup = true;
	}
exit_prepare_transfer_hardware:
	return ret;
}

static int spi_geni_unprepare_transfer_hardware(struct spi_master *spi)
{
	struct spi_geni_master *mas = spi_master_get_devdata(spi);

	pm_runtime_put_sync(mas->dev);
	return 0;
}

static void setup_fifo_xfer(struct spi_transfer *xfer,
				struct spi_geni_master *mas, u16 mode,
				struct spi_master *spi)
{
	u32 m_cmd = 0;
	u32 m_param = 0;
	u32 spi_tx_cfg = geni_read_reg(mas->base, SE_SPI_TRANS_CFG);
	u32 trans_len = 0;

	if (xfer->bits_per_word != mas->cur_word_len) {
		spi_setup_word_len(mas, mode, xfer->bits_per_word);
		mas->cur_word_len = xfer->bits_per_word;
	}

	if (xfer->tx_buf && xfer->rx_buf)
		m_cmd = SPI_FULL_DUPLEX;
	else if (xfer->tx_buf)
		m_cmd = SPI_TX_ONLY;
	else if (xfer->rx_buf)
		m_cmd = SPI_RX_ONLY;

	spi_tx_cfg &= ~CS_TOGGLE;
	if (xfer->cs_change)
		spi_tx_cfg |= CS_TOGGLE;
	trans_len = ((xfer->len / (mas->cur_word_len >> 3)) & TRANS_LEN_MSK);
	if (!list_is_last(&xfer->transfer_list, &spi->cur_msg->transfers))
		m_param |= FRAGMENTATION;

	mas->cur_xfer = xfer;
	if (m_cmd & SPI_TX_ONLY) {
		mas->tx_rem_bytes = xfer->len;
		geni_write_reg(trans_len, mas->base, SE_SPI_TX_TRANS_LEN);
	}

	if (m_cmd & SPI_RX_ONLY) {
		geni_write_reg(trans_len, mas->base, SE_SPI_RX_TRANS_LEN);
		mas->rx_rem_bytes = xfer->len;
	}
	geni_write_reg(spi_tx_cfg, mas->base, SE_SPI_TRANS_CFG);
	geni_setup_m_cmd(mas->base, m_cmd, m_param);
	geni_write_reg(mas->tx_wm, mas->base, SE_GENI_TX_WATERMARK_REG);
	/* Ensure all writes are done before the WM interrupt */
	mb();
}

static void handle_fifo_timeout(struct spi_geni_master *mas)
{
	unsigned long timeout;
	u32 tx_trans_len = geni_read_reg(mas->base, SE_SPI_TX_TRANS_LEN);
	u32 rx_trans_len = geni_read_reg(mas->base, SE_SPI_RX_TRANS_LEN);
	u32 spi_tx_cfg = geni_read_reg(mas->base, SE_SPI_TRANS_CFG);
	u32 m_cmd = geni_read_reg(mas->base, SE_GENI_M_CMD0);

	/* Timed-out on a FIFO xfer, print relevant reg info. */
	dev_err(mas->dev, "tx_rem_bytes %d rx_rem_bytes %d\n",
			mas->tx_rem_bytes, mas->rx_rem_bytes);
	dev_err(mas->dev, "tx_trans_len %d rx_trans_len %d\n", tx_trans_len,
								rx_trans_len);
	dev_err(mas->dev, "spi_tx_cfg 0x%x m_cmd 0x%x\n", spi_tx_cfg, m_cmd);
	reinit_completion(&mas->xfer_done);
	geni_cancel_m_cmd(mas->base);
	/* Ensure cmd cancel is written */
	mb();
	timeout = wait_for_completion_timeout(&mas->xfer_done, HZ);
	if (!timeout) {
		reinit_completion(&mas->xfer_done);
		geni_abort_m_cmd(mas->base);
		/* Ensure cmd abort is written */
		mb();
		timeout = wait_for_completion_timeout(&mas->xfer_done,
								HZ);
		if (!timeout)
			dev_err(mas->dev,
				"Failed to cancel/abort m_cmd\n");
	}
}

static int spi_geni_transfer_one(struct spi_master *spi,
				struct spi_device *slv,
				struct spi_transfer *xfer)
{
	int ret = 0;
	struct spi_geni_master *mas = spi_master_get_devdata(spi);
	unsigned long timeout;

	if ((xfer->tx_buf == NULL) && (xfer->rx_buf == NULL)) {
		dev_err(mas->dev, "Invalid xfer both tx rx are NULL\n");
		return -EINVAL;
	}

	reinit_completion(&mas->xfer_done);
	/* Speed and bits per word can be overridden per transfer */
	if (xfer->speed_hz != mas->cur_speed_hz) {
		ret = do_spi_clk_cfg(mas->cur_speed_hz, mas);
		if (ret) {
			dev_err(mas->dev, "%s:Err setting clks:%d\n",
								__func__, ret);
			goto geni_transfer_one_exit;
		}
		mas->cur_speed_hz = xfer->speed_hz;
	}

	setup_fifo_xfer(xfer, mas, slv->mode, spi);
	timeout = wait_for_completion_timeout(&mas->xfer_done,
					msecs_to_jiffies(SPI_XFER_TIMEOUT_MS));
	if (!timeout) {
		dev_err(mas->dev, "Xfer[len %d tx %p rx %p n %d] timed out.\n",
						xfer->len, xfer->tx_buf,
						xfer->rx_buf,
						xfer->bits_per_word);
		ret = -ETIMEDOUT;
		handle_fifo_timeout(mas);
	}
geni_transfer_one_exit:
	return ret;
}

static void geni_spi_handle_tx(struct spi_geni_master *mas)
{
	int i = 0;
	int tx_fifo_width = (mas->tx_fifo_width >> 3);
	int max_bytes = (mas->tx_fifo_depth - mas->tx_wm) * tx_fifo_width;
	const u8 *tx_buf = mas->cur_xfer->tx_buf;

	tx_buf += (mas->cur_xfer->len - mas->tx_rem_bytes);
	max_bytes = min_t(int, mas->tx_rem_bytes, max_bytes);
	while (i < max_bytes) {
		int j;
		u32 fifo_word = 0;
		u8 *fifo_byte;
		int bytes_to_write = min_t(int, (max_bytes - i), tx_fifo_width);

		fifo_byte = (u8 *)&fifo_word;
		for (j = 0; j < bytes_to_write; j++)
			fifo_byte[j] = tx_buf[i++];
		geni_write_reg(fifo_word, mas->base, SE_GENI_TX_FIFOn);
		/* Ensure FIFO writes are written in order */
		mb();
	}
	mas->tx_rem_bytes -= max_bytes;
	if (!mas->tx_rem_bytes) {
		geni_write_reg(0, mas->base, SE_GENI_TX_WATERMARK_REG);
		/* Barrier here before return to prevent further ISRs */
		mb();
	}
}

static void geni_spi_handle_rx(struct spi_geni_master *mas)
{
	int i = 0;
	int fifo_width = (mas->tx_fifo_width >> 3);
	u32 rx_fifo_status = geni_read_reg(mas->base, SE_GENI_RX_FIFO_STATUS);
	int rx_bytes = 0;
	int rx_wc = 0;
	u8 *rx_buf = mas->cur_xfer->rx_buf;

	rx_wc = (rx_fifo_status & RX_FIFO_WC_MSK);
	if (rx_fifo_status & RX_LAST) {
		int rx_last_byte_valid =
			(rx_fifo_status & RX_LAST_BYTE_VALID_MSK)
					>> RX_LAST_BYTE_VALID_SHFT;
		if (rx_last_byte_valid && (rx_last_byte_valid < 4)) {
			rx_wc -= 1;
			rx_bytes += rx_last_byte_valid;
		}
	}
	rx_bytes += rx_wc * fifo_width;
	rx_bytes = min_t(int, mas->rx_rem_bytes, rx_bytes);
	rx_buf += (mas->cur_xfer->len - mas->rx_rem_bytes);
	while (i < rx_bytes) {
		u32 fifo_word = 0;
		u8 *fifo_byte;
		int read_bytes = min_t(int, (rx_bytes - i), fifo_width);
		int j;

		fifo_word = geni_read_reg(mas->base, SE_GENI_RX_FIFOn);
		fifo_byte = (u8 *)&fifo_word;
		for (j = 0; j < read_bytes; j++)
			rx_buf[i++] = fifo_byte[j];
	}
	mas->rx_rem_bytes -= rx_bytes;
}

static irqreturn_t geni_spi_irq(int irq, void *dev)
{
	struct spi_geni_master *mas = dev;
	u32 m_irq = geni_read_reg(mas->base, SE_GENI_M_IRQ_STATUS);

	if ((m_irq & M_RX_FIFO_WATERMARK_EN) || (m_irq & M_RX_FIFO_LAST_EN))
		geni_spi_handle_rx(mas);

	if ((m_irq & M_TX_FIFO_WATERMARK_EN))
		geni_spi_handle_tx(mas);

	if ((m_irq & M_CMD_DONE_EN) || (m_irq & M_CMD_CANCEL_EN) ||
		(m_irq & M_CMD_ABORT_EN)) {
		complete(&mas->xfer_done);
	}
	geni_write_reg(m_irq, mas->base, SE_GENI_M_IRQ_CLEAR);
	return IRQ_HANDLED;
}

static int spi_geni_probe(struct platform_device *pdev)
{
	int ret;
	struct spi_master *spi;
	struct spi_geni_master *geni_mas;
	struct se_geni_rsc *rsc;
	struct resource *res;

	spi = spi_alloc_master(&pdev->dev, sizeof(struct spi_geni_master));
	if (!spi) {
		ret = -ENOMEM;
		dev_err(&pdev->dev, "Failed to alloc spi struct\n");
		goto spi_geni_probe_err;
	}

	platform_set_drvdata(pdev, spi);
	geni_mas = spi_master_get_devdata(spi);
	rsc = &geni_mas->spi_rsc;
	geni_mas->dev = &pdev->dev;
	spi->dev.of_node = pdev->dev.of_node;
	rsc->geni_pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR_OR_NULL(rsc->geni_pinctrl)) {
		dev_err(&pdev->dev, "No pinctrl config specified!\n");
		ret = PTR_ERR(rsc->geni_pinctrl);
		goto spi_geni_probe_err;
	}

	rsc->geni_gpio_active = pinctrl_lookup_state(rsc->geni_pinctrl,
							PINCTRL_DEFAULT);
	if (IS_ERR_OR_NULL(rsc->geni_gpio_active)) {
		dev_err(&pdev->dev, "No default config specified!\n");
		ret = PTR_ERR(rsc->geni_gpio_active);
		goto spi_geni_probe_err;
	}

	rsc->geni_gpio_sleep = pinctrl_lookup_state(rsc->geni_pinctrl,
							PINCTRL_SLEEP);
	if (IS_ERR_OR_NULL(rsc->geni_gpio_sleep)) {
		dev_err(&pdev->dev, "No sleep config specified!\n");
		ret = PTR_ERR(rsc->geni_gpio_sleep);
		goto spi_geni_probe_err;
	}

	rsc->se_clk = devm_clk_get(&pdev->dev, "se-clk");
	if (IS_ERR(rsc->se_clk)) {
		ret = PTR_ERR(rsc->se_clk);
		dev_err(&pdev->dev, "Err getting SE Core clk %d\n", ret);
		goto spi_geni_probe_err;
	}

	rsc->m_ahb_clk = devm_clk_get(&pdev->dev, "m-ahb");
	if (IS_ERR(rsc->m_ahb_clk)) {
		ret = PTR_ERR(rsc->m_ahb_clk);
		dev_err(&pdev->dev, "Err getting M AHB clk %d\n", ret);
		goto spi_geni_probe_err;
	}

	rsc->s_ahb_clk = devm_clk_get(&pdev->dev, "s-ahb");
	if (IS_ERR(rsc->s_ahb_clk)) {
		ret = PTR_ERR(rsc->s_ahb_clk);
		dev_err(&pdev->dev, "Err getting S AHB clk %d\n", ret);
		goto spi_geni_probe_err;
	}

	if (of_property_read_u32(pdev->dev.of_node, "spi-max-frequency",
				&spi->max_speed_hz)) {
		dev_err(&pdev->dev, "Max frequency not specified.\n");
		ret = -ENXIO;
		goto spi_geni_probe_err;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "se_phys");
	if (!res) {
		ret = -ENXIO;
		dev_err(&pdev->dev, "Err getting IO region\n");
		goto spi_geni_probe_err;
	}

	geni_mas->phys_addr = res->start;
	geni_mas->size = resource_size(res);
	geni_mas->base = devm_ioremap(&pdev->dev, res->start,
						resource_size(res));
	if (!geni_mas->base) {
		ret = -ENOMEM;
		dev_err(&pdev->dev, "Err IO mapping iomem\n");
		goto spi_geni_probe_err;
	}

	geni_mas->irq = platform_get_irq(pdev, 0);
	if (geni_mas->irq < 0) {
		dev_err(&pdev->dev, "Err getting IRQ\n");
		ret = geni_mas->irq;
		goto spi_geni_probe_unmap;
	}
	ret = devm_request_irq(&pdev->dev, geni_mas->irq, geni_spi_irq,
			       IRQF_TRIGGER_HIGH, "spi_geni", geni_mas);
	if (ret) {
		dev_err(&pdev->dev, "Request_irq failed:%d: err:%d\n",
				   geni_mas->irq, ret);
		goto spi_geni_probe_unmap;
	}

	spi->mode_bits = (SPI_CPOL | SPI_CPHA | SPI_LOOP | SPI_CS_HIGH);
	spi->bits_per_word_mask = SPI_BPW_RANGE_MASK(4, 32);
	spi->num_chipselect = SPI_NUM_CHIPSELECT;
	spi->prepare_transfer_hardware = spi_geni_prepare_transfer_hardware;
	spi->prepare_message = spi_geni_prepare_message;
	spi->unprepare_message = spi_geni_unprepare_message;
	spi->transfer_one = spi_geni_transfer_one;
	spi->unprepare_transfer_hardware
			= spi_geni_unprepare_transfer_hardware;
	spi->auto_runtime_pm = false;

	init_completion(&geni_mas->xfer_done);
	pm_runtime_enable(&pdev->dev);
	ret = spi_register_master(spi);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register SPI master\n");
		goto spi_geni_probe_unmap;
	}
	return ret;
spi_geni_probe_unmap:
	devm_iounmap(&pdev->dev, geni_mas->base);
spi_geni_probe_err:
	spi_master_put(spi);
	return ret;
}

static int spi_geni_remove(struct platform_device *pdev)
{
	struct spi_master *master = platform_get_drvdata(pdev);
	struct spi_geni_master *geni_mas = spi_master_get_devdata(master);

	spi_unregister_master(master);
	se_geni_resources_off(&geni_mas->spi_rsc);
	pm_runtime_put_noidle(&pdev->dev);
	pm_runtime_disable(&pdev->dev);
	return 0;
}

#ifdef CONFIG_PM
static int spi_geni_runtime_suspend(struct device *dev)
{
	int ret = 0;
	struct spi_master *spi = get_spi_master(dev);
	struct spi_geni_master *geni_mas = spi_master_get_devdata(spi);

	ret = se_geni_resources_off(&geni_mas->spi_rsc);
	return ret;
}

static int spi_geni_runtime_resume(struct device *dev)
{
	int ret = 0;
	struct spi_master *spi = get_spi_master(dev);
	struct spi_geni_master *geni_mas = spi_master_get_devdata(spi);

	ret = se_geni_resources_on(&geni_mas->spi_rsc);
	return ret;
}

static int spi_geni_resume(struct device *dev)
{
	return 0;
}

static int spi_geni_suspend(struct device *dev)
{
	if (!pm_runtime_status_suspended(dev))
		return -EBUSY;
	return 0;
}
#else
static int spi_geni_runtime_suspend(struct device *dev)
{
	return 0;
}

static int spi_geni_runtime_resume(struct device *dev)
{
	return 0;
}

static int spi_geni_resume(struct device *dev)
{
	return 0;
}

static int spi_geni_suspend(struct device *dev)
{
	return 0;
}
#endif

static const struct dev_pm_ops spi_geni_pm_ops = {
	SET_RUNTIME_PM_OPS(spi_geni_runtime_suspend,
					spi_geni_runtime_resume, NULL)
	SET_SYSTEM_SLEEP_PM_OPS(spi_geni_suspend, spi_geni_resume)
};

static const struct of_device_id spi_geni_dt_match[] = {
	{ .compatible = "qcom,spi-geni" },
	{}
};

static struct platform_driver spi_geni_driver = {
	.probe  = spi_geni_probe,
	.remove = spi_geni_remove,
	.driver = {
		.name = "spi_geni",
		.pm = &spi_geni_pm_ops,
		.of_match_table = spi_geni_dt_match,
	},
};
module_platform_driver(spi_geni_driver);

MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:spi_geni");
