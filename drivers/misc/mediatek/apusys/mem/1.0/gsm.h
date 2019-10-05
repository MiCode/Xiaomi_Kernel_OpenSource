/*
 * Copyright (C) 2018 MediaTek Inc.
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

#ifndef __GSM_H__
#define __GSM_H__

#include <linux/types.h>
#include <linux/sizes.h>
#include "mdla_ioctl.h"

#define GSM_SIZE        (SZ_1M)
#define GSM_MVA_BASE    (0x1D000000)
#define GSM_MVA_INVALID (0xFFFFFFFF)

void *gsm_alloc(size_t size);
void *gsm_mva_to_virt(u32 mva);
int gsm_release(void *vaddr, size_t size);
int mdla_gsm_alloc(struct ioctl_malloc *malloc_data);
void mdla_gsm_free(struct ioctl_malloc *malloc_data);

#define is_gsm_mva(a) \
	(likely(((a) >= GSM_MVA_BASE) && ((a) < (GSM_MVA_BASE + SZ_1M))))

#endif /* KMOD_GSM_H_ */

