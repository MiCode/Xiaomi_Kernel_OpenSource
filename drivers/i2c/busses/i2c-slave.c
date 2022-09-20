// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/clk.h>
#include <linux/interconnect.h>
#include <linux/io.h>
#include <linux/ipc_logging.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include "i2c-slave.h"

#define CREATE_TRACE_POINTS
#include <trace/i2c_slave_trace.h>

/**
 * i2c_slave_trace_log: FTRACE Logging.
 * @dev: Driver model device node for the i2c slave.
 * @fmt: ftrace log format.
 *
 * This function will add logs to ftrace
 * file.
 *
 * Return: None.
 */
void i2c_slave_trace_log(struct device *dev, const char *fmt, ...)
{
	struct va_format vaf = {
		.fmt = fmt,
	};

	va_list args;

	va_start(args, fmt);
	vaf.va = &args;
	trace_i2c_slave_log_info(dev_name(dev), &vaf);
	va_end(args);
}

/**
 * dump_register - To dump the register value.
 * @i2c_slave: Pointer to Main Structure.
 *
 * This function will dump the all readable register
 * value to IPC log.
 *
 * Return: None.
 */
static void dump_register(struct i2c_slave *i2c_slave)
{
	unsigned int temp;

	temp = readl_relaxed(i2c_slave->base + I2C_S_DEVICE_ADDR);
	I2C_SLAVE_DBG(i2c_slave->ipcl, false, i2c_slave->dev,
		      "I2C_S_DEVICE_ADDR: 0x%x\n", temp);

	temp = readl_relaxed(i2c_slave->base + I2C_S_IRQ_STATUS);
	I2C_SLAVE_DBG(i2c_slave->ipcl, false, i2c_slave->dev,
		      "I2C_S_IRQ_STATUS: 0x%x\n", temp);

	temp = readl_relaxed(i2c_slave->base + I2C_S_CONFIG);
	I2C_SLAVE_DBG(i2c_slave->ipcl, false, i2c_slave->dev,
		      "I2C_S_CONFIG: 0x%x\n", temp);

	temp = readl_relaxed(i2c_slave->base + I2C_S_IRQ_EN);
	I2C_SLAVE_DBG(i2c_slave->ipcl, false, i2c_slave->dev,
		      "I2C_S_IRQ_EN: 0x%x\n", temp);

	temp = readl_relaxed(i2c_slave->base + I2C_S_FIFOS_STATUS);
	I2C_SLAVE_DBG(i2c_slave->ipcl, false, i2c_slave->dev,
		      "I2C_S_FIFOS_STATUS: 0x%x\n", temp);

	temp = readl_relaxed(i2c_slave->base + I2C_S_DEBUG_REG1);
	I2C_SLAVE_DBG(i2c_slave->ipcl, false, i2c_slave->dev,
		      "I2C_S_DEBUG_REG1: 0x%x\n", temp);

	temp = readl_relaxed(i2c_slave->base + I2C_S_DEBUG_REG2);
	I2C_SLAVE_DBG(i2c_slave->ipcl, false, i2c_slave->dev,
		      "I2C_S_DEBUG_REG2: 0x%x\n", temp);

	temp = readl_relaxed(i2c_slave->base +  I2C_S_CLK_LOW_TIMEOUT);
	I2C_SLAVE_DBG(i2c_slave->ipcl, false, i2c_slave->dev,
		      "I2C_S_CLK_LOW_TIMEOUT: 0x%x\n", temp);

	temp = readl_relaxed(i2c_slave->base +  I2C_S_CLK_RELEASE_DELAY_CNT_VAL);
	I2C_SLAVE_DBG(i2c_slave->ipcl, false, i2c_slave->dev,
		      "I2C_S_CLK_RELEASE_DELAY_CNT_VAL: 0x%x\n", temp);

	temp = readl_relaxed(i2c_slave->base +  I2C_S_SDA_HOLD_CNT_VAL);
	I2C_SLAVE_DBG(i2c_slave->ipcl, false, i2c_slave->dev,
		      "I2C_S_SDA_HOLD_CNT_VAL: 0x%x\n", temp);

	temp = readl_relaxed(i2c_slave->base +  SMBALERT_STATUS_REG);
	I2C_SLAVE_DBG(i2c_slave->ipcl, false, i2c_slave->dev,
		      "SMBALERT_STATUS_REG: 0x%x\n", temp);

	temp = readl_relaxed(i2c_slave->base +  SMBALERT_PEC_REG);
	I2C_SLAVE_DBG(i2c_slave->ipcl, false, i2c_slave->dev,
		      "SMBALERT_PEC_REG: 0x%x\n", temp);
}

/**
 * read_tx_fifo_byte_count: To read TX FIFO count.
 * @i2c_slave: Pointer to Main Structure.
 *
 * This function will read the number of bytes written
 * to the TX FIFO.
 *
 * Return: Number of bytes written to the TX FIFO.
 */
static unsigned int read_tx_fifo_byte_count(struct i2c_slave *i2c_slave)
{
	unsigned int count = 0;

	count = readl_relaxed(i2c_slave->base + I2C_S_FIFOS_STATUS);
	return (count & 0xFFFF);
}

/**
 * read_rx_fifo_byte_count: To read RX FIFO count.
 * @i2c_slave: Pointer to Main Structure.
 *
 * This function will read the number of bytes written
 * to the RX FIFO.
 *
 * Return: Number of bytes written to the RX FIFO.
 */
static unsigned int read_rx_fifo_byte_count(struct i2c_slave *i2c_slave)
{
	unsigned int count = 0;

	count = readl_relaxed(i2c_slave->base + I2C_S_FIFOS_STATUS);
	return ((count & 0xFFFF0000) >> 16);
}

/**
 * i2c_slave_write_fifo: This function will write data to TX FIFO.
 * @i2c_slave: Pointer to Main Structure.
 *
 * This function will write data to TX FIFO.
 *
 * Return: None.
 */
static void i2c_slave_write_fifo(struct i2c_slave *i2c_slave)
{
	int i;

	if (i2c_slave->tx_count == 0) {
		I2C_SLAVE_ERR(i2c_slave->ipcl, true, i2c_slave->dev,
			      "TX FIFO write count is zero\n");
		return;
	}

	for (i = 0; i < i2c_slave->tx_count; i++) {
		I2C_SLAVE_DBG(i2c_slave->ipcl, false, i2c_slave->dev,
			      "Data to Tx FIFO: 0x%x\n", i2c_slave->tx_msg_buf[i]);
		writel_relaxed(i2c_slave->tx_msg_buf[i],
			       i2c_slave->base + I2C_S_TX_FIFO);
	}

	i2c_slave->tx_count = 0;
}

/**
 * i2c_slave_read_fifo: This function will write data to RX FIFO.
 * @i2c_slave: Pointer to Main Structure.
 *
 * This function will write data to RX FIFO.
 *
 * Return: None.
 */
static void i2c_slave_read_fifo(struct i2c_slave *i2c_slave)
{
	unsigned int rx_data_count;
	int i;

	rx_data_count = read_rx_fifo_byte_count(i2c_slave);
	if (rx_data_count == 0) {
		I2C_SLAVE_ERR(i2c_slave->ipcl, true, i2c_slave->dev,
			      "RX FIFO empty\n");
	} else if (i2c_slave->rx_count >= I2C_SLAVE_MAX_MSG_SIZE) {
		I2C_SLAVE_ERR(i2c_slave->ipcl, true, i2c_slave->dev,
			      "RX data buffer full\n");
	} else {
		for (i = 0; i < rx_data_count &&
		     i2c_slave->rx_count < I2C_SLAVE_MAX_MSG_SIZE; i++) {
			i2c_slave->rx_msg_buf[i2c_slave->rx_count] =
					readl_relaxed(i2c_slave->base + I2C_S_RX_FIFO);
			I2C_SLAVE_DBG(i2c_slave->ipcl, false, i2c_slave->dev,
				      "Data from RX_FIFO: 0x%x",
				      i2c_slave->rx_msg_buf[i2c_slave->rx_count]);
			i2c_slave->rx_count++;
		}
	}
}

/**
 * i2c_slave_write_bit: To SET the register bit.
 * @i2c_slave: Pointer to Main Structure.
 * @reg: offset of Register address.
 * @bit: bit to modify.
 *
 * This function will set the bit to given register.
 *
 * Return: None.
 */

static void i2c_slave_write_bit(struct i2c_slave *i2c_slave, int reg, int bit)
{
	unsigned int temp;

	temp = readl_relaxed(i2c_slave->base + reg) | bit;
	writel_relaxed(temp, i2c_slave->base + reg);
}

/**
 * i2c_slave_send_ack: To send ACK to master.
 * @i2c_slave: Pointer to Main Structure.
 *
 * This function will send ACK to master.
 *
 * Return: None.
 */
static void i2c_slave_send_ack(struct i2c_slave *i2c_slave)
{
	writel_relaxed(ACK_RESUME, i2c_slave->base + I2C_S_CONTROL);
}

/**
 * i2c_slave_send_nack: To send NACK to master.
 * @i2c_slave: Pointer to Main Structure.
 *
 * This function will send NACK to master.
 *
 * Return: None.
 */

static void i2c_slave_send_nack(struct i2c_slave *i2c_slave)
{
	writel_relaxed(NACK, i2c_slave->base + I2C_S_CONTROL);
}

/**
 * i2c_slave_clear_irq: To clear IRQ bit
 * @i2c_slave: Pointer to Main Structure.
 * enum: enum to IRQ bit
 *
 * This function will clear the handled IRQ bit.
 *
 * Return: None.
 */
static void i2c_slave_clear_irq(struct i2c_slave *i2c_slave,
				enum i2c_slave_irq_status bit)
{
	if (bit == ALL_IRQ)
		writel_relaxed(ALL_IRQ, i2c_slave->base + I2C_S_IRQ_CLR);
	else
		writel_relaxed(1 << bit, i2c_slave->base + I2C_S_IRQ_CLR);
}

/**
 * i2c_slave_enable_irq: To enable IRQ.
 * @i2c_slave: Pointer to Main Structure.
 *
 * This function will enable the IRQ.
 *
 * Return: None.
 */
static void i2c_slave_enable_irq(struct i2c_slave *i2c_slave)
{
	writel_relaxed(ALL_IRQ, i2c_slave->base + I2C_S_IRQ_EN);
}

/**
 * i2c_slave_irq: I2C Slave IQR handler
 * @irq: IRQ number
 * @dev: Pointer to Dev Structure.
 *
 * This function will handle IRQ bits.
 *
 * return: IRQ_HANDLED for success.
 */
static irqreturn_t i2c_slave_irq(int irq, void *dev)
{
	unsigned int irq_stat;
	struct i2c_slave *i2c_slave = dev;

	irq_stat = readl_relaxed(i2c_slave->base + I2C_S_IRQ_STATUS);
	I2C_SLAVE_DBG(i2c_slave->ipcl, false, i2c_slave->dev,
		      "irq status: 0x%x\n", irq_stat);

	if (irq_stat & (1 << ERR_CONDITION)) {
		I2C_SLAVE_DBG(i2c_slave->ipcl, false, i2c_slave->dev, "%s\n",
			      irq_log[ERR_CONDITION]);
		dump_register(i2c_slave);
		i2c_slave_write_bit(i2c_slave, I2C_S_SW_RESET_REG, SW_RESET);
		i2c_slave_clear_irq(i2c_slave, ALL_IRQ);
		i2c_slave_enable_irq(i2c_slave);
		i2c_slave_write_bit(i2c_slave, I2C_S_CONTROL,
				    CLEAR_TX_FIFO | CLEAR_RX_FIFO);
		i2c_slave_write_bit(i2c_slave, I2C_S_CLK_LOW_TIMEOUT, TIMER_MODE);
		i2c_slave_write_bit(i2c_slave, I2C_S_CONFIG, CORE_EN);
		i2c_slave_send_nack(i2c_slave);
	}

	if (irq_stat & (1 << CLOCK_LOW_TIMEOUT)) {
		I2C_SLAVE_DBG(i2c_slave->ipcl, false, i2c_slave->dev, "%s\n",
			      irq_log[CLOCK_LOW_TIMEOUT]);
		i2c_slave_clear_irq(i2c_slave, ALL_IRQ);
	}

	if (irq_stat & (1 << STOP_DETECTED)) {
		I2C_SLAVE_DBG(i2c_slave->ipcl, false, i2c_slave->dev, "%s\n",
			      irq_log[STOP_DETECTED]);
		i2c_slave_clear_irq(i2c_slave, STOP_DETECTED);
	}

	if (irq_stat & (1 << RX_FIFO_FULL)) {
		I2C_SLAVE_DBG(i2c_slave->ipcl, false, i2c_slave->dev, "%s\n",
			      irq_log[RX_FIFO_FULL]);
		i2c_slave_send_nack(i2c_slave);
		i2c_slave_clear_irq(i2c_slave, RX_FIFO_FULL);
	}

	if (irq_stat & (1 << STRCH_RD)) {
		I2C_SLAVE_DBG(i2c_slave->ipcl, false, i2c_slave->dev, "%s\n",
			      irq_log[STRCH_RD]);
		i2c_slave->tx_count = read_tx_fifo_byte_count(i2c_slave);
		if (i2c_slave->tx_count > 0) {
			i2c_slave_send_ack(i2c_slave);
		} else {
			I2C_SLAVE_ERR(i2c_slave->ipcl, true, i2c_slave->dev,
				      "TX FIFO empty\n");
			i2c_slave_send_nack(i2c_slave);
		}
		i2c_slave_clear_irq(i2c_slave, STRCH_RD);
	}

	if (irq_stat & (1 << RX_DATA_AVAIL)) {
		I2C_SLAVE_DBG(i2c_slave->ipcl, false, i2c_slave->dev, "%s\n",
			      irq_log[RX_DATA_AVAIL]);
		i2c_slave_read_fifo(i2c_slave);
		i2c_slave_clear_irq(i2c_slave, RX_DATA_AVAIL);
	}

	if (irq_stat & (1 << STRCH_WR)) {
		I2C_SLAVE_DBG(i2c_slave->ipcl, false, i2c_slave->dev, "%s\n",
			      irq_log[STRCH_WR]);
		i2c_slave_clear_irq(i2c_slave, STRCH_WR);
		i2c_slave_send_ack(i2c_slave);
	}

	if (irq_stat & (1 << TX_FIFO_EMPTY)) {
		I2C_SLAVE_DBG(i2c_slave->ipcl, false, i2c_slave->dev, "%s\n",
			      irq_log[TX_FIFO_EMPTY]);
		i2c_slave->tx_count = read_tx_fifo_byte_count(i2c_slave);
		i2c_slave_write_fifo(i2c_slave);
		i2c_slave_clear_irq(i2c_slave, TX_FIFO_EMPTY);
	}

	if (irq_stat & (1 << GCA_DETECTED)) {
		I2C_SLAVE_DBG(i2c_slave->ipcl, false, i2c_slave->dev, "%s\n",
			      irq_log[GCA_DETECTED]);
		i2c_slave_send_nack(i2c_slave);
		i2c_slave_clear_irq(i2c_slave, GCA_DETECTED);
	}

	if (irq_stat & (1 << RESTART_DETECTED)) {
		I2C_SLAVE_DBG(i2c_slave->ipcl, false, i2c_slave->dev, "%s\n",
			      irq_log[RESTART_DETECTED]);
		i2c_slave_send_ack(i2c_slave);
		i2c_slave_clear_irq(i2c_slave, RESTART_DETECTED);
	}

	if (irq_stat & (1 << SMBALERT_ARA_DONE)) {
		I2C_SLAVE_DBG(i2c_slave->ipcl, false, i2c_slave->dev, "%s\n",
			      irq_log[SMBALERT_ARA_DONE]);
		i2c_slave_clear_irq(i2c_slave, SMBALERT_ARA_DONE);
	}

	if (irq_stat & (1 << SMBALERT_LOST_ARB)) {
		I2C_SLAVE_DBG(i2c_slave->ipcl, false, i2c_slave->dev, "%s\n",
			      irq_log[SMBALERT_LOST_ARB]);
		i2c_slave_clear_irq(i2c_slave, SMBALERT_LOST_ARB);
	}

	return IRQ_HANDLED;
}

/**
 * i2c_slave_write: write data to fifo.
 * @i2c_slave: Pointer to Main Structure.
 * @buf: TX Data buffer.
 * @count: TX Data count.
 *
 * This function will write data to TX FIFO.
 *
 * Return: 0 for success, negative number for error condition.
 */
static int i2c_slave_write(struct i2c_slave *i2c_slave, uint8_t *buf, size_t count)
{
	if (!buf) {
		I2C_SLAVE_ERR(i2c_slave->ipcl, true, i2c_slave->dev,
			      "Invalid write buffer\n");
		return -EINVAL;
	}

	if (!count) {
		I2C_SLAVE_ERR(i2c_slave->ipcl, true, i2c_slave->dev,
			      "Write count is zero\n");
		return -EINVAL;
	}

	i2c_slave->tx_msg_buf = buf;
	i2c_slave->tx_count = count;
	i2c_slave_write_fifo(i2c_slave);
	return 0;
}

/**
 * i2c_slave_read: read data from fifo.
 * @i2c_slave: Pointer to Main Structure.
 * @buf: RX Data buffer.
 * @count: RX Data count.
 *
 * This function will read data from RX FIFO.
 *
 * Return: 0 for success, negative number for error condition.
 */

static int i2c_slave_read(struct i2c_slave *i2c_slave, uint8_t *buf, size_t count)
{
	int i;

	if (!buf) {
		I2C_SLAVE_ERR(i2c_slave->ipcl, true, i2c_slave->dev,
			      "Invalid Read buffer\n");
		return -EINVAL;
	}

	if (count == 0) {
		I2C_SLAVE_ERR(i2c_slave->ipcl, true, i2c_slave->dev,
			      "Read count is zero\n");
		return -EINVAL;
	}

	if (count <= I2C_SMBUS_BYTE_DATA &&
	    i2c_slave->rx_count < count) {
		I2C_SLAVE_ERR(i2c_slave->ipcl, true, i2c_slave->dev,
			      "Data not available\n");
		return -EINVAL;
	}

	if (count > I2C_SMBUS_BYTE_DATA)
		count = i2c_slave->rx_count;

	i2c_slave->rx_count -= count;
	for (i = 0; i < count; i++)
		buf[i] = i2c_slave->rx_msg_buf[i];

	if (count <= I2C_SMBUS_BYTE_DATA) {
		for (i = 0; i < i2c_slave->rx_count; i++)
			i2c_slave->rx_msg_buf[i] =
				i2c_slave->rx_msg_buf[i + count];
	}

	i2c_slave_read_fifo(i2c_slave);
	return count;
}

/**
 * i2c_slave_xfer: SMbus transfer function.
 * @adap: I2C driver adapter
 * @addr: Slave address
 * @flags: i2c client flag
 * @read_write: Read/Write flag
 * @command: SMbus command code
 * @protocol: Command type
 * @data: Pointer to client data
 *
 * Based on read_write flag, it will call read
 * write function.
 *
 * Return: 0 for success, Negative number for error condition.
 */
static int i2c_slave_xfer(struct i2c_adapter *adap, u16 addr,
			  unsigned short flags, char read_write,
			  u8 command, int protocol, union i2c_smbus_data *data)
{
	struct i2c_slave *i2c_slave = i2c_get_adapdata(adap);
	u8 buf[I2C_SMBUS_BLOCK_MAX];
	int ret = 0, count, i;

	if (addr != i2c_slave->slave_addr) {
		i2c_slave->slave_addr = addr;
		writel_relaxed(addr, i2c_slave->base + I2C_S_DEVICE_ADDR);
	}

	if (read_write == I2C_SMBUS_READ) {
		switch (protocol) {
		case I2C_SMBUS_BYTE:
		case I2C_SMBUS_BYTE_DATA:
			count = i2c_slave_read(i2c_slave, buf, I2C_SLAVE_BYTE_DATA);
			if (count == I2C_SMBUS_BYTE) {
				data->byte = buf[0];
				return 0;
			}
			ret = count;
			break;

		case I2C_SMBUS_WORD_DATA:
			count = i2c_slave_read(i2c_slave, buf, I2C_SLAVE_WORD_DATA);
			if (count == I2C_SMBUS_BYTE_DATA) {
				data->word = (buf[0] | (buf[1] << 8));
				return 0;
			}
			ret = count;
			break;

		case I2C_SMBUS_BLOCK_DATA:
			count = i2c_slave_read(i2c_slave, buf, I2C_SMBUS_BLOCK_MAX);
			if (count > 0) {
				data->block[0] = count;
				for (i = 0; i < count; i++)
					data->block[i + 1] = buf[i];
				return 0;
			}
			ret = count;
			break;

		default:
			break;
		}
	} else if (read_write == I2C_SMBUS_WRITE) {
		switch (protocol) {
		case I2C_SMBUS_BYTE:
		case I2C_SMBUS_BYTE_DATA:
			buf[0] = data->byte;
			ret = i2c_slave_write(i2c_slave, buf, I2C_SLAVE_BYTE_DATA);
			break;

		case I2C_SMBUS_WORD_DATA:
			buf[0] = (uint8_t)(data->word & 0xFF);
			buf[1] = (uint8_t)(data->word >> 8);
			ret = i2c_slave_write(i2c_slave, buf, I2C_SLAVE_WORD_DATA);
			break;

		case I2C_SMBUS_BLOCK_DATA:
			if (data->block[0] > I2C_SMBUS_BLOCK_MAX)
				data->block[0] = I2C_SMBUS_BLOCK_MAX;
			for (i = 0; i < data->block[0]; i++)
				buf[i] = data->block[i + 1];
			ret = i2c_slave_write(i2c_slave, buf, data->block[0]);
			break;
		default:
			break;
		}
	}

	return ret;
}

/**
 * i2c_slave_enable_clk: Enable AHB & XO clock.
 * @i2c_slave: Pointer to Main Structure.
 *
 * This function will enable AHB and XO clock.
 *
 * Return: 0 for success, negative number for error condition.
 */
static int i2c_slave_enable_clk(struct i2c_slave *i2c_slave)
{
	int ret = 0;

	i2c_slave->xo_clk = devm_clk_get(i2c_slave->dev, "sm_bus_xo_clk");
	if (IS_ERR(i2c_slave->xo_clk)) {
		ret = PTR_ERR(i2c_slave->xo_clk);
		I2C_SLAVE_ERR(i2c_slave->ipcl, true, i2c_slave->dev,
			      "Err getting XO clk %d\n", ret);
		return ret;
	}

	i2c_slave->ahb_clk = devm_clk_get(i2c_slave->dev, "sm_bus_ahb_clk");
	if (IS_ERR(i2c_slave->ahb_clk)) {
		ret = PTR_ERR(i2c_slave->ahb_clk);
		I2C_SLAVE_ERR(i2c_slave->ipcl, true, i2c_slave->dev,
			      "Err getting AHB clk %d\n", ret);
		return ret;
	}

	ret = clk_prepare_enable(i2c_slave->ahb_clk);
	if (ret)
		return ret;

	ret = clk_prepare_enable(i2c_slave->xo_clk);
	if (ret)
		goto err_on_xo;

	return 0;

err_on_xo:
	clk_disable_unprepare(i2c_slave->ahb_clk);
	return ret;
}

/**
 * i2c_slave_icc_init: Enable ICB voting.
 * @i2c_slave: Pointer to Main Structure.
 *
 * This function will enable ICB voting for i2c slave.
 *
 * Return: 0 for success, negative number for error condition.
 */
static int i2c_slave_icc_init(struct i2c_slave *i2c_slave)
{
	int ret = 0;

	i2c_slave->icc_path = devm_of_icc_get(i2c_slave->dev, "i2c-slave-config");
	if (IS_ERR(i2c_slave->icc_path)) {
		I2C_SLAVE_ERR(i2c_slave->ipcl, true, i2c_slave->dev,
			      "devm_of_icc_get failed: %d:\n", ret);
		return -EINVAL;
	}

	i2c_slave->bw = APPS_PROC_TO_I2C_SLAVE_VOTE;
	icc_set_bw(i2c_slave->icc_path, i2c_slave->bw, i2c_slave->bw);

	ret = icc_enable(i2c_slave->icc_path);
	if (ret) {
		I2C_SLAVE_ERR(i2c_slave->ipcl, true, i2c_slave->dev,
			      "ICC enable failed err: %d\n", ret);
		devres_free(&i2c_slave->icc_path);
		return ret;
	}

	return 0;
}

/**
 * i2c_slave_func: To check supported i2c functionality.
 * @adap: I2C driver adapter.
 *
 * This function will use to determine what the adapter supports.
 *
 * Return: 0 for success, negative number for error condition.
 */
static u32 i2c_slave_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_SMBUS_BYTE;
}

static const struct i2c_algorithm i2c_slave_algo = {
	.smbus_xfer	= i2c_slave_xfer,
	.functionality	= i2c_slave_func,
};

/**
 * i2c_slave_probe: Driver Probe function.
 * @pdev: Pointer to platform device structure.
 *
 * This function will performs pre-initialization tasks such as reading dtsi property,
 * setting clock, IRQ request, initializing registers, allocating memory,
 * and initializing registers for i2c slave.
 *
 * Return: 0 for success, negative number for error condition.
 */
static int i2c_slave_probe(struct platform_device *pdev)
{
	struct i2c_slave *i2c_slave;
	struct resource *res;
	static const char adapName[10] = "I2C-slave";
	char ipc_name[I2C_NAME_SIZE];
	int ret = 0;

	i2c_slave = devm_kzalloc(&pdev->dev,
				 sizeof(*i2c_slave), GFP_KERNEL);
	if (!i2c_slave) {
		ret = -ENOMEM;
		goto err;
	}

	i2c_slave->dev = &pdev->dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		ret = -EINVAL;
		goto err;
	}

	i2c_slave->base = devm_ioremap_resource(i2c_slave->dev, res);
	if (IS_ERR(i2c_slave->base)) {
		ret = PTR_ERR(i2c_slave->base);
		goto err;
	}

	scnprintf(ipc_name, I2C_NAME_SIZE, "%s", dev_name(i2c_slave->dev));
	i2c_slave->ipcl = ipc_log_context_create(2, ipc_name, 0);
	if (!i2c_slave->ipcl) {
		dev_err(i2c_slave->dev, "Error: Failed to create IPC log file\n");
		ret = -EINVAL;
		goto err_ipc;
	}

	ret = i2c_slave_enable_clk(i2c_slave);
	if (ret)
		goto err_ipc;

	i2c_slave->irq = platform_get_irq(pdev, 0);
	if (i2c_slave->irq < 0) {
		I2C_SLAVE_ERR(i2c_slave->ipcl, true, i2c_slave->dev,
			      "get_irq failed: %d:\n", i2c_slave->irq);
		ret = i2c_slave->irq;
		goto err_irq;
	}

	ret = devm_request_irq(i2c_slave->dev, i2c_slave->irq, i2c_slave_irq, 0,
			       "i2c_slave", i2c_slave);
	if (ret) {
		I2C_SLAVE_ERR(i2c_slave->ipcl, true, i2c_slave->dev,
			      "Request_irq failed: %d: err: %d\n", i2c_slave->irq, ret);
		goto err_irq;
	}

	ret = i2c_slave_icc_init(i2c_slave);
	if (ret) {
		I2C_SLAVE_ERR(i2c_slave->ipcl, true, i2c_slave->dev,
			      "ICC init failed ret: %d\n", ret);
		goto err_icc;
	}

	i2c_slave->adap.algo = &i2c_slave_algo;
	i2c_slave->rx_count = 0;

	/* Enable IRQ */
	i2c_slave_enable_irq(i2c_slave);

	/* Set default slave address */
	writel_relaxed(SLAVE_ADDR, i2c_slave->base + I2C_S_DEVICE_ADDR);
	i2c_slave->slave_addr = SLAVE_ADDR;

	/* Enable core */
	writel_relaxed(CORE_EN, i2c_slave->base + I2C_S_CONFIG);

	ret = strscpy(i2c_slave->adap.name, adapName, sizeof(i2c_slave->adap.name));
	if (ret != strlen(adapName)) {
		I2C_SLAVE_ERR(i2c_slave->ipcl, true, i2c_slave->dev,
			      "Failed to set adapter name\n");
		goto err_adap;
	}

	i2c_set_adapdata(&i2c_slave->adap, i2c_slave);
	platform_set_drvdata(pdev, i2c_slave);
	i2c_slave->adap.dev.parent = i2c_slave->dev;
	i2c_slave->adap.dev.of_node = pdev->dev.of_node;
	ret = i2c_add_adapter(&i2c_slave->adap);
	if (ret) {
		I2C_SLAVE_ERR(i2c_slave->ipcl, true, i2c_slave->dev,
			      "Add adapter failed, ret=%d\n", ret);
		goto err_adap;
	}

	I2C_SLAVE_DBG(i2c_slave->ipcl, true, i2c_slave->dev,
		      "I2C Slave probed\n");
	return 0;

err_adap:
	icc_disable(i2c_slave->icc_path);
	devres_free(&i2c_slave->icc_path);
err_icc:
	disable_irq(i2c_slave->irq);
err_irq:
	clk_disable_unprepare(i2c_slave->ahb_clk);
	clk_disable_unprepare(i2c_slave->xo_clk);
err_ipc:
	if (i2c_slave->ipcl)
		ipc_log_context_destroy(i2c_slave->ipcl);
err:
	return ret;
}

/**
 * i2c_slave_remove: This function will release the resources.
 * @pdev: Pointer to platform device structure.
 *
 * This function will release clocks, IRQ and other allocated resources for
 * i2c slave.
 *
 * Return: 0 for success.
 */
static int i2c_slave_remove(struct platform_device *pdev)
{
	struct i2c_slave *i2c_slave = platform_get_drvdata(pdev);

	clk_disable_unprepare(i2c_slave->ahb_clk);
	clk_disable_unprepare(i2c_slave->xo_clk);
	disable_irq(i2c_slave->irq);
	icc_disable(i2c_slave->icc_path);
	devres_free(&i2c_slave->icc_path);
	i2c_del_adapter(&i2c_slave->adap);

	if (i2c_slave->ipcl)
		ipc_log_context_destroy(i2c_slave->ipcl);

	return 0;
}

/**
 * i2c_slave_suspend: driver suspend function.
 * @dev: Pointer to device structure.
 *
 * This function will put driver into suspend state by releasing
 * icc, irq and core clock.
 *
 * Return: 0 for success.
 */
#ifdef I2C_SLAVE_SUSPEND_RESUME
static int i2c_slave_suspend(struct device *dev)
{
	struct i2c_slave *i2c_slave = dev_get_drvdata(dev);

	disable_irq(i2c_slave->irq);
	icc_disable(i2c_slave->icc_path);
	clk_disable_unprepare(i2c_slave->ahb_clk);
	clk_disable_unprepare(i2c_slave->xo_clk);
	I2C_SLAVE_DBG(i2c_slave->ipcl, true, i2c_slave->dev,
		      "%s\n", __func__);
	return 0;
}

/**
 * i2c_slave_resume: driver resume function.
 * @dev: Pointer to device structure.
 *
 * This function will resume the driver by enabling icc, irq
 * and core clock.
 *
 * Return: 0 for success, negative number for error condition.
 */
static int i2c_slave_resume(struct device *dev)
{
	struct i2c_slave *i2c_slave = dev_get_drvdata(dev);
	int ret = 0;

	enable_irq(i2c_slave->irq);

	ret = icc_enable(i2c_slave->icc_path);
	if (ret) {
		I2C_SLAVE_ERR(i2c_slave->ipcl, true, i2c_slave->dev,
			      "ICC enable failed err: %d\n", ret);
		goto err_icc;
	}

	ret = clk_prepare_enable(i2c_slave->ahb_clk);
	if (ret) {
		I2C_SLAVE_DBG(i2c_slave->ipcl, true, i2c_slave->dev,
			      "%s: failing at ahb clk prepare enable ret=%d\n",
			      __func__, ret);
		goto err_ahb_clk;
	}

	ret = clk_prepare_enable(i2c_slave->xo_clk);
	if (ret) {
		I2C_SLAVE_DBG(i2c_slave->ipcl, true, i2c_slave->dev,
			      "%s: failing at xo clk prepare enable ret=%d\n",
			       __func__, ret);
		clk_disable_unprepare(i2c_slave->ahb_clk);
		goto err_xo_clk;
	}

	I2C_SLAVE_DBG(i2c_slave->ipcl, true, i2c_slave->dev,
		      "%s:\n", __func__);
	return 0;

err_xo_clk:
	clk_disable_unprepare(i2c_slave->ahb_clk);
err_ahb_clk:
	icc_disable(i2c_slave->icc_path);
err_icc:
	return ret;
}
#else
static int i2c_slave_suspend(struct device *dev)
{
	return 0;
}

static int i2c_slave_resume(struct device *dev)
{
	return 0;
}
#endif

static const struct dev_pm_ops i2c_slave_pm_ops = {
	.suspend = i2c_slave_suspend,
	.resume = i2c_slave_resume,
};

static const struct of_device_id i2c_slave_dt_match[] = {
	{.compatible = "qcom,i2c-slave" },
	{ }
};
MODULE_DEVICE_TABLE(of, i2c_slave_dt_match);

static struct platform_driver i2c_slave_driver = {
	.driver = {
		.name = "i2c_slave",
		.pm = &i2c_slave_pm_ops,
		.of_match_table = i2c_slave_dt_match,
	},
	.probe  = i2c_slave_probe,
	.remove = i2c_slave_remove,
};

module_platform_driver(i2c_slave_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("i2c slave");
