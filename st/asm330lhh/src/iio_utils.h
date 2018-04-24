/* IIO - useful set of util functionality
 *
 * Copyright (c) 2008 Jonathan Cameron
 * Modified by Denis Ciocca <denis.ciocca@st.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

#ifndef IIO_UTILS
#define IIO_UTILS

#include <linux/iio/types.h>

#ifdef PLTF_LINUX_ENABLED
#include <stdint.h>
#endif /* PLTF_LINUX_ENABLED */

#define IIO_MAX_NAME_LENGTH				(70)
#define IIO_UTILS_MAX_SAMP_FREQ_AVAILABLE		(10)
#define IIO_UTILS_SCALE_AVAILABLE			(10)

/*
 * iio_scale_available: IIO sensor scale informations
 * @values: scale value to convert raw data into IIO framework standard units.
 * @num_available: number of IIO channels available.
 */
struct iio_scale_available {
	float values[IIO_UTILS_SCALE_AVAILABLE];
	unsigned int num_available;
};

/*
 * iio_sampling_frequency_available: IIO sensor samplingg frequency available
 * @hz: frequencies list available.
 * @num_available: number of frequencies available.
 */
struct iio_sampling_frequency_available {
	unsigned int hz[IIO_UTILS_MAX_SAMP_FREQ_AVAILABLE];
	unsigned int num_available;
};

/*
 * iio_device: IIO devive info
 * @dev_num: IIO device number.
 * @name: IIO device name.
 */
struct iio_device {
	unsigned int dev_num;
	char name[IIO_MAX_NAME_LENGTH];
};

/**
 * iio_channel_info: information about a given channel
 * @name: channel name.
 * @generic_name: general name for channel type.
 * @scale: scale factor to be applied for conversion to si units.
 * @offset: offset to be applied for conversion to si units.
 * @index: the channel index in the buffer output.
 * @bytes: number of bytes occupied in buffer output.
 * @bits_used: number of valid bit in data.
 * @shift: number of bit to shift in data.
 * @mask: a bit mask for the raw output.
 * @be: channel use big endian format.
 * @is_signed: is the raw value stored signed.
 * @enabled: is this channel enabled.
 * @location: where data are located in channel data.
 **/
struct iio_channel_info {
	char *name;
	char *generic_name;
	float scale;
	float offset;
	unsigned index;
	unsigned bytes;
	unsigned bits_used;
	unsigned shift;
	uint64_t mask;
	unsigned be;
	unsigned is_signed;
	unsigned enabled;
	unsigned location;
};

/**
 * iio_utils_build_channel_array() - Figure out which channels are present
 * @device_dir: the IIO device directory in sysfs.
 * @ci_array: where store the channel informations.
 * @counter: number of valid channels found.
 *
 * Return value: 1 on success, negative number on fail.
 **/
int iio_utils_build_channel_array(const char *device_dir,
				struct iio_channel_info **ci_array, int *counter);

/**
 * iio_utils_get_devices_name() - Get all the IIO devices found on system
 * @devices: IIO devices found informations.
 * @max_list: max number of devices to get informations.
 *
 * Return value: IIO device found number, negative number on fail.
 **/
int iio_utils_get_devices_name(struct iio_device devices[], unsigned int max_list);

/**
 * iio_utils_get_sampling_frequency_available() - Get sampling frequency available list from a generic device
 * @device_dir: the sysfs directory of IIO device attributes.
 * @sfa: sampling frequency available data.
 *
 * Return value: number of sampling frequency available, negative number on fail.
 **/
int iio_utils_get_sampling_frequency_available(const char *device_dir,
					struct iio_sampling_frequency_available *sfa);

/**
 * iio_utils_set_sampling_frequency() - Set sampling frequency to a generic device
 * @device_dir: the sysfs directory of IIO device attributes.
 * @frequency: sampling frequency value.				[hz]
 *
 * Return value: 0 on success, negative number on fail.
 **/
int iio_utils_set_sampling_frequency(const char *device_dir, unsigned int frequency);

/**
 * iio_utils_set_max_delivery_rate() - Set max delivery rate to a generic device
 * @device_dir: the sysfs directory of IIO device attributes.
 * @deelay: rate value.							[ms]
 *
 * Return value: 0 on success, negative number on fail.
 **/
int iio_utils_set_max_delivery_rate(const char *device_dir, unsigned int deelay);

/**
 * iio_utils_get_scale_available() - Get scale factory list from a generic device
 * @device_dir: the sysfs directory of IIO device attributes.
 * @sa: scale factor data.
 * @device_type: IIO sensor type.
 *
 * Return value: number of scale available, negative number on fail.
 **/
int iio_utils_get_scale_available(const char *device_dir, struct iio_scale_available *sa, enum iio_chan_type device_type);

/**
 * iio_utils_set_scale() - Set scale factory value to a generic device [same meaning of change full-scale value]
 * @device_dir: the sysfs directory of IIO device attributes.
 * @sa: scale factor data.
 * @device_type: IIO sensor type.
 *
 * Return value: 0 on success, negative number on fail.
 **/
int iio_utils_set_scale(const char *device_dir, float value, enum iio_chan_type device_type);

/**
 * iio_utils_get_hw_fifo_length() - Get FIFO size from a generic device
 * @device_dir: the sysfs directory of IIO device attributes.
 *
 * Return value: FIFO size (number of elements), negative number on fail.
 **/
int iio_utils_get_hw_fifo_length(const char *device_dir);

/**
 * iio_utils_set_hw_fifo_watermark() - Set FIFO watermark to a generic device
 * @device_dir: the sysfs directory of IIO device attributes.
 * @watermark: watermark value.
 *
 * Return value: 0 on success, negative number on fail.
 **/
int iio_utils_set_hw_fifo_watermark(const char *device_dir, unsigned int watermark);

/**
 * iio_utils_hw_fifo_flush() - Flush FIFO of generic device
 * @device_dir: the sysfs directory of IIO device attributes.
 *
 * Return value: 0 on success, negative number on fail.
 **/
int iio_utils_hw_fifo_flush(const char *device_dir);

/**
 * iio_utils_enable_sensor() - Enable/Disable a generic device
 * @device_dir: the sysfs directory of IIO device attributes.
 * @bool: enable/disable device.
 *
 * Return value: 0 on success, negative number on fail.
 **/
int iio_utils_enable_sensor(const char *device_dir, bool enable);

/**
 * iio_utils_support_injection_mode() - Injection of a generic device supported?
 * @device_dir: the sysfs directory of IIO device attributes.
 * @bool: enable/disable injection mode.
 *
 * Return value: 0 if sensor is injector, 1 if injected, negative number on fail.
 **/
int iio_utils_support_injection_mode(const char *device_dir);

/**
 * iio_utils_set_injection_mode() - Set/Unset injection of a generic device
 * @device_dir: the sysfs directory of IIO device attributes.
 * @bool: enable/disable injection mode.
 *
 * Return value: 0 on success, negative number on fail.
 **/
int iio_utils_set_injection_mode(const char *device_dir, bool enable);

/**
 * iio_utils_inject_data() - Inject data to a generic device
 * @device_dir: the sysfs directory of IIO device attributes.
 * @data: sensor data to inject.
 * @len: data length to inject.
 * @device_type: IIO sensor type.
 *
 * Return value: 0 on success, negative number on fail.
 **/
int iio_utils_inject_data(const char *device_dir, uint8_t *data, int len, enum iio_chan_type device_type);

/**
 * iio_utils_get_selftest_available() - Get selftest available list of generic device
 * @device_dir: the sysfs directory of IIO device attributes.
 * @list: selftest available list string.
 *
 * Return value: 0 on success, negative number on fail.
 **/
int iio_utils_get_selftest_available(const char *device_dir, char list[][20]);

/**
 * iio_utils_execute_selftest() - execute selftest on generic device
 * @device_dir: the sysfs directory of IIO device attributes.
 *
 * Return value: 1 on selftest pass, 0 on selftest failure and negative number on fail.
 **/
int iio_utils_execute_selftest(const char *device_dir, char *mode);

#endif /* IIO_UTILS */
