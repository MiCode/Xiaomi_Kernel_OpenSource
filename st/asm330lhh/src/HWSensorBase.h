/*
 * Copyright (C) 2015-2016 STMicroelectronics
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

#ifndef ST_HWSENSOR_BASE_H
#define ST_HWSENSOR_BASE_H

#include <poll.h>
#include <math.h>

#include "SensorBase.h"

extern "C" {
	#include "iio_utils.h"
};

#define HW_SENSOR_BASE_DEFAULT_IIO_BUFFER_LEN	(2)
#define HW_SENSOR_BASE_IIO_SYSFS_PATH_MAX	(50)
#define HW_SENSOR_BASE_IIO_DEVICE_NAME_MAX	(30)
#define HW_SENSOR_BASE_MAX_CHANNELS		(8)

struct HWSensorBaseCommonData {
	char iio_sysfs_path[HW_SENSOR_BASE_IIO_SYSFS_PATH_MAX];
	char device_name[HW_SENSOR_BASE_IIO_DEVICE_NAME_MAX];
	unsigned int iio_dev_num;

	int num_channels;
	struct iio_channel_info channels[HW_SENSOR_BASE_MAX_CHANNELS];

	struct iio_scale_available sa;
} typedef HWSensorBaseCommonData;


#ifdef CONFIG_ST_HAL_HAS_SELFTEST_FUNCTIONS
struct selftest_data {
	unsigned int available;
	char mode[5][20];
};
#endif /* CONFIG_ST_HAL_HAS_SELFTEST_FUNCTIONS */

class HWSensorBase;
class HWSensorBaseWithPollrate;

/*
 * class HWSensorBase
 */
class HWSensorBase : public SensorBase {
protected:
	ssize_t scan_size;
	struct pollfd pollfd_iio[2];
	FlushRequested flush_requested;
	HWSensorBaseCommonData common_data;
	ChangeODRTimestampStack odr_switch;
#ifdef CONFIG_ST_HAL_FACTORY_CALIBRATION
	bool factory_calibration_updated;
	float factory_offset[3];
	float factory_scale[3];
#endif /* CONFIG_ST_HAL_FACTORY_CALIBRATION */
#ifdef CONFIG_ST_HAL_HAS_SELFTEST_FUNCTIONS
	struct selftest_data selftest;
#endif /* CONFIG_ST_HAL_HAS_SELFTEST_FUNCTIONS */
#if (CONFIG_ST_HAL_ANDROID_VERSION >= ST_HAL_MARSHMALLOW_VERSION)
	uint8_t *injection_data;
#endif /* CONFIG_ST_HAL_ANDROID_VERSION */
	bool has_event_channels;

	int WriteBufferLenght(unsigned int buf_len);

public:
	HWSensorBase(HWSensorBaseCommonData *data, const char *name,
				int handle, int sensor_type, unsigned int hw_fifo_len,
				float power_consumption);
	virtual ~HWSensorBase();

#ifdef CONFIG_ST_HAL_HAS_SELFTEST_FUNCTIONS
	virtual selftest_status ExecuteSelfTest();
	void GetSelfTestAvailable();
#endif /* CONFIG_ST_HAL_HAS_SELFTEST_FUNCTIONS */

	virtual int Enable(int handle, bool enable, bool lock_en_mute);
	virtual int SetDelay(int handle, int64_t period_ns, int64_t timeout, bool lock_en_mute);

	virtual int AddSensorDependency(SensorBase *p);
	virtual void RemoveSensorDependency(SensorBase *p);

	int ApplyFactoryCalibrationData(char *filename, time_t *last_modification);

	virtual void ProcessEvent(struct iio_event_data *event_data);
	virtual int FlushData(int handle, bool lock_en_mute);
	virtual void ProcessFlushData(int handle, int64_t timestamp);
	virtual void ThreadDataTask();
	virtual void ThreadEventsTask();

#if (CONFIG_ST_HAL_ANDROID_VERSION >= ST_HAL_MARSHMALLOW_VERSION)
	virtual int InjectionMode(bool enable);
	virtual int InjectSensorData(const sensors_event_t *data);
#endif /* CONFIG_ST_HAL_ANDROID_VERSION */
	bool hasEventChannels() { return has_event_channels; }
	bool hasDataChannels() { return common_data.num_channels > 0; }
};


/*
 * class HWSensorBaseWithPollrate
 */
class HWSensorBaseWithPollrate : public HWSensorBase {
private:
	struct iio_sampling_frequency_available sampling_frequency_available;

public:
	HWSensorBaseWithPollrate(HWSensorBaseCommonData *data, const char *name,
			struct iio_sampling_frequency_available *sfa, int handle,
			int sensor_type, unsigned int hw_fifo_len,
			float power_consumption);
	virtual ~HWSensorBaseWithPollrate();

	virtual int SetDelay(int handle, int64_t period_ns, int64_t timeout, bool lock_en_mute);
	virtual int FlushData(int handle, bool lock_en_mute);
	virtual void WriteDataToPipe(int64_t hw_pollrate);
};

#endif /* ST_HWSENSOR_BASE_H */
