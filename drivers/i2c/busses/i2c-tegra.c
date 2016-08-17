/*
 * drivers/i2c/busses/i2c-tegra.c
 *
 * Copyright (C) 2010 Google, Inc.
 * Author: Colin Cross <ccross@android.com>
 *
 * Copyright (C) 2010-2013 NVIDIA CORPORATION. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/*#define DEBUG           1*/
/*#define VERBOSE_DEBUG   1*/

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/i2c-tegra.h>
#include <linux/of_device.h>
#include <linux/of_i2c.h>
#include <linux/module.h>
#include <linux/spinlock.h>

#include <asm/unaligned.h>

#include <mach/clk.h>
#include <mach/pinmux.h>

#define TEGRA_I2C_TIMEOUT			(msecs_to_jiffies(1000))
#define TEGRA_I2C_RETRIES			3
#define BYTES_PER_FIFO_WORD			4

#define I2C_CNFG				0x000
#define I2C_CNFG_DEBOUNCE_CNT_SHIFT		12
#define I2C_CNFG_PACKET_MODE_EN			(1<<10)
#define I2C_CNFG_NEW_MASTER_FSM			(1<<11)
#define I2C_STATUS				0x01C
#define I2C_STATUS_BUSY				(1<<8)
#define I2C_SL_CNFG				0x020
#define I2C_SL_CNFG_NACK			(1<<1)
#define I2C_SL_CNFG_NEWSL			(1<<2)
#define I2C_SL_ADDR1				0x02c
#define I2C_SL_ADDR2				0x030
#define I2C_TX_FIFO				0x050
#define I2C_RX_FIFO				0x054
#define I2C_PACKET_TRANSFER_STATUS		0x058
#define I2C_FIFO_CONTROL			0x05c
#define I2C_FIFO_CONTROL_TX_FLUSH		(1<<1)
#define I2C_FIFO_CONTROL_RX_FLUSH		(1<<0)
#define I2C_FIFO_CONTROL_TX_TRIG_SHIFT		5
#define I2C_FIFO_CONTROL_RX_TRIG_SHIFT		2
#define I2C_FIFO_STATUS				0x060
#define I2C_FIFO_STATUS_TX_MASK			0xF0
#define I2C_FIFO_STATUS_TX_SHIFT		4
#define I2C_FIFO_STATUS_RX_MASK			0x0F
#define I2C_FIFO_STATUS_RX_SHIFT		0
#define I2C_INT_MASK				0x064
#define I2C_INT_STATUS				0x068
#define I2C_INT_BUS_CLEAR_DONE		(1<<11)
#define I2C_INT_PACKET_XFER_COMPLETE		(1<<7)
#define I2C_INT_ALL_PACKETS_XFER_COMPLETE	(1<<6)
#define I2C_INT_TX_FIFO_OVERFLOW		(1<<5)
#define I2C_INT_RX_FIFO_UNDERFLOW		(1<<4)
#define I2C_INT_NO_ACK				(1<<3)
#define I2C_INT_ARBITRATION_LOST		(1<<2)
#define I2C_INT_TX_FIFO_DATA_REQ		(1<<1)
#define I2C_INT_RX_FIFO_DATA_REQ		(1<<0)

#define I2C_CLK_DIVISOR				0x06c
#define I2C_CLK_DIVISOR_STD_FAST_MODE_SHIFT	16
#define I2C_CLK_MULTIPLIER_STD_FAST_MODE	8

#define DVC_CTRL_REG1				0x000
#define DVC_CTRL_REG1_INTR_EN			(1<<10)
#define DVC_CTRL_REG2				0x004
#define DVC_CTRL_REG3				0x008
#define DVC_CTRL_REG3_SW_PROG			(1<<26)
#define DVC_CTRL_REG3_I2C_DONE_INTR_EN		(1<<30)
#define DVC_STATUS				0x00c
#define DVC_STATUS_I2C_DONE_INTR		(1<<30)

#define I2C_ERR_NONE				0x00
#define I2C_ERR_NO_ACK				0x01
#define I2C_ERR_ARBITRATION_LOST		0x02
#define I2C_ERR_UNKNOWN_INTERRUPT		0x04
#define I2C_ERR_UNEXPECTED_STATUS		0x08

#define PACKET_HEADER0_HEADER_SIZE_SHIFT	28
#define PACKET_HEADER0_PACKET_ID_SHIFT		16
#define PACKET_HEADER0_CONT_ID_SHIFT		12
#define PACKET_HEADER0_PROTOCOL_I2C		(1<<4)

#define I2C_HEADER_HIGHSPEED_MODE		(1<<22)
#define I2C_HEADER_CONT_ON_NAK			(1<<21)
#define I2C_HEADER_SEND_START_BYTE		(1<<20)
#define I2C_HEADER_READ				(1<<19)
#define I2C_HEADER_10BIT_ADDR			(1<<18)
#define I2C_HEADER_IE_ENABLE			(1<<17)
#define I2C_HEADER_REPEAT_START			(1<<16)
#define I2C_HEADER_CONTINUE_XFER		(1<<15)
#define I2C_HEADER_MASTER_ADDR_SHIFT		12
#define I2C_HEADER_SLAVE_ADDR_SHIFT		1

#define I2C_BUS_CLEAR_CNFG				0x084
#define I2C_BC_SCLK_THRESHOLD				(9<<16)
#define I2C_BC_STOP_COND				(1<<2)
#define I2C_BC_TERMINATE				(1<<1)
#define I2C_BC_ENABLE					(1<<0)

#define I2C_BUS_CLEAR_STATUS				0x088
#define I2C_BC_STATUS					(1<<0)

#define SL_ADDR1(addr) (addr & 0xff)
#define SL_ADDR2(addr) ((addr >> 8) & 0xff)

/*
 * msg_end_type: The bus control which need to be send at end of transfer.
 * @MSG_END_STOP: Send stop pulse at end of transfer.
 * @MSG_END_REPEAT_START: Send repeat start at end of transfer.
 * @MSG_END_CONTINUE: The following on message is coming and so do not send
 *		stop or repeat start.
 */

enum msg_end_type {
	MSG_END_STOP,
	MSG_END_REPEAT_START,
	MSG_END_CONTINUE,
};

struct tegra_i2c_chipdata {
	bool timeout_irq_occurs_before_bus_inactive;
	bool has_xfer_complete_interrupt;
	bool has_hw_arb_support;
	bool has_fast_clock;
	bool has_clk_divisor_std_fast_mode;
	bool has_continue_xfer_support;
	u16 clk_divisor_std_fast_mode;
	u16 clk_divisor_hs_mode;
	int clk_multiplier_hs_mode;
};

struct tegra_i2c_dev;

struct tegra_i2c_bus {
	struct tegra_i2c_dev *dev;
	const struct tegra_pingroup_config *mux;
	int mux_len;
	unsigned long bus_clk_rate;
	struct i2c_adapter adapter;
	int scl_gpio;
	int sda_gpio;
};

/**
 * struct tegra_i2c_dev	- per device i2c context
 * @dev: device reference for power management
 * @adapter: core i2c layer adapter information
 * @clk: clock reference for i2c controller
 * @i2c_clk: clock reference for i2c bus
 * @base: ioremapped registers cookie
 * @cont_id: i2c controller id, used for for packet header
 * @irq: irq number of transfer complete interrupt
 * @is_dvc: identifies the DVC i2c controller, has a different register layout
 * @msg_complete: transfer completion notifier
 * @msg_err: error code for completed message
 * @msg_buf: pointer to current message data
 * @msg_buf_remaining: size of unsent data in the message buffer
 * @msg_read: identifies read transfers
 * @bus_clk_rate: current i2c bus clock rate
 * @is_suspended: prevents i2c controller accesses after suspend is called
 */
struct tegra_i2c_dev {
	struct device *dev;
	struct clk *div_clk;
	struct clk *fast_clk;
	struct rt_mutex dev_lock;
	spinlock_t fifo_lock;
	void __iomem *base;
	int cont_id;
	int irq;
	bool irq_disabled;
	int is_dvc;
	struct completion msg_complete;
	int msg_err;
	int next_msg_err;
	u8 *msg_buf;
	u8 *next_msg_buf;
	u32 packet_header;
	u32 next_packet_header;
	u32 payload_size;
	u32 next_payload_size;
	u32 io_header;
	u32 next_io_header;
	size_t msg_buf_remaining;
	size_t next_msg_buf_remaining;
	int msg_read;
	int next_msg_read;
	struct i2c_msg *msgs;
	int msg_add;
	int next_msg_add;
	int msgs_num;
	bool is_suspended;
	int bus_count;
	const struct tegra_pingroup_config *last_mux;
	int last_mux_len;
	unsigned long last_bus_clk_rate;
	u16 slave_addr;
	bool is_clkon_always;
	bool is_high_speed_enable;
	u16 hs_master_code;
	bool use_single_xfer_complete;
	int (*arb_recovery)(int scl_gpio, int sda_gpio);
	struct tegra_i2c_chipdata *chipdata;
	struct tegra_i2c_bus busses[1];
};

static void dvc_writel(struct tegra_i2c_dev *i2c_dev, u32 val, unsigned long reg)
{
	writel(val, i2c_dev->base + reg);
}

static u32 dvc_readl(struct tegra_i2c_dev *i2c_dev, unsigned long reg)
{
	return readl(i2c_dev->base + reg);
}

static void dvc_i2c_mask_irq(struct tegra_i2c_dev *i2c_dev, u32 mask)
{
	u32 int_mask = dvc_readl(i2c_dev, DVC_CTRL_REG3);
	int_mask &= ~mask;
	dvc_writel(i2c_dev, int_mask, DVC_CTRL_REG3);
}

static void dvc_i2c_unmask_irq(struct tegra_i2c_dev *i2c_dev, u32 mask)
{
	u32 int_mask = dvc_readl(i2c_dev, DVC_CTRL_REG3);
	int_mask |= mask;
	dvc_writel(i2c_dev, int_mask, DVC_CTRL_REG3);
}

/*
 * i2c_writel and i2c_readl will offset the register if necessary to talk
 * to the I2C block inside the DVC block
 */
static unsigned long tegra_i2c_reg_addr(struct tegra_i2c_dev *i2c_dev,
	unsigned long reg)
{
	if (i2c_dev->is_dvc)
		reg += (reg >= I2C_TX_FIFO) ? 0x10 : 0x40;
	return reg;
}

static void i2c_writel(struct tegra_i2c_dev *i2c_dev, u32 val,
	unsigned long reg)
{
	writel(val, i2c_dev->base + tegra_i2c_reg_addr(i2c_dev, reg));

	/* Read back register to make sure that register writes completed */
	if (reg != I2C_TX_FIFO)
		readl(i2c_dev->base + tegra_i2c_reg_addr(i2c_dev, reg));
}

static u32 i2c_readl(struct tegra_i2c_dev *i2c_dev, unsigned long reg)
{
	return readl(i2c_dev->base + tegra_i2c_reg_addr(i2c_dev, reg));
}

static void i2c_writesl(struct tegra_i2c_dev *i2c_dev, void *data,
	unsigned long reg, int len)
{
	writesl(i2c_dev->base + tegra_i2c_reg_addr(i2c_dev, reg), data, len);
}

static void i2c_readsl(struct tegra_i2c_dev *i2c_dev, void *data,
	unsigned long reg, int len)
{
	readsl(i2c_dev->base + tegra_i2c_reg_addr(i2c_dev, reg), data, len);
}

static void tegra_i2c_mask_irq(struct tegra_i2c_dev *i2c_dev, u32 mask)
{
	u32 int_mask = i2c_readl(i2c_dev, I2C_INT_MASK);
	int_mask &= ~mask;
	i2c_writel(i2c_dev, int_mask, I2C_INT_MASK);
}

static void tegra_i2c_unmask_irq(struct tegra_i2c_dev *i2c_dev, u32 mask)
{
	u32 int_mask = i2c_readl(i2c_dev, I2C_INT_MASK);
	int_mask |= mask;
	i2c_writel(i2c_dev, int_mask, I2C_INT_MASK);
}

static int tegra_i2c_flush_fifos(struct tegra_i2c_dev *i2c_dev)
{
	unsigned long timeout = jiffies + HZ;
	u32 val = i2c_readl(i2c_dev, I2C_FIFO_CONTROL);
	val |= I2C_FIFO_CONTROL_TX_FLUSH | I2C_FIFO_CONTROL_RX_FLUSH;
	i2c_writel(i2c_dev, val, I2C_FIFO_CONTROL);

	while (i2c_readl(i2c_dev, I2C_FIFO_CONTROL) &
		(I2C_FIFO_CONTROL_TX_FLUSH | I2C_FIFO_CONTROL_RX_FLUSH)) {
		if (time_after(jiffies, timeout)) {
			dev_warn(i2c_dev->dev, "timeout waiting for fifo flush\n");
			return -ETIMEDOUT;
		}
		msleep(1);
	}
	return 0;
}

static int tegra_i2c_empty_rx_fifo(struct tegra_i2c_dev *i2c_dev)
{
	u32 val;
	int rx_fifo_avail;
	u8 *buf = i2c_dev->msg_buf;
	size_t buf_remaining = i2c_dev->msg_buf_remaining;
	int words_to_transfer;

	val = i2c_readl(i2c_dev, I2C_FIFO_STATUS);
	rx_fifo_avail = (val & I2C_FIFO_STATUS_RX_MASK) >>
		I2C_FIFO_STATUS_RX_SHIFT;

	/* Rounds down to not include partial word at the end of buf */
	words_to_transfer = buf_remaining / BYTES_PER_FIFO_WORD;
	if (words_to_transfer > rx_fifo_avail)
		words_to_transfer = rx_fifo_avail;

	i2c_readsl(i2c_dev, buf, I2C_RX_FIFO, words_to_transfer);

	buf += words_to_transfer * BYTES_PER_FIFO_WORD;
	buf_remaining -= words_to_transfer * BYTES_PER_FIFO_WORD;
	rx_fifo_avail -= words_to_transfer;

	/*
	 * If there is a partial word at the end of buf, handle it manually to
	 * prevent overwriting past the end of buf
	 */
	if (rx_fifo_avail > 0 && buf_remaining > 0) {
		BUG_ON(buf_remaining > 3);
		val = i2c_readl(i2c_dev, I2C_RX_FIFO);
		memcpy(buf, &val, buf_remaining);
		buf_remaining = 0;
		rx_fifo_avail--;
	}

	BUG_ON(rx_fifo_avail > 0 && buf_remaining > 0);
	i2c_dev->msg_buf_remaining = buf_remaining;
	i2c_dev->msg_buf = buf;
	return 0;
}

static int tegra_i2c_fill_tx_fifo(struct tegra_i2c_dev *i2c_dev)
{
	u32 val;
	int tx_fifo_avail;
	u8 *buf;
	size_t buf_remaining;
	int words_to_transfer;

	if (!i2c_dev->msg_buf_remaining)
		return 0;
	buf = i2c_dev->msg_buf;
	buf_remaining = i2c_dev->msg_buf_remaining;

	val = i2c_readl(i2c_dev, I2C_FIFO_STATUS);
	tx_fifo_avail = (val & I2C_FIFO_STATUS_TX_MASK) >>
		I2C_FIFO_STATUS_TX_SHIFT;

	/* Rounds down to not include partial word at the end of buf */
	words_to_transfer = buf_remaining / BYTES_PER_FIFO_WORD;

	/* It's very common to have < 4 bytes, so optimize that case. */
	if (words_to_transfer) {
		if (words_to_transfer > tx_fifo_avail)
			words_to_transfer = tx_fifo_avail;

		/*
		 * Update state before writing to FIFO.  If this casues us
		 * to finish writing all bytes (AKA buf_remaining goes to 0) we
		 * have a potential for an interrupt (PACKET_XFER_COMPLETE is
		 * not maskable).  We need to make sure that the isr sees
		 * buf_remaining as 0 and doesn't call us back re-entrantly.
		 */
		buf_remaining -= words_to_transfer * BYTES_PER_FIFO_WORD;
		tx_fifo_avail -= words_to_transfer;
		i2c_dev->msg_buf_remaining = buf_remaining;
		i2c_dev->msg_buf = buf +
			words_to_transfer * BYTES_PER_FIFO_WORD;
		barrier();

		i2c_writesl(i2c_dev, buf, I2C_TX_FIFO, words_to_transfer);

		buf += words_to_transfer * BYTES_PER_FIFO_WORD;
	}

	/*
	 * If there is a partial word at the end of buf, handle it manually to
	 * prevent reading past the end of buf, which could cross a page
	 * boundary and fault.
	 */
	if (tx_fifo_avail > 0 && buf_remaining > 0) {
		if (buf_remaining > 3) {
			dev_err(i2c_dev->dev,
				"Remaining buffer more than 3 %d\n",
				buf_remaining);
			BUG();
		}
		memcpy(&val, buf, buf_remaining);

		/* Again update before writing to FIFO to make sure isr sees. */
		i2c_dev->msg_buf_remaining = 0;
		i2c_dev->msg_buf = NULL;
		barrier();

		i2c_writel(i2c_dev, val, I2C_TX_FIFO);
	}

	return 0;
}

/*
 * One of the Tegra I2C blocks is inside the DVC (Digital Voltage Controller)
 * block.  This block is identical to the rest of the I2C blocks, except that
 * it only supports master mode, it has registers moved around, and it needs
 * some extra init to get it into I2C mode.  The register moves are handled
 * by i2c_readl and i2c_writel
 */
static void tegra_dvc_init(struct tegra_i2c_dev *i2c_dev)
{
	u32 val = 0;
	val = dvc_readl(i2c_dev, DVC_CTRL_REG3);
	val |= DVC_CTRL_REG3_SW_PROG;
	dvc_writel(i2c_dev, val, DVC_CTRL_REG3);

	val = dvc_readl(i2c_dev, DVC_CTRL_REG1);
	val |= DVC_CTRL_REG1_INTR_EN;
	dvc_writel(i2c_dev, val, DVC_CTRL_REG1);
}

static void tegra_i2c_slave_init(struct tegra_i2c_dev *i2c_dev)
{
	u32 val = I2C_SL_CNFG_NEWSL | I2C_SL_CNFG_NACK;

	i2c_writel(i2c_dev, val, I2C_SL_CNFG);

	if (i2c_dev->slave_addr) {
		u16 addr = i2c_dev->slave_addr;

		i2c_writel(i2c_dev, SL_ADDR1(addr), I2C_SL_ADDR1);
		i2c_writel(i2c_dev, SL_ADDR2(addr), I2C_SL_ADDR2);
	}
}

static inline int tegra_i2c_clock_enable(struct tegra_i2c_dev *i2c_dev)
{
	int ret;
	if (i2c_dev->chipdata->has_fast_clock) {
		ret = clk_prepare_enable(i2c_dev->fast_clk);
		if (ret < 0) {
			dev_err(i2c_dev->dev,
				"Error in enabling fast clock err %d\n", ret);
			return ret;
		}
	}
	ret = clk_prepare_enable(i2c_dev->div_clk);
	if (ret < 0) {
		dev_err(i2c_dev->dev,
			"Error in enabling div clock err %d\n", ret);
		clk_disable_unprepare(i2c_dev->fast_clk);
	}
	return ret;
}

static inline void tegra_i2c_clock_disable(struct tegra_i2c_dev *i2c_dev)
{
	clk_disable_unprepare(i2c_dev->div_clk);
	if (i2c_dev->chipdata->has_fast_clock)
		clk_disable_unprepare(i2c_dev->fast_clk);
}

static void tegra_i2c_set_clk_rate(struct tegra_i2c_dev *i2c_dev)
{
	u32 clk_multiplier;
	if (i2c_dev->is_high_speed_enable)
		clk_multiplier = i2c_dev->chipdata->clk_multiplier_hs_mode
			* (i2c_dev->chipdata->clk_divisor_hs_mode + 1);
	else
		clk_multiplier = I2C_CLK_MULTIPLIER_STD_FAST_MODE
		* (i2c_dev->chipdata->clk_divisor_std_fast_mode + 1);

	clk_set_rate(i2c_dev->div_clk, i2c_dev->last_bus_clk_rate
							* clk_multiplier);
}

static int tegra_i2c_init(struct tegra_i2c_dev *i2c_dev)
{
	u32 val;
	int err = 0;
	u32 clk_divisor = 0;

	tegra_i2c_clock_enable(i2c_dev);

	tegra_periph_reset_assert(i2c_dev->div_clk);
	udelay(2);
	tegra_periph_reset_deassert(i2c_dev->div_clk);

	if (i2c_dev->is_dvc)
		tegra_dvc_init(i2c_dev);

	val = I2C_CNFG_NEW_MASTER_FSM | I2C_CNFG_PACKET_MODE_EN |
		(0x2 << I2C_CNFG_DEBOUNCE_CNT_SHIFT);
	i2c_writel(i2c_dev, val, I2C_CNFG);
	i2c_writel(i2c_dev, 0, I2C_INT_MASK);

	tegra_i2c_set_clk_rate(i2c_dev);

	clk_divisor |= i2c_dev->chipdata->clk_divisor_hs_mode;
	if (i2c_dev->chipdata->has_clk_divisor_std_fast_mode)
		clk_divisor |= i2c_dev->chipdata->clk_divisor_std_fast_mode
				<< I2C_CLK_DIVISOR_STD_FAST_MODE_SHIFT;
	i2c_writel(i2c_dev, clk_divisor, I2C_CLK_DIVISOR);

	if (!i2c_dev->is_dvc) {
		u32 sl_cfg = i2c_readl(i2c_dev, I2C_SL_CNFG);
		sl_cfg |= I2C_SL_CNFG_NACK | I2C_SL_CNFG_NEWSL;
		i2c_writel(i2c_dev, sl_cfg, I2C_SL_CNFG);
		i2c_writel(i2c_dev, 0xfc, I2C_SL_ADDR1);
		i2c_writel(i2c_dev, 0x00, I2C_SL_ADDR2);

	}

	val = 7 << I2C_FIFO_CONTROL_TX_TRIG_SHIFT |
		0 << I2C_FIFO_CONTROL_RX_TRIG_SHIFT;
	i2c_writel(i2c_dev, val, I2C_FIFO_CONTROL);

	if (!i2c_dev->is_dvc)
		tegra_i2c_slave_init(i2c_dev);

	if (tegra_i2c_flush_fifos(i2c_dev))
		err = -ETIMEDOUT;

	tegra_i2c_clock_disable(i2c_dev);

	if (i2c_dev->irq_disabled) {
		i2c_dev->irq_disabled = 0;
		enable_irq(i2c_dev->irq);
	}

	return err;
}
static int tegra_i2c_copy_next_to_current(struct tegra_i2c_dev *i2c_dev)
{
	i2c_dev->msg_buf = i2c_dev->next_msg_buf;
	i2c_dev->msg_buf_remaining = i2c_dev->next_msg_buf_remaining;
	i2c_dev->msg_err = i2c_dev->next_msg_err;
	i2c_dev->msg_read = i2c_dev->next_msg_read;
	i2c_dev->msg_add = i2c_dev->next_msg_add;
	i2c_dev->packet_header = i2c_dev->next_packet_header;
	i2c_dev->io_header = i2c_dev->next_io_header;
	i2c_dev->payload_size = i2c_dev->next_payload_size;

	return 0;
}

static irqreturn_t tegra_i2c_isr(int irq, void *dev_id)
{
	u32 status;
	unsigned long flags = 0;

	const u32 status_err = I2C_INT_NO_ACK | I2C_INT_ARBITRATION_LOST
					| I2C_INT_TX_FIFO_OVERFLOW;
	struct tegra_i2c_dev *i2c_dev = dev_id;
	u32 mask;

	status = i2c_readl(i2c_dev, I2C_INT_STATUS);

	if (status == 0) {
		dev_warn(i2c_dev->dev, "unknown interrupt Add 0x%02x\n",
						i2c_dev->msg_add);
		i2c_dev->msg_err |= I2C_ERR_UNKNOWN_INTERRUPT;

		if (!i2c_dev->irq_disabled) {
			disable_irq_nosync(i2c_dev->irq);
			i2c_dev->irq_disabled = 1;
		}
		goto err;
	}

	if (unlikely(status & status_err)) {
		dev_dbg(i2c_dev->dev, "I2c error status 0x%08x\n", status);
		if (status & I2C_INT_NO_ACK) {
			i2c_dev->msg_err |= I2C_ERR_NO_ACK;
			dev_warn(i2c_dev->dev, "no acknowledge from address"
					" 0x%x\n", i2c_dev->msg_add);
			dev_dbg(i2c_dev->dev, "Packet status 0x%08x\n",
				i2c_readl(i2c_dev, I2C_PACKET_TRANSFER_STATUS));
		}

		if (status & I2C_INT_ARBITRATION_LOST) {
			i2c_dev->msg_err |= I2C_ERR_ARBITRATION_LOST;
			dev_warn(i2c_dev->dev, "arbitration lost during "
				" communicate to add 0x%x\n", i2c_dev->msg_add);
			dev_dbg(i2c_dev->dev, "Packet status 0x%08x\n",
				i2c_readl(i2c_dev, I2C_PACKET_TRANSFER_STATUS));
		}

		if (status & I2C_INT_TX_FIFO_OVERFLOW) {
			i2c_dev->msg_err |= I2C_INT_TX_FIFO_OVERFLOW;
			dev_warn(i2c_dev->dev, "Tx fifo overflow during "
				" communicate to add 0x%x\n", i2c_dev->msg_add);
			dev_dbg(i2c_dev->dev, "Packet status 0x%08x\n",
				i2c_readl(i2c_dev, I2C_PACKET_TRANSFER_STATUS));
		}
		goto err;
	}

	if (i2c_dev->chipdata->has_hw_arb_support &&
			(status & I2C_INT_BUS_CLEAR_DONE))
		goto err;

	if (unlikely((i2c_readl(i2c_dev, I2C_STATUS) & I2C_STATUS_BUSY)
				&& (status == I2C_INT_TX_FIFO_DATA_REQ)
				&& i2c_dev->msg_read
				&& i2c_dev->msg_buf_remaining)) {
		dev_warn(i2c_dev->dev, "unexpected status\n");
		i2c_dev->msg_err |= I2C_ERR_UNEXPECTED_STATUS;

		if (!i2c_dev->irq_disabled) {
			disable_irq_nosync(i2c_dev->irq);
			i2c_dev->irq_disabled = 1;
		}

		goto err;
	}

	if (i2c_dev->msg_read && (status & I2C_INT_RX_FIFO_DATA_REQ)) {
		if (i2c_dev->msg_buf_remaining)
			tegra_i2c_empty_rx_fifo(i2c_dev);
		else
			BUG();
	}

	if (!i2c_dev->msg_read && (status & I2C_INT_TX_FIFO_DATA_REQ)) {
		if (i2c_dev->msg_buf_remaining) {

			if (!i2c_dev->chipdata->has_xfer_complete_interrupt)
				spin_lock_irqsave(&i2c_dev->fifo_lock, flags);

			tegra_i2c_fill_tx_fifo(i2c_dev);

			if (!i2c_dev->chipdata->has_xfer_complete_interrupt)
				spin_unlock_irqrestore(&i2c_dev->fifo_lock, flags);

		}
		else
			tegra_i2c_mask_irq(i2c_dev, I2C_INT_TX_FIFO_DATA_REQ);
	}

	i2c_writel(i2c_dev, status, I2C_INT_STATUS);

	if (i2c_dev->is_dvc)
		dvc_writel(i2c_dev, DVC_STATUS_I2C_DONE_INTR, DVC_STATUS);

	if (status & I2C_INT_ALL_PACKETS_XFER_COMPLETE) {
		BUG_ON(i2c_dev->msg_buf_remaining);
		complete(&i2c_dev->msg_complete);
	} else if ((status & I2C_INT_PACKET_XFER_COMPLETE)
				&& i2c_dev->use_single_xfer_complete) {
		BUG_ON(i2c_dev->msg_buf_remaining);
		complete(&i2c_dev->msg_complete);
	}

	return IRQ_HANDLED;

err:
	dev_dbg(i2c_dev->dev, "reg: 0x%08x 0x%08x 0x%08x 0x%08x\n",
		 i2c_readl(i2c_dev, I2C_CNFG), i2c_readl(i2c_dev, I2C_STATUS),
		 i2c_readl(i2c_dev, I2C_INT_STATUS),
		 i2c_readl(i2c_dev, I2C_PACKET_TRANSFER_STATUS));

	dev_dbg(i2c_dev->dev, "packet: 0x%08x %u 0x%08x\n",
		 i2c_dev->packet_header, i2c_dev->payload_size,
		 i2c_dev->io_header);

	if (i2c_dev->msgs) {
		struct i2c_msg *msgs = i2c_dev->msgs;
		int i;

		for (i = 0; i < i2c_dev->msgs_num; i++)
			dev_dbg(i2c_dev->dev,
				 "msgs[%d] %c, addr=0x%04x, len=%d\n",
				 i, (msgs[i].flags & I2C_M_RD) ? 'R' : 'W',
				 msgs[i].addr, msgs[i].len);
	}

	mask = I2C_INT_NO_ACK | I2C_INT_ARBITRATION_LOST |
		I2C_INT_PACKET_XFER_COMPLETE | I2C_INT_TX_FIFO_DATA_REQ |
		I2C_INT_RX_FIFO_DATA_REQ | I2C_INT_TX_FIFO_OVERFLOW;

	i2c_writel(i2c_dev, status, I2C_INT_STATUS);

	if (i2c_dev->chipdata->has_xfer_complete_interrupt)
		mask |= I2C_INT_ALL_PACKETS_XFER_COMPLETE;

	if (!(i2c_dev->use_single_xfer_complete &&
			i2c_dev->chipdata->has_xfer_complete_interrupt))
		mask |= I2C_INT_ALL_PACKETS_XFER_COMPLETE;

	if (i2c_dev->chipdata->has_hw_arb_support)
		mask |= I2C_INT_BUS_CLEAR_DONE;

	/* An error occurred, mask all interrupts */
	tegra_i2c_mask_irq(i2c_dev, mask);

	/* An error occured, mask dvc interrupt */
	if (i2c_dev->is_dvc)
		dvc_i2c_mask_irq(i2c_dev, DVC_CTRL_REG3_I2C_DONE_INTR_EN);

	if (i2c_dev->is_dvc)
		dvc_writel(i2c_dev, DVC_STATUS_I2C_DONE_INTR, DVC_STATUS);

	complete(&i2c_dev->msg_complete);

	return IRQ_HANDLED;
}

static int tegra_i2c_send_next_read_msg_pkt_header(struct tegra_i2c_dev *i2c_dev, struct i2c_msg *next_msg, enum msg_end_type end_state)
{
	i2c_dev->next_msg_buf = next_msg->buf;
	i2c_dev->next_msg_buf_remaining = next_msg->len;
	i2c_dev->next_msg_err = I2C_ERR_NONE;
	i2c_dev->next_msg_read = 1;
	i2c_dev->next_msg_add = next_msg->addr;
	i2c_dev->next_packet_header = (0 << PACKET_HEADER0_HEADER_SIZE_SHIFT) |
			PACKET_HEADER0_PROTOCOL_I2C |
			(i2c_dev->cont_id << PACKET_HEADER0_CONT_ID_SHIFT) |
			(1 << PACKET_HEADER0_PACKET_ID_SHIFT);

	i2c_writel(i2c_dev, i2c_dev->next_packet_header, I2C_TX_FIFO);

	i2c_dev->next_payload_size = next_msg->len - 1;
	i2c_writel(i2c_dev, i2c_dev->next_payload_size, I2C_TX_FIFO);

	i2c_dev->next_io_header = I2C_HEADER_IE_ENABLE;

	if (end_state == MSG_END_CONTINUE)
		i2c_dev->next_io_header |= I2C_HEADER_CONTINUE_XFER;
	else if (end_state == MSG_END_REPEAT_START)
		i2c_dev->next_io_header |= I2C_HEADER_REPEAT_START;

	if (next_msg->flags & I2C_M_TEN) {
		i2c_dev->next_io_header |= next_msg->addr;
		i2c_dev->next_io_header |= I2C_HEADER_10BIT_ADDR;
	} else {
		i2c_dev->next_io_header |= (next_msg->addr << I2C_HEADER_SLAVE_ADDR_SHIFT);
	}
	if (next_msg->flags & I2C_M_IGNORE_NAK)
		i2c_dev->next_io_header |= I2C_HEADER_CONT_ON_NAK;

	i2c_dev->next_io_header |= I2C_HEADER_READ;

	if (i2c_dev->is_high_speed_enable) {
		i2c_dev->next_io_header |= I2C_HEADER_HIGHSPEED_MODE;
		i2c_dev->next_io_header |= ((i2c_dev->hs_master_code & 0x7)
					<<  I2C_HEADER_MASTER_ADDR_SHIFT);
	}
	i2c_writel(i2c_dev, i2c_dev->next_io_header, I2C_TX_FIFO);

	return 0;
}

static int tegra_i2c_xfer_msg(struct tegra_i2c_bus *i2c_bus,
	struct i2c_msg *msg, enum msg_end_type end_state, struct i2c_msg *next_msg, enum msg_end_type next_msg_end_state)
{
	struct tegra_i2c_dev *i2c_dev = i2c_bus->dev;
	u32 int_mask;
	int ret;
	unsigned long flags = 0;

	if (msg->len == 0)
		return -EINVAL;

	tegra_i2c_flush_fifos(i2c_dev);


	i2c_dev->msg_buf = msg->buf;
	i2c_dev->msg_buf_remaining = msg->len;
	i2c_dev->msg_err = I2C_ERR_NONE;
	i2c_dev->msg_read = (msg->flags & I2C_M_RD);
	INIT_COMPLETION(i2c_dev->msg_complete);

	if (!i2c_dev->chipdata->has_xfer_complete_interrupt)
		spin_lock_irqsave(&i2c_dev->fifo_lock, flags);

	i2c_dev->msg_add = msg->addr;

	i2c_dev->packet_header = (0 << PACKET_HEADER0_HEADER_SIZE_SHIFT) |
			PACKET_HEADER0_PROTOCOL_I2C |
			(i2c_dev->cont_id << PACKET_HEADER0_CONT_ID_SHIFT) |
			(1 << PACKET_HEADER0_PACKET_ID_SHIFT);
	i2c_writel(i2c_dev, i2c_dev->packet_header, I2C_TX_FIFO);

	i2c_dev->payload_size = msg->len - 1;
	i2c_writel(i2c_dev, i2c_dev->payload_size, I2C_TX_FIFO);

	i2c_dev->use_single_xfer_complete = true;
	i2c_dev->io_header = 0;
	if (next_msg == NULL)
		i2c_dev->io_header = I2C_HEADER_IE_ENABLE;

	if (end_state == MSG_END_CONTINUE)
		i2c_dev->io_header |= I2C_HEADER_CONTINUE_XFER;
	else if (end_state == MSG_END_REPEAT_START)
		i2c_dev->io_header |= I2C_HEADER_REPEAT_START;

	if (msg->flags & I2C_M_TEN) {
		i2c_dev->io_header |= msg->addr;
		i2c_dev->io_header |= I2C_HEADER_10BIT_ADDR;
	} else {
		i2c_dev->io_header |= (msg->addr << I2C_HEADER_SLAVE_ADDR_SHIFT);
	}
	if (msg->flags & I2C_M_IGNORE_NAK)
		i2c_dev->io_header |= I2C_HEADER_CONT_ON_NAK;
	if (msg->flags & I2C_M_RD)
		i2c_dev->io_header |= I2C_HEADER_READ;
	if (i2c_dev->is_high_speed_enable) {
		i2c_dev->io_header |= I2C_HEADER_HIGHSPEED_MODE;
		i2c_dev->io_header |= ((i2c_dev->hs_master_code & 0x7)
					<<  I2C_HEADER_MASTER_ADDR_SHIFT);
	}
	i2c_writel(i2c_dev, i2c_dev->io_header, I2C_TX_FIFO);

	if (!(msg->flags & I2C_M_RD))
		tegra_i2c_fill_tx_fifo(i2c_dev);

	if (i2c_dev->is_dvc)
		dvc_i2c_unmask_irq(i2c_dev, DVC_CTRL_REG3_I2C_DONE_INTR_EN);

	int_mask = I2C_INT_NO_ACK | I2C_INT_ARBITRATION_LOST
					| I2C_INT_TX_FIFO_OVERFLOW;
	if (i2c_dev->chipdata->has_xfer_complete_interrupt)
		int_mask |= I2C_INT_PACKET_XFER_COMPLETE;

	if (i2c_dev->chipdata->has_xfer_complete_interrupt)
		int_mask |= I2C_INT_ALL_PACKETS_XFER_COMPLETE;

	if (msg->flags & I2C_M_RD)
		int_mask |= I2C_INT_RX_FIFO_DATA_REQ;
	else if (i2c_dev->msg_buf_remaining)
		int_mask |= I2C_INT_TX_FIFO_DATA_REQ;

	if (next_msg != NULL) {
		tegra_i2c_send_next_read_msg_pkt_header(i2c_dev, next_msg,
							next_msg_end_state);
		tegra_i2c_copy_next_to_current(i2c_dev);
		int_mask |= I2C_INT_RX_FIFO_DATA_REQ;
		i2c_dev->use_single_xfer_complete = false;
	}

	if (!(i2c_dev->use_single_xfer_complete &&
			i2c_dev->chipdata->has_xfer_complete_interrupt))
		int_mask |= I2C_INT_ALL_PACKETS_XFER_COMPLETE;

	if (!i2c_dev->chipdata->has_xfer_complete_interrupt)
		spin_unlock_irqrestore(&i2c_dev->fifo_lock, flags);

	tegra_i2c_unmask_irq(i2c_dev, int_mask);

	dev_dbg(i2c_dev->dev, "unmasked irq: %02x\n",
		i2c_readl(i2c_dev, I2C_INT_MASK));

	ret = wait_for_completion_timeout(&i2c_dev->msg_complete,
					TEGRA_I2C_TIMEOUT);
	tegra_i2c_mask_irq(i2c_dev, int_mask);

	if (i2c_dev->is_dvc)
		dvc_i2c_mask_irq(i2c_dev, DVC_CTRL_REG3_I2C_DONE_INTR_EN);

	if (WARN_ON(ret == 0)) {
		dev_err(i2c_dev->dev,
			"i2c transfer timed out, addr 0x%04x, data 0x%02x\n",
			msg->addr, msg->buf[0]);

		tegra_i2c_init(i2c_dev);
		return -ETIMEDOUT;
	}

	dev_dbg(i2c_dev->dev, "transfer complete: %d %d %d\n",
		ret, completion_done(&i2c_dev->msg_complete), i2c_dev->msg_err);

	if (likely(i2c_dev->msg_err == I2C_ERR_NONE))
		return 0;

	if ((i2c_dev->chipdata->timeout_irq_occurs_before_bus_inactive) &&
		(i2c_dev->msg_err == I2C_ERR_NO_ACK)) {
		/*
		* In NACK error condition resetting of I2C controller happens
		* before STOP condition is properly completed by I2C controller,
		* so wait for 2 clock cycle to complete STOP condition.
		*/
		udelay(DIV_ROUND_UP(2 * 1000000, i2c_dev->last_bus_clk_rate));
	}

	/*
	 * NACK interrupt is generated before the I2C controller generates the
	 * STOP condition on the bus. So wait for 2 clock periods before resetting
	 * the controller so that STOP condition has been delivered properly.
	 */
	if (i2c_dev->msg_err == I2C_ERR_NO_ACK)
		udelay(DIV_ROUND_UP(2 * 1000000, i2c_dev->last_bus_clk_rate));

	tegra_i2c_init(i2c_dev);

	/* Arbitration Lost occurs, Start recovery */
	if (i2c_dev->msg_err == I2C_ERR_ARBITRATION_LOST) {
		if (i2c_dev->chipdata->has_hw_arb_support) {
			INIT_COMPLETION(i2c_dev->msg_complete);
			i2c_writel(i2c_dev, I2C_BC_ENABLE
					| I2C_BC_SCLK_THRESHOLD
					| I2C_BC_STOP_COND
					| I2C_BC_TERMINATE
					, I2C_BUS_CLEAR_CNFG);
			tegra_i2c_unmask_irq(i2c_dev, I2C_INT_BUS_CLEAR_DONE);

			wait_for_completion_timeout(&i2c_dev->msg_complete,
				TEGRA_I2C_TIMEOUT);

			if (!(i2c_readl(i2c_dev, I2C_BUS_CLEAR_STATUS) & I2C_BC_STATUS))
				dev_warn(i2c_dev->dev, "Un-recovered Arbitration lost\n");
			else
				dev_warn(i2c_dev->dev, "Recovered Arbitration lost\n");

		} else if (i2c_dev->arb_recovery)
			i2c_dev->arb_recovery(i2c_bus->scl_gpio,
							i2c_bus->sda_gpio);
		return -EAGAIN;
	}

	if (i2c_dev->msg_err == I2C_ERR_NO_ACK) {
		if (msg->flags & I2C_M_IGNORE_NAK)
			return 0;
		return -EREMOTEIO;
	}

	if (i2c_dev->msg_err & I2C_ERR_UNEXPECTED_STATUS)
		return -EAGAIN;

	return -EIO;
}

static int tegra_i2c_xfer(struct i2c_adapter *adap, struct i2c_msg msgs[],
	int num)
{
	struct tegra_i2c_bus *i2c_bus = i2c_get_adapdata(adap);
	struct tegra_i2c_dev *i2c_dev = i2c_bus->dev;
	int i;
	int ret = 0;
	bool continue_xfer = i2c_dev->chipdata->has_continue_xfer_support;

	rt_mutex_lock(&i2c_dev->dev_lock);

	if (i2c_dev->is_suspended) {
		rt_mutex_unlock(&i2c_dev->dev_lock);
		return -EBUSY;
	}

	/* Support I2C_M_NOSTART only if HW support continue xfer. */
	for (i = 0; i < num - 1; i++) {
			if ((msgs[i + 1].flags & I2C_M_NOSTART) && !continue_xfer) {
			dev_err(i2c_dev->dev, "mesg %d have illegal flag\n", i + 1);
			rt_mutex_unlock(&i2c_dev->dev_lock);
			return -EINVAL;
		}
	}

	if (i2c_dev->last_mux != i2c_bus->mux) {
		tegra_pinmux_set_safe_pinmux_table(i2c_dev->last_mux,
			i2c_dev->last_mux_len);
		tegra_pinmux_config_pinmux_table(i2c_bus->mux,
			i2c_bus->mux_len);
		i2c_dev->last_mux = i2c_bus->mux;
		i2c_dev->last_mux_len = i2c_bus->mux_len;
	}

	if (i2c_dev->last_bus_clk_rate != i2c_bus->bus_clk_rate) {
		tegra_i2c_set_clk_rate(i2c_dev);
		i2c_dev->last_bus_clk_rate = i2c_bus->bus_clk_rate;
	}

	i2c_dev->msgs = msgs;
	i2c_dev->msgs_num = num;

	pm_runtime_get_sync(&adap->dev);
	tegra_i2c_clock_enable(i2c_dev);

	for (i = 0; i < num; i++) {
		enum msg_end_type end_type = MSG_END_STOP;
		enum msg_end_type next_msg_end_type = MSG_END_STOP;

		if (i < (num - 1)) {
			if (msgs[i + 1].flags & I2C_M_NOSTART)
				end_type = MSG_END_CONTINUE;
			else
				end_type = MSG_END_REPEAT_START;
			if (i < num - 2) {
				if (msgs[i + 2].flags & I2C_M_NOSTART)
					next_msg_end_type = MSG_END_CONTINUE;
				else
					next_msg_end_type = MSG_END_REPEAT_START;
			}
			if ((!(msgs[i].flags & I2C_M_RD)) && (msgs[i].len <= 8) && (msgs[i+1].flags & I2C_M_RD)
					&& (next_msg_end_type != MSG_END_CONTINUE) && (end_type == MSG_END_REPEAT_START)) {
				ret = tegra_i2c_xfer_msg(i2c_bus, &msgs[i], end_type, &msgs[i+1], next_msg_end_type);
				if (ret)
					break;
				i++;
			} else {
				ret = tegra_i2c_xfer_msg(i2c_bus, &msgs[i], end_type, NULL, next_msg_end_type);
				if (ret)
					break;
			}
		} else {
			ret = tegra_i2c_xfer_msg(i2c_bus, &msgs[i], end_type, NULL, next_msg_end_type);
			if (ret)
				break;
		}
	}

	tegra_i2c_clock_disable(i2c_dev);
	pm_runtime_put(&adap->dev);

	rt_mutex_unlock(&i2c_dev->dev_lock);

	i2c_dev->msgs = NULL;
	i2c_dev->msgs_num = 0;

	return ret ?: i;
}

static u32 tegra_i2c_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL | I2C_FUNC_10BIT_ADDR |
			I2C_FUNC_PROTOCOL_MANGLING;
}

static const struct i2c_algorithm tegra_i2c_algo = {
	.master_xfer	= tegra_i2c_xfer,
	.functionality	= tegra_i2c_func,
};

static struct tegra_i2c_chipdata tegra20_i2c_chipdata = {
	.timeout_irq_occurs_before_bus_inactive = true,
	.has_xfer_complete_interrupt = false,
	.has_hw_arb_support = false,
	.has_fast_clock = true,
	.has_clk_divisor_std_fast_mode = false,
	.clk_divisor_std_fast_mode = 0,
	.clk_divisor_hs_mode = 3,
	.clk_multiplier_hs_mode = 12,
};

static struct tegra_i2c_chipdata tegra11_i2c_chipdata = {
	.timeout_irq_occurs_before_bus_inactive = false,
	.has_xfer_complete_interrupt = true,
	.has_hw_arb_support = true,
	.has_fast_clock = false,
	.has_clk_divisor_std_fast_mode = true,
	.clk_divisor_std_fast_mode = 0x19,
	.clk_divisor_hs_mode = 1,
	.clk_multiplier_hs_mode = 3,
};

/* Match table for of_platform binding */
static const struct of_device_id tegra_i2c_of_match[] __devinitconst = {
	{ .compatible = "nvidia,tegra114-i2c", .data = &tegra11_i2c_chipdata, },
	{ .compatible = "nvidia,tegra20-i2c", .data = &tegra20_i2c_chipdata, },
	{ .compatible = "nvidia,tegra20-i2c-dvc", .data = &tegra20_i2c_chipdata, },
	{},
};
MODULE_DEVICE_TABLE(of, tegra_i2c_of_match);

static struct platform_device_id tegra_i2c_devtype[] = {
	{
		.name = "tegra-i2c",
		.driver_data = (unsigned long)&tegra20_i2c_chipdata,
	},
	{
		.name = "tegra11-i2c",
		.driver_data = (unsigned long)&tegra11_i2c_chipdata,
	}
};

static int __devinit tegra_i2c_probe(struct platform_device *pdev)
{
	struct tegra_i2c_dev *i2c_dev;
	struct tegra_i2c_platform_data *plat = pdev->dev.platform_data;
	struct resource *res;
	struct clk *div_clk;
	struct clk *fast_clk = NULL;
	const unsigned int *prop;
	void __iomem *base;
	int irq;
	int nbus;
	int i = 0;
	int ret = 0;
	struct tegra_i2c_chipdata *chip_data = NULL;
	const struct of_device_id *match;

	match = of_match_device(of_match_ptr(tegra_i2c_of_match), &pdev->dev);
	if (match)
		chip_data = match->data;
	else
		chip_data = (struct tegra_i2c_chipdata *)pdev->id_entry->driver_data;

	if (!plat || !chip_data) {
		dev_err(&pdev->dev, "no platform/chip data?\n");
		return -ENODEV;
	}

#if !defined(CONFIG_ARCH_TEGRA_2x_SOC)
	chip_data->has_continue_xfer_support = true;
#endif

	if (plat->bus_count <= 0 || plat->adapter_nr < 0) {
		dev_err(&pdev->dev, "invalid platform data?\n");
		return -ENODEV;
	}

	WARN_ON(plat->bus_count > TEGRA_I2C_MAX_BUS);
	nbus = min(TEGRA_I2C_MAX_BUS, plat->bus_count);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "no mem resource\n");
		return -EINVAL;
	}

	base = devm_request_and_ioremap(&pdev->dev, res);
	if (!base) {
		dev_err(&pdev->dev, "Cannot request/ioremap I2C registers\n");
		return -EADDRNOTAVAIL;
	}

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		dev_err(&pdev->dev, "no irq resource\n");
		return -EINVAL;
	}
	irq = res->start;

	i2c_dev = devm_kzalloc(&pdev->dev, sizeof(struct tegra_i2c_dev) +
			  (nbus-1) * sizeof(struct tegra_i2c_bus), GFP_KERNEL);
	if (!i2c_dev) {
		dev_err(&pdev->dev, "Could not allocate struct tegra_i2c_dev");
		return -ENOMEM;
	}

	i2c_dev->chipdata = chip_data;

	if (!i2c_dev->chipdata) {
		dev_err(&pdev->dev, "Error: Chip data is not valid\n");
		return -ENOMEM;
	}

	div_clk = devm_clk_get(&pdev->dev, "div-clk");
	if (IS_ERR(div_clk)) {
		dev_err(&pdev->dev, "missing controller clock");
		return PTR_ERR(div_clk);
	}

	if (i2c_dev->chipdata->has_fast_clock) {
		fast_clk = devm_clk_get(&pdev->dev, "fast-clk");
		if (IS_ERR(fast_clk)) {
			dev_err(&pdev->dev, "missing controller fast clock");
			return PTR_ERR(fast_clk);
		}
	}

	i2c_dev->base = base;
	i2c_dev->div_clk = div_clk;
	if (i2c_dev->chipdata->has_fast_clock)
		i2c_dev->fast_clk = fast_clk;
	i2c_dev->irq = irq;
	i2c_dev->cont_id = pdev->id;
	i2c_dev->dev = &pdev->dev;
	i2c_dev->is_clkon_always = plat->is_clkon_always;

	i2c_dev->last_bus_clk_rate = 100000; /* default clock rate */
	if (plat) {
		i2c_dev->last_bus_clk_rate = plat->bus_clk_rate[0];

	} else if (i2c_dev->dev->of_node) {    /* if there is a device tree node ... */
		/* TODO: DAN: this doesn't work for DT */
		prop = of_get_property(i2c_dev->dev->of_node,
				"clock-frequency", NULL);
		if (prop)
			i2c_dev->last_bus_clk_rate = be32_to_cpup(prop);

		/* FIXME! Populate the Tegra30 and then support M_NOSTART */
		i2c_dev->chipdata->has_continue_xfer_support = false;
	}

	i2c_dev->is_high_speed_enable = plat->is_high_speed_enable;
	i2c_dev->last_bus_clk_rate = plat->bus_clk_rate[0] ?: 100000;
	i2c_dev->msgs = NULL;
	i2c_dev->msgs_num = 0;
	rt_mutex_init(&i2c_dev->dev_lock);

	if (pdev->dev.of_node)
		i2c_dev->is_dvc = of_device_is_compatible(pdev->dev.of_node,
						"nvidia,tegra20-i2c-dvc");
	else
		i2c_dev->is_dvc = plat->is_dvc;
	i2c_dev->slave_addr = plat->slave_addr;
	i2c_dev->hs_master_code = plat->hs_master_code;
	i2c_dev->is_dvc = plat->is_dvc;
	init_completion(&i2c_dev->msg_complete);

	if (!i2c_dev->chipdata->has_xfer_complete_interrupt)
		spin_lock_init(&i2c_dev->fifo_lock);

	if (!i2c_dev->chipdata->has_hw_arb_support)
		i2c_dev->arb_recovery = plat->arb_recovery;

	platform_set_drvdata(pdev, i2c_dev);

	if (i2c_dev->is_clkon_always)
		tegra_i2c_clock_enable(i2c_dev);

	ret = tegra_i2c_init(i2c_dev);
	if (ret) {
		dev_err(&pdev->dev, "Failed to initialize i2c controller");
		return ret;
	}

	ret = devm_request_irq(&pdev->dev, i2c_dev->irq,
			tegra_i2c_isr, IRQF_NO_SUSPEND,
			dev_name(&pdev->dev), i2c_dev);
	if (ret) {
		dev_err(&pdev->dev, "Failed to request irq %i\n", i2c_dev->irq);
		return ret;
	}

	pm_runtime_enable(&pdev->dev);

	for (i = 0; i < nbus; i++) {
		struct tegra_i2c_bus *i2c_bus = &i2c_dev->busses[i];

		i2c_bus->dev = i2c_dev;
		i2c_bus->mux = plat->bus_mux[i];
		i2c_bus->mux_len = plat->bus_mux_len[i];
		i2c_bus->bus_clk_rate = plat->bus_clk_rate[i] ?: 100000;

		if (i2c_dev->arb_recovery) {
			i2c_bus->scl_gpio = plat->scl_gpio[i];
			i2c_bus->sda_gpio = plat->sda_gpio[i];
		}
		i2c_bus->adapter.dev.of_node = pdev->dev.of_node;
		i2c_bus->adapter.algo = &tegra_i2c_algo;
		i2c_set_adapdata(&i2c_bus->adapter, i2c_bus);
		i2c_bus->adapter.owner = THIS_MODULE;
		i2c_bus->adapter.class = I2C_CLASS_HWMON;
		strlcpy(i2c_bus->adapter.name, "Tegra I2C adapter",
			sizeof(i2c_bus->adapter.name));
		i2c_bus->adapter.dev.parent = &pdev->dev;
		i2c_bus->adapter.nr = plat->adapter_nr + i;

		if (plat->retries)
			i2c_bus->adapter.retries = plat->retries;
		else
			i2c_bus->adapter.retries = TEGRA_I2C_RETRIES;

		if (plat->timeout)
			i2c_bus->adapter.timeout = plat->timeout;

		ret = i2c_add_numbered_adapter(&i2c_bus->adapter);
		if (ret) {
			dev_err(&pdev->dev, "Failed to add I2C adapter\n");
			goto err_del_bus;
		}

		of_i2c_register_devices(&i2c_bus->adapter);
		pm_runtime_enable(&i2c_bus->adapter.dev);

		i2c_dev->bus_count++;
	}


	return 0;

err_del_bus:
	while (i2c_dev->bus_count--)
		i2c_del_adapter(&i2c_dev->busses[i2c_dev->bus_count].adapter);
	return ret;
}

static int __devexit tegra_i2c_remove(struct platform_device *pdev)
{
	struct tegra_i2c_dev *i2c_dev = platform_get_drvdata(pdev);

	while (i2c_dev->bus_count--) {
		i2c_del_adapter(&i2c_dev->busses[i2c_dev->bus_count].adapter);
		pm_runtime_disable(&i2c_dev->busses[i2c_dev->bus_count].adapter.dev);
	}

	if (i2c_dev->is_clkon_always)
		tegra_i2c_clock_disable(i2c_dev);

	pm_runtime_disable(&pdev->dev);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int tegra_i2c_suspend_noirq(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct tegra_i2c_dev *i2c_dev = platform_get_drvdata(pdev);

	rt_mutex_lock(&i2c_dev->dev_lock);

	i2c_dev->is_suspended = true;
	if (i2c_dev->is_clkon_always)
		tegra_i2c_clock_disable(i2c_dev);

	rt_mutex_unlock(&i2c_dev->dev_lock);

	return 0;
}

static int tegra_i2c_resume_noirq(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct tegra_i2c_dev *i2c_dev = platform_get_drvdata(pdev);
	int ret;

	rt_mutex_lock(&i2c_dev->dev_lock);

	if (i2c_dev->is_clkon_always)
		tegra_i2c_clock_enable(i2c_dev);

	ret = tegra_i2c_init(i2c_dev);

	if (ret) {
		rt_mutex_unlock(&i2c_dev->dev_lock);
		return ret;
	}

	i2c_dev->is_suspended = false;

	rt_mutex_unlock(&i2c_dev->dev_lock);

	return 0;
}

static const struct dev_pm_ops tegra_i2c_pm = {
	.suspend_noirq = tegra_i2c_suspend_noirq,
	.resume_noirq = tegra_i2c_resume_noirq,
};
#define TEGRA_I2C_PM	(&tegra_i2c_pm)
#else
#define TEGRA_I2C_PM	NULL
#endif

static struct platform_driver tegra_i2c_driver = {
	.probe   = tegra_i2c_probe,
	.remove  = __devexit_p(tegra_i2c_remove),
	.id_table = tegra_i2c_devtype,
	.driver  = {
		.name  = "tegra-i2c",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(tegra_i2c_of_match),
		.pm    = TEGRA_I2C_PM,
	},
};

static int __init tegra_i2c_init_driver(void)
{
	return platform_driver_register(&tegra_i2c_driver);
}

static void __exit tegra_i2c_exit_driver(void)
{
	platform_driver_unregister(&tegra_i2c_driver);
}

subsys_initcall(tegra_i2c_init_driver);
module_exit(tegra_i2c_exit_driver);

MODULE_DESCRIPTION("nVidia Tegra2 I2C Bus Controller driver");
MODULE_AUTHOR("Colin Cross");
MODULE_LICENSE("GPL v2");
