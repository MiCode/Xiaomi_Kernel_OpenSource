/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#ifndef _MSM_HEAP_HELPERS_H
#define _MSM_HEAP_HELPERS_H

#include "heap-helpers.h"

/**
 * msm_heap_helper_buffer - wrapper struct of struct heap_helper_buffer
 * @struct heap_helper_buffer:	the heap_helper_buffer struct we contain
 * @flags:			the flags returned to dma_buf_get_flags()
 */

struct msm_heap_helper_buffer {
	struct heap_helper_buffer buffer;
	int flags;
};

#endif /* _MSM_HEAP_HELPERS_H */
