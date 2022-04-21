/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
 * All Rights Reserved.
 *
 */

#ifndef _ISEE_KERNEL_API_H_

#include <tee_client_api.h>

int is_teei_ready(void);
unsigned long tz_get_share_buffer(unsigned int driver_id);
int tz_wait_for_notification(unsigned int driver_id);
int tz_notify_driver(unsigned int driver_id);
int tz_create_share_buffer(unsigned int driver_id, unsigned int buff_size);
int tz_free_share_buffer(unsigned int driver_id);
int tz_load_drv(struct TEEC_UUID *uuid);
int tz_unload_drv(struct TEEC_UUID *uuid);

#endif /* _ISEE_KERNEL_API_H_ */
