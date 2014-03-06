/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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

irqreturn_t a3xx_irq_handler(struct adreno_device *adreno_dev);
void a3xx_irq_control(struct adreno_device *adreno_dev, int state);
unsigned int a3xx_irq_pending(struct adreno_device *adreno_dev);
void a3xx_busy_cycles(struct adreno_device *adreno_dev,
			struct adreno_busy_data *);

int a3xx_rb_init(struct adreno_device *adreno_dev,
			struct adreno_ringbuffer *rb);
int a3xx_perfcounter_init(struct adreno_device *adreno_dev);
void a3xx_perfcounter_close(struct adreno_device *adreno_dev);
int a3xx_perfcounter_enable(struct adreno_device *adreno_dev,
	unsigned int group, unsigned int counter, unsigned int countable);
uint64_t a3xx_perfcounter_read(struct adreno_device *adreno_dev,
	unsigned int group, unsigned int counter);
void a3xx_perfcounter_disable(struct adreno_device *adreno_dev,
	unsigned int group, unsigned int counter);
void a3xx_perfcounter_save(struct adreno_device *adreno_dev);
void a3xx_perfcounter_restore(struct adreno_device *adreno_dev);

void a3xx_soft_reset(struct adreno_device *adreno_dev);
void a3xx_irq_setup(struct adreno_device *adreno_dev);
void a3xx_a4xx_err_callback(struct adreno_device *adreno_dev, int bit);
void a3xx_fatal_err_callback(struct adreno_device *adreno_dev, int bit);
void a3xx_gpu_idle_callback(struct adreno_device *adreno_dev, int irq);
void a3xx_cp_callback(struct adreno_device *adreno_dev, int irq);

void a3xx_fault_detect_start(struct adreno_device *adreno_dev);
void a3xx_fault_detect_stop(struct adreno_device *adreno_dev);

#endif /*__A3XX_H */
