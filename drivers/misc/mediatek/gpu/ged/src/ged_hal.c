#include <linux/version.h>
#include <asm/io.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/genalloc.h>
#include <linux/sched.h>
#include <linux/mutex.h>
#include <linux/xlog.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/mtk_gpu_utility.h>

#include "ged_base.h"
#include "ged_hal.h"
#include "ged_debugFS.h"

static struct dentry* gpsHALDir = NULL;
static struct dentry* gpsTotalGPUFreqLevelCountEntry = NULL;
static struct dentry* gpsCustomBoostGPUFreqEntry = NULL;
static struct dentry* gpsCustomUpboundGPUFreqEntry = NULL;

//-----------------------------------------------------------------------------
static void* ged_total_gpu_freq_level_count_seq_start(struct seq_file *psSeqFile, loff_t *puiPosition)
{
    if (0 == *puiPosition)
    {
        return 1;
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
        return 1;
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
        return 1;
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

    return err;

ERROR:

    ged_hal_exit();

    return err;
}
//-----------------------------------------------------------------------------
void ged_hal_exit(void)
{
    ged_debugFS_remove_entry(gpsCustomUpboundGPUFreqEntry);
    ged_debugFS_remove_entry(gpsCustomBoostGPUFreqEntry);
    ged_debugFS_remove_entry(gpsTotalGPUFreqLevelCountEntry);
    ged_debugFS_remove_entry_dir(gpsHALDir);
}
//-----------------------------------------------------------------------------
