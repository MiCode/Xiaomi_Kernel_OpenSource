// SPDX-License-Identifier: GPL-2.0
/*
 * GPU memory trace points
 *
 * Copyright (C) 2020 Google, Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 */

#include <linux/module.h>

#define CREATE_TRACE_POINTS
#include <trace/events/gpu_mem.h>

EXPORT_TRACEPOINT_SYMBOL(gpu_mem_total);
