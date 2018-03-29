/*
* Copyright (c) 2014 MediaTek Inc.
* Author: Chiawen Lee <chiawen.lee@mediatek.com>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*/
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/regulator/consumer.h>
#include <linux/reboot.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/pm_runtime.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/bug.h>
#include <linux/mtk_gpu_utility.h>

#include "mt_gpufreq.h"
#include <trace/events/mtk_events.h>
#include <dt-bindings/clock/mt8173-clk.h>

#include "mtk_mfgsys.h"
#include "rgxdevice.h"
#include "osfunc.h"
#include "pvrsrv.h"
#include "rgxhwperf.h"


#define MTK_DEFER_DVFS_WORK_MS          10000
#define MTK_DVFS_SWITCH_INTERVAL_MS     50
#define MTK_SYS_BOOST_DURATION_MS       50
#define MTK_RGX_DEVICE_INDEX_INVALID    -1

static IMG_HANDLE g_hDVFSTimer;
static POS_LOCK ghDVFSLock;

static IMG_BOOL g_bExit = IMG_TRUE;
static IMG_INT32 g_iSkipCount;
static IMG_UINT32 g_sys_dvfs_time_ms;

static IMG_UINT32 g_bottom_freq_id;
static IMG_UINT32 gpu_bottom_freq;
static IMG_UINT32 g_cust_boost_freq_id;
static IMG_UINT32 gpu_cust_boost_freq;
static IMG_UINT32 g_cust_upbound_freq_id;
static IMG_UINT32 gpu_cust_upbound_freq;

static IMG_UINT32 gpu_power;
static IMG_UINT32 gpu_current;
static IMG_UINT32 gpu_dvfs_enable;
static IMG_UINT32 boost_gpu_enable;
static IMG_UINT32 gpu_debug_enable;
static IMG_UINT32 gpu_dvfs_force_idle;
static IMG_UINT32 gpu_dvfs_cb_force_idle;

static IMG_UINT32 gpu_pre_loading;
static IMG_UINT32 gpu_loading;
static IMG_UINT32 gpu_block;
static IMG_UINT32 gpu_idle;
static IMG_UINT32 gpu_freq;


static struct platform_device *sPVRLDMDev;
static struct platform_device *sMFGASYNCDev;
static struct platform_device *sMFG2DDev;
#define GET_MTK_MFG_BASE(x) (struct mtk_mfg_base *)(x->dev.platform_data)

static char *top_mfg_clk_name[] = {
	"mfg_mem_in_sel",
	"mfg_axi_in_sel",
	"top_axi",
	"top_mem",
	"top_mfg",
};
#define MAX_TOP_MFG_CLK ARRAY_SIZE(top_mfg_clk_name)

#define REG_MFG_AXI BIT(0)
#define REG_MFG_MEM BIT(1)
#define REG_MFG_G3D BIT(2)
#define REG_MFG_26M BIT(3)
#define REG_MFG_ALL (REG_MFG_AXI | REG_MFG_MEM | REG_MFG_G3D | REG_MFG_26M)

#define REG_MFG_CG_STA 0x00
#define REG_MFG_CG_SET 0x04
#define REG_MFG_CG_CLR 0x08


static void mtk_mfg_set_clock_gating(void __iomem *reg)
{
	writel(REG_MFG_ALL, reg + REG_MFG_CG_SET);
}

static void mtk_mfg_clr_clock_gating(void __iomem *reg)
{
	writel(REG_MFG_ALL, reg + REG_MFG_CG_CLR);
}

#if defined(MTK_ENABLE_HWAPM)
static void mtk_mfg_enable_hw_apm(void)
{
	struct mtk_mfg_base *mfg_base = GET_MTK_MFG_BASE(sPVRLDMDev);

	writel(0x004a3d4d, mfg_base->reg_base + 0x24);
	writel(0x4d45520b, mfg_base->reg_base + 0x28);
	writel(0x7a710184, mfg_base->reg_base + 0xe0);
	writel(0x835f6856, mfg_base->reg_base + 0xe4);
	writel(0x00470248, mfg_base->reg_base + 0xe8);
	writel(0x80000000, mfg_base->reg_base + 0xec);
	writel(0x08000000, mfg_base->reg_base + 0xa0);
}

static void mtk_mfg_disable_hw_apm(void)
{
#if 0
	struct mtk_mfg_base *mfg_base = GET_MTK_MFG_BASE(sPVRLDMDev);

	writel(0x00, mfg_base->reg_base + 0x24);
	writel(0x00, mfg_base->reg_base + 0x28);
	writel(0x00, mfg_base->reg_base + 0xe0);
	writel(0x00, mfg_base->reg_base + 0xe4);
	writel(0x00, mfg_base->reg_base + 0xe8);
	writel(0x00, mfg_base->reg_base + 0xec);
	writel(0x00, mfg_base->reg_base + 0xa0);
#endif
}
#else
static void mtk_mfg_enable_hw_apm(void) {};
static void mtk_mfg_disable_hw_apm(void) {};
#endif /* MTK_ENABLE_HWAPM */

static void mtk_mfg_enable_clock(void)
{
	int i;
	struct mtk_mfg_base *mfg_base = GET_MTK_MFG_BASE(sPVRLDMDev);

	/* Power on vgpu */
	mt_gpufreq_voltage_enable_set(1);

	/* Resume mfg power domain */
	pm_runtime_get_sync(&mfg_base->mfg_async_pdev->dev);
	pm_runtime_get_sync(&mfg_base->mfg_2d_pdev->dev);
	pm_runtime_get_sync(&mfg_base->pdev->dev);

	/* Prepare and enable mfg top clock */
	for (i = 0; i < MAX_TOP_MFG_CLK; i++) {
		clk_prepare(mfg_base->top_clk[i]);
		clk_enable(mfg_base->top_clk[i]);
	}

	/* Enable(un-gated) mfg clock */
	mtk_mfg_clr_clock_gating(mfg_base->reg_base);
}

static void mtk_mfg_disable_clock(void)
{
	int i;
	struct mtk_mfg_base *mfg_base = GET_MTK_MFG_BASE(sPVRLDMDev);

	/* Disable(gated) mfg clock */
	mtk_mfg_set_clock_gating(mfg_base->reg_base);

	/* Disable and unprepare mfg top clock */
	for (i = MAX_TOP_MFG_CLK - 1; i >= 0; i--) {
		clk_disable(mfg_base->top_clk[i]);
		clk_unprepare(mfg_base->top_clk[i]);
	}

	/* Suspend mfg power domain */
	pm_runtime_put_sync(&mfg_base->pdev->dev);
	pm_runtime_put_sync(&mfg_base->mfg_2d_pdev->dev);
	pm_runtime_put_sync(&mfg_base->mfg_async_pdev->dev);

	/* Power off vgpu */
	mt_gpufreq_voltage_enable_set(0);
}

static int mfg_notify_handler(struct notifier_block *this, unsigned long code,
			      void *unused)
{
	struct mtk_mfg_base *mfg_base = container_of(this,
						     typeof(*mfg_base),
						     mfg_notifier);
	if ((code != SYS_RESTART) && (code != SYS_POWER_OFF))
		return 0;

	dev_err(&mfg_base->pdev->dev, "shutdown notified, code=%x\n", (unsigned int)code);

	mutex_lock(&mfg_base->set_power_state);

	mtk_mfg_enable_clock();
	mtk_mfg_enable_hw_apm();

	mfg_base->shutdown = true;

	mutex_unlock(&mfg_base->set_power_state);

	return 0;
}


static int mtk_mfg_bind_device_resource(struct platform_device *pdev,
				 struct mtk_mfg_base *mfg_base)
{
	int i, err;
	int len = sizeof(struct clk *) * MAX_TOP_MFG_CLK;

	mfg_base->top_clk = devm_kzalloc(&pdev->dev, len, GFP_KERNEL);
	if (!mfg_base->top_clk)
		return -ENOMEM;

	mfg_base->reg_base = of_iomap(pdev->dev.of_node, 1);
	if (!mfg_base->reg_base) {
		pr_err("Unable to ioremap registers pdev %p\n", pdev);
		return -ENOMEM;
	}

	for (i = 0; i < MAX_TOP_MFG_CLK; i++) {
		mfg_base->top_clk[i] = devm_clk_get(&pdev->dev,
						    top_mfg_clk_name[i]);
		if (IS_ERR(mfg_base->top_clk[i])) {
			err = PTR_ERR(mfg_base->top_clk[i]);
			dev_err(&pdev->dev, "devm_clk_get %s failed !!!\n",
				top_mfg_clk_name[i]);
			goto err_iounmap_reg_base;
		}
	}
	if (!sMFGASYNCDev || !sMFG2DDev) {
		dev_err(&pdev->dev, "Failed to get pm_domain\n");
		err = -EPROBE_DEFER;
		goto err_iounmap_reg_base;
	}
	mfg_base->mfg_2d_pdev = sMFG2DDev;
	mfg_base->mfg_async_pdev = sMFGASYNCDev;

	mfg_base->mfg_notifier.notifier_call = mfg_notify_handler;
	register_reboot_notifier(&mfg_base->mfg_notifier);

	pm_runtime_enable(&pdev->dev);

	mfg_base->pdev = pdev;
	return 0;

err_iounmap_reg_base:
	iounmap(mfg_base->reg_base);
	return err;
}

static int mtk_mfg_unbind_device_resource(struct platform_device *pdev,
				   struct mtk_mfg_base *mfg_base)
{
	mfg_base->pdev = NULL;

	if (mfg_base->mfg_notifier.notifier_call) {
		unregister_reboot_notifier(&mfg_base->mfg_notifier);
		mfg_base->mfg_notifier.notifier_call = NULL;
	}
	iounmap(mfg_base->reg_base);
	pm_runtime_disable(&pdev->dev);

	return 0;
}


PVRSRV_ERROR MTKDevPrePowerState(PVRSRV_DEV_POWER_STATE eNewPowerState,
				    PVRSRV_DEV_POWER_STATE eCurrentPowerState,
				    IMG_BOOL bForced)
{
	struct mtk_mfg_base *mfg_base = GET_MTK_MFG_BASE(sPVRLDMDev);

	mutex_lock(&mfg_base->set_power_state);

	if ((PVRSRV_DEV_POWER_STATE_OFF == eNewPowerState) &&
	    (PVRSRV_DEV_POWER_STATE_ON == eCurrentPowerState)) {

		if (g_hDVFSTimer && PVRSRVGetInitServerState(PVRSRV_INIT_SERVER_SUCCESSFUL))
			g_bExit = IMG_TRUE;

		mtk_mfg_disable_hw_apm();
		mtk_mfg_disable_clock();
	}

	mutex_unlock(&mfg_base->set_power_state);
	return PVRSRV_OK;
}

PVRSRV_ERROR MTKDevPostPowerState(PVRSRV_DEV_POWER_STATE eNewPowerState,
				     PVRSRV_DEV_POWER_STATE eCurrentPowerState,
				     IMG_BOOL bForced)
{
	struct mtk_mfg_base *mfg_base = GET_MTK_MFG_BASE(sPVRLDMDev);

	mutex_lock(&mfg_base->set_power_state);

	if ((PVRSRV_DEV_POWER_STATE_ON == eNewPowerState) &&
	    (PVRSRV_DEV_POWER_STATE_OFF == eCurrentPowerState)) {

		mtk_mfg_enable_clock();
		mtk_mfg_enable_hw_apm();

		if (g_hDVFSTimer && PVRSRVGetInitServerState(PVRSRV_INIT_SERVER_SUCCESSFUL))
			g_bExit = IMG_FALSE;
	}

	mutex_unlock(&mfg_base->set_power_state);
	return PVRSRV_OK;
}

static void MTKEnableClock(void)
{
	struct mtk_mfg_base *mfg_base = GET_MTK_MFG_BASE(sPVRLDMDev);

	mutex_lock(&mfg_base->set_power_state);
	if (mfg_base->shutdown) {
		mutex_unlock(&mfg_base->set_power_state);
		pr_info("skip MTKEnableClock \n");
		return;
	}
	mtk_mfg_enable_clock();
	mutex_unlock(&mfg_base->set_power_state);
}

static void MTKDisableClock(void)
{
	struct mtk_mfg_base *mfg_base = GET_MTK_MFG_BASE(sPVRLDMDev);

	mutex_lock(&mfg_base->set_power_state);
	if (mfg_base->shutdown) {
		mutex_unlock(&mfg_base->set_power_state);
		pr_info("skip MTKDisableClock \n");
		return;
	}
	mtk_mfg_disable_clock();

	mutex_unlock(&mfg_base->set_power_state);
}

PVRSRV_ERROR MTKSystemPrePowerState(PVRSRV_SYS_POWER_STATE eNewPowerState)
{
	return PVRSRV_OK;
}

PVRSRV_ERROR MTKSystemPostPowerState(PVRSRV_SYS_POWER_STATE eNewPowerState)
{
	return PVRSRV_OK;
}

int MTKMFGBaseInit(struct platform_device *pdev)
{
	int err;
	struct mtk_mfg_base *mfg_base;

	mfg_base = devm_kzalloc(&pdev->dev, sizeof(*mfg_base), GFP_KERNEL);
	if (!mfg_base)
		return -ENOMEM;

	err = mtk_mfg_bind_device_resource(pdev, mfg_base);
	if (err != 0)
		return err;

	mutex_init(&mfg_base->set_power_state);

	/* attach mfg_base to pdev->dev.platform_data */
	pdev->dev.platform_data = mfg_base;
	sPVRLDMDev = pdev;

	return 0;
}

int MTKMFGBaseDeInit(struct platform_device *pdev)
{
	struct mtk_mfg_base *mfg_base = GET_MTK_MFG_BASE(sPVRLDMDev);

	if (pdev != sPVRLDMDev) {
		dev_err(&pdev->dev, "release %p != %p\n", pdev, sPVRLDMDev);
		return 0;
	}

	mtk_mfg_unbind_device_resource(pdev, mfg_base);

	return 0;
}

static PVRSRV_DEVICE_NODE *MTKGetRGXDevNode(void)
{
	PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();
	IMG_UINT32 i;

	for (i = 0; i < psPVRSRVData->ui32RegisteredDevices; i++) {
		PVRSRV_DEVICE_NODE *psDeviceNode = psPVRSRVData->apsRegisteredDevNodes[i];

		if (psDeviceNode && psDeviceNode->psDevConfig &&
			psDeviceNode->psDevConfig->eDeviceType == PVRSRV_DEVICE_TYPE_RGX)
			return psDeviceNode;
	}

	return NULL;
}

static IMG_UINT32 MTKGetRGXDevIdx(void)
{
	static IMG_UINT32 ms_ui32RGXDevIdx = MTK_RGX_DEVICE_INDEX_INVALID;

	if (MTK_RGX_DEVICE_INDEX_INVALID == ms_ui32RGXDevIdx) {
		PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();
		IMG_UINT32 i;

		for (i = 0; i < psPVRSRVData->ui32RegisteredDevices; i++) {
			PVRSRV_DEVICE_NODE *psDeviceNode = psPVRSRVData->apsRegisteredDevNodes[i];

			if (psDeviceNode && psDeviceNode->psDevConfig &&
				psDeviceNode->psDevConfig->eDeviceType == PVRSRV_DEVICE_TYPE_RGX) {
				ms_ui32RGXDevIdx = i;
				break;
			}
		}
	}

	return ms_ui32RGXDevIdx;
}

static void MTKWriteBackFreqToRGX(IMG_UINT32 ui32DeviceIndex, IMG_UINT32 ui32NewFreq)
{
	PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();
	PVRSRV_DEVICE_NODE *psDeviceNode = psPVRSRVData->apsRegisteredDevNodes[ui32DeviceIndex];
	RGX_DATA *psRGXData = (RGX_DATA *)psDeviceNode->psDevConfig->hDevData;

    /* kHz to Hz write to RGX as the same unit */
	psRGXData->psRGXTimingInfo->ui32CoreClockSpeed = ui32NewFreq * 1000;
}

static IMG_BOOL MTKDoGpuDVFS(IMG_UINT32 ui32NewFreqID, IMG_BOOL bIdleDevice)
{
	PVRSRV_ERROR eResult;
	IMG_UINT32 ui32RGXDevIdx;

	if (mt_gpufreq_dvfs_ready() == false)
		return IMG_FALSE;

	if (1) {
		struct mtk_mfg_base *mfg_base = GET_MTK_MFG_BASE(sPVRLDMDev);
		mutex_lock(&mfg_base->set_power_state);
		if (mfg_base->shutdown) {
			mutex_unlock(&mfg_base->set_power_state);
			pr_info("skip MTKDoGpuDVFS \n");
			return IMG_FALSE;
		}
		mutex_unlock(&mfg_base->set_power_state);
	}

	/* bottom bound */
	if (ui32NewFreqID > g_bottom_freq_id)
		ui32NewFreqID = g_bottom_freq_id;

	if (ui32NewFreqID > g_cust_boost_freq_id)
		ui32NewFreqID = g_cust_boost_freq_id;

	/* up bound */
	if (ui32NewFreqID < g_cust_upbound_freq_id)
		ui32NewFreqID = g_cust_upbound_freq_id;

	/* thermal power limit */
	if (ui32NewFreqID < mt_gpufreq_get_thermal_limit_index())
		ui32NewFreqID = mt_gpufreq_get_thermal_limit_index();

	/* no change */
	if (ui32NewFreqID == mt_gpufreq_get_cur_freq_index())
		return IMG_FALSE;

	ui32RGXDevIdx = MTKGetRGXDevIdx();
	if (MTK_RGX_DEVICE_INDEX_INVALID == ui32RGXDevIdx)
		return IMG_FALSE;

	eResult = PVRSRVDevicePreClockSpeedChange(ui32RGXDevIdx, bIdleDevice, (void *)NULL);
	if ((PVRSRV_OK == eResult) || (PVRSRV_ERROR_RETRY == eResult)) {
		unsigned int ui32GPUFreq;
		unsigned int ui32CurFreqID;
		PVRSRV_DEV_POWER_STATE ePowerState;

		PVRSRVGetDevicePowerState(ui32RGXDevIdx, &ePowerState);
		if (ePowerState != PVRSRV_DEV_POWER_STATE_ON)
			MTKEnableClock();

		mt_gpufreq_target(ui32NewFreqID);

		ui32CurFreqID = mt_gpufreq_get_cur_freq_index();
		ui32GPUFreq = mt_gpufreq_get_frequency_by_level(ui32CurFreqID);
		gpu_freq = ui32GPUFreq;

		MTKWriteBackFreqToRGX(ui32RGXDevIdx, ui32GPUFreq);

		if (ePowerState != PVRSRV_DEV_POWER_STATE_ON)
			MTKDisableClock();

		if (PVRSRV_OK == eResult)
			PVRSRVDevicePostClockSpeedChange(ui32RGXDevIdx, bIdleDevice, (void *)NULL);

		return IMG_TRUE;
	}

	return IMG_FALSE;
}

static void MTKFreqInputBoostCB(unsigned int ui32BoostFreqID)
{
	if (g_iSkipCount > 0)
		return;

	if (boost_gpu_enable == 0)
		return;

	OSLockAcquire(ghDVFSLock);

	if (ui32BoostFreqID < mt_gpufreq_get_cur_freq_index()) {
		if (MTKDoGpuDVFS(ui32BoostFreqID, gpu_dvfs_cb_force_idle == 0 ? IMG_FALSE : IMG_TRUE))
			g_sys_dvfs_time_ms = OSClockms();
	}

	OSLockRelease(ghDVFSLock);
}

static void MTKFreqPowerLimitCB(unsigned int ui32LimitFreqID)
{
	if (g_iSkipCount > 0)
		return;

	OSLockAcquire(ghDVFSLock);

	if (ui32LimitFreqID > mt_gpufreq_get_cur_freq_index()) {
		if (MTKDoGpuDVFS(ui32LimitFreqID, gpu_dvfs_cb_force_idle == 0 ? IMG_FALSE : IMG_TRUE))
			g_sys_dvfs_time_ms = OSClockms();
	}

	OSLockRelease(ghDVFSLock);
}


/* Handle used by DebugFS to get GPU utilisation stats */
static IMG_HANDLE ghGpuDVFSDebugFS;
static void MTKCalGpuLoading(unsigned int *pui32Loading, unsigned int *pui32Block, unsigned int *pui32Idle)
{
	PVRSRV_DEVICE_NODE *psDeviceNode;
	PVRSRV_RGXDEV_INFO *psDevInfo;

	*pui32Loading = 0;
	*pui32Block = 0;
	*pui32Idle = 0;

	psDeviceNode = MTKGetRGXDevNode();
	if (!psDeviceNode)
		return;

	psDevInfo = psDeviceNode->pvDevice;

	if (psDevInfo->pfnRegisterGpuUtilStats && psDevInfo->pfnGetGpuUtilStats &&
		psDeviceNode->eHealthStatus == PVRSRV_DEVICE_HEALTH_STATUS_OK) {
		RGXFWIF_GPU_UTIL_STATS sGpuUtilStats;
		PVRSRV_ERROR eError = PVRSRV_OK;

		if (ghGpuDVFSDebugFS == NULL)
			eError = psDevInfo->pfnRegisterGpuUtilStats(&ghGpuDVFSDebugFS);

		if (eError == PVRSRV_OK)
			eError = psDevInfo->pfnGetGpuUtilStats(psDeviceNode, ghGpuDVFSDebugFS, &sGpuUtilStats);

		if ((eError == PVRSRV_OK) &&
			(sGpuUtilStats.bValid) &&
			((IMG_UINT32)sGpuUtilStats.ui64GpuStatCumulative)) {
			IMG_UINT64 util;
			IMG_UINT32 rem;

			util = 100 * sGpuUtilStats.ui64GpuStatActiveHigh;
			*pui32Loading = OSDivide64(util, (IMG_UINT32)sGpuUtilStats.ui64GpuStatCumulative, &rem);

			util = 100 * sGpuUtilStats.ui64GpuStatBlocked;
			*pui32Block = OSDivide64(util, (IMG_UINT32)sGpuUtilStats.ui64GpuStatCumulative, &rem);

			util = 100 * sGpuUtilStats.ui64GpuStatIdle;
			*pui32Idle = OSDivide64(util, (IMG_UINT32)sGpuUtilStats.ui64GpuStatCumulative, &rem);
		}
	}
}

static IMG_BOOL MTKGpuDVFSPolicy(IMG_UINT32 ui32GPULoading, unsigned int *pui32NewFreqID)
{
	int i32MaxLevel = (int)(mt_gpufreq_get_dvfs_table_num() - 1);
	int i32CurFreqID = (int)mt_gpufreq_get_cur_freq_index();
	int i32NewFreqID = i32CurFreqID;

	if (ui32GPULoading >= 99)
		i32NewFreqID = 0;
	else if (ui32GPULoading <= 1)
		i32NewFreqID = i32MaxLevel;
	else if (ui32GPULoading >= 85)
		i32NewFreqID -= 2;
	else if (ui32GPULoading <= 30)
		i32NewFreqID += 2;
	else if (ui32GPULoading >= 70)
		i32NewFreqID -= 1;
	else if (ui32GPULoading <= 50)
		i32NewFreqID += 1;

	if (i32NewFreqID < i32CurFreqID) {
		if (gpu_pre_loading * 17 / 10 < ui32GPULoading)
			i32NewFreqID -= 1;
	} else if (i32NewFreqID > i32CurFreqID) {
		if (ui32GPULoading * 17 / 10 < gpu_pre_loading)
			i32NewFreqID += 1;
	}

	if (i32NewFreqID > i32MaxLevel)
		i32NewFreqID = i32MaxLevel;
	else if (i32NewFreqID < 0)
		i32NewFreqID = 0;

	if (i32NewFreqID != i32CurFreqID) {
		*pui32NewFreqID = (unsigned int)i32NewFreqID;
		return IMG_TRUE;
	}

	return IMG_FALSE;
}

static void MTKDVFSTimerFuncCB(void *pvData)
{
	IMG_UINT32 x1, x2, x3, y1, y2, y3;

	int i32MaxLevel = (int)(mt_gpufreq_get_dvfs_table_num() - 1);
	int i32CurFreqID = (int)mt_gpufreq_get_cur_freq_index();

	x1 = 10000;
	x2 = 30000;
	x3 = 50000;
	y1 = 50;
	y2 = 430;
	y3 = 750;

	if (gpu_debug_enable)
		PVR_DPF((PVR_DBG_ERROR, "MTKDVFSTimerFuncCB"));

	if (gpu_dvfs_enable == 0) {
		gpu_power = 0;
		gpu_current = 0;
		gpu_loading = 0;
		gpu_block = 0;
		gpu_idle = 0;

		return;
	}

	if (g_iSkipCount > 0) {
		gpu_power = 0;
		gpu_current = 0;
		gpu_loading = 0;
		gpu_block = 0;
		gpu_idle = 0;
		g_iSkipCount -= 1;

		return;
	}

	if ((!g_bExit) || (i32CurFreqID < i32MaxLevel)) {
		IMG_UINT32 ui32NewFreqID;

		gpu_power = 0;
		gpu_current = 0;

		MTKCalGpuLoading(&gpu_loading, &gpu_block, &gpu_idle);

		OSLockAcquire(ghDVFSLock);

		/* check system boost duration */
		if ((g_sys_dvfs_time_ms > 0) && (OSClockms() - g_sys_dvfs_time_ms < MTK_SYS_BOOST_DURATION_MS)) {
			OSLockRelease(ghDVFSLock);
			return;
		}

		g_sys_dvfs_time_ms = 0;

		/* do gpu dvfs */
		if (MTKGpuDVFSPolicy(gpu_loading, &ui32NewFreqID))
			MTKDoGpuDVFS(ui32NewFreqID, gpu_dvfs_force_idle == 0 ? IMG_FALSE : IMG_TRUE);

		gpu_pre_loading = gpu_loading;

		OSLockRelease(ghDVFSLock);
	}
}

void MTKMFGEnableDVFSTimer(bool bEnable)
{
	/* OSEnableTimer() and OSDisableTimer() should be called sequentially, following call will lead to assertion.
	   OSEnableTimer();
	   OSEnableTimer();
	   OSDisableTimer();
	   ...
	   bPreEnable is to avoid such scenario */
	static bool bPreEnable;

	if (gpu_debug_enable)
		PVR_DPF((PVR_DBG_ERROR, "MTKMFGEnableDVFSTimer: %s (%s)",
			bEnable ? "yes" : "no", bPreEnable ? "yes" : "no"));

	if (g_hDVFSTimer) {
		if (bEnable == true && bPreEnable == false) {
			OSEnableTimer(g_hDVFSTimer);
			bPreEnable = true;
		} else if (bEnable == false && bPreEnable == true) {
			OSDisableTimer(g_hDVFSTimer);
			bPreEnable = false;
		}
	}
}

static void MTKBoostGpuFreq(void)
{
	if (gpu_debug_enable)
		PVR_DPF((PVR_DBG_ERROR, "MTKBoostGpuFreq"));

	MTKFreqInputBoostCB(0);
}

static void MTKSetBottomGPUFreq(unsigned int ui32FreqLevel)
{
	unsigned int ui32MaxLevel;

	if (gpu_debug_enable)
		PVR_DPF((PVR_DBG_ERROR, "MTKSetBottomGPUFreq: freq = %d", ui32FreqLevel));

	ui32MaxLevel = mt_gpufreq_get_dvfs_table_num() - 1;
	if (ui32MaxLevel < ui32FreqLevel)
		ui32FreqLevel = ui32MaxLevel;

	OSLockAcquire(ghDVFSLock);

	/* 0 => The highest frequency
	   table_num - 1 => The lowest frequency */
	g_bottom_freq_id = ui32MaxLevel - ui32FreqLevel;
	gpu_bottom_freq = mt_gpufreq_get_frequency_by_level(g_bottom_freq_id);

	if (g_bottom_freq_id < mt_gpufreq_get_cur_freq_index())
		MTKDoGpuDVFS(g_bottom_freq_id, gpu_dvfs_cb_force_idle == 0 ? IMG_FALSE : IMG_TRUE);

	OSLockRelease(ghDVFSLock);
}

static unsigned int MTKCustomGetGpuFreqLevelCount(void)
{
	return mt_gpufreq_get_dvfs_table_num();
}

static void MTKCustomBoostGpuFreq(unsigned int ui32FreqLevel)
{
	unsigned int ui32MaxLevel;

	if (gpu_debug_enable)
		PVR_DPF((PVR_DBG_ERROR, "MTKCustomBoostGpuFreq: freq = %d", ui32FreqLevel));

	ui32MaxLevel = mt_gpufreq_get_dvfs_table_num() - 1;
	if (ui32MaxLevel < ui32FreqLevel)
		ui32FreqLevel = ui32MaxLevel;

	OSLockAcquire(ghDVFSLock);

	/* 0 => The highest frequency
	   table_num - 1 => The lowest frequency */
	g_cust_boost_freq_id = ui32MaxLevel - ui32FreqLevel;
	gpu_cust_boost_freq = mt_gpufreq_get_frequency_by_level(g_cust_boost_freq_id);

	if (g_cust_boost_freq_id < mt_gpufreq_get_cur_freq_index())
		MTKDoGpuDVFS(g_cust_boost_freq_id, gpu_dvfs_cb_force_idle == 0 ? IMG_FALSE : IMG_TRUE);

	OSLockRelease(ghDVFSLock);
}

static void MTKCustomUpBoundGpuFreq(unsigned int ui32FreqLevel)
{
	unsigned int ui32MaxLevel;

	if (gpu_debug_enable)
		PVR_DPF((PVR_DBG_ERROR, "MTKCustomUpBoundGpuFreq: freq = %d", ui32FreqLevel));

	ui32MaxLevel = mt_gpufreq_get_dvfs_table_num() - 1;
	if (ui32MaxLevel < ui32FreqLevel)
		ui32FreqLevel = ui32MaxLevel;

	OSLockAcquire(ghDVFSLock);

	/* 0 => The highest frequency
	   table_num - 1 => The lowest frequency */
	g_cust_upbound_freq_id = ui32MaxLevel - ui32FreqLevel;
	gpu_cust_upbound_freq = mt_gpufreq_get_frequency_by_level(g_cust_upbound_freq_id);

	if (g_cust_upbound_freq_id > mt_gpufreq_get_cur_freq_index())
		MTKDoGpuDVFS(g_cust_upbound_freq_id, gpu_dvfs_cb_force_idle == 0 ? IMG_FALSE : IMG_TRUE);

	OSLockRelease(ghDVFSLock);
}

static unsigned int MTKGetCustomBoostGpuFreq(void)
{
	unsigned int ui32MaxLevel = mt_gpufreq_get_dvfs_table_num() - 1;

	return (ui32MaxLevel - g_cust_boost_freq_id);
}

static unsigned int MTKGetCustomUpBoundGpuFreq(void)
{
	unsigned int ui32MaxLevel = mt_gpufreq_get_dvfs_table_num() - 1;

	return (ui32MaxLevel - g_cust_upbound_freq_id);
}

static IMG_UINT32 MTKGetGpuLoading(void)
{
	return gpu_loading;
}

static IMG_UINT32 MTKGetGpuBlock(void)
{
	return gpu_block;
}

static IMG_UINT32 MTKGetGpuIdle(void)
{
	return gpu_idle;
}

static IMG_UINT32 MTKGetGpuFreq(void)
{
	return gpu_freq;
}

static IMG_UINT32 MTKGetPowerIndex(void)
{
	return gpu_current;
}

int MTKMFGSystemInit(void)
{
	PVRSRV_ERROR error;

	error = OSLockCreate(&ghDVFSLock, LOCK_TYPE_PASSIVE);
	if (error != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR, "Create DVFS Lock Failed"));
		return error;
	}

	g_iSkipCount = MTK_DEFER_DVFS_WORK_MS / MTK_DVFS_SWITCH_INTERVAL_MS;

	g_hDVFSTimer = OSAddTimer(MTKDVFSTimerFuncCB, (IMG_VOID *)NULL, MTK_DVFS_SWITCH_INTERVAL_MS);
	if (!g_hDVFSTimer) {
		OSLockDestroy(ghDVFSLock);
		PVR_DPF((PVR_DBG_ERROR, "Create DVFS Timer Failed"));
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	MTKMFGEnableDVFSTimer(true);

#ifdef MTK_GPU_DVFS
	gpu_dvfs_enable = 1;
#else
	gpu_dvfs_enable = 0;
#endif

	boost_gpu_enable = 1;

	g_sys_dvfs_time_ms = 0;

	g_bottom_freq_id = mt_gpufreq_get_dvfs_table_num() - 1;
	gpu_bottom_freq = mt_gpufreq_get_frequency_by_level(g_bottom_freq_id);

	g_cust_boost_freq_id = mt_gpufreq_get_dvfs_table_num() - 1;
	gpu_cust_boost_freq = mt_gpufreq_get_frequency_by_level(g_cust_boost_freq_id);

	g_cust_upbound_freq_id = 0;
	gpu_cust_upbound_freq = mt_gpufreq_get_frequency_by_level(g_cust_upbound_freq_id);

	gpu_debug_enable = 0;

	mt_gpufreq_mfgclock_notify_registerCB(MTKEnableClock, MTKDisableClock);

	mt_gpufreq_input_boost_notify_registerCB(MTKFreqInputBoostCB);
	mt_gpufreq_power_limit_notify_registerCB(MTKFreqPowerLimitCB);

	mtk_boost_gpu_freq_fp = MTKBoostGpuFreq;

	mtk_set_bottom_gpu_freq_fp = MTKSetBottomGPUFreq;

	mtk_custom_get_gpu_freq_level_count_fp = MTKCustomGetGpuFreqLevelCount;

	mtk_custom_boost_gpu_freq_fp = MTKCustomBoostGpuFreq;

	mtk_custom_upbound_gpu_freq_fp = MTKCustomUpBoundGpuFreq;

	mtk_get_custom_boost_gpu_freq_fp = MTKGetCustomBoostGpuFreq;

	mtk_get_custom_upbound_gpu_freq_fp = MTKGetCustomUpBoundGpuFreq;

	mtk_get_gpu_power_loading_fp = MTKGetPowerIndex;

	mtk_get_gpu_loading_fp = MTKGetGpuLoading;
	mtk_get_gpu_block_fp = MTKGetGpuBlock;
	mtk_get_gpu_idle_fp = MTKGetGpuIdle;
	mtk_get_gpu_freq_fp = MTKGetGpuFreq;

	return PVRSRV_OK;
}

int MTKMFGSystemDeInit(void)
{
	g_bExit = IMG_TRUE;

	if (g_hDVFSTimer) {
		MTKMFGEnableDVFSTimer(false);
		OSRemoveTimer(g_hDVFSTimer);
		g_hDVFSTimer = IMG_NULL;
	}

	if (ghDVFSLock)	{
		OSLockDestroy(ghDVFSLock);
		ghDVFSLock = NULL;
	}

	return PVRSRV_OK;
}

module_param(gpu_loading, uint, 0644);
module_param(gpu_block, uint, 0644);
module_param(gpu_idle, uint, 0644);
module_param(gpu_power, uint, 0644);
module_param(gpu_dvfs_enable, uint, 0644);
module_param(boost_gpu_enable, uint, 0644);
module_param(gpu_debug_enable, uint, 0644);
module_param(gpu_dvfs_force_idle, uint, 0644);
module_param(gpu_dvfs_cb_force_idle, uint, 0644);
module_param(gpu_bottom_freq, uint, 0644);
module_param(gpu_cust_boost_freq, uint, 0644);
module_param(gpu_cust_upbound_freq, uint, 0644);
module_param(gpu_freq, uint, 0644);

static int mtk_mfg_async_probe(struct platform_device *pdev)
{
	pr_info("mtk_mfg_async_probe\n");

	if (!pdev->dev.pm_domain) {
		dev_err(&pdev->dev, "Failed to get dev->pm_domain\n");
		return -EPROBE_DEFER;
	}

	sMFGASYNCDev = pdev;
	pm_runtime_enable(&pdev->dev);
	return 0;
}

static int mtk_mfg_async_remove(struct platform_device *pdev)
{
	pm_runtime_disable(&pdev->dev);
	return 0;
}

static const struct of_device_id mtk_mfg_async_of_ids[] = {
	{ .compatible = "mediatek,mt8173-mfg-async",},
	{}
};

static struct platform_driver mtk_mfg_async_driver = {
	.probe  = mtk_mfg_async_probe,
	.remove = mtk_mfg_async_remove,
	.driver = {
		.name = "mfg-async",
		.of_match_table = mtk_mfg_async_of_ids,
	}
};

static int __init mtk_mfg_async_init(void)
{
	int ret;

	ret = platform_driver_register(&mtk_mfg_async_driver);
	if (ret != 0) {
		pr_err("Failed to register mfg async driver\n");
		return ret;
	}

	return ret;
}
subsys_initcall(mtk_mfg_async_init);

static int mtk_mfg_2d_probe(struct platform_device *pdev)
{
	pr_info("mtk_mfg_2d_probe\n");

	if (!pdev->dev.pm_domain) {
		dev_err(&pdev->dev, "Failed to get dev->pm_domain\n");
		return -EPROBE_DEFER;
	}

	sMFG2DDev = pdev;
	pm_runtime_enable(&pdev->dev);
	return 0;
}

static int mtk_mfg_2d_remove(struct platform_device *pdev)
{
	pm_runtime_disable(&pdev->dev);
	return 0;
}

static const struct of_device_id mtk_mfg_2d_of_ids[] = {
	{ .compatible = "mediatek,mt8173-mfg-2d",},
	{}
};

static struct platform_driver mtk_mfg_2d_driver = {
	.probe  = mtk_mfg_2d_probe,
	.remove = mtk_mfg_2d_remove,
	.driver = {
		.name = "mfg-2d",
		.of_match_table = mtk_mfg_2d_of_ids,
	}
};

static int __init mtk_mfg_2d_init(void)
{
	int ret;

	ret = platform_driver_register(&mtk_mfg_2d_driver);
	if (ret != 0) {
		pr_err("Failed to register mfg async driver\n");
		return ret;
	}

	return ret;
}
subsys_initcall(mtk_mfg_2d_init);
