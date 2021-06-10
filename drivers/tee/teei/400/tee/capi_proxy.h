/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
 * All Rights Reserved.
 *
 */

#ifndef CAPI_PROXY_H
#define CAPI_PROXY_H

#include <tee_drv.h>

enum {
	CAPI_OP_INIT_CONTEXT,
	CAPI_OP_FINAL_CONTEXT,
	CAPI_OP_OPEN_SESSION,
	CAPI_OP_INVOKE_COMMAND,
	CAPI_OP_CLOSE_SESSION,
	CAPI_OP_REGISTER_SHM,
	CAPI_OP_ALLOCATE_SHM,
	CAPI_OP_RELEASE_SHM,
};

int tee_ioctl_capi_proxy(struct tee_context *ctx,
			struct tee_ioctl_capi_proxy_arg __user *uarg);

#endif	/* end of CAPI_PROXY_H */
