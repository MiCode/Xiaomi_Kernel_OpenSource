/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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
