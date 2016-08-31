/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define DEBUG           1
#define VERBOSE_DEBUG   1

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/mipi-bif.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/mipi-bif-tegra.h>
#include <linux/pm_runtime.h>
#include <linux/clk/tegra.h>

#define TEGRA_MIPIBIF_TIMEOUT 1000

#define MIPIBIF_CTRL				0x0
#define MIPIBIF_CTRL_GO				(1<<31)
#define MIPIBIF_CTRL_COMMAND_TYPE_SHIFT		20
#define MIPIBIF_CTRL_COMMAND_NO_READ		(0x0 << MIPIBIF_CTRL_COMMAND_TYPE_SHIFT)
#define MIPIBIF_CTRL_COMMAND_READ_DATA		(0x1 << MIPIBIF_CTRL_COMMAND_TYPE_SHIFT)
#define MIPIBIF_CTRL_COMMAND_INTERRUPT_DATA	(0x2 << MIPIBIF_CTRL_COMMAND_TYPE_SHIFT)
#define MIPIBIF_CTRL_COMMAND_BUS_QUERY		(0x3 << MIPIBIF_CTRL_COMMAND_TYPE_SHIFT)
#define MIPIBIF_CTRL_COMMAND_STBY		(0x4 << MIPIBIF_CTRL_COMMAND_TYPE_SHIFT)
#define MIPIBIF_CTRL_COMMAND_PWDN		(0x5 << MIPIBIF_CTRL_COMMAND_TYPE_SHIFT)
#define MIPIBIF_CTRL_COMMAND_HARD_RESET		(0x6 << MIPIBIF_CTRL_COMMAND_TYPE_SHIFT)
#define MIPIBIF_CTRL_ACTIVATE			(0x7 << MIPIBIF_CTRL_COMMAND_TYPE_SHIFT)
#define MIPIBIF_CTRL_COMMAND_EXIT_INT		(0x8 << MIPIBIF_CTRL_COMMAND_TYPE_SHIFT)
#define MIPIBIF_CTRL_PACKETCOUNT_SHIFT		8
#define MIPIBIF_CTRL_PACKETCOUNT_MASK		(1FF << MIPIBIF_CTRL_PACKETCOUNT_SHIFT)

#define MIPIBIF_TIMING_PVT			0x4
#define MIPIBIF_TIMING_TBIF_0_SHIFT		16
#define MIPIBIF_TIMING_TBIF_1_SHIFT		12
#define MIPIBIF_TIMING_TBIF_STOP_SHIFT		8

#define MIPIBIF_TIMING0				0x8
#define MIPIBIF_TIMING0_TRESP_MIN_SHIFT		28
#define MIPIBIF_TIMING0_TRESP_MAX_SHIFT		20
#define MIPIBIF_TIMING0_TBIF_SHIFT		0

#define MIPIBIF_TIMING1				0xc
#define MIPIBIF_TIMING1_INT_TO_SHIFT		16
#define MIPIBIF_TIMING1_TINTARM_SHIFT		8
#define MIPIBIF_TIMING1_TINTTRA_SHIFT		4
#define MIPIBIF_TIMING1_TINTACT_SHIFT		0

#define MIPIBIF_TIMING2			0x10
#define MIPIBIF_TIMING2_TPDL_SHIFT	15
#define MIPIBIF_TIMING2_TPUL_SHIFT	0

#define MIPIBIF_TIMING3			0x14
#define MIPIBIF_TIMING3_TACT_SHIFT	20
#define MIPIBIF_TIMING3_TPUP_SHIFT	0

#define MIPIBIF_TIMING4			0x18
#define MIPIBIF_TIMING4_TCLWS_SHIFT	0

#define MIPIBIF_STATUS					0x2c
#define	MIPIBIF_NUM_PACKETS_TRANSMITTED_SHIFT		16
#define	MIPIBIF_NUM_PACKETS_TRANSMITTED_MASK		(0x1FF << MIPIBIF_NUM_PACKETS_TRANSMITTED_SHIFT)
#define	MIPIBIF_NUM_PACKETS_RCVD_SHIFT			4
#define	MIPIBIF_NUM_PACKETS_RCVD_MASK			(0x1FF << MIPIBIF_NUM_PACKETS_RCVD_SHIFT)
#define	MIPIBIF_CTRL_BUSY				(1<<2)
#define	MIPIBIF_INTERRUPT_RECV_STATUS			(1<<1)
#define	MIPIBIF_BQ_RECV_STATUS				(1<<0)

#define MIPIBIF_INTERRUPT_EN				0x30
#define MIPIBIF_INTERRUPT_RXF_UNR_OVR_INT_EN		(1<<10)
#define MIPIBIF_INTERRUPT_TXF_UNR_OVR_INT_EN		(1<<9)
#define MIPIBIF_INTERRUPT_NO_RESPONSE_ERR_INT_EN	(1<<8)
#define MIPIBIF_INTERRUPT_RXF_DATA_REQ_INT_EN		(1<<7)
#define MIPIBIF_INTERRUPT_TXF_DATA_REQ_INT_EN		(1<<6)
#define MIPIBIF_INTERRUPT_XFER_DONE_INT_EN		(1<<5)
#define MIPIBIF_INTERRUPT_INV_ERR_INT_EN		(1<<4)
#define MIPIBIF_INTERRUPT_PKT_RECV_ERR_INT_EN		(1<<3)
#define MIPIBIF_INTERRUPT_LOW_PHASE_IN_WORD_ERR_INT_EN	(1<<2)
#define MIPIBIF_INTERRUPT_INCOMPLETE_PKT_RECV_ERR_INT_EN	(1<<1)
#define MIPIBIF_INTERRUPT_PARITY_ERR_INT_EN		(1<<0)

#define MIPIBIF_BCL_ERROR_INTERRUPTS_EN (MIPIBIF_INTERRUPT_LOW_PHASE_IN_WORD_ERR_INT_EN | MIPIBIF_INTERRUPT_INCOMPLETE_PKT_RECV_ERR_INT_EN | MIPIBIF_INTERRUPT_NO_RESPONSE_ERR_INT_EN)

#define MIPIBIF_FIFO_ERROR_INTERRUPTS_EN (MIPIBIF_INTERRUPT_RXF_UNR_OVR_INT_EN | MIPIBIF_INTERRUPT_TXF_UNR_OVR_INT_EN)

#define MIPIBIF_DEFAULT_INTMASK (MIPIBIF_BCL_ERROR_INTERRUPTS_EN | MIPIBIF_FIFO_ERROR_INTERRUPTS_EN | MIPIBIF_INTERRUPT_XFER_DONE_INT_EN)

#define MIPIBIF_INTERRUPT_STATUS			0x34
#define MIPIBIF_INTERRUPT_NO_RESPONSE_ERR		(1<<8)
#define MIPIBIF_INTERRUPT_RXF_DATA_REQ			(1<<7)
#define MIPIBIF_INTERRUPT_TXF_DATA_REQ			(1<<6)
#define MIPIBIF_INTERRUPT_XFER_DONE			(1<<5)
#define MIPIBIF_INTERRUPT_INV_ERR			(1<<4)
#define MIPIBIF_INTERRUPT_PKT_RECV_ERR			(1<<3)
#define MIPIBIF_INTERRUPT_LOW_PHASE_IN_WORD_ERR		(1<<2)
#define MIPIBIF_INTERRUPT_INCOMPLETE_PKT_RECV_ERR	(1<<1)
#define MIPIBIF_INTERRUPT_PARITY_ERR			(1<<0)

#define MIPIBIF_TX_FIFO			0x38
#define MIPIBIF_TX_FIFO_MASK		0x3FF

#define MIPIBIF_RX_FIFO			0x3c
#define MIPIBIF_RX_FIFO_MASK		0x3FFFF

#define MIPIBIF_FIFO_CONTROL			0x40
#define MIPIBIF_RXFIFO_FLUSH			(1<<9)
#define MIPIBIF_TXFIFO_FLUSH			(1<<8)
#define MIPIBIF_RXFIFO_ATN_LVL_SHIFT		4
#define MIPIBIF_TXFIFO_ATN_LVL_SHIFT		0

#define MIPIBIF_FIFO_STATUS			0x44
#define MIPIBIF_RX_FIFO_FULL_COUNT_SHIFT	24
#define MIPIBIF_RX_FIFO_FULL_COUNT_MASK		(0xFF<<MIPIBIF_RX_FIFO_FULL_COUNT_SHIFT)
#define MIPIBIF_TX_FIFO_EMPTY_COUNT_SHIFT	16
#define MIPIBIF_TX_FIFO_EMPTY_COUNT_MASK	(0xFF << MIPIBIF_TX_FIFO_EMPTY_COUNT_SHIFT)
#define MIPIBIF_TX_FIFO_OVF			(1<<7)
#define MIPIBIF_TX_FIFO_UNR			(1<<6)
#define MIPIBIF_RX_FIFO_OVF			(1<<5)
#define MIPIBIF_RX_FIFO_UNR			(1<<4)
#define MIPIBIF_TX_FIFO_FULL			(1<<3)
#define MIPIBIF_TX_FIFO_EMPTY			(1<<2)
#define MIPIBIF_RX_FIFO_FULL			(1<<1)
#define MIPIBIF_RX_FIFO_EMPTY			(1<<0)

#define MIPIBIF_ERR_NONE			0
#define MIPIBIF_ERR_NO_RESPONSE			0x1
#define MIPIBIF_ERR_INV				0x2
#define MIPIBIF_ERR_PKT_RECV			0x4
#define MIPIBIF_ERR_LOW_PHASE_IN_WORD		0x8
#define	MIPIBIF_ERR_INCOMPLETE_PKT_RECV		0x10
#define	MIPIBIF_ERR_PARITY			0x20
#define MIPIBIF_ERR_RECV_DATA_TYPE		(MIPIBIF_ERR_PARITY | MIPIBIF_ERR_PKT_RECV | MIPIBIF_ERR_INV)

struct tegra_mipi_bif_dev {
	struct device *dev;
	struct clk *mipi_bif_clk;
	struct rt_mutex dev_lock;
	spinlock_t fifo_lock;
	void __iomem *base;
	int cont_id;
	int irq;
	int tauBIF;
	struct completion msg_complete;
	int msg_err;
	u8 *msg_buf;
	u16 msg_device_addr;
	u16 msg_reg_addr;
	u16 msg_commands;
	u16 msg_len;
	size_t msg_buf_remaining;
	int current_command_count;
	struct mipi_bif_adapter adapter;
	unsigned long bus_clk_rate;
	bool is_suspended;
};

static void tegra_mipi_bif_send(struct tegra_mipi_bif_dev *mipi_bif_dev,
	u32 cmd)
{
	writel(cmd, mipi_bif_dev->base + MIPIBIF_TX_FIFO);
	mipi_bif_dev->current_command_count++;
}

static void tegra_mipi_bif_mask_irq(struct tegra_mipi_bif_dev *mipi_bif_dev,
	u32 mask)
{
	u32 int_mask = readl(mipi_bif_dev->base + MIPIBIF_INTERRUPT_EN);
	int_mask &= ~mask;
	writel(int_mask, mipi_bif_dev->base + MIPIBIF_INTERRUPT_EN);
}

static void tegra_mipi_bif_unmask_irq(struct tegra_mipi_bif_dev *mipi_bif_dev,
	u32 mask)
{
	u32 int_mask = readl(mipi_bif_dev->base + MIPIBIF_INTERRUPT_EN);
	int_mask |= mask;
	writel(int_mask, mipi_bif_dev->base + MIPIBIF_INTERRUPT_EN);
}

static int
tegra_mipi_bif_send_DIP0_command(struct tegra_mipi_bif_dev *mipi_bif_dev)
{
	int ret;

	init_completion(&mipi_bif_dev->msg_complete);

	writel(MIPI_BIF_BUS_COMMAND_DIP0,
				mipi_bif_dev->base + MIPIBIF_TX_FIFO);
	writel(MIPIBIF_DEFAULT_INTMASK,
				mipi_bif_dev->base + MIPIBIF_INTERRUPT_EN);
	writel(MIPIBIF_CTRL_GO | MIPIBIF_CTRL_COMMAND_BUS_QUERY,
				mipi_bif_dev->base + MIPIBIF_CTRL);

	ret = wait_for_completion_timeout(&mipi_bif_dev->msg_complete,
							TEGRA_MIPIBIF_TIMEOUT);
	if (WARN_ON(ret == 0)) {
		dev_err(mipi_bif_dev->dev, "%s:transfer timed out", __func__);
		return -ETIMEDOUT;
	}

	if (readl(mipi_bif_dev->base + MIPIBIF_STATUS) & MIPIBIF_BQ_RECV_STATUS)
		return 1;

	return 0;
}

static int
tegra_mipi_bif_send_DIP1_command(struct tegra_mipi_bif_dev *mipi_bif_dev)
{
	int ret;

	init_completion(&mipi_bif_dev->msg_complete);

	writel(MIPI_BIF_BUS_COMMAND_DIP1,
				mipi_bif_dev->base + MIPIBIF_TX_FIFO);
	writel(MIPIBIF_DEFAULT_INTMASK,
				mipi_bif_dev->base + MIPIBIF_INTERRUPT_EN);
	writel(MIPIBIF_CTRL_GO | MIPIBIF_CTRL_COMMAND_BUS_QUERY,
				mipi_bif_dev->base + MIPIBIF_CTRL);

	ret = wait_for_completion_timeout(&mipi_bif_dev->msg_complete,
							TEGRA_MIPIBIF_TIMEOUT);
	if (WARN_ON(ret == 0)) {
		dev_err(mipi_bif_dev->dev, "%s:transfer timed out", __func__);
		return -ETIMEDOUT;
	}

	if (readl(mipi_bif_dev->base + MIPIBIF_STATUS) & MIPIBIF_BQ_RECV_STATUS)
		return 1;

	return 0;
}

static int
tegra_mipi_bif_send_DIE0_command(struct tegra_mipi_bif_dev *mipi_bif_dev)
{
	int ret;

	init_completion(&mipi_bif_dev->msg_complete);

	writel(MIPI_BIF_BUS_COMMAND_DIE0,
				mipi_bif_dev->base + MIPIBIF_TX_FIFO);
	writel(MIPIBIF_DEFAULT_INTMASK,
				mipi_bif_dev->base + MIPIBIF_INTERRUPT_EN);
	writel(MIPIBIF_CTRL_GO, mipi_bif_dev->base + MIPIBIF_CTRL);

	ret = wait_for_completion_timeout(&mipi_bif_dev->msg_complete,
							TEGRA_MIPIBIF_TIMEOUT);
	if (WARN_ON(ret == 0)) {
		dev_err(mipi_bif_dev->dev, "%s:transfer timed out", __func__);
		return -ETIMEDOUT;
	}
	return 0;
}

static int
tegra_mipi_bif_send_DIE1_command(struct tegra_mipi_bif_dev *mipi_bif_dev)
{
	int ret;

	init_completion(&mipi_bif_dev->msg_complete);

	writel(MIPI_BIF_BUS_COMMAND_DIE1,
				mipi_bif_dev->base + MIPIBIF_TX_FIFO);
	writel(MIPIBIF_DEFAULT_INTMASK,
				mipi_bif_dev->base + MIPIBIF_INTERRUPT_EN);
	writel(MIPIBIF_CTRL_GO, mipi_bif_dev->base + MIPIBIF_CTRL);

	ret = wait_for_completion_timeout(&mipi_bif_dev->msg_complete,
							TEGRA_MIPIBIF_TIMEOUT);
	if (WARN_ON(ret == 0)) {
		dev_err(mipi_bif_dev->dev, "%s:transfer timed out", __func__);
		return -ETIMEDOUT;
	}
	return 0;
}

static int
tegra_mipi_bif_send_DISS_command(struct tegra_mipi_bif_dev *mipi_bif_dev)
{
	int ret;

	init_completion(&mipi_bif_dev->msg_complete);

	writel(MIPI_BIF_BUS_COMMAND_DISS,
				mipi_bif_dev->base + MIPIBIF_TX_FIFO);
	writel(MIPIBIF_DEFAULT_INTMASK,
				mipi_bif_dev->base + MIPIBIF_INTERRUPT_EN);
	writel(MIPIBIF_CTRL_GO, mipi_bif_dev->base + MIPIBIF_CTRL);

	ret = wait_for_completion_timeout(&mipi_bif_dev->msg_complete,
							TEGRA_MIPIBIF_TIMEOUT);
	if (WARN_ON(ret == 0)) {
		dev_err(mipi_bif_dev->dev, "%s:transfer timed out", __func__);
		return -ETIMEDOUT;
	}
	return 0;
}

static int tegra_mipi_bif_HardReset(struct tegra_mipi_bif_dev *mipi_bif_dev)
{
	int ret;

	init_completion(&mipi_bif_dev->msg_complete);

	writel(MIPIBIF_DEFAULT_INTMASK,
				mipi_bif_dev->base + MIPIBIF_INTERRUPT_EN);
	writel(MIPIBIF_CTRL_GO | MIPIBIF_CTRL_COMMAND_HARD_RESET,
				mipi_bif_dev->base + MIPIBIF_CTRL);

	ret = wait_for_completion_timeout(&mipi_bif_dev->msg_complete,
							TEGRA_MIPIBIF_TIMEOUT);
	if (WARN_ON(ret == 0)) {
		dev_err(mipi_bif_dev->dev, "%s:transfer timed out", __func__);
		return -ETIMEDOUT;
	}
	return 0;
}

static int tegra_mipi_bif_StandBy(struct tegra_mipi_bif_dev *mipi_bif_dev)
{
	int ret;

	init_completion(&mipi_bif_dev->msg_complete);

	writel(MIPI_BIF_BUS_COMMAND_STBY,
				mipi_bif_dev->base + MIPIBIF_TX_FIFO);
	writel(MIPIBIF_DEFAULT_INTMASK,
				mipi_bif_dev->base + MIPIBIF_INTERRUPT_EN);
	writel(MIPIBIF_CTRL_GO | MIPIBIF_CTRL_COMMAND_STBY,
				mipi_bif_dev->base + MIPIBIF_CTRL);

	ret = wait_for_completion_timeout(&mipi_bif_dev->msg_complete,
							TEGRA_MIPIBIF_TIMEOUT);
	if (WARN_ON(ret == 0)) {
		dev_err(mipi_bif_dev->dev, "%s:transfer timed out", __func__);
		return -ETIMEDOUT;
	}
	return 0;
}

static int tegra_mipi_bif_Activate(struct tegra_mipi_bif_dev *mipi_bif_dev)
{
	int ret;

	init_completion(&mipi_bif_dev->msg_complete);

	writel(MIPIBIF_DEFAULT_INTMASK,
				mipi_bif_dev->base + MIPIBIF_INTERRUPT_EN);
	writel(MIPIBIF_CTRL_GO | MIPIBIF_CTRL_ACTIVATE,
				mipi_bif_dev->base + MIPIBIF_CTRL);

	ret = wait_for_completion_timeout(&mipi_bif_dev->msg_complete,
							TEGRA_MIPIBIF_TIMEOUT);
	if (WARN_ON(ret == 0)) {
		dev_err(mipi_bif_dev->dev, "%s:transfer timed out", __func__);
		return -ETIMEDOUT;
	}
	return 0;
}

static int tegra_mipi_bif_PowerDown(struct tegra_mipi_bif_dev *mipi_bif_dev)
{
	int ret;

	init_completion(&mipi_bif_dev->msg_complete);

	writel(MIPIBIF_DEFAULT_INTMASK,
				mipi_bif_dev->base + MIPIBIF_INTERRUPT_EN);
	writel(MIPI_BIF_BUS_COMMAND_PWDN,
				mipi_bif_dev->base + MIPIBIF_TX_FIFO);
	writel(MIPIBIF_CTRL_GO | MIPIBIF_CTRL_COMMAND_PWDN,
				mipi_bif_dev->base + MIPIBIF_CTRL);

	ret = wait_for_completion_timeout(&mipi_bif_dev->msg_complete,
							TEGRA_MIPIBIF_TIMEOUT);
	if (WARN_ON(ret == 0)) {
		dev_err(mipi_bif_dev->dev, "%s:transfer timed out", __func__);
		return -ETIMEDOUT;
	}
	return 0;
}

static int tegra_mipi_bif_Exit_Interrupt(
	struct tegra_mipi_bif_dev *mipi_bif_dev)
{
	int ret;
	init_completion(&mipi_bif_dev->msg_complete);

	writel(MIPIBIF_DEFAULT_INTMASK,
				mipi_bif_dev->base + MIPIBIF_INTERRUPT_EN);
	writel(MIPIBIF_CTRL_GO | MIPIBIF_CTRL_COMMAND_EXIT_INT,
				mipi_bif_dev->base + MIPIBIF_CTRL);

	ret = wait_for_completion_timeout(&mipi_bif_dev->msg_complete,
							TEGRA_MIPIBIF_TIMEOUT);
	if (WARN_ON(ret == 0)) {
		dev_err(mipi_bif_dev->dev, "%s:transfer timed out", __func__);
		return -ETIMEDOUT;
	}
	return 0;
}

static int tegra_mipi_bif_flush_fifos(struct tegra_mipi_bif_dev *mipi_bif_dev)
{
	unsigned long timeout = jiffies + HZ;

	u32 val = readl(mipi_bif_dev->base + MIPIBIF_FIFO_CONTROL);
	val |= MIPIBIF_RXFIFO_FLUSH;
	val |= MIPIBIF_TXFIFO_FLUSH;
	writel(val, mipi_bif_dev->base + MIPIBIF_FIFO_CONTROL);

	while (readl(mipi_bif_dev->base + MIPIBIF_FIFO_CONTROL) &
		(MIPIBIF_RXFIFO_FLUSH | MIPIBIF_RXFIFO_FLUSH)) {
		if (time_after(jiffies, timeout)) {
			dev_warn(mipi_bif_dev->dev, "timeout for fifo flush\n");
			return -ETIMEDOUT;
		}
		msleep(1);
	}
	return 0;
}

static int
tegra_mipi_bif_program_timings(struct tegra_mipi_bif_dev *mipi_bif_dev)
{
	u32 timing_pvt;
	u32 timing0, timing1, timing2, timing3, timing4;

	timing_pvt = 0 << MIPIBIF_TIMING_TBIF_0_SHIFT;
	timing_pvt |= 2 << MIPIBIF_TIMING_TBIF_1_SHIFT;
	timing_pvt |= 4 << MIPIBIF_TIMING_TBIF_STOP_SHIFT;
	writel(timing_pvt, mipi_bif_dev->base + MIPIBIF_TIMING_PVT);

	timing0 = 3 << MIPIBIF_TIMING0_TRESP_MIN_SHIFT;
	timing0 |= 0xe << MIPIBIF_TIMING0_TRESP_MAX_SHIFT;
	timing0 |= (mipi_bif_dev->bus_clk_rate * mipi_bif_dev->tauBIF - 1)
						<< MIPIBIF_TIMING0_TBIF_SHIFT;
	writel(timing0, mipi_bif_dev->base + MIPIBIF_TIMING0);

	timing2 = (2500 * mipi_bif_dev->bus_clk_rate - 1)
						<< MIPIBIF_TIMING2_TPDL_SHIFT;
	timing2 |= (50 * mipi_bif_dev->bus_clk_rate - 1)
						<< MIPIBIF_TIMING2_TPUL_SHIFT;
	writel(timing2, mipi_bif_dev->base + MIPIBIF_TIMING2);

	timing3 = (100 * mipi_bif_dev->bus_clk_rate - 1)
						<< MIPIBIF_TIMING3_TACT_SHIFT;
	timing3 |= (11000 * mipi_bif_dev->bus_clk_rate - 1)
						<< MIPIBIF_TIMING3_TPUP_SHIFT;
	writel(timing3, mipi_bif_dev->base + MIPIBIF_TIMING3);

	timing1 = readl(mipi_bif_dev->base + MIPIBIF_TIMING1);
	writel(timing1 | ((5000 / mipi_bif_dev->tauBIF) - 1),
					mipi_bif_dev->base + MIPIBIF_TIMING1);

	timing4 = 0xdab << MIPIBIF_TIMING4_TCLWS_SHIFT;
	writel(timing4, mipi_bif_dev->base + MIPIBIF_TIMING4);
	return 0;
}

static int tegra_mipi_bif_init(struct tegra_mipi_bif_dev *mipi_bif_dev)
{
	u32 fifo_control;

	pm_runtime_get_sync(mipi_bif_dev->dev);
	clk_prepare_enable(mipi_bif_dev->mipi_bif_clk);

	tegra_periph_reset_assert(mipi_bif_dev->mipi_bif_clk);
	udelay(2);
	tegra_periph_reset_deassert(mipi_bif_dev->mipi_bif_clk);
	clk_set_rate(mipi_bif_dev->mipi_bif_clk,
					mipi_bif_dev->bus_clk_rate * 1000000);

	writel(0, mipi_bif_dev->base + MIPIBIF_INTERRUPT_EN);
	fifo_control = 0 << MIPIBIF_RXFIFO_ATN_LVL_SHIFT;
	fifo_control |= 5 << MIPIBIF_TXFIFO_ATN_LVL_SHIFT;
	fifo_control |= MIPIBIF_RXFIFO_FLUSH;
	fifo_control |= MIPIBIF_TXFIFO_FLUSH;
	writel(fifo_control, mipi_bif_dev->base + MIPIBIF_FIFO_CONTROL);

	tegra_mipi_bif_program_timings(mipi_bif_dev);

	clk_disable_unprepare(mipi_bif_dev->mipi_bif_clk);
	pm_runtime_put(mipi_bif_dev->dev);
	return 0;
}

static int tegra_mipi_bif_empty_rx_fifo(struct tegra_mipi_bif_dev *mipi_bif_dev)
{
	u32 val;
	int rx_fifo_avail;
	u8 *buf = mipi_bif_dev->msg_buf;
	size_t buf_remaining = mipi_bif_dev->msg_buf_remaining;
	int to_be_transferred = buf_remaining;
	int ret = 0;
	val = readl(mipi_bif_dev->base + MIPIBIF_FIFO_STATUS);
	rx_fifo_avail = (val & MIPIBIF_RX_FIFO_FULL_COUNT_MASK)
					>> MIPIBIF_RX_FIFO_FULL_COUNT_SHIFT;
	if (to_be_transferred > rx_fifo_avail)
		to_be_transferred = rx_fifo_avail;

	while (to_be_transferred > 0) {
		val = readl(mipi_bif_dev->base + MIPIBIF_RX_FIFO);
		val = val >> 7;
		if (!(val & MIPI_BIF_RD_ACK_BIT)) {
			dev_warn(mipi_bif_dev->dev, "Error in data:%x\n", val);
			mipi_bif_dev->msg_err = MIPIBIF_ERR_RECV_DATA_TYPE;
			ret = -EIO;
			break;
		}
		*buf = val & 0xFF;

		rx_fifo_avail--;
		to_be_transferred--;
		buf_remaining--;
		buf++;

	}
	mipi_bif_dev->msg_buf_remaining = buf_remaining;
	mipi_bif_dev->msg_buf = buf;
	return ret;
}

static int tegra_mipi_bif_fill_tx_fifo(struct tegra_mipi_bif_dev *mipi_bif_dev)
{
	u8 *buf;
	u32 val;
	size_t buf_remaining;
	int tx_fifo_available;
	int to_be_transferred;

	buf = mipi_bif_dev->msg_buf;
	buf_remaining = mipi_bif_dev->msg_buf_remaining;
	to_be_transferred = buf_remaining;

	val = readl(mipi_bif_dev->base + MIPIBIF_FIFO_STATUS);

	tx_fifo_available = (val & MIPIBIF_TX_FIFO_EMPTY_COUNT_MASK)
					>> MIPIBIF_TX_FIFO_EMPTY_COUNT_SHIFT;

	if (tx_fifo_available > 0) {
		if (to_be_transferred > tx_fifo_available)
			to_be_transferred = tx_fifo_available;

		mipi_bif_dev->msg_buf = buf + to_be_transferred;
		mipi_bif_dev->msg_buf_remaining -= to_be_transferred;
		buf_remaining = mipi_bif_dev->msg_buf_remaining;

		while (to_be_transferred) {
			writel(*buf, mipi_bif_dev->base + MIPIBIF_TX_FIFO);
			buf++;
			to_be_transferred--;
			tx_fifo_available--;
		}
	}
	return 0;
}

static void
tegra_mipi_bif_device_detect(struct tegra_mipi_bif_dev *mipi_bif_dev, int n)
{
	int ret;
	if (!n)
		return;
	if (tegra_mipi_bif_send_DIP0_command(mipi_bif_dev)) {
		dev_dbg(mipi_bif_dev->dev, "0");
		ret = tegra_mipi_bif_send_DIE0_command(mipi_bif_dev);
		tegra_mipi_bif_device_detect(mipi_bif_dev, n - 1);
	}
	if (tegra_mipi_bif_send_DIP1_command(mipi_bif_dev)) {
		dev_dbg(mipi_bif_dev->dev, "1");
		ret = tegra_mipi_bif_send_DIE1_command(mipi_bif_dev);
		tegra_mipi_bif_device_detect(mipi_bif_dev, n - 1);
	}
}

static int
tegra_mipi_bif_xfer(struct mipi_bif_adapter *adap, struct mipi_bif_msg *msg)
{
	struct tegra_mipi_bif_dev *mipi_bif_dev = mipi_bif_get_adapdata(adap);
	int ret = 0;
	u32 sda_part;
	u32 era_part;
	u32 wra_part;
	u32 rra_part;
	u32 rbe = MIPI_BIF_BUS_COMMAND_RBE0;
	u32 rbl = MIPI_BIF_BUS_COMMAND_RBL0;
	u32 ctrl_reg = 0;

	u32 def_int_mask = MIPIBIF_DEFAULT_INTMASK;
	u32 int_mask = def_int_mask;

	rt_mutex_lock(&mipi_bif_dev->dev_lock);

	if (mipi_bif_dev->is_suspended) {
		rt_mutex_unlock(&mipi_bif_dev->dev_lock);
		return -EBUSY;
	}

	tegra_mipi_bif_init(mipi_bif_dev);
	pm_runtime_get_sync(mipi_bif_dev->dev);
	clk_prepare_enable(mipi_bif_dev->mipi_bif_clk);

	tegra_mipi_bif_flush_fifos(mipi_bif_dev);

	INIT_COMPLETION(mipi_bif_dev->msg_complete);

	mipi_bif_dev->msg_device_addr = msg->device_addr;
	mipi_bif_dev->msg_commands = msg->commands;
	mipi_bif_dev->msg_buf_remaining = 0;
	mipi_bif_dev->current_command_count = 0;
	mipi_bif_dev->msg_err = MIPIBIF_ERR_NONE;
	sda_part = msg->device_addr & 0xFF;

	if (msg->commands == MIPI_BIF_WRITE) {

		if (msg->len == 0 || (!msg->buf))
			ret = -EINVAL;

		mipi_bif_dev->msg_buf = msg->buf;
		mipi_bif_dev->msg_len = msg->len;
		mipi_bif_dev->msg_buf_remaining = msg->len;
		mipi_bif_dev->msg_reg_addr = msg->reg_addr;

		wra_part = msg->reg_addr & 0xFF;
		era_part = (msg->reg_addr & 0xFF00) >> 8;

		tegra_mipi_bif_send(mipi_bif_dev, sda_part | MIPI_BIF_SDA);
		tegra_mipi_bif_send(mipi_bif_dev, era_part | MIPI_BIF_ERA);
		tegra_mipi_bif_send(mipi_bif_dev, wra_part | MIPI_BIF_WRA);

		ctrl_reg = MIPIBIF_CTRL_COMMAND_NO_READ;
		ctrl_reg |= ((msg->len + mipi_bif_dev->current_command_count - 1)
					<< MIPIBIF_CTRL_PACKETCOUNT_SHIFT);

		tegra_mipi_bif_fill_tx_fifo(mipi_bif_dev);

		if (mipi_bif_dev->msg_buf_remaining)
			int_mask |= MIPIBIF_INTERRUPT_TXF_DATA_REQ_INT_EN;

		tegra_mipi_bif_unmask_irq(mipi_bif_dev, int_mask);

		ctrl_reg |= MIPIBIF_CTRL_GO;
		writel(ctrl_reg, mipi_bif_dev->base + MIPIBIF_CTRL);

		ret = wait_for_completion_timeout(&mipi_bif_dev->msg_complete,
							TEGRA_MIPIBIF_TIMEOUT);
		if (WARN_ON(ret == 0)) {
			dev_err(mipi_bif_dev->dev, "%s:timed out", __func__);
			ret = -ETIMEDOUT;
		}

	} else if (msg->commands == MIPI_BIF_READDATA) {

		if (msg->len == 0)
			ret = -EINVAL;

		mipi_bif_dev->msg_buf = msg->buf;
		mipi_bif_dev->msg_len = msg->len;
		mipi_bif_dev->msg_buf_remaining = msg->len;
		mipi_bif_dev->msg_reg_addr = msg->reg_addr;

		rra_part = msg->reg_addr & 0xFF;
		era_part = (msg->reg_addr & 0xFF00) >> 8;
		rbe |= ((msg->len & 0xFF00) >> 8);
		rbl |= (msg->len & 0xFF);

		if (msg->len == 256)
			rbe = rbl = 0;

		tegra_mipi_bif_send(mipi_bif_dev, sda_part | MIPI_BIF_SDA);
		tegra_mipi_bif_send(mipi_bif_dev,
					rbe | MIPI_BIF_BUS_COMMAND_RBE0);
		tegra_mipi_bif_send(mipi_bif_dev,
					rbl | MIPI_BIF_BUS_COMMAND_RBL0);
		tegra_mipi_bif_send(mipi_bif_dev, era_part | MIPI_BIF_ERA);
		tegra_mipi_bif_send(mipi_bif_dev, rra_part | MIPI_BIF_RRA);

		int_mask |= MIPIBIF_INTERRUPT_RXF_DATA_REQ_INT_EN;
		tegra_mipi_bif_unmask_irq(mipi_bif_dev, int_mask);


		ctrl_reg |= MIPIBIF_CTRL_COMMAND_READ_DATA;
		ctrl_reg |= ((mipi_bif_dev->current_command_count - 1)
					<< MIPIBIF_CTRL_PACKETCOUNT_SHIFT);
		ctrl_reg |= MIPIBIF_CTRL_GO;
		writel(ctrl_reg, mipi_bif_dev->base + MIPIBIF_CTRL);

		ret = wait_for_completion_timeout(&mipi_bif_dev->msg_complete,
							TEGRA_MIPIBIF_TIMEOUT);
		if (WARN_ON(ret == 0)) {
			dev_err(mipi_bif_dev->dev, "%s:timed out", __func__);
			ret = -ETIMEDOUT;
		}

	} else if (msg->commands == MIPI_BIF_INT_READ) {

		tegra_mipi_bif_send(mipi_bif_dev, sda_part | MIPI_BIF_SDA);
		tegra_mipi_bif_send(mipi_bif_dev, MIPI_BIF_BUS_COMMAND_EINT);

		tegra_mipi_bif_unmask_irq(mipi_bif_dev, int_mask);

		ctrl_reg = MIPIBIF_CTRL_COMMAND_INTERRUPT_DATA;
		ctrl_reg |= ((mipi_bif_dev->current_command_count - 1)
					<< MIPIBIF_CTRL_PACKETCOUNT_SHIFT);
		ctrl_reg |= MIPIBIF_CTRL_GO;
		writel(ctrl_reg, mipi_bif_dev->base + MIPIBIF_CTRL);

		ret = wait_for_completion_timeout(&mipi_bif_dev->msg_complete,
							TEGRA_MIPIBIF_TIMEOUT);
		if (WARN_ON(ret == 0)) {
			dev_err(mipi_bif_dev->dev, "%s:timed out", __func__);
			ret = -ETIMEDOUT;
		}

		if (!(readl(mipi_bif_dev->base + MIPIBIF_STATUS)
					& MIPIBIF_INTERRUPT_RECV_STATUS)) {
			dev_err(mipi_bif_dev->dev, "%s:no interrupt", __func__);
			ret = -EIO;
		}

	} else if (msg->commands == MIPI_BIF_STDBY) {
		ret = tegra_mipi_bif_StandBy(mipi_bif_dev);
	} else if (msg->commands == MIPI_BIF_PWRDOWN) {
		ret = tegra_mipi_bif_PowerDown(mipi_bif_dev);
	} else if (msg->commands == MIPI_BIF_ACTIVATE) {
		ret = tegra_mipi_bif_Activate(mipi_bif_dev);
	} else if (msg->commands == MIPI_BIF_INT_EXIT) {
		ret = tegra_mipi_bif_Exit_Interrupt(mipi_bif_dev);
	} else if (msg->commands == MIPI_BIF_HARD_RESET) {
		ret = tegra_mipi_bif_HardReset(mipi_bif_dev);
	}/* else if (msg->commands == MIPI_BIF_BUSQUERY) {
		ret = tegra_mipi_bif_send_DISS_command(mipi_bif_dev);
		tegra_mipi_bif_device_detect(mipi_bif_dev, 80);
	}*/

	tegra_mipi_bif_mask_irq(mipi_bif_dev, int_mask);

	if (mipi_bif_dev->msg_err & MIPIBIF_ERR_RECV_DATA_TYPE)
		ret = -EAGAIN;
	else if (mipi_bif_dev->msg_err != MIPIBIF_ERR_NONE)
		ret = -EIO;

	clk_disable_unprepare(mipi_bif_dev->mipi_bif_clk);
	pm_runtime_put(mipi_bif_dev->dev);
	rt_mutex_unlock(&mipi_bif_dev->dev_lock);
	return ret;
}

static const struct mipi_bif_algorithm tegra_mipi_bif_algo = {
	.master_xfer	= tegra_mipi_bif_xfer,
};

static irqreturn_t tegra_mipi_bif_isr(int irq, void *dev_id)
{
	u32 status;
	int ret;
	struct tegra_mipi_bif_dev *mipi_bif_dev = dev_id;
	status = readl(mipi_bif_dev->base +  MIPIBIF_INTERRUPT_STATUS);

	if (status == 0) {
		dev_warn(mipi_bif_dev->dev, "Unknown interrupt\n");
		goto err;
	}

	if (status & MIPIBIF_INTERRUPT_NO_RESPONSE_ERR) {
		mipi_bif_dev->msg_err = MIPIBIF_ERR_NO_RESPONSE;
		dev_warn(mipi_bif_dev->dev,
			"error: No response from slave within tresp\n");
		goto err;
	}

	if (status & MIPIBIF_INTERRUPT_INV_ERR) {
		mipi_bif_dev->msg_err = MIPIBIF_ERR_INV;
		dev_warn(mipi_bif_dev->dev,
			"error: Incorrect inversion received\n");
		goto err;
	}

	if (status & MIPIBIF_INTERRUPT_PKT_RECV_ERR) {
		mipi_bif_dev->msg_err = MIPIBIF_ERR_PKT_RECV;
		dev_warn(mipi_bif_dev->dev,
			"error: Incorrect ack received\n");
		goto err;
	}

	if (status & MIPIBIF_INTERRUPT_LOW_PHASE_IN_WORD_ERR) {
		mipi_bif_dev->msg_err = MIPIBIF_ERR_LOW_PHASE_IN_WORD;
		dev_warn(mipi_bif_dev->dev,
			"error: Low phase in word err received\n");
		goto err;
	}

	if (status & MIPIBIF_INTERRUPT_INCOMPLETE_PKT_RECV_ERR) {
		mipi_bif_dev->msg_err = MIPIBIF_ERR_INCOMPLETE_PKT_RECV;
		dev_warn(mipi_bif_dev->dev,
			"error: Incomplete packet received\n");
		goto err;
	}

	if (status & MIPIBIF_INTERRUPT_PARITY_ERR) {
		mipi_bif_dev->msg_err = MIPIBIF_ERR_PARITY;
		dev_warn(mipi_bif_dev->dev,
			"error: Incorrect parity received\n");
		goto err;
	}

	if ((mipi_bif_dev->msg_commands == MIPI_BIF_WRITE)
				&& (status & MIPIBIF_INTERRUPT_TXF_DATA_REQ)) {
		if (mipi_bif_dev->msg_buf_remaining)
			tegra_mipi_bif_fill_tx_fifo(mipi_bif_dev);
		else
			tegra_mipi_bif_mask_irq(mipi_bif_dev,
					MIPIBIF_INTERRUPT_TXF_DATA_REQ_INT_EN);
	}

	if ((mipi_bif_dev->msg_commands == MIPI_BIF_READDATA)
				&& (status & MIPIBIF_INTERRUPT_RXF_DATA_REQ)) {
		if (mipi_bif_dev->msg_buf_remaining) {
			ret = tegra_mipi_bif_empty_rx_fifo(mipi_bif_dev);
			if (ret)
				goto err;
		} else {
			tegra_mipi_bif_mask_irq(mipi_bif_dev,
					MIPIBIF_INTERRUPT_RXF_DATA_REQ_INT_EN);
		}
	}

	if (status & MIPIBIF_INTERRUPT_XFER_DONE) {
		WARN_ON(mipi_bif_dev->msg_buf_remaining);
		complete(&mipi_bif_dev->msg_complete);
	}
	writel(status, mipi_bif_dev->base + MIPIBIF_INTERRUPT_STATUS);
	return IRQ_HANDLED;

err:
	writel(status, mipi_bif_dev->base + MIPIBIF_INTERRUPT_STATUS);
	complete(&mipi_bif_dev->msg_complete);
	return IRQ_HANDLED;
}

static int tegra_mipi_bif_remove(struct platform_device *pdev)
{
	pm_runtime_disable(&pdev->dev);
	return 0;
}
static int tegra_mipi_bif_probe(struct platform_device *pdev)
{
	struct tegra_mipi_bif_dev *mipi_bif_dev;
	struct tegra_mipi_bif_platform_data *plat = pdev->dev.platform_data;
	struct resource *res;
	struct clk *mipi_bif_clk;
	void __iomem *base;
	int irq;
	int ret;

	if (!plat) {
		dev_err(&pdev->dev, "no platform data?\n");
		return -ENODEV;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "no mem resource\n");
		return -EINVAL;
	}

	base = devm_request_and_ioremap(&pdev->dev, res);
	if (!base) {
		dev_err(&pdev->dev, "Cannot request/ioremap MIPIBIF regs\n");
		return -EADDRNOTAVAIL;
	}

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		dev_err(&pdev->dev, "no irq resource\n");
		return -EINVAL;
	}
	irq = res->start;

	mipi_bif_clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(mipi_bif_clk)) {
		dev_err(&pdev->dev, "missing mipi bif controller clock\n");
		return PTR_ERR(mipi_bif_clk);
	}

	mipi_bif_dev = devm_kzalloc(&pdev->dev,
				sizeof(struct tegra_mipi_bif_dev), GFP_KERNEL);
	if (!mipi_bif_dev) {
		dev_err(&pdev->dev, "Failed to allocate memory\n");
		return -ENOMEM;
	}

	mipi_bif_dev->bus_clk_rate = plat->bus_clk_rate;
	mipi_bif_dev->tauBIF = plat->tauBIF;
	mipi_bif_dev->base = base;
	mipi_bif_dev->mipi_bif_clk = mipi_bif_clk;
	mipi_bif_dev->irq = irq;
	mipi_bif_dev->cont_id = pdev->id;
	mipi_bif_dev->dev = &pdev->dev;

	ret = devm_request_irq(&pdev->dev, mipi_bif_dev->irq,
			tegra_mipi_bif_isr, 0, pdev->name, mipi_bif_dev);
	if (ret) {
		dev_err(&pdev->dev, "Failed to request irq %i\n",
							mipi_bif_dev->irq);
		return ret;
	}

	pm_runtime_enable(&pdev->dev);

	platform_set_drvdata(pdev, mipi_bif_dev);

	rt_mutex_init(&mipi_bif_dev->dev_lock);
	spin_lock_init(&mipi_bif_dev->fifo_lock);
	init_completion(&mipi_bif_dev->msg_complete);

	mipi_bif_dev->adapter.algo = &tegra_mipi_bif_algo;
	strlcpy(mipi_bif_dev->adapter.name, "Tegra MIPIBIF adapter",
			sizeof(mipi_bif_dev->adapter.name));

	mipi_bif_dev->adapter.dev.parent = &pdev->dev;
	mipi_bif_dev->adapter.nr = plat->adapter_nr;
	mipi_bif_set_adapdata(&mipi_bif_dev->adapter, mipi_bif_dev);

	ret = mipi_bif_add_numbered_adapter(&mipi_bif_dev->adapter);
	if (ret) {
		dev_err(&pdev->dev, "Failed to add mipi_bif adapter\n");
		return ret;
	}
	return 0;
}

static int tegra_mipi_bif_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct tegra_mipi_bif_dev *mipi_bif_dev = platform_get_drvdata(pdev);

	rt_mutex_lock(&mipi_bif_dev->dev_lock);

	mipi_bif_dev->is_suspended = false;

	rt_mutex_unlock(&mipi_bif_dev->dev_lock);

	return 0;
}

static int tegra_mipi_bif_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct tegra_mipi_bif_dev *mipi_bif_dev = platform_get_drvdata(pdev);

	rt_mutex_lock(&mipi_bif_dev->dev_lock);

	mipi_bif_dev->is_suspended = true;

	rt_mutex_unlock(&mipi_bif_dev->dev_lock);

	return 0;
}

static const struct dev_pm_ops tegra_mipi_bif_pm = {
	.suspend = tegra_mipi_bif_suspend,
	.resume = tegra_mipi_bif_resume,
};

static struct platform_driver tegra_mipi_bif_driver = {
	.probe   = tegra_mipi_bif_probe,
	.remove  = tegra_mipi_bif_remove,
	.driver  = {
		.name  = "tegra-mipi-bif",
		.owner = THIS_MODULE,
		.pm    = &tegra_mipi_bif_pm
	},
};

static int __init tegra_mipi_bif_init_driver(void)
{
	return platform_driver_register(&tegra_mipi_bif_driver);
}

static void __exit tegra_mipi_bif_exit_driver(void)
{
	platform_driver_unregister(&tegra_mipi_bif_driver);
}

subsys_initcall(tegra_mipi_bif_init_driver);
module_exit(tegra_mipi_bif_exit_driver);

MODULE_DESCRIPTION("nVidia Tegra MIPI BIF Controller driver");
MODULE_AUTHOR("Chaitanya Bandi");
MODULE_LICENSE("GPL v2");
