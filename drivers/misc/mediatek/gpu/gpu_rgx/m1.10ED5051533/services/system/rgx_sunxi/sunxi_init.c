/*************************************************************************/ /*!
@Title          System Configuration
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    System Configuration functions
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

#if defined(SUPPORT_ION)
#include "ion_sys.h"
#endif /* defined(SUPPORT_ION) */

#if defined(SUPPORT_PDVFS)
#include "rgxpdvfs.h"
#endif

#include <linux/hardirq.h>
#include <linux/clk.h>
#include <linux/clk-private.h>
#include <linux/io.h>
#include <linux/clk/sunxi_name.h>
#include <linux/clk/sunxi.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <mach/irqs.h>
#include <mach/hardware.h>
#include <mach/platform.h>
#include <mach/sys_config.h>
#include "power.h"
#include "sunxi_init.h"
#include "pvrsrv_device.h"
#include "syscommon.h"

#ifdef CONFIG_CPU_BUDGET_THERMAL
#include <linux/cpu_budget_cooling.h>
#include "rgxdevice.h"
static int Is_powernow = 0;
#endif /* CONFIG_CPU_BUDGET_THERMAL */

#if !defined(PVR_DVFS) && !defined(SUPPORT_PDVFS)
typedef struct
{
	IMG_UINT32			ui32Volt;
	IMG_UINT32			ui32Freq;
} IMG_OPP;
#endif

static const IMG_OPP asOPPTable[] =
{
#if defined(PVR_DVFS) || defined(SUPPORT_PDVFS)
	{ 824,  240000000},
	{ 840,  260000000},
	{ 856,  280000000},
	{ 872,  300000000},
	{ 887,  320000000},
	{ 903,  340000000},
	{ 919,  360000000},
	{ 935,  380000000},
	{ 951,  400000000},
	{ 996,  420000000},
	{ 982,  440000000},
	{ 998,  460000000},
	{ 1014, 480000000},
	{ 1029, 500000000},
	{ 1045, 520000000},
	{ 1061, 540000000},
#else
	{ 700,  48000000},
	{ 800, 120000000},
	{ 800, 240000000},
	{ 900, 320000000},
	{ 900, 384000000},
	{1000, 480000000},
	{1100, 528000000},
#endif
};

#define LEVEL_COUNT (sizeof(asOPPTable) / sizeof(asOPPTable[0]))

#define AXI_CLK_FREQ 320
#define GPU_CTRL "gpuctrl"

static struct clk *gpu_core_clk        = NULL;
static struct clk *gpu_mem_clk         = NULL;
static struct clk *gpu_axi_clk         = NULL;
static struct clk *gpu_pll_clk         = NULL;
static struct clk *gpu_ctrl_clk        = NULL;
static struct regulator *rgx_regulator = NULL;
static char *regulator_id              = "axp22_dcdc2";

#if defined(PVR_DVFS)
	#define DEFAULT_MIN_VF_LEVEL 0
#else
	#define DEFAULT_MIN_VF_LEVEL 4
#endif

static IMG_UINT32 min_vf_level_val     = DEFAULT_MIN_VF_LEVEL;
static IMG_UINT32 max_vf_level_val     = LEVEL_COUNT - 1;

static PVRSRV_DEVICE_CONFIG* gpsDevConfig = NULL;

long int GetConfigFreq(void)
{
	return asOPPTable[min_vf_level_val].ui32Freq;
}

IMG_UINT32 AwClockFreqGet(IMG_HANDLE hSysData)
{
	return (IMG_UINT32)clk_get_rate(gpu_core_clk);
}

static void AssertGpuResetSignal(void)
{
	if(sunxi_periph_reset_assert(gpu_core_clk))
	{
		PVR_DPF((PVR_DBG_ERROR, "Failed to pull down gpu reset!"));
	}
	if(sunxi_periph_reset_assert(gpu_ctrl_clk))
	{
		PVR_DPF((PVR_DBG_ERROR, "Failed to pull down gpu control reset!"));
	}
}

static void DeAssertGpuResetSignal(void)
{
	if(sunxi_periph_reset_deassert(gpu_ctrl_clk))
	{
		PVR_DPF((PVR_DBG_ERROR, "Failed to release gpu control reset!"));
	}
	if(sunxi_periph_reset_deassert(gpu_core_clk))
	{
		PVR_DPF((PVR_DBG_ERROR, "Failed to release gpu reset!"));
	}
}

static void RgxEnableClock(void)
{
	if(gpu_core_clk->enable_count == 0)
	{
		if(clk_prepare_enable(gpu_pll_clk))
		{
			PVR_DPF((PVR_DBG_ERROR, "Failed to enable pll9 clock!"));
		}
		if(clk_prepare_enable(gpu_core_clk))
		{
			PVR_DPF((PVR_DBG_ERROR, "Failed to enable core clock!"));
		}
		if(clk_prepare_enable(gpu_mem_clk))
		{
			PVR_DPF((PVR_DBG_ERROR, "Failed to enable mem clock!"));
		}
		if(clk_prepare_enable(gpu_axi_clk))
		{
			PVR_DPF((PVR_DBG_ERROR, "Failed to enable axi clock!"));
		}
		if(clk_prepare_enable(gpu_ctrl_clk))
		{
			PVR_DPF((PVR_DBG_ERROR, "Failed to enable ctrl clock!"));
		}
	}
}

static void RgxDisableClock(void)
{
	if(gpu_core_clk->enable_count == 1)
	{
		clk_disable_unprepare(gpu_ctrl_clk);
		clk_disable_unprepare(gpu_axi_clk);
		clk_disable_unprepare(gpu_mem_clk);
		clk_disable_unprepare(gpu_core_clk);
		clk_disable_unprepare(gpu_pll_clk);
	}
}

static void RgxEnablePower(void)
{
	if(!regulator_is_enabled(rgx_regulator))
	{
		regulator_enable(rgx_regulator);
	}
}

static void RgxDisablePower(void)
{
	if(regulator_is_enabled(rgx_regulator))
	{
		regulator_disable(rgx_regulator);
	}
}

static void SetVoltage(IMG_UINT32 ui32Volt)
{
	if(regulator_set_voltage(rgx_regulator, ui32Volt*1000, ui32Volt*1000) != 0)
	{
		PVR_DPF((PVR_DBG_ERROR, "Failed to set gpu power voltage!"));
	}
}

static void SetClkVal(const char clk_name[], int freq)
{
	struct clk *clk = NULL;
	
	if(!strcmp(clk_name, "pll"))
	{
		clk = gpu_pll_clk;
	}
	else if(!strcmp(clk_name, "core"))
	{
		clk = gpu_core_clk;
	}
	else if(!strcmp(clk_name, "mem"))
	{
		clk = gpu_mem_clk;
	}
	else
	{
		clk = gpu_axi_clk;
	}
	
	if(clk_set_rate(clk, freq))
	{
		clk = NULL;
		return;
	}

	if(clk == gpu_pll_clk)
	{
		/* delay for gpu pll stability */
		udelay(100);
	}
	
	clk = NULL;
}

#if defined(PVR_DVFS) || defined(SUPPORT_PDVFS)
static void SetFrequency(IMG_UINT32 ui32Frequency)
{
	SetClkVal("pll", (int) ui32Frequency);
}
#endif

static void ParseFexPara(void)
{
	script_item_u regulator_id_fex, min_vf_level, max_vf_level;
	if(SCIRPT_ITEM_VALUE_TYPE_STR == script_get_item("rgx_para", "regulator_id", &regulator_id_fex))
	{
		regulator_id = regulator_id_fex.str;
	}
	
	if(SCIRPT_ITEM_VALUE_TYPE_INT == script_get_item("rgx_para", "min_vf_level", &min_vf_level))
	{
		if((min_vf_level.val >= 0 && min_vf_level.val < LEVEL_COUNT))
		{
			min_vf_level_val = min_vf_level.val;
		}
	}
	else
	{
		goto err_out2;
	}
	
	if(SCIRPT_ITEM_VALUE_TYPE_INT == script_get_item("rgx_para", "max_vf_level", &max_vf_level))
	{
		if(max_vf_level.val >= min_vf_level_val && max_vf_level.val < LEVEL_COUNT)
		{
			max_vf_level_val = max_vf_level.val;
		}
	}
	else
	{
		goto err_out1;
	}
	
	return;

err_out1:
	min_vf_level_val = DEFAULT_MIN_VF_LEVEL;
err_out2:
	regulator_id = "axp22_dcdc2";
	return;
}

PVRSRV_ERROR AwPrePowerState(IMG_HANDLE hSysData,
							 PVRSRV_DEV_POWER_STATE eNewPowerState,
							 PVRSRV_DEV_POWER_STATE eCurrentPowerState,
							 IMG_BOOL bForced)
{
	if (eNewPowerState == PVRSRV_DEV_POWER_STATE_ON)
	{
		RgxEnablePower();
	
		mdelay(2);
	
		/* set external isolation invalid */
		writel(0, SUNXI_R_PRCM_VBASE + GPU_PWROFF_GATING);
	
		DeAssertGpuResetSignal();
		
		RgxEnableClock();
		
		/* set delay for internal power stability */
		writel(0x100, SUNXI_GPU_CTRL_VBASE + 0x18);
	}
	
	return PVRSRV_OK;
}

PVRSRV_ERROR AwPostPowerState(IMG_HANDLE hSysData,
							  PVRSRV_DEV_POWER_STATE eNewPowerState,
							  PVRSRV_DEV_POWER_STATE eCurrentPowerState,
							  IMG_BOOL bForced)
{
	if (eNewPowerState == PVRSRV_DEV_POWER_STATE_OFF)
	{
		RgxDisableClock();
		
		AssertGpuResetSignal();
	
		/* set external isolation valid */
		writel(1, SUNXI_R_PRCM_VBASE + GPU_PWROFF_GATING);
	
		RgxDisablePower();
	}
	
	return PVRSRV_OK;
}

#ifdef CONFIG_CPU_BUDGET_THERMAL
static void RgxDvfsChange(int vf_level, int up_flag)
{
#if !defined (PVR_DVFS)
	PVRSRV_ERROR err;
	err = PVRSRVDevicePreClockSpeedChange(gpsDevConfig->psDevNode, IMG_TRUE, NULL);
	if(err == PVRSRV_OK)
	{
		if(up_flag == 1)
		{
			SetVoltage(asOPPTable[vf_level].ui32Volt);
			SetClkVal("pll", asOPPTable[vf_level].ui32Freq);
		}
		else
		{
			SetClkVal("pll", asOPPTable[vf_level].ui32Freq);
			SetVoltage(asOPPTable[vf_level].ui32Volt);
		}

		/* update core clock speed value in the driver */
		{
			RGX_DATA *psRGXData = gpsDevConfig->hDevData;

			if (psRGXData)
			{
				psRGXData->psRGXTimingInfo->ui32CoreClockSpeed =
					asOPPTable[vf_level].ui32Freq;
			}
			else
			{
				PVR_DPF((PVR_DBG_ERROR, "Failed to update gpu core clock speed value!"));
			}
		}

		PVRSRVDevicePostClockSpeedChange(gpsDevConfig->psDevNode, IMG_TRUE, NULL);
	}
#endif
}

static int rgx_throttle_notifier_call(struct notifier_block *nfb, unsigned long mode, void *cmd)
{
	int retval = NOTIFY_DONE;
	if(mode == BUDGET_GPU_THROTTLE && Is_powernow)
	{
		RgxDvfsChange(min_vf_level_val, 0);
		Is_powernow = 0;
	}
	else
	{
		if(cmd && (*(int *)cmd) == 1 && !Is_powernow)
		{
			RgxDvfsChange(max_vf_level_val, 0);
			Is_powernow = 1;
		}
		else if(cmd && (*(int *)cmd) == 0 && Is_powernow)
		{
			RgxDvfsChange(min_vf_level_val, 0);
			Is_powernow = 0;
		}
	}
	
	return retval;
}

static struct notifier_block rgx_throttle_notifier = {
.notifier_call = rgx_throttle_notifier_call,
};
#endif /* CONFIG_CPU_BUDGET_THERMAL */

void RgxSunxiDeInit(void)
{
#ifdef CONFIG_CPU_BUDGET_THERMAL
	unregister_budget_cooling_notifier(&rgx_throttle_notifier);
#endif /* CONFIG_CPU_BUDGET_THERMAL */
	regulator_put(rgx_regulator);
	rgx_regulator = NULL;
}

void RgxSunxiInit(PVRSRV_DEVICE_CONFIG* psDevConfig)
{
	IMG_UINT32 vf_level_val;

	ParseFexPara();

	rgx_regulator = regulator_get(NULL, regulator_id);
	if (IS_ERR(rgx_regulator))
	{
		PVR_DPF((PVR_DBG_ERROR, "Failed to get rgx regulator!"));
		rgx_regulator = NULL;
		return;
	}
	
	gpu_core_clk = clk_get(NULL, GPUCORE_CLK);
	gpu_mem_clk  = clk_get(NULL, GPUMEM_CLK);
	gpu_axi_clk  = clk_get(NULL, GPUAXI_CLK);
	gpu_pll_clk  = clk_get(NULL, PLL9_CLK);
	gpu_ctrl_clk = clk_get(NULL, GPU_CTRL);

	gpsDevConfig = psDevConfig;

#if defined(PVR_DVFS) || defined(SUPPORT_PDVFS)
	gpsDevConfig->sDVFS.sDVFSDeviceCfg.pasOPPTable = asOPPTable;
	gpsDevConfig->sDVFS.sDVFSDeviceCfg.ui32OPPTableSize = LEVEL_COUNT;
	gpsDevConfig->sDVFS.sDVFSDeviceCfg.pfnSetFrequency = SetFrequency;
	gpsDevConfig->sDVFS.sDVFSDeviceCfg.pfnSetVoltage = SetVoltage;
#endif

	vf_level_val = min_vf_level_val;

	SetVoltage(asOPPTable[vf_level_val].ui32Volt);
	
	SetClkVal("pll", asOPPTable[vf_level_val].ui32Freq);
	SetClkVal("core", asOPPTable[vf_level_val].ui32Freq);
	SetClkVal("mem", asOPPTable[vf_level_val].ui32Freq);
	SetClkVal("axi", AXI_CLK_FREQ);

	(void) AwPrePowerState(psDevConfig->hSysData,
						   PVRSRV_DEV_POWER_STATE_ON,
						   PVRSRV_DEV_POWER_STATE_DEFAULT,
						   IMG_FALSE);

#ifdef CONFIG_CPU_BUDGET_THERMAL
	register_budget_cooling_notifier(&rgx_throttle_notifier);
#endif /* CONFIG_CPU_BUDGET_THERMAL */
}
