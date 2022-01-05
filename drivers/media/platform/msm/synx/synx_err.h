/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __SYNX_ERR_H__
#define __SYNX_ERR_H__

#include <linux/err.h>

/**
 * Error codes returned from framework
 *
 * Return codes are mapped to platform specific
 * return values.
 */
#define SYNX_SUCCESS   0
#define SYNX_NOMEM     ENOMEM
#define SYNX_NOSUPPORT EOPNOTSUPP
#define SYNX_NOPERM    EPERM
#define SYNX_TIMEOUT   ETIMEDOUT
#define SYNX_ALREADY   EALREADY
#define SYNX_NOENT     ENOENT
#define SYNX_INVALID   EINVAL
#define SYNX_BUSY      EBUSY

#endif /* __SYNX_ERR_H__ */
