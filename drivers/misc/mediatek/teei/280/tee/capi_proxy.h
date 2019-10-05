/*
 * Copyright (c) 2015-2016 MICROTRUST Incorporated
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
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
