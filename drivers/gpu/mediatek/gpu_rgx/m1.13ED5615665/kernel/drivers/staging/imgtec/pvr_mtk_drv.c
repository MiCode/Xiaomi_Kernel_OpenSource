/* vi: set ts=8 sw=8 sts=8: */
/*************************************************************************/ /*!
@File
@Title          MT8173 DRM platform driver
@Codingstyle    LinuxKernel
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/

#include <drm/drmP.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/version.h>

#include "module_common.h"
#include "pvr_drv.h"
#include "pvrmodule.h"

/* This header must always be included last */
#include "kernel_compatibility.h"

static struct drm_driver pvr_drm_platform_driver;

static int pvr_probe(struct platform_device *pdev)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0))
	struct drm_device *ddev;
	int ret;

	DRM_DEBUG_DRIVER("device %p\n", &pdev->dev);

	ddev = drm_dev_alloc(&pvr_drm_platform_driver, &pdev->dev);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 0))
	if (IS_ERR(ddev))
		return PTR_ERR(ddev);
#else
	if (!ddev)
		return -ENOMEM;
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0))
	/* Needed by drm_platform_set_busid */
	ddev->platformdev = pdev;
#endif

	/*
	 * The load callback, called from drm_dev_register, is deprecated,
	 * because of potential race conditions. Calling the function here,
	 * before calling drm_dev_register, avoids those potential races.
	 */
	BUG_ON(pvr_drm_platform_driver.load != NULL);
	ret = pvr_drm_load(ddev, 0);
	if (ret)
		goto err_drm_dev_put;

	ret = drm_dev_register(ddev, 0);
	if (ret)
		goto err_drm_dev_unload;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 11, 0))
	DRM_INFO("Initialized %s %d.%d.%d %s on minor %d\n",
		pvr_drm_platform_driver.name,
		pvr_drm_platform_driver.major,
		pvr_drm_platform_driver.minor,
		pvr_drm_platform_driver.patchlevel,
		pvr_drm_platform_driver.date,
		ddev->primary->index);
#endif
	return 0;

err_drm_dev_unload:
	pvr_drm_unload(ddev);
err_drm_dev_put:
	drm_dev_put(ddev);
	return	ret;
#else
	DRM_DEBUG_DRIVER("device %p\n", &pdev->dev);

	return drm_platform_init(&pvr_drm_platform_driver, pdev);
#endif
}

static int pvr_remove(struct platform_device *pdev)
{
	struct drm_device *ddev = platform_get_drvdata(pdev);

	DRM_DEBUG_DRIVER("device %p\n", &pdev->dev);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0))
	drm_dev_unregister(ddev);

	/* The unload callback, called from drm_dev_unregister, is
	 * deprecated. Call the unload function directly.
	 */
	BUG_ON(pvr_drm_platform_driver.unload != NULL);
	pvr_drm_unload(ddev);

	drm_dev_put(ddev);
#else
	drm_put_dev(ddev);
#endif
	return 0;
}

static void pvr_shutdown(struct platform_device *pdev)
{
	struct drm_device *ddev = platform_get_drvdata(pdev);
	struct pvr_drm_private *priv = ddev->dev_private;

	DRM_DEBUG_DRIVER("device %p\n", &pdev->dev);

	PVRSRVCommonDeviceShutdown(priv->dev_node);
}

static const struct of_device_id mtk_powervr_of_match[] = {
	{ .compatible = "mediatek,mt8173-gpu", },
	{ .compatible = "mediatek,mt8173-han", },
	{ .compatible = "mediatek,HAN", },
	{},
};

MODULE_DEVICE_TABLE(of, mtk_powervr_of_match);

static struct platform_driver pvr_platform_driver = {
	.driver = {
		.name		= DRVNAME,
		.of_match_table = of_match_ptr(mtk_powervr_of_match),
		.pm		= &pvr_pm_ops,
	},
	.probe			= pvr_probe,
	.remove			= pvr_remove,
	.shutdown		= pvr_shutdown,
};

static int __init pvr_init(void)
{
	int err = 0;

	DRM_DEBUG_DRIVER("\n");

	pvr_drm_platform_driver = pvr_drm_generic_driver;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0)) && \
	(LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0))
	pvr_drm_platform_driver.set_busid = drm_platform_set_busid;
#endif

	err = PVRSRVCommonDriverInit();
	if (err)
		return err;

	return platform_driver_register(&pvr_platform_driver);
}

static void __exit pvr_exit(void)
{
	DRM_DEBUG_DRIVER("\n");

	platform_driver_unregister(&pvr_platform_driver);

	PVRSRVCommonDriverDeinit();

	DRM_DEBUG_DRIVER("done\n");
}

late_initcall(pvr_init);
module_exit(pvr_exit);
