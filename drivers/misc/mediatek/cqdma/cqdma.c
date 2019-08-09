/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/miscdevice.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/slab.h>
#ifdef CONFIG_MTK_GIC
#include <linux/irqchip/mt-gic.h>
#endif

#ifdef CONFIG_PM_WAKELOCKS
#include <linux/pm_wakeup.h>
#else
#include <linux/wakelock.h>
#endif

#include <mt-plat/mtk_io.h>
#include <mt-plat/dma.h>
#include <mt-plat/sync_write.h>
#include <mt-plat/mtk_lpae.h>
#include <linux/clk.h>

struct clk *clk_cqdma;

struct cqdma_env_info {
	void __iomem *base;
	u32 irq;
};
static struct cqdma_env_info *env_info;
static u32 keep_clock_ao;
static u32 nr_cqdma_channel;
#ifdef CONFIG_PM_WAKELOCKS
struct wakeup_source *wk_lock;
#else
struct wake_lock *wk_lock;
#endif

/*
 * DMA information
 */

#define GDMA_START          (0)

/*
 * General DMA channel register mapping
 */
#define DMA_INT_FLAG(ch)           IOMEM((env_info[ch].base + 0x0000))
#define DMA_INT_EN(ch)             IOMEM((env_info[ch].base + 0x0004))
#define DMA_START(ch)              IOMEM((env_info[ch].base + 0x0008))
#define DMA_RESET(ch)              IOMEM((env_info[ch].base + 0x000C))
#define DMA_STOP(ch)               IOMEM((env_info[ch].base + 0x0010))
#define DMA_FLUSH(ch)              IOMEM((env_info[ch].base + 0x0014))
#define DMA_CON(ch)                IOMEM((env_info[ch].base + 0x0018))
#define DMA_SRC(ch)                IOMEM((env_info[ch].base + 0x001C))
#define DMA_DST(ch)                IOMEM((env_info[ch].base + 0x0020))
#define DMA_LEN1(ch)               IOMEM((env_info[ch].base + 0x0024))
#define DMA_LEN2(ch)               IOMEM((env_info[ch].base + 0x0028))
#define DMA_JUMP_ADDR(ch)          IOMEM((env_info[ch].base + 0x002C))
#define DMA_IBUFF_SIZE(ch)         IOMEM((env_info[ch].base + 0x0030))
#define DMA_CONNECT(ch)            IOMEM((env_info[ch].base + 0x0034))
#define DMA_AXIATTR(ch)            IOMEM((env_info[ch].base + 0x0038))
#define DMA_DBG_STAT(ch)           IOMEM((env_info[ch].base + 0x0050))

#if defined(CONFIG_MACH_MT6799) || defined(CONFIG_MACH_MT6763) ||\
defined(CONFIG_MACH_MT6779)
#define DMA_VIO_DBG1(ch)           IOMEM((env_info[ch].base + 0x0040))
#define DMA_VIO_DBG(ch)            IOMEM((env_info[ch].base + 0x0044))
#else
#define DMA_VIO_DBG1(ch)           IOMEM((env_info[ch].base + 0x003c))
#define DMA_VIO_DBG(ch)            IOMEM((env_info[ch].base + 0x0060))
#endif

/*Everest,Elbrus,whitney:0x60,0x64,0x68*/
#if defined(CONFIG_ARCH_MT6797) ||\
defined(CONFIG_MACH_MT6799) || defined(CONFIG_MACH_MT6763) ||\
defined(CONFIG_MACH_MT6779)
#define DMA_SRC_4G_SUPPORT(ch)     IOMEM((env_info[ch].base + 0x0060))
#define DMA_DST_4G_SUPPORT(ch)     IOMEM((env_info[ch].base + 0x0064))
#define DMA_JUMP_4G_SUPPORT(ch)    IOMEM((env_info[ch].base + 0x0068))

#elif defined(CONFIG_ARCH_MT6752)
/*k2:0x40,0x44,0x48*/
#define DMA_SRC_4G_SUPPORT(ch)     IOMEM((env_info[ch].base + 0x0040))
#define DMA_DST_4G_SUPPORT(ch)     IOMEM((env_info[ch].base + 0x0044))
#define DMA_JUMP_4G_SUPPORT(ch)    IOMEM((env_info[ch].base + 0x0048))
#else
/*jade ,Olympus,KIBO:0xe0,0xe4,0xe8*//*denali,Rainier:N/A*/
#define DMA_SRC_4G_SUPPORT(ch)     IOMEM((env_info[ch].base + 0x00E0))
#define DMA_DST_4G_SUPPORT(ch)     IOMEM((env_info[ch].base + 0x00E4))
#define DMA_JUMP_4G_SUPPORT(ch)    IOMEM((env_info[ch].base + 0x00E8))
#endif



/*
 * Register Setting
 */
#define DMA_GDMA_LEN_MAX_MASK   (0x000FFFFF)

#define DMA_CON_DIR             (0x00000001)
#define DMA_CON_FPEN            (0x00000002)	/* Use fix pattern. */
#define DMA_CON_SLOW_EN         (0x00000004)
#define DMA_CON_DFIX            (0x00000008)
#define DMA_CON_SFIX            (0x00000010)
#define DMA_CON_WPEN            (0x00008000)
#define DMA_CON_WPSD            (0x00100000)
#define DMA_CON_WSIZE_1BYTE     (0x00000000)
#define DMA_CON_WSIZE_2BYTE     (0x01000000)
#define DMA_CON_WSIZE_4BYTE     (0x02000000)
#define DMA_CON_RSIZE_1BYTE     (0x00000000)
#define DMA_CON_RSIZE_2BYTE     (0x10000000)
#define DMA_CON_RSIZE_4BYTE     (0x20000000)
#define DMA_CON_BURST_MASK      (0x00070000)
#define DMA_CON_SLOW_OFFSET     (5)
#define DMA_CON_SLOW_MAX_MASK   (0x000003FF)

#define DMA_START_BIT           (0x00000001)
#define DMA_STOP_BIT            (0x00000000)
#define DMA_INT_FLAG_BIT        (0x00000001)
#define DMA_INT_FLAG_CLR_BIT    (0x00000000)
#define DMA_INT_EN_BIT          (0x00000001)
#define DMA_FLUSH_BIT           (0x00000001)
#define DMA_FLUSH_CLR_BIT       (0x00000000)
#define DMA_UART_RX_INT_EN_BIT  (0x00000003)
#define DMA_INT_EN_CLR_BIT      (0x00000000)
#define DMA_WARM_RST_BIT        (0x00000001)
#define DMA_HARD_RST_BIT        (0x00000002)
#define DMA_HARD_RST_CLR_BIT    (0x00000000)
#define DMA_READ_COHER_BIT      (0x00000010)
#define DMA_WRITE_COHER_BIT     (0x00100000)
#define DMA_ADDR2_EN_BIT        (0x00000001)

/*
 * Register Limitation
 */
#define MAX_TRANSFER_LEN1   (0xFFFFF)
#define MAX_TRANSFER_LEN2   (0xFFFFF)
#define MAX_SLOW_DOWN_CNTER (0x3FF)

/*
 * channel information structures
 */

struct dma_ctrl {
	int in_use;
	void (*isr_cb)(void *);
	void *data;
};

/*
 * global variables
 */

#define CQDMA_MAX_CHANNEL			(8)

static struct dma_ctrl dma_ctrl[CQDMA_MAX_CHANNEL];
static DEFINE_SPINLOCK(dma_drv_lock);

#define PDN_APDMA_MODULE_NAME ("CQDMA")
#define GDMA_WARM_RST_TIMEOUT   (100)	/* ms */

/*
 * mt_req_gdma: request a general DMA.
 * @chan: specify a channel or not
 * Return channel number for success; return negative errot code for failure.
 */
int mt_req_gdma(int chan)
{
	unsigned long flags;
	int i;

	if (clk_cqdma && !keep_clock_ao) {
		if (clk_prepare_enable(clk_cqdma)) {
			pr_err("enable CQDMA clk fail!\n");
			return -DMA_ERR_NO_FREE_CH;
		}
	}

	spin_lock_irqsave(&dma_drv_lock, flags);

	if (chan == GDMA_ANY) {
		for (i = GDMA_START; i < nr_cqdma_channel; i++) {
			if (dma_ctrl[i].in_use)
				continue;
			else {
				dma_ctrl[i].in_use = 1;
#ifdef CONFIG_PM_WAKELOCKS
				__pm_stay_awake(&wk_lock[i]);
#else
				wake_lock(&wk_lock[i]);
#endif
				break;
			}
		}
	} else {
		if (dma_ctrl[chan].in_use)
			i = nr_cqdma_channel;
		else {
			i = chan;
			dma_ctrl[chan].in_use = 1;
#ifdef CONFIG_PM_WAKELOCKS
			__pm_stay_awake(&wk_lock[chan]);
#else
			wake_lock(&wk_lock[chan]);
#endif
		}
	}

	spin_unlock_irqrestore(&dma_drv_lock, flags);

	if (i < nr_cqdma_channel) {
		mt_reset_gdma_conf(i);
		return i;
	}

	/* disable cqdma clock */
	if (clk_cqdma && !keep_clock_ao)
		clk_disable_unprepare(clk_cqdma);

	return -DMA_ERR_NO_FREE_CH;
}
EXPORT_SYMBOL(mt_req_gdma);

/*
 * mt_start_gdma: start the DMA stransfer for the specified GDMA channel
 * @channel: GDMA channel to start
 * Return 0 for success; return negative errot code for failure.
 */
int mt_start_gdma(int channel)
{
	if ((channel < GDMA_START) ||
			(channel >= (GDMA_START + nr_cqdma_channel)))
		return -DMA_ERR_INVALID_CH;

	if (dma_ctrl[channel].in_use == 0)
		return -DMA_ERR_CH_FREE;

	mt_reg_sync_writel(DMA_INT_FLAG_CLR_BIT, DMA_INT_FLAG(channel));
	mt_reg_sync_writel(DMA_START_BIT, DMA_START(channel));

	return 0;
}
EXPORT_SYMBOL(mt_start_gdma);

/*
 * mt_polling_gdma: wait the DMA to finish for the specified GDMA channel
 * @channel: GDMA channel to polling
 * @timeout: polling timeout in ms
 * Return 0 for success;
 * Return 1 for timeout
 * return negative errot code for failure.
 */
int mt_polling_gdma(int channel, unsigned long timeout)
{
	if (channel < GDMA_START)
		return -DMA_ERR_INVALID_CH;

	if (channel >= (GDMA_START + nr_cqdma_channel))
		return -DMA_ERR_INVALID_CH;

	if (dma_ctrl[channel].in_use == 0)
		return -DMA_ERR_CH_FREE;

	timeout = jiffies + ((HZ * timeout) / 1000);

	do {
		if (time_after(jiffies, timeout)) {
			pr_err("GDMA_%d polling timeout !!\n", channel);
			mt_dump_gdma(channel);
			return 1;
		}
	} while (readl(DMA_START(channel)));

	return 0;
}
EXPORT_SYMBOL(mt_polling_gdma);

/*
 * mt_stop_gdma: stop the DMA stransfer for the specified GDMA channel
 * @channel: GDMA channel to stop
 * Return 0 for success; return negative errot code for failure.
 */
int mt_stop_gdma(int channel)
{
	if (channel < GDMA_START)
		return -DMA_ERR_INVALID_CH;

	if (channel >= (GDMA_START + nr_cqdma_channel))
		return -DMA_ERR_INVALID_CH;

	if (dma_ctrl[channel].in_use == 0)
		return -DMA_ERR_CH_FREE;

	mt_reg_sync_writel(DMA_FLUSH_BIT, DMA_FLUSH(channel));
	while (readl(DMA_START(channel)))
		;
	mt_reg_sync_writel(DMA_FLUSH_CLR_BIT, DMA_FLUSH(channel));
	mt_reg_sync_writel(DMA_INT_FLAG_CLR_BIT, DMA_INT_FLAG(channel));

	return 0;
}
EXPORT_SYMBOL(mt_stop_gdma);

/*
 * mt_config_gdma: configure the given GDMA channel.
 * @channel: GDMA channel to configure
 * @config: pointer to the mt_gdma_conf structure in which
 * the GDMA configurations store
 * @flag: ALL, SRC, DST, or SRC_AND_DST.
 * Return 0 for success; return negative errot code for failure.
 */
int mt_config_gdma(int channel, struct mt_gdma_conf *config, int flag)
{
	unsigned int dma_con = 0x0, limiter = 0;

	if ((channel < GDMA_START) ||
			(channel >= (GDMA_START + nr_cqdma_channel)))
		return -DMA_ERR_INVALID_CH;

	if (dma_ctrl[channel].in_use == 0)
		return -DMA_ERR_CH_FREE;

	if (!config)
		return -DMA_ERR_INV_CONFIG;

	if (config->sfix) {
		pr_err("GMDA fixed address mode doesn't support\n");
		return -DMA_ERR_INV_CONFIG;
	}

	if (config->dfix) {
		pr_err("GMDA fixed address mode doesn't support\n");
		return -DMA_ERR_INV_CONFIG;
	}

	if (config->count > MAX_TRANSFER_LEN1) {
		pr_err("GDMA transfer length cannot exceeed 0x%x.\n",
				MAX_TRANSFER_LEN1);
		return -DMA_ERR_INV_CONFIG;
	}

	if (config->limiter > MAX_SLOW_DOWN_CNTER) {
		pr_err("GDMA slow down counter cannot exceeed 0x%x.\n",
				MAX_SLOW_DOWN_CNTER);
		return -DMA_ERR_INV_CONFIG;
	}

	switch (flag) {
	case ALL:
		/* Control Register */
		mt_reg_sync_writel((u32) config->src, DMA_SRC(channel));
		mt_reg_sync_writel((u32) config->dst, DMA_DST(channel));

		mt_reg_sync_writel((config->wplen) & DMA_GDMA_LEN_MAX_MASK,
				DMA_LEN2(channel));
		mt_reg_sync_writel(config->wpto, DMA_JUMP_ADDR(channel));
		mt_reg_sync_writel((config->count) & DMA_GDMA_LEN_MAX_MASK,
				DMA_LEN1(channel));

		if (enable_4G()) {
			/*
			 * enable_4G() valid in Jade,K2,ROME,Everest,
			 * unvalid after Olympus
			 *
			 * in Jade need set bit 32 when enable_4GB()is true,
			 * whever address is in 4th-GB or not
			 */
			mt_reg_sync_writel(
				(DMA_ADDR2_EN_BIT |
				 readl(DMA_SRC_4G_SUPPORT(channel))),
					   DMA_SRC_4G_SUPPORT(channel));
			mt_reg_sync_writel(
				(DMA_ADDR2_EN_BIT |
				 readl(DMA_DST_4G_SUPPORT(channel))),
					   DMA_DST_4G_SUPPORT(channel));
			mt_reg_sync_writel(
				(DMA_ADDR2_EN_BIT |
				 readl(DMA_JUMP_4G_SUPPORT(channel))),
					   DMA_JUMP_4G_SUPPORT(channel));
			pr_debug("2:ADDR2_cfg(4GB):%x %x %x\n",
					readl(DMA_SRC_4G_SUPPORT(channel)),
					readl(DMA_DST_4G_SUPPORT(channel)),
					readl(DMA_JUMP_4G_SUPPORT(channel)));
		} else {
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
			mt_reg_sync_writel((u32)((u64)(config->src) >> 32),
					DMA_SRC_4G_SUPPORT(channel));
			mt_reg_sync_writel((u32)((u64)(config->dst) >> 32),
					DMA_DST_4G_SUPPORT(channel));
			mt_reg_sync_writel((u32)((u64)(config->jump) >> 32),
					DMA_JUMP_4G_SUPPORT(channel));
#endif

			pr_debug("2:ADDR2_cfg(4GB):SRC=0x%x  DST=0x%x JUMP=0x%x\n",
					readl(DMA_SRC_4G_SUPPORT(channel)),
					readl(DMA_DST_4G_SUPPORT(channel)),
					readl(DMA_JUMP_4G_SUPPORT(channel)));
		}

		if (config->wpen)
			dma_con |= DMA_CON_WPEN;

		if (config->wpsd)
			dma_con |= DMA_CON_WPSD;

		if (config->iten) {
			dma_ctrl[channel].isr_cb = config->isr_cb;
			dma_ctrl[channel].data = config->data;
			mt_reg_sync_writel(DMA_INT_EN_BIT,
					DMA_INT_EN(channel));
		} else {
			dma_ctrl[channel].isr_cb = NULL;
			dma_ctrl[channel].data = NULL;
			mt_reg_sync_writel(DMA_INT_EN_CLR_BIT,
					DMA_INT_EN(channel));
		}

		if (!(config->dfix) && !(config->sfix))
			dma_con |= (config->burst & DMA_CON_BURST_MASK);
		else {
			if (config->dfix) {
				dma_con |= DMA_CON_DFIX;
				dma_con |= DMA_CON_WSIZE_1BYTE;
			}

			if (config->sfix) {
				dma_con |= DMA_CON_SFIX;
				dma_con |= DMA_CON_RSIZE_1BYTE;
			}
			/*
			 * fixed src/dst mode only supports burst type SINGLE
			 */
			dma_con |= DMA_CON_BURST_SINGLE;
		}

		if (config->limiter) {
			limiter = (config->limiter) & DMA_CON_SLOW_MAX_MASK;
			dma_con |= limiter << DMA_CON_SLOW_OFFSET;
			dma_con |= DMA_CON_SLOW_EN;
		}

		mt_reg_sync_writel(dma_con, DMA_CON(channel));
		break;

	case SRC:
		mt_reg_sync_writel((u32) config->src, DMA_SRC(channel));
		break;

	case DST:
		mt_reg_sync_writel((u32) config->dst, DMA_DST(channel));
		break;

	case SRC_AND_DST:
		mt_reg_sync_writel((u32) config->src, DMA_SRC(channel));
		mt_reg_sync_writel((u32) config->dst, DMA_DST(channel));
		break;

	default:
		break;
	}

	/*
	 * use the data synchronization barrier
	 * to ensure that all writes are completed
	 */
	mb();

	return 0;
}
EXPORT_SYMBOL(mt_config_gdma);

/*
 * mt_free_gdma: free a general DMA.
 * @channel: channel to free
 * Return 0 for success; return negative errot code for failure.
 */
int mt_free_gdma(int channel)
{
	if (channel < GDMA_START)
		return -DMA_ERR_INVALID_CH;

	if (channel >= (GDMA_START + nr_cqdma_channel))
		return -DMA_ERR_INVALID_CH;

	if (dma_ctrl[channel].in_use == 0)
		return -DMA_ERR_CH_FREE;

	mt_stop_gdma(channel);

	/* disable cqdma clock */
	if (clk_cqdma && !keep_clock_ao)
		clk_disable_unprepare(clk_cqdma);

#ifdef CONFIG_PM_WAKELOCKS
	__pm_relax(&wk_lock[channel]);
#else
	wake_unlock(&wk_lock[channel]);
#endif

	dma_ctrl[channel].isr_cb = NULL;
	dma_ctrl[channel].data = NULL;
	dma_ctrl[channel].in_use = 0;

	return 0;
}
EXPORT_SYMBOL(mt_free_gdma);

/*
 * mt_dump_gdma: dump registers for the specified GDMA channel
 * @channel: GDMA channel to dump registers
 * Return 0 for success; return negative errot code for failure.
 */
int mt_dump_gdma(int channel)
{
	unsigned int i;

	pr_debug("Channel 0x%x\n", channel);
	for (i = 0; i < 96; i++)
		pr_debug("addr:%p, value:%x\n",
				env_info[channel].base + i * 4,
				readl(env_info[channel].base + i * 4));

	return 0;
}
EXPORT_SYMBOL(mt_dump_gdma);

/*
 * mt_warm_reset_gdma: warm reset the specified GDMA channel
 * @channel: GDMA channel to warm reset
 * Return 0 for success; return negative errot code for failure.
 */
int mt_warm_reset_gdma(int channel)
{
	if (channel < GDMA_START)
		return -DMA_ERR_INVALID_CH;

	if (channel >= (GDMA_START + nr_cqdma_channel))
		return -DMA_ERR_INVALID_CH;

	if (dma_ctrl[channel].in_use == 0)
		return -DMA_ERR_CH_FREE;

	mt_reg_sync_writel(DMA_WARM_RST_BIT, DMA_RESET(channel));

	if (mt_polling_gdma(channel, GDMA_WARM_RST_TIMEOUT) != 0)
		return 1;

	return 0;
}
EXPORT_SYMBOL(mt_warm_reset_gdma);

/*
 * mt_hard_reset_gdma: hard reset the specified GDMA channel
 * @channel: GDMA channel to hard reset
 * Return 0 for success; return negative errot code for failure.
 */
int mt_hard_reset_gdma(int channel)
{
	if (channel < GDMA_START)
		return -DMA_ERR_INVALID_CH;

	if (channel >= (GDMA_START + nr_cqdma_channel))
		return -DMA_ERR_INVALID_CH;

	if (dma_ctrl[channel].in_use == 0)
		return -DMA_ERR_CH_FREE;

	pr_debug("GDMA_%d Hard Reset !!\n", channel);

	mt_reg_sync_writel(DMA_HARD_RST_BIT, DMA_RESET(channel));
	mt_reg_sync_writel(DMA_HARD_RST_CLR_BIT, DMA_RESET(channel));

	return 0;
}
EXPORT_SYMBOL(mt_hard_reset_gdma);

/*
 * mt_reset_gdma: reset the specified GDMA channel
 * @channel: GDMA channel to reset
 * Return 0 for success; return negative errot code for failure.
 */
int mt_reset_gdma(int channel)
{
	if (channel < GDMA_START)
		return -DMA_ERR_INVALID_CH;

	if (channel >= (GDMA_START + nr_cqdma_channel))
		return -DMA_ERR_INVALID_CH;

	if (dma_ctrl[channel].in_use == 0)
		return -DMA_ERR_CH_FREE;

	if (mt_warm_reset_gdma(channel) != 0)
		mt_hard_reset_gdma(channel);

	return 0;
}
EXPORT_SYMBOL(mt_reset_gdma);

/*
 * gdma1_irq_handler: general DMA channel 1 interrupt service routine.
 * @irq: DMA IRQ number
 * @dev_id:
 * Return IRQ returned code.
 */
static irqreturn_t gdma1_irq_handler(int irq, void *dev_id)
{
	unsigned int glbsta;
	unsigned int i;

	for (i = 0; i < nr_cqdma_channel; i++)
		if (env_info[i].irq == irq)
			break;

	if (i == nr_cqdma_channel) {
		pr_debug("[CQDMA]irq:%d over nr_cqdma_channel!\n", irq);
		return IRQ_NONE;
	}

	glbsta = readl(DMA_INT_FLAG(i));

	if (glbsta & 0x1) {
		if (dma_ctrl[i].isr_cb)
			dma_ctrl[i].isr_cb(dma_ctrl[i].data);

		mt_reg_sync_writel(DMA_INT_FLAG_CLR_BIT, DMA_INT_FLAG(i));
	} else {
		return IRQ_NONE;
	}

	return IRQ_HANDLED;
}

/*
 * mt_reset_gdma_conf: reset the config of the specified DMA channel
 * @iChannel: channel number of the DMA channel to reset
 */
void mt_reset_gdma_conf(const unsigned int channel)
{
	struct mt_gdma_conf conf;

	memset(&conf, 0, sizeof(struct mt_gdma_conf));

	if (mt_config_gdma(channel, &conf, ALL) != 0)
		return;
}

static const struct of_device_id cqdma_of_ids[] = {
	{ .compatible = "mediatek,mt-cqdma-v1", },
	{}
};

static void cqdma_reset(int nr_channel)
{
	int i = 0;

	for (i = 0; i < nr_channel; i++)
		mt_reset_gdma_conf(i);
}

static int cqdma_probe(struct platform_device *pdev)
{
	int ret = 0, irq = 0;
	unsigned int i;
	struct resource *res;
	const char *keep_clk_ao_str = NULL;

	pr_debug("[MTK CQDMA] module probe.\n");

	of_property_read_u32(pdev->dev.of_node,
			"nr_channel", &nr_cqdma_channel);
	if (!nr_cqdma_channel) {
		pr_err("[CQDMA] no channel found\n");
		return -ENODEV;
	}
	pr_debug("[CQDMA] DMA channel = %d\n", nr_cqdma_channel);

	env_info = kmalloc(sizeof(struct cqdma_env_info)*(nr_cqdma_channel),
			GFP_KERNEL);
	if (!env_info)
		return -ENOMEM;

#ifdef CONFIG_PM_WAKELOCKS
	wk_lock = kmalloc(sizeof(struct wakeup_source)*(nr_cqdma_channel),
			GFP_KERNEL);
#else
	wk_lock = kmalloc(sizeof(struct wake_lock)*(nr_cqdma_channel),
			GFP_KERNEL);
#endif
	if (!wk_lock)
		return -ENOMEM;

	for (i = 0; i < nr_cqdma_channel; i++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		env_info[i].base = devm_ioremap_resource(&pdev->dev, res);
		env_info[i].irq = platform_get_irq(pdev, i);

		if (IS_ERR(env_info[i].base) || (env_info[i].irq <= 0)) {
			pr_err("unable to map CQDMA%d base reg and irq=%d!\n",
					i, irq);
			return -EINVAL;
		}
		pr_debug("[CQDMA%d] vbase = 0x%p, irq = %d\n",
				i, env_info[i].base, env_info[i].irq);
	}

	cqdma_reset(nr_cqdma_channel);

	for (i = 0; i < nr_cqdma_channel; i++) {
		ret = request_irq(env_info[i].irq, gdma1_irq_handler,
				IRQF_TRIGGER_NONE, "CQDMA", &dma_ctrl);
		if (ret > 0)
			pr_err("GDMA%d IRQ LINE NOT AVAILABLE,ret 0x%x!!\n",
					i, ret);

#ifdef CONFIG_PM_WAKELOCKS
		wakeup_source_init(&wk_lock[i], "cqdma_wakelock");
#else
		wake_lock_init(&wk_lock[i],
				WAKE_LOCK_SUSPEND, "cqdma_wakelock");
#endif
	}

	clk_cqdma = devm_clk_get(&pdev->dev, "cqdma");
	if (IS_ERR(clk_cqdma)) {
		pr_err("can not get CQDMA clock fail!\n");
		return PTR_ERR(clk_cqdma);
	}

	if (!of_property_read_string(pdev->dev.of_node,
				"keep_clock_ao", &keep_clk_ao_str)) {
		if (keep_clk_ao_str && !strncmp(keep_clk_ao_str, "yes", 3)) {
			ret = clk_prepare_enable(clk_cqdma);
			if (ret)
				pr_info("enable CQDMA clk fail!\n");
			else
				keep_clock_ao = 1;
		}
	}

	return ret;
}

static int cqdma_remove(struct platform_device *dev)
{
	return 0;
}

static struct platform_driver mtk_cqdma_driver = {
	.probe = cqdma_probe,
	.remove = cqdma_remove,
	.driver = {
		.name = "cqdma",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = cqdma_of_ids,
#endif
		},
};

static int __init init_cqdma(void)
{
	int ret = 0;

	ret = platform_driver_register(&mtk_cqdma_driver);
	if (ret)
		pr_err("CQDMA init FAIL, ret 0x%x!!!\n", ret);

	return ret;
}

late_initcall(init_cqdma);
