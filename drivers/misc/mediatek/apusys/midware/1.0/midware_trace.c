/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "midware_trace.h"

void midware_trace_begin(int sub_cmd_id)
{
	char buf[TRACE_LEN];
	int len;

	if (!cfg_apusys_trace)
		return;

	len = snprintf(buf, sizeof(buf),
		       "apusys_mid|cmd_id:%d", sub_cmd_id);

	if (len >= TRACE_LEN)
		len = TRACE_LEN - 1;

	trace_async_tag(1, buf);
}

void midware_trace_end(int status)
{
	char buf[TRACE_LEN];
	int len;

	if (!cfg_apusys_trace)
		return;

	len = snprintf(buf, sizeof(buf), "apusys_mid|status:%d", status);

	if (len >= TRACE_LEN)
		len = TRACE_LEN - 1;

	trace_async_tag(0, buf);
}
