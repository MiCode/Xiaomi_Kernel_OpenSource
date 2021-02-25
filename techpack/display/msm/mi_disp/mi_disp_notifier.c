/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (C) 2020 XiaoMi, Inc.
 */

#include <linux/notifier.h>

static BLOCKING_NOTIFIER_HEAD(mi_disp_notifier_list);

/**
 * mi_drm_register_client - register a client notifier
 * @nb: notifier block to callback on events
 *
 * This function registers a notifier callback function
 * to msm_drm_notifier_list, which would be called when
 * received unblank/power down event.
 */
int mi_disp_register_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&mi_disp_notifier_list, nb);
}
EXPORT_SYMBOL(mi_disp_register_client);

/**
 * mi_drm_unregister_client - unregister a client notifier
 * @nb: notifier block to callback on events
 *
 * This function unregisters the callback function from
 * msm_drm_notifier_list.
 */
int mi_disp_unregister_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&mi_disp_notifier_list, nb);
}
EXPORT_SYMBOL(mi_disp_unregister_client);

/**
 * mi_drm_notifier_call_chain - notify clients of drm_events
 * @val: event MSM_DRM_EARLY_EVENT_BLANK or MSM_DRM_EVENT_BLANK
 * @v: notifier data, inculde display id and display blank
 *     event(unblank or power down).
 */
int mi_disp_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&mi_disp_notifier_list, val, v);
}
EXPORT_SYMBOL(mi_disp_notifier_call_chain);