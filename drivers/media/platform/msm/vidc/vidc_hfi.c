/* Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/slab.h>
#include "msm_vidc_debug.h"
#include "vidc_hfi_api.h"
#include "venus_hfi.h"

struct hfi_device *vidc_hfi_initialize(enum msm_vidc_hfi_type hfi_type,
		u32 device_id, struct msm_vidc_platform_resources *res,
		hfi_cmd_response_callback callback)
{
	struct hfi_device *hdev = NULL;
	int rc = 0;

	hdev = (struct hfi_device *)
			kzalloc(sizeof(struct hfi_device), GFP_KERNEL);
	if (!hdev) {
		dprintk(VIDC_ERR, "%s: failed to allocate hdev\n", __func__);
		return NULL;
	}

	switch (hfi_type) {
	case VIDC_HFI_VENUS:
		rc = venus_hfi_initialize(hdev, device_id, res, callback);
		break;
	default:
		dprintk(VIDC_ERR, "Unsupported host-firmware interface\n");
		goto err_hfi_init;
	}

	if (rc) {
		if (rc != -EPROBE_DEFER)
			dprintk(VIDC_ERR, "%s device init failed rc = %d",
				__func__, rc);
		goto err_hfi_init;
	}

	return hdev;

err_hfi_init:
	kfree(hdev);
	return ERR_PTR(rc);
}

void vidc_hfi_deinitialize(enum msm_vidc_hfi_type hfi_type,
			struct hfi_device *hdev)
{
	if (!hdev) {
		dprintk(VIDC_ERR, "%s invalid device %pK", __func__, hdev);
		return;
	}

	switch (hfi_type) {
	case VIDC_HFI_VENUS:
		venus_hfi_delete_device(hdev->hfi_device_data);
		break;
	default:
		dprintk(VIDC_ERR, "Unsupported host-firmware interface\n");
	}

	kfree(hdev);
}

