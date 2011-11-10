/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/android_pmem.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/msm_audio.h>
#include <linux/clk.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <mach/msm_iomap-8x60.h>
#include <mach/audio_dma_msm8k.h>
#include <sound/dai.h>
#include "lpass-pcm.h"

struct dai_baseinfo {
	void __iomem *base;
};

static struct dai_baseinfo dai_info;

struct dai_drv {
	u8	*buffer;
	u32	buffer_phys;
	int channels;
	irqreturn_t (*callback) (int intrsrc, void *private_data);
	void *private_data;
	int in_use;
	u32	buffer_len;
	u32	period_len;
	u32	master_mode;
};

static struct dai_drv *dai[MAX_CHANNELS];
static spinlock_t dai_lock;

static int dai_find_dma_channel(uint32_t intrsrc)
{
	int i, dma_channel = 0;
	pr_debug("%s\n", __func__);

	for (i = 0; i <= 27; i += 3) {
		if (intrsrc & (1 << i)) {
			dma_channel = i / 3;
			break;
		}
	}
	return dma_channel;
}

void register_dma_irq_handler(int dma_ch,
	irqreturn_t (*callback) (int intrsrc, void *private_data),
	void *private_data)
{
	pr_debug("%s\n", __func__);
	dai[dma_ch]->callback = callback;
	dai[dma_ch]->private_data = private_data;
}

void unregister_dma_irq_handler(int dma_ch)
{
	pr_debug("%s\n", __func__);
	dai[dma_ch]->callback = NULL;
	dai[dma_ch]->private_data = NULL;
}

static irqreturn_t dai_irq_handler(int irq, void *data)
{
	unsigned long flag;
	uint32_t intrsrc;
	uint32_t dma_ch = 0;
	irqreturn_t ret = IRQ_HANDLED;

	pr_debug("%s\n", __func__);
	spin_lock_irqsave(&dai_lock, flag);
	intrsrc = readl(dai_info.base + LPAIF_IRQ_STAT(0));
	writel(intrsrc, dai_info.base + LPAIF_IRQ_CLEAR(0));
	mb();
	while (intrsrc) {
		dma_ch = dai_find_dma_channel(intrsrc);

		if (!dai[dma_ch]->callback)
			goto handled;
		if (!dai[dma_ch]->private_data)
			goto handled;
		ret = dai[dma_ch]->callback(intrsrc,
				dai[dma_ch]->private_data);
		intrsrc &= ~(0x7 << (dma_ch * 3));
	}
handled:
	spin_unlock_irqrestore(&dai_lock, flag);
	return ret;
}

void dai_print_state(uint32_t dma_ch)
{
	int i = 0;
	unsigned long *ptrmem = (unsigned long *)dai_info.base;

	for (i = 0; i < 4; i++, ++ptrmem)
		pr_debug("[0x%08x]=0x%08x\n", (unsigned int)ptrmem,
					(unsigned int)*ptrmem);

	ptrmem = (unsigned long *)(dai_info.base
			+ DMA_CH_CTL_BASE + DMA_CH_INDEX(dma_ch));
	for (i = 0; i < 10; i++, ++ptrmem)
		pr_debug("[0x%08x]=0x%08x\n", (unsigned int)ptrmem,
					(unsigned int) *ptrmem);
}

static int dai_enable_irq(uint32_t dma_ch)
{
	int ret;
	pr_debug("%s\n", __func__);
	ret = request_irq(LPASS_SCSS_AUDIO_IF_OUT0_IRQ, dai_irq_handler,
			IRQF_TRIGGER_RISING | IRQF_SHARED, "msm-i2s",
						(void *) (dma_ch+1));
	if (ret < 0) {
		pr_debug("Request Irq Failed err = %d\n", ret);
		return ret;
	}
	return ret;
}

static void dai_config_dma(uint32_t dma_ch)
{
	pr_debug("%s dma_ch = %u\n", __func__, dma_ch);

	writel(dai[dma_ch]->buffer_phys,
			dai_info.base + LPAIF_DMA_BASE(dma_ch));
	writel(((dai[dma_ch]->buffer_len >> 2) - 1),
			dai_info.base + LPAIF_DMA_BUFF_LEN(dma_ch));
	writel(((dai[dma_ch]->period_len >> 2) - 1),
			dai_info.base + LPAIF_DMA_PER_LEN(dma_ch));
	mb();
}

static void dai_enable_codec(uint32_t dma_ch, int codec)
{
	uint32_t intrVal;
	uint32_t i2sctl;
	pr_debug("%s\n", __func__);

	intrVal = readl(dai_info.base + LPAIF_IRQ_EN(0));
	intrVal = intrVal | (7 << (dma_ch * 3));
	writel(intrVal, dai_info.base + LPAIF_IRQ_EN(0));
	if (codec == DAI_SPKR) {
		writel(0x0813, dai_info.base + LPAIF_DMA_CTL(dma_ch));
		i2sctl = 0x4400;
		i2sctl |= (dai[dma_ch]->master_mode ? WS_SRC_INT : WS_SRC_EXT);
		writel(i2sctl, dai_info.base + LPAIF_I2S_CTL_OFFSET(DAI_SPKR));
	} else if (codec == DAI_MIC) {
		writel(0x81b, dai_info.base + LPAIF_DMA_CTL(dma_ch));
		i2sctl = 0x0110;
		i2sctl |= (dai[dma_ch]->master_mode ? WS_SRC_INT : WS_SRC_EXT);
		writel(i2sctl, dai_info.base + LPAIF_I2S_CTL_OFFSET(DAI_MIC));
	}
}

static void dai_disable_codec(uint32_t dma_ch, int codec)
{
	uint32_t intrVal = 0;
	uint32_t intrVal1 = 0;
	unsigned long flag = 0x0;

	pr_debug("%s\n", __func__);
	spin_lock_irqsave(&dai_lock, flag);

	intrVal1 = readl(dai_info.base + LPAIF_I2S_CTL_OFFSET(codec));

	if (codec == DAI_SPKR)
		intrVal1 = intrVal1 & ~(1 << 14);
	else if (codec == DAI_MIC)
		intrVal1 = intrVal1 & ~(1 << 8);

	writel(intrVal1, dai_info.base + LPAIF_I2S_CTL_OFFSET(codec));
	intrVal = 0x0;
	writel(intrVal, dai_info.base + LPAIF_DMA_CTL(dma_ch));

	spin_unlock_irqrestore(&dai_lock, flag);
}

int dai_open(uint32_t dma_ch)
{

	pr_debug("%s\n", __func__);
	if (!dai_info.base) {
		pr_debug("%s failed as no msm-dai device\n", __func__);
		return -ENODEV;
	}
	if (dma_ch >= MAX_CHANNELS) {
		pr_debug("%s over max channesl %d\n", __func__, dma_ch);
		return -ENODEV;
	}
	return 0;
}

void dai_close(uint32_t dma_ch)
{
	pr_debug("%s\n", __func__);
	if ((dma_ch >= 0) && (dma_ch < 5))
		dai_disable_codec(dma_ch, DAI_SPKR);
	else
		dai_disable_codec(dma_ch, DAI_MIC);
	free_irq(LPASS_SCSS_AUDIO_IF_OUT0_IRQ, (void *) (dma_ch + 1));
}

void dai_set_master_mode(uint32_t dma_ch, int mode)
{
	if (dma_ch < MAX_CHANNELS)
		dai[dma_ch]->master_mode = mode;
	else
		pr_err("%s: invalid dma channel\n", __func__);
}

int dai_set_params(uint32_t dma_ch, struct dai_dma_params *params)
{
	pr_debug("%s\n", __func__);
	dai[dma_ch]->buffer = params->buffer;
	dai[dma_ch]->buffer_phys = params->src_start;
	dai[dma_ch]->channels = params->channels;
	dai[dma_ch]->buffer_len = params->buffer_size;
	dai[dma_ch]->period_len = params->period_size;
	mb();
	dai_config_dma(dma_ch);
	return dma_ch;
}

int dai_start(uint32_t dma_ch)
{
	unsigned long flag = 0x0;

	spin_lock_irqsave(&dai_lock, flag);
	dai_enable_irq(dma_ch);
	if ((dma_ch >= 0) && (dma_ch < 5))
		dai_enable_codec(dma_ch, DAI_SPKR);
	else
		dai_enable_codec(dma_ch, DAI_MIC);
	spin_unlock_irqrestore(&dai_lock, flag);
	dai_print_state(dma_ch);
	return 0;
}

#define   HDMI_BURST_INCR4		(1 << 11)
#define   HDMI_WPSCNT			(1 << 8)
#define   HDMI_AUDIO_INTF		(5 << 4)
#define   HDMI_FIFO_WATER_MARK		(7 << 1)
#define   HDMI_ENABLE			(1)

int dai_start_hdmi(uint32_t dma_ch)
{
	unsigned long flag = 0x0;
	uint32_t val;

	pr_debug("%s dma_ch = %u\n", __func__, dma_ch);

	spin_lock_irqsave(&dai_lock, flag);

	dai_enable_irq(dma_ch);

	if ((dma_ch >= 0) && (dma_ch < 5)) {

		val = readl(dai_info.base + LPAIF_IRQ_EN(0));
		val = val | (7 << (dma_ch * 3));
		writel(val, dai_info.base + LPAIF_IRQ_EN(0));
		mb();


		val = (HDMI_BURST_INCR4 | HDMI_WPSCNT | HDMI_AUDIO_INTF |
			HDMI_FIFO_WATER_MARK | HDMI_ENABLE);

		writel(val, dai_info.base + LPAIF_DMA_CTL(dma_ch));
	}
	spin_unlock_irqrestore(&dai_lock, flag);

	mb();
	dai_print_state(dma_ch);
	return 0;
}

int wait_for_dma_cnt_stop(uint32_t dma_ch)
{
	uint32_t dma_per_cnt_reg_val, dma_per_cnt, prev_dma_per_cnt;
	uint32_t i;

	pr_info("%s dma_ch %u\n", __func__, dma_ch);

	dma_per_cnt_reg_val =  readl_relaxed(dai_info.base +
					LPAIF_DMA_PER_CNT(dma_ch));

	dma_per_cnt =
		((LPAIF_DMA_PER_CNT_PER_CNT_MASK & dma_per_cnt_reg_val) >>
			LPAIF_DMA_PER_CNT_PER_CNT_SHIFT) -
		((LPAIF_DMA_PER_CNT_FIFO_WORDCNT_MASK & dma_per_cnt_reg_val) >>
			LPAIF_DMA_PER_CNT_FIFO_WORDCNT_SHIFT);

	prev_dma_per_cnt = dma_per_cnt;

	i = 1;
	pr_info("%s: i = %u dma_per_cnt_reg_val 0x%08x , dma_per_cnt %u\n",
		__func__, i, dma_per_cnt_reg_val, dma_per_cnt);

	while (i <= 50) {
		msleep(50);

		dma_per_cnt_reg_val =  readl_relaxed(dai_info.base +
						LPAIF_DMA_PER_CNT(dma_ch));

		dma_per_cnt =
		((LPAIF_DMA_PER_CNT_PER_CNT_MASK & dma_per_cnt_reg_val) >>
			LPAIF_DMA_PER_CNT_PER_CNT_SHIFT) -
		((LPAIF_DMA_PER_CNT_FIFO_WORDCNT_MASK & dma_per_cnt_reg_val) >>
			LPAIF_DMA_PER_CNT_FIFO_WORDCNT_SHIFT);

		i++;

		pr_info("%s: i = %u dma_per_cnt_reg_val 0x%08x , dma_per_cnt %u\n",
			__func__, i, dma_per_cnt_reg_val, dma_per_cnt);

		if (prev_dma_per_cnt == dma_per_cnt)
			break;

		prev_dma_per_cnt = dma_per_cnt;
	}
	return 0;
}

void dai_stop_hdmi(uint32_t dma_ch)
{
	unsigned long flag = 0x0;
	uint32_t intrVal;
	uint32_t int_mask = 0x00000007;

	pr_debug("%s dma_ch %u\n", __func__, dma_ch);

	spin_lock_irqsave(&dai_lock, flag);

	free_irq(LPASS_SCSS_AUDIO_IF_OUT0_IRQ, (void *) (dma_ch + 1));


	intrVal = 0x0;
	writel(intrVal, dai_info.base + LPAIF_DMA_CTL(dma_ch));

	mb();

	intrVal = readl(dai_info.base + LPAIF_IRQ_EN(0));

	int_mask = ((int_mask) << (dma_ch * 3));
	int_mask = ~int_mask;

	intrVal = intrVal & int_mask;
	writel(intrVal, dai_info.base + LPAIF_IRQ_EN(0));

	mb();

	spin_unlock_irqrestore(&dai_lock, flag);
}

int dai_stop(uint32_t dma_ch)
{
	pr_debug("%s\n", __func__);
	return 0;
}


uint32_t dai_get_dma_pos(uint32_t dma_ch)
{

	uint32_t addr;

	pr_debug("%s\n", __func__);
	addr = readl(dai_info.base + LPAIF_DMA_CURR_ADDR(dma_ch));

	return addr;
}

static int __devinit dai_probe(struct platform_device *pdev)
{
	int rc = 0;
	int i = 0;
	struct resource *src;
	src = platform_get_resource_byname(pdev, IORESOURCE_MEM, "msm-dai");
	if (!src) {
		rc = -ENODEV;
		pr_debug("%s Error  rc=%d\n", __func__, rc);
		goto error;
	}
	for (i = 0; i <= MAX_CHANNELS; i++) {
		dai[i] = kzalloc(sizeof(struct dai_drv), GFP_KERNEL);
		if (!dai[0]) {
			pr_debug("Allocation failed for dma_channel = 0\n");
			return -ENODEV;
		}
	}
	dai_info.base = ioremap(src->start, (src->end - src->start) + 1);
	pr_debug("%s: msm-dai: 0x%08x\n", __func__,
				(unsigned int)dai_info.base);
	spin_lock_init(&dai_lock);
error:
	return rc;
}

static int dai_remove(struct platform_device *pdev)
{
	iounmap(dai_info.base);
	return 0;
}

static struct platform_driver dai_driver = {
	.probe = dai_probe,
	.remove = dai_remove,
	.driver = {
		.name = "msm-dai",
		.owner = THIS_MODULE
		},
};

static struct resource msm_lpa_resources[] = {
	{
		.start = MSM_LPA_PHYS,
		.end   = MSM_LPA_END,
		.flags = IORESOURCE_MEM,
		.name  = "msm-dai",
	},
};

static struct platform_device *codec_device;

static int msm_dai_dev_register(const char *name)
{
	int ret = 0;

	pr_debug("%s : called\n", __func__);
	codec_device = platform_device_alloc(name, -1);
	if (codec_device == NULL) {
		pr_debug("Failed to allocate %s\n", name);
		return -ENODEV;
	}

	platform_set_drvdata(codec_device, (void *)&dai_info);
	platform_device_add_resources(codec_device, &msm_lpa_resources[0],
				ARRAY_SIZE(msm_lpa_resources));
	ret = platform_device_add(codec_device);
	if (ret != 0) {
		pr_debug("Failed to register %s: %d\n", name, ret);
		platform_device_put(codec_device);
	}
	return ret;
}

static int __init dai_init(void)
{
	if (msm_dai_dev_register("msm-dai")) {
		pr_notice("dai_init: msm-dai Failed");
		return -ENODEV;
	}
	return platform_driver_register(&dai_driver);
}

static void __exit dai_exit(void)
{
	platform_driver_unregister(&dai_driver);
	platform_device_put(codec_device);
}

module_init(dai_init);
module_exit(dai_exit);

MODULE_DESCRIPTION("MSM I2S driver");
MODULE_LICENSE("GPL v2");
