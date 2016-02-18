#ifndef __EPM_ADC_H
#define __EPM_ADC_H

#include <linux/i2c.h>
#include <uapi/linux/epm_adc.h>

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
#endif /* __EPM_ADC_H */
