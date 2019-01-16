#include <linux/module.h>
#include <linux/sched.h>
#include "mtk_mfgsys.h"
#include <mach/mt_clkmgr.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_boot.h>
#include <mach/mt_gpufreq.h>
#include "rgxdevice.h"
#include "osfunc.h"
#include "pvrsrv.h"
#include "rgxhwperf.h"

/* MTK: disable 6795 DVFS temporarily, fix and remove me */

#ifdef CONFIG_MTK_HIBERNATION
#include "sysconfig.h"
#include <mach/mtk_hibernate_dpm.h>
#include <mach/mt_irq.h>
#include <mach/irqs.h>
#endif

#include <trace/events/mtk_events.h>
#include <linux/mtk_gpu_utility.h>

#define MTK_DEFER_DVFS_WORK_MS          10000
#define MTK_DVFS_SWITCH_INTERVAL_MS     50//16//100
#define MTK_SYS_BOOST_DURATION_MS       50
#define MTK_WAIT_FW_RESPONSE_TIMEOUT_US 5000
#define MTK_GPIO_REG_OFFSET             0x30
#define MTK_GPU_FREQ_ID_INVALID         0xFFFFFFFF
#define MTK_RGX_DEVICE_INDEX_INVALID    -1

static IMG_HANDLE g_hDVFSTimer = NULL;
static POS_LOCK ghDVFSTimerLock = NULL;
static POS_LOCK ghDVFSLock = NULL;
#if defined(MTK_USE_HW_APM) && defined(CONFIG_ARCH_MT6795)
static IMG_PVOID g_pvRegsKM = NULL;
#endif
#ifdef MTK_CAL_POWER_INDEX
static IMG_PVOID g_pvRegsBaseKM = NULL;
#endif

static IMG_BOOL g_bTimerEnable = IMG_FALSE;
static IMG_BOOL g_bExit = IMG_TRUE;
static IMG_INT32 g_iSkipCount;
static IMG_UINT32 g_sys_dvfs_time_ms;

static IMG_UINT32 g_bottom_freq_id;
static IMG_UINT32 gpu_bottom_freq;
static IMG_UINT32 g_cust_boost_freq_id;
static IMG_UINT32 gpu_cust_boost_freq;
static IMG_UINT32 g_cust_upbound_freq_id;
static IMG_UINT32 gpu_cust_upbound_freq;

static IMG_UINT32 gpu_power = 0;
static IMG_UINT32 gpu_dvfs_enable;
static IMG_UINT32 boost_gpu_enable;
static IMG_UINT32 gpu_debug_enable;
static IMG_UINT32 gpu_dvfs_force_idle = 0;
static IMG_UINT32 gpu_dvfs_cb_force_idle = 0;

static IMG_UINT32 gpu_pre_loading = 0;
static IMG_UINT32 gpu_loading = 0;
static IMG_UINT32 gpu_block = 0;
static IMG_UINT32 gpu_idle = 0;
static IMG_UINT32 gpu_freq = 0;

static IMG_BOOL g_bUnsync =IMG_FALSE;
static IMG_UINT32 g_ui32_unsync_freq_id = 0;

#ifdef CONFIG_MTK_SEGMENT_TEST
static IMG_UINT32 efuse_mfg_enable =0;
#endif

#ifdef CONFIG_MTK_HIBERNATION
int gpu_pm_restore_noirq(struct device *device);
extern void mt_irq_set_sens(unsigned int irq, unsigned int sens);
extern void mt_irq_set_polarity(unsigned int irq, unsigned int polarity);
int gpu_pm_restore_noirq(struct device *device)
{
#if defined(MTK_CONFIG_OF) && defined(CONFIG_OF)
    int irq = MTKSysGetIRQ();
#else
    int irq = SYS_MTK_RGX_IRQ;
#endif
    mt_irq_set_sens(irq, MT_LEVEL_SENSITIVE);
    mt_irq_set_polarity(irq, MT_POLARITY_LOW);
    return 0;
}
#endif

static PVRSRV_DEVICE_NODE* MTKGetRGXDevNode(IMG_VOID)
{
    PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();
    IMG_UINT32 i;
    for (i = 0; i < psPVRSRVData->ui32RegisteredDevices; i++)
    {
        PVRSRV_DEVICE_NODE* psDeviceNode = psPVRSRVData->apsRegisteredDevNodes[i];
        if (psDeviceNode && psDeviceNode->psDevConfig &&
            psDeviceNode->psDevConfig->eDeviceType == PVRSRV_DEVICE_TYPE_RGX)
        {
            return psDeviceNode;
        }
    }
    return NULL;
}

static IMG_UINT32 MTKGetRGXDevIdx(IMG_VOID)
{
    static IMG_UINT32 ms_ui32RGXDevIdx = MTK_RGX_DEVICE_INDEX_INVALID;
    if (MTK_RGX_DEVICE_INDEX_INVALID == ms_ui32RGXDevIdx)
    {
        PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();
        IMG_UINT32 i;
        for (i = 0; i < psPVRSRVData->ui32RegisteredDevices; i++)
        {
            PVRSRV_DEVICE_NODE* psDeviceNode = psPVRSRVData->apsRegisteredDevNodes[i];
            if (psDeviceNode && psDeviceNode->psDevConfig &&
                psDeviceNode->psDevConfig->eDeviceType == PVRSRV_DEVICE_TYPE_RGX)
            {
                ms_ui32RGXDevIdx = i;
                break;
            }
        }
    }
    return ms_ui32RGXDevIdx;
}

static IMG_VOID MTKWriteBackFreqToRGX(IMG_UINT32 ui32DeviceIndex, IMG_UINT32 ui32NewFreq)
{
    PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();
    PVRSRV_DEVICE_NODE* psDeviceNode = psPVRSRVData->apsRegisteredDevNodes[ui32DeviceIndex];
    RGX_DATA* psRGXData = (RGX_DATA*)psDeviceNode->psDevConfig->hDevData;
    psRGXData->psRGXTimingInfo->ui32CoreClockSpeed = ui32NewFreq * 1000; /* kHz to Hz write to RGX as the same unit */
}

static IMG_VOID MTKEnableMfgClock(void)
{
    mt_gpufreq_voltage_enable_set(1);

	enable_clock(MT_CG_MFG_AXI, "MFG");
	enable_clock(MT_CG_MFG_MEM, "MFG");
	enable_clock(MT_CG_MFG_G3D, "MFG");
	enable_clock(MT_CG_MFG_26M, "MFG");

#if defined(CONFIG_ARCH_MT6795)
#else
#ifdef CONFIG_MTK_SEGMENT_TEST
	//check mfg
	if (DRV_Reg32(0xf0006610) & 0x10)
	{
		efuse_mfg_enable =1;
	}
#endif
#endif

    if (gpu_debug_enable)
    {
        PVR_DPF((PVR_DBG_ERROR, "MTKEnableMfgClock"));
    }
}

#define MFG_BUS_IDLE_BIT ( 1 << 16 )

static IMG_VOID MTKDisableMfgClock(void)
{
    
    volatile int polling_count = 200000;
    volatile int i = 0;
    
    do {
        /// 0x13FFF000[16]
        /// 1'b1: bus idle
        /// 1'b0: bus busy  
        if (  DRV_Reg32(g_pvRegsKM) & MFG_BUS_IDLE_BIT )
        {
	    i = 1;
            break;
        }
        
    } while (polling_count--);

#ifdef MTK_DEBUG    
    if(i==0)
        PVR_DPF((PVR_DBG_WARNING, "MTKDisableMfgClock: wait IDLE TIMEOUT"));
#endif
    
	disable_clock(MT_CG_MFG_26M, "MFG");
	disable_clock(MT_CG_MFG_G3D, "MFG");
	disable_clock(MT_CG_MFG_MEM, "MFG");
	disable_clock(MT_CG_MFG_AXI, "MFG");

    mt_gpufreq_voltage_enable_set(0);

    if (gpu_debug_enable)
    {
        PVR_DPF((PVR_DBG_ERROR, "MTKDisableMfgClock"));
    }
}

#if defined(MTK_USE_HW_APM) && defined(CONFIG_ARCH_MT6795)
static int MTKInitHWAPM(void)
{
    if (!g_pvRegsKM)
    {
        PVRSRV_DEVICE_NODE* psDevNode = MTKGetRGXDevNode();
        if (psDevNode)
        {
            IMG_CPU_PHYADDR sRegsPBase;
            PVRSRV_RGXDEV_INFO* psDevInfo = psDevNode->pvDevice;
            PVRSRV_DEVICE_CONFIG *psDevConfig = psDevNode->psDevConfig;
            if (psDevInfo)
            {
                PVR_DPF((PVR_DBG_ERROR, "psDevInfo->pvRegsBaseKM: %p", psDevInfo->pvRegsBaseKM));
            }
            if (psDevConfig)
            {
                sRegsPBase = psDevConfig->sRegsCpuPBase;
                sRegsPBase.uiAddr += 0xfff000;
                PVR_DPF((PVR_DBG_ERROR, "sRegsCpuPBase.uiAddr: 0x%08lx", (unsigned long)psDevConfig->sRegsCpuPBase.uiAddr));
                PVR_DPF((PVR_DBG_ERROR, "sRegsPBase.uiAddr: 0x%08lx", (unsigned long)sRegsPBase.uiAddr));
                g_pvRegsKM = OSMapPhysToLin(sRegsPBase, 0xFF, 0);
            }
        }
    }

    if (g_pvRegsKM)
    {
#if 0
    	DRV_WriteReg32(g_pvRegsKM + 0x24, 0x80076674);
    	DRV_WriteReg32(g_pvRegsKM + 0x28, 0x0e6d0a09);
#else
        //DRV_WriteReg32(g_pvRegsKM + 0xa0, 0x00bd0140);
        DRV_WriteReg32(g_pvRegsKM + 0x24, 0x002f313f);
        DRV_WriteReg32(g_pvRegsKM + 0x28, 0x3f383609);
        DRV_WriteReg32(g_pvRegsKM + 0xe0, 0x6c630176);
        DRV_WriteReg32(g_pvRegsKM + 0xe4, 0x75515a48);
        DRV_WriteReg32(g_pvRegsKM + 0xe8, 0x00210228);
        DRV_WriteReg32(g_pvRegsKM + 0xec, 0x80000000);
#endif
    }
	return PVRSRV_OK;
}

static int MTKDeInitHWAPM(void)
{
#if 0
    if (g_pvRegsKM)
    {
	    DRV_WriteReg32(g_pvRegsKM + 0x24, 0x0);
    	DRV_WriteReg32(g_pvRegsKM + 0x28, 0x0);
    }
#endif
	return PVRSRV_OK;
}
#endif

static IMG_BOOL MTKDoGpuDVFS(IMG_UINT32 ui32NewFreqID, IMG_BOOL bIdleDevice)
{
    PVRSRV_ERROR eResult;
    IMG_UINT32 ui32RGXDevIdx;
    IMG_BOOL bet = IMG_FALSE;

    // bottom bound
    if (ui32NewFreqID > g_bottom_freq_id)
    {
        ui32NewFreqID = g_bottom_freq_id;
    }
    if (ui32NewFreqID > g_cust_boost_freq_id)
    {
        ui32NewFreqID = g_cust_boost_freq_id;
    }

    // up bound
    if (ui32NewFreqID < g_cust_upbound_freq_id)
    {
        ui32NewFreqID = g_cust_upbound_freq_id;
    }

    // thermal power limit
    if (ui32NewFreqID < mt_gpufreq_get_thermal_limit_index())
    {
        ui32NewFreqID = mt_gpufreq_get_thermal_limit_index();
    }

    // no change
    if (ui32NewFreqID == mt_gpufreq_get_cur_freq_index())
    {
        return IMG_FALSE;
    }

    ui32RGXDevIdx = MTKGetRGXDevIdx();
    if (MTK_RGX_DEVICE_INDEX_INVALID == ui32RGXDevIdx)
    {
        return IMG_FALSE;
    }

    eResult = PVRSRVDevicePreClockSpeedChange(ui32RGXDevIdx, bIdleDevice, (IMG_VOID*)NULL);

    if ((PVRSRV_OK == eResult) || (PVRSRV_ERROR_RETRY == eResult))
    {
        unsigned int ui32GPUFreq;
        unsigned int ui32CurFreqID;
        PVRSRV_DEV_POWER_STATE ePowerState;

        PVRSRVGetDevicePowerState(ui32RGXDevIdx, &ePowerState);
        if (ePowerState == PVRSRV_DEV_POWER_STATE_ON)
        {
            mt_gpufreq_target(ui32NewFreqID);
            g_bUnsync = IMG_FALSE;
        }
        else
        {
            g_ui32_unsync_freq_id = ui32NewFreqID;
            g_bUnsync = IMG_TRUE;
        }

        ui32CurFreqID = mt_gpufreq_get_cur_freq_index();
        ui32GPUFreq = mt_gpufreq_get_frequency_by_level(ui32CurFreqID);
        gpu_freq = ui32GPUFreq;
#if defined(CONFIG_TRACING) && defined(CONFIG_MTK_SCHED_TRACERS)
        if (RGXHWPerfFTraceGPUEventsEnabled())
        {
            trace_gpu_freq(ui32GPUFreq);
        }
#endif
        MTKWriteBackFreqToRGX(ui32RGXDevIdx, ui32GPUFreq);

#ifdef MTK_DEBUG
        printk(KERN_ERR "PVR_K: 3DFreq=%d, Volt=%d\n", ui32GPUFreq, mt_gpufreq_get_cur_volt());
#endif

        if (PVRSRV_OK == eResult)
        {
            PVRSRVDevicePostClockSpeedChange(ui32RGXDevIdx, bIdleDevice, (IMG_VOID*)NULL);
        }

        return IMG_TRUE;
    }

    return IMG_FALSE;
}

static void MTKFreqInputBoostCB(unsigned int ui32BoostFreqID)
{
    if (0 < g_iSkipCount)
    {
        return;
    }

    if (boost_gpu_enable == 0)
    {
        return;
    }

    OSLockAcquire(ghDVFSLock);

    if (ui32BoostFreqID < mt_gpufreq_get_cur_freq_index())
    {
        if (MTKDoGpuDVFS(ui32BoostFreqID, gpu_dvfs_cb_force_idle == 0 ? IMG_FALSE : IMG_TRUE))
        {
            g_sys_dvfs_time_ms = OSClockms();
        }
    }

    OSLockRelease(ghDVFSLock);

}

static void MTKFreqPowerLimitCB(unsigned int ui32LimitFreqID)
{
    if (0 < g_iSkipCount)
    {
        return;
    }

    OSLockAcquire(ghDVFSLock);

    if (ui32LimitFreqID > mt_gpufreq_get_cur_freq_index())
    {
        if (MTKDoGpuDVFS(ui32LimitFreqID, gpu_dvfs_cb_force_idle == 0 ? IMG_FALSE : IMG_TRUE))
        {
            g_sys_dvfs_time_ms = OSClockms();
        }
    }

    OSLockRelease(ghDVFSLock);
}

#ifdef MTK_CAL_POWER_INDEX
static IMG_VOID MTKStartPowerIndex(IMG_VOID)
{
    if (!g_pvRegsBaseKM)
    {
        PVRSRV_DEVICE_NODE* psDevNode = MTKGetRGXDevNode();
        if (psDevNode)
        {
            PVRSRV_RGXDEV_INFO* psDevInfo = psDevNode->pvDevice;
            if (psDevInfo)
            {
                g_pvRegsBaseKM = psDevInfo->pvRegsBaseKM;
            }
        }
    }
    if (g_pvRegsBaseKM)
    {
        DRV_WriteReg32(g_pvRegsBaseKM + (uintptr_t)0x6320, 0x1);
    }
}

static IMG_VOID MTKReStartPowerIndex(IMG_VOID)
{
    if (g_pvRegsBaseKM)
    {
        DRV_WriteReg32(g_pvRegsBaseKM + (uintptr_t)0x6320, 0x1);
    }
}

static IMG_VOID MTKStopPowerIndex(IMG_VOID)
{
    if (g_pvRegsBaseKM)
    {
        DRV_WriteReg32(g_pvRegsBaseKM + (uintptr_t)0x6320, 0x0);
    }
}

static IMG_UINT32 MTKCalPowerIndex(IMG_VOID)
{
    IMG_UINT32 ui32State, ui32Result;
    PVRSRV_DEV_POWER_STATE  ePowerState;
    IMG_BOOL bTimeout;
    IMG_UINT32 u32Deadline;
    IMG_PVOID pvGPIO_REG = g_pvRegsKM + (uintptr_t)MTK_GPIO_REG_OFFSET;
    IMG_PVOID pvPOWER_ESTIMATE_RESULT = g_pvRegsBaseKM + (uintptr_t)6328;

    if ((!g_pvRegsKM) || (!g_pvRegsBaseKM))
    {
        return 0;
    }

    if (PVRSRVPowerLock() != PVRSRV_OK)
    {
        return 0;
    }

	PVRSRVGetDevicePowerState(MTKGetRGXDevIdx(), &ePowerState);
    if (ePowerState != PVRSRV_DEV_POWER_STATE_ON)
	{
		PVRSRVPowerUnlock();
		return 0;
	}

    //writes 1 to GPIO_INPUT_REQ, bit[0]
    DRV_WriteReg32(pvGPIO_REG, DRV_Reg32(pvGPIO_REG) | 0x1);

    //wait for 1 in GPIO_OUTPUT_REQ, bit[16]
    bTimeout = IMG_TRUE;
    u32Deadline = OSClockus() + MTK_WAIT_FW_RESPONSE_TIMEOUT_US;
    while(OSClockus() < u32Deadline)
    {
        if (0x10000 & DRV_Reg32(pvGPIO_REG))
        {
            bTimeout = IMG_FALSE;
            break;
        }
    }

    //writes 0 to GPIO_INPUT_REQ, bit[0]
    DRV_WriteReg32(pvGPIO_REG, DRV_Reg32(pvGPIO_REG) & (~0x1));
    if (bTimeout)
    {
        PVRSRVPowerUnlock();
        return 0;
    }

    //read GPIO_OUTPUT_DATA, bit[24]
    ui32State = DRV_Reg32(pvGPIO_REG) >> 24;

    //read POWER_ESTIMATE_RESULT
    ui32Result = DRV_Reg32(pvPOWER_ESTIMATE_RESULT);

    //writes 1 to GPIO_OUTPUT_ACK, bit[17]
    DRV_WriteReg32(pvGPIO_REG, DRV_Reg32(pvGPIO_REG)|0x20000);

    //wait for 0 in GPIO_OUTPUT_REG, bit[16]
    bTimeout = IMG_TRUE;
    u32Deadline = OSClockus() + MTK_WAIT_FW_RESPONSE_TIMEOUT_US;
    while(OSClockus() < u32Deadline)
    {
        if (!(0x10000 & DRV_Reg32(pvGPIO_REG)))
        {
            bTimeout = IMG_FALSE;
            break;
        }
    }

    //writes 0 to GPIO_OUTPUT_ACK, bit[17]
    DRV_WriteReg32(pvGPIO_REG, DRV_Reg32(pvGPIO_REG) & (~0x20000));
    if (bTimeout)
    {
        PVRSRVPowerUnlock();
        return 0;
    }

    MTKReStartPowerIndex();

    PVRSRVPowerUnlock();

    return (1 == ui32State) ? ui32Result : 0;
}
#endif

static IMG_VOID MTKCalGpuLoading(unsigned int* pui32Loading , unsigned int* pui32Block,unsigned int* pui32Idle)
{
    PVRSRV_DEVICE_NODE* psDevNode = MTKGetRGXDevNode();
    if (!psDevNode)
    {
        return;
    }
    PVRSRV_RGXDEV_INFO* psDevInfo = psDevNode->pvDevice;
    if (psDevInfo && psDevInfo->pfnGetGpuUtilStats)
    {
        RGXFWIF_GPU_UTIL_STATS sGpuUtilStats = {0};
        sGpuUtilStats = psDevInfo->pfnGetGpuUtilStats(psDevInfo->psDeviceNode);
        if (sGpuUtilStats.bValid)
        {
#if 0
            PVR_DPF((PVR_DBG_ERROR,"Loading: A(%d), I(%d), B(%d)",
                sGpuUtilStats.ui32GpuStatActive, sGpuUtilStats.ui32GpuStatIdle, sGpuUtilStats.ui32GpuStatBlocked));
#endif

            *pui32Loading = sGpuUtilStats.ui32GpuStatActiveHigh/100;
            *pui32Block = sGpuUtilStats.ui32GpuStatBlocked/100;
            *pui32Idle = sGpuUtilStats.ui32GpuStatIdle/100;
        }
    }
}

static IMG_BOOL MTKGpuDVFSPolicy(IMG_UINT32 ui32GPULoading, unsigned int* pui32NewFreqID)
{
    int i32MaxLevel = (int)(mt_gpufreq_get_dvfs_table_num() - 1);
    int i32CurFreqID = (int)mt_gpufreq_get_cur_freq_index();
    int i32NewFreqID = i32CurFreqID;
//charge by zhoulingyun for cx861 powersave (wufangqi 20150906) start
   #if 0
   if (ui32GPULoading >= 99)
    {
        i32NewFreqID = 0;
    }
    else if (ui32GPULoading <= 1)
    {
        i32NewFreqID = i32MaxLevel;
    }
    else if (ui32GPULoading >= 85)
    {
        i32NewFreqID -= 2;
    }
    else if (ui32GPULoading <= 30)
    {
        i32NewFreqID += 2;
    }
    else if (ui32GPULoading >= 70)
    {
        i32NewFreqID -= 1;
    }
    else if (ui32GPULoading <= 50)
    {
        i32NewFreqID += 1;
    }
   #else
    if (ui32GPULoading >= 99)
    {
        i32NewFreqID = 0;
    }
    else if (ui32GPULoading <= 1)
    {
        i32NewFreqID = i32MaxLevel;
    }
    else if (ui32GPULoading >= 95)
    {
        i32NewFreqID -= 2;
    }
    else if (ui32GPULoading <= 40)
    {
        i32NewFreqID += 2;
    }
    else if (ui32GPULoading >= 80)
    {
        i32NewFreqID -= 1;
    }
    else if (ui32GPULoading <= 60)
    {
        i32NewFreqID += 1;
    }
    #endif 
//charge by zhoulingyun for cx861 powersave (wufangqi 20150906) end
    if (i32NewFreqID < i32CurFreqID)
    {
        if (gpu_pre_loading * 17 / 10 < ui32GPULoading)
        {
            i32NewFreqID -= 1;
        }
    }
    else if (i32NewFreqID > i32CurFreqID)
    {
        if (ui32GPULoading * 17 / 10 < gpu_pre_loading)
        {
            i32NewFreqID += 1;
        }
    }

    if (i32NewFreqID > i32MaxLevel)
    {
        i32NewFreqID = i32MaxLevel;
    }
    else if (i32NewFreqID < 0)
    {
        i32NewFreqID = 0;
    }

    if (i32NewFreqID != i32CurFreqID)
    {
        
        *pui32NewFreqID = (unsigned int)i32NewFreqID;
        return IMG_TRUE;
    }
    
    return IMG_FALSE;
}

static IMG_VOID MTKDVFSTimerFuncCB(IMG_PVOID pvData)
{
    int i32MaxLevel = (int)(mt_gpufreq_get_dvfs_table_num() - 1);
    int i32CurFreqID = (int)mt_gpufreq_get_cur_freq_index();

    if (gpu_debug_enable)
    {
        PVR_DPF((PVR_DBG_ERROR, "MTKDVFSTimerFuncCB"));
    }

    if (0 == gpu_dvfs_enable)
    {
        gpu_power = 0;
        gpu_loading = 0;
        gpu_block= 0;
        gpu_idle = 0;
        return;
    }

    if (g_iSkipCount > 0)
    {
        gpu_power = 0;
        gpu_loading = 0;
        gpu_block= 0;
        gpu_idle = 0;
        g_iSkipCount -= 1;
    }
    else if ((!g_bExit) || (i32CurFreqID < i32MaxLevel))
    {
        IMG_UINT32 ui32NewFreqID;

        // calculate power index
#ifdef MTK_CAL_POWER_INDEX
        gpu_power = MTKCalPowerIndex();
#else
        gpu_power = 0;
#endif

        MTKCalGpuLoading(&gpu_loading, &gpu_block, &gpu_idle);

        OSLockAcquire(ghDVFSLock);

        // check system boost duration
        if ((g_sys_dvfs_time_ms > 0) && (OSClockms() - g_sys_dvfs_time_ms < MTK_SYS_BOOST_DURATION_MS))
        {
            OSLockRelease(ghDVFSLock);
            return;
        }
        else
        {
            g_sys_dvfs_time_ms = 0;
        }

        // do gpu dvfs
        if (MTKGpuDVFSPolicy(gpu_loading, &ui32NewFreqID))
        {
            MTKDoGpuDVFS(ui32NewFreqID, gpu_dvfs_force_idle == 0 ? IMG_FALSE : IMG_TRUE);
        }

        gpu_pre_loading = gpu_loading;

        OSLockRelease(ghDVFSLock);
    }
}

void MTKMFGEnableDVFSTimer(bool bEnable)
{
    if (gpu_debug_enable)
    {
        PVR_DPF((PVR_DBG_ERROR, "MTKMFGEnableDVFSTimer: %s", bEnable ? "yes" : "no"));
    }

    if (NULL == g_hDVFSTimer)
    {
        PVR_DPF((PVR_DBG_ERROR, "MTKMFGEnableDVFSTimer: g_hDVFSTimer is NULL"));
        return;
    }

    OSLockAcquire(ghDVFSTimerLock);

    if (bEnable)
    {
        if (!g_bTimerEnable)
        {
            if (PVRSRV_OK == OSEnableTimer(g_hDVFSTimer))
            {
                g_bTimerEnable = IMG_TRUE;
            }
        }
    }
    else
    {
        if (g_bTimerEnable)
        {
            if (PVRSRV_OK == OSDisableTimer(g_hDVFSTimer))
            {
                g_bTimerEnable = IMG_FALSE;
            }
        }
    }

    OSLockRelease(ghDVFSTimerLock);
}

PVRSRV_ERROR MTKDevPrePowerState(PVRSRV_DEV_POWER_STATE eNewPowerState,
                                         PVRSRV_DEV_POWER_STATE eCurrentPowerState,
									     IMG_BOOL bForced)
{
    if( PVRSRV_DEV_POWER_STATE_OFF == eNewPowerState &&
        PVRSRV_DEV_POWER_STATE_ON == eCurrentPowerState )
    {
        if (g_hDVFSTimer && PVRSRVGetInitServerState(PVRSRV_INIT_SERVER_SUCCESSFUL))
        {
            g_bExit = IMG_TRUE;

#ifdef MTK_CAL_POWER_INDEX
            MTKStopPowerIndex();
#endif
        }

#if defined(MTK_USE_HW_APM) && defined(CONFIG_ARCH_MT6795)
        MTKDeInitHWAPM();
#endif
        MTKDisableMfgClock();
    }

	return PVRSRV_OK;
}

PVRSRV_ERROR MTKDevPostPowerState(PVRSRV_DEV_POWER_STATE eNewPowerState,
                                          PVRSRV_DEV_POWER_STATE eCurrentPowerState,
									      IMG_BOOL bForced)
{
    if( PVRSRV_DEV_POWER_STATE_OFF == eCurrentPowerState &&
        PVRSRV_DEV_POWER_STATE_ON == eNewPowerState)
    {
        MTKEnableMfgClock();

#if defined(MTK_USE_HW_APM) && defined(CONFIG_ARCH_MT6795)
        MTKInitHWAPM();
#endif
        if (g_hDVFSTimer && PVRSRVGetInitServerState(PVRSRV_INIT_SERVER_SUCCESSFUL))
        {
#ifdef MTK_CAL_POWER_INDEX
            MTKStartPowerIndex();
#endif
            g_bExit = IMG_FALSE;
        }
#if 0
        if (g_iSkipCount > 0)
        {
            // During boot up
            unsigned int ui32NewFreqID = mt_gpufreq_get_dvfs_table_num() - 1;
            unsigned int ui32CurFreqID = mt_gpufreq_get_cur_freq_index();
            if (ui32NewFreqID != ui32CurFreqID)
            {
                IMG_UINT32 ui32RGXDevIdx = MTKGetRGXDevIdx();
                unsigned int ui32GPUFreq = mt_gpufreq_get_frequency_by_level(ui32NewFreqID);
                mt_gpufreq_target(ui32NewFreqID);
                MTKWriteBackFreqToRGX(ui32RGXDevIdx, ui32GPUFreq);
            }
        }
#endif
		if(IMG_TRUE == g_bUnsync)
		{
			mt_gpufreq_target(g_ui32_unsync_freq_id);
			g_bUnsync = IMG_FALSE;
		}
    }

    return PVRSRV_OK;
}

PVRSRV_ERROR MTKSystemPrePowerState(PVRSRV_SYS_POWER_STATE eNewPowerState)
{
	if(PVRSRV_SYS_POWER_STATE_OFF == eNewPowerState)
    {
        ;
    }

	return PVRSRV_OK;
}

PVRSRV_ERROR MTKSystemPostPowerState(PVRSRV_SYS_POWER_STATE eNewPowerState)
{
	if(PVRSRV_SYS_POWER_STATE_ON == eNewPowerState)
	{
    }

	return PVRSRV_OK;
}

static void MTKBoostGpuFreq(void)
{
    if (gpu_debug_enable)
    {
        PVR_DPF((PVR_DBG_ERROR, "MTKBoostGpuFreq"));
    }
    MTKFreqInputBoostCB(0);
}

static void MTKSetBottomGPUFreq(unsigned int ui32FreqLevel)
{
    unsigned int ui32MaxLevel;

    if (gpu_debug_enable)
    {
        PVR_DPF((PVR_DBG_ERROR, "MTKSetBottomGPUFreq: freq = %d", ui32FreqLevel));
    }

    ui32MaxLevel = mt_gpufreq_get_dvfs_table_num() - 1;
    if (ui32MaxLevel < ui32FreqLevel)
    {
        ui32FreqLevel = ui32MaxLevel;
    }

    OSLockAcquire(ghDVFSLock);

    // 0 => The highest frequency
    // table_num - 1 => The lowest frequency
    g_bottom_freq_id = ui32MaxLevel - ui32FreqLevel;
    gpu_bottom_freq = mt_gpufreq_get_frequency_by_level(g_bottom_freq_id);

    if (g_bottom_freq_id < mt_gpufreq_get_cur_freq_index())
    {
        MTKDoGpuDVFS(g_bottom_freq_id, gpu_dvfs_cb_force_idle == 0 ? IMG_FALSE : IMG_TRUE);
    }
     
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
    {
        PVR_DPF((PVR_DBG_ERROR, "MTKCustomBoostGpuFreq: freq = %d", ui32FreqLevel));
    }

    ui32MaxLevel = mt_gpufreq_get_dvfs_table_num() - 1;
    if (ui32MaxLevel < ui32FreqLevel)
    {
        ui32FreqLevel = ui32MaxLevel;
    }

    OSLockAcquire(ghDVFSLock);

    // 0 => The highest frequency
    // table_num - 1 => The lowest frequency
    g_cust_boost_freq_id = ui32MaxLevel - ui32FreqLevel;
    gpu_cust_boost_freq = mt_gpufreq_get_frequency_by_level(g_cust_boost_freq_id);

    if (g_cust_boost_freq_id < mt_gpufreq_get_cur_freq_index())
    {
        MTKDoGpuDVFS(g_cust_boost_freq_id, gpu_dvfs_cb_force_idle == 0 ? IMG_FALSE : IMG_TRUE);
    }

    OSLockRelease(ghDVFSLock);
}

static void MTKCustomUpBoundGpuFreq(unsigned int ui32FreqLevel)
{
    unsigned int ui32MaxLevel;

    if (gpu_debug_enable)
    {
        PVR_DPF((PVR_DBG_ERROR, "MTKCustomUpBoundGpuFreq: freq = %d", ui32FreqLevel));
    }

    ui32MaxLevel = mt_gpufreq_get_dvfs_table_num() - 1;
    if (ui32MaxLevel < ui32FreqLevel)
    {
        ui32FreqLevel = ui32MaxLevel;
    }

    OSLockAcquire(ghDVFSLock);

    // 0 => The highest frequency
    // table_num - 1 => The lowest frequency
    g_cust_upbound_freq_id = ui32MaxLevel - ui32FreqLevel;
    gpu_cust_upbound_freq = mt_gpufreq_get_frequency_by_level(g_cust_upbound_freq_id);

    if (g_cust_upbound_freq_id > mt_gpufreq_get_cur_freq_index())
    {
        MTKDoGpuDVFS(g_cust_upbound_freq_id, gpu_dvfs_cb_force_idle == 0 ? IMG_FALSE : IMG_TRUE);
    }
     
    OSLockRelease(ghDVFSLock);
}

unsigned int MTKGetCustomBoostGpuFreq(void)
{
    unsigned int ui32MaxLevel = mt_gpufreq_get_dvfs_table_num() - 1;
    return ui32MaxLevel - g_cust_boost_freq_id;
}

unsigned int MTKGetCustomUpBoundGpuFreq(void)
{
    unsigned int ui32MaxLevel = mt_gpufreq_get_dvfs_table_num() - 1;
    return ui32MaxLevel - g_cust_upbound_freq_id;
}

static IMG_UINT32 MTKGetGpuLoading(IMG_VOID)
{
    return gpu_loading;
}

static IMG_UINT32 MTKGetGpuBlock(IMG_VOID)
{
    return gpu_block;
}

static IMG_UINT32 MTKGetGpuIdle(IMG_VOID)
{
    return gpu_idle;
}

static IMG_UINT32 MTKGetPowerIndex(IMG_VOID)
{
    return gpu_power;
}

typedef void (*gpufreq_input_boost_notify)(unsigned int );
typedef void (*gpufreq_power_limit_notify)(unsigned int );
extern void mt_gpufreq_input_boost_notify_registerCB(gpufreq_input_boost_notify pCB);
extern void mt_gpufreq_power_limit_notify_registerCB(gpufreq_power_limit_notify pCB);

extern unsigned int (*mtk_get_gpu_loading_fp)(void);
extern unsigned int (*mtk_get_gpu_block_fp)(void);
extern unsigned int (*mtk_get_gpu_idle_fp)(void);
extern unsigned int (*mtk_get_gpu_power_loading_fp)(void);
extern void (*mtk_enable_gpu_dvfs_timer_fp)(bool bEnable);
extern void (*mtk_boost_gpu_freq_fp)(void);
extern void (*mtk_set_bottom_gpu_freq_fp)(unsigned int);

extern unsigned int (*mtk_custom_get_gpu_freq_level_count_fp)(void);
extern void (*mtk_custom_boost_gpu_freq_fp)(unsigned int ui32FreqLevel);
extern void (*mtk_custom_upbound_gpu_freq_fp)(unsigned int ui32FreqLevel);
extern unsigned int (*mtk_get_custom_boost_gpu_freq_fp)(void);
extern unsigned int (*mtk_get_custom_upbound_gpu_freq_fp)(void);

PVRSRV_ERROR MTKMFGSystemInit(void)
{
    PVRSRV_ERROR error;

	error = OSLockCreate(&ghDVFSLock, LOCK_TYPE_PASSIVE);
	if (error != PVRSRV_OK)
    {
        PVR_DPF((PVR_DBG_ERROR, "Create DVFS Lock Failed"));
        goto ERROR;
    }

	error = OSLockCreate(&ghDVFSTimerLock, LOCK_TYPE_PASSIVE);
	if (error != PVRSRV_OK)
    {
        PVR_DPF((PVR_DBG_ERROR, "Create DVFS Timer Lock Failed"));
        goto ERROR;
    }

    g_iSkipCount = MTK_DEFER_DVFS_WORK_MS / MTK_DVFS_SWITCH_INTERVAL_MS;

    g_hDVFSTimer = OSAddTimer(MTKDVFSTimerFuncCB, (IMG_VOID *)NULL, MTK_DVFS_SWITCH_INTERVAL_MS);
    if(!g_hDVFSTimer)
    {
    	PVR_DPF((PVR_DBG_ERROR, "Create DVFS Timer Failed"));
        goto ERROR;
    }

    if (PVRSRV_OK == OSEnableTimer(g_hDVFSTimer))
    {
        g_bTimerEnable = IMG_TRUE;
    }

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

#ifdef CONFIG_MTK_HIBERNATION
    register_swsusp_restore_noirq_func(ID_M_GPU, gpu_pm_restore_noirq, NULL);
#endif

#if defined(MTK_USE_HW_APM) && defined(CONFIG_ARCH_MT6795)
    if (!g_pvRegsKM)
    {
        PVRSRV_DEVICE_NODE* psDevNode = MTKGetRGXDevNode();
        if (psDevNode)
        {
            IMG_CPU_PHYADDR sRegsPBase;
            PVRSRV_RGXDEV_INFO* psDevInfo = psDevNode->pvDevice;
            PVRSRV_DEVICE_CONFIG *psDevConfig = psDevNode->psDevConfig;
            if (psDevConfig && (!g_pvRegsKM))
            {
                sRegsPBase = psDevConfig->sRegsCpuPBase;
                sRegsPBase.uiAddr += 0xfff000;
        	    g_pvRegsKM = OSMapPhysToLin(sRegsPBase, 0xFF, 0);
            }
        }
    }
#endif

    return PVRSRV_OK;

ERROR:

    MTKMFGSystemDeInit();

    return PVRSRV_ERROR_INIT_FAILURE;
}

IMG_VOID MTKMFGSystemDeInit(void)
{
#ifdef CONFIG_MTK_HIBERNATION
    unregister_swsusp_restore_noirq_func(ID_M_GPU);
#endif

    g_bExit = IMG_TRUE;

	if(g_hDVFSTimer)
	{
        OSDisableTimer(g_hDVFSTimer);
		OSRemoveTimer(g_hDVFSTimer);
		g_hDVFSTimer = IMG_NULL;
    }

    if (ghDVFSLock)
    {
        OSLockDestroy(ghDVFSLock);
        ghDVFSLock = NULL;
    }

    if (ghDVFSTimerLock)
    {
        OSLockDestroy(ghDVFSTimerLock);
        ghDVFSTimerLock = NULL;
    }

#ifdef MTK_CAL_POWER_INDEX
    g_pvRegsBaseKM = NULL;
#endif

#if defined(MTK_USE_HW_APM) && defined(CONFIG_ARCH_MT6795)
    if (g_pvRegsKM)
    {
        OSUnMapPhysToLin(g_pvRegsKM, 0xFF, 0);
        g_pvRegsKM = NULL;
    }
#endif
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

#ifdef CONFIG_MTK_SEGMENT_TEST
module_param(efuse_mfg_enable, uint, 0644);
#endif
