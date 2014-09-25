/*
 * linux/drivers/modem_control/mcd_cpu.h
 *
 * Version 1.0
 *
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
 *
 * Contact: Ranquet Guillaume <guillaumex.ranquet@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 */

#ifndef _MDM_CPU_H
#define _MDM_CPU_H
int cpu_init_gpio(void *data);
int cpu_cleanup_gpio(void *data);
int get_gpio_irq_cdump(void *data);
int get_gpio_irq_rst(void *data);
int get_gpio_mdm_state(void *data);
int get_gpio_rst(void *data);
int get_gpio_pwr(void *data);
int cpu_init_gpio_ngff(void *data);
int cpu_cleanup_gpio_ngff(void *data);
int get_gpio_mdm_state_ngff(void *data);
int get_gpio_irq_cdump_ngff(void *data);
int get_gpio_irq_rst_ngff(void *data);
#endif
