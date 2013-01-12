/*
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#ifndef __EPM_ADC_H
#define __EPM_ADC_H

#include <linux/i2c.h>

struct epm_chan_request {
	/* EPM ADC device index. 0 - ADC1, 1 - ADC2 */
	uint32_t device_idx;
	/* Channel number within the EPM ADC device  */
	uint32_t channel_idx;
	/* The data meaningful for each individual channel whether it is
	 * voltage, current etc. */
	int32_t physical;
};

struct epm_chan_properties {
	uint32_t resistorValue;
	uint32_t gain;
};

struct epm_adc_platform_data {
	struct epm_chan_properties *channel;
	uint32_t num_channels;
	uint32_t num_adc;
	uint32_t chan_per_adc;
	uint32_t chan_per_mux;
	struct i2c_board_info epm_i2c_board_info;
	uint32_t bus_id;
	uint32_t gpio_expander_base_addr;
};

#define EPM_ADC_IOCTL_CODE		0x91

#define EPM_ADC_REQUEST		_IOWR(EPM_ADC_IOCTL_CODE, 1,	\
					struct epm_chan_request)

#define EPM_ADC_INIT		_IOR(EPM_ADC_IOCTL_CODE, 2,	\
					     uint32_t)

#define EPM_ADC_DEINIT		_IOR(EPM_ADC_IOCTL_CODE, 3,	\
					     uint32_t)
#endif /* __EPM_ADC_H */
