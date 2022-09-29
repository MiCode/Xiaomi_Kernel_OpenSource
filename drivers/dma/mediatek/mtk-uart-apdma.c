// SPDX-License-Identifier: GPL-2.0
/*
 * MediaTek UART APDMA driver.
 *
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Long Cheng <long.cheng@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/iopoll.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_dma.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/serial_8250.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/sched/clock.h>

#include "../virt-dma.h"
#include "../../tty/serial/8250/8250.h"

/* The default number of virtual channel */
#define MTK_UART_APDMA_NR_VCHANS	8

#define VFF_EN_B		BIT(0)
#define VFF_STOP_B		BIT(0)
#define VFF_FLUSH_B		BIT(0)
/* rx valid size >=  vff thre */
#define VFF_RX_INT_EN_B		(BIT(0) | BIT(1))
/* tx left size >= vff thre */
#define VFF_TX_INT_EN_B		BIT(0)
#define VFF_WARM_RST_B		BIT(0)
#define VFF_RX_INT_CLR_B	3
#define VFF_TX_INT_CLR_B	0
#define VFF_STOP_CLR_B		0
#define VFF_EN_CLR_B		0
#define VFF_INT_EN_CLR_B	0
#define VFF_4G_SUPPORT_CLR_B	0
#define VFF_ORI_ADDR_BITS_NUM    32
#define VFF_RX_FLOWCTL_THRE_SIZE 0xc00
/*
 * interrupt trigger level for tx
 * if threshold is n, no polling is required to start tx.
 * otherwise need polling VFF_FLUSH.
 */
#define VFF_TX_THRE(n)		(n)
/* interrupt trigger level for rx */
#define VFF_RX_THRE(n)		((n) / 16)

#define VFF_RING_SIZE	0xffff
/* invert this bit when wrap ring head again */
#define VFF_RING_WRAP	0x10000

#define VFF_INT_FLAG		0x00
#define VFF_INT_EN		0x04
#define VFF_EN			0x08
#define VFF_RST			0x0c
#define VFF_STOP		0x10
#define VFF_FLUSH		0x14
#define VFF_ADDR		0x1c
#define VFF_LEN			0x24
#define VFF_THRE		0x28
#define VFF_WPT			0x2c
#define VFF_RPT			0x30
#define VFF_RX_FLOWCTL_THRE	 0x34
#define VFF_INT_BUF_SIZE	0x38
/* TX: the buffer size HW can read. RX: the buffer size SW can read. */
#define VFF_VALID_SIZE		0x3c
/* TX: the buffer size SW can write. RX: the buffer size HW can write. */
#define VFF_LEFT_SIZE		0x40
#define VFF_DEBUG_STATUS	0x50
#define VFF_4G_SUPPORT		0x54

#define UART_RECORD_COUNT	5
#define MAX_POLLING_CNT		5000
#define UART_RECORD_MAXLEN	4096
#define CONFIG_UART_DMA_DATA_RECORD

struct uart_info {
	unsigned int wpt_reg;
	unsigned int rpt_reg;
	unsigned int trans_len;
	unsigned long long trans_time;
	unsigned long long trans_duration_time;
	unsigned char rec_buf[UART_RECORD_MAXLEN];
};

struct mtk_uart_apdmacomp {
	unsigned int addr_bits;
};
struct mtk_uart_apdmadev {
	struct dma_device ddev;
	struct clk *clk;
	unsigned int support_bits;
	unsigned int dma_requests;
	unsigned int support_hub;
};

static unsigned int clk_count;

struct mtk_uart_apdma_desc {
	struct virt_dma_desc vd;

	dma_addr_t addr;
	unsigned int avail_len;
};

struct mtk_chan {
	struct virt_dma_chan vc;
	struct dma_slave_config	cfg;
	struct mtk_uart_apdma_desc *desc;
	enum dma_transfer_direction dir;

	void __iomem *base;
	unsigned int irq;

	unsigned int irq_wg;
	unsigned int rx_status;
	unsigned int rec_idx;
	unsigned int cur_rpt;
	unsigned long long rec_total;
	unsigned int start_record_wpt;
	unsigned int start_record_rpt;
	unsigned int start_int_flag;
	unsigned int start_int_en;
	unsigned int start_en;
	unsigned int start_int_buf_size;
	unsigned long long start_record_time;
	struct uart_info rec_info[UART_RECORD_COUNT];
};

static inline struct mtk_uart_apdmadev *
to_mtk_uart_apdma_dev(struct dma_device *d)
{
	return container_of(d, struct mtk_uart_apdmadev, ddev);
}

static inline struct mtk_chan *to_mtk_uart_apdma_chan(struct dma_chan *c)
{
	return container_of(c, struct mtk_chan, vc.chan);
}

static inline struct mtk_uart_apdma_desc *to_mtk_uart_apdma_desc
	(struct dma_async_tx_descriptor *t)
{
	return container_of(t, struct mtk_uart_apdma_desc, vd.tx);
}

static void mtk_uart_apdma_write(struct mtk_chan *c,
			       unsigned int reg, unsigned int val)
{
	writel(val, c->base + reg);
	/* Flush register write */
	mb();
}

static unsigned int mtk_uart_apdma_read(struct mtk_chan *c, unsigned int reg)
{
	return readl(c->base + reg);
}

static void mtk_uart_apdma_desc_free(struct virt_dma_desc *vd)
{
		kfree(container_of(vd, struct mtk_uart_apdma_desc, vd));
}

void mtk_save_uart_apdma_reg(struct dma_chan *chan, unsigned int *reg_buf)
{
	struct mtk_chan *c = to_mtk_uart_apdma_chan(chan);

	reg_buf[0] = mtk_uart_apdma_read(c, VFF_INT_FLAG);
	reg_buf[1] = mtk_uart_apdma_read(c, VFF_INT_EN);
	reg_buf[2] = mtk_uart_apdma_read(c, VFF_EN);
	reg_buf[3] = mtk_uart_apdma_read(c, VFF_FLUSH);
	reg_buf[4] = mtk_uart_apdma_read(c, VFF_ADDR);
	reg_buf[5] = mtk_uart_apdma_read(c, VFF_LEN);
	reg_buf[6] = mtk_uart_apdma_read(c, VFF_THRE);
	reg_buf[7] = mtk_uart_apdma_read(c, VFF_WPT);
	reg_buf[8] = mtk_uart_apdma_read(c, VFF_RPT);
	reg_buf[9] = mtk_uart_apdma_read(c, VFF_INT_BUF_SIZE);
	reg_buf[10] = mtk_uart_apdma_read(c, VFF_VALID_SIZE);
	reg_buf[11] = mtk_uart_apdma_read(c, VFF_LEFT_SIZE);
	reg_buf[12] = mtk_uart_apdma_read(c, VFF_DEBUG_STATUS);
}
EXPORT_SYMBOL(mtk_save_uart_apdma_reg);

void mtk_uart_apdma_start_record(struct dma_chan *chan)
{
	struct mtk_chan *c = to_mtk_uart_apdma_chan(chan);

	c->start_record_wpt =  mtk_uart_apdma_read(c, VFF_WPT);
	c->start_record_rpt = mtk_uart_apdma_read(c, VFF_RPT);
	c->start_int_flag =  mtk_uart_apdma_read(c, VFF_INT_FLAG);
	c->start_int_en =  mtk_uart_apdma_read(c, VFF_INT_EN);
	c->start_en =  mtk_uart_apdma_read(c, VFF_EN);
	c->start_int_buf_size =  mtk_uart_apdma_read(c, VFF_INT_BUF_SIZE);
	c->start_record_time = sched_clock();
}
EXPORT_SYMBOL(mtk_uart_apdma_start_record);

void mtk_uart_apdma_end_record(struct dma_chan *chan)
{
	struct mtk_chan *c = to_mtk_uart_apdma_chan(chan);
	unsigned int _wpt =  mtk_uart_apdma_read(c, VFF_WPT);
	unsigned int _rpt = mtk_uart_apdma_read(c, VFF_RPT);
	unsigned int _int_flag = mtk_uart_apdma_read(c, VFF_INT_FLAG);
	unsigned int _int_en = mtk_uart_apdma_read(c, VFF_INT_EN);
	unsigned int _en = mtk_uart_apdma_read(c, VFF_EN);
	unsigned int _int_buf_size = mtk_uart_apdma_read(c, VFF_INT_BUF_SIZE);
	unsigned long long starttime = c->start_record_time;
	unsigned long long endtime = sched_clock();
	unsigned long ns1 = do_div(starttime, 1000000000);
	unsigned long ns2 = do_div(endtime, 1000000000);

	dev_info(c->vc.chan.device->dev,
			"[%s] [%s] [start %5lu.%06lu] start_wpt=0x%x, start_rpt=0x%x,\n"
			"start_int_flag=0x%x, start_int_en=0x%x, start_en=0x%x, start_int_buf_size=0x%x\n",
			__func__, c->dir == DMA_DEV_TO_MEM ? "dma_rx" : "dma_tx",
			(unsigned long)starttime, ns1 / 1000,
			c->start_record_wpt, c->start_record_rpt, c->start_int_flag,
			c->start_int_en, c->start_en, c->start_int_buf_size);
	dev_info(c->vc.chan.device->dev,
			"[%s] [%s] [end %5lu.%06lu] end_wpt=0x%x, end_rpt=0x%x\n"
			"end_int_flag=0x%x, end_int_en=0x%x, end_en=0x%x, end_int_buf_size=0x%x\n",
			__func__, c->dir == DMA_DEV_TO_MEM ? "dma_rx" : "dma_tx",
			(unsigned long)endtime, ns2 / 1000, _wpt, _rpt, _int_flag,
			_int_en, _en, _int_buf_size);

}
EXPORT_SYMBOL(mtk_uart_apdma_end_record);

void mtk_uart_apdma_data_dump(struct dma_chan *chan)
{
	struct mtk_chan *c = to_mtk_uart_apdma_chan(chan);
	unsigned int count = 0;
	unsigned int idx = 0;

	idx = c->rec_total < UART_RECORD_COUNT ? 0 : (c->rec_idx + 1) % UART_RECORD_COUNT;

	while (count < min_t(unsigned int, UART_RECORD_COUNT, c->rec_total)) {
		unsigned long long endtime = c->rec_info[idx].trans_time;
		unsigned long long durationtime = c->rec_info[idx].trans_duration_time;
		unsigned long ns = 0;
		unsigned long long elapseNs = do_div(durationtime, 1000000000);
#ifdef CONFIG_UART_DMA_DATA_RECORD
		unsigned int cnt = 0;
		unsigned int cyc = 0;
		unsigned int len = c->rec_info[idx].trans_len;
		unsigned char raw_buf[256*3 + 4];
		const unsigned char *ptr = c->rec_info[idx].rec_buf;
#endif

		ns = do_div(endtime, 1000000000);
		dev_info(c->vc.chan.device->dev,
			"[%s] [%s] [begin %5lu.%06lu] [elapsed time %5lu.%06lu] total=%llu,idx=%d,wpt=0x%x,rpt=0x%x,len=%d\n",
			__func__, c->dir == DMA_DEV_TO_MEM ? "dma_rx" : "dma_tx",
			(unsigned long)endtime, ns / 1000,
			(unsigned long)durationtime, elapseNs/1000,
			c->rec_total, idx,
			c->rec_info[idx].wpt_reg, c->rec_info[idx].rpt_reg,
			c->rec_info[idx].trans_len);
#ifdef CONFIG_UART_DMA_DATA_RECORD
		if (len > UART_RECORD_MAXLEN) {
			pr_info("[%s] msg len is exceed buf size:%d\n",
				__func__, UART_RECORD_MAXLEN);
			count++;
			continue;
		}

		for (cyc = 0; cyc < len; cyc += 256) {
			unsigned int cnt_min = len - cyc < 256 ? len - cyc : 256;

			for (cnt = 0; cnt < cnt_min; cnt++)
				(void)snprintf(raw_buf + 3 * cnt, 4, "%02X ", ptr[cnt + cyc]);
			raw_buf[3*cnt] = '\0';
			pr_info("%s [%d] data = %s\n",
			    c->dir == DMA_DEV_TO_MEM ? "Rx" : "Tx", cyc, raw_buf);
		}
#endif
		idx++;
		idx = idx % UART_RECORD_COUNT;
		count++;
	}
}
EXPORT_SYMBOL(mtk_uart_apdma_data_dump);

void mtk_uart_rx_setting(struct dma_chan *chan, int copied, int total)
{
	struct mtk_chan *c = to_mtk_uart_apdma_chan(chan);
	unsigned int rpt_old, rpt_new, vff_sz;

	if (total > copied) {
		vff_sz = c->cfg.src_port_window_size;
		rpt_old = mtk_uart_apdma_read(c, VFF_RPT);
		rpt_new = rpt_old + (unsigned int)copied;
		if ((rpt_new & vff_sz) == vff_sz)
			rpt_new = (rpt_new - vff_sz) ^ VFF_RING_WRAP;
		pr_info("%s: copied=%d,total=%d,rpt_old=0x%x,wpt_old=0x%x,rpt_new=0x%x\n",
			__func__, copied, total, rpt_old, c->irq_wg, rpt_new);
		c->irq_wg = rpt_new;
	}
	/* Flush before update vff_rpt */
	mb();
	/* Let DMA start moving data */
	mtk_uart_apdma_write(c, VFF_RPT, c->irq_wg);

}
EXPORT_SYMBOL(mtk_uart_rx_setting);

void mtk_uart_get_apdma_rpt(struct dma_chan *chan, unsigned int *rpt)
{
	struct mtk_chan *c = to_mtk_uart_apdma_chan(chan);

	*rpt = c->cur_rpt & VFF_RING_SIZE;

}
EXPORT_SYMBOL(mtk_uart_get_apdma_rpt);

static void mtk_uart_apdma_start_tx(struct mtk_chan *c)
{
	struct mtk_uart_apdmadev *mtkd =
				to_mtk_uart_apdma_dev(c->vc.chan.device);
	struct mtk_uart_apdma_desc *d = c->desc;
	unsigned int wpt, vff_sz, left_data, rst_status;
	unsigned int idx = 0;
	int poll_cnt = MAX_POLLING_CNT;

	vff_sz = c->cfg.dst_port_window_size;

	left_data = mtk_uart_apdma_read(c, VFF_INT_BUF_SIZE);
	while ((left_data != 0) && (poll_cnt > 0)) {
		udelay(2);
		left_data = mtk_uart_apdma_read(c, VFF_INT_BUF_SIZE);
		poll_cnt--;
	}
	if (poll_cnt != MAX_POLLING_CNT)
		pr_info("%s: poll_cnt[%d] is not MAX_POLLING_CNT!\n", __func__, poll_cnt);

	wpt = mtk_uart_apdma_read(c, VFF_ADDR);
	if (wpt == ((unsigned int)d->addr)) {
		mtk_uart_apdma_write(c, VFF_ADDR, 0);
		mtk_uart_apdma_write(c, VFF_THRE, 0);
		mtk_uart_apdma_write(c, VFF_LEN, 0);
		mtk_uart_apdma_write(c, VFF_RST, VFF_WARM_RST_B);
		/* Make sure cmd sequence */
		mb();
		udelay(1);
		rst_status = mtk_uart_apdma_read(c, VFF_RST);
		if (rst_status != 0) {
			udelay(5);
			pr_info("%s: apdma: rst_status: 0x%x, new rst_status: 0x%x!\n",
				__func__, rst_status, mtk_uart_apdma_read(c, VFF_RST));
		}
	}

	if (!mtk_uart_apdma_read(c, VFF_LEN)) {
		mtk_uart_apdma_write(c, VFF_ADDR, d->addr);
		mtk_uart_apdma_write(c, VFF_LEN, vff_sz);
		mtk_uart_apdma_write(c, VFF_THRE, VFF_TX_THRE(vff_sz));
		mtk_uart_apdma_write(c, VFF_WPT, 0);
		mtk_uart_apdma_write(c, VFF_INT_FLAG, VFF_TX_INT_CLR_B);

		if (mtkd->support_bits > VFF_ORI_ADDR_BITS_NUM)
			mtk_uart_apdma_write(c, VFF_4G_SUPPORT,
					upper_32_bits(d->addr));
	}

	mtk_uart_apdma_write(c, VFF_EN, VFF_EN_B);
	if (mtk_uart_apdma_read(c, VFF_EN) != VFF_EN_B)
		dev_err(c->vc.chan.device->dev, "Enable TX fail\n");

	if (!mtk_uart_apdma_read(c, VFF_LEFT_SIZE)) {
		mtk_uart_apdma_write(c, VFF_INT_EN, VFF_TX_INT_EN_B);
		return;
	}

	wpt = mtk_uart_apdma_read(c, VFF_WPT);

	idx = (unsigned int)(c->rec_idx % UART_RECORD_COUNT);
	c->rec_idx++;
	c->rec_idx = (unsigned int)(c->rec_idx % UART_RECORD_COUNT);
	c->rec_total++;

	c->rec_info[idx].wpt_reg = wpt;
	c->rec_info[idx].rpt_reg = mtk_uart_apdma_read(c, VFF_RPT);
	c->rec_info[idx].trans_len = c->desc->avail_len;
	c->rec_info[idx].trans_time = sched_clock();
#ifdef CONFIG_UART_DMA_DATA_RECORD
	if (d->vd.tx.callback_param != NULL) {
		struct uart_8250_port *p = (struct uart_8250_port *)d->vd.tx.callback_param;
		struct uart_state *u_state = p->port.state;
		struct circ_buf *xmit = &u_state->xmit;
		const char *ptr = xmit->buf + xmit->tail;
		int tx_size = CIRC_CNT_TO_END(xmit->head, xmit->tail, UART_XMIT_SIZE);
		int dump_len = min(tx_size, UART_RECORD_MAXLEN);

		if (u_state != NULL) {
			if (c->rec_info[idx].trans_len <= UART_RECORD_MAXLEN)
				memcpy(c->rec_info[idx].rec_buf, ptr,
					min((unsigned int)dump_len, c->rec_info[idx].trans_len));
		} else {
			dev_info(c->vc.chan.device->dev, "[%s] u_state==NULL\n", __func__);
		}
	}
#endif

	wpt += c->desc->avail_len;
	if ((wpt & VFF_RING_SIZE) == vff_sz)
		wpt = (wpt & VFF_RING_WRAP) ^ VFF_RING_WRAP;

	/* Flush before update vff_wpt */
	mb();
	/* Let DMA start moving data */
	mtk_uart_apdma_write(c, VFF_WPT, wpt);

	/* HW auto set to 0 when left size >= threshold */
	mtk_uart_apdma_write(c, VFF_INT_EN, VFF_TX_INT_EN_B);
	if (!mtk_uart_apdma_read(c, VFF_FLUSH))
		mtk_uart_apdma_write(c, VFF_FLUSH, VFF_FLUSH_B);
}

static void mtk_uart_apdma_start_rx(struct mtk_chan *c)
{
	struct mtk_uart_apdmadev *mtkd =
				to_mtk_uart_apdma_dev(c->vc.chan.device);
	struct mtk_uart_apdma_desc *d = c->desc;
	unsigned int vff_sz;
	int idx;

	if (d == NULL) {
		dev_info(c->vc.chan.device->dev, "%s:[%d] FIX ME1!", __func__, c->irq);
		return;
	}
	idx = (unsigned int)(c->rec_idx % UART_RECORD_COUNT);
	c->rec_info[idx].trans_time = sched_clock();

	vff_sz = c->cfg.src_port_window_size;
	if (!mtk_uart_apdma_read(c, VFF_LEN)) {
		mtk_uart_apdma_write(c, VFF_ADDR, d->addr);
		mtk_uart_apdma_write(c, VFF_LEN, vff_sz);
		mtk_uart_apdma_write(c, VFF_THRE, VFF_RX_THRE(vff_sz));
		mtk_uart_apdma_write(c, VFF_RPT, 0);
		mtk_uart_apdma_write(c, VFF_INT_FLAG, VFF_RX_INT_CLR_B);

		if (mtkd->support_bits > VFF_ORI_ADDR_BITS_NUM)
			mtk_uart_apdma_write(c, VFF_4G_SUPPORT,
					upper_32_bits(d->addr));
	}

	mtk_uart_apdma_write(c, VFF_RX_FLOWCTL_THRE, VFF_RX_FLOWCTL_THRE_SIZE);
	mtk_uart_apdma_write(c, VFF_INT_EN, VFF_RX_INT_EN_B);
	mtk_uart_apdma_write(c, VFF_EN, VFF_EN_B);
	if (mtk_uart_apdma_read(c, VFF_EN) != VFF_EN_B)
		dev_err(c->vc.chan.device->dev, "Enable RX fail\n");
}

static void mtk_uart_apdma_tx_handler(struct mtk_chan *c)
{
	struct mtk_uart_apdma_desc *d = c->desc;
	int idx = (unsigned int)(c->rec_idx % UART_RECORD_COUNT);

	mtk_uart_apdma_write(c, VFF_INT_FLAG, VFF_TX_INT_CLR_B);
	if (unlikely(d == NULL)) {
		dev_info(c->vc.chan.device->dev, "TX[%d] FIX ME!", c->irq);
		return;
	}
	mtk_uart_apdma_write(c, VFF_INT_EN, VFF_INT_EN_CLR_B);
	mtk_uart_apdma_write(c, VFF_EN, VFF_EN_CLR_B);

	list_del(&d->vd.node);
	vchan_cookie_complete(&d->vd);
	c->rec_info[idx].trans_duration_time = sched_clock() - c->rec_info[idx].trans_time;
}

static void mtk_uart_apdma_rx_handler(struct mtk_chan *c)
{
	struct mtk_uart_apdma_desc *d = c->desc;
	unsigned int len, wg, rg;
	int cnt;
	unsigned int idx = 0;

	mtk_uart_apdma_write(c, VFF_INT_FLAG, VFF_RX_INT_CLR_B);
	//Read VFF_VALID_FLAG value
	mb();

	if (!mtk_uart_apdma_read(c, VFF_VALID_SIZE))
		return;

	mtk_uart_apdma_write(c, VFF_EN, VFF_EN_CLR_B);
	mtk_uart_apdma_write(c, VFF_INT_EN, VFF_INT_EN_CLR_B);

	len = c->cfg.src_port_window_size;
	rg = mtk_uart_apdma_read(c, VFF_RPT);
	wg = mtk_uart_apdma_read(c, VFF_WPT);
	cnt = (wg & VFF_RING_SIZE) - (rg & VFF_RING_SIZE);

	/*
	 * The buffer is ring buffer. If wrap bit different,
	 * represents the start of the next cycle for WPT
	 */
	if ((rg ^ wg) & VFF_RING_WRAP)
		cnt += len;

	c->rx_status = d->avail_len - cnt;
	c->irq_wg = wg;
	c->cur_rpt = rg;

	idx = (unsigned int)(c->rec_idx % UART_RECORD_COUNT);
	c->rec_idx++;
	c->rec_idx = (unsigned int)(c->rec_idx % UART_RECORD_COUNT);
	c->rec_total++;

	c->rec_info[idx].wpt_reg = wg;
	c->rec_info[idx].rpt_reg = rg;
	c->rec_info[idx].trans_len = cnt;
	c->rec_info[idx].trans_duration_time = sched_clock() - c->rec_info[idx].trans_time;
#ifdef CONFIG_UART_DMA_DATA_RECORD
	if (d->vd.tx.callback_param != NULL) {
		struct uart_8250_port *p = (struct uart_8250_port *)d->vd.tx.callback_param;
		struct uart_8250_dma *dma = p->dma;

	if ((dma != NULL) && (cnt <= UART_RECORD_MAXLEN))
		memcpy(c->rec_info[idx].rec_buf, (unsigned char *)dma->rx_buf,
			cnt);
	}
#endif

	list_del(&d->vd.node);
	vchan_cookie_complete(&d->vd);
}

static irqreturn_t mtk_uart_apdma_irq_handler(int irq, void *dev_id)
{
	struct dma_chan *chan = (struct dma_chan *)dev_id;
	struct mtk_chan *c = to_mtk_uart_apdma_chan(chan);
	//unsigned long flags;

	//spin_lock_irqsave(&c->vc.lock, flags);
	spin_lock(&c->vc.lock);
	if (c->dir == DMA_DEV_TO_MEM)
		mtk_uart_apdma_rx_handler(c);
	else if (c->dir == DMA_MEM_TO_DEV)
		mtk_uart_apdma_tx_handler(c);
	//spin_unlock_irqrestore(&c->vc.lock, flags);
	spin_unlock(&c->vc.lock);
	return IRQ_HANDLED;
}

static int mtk_uart_apdma_alloc_chan_resources(struct dma_chan *chan)
{
	struct mtk_uart_apdmadev *mtkd = to_mtk_uart_apdma_dev(chan->device);
	struct mtk_chan *c = to_mtk_uart_apdma_chan(chan);
	unsigned int status;
	int ret;
	if (mtkd->support_hub)
		pr_info("debug: %s: clk_count[%d]\n", __func__, clk_count);
	ret = pm_runtime_get_sync(mtkd->ddev.dev);
	if (ret < 0) {
		pm_runtime_put_noidle(chan->device->dev);
		return ret;
	}

	mtk_uart_apdma_write(c, VFF_ADDR, 0);
	mtk_uart_apdma_write(c, VFF_THRE, 0);
	mtk_uart_apdma_write(c, VFF_LEN, 0);
	mtk_uart_apdma_write(c, VFF_RST, VFF_WARM_RST_B);

	ret = readx_poll_timeout(readl, c->base + VFF_EN,
			  status, !status, 10, 100);
	if (ret)
		goto err_pm;

	ret = request_irq(c->irq, mtk_uart_apdma_irq_handler,
			  IRQF_TRIGGER_NONE, KBUILD_MODNAME, chan);
	if (ret < 0) {
		dev_err(chan->device->dev, "Can't request dma IRQ\n");
		ret = -EINVAL;
		goto err_pm;
	}

	ret = enable_irq_wake(c->irq);
	if (ret) {
		dev_info(chan->device->dev, "Can't enable dma IRQ wake\n");
		ret = -EINVAL;
		goto err_pm;
	}

	if (mtkd->support_bits > VFF_ORI_ADDR_BITS_NUM)
		mtk_uart_apdma_write(c, VFF_4G_SUPPORT, VFF_4G_SUPPORT_CLR_B);

err_pm:
	return ret;
}

static void mtk_uart_apdma_free_chan_resources(struct dma_chan *chan)
{
	struct mtk_uart_apdmadev *mtkd = to_mtk_uart_apdma_dev(chan->device);
	struct mtk_chan *c = to_mtk_uart_apdma_chan(chan);

	free_irq(c->irq, chan);

	tasklet_kill(&c->vc.task);

	vchan_free_chan_resources(&c->vc);
	pm_runtime_put_sync(mtkd->ddev.dev);
}

static enum dma_status mtk_uart_apdma_tx_status(struct dma_chan *chan,
					 dma_cookie_t cookie,
					 struct dma_tx_state *txstate)
{
	struct mtk_chan *c = to_mtk_uart_apdma_chan(chan);
	enum dma_status ret;

	ret = dma_cookie_status(chan, cookie, txstate);
	if (!txstate)
		return ret;

	dma_set_residue(txstate, c->rx_status);

	return ret;
}

/*
 * dmaengine_prep_slave_single will call the function. and sglen is 1.
 * 8250 uart using one ring buffer, and deal with one sg.
 */
static struct dma_async_tx_descriptor *mtk_uart_apdma_prep_slave_sg
	(struct dma_chan *chan, struct scatterlist *sgl,
	unsigned int sglen, enum dma_transfer_direction dir,
	unsigned long tx_flags, void *context)
{
	struct mtk_chan *c = to_mtk_uart_apdma_chan(chan);
	struct mtk_uart_apdma_desc *d;
	unsigned int poll_cnt = 0;

	if (!is_slave_direction(dir) || sglen != 1) {
		pr_info("%s is_slave_direction: %d, sglen: %d\n",
			__func__, is_slave_direction(dir), sglen);
		return NULL;
	}

	/* Now allocate and setup the descriptor */
	d = kzalloc(sizeof(*d), GFP_NOWAIT);
	while ((!d) && (poll_cnt < MAX_POLLING_CNT)) {
		udelay(4);
		d = kzalloc(sizeof(*d), GFP_KERNEL);
		poll_cnt++;
	}
	if (!d) {
		pr_info("%s kzalloc fail retry count: %d\n", __func__, poll_cnt);
		return NULL;
	}

	d->avail_len = sg_dma_len(sgl);
	d->addr = sg_dma_address(sgl);
	c->dir = dir;

	return vchan_tx_prep(&c->vc, &d->vd, tx_flags);
}

static void mtk_uart_apdma_issue_pending(struct dma_chan *chan)
{
	struct mtk_chan *c = to_mtk_uart_apdma_chan(chan);
	struct virt_dma_desc *vd;
	unsigned long flags;

	spin_lock_irqsave(&c->vc.lock, flags);
	if (vchan_issue_pending(&c->vc)) {
		vd = vchan_next_desc(&c->vc);
		c->desc = to_mtk_uart_apdma_desc(&vd->tx);

		if (c->dir == DMA_DEV_TO_MEM)
			mtk_uart_apdma_start_rx(c);
		else if (c->dir == DMA_MEM_TO_DEV)
			mtk_uart_apdma_start_tx(c);
	}

	spin_unlock_irqrestore(&c->vc.lock, flags);
}

static int mtk_uart_apdma_slave_config(struct dma_chan *chan,
				   struct dma_slave_config *config)
{
	struct mtk_chan *c = to_mtk_uart_apdma_chan(chan);

	memcpy(&c->cfg, config, sizeof(*config));

	return 0;
}

static int mtk_uart_apdma_terminate_all(struct dma_chan *chan)
{
	struct mtk_chan *c = to_mtk_uart_apdma_chan(chan);
	unsigned long flags;
	unsigned int status;
	LIST_HEAD(head);
	int ret;
	bool state;

	if (mtk_uart_apdma_read(c, VFF_INT_BUF_SIZE)) {
		mtk_uart_apdma_write(c, VFF_FLUSH, VFF_FLUSH_B);
		ret = readx_poll_timeout(readl, c->base + VFF_FLUSH,
				  status, status != VFF_FLUSH_B, 10, 100);
		dev_info(c->vc.chan.device->dev, "flush %s[%d]: %d\n",
			c->dir == DMA_DEV_TO_MEM ? "RX":"TX", c->irq, ret);
		/*
		 * DMA hardware will generate a interrupt immediately
		 * once flush done, so we need to wait the interrupt to be
		 * handled before free resources.
		 */
		state = true;
		while (state)
			irq_get_irqchip_state(c->irq,
				IRQCHIP_STATE_PENDING, &state);
		state = true;
		while (state)
			irq_get_irqchip_state(c->irq,
				IRQCHIP_STATE_ACTIVE, &state);
	}

	/*
	 * Stop need 3 steps.
	 * 1. set stop to 1
	 * 2. wait en to 0
	 * 3. set stop as 0
	 */
	mtk_uart_apdma_write(c, VFF_STOP, VFF_STOP_B);
	ret = readx_poll_timeout(readl, c->base + VFF_EN,
			  status, !status, 10, 100);
	if (ret)
		dev_err(c->vc.chan.device->dev, "stop: fail, status=0x%x\n",
			mtk_uart_apdma_read(c, VFF_DEBUG_STATUS));

	mtk_uart_apdma_write(c, VFF_STOP, VFF_STOP_CLR_B);
	mtk_uart_apdma_write(c, VFF_INT_EN, VFF_INT_EN_CLR_B);

	if (c->dir == DMA_DEV_TO_MEM)
		mtk_uart_apdma_write(c, VFF_INT_FLAG, VFF_RX_INT_CLR_B);
	else if (c->dir == DMA_MEM_TO_DEV)
		mtk_uart_apdma_write(c, VFF_INT_FLAG, VFF_TX_INT_CLR_B);

	synchronize_irq(c->irq);

	spin_lock_irqsave(&c->vc.lock, flags);
	vchan_get_all_descriptors(&c->vc, &head);
	spin_unlock_irqrestore(&c->vc.lock, flags);

	vchan_dma_desc_free_list(&c->vc, &head);

	return 0;
}

static int mtk_uart_apdma_device_pause(struct dma_chan *chan)
{
	struct mtk_chan *c = to_mtk_uart_apdma_chan(chan);
	unsigned long flags;

	spin_lock_irqsave(&c->vc.lock, flags);

	mtk_uart_apdma_write(c, VFF_EN, VFF_EN_CLR_B);
	mtk_uart_apdma_write(c, VFF_INT_EN, VFF_INT_EN_CLR_B);

	synchronize_irq(c->irq);

	spin_unlock_irqrestore(&c->vc.lock, flags);

	return 0;
}

static void mtk_uart_apdma_free(struct mtk_uart_apdmadev *mtkd)
{
	while (!list_empty(&mtkd->ddev.channels)) {
		struct mtk_chan *c = list_first_entry(&mtkd->ddev.channels,
			struct mtk_chan, vc.chan.device_node);

		list_del(&c->vc.chan.device_node);
		tasklet_kill(&c->vc.task);
	}
}

static const struct mtk_uart_apdmacomp mt6779_comp = {
	.addr_bits = 34
};

static const struct mtk_uart_apdmacomp mt6985_comp = {
	.addr_bits = 35
};

static const struct of_device_id mtk_uart_apdma_match[] = {
	{ .compatible = "mediatek,mt6577-uart-dma", .data = NULL},
	{ .compatible = "mediatek,mt2712-uart-dma", .data = NULL},
	{ .compatible = "mediatek,mt6779-uart-dma", .data = &mt6779_comp},
	{ .compatible = "mediatek,mt6985-uart-dma", .data = &mt6985_comp},
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, mtk_uart_apdma_match);

static void mtk_uart_apdma_parse_peri(struct platform_device *pdev)
{
		void __iomem *peri_remap_apdma = NULL;
		unsigned int peri_apdma_base = 0, peri_apdma_mask = 0, peri_apdma_val = 0;
		unsigned int index = 0;

		index = of_property_read_u32_index(pdev->dev.of_node,
			"peri-regs", 0, &peri_apdma_base);
		if (index) {
			pr_notice("[%s] get peri_addr fail\n", __func__);
			return;
		}

		index = of_property_read_u32_index(pdev->dev.of_node,
			"peri-regs", 1, &peri_apdma_mask);
		if (index) {
			pr_notice("[%s] get peri_addr fail\n", __func__);
			return;
		}

		index = of_property_read_u32_index(pdev->dev.of_node,
			"peri-regs", 2, &peri_apdma_val);
		if (index) {
			pr_notice("[%s] get peri_addr fail\n", __func__);
			return;
		}

		peri_remap_apdma = ioremap(peri_apdma_base, 0x10);
		if (!peri_remap_apdma) {
			pr_notice("[%s] peri_remap_addr(%x) ioremap fail\n",
					__func__, peri_apdma_base);

				return;
		}

		writel(((readl(peri_remap_apdma) & (~peri_apdma_mask)) | peri_apdma_val),
			(void *)peri_remap_apdma);

		dev_info(&pdev->dev, "apdma clock protection:0x%x=0x%x",
			peri_apdma_base, readl(peri_remap_apdma));

}

static int mtk_uart_apdma_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct mtk_uart_apdmadev *mtkd;
	int rc;
	struct mtk_chan *c;
	unsigned int i;
	const struct mtk_uart_apdmacomp *comp;

	mtkd = devm_kzalloc(&pdev->dev, sizeof(*mtkd), GFP_KERNEL);
	if (!mtkd)
		return -ENOMEM;

#ifndef CONFIG_FPGA_EARLY_PORTING
	mtkd->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(mtkd->clk)) {
		dev_err(&pdev->dev, "No clock specified\n");
		rc = PTR_ERR(mtkd->clk);
		return rc;
	}
#endif

	comp = of_device_get_match_data(&pdev->dev);
	if (comp == NULL) {
		/*In order to compatiable with legacy device tree file*/
		dev_info(&pdev->dev,
			"No compatiable, using DTS configration\n");

		if (of_property_read_bool(pdev->dev.of_node,
				"mediatek,dma-33bits"))
			mtkd->support_bits = 33;
		else
			mtkd->support_bits = 32;
	} else
		mtkd->support_bits = comp->addr_bits;

	dev_info(&pdev->dev,
			"DMA address bits: %d\n",  mtkd->support_bits);
	rc = dma_set_mask_and_coherent(&pdev->dev,
			DMA_BIT_MASK(mtkd->support_bits));
	if (rc)
		return rc;

	dma_cap_set(DMA_SLAVE, mtkd->ddev.cap_mask);
	mtkd->ddev.device_alloc_chan_resources =
				mtk_uart_apdma_alloc_chan_resources;
	mtkd->ddev.device_free_chan_resources =
				mtk_uart_apdma_free_chan_resources;
	mtkd->ddev.device_tx_status = mtk_uart_apdma_tx_status;
	mtkd->ddev.device_issue_pending = mtk_uart_apdma_issue_pending;
	mtkd->ddev.device_prep_slave_sg = mtk_uart_apdma_prep_slave_sg;
	mtkd->ddev.device_config = mtk_uart_apdma_slave_config;
	mtkd->ddev.device_pause = mtk_uart_apdma_device_pause;
	mtkd->ddev.device_terminate_all = mtk_uart_apdma_terminate_all;
	mtkd->ddev.src_addr_widths = BIT(DMA_SLAVE_BUSWIDTH_1_BYTE);
	mtkd->ddev.dst_addr_widths = BIT(DMA_SLAVE_BUSWIDTH_1_BYTE);
	mtkd->ddev.directions = BIT(DMA_DEV_TO_MEM) | BIT(DMA_MEM_TO_DEV);
	mtkd->ddev.residue_granularity = DMA_RESIDUE_GRANULARITY_SEGMENT;
	mtkd->ddev.dev = &pdev->dev;
	INIT_LIST_HEAD(&mtkd->ddev.channels);

	mtkd->dma_requests = MTK_UART_APDMA_NR_VCHANS;
	if (of_property_read_u32(np, "dma-requests", &mtkd->dma_requests)) {
		dev_info(&pdev->dev,
			 "Using %u as missing dma-requests property\n",
			 MTK_UART_APDMA_NR_VCHANS);
	}

	if (of_property_read_u32(np, "support-hub", &mtkd->support_hub)) {
		mtkd->support_hub = 0;
		dev_info(&pdev->dev,
			 "Using %u as missing support-hub property\n",
			 mtkd->support_hub);
	}

	if (mtkd->support_hub) {
		clk_count = 0;
		if (!clk_prepare_enable(mtkd->clk))
			clk_count++;
		pr_info("[%s]: support_hub[%d], clk_count[%d]\n", __func__,
			mtkd->support_hub, clk_count);
		mtk_uart_apdma_parse_peri(pdev);
	}

	for (i = 0; i < mtkd->dma_requests; i++) {
		c = devm_kzalloc(mtkd->ddev.dev, sizeof(*c), GFP_KERNEL);
		if (!c) {
			rc = -ENODEV;
			goto err_no_dma;
		}

		c->base = devm_platform_ioremap_resource(pdev, i);
		if (IS_ERR(c->base)) {
			rc = PTR_ERR(c->base);
			goto err_no_dma;
		}
		c->vc.desc_free = mtk_uart_apdma_desc_free;
		vchan_init(&c->vc, &mtkd->ddev);

		rc = platform_get_irq(pdev, i);
		if (rc < 0) {
			dev_err(&pdev->dev, "failed to get IRQ[%d]\n", i);
			goto err_no_dma;
		}
		c->irq = rc;
		c->rec_idx = 0;
	}

	pm_runtime_enable(&pdev->dev);
	pm_runtime_set_active(&pdev->dev);

	rc = dma_async_device_register(&mtkd->ddev);
	if (rc)
		goto rpm_disable;

	platform_set_drvdata(pdev, mtkd);

	/* Device-tree DMA controller registration */
	rc = of_dma_controller_register(np, of_dma_xlate_by_chan_id, mtkd);
	if (rc)
		goto dma_remove;

	return rc;

dma_remove:
	dma_async_device_unregister(&mtkd->ddev);
rpm_disable:
	pm_runtime_disable(&pdev->dev);
err_no_dma:
	mtk_uart_apdma_free(mtkd);
	return rc;
}

static int mtk_uart_apdma_remove(struct platform_device *pdev)
{
	struct mtk_uart_apdmadev *mtkd = platform_get_drvdata(pdev);

	of_dma_controller_free(pdev->dev.of_node);

	mtk_uart_apdma_free(mtkd);

	dma_async_device_unregister(&mtkd->ddev);

	pm_runtime_disable(&pdev->dev);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int mtk_uart_apdma_suspend(struct device *dev)
{
	struct mtk_uart_apdmadev *mtkd = dev_get_drvdata(dev);
	pr_info("[%s]: support_hub:%d, clk_count: %d\n", __func__,
		mtkd->support_hub, clk_count);
	if (mtkd->support_hub) {
		pr_info("[%s]: support_hub:%d, skip suspend\n", __func__, mtkd->support_hub);
		return 0;
	}
	if (!pm_runtime_suspended(dev))
		clk_disable_unprepare(mtkd->clk);

	return 0;
}

static int mtk_uart_apdma_resume(struct device *dev)
{
	int ret;
	struct mtk_uart_apdmadev *mtkd = dev_get_drvdata(dev);

	pr_info("[%s]: support_hub:%d, clk_count: %d\n", __func__,
		mtkd->support_hub, clk_count);
	if (!pm_runtime_suspended(dev)) {
		if ((mtkd->support_hub == 1) && (clk_count >= 1))
			return 0;
		ret = clk_prepare_enable(mtkd->clk);
		if (ret)
			return ret;
		if (mtkd->support_hub == 1)
			clk_count++;
		pr_info("[%s]: ret:%d\n", __func__, ret);
	}
	return 0;
}
#endif /* CONFIG_PM_SLEEP */

#ifdef CONFIG_PM
static int mtk_uart_apdma_runtime_suspend(struct device *dev)
{
	struct mtk_uart_apdmadev *mtkd = dev_get_drvdata(dev);
	pr_info("[%s]: support_hub:%d, clk_count: %d\n", __func__, mtkd->support_hub, clk_count);
	if (mtkd->support_hub) {
		pr_info("[%s]: support_hub:%d, skip runtime suspend\n", __func__,
			mtkd->support_hub);
		return 0;
	}

	clk_disable_unprepare(mtkd->clk);

	return 0;
}

static int mtk_uart_apdma_runtime_resume(struct device *dev)
{
	struct mtk_uart_apdmadev *mtkd = dev_get_drvdata(dev);
	pr_info("[%s]: support_hub:%d, clk_count: %d\n", __func__, mtkd->support_hub, clk_count);
	if ((mtkd->support_hub == 1) && (clk_count >= 1))
		return 0;
	if (mtkd->support_hub)
		clk_count++;
	return clk_prepare_enable(mtkd->clk);
}
#endif /* CONFIG_PM */

static const struct dev_pm_ops mtk_uart_apdma_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mtk_uart_apdma_suspend, mtk_uart_apdma_resume)
	SET_RUNTIME_PM_OPS(mtk_uart_apdma_runtime_suspend,
			   mtk_uart_apdma_runtime_resume, NULL)
};

static struct platform_driver mtk_uart_apdma_driver = {
	.probe	= mtk_uart_apdma_probe,
	.remove	= mtk_uart_apdma_remove,
	.driver = {
		.name		= KBUILD_MODNAME,
		.pm		= &mtk_uart_apdma_pm_ops,
		.of_match_table = of_match_ptr(mtk_uart_apdma_match),
	},
};

module_platform_driver(mtk_uart_apdma_driver);

MODULE_DESCRIPTION("MediaTek UART APDMA Controller Driver");
MODULE_AUTHOR("Long Cheng <long.cheng@mediatek.com>");
MODULE_LICENSE("GPL v2");
