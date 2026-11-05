/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _LINUX_SI_CORE_XTS_H__
#define _LINUX_SI_CORE_XTS_H__

#include <linux/firmware/qcom/si_object.h>

struct si_object *init_si_mem_object_user(struct dma_buf *dma_buf,
	void (*release)(void *), void *private);

/* For 'mem_object_to_dma_buf' and 'is_mem_object' caller should own the 'object',
 * (i.e. someone should have already called '__get_si_object').
 */

int is_mem_object(struct si_object *object);
struct dma_buf *mem_object_to_dma_buf(struct si_object *object);

#endif /* _LINUX_SI_CORE_XTS_H__ */
