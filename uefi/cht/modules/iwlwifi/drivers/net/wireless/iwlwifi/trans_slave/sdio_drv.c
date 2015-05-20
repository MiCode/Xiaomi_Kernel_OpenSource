/******************************************************************************
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2007 - 2014 Intel Corporation. All rights reserved.
 * Copyright(c) 2013 - 2014 Intel Mobile Communications GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110,
 * USA
 *
 * The full GNU General Public License is included in this distribution
 * in the file called COPYING.
 *
 * Contact Information:
 *  Intel Linux Wireless <ilw@linux.intel.com>
 * Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497
 *
 * BSD LICENSE
 *
 * Copyright(c) 2005 - 2014 Intel Corporation. All rights reserved.
 * Copyright(c) 2013 - 2014 Intel Mobile Communications GmbH
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name Intel Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/

#include <linux/module.h>
#include <linux/mmc/sdio_func.h>
#include <linux/mmc/sdio_ids.h>
#include <linux/mmc/host.h>
#include <linux/pm_runtime.h>

#ifdef CONFIG_X86_MRFLD
#include <linux/wlan_plat.h>
#include <linux/platform_device.h>
#endif

#include "sdio_internal.h"
#include "iwl-trans.h"
#include "iwl-config.h"
#include "iwl-constants.h"

/*
 * Vendor ID and SDIO compatible devices
 */
#define INTEL_SDIO_VENDOR_ID	(0x89)
#define INTEL_SDIO_DRIVER_NAME  "iwlwifi_sdio"

#define IWL_SDIO_DEVICE(dev_id, cfg) \
	.class = SDIO_ANY_ID, \
	.vendor = INTEL_SDIO_VENDOR_ID, \
	.device = (dev_id), \
	.driver_data = (kernel_ulong_t)&(cfg)

/*
 * Device IDs table for the SDIO bus enumeration
 */
static const struct sdio_device_id iwl_sdio_device_ids[] = {
	{IWL_SDIO_DEVICE(0x3160, iwl3160_2ac_cfg)},
	{IWL_SDIO_DEVICE(0x7260, iwl7260_2ac_cfg)},
	{IWL_SDIO_DEVICE(0x5501, iwl4165_2ac_sdio_cfg)},
	{IWL_SDIO_DEVICE(0x5502, iwl8260_2ac_sdio_cfg)},
	/* zero ending */
	{},
};
MODULE_DEVICE_TABLE(sdio, iwl_sdio_device_ids);

#ifdef CONFIG_PM_SLEEP
/*
 * Suspend flow function for SDIO.
 *
 * @dev - Device to suspend.
 */
static int iwl_sdio_suspend(struct device *dev)
{
	struct sdio_func *func = dev_to_sdio_func(dev);
	struct iwl_trans *trans = sdio_get_drvdata(func);
	struct iwl_trans_slv *trans_slv = IWL_TRANS_GET_SLV_TRANS(trans);
	int ret;

	/*
	 * There seems to be some platform issue that prevents the mmc
	 * from resuming properly when MMC_PM_KEEP_POWER is not set.
	 * Workaround it by always setting the flag, even when not needed.
	 * Since it's a work around, abort suspend only if wowlan is enabled.
	 */
	ret = sdio_set_host_pm_flags(func, MMC_PM_KEEP_POWER);
	if (ret && trans_slv->wowlan_enabled) {
		IWL_WARN_DEV(dev, "Unable to set MMC_PM_KEEP_POWER\n");
		return ret;
	}

	_iwl_sdio_suspend(trans);
	return 0;
}

/*
 * Resume flow function for SDIO.
 *
 * @dev - Device to resume.
 */
static int iwl_sdio_resume(struct device *dev)
{
	struct sdio_func *func = dev_to_sdio_func(dev);
	struct iwl_trans *trans = sdio_get_drvdata(func);
	struct mmc_host *host;

	_iwl_sdio_resume(trans);

	host = func->card->host;
	host->pm_flags &= ~MMC_PM_KEEP_POWER;

	return 0;
}
#endif

/*
 * Probe flow function for SDIO.
 * The transport layer shouldl be initialized after this call.
 *
 *@func - SDIO bus function struct.
 *@id - SDIO Device id (one that was registered).
 */
static int iwl_sdio_probe(struct sdio_func *func,
			  const struct sdio_device_id *id)
{
	const struct iwl_cfg *cfg = (struct iwl_cfg *)(id->driver_data);
	struct iwl_trans_sdio *trans_sdio;
	struct iwl_trans *trans;
	int ret;

	/* Allocate generic transport layer */
	trans = iwl_trans_sdio_alloc(func, id, cfg);
	if (IS_ERR(trans))
		return PTR_ERR(trans);

	trans_sdio = IWL_TRANS_GET_SDIO_TRANS(trans);

	/* Set the generic transport as the private data of the
	 * the sdio function */
	sdio_set_drvdata(func, trans);

	/* Read CSR_HW_REV prior to fw request flow */
	ret = iwl_sdio_read_hw_rev_nic_off(trans);
	if (ret)
		goto out_free_trans;

	trans_sdio->drv = iwl_drv_start(trans, cfg);
	if (IS_ERR(trans_sdio->drv)) {
		ret = PTR_ERR(trans_sdio->drv);
		goto out_free_trans;
	}

	/* register transport layer debugfs here */
	ret = iwl_trans_slv_dbgfs_register(trans, trans->dbgfs_dir);
	if (ret)
		goto out_free_drv;

	IWL_INFO(trans, "SDIO probing completed successfully\n");
	return 0;

out_free_drv:
	iwl_drv_stop(trans_sdio->drv);
out_free_trans:
	iwl_trans_sdio_free(trans);
	sdio_set_drvdata(func, NULL);

	__iwl_err(&func->dev, 0, 0, "Failed to complete SDIO probe\n");
	return ret;
}

/*
 * Called to remove the sdio transport.
 * Powers down the entire HW, including AL.
 *
 *@func - SDIO bus function struct.
 */
static void iwl_sdio_remove(struct sdio_func *func)
{
	struct iwl_trans *trans = sdio_get_drvdata(func);
	struct iwl_trans_sdio *trans_sdio = IWL_TRANS_GET_SDIO_TRANS(trans);

	/* Stop driver and the FW request callback */
	iwl_drv_stop(trans_sdio->drv);

	/* Clear data in SDIO bus driver */
	sdio_set_drvdata(func, NULL);

	/* Release all BUS allocated memroy */
	iwl_trans_sdio_free(trans);

	__iwl_info(&func->dev, "SDIO remove completed successfully\n");
}

/*
 * iwl sdio driver
 * API to sdio bus driver stack.
 */

#ifdef CONFIG_PM_SLEEP
static const struct dev_pm_ops iwl_pm_ops = {
	.suspend	= iwl_sdio_suspend,
	.resume		= iwl_sdio_resume,
};
#endif

static struct sdio_driver iwl_sdio_driver = {
	.name = INTEL_SDIO_DRIVER_NAME,
	.probe = iwl_sdio_probe,
	.remove = iwl_sdio_remove,
	.id_table = iwl_sdio_device_ids,
	.drv = {
#ifdef CONFIG_PM_SLEEP
	.pm     = &iwl_pm_ops,
#endif
	},
};

#ifdef CONFIG_X86_MRFLD
static int iwlwifi_probe(struct platform_device *pdev)
{
	return 0;
}

static int iwlwifi_plat_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver iwlwifi_mrfld_driver = {
	.probe		= iwlwifi_probe,
	.remove		= iwlwifi_plat_remove,
	.driver = {
		.name	= "wlan",
		.owner	= THIS_MODULE,
	}
};
#endif

/*
 * Register the iwlwifi sdio driver with the SDIO bus driver.
 */
int iwl_sdio_register_driver(void)
{
	int ret;

#ifdef CONFIG_X86_MRFLD
	ret = platform_driver_register(&iwlwifi_mrfld_driver);
	if (ret) {
		pr_err("Failed to register iwlwifi SDIO Bus driver, %d\n", ret);
		return ret;
	}
	msleep(1000);
#endif

	ret = sdio_register_driver(&iwl_sdio_driver);
	if (ret)
		pr_err("Failed to register iwlwifi SDIO Bus driver, error %d\n",
		       ret);

	return ret;
}

/*
 * Unregister the iwlwifi sdio driver with the SDIO bus driver.
 */
void iwl_sdio_unregister_driver(void)
{
	sdio_unregister_driver(&iwl_sdio_driver);
}
