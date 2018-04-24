/*
 * STMicroelectronics calibration daemon
 *
 * Copyright 2015-2016 STMicroelectronics Inc.
 * Author: Denis Ciocca - <denis.ciocca@st.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <poll.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/inotify.h>
#include <cutils/log.h>

#include "../configuration.h"
#include "../src/common_data.h"

#define ST_CALIBRATION_SLEEP_USEC			(3000000)
#define ST_CALIBRATION_PERSIST_DIR			"/persist/STCalibration"
#define ST_CALIBRATION_APK_DIR				"/data/data/com.st.mems.st_calibrationtool/files"

#define ST_CALIBRATION_PERSIST_ACCEL_FILENAME		"accel.txt"
#define ST_CALIBRATION_PERSIST_GYRO_FILENAME		"gyro.txt"

#define EVENT_SIZE					(sizeof(struct inotify_event))
#define BUF_LEN						(1024 * (EVENT_SIZE + 16))

#define ARRAY_SIZE(x)					((sizeof(x)/sizeof(0[x])) / \
								((size_t)(!(sizeof(x) % sizeof(0[x])))))

static const struct st_sensors_daemon_filename {
	char *name;
} st_sensors_daemon_filename[] = {
	{ .name = ST_CALIBRATION_PERSIST_ACCEL_FILENAME, },
	{ .name = ST_CALIBRATION_PERSIST_GYRO_FILENAME, },
};

static int st_sesors_daemon_verify_filename(char *filename)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(st_sensors_daemon_filename); i++) {
		if (strcmp(filename, st_sensors_daemon_filename[i].name) == 0)
			break;
	}
	if (i == ARRAY_SIZE(st_sensors_daemon_filename))
		return -EINVAL;

	return 0;
}

/*
 * st_sensors_daemon_copy_file_to_folder: update file from /persist to /data
 * 			or update file from /data/data/com.st.* to /persist
 * @filename: calibration filename to update.
 * @where: copy from /persist to /data (0), copy from /data/data/com.st.* to /persist (1).
 */
static void st_sensors_daemon_copy_file_to_folder(char *filename, int where)
{
	int err;
	uint8_t *buffer;
	FILE *f_in, *f_out;
	struct stat stat_buffer;
	char *data_file_to_read, *data_file_to_write, *dir1, *dir2;

	err = st_sesors_daemon_verify_filename(filename);
	if (err < 0)
		return;

	if (where == 0) {
		dir1 = ST_CALIBRATION_PERSIST_DIR;
		dir2 = ST_HAL_FACTORY_DATA_PATH;
	} else {
		dir1 = ST_CALIBRATION_APK_DIR;
		dir2 = ST_CALIBRATION_PERSIST_DIR;
	}

	err = asprintf(&data_file_to_read, "%s/%s", dir1, filename);
	if (err < 0)
		return;

	err = asprintf(&data_file_to_write, "%s/%s", dir2, filename);
	if (err < 0)
		goto free_data_file_to_read;

	err = stat(data_file_to_read, &stat_buffer);
	if (err < 0)
		goto free_data_file_to_write;

	if (stat_buffer.st_size <= 0)
		goto free_data_file_to_write;

	buffer = (uint8_t *)malloc(sizeof(uint8_t) * stat_buffer.st_size);
	if (!buffer)
		goto free_data_file_to_write;

	f_in = fopen(data_file_to_read, "r");
	if (!f_in)
		goto free_buffer_data;

	f_out = fopen(data_file_to_write, "w");
	if (!f_out)
		goto close_data_file_to_read;

	err = fread(buffer, stat_buffer.st_size, 1, f_in);
	if (err <= 0)
		goto close_data_file_to_write;

	err = fwrite(buffer, stat_buffer.st_size, 1, f_out);
	if (err <= 0)
		goto close_data_file_to_write;

#if (CONFIG_ST_HAL_DEBUG_LEVEL >= ST_HAL_DEBUG_INFO)
	ALOGI("factory calibration file updated: %s", data_file_to_write);
#endif /* CONFIG_ST_HAL_DEBUG_LEVEL */

close_data_file_to_write:
	fclose(f_out);
close_data_file_to_read:
	fclose(f_in);
free_buffer_data:
	free(buffer);
free_data_file_to_write:
	free(data_file_to_write);
free_data_file_to_read:
	free(data_file_to_read);

	return;
}

/*
 * st_sensors_daemon_remove_file_from_folder: remove file from /persist or /data
 * @filename: calibration filename to remove.
 * @where: remove from /data (0), remove from /persist (1).
 */
static void st_sensors_daemon_remove_file_from_folder(char *filename, int where)
{
	int err;
	char *data_file_to_remove, *dir;

	err = st_sesors_daemon_verify_filename(filename);
	if (err < 0)
		return;

	if (where == 0)
		dir = ST_HAL_FACTORY_DATA_PATH;
	else
		dir = ST_CALIBRATION_PERSIST_DIR;

	err = asprintf(&data_file_to_remove, "%s/%s", dir, filename);
	if (err < 0)
		return;

	err = remove(data_file_to_remove);
	if (err < 0) {
		ALOGE("failed to remove factory calibration file (%s).", data_file_to_remove);
		free(data_file_to_remove);
		return;
	}

#if (CONFIG_ST_HAL_DEBUG_LEVEL >= ST_HAL_DEBUG_INFO)
	ALOGI("factory calibration file removed: %s", data_file_to_remove);
#endif /* CONFIG_ST_HAL_DEBUG_LEVEL */

	free(data_file_to_remove);
}

int main(int __attribute__((unused))argc, char __attribute__((unused))*argv[])
{
	char buffer[BUF_LEN];
	struct stat stat_buffer;
	struct inotify_event *event;
	int err = 0, i, inotify_fd, inotify_watch_fd, length;
#ifdef CONFIG_ST_HAL_FACTORY_CALIBRATION_USING_APK
	struct pollfd fds[2];
	struct stat stat_buffer_apk;
	int j, inotify_apk_fd, inotify_apk_watch_fd;
#endif /* CONFIG_ST_HAL_FACTORY_CALIBRATION_USING_APK */

#if (CONFIG_ST_HAL_DEBUG_LEVEL >= ST_HAL_DEBUG_INFO)
	ALOGI("ST factory calibration deamon start.");
#endif /* CONFIG_ST_HAL_DEBUG_LEVEL */

#ifdef CONFIG_ST_HAL_FACTORY_CALIBRATION_USING_APK
	while ((stat(ST_HAL_DATA_PATH, &stat_buffer) < 0) ||
			(stat(ST_CALIBRATION_APK_DIR, &stat_buffer_apk) < 0))
		usleep(ST_CALIBRATION_SLEEP_USEC);
#else /* CONFIG_ST_HAL_FACTORY_CALIBRATION_USING_APK */
	while (stat(ST_HAL_DATA_PATH, &stat_buffer) < 0)
		usleep(ST_CALIBRATION_SLEEP_USEC);
#endif /* CONFIG_ST_HAL_FACTORY_CALIBRATION_USING_APK */

	err = mkdir(ST_HAL_FACTORY_DATA_PATH, S_IRWXU);
	if (err < 0) {
		if (errno != EEXIST)
			ALOGE("Failed to create \"%s\" directory (errno: %d).", ST_HAL_FACTORY_DATA_PATH, -errno);
	}
#ifdef CONFIG_ST_HAL_FACTORY_CALIBRATION_USING_APK
	err = mkdir(ST_CALIBRATION_PERSIST_DIR, S_IRWXU);
	if (err < 0) {
		if (errno != EEXIST)
			ALOGE("Failed to create \"%s\" directory (errno: %d).", ST_CALIBRATION_PERSIST_DIR, -errno);
	}
#endif /* CONFIG_ST_HAL_FACTORY_CALIBRATION_USING_APK */

	inotify_fd = inotify_init();
	if (inotify_fd < 0)
		return -errno;

#ifdef CONFIG_ST_HAL_FACTORY_CALIBRATION_USING_APK
	inotify_apk_fd = inotify_init();
	if (inotify_apk_fd < 0)
		return -errno;
#endif /* CONFIG_ST_HAL_FACTORY_CALIBRATION_USING_APK */

	inotify_watch_fd = inotify_add_watch(inotify_fd,
					ST_CALIBRATION_PERSIST_DIR,
					IN_MODIFY | IN_CREATE | IN_DELETE );
	if (inotify_watch_fd < 0)
		return -errno;

#ifdef CONFIG_ST_HAL_FACTORY_CALIBRATION_USING_APK
	inotify_apk_watch_fd = inotify_add_watch(inotify_apk_fd,
					ST_CALIBRATION_APK_DIR,
					IN_MODIFY | IN_CREATE | IN_DELETE );
	if (inotify_apk_watch_fd < 0) {
		err = -errno;
		goto remove_inotify_watch;
	}

	fds[0].fd = inotify_fd;
	fds[0].events = POLLIN;

	fds[1].fd = inotify_apk_fd;
	fds[1].events = POLLIN;
#endif /* CONFIG_ST_HAL_FACTORY_CALIBRATION_USING_APK */

	while (1) {
#ifdef CONFIG_ST_HAL_FACTORY_CALIBRATION_USING_APK
		err = poll(fds, 2, -1);
		if (err < 0)
			continue;

		for (j = 0; j < 2; j++) {
			if (fds[j].revents & POLLIN) {
				length = read(fds[j].fd, buffer, BUF_LEN);
				if (length <= 0)
					continue;

				i = 0;
				while (i < length) {
					event = (struct inotify_event *)&buffer[i];
					if ((event->len > 0) && !(event->mask & IN_ISDIR)) {
						if (event->mask & IN_CREATE) {
							st_sensors_daemon_copy_file_to_folder(event->name, j);
						} else if (event->mask & IN_DELETE) {
							st_sensors_daemon_remove_file_from_folder(event->name, j);
						} else if (event->mask & IN_MODIFY) {
							st_sensors_daemon_copy_file_to_folder(event->name, j);
						}
					}
					i += EVENT_SIZE + event->len;
				}
			}
		}
#else /* CONFIG_ST_HAL_FACTORY_CALIBRATION_USING_APK */
		length = read(inotify_fd, buffer, BUF_LEN);
		if (length <= 0)
			continue;

		i = 0;
		while (i < length) {
			event = (struct inotify_event *)&buffer[i];
			if ((event->len > 0) && !(event->mask & IN_ISDIR)) {
				if (event->mask & IN_CREATE) {
					st_sensors_daemon_copy_file_to_folder(event->name, 0);
				} else if (event->mask & IN_DELETE) {
					st_sensors_daemon_remove_file_from_folder(event->name, 0);
				} else if (event->mask & IN_MODIFY) {
					st_sensors_daemon_copy_file_to_folder(event->name, 0);
				}
			}
			i += EVENT_SIZE + event->len;
		}
#endif /* CONFIG_ST_HAL_FACTORY_CALIBRATION_USING_APK */
	}

remove_inotify_watch:
	inotify_rm_watch(inotify_fd, inotify_watch_fd);

	return err < 0 ? err : 0;
}
