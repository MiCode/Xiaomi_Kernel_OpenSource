/*
 * drivers/i2c/busses/i2c-slave-tegra.c
 * I2c slave driver for the Nvidia's tegra SOC.
 *
 * Copyright (c) 2009-2011, NVIDIA Corporation.
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
#include <linux/i2c.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/i2c-tegra.h>
#include <linux/i2c-slave.h>
#include <linux/module.h>
#include <asm/unaligned.h>
#include <mach/clk.h>
#include <mach/pinmux.h>
#include <linux/pm_runtime.h>
#define BYTES_PER_FIFO_WORD 4
#define to_jiffies(msecs) msecs_to_jiffies(msecs)

#define I2C_CNFG				0x000
#define I2C_CNFG_PACKET_MODE_EN			(1<<10)
#define I2C_CNFG_NEW_MASTER_FSM			(1<<11)
#define I2C_CNFG_DEBOUNCE_CNT_SHIFT		12
#define I2C_CNFG_DEBOUNCE_CNT_MASK		(0x7)

#define I2C_STATUS				0x01C

#define I2C_SLV_CNFG				0x020
#define I2C_SLV_CNFG_NEWSL			(1<<2)
#define I2C_SLV_CNFG_ENABLE_SL			(1<<3)
#define I2C_SLV_CNFG_PKT_MODE_EN		(1<<4)
#define I2C_SLV_CNFG_FIFO_XFER_EN		(1<<20)
#define I2C_SLV_CNFG_ACK_LAST_BYTE		(1<<6)
#define I2C_SLV_CNFG_ACK_LAST_BYTE_VALID	(1<<7)

#define I2C_SLV_ADDR1				0x02c
#define I2C_SLV_ADDR1_ADDR_SHIFT		0x0

#define I2C_SLV_ADDR2				0x030
#define I2C_SLV_ADDR2_ADDR0_HI_SHIFT		0x1
#define I2C_SLV_ADDR2_ADDR0_MASK		0x7
#define I2C_SLV_ADDR2_ADDR0_TEN_BIT_ADDR_MODE	0x1

#define I2C_SLV_INT_MASK			0x040

#define I2C_TX_FIFO				0x050
#define I2C_RX_FIFO				0x054
#define I2C_PACKET_TRANSFER_STATUS		0x058

#define I2C_FIFO_CONTROL			0x05c
#define I2C_FIFO_CONTROL_SLV_TX_FLUSH		(1<<9)
#define I2C_FIFO_CONTROL_SLV_RX_FLUSH		(1<<8)
#define I2C_FIFO_CONTROL_SLV_TX_TRIG_SHIFT	13
#define I2C_FIFO_CONTROL_SLV_TX_TRIG_MASK	(0x7 << 13)
#define I2C_FIFO_CONTROL_SLV_RX_TRIG_SHIFT	10
#define I2C_FIFO_CONTROL_SLV_RX_TRIG_MASK	(1 << 10)

#define I2C_FIFO_STATUS				0x060
#define I2C_FIFO_STATUS_SLV_TX_MASK		(0xF << 20)
#define I2C_FIFO_STATUS_SLV_TX_SHIFT		20
#define I2C_FIFO_STATUS_SLV_RX_MASK		(0x0F << 16)
#define I2C_FIFO_STATUS_SLV_RX_SHIFT		16

#define I2C_INT_MASK				0x064
#define I2C_INT_STATUS				0x068
#define I2C_INT_PACKET_XFER_COMPLETE		(1<<7)
#define I2C_INT_ALL_PACKETS_XFER_COMPLETE	(1<<6)
#define I2C_INT_TX_FIFO_OVERFLOW		(1<<5)
#define I2C_INT_RX_FIFO_UNDERFLOW		(1<<4)
#define I2C_INT_NO_ACK				(1<<3)
#define I2C_INT_ARBITRATION_LOST		(1<<2)
#define I2C_INT_TX_FIFO_DATA_REQ		(1<<1)
#define I2C_INT_RX_FIFO_DATA_REQ		(1<<0)

#define I2C_INT_SLV_PKT_XFER_ERR		(1 << 25)
#define I2C_INT_SLV_TX_BUFFER_REQ		(1 << 24)
#define I2C_INT_SLV_RX_BUFFER_FILLED		(1 << 23)
#define I2C_INT_SLV_PACKET_XFER_COMPLETE	(1 << 22)
#define I2C_INT_SLV_TFIFO_OVF_REQ		(1 << 21)
#define I2C_INT_SLV_RFIFO_UNF_REQ		(1 << 20)
#define I2C_INT_SLV_TFIFO_DATA_REQ		(1 << 17)
#define I2C_INT_SLV_RFIFO_DATA_REQ		(1 << 16)

#define I2C_SLV_TX_FIFO				0x078
#define I2C_SLV_RX_FIFO				0x07c

#define I2C_SLV_PACKET_STATUS			0x80
#define I2C_SLV_PACKET_STATUS_BYTENUM_SHIFT	4
#define I2C_SLV_PACKET_STATUS_BYTENUM_MASK	0xFFF0

#define I2C_CLK_DIVISOR				0x06c

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
#define I2C_HEADER_MASTER_ADDR_SHIFT		12
#define I2C_HEADER_SLAVE_ADDR_SHIFT		1

#define I2C_FIFO_DEPTH				8
/* Transfer state of the i2c slave */
#define TRANSFER_STATE_NONE			0
#define TRANSFER_STATE_READ			1
#define TRANSFER_STATE_WRITE			2

#define I2C_SLV_TRANS_PREMATURE_END I2C_INT_SLV_PKT_XFER_ERR

#define I2C_SLV_TRANS_ALL_XFER_END I2C_INT_SLV_PACKET_XFER_COMPLETE

#define I2C_SLV_TRANS_END \
	(I2C_INT_SLV_PKT_XFER_ERR | I2C_INT_SLV_PACKET_XFER_COMPLETE)

#define I2C_INT_STATUS_RX_BUFFER_FILLED I2C_INT_SLV_RX_BUFFER_FILLED

#define I2C_INT_STATUS_RX_DATA_AVAILABLE \
	(I2C_INT_SLV_RX_BUFFER_FILLED | I2C_INT_SLV_RFIFO_DATA_REQ)

#define I2C_INT_STATUS_TX_BUFFER_REQUEST \
		(I2C_INT_SLV_TX_BUFFER_REQ | I2C_INT_SLV_TFIFO_DATA_REQ)

#define I2C_SLV_ERRORS_INT_MASK  (I2C_INT_SLV_TFIFO_OVF_REQ | \
		 I2C_INT_SLV_RFIFO_UNF_REQ | I2C_INT_SLV_PKT_XFER_ERR)

#define I2C_SLV_DEFAULT_INT_MASK (I2C_INT_SLV_TFIFO_OVF_REQ | \
		 I2C_INT_SLV_RFIFO_UNF_REQ | I2C_INT_SLV_PKT_XFER_ERR | \
		 I2C_INT_SLV_RX_BUFFER_FILLED |  I2C_INT_SLV_TX_BUFFER_REQ)

struct tegra_i2c_slave_dev;

struct tegra_i2c_slave_bus {
	struct tegra_i2c_slave_dev *dev;
	const struct tegra_pingroup_config *pinmux;
	int mux_len;
	unsigned long bus_clk_rate;
	struct i2c_slave_adapter slv_adap;
};

struct tegra_i2c_slave_dev {
	struct device *dev;
	struct clk *clk;
	struct resource *iomem;
	void __iomem *base;
	int cont_id;
	int irq;
	spinlock_t lock;
	struct completion rx_msg_complete;
	struct completion tx_msg_complete;
	bool is_rx_waiting;
	bool is_tx_waiting;
	u8 *rx_msg_buff;
	int rx_msg_buf_size;
	int rx_msg_head;
	int rx_msg_tail;
	u8 *tx_msg_buff;
	int tx_msg_buf_size;
	int tx_msg_head;
	int tx_msg_tail;
	bool is_slave_started;
	int slave_add;
	bool is_ten_bit_addr;
	u32 dummy_word;
	unsigned long rx_pack_hdr1;
	unsigned long rx_pack_hdr2;
	unsigned long rx_pack_hdr3;
	int curr_transfer;
	unsigned long int_mask;
	int nack_packet_count;
	bool is_first_byte_read_wait;
	int curr_packet_bytes_read;
	unsigned int cont_status;
	bool is_dummy_char_cycle;
	unsigned long curr_packet_tx_tail;
	const struct tegra_pingroup_config *pin_mux;
	int bus_clk;
	struct tegra_i2c_slave_bus bus;
};

#define get_space_count(rInd, wInd, maxsize) \
	    (((wInd > rInd) ? (maxsize - wInd + rInd) : (rInd - wInd)) - 1)

#define get_data_count(rInd, wInd, maxsize) \
	    ((wInd >= rInd) ? (wInd - rInd) : (maxsize - rInd + wInd - 1))

static void set_tx_trigger_level(struct tegra_i2c_slave_dev *i2c_dev, int trig)
{
	unsigned long fifo_control = readl(i2c_dev->base + I2C_FIFO_CONTROL);
	if (trig) {
		fifo_control &= ~I2C_FIFO_CONTROL_SLV_TX_TRIG_MASK;
		fifo_control |= (trig-1) << I2C_FIFO_CONTROL_SLV_TX_TRIG_SHIFT;
		writel(fifo_control, i2c_dev->base + I2C_FIFO_CONTROL);
	}
}

static void set_rx_trigger_level(struct tegra_i2c_slave_dev *i2c_dev, int trig)
{
	unsigned long fifo_control = readl(i2c_dev->base + I2C_FIFO_CONTROL);
	if (trig) {
		fifo_control &= ~I2C_FIFO_CONTROL_SLV_RX_TRIG_MASK;
		fifo_control |= (trig-1) << I2C_FIFO_CONTROL_SLV_RX_TRIG_SHIFT;
		writel(fifo_control, i2c_dev->base + I2C_FIFO_CONTROL);
	}
}

static void reset_slave_tx_fifo(struct tegra_i2c_slave_dev *i2c_dev)
{
	unsigned long fifo_control = readl(i2c_dev->base + I2C_FIFO_CONTROL);
	unsigned long timeout_count = 1000;

	writel(fifo_control | I2C_FIFO_CONTROL_SLV_TX_FLUSH,
					i2c_dev->base + I2C_FIFO_CONTROL);
	while (timeout_count--) {
		fifo_control = readl(i2c_dev->base + I2C_FIFO_CONTROL);
		if (!(fifo_control & I2C_FIFO_CONTROL_SLV_TX_FLUSH))
			break;
		udelay(1);
	}
	if (!timeout_count) {
		dev_err(i2c_dev->dev, "Not able to flush tx fifo\n");
		BUG();
	}
}

static void do_tx_fifo_empty(struct tegra_i2c_slave_dev *i2c_dev,
			unsigned long *empty_count)
{
	unsigned long fifo_status = readl(i2c_dev->base + I2C_FIFO_STATUS);
	unsigned long tx_fifo_empty_count;

	tx_fifo_empty_count = (fifo_status & I2C_FIFO_STATUS_SLV_TX_MASK) >>
					I2C_FIFO_STATUS_SLV_TX_SHIFT;
	if (tx_fifo_empty_count < I2C_FIFO_DEPTH)
		reset_slave_tx_fifo(i2c_dev);
	if (empty_count)
		*empty_count = tx_fifo_empty_count;
}

static void get_packet_headers(struct tegra_i2c_slave_dev *i2c_dev, u32 msg_len,
		u32 flags, unsigned long *packet_header1,
		unsigned long *packet_header2, unsigned long *packet_header3)
{
	unsigned long packet_header;
	*packet_header1 = (0 << PACKET_HEADER0_HEADER_SIZE_SHIFT) |
			PACKET_HEADER0_PROTOCOL_I2C |
			(i2c_dev->cont_id << PACKET_HEADER0_CONT_ID_SHIFT) |
			(1 << PACKET_HEADER0_PACKET_ID_SHIFT);
	*packet_header2 = msg_len-1;
	if (i2c_dev->is_ten_bit_addr)
		packet_header = i2c_dev->slave_add | I2C_HEADER_10BIT_ADDR;
	else
		packet_header = i2c_dev->slave_add <<
						I2C_HEADER_SLAVE_ADDR_SHIFT;

	if (flags & I2C_M_RD)
		packet_header |= I2C_HEADER_READ;

	*packet_header3 = packet_header;
}

static void configure_i2c_slave_packet_mode(struct tegra_i2c_slave_dev *i2c_dev)
{
	unsigned long i2c_config;
	i2c_config = I2C_CNFG_PACKET_MODE_EN | I2C_CNFG_NEW_MASTER_FSM;
	i2c_config |= (2 << I2C_CNFG_DEBOUNCE_CNT_SHIFT);
	writel(i2c_config, i2c_dev->base + I2C_CNFG);
}

static void configure_i2c_slave_address(struct tegra_i2c_slave_dev *i2c_dev)
{

	unsigned long slave_add_reg;
	unsigned long i2c_slv_config;
	unsigned long slave_add;

	if (i2c_dev->is_ten_bit_addr) {
		slave_add = i2c_dev->slave_add & 0xFF;
		slave_add_reg = readl(i2c_dev->base + I2C_SLV_ADDR1);
		slave_add_reg &= ~(0xFF);
		slave_add_reg |= slave_add << I2C_SLV_ADDR1_ADDR_SHIFT;
		writel(slave_add_reg, i2c_dev->base + I2C_SLV_ADDR1);

		slave_add = (i2c_dev->slave_add >> 8) & 0x3;
		slave_add_reg = readl(i2c_dev->base + I2C_SLV_ADDR2);
		slave_add_reg &= ~I2C_SLV_ADDR2_ADDR0_MASK;
		slave_add_reg |= slave_add |
					I2C_SLV_ADDR2_ADDR0_TEN_BIT_ADDR_MODE;
		writel(slave_add_reg, i2c_dev->base + I2C_SLV_ADDR2);
	} else {
		slave_add = (i2c_dev->slave_add & 0x3FF);
		slave_add_reg = readl(i2c_dev->base + I2C_SLV_ADDR1);
		slave_add_reg &= ~(0x3FF);
		slave_add_reg |= slave_add << I2C_SLV_ADDR1_ADDR_SHIFT;
		writel(slave_add_reg, i2c_dev->base + I2C_SLV_ADDR1);

		slave_add_reg = readl(i2c_dev->base + I2C_SLV_ADDR2);
		slave_add_reg &= ~I2C_SLV_ADDR2_ADDR0_MASK;
		writel(slave_add_reg, i2c_dev->base + I2C_SLV_ADDR2);
	}

	i2c_slv_config = I2C_SLV_CNFG_NEWSL;
	if (i2c_dev->slave_add) {
		i2c_slv_config = I2C_SLV_CNFG_ENABLE_SL |
				I2C_SLV_CNFG_PKT_MODE_EN |
				I2C_SLV_CNFG_FIFO_XFER_EN;
	}
	writel(i2c_slv_config, i2c_dev->base + I2C_SLV_CNFG);
}

static void copy_rx_data(struct tegra_i2c_slave_dev *i2c_dev, u8 rcv_char)
{
	if (get_space_count(i2c_dev->rx_msg_tail, i2c_dev->rx_msg_head,
			i2c_dev->rx_msg_buf_size)){
		i2c_dev->rx_msg_buff[i2c_dev->rx_msg_head++] = rcv_char;
		if (i2c_dev->rx_msg_head == i2c_dev->rx_msg_buf_size)
			i2c_dev->rx_msg_head = 0;
	} else {
		dev_warn(i2c_dev->dev, "The slave rx buffer is full, ignoring "
					"new receive data\n");
	}
}

static void handle_packet_first_byte_read(struct tegra_i2c_slave_dev *i2c_dev)
{
	unsigned long fifo_status;
	int filled_slots;
	unsigned long i2c_sl_config;
	unsigned long recv_data;

	fifo_status = readl(i2c_dev->base + I2C_FIFO_STATUS);
	filled_slots = (fifo_status & I2C_FIFO_STATUS_SLV_RX_MASK) >>
					I2C_FIFO_STATUS_SLV_RX_SHIFT;

	writel(I2C_INT_STATUS_RX_DATA_AVAILABLE,
				i2c_dev->base + I2C_INT_STATUS);
	if (unlikely(filled_slots != 1)) {
		dev_err(i2c_dev->dev, "Unexpected number of filed slots %d",
					filled_slots);
		BUG();
	}
	recv_data = readl(i2c_dev->base + I2C_SLV_RX_FIFO);
	copy_rx_data(i2c_dev, (u8)recv_data);

	i2c_dev->is_first_byte_read_wait = false;
	i2c_dev->curr_transfer = TRANSFER_STATE_READ;
	i2c_dev->curr_packet_bytes_read = 0;

	/* Write  packet Header */
	writel(i2c_dev->rx_pack_hdr1, i2c_dev->base + I2C_SLV_TX_FIFO);
	writel(i2c_dev->rx_pack_hdr2, i2c_dev->base + I2C_SLV_TX_FIFO);
	writel(i2c_dev->rx_pack_hdr3, i2c_dev->base + I2C_SLV_TX_FIFO);

	set_rx_trigger_level(i2c_dev, 4);
	i2c_dev->int_mask |= I2C_INT_SLV_RFIFO_DATA_REQ;
	writel(i2c_dev->int_mask, i2c_dev->base + I2C_INT_MASK);

	/* Ack the master */
	i2c_sl_config = readl(i2c_dev->base + I2C_SLV_CNFG);
	i2c_sl_config |= I2C_SLV_CNFG_ACK_LAST_BYTE |
				I2C_SLV_CNFG_ACK_LAST_BYTE_VALID;
	writel(i2c_sl_config, i2c_dev->base + I2C_SLV_CNFG);
}

static void handle_packet_byte_read(struct tegra_i2c_slave_dev *i2c_dev)
{
	unsigned long fifo_status;
	int i, j;
	int filled_slots;
	unsigned long recv_data;
	int curr_xfer_size;

	fifo_status = readl(i2c_dev->base + I2C_FIFO_STATUS);
	filled_slots = (fifo_status & I2C_FIFO_STATUS_SLV_RX_MASK) >>
					I2C_FIFO_STATUS_SLV_RX_SHIFT;

	curr_xfer_size = BYTES_PER_FIFO_WORD * filled_slots;
	if (i2c_dev->cont_status & I2C_SLV_TRANS_PREMATURE_END) {
		curr_xfer_size = readl(i2c_dev->base + I2C_SLV_PACKET_STATUS);
		curr_xfer_size =
			(curr_xfer_size & I2C_SLV_PACKET_STATUS_BYTENUM_MASK) >>
				I2C_SLV_PACKET_STATUS_BYTENUM_SHIFT;

		BUG_ON(filled_slots != ((curr_xfer_size -
				i2c_dev->curr_packet_bytes_read + 3) >> 2));
		curr_xfer_size -= i2c_dev->curr_packet_bytes_read;
	}

	i2c_dev->curr_packet_bytes_read += curr_xfer_size;
	for (i = 0; i < filled_slots; ++i) {
		recv_data = readl(i2c_dev->base + I2C_SLV_RX_FIFO);
		for (j = 0; j < BYTES_PER_FIFO_WORD; ++j) {
			copy_rx_data(i2c_dev, (u8)(recv_data >> j*8));
			curr_xfer_size--;
			if (!curr_xfer_size)
				break;
		}
	}
	if (i2c_dev->cont_status & I2C_SLV_TRANS_PREMATURE_END) {
		writel(I2C_SLV_TRANS_END | I2C_INT_STATUS_RX_BUFFER_FILLED,
					i2c_dev->base + I2C_INT_STATUS);

		i2c_dev->is_first_byte_read_wait = true;
		i2c_dev->curr_transfer = TRANSFER_STATE_NONE;
		i2c_dev->curr_packet_bytes_read = 0;
		set_rx_trigger_level(i2c_dev, 1);
		writel(0, i2c_dev->base + I2C_SLV_INT_MASK);
		i2c_dev->int_mask = I2C_SLV_DEFAULT_INT_MASK;
		writel(i2c_dev->int_mask, i2c_dev->base + I2C_INT_MASK);
	}
}

static void handle_rx_interrupt(struct tegra_i2c_slave_dev *i2c_dev)
{
	if (i2c_dev->is_first_byte_read_wait)
		handle_packet_first_byte_read(i2c_dev);
	else
		handle_packet_byte_read(i2c_dev);

	if (i2c_dev->is_rx_waiting) {
		complete(&i2c_dev->rx_msg_complete);
		i2c_dev->is_rx_waiting = false;
	}
}

static void handle_tx_transaction_end(struct tegra_i2c_slave_dev *i2c_dev)
{
	unsigned long curr_packet_size;

	i2c_dev->curr_transfer = TRANSFER_STATE_NONE;
	curr_packet_size = readl(i2c_dev->base + I2C_SLV_PACKET_STATUS);
	curr_packet_size =
		(curr_packet_size & I2C_SLV_PACKET_STATUS_BYTENUM_MASK) >>
				I2C_SLV_PACKET_STATUS_BYTENUM_SHIFT;

	/* Get transfer count from request size.*/
	if ((curr_packet_size == 0) &&
		   (i2c_dev->cont_status & I2C_SLV_TRANS_ALL_XFER_END) &&
		    (!(i2c_dev->cont_status & I2C_SLV_TRANS_PREMATURE_END))) {
		if (!i2c_dev->is_dummy_char_cycle)
			i2c_dev->tx_msg_tail = i2c_dev->curr_packet_tx_tail;
	} else {
		if (!i2c_dev->is_dummy_char_cycle) {
			i2c_dev->tx_msg_tail += curr_packet_size;
			if (i2c_dev->tx_msg_tail >= i2c_dev->tx_msg_buf_size)
				i2c_dev->tx_msg_tail -=
						i2c_dev->tx_msg_buf_size;
		}
	}
	writel(I2C_SLV_TRANS_END, i2c_dev->base + I2C_INT_STATUS);

	i2c_dev->curr_transfer = TRANSFER_STATE_NONE;
	set_tx_trigger_level(i2c_dev, 1);
	writel(0, i2c_dev->base + I2C_SLV_INT_MASK);
	i2c_dev->int_mask = I2C_SLV_DEFAULT_INT_MASK;
	writel(i2c_dev->int_mask, i2c_dev->base + I2C_INT_MASK);
	if (i2c_dev->is_tx_waiting) {
		complete(&i2c_dev->tx_msg_complete);
		i2c_dev->is_tx_waiting = false;
	}
}

static void handle_tx_trigger_int(struct tegra_i2c_slave_dev *i2c_dev)
{
	unsigned long fifo_status;
	int empty_slots;
	int i, j;
	int data_available;
	unsigned long header1, header2, header3;
	unsigned long tx_data;
	int word_to_write;
	int bytes_remain;
	int bytes_in_curr_word;
	int tx_tail;
	int packet_len;

	fifo_status = readl(i2c_dev->base + I2C_FIFO_STATUS);
	empty_slots = (fifo_status & I2C_FIFO_STATUS_SLV_TX_MASK) >>
					I2C_FIFO_STATUS_SLV_TX_SHIFT;
	BUG_ON(empty_slots <= 3);
	if (i2c_dev->curr_transfer == TRANSFER_STATE_NONE) {
		empty_slots -= 3;

		/* Clear the tfifo request. */
		writel(I2C_INT_STATUS_TX_BUFFER_REQUEST,
					i2c_dev->base + I2C_INT_STATUS);

		/* Get Number of bytes it can transfer in current */
		data_available = get_data_count(i2c_dev->tx_msg_tail,
			i2c_dev->tx_msg_head, i2c_dev->tx_msg_buf_size);
		if (data_available)
			packet_len = min(empty_slots*BYTES_PER_FIFO_WORD,
							 data_available);
		else
			packet_len = empty_slots*BYTES_PER_FIFO_WORD;

		get_packet_headers(i2c_dev, packet_len, I2C_M_RD,
					&header1, &header2, &header3);

		/* Write  packet Header */
		writel(header1, i2c_dev->base + I2C_SLV_TX_FIFO);
		writel(header2, i2c_dev->base + I2C_SLV_TX_FIFO);
		writel(header3, i2c_dev->base + I2C_SLV_TX_FIFO);

		fifo_status = readl(i2c_dev->base + I2C_FIFO_STATUS);
		if (data_available) {
			word_to_write = (packet_len + 3) >> 2;
			bytes_remain = packet_len;
			tx_tail = i2c_dev->tx_msg_tail;
			for (i = 0; i < word_to_write; i++) {
				bytes_in_curr_word =
					 min(bytes_remain, BYTES_PER_FIFO_WORD);
				tx_data = 0;
				for (j = 0; j < bytes_in_curr_word; ++j) {
					tx_data |= (i2c_dev->tx_msg_buff[
						  tx_tail++]<<(j*8));
					if (tx_tail >= i2c_dev->tx_msg_buf_size)
						tx_tail = 0;
				}
				writel(tx_data, i2c_dev->base +
							I2C_SLV_TX_FIFO);
				bytes_remain -= bytes_in_curr_word;
			}
			i2c_dev->curr_packet_tx_tail = tx_tail;
			i2c_dev->is_dummy_char_cycle = false;
		} else {
			i2c_dev->curr_packet_tx_tail = i2c_dev->tx_msg_tail;
			for (i = 0; i < empty_slots; i++)
				writel(i2c_dev->dummy_word,
					i2c_dev->base + I2C_SLV_TX_FIFO);
			i2c_dev->is_dummy_char_cycle = true;
		}

		i2c_dev->curr_transfer = TRANSFER_STATE_WRITE;
		i2c_dev->int_mask &= ~I2C_INT_SLV_TFIFO_DATA_REQ;
		i2c_dev->int_mask |= I2C_SLV_TRANS_END;
		writel(i2c_dev->int_mask, i2c_dev->base + I2C_INT_MASK);
	} else {
		dev_err(i2c_dev->dev, "I2cSlaveIsr(): Illegal transfer at "
				"this point\n");
		BUG();
	}
}

static void handle_tx_interrupt(struct tegra_i2c_slave_dev *i2c_dev)
{
	if (i2c_dev->cont_status & I2C_SLV_TRANS_END)
		handle_tx_transaction_end(i2c_dev);
	else
		handle_tx_trigger_int(i2c_dev);
}

static irqreturn_t tegra_i2c_slave_isr(int irq, void *dev_id)
{
	struct tegra_i2c_slave_dev *i2c_dev = dev_id;
	unsigned long flags;

	/* Read the Interrupt status register & PKT_STATUS */
	i2c_dev->cont_status = readl(i2c_dev->base + I2C_INT_STATUS);

	dev_dbg(i2c_dev->dev, "ISR ContStatus 0x%08x\n", i2c_dev->cont_status);
	spin_lock_irqsave(&i2c_dev->lock, flags);

	if ((i2c_dev->cont_status & I2C_INT_STATUS_RX_DATA_AVAILABLE) ||
		(i2c_dev->curr_transfer == TRANSFER_STATE_READ)) {
		handle_rx_interrupt(i2c_dev);
		goto Done;
	}

	if ((i2c_dev->cont_status & I2C_INT_STATUS_TX_BUFFER_REQUEST) ||
		(i2c_dev->curr_transfer == TRANSFER_STATE_WRITE)) {
		handle_tx_interrupt(i2c_dev);
		goto Done;
	}

	dev_err(i2c_dev->dev, "Tegra I2c Slave got unwanted interrupt "
			"IntStatus 0x%08x\n", i2c_dev->cont_status);
	BUG();

Done:
	spin_unlock_irqrestore(&i2c_dev->lock, flags);
	return IRQ_HANDLED;
}

static int tegra_i2c_slave_start(struct i2c_slave_adapter *slv_adap, int addr,
		int is_ten_bit_addr, unsigned char dummy_char)
{
	struct tegra_i2c_slave_bus *i2c_bus = i2c_get_slave_adapdata(slv_adap);
	struct tegra_i2c_slave_dev *i2c_dev = i2c_bus->dev;
	unsigned long flags;

	spin_lock_irqsave(&i2c_dev->lock, flags);
	if (i2c_dev->is_slave_started) {
		spin_unlock_irqrestore(&i2c_dev->lock, flags);
		return -EBUSY;
	}

	i2c_dev->rx_msg_buff = (u8 *)(i2c_dev+1);
	i2c_dev->rx_msg_head = 0;
	i2c_dev->rx_msg_tail = 0;
	i2c_dev->is_rx_waiting = false;

	i2c_dev->tx_msg_head = 0;
	i2c_dev->tx_msg_tail = 0;
	i2c_dev->is_tx_waiting = true;

	i2c_dev->dummy_word =  (dummy_char << 8) | dummy_char;
	i2c_dev->dummy_word |=  i2c_dev->dummy_word << 16;

	i2c_dev->slave_add = addr;
	i2c_dev->is_ten_bit_addr = is_ten_bit_addr;

	get_packet_headers(i2c_dev, 4096, 0, &i2c_dev->rx_pack_hdr1,
		&i2c_dev->rx_pack_hdr2, &i2c_dev->rx_pack_hdr3);

	pm_runtime_get_sync(i2c_dev->dev);
	configure_i2c_slave_packet_mode(i2c_dev);
	configure_i2c_slave_address(i2c_dev);
	do_tx_fifo_empty(i2c_dev, NULL);
	set_rx_trigger_level(i2c_dev, 1);
	writel(0, i2c_dev->base + I2C_SLV_INT_MASK);

	if (i2c_bus->pinmux)
		tegra_pinmux_config_tristate_table(i2c_bus->pinmux,
				i2c_bus->mux_len, TEGRA_TRI_NORMAL);

	i2c_dev->curr_transfer = 0;
	i2c_dev->is_slave_started = true;
	i2c_dev->int_mask =  I2C_SLV_DEFAULT_INT_MASK;
	i2c_dev->is_first_byte_read_wait = true;
	writel(i2c_dev->int_mask, i2c_dev->base + I2C_INT_MASK);
	spin_unlock_irqrestore(&i2c_dev->lock, flags);
	return 0;
}

static void tegra_i2c_slave_stop(struct i2c_slave_adapter *slv_adap,
		int is_buffer_clear)
{
	struct tegra_i2c_slave_bus *i2c_bus = i2c_get_slave_adapdata(slv_adap);
	struct tegra_i2c_slave_dev *i2c_dev = i2c_bus->dev;
	unsigned long flags;

	spin_lock_irqsave(&i2c_dev->lock, flags);
	if (!i2c_dev->is_slave_started) {
		spin_unlock_irqrestore(&i2c_dev->lock, flags);
		return;
	}

	i2c_dev->slave_add = 0;
	i2c_dev->is_ten_bit_addr = false;
	configure_i2c_slave_address(i2c_dev);
	writel(0, i2c_dev->base + I2C_SLV_INT_MASK);
	writel(0, i2c_dev->base + I2C_INT_MASK);
	i2c_dev->curr_transfer = 0;
	i2c_dev->is_slave_started = false;
	pm_runtime_put_sync(i2c_dev->dev);
	if (is_buffer_clear) {
		i2c_dev->rx_msg_head = 0;
		i2c_dev->rx_msg_tail = 0;
		i2c_dev->is_rx_waiting = false;
		i2c_dev->tx_msg_head = 0;
		i2c_dev->tx_msg_tail = 0;
		i2c_dev->is_tx_waiting = false;
	}
	if (i2c_bus->pinmux)
		tegra_pinmux_config_tristate_table(i2c_bus->pinmux,
				i2c_bus->mux_len, TEGRA_TRI_TRISTATE);
	spin_unlock_irqrestore(&i2c_dev->lock, flags);
}

static int tegra_i2c_slave_send(struct i2c_slave_adapter *slv_adap,
		const char *buf, int count)
{
	struct tegra_i2c_slave_bus *i2c_bus = i2c_get_slave_adapdata(slv_adap);
	struct tegra_i2c_slave_dev *i2c_dev = i2c_bus->dev;
	unsigned long flags;
	unsigned long space_available;
	int i;

	spin_lock_irqsave(&i2c_dev->lock, flags);
	if (!i2c_dev->is_slave_started) {
		spin_unlock_irqrestore(&i2c_dev->lock, flags);
		return -EPERM;
	}

	space_available = get_space_count(i2c_dev->tx_msg_tail,
				i2c_dev->tx_msg_head, i2c_dev->tx_msg_buf_size);
	if (space_available < count) {
		spin_unlock_irqrestore(&i2c_dev->lock, flags);
		return 0;
	}

	for (i = 0; i < count; ++i) {
		i2c_dev->tx_msg_buff[i2c_dev->tx_msg_head++] = *buf++;
		if (i2c_dev->tx_msg_head >= i2c_dev->tx_msg_buf_size)
			i2c_dev->tx_msg_head = 0;
	}
	i2c_dev->is_tx_waiting = false;
	spin_unlock_irqrestore(&i2c_dev->lock, flags);
	return count;
}

static int tegra_i2c_slave_get_tx_status(struct i2c_slave_adapter *slv_adap,
		int timeout_ms)
{
	struct tegra_i2c_slave_bus *i2c_bus = i2c_get_slave_adapdata(slv_adap);
	struct tegra_i2c_slave_dev *i2c_dev = i2c_bus->dev;
	unsigned long flags;
	unsigned long data_available;

	spin_lock_irqsave(&i2c_dev->lock, flags);
	if (!i2c_dev->is_slave_started) {
		spin_unlock_irqrestore(&i2c_dev->lock, flags);
		return -EPERM;
	}

	data_available = get_data_count(i2c_dev->tx_msg_tail,
				i2c_dev->tx_msg_head, i2c_dev->tx_msg_buf_size);
	if (!data_available) {
		spin_unlock_irqrestore(&i2c_dev->lock, flags);
		return 0;
	}

	INIT_COMPLETION(i2c_dev->tx_msg_complete);
	if (timeout_ms)
		i2c_dev->is_tx_waiting = true;
	spin_unlock_irqrestore(&i2c_dev->lock, flags);
	if (timeout_ms) {
		wait_for_completion_timeout(&i2c_dev->tx_msg_complete,
					to_jiffies(timeout_ms));
		spin_lock_irqsave(&i2c_dev->lock, flags);
		i2c_dev->is_tx_waiting = false;
		data_available = get_data_count(i2c_dev->tx_msg_tail,
					i2c_dev->tx_msg_head,
					i2c_dev->tx_msg_buf_size);
		spin_unlock_irqrestore(&i2c_dev->lock, flags);
		if (data_available)
			return -ETIMEDOUT;
	}
	return data_available;
}

/*
 * Timeoutms = 0, MinBytesRead = 0, read without waiting.
 * Timeoutms = 0, MinBytesRead != 0, block till min bytes read.
 * Timeoutms != 0, wait till timeout to read data..
 * Timeoutms  = INF, wait till all req bytes read.
 */

static int tegra_i2c_slave_recv(struct i2c_slave_adapter *slv_adap, char *buf,
		int count, int min_count, int timeout_ms)
{
	struct tegra_i2c_slave_bus *i2c_bus = i2c_get_slave_adapdata(slv_adap);
	struct tegra_i2c_slave_dev *i2c_dev = i2c_bus->dev;
	unsigned long flags;
	int data_available;
	int bytes_copy;
	int i;
	int read_count = 0;
	bool is_inf_wait = false;
	int run_count = 0;

	spin_lock_irqsave(&i2c_dev->lock, flags);
	if (!i2c_dev->is_slave_started) {
		spin_unlock_irqrestore(&i2c_dev->lock, flags);
		return -EPERM;
	}

	do {
		data_available = get_data_count(i2c_dev->rx_msg_tail,
				i2c_dev->rx_msg_head, i2c_dev->rx_msg_buf_size);

		bytes_copy = min(data_available, count);

		if (!data_available) {
			spin_unlock_irqrestore(&i2c_dev->lock, flags);
			return 0;
		}
		for (i = 0; i < bytes_copy; ++i) {
			*buf++ = i2c_dev->rx_msg_buff[i2c_dev->rx_msg_tail++];
			if (i2c_dev->rx_msg_tail >= i2c_dev->rx_msg_buf_size)
				i2c_dev->rx_msg_tail = 0;
			read_count++;
		}
		if (!timeout_ms) {
			if ((!min_count) || (read_count >= min_count))
				break;
			is_inf_wait = true;
		} else {
			if ((read_count == count) || run_count)
				break;
		}
		i2c_dev->is_rx_waiting = true;
		INIT_COMPLETION(i2c_dev->rx_msg_complete);
		spin_unlock_irqrestore(&i2c_dev->lock, flags);
		if (is_inf_wait)
			wait_for_completion(&i2c_dev->rx_msg_complete);
		else
			wait_for_completion_timeout(&i2c_dev->rx_msg_complete,
						to_jiffies(timeout_ms));

		spin_lock_irqsave(&i2c_dev->lock, flags);
	} while (1);
	spin_unlock_irqrestore(&i2c_dev->lock, flags);
	i2c_dev->is_rx_waiting = false;
	return read_count;
}

static int tegra_i2c_slave_flush_buffer(struct i2c_slave_adapter *slv_adap,
		int is_flush_tx_buffer, int is_flush_rx_buffer)
{
	struct tegra_i2c_slave_bus *i2c_bus = i2c_get_slave_adapdata(slv_adap);
	struct tegra_i2c_slave_dev *i2c_dev = i2c_bus->dev;
	unsigned long flags;

	spin_lock_irqsave(&i2c_dev->lock, flags);
	if (!i2c_dev->is_slave_started) {
		spin_unlock_irqrestore(&i2c_dev->lock, flags);
		return -EPERM;
	}
	if (is_flush_tx_buffer) {
		i2c_dev->tx_msg_head = 0;
		i2c_dev->tx_msg_tail = 0;
	}
	if (is_flush_rx_buffer) {
		i2c_dev->rx_msg_head = 0;
		i2c_dev->rx_msg_tail = 0;
	}
	spin_unlock_irqrestore(&i2c_dev->lock, flags);
	return 0;
}

static int tegra_i2c_slave_get_nack_cycle(struct i2c_slave_adapter *slv_adap,
		int is_cout_reset)
{
	struct tegra_i2c_slave_bus *i2c_bus = i2c_get_slave_adapdata(slv_adap);
	struct tegra_i2c_slave_dev *i2c_dev = i2c_bus->dev;
	unsigned long flags;
	int retval;

	spin_lock_irqsave(&i2c_dev->lock, flags);
	if (!i2c_dev->is_slave_started) {
		spin_unlock_irqrestore(&i2c_dev->lock, flags);
		dev_dbg(i2c_dev->dev, "The slave bus is already started\n");
		return -EPERM;
	}

	retval = i2c_dev->nack_packet_count;
	if (is_cout_reset)
		i2c_dev->nack_packet_count = 0;

	spin_unlock_irqrestore(&i2c_dev->lock, flags);
	return retval;
}

static const struct i2c_slave_algorithm tegra_i2c_slave_algo = {
	.slave_start		= tegra_i2c_slave_start,
	.slave_stop		= tegra_i2c_slave_stop,
	.slave_send		= tegra_i2c_slave_send,
	.slave_get_tx_status	= tegra_i2c_slave_get_tx_status,
	.slave_recv		= tegra_i2c_slave_recv,
	.slave_flush_buffer	= tegra_i2c_slave_flush_buffer,
	.slave_get_nack_cycle	= tegra_i2c_slave_get_nack_cycle,
};

static int tegra_i2c_slave_probe(struct platform_device *pdev)
{
	struct tegra_i2c_slave_dev *i2c_dev;
	struct tegra_i2c_slave_bus *i2c_bus = NULL;
	struct tegra_i2c_slave_platform_data *pdata = pdev->dev.platform_data;
	struct resource *res;
	struct resource *iomem;
	struct clk *clk;
	void *base;
	int irq;
	int ret = 0;
	int rx_buffer_size;
	int tx_buffer_size;

	if (!pdata) {
		dev_err(&pdev->dev, "no platform data?\n");
		return -ENODEV;
	}

	if (pdata->adapter_nr < 0) {
		dev_err(&pdev->dev, "invalid platform data?\n");
		return -ENODEV;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "no mem resource?\n");
		return -ENODEV;
	}
	iomem = request_mem_region(res->start, resource_size(res), pdev->name);
	if (!iomem) {
		dev_err(&pdev->dev, "I2C region already claimed\n");
		return -EBUSY;
	}

	base = ioremap(iomem->start, resource_size(iomem));
	if (!base) {
		dev_err(&pdev->dev, "Can't ioremap I2C region\n");
		return -ENOMEM;
	}

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		dev_err(&pdev->dev, "no irq resource?\n");
		ret = -ENODEV;
		goto err_iounmap;
	}
	irq = res->start;

	clk = clk_get(&pdev->dev, NULL);
	if (!clk) {
		ret = -ENODEV;
		goto err_release_region;
	}

	rx_buffer_size = (pdata->max_rx_buffer_size)?:4096;
	tx_buffer_size = (pdata->max_tx_buffer_size)?:4096;
	i2c_dev = kzalloc(sizeof(struct tegra_i2c_slave_dev) +
			rx_buffer_size + tx_buffer_size, GFP_KERNEL);
	if (!i2c_dev) {
		ret = -ENOMEM;
		goto err_clk_put;
	}

	i2c_dev->base = base;
	i2c_dev->clk = clk;
	i2c_dev->iomem = iomem;
	i2c_dev->irq = irq;
	i2c_dev->cont_id = pdev->id;
	i2c_dev->dev = &pdev->dev;
	i2c_dev->bus_clk = pdata->bus_clk_rate?: 100000;
	i2c_dev->rx_msg_buff = (u8 *)(i2c_dev+1);
	i2c_dev->rx_msg_buf_size = rx_buffer_size;
	i2c_dev->rx_msg_head = 0;
	i2c_dev->rx_msg_tail = 0;
	i2c_dev->is_rx_waiting = 0;
	i2c_dev->tx_msg_buff = i2c_dev->rx_msg_buff + rx_buffer_size;
	i2c_dev->tx_msg_buf_size = tx_buffer_size;
	i2c_dev->tx_msg_head = 0;
	i2c_dev->tx_msg_tail = 0;
	i2c_dev->is_tx_waiting = 0;

	i2c_dev->is_slave_started = false;
	spin_lock_init(&i2c_dev->lock);

	init_completion(&i2c_dev->rx_msg_complete);
	init_completion(&i2c_dev->tx_msg_complete);

	platform_set_drvdata(pdev, i2c_dev);

	ret = request_irq(i2c_dev->irq, tegra_i2c_slave_isr, IRQF_DISABLED,
					pdev->name, i2c_dev);
	if (ret) {
		dev_err(&pdev->dev, "Failed to request irq %i\n", i2c_dev->irq);
		goto err_free;
	}

	i2c_bus = &i2c_dev->bus;
	i2c_bus->dev = i2c_dev;
	i2c_bus->pinmux = pdata->pinmux;
	i2c_bus->mux_len = pdata->bus_mux_len;
	i2c_bus->bus_clk_rate = pdata->bus_clk_rate ?: 100000;

	i2c_bus->slv_adap.slv_algo = &tegra_i2c_slave_algo;
	i2c_bus->slv_adap.owner = THIS_MODULE;
	i2c_bus->slv_adap.class = I2C_CLASS_HWMON;
	strlcpy(i2c_bus->slv_adap.name, "Tegra I2C SLAVE adapter",
				sizeof(i2c_bus->slv_adap.name));
	i2c_bus->slv_adap.parent_dev = &pdev->dev;
	i2c_bus->slv_adap.dev = NULL;
	i2c_bus->slv_adap.nr = pdata->adapter_nr;
	ret = i2c_add_slave_adapter(&i2c_bus->slv_adap, true);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to add I2C adapter\n");
		goto err_free_irq;
	}
	i2c_set_slave_adapdata(&i2c_bus->slv_adap, i2c_bus);
	dev_dbg(&pdev->dev, "%s() suucess\n", __func__);
	pm_runtime_enable(i2c_dev->dev);
	return 0;

err_free_irq:
	free_irq(i2c_dev->irq, i2c_dev);
err_free:
	kfree(i2c_dev);
err_clk_put:
	clk_put(clk);
err_release_region:
	release_mem_region(iomem->start, resource_size(iomem));
err_iounmap:
	iounmap(base);
	dev_dbg(&pdev->dev, "%s() failed %d\n", __func__, ret);
	return ret;
}

static int tegra_i2c_slave_remove(struct platform_device *pdev)
{
	struct tegra_i2c_slave_dev *i2c_dev = platform_get_drvdata(pdev);

	i2c_del_slave_adapter(&i2c_dev->bus.slv_adap);
	pm_runtime_disable(i2c_dev->dev);
	free_irq(i2c_dev->irq, i2c_dev);
	clk_put(i2c_dev->clk);
	release_mem_region(i2c_dev->iomem->start,
			resource_size(i2c_dev->iomem));
	iounmap(i2c_dev->base);
	kfree(i2c_dev);
	return 0;
}

#ifdef CONFIG_PM
static int tegra_i2c_slave_suspend(struct platform_device *pdev,
	pm_message_t state)
{
	return 0;
}

static int tegra_i2c_slave_resume(struct platform_device *pdev)
{
	return 0;
}
#endif
#if defined(CONFIG_PM_RUNTIME)
static int tegra_i2c_slave_runtime_idle(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct tegra_i2c_slave_dev *i2c_dev = platform_get_drvdata(pdev);
	clk_disable_unprepare(i2c_dev->clk);
	return 0;
}
static int tegra_i2c_slave_runtime_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct tegra_i2c_slave_dev *i2c_dev = platform_get_drvdata(pdev);
	clk_prepare_enable(i2c_dev->clk);
	return 0;
}
static const struct dev_pm_ops tegra_i2c_slave_dev_pm_ops = {
	.runtime_idle = tegra_i2c_slave_runtime_idle,
	.runtime_resume = tegra_i2c_slave_runtime_resume,
};
#endif

static struct platform_driver tegra_i2c_slave_driver = {
	.probe   = tegra_i2c_slave_probe,
	.remove  = tegra_i2c_slave_remove,
#ifdef CONFIG_PM
	.suspend = tegra_i2c_slave_suspend,
	.resume  = tegra_i2c_slave_resume,
#endif
	.driver  = {
		.name  = "tegra-i2c-slave",
		.owner = THIS_MODULE,
#if defined(CONFIG_PM_RUNTIME)
		.pm = &tegra_i2c_slave_dev_pm_ops,
#endif
	},
};

static int __init tegra_i2c_slave_init_driver(void)
{
	return platform_driver_register(&tegra_i2c_slave_driver);
}

static void __exit tegra_i2c_slave_exit_driver(void)
{
	platform_driver_unregister(&tegra_i2c_slave_driver);
}
subsys_initcall(tegra_i2c_slave_init_driver);
module_exit(tegra_i2c_slave_exit_driver);
