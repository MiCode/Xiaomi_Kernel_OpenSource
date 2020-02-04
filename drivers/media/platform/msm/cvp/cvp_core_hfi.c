// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/slab.h>
#include "msm_cvp_debug.h"
#include "cvp_hfi_api.h"
#include "cvp_core_hfi.h"

struct cvp_hfi_device *cvp_hfi_initialize(enum msm_cvp_hfi_type hfi_type,
		u32 device_id, struct msm_cvp_platform_resources *res,
		hfi_cmd_response_callback callback)
{
	struct cvp_hfi_device *hdev = NULL;
	int rc = 0;

	hdev = kzalloc(sizeof(struct cvp_hfi_device), GFP_KERNEL);
	if (!hdev) {
		dprintk(CVP_ERR, "%s: failed to allocate hdev\n", __func__);
		return NULL;
	}

	rc = cvp_iris_hfi_initialize(hdev, device_id, res, callback);

	if (rc) {
		if (rc != -EPROBE_DEFER)
			dprintk(CVP_ERR, "%s device init failed rc = %d",
				__func__, rc);
		goto err_hfi_init;
	}

	return hdev;

err_hfi_init:
	kfree(hdev);
	return ERR_PTR(rc);
}

void cvp_hfi_deinitialize(enum msm_cvp_hfi_type hfi_type,
			struct cvp_hfi_device *hdev)
{
	if (!hdev) {
		dprintk(CVP_ERR, "%s invalid device %pK", __func__, hdev);
		return;
	}

	cvp_iris_hfi_delete_device(hdev->hfi_device_data);

	kfree(hdev);
}

