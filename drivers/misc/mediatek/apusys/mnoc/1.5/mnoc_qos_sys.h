/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#if IS_ENABLED(CONFIG_MTK_QOS_FRAMEWORK)
int mnoc_qos_create_sys(struct device *dev);
void mnoc_qos_remove_sys(struct device *dev);
#else /* !ENABLED(CONFIG_MTK_QOS_FRAMEWORK) */
static inline int mnoc_qos_create_sys(struct device *dev) { return 0; };
static inline void mnoc_qos_remove_sys(struct device *dev) {}
#endif
