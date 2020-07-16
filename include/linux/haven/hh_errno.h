/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 *
 */

#ifndef __HH_ERRNO_H
#define __HH_ERRNO_H

#include <linux/errno.h>

#define HH_ERROR_OK			0
#define HH_ERROR_UNIMPLEMENTED		-1

#define HH_ERROR_ARG_INVAL		1
#define HH_ERROR_ARG_SIZE		2
#define HH_ERROR_ARG_ALIGN		3

#define HH_ERROR_NOMEM			10

#define HH_ERROR_ADDR_OVFL		20
#define	HH_ERROR_ADDR_UNFL		21
#define HH_ERROR_ADDR_INVAL		22

#define HH_ERROR_DENIED			30
#define HH_ERROR_BUSY			31
#define HH_ERROR_IDLE			32

#define HH_ERROR_IRQ_BOUND		40
#define HH_ERROR_IRQ_UNBOUND		41

#define HH_ERROR_CSPACE_CAP_NULL	50
#define HH_ERROR_CSPACE_CAP_REVOKED	51
#define HH_ERROR_CSPACE_WRONG_OBJ_TYPE	52
#define HH_ERROR_CSPACE_INSUF_RIGHTS	53
#define HH_ERROR_CSPACE_FULL		54

#define HH_ERROR_MSGQUEUE_EMPTY		60
#define HH_ERROR_MSGQUEUE_FULL		61

static inline int hh_remap_error(int hh_error)
{
	switch (hh_error) {
	case HH_ERROR_OK:
		return 0;
	case HH_ERROR_NOMEM:
		return -ENOMEM;
	case HH_ERROR_DENIED:
	case HH_ERROR_CSPACE_CAP_NULL:
	case HH_ERROR_CSPACE_CAP_REVOKED:
	case HH_ERROR_CSPACE_WRONG_OBJ_TYPE:
	case HH_ERROR_CSPACE_INSUF_RIGHTS:
	case HH_ERROR_CSPACE_FULL:
		return -EACCES;
	case HH_ERROR_BUSY:
	case HH_ERROR_IDLE:
		return -EBUSY;
	case HH_ERROR_IRQ_BOUND:
	case HH_ERROR_IRQ_UNBOUND:
	case HH_ERROR_MSGQUEUE_FULL:
	case HH_ERROR_MSGQUEUE_EMPTY:
		return -EPERM;
	case HH_ERROR_UNIMPLEMENTED:
	case HH_ERROR_ARG_INVAL:
	case HH_ERROR_ARG_SIZE:
	case HH_ERROR_ARG_ALIGN:
	case HH_ERROR_ADDR_OVFL:
	case HH_ERROR_ADDR_UNFL:
	case HH_ERROR_ADDR_INVAL:
	default:
		return -EINVAL;
	}
}

#endif
