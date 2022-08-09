/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef SLATECOM_INTF_H
#define SLATECOM_INTF_H

#include <linux/notifier.h>

enum slate_submodule_state {
	DSP_ERROR,
	DSP_READY,
	BT_ERROR,
	BT_READY,
};

/* Use the slatecom_register_notifier API to register for slate
 * submodule state
 * This API will return a handle that can be used to un-reg for events
 * using the slatecom_unregister_notifer API by passing in that handle
 * as an argument.
 */
#if IS_ENABLED(CONFIG_MSM_SLATECOM_INTERFACE)

void *slatecom_register_notifier(struct notifier_block *nb);
int slatecom_unregister_notifier(void *handle, struct notifier_block *nb);

#else

static inline void *slatecom_register_notifier(struct notifier_block *nb)
{
	return ERR_PTR(-ENODEV);
}

static inline int slatecom_unregister_notifier(void *handle,
						struct notifier_block *nb)
{
	return -ENODEV;
}

#endif /* CONFIG_MSM_SLATECOM_INTERFACE */

#endif /* SLATECOM_INTF_H */
