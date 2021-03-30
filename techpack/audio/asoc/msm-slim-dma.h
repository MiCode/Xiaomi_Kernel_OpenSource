/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2014, 2017 The Linux Foundation. All rights reserved.
 *
 */
#ifndef _MSM_SLIMBUS_DMA_H
#define _MSM_SLIMBUS_DMA_H

#include <linux/slimbus/slimbus.h>

/*
 * struct msm_slim_dma_data - DMA data for slimbus data transfer
 *
 * @sdev: Handle to the slim_device instance associated with the
 *	  data transfer.
 * @ph:	Port handle for the slimbus ports.
 * @dai_channel_ctl: callback function into the CPU dai driver
 *		     to setup the data path.
 *
 * This structure is used to share the slimbus port handles and
 * other data path setup related handles with other drivers.
 */
struct msm_slim_dma_data {

	/* Handle to slimbus device */
	struct slim_device *sdev;

	/* Port Handle */
	u32 ph;

	/* Callback for data channel control */
	int (*dai_channel_ctl)(struct msm_slim_dma_data *dma_data,
				struct snd_soc_dai *dai, bool enable);
};

#endif
