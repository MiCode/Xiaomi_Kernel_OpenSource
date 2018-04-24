/*
 * STMicroelectronics SensorHAL simple test
 *
 * Copyright 2017 STMicroelectronics Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 */

#undef ANDROID_LOG
#undef LOG_FILE

#include <asm/types.h>
#include <stdio.h>
#include <linux/limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <dlfcn.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <hardware/sensors.h>

#ifndef LOG_TAG
#define LOG_TAG "test_linux"
#endif

#ifdef ANDROID_LOG

#include <utils/Log.h>
#include <android/log.h>
#define tl_log(...) \
	__android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

#else /* ANDROID_LOG */

#ifdef LOG_FILE

#define LOG_FILENAME "/storage/emulated/0/test_linux.log"
#define tl_log(fmt, ...) do { \
	fprintf(logfd, "%s:%s:%s:%d > " fmt "\n", \
		    LOG_TAG, __FUNCTION__, __FILE__, __LINE__, ##__VA_ARGS__); \
	} while(0);

#else /* LOG_FILE */

#define tl_log(fmt, ...) printf("%s:%s:%s:%d > " fmt "\n", \
		    LOG_TAG, __FUNCTION__, __FILE__, __LINE__, ##__VA_ARGS__)

#endif /* LOG_FILE */

#endif /* ANDROID_LOG */

/* Max event buffer for poll sensor */
#define BUFFER_EVENT	256

/* Defines to enable/disable sensors */
#define SENSOR_ENABLE	1
#define SENSOR_DISABLE	0

/* Translate sansor index from listo to handle */
#define HANDLE_FROM_INDEX(_i) (i + 1)
#define INVALID_HANDLE	-1

static struct sensors_module_t *hmi;
static struct hw_device_t *dev;
struct sensors_poll_device_t *poll_dev;
static struct sensor_t const* list;
static int sensor_num;

/* Path to SensorHAL in target filesystem */
static const char *hal_paths[] = {
	"/tmp/mnt/SensorHAL.so",
	"/system/vendor/lib64/hw/SensorHAL.so",
	"/system/vendor/lib/hw/SensorHAL.so",
};

static const char* types[] = {
	"Meta",
	"Acc",
	"Mag",
	"Orientation",
	"Gyro",
	"Light",
	"Press",
	"Temp",
	"Proximity",
	"Gravity",
	"Lin Acc",
	"Rot Vect",
};

/* Report sensor type in string format */
static const char *type_str(int type)
{
	int type_count = sizeof(types)/sizeof(char *);

	if (type < 0 || type >= type_count)
		return "unknown";

	return types[type];
}

static void dump_event(struct sensors_event_t *e)
{
	switch (e->type) {
	case SENSOR_TYPE_ACCELEROMETER:
		tl_log("event: x=%10.2f y=%10.2f z=%10.2f timestamp=%ld",
			e->acceleration.x, e->acceleration.y, e->acceleration.z,
			e->timestamp);
		break;
	case SENSOR_TYPE_MAGNETIC_FIELD:
		tl_log("event: x=%10.2f y=%10.2f z=%10.2f timestamp=%ld",
			e->magnetic.x, e->magnetic.y, e->magnetic.z,
			e->timestamp);
		break;
	case SENSOR_TYPE_GYROSCOPE:
		tl_log("event: x=%10.2f y=%10.2f z=%10.2f timestamp=%ld",
			e->gyro.x, e->gyro.y, e->gyro.z, e->timestamp);
		break;
	default:
		tl_log("Unhandled events %d", e->type);
		break;
	}
}

static void dump_sensor(const struct sensor_t *s)
{
	int i;

	tl_log("\nSensor List");
	for(i = 0; i < sensor_num; i++) {
		tl_log("\n\tName %s", s[i].name);
		tl_log("\tVendor %s", s[i].vendor);
		tl_log("\tHandle %d", s[i].handle);
		tl_log("\t\tType %s (%d)", type_str(s[i].type), s[i].type);
		tl_log("\t\tVersion %d", s[i].version);
		tl_log("\t\tMax Range %d", s[i].maxRange);
		tl_log("\t\tResolution %d", s[i].resolution);
		tl_log("\t\tPower %d", s[i].power);
		tl_log("\t\tMin Delay %d", s[i].minDelay);
		tl_log("\t\tFIFO Reserved Event %d", s[i].fifoReservedEventCount);
		tl_log("\t\tFIFO Max Event %d", s[i].fifoMaxEventCount);
	}
}

static int get_sensor(const struct sensor_t *s, int type,
		      struct sensor_t **sens)
{
	int i;

	for(i = 0; i < sensor_num; i++) {
		if (type == s[i].type) {
			*sens = (struct sensor_t *)&s[i];

			return HANDLE_FROM_INDEX(i);
		}
	}

	return INVALID_HANDLE;
}

static void run_sensors_poll_v0(int maxcount)
{
	int tot = 0;

	poll_dev = (struct sensors_poll_device_t *)dev;

	while (1) {
		sensors_event_t events[BUFFER_EVENT];
		int i, count;

		count = poll_dev->poll(poll_dev,
				events,
				sizeof(events)/sizeof(sensors_event_t));

		for(i = 0; i < count; i++)
			dump_event(&events[i]);

		tot += count;
		if (tot >= maxcount && maxcount != 0)
			break;
	}
}

static int sensor_set_delay(int handle, int64_t delay)
{
	return poll_dev->setDelay(poll_dev, handle, delay);
}

static int sensor_activate(int handle, int enable)
{
	return poll_dev->activate(poll_dev, handle, enable);
}

static int open_hal(char *lcl_path)
{
	void *hal;
	int err;
	const char *lh_path = NULL;

	if (!lcl_path) {
		unsigned i;

		for(i = 0; i < sizeof(hal_paths)/sizeof(const char*); i++) {
			if (!access(hal_paths[i], R_OK)) {
				lh_path = hal_paths[i];
				break;
			}
		}

		if (!lh_path) {
			fprintf(stderr, "ERROR: unable to find HAL\n");
			exit(1);
		}
	} else
		lh_path = lcl_path;

	hal = dlopen(lh_path, RTLD_NOW);
	if (!hal) {
		fprintf(stderr, "ERROR: unable to load HAL %s: %s\n", lh_path,
			dlerror());
		return -1;
	}

	hmi = dlsym(hal, HAL_MODULE_INFO_SYM_AS_STR);
	if (!hmi) {
		fprintf(stderr, "ERROR: unable to find %s entry point in HAL\n",
			HAL_MODULE_INFO_SYM_AS_STR);
		return -1;
	}

	tl_log("HAL loaded: name %s vendor %s version %d.%d id %s",
	       hmi->common.name, hmi->common.author,
	       hmi->common.version_major, hmi->common.version_minor,
	       hmi->common.id);

	err = hmi->common.methods->open((struct hw_module_t *)hmi,
					SENSORS_HARDWARE_POLL, &dev);
	if (err) {
		fprintf(stderr, "ERROR: failed to initialize HAL: %d\n", err);
		exit(1);
	}

	poll_dev = (struct sensors_poll_device_t *)dev;

	return 0;
}

int main(int argc, char **argv)
{
	int sindex = 0;
	int num_sample = 50;
	int ret;
	int handle;
	struct sensor_t *sensor = NULL;
	int test_sensor_type[] = {
		SENSOR_TYPE_ACCELEROMETER,
		SENSOR_TYPE_GYROSCOPE,
		-1,
	};

	ret = open_hal(NULL);
	if (ret) {
		fprintf(stderr, "ERROR: unable to open SensorHAL\n");
		exit(1);
	}

	sensor_num = hmi->get_sensors_list(hmi, &list);
	if (!sensor_num) {
		fprintf(stderr, "ERROR: no sensors available\n");
		exit(1);
	}

	/* Dump sensor list */
	dump_sensor(list);

	while(test_sensor_type[sindex] != -1) {
		/* Activate sensor */
		handle = get_sensor(list, test_sensor_type[sindex], &sensor);
		if (handle != INVALID_HANDLE && sensor) {
			tl_log("Activating %s sensor (handle %d)", sensor->name, handle);
			sensor_activate(handle, SENSOR_ENABLE);

			/* Start polling data */
			tl_log("Polling %s sensor (handle %d)", sensor->name, handle);
			run_sensors_poll_v0(num_sample);

			/* Deactivate sensor */
			tl_log("Deactivating %s sensor (handle %d)", sensor->name, handle);
			sensor_activate(handle, SENSOR_DISABLE);
		}

	sindex++;
	}

	return 0;
}
