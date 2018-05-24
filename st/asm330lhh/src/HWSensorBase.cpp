/*
 * STMicroelectronics HW SensorBase Class
 *
 * Copyright 2015-2016 STMicroelectronics Inc.
 * Author: Denis Ciocca - <denis.ciocca@st.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 */

#define __STDC_LIMIT_MACROS
#define __STDINT_LIMITS
#define _BSD_SOURCE

#include <endian.h>
#include <fcntl.h>
#include <assert.h>
#include <string.h>
#include <signal.h>

#include "HWSensorBase.h"

#define HW_SENSOR_BASE_DELAY_TRANSFER_DATA		(500000000LL)

/**
 * size_from_channelarray() - Calculate the storage size of a scan
 * @channels: the channel info array.
 * @num_channels: number of channels.
 **/
static int size_from_channelarray(struct iio_channel_info *channels, int num_channels)
{
	int bytes = 0, i;

	for (i = 0; i < num_channels; i++) {
		if (channels[i].bytes == 0)
			continue;

		if (bytes % channels[i].bytes == 0)
			channels[i].location = bytes;
		else
			channels[i].location = bytes - (bytes % channels[i].bytes) + channels[i].bytes;

		bytes = channels[i].location + channels[i].bytes;
	}

	return bytes;
}

/**
 * process_2byte_received() - Return channel data from 2 byte
 * @input: 2 byte of data received from buffer channel.
 * @info: information about channel structure.
 **/
static float process_2byte_received(int input, struct iio_channel_info *info)
{
	float res;
	int16_t val;

	if (info->be)
		input = be16toh((uint16_t)input);
	else
		input = le16toh((uint16_t)input);

	val = input >> info->shift;

	if (info->is_signed) {
		val &= (1 << info->bits_used) - 1;
		val = (int16_t)(val << (16 - info->bits_used)) >> (16 - info->bits_used);
		res = (float)val;
	} else {
		val &= (1 << info->bits_used) - 1;
		res = (float)((uint16_t)val);
	}

	return ((res + info->offset) * info->scale);
}

/**
 * process_3byte_received() - Return channel data from 3 byte
 * @input: 3 byte of data received from buffer channel.
 * @info: information about channel structure.
 **/
static float process_3byte_received(int input, struct iio_channel_info *info)
{
	float res;
	int32_t val;

	if (info->be)
		input = be32toh((uint32_t)input);
	else
		input = le32toh((uint32_t)input);

	val = input >> info->shift;
	if (info->is_signed) {
		val &= (1 << info->bits_used) - 1;
		val = (int32_t)(val << (24 - info->bits_used)) >> (24 - info->bits_used);
		res = (float)val;
	} else {
		val &= (1 << info->bits_used) - 1;
		res = (float)((uint32_t)val);
	}

	return ((res + info->offset) * info->scale);
}

/**
 * process_scan() - This functions use channels device information to build data
 * @hw_sensor: pointer to current hardware sensor.
 * @data: sensor data of all channels read from buffer.
 * @channels: information about channel structure.
 * @num_channels: number of channels of the sensor.
 **/
static int ProcessScanData(uint8_t *data, struct iio_channel_info *channels, int num_channels, SensorBaseData *sensor_out_data)
{
	int k;

	for (k = 0; k < num_channels; k++) {

		sensor_out_data->offset[k] = 0;

		switch (channels[k].bytes) {
		case 1:
			sensor_out_data->raw[k] = *(uint8_t *)(data + channels[k].location);
			break;
		case 2:
			sensor_out_data->raw[k] = process_2byte_received(*(uint16_t *)
					(data + channels[k].location), &channels[k]);
			break;
		case 3:
			sensor_out_data->raw[k] = process_3byte_received(*(uint32_t *)
					(data + channels[k].location), &channels[k]);
			break;
		case 4:
			uint32_t val;

			if (channels[k].be)
				val = be32toh(*(uint32_t *)
						(data + channels[k].location));
			else
				val = le32toh(*(uint32_t *)
						(data + channels[k].location));
			val >>= channels[k].shift;
			val &= channels[k].mask;
			if (channels[k].is_signed) {
				sensor_out_data->raw[k] = ((float)(int32_t)val +
						channels[k].offset) * channels[k].scale;
			} else {
				sensor_out_data->raw[k] = ((float)val +
						channels[k].offset) * channels[k].scale;
			}

			break;
		case 8:
			if (channels[k].is_signed) {
				int64_t val = *(int64_t *)(data + channels[k].location);
				if ((val >> channels[k].bits_used) & 1)
					val = (val & channels[k].mask) | ~channels[k].mask;

				if ((channels[k].scale == 1.0f) && (channels[k].offset == 0.0f)) {
					sensor_out_data->timestamp = val;
				} else {
					sensor_out_data->raw[k] = (((float)val +
							channels[k].offset) * channels[k].scale);
				}
			} else {
				uint64_t val = *(uint64_t *)(data + channels[k].location);
				sensor_out_data->raw[k] = val;
			}

			break;
		default:
			return -EINVAL;
		}
	}

	return num_channels;
}

#if (CONFIG_ST_HAL_ANDROID_VERSION >= ST_HAL_MARSHMALLOW_VERSION)
static int ProcessInjectionData(float *data, struct iio_channel_info *channels, int num_channels, uint8_t *out_data, int64_t timestamp)
{
	int k;

	for (k = 0; k < num_channels; k++) {
		switch (channels[k].bytes) {
		case 1:
			*(uint8_t *)(out_data + channels[k].location) = data[k];
			break;

		case 2:
			*(uint16_t *)(out_data + channels[k].location) = (int16_t)(data[k] / channels[k].scale);
			break;

		case 8:
			*(int64_t *)(out_data + channels[k].location) = timestamp;
			break;

		default:
			return -EINVAL;
		}
	}

	return num_channels;
}
#endif /* CONFIG_ST_HAL_ANDROID_VERSION */


HWSensorBase::HWSensorBase(HWSensorBaseCommonData *data, const char *name,
		int handle, int sensor_type, unsigned int hw_fifo_len,
		float power_consumption) : SensorBase(name, handle, sensor_type)
{
	int err;
	char *buffer_path;

	memcpy(&common_data, data, sizeof(common_data));

	sensor_t_data.power = power_consumption;
	sensor_t_data.fifoMaxEventCount = hw_fifo_len;

#ifdef CONFIG_ST_HAL_FACTORY_CALIBRATION
	memset(factory_offset, 0, 3 * sizeof(float));

	factory_scale[0] = 1.0f;
	factory_scale[1] = 1.0f;
	factory_scale[2] = 1.0f;
	factory_calibration_updated = false;
#endif /* CONFIG_ST_HAL_FACTORY_CALIBRATION */

#ifdef CONFIG_ST_HAL_HAS_SELFTEST_FUNCTIONS
	selftest.available = 0;
#endif /* CONFIG_ST_HAL_HAS_SELFTEST_FUNCTIONS */

	scan_size = size_from_channelarray(common_data.channels, common_data.num_channels);

	err = asprintf(&buffer_path, "/dev/iio:device%d", data->iio_dev_num);
	if (err <= 0) {
		ALOGE("%s: Failed to allocate iio device path string.", GetName());
		goto invalid_this_class;
	}

	pollfd_iio[0].fd = open(buffer_path, O_RDONLY | O_NONBLOCK);
	if (pollfd_iio[0].fd < 0) {
		ALOGE("%s: Failed to open iio char device (%s)." , GetName(), buffer_path);
		goto free_buffer_path;
	}

	pollfd_iio[0].events = POLLIN;

	if (!ioctl(pollfd_iio[0].fd, IIO_GET_EVENT_FD_IOCTL, &pollfd_iio[1].fd)) {
		pollfd_iio[1].events = POLLIN;
		has_event_channels = true;
	} else {
		has_event_channels= false;
	}

#if (CONFIG_ST_HAL_ANDROID_VERSION >= ST_HAL_MARSHMALLOW_VERSION)
	err = iio_utils::iio_utils_support_injection_mode(common_data.iio_sysfs_path);
	switch (err) {
	case 0:
#if (CONFIG_ST_HAL_DEBUG_LEVEL >= ST_HAL_DEBUG_INFO)
		ALOGD("\"%s\": injection mode available, sensor is an injector.", GetName());
#endif /* CONFIG_ST_HAL_DEBUG_INFO */
		sensor_t_data.flags |= DATA_INJECTION_MASK;
		injection_mode = SENSOR_INJECTOR;
		break;

	case 1:
#if (CONFIG_ST_HAL_DEBUG_LEVEL >= ST_HAL_DEBUG_INFO)
		ALOGD("\"%s\": injection mode available, sensor is injected.", GetName());
#endif /* CONFIG_ST_HAL_DEBUG_INFO */
		sensor_t_data.flags |= DATA_INJECTION_MASK;
		injection_mode = SENSOR_INJECTED;
		break;

	default:
#if (CONFIG_ST_HAL_DEBUG_LEVEL >= ST_HAL_DEBUG_INFO)
		ALOGD("\"%s\": injection mode not available.", GetName());
#endif /* CONFIG_ST_HAL_DEBUG_INFO */
		sensor_t_data.flags &= ~DATA_INJECTION_MASK;
		break;
	}
#endif /* CONFIG_ST_HAL_ANDROID_VERSION */

	free(buffer_path);

	return;

free_buffer_path:
	free(buffer_path);
invalid_this_class:
	InvalidThisClass();
}

HWSensorBase::~HWSensorBase()
{
	if (!IsValidClass())
		return;

	close(pollfd_iio[0].fd);
	close(pollfd_iio[1].fd);
}

#ifdef CONFIG_ST_HAL_HAS_SELFTEST_FUNCTIONS
void HWSensorBase::GetSelfTestAvailable()
{
	int err;

	err = iio_utils::iio_utils_get_selftest_available(common_data.iio_sysfs_path, selftest.mode);
	if (err < 0)
		return;

	selftest.available = err;
}

selftest_status HWSensorBase::ExecuteSelfTest()
{
	int status;

	if (selftest.available == 0)
		return NOT_AVAILABLE;

	status = iio_utils::iio_utils_execute_selftest(common_data.iio_sysfs_path, &selftest.mode[0][0]);
	if (status < 0) {
		ALOGE("\"%s\": failed to execute selftest procedure. (errno: %d)", GetName(), status);
		return GENERIC_ERROR;
	}

	if (status == 0)
		return FAILURE;

	return PASS;
}
#endif /* CONFIG_ST_HAL_HAS_SELFTEST_FUNCTIONS */

int HWSensorBase::WriteBufferLenght(unsigned int buf_len)
{
	int err;
	unsigned int hw_buf_fifo_len;

	if (buf_len == 0)
		hw_buf_fifo_len = 1;
	else
		hw_buf_fifo_len = buf_len;

	err = iio_utils::iio_utils_set_hw_fifo_watermark(common_data.iio_sysfs_path, hw_buf_fifo_len);
	if (err < 0) {
		ALOGE("%s: Failed to write hw fifo watermark.", GetName());
		return err;
	}

	return 0;
}

int64_t elapsedRealtimeNano()
{
#ifdef PLTF_LINUX_ENABLED
    struct timespec ts;
    int err = clock_gettime(CLOCK_BOOTTIME, &ts);
    if (err) {
        ALOGE("clock_gettime(CLOCK_BOOTTIME) failed: %s", strerror(errno));
        return 0;
    }
    return (ts.tv_sec * 1000000000) + ts.tv_nsec;
#else
    return android::elapsedRealtimeNano();
#endif
}

int HWSensorBase::Enable(int handle, bool enable, bool lock_en_mutex)
{
	int err = 0;
	bool old_status, old_status_no_handle;

	if (lock_en_mutex)
		pthread_mutex_lock(&enable_mutex);

	old_status = GetStatus(false);
	old_status_no_handle = GetStatusExcludeHandle(handle);

	err = SensorBase::Enable(handle, enable, false);
	if (err < 0)
		goto unlock_mutex;

	if ((enable && !old_status) || (!enable && !old_status_no_handle)) {
		err = iio_utils::iio_utils_enable_sensor(common_data.iio_sysfs_path, GetStatus(false));
		if (err < 0) {
			ALOGE("%s: Failed to enable iio sensor device.", GetName());
			goto restore_status_enable;
		}

		if (enable)
			sensor_global_enable = elapsedRealtimeNano();
		else
			sensor_global_disable = elapsedRealtimeNano();
	}

	if (sensor_t_data.handle == handle) {
		if (enable)
			sensor_my_enable = elapsedRealtimeNano();
		 else
			sensor_my_disable = elapsedRealtimeNano();
	}

	if (lock_en_mutex)
		pthread_mutex_unlock(&enable_mutex);

	return 0;

restore_status_enable:
	SensorBase::Enable(handle, !enable, false);
unlock_mutex:
	if (lock_en_mutex)
		pthread_mutex_unlock(&enable_mutex);

	return err;
}

int HWSensorBase::SetDelay(int __attribute__((unused))handle, int64_t __attribute__((unused))period_ns,
					int64_t timeout, bool lock_en_mutex)
{
	int err;
	unsigned int buf_len;

	if (timeout < INT64_MAX) {
		if ((sensor_t_data.fifoMaxEventCount == 0) && (timeout > 0))
			return -EINVAL;
	} else
		return 0;

	if (lock_en_mutex)
		pthread_mutex_lock(&enable_mutex);

	if (sensor_t_data.fifoMaxEventCount > 0) {
		buf_len = timeout / FREQUENCY_TO_NS(1);
		if (buf_len > sensor_t_data.fifoMaxEventCount)
			buf_len = sensor_t_data.fifoMaxEventCount;

		err = WriteBufferLenght(buf_len);
		if (err < 0)
			goto mutex_unlock;
	}

#if (CONFIG_ST_HAL_DEBUG_LEVEL >= ST_HAL_DEBUG_INFO)
	ALOGD("\"%s\": changed pollrate to timeout=%" PRIu64 "ms (sensor type: %d).", sensor_t_data.name,
			(uint64_t)NS_TO_MS((uint64_t)timeout), sensor_t_data.type);
#endif /* CONFIG_ST_HAL_DEBUG_INFO */

	if (lock_en_mutex)
		pthread_mutex_unlock(&enable_mutex);

	return 0;

mutex_unlock:
	if (lock_en_mutex)
		pthread_mutex_unlock(&enable_mutex);

	return err;
}

int HWSensorBase::AddSensorDependency(SensorBase *p)
{
	int dependency_id, err;

	dependency_id = SensorBase::AddSensorDependency(p);
	if (dependency_id < 0)
		return dependency_id;

	err = AllocateBufferForDependencyData((DependencyID)dependency_id, p->GetMaxFifoLenght());
	if (err < 0)
		return err;

	return 0;
}

void HWSensorBase::RemoveSensorDependency(SensorBase *p)
{
	DeAllocateBufferForDependencyData(GetDependencyIDFromHandle(p->GetHandle()));
	SensorBase::RemoveSensorDependency(p);
}

int HWSensorBase::ApplyFactoryCalibrationData(char *filename, time_t *last_modification)
{
#ifdef CONFIG_ST_HAL_FACTORY_CALIBRATION
	int err;
	struct stat file_stat;
	FILE *calibration_file;
	double calibration_timediff;

	err = stat(filename, &file_stat);
	if (err < 0)
		return err;

	calibration_timediff = difftime(file_stat.st_mtime, *last_modification);
	if (calibration_timediff > 0) {
		factory_calibration_updated = true;
		*last_modification = file_stat.st_mtime;
	}

	calibration_file = fopen(filename, "r");
	if (!calibration_file)
		return -errno;

	err = fscanf(calibration_file, "%f,%f,%f\n", &factory_offset[0], &factory_offset[1], &factory_offset[2]);
	if (err < 0) {
		fclose(calibration_file);
		return err;
	}

	err = fscanf(calibration_file, "%f,%f,%f\n", &factory_scale[0], &factory_scale[1], &factory_scale[2]);
	if (err < 0)
		ALOGW("\"%s\": Failed to read factory scale values, it will be used default values.", GetName());

	fclose(calibration_file);

	return 0;
#else /* CONFIG_ST_HAL_FACTORY_CALIBRATION */
	(void)filename;
	(void)last_modification;

	return 0;
#endif /* CONFIG_ST_HAL_FACTORY_CALIBRATION */
}

void HWSensorBase::ProcessEvent(struct iio_event_data *event_data)
{
	uint8_t event_type, event_dir;

	event_type = IIO_EVENT_CODE_EXTRACT_TYPE(event_data->id);
	event_dir = IIO_EVENT_CODE_EXTRACT_DIR(event_data->id);

	if ((event_type == IIO_EV_TYPE_FIFO_FLUSH)  || (event_dir == IIO_EV_DIR_FIFO_DATA))
		ProcessFlushData(sensor_t_data.handle, event_data->timestamp);
}

int HWSensorBase::FlushData(int handle, bool lock_en_mutex)
{
	int err;
	unsigned int i;

	if (lock_en_mutex)
		pthread_mutex_lock(&enable_mutex);

	if (GetStatus(false)) {
		err = flush_requested.writeElement(handle);
		if (err < 0)
			goto unlock_mutex;

		if ((GetMinTimeout(false) > 0) && (GetMinTimeout(false) < INT64_MAX)) {
			for (i = 0; i < dependencies.num; i++)
				dependencies.sb[i]->FlushData(sensor_t_data.handle, true);

			err = iio_utils::iio_utils_hw_fifo_flush(common_data.iio_sysfs_path);
			if (err < 0) {
				ALOGE("%s: Failed to flush hw fifo.", GetName());
				goto unlock_mutex;
			}
		} else
			ProcessFlushData(sensor_t_data.handle, 0);
	} else
		goto unlock_mutex;

	if (lock_en_mutex)
		pthread_mutex_unlock(&enable_mutex);

	return 0;

unlock_mutex:
	if (lock_en_mutex)
		pthread_mutex_unlock(&enable_mutex);

	return -EINVAL;
}

void HWSensorBase::ProcessFlushData(int __attribute__((unused))handle, int64_t timestamp)
{
	unsigned int i;
	int err, flush_handle;

	flush_handle = flush_requested.readElement();
	if (flush_handle < 0)
		return;

	pthread_mutex_lock(&sample_in_processing_mutex);

	if (timestamp > sample_in_processing_timestamp) {
		err = flush_stack.writeElement(flush_handle, timestamp);
		if (err < 0)
			ALOGE("%s: Failed to write Flush event into stack.", GetName());
	} else {
		if (flush_handle == sensor_t_data.handle)
			WriteFlushEventToPipe();
		else {
			for (i = 0; i < push_data.num; i++)
				push_data.sb[i]->ProcessFlushData(flush_handle, timestamp);
		}
	}

	pthread_mutex_unlock(&sample_in_processing_mutex);
}

void HWSensorBase::ThreadDataTask()
{
	uint8_t *data;
	unsigned int hw_fifo_len;
	SensorBaseData sensor_data;
	int err, i, read_size, flush_handle;
	int64_t timestamp_flush, timestamp_odr_switch, new_pollrate = 0, old_pollrate = 0;

	if (sensor_t_data.fifoMaxEventCount > 0)
		hw_fifo_len = sensor_t_data.fifoMaxEventCount;
	else
		hw_fifo_len = 1;

	data = (uint8_t *)malloc(hw_fifo_len * scan_size * HW_SENSOR_BASE_DEFAULT_IIO_BUFFER_LEN * sizeof(uint8_t));
	if (!data) {
		ALOGE("%s: Failed to allocate sensor data buffer.", GetName());
		return;
	}

	while (true) {
		err = poll(&pollfd_iio[0], 1, -1);
		if (err <= 0)
			continue;

		if (pollfd_iio[0].revents & POLLIN) {
			read_size = read(pollfd_iio[0].fd, data, hw_fifo_len * scan_size * HW_SENSOR_BASE_DEFAULT_IIO_BUFFER_LEN);
			if (read_size <= 0) {
				ALOGE("%s: Failed to read data from iio char device.", GetName());
				continue;
			}

			for (i = 0; i < (read_size / scan_size); i++) {
				err = ProcessScanData(data + (i * scan_size), common_data.channels, common_data.num_channels, &sensor_data);
				if (err < 0)
					continue;

				pthread_mutex_lock(&sample_in_processing_mutex);
				sample_in_processing_timestamp = sensor_data.timestamp;
				pthread_mutex_unlock(&sample_in_processing_mutex);

				timestamp_odr_switch = odr_switch.readLastElement(&new_pollrate);
				if (sensor_data.timestamp > timestamp_odr_switch) {
					sensor_data.pollrate_ns = new_pollrate;
					old_pollrate = new_pollrate;
					odr_switch.removeLastElement();
				} else
					sensor_data.pollrate_ns = old_pollrate;

				flush_handle = flush_stack.readLastElement(&timestamp_flush);
				if ((flush_handle >= 0) && (timestamp_flush <= sensor_data.timestamp)) {
					sensor_data.flush_event_handle = flush_handle;
					flush_stack.removeLastElement();
				} else
					sensor_data.flush_event_handle = -1;

				ProcessData(&sensor_data);
			}
		}
	}
}

void HWSensorBase::ThreadEventsTask()
{
	int err, i, read_size;
	struct iio_event_data event_data[10];

	while (true) {
		err = poll(&pollfd_iio[1], 1, -1);
		if (err <= 0)
			continue;

		if (pollfd_iio[1].revents & POLLIN) {
			read_size = read(pollfd_iio[1].fd, event_data, 10 * sizeof(struct iio_event_data));
			if (read_size <= 0) {
				ALOGE("%s: Failed to read event data from iio char device.", GetName());
				continue;
			}

			for (i = 0; i < (int)(read_size / sizeof(struct iio_event_data)); i++)
				ProcessEvent(&event_data[i]);
		}
	}
}

#if (CONFIG_ST_HAL_ANDROID_VERSION >= ST_HAL_MARSHMALLOW_VERSION)
int HWSensorBase::InjectionMode(bool enable)
{
	int err;

	switch (injection_mode) {
	case SENSOR_INJECTION_NONE:
		break;

	case SENSOR_INJECTOR:
		if (enable) {
			injection_data = (uint8_t *)malloc(sizeof(uint8_t) * scan_size);
			if (!injection_data)
				return -ENOMEM;
		} else
			free(injection_data);

		err = iio_utils::iio_utils_set_injection_mode(common_data.iio_sysfs_path, enable);
		if (err < 0) {
			ALOGE("%s: Failed to switch injection mode.", GetName());
			free(injection_data);
			return err;
		}

		break;

	case SENSOR_INJECTED:
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

int HWSensorBase::InjectSensorData(const sensors_event_t *data)
{
	int err;
	enum iio_chan_type iio_sensor_type;

	err = ProcessInjectionData((float *)data->data, common_data.channels, common_data.num_channels, injection_data, data->timestamp);
	if (err < 0)
		return err;

	switch (sensor_t_data.type) {
	case SENSOR_TYPE_ACCELEROMETER:
		iio_sensor_type = IIO_ACCEL;
		break;
	case SENSOR_TYPE_GYROSCOPE:
		iio_sensor_type = IIO_ANGL_VEL;
		break;
	default:
		return -EINVAL;
	}

	return iio_utils::iio_utils_inject_data(common_data.iio_sysfs_path, injection_data, scan_size, iio_sensor_type);
}
#endif /* CONFIG_ST_HAL_ANDROID_VERSION */

HWSensorBaseWithPollrate::HWSensorBaseWithPollrate(HWSensorBaseCommonData *data, const char *name,
		struct iio_sampling_frequency_available *sfa, int handle,
		int sensor_type, unsigned int hw_fifo_len, float power_consumption) :
		HWSensorBase(data, name, handle, sensor_type, hw_fifo_len, power_consumption)
{
	unsigned int i, max_sampling_frequency = 0;
#if (CONFIG_ST_HAL_ANDROID_VERSION > ST_HAL_KITKAT_VERSION)
	unsigned int min_sampling_frequency = UINT_MAX;
#endif /* CONFIG_ST_HAL_ANDROID_VERSION */

	memcpy(&sampling_frequency_available, sfa, sizeof(sampling_frequency_available));

	for (i = 0; i < sfa->num_available; i++) {
		if ((max_sampling_frequency < sfa->hz[i]) && (sfa->hz[i] <= CONFIG_ST_HAL_MAX_SAMPLING_FREQUENCY))
			max_sampling_frequency = sfa->hz[i];

#if (CONFIG_ST_HAL_ANDROID_VERSION > ST_HAL_KITKAT_VERSION)
		if (min_sampling_frequency > sfa->hz[i])
			min_sampling_frequency = sfa->hz[i];
#endif /* CONFIG_ST_HAL_ANDROID_VERSION */
	}

	sensor_t_data.minDelay = FREQUENCY_TO_US(max_sampling_frequency);
#if (CONFIG_ST_HAL_ANDROID_VERSION > ST_HAL_KITKAT_VERSION)
	sensor_t_data.maxDelay = FREQUENCY_TO_US(min_sampling_frequency);
#endif /* CONFIG_ST_HAL_ANDROID_VERSION */
}

HWSensorBaseWithPollrate::~HWSensorBaseWithPollrate()
{

}

int HWSensorBaseWithPollrate::SetDelay(int handle, int64_t period_ns, int64_t timeout, bool lock_en_mutex)
{
	int err, i;
#if (CONFIG_ST_HAL_DEBUG_LEVEL >= ST_HAL_DEBUG_INFO)
	bool message = false;
#endif /* CONFIG_ST_HAL_DEBUG_INFO */
	unsigned int sampling_frequency, buf_len;
	int64_t min_pollrate_ns, min_timeout_ns = 0, timestamp;

	if (lock_en_mutex)
		pthread_mutex_lock(&enable_mutex);

	if ((sensors_pollrates[handle] == period_ns) && (sensors_timeout[handle] == timeout)) {
		err = 0;
		goto mutex_unlock;
	}

	if ((period_ns > 0) && (timeout < INT64_MAX)) {
#if (CONFIG_ST_HAL_ANDROID_VERSION > ST_HAL_KITKAT_VERSION)
		if (period_ns > (((int64_t)sensor_t_data.maxDelay) * 1000))
			period_ns = sensor_t_data.maxDelay * 1000;
#endif /* CONFIG_ST_HAL_ANDROID_VERSION */

		if ((period_ns < (((int64_t)sensor_t_data.minDelay) * 1000)) && (period_ns > 0))
			period_ns = sensor_t_data.minDelay * 1000;
	}

	err = SensorBase::SetDelay(handle, period_ns, timeout, false);
	if (err < 0)
		goto mutex_unlock;

	min_pollrate_ns = GetMinPeriod(false);
	if (min_pollrate_ns == 0) {
		err = 0;
		current_min_pollrate = 0;
		current_min_timeout = INT64_MAX;
		goto mutex_unlock;
	}

	sampling_frequency = NS_TO_FREQUENCY(min_pollrate_ns);
	for (i = 0; i < (int)sampling_frequency_available.num_available; i++) {
		if (sampling_frequency_available.hz[i] >= sampling_frequency)
			break;
	}
	if (i == (int)sampling_frequency_available.num_available)
		i--;

	if (current_min_pollrate != min_pollrate_ns) {
		err = iio_utils::iio_utils_set_sampling_frequency(common_data.iio_sysfs_path, sampling_frequency_available.hz[i]);
		if (err < 0) {
			ALOGE("%s: Failed to write sampling frequency to iio device.", GetName());
			goto mutex_unlock;
		}

		timestamp = elapsedRealtimeNano();

		err = odr_switch.writeElement(timestamp, FREQUENCY_TO_NS(sampling_frequency_available.hz[i]));
		if (err < 0)
			ALOGE("%s: Failed to write new odr on stack.", GetName());

		if (handle == sensor_t_data.handle)
			AddNewPollrate(timestamp, period_ns);

		current_min_pollrate = min_pollrate_ns;
#if (CONFIG_ST_HAL_DEBUG_LEVEL >= ST_HAL_DEBUG_INFO)
		message = true;
#endif /* CONFIG_ST_HAL_DEBUG_INFO */

	}

	if (sensor_t_data.fifoMaxEventCount > 0) {
		min_timeout_ns = GetMinTimeout(false);

#ifdef CONFIG_ST_HAL_COMPENSATE_DELAY
		if (min_timeout_ns < HW_SENSOR_BASE_DELAY_TRANSFER_DATA)
			min_timeout_ns = 0;
		else
			min_timeout_ns -= HW_SENSOR_BASE_DELAY_TRANSFER_DATA;
#endif /* CONFIG_ST_HAL_COMPENSATE_DELAY */
		if (current_min_timeout != min_timeout_ns) {
			buf_len = min_timeout_ns / FREQUENCY_TO_NS(sampling_frequency_available.hz[i]);

			if (buf_len > sensor_t_data.fifoMaxEventCount)
				buf_len = sensor_t_data.fifoMaxEventCount;

			err = WriteBufferLenght(buf_len);
			if (err < 0)
				goto mutex_unlock;

			current_min_timeout = min_timeout_ns;
#if (CONFIG_ST_HAL_DEBUG_LEVEL >= ST_HAL_DEBUG_INFO)
			message = true;
#endif /* CONFIG_ST_HAL_DEBUG_INFO */
		}
	}

#if (CONFIG_ST_HAL_DEBUG_LEVEL >= ST_HAL_DEBUG_INFO)
	if (message)
		ALOGD("\"%s\": changed pollrate to %.2fHz, timeout=%" PRIu64 "ms (sensor type: %d).",
			sensor_t_data.name, NS_TO_FREQUENCY((float)(uint64_t)min_pollrate_ns),
			(uint64_t)NS_TO_MS((uint64_t)min_timeout_ns), sensor_t_data.type);
#endif /* CONFIG_ST_HAL_DEBUG_INFO */

	if (lock_en_mutex)
		pthread_mutex_unlock(&enable_mutex);

	return 0;

mutex_unlock:
	if (lock_en_mutex)
		pthread_mutex_unlock(&enable_mutex);

	return err;
}

int HWSensorBaseWithPollrate::FlushData(int handle, bool lock_en_mutex)
{
	int err;
	unsigned int i;

	if (lock_en_mutex)
		pthread_mutex_lock(&enable_mutex);

	if (GetStatus(false)) {
		err = flush_requested.writeElement(handle);
		if (err < 0)
			goto unlock_mutex;

		if ((GetMinTimeout(false) > 0) && (GetMinTimeout(false) < INT64_MAX)) {
			for (i = 0; i < dependencies.num; i++)
				dependencies.sb[i]->FlushData(sensor_t_data.handle, true);

			err = iio_utils::iio_utils_hw_fifo_flush(common_data.iio_sysfs_path);
			if (err < 0) {
				ALOGE("%s: Failed to flush hw fifo.", GetName());
				goto unlock_mutex;
			}
		} else
			ProcessFlushData(sensor_t_data.handle, elapsedRealtimeNano());
	} else
		goto unlock_mutex;

	if (lock_en_mutex)
		pthread_mutex_unlock(&enable_mutex);

	return 0;

unlock_mutex:
	if (lock_en_mutex)
		pthread_mutex_unlock(&enable_mutex);

	return -EINVAL;
}

void HWSensorBaseWithPollrate::WriteDataToPipe(int64_t hw_pollrate)
{
	int err;
	float temp;
	bool odr_changed = false;
	int64_t timestamp_change = 0, new_pollrate = 0;

	err = CheckLatestNewPollrate(&timestamp_change, &new_pollrate);
	if ((err >= 0) && (sensor_event.timestamp > timestamp_change)) {
		current_real_pollrate = new_pollrate;
		DeleteLatestNewPollrate();
		odr_changed = true;
	}

	if (ValidDataToPush(sensor_event.timestamp)) {
		temp = (float)current_real_pollrate / hw_pollrate;
		decimator = (int)(temp + (temp / 20));
		samples_counter++;

		if (decimator == 0)
			decimator = 1;

		if (((samples_counter % decimator) == 0) || odr_changed) {
			err = write(write_pipe_fd, &sensor_event, sizeof(sensors_event_t));
			if (err <= 0) {
				ALOGE("%s: Failed to write sensor data to pipe. (errno: %d)", android_name, -errno);
				samples_counter--;
				return;
			}

			samples_counter = 0;
			last_data_timestamp = sensor_event.timestamp;

#if (CONFIG_ST_HAL_DEBUG_LEVEL >= ST_HAL_DEBUG_EXTRA_VERBOSE)
			ALOGD("\"%s\": pushed data to android: timestamp=%" PRIu64 "ns real_pollrate=%" PRIu64 " (sensor type: %d).", sensor_t_data.name, sensor_event.timestamp, current_real_pollrate, sensor_t_data.type);
#endif /* CONFIG_ST_HAL_DEBUG_LEVEL */
		}
	}
}
