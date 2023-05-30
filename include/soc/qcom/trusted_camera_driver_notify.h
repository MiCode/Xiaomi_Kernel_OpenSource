/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __TRUSTED_CAMERA_DRIVER_NOTIFY_H
#define __TRUSTED_CAMERA_DRIVER_NOTIFY_H

#include <soc/qcom/smci_object.h>

#define ITRUSTEDCAMERADRIVERNOTIFY_OP_WIPEMEMORY 0

static inline int32_t
trusted_camera_driver_notify_release(struct smci_object self)
{
	return smci_object_invoke(self, SMCI_OBJECT_OP_RELEASE, 0, 0);
}

static inline int32_t
trusted_camera_driver_notify_retain(struct smci_object self)
{
	return smci_object_invoke(self, SMCI_OBJECT_OP_RETAIN, 0, 0);
}

static inline int32_t
trusted_camera_driver_notify_wipememory(struct smci_object self)
{
	return smci_object_invoke(self, ITRUSTEDCAMERADRIVERNOTIFY_OP_WIPEMEMORY, 0, 0);
}
#endif /* __TRUSTED_CAMERA_DRIVER_NOTIFY_H */
