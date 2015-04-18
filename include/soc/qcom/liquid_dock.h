/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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

#include <linux/notifier.h>

#if IS_ENABLED(CONFIG_QCOM_LIQUID_DOCK)
void register_liquid_dock_notify(struct notifier_block *nb);
void unregister_liquid_dock_notify(struct notifier_block *nb);
#else
static inline void register_liquid_dock_notify(struct notifier_block *nb) { }
static inline void unregister_liquid_dock_notify(struct notifier_block *nb) { }
#endif
