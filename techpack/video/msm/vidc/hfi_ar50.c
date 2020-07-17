// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#include "hfi_common.h"
#include "hfi_io_common.h"

void __interrupt_init_ar50(struct venus_hfi_device *device, u32 sid)
{
	__write_register(device, WRAPPER_INTR_MASK,
			WRAPPER_INTR_MASK_A2HVCODEC_BMSK, sid);
}
