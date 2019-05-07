/*
 * Copyright (c) 2017 MediaTek Inc.
 * Author: Nick Fan <nick.fan@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#ifndef MALI_KBASE_RUNTIME_PM_H
#define MALI_KBASE_RUNTIME_PM_H

extern struct devfreq_cooling_power power_model_simple_ops;

int kbase_power_model_simple_init(struct kbase_device *kbdev);

#endif /*MALI_KBASE_RUNTIME_PM_H*/
