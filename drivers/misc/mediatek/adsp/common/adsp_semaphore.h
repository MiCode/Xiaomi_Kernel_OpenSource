/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __ADSP_SEMAPHORE_H
#define __ADSP_SEMAPHORE_H

struct adsp_sem_info {
	unsigned int way_bits;
	unsigned int ctrl_bit;
	unsigned int timeout;
	void __iomem *reg;
};

int adsp_sem_init(unsigned int way_bits,
		unsigned int ctrl_bit,
		unsigned int timeout,
		void __iomem *reg);

#endif
