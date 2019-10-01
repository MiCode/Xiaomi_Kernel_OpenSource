// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */

#include "msm_cvp_common.h"
#include "cvp_hfi_api.h"
#include "msm_cvp_debug.h"
#include "msm_cvp_clocks.h"

int msm_cvp_set_clocks(struct msm_cvp_core *core)
{
	struct cvp_hfi_device *hdev;
	int rc;

	if (!core || !core->device) {
		dprintk(CVP_ERR, "%s Invalid args: %pK\n", __func__, core);
		return -EINVAL;
	}

	hdev = core->device;
	rc = call_hfi_op(hdev, scale_clocks,
		hdev->hfi_device_data, core->curr_freq);
	return rc;
}
