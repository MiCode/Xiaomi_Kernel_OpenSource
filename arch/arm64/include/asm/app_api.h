/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __ASM_APP_API_H
#define __ASM_APP_API_H

#include <linux/types.h>
#include <linux/sched.h>
#include <linux/fs.h>

#define APP_SETTING_BIT		30
#define MAX_ENTRIES		10

/*
 * APIs to set / clear the app setting bits
 * in the register.
 */
#ifdef CONFIG_MSM_APP_API
extern void set_app_setting_bit(uint32_t bit);
extern void clear_app_setting_bit(uint32_t bit);
#else
static inline void set_app_setting_bit(uint32_t bit) {}
static inline void clear_app_setting_bit(uint32_t bit) {}
#endif

#ifdef CONFIG_MSM_APP_SETTINGS
extern void switch_app_setting_bit(struct task_struct *prev,
				   struct task_struct *next);
extern void apply_app_setting_bit(struct file *file);
extern bool use_app_setting;
#endif

#endif
