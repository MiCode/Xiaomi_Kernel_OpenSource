/*
 * Copyright (C) 2008 The Android Open Source Project
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

#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <android/sensor.h>
#include <cutils/log.h>

#include <gui/SensorManager.h>
#include <gui/Sensor.h>

#include "../src/common_data.h"
#include "../src/SelfTest.h"


#define ST_SELFTEST_TOOL_TIMEOUT_ANSWER_MS		(15000)
#define ST_SELFTEST_COMMAND_LINE_LENGTH			(20)

using namespace android;
using android::String16;


static struct handler_list {
	int handle[ST_SELFTEST_COMMAND_LINE_LENGTH];
	int num;
} handler_list;


int main(int argc, char **argv)
{
	struct pollfd fds;
	Sensor const* const* list;
	SensorManager *sensor_manager;
	struct selftest_cmd_t cmd_data;
	const char *result_string = NULL;
	struct selftest_results_t results_data;
	int err, i, n, c, g, fd_cmd, fd_results, read_size, sensors_available;

	handler_list.num = 0;

	if (argc == 1)
		ALOGI("no arguments, all available selftest will be executed.");
	else {
		ALOGI("parsing command line arguments...");

		while ((c = getopt(argc, argv, "h:")) != -1) {
			switch (c) {
			case 'h':
				handler_list.handle[handler_list.num] = atoi(optarg);
				handler_list.num++;

				if (handler_list.num >= ST_SELFTEST_COMMAND_LINE_LENGTH) {
					ALOGE("too many arguments...Abort!");
					return -1;
				}

				break;

			case '?':
				ALOGI("invalid option. Abort!");
				return -1;
			}
		}

		if (handler_list.num == 0)
			ALOGE("no valid arguments. All available selftest will be executed.");
	}

#if (CONFIG_ST_HAL_ANDROID_VERSION >= ST_HAL_MARSHMALLOW_VERSION)
	sensor_manager = &SensorManager::getInstanceForPackage(String16("com.st.selftest"));
#else /* CONFIG_ST_HAL_ANDROID_VERSION */
	sensor_manager = new SensorManager();
#endif /* CONFIG_ST_HAL_ANDROID_VERSION */

	sensors_available = sensor_manager->getSensorList(&list);
	if (sensors_available <= 0) {
		ALOGE("no sensors available");
		return -ENODEV;
	}

	fd_cmd = open(ST_HAL_SELFTEST_CMD_DATA_PATH, O_WRONLY | O_NONBLOCK);
	if (fd_cmd < 0)
		return -errno;

	fd_results = open(ST_HAL_SELFTEST_RESULTS_DATA_PATH, O_RDWR | O_NONBLOCK);
	if (fd_results < 0) {
		close(fd_cmd);
		return -errno;
	}

	do {
		read_size = read(fd_results, &cmd_data, sizeof(struct selftest_cmd_t));
	} while (read_size > 0);

	fds.fd = fd_results;
	fds.events = POLLIN;

	if (handler_list.num > 0) {
		for (n = 0; n < handler_list.num; n++) {
			for (i = 0; i < sensors_available; i++) {
				if (handler_list.handle[n] == list[i]->getHandle())
				break;
			}
			if (i == sensors_available)
				ALOGE("sensor handle not found: (%d). Selftest will be skipped.", handler_list.handle[n]);
		}
	}

	for (i = 0; i < sensors_available; i++) {
		if (handler_list.num > 0) {
			for (n = 0; n < handler_list.num; n++) {
				if (handler_list.handle[n] == list[i]->getHandle())
					break;
			}
			if (n == handler_list.num)
				continue;
		}

		cmd_data.handle = list[i]->getHandle();
		cmd_data.mode = 1;
		write(fd_cmd, &cmd_data, sizeof(struct selftest_cmd_t));

		err = poll(&fds, 1, ST_SELFTEST_TOOL_TIMEOUT_ANSWER_MS);
		if (err < 0)
			continue;

		if (err == 0) {
			ALOGW("\"%s\": timeout waiting for selftest result", (const char *)list[i]->getName());
			continue;
		}

		if (fds.revents & POLLIN) {
			read_size = read(fd_results, &results_data, sizeof(struct selftest_results_t));
			if (read_size != sizeof(struct selftest_results_t)) {
				ALOGE("results format not valid");
				continue;
			}

			switch (results_data.status) {
			case NOT_AVAILABLE:
				result_string = "not available";
				break;

			case GENERIC_ERROR:
				result_string = "generic error";
				break;

			case FAILURE:
				result_string = "fail";
				break;

			case PASS:
				result_string = "pass";
				break;

			default:
				continue;
			}

			for (g = 0; g < sensors_available; g++) {
				if (list[g]->getHandle() == results_data.handle)
					break;
			}
			if (g == sensors_available) {
				ALOGE("invalid handle result from SensorHAL");
				continue;
			}

			ALOGI("\"%s\": handle=%d result=\"%s\"", (const char *)list[g]->getName(), results_data.handle, result_string);
		}
	}

	ALOGI("Selftest finished!");

	return 0;
}
