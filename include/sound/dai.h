/* Copyright (c) 2010-2011, The Linux Foundation. All rights reserved.
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
#ifndef __DAI_H__
#define __DAI_H__

struct dai_dma_params {
	u8 *buffer;
	uint32_t src_start;
	uint32_t bus_id;
	int buffer_size;
	int period_size;
	int channels;
};

enum {
	DAI_SPKR = 0,
	DAI_MIC,
	DAI_MI2S,
	DAI_SEC_SPKR,
	DAI_SEC_MIC,
};

/* Function Prototypes */
int dai_open(uint32_t dma_ch);
void dai_close(uint32_t dma_ch);
int dai_start(uint32_t dma_ch);
int dai_stop(uint32_t dma_ch);
int dai_set_params(uint32_t dma_ch, struct dai_dma_params *params);
uint32_t dai_get_dma_pos(uint32_t dma_ch);
void register_dma_irq_handler(int dma_ch,
		irqreturn_t (*callback) (int intrSrc, void *private_data),
		void *private_data);
void unregister_dma_irq_handler(int dma_ch);
void dai_set_master_mode(uint32_t dma_ch, int mode);
int dai_start_hdmi(uint32_t dma_ch);
int wait_for_dma_cnt_stop(uint32_t dma_ch);
void dai_stop_hdmi(uint32_t dma_ch);

#endif
