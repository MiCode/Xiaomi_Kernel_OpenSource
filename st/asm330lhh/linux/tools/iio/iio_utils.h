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

#include <linux/ioctl.h>
#include <linux/types.h>

/**
 * struct iio_event_data - The actual event being pushed to userspace
 * @id:		event identifier
 * @timestamp:	best estimate of time of event occurrence (often from
 *		the interrupt handler)
 */
struct iio_event_data {
	__u64	id;
	__s64	timestamp;
};

#define IIO_GET_EVENT_FD_IOCTL _IOR('i', 0x90, int)

#define IIO_EVENT_CODE_EXTRACT_TYPE(mask) ((mask >> 56) & 0xFF)

#define IIO_EVENT_CODE_EXTRACT_DIR(mask) ((mask >> 48) & 0x7F)

#define IIO_EVENT_CODE_EXTRACT_CHAN_TYPE(mask) ((mask >> 32) & 0xFF)

/* Event code number extraction depends on which type of event we have.
 * Perhaps review this function in the future*/
#define IIO_EVENT_CODE_EXTRACT_CHAN(mask) ((__s16)(mask & 0xFFFF))
#define IIO_EVENT_CODE_EXTRACT_CHAN2(mask) ((__s16)(((mask) >> 16) & 0xFFFF))

#define IIO_EVENT_CODE_EXTRACT_MODIFIER(mask) ((mask >> 40) & 0xFF)
#define IIO_EVENT_CODE_EXTRACT_DIFF(mask) (((mask) >> 55) & 0x1)

enum iio_chan_type {
	IIO_VOLTAGE,
	IIO_CURRENT,
	IIO_POWER,
	IIO_ACCEL,
	IIO_ANGL_VEL,
	IIO_MAGN,
	IIO_LIGHT,
	IIO_INTENSITY,
	IIO_PROXIMITY,
	IIO_TEMP,
	IIO_INCLI,
	IIO_ROT,
	IIO_ANGL,
	IIO_TIMESTAMP,
	IIO_CAPACITANCE,
	IIO_ALTVOLTAGE,
	IIO_CCT,
	IIO_PRESSURE,
	IIO_HUMIDITYRELATIVE,
	IIO_ACTIVITY,
	IIO_STEPS,
	IIO_ENERGY,
	IIO_DISTANCE,
	IIO_VELOCITY,
	IIO_CONCENTRATION,
	IIO_RESISTANCE,
	IIO_SIGN_MOTION,
	IIO_STEP_DETECTOR,
	IIO_STEP_COUNTER,
	IIO_LINEAR_ACCEL,
	IIO_GRAVITY,
	IIO_TILT,
	IIO_TAP,
	IIO_TAP_TAP,
	IIO_WRIST_TILT_GESTURE,
	IIO_GESTURE,
};

enum iio_modifier {
	IIO_NO_MOD,
	IIO_MOD_X,
	IIO_MOD_Y,
	IIO_MOD_Z,
	IIO_MOD_X_AND_Y,
	IIO_MOD_X_AND_Z,
	IIO_MOD_Y_AND_Z,
	IIO_MOD_X_AND_Y_AND_Z,
	IIO_MOD_X_OR_Y,
	IIO_MOD_X_OR_Z,
	IIO_MOD_Y_OR_Z,
	IIO_MOD_X_OR_Y_OR_Z,
	IIO_MOD_LIGHT_BOTH,
	IIO_MOD_LIGHT_IR,
	IIO_MOD_ROOT_SUM_SQUARED_X_Y,
	IIO_MOD_SUM_SQUARED_X_Y_Z,
	IIO_MOD_LIGHT_CLEAR,
	IIO_MOD_LIGHT_RED,
	IIO_MOD_LIGHT_GREEN,
	IIO_MOD_LIGHT_BLUE,
	IIO_MOD_QUATERNION,
	IIO_MOD_TEMP_AMBIENT,
	IIO_MOD_TEMP_OBJECT,
	IIO_MOD_NORTH_MAGN,
	IIO_MOD_NORTH_TRUE,
	IIO_MOD_NORTH_MAGN_TILT_COMP,
	IIO_MOD_NORTH_TRUE_TILT_COMP,
	IIO_MOD_RUNNING,
	IIO_MOD_JOGGING,
	IIO_MOD_WALKING,
	IIO_MOD_STILL,
	IIO_MOD_ROOT_SUM_SQUARED_X_Y_Z,
	IIO_MOD_I,
	IIO_MOD_Q,
	IIO_MOD_CO2,
	IIO_MOD_VOC,
};

enum iio_event_type {
	IIO_EV_TYPE_THRESH,
	IIO_EV_TYPE_MAG,
	IIO_EV_TYPE_ROC,
	IIO_EV_TYPE_THRESH_ADAPTIVE,
	IIO_EV_TYPE_MAG_ADAPTIVE,
	IIO_EV_TYPE_CHANGE,
	IIO_EV_TYPE_FIFO_FLUSH,
};

enum iio_event_direction {
	IIO_EV_DIR_EITHER,
	IIO_EV_DIR_RISING,
	IIO_EV_DIR_FALLING,
	IIO_EV_DIR_NONE,
	IIO_EV_DIR_FIFO_EMPTY,
	IIO_EV_DIR_FIFO_DATA,
};

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
