/*
 * STMicroelectronics SensorBase Class
 *
 * Copyright 2015-2016 STMicroelectronics Inc.
 * Author: Denis Ciocca - <denis.ciocca@st.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 */

#define __STDC_LIMIT_MACROS
#define __STDINT_LIMITS

#include <stdint.h>
#include <fcntl.h>
#include <assert.h>
#include <string.h>
#include <signal.h>

#include "SensorBase.h"

#if (CONFIG_ST_HAL_ANDROID_VERSION == ST_HAL_KITKAT_VERSION)
void atomic_init(atomic_short *atom, int num)
{
	pthread_mutex_lock(&atom->atomic_mutex);
	atom->counter = num;
	pthread_mutex_unlock(&atom->atomic_mutex);
}

void atomic_fetch_add(atomic_short *atom, int num)
{
	pthread_mutex_lock(&atom->atomic_mutex);
	atom->counter += num;
	pthread_mutex_unlock(&atom->atomic_mutex);
}

void atomic_fetch_sub(atomic_short *atom, int num)
{
	pthread_mutex_lock(&atom->atomic_mutex);

	if (atom->counter >= num)
		atom->counter -= num;
	else
		atom->counter = 0;

	pthread_mutex_unlock(&atom->atomic_mutex);
}

int atomic_load(atomic_short *atom)
{
	short ret;

	pthread_mutex_lock(&atom->atomic_mutex);
	ret = atom->counter;
	pthread_mutex_unlock(&atom->atomic_mutex);

	return ret;
}
#endif /* CONFIG_ST_HAL_ANDROID_VERSION */

SensorBase::SensorBase(const char *name, int handle, int type)
{
	int i, err, pipe_fd[2];

	if (strlen(name) + 1 > SENSOR_BASE_ANDROID_NAME_MAX) {
		memcpy(android_name, name, SENSOR_BASE_ANDROID_NAME_MAX - 1);
		android_name[SENSOR_BASE_ANDROID_NAME_MAX - 1] = '\0';
	} else
		memcpy(android_name, name, strlen(name) + 1);

	valid_class = true;
	memset(dependencies_type_list, 0, SENSOR_DEPENDENCY_ID_MAX * sizeof(int));
	memset(&push_data, 0, sizeof(dependencies_t));
	memset(&dependencies, 0, sizeof(push_data_t));
	memset(&sensor_t_data, 0, sizeof(struct sensor_t));
	memset(&sensor_event, 0, sizeof(sensors_event_t));
	memset(sensors_pollrates, 0, ST_HAL_IIO_MAX_DEVICES * sizeof(int64_t));

	for (i = 0; i < ST_HAL_IIO_MAX_DEVICES; i++)
		sensors_timeout[i] = INT64_MAX;

	sensor_event.version = sizeof(sensors_event_t);
	sensor_event.sensor = handle;
	sensor_event.type = type;

	sensor_t_data.name = android_name;
	sensor_t_data.handle = handle;
	sensor_t_data.type = type;
	sensor_t_data.vendor = "STMicroelectronics";
	sensor_t_data.version = 1;

	last_data_timestamp = 0;
	enabled_sensors_mask = 0;
	current_real_pollrate = 0;
	sample_in_processing_timestamp = 0;
	current_min_pollrate = 0;
	current_min_timeout = INT64_MAX;
	sensor_global_enable = 0;
	sensor_global_disable = 1;
	sensor_my_enable = 0;
	sensor_my_disable = 1;
	decimator = 1;
	samples_counter = 0;

#if (CONFIG_ST_HAL_ANDROID_VERSION >= ST_HAL_MARSHMALLOW_VERSION)
	injection_mode = SENSOR_INJECTION_NONE;
#endif /* CONFIG_ST_HAL_ANDROID_VERSION */

	write_pipe_fd = -EINVAL;
	read_pipe_fd = -EINVAL;

	pthread_mutex_init(&enable_mutex, NULL);
	pthread_mutex_init(&sample_in_processing_mutex, NULL);

	err = pipe(pipe_fd);
	if (err < 0) {
		ALOGE("%s: Failed to create pipe file.", GetName());
		goto invalid_the_class;
	}

	fcntl(pipe_fd[0], F_SETFL, O_NONBLOCK);

	write_pipe_fd = pipe_fd[1];
	read_pipe_fd = pipe_fd[0];

	return;

invalid_the_class:
	InvalidThisClass();
}

SensorBase::~SensorBase()
{
	close(write_pipe_fd);
	close(read_pipe_fd);
}

DependencyID SensorBase::GetDependencyIDFromHandle(int handle)
{
	return handle_remapping_ID[handle];
}

void SensorBase::SetDependencyIDOfHandle(int handle, DependencyID id)
{
	handle_remapping_ID[handle] = id;
}

void SensorBase::InvalidThisClass()
{
	valid_class = false;
}

bool SensorBase::IsValidClass()
{
	return valid_class;
}

int SensorBase::CustomInit()
{
	return 0;
}

int SensorBase::GetHandle()
{
	return sensor_t_data.handle;
}

int SensorBase::GetType()
{
	return sensor_t_data.type;
}

int SensorBase::GetMaxFifoLenght()
{
	return sensor_t_data.fifoMaxEventCount;
}

int SensorBase::GetFdPipeToRead()
{
	return read_pipe_fd;
}

void SensorBase::SetBitEnableMask(int handle)
{
	enabled_sensors_mask |= (1ULL << handle);
}

void SensorBase::ResetBitEnableMask(int handle)
{
	enabled_sensors_mask &= ~(1ULL << handle);
}

int SensorBase::AddNewPollrate(int64_t timestamp, int64_t pollrate)
{
	return odr_stack.writeElement(timestamp, pollrate);
}

int SensorBase::CheckLatestNewPollrate(int64_t *timestamp, int64_t *pollrate)
{
	*timestamp = odr_stack.readLastElement(pollrate);
	if (*timestamp < 0)
		return -EINVAL;

	return 0;
}

void SensorBase::DeleteLatestNewPollrate()
{
	odr_stack.removeLastElement();
}

bool SensorBase::ValidDataToPush(int64_t timestamp)
{
	if (sensor_my_enable > sensor_my_disable) {
		if (timestamp > sensor_my_enable)
			return true;
	} else {
		if ((timestamp > sensor_my_enable) && (timestamp < sensor_my_disable))
			return true;
	}

	return false;
}

bool SensorBase::GetDependencyMaxRange(int type, float *maxRange)
{
	bool found;
	unsigned int i;
	float maxRange_priv = 0;

	if (sensor_t_data.type == type) {
		*maxRange = sensor_t_data.maxRange;
		return true;
	}

	for (i = 0; i < dependencies.num; i++) {
		found = dependencies.sb[i]->GetDependencyMaxRange(type, &maxRange_priv);
		if (found) {
			*maxRange = maxRange_priv;
			return true;
		}
	}

	return false;
}

char* SensorBase::GetName()
{
	return (char *)sensor_t_data.name;
}

#ifdef CONFIG_ST_HAL_HAS_SELFTEST_FUNCTIONS
selftest_status SensorBase::ExecuteSelfTest()
{
	return NOT_AVAILABLE;
}
#endif /* CONFIG_ST_HAL_HAS_SELFTEST_FUNCTIONS */

int SensorBase::Enable(int handle, bool enable, bool lock_en_mutex)
{
	int err = 0;
	unsigned int i = 0;

	if (lock_en_mutex)
		pthread_mutex_lock(&enable_mutex);

	if ((handle == sensor_t_data.handle) && (enable == GetStatusOfHandle(handle)))
		goto enable_unlock_mutex;

	if ((enable && !GetStatus(false)) || (!enable && !GetStatusExcludeHandle(handle))) {
		if (enable) {
			SetBitEnableMask(handle);

			flush_stack.resetBuffer();
		} else {
			err = SetDelay(handle, 0, INT64_MAX, false);
			if (err < 0)
				goto enable_unlock_mutex;

			ResetBitEnableMask(handle);
		}

		for (i = 0; i < dependencies.num; i++) {
			err = dependencies.sb[i]->Enable(sensor_t_data.handle, enable, true);
			if (err < 0)
				goto restore_enable_dependencies;
		}

#if (CONFIG_ST_HAL_DEBUG_LEVEL >= ST_HAL_DEBUG_INFO)
		if (enable)
			ALOGD("\"%s\": power-on (sensor type: %d).", sensor_t_data.name, sensor_t_data.type);
		else
			ALOGD("\"%s\": power-off (sensor type: %d).", sensor_t_data.name, sensor_t_data.type);
#endif /* CONFIG_ST_HAL_DEBUG_LEVEL */
	} else {
		if (enable)
			SetBitEnableMask(handle);
		else {
			err = SetDelay(handle, 0, INT64_MAX, false);
			if (err < 0)
				goto enable_unlock_mutex;

			ResetBitEnableMask(handle);
		}
	}

	if (lock_en_mutex)
		pthread_mutex_unlock(&enable_mutex);

	return 0;

restore_enable_dependencies:
	while (i > 0) {
		i--;
		dependencies.sb[i]->Enable(sensor_t_data.handle, !enable, true);
	}

	if (enable)
		ResetBitEnableMask(handle);
	else
		SetBitEnableMask(handle);
enable_unlock_mutex:
	if (lock_en_mutex)
		pthread_mutex_unlock(&enable_mutex);

	return err;
}

bool SensorBase::GetStatusExcludeHandle(int handle)
{
	return (enabled_sensors_mask & ~(1ULL << handle)) > 0 ? true : false;
}

bool SensorBase::GetStatusOfHandle(int handle)
{
	return (enabled_sensors_mask & (1ULL << handle)) > 0 ? true : false;
}

bool SensorBase::GetStatusOfHandle(int handle, bool lock_en_mutex)
{
	bool status;

	if (lock_en_mutex)
		pthread_mutex_lock(&enable_mutex);

	status = (enabled_sensors_mask & (1ULL << handle)) > 0 ? true : false;

	if (lock_en_mutex)
		pthread_mutex_unlock(&enable_mutex);

	return status;
}

bool SensorBase::GetStatus(bool lock_en_mutex)
{
	bool status;

	if (lock_en_mutex)
		pthread_mutex_lock(&enable_mutex);

	status = enabled_sensors_mask > 0 ? true : false;

	if (lock_en_mutex)
		pthread_mutex_unlock(&enable_mutex);

	return status;
}

int SensorBase::SetDelay(int handle, int64_t period_ns, int64_t timeout, bool lock_en_mutex)
{
	int err, i;
	int64_t restore_min_timeout, restore_min_period_ms;

	if ((timeout > 0) && (timeout < INT64_MAX) && (sensor_t_data.fifoMaxEventCount == 0))
		return -EINVAL;

	if (lock_en_mutex)
		pthread_mutex_lock(&enable_mutex);

	restore_min_timeout = sensors_timeout[handle];
	restore_min_period_ms = sensors_pollrates[handle];

	sensors_pollrates[handle] = period_ns;
	sensors_timeout[handle] = timeout;

	for (i = 0; i < (int)dependencies.num; i++) {
		err = dependencies.sb[i]->SetDelay(sensor_t_data.handle, GetMinPeriod(false), GetMinTimeout(false), true);
		if (err < 0)
			goto restore_delay_dependencies;
	}

	if (lock_en_mutex)
		pthread_mutex_unlock(&enable_mutex);

	return 0;

restore_delay_dependencies:
	sensors_pollrates[handle] = restore_min_period_ms;
	sensors_timeout[handle] = restore_min_timeout;

	for (i--; i >= 0; i--)
		dependencies.sb[i]->SetDelay(sensor_t_data.handle, GetMinPeriod(false), GetMinTimeout(false), true);

	if (lock_en_mutex)
		pthread_mutex_unlock(&enable_mutex);

	return err;
}

void SensorBase::GetDepenciesTypeList(int type[SENSOR_DEPENDENCY_ID_MAX])
{
	memcpy(type, dependencies_type_list, SENSOR_DEPENDENCY_ID_MAX * sizeof(int));
}

int SensorBase::AllocateBufferForDependencyData(DependencyID id, unsigned int max_fifo_len)
{
	circular_buffer_data[id] = new CircularBuffer(max_fifo_len < 2 ? 10 : 10 * max_fifo_len);
	if (!circular_buffer_data[id]) {
		ALOGE("%s: Failed to allocate circular buffer data.", GetName());
		return -ENOMEM;
	}

	return 0;
}

void SensorBase::DeAllocateBufferForDependencyData(DependencyID id)
{
	delete circular_buffer_data[id];
}

int SensorBase::AddSensorToDataPush(SensorBase *t)
{
	if (push_data.num >= SENSOR_DEPENDENCY_ID_MAX) {
		ALOGE("%s: Failed to add dependency data, too many sensors to push data.", android_name);
		return -ENOMEM;
	}

	push_data.sb[push_data.num] = t;
	push_data.num++;

	return 0;
}

void SensorBase::RemoveSensorToDataPush(SensorBase *t)
{
	unsigned int i;

	for (i = 0; i < push_data.num; i++) {
		if (t == push_data.sb[i])
			break;
	}
	if (i == push_data.num)
		return;

	for (; i < push_data.num - 1; i++)
		push_data.sb[i] = push_data.sb[i + 1];

	push_data.num--;
}

int SensorBase::AddSensorDependency(SensorBase *p)
{
	int err;
	unsigned int dependency_id;
	struct sensor_t dependecy_data;
#if (CONFIG_ST_HAL_ANDROID_VERSION > ST_HAL_KITKAT_VERSION)
	uint32_t sensor_dependency_wake_flag;
#endif /* CONFIG_ST_HAL_ANDROID_VERSION */

	if (dependencies.num >= SENSOR_DEPENDENCY_ID_MAX) {
		ALOGE("%s: Failed to add dependency, too many dependencies.", android_name);
		return -ENOMEM;
	}

	dependency_id = dependencies.num;
	SetDependencyIDOfHandle(p->GetHandle(), (DependencyID)dependency_id);

	err = p->AddSensorToDataPush(this);
	if (err < 0)
		return err;

	p->GetSensor_tData(&dependecy_data);
	sensor_t_data.power += dependecy_data.power;

#if (CONFIG_ST_HAL_ANDROID_VERSION > ST_HAL_KITKAT_VERSION)
	sensor_dependency_wake_flag = (dependecy_data.flags & SENSOR_FLAG_WAKE_UP);
	if (!sensor_dependency_wake_flag)
		sensor_t_data.flags &= ~sensor_dependency_wake_flag;
#endif /* CONFIG_ST_HAL_ANDROID_VERSION */

	dependencies.sb[dependency_id] = p;
	dependencies.num++;

	return dependency_id;
}

void SensorBase::RemoveSensorDependency(SensorBase *p)
{
	unsigned int i;

	for (i = 0; i < dependencies.num; i++) {
		if (p == dependencies.sb[i])
			break;
	}
	if (i == dependencies.num)
		return;

	p->RemoveSensorToDataPush(this);

	for (; i < dependencies.num - 1; i++)
		dependencies.sb[i] = dependencies.sb[i + 1];

	dependencies.num--;
}

bool SensorBase::GetSensor_tData(struct sensor_t *data)
{
	memcpy(data, &sensor_t_data, sizeof(struct sensor_t));

	if (sensor_t_data.type >= SENSOR_TYPE_ST_CUSTOM_NO_SENSOR)
		return false;

	return true;
}

int SensorBase::FlushData(int __attribute__((unused))handle,
					bool __attribute__((unused))lock_en_mute)
{
	return 0;
}

void SensorBase::ProcessFlushData(int __attribute__((unused))handle,
					int64_t __attribute__((unused))timestamp)
{
	return;
}

void SensorBase::WriteFlushEventToPipe()
{
	int err;
	sensors_event_t flush_event_data;

	memset(&flush_event_data, 0, sizeof(sensors_event_t));

	flush_event_data.sensor = 0;
	flush_event_data.timestamp = 0;
	flush_event_data.meta_data.sensor = sensor_t_data.handle;
	flush_event_data.meta_data.what = META_DATA_FLUSH_COMPLETE;
	flush_event_data.type = SENSOR_TYPE_META_DATA;
	flush_event_data.version = META_DATA_VERSION;

#if (CONFIG_ST_HAL_DEBUG_LEVEL >= ST_HAL_DEBUG_VERBOSE)
	ALOGD("\"%s\": write flush event to pipe (sensor type: %d).", GetName(), GetType());
#endif /* CONFIG_ST_HAL_DEBUG_LEVEL */

	err = write(write_pipe_fd, &flush_event_data, sizeof(sensors_event_t));
	if (err <= 0)
		ALOGE("%s: Failed to write flush event data to pipe.", android_name);
}

void SensorBase::WriteDataToPipe(int64_t __attribute__((unused))hw_pollrate)
{
	int err;

	if (ValidDataToPush(sensor_event.timestamp)) {
		if (sensor_event.timestamp > last_data_timestamp) {
			err = write(write_pipe_fd, &sensor_event, sizeof(sensors_event_t));
			if (err <= 0) {
				ALOGE("%s: Failed to write sensor data to pipe. (errno: %d)", android_name, -errno);
				return;
			}

			last_data_timestamp = sensor_event.timestamp;

#if (CONFIG_ST_HAL_DEBUG_LEVEL >= ST_HAL_DEBUG_EXTRA_VERBOSE)
			ALOGD("\"%s\": pushed data to android: timestamp=%" PRIu64 "ns (sensor type: %d).", sensor_t_data.name, sensor_event.timestamp, sensor_t_data.type);
#endif /* CONFIG_ST_HAL_DEBUG_LEVEL */
		}
	}
}

void SensorBase::ProcessData(SensorBaseData *data)
{
	unsigned int i;

	if (data->flush_event_handle == sensor_t_data.handle)
		WriteFlushEventToPipe();

	for (i = 0; i < push_data.num; i++)
		push_data.sb[i]->ReceiveDataFromDependency(sensor_t_data.handle, data);
}

void SensorBase::ReceiveDataFromDependency(int handle, SensorBaseData *data)
{
#if (CONFIG_ST_HAL_DEBUG_LEVEL >= ST_HAL_DEBUG_EXTRA_VERBOSE)
	int err;
#endif /* CONFIG_ST_HAL_DEBUG_LEVEL */
	bool fill_buffer = false;

	if (sensor_global_enable > sensor_global_disable) {
		if (data->timestamp > sensor_global_enable)
			fill_buffer = true;
	} else {
		if ((data->timestamp > sensor_global_enable) && (data->timestamp < sensor_global_disable))
			fill_buffer = true;
	}

	if (fill_buffer) {
#if (CONFIG_ST_HAL_DEBUG_LEVEL >= ST_HAL_DEBUG_EXTRA_VERBOSE)
		err = circular_buffer_data[GetDependencyIDFromHandle(handle)]->writeElement(data);
		if (err < 0)
			ALOGE("%s: Circular Buffer override, increase CircularBuffer size. Receiving data from dependency %d.", GetName(), GetDependencyIDFromHandle(handle));
#else /* CONFIG_ST_HAL_DEBUG_LEVEL */
		circular_buffer_data[GetDependencyIDFromHandle(handle)]->writeElement(data);
#endif /* CONFIG_ST_HAL_DEBUG_LEVEL */
	}

	return;
}

int SensorBase::GetLatestValidDataFromDependency(int dependency_id, SensorBaseData *data, int64_t timesync)
{
	return circular_buffer_data[dependency_id]->readSyncElement(data, timesync);
}

int64_t SensorBase::GetMinTimeout(bool lock_en_mutex)
{
	int i;
	int64_t min = INT64_MAX;

	if (lock_en_mutex)
		pthread_mutex_lock(&enable_mutex);

	for (i = 0; i < ST_HAL_IIO_MAX_DEVICES; i++) {
		if ((sensors_timeout[i] < min) && (sensors_timeout[i] < INT64_MAX))
			min = sensors_timeout[i];
	}

	if (lock_en_mutex)
		pthread_mutex_unlock(&enable_mutex);

	return min;
}

int64_t SensorBase::GetMinPeriod(bool lock_en_mutex)
{
	int i;
	int64_t min = INT64_MAX;

	if (lock_en_mutex)
		pthread_mutex_lock(&enable_mutex);

	for (i = 0; i < ST_HAL_IIO_MAX_DEVICES; i++) {
		if ((sensors_pollrates[i] < min) && (sensors_pollrates[i] > 0))
			min = sensors_pollrates[i];
	}

	if (lock_en_mutex)
		pthread_mutex_unlock(&enable_mutex);

	return min == INT64_MAX ? 0 : min;
}

void *SensorBase::ThreadDataWork(void *context)
{
	SensorBase *mypointer = (SensorBase *)context;

	mypointer->ThreadDataTask();

	return mypointer;
}

void SensorBase::ThreadDataTask()
{
	pthread_exit(NULL);
}

void *SensorBase::ThreadEventsWork(void *context)
{
	SensorBase *mypointer = (SensorBase *)context;

	mypointer->ThreadEventsTask();

	return mypointer;
}

void SensorBase::ThreadEventsTask()
{
	pthread_exit(NULL);
}

#if (CONFIG_ST_HAL_ANDROID_VERSION >= ST_HAL_MARSHMALLOW_VERSION)
int SensorBase::InjectionMode(bool __attribute__((unused))enable)
{
	return 0;
}

int SensorBase::InjectSensorData(const sensors_event_t __attribute__((unused))*data)
{
	return -EINVAL;
}
#endif /* CONFIG_ST_HAL_ANDROID_VERSION */

bool SensorBase::hasEventChannels()
{
	return false;
}

bool SensorBase::hasDataChannels()
{
	return false;
}
