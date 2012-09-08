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

struct epm_psoc_init_resp {
	u8	cmd;
	u8	version;
	u8	compatible_ver;
	u8	firm_ver[3];
	u8	num_dev;
	u8	num_channel;
};

struct epm_psoc_channel_configure {
	u8		cmd;
	u8		device_num;
	uint32_t	channel_num;
};

struct epm_psoc_set_avg {
	u8	cmd;
	u8	avg_period;
	u8	return_code;
};

struct epm_psoc_get_data {
	u8		cmd;
	u8		dev_num;
	u8		chan_num;
	uint32_t	timestamp_resp_value;
	uint32_t	reading_value;
};

struct epm_psoc_get_buffered_data {
	u8		cmd;
	u8		dev_num;
	u8		status_mask;
	u8		chan_idx;
	uint32_t	chan_mask;
	uint32_t	timestamp_start;
	uint32_t	timestamp_end;
	u8		buff_data[48];
};

struct epm_psoc_system_time_stamp {
	u8		cmd;
	uint32_t	timestamp;
};

struct epm_psoc_set_channel {
	u8		cmd;
	u8		dev_num;
	uint32_t	channel_mask;
};

struct epm_psoc_get_avg_buffered_switch_data {
	u8		cmd;
	u8		status;
	uint32_t	timestamp_start;
	uint32_t	channel_mask;
	u8		avg_data[54];
};

struct epm_psoc_set_channel_switch {
	u8		cmd;
	u8		dev;
	uint32_t	delay;
};

struct epm_psoc_set_vadc {
	u8		cmd;
	u8		vadc_dev;
	uint32_t	vadc_voltage;
};

#ifdef __KERNEL__
struct epm_chan_properties {
	uint32_t resistorvalue;
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
#endif

#define EPM_ADC_IOCTL_CODE		0x91

#define EPM_ADC_REQUEST		_IOWR(EPM_ADC_IOCTL_CODE, 1,	\
					struct epm_chan_request)

#define EPM_ADC_INIT		_IOR(EPM_ADC_IOCTL_CODE, 2,	\
					     uint32_t)

#define EPM_ADC_DEINIT		_IOR(EPM_ADC_IOCTL_CODE, 3,	\
					     uint32_t)

#define EPM_PSOC_ADC_INIT		_IOR(EPM_ADC_IOCTL_CODE, 4, \
					struct epm_psoc_init_resp)

#define EPM_PSOC_ADC_CHANNEL_ENABLE	_IOWR(EPM_ADC_IOCTL_CODE, 5, \
					struct epm_psoc_channel_configure)

#define EPM_PSOC_ADC_CHANNEL_DISABLE	_IOWR(EPM_ADC_IOCTL_CODE, 6, \
					struct epm_psoc_channel_configure)

#define EPM_PSOC_ADC_SET_AVERAGING	_IOWR(EPM_ADC_IOCTL_CODE, 7, \
					struct epm_psoc_set_avg)

#define EPM_PSOC_ADC_GET_LAST_MEASUREMENT	_IOWR(EPM_ADC_IOCTL_CODE, 8, \
						struct epm_psoc_get_data)

#define EPM_PSOC_ADC_GET_BUFFERED_DATA		_IOWR(EPM_ADC_IOCTL_CODE, 9, \
					struct epm_psoc_get_buffered_data)

#define EPM_PSOC_ADC_GET_SYSTEM_TIMESTAMP	_IOWR(EPM_ADC_IOCTL_CODE, 10, \
					struct epm_psoc_system_time_stamp)

#define EPM_PSOC_ADC_SET_SYSTEM_TIMESTAMP	_IOWR(EPM_ADC_IOCTL_CODE, 11, \
					struct epm_psoc_system_time_stamp)

#define EPM_PSOC_ADC_GET_AVERAGE_DATA		_IOWR(EPM_ADC_IOCTL_CODE, 12, \
				struct epm_psoc_get_avg_buffered_switch_data)

#define EPM_PSOC_SET_CHANNEL_SWITCH		_IOWR(EPM_ADC_IOCTL_CODE, 13, \
					struct epm_psoc_set_channel_switch)

#define EPM_PSOC_CLEAR_BUFFER			_IOWR(EPM_ADC_IOCTL_CODE, 14, \
						uint32_t)

#define EPM_PSOC_ADC_SET_VADC_REFERENCE		_IOWR(EPM_ADC_IOCTL_CODE, 15, \
						struct epm_psoc_set_vadc)

#endif /* __EPM_ADC_H */
