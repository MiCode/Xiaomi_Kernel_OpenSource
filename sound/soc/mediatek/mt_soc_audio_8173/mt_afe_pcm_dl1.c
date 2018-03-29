/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
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

#include "mt_afe_def.h"
#include "mt_afe_reg.h"
#include "mt_afe_clk.h"
#include "mt_afe_control.h"
#include "mt_afe_digital_type.h"
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <sound/soc.h>
#include <linux/sched.h>
#include <linux/cgroup.h>
#include <linux/pid.h>

enum mt_afe_dl1_playback_mux {
	MTK_INTERFACE = 0,
	I2S0,
	MTK_INTERFACE_AND_I2S0,
	MTK_INTERFACE_AND_I2S1_DATA2,
	/* i2s1 data2 to external chip then feedback from i2s0 to mtk interface */
	I2S1_DATA2_EXT_I2S0_TO_MTK_INTERFACE
};

struct mt_pcm_dl1_priv {
	bool prepared;
	bool enable_mtk_interface;
	bool enable_i2s0;
	bool enable_i2s0_low_jitter;
	bool enable_i2s1_low_jitter;
	bool enable_sram;
	bool enable_bus_clk_boost;
	unsigned int playback_mux;
	unsigned int i2s0_clock_mode;
	unsigned int i2s1_clock_mode;
};

/* #define ADJUST_THREAD_GROUP */
#ifdef ADJUST_THREAD_GROUP
static pid_t pid_old;
#endif

/*
 *    function implementation
 */
static int mt_pcm_dl1_close(struct snd_pcm_substream *substream);
static int mt_pcm_dl1_prestart(struct snd_pcm_substream *substream);
static int mt_pcm_dl1_post_stop(struct snd_pcm_substream *substream);

static const struct snd_pcm_hardware mt_pcm_dl1_hardware = {
	.info = (SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
		 SNDRV_PCM_INFO_RESUME | SNDRV_PCM_INFO_MMAP_VALID),
	.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
	.rates = SOC_HIFI_USE_RATE,
	.rate_min = SOC_NORMAL_USE_CHANNELS_MIN,
	.rate_max = SOC_NORMAL_USE_CHANNELS_MIN,
	.channels_min = SOC_NORMAL_USE_CHANNELS_MIN,
	.channels_max = SOC_NORMAL_USE_CHANNELS_MAX,
	.buffer_bytes_max = DL1_MAX_BUFFER_SIZE,
	.period_bytes_min = SOC_NORMAL_USE_PERIOD_SIZE_MIN,
	.period_bytes_max = (DL1_MAX_BUFFER_SIZE / SOC_NORMAL_USE_PERIODS_MIN),
	.periods_min = SOC_NORMAL_USE_PERIODS_MIN,
	.periods_max = SOC_NORMAL_USE_PERIODS_MAX,
	.fifo_size = 0,
};

static struct snd_pcm_hw_constraint_list mt_pcm_dl1_constraints_rates = {
	.count = ARRAY_SIZE(soc_hifi_supported_sample_rates),
	.list = soc_hifi_supported_sample_rates,
	.mask = 0,
};

static int mt_pcm_dl1_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	int ret = 0;
#ifdef ADJUST_THREAD_GROUP
	struct task_struct *tsk;
	pid_t current_pid = 0;
#endif
	pr_debug("%s\n", __func__);

	snd_soc_set_runtime_hwparams(substream, &mt_pcm_dl1_hardware);

	ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
					 &mt_pcm_dl1_constraints_rates);
	if (unlikely(ret < 0))
		pr_err("snd_pcm_hw_constraint_list failed: 0x%x\n", ret);

	ret = snd_pcm_hw_constraint_integer(runtime, SNDRV_PCM_HW_PARAM_PERIODS);
	if (unlikely(ret < 0))
		pr_err("snd_pcm_hw_constraint_integer failed: 0x%x\n", ret);

	/* here open audio clocks */
	mt_afe_main_clk_on();
	mt_afe_dac_clk_on();

	if (unlikely(ret < 0)) {
		pr_err("%s mt_pcm_dl1_close\n", __func__);
		mt_pcm_dl1_close(substream);
		return ret;
	}
#ifdef ADJUST_THREAD_GROUP
	current_pid = task_pid_nr(current);
	if (pid_old != current_pid) {
		tsk = find_task_by_vpid(1);
		if (tsk) {
			cgroup_attach_task_all(tsk, current);
			pid_old = current_pid;
		}
	}
#endif

	pr_debug("%s return\n", __func__);
	return 0;
}

static int mt_pcm_dl1_close(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mt_pcm_dl1_priv *priv = snd_soc_platform_get_drvdata(rtd->platform);

	pr_debug("%s\n", __func__);
	if (priv->prepared) {
		mt_pcm_dl1_post_stop(substream);
		mt_afe_remove_ctx_substream(MT_AFE_MEM_CTX_DL1);
		priv->prepared = false;
	}

	if (priv->enable_bus_clk_boost) {
		mt_afe_bus_clk_restore();
		priv->enable_bus_clk_boost = false;
	}

	mt_afe_dac_clk_off();
	mt_afe_main_clk_off();
	return 0;
}

static int mt_pcm_dl1_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *hw_params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_dma_buffer *dma_buf = &substream->dma_buffer;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mt_pcm_dl1_priv *priv = snd_soc_platform_get_drvdata(rtd->platform);
	int ret = 0;
	size_t buffer_size = params_buffer_bytes(hw_params);

	pr_debug("%s\n", __func__);

	dma_buf->dev.type = SNDRV_DMA_TYPE_DEV;
	dma_buf->dev.dev = substream->pcm->card->dev;
	dma_buf->private_data = NULL;

#ifdef AUDIO_MEMORY_SRAM
	if (buffer_size > mt_afe_get_sram_size()) {
		pr_debug("%s force to use dram for size %zu\n", __func__, buffer_size);
		priv->enable_sram = false;
	} else {
		priv->enable_sram = true;
	}
#else
	priv->enable_sram = false;
#endif

	if (priv->enable_sram) {
		substream->runtime->dma_bytes = buffer_size;
		substream->runtime->dma_area = (unsigned char *)mt_afe_get_sram_base_ptr();
		substream->runtime->dma_addr = mt_afe_get_sram_phy_addr();
	} else {
		ret = snd_pcm_lib_malloc_pages(substream, buffer_size);
		mt_afe_emi_clk_on();
	}

	if (ret >= 0)
		mt_afe_init_dma_buffer(MT_AFE_MEM_CTX_DL1, runtime);
	else
		pr_err("%s snd_pcm_lib_malloc_pages fail %d\n", __func__, ret);

	pr_debug("%s dma_bytes = %zu dma_area = %p dma_addr = 0x%llx\n",
		 __func__, substream->runtime->dma_bytes, substream->runtime->dma_area,
		 (unsigned long long)substream->runtime->dma_addr);

	return ret;
}

static int mt_pcm_dl1_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mt_pcm_dl1_priv *priv = snd_soc_platform_get_drvdata(rtd->platform);
	int ret = 0;

	pr_debug("%s\n", __func__);

	if (!priv->enable_sram && runtime->dma_area) {
		ret = snd_pcm_lib_free_pages(substream);
		mt_afe_emi_clk_off();
	}

	return ret;
}

static int mt_pcm_dl1_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mt_pcm_dl1_priv *priv = snd_soc_platform_get_drvdata(rtd->platform);

	pr_debug("%s rate = %u channels = %u format = %d period_size = %lu\n",
		 __func__, runtime->rate, runtime->channels,
		 runtime->format, runtime->period_size);

	/* HW sequence: */
	/* mt_pcm_dl1_prestart->codec->mt_pcm_dl1_start */
	if (likely(!priv->prepared)) {
		mt_afe_add_ctx_substream(MT_AFE_MEM_CTX_DL1, substream);
		mt_pcm_dl1_prestart(substream);
		priv->prepared = true;
	}

	if (runtime->rate > 48000 && !priv->enable_bus_clk_boost) {
		mt_afe_bus_clk_boost();
		priv->enable_bus_clk_boost = true;
	}
	return 0;
}

static int mt_pcm_dl1_prestart(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mt_pcm_dl1_priv *priv = snd_soc_platform_get_drvdata(rtd->platform);
	uint32_t memif_format = (runtime->format == SNDRV_PCM_FORMAT_S24_LE) ?
		MT_AFE_MEMIF_32_BIT_ALIGN_8BIT_0_24BIT_DATA : MT_AFE_MEMIF_16_BIT;
	uint32_t conn_format = snd_pcm_format_width(runtime->format) > 16 ?
		MT_AFE_CONN_OUTPUT_24BIT : MT_AFE_CONN_OUTPUT_16BIT;
	uint32_t wlen = snd_pcm_format_width(runtime->format) > 16 ?
		MT_AFE_I2S_WLEN_32BITS : MT_AFE_I2S_WLEN_16BITS;

	mt_afe_set_memif_fetch_format(MT_AFE_DIGITAL_BLOCK_MEM_DL1, memif_format);

	if (priv->playback_mux == MTK_INTERFACE ||
	    priv->playback_mux == MTK_INTERFACE_AND_I2S0 ||
	    priv->playback_mux == MTK_INTERFACE_AND_I2S1_DATA2 ||
	    priv->playback_mux == I2S1_DATA2_EXT_I2S0_TO_MTK_INTERFACE) {
		if (priv->i2s1_clock_mode == MT_AFE_LOW_JITTER_CLOCK) {
			mt_afe_enable_apll(runtime->rate);
			mt_afe_enable_apll_tuner(runtime->rate);
			mt_afe_set_mclk(MT_AFE_I2S1, runtime->rate);
			mt_afe_set_mclk(MT_AFE_ENGEN, runtime->rate);
			mt_afe_enable_apll_div_power(MT_AFE_I2S1, runtime->rate);
			mt_afe_enable_apll_div_power(MT_AFE_ENGEN, runtime->rate);
			priv->enable_i2s1_low_jitter = true;
		}

		if (priv->playback_mux == MTK_INTERFACE_AND_I2S1_DATA2) {
			mt_afe_set_out_conn_format(conn_format, INTER_CONN_O19);
			mt_afe_set_out_conn_format(conn_format, INTER_CONN_O20);
			mt_afe_set_connection(INTER_CONNECT, INTER_CONN_I05, INTER_CONN_O19);
			mt_afe_set_connection(INTER_CONNECT, INTER_CONN_I06, INTER_CONN_O20);
			mt_afe_set_out_conn_format(conn_format, INTER_CONN_O03);
			mt_afe_set_out_conn_format(conn_format, INTER_CONN_O04);
			mt_afe_set_connection(INTER_CONNECT, INTER_CONN_I05, INTER_CONN_O03);
			mt_afe_set_connection(INTER_CONNECT, INTER_CONN_I06, INTER_CONN_O04);
		} else if (priv->playback_mux == I2S1_DATA2_EXT_I2S0_TO_MTK_INTERFACE) {
			mt_afe_set_out_conn_format(conn_format, INTER_CONN_O19);
			mt_afe_set_out_conn_format(conn_format, INTER_CONN_O20);
			mt_afe_set_connection(INTER_CONNECT, INTER_CONN_I05, INTER_CONN_O19);
			mt_afe_set_connection(INTER_CONNECT, INTER_CONN_I06, INTER_CONN_O20);
		} else {
			mt_afe_set_out_conn_format(conn_format, INTER_CONN_O03);
			mt_afe_set_out_conn_format(conn_format, INTER_CONN_O04);
			mt_afe_set_connection(INTER_CONNECT, INTER_CONN_I05, INTER_CONN_O03);
			mt_afe_set_connection(INTER_CONNECT, INTER_CONN_I06, INTER_CONN_O04);
		}

		if (mt_afe_get_memory_path_state(MT_AFE_DIGITAL_BLOCK_I2S_OUT_DAC) == false) {
			mt_afe_enable_memory_path(MT_AFE_DIGITAL_BLOCK_I2S_OUT_DAC);
			mt_afe_set_i2s_dac_out(runtime->rate, priv->i2s1_clock_mode, wlen);
			mt_afe_enable_i2s_dac();
		} else {
			mt_afe_enable_memory_path(MT_AFE_DIGITAL_BLOCK_I2S_OUT_DAC);
		}
		priv->enable_mtk_interface = true;
	}

	if (priv->playback_mux == I2S0 ||
	    priv->playback_mux == MTK_INTERFACE_AND_I2S0 ||
	    priv->playback_mux == I2S1_DATA2_EXT_I2S0_TO_MTK_INTERFACE) {
		if (priv->i2s0_clock_mode == MT_AFE_LOW_JITTER_CLOCK) {
			mt_afe_enable_apll(runtime->rate);
			mt_afe_enable_apll_tuner(runtime->rate);
			mt_afe_set_mclk(MT_AFE_I2S0, runtime->rate);
			mt_afe_set_mclk(MT_AFE_ENGEN, runtime->rate);
			mt_afe_enable_apll_div_power(MT_AFE_I2S0, runtime->rate);
			mt_afe_enable_apll_div_power(MT_AFE_ENGEN, runtime->rate);
			priv->enable_i2s0_low_jitter = true;
		}

		if (priv->playback_mux == I2S1_DATA2_EXT_I2S0_TO_MTK_INTERFACE) {
			mt_afe_set_connection(INTER_CONNECT, INTER_CONN_I00, INTER_CONN_O03);
			mt_afe_set_connection(INTER_CONNECT, INTER_CONN_I01, INTER_CONN_O04);
		} else {
			mt_afe_set_out_conn_format(conn_format, INTER_CONN_O00);
			mt_afe_set_out_conn_format(conn_format, INTER_CONN_O01);
			mt_afe_set_connection(INTER_CONNECT, INTER_CONN_I05, INTER_CONN_O00);
			mt_afe_set_connection(INTER_CONNECT, INTER_CONN_I06, INTER_CONN_O01);
		}

#ifdef ENABLE_I2S0_CLK_RESYNC
		/* i2s0 soft reset begin */
		mt_afe_set_reg(AUDIO_TOP_CON1, 0x2, 0x2);
#endif

		mt_afe_set_sample_rate(MT_AFE_DIGITAL_BLOCK_MEM_I2S, runtime->rate);

		if (!mt_afe_get_memory_path_state(MT_AFE_DIGITAL_BLOCK_I2S_IN_2)) {
			mt_afe_enable_memory_path(MT_AFE_DIGITAL_BLOCK_I2S_IN_2);

			if (priv->enable_i2s0_low_jitter) {
				mt_afe_set_2nd_i2s_in(wlen,
						MT_AFE_I2S_SRC_MASTER_MODE,
						MT_AFE_BCK_INV_NO_INVERSE,
						MT_AFE_LOW_JITTER_CLOCK);
			} else {
				mt_afe_set_2nd_i2s_in(wlen,
						MT_AFE_I2S_SRC_MASTER_MODE,
						MT_AFE_BCK_INV_NO_INVERSE,
						MT_AFE_NORMAL_CLOCK);
			}

			mt_afe_enable_2nd_i2s_in();
		} else {
			mt_afe_enable_memory_path(MT_AFE_DIGITAL_BLOCK_I2S_IN_2);
		}

		if (priv->playback_mux == I2S0 ||
		    priv->playback_mux == MTK_INTERFACE_AND_I2S0) {
			if (mt_afe_get_memory_path_state(MT_AFE_DIGITAL_BLOCK_I2S_OUT_2)) {
				mt_afe_enable_memory_path(MT_AFE_DIGITAL_BLOCK_I2S_OUT_2);
				mt_afe_disable_2nd_i2s_out();
			} else {
				mt_afe_enable_memory_path(MT_AFE_DIGITAL_BLOCK_I2S_OUT_2);
			}

			if (priv->enable_i2s0_low_jitter)
				mt_afe_set_2nd_i2s_out(runtime->rate, MT_AFE_LOW_JITTER_CLOCK, wlen);
			else
				mt_afe_set_2nd_i2s_out(runtime->rate, MT_AFE_NORMAL_CLOCK, wlen);

			mt_afe_enable_2nd_i2s_out();
		}

#ifdef ENABLE_I2S0_CLK_RESYNC
		/* i2s0 soft reset end */
		udelay(1);
		mt_afe_set_reg(AUDIO_TOP_CON1, 0x0, 0x2);
#endif

		priv->enable_i2s0 = true;
	}

	mt_afe_enable_afe(true);
	return 0;
}

static int mt_pcm_dl1_start(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *const runtime = substream->runtime;
	struct timespec curr_tstamp;

	pr_debug("%s rate = %u channels = %u format = %d period_size = %lu\n",
		 __func__, runtime->rate, runtime->channels,
		 runtime->format, runtime->period_size);

	/* set dl1 sample ratelimit_state */
	mt_afe_set_sample_rate(MT_AFE_DIGITAL_BLOCK_MEM_DL1, runtime->rate);
	mt_afe_set_channels(MT_AFE_DIGITAL_BLOCK_MEM_DL1, runtime->channels);

	/* here to set interrupt */
	mt_afe_set_irq_counter(MT_AFE_IRQ_MCU_MODE_IRQ1, runtime->period_size);
	mt_afe_set_irq_rate(MT_AFE_IRQ_MCU_MODE_IRQ1, runtime->rate);

	mt_afe_enable_memory_path(MT_AFE_DIGITAL_BLOCK_MEM_DL1);
	mt_afe_set_irq_state(MT_AFE_IRQ_MCU_MODE_IRQ1, true);
	snd_pcm_gettime(substream->runtime, (struct timespec *)&curr_tstamp);

	pr_debug("%s curr_tstamp %ld %ld\n", __func__, curr_tstamp.tv_sec, curr_tstamp.tv_nsec);

	return 0;
}

static int mt_pcm_dl1_stop(struct snd_pcm_substream *substream)
{
	pr_debug("%s\n", __func__);

	mt_afe_disable_memory_path(MT_AFE_DIGITAL_BLOCK_MEM_DL1);
	mt_afe_set_irq_state(MT_AFE_IRQ_MCU_MODE_IRQ1, false);

	/* clean audio hardware buffer */
	mt_afe_reset_dma_buffer(MT_AFE_MEM_CTX_DL1);

	return 0;
}

static int mt_pcm_dl1_post_stop(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mt_pcm_dl1_priv *priv = snd_soc_platform_get_drvdata(rtd->platform);

	pr_debug("%s\n", __func__);

	if (priv->enable_mtk_interface) {
		mt_afe_disable_memory_path(MT_AFE_DIGITAL_BLOCK_I2S_OUT_DAC);
		if (!mt_afe_get_memory_path_state(MT_AFE_DIGITAL_BLOCK_I2S_OUT_DAC))
			mt_afe_disable_i2s_dac();

		if (priv->playback_mux == MTK_INTERFACE_AND_I2S1_DATA2) {
			mt_afe_set_connection(INTER_DISCONNECT, INTER_CONN_I05, INTER_CONN_O19);
			mt_afe_set_connection(INTER_DISCONNECT, INTER_CONN_I06, INTER_CONN_O20);
			mt_afe_set_connection(INTER_DISCONNECT, INTER_CONN_I05, INTER_CONN_O03);
			mt_afe_set_connection(INTER_DISCONNECT, INTER_CONN_I06, INTER_CONN_O04);
		} else if (priv->playback_mux == I2S1_DATA2_EXT_I2S0_TO_MTK_INTERFACE) {
			mt_afe_set_connection(INTER_DISCONNECT, INTER_CONN_I05, INTER_CONN_O19);
			mt_afe_set_connection(INTER_DISCONNECT, INTER_CONN_I06, INTER_CONN_O20);
		} else {
			mt_afe_set_connection(INTER_DISCONNECT, INTER_CONN_I05, INTER_CONN_O03);
			mt_afe_set_connection(INTER_DISCONNECT, INTER_CONN_I06, INTER_CONN_O04);
		}
		priv->enable_mtk_interface = false;

		if (priv->enable_i2s1_low_jitter) {
			mt_afe_disable_apll_div_power(MT_AFE_I2S1, runtime->rate);
			mt_afe_disable_apll_div_power(MT_AFE_ENGEN, runtime->rate);
			mt_afe_disable_apll_tuner(runtime->rate);
			mt_afe_disable_apll(runtime->rate);
			priv->enable_i2s1_low_jitter = false;
		}
	}

	if (priv->enable_i2s0) {
		if (priv->playback_mux == I2S0 ||
		    priv->playback_mux == MTK_INTERFACE_AND_I2S0) {
			mt_afe_disable_memory_path(MT_AFE_DIGITAL_BLOCK_I2S_OUT_2);
			if (!mt_afe_get_memory_path_state(MT_AFE_DIGITAL_BLOCK_I2S_OUT_2))
				mt_afe_disable_2nd_i2s_out();
		}

		mt_afe_disable_memory_path(MT_AFE_DIGITAL_BLOCK_I2S_IN_2);
		if (!mt_afe_get_memory_path_state(MT_AFE_DIGITAL_BLOCK_I2S_IN_2))
			mt_afe_disable_2nd_i2s_in();

		if (priv->playback_mux == I2S1_DATA2_EXT_I2S0_TO_MTK_INTERFACE) {
			if (!mt_afe_get_memory_path_state(MT_AFE_DIGITAL_BLOCK_I2S_IN_2)) {
				mt_afe_set_connection(INTER_DISCONNECT, INTER_CONN_I00, INTER_CONN_O03);
				mt_afe_set_connection(INTER_DISCONNECT, INTER_CONN_I01, INTER_CONN_O04);
			}
		} else {
			mt_afe_set_connection(INTER_DISCONNECT, INTER_CONN_I05, INTER_CONN_O00);
			mt_afe_set_connection(INTER_DISCONNECT, INTER_CONN_I06, INTER_CONN_O01);
		}

		priv->enable_i2s0 = false;

		if (priv->enable_i2s0_low_jitter) {
			mt_afe_disable_apll_div_power(MT_AFE_I2S0, runtime->rate);
			mt_afe_disable_apll_div_power(MT_AFE_ENGEN, runtime->rate);
			mt_afe_disable_apll_tuner(runtime->rate);
			mt_afe_disable_apll(runtime->rate);
			priv->enable_i2s0_low_jitter = false;
		}
	}

	mt_afe_enable_afe(false);
	return 0;
}

static int mt_pcm_dl1_trigger(struct snd_pcm_substream *substream, int cmd)
{
	pr_debug("%s cmd=%d\n", __func__, cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		return mt_pcm_dl1_start(substream);
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		return mt_pcm_dl1_stop(substream);
	}
	return -EINVAL;
}

static snd_pcm_uframes_t mt_pcm_dl1_pointer(struct snd_pcm_substream *substream)
{
	return mt_afe_update_hw_ptr(MT_AFE_MEM_CTX_DL1);
}

static struct snd_pcm_ops mt_pcm_dl1_ops = {
	.open = mt_pcm_dl1_open,
	.close = mt_pcm_dl1_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = mt_pcm_dl1_hw_params,
	.hw_free = mt_pcm_dl1_hw_free,
	.prepare = mt_pcm_dl1_prepare,
	.trigger = mt_pcm_dl1_trigger,
	.pointer = mt_pcm_dl1_pointer,
};

static const char *const mt_pcm_dl1_playback_mux_function[] = {
	ENUM_TO_STR(MTK_INTERFACE),
	ENUM_TO_STR(I2S0),
	ENUM_TO_STR(MTK_INTERFACE_AND_I2S0),
	ENUM_TO_STR(MTK_INTERFACE_AND_I2S1_DATA2),
	ENUM_TO_STR(I2S1_DATA2_EXT_I2S0_TO_MTK_INTERFACE)
};

static int dl1_playback_mux_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt_pcm_dl1_priv *priv = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = priv->playback_mux;
	return 0;
}

static int dl1_playback_mux_set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt_pcm_dl1_priv *priv = snd_soc_component_get_drvdata(component);

	priv->playback_mux = ucontrol->value.integer.value[0];
	return 0;
}

static const char *const mt_pcm_dl1_i2s0_clock_function[] = { "Normal", "Low Jitter" };

static int dl1_i2s0_clock_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt_pcm_dl1_priv *priv = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = priv->i2s0_clock_mode;
	return 0;
}

static int dl1_i2s0_clock_set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt_pcm_dl1_priv *priv = snd_soc_component_get_drvdata(component);

	priv->i2s0_clock_mode = ucontrol->value.integer.value[0];
	return 0;
}

static int dl1_i2s1_clock_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt_pcm_dl1_priv *priv = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = priv->i2s1_clock_mode;
	return 0;
}

static int dl1_i2s1_clock_set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt_pcm_dl1_priv *priv = snd_soc_component_get_drvdata(component);

	priv->i2s1_clock_mode = ucontrol->value.integer.value[0];
	return 0;
}

static const struct soc_enum mt_pcm_dl1_control_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt_pcm_dl1_playback_mux_function),
			mt_pcm_dl1_playback_mux_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt_pcm_dl1_i2s0_clock_function),
			mt_pcm_dl1_i2s0_clock_function),
};

static const struct snd_kcontrol_new mt_pcm_dl1_controls[] = {
	SOC_ENUM_EXT("DL1_Playback_Mux", mt_pcm_dl1_control_enum[0],
		dl1_playback_mux_get, dl1_playback_mux_set),
	SOC_ENUM_EXT("DL1_I2S0_Clock", mt_pcm_dl1_control_enum[1],
		dl1_i2s0_clock_get, dl1_i2s0_clock_set),
	SOC_ENUM_EXT("DL1_I2S1_Clock", mt_pcm_dl1_control_enum[1],
		dl1_i2s1_clock_get, dl1_i2s1_clock_set),
};

static int mt_pcm_dl1_probe(struct snd_soc_platform *platform)
{
	snd_soc_add_platform_controls(platform, mt_pcm_dl1_controls,
				ARRAY_SIZE(mt_pcm_dl1_controls));
	return 0;
}

static struct snd_soc_platform_driver mt_pcm_dl1_platform = {
	.ops = &mt_pcm_dl1_ops,
	.probe = mt_pcm_dl1_probe,
};

static int mt_pcm_dl1_dev_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mt_pcm_dl1_priv *priv;
	int rc;

	pr_debug("%s dev name %s\n", __func__, dev_name(dev));

	rc = dma_set_mask(dev, DMA_BIT_MASK(33));
	if (rc)
		return rc;

	if (dev->of_node) {
		dev_set_name(dev, "%s", MT_SOC_DL1_PCM);
		pr_debug("%s set dev name %s\n", __func__, dev_name(dev));
	}

	priv = devm_kzalloc(dev, sizeof(struct mt_pcm_dl1_priv), GFP_KERNEL);
	if (unlikely(!priv)) {
		pr_err("%s failed to allocate private data\n", __func__);
		return -ENOMEM;
	}

	priv->i2s0_clock_mode = MT_AFE_NORMAL_CLOCK;

	dev_set_drvdata(dev, priv);

	return snd_soc_register_platform(dev, &mt_pcm_dl1_platform);
}

static int mt_pcm_dl1_dev_remove(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

static const struct of_device_id mt_pcm_dl1_dt_match[] = {
	{.compatible = "mediatek," MT_SOC_DL1_PCM,},
	{}
};

MODULE_DEVICE_TABLE(of, mt_pcm_dl1_dt_match);

static struct platform_driver mt_pcm_dl1_driver = {
	.driver = {
		   .name = MT_SOC_DL1_PCM,
		   .owner = THIS_MODULE,
		   .of_match_table = mt_pcm_dl1_dt_match,
		   },
	.probe = mt_pcm_dl1_dev_probe,
	.remove = mt_pcm_dl1_dev_remove,
};

module_platform_driver(mt_pcm_dl1_driver);

MODULE_DESCRIPTION("AFE PCM DL1 platform driver");
MODULE_LICENSE("GPL");
