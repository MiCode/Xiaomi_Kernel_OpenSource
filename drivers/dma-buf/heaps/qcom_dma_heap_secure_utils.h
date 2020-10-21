/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#ifndef _QCOM_DMA_HEAP_SECURE_UTILS_H
#define _QCOM_DMA_HEAP_SECURE_UTILS_H

int hyp_assign_from_flags(u64 base, u64 size, unsigned long flags);
bool qcom_is_buffer_hlos_accessible(unsigned long flags);

#endif /* _QCOM_DMA_HEAP_SECURE_UTILS_H */
