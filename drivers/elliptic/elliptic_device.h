/**
 * Copyright Elliptic Labs
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 */

#pragma once

#include <linux/ioctl.h>

#define ELLIPTIC_DEVICENAME "elliptic"

#define IOCTL_ELLIPTIC_APP	197
#define MIRROR_TAG		0x3D0A4842

#define IOCTL_ELLIPTIC_DATA_IO_CANCEL _IO(IOCTL_ELLIPTIC_APP, 2)
#define IOCTL_ELLIPTIC_ACTIVATE_ENGINE _IOW(IOCTL_ELLIPTIC_APP, 3, int)
#define IOCTL_ELLIPTIC_DATA_IO_MIRROR _IOW(IOCTL_ELLIPTIC_APP, 117, unsigned char *)
