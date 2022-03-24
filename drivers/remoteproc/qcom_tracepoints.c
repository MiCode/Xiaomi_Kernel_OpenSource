// SPDX-License-Identifier: GPL-2.0-only
/*
 * Qualcomm Technologies, Inc. Peripheral Image Loader helpers
 *
 * Copyright (c) 2021 The Linux Foundation. All rights reserved.
 */

#if IS_MODULE(CONFIG_QCOM_RPROC_COMMON)
#define CREATE_TRACE_POINTS
#include <trace/events/rproc_qcom.h>
EXPORT_TRACEPOINT_SYMBOL(rproc_qcom_event);
#endif
