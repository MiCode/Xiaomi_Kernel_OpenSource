/* Copyright (c) 2009-2012, Code Aurora Forum. All rights reserved.
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
/*
 * QUP driver for Qualcomm MSM platforms
 *
 */

/* #define DEBUG */

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/timer.h>
#include <linux/slab.h>
#include <mach/board.h>
#include <mach/gpiomux.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_i2c.h>

MODULE_LICENSE("GPL v2");
MODULE_VERSION("0.2");
MODULE_ALIAS("platform:i2c_qup");

/* QUP Registers */
enum {
	QUP_CONFIG              = 0x0,
	QUP_STATE               = 0x4,
	QUP_IO_MODE             = 0x8,
	QUP_SW_RESET            = 0xC,
	QUP_OPERATIONAL         = 0x18,
	QUP_ERROR_FLAGS         = 0x1C,
	QUP_ERROR_FLAGS_EN      = 0x20,
	QUP_MX_READ_CNT         = 0x208,
	QUP_MX_INPUT_CNT        = 0x200,
	QUP_MX_WR_CNT           = 0x100,
	QUP_OUT_DEBUG           = 0x108,
	QUP_OUT_FIFO_CNT        = 0x10C,
	QUP_OUT_FIFO_BASE       = 0x110,
	QUP_IN_READ_CUR         = 0x20C,
	QUP_IN_DEBUG            = 0x210,
	QUP_IN_FIFO_CNT         = 0x214,
	QUP_IN_FIFO_BASE        = 0x218,
	QUP_I2C_CLK_CTL         = 0x400,
	QUP_I2C_STATUS          = 0x404,
};

/* QUP States and reset values */
enum {
	QUP_RESET_STATE         = 0,
	QUP_RUN_STATE           = 1U,
	QUP_STATE_MASK          = 3U,
	QUP_PAUSE_STATE         = 3U,
	QUP_STATE_VALID         = 1U << 2,
	QUP_I2C_MAST_GEN        = 1U << 4,
	QUP_OPERATIONAL_RESET   = 0xFF0,
	QUP_I2C_STATUS_RESET    = 0xFFFFFC,
};

/* QUP OPERATIONAL FLAGS */
enum {
	QUP_OUT_SVC_FLAG        = 1U << 8,
	QUP_IN_SVC_FLAG         = 1U << 9,
	QUP_MX_INPUT_DONE       = 1U << 11,
};

/* I2C mini core related values */
enum {
	I2C_MINI_CORE           = 2U << 8,
	I2C_N_VAL               = 0xF,

};

/* Packing Unpacking words in FIFOs , and IO modes*/
enum {
	QUP_WR_BLK_MODE  = 1U << 10,
	QUP_RD_BLK_MODE  = 1U << 12,
	QUP_UNPACK_EN = 1U << 14,
	QUP_PACK_EN = 1U << 15,
};

/* QUP tags */
enum {
	QUP_OUT_NOP   = 0,
	QUP_OUT_START = 1U << 8,
	QUP_OUT_DATA  = 2U << 8,
	QUP_OUT_STOP  = 3U << 8,
	QUP_OUT_REC   = 4U << 8,
	QUP_IN_DATA   = 5U << 8,
	QUP_IN_STOP   = 6U << 8,
	QUP_IN_NACK   = 7U << 8,
};

/* Status, Error flags */
enum {
	I2C_STATUS_WR_BUFFER_FULL  = 1U << 0,
	I2C_STATUS_BUS_ACTIVE      = 1U << 8,
	I2C_STATUS_BUS_MASTER	   = 1U << 9,
	I2C_STATUS_ERROR_MASK      = 0x38000FC,
	QUP_I2C_NACK_FLAG          = 1U << 3,
	QUP_IN_NOT_EMPTY           = 1U << 5,
	QUP_STATUS_ERROR_FLAGS     = 0x7C,
};

/* Master status clock states */
enum {
	I2C_CLK_RESET_BUSIDLE_STATE	= 0,
	I2C_CLK_FORCED_LOW_STATE	= 5,
};

#define QUP_MAX_CLK_STATE_RETRIES	300

static char const * const i2c_rsrcs[] = {"i2c_clk", "i2c_sda"};

static struct gpiomux_setting recovery_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

struct qup_i2c_dev {
	struct device                *dev;
	void __iomem                 *base;		/* virtual */
	void __iomem                 *gsbi;		/* virtual */
	int                          in_irq;
	int                          out_irq;
	int                          err_irq;
	int                          num_irqs;
	struct clk                   *clk;
	struct clk                   *pclk;
	struct i2c_adapter           adapter;

	struct i2c_msg               *msg;
	int                          pos;
	int                          cnt;
	int                          err;
	int                          mode;
	int                          clk_ctl;
	int                          one_bit_t;
	int                          out_fifo_sz;
	int                          in_fifo_sz;
	int                          out_blk_sz;
	int                          in_blk_sz;
	int                          wr_sz;
	struct msm_i2c_platform_data *pdata;
	int                          suspended;
	int                          clk_state;
	struct timer_list            pwr_timer;
	struct mutex                 mlock;
	void                         *complete;
	int                          i2c_gpios[ARRAY_SIZE(i2c_rsrcs)];
};

#ifdef DEBUG
static void
qup_print_status(struct qup_i2c_dev *dev)
{
	uint32_t val;
	val = readl_relaxed(dev->base+QUP_CONFIG);
	dev_dbg(dev->dev, "Qup config is :0x%x\n", val);
	val = readl_relaxed(dev->base+QUP_STATE);
	dev_dbg(dev->dev, "Qup state is :0x%x\n", val);
	val = readl_relaxed(dev->base+QUP_IO_MODE);
	dev_dbg(dev->dev, "Qup mode is :0x%x\n", val);
}
#else
static inline void qup_print_status(struct qup_i2c_dev *dev)
{
}
#endif

static irqreturn_t
qup_i2c_interrupt(int irq, void *devid)
{
	struct qup_i2c_dev *dev = devid;
	uint32_t status = readl_relaxed(dev->base + QUP_I2C_STATUS);
	uint32_t status1 = readl_relaxed(dev->base + QUP_ERROR_FLAGS);
	uint32_t op_flgs = readl_relaxed(dev->base + QUP_OPERATIONAL);
	int err = 0;

	if (!dev->msg || !dev->complete) {
		/* Clear Error interrupt if it's a level triggered interrupt*/
		if (dev->num_irqs == 1) {
			writel_relaxed(QUP_RESET_STATE, dev->base+QUP_STATE);
			/* Ensure that state is written before ISR exits */
			mb();
		}
		return IRQ_HANDLED;
	}

	if (status & I2C_STATUS_ERROR_MASK) {
		dev_err(dev->dev, "QUP: I2C status flags :0x%x, irq:%d\n",
			status, irq);
		err = status;
		/* Clear Error interrupt if it's a level triggered interrupt*/
		if (dev->num_irqs == 1) {
			writel_relaxed(QUP_RESET_STATE, dev->base+QUP_STATE);
			/* Ensure that state is written before ISR exits */
			mb();
		}
		goto intr_done;
	}

	if (status1 & 0x7F) {
		dev_err(dev->dev, "QUP: QUP status flags :0x%x\n", status1);
		err = -status1;
		/* Clear Error interrupt if it's a level triggered interrupt*/
		if (dev->num_irqs == 1) {
			writel_relaxed((status1 & QUP_STATUS_ERROR_FLAGS),
				dev->base + QUP_ERROR_FLAGS);
			/* Ensure that error flags are cleared before ISR
			 * exits
			 */
			mb();
		}
		goto intr_done;
	}

	if ((dev->num_irqs == 3) && (dev->msg->flags == I2C_M_RD)
		&& (irq == dev->out_irq))
		return IRQ_HANDLED;
	if (op_flgs & QUP_OUT_SVC_FLAG) {
		writel_relaxed(QUP_OUT_SVC_FLAG, dev->base + QUP_OPERATIONAL);
		/* Ensure that service flag is acknowledged before ISR exits */
		mb();
	}
	if (dev->msg->flags == I2C_M_RD) {
		if ((op_flgs & QUP_MX_INPUT_DONE) ||
			(op_flgs & QUP_IN_SVC_FLAG)) {
			writel_relaxed(QUP_IN_SVC_FLAG, dev->base
					+ QUP_OPERATIONAL);
			/* Ensure that service flag is acknowledged before ISR
			 * exits
			 */
			mb();
		} else
			return IRQ_HANDLED;
	}

intr_done:
	dev_dbg(dev->dev, "QUP intr= %d, i2c status=0x%x, qup status = 0x%x\n",
			irq, status, status1);
	qup_print_status(dev);
	dev->err = err;
	complete(dev->complete);
	return IRQ_HANDLED;
}

static int
qup_i2c_poll_state(struct qup_i2c_dev *dev, uint32_t req_state, bool only_valid)
{
	uint32_t retries = 0;

	dev_dbg(dev->dev, "Polling for state:0x%x, or valid-only:%d\n",
				req_state, only_valid);

	while (retries != 2000) {
		uint32_t status = readl_relaxed(dev->base + QUP_STATE);

		/*
		 * If only valid bit needs to be checked, requested state is
		 * 'don't care'
		 */
		if (status & QUP_STATE_VALID) {
			if (only_valid)
				return 0;
			else if ((req_state & QUP_I2C_MAST_GEN) &&
					(status & QUP_I2C_MAST_GEN))
				return 0;
			else if ((status & QUP_STATE_MASK) == req_state)
				return 0;
		}
		if (retries++ == 1000)
			udelay(100);
	}
	return -ETIMEDOUT;
}

static int
qup_update_state(struct qup_i2c_dev *dev, uint32_t state)
{
	if (qup_i2c_poll_state(dev, 0, true) != 0)
		return -EIO;
	writel_relaxed(state, dev->base + QUP_STATE);
	if (qup_i2c_poll_state(dev, state, false) != 0)
		return -EIO;
	return 0;
}

/*
 * Before calling qup_config_core_on_en(), please make
 * sure that QuPE core is in RESET state.
 */
static void
qup_config_core_on_en(struct qup_i2c_dev *dev)
{
	uint32_t status;

	status = readl_relaxed(dev->base + QUP_CONFIG);
	status |= BIT(13);
	writel_relaxed(status, dev->base + QUP_CONFIG);
	/* making sure that write has really gone through */
	mb();
}

static void
qup_i2c_pwr_mgmt(struct qup_i2c_dev *dev, unsigned int state)
{
	dev->clk_state = state;
	if (state != 0) {
		clk_enable(dev->clk);
		clk_enable(dev->pclk);
	} else {
		qup_update_state(dev, QUP_RESET_STATE);
		clk_disable(dev->clk);
		qup_config_core_on_en(dev);
		clk_disable(dev->pclk);
	}
}

static void
qup_i2c_pwr_timer(unsigned long data)
{
	struct qup_i2c_dev *dev = (struct qup_i2c_dev *) data;
	dev_dbg(dev->dev, "QUP_Power: Inactivity based power management\n");
	if (dev->clk_state == 1)
		qup_i2c_pwr_mgmt(dev, 0);
}

static int
qup_i2c_poll_writeready(struct qup_i2c_dev *dev, int rem)
{
	uint32_t retries = 0;

	while (retries != 2000) {
		uint32_t status = readl_relaxed(dev->base + QUP_I2C_STATUS);

		if (!(status & I2C_STATUS_WR_BUFFER_FULL)) {
			if (((dev->msg->flags & I2C_M_RD) || (rem == 0)) &&
				!(status & I2C_STATUS_BUS_ACTIVE))
				return 0;
			else if ((dev->msg->flags == 0) && (rem > 0))
				return 0;
			else /* 1-bit delay before we check for bus busy */
				udelay(dev->one_bit_t);
		}
		if (retries++ == 1000) {
			/*
			 * Wait for FIFO number of bytes to be absolutely sure
			 * that I2C write state machine is not idle. Each byte
			 * takes 9 clock cycles. (8 bits + 1 ack)
			 */
			usleep_range((dev->one_bit_t * (dev->out_fifo_sz * 9)),
				(dev->one_bit_t * (dev->out_fifo_sz * 9)));
		}
	}
	qup_print_status(dev);
	return -ETIMEDOUT;
}

static int qup_i2c_poll_clock_ready(struct qup_i2c_dev *dev)
{
	uint32_t retries = 0;

	/*
	 * Wait for the clock state to transition to either IDLE or FORCED
	 * LOW.  This will usually happen within one cycle of the i2c clock.
	 */

	while (retries++ < QUP_MAX_CLK_STATE_RETRIES) {
		uint32_t status = readl_relaxed(dev->base + QUP_I2C_STATUS);
		uint32_t clk_state = (status >> 13) & 0x7;

		if (clk_state == I2C_CLK_RESET_BUSIDLE_STATE ||
				clk_state == I2C_CLK_FORCED_LOW_STATE)
			return 0;
		/* 1-bit delay before we check again */
		udelay(dev->one_bit_t);
	}

	dev_err(dev->dev, "Error waiting for clk ready\n");
	return -ETIMEDOUT;
}

static inline int qup_i2c_request_gpios(struct qup_i2c_dev *dev)
{
	int i;
	int result = 0;

	for (i = 0; i < ARRAY_SIZE(i2c_rsrcs); ++i) {
		if (dev->i2c_gpios[i] >= 0) {
			result = gpio_request(dev->i2c_gpios[i], i2c_rsrcs[i]);
			if (result) {
				dev_err(dev->dev,
					"gpio_request for pin %d failed\
					with error %d\n", dev->i2c_gpios[i],
					result);
				goto error;
			}
		}
	}
	return 0;

error:
	for (; --i >= 0;) {
		if (dev->i2c_gpios[i] >= 0)
			gpio_free(dev->i2c_gpios[i]);
	}
	return result;
}

static inline void qup_i2c_free_gpios(struct qup_i2c_dev *dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(i2c_rsrcs); ++i) {
		if (dev->i2c_gpios[i] >= 0)
			gpio_free(dev->i2c_gpios[i]);
	}
}

#ifdef DEBUG
static void qup_verify_fifo(struct qup_i2c_dev *dev, uint32_t val,
				uint32_t addr, int rdwr)
{
	if (rdwr)
		dev_dbg(dev->dev, "RD:Wrote 0x%x to out_ff:0x%x\n", val, addr);
	else
		dev_dbg(dev->dev, "WR:Wrote 0x%x to out_ff:0x%x\n", val, addr);
}
#else
static inline void qup_verify_fifo(struct qup_i2c_dev *dev, uint32_t val,
				uint32_t addr, int rdwr)
{
}
#endif

static void
qup_issue_read(struct qup_i2c_dev *dev, struct i2c_msg *msg, int *idx,
		uint32_t carry_over)
{
	uint16_t addr = (msg->addr << 1) | 1;
	/* QUP limit 256 bytes per read. By HW design, 0 in the 8-bit field
	 * is treated as 256 byte read.
	 */
	uint16_t rd_len = ((dev->cnt == 256) ? 0 : dev->cnt);

	if (*idx % 4) {
		writel_relaxed(carry_over | ((QUP_OUT_START | addr) << 16),
		dev->base + QUP_OUT_FIFO_BASE);/* + (*idx-2)); */

		qup_verify_fifo(dev, carry_over |
			((QUP_OUT_START | addr) << 16), (uint32_t)dev->base
			+ QUP_OUT_FIFO_BASE + (*idx - 2), 1);
		writel_relaxed((QUP_OUT_REC | rd_len),
			dev->base + QUP_OUT_FIFO_BASE);/* + (*idx+2)); */

		qup_verify_fifo(dev, (QUP_OUT_REC | rd_len),
		(uint32_t)dev->base + QUP_OUT_FIFO_BASE + (*idx + 2), 1);
	} else {
		writel_relaxed(((QUP_OUT_REC | rd_len) << 16)
			| QUP_OUT_START | addr,
			dev->base + QUP_OUT_FIFO_BASE);/* + (*idx)); */

		qup_verify_fifo(dev, QUP_OUT_REC << 16 | rd_len << 16 |
		QUP_OUT_START | addr,
		(uint32_t)dev->base + QUP_OUT_FIFO_BASE + (*idx), 1);
	}
	*idx += 4;
}

static void
qup_issue_write(struct qup_i2c_dev *dev, struct i2c_msg *msg, int rem,
			int *idx, uint32_t *carry_over)
{
	int entries = dev->cnt;
	int empty_sl = dev->wr_sz - ((*idx) >> 1);
	int i = 0;
	uint32_t val = 0;
	uint32_t last_entry = 0;
	uint16_t addr = msg->addr << 1;

	if (dev->pos == 0) {
		if (*idx % 4) {
			writel_relaxed(*carry_over | ((QUP_OUT_START |
							addr) << 16),
					dev->base + QUP_OUT_FIFO_BASE);

			qup_verify_fifo(dev, *carry_over | QUP_OUT_START << 16 |
				addr << 16, (uint32_t)dev->base +
				QUP_OUT_FIFO_BASE + (*idx) - 2, 0);
		} else
			val = QUP_OUT_START | addr;
		*idx += 2;
		i++;
		entries++;
	} else {
		/* Avoid setp time issue by adding 1 NOP when number of bytes
		 * are more than FIFO/BLOCK size. setup time issue can't appear
		 * otherwise since next byte to be written will always be ready
		 */
		val = (QUP_OUT_NOP | 1);
		*idx += 2;
		i++;
		entries++;
	}
	if (entries > empty_sl)
		entries = empty_sl;

	for (; i < (entries - 1); i++) {
		if (*idx % 4) {
			writel_relaxed(val | ((QUP_OUT_DATA |
				msg->buf[dev->pos]) << 16),
				dev->base + QUP_OUT_FIFO_BASE);

			qup_verify_fifo(dev, val | QUP_OUT_DATA << 16 |
				msg->buf[dev->pos] << 16, (uint32_t)dev->base +
				QUP_OUT_FIFO_BASE + (*idx) - 2, 0);
		} else
			val = QUP_OUT_DATA | msg->buf[dev->pos];
		(*idx) += 2;
		dev->pos++;
	}
	if (dev->pos < (msg->len - 1))
		last_entry = QUP_OUT_DATA;
	else if (rem > 1) /* not last array entry */
		last_entry = QUP_OUT_DATA;
	else
		last_entry = QUP_OUT_STOP;
	if ((*idx % 4) == 0) {
		/*
		 * If read-start and read-command end up in different fifos, it
		 * may result in extra-byte being read due to extra-read cycle.
		 * Avoid that by inserting NOP as the last entry of fifo only
		 * if write command(s) leave 1 space in fifo.
		 */
		if (rem > 1) {
			struct i2c_msg *next = msg + 1;
			if (next->addr == msg->addr && (next->flags & I2C_M_RD)
				&& *idx == ((dev->wr_sz*2) - 4)) {
				writel_relaxed(((last_entry |
					msg->buf[dev->pos]) |
					((1 | QUP_OUT_NOP) << 16)), dev->base +
					QUP_OUT_FIFO_BASE);/* + (*idx) - 2); */

				qup_verify_fifo(dev,
					((last_entry | msg->buf[dev->pos]) |
					((1 | QUP_OUT_NOP) << 16)),
					(uint32_t)dev->base +
					QUP_OUT_FIFO_BASE + (*idx), 0);
				*idx += 2;
			} else if (next->flags == 0 && dev->pos == msg->len - 1
					&& *idx < (dev->wr_sz*2) &&
					(next->addr != msg->addr)) {
				/* Last byte of an intermittent write */
				writel_relaxed((QUP_OUT_STOP |
						msg->buf[dev->pos]),
					dev->base + QUP_OUT_FIFO_BASE);

				qup_verify_fifo(dev,
					QUP_OUT_STOP | msg->buf[dev->pos],
					(uint32_t)dev->base +
					QUP_OUT_FIFO_BASE + (*idx), 0);
				*idx += 2;
			} else
				*carry_over = (last_entry | msg->buf[dev->pos]);
		} else {
			writel_relaxed((last_entry | msg->buf[dev->pos]),
			dev->base + QUP_OUT_FIFO_BASE);/* + (*idx) - 2); */

			qup_verify_fifo(dev, last_entry | msg->buf[dev->pos],
			(uint32_t)dev->base + QUP_OUT_FIFO_BASE +
			(*idx), 0);
		}
	} else {
		writel_relaxed(val | ((last_entry | msg->buf[dev->pos]) << 16),
		dev->base + QUP_OUT_FIFO_BASE);/* + (*idx) - 2); */

		qup_verify_fifo(dev, val | (last_entry << 16) |
		(msg->buf[dev->pos] << 16), (uint32_t)dev->base +
		QUP_OUT_FIFO_BASE + (*idx) - 2, 0);
	}

	*idx += 2;
	dev->pos++;
	dev->cnt = msg->len - dev->pos;
}

static void
qup_set_read_mode(struct qup_i2c_dev *dev, int rd_len)
{
	uint32_t wr_mode = (dev->wr_sz < dev->out_fifo_sz) ?
				QUP_WR_BLK_MODE : 0;
	if (rd_len > 256) {
		dev_dbg(dev->dev, "HW limit: Breaking reads in chunk of 256\n");
		rd_len = 256;
	}
	if (rd_len <= dev->in_fifo_sz) {
		writel_relaxed(wr_mode | QUP_PACK_EN | QUP_UNPACK_EN,
			dev->base + QUP_IO_MODE);
		writel_relaxed(rd_len, dev->base + QUP_MX_READ_CNT);
	} else {
		writel_relaxed(wr_mode | QUP_RD_BLK_MODE |
			QUP_PACK_EN | QUP_UNPACK_EN, dev->base + QUP_IO_MODE);
		writel_relaxed(rd_len, dev->base + QUP_MX_INPUT_CNT);
	}
}

static int
qup_set_wr_mode(struct qup_i2c_dev *dev, int rem)
{
	int total_len = 0;
	int ret = 0;
	int len = dev->msg->len;
	struct i2c_msg *next = NULL;
	if (rem > 1)
		next = dev->msg + 1;
	while (rem > 1 && next->flags == 0 && (next->addr == dev->msg->addr)) {
		len += next->len + 1;
		next = next + 1;
		rem--;
	}
	if (len >= (dev->out_fifo_sz - 1)) {
		total_len = len + 1 + (len/(dev->out_blk_sz-1));

		writel_relaxed(QUP_WR_BLK_MODE | QUP_PACK_EN | QUP_UNPACK_EN,
			dev->base + QUP_IO_MODE);
		dev->wr_sz = dev->out_blk_sz;
	} else
		writel_relaxed(QUP_PACK_EN | QUP_UNPACK_EN,
			dev->base + QUP_IO_MODE);

	if (rem > 1) {
		if (next->addr == dev->msg->addr &&
			next->flags == I2C_M_RD) {
			qup_set_read_mode(dev, next->len);
			/* make sure read start & read command are in 1 blk */
			if ((total_len % dev->out_blk_sz) ==
				(dev->out_blk_sz - 1))
				total_len += 3;
			else
				total_len += 2;
		}
	}
	/* WRITE COUNT register valid/used only in block mode */
	if (dev->wr_sz == dev->out_blk_sz)
		writel_relaxed(total_len, dev->base + QUP_MX_WR_CNT);
	return ret;
}


static void qup_i2c_recover_bus_busy(struct qup_i2c_dev *dev)
{
	int i;
	int gpio_clk;
	int gpio_dat;
	bool gpio_clk_status = false;
	uint32_t status = readl_relaxed(dev->base + QUP_I2C_STATUS);
	struct gpiomux_setting old_gpio_setting;

	if (dev->pdata->msm_i2c_config_gpio)
		return;

	if (!(status & (I2C_STATUS_BUS_ACTIVE)) ||
		(status & (I2C_STATUS_BUS_MASTER)))
		return;

	gpio_clk = dev->i2c_gpios[0];
	gpio_dat = dev->i2c_gpios[1];

	if ((gpio_clk == -1) && (gpio_dat == -1)) {
		dev_err(dev->dev, "Recovery failed due to undefined GPIO's\n");
		return;
	}

	disable_irq(dev->err_irq);
	for (i = 0; i < ARRAY_SIZE(i2c_rsrcs); ++i) {
		if (msm_gpiomux_write(dev->i2c_gpios[i], GPIOMUX_ACTIVE,
				&recovery_config, &old_gpio_setting)) {
			dev_err(dev->dev, "GPIO pins have no active setting\n");
			goto recovery_end;
		}
	}

	dev_warn(dev->dev, "i2c_scl: %d, i2c_sda: %d\n",
		 gpio_get_value(gpio_clk), gpio_get_value(gpio_dat));

	for (i = 0; i < 9; i++) {
		if (gpio_get_value(gpio_dat) && gpio_clk_status)
			break;
		gpio_direction_output(gpio_clk, 0);
		udelay(5);
		gpio_direction_output(gpio_dat, 0);
		udelay(5);
		gpio_direction_input(gpio_clk);
		udelay(5);
		if (!gpio_get_value(gpio_clk))
			udelay(20);
		if (!gpio_get_value(gpio_clk))
			usleep_range(10000, 10000);
		gpio_clk_status = gpio_get_value(gpio_clk);
		gpio_direction_input(gpio_dat);
		udelay(5);
	}

	/* Configure ALT funciton to QUP I2C*/
	for (i = 0; i < ARRAY_SIZE(i2c_rsrcs); ++i) {
		msm_gpiomux_write(dev->i2c_gpios[i], GPIOMUX_ACTIVE,
				&old_gpio_setting, NULL);
	}

	udelay(10);

	status = readl_relaxed(dev->base + QUP_I2C_STATUS);
	if (!(status & I2C_STATUS_BUS_ACTIVE)) {
		dev_info(dev->dev, "Bus busy cleared after %d clock cycles, "
			 "status %x\n",
			 i, status);
		goto recovery_end;
	}

	dev_warn(dev->dev, "Bus still busy, status %x\n", status);

recovery_end:
	enable_irq(dev->err_irq);
}

static int
qup_i2c_xfer(struct i2c_adapter *adap, struct i2c_msg msgs[], int num)
{
	DECLARE_COMPLETION_ONSTACK(complete);
	struct qup_i2c_dev *dev = i2c_get_adapdata(adap);
	int ret;
	int rem = num;
	long timeout;
	int err;

	del_timer_sync(&dev->pwr_timer);
	mutex_lock(&dev->mlock);

	if (dev->suspended) {
		mutex_unlock(&dev->mlock);
		return -EIO;
	}

	if (dev->clk_state == 0) {
		if (dev->clk_ctl == 0) {
			if (dev->pdata->src_clk_rate > 0)
				clk_set_rate(dev->clk,
						dev->pdata->src_clk_rate);
			else
				dev->pdata->src_clk_rate = 19200000;
		}
		qup_i2c_pwr_mgmt(dev, 1);
	}
	/* Initialize QUP registers during first transfer */
	if (dev->clk_ctl == 0) {
		int fs_div;
		int hs_div;
		uint32_t fifo_reg;

		if (dev->gsbi) {
			writel_relaxed(0x2 << 4, dev->gsbi);
			/* GSBI memory is not in the same 1K region as other
			 * QUP registers. mb() here ensures that the GSBI
			 * register is updated in correct order and that the
			 * write has gone through before programming QUP core
			 * registers
			 */
			mb();
		}

		fs_div = ((dev->pdata->src_clk_rate
				/ dev->pdata->clk_freq) / 2) - 3;
		hs_div = 3;
		dev->clk_ctl = ((hs_div & 0x7) << 8) | (fs_div & 0xff);
		fifo_reg = readl_relaxed(dev->base + QUP_IO_MODE);
		if (fifo_reg & 0x3)
			dev->out_blk_sz = (fifo_reg & 0x3) * 16;
		else
			dev->out_blk_sz = 16;
		if (fifo_reg & 0x60)
			dev->in_blk_sz = ((fifo_reg & 0x60) >> 5) * 16;
		else
			dev->in_blk_sz = 16;
		/*
		 * The block/fifo size w.r.t. 'actual data' is 1/2 due to 'tag'
		 * associated with each byte written/received
		 */
		dev->out_blk_sz /= 2;
		dev->in_blk_sz /= 2;
		dev->out_fifo_sz = dev->out_blk_sz *
					(2 << ((fifo_reg & 0x1C) >> 2));
		dev->in_fifo_sz = dev->in_blk_sz *
					(2 << ((fifo_reg & 0x380) >> 7));
		dev_dbg(dev->dev, "QUP IN:bl:%d, ff:%d, OUT:bl:%d, ff:%d\n",
				dev->in_blk_sz, dev->in_fifo_sz,
				dev->out_blk_sz, dev->out_fifo_sz);
	}

	writel_relaxed(1, dev->base + QUP_SW_RESET);
	ret = qup_i2c_poll_state(dev, QUP_RESET_STATE, false);
	if (ret) {
		dev_err(dev->dev, "QUP Busy:Trying to recover\n");
		goto out_err;
	}

	if (dev->num_irqs == 3) {
		enable_irq(dev->in_irq);
		enable_irq(dev->out_irq);
	}
	enable_irq(dev->err_irq);

	/* Initialize QUP registers */
	writel_relaxed(0, dev->base + QUP_CONFIG);
	writel_relaxed(QUP_OPERATIONAL_RESET, dev->base + QUP_OPERATIONAL);
	writel_relaxed(QUP_STATUS_ERROR_FLAGS, dev->base + QUP_ERROR_FLAGS_EN);

	writel_relaxed(I2C_MINI_CORE | I2C_N_VAL, dev->base + QUP_CONFIG);

	/* Initialize I2C mini core registers */
	writel_relaxed(0, dev->base + QUP_I2C_CLK_CTL);
	writel_relaxed(QUP_I2C_STATUS_RESET, dev->base + QUP_I2C_STATUS);

	while (rem) {
		bool filled = false;

		dev->cnt = msgs->len - dev->pos;
		dev->msg = msgs;

		dev->wr_sz = dev->out_fifo_sz;
		dev->err = 0;
		dev->complete = &complete;

		if (qup_i2c_poll_state(dev, QUP_I2C_MAST_GEN, false) != 0) {
			ret = -EIO;
			goto out_err;
		}

		qup_print_status(dev);
		/* HW limits Read upto 256 bytes in 1 read without stop */
		if (dev->msg->flags & I2C_M_RD) {
			qup_set_read_mode(dev, dev->cnt);
			if (dev->cnt > 256)
				dev->cnt = 256;
		} else {
			ret = qup_set_wr_mode(dev, rem);
			if (ret != 0)
				goto out_err;
			/* Don't fill block till we get interrupt */
			if (dev->wr_sz == dev->out_blk_sz)
				filled = true;
		}

		err = qup_update_state(dev, QUP_RUN_STATE);
		if (err < 0) {
			ret = err;
			goto out_err;
		}

		qup_print_status(dev);
		writel_relaxed(dev->clk_ctl, dev->base + QUP_I2C_CLK_CTL);
		/* CLK_CTL register is not in the same 1K region as other QUP
		 * registers. Ensure that clock control is written before
		 * programming other QUP registers
		 */
		mb();

		do {
			int idx = 0;
			uint32_t carry_over = 0;

			/* Transition to PAUSE state only possible from RUN */
			err = qup_update_state(dev, QUP_PAUSE_STATE);
			if (err < 0) {
				ret = err;
				goto out_err;
			}

			qup_print_status(dev);
			/* This operation is Write, check the next operation
			 * and decide mode
			 */
			while (filled == false) {
				if ((msgs->flags & I2C_M_RD))
					qup_issue_read(dev, msgs, &idx,
							carry_over);
				else if (!(msgs->flags & I2C_M_RD))
					qup_issue_write(dev, msgs, rem, &idx,
							&carry_over);
				if (idx >= (dev->wr_sz << 1))
					filled = true;
				/* Start new message */
				if (filled == false) {
					if (msgs->flags & I2C_M_RD)
							filled = true;
					else if (rem > 1) {
						/* Only combine operations with
						 * same address
						 */
						struct i2c_msg *next = msgs + 1;
						if (next->addr != msgs->addr)
							filled = true;
						else {
							rem--;
							msgs++;
							dev->msg = msgs;
							dev->pos = 0;
							dev->cnt = msgs->len;
							if (msgs->len > 256)
								dev->cnt = 256;
						}
					} else
						filled = true;
				}
			}
			err = qup_update_state(dev, QUP_RUN_STATE);
			if (err < 0) {
				ret = err;
				goto out_err;
			}
			dev_dbg(dev->dev, "idx:%d, rem:%d, num:%d, mode:%d\n",
				idx, rem, num, dev->mode);

			qup_print_status(dev);
			timeout = wait_for_completion_timeout(&complete,
					msecs_to_jiffies(dev->out_fifo_sz));
			if (!timeout) {
				uint32_t istatus = readl_relaxed(dev->base +
							QUP_I2C_STATUS);
				uint32_t qstatus = readl_relaxed(dev->base +
							QUP_ERROR_FLAGS);
				uint32_t op_flgs = readl_relaxed(dev->base +
							QUP_OPERATIONAL);

				/*
				 * Dont wait for 1 sec if i2c sees the bus
				 * active and controller is not master.
				 * A slave has pulled line low. Try to recover
				 */
				if (!(istatus & I2C_STATUS_BUS_ACTIVE) ||
					(istatus & I2C_STATUS_BUS_MASTER)) {
					timeout =
					wait_for_completion_timeout(&complete,
									HZ);
					if (timeout)
						goto timeout_err;
				}
				qup_i2c_recover_bus_busy(dev);
				dev_err(dev->dev,
					"Transaction timed out, SL-AD = 0x%x\n",
					dev->msg->addr);

				dev_err(dev->dev, "I2C Status: %x\n", istatus);
				dev_err(dev->dev, "QUP Status: %x\n", qstatus);
				dev_err(dev->dev, "OP Flags: %x\n", op_flgs);
				writel_relaxed(1, dev->base + QUP_SW_RESET);
				/* Make sure that the write has gone through
				 * before returning from the function
				 */
				mb();
				ret = -ETIMEDOUT;
				goto out_err;
			}
timeout_err:
			if (dev->err) {
				if (dev->err > 0 &&
					dev->err & QUP_I2C_NACK_FLAG) {
					dev_err(dev->dev,
					"I2C slave addr:0x%x not connected\n",
					dev->msg->addr);
					dev->err = ENOTCONN;
				} else if (dev->err < 0) {
					dev_err(dev->dev,
					"QUP data xfer error %d\n", dev->err);
					ret = dev->err;
					goto out_err;
				} else if (dev->err > 0) {
					/*
					 * ISR returns +ve error if error code
					 * is I2C related, e.g. unexpected start
					 * So you may call recover-bus-busy when
					 * this error happens
					 */
					qup_i2c_recover_bus_busy(dev);
				}
				ret = -dev->err;
				goto out_err;
			}
			if (dev->msg->flags & I2C_M_RD) {
				int i;
				uint32_t dval = 0;
				for (i = 0; dev->pos < dev->msg->len; i++,
						dev->pos++) {
					uint32_t rd_status =
						readl_relaxed(dev->base
							+ QUP_OPERATIONAL);
					if (i % 2 == 0) {
						if ((rd_status &
							QUP_IN_NOT_EMPTY) == 0)
							break;
						dval = readl_relaxed(dev->base +
							QUP_IN_FIFO_BASE);
						dev->msg->buf[dev->pos] =
							dval & 0xFF;
					} else
						dev->msg->buf[dev->pos] =
							((dval & 0xFF0000) >>
							 16);
				}
				dev->cnt -= i;
			} else
				filled = false; /* refill output FIFO */
			dev_dbg(dev->dev, "pos:%d, len:%d, cnt:%d\n",
					dev->pos, msgs->len, dev->cnt);
		} while (dev->cnt > 0);
		if (dev->cnt == 0) {
			if (msgs->len == dev->pos) {
				rem--;
				msgs++;
				dev->pos = 0;
			}
			if (rem) {
				err = qup_i2c_poll_clock_ready(dev);
				if (err < 0) {
					ret = err;
					goto out_err;
				}
				err = qup_update_state(dev, QUP_RESET_STATE);
				if (err < 0) {
					ret = err;
					goto out_err;
				}
			}
		}
		/* Wait for I2C bus to be idle */
		ret = qup_i2c_poll_writeready(dev, rem);
		if (ret) {
			dev_err(dev->dev,
				"Error waiting for write ready\n");
			goto out_err;
		}
	}

	ret = num;
 out_err:
	disable_irq(dev->err_irq);
	if (dev->num_irqs == 3) {
		disable_irq(dev->in_irq);
		disable_irq(dev->out_irq);
	}
	dev->complete = NULL;
	dev->msg = NULL;
	dev->pos = 0;
	dev->err = 0;
	dev->cnt = 0;
	dev->pwr_timer.expires = jiffies + 3*HZ;
	add_timer(&dev->pwr_timer);
	mutex_unlock(&dev->mlock);
	return ret;
}

static u32
qup_i2c_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | (I2C_FUNC_SMBUS_EMUL & ~I2C_FUNC_SMBUS_QUICK);
}

static const struct i2c_algorithm qup_i2c_algo = {
	.master_xfer	= qup_i2c_xfer,
	.functionality	= qup_i2c_func,
};

static int __devinit
qup_i2c_probe(struct platform_device *pdev)
{
	struct qup_i2c_dev	*dev;
	struct resource         *qup_mem, *gsbi_mem, *qup_io, *gsbi_io, *res;
	struct resource		*in_irq, *out_irq, *err_irq;
	struct clk         *clk, *pclk;
	int ret = 0;
	int i;
	struct msm_i2c_platform_data *pdata;

	gsbi_mem = NULL;
	dev_dbg(&pdev->dev, "qup_i2c_probe\n");

	if (pdev->dev.of_node) {
		struct device_node *node = pdev->dev.of_node;
		pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
		if (!pdata)
			return -ENOMEM;
		ret = of_property_read_u32(node, "qcom,i2c-bus-freq",
					&pdata->clk_freq);
		if (ret)
			goto get_res_failed;
		ret = of_property_read_u32(node, "cell-index", &pdev->id);
		if (ret)
			goto get_res_failed;
		/* Optional property */
		of_property_read_u32(node, "qcom,i2c-src-freq",
					&pdata->src_clk_rate);
	} else
		pdata = pdev->dev.platform_data;

	if (!pdata) {
		dev_err(&pdev->dev, "platform data not initialized\n");
		return -ENOSYS;
	}
	qup_mem = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"qup_phys_addr");
	if (!qup_mem) {
		dev_err(&pdev->dev, "no qup mem resource?\n");
		ret = -ENODEV;
		goto get_res_failed;
	}

	/*
	 * We only have 1 interrupt for new hardware targets and in_irq,
	 * out_irq will be NULL for those platforms
	 */
	in_irq = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
						"qup_in_intr");

	out_irq = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
						"qup_out_intr");

	err_irq = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
						"qup_err_intr");
	if (!err_irq) {
		dev_err(&pdev->dev, "no error irq resource?\n");
		ret = -ENODEV;
		goto get_res_failed;
	}

	qup_io = request_mem_region(qup_mem->start, resource_size(qup_mem),
					pdev->name);
	if (!qup_io) {
		dev_err(&pdev->dev, "QUP region already claimed\n");
		ret = -EBUSY;
		goto get_res_failed;
	}
	if (!pdata->use_gsbi_shared_mode) {
		gsbi_mem = platform_get_resource_byname(pdev, IORESOURCE_MEM,
							"gsbi_qup_i2c_addr");
		if (!gsbi_mem) {
			dev_dbg(&pdev->dev, "Assume BLSP\n");
			/*
			 * BLSP core does not need protocol programming so this
			 * resource is not expected
			 */
			goto blsp_core_init;
		}
		gsbi_io = request_mem_region(gsbi_mem->start,
						resource_size(gsbi_mem),
						pdev->name);
		if (!gsbi_io) {
			dev_err(&pdev->dev, "GSBI region already claimed\n");
			ret = -EBUSY;
			goto err_res_failed;
		}
	}

blsp_core_init:
	clk = clk_get(&pdev->dev, "core_clk");
	if (IS_ERR(clk)) {
		dev_err(&pdev->dev, "Could not get core_clk\n");
		ret = PTR_ERR(clk);
		goto err_clk_get_failed;
	}

	pclk = clk_get(&pdev->dev, "iface_clk");
	if (IS_ERR(pclk)) {
		dev_err(&pdev->dev, "Could not get iface_clk\n");
		ret = PTR_ERR(pclk);
		clk_put(clk);
		goto err_clk_get_failed;
	}

	/* We support frequencies upto FAST Mode(400KHz) */
	if (pdata->clk_freq <= 0 ||
			pdata->clk_freq > 400000) {
		dev_err(&pdev->dev, "clock frequency not supported\n");
		ret = -EIO;
		goto err_config_failed;
	}

	dev = kzalloc(sizeof(struct qup_i2c_dev), GFP_KERNEL);
	if (!dev) {
		ret = -ENOMEM;
		goto err_alloc_dev_failed;
	}

	dev->dev = &pdev->dev;
	if (in_irq)
		dev->in_irq = in_irq->start;
	if (out_irq)
		dev->out_irq = out_irq->start;
	dev->err_irq = err_irq->start;
	if (in_irq && out_irq)
		dev->num_irqs = 3;
	else
		dev->num_irqs = 1;
	dev->clk = clk;
	dev->pclk = pclk;
	dev->base = ioremap(qup_mem->start, resource_size(qup_mem));
	if (!dev->base) {
		ret = -ENOMEM;
		goto err_ioremap_failed;
	}

	/* Configure GSBI block to use I2C functionality */
	if (gsbi_mem) {
		dev->gsbi = ioremap(gsbi_mem->start, resource_size(gsbi_mem));
		if (!dev->gsbi) {
			ret = -ENOMEM;
			goto err_gsbi_failed;
		}
	}

	for (i = 0; i < ARRAY_SIZE(i2c_rsrcs); ++i) {
		res = platform_get_resource_byname(pdev, IORESOURCE_IO,
						   i2c_rsrcs[i]);
		dev->i2c_gpios[i] = res ? res->start : -1;
	}

	ret = qup_i2c_request_gpios(dev);
	if (ret)
		goto err_request_gpio_failed;

	platform_set_drvdata(pdev, dev);

	dev->one_bit_t = (USEC_PER_SEC/pdata->clk_freq) + 1;
	dev->pdata = pdata;
	dev->clk_ctl = 0;
	dev->pos = 0;

	/*
	 * If bootloaders leave a pending interrupt on certain GSBI's,
	 * then we reset the core before registering for interrupts.
	 */
	clk_prepare_enable(dev->clk);
	clk_prepare_enable(dev->pclk);
	writel_relaxed(1, dev->base + QUP_SW_RESET);
	if (qup_i2c_poll_state(dev, 0, true) != 0)
		goto err_reset_failed;
	clk_disable_unprepare(dev->clk);
	clk_disable_unprepare(dev->pclk);

	/*
	 * We use num_irqs to also indicate if we got 3 interrupts or just 1.
	 * If we have just 1, we use err_irq as the general purpose irq
	 * and handle the changes in ISR accordingly
	 * Per Hardware guidelines, if we have 3 interrupts, they are always
	 * edge triggering, and if we have 1, it's always level-triggering
	 */
	if (dev->num_irqs == 3) {
		ret = request_irq(dev->in_irq, qup_i2c_interrupt,
				IRQF_TRIGGER_RISING, "qup_in_intr", dev);
		if (ret) {
			dev_err(&pdev->dev, "request_in_irq failed\n");
			goto err_request_irq_failed;
		}
		/*
		 * We assume out_irq exists if in_irq does since platform
		 * configuration either has 3 interrupts assigned to QUP or 1
		 */
		ret = request_irq(dev->out_irq, qup_i2c_interrupt,
				IRQF_TRIGGER_RISING, "qup_out_intr", dev);
		if (ret) {
			dev_err(&pdev->dev, "request_out_irq failed\n");
			free_irq(dev->in_irq, dev);
			goto err_request_irq_failed;
		}
		ret = request_irq(dev->err_irq, qup_i2c_interrupt,
				IRQF_TRIGGER_RISING, "qup_err_intr", dev);
		if (ret) {
			dev_err(&pdev->dev, "request_err_irq failed\n");
			free_irq(dev->out_irq, dev);
			free_irq(dev->in_irq, dev);
			goto err_request_irq_failed;
		}
	} else {
		ret = request_irq(dev->err_irq, qup_i2c_interrupt,
				IRQF_TRIGGER_HIGH, "qup_err_intr", dev);
		if (ret) {
			dev_err(&pdev->dev, "request_err_irq failed\n");
			goto err_request_irq_failed;
		}
	}
	disable_irq(dev->err_irq);
	if (dev->num_irqs == 3) {
		disable_irq(dev->in_irq);
		disable_irq(dev->out_irq);
	}
	i2c_set_adapdata(&dev->adapter, dev);
	dev->adapter.algo = &qup_i2c_algo;
	strlcpy(dev->adapter.name,
		"QUP I2C adapter",
		sizeof(dev->adapter.name));
	dev->adapter.nr = pdev->id;
	if (pdata->msm_i2c_config_gpio)
		pdata->msm_i2c_config_gpio(dev->adapter.nr, 1);

	dev->suspended = 0;
	mutex_init(&dev->mlock);
	dev->clk_state = 0;
	clk_prepare(dev->clk);
	clk_prepare(dev->pclk);
	setup_timer(&dev->pwr_timer, qup_i2c_pwr_timer, (unsigned long) dev);

	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	ret = i2c_add_numbered_adapter(&dev->adapter);
	if (ret) {
		dev_err(&pdev->dev, "i2c_add_adapter failed\n");
		if (dev->num_irqs == 3) {
			free_irq(dev->out_irq, dev);
			free_irq(dev->in_irq, dev);
		}
		free_irq(dev->err_irq, dev);
	} else {
		if (dev->dev->of_node)
			of_i2c_register_devices(&dev->adapter);
		return 0;
	}


err_request_irq_failed:
	qup_i2c_free_gpios(dev);
	if (dev->gsbi)
		iounmap(dev->gsbi);
err_reset_failed:
	clk_disable_unprepare(dev->clk);
	clk_disable_unprepare(dev->pclk);
err_request_gpio_failed:
err_gsbi_failed:
	iounmap(dev->base);
err_ioremap_failed:
	kfree(dev);
err_alloc_dev_failed:
err_config_failed:
	clk_put(clk);
	clk_put(pclk);
err_clk_get_failed:
	if (gsbi_mem)
		release_mem_region(gsbi_mem->start, resource_size(gsbi_mem));
err_res_failed:
	release_mem_region(qup_mem->start, resource_size(qup_mem));
get_res_failed:
	if (pdev->dev.of_node)
		kfree(pdata);
	return ret;
}

static int __devexit
qup_i2c_remove(struct platform_device *pdev)
{
	struct qup_i2c_dev	*dev = platform_get_drvdata(pdev);
	struct resource		*qup_mem, *gsbi_mem;

	/* Grab mutex to ensure ongoing transaction is over */
	mutex_lock(&dev->mlock);
	dev->suspended = 1;
	mutex_unlock(&dev->mlock);
	mutex_destroy(&dev->mlock);
	del_timer_sync(&dev->pwr_timer);
	if (dev->clk_state != 0)
		qup_i2c_pwr_mgmt(dev, 0);
	platform_set_drvdata(pdev, NULL);
	if (dev->num_irqs == 3) {
		free_irq(dev->out_irq, dev);
		free_irq(dev->in_irq, dev);
	}
	free_irq(dev->err_irq, dev);
	i2c_del_adapter(&dev->adapter);
	clk_unprepare(dev->clk);
	clk_unprepare(dev->pclk);
	clk_put(dev->clk);
	clk_put(dev->pclk);
	qup_i2c_free_gpios(dev);
	if (dev->gsbi)
		iounmap(dev->gsbi);
	iounmap(dev->base);

	pm_runtime_disable(&pdev->dev);

	if (!(dev->pdata->use_gsbi_shared_mode)) {
		gsbi_mem = platform_get_resource_byname(pdev, IORESOURCE_MEM,
							"gsbi_qup_i2c_addr");
		release_mem_region(gsbi_mem->start, resource_size(gsbi_mem));
	}
	qup_mem = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"qup_phys_addr");
	release_mem_region(qup_mem->start, resource_size(qup_mem));
	if (dev->dev->of_node)
		kfree(dev->pdata);
	kfree(dev);
	return 0;
}

#ifdef CONFIG_PM
static int qup_i2c_suspend(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);
	struct qup_i2c_dev *dev = platform_get_drvdata(pdev);

	/* Grab mutex to ensure ongoing transaction is over */
	mutex_lock(&dev->mlock);
	dev->suspended = 1;
	mutex_unlock(&dev->mlock);
	del_timer_sync(&dev->pwr_timer);
	if (dev->clk_state != 0)
		qup_i2c_pwr_mgmt(dev, 0);
	clk_unprepare(dev->clk);
	clk_unprepare(dev->pclk);
	qup_i2c_free_gpios(dev);
	return 0;
}

static int qup_i2c_resume(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);
	struct qup_i2c_dev *dev = platform_get_drvdata(pdev);
	BUG_ON(qup_i2c_request_gpios(dev) != 0);
	clk_prepare(dev->clk);
	clk_prepare(dev->pclk);
	dev->suspended = 0;
	return 0;
}
#endif /* CONFIG_PM */

#ifdef CONFIG_PM_RUNTIME
static int i2c_qup_runtime_idle(struct device *dev)
{
	dev_dbg(dev, "pm_runtime: idle...\n");
	return 0;
}

static int i2c_qup_runtime_suspend(struct device *dev)
{
	dev_dbg(dev, "pm_runtime: suspending...\n");
	return 0;
}

static int i2c_qup_runtime_resume(struct device *dev)
{
	dev_dbg(dev, "pm_runtime: resuming...\n");
	return 0;
}
#endif

static const struct dev_pm_ops i2c_qup_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(
		qup_i2c_suspend,
		qup_i2c_resume
	)
	SET_RUNTIME_PM_OPS(
		i2c_qup_runtime_suspend,
		i2c_qup_runtime_resume,
		i2c_qup_runtime_idle
	)
};

static struct of_device_id i2c_qup_dt_match[] = {
	{
		.compatible = "qcom,i2c-qup",
	},
	{}
};

static struct platform_driver qup_i2c_driver = {
	.probe		= qup_i2c_probe,
	.remove		= __devexit_p(qup_i2c_remove),
	.driver		= {
		.name	= "qup_i2c",
		.owner	= THIS_MODULE,
		.pm = &i2c_qup_dev_pm_ops,
		.of_match_table = i2c_qup_dt_match,
	},
};

/* QUP may be needed to bring up other drivers */
static int __init
qup_i2c_init_driver(void)
{
	return platform_driver_register(&qup_i2c_driver);
}
arch_initcall(qup_i2c_init_driver);

static void __exit qup_i2c_exit_driver(void)
{
	platform_driver_unregister(&qup_i2c_driver);
}
module_exit(qup_i2c_exit_driver);

