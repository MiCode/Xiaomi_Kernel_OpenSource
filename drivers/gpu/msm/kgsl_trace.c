// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2011,2013,2015,2019-2020 The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>

#include "kgsl_device.h"

/* Instantiate tracepoints */
#define CREATE_TRACE_POINTS
#include "kgsl_trace.h"
#include "kgsl_trace_power.h"

EXPORT_TRACEPOINT_SYMBOL(kgsl_regwrite);
EXPORT_TRACEPOINT_SYMBOL(kgsl_issueibcmds);
EXPORT_TRACEPOINT_SYMBOL(kgsl_user_pwrlevel_constraint);
EXPORT_TRACEPOINT_SYMBOL(kgsl_constraint);

EXPORT_TRACEPOINT_SYMBOL(gpu_frequency);
