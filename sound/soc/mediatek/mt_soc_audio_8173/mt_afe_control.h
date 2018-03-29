/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
 * Copyright (C) 2018 XiaoMi, Inc.
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

#ifndef __MT_AFE_CONTROL_H__
#define __MT_AFE_CONTROL_H__

#include "mt_afe_digital_type.h"
#include "mt_afe_connection.h"
#include <sound/pcm.h>


int mt_afe_platform_init(void *dev);
void mt_afe_platform_deinit(void *dev);

void mt_afe_set_sample_rate(uint32_t aud_block, uint32_t sample_rate);
void mt_afe_set_channels(uint32_t memory_interface, uint32_t channel);
void mt_afe_set_mono_type(uint32_t memory_interface, uint32_t mono_type);

void mt_afe_set_irq_counter(uint32_t irq_mode, uint32_t counter);
void mt_afe_set_irq_rate(uint32_t irq_mode, uint32_t sample_rate);
void mt_afe_set_irq_state(uint32_t irq_mode, bool enable);
int mt_afe_get_irq_state(uint32_t irq_mode, struct mt_afe_irq_status *mcu_mode);

int mt_afe_enable_memory_path(uint32_t block);
int mt_afe_disable_memory_path(uint32_t block);
bool mt_afe_get_memory_path_state(uint32_t block);

void mt_afe_set_i2s_dac_out(uint32_t sample_rate, uint32_t clock_mode, uint32_t wlen);
int mt_afe_enable_i2s_dac(void);
int mt_afe_disable_i2s_dac(void);
void mt_afe_enable_afe(bool enable);

void mt_afe_set_mtkif_adc_in(uint32_t sample_rate);
void mt_afe_enable_mtkif_adc(void);
void mt_afe_disable_mtkif_adc(void);
void mt_afe_set_i2s_adc_in(uint32_t sample_rate, uint32_t clock_mode);
void mt_afe_enable_i2s_adc(void);
void mt_afe_disable_i2s_adc(void);
void mt_afe_set_i2s_adc2_in(uint32_t sample_rate, uint32_t clock_mode);
void mt_afe_enable_i2s_adc2(void);
void mt_afe_disable_i2s_adc2(void);

void mt_afe_set_2nd_i2s_out(uint32_t sample_rate, uint32_t clock_mode, uint32_t wlen);
int mt_afe_enable_2nd_i2s_out(void);
int mt_afe_disable_2nd_i2s_out(void);
void mt_afe_set_2nd_i2s_in(uint32_t wlen, uint32_t src_mode,
			uint32_t bck_inv, uint32_t clock_mode);
int mt_afe_enable_2nd_i2s_in(void);
int mt_afe_disable_2nd_i2s_in(void);
void mt_afe_set_i2s_asrc_config(unsigned int sample_rate);

void mt_afe_set_hw_digital_gain_mode(uint32_t gain_type, uint32_t sample_rate,
						uint32_t sample_per_step);
void mt_afe_set_hw_digital_gain_state(int gain_type, bool enable);
void mt_afe_set_hw_digital_gain(uint32_t gain, int gain_type);
int mt_afe_enable_sinegen_hw(uint32_t connection, uint32_t direction);
int mt_afe_disable_sinegen_hw(void);

void mt_afe_set_memif_fetch_format(uint32_t interface_type, uint32_t fetch_format);
void mt_afe_set_out_conn_format(uint32_t connection_format, uint32_t output);

void mt_afe_enable_apll(uint32_t sample_rate);
void mt_afe_disable_apll(uint32_t sample_rate);
void mt_afe_enable_apll_tuner(uint32_t sample_rate);
void mt_afe_disable_apll_tuner(uint32_t sample_rate);
void mt_afe_enable_apll_div_power(uint32_t clock_type, uint32_t sample_rate);
void mt_afe_disable_apll_div_power(uint32_t clock_type, uint32_t sample_rate);
uint32_t mt_afe_set_mclk(uint32_t clock_type, uint32_t sample_rate);
void mt_afe_set_i2s3_bclk(uint32_t mck_div, uint32_t sample_rate, uint32_t channels,
			uint32_t sample_bits);

void mt_afe_set_dai_bt(struct mt_afe_digital_dai_bt *dai_bt);
int mt_afe_enable_dai_bt(void);
int mt_afe_disable_dai_bt(void);
int mt_afe_enable_merge_i2s(uint32_t sample_rate);
int mt_afe_disable_merge_i2s(void);
void mt_afe_suspend(void);
void mt_afe_resume(void);
struct mt_afe_mem_control_t *mt_afe_get_mem_ctx(enum mt_afe_mem_context mem_context);
void mt_afe_add_ctx_substream(enum mt_afe_mem_context mem_context,
			 struct snd_pcm_substream *substream);
void mt_afe_remove_ctx_substream(enum mt_afe_mem_context mem_context);
void mt_afe_init_dma_buffer(enum mt_afe_mem_context mem_context,
			struct snd_pcm_runtime *runtime);
void mt_afe_reset_dma_buffer(enum mt_afe_mem_context mem_context);
int mt_afe_update_hw_ptr(enum mt_afe_mem_context mem_context);

unsigned int mt_afe_get_board_channel_type(void);
void mt_afe_set_pcmif_asrc(struct mt_afe_pcm_info *pcm_info);
void mt_afe_enable_pcmif_asrc(struct mt_afe_pcm_info *pcm_info);
void mt_afe_disable_pcmif_asrc(void);
void mt_afe_set_pcmif(struct mt_afe_pcm_info *pcm_info);
void mt_afe_enable_pcmif(bool enable);
void mt_afe_set_hdmi_out_channel(unsigned int channels);
int mt_afe_enable_hdmi_out(void);
int mt_afe_disable_hdmi_out(void);
void mt_afe_set_hdmi_tdm1_config(unsigned int channels, unsigned int i2s_wlen);
void mt_afe_set_hdmi_tdm2_config(unsigned int channels);
int mt_afe_enable_hdmi_tdm(void);
int mt_afe_disable_hdmi_tdm(void);
int mt_afe_enable_hdmi_tdm_i2s_loopback(void);
int mt_afe_disable_hdmi_tdm_i2s_loopback(void);
void mt_afe_set_hdmi_tdm_i2s_loopback_data(unsigned int sdata_index);
void mt_afe_set_8173_mclk(bool enable);
void mt_afe_set_init(void);

#endif
