/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __ADSP_SHMEM_DEVICE_H_
#define __ADSP_SHMEM_DEVICE_H_

#define ADSP_VFE        0
#define ADSP_CSID       0
#define ADSP_CCI        0

enum camera_status_state {
	CAMERA_STATUS_STOP = 1234,
	CAMERA_STATUS_INIT,
	CAMERA_STATUS_START,
	CAMERA_STATUS_END,
};

struct adsp_camera_header {
	unsigned short debug;
	unsigned short status;
	uint32_t    frame_idx;
	char sensor_name[32];
	unsigned short width;
	unsigned short height;
	unsigned short stride;
	unsigned short format;
	uint32_t    frame_size;
	unsigned short data_type;
	uint32_t header_size;
	char    printf_buf; /* last */
};

int         adsp_shmem_get_state(void);
void        adsp_shmem_set_state(enum camera_status_state state);
const char *adsp_shmem_get_sensor_name(void);
int         adsp_shmem_is_initialized(void);

/* true if ADSP is initialized AND in state INIT or START */
int         adsp_shmem_is_working(void);

#endif /* __ADSP_SHMEM_DEVICE_H_ */
