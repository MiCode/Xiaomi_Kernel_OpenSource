// SPDX-License-Identifier: GPL-2.0-only
/* vendor_hook.c
 *
 * Copyright 2022 Google LLC
 */
#include "blk.h"
#include "blk-mq-tag.h"
#include "blk-mq.h"
#include <linux/blk-mq.h>

#define CREATE_TRACE_POINTS
#include <trace/hooks/vendor_hooks.h>
#include <linux/tracepoint.h>
#include <trace/hooks/block.h>

EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_blk_alloc_rqs);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_blk_rq_ctx_init);

/*
 * For type visibility
 */
const struct blk_mq_alloc_data *GKI_struct_blk_mq_alloc_data;
EXPORT_SYMBOL_GPL(GKI_struct_blk_mq_alloc_data);

