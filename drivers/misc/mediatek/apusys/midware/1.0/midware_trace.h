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

#include <linux/string.h>
#include <linux/kernel.h>
#include "cmd_parser.h"
#include "apusys_trace.h"

extern void trace_tag_begin(const char *format, ...);
extern void trace_tag_end(void);
extern u8 cfg_apusys_trace;

void midware_trace_begin(int sub_cmd_id);
void midware_trace_end(int status);
