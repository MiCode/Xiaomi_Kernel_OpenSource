/*
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef TZ_TEST_MAIN_H
#define TZ_TEST_MAIN_H

#define TZ_TEST_NAME     "tz_test"

struct tzdrv_test_data {
	unsigned int params[32];
	unsigned char error_info[128];
	int tee_error_code;
	int tee_spend_time_ms;
};

#define TZDRV_IOC_MAGIC 'T'
#define TZDRV_CMD_KERNEL_CA_TEST _IOWR(TZDRV_IOC_MAGIC, 2,\
					 struct tzdrv_test_data)
#define TZDRV_CMD_SECURE_DRV_TEST _IOWR(TZDRV_IOC_MAGIC, 3,\
					 struct tzdrv_test_data)

#define TZDRV_IOC_MAXNR	(10)

extern int kernel_ca_test(struct tzdrv_test_data *param);
extern int secure_drv_test(struct tzdrv_test_data *param);
#define IFASSIGN(a, b) (a = b)

#endif	/* end of TZ_TEST_MAIN_H */
