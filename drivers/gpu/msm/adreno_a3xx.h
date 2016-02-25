/* Copyright (c) 2013-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __A3XX_H
#define __A3XX_H

unsigned int a3xx_irq_pending(struct adreno_device *adreno_dev);

int a3xx_microcode_read(struct adreno_device *adreno_dev);
int a3xx_microcode_load(struct adreno_device *adreno_dev,
				unsigned int start_type);
int a3xx_perfcounter_enable(struct adreno_device *adreno_dev,
	unsigned int group, unsigned int counter, unsigned int countable);
uint64_t a3xx_perfcounter_read(struct adreno_device *adreno_dev,
	unsigned int group, unsigned int counter);

void a3xx_a4xx_err_callback(struct adreno_device *adreno_dev, int bit);

void a3xx_snapshot(struct adreno_device *adreno_dev,
		struct kgsl_snapshot *snapshot);
#endif /*__A3XX_H */
