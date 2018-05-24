/*
 * STMicroelectronics iio_utils header for SensorHAL
 *
 * Copyright 2015-2018 STMicroelectronics Inc.
 * Author: Denis Ciocca - <denis.ciocca@st.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef __IIO_UTILS
#define __IIO_UTILS

#include <sys/ioctl.h>
#include <stdint.h>
#include <linux/iio/types.h>

#define IIO_MAX_NAME_LENGTH			70
#define IIO_UTILS_MAX_SAMP_FREQ_AVAILABLE	10
#define IIO_UTILS_SCALE_AVAILABLE		10

#define FORMAT_SCAN_ELEMENTS_DIR		"%s/scan_elements"
#define FORMAT_TYPE_FILE			"%s_type"
#define FORMAT_EVENT_ELEMENT_DIR		"%s/events"
#define SELFTEST_POSITIVE_RESULT		"pass"
#define SELFTEST_NEGATIVE_RESULT		"fail"

#define IIO_GET_EVENT_FD_IOCTL 			_IOR('i', 0x90, int)
#define IIO_EVENT_CODE_EXTRACT_TYPE(mask) 	((mask >> 56) & 0xFF)
#define IIO_EVENT_CODE_EXTRACT_DIR(mask) 	((mask >> 48) & 0x7F)
#define IIO_EVENT_CODE_EXTRACT_CHAN_TYPE(mask)	((mask >> 32) & 0xFF)

/*
 * Event code number extraction depends on which type of event we have.
 * Perhaps review this function in the future
 */
#define IIO_EVENT_CODE_EXTRACT_CHAN(mask)	((__s16)(mask & 0xFFFF))
#define IIO_EVENT_CODE_EXTRACT_CHAN2(mask)	((__s16)(((mask) >> 16) & 0xFFFF))
#define IIO_EVENT_CODE_EXTRACT_MODIFIER(mask)	((mask >> 40) & 0xFF)
#define IIO_EVENT_CODE_EXTRACT_DIFF(mask)	(((mask) >> 55) & 0x1)

struct iio_scale_available {
	float values[IIO_UTILS_SCALE_AVAILABLE];
	unsigned int num_available;
};

struct iio_sampling_frequency_available {
	unsigned int hz[IIO_UTILS_MAX_SAMP_FREQ_AVAILABLE];
	unsigned int num_available;
};

struct iio_device {
	unsigned int dev_num;
	char name[IIO_MAX_NAME_LENGTH];
};

struct iio_channel_info {
	char *name;
	char *generic_name;
	float scale;
	float offset;
	unsigned index;
	unsigned bytes;
	unsigned bits_used;
	unsigned shift;
	unsigned long long mask;
	unsigned be;
	unsigned is_signed;
	unsigned enabled;
	unsigned location;
};

class iio_utils {
	private:
		static inline int iio_utils_check_syfs_file(const char *filename,
							    const char *basedir);
		static int iio_utils_write_sysfs_int(char *filename,
						     char *basedir,
						     int val);
		static int iio_utils_write_sysfs_int_and_verify(char *filename,
								char *basedir,
								int val,
								bool verify);
		static int iio_utils_write_sysfs_int_and_verify(char *filename,
								char *basedir,
								int val);
		static inline int iio_utils_break_up_name(const char *full_name,
							  char **generic_name);
		static inline int iio_utils_get_type(unsigned *is_signed,
						     unsigned *bytes,
						     unsigned *bits_used,
						     unsigned *shift,
						     unsigned long long *mask,
						     unsigned *be,
						     const char *device_dir,
						     const char *name,
						     const char *generic_name);
		static inline int iio_utils_get_param_float(float *output,
							    const char *param_name,
							    const char *device_dir,
							    const char *name,
							    const char *generic_name);
		static inline void iio_utils_sort_channel_array_by_index(
						struct iio_channel_info **ci_array,
						int cnt);
		static int iio_utils_enable_event_channels(const char *device_dir,
							   bool enable);
		static int iio_utils_write_sysfs_float(char *filename,
						       char *basedir,
						       float val,
						       bool verify);
		static int iio_utils_write_sysfs_float_and_verify(char *filename,
								  char *basedir,
								  float val);
		static int iio_utils_read_sysfs_posint(char *filename,
						       char *basedir);
		static int iio_utils_get_selftest_available(const char *device_dir,
							    char list[][20]);
		static int iio_utils_execute_selftest(const char *device_dir,
						      char *mode);
		static int iio_utils_read_sysfs_string(char *filename,
						       char *basedir,
						       char *str);
		static int iio_utils_write_sysfs_byte(char *filename,
						      char *basedir,
						      unsigned char *data,
						      size_t len);
		static int iio_utils_write_sysfs_string(char *filename,
							char *basedir,
							char *val,
							bool verify);
		static int iio_utils_write_sysfs_string(char *filename,
							char *basedir,
							char *val);

	public:
		static int iio_utils_enable_sensor(const char *device_dir,
						   bool enable);
		static int iio_utils_support_injection_mode(const char *device_dir);
		static int iio_utils_set_injection_mode(const char *device_dir,
							bool enable);
		static int iio_utils_inject_data(const char *device_dir,
						 unsigned char *data, int len,
						 enum iio_chan_type device_type);
		static int iio_utils_get_hw_fifo_length(const char *device_dir);
		static int iio_utils_set_hw_fifo_watermark(const char *device_dir,
							   unsigned int watermark);
		static int iio_utils_hw_fifo_flush(const char *device_dir);
		static int iio_utils_set_scale(const char *device_dir,
					       float value,
					       enum iio_chan_type device_type);
		static int iio_utils_get_scale_available(const char *device_dir,
							 struct iio_scale_available *sa,
							 enum iio_chan_type device_type);
		static int iio_utils_get_sampling_frequency_available(const char *device_dir,
					struct iio_sampling_frequency_available *sfa);
		static int iio_utils_set_sampling_frequency(const char *device_dir,
							    unsigned int frequency);
		static int iio_utils_set_max_delivery_rate(const char *device_dir,
							   unsigned int deelay);
		static int iio_utils_build_channel_array(const char *device_dir,
				struct iio_channel_info **ci_array, int *counter);
		static int iio_utils_get_devices_name(struct iio_device devices[],
						      unsigned int max_list);
};

#endif /* __IIO_UTILS */
