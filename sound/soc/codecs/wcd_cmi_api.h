/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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

#ifndef __CMI_API__
#define __CMI_API__

enum cmi_api_result {
	CMI_API_FAILED = 1,
	CMI_API_BUSY,
	CMI_API_NO_MEMORY,
	CMI_API_NOT_READY,
};

enum cmi_api_event {
	CMI_API_MSG = 1,
	CMI_API_OFFLINE,
	CMI_API_ONLINE,
	CMI_API_DEINITIALIZED,
};

struct cmi_api_notification {
	enum cmi_api_event event;
	enum cmi_api_result result;
	void *message;
};

void *cmi_register(
	void notification_callback
		(const struct cmi_api_notification *parameter),
	u32 service);
enum cmi_api_result cmi_deregister(void *reg_handle);
enum cmi_api_result cmi_send_msg(void *message);

#endif /*__CMI_API__*/
