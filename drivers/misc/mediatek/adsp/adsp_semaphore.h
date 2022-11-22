/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __ADSP_SEMAPHORE_H
#define __ADSP_SEMAPHORE_H

int get_adsp_semaphore(unsigned int flags);
int release_adsp_semaphore(unsigned int flags);

int adsp_semaphore_init(unsigned int way_bits,
			unsigned int ctrl_bit,
			unsigned int timeout);

int get_adsp_clock_semaphore(void);
int release_adsp_clock_semaphore(void);

#endif
