/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/version.h>
#include <asm/io.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/genalloc.h>
#include <linux/sched.h>
#include <linux/mutex.h>
//#include <linux/xlog.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <mt-plat/mtk_gpu_utility.h>

#include "ged_base.h"
#include "ged_hal.h"
#include "ged_debugFS.h"

#include "ged_dvfs.h"

#include "ged_notify_sw_vsync.h"

static struct dentry* gpsHALDir = NULL;
static struct dentry* gpsTotalGPUFreqLevelCountEntry = NULL;
static struct dentry* gpsCustomBoostGPUFreqEntry = NULL;
static struct dentry* gpsCustomUpboundGPUFreqEntry = NULL;
static struct dentry* gpsVsyncOffsetLevelEntry = NULL;
static struct dentry* gpsVsyncOffsetEnableEntry = NULL;
static struct dentry* gpsDvfsTuningModeEntry = NULL;
static struct dentry* gpsDvfsCurFreqEntry = NULL;
static struct dentry* gpsDvfsPreFreqEntry = NULL;
static struct dentry* gpsDvfsGpuUtilizationEntry = NULL;
static struct dentry* gpsFpsUpperBoundEntry = NULL;
static struct dentry* gpsIntegrationReportReadEntry = NULL;

int tokenizer(char* pcSrc, int i32len, int* pi32IndexArray, int i32NumToken)
{
	int i = 0;
	int j = 0;
	int head = -1;

	for( ;i<i32len;i++)
	{
		if(pcSrc[i]!=' ')
		{
			if(head==-1)
			{
				head = i;
			}
		}
		else
		{
			if(head!=-1)
			{
				pi32IndexArray[j] = head;
				j++;
				if(j==i32NumToken)
					return j;
				head = -1;
			}
			pcSrc[i] = 0;
		}
	}

	if(head!=-1)
	{
		pi32IndexArray[j] = head;
		j++;
		return j;
	}

	return -1;
}


//-----------------------------------------------------------------------------
static void* ged_total_gpu_freq_level_count_seq_start(struct seq_file *psSeqFile, loff_t *puiPosition)
{
	if (0 == *puiPosition)
	{
		return SEQ_START_TOKEN;
	}

	return NULL;
}
//-----------------------------------------------------------------------------
static void ged_total_gpu_freq_level_count_seq_stop(struct seq_file *psSeqFile, void *pvData)
{

}
//-----------------------------------------------------------------------------
static void* ged_total_gpu_freq_level_count_seq_next(struct seq_file *psSeqFile, void *pvData, loff_t *puiPosition)
{
	return NULL;
}
//-----------------------------------------------------------------------------
static int ged_total_gpu_freq_level_count_seq_show(struct seq_file *psSeqFile, void *pvData)
{
	if (pvData != NULL)
	{
		unsigned int ui32FreqLevelCount;
		if (false == mtk_custom_get_gpu_freq_level_count(&ui32FreqLevelCount))
		{
			ui32FreqLevelCount = 0;
		}
		seq_printf(psSeqFile, "%u\n", ui32FreqLevelCount);
	}

	return 0;
}
//-----------------------------------------------------------------------------
static struct seq_operations gsTotalGPUFreqLevelCountReadOps = 
{
	.start = ged_total_gpu_freq_level_count_seq_start,
	.stop = ged_total_gpu_freq_level_count_seq_stop,
	.next = ged_total_gpu_freq_level_count_seq_next,
	.show = ged_total_gpu_freq_level_count_seq_show,
};
//-----------------------------------------------------------------------------
static ssize_t ged_custom_boost_gpu_freq_write_entry(const char __user *pszBuffer, size_t uiCount, loff_t uiPosition, void *pvData)
{
#define GED_HAL_DEBUGFS_SIZE 64
	char acBuffer[GED_HAL_DEBUGFS_SIZE];

	int i32Value;

	if ((0 < uiCount) && (uiCount < GED_HAL_DEBUGFS_SIZE))
	{
		if (0 == ged_copy_from_user(acBuffer, pszBuffer, uiCount))
		{
			acBuffer[uiCount] = '\0';
			if (sscanf(acBuffer, "%d", &i32Value) == 1)
			{
				if (i32Value < 0)
					i32Value = 0;
				mtk_custom_boost_gpu_freq(i32Value);
			}
			//else if (...) //for other commands
			//{
			//}
		}
	}

	return uiCount;
}
//-----------------------------------------------------------------------------
static void* ged_custom_boost_gpu_freq_seq_start(struct seq_file *psSeqFile, loff_t *puiPosition)
{
	if (0 == *puiPosition)
	{
		return SEQ_START_TOKEN;
	}

	return NULL;
}
//-----------------------------------------------------------------------------
static void ged_custom_boost_gpu_freq_seq_stop(struct seq_file *psSeqFile, void *pvData)
{

}
//-----------------------------------------------------------------------------
static void* ged_custom_boost_gpu_freq_seq_next(struct seq_file *psSeqFile, void *pvData, loff_t *puiPosition)
{
	return NULL;
}
//-----------------------------------------------------------------------------
static int ged_custom_boost_gpu_freq_seq_show(struct seq_file *psSeqFile, void *pvData)
{
	if (pvData != NULL)
	{
		unsigned int ui32BoostGpuFreqLevel;
		if (false == mtk_get_custom_boost_gpu_freq(&ui32BoostGpuFreqLevel))
		{
			ui32BoostGpuFreqLevel = 0;
		}
		seq_printf(psSeqFile, "%u\n", ui32BoostGpuFreqLevel);
	}

	return 0;
}
//-----------------------------------------------------------------------------
static struct seq_operations gsCustomBoostGpuFreqReadOps = 
{
	.start = ged_custom_boost_gpu_freq_seq_start,
	.stop = ged_custom_boost_gpu_freq_seq_stop,
	.next = ged_custom_boost_gpu_freq_seq_next,
	.show = ged_custom_boost_gpu_freq_seq_show,
};
//-----------------------------------------------------------------------------
static ssize_t ged_custom_upbound_gpu_freq_write_entry(const char __user *pszBuffer, size_t uiCount, loff_t uiPosition, void *pvData)
{
#define GED_HAL_DEBUGFS_SIZE 64
	char acBuffer[GED_HAL_DEBUGFS_SIZE];

	int i32Value;

	if ((0 < uiCount) && (uiCount < GED_HAL_DEBUGFS_SIZE))
	{
		if (0 == ged_copy_from_user(acBuffer, pszBuffer, uiCount))
		{
			acBuffer[uiCount] = '\0';
			if (sscanf(acBuffer, "%d", &i32Value) == 1)
			{
				if (i32Value < 0)
					i32Value = 0;
				mtk_custom_upbound_gpu_freq(i32Value);
			}
			//else if (...) //for other commands
			//{
			//}
		}
	}

	return uiCount;
}
//-----------------------------------------------------------------------------
static void* ged_custom_upbound_gpu_freq_seq_start(struct seq_file *psSeqFile, loff_t *puiPosition)
{
	if (0 == *puiPosition)
	{
		return SEQ_START_TOKEN;
	}

	return NULL;
}
//-----------------------------------------------------------------------------
static void ged_custom_upbound_gpu_freq_seq_stop(struct seq_file *psSeqFile, void *pvData)
{

}
//-----------------------------------------------------------------------------
static void* ged_custom_upbound_gpu_freq_seq_next(struct seq_file *psSeqFile, void *pvData, loff_t *puiPosition)
{
	return NULL;
}
//-----------------------------------------------------------------------------
static int ged_custom_upbound_gpu_freq_seq_show(struct seq_file *psSeqFile, void *pvData)
{
	if (pvData != NULL)
	{
		unsigned int ui32UpboundGpuFreqLevel;
		if (false == mtk_get_custom_upbound_gpu_freq(&ui32UpboundGpuFreqLevel))
		{
			ui32UpboundGpuFreqLevel = 0;
			seq_printf(psSeqFile, "call mtk_get_custom_upbound_gpu_freq false\n");
		}
		seq_printf(psSeqFile, "%u\n", ui32UpboundGpuFreqLevel);
	}

	return 0;
}
//-----------------------------------------------------------------------------
static struct seq_operations gsCustomUpboundGpuFreqReadOps = 
{
	.start = ged_custom_upbound_gpu_freq_seq_start,
	.stop = ged_custom_upbound_gpu_freq_seq_stop,
	.next = ged_custom_upbound_gpu_freq_seq_next,
	.show = ged_custom_upbound_gpu_freq_seq_show,
};
//-----------------------------------------------------------------------------

static bool bForce=GED_FALSE;
static ssize_t ged_vsync_offset_enable_write_entry(const char __user *pszBuffer, size_t uiCount, loff_t uiPosition, void *pvData)
{
#define GED_HAL_DEBUGFS_SIZE 64
#define NUM_TOKEN 2

	/*
	 *  This proc node accept only: [CMD] [NUM]
	 *  for ex: "touch 1"
	 *  
	 */    

	char acBuffer[GED_HAL_DEBUGFS_SIZE];
	int aint32Indx[NUM_TOKEN];
	char* pcCMD;
	char* pcValue;
	int i;



	if ((0 < uiCount) && (uiCount < GED_HAL_DEBUGFS_SIZE))
	{
		if (0 == ged_copy_from_user(acBuffer, pszBuffer, uiCount))
		{
			acBuffer[uiCount] = '\0';
			i=tokenizer(acBuffer, uiCount, aint32Indx, NUM_TOKEN);
			if(i==NUM_TOKEN)
			{
				pcCMD = acBuffer+aint32Indx[0];

				pcValue = acBuffer+aint32Indx[1];
 
				if(strcmp(pcCMD,"touch_down")==0)
				{
					if ( (*pcValue)=='1'|| (*pcValue) =='0')
					{
						if( (*pcValue) -'0'==0) // touch up
							ged_dvfs_vsync_offset_event_switch(GED_DVFS_VSYNC_OFFSET_TOUCH_EVENT , false);
						else // touch down
							ged_dvfs_vsync_offset_event_switch(GED_DVFS_VSYNC_OFFSET_TOUCH_EVENT , true);
					}  
				}
				else if(strcmp(pcCMD,"enable_WFD")==0)
				{
					if ( (*pcValue) =='1'|| (*pcValue) =='0')
					{
						if( (*pcValue) -'0'==0) // WFD turn-off
							ged_dvfs_vsync_offset_event_switch(GED_DVFS_VSYNC_OFFSET_WFD_EVENT , false);
						else // WFD turn-on
							ged_dvfs_vsync_offset_event_switch(GED_DVFS_VSYNC_OFFSET_WFD_EVENT , true);
					}
				}
				else if(strcmp(pcCMD,"enable_debug")==0)
					{
						if ( (*pcValue) =='1'|| (*pcValue) =='0'||(*pcValue) =='2')
						{
							if( (*pcValue) -'0'==1) // force off
							{
								ged_dvfs_vsync_offset_event_switch(GED_DVFS_VSYNC_OFFSET_FORCE_OFF , true);
								bForce = GED_FALSE;
							}
							else if( (*pcValue) -'0'==2) // force on
							{
								ged_dvfs_vsync_offset_event_switch(GED_DVFS_VSYNC_OFFSET_FORCE_ON , true);
								bForce = GED_TRUE;
							}
							else // turn-off debug
								ged_dvfs_vsync_offset_event_switch(GED_DVFS_VSYNC_OFFSET_DEBUG_CLEAR_EVENT , true);
						}
					}
					else if(strcmp(pcCMD, "gas") == 0)
					{
						if ( (*pcValue) =='1'|| (*pcValue) =='0')
						{
							if( (*pcValue) -'0'==0)
								ged_dvfs_vsync_offset_event_switch(GED_DVFS_VSYNC_OFFSET_GAS_EVENT, false);
							else
								ged_dvfs_vsync_offset_event_switch(GED_DVFS_VSYNC_OFFSET_GAS_EVENT, true);
						}
					}
					else if(strcmp(pcCMD, "enable_VR") == 0)
					{
						if ( (*pcValue) =='1'|| (*pcValue) =='0')
						{
							if( (*pcValue) -'0'==0)
								ged_dvfs_vsync_offset_event_switch(GED_DVFS_VSYNC_OFFSET_VR_EVENT, false);
							else
								ged_dvfs_vsync_offset_event_switch(GED_DVFS_VSYNC_OFFSET_VR_EVENT, true);
						}
					}
					else if (strcmp(pcCMD, "mhl4k-vid") == 0)
					{
						if ((*pcValue) == '1'|| (*pcValue) == '0')
						{
							if ((*pcValue) -'0' == 0)
								ged_dvfs_vsync_offset_event_switch(GED_DVFS_VSYNC_OFFSET_MHL4K_VID_EVENT, false);
							else
								ged_dvfs_vsync_offset_event_switch(GED_DVFS_VSYNC_OFFSET_MHL4K_VID_EVENT, true);
						}
					}
					else if (strcmp(pcCMD, "low-power-mode") == 0)
                                        {
                                                if ((*pcValue) == '1'|| (*pcValue) == '0')
                                                {
                                                        if ((*pcValue) -'0' == 0)
                                                                ged_dvfs_vsync_offset_event_switch(GED_DVFS_VSYNC_OFFSET_LOW_POWER_MODE_EVENT, false);
                                                        else
                                                                ged_dvfs_vsync_offset_event_switch(GED_DVFS_VSYNC_OFFSET_LOW_POWER_MODE_EVENT, true);
                                                }
					}
					else if (strcmp(pcCMD, "vilte-vid") == 0)
					{
						if ((*pcValue) == '1'|| (*pcValue) == '0')
						{
							if ((*pcValue) -'0' == 0)
								ged_dvfs_vsync_offset_event_switch(GED_DVFS_VSYNC_OFFSET_VILTE_VID_EVENT, false);
							else
								ged_dvfs_vsync_offset_event_switch(GED_DVFS_VSYNC_OFFSET_VILTE_VID_EVENT, true);
						}
					}
				else
					{
						GED_LOGE("unknow command:%s %c",pcCMD,*pcValue);
					}
			}

		}
	}
	return uiCount;

}
//-----------------------------------------------------------------------------
static void* ged_vsync_offset_enable_seq_start(struct seq_file *psSeqFile, loff_t *puiPosition)
{
	if (0 == *puiPosition)
	{
		return SEQ_START_TOKEN;
	}

	return NULL;
}
//-----------------------------------------------------------------------------
static void ged_vsync_offset_enable_seq_stop(struct seq_file *psSeqFile, void *pvData)
{

}
//-----------------------------------------------------------------------------
static void* ged_vsync_offset_enable_seq_next(struct seq_file *psSeqFile, void *pvData, loff_t *puiPosition)
{
	return NULL;
}
//-----------------------------------------------------------------------------



extern unsigned int g_ui32EventStatus; 
extern unsigned int g_ui32EventDebugStatus;

static int ged_vsync_offset_enable_seq_show(struct seq_file *psSeqFile, void *pvData)
{
	if (pvData != NULL)
	{
		seq_printf(psSeqFile, "g_ui32EventStatus =%x\n",g_ui32EventStatus);
		seq_printf(psSeqFile, "g_ui32EventDebugStatus =%x\n",g_ui32EventDebugStatus);
		if( g_ui32EventDebugStatus&GED_EVENT_FORCE_ON )
		{
			seq_printf(psSeqFile, "Debug mode: Force on\n");
		}
		else if ( g_ui32EventDebugStatus&GED_EVENT_FORCE_OFF )
		{
			seq_printf(psSeqFile, "Debug mode: Force off\n");
		}
		else
		{
			seq_printf(psSeqFile, "Touch: %d\n",  g_ui32EventStatus&GED_EVENT_TOUCH?1:0 );
			seq_printf(psSeqFile, "WFD: %d\n",  g_ui32EventStatus&GED_EVENT_WFD?1:0 );
			seq_printf(psSeqFile, "MHL: %d\n",  g_ui32EventStatus&GED_EVENT_MHL?1:0 );
			seq_printf(psSeqFile, "GAS: %d\n",  g_ui32EventStatus&GED_EVENT_GAS?1:0 );
			seq_printf(psSeqFile, "VR: %d\n",  g_ui32EventStatus&GED_EVENT_VR?1:0 );
			seq_printf(psSeqFile, "Thermal: %d\n", g_ui32EventStatus&GED_EVENT_THERMAL?1:0 );
			seq_printf(psSeqFile, "Low power mode: %d\n", g_ui32EventStatus & GED_EVENT_LOW_POWER_MODE ? 1 : 0);
			seq_printf(psSeqFile, "MHL4K Video: %d\n", g_ui32EventStatus & GED_EVENT_MHL4K_VID ? 1 : 0);
			seq_printf(psSeqFile, "ViLTE Video: %d\n", g_ui32EventStatus & GED_EVENT_VILTE_VID ? 1 : 0);
		}
	}

	return 0;
}
//-----------------------------------------------------------------------------
static struct seq_operations gsVsync_offset_enableReadOps = 
{
	.start = ged_vsync_offset_enable_seq_start,
	.stop = ged_vsync_offset_enable_seq_stop,
	.next = ged_vsync_offset_enable_seq_next,
	.show = ged_vsync_offset_enable_seq_show,
};
//-----------------------------------------------------------------------------

//ged_dvfs_vsync_offset_level_set
static ssize_t ged_vsync_offset_level_write_entry(
		const char __user *pszBuffer,
		size_t uiCount,
		loff_t uiPosition,
		void *pvData)
{
#define GED_HAL_DEBUGFS_SIZE 64
#define NUM_TOKEN 2

	/*
	 *  This proc node accept only: [CMD] [NUM]
	 *  for ex: "touch 1"
	 *  
	 */    

	char acBuffer[GED_HAL_DEBUGFS_SIZE];
	int aint32Indx[NUM_TOKEN];
	char* pcCMD;
	char* pcValue;
	int i;
	int i32VsyncOffsetLevel;
	int ret;

	if (!((0 < uiCount) && (uiCount < GED_HAL_DEBUGFS_SIZE - 1)))
		return 0;

	if (ged_copy_from_user(acBuffer, pszBuffer, uiCount))
		return 0;

	acBuffer[uiCount] = '\n';
	acBuffer[uiCount+1] = 0;
	i=tokenizer(acBuffer, uiCount, aint32Indx, NUM_TOKEN);
	GED_LOGE("i=%d",i);
	if(i==NUM_TOKEN)
	{
		pcCMD = acBuffer+aint32Indx[0];

		pcValue = acBuffer+aint32Indx[1];
		if(strcmp(pcCMD,"set_vsync_offset")==0)
		{
			ret = kstrtoint(pcValue, 0, &i32VsyncOffsetLevel);
			ged_dvfs_vsync_offset_level_set(i32VsyncOffsetLevel);
		}
	}

	return 0;
}

//-----------------------------------------------------------------------------

static void* ged_vsync_offset_level_seq_start(struct seq_file *psSeqFile, loff_t *puiPosition)
{
	if (0 == *puiPosition)
	{
		return SEQ_START_TOKEN;
	}

	return NULL;
}
//-----------------------------------------------------------------------------
static void ged_vsync_offset_level_seq_stop(struct seq_file *psSeqFile, void *pvData)
{

}
//-----------------------------------------------------------------------------
static void* ged_vsync_offset_level_seq_next(struct seq_file *psSeqFile, void *pvData, loff_t *puiPosition)
{
	return NULL;
}
//-----------------------------------------------------------------------------
static int ged_vsync_offset_level_seq_show(struct seq_file *psSeqFile, void *pvData)
{
	if (pvData != NULL)
	{
		seq_printf(psSeqFile, "%d\n", ged_dvfs_vsync_offset_level_get());
	}

	return 0;
}
//-----------------------------------------------------------------------------
static struct seq_operations gsVsync_offset_levelReadOps = 
{
	.start = ged_vsync_offset_level_seq_start,
	.stop = ged_vsync_offset_level_seq_stop,
	.next = ged_vsync_offset_level_seq_next,
	.show = ged_vsync_offset_level_seq_show,
};
//-----------------------------------------------------------------------------


static ssize_t ged_dvfs_tuning_mode_write_entry(const char __user *pszBuffer, size_t uiCount, loff_t uiPosition, void *pvData)
{
#define GED_HAL_DEBUGFS_SIZE 64
	char acBuffer[GED_HAL_DEBUGFS_SIZE];


	if ((0 < uiCount) && (uiCount < GED_HAL_DEBUGFS_SIZE))
	{
		if (0 == ged_copy_from_user(acBuffer, pszBuffer, uiCount))
		{
			GED_DVFS_TUNING_MODE eTuningMode;
			acBuffer[uiCount] = '\0';
			if (sscanf(acBuffer, "%u", &eTuningMode) == 1)
			{
				if( GED_DVFS_DEFAULT<=eTuningMode && eTuningMode<=GED_DVFS_PERFORMANCE)
					ged_dvfs_set_tuning_mode(eTuningMode);
			}
			//else if (...) //for other commands
			//{
			//}
		}
	}

	return uiCount;
}
//-----------------------------------------------------------------------------
static void* ged_dvfs_tuning_mode_seq_start(struct seq_file *psSeqFile, loff_t *puiPosition)
{
	if (0 == *puiPosition)
	{
		return SEQ_START_TOKEN;
	}

	return NULL;
}
//-----------------------------------------------------------------------------
static void ged_dvfs_tuning_mode_seq_stop(struct seq_file *psSeqFile, void *pvData)
{

}
//-----------------------------------------------------------------------------
static void* ged_dvfs_tuning_mode_seq_next(struct seq_file *psSeqFile, void *pvData, loff_t *puiPosition)
{
	return NULL;
}
//-----------------------------------------------------------------------------

static int ged_dvfs_tuning_mode_seq_show(struct seq_file *psSeqFile, void *pvData)
{
	if (pvData != NULL)
	{
		GED_DVFS_TUNING_MODE eTuningMode;
		eTuningMode = ged_dvfs_get_tuning_mode();
		seq_printf(psSeqFile, "%u\n",eTuningMode);      
	}

	return 0;
}
//-----------------------------------------------------------------------------
static struct seq_operations gsDvfs_tuning_mode_ReadOps = 
{
	.start = ged_dvfs_tuning_mode_seq_start,
	.stop = ged_dvfs_tuning_mode_seq_stop,
	.next = ged_dvfs_tuning_mode_seq_next,
	.show = ged_dvfs_tuning_mode_seq_show,
};
//-----------------------------------------------------------------------------

static void* ged_dvfs_cur_freq_seq_start(struct seq_file *psSeqFile, loff_t *puiPosition)
{
	if (0 == *puiPosition)
	{
		return SEQ_START_TOKEN;
	}

	return NULL;
}
//-----------------------------------------------------------------------------
static void ged_dvfs_cur_freq_seq_stop(struct seq_file *psSeqFile, void *pvData)
{

}
//-----------------------------------------------------------------------------
static void* ged_dvfs_cur_freq_seq_next(struct seq_file *psSeqFile, void *pvData, loff_t *puiPosition)
{
	return NULL;
}
//-----------------------------------------------------------------------------

static int ged_dvfs_cur_freq_seq_show(struct seq_file *psSeqFile, void *pvData)
{
	if (pvData != NULL)
	{
		GED_DVFS_FREQ_DATA sFreqInfo;
		ged_dvfs_get_gpu_cur_freq(&sFreqInfo);
		seq_printf(psSeqFile, "%u %lu\n", sFreqInfo.ui32Idx, sFreqInfo.ulFreq);
	}

	return 0;
}
//-----------------------------------------------------------------------------
static struct seq_operations gsDvfs_cur_freq_ReadOps = 
{
	.start = ged_dvfs_cur_freq_seq_start,
	.stop = ged_dvfs_cur_freq_seq_stop,
	.next = ged_dvfs_cur_freq_seq_next,
	.show = ged_dvfs_cur_freq_seq_show,
};


//-----------------------------------------------------------------------------

static void* ged_dvfs_pre_freq_seq_start(struct seq_file *psSeqFile, loff_t *puiPosition)
{
	if (0 == *puiPosition)
	{
		return SEQ_START_TOKEN;
	}

	return NULL;
}
//-----------------------------------------------------------------------------
static void ged_dvfs_pre_freq_seq_stop(struct seq_file *psSeqFile, void *pvData)
{

}
//-----------------------------------------------------------------------------
static void* ged_dvfs_pre_freq_seq_next(struct seq_file *psSeqFile, void *pvData, loff_t *puiPosition)
{
	return NULL;
}
//-----------------------------------------------------------------------------

static int ged_dvfs_pre_freq_seq_show(struct seq_file *psSeqFile, void *pvData)
{
	if (pvData != NULL)
	{
		GED_DVFS_FREQ_DATA sFreqInfo;
		ged_dvfs_get_gpu_pre_freq(&sFreqInfo);
		seq_printf(psSeqFile, "%u %lu\n", sFreqInfo.ui32Idx, sFreqInfo.ulFreq);
	}

	return 0;
}
//-----------------------------------------------------------------------------
static struct seq_operations gsDvfs_pre_freq_ReadOps = 
{
	.start = ged_dvfs_pre_freq_seq_start,
	.stop = ged_dvfs_pre_freq_seq_stop,
	.next = ged_dvfs_pre_freq_seq_next,
	.show = ged_dvfs_pre_freq_seq_show,
};


//-----------------------------------------------------------------------------

static void* ged_dvfs_gpu_util_seq_start(struct seq_file *psSeqFile, loff_t *puiPosition)
{
	if (0 == *puiPosition)
	{
		return SEQ_START_TOKEN;
	}

	return NULL;
}
//-----------------------------------------------------------------------------
static void ged_dvfs_gpu_util_seq_stop(struct seq_file *psSeqFile, void *pvData)
{

}
//-----------------------------------------------------------------------------
static void* ged_dvfs_gpu_util_seq_next(struct seq_file *psSeqFile, void *pvData, loff_t *puiPosition)
{
	return NULL;
}
//-----------------------------------------------------------------------------

static int ged_dvfs_gpu_util_seq_show(struct seq_file *psSeqFile, void *pvData)
{
	if (pvData != NULL)
	{
		unsigned int loading;
		unsigned int block;
		unsigned int idle;
		mtk_get_gpu_loading(&loading);
		mtk_get_gpu_block(&block);
		mtk_get_gpu_idle(&idle);
		seq_printf(psSeqFile, "%u %u %u\n",loading,block,idle);      
	}

	return 0;
}
//-----------------------------------------------------------------------------
static struct seq_operations gsDvfs_gpu_util_ReadOps = 
{
	.start = ged_dvfs_gpu_util_seq_start,
	.stop = ged_dvfs_gpu_util_seq_stop,
	.next = ged_dvfs_gpu_util_seq_next,
	.show = ged_dvfs_gpu_util_seq_show,
};
//-----------------------------------------------------------------------------

static uint32_t _fps_upper_bound = 60;

static void *ged_fps_ub_seq_start(struct seq_file *seq, loff_t *pos)
{
#if 0
	if (0 == *pos)
		return SEQ_START_TOKEN;

	return NULL;
#else
	return *pos ? NULL : SEQ_START_TOKEN;
#endif
}

static void ged_fps_ub_seq_stop(struct seq_file *seq, void *v)
{
}

static void *ged_fps_ub_seq_next(struct seq_file *seq, void *v, loff_t *pos)
{
	return NULL;
}

static int ged_fps_ub_seq_show(struct seq_file *seq, void *v)
{
	printk("+%s", __func__);
	seq_printf(seq, "%u\n", _fps_upper_bound);
	return 0;
}

static struct seq_operations gs_fps_ub_read_ops =
{
	.start  = ged_fps_ub_seq_start,
	.stop   = ged_fps_ub_seq_stop,
	.next   = ged_fps_ub_seq_next,
	.show   = ged_fps_ub_seq_show,
};

#define MAX_FPS_DIGITS	2
static ssize_t ged_fps_ub_write(const char __user *pszBuffer, size_t uiCount,
		loff_t uiPosition, void *pvData)
{
	char str_num[MAX_FPS_DIGITS + 1];

	if (0 == ged_copy_from_user(str_num, pszBuffer, MAX_FPS_DIGITS))
	{
		str_num[MAX_FPS_DIGITS] = 0;
		_fps_upper_bound = simple_strtol(str_num, NULL, 10);
		ged_dvfs_probe_signal(GED_FPS_CHANGE_SIGNAL_EVENT);
		printk("GED: fps is set to %d", _fps_upper_bound);
	}

	return uiCount;
}

//-----------------------------------------------------------------------------

static void* ged_dvfs_integration_report_seq_start(struct seq_file *psSeqFile, loff_t *puiPosition)
{
	if (0 == *puiPosition)
	{
		return SEQ_START_TOKEN;
	}

	return NULL;
}
//-----------------------------------------------------------------------------
static void ged_dvfs_integration_report_seq_stop(struct seq_file *psSeqFile, void *pvData)
{

}
//-----------------------------------------------------------------------------
static void* ged_dvfs_integration_report_seq_next(struct seq_file *psSeqFile, void *pvData, loff_t *puiPosition)
{
	return NULL;
}
//-----------------------------------------------------------------------------

extern void (*ged_dvfs_cal_gpu_utilization_fp)(unsigned int* pui32Loading , unsigned int* pui32Block,unsigned int* pui32Idle) ;
extern void (*ged_dvfs_gpu_freq_commit_fp)(unsigned long ui32NewFreqID, GED_DVFS_COMMIT_TYPE eCommitType, int* pbCommited) ;
extern bool ged_gpu_power_on_notified;
extern bool ged_gpu_power_off_notified;
static int ged_dvfs_integration_report_seq_show(struct seq_file *psSeqFile, void *pvData)
{
	if (pvData != NULL)
	{
		seq_printf(psSeqFile, "GPU Utilization fp: %p\n", ged_dvfs_cal_gpu_utilization_fp);
		seq_printf(psSeqFile, "GPU DVFS idx commit fp: %p\n", ged_dvfs_gpu_freq_commit_fp);
		seq_printf(psSeqFile, "GPU clock notify on: %d\n", ged_gpu_power_on_notified);
		seq_printf(psSeqFile, "GPU clock notify off: %d\n", ged_gpu_power_off_notified);
	}
	return 0;
}
//-----------------------------------------------------------------------------
static struct seq_operations gsIntegrationReportReadOps = 
{
	.start = ged_dvfs_integration_report_seq_start,
	.stop = ged_dvfs_integration_report_seq_stop,
	.next = ged_dvfs_integration_report_seq_next,
	.show = ged_dvfs_integration_report_seq_show,
};
//-----------------------------------------------------------------------------

GED_ERROR ged_hal_init(void)
{
	GED_ERROR err = GED_OK;

	err = ged_debugFS_create_entry_dir(
			"hal",
			NULL,
			&gpsHALDir);

	if (unlikely(err != GED_OK))
	{
		err = GED_ERROR_FAIL;
		GED_LOGE("ged: failed to create hal dir!\n");
		goto ERROR;
	}

	/* Feedback the gpu freq level count */
	err = ged_debugFS_create_entry(
			"total_gpu_freq_level_count",
			gpsHALDir,
			&gsTotalGPUFreqLevelCountReadOps,
			NULL,
			NULL,
			&gpsTotalGPUFreqLevelCountEntry);

	if (unlikely(err != GED_OK))
	{
		GED_LOGE("ged: failed to create total_gpu_freq_level_count entry!\n");
		goto ERROR;
	}

	/* Control the gpu freq */
	err = ged_debugFS_create_entry(
			"custom_boost_gpu_freq",
			gpsHALDir,
			&gsCustomBoostGpuFreqReadOps,
			ged_custom_boost_gpu_freq_write_entry,
			NULL,
			&gpsCustomBoostGPUFreqEntry);

	if (unlikely(err != GED_OK))
	{
		GED_LOGE("ged: failed to create custom_boost_gpu_freq entry!\n");
		goto ERROR;
	}

	/* Control the gpu freq */
	err = ged_debugFS_create_entry(
			"custom_upbound_gpu_freq",
			gpsHALDir,
			&gsCustomUpboundGpuFreqReadOps,
			ged_custom_upbound_gpu_freq_write_entry,
			NULL,
			&gpsCustomUpboundGPUFreqEntry);

	if (unlikely(err != GED_OK))
	{
		GED_LOGE("ged: failed to create custom_upbound_gpu_freq entry!\n");
		goto ERROR;
	}

	/* Enable/Disable the vsync offset */

	err = ged_debugFS_create_entry(
			"event_notify",
			gpsHALDir,
			&gsVsync_offset_enableReadOps,
			ged_vsync_offset_enable_write_entry,
			NULL,
			&gpsVsyncOffsetEnableEntry);


	/* Control the vsync offset level */

	err = ged_debugFS_create_entry(
			"vsync_offset_level",
			gpsHALDir,
			&gsVsync_offset_levelReadOps,
			ged_vsync_offset_level_write_entry,
			NULL,
			&gpsVsyncOffsetLevelEntry);

	/* Control the dvfs policy threshold level */

	err = ged_debugFS_create_entry(
			"custom_dvfs_mode",
			gpsHALDir,
			&gsDvfs_tuning_mode_ReadOps, 
			ged_dvfs_tuning_mode_write_entry, 
			NULL,
			&gpsDvfsTuningModeEntry);


	/* Get current GPU freq */

	err = ged_debugFS_create_entry(
			"current_freqency",
			gpsHALDir,
			&gsDvfs_cur_freq_ReadOps, 
			NULL, 
			NULL,
			&gpsDvfsCurFreqEntry);

	/* Get previous GPU freq */

	err = ged_debugFS_create_entry(
			"previous_freqency",
			gpsHALDir,
			&gsDvfs_pre_freq_ReadOps, 
			NULL, 
			NULL,
			&gpsDvfsPreFreqEntry);

	/* Get GPU Utilization */

	err = ged_debugFS_create_entry(
			"gpu_utilization",
			gpsHALDir,
			&gsDvfs_gpu_util_ReadOps, 
			NULL, 
			NULL,
			&gpsDvfsGpuUtilizationEntry);

	/* Get FPS upper bound */
	err = ged_debugFS_create_entry(
			"fps_upper_bound",
			gpsHALDir,
			&gs_fps_ub_read_ops,
			ged_fps_ub_write,
			NULL,
			&gpsFpsUpperBoundEntry
			);

	if (unlikely(err != GED_OK))
	{
		GED_LOGE("ged: failed to create vsync_offset_level entry!\n");
		goto ERROR;
	}

	/* Report Integration Status */
	err = ged_debugFS_create_entry(
			"integration_report",
			gpsHALDir,
			&gsIntegrationReportReadOps,
			NULL,
			NULL,
			&gpsIntegrationReportReadEntry);

	if (unlikely(err != GED_OK))
	{
		GED_LOGE("ged: failed to create integration_report entry!\n");
		goto ERROR;
	}

	return err;

ERROR:

	ged_hal_exit();

	return err;
}
//-----------------------------------------------------------------------------
void ged_hal_exit(void)
{
	ged_debugFS_remove_entry(gpsIntegrationReportReadEntry);
	ged_debugFS_remove_entry(gpsFpsUpperBoundEntry);
	ged_debugFS_remove_entry(gpsVsyncOffsetLevelEntry);
	ged_debugFS_remove_entry(gpsCustomUpboundGPUFreqEntry);
	ged_debugFS_remove_entry(gpsCustomBoostGPUFreqEntry);
	ged_debugFS_remove_entry(gpsVsyncOffsetEnableEntry);
	ged_debugFS_remove_entry(gpsTotalGPUFreqLevelCountEntry);
	ged_debugFS_remove_entry(gpsDvfsCurFreqEntry);
	ged_debugFS_remove_entry(gpsDvfsPreFreqEntry);
	ged_debugFS_remove_entry(gpsDvfsGpuUtilizationEntry);
	ged_debugFS_remove_entry_dir(gpsHALDir);
}
//-----------------------------------------------------------------------------
