/* Copyright (c) 2010-2011, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <mach/dal_axi.h>

/* The AXI device ID */
#define DALDEVICEID_AXI   0x02000053
#define DALRPC_PORT_NAME  "DAL00"

enum {
	DALRPC_AXI_ALLOCATE = DALDEVICE_FIRST_DEVICE_API_IDX + 1,
	DALRPC_AXI_FREE = DALDEVICE_FIRST_DEVICE_API_IDX + 2,
	DALRPC_AXI_CONFIGURE_BRIDGE = DALDEVICE_FIRST_DEVICE_API_IDX + 11
};

enum {
	DAL_AXI_BRIDGE_CFG_CGR_SS_2DGRP_SYNC_MODE = 14,
	DAL_AXI_BRIDGE_CFG_CGR_SS_2DGRP_ASYNC_MODE,
	DAL_AXI_BRIDGE_CFG_CGR_SS_2DGRP_ISOSYNC_MODE,
	DAL_AXI_BRIDGE_CFG_CGR_SS_2DGRP_DEBUG_EN,
	DAL_AXI_BRIDGE_CFG_CGR_SS_2DGRP_DEBUG_DIS,
	DAL_AXI_BRIDGE_CFG_CGR_SS_3DGRP_SYNC_MODE,
	DAL_AXI_BRIDGE_CFG_CGR_SS_3DGRP_ASYNC_MODE,
	DAL_AXI_BRIDGE_CFG_CGR_SS_3DGRP_ISOSYNC_MODE,
	DAL_AXI_BRIDGE_CFG_CGR_SS_3DGRP_DEBUG_EN,
	DAL_AXI_BRIDGE_CFG_CGR_SS_3DGRP_DEBUG_DIS,
	/* 7x27(A) Graphics Subsystem Bridge Configuration */
	DAL_AXI_BRIDGE_CFG_GRPSS_XBAR_SYNC_MODE = 58,
	DAL_AXI_BRIDGE_CFG_GRPSS_XBAR_ASYNC_MODE = 59,
	DAL_AXI_BRIDGE_CFG_GRPSS_XBAR_ISOSYNC_MODE = 60

};

static void *cam_dev_handle;
static int __axi_free(int mode)
{
	int rc = 0;

	if (!cam_dev_handle)
		return rc;

	rc = dalrpc_fcn_0(DALRPC_AXI_FREE, cam_dev_handle, mode);
	if (rc) {
		printk(KERN_ERR "%s: AXI bus device (%d) failed to be configured\n",
			__func__, rc);
		goto fail_dal_fcn_0;
	}

	/* close device handle */
	rc = daldevice_detach(cam_dev_handle);
	if (rc) {
		printk(KERN_ERR "%s: failed to detach AXI bus device (%d)\n",
			__func__, rc);
		goto fail_dal_attach_detach;
	}
	cam_dev_handle = NULL;
	return 0;

fail_dal_fcn_0:
	(void)daldevice_detach(cam_dev_handle);
	cam_dev_handle = NULL;
fail_dal_attach_detach:
	return rc;
}

static int __axi_allocate(int mode)
{
	int rc;

	/* get device handle */
	rc = daldevice_attach(DALDEVICEID_AXI, DALRPC_PORT_NAME,
				DALRPC_DEST_MODEM, &cam_dev_handle);
	if (rc) {
		printk(KERN_ERR "%s: failed to attach AXI bus device (%d)\n",
			__func__, rc);
		goto fail_dal_attach_detach;
	}

	rc = dalrpc_fcn_0(DALRPC_AXI_ALLOCATE, cam_dev_handle, mode);
	if (rc) {
		printk(KERN_ERR "%s: AXI bus device (%d) failed to be configured\n",
			__func__, rc);
		goto fail_dal_fcn_0;
	}

	return 0;

fail_dal_fcn_0:
	(void)daldevice_detach(cam_dev_handle);
	cam_dev_handle = NULL;
fail_dal_attach_detach:
	return rc;
}

static int axi_configure_bridge_grfx_sync_mode(int bridge_mode)
{
	int rc;
	void *dev_handle;

	/* get device handle */
	rc = daldevice_attach(
		DALDEVICEID_AXI, DALRPC_PORT_NAME,
		DALRPC_DEST_MODEM, &dev_handle
	);
	if (rc) {
		printk(KERN_ERR "%s: failed to attach AXI bus device (%d)\n",
			__func__, rc);
		goto fail_dal_attach_detach;
	}

	/* call ConfigureBridge */
	rc = dalrpc_fcn_0(
		DALRPC_AXI_CONFIGURE_BRIDGE, dev_handle,
		bridge_mode
	);
	if (rc) {
		printk(KERN_ERR "%s: AXI bus device (%d) failed to be configured\n",
			__func__, rc);
		goto fail_dal_fcn_0;
	}

	/* close device handle */
	rc = daldevice_detach(dev_handle);
	if (rc) {
		printk(KERN_ERR "%s: failed to detach AXI bus device (%d)\n",
			__func__, rc);
		goto fail_dal_attach_detach;
	}

	return 0;

fail_dal_fcn_0:
	(void)daldevice_detach(dev_handle);
fail_dal_attach_detach:

	return rc;
}

int axi_free(mode)
{
	return __axi_free(mode);
}

int axi_allocate(mode)
{
	return __axi_allocate(mode);
}

int set_grp2d_async(void)
{
	return axi_configure_bridge_grfx_sync_mode(
		DAL_AXI_BRIDGE_CFG_CGR_SS_2DGRP_ASYNC_MODE);
}

int set_grp3d_async(void)
{
	return axi_configure_bridge_grfx_sync_mode(
		DAL_AXI_BRIDGE_CFG_CGR_SS_3DGRP_ASYNC_MODE);
}

int set_grp_xbar_async(void)
{	return axi_configure_bridge_grfx_sync_mode(
		DAL_AXI_BRIDGE_CFG_GRPSS_XBAR_ASYNC_MODE);
}
