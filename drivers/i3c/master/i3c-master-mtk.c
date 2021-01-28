// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/i3c/master.h>
#include <linux/interrupt.h>
#include <linux/iopoll.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/overflow.h>

#define DRV_NAME		"i3c-master-mtk"

#define SLAVE_ADDR		0x04
#define INTR_MASK		0x08
#define INTR_STAT		0x0c
#define INTR_TRANSAC_COMP	BIT(0)
#define INTR_ACKERR		GENMASK(2, 1)
#define INTR_ARB_LOST		BIT(3)
#define INTR_RS_MULTI		BIT(4)
#define INTR_IBI		BIT(7)
#define INTR_MAS_ERR		BIT(8)
#define INTR_ALL		(INTR_MAS_ERR | INTR_ARB_LOST |\
				INTR_ACKERR | INTR_TRANSAC_COMP)

#define DATA_PORT		0x0
#define CONTROL			0x10
#define CONTROL_WRAPPER		BIT(0)
#define CONTROL_RS		BIT(1)
#define CONTROL_DMA_EN		BIT(2)
#define CONTROL_CLK_EXT_EN	BIT(3)
#define CONTROL_DIR_CHANGE	BIT(4)
#define CONTROL_ACKERR_DET_EN	BIT(5)
#define CONTROL_LEN_CHANGE	BIT(6)
#define CONTROL_DMAACK_EN	BIT(8)
#define CONTROL_ASYNC_MODE	BIT(9)

#define TRANSFER_LEN		0x14
#define TRANSAC_LEN		0x18
#define TRANSAC_LEN_WRRD	0x0002
#define TRANS_ONE_LEN		0x0001

#define DELAY_LEN		0x1c
#define DELAY_LEN_DEFAULT	0x000a

#define TIMING			0x20
#define TIMING_VALUE(sample_cnt, step_cnt) ({ \
	typeof(sample_cnt) sample_cnt_ = (sample_cnt); \
	typeof(step_cnt) step_cnt_ = (step_cnt); \
	(((sample_cnt_) << 8) | (step_cnt_)); \
})

#define START			0x24
#define START_EN		BIT(0)
#define START_MUL_TRIG		BIT(14)
#define START_MUL_CNFG		BIT(15)

#define EXT_CONF		0x28
#define EXT_CONF_DEFAULT	0x0a1f

#define LTIMING			0x2c
#define LTIMING_VALUE(sample_cnt, step_cnt) ({ \
	typeof(sample_cnt) sample_cnt_ = (sample_cnt); \
	typeof(step_cnt) step_cnt_ = (step_cnt); \
	(((sample_cnt_) << 6) | (step_cnt_)); \
})

#define HS_LTIMING_VALUE(i3c_sample_cnt, i3c_step_cnt) ({ \
	typeof(i3c_sample_cnt) i3c_sample_cnt_ = (i3c_sample_cnt); \
	typeof(i3c_step_cnt) i3c_step_cnt_ = (i3c_step_cnt); \
	((i3c_sample_cnt_ << 12) | (i3c_step_cnt_ << 9)); \
})

#define HS			0x30
#define HS_CLR_VALUE		0x0000
#define HS_DEFAULT_VALUE	0x0083
#define HS_VALUE(sample_cnt, step_cnt) ({ \
	typeof(sample_cnt) sample_cnt_ = (sample_cnt); \
	typeof(step_cnt) step_cnt_ = (step_cnt); \
	(HS_DEFAULT_VALUE | \
	((sample_cnt_) << 12) | ((step_cnt_) << 8)); \
})

#define IO_CONFIG		0x34
#define IO_CONFIG_PUSH_PULL	0x0000

#define FIFO_ADDR_CLR		0x38
#define FIFO_CLR		0x0003

#define MCU_INTR		0x40
#define MCU_INTR_EN		BIT(0)

#define TRANSFER_LEN_AUX	0x44
#define CLOCK_DIV		0x48
#define CLOCK_DIV_DEFAULT	((INTER_CLK_DIV - 1) << 8 |\
				(INTER_CLK_DIV - 1))
#define CLOCK_DIV_HS	        ((HS_CLK_DIV - 1) << 8 | (INTER_CLK_DIV - 1))

#define SOFTRESET		0x50
#define SOFT_RST		BIT(0)
#define I3C_ERR_RST		BIT(3)

#define TRAFFIC			0x54
#define TRAFFIC_DAA_EN		BIT(4)
#define TRAFFIC_TBIT		BIT(7)
#define TRAFFIC_HEAD_ONLY	BIT(9)
#define TRAFFIC_SKIP_SLV_ADDR	BIT(10)
#define TRAFFIC_IBI_EN		BIT(13)
#define TRAFFIC_HANDOFF		BIT(14)

#define DEF_DA			0x68
#define DEF_DAA_SLV_PARITY	BIT(8)
#define USE_DEF_DA		BIT(7)

#define SHAPE			0x6c
#define SHAPE_T_STALL		BIT(1)
#define SHAPE_T_PARITY		BIT(2)

#define HFIFO_DATA		0x70
#define NINTH_BIT_IGNORE	0
#define NINTH_BIT_ACK		1
#define NINTH_BIT_NACK		2
#define NINTH_BIT_ODD_PAR	3
#define INST_WITH_HS		BIT(10)
#define UNLOCK_HFIFO		BIT(15)
#define HFIFO_DATA_08		0x8208
#define HFIFO_DATA_7E		(UNLOCK_HFIFO |\
				(I3C_BROADCAST_ADDR << 1) |\
				(NINTH_BIT_ACK << 8))
#define HFIFO_HEAD		(UNLOCK_HFIFO | INST_WITH_HS |\
				(NINTH_BIT_ODD_PAR << 8))

#define FIFO_STAT		0xf4

#define DMA_INT_FLAG		0x0
#define DMA_EN			0x08
#define DMA_RST			0x0c
#define DMA_CON			0x18
#define DMA_TX_MEM_ADDR		0x1c
#define DMA_RX_MEM_ADDR		0x20
#define DMA_TX_LEN		0x24
#define DMA_RX_LEN		0x28
#define DMA_TX_4G_MODE		0x54
#define DMA_RX_4G_MODE		0x58
#define CHN_ERROR		0xd0
#define DEBUGSTAT		0xe4
#define DEBUGCTRL		0xe8
#define IBI_SWITCH		BIT(2)

#define DMA_EN_START		BIT(0)
#define DMA_RST_HARD		BIT(1)
#define DMA_4G_MODE		BIT(0)
#define DMA_CLR_FLAG		0x0000
#define DMA_CON_TX		0x0000
#define DMA_CON_RX		0x0001

#define HS_CLK_DIV              1
#define INTER_CLK_DIV		5
#define MAX_SAMPLE_CNT		8
#define MAX_STEP_CNT		64
#define MAX_HS_STEP_CNT		8
#define MAX_I3C_DEVS		3

#define MTK_I3C_BCR		0x01
#define MTK_I3C_DCR		0x02
#define MTK_I3C_PID		0x03040506

#define MIN(x, y) ((x) > (y) ? (y) : (x))

enum mtk_trans_op {
	MASTER_WR = 1,
	MASTER_RD,
	MASTER_WRRD,
	/* I3C private op */
	MASTER_CCC_BROADCAST,
	MASTER_DAA,
};

enum mtk_trans_mode {
	I2C_TRANSFER = 1,
	I3C_TRANSFER,
	I3C_CCC,
};

struct daa_addr_ary {
	u8 addr;
	bool used;
};

struct daa_anchor {
	struct daa_addr_ary daa_addr[MAX_I3C_DEVS];
	int idx;
};

struct mtk_i3c_cmd {
	enum mtk_trans_op op;
	u8 ccc_id;
	u16 addr;				/* device addr */

	u16 tx_len;
	const void *tx_buf;
	u16 rx_len;
	void *rx_buf;
};

struct mtk_i3c_xfer {
	struct list_head node;
	struct completion complete;		/* i3c transfer stop */

	int ret;
	u16 irq_stat;				/* interrupt status */
	enum mtk_trans_mode mode;
	bool ignore_restart_irq;
	bool auto_restart;
	unsigned int ncmds;
	unsigned int trans_num;
	u8 *msg_buf_w;
	u8 *msg_buf_r;
	dma_addr_t rpaddr;
	dma_addr_t wpaddr;
	struct mtk_i3c_cmd cmds[0];
};

struct mtk_i3c_i2c_dev_data {
	u16 id;
	s16 ibi;
	struct i3c_generic_ibi_pool *ibi_pool;
};

struct mtk_i3c_master {
	struct device *dev;
	struct i3c_master_controller mas_ctrler;
	struct {
		unsigned int num_slots;
		struct i3c_dev_desc **slots;
		spinlock_t lock;
		int ibi_en_count;
	} ibi;
	/* set in i3c probe */
	void __iomem *regs;			/* i3c base addr */
	void __iomem *dma_regs;			/* apdma base address*/
	struct clk *clk_main;			/* main clock for i3c bus */
	struct clk *clk_dma;			/* DMA clock for i3c via DMA */
	struct clk *clk_arb;			/* Arbitrator clock for i3c */
	struct daa_anchor daa_anchor;
	u16 timing_reg[2];
	u16 ltiming_reg[2];
	u16 high_speed_reg[2];
	struct {
		struct list_head list;
		struct mtk_i3c_xfer *cur;
		spinlock_t lock;		/* Lock for stats update */
	} xferqueue;
	u16 irqnr;
	u16 ibi_addr;
	u16 ibi_intr;
	u16 daa_count;
};

static inline struct mtk_i3c_master *
to_mt_i3c_master(struct i3c_master_controller *master)
{
	return container_of(master, struct mtk_i3c_master, mas_ctrler);
}

static u16 mtk_i3c_readw(struct mtk_i3c_master *master, u16 offset)
{
	return readw(master->regs + offset);
}
static bool i3c_debug;
static void mtk_i3c_writew(struct mtk_i3c_master *master, u16 val, u16 offset)
{
	if (i3c_debug)
		pr_info("wrg: %x val:0x%x\n", offset, val);
	writew(val, master->regs + offset);
}

static void i3c_reg_dump(struct mtk_i3c_master *master)
{
	pr_info("SLAVES val:0x%x\n", mtk_i3c_readw(master, SLAVE_ADDR));
	pr_info("INTR_STAT:0x%x\n", mtk_i3c_readw(master, INTR_STAT));
	pr_info("INTR_MASK val:0x%x\n", mtk_i3c_readw(master, INTR_MASK));
	pr_info("CONTROL val:0x%x\n", mtk_i3c_readw(master, CONTROL));
	pr_info("START val:0x%x\n", mtk_i3c_readw(master, START));
	pr_info("TRAFFIC val:0x%x\n", mtk_i3c_readw(master, TRAFFIC));
	pr_info("SHAPE val:0x%x\n", mtk_i3c_readw(master, SHAPE));
	pr_info("TIMING val:0x%x\n", mtk_i3c_readw(master, TIMING));
	pr_info("LTIMING val:0x%x\n", mtk_i3c_readw(master, LTIMING));
	pr_info("HS val:0x%x\n", mtk_i3c_readw(master, HS));
	pr_info("ERROR val:0x%x\n", mtk_i3c_readw(master, CHN_ERROR));
	pr_info("DEF DA:0x%x\n", mtk_i3c_readw(master, DEF_DA));
	pr_info("TRANSFER_LEN val:0x%x\n", mtk_i3c_readw(master, TRANSFER_LEN));
	pr_info("TRANSAC_LEN val:0x%x\n", mtk_i3c_readw(master, TRANSAC_LEN));
	pr_info("FIFO_STAT val:0x%x\n", mtk_i3c_readw(master, FIFO_STAT));
	pr_info("DEBUGSTAT val:0x%x\n", mtk_i3c_readw(master, DEBUGSTAT));
}


/**
 * Calculate i3c port speed
 *
 * Hardware design:
 * i3c_bus_freq = parent_clk / (clock_div * 2 * sample_cnt * step_cnt)
 * clock_div: fixed in hardware, but may be various in different SoCs
 *
 * The calculation want to pick the highest bus frequency that is still
 * less than or equal to master->speed_hz. The calculation try to get
 * sample_cnt and step_cn
 */
static int mtk_i3c_calculate_speed(struct mtk_i3c_master *master,
				   unsigned int clk_src,
				   unsigned int target_speed,
				   unsigned int *timing_step_cnt,
				   unsigned int *timing_sample_cnt)
{
	unsigned int sample_cnt, step_cnt, max_step_cnt, opt_div;
	unsigned int best_mul, cnt_mul, base_step_cnt;
	unsigned int base_sample_cnt = MAX_SAMPLE_CNT;

	pr_info("i3c target speed:%d,clk_src:%d\n", target_speed, clk_src);
	if (target_speed > I3C_BUS_I2C_FM_PLUS_SCL_RATE)
		max_step_cnt = MAX_HS_STEP_CNT;
	else
		max_step_cnt = MAX_STEP_CNT;

	base_step_cnt = max_step_cnt;
	/* Find the best combination */
	opt_div = DIV_ROUND_UP(clk_src >> 1, target_speed);
	pr_info("i3c opt_div:%d\n", opt_div);
	best_mul = MAX_SAMPLE_CNT * max_step_cnt;

	/* Search for the best pair (sample_cnt, step_cnt) with
	 * 1 < sample_cnt < MAX_SAMPLE_CNT
	 * 1 < step_cnt < max_step_cnt
	 * sample_cnt * step_cnt >= opt_div
	 * optimizing for sample_cnt * step_cnt being minimal
	 */
	for (sample_cnt = 1; sample_cnt <= MAX_SAMPLE_CNT; sample_cnt++) {
		step_cnt = DIV_ROUND_UP(opt_div, sample_cnt);
		cnt_mul = step_cnt * sample_cnt;
		if (step_cnt > max_step_cnt/* || step_cnt < 2*/)
			continue;

		if (cnt_mul < best_mul) {
			best_mul = cnt_mul;
			base_sample_cnt = sample_cnt;
			base_step_cnt = step_cnt;
			if (best_mul == opt_div)
				break;
		}
	}

	sample_cnt = base_sample_cnt;
	step_cnt = base_step_cnt;

	if ((clk_src / (2 * sample_cnt * step_cnt)) > target_speed) {
		dev_err(master->dev, "Unsupport speed (%uhz)\n", target_speed);
		return -EINVAL;
	}

	*timing_step_cnt = step_cnt - 1;
	*timing_sample_cnt = sample_cnt - 1;
	pr_info("i3c step:%d, sample:%d\n", step_cnt, sample_cnt);
	return 0;
}

static int mtk_i3c_set_speed(struct mtk_i3c_master *master,
			     unsigned int clk_src, unsigned int bc_clk_rate)
{
	struct i3c_bus *bus = i3c_master_get_bus(&master->mas_ctrler);
	unsigned int i2c_step_cnt, i2c_sample_cnt, i3c_step_cnt, i3c_sample_cnt;
	int ret;

	ret = mtk_i3c_calculate_speed(master, bc_clk_rate, bus->scl_rate.i2c,
				      &i2c_step_cnt, &i2c_sample_cnt);
	if (ret < 0)
		return ret;

	master->timing_reg[0] = TIMING_VALUE(i2c_sample_cnt, i2c_step_cnt);
	/* Disable the high speed transaction */
	master->high_speed_reg[0] = HS_CLR_VALUE;
	master->ltiming_reg[0] = LTIMING_VALUE(i2c_sample_cnt, i2c_step_cnt);

	ret = mtk_i3c_calculate_speed(master, clk_src, bus->scl_rate.i3c,
				      &i3c_step_cnt, &i3c_sample_cnt);
	if (ret < 0)
		return ret;

	master->high_speed_reg[1] = HS_VALUE(i3c_sample_cnt, i3c_step_cnt);
	master->ltiming_reg[1] = HS_LTIMING_VALUE(i3c_sample_cnt, i3c_step_cnt);

	return 0;
}

static inline u32 mtk_i3c_set_4g_mode(dma_addr_t addr)
{
	return (addr & BIT_ULL(32)) ? DMA_4G_MODE : DMA_CLR_FLAG;
}

static inline struct mtk_i3c_master *
to_mtk_i3c_master(struct i3c_master_controller *master)
{
	return container_of(master, struct mtk_i3c_master, mas_ctrler);
}

static u8 mtk_i3c_master_get_free_addr(struct mtk_i3c_master *master)
{
	int slot = 0;

	for (slot = 0; slot < MAX_I3C_DEVS; slot++) {
		if (master->daa_anchor.daa_addr[slot].used)
			continue;

		master->daa_anchor.idx = slot;
		master->daa_anchor.daa_addr[slot].used = true;
		return master->daa_anchor.daa_addr[slot].addr;
	}

	return 0;
}

static int mtk_i3c_master_clock_enable(struct mtk_i3c_master *master)
{
	int ret;

	ret = clk_prepare_enable(master->clk_dma);
	if (ret)
		return ret;

	ret = clk_prepare_enable(master->clk_main);
	if (ret)
		goto err_main;

	if (!IS_ERR(master->clk_arb)) {
		ret = clk_prepare_enable(master->clk_arb);
		if (ret)
			goto err_arb;
	}

	return 0;

err_arb:
	clk_disable_unprepare(master->clk_main);
err_main:
	clk_disable_unprepare(master->clk_dma);

	return ret;
}

static void mtk_i3c_master_clock_disable(struct mtk_i3c_master *master)
{
	if (!IS_ERR(master->clk_arb))
		clk_disable_unprepare(master->clk_arb);
	clk_disable_unprepare(master->clk_main);
	clk_disable_unprepare(master->clk_dma);
}

static inline void mtk_i3c_master_init_hw(struct mtk_i3c_master *master)
{
	struct mtk_i3c_xfer *mtk_xfer = master->xferqueue.cur;

	if (!mtk_xfer->trans_num)
		mtk_i3c_writew(master, SOFT_RST, SOFTRESET);

	mtk_i3c_writew(master, IO_CONFIG_PUSH_PULL, IO_CONFIG);
	mtk_i3c_writew(master, CLOCK_DIV_DEFAULT, CLOCK_DIV);

	mtk_i3c_writew(master, DELAY_LEN_DEFAULT, DELAY_LEN);
	mtk_i3c_writew(master, EXT_CONF_DEFAULT, EXT_CONF);
	mtk_i3c_writew(master, FIFO_CLR, FIFO_ADDR_CLR);

	/* DMA hard reset */
	writel(DMA_RST_HARD, master->dma_regs + DMA_RST);
	writel(DMA_CLR_FLAG, master->dma_regs + DMA_RST);
}

static int mtk_i3c_master_apdma_tx(struct mtk_i3c_master *master)
{
	struct mtk_i3c_xfer *xfer = master->xferqueue.cur;
	struct mtk_i3c_cmd *cmd = &xfer->cmds[xfer->trans_num];
	u32 reg_4g_mode;

	xfer->msg_buf_w = kzalloc(cmd->tx_len, GFP_ATOMIC);
	if (!xfer->msg_buf_w)
		return -ENOMEM;

	memcpy(xfer->msg_buf_w, cmd->tx_buf, cmd->tx_len);
	writel(DMA_CLR_FLAG, master->dma_regs + DMA_INT_FLAG);
	writel(DMA_CON_TX, master->dma_regs + DMA_CON);

	xfer->wpaddr = dma_map_single(master->dev, xfer->msg_buf_w,
				      cmd->tx_len, DMA_TO_DEVICE);
	if (dma_mapping_error(master->dev, xfer->wpaddr)) {
		kfree(xfer->msg_buf_w);
		return -ENOMEM;
	}

	reg_4g_mode = mtk_i3c_set_4g_mode(xfer->wpaddr);
	writel(reg_4g_mode, master->dma_regs + DMA_TX_4G_MODE);
	writel((u32)xfer->wpaddr, master->dma_regs + DMA_TX_MEM_ADDR);
	writel(cmd->tx_len, master->dma_regs + DMA_TX_LEN);

	return 0;
}

static int mtk_i3c_master_apdma_rx(struct mtk_i3c_master *master)
{
	struct mtk_i3c_xfer *xfer = master->xferqueue.cur;
	struct mtk_i3c_cmd *cmd = &xfer->cmds[xfer->trans_num];
	u32 reg_4g_mode;

	xfer->msg_buf_r = kzalloc(cmd->rx_len, GFP_ATOMIC);
	if (!xfer->msg_buf_r)
		return -ENOMEM;

	writel(DMA_CLR_FLAG, master->dma_regs + DMA_INT_FLAG);
	writel(DMA_CON_RX, master->dma_regs + DMA_CON);

	xfer->rpaddr = dma_map_single(master->dev, xfer->msg_buf_r,
				      cmd->rx_len, DMA_FROM_DEVICE);
	if (dma_mapping_error(master->dev, xfer->rpaddr)) {
		kfree(xfer->msg_buf_r);
		return -ENOMEM;
	}

	reg_4g_mode = mtk_i3c_set_4g_mode(xfer->rpaddr);
	writel(reg_4g_mode, master->dma_regs + DMA_RX_4G_MODE);
	writel((u32)xfer->rpaddr, master->dma_regs + DMA_RX_MEM_ADDR);
	writel(cmd->rx_len, master->dma_regs + DMA_RX_LEN);

	return 0;
}

static void mtk_i3c_master_apdma_end(struct mtk_i3c_master *master)
{
	struct mtk_i3c_xfer *xfer = master->xferqueue.cur;
	struct mtk_i3c_cmd *cmd;
	u16 int_reg = 0;

	if (!xfer) {
		pr_info("i3c debug xfer NULL FLOW\n");
		return;
	}

	cmd = &xfer->cmds[xfer->trans_num];

	if (master->ibi.ibi_en_count > 0)
		int_reg |= INTR_IBI;

	mtk_i3c_writew(master, int_reg, INTR_MASK);

	if (cmd->op == MASTER_WR) {
		dma_unmap_single(master->dev, xfer->wpaddr,
				 cmd->tx_len, DMA_TO_DEVICE);
		kfree(xfer->msg_buf_w);

	} else if (cmd->op == MASTER_RD && cmd->ccc_id != I3C_CCC_GETMXDS) {
		dma_unmap_single(master->dev, xfer->rpaddr,
				 cmd->rx_len, DMA_FROM_DEVICE);
		memcpy(cmd->rx_buf, xfer->msg_buf_r, cmd->rx_len);
		kfree(xfer->msg_buf_r);
	} else if (cmd->op == MASTER_WRRD) {
		dma_unmap_single(master->dev, xfer->wpaddr, cmd->tx_len,
				 DMA_TO_DEVICE);
		dma_unmap_single(master->dev, xfer->rpaddr, cmd->rx_len,
				 DMA_FROM_DEVICE);
		memcpy(cmd->rx_buf, xfer->msg_buf_r, cmd->rx_len);
		kfree(xfer->msg_buf_w);
		kfree(xfer->msg_buf_r);
	}
}

static int mtk_i3c_master_start_transfer(struct mtk_i3c_master *master)
{
	struct mtk_i3c_xfer *xfer = master->xferqueue.cur;
	struct mtk_i3c_cmd *cmd = &xfer->cmds[xfer->trans_num];
	u16 addr_reg, control_reg, start_reg, tmp_rg_val;
	u16 int_reg = 0, data_size;
	int ret = 0;
	u8 *ptr;

	xfer->irq_stat = 0;

	mtk_i3c_master_init_hw(master);

	control_reg = CONTROL_ACKERR_DET_EN | CONTROL_CLK_EXT_EN | CONTROL_RS;
	if ((cmd->op != MASTER_DAA) && (cmd->ccc_id != I3C_CCC_GETMXDS))
		control_reg |= CONTROL_DMA_EN | CONTROL_DMAACK_EN |
			       CONTROL_ASYNC_MODE;

	if (cmd->op == MASTER_WRRD)
		control_reg |= CONTROL_DIR_CHANGE;

	mtk_i3c_writew(master, control_reg, CONTROL);

	addr_reg = cmd->addr << 1;
	if (cmd->op == MASTER_RD || cmd->op == MASTER_DAA)
		addr_reg |= 0x1;
	mtk_i3c_writew(master, addr_reg, SLAVE_ADDR);

	if (xfer->auto_restart)
		int_reg = INTR_RS_MULTI;

	int_reg |= INTR_ALL;

	if (master->ibi.ibi_en_count > 0)
		int_reg |= INTR_IBI;

	/* Clear interrupt status */
	mtk_i3c_writew(master, int_reg, INTR_STAT);
	/* Enable interrupt */
	mtk_i3c_writew(master, int_reg, INTR_MASK);

	/* Set transfer and transaction len */
	switch (cmd->op) {
	case MASTER_WRRD:
		mtk_i3c_writew(master, cmd->tx_len, TRANSFER_LEN);
		mtk_i3c_writew(master, cmd->rx_len, TRANSFER_LEN_AUX);
		mtk_i3c_writew(master, TRANSAC_LEN_WRRD, TRANSAC_LEN);
		break;
	case MASTER_WR:
		mtk_i3c_writew(master, cmd->tx_len, TRANSFER_LEN);
		mtk_i3c_writew(master, xfer->ncmds, TRANSAC_LEN);
		break;
	case MASTER_RD:
		mtk_i3c_writew(master, cmd->rx_len, TRANSFER_LEN);
		mtk_i3c_writew(master, xfer->ncmds, TRANSAC_LEN);
		break;
	case MASTER_CCC_BROADCAST:
		mtk_i3c_writew(master, TRANS_ONE_LEN, TRANSFER_LEN);
		mtk_i3c_writew(master, TRANS_ONE_LEN, TRANSAC_LEN);
		break;
	case MASTER_DAA:
		mtk_i3c_writew(master, cmd->rx_len + cmd->tx_len, TRANSFER_LEN);
		mtk_i3c_writew(master, MAX_I3C_DEVS, TRANSAC_LEN);
		break;
	}
	mtk_i3c_writew(master, master->timing_reg[0], TIMING);
	mtk_i3c_writew(master, master->ltiming_reg[0], LTIMING);
	if (cmd->op == MASTER_DAA)
		mtk_i3c_writew(master, master->high_speed_reg[1] & 0xff7f, HS);
	else
		mtk_i3c_writew(master, master->high_speed_reg[1], HS);
	mtk_i3c_writew(master, master->ltiming_reg[0] | master->ltiming_reg[1],
					LTIMING);

	mtk_i3c_writew(master, CLOCK_DIV_HS, CLOCK_DIV);

	if (xfer->mode == I2C_TRANSFER) {
		mtk_i3c_writew(master, master->timing_reg[0], TIMING);
		mtk_i3c_writew(master, master->ltiming_reg[0], LTIMING);
		mtk_i3c_writew(master, master->high_speed_reg[0], HS);
		mtk_i3c_writew(master, 0, TRAFFIC);
		mtk_i3c_writew(master, 0, SHAPE);
	} else if ((xfer->mode == I3C_TRANSFER) && (!xfer->trans_num)) {
		mtk_i3c_writew(master, HFIFO_DATA_7E, HFIFO_DATA);
		mtk_i3c_writew(master, TRAFFIC_HANDOFF | TRAFFIC_TBIT, TRAFFIC);
		mtk_i3c_writew(master, SHAPE_T_STALL | SHAPE_T_PARITY, SHAPE);
	} else if (xfer->mode == I3C_CCC) {
		if (cmd->op == MASTER_DAA)
			tmp_rg_val = TRAFFIC_HANDOFF;
		else
			tmp_rg_val = TRAFFIC_HANDOFF | TRAFFIC_TBIT;

		mtk_i3c_writew(master, HFIFO_DATA_7E, HFIFO_DATA);
		mtk_i3c_writew(master, HFIFO_HEAD | cmd->ccc_id, HFIFO_DATA);

		if (cmd->op == MASTER_DAA) {
			tmp_rg_val |= TRAFFIC_DAA_EN;
		} else if (cmd->op == MASTER_CCC_BROADCAST) {
			tmp_rg_val |= TRAFFIC_HEAD_ONLY | TRAFFIC_SKIP_SLV_ADDR;
			data_size = cmd->tx_len;
			ptr = (u8 *)cmd->tx_buf;
			while (data_size--)
				mtk_i3c_writew(master, HFIFO_HEAD | *ptr++,
					       HFIFO_DATA);
		}

		mtk_i3c_writew(master, tmp_rg_val, TRAFFIC);
		mtk_i3c_writew(master, SHAPE_T_PARITY | SHAPE_T_STALL, SHAPE);
		if (cmd->op == MASTER_DAA)
			mtk_i3c_writew(master, SHAPE_T_STALL, SHAPE);
	}

	if (master->ibi.ibi_en_count > 0) {
		tmp_rg_val = mtk_i3c_readw(master, TRAFFIC);
		tmp_rg_val |= TRAFFIC_IBI_EN;
		mtk_i3c_writew(master, tmp_rg_val, TRAFFIC);
	}

	/* Prepare buffer data to start transfer */
	if (cmd->op == MASTER_RD && cmd->ccc_id != I3C_CCC_GETMXDS) {
		ret = mtk_i3c_master_apdma_rx(master);
	} else if (cmd->op == MASTER_WR) {
		ret = mtk_i3c_master_apdma_tx(master);
	} else if (cmd->op == MASTER_WRRD) {
		ret = mtk_i3c_master_apdma_rx(master);
		ret |= mtk_i3c_master_apdma_tx(master);
	}

	if (ret)
		return ret;

	if (cmd->op != MASTER_DAA && cmd->ccc_id != I3C_CCC_GETMXDS)
		writel(DMA_EN_START, master->dma_regs + DMA_EN);

	start_reg = START_EN;
	if (xfer->auto_restart) {
		start_reg |= START_MUL_TRIG;
		if ((xfer->ncmds - xfer->trans_num) >= 2 ||
		    cmd->op == MASTER_DAA)
			start_reg |= START_MUL_CNFG;
	}

	mtk_i3c_writew(master, MCU_INTR_EN, MCU_INTR);
	mtk_i3c_writew(master, start_reg, START);

	return 0;
}

static struct mtk_i3c_xfer *
mtk_i3c_master_alloc_xfer(struct mtk_i3c_master *master, unsigned int ncmds)
{
	struct mtk_i3c_xfer *xfer;

	xfer = kzalloc(struct_size(xfer, cmds, ncmds), GFP_KERNEL);
	if (!xfer)
		return NULL;

	INIT_LIST_HEAD(&xfer->node);
	xfer->ncmds = ncmds;
	xfer->ret = -ETIMEDOUT;

	return xfer;
}

static void mtk_i3c_master_free_xfer(struct mtk_i3c_xfer *xfer)
{
	kfree(xfer);
}

static void mtk_i3c_master_start_xfer_locked(struct mtk_i3c_master *master)
{
	struct mtk_i3c_xfer *xfer = master->xferqueue.cur;
	unsigned int ret = 0;

	if (!xfer)
		return;

	xfer->trans_num = 0;
	ret = mtk_i3c_master_start_transfer(master);
	xfer->ret = ret;
}

static void mtk_i3c_master_end_xfer_locked(struct mtk_i3c_master *master)
{
	struct mtk_i3c_xfer *xfer = master->xferqueue.cur;
	struct mtk_i3c_cmd *cmd = &xfer->cmds[xfer->trans_num];
	int slot = 0;
	u16 fifo_offset = 0;
	u8 *data;

	if (!xfer)
		return;

	if (xfer->irq_stat & (INTR_ACKERR | INTR_MAS_ERR)) {
		if (cmd->op == MASTER_DAA) {
			slot = master->daa_anchor.idx;
			master->daa_anchor.daa_addr[slot].used = false;
		}

		/* GETMXDS 5bytes, but slave maybe response less than 5bytes */
		fifo_offset = 0x1f & mtk_i3c_readw(master, FIFO_STAT);
		if ((cmd->ccc_id == I3C_CCC_GETMXDS) && (fifo_offset > 0)) {
			data = xfer->cmds->rx_buf;
			while (fifo_offset--) {
				*data = readb(master->regs + DATA_PORT);
				data++;
			}
			xfer->ret = 0;
		} else {
			dev_err(master->dev, "Addr: %x, device ACK error\n",
							cmd->addr);
			xfer->ret = -ENXIO;
			i3c_reg_dump(master);
		}

	}

	mtk_i3c_master_apdma_end(master);

	complete(&xfer->complete);
	xfer = list_first_entry_or_null(&master->xferqueue.list,
					struct mtk_i3c_xfer, node);
	if (xfer)
		list_del_init(&xfer->node);

	master->xferqueue.cur = xfer;
	mtk_i3c_master_start_xfer_locked(master);
}

static void mtk_i3c_handle_ibi_payload(struct mtk_i3c_master *master)
{
	struct mtk_i3c_i2c_dev_data *data;
	struct i3c_ibi_slot *slot;
	struct i3c_dev_desc *i3cdev = NULL;
	u32 i;
	u16 fifo_len;
	u16 reg;
	u8 *buf;
	bool match = false;

	i3c_bus_for_each_i3cdev(&master->mas_ctrler.bus, i3cdev) {
		if (i3cdev->info.dyn_addr == 0x8)
			continue;
		if (i3cdev->info.dyn_addr == master->ibi_addr) {
			match = true;
			break;
		}
	}

	if (!i3cdev) {
		pr_err("[%s] not matched i3cdev(addr:%x)\n",
			__func__, master->ibi_addr);
		return;
	}

	if (!match) {
		pr_err("ibi addr not matched\n");
		return;
	}
	data = i3c_dev_get_master_data(i3cdev);
	spin_lock(&master->ibi.lock);
	slot = i3c_generic_ibi_get_free_slot(data->ibi_pool);
	if (!slot) {
		pr_err("[%s] not free slot\n", __func__);
		goto out_unlock;
	}
	buf = slot->data;
	fifo_len = 0x1f & mtk_i3c_readw(master, FIFO_STAT);
	fifo_len = MIN(fifo_len, i3cdev->info.max_ibi_len);
	fifo_len = MIN(fifo_len, i3cdev->ibi->max_payload_len);
	slot->len = fifo_len;
	for (i = 0; i < fifo_len; i++)
		*(buf + i) = mtk_i3c_readw(master, DATA_PORT);

	/*  disable slave ibi */
	mtk_i3c_writew(master, FIFO_CLR, FIFO_ADDR_CLR);
	reg = mtk_i3c_readw(master, CONTROL);
	mtk_i3c_writew(master, reg | CONTROL_RS, CONTROL);
	mtk_i3c_writew(master, i3cdev->info.dyn_addr << 1, SLAVE_ADDR);
	mtk_i3c_writew(master, 1, TRANSFER_LEN);
	mtk_i3c_writew(master, 0, TRANSFER_LEN_AUX);
	mtk_i3c_writew(master, 1, TRANSAC_LEN);
	mtk_i3c_writew(master, 0x4080, TRAFFIC);
	mtk_i3c_writew(master, 6, SHAPE);
	mtk_i3c_writew(master, HFIFO_DATA_7E, HFIFO_DATA);
	mtk_i3c_writew(master, HFIFO_HEAD | I3C_CCC_ID(0x1, false), HFIFO_DATA);
	mtk_i3c_writew(master, 1, DATA_PORT);
	/* execute START after DATA_PORT */
	mb();
	mtk_i3c_writew(master, 1, START);
	/* disable master ibi */
	reg = mtk_i3c_readw(master, INTR_MASK);
	mtk_i3c_writew(master, reg & ~(INTR_IBI), INTR_MASK);
	reg = mtk_i3c_readw(master, TRAFFIC);
	mtk_i3c_writew(master, reg & (~TRAFFIC_IBI_EN), TRAFFIC);

	i3c_master_queue_ibi(i3cdev, slot);

out_unlock:
	spin_unlock(&master->ibi.lock);

}
static void mtk_i3c_handle_ibi(struct mtk_i3c_master *master)
{
	struct i3c_dev_desc *i3cdev = NULL;

	u16 ibi_addr;
	u16 ibi_len;
	u16 control_reg;
	bool match;

	match = false;
	ibi_addr = mtk_i3c_readw(master, CHN_ERROR);
	ibi_addr = ibi_addr >> 9;
	master->ibi_addr = ibi_addr;

	/* read ibi payload */
	i3c_bus_for_each_i3cdev(&master->mas_ctrler.bus, i3cdev) {
		if (i3cdev->info.dyn_addr == 0x8)
			continue;
		if (i3cdev->info.dyn_addr == ibi_addr) {
			match = true;
			break;
		}
	}

	if (!i3cdev) {
		pr_err("[%s] not matched i3cdev(addr:%x)\n",
			__func__, ibi_addr);
		return;
	}

	if (!match) {
		mtk_i3c_writew(master, 1, SOFTRESET);
		return;
	}
	/* Select min value between max_ibi_len and max_payload_len */
	ibi_len = MIN(i3cdev->info.max_ibi_len, i3cdev->ibi->max_payload_len);

	spin_lock(&master->ibi.lock);
	control_reg = CONTROL_ACKERR_DET_EN;
	mtk_i3c_writew(master, control_reg, CONTROL);
	mtk_i3c_writew(master, FIFO_CLR, FIFO_ADDR_CLR);
	/* Clear IBI INT */
	mtk_i3c_writew(master, 0x1ff, INTR_STAT);
	mtk_i3c_writew(master, INTR_ALL | INTR_IBI, INTR_MASK);
	mtk_i3c_writew(master, TRAFFIC_SKIP_SLV_ADDR | TRAFFIC_HANDOFF |
					TRAFFIC_TBIT | TRAFFIC_IBI_EN, TRAFFIC);
	mtk_i3c_writew(master, 0x6, SHAPE);
	mtk_i3c_writew(master, ibi_len, TRANSFER_LEN);
	mtk_i3c_writew(master, 0, TRANSFER_LEN_AUX);
	mtk_i3c_writew(master, 1, TRANSAC_LEN);
	mtk_i3c_writew(master, START_EN	| START_MUL_TRIG | START_MUL_CNFG,

									START);
	/* Execute START before SLAVE_ADDR */
	mb();
	mtk_i3c_writew(master, (ibi_addr << 1) | 0x1, SLAVE_ADDR);
	/* Execute START after SLAVE_ADDR */
	mb();
	mtk_i3c_writew(master, 0x4001, START);
	/* Execute START before SOFTRESET */
	mb();
	mtk_i3c_writew(master, I3C_ERR_RST, SOFTRESET);
	spin_unlock(&master->ibi.lock);
}

static irqreturn_t mtk_i3c_master_irq(int irqno, void *dev_id)
{
	struct mtk_i3c_master *master = dev_id;
	struct mtk_i3c_xfer *xfer;
	struct mtk_i3c_cmd *cmd;
	u16 restart_flag = 0, intr_stat;
	unsigned long flags;
	int ret = 0;
	u8 addr;

	intr_stat = mtk_i3c_readw(master, INTR_STAT);
	mtk_i3c_writew(master, intr_stat, INTR_STAT);

	xfer = master->xferqueue.cur;
	if (intr_stat & INTR_IBI) {
		master->ibi_intr = intr_stat;
		mtk_i3c_handle_ibi(master);
		return IRQ_HANDLED;
	}

	if ((master->ibi_intr & INTR_IBI) && (intr_stat & INTR_TRANSAC_COMP)) {
		mtk_i3c_handle_ibi_payload(master);
		master->ibi_intr = 0;
		return IRQ_HANDLED;
	}

	if (!xfer)
		return IRQ_HANDLED;
	cmd = &xfer->cmds[xfer->trans_num];

	if (xfer->auto_restart)
		restart_flag = INTR_RS_MULTI;

	spin_lock_irqsave(&master->xferqueue.lock, flags);
	/*
	 * when occurs ack error, i3c controller generate two interrupts
	 * first is the ack error interrupt, then the complete interrupt
	 * i3c->irq_stat need keep the two interrupt value.
	 */
	xfer->irq_stat |= intr_stat;
	if (xfer->irq_stat & INTR_TRANSAC_COMP) {
		mtk_i3c_master_end_xfer_locked(master);
		goto exit_irq;
	}

	if (xfer->irq_stat & restart_flag) {
		xfer->irq_stat = 0;

		if (xfer->ignore_restart_irq) {
			xfer->ignore_restart_irq = false;
			mtk_i3c_writew(master, START_MUL_CNFG | START_MUL_TRIG |
					       START_EN, START);
		} else if (cmd->op == MASTER_DAA) {
			mtk_i3c_writew(master, FIFO_CLR, FIFO_ADDR_CLR);
			master->daa_count++;
			if (master->daa_count < MAX_I3C_DEVS) {
				addr = mtk_i3c_master_get_free_addr(master);
				mtk_i3c_writew(master, (DEF_DAA_SLV_PARITY |
						USE_DEF_DA | addr), DEF_DA);
			} else
				master->daa_count = 0;

			mtk_i3c_writew(master, START_MUL_CNFG | START_MUL_TRIG |
					       START_EN, START);
		} else {
			mtk_i3c_master_apdma_end(master);

			xfer->trans_num++;
			ret = mtk_i3c_master_start_transfer(master);
			xfer->ret = ret;
		}
	}
exit_irq:
	spin_unlock_irqrestore(&master->xferqueue.lock, flags);
	return IRQ_HANDLED;
}

static void mtk_i3c_master_queue_xfer(struct mtk_i3c_master *master,
				      struct mtk_i3c_xfer *xfer)
{
	unsigned long flags;

	init_completion(&xfer->complete);
	spin_lock_irqsave(&master->xferqueue.lock, flags);
	if (master->xferqueue.cur) {
		list_add_tail(&xfer->node, &master->xferqueue.list);
	} else {
		master->xferqueue.cur = xfer;
		i3c_debug = 0;
		mtk_i3c_master_start_xfer_locked(master);
		i3c_debug = 0;
	}
	spin_unlock_irqrestore(&master->xferqueue.lock, flags);
}

static void mtk_i3c_master_unqueue_xfer(struct mtk_i3c_master *master,
					struct mtk_i3c_xfer *xfer)
{
	struct mtk_i3c_cmd *ccmd = &xfer->cmds[xfer->trans_num];
	unsigned long flags;

	dev_err(master->dev, "addr: %x, transfer timeout\n", ccmd->addr);
	xfer->ret = -ETIMEDOUT;

	i3c_reg_dump(master);
	spin_lock_irqsave(&master->xferqueue.lock, flags);
	mtk_i3c_master_apdma_end(master);
	if (master->xferqueue.cur == xfer)
		master->xferqueue.cur = NULL;
	else
		list_del_init(&xfer->node);
	spin_unlock_irqrestore(&master->xferqueue.lock, flags);
}

static bool mtk_i3c_master_supports_ccc_cmd(struct i3c_master_controller *m,
					    const struct i3c_ccc_cmd *cmd)
{
	if (cmd->ndests > 1)
		return false;

	switch (cmd->id) {
	case I3C_CCC_ENEC(true):
	case I3C_CCC_ENEC(false):
	case I3C_CCC_DISEC(true):
	case I3C_CCC_DISEC(false):
	case I3C_CCC_ENTAS(0, true):
	case I3C_CCC_ENTAS(0, false):
	case I3C_CCC_RSTDAA(true):
	case I3C_CCC_RSTDAA(false):
	/* FALL THROUGH */
	case I3C_CCC_ENTDAA:
	case I3C_CCC_SETMWL(true):
	case I3C_CCC_SETMWL(false):
	case I3C_CCC_SETMRL(true):
	case I3C_CCC_SETMRL(false):
	/* FALL THROUGH */
	case I3C_CCC_DEFSLVS:
	case I3C_CCC_ENTHDR(0):
	case I3C_CCC_SETDASA:
	case I3C_CCC_SETNEWDA:
	case I3C_CCC_GETMWL:
	case I3C_CCC_GETMRL:
	case I3C_CCC_GETPID:
	case I3C_CCC_GETBCR:
	case I3C_CCC_GETDCR:
	case I3C_CCC_GETSTATUS:
	case I3C_CCC_GETACCMST:
	case I3C_CCC_GETMXDS:
	case I3C_CCC_GETHDRCAP:
		return true;
	default:
		break;
	}

	return false;
}

static int mtk_i3c_master_send_ccc_cmd(struct i3c_master_controller *m,
				       struct i3c_ccc_cmd *cmd)
{
	struct mtk_i3c_master *master = to_mtk_i3c_master(m);
	struct mtk_i3c_xfer *mtk_xfer;
	struct mtk_i3c_cmd *ccmd;
	int ret = 0;

	mtk_xfer = mtk_i3c_master_alloc_xfer(master, 1);
	if (!mtk_xfer)
		return -ENOMEM;

	mtk_xfer->mode = I3C_CCC;
	mtk_xfer->ignore_restart_irq = false;
	mtk_xfer->auto_restart = false;

	ccmd = mtk_xfer->cmds;
	ccmd->addr = cmd->dests[0].addr;
	ccmd->ccc_id = cmd->id;

	if (cmd->rnw) {
		ccmd->op = MASTER_RD;
		ccmd->rx_len = cmd->dests[0].payload.len;
		ccmd->rx_buf = cmd->dests[0].payload.data;
	} else {
		ccmd->op = MASTER_WR;
		ccmd->tx_len = cmd->dests[0].payload.len;
		ccmd->tx_buf = cmd->dests[0].payload.data;
	}

	if (ccmd->ccc_id < I3C_CCC_DIRECT) {
		ccmd->op = MASTER_CCC_BROADCAST;
		if (ccmd->ccc_id == I3C_CCC_ENTDAA) {
			ccmd->op = MASTER_DAA;
			ccmd->rx_len = 8;
			ccmd->tx_len = 1;
			mtk_xfer->ignore_restart_irq = false;
			mtk_xfer->auto_restart = true;
		}
	}

	mtk_i3c_master_queue_xfer(master, mtk_xfer);
	if (!wait_for_completion_timeout(&mtk_xfer->complete,
					 msecs_to_jiffies(2000)))
		mtk_i3c_master_unqueue_xfer(master, mtk_xfer);

	ret = mtk_xfer->ret;

	mtk_i3c_master_free_xfer(mtk_xfer);
	if (ret == -ENXIO)
		return I3C_ERROR_M2;

	return ret;
}

static int mtk_i3c_master_priv_xfers(struct i3c_dev_desc *dev,
				     struct i3c_priv_xfer *xfers,
				     int nxfers)
{
	struct i3c_master_controller *m = i3c_dev_get_master(dev);
	struct mtk_i3c_master *master = to_mtk_i3c_master(m);
	struct mtk_i3c_xfer *mtk_xfer;
	int i, ret = 0;

	if (!nxfers)
		return 0;

	mtk_xfer = mtk_i3c_master_alloc_xfer(master, nxfers);
	if (!mtk_xfer)
		return -ENOMEM;

	mtk_xfer->mode = I3C_TRANSFER;

	if (nxfers == 2 && !xfers[0].rnw && xfers[1].rnw)
		mtk_xfer->auto_restart = false;
	else
		mtk_xfer->auto_restart = true;

	if (mtk_xfer->auto_restart && nxfers >= 2)
		mtk_xfer->ignore_restart_irq = true;
	else
		mtk_xfer->ignore_restart_irq = false;

	for (i = 0; i < nxfers; i++) {
		struct mtk_i3c_cmd *ccmd = &mtk_xfer->cmds[i];

		ccmd->addr = dev->info.dyn_addr;
		ccmd->ccc_id = 0;

		if (!mtk_xfer->auto_restart) {
			/* combined two messages into one transaction */
			ccmd->op = MASTER_WRRD;
			ccmd->tx_len = xfers[i].len;
			ccmd->rx_len = xfers[i + 1].len;
			ccmd->tx_buf = xfers->data.out;
			ccmd->rx_buf = xfers[i + 1].data.in;
			break;
		}

		if (xfers[i].rnw) {
			ccmd->op = MASTER_RD;
			ccmd->rx_len = xfers->len;
			ccmd->rx_buf = xfers->data.in;
		} else {
			ccmd->op = MASTER_WR;
			ccmd->tx_len = xfers->len;
			ccmd->tx_buf = xfers->data.out;
		}
	}

	mtk_i3c_master_queue_xfer(master, mtk_xfer);
	if (!wait_for_completion_timeout(&mtk_xfer->complete,
					 msecs_to_jiffies(2000)))
		mtk_i3c_master_unqueue_xfer(master, mtk_xfer);

	ret = mtk_xfer->ret;

	mtk_i3c_master_free_xfer(mtk_xfer);
	return ret;
}

static int mtk_i3c_master_i2c_xfers(struct i2c_dev_desc *dev,
				    const struct i2c_msg *xfers,
				    int nxfers)
{
	struct i3c_master_controller *m = i2c_dev_get_master(dev);
	struct mtk_i3c_master *master = to_mtk_i3c_master(m);
	struct mtk_i3c_xfer *mtk_xfer;
	int i, ret = 0;

	mtk_xfer = mtk_i3c_master_alloc_xfer(master, nxfers);
	if (!mtk_xfer)
		return -ENOMEM;

	mtk_xfer->mode = I2C_TRANSFER;

	if (!(xfers[0].flags & I2C_M_RD) && (xfers[1].flags & I2C_M_RD) &&
	    xfers[0].addr == xfers[1].addr && nxfers == 2)
		mtk_xfer->auto_restart = false;
	else
		mtk_xfer->auto_restart = true;

	mtk_xfer->ignore_restart_irq = false;

	for (i = 0; i < nxfers; i++) {
		struct mtk_i3c_cmd *ccmd = &mtk_xfer->cmds[i];

		ccmd->addr = xfers->addr;
		ccmd->ccc_id = 0;

		if (!mtk_xfer->auto_restart) {
			/* combined two messages into one transaction */
			ccmd->op = MASTER_WRRD;
			ccmd->tx_len = xfers[i].len;
			ccmd->rx_len = xfers[i + 1].len;
			ccmd->tx_buf = xfers[i].buf;
			ccmd->rx_buf = xfers[i + 1].buf;
			break;
		}

		if (xfers[i].flags & I2C_M_RD) {
			ccmd->op = MASTER_RD;
			ccmd->rx_len = xfers[i].len;
			ccmd->rx_buf = xfers[i].buf;
		} else {
			ccmd->op = MASTER_WR;
			ccmd->tx_len = xfers[i].len;
			ccmd->tx_buf = xfers[i].buf;
		}
	}

	mtk_i3c_master_queue_xfer(master, mtk_xfer);
	if (!wait_for_completion_timeout(&mtk_xfer->complete,
					 msecs_to_jiffies(2000)))
		mtk_i3c_master_unqueue_xfer(master, mtk_xfer);

	ret = mtk_xfer->ret;

	mtk_i3c_master_free_xfer(mtk_xfer);
	return ret;
}

static int mtk_i3c_master_do_daa(struct i3c_master_controller *m)
{
	struct mtk_i3c_master *master = to_mtk_i3c_master(m);
	int ret = 0, slot;
	u8 last_addr = 0;

	for (slot = 0; slot < MAX_I3C_DEVS; slot++) {
		if (master->daa_anchor.daa_addr[slot].used)
			continue;

		ret = i3c_master_get_free_addr(m, last_addr + 1);
		if (ret < 0)
			return -ENOSPC;

		last_addr = ret;
		master->daa_anchor.daa_addr[slot].addr = last_addr;
	}

	ret = i3c_master_entdaa_locked(&master->mas_ctrler);
	if (ret && ret != I3C_ERROR_M2)
		return ret;

	/*
	 * Clear all retaining registers filled during DAA. We already
	 * have the addressed assigned to them in the addrs array.
	 */
	for (slot = 1; slot < MAX_I3C_DEVS; slot++) {
		if (master->daa_anchor.daa_addr[slot].used) {
			last_addr = master->daa_anchor.daa_addr[slot].addr;
			i3c_master_add_i3c_dev_locked(m, last_addr);
		}
	}

	/*
	 * Clear slots that ended up not being used. Can be caused by I3C
	 * device creation failure or when the I3C device was already known
	 * by the system but with a different address (in this case the device
	 * already has a slot and does not need a new one).
	 */
	i3c_master_defslvs_locked(&master->mas_ctrler);

	/* Unmask Hot-Join and Mastership request interrupts. */
	i3c_master_enec_locked(m, I3C_BROADCAST_ADDR, I3C_CCC_EVENT_HJ |
				  I3C_CCC_EVENT_MR);

	return 0;
}

static int mtk_i3c_master_bus_init(struct i3c_master_controller *m)
{
	struct mtk_i3c_master *master = to_mtk_i3c_master(m);
	struct i3c_device_info info = { };
	unsigned long source_clk_rate;
	unsigned long bc_clk_rate;
	int ret;

	source_clk_rate = clk_get_rate(master->clk_main) / HS_CLK_DIV;
	bc_clk_rate = clk_get_rate(master->clk_main) / INTER_CLK_DIV;
	ret = mtk_i3c_set_speed(master, source_clk_rate, bc_clk_rate);
	if (ret) {
		dev_err(master->dev, "Failed to set the bus speed.\n");
		return -EINVAL;
	}

	/* Get an address for the master. */
	ret = i3c_master_get_free_addr(m, 0);
	if (ret < 0)
		return ret;

	master->daa_anchor.daa_addr[0].addr = (u8)ret;
	master->daa_anchor.daa_addr[0].used = true;
	master->daa_anchor.idx = 0;

	/*
	 * In I3C protocol host controller is also with device role,
	 * so the driver should provide dcr, bcr, and pid info
	 * of host controller itself
	 */
	memset(&info, 0, sizeof(info));
	info.dyn_addr = ret;
	info.dcr = MTK_I3C_DCR;
	info.bcr = MTK_I3C_BCR;
	info.pid = MTK_I3C_PID;

	if (info.bcr & I3C_BCR_HDR_CAP)
		info.hdr_cap = I3C_CCC_HDR_MODE(I3C_HDR_DDR);

	return i3c_master_set_info(&master->mas_ctrler, &info);
}

static int mt_i3c_master_disable_ibi(struct i3c_dev_desc *dev)
{
	int ret = 0;
	struct i3c_master_controller *m = i3c_dev_get_master(dev);
	struct mtk_i3c_master *master = to_mt_i3c_master(m);
	unsigned long flags;
	u16 reg;

	dev_info(master->dev, "enter %s\n", __func__);

	ret = i3c_master_disec_locked(m, dev->info.dyn_addr,
					  I3C_CCC_EVENT_SIR);
	if (ret)
		goto DONE;

	spin_lock_irqsave(&master->ibi.lock, flags);
	master->ibi.ibi_en_count--;
	reg = mtk_i3c_readw(master, INTR_MASK);
	mtk_i3c_writew(master, reg & ~(INTR_IBI), INTR_MASK);

	reg = mtk_i3c_readw(master, TRAFFIC);
	mtk_i3c_writew(master, reg & (~TRAFFIC_IBI_EN), TRAFFIC);

	reg = mtk_i3c_readw(master, INTR_STAT);
	mtk_i3c_writew(master, INTR_IBI, INTR_STAT);
	spin_unlock_irqrestore(&master->ibi.lock, flags);

DONE:
	return ret;
}

static int mt_i3c_master_enable_ibi(struct i3c_dev_desc *dev)
{
	struct i3c_master_controller *m = i3c_dev_get_master(dev);
	struct mtk_i3c_master *master = to_mt_i3c_master(m);
	unsigned long flags;
	int ret = 0;
	u16 reg;

	spin_lock_irqsave(&master->ibi.lock, flags);
	reg = mtk_i3c_readw(master, INTR_STAT);
	mtk_i3c_writew(master, INTR_IBI, INTR_STAT);
	master->ibi.ibi_en_count++;

	spin_unlock_irqrestore(&master->ibi.lock, flags);
	ret = i3c_master_enec_locked(m, dev->info.dyn_addr,
					 I3C_CCC_EVENT_SIR);
	if (ret) {
		spin_lock_irqsave(&master->ibi.lock, flags);
		reg = mtk_i3c_readw(master, TRAFFIC);
		mtk_i3c_writew(master, reg & (~TRAFFIC_IBI_EN), TRAFFIC);
		master->ibi.ibi_en_count--;
		spin_unlock_irqrestore(&master->ibi.lock, flags);
		pr_err("Fail to CCC ENEC addr:0x%x", dev->info.dyn_addr);
		goto DONE;
	}
DONE:
	return ret;
}

static int mt_i3c_master_request_ibi(struct i3c_dev_desc *dev,
				       const struct i3c_ibi_setup *req)
{
	struct i3c_master_controller *m = i3c_dev_get_master(dev);
	struct mtk_i3c_master *master = to_mt_i3c_master(m);
	struct mtk_i3c_i2c_dev_data *data;
	unsigned long flags;
	unsigned int i;

	data = kzalloc(sizeof(struct mtk_i3c_i2c_dev_data), GFP_KERNEL);
	i3c_dev_set_master_data(dev, data);

	dev_info(master->dev, "enter %s\n", __func__);

	data->ibi_pool = i3c_generic_ibi_alloc_pool(dev, req);
	if (IS_ERR(data->ibi_pool))
		return PTR_ERR(data->ibi_pool);

	spin_lock_irqsave(&master->ibi.lock, flags);
	for (i = 0; i < master->ibi.num_slots; i++) {
		if (!master->ibi.slots[i]) {
			data->ibi = i;
			master->ibi.slots[i] = dev;
			break;
		}
	}
	spin_unlock_irqrestore(&master->ibi.lock, flags);

	if (i < master->ibi.num_slots)
		return 0;

	i3c_generic_ibi_free_pool(data->ibi_pool);
	data->ibi_pool = NULL;

	return -ENOSPC;
}

static void mt_i3c_master_free_ibi(struct i3c_dev_desc *dev)
{
	struct i3c_master_controller *m = i3c_dev_get_master(dev);
	struct mtk_i3c_master *master = to_mt_i3c_master(m);
	struct mtk_i3c_i2c_dev_data *data = i3c_dev_get_master_data(dev);
	unsigned long flags;

	spin_lock_irqsave(&master->ibi.lock, flags);
	master->ibi.slots[data->ibi] = NULL;
	data->ibi = -1;
	spin_unlock_irqrestore(&master->ibi.lock, flags);

	i3c_generic_ibi_free_pool(data->ibi_pool);

}

static void mt_i3c_master_recycle_ibi_slot(struct i3c_dev_desc *dev,
					     struct i3c_ibi_slot *slot)
{
	struct mtk_i3c_i2c_dev_data *data = i3c_dev_get_master_data(dev);

	i3c_generic_ibi_recycle_slot(data->ibi_pool, slot);
}

static const struct i3c_master_controller_ops mtk_i3c_master_ops = {
	.bus_init = mtk_i3c_master_bus_init,
	.do_daa = mtk_i3c_master_do_daa,
	.supports_ccc_cmd = mtk_i3c_master_supports_ccc_cmd,
	.send_ccc_cmd = mtk_i3c_master_send_ccc_cmd,
	.priv_xfers = mtk_i3c_master_priv_xfers,
	.i2c_xfers = mtk_i3c_master_i2c_xfers,
	.enable_ibi = mt_i3c_master_enable_ibi,
	.disable_ibi = mt_i3c_master_disable_ibi,
	.request_ibi = mt_i3c_master_request_ibi,
	.free_ibi = mt_i3c_master_free_ibi,
	.recycle_ibi_slot = mt_i3c_master_recycle_ibi_slot,
};

static int mtk_i3c_master_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_i3c_master *master;
	struct resource *res;
	int ret, irqnr;

	master = devm_kzalloc(dev, sizeof(*master), GFP_KERNEL);
	if (!master)
		return -ENOMEM;

	master->ibi.ibi_en_count = 0;
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	master->regs = devm_ioremap_resource(dev, res);
	if (IS_ERR(master->regs))
		return PTR_ERR(master->regs);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	master->dma_regs = devm_ioremap_resource(dev, res);
	if (IS_ERR(master->dma_regs))
		return PTR_ERR(master->dma_regs);

	irqnr = platform_get_irq(pdev, 0);
	if (irqnr < 0)
		return irqnr;

	master->irqnr = irqnr;
	ret = devm_request_irq(dev, irqnr, mtk_i3c_master_irq,
			       IRQF_TRIGGER_NONE, DRV_NAME, master);
	if (ret < 0) {
		dev_err(dev, "Request I3C IRQ %d fail\n", irqnr);
		return ret;
	}

	spin_lock_init(&master->xferqueue.lock);
	INIT_LIST_HEAD(&master->xferqueue.list);

	if (dma_set_mask(dev, DMA_BIT_MASK(33))) {
		dev_err(dev, "dma_set_mask return error.\n");
		return -EINVAL;
	}

	master->clk_main = devm_clk_get(dev, "main");
	if (IS_ERR(master->clk_main)) {
		dev_err(dev, "cannot get main clock\n");
		return PTR_ERR(master->clk_main);
	}
	master->clk_dma = devm_clk_get(dev, "dma");
	if (IS_ERR(master->clk_dma)) {
		dev_err(dev, "cannot get dma clock\n");
		return PTR_ERR(master->clk_dma);
	}

	master->clk_arb = devm_clk_get(dev, "arb");
	if (IS_ERR(master->clk_arb))
		dev_err(dev, "get fail arb clock or no need\n");

	ret = mtk_i3c_master_clock_enable(master);
	if (ret) {
		dev_err(dev, "clock enable failed!\n");
		return ret;
	}

	master->dev = dev;
	platform_set_drvdata(pdev, master);

	master->ibi.num_slots = MAX_I3C_DEVS;
	master->ibi.slots = devm_kcalloc(&pdev->dev, master->ibi.num_slots,
					 sizeof(*master->ibi.slots),
					 GFP_KERNEL);
	if (!master->ibi.slots)
		return -ENOMEM;

	master->ibi_intr = 0;
	master->daa_count = 0;
	ret = i3c_master_register(&master->mas_ctrler, dev,
				  &mtk_i3c_master_ops, false);
	if (ret) {
		dev_err(dev, "Failed to add i3c bus to i3c core\n");
		mtk_i3c_master_clock_disable(master);
		return ret;
	}

	return 0;
}

static int mtk_i3c_master_remove(struct platform_device *pdev)
{
	struct mtk_i3c_master *master = platform_get_drvdata(pdev);

	i3c_master_unregister(&master->mas_ctrler);
	mtk_i3c_master_clock_disable(master);

	return 0;
}

static const struct of_device_id mtk_i3c_master_of_ids[] = {
	{ .compatible = "mediatek,i3c-master" },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, mtk_i3c_master_of_ids);

static int mtk_i3c_suspend_noirq(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mtk_i3c_master *master = platform_get_drvdata(pdev);

	mtk_i3c_master_clock_disable(master);
	return 0;
}

static int mtk_i3c_resume_noirq(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mtk_i3c_master *master = platform_get_drvdata(pdev);
	int ret;

	ret = mtk_i3c_master_clock_enable(master);
	return ret;
}

static const struct dev_pm_ops mtk_i3c_dev_pm_ops = {
#ifdef CONFIG_PM_SLEEP
	.suspend_noirq = mtk_i3c_suspend_noirq,
	.resume_noirq = mtk_i3c_resume_noirq,
#endif
};

static struct platform_driver mtk_i3c_master_driver = {
	.probe = mtk_i3c_master_probe,
	.remove = mtk_i3c_master_remove,
	.driver = {
		.name = DRV_NAME,
		.pm = &mtk_i3c_dev_pm_ops,
		.of_match_table = mtk_i3c_master_of_ids,
	},
};
module_platform_driver(mtk_i3c_master_driver);

MODULE_DESCRIPTION("MediaTek I3C master driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:mtk-i3c-master");
