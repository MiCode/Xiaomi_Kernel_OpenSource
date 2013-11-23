#ifndef _UAPI_EPM_ADC_H
#define _UAPI_EPM_ADC_H

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
	uint8_t	cmd;
	uint8_t	version;
	uint8_t	compatible_ver;
	uint8_t	firm_ver[3];
	uint8_t	num_dev;
	uint8_t	num_channel;
};

struct epm_psoc_channel_configure {
	uint8_t		cmd;
	uint8_t		device_num;
	uint32_t	channel_num;
};

struct epm_psoc_set_avg {
	uint8_t	cmd;
	uint8_t	avg_period;
	uint8_t	return_code;
};

struct epm_psoc_get_data {
	uint8_t		cmd;
	uint8_t		dev_num;
	uint8_t		chan_num;
	uint32_t	timestamp_resp_value;
	int16_t		reading_raw;
	int32_t		reading_value;
};

struct epm_psoc_get_buffered_data {
	uint8_t		cmd;
	uint8_t		dev_num;
	uint8_t		status_mask;
	uint8_t		chan_idx;
	uint32_t	chan_mask;
	uint32_t	timestamp_start;
	uint32_t	timestamp_end;
	uint8_t		buff_data[48];
};

struct epm_psoc_system_time_stamp {
	uint8_t		cmd;
	uint32_t	timestamp;
};

struct epm_psoc_set_channel {
	uint8_t		cmd;
	uint8_t		dev_num;
	uint32_t	channel_mask;
};

struct result_buffer {
	uint32_t	channel;
	uint32_t	avg_buffer_sample;
	uint32_t	result;
};

struct epm_psoc_get_avg_buffered_switch_data {
	uint8_t			cmd;
	uint8_t			status;
	uint32_t		timestamp_start;
	uint32_t		channel_mask;
	uint8_t			avg_data[54];
	struct result_buffer	data[54];
};

struct epm_psoc_set_channel_switch {
	uint8_t		cmd;
	uint8_t		dev;
	uint32_t	delay;
};

struct epm_psoc_set_vadc {
	uint8_t		cmd;
	uint8_t		vadc_dev;
	uint32_t	vadc_voltage;
};

struct epm_chan_properties {
	uint32_t resistorvalue;
	uint32_t gain;
};

struct epm_marker_level {
	uint8_t		level;
};

struct epm_gpio_buffer_request {
	uint8_t		cmd;
	uint8_t		bitmask_monitor_pin;
	uint8_t		status;
};

struct epm_get_gpio_buffer_resp {
	uint8_t		cmd;
	uint8_t		status;
	uint8_t		bitmask_monitor_pin;
	uint32_t	timestamp;
};

struct epm_get_high_res_avg_data {
	uint8_t		cmd;
	uint8_t		status;
	uint32_t	channel_mask;
	uint32_t	timestamp;
	uint8_t		buf_data[54];
};

struct epm_generic_request {
	uint8_t		buf_data[65];
};


#define EPM_ADC_IOCTL_CODE		0x91

#define EPM_ADC_REQUEST		_IOWR(EPM_ADC_IOCTL_CODE, 1,	\
					struct epm_chan_request)

#define EPM_ADC_INIT		_IOR(EPM_ADC_IOCTL_CODE, 2,	\
					     uint32_t)

#define EPM_ADC_DEINIT		_IOR(EPM_ADC_IOCTL_CODE, 3,	\
					     uint32_t)

#define EPM_MARKER1_REQUEST	_IOR(EPM_ADC_IOCTL_CODE, 90,	\
						uint32_t)


#define EPM_MARKER1_RELEASE	_IOR(EPM_ADC_IOCTL_CODE, 91,	\
						uint32_t)

#define EPM_MARKER1_SET_LEVEL	_IOWR(EPM_ADC_IOCTL_CODE, 92,	\
						uint32_t)

#define EPM_MARKER2_REQUEST	_IOR(EPM_ADC_IOCTL_CODE, 93,	\
						uint32_t)

#define EPM_MARKER2_SET_LEVEL	_IOWR(EPM_ADC_IOCTL_CODE, 94,	\
						uint32_t)

#define EPM_MARKER2_RELEASE	_IOR(EPM_ADC_IOCTL_CODE, 95,	\
						uint32_t)

#define EPM_PSOC_ADC_INIT		_IOWR(EPM_ADC_IOCTL_CODE, 4, \
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

#define EPM_PSOC_ADC_DEINIT		_IOWR(EPM_ADC_IOCTL_CODE, 16,	\
							     uint32_t)

#define EPM_PSOC_GPIO_BUFFER_REQUEST	_IOWR(EPM_ADC_IOCTL_CODE, 17,	\
					struct epm_gpio_buffer_request)

#define EPM_PSOC_GET_GPIO_BUFFER_DATA	_IOWR(EPM_ADC_IOCTL_CODE, 18,	\
					struct epm_get_gpio_buffer_resp)

#define EPM_PSOC_PAUSE_CONVERSION_REQUEST _IOWR(EPM_ADC_IOCTL_CODE, 19,	\
								uint32_t)

#define EPM_PSOC_UNPAUSE_CONVERSION_REQUEST _IOWR(EPM_ADC_IOCTL_CODE, 20, \
								uint32_t)

#define EPM_PSOC_16_BIT_AVERAGED_REQUEST	_IOWR(EPM_ADC_IOCTL_CODE, 21, \
					struct epm_get_high_res_avg_data)

#define EPM_PSOC_GENERIC_REQUEST		_IOWR(EPM_ADC_IOCTL_CODE, 22, \
					struct epm_generic_request)

#endif /* _UAPI_EPM_ADC_H */
