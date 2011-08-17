/*
 * TSIF Driver
 *
 * Copyright (c) 2009-2011, Code Aurora Forum. All rights reserved.
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
#include <linux/module.h>       /* Needed by all modules */
#include <linux/kernel.h>       /* Needed for KERN_INFO */
#include <linux/init.h>         /* Needed for the macros */
#include <linux/err.h>          /* IS_ERR etc. */
#include <linux/platform_device.h>

#include <linux/ioport.h>       /* XXX_mem_region */
#include <linux/debugfs.h>
#include <linux/dma-mapping.h>  /* dma_XXX */
#include <linux/delay.h>        /* msleep */

#include <linux/io.h>             /* ioXXX */
#include <linux/uaccess.h>        /* copy_from_user */
#include <linux/clk.h>
#include <linux/wakelock.h>
#include <linux/tsif_api.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>          /* kfree, kzalloc */

#include <mach/gpio.h>
#include <mach/dma.h>
#include <mach/msm_tsif.h>

/*
 * TSIF register offsets
 */
#define TSIF_STS_CTL_OFF               (0x0)
#define TSIF_TIME_LIMIT_OFF            (0x4)
#define TSIF_CLK_REF_OFF               (0x8)
#define TSIF_LPBK_FLAGS_OFF            (0xc)
#define TSIF_LPBK_DATA_OFF            (0x10)
#define TSIF_TEST_CTL_OFF             (0x14)
#define TSIF_TEST_MODE_OFF            (0x18)
#define TSIF_TEST_RESET_OFF           (0x1c)
#define TSIF_TEST_EXPORT_OFF          (0x20)
#define TSIF_TEST_CURRENT_OFF         (0x24)

#define TSIF_DATA_PORT_OFF            (0x100)

/* bits for TSIF_STS_CTL register */
#define TSIF_STS_CTL_EN_IRQ       (1 << 28)
#define TSIF_STS_CTL_PACK_AVAIL   (1 << 27)
#define TSIF_STS_CTL_1ST_PACKET   (1 << 26)
#define TSIF_STS_CTL_OVERFLOW     (1 << 25)
#define TSIF_STS_CTL_LOST_SYNC    (1 << 24)
#define TSIF_STS_CTL_TIMEOUT      (1 << 23)
#define TSIF_STS_CTL_INV_SYNC     (1 << 21)
#define TSIF_STS_CTL_INV_NULL     (1 << 20)
#define TSIF_STS_CTL_INV_ERROR    (1 << 19)
#define TSIF_STS_CTL_INV_ENABLE   (1 << 18)
#define TSIF_STS_CTL_INV_DATA     (1 << 17)
#define TSIF_STS_CTL_INV_CLOCK    (1 << 16)
#define TSIF_STS_CTL_SPARE        (1 << 15)
#define TSIF_STS_CTL_EN_NULL      (1 << 11)
#define TSIF_STS_CTL_EN_ERROR     (1 << 10)
#define TSIF_STS_CTL_LAST_BIT     (1 <<  9)
#define TSIF_STS_CTL_EN_TIME_LIM  (1 <<  8)
#define TSIF_STS_CTL_EN_TCR       (1 <<  7)
#define TSIF_STS_CTL_TEST_MODE    (3 <<  5)
#define TSIF_STS_CTL_EN_DM        (1 <<  4)
#define TSIF_STS_CTL_STOP         (1 <<  3)
#define TSIF_STS_CTL_START        (1 <<  0)

/*
 * Data buffering parameters
 *
 * Data stored in cyclic buffer;
 *
 * Data organized in chunks of packets.
 * One chunk processed at a time by the data mover
 *
 */
#define TSIF_PKTS_IN_CHUNK_DEFAULT  (16)  /**< packets in one DM chunk */
#define TSIF_CHUNKS_IN_BUF_DEFAULT   (8)
#define TSIF_PKTS_IN_CHUNK        (tsif_device->pkts_per_chunk)
#define TSIF_CHUNKS_IN_BUF        (tsif_device->chunks_per_buf)
#define TSIF_PKTS_IN_BUF          (TSIF_PKTS_IN_CHUNK * TSIF_CHUNKS_IN_BUF)
#define TSIF_BUF_SIZE             (TSIF_PKTS_IN_BUF * TSIF_PKT_SIZE)
#define TSIF_MAX_ID               1

#define ROW_RESET                 (MSM_CLK_CTL_BASE + 0x214)
#define GLBL_CLK_ENA              (MSM_CLK_CTL_BASE + 0x000)
#define CLK_HALT_STATEB           (MSM_CLK_CTL_BASE + 0x104)
#define TSIF_NS_REG               (MSM_CLK_CTL_BASE + 0x0b4)
#define TV_NS_REG                 (MSM_CLK_CTL_BASE + 0x0bc)

/* used to create debugfs entries */
static const struct {
	const char *name;
	mode_t mode;
	int offset;
} debugfs_tsif_regs[] = {
	{"sts_ctl",      S_IRUGO | S_IWUSR, TSIF_STS_CTL_OFF},
	{"time_limit",   S_IRUGO | S_IWUSR, TSIF_TIME_LIMIT_OFF},
	{"clk_ref",      S_IRUGO | S_IWUSR, TSIF_CLK_REF_OFF},
	{"lpbk_flags",   S_IRUGO | S_IWUSR, TSIF_LPBK_FLAGS_OFF},
	{"lpbk_data",    S_IRUGO | S_IWUSR, TSIF_LPBK_DATA_OFF},
	{"test_ctl",     S_IRUGO | S_IWUSR, TSIF_TEST_CTL_OFF},
	{"test_mode",    S_IRUGO | S_IWUSR, TSIF_TEST_MODE_OFF},
	{"test_reset",             S_IWUSR, TSIF_TEST_RESET_OFF},
	{"test_export",  S_IRUGO | S_IWUSR, TSIF_TEST_EXPORT_OFF},
	{"test_current", S_IRUGO,           TSIF_TEST_CURRENT_OFF},
	{"data_port",    S_IRUSR,           TSIF_DATA_PORT_OFF},
};

/* structures for Data Mover */
struct tsif_dmov_cmd {
	dmov_box box;
	dma_addr_t box_ptr;
};

struct msm_tsif_device;

struct tsif_xfer {
	struct msm_dmov_cmd hdr;
	struct msm_tsif_device *tsif_device;
	int busy;
	int wi;   /**< set devices's write index after xfer */
};

struct msm_tsif_device {
	struct list_head devlist;
	struct platform_device *pdev;
	struct resource *memres;
	void __iomem *base;
	unsigned int irq;
	int mode;
	u32 time_limit;
	enum tsif_state state;
	struct wake_lock wake_lock;
	/* clocks */
	struct clk *tsif_clk;
	struct clk *tsif_pclk;
	struct clk *tsif_ref_clk;
	/* debugfs */
	struct dentry *dent_tsif;
	struct dentry *debugfs_tsif_regs[ARRAY_SIZE(debugfs_tsif_regs)];
	struct dentry *debugfs_gpio;
	struct dentry *debugfs_action;
	struct dentry *debugfs_dma;
	struct dentry *debugfs_databuf;
	struct debugfs_blob_wrapper blob_wrapper_databuf;
	/* DMA related */
	int dma;
	int crci;
	void *data_buffer;
	dma_addr_t data_buffer_dma;
	u32 pkts_per_chunk;
	u32 chunks_per_buf;
	int ri;
	int wi;
	int dmwi;  /**< DataMover write index */
	struct tsif_dmov_cmd *dmov_cmd[2];
	dma_addr_t dmov_cmd_dma[2];
	struct tsif_xfer xfer[2];
	struct tasklet_struct dma_refill;
	/* statistics */
	u32 stat_rx;
	u32 stat_overflow;
	u32 stat_lost_sync;
	u32 stat_timeout;
	u32 stat_dmov_err;
	u32 stat_soft_drop;
	int stat_ifi; /* inter frame interval */
	u32 stat0, stat1;
	/* client */
	void *client_data;
	void (*client_notify)(void *client_data);
};

/* ===clocks begin=== */

static void tsif_put_clocks(struct msm_tsif_device *tsif_device)
{
	if (tsif_device->tsif_clk) {
		clk_put(tsif_device->tsif_clk);
		tsif_device->tsif_clk = NULL;
	}
	if (tsif_device->tsif_pclk) {
		clk_put(tsif_device->tsif_pclk);
		tsif_device->tsif_pclk = NULL;
	}

	if (tsif_device->tsif_ref_clk) {
		clk_put(tsif_device->tsif_ref_clk);
		tsif_device->tsif_ref_clk = NULL;
	}
}

static int tsif_get_clocks(struct msm_tsif_device *tsif_device)
{
	struct msm_tsif_platform_data *pdata =
		tsif_device->pdev->dev.platform_data;
	int rc = 0;

	if (pdata->tsif_clk) {
		tsif_device->tsif_clk = clk_get(&tsif_device->pdev->dev,
						pdata->tsif_clk);
		if (IS_ERR(tsif_device->tsif_clk)) {
			dev_err(&tsif_device->pdev->dev, "failed to get %s\n",
				pdata->tsif_clk);
			rc = PTR_ERR(tsif_device->tsif_clk);
			tsif_device->tsif_clk = NULL;
			goto ret;
		}
	}
	if (pdata->tsif_pclk) {
		tsif_device->tsif_pclk = clk_get(&tsif_device->pdev->dev,
						 pdata->tsif_pclk);
		if (IS_ERR(tsif_device->tsif_pclk)) {
			dev_err(&tsif_device->pdev->dev, "failed to get %s\n",
				pdata->tsif_pclk);
			rc = PTR_ERR(tsif_device->tsif_pclk);
			tsif_device->tsif_pclk = NULL;
			goto ret;
		}
	}
	if (pdata->tsif_ref_clk) {
		tsif_device->tsif_ref_clk = clk_get(&tsif_device->pdev->dev,
						    pdata->tsif_ref_clk);
		if (IS_ERR(tsif_device->tsif_ref_clk)) {
			dev_err(&tsif_device->pdev->dev, "failed to get %s\n",
				pdata->tsif_ref_clk);
			rc = PTR_ERR(tsif_device->tsif_ref_clk);
			tsif_device->tsif_ref_clk = NULL;
			goto ret;
		}
	}
	return 0;
ret:
	tsif_put_clocks(tsif_device);
	return rc;
}

static void tsif_clock(struct msm_tsif_device *tsif_device, int on)
{
	if (on) {
		if (tsif_device->tsif_clk)
			clk_enable(tsif_device->tsif_clk);
		if (tsif_device->tsif_pclk)
			clk_enable(tsif_device->tsif_pclk);
		clk_enable(tsif_device->tsif_ref_clk);
	} else {
		if (tsif_device->tsif_clk)
			clk_disable(tsif_device->tsif_clk);
		if (tsif_device->tsif_pclk)
			clk_disable(tsif_device->tsif_pclk);
		clk_disable(tsif_device->tsif_ref_clk);
	}
}
/* ===clocks end=== */
/* ===gpio begin=== */

static void tsif_gpios_free(const struct msm_gpio *table, int size)
{
	int i;
	const struct msm_gpio *g;
	for (i = size-1; i >= 0; i--) {
		g = table + i;
		gpio_free(GPIO_PIN(g->gpio_cfg));
	}
}

static int tsif_gpios_request(const struct msm_gpio *table, int size)
{
	int rc;
	int i;
	const struct msm_gpio *g;
	for (i = 0; i < size; i++) {
		g = table + i;
		rc = gpio_request(GPIO_PIN(g->gpio_cfg), g->label);
		if (rc) {
			pr_err("gpio_request(%d) <%s> failed: %d\n",
			       GPIO_PIN(g->gpio_cfg), g->label ?: "?", rc);
			goto err;
		}
	}
	return 0;
err:
	tsif_gpios_free(table, i);
	return rc;
}

static int tsif_gpios_disable(const struct msm_gpio *table, int size)
{
	int rc = 0;
	int i;
	const struct msm_gpio *g;
	for (i = size-1; i >= 0; i--) {
		int tmp;
		g = table + i;
		tmp = gpio_tlmm_config(g->gpio_cfg, GPIO_CFG_DISABLE);
		if (tmp) {
			pr_err("gpio_tlmm_config(0x%08x, GPIO_CFG_DISABLE)"
			       " <%s> failed: %d\n",
			       g->gpio_cfg, g->label ?: "?", rc);
			pr_err("pin %d func %d dir %d pull %d drvstr %d\n",
			       GPIO_PIN(g->gpio_cfg), GPIO_FUNC(g->gpio_cfg),
			       GPIO_DIR(g->gpio_cfg), GPIO_PULL(g->gpio_cfg),
			       GPIO_DRVSTR(g->gpio_cfg));
			if (!rc)
				rc = tmp;
		}
	}

	return rc;
}

static int tsif_gpios_enable(const struct msm_gpio *table, int size)
{
	int rc;
	int i;
	const struct msm_gpio *g;
	for (i = 0; i < size; i++) {
		g = table + i;
		rc = gpio_tlmm_config(g->gpio_cfg, GPIO_CFG_ENABLE);
		if (rc) {
			pr_err("gpio_tlmm_config(0x%08x, GPIO_CFG_ENABLE)"
			       " <%s> failed: %d\n",
			       g->gpio_cfg, g->label ?: "?", rc);
			pr_err("pin %d func %d dir %d pull %d drvstr %d\n",
			       GPIO_PIN(g->gpio_cfg), GPIO_FUNC(g->gpio_cfg),
			       GPIO_DIR(g->gpio_cfg), GPIO_PULL(g->gpio_cfg),
			       GPIO_DRVSTR(g->gpio_cfg));
			goto err;
		}
	}
	return 0;
err:
	tsif_gpios_disable(table, i);
	return rc;
}

static int tsif_gpios_request_enable(const struct msm_gpio *table, int size)
{
	int rc = tsif_gpios_request(table, size);
	if (rc)
		return rc;
	rc = tsif_gpios_enable(table, size);
	if (rc)
		tsif_gpios_free(table, size);
	return rc;
}

static void tsif_gpios_disable_free(const struct msm_gpio *table, int size)
{
	tsif_gpios_disable(table, size);
	tsif_gpios_free(table, size);
}

static int tsif_start_gpios(struct msm_tsif_device *tsif_device)
{
	struct msm_tsif_platform_data *pdata =
		tsif_device->pdev->dev.platform_data;
	return tsif_gpios_request_enable(pdata->gpios, pdata->num_gpios);
}

static void tsif_stop_gpios(struct msm_tsif_device *tsif_device)
{
	struct msm_tsif_platform_data *pdata =
		tsif_device->pdev->dev.platform_data;
	tsif_gpios_disable_free(pdata->gpios, pdata->num_gpios);
}

/* ===gpio end=== */

static int tsif_start_hw(struct msm_tsif_device *tsif_device)
{
	u32 ctl = TSIF_STS_CTL_EN_IRQ |
		  TSIF_STS_CTL_EN_TIME_LIM |
		  TSIF_STS_CTL_EN_TCR |
		  TSIF_STS_CTL_EN_DM;
	dev_info(&tsif_device->pdev->dev, "%s\n", __func__);
	switch (tsif_device->mode) {
	case 1: /* mode 1 */
		ctl |= (0 << 5);
		break;
	case 2: /* mode 2 */
		ctl |= (1 << 5);
		break;
	case 3: /* manual - control from debugfs */
		return 0;
		break;
	default:
		return -EINVAL;
	}
	iowrite32(ctl, tsif_device->base + TSIF_STS_CTL_OFF);
	iowrite32(tsif_device->time_limit,
		  tsif_device->base + TSIF_TIME_LIMIT_OFF);
	wmb();
	iowrite32(ctl | TSIF_STS_CTL_START,
		  tsif_device->base + TSIF_STS_CTL_OFF);
	wmb();
	ctl = ioread32(tsif_device->base + TSIF_STS_CTL_OFF);
	return (ctl & TSIF_STS_CTL_START) ? 0 : -EFAULT;
}

static void tsif_stop_hw(struct msm_tsif_device *tsif_device)
{
	iowrite32(TSIF_STS_CTL_STOP, tsif_device->base + TSIF_STS_CTL_OFF);
	wmb();
}

/* ===DMA begin=== */
/**
 * TSIF DMA theory of operation
 *
 * Circular memory buffer \a tsif_mem_buffer allocated;
 * 4 pointers points to and moved forward on:
 * - \a ri index of first ready to read packet.
 *      Updated by client's call to tsif_reclaim_packets()
 * - \a wi points to the next packet to be written by DM.
 *      Data below is valid and will not be overriden by DMA.
 *      Moved on DM callback
 * - \a dmwi points to the next packet not scheduled yet for DM
 *      moved when packet scheduled for DM
 *
 * In addition, DM xfer keep internal \a wi - copy of \a tsif_device->dmwi
 * at time immediately after scheduling.
 *
 * Initially, 2 packets get scheduled for the DM.
 *
 * Upon packet receive, DM writes packet to the pre-programmed
 * location and invoke its callback.
 *
 * DM callback moves sets wi pointer to \a xfer->wi;
 * then it schedules next packet for DM and moves \a dmwi pointer.
 *
 * Buffer overflow handling
 *
 * If \a dmwi == \a ri-1, buffer is full and \a dmwi can't be advanced.
 * DMA re-scheduled to the same index.
 * Callback check and not move \a wi to become equal to \a ri
 *
 * On \a read request, data between \a ri and \a wi pointers may be read;
 * \ri pointer moved accordingly.
 *
 * It is always granted, on modulo sizeof(tsif_mem_buffer), that
 * \a wi is between [\a ri, \a dmwi]
 *
 * Amount of data available is (wi-ri)*TSIF_PKT_SIZE
 *
 * Number of scheduled packets for DM: (dmwi-wi)
 */

/**
 * tsif_dma_schedule - schedule DMA transfers
 *
 * @tsif_device: device
 *
 * Executed from process context on init, or from tasklet when
 * re-scheduling upon DMA completion.
 * This prevent concurrent execution from several CPU's
 */
static void tsif_dma_schedule(struct msm_tsif_device *tsif_device)
{
	int i, dmwi0, dmwi1, found = 0;
	/* find free entry */
	for (i = 0; i < 2; i++) {
		struct tsif_xfer *xfer = &tsif_device->xfer[i];
		if (xfer->busy)
			continue;
		found++;
		xfer->busy = 1;
		dmwi0 = tsif_device->dmwi;
		tsif_device->dmov_cmd[i]->box.dst_row_addr =
			tsif_device->data_buffer_dma + TSIF_PKT_SIZE * dmwi0;
		/* proposed value for dmwi */
		dmwi1 = (dmwi0 + TSIF_PKTS_IN_CHUNK) % TSIF_PKTS_IN_BUF;
		/**
		 * If dmwi going to overlap with ri,
		 * overflow occurs because data was not read.
		 * Still get this packet, to not interrupt TSIF
		 * hardware, but do not advance dmwi.
		 *
		 * Upon receive, packet will be dropped.
		 */
		if (dmwi1 != tsif_device->ri) {
			tsif_device->dmwi = dmwi1;
		} else {
			dev_info(&tsif_device->pdev->dev,
				 "Overflow detected\n");
		}
		xfer->wi = tsif_device->dmwi;
#ifdef CONFIG_TSIF_DEBUG
		dev_info(&tsif_device->pdev->dev,
			"schedule xfer[%d] -> [%2d]{%2d}\n",
			i, dmwi0, xfer->wi);
#endif
		/* complete all the writes to box */
		dma_coherent_pre_ops();
		msm_dmov_enqueue_cmd(tsif_device->dma, &xfer->hdr);
	}
	if (!found)
		dev_info(&tsif_device->pdev->dev,
			 "All xfer entries are busy\n");
}

/**
 * tsif_dmov_complete_func - DataMover completion callback
 *
 * @cmd:      original DM command
 * @result:   DM result
 * @err:      optional error buffer
 *
 * Executed in IRQ context (Data Mover's IRQ)
 * DataMover's spinlock @msm_dmov_lock held.
 */
static void tsif_dmov_complete_func(struct msm_dmov_cmd *cmd,
				    unsigned int result,
				    struct msm_dmov_errdata *err)
{
	int i;
	u32 data_offset;
	struct tsif_xfer *xfer;
	struct msm_tsif_device *tsif_device;
	int reschedule = 0;
	if (!(result & DMOV_RSLT_VALID)) { /* can I trust to @cmd? */
		pr_err("Invalid DMOV result: rc=0x%08x, cmd = %p", result, cmd);
		return;
	}
	/* restore original context */
	xfer = container_of(cmd, struct tsif_xfer, hdr);
	tsif_device = xfer->tsif_device;
	i = xfer - tsif_device->xfer;
	data_offset = tsif_device->dmov_cmd[i]->box.dst_row_addr -
		      tsif_device->data_buffer_dma;

	/* order reads from the xferred buffer */
	dma_coherent_post_ops();
	if (result & DMOV_RSLT_DONE) {
		int w = data_offset / TSIF_PKT_SIZE;
		tsif_device->stat_rx++;
		/*
		 * sowtware overflow when I was scheduled?
		 *
		 * @w is where this xfer was actually written to;
		 * @xfer->wi is where device's @wi will be set;
		 *
		 * if these 2 are equal, we are short in space and
		 * going to overwrite this xfer - this is "soft drop"
		 */
		if (w == xfer->wi)
			tsif_device->stat_soft_drop++;
		reschedule = (tsif_device->state == tsif_state_running);
#ifdef CONFIG_TSIF_DEBUG
		/* IFI calculation */
		/*
		 * update stat_ifi (inter frame interval)
		 *
		 * Calculate time difference between last and 1-st
		 * packets in chunk
		 *
		 * To be removed after tuning
		 */
		if (TSIF_PKTS_IN_CHUNK > 1) {
			void *ptr = tsif_device->data_buffer + data_offset;
			u32 *p0 = ptr;
			u32 *p1 = ptr + (TSIF_PKTS_IN_CHUNK - 1) *
				TSIF_PKT_SIZE;
			u32 tts0 = TSIF_STATUS_TTS(tsif_device->stat0 =
						   tsif_pkt_status(p0));
			u32 tts1 = TSIF_STATUS_TTS(tsif_device->stat1 =
						   tsif_pkt_status(p1));
			tsif_device->stat_ifi = (tts1 - tts0) /
				(TSIF_PKTS_IN_CHUNK - 1);
		}
#endif
	} else {
		/**
		 *  Error or flush
		 *
		 *  To recover - re-open TSIF device.
		 */
		/* mark status "not valid" in data buffer */
		int n;
		void *ptr = tsif_device->data_buffer + data_offset;
		for (n = 0; n < TSIF_PKTS_IN_CHUNK; n++) {
			u32 *p = ptr + (n * TSIF_PKT_SIZE);
			/* last dword is status + TTS */
			p[TSIF_PKT_SIZE / sizeof(*p) - 1] = 0;
		}
		if (result & DMOV_RSLT_ERROR) {
			dev_err(&tsif_device->pdev->dev,
				"DMA error (0x%08x)\n", result);
			tsif_device->stat_dmov_err++;
			/* force device close */
			if (tsif_device->state == tsif_state_running) {
				tsif_stop_hw(tsif_device);
				/*
				 * Clocks _may_ be stopped right from IRQ
				 * context. This is far from optimal w.r.t
				 * latency.
				 *
				 * But, this branch taken only in case of
				 * severe hardware problem (I don't even know
				 * what should happens for DMOV_RSLT_ERROR);
				 * thus I prefer code simplicity over
				 * performance.
				 */
				tsif_clock(tsif_device, 0);
				tsif_device->state = tsif_state_flushing;
			}
		}
		if (result & DMOV_RSLT_FLUSH) {
			/*
			 * Flushing normally happens in process of
			 * @tsif_stop(), when we are waiting for outstanding
			 * DMA commands to be flushed.
			 */
			dev_info(&tsif_device->pdev->dev,
				 "DMA channel flushed (0x%08x)\n", result);
			if (tsif_device->state == tsif_state_flushing) {
				if ((!tsif_device->xfer[0].busy) &&
				    (!tsif_device->xfer[1].busy)) {
					tsif_device->state = tsif_state_stopped;
				}
			}
		}
		if (err)
			dev_err(&tsif_device->pdev->dev,
				"Flush data: %08x %08x %08x %08x %08x %08x\n",
				err->flush[0], err->flush[1], err->flush[2],
				err->flush[3], err->flush[4], err->flush[5]);
	}
	tsif_device->wi = xfer->wi;
	xfer->busy = 0;
	if (tsif_device->client_notify)
		tsif_device->client_notify(tsif_device->client_data);
	/*
	 * Can't schedule next DMA -
	 * DataMover driver still hold its semaphore,
	 * deadlock will occur.
	 */
	if (reschedule)
		tasklet_schedule(&tsif_device->dma_refill);
}

/**
 * tsif_dma_refill - tasklet function for tsif_device->dma_refill
 *
 * @data:   tsif_device
 *
 * Reschedule DMA requests
 *
 * Executed in tasklet
 */
static void tsif_dma_refill(unsigned long data)
{
	struct msm_tsif_device *tsif_device = (struct msm_tsif_device *) data;
	if (tsif_device->state == tsif_state_running)
		tsif_dma_schedule(tsif_device);
}

/**
 * tsif_dma_flush - flush DMA channel
 *
 * @tsif_device:
 *
 * busy wait till DMA flushed
 */
static void tsif_dma_flush(struct msm_tsif_device *tsif_device)
{
	if (tsif_device->xfer[0].busy || tsif_device->xfer[1].busy) {
		tsif_device->state = tsif_state_flushing;
		while (tsif_device->xfer[0].busy ||
		       tsif_device->xfer[1].busy) {
			msm_dmov_flush(tsif_device->dma);
			msleep(10);
		}
	}
	tsif_device->state = tsif_state_stopped;
	if (tsif_device->client_notify)
		tsif_device->client_notify(tsif_device->client_data);
}

static void tsif_dma_exit(struct msm_tsif_device *tsif_device)
{
	int i;
	tsif_device->state = tsif_state_flushing;
	tasklet_kill(&tsif_device->dma_refill);
	tsif_dma_flush(tsif_device);
	for (i = 0; i < 2; i++) {
		if (tsif_device->dmov_cmd[i]) {
			dma_free_coherent(NULL, sizeof(struct tsif_dmov_cmd),
					  tsif_device->dmov_cmd[i],
					  tsif_device->dmov_cmd_dma[i]);
			tsif_device->dmov_cmd[i] = NULL;
		}
	}
	if (tsif_device->data_buffer) {
		tsif_device->blob_wrapper_databuf.data = NULL;
		tsif_device->blob_wrapper_databuf.size = 0;
		dma_free_coherent(NULL, TSIF_BUF_SIZE,
				  tsif_device->data_buffer,
				  tsif_device->data_buffer_dma);
		tsif_device->data_buffer = NULL;
	}
}

static int tsif_dma_init(struct msm_tsif_device *tsif_device)
{
	int i;
	/* TODO: allocate all DMA memory in one buffer */
	/* Note: don't pass device,
	   it require coherent_dma_mask id device definition */
	tsif_device->data_buffer = dma_alloc_coherent(NULL, TSIF_BUF_SIZE,
				&tsif_device->data_buffer_dma, GFP_KERNEL);
	if (!tsif_device->data_buffer)
		goto err;
	dev_info(&tsif_device->pdev->dev, "data_buffer: %p phys 0x%08x\n",
		 tsif_device->data_buffer, tsif_device->data_buffer_dma);
	tsif_device->blob_wrapper_databuf.data = tsif_device->data_buffer;
	tsif_device->blob_wrapper_databuf.size = TSIF_BUF_SIZE;
	tsif_device->ri = 0;
	tsif_device->wi = 0;
	tsif_device->dmwi = 0;
	for (i = 0; i < 2; i++) {
		dmov_box *box;
		struct msm_dmov_cmd *hdr;
		tsif_device->dmov_cmd[i] = dma_alloc_coherent(NULL,
			sizeof(struct tsif_dmov_cmd),
			&tsif_device->dmov_cmd_dma[i], GFP_KERNEL);
		if (!tsif_device->dmov_cmd[i])
			goto err;
		dev_info(&tsif_device->pdev->dev, "dma[%i]: %p phys 0x%08x\n",
			 i, tsif_device->dmov_cmd[i],
			 tsif_device->dmov_cmd_dma[i]);
		/* dst in 16 LSB, src in 16 MSB */
		box = &(tsif_device->dmov_cmd[i]->box);
		box->cmd = CMD_MODE_BOX | CMD_LC |
			   CMD_SRC_CRCI(tsif_device->crci);
		box->src_row_addr =
			tsif_device->memres->start + TSIF_DATA_PORT_OFF;
		box->src_dst_len = (TSIF_PKT_SIZE << 16) | TSIF_PKT_SIZE;
		box->num_rows = (TSIF_PKTS_IN_CHUNK << 16) | TSIF_PKTS_IN_CHUNK;
		box->row_offset = (0 << 16) | TSIF_PKT_SIZE;

		tsif_device->dmov_cmd[i]->box_ptr = CMD_PTR_LP |
			DMOV_CMD_ADDR(tsif_device->dmov_cmd_dma[i] +
				      offsetof(struct tsif_dmov_cmd, box));
		tsif_device->xfer[i].tsif_device = tsif_device;
		hdr = &tsif_device->xfer[i].hdr;
		hdr->cmdptr = DMOV_CMD_ADDR(tsif_device->dmov_cmd_dma[i] +
			      offsetof(struct tsif_dmov_cmd, box_ptr));
		hdr->complete_func = tsif_dmov_complete_func;
	}
	msm_dmov_flush(tsif_device->dma);
	return 0;
err:
	dev_err(&tsif_device->pdev->dev, "Failed to allocate DMA buffers\n");
	tsif_dma_exit(tsif_device);
	return -ENOMEM;
}

/* ===DMA end=== */

/* ===IRQ begin=== */

static irqreturn_t tsif_irq(int irq, void *dev_id)
{
	struct msm_tsif_device *tsif_device = dev_id;
	u32 sts_ctl = ioread32(tsif_device->base + TSIF_STS_CTL_OFF);
	if (!(sts_ctl & (TSIF_STS_CTL_PACK_AVAIL |
			 TSIF_STS_CTL_OVERFLOW |
			 TSIF_STS_CTL_LOST_SYNC |
			 TSIF_STS_CTL_TIMEOUT))) {
		dev_warn(&tsif_device->pdev->dev, "Spurious interrupt\n");
		return IRQ_NONE;
	}
	if (sts_ctl & TSIF_STS_CTL_PACK_AVAIL) {
		dev_info(&tsif_device->pdev->dev, "TSIF IRQ: PACK_AVAIL\n");
		tsif_device->stat_rx++;
	}
	if (sts_ctl & TSIF_STS_CTL_OVERFLOW) {
		dev_info(&tsif_device->pdev->dev, "TSIF IRQ: OVERFLOW\n");
		tsif_device->stat_overflow++;
	}
	if (sts_ctl & TSIF_STS_CTL_LOST_SYNC) {
		dev_info(&tsif_device->pdev->dev, "TSIF IRQ: LOST SYNC\n");
		tsif_device->stat_lost_sync++;
	}
	if (sts_ctl & TSIF_STS_CTL_TIMEOUT) {
		dev_info(&tsif_device->pdev->dev, "TSIF IRQ: TIMEOUT\n");
		tsif_device->stat_timeout++;
	}
	iowrite32(sts_ctl, tsif_device->base + TSIF_STS_CTL_OFF);
	wmb();
	return IRQ_HANDLED;
}

/* ===IRQ end=== */

/* ===Device attributes begin=== */

static ssize_t show_stats(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	struct msm_tsif_device *tsif_device = dev_get_drvdata(dev);
	char *state_string;
	switch (tsif_device->state) {
	case tsif_state_stopped:
		state_string = "stopped";
		break;
	case tsif_state_running:
		state_string = "running";
		break;
	case tsif_state_flushing:
		state_string = "flushing";
		break;
	default:
		state_string = "???";
	}
	return snprintf(buf, PAGE_SIZE,
			"Device       %s\n"
			"Mode       = %d\n"
			"Time limit = %d\n"
			"State        %s\n"
			"Client     = %p\n"
			"Pkt/Buf    = %d\n"
			"Pkt/chunk  = %d\n"
			"--statistics--\n"
			"Rx chunks  = %d\n"
			"Overflow   = %d\n"
			"Lost sync  = %d\n"
			"Timeout    = %d\n"
			"DMA error  = %d\n"
			"Soft drop  = %d\n"
			"IFI        = %d\n"
			"(0x%08x - 0x%08x) / %d\n"
			"--debug--\n"
			"GLBL_CLK_ENA     = 0x%08x\n"
			"ROW_RESET        = 0x%08x\n"
			"CLK_HALT_STATEB  = 0x%08x\n"
			"TV_NS_REG        = 0x%08x\n"
			"TSIF_NS_REG      = 0x%08x\n",
			dev_name(dev),
			tsif_device->mode,
			tsif_device->time_limit,
			state_string,
			tsif_device->client_data,
			TSIF_PKTS_IN_BUF,
			TSIF_PKTS_IN_CHUNK,
			tsif_device->stat_rx,
			tsif_device->stat_overflow,
			tsif_device->stat_lost_sync,
			tsif_device->stat_timeout,
			tsif_device->stat_dmov_err,
			tsif_device->stat_soft_drop,
			tsif_device->stat_ifi,
			tsif_device->stat1,
			tsif_device->stat0,
			TSIF_PKTS_IN_CHUNK - 1,
			ioread32(GLBL_CLK_ENA),
			ioread32(ROW_RESET),
			ioread32(CLK_HALT_STATEB),
			ioread32(TV_NS_REG),
			ioread32(TSIF_NS_REG)
			);
}
/**
 * set_stats - reset statistics on write
 *
 * @dev:
 * @attr:
 * @buf:
 * @count:
 */
static ssize_t set_stats(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	struct msm_tsif_device *tsif_device = dev_get_drvdata(dev);
	tsif_device->stat_rx = 0;
	tsif_device->stat_overflow = 0;
	tsif_device->stat_lost_sync = 0;
	tsif_device->stat_timeout = 0;
	tsif_device->stat_dmov_err = 0;
	tsif_device->stat_soft_drop = 0;
	tsif_device->stat_ifi = 0;
	return count;
}
static DEVICE_ATTR(stats, S_IRUGO | S_IWUSR, show_stats, set_stats);

static ssize_t show_mode(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct msm_tsif_device *tsif_device = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%d\n", tsif_device->mode);
}

static ssize_t set_mode(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct msm_tsif_device *tsif_device = dev_get_drvdata(dev);
	int value;
	int rc;
	if (1 != sscanf(buf, "%d", &value)) {
		dev_err(&tsif_device->pdev->dev,
			"Failed to parse integer: <%s>\n", buf);
		return -EINVAL;
	}
	rc = tsif_set_mode(tsif_device, value);
	if (!rc)
		rc = count;
	return rc;
}
static DEVICE_ATTR(mode, S_IRUGO | S_IWUSR, show_mode, set_mode);

static ssize_t show_time_limit(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct msm_tsif_device *tsif_device = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%d\n", tsif_device->time_limit);
}

static ssize_t set_time_limit(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct msm_tsif_device *tsif_device = dev_get_drvdata(dev);
	int value;
	int rc;
	if (1 != sscanf(buf, "%d", &value)) {
		dev_err(&tsif_device->pdev->dev,
			"Failed to parse integer: <%s>\n", buf);
		return -EINVAL;
	}
	rc = tsif_set_time_limit(tsif_device, value);
	if (!rc)
		rc = count;
	return rc;
}
static DEVICE_ATTR(time_limit, S_IRUGO | S_IWUSR,
		   show_time_limit, set_time_limit);

static ssize_t show_buf_config(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct msm_tsif_device *tsif_device = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%d * %d\n",
			tsif_device->pkts_per_chunk,
			tsif_device->chunks_per_buf);
}

static ssize_t set_buf_config(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct msm_tsif_device *tsif_device = dev_get_drvdata(dev);
	u32 p, c;
	int rc;
	if (2 != sscanf(buf, "%d * %d", &p, &c)) {
		dev_err(&tsif_device->pdev->dev,
			"Failed to parse integer: <%s>\n", buf);
		return -EINVAL;
	}
	rc = tsif_set_buf_config(tsif_device, p, c);
	if (!rc)
		rc = count;
	return rc;
}
static DEVICE_ATTR(buf_config, S_IRUGO | S_IWUSR,
		   show_buf_config, set_buf_config);

static struct attribute *dev_attrs[] = {
	&dev_attr_stats.attr,
	&dev_attr_mode.attr,
	&dev_attr_time_limit.attr,
	&dev_attr_buf_config.attr,
	NULL,
};
static struct attribute_group dev_attr_grp = {
	.attrs = dev_attrs,
};
/* ===Device attributes end=== */

/* ===debugfs begin=== */

static int debugfs_iomem_x32_set(void *data, u64 val)
{
	iowrite32(val, data);
	wmb();
	return 0;
}

static int debugfs_iomem_x32_get(void *data, u64 *val)
{
	*val = ioread32(data);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(fops_iomem_x32, debugfs_iomem_x32_get,
			debugfs_iomem_x32_set, "0x%08llx\n");

struct dentry *debugfs_create_iomem_x32(const char *name, mode_t mode,
					struct dentry *parent, u32 *value)
{
	return debugfs_create_file(name, mode, parent, value, &fops_iomem_x32);
}

static int action_open(struct msm_tsif_device *tsif_device)
{
	int rc = -EINVAL;
	int result;

	struct msm_tsif_platform_data *pdata =
		tsif_device->pdev->dev.platform_data;
	dev_info(&tsif_device->pdev->dev, "%s\n", __func__);
	if (tsif_device->state != tsif_state_stopped)
		return -EAGAIN;
	rc = tsif_dma_init(tsif_device);
	if (rc) {
		dev_err(&tsif_device->pdev->dev, "failed to init DMA\n");
		return rc;
	}
	tsif_device->state = tsif_state_running;
	/*
	 * DMA should be scheduled prior to TSIF hardware initialization,
	 * otherwise "bus error" will be reported by Data Mover
	 */
	enable_irq(tsif_device->irq);
	tsif_clock(tsif_device, 1);
	tsif_dma_schedule(tsif_device);
	/*
	 * init the device if required
	 */
	if (pdata->init)
		pdata->init(pdata);
	rc = tsif_start_hw(tsif_device);
	if (rc) {
		dev_err(&tsif_device->pdev->dev, "Unable to start HW\n");
		tsif_dma_exit(tsif_device);
		tsif_clock(tsif_device, 0);
		return rc;
	}

	result = pm_runtime_get(&tsif_device->pdev->dev);
	if (result < 0) {
		dev_err(&tsif_device->pdev->dev,
			"Runtime PM: Unable to wake up the device, rc = %d\n",
			result);
		return result;
	}

	wake_lock(&tsif_device->wake_lock);
	return rc;
}

static int action_close(struct msm_tsif_device *tsif_device)
{
	dev_info(&tsif_device->pdev->dev, "%s, state %d\n", __func__,
		 (int)tsif_device->state);
	/*
	 * DMA should be flushed/stopped prior to TSIF hardware stop,
	 * otherwise "bus error" will be reported by Data Mover
	 */
	tsif_stop_hw(tsif_device);
	tsif_dma_exit(tsif_device);
	tsif_clock(tsif_device, 0);
	disable_irq(tsif_device->irq);

	pm_runtime_put(&tsif_device->pdev->dev);
	wake_unlock(&tsif_device->wake_lock);
	return 0;
}


static struct {
	int (*func)(struct msm_tsif_device *);
	const char *name;
} actions[] = {
	{ action_open,  "open"},
	{ action_close, "close"},
};

static ssize_t tsif_debugfs_action_write(struct file *filp,
					 const char __user *userbuf,
					 size_t count, loff_t *f_pos)
{
	int i;
	struct msm_tsif_device *tsif_device = filp->private_data;
	char s[40];
	int len = min(sizeof(s) - 1, count);
	if (copy_from_user(s, userbuf, len))
		return -EFAULT;
	s[len] = '\0';
	dev_info(&tsif_device->pdev->dev, "%s:%s\n", __func__, s);
	for (i = 0; i < ARRAY_SIZE(actions); i++) {
		if (!strncmp(s, actions[i].name,
		    min(count, strlen(actions[i].name)))) {
			int rc = actions[i].func(tsif_device);
			if (!rc)
				rc = count;
			return rc;
		}
	}
	return -EINVAL;
}

static int tsif_debugfs_generic_open(struct inode *inode, struct file *filp)
{
	filp->private_data = inode->i_private;
	return 0;
}

static const struct file_operations fops_debugfs_action = {
	.open  = tsif_debugfs_generic_open,
	.write = tsif_debugfs_action_write,
};

static ssize_t tsif_debugfs_dma_read(struct file *filp, char __user *userbuf,
				     size_t count, loff_t *f_pos)
{
	static char bufa[200];
	static char *buf = bufa;
	int sz = sizeof(bufa);
	struct msm_tsif_device *tsif_device = filp->private_data;
	int len = 0;
	if (tsif_device) {
		int i;
		len += snprintf(buf + len, sz - len,
				"ri %3d | wi %3d | dmwi %3d |",
				tsif_device->ri, tsif_device->wi,
				tsif_device->dmwi);
		for (i = 0; i < 2; i++) {
			struct tsif_xfer *xfer = &tsif_device->xfer[i];
			if (xfer->busy) {
				u32 dst =
				    tsif_device->dmov_cmd[i]->box.dst_row_addr;
				u32 base = tsif_device->data_buffer_dma;
				int w = (dst - base) / TSIF_PKT_SIZE;
				len += snprintf(buf + len, sz - len,
						" [%3d]{%3d}",
						w, xfer->wi);
			} else {
				len += snprintf(buf + len, sz - len,
						" ---idle---");
			}
		}
			len += snprintf(buf + len, sz - len, "\n");
	} else {
		len += snprintf(buf + len, sz - len, "No TSIF device???\n");
	}
	return simple_read_from_buffer(userbuf, count, f_pos, buf, len);
}

static const struct file_operations fops_debugfs_dma = {
	.open = tsif_debugfs_generic_open,
	.read = tsif_debugfs_dma_read,
};

static ssize_t tsif_debugfs_gpios_read(struct file *filp, char __user *userbuf,
				       size_t count, loff_t *f_pos)
{
	static char bufa[300];
	static char *buf = bufa;
	int sz = sizeof(bufa);
	struct msm_tsif_device *tsif_device = filp->private_data;
	int len = 0;
	if (tsif_device) {
		struct msm_tsif_platform_data *pdata =
			tsif_device->pdev->dev.platform_data;
		int i;
		for (i = 0; i < pdata->num_gpios; i++) {
			if (pdata->gpios[i].gpio_cfg) {
				int x = !!gpio_get_value(GPIO_PIN(
					pdata->gpios[i].gpio_cfg));
				len += snprintf(buf + len, sz - len,
						"%15s: %d\n",
						pdata->gpios[i].label, x);
			}
		}
	} else {
		len += snprintf(buf + len, sz - len, "No TSIF device???\n");
	}
	return simple_read_from_buffer(userbuf, count, f_pos, buf, len);
}

static const struct file_operations fops_debugfs_gpios = {
	.open = tsif_debugfs_generic_open,
	.read = tsif_debugfs_gpios_read,
};


static void tsif_debugfs_init(struct msm_tsif_device *tsif_device)
{
	tsif_device->dent_tsif = debugfs_create_dir(
	      dev_name(&tsif_device->pdev->dev), NULL);
	if (tsif_device->dent_tsif) {
		int i;
		void __iomem *base = tsif_device->base;
		for (i = 0; i < ARRAY_SIZE(debugfs_tsif_regs); i++) {
			tsif_device->debugfs_tsif_regs[i] =
			   debugfs_create_iomem_x32(
				debugfs_tsif_regs[i].name,
				debugfs_tsif_regs[i].mode,
				tsif_device->dent_tsif,
				base + debugfs_tsif_regs[i].offset);
		}
		tsif_device->debugfs_gpio = debugfs_create_file("gpios",
		    S_IRUGO,
		    tsif_device->dent_tsif, tsif_device, &fops_debugfs_gpios);
		tsif_device->debugfs_action = debugfs_create_file("action",
		    S_IWUSR,
		    tsif_device->dent_tsif, tsif_device, &fops_debugfs_action);
		tsif_device->debugfs_dma = debugfs_create_file("dma",
		    S_IRUGO,
		    tsif_device->dent_tsif, tsif_device, &fops_debugfs_dma);
		tsif_device->debugfs_databuf = debugfs_create_blob("data_buf",
		    S_IRUGO,
		    tsif_device->dent_tsif, &tsif_device->blob_wrapper_databuf);
	}
}

static void tsif_debugfs_exit(struct msm_tsif_device *tsif_device)
{
	if (tsif_device->dent_tsif) {
		int i;
		debugfs_remove_recursive(tsif_device->dent_tsif);
		tsif_device->dent_tsif = NULL;
		for (i = 0; i < ARRAY_SIZE(debugfs_tsif_regs); i++)
			tsif_device->debugfs_tsif_regs[i] = NULL;
		tsif_device->debugfs_gpio = NULL;
		tsif_device->debugfs_action = NULL;
		tsif_device->debugfs_dma = NULL;
		tsif_device->debugfs_databuf = NULL;
	}
}
/* ===debugfs end=== */

/* ===module begin=== */
static LIST_HEAD(tsif_devices);

static struct msm_tsif_device *tsif_find_by_id(int id)
{
	struct msm_tsif_device *tsif_device;
	list_for_each_entry(tsif_device, &tsif_devices, devlist) {
		if (tsif_device->pdev->id == id)
			return tsif_device;
	}
	return NULL;
}

static int __devinit msm_tsif_probe(struct platform_device *pdev)
{
	int rc = -ENODEV;
	struct msm_tsif_platform_data *plat = pdev->dev.platform_data;
	struct msm_tsif_device *tsif_device;
	struct resource *res;
	/* check device validity */
	/* must have platform data */
	if (!plat) {
		dev_err(&pdev->dev, "Platform data not available\n");
		rc = -EINVAL;
		goto out;
	}

	if ((pdev->id < 0) || (pdev->id > TSIF_MAX_ID)) {
		dev_err(&pdev->dev, "Invalid device ID %d\n", pdev->id);
		rc = -EINVAL;
		goto out;
	}
	/* OK, we will use this device */
	tsif_device = kzalloc(sizeof(struct msm_tsif_device), GFP_KERNEL);
	if (!tsif_device) {
		dev_err(&pdev->dev, "Failed to allocate memory for device\n");
		rc = -ENOMEM;
		goto out;
	}
	/* cross links */
	tsif_device->pdev = pdev;
	platform_set_drvdata(pdev, tsif_device);
	tsif_device->mode = 1;
	tsif_device->pkts_per_chunk = TSIF_PKTS_IN_CHUNK_DEFAULT;
	tsif_device->chunks_per_buf = TSIF_CHUNKS_IN_BUF_DEFAULT;
	tasklet_init(&tsif_device->dma_refill, tsif_dma_refill,
		     (unsigned long)tsif_device);
	if (tsif_get_clocks(tsif_device))
		goto err_clocks;
/* map I/O memory */
	tsif_device->memres = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!tsif_device->memres) {
		dev_err(&pdev->dev, "Missing MEM resource\n");
		rc = -ENXIO;
		goto err_rgn;
	}
	res = platform_get_resource(pdev, IORESOURCE_DMA, 0);
	if (!res) {
		dev_err(&pdev->dev, "Missing DMA resource\n");
		rc = -ENXIO;
		goto err_rgn;
	}
	tsif_device->dma = res->start;
	tsif_device->crci = res->end;
	tsif_device->base = ioremap(tsif_device->memres->start,
				    resource_size(tsif_device->memres));
	if (!tsif_device->base) {
		dev_err(&pdev->dev, "ioremap failed\n");
		goto err_ioremap;
	}
	dev_info(&pdev->dev, "remapped phys 0x%08x => virt %p\n",
		 tsif_device->memres->start, tsif_device->base);
	rc = tsif_start_gpios(tsif_device);
	if (rc)
		goto err_gpio;

	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	tsif_debugfs_init(tsif_device);
	rc = platform_get_irq(pdev, 0);
	if (rc > 0) {
		tsif_device->irq = rc;
		rc = request_irq(tsif_device->irq, tsif_irq, IRQF_SHARED,
				 dev_name(&pdev->dev), tsif_device);
		disable_irq(tsif_device->irq);
	}
	if (rc) {
		dev_err(&pdev->dev, "failed to request IRQ %d : %d\n",
			tsif_device->irq, rc);
		goto err_irq;
	}
	rc = sysfs_create_group(&pdev->dev.kobj, &dev_attr_grp);
	if (rc) {
		dev_err(&pdev->dev, "failed to create dev. attrs : %d\n", rc);
		goto err_attrs;
	}
	wake_lock_init(&tsif_device->wake_lock, WAKE_LOCK_SUSPEND,
		       dev_name(&pdev->dev));
	dev_info(&pdev->dev, "Configured irq %d memory 0x%08x DMA %d CRCI %d\n",
		 tsif_device->irq, tsif_device->memres->start,
		 tsif_device->dma, tsif_device->crci);
	list_add(&tsif_device->devlist, &tsif_devices);
	return 0;
/* error path */
	sysfs_remove_group(&pdev->dev.kobj, &dev_attr_grp);
err_attrs:
	free_irq(tsif_device->irq, tsif_device);
err_irq:
	tsif_debugfs_exit(tsif_device);
	tsif_stop_gpios(tsif_device);
err_gpio:
	iounmap(tsif_device->base);
err_ioremap:
err_rgn:
	tsif_put_clocks(tsif_device);
err_clocks:
	kfree(tsif_device);
out:
	return rc;
}

static int __devexit msm_tsif_remove(struct platform_device *pdev)
{
	struct msm_tsif_device *tsif_device = platform_get_drvdata(pdev);
	dev_info(&pdev->dev, "Unload\n");
	list_del(&tsif_device->devlist);
	wake_lock_destroy(&tsif_device->wake_lock);
	sysfs_remove_group(&pdev->dev.kobj, &dev_attr_grp);
	free_irq(tsif_device->irq, tsif_device);
	tsif_debugfs_exit(tsif_device);
	tsif_dma_exit(tsif_device);
	tsif_stop_gpios(tsif_device);
	iounmap(tsif_device->base);
	tsif_put_clocks(tsif_device);

	pm_runtime_put(&pdev->dev);
	pm_runtime_disable(&pdev->dev);
	kfree(tsif_device);
	return 0;
}

static int tsif_runtime_suspend(struct device *dev)
{
	dev_dbg(dev, "pm_runtime: suspending...\n");
	return 0;
}

static int tsif_runtime_resume(struct device *dev)
{
	dev_dbg(dev, "pm_runtime: resuming...\n");
	return 0;
}

static const struct dev_pm_ops tsif_dev_pm_ops = {
	.runtime_suspend = tsif_runtime_suspend,
	.runtime_resume = tsif_runtime_resume,
};


static struct platform_driver msm_tsif_driver = {
	.probe          = msm_tsif_probe,
	.remove         = __exit_p(msm_tsif_remove),
	.driver         = {
		.name   = "msm_tsif",
		.pm     = &tsif_dev_pm_ops,
	},
};

static int __init mod_init(void)
{
	int rc = platform_driver_register(&msm_tsif_driver);
	if (rc)
		pr_err("TSIF: platform_driver_register failed: %d\n", rc);
	return rc;
}

static void __exit mod_exit(void)
{
	platform_driver_unregister(&msm_tsif_driver);
}
/* ===module end=== */

/* public API */

int tsif_get_active(void)
{
	struct msm_tsif_device *tsif_device;
	list_for_each_entry(tsif_device, &tsif_devices, devlist) {
		return tsif_device->pdev->id;
	}
	return -ENODEV;
}
EXPORT_SYMBOL(tsif_get_active);

void *tsif_attach(int id, void (*notify)(void *client_data), void *data)
{
	struct msm_tsif_device *tsif_device = tsif_find_by_id(id);
	if (!tsif_device)
		return ERR_PTR(-ENODEV);
	if (tsif_device->client_notify || tsif_device->client_data)
		return ERR_PTR(-EBUSY);
	tsif_device->client_notify = notify;
	tsif_device->client_data = data;
	/* prevent from unloading */
	get_device(&tsif_device->pdev->dev);
	return tsif_device;
}
EXPORT_SYMBOL(tsif_attach);

void tsif_detach(void *cookie)
{
	struct msm_tsif_device *tsif_device = cookie;
	tsif_device->client_notify = NULL;
	tsif_device->client_data = NULL;
	put_device(&tsif_device->pdev->dev);
}
EXPORT_SYMBOL(tsif_detach);

void tsif_get_info(void *cookie, void **pdata, int *psize)
{
	struct msm_tsif_device *tsif_device = cookie;
	if (pdata)
		*pdata = tsif_device->data_buffer;
	if (psize)
		*psize = TSIF_PKTS_IN_BUF;
}
EXPORT_SYMBOL(tsif_get_info);

int tsif_set_mode(void *cookie, int mode)
{
	struct msm_tsif_device *tsif_device = cookie;
	if (tsif_device->state != tsif_state_stopped) {
		dev_err(&tsif_device->pdev->dev,
			"Can't change mode while device is active\n");
		return -EBUSY;
	}
	switch (mode) {
	case 1:
	case 2:
	case 3:
		tsif_device->mode = mode;
		break;
	default:
		dev_err(&tsif_device->pdev->dev, "Invalid mode: %d\n", mode);
		return -EINVAL;
	}
	return 0;
}
EXPORT_SYMBOL(tsif_set_mode);

int tsif_set_time_limit(void *cookie, u32 value)
{
	struct msm_tsif_device *tsif_device = cookie;
	if (tsif_device->state != tsif_state_stopped) {
		dev_err(&tsif_device->pdev->dev,
			"Can't change time limit while device is active\n");
		return -EBUSY;
	}
	if (value != (value & 0xFFFFFF)) {
		dev_err(&tsif_device->pdev->dev,
			"Invalid time limit (should be 24 bit): %#x\n", value);
		return -EINVAL;
	}
	tsif_device->time_limit = value;
	return 0;
}
EXPORT_SYMBOL(tsif_set_time_limit);

int tsif_set_buf_config(void *cookie, u32 pkts_in_chunk, u32 chunks_in_buf)
{
	struct msm_tsif_device *tsif_device = cookie;
	if (tsif_device->data_buffer) {
		dev_err(&tsif_device->pdev->dev,
			"Data buffer already allocated: %p\n",
			tsif_device->data_buffer);
		return -EBUSY;
	}
	/* check for crazy user */
	if (pkts_in_chunk * chunks_in_buf > 10240) {
		dev_err(&tsif_device->pdev->dev,
			"Buffer requested is too large: %d * %d\n",
			pkts_in_chunk,
			chunks_in_buf);
		return -EINVAL;
	}
	/* parameters are OK, execute */
	tsif_device->pkts_per_chunk = pkts_in_chunk;
	tsif_device->chunks_per_buf = chunks_in_buf;
	return 0;
}
EXPORT_SYMBOL(tsif_set_buf_config);

void tsif_get_state(void *cookie, int *ri, int *wi, enum tsif_state *state)
{
	struct msm_tsif_device *tsif_device = cookie;
	if (ri)
		*ri    = tsif_device->ri;
	if (wi)
		*wi    = tsif_device->wi;
	if (state)
		*state = tsif_device->state;
}
EXPORT_SYMBOL(tsif_get_state);

int tsif_start(void *cookie)
{
	struct msm_tsif_device *tsif_device = cookie;
	return action_open(tsif_device);
}
EXPORT_SYMBOL(tsif_start);

void tsif_stop(void *cookie)
{
	struct msm_tsif_device *tsif_device = cookie;
	action_close(tsif_device);
}
EXPORT_SYMBOL(tsif_stop);

void tsif_reclaim_packets(void *cookie, int read_index)
{
	struct msm_tsif_device *tsif_device = cookie;
	tsif_device->ri = read_index;
}
EXPORT_SYMBOL(tsif_reclaim_packets);

module_init(mod_init);
module_exit(mod_exit);

MODULE_DESCRIPTION("TSIF (Transport Stream Interface)"
		   " Driver for the MSM chipset");
MODULE_LICENSE("GPL v2");

