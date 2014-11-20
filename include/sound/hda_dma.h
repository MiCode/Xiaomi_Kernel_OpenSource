/*
 *  soc-hda-dma.h - Generic dma header
 *
 *  Copyright (c) 2014 Intel Corporation
 *  Author: Jeeja KP <jeeja.kp@intel.com>
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 *  more details.
 *
 */
#ifndef _SOUND_HDA_DMA_H_
#define _SOUND_HDA_DMA_H_

struct dma_desc {
	int fmt;
	unsigned int playback:1;
	unsigned int id:4;
	unsigned int started:1;
	unsigned int padding:26;
};

void azx_set_dma_decouple_mode(struct azx *chip);
int azx_link_dma_reset(struct azx *chip, struct azx_dev *azx_dev);
void azx_host_dma_stop(struct azx *chip, struct azx_dev *azx_dev);
void azx_host_dma_start(struct azx *chip, struct azx_dev *azx_dev);

int azx_alloc_dma_locked(struct azx *chip, struct dma_desc *dma);
int azx_release_dma_locked(struct azx *chip, struct dma_desc *dma);
int azx_dma_decouple(struct azx *chip, struct azx_dev *azx_dev,
			bool decouple);
int azx_link_dma_reset_locked(struct azx *chip, struct dma_desc *dma);
int azx_link_dma_run_ctrl(struct azx *chip, struct azx_dev *azx_dev, bool run);
int azx_link_dma_set_stream_id(struct azx *chip, struct azx_dev *azx_dev);
int azx_link_dma_set_format(struct azx *chip, struct azx_dev *azx_dev, int fmt);
int azx_host_dma_reset_locked(struct azx *chip, struct dma_desc *dma);
void azx_host_dma_reset(struct azx *chip, struct azx_dev *azx_dev);
int azx_host_dma_run_ctrl_locked(struct azx *chip, struct dma_desc *dma,
				bool run);
void azx_host_dma_set_stream_id(struct azx *chip,
				struct azx_dev *azx_dev);
void azx_host_dma_set_format(struct azx *chip, struct azx_dev *azx_dev,
			 int fmt);

#endif /* _SOC__HDA_DMA_H__*/
