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

#ifndef _LINUX_QCOM_GENI_SE
#define _LINUX_QCOM_GENI_SE
#include <linux/io.h>

enum se_xfer_mode {
	INVALID,
	FIFO_MODE,
	GSI_DMA,
};

enum se_protocol_types {
	NONE,
	SPI,
	UART,
	I2C,
	I3C
};

#define GENI_INIT_CFG_REVISION		(0x0)
#define GENI_S_INIT_CFG_REVISION	(0x4)
#define GENI_FORCE_DEFAULT_REG		(0x20)
#define GENI_OUTPUT_CTRL		(0x24)
#define GENI_CGC_CTRL			(0x28)
#define SE_GENI_STATUS			(0x40)
#define GENI_SER_M_CLK_CFG		(0x48)
#define GENI_SER_S_CLK_CFG		(0x4C)
#define GENI_CLK_CTRL_RO		(0x60)
#define GENI_IF_DISABLE_RO		(0x64)
#define GENI_FW_REVISION_RO		(0x68)
#define GENI_FW_S_REVISION_RO		(0x6C)
#define SE_GENI_CLK_SEL			(0x7C)
#define SE_GENI_DMA_MODE_EN		(0x258)
#define SE_GENI_TX_PACKING_CFG0		(0x260)
#define SE_GENI_TX_PACKING_CFG1		(0x264)
#define SE_GENI_RX_PACKING_CFG0		(0x284)
#define SE_GENI_RX_PACKING_CFG1		(0x288)
#define SE_GENI_M_CMD0			(0x600)
#define SE_GENI_M_CMD_CTRL_REG		(0x604)
#define SE_GENI_M_IRQ_STATUS		(0x610)
#define SE_GENI_M_IRQ_EN		(0x614)
#define SE_GENI_M_IRQ_CLEAR		(0x618)
#define SE_GENI_S_CMD0			(0x630)
#define SE_GENI_S_CMD_CTRL_REG		(0x634)
#define SE_GENI_S_IRQ_STATUS		(0x640)
#define SE_GENI_S_IRQ_EN		(0x644)
#define SE_GENI_S_IRQ_CLEAR		(0x648)
#define SE_GENI_TX_FIFOn		(0x700)
#define SE_GENI_RX_FIFOn		(0x780)
#define SE_GENI_TX_FIFO_STATUS		(0x800)
#define SE_GENI_RX_FIFO_STATUS		(0x804)
#define SE_GENI_TX_WATERMARK_REG	(0x80C)
#define SE_GENI_RX_WATERMARK_REG	(0x810)
#define SE_GENI_RX_RFR_WATERMARK_REG	(0x814)
#define SE_GENI_M_GP_LENGTH		(0x910)
#define SE_GENI_S_GP_LENGTH		(0x914)
#define SE_IRQ_EN			(0xE1C)
#define SE_HW_PARAM_0			(0xE24)
#define SE_HW_PARAM_1			(0xE28)
#define SE_DMA_GENERAL_CFG		(0xE30)

/* GENI_OUTPUT_CTRL fields */
#define DEFAULT_IO_OUTPUT_CTRL_MSK	(GENMASK(6, 0))

/* GENI_FORCE_DEFAULT_REG fields */
#define FORCE_DEFAULT	(BIT(0))

/* GENI_CGC_CTRL fields */
#define CFG_AHB_CLK_CGC_ON		(BIT(0))
#define CFG_AHB_WR_ACLK_CGC_ON		(BIT(1))
#define DATA_AHB_CLK_CGC_ON		(BIT(2))
#define SCLK_CGC_ON			(BIT(3))
#define TX_CLK_CGC_ON			(BIT(4))
#define RX_CLK_CGC_ON			(BIT(5))
#define EXT_CLK_CGC_ON			(BIT(6))
#define PROG_RAM_HCLK_OFF		(BIT(8))
#define PROG_RAM_SCLK_OFF		(BIT(9))
#define DEFAULT_CGC_EN			(GENMASK(6, 0))

/* GENI_STATUS fields */
#define M_GENI_CMD_ACTIVE		(BIT(0))
#define S_GENI_CMD_ACTIVE		(BIT(12))

/* GENI_SER_M_CLK_CFG/GENI_SER_S_CLK_CFG */
#define SER_CLK_EN			(BIT(0))
#define CLK_DIV_MSK			(GENMASK(15, 4))
#define CLK_DIV_SHFT			(4)

/* CLK_CTRL_RO fields */

/* IF_DISABLE_RO fields */

/* FW_REVISION_RO fields */
#define FW_REV_PROTOCOL_MSK	(GENMASK(15, 8))
#define FW_REV_PROTOCOL_SHFT	(8)

/* SE_GENI_DMA_MODE_EN */
#define GENI_DMA_MODE_EN	(BIT(0))

/* GENI_M_CMD0 fields */
#define M_OPCODE_MSK		(GENMASK(31, 27))
#define M_OPCODE_SHFT		(27)
#define M_PARAMS_MSK		(GENMASK(26, 0))

/* GENI_M_CMD_CTRL_REG */
#define M_GENI_CMD_CANCEL	BIT(2)
#define M_GENI_CMD_ABORT	BIT(1)
#define M_GENI_DISABLE		BIT(0)

/* GENI_S_CMD0 fields */
#define S_OPCODE_MSK		(GENMASK(31, 27))
#define S_OPCODE_SHFT		(27)
#define S_PARAMS_MSK		(GENMASK(26, 0))

/* GENI_S_CMD_CTRL_REG */
#define S_GENI_CMD_CANCEL	(BIT(2))
#define S_GENI_CMD_ABORT	(BIT(1))
#define S_GENI_DISABLE		(BIT(0))

/* GENI_M_IRQ_EN fields */
#define M_CMD_DONE_EN		(BIT(0))
#define M_CMD_OVERRUN_EN	(BIT(1))
#define M_ILLEGAL_CMD_EN	(BIT(2))
#define M_CMD_FAILURE_EN	(BIT(3))
#define M_CMD_CANCEL_EN		(BIT(4))
#define M_CMD_ABORT_EN		(BIT(5))
#define M_TIMESTAMP_EN		(BIT(6))
#define M_RX_IRQ_EN		(BIT(7))
#define M_GP_SYNC_IRQ_0_EN	(BIT(8))
#define M_GP_IRQ_0_EN		(BIT(9))
#define M_GP_IRQ_1_EN		(BIT(10))
#define M_GP_IRQ_2_EN		(BIT(11))
#define M_GP_IRQ_3_EN		(BIT(12))
#define M_GP_IRQ_4_EN		(BIT(13))
#define M_GP_IRQ_5_EN		(BIT(14))
#define M_IO_DATA_DEASSERT_EN	(BIT(22))
#define M_IO_DATA_ASSERT_EN	(BIT(23))
#define M_RX_FIFO_RD_ERR_EN	(BIT(24))
#define M_RX_FIFO_WR_ERR_EN	(BIT(25))
#define M_RX_FIFO_WATERMARK_EN	(BIT(26))
#define M_RX_FIFO_LAST_EN	(BIT(27))
#define M_TX_FIFO_RD_ERR_EN	(BIT(28))
#define M_TX_FIFO_WR_ERR_EN	(BIT(29))
#define M_TX_FIFO_WATERMARK_EN	(BIT(30))
#define M_SEC_IRQ_EN		(BIT(31))
#define M_COMMON_GENI_M_IRQ_EN	(GENMASK(3, 0) |  M_TIMESTAMP_EN | \
				GENMASK(14, 8) | M_IO_DATA_DEASSERT_EN | \
				M_IO_DATA_ASSERT_EN | M_RX_FIFO_RD_ERR_EN | \
				M_RX_FIFO_WR_ERR_EN | M_TX_FIFO_RD_ERR_EN | \
				M_TX_FIFO_WR_ERR_EN | M_SEC_IRQ_EN)

/* GENI_S_IRQ_EN fields */
#define S_CMD_DONE_EN		(BIT(0))
#define S_CMD_OVERRUN_EN	(BIT(1))
#define S_ILLEGAL_CMD_EN	(BIT(2))
#define S_CMD_FAILURE_EN	(BIT(3))
#define S_CMD_CANCEL_EN		(BIT(4))
#define S_CMD_ABORT_EN		(BIT(5))
#define S_GP_SYNC_IRQ_0_EN	(BIT(8))
#define S_GP_IRQ_0_EN		(BIT(9))
#define S_GP_IRQ_1_EN		(BIT(10))
#define S_GP_IRQ_2_EN		(BIT(11))
#define S_GP_IRQ_3_EN		(BIT(12))
#define S_GP_IRQ_4_EN		(BIT(13))
#define S_GP_IRQ_5_EN		(BIT(14))
#define S_IO_DATA_DEASSERT_EN	(BIT(22))
#define S_IO_DATA_ASSERT_EN	(BIT(23))
#define S_RX_FIFO_RD_ERR_EN	(BIT(24))
#define S_RX_FIFO_WR_ERR_EN	(BIT(25))
#define S_RX_FIFO_WATERMARK_EN	(BIT(26))
#define S_RX_FIFO_LAST_EN	(BIT(27))
#define S_COMMON_GENI_S_IRQ_EN	(GENMASK(3, 0) | GENMASK(14, 8) | \
				 S_RX_FIFO_RD_ERR_EN | S_RX_FIFO_WR_ERR_EN)

/*  GENI_/TX/RX/RX_RFR/_WATERMARK_REG fields */
#define WATERMARK_MSK		(GENMASK(5, 0))

/* GENI_TX_FIFO_STATUS fields */
#define TX_FIFO_WC		(GENMASK(27, 0))

/*  GENI_RX_FIFO_STATUS fields */
#define RX_LAST			(BIT(31))
#define RX_LAST_BYTE_VALID_MSK	(GENMASK(30, 28))
#define RX_LAST_BYTE_VALID_SHFT	(28)
#define RX_FIFO_WC_MSK		(GENMASK(24, 0))

/* SE_IRQ_EN fields */
#define DMA_RX_IRQ_EN		(BIT(0))
#define DMA_TX_IRQ_EN		(BIT(1))
#define GENI_M_IRQ_EN		(BIT(2))
#define GENI_S_IRQ_EN		(BIT(3))

/* SE_HW_PARAM_0 fields */
#define TX_FIFO_WIDTH_MSK	(GENMASK(29, 24))
#define TX_FIFO_WIDTH_SHFT	(24)
#define TX_FIFO_DEPTH_MSK	(GENMASK(21, 16))
#define TX_FIFO_DEPTH_SHFT	(16)

/* SE_HW_PARAM_1 fields */
#define RX_FIFO_WIDTH_MSK	(GENMASK(29, 24))
#define RX_FIFO_WIDTH_SHFT	(24)
#define RX_FIFO_DEPTH_MSK	(GENMASK(21, 16))
#define RX_FIFO_DEPTH_SHFT	(16)

/* SE_DMA_GENERAL_CFG */
#define DMA_RX_CLK_CGC_ON	(BIT(0))
#define DMA_TX_CLK_CGC_ON	(BIT(1))
#define DMA_AHB_SLV_CFG_ON	(BIT(2))
#define AHB_SEC_SLV_CLK_CGC_ON	(BIT(3))
#define DUMMY_RX_NON_BUFFERABLE	(BIT(4))
#define RX_DMA_ZERO_PADDING_EN	(BIT(5))
#define RX_DMA_IRQ_DELAY_MSK	(GENMASK(8, 6))
#define RX_DMA_IRQ_DELAY_SHFT	(6)

static inline unsigned int geni_read_reg(void __iomem *base, int offset)
{
	return readl_relaxed(base + offset);
}

static inline void geni_write_reg(unsigned int value, void __iomem *base,
				int offset)
{
	return writel_relaxed(value, (base + offset));
}

static inline int get_se_proto(void __iomem *base)
{
	int proto = 0;

	proto = ((geni_read_reg(base, GENI_FW_REVISION_RO)
			& FW_REV_PROTOCOL_MSK) >> FW_REV_PROTOCOL_SHFT);
	return proto;
}

static inline int se_geni_irq_en(void __iomem *base, int mode)
{
	int ret = 0;
	unsigned int common_geni_m_irq_en;
	unsigned int common_geni_s_irq_en;
	int proto = get_se_proto(base);

	common_geni_m_irq_en = geni_read_reg(base, SE_GENI_M_IRQ_EN);
	common_geni_s_irq_en = geni_read_reg(base, SE_GENI_S_IRQ_EN);
	/* Common to all modes */
	common_geni_m_irq_en |= M_COMMON_GENI_M_IRQ_EN;
	common_geni_s_irq_en |= S_COMMON_GENI_S_IRQ_EN;

	switch (mode) {
	case FIFO_MODE:
	{
		if (proto == I2C) {
			common_geni_m_irq_en |=
				(M_CMD_DONE_EN | M_TX_FIFO_WATERMARK_EN);
			common_geni_s_irq_en |= S_CMD_DONE_EN;
		}
		break;
	}
	case GSI_DMA:
		break;
	default:
		pr_err("%s: Invalid mode %d\n", __func__, mode);
		ret = -ENXIO;
		goto exit_irq_en;
	}


	geni_write_reg(common_geni_m_irq_en, base, SE_GENI_M_IRQ_EN);
	geni_write_reg(common_geni_s_irq_en, base, SE_GENI_S_IRQ_EN);
exit_irq_en:
	return ret;
}


static inline void se_set_rx_rfr_wm(void __iomem *base, unsigned int rx_wm,
						unsigned int rx_rfr)
{
	geni_write_reg(rx_wm, base, SE_GENI_RX_WATERMARK_REG);
	geni_write_reg(rx_rfr, base, SE_GENI_RX_RFR_WATERMARK_REG);
}

static inline int se_io_set_mode(void __iomem *base, int mode)
{
	int ret = 0;
	unsigned int io_mode = 0;
	unsigned int geni_dma_mode = 0;

	io_mode = geni_read_reg(base, SE_IRQ_EN);
	geni_dma_mode = geni_read_reg(base, SE_GENI_DMA_MODE_EN);

	switch (mode) {
	case FIFO_MODE:
	{
		io_mode |= (GENI_M_IRQ_EN | GENI_S_IRQ_EN);
		io_mode |= (DMA_TX_IRQ_EN | DMA_RX_IRQ_EN);
		geni_dma_mode &= ~GENI_DMA_MODE_EN;
		break;

	}
	default:
		ret = -ENXIO;
		goto exit_set_mode;
	}
	geni_write_reg(io_mode, base, SE_IRQ_EN);
	geni_write_reg(geni_dma_mode, base, SE_GENI_DMA_MODE_EN);
exit_set_mode:
	return ret;
}

static inline void se_io_init(void __iomem *base)
{
	unsigned int io_op_ctrl = 0;
	unsigned int geni_cgc_ctrl;
	unsigned int dma_general_cfg;

	geni_cgc_ctrl = geni_read_reg(base, GENI_CGC_CTRL);
	dma_general_cfg = geni_read_reg(base, SE_DMA_GENERAL_CFG);
	geni_cgc_ctrl |= DEFAULT_CGC_EN;
	dma_general_cfg |= (AHB_SEC_SLV_CLK_CGC_ON | DMA_AHB_SLV_CFG_ON |
			DMA_TX_CLK_CGC_ON | DMA_RX_CLK_CGC_ON);
	io_op_ctrl |= DEFAULT_IO_OUTPUT_CTRL_MSK;
	geni_write_reg(geni_cgc_ctrl, base, GENI_CGC_CTRL);
	geni_write_reg(dma_general_cfg, base, SE_DMA_GENERAL_CFG);

	geni_write_reg(io_op_ctrl, base, GENI_OUTPUT_CTRL);
	geni_write_reg(FORCE_DEFAULT, base, GENI_FORCE_DEFAULT_REG);
}

static inline int geni_se_init(void __iomem *base, int mode,
		unsigned int rx_wm, unsigned int rx_rfr)
{
	int ret = 0;

	se_io_init(base);
	ret = se_io_set_mode(base, mode);
	if (ret)
		goto exit_geni_se_init;

	se_set_rx_rfr_wm(base, rx_wm, rx_rfr);
	ret = se_geni_irq_en(base, mode);
	if (ret)
		goto exit_geni_se_init;

exit_geni_se_init:
	return ret;
}

static inline void geni_setup_m_cmd(void __iomem *base, u32 cmd,
								u32 params)
{
	u32 m_cmd = geni_read_reg(base, SE_GENI_M_CMD0);

	m_cmd &= ~(M_OPCODE_MSK | M_PARAMS_MSK);
	m_cmd |= (cmd << M_OPCODE_SHFT);
	m_cmd |= (params & M_PARAMS_MSK);
	geni_write_reg(m_cmd, base, SE_GENI_M_CMD0);
}

static inline void geni_setup_s_cmd(void __iomem *base, u32 cmd,
								u32 params)
{
	u32 s_cmd = geni_read_reg(base, SE_GENI_S_CMD0);

	s_cmd &= ~(S_OPCODE_MSK | S_PARAMS_MSK);
	s_cmd |= (cmd << S_OPCODE_SHFT);
	s_cmd |= (params & S_PARAMS_MSK);
	geni_write_reg(s_cmd, base, SE_GENI_S_CMD0);
}

static inline void geni_cancel_m_cmd(void __iomem *base)
{
	geni_write_reg(M_GENI_CMD_CANCEL, base, SE_GENI_S_CMD_CTRL_REG);
}

static inline void geni_cancel_s_cmd(void __iomem *base)
{
	geni_write_reg(S_GENI_CMD_CANCEL, base, SE_GENI_S_CMD_CTRL_REG);
}

static inline void geni_abort_m_cmd(void __iomem *base)
{
	geni_write_reg(M_GENI_CMD_ABORT, base, SE_GENI_M_CMD_CTRL_REG);
}

static inline void qcom_geni_abort_s_cmd(void __iomem *base)
{
	geni_write_reg(S_GENI_CMD_ABORT, base, SE_GENI_S_CMD_CTRL_REG);
}

static inline int get_tx_fifo_depth(void __iomem *base)
{
	int tx_fifo_depth;

	tx_fifo_depth = ((geni_read_reg(base, SE_HW_PARAM_0)
			& TX_FIFO_DEPTH_MSK) >> TX_FIFO_DEPTH_SHFT);
	return tx_fifo_depth;
}

static inline int get_tx_fifo_width(void __iomem *base)
{
	int tx_fifo_width;

	tx_fifo_width = ((geni_read_reg(base, SE_HW_PARAM_0)
			& TX_FIFO_WIDTH_MSK) >> TX_FIFO_WIDTH_SHFT);
	return tx_fifo_width;
}

static inline int get_rx_fifo_depth(void __iomem *base)
{
	int rx_fifo_depth;

	rx_fifo_depth = ((geni_read_reg(base, SE_HW_PARAM_1)
			& RX_FIFO_DEPTH_MSK) >> RX_FIFO_DEPTH_SHFT);
	return rx_fifo_depth;
}

static inline void se_config_packing(void __iomem *base, int bpw,
				int pack_words, bool msb_to_lsb)
{
	u32 cfg[4] = {0};
	unsigned long cfg0, cfg1;
	int len = ((bpw < 8) ? (bpw - 1) : 7);
	int idx = ((msb_to_lsb == 1) ? len : 0);
	int iter = (bpw * pack_words) >> 3;
	int i;

	for (i = 0; i < iter; i++) {
		cfg[i] = ((idx << 5) | (msb_to_lsb << 4) | (len << 1));
		idx += (len + 1);
		if (i == iter - 1)
			cfg[i] |= 1;
	}
	cfg0 = cfg[0] | (cfg[1] << 10);
	cfg1 = cfg[2] | (cfg[3] << 10);
	geni_write_reg(cfg0, base, SE_GENI_TX_PACKING_CFG0);
	geni_write_reg(cfg1, base, SE_GENI_TX_PACKING_CFG1);
	geni_write_reg(cfg0, base, SE_GENI_RX_PACKING_CFG0);
	geni_write_reg(cfg1, base, SE_GENI_RX_PACKING_CFG1);
}
#endif
