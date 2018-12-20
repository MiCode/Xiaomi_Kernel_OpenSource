// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/i3c/master.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/qcom-geni-se.h>
#include <linux/pinctrl/consumer.h>
#include <linux/ipc_logging.h>

#define SE_I3C_SCL_HIGH			0x268
#define SE_I3C_TX_TRANS_LEN		0x26C
#define SE_I3C_RX_TRANS_LEN		0x270
#define SE_I3C_DELAY_COUNTER		0x274
#define SE_I2C_SCL_COUNTERS		0x278
#define SE_I3C_SCL_CYCLE		0x27C
#define SE_GENI_HW_IRQ_EN		0x920
#define SE_GENI_HW_IRQ_IGNORE_ON_ACTIVE	0x924
#define SE_GENI_HW_IRQ_CMD_PARAM_0	0x930

/* SE_GENI_M_CLK_CFG field shifts */
#define CLK_DEV_VALUE_SHFT	4
#define SER_CLK_EN_SHFT		0

/* SE_GENI_HW_IRQ_CMD_PARAM_0 field shifts */
#define M_IBI_IRQ_PARAM_7E_SHFT		0
#define M_IBI_IRQ_PARAM_STOP_STALL_SHFT	1

/* SE_I2C_SCL_COUNTERS field shifts */
#define I2C_SCL_HIGH_COUNTER_SHFT	20
#define I2C_SCL_LOW_COUNTER_SHFT	10

#define	SE_I3C_ERR  (M_CMD_OVERRUN_EN | M_ILLEGAL_CMD_EN | M_CMD_FAILURE_EN |\
	M_CMD_ABORT_EN | M_GP_IRQ_0_EN | M_GP_IRQ_1_EN | M_GP_IRQ_2_EN | \
	M_GP_IRQ_3_EN | M_GP_IRQ_4_EN)

/* M_CMD OP codes for I2C/I3C */
#define I3C_READ_IBI_HW			0
#define I2C_WRITE			1
#define I2C_READ			2
#define I2C_WRITE_READ			3
#define I2C_ADDR_ONLY			4
#define I3C_INBAND_RESET		5
#define I2C_BUS_CLEAR			6
#define I2C_STOP_ON_BUS			7
#define I3C_HDR_DDR_EXIT		8
#define I3C_PRIVATE_WRITE		9
#define I3C_PRIVATE_READ		10
#define I3C_HDR_DDR_WRITE		11
#define I3C_HDR_DDR_READ		12
#define I3C_DIRECT_CCC_ADDR_ONLY	13
#define I3C_BCAST_CCC_ADDR_ONLY		14
#define I3C_READ_IBI			15
#define I3C_BCAST_CCC_WRITE		16
#define I3C_DIRECT_CCC_WRITE		17
#define I3C_DIRECT_CCC_READ		18
/* M_CMD params for I3C */
#define PRE_CMD_DELAY		BIT(0)
#define TIMESTAMP_BEFORE	BIT(1)
#define STOP_STRETCH		BIT(2)
#define TIMESTAMP_AFTER		BIT(3)
#define POST_COMMAND_DELAY	BIT(4)
#define IGNORE_ADD_NACK		BIT(6)
#define READ_FINISHED_WITH_ACK	BIT(7)
#define CONTINUOUS_MODE_DAA	BIT(8)
#define SLV_ADDR_MSK		GENMASK(15, 9)
#define SLV_ADDR_SHFT		9
#define CCC_HDR_CMD_MSK		GENMASK(23, 16)
#define CCC_HDR_CMD_SHFT	16
#define IBI_NACK_TBL_CTRL	BIT(24)
#define USE_7E			BIT(25)
#define BYPASS_ADDR_PHASE	BIT(26)

enum geni_i3c_err_code {
	RD_TERM,
	NACK,
	CRC_ERR,
	BUS_PROTO,
	NACK_7E,
	NACK_IBI,
	GENI_OVERRUN,
	GENI_ILLEGAL_CMD,
	GENI_ABORT_DONE,
	GENI_TIMEOUT,
};

#define DM_I3C_CB_ERR   ((BIT(NACK) | BIT(BUS_PROTO) | BIT(NACK_7E)) << 5)

#define I3C_AUTO_SUSPEND_DELAY	250
#define KHZ(freq)		(1000 * freq)
#define PACKING_BYTES_PW	4
#define XFER_TIMEOUT		HZ
#define DFS_INDEX_MAX		7
#define I3C_CORE2X_VOTE		(960)
#define DEFAULT_BUS_WIDTH	(4)
#define DEFAULT_SE_CLK		(19200000)

#define I3C_DDR_READ_CMD BIT(7)
#define I3C_ADDR_MASK	0x7f

enum i3c_trans_dir {
	WRITE_TRANSACTION = 0,
	READ_TRANSACTION = 1
};

struct geni_se {
	void __iomem *base;
	struct device *dev;
	struct se_geni_rsc i3c_rsc;
};

struct geni_i3c_dev {
	struct geni_se se;
	unsigned int tx_wm;
	int irq;
	int err;
	struct i3c_master_controller ctrlr;
	void *ipcl;
	struct completion done;
	struct mutex lock;
	spinlock_t spinlock;
	u32 clk_src_freq;
	u32 dfs_idx;
	u8 *cur_buf;
	enum i3c_trans_dir cur_rnw;
	int cur_len;
	int cur_idx;
	unsigned long newaddrslots[(I3C_ADDR_MASK + 1) / BITS_PER_LONG];
	const struct geni_i3c_clk_fld *clk_fld;
};

struct geni_i3c_i2c_dev_data {
	u32 dummy;  /* placeholder for now, later will hold IBI information */
};

struct i3c_xfer_params {
	enum se_xfer_mode mode;
	u32 m_cmd;
	u32 m_param;
};

struct geni_i3c_err_log {
	int err;
	const char *msg;
};

static struct geni_i3c_err_log gi3c_log[] = {
	[RD_TERM] = { -EINVAL, "I3C slave early read termination" },
	[NACK] = { -ENOTCONN, "NACK: slave unresponsive, check power/reset" },
	[CRC_ERR] = { -EINVAL, "CRC or parity error" },
	[BUS_PROTO] = { -EPROTO, "Bus proto err, noisy/unexpected start/stop" },
	[NACK_7E] = { -EBUSY, "NACK on 7E, unexpected protocol error" },
	[NACK_IBI] = { -EINVAL, "NACK on IBI" },
	[GENI_OVERRUN] = { -EIO, "Cmd overrun, check GENI cmd-state machine" },
	[GENI_ILLEGAL_CMD] = { -EILSEQ,
				"Illegal cmd, check GENI cmd-state machine" },
	[GENI_ABORT_DONE] = { -ETIMEDOUT, "Abort after timeout successful" },
	[GENI_TIMEOUT] = { -ETIMEDOUT, "I3C transaction timed out" },
};

struct geni_i3c_clk_fld {
	u32 clk_freq_out;
	u32 clk_src_freq;
	u8  clk_div;
	u8  i2c_t_high_cnt;
	u8  i2c_t_low_cnt;
	u8  i2c_t_cycle_cnt;
	u8  i3c_t_high_cnt;
	u8  i3c_t_cycle_cnt;
};

static struct geni_i3c_dev*
to_geni_i3c_master(struct i3c_master_controller *master)
{
	return container_of(master, struct geni_i3c_dev, ctrlr);
}

/*
 * Hardware uses the underlying formula to calculate time periods of
 * SCL clock cycle. Firmware uses some additional cycles excluded from the
 * below formula and it is confirmed that the time periods are within
 * specification limits.
 *
 * time of high period of I2C SCL:
 *         i2c_t_high = (i2c_t_high_cnt * clk_div) / source_clock
 * time of low period of I2C SCL:
 *         i2c_t_low = (i2c_t_low_cnt * clk_div) / source_clock
 * time of full period of I2C SCL:
 *         i2c_t_cycle = (i2c_t_cycle_cnt * clk_div) / source_clock
 * time of high period of I3C SCL:
 *         i3c_t_high = (i3c_t_high_cnt * clk_div) / source_clock
 * time of full period of I3C SCL:
 *         i3c_t_cycle = (i3c_t_cycle_cnt * clk_div) / source_clock
 * clk_freq_out = t / t_cycle
 */
static const struct geni_i3c_clk_fld geni_i3c_clk_map[] = {
	{ KHZ(100),    19200, 7, 10, 11, 26, 0, 0 },
	{ KHZ(400),    19200, 2,  5, 12, 24, 0, 0 },
	{ KHZ(1000),   19200, 1,  3,  9, 18, 0, 0 },
	{ KHZ(12500), 100000, 1, 60, 140, 250, 8, 16 },
};

static int geni_i3c_clk_map_idx(struct geni_i3c_dev *gi3c)
{
	int i;
	struct i3c_master_controller *m = &gi3c->ctrlr;
	const struct geni_i3c_clk_fld *itr = geni_i3c_clk_map;
	struct i3c_bus *bus = i3c_master_get_bus(m);

	for (i = 0; i < ARRAY_SIZE(geni_i3c_clk_map); i++, itr++) {
		if ((!bus ||
			 itr->clk_freq_out == bus->scl_rate.i3c) &&
			 KHZ(itr->clk_src_freq) == gi3c->clk_src_freq) {
			gi3c->clk_fld = itr;
			return 0;
		}
	}

	return -EINVAL;
}

static void set_new_addr_slot(unsigned long *addrslot, u8 addr)
{
	unsigned long *ptr;

	if (addr > I3C_ADDR_MASK)
		return;

	ptr = addrslot + (addr / BITS_PER_LONG);
	*ptr |= 1 << (addr % BITS_PER_LONG);
}

static void clear_new_addr_slot(unsigned long *addrslot, u8 addr)
{
	unsigned long *ptr;

	if (addr > I3C_ADDR_MASK)
		return;

	ptr = addrslot + (addr / BITS_PER_LONG);
	*ptr &= ~(1 << (addr % BITS_PER_LONG));
}

static bool is_new_addr_slot_set(unsigned long *addrslot, u8 addr)
{
	unsigned long *ptr;

	if (addr > I3C_ADDR_MASK)
		return false;

	ptr = addrslot + (addr / BITS_PER_LONG);
	return ((*ptr & (1 << (addr % BITS_PER_LONG))) != 0);
}

static void qcom_geni_i3c_conf(struct geni_i3c_dev *gi3c)
{
	const struct geni_i3c_clk_fld *itr = gi3c->clk_fld;
	u32 val;
	unsigned long freq;
	int ret = 0;

	if (gi3c->dfs_idx > DFS_INDEX_MAX)
		ret = geni_se_clk_freq_match(&gi3c->se.i3c_rsc,
				KHZ(itr->clk_src_freq),
				&gi3c->dfs_idx, &freq, false);
	if (ret)
		gi3c->dfs_idx = 0;

	writel_relaxed(gi3c->dfs_idx, gi3c->se.base + SE_GENI_CLK_SEL);

	val = itr->clk_div << CLK_DEV_VALUE_SHFT;
	val |= 1 << SER_CLK_EN_SHFT;
	writel_relaxed(val, gi3c->se.base + GENI_SER_M_CLK_CFG);

	val = itr->i2c_t_high_cnt << I2C_SCL_HIGH_COUNTER_SHFT;
	val |= itr->i2c_t_low_cnt << I2C_SCL_LOW_COUNTER_SHFT;
	val |= itr->i2c_t_cycle_cnt;
	writel_relaxed(val, gi3c->se.base + SE_I2C_SCL_COUNTERS);

	writel_relaxed(itr->i3c_t_cycle_cnt, gi3c->se.base + SE_I3C_SCL_CYCLE);
	writel_relaxed(itr->i3c_t_high_cnt, gi3c->se.base + SE_I3C_SCL_HIGH);

	writel_relaxed(1, gi3c->se.base + SE_GENI_HW_IRQ_IGNORE_ON_ACTIVE);

	val = 1 << M_IBI_IRQ_PARAM_STOP_STALL_SHFT;
	val |= 1 << M_IBI_IRQ_PARAM_7E_SHFT;
	writel_relaxed(val, gi3c->se.base + SE_GENI_HW_IRQ_CMD_PARAM_0);

	writel_relaxed(1, gi3c->se.base + SE_GENI_HW_IRQ_EN);
}

static void geni_i3c_err(struct geni_i3c_dev *gi3c, int err)
{
	if (gi3c->cur_rnw == WRITE_TRANSACTION)
		dev_dbg(gi3c->se.dev, "len:%d, write\n", gi3c->cur_len);
	else
		dev_dbg(gi3c->se.dev, "len:%d, read\n", gi3c->cur_len);

	dev_dbg(gi3c->se.dev, "%s\n", gi3c_log[err].msg);
	gi3c->err = gi3c_log[err].err;
}

static irqreturn_t geni_i3c_irq(int irq, void *dev)
{
	struct geni_i3c_dev *gi3c = dev;
	int j;
	u32 m_stat, m_stat_mask, rx_st;
	u32 dm_tx_st, dm_rx_st, dma;
	unsigned long flags;

	spin_lock_irqsave(&gi3c->spinlock, flags);

	m_stat = readl_relaxed(gi3c->se.base + SE_GENI_M_IRQ_STATUS);
	m_stat_mask = readl_relaxed(gi3c->se.base + SE_GENI_M_IRQ_EN);
	rx_st = readl_relaxed(gi3c->se.base + SE_GENI_RX_FIFO_STATUS);
	dm_tx_st = readl_relaxed(gi3c->se.base + SE_DMA_TX_IRQ_STAT);
	dm_rx_st = readl_relaxed(gi3c->se.base + SE_DMA_RX_IRQ_STAT);
	dma = readl_relaxed(gi3c->se.base + SE_GENI_DMA_MODE_EN);

	if ((m_stat   & SE_I3C_ERR) ||
		(dm_rx_st & DM_I3C_CB_ERR)) {
		if (m_stat & M_GP_IRQ_0_EN)
			geni_i3c_err(gi3c, RD_TERM);
		if (m_stat & M_GP_IRQ_1_EN)
			geni_i3c_err(gi3c, NACK);
		if (m_stat & M_GP_IRQ_2_EN)
			geni_i3c_err(gi3c, CRC_ERR);
		if (m_stat & M_GP_IRQ_3_EN)
			geni_i3c_err(gi3c, BUS_PROTO);
		if (m_stat & M_GP_IRQ_4_EN)
			geni_i3c_err(gi3c, NACK_7E);
		if (m_stat & M_CMD_OVERRUN_EN)
			geni_i3c_err(gi3c, GENI_OVERRUN);
		if (m_stat & M_ILLEGAL_CMD_EN)
			geni_i3c_err(gi3c, GENI_ILLEGAL_CMD);
		if (m_stat & M_CMD_ABORT_EN)
			geni_i3c_err(gi3c, GENI_ABORT_DONE);

		/* Disable the TX Watermark interrupt to stop TX */
		if (!dma)
			writel_relaxed(0, gi3c->se.base +
				SE_GENI_TX_WATERMARK_REG);
		goto irqret;
	}

	if (dma) {
		dev_dbg(gi3c->se.dev, "i3c dma tx:0x%x, dma rx:0x%x\n",
			dm_tx_st, dm_rx_st);
		goto irqret;
	}

	if ((m_stat & (M_RX_FIFO_WATERMARK_EN | M_RX_FIFO_LAST_EN)) &&
		(gi3c->cur_rnw == READ_TRANSACTION) &&
		gi3c->cur_buf) {
		u32 rxcnt = rx_st & RX_FIFO_WC_MSK;

		for (j = 0; j < rxcnt; j++) {
			u32 val;
			int p = 0;

			val = readl_relaxed(gi3c->se.base + SE_GENI_RX_FIFOn);
			while (gi3c->cur_idx < gi3c->cur_len &&
				 p < sizeof(val)) {
				gi3c->cur_buf[gi3c->cur_idx++] = val & 0xff;
				val >>= 8;
				p++;
			}
			if (gi3c->cur_idx == gi3c->cur_len)
				break;
		}
	} else if ((m_stat & M_TX_FIFO_WATERMARK_EN) &&
		(gi3c->cur_rnw == WRITE_TRANSACTION) &&
		(gi3c->cur_buf)) {
		for (j = 0; j < gi3c->tx_wm; j++) {
			u32 temp;
			u32 val = 0;
			int p = 0;

			while (gi3c->cur_idx < gi3c->cur_len &&
					p < sizeof(val)) {
				temp = gi3c->cur_buf[gi3c->cur_idx++];
				val |= temp << (p * 8);
				p++;
			}
			writel_relaxed(val, gi3c->se.base + SE_GENI_TX_FIFOn);
			if (gi3c->cur_idx == gi3c->cur_len) {
				writel_relaxed(0, gi3c->se.base +
					SE_GENI_TX_WATERMARK_REG);
				break;
			}
		}
	}
irqret:
	if (m_stat)
		writel_relaxed(m_stat, gi3c->se.base + SE_GENI_M_IRQ_CLEAR);

	if (dma) {
		if (dm_tx_st)
			writel_relaxed(dm_tx_st,
				gi3c->se.base + SE_DMA_TX_IRQ_CLR);
		if (dm_rx_st)
			writel_relaxed(dm_rx_st,
				gi3c->se.base + SE_DMA_RX_IRQ_CLR);
	}
	/* if this is err with done-bit not set, handle that through timeout. */
	if (m_stat & M_CMD_DONE_EN || m_stat & M_CMD_ABORT_EN) {
		writel_relaxed(0, gi3c->se.base + SE_GENI_TX_WATERMARK_REG);
		complete(&gi3c->done);
	} else if ((dm_tx_st & TX_DMA_DONE) ||
		(dm_rx_st & RX_DMA_DONE) ||
		(dm_rx_st & RX_RESET_DONE))
		complete(&gi3c->done);

	spin_unlock_irqrestore(&gi3c->spinlock, flags);
	return IRQ_HANDLED;
}

static int i3c_geni_runtime_get_mutex_lock(struct geni_i3c_dev *gi3c)
{
	int ret;

	mutex_lock(&gi3c->lock);

	reinit_completion(&gi3c->done);
	ret = pm_runtime_get_sync(gi3c->se.dev);
	if (ret < 0) {
		dev_err(gi3c->se.dev,
			"error turning on SE resources:%d\n", ret);
		pm_runtime_put_noidle(gi3c->se.dev);
		/* Set device in suspended since resume failed */
		pm_runtime_set_suspended(gi3c->se.dev);

		mutex_unlock(&gi3c->lock);
		return ret;
	}

	qcom_geni_i3c_conf(gi3c);

	return 0; /* return 0 to indicate SUCCESS */
}

static void i3c_geni_runtime_put_mutex_unlock(struct geni_i3c_dev *gi3c)
{
	pm_runtime_mark_last_busy(gi3c->se.dev);
	pm_runtime_put_autosuspend(gi3c->se.dev);
	mutex_unlock(&gi3c->lock);
}

static int _i3c_geni_execute_command
(
	struct geni_i3c_dev *gi3c,
	struct i3c_xfer_params *xfer
)
{
	dma_addr_t tx_dma = 0;
	dma_addr_t rx_dma = 0;
	int ret, time_remaining = 0;
	enum i3c_trans_dir rnw = gi3c->cur_rnw;
	u32 len = gi3c->cur_len;

	geni_se_select_mode(gi3c->se.base, xfer->mode);

	gi3c->err = 0;
	gi3c->cur_idx = 0;

	if (rnw == READ_TRANSACTION) {
		dev_dbg(gi3c->se.dev, "I3C cmd:0x%x param:0x%x READ len:%d\n",
			xfer->m_cmd, xfer->m_param, len);
		writel_relaxed(len, gi3c->se.base + SE_I3C_RX_TRANS_LEN);
		geni_setup_m_cmd(gi3c->se.base, xfer->m_cmd, xfer->m_param);
		if (xfer->mode == SE_DMA) {
			ret = geni_se_rx_dma_prep(gi3c->se.i3c_rsc.wrapper_dev,
					gi3c->se.base, gi3c->cur_buf,
					len, &rx_dma);
			if (ret) {
				xfer->mode = FIFO_MODE;
				geni_se_select_mode(gi3c->se.base, xfer->mode);
			}
		}
	} else {
		dev_dbg(gi3c->se.dev, "I3C cmd:0x%x param:0x%x WRITE len:%d\n",
			xfer->m_cmd, xfer->m_param, len);
		writel_relaxed(len, gi3c->se.base + SE_I3C_TX_TRANS_LEN);
		geni_setup_m_cmd(gi3c->se.base, xfer->m_cmd, xfer->m_param);
		if (xfer->mode == SE_DMA) {
			ret = geni_se_tx_dma_prep(gi3c->se.i3c_rsc.wrapper_dev,
					gi3c->se.base, gi3c->cur_buf,
					len, &tx_dma);
			if (ret) {
				xfer->mode = FIFO_MODE;
				geni_se_select_mode(gi3c->se.base, xfer->mode);
			}
		}
		if (xfer->mode == FIFO_MODE && len > 0) /* Get FIFO IRQ */
			writel_relaxed(1, gi3c->se.base +
				SE_GENI_TX_WATERMARK_REG);
	}
	time_remaining = wait_for_completion_timeout(&gi3c->done,
						XFER_TIMEOUT);
	if (!time_remaining) {
		unsigned long flags;

		spin_lock_irqsave(&gi3c->spinlock, flags);
		geni_i3c_err(gi3c, GENI_TIMEOUT);
		gi3c->cur_buf = NULL;
		gi3c->cur_len = gi3c->cur_idx = 0;
		gi3c->cur_rnw = 0;
		geni_abort_m_cmd(gi3c->se.base);
		spin_unlock_irqrestore(&gi3c->spinlock, flags);
		time_remaining = wait_for_completion_timeout(&gi3c->done,
							XFER_TIMEOUT);
	}
	if (xfer->mode == SE_DMA) {
		if (gi3c->err) {
			if (rnw == READ_TRANSACTION)
				writel_relaxed(1, gi3c->se.base +
					SE_DMA_TX_FSM_RST);
			else
				writel_relaxed(1, gi3c->se.base +
					SE_DMA_RX_FSM_RST);
			wait_for_completion_timeout(&gi3c->done, XFER_TIMEOUT);
		}
		geni_se_rx_dma_unprep(gi3c->se.i3c_rsc.wrapper_dev,
				rx_dma, len);
		geni_se_tx_dma_unprep(gi3c->se.i3c_rsc.wrapper_dev,
				tx_dma, len);
	}
	ret = gi3c->err;
	if (gi3c->err)
		dev_err(gi3c->se.dev, "I3C transaction error :%d\n", gi3c->err);

	gi3c->cur_buf = NULL;
	gi3c->cur_len = gi3c->cur_idx = 0;
	gi3c->cur_rnw = 0;
	gi3c->err = 0;

	return ret;
}

static int i3c_geni_execute_read_command
(
	struct geni_i3c_dev *gi3c,
	struct i3c_xfer_params *xfer,
	u8 *buf,
	u32 len
)
{
	gi3c->cur_rnw = READ_TRANSACTION;
	gi3c->cur_buf = buf;
	gi3c->cur_len = len;
	return _i3c_geni_execute_command(gi3c, xfer);
}

static int i3c_geni_execute_write_command
(
	struct geni_i3c_dev *gi3c,
	struct i3c_xfer_params *xfer,
	u8 *buf,
	u32 len
)
{
	gi3c->cur_rnw = WRITE_TRANSACTION;
	gi3c->cur_buf = buf;
	gi3c->cur_len = len;
	return _i3c_geni_execute_command(gi3c, xfer);
}

static void geni_i3c_perform_daa(struct geni_i3c_dev *gi3c)
{
	struct i3c_master_controller *m = &gi3c->ctrlr;
	struct i3c_bus *bus = i3c_master_get_bus(m);
	u8 last_dyn_addr = 0;
	int ret;

	while (1) {
		u8 rx_buf[8], tx_buf[8];
		struct i3c_xfer_params xfer = { FIFO_MODE };
		struct i3c_device_info info = { 0 };
		struct i3c_dev_desc *i3cdev;
		bool new_device = true;
		u64 pid;
		u8 bcr, dcr, addr;

		dev_dbg(gi3c->se.dev, "i3c entdaa read\n");

		xfer.m_cmd = I2C_READ;
		xfer.m_param = STOP_STRETCH | CONTINUOUS_MODE_DAA | USE_7E;

		ret = i3c_geni_execute_read_command(gi3c, &xfer, rx_buf, 8);
		if (ret)
			break;

		dcr = rx_buf[7];
		bcr = rx_buf[6];
		pid = ((u64)rx_buf[0] << 40) |
			((u64)rx_buf[1] << 32) |
			((u64)rx_buf[2] << 24) |
			((u64)rx_buf[3] << 16) |
			((u64)rx_buf[4] <<  8) |
			((u64)rx_buf[5]);

		i3c_bus_for_each_i3cdev(bus, i3cdev) {
			i3c_device_get_info(i3cdev->dev, &info);
			if (pid == info.pid &&
				dcr == info.dcr &&
				bcr == info.bcr) {
				new_device = false;
				addr = (info.dyn_addr) ? info.dyn_addr :
					info.static_addr;
				break;
			}
		}

		if (new_device) {
			ret = i3c_master_get_free_addr(m,
						last_dyn_addr + 1);
			if (ret < 0)
				goto daa_err;
			addr = last_dyn_addr = (u8)ret;
			set_new_addr_slot(gi3c->newaddrslots, addr);
		}

		tx_buf[0] = (addr & I3C_ADDR_MASK) << 1;
		tx_buf[0] |= ~(hweight8(addr & I3C_ADDR_MASK) & 1);

		dev_dbg(gi3c->se.dev, "i3c entdaa write\n");

		xfer.m_cmd = I2C_WRITE;
		xfer.m_param = STOP_STRETCH | BYPASS_ADDR_PHASE | USE_7E;

		ret = i3c_geni_execute_write_command(gi3c, &xfer, tx_buf, 1);
		if (ret)
			break;
	}
daa_err:
	return;
}

static int geni_i3c_master_send_ccc_cmd
(
	struct i3c_master_controller *m,
	struct i3c_ccc_cmd *cmd
)
{
	struct geni_i3c_dev *gi3c = to_geni_i3c_master(m);
	int i, ret;

	if (!(cmd->id & I3C_CCC_DIRECT) && (cmd->ndests != 1))
		return -EINVAL;

	ret = i3c_geni_runtime_get_mutex_lock(gi3c);
	if (ret)
		return ret;

	for (i = 0; i < cmd->ndests; i++) {
		int stall = (i < (cmd->ndests - 1)) ||
			(cmd->id == I3C_CCC_ENTDAA);
		struct i3c_xfer_params xfer = { FIFO_MODE };

		xfer.m_param  = (stall ? STOP_STRETCH : 0);
		xfer.m_param |= (cmd->id << CCC_HDR_CMD_SHFT);
		xfer.m_param |= IBI_NACK_TBL_CTRL;
		if (cmd->id & I3C_CCC_DIRECT) {
			xfer.m_param |= ((cmd->dests[i].addr & I3C_ADDR_MASK)
					<< SLV_ADDR_SHFT);
			if (cmd->rnw) {
				if (i == 0)
					xfer.m_cmd = I3C_DIRECT_CCC_READ;
				else
					xfer.m_cmd = I3C_PRIVATE_READ;
			} else {
				if (i == 0)
					xfer.m_cmd =
					   (cmd->dests[i].payload.len > 0) ?
						I3C_DIRECT_CCC_WRITE :
						I3C_DIRECT_CCC_ADDR_ONLY;
				else
					xfer.m_cmd = I3C_PRIVATE_WRITE;
			}
		} else {
			if (cmd->dests[i].payload.len > 0)
				xfer.m_cmd = I3C_BCAST_CCC_WRITE;
			else
				xfer.m_cmd = I3C_BCAST_CCC_ADDR_ONLY;
		}

		if (i == 0)
			xfer.m_param |= USE_7E;

		if (cmd->rnw)
			ret = i3c_geni_execute_read_command(gi3c, &xfer,
				cmd->dests[i].payload.data,
				cmd->dests[i].payload.len);
		else
			ret = i3c_geni_execute_write_command(gi3c, &xfer,
				cmd->dests[i].payload.data,
				cmd->dests[i].payload.len);
		if (ret)
			break;

		if (cmd->id == I3C_CCC_ENTDAA)
			geni_i3c_perform_daa(gi3c);
	}

	dev_dbg(gi3c->se.dev, "i3c ccc: txn ret:%d\n", ret);

	i3c_geni_runtime_put_mutex_unlock(gi3c);

	return ret;
}

static int geni_i3c_master_priv_xfers
(
	struct i3c_dev_desc *dev,
	struct i3c_priv_xfer *xfers,
	int nxfers
)
{
	struct i3c_master_controller *m = i3c_dev_get_master(dev);
	struct geni_i3c_dev *gi3c = to_geni_i3c_master(m);
	int i, ret;
	bool use_7e = true;

	if (nxfers <= 0)
		return 0;

	ret = i3c_geni_runtime_get_mutex_lock(gi3c);
	if (ret)
		return ret;

	for (i = 0; i < nxfers; i++) {
		bool stall = (i < (nxfers - 1));
		struct i3c_xfer_params xfer = { FIFO_MODE };

		xfer.m_param  = (stall ? STOP_STRETCH : 0);
		xfer.m_param |= ((dev->info.dyn_addr & I3C_ADDR_MASK)
				<< SLV_ADDR_SHFT);
		xfer.m_param |= (use_7e) ? USE_7E : 0;

		/* Update use_7e status for next loop iteration */
		use_7e = !stall;

		if (xfers[i].rnw) {
			xfer.m_cmd = I3C_PRIVATE_READ;
			ret = i3c_geni_execute_read_command(gi3c, &xfer,
				(u8 *)xfers[i].data.in,
				xfers[i].len);
		} else {
			xfer.m_cmd = I3C_PRIVATE_WRITE;
			ret = i3c_geni_execute_write_command(gi3c, &xfer,
				(u8 *)xfers[i].data.out,
				xfers[i].len);
		}

		if (ret)
			break;
	}

	dev_dbg(gi3c->se.dev, "i3c priv: txn ret:%d\n", ret);

	i3c_geni_runtime_put_mutex_unlock(gi3c);

	return ret;
}

static int geni_i3c_master_i2c_xfers
(
	struct i2c_dev_desc *dev,
	const struct i2c_msg *msgs,
	int num
)
{
	struct i3c_master_controller *m = i2c_dev_get_master(dev);
	struct geni_i3c_dev *gi3c = to_geni_i3c_master(m);
	int i, ret;

	ret = i3c_geni_runtime_get_mutex_lock(gi3c);
	if (ret)
		return ret;

	dev_dbg(gi3c->se.dev, "i2c xfer:num:%d, msgs:len:%d,flg:%d\n",
		num, msgs[0].len, msgs[0].flags);
	for (i = 0; i < num; i++) {
		struct i3c_xfer_params xfer;

		xfer.m_cmd    = (msgs[i].flags & I2C_M_RD) ? I2C_READ :
							I2C_WRITE;
		xfer.m_param  = (i < (num - 1)) ? STOP_STRETCH : 0;
		xfer.m_param |= ((msgs[i].addr & I3C_ADDR_MASK)
				<< SLV_ADDR_SHFT);
		xfer.mode     = msgs[i].len > 32 ? SE_DMA : FIFO_MODE;
		if (msgs[i].flags & I2C_M_RD)
			ret = i3c_geni_execute_read_command(gi3c, &xfer,
						msgs[i].buf, msgs[i].len);
		else
			ret = i3c_geni_execute_write_command(gi3c, &xfer,
						msgs[i].buf, msgs[i].len);
		if (ret)
			break;
	}

	dev_dbg(gi3c->se.dev, "i2c: txn ret:%d\n", ret);

	i3c_geni_runtime_put_mutex_unlock(gi3c);

	return ret;
}

static u32 geni_i3c_master_i2c_funcs(struct i3c_master_controller *m)
{
	return I2C_FUNC_I2C;
}

static int geni_i3c_master_attach_i2c_dev(struct i2c_dev_desc *dev)
{
	struct i3c_master_controller *m = i2c_dev_get_master(dev);
	struct geni_i3c_dev *gi3c = to_geni_i3c_master(m);
	struct geni_i3c_i2c_dev_data *data;

	data = devm_kzalloc(gi3c->se.dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	i2c_dev_set_master_data(dev, data);

	return 0;
}

static void geni_i3c_master_detach_i2c_dev(struct i2c_dev_desc *dev)
{
	struct geni_i3c_i2c_dev_data *data = i2c_dev_get_master_data(dev);

	i2c_dev_set_master_data(dev, NULL);
	kfree(data);
}

static int geni_i3c_master_attach_i3c_dev(struct i3c_dev_desc *dev)
{
	struct i3c_master_controller *m = i3c_dev_get_master(dev);
	struct geni_i3c_dev *gi3c = to_geni_i3c_master(m);
	struct geni_i3c_i2c_dev_data *data;

	data = devm_kzalloc(gi3c->se.dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	i3c_dev_set_master_data(dev, data);

	return 0;
}

static int geni_i3c_master_reattach_i3c_dev
(
	struct i3c_dev_desc *dev,
	u8 old_dyn_addr
)
{
	return 0;
}

static void geni_i3c_master_detach_i3c_dev(struct i3c_dev_desc *dev)
{
	struct geni_i3c_i2c_dev_data *data = i3c_dev_get_master_data(dev);

	i3c_dev_set_master_data(dev, NULL);
	kfree(data);
}

static int geni_i3c_master_entdaa_locked(struct geni_i3c_dev *gi3c)
{
	struct i3c_master_controller *m = &gi3c->ctrlr;
	u8 addr;
	int ret;

	ret = i3c_master_entdaa_locked(m);
	if (ret && ret != I3C_ERROR_M2)
		return ret;

	for (addr = 0; addr <= I3C_ADDR_MASK; addr++) {
		if (is_new_addr_slot_set(gi3c->newaddrslots, addr)) {
			clear_new_addr_slot(gi3c->newaddrslots, addr);
			i3c_master_add_i3c_dev_locked(m, addr);
		}
	}

	return 0;
}

static int geni_i3c_master_do_daa(struct i3c_master_controller *m)
{
	struct geni_i3c_dev *gi3c = to_geni_i3c_master(m);

	return geni_i3c_master_entdaa_locked(gi3c);
}

static int geni_i3c_master_bus_init(struct i3c_master_controller *m)
{
	struct geni_i3c_dev *gi3c = to_geni_i3c_master(m);
	struct i3c_bus *bus = i3c_master_get_bus(m);
	struct i3c_device_info info = { };
	int ret;

	ret = pm_runtime_get_sync(gi3c->se.dev);
	if (ret < 0) {
		dev_err(gi3c->se.dev, "%s: error turning SE resources:%d\n",
			__func__, ret);
		pm_runtime_put_noidle(gi3c->se.dev);
		/* Set device in suspended since resume failed */
		pm_runtime_set_suspended(gi3c->se.dev);
		return ret;
	}

	ret = geni_i3c_clk_map_idx(gi3c);
	if (ret) {
		dev_err(gi3c->se.dev,
			"Invalid clk frequency %d Hz src or %ld Hz bus: %d\n",
			gi3c->clk_src_freq, bus->scl_rate.i3c,
			ret);
		goto err_cleanup;
	}

	qcom_geni_i3c_conf(gi3c);

	/* Get an address for the master. */
	ret = i3c_master_get_free_addr(m, 0);
	if (ret < 0)
		goto err_cleanup;

	info.dyn_addr = ret;
	info.dcr = I3C_DCR_GENERIC_DEVICE;
	info.bcr = I3C_BCR_I3C_MASTER | I3C_BCR_HDR_CAP;
	info.pid = 0;

	ret = i3c_master_set_info(&gi3c->ctrlr, &info);

err_cleanup:
	pm_runtime_mark_last_busy(gi3c->se.dev);
	pm_runtime_put_autosuspend(gi3c->se.dev);

	return ret;
}

static void geni_i3c_master_bus_cleanup(struct i3c_master_controller *m)
{
}

static bool geni_i3c_master_supports_ccc_cmd
(
	struct i3c_master_controller *m,
	const struct i3c_ccc_cmd *cmd
)
{
	switch (cmd->id) {
	case I3C_CCC_ENEC(true):
	/* fallthrough */
	case I3C_CCC_ENEC(false):
	/* fallthrough */
	case I3C_CCC_DISEC(true):
	/* fallthrough */
	case I3C_CCC_DISEC(false):
	/* fallthrough */
	case I3C_CCC_ENTAS(0, true):
	/* fallthrough */
	case I3C_CCC_ENTAS(0, false):
	/* fallthrough */
	case I3C_CCC_RSTDAA(true):
	/* fallthrough */
	case I3C_CCC_RSTDAA(false):
	/* fallthrough */
	case I3C_CCC_ENTDAA:
	/* fallthrough */
	case I3C_CCC_SETMWL(true):
	/* fallthrough */
	case I3C_CCC_SETMWL(false):
	/* fallthrough */
	case I3C_CCC_SETMRL(true):
	/* fallthrough */
	case I3C_CCC_SETMRL(false):
	/* fallthrough */
	case I3C_CCC_DEFSLVS:
	/* fallthrough */
	case I3C_CCC_ENTHDR(0):
	/* fallthrough */
	case I3C_CCC_SETDASA:
	/* fallthrough */
	case I3C_CCC_SETNEWDA:
	/* fallthrough */
	case I3C_CCC_GETMWL:
	/* fallthrough */
	case I3C_CCC_GETMRL:
	/* fallthrough */
	case I3C_CCC_GETPID:
	/* fallthrough */
	case I3C_CCC_GETBCR:
	/* fallthrough */
	case I3C_CCC_GETDCR:
	/* fallthrough */
	case I3C_CCC_GETSTATUS:
	/* fallthrough */
	case I3C_CCC_GETACCMST:
	/* fallthrough */
	case I3C_CCC_GETMXDS:
	/* fallthrough */
	case I3C_CCC_GETHDRCAP:
		return true;
	default:
		break;
	}

	return false;
}

static int geni_i3c_master_enable_ibi(struct i3c_dev_desc *dev)
{
	return -ENOTSUPP;
}

static int geni_i3c_master_disable_ibi(struct i3c_dev_desc *dev)
{
	return -ENOTSUPP;
}

static int geni_i3c_master_request_ibi(struct i3c_dev_desc *dev,
	const struct i3c_ibi_setup *req)
{
	return -ENOTSUPP;
}

static void geni_i3c_master_free_ibi(struct i3c_dev_desc *dev)
{
}

static void geni_i3c_master_recycle_ibi_slot
(
	struct i3c_dev_desc *dev,
	struct i3c_ibi_slot *slot
)
{
}

static const struct i3c_master_controller_ops geni_i3c_master_ops = {
	.bus_init = geni_i3c_master_bus_init,
	.bus_cleanup = geni_i3c_master_bus_cleanup,
	.do_daa = geni_i3c_master_do_daa,
	.attach_i3c_dev = geni_i3c_master_attach_i3c_dev,
	.reattach_i3c_dev = geni_i3c_master_reattach_i3c_dev,
	.detach_i3c_dev = geni_i3c_master_detach_i3c_dev,
	.attach_i2c_dev = geni_i3c_master_attach_i2c_dev,
	.detach_i2c_dev = geni_i3c_master_detach_i2c_dev,
	.supports_ccc_cmd = geni_i3c_master_supports_ccc_cmd,
	.send_ccc_cmd = geni_i3c_master_send_ccc_cmd,
	.priv_xfers = geni_i3c_master_priv_xfers,
	.i2c_xfers = geni_i3c_master_i2c_xfers,
	.i2c_funcs = geni_i3c_master_i2c_funcs,
	.enable_ibi = geni_i3c_master_enable_ibi,
	.disable_ibi = geni_i3c_master_disable_ibi,
	.request_ibi = geni_i3c_master_request_ibi,
	.free_ibi = geni_i3c_master_free_ibi,
	.recycle_ibi_slot = geni_i3c_master_recycle_ibi_slot,
};

static int i3c_geni_rsrcs_clk_init(struct geni_i3c_dev *gi3c)
{
	int ret;

	gi3c->se.i3c_rsc.se_clk = devm_clk_get(gi3c->se.dev, "se-clk");
	if (IS_ERR(gi3c->se.i3c_rsc.se_clk)) {
		ret = PTR_ERR(gi3c->se.i3c_rsc.se_clk);
		dev_err(gi3c->se.dev, "Error getting SE Core clk %d\n", ret);
		return ret;
	}

	gi3c->se.i3c_rsc.m_ahb_clk = devm_clk_get(gi3c->se.dev, "m-ahb");
	if (IS_ERR(gi3c->se.i3c_rsc.m_ahb_clk)) {
		ret = PTR_ERR(gi3c->se.i3c_rsc.m_ahb_clk);
		dev_err(gi3c->se.dev, "Error getting M AHB clk %d\n", ret);
		return ret;
	}

	gi3c->se.i3c_rsc.s_ahb_clk = devm_clk_get(gi3c->se.dev, "s-ahb");
	if (IS_ERR(gi3c->se.i3c_rsc.s_ahb_clk)) {
		ret = PTR_ERR(gi3c->se.i3c_rsc.s_ahb_clk);
		dev_err(gi3c->se.dev, "Error getting S AHB clk %d\n", ret);
		return ret;
	}

	return 0;
}

static int i3c_geni_rsrcs_init(struct geni_i3c_dev *gi3c,
			struct platform_device *pdev)
{
	struct resource *res;
	struct platform_device *wrapper_pdev;
	struct device_node *wrapper_ph_node;
	int ret;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -EINVAL;

	gi3c->se.base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(gi3c->se.base))
		return PTR_ERR(gi3c->se.base);

	wrapper_ph_node = of_parse_phandle(pdev->dev.of_node,
			"qcom,wrapper-core", 0);
	if (IS_ERR_OR_NULL(wrapper_ph_node)) {
		ret = PTR_ERR(wrapper_ph_node);
		dev_err(&pdev->dev, "No wrapper core defined\n");
		return ret;
	}

	wrapper_pdev = of_find_device_by_node(wrapper_ph_node);
	of_node_put(wrapper_ph_node);
	if (IS_ERR_OR_NULL(wrapper_pdev)) {
		ret = PTR_ERR(wrapper_pdev);
		dev_err(&pdev->dev, "Cannot retrieve wrapper device\n");
		return ret;
	}

	gi3c->se.i3c_rsc.wrapper_dev = &wrapper_pdev->dev;

	ret = geni_se_resources_init(&gi3c->se.i3c_rsc, I3C_CORE2X_VOTE,
				     (DEFAULT_SE_CLK * DEFAULT_BUS_WIDTH));
	if (ret) {
		dev_err(gi3c->se.dev, "geni_se_resources_init\n");
		return ret;
	}

	ret = device_property_read_u32(&pdev->dev, "se-clock-frequency",
		&gi3c->clk_src_freq);
	if (ret) {
		dev_info(&pdev->dev,
			"SE clk freq not specified, default to 100 MHz.\n");
		gi3c->clk_src_freq = 100000000;
	}

	ret = device_property_read_u32(&pdev->dev, "dfs-index",
		&gi3c->dfs_idx);
	if (ret)
		gi3c->dfs_idx = 0xf;

	gi3c->se.i3c_rsc.geni_pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(gi3c->se.i3c_rsc.geni_pinctrl)) {
		dev_err(&pdev->dev, "Error no pinctrl config specified\n");
		ret = PTR_ERR(gi3c->se.i3c_rsc.geni_pinctrl);
		return ret;
	}
	gi3c->se.i3c_rsc.geni_gpio_active =
		pinctrl_lookup_state(gi3c->se.i3c_rsc.geni_pinctrl, "default");
	if (IS_ERR(gi3c->se.i3c_rsc.geni_gpio_active)) {
		dev_err(&pdev->dev, "No default config specified\n");
		ret = PTR_ERR(gi3c->se.i3c_rsc.geni_gpio_active);
		return ret;
	}
	gi3c->se.i3c_rsc.geni_gpio_sleep =
		pinctrl_lookup_state(gi3c->se.i3c_rsc.geni_pinctrl, "sleep");
	if (IS_ERR(gi3c->se.i3c_rsc.geni_gpio_sleep)) {
		dev_err(&pdev->dev, "No sleep config specified\n");
		ret = PTR_ERR(gi3c->se.i3c_rsc.geni_gpio_sleep);
		return ret;
	}

	return 0;
}

static int geni_i3c_probe(struct platform_device *pdev)
{
	struct geni_i3c_dev *gi3c;
	u32 proto, tx_depth;
	int ret;
	u32 se_mode;

	gi3c = devm_kzalloc(&pdev->dev, sizeof(*gi3c), GFP_KERNEL);
	if (!gi3c)
		return -ENOMEM;

	gi3c->se.dev = &pdev->dev;

	ret = i3c_geni_rsrcs_init(gi3c, pdev);
	if (ret)
		return ret;

	ret = i3c_geni_rsrcs_clk_init(gi3c);
	if (ret)
		return ret;

	gi3c->irq = platform_get_irq(pdev, 0);
	if (gi3c->irq < 0) {
		dev_err(&pdev->dev, "IRQ error for i3c-master-geni\n");
		return gi3c->irq;
	}

	ret = geni_i3c_clk_map_idx(gi3c);
	if (ret) {
		dev_err(&pdev->dev, "Invalid source clk frequency %d Hz: %d\n",
			gi3c->clk_src_freq, ret);
		return ret;
	}

	init_completion(&gi3c->done);
	mutex_init(&gi3c->lock);
	spin_lock_init(&gi3c->spinlock);
	platform_set_drvdata(pdev, gi3c);
	ret = devm_request_irq(&pdev->dev, gi3c->irq, geni_i3c_irq,
		IRQF_TRIGGER_HIGH, dev_name(&pdev->dev), gi3c);
	if (ret) {
		dev_err(&pdev->dev, "Request_irq failed:%d: err:%d\n",
			gi3c->irq, ret);
		return ret;
	}
	/* Disable the interrupt so that the system can enter low-power mode */
	disable_irq(gi3c->irq);

	ret = se_geni_resources_on(&gi3c->se.i3c_rsc);
	if (ret) {
		dev_err(&pdev->dev, "Error turning on resources %d\n", ret);
		return ret;
	}

	if (!gi3c->ipcl) {
		char ipc_name[I2C_NAME_SIZE];

		snprintf(ipc_name, I2C_NAME_SIZE, "i3c-%d", gi3c->ctrlr.bus.id);
		gi3c->ipcl = ipc_log_context_create(2, ipc_name, 0);
	}

	proto = get_se_proto(gi3c->se.base);
	if (proto != I3C) {
		dev_err(&pdev->dev, "Invalid proto %d\n", proto);
		se_geni_resources_off(&gi3c->se.i3c_rsc);
		return -ENXIO;
	}

	se_mode = readl_relaxed(gi3c->se.base + GENI_IF_FIFO_DISABLE_RO);
	if (se_mode) {
		dev_err(&pdev->dev, "Non supported mode %d\n", se_mode);
		se_geni_resources_off(&gi3c->se.i3c_rsc);
		return -ENXIO;
	}

	tx_depth = get_tx_fifo_depth(gi3c->se.base);
	gi3c->tx_wm = tx_depth - 1;
	geni_se_init(gi3c->se.base, gi3c->tx_wm, tx_depth);
	se_config_packing(gi3c->se.base, BITS_PER_BYTE, PACKING_BYTES_PW, true);
	se_geni_resources_off(&gi3c->se.i3c_rsc);
	GENI_SE_DBG(gi3c->ipcl, false, gi3c->se.dev,
		"i3c fifo/se-dma mode. fifo depth:%d\n", tx_depth);

	pm_runtime_set_suspended(gi3c->se.dev);
	pm_runtime_set_autosuspend_delay(gi3c->se.dev, I3C_AUTO_SUSPEND_DELAY);
	pm_runtime_use_autosuspend(gi3c->se.dev);
	pm_runtime_enable(gi3c->se.dev);

	ret = i3c_master_register(&gi3c->ctrlr, &pdev->dev,
		&geni_i3c_master_ops, false);

	if (ret)
		return ret;

	GENI_SE_DBG(gi3c->ipcl, false, gi3c->se.dev, "I3C probed\n");
	return ret;
}

static int geni_i3c_remove(struct platform_device *pdev)
{
	struct geni_i3c_dev *gi3c = platform_get_drvdata(pdev);
	int ret = 0;

	pm_runtime_disable(gi3c->se.dev);
	ret = i3c_master_unregister(&gi3c->ctrlr);
	if (gi3c->ipcl)
		ipc_log_context_destroy(gi3c->ipcl);
	return ret;
}

#ifdef CONFIG_PM
static int geni_i3c_runtime_suspend(struct device *dev)
{
	struct geni_i3c_dev *gi3c = dev_get_drvdata(dev);

	disable_irq(gi3c->irq);
	se_geni_resources_off(&gi3c->se.i3c_rsc);
	return 0;
}

static int geni_i3c_runtime_resume(struct device *dev)
{
	int ret;
	struct geni_i3c_dev *gi3c = dev_get_drvdata(dev);

	ret = se_geni_resources_on(&gi3c->se.i3c_rsc);
	if (ret)
		return ret;

	enable_irq(gi3c->irq);

	return 0;
}
#else
static int geni_i3c_runtime_suspend(struct device *dev)
{
	return 0;
}

static int geni_i3c_runtime_resume(struct device *dev)
{
	return 0;
}
#endif

static const struct dev_pm_ops geni_i3c_pm_ops = {
	.runtime_suspend = geni_i3c_runtime_suspend,
	.runtime_resume  = geni_i3c_runtime_resume,
};

static const struct of_device_id geni_i3c_dt_match[] = {
	{ .compatible = "qcom,geni-i3c" },
	{ }
};
MODULE_DEVICE_TABLE(of, geni_i3c_dt_match);

static struct platform_driver geni_i3c_master = {
	.probe  = geni_i3c_probe,
	.remove = geni_i3c_remove,
	.driver = {
		.name = "geni_i3c_master",
		.pm = &geni_i3c_pm_ops,
		.of_match_table = geni_i3c_dt_match,
	},
};

module_platform_driver(geni_i3c_master);

MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:geni_i3c_master");
